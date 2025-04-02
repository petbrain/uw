#pragma once

/*
 * Array internals.
 */

#include "include/uw_base.h"
#include "src/uw_compound_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// the following constants must be power of two:
#define UWARRAY_INITIAL_CAPACITY    4
#define UWARRAY_CAPACITY_INCREMENT  16

typedef struct {
    UwValuePtr items;
    unsigned length;
    unsigned capacity;
} _UwArray;

extern UwType _uw_array_type;

/****************************************************************
 * Helpers
 */

static inline unsigned _uw_array_length(_UwArray* array)
{
    return array->length;
}

static inline unsigned _uw_array_capacity(_UwArray* array)
{
    return array->capacity;
}

bool _uw_alloc_array(UwTypeId type_id, _UwArray* array, unsigned capacity);
/*
 * - allocate array items
 * - set array->length = 0
 * - set array->capacity = rounded capacity
 *
 * Return true if array->items is not nullptr.
 */

bool _uw_array_resize(UwTypeId type_id, _UwArray* array, unsigned desired_capacity);
/*
 * Reallocate array.
 */

void _uw_destroy_array(UwTypeId type_id, _UwArray* array, UwValuePtr parent);
/*
 * Call destructor for all items and free the array items.
 * For compound values call _uw_abandon before the destructor.
 */

bool _uw_array_eq(_UwArray* a, _UwArray* b);
/*
 * Compare for equality.
 */

bool _uw_array_append_item(UwTypeId type_id, _UwArray* array, UwValuePtr item, UwValuePtr parent);
/*
 * Append: move `item` on the array using uw_move() and call _uw_embrace(parent, item)
 */

UwResult _uw_array_pop(_UwArray* array);
/*
 * Pop item from array.
 */

void _uw_array_del(_UwArray* array, unsigned start_index, unsigned end_index);
/*
 * Delete items from array.
 */

#ifdef __cplusplus
}
#endif
