#include "gb2d/interop/gb2d_status_codes.h"

namespace gb2d::interop {

const char* to_string(StatusCode code) noexcept {
    switch (code) {
        case StatusCode::OK: return "OK";
        case StatusCode::NOT_INITIALIZED: return "NOT_INITIALIZED";
        case StatusCode::INVALID_ID: return "INVALID_ID";
        case StatusCode::CONTEXT_UNLOADING: return "CONTEXT_UNLOADING";
        case StatusCode::ALREADY_LOADED: return "ALREADY_LOADED";
        case StatusCode::RUNTIME_ERROR: return "RUNTIME_ERROR";
        case StatusCode::INTERNAL_ERROR: return "INTERNAL_ERROR";
        case StatusCode::BAD_FORMAT: return "BAD_FORMAT";
        case StatusCode::SUPPRESSED: return "SUPPRESSED";
        default: return "UNKNOWN_STATUS_CODE";
    }
}

} // namespace gb2d::interop
