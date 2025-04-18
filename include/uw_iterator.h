#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /*
     * Arguments for Iterator constructor
     */
    UwValuePtr iterable;
} UwIteratorCtorArgs;

/****************************************************************
 * LineReader interface
 */

typedef UwResult (*UwMethodStartReadLines)(UwValuePtr self);
/*
 * Prepare to read lines.
 *
 * Basically, any value that implements LineReader interface
 * must be prepared to read lines without making this call.
 *
 * Calling this method again should reset line reader.
 */

typedef UwResult (*UwMethodReadLine)(UwValuePtr self);
/*
 * Read next line.
 */

typedef UwResult (*UwMethodReadLineInPlace)(UwValuePtr self, UwValuePtr line);
/*
 * Truncate line and read next line into it.
 * Return true if read some data, false if error or eof.
 *
 * Rationale: avoid frequent memory allocations that would take place
 * if we returned line by line.
 * On the contrary, existing line is a pre-allocated buffer for the next one.
 */

typedef bool (*UwMethodUnreadLine)(UwValuePtr self, UwValuePtr line);
/*
 * Push line back to the reader.
 * Only one pushback is guaranteed.
 * Return false if pushback buffer is full.
 */

typedef unsigned (*UwMethodGetLineNumber)(UwValuePtr self);
/*
 * Return current line number, 1-based.
 */

typedef void (*UwMethodStopReadLines)(UwValuePtr self);
/*
 * Free internal buffer.
 */

typedef struct {
    UwMethodStartReadLines  start;
    UwMethodReadLine        read_line;
    UwMethodReadLineInPlace read_line_inplace;
    UwMethodUnreadLine      unread_line;
    UwMethodGetLineNumber   get_line_number;
    UwMethodStopReadLines   stop;

} UwInterface_LineReader;


/*
 * Shorthand methods
 */
static inline UwResult uw_start_read_lines (UwValuePtr reader) { return uw_interface(reader->type_id, LineReader)->start(reader); }
static inline UwResult uw_read_line        (UwValuePtr reader) { return uw_interface(reader->type_id, LineReader)->read_line(reader); }
static inline UwResult uw_read_line_inplace(UwValuePtr reader, UwValuePtr line) { return uw_interface(reader->type_id, LineReader)->read_line_inplace(reader, line); }
static inline bool     uw_unread_line      (UwValuePtr reader, UwValuePtr line) { return uw_interface(reader->type_id, LineReader)->unread_line(reader, line); }
static inline unsigned uw_get_line_number  (UwValuePtr reader) { return uw_interface(reader->type_id, LineReader)->get_line_number(reader); }
static inline void     uw_stop_read_lines  (UwValuePtr reader) { uw_interface(reader->type_id, LineReader)->stop(reader); }


#ifdef __cplusplus
}
#endif
