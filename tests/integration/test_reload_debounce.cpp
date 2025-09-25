// Validates debounce logic for rapid reload requests (T012)
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>
#include "gb2d/interop/gb2d_interop.h"
#include "gb2d/interop/gb2d_status_codes.h"
#include "services/configuration/ConfigurationManager.h"

using namespace gb2d::interop;

TEST_CASE("Reload debounce enforces single effective reload within window (T012)", "[interop][integration][T012]") {
    gb2d_runtime__reset_for_tests();
    gb2d::ConfigurationManager::loadOrDefault();
    // Set a short debounce window to keep test fast
    gb2d::ConfigurationManager::set("scripting.reload.debounceMs", static_cast<int64_t>(150));

    REQUIRE(gb2d_runtime_initialize() == StatusCode::OK);
    REQUIRE(gb2d_script_load("scriptReload.dll") == StatusCode::OK);

    // First request should count
    REQUIRE(gb2d_script_request_reload("scriptReload.dll") == StatusCode::OK);
    auto initial = gb2d_test_effective_reload_requests();
    REQUIRE(initial == 1);

    // Immediate second request inside debounce window: coalesced => SUPPRESSED
    REQUIRE(gb2d_script_request_reload("scriptReload.dll") == StatusCode::SUPPRESSED);
    REQUIRE(gb2d_test_effective_reload_requests() == initial); // unchanged

    // Sleep past debounce window
    std::this_thread::sleep_for(std::chrono::milliseconds(170));
    REQUIRE(gb2d_script_request_reload("scriptReload.dll") == StatusCode::OK);
    REQUIRE(gb2d_test_effective_reload_requests() == initial + 1);

    gb2d_runtime__reset_for_tests();
}
