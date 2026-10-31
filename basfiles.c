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

#include <dirent.h>
#include <glob.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "basfiles.h"

typedef struct _fileent_t {
    char*           shown;          // the name that is listed
    char*           path;           // where to stat it
    struct stat     st;
    char*           target;         // symbolic link: where it points to (0 = unknown)
} fileent_t;

typedef struct _filelist_t {
    fileent_t*      ents;
    size_t          count, cap;
} filelist_t;

static void filelist_free( filelist_t* fl ) {
    for ( size_t i=0; i < fl->count; ++i ) {
        free( fl->ents[i].shown );
        free( fl->ents[i].path );
        free( fl->ents[i].target );
    }
    free( fl->ents );
    fl->ents = 0; fl->count = fl->cap = 0;
}

// adds the entry; false if it can't be stat'ed (it vanished meanwhile: skipped) or out of memory (*poom)
static bool filelist_add( filelist_t* fl, const char* shown, const char* path, bool* poom ) {
    struct stat st;
    if ( lstat( path, &st ) != 0 ) {
        return false;
    }
    if ( fl->count == fl->cap ) {
        size_t cap = fl->cap ? fl->cap * 2U : 64U;
        fileent_t* ents = (fileent_t*) realloc( fl->ents, cap * sizeof(fileent_t) );
        if ( ents == 0 ) {
            *poom = true;
            return false;
        }
        fl->ents = ents; fl->cap = cap;
    }
    fileent_t* e = &fl->ents[ fl->count ];
    memset( e, 0, sizeof(*e) );
    e->shown = strdup( shown );
    e->path = strdup( path );
    e->st = st;
    if ( e->shown == 0 || e->path == 0 ) {
        free( e->shown ); free( e->path );
        *poom = true;
        return false;
    }
    if ( S_ISLNK( st.st_mode ) ) {
        char buf[4096];
        ssize_t n = readlink( path, buf, sizeof(buf) - 1U );
        if ( n >= 0 ) {
            buf[n] = '\0';
            e->target = strdup( buf );
        }
    }
    ++fl->count;
    return true;
}

// oldest first; equal times by name
static int filelist_compare( const void* pa, const void* pb ) {
    const fileent_t* a = (const fileent_t*) pa;
    const fileent_t* b = (const fileent_t*) pb;
    if ( a->st.st_mtime != b->st.st_mtime ) {
        return a->st.st_mtime < b->st.st_mtime ? -1 : 1;
    }
    if ( a->st.st_mtim.tv_nsec != b->st.st_mtim.tv_nsec ) {
        return a->st.st_mtim.tv_nsec < b->st.st_mtim.tv_nsec ? -1 : 1;
    }
    return strcmp( a->shown, b->shown );
}

static bool files_has_wildcard( const char* spec ) {
    return strpbrk( spec, "*?[" ) != 0;
}

// the entries of a directory (hidden names are left out)
static bool files_read_dir( filelist_t* fl, const char* dirname, char* err, size_t errsz ) {
    DIR* d = opendir( dirname );
    if ( d == 0 ) {
        snprintf( err, errsz, "%s", errno == ENOENT ? "File not found" : "Can't open directory" );
        return false;
    }
    bool oom = false;
    struct dirent* de;
    size_t dirlen = strlen( dirname );
    while ( !oom && ( de = readdir( d ) ) != 0 ) {
        if ( de->d_name[0] == '.' ) {
            continue;
        }
        size_t len = dirlen + strlen( de->d_name ) + 2U;
        char* path = (char*) malloc( len );
        if ( path == 0 ) {
            oom = true;
            break;
        }
        snprintf( path, len, "%s%s%s", dirname, dirlen > 0 && dirname[ dirlen - 1U ] == '/' ? "" : "/", de->d_name );
        filelist_add( fl, de->d_name, path, &oom );
        free( path );
    }
    closedir( d );
    if ( oom ) {
        snprintf( err, errsz, "Out of memory" );
        return false;
    }
    return true;
}

static bool files_collect( filelist_t* fl, const char* spec, char* err, size_t errsz ) {
    bool oom = false;
    if ( spec[0] == '\0' ) {
        return files_read_dir( fl, ".", err, errsz );
    }
    if ( files_has_wildcard( spec ) ) {
        glob_t g;
        memset( &g, 0, sizeof(g) );
        int rv = glob( spec, 0, 0, &g );
        if ( rv == GLOB_NOMATCH ) {
            snprintf( err, errsz, "File not found" );
            return false;
        }
        if ( rv != 0 ) {
            snprintf( err, errsz, "%s", rv == GLOB_NOSPACE ? "Out of memory" : "Can't read directory" );
            return false;
        }
        for ( size_t i=0; i < g.gl_pathc && !oom; ++i ) {
            filelist_add( fl, g.gl_pathv[i], g.gl_pathv[i], &oom );
        }
        globfree( &g );
        if ( oom ) {
            snprintf( err, errsz, "Out of memory" );
            return false;
        }
        return true;
    }
    struct stat st;
    if ( stat( spec, &st ) == 0 && S_ISDIR( st.st_mode ) ) {
        return files_read_dir( fl, spec, err, errsz );
    }
    if ( !filelist_add( fl, spec, spec, &oom ) ) {
        snprintf( err, errsz, "%s", oom ? "Out of memory" : "File not found" );
        return false;
    }
    return true;
}

// -- the text --------------------------------------------------------------------------

typedef struct _text_t {
    char*   s;
    size_t  len, cap;
    bool    oom;
} text_t;

static void text_add( text_t* t, const char* piece ) {
    if ( piece == 0 || t->oom ) {
        return;
    }
    size_t n = strlen( piece );
    if ( t->len + n + 1U > t->cap ) {
        size_t cap = t->cap ? t->cap : 256U;
        while ( cap < t->len + n + 1U ) {
            cap *= 2U;
        }
        char* s = (char*) realloc( t->s, cap );
        if ( s == 0 ) {
            t->oom = true;
            return;
        }
        t->s = s; t->cap = cap;
    }
    memcpy( t->s + t->len, piece, n + 1U );
    t->len += n;
}

static bool has_extension_ci( const char* name, const char* ext ) {
    size_t ln = strlen( name ), le = strlen( ext );
    return ln > le && strcasecmp( name + ln - le, ext ) == 0;
}

bool files_format_entry( int sizewidth, long long size, time_t mtime, hlclass_t nameclass, const char* name, const char* indicator,
    const char* target, hlstyle_t style, void* styleud, files_emit_t emit, void* emitud, bool* pstop, char* err, size_t errsz ) {
    char sizetext[48], timetext[32];
    snprintf( sizetext, sizeof(sizetext), "%*lld", sizewidth, size );
    struct tm tmv;
    if ( localtime_r( &mtime, &tmv ) == 0 || strftime( timetext, sizeof(timetext), "%Y-%m-%d %H:%M", &tmv ) == 0 ) {
        snprintf( timetext, sizeof(timetext), "unknown         " );
    }
    text_t t;
    memset( &t, 0, sizeof(t) );
    text_add( &t, style ? style( HL_NUMBER, styleud ) : 0 );
    text_add( &t, sizetext );
    text_add( &t, " " );
    text_add( &t, style ? style( HL_COMMENT, styleud ) : 0 );
    text_add( &t, timetext );
    text_add( &t, " " );
    text_add( &t, style ? style( nameclass, styleud ) : 0 );
    text_add( &t, name );
    text_add( &t, indicator );
    if ( target != 0 ) {
        text_add( &t, " -> " );
        text_add( &t, target );
    }
    text_add( &t, style ? style( HL_NONE, styleud ) : 0 );
    if ( t.oom ) {
        free( t.s );
        snprintf( err, errsz, "Out of memory" );
        return false;
    }
    *pstop = !emit( emitud, t.s );
    free( t.s );
    return true;
}

bool files_list( const char* spec, hlstyle_t style, void* styleud, files_emit_t emit, void* emitud, char* err, size_t errsz ) {
    filelist_t fl;
    memset( &fl, 0, sizeof(fl) );
    if ( !files_collect( &fl, spec, err, errsz ) ) {
        filelist_free( &fl );
        return false;
    }
    qsort( fl.ents, fl.count, sizeof(fileent_t), filelist_compare );
    int sizewidth = 1;
    for ( size_t i=0; i < fl.count; ++i ) {
        char num[32];
        int w = snprintf( num, sizeof(num), "%lld", (long long) fl.ents[i].st.st_size );
        if ( w > sizewidth ) {
            sizewidth = w;
        }
    }
    bool ok = true, stop = false;
    for ( size_t i=0; i < fl.count && ok && !stop; ++i ) {
        const fileent_t* e = &fl.ents[i];
        mode_t m = e->st.st_mode;
        char indicator[2] = { '\0', '\0' };
        hlclass_t nameclass = HL_IDENT;
        if ( S_ISDIR( m ) ) {
            indicator[0] = '/'; nameclass = HL_FILEDIR;
        } else if ( S_ISLNK( m ) ) {
            indicator[0] = '@'; nameclass = HL_FILELINK;
        } else if ( S_ISFIFO( m ) ) {
            indicator[0] = '|';
        } else if ( S_ISSOCK( m ) ) {
            indicator[0] = '=';
        } else if ( S_ISREG( m ) && ( m & ( S_IXUSR | S_IXGRP | S_IXOTH ) ) != 0 ) {
            indicator[0] = '*'; nameclass = HL_FILEEXEC;
        } else if ( has_extension_ci( e->shown, ".bas" ) || has_extension_ci( e->shown, ".e99" ) ) {
            nameclass = HL_FILEPROG;
        }
        ok = files_format_entry( sizewidth, (long long) e->st.st_size, e->st.st_mtime, nameclass, e->shown, indicator, e->target,
            style, styleud, emit, emitud, &stop, err, errsz );
    }
    filelist_free( &fl );
    return ok;
}
