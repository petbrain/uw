#pragma once

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Traversing cyclic references requires the list of parents with reference counts.
 * Parent reference count is necessary for cases when, say, a list contains items
 * pointing to the same other list.
 */

#define UW_PARENTS_CHUNK_SIZE  8

typedef struct {
    /*
     * Pointers and refcounts on the list of parents are allocated in chunks
     * because alignment for pointer and integer can be different.
     */
    struct __UwCompoundData* parents[UW_PARENTS_CHUNK_SIZE];
    unsigned parents_refcount[UW_PARENTS_CHUNK_SIZE];
} _UwParentsChunk;

struct __UwCompoundData;
typedef struct __UwCompoundData _UwCompoundData;
struct __UwCompoundData {
    /*
     * We need reference count, so we embed struct data.
     */
    _UwStructData struct_data;

    /*
     * The minimal structure for tracking circular references
     * is capable to hold two pointers to parent values.
     *
     * For more back references a list is allocated.
     */
    bool destroying;
    struct {
        union {
            ptrdiff_t using_parents_list: 1;    // given that pointers are aligned, we use least significant bit for the flag
            _UwCompoundData* parents[2];        // using_parents_list == 0, using pointers as is
            struct {
                _UwParentsChunk* parents_list;  // using_parents_list == 1, this points to the list of other parents
                unsigned num_parents_chunks;
            };
        };
        unsigned parents_refcount[2];
    };
};

#define _uw_compound_data_ptr(value)  ((_UwCompoundData*) ((value)->struct_data))


/*
 * methods for use in statically defined descendant types
 */

void _uw_compound_destroy(UwValuePtr self);

#ifdef __cplusplus
}
#endif
