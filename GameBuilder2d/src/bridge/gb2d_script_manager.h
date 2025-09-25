#pragma once

#include <unordered_map>
#include <cstdint>
#include <string>
#include <mutex>
#include <string_view>
#include <chrono>
#include "gb2d/interop/gb2d_status_codes.h"

namespace gb2d::interop {

class ScriptManager {
public:
    static ScriptManager& instance();

    void set_initialized();
    bool is_initialized() const noexcept;

    StatusCode load(std::string_view path);
    StatusCode unload(std::string_view path);
    bool exists(std::string_view path);
    StatusCode request_reload(std::string_view path); // placeholder (debounce in T021)
    struct LogCounters { uint64_t info=0, warn=0, error=0; };
    void increment_info(std::string_view path);
    void increment_warn(std::string_view path);
    void increment_error(std::string_view path);
    StatusCode get_counters(std::string_view path, LogCounters& out);
    std::string get_last_loaded_script();

#ifdef GB2D_INTERNAL_TESTING
    void reset_for_tests();
    // Test hook: returns number of effective (non-debounced) reload requests processed.
    uint64_t test_effective_reload_requests() const { return effective_reload_requests_; }
    uint64_t test_reload_cycles(std::string_view path) const {
        auto it = reload_cycles_.find(std::string(path));
        if (it == reload_cycles_.end()) return 0;
        return it->second;
    }
#endif

private:
    ScriptManager() = default;
    static constexpr std::size_t kDefaultMaxScripts = 64; // legacy default; overridden via config scripting.maxContexts
    bool initialized_ = false;
    std::unordered_map<std::string, LogCounters> scripts_;
    std::string last_loaded_script_;
    // Debounce tracking: last accepted reload request time per script.
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_reload_request_;
    uint64_t effective_reload_requests_ = 0; // incremented when a request is not debounced
    // Reload cycles executed per script (incremented after a successful unload+load cycle)
    std::unordered_map<std::string, uint64_t> reload_cycles_;
    mutable std::mutex mtx_;

    // Performs a simulated reload (unload + load) for an already-loaded script.
    // PRECONDITION: mtx_ is held by caller.
    StatusCode perform_reload_unlocked(const std::string& path);
};

} // namespace gb2d::interop
