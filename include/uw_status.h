#pragma once

#include <stdio.h>

#include <uw_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UW_SUCCESS                    0
#define UW_STATUS_VA_END              1  // used as a terminator for variadic arguments
#define UW_ERROR_ERRNO                2
#define UW_ERROR_OOM                  3
#define UW_ERROR_NOT_IMPLEMENTED      4
#define UW_ERROR_INCOMPATIBLE_TYPE    5
#define UW_ERROR_EOF                  6
#define UW_ERROR_INDEX_OUT_OF_RANGE   7

// array errors
#define UW_ERROR_EXTRACT_FROM_EMPTY_ARRAY  8

// map errors
#define UW_ERROR_KEY_NOT_FOUND        9

// File errors
#define UW_ERROR_FILE_ALREADY_OPENED  10
#define UW_ERROR_NOT_REGULAR_FILE     11

// StringIO errors
#define UW_ERROR_UNREAD_FAILED        12

uint16_t uw_define_status(char* status);
/*
 * Define status in the global table.
 * Return status code or UW_ERROR_OOM
 *
 * This function should be called from the very beginning of main() function
 * or from constructors that are called before main().
 */

char* uw_status_str(uint16_t status_code);
/*
 * Get status string by status code.
 */

static inline bool uw_ok(UwValuePtr status)
{
    if (!status) {
        return false;
    }
    if (!uw_is_status(status)) {
        // any other type means okay
        return true;
    }
    return !status->is_error;
}

static inline bool uw_error(UwValuePtr status)
{
    return !uw_ok(status);
}

#define uw_return_if_error(value_ptr)  \
    do {  \
        if (uw_error(value_ptr)) {  \
            return uw_move(value_ptr);  \
        }  \
    } while (false)

static inline bool uw_eof(UwValuePtr status)
{
    if (!status) {
        return false;
    }
    if (!uw_is_status(status)) {
        return false;
    }
    return status->status_code == UW_ERROR_EOF;
}

static inline bool uw_va_end(UwValuePtr status)
{
    if (!status) {
        return false;
    }
    if (!uw_is_status(status)) {
        return false;
    }
    return status->status_code == UW_STATUS_VA_END;
}

void _uw_set_status_location(UwValuePtr status, char* file_name, unsigned line_number);
void _uw_set_status_desc(UwValuePtr status, char* fmt, ...);
void _uw_set_status_desc_ap(UwValuePtr status, char* fmt, va_list ap);
/*
 * Set description for status.
 * If out of memory assign UW_ERROR_OOM to status.
 */

void uw_print_status(FILE* fp, UwValuePtr status);


#ifdef __cplusplus
}
#endif
