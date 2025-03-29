#pragma once

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

extern UwType _uw_charptr_type;

UwResult uw_charptr_to_string(UwValuePtr self);
bool uw_charptr_to_string_inplace(UwValuePtr v);
/*
 * If `v` is CharPtr, convert it to String in place.
 * Return false if OOM.
 */

unsigned _uw_charptr_strlen2(UwValuePtr charptr, uint8_t* char_size);

bool _uw_charptr_equal_string(UwValuePtr charptr, UwValuePtr str);
// comparison helper

#ifdef __cplusplus
}
#endif
