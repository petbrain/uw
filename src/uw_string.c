#include <errno.h>
#include <string.h>

#include <libpussy/dump.h>

#include "include/uw.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_string_internal.h"

[[noreturn]]
void _uw_panic_bad_char_size(uint8_t char_size)
{
    uw_panic("Bad char size: %u\n", char_size);
}

// lookup table to validate capacity

#define _header_size  offsetof(_UwStringData, data)

static unsigned _max_capacity[4] = {
    0xFFFF'FFFF - _header_size,
    (0xFFFF'FFFF - _header_size) / 2,
    (0xFFFF'FFFF - _header_size) / 3,
    (0xFFFF'FFFF - _header_size) / 4
};

/****************************************************************
 * Basic functions
 */

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

static unsigned calc_string_data_size(uint8_t char_size, unsigned desired_capacity, unsigned* real_capacity)
/*
 * Calculate memory size for string data.
 */
{
    unsigned size = offsetof(_UwStringData, data) + char_size * desired_capacity + UWSTRING_BLOCK_SIZE - 1;
    size &= ~(UWSTRING_BLOCK_SIZE - 1);
    if (real_capacity) {
        *real_capacity = (size - offsetof(_UwStringData, data)) / char_size;
    }
    return size;
}

static inline unsigned get_string_data_size(UwValuePtr str)
/*
 * Get memory size occupied by string data.
 */
{
    return calc_string_data_size(_uw_string_char_size(str), _uw_string_capacity(str), nullptr);
}

static bool make_empty_string(UwValuePtr result, unsigned capacity, uint8_t char_size)
/*
 * Create empty string with desired parameters.
 * Result type_id must be set before calling this function, other fields are assumed undefined.
 * Return false if OOM.
 */
{
    result->str_char_size = char_size - 1;  // char_size is stored as 0-based

    // check if string can be embedded into result

    if (capacity <= embedded_capacity[char_size - 1]) {
        result->str_embedded = 1;
        result->str_embedded_length = 0;
        result->str_4[0] = 0;
        result->str_4[1] = 0;
        result->str_4[2] = 0;
        return true;
    }

    if(capacity > _max_capacity[char_size]) {
        return false;
    }

    result->str_embedded = 0;
    result->str_length = 0;

    // allocate string

    unsigned real_capacity;
    unsigned memsize = calc_string_data_size(char_size, capacity, &real_capacity);

    _UwStringData* string_data = uw_typeof(result)->allocator->allocate(memsize, true);
    if (!string_data) {
        return false;
    }
    string_data->refcount = 1;
    string_data->capacity = real_capacity;
    result->string_data = string_data;
    return true;
}

static bool _do_clone_string_data(UwValuePtr str)
/*
 * non-inline helper for clone_string_data
 */
{
    _UwStringData* orig_sdata = str->string_data;
    unsigned length = str->str_length;
    uint8_t char_size = _uw_string_char_size(str);

    // allocate string
    if (!make_empty_string(str, length, char_size)) {
        return false;
    }
    // copy original string to new string;
    // new string can be embedded, use _uw_string_start
    memcpy(_uw_string_start(str), orig_sdata->data, length * char_size);
    str->str_length = length;
    orig_sdata->refcount--;
    return true;
}

static inline bool clone_string_data(UwValuePtr str)
/*
 * If `str` is not embedded and its refcount is greater than 1,
 * make a copy of allocated string data and decrement refcount
 * of original data.
 *
 * This function is called when a string is going to be modified inplace
 * without expanding or increasing char size.
 */
{
    if (str->str_embedded) {
        return true;
    }
    _UwStringData* sdata = str->string_data;
    if (_uw_unlikely(sdata->refcount > 1)) {
        return _do_clone_string_data(str);
    }
    return true;
}

static bool _do_expand_string(UwValuePtr str, unsigned increment, uint8_t new_char_size)
/*
 * non-inline helper for expand_string
 */
{
    uint8_t char_size = _uw_string_char_size(str);

    if (str->str_embedded) {
        if (new_char_size < char_size) {
            // current char_size is greater than new one, use current as new:
            new_char_size = char_size;
        }
        if (increment > _max_capacity[new_char_size] - str->str_embedded_length) {
            return false;
        }
        unsigned new_length = str->str_embedded_length + increment;
        if (new_length <= embedded_capacity[new_char_size - 1]) {
            // no need to expand
            if (new_char_size > char_size) {
                // but need to make existing chars wider
                _UwValue orig_str = *str;
                str->str_char_size = new_char_size - 1;  // char_size is stored as 0-based
                get_str_methods(&orig_str)->copy_to(
                    _uw_string_start(&orig_str),
                    str, 0, orig_str.str_embedded_length
                );
            }
            return true;
        }
        // go copy

    } else if (str->string_data->refcount == 1 && new_char_size <= char_size) {

        // expand string inplace

        uint8_t char_size = _uw_string_char_size(str);
        unsigned length = str->str_length;

        if (increment > _max_capacity[char_size] - length) {
            return false;
        }

        unsigned new_length = length + increment;
        unsigned capacity = _uw_string_capacity(str);

        if (new_length <= capacity) {
            // no need to expand
            return true;
        }

        unsigned orig_memsize = get_string_data_size(str);
        unsigned new_capacity;
        unsigned new_memsize = calc_string_data_size(char_size, new_length, &new_capacity);

        // reallocate data

        if (!uw_typeof(str)->allocator->reallocate((void**) &str->string_data,
                                                    orig_memsize, new_memsize, false, nullptr)) {
            return false;
        }
        str->string_data->capacity = new_capacity;
        str->str_length = length;
        return true;
    }

    // make a copy before modification of shared string data

    _UwValue orig_str = *str;
    unsigned length = _uw_string_length(str);
    unsigned capacity = _uw_string_capacity(str);

    if (increment > _max_capacity[new_char_size] - length) {
        // cannot expand
        return false;
    }

    unsigned new_capacity = length + increment;
    if (new_capacity < capacity) {
        new_capacity = capacity;
    }

    if (new_char_size < char_size) {
        new_char_size = char_size;
    }

    // allocate string
    if (!make_empty_string(str, new_capacity, new_char_size)) {
        return false;
    }
    // copy original string to new string
    get_str_methods(&orig_str)->copy_to(_uw_string_start(&orig_str), str, 0, length);
    str->str_length = length;

    if (!orig_str.str_embedded) {
        if (--orig_str.string_data->refcount == 0) {
            // free original string data if reference count is dropped to zero
            uw_typeof(&orig_str)->allocator->release((void**) &orig_str.string_data, get_string_data_size(&orig_str));
        }
    }
    return true;
}

static inline bool expand_string(UwValuePtr str, unsigned increment, uint8_t new_char_size)
/*
 * Expand string in place, if necessary, replacing `str->string_data`.
 *
 * If string refcount is greater than 1, always make a copy of `str->string_data`
 * because the string is about to be updated.
 */
{
    uw_assert_string(str);

    // quick check if we should not do anything

    uint8_t char_size = _uw_string_char_size(str);
    if (_uw_likely(new_char_size <= char_size)) {
        if (_uw_likely(str->str_embedded)) {
            if (str->str_embedded_length + increment < embedded_capacity[char_size]) {
                return true;
            }
        } else {
            if (str->str_length + increment < str->string_data->capacity) {
                return true;
            }
        }
    }

    return _do_expand_string(str, increment, new_char_size);
}

/****************************************************************
 * Basic interface methods
 */

static UwResult string_create(UwTypeId type_id, void* ctor_args)
{
    // XXX use ctor_args for initializer?

//    return _uw_create_string(va_arg(ap, UwValuePtr))
    return UwString();
}

static void string_destroy(UwValuePtr self)
{
    if (self->str_embedded) {
        return;
    }
    if (0 == --self->string_data->refcount) {
        uw_typeof(self)->allocator->release((void**) &self->string_data, get_string_data_size(self));
    }
}

static UwResult string_clone(UwValuePtr self)
{
    UwValue result = *self;
    if (!result.str_embedded) {
        if (result.string_data) {
            result.string_data->refcount++;
        }
    }
    return uw_move(&result);
}

static UwResult string_deepcopy(UwValuePtr self)
{
    UwValue result = UwString();
    unsigned length = _uw_string_length(self);
    if (!make_empty_string(&result, length, _uw_string_char_size(self))) {
        return UwOOM();
    }
    get_str_methods(self)->copy_to(_uw_string_start(self), &result, 0, length);
    _uw_string_set_length(&result, length);
    return uw_move(&result);
}

static void string_hash(UwValuePtr self, UwHashContext* ctx)
{
    // mind maps: the hash should be the same for subtypes, that's why not using self->type_id here
    _uw_hash_uint64(ctx, UwTypeId_String);

    unsigned length = _uw_string_length(self);
    if (length) {
        get_str_methods(self)->hash(_uw_string_start(self), length, ctx);
    }
}

static void string_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    _uw_string_dump_data(fp, self, next_indent);
}

void _uw_string_dump_data(FILE* fp, UwValuePtr str, int indent)
{
    if (str->str_embedded) {
        fprintf(fp, " embedded,");
    } else {
        fprintf(fp, " data=%p, refcount=%u, data size=%u, ptr=%p",
                str->string_data, str->string_data->refcount,
                get_string_data_size(str), _uw_string_start(str));
    }

    unsigned capacity = _uw_string_capacity(str);
    unsigned length = _uw_string_length(str);
    uint8_t char_size = _uw_string_char_size(str);

    fprintf(fp, " length=%u, capacity=%u, char size=%u\n", length, capacity, char_size);
    indent += 4;

    if (length) {
        // print first 80 characters
        _uw_print_indent(fp, indent);
        uint8_t* rem_addr;
        uint8_t* start = _uw_string_start_end(str, &rem_addr);
        uint8_t* ptr = start;
        for(unsigned i = 0; i < length; i++) {
            _uw_putchar32_utf8(fp, _uw_get_char(ptr, char_size));
            ptr += char_size;
            if (i == 80) {
                fprintf(fp, "...");
                break;
            }
        }
        fputc('\n', fp);

        dump_hex(fp, indent, start, length * char_size, (uint8_t*) (((ptrdiff_t) start) & 15), true, true);
        if (length < capacity) {
            _uw_print_indent(fp, indent);
            fputs("capacity remainder:\n", fp);
            dump_hex(fp, indent, rem_addr, (capacity - length) * char_size,
                     (uint8_t*) ((((ptrdiff_t) start) & 15) + length * char_size), true, true);
        }
    }
}

static bool string_is_true(UwValuePtr self)
{
    return _uw_string_length(self);
}

static inline bool _uw_string_eq(UwValuePtr a, UwValuePtr b)
{
    if (a == b) {
        return true;
    }
    unsigned a_length = _uw_string_length(a);
    unsigned b_length = _uw_string_length(b);
    if (a_length != b_length) {
        return false;
    }
    if (a_length == 0) {
        return true;
    }
    return get_str_methods(a)->equal(_uw_string_start(a), b, 0, a_length);
}

static bool string_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return _uw_string_eq(self, other);
}

static bool string_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_String:
                return _uw_string_eq(self, other);

            case UwTypeId_CharPtr:
                return _uw_charptr_equal_string(other, self);

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

UwType _uw_string_type = {
    .id             = UwTypeId_String,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "String",
    .allocator      = &default_allocator,
    .create         = string_create,
    .destroy        = string_destroy,
    .clone          = string_clone,
    .hash           = string_hash,
    .deepcopy       = string_deepcopy,
    .dump           = string_dump,
    .to_string      = string_deepcopy,  // yes, simply make a copy
    .is_true        = string_is_true,
    .equal_sametype = string_equal_sametype,
    .equal          = string_equal

    // [UwInterfaceId_RandomAccess] = &string_type_random_access_interface
};

/****************************************************************
 * UTF-8/UTF-32 functions
 */

char* uw_char32_to_utf8(char32_t codepoint, char* buffer)
{
    /*
     * U+0000 - U+007F      0xxxxxxx
     * U+0080 - U+07FF      110xxxxx  10xxxxxx
     * U+0800 - U+FFFF      1110xxxx  10xxxxxx  10xxxxxx
     * U+010000 - U+10FFFF  11110xxx  10xxxxxx  10xxxxxx  10xxxxxx
     */
    if (codepoint < 0x80) {
        *buffer++ = (char) codepoint;
        return buffer;
    }
    if (codepoint < 0b1'00000'000000) {
        *buffer++ = (char) (0xC0 | (codepoint >> 6));
        *buffer++ = (char) (0x80 | (codepoint & 0x3F));
        return buffer;
    }
    if (codepoint < 0b1'0000'000000'000000) {
        *buffer++ = (char) (0xE0 | (codepoint >> 12));
        *buffer++ = (char) (0x80 | ((codepoint >> 6) & 0x3F));
        *buffer++ = (char) (0x80 | (codepoint & 0x3F));
        return buffer;
    }
    *buffer++ = (char) (0xF0 | ((codepoint >> 18) & 0x07));
    *buffer++ = (char) (0x80 | ((codepoint >> 12) & 0x3F));
    *buffer++ = (char) (0x80 | ((codepoint >> 6) & 0x3F));
    *buffer++ = (char) (0x80 | (codepoint & 0x3F));
    return buffer;
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
    unsigned remaining = *bytes_remaining;
    if (!remaining) {
        return false;
    }

    char32_t result = 0;
    char8_t next;

    char8_t c = *((*ptr)++);
    remaining--;

#   define APPEND_NEXT         \
        next = **ptr;          \
        if (_uw_unlikely((next & 0b1100'0000) != 0b1000'0000)) goto bad_utf8; \
        (*ptr)++;              \
        remaining--;  \
        result <<= 6;       \
        result |= next & 0x3F;

    if (c < 0x80) {
        result = c;
        *bytes_remaining = remaining;
    } else {
        result = c & 0b0001'1111;
        if ((c & 0b1110'0000) == 0b1100'0000) {
            if (_uw_unlikely(!remaining)) return false;
            APPEND_NEXT
            *bytes_remaining = remaining;
        } else if ((c & 0b1111'0000) == 0b1110'0000) {
            if (_uw_unlikely(remaining < 2)) return false;
            APPEND_NEXT
            APPEND_NEXT
            *bytes_remaining = remaining;
        } else if ((c & 0b1111'1000) == 0b1111'0000) {
            if (_uw_unlikely(remaining < 3)) return false;
            APPEND_NEXT
            APPEND_NEXT
            APPEND_NEXT
            *bytes_remaining = remaining;
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
    *codepoint = result;
    return true;
#   undef APPEND_NEXT
}

unsigned utf8_strlen(char8_t* str)
{
    unsigned length = 0;
    while(_uw_likely(*str != 0)) {
        char32_t c = read_utf8_char(&str);
        if (_uw_likely(c != 0xFFFFFFFF)) {
            length++;
        }
    }
    return length;
}

unsigned utf8_strlen2(char8_t* str, uint8_t* char_size)
{
    unsigned length = 0;
    uint8_t  width = 0;
    while(_uw_likely(*str != 0)) {
        char32_t c = read_utf8_char(&str);
        if (_uw_likely(c != 0xFFFFFFFF)) {
            width = update_char_width(width, c);
            length++;
        }
    }
    *char_size = char_width_to_char_size(width);
    return length;
}

unsigned utf8_strlen2_buf(char8_t* buffer, unsigned* size, uint8_t* char_size)
{
    char8_t* ptr = buffer;
    unsigned bytes_remaining = *size;
    unsigned length = 0;
    uint8_t  width = 0;

    while (_uw_likely(bytes_remaining)) {
        char32_t c;
        if (_uw_unlikely(!read_utf8_buffer(&ptr, &bytes_remaining, &c))) {
            break;
        }
        if (_uw_likely(c != 0xFFFFFFFF)) {
            width = update_char_width(width, c);
            length++;
        }
    }
    *size -= bytes_remaining;

    if (char_size) {
        *char_size = char_width_to_char_size(width);
    }

    return length;
}

uint8_t utf8_char_size(char8_t* str, unsigned max_len)
{
    uint8_t width = 0;
    while(_uw_likely(*str != 0)) {
        char32_t c = read_utf8_char(&str);
        if (_uw_likely(c != 0xFFFFFFFF)) {
            width = update_char_width(width, c);
        }
    }
    return char_width_to_char_size(width);
}

char8_t* utf8_skip(char8_t* str, unsigned n)
{
    while(n--) {
        read_utf8_char(&str);
        if (_uw_unlikely(*str == 0)) {
            break;
        }
    }
    return str;
}

unsigned uw_strlen_in_utf8(UwValuePtr str)
{
#   define INCREMENT_LENGTH  \
        if (c < 0x80) {  \
            length++;  \
        } else if (c < 0b1'00000'000000) {  \
            length += 2;  \
        } else if (c < 0b1'0000'000000'000000) {  \
            length += 3;  \
        } else {  \
            length += 4;  \
        }

    unsigned length = 0;

    if (_uw_unlikely(uw_is_charptr(str))) {
        switch (str->charptr_subtype) {
            case UW_CHARPTR:
                length = strlen((char*) str->charptr);
                break;
            case UW_CHAR32PTR: {
                char32_t* ptr = str->char32ptr;
                for (;;) {
                    char32_t c = *ptr++;
                    if (c == 0) {
                        break;
                    }
                    INCREMENT_LENGTH
                }
                break;
            }
            default:
                _uw_panic_bad_charptr_subtype(str);
        }
    } else {
        uw_assert_string(str);
        uint8_t char_size = _uw_string_char_size(str);
        unsigned n;
        uint8_t* ptr = _uw_string_start_length(str, &n);
        while (n) {
            char32_t c = _uw_get_char(ptr, char_size);
            INCREMENT_LENGTH
            ptr += char_size;
            n--;
        }
    }
    return length;

#   undef INCREMENT_LENGTH
}

void _uw_putchar32_utf8(FILE* fp, char32_t codepoint)
{
    char buffer[5];
    char* start = buffer;
    char* end = uw_char32_to_utf8(codepoint, buffer);
    while (start < end) {
        fputc(*start++, fp);
    }
}

unsigned u32_strlen(char32_t* str)
{
    unsigned length = 0;
    while (_uw_likely(*str++)) {
        length++;
    }
    return length;
}

unsigned u32_strlen2(char32_t* str, uint8_t* char_size)
{
    unsigned length = 0;
    uint8_t width = 0;
    char32_t c;
    while (_uw_likely(c = *str++)) {
        width = update_char_width(width, c);
        length++;
    }
    *char_size = char_width_to_char_size(width);
    return length;
}

int u32_strcmp(char32_t* a, char32_t* b)
{
    if (a == b) {
        return 0;
    }
    for (;;) {
        char32_t ca = *a++;
        char32_t cb = *b++;
        if (ca < cb) {
            return -1;
        } else if (ca > cb) {
            return 1;
        } else if (ca == 0) {
            return 0;
        }
    }
}

int u32_strcmp_u8(char32_t* a, char8_t* b)
{
    for (;;) {
        char32_t ca = *a++;
        char32_t cb = read_utf8_char(&b);
        if (ca < cb) {
            return -1;
        } else if (ca > cb) {
            return 1;
        } else if (ca == 0) {
            return 0;
        }
    }
}

char32_t* u32_strchr(char32_t* str, char32_t chr)
{
    char32_t c;
    while (_uw_likely(c = *str)) {
        if (_uw_unlikely(c == chr)) {
            return str;
        }
        str++;
    }
    return nullptr;
}

uint8_t u32_char_size(char32_t* str, unsigned max_len)
{
    uint8_t width = 0;
    while (max_len--) {
        char32_t c = *str++;
        if (_uw_unlikely(c == 0)) {
            break;
        }
        width = update_char_width(width, c);
    }
    return char_width_to_char_size(width);
}


/****************************************************************
 * Methods that depend on char_size field.
 */

/*
 * Implementation of hash methods.
 *
 * Calculate hash as if characters were chat32_t regardless of storage size.
 */

// integral types:

#define STR_HASH_IMPL(type_name)  \
    static void _hash_##type_name(uint8_t* self_ptr, unsigned length, UwHashContext* ctx) \
    {  \
        type_name* ptr = (type_name*) self_ptr;  \
        while (_uw_likely(length--)) {  \
            union {  \
                struct {  \
                    char32_t a;  \
                    char32_t b;  \
                };  \
                uint64_t i64;  \
            } data;  \
            \
            data.a = *ptr++;  \
            \
            if (_uw_unlikely(0 == length--)) {  \
                data.b = 0;  \
                _uw_hash_uint64(ctx, data.i64);  \
                break;  \
            }  \
            data.b = *ptr++;  \
            _uw_hash_uint64(ctx, data.i64);  \
        }  \
    }

STR_HASH_IMPL(uint8_t)
STR_HASH_IMPL(uint16_t)
STR_HASH_IMPL(uint32_t)

// uint24:

static void _hash_uint24_t(uint8_t* self_ptr, unsigned length, UwHashContext* ctx)
{
    while (_uw_likely(length--)) {
        union {
            struct {
                char32_t a;
                char32_t b;
            };
            uint64_t i64;
        } data;

        data.a = _uw_get_char_uint24_t(self_ptr);
        self_ptr += 3;

        if (_uw_unlikely(0 == length--)) {
            data.b = 0;
            _uw_hash_uint64(ctx, data.i64);
            break;
        }
        data.b = _uw_get_char_uint24_t(self_ptr);
        self_ptr += 3;
        _uw_hash_uint64(ctx, data.i64);
    }
}


/*
 * Implementation of max_char_size methods.
 */

static uint8_t _max_char_size_uint8_t(uint8_t* self_ptr, unsigned length)
{
    return 1;
}

static uint8_t _max_char_size_uint16_t(uint8_t* self_ptr, unsigned length)
{
    uint16_t* ptr = (uint16_t*) self_ptr;
    while (_uw_likely(length--)) {
        uint16_t c = *ptr++;
        if (_uw_unlikely(c >= 256)) {
            return 2;
        }
    }
    return 1;
}

static uint8_t _max_char_size_uint24_t(uint8_t* self_ptr, unsigned length)
{
    while (_uw_likely(length--)) {
        char32_t c = _uw_get_char_uint24_t(self_ptr);
        self_ptr += 3;
        if (_uw_unlikely(c >= 65536)) {
            return 3;
        } else if (_uw_unlikely(c >= 256)) {
            return 2;
        }
    }
    return 1;
}

static uint8_t _max_char_size_uint32_t(uint8_t* self_ptr, unsigned length)
{
    uint32_t* ptr = (uint32_t*) self_ptr;
    while (_uw_likely(length--)) {
        uint32_t c = *ptr++;
        if (_uw_unlikely(c >= 16777216)) {
            return 4;
        } else if (_uw_unlikely(c >= 65536)) {
            return 3;
        } else if (_uw_unlikely(c >= 256)) {
            return 2;
        }
    }
    return 1;
}

/*
 * Implementation of equality methods.
 *
 * In helper functions self argument is always uint8_t regardless of char width.
 * That's because get_ptr returns uint8_t*.
 */

// memcmp for the same char size:

#define STR_EQ_MEMCMP_HELPER_IMPL(type_name)  \
    static inline bool eq_##type_name##_##type_name(uint8_t* self_ptr, type_name* other_ptr, unsigned length)  \
    {  \
        return memcmp(self_ptr, other_ptr, length * sizeof(type_name)) == 0;  \
    }

STR_EQ_MEMCMP_HELPER_IMPL(uint8_t)
STR_EQ_MEMCMP_HELPER_IMPL(uint16_t)
STR_EQ_MEMCMP_HELPER_IMPL(uint24_t)
STR_EQ_MEMCMP_HELPER_IMPL(uint32_t)

// integral types:

#define STR_EQ_HELPER_IMPL(type_name_self, type_name_other)  \
    static inline bool eq_##type_name_self##_##type_name_other(uint8_t* self_ptr, type_name_other* other_ptr, unsigned length)  \
    {  \
        type_name_self* this_ptr = (type_name_self*) self_ptr;  \
        while (_uw_likely(length--)) {  \
            if (_uw_unlikely(*this_ptr++ != *other_ptr++)) {  \
                return false;  \
            }  \
        }  \
        return true;  \
    }

STR_EQ_HELPER_IMPL(uint8_t,  uint16_t)
STR_EQ_HELPER_IMPL(uint8_t,  uint32_t)
STR_EQ_HELPER_IMPL(uint16_t, uint8_t)
STR_EQ_HELPER_IMPL(uint16_t, uint32_t)
STR_EQ_HELPER_IMPL(uint32_t, uint8_t)
STR_EQ_HELPER_IMPL(uint32_t, uint16_t)

// uint24_t as self type:

#define STR_EQ_S24_HELPER_IMPL(type_name_other)  \
    static inline bool eq_uint24_t_##type_name_other(uint8_t* self_ptr, type_name_other* other_ptr, unsigned length)  \
    {  \
        while (_uw_likely(length--)) {  \
            if (_uw_unlikely(_uw_get_char_uint24_t(self_ptr) != *other_ptr++)) {  \
                return false;  \
            }  \
            self_ptr += 3;  \
        }  \
        return true;  \
    }

STR_EQ_S24_HELPER_IMPL(uint8_t)
STR_EQ_S24_HELPER_IMPL(uint16_t)
STR_EQ_S24_HELPER_IMPL(uint32_t)

// uint24_t as other type:

#define STR_EQ_O24_HELPER_IMPL(type_name_self)  \
    static inline bool eq_##type_name_self##_uint24_t(uint8_t* self_ptr, uint24_t* other_ptr, unsigned length)  \
    {  \
        type_name_self* this_ptr = (type_name_self*) self_ptr;  \
        while (_uw_likely(length--)) {  \
            if (_uw_unlikely(*this_ptr++ != _uw_get_char_uint24_t((uint8_t*) other_ptr))) {  \
                return false;  \
            }  \
            other_ptr++;  \
        }  \
        return true;  \
    }

STR_EQ_O24_HELPER_IMPL(uint8_t)
STR_EQ_O24_HELPER_IMPL(uint16_t)
STR_EQ_O24_HELPER_IMPL(uint32_t)

#define STR_EQ_IMPL(type_name_self)  \
    static bool _eq_##type_name_self(uint8_t* self_ptr, UwValuePtr other, unsigned other_start_pos, unsigned length)  \
    {  \
        uint8_t char_size = _uw_string_char_size(other);  \
        uint8_t* other_ptr = _uw_string_start(other) + other_start_pos * char_size;  \
        switch (char_size) {  \
            case 1: return eq_##type_name_self##_uint8_t(self_ptr, (uint8_t*) other_ptr, length);  \
            case 2: return eq_##type_name_self##_uint16_t(self_ptr, (uint16_t*) other_ptr, length);  \
            case 3: return eq_##type_name_self##_uint24_t(self_ptr, (uint24_t*) other_ptr, length);  \
            case 4: return eq_##type_name_self##_uint32_t(self_ptr, (uint32_t*) other_ptr, length);  \
            default: return false;  \
        }  \
    }

STR_EQ_IMPL(uint8_t)
STR_EQ_IMPL(uint16_t)
STR_EQ_IMPL(uint24_t)
STR_EQ_IMPL(uint32_t)

// comparison with har32_t null-terminated string, integral `self` types

#define STR_EQ_U32_IMPL(type_name_self)  \
    static bool _eq_##type_name_self##_char32_t(uint8_t* self_ptr, char32_t* other, unsigned length)  \
    {  \
        type_name_self* this_ptr = (type_name_self*) self_ptr;  \
        while (_uw_likely(length--)) {  \
            char32_t c = *other++;  \
            if (_uw_unlikely(c == 0)) {  \
                return false;  \
            }  \
            if (_uw_unlikely(*this_ptr++ != c)) {  \
                return false;  \
            }  \
        }  \
        return *other == 0;  \
    }

STR_EQ_U32_IMPL(uint8_t)
STR_EQ_U32_IMPL(uint16_t)
STR_EQ_U32_IMPL(uint32_t)

// comparison with char32_t null-terminated string, uint24 self type

static bool _eq_uint24_t_char32_t(uint8_t* self_ptr, char32_t* other, unsigned length)
{
    while (_uw_likely(length--)) {
        char32_t c = *other++;
        if (_uw_unlikely(c == 0)) {
            return false;
        }
        if (_uw_unlikely((_uw_get_char_uint24_t(self_ptr)) != c)) {
            return false;
        }
        self_ptr += 3;
    }
    return *other == 0;
}

// Comparison with UTF-8 null-terminated string.
// Specific checking for invalid codepoint looks unnecessary, but wrong char32_t string may
// contain such a value. This should not make comparison successful.

#define STR_EQ_UTF8_IMPL(type_name_self, check_invalid_codepoint)  \
    static bool _eq_##type_name_self##_char8_t(uint8_t* self_ptr, char8_t* other, unsigned length)  \
    {  \
        type_name_self* this_ptr = (type_name_self*) self_ptr;  \
        while(_uw_likely(length--)) {  \
            if (_uw_unlikely(*other == 0)) {  \
                return false;  \
            }  \
            char32_t codepoint = read_utf8_char(&other);  \
            if (check_invalid_codepoint) { \
                if (_uw_unlikely(codepoint == 0xFFFFFFFF)) {  \
                    return false;  \
                }  \
            }  \
            if (_uw_unlikely(*this_ptr++ != codepoint)) {  \
                return false;  \
            }  \
        }  \
        return *other == 0;  \
    }

STR_EQ_UTF8_IMPL(uint8_t,  0)
STR_EQ_UTF8_IMPL(uint16_t, 0)
STR_EQ_UTF8_IMPL(uint32_t, 1)

static bool _eq_uint24_t_char8_t(uint8_t* self_ptr, char8_t* other, unsigned length)
{
    while(_uw_likely(length--)) {
        if (_uw_unlikely(*other == 0)) {
            return false;
        }
        char32_t codepoint = read_utf8_char(&other);
        if (_uw_unlikely(_uw_get_char_uint24_t(self_ptr) != codepoint)) {
            return false;
        }
        self_ptr += 3;
    }
    return *other == 0;
}

/*
 * Implementation of copy methods.
 *
 * When copying from a source with lesser or equal char size
 * the caller must ensure destination char size is sufficient.
 *
 * In helper functions self argument is always uint8_t regardless of char width.
 * That's because get_ptr returns uint8_t*.
 */

// use memcpy for the same char size:

#define STR_COPY_TO_MEMCPY_HELPER_IMPL(type_name)  \
    static inline void cp_##type_name##_##type_name(uint8_t* self_ptr, type_name* dest_ptr, unsigned length)  \
    {  \
        memcpy(dest_ptr, self_ptr, length * sizeof(type_name));  \
    }  \

STR_COPY_TO_MEMCPY_HELPER_IMPL(uint8_t)
STR_COPY_TO_MEMCPY_HELPER_IMPL(uint16_t)
STR_COPY_TO_MEMCPY_HELPER_IMPL(uint24_t)
STR_COPY_TO_MEMCPY_HELPER_IMPL(uint32_t)

// integral types:

#define STR_COPY_TO_HELPER_IMPL(type_name_self, type_name_dest)  \
    static inline void cp_##type_name_self##_##type_name_dest(uint8_t* self_ptr, type_name_dest* dest_ptr, unsigned length)  \
    {  \
        type_name_self* src_ptr = (type_name_self*) self_ptr;  \
        while (_uw_likely(length--)) {  \
            *dest_ptr++ = *src_ptr++;  \
        }  \
    }

STR_COPY_TO_HELPER_IMPL(uint8_t,  uint16_t)
STR_COPY_TO_HELPER_IMPL(uint8_t,  uint32_t)
STR_COPY_TO_HELPER_IMPL(uint16_t, uint8_t)
STR_COPY_TO_HELPER_IMPL(uint16_t, uint32_t)
STR_COPY_TO_HELPER_IMPL(uint32_t, uint8_t)
STR_COPY_TO_HELPER_IMPL(uint32_t, uint16_t)

// uint24_t as source type:

#define STR_COPY_TO_S24_HELPER_IMPL(type_name_dest)  \
    static inline void cp_uint24_t_##type_name_dest(uint8_t* self_ptr, type_name_dest* dest_ptr, unsigned length)  \
    {  \
        while (_uw_likely(length--)) {  \
            *dest_ptr++ = _uw_get_char_uint24_t(self_ptr);  \
            self_ptr += 3;  \
        }  \
    }

STR_COPY_TO_S24_HELPER_IMPL(uint8_t)
STR_COPY_TO_S24_HELPER_IMPL(uint16_t)
STR_COPY_TO_S24_HELPER_IMPL(uint32_t)

// uint24_t as destination type:

#define STR_COPY_TO_D24_HELPER_IMPL(type_name_self)  \
    static inline void cp_##type_name_self##_uint24_t(uint8_t* self_ptr, uint24_t* dest_ptr, unsigned length)  \
    {  \
        type_name_self* src_ptr = (type_name_self*) self_ptr;  \
        while (_uw_likely(length--)) {  \
            _uw_put_char_uint24_t((uint8_t*) dest_ptr, *src_ptr++);  \
            dest_ptr++;  \
        }  \
    }

STR_COPY_TO_D24_HELPER_IMPL(uint8_t)
STR_COPY_TO_D24_HELPER_IMPL(uint16_t)
STR_COPY_TO_D24_HELPER_IMPL(uint32_t)

#define STR_COPY_TO_IMPL(type_name_self)  \
    static void _cp_to_##type_name_self(uint8_t* self_ptr, UwValuePtr dest, unsigned dest_start_pos, unsigned length)  \
    {  \
        uint8_t char_size = _uw_string_char_size(dest);  \
        uint8_t* dest_ptr = _uw_string_start(dest) + dest_start_pos * char_size;  \
        switch (char_size) {  \
            case 1: cp_##type_name_self##_uint8_t(self_ptr, (uint8_t*) dest_ptr, length); return; \
            case 2: cp_##type_name_self##_uint16_t(self_ptr, (uint16_t*) dest_ptr, length); return; \
            case 3: cp_##type_name_self##_uint24_t(self_ptr, (uint24_t*) dest_ptr, length); return; \
            case 4: cp_##type_name_self##_uint32_t(self_ptr, (uint32_t*) dest_ptr, length); return; \
        }  \
    }

STR_COPY_TO_IMPL(uint8_t)
STR_COPY_TO_IMPL(uint16_t)
STR_COPY_TO_IMPL(uint24_t)
STR_COPY_TO_IMPL(uint32_t)

// copy to C-string

static void _cp_to_u8_uint8_t(uint8_t* self_ptr, char* dest, unsigned length)
{
    memcpy(dest, self_ptr, length);
    *(dest + length) = 0;
}

// integral types:

#define STR_COPY_TO_U8_IMPL(type_name_self)  \
    static void _cp_to_u8_##type_name_self(uint8_t* self_ptr, char* dest, unsigned length)  \
    {  \
        type_name_self* src_ptr = (type_name_self*) self_ptr;  \
        while (_uw_likely(length--)) {  \
            dest = uw_char32_to_utf8(*src_ptr++, dest); \
        }  \
        *dest = 0;  \
    }

STR_COPY_TO_U8_IMPL(uint16_t)
STR_COPY_TO_U8_IMPL(uint32_t)

// uint24_t

static void _cp_to_u8_uint24_t(uint8_t* self_ptr, char* dest, unsigned length)
{
    while (_uw_likely(length--)) {
        char32_t c = _uw_get_char_uint24_t(self_ptr);
        self_ptr += 3;
        dest = uw_char32_to_utf8(c, dest);
    }
    *dest = 0;
}

// copy from char32_t string, integral `self` types

#define STR_CP_FROM_U32_IMPL(type_name_self)  \
    static unsigned _cp_from_char32_t_##type_name_self(uint8_t* self_ptr, char32_t* src_ptr, unsigned length)  \
    {  \
        type_name_self* dest_ptr = (type_name_self*) self_ptr;  \
        unsigned chars_copied = 0;  \
        while (_uw_likely(length--)) {  \
            char32_t c = *src_ptr++;  \
            if (_uw_unlikely(c == 0)) {  \
                break;  \
            }  \
            *dest_ptr++ = c;  \
            chars_copied++;  \
        }  \
        return chars_copied;  \
    }

STR_CP_FROM_U32_IMPL(uint8_t)
STR_CP_FROM_U32_IMPL(uint16_t)
STR_CP_FROM_U32_IMPL(uint32_t)

// copy from char32_t string, uint24 self

static unsigned _cp_from_char32_t_uint24_t(uint8_t* self_ptr, char32_t* src_ptr, unsigned length)
{
    unsigned chars_copied = 0;
    while (_uw_likely(length--)) {
        char32_t c = *src_ptr++;
        if (_uw_unlikely(c == 0)) {
            break;
        }
        _uw_put_char_uint24_t(self_ptr, c);
        self_ptr += 3;
        chars_copied++;
    }
    return chars_copied;
}

// copy from UTF-8_t null-terminated string

#define STR_CP_FROM_U8_IMPL(type_name_self)  \
    static unsigned _cp_from_u8_##type_name_self(uint8_t* self_ptr, char8_t* src_ptr, unsigned length)  \
    {  \
        type_name_self* dest_ptr = (type_name_self*) self_ptr;  \
        unsigned chars_copied = 0;  \
        while (_uw_likely(*src_ptr != 0 && length--)) {  \
            char32_t c = read_utf8_char(&src_ptr);  \
            if (_uw_likely(c != 0xFFFFFFFF)) {  \
                *dest_ptr++ = c;  \
                chars_copied++;  \
            }  \
        }  \
        return chars_copied;  \
    }

STR_CP_FROM_U8_IMPL(uint8_t)
STR_CP_FROM_U8_IMPL(uint16_t)
STR_CP_FROM_U8_IMPL(uint32_t)

static unsigned _cp_from_u8_uint24_t(uint8_t* self_ptr, uint8_t* src_ptr, unsigned length)
{
    unsigned chars_copied = 0;
    while (_uw_likely((*((uint8_t*) src_ptr)) && length--)) {
        char32_t c = read_utf8_char(&src_ptr);
        if (_uw_likely(c != 0xFFFFFFFF)) {
            _uw_put_char_uint24_t(self_ptr, c);
            self_ptr += 3;
            chars_copied++;
        }
    }
    return chars_copied;
}

/*
 * String methods table
 */
StrMethods _uws_str_methods[4] = {
    { _hash_uint8_t,        _max_char_size_uint8_t,
      _eq_uint8_t,          _eq_uint8_t_char8_t,   _eq_uint8_t_char32_t,
      _cp_to_uint8_t,       _cp_to_u8_uint8_t,
      _cp_from_u8_uint8_t,  _cp_from_char32_t_uint8_t
    },
    { _hash_uint16_t,       _max_char_size_uint16_t,
      _eq_uint16_t,         _eq_uint16_t_char8_t,  _eq_uint16_t_char32_t,
      _cp_to_uint16_t,      _cp_to_u8_uint16_t,
      _cp_from_u8_uint16_t, _cp_from_char32_t_uint16_t
    },
    { _hash_uint24_t,       _max_char_size_uint24_t,
      _eq_uint24_t,         _eq_uint24_t_char8_t,  _eq_uint24_t_char32_t,
      _cp_to_uint24_t,      _cp_to_u8_uint24_t,
      _cp_from_u8_uint24_t, _cp_from_char32_t_uint24_t
    },
    { _hash_uint32_t,       _max_char_size_uint32_t,
      _eq_uint32_t,         _eq_uint32_t_char8_t,  _eq_uint32_t_char32_t,
      _cp_to_uint32_t,      _cp_to_u8_uint32_t,
      _cp_from_u8_uint32_t, _cp_from_char32_t_uint32_t
    }
};


/****************************************************************
 * Constructors
 */

UwResult uw_create_empty_string(unsigned capacity, uint8_t char_size)
{
    // using not autocleaned variable here, no uw_move necessary on exit
    __UWDECL_String(result);

    if (!make_empty_string(&result, capacity, char_size)) {
        return UwOOM();
    }
    return result;
}

UwResult _uw_create_string(UwValuePtr initializer)
{
    if (!uw_is_string(initializer)) {
        if (uw_is_charptr(initializer)) {
            return uw_charptr_to_string(initializer);
        }
        return UwError(UW_ERROR_INCOMPATIBLE_TYPE);
    }

    // using not autocleaned variable here, no uw_move necessary on exit
    __UWDECL_String(result);

    unsigned length = _uw_string_length(initializer);
    if (!make_empty_string(&result, length, _uw_string_char_size(initializer))) {
        return UwOOM();
    }
    get_str_methods(initializer)->copy_to(_uw_string_start(initializer), &result, 0, length);
    _uw_string_set_length(&result, length);
    return uw_move(&result);
}

UwResult _uw_create_string_u8(char8_t* initializer)
{
    unsigned length = 0;
    uint8_t char_size = 1;

    if (initializer && *initializer == 0) {
        initializer = nullptr;
    } else {
        length = utf8_strlen2(initializer, &char_size);
    }

    // using not autocleaned variable here, no uw_move necessary on exit
    __UWDECL_String(result);

    if (!make_empty_string(&result, length, char_size)) {
        return UwOOM();
    }
    if (initializer) {
        get_str_methods(&result)->copy_from_utf8(_uw_string_start(&result), initializer, length);
        _uw_string_set_length(&result, length);
    }
    return result;
}

UwResult _uw_create_string_u32(char32_t* initializer)
{
    unsigned length = 0;
    uint8_t char_size = 1;

    if (initializer && *initializer == 0) {
        initializer = nullptr;
    } else {
        length = u32_strlen2(initializer, &char_size);
    }

    // using not autocleaned variable here, no uw_move necessary on exit
    __UWDECL_String(result);

    if (!make_empty_string(&result, length, char_size)) {
        return UwOOM();
    }
    if (initializer) {
        get_str_methods(&result)->copy_from_utf32(_uw_string_start(&result), initializer, length);
        _uw_string_set_length(&result, length);
    }
    return result;
}

/****************************************************************
 * String functions
 */

unsigned uw_strlen(UwValuePtr str)
{
    if (_uw_unlikely(uw_is_charptr(str))) {
        uint8_t char_size;
        return _uw_charptr_strlen2(str, &char_size);
    } else {
        uw_assert_string(str);
        return _uw_string_length(str);
    }
}

#define STRING_EQ_IMPL(suffix, type_name_b)  \
{  \
    uw_assert_string(a);  \
    return get_str_methods(a)->equal_##suffix(_uw_string_start(a), (type_name_b*) b, _uw_string_length(a));  \
}

bool _uw_equal_u8 (UwValuePtr a, char8_t* b)  STRING_EQ_IMPL(u8,   char8_t)
bool _uw_equal_u32(UwValuePtr a, char32_t* b) STRING_EQ_IMPL(u32,  char32_t)

#define SUBSTRING_EQ_IMPL(suffix, type_name_b)  \
{  \
    uw_assert_string(a);  \
    unsigned a_length;  \
    uint8_t* a_ptr = _uw_string_start_length(a, &a_length);  \
    \
    if (end_pos > a_length) {  \
        end_pos = a_length;  \
    }  \
    if (end_pos < start_pos) {  \
        return false;  \
    }  \
    if (end_pos == start_pos && *b == 0) {  \
        return true;  \
    }  \
    \
    return get_str_methods(a)->equal_##suffix(  \
        a_ptr + start_pos * _uw_string_char_size(a),  \
        b,  \
        end_pos - start_pos);  \
}

bool _uw_substring_eq_u8 (UwValuePtr a, unsigned start_pos, unsigned end_pos, char8_t*  b) SUBSTRING_EQ_IMPL(u8,  char8_t)
bool _uw_substring_eq_u32(UwValuePtr a, unsigned start_pos, unsigned end_pos, char32_t* b) SUBSTRING_EQ_IMPL(u32, char32_t)

bool _uw_substring_eq(UwValuePtr a, unsigned start_pos, unsigned end_pos, UwValuePtr b)
{
    uw_assert_string(a);
    unsigned a_length;
    uint8_t* a_ptr = _uw_string_start_length(a, &a_length);

    if (end_pos > a_length) {
        end_pos = a_length;
    }
    if (end_pos < start_pos) {
        return false;
    }

    uw_assert_string(b);

    if (end_pos == start_pos && _uw_string_length(b) == 0) {
        return true;
    }

    return get_str_methods(a)->equal(
        a_ptr + start_pos * _uw_string_char_size(a),
        b, 0,
        end_pos - start_pos
    );
}

bool _uw_startswith_c32(UwValuePtr str, char32_t prefix)
{
    uw_assert_string(str);
    unsigned length = _uw_string_length(str);
    if (length == 0) {
        return false;
    }
    return _uw_get_char(_uw_string_start(str), _uw_string_char_size(str)) == prefix;
}

bool _uw_startswith_u8(UwValuePtr str, char8_t* prefix)
{
    return _uw_substring_eq_u8(str, 0, utf8_strlen(prefix), prefix);
}

bool _uw_startswith_u32(UwValuePtr str, char32_t* prefix)
{
    return _uw_substring_eq_u32(str, 0, u32_strlen(prefix), prefix);
}

bool _uw_startswith(UwValuePtr str, UwValuePtr prefix)
{
    return _uw_substring_eq(str, 0, uw_strlen(prefix), prefix);
}


bool _uw_endswith_c32(UwValuePtr str, char32_t prefix)
{
    uw_assert_string(str);
    unsigned length;
    uint8_t* ptr = _uw_string_start_length(str, &length);
    if (length == 0) {
        return false;
    }
    uint8_t char_size = _uw_string_char_size(str);
    return prefix == _uw_get_char(ptr + (length - 1) * char_size, char_size);
}

bool _uw_endswith_u8(UwValuePtr str, char8_t* suffix)
{
    unsigned str_len = uw_strlen(str);
    unsigned suffix_len = utf8_strlen(suffix);
    return _uw_substring_eq_u8(str, str_len - suffix_len, str_len, suffix);
}

bool _uw_endswith_u32(UwValuePtr str, char32_t*  suffix)
{
    unsigned str_len = uw_strlen(str);
    unsigned suffix_len = u32_strlen(suffix);
    return _uw_substring_eq_u32(str, str_len - suffix_len, str_len, suffix);
}

bool _uw_endswith(UwValuePtr str, UwValuePtr suffix)
{
    unsigned str_len = uw_strlen(str);
    unsigned suffix_len = uw_strlen(suffix);
    return _uw_substring_eq(str, str_len - suffix_len, str_len, suffix);
}


CStringPtr uw_string_to_utf8(UwValuePtr str)
{
    CStringPtr result = nullptr;

    if (_uw_unlikely(uw_is_charptr(str))) {
        switch (str->charptr_subtype) {
            case UW_CHARPTR: {
                result = malloc(strlen((char*) str) + 1);
                if (!result) {
                    return nullptr;
                }
                strcpy(result, (char*) str->charptr);
                break;
            }
            case UW_CHAR32PTR: {
                result = malloc(uw_strlen_in_utf8(str) + 1);
                if (!result) {
                    return nullptr;
                }
                char32_t* src_ptr = str->char32ptr;
                char* dest_ptr = result;
                for(;;) {
                    char32_t c = *src_ptr++;
                    if (c == 0) {
                        break;
                    }
                    dest_ptr = uw_char32_to_utf8(c, dest_ptr);
                }
                *dest_ptr = 0;
                break;
            }
            default:
                _uw_panic_bad_charptr_subtype(str);
        }
    } else {
        uw_assert_string(str);

        result = malloc(uw_strlen_in_utf8(str) + 1);
        if (!result) {
            return nullptr;
        }
        unsigned length;
        uint8_t* ptr = _uw_string_start_length(str, &length);
        get_str_methods(str)->copy_to_u8(ptr, result, length);
    }
    return result;
}

void uw_string_to_utf8_buf(UwValuePtr str, char* buffer)
{
    if (_uw_unlikely(uw_is_charptr(str))) {
        switch (str->charptr_subtype) {
            case UW_CHARPTR:
                strcpy(buffer, (char*) str->charptr);
                return;
            case UW_CHAR32PTR:
                for(char32_t* src_ptr = str->char32ptr;;) {
                    char32_t c = *src_ptr++;
                    if (c == 0) {
                        break;
                    }
                    buffer = uw_char32_to_utf8(c, buffer);
                }
                *buffer = 0;
                return;
            default:
                _uw_panic_bad_charptr_subtype(str);
        }
    } else {
        uw_assert_string(str);
        unsigned length;
        uint8_t* ptr = _uw_string_start_length(str, &length);
        get_str_methods(str)->copy_to_u8(ptr, buffer, length);
    }
}

void uw_substr_to_utf8_buf(UwValuePtr str, unsigned start_pos, unsigned end_pos, char* buffer)
{
    if (_uw_unlikely(uw_is_charptr(str))) {
        if (start_pos >= end_pos) {
            *buffer = 0;
            return;
        }
        switch (str->charptr_subtype) {
            case UW_CHARPTR: {
                char8_t* src_ptr = str->charptr;
                unsigned i = 0;
                for(; i < start_pos; i++) {
                    char32_t c = read_utf8_char(&src_ptr);
                    if (_uw_unlikely(c == 0xFFFFFFFF)) {
                        continue;
                    }
                    if (c == 0) {
                        *buffer = 0;
                        return;
                    }
                }
                for(; i < end_pos; i++) {
                    char32_t c = read_utf8_char(&src_ptr);
                    if (_uw_unlikely(c == 0xFFFFFFFF)) {
                        continue;
                    }
                    if (c == 0) {
                        *buffer = 0;
                        return;
                    }
                    buffer = uw_char32_to_utf8(c, buffer);
                }
                *buffer = 0;
                return;
            }

            case UW_CHAR32PTR: {
                char32_t* src_ptr = str->char32ptr;
                unsigned i = 0;
                for(; i < start_pos; i++) {
                    char32_t c = *src_ptr++;
                    if (c == 0) {
                        *buffer = 0;
                        return;
                    }
                }
                for(; i < end_pos; i++) {
                    char32_t c = *src_ptr++;
                    if (c == 0) {
                        *buffer = 0;
                        return;
                    }
                    buffer = uw_char32_to_utf8(c, buffer);
                }
                *buffer = 0;
                return;
            }
            default:
                _uw_panic_bad_charptr_subtype(str);
        }
    } else {
        uw_assert_string(str);
        unsigned length;
        uint8_t* ptr = _uw_string_start_length(str, &length);
        if (end_pos >= length) {
            end_pos = length;
        }
        if (end_pos <= start_pos) {
            *buffer = 0;
            return;
        }
        get_str_methods(str)->copy_to_u8(
            ptr + start_pos * _uw_string_char_size(str),
            buffer,
            end_pos - start_pos
        );
    }
}

void uw_destroy_cstring(CStringPtr* str)
{
    free(*str);
    *str = nullptr;
}

bool _uw_string_append_c32(UwValuePtr dest, char32_t c)
{
    uint8_t char_size = calc_char_size(c);
    if (!expand_string(dest, 1, char_size)) {
        return false;
    }
    unsigned length = _uw_string_inc_length(dest, 1);
    _uw_put_char(_uw_string_char_ptr(dest, length), c, char_size);
    return true;
}

static bool append_u8(UwValuePtr dest, char8_t* src, unsigned src_len, uint8_t src_char_size)
/*
 * `src_len` contains the number of codepoints, not the number of bytes in `src`
 */
{
    if (_uw_unlikely(src_len == 0)) {
        return true;
    }
    if (!expand_string(dest, src_len, src_char_size)) {
        return false;
    }
    unsigned dest_length = _uw_string_inc_length(dest, src_len);
    get_str_methods(dest)->copy_from_utf8(
        _uw_string_start(dest) + dest_length * _uw_string_char_size(dest),
        src,
        src_len
    );
    return true;
}

bool _uw_string_append_u8(UwValuePtr dest, char8_t* src)
{
    uint8_t src_char_size;
    unsigned src_len = utf8_strlen2(src, &src_char_size);
    return append_u8(dest, src, src_len, src_char_size);
}

bool _uw_string_append_substring_u8(UwValuePtr dest, char8_t* src, unsigned src_start_pos, unsigned src_end_pos)
{
    uint8_t src_char_size;
    unsigned src_len = utf8_strlen2(src, &src_char_size);
    if (src_end_pos > src_len) {
        src_end_pos = src_len;
    }
    if (src_start_pos >= src_end_pos) {
        return true;
    }
    src_len = src_end_pos - src_start_pos;
    src = utf8_skip(src, src_start_pos);

    return append_u8(dest, src, src_len, src_char_size);
}

static bool append_u32(UwValuePtr dest, char32_t* src, unsigned src_len, uint8_t src_char_size)
{
    if (_uw_unlikely(src_len == 0)) {
        return true;
    }
    if (!expand_string(dest, src_len, src_char_size)) {
        return false;
    }
    unsigned dest_length = _uw_string_inc_length(dest, src_len);
    get_str_methods(dest)->copy_from_utf32(
        _uw_string_start(dest) + dest_length * _uw_string_char_size(dest),
        src,
        src_len
    );
    return true;
}

bool _uw_string_append_u32(UwValuePtr dest, char32_t* src)
{
    uint8_t src_char_size;
    unsigned src_len = u32_strlen2(src, &src_char_size);
    return append_u32(dest, src, src_len, src_char_size);
}

bool _uw_string_append_substring_u32(UwValuePtr dest, char32_t*  src, unsigned src_start_pos, unsigned src_end_pos)
{
    uint8_t src_char_size;
    unsigned src_len = u32_strlen2(src, &src_char_size);
    if (src_end_pos > src_len) {
        src_end_pos = src_len;
    }
    if (src_start_pos >= src_end_pos) {
        return true;
    }
    src_len = src_end_pos - src_start_pos;
    src += src_start_pos;

    return append_u32(dest, src, src_len, src_char_size);
}

static bool append_string(UwValuePtr dest, UwValuePtr src, unsigned src_start_pos, unsigned src_len)
{
    if (_uw_unlikely(src_len == 0)) {
        return true;
    }
    if (!expand_string(dest, src_len, _uw_string_char_size(src))) {
        return false;
    }
    unsigned dest_length = _uw_string_inc_length(dest, src_len);
    get_str_methods(src)->copy_to(
        _uw_string_start(src) + src_start_pos * _uw_string_char_size(src),
        dest,
        dest_length, src_len
    );
    return true;
}

bool _uw_string_append(UwValuePtr dest, UwValuePtr src)
{
    if (_uw_unlikely(uw_is_charptr(src))) {
        switch (src->charptr_subtype) {
            case UW_CHARPTR:
                return _uw_string_append_u8(dest, src->charptr);
            case UW_CHAR32PTR:
                return _uw_string_append_u32(dest, src->char32ptr);
            default:
                _uw_panic_bad_charptr_subtype(src);
        }
    } else {
        uw_assert_string(src);
        unsigned src_len = _uw_string_length(src);
        return append_string(dest, src, 0, src_len);
    }
}

bool _uw_string_append_substring(UwValuePtr dest, UwValuePtr src, unsigned src_start_pos, unsigned src_end_pos)
{
    uw_assert_string(src);

    unsigned src_len = _uw_string_length(src);
    if (src_end_pos > src_len) {
        src_end_pos = src_len;
    }
    if (src_start_pos >= src_end_pos) {
        return true;
    }
    src_len = src_end_pos - src_start_pos;

    return append_string(dest, src, src_start_pos, src_len);
}

bool uw_string_append_utf8(UwValuePtr dest, char8_t* buffer, unsigned size, unsigned* bytes_processed)
{
    if (size == 0) {
        return true;
    }
    uint8_t src_char_size;
    *bytes_processed = size;
    unsigned src_len = utf8_strlen2_buf(buffer, bytes_processed, &src_char_size);

    if (src_len) {
        if (!expand_string(dest, src_len, src_char_size)) {
            return false;
        }
        unsigned dest_length = _uw_string_inc_length(dest, src_len);
        get_str_methods(dest)->copy_from_utf8(
            _uw_string_start(dest) + dest_length * _uw_string_char_size(dest),
            buffer,
            src_len
        );
    }
    return true;
}

bool uw_string_append_buffer(UwValuePtr dest, uint8_t* buffer, unsigned size)
{
    if (size == 0) {
        return true;
    }
    uw_assert(uw_string_char_size(dest) == 1);

    if (!expand_string(dest, size, 1)) {
        return false;
    }
    unsigned dest_length = _uw_string_inc_length(dest, size);
    uint8_t* ptr = _uw_string_char_ptr(dest, dest_length);
    while (size--) {
        *ptr++ = *buffer++;
    }
    return true;
}

bool _uw_string_insert_many_c32(UwValuePtr str, unsigned position, char32_t chr, unsigned n)
{
    if (n == 0) {
        return true;
    }
    uw_assert(position <= uw_strlen(str));

    if (!expand_string(str, n, calc_char_size(chr))) {
        return false;
    }
    unsigned len = _uw_string_inc_length(str, n);
    uint8_t char_size = _uw_string_char_size(str);
    uint8_t* insertion_ptr = _uw_string_start(str) + position * char_size;

    if (position < len) {
        memmove(insertion_ptr + n * char_size, insertion_ptr, (len - position) * char_size);
    }
    for (unsigned i = 0; i < n; i++) {
        _uw_put_char(insertion_ptr, chr, char_size);
        insertion_ptr += char_size;
    }
    return true;
}

UwResult uw_substr(UwValuePtr str, unsigned start_pos, unsigned end_pos)
{
    uw_assert_string(str);
    StrMethods* strmeth = get_str_methods(str);

    unsigned length;
    uint8_t* src = _uw_string_start_length(str, &length);

    if (end_pos > length) {
        end_pos = length;
    }
    if (start_pos >= end_pos) {
        return uw_create_empty_string(0, 1);
    }
    length = end_pos - start_pos;

    src += start_pos * _uw_string_char_size(str);
    uint8_t char_size = strmeth->max_char_size(src, length);

    UwValue result = uw_create_empty_string(length, char_size);
    if (uw_ok(&result)) {
        strmeth->copy_to(src, &result, 0, length);
        _uw_string_set_length(&result, length);
    }
    return uw_move(&result);
}

bool uw_string_erase(UwValuePtr str, unsigned start_pos, unsigned end_pos)
{
    uw_assert_string(str);
    unsigned length;
    uint8_t* ptr = _uw_string_start_length(str, &length);

    if (start_pos >= length || start_pos >= end_pos) {
        return true;
    }
    if (!clone_string_data(str)) {
        return false;
    }
    if (end_pos >= length) {
        // truncate
        length = start_pos;
    } else {
        unsigned tail_len = length - end_pos;
        uint8_t char_size = _uw_string_char_size(str);
        memmove(ptr + start_pos * char_size, ptr + end_pos * char_size, tail_len * char_size);
        length -= end_pos - start_pos;
    }
    _uw_string_set_length(str, length);
    return true;
}

bool uw_string_truncate(UwValuePtr str, unsigned position)
{
    uw_assert_string(str);
    if (position >= _uw_string_length(str)) {
        return true;
    }
    if (!clone_string_data(str)) {
        return false;
    }
    _uw_string_set_length(str, position);
    return true;
}

bool uw_strchr(UwValuePtr str, char32_t chr, unsigned start_pos, unsigned* result)
{
    uw_assert_string(str);
    uint8_t char_size = _uw_string_char_size(str);
    unsigned length;
    uint8_t* ptr = _uw_string_start_length(str, &length);
    ptr += start_pos * char_size;
    for (unsigned i = start_pos; i < length; i++) {
        char32_t codepoint = _uw_get_char(ptr, char_size);
        if (codepoint == chr) {
            if (result) {
                *result = i;
            }
            return true;
        }
        ptr += char_size;
    }
    return false;
}

bool uw_string_ltrim(UwValuePtr str)
{
    uw_assert_string(str);
    unsigned len;
    uint8_t* ptr = _uw_string_start_length(str, &len);
    uint8_t char_size = _uw_string_char_size(str);

    unsigned i = 0;
    while (i < len) {
        char32_t c = _uw_get_char(ptr, char_size);
        if (!uw_isspace(c)) {
            break;
        }
        i++;
        ptr += char_size;
    }
    return uw_string_erase(str, 0, i);
}

bool uw_string_rtrim(UwValuePtr str)
{
    uw_assert_string(str);
    unsigned n;
    uint8_t* ptr = _uw_string_start_length(str, &n);
    uint8_t char_size = _uw_string_char_size(str);
    ptr += n * char_size;
    while (n) {
        ptr -= char_size;
        char32_t c = _uw_get_char(ptr, char_size);
        if (!uw_isspace(c)) {
            break;
        }
        n--;
    }
    return uw_string_truncate(str, n);
}

bool uw_string_trim(UwValuePtr str)
{
    return uw_string_rtrim(str) && uw_string_ltrim(str);
}

bool uw_string_lower(UwValuePtr str)
{
    if (!expand_string(str, 0, 0)) {  // make copy if refcount > 1
        return false;
    }
    unsigned n;
    uint8_t* ptr = _uw_string_start_length(str, &n);
    uint8_t char_size = _uw_string_char_size(str);
    while (n) {
        _uw_put_char(ptr, uw_char_lower(_uw_get_char(ptr, char_size)), char_size);
        ptr += char_size;
        n--;
    }
    return true;
}

bool uw_string_upper(UwValuePtr str)
{
    if (!clone_string_data(str)) {
        return false;
    }
    unsigned n;
    uint8_t* ptr = _uw_string_start_length(str, &n);
    uint8_t char_size = _uw_string_char_size(str);
    while (n) {
        _uw_put_char(ptr, uw_char_upper(_uw_get_char(ptr, char_size)), char_size);
        ptr += char_size;
        n--;
    }
    return true;
}

UwResult uw_string_split_chr(UwValuePtr str, char32_t splitter, unsigned maxsplit)
{
    uw_assert_string(str);
    StrMethods* strmeth = get_str_methods(str);

    unsigned len;
    uint8_t* ptr = _uw_string_start_length(str, &len);
    uint8_t char_size = _uw_string_char_size(str);

    UwValue result = UwArray();
    uw_return_if_error(&result);

    uint8_t* start = ptr;
    unsigned i = 0;
    unsigned start_i = 0;
    uint8_t substr_width = 0;

    while (i < len) {
        char32_t c = _uw_get_char(ptr, char_size);
        if (c == splitter) {
            // create substring
            unsigned substr_len = i - start_i;
            UwValue substr = uw_create_empty_string(substr_len, char_width_to_char_size(substr_width));
            uw_return_if_error(&substr);

            if (substr_len) {
                strmeth->copy_to(start, &substr, 0, substr_len);
                _uw_string_set_length(&substr, substr_len);
            }
            if (!uw_array_append(&result, &substr)) {
                return UwOOM();
            }

            start_i = i + 1;
            start = ptr + char_size;
            substr_width = 0;

            if (maxsplit) {
                if (0 == --maxsplit) {
                    do {
                        ptr += char_size;
                        c = _uw_get_char(ptr, char_size);
                        substr_width = update_char_width(substr_width, c);
                    } while (++i < len);
                    break;
                }
            }
        }
        i++;
        ptr += char_size;
        substr_width = update_char_width(substr_width, c);
    }
    // create final substring
    {
        unsigned substr_len = i - start_i;
        UwValue substr = uw_create_empty_string(substr_len, char_width_to_char_size(substr_width));
        uw_return_if_error(&substr);

        if (substr_len) {
            strmeth->copy_to(start, &substr, 0, substr_len);
            _uw_string_set_length(&substr, substr_len);
        }
        if (!uw_array_append(&result, &substr)) {
            return UwOOM();
        }
    }
    return uw_move(&result);
}

UwResult uw_string_rsplit_chr(UwValuePtr str, char32_t splitter, unsigned maxsplit)
{
    uw_assert_string(str);
    StrMethods* strmeth = get_str_methods(str);

    unsigned len;
    uint8_t* start = _uw_string_start_length(str, &len);
    uint8_t char_size = _uw_string_char_size(str);

    UwValue result = UwArray();
    if (uw_error(&result) || len == 0) {
        return uw_move(&result);
    }

    unsigned i = len - 1;
    unsigned end_i = i;
    uint8_t substr_width = 0;

    start += i * char_size;
    while (i) {
        char32_t c = _uw_get_char(start, char_size);
        if (c == splitter) {
            // create substring
            unsigned substr_len = end_i - i;
            UwValue substr = uw_create_empty_string(substr_len, char_width_to_char_size(substr_width));
            uw_return_if_error(&substr);

            if (substr_len) {
                strmeth->copy_to(start + char_size, &substr, 0, substr_len);
                _uw_string_set_length(&substr, substr_len);
            }
            if (!uw_array_insert(&result, 0, &substr)) {
                return UwOOM();
            }

            end_i = i - 1;
            substr_width = 0;

            if (maxsplit) {
                if (0 == --maxsplit) {
                    do {
                        i--;
                        start -= char_size;
                        c = _uw_get_char(start, char_size);
                        substr_width = update_char_width(substr_width, c);
                    } while (i);
                    break;
                }
            }
        }
        i--;
        start -= char_size;
        substr_width = update_char_width(substr_width, c);
    }
    // create final substring
    {
        unsigned substr_len = end_i + 1;
        UwValue substr = uw_create_empty_string(substr_len, char_width_to_char_size(substr_width));
        uw_return_if_error(&substr);

        if (substr_len) {
            strmeth->copy_to(start, &substr, 0, substr_len);
            _uw_string_set_length(&substr, substr_len);
        }
        if (!uw_array_insert(&result, 0, &substr)) {
            return UwOOM();
        }
    }
    return uw_move(&result);
}

UwResult _uw_strcat_va_v(...)
{
    va_list ap;
    va_start(ap);
    UwValue result = _uw_strcat_ap_v(ap);
    va_end(ap);
    return uw_move(&result);
}

UwResult _uw_strcat_va_p(...)
{
    va_list ap;
    va_start(ap);
    UwValue result = _uw_strcat_ap_p(ap);
    va_end(ap);
    return uw_move(&result);
}

UwResult _uw_strcat_ap_v(va_list ap)
{
    // default error is OOM unless some arg is a status
    UwValue error = UwOOM();

    // count the number of args, check their types,
    // calculate total length and max char width
    unsigned result_len = 0;
    uint8_t max_char_size = 1;
    va_list temp_ap;
    va_copy(temp_ap, ap);
    for (unsigned arg_no = 0;;) {
        // arg is not auto-cleaned here because we don't consume it yet
        _UwValue arg = va_arg(temp_ap, _UwValue);
        arg_no++;
        if (uw_is_status(&arg)) {
            if (uw_va_end(&arg)) {
                break;
            }
            uw_destroy(&error);
            error = uw_clone(&arg);
            va_end(temp_ap);
            _uw_destroy_args(ap);
            return uw_move(&error);
        }

        uint8_t char_size;
        if (uw_is_string(&arg)) {
            result_len += _uw_string_length(&arg);
            char_size = _uw_string_char_size(&arg);

        } else if (uw_is_charptr(&arg)) {
            result_len += _uw_charptr_strlen2(&arg, &char_size);

        } else {
            // XXX to_string?
            uw_destroy(&error);
            error = UwError(UW_ERROR_INCOMPATIBLE_TYPE);
            _uw_set_status_desc(&error, "Bad argument %u type for uw_strcat: %u, %s",
                                arg_no, arg.type_id, uw_get_type_name(arg.type_id));
            va_end(temp_ap);
            _uw_destroy_args(ap);
            return uw_move(&error);
        }
        if (max_char_size < char_size) {
            max_char_size = char_size;
        }
    }
    va_end(temp_ap);

    if (result_len == 0) {
        return UwString();
    }

    // allocate resulting string
    UwValue str = uw_create_empty_string(result_len, max_char_size);
    uw_return_if_error(&str);

    // concatenate
    for (;;) {{
        UwValue arg = va_arg(ap, _UwValue);
        if (uw_va_end(&arg)) {
            return uw_move(&str);
        }
        if (!_uw_string_append(&str, &arg)) {
            break;
        }
    }}
    _uw_destroy_args(ap);
    return uw_move(&error);
}

UwResult _uw_strcat_ap_p(va_list ap)
{
    // count the number of args, check their types,
    // calculate total length and max char width
    unsigned result_len = 0;
    uint8_t max_char_size = 1;
    va_list temp_ap;
    va_copy(temp_ap, ap);
    for (unsigned arg_no = 0;;) {
        UwValuePtr arg = va_arg(temp_ap, UwValuePtr);
        if (!arg) {
            break;
        }
        arg_no++;

        uint8_t char_size;
        if (uw_is_string(arg)) {
            result_len += _uw_string_length(arg);
            char_size = _uw_string_char_size(arg);

        } else if (uw_is_charptr(arg)) {
            result_len += _uw_charptr_strlen2(arg, &char_size);

        } else {
            UwValue error = UwError(UW_ERROR_INCOMPATIBLE_TYPE);
            _uw_set_status_desc(&error, "Bad argument %u type for uw_strcat: %u, %s",
                                arg_no, arg->type_id, uw_get_type_name(arg->type_id));
            va_end(temp_ap);
            return uw_move(&error);
        }
        if (max_char_size < char_size) {
            max_char_size = char_size;
        }
    }
    va_end(temp_ap);

    if (result_len == 0) {
        return UwString();
    }

    // allocate resulting string
    UwValue str = uw_create_empty_string(result_len, max_char_size);
    uw_return_if_error(&str);

    // concatenate
    for (;;) {{
        UwValuePtr arg = va_arg(ap, UwValuePtr);
        if (!arg) {
            return uw_move(&str);
        }
        if (!_uw_string_append(&str, arg)) {
            break;
        }
    }}
    return UwOOM();
}

unsigned uw_string_skip_spaces(UwValuePtr str, unsigned position)
{
    uw_assert_string(str);
    uint8_t char_size = _uw_string_char_size(str);
    unsigned length;
    uint8_t* ptr = _uw_string_start_length(str, &length);
    ptr += position * char_size;
    while (position < length) {
        char32_t c = _uw_get_char(ptr, char_size);
        if (!uw_isspace(c)) {
            return position;
        }
        position++;
        ptr += char_size;
    }
    return length;
}

unsigned uw_string_skip_chars(UwValuePtr str, unsigned position, char32_t* skipchars)
{
    uw_assert_string(str);
    uint8_t char_size = _uw_string_char_size(str);
    unsigned length;
    uint8_t* ptr = _uw_string_start_length(str, &length);
    ptr += position * char_size;
    while (position < length) {
        char32_t c = _uw_get_char(ptr, char_size);
        if (!u32_strchr(skipchars, c)) {
            return position;
        }
        position++;
        ptr += char_size;
    }
    return length;
}

UwResult uw_string_to_int(UwValuePtr str)
{
    uw_expect(string, str);
    UW_CSTRING_LOCAL(s, str);
    UwValue result = UwNull();
    errno = 0;
    if (_uw_char_at(str, uw_string_skip_spaces(str, 0)) == '-') {
        result = UwSigned(strtol(s, nullptr, 0));
    } else {
        result = UwUnsigned(strtoul(s, nullptr, 0));
    }
    if (errno) {
        result = UwErrno(errno);
    }
    return uw_move(&result);
}

UwResult uw_string_to_float(UwValuePtr str)
{
    uw_expect(string, str);
    UW_CSTRING_LOCAL(s, str);
    errno = 0;
    UwValue result = UwFloat(strtod(s, nullptr));
    if (errno) {
        result = UwErrno(errno);
    }
    return uw_move(&result);
}

bool uw_string_isdigit(UwValuePtr str)
{
    uw_assert_string(str);
    unsigned length;
    uint8_t* ptr = _uw_string_start_length(str, &length);
    if (length == 0) {
        return false;
    }
    uint8_t char_size = _uw_string_char_size(str);
    for (unsigned i = 0; i < length; i++) {
        char32_t c = _uw_get_char(ptr, char_size);
        if (!uw_isdigit(c)) {
            return false;
        }
        ptr += char_size;
    }
    return true;
}
