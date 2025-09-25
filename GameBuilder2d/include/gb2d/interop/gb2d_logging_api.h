#pragma once

// gb2d_logging_api.h
// C ABI for logging from managed scripts into native logger.

#include "gb2d/interop/gb2d_status_codes.h"

// Export macro portability
#if defined(_WIN32) || defined(__CYGWIN__)
	#define GB2D_LOGGING_API __declspec(dllexport)
#else
	#define GB2D_LOGGING_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

GB2D_LOGGING_API gb2d::interop::StatusCode gb2d_log_info(const char* message_utf8);
GB2D_LOGGING_API gb2d::interop::StatusCode gb2d_log_warn(const char* message_utf8);
GB2D_LOGGING_API gb2d::interop::StatusCode gb2d_log_error(const char* message_utf8);

// Retrieves per-script log counters (info, warn, error) into provided out parameters.
// Returns OK or INVALID_ID / NOT_INITIALIZED / BAD_FORMAT.
GB2D_LOGGING_API gb2d::interop::StatusCode gb2d_log_get_counters(const char* script_path,
	uint64_t* out_info, uint64_t* out_warn, uint64_t* out_error);

#ifdef __cplusplus
}
#endif
