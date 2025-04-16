#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    _UwValue iterable;  // cloned iterable value
} _UwIterator;

#define get_iterator_data_ptr(value)  ((_UwIterator*) _uw_get_data_ptr((value), UwTypeId_Iterator))

#ifdef __cplusplus
}
#endif
