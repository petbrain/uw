#pragma once

#include <stdarg.h>

#include <uw_assert.h>
#include <uw_helpers.h>
#include <uw_types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct __UwInterface {
    unsigned interface_id;
    void** interface_methods;  // pointer to array of pointers to functions,
                               // uw_interface casts this to pointer to interface structure
};
typedef struct __UwInterface _UwInterface;

// Built-in interfaces
/*
// TBD, TODO
#define UwInterfaceId_Logic         0
    // TBD
#define UwInterfaceId_Arithmetic    1
    // TBD
#define UwInterfaceId_Bitwise       2
    // TBD
#define UwInterfaceId_Comparison    3
    // UwMethodCompare  -- compare_sametype, compare;
#define UwInterfaceId_RandomAccess  4
    // UwMethodLength
    // UwMethodGetItem     (by index for arrays/strings or by key for maps)
    // UwMethodSetItem     (by index for arrays/strings or by key for maps)
    // UwMethodDeleteItem  (by index for arrays/strings or by key for maps)
    // UwMethodPopItem -- necessary here? it's just delete_item(length - 1)
#define UwInterfaceId_String        5
    // TBD substring, truncate, trim, append_substring, etc
#define UwInterfaceId_Array         6
    // string supports this interface
    // UwMethodAppend
    // UwMethodSlice
    // UwMethodDeleteRange
*/

//#define UwInterfaceId_LineReader    0


unsigned _uw_register_interface(char* name, unsigned num_methods);
/*
 * Generate global identifier for interface and associate
 * `name` and `num_methods` with it.
 *
 * The number of methods is used to initialize interface fields
 * in `_uw_add_type` and `_uw_subtype` functions.
 */

#define uw_register_interface(name, interface_type)  \
    _uw_register_interface((name), sizeof(interface_type) / sizeof(void*))

char* uw_get_interface_name(unsigned interface_id);
/*
 * Get registered interface name by id.
 */

static inline _UwInterface* _uw_lookup_interface(UwTypeId type_id, unsigned interface_id)
{
    UwType* type = _uw_types[type_id];
    _UwInterface* iface = type->interfaces;
    if (iface) {
        unsigned i = 0;
        do {
            if (iface->interface_id == interface_id) {
                return iface;
            }
            iface++;
            i++;
        } while (i < type->num_interfaces);
    }
    return nullptr;
}

static inline void* _uw_get_interface(UwTypeId type_id, unsigned interface_id)
{
    _UwInterface* result = _uw_lookup_interface(type_id, interface_id);
    if (result) {
        return result->interface_methods;
    }
    _uw_panic_no_interface(type_id, interface_id);
}

static inline bool _uw_has_interface(UwTypeId type_id, unsigned interface_id)
{
    return (bool) _uw_lookup_interface(type_id, interface_id);
}

/*
 * The following macro depends on naming scheme where
 * interface structure is named UwInterface_<interface_name>
 * and id is named UwInterfaceId_<interface_name>
 *
 * Example use:
 *
 * uw_interface(reader->type_id, LineReader)->read_line(reader);
 *
 * Calling super method:
 *
 * uw_interface(UwTypeId_AncestorOfReader, LineReader)->read_line(reader);
 */
#define uw_interface(type_id, interface_name)  \
    (  \
        (UwInterface_##interface_name *)  \
            _uw_get_interface((type_id), UwInterfaceId_##interface_name)  \
    )

#ifdef __cplusplus
}
#endif
