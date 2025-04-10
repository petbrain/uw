#pragma once

#include <stdarg.h>
#include <uchar.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UwArray(...)  _uw_array_create(__VA_ARGS__ __VA_OPT__(,) UwVaEnd())
extern UwResult _uw_array_create(...);
/*
 * Array constructor arguments are array items.
 */

/****************************************************************
 * Append and insert functions
 */

#define uw_array_append(array, item) _Generic((item), \
             nullptr_t: _uw_array_append_null,       \
                  bool: _uw_array_append_bool,       \
                  char: _uw_array_append_signed,     \
         unsigned char: _uw_array_append_unsigned,   \
                 short: _uw_array_append_signed,     \
        unsigned short: _uw_array_append_unsigned,   \
                   int: _uw_array_append_signed,     \
          unsigned int: _uw_array_append_unsigned,   \
                  long: _uw_array_append_signed,     \
         unsigned long: _uw_array_append_unsigned,   \
             long long: _uw_array_append_signed,     \
    unsigned long long: _uw_array_append_unsigned,   \
                 float: _uw_array_append_float,      \
                double: _uw_array_append_float,      \
                 char*: _uw_array_append_u8_wrapper, \
              char8_t*: _uw_array_append_u8,         \
             char32_t*: _uw_array_append_u32,        \
            UwValuePtr: _uw_array_append             \
    )((array), (item))

bool _uw_array_append(UwValuePtr array, UwValuePtr item);
/*
 * The basic append function.
 *
 * `item` is cloned before appending. CharPtr values are converted to UW strings.
 */

static inline bool _uw_array_append_null    (UwValuePtr array, UwType_Null     item) { __UWDECL_Null     (v);       return _uw_array_append(array, &v); }
static inline bool _uw_array_append_bool    (UwValuePtr array, UwType_Bool     item) { __UWDECL_Bool     (v, item); return _uw_array_append(array, &v); }
static inline bool _uw_array_append_signed  (UwValuePtr array, UwType_Signed   item) { __UWDECL_Signed   (v, item); return _uw_array_append(array, &v); }
static inline bool _uw_array_append_unsigned(UwValuePtr array, UwType_Unsigned item) { __UWDECL_Unsigned (v, item); return _uw_array_append(array, &v); }
static inline bool _uw_array_append_float   (UwValuePtr array, UwType_Float    item) { __UWDECL_Float    (v, item); return _uw_array_append(array, &v); }
static inline bool _uw_array_append_u8      (UwValuePtr array, char8_t*        item) { __UWDECL_CharPtr  (v, item); return _uw_array_append(array, &v); }
static inline bool _uw_array_append_u32     (UwValuePtr array, char32_t*       item) { __UWDECL_Char32Ptr(v, item); return _uw_array_append(array, &v); }

static inline bool _uw_array_append_u8_wrapper(UwValuePtr array, char* item)
{
    return _uw_array_append_u8(array, (char8_t*) item);
}

UwResult _uw_array_append_va(UwValuePtr array, ...);
/*
 * Variadic functions accept values, not pointers.
 * This encourages use cases when values are created during the call.
 * If an error is occured, a Status value is pushed on stack.
 * As long as statuses are prohibited, the function returns the first
 * status encountered and destroys all passed arguments.
 *
 * CAVEAT: DO NOT PASS LOCAL VARIABLES BY VALUES!
 */

#define uw_array_append_va(array, ...)  \
    _uw_array_append_va(array __VA_OPT__(,) __VA_ARGS__, UwVaEnd())

UwResult uw_array_append_ap(UwValuePtr array, va_list ap);
/*
 * Append items to the `array`.
 * Item are cloned before appending. CharPtr values are converted to UW strings.
 */

#define uw_array_insert(array, index, item) _Generic((item), \
             nullptr_t: _uw_array_insert_null,       \
                  bool: _uw_array_insert_bool,       \
                  char: _uw_array_insert_signed,     \
         unsigned char: _uw_array_insert_unsigned,   \
                 short: _uw_array_insert_signed,     \
        unsigned short: _uw_array_insert_unsigned,   \
                   int: _uw_array_insert_signed,     \
          unsigned int: _uw_array_insert_unsigned,   \
                  long: _uw_array_insert_signed,     \
         unsigned long: _uw_array_insert_unsigned,   \
             long long: _uw_array_insert_signed,     \
    unsigned long long: _uw_array_insert_unsigned,   \
                 float: _uw_array_insert_float,      \
                double: _uw_array_insert_float,      \
                 char*: _uw_array_insert_u8_wrapper, \
              char8_t*: _uw_array_insert_u8,         \
             char32_t*: _uw_array_insert_u32,        \
            UwValuePtr: _uw_array_insert             \
    )((array), (index), (item))

bool _uw_array_insert(UwValuePtr array, unsigned index, UwValuePtr item);
/*
 * The basic insert function.
 *
 * `item` is cloned before inserting. CharPtr values are converted to UW strings.
 */

static inline bool _uw_array_insert_null    (UwValuePtr array, unsigned index, UwType_Null     item) { __UWDECL_Null     (v);       return _uw_array_insert(array, index, &v); }
static inline bool _uw_array_insert_bool    (UwValuePtr array, unsigned index, UwType_Bool     item) { __UWDECL_Bool     (v, item); return _uw_array_insert(array, index, &v); }
static inline bool _uw_array_insert_signed  (UwValuePtr array, unsigned index, UwType_Signed   item) { __UWDECL_Signed   (v, item); return _uw_array_insert(array, index, &v); }
static inline bool _uw_array_insert_unsigned(UwValuePtr array, unsigned index, UwType_Unsigned item) { __UWDECL_Unsigned (v, item); return _uw_array_insert(array, index, &v); }
static inline bool _uw_array_insert_float   (UwValuePtr array, unsigned index, UwType_Float    item) { __UWDECL_Float    (v, item); return _uw_array_insert(array, index, &v); }
static inline bool _uw_array_insert_u8      (UwValuePtr array, unsigned index, char8_t*        item) { __UWDECL_CharPtr  (v, item); return _uw_array_insert(array, index, &v); }
static inline bool _uw_array_insert_u32     (UwValuePtr array, unsigned index, char32_t*       item) { __UWDECL_Char32Ptr(v, item); return _uw_array_insert(array, index, &v); }

static inline bool _uw_array_insert_u8_wrapper(UwValuePtr array, unsigned index, char* item)
{
    return _uw_array_insert_u8(array, index, (char8_t*) item);
}


/****************************************************************
 * Join array items. Return string value.
 */

#define uw_array_join(separator, array) _Generic((separator), \
              char32_t: _uw_array_join_c32,        \
                   int: _uw_array_join_c32,        \
                 char*: _uw_array_join_u8_wrapper, \
              char8_t*: _uw_array_join_u8,         \
             char32_t*: _uw_array_join_u32,        \
            UwValuePtr: _uw_array_join             \
    )((separator), (array))

UwResult _uw_array_join_c32(char32_t   separator, UwValuePtr array);
UwResult _uw_array_join_u8 (char8_t*   separator, UwValuePtr array);
UwResult _uw_array_join_u32(char32_t*  separator, UwValuePtr array);
UwResult _uw_array_join    (UwValuePtr separator, UwValuePtr array);

static inline UwResult _uw_array_join_u8_wrapper(char* separator, UwValuePtr array)
{
    return _uw_array_join_u8((char8_t*) separator, array);
}

/****************************************************************
 * Get/set array items
 */

UwResult uw_array_pull(UwValuePtr array);
/*
 * Extract first item from the array.
 */

UwResult uw_array_pop(UwValuePtr array);
/*
 * Extract last item from the array.
 */

#define uw_array_item(array, index) _Generic((index), \
             int: _uw_array_item_signed,  \
         ssize_t: _uw_array_item_signed,  \
        unsigned: _uw_array_item  \
    )((array), (index))

UwResult _uw_array_item_signed(UwValuePtr array, ssize_t index);
UwResult _uw_array_item(UwValuePtr array, unsigned index);
/*
 * Return a clone of array item.
 * Negative indexes are allowed for signed version,
 * where -1 is the index of last item.
 */

#define uw_array_set_item(array, index, item) _Generic((index), \
             int: _uw_array_set_item_signed,  \
         ssize_t: _uw_array_set_item_signed,  \
        unsigned: _uw_array_set_item  \
    )((array), (index), (item))

UwResult _uw_array_set_item_signed(UwValuePtr array, ssize_t index, UwValuePtr item);
UwResult _uw_array_set_item(UwValuePtr array, unsigned index, UwValuePtr item);
/*
 * Set item at specific index.
 * Negative indexes are allowed for signed version,
 * where -1 is the index of last item.
 * Return UwStatus.
 */


/****************************************************************
 * Miscellaneous array functions
 */

bool uw_array_resize(UwValuePtr array, unsigned desired_capacity);

unsigned uw_array_length(UwValuePtr array);

void uw_array_del(UwValuePtr array, unsigned start_index, unsigned end_index);
/*
 * Delete items from array.
 * `end_index` is exclusive, i.e. the number of items to delete equals to end_index - start_index..
 */

void uw_array_clean(UwValuePtr array);
/*
 * Delete all items from array.
 */

UwResult uw_array_slice(UwValuePtr array, unsigned start_index, unsigned end_index);
/*
 * Return shallow copy of the given range of array.
 */

bool uw_array_dedent(UwValuePtr lines);
/*
 * Dedent array of strings.
 * XXX count treat tabs as single spaces.
 *
 * Return true on success, false if OOM.
 */

#ifdef __cplusplus
}
#endif
