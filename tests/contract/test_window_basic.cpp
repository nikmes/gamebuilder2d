#include <catch2/catch_test_macros.hpp>
#include "gb2d/interop/gb2d_window_api.h"
#include "gb2d/interop/gb2d_interop.h"

using namespace gb2d::interop;

TEST_CASE("Window create / set title / close happy path (T006)", "[interop][contract][T006]") {
    gb2d_window_id id = 0;
    // Expected final behavior: initialize then create returns OK.
    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);
    REQUIRE(gb2d_window_create("First Window", 640, 480, &id) == StatusCode::OK);
    REQUIRE(id != 0);
    REQUIRE(gb2d_window_set_title(id, "Updated Title") == StatusCode::OK);
    REQUIRE(gb2d_window_close(id) == StatusCode::OK);
    REQUIRE(gb2d_window_exists(id) == 0);
}
