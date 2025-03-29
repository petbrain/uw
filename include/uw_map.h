#pragma once

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UwMap(...) _uw_map_create(__VA_ARGS__  __VA_OPT__(,) UwVaEnd())
extern UwResult _uw_map_create(...);
/*
 * Map constructor arguments are key-value pairs.
 */

/****************************************************************
 * Update map: insert key-value pair or replace existing value.
 */

bool uw_map_update(UwValuePtr map, UwValuePtr key, UwValuePtr value);
/*
 * `key` is deeply copied and `value` is cloned before adding.
 * CharPtr values are converted to UW strings.
 */

UwResult _uw_map_update_va(UwValuePtr map, ...);
/*
 * Variadic functions accept values, not pointers.
 * This encourages use cases when values are created during the call.
 * If an error is occured, a Status value is pushed on stack.
 * As long as statuses are prohibited, the function returns the first
 * status encountered and destroys all passed arguments.
 */

#define uw_map_update_va(map, ...)  \
    _uw_map_update_va(map __VA_OPT__(,) __VA_ARGS__, UwVaEnd())

UwResult uw_map_update_ap(UwValuePtr map, va_list ap);

/****************************************************************
 * Check `key` is in `map`.
 */

#define uw_map_has_key(map, key) _Generic((key),    \
             nullptr_t: _uw_map_has_key_null,       \
                  bool: _uw_map_has_key_bool,       \
                  char: _uw_map_has_key_signed,     \
         unsigned char: _uw_map_has_key_unsigned,   \
                 short: _uw_map_has_key_signed,     \
        unsigned short: _uw_map_has_key_unsigned,   \
                   int: _uw_map_has_key_signed,     \
          unsigned int: _uw_map_has_key_unsigned,   \
                  long: _uw_map_has_key_signed,     \
         unsigned long: _uw_map_has_key_unsigned,   \
             long long: _uw_map_has_key_signed,     \
    unsigned long long: _uw_map_has_key_unsigned,   \
                 float: _uw_map_has_key_float,      \
                double: _uw_map_has_key_float,      \
                 char*: _uw_map_has_key_u8_wrapper, \
              char8_t*: _uw_map_has_key_u8,         \
             char32_t*: _uw_map_has_key_u32,        \
            UwValuePtr: _uw_map_has_key             \
    )((map), (key))

bool _uw_map_has_key(UwValuePtr map, UwValuePtr key);

static inline bool _uw_map_has_key_null    (UwValuePtr map, UwType_Null     key) { __UWDECL_Null     (v);      return _uw_map_has_key(map, &v); }
static inline bool _uw_map_has_key_bool    (UwValuePtr map, UwType_Bool     key) { __UWDECL_Bool     (v, key); return _uw_map_has_key(map, &v); }
static inline bool _uw_map_has_key_signed  (UwValuePtr map, UwType_Signed   key) { __UWDECL_Signed   (v, key); return _uw_map_has_key(map, &v); }
static inline bool _uw_map_has_key_unsigned(UwValuePtr map, UwType_Unsigned key) { __UWDECL_Unsigned (v, key); return _uw_map_has_key(map, &v); }
static inline bool _uw_map_has_key_float   (UwValuePtr map, UwType_Float    key) { __UWDECL_Float    (v, key); return _uw_map_has_key(map, &v); }
static inline bool _uw_map_has_key_u8      (UwValuePtr map, char8_t*        key) { __UWDECL_Char8Ptr (v, key); return _uw_map_has_key(map, &v); }
static inline bool _uw_map_has_key_u32     (UwValuePtr map, char32_t*       key) { __UWDECL_Char32Ptr(v, key); return _uw_map_has_key(map, &v); }

static inline bool _uw_map_has_key_u8_wrapper(UwValuePtr map, char* key)
{
    return _uw_map_has_key_u8(map, (char8_t*) key);
}

/****************************************************************
 * Get value by `key`. Return UW_ERROR_NO_KEY if `key`
 * is not in the `map`.
 *
 * It's the caller's responsibility to destroy returned value
 * to avoid memory leaks.
 * The simplest way is assigning it to UwValue.
 */

#define uw_map_get(map, key) _Generic((key),    \
             nullptr_t: _uw_map_get_null,       \
                  bool: _uw_map_get_bool,       \
                  char: _uw_map_get_signed,     \
         unsigned char: _uw_map_get_unsigned,   \
                 short: _uw_map_get_signed,     \
        unsigned short: _uw_map_get_unsigned,   \
                   int: _uw_map_get_signed,     \
          unsigned int: _uw_map_get_unsigned,   \
                  long: _uw_map_get_signed,     \
         unsigned long: _uw_map_get_unsigned,   \
             long long: _uw_map_get_signed,     \
    unsigned long long: _uw_map_get_unsigned,   \
                 float: _uw_map_get_float,      \
                double: _uw_map_get_float,      \
                 char*: _uw_map_get_u8_wrapper, \
              char8_t*: _uw_map_get_u8,         \
             char32_t*: _uw_map_get_u32,        \
            UwValuePtr: _uw_map_get             \
    )((map), (key))

UwResult _uw_map_get(UwValuePtr map, UwValuePtr key);

static inline UwResult _uw_map_get_null    (UwValuePtr map, UwType_Null     key) { __UWDECL_Null     (v);      return _uw_map_get(map, &v); }
static inline UwResult _uw_map_get_bool    (UwValuePtr map, UwType_Bool     key) { __UWDECL_Bool     (v, key); return _uw_map_get(map, &v); }
static inline UwResult _uw_map_get_signed  (UwValuePtr map, UwType_Signed   key) { __UWDECL_Signed   (v, key); return _uw_map_get(map, &v); }
static inline UwResult _uw_map_get_unsigned(UwValuePtr map, UwType_Unsigned key) { __UWDECL_Unsigned (v, key); return _uw_map_get(map, &v); }
static inline UwResult _uw_map_get_float   (UwValuePtr map, UwType_Float    key) { __UWDECL_Float    (v, key); return _uw_map_get(map, &v); }
static inline UwResult _uw_map_get_u8      (UwValuePtr map, char8_t*        key) { __UWDECL_Char8Ptr (v, key); return _uw_map_get(map, &v); }
static inline UwResult _uw_map_get_u32     (UwValuePtr map, char32_t*       key) { __UWDECL_Char32Ptr(v, key); return _uw_map_get(map, &v); }

static inline UwResult _uw_map_get_u8_wrapper(UwValuePtr map, char* key)
{
    return _uw_map_get_u8(map, (char8_t*) key);
}

/****************************************************************
 * Delete item from map by `key`.
 *
 * Return true if value was deleted, false if no value was identified by key.
 */

#define uw_map_del(map, key) _Generic((key),    \
             nullptr_t: _uw_map_del_null,       \
                  bool: _uw_map_del_bool,       \
                  char: _uw_map_del_signed,     \
         unsigned char: _uw_map_del_unsigned,   \
                 short: _uw_map_del_signed,     \
        unsigned short: _uw_map_del_unsigned,   \
                   int: _uw_map_del_signed,     \
          unsigned int: _uw_map_del_unsigned,   \
                  long: _uw_map_del_signed,     \
         unsigned long: _uw_map_del_unsigned,   \
             long long: _uw_map_del_signed,     \
    unsigned long long: _uw_map_del_unsigned,   \
                 float: _uw_map_del_float,      \
                double: _uw_map_del_float,      \
                 char*: _uw_map_del_u8_wrapper, \
              char8_t*: _uw_map_del_u8,         \
             char32_t*: _uw_map_del_u32,        \
            UwValuePtr: _uw_map_del             \
    )((map), (key))

bool _uw_map_del(UwValuePtr map, UwValuePtr key);

static inline bool _uw_map_del_null    (UwValuePtr map, UwType_Null     key) { __UWDECL_Null     (v);      return _uw_map_del(map, &v); }
static inline bool _uw_map_del_bool    (UwValuePtr map, UwType_Bool     key) { __UWDECL_Bool     (v, key); return _uw_map_del(map, &v); }
static inline bool _uw_map_del_signed  (UwValuePtr map, UwType_Signed   key) { __UWDECL_Signed   (v, key); return _uw_map_del(map, &v); }
static inline bool _uw_map_del_unsigned(UwValuePtr map, UwType_Unsigned key) { __UWDECL_Unsigned (v, key); return _uw_map_del(map, &v); }
static inline bool _uw_map_del_float   (UwValuePtr map, UwType_Float    key) { __UWDECL_Float    (v, key); return _uw_map_del(map, &v); }
static inline bool _uw_map_del_u8      (UwValuePtr map, char8_t*        key) { __UWDECL_Char8Ptr (v, key); return _uw_map_del(map, &v); }
static inline bool _uw_map_del_u32     (UwValuePtr map, char32_t*       key) { __UWDECL_Char32Ptr(v, key); return _uw_map_del(map, &v); }

static inline bool _uw_map_del_u8_wrapper(UwValuePtr map, char* key)
{
    return _uw_map_del_u8(map, (char8_t*) key);
}

/****************************************************************
 * Misc. functions.
 */

unsigned uw_map_length(UwValuePtr map);
/*
 * Return the number of items in `map`.
 */

bool uw_map_item(UwValuePtr map, unsigned index, UwValuePtr key, UwValuePtr value);
/*
 * Get key-value pair from the map.
 * If `index` is valid return true and write result to key and value.
 * It's the caller's responsibility to destroy returned key-value to avoid memory leaks.
 * The simplest way is passing pointers to UwValue.
 *
 * Example:
 *
 * UwValue key = UwNull();
 * UwValue value = UwNull();
 * if (uw_map_item(map, i, &key, &value)) {
 *     // success!
 * }
 */

#ifdef __cplusplus
}
#endif
