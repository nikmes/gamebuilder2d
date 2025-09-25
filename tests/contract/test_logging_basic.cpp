#include <catch2/catch_test_macros.hpp>
#include "gb2d/interop/gb2d_logging_api.h"
#include "gb2d/interop/gb2d_interop.h"

using namespace gb2d::interop;

TEST_CASE("Logging info/warn/error happy path (T007)", "[interop][contract][T007]") {
    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);
    REQUIRE(gb2d_log_info("Hello world") == StatusCode::OK);
    REQUIRE(gb2d_log_warn("Careful") == StatusCode::OK);
    REQUIRE(gb2d_log_error("Boom") == StatusCode::OK);
}
