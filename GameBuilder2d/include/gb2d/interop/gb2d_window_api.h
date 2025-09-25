#pragma once

// gb2d_window_api.h
// C ABI for window operations invoked from managed code.
// NOTE: Stub implementations currently return NOT_INITIALIZED until runtime + script manager are in place.

#include <stdint.h>
#include <cstddef> // for std::size_t
#include <string>
#include "gb2d/interop/gb2d_status_codes.h"

// Export macro portability
#if defined(_WIN32) || defined(__CYGWIN__)
	#define GB2D_WINDOW_API __declspec(dllexport)
#else
	#define GB2D_WINDOW_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t gb2d_window_id; // 0 reserved for invalid

// Constraints (shared with tests):
//  - Title must be valid UTF-8
//  - Title length (in bytes) 1..GB2D_WINDOW_TITLE_MAX_BYTES (inclusive)
//  - Zero-length or oversized titles return BAD_FORMAT
//  - Width/height must be > 0
// Max bytes chosen to comfortably fit typical UI tab labels while avoiding abuse.
inline constexpr std::size_t GB2D_WINDOW_TITLE_MAX_BYTES = 256;

// Creates a window with UTF-8 title. width/height > 0 required.
// Returns OK and sets out_id, or BAD_FORMAT / INTERNAL_ERROR / NOT_INITIALIZED.
GB2D_WINDOW_API gb2d::interop::StatusCode gb2d_window_create(const char* title_utf8, int width, int height, gb2d_window_id* out_id);

// Sets window title (UTF-8). Returns OK / INVALID_ID / BAD_FORMAT / NOT_INITIALIZED.
GB2D_WINDOW_API gb2d::interop::StatusCode gb2d_window_set_title(gb2d_window_id id, const char* title_utf8);

// Closes window. Returns OK / INVALID_ID / NOT_INITIALIZED.
GB2D_WINDOW_API gb2d::interop::StatusCode gb2d_window_close(gb2d_window_id id);

// Returns 1 if exists, 0 if not. If runtime not initialized returns 0.
GB2D_WINDOW_API int gb2d_window_exists(gb2d_window_id id);

// NOTE: Internal helpers follow after the extern "C" block with normal C++ linkage.

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
// Internal helper (not part of public C ABI): close all windows for a script path (used by ScriptManager unload auto-close T025).
// C++ linkage on purpose (takes std::string). Not exported with GB2D_WINDOW_API.
std::size_t gb2d_window__close_all_for_script(const std::string& script_path);
#endif
