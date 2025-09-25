#include "gb2d/interop/gb2d_interop.h"
#include <atomic>
#include <string>
#include "gb2d_script_manager.h"

using namespace gb2d::interop;

// Global runtime state (Phase T019). Will be expanded with hostfxr handles & script registry.
std::atomic<bool> g_initialized{false};

// Thread-local active script path for attribution (T024 Option B). C++ linkage.
thread_local std::string g_active_script_path;

extern "C" {

StatusCode gb2d_runtime_initialize() {
    bool expected = false;
    if (g_initialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        // First successful initialization path. Future: load hostfxr, configure search paths, etc.
        ScriptManager::instance().set_initialized();
        return StatusCode::OK;
    }
    // Already initialized: idempotent OK.
    return StatusCode::OK;
}

bool gb2d_runtime_is_initialized() {
    return g_initialized.load(std::memory_order_acquire);
}

// Internal helper for native subsystems (window auto-attribution) - not part of public header yet.
const char* gb2d_script__active_path() {
    return g_active_script_path.empty() ? nullptr : g_active_script_path.c_str();
}

StatusCode gb2d_script_load(const char* path) {
    if (!g_initialized.load(std::memory_order_acquire)) return StatusCode::NOT_INITIALIZED;
    if (!path) return StatusCode::BAD_FORMAT;
    return ScriptManager::instance().load(path);
}

StatusCode gb2d_script_unload(const char* path) {
    if (!g_initialized.load(std::memory_order_acquire)) return StatusCode::NOT_INITIALIZED;
    if (!path) return StatusCode::BAD_FORMAT;
    return ScriptManager::instance().unload(path);
}

StatusCode gb2d_script_request_reload(const char* path) {
    if (!g_initialized.load(std::memory_order_acquire)) return StatusCode::NOT_INITIALIZED;
    if (!path) return StatusCode::BAD_FORMAT;
    return ScriptManager::instance().request_reload(path);
}

// Active script context management

StatusCode gb2d_script_set_active(const char* path) {
    if (!g_initialized.load(std::memory_order_acquire)) return StatusCode::NOT_INITIALIZED;
    if (!path || *path == '\0') return StatusCode::BAD_FORMAT;
    // Validate script exists
    if (!ScriptManager::instance().exists(path)) return StatusCode::INVALID_ID;
    g_active_script_path = path;
    return StatusCode::OK;
}

StatusCode gb2d_script_clear_active() {
    if (!g_initialized.load(std::memory_order_acquire)) return StatusCode::NOT_INITIALIZED;
    g_active_script_path.clear();
    return StatusCode::OK;
}

#ifdef GB2D_INTERNAL_TESTING
StatusCode gb2d_runtime__reset_for_tests() {
    g_initialized.store(false, std::memory_order_release);
    ScriptManager::instance().reset_for_tests();
    g_active_script_path.clear();
    return StatusCode::OK;
}

std::uint64_t gb2d_test_effective_reload_requests() {
    return ScriptManager::instance().test_effective_reload_requests();
}
#endif

} // extern "C"

// (moved definition to top of file)
