#pragma once

#include <stdarg.h>
#include <uchar.h>

#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern UwTypeId UwTypeId_File;

/****************************************************************
 * File interface
 */

typedef UwResult (*UwMethodOpenFile)         (UwValuePtr self, UwValuePtr file_name, int flags, mode_t mode);
typedef void     (*UwMethodCloseFile)        (UwValuePtr self);
typedef bool     (*UwMethodSetFileDescriptor)(UwValuePtr self, int fd);
typedef UwResult (*UwMethodGetFileName)      (UwValuePtr self);
typedef bool     (*UwMethodSetFileName)      (UwValuePtr self, UwValuePtr file_name);

// XXX other fd operation: seek, tell, etc.

typedef struct {
    UwMethodOpenFile          open;
    UwMethodCloseFile         close;  // only if opened with `open`, don't close one assigned by `set_fd`, right?
    UwMethodSetFileDescriptor set_fd;
    UwMethodGetFileName       get_name;
    UwMethodSetFileName       set_name;

} UwInterface_File;

/****************************************************************
 * FileReader interface
 */

typedef UwResult (*UwMethodReadFile)(UwValuePtr self, void* buffer, unsigned buffer_size, unsigned* bytes_read);

typedef struct {
    UwMethodReadFile read;

} UwInterface_FileReader;

/****************************************************************
 * FileWriter interface
 */

typedef UwResult (*UwMethodWriteFile)(UwValuePtr self, void* data, unsigned size, unsigned* bytes_written);

// XXX add truncate method

typedef struct {
    UwMethodWriteFile write;

} UwInterface_FileWriter;

/****************************************************************
 * Shorthand functions
 */

#define uw_file_open(file_name, flags, mode) _Generic((file_name), \
             char*: _uw_file_open_u8_wrapper,  \
          char8_t*: _uw_file_open_u8,          \
         char32_t*: _uw_file_open_u32,         \
        UwValuePtr: _uw_file_open              \
    )((file_name), (flags), (mode))

UwResult _uw_file_open(UwValuePtr file_name, int flags, mode_t mode);

static inline UwResult _uw_file_open_u8  (char8_t*  file_name, int flags, mode_t mode) { __UWDECL_CharPtr  (fname, file_name); return _uw_file_open(&fname, flags, mode); }
static inline UwResult _uw_file_open_u32 (char32_t* file_name, int flags, mode_t mode) { __UWDECL_Char32Ptr(fname, file_name); return _uw_file_open(&fname, flags, mode); }

static inline UwResult _uw_file_open_u8_wrapper(char* file_name, int flags, mode_t mode)
{
    return _uw_file_open_u8((char8_t*) file_name, flags, mode);
}

static inline void     uw_file_close   (UwValuePtr file)         { uw_interface(file->type_id, File)->close(file); }
static inline bool     uw_file_set_fd  (UwValuePtr file, int fd) { return uw_interface(file->type_id, File)->set_fd(file, fd); }
static inline UwResult uw_file_get_name(UwValuePtr file)         { return uw_interface(file->type_id, File)->get_name(file); }
static inline bool     uw_file_set_name(UwValuePtr file, UwValuePtr file_name)  { return uw_interface(file->type_id, File)->set_name(file, file_name); }

static inline UwResult uw_file_read(UwValuePtr file, void* buffer, unsigned buffer_size, unsigned* bytes_read)
{
    return uw_interface(file->type_id, FileReader)->read(file, buffer, buffer_size, bytes_read);
}

static inline UwResult uw_file_write(UwValuePtr file, void* data, unsigned size, unsigned* bytes_written)
{
    return uw_interface(file->type_id, FileWriter)->write(file, data, size, bytes_written);
}


/****************************************************************
 * Miscellaneous functions
 */

UwResult uw_file_size(UwValuePtr file_name);
/*
 * Return file size as Unsigned or Status if error.
 */


/****************************************************************
 * Path functions, probably should be separated
 */

UwResult uw_basename(UwValuePtr filename);
UwResult uw_dirname(UwValuePtr filename);

/*
 * uw_path generic macro allows passing arguments
 * either by value or by reference.
 * When passed by value, the function destroys them
 * .
 * CAVEAT: DO NOT PASS LOCAL VARIABLES BY VALUES!
 */

#define uw_path(part, ...) _Generic((part),  \
        _UwValue:   _uw_path_v,  \
        UwValuePtr: _uw_path_p   \
    )((part) __VA_OPT__(,) __VA_ARGS__,  \
        _Generic((part),  \
            _UwValue:   UwVaEnd(),  \
            UwValuePtr: nullptr     \
        ))

UwResult _uw_path_v(...);
UwResult _uw_path_p(...);

#ifdef __cplusplus
}
#endif
