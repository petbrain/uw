#include <string.h>

#include "src/uw_compound_internal.h"
#include "src/uw_struct_internal.h"


UwValuePtr _uw_on_chain(UwValuePtr value, _UwCompoundChain* tail)
{
    while (tail) {
        if (value->struct_data == tail->value->struct_data) {
            return tail->value;
        }
        tail = tail->prev;
    }
    return nullptr;
}

static inline _UwParentsChunk* get_parents_list(_UwCompoundData* cdata)
/*
 * Return pointer to the list of parents with `using_parents_list` flag cleared.
 */
{
    ptrdiff_t ptr = (ptrdiff_t) cdata->parents_list;
    ptr &= ~1;
    return (_UwParentsChunk*) ptr;
}

bool _uw_is_embraced(UwValuePtr value)
{
    _UwCompoundData* cdata = _uw_compound_data_ptr(value);

    if (cdata->using_parents_list) {
        return true;
    }
    if (cdata->parents[0]) {
        return true;
    }
    if (cdata->parents[1]) {
        return true;
    }
    return false;
}

bool _uw_adopt(UwValuePtr parent, UwValuePtr child)
{
    _UwCompoundData* parent_cdata = _uw_compound_data_ptr(parent);
    _UwCompoundData* child_cdata = _uw_compound_data_ptr(child);

    if (parent_cdata == child_cdata) {
success:
        child_cdata->struct_data.refcount--;
        return true;;
    }
    if (child_cdata->using_parents_list) {

        // find parent_cdata on the list
        // also, find available position for insertion
        _UwParentsChunk* chunk_ptr = get_parents_list(child_cdata);
        _UwParentsChunk* avail_chunk = nullptr;
        unsigned avail_pos;
        for (unsigned n = child_cdata->num_parents_chunks; n; n--, chunk_ptr++) {
            _UwCompoundData** parent_ptr = chunk_ptr->parents;
            for (unsigned i = 0; i < UW_PARENTS_CHUNK_SIZE; i++, parent_ptr++) {
                if (*parent_ptr == parent_cdata) {
                    chunk_ptr->parents_refcount[i]++;
                    goto success;
                }
                if (*parent_ptr == nullptr) {
                    if (avail_chunk == nullptr) {
                        avail_chunk = chunk_ptr;
                        avail_pos = i;
                    }
                }
            }
        }
        // append parent_cdata to the list
        if (avail_chunk) {
            avail_chunk->parents[avail_pos] = parent_cdata;
            avail_chunk->parents_refcount[avail_pos]++;
            goto success;
        }
        // extend list
        unsigned old_size = child_cdata->num_parents_chunks * sizeof(_UwParentsChunk);
        unsigned new_size = old_size + sizeof(_UwParentsChunk);
        _UwParentsChunk* parents_list = get_parents_list(child_cdata);
        if (!default_allocator.reallocate((void**) &parents_list, old_size, new_size, true, nullptr)) {
            return false;
        }
        child_cdata->parents_list = parents_list;
        child_cdata->using_parents_list = true;
        _UwParentsChunk* new_chunk = &parents_list[child_cdata->num_parents_chunks];
        child_cdata->num_parents_chunks++;
        new_chunk->parents[0] = parent_cdata;
        new_chunk->parents_refcount[0] = 1;
        goto success;

    } else {
        // the list is not allocated yet, check embedded one

        if (child_cdata->parents[0] == parent_cdata) {
            child_cdata->parents_refcount[0]++;
            goto success;
        }
        if (child_cdata->parents[1] == parent_cdata) {
            child_cdata->parents_refcount[1]++;
            goto success;
        }
        // parent_cdata is not on the list yet, try to append to embedded one
        if (child_cdata->parents[0] == nullptr) {
            child_cdata->parents[0] = parent_cdata;
            child_cdata->parents_refcount[0] = 1;
            goto success;
        }
        if (child_cdata->parents[1] == nullptr) {
            child_cdata->parents[1] = parent_cdata;
            child_cdata->parents_refcount[1] = 1;
            goto success;
        }
        // allocate list
        _UwParentsChunk* chunk_ptr = default_allocator.allocate(sizeof(_UwParentsChunk), true);
        if (!chunk_ptr) {
            return false;
        }
        child_cdata->parents_list = chunk_ptr;
        child_cdata->using_parents_list = true;
        child_cdata->num_parents_chunks = 1;
        chunk_ptr->parents[0] = parent_cdata;
        chunk_ptr->parents_refcount[0] = 1;
        goto success;
    }
}

static void shrink_parents_list(_UwCompoundData* child, _UwParentsChunk* chunk_ptr, unsigned chunks_left)
/*
 * `chunk_ptr` points to a chunk from which a pointer was just removed
 * Check how many pointers left in that chunk and shrink the list if possible.
 * `chunks_left` is the number of chunks on the list after `chunk_ptr`.
 */
{
    unsigned num_items_in_chunk = 0;
    _UwCompoundData** parent_ptr = chunk_ptr->parents;
    for(unsigned i = 0; i < UW_PARENTS_CHUNK_SIZE; i++, parent_ptr++) {
        if (*parent_ptr) {
            num_items_in_chunk++;
        }
    }
    if (num_items_in_chunk == 2 && child->num_parents_chunks == 1) {
        // last chunk with exactly 2 iems --> deallocate list
        _UwParentsChunk* parents_list = get_parents_list(child);
        child->num_parents_chunks = 0;
        parent_ptr = parents_list->parents;
        for (unsigned i = 0, j = 0; i < UW_PARENTS_CHUNK_SIZE; i++, parent_ptr++) {
            if (*parent_ptr) {
                child->parents[j] = *parent_ptr;
                child->parents_refcount[j] = parents_list->parents_refcount[i];
                j++;
            }
        }
        default_allocator.release((void**) &parents_list, sizeof(_UwParentsChunk));  // see above, num_parents_chunks was 1
        return;
    }
    // delete chunk
    if (chunks_left) {
        memmove(chunk_ptr, chunk_ptr + 1, chunks_left * sizeof(_UwParentsChunk));
    }
    // shrink
    unsigned old_size = child->num_parents_chunks * sizeof(_UwParentsChunk);
    unsigned new_size = old_size - sizeof(_UwParentsChunk);
    _UwParentsChunk* parents_list = get_parents_list(child);
    default_allocator.reallocate((void**) &parents_list, old_size, new_size, false, nullptr);
    child->num_parents_chunks--;
}

bool _uw_abandon(UwValuePtr parent, UwValuePtr child)
{
    _UwCompoundData* parent_cdata = _uw_compound_data_ptr(parent);
    _UwCompoundData* child_cdata = _uw_compound_data_ptr(child);

    if (parent_cdata == child_cdata) {
        return true;
    }
    if (child_cdata->using_parents_list) {

        // find parent_cdata on the list
        _UwParentsChunk* chunk_ptr = get_parents_list(child_cdata);
        for (unsigned n = child_cdata->num_parents_chunks; n; n--, chunk_ptr++) {
            _UwCompoundData** parent_ptr = chunk_ptr->parents;
            for (unsigned i = 0; i < UW_PARENTS_CHUNK_SIZE; i++, parent_ptr++) {
                if (*parent_ptr == parent_cdata) {
                    if (--chunk_ptr->parents_refcount[i]) {
                        return false;  // not fully abandoned yet
                    }
                    // delete parent_cdata from list
                    *parent_ptr = nullptr;
                    shrink_parents_list(child_cdata, chunk_ptr, n - 1);
                    return true;
                }
            }
        }
    } else {

        // check embedded list
        if (child_cdata->parents[0] == parent_cdata) {
            if (--child_cdata->parents_refcount[0]) {
                return false;  // not fully abandoned yet
            }
            child_cdata->parents[0] = nullptr;
            return true;
        }
        if (child_cdata->parents[1] == parent_cdata) {
            if (--child_cdata->parents_refcount[1]) {
                return false;  // not fully abandoned yet
            }
            child_cdata->parents[1] = nullptr;
            return true;
        }
    }
    // parent_cdata not found, this means it's already abandoned
    return true;
}

// bit flags for the result of cyclic reference checker
#define HAVE_CYCLIC_REFS  1  // cyclic references are present
#define NONZERO_REFCOUNT  2  // reference count of some data in chain is nonzero

// forward declaration
static unsigned check_cyclic_refs(_UwCompoundData* first, _UwCompoundData* cdata);

static unsigned check_parent_link(_UwCompoundData* first, _UwCompoundData* parent)
/*
 * helper function for check_cyclic_refs
 */
{
    unsigned result = 0;  // bit flags: HAVE_CYCLIC_REFS and NONZERO_REFCOUNT

    if (parent->struct_data.refcount) {
        result |= NONZERO_REFCOUNT;
    }
    if (parent == first) {
        result |= HAVE_CYCLIC_REFS;
    } else {
        result |= check_cyclic_refs(first, parent);
    }
    return result;
}

static unsigned check_cyclic_refs(_UwCompoundData* first, _UwCompoundData* cdata)
/*
 * Check if any parent of `cdata` is equal to `first` and whether its refcount is zero.
 */
{
    unsigned result = 0;  // bit flags: HAVE_CYCLIC_REFS and NONZERO_REFCOUNT

    if (cdata->using_parents_list) {

        // process list of parents
        _UwParentsChunk* chunk_ptr = get_parents_list(cdata);
        for (unsigned n = cdata->num_parents_chunks; n; n--, chunk_ptr++) {
            _UwCompoundData** parent_ptr = chunk_ptr->parents;
            for (unsigned i = 0; i < UW_PARENTS_CHUNK_SIZE; i++, parent_ptr++) {
                _UwCompoundData* parent = *parent_ptr;
                if (parent) {
                    result |= check_parent_link(first, parent);
                }
            }
        }
    } else {

        // check embedded list
        if (cdata->parents[0]) {
            result |= check_parent_link(first, cdata->parents[0]);
        }
        if (cdata->parents[1]) {
            result |= check_parent_link(first, cdata->parents[1]);
        }
    }
    return result;
}

bool _uw_need_break_cyclic_refs(UwValuePtr value)
{
    _UwCompoundData* cdata = _uw_compound_data_ptr(value);
    return check_cyclic_refs(cdata, cdata) == HAVE_CYCLIC_REFS;
}

void _uw_dump_compound_data(FILE* fp, UwValuePtr value, int indent)
{
    _UwCompoundData* cdata = _uw_compound_data_ptr(value);

    if (cdata->using_parents_list) {
        fprintf(fp, " compound, %u chunks:\n", cdata->num_parents_chunks);

        unsigned line_length = 0;  // rough
        _UwParentsChunk* chunk_ptr = get_parents_list(cdata);
        for (unsigned n = cdata->num_parents_chunks; n; n--, chunk_ptr++) {
            _UwCompoundData** parent_ptr = chunk_ptr->parents;
            for (unsigned i = 0; i < UW_PARENTS_CHUNK_SIZE; i++, parent_ptr++) {
                _UwCompoundData* parent = *parent_ptr;
                if (parent) {
                    char str[128];
                    line_length += sprintf(str, "parent %p refcount %u",
                                           parent, chunk_ptr->parents_refcount[i]);
                    fputs(str, fp);
                    if (line_length >= 80) {
                        fputc('\n', fp);
                        line_length = 0;
                    } else {
                        fputc('\t', fp);
                    }
                }
            }
        }
        if (line_length) {
            fputc('\n', fp);
        }
    } else {
        fprintf(fp, " compound; embedded:\n");
        _uw_print_indent(fp, indent);
        fprintf(fp, "parent %p refcount %u; parent %p refcount %u\n",
                cdata->parents[0], cdata->parents_refcount[0],
                cdata->parents[1], cdata->parents_refcount[1]);
    }
}

void _uw_compound_destroy(UwValuePtr self)
{
    _UwStructData* struct_data = self->struct_data;

    if (!struct_data) {
        return;
    }
    if (struct_data->refcount) {
        struct_data->refcount--;
    }
    if (struct_data->refcount) {
        return;
    }

    _UwCompoundData* cdata = _uw_compound_data_ptr(self);

    if (cdata->destroying) {
        return;
    }
    if (_uw_is_embraced(self)) {

        // we have a parent, check if there is a cyclic reference
        if (!_uw_need_break_cyclic_refs(self)) {
            // can't destroy self, because it's still a part of some object
            return;
        }
        // there are breakable cyclic references, okay to destroy
    }
    cdata->destroying = true;

    _uw_struct_release(self);
}

static void compound_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    _uw_dump_struct_data(fp, self);
    _uw_dump_compound_data(fp, self, next_indent);
}

static bool compound_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_Compound) {
            // basic Compounds are empty and empty always equals empty
            return true;
        } else {
            // check base type
            t = _uw_types[t]->ancestor_id;
            if (t == UwTypeId_Null) {
                return false;
            }
        }
    }
}

static void compound_fini(UwValuePtr self)
{
    _UwCompoundData* cdata = _uw_compound_data_ptr(self);

    if (cdata->using_parents_list) {
        _UwParentsChunk* chunk_ptr = get_parents_list(cdata);
        default_allocator.release((void**) &chunk_ptr, cdata->num_parents_chunks * sizeof(_UwParentsChunk));
        cdata->parents[0] = nullptr;
        cdata->parents[1] = nullptr;
    }

    // do not call Struct.fini because it's a no op
}

UwType _uw_compound_type = {
    .id             = UwTypeId_Compound,
    .ancestor_id    = UwTypeId_Struct,
    .name           = "Compound",
    .allocator      = &default_allocator,
    .create         = _uw_struct_create,
    .destroy        = _uw_compound_destroy,
    .clone          = _uw_struct_clone,
    .hash           = _uw_struct_hash,
    .deepcopy       = _uw_struct_deepcopy,
    .dump           = compound_dump,
    .to_string      = _uw_struct_to_string,
    .is_true        = _uw_struct_is_true,
    .equal_sametype = _uw_struct_equal_sametype,
    .equal          = compound_equal,

    .data_offset    = 0,
    .data_size      = sizeof(_UwCompoundData),

    .init           = _uw_struct_init,
    .fini           = compound_fini
};
