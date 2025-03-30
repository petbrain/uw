#ifndef _GNU_SOURCE
//  for vasprintf
#   define _GNU_SOURCE
#endif

#include <string.h>

#include <libpussy/mmarray.h>

#include "include/uw_base.h"
#include "include/uw_string.h"
#include "uw_struct_internal.h"

typedef struct {
    _UwValue description;  // string
} _UwStatusData;

#define get_data_ptr(value)  ((_UwStatusData*) _uw_get_data_ptr((value), UwTypeId_Status))

static char* basic_statuses[] = {
    [UW_SUCCESS]                   = "SUCCESS",
    [UW_STATUS_VA_END]             = "VA_END",
    [UW_ERROR_ERRNO]               = "ERRNO",
    [UW_ERROR_OOM]                 = "OOM",
    [UW_ERROR_NOT_IMPLEMENTED]     = "NOT IMPLEMENTED",
    [UW_ERROR_INCOMPATIBLE_TYPE]   = "INCOMPATIBLE_TYPE",
    [UW_ERROR_EOF]                 = "EOF",
    [UW_ERROR_INDEX_OUT_OF_RANGE]  = "UW_ERROR_INDEX_OUT_OF_RANGE",
    [UW_ERROR_POP_FROM_EMPTY_LIST] = "POP_FROM_EMPTY_LIST",
    [UW_ERROR_KEY_NOT_FOUND]       = "KEY_NOT_FOUND",
    [UW_ERROR_FILE_ALREADY_OPENED] = "FILE_ALREADY_OPENED",
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
    num_statuses = UW_LENGTH_OF(basic_statuses);
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

UwResult uw_status_desc(UwValuePtr status)
{
    if (!status) {
        return UwString_1_12(6, '(', 'n', 'u', 'l', 'l', ')', 0, 0, 0, 0, 0, 0);
    }
    if (!uw_is_status(status)) {
        return UwString_1_12(10, 'b', 'a', 'd', ' ', 's', 't', 'a', 't', 'u', 's', 0, 0);
    }
    _UwStatusData* status_data = get_data_ptr(status);
    if (status_data) {
        if (uw_is_string(&status_data->description)) {
            return uw_clone(&status_data->description);
        }
    }
    return UwString_1_12(6, '(', 'n', 'o', 'n', 'e', ')', 0, 0, 0, 0, 0, 0);
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

    _UwStatusData* status_data = get_data_ptr(status);
    if (status_data) {
        uw_destroy(&status_data->description);
    } else {
        UwValue err = _uw_struct_alloc(status, nullptr);
        if (uw_error(&err)) {
            return;
        }
        status_data = get_data_ptr(status);
    }
    char* desc;
    if (vasprintf(&desc, fmt, ap) == -1) {
        return;
    }
    status_data->description = uw_create_string(desc);
    if (uw_error(&status_data->description)) {
        uw_destroy(&status_data->description);
    }
    free(desc);
}

/****************************************************************
 * Basic interface methods
 */

static UwResult status_create(UwTypeId type_id, void* ctor_args)
{
    // XXX use ctor_args for initializer?

    // using not autocleaned variable here, no uw_move necessary on exit
    __UWDECL_Status(result, UW_SUCCESS);
    // do not allocate status data
    return result;
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
    if (!result.struct_data) {
        return uw_move(&result);
    }
    result.struct_data = nullptr;

    UwValue err = _uw_struct_alloc(&result, nullptr);
    if (uw_error(&err)) {
        return uw_move(&err);
    }

    _UwStatusData* status_data = get_data_ptr(self);
    uw_destroy(&status_data->description);
    status_data->description = uw_deepcopy(&status_data->description);
    return uw_move(&result);
}

static void status_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    if (self->struct_data) {
        _uw_dump_struct_data(fp, self);
    }
    if (self->status_code == UW_ERROR_ERRNO) {
        fprintf(fp, "\nerrno %d: %s\n", self->uw_errno, strerror(self->uw_errno));
    } else {
        UwValue desc = uw_status_desc(self);
        UW_CSTRING_LOCAL(cdesc, &desc);
        fprintf(fp, "\n%s (%u): %s\n", uw_status_str(self->status_code), self->status_code, cdesc);
        if (self->struct_data) {
            _uw_dump_struct_data(fp, self);
        }
    }
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
        } else {
            // check base type
            t = _uw_types[t]->ancestor_id;
            if (t == UwTypeId_Null) {
                return false;
            }
        }
    }
}

static void status_fini(UwValuePtr self)
{
    _UwStatusData* status_data = get_data_ptr(self);
    if (status_data) {
        uw_destroy(&status_data->description);

        // do not call Struct.fini because it's a no op
    }
}

UwType _uw_status_type = {
    .id             = UwTypeId_Status,
    .ancestor_id    = UwTypeId_Struct,
    .name           = "Status",
    .allocator      = &default_allocator,
    .create         = status_create,
    .destroy        = _uw_struct_destroy,
    .clone          = _uw_struct_clone,
    .hash           = status_hash,
    .deepcopy       = status_deepcopy,
    .dump           = status_dump,
    .to_string      = _uw_struct_to_string,
    .is_true        = _uw_struct_is_true,
    .equal_sametype = status_equal_sametype,
    .equal          = status_equal,

    .data_offset    = sizeof(_UwStructData),
    .data_size      = sizeof(_UwStatusData),

    .init           = _uw_struct_init,
    .fini           = status_fini
};

// make sure _UwStructData has correct padding
static_assert((sizeof(_UwStructData) & (alignof(_UwStatusData) - 1)) == 0);
