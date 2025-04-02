#include <string.h>

#include "include/uw.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_list_internal.h"
#include "src/uw_string_internal.h"
#include "src/uw_struct_internal.h"

#define get_data_ptr(value)  ((_UwList*) _uw_get_data_ptr((value), UwTypeId_List))

[[noreturn]]
static void panic_status()
{
    uw_panic("List cannot contain Status values");
}

static void list_fini(UwValuePtr self)
{
    _uw_destroy_list(self->type_id, get_data_ptr(self), self);

    // call super method, we know the ancestor is Compound
    _uw_types[UwTypeId_Compound]->fini(self);

    // if we did not knew, then:
    // uw_ancestor_of(UwTypeId_List)->fini(self);
}

static UwResult list_init(UwValuePtr self, void* ctor_args)
{
    // XXX not using ctor_args for now

    // call super method, we know the ancestor is Compound
    UwValue status = _uw_types[UwTypeId_Compound]->init(self, ctor_args);

    // if we did not knew, then:
    // UwValue status = uw_ancestor_of(UwTypeId_List)->init(self, ctor_args);

    uw_return_if_error(&status);

    // allocate list

    if (_uw_alloc_list(self->type_id, get_data_ptr(self), UWLIST_INITIAL_CAPACITY)) {
        return UwOK();
    }
    list_fini(self);
    return UwOOM();
}

static void list_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    _UwList* list = get_data_ptr(self);
    UwValuePtr item_ptr = list->items;
    for (unsigned n = list->length; n; n--, item_ptr++) {
        _uw_call_hash(item_ptr, ctx);
    }
}

static UwResult list_deepcopy(UwValuePtr self)
{
    UwValue dest = uw_create(self->type_id);
    uw_return_if_error(&dest);

    _UwList* src_list = get_data_ptr(self);
    _UwList* dest_list = get_data_ptr(&dest);

    if (!_uw_list_resize(dest.type_id, dest_list, src_list->length)) {
        return UwOOM();
    }

    UwValuePtr src_item_ptr = src_list->items;
    UwValuePtr dest_item_ptr = dest_list->items;
    for (unsigned i = 0; i < src_list->length; i++) {
        *dest_item_ptr = uw_deepcopy(src_item_ptr);
        if (uw_error(dest_item_ptr)) {
            return uw_move(dest_item_ptr);
        }
        src_item_ptr++;
        dest_item_ptr++;
        dest_list->length++;
    }
    return uw_move(&dest);
}

static void list_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
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

    _UwList* list = get_data_ptr(self);
    fprintf(fp, "%u items, capacity=%u\n", list->length, list->capacity);

    next_indent += 4;
    UwValuePtr item_ptr = list->items;
    for (unsigned n = list->length; n; n--, item_ptr++) {
        _uw_call_dump(fp, item_ptr, next_indent, next_indent, &this_link);
    }
}

static UwResult list_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool list_is_true(UwValuePtr self)
{
    return get_data_ptr(self)->length;
}

static bool list_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return _uw_list_eq(get_data_ptr(self), get_data_ptr(other));
}

static bool list_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_List) {
            return _uw_list_eq(get_data_ptr(self), get_data_ptr(other));
        } else {
            // check base type
            t = _uw_types[t]->ancestor_id;
            if (t == UwTypeId_Null) {
                return false;
            }
        }
    }
}

UwType _uw_list_type = {
    .id             = UwTypeId_List,
    .ancestor_id    = UwTypeId_Compound,
    .name           = "List",
    .allocator      = &default_allocator,

    .create         = _uw_struct_create,
    .destroy        = _uw_compound_destroy,
    .clone          = _uw_struct_clone,
    .hash           = list_hash,
    .deepcopy       = list_deepcopy,
    .dump           = list_dump,
    .to_string      = list_to_string,
    .is_true        = list_is_true,
    .equal_sametype = list_equal_sametype,
    .equal          = list_equal,

    .data_offset    = sizeof(_UwCompoundData),
    .data_size      = sizeof(_UwList),

    .init           = list_init,
    .fini           = list_fini

    // [UwInterfaceId_RandomAccess] = &list_type_random_access_interface,
    // [UwInterfaceId_List]         = &list_type_list_interface
};

// make sure _UwCompoundData has correct padding
static_assert((sizeof(_UwCompoundData) & (alignof(_UwList) - 1)) == 0);


static unsigned round_capacity(unsigned capacity)
{
    if (capacity <= UWLIST_CAPACITY_INCREMENT) {
        return align_unsigned(capacity, UWLIST_INITIAL_CAPACITY);
    } else {
        return align_unsigned(capacity, UWLIST_CAPACITY_INCREMENT);
    }
}

UwResult _uw_list_create(...)
{
    va_list ap;
    va_start(ap);
    UwValue list = uw_create(UwTypeId_List);
    if (uw_error(&list)) {
        // uw_list_append_ap destroys args on exit, we must do the same
        _uw_destroy_args(ap);
        return uw_move(&list);
    }
    UwValue status = uw_list_append_ap(&list, ap);
    va_end(ap);
    uw_return_if_error(&status);
    return uw_move(&list);
}

bool _uw_alloc_list(UwTypeId type_id, _UwList* list, unsigned capacity)
{
    if (capacity >= (UINT_MAX - UWLIST_CAPACITY_INCREMENT)) {
        return false;
    }

    list->length = 0;
    list->capacity = round_capacity(capacity);

    unsigned memsize = list->capacity * sizeof(_UwValue);
    list->items = _uw_types[type_id]->allocator->allocate(memsize, true);

    return list->items != nullptr;
}

void _uw_destroy_list(UwTypeId type_id, _UwList* list, UwValuePtr parent)
{
    if (list->items) {
        UwValuePtr item_ptr = list->items;
        for (unsigned n = list->length; n; n--, item_ptr++) {
            if (uw_is_compound(item_ptr)) {
                _uw_abandon(parent, item_ptr);
            }
            uw_destroy(item_ptr);
        }
        unsigned memsize = list->capacity * sizeof(_UwValue);
        _uw_types[type_id]->allocator->release((void**) &list->items, memsize);
    }
}

bool _uw_list_resize(UwTypeId type_id, _UwList* list, unsigned desired_capacity)
{
    if (desired_capacity < list->length) {
        desired_capacity = list->length;
    } else if (desired_capacity >= (UINT_MAX - UWLIST_CAPACITY_INCREMENT)) {
        return false;
    }
    unsigned new_capacity = round_capacity(desired_capacity);

    Allocator* allocator = _uw_types[type_id]->allocator;

    unsigned old_memsize = list->capacity * sizeof(_UwValue);
    unsigned new_memsize = new_capacity * sizeof(_UwValue);

    if (!allocator->reallocate((void**) &list->items, old_memsize, new_memsize, true, nullptr)) {
        return false;
    }
    list->capacity = new_capacity;
    return true;
}

bool uw_list_resize(UwValuePtr list, unsigned desired_capacity)
{
    uw_assert_list(list);
    return _uw_list_resize(list->type_id, get_data_ptr(list), desired_capacity);
}

unsigned uw_list_length(UwValuePtr list)
{
    uw_assert_list(list);
    return _uw_list_length(get_data_ptr(list));
}

bool _uw_list_eq(_UwList* a, _UwList* b)
{
    unsigned n = a->length;
    if (b->length != n) {
        // lists have different lengths
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

bool _uw_list_append(UwValuePtr list, UwValuePtr item)
// XXX this will be an interface method, _uwi_list_append
{
    uw_assert_list(list);

    UwValue v = uw_clone(item);
    if (uw_error(&v)) {
        uw_destroy(&v);
        return false;
    }
    return _uw_list_append_item(list->type_id, get_data_ptr(list), &v, list);
}

static bool grow_list(UwTypeId type_id, _UwList* list)
{
    uw_assert(list->length <= list->capacity);

    if (list->length == list->capacity) {
        unsigned new_capacity;
        if (list->capacity <= UWLIST_CAPACITY_INCREMENT) {
            new_capacity = list->capacity + UWLIST_INITIAL_CAPACITY;
        } else {
            new_capacity = list->capacity + UWLIST_CAPACITY_INCREMENT;
        }
        if (!_uw_list_resize(type_id, list, new_capacity)) {
            return false;
        }
    }
    return true;
}

bool _uw_list_append_item(UwTypeId type_id, _UwList* list, UwValuePtr item, UwValuePtr parent)
{
    if (uw_is_status(item)) {
        // prohibit appending Status values
        panic_status();
    }
    if (!grow_list(type_id, list)) {
        return false;
    }
    _uw_embrace(parent, item);
    list->items[list->length] = uw_move(item);
    list->length++;
    return true;
}

UwResult _uw_list_append_va(UwValuePtr list, ...)
{
    va_list ap;
    va_start(ap);
    UwValue result = uw_list_append_ap(list, ap);
    va_end(ap);
    return uw_move(&result);
}

UwResult uw_list_append_ap(UwValuePtr dest, va_list ap)
{
    uw_assert_list(dest);

    UwTypeId type_id = dest->type_id;
    _UwList* list = get_data_ptr(dest);
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
        if (!_uw_list_append_item(type_id, list, &arg, dest)) {
            goto failure;
        }
        num_appended++;
    }}

failure:
    // rollback
    while (num_appended--) {
        UwValue v = _uw_list_pop(list);
        uw_destroy(&v);
    }
    // consume args
    _uw_destroy_args(ap);
    return uw_move(&error);
}

bool _uw_list_insert(UwValuePtr list, unsigned index, UwValuePtr item)
{
    if (uw_is_status(item)) {
        // prohibit appending Status values
        panic_status();
    }

    uw_assert_list(list);
    _UwList* _list = get_data_ptr(list);

    UwValue v = uw_clone(item);
    if (uw_error(&v)) {
        uw_destroy(&v);
        return false;
    }
    if (index > _list->length) {
        return false;
    }
    if (!grow_list(list->type_id, _list)) {
        return false;
    }
    _uw_embrace(list, item);
    if (index < _list->length) {
        memmove(&_list->items[index + 1], &_list->items[index], (_list->length - index) * sizeof(_UwValue));
    }
    _list->items[index] = uw_move(&v);
    _list->length++;
    return true;
}

UwResult uw_list_item(UwValuePtr self, int index)
{
    uw_assert_list(self);

    _UwList* list = get_data_ptr(self);

    if (index < 0) {
        index = list->length + index;
        if (index < 0) {
            return UwError(UW_ERROR_INDEX_OUT_OF_RANGE);
        }
    } else if (((unsigned) index) >= list->length) {
        return UwError(UW_ERROR_INDEX_OUT_OF_RANGE);
    }
    return uw_clone(&list->items[index]);
}

UwResult uw_list_set_item(UwValuePtr self, int index, UwValuePtr item)
{
    uw_assert_list(self);

    _UwList* list = get_data_ptr(self);

    if (index < 0) {
        index = list->length + index;
        if (index < 0) {
            return UwError(UW_ERROR_INDEX_OUT_OF_RANGE);
        }
    } else if (((unsigned) index) >= list->length) {
        return UwError(UW_ERROR_INDEX_OUT_OF_RANGE);
    }

    uw_destroy(&list->items[index]);
    list->items[index] = uw_clone(item);
    return UwOK();
}

UwResult uw_list_pop(UwValuePtr self)
{
    uw_assert_list(self);
    return _uw_list_pop(get_data_ptr(self));
}

UwResult _uw_list_pop(_UwList* list)
{
    if (list->length == 0) {
        return UwError(UW_ERROR_POP_FROM_EMPTY_LIST);
    }
    list->length--;
    return uw_move(&list->items[list->length]);
}

void uw_list_del(UwValuePtr self, unsigned start_index, unsigned end_index)
{
    uw_assert_list(self);
    _uw_list_del(get_data_ptr(self), start_index, end_index);
}

void uw_list_clean(UwValuePtr self)
{
    uw_assert_list(self);
    _uw_list_del(get_data_ptr(self), 0, UINT_MAX);
}

void _uw_list_del(_UwList* list, unsigned start_index, unsigned end_index)
{
    if (list->length == 0) {
        return;
    }
    if (end_index > list->length) {
        end_index = list->length;
    }
    if (start_index >= end_index) {
        return;
    }

    UwValuePtr item_ptr = &list->items[start_index];
    for (unsigned i = start_index; i < end_index; i++, item_ptr++) {
        uw_destroy(item_ptr);
    }
    unsigned new_length = list->length - (end_index - start_index);
    unsigned tail_length = list->length - end_index;
    if (tail_length) {
        memmove(&list->items[start_index], &list->items[end_index], tail_length * sizeof(_UwValue));
        memset(&list->items[new_length], 0, (list->length - new_length) * sizeof(_UwValue));
    }
    list->length = new_length;
}

UwResult uw_list_slice(UwValuePtr self, unsigned start_index, unsigned end_index)
{
    _UwList* src_list = get_data_ptr(self);
    unsigned length = _uw_list_length(src_list);

    if (end_index > length) {
        end_index = length;
    }
    if (start_index >= end_index) {
        // return empty list
        return UwList();
    }
    unsigned slice_len = end_index - start_index;

    UwValue dest = UwList();
    uw_return_if_error(&dest);

    if (!uw_list_resize(&dest, slice_len)) {
        return UwOOM();
    }
    _UwList* dest_list = get_data_ptr(&dest);

    UwValuePtr src_item_ptr = &src_list->items[start_index];
    UwValuePtr dest_item_ptr = dest_list->items;
    for (unsigned i = start_index; i < end_index; i++) {
        *dest_item_ptr = uw_clone(src_item_ptr);
        if (uw_error(dest_item_ptr)) {
            return uw_move(dest_item_ptr);
        }
        src_item_ptr++;
        dest_item_ptr++;
        dest_list->length++;
    }
    return uw_move(&dest);
}

UwResult _uw_list_join_c32(char32_t separator, UwValuePtr list)
{
    char32_t s[2] = {separator, 0};
    UwValue sep = UwChar32Ptr(s);
    return _uw_list_join(&sep, list);
}

UwResult _uw_list_join_u8(char8_t* separator, UwValuePtr list)
{
    UwValue sep = UwChar8Ptr(separator);
    return _uw_list_join(&sep, list);
}

UwResult _uw_list_join_u32(char32_t*  separator, UwValuePtr list)
{
    UwValue sep = UwChar32Ptr(separator);
    return _uw_list_join(&sep, list);
}

UwResult _uw_list_join(UwValuePtr separator, UwValuePtr list)
{
    unsigned num_items = uw_list_length(list);
    if (num_items == 0) {
        return UwString();
    }
    if (num_items == 1) {
        UwValue item = uw_list_item(list, 0);
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
    bool separator_is_string = uw_is_string(separator);

    if (separator_is_string) {
        max_char_size = uw_string_char_size(separator);
        separator_len = uw_strlen(separator);
    } if (uw_is_charptr(separator)) {
        separator_len = _uw_charptr_strlen2(separator, &max_char_size);
    } else {
        UwValue error = UwError(UW_ERROR_INCOMPATIBLE_TYPE);
        _uw_set_status_desc(&error, "Bad separator type for uw_list_join: %u, %s",
                            separator->type_id, uw_get_type_name(separator->type_id));
        return uw_move(&error);
    }

    // calculate total length and max char width of string items
    unsigned num_charptrs = 0;
    unsigned result_len = 0;
    for (unsigned i = 0; i < num_items; i++) {{   // nested scope for autocleaning item
        UwValue item = uw_list_item(list, i);
        if (uw_is_string(&item)) {
            if (i) {
                result_len += separator_len;
            }
            uint8_t char_size = uw_string_char_size(&item);
            if (max_char_size < char_size) {
                max_char_size = char_size;
            }
            result_len += uw_strlen(&item);
        } else if (uw_is_charptr(&item)) {
            if (i) {
                result_len += separator_len;
            }
            num_charptrs++;
        }
        // XXX skipping non-string values
    }}

    // can allocate array for CharPtr now
    unsigned charptr_len[num_charptrs + 1];

    if (num_charptrs) {
        // need one more pass to get lengths and char sizes of CharPtr items
        unsigned charptr_index = 0;
        for (unsigned i = 0; i < num_items; i++) {{   // nested scope for autocleaning item
            UwValue item = uw_list_item(list, i);
            if (uw_is_charptr(&item)) {
                uint8_t char_size;
                unsigned len = _uw_charptr_strlen2(&item, &char_size);

                charptr_len[charptr_index++] = len;

                result_len += len;
                if (max_char_size < char_size) {
                    max_char_size = char_size;
                }
            }
        }}
    }

    // join list items
    UwValue result = uw_create_empty_string(result_len, max_char_size);
    uw_return_if_error(&result);

    unsigned charptr_index = 0;
    for (unsigned i = 0; i < num_items; i++) {{   // nested scope for autocleaning item
        UwValue item = uw_list_item(list, i);
        bool is_string = uw_is_string(&item);
        if (is_string || uw_is_charptr(&item)) {
            if (i) {
                if (separator_is_string) {
                    if (!_uw_string_append(&result, separator)) {
                        return UwOOM();
                    }
                } else {
                    if (!_uw_string_append_charptr(&result, separator, separator_len, max_char_size)) {
                        return UwOOM();
                    }
                }
            }
            if (is_string) {
                if (!_uw_string_append(&result, &item)) {
                    return UwOOM();
                }
            } else {
                if (!_uw_string_append_charptr(&result, &item, charptr_len[charptr_index], max_char_size)) {
                    return UwOOM();
                }
                charptr_index++;
            }
        }
        // XXX skipping non-string values
    }}
    return uw_move(&result);
}

bool uw_list_dedent(UwValuePtr lines)
{
    static char32_t indent_chars[] = {' ', '\t', 0};

    unsigned n = uw_list_length(lines);

    // dedent inplace, so access items directly to avoid cloning
    _UwList* list = get_data_ptr(lines);

    unsigned indent[n];

    // measure indents
    unsigned min_indent = UINT_MAX;
    for (unsigned i = 0; i < n; i++) {
        UwValuePtr line = &list->items[i];
        if (uw_is_string(line)) {
            indent[i] = uw_string_skip_chars(line, 0, indent_chars);
            if (indent[i] && indent[i] < min_indent) {
                min_indent = indent[i];
            }
        } else {
            indent[i] = 0;
        }
    }
    if (min_indent == UINT_MAX || min_indent == 0) {
        // nothing to dedent
        return true;
    }

    for (unsigned i = 0; i < n; i++) {
        if (indent[i]) {
            UwValuePtr line = &list->items[i];
            if (!uw_string_erase(line, 0, min_indent)) {
                return false;
            }
        }
    }
    return true;
}
