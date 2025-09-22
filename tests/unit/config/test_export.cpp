#include <catch2/catch_test_macros.hpp>
#include "services/configuration/ConfigurationManager.h"
#include <string>

TEST_CASE("exportCompact returns minified JSON", "[config]") {
    gb2d::ConfigurationManager::loadOrDefault();
    gb2d::ConfigurationManager::set("window.width", (int64_t)1337);
    gb2d::ConfigurationManager::set("ui.theme", std::string("zen"));

    std::string out = gb2d::ConfigurationManager::exportCompact();

    // Must contain our keys and values
    REQUIRE(out.find("\"window\"") != std::string::npos);
    REQUIRE(out.find("\"width\"") != std::string::npos);
    REQUIRE(out.find("1337") != std::string::npos);
    REQUIRE(out.find("\"ui\"") != std::string::npos);
    REQUIRE(out.find("\"theme\"") != std::string::npos);
    REQUIRE(out.find("zen") != std::string::npos);

    // Should be compact (no pretty-printed newlines)
    REQUIRE(out.find('\n') == std::string::npos);
}
