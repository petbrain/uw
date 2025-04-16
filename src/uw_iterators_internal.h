#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void _uw_init_iterators();
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

#ifdef __cplusplus
}
#endif
