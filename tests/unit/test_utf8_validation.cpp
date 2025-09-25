#include <catch2/catch_test_macros.hpp>
#include <string>
#include "gb2d/interop/gb2d_window_api.h" // Reuses window title validator for now (will be factored in T026)
#include "gb2d/interop/gb2d_interop.h"

using namespace gb2d::interop;

static bool create_title_expect(const char* title, StatusCode expected) {
    gb2d_window_id id = 0;
    auto sc = gb2d_window_create(title, 64, 64, &id);
    if (expected == StatusCode::OK) {
        if (sc != StatusCode::OK) return false;
        // cleanup
        REQUIRE(gb2d_window_close(id) == StatusCode::OK);
        return true;
    }
    return sc == expected;
}

TEST_CASE("UTF-8 bad inputs return BAD_FORMAT (T016)", "[interop][unit][utf8][T016]") {
    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);

    SECTION("Overlong encoding (\xC0\xAF) rejected") {
        const char overlong[] = { (char)0xC0, (char)0xAF, 0 }; // invalid overlong form
        REQUIRE(create_title_expect(overlong, StatusCode::BAD_FORMAT));
    }
    SECTION("Lone continuation byte 0x80 rejected") {
        const char cont[] = { (char)0x80, 0 };
        REQUIRE(create_title_expect(cont, StatusCode::BAD_FORMAT));
    }
    SECTION("Truncated 2-byte sequence") {
        const char trunc2[] = { (char)0xC2, 0 }; // needs one continuation
        REQUIRE(create_title_expect(trunc2, StatusCode::BAD_FORMAT));
    }
    SECTION("Truncated 3-byte sequence") {
        const char trunc3[] = { (char)0xE2, (char)0x82, 0 }; // incomplete for e.g. Euro sign
        REQUIRE(create_title_expect(trunc3, StatusCode::BAD_FORMAT));
    }
    SECTION("Invalid leading 0xF5 (> U+10FFFF range)") {
        const char badLead[] = { (char)0xF5, (char)0x80, (char)0x80, (char)0x80, 0 };
        REQUIRE(create_title_expect(badLead, StatusCode::BAD_FORMAT));
    }
    SECTION("Valid mixed UTF-8 passes") {
        const char* good = "Hello \xF0\x9F\x9A\x80"; // Hello + rocket
        REQUIRE(create_title_expect(good, StatusCode::OK));
    }
}
