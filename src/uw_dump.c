#include "include/uw.h"

void _uw_dump_start(FILE* fp, UwValuePtr value, int indent)
{
    char* type_name = uw_typeof(value)->name;

    _uw_print_indent(fp, indent);
    fprintf(fp, "%p", value);
    if (type_name == nullptr) {
        fprintf(fp, " BAD TYPE %d", value->type_id);
    } else {
        fprintf(fp, " %s (type id: %d)", type_name, value->type_id);
    }
}

void _uw_print_indent(FILE* fp, int indent)
{
    for (int i = 0; i < indent; i++ ) {
        fputc(' ', fp);
    }
}

void _uw_dump(FILE* fp, UwValuePtr value, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    if (value == nullptr) {
        _uw_print_indent(fp, first_indent);
        fprintf(fp, "nullptr\n");
        return;
    }
    UwMethodDump fn_dump = uw_typeof(value)->dump;
    uw_assert(fn_dump != nullptr);
    fn_dump(value, fp, first_indent, next_indent, tail);
}

void uw_dump(FILE* fp, UwValuePtr value)
{
    _uw_dump(fp, value, 0, 0, nullptr);
}
