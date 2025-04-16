#pragma once

#include <uw_iterators.h>

#ifdef __cplusplus
extern "C" {
#endif

extern UwTypeId UwTypeId_StringIO;

/****************************************************************
 * Constructors
 */

typedef struct {
    UwValuePtr string;
} UwStringIOCtorArgs;

#define uw_create_string_io(str) _Generic((str), \
             char*: _uw_create_string_io_u8_wrapper,  \
          char8_t*: _uw_create_string_io_u8,          \
         char32_t*: _uw_create_string_io_u32,         \
        UwValuePtr: _uw_create_string_io              \
    )((str))

UwResult _uw_create_string_io_u8  (char8_t* str);
UwResult _uw_create_string_io_u32 (char32_t* str);
UwResult _uw_create_string_io     (UwValuePtr str);

static inline UwResult _uw_create_string_io_u8_wrapper(char* str)
{
    return _uw_create_string_io_u8((char8_t*) str);
}

/****************************************************************
 * Interface shorthand methods
 */

static inline UwResult uw_start_read_lines (UwValuePtr reader) { return uw_interface(reader->type_id, LineReader)->start(reader); }
static inline UwResult uw_read_line        (UwValuePtr reader) { return uw_interface(reader->type_id, LineReader)->read_line(reader); }
static inline UwResult uw_read_line_inplace(UwValuePtr reader, UwValuePtr line) { return uw_interface(reader->type_id, LineReader)->read_line_inplace(reader, line); }
static inline bool     uw_unread_line      (UwValuePtr reader, UwValuePtr line) { return uw_interface(reader->type_id, LineReader)->unread_line(reader, line); }
static inline unsigned uw_get_line_number  (UwValuePtr reader) { return uw_interface(reader->type_id, LineReader)->get_line_number(reader); }
static inline void     uw_stop_read_lines  (UwValuePtr reader) { uw_interface(reader->type_id, LineReader)->stop(reader); }

#ifdef __cplusplus
}
#endif
