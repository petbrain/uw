#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct _UwHashContext {
    uint64_t seed;
    uint64_t see1;
    uint64_t see2;
    uint64_t buffer[6];
    int buf_size;
};

void _uw_hash_init(UwHashContext* ctx);

// the following prototypes are duplicated in uw_base.h
void _uw_hash_uint64(UwHashContext* ctx, uint64_t data);
void _uw_hash_buffer(UwHashContext* ctx, void* buffer, size_t length);
void _uw_hash_string(UwHashContext* ctx, char* str);
void _uw_hash_string32(UwHashContext* ctx, char32_t* str);

UwType_Hash _uw_hash_finish(UwHashContext* ctx);

#ifdef __cplusplus
}
#endif
