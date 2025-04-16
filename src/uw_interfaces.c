#include <limits.h>
#include <string.h>

#include <libpussy/arena.h>
#include <libpussy/mmarray.h>

#include "include/uw_interfaces.h"
#include "src/uw_interfaces_internal.h"

typedef struct {
    char* name;
    unsigned num_methods;
} InterfaceInfo;

static InterfaceInfo* registered_interfaces = nullptr;  // array for interfaces

static Arena* arena = nullptr;  // arena for interface methods, per data type


[[ noreturn ]]
void _uw_panic_no_interface(UwTypeId type_id, unsigned interface_id)
{
    uw_panic("Interface %u is not defined for %s\n", interface_id, _uw_types[type_id]->name);
}

#define MAX_INTERFACES  (UINT_MAX - 1)  // UINT_MAX equals to -1 which is used as terminator
                                        // in uw_add_type and uw_subtype, that's why UINT_MAX - 1

[[ gnu::constructor ]]
void _uw_init_interfaces()
{
    if (registered_interfaces) {
        return;
    }

    registered_interfaces = mmarray_allocate(0, sizeof(InterfaceInfo));

    arena = create_arena(0);
    if (!arena) {
        uw_panic("Cannot create arena\n");
    }

    // register built-ion interfaces

    uw_assert(UwInterfaceId_LineReader == uw_register_interface("LineReader", UwInterface_LineReader));
}

unsigned _uw_register_interface(char* name, unsigned num_methods)
{
    _uw_init_types();

    unsigned n = mmarray_length(registered_interfaces);
    if (n == MAX_INTERFACES) {
        uw_panic("Cannot define more interfaces than %u\n", MAX_INTERFACES);
    }
    InterfaceInfo info = {
        .name = name,
        .num_methods = num_methods
    };
    registered_interfaces = mmarray_append_item(registered_interfaces, &info);
    return n;
}

char* uw_get_interface_name(unsigned interface_id)
{
    unsigned n = mmarray_length(registered_interfaces);
    if (interface_id >= n) {
        uw_panic("Interfaces %u is not registered yet\n", interface_id);
    }
    return registered_interfaces[interface_id].name;
}

unsigned _uw_get_num_interface_methods(unsigned interface_id)
/*
 * Get the number of methods of interface.
 */
{
    unsigned n = mmarray_length(registered_interfaces);
    if (interface_id >= n) {
        uw_panic("Interfaces %u is not registered yet\n", interface_id);
    }
    return registered_interfaces[interface_id].num_methods;
}

static void alloc_interfaces(UwType* type)
/*
 * Allocate type->interfaces.
 * type->num_interfaces must be initialized.
 */
{
    type->interfaces = arena_alloc(arena, type->num_interfaces, _UwInterface);
    if (!type->interfaces) {
        uw_panic("%s: cannot allocate memory block for %u interfaces for type %s\n",
                 __func__, type->num_interfaces, type->name);
    }
}

static inline unsigned methods_memsize(unsigned num_methods)
/*
 * Return size of memory block in bytes to store pointers to interface methods.
 */
{
    return num_methods * (sizeof(void*) + sizeof(UwTypeId)) * 2;
}

static void** alloc_methods(unsigned num_methods)
/*
 * Allocate memory block for pointers to interface methods.
 */
{
    unsigned memsize = methods_memsize(num_methods);
    void** result = _arena_alloc(arena, memsize, alignof(void*));
    if (!result) {
        uw_panic("cannot allocate %u-bytes memory block for %u methods\n", memsize, num_methods);
    }
    return result;
}

static void** make_interface_methods(unsigned interface_id, void** methods)
/*
 * Allocate memory block and copy `methods`, checking them for nullptr.
 */
{
    unsigned num_methods = _uw_get_num_interface_methods(interface_id);
    void** interface_methods = alloc_methods(num_methods);
    bzero(interface_methods, methods_memsize(num_methods));

    for (unsigned i = 0; i < num_methods; i++) {
        void* meth = methods[i];
        if (meth) {
            interface_methods[i] = meth;
        } else {
            uw_panic("Method %u for interface %s is not defined\n",
                     i, uw_get_interface_name(interface_id));
        }
    }
    return interface_methods;
}

void _uw_create_interfaces(UwType* type, va_list ap)
{
    // count interface argument pairs
    type->num_interfaces = 0;
    va_list temp_ap;
    va_copy(temp_ap, ap);
    for (;;) {
        unsigned interface_id = va_arg(temp_ap, unsigned);
        if (((int) interface_id) == -1) {
            break;
        }
        va_arg(temp_ap, void**);
        type->num_interfaces++;
    }
    va_end(temp_ap);

    if (type->num_interfaces == 0) {
        type->interfaces = nullptr;
        return;
    }

    // init interfaces
    alloc_interfaces(type);
    _UwInterface* iface = type->interfaces;
    for (unsigned i = 0; i < type->num_interfaces; i++, iface++) {
        iface->interface_id = va_arg(ap, unsigned);
        iface->interface_methods = make_interface_methods(iface->interface_id, va_arg(ap, void**));
    }
}

void _uw_update_interfaces(UwType* type, UwType* ancestor, va_list ap)
{
    // count new interfaces, starting from existing
    type->num_interfaces = ancestor->num_interfaces;
    va_list temp_ap;
    va_copy(temp_ap, ap);
    for (;;) {
        unsigned interface_id = va_arg(temp_ap, unsigned);
        if (((int) interface_id) == -1) {
            break;
        }
        va_arg(temp_ap, void**);
        if (!_uw_has_interface(ancestor->id, interface_id)) {
            type->num_interfaces++;
        }
    }
    va_end(temp_ap);

    if (type->num_interfaces == 0) {
        type->interfaces = nullptr;
        return;
    }

    // copy ancestor's interfaces
    alloc_interfaces(type);
    memcpy(type->interfaces, ancestor->interfaces, ancestor->num_interfaces * sizeof(_UwInterface));

    // update interface methods
    _UwInterface* new_iface = &type->interfaces[ancestor->num_interfaces];
    for (;;) {
        unsigned interface_id = va_arg(ap, unsigned);
        if (((int) interface_id) == -1) {
            break;
        }
        _UwInterface* src_iface = _uw_lookup_interface(ancestor->id, interface_id);
        if (src_iface) {
            // update existing interface
            _UwInterface* dest_iface = _uw_lookup_interface(type->id, interface_id);
            void** new_methods = va_arg(ap, void**);
            unsigned num_methods = _uw_get_num_interface_methods(interface_id);
            void** interface_methods = alloc_methods(num_methods);
            memcpy(interface_methods, src_iface->interface_methods, methods_memsize(num_methods));

            for (unsigned i = 0; i < num_methods; i++) {
                void* meth = new_methods[i];
                if (meth) {
                    interface_methods[i] = meth;
                }
            }
            dest_iface->interface_methods = interface_methods;
        } else {
            // add new interface
            new_iface->interface_id = interface_id;
            new_iface->interface_methods = make_interface_methods(interface_id, va_arg(ap, void**));
            new_iface++;
        }
    }
}
