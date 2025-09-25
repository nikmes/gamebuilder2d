#include <catch2/catch_test_macros.hpp>
#include "gb2d/interop/gb2d_window_api.h"
#include "gb2d/interop/gb2d_interop.h"
#include <string>

using namespace gb2d::interop;

TEST_CASE("Window title validation (UTF-8 & length) (T026-pre)", "[interop][contract][title][T026]") {
    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);

    gb2d_window_id id = 0;

    SECTION("Empty title rejected") {
        REQUIRE(gb2d_window_create("", 320, 200, &id) == StatusCode::BAD_FORMAT);
    }

    SECTION("Too long title rejected") {
        std::string big(GB2D_WINDOW_TITLE_MAX_BYTES + 1, 'a');
        REQUIRE(gb2d_window_create(big.c_str(), 320, 200, &id) == StatusCode::BAD_FORMAT);
    }

    SECTION("Boundary length accepted") {
        std::string edge(GB2D_WINDOW_TITLE_MAX_BYTES, 'b');
        REQUIRE(gb2d_window_create(edge.c_str(), 320, 200, &id) == StatusCode::OK);
        REQUIRE(id != 0);
    }

    SECTION("Invalid UTF-8 rejected (lone continuation byte)") {
        const char bad[] = { static_cast<char>(0x80), 0 }; // 0x80 alone not valid
        REQUIRE(gb2d_window_create(bad, 320, 200, &id) == StatusCode::BAD_FORMAT);
    }

    SECTION("Valid multi-byte UTF-8 accepted (emoji)") {
        const char* emoji = "Window \xF0\x9F\x9A\x80"; // "Window 🚀"
        REQUIRE(gb2d_window_create(emoji, 320, 200, &id) == StatusCode::OK);
        REQUIRE(id != 0);
    }

    // Title change validation
    REQUIRE(gb2d_window_create("Initial", 320, 200, &id) == StatusCode::OK);
    REQUIRE(id != 0);

    SECTION("Set title rejects empty") {
        REQUIRE(gb2d_window_set_title(id, "") == StatusCode::BAD_FORMAT);
    }

    SECTION("Set title rejects invalid UTF-8") {
        const char bad2[] = { static_cast<char>(0xC0), static_cast<char>(0xAF), 0 }; // overlong encoding attempt
        REQUIRE(gb2d_window_set_title(id, bad2) == StatusCode::BAD_FORMAT);
    }

    SECTION("Set title accepts boundary length") {
        std::string edge2(GB2D_WINDOW_TITLE_MAX_BYTES, 'c');
        REQUIRE(gb2d_window_set_title(id, edge2.c_str()) == StatusCode::OK);
    }
}
