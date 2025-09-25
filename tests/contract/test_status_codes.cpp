#include <catch2/catch_test_macros.hpp>
#include "gb2d/interop/gb2d_status_codes.h"

using namespace gb2d::interop;

TEST_CASE("Status codes have stable string representations", "[interop][contract][T005]") {
    struct Case { StatusCode code; const char* expected; } cases[] = {
        {StatusCode::OK, "OK"},
        {StatusCode::NOT_INITIALIZED, "NOT_INITIALIZED"},
        {StatusCode::INVALID_ID, "INVALID_ID"},
        {StatusCode::CONTEXT_UNLOADING, "CONTEXT_UNLOADING"},
        {StatusCode::ALREADY_LOADED, "ALREADY_LOADED"},
        {StatusCode::RUNTIME_ERROR, "RUNTIME_ERROR"},
        {StatusCode::INTERNAL_ERROR, "INTERNAL_ERROR"},
        {StatusCode::BAD_FORMAT, "BAD_FORMAT"},
    {StatusCode::SUPPRESSED, "SUPPRESSED"},
    };
    for (auto& c : cases) {
        const char* s = to_string(c.code);
        // Compare via std::strcmp to avoid requiring Catch2's string_view converter symbol on MSVC.
        REQUIRE(std::strcmp(s, c.expected) == 0);
    }
}

TEST_CASE("API version is 1", "[interop][contract][T005]") {
    REQUIRE(GB2D_INTEROP_API_VERSION == 1u);
}
