#include "src/uw_struct_internal.h"

static UwResult call_init(UwValuePtr self, void* ctor_args, UwTypeId type_id)
{
    if (type_id == UwTypeId_Struct) {
        // reached the bottom
        return UwOK();
    }

    UwType* type = _uw_types[type_id];
    UwTypeId ancestor_id = type->ancestor_id;

    UwValue result = call_init(self, ctor_args, ancestor_id);
    if (uw_ok(&result)) {
        // initialized all ancestors, okay to call init for this type_id
        UwMethodInit init = type->init;
        if (init) {
            result = init(self, ctor_args);
            if (uw_error(&result)) {
                // ancestor init failed, call fini for already called init
                while (type->id != UwTypeId_Struct) {
                    UwMethodFini fini = type->fini;
                    if (fini) {
                        fini(self);
                    }
                    type = uw_ancestor_of(type->id);
                }
            }
        }
    }
    return uw_move(&result);
}

UwResult _uw_struct_alloc(UwValuePtr self, void* ctor_args)
{
    UwType* type = uw_typeof(self);

    // allocate memory

    unsigned memsize = type->data_offset + type->data_size;
    self->struct_data = type->allocator->allocate(memsize, true);
    if (!self->struct_data) {
        return UwOOM();
    }

    self->struct_data->refcount = 1;

    return call_init(self, ctor_args, self->type_id);
}

void _uw_struct_release(UwValuePtr self)
{
    // call fini methods

    UwType* type = uw_typeof(self);
    while (type->id != UwTypeId_Struct) {
        UwMethodFini fini = type->fini;
        if (fini) {
            fini(self);
        }
        type = uw_ancestor_of(type->id);
    }

    // release memory

    type = uw_typeof(self);

    unsigned memsize = type->data_offset + type->data_size;
    type->allocator->release((void**) &self->struct_data, memsize);

    // reset value to Null
    self->type_id = UwTypeId_Null;
}

UwResult _uw_struct_create(UwTypeId type_id, void* ctor_args)
{
    UwValue result = {
        .type_id = type_id
    };
    uw_expect_ok( _uw_struct_alloc(&result, ctor_args) );
    return uw_move(&result);
}

void _uw_struct_destroy(UwValuePtr self)
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
    _uw_struct_release(self);
}

UwResult _uw_struct_clone(UwValuePtr self)
{
    if (self->struct_data) {
        self->struct_data->refcount++;
    }
    return *self;
}

void _uw_struct_hash(UwValuePtr self, UwHashContext* ctx)
{
    _uw_hash_uint64(ctx, self->type_id);
    _uw_hash_uint64(ctx, (uint64_t) self->struct_data);
}

UwResult _uw_struct_deepcopy(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

void _uw_dump_struct_data(FILE* fp, UwValuePtr value)
{
    if (value->struct_data) {
        fprintf(fp, " data=%p refcount=%u;", value->struct_data, value->struct_data->refcount);
    } else {
        fprintf(fp, " data=NULL;");
    }
}

static void struct_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _uw_dump_start(fp, self, first_indent);
    _uw_dump_struct_data(fp, self);
}

UwResult _uw_struct_to_string(UwValuePtr self)
{
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

bool _uw_struct_is_true(UwValuePtr self)
{
    return false;
}

bool _uw_struct_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    // basic Structs are empty and empty always equals empty
    return true;
}

bool _uw_struct_equal(UwValuePtr self, UwValuePtr other)
{
    UwTypeId t = other->type_id;
    for (;;) {
        if (t == UwTypeId_Struct) {
            // basic Structs are empty and empty always equals empty
            return true;
        }
        // check base type
        t = _uw_types[t]->ancestor_id;
        if (t == UwTypeId_Null) {
            return false;
        }
    }
}

UwType _uw_struct_type = {
    .id             = UwTypeId_Struct,
    .ancestor_id    = UwTypeId_Null,  // no ancestor
    .name           = "Struct",
    .allocator      = &default_allocator,

    .create         = _uw_struct_create,
    .destroy        = _uw_struct_destroy,
    .clone          = _uw_struct_clone,
    .hash           = _uw_struct_hash,
    .deepcopy       = _uw_struct_deepcopy,
    .dump           = struct_dump,
    .to_string      = _uw_struct_to_string,
    .is_true        = _uw_struct_is_true,
    .equal_sametype = _uw_struct_equal_sametype,
    .equal          = _uw_struct_equal,

    .data_offset    = 0,
    .data_size      = sizeof(_UwStructData),
};
