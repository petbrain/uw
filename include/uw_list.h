#pragma once

#include <stdarg.h>
#include <uchar.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UwList(...)  _uw_list_create(__VA_ARGS__ __VA_OPT__(,) UwVaEnd())
extern UwResult _uw_list_create(...);
/*
 * List constructor arguments are list items.
 */

/****************************************************************
 * Append and insert functions
 */

#define uw_list_append(list, item) _Generic((item), \
             nullptr_t: _uw_list_append_null,       \
                  bool: _uw_list_append_bool,       \
                  char: _uw_list_append_signed,     \
         unsigned char: _uw_list_append_unsigned,   \
                 short: _uw_list_append_signed,     \
        unsigned short: _uw_list_append_unsigned,   \
                   int: _uw_list_append_signed,     \
          unsigned int: _uw_list_append_unsigned,   \
                  long: _uw_list_append_signed,     \
         unsigned long: _uw_list_append_unsigned,   \
             long long: _uw_list_append_signed,     \
    unsigned long long: _uw_list_append_unsigned,   \
                 float: _uw_list_append_float,      \
                double: _uw_list_append_float,      \
                 char*: _uw_list_append_u8_wrapper, \
              char8_t*: _uw_list_append_u8,         \
             char32_t*: _uw_list_append_u32,        \
            UwValuePtr: _uw_list_append             \
    )((list), (item))

bool _uw_list_append(UwValuePtr list, UwValuePtr item);
/*
 * The basic append function.
 *
 * `item` is cloned before appending. CharPtr values are converted to UW strings.
 */

static inline bool _uw_list_append_null    (UwValuePtr list, UwType_Null     item) { __UWDECL_Null     (v);       return _uw_list_append(list, &v); }
static inline bool _uw_list_append_bool    (UwValuePtr list, UwType_Bool     item) { __UWDECL_Bool     (v, item); return _uw_list_append(list, &v); }
static inline bool _uw_list_append_signed  (UwValuePtr list, UwType_Signed   item) { __UWDECL_Signed   (v, item); return _uw_list_append(list, &v); }
static inline bool _uw_list_append_unsigned(UwValuePtr list, UwType_Unsigned item) { __UWDECL_Unsigned (v, item); return _uw_list_append(list, &v); }
static inline bool _uw_list_append_float   (UwValuePtr list, UwType_Float    item) { __UWDECL_Float    (v, item); return _uw_list_append(list, &v); }
static inline bool _uw_list_append_u8      (UwValuePtr list, char8_t*        item) { __UWDECL_Char8Ptr (v, item); return _uw_list_append(list, &v); }
static inline bool _uw_list_append_u32     (UwValuePtr list, char32_t*       item) { __UWDECL_Char32Ptr(v, item); return _uw_list_append(list, &v); }

static inline bool _uw_list_append_u8_wrapper(UwValuePtr list, char* item)
{
    return _uw_list_append_u8(list, (char8_t*) item);
}

UwResult _uw_list_append_va(UwValuePtr list, ...);
/*
 * Variadic functions accept values, not pointers.
 * This encourages use cases when values are created during the call.
 * If an error is occured, a Status value is pushed on stack.
 * As long as statuses are prohibited, the function returns the first
 * status encountered and destroys all passed arguments.
 *
 * CAVEAT: DO NOT PASS LOCAL VARIABLES BY VALUES!
 */

#define uw_list_append_va(list, ...)  \
    _uw_list_append_va(list __VA_OPT__(,) __VA_ARGS__, UwVaEnd())

UwResult uw_list_append_ap(UwValuePtr list, va_list ap);
/*
 * Append items to the `list`.
 * Item are cloned before appending. CharPtr values are converted to UW strings.
 */

#define uw_list_insert(list, index, item) _Generic((item), \
             nullptr_t: _uw_list_insert_null,       \
                  bool: _uw_list_insert_bool,       \
                  char: _uw_list_insert_signed,     \
         unsigned char: _uw_list_insert_unsigned,   \
                 short: _uw_list_insert_signed,     \
        unsigned short: _uw_list_insert_unsigned,   \
                   int: _uw_list_insert_signed,     \
          unsigned int: _uw_list_insert_unsigned,   \
                  long: _uw_list_insert_signed,     \
         unsigned long: _uw_list_insert_unsigned,   \
             long long: _uw_list_insert_signed,     \
    unsigned long long: _uw_list_insert_unsigned,   \
                 float: _uw_list_insert_float,      \
                double: _uw_list_insert_float,      \
                 char*: _uw_list_insert_u8_wrapper, \
              char8_t*: _uw_list_insert_u8,         \
             char32_t*: _uw_list_insert_u32,        \
            UwValuePtr: _uw_list_insert             \
    )((list), (index), (item))

bool _uw_list_insert(UwValuePtr list, unsigned index, UwValuePtr item);
/*
 * The basic insert function.
 *
 * `item` is cloned before inserting. CharPtr values are converted to UW strings.
 */

static inline bool _uw_list_insert_null    (UwValuePtr list, unsigned index, UwType_Null     item) { __UWDECL_Null     (v);       return _uw_list_insert(list, index, &v); }
static inline bool _uw_list_insert_bool    (UwValuePtr list, unsigned index, UwType_Bool     item) { __UWDECL_Bool     (v, item); return _uw_list_insert(list, index, &v); }
static inline bool _uw_list_insert_signed  (UwValuePtr list, unsigned index, UwType_Signed   item) { __UWDECL_Signed   (v, item); return _uw_list_insert(list, index, &v); }
static inline bool _uw_list_insert_unsigned(UwValuePtr list, unsigned index, UwType_Unsigned item) { __UWDECL_Unsigned (v, item); return _uw_list_insert(list, index, &v); }
static inline bool _uw_list_insert_float   (UwValuePtr list, unsigned index, UwType_Float    item) { __UWDECL_Float    (v, item); return _uw_list_insert(list, index, &v); }
static inline bool _uw_list_insert_u8      (UwValuePtr list, unsigned index, char8_t*        item) { __UWDECL_Char8Ptr (v, item); return _uw_list_insert(list, index, &v); }
static inline bool _uw_list_insert_u32     (UwValuePtr list, unsigned index, char32_t*       item) { __UWDECL_Char32Ptr(v, item); return _uw_list_insert(list, index, &v); }

static inline bool _uw_list_insert_u8_wrapper(UwValuePtr list, unsigned index, char* item)
{
    return _uw_list_insert_u8(list, index, (char8_t*) item);
}


/****************************************************************
 * Join list items. Return string value.
 */

#define uw_list_join(separator, list) _Generic((separator), \
              char32_t: _uw_list_join_c32,        \
                   int: _uw_list_join_c32,        \
                 char*: _uw_list_join_u8_wrapper, \
              char8_t*: _uw_list_join_u8,         \
             char32_t*: _uw_list_join_u32,        \
            UwValuePtr: _uw_list_join             \
    )((separator), (list))

UwResult _uw_list_join_c32(char32_t   separator, UwValuePtr list);
UwResult _uw_list_join_u8 (char8_t*   separator, UwValuePtr list);
UwResult _uw_list_join_u32(char32_t*  separator, UwValuePtr list);
UwResult _uw_list_join    (UwValuePtr separator, UwValuePtr list);

static inline UwResult _uw_list_join_u8_wrapper(char* separator, UwValuePtr list)
{
    return _uw_list_join_u8((char8_t*) separator, list);
}

/****************************************************************
 * Miscellaneous list functions
 */

bool uw_list_resize(UwValuePtr list, unsigned desired_capacity);

unsigned uw_list_length(UwValuePtr list);

UwResult uw_list_item(UwValuePtr list, int index);
/*
 * Return a clone of list item. Negative indexes are allowed: -1 last item.
 * It's the caller's responsibility to destroy returned value to avoid memory leaks.
 * The simplest way is assigning it to an UwValue.
 */

UwResult uw_list_set_item(UwValuePtr list, int index, UwValuePtr item);
/*
 * Set item at specific index.
 * Return UwStatus.
 */

UwResult uw_list_pop(UwValuePtr list);
/*
 * Extract last item from the list.
 * It's the caller's responsibility to destroy returned value to avoid memory leaks.
 * The simplest way is assigning it to an UwValue.
 */

void uw_list_del(UwValuePtr list, unsigned start_index, unsigned end_index);
/*
 * Delete items from list.
 * `end_index` is exclusive, i.e. the number of items to delete equals to end_index - start_index..
 */

void uw_list_clean(UwValuePtr list);
/*
 * Delete all items from list.
 */

UwResult uw_list_slice(UwValuePtr list, unsigned start_index, unsigned end_index);
/*
 * Return shallow copy of the given range of list.
 */

bool uw_list_dedent(UwValuePtr lines);
/*
 * Dedent list of strings.
 * XXX count treat tabs as single spaces.
 *
 * Return true on success, false if OOM.
 */

#ifdef __cplusplus
}
#endif
