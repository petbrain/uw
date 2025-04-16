#pragma once

#include <uw_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************
 * Assertions and panic
 *
 * Unlike assertions from standard library they cannot be
 * turned off and can be used for input parameters validation.
 */

#define uw_assert(condition) \
    ({  \
        if (_uw_unlikely( !(condition) )) {  \
            uw_panic("UW assertion failed at %s:%s:%d: " #condition "\n", __FILE__, __func__, __LINE__);  \
        }  \
    })

[[noreturn]]
void uw_panic(char* fmt, ...);

[[noreturn]]
void _uw_panic_bad_charptr_subtype(UwValuePtr v);
/*
 * Implemented in src/uw_charptr.c
 */

[[ noreturn ]]
void _uw_panic_no_interface(UwTypeId type_id, unsigned interface_id);
/*
 * Implemented in src/uw_interfaces.c
 */

[[ noreturn ]]
void _uw_panic_bad_char_size(uint8_t char_size);
/*
 * Implemented in src/uw_string.c
 */

#ifdef __cplusplus
}
#endif
