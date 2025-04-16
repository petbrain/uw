#pragma once

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void _uw_init_interfaces();
/*
 * Initialize interface ids.
 * Declared with [[ gnu::constructor ]] attribute and automatically called
 * before main().
 *
 * However, the order of initialization is undefined and other modules
 * that use interfaces must call it explicitly from their constructors.
 *
 * This function is idempotent.
 */

void _uw_create_interfaces(UwType* type, va_list ap);
void _uw_update_interfaces(UwType* type, UwType* ancestor, va_list ap);
unsigned _uw_get_num_interface_methods(unsigned interface_id);
/*
 * for internal use
 */

#ifdef __cplusplus
}
#endif
