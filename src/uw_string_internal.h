#pragma once

/*
 * String internals.
 */

#include "include/uw_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UWSTRING_BLOCK_SIZE    16
/*
 * The string is allocated in blocks.
 * Given that typical allocator's granularity is 16,
 * there's no point to make block size less than that.
 */

extern UwType _uw_string_type;

/****************************************************************
 * Low level helper functions.
 */

static unsigned embedded_capacity[4] = {12, 6, 4, 3};

static inline unsigned _uw_string_capacity(UwValuePtr s)
{
    if (_uw_likely(s->str_embedded)) {
        return embedded_capacity[s->str_char_size];
    } else {
        return s->string_data->capacity;
    }
}

static inline unsigned _uw_string_length(UwValuePtr s)
{
    if (_uw_likely(s->str_embedded)) {
        return s->str_embedded_length;
    } else {
        return s->str_length;
    }
}

static inline void _uw_string_set_length(UwValuePtr s, unsigned length)
{
    if (_uw_likely(s->str_embedded)) {
        s->str_embedded_length = length;
    } else {
        s->str_length = length;
    }
}

static inline unsigned _uw_string_inc_length(UwValuePtr s, unsigned increment)
/*
 * Increment length, return previous value.
 */
{
    unsigned length;
    if (_uw_likely(s->str_embedded)) {
        length = s->str_embedded_length;
        s->str_embedded_length = length + increment;
    } else {
        length = s->str_length;
        s->str_length = length + increment;
    }
    return length;
}

/****************************************************************
 * Methods that depend on char_size field.
 */

typedef void     (*Hash)(uint8_t* self_ptr, unsigned length, UwHashContext* ctx);
typedef uint8_t  (*MaxCharSize)(uint8_t* self_ptr, unsigned length);
typedef bool     (*Equal)(uint8_t* self_ptr, UwValuePtr other, unsigned other_start_pos, unsigned length);
typedef bool     (*EqualUtf8)(uint8_t* self_ptr, char8_t* other, unsigned length);
typedef bool     (*EqualUtf32)(uint8_t* self_ptr, char32_t* other, unsigned length);
typedef void     (*CopyTo)(uint8_t* self_ptr, UwValuePtr dest, unsigned dest_start_pos, unsigned length);
typedef void     (*CopyToUtf8)(uint8_t* self_ptr, char* dest_ptr, unsigned length);
typedef unsigned (*CopyFromUtf8)(uint8_t* self_ptr, char8_t* src_ptr, unsigned length);
typedef unsigned (*CopyFromUtf32)(uint8_t* self_ptr, char32_t* src_ptr, unsigned length);

typedef struct {
    Hash          hash;
    MaxCharSize   max_char_size;
    Equal         equal;
    EqualUtf8     equal_u8;
    EqualUtf32    equal_u32;
    CopyTo        copy_to;
    CopyToUtf8    copy_to_u8;
    CopyFromUtf8  copy_from_utf8;
    CopyFromUtf32 copy_from_utf32;
} StrMethods;

extern StrMethods _uws_str_methods[4];

static inline StrMethods* get_str_methods(UwValuePtr s)
{
    return &_uws_str_methods[s->str_char_size];
}

/****************************************************************
 * Character width functions
 */

static inline uint8_t update_char_width(uint8_t width, char32_t c)
/*
 * Set bits in `width` according to char size.
 * Return updated `width`.
 *
 * This function is used to calculate max charactter size in a string.
 * The result is converted to char size by char_width_to_char_size()
 */
{
    if (_uw_unlikely(c >= 16777216)) {
        return width | 4;
    }
    if (_uw_unlikely(c >= 65536)) {
        return width | 2;
    }
    if (_uw_unlikely(c >= 256)) {
        return width | 1;
    }
    return width;
}

static inline uint8_t char_width_to_char_size(uint8_t width)
/*
 * Convert `width` bits produced by update_char_width() to char size.
 */
{
    if (width & 4) {
        return 4;
    }
    if (width & 2) {
        return 3;
    }
    if (width & 1) {
        return 2;
    }
    return 1;
}

static inline uint8_t calc_char_size(char32_t c)
{
    if (c < 256) {
        return 1;
    } else if (c < 65536) {
        return 2;
    } else if (c < 16777216) {
        return 3;
    } else {
        return 4;
    }
}

/****************************************************************
 * Misc. functions
 */

void _uw_string_dump_data(FILE* fp, UwValuePtr str, int indent);
/*
 * Helper function for _uw_string_dump.
 */

/****************************************************************
 * UTF-8 functions
 */

static inline char32_t read_utf8_char(char8_t** str)
/*
 * Decode UTF-8 character from null-terminated string, update `*str`.
 *
 * Stop decoding if null character is encountered.
 *
 * Return decoded character, null, or 0xFFFFFFFF if UTF-8 sequence is invalid.
 */
{
    char8_t c = **str;
    if (_uw_unlikely(c == 0)) {
        return 0;
    }
    (*str)++;

    if (_uw_likely(c < 0x80)) {
        return  c;
    }

    char32_t codepoint = 0;
    char8_t next;

#   define APPEND_NEXT         \
        next = **str;          \
        if (_uw_unlikely(next == 0)) return 0; \
        if (_uw_unlikely((next & 0b1100'0000) != 0b1000'0000)) goto bad_utf8; \
        (*str)++;              \
        codepoint <<= 6;       \
        codepoint |= next & 0x3F;

    codepoint = c & 0b0001'1111;
    if ((c & 0b1110'0000) == 0b1100'0000) {
        APPEND_NEXT
    } else if ((c & 0b1111'0000) == 0b1110'0000) {
        APPEND_NEXT
        APPEND_NEXT
    } else if ((c & 0b1111'1000) == 0b1111'0000) {
        APPEND_NEXT
        APPEND_NEXT
        APPEND_NEXT
    } else {
        goto bad_utf8;
    }
    if (_uw_unlikely(codepoint == 0)) {
        // zero codepoint encoded with 2 or more bytes,
        // make it invalid to avoid mixing up with 1-byte null character
bad_utf8:
        codepoint = 0xFFFFFFFF;
    }
    return codepoint;
#   undef APPEND_NEXT
}

static inline bool read_utf8_buffer(char8_t** ptr, unsigned* bytes_remaining, char32_t* codepoint)
/*
 * Decode UTF-8 character from buffer, update `*ptr`.
 *
 * Null charaters are returned as zero codepoints.
 *
 * Return false if UTF-8 sequence is incomplete or `bytes_remaining` is zero.
 * Otherwise return true.
 * If character is invalid, 0xFFFFFFFF is written to the `codepoint`.
 */
{
    char8_t* p = *ptr;
    unsigned remaining = *bytes_remaining;
    if (!remaining) {
        return false;
    }

    char32_t result = 0;
    char8_t next;

#   define APPEND_NEXT      \
        next = *p++;        \
        remaining--;        \
        if (_uw_unlikely((next & 0b1100'0000) != 0b1000'0000)) goto bad_utf8; \
        result <<= 6;       \
        result |= next & 0x3F;

    char8_t c = *p++;
    remaining--;
    if (c < 0x80) {
        result = c;
    } else {
        result = c & 0b0001'1111;
        if ((c & 0b1110'0000) == 0b1100'0000) {
            if (_uw_unlikely(!remaining)) return false;
            APPEND_NEXT
        } else if ((c & 0b1111'0000) == 0b1110'0000) {
            if (_uw_unlikely(remaining < 2)) return false;
            APPEND_NEXT
            APPEND_NEXT
        } else if ((c & 0b1111'1000) == 0b1111'0000) {
            if (_uw_unlikely(remaining < 3)) return false;
            APPEND_NEXT
            APPEND_NEXT
            APPEND_NEXT
        } else {
            goto bad_utf8;
        }
        if (_uw_unlikely(codepoint == 0)) {
            // zero codepoint encoded with 2 or more bytes,
            // make it invalid to avoid mixing up with 1-byte null character
bad_utf8:
            result = 0xFFFFFFFF;
        }
    }
    *ptr = p;
    *bytes_remaining = remaining;
    *codepoint = result;
    return true;

#   undef APPEND_NEXT
}

#ifdef __cplusplus
}
#endif
