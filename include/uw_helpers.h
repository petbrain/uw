#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define UW_LENGTH(array)  (sizeof(array) / sizeof((array)[0]))
/*
 * Get array length
 */

#define _uw_likely(x)    __builtin_expect(!!(x), 1)
#define _uw_unlikely(x)  __builtin_expect(!!(x), 0)
/*
 * Branch optimization hints
 */

#ifdef __cplusplus
}
#endif
