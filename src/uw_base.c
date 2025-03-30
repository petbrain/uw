#include <string.h>

#include <libpussy/allocator.h>  // for align_unsigned
#include <libpussy/arena.h>
#include <libpussy/mmarray.h>

#include "include/uw_base.h"
#include "include/uw_file.h"
#include "include/uw_string.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_hash_internal.h"
#include "src/uw_list_internal.h"
#include "src/uw_map_internal.h"
#include "src/uw_string_internal.h"

/****************************************************************
 * Type system data
 */

UwType** _uw_types = nullptr;
static UwTypeId num_uw_types = 0;

typedef struct {
    char* name;
    unsigned num_methods;
} InterfaceInfo;

static InterfaceInfo* registered_interfaces = nullptr;

static Arena* arena = nullptr;  // arena for interfaces and other(?) data

/****************************************************************
 * Basic functions
 */

[[noreturn]]
void uw_panic(char* fmt, ...)
{
    va_list ap;
    va_start(ap);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    abort();
}

[[ noreturn ]]
void _uw_panic_no_interface(UwTypeId type_id, unsigned interface_id)
{
    uw_panic("Interface %u is not defined for %s\n", interface_id, _uw_types[type_id]->name);
}

UwType_Hash uw_hash(UwValuePtr value)
{
    UwHashContext ctx;
    _uw_hash_init(&ctx);
    _uw_call_hash(value, &ctx);
    return _uw_hash_finish(&ctx);
}

/****************************************************************
 * Dump functions.
 */

void _uw_dump_start(FILE* fp, UwValuePtr value, int indent)
{
    char* type_name = uw_typeof(value)->name;

    _uw_print_indent(fp, indent);
    fprintf(fp, "%p", value);
    if (type_name == nullptr) {
        fprintf(fp, " BAD TYPE %d", value->type_id);
    } else {
        fprintf(fp, " %s (type id: %d)", type_name, value->type_id);
    }
}

void _uw_dump_struct_data(FILE* fp, UwValuePtr value)
{
    if (value->struct_data) {
        fprintf(fp, " data=%p refcount=%u;", value->struct_data, value->struct_data->refcount);
    } else {
        fprintf(fp, " data=NULL;");
    }
}

void _uw_print_indent(FILE* fp, int indent)
{
    for (int i = 0; i < indent; i++ ) {
        fputc(' ', fp);
    }
}

void _uw_dump(FILE* fp, UwValuePtr value, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    if (value == nullptr) {
        _uw_print_indent(fp, first_indent);
        fprintf(fp, "nullptr\n");
        return;
    }
    UwMethodDump fn_dump = uw_typeof(value)->dump;
    uw_assert(fn_dump != nullptr);
    fn_dump(value, fp, first_indent, next_indent, tail);
}

void uw_dump(FILE* fp, UwValuePtr value)
{
    _uw_dump(fp, value, 0, 0, nullptr);
}

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
        switch (t) {
            case UwTypeId_Bool:
                return self->bool_value == other->bool_value;

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

extern UwType _uw_struct_type;    // defined in uw_struct.c
extern UwType _uw_compound_type;  // defined in uw_compound.c
extern UwType _uw_status_type;    // defined in uw_status.c
extern UwType _uw_map_type;       // defined in uw_map.c

static UwType* basic_types[] = {
    [UwTypeId_Null]     = &null_type,
    [UwTypeId_Bool]     = &bool_type,
    [UwTypeId_Int]      = &int_type,
    [UwTypeId_Signed]   = &signed_type,
    [UwTypeId_Unsigned] = &unsigned_type,
    [UwTypeId_Float]    = &float_type,
    [UwTypeId_Ptr]      = &ptr_type,
    [UwTypeId_CharPtr]  = &_uw_charptr_type,
    [UwTypeId_String]   = &_uw_string_type,
    [UwTypeId_Struct]   = &_uw_struct_type,
    [UwTypeId_Compound] = &_uw_compound_type,
    [UwTypeId_Status]   = &_uw_status_type,
    [UwTypeId_List]     = &_uw_list_type,
    [UwTypeId_Map]      = &_uw_map_type
};

// Miscellaneous interfaces
unsigned UwInterfaceId_File = 0;
unsigned UwInterfaceId_FileReader = 0;
unsigned UwInterfaceId_FileWriter = 0;
unsigned UwInterfaceId_LineReader = 0;

[[ gnu::constructor ]]
static void init_type_system()
{
    if (_uw_types) {
        return;
    }

    num_uw_types = UW_LENGTH_OF(basic_types);
    _uw_types = mmarray_allocate(num_uw_types, sizeof(UwType*));

    for(UwTypeId i = 0; i < num_uw_types; i++) {
        UwType* t = basic_types[i];
        if (!t) {
            uw_panic("Type %u is not defined\n", i);
        }
        _uw_types[i] = t;
    }

    registered_interfaces = mmarray_allocate(0, sizeof(InterfaceInfo));

    arena = create_arena(0);
    if (!arena) {
        uw_panic("Cannot create arena\n");
    }
}

#define MAX_INTERFACES  (UINT_MAX - 1)  // UINT_MAX equals to -1 which is used as terminator
                                        // in uw_add_type and uw_subtype, that's why UINT_MAX - 1

unsigned _uw_register_interface(char* name, unsigned num_methods)
{
    init_type_system();

    unsigned n = mmarray_length(registered_interfaces);
    if (n == MAX_INTERFACES) {
        uw_panic("Cannot define more interfaces than %u\n", MAX_INTERFACES);
    }
    InterfaceInfo info = {
        .name = name,
        .num_methods = num_methods
    };
    registered_interfaces = mmarray_append_item(registered_interfaces, &info);
    return n;
}

char* uw_get_interface_name(unsigned interface_id)
{
    unsigned n = mmarray_length(registered_interfaces);
    if (interface_id >= n) {
        uw_panic("Interfaces %u is not registered yet\n", interface_id);
    }
    return registered_interfaces[interface_id].name;
}

static unsigned get_num_interface_methods(unsigned interface_id)
/*
 * Get the number of methods of interface.
 */
{
    unsigned n = mmarray_length(registered_interfaces);
    if (interface_id >= n) {
        uw_panic("Interfaces %u is not registered yet\n", interface_id);
    }
    return registered_interfaces[interface_id].num_methods;
}

static void alloc_interfaces(UwType* type)
/*
 * Allocate type->interfaces.
 * type->num_interfaces must be initialized.
 */
{
    type->interfaces = arena_alloc(arena, type->num_interfaces, _UwInterface);
    if (!type->interfaces) {
        uw_panic("%s: cannot allocate memory block for %u interfaces for type %s\n",
                 __func__, type->num_interfaces, type->name);
    }
}

static inline unsigned methods_memsize(unsigned num_methods)
/*
 * Return size of memory block in bytes to store pointers to interface methods.
 */
{
    return num_methods * (sizeof(void*) + sizeof(UwTypeId)) * 2;
}

static void** alloc_methods(unsigned num_methods)
/*
 * Allocate memory block for pointers to interface methods.
 */
{
    unsigned memsize = methods_memsize(num_methods);
    void** result = _arena_alloc(arena, memsize, alignof(void*));
    if (!result) {
        uw_panic("cannot allocate %u-bytes memory block for %u methods\n", memsize, num_methods);
    }
    return result;
}

static void** make_interface_methods(unsigned interface_id, void** methods)
/*
 * Allocate memory block and copy `methods`, checking them for nullptr.
 */
{
    unsigned num_methods = get_num_interface_methods(interface_id);
    void** interface_methods = alloc_methods(num_methods);
    bzero(interface_methods, methods_memsize(num_methods));

    for (unsigned i = 0; i < num_methods; i++) {
        void* meth = methods[i];
        if (meth) {
            interface_methods[i] = meth;
        } else {
            uw_panic("Method %u for interface %s is not defined\n",
                     i, uw_get_interface_name(interface_id));
        }
    }
    return interface_methods;
}

static void init_interfaces(UwType* type, va_list ap)
{
    // count interface argument pairs
    type->num_interfaces = 0;
    va_list temp_ap;
    va_copy(temp_ap, ap);
    for (;;) {
        unsigned interface_id = va_arg(temp_ap, unsigned);
        if (((int) interface_id) == -1) {
            break;
        }
        va_arg(temp_ap, void**);
        type->num_interfaces++;
    }
    va_end(temp_ap);

    if (type->num_interfaces == 0) {
        type->interfaces = nullptr;
        return;
    }

    // init interfaces
    alloc_interfaces(type);
    _UwInterface* iface = type->interfaces;
    for (unsigned i = 0; i < type->num_interfaces; i++, iface++) {
        iface->interface_id = va_arg(ap, unsigned);
        iface->interface_methods = make_interface_methods(iface->interface_id, va_arg(ap, void**));
    }
}

static void update_interfaces(UwType* type, UwType* ancestor, va_list ap)
{
    // count interfaces
    type->num_interfaces = ancestor->num_interfaces;
    va_list temp_ap;
    va_copy(temp_ap, ap);
    for (;;) {
        unsigned interface_id = va_arg(temp_ap, unsigned);
        if (((int) interface_id) == -1) {
            break;
        }
        va_arg(temp_ap, void**);
        if (!_uw_has_interface(ancestor->id, interface_id)) {
            type->num_interfaces++;
        }
    }
    va_end(temp_ap);

    if (type->num_interfaces == 0) {
        type->interfaces = nullptr;
        return;
    }

    // copy ancestor's interfaces
    alloc_interfaces(type);
    memcpy(type->interfaces, ancestor->interfaces, ancestor->num_interfaces * sizeof(_UwInterface));

    // update interface methods
    _UwInterface* dest_iface = &type->interfaces[ancestor->num_interfaces];
    for (;;) {
        unsigned interface_id = va_arg(ap, unsigned);
        if (((int) interface_id) == -1) {
            break;
        }
        void** src_methods = _uw_lookup_interface(ancestor->id, interface_id);
        if (src_methods) {
            // update existing interface
            void** new_methods = va_arg(ap, void**);
            unsigned num_methods = get_num_interface_methods(interface_id);
            void** interface_methods = alloc_methods(num_methods);
            memcpy(interface_methods, src_methods, methods_memsize(num_methods));

            for (unsigned i = 0; i < num_methods; i++) {
                void* meth = new_methods[i];
                if (meth) {
                    interface_methods[i] = meth;
                }
            }
        } else {
            // add new interface
            dest_iface->interface_id = interface_id;
            dest_iface->interface_methods = make_interface_methods(interface_id, va_arg(ap, void**));
            dest_iface++;
        }
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
    init_type_system();

    // add type

    UwTypeId type_id = add_type(type);

    va_list ap;
    va_start(ap);
    init_interfaces(type, ap);
    va_end(ap);

    return type_id;
}

UwTypeId _uw_subtype(UwType* type, char* name, UwTypeId ancestor_id,
                     unsigned data_size, unsigned alignment, ...)
{
    // the order constructor are called is undefined, make sure the type system is initialized
    init_type_system();

    uw_assert(ancestor_id != UwTypeId_Null);

    UwType* ancestor = _uw_types[ancestor_id];

    *type = *ancestor;  // copy type structure

    type->ancestor_id = ancestor_id;
    type->name = name;
    type->data_offset = align_unsigned(ancestor->data_offset + ancestor->data_size, alignment);
    type->data_size = data_size;

    UwTypeId type_id = add_type(type);

    va_list ap;
    va_start(ap);
    update_interfaces(type, ancestor, ap);
    va_end(ap);

    return type_id;
}
