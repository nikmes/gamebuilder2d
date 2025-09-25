#include <catch2/catch_test_macros.hpp>
#include "gb2d/interop/gb2d_interop.h"
#include "gb2d/interop/gb2d_logging_api.h"
#include "gb2d/interop/gb2d_status_codes.h"

using namespace gb2d::interop;

TEST_CASE("Logging counters across scripts (T011)", "[interop][integration][T011]") {
    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);

    // Load two scripts
    REQUIRE(gb2d_script_load("scriptA.dll") == StatusCode::OK);
    REQUIRE(gb2d_script_load("scriptB.dll") == StatusCode::OK);

    // Active script A
    REQUIRE(gb2d_script_set_active("scriptA.dll") == StatusCode::OK);
    REQUIRE(gb2d_log_info("A:info1") == StatusCode::OK);
    REQUIRE(gb2d_log_warn("A:warn1") == StatusCode::OK);
    REQUIRE(gb2d_log_info("A:info2") == StatusCode::OK);

    // Switch to script B
    REQUIRE(gb2d_script_set_active("scriptB.dll") == StatusCode::OK);
    REQUIRE(gb2d_log_error("B:error1") == StatusCode::OK);
    REQUIRE(gb2d_log_info("B:info1") == StatusCode::OK);

    // Clear active and log again (should fall back to last loaded script heuristic = scriptB)
    REQUIRE(gb2d_script_clear_active() == StatusCode::OK);
    REQUIRE(gb2d_log_warn("fallback->B:warn2") == StatusCode::OK);

    uint64_t a_info=0,a_warn=0,a_err=0;
    uint64_t b_info=0,b_warn=0,b_err=0;
    REQUIRE(gb2d_log_get_counters("scriptA.dll", &a_info, &a_warn, &a_err) == StatusCode::OK);
    REQUIRE(gb2d_log_get_counters("scriptB.dll", &b_info, &b_warn, &b_err) == StatusCode::OK);

    // Validate counters
    CHECK(a_info == 2);
    CHECK(a_warn == 1);
    CHECK(a_err == 0);
    CHECK(b_info == 1); // B:info1
    CHECK(b_warn == 1); // fallback warn hits B after clear
    CHECK(b_err == 1);  // B:error1
}
