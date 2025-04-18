#include <stdarg.h>

#include <libpussy/mmarray.h>

#include "include/uw.h"
#include "src/uw_array_internal.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_compound_internal.h"
#include "src/uw_interfaces_internal.h"
#include "src/uw_string_internal.h"
#include "src/uw_struct_internal.h"

UwType** _uw_types = nullptr;
static UwTypeId num_uw_types = 0;

/****************************************************************
 * Null type
 */

static UwResult null_create(UwTypeId type_id, void* ctor_args)
{
    return UwNull();
}

static void null_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, UwTypeId_Null);
}

static void null_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fputc('\n', fp);
}

static UwResult null_to_string(UwValuePtr self)
{
    return UwString_1_12(4, 'n', 'u', 'l', 'l', 0, 0, 0, 0, 0, 0, 0, 0);
}

static bool null_is_true(UwValuePtr self)
{
    return false;
}

static bool null_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return true;
}

static bool null_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Null:
                return true;

            case UwTypeId_CharPtr:
            case UwTypeId_Ptr:
                return other->ptr == nullptr;

            default: {
                // check base type
                t = _uw_types[t]->ancestor_id;
                if (t == UwTypeId_Null) {
                    return false;
                }
            }
        }
    }
}

static UwType null_type = {
    .id             = UwTypeId_Null,
    .ancestor_id    = UwTypeId_Null,  // no ancestor; Null can't be an ancestor for any type
    .name           = "Null",
    .create         = null_create,
    .destroy        = nullptr,
    .clone          = nullptr,
    .hash           = null_hash,
    .deepcopy       = nullptr,
    .dump           = null_dump,
    .to_string      = null_to_string,
    .is_true        = null_is_true,
    .equal_sametype = null_equal_sametype,
    .equal          = null_equal
};

/****************************************************************
 * Bool type
 */

static UwResult bool_create(UwTypeId type_id, void* ctor_args)
{
    // XXX use ctor_args for initializer?
    return UwBool(false);
}

static void bool_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: the hash should be the same for subtypes, that's why not using self->type_id here
    _uw_hash_uint64(ctx, UwTypeId_Bool);
    _uw_hash_uint64(ctx, self->bool_value);
}

static void bool_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %s\n", self->bool_value? "true" : "false");
}

static UwResult bool_to_string(UwValuePtr self)
{
    if (self->bool_value) {
        return UwString_1_12(4, 't', 'r', 'u', 'e', 0, 0, 0, 0, 0, 0, 0, 0);
    } else {
        return UwString_1_12(5, 'f', 'a', 'l', 's', 'e', 0, 0, 0, 0, 0, 0, 0);
    }
}

static bool bool_is_true(UwValuePtr self)
{
    return self->bool_value;
}

static bool bool_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->bool_value == other->bool_value;
}

static bool bool_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_Bool) {
            return self->bool_value == other->bool_value;
        }
        // check base type
        t = _uw_types[t]->ancestor_id;
        if (t == UwTypeId_Null) {
            return false;
        }
    }
}

static UwType bool_type = {
    .id             = UwTypeId_Bool,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "Bool",
    .create         = bool_create,
    .destroy        = nullptr,
    .clone          = nullptr,
    .hash           = bool_hash,
    .deepcopy       = nullptr,
    .dump           = bool_dump,
    .to_string      = bool_to_string,
    .is_true        = bool_is_true,
    .equal_sametype = bool_equal_sametype,
    .equal          = bool_equal

    // [UwInterfaceId_Logic] = &bool_type_logic_interface
};

/****************************************************************
 * Abstract Integer type
 */

static UwResult int_create(UwTypeId type_id, void* ctor_args)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static void int_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, UwTypeId_Int);
}

static void int_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fputs(": abstract\n", fp);
}

static UwResult int_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool int_is_true(UwValuePtr self)
{
    return self->signed_value;
}

static bool int_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return true;
}

static bool int_equal(UwValuePtr self, UwValuePtr other)
{
    return false;
}

static UwType int_type = {
    .id             = UwTypeId_Int,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "Int",
    .create         = int_create,
    .destroy        = nullptr,
    .clone          = nullptr,
    .hash           = int_hash,
    .deepcopy       = nullptr,
    .dump           = int_dump,
    .to_string      = int_to_string,
    .is_true        = int_is_true,
    .equal_sametype = int_equal_sametype,
    .equal          = int_equal

    // [UwInterfaceId_Logic]      = &int_type_logic_interface,
    // [UwInterfaceId_Arithmetic] = &int_type_arithmetic_interface,
    // [UwInterfaceId_Bitwise]    = &int_type_bitwise_interface,
    // [UwInterfaceId_Comparison] = &int_type_comparison_interface
};

/****************************************************************
 * Signed type
 */

static UwResult signed_create(UwTypeId type_id, void* ctor_args)
{
    // XXX use ctor_args for initializer?
    return UwSigned(0);
}

static void signed_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: same signed and unsigned integers must have same hash, so
    if (self->signed_value < 0) {
        _uw_hash_uint64(ctx, UwTypeId_Signed);
    } else {
        _uw_hash_uint64(ctx, UwTypeId_Unsigned);
    }
    _uw_hash_uint64(ctx, self->signed_value);
}

static void signed_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %lld\n", (long long) self->signed_value);
}

static UwResult signed_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool signed_is_true(UwValuePtr self)
{
    return self->signed_value;
}

static bool signed_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->signed_value == other->signed_value;
}

static bool signed_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Signed:
                return self->signed_value == other->signed_value;

            case UwTypeId_Unsigned:
                if (self->signed_value < 0) {
                    return false;
                } else {
                    return self->signed_value == (UwType_Signed) other->unsigned_value;
                }

            case UwTypeId_Float:
                return self->signed_value == other->float_value;

            default: {
                // check base type
                t = _uw_types[t]->ancestor_id;
                if (t == UwTypeId_Null) {
                    return false;
                }
            }
        }
    }
}

static UwType signed_type = {
    .id             = UwTypeId_Signed,
    .ancestor_id    = UwTypeId_Int,
    .name           = "Signed",
    .create         = signed_create,
    .destroy        = nullptr,
    .clone          = nullptr,
    .hash           = signed_hash,
    .deepcopy       = nullptr,
    .dump           = signed_dump,
    .to_string      = signed_to_string,
    .is_true        = signed_is_true,
    .equal_sametype = signed_equal_sametype,
    .equal          = signed_equal

    // [UwInterfaceId_Logic]      = &int_type_logic_interface,
    // [UwInterfaceId_Arithmetic] = &int_type_arithmetic_interface,
    // [UwInterfaceId_Bitwise]    = &int_type_bitwise_interface,
    // [UwInterfaceId_Comparison] = &int_type_comparison_interface
};

/****************************************************************
 * Unsigned type
 */

static UwResult unsigned_create(UwTypeId type_id, void* ctor_args)
{
    // XXX use ctor_args for initializer?
    return UwUnsigned(0);
}

static void unsigned_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: same signed and unsigned integers must have same hash,
    // so using UwTypeId_Unsigned, not self->type_id
    _uw_hash_uint64(ctx, UwTypeId_Unsigned);
    _uw_hash_uint64(ctx, self->unsigned_value);
}

static void unsigned_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %llu\n", (unsigned long long) self->unsigned_value);
}

static UwResult unsigned_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool unsigned_is_true(UwValuePtr self)
{
    return self->unsigned_value;
}

static bool unsigned_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->unsigned_value == other->unsigned_value;
}

static bool unsigned_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Signed:
                if (other->signed_value < 0) {
                    return false;
                } else {
                    return ((UwType_Signed) self->unsigned_value) == other->signed_value;
                }

            case UwTypeId_Unsigned:
                return self->unsigned_value == other->unsigned_value;

            case UwTypeId_Float:
                return self->unsigned_value == other->float_value;

            default: {
                // check base type
                t = _uw_types[t]->ancestor_id;
                if (t == UwTypeId_Null) {
                    return false;
                }
            }
        }
    }
}

static UwType unsigned_type = {
    .id             = UwTypeId_Unsigned,
    .ancestor_id    = UwTypeId_Int,
    .name           = "Unsigned",
    .create         = unsigned_create,
    .destroy        = nullptr,
    .clone          = nullptr,
    .hash           = unsigned_hash,
    .deepcopy       = nullptr,
    .dump           = unsigned_dump,
    .to_string      = unsigned_to_string,
    .is_true        = unsigned_is_true,
    .equal_sametype = unsigned_equal_sametype,
    .equal          = unsigned_equal

    // [UwInterfaceId_Logic]      = &int_type_logic_interface,
    // [UwInterfaceId_Arithmetic] = &int_type_arithmetic_interface,
    // [UwInterfaceId_Bitwise]    = &int_type_bitwise_interface,
    // [UwInterfaceId_Comparison] = &int_type_comparison_interface
};

/****************************************************************
 * Float type
 */

static UwResult float_create(UwTypeId type_id, void* ctor_args)
{
    // XXX use ctor_args for initializer?
    return UwFloat(0.0);
}

static void float_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: the hash should be the same for subtypes, that's why not using self->type_id here
    _uw_hash_uint64(ctx, UwTypeId_Float);
    _uw_hash_buffer(ctx, &self->float_value, sizeof(self->float_value));
}

static void float_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %f\n", self->float_value);
}

static UwResult float_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool float_is_true(UwValuePtr self)
{
    return self->float_value;
}

static bool float_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->float_value == other->float_value;
}

static bool float_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Signed:   return self->float_value == (UwType_Float) other->signed_value;
            case UwTypeId_Unsigned: return self->float_value == (UwType_Float) other->unsigned_value;
            case UwTypeId_Float:    return self->float_value == other->float_value;
            default: {
                // check base type
                t = _uw_types[t]->ancestor_id;
                if (t == UwTypeId_Null) {
                    return false;
                }
            }
        }
    }
}

static UwType float_type = {
    .id             = UwTypeId_Float,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "Float",
    .create         = float_create,
    .destroy        = nullptr,
    .clone          = nullptr,
    .hash           = float_hash,
    .deepcopy       = nullptr,
    .dump           = float_dump,
    .to_string      = float_to_string,
    .is_true        = float_is_true,
    .equal_sametype = float_equal_sametype,
    .equal          = float_equal

    // [UwInterfaceId_Logic]      = &float_type_logic_interface,
    // [UwInterfaceId_Arithmetic] = &float_type_arithmetic_interface,
    // [UwInterfaceId_Comparison] = &float_type_comparison_interface
};

/****************************************************************
 * DateTime type
 */

static UwResult datetime_create(UwTypeId type_id, void* ctor_args)
{
    // XXX use ctor_args for initializer?
    return UwDateTime();
}

static void datetime_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: the hash should be the same for subtypes, that's why not using self->type_id here
    _uw_hash_uint64(ctx, UwTypeId_DateTime);
    _uw_hash_uint64(ctx, self->year);
    _uw_hash_uint64(ctx, self->month);
    _uw_hash_uint64(ctx, self->day);
    _uw_hash_uint64(ctx, self->hour);
    _uw_hash_uint64(ctx, self->minute);
    _uw_hash_uint64(ctx, self->second);
    _uw_hash_uint64(ctx, self->nanosecond);
    _uw_hash_uint64(ctx, self->gmt_offset + (1L << 8 * sizeof(self->gmt_offset)));  // make positive (biased)
}

static void datetime_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);

    fprintf(fp, ": %04u-%02u-%02u %02u:%02u:%02u",
            self->year, self->month, self->day,
            self->hour, self->minute, self->second);

    if (self->nanosecond) {
        // format fractional part and print &frac[1] later
        char frac[12];
        snprintf(frac, sizeof(frac), "%u", self->nanosecond + 1000'000'000);
        fputs(&frac[1], fp);
    }
    if (self->gmt_offset) {
        // gmt_offset can be negative
        int offset_hours = self->gmt_offset / 60;
        int offset_minutes = self->gmt_offset % 60;
        // make sure minutes are positive
        if (offset_minutes < 0) {
            offset_minutes = -offset_minutes;
        }
        fprintf(fp, "%c%02d:%02d", (offset_hours< 0)? '-' : '+', offset_hours, offset_minutes);
    }
    fputc('\n', fp);
}

static UwResult datetime_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool datetime_is_true(UwValuePtr self)
{
    return self->year && self->month && self->day
           && self->hour && self->minute && self->second && self->nanosecond;
}

static bool datetime_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->year       == other->year &&
           self->month      == other->month &&
           self->day        == other->day &&
           self->hour       == other->hour &&
           self->minute     == other->minute &&
           self->second     == other->second &&
           self->nanosecond == other->nanosecond &&
           self->gmt_offset == other->gmt_offset;
}

static bool datetime_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_DateTime) {
            return datetime_equal_sametype(self, other);
        }
        // check base type
        t = _uw_types[t]->ancestor_id;
        if (t == UwTypeId_Null) {
            return false;
        }
    }
}

static UwType datetime_type = {
    .id             = UwTypeId_DateTime,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "DateTime",
    .create         = datetime_create,
    .destroy        = nullptr,
    .clone          = nullptr,
    .hash           = datetime_hash,
    .deepcopy       = datetime_to_string,
    .dump           = datetime_dump,
    .to_string      = datetime_to_string,
    .is_true        = datetime_is_true,
    .equal_sametype = datetime_equal_sametype,
    .equal          = datetime_equal
};

/****************************************************************
 * Timestamp type
 */

static UwResult timestamp_create(UwTypeId type_id, void* ctor_args)
{
    // XXX use ctor_args for initializer?
    return UwTimestamp();
}

static void timestamp_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: the hash should be the same for subtypes, that's why not using self->type_id here
    _uw_hash_uint64(ctx, UwTypeId_Timestamp);
    _uw_hash_uint64(ctx, self->ts_seconds);
    _uw_hash_uint64(ctx, self->ts_nanoseconds);
}

static void timestamp_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %zu", self->ts_seconds);
    if (self->ts_nanoseconds) {
        // format fractional part and print &frac[1] later
        char frac[12];
        snprintf(frac, sizeof(frac), "%u", self->ts_nanoseconds + 1000'000'000);
        fputs(&frac[1], fp);
    }
    fputc('\n', fp);
}

static UwResult timestamp_to_string(UwValuePtr self)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%zu.%09u", self->ts_seconds, self->ts_nanoseconds);
    return uw_create_string(buf);
}

static bool timestamp_is_true(UwValuePtr self)
{
    return self->ts_seconds && self->ts_nanoseconds;
}

static bool timestamp_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->ts_seconds == other->ts_seconds
        && self->ts_nanoseconds == other->ts_nanoseconds;
}

static bool timestamp_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_Timestamp) {
            return timestamp_equal_sametype(self, other);
        }
        // check base type
        t = _uw_types[t]->ancestor_id;
        if (t == UwTypeId_Null) {
            return false;
        }
    }
}

static UwType timestamp_type = {
    .id             = UwTypeId_Timestamp,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "Timestamp",
    .create         = timestamp_create,
    .destroy        = nullptr,
    .clone          = nullptr,
    .hash           = timestamp_hash,
    .deepcopy       = timestamp_to_string,
    .dump           = timestamp_dump,
    .to_string      = timestamp_to_string,
    .is_true        = timestamp_is_true,
    .equal_sametype = timestamp_equal_sametype,
    .equal          = timestamp_equal
};

/****************************************************************
 * Pointer type
 */

static UwResult ptr_create(UwTypeId type_id, void* ctor_args)
{
    // XXX use ctor_args for initializer?
    return UwPtr(nullptr);
}

static void ptr_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: the hash should be the same for subtypes, that's why not using self->type_id here
    _uw_hash_uint64(ctx, UwTypeId_Ptr);
    _uw_hash_buffer(ctx, &self->ptr, sizeof(self->ptr));
}

static void ptr_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fprintf(fp, ": %p\n", self->ptr);
}

static UwResult ptr_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool ptr_is_true(UwValuePtr self)
{
    return self->ptr;
}

static bool ptr_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return self->ptr == other->ptr;
}

static bool ptr_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Null:
                return self->ptr == nullptr;

            case UwTypeId_Ptr:
                return self->ptr == other->ptr;

            default: {
                // check base type
                t = _uw_types[t]->ancestor_id;
                if (t == UwTypeId_Null) {
                    return false;
                }
            }
        }
    }
}

static UwType ptr_type = {
    .id             = UwTypeId_Ptr,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "Ptr",
    .create         = ptr_create,
    .destroy        = nullptr,
    .clone          = nullptr,
    .hash           = ptr_hash,
    .deepcopy       = ptr_to_string,
    .dump           = ptr_dump,
    .to_string      = ptr_to_string,
    .is_true        = ptr_is_true,
    .equal_sametype = ptr_equal_sametype,
    .equal          = ptr_equal
};

/****************************************************************
 * Type system initialization.
 */

extern UwType _uw_status_type;    // defined in uw_status.c
extern UwType _uw_iterator_type;  // defined in uw_iterator.c
extern UwType _uw_map_type;       // defined in uw_map.c

static UwType* basic_types[] = {
    [UwTypeId_Null]      = &null_type,
    [UwTypeId_Bool]      = &bool_type,
    [UwTypeId_Int]       = &int_type,
    [UwTypeId_Signed]    = &signed_type,
    [UwTypeId_Unsigned]  = &unsigned_type,
    [UwTypeId_Float]     = &float_type,
    [UwTypeId_DateTime]  = &datetime_type,
    [UwTypeId_Timestamp] = &timestamp_type,
    [UwTypeId_Ptr]       = &ptr_type,
    [UwTypeId_CharPtr]   = &_uw_charptr_type,
    [UwTypeId_String]    = &_uw_string_type,
    [UwTypeId_Struct]    = &_uw_struct_type,
    [UwTypeId_Compound]  = &_uw_compound_type,
    [UwTypeId_Status]    = &_uw_status_type,
    [UwTypeId_Iterator]  = &_uw_iterator_type,
    [UwTypeId_Array]     = &_uw_array_type,
    [UwTypeId_Map]       = &_uw_map_type
};

[[ gnu::constructor ]]
void _uw_init_types()
{
    _uw_init_interfaces();

    if (_uw_types) {
        return;
    }

    num_uw_types = UW_LENGTH(basic_types);
    _uw_types = mmarray_allocate(num_uw_types, sizeof(UwType*));

    for(UwTypeId i = 0; i < num_uw_types; i++) {
        UwType* t = basic_types[i];
        if (!t) {
            uw_panic("Type %u is not defined\n", i);
        }
        _uw_types[i] = t;
    }
}

static UwTypeId add_type(UwType* type)
{
    if (num_uw_types == ((1 << 8 * sizeof(UwTypeId)) - 1)) {
        uw_panic("Cannot define more types than %u\n", num_uw_types);
    }
    _uw_types = mmarray_append_item(_uw_types, &type);
    UwTypeId type_id = num_uw_types++;
    type->id = type_id;
    return type_id;
}

UwTypeId _uw_add_type(UwType* type, ...)
{
    // the order constructor are called is undefined, make sure the type system is initialized
    _uw_init_types();

    // add type

    UwTypeId type_id = add_type(type);

    va_list ap;
    va_start(ap);
    _uw_create_interfaces(type, ap);
    va_end(ap);

    return type_id;
}

UwTypeId _uw_subtype(UwType* type, char* name, UwTypeId ancestor_id,
                     unsigned data_size, unsigned alignment, ...)
{
    // the order constructor are called is undefined, make sure the type system is initialized
    _uw_init_types();

    uw_assert(ancestor_id != UwTypeId_Null);

    UwType* ancestor = _uw_types[ancestor_id];

    *type = *ancestor;  // copy type structure
    type->init = nullptr;
    type->fini = nullptr;

    type->ancestor_id = ancestor_id;
    type->name = name;
    type->data_offset = align_unsigned(ancestor->data_offset + ancestor->data_size, alignment);
    type->data_size = data_size;

    UwTypeId type_id = add_type(type);

    va_list ap;
    va_start(ap);
    _uw_update_interfaces(type, ancestor, ap);
    va_end(ap);

    return type_id;
}

void uw_dump_types(FILE* fp)
{
    fputs( "=== UW types ===\n", fp);
    for (UwTypeId i = 0; i < num_uw_types; i++) {
        UwType* t = _uw_types[i];
        fprintf(fp, "%u: %s; ancestor=%u (%s)\n",
                t->id, t->name, t->ancestor_id, _uw_types[t->ancestor_id]->name);
        if (t->num_interfaces) {
            _UwInterface* iface = t->interfaces;
            for (unsigned j = 0; j < t->num_interfaces; j++, iface++) {
                unsigned num_methods = _uw_get_num_interface_methods(iface->interface_id);
                void** methods = iface->interface_methods;
                fprintf(fp, "    interface %u (%s):\n        ",
                        iface->interface_id, uw_get_interface_name(iface->interface_id));
                for (unsigned k = 0; k < num_methods; k++, methods++) {
                    fprintf(fp, "%p ", *methods);
                }
                fputc('\n', fp);
            }
        }
    }
}
