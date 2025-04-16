#include <limits.h>

#include "include/uw.h"
#include "src/uw_interfaces_internal.h"

unsigned UwInterfaceId_LineReader = UINT_MAX;

[[ gnu::constructor ]]
void _uw_init_iterators()
{
    _uw_init_interfaces();

    if (UwInterfaceId_LineReader == UINT_MAX) { UwInterfaceId_LineReader = uw_register_interface("LineReader", UwInterface_LineReader); }
}
