#include <limits.h>
#include <string.h>

#include "include/uw.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_array_internal.h"
#include "src/uw_string_internal.h"
#include "src/uw_struct_internal.h"

#define get_data_ptr(value)  ((_UwArray*) _uw_get_data_ptr((value), UwTypeId_Array))

[[noreturn]]
static void panic_status()
{
    uw_panic("Array cannot contain Status values");
}

static void array_fini(UwValuePtr self)
{
    _UwArray* array_data = get_data_ptr(self);
    uw_assert(array_data->itercount == 0);
    _uw_destroy_array(self->type_id, array_data, self);
}

static UwResult array_init(UwValuePtr self, void* ctor_args)
{
    // XXX not using ctor_args for now

    UwValue status = _uw_alloc_array(self->type_id, get_data_ptr(self), UWARRAY_INITIAL_CAPACITY);
    if (uw_error(&status)) {
        array_fini(self);
    }
    return uw_move(&status);
}

static void array_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    _UwArray* array_data = get_data_ptr(self);
    UwValuePtr item_ptr = array_data->items;
    for (unsigned n = array_data->length; n; n--, item_ptr++) {
        _uw_call_hash(item_ptr, ctx);
    }
}

static UwResult array_deepcopy(UwValuePtr self)
{
    UwValue dest = uw_create(self->type_id);
    uw_return_if_error(&dest);

    _UwArray* src_array = get_data_ptr(self);
    _UwArray* dest_array = get_data_ptr(&dest);

    uw_expect_ok( _uw_array_resize(dest.type_id, dest_array, src_array->length) );

    UwValuePtr src_item_ptr = src_array->items;
    UwValuePtr dest_item_ptr = dest_array->items;
    for (unsigned i = 0; i < src_array->length; i++) {
        *dest_item_ptr = uw_deepcopy(src_item_ptr);
        uw_return_if_error(dest_item_ptr);
        _uw_embrace(&dest, dest_item_ptr);
        src_item_ptr++;
        dest_item_ptr++;
        dest_array->length++;
    }
    return uw_move(&dest);
}

static void array_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    _uw_dump_struct_data(fp, self);
    _uw_dump_compound_data(fp, self, next_indent);
    _uw_print_indent(fp, next_indent);

    UwValuePtr value_seen = _uw_on_chain(self, tail);
    if (value_seen) {
        fprintf(fp, "already dumped: %p, data=%p\n", value_seen, value_seen->struct_data);
        return;
    }

    _UwCompoundChain this_link = {
        .prev = tail,
        .value = self
    };

    _UwArray* array_data = get_data_ptr(self);
    fprintf(fp, "%u items, capacity=%u\n", array_data->length, array_data->capacity);

    next_indent += 4;
    UwValuePtr item_ptr = array_data->items;
    for (unsigned n = array_data->length; n; n--, item_ptr++) {
        _uw_call_dump(fp, item_ptr, next_indent, next_indent, &this_link);
    }
}

static UwResult array_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool array_is_true(UwValuePtr self)
{
    return get_data_ptr(self)->length;
}

static bool array_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return _uw_array_eq(get_data_ptr(self), get_data_ptr(other));
}

static bool array_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_Array) {
            return _uw_array_eq(get_data_ptr(self), get_data_ptr(other));
        }
        // check base type
        t = _uw_types[t]->ancestor_id;
        if (t == UwTypeId_Null) {
            return false;
        }
    }
}

UwType _uw_array_type = {
    .id             = UwTypeId_Array,
    .ancestor_id    = UwTypeId_Compound,
    .name           = "Array",
    .allocator      = &default_allocator,

    .create         = _uw_struct_create,
    .destroy        = _uw_compound_destroy,
    .clone          = _uw_struct_clone,
    .hash           = array_hash,
    .deepcopy       = array_deepcopy,
    .dump           = array_dump,
    .to_string      = array_to_string,
    .is_true        = array_is_true,
    .equal_sametype = array_equal_sametype,
    .equal          = array_equal,

    .data_offset    = sizeof(_UwCompoundData),
    .data_size      = sizeof(_UwArray),

    .init           = array_init,
    .fini           = array_fini

    // [UwInterfaceId_RandomAccess] = &array_type_random_access_interface,
    // [UwInterfaceId_Array]        = &array_type_array_interface
};

// make sure _UwCompoundData has correct padding
static_assert((sizeof(_UwCompoundData) & (alignof(_UwArray) - 1)) == 0);


static unsigned round_capacity(unsigned capacity)
{
    if (capacity <= UWARRAY_CAPACITY_INCREMENT) {
        return align_unsigned(capacity, UWARRAY_INITIAL_CAPACITY);
    } else {
        return align_unsigned(capacity, UWARRAY_CAPACITY_INCREMENT);
    }
}

UwResult _uw_array_create(...)
{
    va_list ap;
    va_start(ap);
    UwValue array = uw_create(UwTypeId_Array);
    if (uw_error(&array)) {
        // uw_array_append_ap destroys args on exit, we must do the same
        _uw_destroy_args(ap);
        return uw_move(&array);
    }
    UwValue status = uw_array_append_ap(&array, ap);
    va_end(ap);
    uw_return_if_error(&status);
    return uw_move(&array);
}

UwResult _uw_alloc_array(UwTypeId type_id, _UwArray* array_data, unsigned capacity)
{
    if (capacity >= (UINT_MAX - UWARRAY_CAPACITY_INCREMENT)) {
        return UwError(UW_ERROR_DATA_SIZE_TOO_BIG);
    }

    array_data->length = 0;
    array_data->capacity = round_capacity(capacity);

    unsigned memsize = array_data->capacity * sizeof(_UwValue);
    array_data->items = _uw_types[type_id]->allocator->allocate(memsize, true);

    if (array_data->items) {
        return UwOK();
    } else {
        return UwOOM();
    }
}

void _uw_destroy_array(UwTypeId type_id, _UwArray* array_data, UwValuePtr parent)
{
    if (array_data->items) {
        UwValuePtr item_ptr = array_data->items;
        for (unsigned n = array_data->length; n; n--, item_ptr++) {
            if (uw_is_compound(item_ptr)) {
                _uw_abandon(parent, item_ptr);
            }
            uw_destroy(item_ptr);
        }
        unsigned memsize = array_data->capacity * sizeof(_UwValue);
        _uw_types[type_id]->allocator->release((void**) &array_data->items, memsize);
    }
}

UwResult _uw_array_resize(UwTypeId type_id, _UwArray* array_data, unsigned desired_capacity)
{
    if (desired_capacity < array_data->length) {
        desired_capacity = array_data->length;
    } else if (desired_capacity >= (UINT_MAX - UWARRAY_CAPACITY_INCREMENT)) {
        return UwError(UW_ERROR_DATA_SIZE_TOO_BIG);
    }
    unsigned new_capacity = round_capacity(desired_capacity);

    Allocator* allocator = _uw_types[type_id]->allocator;

    unsigned old_memsize = array_data->capacity * sizeof(_UwValue);
    unsigned new_memsize = new_capacity * sizeof(_UwValue);

    if (!allocator->reallocate((void**) &array_data->items, old_memsize, new_memsize, true, nullptr)) {
        return UwOOM();
    }
    array_data->capacity = new_capacity;
    return UwOK();
}

UwResult uw_array_resize(UwValuePtr array, unsigned desired_capacity)
{
    uw_assert_array(array);
    _UwArray* array_data = get_data_ptr(array);
    if (array_data->itercount) {
        return UwError(UW_ERROR_ITERATION_IN_PROGRESS);
    }
    return _uw_array_resize(array->type_id, array_data, desired_capacity);
}

unsigned uw_array_length(UwValuePtr array)
{
    uw_assert_array(array);
    return _uw_array_length(get_data_ptr(array));
}

bool _uw_array_eq(_UwArray* a, _UwArray* b)
{
    unsigned n = a->length;
    if (b->length != n) {
        // arrays have different lengths
        return false;
    }

    UwValuePtr a_ptr = a->items;
    UwValuePtr b_ptr = b->items;
    while (n) {
        if (!_uw_equal(a_ptr, b_ptr)) {
            return false;
        }
        n--;
        a_ptr++;
        b_ptr++;
    }
    return true;
}

UwResult _uw_array_append(UwValuePtr array, UwValuePtr item)
// XXX this will be an interface method, _uwi_array_append
{
    uw_assert_array(array);
    _UwArray* array_data = get_data_ptr(array);
    if (array_data->itercount) {
        return UwError(UW_ERROR_ITERATION_IN_PROGRESS);
    }
    UwValue v = uw_clone(item);
    return _uw_array_append_item(array->type_id, array_data, &v, array);
}

static UwResult grow_array(UwTypeId type_id, _UwArray* array_data)
{
    uw_assert(array_data->length <= array_data->capacity);

    if (array_data->length == array_data->capacity) {
        unsigned new_capacity;
        if (array_data->capacity <= UWARRAY_CAPACITY_INCREMENT) {
            new_capacity = array_data->capacity + UWARRAY_INITIAL_CAPACITY;
        } else {
            new_capacity = array_data->capacity + UWARRAY_CAPACITY_INCREMENT;
        }
        uw_expect_ok( _uw_array_resize(type_id, array_data, new_capacity) );
    }
    return UwOK();
}

UwResult _uw_array_append_item(UwTypeId type_id, _UwArray* array_data, UwValuePtr item, UwValuePtr parent)
{
    if (uw_is_status(item)) {
        // prohibit appending Status values
        panic_status();
    }
    uw_expect_ok( grow_array(type_id, array_data) );
    _uw_embrace(parent, item);
    array_data->items[array_data->length] = uw_move(item);
    array_data->length++;
    return UwOK();
}

UwResult _uw_array_append_va(UwValuePtr array, ...)
{
    va_list ap;
    va_start(ap);
    UwValue result = uw_array_append_ap(array, ap);
    va_end(ap);
    return uw_move(&result);
}

UwResult uw_array_append_ap(UwValuePtr dest, va_list ap)
{
    uw_assert_array(dest);
    _UwArray* array_data = get_data_ptr(dest);
    if (array_data->itercount) {
        return UwError(UW_ERROR_ITERATION_IN_PROGRESS);
    }
    UwTypeId type_id = dest->type_id;
    unsigned num_appended = 0;
    UwValue error = UwOOM();  // default error is OOM unless some arg is a status
    for(;;) {{
        UwValue arg = va_arg(ap, _UwValue);
        if (uw_is_status(&arg)) {
            if (uw_va_end(&arg)) {
                return UwOK();
            }
            uw_destroy(&error);
            error = uw_move(&arg);
            goto failure;
        }
        if (!uw_charptr_to_string_inplace(&arg)) {
            goto failure;
        }
        UwValue status = _uw_array_append_item(type_id, array_data, &arg, dest);
        if (uw_error(&status)) {
            uw_destroy(&error);
            error = uw_move(&status);
            goto failure;
        }
        num_appended++;
    }}

failure:
    // rollback
    while (num_appended--) {
        UwValue v = _uw_array_pop(array_data);
        uw_destroy(&v);
    }
    // consume args
    _uw_destroy_args(ap);
    return uw_move(&error);
}

UwResult _uw_array_insert(UwValuePtr array, unsigned index, UwValuePtr item)
{
    if (uw_is_status(item)) {
        // prohibit appending Status values
        panic_status();
    }
    uw_assert_array(array);
    _UwArray* array_data = get_data_ptr(array);
    if (array_data->itercount) {
        return UwError(UW_ERROR_ITERATION_IN_PROGRESS);
    }
    if (index > array_data->length) {
        return UwError(UW_ERROR_INDEX_OUT_OF_RANGE);
    }
    uw_expect_ok( grow_array(array->type_id, array_data) );

    _uw_embrace(array, item);
    if (index < array_data->length) {
        memmove(&array_data->items[index + 1], &array_data->items[index], (array_data->length - index) * sizeof(_UwValue));
    }
    array_data->items[index] = uw_clone(item);
    array_data->length++;
    return UwOK();
}

UwResult _uw_array_item_signed(UwValuePtr array, ssize_t index)
{
    uw_assert_array(array);

    _UwArray* array_data = get_data_ptr(array);

    if (index < 0) {
        index = array_data->length + index;
        if (index < 0) {
            return UwError(UW_ERROR_INDEX_OUT_OF_RANGE);
        }
    } else if (index >= array_data->length) {
        return UwError(UW_ERROR_INDEX_OUT_OF_RANGE);
    }
    return uw_clone(&array_data->items[index]);
}

UwResult _uw_array_item(UwValuePtr array, unsigned index)
{
    uw_assert_array(array);
    _UwArray* array_data = get_data_ptr(array);
    if (index < array_data->length) {
        return uw_clone(&array_data->items[index]);
    } else {
        return UwError(UW_ERROR_INDEX_OUT_OF_RANGE);
    }
}

UwResult _uw_array_set_item_signed(UwValuePtr array, ssize_t index, UwValuePtr item)
{
    uw_assert_array(array);
    _UwArray* array_data = get_data_ptr(array);
    if (array_data->itercount) {
        return UwError(UW_ERROR_ITERATION_IN_PROGRESS);
    }
    if (index < 0) {
        index = array_data->length + index;
        if (index < 0) {
            return UwError(UW_ERROR_INDEX_OUT_OF_RANGE);
        }
    } else if (index >= array_data->length) {
        return UwError(UW_ERROR_INDEX_OUT_OF_RANGE);
    }

    uw_destroy(&array_data->items[index]);
    array_data->items[index] = uw_clone(item);
    return UwOK();
}

UwResult _uw_array_set_item(UwValuePtr array, unsigned index, UwValuePtr item)
{
    uw_assert_array(array);
    _UwArray* array_data = get_data_ptr(array);
    if (array_data->itercount) {
        return UwError(UW_ERROR_ITERATION_IN_PROGRESS);
    }
    if (index < array_data->length) {
        uw_destroy(&array_data->items[index]);
        array_data->items[index] = uw_clone(item);
        return UwOK();
    } else {
        return UwError(UW_ERROR_INDEX_OUT_OF_RANGE);
    }
}

UwResult uw_array_pull(UwValuePtr array)
{
    uw_assert_array(array);
    _UwArray* array_data = get_data_ptr(array);
    if (array_data->itercount) {
        return UwError(UW_ERROR_ITERATION_IN_PROGRESS);
    }
    if (array_data->length == 0) {
        return UwError(UW_ERROR_EXTRACT_FROM_EMPTY_ARRAY);
    }
    _UwValue result = uw_clone(&array_data->items[0]);
    _uw_array_del(array_data, 0, 1);
    return result;
}

UwResult uw_array_pop(UwValuePtr array)
{
    uw_assert_array(array);
    _UwArray* array_data = get_data_ptr(array);
    if (array_data->itercount) {
        return UwError(UW_ERROR_ITERATION_IN_PROGRESS);
    }
    return _uw_array_pop(array_data);
}

UwResult _uw_array_pop(_UwArray* array_data)
{
    if (array_data->length == 0) {
        return UwError(UW_ERROR_EXTRACT_FROM_EMPTY_ARRAY);
    }
    array_data->length--;
    return uw_move(&array_data->items[array_data->length]);
}

void uw_array_del(UwValuePtr array, unsigned start_index, unsigned end_index)
{
    uw_assert_array(array);
    _UwArray* array_data = get_data_ptr(array);
    if (array_data->itercount) {
        return; // UwError(UW_ERROR_ITERATION_IN_PROGRESS);
    }
    _uw_array_del(array_data, start_index, end_index);
}

void uw_array_clean(UwValuePtr array)
{
    uw_assert_array(array);
    _UwArray* array_data = get_data_ptr(array);
    if (array_data->itercount) {
        return; // UwError(UW_ERROR_ITERATION_IN_PROGRESS);
    }
    _uw_array_del(array_data, 0, UINT_MAX);
}

void _uw_array_del(_UwArray* array_data, unsigned start_index, unsigned end_index)
{
    if (array_data->length == 0) {
        return;
    }
    if (end_index > array_data->length) {
        end_index = array_data->length;
    }
    if (start_index >= end_index) {
        return;
    }

    UwValuePtr item_ptr = &array_data->items[start_index];
    for (unsigned i = start_index; i < end_index; i++, item_ptr++) {
        uw_destroy(item_ptr);
    }
    unsigned new_length = array_data->length - (end_index - start_index);
    unsigned tail_length = array_data->length - end_index;
    if (tail_length) {
        memmove(&array_data->items[start_index], &array_data->items[end_index], tail_length * sizeof(_UwValue));
        memset(&array_data->items[new_length], 0, (array_data->length - new_length) * sizeof(_UwValue));
    }
    array_data->length = new_length;
}

UwResult uw_array_slice(UwValuePtr array, unsigned start_index, unsigned end_index)
{
    _UwArray* src_array = get_data_ptr(array);
    unsigned length = _uw_array_length(src_array);

    if (end_index > length) {
        end_index = length;
    }
    if (start_index >= end_index) {
        // return empty array
        return UwArray();
    }
    unsigned slice_len = end_index - start_index;

    UwValue dest = UwArray();
    uw_return_if_error(&dest);

    uw_expect_ok( uw_array_resize(&dest, slice_len) );

    _UwArray* dest_array = get_data_ptr(&dest);

    UwValuePtr src_item_ptr = &src_array->items[start_index];
    UwValuePtr dest_item_ptr = dest_array->items;
    for (unsigned i = start_index; i < end_index; i++) {
        *dest_item_ptr = uw_clone(src_item_ptr);
        src_item_ptr++;
        dest_item_ptr++;
        dest_array->length++;
    }
    return uw_move(&dest);
}

UwResult _uw_array_join_c32(char32_t separator, UwValuePtr array)
{
    char32_t s[2] = {separator, 0};
    UwValue sep = UwChar32Ptr(s);
    return _uw_array_join(&sep, array);
}

UwResult _uw_array_join_u8(char8_t* separator, UwValuePtr array)
{
    UwValue sep = UwCharPtr(separator);
    return _uw_array_join(&sep, array);
}

UwResult _uw_array_join_u32(char32_t*  separator, UwValuePtr array)
{
    UwValue sep = UwChar32Ptr(separator);
    return _uw_array_join(&sep, array);
}

UwResult _uw_array_join(UwValuePtr separator, UwValuePtr array)
{
    unsigned num_items = uw_array_length(array);
    if (num_items == 0) {
        return UwString();
    }
    if (num_items == 1) {
        UwValue item = uw_array_item(array, 0);
        if (uw_is_string(&item)) {
            return uw_move(&item);
        } if (uw_is_charptr(&item)) {
            return uw_charptr_to_string(&item);
        } else {
            // XXX skipping non-string values
            return UwString();
        }
    }

    uint8_t max_char_size;
    unsigned separator_len;

    if (uw_is_string(separator)) {
        max_char_size = _uw_string_char_size(separator);
        separator_len = _uw_string_length(separator);

    } if (uw_is_charptr(separator)) {
        separator_len = _uw_charptr_strlen2(separator, &max_char_size);

    } else {
        UwValue error = UwError(UW_ERROR_INCOMPATIBLE_TYPE);
        _uw_set_status_desc(&error, "Bad separator type for uw_array_join: %u, %s",
                            separator->type_id, uw_get_type_name(separator->type_id));
        return uw_move(&error);
    }

    // calculate total length and max char width of string items
    unsigned result_len = 0;
    for (unsigned i = 0; i < num_items; i++) {{   // nested scope for autocleaning item
        if (i) {
            result_len += separator_len;
        }
        UwValue item = uw_array_item(array, i);
        uint8_t char_size;
        if (uw_is_string(&item)) {
            char_size = _uw_string_char_size(&item);
            result_len += _uw_string_length(&item);
        } else if (uw_is_charptr(&item)) {
            result_len += _uw_charptr_strlen2(separator, &char_size);
        } else {
            // XXX skipping non-string values
            continue;
        }
        if (max_char_size < char_size) {
            max_char_size = char_size;
        }
    }}

    // join array items
    UwValue result = uw_create_empty_string(result_len, max_char_size);
    uw_return_if_error(&result);

    for (unsigned i = 0; i < num_items; i++) {{   // nested scope for autocleaning item
        UwValue item = uw_array_item(array, i);
        if (uw_is_string(&item) || uw_is_charptr(&item)) {
            if (i) {
                if (!_uw_string_append(&result, separator)) {
                    return UwOOM();
                }
            }
            if (!_uw_string_append(&result, &item)) {
                return UwOOM();
            }
        }
    }}
    return uw_move(&result);
}

UwResult uw_array_dedent(UwValuePtr lines)
{
    // dedent inplace, so access items directly to avoid cloning
    _UwArray* array_data = get_data_ptr(lines);

    if (array_data->itercount) {
        return UwError(UW_ERROR_ITERATION_IN_PROGRESS);
    }

    static char32_t indent_chars[] = {' ', '\t', 0};

    unsigned n = uw_array_length(lines);

    unsigned indent[n];

    // measure indents
    unsigned min_indent = UINT_MAX;
    for (unsigned i = 0; i < n; i++) {
        UwValuePtr line = &array_data->items[i];
        if (uw_is_string(line)) {
            indent[i] = uw_string_skip_chars(line, 0, indent_chars);
            if (_uw_string_length(line) && indent[i] < min_indent) {
                min_indent = indent[i];
            }
        } else {
            indent[i] = 0;
        }
    }
    if (min_indent == UINT_MAX || min_indent == 0) {
        // nothing to dedent
        return UwOK();
    }

    for (unsigned i = 0; i < n; i++) {
        if (indent[i]) {
            UwValuePtr line = &array_data->items[i];
            if (!uw_string_erase(line, 0, min_indent)) {
                return UwOOM();
            }
        }
    }
    return UwOK();
}
