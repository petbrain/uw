#include "include/uw.h"
#include "src/uw_array_internal.h"
#include "src/uw_interfaces_internal.h"
#include "src/uw_iterator_internal.h"
#include "src/uw_struct_internal.h"

typedef struct {
    bool iteration_in_progress;
    unsigned index;
    int increment;
    unsigned line_number;  // does not match index because line reader may skip non-strings
} _UwArrayIterator;

#define get_data_ptr(value)  ((_UwArrayIterator*) _uw_get_data_ptr((value), UwTypeId_ArrayIterator))


static bool start_iteration(UwValuePtr self)
{
    _UwArrayIterator* data = get_data_ptr(self);
    if (data->iteration_in_progress) {
        return false;
    }
    data->index = 0;
    data->increment = 1;
    data->iteration_in_progress = true;

    // increment itercount
    _UwIterator* iter_data = get_iterator_data_ptr(self);
    _UwArray* array_data = get_array_data_ptr(&iter_data->iterable);
    array_data->itercount++;
    return true;
}

static void stop_iteration(UwValuePtr self)
{
    _UwArrayIterator* data = get_data_ptr(self);
    if (data->iteration_in_progress) {
        _UwIterator* iter_data = get_iterator_data_ptr(self);
        _UwArray* array_data = get_array_data_ptr(&iter_data->iterable);
        array_data->itercount--;
        data->iteration_in_progress = false;
    }
}

/****************************************************************
 * LineReader interface
 */

static UwResult start_read_lines(UwValuePtr self)
{
    if (!start_iteration(self)) {
        return UwError(UW_ERROR_ITERATION_IN_PROGRESS);
    }
    _UwArrayIterator* data = get_data_ptr(self);
    data->line_number = 1;
    return UwOK();
}

static UwResult read_line(UwValuePtr self)
{
    _UwArrayIterator* data = get_data_ptr(self);
    _UwIterator* iter_data = get_iterator_data_ptr(self);
    _UwArray* array_data = get_array_data_ptr(&iter_data->iterable);

    while (data->index < array_data->length) {{
        UwValue result = uw_clone(&array_data->items[data->index++]);
        if (uw_is_string(&result)) {
            data->line_number++;
            return uw_move(&result);
        }
    }}
    return UwError(UW_ERROR_EOF);
}

static UwResult read_line_inplace(UwValuePtr self, UwValuePtr line)
{
    uw_destroy(line);

    _UwArrayIterator* data = get_data_ptr(self);
    _UwIterator* iter_data = get_iterator_data_ptr(self);
    _UwArray* array_data = get_array_data_ptr(&iter_data->iterable);

    while (data->index < array_data->length) {{
        UwValue result = uw_clone(&array_data->items[data->index++]);
        if (uw_is_string(&result)) {
            *line = uw_move(&result);
            data->line_number++;
            return UwOK();
        }
    }}
    return UwError(UW_ERROR_EOF);
}

static bool unread_line(UwValuePtr self, UwValuePtr line)
{
    _UwArrayIterator* data = get_data_ptr(self);
    _UwIterator* iter_data = get_iterator_data_ptr(self);
    _UwArray* array_data = get_array_data_ptr(&iter_data->iterable);

    // simply decrement iteration index, that's equivalent to pushback
    while (data->index) {
        if (uw_is_string(&array_data->items[data->index--])) {
            data->line_number--;
            return true;
        }
    }
    return false;
}

static unsigned get_line_number(UwValuePtr self)
{
    _UwArrayIterator* data = get_data_ptr(self);
    return data->line_number;
}

static void stop_read_lines(UwValuePtr self)
{
    stop_iteration(self);
}

static UwInterface_LineReader line_reader_interface = {
    .start             = start_read_lines,
    .read_line         = read_line,
    .read_line_inplace = read_line_inplace,
    .get_line_number   = get_line_number,
    .unread_line       = unread_line,
    .stop              = stop_read_lines
};

/****************************************************************
 * ArrayIterator type
 */

static void ari_fini(UwValuePtr self)
{
    // force stop iteration
    stop_iteration(self);
}

static UwType array_iterator_type;

UwTypeId UwTypeId_ArrayIterator = 0;

[[ gnu::constructor ]]
static void init_array_iterator_type()
{
    if (UwTypeId_ArrayIterator == 0) {
        UwTypeId_ArrayIterator = uw_subtype(
            &array_iterator_type, "ArrayIterator", UwTypeId_Iterator, _UwArrayIterator,
            UwInterfaceId_LineReader, &line_reader_interface
        );
        array_iterator_type.fini = ari_fini;
    }
}
