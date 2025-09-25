#include "gb2d/interop/gb2d_window_api.h"
#include "gb2d/interop/gb2d_interop.h"
#include "services/window/WindowManager.h"
#include "services/logger/LogManager.h"
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <string>
#include "gb2d/interop/utf8_validation.h"

using namespace gb2d::interop;

namespace {
    struct WindowRegistry {
        std::mutex mtx;
        std::unordered_map<gb2d_window_id, std::string> id_to_internal; // numeric -> manager string id
        std::unordered_map<std::string, gb2d_window_id> internal_to_id; // reverse
        // Ownership: script path -> set of window ids
        std::unordered_map<std::string, std::vector<gb2d_window_id>> script_windows;
        gb2d_window_id next_id{1}; // 0 reserved
        gb2d::WindowManager manager; // simple singleton-like instance for interop calls
    };
    WindowRegistry& registry() { static WindowRegistry r; return r; }

    inline bool runtime_ready() { return gb2d_runtime_is_initialized(); }

    inline StatusCode validate_title(const char* t) {
        if (!t) return StatusCode::BAD_FORMAT;
        std::size_t len = 0;
        if (!utf8::validate(t, GB2D_WINDOW_TITLE_MAX_BYTES, len)) return StatusCode::BAD_FORMAT;
        return StatusCode::OK;
    }
}

extern "C" {

// From interop runtime (thread-local active script path) accessor (C linkage)
extern const char* gb2d_script__active_path();

StatusCode gb2d_window_create(const char* title_utf8, int width, int height, gb2d_window_id* out_id) {
    if (!runtime_ready()) return StatusCode::NOT_INITIALIZED;
    if (!out_id) return StatusCode::BAD_FORMAT;
    if (width <= 0 || height <= 0) return StatusCode::BAD_FORMAT;
    if (auto v = validate_title(title_utf8); v != StatusCode::OK) return v;
    auto& r = registry();
    std::lock_guard lock(r.mtx);
    std::string internal_id = r.manager.createWindow(title_utf8 ? title_utf8 : "Window");
    gb2d_window_id numeric = r.next_id++;
    r.id_to_internal[numeric] = internal_id;
    r.internal_to_id[internal_id] = numeric;
        if (const char* active = gb2d_script__active_path(); active && *active) {
            auto& vec = r.script_windows[active];
        vec.push_back(numeric);
    }
    *out_id = numeric;
    return StatusCode::OK;
}

StatusCode gb2d_window_set_title(gb2d_window_id id, const char* title_utf8) {
    if (!runtime_ready()) return StatusCode::NOT_INITIALIZED;
    if (auto v = validate_title(title_utf8); v != StatusCode::OK) return v;
    auto& r = registry();
    std::lock_guard lock(r.mtx);
    auto it = r.id_to_internal.find(id);
    if (it == r.id_to_internal.end()) return StatusCode::INVALID_ID;
    bool ok = r.manager.setWindowTitle(it->second, title_utf8);
    return ok ? StatusCode::OK : StatusCode::INTERNAL_ERROR;
}

StatusCode gb2d_window_close(gb2d_window_id id) {
    if (!runtime_ready()) return StatusCode::NOT_INITIALIZED;
    auto& r = registry();
    std::lock_guard lock(r.mtx);
    auto it = r.id_to_internal.find(id);
    if (it == r.id_to_internal.end()) return StatusCode::INVALID_ID;
    bool ok = r.manager.closeWindow(it->second);
    r.internal_to_id.erase(it->second);
    r.id_to_internal.erase(it);
    return ok ? StatusCode::OK : StatusCode::INVALID_ID;
}

int gb2d_window_exists(gb2d_window_id id) {
    if (!runtime_ready()) return 0;
    auto& r = registry();
    std::lock_guard lock(r.mtx);
    return r.id_to_internal.count(id) ? 1 : 0;
}
} // extern "C"

// C++ linkage internal helper
std::size_t gb2d_window__close_all_for_script(const std::string& script_path) {
    if (!runtime_ready()) return 0;
    auto& r = registry();
    std::lock_guard lock(r.mtx);
    auto it = r.script_windows.find(script_path);
    if (it == r.script_windows.end()) return 0;
    std::size_t closed = 0;
    for (auto wid : it->second) {
        auto wit = r.id_to_internal.find(wid);
        if (wit != r.id_to_internal.end()) {
            r.manager.closeWindow(wit->second);
            r.internal_to_id.erase(wit->second);
            r.id_to_internal.erase(wit);
            ++closed;
        }
    }
    r.script_windows.erase(it);
    return closed;
}
