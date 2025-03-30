#pragma once

#ifdef __cplusplus
extern "C" {
#endif

UwResult uw_parse_kvargs(int argc, char* argv[]);
/*
 * Parse argv as key=value. If argument contains no '=', then value is null.
 *
 * key for argv[0] is 0.
 *
 * Assume argv in utf-8
 */

#ifdef __cplusplus
}
#endif
