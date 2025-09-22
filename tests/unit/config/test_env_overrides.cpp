#include <catch2/catch_test_macros.hpp>
#include "services/configuration/ConfigurationManager.h"
#include <filesystem>
#include <cstdlib>

static void set_env(const char* k, const char* v) {
#if defined(_WIN32)
    _putenv_s(k, v);
#else
    setenv(k, v, 1);
#endif
}

TEST_CASE("env overrides apply", "[config]") {
    // Prepare a clean env base path so we don't touch user config
    auto base = std::filesystem::temp_directory_path() / "gb2d_configdir_env";
    std::filesystem::create_directories(base);
    set_env("GB2D_CONFIG_DIR", base.string().c_str());

    // Start from defaults then apply env overrides
    gb2d::ConfigurationManager::loadOrDefault();

    // Override width/height/theme + a boolean flag
    set_env("GB2D_WINDOW__WIDTH", "2001");
    set_env("GB2D_WINDOW__HEIGHT", "1001");
    set_env("GB2D_UI__THEME", "ayu");
    set_env("GB2D_FEATURE__ENABLED", "true");

    // Implemented to apply env on load(); call load to trigger re-application
    gb2d::ConfigurationManager::load();

    REQUIRE(gb2d::ConfigurationManager::getInt("window.width", 0) == 2001);
    REQUIRE(gb2d::ConfigurationManager::getInt("window.height", 0) == 1001);
    REQUIRE(gb2d::ConfigurationManager::getString("ui.theme", "") == std::string("ayu"));
    REQUIRE(gb2d::ConfigurationManager::getBool("feature.enabled", false) == true);
}
