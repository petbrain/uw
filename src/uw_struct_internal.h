#pragma once

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

UwResult _uw_struct_alloc(UwValuePtr self, void* ctor_args);
/*
 * Allocate struct_data and call init method.
 */

void _uw_struct_release(UwValuePtr self);
/*
 * Call init method, release struct_data and reset type of self to Null.
 */


/*
 * methods for use in statically defined descendant types
 */
UwResult _uw_struct_create(UwTypeId type_id, void* ctor_args);
void     _uw_struct_destroy(UwValuePtr self);
UwResult _uw_struct_clone(UwValuePtr self);
void     _uw_struct_hash(UwValuePtr self, UwHashContext* ctx);
UwResult _uw_struct_deepcopy(UwValuePtr self);
UwResult _uw_struct_to_string(UwValuePtr self);
bool     _uw_struct_is_true(UwValuePtr self);
bool     _uw_struct_equal_sametype(UwValuePtr self, UwValuePtr other);
bool     _uw_struct_equal(UwValuePtr self, UwValuePtr other);

#ifdef __cplusplus
}
#endif
