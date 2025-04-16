#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "include/uw_assert.h"

[[noreturn]]
void uw_panic(char* fmt, ...)
{
    va_list ap;
    va_start(ap);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    abort();
}
