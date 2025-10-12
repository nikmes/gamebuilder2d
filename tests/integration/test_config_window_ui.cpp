#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "services/configuration/ConfigurationEditorState.h"
#include "services/configuration/ConfigurationManager.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#if defined(_WIN32)
#  include <stdlib.h>
#endif

using Catch::Approx;

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

struct ScopedConfigEnv {
    std::filesystem::path base;
    std::filesystem::path envDir;

    explicit ScopedConfigEnv(const std::string& suffix) {
        namespace fs = std::filesystem;
        base = fs::temp_directory_path() / fs::path(suffix);
        envDir = base / "config";
        std::error_code ec;
        fs::remove_all(base, ec);
        fs::create_directories(envDir, ec);
        set_env("GB2D_CONFIG_DIR", envDir.string().c_str());
    }

    ~ScopedConfigEnv() {
        set_env("GB2D_CONFIG_DIR", "");
        gb2d::ConfigurationManager::loadOrDefault();
        std::error_code ec;
        std::filesystem::remove_all(base, ec);
    }

    std::filesystem::path configPath() const {
        return base / "config.json";
    }

    std::filesystem::path backupPath() const {
        return base / "config.backup.json";
    }
};

struct ConfigurationWindowHarness {
    void reloadFromCurrent() {
        editorState_ = gb2d::ConfigurationEditorState::fromCurrent();
        lastAppliedSnapshot_ = editorState_.toJson();
        baselineSnapshot_ = lastAppliedSnapshot_;
        stagedSnapshotDirty_ = true;
        hasUnappliedChanges_ = false;
        hasAppliedUnsavedChanges_ = false;
    }

    bool setField(std::string_view id, gb2d::ConfigValue value) {
        if (!editorState_.setFieldValue(id, std::move(value))) {
            return false;
        }
        editorState_.validateField(id, gb2d::ValidationPhase::OnEdit);
        updateAfterMutation();
        return true;
    }

    bool apply() {
        const bool validationIssues = editorState_.hasInvalidFields() || !editorState_.unknownValidation().valid;
        if (!hasUnappliedChanges_ && !validationIssues) {
            return true;
        }
        if (!editorState_.validateAll(gb2d::ValidationPhase::OnApply)) {
            return false;
        }
        const nlohmann::json snapshot = currentSnapshot();
        if (!gb2d::ConfigurationManager::applyRuntime(snapshot)) {
            return false;
        }
        lastAppliedSnapshot_ = snapshot;
        hasUnappliedChanges_ = false;
        hasAppliedUnsavedChanges_ = (snapshot != baselineSnapshot_);
        stagedSnapshotDirty_ = true;
        return true;
    }

    bool save(bool requestBackup = true, bool* outBackupCreated = nullptr) {
        if (!apply()) {
            if (outBackupCreated) {
                *outBackupCreated = false;
            }
            return false;
        }

        bool backupCreated = false;
        if (!gb2d::ConfigurationManager::save(requestBackup, requestBackup ? &backupCreated : nullptr)) {
            if (outBackupCreated) {
                *outBackupCreated = backupCreated;
            }
            return false;
        }

        if (outBackupCreated) {
            *outBackupCreated = backupCreated;
        }

        editorState_.commitToCurrent();
        baselineSnapshot_ = lastAppliedSnapshot_;
        hasAppliedUnsavedChanges_ = false;
        stagedSnapshotDirty_ = true;
        return true;
    }

    [[nodiscard]] bool hasUnappliedChanges() const noexcept { return hasUnappliedChanges_; }
    [[nodiscard]] bool hasAppliedUnsavedChanges() const noexcept { return hasAppliedUnsavedChanges_; }
    [[nodiscard]] const gb2d::ConfigurationEditorState& state() const noexcept { return editorState_; }

private:
    void updateAfterMutation() {
        stagedSnapshotDirty_ = true;
        hasUnappliedChanges_ = (currentSnapshot() != lastAppliedSnapshot_);
    }

    const nlohmann::json& currentSnapshot() {
        if (stagedSnapshotDirty_) {
            stagedSnapshot_ = editorState_.toJson();
            stagedSnapshotDirty_ = false;
        }
        return stagedSnapshot_;
    }

    gb2d::ConfigurationEditorState editorState_{};
    nlohmann::json lastAppliedSnapshot_{};
    nlohmann::json baselineSnapshot_{};
    nlohmann::json stagedSnapshot_{};
    bool stagedSnapshotDirty_{true};
    bool hasUnappliedChanges_{false};
    bool hasAppliedUnsavedChanges_{false};
};

} // namespace

TEST_CASE("Configuration window apply and save persist across restart", "[integration][config][ui]") {
    ScopedConfigEnv scopedEnv("gb2d_config_window_ui");
    gb2d::ConfigurationManager::loadOrDefault();

    ConfigurationWindowHarness window;
    window.reloadFromCurrent();

    REQUIRE(window.setField("window.width", gb2d::ConfigValue{static_cast<std::int64_t>(1724)}));
    REQUIRE(window.setField("audio.volumes.master", gb2d::ConfigValue{0.35}));
    REQUIRE(window.hasUnappliedChanges());
    REQUIRE(window.state().isDirty());

    REQUIRE(window.apply());
    CHECK_FALSE(window.hasUnappliedChanges());
    CHECK(window.hasAppliedUnsavedChanges());
    CHECK(gb2d::ConfigurationManager::getInt("window.width", 0) == 1724);
    CHECK(gb2d::ConfigurationManager::getDouble("audio.volumes.master", 0.0) == Approx(0.35));

    bool backupCreated = false;
    REQUIRE(window.save(true, &backupCreated));
    CHECK(backupCreated);
    CHECK_FALSE(window.hasAppliedUnsavedChanges());
    CHECK_FALSE(window.state().isDirty());

    const auto cfgPath = scopedEnv.configPath();
    const auto backupPath = scopedEnv.backupPath();
    REQUIRE(std::filesystem::exists(cfgPath));
    REQUIRE(std::filesystem::exists(backupPath));

    {
        std::ifstream in(cfgPath, std::ios::binary);
        REQUIRE(in.good());
        const auto doc = nlohmann::json::parse(in, nullptr, true, true);
        REQUIRE(doc.contains("window"));
        REQUIRE(doc["window"].contains("width"));
        CHECK(doc["window"]["width"].get<std::int64_t>() == 1724);
    REQUIRE(doc.contains("audio"));
    REQUIRE(doc["audio"].contains("volumes"));
    const auto& volumes = doc["audio"]["volumes"];
    REQUIRE(volumes.is_object());
    REQUIRE(volumes.contains("master"));
    CHECK(volumes["master"].get<double>() == Approx(0.35));
    }

    // Simulate runtime divergence and ensure reload pulls saved values
    gb2d::ConfigurationManager::set("window.width", static_cast<std::int64_t>(1199));
    gb2d::ConfigurationManager::set("audio.volumes.master", 1.0);
    REQUIRE(gb2d::ConfigurationManager::load());
    CHECK(gb2d::ConfigurationManager::getInt("window.width", 0) == 1724);
    CHECK(gb2d::ConfigurationManager::getDouble("audio.volumes.master", 0.0) == Approx(0.35));

    ConfigurationWindowHarness reopened;
    reopened.reloadFromCurrent();
    CHECK_FALSE(reopened.hasUnappliedChanges());
    CHECK_FALSE(reopened.hasAppliedUnsavedChanges());
    CHECK_FALSE(reopened.state().isDirty());

    const auto* widthField = reopened.state().field("window.width");
    REQUIRE(widthField != nullptr);
    REQUIRE(std::holds_alternative<std::int64_t>(widthField->currentValue));
    CHECK(std::get<std::int64_t>(widthField->currentValue) == 1724);

    const auto* volumeField = reopened.state().field("audio.volumes.master");
    REQUIRE(volumeField != nullptr);
    REQUIRE(std::holds_alternative<double>(volumeField->currentValue));
    CHECK(std::get<double>(volumeField->currentValue) == Approx(0.35));
}
