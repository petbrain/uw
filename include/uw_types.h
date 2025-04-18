#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <uchar.h>

#include <libpussy/allocator.h>

#include <uw_helpers.h>

#ifdef __cplusplus
extern "C" {
#endif

// automatically cleaned value
#define _UW_VALUE_CLEANUP [[ gnu::cleanup(uw_destroy) ]]
#define UwValue _UW_VALUE_CLEANUP _UwValue

// Built-in types
#define UwTypeId_Null        0
#define UwTypeId_Bool        1U
#define UwTypeId_Int         2U  // abstract integer
#define UwTypeId_Signed      3U  // subtype of int, signed integer
#define UwTypeId_Unsigned    4U  // subtype of int, unsigned integer
#define UwTypeId_Float       5U
#define UwTypeId_DateTime    6U
#define UwTypeId_Timestamp   7U
#define UwTypeId_Ptr         8U  // container for void*
#define UwTypeId_CharPtr     9U  // container for pointers to static C strings
#define UwTypeId_String     10U
#define UwTypeId_Struct     11U  // the base for reference counted data
#define UwTypeId_Compound   12U  // the base for values that may contain circular references
#define UwTypeId_Status     13U  // value_data is optional
#define UwTypeId_Iterator   14U
#define UwTypeId_Array      15U
#define UwTypeId_Map        16U

// char* sub-types
#define UW_CHARPTR    0
#define UW_CHAR32PTR  1

// limits
#define UW_SIGNED_MAX  0x7fff'ffff'ffff'ffffLL
#define UW_UNSIGNED_MAX  0xffff'ffff'ffff'ffffULL

// type checking
#define uw_is_null(value)      uw_is_subtype((value), UwTypeId_Null)
#define uw_is_bool(value)      uw_is_subtype((value), UwTypeId_Bool)
#define uw_is_int(value)       uw_is_subtype((value), UwTypeId_Int)
#define uw_is_signed(value)    uw_is_subtype((value), UwTypeId_Signed)
#define uw_is_unsigned(value)  uw_is_subtype((value), UwTypeId_Unsigned)
#define uw_is_float(value)     uw_is_subtype((value), UwTypeId_Float)
#define uw_is_datetime(value)  uw_is_subtype((value), UwTypeId_DateTime)
#define uw_is_timestamp(value) uw_is_subtype((value), UwTypeId_Timestamp)
#define uw_is_ptr(value)       uw_is_subtype((value), UwTypeId_Ptr)
#define uw_is_charptr(value)   uw_is_subtype((value), UwTypeId_CharPtr)
#define uw_is_string(value)    uw_is_subtype((value), UwTypeId_String)
#define uw_is_struct(value)    uw_is_subtype((value), UwTypeId_Struct)
#define uw_is_compound(value)  uw_is_subtype((value), UwTypeId_Compound)
#define uw_is_status(value)    uw_is_subtype((value), UwTypeId_Status)
#define uw_is_iterator(value)  uw_is_subtype((value), UwTypeId_Iterator)
#define uw_is_array(value)     uw_is_subtype((value), UwTypeId_Array)
#define uw_is_map(value)       uw_is_subtype((value), UwTypeId_Map)
#define uw_is_file(value)      uw_is_subtype((value), UwTypeId_File)
#define uw_is_stringio(value)  uw_is_subtype((value), UwTypeId_StringIO)

#define uw_assert_null(value)      uw_assert(uw_is_null    (value))
#define uw_assert_bool(value)      uw_assert(uw_is_bool    (value))
#define uw_assert_int(value)       uw_assert(uw_is_int     (value))
#define uw_assert_signed(value)    uw_assert(uw_is_signed  (value))
#define uw_assert_unsigned(value)  uw_assert(uw_is_unsigned(value))
#define uw_assert_float(value)     uw_assert(uw_is_float   (value))
#define uw_assert_datetime(value)  uw_assert(uw_is_datetime(value))
#define uw_assert_timestamp(value) uw_assert(uw_is_timestamp(value))
#define uw_assert_ptr(value)       uw_assert(uw_is_ptr     (value))
#define uw_assert_charptr(value)   uw_assert(uw_is_charptr (value))
#define uw_assert_string(value)    uw_assert(uw_is_string  (value))
#define uw_assert_struct(value)    uw_assert(uw_is_struct  (value))
#define uw_assert_compound(value)  uw_assert(uw_is_compound(value))
#define uw_assert_status(value)    uw_assert(uw_is_status  (value))
#define uw_assert_iterator(value)  uw_assert(uw_is_iterator(value))
#define uw_assert_array(value)     uw_assert(uw_is_array   (value))
#define uw_assert_map(value)       uw_assert(uw_is_map     (value))
#define uw_assert_file(value)      uw_assert(uw_is_file    (value))
#define uw_assert_stringio(value)  uw_assert(uw_is_stringio(value))

// forward declarations

// defined in uw_status.h
struct __UwStatusData;

// defined in uw_interfaces.h
struct __UwInterface;
typedef struct __UwInterface _UwInterface;

// defined privately in src/uw_hash.c:
struct _UwHashContext;
typedef struct _UwHashContext UwHashContext;


// Type for type id.
typedef uint16_t UwTypeId;

// Integral types
typedef nullptr_t  UwType_Null;
typedef bool       UwType_Bool;
typedef int64_t    UwType_Signed;
typedef uint64_t   UwType_Unsigned;
typedef double     UwType_Float;

typedef struct {
    unsigned refcount;
    uint32_t capacity;
    uint8_t data[];
} _UwStringData;

typedef struct { uint8_t v[3]; } uint24_t;  // three bytes wide characters, always little-endian

typedef struct {
    union {
        // the basic structure contains reference count only,
        unsigned refcount;

        // but we need to make sure it has correct size for proper alignmenf of subsequent structures
        void* padding;
    };
} _UwStructData;

// make sure largest C type fits into 64 bits
static_assert( sizeof(long long) <= sizeof(uint64_t) );

union __UwValue {
    /*
     * 128-bit value
     */

    UwTypeId /* uint16_t */ type_id;

    uint64_t u64[2];

    struct {
        // integral types
        UwTypeId /* uint16_t */ _integral_type_id;
        uint8_t carry;  // for integer arithmetic
        uint8_t  _integral_pagging_1;
        uint32_t _integral_pagging_2;
        union {
            // Integral types
            UwType_Bool     bool_value;
            UwType_Signed   signed_value;
            UwType_Unsigned unsigned_value;
            UwType_Float    float_value;
        };
    };

    struct {
        // charptr and ptr
        UwTypeId /* uint16_t */ _charptr_type_id;
        uint8_t charptr_subtype; // see UW_CHAR*PTR constants
        uint8_t  _charptr_padding_1;
        uint32_t _charptr_pagging_2;
        union {
            // C string pointers for UwType_CharPtr
            char8_t*  charptr;
            char32_t* char32ptr;

            // void*
            void* ptr;
        };
    };

    struct {
        // struct
        UwTypeId /* uint16_t */ _struct_type_id;
        int16_t _struct_padding1;
        uint32_t _struct_padding2;
        _UwStructData* struct_data;  // the first member of struct_data is reference count
    };

    struct {
        // status
        UwTypeId /* uint16_t */ _status_type_id;
        int16_t uw_errno;
        uint16_t line_number;   // not set for UW_SUCCESS
        union {
            uint16_t is_error;  // all bit fields are zero for UW_SUCCESS
                                // so we can avoid bit operations when checking for success
            struct {
                uint16_t status_code: 15,
                         has_status_data: 1;
            };
        };
        union {
            char* file_name;  // when has_status_data == 0, not set for UW_SUCCESS
            struct __UwStatusData* status_data;  // when has_status_data == 1
        };
    };

    struct {
        // embedded string
        UwTypeId /* uint16_t */ _emb_string_type_id;
        uint8_t str_embedded:1,       // the string data is embedded into UwValue
                str_char_size:2;
        uint8_t str_embedded_length;  // length of embedded string
        union {
            uint8_t  str_1[12];
            uint16_t str_2[6];
            uint24_t str_3[4];
            uint32_t str_4[3];
        };
    };

    struct {
        // allocated string
        UwTypeId /* uint16_t */ _string_type_id;
        uint8_t _x_str_embedded:1,   // zero for allocated string
                _x_str_char_size:2;
        uint8_t _str_padding;
        uint32_t str_length;
        _UwStringData* string_data;
    };

    struct {
        // date/time
        UwTypeId /* uint16_t */ _datetime_type_id;
        uint16_t year;
        // -- 32 bits
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        // -- 64 bits
        uint32_t nanosecond;
        int16_t gmt_offset;  // in minutes
        uint8_t second;

        uint8_t tzindex;
        /* Index in the zone info cache.
         * If zero, zone info is undefined.
         */
    };

    struct {
        // timestamp
        UwTypeId /* uint16_t */ _timestamp_type_id;
        uint16_t _timestamp_padding;
        uint32_t ts_nanoseconds;
        uint64_t ts_seconds;
    };

};
typedef union __UwValue _UwValue;

typedef _UwValue* UwValuePtr;
typedef _UwValue  UwResult;  // alias for return values

// make sure _UwValue structure is correct
static_assert( offsetof(_UwValue, charptr_subtype) == 2 );

static_assert( offsetof(_UwValue, bool_value) == 8 );
static_assert( offsetof(_UwValue, charptr)    == 8 );
static_assert( offsetof(_UwValue, struct_data) == 8 );
static_assert( offsetof(_UwValue, status_data) == 8 );
static_assert( offsetof(_UwValue, string_data) == 8 );

static_assert( offsetof(_UwValue, str_embedded_length) == 3 );
static_assert( offsetof(_UwValue, str_1) == 4 );
static_assert( offsetof(_UwValue, str_2) == 4 );
static_assert( offsetof(_UwValue, str_3) == 4 );
static_assert( offsetof(_UwValue, str_4) == 4 );

static_assert( sizeof(_UwValue) == 16 );


struct __UwStatusData {
    unsigned refcount;
    unsigned line_number;
    char* file_name;
    _UwValue description;  // string
};
typedef struct __UwStatusData _UwStatusData;


struct __UwCompoundChain {
    /*
     * Compound values may contain cyclic references.
     * This structure along with function `_uw_on_chain`
     * helps to detect them.
     * See dump implementation for array and map values.
     */
    struct __UwCompoundChain* prev;
    UwValuePtr value;
};
typedef struct __UwCompoundChain _UwCompoundChain;

// Function types for the basic interface.
// The basic interface is embedded into UwType structure.
typedef UwResult (*UwMethodCreate)  (UwTypeId type_id, void* ctor_args);
typedef void     (*UwMethodDestroy) (UwValuePtr self);
typedef UwResult (*UwMethodClone)   (UwValuePtr self);
typedef void     (*UwMethodHash)    (UwValuePtr self, UwHashContext* ctx);
typedef UwResult (*UwMethodDeepCopy)(UwValuePtr self);
typedef void     (*UwMethodDump)    (UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail);
typedef UwResult (*UwMethodToString)(UwValuePtr self);
typedef bool     (*UwMethodIsTrue)  (UwValuePtr self);
typedef bool     (*UwMethodEqual)   (UwValuePtr self, UwValuePtr other);

// Function types for struct interface.
// They modify the data in place and return status.
typedef UwResult (*UwMethodInit)(UwValuePtr self, void* ctor_args);
typedef void     (*UwMethodFini)(UwValuePtr self);


typedef struct {
    /*
     * UW type
     */
    UwTypeId id;
    UwTypeId ancestor_id;
    char* name;
    Allocator* allocator;

    // basic interface
    // optional methods should be called only if not nullptr
    UwMethodCreate   create;    // mandatory
    UwMethodDestroy  destroy;   // optional if value does not have allocated data
    UwMethodClone    clone;     // optional; if set, it is called by uw_clone()
    UwMethodHash     hash;      // mandatory
    UwMethodDeepCopy deepcopy;  // optional; XXX how it should work with subtypes is not clear yet
    UwMethodDump     dump;      // mandatory
    UwMethodToString to_string; // mandatory
    UwMethodIsTrue   is_true;   // mandatory
    UwMethodEqual    equal_sametype;  // mandatory
    UwMethodEqual    equal;     // mandatory

    // struct data offset and size
    unsigned data_offset;
    unsigned data_size;

    /*
     * Struct interface
     *
     * Methods are optional and can be null.
     * They are called by _uw_struct_alloc and _uw_struct_release.
     *
     * init must be atomic, i.e. allocate either all data or nothing.
     * If init fails for some subtype, _uw_struct_alloc calls fini method
     * for already called init in the pedigree.
     */
    UwMethodInit init;
    UwMethodFini fini;

    // other interfaces
    unsigned num_interfaces;
    _UwInterface* interfaces;
} UwType;


void _uw_init_types();
/*
 * Initialize types.
 * Declared with [[ gnu::constructor ]] attribute and automatically called
 * before main().
 *
 * However, the order of initialization is undefined and other modules
 * that must call it explicitly from their constructors.
 *
 * This function is idempotent.
 */

extern UwType** _uw_types;
/*
 * Global list of types initialized with built-in types.
 */

#define uw_typeof(value)  (_uw_types[(value)->type_id])

#define uw_ancestor_of(type_id) (_uw_types[_uw_types[type_id]->ancestor_id])

static inline bool uw_is_subtype(UwValuePtr value, UwTypeId type_id)
{
    UwTypeId t = value->type_id;
    for (;;) {
        if (_uw_likely(t == type_id)) {
            return true;
        }
        t = _uw_types[t]->ancestor_id;
        if (_uw_likely(t == UwTypeId_Null)) {
            return false;
        }
    }
}

UwTypeId _uw_add_type(UwType* type, ...);
#define uw_add_type(type, ...)  _uw_add_type((type) __VA_OPT__(,) __VA_ARGS__, -1)
/*
 * Add type to the first available position in the global list.
 *
 * All fields of `type` must be initialized except interfaces.
 *
 * Variadic arguments are pairs of interface id and interface pointer
 * terminated by -1 (`uw_add_type` wrapper does that by default).
 *
 * Return new type id.
 *
 * All errors in this function are considered as critical and cause program abort.
 */

UwTypeId _uw_subtype(UwType* type, char* name, UwTypeId ancestor_id,
                     unsigned data_size, unsigned alignment, ...);

#define uw_subtype(type, name, ancestor_id, data_type, ...)  \
    _uw_subtype((type), (name), (ancestor_id), \
                sizeof(data_type), alignof(data_type) __VA_OPT__(,) __VA_ARGS__, -1)
/*
 * `type` and `name` should point to a static storage.
 *
 * Variadic arguments are interfaces to override.
 * They are pairs of interface id and interface pointer
 * terminated by -1 (`uw_subtype` wrapper does that by default).
 * Interfaces contain only methods to override, methods that do not need to be overriden
 * should be null pointers.
 *
 * The function initializes `type` with ancestor's type, calculates data_offset,
 * sets data_size and other essential fields, and then adds `type`
 * to the global list.
 *
 * The caller may alter basic methods after calling this function.
 *
 * Return new type id.
 *
 * All errors in this function are considered as critical and cause program abort.
 */

#define uw_get_type_name(v) _Generic((v),        \
                  char: _uw_get_type_name_by_id, \
         unsigned char: _uw_get_type_name_by_id, \
                 short: _uw_get_type_name_by_id, \
        unsigned short: _uw_get_type_name_by_id, \
                   int: _uw_get_type_name_by_id, \
          unsigned int: _uw_get_type_name_by_id, \
                  long: _uw_get_type_name_by_id, \
         unsigned long: _uw_get_type_name_by_id, \
             long long: _uw_get_type_name_by_id, \
    unsigned long long: _uw_get_type_name_by_id, \
            UwValuePtr: _uw_get_type_name_from_value  \
    )(v)

static inline char* _uw_get_type_name_by_id     (uint8_t type_id)  { return _uw_types[type_id]->name; }
static inline char* _uw_get_type_name_from_value(UwValuePtr value) { return _uw_types[value->type_id]->name; }

void uw_dump_types(FILE* fp);

/****************************************************************
 * Constructors
 */

#define uw_create(type_id)              _uw_types[type_id]->create((type_id), nullptr)
#define uw_create2(type_id, ctor_args)  _uw_types[type_id]->create((type_id), (ctor_args))
/*
 * Basic constructors.
 */

/*
 * In-place declarations and rvalues
 *
 * __UWDECL_* macros define values that are not automatically cleaned. Use carefully.
 *
 * UWDECL_* macros define automatically cleaned values. Okay to use for local variables.
 *
 * Uw<Typename> macros define rvalues that should be destroyed either explicitly
 * or automatically, by assigning them to an automatically cleaned variable
 * or passing to UwArray(), UwMap() or other uw_*_va function that takes care of its arguments.
 */

#define __UWDECL_Null(name)  \
    /* declare Null variable */  \
    _UwValue name = {  \
        .type_id = UwTypeId_Null  \
    }

#define UWDECL_Null(name)  _UW_VALUE_CLEANUP __UWDECL_Null(name)

#define UwNull()  \
    /* make Null rvalue */  \
    ({  \
        __UWDECL_Null(v);  \
        v;  \
    })

#define __UWDECL_Bool(name, initializer)  \
    /* declare Bool variable */  \
    _UwValue name = {  \
        ._integral_type_id = UwTypeId_Bool,  \
        .bool_value = (initializer)  \
    }

#define UWDECL_Bool(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_Bool((name), (initializer))

#define UwBool(initializer)  \
    /* make Bool rvalue */  \
    ({  \
        __UWDECL_Bool(v, (initializer));  \
        v;  \
    })

#define __UWDECL_Signed(name, initializer)  \
    /* declare Signed variable */  \
    _UwValue name = {  \
        ._integral_type_id = UwTypeId_Signed,  \
        .signed_value = (initializer),  \
    }

#define UWDECL_Signed(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_Signed((name), (initializer))

#define UwSigned(initializer)  \
    /* make Signed rvalue */  \
    ({  \
        __UWDECL_Signed(v, (initializer));  \
        v;  \
    })

#define __UWDECL_Unsigned(name, initializer)  \
    /* declare Unsigned variable */  \
    _UwValue name = {  \
        ._integral_type_id = UwTypeId_Unsigned,  \
        .unsigned_value = (initializer)  \
    }

#define UWDECL_Unsigned(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_Unsigned((name), (initializer))

#define UwUnsigned(initializer)  \
    /* make Unsigned rvalue */  \
    ({  \
        __UWDECL_Unsigned(v, (initializer));  \
        v;  \
    })

#define __UWDECL_Float(name, initializer)  \
    /* declare Float variable */  \
    _UwValue name = {  \
        ._integral_type_id = UwTypeId_Float,  \
        .float_value = (initializer)  \
    }

#define UWDECL_Float(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_Float((name), (initializer))

#define UwFloat(initializer)  \
    /* make Float rvalue */  \
    ({  \
        __UWDECL_Float(v, (initializer));  \
        v;  \
    })

// The very basic string declaration and rvalue, see uw_string.h for more macros:

#define __UWDECL_String(name)  \
    /* declare empty String variable */  \
    _UwValue name = {  \
        ._emb_string_type_id = UwTypeId_String,  \
        .str_embedded = 1  \
    }

#define UWDECL_String(name)  _UW_VALUE_CLEANUP __UWDECL_String(name)

#define UwString()  \
    /* make empty String rvalue */  \
    ({  \
        __UWDECL_String(v);  \
        v;  \
    })

#define __UWDECL_DateTime(name)  \
    /* declare DateTime variable */  \
    _UwValue name = {  \
        ._datetime_type_id = UwTypeId_DateTime  \
    }

#define UWDECL_DateTime(name)  _UW_VALUE_CLEANUP __UWDECL_DateTime((name))

#define UwDateTime()  \
    /* make DateTime rvalue */  \
    ({  \
        __UWDECL_DateTime(v);  \
        v;  \
    })

#define __UWDECL_Timestamp(name)  \
    /* declare Timestamp variable */  \
    _UwValue name = {  \
        ._timestamp_type_id = UwTypeId_Timestamp  \
    }

#define UWDECL_Timestamp(name)  _UW_VALUE_CLEANUP __UWDECL_Timestamp((name))

#define UwTimestamp()  \
    /* make Timestamp rvalue */  \
    ({  \
        __UWDECL_Timestamp(v);  \
        v;  \
    })

#define __UWDECL_CharPtr(name, initializer)  \
    /* declare CharPtr variable */  \
    _UwValue name = {  \
        ._charptr_type_id = UwTypeId_CharPtr,  \
        .charptr_subtype = UW_CHARPTR,  \
        .charptr = (char8_t*) (initializer)  \
    }

#define UWDECL_CharPtr(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_CharPtr((name), (initializer))

#define UwCharPtr(initializer)  \
    /* make CharPtr rvalue */  \
    ({  \
        __UWDECL_CharPtr(v, (initializer));  \
        v;  \
    })

#define __UWDECL_Char32Ptr(name, initializer)  \
    /* declare Char32Ptr variable */  \
    _UwValue name = {  \
        ._charptr_type_id = UwTypeId_CharPtr,  \
        .charptr_subtype = UW_CHAR32PTR,  \
        .char32ptr = (initializer)  \
    }

#define UWDECL_Char32Ptr(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_Char32Ptr((name), (initializer))

#define UwChar32Ptr(initializer)  \
    /* make Char32Ptr rvalue */  \
    ({  \
        __UWDECL_Char32Ptr(v, (initializer));  \
        v;  \
    })

#define __UWDECL_Ptr(name, initializer)  \
    /* declare Ptr variable */  \
    _UwValue name = {  \
        ._charptr_type_id = UwTypeId_Ptr,  \
        .ptr = (initializer)  \
    }

#define UWDECL_Ptr(name, initializer)  _UW_VALUE_CLEANUP __UWDECL_Ptr((name), (initializer))

#define UwPtr(initializer)  \
    /* make Ptr rvalue */  \
    ({  \
        __UWDECL_Ptr(v, (initializer));  \
        v;  \
    })


// Status declarations and rvalues

#define __UWDECL_Status(name, _status_code)  \
    /* declare Status variable */  \
    _UwValue name = {  \
        ._status_type_id = UwTypeId_Status,  \
        .status_code = _status_code,  \
        .line_number = __LINE__, \
        .file_name = __FILE__ \
    }

#define UWDECL_Status(name, _status_code)  \
    _UW_VALUE_CLEANUP __UWDECL_Status((name), (_status_code))

#define UwStatus(_status_code)  \
    /* make Status rvalue */  \
    ({  \
        __UWDECL_Status(status, (_status_code));  \
        status;  \
    })

#define UwOK()  \
    /* make success rvalue */  \
    ({  \
        _UwValue status = {  \
            ._status_type_id = UwTypeId_Status,  \
            .is_error = 0  \
        };  \
        status;  \
    })

#define UwError(code)  \
    /* make Status rvalue */  \
    ({  \
        __UWDECL_Status(status, (code));  \
        status;  \
    })

#define UwOOM()  \
    /* make UW_ERROR_OOM rvalue */  \
    ({  \
        __UWDECL_Status(status, UW_ERROR_OOM);  \
        status;  \
    })

#define UwVaEnd()  \
    /* make VA_END rvalue */  \
    ({  \
        _UwValue status = {  \
            ._status_type_id = UwTypeId_Status,  \
            .status_code = UW_STATUS_VA_END  \
        };  \
        status;  \
    })

#define __UWDECL_Errno(name, _errno)  \
    /* declare errno Status variable */  \
    _UwValue name = {  \
        ._status_type_id = UwTypeId_Status,  \
        .status_code = UW_ERROR_ERRNO,  \
        .uw_errno = _errno,  \
        .line_number = __LINE__, \
        .file_name = __FILE__ \
    }

#define UWDECL_Errno(name, _errno)  _UW_VALUE_CLEANUP __UWDECL_Errno((name), (_errno))

#define UwErrno(_errno)  \
    /* make errno Status rvalue */  \
    ({  \
        __UWDECL_Errno(status, _errno);  \
        status;  \
    })

/****************************************************************
 * Basic methods
 *
 * Most of them are inline wrappers around method invocation
 * macros.
 *
 * Using inline functions to avoid multiple evaluation of args
 * when, say, uw_destroy(vptr++) is called.
 */

static inline void uw_destroy(UwValuePtr value)
/*
 * Destroy value: call destructor and make `value` Null.
 */
{
    if (value->type_id != UwTypeId_Null) {
        UwMethodDestroy fn = uw_typeof(value)->destroy;
        if (fn) {
            fn(value);
        }
        value->type_id = UwTypeId_Null;
    }
}

static inline UwResult uw_clone(UwValuePtr value)
/*
 * Clone value.
 */
{
    UwMethodClone fn = uw_typeof(value)->clone;
    if (fn) {
        return fn(value);
    } else {
        return *value;
    }
}

static inline UwResult uw_move(UwValuePtr v)
/*
 * "Move" value to another variable or out of the function
 * (i.e. return a value and reset autocleaned variable)
 */
{
    _UwValue tmp = *v;
    v->type_id = UwTypeId_Null;
    return tmp;
}

static inline UwResult uw_deepcopy(UwValuePtr value)
{
    UwMethodDeepCopy fn = uw_typeof(value)->deepcopy;
    if (fn) {
        return fn(value);
    } else {
        return *value;
    }
}

static inline bool uw_is_true(UwValuePtr value)
{
    return uw_typeof(value)->is_true(value);
}

static inline UwResult uw_to_string(UwValuePtr value)
{
    return uw_typeof(value)->to_string(value);
}

/****************************************************************
 * API for struct types
 */

static inline void* _uw_get_data_ptr(UwValuePtr v, UwTypeId type_id)
/*
 * Helper function to get pointer to struct data.
 * Typically used in macros like this:
 *
 * #define get_data_ptr(value)  ((MyType*) _uw_get_data_ptr((value), UwTypeId_MyType))
 */
{
    if (v->struct_data) {
        UwType* t = _uw_types[type_id];
        return (void*) (
            ((uint8_t*) v->struct_data) + t->data_offset
        );
    } else {
        return nullptr;
    }
}

/****************************************************************
 * API for compound types
 */

bool _uw_adopt(UwValuePtr parent, UwValuePtr child);
/*
 * Add parent to child's parents or increment
 * parents_refcount if added already.
 *
 * Decrement child refcount.
 *
 * Return false if OOM.
 */

bool _uw_abandon(UwValuePtr parent, UwValuePtr child);
/*
 * Decrement parents_refcount in child's list of parents and when it reaches zero
 * remove parent from child's parents and return true.
 *
 * If child still refers to parent, return false.
 */

bool _uw_is_embraced(UwValuePtr value);
/*
 * Return true if value is embraced by some parent.
 */

bool _uw_need_break_cyclic_refs(UwValuePtr value);
/*
 * Check if all parents have zero refcount and there are cyclic references.
 */

static inline bool _uw_embrace(UwValuePtr parent, UwValuePtr child)
{
    if (uw_is_compound(child)) {
        return _uw_adopt(parent, child);
    } else {
        return true;
    }
}

UwValuePtr _uw_on_chain(UwValuePtr value, _UwCompoundChain* tail);
/*
 * Check if value struct_data is on chain.
 */

/****************************************************************
 * Compare for equality.
 */

static inline bool _uw_equal(UwValuePtr a, UwValuePtr b)
{
    if (a == b) {
        // compare with self
        return true;
    }
    if (a->u64[0] == b->u64[0] && a->u64[1] == b->u64[1]) {
        // quick comparison
        return true;
    }
    UwType* t = uw_typeof(a);
    UwMethodEqual cmp;
    if (a->type_id == b->type_id) {
        cmp = t->equal_sametype;
    } else {
        cmp = t->equal;
    }
    return cmp(a, b);
}

#define uw_equal(a, b) _Generic((b),           \
             nullptr_t: _uwc_equal_null,       \
                  bool: _uwc_equal_bool,       \
                  char: _uwc_equal_char,       \
         unsigned char: _uwc_equal_uchar,      \
                 short: _uwc_equal_short,      \
        unsigned short: _uwc_equal_ushort,     \
                   int: _uwc_equal_int,        \
          unsigned int: _uwc_equal_uint,       \
                  long: _uwc_equal_long,       \
         unsigned long: _uwc_equal_ulong,      \
             long long: _uwc_equal_longlong,   \
    unsigned long long: _uwc_equal_ulonglong,  \
                 float: _uwc_equal_float,      \
                double: _uwc_equal_double,     \
                 char*: _uwc_equal_u8_wrapper, \
              char8_t*: _uwc_equal_u8,         \
             char32_t*: _uwc_equal_u32,        \
            UwValuePtr: _uw_equal              \
    )((a), (b))
/*
 * Type-generic compare for equality.
 */

static inline bool _uwc_equal_null      (UwValuePtr a, nullptr_t          b) { return uw_is_null(a); }
static inline bool _uwc_equal_bool      (UwValuePtr a, bool               b) { __UWDECL_Bool     (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_char      (UwValuePtr a, char               b) { __UWDECL_Signed   (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_uchar     (UwValuePtr a, unsigned char      b) { __UWDECL_Unsigned (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_short     (UwValuePtr a, short              b) { __UWDECL_Signed   (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_ushort    (UwValuePtr a, unsigned short     b) { __UWDECL_Unsigned (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_int       (UwValuePtr a, int                b) { __UWDECL_Signed   (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_uint      (UwValuePtr a, unsigned int       b) { __UWDECL_Unsigned (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_long      (UwValuePtr a, long               b) { __UWDECL_Signed   (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_ulong     (UwValuePtr a, unsigned long      b) { __UWDECL_Unsigned (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_longlong  (UwValuePtr a, long long          b) { __UWDECL_Signed   (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_ulonglong (UwValuePtr a, unsigned long long b) { __UWDECL_Unsigned (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_float     (UwValuePtr a, float              b) { __UWDECL_Float    (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_double    (UwValuePtr a, double             b) { __UWDECL_Float    (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_u8        (UwValuePtr a, char8_t*           b) { __UWDECL_CharPtr  (v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_u32       (UwValuePtr a, char32_t*          b) { __UWDECL_Char32Ptr(v, b); return _uw_equal(a, &v); }
static inline bool _uwc_equal_u8_wrapper(UwValuePtr a, char*              b) { return _uwc_equal_u8(a, (char8_t*) b); }

#ifdef __cplusplus
}
#endif
