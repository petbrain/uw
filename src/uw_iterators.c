#include "include/uw.h"

unsigned UwInterfaceId_LineReader = UINT_MAX;

[[ gnu::constructor ]]
void _uw_init_iterators()
{
    if (UwInterfaceId_LineReader == UINT_MAX) { UwInterfaceId_LineReader = uw_register_interface("LineReader", UwInterface_LineReader); }
}
