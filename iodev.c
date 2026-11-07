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

    ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
        applies. Programs written in the built-in programming language, and
        executables made from them with BUILD, do not fall under this license
        as the exception describes (see the file LICENSE-EXCEPTION).

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#include <ctype.h>
#include <dirent.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>
#include <sys/socket.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include "iodev.h"
#include "policy.h"

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
    return policy_check( operation, drv != 0 ? drv->name : 0, path );      // the security policy (policy.[ch], docs/security-policy.md): only while a program runs
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

// a driver that fails in read / write can leave a better message than "Device I/O error" here
static const char* iodev_pending = 0;

static const char* iodev_fail( const char* fallback ) {
    const char* e = iodev_pending != 0 ? iodev_pending : fallback;
    iodev_pending = 0;
    return e;
}

static bool (*iodev_intr)( void* ) = 0;
static void* iodev_intrud = 0;

void iodev_set_interrupt( bool (*func)( void* usrdata ), void* usrdata ) {
    iodev_intr = func;
    iodev_intrud = usrdata;
}

static const char* iodev_known_error( const char* err );

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
    if ( ( mode & IOMODE_READ ) != 0 && ( mode & IOMODE_WRITE ) != 0 && ( mode & IOMODE_APPEND ) == 0 ) {
        fmode = stat( path, &st ) == 0 ? "r+b" : "w+b";         // UPDATE / RANDOM: keep what is there
    }
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
    bool update = ( mode & IOMODE_READ ) != 0 && ( mode & IOMODE_WRITE ) != 0;
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
    } else if ( ( mode & IOMODE_WRITE ) != 0 && ( mode & IOMODE_APPEND ) == 0 && !update ) {
        f->len = 0;             // OUTPUT truncates (UPDATE and RANDOM do not)
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
    memfile_t* f = h->file;
    if ( pos < 0 || pos > 0x7FFFFFFF ) {
        return false;
    }
    if ( (uint64_t) pos > (uint64_t) f->len ) {
        if ( ( h->mode & IOMODE_WRITE ) == 0 ) {
            return false;       // reading behind the end
        }
        if ( (size_t) pos > f->cap ) {      // writing behind the end: the gap is filled with zeros
            uint8_t* data = (uint8_t*) realloc( f->data, (size_t) pos );
            if ( data == 0 ) {
                return false;
            }
            f->data = data; f->cap = (size_t) pos;
        }
        memset( f->data + f->len, 0, (size_t) pos - f->len );
        f->len = (size_t) pos;
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

// -- the config: and local: drivers (a program's own files, docs/io-subsystem.md "config: and local:") -----------------------------
//
// config: = the tree below $XDG_CONFIG_HOME/<program>/ (default ~/.config/<program>/), local: = below $XDG_DATA_HOME/<program>/ (default
// ~/.local/share/<program>/). <program> is the name of the running program (iodev_set_program_name()). A path can't leave the tree: no
// absolute names, no ".." and no symbolic link on the way (checked with realpath() / lstat()); the folders are created when they are
// needed (mode 0700); a program's trees together are limited to OWN_QUOTA bytes. Without a usable program name (typed in, or the
// reserved name "engine1999": ~/.config/engine1999/grants holds the security policy's permissions) the drivers do not fail: the name
// is used as a plain file name, as if there were no prefix (the file driver's rules, i.e. the security policy, apply then).

#include <pwd.h>

#define OWN_QUOTA_DEFAULT   ( UINT64_C(64) * UINT64_C(1048576) )
#define OWN_NAME_MAX        64

static char own_name[ OWN_NAME_MAX + 1 ];       // "" = no name
static uint64_t own_quota = OWN_QUOTA_DEFAULT;

static bool own_name_valid( const char* n ) {
    size_t len = strlen( n );
    if ( len == 0 || len > OWN_NAME_MAX || n[0] == '.' || strcasecmp( n, "engine1999" ) == 0 ) {
        return false;
    }
    for ( size_t i=0; i < len; ++i ) {
        unsigned char c = (unsigned char) n[i];
        if ( !( isalnum( c ) || c == '-' || c == '_' || c == '.' ) ) {
            return false;
        }
    }
    return true;
}

// the name of the program: the base name of its file without the extension and without a leading "engine1999-" (the name of an installed
// game), so that "rustinpeace.bas" and the executable "engine1999-rustinpeace" share their files
void iodev_set_program_name( const char* file ) {
    own_name[0] = '\0';
    if ( file == 0 || file[0] == '\0' ) {
        return;
    }
    const char* base = strrchr( file, '/' );
    base = base != 0 ? base + 1 : file;
    char tmp[256];
    snprintf( tmp, sizeof(tmp), "%s", base );
    char* dot = strrchr( tmp, '.' );
    if ( dot != 0 && dot > tmp ) {
        *dot = '\0';
    }
    const char* name = tmp;
    if ( strncasecmp( name, "engine1999-", 11U ) == 0 ) {
        name += 11;
    } else if ( strncasecmp( name, "engine1999c-", 12U ) == 0 ) {
        name += 12;
    }
    if ( own_name_valid( name ) ) {
        snprintf( own_name, sizeof(own_name), "%s", name );
    }
}

const char* iodev_program_name( void ) {
    return own_name;
}

void iodev_set_own_limit( uint64_t bytes ) {
    own_quota = bytes != 0 ? bytes : OWN_QUOTA_DEFAULT;
}

// mkdir -p, mode 0700
static bool own_mkdirs( const char* path ) {
    char tmp[PATH_MAX];
    if ( strlen( path ) >= sizeof(tmp) ) {
        return false;
    }
    snprintf( tmp, sizeof(tmp), "%s", path );
    for ( char* p = tmp + 1; ; ++p ) {
        if ( *p == '/' || *p == '\0' ) {
            char c = *p;
            *p = '\0';
            if ( mkdir( tmp, 0700 ) != 0 && errno != EEXIST ) {
                return false;
            }
            if ( c == '\0' ) {
                break;
            }
            *p = '/';
        }
    }
    return true;
}

static bool own_home( char* out, size_t outsize ) {
    const char* home = getenv( "HOME" );
    if ( home == 0 || home[0] != '/' ) {
        struct passwd* pw = getpwuid( getuid() );
        home = pw != 0 ? pw->pw_dir : 0;
    }
    if ( home == 0 || home[0] != '/' ) {
        return false;
    }
    snprintf( out, outsize, "%s", home );
    return true;
}

// the folder of the program (created when it does not exist); kind 0 = config, 1 = local
static bool own_root( int kind, char* out, size_t outsize ) {
    char base[PATH_MAX];
    const char* env = getenv( kind == 0 ? "XDG_CONFIG_HOME" : "XDG_DATA_HOME" );
    if ( env != 0 && env[0] == '/' ) {
        snprintf( base, sizeof(base), "%s", env );
    } else {
        char home[PATH_MAX];
        if ( !own_home( home, sizeof(home) ) ) {
            return false;
        }
        const char* sub = kind == 0 ? "/.config" : "/.local/share";
        if ( strlen( home ) + strlen( sub ) >= sizeof(base) ) {
            return false;
        }
        strcpy( base, home );
        strcat( base, sub );
    }
    size_t bl = strlen( base );
    while ( bl > 1U && base[bl-1U] == '/' ) {
        base[--bl] = '\0';
    }
    int n = snprintf( out, outsize, "%s/%s", base, own_name );
    if ( n < 0 || (size_t) n >= outsize ) {
        return false;
    }
    return own_mkdirs( out );
}

// a name that may not leave the folder: not absolute, no ".." component
static bool own_lexical_ok( const char* path ) {
    if ( path[0] == '/' ) {
        return false;
    }
    for ( const char* p = path; *p; ) {
        const char* e = strchr( p, '/' );
        size_t n = e ? (size_t)( e - p ) : strlen( p );
        if ( n == 2U && p[0] == '.' && p[1] == '.' ) {
            return false;
        }
        p += n;
        if ( *p == '/' ) {
            ++p;
        }
    }
    return true;
}

static bool own_inside( const char* real, const char* root ) {
    size_t n = strlen( root );
    return strncmp( real, root, n ) == 0 && ( real[n] == '\0' || real[n] == '/' || n == 1U );
}

// 0 = error (message in err), 1 = full is the path in the folder, 2 = there is no program name: the plain name (rel) is to be used
static int own_resolve( const iodrv_t* drv, const char* rel, char* full, size_t fullsize, char* err, size_t errsz ) {
    if ( own_name[0] == '\0' ) {
        snprintf( full, fullsize, "%s", rel );
        return 2;
    }
    if ( !own_lexical_ok( rel ) ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return 0;
    }
    char root[PATH_MAX];
    if ( !own_root( drv->userdata != 0 ? 1 : 0, root, sizeof(root) ) ) {
        iodev_set_err( err, errsz, "Device I/O error" );
        return 0;
    }
    int n = snprintf( full, fullsize, rel[0] != '\0' ? "%s/%s" : "%s", root, rel );
    if ( n < 0 || (size_t) n >= fullsize ) {
        iodev_set_err( err, errsz, "Bad file name" );
        return 0;
    }
    if ( rel[0] == '\0' ) {
        return 1;
    }
    // the folder that holds the file must really be inside the program's folder (a symbolic link in the way would lead out of it) ...
    char parent[PATH_MAX], realroot[PATH_MAX], realparent[PATH_MAX];
    snprintf( parent, sizeof(parent), "%s", full );
    char* slash = strrchr( parent, '/' );
    if ( slash != 0 && slash != parent ) {
        *slash = '\0';
    }
    if ( realpath( root, realroot ) == 0 || realpath( parent, realparent ) == 0 ) {
        iodev_set_err( err, errsz, "File not found" );
        return 0;
    }
    if ( !own_inside( realparent, realroot ) ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return 0;
    }
    // ... and the file itself must not be a link
    struct stat st;
    if ( lstat( full, &st ) == 0 && S_ISLNK( st.st_mode ) ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return 0;
    }
    return 1;
}

// the bytes in all files below dir (links are not followed)
static uint64_t own_tree_size( const char* dir, int depth ) {
    uint64_t sum = 0;
    DIR* d = opendir( dir );
    if ( d == 0 ) {
        return 0;
    }
    struct dirent* de;
    while ( ( de = readdir( d ) ) != 0 ) {
        if ( strcmp( de->d_name, "." ) == 0 || strcmp( de->d_name, ".." ) == 0 ) {
            continue;
        }
        char path[PATH_MAX];
        if ( snprintf( path, sizeof(path), "%s/%s", dir, de->d_name ) >= (int) sizeof(path) ) {
            continue;
        }
        struct stat st;
        if ( lstat( path, &st ) != 0 ) {
            continue;
        }
        if ( S_ISREG( st.st_mode ) ) {
            sum += (uint64_t) st.st_size;
        } else if ( S_ISDIR( st.st_mode ) && depth < 32 ) {
            sum += own_tree_size( path, depth + 1 );
        }
    }
    closedir( d );
    return sum;
}

typedef struct _ownhandle_t {
    void*       inner;                  // the file driver's handle
    bool        writable;
    uint64_t    used;                   // bytes in the program's trees (measured at OPEN) + what this channel wrote
    uint64_t    limit;
} ownhandle_t;

static void* own_open( iodrv_t* drv, const char* path, unsigned mode, char* err, size_t errsz ) {
    char full[PATH_MAX];
    int r = own_resolve( drv, path, full, sizeof(full), err, errsz );
    if ( r == 0 ) {
        return 0;
    }
    bool writing = ( mode & IOMODE_WRITE ) != 0;
    if ( r == 2 && !iodev_allowed( writing ? "write" : "read", &file_driver, path ) ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return 0;
    }
    void* inner = file_open( &file_driver, full, mode, err, errsz );
    if ( inner == 0 ) {
        return 0;
    }
    ownhandle_t* h = (ownhandle_t*) calloc( 1U, sizeof(*h) );
    if ( h == 0 ) {
        file_close( inner );
        iodev_set_err( err, errsz, "Out of memory" );
        return 0;
    }
    h->inner = inner;
    h->writable = writing;
    h->limit = r == 2 ? UINT64_MAX : own_quota;      // (a plain file in the current directory has no limit of ours)
    if ( writing && r == 1 ) {
        char root0[PATH_MAX], root1[PATH_MAX];
        h->used = ( own_root( 0, root0, sizeof(root0) ) ? own_tree_size( root0, 0 ) : 0 ) +
                  ( own_root( 1, root1, sizeof(root1) ) ? own_tree_size( root1, 0 ) : 0 );
    }
    return h;
}

static ssize_t own_read( void* handle, void* buf, size_t count ) {
    return file_read( ( (ownhandle_t*) handle )->inner, buf, count );
}

static ssize_t own_write( void* handle, const void* buf, size_t count ) {
    ownhandle_t* h = (ownhandle_t*) handle;
    if ( h->used + (uint64_t) count > h->limit ) {
        iodev_pending = "Disk full";
        return -1;
    }
    ssize_t n = file_write( h->inner, buf, count );
    if ( n > 0 ) {
        h->used += (uint64_t) n;
    }
    return n;
}

static int64_t own_size( void* handle ) {
    return file_size( ( (ownhandle_t*) handle )->inner );
}

static bool own_close( void* handle ) {
    ownhandle_t* h = (ownhandle_t*) handle;
    bool ok = file_close( h->inner );
    free( h );
    return ok;
}

static bool own_seek( void* handle, int64_t pos ) {
    return file_seek( ( (ownhandle_t*) handle )->inner, pos );
}

static bool own_list( iodrv_t* drv, const char* spec, hlstyle_t style, void* styleud, files_emit_t emit, void* emitud, char* err,
    size_t errsz ) {
    char full[PATH_MAX];
    int r = own_resolve( drv, spec, full, sizeof(full), err, errsz );
    if ( r == 0 ) {
        return false;
    }
    if ( r == 2 && !iodev_allowed( "list", &file_driver, spec ) ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return false;
    }
    return file_list( &file_driver, full, style, styleud, emit, emitud, err, errsz );
}

static bool own_remove( iodrv_t* drv, const char* path, char* err, size_t errsz ) {
    char full[PATH_MAX];
    int r = own_resolve( drv, path, full, sizeof(full), err, errsz );
    if ( r == 0 ) {
        return false;
    }
    if ( r == 2 && !iodev_allowed( "write", &file_driver, path ) ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return false;
    }
    return file_remove( &file_driver, full, err, errsz );
}

static bool own_rename( iodrv_t* drv, const char* from, const char* to, char* err, size_t errsz ) {
    char f1[PATH_MAX], f2[PATH_MAX];
    int r1 = own_resolve( drv, from, f1, sizeof(f1), err, errsz );
    if ( r1 == 0 ) {
        return false;
    }
    int r2 = own_resolve( drv, to, f2, sizeof(f2), err, errsz );
    if ( r2 == 0 ) {
        return false;
    }
    if ( r1 == 2 && ( !iodev_allowed( "write", &file_driver, from ) || !iodev_allowed( "write", &file_driver, to ) ) ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return false;
    }
    return file_rename( &file_driver, f1, f2, err, errsz );
}

static bool own_mkdir( iodrv_t* drv, const char* path, char* err, size_t errsz ) {
    char full[PATH_MAX];
    int r = own_resolve( drv, path, full, sizeof(full), err, errsz );
    if ( r == 0 ) {
        return false;
    }
    if ( r == 2 && !iodev_allowed( "write", &file_driver, path ) ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return false;
    }
    return file_mkdir( &file_driver, full, err, errsz );
}

// userdata: 0 = config, 1 = local (CHDIR is not offered: it would move the whole process)
static iodrv_t config_driver = { "config", own_open, own_read, own_write, own_size, own_close, own_list, own_remove, own_rename, own_mkdir, 0, own_seek, 0 };
static iodrv_t local_driver = { "local", own_open, own_read, own_write, own_size, own_close, own_list, own_remove, own_rename, own_mkdir, 0, own_seek, (void*) 1 };

// -- the dir driver (a directory as a text file: the data of the file requester) ------------------------

typedef struct _dirhandle_t {
    char*       text;
    size_t      len, pos;
} dirhandle_t;

typedef struct _direntry_t {
    char*       name;                   // with a "/" at the end for a directory
    bool        isdir;
} direntry_t;

static int dir_compare( const void* pa, const void* pb ) {
    const direntry_t* a = (const direntry_t*) pa;
    const direntry_t* b = (const direntry_t*) pb;
    if ( a->isdir != b->isdir ) {
        return a->isdir ? -1 : 1;
    }
    int c = strcasecmp( a->name, b->name );
    return c != 0 ? c : strcmp( a->name, b->name );
}

// does name match one of the patterns of pat (separated by ";")? An empty pattern matches everything
static bool dir_match( const char* pat, const char* name ) {
    if ( pat[0] == '\0' ) {
        return true;
    }
    char one[256];
    while ( *pat ) {
        size_t n = strcspn( pat, ";" );
        if ( n > 0 && n < sizeof(one) ) {
            memcpy( one, pat, n );
            one[n] = '\0';
            if ( fnmatch( one, name, FNM_PERIOD ) == 0 ) {
                return true;
            }
        }
        pat += n;
        if ( *pat == ';' ) {
            ++pat;
        }
    }
    return false;
}

// "dir:", "dir:/some/dir", "dir:/some/dir/*.bas;*.txt", "dir:*.bas". First line: the full name of the directory. Then the
// directories (name and "/"), then the files that match the pattern; each in alphabetical order; hidden names only when asked for.
static void* dir_open( iodrv_t* drv, const char* path, unsigned mode, char* err, size_t errsz ) {
    (void) drv;
    if ( mode & IOMODE_WRITE ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return 0;
    }
    char dirname[4096];
    const char* pat = "";
    const char* slash = strrchr( path, '/' );
    const char* last = slash ? slash + 1 : path;
    if ( strpbrk( last, "*?[" ) != 0 ) {
        pat = last;
        size_t n = slash ? (size_t)( slash - path ) + ( slash == path ? 1U : 0U ) : 0U;
        if ( n >= sizeof(dirname) ) {
            iodev_set_err( err, errsz, "Bad file name" );
            return 0;
        }
        memcpy( dirname, path, n );
        dirname[n] = '\0';
    } else {
        if ( strlen( path ) >= sizeof(dirname) ) {
            iodev_set_err( err, errsz, "Bad file name" );
            return 0;
        }
        strcpy( dirname, path );
    }
    char full[4200];
    if ( realpath( dirname[0] ? dirname : ".", full ) == 0 ) {
        iodev_set_err( err, errsz, iodev_errno_text( errno ) );
        return 0;
    }
    DIR* d = opendir( full );
    if ( d == 0 ) {
        iodev_set_err( err, errsz, iodev_errno_text( errno ) );
        return 0;
    }
    bool wanthidden = pat[0] == '.';
    direntry_t* ent = 0;
    size_t n = 0, cap = 0;
    bool ok = true;
    struct dirent* de;
    while ( ok && ( de = readdir( d ) ) != 0 ) {
        const char* nm = de->d_name;
        if ( strcmp( nm, "." ) == 0 || strcmp( nm, ".." ) == 0 || ( nm[0] == '.' && !wanthidden ) ) {
            continue;
        }
        char fn[4400];
        struct stat st;
        snprintf( fn, sizeof(fn), "%s/%s", full, nm );
        bool isdir = stat( fn, &st ) == 0 && S_ISDIR( st.st_mode );
        if ( !isdir && !dir_match( pat, nm ) ) {
            continue;
        }
        if ( n == cap ) {
            size_t ncap = cap ? cap * 2U : 64U;
            direntry_t* ne = (direntry_t*) realloc( ent, ncap * sizeof(*ne) );
            if ( ne == 0 ) {
                ok = false;
                break;
            }
            ent = ne; cap = ncap;
        }
        size_t l = strlen( nm );
        ent[n].name = (char*) malloc( l + 2U );
        if ( ent[n].name == 0 ) {
            ok = false;
            break;
        }
        memcpy( ent[n].name, nm, l );
        if ( isdir ) {
            ent[n].name[l++] = '/';
        }
        ent[n].name[l] = '\0';
        ent[n].isdir = isdir;
        ++n;
    }
    closedir( d );
    dirhandle_t* h = ok ? (dirhandle_t*) calloc( 1U, sizeof(*h) ) : 0;
    if ( h != 0 ) {
        qsort( ent, n, sizeof(*ent), dir_compare );
        size_t total = strlen( full ) + 2U;
        for ( size_t i=0; i < n; ++i ) {
            total += strlen( ent[i].name ) + 1U;
        }
        h->text = (char*) malloc( total );
        if ( h->text == 0 ) {
            free( h );
            h = 0;
        } else {
            size_t o = (size_t) sprintf( h->text, "%s\n", full );
            for ( size_t i=0; i < n; ++i ) {
                o += (size_t) sprintf( h->text + o, "%s\n", ent[i].name );
            }
            h->len = o;
        }
    }
    for ( size_t i=0; i < n; ++i ) {
        free( ent[i].name );
    }
    free( ent );
    if ( h == 0 ) {
        iodev_set_err( err, errsz, "Out of memory" );
    }
    return h;
}

static ssize_t dir_read( void* handle, void* buf, size_t count ) {
    dirhandle_t* h = (dirhandle_t*) handle;
    size_t n = h->len - h->pos;
    if ( n > count ) {
        n = count;
    }
    memcpy( buf, h->text + h->pos, n );
    h->pos += n;
    return (ssize_t) n;
}

static int64_t dir_size( void* handle ) {
    return (int64_t) ( (dirhandle_t*) handle )->len;
}

static bool dir_close( void* handle ) {
    dirhandle_t* h = (dirhandle_t*) handle;
    free( h->text );
    free( h );
    return true;
}

static iodrv_t dir_driver = { "dir", dir_open, dir_read, 0, dir_size, dir_close, 0, 0, 0, 0, 0, 0, 0 };

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

// -- the stdin, stdout and stderr drivers (the process's standard streams as channels) -------------------

// `OPEN "stdout:" FOR OUTPUT AS #1`, `OPEN "stderr:" ...`, `OPEN "stdin:" FOR INPUT AS #2`: for scripts (docs/console-host.md). The name
// has nothing behind the colon. stdout is the C library's stdout, so it stays in order with what a host prints through it.

static void* std_open( iodrv_t* drv, const char* path, unsigned mode, char* err, size_t errsz ) {
    int fd = (int)(intptr_t) drv->userdata;
    if ( path[0] != '\0' ) {
        iodev_set_err( err, errsz, "Bad file name" );
        return 0;
    }
    bool wr = ( mode & IOMODE_WRITE ) != 0, rd = ( mode & IOMODE_READ ) != 0;
    if ( ( mode & IOMODE_RANDOM ) != 0 || ( fd == 0 ? wr : rd ) ) {
        iodev_set_err( err, errsz, "Bad file mode" );
        return 0;
    }
    return (void*)( (uintptr_t) fd + 1U );     // a non-null tag
}

// at most one line per call, byte by byte, so that nothing behind the line is taken away from INPUT, GET and the host's own reads
static ssize_t stdin_read( void* handle, void* buf, size_t count ) {
    (void) handle;
    uint8_t* p = (uint8_t*) buf;
    size_t n = 0;
    fflush( stdout );
    while ( n < count ) {
        uint8_t c;
        ssize_t r = read( 0, &c, 1U );
        if ( r < 0 ) {
            if ( errno == EINTR ) {
                continue;
            }
            return n > 0 ? (ssize_t) n : -1;
        }
        if ( r == 0 ) {
            break;
        }
        p[ n++ ] = c;
        if ( c == '\n' ) {
            break;
        }
    }
    return (ssize_t) n;
}

static ssize_t stdout_write( void* handle, const void* buf, size_t count ) {
    (void) handle;
    return fwrite( buf, 1U, count, stdout ) == count ? (ssize_t) count : -1;
}

static ssize_t stderr_write( void* handle, const void* buf, size_t count ) {
    (void) handle;
    fflush( stdout );           // errors appear in order with the output before them
    return fwrite( buf, 1U, count, stderr ) == count ? (ssize_t) count : -1;
}

static int64_t std_size( void* handle ) {
    (void) handle;
    return -1;
}

static bool std_close( void* handle ) {
    (void) handle;
    return true;                // the process's streams stay open
}

static iodrv_t stdin_driver  = { "stdin",  std_open, stdin_read, 0, std_size, std_close, 0, 0, 0, 0, 0, 0, (void*)(intptr_t) 0 };
static iodrv_t stdout_driver = { "stdout", std_open, 0, stdout_write, std_size, std_close, 0, 0, 0, 0, 0, 0, (void*)(intptr_t) 1 };
static iodrv_t stderr_driver = { "stderr", std_open, 0, stderr_write, std_size, std_close, 0, 0, 0, 0, 0, 0, (void*)(intptr_t) 2 };

// -- the play driver (lines of music) ---------------------------------------------------------------

static iomusic_t iodev_music;
static bool iodev_music_set = false;

void iodev_set_music( const iomusic_t* music ) {
    if ( music != 0 ) {
        iodev_music = *music;
    }
    iodev_music_set = music != 0;
}

typedef struct _playhandle_t {
    int         voice;
    char*       line;
    size_t      len, cap;
} playhandle_t;

static void* play_open( iodrv_t* drv, const char* path, unsigned mode, char* err, size_t errsz ) {
    if ( ( mode & IOMODE_READ ) != 0 ) {
        iodev_set_err( err, errsz, "Bad file mode" );
        return 0;
    }
    int voice = 1;
    if ( path[0] != '\0' ) {
        if ( path[1] != '\0' || path[0] < '1' || path[0] > '8' ) {
            iodev_set_err( err, errsz, "Bad file name" );       // "play:" or "play:1" .. "play:8"
            return 0;
        }
        voice = path[0] - '0';
    }
    if ( !iodev_music_set ) {
        iodev_set_err( err, errsz, "Not supported by device" );
        return 0;
    }
    if ( !iodev_allowed( "sound", drv, path ) ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return 0;
    }
    playhandle_t* h = (playhandle_t*) calloc( 1U, sizeof(*h) );
    if ( h == 0 ) {
        iodev_set_err( err, errsz, "Out of memory" );
        return 0;
    }
    h->voice = voice;
    return h;
}

static bool play_flush( playhandle_t* h ) {
    if ( h->len == 0 ) {
        return true;
    }
    h->line[ h->len ] = '\0';
    char err[80];
    err[0] = '\0';
    bool ok = iodev_music_set && iodev_music.play( iodev_music.usrdata, h->voice, h->line, err, sizeof(err) );
    h->len = 0;
    if ( !ok ) {
        static char msg[80];            // the parser's message ("Unknown music command 'Z'") is passed on
        snprintf( msg, sizeof(msg), "%s", err[0] ? err : "Device I/O error" );
        iodev_pending = msg;
    }
    return ok;
}

static ssize_t play_write( void* handle, const void* buf, size_t count ) {
    playhandle_t* h = (playhandle_t*) handle;
    const char* p = (const char*) buf;
    for ( size_t i=0; i < count; ++i ) {
        if ( p[i] == '\n' || p[i] == '\r' ) {
            if ( !play_flush( h ) ) {
                return -1;
            }
            continue;
        }
        if ( h->len + 2U > h->cap ) {
            size_t cap = h->cap ? h->cap * 2U : 128U;
            char* line = (char*) realloc( h->line, cap );
            if ( line == 0 ) {
                return -1;
            }
            h->line = line; h->cap = cap;
        }
        h->line[ h->len++ ] = p[i];
    }
    return (ssize_t) count;
}

static bool play_close( void* handle ) {
    playhandle_t* h = (playhandle_t*) handle;
    bool ok = play_flush( h );          // a last line without a newline is played, too
    free( h->line );
    free( h );
    return ok;
}

static ssize_t play_read( void* handle, void* buf, size_t count ) {
    (void) handle; (void) buf; (void) count;
    return -1;
}

static iodrv_t play_driver = { "play", play_open, play_read, play_write, 0, play_close, 0, 0, 0, 0, 0, 0, 0 };

// -- the tcp driver (a network connection) ------------------------------------------------------------

typedef struct _tcphandle_t {
    int         fd;
} tcphandle_t;

#define TCP_SLICE_MS        100
#define TCP_CONNECT_MS      10000

// waits for the socket; 1 = ready, 0 = timed out (timeout < 0: no limit), -1 = error or Break (iodev_pending is set)
static int tcp_wait( int fd, short events, int timeout ) {
    int waited = 0;
    for (;;) {
        struct pollfd pfd = { fd, events, 0 };
        int r = poll( &pfd, 1U, TCP_SLICE_MS );
        if ( r > 0 ) {
            return 1;
        }
        if ( r < 0 && errno != EINTR ) {
            return -1;
        }
        if ( iodev_intr != 0 && iodev_intr( iodev_intrud ) ) {
            iodev_pending = "Break";
            return -1;
        }
        waited += TCP_SLICE_MS;
        if ( timeout >= 0 && waited >= timeout ) {
            return 0;
        }
    }
}

static void* tcp_open( iodrv_t* drv, const char* path, unsigned mode, char* err, size_t errsz ) {
    (void) mode;
    char host[256], port[16];
    const char* colon = strrchr( path, ':' );
    if ( colon == 0 || colon == path || colon[1] == '\0' || (size_t)( colon - path ) >= sizeof(host) || strlen( colon + 1 ) >= sizeof(port) ) {
        iodev_set_err( err, errsz, "Bad file name" );           // "tcp:host:port", "tcp:[::1]:port"
        return 0;
    }
    size_t hl = (size_t)( colon - path );
    const char* hs = path;
    if ( hl >= 2U && path[0] == '[' && path[hl - 1U] == ']' ) {
        ++hs; hl -= 2U;
    }
    memcpy( host, hs, hl );
    host[hl] = '\0';
    snprintf( port, sizeof(port), "%s", colon + 1 );
    if ( !iodev_allowed( "net", drv, path ) ) {
        iodev_set_err( err, errsz, "Permission denied" );
        return 0;
    }
    struct addrinfo hints, *res = 0;
    memset( &hints, 0, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ( getaddrinfo( host, port, &hints, &res ) != 0 || res == 0 ) {
        iodev_set_err( err, errsz, "Host not found" );
        return 0;
    }
    const char* fail = "Connection refused";
    int fd = -1;
    for ( struct addrinfo* ai = res; ai != 0 && fd < 0; ai = ai->ai_next ) {
        int s = socket( ai->ai_family, ai->ai_socktype, ai->ai_protocol );
        if ( s < 0 ) {
            continue;
        }
        fcntl( s, F_SETFL, fcntl( s, F_GETFL, 0 ) | O_NONBLOCK );
        bool ok = connect( s, ai->ai_addr, ai->ai_addrlen ) == 0;
        if ( !ok && errno == EINPROGRESS ) {
            int w = tcp_wait( s, POLLOUT, TCP_CONNECT_MS );
            int soerr = 0;
            socklen_t sl = sizeof(soerr);
            if ( w > 0 && getsockopt( s, SOL_SOCKET, SO_ERROR, &soerr, &sl ) == 0 && soerr == 0 ) {
                ok = true;
            } else if ( w == 0 ) {
                fail = "Timed out";
            } else if ( w < 0 ) {
                fail = iodev_fail( "Device I/O error" );
            }
        }
        if ( ok ) {
            fd = s;
        } else {
            close( s );
        }
    }
    freeaddrinfo( res );
    if ( fd < 0 ) {
        iodev_set_err( err, errsz, fail );
        return 0;
    }
    tcphandle_t* h = (tcphandle_t*) malloc( sizeof(*h) );
    if ( h == 0 ) {
        close( fd );
        iodev_set_err( err, errsz, "Out of memory" );
        return 0;
    }
    h->fd = fd;
    return h;
}

static ssize_t tcp_read( void* handle, void* buf, size_t count ) {
    tcphandle_t* h = (tcphandle_t*) handle;
    for (;;) {
        ssize_t n = recv( h->fd, buf, count, MSG_DONTWAIT );
        if ( n >= 0 ) {
            return n;               // 0 = the other side closed the connection
        }
        if ( errno == ECONNRESET ) {
            iodev_pending = "Connection reset";
            return -1;
        }
        if ( errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR ) {
            return -1;
        }
        if ( tcp_wait( h->fd, POLLIN, -1 ) < 0 ) {
            return -1;
        }
    }
}

static ssize_t tcp_write( void* handle, const void* buf, size_t count ) {
    tcphandle_t* h = (tcphandle_t*) handle;
    size_t done = 0;
    while ( done < count ) {
        ssize_t n = send( h->fd, (const char*) buf + done, count - done, MSG_NOSIGNAL | MSG_DONTWAIT );
        if ( n > 0 ) {
            done += (size_t) n;
        } else if ( n < 0 && ( errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ) ) {
            if ( tcp_wait( h->fd, POLLOUT, -1 ) < 0 ) {
                return -1;
            }
        } else {
            if ( errno == EPIPE || errno == ECONNRESET ) {
                iodev_pending = "Connection reset";
            }
            return -1;
        }
    }
    return (ssize_t) count;
}

static bool tcp_close( void* handle ) {
    tcphandle_t* h = (tcphandle_t*) handle;
    close( h->fd );
    free( h );
    return true;
}

static iodrv_t tcp_driver = { "tcp", tcp_open, tcp_read, tcp_write, 0, tcp_close, 0, 0, 0, 0, 0, 0, 0 };

void iodev_init( void ) {
    if ( !iodev_ready ) {
        iodev_ready = true;
        iodev_register( &file_driver );
        iodev_register( &null_driver );
        iodev_register( &mem_driver );
        iodev_register( &sh_driver );
        iodev_register( &res_driver );
        iodev_register( &config_driver );
        iodev_register( &local_driver );
        iodev_register( &dir_driver );
        iodev_register( &con_driver );
        iodev_register( &play_driver );
        iodev_register( &tcp_driver );
        iodev_register( &stdin_driver );
        iodev_register( &stdout_driver );
        iodev_register( &stderr_driver );
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
        "Bad file name", "Out of memory", "File already exists", "Bad file mode", "Not supported by device", "Host not found",
        "Connection refused", "Timed out", "Break" };
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
    free( c->recbuf );
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
    return iotab_open_ex( tab, chan, name, defaultdriver, mode, 0U, perr );
}

bool iotab_open_ex( iotab_t* tab, int chan, const char* name, const char* defaultdriver, unsigned mode, size_t reclen, const char** perr ) {
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
        *perr = iodev_known_error( err );
        return false;
    }
    iochan_t* c = (iochan_t*) calloc( 1U, sizeof(*c) );
    if ( c == 0 ) {
        drv->close( h );
        *perr = "Out of memory";
        return false;
    }
    c->drv = drv; c->handle = h; c->mode = mode;
    if ( mode & IOMODE_RANDOM ) {
        c->reclen = reclen ? reclen : IOREC_DEFAULT;
        c->recbuf = (uint8_t*) malloc( c->reclen );
        if ( c->recbuf == 0 || c->reclen > IOREC_MAX || drv->seek == 0 ) {
            *perr = c->recbuf == 0 ? "Out of memory" : c->reclen > IOREC_MAX ? "Illegal argument" : "Not supported by device";
            drv->close( h );
            free( c->recbuf );
            free( c );
            return false;
        }
        memset( c->recbuf, ' ', c->reclen );
    }
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
        *perr = iodev_fail( "Device I/O error" );
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
            *perr = iodev_fail( "Device I/O error" );
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
    size_t len = 0;
    return iotab_readline_n( tab, chan, pline, &len, perr );
}

bool iotab_readline_n( iotab_t* tab, int chan, char** pline, size_t* plen, const char** perr ) {
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
    *plen = len;
    *perr = 0;
    return true;
}

bool iotab_write( iotab_t* tab, int chan, const char* data, size_t len, const char** perr ) {
    iochan_t* c = iotab_get( tab, chan, true, perr );
    if ( c == 0 ) {
        return false;
    }
    if ( len > 0 && ( c->drv->write == 0 || c->drv->write( c->handle, data, len ) != (ssize_t) len ) ) {
        *perr = iodev_fail( "Device I/O error" );
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
        *perr = iodev_fail( "Device I/O error" );
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
    *ppos = ( c->mode & IOMODE_RANDOM ) != 0 ? (int64_t) c->recno : c->pos;     // random files: the number of the last record
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

// -- random files ----------------------------------------------------------------------------------------

static iochan_t* iotab_get_random( iotab_t* tab, int chan, const char** perr ) {
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
    if ( ( c->mode & IOMODE_RANDOM ) == 0 ) {
        *perr = "Bad file mode";
        return 0;
    }
    return c;
}

uint8_t* iotab_recbuf( iotab_t* tab, int chan, size_t* plen ) {
    const char* e = 0;
    iochan_t* c = iotab_get_random( tab, chan, &e );
    if ( c == 0 ) {
        return 0;
    }
    if ( plen != 0 ) {
        *plen = c->reclen;
    }
    return c->recbuf;
}

// the record number to use: 0 = the next one
static bool iochan_recnum( iochan_t* c, uint32_t rec, uint32_t* pnum, const char** perr ) {
    uint32_t num = rec != 0 ? rec : c->recno + 1U;
    if ( num == 0 || num > 0x7FFFFFFFU / 2U ) {
        *perr = "Bad record number";
        return false;
    }
    *pnum = num;
    return true;
}

bool iotab_getrec( iotab_t* tab, int chan, uint32_t rec, const char** perr ) {
    iochan_t* c = iotab_get_random( tab, chan, perr );
    uint32_t num = 0;
    if ( c == 0 || !iochan_recnum( c, rec, &num, perr ) ) {
        return false;
    }
    int64_t at = (int64_t)( num - 1U ) * (int64_t) c->reclen;
    int64_t size = c->drv->size != 0 ? c->drv->size( c->handle ) : -1;
    if ( size >= 0 && at >= size ) {
        *perr = "Input past end";
        return false;
    }
    if ( !c->drv->seek( c->handle, at ) ) {
        *perr = "Bad position";
        return false;
    }
    size_t got = 0;
    while ( got < c->reclen ) {
        ssize_t n = c->drv->read( c->handle, c->recbuf + got, c->reclen - got );
        if ( n < 0 ) {
            *perr = iodev_fail( "Device I/O error" );
            return false;
        }
        if ( n == 0 ) {
            break;
        }
        got += (size_t) n;
    }
    if ( got == 0 ) {
        *perr = "Input past end";
        return false;
    }
    memset( c->recbuf + got, ' ', c->reclen - got );        // a short last record is filled up
    c->recno = num;
    c->rpos = c->rlen = 0;
    c->pos = at + (int64_t) c->reclen;
    return true;
}

bool iotab_putrec( iotab_t* tab, int chan, uint32_t rec, const char** perr ) {
    iochan_t* c = iotab_get_random( tab, chan, perr );
    uint32_t num = 0;
    if ( c == 0 || !iochan_recnum( c, rec, &num, perr ) ) {
        return false;
    }
    int64_t at = (int64_t)( num - 1U ) * (int64_t) c->reclen;
    if ( !c->drv->seek( c->handle, at ) ) {
        *perr = "Bad position";
        return false;
    }
    if ( c->drv->write == 0 || c->drv->write( c->handle, c->recbuf, c->reclen ) != (ssize_t) c->reclen ) {
        *perr = iodev_fail( "Device I/O error" );
        return false;
    }
    c->recno = num;
    c->rpos = c->rlen = 0;
    c->pos = at + (int64_t) c->reclen;
    return true;
}
