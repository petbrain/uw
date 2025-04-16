#pragma once

#include <uw_types.h>

#ifdef __cplusplus
extern "C" {
#endif

UwResult uw_monotonic();
/*
 * Return current timestamp.
 */

UwResult uw_timestamp_sum(UwValuePtr a, UwValuePtr b);
/*
 * Calculate a + b
 */

UwResult uw_timestamp_diff(UwValuePtr a, UwValuePtr b);
/*
 * Calculate  a - b
 */

#ifdef __cplusplus
}
#endif
