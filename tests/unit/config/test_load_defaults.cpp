#include <catch2/catch_test_macros.hpp>
#include "services/configuration/ConfigurationManager.h"

TEST_CASE("loadOrDefault sets defaults", "[config]") {
    gb2d::ConfigurationManager::loadOrDefault();
    REQUIRE(gb2d::ConfigurationManager::getInt("window.width", 0) == 1280);
    REQUIRE(gb2d::ConfigurationManager::getInt("window.height", 0) == 720);
    REQUIRE(gb2d::ConfigurationManager::getString("ui.theme", "") == std::string("dark"));

    // Legacy section separator '::' should not resolve to dotted keys
    REQUIRE(gb2d::ConfigurationManager::getInt("window::width", -1) == -1);
    gb2d::ConfigurationManager::set("window.fullscreen", true);
    REQUIRE(gb2d::ConfigurationManager::getBool("window.fullscreen", false) == true);
}
