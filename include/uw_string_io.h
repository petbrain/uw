#pragma once

/*
 * StringIO provides LineReader interface.
 * It's a singleton iterator for self.
 */

#include <uw.h>

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

#ifdef __cplusplus
}
#endif
