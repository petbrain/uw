#include <string.h>

#include "include/uw_base.h"
#include "include/uw_string.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_string_internal.h"

[[noreturn]]
static void panic_bad_charptr_subtype(UwValuePtr v)
{
    uw_dump(stderr, v);
    uw_panic("Bad charptr subtype %u\n", v->charptr_subtype);
}

static UwResult charptr_create(UwTypeId type_id, void* ctor_args)
{
    // XXX use ctor_args for initializer?

    _UwValue result = {
        ._charptr_type_id = UwTypeId_CharPtr,
        .charptr_subtype = UW_CHARPTR,
        .charptr = ""
    };
    return result;
}

static void charptr_hash(UwValuePtr self, UwHashContext* ctx)
{
    /*
     * To be able to use CharPtr as arguments to map functions,
     * make hashes same as for UW string.
     */
    _uw_hash_uint64(ctx, UwTypeId_String);

    switch (self->charptr_subtype) {
        case UW_CHARPTR:
            if (self->charptr) {
                unsigned char* ptr = (unsigned char*) self->charptr;
                for (;;) {
                    union {
                        struct {
                            char32_t a;
                            char32_t b;
                        };
                        uint64_t i64;
                    } data;
                    data.a = *ptr++;
                    if (data.a == 0) {
                        break;
                    }
                    data.b = *ptr++;
                    _uw_hash_uint64(ctx, data.i64);
                    if (data.b == 0) {
                        break;
                    }
                }
            }
            break;

        case UW_CHAR8PTR:
            if (self->char8ptr) {
                char8_t* ptr = self->char8ptr;
                for (;;) {
                    union {
                        struct {
                            char32_t a;
                            char32_t b;
                        };
                        uint64_t i64;
                    } data;
                    data.a = read_utf8_char(&ptr);
                    if (data.a == 0) {
                        break;
                    }
                    data.b = read_utf8_char(&ptr);
                    _uw_hash_uint64(ctx, data.i64);
                    if (data.b == 0) {
                        break;
                    }
                }
            }
            break;

        case UW_CHAR32PTR:
            if (self->char32ptr) {
                char32_t* ptr = self->char32ptr;
                for (;;) {
                    union {
                        struct {
                            char32_t a;
                            char32_t b;
                        };
                        uint64_t i64;
                    } data;
                    data.a = *ptr++;
                    if (data.a == 0) {
                        break;
                    }
                    data.b = *ptr++;
                    _uw_hash_uint64(ctx, data.i64);
                    if (data.b == 0) {
                        break;
                    }
                }
            }
            break;

        default:
            panic_bad_charptr_subtype(self);
    }
}

static void charptr_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    fputc('\n', fp);
    _uw_print_indent(fp, next_indent);

    switch (self->charptr_subtype) {
        case UW_CHARPTR: {
            fputs("char*: ", fp);
            char* ptr = self->charptr;
            for (unsigned i = 0;; i++) {
                char c = *ptr++;
                if (c == 0) {
                    break;
                }
                fputc(c, fp);
                if (i == 80) {
                    fprintf(fp, "...");
                    break;
                }
            }
            break;
        }
        case UW_CHAR8PTR: {
            fputs("char8_t*: ", fp);
            char8_t* ptr = self->char8ptr;
            for (unsigned i = 0;; i++) {
                char32_t c = read_utf8_char(&ptr);
                if (c == 0) {
                    break;
                }
                _uw_putchar32_utf8(fp, c);
                if (i == 80) {
                    fprintf(fp, "...");
                    break;
                }
            }
            break;
        }
        case UW_CHAR32PTR: {
            fputs("char32_t*: ", fp);
            char32_t* ptr = self->char32ptr;
            for (unsigned i = 0;; i++) {
                char32_t c = *ptr++;
                if (c == 0) {
                    break;
                }
                _uw_putchar32_utf8(fp, c);
                if (i == 80) {
                    fprintf(fp, "...");
                    break;
                }
            }
            break;
        }
        default:
            panic_bad_charptr_subtype(self);
    }
    fputc('\n', fp);
}

UwResult uw_charptr_to_string(UwValuePtr self)
{
    switch (self->charptr_subtype) {
        case UW_CHARPTR:   return  uw_create_string_cstr(self->charptr);
        case UW_CHAR8PTR:  return _uw_create_string_u8  (self->char8ptr);
        case UW_CHAR32PTR: return _uw_create_string_u32 (self->char32ptr);
        default:
            panic_bad_charptr_subtype(self);
    }
}

static bool charptr_is_true(UwValuePtr self)
{
    switch (self->charptr_subtype) {
        case UW_CHARPTR:   return self->charptr != nullptr && *self->charptr;
        case UW_CHAR8PTR:  return self->char8ptr != nullptr && *self->char8ptr;
        case UW_CHAR32PTR: return self->char32ptr != nullptr && *self->char32ptr;
        default:
            panic_bad_charptr_subtype(self);
    }
}

static bool charptr_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    switch (self->charptr_subtype) {
        case UW_CHARPTR:
            switch (other->charptr_subtype) {
                case UW_CHARPTR:
                    if (self->charptr == nullptr) {
                        return other->charptr == nullptr;
                    } else {
                        return strcmp(self->charptr, other->charptr) == 0;
                    }
                case UW_CHAR8PTR:
                    if (self->charptr == nullptr) {
                        return other->char8ptr == nullptr;
                    } else {
                        return strcmp(self->charptr, (char*) other->char8ptr) == 0;
                    }
                case UW_CHAR32PTR:
                    if (self->charptr == nullptr) {
                        return other->char32ptr == nullptr;
                    } else {
                        return u32_strcmp_cstr(other->char32ptr, self->charptr) == 0;
                    }
                default:
                    panic_bad_charptr_subtype(other);;
            }

        case UW_CHAR8PTR:
            switch (other->charptr_subtype) {
                case UW_CHARPTR:
                    if (self->char8ptr == nullptr) {
                        return other->charptr == nullptr;
                    } else {
                        return strcmp((char*) self->char8ptr, other->charptr) == 0;
                    }
                case UW_CHAR8PTR:
                    if (self->char8ptr == nullptr) {
                        return other->char8ptr == nullptr;
                    } else {
                        return strcmp((char*) self->char8ptr, (char*) other->char8ptr) == 0;
                    }
                case UW_CHAR32PTR:
                    if (self->charptr == nullptr) {
                        return other->char32ptr == nullptr;
                    } else {
                        return u32_strcmp_u8(other->char32ptr, self->char8ptr) == 0;
                    }
                default:
                    panic_bad_charptr_subtype(other);
            }

        case UW_CHAR32PTR:
            switch (other->charptr_subtype) {
                case UW_CHARPTR:
                    if (self->char32ptr == nullptr) {
                        return other->charptr == nullptr;
                    } else {
                        return u32_strcmp_cstr(self->char32ptr, other->charptr) == 0;
                    }
                case UW_CHAR8PTR:
                    if (self->char32ptr == nullptr) {
                        return other->char8ptr == nullptr;
                    } else {
                        return u32_strcmp_u8(self->char32ptr, other->char8ptr) == 0;
                    }
                case UW_CHAR32PTR:
                    if (self->char32ptr == nullptr) {
                        return other->char32ptr == nullptr;
                    } else {
                        return u32_strcmp(self->char32ptr, other->char32ptr) == 0;
                    }
                default:
                    panic_bad_charptr_subtype(other);
            }

        default:
            panic_bad_charptr_subtype(self);
    }
}

static bool charptr_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        switch (t) {
            case UwTypeId_Null:
                return self->charptr == nullptr;

            case UwTypeId_Ptr:
                return self->charptr == other->ptr;

            case UwTypeId_String:
                return _uw_charptr_equal_string(self, other);

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

UwType _uw_charptr_type = {
    .id             = UwTypeId_CharPtr,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "CharPtr",
    .allocator      = nullptr,
    .create         = charptr_create,
    .destroy        = nullptr,
    .clone          = uw_charptr_to_string,
    .hash           = charptr_hash,
    .deepcopy       = uw_charptr_to_string,
    .dump           = charptr_dump,
    .to_string      = uw_charptr_to_string,
    .is_true        = charptr_is_true,
    .equal_sametype = charptr_equal_sametype,
    .equal          = charptr_equal,
};

bool uw_charptr_to_string_inplace(UwValuePtr v)
{
    if (!uw_is_charptr(v)) {
        return true;
    }
    UwValue result = uw_charptr_to_string(v);
    if (uw_ok(&result)) {
        *v = uw_move(&result);
        return true;
    } else {
        return false;
    }
}

bool _uw_charptr_equal_string(UwValuePtr charptr, UwValuePtr str)
{
    switch (charptr->charptr_subtype) {
        case UW_CHARPTR:
            if (charptr->charptr == nullptr) {
                return false;
            } else {
                return uw_equal_cstr(str, charptr->charptr);
            }
        case UW_CHAR8PTR:
            if (charptr->char8ptr == nullptr) {
                return false;
            } else {
                return _uw_equal_u8(str, charptr->char8ptr);
            }
        case UW_CHAR32PTR:
            if (charptr->char32ptr == nullptr) {
                return false;
            } else {
                return _uw_equal_u32(str, charptr->char32ptr);
            }
        default:
            panic_bad_charptr_subtype(charptr);
    }
}

unsigned _uw_charptr_strlen2(UwValuePtr charptr, uint8_t* char_size)
{
    switch (charptr->charptr_subtype) {
        case UW_CHARPTR:   *char_size = 1; return strlen(charptr->charptr);
        case UW_CHAR8PTR:  return utf8_strlen2(charptr->char8ptr, char_size);
        case UW_CHAR32PTR: return u32_strlen2(charptr->char32ptr, char_size);
        default:
            panic_bad_charptr_subtype(charptr);
    }
}
