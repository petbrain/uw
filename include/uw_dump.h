#pragma once

#include <stdio.h>

#include <uw_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************
 * Dump functions
 */

void _uw_print_indent(FILE* fp, int indent);
void _uw_dump_start(FILE* fp, UwValuePtr value, int indent);
void _uw_dump_struct_data(FILE* fp, UwValuePtr value);
void _uw_dump_compound_data(FILE* fp, UwValuePtr value, int indent);
void _uw_dump(FILE* fp, UwValuePtr value, int first_indent, int next_indent, _UwCompoundChain* tail);

void uw_dump(FILE* fp, UwValuePtr value);

static inline void _uw_call_dump(FILE* fp, UwValuePtr value, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    uw_typeof(value)->dump(value, fp, first_indent, next_indent, tail);
}

#ifdef __cplusplus
}
#endif
