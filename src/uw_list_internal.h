#pragma once

/*
 * List internals.
 */

#include "include/uw_base.h"
#include "src/uw_compound_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// the following constants must be power of two:
#define UWLIST_INITIAL_CAPACITY    4
#define UWLIST_CAPACITY_INCREMENT  16

typedef struct {
    UwValuePtr items;
    unsigned length;
    unsigned capacity;
} _UwList;

extern UwType _uw_list_type;

/****************************************************************
 * Helpers
 */

static inline unsigned _uw_list_length(_UwList* list)
{
    return list->length;
}

static inline unsigned _uw_list_capacity(_UwList* list)
{
    return list->capacity;
}

bool _uw_alloc_list(UwTypeId type_id, _UwList* list, unsigned capacity);
/*
 * - allocate list items
 * - set list->length = 0
 * - set list->capacity = rounded capacity
 *
 * Return true if list->items is not nullptr.
 */

bool _uw_list_resize(UwTypeId type_id, _UwList* list, unsigned desired_capacity);
/*
 * Reallocate list.
 */

void _uw_destroy_list(UwTypeId type_id, _UwList* list, UwValuePtr parent);
/*
 * Call destructor for all items and free the list items.
 * For compound values call _uw_abandon before the destructor.
 */

bool _uw_list_eq(_UwList* a, _UwList* b);
/*
 * Compare for equality.
 */

bool _uw_list_append_item(UwTypeId type_id, _UwList* list, UwValuePtr item, UwValuePtr parent);
/*
 * Append: move `item` on the list using uw_move() and call _uw_embrace(parent, item)
 */

UwResult _uw_list_pop(_UwList* list);
/*
 * Pop item from list.
 */

void _uw_list_del(_UwList* list, unsigned start_index, unsigned end_index);
/*
 * Delete items from list.
 */

#ifdef __cplusplus
}
#endif
