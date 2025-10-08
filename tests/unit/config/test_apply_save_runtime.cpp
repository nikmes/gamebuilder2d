#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "services/configuration/ConfigurationManager.h"
#include "services/configuration/ConfigurationEditorState.h"
#include "services/hotkey/HotKeyActions.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <cstdlib>
#include <chrono>

using Catch::Approx;
using gb2d::ConfigValue;
using gb2d::ConfigurationEditorState;
using gb2d::ConfigurationManager;
using gb2d::ValidationPhase;

namespace {
void set_env(const char* key, const char* value) {
#if defined(_WIN32)
    _putenv_s(key, value ? value : "");
#else
    if (!value || value[0] == '\0') {
        unsetenv(key);
    } else {
        setenv(key, value, 1);
    }
#endif
}

std::filesystem::path prepare_config_dir(const std::string& suffix) {
    namespace fs = std::filesystem;
    auto base = fs::temp_directory_path() / fs::path("gb2d_cfg_" + suffix);
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);
    auto envPath = base / "config";
    fs::create_directories(envPath, ec);
    set_env("GB2D_CONFIG_DIR", envPath.string().c_str());
    return base;
}

void clear_config_dir(const std::filesystem::path& dir) {
    set_env("GB2D_CONFIG_DIR", "");
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

struct ConfigDirScope {
    explicit ConfigDirScope(const std::string& suffix) : dir(prepare_config_dir(suffix)) {}
    ~ConfigDirScope() { clear_config_dir(dir); }
    std::filesystem::path dir;
};

std::string unique_hook_name(const std::string& base) {
    static std::atomic_uint counter{0};
    return base + std::to_string(counter.fetch_add(1) + 1);
}

std::filesystem::path unique_temp_dir(const std::string& prefix) {
    namespace fs = std::filesystem;
    static std::atomic_uint64_t counter{0};
    auto id = counter.fetch_add(1, std::memory_order_relaxed);
    auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto name = prefix + std::to_string(static_cast<unsigned long long>(id)) + "_" + std::to_string(timestamp);
    return fs::temp_directory_path() / fs::path(std::move(name));
}
} // namespace

TEST_CASE("ConfigurationManager validateFieldValue enforces schema constraints", "[config][schema]") {
    ConfigurationManager::loadOrDefault();
    const auto& schema = ConfigurationManager::schema();

    const auto* volumeField = schema.findField("audio.master_volume");
    REQUIRE(volumeField != nullptr);

    {
        ConfigValue tooHigh = 1.5;
        auto state = ConfigurationManager::validateFieldValue(*volumeField, tooHigh, ValidationPhase::OnApply);
        CHECK_FALSE(state.valid);
        CHECK(state.message.find("Maximum value") != std::string::npos);
    }

    {
        ConfigValue misaligned = 0.333;
        auto state = ConfigurationManager::validateFieldValue(*volumeField, misaligned, ValidationPhase::OnApply);
        CHECK_FALSE(state.valid);
        CHECK(state.message.find("step") != std::string::npos);
    }

    {
        ConfigValue okValue = 0.5;
        auto state = ConfigurationManager::validateFieldValue(*volumeField, okValue, ValidationPhase::OnApply);
        CHECK(state.valid);
    }

    const auto* searchPathsField = schema.findField("audio.search_paths");
    REQUIRE(searchPathsField != nullptr);
    {
        ConfigValue badPaths = std::vector<std::string>{"assets/audio", ""};
        auto state = ConfigurationManager::validateFieldValue(*searchPathsField, badPaths, ValidationPhase::OnApply);
        CHECK_FALSE(state.valid);
        CHECK(state.message.find("cannot be empty") != std::string::npos);
    }

    const auto* placeholderField = schema.findField("textures.placeholder_path");
    REQUIRE(placeholderField != nullptr);
    {
        auto dir = unique_temp_dir("gb2d_placeholder_");
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        ConfigValue candidate = dir.string();
        auto state = ConfigurationManager::validateFieldValue(*placeholderField, candidate, ValidationPhase::OnApply);
        CHECK_FALSE(state.valid);
        CHECK(state.message.find("Expected a file path") != std::string::npos);
        std::filesystem::remove_all(dir, ec);
    }
}

TEST_CASE("ConfigurationManager applyRuntime seeds defaults and fires reload hooks", "[config][apply]") {
    ConfigurationManager::loadOrDefault();

    auto flag = std::make_shared<bool>(false);
    auto hookName = unique_hook_name("test_apply_runtime_");
    ConfigurationManager::pushReloadHook({hookName, [flag]() { *flag = true; }});

    nlohmann::json doc = nlohmann::json::object({
        {"window", nlohmann::json::object({{"width", 1600}, {"height", 900}})},
        {"audio", nlohmann::json::object({{"master_volume", 0.25}})}
    });

    REQUIRE(ConfigurationManager::applyRuntime(doc));
    CHECK(*flag);

    CHECK(ConfigurationManager::getInt("window.width", 0) == 1600);
    CHECK(ConfigurationManager::getInt("window.height", 0) == 900);
    CHECK(ConfigurationManager::getDouble("audio.master_volume", 1.0) == Approx(0.25));

    const auto& raw = ConfigurationManager::raw();
    REQUIRE(raw.contains("input"));
    REQUIRE(raw["input"].contains("hotkeys"));
    const auto& hotkeys = raw["input"]["hotkeys"];
    REQUIRE(hotkeys.is_array());

    bool foundConfigShortcut = false;
    for (const auto& entry : hotkeys) {
        if (!entry.is_object()) {
            continue;
        }
        if (!entry.contains("action") || !entry.contains("shortcut")) {
            continue;
        }
        if (entry["action"] == gb2d::hotkeys::actions::OpenConfigurationWindow) {
            foundConfigShortcut = true;
            CHECK(entry["shortcut"] == "Ctrl+,");
            break;
        }
    }
    CHECK(foundConfigShortcut);
}

TEST_CASE("ConfigurationManager save emits optional backups", "[config][save]") {
    ConfigDirScope dirScope("backup_tests");
    const auto& dir = dirScope.dir;

    ConfigurationManager::loadOrDefault();
    ConfigurationManager::set("window.width", static_cast<int64_t>(1777));

    bool backupCreated = false;
    REQUIRE(ConfigurationManager::save(true, &backupCreated));
    CHECK(backupCreated);

    auto backupPath = dir / "config.backup.json";
    REQUIRE(std::filesystem::exists(backupPath));

    {
        std::ifstream in(backupPath, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(content.find("1777") != std::string::npos);
    }

    ConfigurationManager::set("window.width", static_cast<int64_t>(1888));
    backupCreated = false;
    REQUIRE(ConfigurationManager::save(true, &backupCreated));
    CHECK(backupCreated);
    {
        std::ifstream in(backupPath, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(content.find("1888") != std::string::npos);
    }

    bool backupFlag = true;
    REQUIRE(ConfigurationManager::save(false, &backupFlag));
    CHECK_FALSE(backupFlag);

}

TEST_CASE("ConfigurationEditorState commitToCurrent clears dirty state and unknown entries", "[config][editor-state][commit]") {
    ConfigurationManager::loadOrDefault();
    auto state = ConfigurationEditorState::fromCurrent();

    REQUIRE(state.setFieldValue("window.width", ConfigValue{static_cast<std::int64_t>(1700)}));
    auto* width = state.field("window.width");
    REQUIRE(width != nullptr);
    CHECK(width->isDirty());
    CHECK(width->canUndo());

    state.commitToCurrent();
    CHECK_FALSE(state.isDirty());
    CHECK_FALSE(width->isDirty());
    CHECK_FALSE(width->canUndo());

    nlohmann::json unknown = nlohmann::json::object({{"custom.setting", 1}});
    state.setUnknownEntries(unknown);
    CHECK(state.isDirty());
    CHECK(state.isUnknownDirty());

    state.commitToCurrent();
    CHECK_FALSE(state.isDirty());
    CHECK_FALSE(state.isUnknownDirty());

    state.setUnknownEntries(nlohmann::json::object({{"custom.setting", 2}}));
    CHECK(state.isDirty());
    state.revertUnknownEntries();
    CHECK_FALSE(state.isDirty());
}
