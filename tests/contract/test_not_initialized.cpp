#include <catch2/catch_test_macros.hpp>
#include "gb2d/interop/gb2d_interop.h"
#include "gb2d/interop/gb2d_window_api.h"
#include "gb2d/interop/gb2d_logging_api.h"

using namespace gb2d::interop;

TEST_CASE("APIs return NOT_INITIALIZED before bootstrap then OK after (T009)", "[interop][contract][T009]") {
    gb2d_window_id id = 0;
    // Ensure a clean runtime state regardless of previous tests
#ifdef GB2D_INTERNAL_TESTING
    REQUIRE(gb2d_runtime__reset_for_tests() == StatusCode::OK);
#endif
    // Pre-init expectations
    REQUIRE(gb2d_window_create("PreInit", 100, 100, &id) == StatusCode::NOT_INITIALIZED);
    REQUIRE(gb2d_log_info("msg") == StatusCode::NOT_INITIALIZED);
    REQUIRE(gb2d_script_load("scripts/example/ScriptB.dll") == StatusCode::NOT_INITIALIZED);

    // Initialize
    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);

    // Post-init baseline operations (will fail until implemented)
    REQUIRE(gb2d_log_info("after init") == StatusCode::OK);
}
