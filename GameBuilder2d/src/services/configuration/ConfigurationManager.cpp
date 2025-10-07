#include "ConfigurationManager.h"
#include "paths.h"
#include "json_io.h"
#include "services/hotkey/HotKeyActions.h"
#include <nlohmann/json.hpp>
using nlohmann::json;
#include <filesystem>
#include <cstdlib>
#include <string_view>
#include <cctype>
#include <mutex>
#include <map>
#include <set>
#include <optional>
#include <array>
#include <utility>
#include <algorithm>
#include <regex>
#include <sstream>
#include <iomanip>
#include <cmath>

#if !defined(_WIN32)
extern "C" char **environ;
#endif

namespace gb2d {
namespace {
	static constexpr int kCurrentConfigVersion = 1;

	namespace hotkey_actions = gb2d::hotkeys::actions;

	const json* get_by_path(const json& j, const std::string& path);
	json& ensure_json_path(json& j, const std::string& path);
	bool validate_enum(const ConfigFieldDesc& desc, const std::string& value, FieldValidationState& state, ValidationPhase phase);
	std::string enum_values_hint(const ConfigFieldDesc& desc);
	std::optional<std::string> path_mode_for(const ConfigFieldDesc& desc);
	bool value_is_present(const ConfigValue& value) noexcept;
	FieldValidationState validate_boolean(const ConfigValue& value);
	FieldValidationState validate_integer(const ConfigFieldDesc& desc, const ConfigValue& value);
	FieldValidationState validate_float(const ConfigFieldDesc& desc, const ConfigValue& value);
	FieldValidationState validate_enum_value(const ConfigFieldDesc& desc, const ConfigValue& value, ValidationPhase phase);
	FieldValidationState validate_string_value(const ConfigFieldDesc& desc, const ConfigValue& value);
	FieldValidationState validate_path_value(const ConfigFieldDesc& desc, const ConfigValue& value);
	FieldValidationState validate_list_value(const ConfigFieldDesc& desc, const ConfigValue& value);
	FieldValidationState validate_numeric_range(const ConfigFieldDesc& desc, double value);
	FieldValidationState validate_list(const ConfigFieldDesc& desc, const std::vector<std::string>& list);
	FieldValidationState validate_path(const ConfigFieldDesc& desc, const std::string& path);
	FieldValidationState validate_string_regex(const ConfigFieldDesc& desc, const std::string& value);

	using HotkeyDefault = std::pair<const char*, const char*>;
	constexpr std::array<HotkeyDefault, 21> kDefaultHotkeys = {{
		{hotkey_actions::OpenFileDialog,       "Ctrl+O"},
		{hotkey_actions::OpenImageDialog,      "Ctrl+Shift+O"},
		{hotkey_actions::ToggleEditorFullscreen, "F11"},
		{hotkey_actions::FocusTextEditor,      "Ctrl+Shift+E"},
		{hotkey_actions::ShowConsole,          "Ctrl+Shift+C"},
		{hotkey_actions::SpawnDockWindow,      "Ctrl+Shift+N"},
		{hotkey_actions::OpenHotkeySettings,   "Ctrl+Alt+K"},
		{hotkey_actions::SaveLayout,           "Ctrl+Alt+S"},
		{hotkey_actions::OpenLayoutManager,    "Ctrl+Alt+L"},
		{hotkey_actions::CodeNewFile,          "Ctrl+N"},
		{hotkey_actions::CodeOpenFile,         "Ctrl+Shift+O"},
		{hotkey_actions::CodeSaveFile,         "Ctrl+S"},
		{hotkey_actions::CodeSaveFileAs,       "Ctrl+Shift+S"},
		{hotkey_actions::CodeSaveAll,          "Ctrl+Alt+S"},
		{hotkey_actions::CodeCloseTab,         "Ctrl+W"},
		{hotkey_actions::CodeCloseAllTabs,     "Ctrl+Shift+W"},
		{hotkey_actions::GameToggleFullscreen, "Alt+Enter"},
		{hotkey_actions::GameReset,            "Ctrl+R"},
		{hotkey_actions::GameCycleNext,        "Ctrl+Tab"},
		{hotkey_actions::GameCyclePrev,        "Ctrl+Shift+Tab"},
		{hotkey_actions::FullscreenExit,       "Esc"},
	}};

	json buildHotkeyDefaultsArray() {
		json arr = json::array();
		for (const auto& [actionId, shortcut] : kDefaultHotkeys) {
			json entry = json::object();
			entry["action"] = actionId;
			entry["shortcut"] = shortcut;
			arr.push_back(std::move(entry));
		}
		return arr;
	}

	void ensureHotkeyDefaults(json& j, bool overrideExisting) {
		const json* existing = get_by_path(j, "input.hotkeys");
		bool shouldApply = overrideExisting;
		if (!shouldApply) {
			if (!existing) {
				shouldApply = true;
			} else if (!existing->is_array()) {
				shouldApply = true;
			}
		}
		if (!shouldApply) {
			return;
		}
		ensure_json_path(j, "input.hotkeys") = buildHotkeyDefaultsArray();
	}

	ConfigurationSchema buildConfigurationSchema() {
		ConfigurationSchemaBuilder builder;

		builder.section("window", [](ConfigSectionBuilder& section) {
			section.label("Window")
				.description("Primary editor window dimensions and startup mode.");
			section.field("window.width", ConfigFieldType::Integer, [](ConfigFieldBuilder& field) {
				field.label("Width")
					.description("Width of the main application window in pixels.")
					.defaultInt(1280)
					.min(640.0)
					.max(7680.0)
					.step(1.0);
			});
			section.field("window.height", ConfigFieldType::Integer, [](ConfigFieldBuilder& field) {
				field.label("Height")
					.description("Height of the main application window in pixels.")
					.defaultInt(720)
					.min(480.0)
					.max(4320.0)
					.step(1.0);
			});
			section.field("window.fullscreen", ConfigFieldType::Boolean, [](ConfigFieldBuilder& field) {
				field.label("Launch in Fullscreen")
					.description("Start the editor in fullscreen mode (toggle later with F11).")
					.defaultBool(false);
				field.uiHint("tooltip", "Ignores fullscreen overrides below when disabled.");
			});
		});

		builder.section("fullscreen", [](ConfigSectionBuilder& section) {
			section.label("Fullscreen Session")
				.description("Overrides used when entering fullscreen gameplay or preview mode.")
				.advanced();
			section.field("fullscreen.width", ConfigFieldType::Integer, [](ConfigFieldBuilder& field) {
				field.label("Display Width")
					.description("Monitor width to request for fullscreen sessions (pixels).")
					.defaultInt(1920)
					.min(640.0)
					.max(7680.0)
					.step(1.0);
			});
			section.field("fullscreen.height", ConfigFieldType::Integer, [](ConfigFieldBuilder& field) {
				field.label("Display Height")
					.description("Monitor height to request for fullscreen sessions (pixels).")
					.defaultInt(1080)
					.min(480.0)
					.max(4320.0)
					.step(1.0);
			});
			section.field("fullscreen.game_width", ConfigFieldType::Integer, [](ConfigFieldBuilder& field) {
				field.label("Game Render Width")
					.description("Internal back-buffer width. Use 0 to derive from window dimensions.")
					.defaultInt(0)
					.min(0.0)
					.max(4096.0)
					.step(1.0)
					.advanced();
				field.uiHint("placeholder", "auto");
			});
			section.field("fullscreen.game_height", ConfigFieldType::Integer, [](ConfigFieldBuilder& field) {
				field.label("Game Render Height")
					.description("Internal back-buffer height. Use 0 to derive from window dimensions.")
					.defaultInt(0)
					.min(0.0)
					.max(4096.0)
					.step(1.0)
					.advanced();
				field.uiHint("placeholder", "auto");
			});
		});

		builder.section("ui", [](ConfigSectionBuilder& section) {
			section.label("User Interface")
				.description("Presentation preferences for the GameBuilder2d editor.");
			section.field("ui.theme", ConfigFieldType::Enum, [](ConfigFieldBuilder& field) {
				field.label("Theme")
					.description("Color theme applied across the editor UI.")
					.defaultString("dark")
					.enumValues({"dark", "light"});
				field.uiHint("enumLabels", json::object({{"dark", "Dark"}, {"light", "Light (Preview)"}}));
			});
		});

		builder.section("textures", [](ConfigSectionBuilder& section) {
			section.label("Textures")
				.description("Runtime texture loading and caching behavior.");
			section.field("textures.search_paths", ConfigFieldType::List, [](ConfigFieldBuilder& field) {
				field.label("Search Paths")
					.description("Directories scanned when loading texture assets.")
					.defaultStringList({"assets/textures"});
				field.uiHint("itemPlaceholder", "assets/textures");
				field.uiHint("pathMode", "directory");
			});
			section.field("textures.default_filter", ConfigFieldType::Enum, [](ConfigFieldBuilder& field) {
				field.label("Filter Mode")
					.description("Texture sampling filter applied after load.")
					.defaultString("bilinear")
					.enumValues({"nearest", "bilinear", "trilinear", "anisotropic"});
				field.uiHint("enumLabels", json::object({
					{"nearest", "Nearest (Pixel)"},
					{"bilinear", "Bilinear"},
					{"trilinear", "Trilinear"},
					{"anisotropic", "Anisotropic 4x"}
				}));
			});
			section.field("textures.generate_mipmaps", ConfigFieldType::Boolean, [](ConfigFieldBuilder& field) {
				field.label("Generate Mipmaps")
					.description("Automatically build mipmaps for loaded textures (slower loads).")
					.defaultBool(false)
					.advanced();
			});
			section.field("textures.max_bytes", ConfigFieldType::Integer, [](ConfigFieldBuilder& field) {
				field.label("Memory Budget (bytes)")
					.description("Optional soft cap for texture memory. 0 disables the limit.")
					.defaultInt(0)
					.min(0.0)
					.step(1048576.0)
					.advanced();
				field.uiHint("placeholder", "0 (unlimited)");
			});
			section.field("textures.placeholder_path", ConfigFieldType::Path, [](ConfigFieldBuilder& field) {
				field.label("Placeholder Texture")
					.description("Optional override texture to use when assets fail to load.")
					.defaultString("")
					.advanced();
				field.uiHint("pathMode", "file");
				field.uiHint("placeholder", "assets/textures/missing.png");
			});
		});

		builder.section("audio", [](ConfigSectionBuilder& section) {
			section.label("Audio")
				.description("Audio device routing, volumes, and preloading options.");
			section.field("audio.enabled", ConfigFieldType::Boolean, [](ConfigFieldBuilder& field) {
				field.label("Enable Audio")
					.description("Master switch that mutes or enables all audio playback.")
					.defaultBool(true);
			});
			section.field("audio.master_volume", ConfigFieldType::Float, [](ConfigFieldBuilder& field) {
				field.label("Master Volume")
					.description("Global gain multiplier applied to all audio channels.")
					.defaultFloat(1.0)
					.min(0.0)
					.max(1.0)
					.step(0.01)
					.precision(2);
			});
			section.field("audio.music_volume", ConfigFieldType::Float, [](ConfigFieldBuilder& field) {
				field.label("Music Volume")
					.description("Gain applied to music tracks.")
					.defaultFloat(1.0)
					.min(0.0)
					.max(1.0)
					.step(0.01)
					.precision(2);
			});
			section.field("audio.sfx_volume", ConfigFieldType::Float, [](ConfigFieldBuilder& field) {
				field.label("SFX Volume")
					.description("Gain applied to sound effects.")
					.defaultFloat(1.0)
					.min(0.0)
					.max(1.0)
					.step(0.01)
					.precision(2);
			});
			section.field("audio.max_concurrent_sounds", ConfigFieldType::Integer, [](ConfigFieldBuilder& field) {
				field.label("Max Concurrent Sounds")
					.description("Upper bound on simultaneously playing sound effects.")
					.defaultInt(16)
					.min(1.0)
					.max(128.0)
					.step(1.0)
					.advanced();
			});
			section.field("audio.search_paths", ConfigFieldType::List, [](ConfigFieldBuilder& field) {
				field.label("Asset Search Paths")
					.description("Directories scanned when resolving audio assets.")
					.defaultStringList({"assets/audio"});
				field.uiHint("itemPlaceholder", "assets/audio");
				field.uiHint("pathMode", "directory");
			});
			section.field("audio.preload_sounds", ConfigFieldType::List, [](ConfigFieldBuilder& field) {
				field.label("Preload Sounds")
					.description("Sound effect files warmed at startup. Leave empty to load on demand.")
					.defaultStringList(std::vector<std::string>{})
					.advanced();
				field.uiHint("pathMode", "file");
				field.uiHint("itemPlaceholder", "assets/audio/ui/click.wav");
			});
			section.field("audio.preload_music", ConfigFieldType::List, [](ConfigFieldBuilder& field) {
				field.label("Preload Music")
					.description("Music file paths loaded eagerly at startup.")
					.defaultStringList(std::vector<std::string>{})
					.advanced();
				field.uiHint("pathMode", "file");
				field.uiHint("itemPlaceholder", "assets/audio/music/theme.ogg");
			});
		});

		builder.section("input", [](ConfigSectionBuilder& section) {
			section.label("Input")
				.description("Keyboard, mouse, and controller customization.");
			section.section("input.hotkeys", [](ConfigSectionBuilder& child) {
				child.label("Hotkeys")
					.description("Keyboard shortcuts mapped to editor commands.");
				child.field("input.hotkeys", ConfigFieldType::Hotkeys, [](ConfigFieldBuilder& field) {
					field.label("Hotkey Catalog")
						.description("Manage shortcuts for editor actions. Each row maps an action to a key chord.")
						.defaultJson(buildHotkeyDefaultsArray());
					field.uiHint("primaryKey", "action");
					field.uiHint("ui", "hotkeyTable");
				});
			});
		});

		builder.section("debug", [](ConfigSectionBuilder& section) {
			section.label("Debug")
				.description("Reserved for developer diagnostics and feature flags.")
				.hidden()
				.advanced();
		});

		builder.section("metadata", [](ConfigSectionBuilder& section) {
			section.label("Metadata")
				.description("Internal settings used for configuration migrations.")
				.hidden();
			section.field("version", ConfigFieldType::Integer, [](ConfigFieldBuilder& field) {
				field.label("Config Version")
					.description("Internal schema version. Used during migration and not user editable.")
					.defaultInt(kCurrentConfigVersion)
					.hidden();
				field.uiHint("readOnly", true);
			});
		});

		return std::move(builder).build();
	}


	json& cfg() {
		static json c;
		return c;
	}

	std::mutex& mtx() {
		static std::mutex m;
		return m;
	}

	std::map<int, std::function<void()>>& subscribers() {
		static std::map<int, std::function<void()>> subs;
		return subs;
	}

	int& next_sub_id() {
		static int id = 1;
		return id;
	}

	std::vector<ConfigurationManager::OnConfigReloadedHook>& reload_hooks() {
		static std::vector<ConfigurationManager::OnConfigReloadedHook> hooks;
		return hooks;
	}

	// Navigate JSON by dotted path; returns pointer if found else nullptr
	const json* get_by_path(const json& j, const std::string& path) {
		const json* cur = &j;
		size_t start = 0;
		while (start <= path.size()) {
			size_t dot = path.find('.', start);
			std::string key = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
			if (!cur->is_object()) return nullptr;
			auto it = cur->find(key);
			if (it == cur->end()) return nullptr;
			if (dot == std::string::npos) {
				return &(*it);
			}
			cur = &(*it);
			start = dot + 1;
		}
		return nullptr;
	}

	// Ensure objects exist along path and return reference to leaf slot
	json& ensure_json_path(json& j, const std::string& path) {
		json* cur = &j;
		size_t start = 0;
		while (start <= path.size()) {
			size_t dot = path.find('.', start);
			std::string key = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
			if (!cur->is_object()) {
				*cur = json::object();
			}
			cur = &((*cur)[key]);
			if (dot == std::string::npos) break;
			start = dot + 1;
		}
		return *cur;
	}

	bool starts_with(std::string_view s, std::string_view pfx) {
		return s.size() >= pfx.size() && 0 == s.compare(0, pfx.size(), pfx);
	}
	std::string normalize_key(std::string key) {
		if (key.find("::") == std::string::npos) return key;
		std::string out; out.reserve(key.size());
		for (size_t i = 0; i < key.size(); ++i) {
			if (key[i] == ':' && i + 1 < key.size() && key[i + 1] == ':') {
				out.push_back('.');
				++i; // skip the second ':'
			} else {
				out.push_back(key[i]);
			}
		}
		return out;
	}

	std::string enum_values_hint(const ConfigFieldDesc& desc) {
		if (desc.validation.enumValues.empty()) {
			return {};
		}
		std::ostringstream oss;
		bool first = true;
		for (const auto& entry : desc.validation.enumValues) {
			if (!first) {
				oss << ", ";
			}
			first = false;
			oss << '"' << entry << '"';
		}
		return oss.str();
	}

	std::optional<std::string> path_mode_for(const ConfigFieldDesc& desc) {
		if (desc.validation.pathMode) {
			return desc.validation.pathMode;
		}
		auto it = desc.uiHints.find("pathMode");
		if (it != desc.uiHints.end() && it->second.is_string()) {
			return it->second.get<std::string>();
		}
		return std::nullopt;
	}

	bool validate_enum(const ConfigFieldDesc& desc, const std::string& value, FieldValidationState& state, ValidationPhase phase) {
		if (desc.validation.enumValues.empty()) {
			return true;
		}
		const bool match = std::find(desc.validation.enumValues.begin(), desc.validation.enumValues.end(), value) != desc.validation.enumValues.end();
		if (!match) {
			state.valid = false;
			std::ostringstream oss;
			oss << "Value must be one of: " << enum_values_hint(desc);
			state.message = oss.str();
			return false;
		}
		return true;
	}

	FieldValidationState validate_numeric_range(const ConfigFieldDesc& desc, double value) {
		FieldValidationState state{};
		if (desc.validation.min && value < *desc.validation.min) {
			state.valid = false;
			std::ostringstream oss;
			oss << "Minimum value is " << *desc.validation.min;
			if (desc.validation.step && *desc.validation.step > 0.0) {
				oss << " (step " << *desc.validation.step << ')';
			}
			state.message = oss.str();
			return state;
		}
		if (desc.validation.max && value > *desc.validation.max) {
			state.valid = false;
			std::ostringstream oss;
			oss << "Maximum value is " << *desc.validation.max;
			if (desc.validation.step && *desc.validation.step > 0.0) {
				oss << " (step " << *desc.validation.step << ')';
			}
			state.message = oss.str();
			return state;
		}
		if (desc.validation.step && *desc.validation.step > 0.0 && desc.validation.min) {
			double offset = (value - *desc.validation.min) / *desc.validation.step;
			double integral;
			if (std::abs(std::modf(offset, &integral)) > 1e-6) {
				FieldValidationState stepState;
				stepState.valid = false;
				std::ostringstream oss;
				oss << "Value must align to step " << *desc.validation.step;
				stepState.message = oss.str();
				return stepState;
			}
		}
		return state;
	}

	FieldValidationState validate_list(const ConfigFieldDesc& desc, const std::vector<std::string>& list) {
		FieldValidationState state{};
		auto pathMode = path_mode_for(desc);
		if (pathMode && *pathMode == "directory") {
			for (const auto& entry : list) {
				if (entry.empty()) {
					state.valid = false;
					state.message = "Directory paths cannot be empty.";
					return state;
				}
			}
		}
		return state;
	}

	FieldValidationState validate_path(const ConfigFieldDesc& desc, const std::string& path) {
		FieldValidationState state{};
		auto pathMode = path_mode_for(desc);
		if (!pathMode) {
			return state;
		}
		if (path.empty()) {
			return state;
		}
		std::filesystem::path p(path);
		switch (desc.type) {
		case ConfigFieldType::Path:
		case ConfigFieldType::String:
		case ConfigFieldType::Enum:
		case ConfigFieldType::List:
			break;
		default:
			return state;
		}
		std::error_code ec;
		bool exists = std::filesystem::exists(p, ec);
		if (ec) {
			state.valid = false;
			state.message = "Unable to verify path.";
			return state;
		}
		if (*pathMode == "file" && exists && std::filesystem::is_directory(p, ec)) {
			state.valid = false;
			state.message = "Expected a file path.";
		}
		if (*pathMode == "directory" && exists && !std::filesystem::is_directory(p, ec)) {
			state.valid = false;
			state.message = "Expected a directory path.";
		}
		return state;
	}

	FieldValidationState validate_string_regex(const ConfigFieldDesc& desc, const std::string& value) {
		FieldValidationState state{};
		if (desc.validation.regex) {
			try {
				if (!std::regex_match(value, std::regex(*desc.validation.regex))) {
					state.valid = false;
					state.message = "Value does not match required format.";
				}
			} catch (const std::regex_error&) {
				// ignore malformed regex definitions
			}
		}
		return state;
	}

	bool value_is_present(const ConfigValue& value) noexcept {
		return !std::holds_alternative<std::monostate>(value);
	}

	FieldValidationState validate_boolean(const ConfigValue& value) {
		FieldValidationState state{};
		if (!std::holds_alternative<bool>(value)) {
			state.valid = false;
			state.message = "Expected a boolean.";
		}
		return state;
	}

	FieldValidationState validate_integer(const ConfigFieldDesc& desc, const ConfigValue& value) {
		if (const auto* intValue = std::get_if<std::int64_t>(&value)) {
			return validate_numeric_range(desc, static_cast<double>(*intValue));
		}
		if (const auto* doubleValue = std::get_if<double>(&value)) {
			if (std::floor(*doubleValue) == *doubleValue) {
				return validate_numeric_range(desc, *doubleValue);
			}
		}
		FieldValidationState state{};
		state.valid = false;
		state.message = "Expected an integer.";
		return state;
	}

	FieldValidationState validate_float(const ConfigFieldDesc& desc, const ConfigValue& value) {
		if (const auto* doubleValue = std::get_if<double>(&value)) {
			return validate_numeric_range(desc, *doubleValue);
		}
		if (const auto* intValue = std::get_if<std::int64_t>(&value)) {
			return validate_numeric_range(desc, static_cast<double>(*intValue));
		}
		FieldValidationState state{};
		state.valid = false;
		state.message = "Expected a number.";
		return state;
	}

	FieldValidationState validate_string_value(const ConfigFieldDesc& desc, const ConfigValue& value) {
		FieldValidationState state{};
		if (const auto* str = std::get_if<std::string>(&value)) {
			return validate_string_regex(desc, *str);
		}
		state.valid = false;
		state.message = "Expected text.";
		return state;
	}

	FieldValidationState validate_enum_value(const ConfigFieldDesc& desc, const ConfigValue& value, ValidationPhase phase) {
		FieldValidationState state = validate_string_value(desc, value);
		if (!state.valid) {
			return state;
		}
		const auto* str = std::get_if<std::string>(&value);
		if (!str) {
			return state;
		}
		if (!validate_enum(desc, *str, state, phase)) {
			return state;
		}
		return state;
	}

	FieldValidationState validate_path_value(const ConfigFieldDesc& desc, const ConfigValue& value) {
		FieldValidationState base = validate_string_value(desc, value);
		if (!base.valid) {
			return base;
		}
		const auto* str = std::get_if<std::string>(&value);
		if (!str) {
			return base;
		}
		FieldValidationState pathState = validate_path(desc, *str);
		if (!pathState.valid) {
			return pathState;
		}
		return base;
	}

	FieldValidationState validate_list_value(const ConfigFieldDesc& desc, const ConfigValue& value) {
		FieldValidationState state{};
		const auto* list = std::get_if<std::vector<std::string>>(&value);
		if (!list) {
			state.valid = false;
			state.message = "Expected a list.";
			return state;
		}
		state = validate_list(desc, *list);
		if (!state.valid) {
			return state;
		}
		if (desc.validation.regex) {
			for (const auto& entry : *list) {
				FieldValidationState itemState = validate_string_regex(desc, entry);
				if (!itemState.valid) {
					return itemState;
				}
			}
		}
		auto pathMode = path_mode_for(desc);
		if (pathMode) {
			for (const auto& entry : *list) {
				FieldValidationState pathState = validate_path(desc, entry);
				if (!pathState.valid) {
					return pathState;
				}
			}
		}
		return state;
	}

	std::string to_lower(std::string s) {
		for (auto& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		return s;
	}

	bool is_integer(const std::string& v) {
		if (v.empty()) return false;
		size_t i = (v[0] == '-' || v[0] == '+') ? 1 : 0;
		if (i >= v.size()) return false;
		for (; i < v.size(); ++i) if (!std::isdigit(static_cast<unsigned char>(v[i]))) return false;
		return true;
	}

	bool parse_bool(std::string v, bool& out) {
		v = to_lower(std::move(v));
		if (v == "true" || v == "1" || v == "yes" || v == "on") { out = true; return true; }
		if (v == "false" || v == "0" || v == "no" || v == "off") { out = false; return true; }
		return false;
	}

	json parse_env_value(const std::string& v) {
		bool b;
		if (parse_bool(v, b)) return json(b);
		if (is_integer(v)) {
			try { return json(std::stoll(v)); } catch (...) {}
		}
		try {
			size_t idx = 0;
			double d = std::stod(v, &idx);
			if (idx == v.size()) return json(d);
		} catch (...) {}
		return json(v);
	}

	std::string map_env_key_to_config_key(std::string key) {
		// Replace double underscores with '.' and lowercase
		std::string out;
		out.reserve(key.size());
		for (size_t i = 0; i < key.size(); ++i) {
			if (key[i] == '_' && i + 1 < key.size() && key[i + 1] == '_') {
				out.push_back('.');
				++i; // skip next '_'
			} else {
				out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(key[i]))));
			}
		}
		return out;
	}

	size_t apply_env_overrides(json& j) {
		// Enumerate environment variables
#if defined(_WIN32)
		char** envp = _environ;
#else
		char** envp = environ;
#endif
		if (!envp) return 0;
		const std::string prefix = "GB2D_";
		size_t count = 0;
		for (char** e = envp; *e; ++e) {
			std::string_view entry(*e);
			size_t eq = entry.find('=');
			if (eq == std::string_view::npos) continue;
			std::string_view name = entry.substr(0, eq);
			std::string_view value = entry.substr(eq + 1);
			if (!starts_with(name, prefix)) continue;
			std::string_view suffix = name.substr(prefix.size());
			// Require at least one double underscore to form a hierarchical key.
			// This skips control vars like GB2D_CONFIG_DIR and avoids clobbering
			// objects with scalar top-level assignments.
			if (suffix.find("__") == std::string_view::npos) continue;
			std::string key_part(suffix);
			std::string key = map_env_key_to_config_key(key_part);
			json val = parse_env_value(std::string(value));
			ensure_json_path(j, key) = std::move(val);
			++count;
		}
		return count;
	}

	enum class MigrateResult { Ok, Migrated, Fallback };

	MigrateResult migrate_if_needed(const std::string& path, json& j, int* fromVersion = nullptr) {
		int version = 0;
		if (j.contains("version")) {
			const auto& v = j["version"];
			if (v.is_number_integer()) version = v.get<int>();
			else if (v.is_string()) {
				try { version = std::stoi(v.get<std::string>()); } catch (...) { version = 0; }
			}
		}
		if (fromVersion) *fromVersion = version;
		if (version == 0 && !j.contains("version")) {
			// Treat as very old; migrate to current
			std::error_code ec;
			std::filesystem::path p(path);
			std::filesystem::path bak = p; bak += ".bak";
			std::filesystem::remove(bak, ec);
			ec.clear();
			std::filesystem::rename(p, bak, ec);
			// Apply migration rules (none currently) and set version
			j["version"] = kCurrentConfigVersion;
			// Write updated file
			gb2d::jsonio::writeJsonAtomic(path, j);
			return MigrateResult::Migrated;
		}

		if (version < kCurrentConfigVersion) {
			// Backup and bump version, then write updated file
			std::error_code ec;
			std::filesystem::path p(path);
			std::filesystem::path bak = p; bak += ".bak";
			std::filesystem::remove(bak, ec);
			ec.clear();
			std::filesystem::rename(p, bak, ec);
			j["version"] = kCurrentConfigVersion;
			gb2d::jsonio::writeJsonAtomic(path, j);
			return MigrateResult::Migrated;
		}

		if (version > kCurrentConfigVersion) {
			// Unknown newer version: fallback to defaults without modifying file
			return MigrateResult::Fallback;
		}

		return MigrateResult::Ok;
	}
}

void ConfigurationManager::loadOrDefault() {
	// Populate in-memory defaults. Later, this will attempt disk load first.
	json& c = cfg();
	c = json::object();
	ensure_json_path(c, "version") = kCurrentConfigVersion;
	ensure_json_path(c, "window.width") = 1280;
	ensure_json_path(c, "window.height") = 720;
	ensure_json_path(c, "window.fullscreen") = false;
	ensure_json_path(c, "fullscreen.width") = 1920;
	ensure_json_path(c, "fullscreen.height") = 1080;
	ensure_json_path(c, "fullscreen.game_width") = 0;
	ensure_json_path(c, "fullscreen.game_height") = 0;
	ensure_json_path(c, "ui.theme") = "dark";
	auto& textureSearch = ensure_json_path(c, "textures.search_paths");
	textureSearch = json::array();
	textureSearch.push_back("assets/textures");
	ensure_json_path(c, "textures.default_filter") = "bilinear";
	ensure_json_path(c, "textures.generate_mipmaps") = false;
	ensure_json_path(c, "textures.max_bytes") = 0;
	ensure_json_path(c, "textures.placeholder_path") = "";
	ensure_json_path(c, "audio.enabled") = true;
	ensure_json_path(c, "audio.master_volume") = 1.0;
	ensure_json_path(c, "audio.music_volume") = 1.0;
	ensure_json_path(c, "audio.sfx_volume") = 1.0;
	ensure_json_path(c, "audio.max_concurrent_sounds") = 16;
	auto& audioSearch = ensure_json_path(c, "audio.search_paths");
	audioSearch = json::array();
	audioSearch.push_back("assets/audio");
	ensure_json_path(c, "audio.preload_sounds") = json::array();
	ensure_json_path(c, "audio.preload_music") = json::array();
	ensureHotkeyDefaults(c, true);
	size_t overrides = apply_env_overrides(c);
	(void)overrides; // no logging
}

bool ConfigurationManager::load() {
	auto path = gb2d::paths::configFilePath();
	auto j = gb2d::jsonio::readJson(path);
	if (!j) {
		// Backup corrupt/unreadable file if it exists, then load defaults
		std::error_code ec;
		std::filesystem::path p(path);
		if (std::filesystem::exists(p, ec)) {
			std::filesystem::path bak = p;
			bak += ".bak";
			std::filesystem::remove(bak, ec);
			std::filesystem::rename(p, bak, ec);
			(void)bak; // silent fallback
		} else {
			// silent default
		}
		loadOrDefault();
		return false;
	}
	// Handle versioning/migration
	int fromVer = 0;
	MigrateResult mr = migrate_if_needed(path, *j, &fromVer);
	if (mr == MigrateResult::Fallback) {
		loadOrDefault();
		return false;
	}
	(void)fromVer; // suppress unused if no logging
	cfg() = std::move(*j);
	ensureHotkeyDefaults(cfg(), false);
	size_t overrides2 = apply_env_overrides(cfg());
	(void)overrides2;

	// Notify reload hooks
	for (const auto& hook : reload_hooks()) {
		if (hook.callback) {
			hook.callback();
		}
	}
	return true;
}

bool ConfigurationManager::save() {
	auto path = gb2d::paths::configFilePath();
	bool ok = gb2d::jsonio::writeJsonAtomic(path, cfg());
	if (ok) {
		// Fire callbacks on caller thread
		std::map<int, std::function<void()>> copy;
		{
			std::lock_guard<std::mutex> lock(mtx());
			copy = subscribers();
		}
		for (auto& [id, cb] : copy) {
			if (cb) cb();
		}
	}
	return ok;
}

bool ConfigurationManager::getBool(const std::string& key, bool defaultValue) {
	const json* v = get_by_path(cfg(), normalize_key(key));
	if (v && v->is_boolean()) return v->get<bool>();
	return defaultValue;
}

int64_t ConfigurationManager::getInt(const std::string& key, int64_t defaultValue) {
	const json* v = get_by_path(cfg(), normalize_key(key));
	if (v && (v->is_number_integer() || v->is_number_unsigned())) return v->get<int64_t>();
	return defaultValue;
}

double ConfigurationManager::getDouble(const std::string& key, double defaultValue) {
	const json* v = get_by_path(cfg(), normalize_key(key));
	if (v && v->is_number()) return v->get<double>();
	return defaultValue;
}

std::string ConfigurationManager::getString(const std::string& key, const std::string& defaultValue) {
	const json* v = get_by_path(cfg(), normalize_key(key));
	if (v && v->is_string()) return v->get<std::string>();
	return defaultValue;
}

std::vector<std::string> ConfigurationManager::getStringList(const std::string& key, const std::vector<std::string>& defaultValue) {
	const json* v = get_by_path(cfg(), normalize_key(key));
	if (v && v->is_array()) {
		std::vector<std::string> out;
		out.reserve(v->size());
		for (const auto& e : *v) {
			if (e.is_string()) out.push_back(e.get<std::string>());
		}
		return out;
	}
	return defaultValue;
}

void ConfigurationManager::set(const std::string& key, bool value) { ensure_json_path(cfg(), normalize_key(key)) = value; }
void ConfigurationManager::set(const std::string& key, int64_t value) { ensure_json_path(cfg(), normalize_key(key)) = value; }
void ConfigurationManager::set(const std::string& key, double value) { ensure_json_path(cfg(), normalize_key(key)) = value; }
void ConfigurationManager::set(const std::string& key, const std::string& value) { ensure_json_path(cfg(), normalize_key(key)) = value; }
void ConfigurationManager::set(const std::string& key, const std::vector<std::string>& value) {
	json arr = json::array();
	for (const auto& s : value) arr.push_back(s);
	ensure_json_path(cfg(), normalize_key(key)) = std::move(arr);
}

void ConfigurationManager::setJson(const std::string& key, const json& value) {
	ensure_json_path(cfg(), normalize_key(key)) = value;
}

int ConfigurationManager::subscribeOnChange(const std::function<void()>& cb) {
	std::lock_guard<std::mutex> lock(mtx());
	int id = next_sub_id()++;
	subscribers()[id] = cb;
	return id;
}

void ConfigurationManager::unsubscribe(int id) {
	std::lock_guard<std::mutex> lock(mtx());
	subscribers().erase(id);
}

std::string ConfigurationManager::exportCompact() {
	return cfg().dump();
}

const json& ConfigurationManager::raw() {
	return cfg();
}

void ConfigurationManager::pushReloadHook(const OnConfigReloadedHook& hook) {
	if (!hook.callback) {
		return;
	}

	auto& hooks = reload_hooks();
	const bool exists = std::any_of(hooks.begin(), hooks.end(), [&](const OnConfigReloadedHook& existing) {
		return !existing.name.empty() && existing.name == hook.name;
	});
	if (exists) {
		return;
	}
	hooks.push_back(hook);
}

const ConfigurationSchema& ConfigurationManager::schema() noexcept {
	static const ConfigurationSchema schemaInstance = buildConfigurationSchema();
	return schemaInstance;
}

const ConfigSectionDesc* ConfigurationManager::findSection(std::string_view id) noexcept {
	return schema().findSection(id);
}

const ConfigFieldDesc* ConfigurationManager::findField(std::string_view id) noexcept {
	return schema().findField(id);
}

ConfigValue ConfigurationManager::valueFor(std::string_view id, ConfigValue fallback) noexcept {
	const auto* desc = findField(id);
	if (!desc) {
		return fallback;
	}
	if (const json* v = get_by_path(cfg(), std::string(id)); v) {
		switch (desc->type) {
		case ConfigFieldType::Boolean:
			if (v->is_boolean()) return v->get<bool>();
			break;
		case ConfigFieldType::Integer:
			if (v->is_number_integer()) return v->get<std::int64_t>();
			if (v->is_number()) return static_cast<std::int64_t>(v->get<double>());
			break;
		case ConfigFieldType::Float:
			if (v->is_number()) return v->get<double>();
			break;
		case ConfigFieldType::Enum:
		case ConfigFieldType::String:
		case ConfigFieldType::Path:
			if (v->is_string()) return v->get<std::string>();
			break;
		case ConfigFieldType::List:
			if (v->is_array()) {
				std::vector<std::string> out;
				for (const auto& entry : *v) {
					if (entry.is_string()) {
						out.push_back(entry.get<std::string>());
					}
				}
				return out;
			}
			break;
		case ConfigFieldType::JsonBlob:
		case ConfigFieldType::Hotkeys:
			return *v;
		}
	}
	return desc->defaultValue;
}

FieldValidationState ConfigurationManager::validateFieldValue(const ConfigFieldDesc& desc,
	const ConfigValue& value,
	ValidationPhase phase) {
	FieldValidationState state{};
	if ((desc.flags & ConfigFieldFlags::Hidden) != ConfigFieldFlags::None) {
		return state;
	}
	if (!value_is_present(value)) {
		return state;
	}
	switch (desc.type) {
	case ConfigFieldType::Boolean:
		return validate_boolean(value);
	case ConfigFieldType::Integer:
		return validate_integer(desc, value);
	case ConfigFieldType::Float:
		return validate_float(desc, value);
	case ConfigFieldType::Enum:
		return validate_enum_value(desc, value, phase);
	case ConfigFieldType::String:
		return validate_string_value(desc, value);
	case ConfigFieldType::Path:
		return validate_path_value(desc, value);
	case ConfigFieldType::List:
		return validate_list_value(desc, value);
	case ConfigFieldType::JsonBlob:
	case ConfigFieldType::Hotkeys:
		return state;
	}
	return state;
}

FieldValidationState ConfigurationManager::validateFieldValue(std::string_view id,
	const ConfigValue& value,
	ValidationPhase phase) {
	if (const auto* desc = findField(id)) {
		return validateFieldValue(*desc, value, phase);
	}
	FieldValidationState state;
	state.valid = false;
	state.message = "Unknown configuration field.";
	return state;
}
}
