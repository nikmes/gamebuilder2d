#include <catch2/catch_test_macros.hpp>
#include "services/configuration/ConfigurationManager.h"

TEST_CASE("loadOrDefault sets defaults", "[config]") {
    gb2d::ConfigurationManager::loadOrDefault();
    REQUIRE(gb2d::ConfigurationManager::getInt("window.width", 0) == 1280);
    REQUIRE(gb2d::ConfigurationManager::getInt("window.height", 0) == 720);
    REQUIRE(gb2d::ConfigurationManager::getString("ui.theme", "") == std::string("dark"));

    // Also support section separator '::' equivalently to '.'
    REQUIRE(gb2d::ConfigurationManager::getInt("window::width", -1) == 1280);
    gb2d::ConfigurationManager::set("window::fullscreen", true);
    REQUIRE(gb2d::ConfigurationManager::getBool("window.fullscreen", false) == true);
}
