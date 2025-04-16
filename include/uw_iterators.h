#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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


#ifdef __cplusplus
}
#endif
