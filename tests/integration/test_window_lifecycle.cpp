#include <catch2/catch_test_macros.hpp>
#include "gb2d/interop/gb2d_interop.h"
#include "gb2d/interop/gb2d_status_codes.h"
#include "gb2d/interop/gb2d_window_api.h"

using namespace gb2d::interop;

TEST_CASE("Window lifecycle via script (T010)", "[interop][integration][T010]") {
    // (T025) validates auto-close behavior when unloading script.
    // Initialize runtime
    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);

    // Load a script context and set active for attribution
    REQUIRE(gb2d_script_load("script_window.dll") == StatusCode::OK);
    REQUIRE(gb2d_script_set_active("script_window.dll") == StatusCode::OK);

    // Create window
    gb2d_window_id wid = 0;
    REQUIRE(gb2d_window_create("My Title", 320, 200, &wid) == StatusCode::OK);
    REQUIRE(wid != 0);
    REQUIRE(gb2d_window_exists(wid) == 1);

    // Change title
    REQUIRE(gb2d_window_set_title(wid, "New Title") == StatusCode::OK);

    // Clear active (not required but simulates script code boundary) then unload script
    REQUIRE(gb2d_script_clear_active() == StatusCode::OK);
    REQUIRE(gb2d_script_unload("script_window.dll") == StatusCode::OK);

    // Auto-close should have removed the window
    REQUIRE(gb2d_window_exists(wid) == 0);
    // Explicit close should now return INVALID_ID
    REQUIRE(gb2d_window_close(wid) == StatusCode::INVALID_ID);
}
