#pragma once

#include <ctype.h>

#ifdef UW_WITH_ICU
    // ICU library for character classification:
#   include <unicode/uchar.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline uint8_t _uw_string_char_size(UwValuePtr s)
/*
 * char_size is stored as 0-based whereas all
 * functions use 1-based values.
 */
{
    return s->str_char_size + 1;
}

static inline uint8_t uw_string_char_size(UwValuePtr str)
{
    uw_assert_string(str);
    return _uw_string_char_size(str);
}

static inline uint8_t* _uw_string_start(UwValuePtr str)
/*
 * Return pointer to the start of internal string data.
 */
{
    if (str->str_embedded) {
        return str->str_1;
    } else {
        return str->string_data->data;
    }
}

static inline uint8_t* _uw_string_start_end(UwValuePtr str, uint8_t** end)
/*
 * Return pointer to the start of internal string data
 * and write final pointer to `end`.
 */
{
    if (str->str_embedded) {
        *end = &str->str_1[str->str_embedded_length * _uw_string_char_size(str)];
        return str->str_1;
    } else {
        _UwStringData* sdata = str->string_data;
        *end = &sdata->data[str->str_length * _uw_string_char_size(str)];
        return sdata->data;
    }
}

static inline uint8_t* _uw_string_start_length(UwValuePtr str, unsigned* length)
/*
 * Return pointer to the start of internal string data
 * and write length of string to `length`.
 */
{
   if (str->str_embedded) {
        *length = str->str_embedded_length;
        return str->str_1;
    } else {
        *length = str->str_length;
        return str->string_data->data;
    }
}

static inline uint8_t* _uw_string_char_ptr(UwValuePtr str, unsigned position)
/*
 * Return address of character in string at `position`.
 * If position is beyond end of line return nullptr.
 */
{
    unsigned offset = position * _uw_string_char_size(str);
    if (str->str_embedded) {
        if (position < str->str_embedded_length) {
            return &str->str_1[offset];
        }
    } else {
        if (position < str->str_length) {
            return &str->string_data->data[offset];
        }
    }
    return nullptr;
}

#define UW_CHAR_METHODS_IMPL(type_name)  \
    static inline char32_t _uw_get_char_##type_name(uint8_t* p)  \
    {  \
        return *((type_name*) p); \
    }  \
    static void _uw_put_char_##type_name(uint8_t* p, char32_t c)  \
    {  \
        *((type_name*) p) = (type_name) c; \
    }

UW_CHAR_METHODS_IMPL(uint8_t)
UW_CHAR_METHODS_IMPL(uint16_t)
UW_CHAR_METHODS_IMPL(uint32_t)

static inline char32_t _uw_get_char_uint24_t(uint8_t* p)
{
    // always little endian
    char32_t c = p[0] | (p[1] << 8) | (p[2] << 16);
    return c;
}

static inline void _uw_put_char_uint24_t(uint8_t* p, char32_t c)
{
    // always little endian
    p[0] = (uint8_t) c; c >>= 8;
    p[1] = (uint8_t) c; c >>= 8;
    p[2] = (uint8_t) c;
}

static inline char32_t _uw_get_char(uint8_t* ptr, uint8_t char_size)
{
    switch (char_size) {
        case 1: return _uw_get_char_uint8_t(ptr);
        case 2: return _uw_get_char_uint16_t(ptr);
        case 3: return _uw_get_char_uint24_t(ptr);
        case 4: return _uw_get_char_uint32_t(ptr);
        default: _uw_panic_bad_char_size(char_size);
    }
}

static inline void _uw_put_char(uint8_t* ptr, char32_t chr, uint8_t char_size)
{
    switch (char_size) {
        case 1: _uw_put_char_uint8_t(ptr, chr); return;
        case 2: _uw_put_char_uint16_t(ptr, chr); return;
        case 3: _uw_put_char_uint24_t(ptr, chr); return;
        case 4: _uw_put_char_uint32_t(ptr, chr); return;
        default: _uw_panic_bad_char_size(char_size);
    }
}

static inline char32_t _uw_char_at(UwValuePtr str, unsigned position)
/*
 * Return character at `position`.
 * If position is beyond end of line return 0.
 */
{
    uint8_t char_size = _uw_string_char_size(str);
    unsigned offset = position * char_size;
    if (str->str_embedded) {
        if (position < str->str_embedded_length) {
            return _uw_get_char(&str->str_1[offset], char_size);
        }
    } else {
        if (position < str->str_length) {
            return _uw_get_char(&str->string_data->data[offset], char_size);
        }
    }
    return 0;
}

static inline char32_t uw_char_at(UwValuePtr str, unsigned position)
{
    uw_assert_string(str);
    return _uw_char_at(str, position);
}

// check if `index` is within string length
#define uw_string_index_valid(str, index) ((index) < uw_strlen(str))

bool _uw_equal_u8 (UwValuePtr a, char8_t* b);
bool _uw_equal_u32(UwValuePtr a, char32_t* b);

unsigned uw_strlen(UwValuePtr str);
/*
 * Return length of string.
 */

CStringPtr uw_string_to_utf8(UwValuePtr str);
/*
 * Create C string.
 */

void uw_string_to_utf8_buf(UwValuePtr str, char* buffer);
void uw_substr_to_utf8_buf(UwValuePtr str, unsigned start_pos, unsigned end_pos, char* buffer);
/*
 * Copy string to buffer, appending terminating 0.
 * Use carefully. The caller is responsible to allocate the buffer.
 * Encode multibyte chars to UTF-8.
 */

UwResult uw_substr(UwValuePtr str, unsigned start_pos, unsigned end_pos);
/*
 * Get substring from `start_pos` to `end_pos`.
 */

bool uw_string_erase(UwValuePtr str, unsigned start_pos, unsigned end_pos);
/*
 * Erase characters from `start_pos` to `end_pos`.
 * This may make a copy of string, so checking return value is mandatory.
 */

bool uw_string_truncate(UwValuePtr str, unsigned position);
/*
 * Truncate string at given `position`.
 * This may make a copy of string, so checking return value is mandatory.
 */

bool uw_strchr(UwValuePtr str, char32_t chr, unsigned start_pos, unsigned* result);
/*
 * Find first occurence of `chr` in `str` starting from `start_pos`.
 *
 * Return true if character is found and write its position to `result`.
 * `result` can be nullptr if position is not needed and the function
 * is called just to check if `chr` is in `str`.
 */

bool uw_string_ltrim(UwValuePtr str);
bool uw_string_rtrim(UwValuePtr str);
bool uw_string_trim(UwValuePtr str);

bool uw_string_lower(UwValuePtr str);
bool uw_string_upper(UwValuePtr str);

unsigned uw_strlen_in_utf8(UwValuePtr str);
/*
 * Return length of str as if was encoded in UTF-8.
 * `str` can be either String or CharPtr.
 */

char* uw_char32_to_utf8(char32_t codepoint, char* buffer);
/*
 * Write up to 4 characters to buffer.
 * Return pointer to the next position in buffer.
 */

void _uw_putchar32_utf8(FILE* fp, char32_t codepoint);

unsigned utf8_strlen(char8_t* str);
/*
 * Count the number of codepoints in UTF8-encoded string.
 */

unsigned utf8_strlen2(char8_t* str, uint8_t* char_size);
/*
 * Count the number of codepoints in UTF8-encoded string
 * and find max char size.
 */

unsigned utf8_strlen2_buf(char8_t* buffer, unsigned* size, uint8_t* char_size);
/*
 * Count the number of codepoints in the buffer and find max char size.
 *
 * Null characters are allowed! They are counted as zero codepoints.
 *
 * Return the number of codepoints.
 * Write the number of processed bytes back to `size`.
 * This number can be less than original `size` if buffer ends with
 * incomplete sequence.
 */

char8_t* utf8_skip(char8_t* str, unsigned n);
/*
 * Skip `n` characters, return pointer to `n`th char.
 */

unsigned u32_strlen(char32_t* str);
/*
 * Find length of null-terminated `str`.
 */

unsigned u32_strlen2(char32_t* str, uint8_t* char_size);
/*
 * Find both length of null-terminated `str` and max char size in one go.
 */

int u32_strcmp   (char32_t* a, char32_t* b);
int u32_strcmp_u8(char32_t* a, char8_t*  b);
/*
 * Compare  null-terminated strings.
 */

char32_t* u32_strchr(char32_t* str, char32_t chr);
/*
 * Find the first occurrence of `chr` in the null-terminated `str`.
 */

uint8_t u32_char_size(char32_t* str, unsigned max_len);
/*
 * Find the maximal size of character in `str`, up to `max_len` or null terminator.
 */

#ifdef UW_WITH_ICU
#   define uw_char_lower(c)  u_tolower(c)
#   define uw_char_upper(c)  u_toupper(c)
#else
#   define uw_char_lower(c)  tolower(c)
#   define uw_char_upper(c)  toupper(c)
#endif

/*
 * uw_strcat generic macro allows passing arguments
 * either by value or by reference.
 * When passed by value, the function destroys them
 * .
 * CAVEAT: DO NOT PASS LOCAL VARIABLES BY VALUES!
 */
#define uw_strcat(str, ...) _Generic((str),  \
        _UwValue:   _uw_strcat_va_v,  \
        UwValuePtr: _uw_strcat_va_p   \
    )((str) __VA_OPT__(,) __VA_ARGS__,\
        _Generic((str),  \
            _UwValue:   UwVaEnd(),  \
            UwValuePtr: nullptr     \
        ))

UwResult _uw_strcat_va_v(...);
UwResult _uw_strcat_va_p(...);

UwResult _uw_strcat_ap_v(va_list ap);
UwResult _uw_strcat_ap_p(va_list ap);

unsigned uw_string_skip_spaces(UwValuePtr str, unsigned position);
/*
 * Find position of the first non-space character starting from `position`.
 * If non-space character is not found, the length is returned.
 */

unsigned uw_string_skip_chars(UwValuePtr str, unsigned position, char32_t* skipchars);
/*
 * Find position of the first character not belonging to `skipchars` starting from `position`.
 * If no `skipchars` encountered, the length is returned.
 */

/****************************************************************
 * Character classification functions
 */

#ifdef UW_WITH_ICU
#   define uw_isspace(c)  u_isspace(c)
#else
#   define uw_isspace(c)  isspace(c)
#endif
/*
 * Return true if `c` is a whitespace character.
 */

#define uw_isdigit(c)  isdigit(c)
/*
 * Return true if `c` is an ASCII digit.
 * Do not consider any other unicode digits because this function
 * is used in conjunction with standard C library (string to number conversion)
 * that does not support unicode character classification.
 */

bool uw_string_isdigit(UwValuePtr str);
/*
 * Return true if `str` is not empty and contains all digits.
 */

/****************************************************************
 * Constructors
 */

#define uw_create_string(initializer) _Generic((initializer),   \
                 char*: _uw_create_string_u8_wrapper, \
              char8_t*: _uw_create_string_u8,         \
             char32_t*: _uw_create_string_u32,        \
            UwValuePtr: _uw_create_string             \
    )((initializer))

UwResult _uw_create_string_u8 (char8_t*   initializer);
UwResult _uw_create_string_u32(char32_t*  initializer);
UwResult _uw_create_string    (UwValuePtr initializer);

static inline UwResult _uw_create_string_u8_wrapper(char* initializer)
{
    return _uw_create_string_u8((char8_t*) initializer);
}

UwResult uw_create_empty_string(unsigned capacity, uint8_t char_size);

/****************************************************************
 * Append functions
 */

#define uw_string_append(dest, src) _Generic((src),   \
              char32_t: _uw_string_append_c32,        \
                   int: _uw_string_append_c32,        \
                 char*: _uw_string_append_u8_wrapper, \
              char8_t*: _uw_string_append_u8,         \
             char32_t*: _uw_string_append_u32,        \
            UwValuePtr: _uw_string_append             \
    )((dest), (src))

bool _uw_string_append_c32(UwValuePtr dest, char32_t   c);
bool _uw_string_append_u8 (UwValuePtr dest, char8_t*   src);
bool _uw_string_append_u32(UwValuePtr dest, char32_t*  src);
bool _uw_string_append    (UwValuePtr dest, UwValuePtr src);

static inline bool _uw_string_append_u8_wrapper(UwValuePtr dest, char* src)
{
    return _uw_string_append_u8(dest, (char8_t*) src);
}

bool uw_string_append_utf8(UwValuePtr dest, char8_t* buffer, unsigned size, unsigned* bytes_processed);
/*
 * Append UTF-8-encoded characters from `buffer`.
 * Write the number of bytes processed to `bytes_processed`, which can be less
 * than `size` if buffer ends with incomplete UTF-8 sequence.
 *
 * Return false if out of memory.
 */

bool uw_string_append_buffer(UwValuePtr dest, uint8_t* buffer, unsigned size);
/*
 * Append bytes from `buffer`.
 * `dest` char size must be 1.
 *
 * Return false if out of memory.
 */

/****************************************************************
 * Insert functions
 * TODO other types
 */

#define uw_string_insert_chars(str, position, chr, n) _Generic((chr), \
              char32_t: _uw_string_insert_many_c32,   \
                   int: _uw_string_insert_many_c32    \
    )((str), (position), (chr), (n))

bool _uw_string_insert_many_c32(UwValuePtr str, unsigned position, char32_t chr, unsigned n);

/****************************************************************
 * Append substring functions.
 *
 * Append `src` substring starting from `src_start_pos` to `src_end_pos`.
 */

#define uw_string_append_substring(dest, src, src_start_pos, src_end_pos) _Generic((src), \
                 char*: _uw_string_append_substring_u8_wrapper,  \
              char8_t*: _uw_string_append_substring_u8,          \
             char32_t*: _uw_string_append_substring_u32,         \
            UwValuePtr: _uw_string_append_substring              \
    )((dest), (src), (src_start_pos), (src_end_pos))

bool _uw_string_append_substring_u8 (UwValuePtr dest, char8_t*   src, unsigned src_start_pos, unsigned src_end_pos);
bool _uw_string_append_substring_u32(UwValuePtr dest, char32_t*  src, unsigned src_start_pos, unsigned src_end_pos);
bool _uw_string_append_substring    (UwValuePtr dest, UwValuePtr src, unsigned src_start_pos, unsigned src_end_pos);

static inline bool _uw_string_append_substring_u8_wrapper(UwValuePtr dest, char* src, unsigned src_start_pos, unsigned src_end_pos)
{
    return _uw_string_append_substring_u8(dest, (char8_t*) src, src_start_pos, src_end_pos);
}

/****************************************************************
 * Substring comparison functions.
 *
 * Compare `str_a` from `start_pos` to `end_pos` with `str_b`.
 */

#define uw_substring_eq(a, start_pos, end_pos, b) _Generic((b), \
             char*: _uw_substring_eq_u8_wrapper,  \
          char8_t*: _uw_substring_eq_u8,          \
         char32_t*: _uw_substring_eq_u32,         \
        UwValuePtr: _uw_substring_eq              \
    )((a), (start_pos), (end_pos), (b))

bool _uw_substring_eq_u8 (UwValuePtr a, unsigned start_pos, unsigned end_pos, char8_t*   b);
bool _uw_substring_eq_u32(UwValuePtr a, unsigned start_pos, unsigned end_pos, char32_t*  b);
bool _uw_substring_eq    (UwValuePtr a, unsigned start_pos, unsigned end_pos, UwValuePtr b);

static inline bool _uw_substring_eq_u8_wrapper(UwValuePtr a, unsigned start_pos, unsigned end_pos, char* b)
{
    return _uw_substring_eq_u8(a, start_pos, end_pos, (char8_t*) b);
}


#define uw_startswith(str, prefix) _Generic((prefix), \
               int: _uw_startswith_c32,  \
          char32_t: _uw_startswith_c32,  \
             char*: _uw_startswith_u8_wrapper,  \
          char8_t*: _uw_startswith_u8,   \
         char32_t*: _uw_startswith_u32,  \
        UwValuePtr: _uw_startswith       \
    )((str), (prefix))

bool _uw_startswith_c32(UwValuePtr str, char32_t   prefix);
bool _uw_startswith_u8 (UwValuePtr str, char8_t*   prefix);
bool _uw_startswith_u32(UwValuePtr str, char32_t*  prefix);
bool _uw_startswith    (UwValuePtr str, UwValuePtr prefix);

static inline bool _uw_startswith_u8_wrapper(UwValuePtr str, char* prefix)
{
    return _uw_startswith_u8(str, (char8_t*) prefix);
}


#define uw_endswith(str, suffix) _Generic((suffix), \
               int: _uw_endswith_c32,  \
          char32_t: _uw_endswith_c32,  \
             char*: _uw_endswith_u8_wrapper,  \
          char8_t*: _uw_endswith_u8,   \
         char32_t*: _uw_endswith_u32,  \
        UwValuePtr: _uw_endswith       \
    )((str), (suffix))

bool _uw_endswith_c32(UwValuePtr str, char32_t   suffix);
bool _uw_endswith_u8 (UwValuePtr str, char8_t*   suffix);
bool _uw_endswith_u32(UwValuePtr str, char32_t*  suffix);
bool _uw_endswith    (UwValuePtr str, UwValuePtr suffix);

static inline bool _uw_endswith_u8_wrapper(UwValuePtr str, char* suffix)
{
    return _uw_endswith_u8(str, (char8_t*) suffix);
}


/****************************************************************
 * Split functions.
 * Return array of strings.
 * maxsplit == 0 imposes no limit
 */

UwResult uw_string_split(UwValuePtr str, unsigned maxsplit);  // split by spaces
UwResult uw_string_split_chr(UwValuePtr str, char32_t splitter, unsigned maxsplit);
UwResult uw_string_rsplit_chr(UwValuePtr str, char32_t splitter, unsigned maxsplit);

#define uw_string_split_any(str, splitters, maxsplit) _Generic((splitters),  \
                 char*: _uw_string_split_any_u8_wrapper, \
              char8_t*: _uw_string_split_any_u8,         \
             char32_t*: _uw_string_split_any_u32,        \
            UwValuePtr: _uw_string_split_any             \
    )((str), (splitters), (maxsplit))

UwResult _uw_string_split_any_u8 (UwValuePtr str, char8_t*   splitters, unsigned maxsplit);
UwResult _uw_string_split_any_u32(UwValuePtr str, char32_t*  splitters, unsigned maxsplit);
UwResult _uw_string_split_any    (UwValuePtr str, UwValuePtr splitters, unsigned maxsplit);

static inline UwResult _uw_string_split_any_u8_wrapper(UwValuePtr str, char* splitters, unsigned maxsplit)
{
    return _uw_string_split_any_u8(str, (char8_t*) splitters, maxsplit);
}

#define uw_string_split_strict(str, splitter, maxsplit) _Generic((splitter),  \
              char32_t: _uw_string_split_c32,               \
                   int: _uw_string_split_c32,               \
                 char*: _uw_string_split_strict_u8_wrapper, \
              char8_t*: _uw_string_split_strict_u8,         \
             char32_t*: _uw_string_split_strict_u32,        \
            UwValuePtr: _uw_string_split_strict             \
    )((str), (splitter), (maxsplit))

UwResult _uw_string_split_strict_u8 (UwValuePtr str, char8_t*   splitter, unsigned maxsplit);
UwResult _uw_string_split_strict_u32(UwValuePtr str, char32_t*  splitter, unsigned maxsplit);
UwResult _uw_string_split_strict    (UwValuePtr str, UwValuePtr splitter, unsigned maxsplit);

static inline UwResult _uw_string_split_strict_u8_wrapper(UwValuePtr str, char* splitter, unsigned maxsplit)
{
    return _uw_string_split_strict_u8(str, (char8_t*) splitter, maxsplit);
}


/****************************************************************
 * Conversion functions.
 */

UwResult uw_string_to_int(UwValuePtr str);
UwResult uw_string_to_float(UwValuePtr str);


/****************************************************************
 * String variable declarations and rvalues with initialization
 */

#define __UWDECL_String_1_12(name, len, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11)  \
    /* declare String variable, character size 1 byte, up to 12 chars */  \
    _UwValue name = {  \
        ._emb_string_type_id = UwTypeId_String,  \
        .str_embedded = 1,  \
        .str_embedded_length = (len),  \
        .str_1[0] = (c0),  \
        .str_1[1] = (c1),  \
        .str_1[2] = (c2),  \
        .str_1[3] = (c3),  \
        .str_1[4] = (c4),  \
        .str_1[5] = (c5),  \
        .str_1[6] = (c6),  \
        .str_1[7] = (c7),  \
        .str_1[8] = (c8),  \
        .str_1[9] = (c9),  \
        .str_1[10] = (c10), \
        .str_1[11] = (c11) \
    }

#define UWDECL_String_1_12(len, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11)  \
    _UW_VALUE_AUTOCLEAN __UWDECL_String_1_12(v, (len), (c0), (c1), (c2), (c3), (c4), (c5), (c6), (c7), (c8), (c9), (c10), (c11))

#define UwString_1_12(len, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11)  \
    /* make String rvalue, character size 1 byte, up to 12 chars */  \
    ({  \
        __UWDECL_String_1_12(v, (len), (c0), (c1), (c2), (c3), (c4), (c5), (c6), (c7), (c8), (c9), (c10), (c11));  \
        static_assert((len) <= UW_LENGTH(v.str_1));  \
        v;  \
    })

#define __UWDECL_String_2_6(name, len, c0, c1, c2, c3, c4, c5)  \
    /* declare String variable, character size 2 bytes, up to 6 chars */  \
    _UwValue name = {  \
        ._emb_string_type_id = UwTypeId_String,  \
        .str_embedded = 1,  \
        .str_embedded_char_size = 1,  \
        .str_embedded_length = (len),  \
        .str_2[0] = (c0),  \
        .str_2[1] = (c1),  \
        .str_2[2] = (c2),  \
        .str_2[3] = (c3),  \
        .str_2[4] = (c4),  \
        .str_2[5] = (c5)   \
    }

#define UWDECL_String_2_6(len, c0, c1, c2, c3, c4, c5)  \
    _UW_VALUE_AUTOCLEAN __UWDECL_String_2_6(v, (len), (c0), (c1), (c2), (c3), (c4), (c5))

#define UwString_2_6(len, c0, c1, c2, c3, c4, c5)  \
    /* make String rvalue, character size 2 bytes, up to 6 chars */  \
    ({  \
        __UWDECL_String_2_6(v, (len), (c0), (c1), (c2), (c3), (c4), (c5));  \
        static_assert((len) <= UW_LENGTH(v.str_2));  \
        v;  \
    })

#define __UWDECL_String_3_4(name, len, c0, c1, c2, c3)  \
    /* declare String variable, character size 3 bytes, up to 4 chars */  \
    _UwValue name = {  \
        ._emb_string_type_id = UwTypeId_String,  \
        .str_embedded = 1,  \
        .str_embedded_char_size = 2,  \
        .str_embedded_length = (len),  \
        .str_3[0] = {{ [0] = (uint8_t) (c0), [1] = (uint8_t) ((c0) >> 8), [2] = (uint8_t) ((c0) >> 16) }},  \
        .str_3[1] = {{ [0] = (uint8_t) (c1), [1] = (uint8_t) ((c1) >> 8), [2] = (uint8_t) ((c1) >> 16) }},  \
        .str_3[2] = {{ [0] = (uint8_t) (c2), [1] = (uint8_t) ((c2) >> 8), [2] = (uint8_t) ((c2) >> 16) }},  \
        .str_3[3] = {{ [0] = (uint8_t) (c3), [1] = (uint8_t) ((c3) >> 8), [2] = (uint8_t) ((c3) >> 16) }}   \
    }

#define UWDECL_String_3_4(len, c0, c1, c2, c3)  \
    _UW_VALUE_AUTOCLEAN __UWDECL_String_3_4(v, (len), (c0), (c1), (c2), (c3))

#define UwString_3_4(len, c0, c1, c2, c3)  \
    /* make String rvalue, character size 3 bytes, up to 4 chars */  \
    ({  \
        __UWDECL_String_3_4(v, (len), (c0), (c1), (c2), (c3));  \
        static_assert((len) <= UW_LENGTH(v.str_3));  \
        v;  \
    })

#define __UWDECL_String_4_3(name, len, c0, c1, c2)  \
    /* declare String variable, character size 4 bytes, up to 3 chars */  \
    _UwValue name = {  \
        ._emb_string_type_id = UwTypeId_String,  \
        .str_embedded = 1,  \
        .str_embedded_char_size = 3,  \
        .str_embedded_length = (len),  \
        .str_4[0] = (c0),  \
        .str_4[1] = (c1),  \
        .str_4[2] = (c2)   \
    }

#define UWDECL_String_4_3(len, c0, c1, c2)  \
    _UW_VALUE_AUTOCLEAN __UWDECL_String_4_3(v, (len), (c0), (c1), (c2))

#define UwString_4_3(len, c0, c1, c2)  \
    /* make String rvalue, character size 4 bytes, up to 3 chars */  \
    ({  \
        __UWDECL_String_4_3(v, (len), (c0), (c1), (c2));  \
        static_assert((len) <= UW_LENGTH(v.str_4));  \
        v;  \
    })

#ifdef __cplusplus
}
#endif
