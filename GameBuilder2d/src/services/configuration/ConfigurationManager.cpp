#include "ConfigurationManager.h"
#include "paths.h"
#include "json_io.h"
#include <nlohmann/json.hpp>
using nlohmann::json;
#include <filesystem>
#include <cstdlib>
#include <string_view>
#include <cctype>
#include <mutex>
#include <map>
#include <optional>

#if !defined(_WIN32)
extern "C" char **environ;
#endif

namespace gb2d {
namespace {
	static constexpr int kCurrentConfigVersion = 1;


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
	json& ensure_path(json& j, const std::string& path) {
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
			ensure_path(j, key) = std::move(val);
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
	ensure_path(c, "version") = kCurrentConfigVersion;
	ensure_path(c, "window.width") = 1280;
	ensure_path(c, "window.height") = 720;
	ensure_path(c, "window.fullscreen") = false;
	ensure_path(c, "fullscreen.width") = 1920;
	ensure_path(c, "fullscreen.height") = 1080;
	ensure_path(c, "fullscreen.game_width") = 0;
	ensure_path(c, "fullscreen.game_height") = 0;
	ensure_path(c, "ui.theme") = "dark";
	auto& textureSearch = ensure_path(c, "textures.search_paths");
	textureSearch = json::array();
	textureSearch.push_back("assets/textures");
	ensure_path(c, "textures.default_filter") = "bilinear";
	ensure_path(c, "textures.generate_mipmaps") = false;
	ensure_path(c, "textures.max_bytes") = 0;
	ensure_path(c, "textures.placeholder_path") = "";
	ensure_path(c, "audio.enabled") = true;
	ensure_path(c, "audio.master_volume") = 1.0;
	ensure_path(c, "audio.music_volume") = 1.0;
	ensure_path(c, "audio.sfx_volume") = 1.0;
	ensure_path(c, "audio.max_concurrent_sounds") = 16;
	auto& audioSearch = ensure_path(c, "audio.search_paths");
	audioSearch = json::array();
	audioSearch.push_back("assets/audio");
	ensure_path(c, "audio.preload_sounds") = json::array();
	ensure_path(c, "audio.preload_music") = json::array();
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
	size_t overrides2 = apply_env_overrides(cfg());
	(void)overrides2;
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

void ConfigurationManager::set(const std::string& key, bool value) { ensure_path(cfg(), normalize_key(key)) = value; }
void ConfigurationManager::set(const std::string& key, int64_t value) { ensure_path(cfg(), normalize_key(key)) = value; }
void ConfigurationManager::set(const std::string& key, double value) { ensure_path(cfg(), normalize_key(key)) = value; }
void ConfigurationManager::set(const std::string& key, const std::string& value) { ensure_path(cfg(), normalize_key(key)) = value; }
void ConfigurationManager::set(const std::string& key, const std::vector<std::string>& value) {
	json arr = json::array();
	for (const auto& s : value) arr.push_back(s);
	ensure_path(cfg(), normalize_key(key)) = std::move(arr);
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
}
