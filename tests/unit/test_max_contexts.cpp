#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>
#include "gb2d/interop/gb2d_interop.h"
#include "../interop_test_helpers.h"

using namespace gb2d::interop;

TEST_CASE("Exceeding max script contexts returns error on 65th load (T017)", "[interop][unit][scripts][T017]") {
    // Ensure clean baseline (previous tests may have loaded scripts)
    gb2d_reset_runtime_for_test();
    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);

    // Load up to the declared capacity (kMaxScripts = 64) -- we don't have a public constant yet.
    // We rely on current behavior: 65th returns RUNTIME_ERROR (placeholder until a dedicated code is added).
    for (int i = 0; i < 64; ++i) {
        std::ostringstream oss; oss << "script_" << i << ".dll"; // placeholder path names
        REQUIRE(gb2d_script_load(oss.str().c_str()) == StatusCode::OK);
    }

    // 65th should fail
    REQUIRE(gb2d_script_load("script_64.dll") == StatusCode::RUNTIME_ERROR);

    // Clean up so subsequent tests (e.g. T008 duplicate load) start fresh
    gb2d_reset_runtime_for_test();
}
