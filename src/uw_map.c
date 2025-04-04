#include <limits.h>
#include <string.h>

#include "include/uw.h"
#include "src/uw_charptr_internal.h"
#include "src/uw_compound_internal.h"
#include "src/uw_map_internal.h"
#include "src/uw_struct_internal.h"

#define get_data_ptr(value)  _uw_get_data_ptr((value), UwTypeId_Map)

UwResult _uw_map_create(...)
{
    va_list ap;
    va_start(ap);
    UwValue map = uw_create(UwTypeId_Map);
    if (uw_error(&map)) {
        // uw_map_update_ap destroys args on exit, we must do the same
        _uw_destroy_args(ap);
        return uw_move(&map);
    }
    UwValue status = uw_map_update_ap(&map, ap);
    va_end(ap);
    uw_return_if_error(&status);
    return uw_move(&map);
}

static inline unsigned get_map_length(_UwMap* map)
{
    return _uw_array_length(&map->kv_pairs) >> 1;
}

static uint8_t get_item_size(unsigned capacity)
/*
 * Return hash table item size for desired capacity.
 *
 * Index 0 in hash table means no item, so one byte is capable
 * to index 255 items, not 256.
 */
{
    uint8_t item_size = 1;

    for (unsigned n = capacity; n > 255; n >>= 8) {
        item_size++;
    }
    return item_size;
}

/****************************************************************
 * methods for acessing hash table
 */

#define HT_ITEM_METHODS(typename) \
    static unsigned get_ht_item_##typename(struct _UwHashTable* ht, unsigned index) \
    { \
        return ((typename*) (ht->items))[index]; \
    } \
    static void set_ht_item_##typename(struct _UwHashTable* ht, unsigned index, unsigned value) \
    { \
        ((typename*) (ht->items))[index] = (typename) value; \
    }

HT_ITEM_METHODS(uint8_t)
HT_ITEM_METHODS(uint16_t)
HT_ITEM_METHODS(uint32_t)

#if UINT_WIDTH > 32
    HT_ITEM_METHODS(uint64_t)
#endif

/*
 * methods for acessing hash table with any item size
 */
static unsigned get_ht_item(struct _UwHashTable* ht, unsigned index)
{
    uint8_t *item_ptr = &ht->items[index * ht->item_size];
    unsigned result = 0;
    for (uint8_t i = ht->item_size; i > 0; i--) {
        result <<= 8;
        result += *item_ptr++;
    }
    return result;
}

static void set_ht_item(struct _UwHashTable* ht, unsigned index, unsigned value)
{
    uint8_t *item_ptr = &ht->items[(index + 1) * ht->item_size];
    for (uint8_t i = ht->item_size; i > 0; i--) {
        *(--item_ptr) = (uint8_t) value;
        value >>= 8;
    }
}

/****************************************************************
 * implementation
 *
 * Notes on indexes and naming conventions.
 *
 * In hash map we store indexes of key-value pair (named `kv_index`),
 * not the index in kv_pairs array.
 *
 * key_index is the index of key in kv_pais array, suitable for passing to uw_array_get.
 *
 * So,
 *
 * value_index = key_index + 1
 * kv_index = key_index / 2
 */

static bool init_hash_table(UwTypeId type_id, struct _UwHashTable* ht,
                            unsigned old_capacity, unsigned new_capacity)
{
    unsigned old_item_size = get_item_size(old_capacity);
    unsigned old_memsize = old_item_size * old_capacity;

    unsigned new_item_size = get_item_size(new_capacity);
    unsigned new_memsize = new_item_size * new_capacity;

    // reallocate items
    // if map is new, ht is initialized to all zero
    // if map is doubled, this reallocates the block
    if (!_uw_types[type_id]->allocator->reallocate((void**) &ht->items, old_memsize, new_memsize, true, nullptr)) {
        return false;
    }
    memset(ht->items, 0, new_memsize);

    ht->item_size    = new_item_size;
    ht->capacity     = new_capacity;
    ht->hash_bitmask = new_capacity - 1;

    switch (new_item_size) {
        case 1:
            ht->get_item = get_ht_item_uint8_t;
            ht->set_item = set_ht_item_uint8_t;
            break;
        case 2:
            ht->get_item = get_ht_item_uint16_t;
            ht->set_item = set_ht_item_uint16_t;
            break;
        case 4:
            ht->get_item = get_ht_item_uint32_t;
            ht->set_item = set_ht_item_uint32_t;
            break;
#if UINT_WIDTH > 32
        case 8:
            ht->get_item = get_ht_item_uint64_t;
            ht->set_item = set_ht_item_uint64_t;
            break;
#endif
        default:
            ht->get_item = get_ht_item;
            ht->set_item = set_ht_item;
            break;
    }
    return true;
}

static void free_hash_table(UwTypeId type_id, struct _UwHashTable* ht)
{
    unsigned ht_memsize = get_item_size(ht->capacity) * ht->capacity;
    _uw_types[type_id]->allocator->release((void**) &ht->items, ht_memsize);
}

static unsigned lookup(_UwMap* map, UwValuePtr key, unsigned* ht_index, unsigned* ht_offset)
/*
 * Lookup key starting from index = hash(key).
 *
 * Return index of key in kv_pairs or UINT_MAX if hash table has no item matching `key`.
 *
 * If `ht_index` is not `nullptr`: write index of hash table item at which lookup has stopped.
 * If `ht_offset` is not `nullptr`: write the difference from final `ht_index` and initial `ht_index` to `ht_offset`;
 */
{
    struct _UwHashTable* ht = &map->hash_table;
    UwType_Hash index = uw_hash(key) & ht->hash_bitmask;
    unsigned offset = 0;
    do {
        unsigned kv_index = ht->get_item(ht, index);

        if (kv_index == 0) {
            // no entry matching key
            if (ht_index) {
                *ht_index = index;
            }
            if (ht_offset) {
                *ht_offset = offset;
            }
            return UINT_MAX;
        }

        // make index 0-based
        kv_index--;

        UwValuePtr k = &map->kv_pairs.items[kv_index * 2];

        // compare keys
        if (_uw_equal(k, key)) {
            // found key
            if (ht_index) {
                *ht_index = index;
            }
            if (ht_offset) {
                *ht_offset = offset;
            }
            return kv_index * 2;
        }

        // probe next item
        index = (index + 1) & ht->hash_bitmask;
        offset++;

    } while (true);
}

static unsigned set_hash_table_item(struct _UwHashTable* hash_table, unsigned ht_index, unsigned kv_index)
/*
 * Assign `kv_index` to `hash_table` at position `ht_index` & hash_bitmask.
 * If the position is already occupied, try next one.
 *
 * Return ht_index, possibly updated.
 */
{
    do {
        ht_index &= hash_table->hash_bitmask;
        if (hash_table->get_item(hash_table, ht_index)) {
            ht_index++;
        } else {
            hash_table->set_item(hash_table, ht_index, kv_index);
            return ht_index;
        }
    } while (true);
}

static bool _uw_map_expand(UwTypeId type_id, _UwMap* map, unsigned desired_capacity, unsigned ht_offset)
/*
 * Expand map if necessary.
 *
 * ht_offset is a hint, can be 0. If greater or equal 1/4 of capacity, hash table size will be doubled.
 */
{
    // expand array if necessary
    unsigned array_cap = desired_capacity << 1;
    if (array_cap > _uw_array_capacity(&map->kv_pairs)) {
        if (!_uw_array_resize(type_id, &map->kv_pairs, array_cap)) {
            return false;
        }
    }

    struct _UwHashTable* ht = &map->hash_table;

    // check if hash table needs expansion
    unsigned quarter_cap = ht->capacity >> 2;
    if ((ht->capacity >= desired_capacity + quarter_cap) && (ht_offset < quarter_cap)) {
        return true;
    }

    unsigned new_capacity = ht->capacity << 1;
    quarter_cap = desired_capacity >> 2;
    while (new_capacity < desired_capacity + quarter_cap) {
        new_capacity <<= 1;
    }

    if (!init_hash_table(type_id, ht, ht->capacity, new_capacity)) {
        return false;
    }

    // rebuild hash table
    UwValuePtr key_ptr = &map->kv_pairs.items[0];
    unsigned kv_index = 1;  // index is 1-based, zero means unused item in hash table
    unsigned n = _uw_array_length(&map->kv_pairs);
    uw_assert((n & 1) == 0);
    while (n) {
        set_hash_table_item(ht, uw_hash(key_ptr), kv_index);
        key_ptr += 2;
        n -= 2;
        kv_index++;
    }
    return true;
}

static bool update_map(UwValuePtr map, UwValuePtr key, UwValuePtr value)
/*
 * key and value are moved to the internal array
 */
{
    UwTypeId type_id = map->type_id;
    _UwMap* __map = get_data_ptr(map);

    // lookup key in the map

    unsigned ht_offset;
    unsigned key_index = lookup(__map, key, nullptr, &ht_offset);

    if (key_index != UINT_MAX) {
        // found key, update value
        unsigned value_index = key_index + 1;
        UwValuePtr v_ptr = &__map->kv_pairs.items[value_index];
        uw_destroy(v_ptr);
        *v_ptr = uw_move(value);
        return true;
    }

    // key not found, insert

    if (!_uw_map_expand(type_id, __map, get_map_length(__map) + 1, ht_offset)) {
        return false;
    }
    // append key and value
    unsigned kv_index = _uw_array_length(&__map->kv_pairs) >> 1;
    set_hash_table_item(&__map->hash_table, uw_hash(key), kv_index + 1);

    if (!_uw_array_append_item(type_id, &__map->kv_pairs, key, map)) {
        goto panic;
    }
    if (!_uw_array_append_item(type_id, &__map->kv_pairs, value, map)) {
        goto panic;
    }
    return true;

panic:
    uw_panic("Failed append to preallocated array");
}

/****************************************************************
 * Basic interface methods
 */

static void map_fini(UwValuePtr self)
{
    _UwMap* map = get_data_ptr(self);

    _uw_destroy_array(self->type_id, &map->kv_pairs, self);

    struct _UwHashTable* ht = &map->hash_table;
    free_hash_table(self->type_id, ht);

    // call super method, we know the ancestor is Compound
    _uw_types[UwTypeId_Compound]->fini(self);

    // if we did not knew, then:
    // uw_ancestor_of(UwTypeId_Map)->fini(self);
}

static UwResult map_init(UwValuePtr self, void* ctor_args)
{
    // XXX not using ctor_args for now

    // call super method, we know the ancestor is Compound
    UwValue status = _uw_types[UwTypeId_Compound]->init(self, ctor_args);

    // if we did not knew, then:
    // UwValue status = uw_ancestor_of(UwTypeId_Map)->init(self, ctor_args);

    uw_return_if_error(&status);

    // init map

    UwValue error = UwOOM();  // default error is OOM unless some arg is a status

    _UwMap* map = get_data_ptr(self);
    struct _UwHashTable* ht = &map->hash_table;
    ht->items_used = 0;
    if (init_hash_table(self->type_id, ht, 0, UWMAP_INITIAL_CAPACITY)) {
        if (_uw_alloc_array(self->type_id, &map->kv_pairs, UWMAP_INITIAL_CAPACITY * 2)) {
            return UwOK();
        }
    }
    map_fini(self);
    return uw_move(&error);
}

static void map_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    _UwMap* map = get_data_ptr(self);
    for (unsigned i = 0, n = _uw_array_length(&map->kv_pairs); i < n; i++) {
        UwValuePtr item = &map->kv_pairs.items[i];
        _uw_call_hash(item, ctx);
    }
}

static UwResult map_deepcopy(UwValuePtr self)
{
    UwValue dest = UwMap();
    uw_return_if_error(&dest);

    _UwMap* src_map = get_data_ptr(self);
    unsigned map_length    = get_map_length(src_map);

    if (!_uw_map_expand(dest.type_id, get_data_ptr(&dest), map_length, 0)) {
        return UwOOM();
    }

    UwValuePtr kv = &src_map->kv_pairs.items[0];
    for (unsigned i = 0; i < map_length; i++) {{
        UwValue key = uw_clone(kv++);  // okay to clone because keys are already deeply copied
        uw_return_if_error(&key);

        UwValue value = uw_clone(kv++);
        uw_return_if_error(&value);

        if (!update_map(&dest, &key, &value)) {
            // should not happen because the map already resized
            return UwOOM();
        }
    }}
    return uw_move(&dest);
}

static void map_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    _uw_dump_struct_data(fp, self);
    _uw_dump_compound_data(fp, self, next_indent);
    _uw_print_indent(fp, next_indent);

    UwValuePtr value_seen = _uw_on_chain(self, tail);
    if (value_seen) {
        fprintf(fp, "already dumped: %p, data=%p\n", value_seen, value_seen->struct_data);
        return;
    }

    _UwCompoundChain this_link = {
        .prev = tail,
        .value = self
    };

    _UwMap* map = get_data_ptr(self);
    fprintf(fp, "%u items, array items/capacity=%u/%u\n",
            get_map_length(map), _uw_array_length(&map->kv_pairs), _uw_array_capacity(&map->kv_pairs));

    next_indent += 4;
    UwValuePtr item_ptr = &map->kv_pairs.items[0];
    for (unsigned n = _uw_array_length(&map->kv_pairs); n; n -= 2) {

        UwValuePtr key   = item_ptr++;
        UwValuePtr value = item_ptr++;

        _uw_print_indent(fp, next_indent);
        fputs("Key:   ", fp);
        _uw_call_dump(fp, key, 0, next_indent + 7, &this_link);

        _uw_print_indent(fp, next_indent);
        fputs("Value: ", fp);
        _uw_call_dump(fp, value, 0, next_indent + 7, &this_link);
    }

    _uw_print_indent(fp, next_indent);

    struct _UwHashTable* ht = &map->hash_table;
    fprintf(fp, "hash table item size %u, capacity=%u (bitmask %llx)\n",
            ht->item_size, ht->capacity, (unsigned long long) ht->hash_bitmask);

    unsigned hex_width = ht->item_size;
    unsigned dec_width;
    switch (ht->item_size) {
        case 1: dec_width = 3; break;
        case 2: dec_width = 5; break;
        case 3: dec_width = 8; break;
        case 4: dec_width = 10; break;
        case 5: dec_width = 13; break;
        case 6: dec_width = 15; break;
        case 7: dec_width = 17; break;
        default: dec_width = 20; break;
    }
    char fmt[32];
    sprintf(fmt, "%%%ux: %%-%uu", hex_width, dec_width);
    unsigned line_len = 0;
    _uw_print_indent(fp, next_indent);
    for (unsigned i = 0; i < ht->capacity; i++ ) {
        unsigned kv_index = ht->get_item(ht, i);
        fprintf(fp, fmt, i, kv_index);
        line_len += dec_width + hex_width + 4;
        if (line_len < 80) {
            fputs("  ", fp);
        } else {
            fputc('\n', fp);
            line_len = 0;
            _uw_print_indent(fp, next_indent);
        }
    }
    if (line_len) {
        fputc('\n', fp);
    }
}

static UwResult map_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static bool map_is_true(UwValuePtr self)
{
    return get_map_length(get_data_ptr(self));
}

static inline bool map_eq(_UwMap* a, _UwMap* b)
{
    return _uw_array_eq(&a->kv_pairs, &b->kv_pairs);
}

static bool map_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return map_eq(get_data_ptr(self), get_data_ptr(other));
}

static bool map_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_Map) {
            return map_eq(get_data_ptr(self), get_data_ptr(other));
        }
        // check base type
        t = _uw_types[t]->ancestor_id;
        if (t == UwTypeId_Null) {
            return false;
        }
    }
}

UwType _uw_map_type = {
    .id             = UwTypeId_Map,
    .ancestor_id    = UwTypeId_Compound,
    .name           = "Map",
    .allocator      = &default_allocator,

    .create         = _uw_struct_create,
    .destroy        = _uw_compound_destroy,
    .clone          = _uw_struct_clone,
    .hash           = map_hash,
    .deepcopy       = map_deepcopy,
    .dump           = map_dump,
    .to_string      = map_to_string,
    .is_true        = map_is_true,
    .equal_sametype = map_equal_sametype,
    .equal          = map_equal,

    .data_offset    = sizeof(_UwCompoundData),
    .data_size      = sizeof(_UwMap),

    .init           = map_init,
    .fini           = map_fini

    // [UwInterfaceId_RandomAccess] = &map_type_random_access_interface
};

// make sure _UwCompoundData has correct padding
static_assert((sizeof(_UwCompoundData) & (alignof(_UwMap) - 1)) == 0);


/****************************************************************
 * map functions
 */

bool uw_map_update(UwValuePtr map, UwValuePtr key, UwValuePtr value)
{
    uw_assert_map(map);

    UwValue map_key = UwNull();
    map_key = uw_deepcopy(key);  // deep copy key for immutability
    if (uw_error(&map_key)) {
        uw_destroy(&map_key);
        return false;
    }
    UwValue map_value = uw_clone(value);
    if (uw_error(&map_value)) {
        uw_destroy(&map_value);
        return false;
    }
    return update_map(map, &map_key, &map_value);
}

UwResult _uw_map_update_va(UwValuePtr map, ...)
{
    va_list ap;
    va_start(ap);
    UwValue result = uw_map_update_ap(map, ap);
    va_end(ap);
    return uw_move(&result);
}

UwResult uw_map_update_ap(UwValuePtr map, va_list ap)
{
    uw_assert_map(map);
    UwValue error = UwOOM();  // default error is OOM unless some arg is a status
    bool done = false;  // for special case when value is missing
    while (!done) {{
        UwValue key = va_arg(ap, _UwValue);
        if (uw_is_status(&key)) {
            if (uw_va_end(&key)) {
                return UwOK();
            }
            uw_destroy(&error);
            error = uw_move(&key);
            goto failure;
        }
        if (!uw_charptr_to_string_inplace(&key)) {
            goto failure;
        }
        UwValue value = va_arg(ap, _UwValue);
        if (uw_is_status(&value)) {
            if (uw_va_end(&value)) {
                uw_destroy(&value);
                value = UwNull();
                done = true;
            } else {
                uw_destroy(&error);
                error = uw_move(&value);
                goto failure;
            }
        }
        if (!uw_charptr_to_string_inplace(&value)) {
            goto failure;
        }
        if (!update_map(map, &key, &value)) {
            goto failure;
        }
    }}

failure:
    // consume args
    if (!done) {
        _uw_destroy_args(ap);
    }
    return uw_move(&error);
}

bool _uw_map_has_key(UwValuePtr self, UwValuePtr key)
{
    uw_assert_map(self);
    _UwMap* map = get_data_ptr(self);
    return lookup(map, key, nullptr, nullptr) != UINT_MAX;
}

UwResult _uw_map_get(UwValuePtr self, UwValuePtr key)
{
    uw_assert_map(self);
    _UwMap* map = get_data_ptr(self);

    // lookup key in the map
    unsigned key_index = lookup(map, key, nullptr, nullptr);

    if (key_index == UINT_MAX) {
        // key not found
        return UwError(UW_ERROR_KEY_NOT_FOUND);
    }

    // return value
    unsigned value_index = key_index + 1;
    return uw_clone(&map->kv_pairs.items[value_index]);
}

bool _uw_map_del(UwValuePtr self, UwValuePtr key)
{
    uw_assert_map(self);

    _UwMap* map = get_data_ptr(self);

    // lookup key in the map

    unsigned ht_index;
    unsigned key_index = lookup(map, key, &ht_index, nullptr);
    if (key_index == UINT_MAX) {
        // key not found
        return false;
    }

    struct _UwHashTable* ht = &map->hash_table;

    // delete item from hash table
    ht->set_item(ht, ht_index, 0);
    ht->items_used--;

    // delete key-value pair
    _uw_array_del(&map->kv_pairs, key_index, key_index + 2);

    if (key_index + 2 < _uw_array_length(&map->kv_pairs)) {
        // key-value was not the last pair in the array,
        // decrement indexes in the hash table that are greater than index of the deleted pair
        unsigned threshold = (key_index + 2) >> 1;
        threshold++; // kv_indexes in hash table are 1-based
        for (unsigned i = 0; i < ht->capacity; i++) {
            unsigned kv_index = ht->get_item(ht, i);
            if (kv_index >= threshold) {
                ht->set_item(ht, i, kv_index - 1);
            }
        }
    }
    return true;
}

unsigned uw_map_length(UwValuePtr self)
{
    uw_assert_map(self);
    return get_map_length(get_data_ptr(self));
}

bool uw_map_item(UwValuePtr self, unsigned index, UwValuePtr key, UwValuePtr value)
{
    uw_assert_map(self);

    _UwMap* map = get_data_ptr(self);

    index <<= 1;

    if (index < _uw_array_length(&map->kv_pairs)) {
        uw_destroy(key);
        uw_destroy(value);
        *key   = uw_clone(&map->kv_pairs.items[index]);
        *value = uw_clone(&map->kv_pairs.items[index + 1]);
        return true;

    } else {
        return false;
    }
}
