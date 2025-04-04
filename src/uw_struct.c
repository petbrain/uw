#include "src/uw_struct_internal.h"

UwResult _uw_struct_alloc(UwValuePtr self, void* ctor_args)
{
    UwType* type = uw_typeof(self);

    // allocate memory

    unsigned memsize = type->data_offset + type->data_size;
    self->struct_data = type->allocator->allocate(memsize, true);
    if (!self->struct_data) {
        return UwOOM();
    }

    // call init method

    UwValue status = type->init(self, ctor_args);
    uw_return_if_error(&status);
    return UwOK();
}

void _uw_struct_release(UwValuePtr self)
{
    UwType* type = uw_typeof(self);

    // call fini method

    type->fini(self);

    // release memory

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
    UwValue status = _uw_struct_alloc(&result, ctor_args);
    uw_return_if_error(&status);
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

UwResult _uw_struct_init(UwValuePtr self, void* ctor_args)
{
    self->struct_data->refcount = 1;
    return UwOK();
}

void _uw_struct_fini(UwValuePtr self)
{
    // no op

    // Most built-in types, i.e. Status, File, Compound, etc.
    // are aware it's no op and don't call it.

    // Make sure they'll do if any code will be added here ever.
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

    .init           = _uw_struct_init,
    .fini           = _uw_struct_fini
};
