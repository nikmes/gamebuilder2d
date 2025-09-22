#include <catch2/catch_test_macros.hpp>
#include "services/configuration/validate.h"
#include <nlohmann/json.hpp>

using gb2d::cfgvalidate::Value;

TEST_CASE("validate: isValidKey enforces pattern", "[validate]") {
    using gb2d::cfgvalidate::isValidKey;
    REQUIRE(isValidKey("a"));
    REQUIRE(isValidKey("a_b.c0"));
    REQUIRE_FALSE(isValidKey(""));
    REQUIRE_FALSE(isValidKey(".a"));
    REQUIRE_FALSE(isValidKey("a."));
    REQUIRE_FALSE(isValidKey("a..b"));
    REQUIRE_FALSE(isValidKey("A.b"));
    REQUIRE_FALSE(isValidKey("a-b"));
}

TEST_CASE("validate: isSupportedJson recognizes allowed types", "[validate]") {
    using gb2d::cfgvalidate::isSupportedJson;
    nlohmann::json j;
    j = true; REQUIRE(isSupportedJson(j));
    j = 42; REQUIRE(isSupportedJson(j));
    j = 3.14; REQUIRE(isSupportedJson(j));
    j = "hello"; REQUIRE(isSupportedJson(j));
    j = nlohmann::json::array({"a","b"}); REQUIRE(isSupportedJson(j));
    j = nlohmann::json::array({"a", 1}); REQUIRE_FALSE(isSupportedJson(j));
    j = nlohmann::json::object({{"k",1}}); REQUIRE_FALSE(isSupportedJson(j));
}

TEST_CASE("validate: toValue and toJson roundtrip", "[validate]") {
    using gb2d::cfgvalidate::toValue;
    using gb2d::cfgvalidate::toJson;
    using gb2d::cfgvalidate::isSupportedJson;

    nlohmann::json srcBool = true; Value v{}; REQUIRE(toValue(srcBool, v)); REQUIRE(toJson(v) == srcBool);
    nlohmann::json srcInt = 123; REQUIRE(toValue(srcInt, v)); REQUIRE(toJson(v) == srcInt);
    nlohmann::json srcDouble = 12.5; REQUIRE(toValue(srcDouble, v)); REQUIRE(toJson(v) == srcDouble);
    nlohmann::json srcStr = "abc"; REQUIRE(toValue(srcStr, v)); REQUIRE(toJson(v) == srcStr);
    nlohmann::json srcArr = nlohmann::json::array({"x","y"}); REQUIRE(toValue(srcArr, v)); REQUIRE(toJson(v) == srcArr);

    nlohmann::json bad = nlohmann::json::array({1,2});
    REQUIRE_FALSE(isSupportedJson(bad));
    REQUIRE_FALSE(toValue(bad, v));
}
