// Verifies that setting configuration key scripting.maxContexts changes capacity limit.
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <sstream>
#include "gb2d/interop/gb2d_interop.h"
#include "services/configuration/ConfigurationManager.h"

using namespace gb2d::interop;

TEST_CASE("Dynamic scripting.maxContexts override limits loads", "[interop][unit][scripts][config]") {
    // Start fresh
    gb2d_runtime__reset_for_tests();

    // Override capacity to small number (2) before initialization.
    gb2d::ConfigurationManager::loadOrDefault();
    gb2d::ConfigurationManager::set("scripting.maxContexts", static_cast<int64_t>(2));

    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);

    // First two loads succeed
    REQUIRE(gb2d_script_load("dyn_script_A.dll") == StatusCode::OK);
    REQUIRE(gb2d_script_load("dyn_script_B.dll") == StatusCode::OK);

    // Third should hit configured limit and return RUNTIME_ERROR
    REQUIRE(gb2d_script_load("dyn_script_C.dll") == StatusCode::RUNTIME_ERROR);

    // Reset for isolation
    gb2d_runtime__reset_for_tests();
}
