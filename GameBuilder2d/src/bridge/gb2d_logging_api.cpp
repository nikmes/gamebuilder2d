// gb2d_logging_api.cpp
// Implements logging interop functions (managed -> native) with UTF-8 validation (T026).
#include "gb2d/interop/gb2d_logging_api.h"
#include "gb2d/interop/gb2d_interop.h" // for status codes & potential future checks
#include "gb2d/interop/utf8_validation.h" // (T026) shared UTF-8 validator
#include "services/logger/LogManager.h"
#include <atomic>
#include "gb2d_script_manager.h"

using namespace gb2d::interop;
extern "C" const char* gb2d_script__active_path();

namespace {
    inline bool is_runtime_initialized() { return gb2d_runtime_is_initialized(); }

    static void attribute_with_fallback(void(gb2d::interop::ScriptManager::*incr)(std::string_view)) {
        auto& mgr = gb2d::interop::ScriptManager::instance();
        // Prefer explicit active context
        if (const char* active = gb2d_script__active_path(); active && *active) {
            (mgr.*incr)(active);
            return;
        }
        // Fallback: last loaded script heuristic (legacy behavior)
        auto s = mgr.get_last_loaded_script();
        if (!s.empty()) (mgr.*incr)(s);
    }
    void attr_info()  { attribute_with_fallback(&gb2d::interop::ScriptManager::increment_info); }
    void attr_warn()  { attribute_with_fallback(&gb2d::interop::ScriptManager::increment_warn); }
    void attr_error() { attribute_with_fallback(&gb2d::interop::ScriptManager::increment_error); }

    // Upper bound for a single log message (bytes, excluding terminator). Generous to allow stack traces.
    inline constexpr std::size_t kMaxLogMessageBytes = 4096;

    inline StatusCode validate_and_log(const char* msg, void(*log_fn)(std::string_view), void(*attr_fn)()) {
        if (!is_runtime_initialized()) return StatusCode::NOT_INITIALIZED;
        std::size_t len = 0;
        if (!gb2d::interop::utf8::validate(msg, kMaxLogMessageBytes, len)) return StatusCode::BAD_FORMAT;
        if (!gb2d::logging::LogManager::isInitialized()) {
            auto s = gb2d::logging::LogManager::init();
            if (s == gb2d::logging::Status::error || s == gb2d::logging::Status::not_initialized) {
                return StatusCode::INTERNAL_ERROR;
            }
        }
        log_fn(msg);
        if (attr_fn) attr_fn();
        return StatusCode::OK;
    }
}

extern "C" {

StatusCode gb2d_log_info(const char* message_utf8) {
    return validate_and_log(message_utf8, [](std::string_view m){ gb2d::logging::LogManager::info("{}", m); }, attr_info);
}
StatusCode gb2d_log_warn(const char* message_utf8) {
    return validate_and_log(message_utf8, [](std::string_view m){ gb2d::logging::LogManager::warn("{}", m); }, attr_warn);
}
StatusCode gb2d_log_error(const char* message_utf8) {
    return validate_and_log(message_utf8, [](std::string_view m){ gb2d::logging::LogManager::error("{}", m); }, attr_error);
}

StatusCode gb2d_log_get_counters(const char* script_path, uint64_t* out_info, uint64_t* out_warn, uint64_t* out_error) {
    if (!is_runtime_initialized()) return StatusCode::NOT_INITIALIZED;
    if (!script_path || !out_info || !out_warn || !out_error) return StatusCode::BAD_FORMAT;
    gb2d::interop::ScriptManager::LogCounters c;
    auto rc = gb2d::interop::ScriptManager::instance().get_counters(script_path, c);
    if (rc != StatusCode::OK) return rc;
    *out_info = c.info; *out_warn = c.warn; *out_error = c.error;
    return StatusCode::OK;
}

} // extern "C"
