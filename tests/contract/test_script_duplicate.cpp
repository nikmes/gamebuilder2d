#include <catch2/catch_test_macros.hpp>
#include "gb2d/interop/gb2d_interop.h"
#include "../interop_test_helpers.h"

using namespace gb2d::interop;

TEST_CASE("Duplicate script load returns ALREADY_LOADED (T008)", "[interop][contract][T008]") {
    gb2d_reset_runtime_for_test();
    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);
    const char* path = "scripts/example/ScriptA.dll"; // canonical test path
    REQUIRE(gb2d_script_load(path) == StatusCode::OK);
    REQUIRE(gb2d_script_load(path) == StatusCode::ALREADY_LOADED);
}
