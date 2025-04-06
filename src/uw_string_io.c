#include "include/uw.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_string_internal.h"
#include "src/uw_struct_internal.h"

typedef struct {
    // line reader data
    _UwValue line;
    _UwValue pushback;
    unsigned line_number;
    unsigned line_position;
} _UwStringIO;

#define get_data_ptr(value)  ((_UwStringIO*) _uw_get_data_ptr((value), UwTypeId_StringIO))

// forward declaration
static UwResult read_line_inplace(UwValuePtr self, UwValuePtr line);

/****************************************************************
 * Constructors
 */

UwResult _uw_create_string_io_cstr(char* str)
{
    __UWDECL_CharPtr(v, str);
    UwStringIOCtorArgs args = { .string = &v };
    return uw_create2(UwTypeId_StringIO, &args);
}

UwResult _uw_create_string_io_u8(char8_t* str)
{
    __UWDECL_Char8Ptr(v, str);
    UwStringIOCtorArgs args = { .string = &v };
    return uw_create2(UwTypeId_StringIO, &args);
}

UwResult _uw_create_string_io_u32(char32_t* str)
{
    __UWDECL_Char32Ptr(v, str);
    UwStringIOCtorArgs args = { .string = &v };
    return uw_create2(UwTypeId_StringIO, &args);
}

UwResult _uw_create_string_io(UwValuePtr str)
{
    UwStringIOCtorArgs args = { .string = str };
    return uw_create2(UwTypeId_StringIO, &args);
}

/****************************************************************
 * Basic interface methods
 */

static UwResult stringio_init(UwValuePtr self, void* ctor_args)
{
    UwStringIOCtorArgs* args = ctor_args;

    UwValue str = UwNull();
    if (args) {
        str = uw_clone(args->string);  // this converts CharPtr to string
    } else {
        str = UwString();
    }
    if (!uw_is_string(&str)) {
        return UwError(UW_ERROR_INCOMPATIBLE_TYPE);
    }
    _UwStringIO* sio = get_data_ptr(self);
    sio->line = uw_move(&str);
    sio->pushback = UwNull();
    return UwOK();
}

static void stringio_fini(UwValuePtr self)
{
    _UwStringIO* sio = get_data_ptr(self);
    uw_destroy(&sio->line);
    uw_destroy(&sio->pushback);
}

static UwResult stringio_deepcopy(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static void stringio_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);

    _UwStringIO* sio = get_data_ptr(self);
    unsigned length = _uw_string_length(&sio->line);
    if (length) {
        get_str_methods(&sio->line)->hash(_uw_string_char_ptr(&sio->line, 0), length, ctx);
    }
}

static void stringio_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _UwStringIO* sio = get_data_ptr(self);

    _uw_dump_start(fp, self, first_indent);
    _uw_dump_struct_data(fp, self);
    _uw_string_dump_data(fp, &sio->line, next_indent);

    _uw_print_indent(fp, next_indent);
    fprintf(fp, "Current position: %u\n", sio->line_position);
    if (uw_is_null(&sio->pushback)) {
        fputs("Pushback: none\n", fp);
    }
    else if (!uw_is_string(&sio->pushback)) {
        fputs("WARNING: bad pushback:\n", fp);
        uw_dump(fp, &sio->pushback);
    } else {
        fputs("Pushback:\n", fp);
        _uw_string_dump_data(fp, &sio->pushback, next_indent);
    }
}

static UwResult stringio_to_string(UwValuePtr self)
{
    _UwStringIO* sio = get_data_ptr(self);
    return uw_clone(&sio->line);
}

static bool stringio_is_true(UwValuePtr self)
{
    _UwStringIO* sio = get_data_ptr(self);
    return uw_is_true(&sio->line);
}

static bool stringio_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    _UwStringIO* sio_self  = get_data_ptr(self);
    _UwStringIO* sio_other = get_data_ptr(other);

    UwMethodEqual fn_cmp = uw_typeof(&sio_self->line)->equal_sametype;
    return fn_cmp(&sio_self->line, &sio_other->line);
}

static bool stringio_equal(UwValuePtr self, UwValuePtr other)
{
    _UwStringIO* sio_self  = get_data_ptr(self);

    UwMethodEqual fn_cmp = uw_typeof(&sio_self->line)->equal;
    return fn_cmp(&sio_self->line, other);
}

/****************************************************************
 * LineReader interface methods
 */

static UwResult start_read_lines(UwValuePtr self)
{
    _UwStringIO* sio = get_data_ptr(self);
    sio->line_position = 0;
    sio->line_number = 0;
    uw_destroy(&sio->pushback);
    return UwOK();
}

static UwResult read_line(UwValuePtr self)
{
    UwValue result = UwString();
    UwValue status = read_line_inplace(self, &result);
    uw_return_if_error(&status);
    return uw_move(&result);
}

static UwResult read_line_inplace(UwValuePtr self, UwValuePtr line)
{
    _UwStringIO* sio = get_data_ptr(self);

    uw_string_truncate(line, 0);

    if (uw_is_string(&sio->pushback)) {
        if (!uw_string_append(line, &sio->pushback)) {
            return UwOOM();
        }
        uw_destroy(&sio->pushback);
        sio->line_number++;
        return UwOK();
    }

    if (!uw_string_index_valid(&sio->line, sio->line_position)) {
        return UwError(UW_ERROR_EOF);
    }

    unsigned lf_pos;
    if (!uw_strchr(&sio->line, '\n', sio->line_position, &lf_pos)) {
        lf_pos = uw_strlen(&sio->line) - 1;
    }
    uw_string_append_substring(line, &sio->line, sio->line_position, lf_pos + 1);
    sio->line_position = lf_pos + 1;
    sio->line_number++;
    return UwOK();
}

static bool unread_line(UwValuePtr self, UwValuePtr line)
{
    _UwStringIO* sio = get_data_ptr(self);

    if (uw_is_null(&sio->pushback)) {
        sio->pushback = uw_clone(line);
        sio->line_number--;
        return true;
    } else {
        return false;
    }
}

static unsigned get_line_number(UwValuePtr self)
{
    return get_data_ptr(self)->line_number;
}

static void stop_read_lines(UwValuePtr self)
{
    _UwStringIO* sio = get_data_ptr(self);
    uw_destroy(&sio->pushback);
}

/****************************************************************
 * StringIO type and interfaces
 */

UwTypeId UwTypeId_StringIO = 0;

static UwInterface_LineReader line_reader_interface = {
    .start             = start_read_lines,
    .read_line         = read_line,
    .read_line_inplace = read_line_inplace,
    .get_line_number   = get_line_number,
    .unread_line       = unread_line,
    .stop              = stop_read_lines
};

static UwType stringio_type = {
    .id             = 0,
    .ancestor_id    = UwTypeId_Struct,
    .name           = "StringIO",
    .allocator      = &default_allocator,

    .create         = _uw_struct_create,
    .destroy        = _uw_struct_destroy,
    .clone          = _uw_struct_clone,
    .hash           = stringio_hash,
    .deepcopy       = stringio_deepcopy,
    .dump           = stringio_dump,
    .to_string      = stringio_to_string,
    .is_true        = stringio_is_true,
    .equal_sametype = stringio_equal_sametype,
    .equal          = stringio_equal,

    .data_offset    = sizeof(_UwStructData),
    .data_size      = sizeof(_UwStringIO),

    .init           = stringio_init,
    .fini           = stringio_fini
};

// make sure _UwStructData has correct padding
static_assert((sizeof(_UwStructData) & (alignof(_UwStringIO) - 1)) == 0);


[[ gnu::constructor ]]
static void init_stringio_type()
{
    // interfaces can be registered by any type in any order
    if (UwInterfaceId_LineReader == 0) { UwInterfaceId_LineReader = uw_register_interface("LineReader", UwInterface_LineReader); }

    UwTypeId_StringIO = uw_add_type(
        &stringio_type,
        UwInterfaceId_LineReader, &line_reader_interface
    );
}
