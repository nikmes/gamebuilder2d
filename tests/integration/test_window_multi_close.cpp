#include <catch2/catch_test_macros.hpp>
#include "gb2d/interop/gb2d_interop.h"
#include "gb2d/interop/gb2d_status_codes.h"
#include "gb2d/interop/gb2d_window_api.h"

using namespace gb2d::interop;

TEST_CASE("Multiple windows auto-close on script unload (T025-multi)", "[interop][integration][T025][multi]") {
    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);

    // Load and activate a script context
    REQUIRE(gb2d_script_load("script_multi.dll") == StatusCode::OK);
    REQUIRE(gb2d_script_set_active("script_multi.dll") == StatusCode::OK);

    // Create several windows under this active script
    constexpr int kWindowCount = 5;
    gb2d_window_id ids[kWindowCount] = {0};
    for (int i = 0; i < kWindowCount; ++i) {
        std::string title = "Win" + std::to_string(i);
        REQUIRE(gb2d_window_create(title.c_str(), 100 + i * 10, 80 + i * 5, &ids[i]) == StatusCode::OK);
        REQUIRE(ids[i] != 0);
        REQUIRE(gb2d_window_exists(ids[i]) == 1);
    }

    // Create a window with no active script (should not be auto-closed)
    REQUIRE(gb2d_script_clear_active() == StatusCode::OK);
    gb2d_window_id orphan = 0;
    REQUIRE(gb2d_window_create("Orphan", 120, 90, &orphan) == StatusCode::OK);
    REQUIRE(orphan != 0);
    REQUIRE(gb2d_window_exists(orphan) == 1);

    // Unload the script; its 5 windows should auto-close
    REQUIRE(gb2d_script_unload("script_multi.dll") == StatusCode::OK);

    for (int i = 0; i < kWindowCount; ++i) {
        INFO("Window id " << ids[i] << " should have been auto-closed");
        REQUIRE(gb2d_window_exists(ids[i]) == 0);
        REQUIRE(gb2d_window_close(ids[i]) == StatusCode::INVALID_ID);
    }

    // Orphan window should still exist (was created with no active script)
    REQUIRE(gb2d_window_exists(orphan) == 1);
    // Clean it up explicitly
    REQUIRE(gb2d_window_close(orphan) == StatusCode::OK);
}
