#include <catch2/catch_test_macros.hpp>
#include "gb2d/interop/gb2d_window_api.h"
#include "gb2d/interop/gb2d_interop.h"

using namespace gb2d::interop;

TEST_CASE("Invalid window id operations return INVALID_ID (T015)", "[interop][unit][window][T015]") {
    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);

    // Id 0 is reserved invalid
    REQUIRE(gb2d_window_set_title(0, "Nope") == StatusCode::INVALID_ID);
    REQUIRE(gb2d_window_close(0) == StatusCode::INVALID_ID);
    REQUIRE(gb2d_window_exists(0) == 0);

    // Create one valid window then close it; further operations should return INVALID_ID
    gb2d_window_id id = 0;
    REQUIRE(gb2d_window_create("Valid", 100, 100, &id) == StatusCode::OK);
    REQUIRE(id != 0);
    REQUIRE(gb2d_window_close(id) == StatusCode::OK);
    // Now id should be invalidated
    REQUIRE(gb2d_window_set_title(id, "Again") == StatusCode::INVALID_ID);
    REQUIRE(gb2d_window_close(id) == StatusCode::INVALID_ID);
    REQUIRE(gb2d_window_exists(id) == 0);
}
