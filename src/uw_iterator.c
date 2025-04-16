#include <limits.h>

#include "include/uw.h"
#include "src/uw_interfaces_internal.h"
#include "src/uw_struct_internal.h"

/****************************************************************
 * Iterator type
 */

typedef struct {
    _UwValue iterable;  // cloned iterable value
} _UwIterator;

#define get_data_ptr(value)  ((_UwIterator*) _uw_get_data_ptr((value), UwTypeId_Iterator))

static UwResult iterator_init(UwValuePtr self, void* ctor_args)
{
    UwIteratorCtorArgs* args = ctor_args;
    _UwIterator* data = get_data_ptr(self);
    data->iterable = uw_clone(args->iterable);
    return UwOK();
}

static void iterator_fini(UwValuePtr self)
{
    _UwIterator* data = get_data_ptr(self);
    uw_destroy(&data->iterable);
}

static void iterator_hash(UwValuePtr self, UwHashContext* ctx)
{
    uw_panic("Iterators do not support hashing");
}

static UwResult iterator_deepcopy(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static void iterator_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _UwIterator* data = get_data_ptr(self);

    _uw_dump_start(fp, self, first_indent);
    _uw_dump_struct_data(fp, self);
    _uw_print_indent(fp, next_indent);
    fputs("Iterable:", fp);
    _uw_call_dump(fp, &data->iterable, next_indent, next_indent, nullptr);
}

static UwResult iterator_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool iterator_is_true(UwValuePtr self)
{
    return false;
}

static bool iterator_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return false;
}

static bool iterator_equal(UwValuePtr self, UwValuePtr other)
{
    return false;
}

UwType _uw_iterator_type = {
    .id             = UwTypeId_Iterator,
    .ancestor_id    = UwTypeId_Struct,
    .name           = "Iterator",
    .allocator      = &default_allocator,

    .create         = _uw_struct_create,
    .destroy        = _uw_struct_destroy,
    .clone          = _uw_struct_clone,
    .hash           = iterator_hash,
    .deepcopy       = iterator_deepcopy,
    .dump           = iterator_dump,
    .to_string      = iterator_to_string,
    .is_true        = iterator_is_true,
    .equal_sametype = iterator_equal_sametype,
    .equal          = iterator_equal,

    .data_offset    = sizeof(_UwStructData),
    .data_size      = sizeof(_UwIterator),

    .init           = iterator_init,
    .fini           = iterator_fini
};

// make sure _UwStructData has correct padding
static_assert((sizeof(_UwStructData) & (alignof(_UwIterator) - 1)) == 0);
