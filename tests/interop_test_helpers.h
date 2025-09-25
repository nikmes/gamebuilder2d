#pragma once
// interop_test_helpers.h
// Internal test-only helpers for isolating interop state between tests.
// Requires gb2d runtime built with GB2D_INTERNAL_TESTING.

#include "gb2d/interop/gb2d_interop.h"

inline void gb2d_reset_runtime_for_test() {
#ifdef GB2D_INTERNAL_TESTING
    gb2d_runtime__reset_for_tests();
#endif
}
