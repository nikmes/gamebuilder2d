#pragma once

// gb2d_interop.h
// High-level C ABI entry points (bootstrap & script management) - stub phase.
// Will be expanded to load/unload/reload scripts. Integration tests (T010-T014)
// will currently fail until implemented.

#include <stdint.h>
#include "gb2d/interop/gb2d_status_codes.h"

// Export macro portability
#if defined(_WIN32) || defined(__CYGWIN__)
	#define GB2D_INTEROP_API __declspec(dllexport)
#else
	#define GB2D_INTEROP_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Bring StatusCode into this scope for simpler declarations.
#ifdef __cplusplus
using gb2d::interop::StatusCode;
#endif

// Initializes the embedded runtime host (idempotent). Returns OK or INTERNAL_ERROR.
GB2D_INTEROP_API StatusCode gb2d_runtime_initialize();

// Loads (or reloads) a script by path. On first load returns OK; on duplicate returns ALREADY_LOADED.
GB2D_INTEROP_API StatusCode gb2d_script_load(const char* /*path*/);

// Unloads a previously loaded script (graceful). Returns INVALID_ID if unknown.
GB2D_INTEROP_API StatusCode gb2d_script_unload(const char* /*path*/);

// Reload request (debounced). Returns OK if scheduled, INVALID_ID if not loaded.
GB2D_INTEROP_API StatusCode gb2d_script_request_reload(const char* /*path*/);

// Sets the active script context for the current thread. Logging calls will attribute
// to this script if it is a currently loaded script. Returns OK, INVALID_ID if script
// not loaded, BAD_FORMAT if path invalid, or NOT_INITIALIZED if runtime not ready.
GB2D_INTEROP_API StatusCode gb2d_script_set_active(const char* /*path*/);

// Clears the active script context for the current thread. Future logging calls will
// fall back to heuristic attribution (currently last loaded) until a new active is set.
GB2D_INTEROP_API StatusCode gb2d_script_clear_active();

// Internal (not part of stable contract yet): query if runtime initialized. Used by other bridge units.
#ifdef __cplusplus
GB2D_INTEROP_API bool gb2d_runtime_is_initialized();
#endif

#ifdef GB2D_INTERNAL_TESTING
// Resets runtime initialization state (tests only). Returns OK always.
GB2D_INTEROP_API StatusCode gb2d_runtime__reset_for_tests();
// Returns number of effective (non-debounced) reload requests processed (tests only).
GB2D_INTEROP_API std::uint64_t gb2d_test_effective_reload_requests();
#endif

#ifdef __cplusplus
}
#endif
