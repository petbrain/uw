#ifndef _GNU_SOURCE
//  for vasprintf
#   define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>

#include <libpussy/mmarray.h>

#include "include/uw.h"
#include "src/uw_struct_internal.h"

static char* basic_statuses[] = {
    [UW_SUCCESS]                   = "SUCCESS",
    [UW_STATUS_VA_END]             = "VA_END",
    [UW_ERROR_ERRNO]               = "ERRNO",
    [UW_ERROR_OOM]                 = "OOM",
    [UW_ERROR_NOT_IMPLEMENTED]     = "NOT IMPLEMENTED",
    [UW_ERROR_INCOMPATIBLE_TYPE]   = "INCOMPATIBLE_TYPE",
    [UW_ERROR_EOF]                 = "EOF",
    [UW_ERROR_INDEX_OUT_OF_RANGE]  = "INDEX_OUT_OF_RANGE",
    [UW_ERROR_EXTRACT_FROM_EMPTY_ARRAY] = "EXTRACT_FROM_EMPTY_ARRAY",
    [UW_ERROR_KEY_NOT_FOUND]       = "KEY_NOT_FOUND",
    [UW_ERROR_FILE_ALREADY_OPENED] = "FILE_ALREADY_OPENED",
    [UW_ERROR_NOT_REGULAR_FILE]    = "NOT_REGULAR_FILE",
    [UW_ERROR_UNREAD_FAILED]       = "UNREAD_FAILED"
};

static char** statuses = nullptr;
static uint16_t num_statuses = 0;

[[ gnu::constructor ]]
static void init_statuses()
{
    if (statuses) {
        return;
    }
    num_statuses = UW_LENGTH(basic_statuses);
    statuses = mmarray_allocate(num_statuses, sizeof(char*));
    for(uint16_t i = 0; i < num_statuses; i++) {
        char* status = basic_statuses[i];
        if (!status) {
            fprintf(stderr, "Status %u is not defined\n", i);
            abort();
        }
        statuses[i] = status;
    }
}

uint16_t uw_define_status(char* status)
{
    // the order constructor are called is undefined, make sure statuses are initialized
    init_statuses();

    if (num_statuses == 65535) {
        fprintf(stderr, "Cannot define more statuses than %u\n", num_statuses);
        return UW_ERROR_OOM;
    }
    statuses = mmarray_append_item(statuses, &status);
    uint16_t status_code = num_statuses++;
    return status_code;
}

char* uw_status_str(uint16_t status_code)
{
    if (status_code < num_statuses) {
        return statuses[status_code];
    } else {
        static char unknown[] = "(unknown)";
        return unknown;
    }
}

void _uw_set_status_location(UwValuePtr status, char* file_name, unsigned line_number)
{
    if (status->has_status_data) {
        status->status_data->file_name = file_name;
        status->status_data->line_number = line_number;
    } else {
        status->file_name = file_name;
        status->line_number = line_number;
    }
}

void _uw_set_status_desc(UwValuePtr status, char* fmt, ...)
{
    va_list ap;
    va_start(ap);
    _uw_set_status_desc_ap(status, fmt, ap);
    va_end(ap);
}

void _uw_set_status_desc_ap(UwValuePtr status, char* fmt, va_list ap)
{
    uw_assert_status(status);

    if (status->has_status_data) {
        uw_destroy(&status->status_data->description);
    } else {
        char* file_name = status->file_name;  // file_name will be overwritten with status_data, save it
        UwValue err = _uw_struct_alloc(status, nullptr);
        if (uw_error(&err)) {
            return;
        }
        status->has_status_data = 1;
        status->status_data->file_name = file_name;
        status->status_data->line_number = status->line_number;
    }
    char* desc;
    if (vasprintf(&desc, fmt, ap) == -1) {
        return;
    }
    UwValue s = uw_create_string(desc);
    if (uw_ok(&s)) {
        status->status_data->description = uw_move(&s);
    }
    free(desc);
}

void uw_print_status(FILE* fp, UwValuePtr status)
{
    UwValue desc = uw_typeof(status)->to_string(status);
    UW_CSTRING_LOCAL(desc_cstr, &desc);
    fputs(desc_cstr, fp);
    fputc('\n', fp);
}

/****************************************************************
 * Basic interface methods
 */

static UwResult status_create(UwTypeId type_id, void* ctor_args)
{
    // XXX use ctor_args for initializer?

    UwValue result = UwOK();
    result.type_id = type_id;
    return uw_move(&result);
}

static void status_destroy(UwValuePtr self)
{
    if (self->has_status_data) {
        _uw_struct_destroy(self);
    } else {
        self->type_id = UwTypeId_Null;
    }
}

static UwResult status_clone(UwValuePtr self)
{
    if (self->has_status_data) {
        // call super method
        return _uw_types[UwTypeId_Struct]->clone(self);
    } else {
        return *self;
    }
}

static void status_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    _uw_hash_uint64(ctx, self->status_code);
    if (self->status_code == UW_ERROR_ERRNO) {
        _uw_hash_uint64(ctx, self->uw_errno);
    }
    // XXX do not hash description?
}

static UwResult status_deepcopy(UwValuePtr self)
{
    UwValue result = *self;
    if (!self->has_status_data) {
        return uw_move(&result);
    }
    result.status_data = nullptr;

    UwValue err = _uw_struct_alloc(&result, nullptr);
    uw_return_if_error(&err);

    result.status_data->file_name = self->status_data->file_name;
    result.status_data->line_number = self->status_data->line_number;
    result.status_data->description = uw_deepcopy(&self->status_data->description);
    return uw_move(&result);
}

static UwResult status_to_string(UwValuePtr status)
{
    if (!status) {
        return UwString_1_12(6, '(', 'n', 'u', 'l', 'l', ')', 0, 0, 0, 0, 0, 0);
    }
    if (!uw_is_status(status)) {
        return UwString_1_12(12, '(', 'n', 'o', 't', ' ', 's', 't', 'a', 't', 'u', 's', ')');
    }
    if (!status->is_error) {
        return UwString_1_12(2, 'O', 'K', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    char* status_str = uw_status_str(status->status_code);
    char* file_name;
    unsigned line_number;
    unsigned description_length = 0;
    uint8_t char_size = 1;

    if (status->has_status_data) {
        file_name = status->status_data->file_name;
        line_number = status->status_data->line_number;
        UwValuePtr desc = &status->status_data->description;
        if (uw_is_string(desc)) {
            description_length = uw_strlen(desc);
            char_size = uw_string_char_size(desc);
        }
    } else {
        file_name = status->file_name;
        line_number = status->line_number;
    }

    unsigned errno_length = 0;
    static char errno_fmt[] = "; errno %d: %s";
    char* errno_str = "";
    if (status->status_code == UW_ERROR_ERRNO) {
        errno_str = strerror(status->uw_errno);
    }
    char errno_desc[strlen(errno_fmt) + 16 + strlen(errno_str)];
    if (status->status_code == UW_ERROR_ERRNO) {
        errno_length = snprintf(errno_desc, sizeof(errno_desc), errno_fmt, status->uw_errno, errno_str);
    } else {
        errno_desc[0] = 0;
    }

    static char fmt[] = "%s; %s:%u%s";
    char desc[sizeof(fmt) + 16 + strlen(status_str) + strlen(file_name) + errno_length];
    unsigned length = snprintf(desc, sizeof(desc), fmt, status_str, file_name, line_number, errno_desc);

    UwValue result = uw_create_empty_string(length + description_length + 2, char_size);
    if (uw_error(&result)) {
        goto error;
    }
    uw_string_append(&result, desc);
    if (description_length) {
        uw_string_append(&result, "; ");
        uw_string_append(&result, &status->status_data->description);
    }
    return uw_move(&result);

error:
    return UwString_1_12(7, '(', 'e', 'r', 'r', 'o', 'r', ')', 0, 0, 0, 0, 0);
}

static void status_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);

    if (self->has_status_data) {
        _uw_dump_struct_data(fp, self);
    }
    UwValue desc = status_to_string(self);
    UW_CSTRING_LOCAL(cdesc, &desc);
    fputc(' ', fp);
    fputs(cdesc, fp);
    fputc('\n', fp);
}

static bool status_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    if (self->status_code == UW_ERROR_ERRNO) {
        return other->status_code == UW_ERROR_ERRNO && self->uw_errno == other->uw_errno;
    }
    else {
        return self->status_code == other->status_code;
    }
}

static bool status_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_Status) {
            return status_equal_sametype(self, other);
        }
        // check base type
        t = _uw_types[t]->ancestor_id;
        if (t == UwTypeId_Null) {
            return false;
        }
    }
}

static void status_fini(UwValuePtr self)
{
    if (self->has_status_data) {
        uw_destroy(&self->status_data->description);
    }
}

UwType _uw_status_type = {
    .id             = UwTypeId_Status,
    .ancestor_id    = UwTypeId_Struct,
    .name           = "Status",
    .allocator      = &default_allocator,
    .create         = status_create,
    .destroy        = status_destroy,
    .clone          = status_clone,
    .hash           = status_hash,
    .deepcopy       = status_deepcopy,
    .dump           = status_dump,
    .to_string      = status_to_string,
    .is_true        = _uw_struct_is_true,
    .equal_sametype = status_equal_sametype,
    .equal          = status_equal,

    .data_offset    = sizeof(_UwStructData),
    .data_size      = sizeof(_UwStatusData),

    .fini           = status_fini
};

// make sure _UwStructData has correct padding
static_assert((sizeof(_UwStructData) & (alignof(_UwStatusData) - 1)) == 0);
