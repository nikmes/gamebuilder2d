#include "gb2d_script_manager.h"

#include "services/configuration/ConfigurationManager.h"
#include "services/logger/LogManager.h"
#include "gb2d/interop/gb2d_window_api.h" // for gb2d_window__close_all_for_script internal helper

#include <algorithm>
#include <cctype>

namespace gb2d::interop {

ScriptManager& ScriptManager::instance() {
    static ScriptManager inst;
    return inst;
}

void ScriptManager::set_initialized() {
    std::lock_guard lock(mtx_);
    initialized_ = true;
}

bool ScriptManager::is_initialized() const noexcept {
    return initialized_;
}

// Simple UTF-8 / format validation placeholder: path must be non-empty and not contain '\n' or '\r'.
static bool is_valid_path(std::string_view p) {
    if (p.empty()) return false;
    return p.find_first_of("\n\r") == std::string_view::npos;
}

StatusCode ScriptManager::load(std::string_view path) {
    if (!is_initialized()) return StatusCode::NOT_INITIALIZED;
    if (!is_valid_path(path)) return StatusCode::BAD_FORMAT;
    std::lock_guard lock(mtx_);
    if (scripts_.find(std::string(path)) != scripts_.end()) {
        return StatusCode::ALREADY_LOADED;
    }
    // Determine capacity (cached per call; inexpensive int fetch)
    std::size_t maxContexts = static_cast<std::size_t>(gb2d::ConfigurationManager::getInt("scripting.maxContexts", static_cast<int64_t>(kDefaultMaxScripts)));
    if (maxContexts == 0) {
        maxContexts = kDefaultMaxScripts; // guard against nonsensical zero/negative values
    }
    if (scripts_.size() >= maxContexts) {
        return StatusCode::RUNTIME_ERROR; // Future: distinct code for capacity exhausted
    }
    scripts_.emplace(std::string(path), LogCounters{});
    last_loaded_script_ = std::string(path);
    return StatusCode::OK;
}

StatusCode ScriptManager::unload(std::string_view path) {
    if (!is_initialized()) return StatusCode::NOT_INITIALIZED;
    std::lock_guard lock(mtx_);
    auto it = scripts_.find(std::string(path));
    if (it == scripts_.end()) {
        return StatusCode::INVALID_ID;
    }
    // Auto-close any windows owned by this script (T025). Best-effort: failure to close individual windows is ignored.
    gb2d_window__close_all_for_script(std::string(path));
    scripts_.erase(it);
    return StatusCode::OK;
}

bool ScriptManager::exists(std::string_view path) {
    std::lock_guard lock(mtx_);
    return scripts_.find(std::string(path)) != scripts_.end();
}

void ScriptManager::increment_info(std::string_view path) {
    std::lock_guard lock(mtx_);
    auto it = scripts_.find(std::string(path));
    if (it != scripts_.end()) it->second.info++;
}
void ScriptManager::increment_warn(std::string_view path) {
    std::lock_guard lock(mtx_);
    auto it = scripts_.find(std::string(path));
    if (it != scripts_.end()) it->second.warn++;
}
void ScriptManager::increment_error(std::string_view path) {
    std::lock_guard lock(mtx_);
    auto it = scripts_.find(std::string(path));
    if (it != scripts_.end()) it->second.error++;
}
StatusCode ScriptManager::get_counters(std::string_view path, LogCounters& out) {
    std::lock_guard lock(mtx_);
    auto it = scripts_.find(std::string(path));
    if (it == scripts_.end()) return StatusCode::INVALID_ID;
    out = it->second;
    return StatusCode::OK;
}

std::string ScriptManager::get_last_loaded_script() {
    std::lock_guard lock(mtx_);
    return last_loaded_script_;
}

StatusCode ScriptManager::request_reload(std::string_view path) {
    if (!is_initialized()) return StatusCode::NOT_INITIALIZED;
    std::lock_guard lock(mtx_);
    auto spath = std::string(path);
    if (scripts_.find(spath) == scripts_.end()) {
        return StatusCode::INVALID_ID;
    }
    using clock = std::chrono::steady_clock;
    auto now = clock::now();
    int64_t debounceMs = gb2d::ConfigurationManager::getInt("scripting.reload.debounceMs", 500);
    // Step 2: clamp & warn on invalid debounce values.
    static bool warnedNegative = false;
    static bool warnedHuge = false;
    constexpr int64_t kMaxDebounceMsReasonable = 10000; // 10s practical upper bound
    if (debounceMs < 0) {
        if (!warnedNegative && (gb2d::logging::LogManager::isInitialized() || gb2d::logging::LogManager::init() == gb2d::logging::Status::ok)) {
            gb2d::logging::LogManager::warn("[debounce] Negative debounce value {}ms adjusted to 0 (no debounce)", debounceMs);
            warnedNegative = true;
        }
        debounceMs = 0; // treat negative as zero (no debounce)
    } else if (debounceMs > kMaxDebounceMsReasonable) {
        if (!warnedHuge && (gb2d::logging::LogManager::isInitialized() || gb2d::logging::LogManager::init() == gb2d::logging::Status::ok)) {
            gb2d::logging::LogManager::warn("[debounce] Excessive debounce value {}ms clamped to {}ms", debounceMs, kMaxDebounceMsReasonable);
            warnedHuge = true;
        }
        debounceMs = kMaxDebounceMsReasonable;
    }
    auto it = last_reload_request_.find(spath);
    bool accept = true;
    std::int64_t elapsed = -1;
    if (it != last_reload_request_.end()) {
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
        if (elapsed < debounceMs) {
            accept = false; // debounced; ignore
        }
    }
    if (accept) {
        last_reload_request_[spath] = now;
        ++effective_reload_requests_;
        // Execute the simulated unload+load cycle now (synchronous for simplicity).
        auto rc = perform_reload_unlocked(spath);
        if (rc != StatusCode::OK) {
            return rc; // propagate any failure status (future: differentiate error types)
        }
    } else {
        // Step 1: emit a debug/info level log indicating suppression (best-effort; ignore failures)
        if (gb2d::logging::LogManager::isInitialized() ||
            gb2d::logging::LogManager::init() == gb2d::logging::Status::ok) {
            // Use info for now (no dedicated debug API exposed in wrapper yet)
            gb2d::logging::LogManager::info("[debounce] Reload suppressed for '{}' ({}ms < {}ms window)", spath, elapsed, debounceMs);
        }
        return StatusCode::SUPPRESSED; // (Step 3) Distinguish coalesced requests.
    }
    return StatusCode::OK;
}

StatusCode ScriptManager::perform_reload_unlocked(const std::string& path) {
    // Preconditions: path exists in scripts_
    // Simulate unload
    auto it = scripts_.find(path);
    if (it == scripts_.end()) {
        return StatusCode::INVALID_ID; // race (should not happen under lock)
    }
    LogCounters counters = it->second; // preserve counters across reload for now
    scripts_.erase(it);
    // Simulate load (re-insert)
    scripts_.emplace(path, counters);
    reload_cycles_[path]++;
    // Could add logging
    return StatusCode::OK;
}

#ifdef GB2D_INTERNAL_TESTING
void ScriptManager::reset_for_tests() {
    std::lock_guard lock(mtx_);
    scripts_.clear();
    initialized_ = false;
    last_loaded_script_.clear();
    last_reload_request_.clear();
    effective_reload_requests_ = 0;
}
#endif

} // namespace gb2d::interop
