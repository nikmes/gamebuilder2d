// Ensures that setting debounce to zero disables suppression entirely (Step 5)
#include <catch2/catch_test_macros.hpp>
#include "gb2d/interop/gb2d_interop.h"
#include "gb2d/interop/gb2d_status_codes.h"
#include "services/configuration/ConfigurationManager.h"

using namespace gb2d::interop;

TEST_CASE("Zero debounce disables suppression (T012-ZERO)", "[interop][integration][T012][zero]") {
    gb2d_runtime__reset_for_tests();
    gb2d::ConfigurationManager::loadOrDefault();
    gb2d::ConfigurationManager::set("scripting.reload.debounceMs", static_cast<int64_t>(0));

    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);
    REQUIRE(gb2d_script_load("scriptReloadZero.dll") == StatusCode::OK);

    constexpr int kRequests = 5;
    for (int i = 0; i < kRequests; ++i) {
        REQUIRE(gb2d_script_request_reload("scriptReloadZero.dll") == StatusCode::OK);
    }
    // Every request should have been effective (no SUPPRESSED)
    REQUIRE(gb2d_test_effective_reload_requests() == kRequests);

    gb2d_runtime__reset_for_tests();
}
