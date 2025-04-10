#include <string.h>

#include "include/uw_to_json.h"

#include "src/uw_charptr_internal.h"
#include "src/uw_string_internal.h"

// forward declarations
static unsigned estimate_length(UwValuePtr value, unsigned indent, unsigned depth, uint8_t* max_char_size);
static bool value_to_json(UwValuePtr value, unsigned indent, unsigned depth, UwValuePtr result);


static unsigned estimate_escaped_length(UwValuePtr str, uint8_t* char_size)
/*
 * Estimate length of escaped string.
 * Only double quotes and characters with codes < 32 to escape.
 */
{
#   define INCREMENT_LENGTH  \
        if (c < 32) {  \
            if (c == '\\' || c == '\b' || c == '\f' ||  \
                c == '\n' || c == '\r' || c == '\t') {  \
                length += 2;  \
            } else {  \
                length += 6;  \
            }  \
        } else {  \
            length++;  \
        }  \
        width = update_char_width(width, c);

    unsigned length = 0;
    uint8_t  width = 0;

    if (uw_is_charptr(str)) {
        switch (str->charptr_subtype) {
            case UW_CHARPTR: {
                char8_t* ptr = str->charptr;
                while(_likely_(*ptr != 0)) {
                    char32_t c = read_utf8_char(&ptr);
                    if (_likely_(c != 0xFFFFFFFF)) {
                        INCREMENT_LENGTH
                    }
                }
                break;
            }
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
        StrMethods* strmeth = get_str_methods(str);

        unsigned n = _uw_string_length(str);
        uint8_t* ptr = _uw_string_char_ptr(str, 0);
        uint8_t chr_sz = _uw_string_char_size(str);

        for (unsigned i = 0; i < n; i++) {
            char32_t c = strmeth->get_char(ptr);
            INCREMENT_LENGTH
            ptr += chr_sz;
        }
    }
    *char_size = char_width_to_char_size(width);
    return length;

#   undef INCREMENT_LENGTH
}

static UwResult escape_string(UwValuePtr str)
/*
 * Escape only double quotes and characters with codes < 32
 */
{
#   define APPEND_ESCAPED_CHAR  \
        if (c < 32) {  \
            if (!uw_string_append(&result, '\\')) {  \
                return UwOOM();  \
            }  \
            bool append_ok = false;  \
            switch (c)  {  \
                case '\\': append_ok = uw_string_append(&result, '\\'); break;  \
                case '\b': append_ok = uw_string_append(&result, 'b'); break;  \
                case '\f': append_ok = uw_string_append(&result, 'f'); break;  \
                case '\n': append_ok = uw_string_append(&result, 'n'); break;  \
                case '\r': append_ok = uw_string_append(&result, 'r'); break;  \
                case '\t': append_ok = uw_string_append(&result, 't'); break;  \
                default:  \
                    if (!uw_string_append(&result, '0')) {  \
                        return UwOOM();  \
                    }  \
                    if (!uw_string_append(&result, '0')) {  \
                        return UwOOM();  \
                    }  \
                    if (!uw_string_append(&result, (c >> 4) + '0')) {  \
                        return UwOOM();  \
                    }  \
                    append_ok = uw_string_append(&result, (c & 15) + '0');  \
            }  \
            if (!append_ok) {  \
                return UwOOM();  \
            }  \
        } else {  \
            if (!uw_string_append(&result, c)) {  \
                return UwOOM();  \
            }  \
        }

    uint8_t char_size;
    unsigned estimated_length = estimate_escaped_length(str, &char_size);

    UwValue result = uw_create_empty_string(estimated_length, char_size);
    uw_return_if_error(&result);

    if (uw_is_charptr(str)) {
        switch (str->charptr_subtype) {
            case UW_CHARPTR: {
                char8_t* ptr = str->charptr;
                while(_likely_(*ptr != 0)) {
                    char32_t c = read_utf8_char(&ptr);
                    if (_likely_(c != 0xFFFFFFFF)) {
                        APPEND_ESCAPED_CHAR
                    }
                }
                break;
            }
            case UW_CHAR32PTR: {
                char32_t* ptr = str->char32ptr;
                for (;;) {
                    char32_t c = *ptr++;
                    if (c == 0) {
                        break;
                    }
                    APPEND_ESCAPED_CHAR
                }
                break;
            }
            default:
                _uw_panic_bad_charptr_subtype(str);
        }
    } else {
        uw_assert_string(str);
        StrMethods* strmeth = get_str_methods(str);

        unsigned length = _uw_string_length(str);
        uint8_t* ptr = _uw_string_char_ptr(str, 0);
        uint8_t char_size = _uw_string_char_size(str);

        for (unsigned i = 0; i < length; i++) {
            char32_t c = strmeth->get_char(ptr);
            APPEND_ESCAPED_CHAR
            ptr += char_size;
        }
    }
    return uw_move(&result);

#   undef APPEND_ESCAPED_CHAR
}

static unsigned estimate_array_length(UwValuePtr value, unsigned indent, unsigned depth, uint8_t* max_char_size)
{
    unsigned length = 2;  // braces
    if (indent) {
        length++; // line break
    }
    unsigned num_items = uw_array_length(value);

    for (unsigned i = 0; i < num_items; i++) {{
        if (i) {
            length += /* comma and space or line break: */ 2;
        }
        if (indent) {
            length += indent * depth;
        }
        UwValue item = uw_array_item(value, i);
        unsigned item_length = estimate_length(&item, indent, depth + 1, max_char_size);
        if (item_length == 0) {
            return 0;
        }
        length += item_length;
    }}
    if (indent) {
        length += /* line break: */ 1 + /* indent before closing brace */ indent * (depth - 1);
    }
    return length;
}

static bool array_to_json(UwValuePtr value, unsigned indent, unsigned depth, UwValuePtr result)
{
    if (!uw_string_append(result, '[')) {
        return false;
    }
    if (indent) {
        if (!uw_string_append(result, '\n')) {
            return false;
        }
    }
    unsigned indent_width = indent * depth;
    char indent_str[indent_width + 1];
    memset(indent_str, ' ', indent_width);
    indent_str[indent_width] = 0;

    unsigned num_items = uw_array_length(value);

    for (unsigned i = 0; i < num_items; i++) {{
        if (i) {
            if (!uw_string_append(result, indent? ",\n" : ", ")) {
                return false;
            }
        }
        if (indent) {
            if (!uw_string_append(result, indent_str)) {
                return false;
            }
        }
        UwValue item = uw_array_item(value, i);
        if (!value_to_json(&item, indent, depth + 1, result)) {
            return false;
        }
    }}
    if (indent) {
        if (!uw_string_append(result, '\n')) {
            return false;
        }
        indent_str[indent * (depth - 1)] = 0;  // dedent closing brace
        if (!uw_string_append(result, indent_str)) {
            return false;
        }
    }
    if (!uw_string_append(result, ']')) {
        return false;
    }
    return true;
}

static unsigned estimate_map_length(UwValuePtr value, unsigned indent, unsigned depth, uint8_t* max_char_size)
{
    unsigned length = 2;  // braces
    if (indent) {
        length++; // line break
    }
    unsigned num_items = uw_map_length(value);

    for (unsigned i = 0; i < num_items; i++) {{
        if (i) {
            length += /* comma and space or line break: */ 2;
        }
        if (indent) {
            length += indent * depth;
        }
        UwValue k = UwNull();
        UwValue v = UwNull();
        uw_map_item(value, i, &k, &v);

        if (!uw_is_string(&k)) {
            // bad type
            return 0;
        }
        unsigned k_length = estimate_length(&k, indent, depth, max_char_size);
        if (k_length == 0) {
            return 0;
        }
        unsigned v_length = estimate_length(&v, indent, depth + 1, max_char_size);
        if (v_length == 0) {
            return 0;
        }
        length += k_length + /* separator and space: */ 2 + v_length;
    }}
    if (indent) {
        length += /* line break: */ 1 + /* indent before closing brace */ indent * (depth - 1);
    }
    return length;
}

static bool map_to_json(UwValuePtr value, unsigned indent, unsigned depth, UwValuePtr result)
{
    if (!uw_string_append(result, '{')) {
        return false;
    }
    if (indent) {
        if (!uw_string_append(result, '\n')) {
            return false;
        }
    }
    unsigned indent_width = indent * depth;
    char indent_str[indent_width + 1];
    memset(indent_str, ' ', indent_width);
    indent_str[indent_width] = 0;

    unsigned num_items = uw_map_length(value);

    for (unsigned i = 0; i < num_items; i++) {{
        UwValue k = UwNull();
        UwValue v = UwNull();
        uw_map_item(value, i, &k, &v);

        if (i) {
            if (!uw_string_append(result, indent? ",\n" : ", ")) {
                return false;
            }
        }
        if (indent) {
            if (!uw_string_append(result, indent_str)) {
                return false;
            }
        }
        if (!uw_string_append(result, '"')) {
            return false;
        }
        UwValue escaped = escape_string(&k);
        if (uw_error(&escaped)) {
            return false;
        }
        if (!uw_string_append(result, &escaped)) {
            return false;
        }
        if (!uw_string_append(result, "\": ")) {
            return false;
        }
        if (!value_to_json(&v, indent, depth + 1, result)) {
            return false;
        }
    }}
    if (indent) {
        if (!uw_string_append(result, '\n')) {
            return false;
        }
        indent_str[indent * (depth - 1)] = 0;  // dedent closing brace
        if (!uw_string_append(result, indent_str)) {
            return false;
        }
    }
    if (!uw_string_append(result, '}')) {
        return false;
    }
    return true;
}

static unsigned estimate_length(UwValuePtr value, unsigned indent, unsigned depth, uint8_t* max_char_size)
/*
 * Estimate length of JSON representation of result.
 * As a side effect detect incompatible values.
 *
 * Return estimated length on success or zero on error.
 */
{
    if (uw_is_null(value)) {
        return 4;

    } else if (uw_is_bool(value)) {
        return (value->bool_value)? 4 : 5;

    } else if (uw_is_int(value)) {
        return 20; // max unsigned: 18446744073709551615

    } else if (uw_is_float(value)) {
        return 16; // no idea how many to reserve, .f conversion may generate very long string

    } else if (uw_is_charptr(value) || uw_is_string(value)) {
        uint8_t char_size;
        unsigned length = estimate_escaped_length(value, &char_size);
        if (*max_char_size < char_size) {
            *max_char_size = char_size;
        }
        return length + /* quotes: */ 2;

    } else if (uw_is_array(value)) {
        return estimate_array_length(value, indent, depth, max_char_size);

    } else if (uw_is_map(value)) {
        return estimate_map_length(value, indent, depth, max_char_size);
    }
    // bad type
    return 0;
}

static bool value_to_json(UwValuePtr value, unsigned indent, unsigned depth, UwValuePtr result)
/*
 * Append serialized value to `result`.
 *
 * Return false if OOM.
 */
{
    if (uw_is_null(value)) {
        return uw_string_append(result, "null");
    }
    if (uw_is_bool(value)) {
        return uw_string_append(result, (value->bool_value)? "true" : "false");
    }
    if (uw_is_signed(value)) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%zd", value->signed_value);
        return uw_string_append(result, buf);
    }
    if (uw_is_unsigned(value)) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%zu", value->unsigned_value);
        return uw_string_append(result, buf);
    }
    if (uw_is_float(value)) {
        char buf[320];
        snprintf(buf, sizeof(buf), "%f", value->float_value);
        return uw_string_append(result, buf);
    }
    if (uw_is_charptr(value) || uw_is_string(value)) {
        UwValue escaped = escape_string(value);
        if (uw_error(&escaped)) {
            return false;
        }
        if (uw_string_append(result, '"')) {
            if (uw_string_append(result, &escaped)) {
                return uw_string_append(result, '"');
            }
        }
        return false;
    }
    if (uw_is_array(value)) {
        return array_to_json(value, indent, depth, result);
    }
    if (uw_is_map(value)) {
        return map_to_json(value, indent, depth, result);
    }
    uw_panic("Incompatible type for JSON");
}

UwResult uw_to_json(UwValuePtr value, unsigned indent)
{
    uint8_t max_char_size = 1;
    unsigned estimated_len = estimate_length(value, indent, 1, &max_char_size);
    if (estimated_len == 0) {
        return UwError(UW_ERROR_INCOMPATIBLE_TYPE);
    }

    UwValue result = uw_create_empty_string(estimated_len, max_char_size);
    uw_return_if_error(&result);

    if (!value_to_json(value, indent, 1, &result)) {
        return UwOOM();
    }
    return uw_move(&result);
}
