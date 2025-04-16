#pragma once

#include <uw_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************
 * Hash functions
 */

typedef uint64_t  UwType_Hash;

void _uw_hash_uint64(UwHashContext* ctx, uint64_t data);
void _uw_hash_buffer(UwHashContext* ctx, void* buffer, size_t length);
void _uw_hash_string(UwHashContext* ctx, char* str);
void _uw_hash_string32(UwHashContext* ctx, char32_t* str);

static inline void _uw_call_hash(UwValuePtr value, UwHashContext* ctx)
/*
 * Call hash method of value.
 */
{
    uw_typeof(value)->hash(value, ctx);
}

UwType_Hash uw_hash(UwValuePtr value);
/*
 * Calculate hash of value.
 */

#ifdef __cplusplus
}
#endif
