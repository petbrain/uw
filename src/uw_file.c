#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "include/uw.h"
#include "src/uw_string_internal.h"
#include "src/uw_struct_internal.h"

#define LINE_READER_BUFFER_SIZE  4096  // typical filesystem block size

typedef struct {
    int fd;               // file descriptor
    bool is_external_fd;  // fd is set by `set_fd` and should not be closed
    int error;            // errno, set by `open`
    _UwValue name;

    // line reader data
    // XXX okay for now, revise later
    char8_t* buffer;    // this has fixed LINE_READER_BUFFER_SIZE
    unsigned position;  // current position in the buffer when scanning for line break
    unsigned data_size; // size of data in the buffer
    char8_t  partial_utf8[4];  // UTF-8 sequence may span adjacent reads
    unsigned partial_utf8_len;
    _UwValue pushback;  // for unread_line
    unsigned line_number;
} _UwFile;

#define get_data_ptr(value)  ((_UwFile*) _uw_get_data_ptr((value), UwTypeId_File))

// forward declarations
static void file_close(UwValuePtr self);
static UwResult read_line_inplace(UwValuePtr self, UwValuePtr line);

/****************************************************************
 * Basic interface methods
 */

static UwResult file_init(UwValuePtr self, void* ctor_args)
{
    _UwFile* f = get_data_ptr(self);
    f->fd = -1;
    f->name = UwNull();
    f->pushback = UwNull();
    return UwOK();
}

static void file_fini(UwValuePtr self)
{
    file_close(self);
}

static void file_hash(UwValuePtr self, UwHashContext* ctx)
{
    // it's not a hash of entire file content!

    _UwFile* f = get_data_ptr(self);

    _uw_hash_uint64(ctx, self->type_id);

    // XXX
    _uw_call_hash(&f->name, ctx);
    _uw_hash_uint64(ctx, f->fd);
    _uw_hash_uint64(ctx, f->is_external_fd);
}

static UwResult file_deepcopy(UwValuePtr self)
{
    // XXX duplicate fd?
    return UwError(UW_ERROR_NOT_IMPLEMENTED);
}

static void file_dump(UwValuePtr self, FILE* fp, int first_indent, int next_indent, _UwCompoundChain* tail)
{
    _UwFile* f = get_data_ptr(self);

    _uw_dump_start(fp, self, first_indent);
    _uw_dump_struct_data(fp, self);

    if (uw_is_string(&f->name)) {
        UW_CSTRING_LOCAL(file_name, &f->name);
        fprintf(fp, " name: %s", file_name);
    } else {
        fprintf(fp, " name: Null");
    }
    fprintf(fp, " fd: %d", f->fd);
    if (f->is_external_fd) {
        fprintf(fp, " (external)");
    }
    fputc('\n', fp);
}

static bool file_equal_sametype(UwValuePtr self, UwValuePtr other)
{
    return false;
}

static bool file_equal(UwValuePtr self, UwValuePtr other)
{
    return false;
}

/****************************************************************
 * File interface methods
 */

static UwResult file_open(UwValuePtr self, UwValuePtr file_name, int flags, mode_t mode)
{
    _UwFile* f = get_data_ptr(self);

    if (f->fd != -1) {
        return UwError(UW_ERROR_FILE_ALREADY_OPENED);
    }

    // open file
    UW_CSTRING_LOCAL(filename_cstr, file_name);
    do {
        f->fd = open(filename_cstr, flags, mode);
    } while (f->fd == -1 && errno == EINTR);

    if (f->fd == -1) {
        f->error = errno;
        return UwErrno(errno);
    }

    // set file name
    uw_destroy(&f->name);
    f->name = uw_clone(file_name);

    f->is_external_fd = false;
    f->line_number = 0;

    uw_destroy(&f->pushback);

    return UwOK();
}

static void file_close(UwValuePtr self)
{
    _UwFile* f = get_data_ptr(self);

    if (f->fd != -1 && !f->is_external_fd) {
        close(f->fd);
    }
    f->fd = -1;
    f->error = 0;
    uw_destroy(&f->name);

    if (f->buffer) {
        free(f->buffer);
        f->buffer = nullptr;
    }
    uw_destroy(&f->pushback);
}

static bool file_set_fd(UwValuePtr self, int fd)
{
    _UwFile* f = get_data_ptr(self);

    if (f->fd != -1) {
        // fd already set
        return false;
    }
    f->fd = fd;
    f->is_external_fd = true;
    f->line_number = 0;
    uw_destroy(&f->pushback);
    return true;
}

static UwResult file_get_name(UwValuePtr self)
{
    _UwFile* f = get_data_ptr(self);
    return uw_clone(&f->name);
}

static bool file_set_name(UwValuePtr self, UwValuePtr file_name)
{
    _UwFile* f = get_data_ptr(self);

    if (f->fd != -1 && !f->is_external_fd) {
        // not an externally set fd
        return false;
    }

    // set file name
    uw_destroy(&f->name);
    f->name = uw_clone(file_name);

    return true;
}

/****************************************************************
 * FileReader interface methods
 */

static UwResult file_read(UwValuePtr self, void* buffer, unsigned buffer_size, unsigned* bytes_read)
{
    _UwFile* f = get_data_ptr(self);

    ssize_t result;
    do {
        result = read(f->fd, buffer, buffer_size);
    } while (result < 0 && errno == EINTR);

    if (result < 0) {
        return UwErrno(errno);
    } else {
        *bytes_read = (unsigned) result;
        return UwOK();
    }
}

/****************************************************************
 * FileWriter interface methods
 */

static UwResult file_write(UwValuePtr self, void* data, unsigned size, unsigned* bytes_written)
{
    _UwFile* f = get_data_ptr(self);

    ssize_t result;
    do {
        result = write(f->fd, data, size);
    } while (result < 0 && errno == EINTR);

    if (result < 0) {
        return UwErrno(errno);
    } else {
        *bytes_written = (unsigned) result;
        return UwOK();
    }
}

/****************************************************************
 * LineReader interface methods
 */

static UwResult start_read_lines(UwValuePtr self)
{
    _UwFile* f = get_data_ptr(self);

    uw_destroy(&f->pushback);

    if (f->buffer == nullptr) {
        f->buffer = malloc(LINE_READER_BUFFER_SIZE);
        if (!f->buffer) {
            return UwOOM();
        }
    }
    f->partial_utf8_len = 0;

    // the following settings will force line_reader to read next chunk of data immediately:
    f->position = LINE_READER_BUFFER_SIZE;
    f->data_size = LINE_READER_BUFFER_SIZE;

    // reset file position
    if (lseek(f->fd, 0, SEEK_SET) == -1) {
        return UwErrno(errno);
    }
    f->line_number = 0;
    return UwOK();
}

static UwResult read_line(UwValuePtr self)
{
    UwValue result = UwString();
    if (uw_ok(&result)) {
        UwValue status = read_line_inplace(self, &result);
        uw_return_if_error(&status);
    }
    return uw_move(&result);
}

static UwResult read_line_inplace(UwValuePtr self, UwValuePtr line)
{
    _UwFile* f = get_data_ptr(self);

    uw_string_truncate(line, 0);

    if (f->buffer == nullptr) {
        UwValue status = start_read_lines(self);
        uw_return_if_error(&status);
    }

    if (uw_is_string(&f->pushback)) {
        if (!uw_string_append(line, &f->pushback)) {
            return UwOOM();
        }
        uw_destroy(&f->pushback);
        f->line_number++;
        return UwOK();
    }

    if ( ! (f->position || f->data_size)) {
        // EOF state
        // XXX warn if f->partial_utf8_len != 0
        return UwError(UW_ERROR_EOF);
    }

    do {
        if (f->position == f->data_size) {

            // reached end of data scanning for line break

            f->position = 0;

            // read next chunk of file
            {
                UwValue status = file_read(self, f->buffer, LINE_READER_BUFFER_SIZE, &f->data_size);
                uw_return_if_error(&status);
                if (f->data_size == 0) {
                    return UwError(UW_ERROR_EOF);
                }
            }

            if (f->partial_utf8_len) {
                // process partial UTF-8 sequence
                while (f->partial_utf8_len < 4) {

                    if (f->position == f->data_size) {
                        // premature end of file
                        // XXX warn?
                        return UwError(UW_ERROR_EOF);
                    }

                    char8_t c = f->buffer[f->position];
                    if (c < 0x80 || ((c & 0xC0) != 0x80)) {
                        // malformed UTF-8 sequence
                        break;
                    }
                    f->position++;
                    f->partial_utf8[f->partial_utf8_len++] = c;

                    char8_t* ptr = f->partial_utf8;
                    unsigned bytes_remaining = f->partial_utf8_len;
                    char32_t chr;
                    if (read_utf8_buffer(&ptr, &bytes_remaining, &chr)) {
                        if (chr != 0xFFFFFFFF) {
                            if (!uw_string_append(line, chr)) {
                                return UwOOM();
                            }
                        }
                        break;
                    }
                }
                f->partial_utf8_len = 0;
            }
        }

        char8_t* ptr = f->buffer + f->position;
        unsigned bytes_remaining = f->data_size - f->position;
        while (bytes_remaining) {
            char32_t chr;
            if (!read_utf8_buffer(&ptr, &bytes_remaining, &chr)) {
                break;
            }
            if (chr != 0xFFFFFFFF) {
                if (!uw_string_append(line, chr)) {
                    return UwOOM();
                }
                if (chr == '\n') {
                    f->position = f->data_size - bytes_remaining;
                    f->line_number++;
                    return UwOK();
                }
            }
        }
        // move unprocessed data to partial_utf8
        while (bytes_remaining--) {
            f->partial_utf8[f->partial_utf8_len++] = *ptr++;
        }
        if (f->data_size < LINE_READER_BUFFER_SIZE) {
            // reached end of file, set EOF state
            f->position = 0;
            f->data_size = 0;
            f->line_number++;
            return UwOK();
        }

        // go read next chunk
        f->position = f->data_size;

    } while(true);
}

static bool unread_line(UwValuePtr self, UwValuePtr line)
{
    _UwFile* f = get_data_ptr(self);

    if (uw_is_null(&f->pushback)) {
        f->pushback = uw_clone(line);
        f->line_number--;
        return true;
    } else {
        return false;
    }
}

static unsigned get_line_number(UwValuePtr self)
{
    return get_data_ptr(self)->line_number;
}

static void stop_read_lines(UwValuePtr self)
{
    _UwFile* f = get_data_ptr(self);

    free(f->buffer);
    f->buffer = nullptr;
    uw_destroy(&f->pushback);
}

/****************************************************************
 * File type and interfaces
 */

UwTypeId UwTypeId_File = 0;

static UwInterface_File file_interface = {
    .open     = file_open,
    .close    = file_close,
    .set_fd   = file_set_fd,
    .get_name = file_get_name,
    .set_name = file_set_name
};

static UwInterface_FileReader file_reader_interface = {
    .read = file_read
};

static UwInterface_FileWriter file_writer_interface = {
    .write = file_write
};

static UwInterface_LineReader line_reader_interface = {
    .start             = start_read_lines,
    .read_line         = read_line,
    .read_line_inplace = read_line_inplace,
    .get_line_number   = get_line_number,
    .unread_line       = unread_line,
    .stop              = stop_read_lines
};

static UwType file_type = {
    .id             = 0,
    .ancestor_id    = UwTypeId_Struct,
    .name           = "File",
    .allocator      = &default_allocator,

    .create         = _uw_struct_create,
    .destroy        = _uw_struct_destroy,
    .clone          = _uw_struct_clone,
    .hash           = file_hash,
    .deepcopy       = file_deepcopy,
    .dump           = file_dump,
    .to_string      = _uw_struct_to_string,
    .is_true        = _uw_struct_is_true,
    .equal_sametype = file_equal_sametype,
    .equal          = file_equal,

    .data_offset    = sizeof(_UwStructData),
    .data_size      = sizeof(_UwFile),

    .init           = file_init,
    .fini           = file_fini
};

// make sure _UwStructData has correct padding
static_assert((sizeof(_UwStructData) & (alignof(_UwFile) - 1)) == 0);


[[ gnu::constructor ]]
static void init_file_type()
{
    // interfaces can be registered by any type in any order
    if (UwInterfaceId_File == 0)       { UwInterfaceId_File       = uw_register_interface("File",       UwInterface_File); }
    if (UwInterfaceId_FileReader == 0) { UwInterfaceId_FileReader = uw_register_interface("FileReader", UwInterface_FileReader); }
    if (UwInterfaceId_FileWriter == 0) { UwInterfaceId_FileWriter = uw_register_interface("FileWriter", UwInterface_FileWriter); }
    if (UwInterfaceId_LineReader == 0) { UwInterfaceId_LineReader = uw_register_interface("LineReader", UwInterface_LineReader); }

    UwTypeId_File = uw_add_type(
        &file_type,
        UwInterfaceId_File,       &file_interface,
        UwInterfaceId_FileReader, &file_reader_interface,
        UwInterfaceId_FileWriter, &file_writer_interface,
        UwInterfaceId_LineReader, &line_reader_interface
    );
}

/****************************************************************
 * Shorthand functions
 */

UwResult _uw_file_open(UwValuePtr file_name, int flags, mode_t mode)
{
    UwValue normalized_filename = uw_clone(file_name);  // this converts CharPtr to String
    uw_return_if_error(&normalized_filename);

    UwValue file = uw_create(UwTypeId_File);
    uw_return_if_error(&file);

    UwValue status = uw_interface(file.type_id, File)->open(&file, &normalized_filename, flags, mode);
    uw_return_if_error(&status);

    return uw_move(&file);
}

/****************************************************************
 * Miscellaneous functions
 */

static UwResult get_file_size(char* file_name)
{
    struct stat statbuf;
    if (stat(file_name, &statbuf) == -1) {
        return UwErrno(errno);
    }
    if ( ! (statbuf.st_mode & S_IFREG)) {
        return UwError(UW_ERROR_NOT_REGULAR_FILE);
    }
    return UwUnsigned(statbuf.st_size);
}

UwResult uw_file_size(UwValuePtr file_name)
{
    if (uw_is_charptr(file_name)) {
        switch (file_name->charptr_subtype) {
            case UW_CHARPTR:
                return get_file_size((char*) file_name->charptr);

            case UW_CHAR32PTR: {
                UW_CSTRING_LOCAL(fname, file_name);
                return get_file_size(fname);
            }
            default:
                _uw_panic_bad_charptr_subtype(file_name);
        }
    } else {
        uw_assert_string(file_name);
        UW_CSTRING_LOCAL(fname, file_name);
        return get_file_size(fname);
    }
}

/****************************************************************
 * Path functions, probably should be separated
 */

UwResult uw_basename(UwValuePtr filename)
{
    UwValue parts = UwNull();
    if (uw_is_charptr(filename)) {
        UwValue f = uw_clone(filename);
        parts = uw_string_rsplit_chr(&f, '/', 1);
    } else {
        uw_expect(string, filename);
        parts = uw_string_rsplit_chr(filename, '/', 1);
    }
    return uw_array_item(&parts, -1);
}

UwResult uw_dirname(UwValuePtr filename)
{
    UwValue parts = UwNull();
    if (uw_is_charptr(filename)) {
        UwValue f = uw_clone(filename);
        parts = uw_string_rsplit_chr(&f, '/', 1);
    } else {
        uw_expect(string, filename);
        parts = uw_string_rsplit_chr(filename, '/', 1);
    }
    return uw_array_item(&parts, 0);
}

UwResult _uw_path_v(...)
{
    UwValue parts = uw_create(UwTypeId_Array);
    va_list ap;
    va_start(ap);
    for (;;) {{
        UwValue arg = va_arg(ap, _UwValue);
        if (uw_is_status(&arg)) {
            if (uw_va_end(&arg)) {
                break;
            }
            _uw_destroy_args(ap);
            va_end(ap);
            return uw_move(&arg);
        }
        if (uw_is_string(&arg) || uw_is_charptr(&arg)) {
            if (!uw_array_append(&parts, &arg)) {
                _uw_destroy_args(ap);
                va_end(ap);
                return UwOOM();
            }
        }
    }}
    va_end(ap);
    return uw_array_join('/', &parts);
}

UwResult _uw_path_p(...)
{
    UwValue parts = uw_create(UwTypeId_Array);
    va_list ap;
    va_start(ap);
    for (;;) {
        UwValuePtr arg = va_arg(ap, UwValuePtr);
        if (!arg) {
            break;
        }
        if (uw_error(arg)) {
            va_end(ap);
            return uw_clone(arg);
        }
        if (uw_is_string(arg) || uw_is_charptr(arg)) {
            if (!uw_array_append(&parts, arg)) {
                va_end(ap);
                return UwOOM();
            }
        }
    }
    va_end(ap);
    return uw_array_join('/', &parts);
}
