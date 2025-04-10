#pragma once

#include <uw.h>

#ifdef __cplusplus
extern "C" {
#endif

UwResult uw_to_json(UwValuePtr value, unsigned indent);
/*
 * Convert `value` to JSON string.
 *
 * If `indent` is nonzero, the result is formatted with indentation.
 */

#ifdef __cplusplus
}
#endif
