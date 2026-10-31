/*
    Engine1999 - A 2D games engine written in C
    Copyright (C) 2026  Ekkehard Morgenstern

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    NOTE: Programs created with a built-in programming language (if any),
          do not fall under this license.

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#include <ctype.h>
#include <fnmatch.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include "iodev.h"

// -- the registry --------------------------------------------------------------------

#define IODEV_MAXDRIVERS    32U

static iodrv_t* iodev_drivers[ IODEV_MAXDRIVERS ];
static size_t iodev_numdrivers = 0;
static bool iodev_ready = false;

static bool iodev_valid_name( const char* name ) {
    size_t n = strlen( name );
    if ( n < 2U || n > 16U || !isalpha( (unsigned char) name[0] ) ) {
        return false;
    }
    for ( size_t i=1; i < n; ++i ) {
        if ( !isalnum( (unsigned char) name[i] ) && name[i] != '_' ) {
            return false;
        }
    }
    return true;
}

iodrv_t* iodev_find( const char* name ) {
    for ( size_t i=0; i < iodev_numdrivers; ++i ) {
        if ( strcasecmp( iodev_drivers[i]->name, name ) == 0 ) {
            return iodev_drivers[i];
        }
    }
    return 0;
}

bool iodev_register( iodrv_t* drv ) {
    if ( drv == 0 || drv->name == 0 || !iodev_valid_name( drv->name ) || drv->open == 0 || iodev_find( drv->name ) != 0 ||
         iodev_numdrivers >= IODEV_MAXDRIVERS ) {
        return false;
    }
    iodev_drivers[ iodev_numdrivers++ ] = drv;
    return true;
}

bool iodev_allowed( const char* operation, const iodrv_t* drv, const char* path ) {
    (void) operation; (void) drv; (void) path;
    return true;        // sandboxing is an open issue (docs/io-subsystem.md)
}

bool iodev_resolve( const char* name, const char* defaultdriver, iodrv_t** pdrv, const char** ppath, const char** perr ) {
    iodev_init();
    *perr = 0;
    size_t n = 0;
    if ( isalpha( (unsigned char) name[0] ) ) {
        while ( n < 17U && ( isalnum( (unsigned char) name[n] ) || name[n] == '_' ) ) {
            ++n;
        }
    }
    iodrv_t* drv = 0;
    if ( n >= 2U && n <= 16U && name[n] == ':' ) {
        char prefix[20];
        memcpy( prefix, name, n );
        prefix[n] = '\0';
        drv = iodev_find( prefix );
        if ( drv == 0 ) {
            *perr = "Device not found";
            return false;
        }
        *ppath = name + n + 1U;
    } else {
        drv = iodev_find( defaultdriver != 0 && defaultdriver[0] != '\0' ? defaultdriver : "file" );
        if ( drv == 0 ) {
            *perr = "Device not found";
            return false;
        }
        *ppath = name;
    }
    *pdrv = drv;
    return true;
}

static const char* iodev_errno_text( int err ) {
    switch ( err ) {
        case ENOENT:    return "File not found";
        case EACCES:
        case EPERM:     return "Permission denied";
        case EISDIR:    return "Is a directory";
        case ENOSPC:    return "Disk full";
        default:        return "Device I/O error";
    }
}

static void iodev_set_err( char* err, size_t errsz, const char* text ) {
    snprintf( err, errsz, "%s", text );
}

// -- the file driver -------------------------------------------------------------------

typedef struct _filehandle_t {
    FILE*       f;
} filehandle_t;

static void* file_open( iodrv_t* drv, const char* path, unsigned mode, char* err, size_t errsz ) {
    (void) drv;
    if ( path[0] == '\0' ) {
        iodev_set_err( err, errsz, "Bad file name" );
        return 0;
    }
    struct stat st;
    if ( stat( path, &st ) == 0 && S_ISDIR( st.st_mode ) ) {
        iodev_set_err( err, errsz, "Is a directory" );
        return 0;
    }
    const char* fmode = ( mode & IOMODE_APPEND ) ? "ab" : ( mode & IOMODE_WRITE ) ? "wb" : "rb";
    FILE* f = fopen( path, fmode );
    if ( f == 0 ) {
        iodev_set_err( err, errsz, iodev_errno_text( errno ) );
        return 0;
    }
    filehandle_t* h = (filehandle_t*) malloc( sizeof(*h) );
    if ( h == 0 ) {
        fclose( f );
        iodev_set_err( err, errsz, "Out of memory" );
        return 0;
    }
    h->f = f;
    return h;
}

static ssize_t file_read( void* handle, void* buf, size_t count ) {
    filehandle_t* h = (filehandle_t*) handle;
    size_t n = fread( buf, 1U, count, h->f );
    return n == 0 && ferror( h->f ) ? -1 : (ssize_t) n;
}

static ssize_t file_write( void* handle, const void* buf, size_t count ) {
    filehandle_t* h = (filehandle_t*) handle;
    return fwrite( buf, 1U, count, h->f ) == count ? (ssize_t) count : -1;
}

static int64_t file_size( void* handle ) {
    filehandle_t* h = (filehandle_t*) handle;
    struct stat st;
    fflush( h->f );
    return fstat( fileno( h->f ), &st ) == 0 ? (int64_t) st.st_size : -1;
}

static bool file_close( void* handle ) {
    filehandle_t* h = (filehandle_t*) handle;
    bool ok = fclose( h->f ) == 0;
    free( h );
    return ok;
}

static bool file_list( iodrv_t* drv, const char* spec, hlstyle_t style, void* styleud, files_emit_t emit, void* emitud, char* err,
    size_t errsz ) {
    (void) drv;
    return files_list( spec, style, styleud, emit, emitud, err, errsz );
}

static bool file_remove( iodrv_t* drv, const char* path, char* err, size_t errsz ) {
    (void) drv;
    if ( unlink( path ) != 0 ) {
        iodev_set_err( err, errsz, iodev_errno_text( errno ) );
        return false;
    }
    return true;
}

static bool file_rename( iodrv_t* drv, const char* from, const char* to, char* err, size_t errsz ) {
    (void) drv;
    if ( from[0] == '\0' || to[0] == '\0' ) {
        iodev_set_err( err, errsz, "Bad file name" );
        return false;
    }
    struct stat st;
    if ( lstat( to, &st ) == 0 ) {
        iodev_set_err( err, errsz, "File already exists" );     // rename() would replace it silently
        return false;
    }
    if ( rename( from, to ) != 0 ) {
        iodev_set_err( err, errsz, errno == EEXIST ? "File already exists" : iodev_errno_text( errno ) );
        return false;
    }
    return true;
}

static bool file_mkdir( iodrv_t* drv, const char* path, char* err, size_t errsz ) {
    (void) drv;
    if ( path[0] == '\0' ) {
        iodev_set_err( err, errsz, "Bad file name" );
        return false;
    }
    if ( mkdir( path, 0777 ) != 0 ) {
        iodev_set_err( err, errsz, errno == EEXIST ? "File already exists" : iodev_errno_text( errno ) );
        return false;
    }
    return true;
}

static bool file_chdir( iodrv_t* drv, const char* path, char* err, size_t errsz ) {
    (void) drv;
    if ( path[0] == '\0' ) {
        iodev_set_err( err, errsz, "Bad file name" );
        return false;
    }
    if ( chdir( path ) != 0 ) {
        iodev_set_err( err, errsz, errno == ENOTDIR ? "Not a directory" : iodev_errno_text( errno ) );
        return false;
    }
    return true;
}

static bool file_seek( void* handle, int64_t pos ) {
    filehandle_t* h = (filehandle_t*) handle;
    return fseeko( h->f, (off_t) pos, SEEK_SET ) == 0;
}

static iodrv_t file_driver = { "file", file_open, file_read, file_write, file_size, file_close, file_list, file_remove, file_rename,
    file_mkdir, file_chdir, file_seek, 0 };

// -- the null driver ---------------------------------------------------------------------

static void* null_open( iodrv_t* drv, const char* path, unsigned mode, char* err, size_t errsz ) {
    (void) drv; (void) path; (void) mode; (void) err; (void) errsz;
    return malloc( 1U );
}

static ssize_t null_read( void* handle, void* buf, size_t count ) {
    (void) handle; (void) buf; (void) count;
    return 0;
}

static ssize_t null_write( void* handle, const void* buf, size_t count ) {
    (void) handle; (void) buf;
    return (ssize_t) count;
}

static int64_t null_size( void* handle ) {
    (void) handle;
    return 0;
}

static bool null_close( void* handle ) {
    free( handle );
    return true;
}

static bool null_seek( void* handle, int64_t pos ) {
    (void) handle; (void) pos;
    return true;
}

static iodrv_t null_driver = { "null", null_open, null_read, null_write, null_size, null_close, 0, 0, 0, 0, 0, null_seek, 0 };

// -- the mem driver (a RAM disk) ----------------------------------------------------------

typedef struct _memfile_t {
    char*       name;
    uint8_t*    data;
    size_t      len, cap;
    time_t      mtime;
    unsigned    refs;           // open handles
    bool        gone;           // removed while open: freed with the last handle
} memfile_t;

typedef struct _memhandle_t {
    memfile_t*  file;
    size_t      pos;
    unsigned    mode;
} memhandle_t;

static memfile_t** mem_files = 0;
static size_t mem_count = 0, mem_cap = 0;

static void mem_free_file( memfile_t* f ) {
    free( f->name );
    free( f->data );
    free( f );
}

static memfile_t* mem_lookup( const char* name, size_t* pindex ) {
    for ( size_t i=0; i < mem_count; ++i ) {
        if ( strcmp( mem_files[i]->name, name ) == 0 ) {
            if ( pindex ) *pindex = i;
            return mem_files[i];
        }
    }
    return 0;
}

static void mem_detach( size_t index ) {
    memfile_t* f = mem_files[index];
    mem_files[index] = mem_files[ --mem_count ];
    f->gone = true;
    if ( f->refs == 0 ) {
        mem_free_file( f );
    }
}

void iodev_mem_clear( void ) {
    while ( mem_count > 0 ) {
        mem_detach( mem_count - 1U );
    }
}

static void* mem_open( iodrv_t* drv, const char* path, unsigned mode, char* err, size_t errsz ) {
    (void) drv;
    if ( path[0] == '\0' ) {
        iodev_set_err( err, errsz, "Bad file name" );
        return 0;
    }
    memfile_t* f = mem_lookup( path, 0 );
    if ( f == 0 && ( mode & IOMODE_WRITE ) == 0 ) {
        iodev_set_err( err, errsz, "File not found" );
        return 0;
    }
    if ( f == 0 ) {
        if ( mem_count == mem_cap ) {
            size_t cap = mem_cap ? mem_cap * 2U : 16U;
            memfile_t** files = (memfile_t**) realloc( mem_files, cap * sizeof(*files) );
            if ( files == 0 ) {
                iodev_set_err( err, errsz, "Out of memory" );
                return 0;
            }
            mem_files = files; mem_cap = cap;
        }
        f = (memfile_t*) calloc( 1U, sizeof(*f) );
        if ( f == 0 || ( f->name = strdup( path ) ) == 0 ) {
            free( f );
            iodev_set_err( err, errsz, "Out of memory" );
            return 0;
        }
        f->mtime = time( 0 );
        mem_files[ mem_count++ ] = f;
    } else if ( ( mode & IOMODE_WRITE ) != 0 && ( mode & IOMODE_APPEND ) == 0 ) {
        f->len = 0;             // OUTPUT truncates
        f->mtime = time( 0 );
    }
    memhandle_t* h = (memhandle_t*) calloc( 1U, sizeof(*h) );
    if ( h == 0 ) {
        iodev_set_err( err, errsz, "Out of memory" );
        return 0;
    }
    h->file = f;
    h->mode = mode;
    h->pos = ( mode & IOMODE_APPEND ) ? f->len : 0U;
    ++f->refs;
    return h;
}

static ssize_t mem_read( void* handle, void* buf, size_t count ) {
    memhandle_t* h = (memhandle_t*) handle;
    if ( h->pos >= h->file->len ) {
        return 0;
    }
    size_t n = h->file->len - h->pos;
    if ( n > count ) {
        n = count;
    }
    memcpy( buf, h->file->data + h->pos, n );
    h->pos += n;
    return (ssize_t) n;
}

static ssize_t mem_write( void* handle, const void* buf, size_t count ) {
    memhandle_t* h = (memhandle_t*) handle;
    memfile_t* f = h->file;
    if ( h->pos + count > f->cap ) {
        size_t cap = f->cap ? f->cap : 256U;
        while ( cap < h->pos + count ) {
            cap *= 2U;
        }
        uint8_t* data = (uint8_t*) realloc( f->data, cap );
        if ( data == 0 ) {
            return -1;
        }
        f->data = data; f->cap = cap;
    }
    memcpy( f->data + h->pos, buf, count );
    h->pos += count;
    if ( h->pos > f->len ) {
        f->len = h->pos;
    }
    f->mtime = time( 0 );
    return (ssize_t) count;
}

static int64_t mem_size( void* handle ) {
    return (int64_t) ((memhandle_t*) handle)->file->len;
}

static bool mem_close( void* handle ) {
    memhandle_t* h = (memhandle_t*) handle;
    memfile_t* f = h->file;
    free( h );
    if ( --f->refs == 0 && f->gone ) {
        mem_free_file( f );
    }
    return true;
}

static bool mem_remove( iodrv_t* drv, const char* path, char* err, size_t errsz ) {
    (void) drv;
    size_t index = 0;
    if ( mem_lookup( path, &index ) == 0 ) {
        iodev_set_err( err, errsz, "File not found" );
        return false;
    }
    mem_detach( index );
    return true;
}

static bool mem_rename( iodrv_t* drv, const char* from, const char* to, char* err, size_t errsz ) {
    (void) drv;
    size_t index = 0;
    memfile_t* f = mem_lookup( from, &index );
    if ( f == 0 ) {
        iodev_set_err( err, errsz, "File not found" );
        return false;
    }
    if ( to[0] == '\0' ) {
        iodev_set_err( err, errsz, "Bad file name" );
        return false;
    }
    if ( mem_lookup( to, 0 ) != 0 ) {
        iodev_set_err( err, errsz, "File already exists" );
        return false;
    }
    char* name = strdup( to );
    if ( name == 0 ) {
        iodev_set_err( err, errsz, "Out of memory" );
        return false;
    }
    free( f->name );
    f->name = name;
    return true;
}

static bool mem_seek( void* handle, int64_t pos ) {
    memhandle_t* h = (memhandle_t*) handle;
    if ( pos < 0 || (uint64_t) pos > (uint64_t) h->file->len ) {
        return false;           // no holes in a RAM file
    }
    h->pos = (size_t) pos;
    return true;
}

static int mem_compare( const void* pa, const void* pb ) {
    const memfile_t* a = *(memfile_t* const*) pa;
    const memfile_t* b = *(memfile_t* const*) pb;
    if ( a->mtime != b->mtime ) {
        return a->mtime < b->mtime ? -1 : 1;
    }
    return strcmp( a->name, b->name );
}

static bool mem_list( iodrv_t* drv, const char* spec, hlstyle_t style, void* styleud, files_emit_t emit, void* emitud, char* err,
    size_t errsz ) {
    (void) drv;
    memfile_t** sel = (memfile_t**) malloc( ( mem_count ? mem_count : 1U ) * sizeof(*sel) );
    if ( sel == 0 ) {
        iodev_set_err( err, errsz, "Out of memory" );
        return false;
    }
    size_t n = 0;
    int sizewidth = 1;
    for ( size_t i=0; i < mem_count; ++i ) {
        if ( spec[0] == '\0' || fnmatch( spec, mem_files[i]->name, 0 ) == 0 ) {
            sel[n++] = mem_files[i];
            char num[32];
            int w = snprintf( num, sizeof(num), "%zu", mem_files[i]->len );
            if ( w > sizewidth ) sizewidth = w;
        }
    }
    if ( n == 0 && spec[0] != '\0' ) {
        free( sel );
        iodev_set_err( err, errsz, "File not found" );
        return false;
    }
    qsort( sel, n, sizeof(*sel), mem_compare );
    bool ok = true, stop = false;
    for ( size_t i=0; i < n && ok && !stop; ++i ) {
        size_t len = strlen( sel[i]->name );
        bool prog = len > 4U && ( strcasecmp( sel[i]->name + len - 4U, ".bas" ) == 0 || strcasecmp( sel[i]->name + len - 4U, ".e99" ) == 0 );
        ok = files_format_entry( sizewidth, (long long) sel[i]->len, sel[i]->mtime, prog ? HL_FILEPROG : HL_IDENT, sel[i]->name, "", 0,
            style, styleud, emit, emitud, &stop, err, errsz );
    }
    free( sel );
    return ok;
}

static iodrv_t mem_driver = { "mem", mem_open, mem_read, mem_write, mem_size, mem_close, mem_list, mem_remove, mem_rename, 0, 0,
    mem_seek, 0 };

// -- the sh driver (a shell command as a file) --------------------------------------------------------

static void* sh_open( iodrv_t* drv, const char* path, unsigned mode, char* err, size_t errsz ) {
    if ( path[0] == '\0' ) {
        iodev_set_err( err, errsz, "Bad file name" );
        return 0;
    }
    if ( ( mode & IOMODE_APPEND ) != 0 ) {
        iodev_set_err( err, errsz, "Bad file mode" );
        return 0;
    }
    if ( !iodev_allowed( "shell", drv, path ) ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return 0;
    }
    fflush( stdout );
    FILE* f = popen( path, ( mode & IOMODE_WRITE ) ? "w" : "r" );
    if ( f == 0 ) {
        iodev_set_err( err, errsz, "Device I/O error" );
        return 0;
    }
    filehandle_t* h = (filehandle_t*) malloc( sizeof(*h) );
    if ( h == 0 ) {
        pclose( f );
        iodev_set_err( err, errsz, "Out of memory" );
        return 0;
    }
    h->f = f;
    return h;
}

static bool sh_close( void* handle ) {
    filehandle_t* h = (filehandle_t*) handle;
    pclose( h->f );             // the exit status of the command is not an I/O error
    free( h );
    return true;
}

static iodrv_t sh_driver = { "sh", sh_open, file_read, file_write, 0, sh_close, 0, 0, 0, 0, 0, 0, 0 };

// -- the res driver (read-only game assets in the directory "res" of the current directory) ---------

#define RES_DIR     "res/"

// "res/" + path, or 0 for a path that leaves the directory (absolute, or with a ".." component)
static char* res_path( const char* path ) {
    if ( path[0] == '/' ) {
        return 0;
    }
    for ( const char* p = path; *p; ) {
        const char* e = strchr( p, '/' );
        size_t n = e ? (size_t)( e - p ) : strlen( p );
        if ( n == 2U && p[0] == '.' && p[1] == '.' ) {
            return 0;
        }
        p += n;
        if ( *p == '/' ) {
            ++p;
        }
    }
    size_t n = strlen( RES_DIR ) + strlen( path ) + 1U;
    char* full = (char*) malloc( n );
    if ( full != 0 ) {
        snprintf( full, n, "%s%s", RES_DIR, path );
    }
    return full;
}

static void* res_open( iodrv_t* drv, const char* path, unsigned mode, char* err, size_t errsz ) {
    if ( mode & IOMODE_WRITE ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return 0;
    }
    char* full = res_path( path );
    if ( full == 0 ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return 0;
    }
    void* h = file_open( drv, full, mode, err, errsz );
    free( full );
    return h;
}

static bool res_list( iodrv_t* drv, const char* spec, hlstyle_t style, void* styleud, files_emit_t emit, void* emitud, char* err,
    size_t errsz ) {
    char* full = res_path( spec );
    if ( full == 0 ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return false;
    }
    bool ok = file_list( drv, full, style, styleud, emit, emitud, err, errsz );
    free( full );
    return ok;
}

static iodrv_t res_driver = { "res", res_open, file_read, 0, file_size, file_close, res_list, 0, 0, 0, 0, file_seek, 0 };

// -- the con driver (the screen and the keyboard as a channel) -------------------------------------------

static iocon_t iodev_con;
static bool iodev_con_set = false;

void iodev_set_console( const iocon_t* con ) {
    if ( con != 0 ) {
        iodev_con = *con;
    }
    iodev_con_set = con != 0;
}

static void* con_open( iodrv_t* drv, const char* path, unsigned mode, char* err, size_t errsz ) {
    (void) drv; (void) path;
    if ( !iodev_con_set || ( mode & IOMODE_APPEND ) != 0 ) {
        iodev_set_err( err, errsz, "Not supported by device" );
        return 0;
    }
    return (void*)( (uintptr_t) 1U + ( ( mode & IOMODE_WRITE ) ? 1U : 0U ) );     // a non-null tag: 1 = input, 2 = output
}

static ssize_t con_read( void* handle, void* buf, size_t count ) {
    (void) handle;
    if ( !iodev_con_set || iodev_con.getkey == 0 || count == 0 ) {
        return -1;
    }
    int key = iodev_con.getkey( iodev_con.usrdata );        // one key per call: keys are not buffered, so INPUT$ waits for each
    if ( key <= 0 || key > 255 ) {
        return key < 0 ? -1 : 0;
    }
    *(uint8_t*) buf = (uint8_t) key;
    return 1;
}

static ssize_t con_write( void* handle, const void* buf, size_t count ) {
    (void) handle;
    if ( !iodev_con_set || iodev_con.write == 0 ) {
        return -1;
    }
    return iodev_con.write( iodev_con.usrdata, (const char*) buf, count ) ? (ssize_t) count : -1;
}

static int64_t con_size( void* handle ) {
    (void) handle;
    return -1;
}

static bool con_close( void* handle ) {
    (void) handle;
    return true;
}

static iodrv_t con_driver = { "con", con_open, con_read, con_write, con_size, con_close, 0, 0, 0, 0, 0, 0, 0 };

void iodev_init( void ) {
    if ( !iodev_ready ) {
        iodev_ready = true;
        iodev_register( &file_driver );
        iodev_register( &null_driver );
        iodev_register( &mem_driver );
        iodev_register( &sh_driver );
        iodev_register( &res_driver );
        iodev_register( &con_driver );
    }
}

// -- whole files -------------------------------------------------------------------------------

bool iodev_read_all( const char* name, const char* defaultdriver, uint8_t** pdata, size_t* plen, const char** perr ) {
    iodrv_t* drv = 0; const char* path = 0;
    if ( !iodev_resolve( name, defaultdriver, &drv, &path, perr ) ) {
        return false;
    }
    if ( !iodev_allowed( "read", drv, path ) ) {
        *perr = "Permission denied";
        return false;
    }
    char err[80];
    err[0] = '\0';
    void* h = drv->open( drv, path, IOMODE_READ, err, sizeof(err) );
    if ( h == 0 ) {
        *perr = err[0] == '\0' || strcmp( err, "File not found" ) == 0 ? "File not found" :
            strcmp( err, "Permission denied" ) == 0 ? "Permission denied" : strcmp( err, "Is a directory" ) == 0 ? "Is a directory" :
            strcmp( err, "Bad file name" ) == 0 ? "Bad file name" : strcmp( err, "Out of memory" ) == 0 ? "Out of memory" :
            "Device I/O error";
        return false;
    }
    size_t cap = 4096U, len = 0;
    uint8_t* data = (uint8_t*) malloc( cap );
    bool ok = data != 0;
    *perr = ok ? 0 : "Out of memory";
    while ( ok ) {
        if ( len == cap ) {
            uint8_t* bigger = (uint8_t*) realloc( data, cap * 2U );
            if ( bigger == 0 ) {
                *perr = "Out of memory";
                ok = false;
                break;
            }
            data = bigger; cap *= 2U;
        }
        ssize_t n = drv->read( h, data + len, cap - len );
        if ( n < 0 ) {
            *perr = "Read error";
            ok = false;
        } else if ( n == 0 ) {
            break;
        } else {
            len += (size_t) n;
        }
    }
    drv->close( h );
    if ( !ok ) {
        free( data );
        return false;
    }
    *pdata = data;
    *plen = len;
    return true;
}

bool iodev_write_all( const char* name, const char* defaultdriver, const void* data, size_t len, const char** perr ) {
    iodrv_t* drv = 0; const char* path = 0;
    if ( !iodev_resolve( name, defaultdriver, &drv, &path, perr ) ) {
        return false;
    }
    if ( !iodev_allowed( "write", drv, path ) ) {
        *perr = "Permission denied";
        return false;
    }
    char err[80];
    err[0] = '\0';
    void* h = drv->open( drv, path, IOMODE_WRITE, err, sizeof(err) );
    if ( h == 0 ) {
        *perr = strcmp( err, "Permission denied" ) == 0 ? "Permission denied" : strcmp( err, "Is a directory" ) == 0 ? "Is a directory" :
            strcmp( err, "Bad file name" ) == 0 ? "Bad file name" : strcmp( err, "Disk full" ) == 0 ? "Disk full" :
            strcmp( err, "Out of memory" ) == 0 ? "Out of memory" : strcmp( err, "File not found" ) == 0 ? "File not found" : "Device I/O error";
        return false;
    }
    bool ok = len == 0 || drv->write( h, data, len ) == (ssize_t) len;
    ok = drv->close( h ) && ok;
    *perr = ok ? 0 : "Write error";
    return ok;
}

bool iodev_exists( const char* name, const char* defaultdriver ) {
    iodrv_t* drv = 0; const char* path = 0; const char* e = 0;
    if ( !iodev_resolve( name, defaultdriver, &drv, &path, &e ) ) {
        return false;
    }
    if ( drv == &file_driver ) {
        struct stat st;
        return stat( path, &st ) == 0 && S_ISREG( st.st_mode );
    }
    char err[80];
    void* h = drv->open( drv, path, IOMODE_READ, err, sizeof(err) );
    if ( h == 0 ) {
        return false;
    }
    drv->close( h );
    return true;
}

bool iodev_list( const char* spec, const char* defaultdriver, hlstyle_t style, void* styleud, files_emit_t emit, void* emitud,
    char* err, size_t errsz ) {
    iodrv_t* drv = 0; const char* path = 0; const char* e = 0;
    if ( !iodev_resolve( spec, defaultdriver, &drv, &path, &e ) ) {
        iodev_set_err( err, errsz, e );
        return false;
    }
    if ( !iodev_allowed( "list", drv, path ) ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return false;
    }
    if ( drv->list == 0 ) {
        iodev_set_err( err, errsz, "Not supported by device" );
        return false;
    }
    return drv->list( drv, path, style, styleud, emit, emitud, err, errsz );
}

// -- KILL, RENAME, MKDIR, CHDIR ---------------------------------------------------------------------

static const char* iodev_known_error( const char* err ) {
    static const char* const known[] = { "File not found", "Permission denied", "Is a directory", "Not a directory", "Disk full",
        "Bad file name", "Out of memory", "File already exists", "Bad file mode", "Not supported by device" };
    for ( size_t i=0; i < sizeof(known) / sizeof(known[0]); ++i ) {
        if ( strcmp( err, known[i] ) == 0 ) {
            return known[i];
        }
    }
    return "Device I/O error";
}

bool iodev_remove( const char* name, const char* defaultdriver, const char** perr ) {
    iodrv_t* drv = 0; const char* path = 0;
    if ( !iodev_resolve( name, defaultdriver, &drv, &path, perr ) ) {
        return false;
    }
    if ( !iodev_allowed( "write", drv, path ) ) {
        *perr = "Permission denied";
        return false;
    }
    char err[80];
    err[0] = '\0';
    if ( drv->remove == 0 ) {
        *perr = "Not supported by device";
        return false;
    }
    if ( !drv->remove( drv, path, err, sizeof(err) ) ) {
        *perr = iodev_known_error( err );
        return false;
    }
    return true;
}

bool iodev_rename( const char* from, const char* to, const char* defaultdriver, const char** perr ) {
    iodrv_t* drv = 0; const char* path = 0;
    iodrv_t* drv2 = 0; const char* path2 = 0;
    if ( !iodev_resolve( from, defaultdriver, &drv, &path, perr ) || !iodev_resolve( to, defaultdriver, &drv2, &path2, perr ) ) {
        return false;
    }
    if ( drv != drv2 ) {
        *perr = "Not the same device";
        return false;
    }
    if ( !iodev_allowed( "write", drv, path ) || !iodev_allowed( "write", drv, path2 ) ) {
        *perr = "Permission denied";
        return false;
    }
    char err[80];
    err[0] = '\0';
    if ( drv->rename == 0 ) {
        *perr = "Not supported by device";
        return false;
    }
    if ( !drv->rename( drv, path, path2, err, sizeof(err) ) ) {
        *perr = iodev_known_error( err );
        return false;
    }
    return true;
}

bool iodev_mkdir( const char* name, const char* defaultdriver, const char** perr ) {
    iodrv_t* drv = 0; const char* path = 0;
    if ( !iodev_resolve( name, defaultdriver, &drv, &path, perr ) ) {
        return false;
    }
    if ( !iodev_allowed( "write", drv, path ) ) {
        *perr = "Permission denied";
        return false;
    }
    char err[80];
    err[0] = '\0';
    if ( drv->mkdir == 0 ) {
        *perr = "Not supported by device";
        return false;
    }
    if ( !drv->mkdir( drv, path, err, sizeof(err) ) ) {
        *perr = iodev_known_error( err );
        return false;
    }
    return true;
}

bool iodev_chdir( const char* name, const char* defaultdriver, const char** perr ) {
    iodrv_t* drv = 0; const char* path = 0;
    if ( !iodev_resolve( name, defaultdriver, &drv, &path, perr ) ) {
        return false;
    }
    if ( !iodev_allowed( "read", drv, path ) ) {
        *perr = "Permission denied";
        return false;
    }
    char err[80];
    err[0] = '\0';
    if ( drv->chdir == 0 ) {
        *perr = "Not supported by device";
        return false;
    }
    if ( !drv->chdir( drv, path, err, sizeof(err) ) ) {
        *perr = iodev_known_error( err );
        return false;
    }
    return true;
}

// -- channels ------------------------------------------------------------------------------------

#define IOCHAN_BUFSIZE  4096U

void iotab_init( iotab_t* tab ) {
    memset( tab, 0, sizeof(*tab) );
}

static void iochan_free( iochan_t* c ) {
    if ( c->drv != 0 && c->handle != 0 ) {
        c->drv->close( c->handle );
    }
    free( c->rbuf );
    free( c );
}

void iotab_closeall( iotab_t* tab ) {
    for ( int i=1; i <= IOCHAN_MAX; ++i ) {
        if ( tab->chan[i] != 0 ) {
            iochan_free( tab->chan[i] );
            tab->chan[i] = 0;
        }
    }
}

bool iotab_isopen( const iotab_t* tab, int chan ) {
    return chan >= 1 && chan <= IOCHAN_MAX && tab->chan[chan] != 0;
}

bool iotab_open( iotab_t* tab, int chan, const char* name, const char* defaultdriver, unsigned mode, const char** perr ) {
    *perr = 0;
    if ( chan < 1 || chan > IOCHAN_MAX ) {
        *perr = "Bad file number";
        return false;
    }
    if ( tab->chan[chan] != 0 ) {
        *perr = "File already open";
        return false;
    }
    iodrv_t* drv = 0; const char* path = 0;
    if ( !iodev_resolve( name, defaultdriver, &drv, &path, perr ) ) {
        return false;
    }
    if ( !iodev_allowed( ( mode & IOMODE_WRITE ) ? "write" : "read", drv, path ) ) {
        *perr = "Permission denied";
        return false;
    }
    char err[80];
    err[0] = '\0';
    void* h = drv->open( drv, path, mode, err, sizeof(err) );
    if ( h == 0 ) {
        static const char* const known[] = { "File not found", "Permission denied", "Is a directory", "Disk full", "Bad file name",
            "Out of memory", "Not supported by device", "Bad file mode" };
        *perr = "Device I/O error";
        for ( size_t i=0; i < sizeof(known) / sizeof(known[0]); ++i ) {
            if ( strcmp( err, known[i] ) == 0 ) {
                *perr = known[i];
            }
        }
        return false;
    }
    iochan_t* c = (iochan_t*) calloc( 1U, sizeof(*c) );
    if ( c == 0 ) {
        drv->close( h );
        *perr = "Out of memory";
        return false;
    }
    c->drv = drv; c->handle = h; c->mode = mode;
    if ( ( mode & IOMODE_APPEND ) && drv->size != 0 ) {
        int64_t size = drv->size( h );
        c->pos = size > 0 ? size : 0;
    }
    tab->chan[chan] = c;
    return true;
}

bool iotab_close( iotab_t* tab, int chan, const char** perr ) {
    *perr = 0;
    if ( chan < 1 || chan > IOCHAN_MAX ) {
        *perr = "Bad file number";
        return false;
    }
    iochan_t* c = tab->chan[chan];
    if ( c == 0 ) {
        return true;        // as in GW-BASIC, closing a channel that is not open is not an error
    }
    tab->chan[chan] = 0;
    bool ok = c->drv->close( c->handle );
    c->handle = 0;
    iochan_free( c );
    if ( !ok ) {
        *perr = "Device I/O error";
    }
    return ok;
}

// the channel for reading / writing, or 0 with a message
static iochan_t* iotab_get( iotab_t* tab, int chan, bool forwrite, const char** perr ) {
    *perr = 0;
    if ( chan < 1 || chan > IOCHAN_MAX ) {
        *perr = "Bad file number";
        return 0;
    }
    iochan_t* c = tab->chan[chan];
    if ( c == 0 ) {
        *perr = "File not open";
        return 0;
    }
    if ( forwrite ? ( c->mode & IOMODE_WRITE ) == 0 : ( c->mode & IOMODE_READ ) == 0 ) {
        *perr = "Bad file mode";
        return 0;
    }
    return c;
}

// fills the read buffer; 1 = data, 0 = end, -1 = error
static int iochan_fill( iochan_t* c ) {
    if ( c->rbuf == 0 ) {
        c->rbuf = (uint8_t*) malloc( IOCHAN_BUFSIZE );
        if ( c->rbuf == 0 ) {
            return -1;
        }
    }
    c->rpos = c->rlen = 0;
    ssize_t n = c->drv->read != 0 ? c->drv->read( c->handle, c->rbuf, IOCHAN_BUFSIZE ) : -1;
    if ( n < 0 ) {
        return -1;
    }
    c->rlen = (size_t) n;
    return n > 0 ? 1 : 0;
}

int iotab_getc( iotab_t* tab, int chan, const char** perr ) {
    iochan_t* c = iotab_get( tab, chan, false, perr );
    if ( c == 0 ) {
        return -2;
    }
    if ( c->rpos >= c->rlen ) {
        int r = iochan_fill( c );
        if ( r < 0 ) {
            *perr = "Device I/O error";
            return -2;
        }
        if ( r == 0 ) {
            return -1;
        }
    }
    ++c->pos;
    return c->rbuf[ c->rpos++ ];
}

bool iotab_readline( iotab_t* tab, int chan, char** pline, const char** perr ) {
    size_t cap = 128U, len = 0;
    char* line = (char*) malloc( cap );
    if ( line == 0 ) {
        *perr = "Out of memory";
        return false;
    }
    bool any = false;
    for (;;) {
        int ch = iotab_getc( tab, chan, perr );
        if ( ch == -2 ) {
            free( line );
            return false;
        }
        if ( ch == -1 ) {
            if ( !any ) {
                free( line );
                *perr = 0;      // the end of the file
                return false;
            }
            break;
        }
        any = true;
        if ( ch == '\n' ) {
            break;
        }
        if ( len + 2U > cap ) {
            char* bigger = (char*) realloc( line, cap * 2U );
            if ( bigger == 0 ) {
                free( line );
                *perr = "Out of memory";
                return false;
            }
            line = bigger; cap *= 2U;
        }
        line[ len++ ] = (char) ch;
    }
    if ( len > 0 && line[ len - 1U ] == '\r' ) {
        --len;
    }
    line[len] = '\0';
    *pline = line;
    *perr = 0;
    return true;
}

bool iotab_write( iotab_t* tab, int chan, const char* data, size_t len, const char** perr ) {
    iochan_t* c = iotab_get( tab, chan, true, perr );
    if ( c == 0 ) {
        return false;
    }
    if ( len > 0 && ( c->drv->write == 0 || c->drv->write( c->handle, data, len ) != (ssize_t) len ) ) {
        *perr = "Device I/O error";
        return false;
    }
    c->pos += (int64_t) len;
    return true;
}

bool iotab_eof( iotab_t* tab, int chan, bool* peof, const char** perr ) {
    iochan_t* c = iotab_get( tab, chan, false, perr );
    if ( c == 0 ) {
        return false;
    }
    if ( c->rpos < c->rlen ) {
        *peof = false;
        return true;
    }
    int r = iochan_fill( c );
    if ( r < 0 ) {
        *perr = "Device I/O error";
        return false;
    }
    *peof = r == 0;
    return true;
}

bool iotab_lof( iotab_t* tab, int chan, int64_t* plen, const char** perr ) {
    *perr = 0;
    if ( chan < 1 || chan > IOCHAN_MAX ) {
        *perr = "Bad file number";
        return false;
    }
    iochan_t* c = tab->chan[chan];
    if ( c == 0 ) {
        *perr = "File not open";
        return false;
    }
    int64_t size = c->drv->size != 0 ? c->drv->size( c->handle ) : -1;
    if ( size < 0 ) {
        *perr = "Not supported by device";
        return false;
    }
    *plen = size;
    return true;
}

bool iotab_loc( iotab_t* tab, int chan, int64_t* ppos, const char** perr ) {
    *perr = 0;
    if ( chan < 1 || chan > IOCHAN_MAX ) {
        *perr = "Bad file number";
        return false;
    }
    iochan_t* c = tab->chan[chan];
    if ( c == 0 ) {
        *perr = "File not open";
        return false;
    }
    *ppos = c->pos;
    return true;
}

bool iotab_seek( iotab_t* tab, int chan, int64_t pos, const char** perr ) {
    *perr = 0;
    if ( chan < 1 || chan > IOCHAN_MAX ) {
        *perr = "Bad file number";
        return false;
    }
    iochan_t* c = tab->chan[chan];
    if ( c == 0 ) {
        *perr = "File not open";
        return false;
    }
    if ( pos < 0 ) {
        *perr = "Bad position";
        return false;
    }
    if ( c->drv->seek == 0 ) {
        *perr = "Not supported by device";
        return false;
    }
    if ( !c->drv->seek( c->handle, pos ) ) {
        *perr = "Bad position";
        return false;
    }
    c->rpos = c->rlen = 0;      // the read buffer is stale
    c->pos = pos;
    return true;
}
