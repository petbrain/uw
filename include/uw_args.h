#pragma once

#ifdef __cplusplus
extern "C" {
#endif

UwResult uw_parse_kvargs(int argc, char* argv[]);
/*
 * The function returns a mapping.
 *
 * The encoding for arguments is assumed to be UTF-8.
 *
 * Arguments starting from 1 are expected in the form of key=value.
 * If an argument contains no '=', it goes to key and the value is set to null.
 *
 * argv[0] is added to the mapping as is under key 0.
 */

#ifdef __cplusplus
}
#endif
