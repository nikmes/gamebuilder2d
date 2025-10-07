#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "services/configuration/ConfigurationManager.h"
#include "services/configuration/ConfigurationEditorState.h"

#include <nlohmann/json.hpp>
#include <variant>
#include <vector>
#include <string>

using Catch::Approx;
using gb2d::ConfigurationEditorState;
using gb2d::ConfigurationManager;
using gb2d::ConfigFieldState;
using gb2d::ConfigSectionState;
using gb2d::ConfigValue;
using gb2d::ValidationPhase;

namespace {
void resetConfiguration() {
    ConfigurationManager::loadOrDefault();
}
}

TEST_CASE("ConfigurationEditorState loads defaults and tracks dirty state", "[config][editor-state]") {
    resetConfiguration();
    auto state = ConfigurationEditorState::fromCurrent();

    auto* widthField = state.field("window.width");
    REQUIRE(widthField != nullptr);
    REQUIRE_FALSE(widthField->isDirty());

    REQUIRE(state.setFieldValue("window.width", ConfigValue{static_cast<std::int64_t>(1400)}));
    REQUIRE(widthField->isDirty());
    REQUIRE(state.isDirty());
    CHECK(std::get<std::int64_t>(widthField->currentValue) == 1400);

    REQUIRE(state.revertField("window.width"));
    CHECK_FALSE(widthField->isDirty());
    CHECK_FALSE(state.isDirty());
}

TEST_CASE("ConfigurationEditorState revert helpers operate on sections", "[config][editor-state]") {
    resetConfiguration();
    auto state = ConfigurationEditorState::fromCurrent();

    REQUIRE(state.setFieldValue("audio.master_volume", ConfigValue{0.5}));
    REQUIRE(state.setFieldValue("audio.music_volume", ConfigValue{0.35}));

    auto* audioSection = state.section("audio");
    REQUIRE(audioSection != nullptr);
    REQUIRE(audioSection->isDirty());

    REQUIRE(state.revertSection("audio"));
    CHECK_FALSE(audioSection->isDirty());
    CHECK_FALSE(state.isDirty());
}

TEST_CASE("ConfigurationEditorState supports revert-to-default for fields", "[config][editor-state]") {
    resetConfiguration();
    auto state = ConfigurationEditorState::fromCurrent();

    auto* placeholder = state.field("textures.placeholder_path");
    REQUIRE(placeholder != nullptr);

    REQUIRE(state.setFieldValue("textures.placeholder_path", ConfigValue{std::string{"assets/textures/missing.png"}}));
    REQUIRE(placeholder->isDirty());

    REQUIRE(state.revertFieldToDefault("textures.placeholder_path"));
    CHECK(std::holds_alternative<std::string>(placeholder->currentValue));
    CHECK(std::get<std::string>(placeholder->currentValue).empty());
    CHECK_FALSE(placeholder->isDirty());
}

TEST_CASE("ConfigurationEditorState handles nested section revert", "[config][editor-state]") {
    resetConfiguration();
    auto state = ConfigurationEditorState::fromCurrent();

    auto* hotkeysField = state.field("input.hotkeys");
    REQUIRE(hotkeysField != nullptr);

    nlohmann::json replacement = nlohmann::json::array({
        nlohmann::json::object({{"action", "CustomAction"}, {"shortcut", "Ctrl+Shift+T"}})
    });

    REQUIRE(state.setFieldValue("input.hotkeys", ConfigValue{replacement}));
    REQUIRE(state.isDirty());

    REQUIRE(state.revertSection("input.hotkeys"));
    CHECK_FALSE(state.isDirty());
    CHECK(std::holds_alternative<nlohmann::json>(hotkeysField->currentValue));
    CHECK(std::get<nlohmann::json>(hotkeysField->currentValue) != replacement);
}

TEST_CASE("ConfigurationEditorState can revert sections and session to defaults", "[config][editor-state]") {
    resetConfiguration();
    auto state = ConfigurationEditorState::fromCurrent();

    REQUIRE(state.setFieldValue("audio.master_volume", ConfigValue{0.25}));
    REQUIRE(state.setFieldValue("audio.music_volume", ConfigValue{0.30}));
    REQUIRE(state.revertSectionToDefaults("audio"));

    auto* master = state.field("audio.master_volume");
    REQUIRE(master != nullptr);
    CHECK(std::get<double>(master->currentValue) == Approx(1.0));

    REQUIRE(state.setFieldValue("window.width", ConfigValue{static_cast<std::int64_t>(1900)}));
    REQUIRE(state.setFieldValue("window.height", ConfigValue{static_cast<std::int64_t>(1000)}));
    state.revertAllToDefaults();

    auto* width = state.field("window.width");
    auto* height = state.field("window.height");
    REQUIRE(width != nullptr);
    REQUIRE(height != nullptr);
    CHECK(std::get<std::int64_t>(width->currentValue) == 1280);
    CHECK(std::get<std::int64_t>(height->currentValue) == 720);
}

TEST_CASE("ConfigurationEditorState validates numeric bounds", "[config][editor-state]") {
    resetConfiguration();
    auto state = ConfigurationEditorState::fromCurrent();

    auto* width = state.field("window.width");
    REQUIRE(width != nullptr);

    REQUIRE(state.setFieldValue("window.width", ConfigValue{static_cast<std::int64_t>(200)}));
    REQUIRE_FALSE(state.validateField("window.width", ValidationPhase::OnEdit));
    CHECK_FALSE(width->validation.valid);
    CHECK(width->validation.message.find("Minimum value") != std::string::npos);

    REQUIRE(state.setFieldValue("window.width", ConfigValue{static_cast<std::int64_t>(1280)}));
    REQUIRE(state.validateField("window.width", ValidationPhase::OnEdit));
    CHECK(width->validation.valid);
    CHECK(width->validation.message.empty());
}

TEST_CASE("ConfigurationEditorState validates enum and list fields", "[config][editor-state]") {
    resetConfiguration();
    auto state = ConfigurationEditorState::fromCurrent();

    REQUIRE(state.setFieldValue("ui.theme", ConfigValue{std::string{"sepia"}}));
    REQUIRE_FALSE(state.validateField("ui.theme", ValidationPhase::OnApply));
    auto* theme = state.field("ui.theme");
    REQUIRE(theme != nullptr);
    CHECK_FALSE(theme->validation.valid);
    CHECK(theme->validation.message.find("one of") != std::string::npos);

    REQUIRE(state.setFieldValue("ui.theme", ConfigValue{std::string{"dark"}}));
    REQUIRE(state.validateField("ui.theme", ValidationPhase::OnApply));
    CHECK(theme->validation.valid);
    CHECK(theme->validation.message.empty());

    REQUIRE(state.setFieldValue("audio.search_paths", ConfigValue{std::vector<std::string>{"", "assets/audio"}}));
    REQUIRE_FALSE(state.validateField("audio.search_paths", ValidationPhase::OnApply));
    auto* searchPaths = state.field("audio.search_paths");
    REQUIRE(searchPaths != nullptr);
    CHECK_FALSE(searchPaths->validation.valid);
    CHECK(searchPaths->validation.message.find("Directory paths cannot be empty") != std::string::npos);

    REQUIRE(state.setFieldValue("audio.search_paths", ConfigValue{std::vector<std::string>{"assets/audio"}}));
    REQUIRE(state.validateField("audio.search_paths", ValidationPhase::OnApply));
    CHECK(searchPaths->validation.valid);
    CHECK(searchPaths->validation.message.empty());
}
