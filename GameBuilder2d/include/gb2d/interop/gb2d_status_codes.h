#pragma once

// gb2d_status_codes.h
// Canonical status code enumeration for the embedded .NET (managed -> native) interop layer.
// NOTE: Implementation intentionally minimal at this stage (Task T003 / T018 placeholder).
// Tests (T005) will enforce stability & mapping contract.

#include <cstdint>

namespace gb2d::interop {

// Increment when breaking changes to the C ABI are introduced.
inline constexpr std::uint32_t GB2D_INTEROP_API_VERSION = 1u;

// Status codes intentionally compact (fits in 1 byte) but stored as 32-bit for alignment.
// Keep values stable; append only.
enum class StatusCode : std::uint32_t {
    OK = 0,
    NOT_INITIALIZED = 1,
    INVALID_ID = 2,
    CONTEXT_UNLOADING = 3,
    ALREADY_LOADED = 4,
    RUNTIME_ERROR = 5,
    INTERNAL_ERROR = 6,
    BAD_FORMAT = 7,
    SUPPRESSED = 8, // (Step 3) e.g. debounced / coalesced request intentionally not acted upon
    // Reserve forward range 100-149 for window related, 150-199 for logging if needed.
};

// Returns a stable null-terminated string literal for the status code.
const char* to_string(StatusCode code) noexcept;

} // namespace gb2d::interop
