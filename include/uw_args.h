#pragma once

/*
 * This file contains both function argument helpers
 * and command line arguments parsing API.
 */

#include <stdarg.h>

#include <uw_status.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************
 * Function argument helpers
 */

/*
 * Argument validation macro
 */
#define uw_expect(value_type, arg)  \
    do {  \
        if (!uw_is_##value_type(_Generic((arg),  \
                _UwValue:   &(arg), \
                UwValuePtr: (arg)   \
            ))) {  \
            if (uw_error(_Generic((arg),  \
                    _UwValue:   &(arg), \
                    UwValuePtr: (arg)))) {  \
                return _Generic((arg),  \
                    _UwValue:   uw_move, \
                    UwValuePtr: uw_clone  \
                )(_Generic((arg),  \
                    _UwValue:   &(arg), \
                    UwValuePtr: arg  \
                ));  \
            }  \
            return UwError(UW_ERROR_INCOMPATIBLE_TYPE);  \
        }  \
    } while (false)


static inline void _uw_destroy_args(va_list ap)
/*
 * Helper for functions that accept values created on stack during function call.
 * Such values cannot be automatically cleaned on error, the callee
 * should do that.
 * See UwArray(), uw_array_append_va, UwMap(), uw_map_update_va
 */
{
    for (;;) {{
        UwValue arg = va_arg(ap, _UwValue);
        if (uw_va_end(&arg)) {
            break;
        }
    }}
}

/****************************************************************
 * Command line arguments parsing
 */

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
