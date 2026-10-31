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

#include <sys/stat.h>
#include "basrepl.h"
#include "basdebug.h"
#include "basfiles.h"
#include "iodev.h"

// -- helpers -------------------------------------------------------------------

// reports an error text; makes sure that it ends in a newline
static void repl_report( repl_t* repl, const char* text ) {
    size_t len = strlen( text );
    bool nl = len > 0 && text[ len - 1U ] == '\n';
    if ( repl->report ) {
        if ( nl ) {
            repl->report( repl, text );
        } else {
            char* copy = (char*) malloc( len + 2U );
            if ( copy ) {
                memcpy( copy, text, len );
                copy[ len ] = '\n'; copy[ len + 1U ] = '\0';
                repl->report( repl, copy );
                free( copy );
            }
        }
    } else {
        fputs( text, stderr );
        if ( !nl ) {
            fputc( '\n', stderr );
        }
    }
}

static void repl_rt_report( struct _runtime_t* rt, void* usrdata, const char* text ) {
    repl_report( (repl_t*) usrdata, text );
}

static void repl_rt_halt( struct _runtime_t* rt, void* usrdata ) ATTR_NORETURN;

static void repl_rt_halt( struct _runtime_t* rt, void* usrdata ) {
    longjmp( ((repl_t*) usrdata)->jmp, 1 );
}

static void repl_comp_report( struct _compiler_t* comp, void* usrdata, const char* text ) {
    repl_report( (repl_t*) usrdata, text );
}

static void repl_comp_halt( struct _compiler_t* comp, void* usrdata ) ATTR_NORETURN;

static void repl_comp_halt( struct _compiler_t* comp, void* usrdata ) {
    longjmp( ((repl_t*) usrdata)->jmp, 1 );
}

static void repl_vm_report( struct _vm_t* vm, void* usrdata, const char* text ) {
    repl_report( (repl_t*) usrdata, text );
}

// -- setup -----------------------------------------------------------------------

// (re)initializes the runtime and the VM; the program is not touched
// keep != 0 (CHAIN): the old runtime is not cleaned up but moved to *keep (the caller cleans it up), and the stored sound
// effects stay
static void repl_reset_runtime_ex( repl_t* repl, runtime_t* keep ) {
    mml_t sfx[SND_MAXSFX];
    bool sfxdef[SND_MAXSFX];
    memset( sfx, 0, sizeof(sfx) );
    memset( sfxdef, 0, sizeof(sfxdef) );
    if ( repl->vmready ) {
        if ( keep != 0 ) {
            memcpy( sfx, repl->vm.sfx, sizeof(sfx) );
            memcpy( sfxdef, repl->vm.sfx_defined, sizeof(sfxdef) );
            memset( repl->vm.sfx, 0, sizeof(repl->vm.sfx) );
            memset( repl->vm.sfx_defined, 0, sizeof(repl->vm.sfx_defined) );
        }
        cleanup_vm( &repl->vm );
    }
    if ( keep != 0 ) {
        *keep = repl->rt;
    } else {
        cleanup_runtime( &repl->rt );
    }
    init_runtime( &repl->rt );
    repl->rt.userdata = repl;
    repl->rt.report   = repl_rt_report;
    repl->rt.halt     = repl_rt_halt;
    init_vm( &repl->vm, &repl->rt.codegen, &repl->rt.varmem );
    repl->vmready = true;
    if ( keep != 0 ) {
        memcpy( repl->vm.sfx, sfx, sizeof(sfx) );
        memcpy( repl->vm.sfx_defined, sfxdef, sizeof(sfxdef) );
    }
}

static void repl_reset_runtime( repl_t* repl ) {
    repl_reset_runtime_ex( repl, 0 );
}

// puts the runtime that repl_reset_runtime_ex() moved to *old back (the new one is discarded); the stored sound effects stay
static void repl_restore_runtime( repl_t* repl, runtime_t* old ) {
    mml_t sfx[SND_MAXSFX];
    bool sfxdef[SND_MAXSFX];
    memcpy( sfx, repl->vm.sfx, sizeof(sfx) );
    memcpy( sfxdef, repl->vm.sfx_defined, sizeof(sfxdef) );
    memset( repl->vm.sfx, 0, sizeof(repl->vm.sfx) );
    memset( repl->vm.sfx_defined, 0, sizeof(repl->vm.sfx_defined) );
    cleanup_vm( &repl->vm );
    cleanup_runtime( &repl->rt );
    repl->rt = *old;
    init_vm( &repl->vm, &repl->rt.codegen, &repl->rt.varmem );
    memcpy( repl->vm.sfx, sfx, sizeof(sfx) );
    memcpy( repl->vm.sfx_defined, sfxdef, sizeof(sfxdef) );
}

bool init_repl( repl_t* repl ) {
    memset( repl, 0, sizeof(*repl) );
    init_program( &repl->pgm );
    init_runtime( &repl->rt );      // so that cleanup_runtime() in repl_reset_runtime() has something to free
    repl->tokbuf = create_buffer();
    if ( repl->tokbuf == 0 ) {
        cleanup_runtime( &repl->rt );
        return false;
    }
    init_compiler( &repl->comp, &repl->rt, 0, false );
    repl_reset_runtime( repl );
    return true;
}

void cleanup_repl( repl_t* repl ) {
    if ( repl->vmready ) {
        cleanup_vm( &repl->vm );
        repl->vmready = false;
    }
    cleanup_compiler( &repl->comp );
    cleanup_runtime( &repl->rt );
    free_program( &repl->pgm );
    delete_buffer( repl->tokbuf );
    repl->tokbuf = 0;
    free( repl->pending );
    repl->pending = 0;
    free( repl->filename );
    repl->filename = 0;
}

static void repl_set_filename( repl_t* repl, const char* filename ) {
    char* copy = strdup( filename );
    if ( copy != 0 ) {      // out of memory: the name is just not shown
        free( repl->filename );
        repl->filename = copy;
    }
}

static void repl_setup_compiler( repl_t* repl ) {
    cleanup_compiler( &repl->comp );
    init_compiler( &repl->comp, &repl->rt, 0, false );
    repl->comp.userdata = repl;
    repl->comp.halt     = repl_comp_halt;
    repl->comp.report   = repl_comp_report;
}

static bool repl_vm_list( struct _vm_t* vm, void* usrdata, const char* firstlabel, int32_t firstline,
    const char* lastlabel, int32_t lastline );
static bool repl_vm_delete( struct _vm_t* vm, void* usrdata, const char* firstlabel, int32_t firstline,
    const char* lastlabel, int32_t lastline );
static bool repl_vm_renumber( struct _vm_t* vm, void* usrdata, const char* firstlabel, int32_t firstline,
    const char* lastlabel, int32_t lastline, int32_t step );
static bool repl_vm_merge( struct _vm_t* vm, void* usrdata, const char* name, bool append );
static bool repl_vm_files( struct _vm_t* vm, void* usrdata, const char* spec );

// hands the host callbacks to the VM
static void repl_setup_vm( repl_t* repl ) {
    vm_t* vm = &repl->vm;
    vm->userdata  = repl;
    vm->report    = repl_vm_report;
    vm->print     = repl->print;
    vm->printn    = repl->printn;
    vm->inkey     = repl->inkey;
    vm->input     = repl->input;
    vm->getkey    = repl->getkey;
    vm->interrupt = repl->interrupt;
    vm->save      = repl->save;
    vm->sound     = repl->sound;
    vm->list      = repl_vm_list;
    vm->deletelines = repl_vm_delete;
    vm->renumber  = repl_vm_renumber;
    vm->merge     = repl_vm_merge;
    vm->files     = repl_vm_files;
}

// -- running -----------------------------------------------------------------------

// reports a run time error of the host side of a program command (with the line number, like the VM does)
static void repl_vm_error( repl_t* repl, const char* text, const char* arg ) {
    char buf[200];
    if ( repl->vm.line != LINENO_NONE ) {
        snprintf( buf, sizeof(buf), "? %s%s%s in line %u", text, arg ? " " : "", arg ? arg : "", (unsigned) repl->vm.line );
    } else {
        snprintf( buf, sizeof(buf), "? %s%s%s", text, arg ? " " : "", arg ? arg : "" );
    }
    repl_report( repl, buf );
}

// where a run starts: the beginning, or a label / line number (RUN target)
static bool repl_find_start( repl_t* repl, const char* label, int32_t line, uint32_t* pstart ) {
    codegen_t* cgen = &repl->rt.codegen;
    *pstart = 0;
    if ( label && label[0] ) {
        uint16_t varoffs = VAROFFS_NONE; varvalue_t value;
        memset( &value, 0, sizeof(value) );
        if ( !vmem_lookup_var( &repl->rt.varmem, VARTYPEV_LABEL, label, &varoffs ) || varoffs == VAROFFS_NONE ||
             !vmem_get_var( &repl->rt.varmem, varoffs, VARTYPEV_LABEL, 0, 0, &value ) || value.lbloffs == CODEOFFS_NONE ) {
            repl_vm_error( repl, "Undefined label", label );
            return false;
        }
        *pstart = value.lbloffs;
    } else if ( line >= 0 ) {
        if ( !cgen_find_line( cgen, (uint32_t) line, pstart ) ) {
            char num[16];
            snprintf( num, sizeof(num), "%d", (int) line );
            repl_vm_error( repl, "Undefined line number", num );
            return false;
        }
    }
    return true;
}

// compiles what the compiler has been set up with, and runs the result; reentry point of error jumps.
// fromstart: run from the beginning (RUN: DATA pointer reset) instead of from the new code (direct mode); then
// <label> / <line> may name another start (RUN target).
// A run may end with a LOAD / RUN request in the VM; repl_finish() carries it out.
static bool repl_execute( repl_t* repl, bool fromstart, const char* label, int32_t line ) {
    bool ok = false;
    codegen_t* cgen = &repl->rt.codegen;
    uint32_t start = fromstart ? UINT32_C(0) : cgen->codesize;
    uint32_t datastart = cgen->datasize;
    uint32_t persistent = cgen->persistent;
    uint32_t prevlast = cgen->datalast;
    repl_setup_vm( repl );
    if ( !fromstart && repl->linesstale ) {
        cgen->numlineaddrs = 0;     // the program changed since it was compiled: a direct line must not jump into the old code by line number
    }
    if ( setjmp( repl->jmp ) == 0 ) {
        run_compiler( &repl->comp );
        if ( basdbg_on( BASDBG_TREE ) ) {
            basdbg_print_tree( &repl->comp );
        }
        ok = cgen_from_program( cgen, &repl->comp );
        if ( ok && fromstart ) {
            repl->linesstale = false;       // the line table is that of the stored program now
        }
        if ( ok && repl->keeprt != 0 ) {
            vmem_copy_vars( &repl->rt.varmem, &repl->keeprt->varmem );      // CHAIN
        }
        if ( ok && !fromstart && repl->vm.datapos == DATAOFFS_NONE && cgen->datalast != prevlast ) {
            // DATA items were added behind the last one that was read: the next READ gets the first new item
            repl->vm.datapos = prevlast == DATAOFFS_NONE ? cgen->datafirst : cgen_datarec_next( cgen->data, prevlast );
        }
        if ( ok && fromstart ) {
            uint32_t target = 0;
            ok = repl_find_start( repl, label, line, &target );
            repl->started = ok;
            if ( ok && target == 0 ) {
                ok = vm_run( &repl->vm );
            } else if ( ok ) {
                repl->vm.datapos = cgen->datafirst;
                ok = vm_run_at( &repl->vm, target );
            }
        } else if ( ok ) {
            ok = vm_run_at( &repl->vm, start );
        }
    }
    // A direct mode line has done its job once it has run (or failed). Unless it defined something that points into its code
    // or data (a function, a label, DATA items), the code and the constants of the line are discarded, so that a long
    // session doesn't fill the segments.
    if ( !fromstart && cgen->persistent == persistent ) {
        cgen_rollback( cgen, start, datastart );
    }
    return ok;
}

// compiles the stored program into the code segment (the previous code is discarded) without running it
bool repl_compile( repl_t* repl ) {
    repl_reset_runtime( repl );
    repl_setup_compiler( repl );
    if ( !begin_iterate_program( &repl->comp.iter, &repl->pgm ) ) {
        repl_report( repl, "? Empty program" );
        return false;
    }
    comp_set_tokens( &repl->comp, repl->comp.iter.tok, repl->comp.iter.hdr.length - LINEHDR_SIZE );
    repl_setup_vm( repl );
    bool ok = false;
    if ( setjmp( repl->jmp ) == 0 ) {
        run_compiler( &repl->comp );
        ok = cgen_from_program( &repl->rt.codegen, &repl->comp );
    }
    if ( ok ) {
        repl->linesstale = false;
    }
    return ok;
}

// RUN [target]: starts the stored program with all variables cleared; keepvars (CHAIN): the variables that the program
// has in common with the previous one (same name and type) keep their values
static bool repl_start_program_ex( repl_t* repl, const char* label, int32_t line, bool keepvars ) {
    runtime_t old;
    if ( keepvars ) {
        repl_reset_runtime_ex( repl, &old );
    } else {
        repl_reset_runtime( repl );
    }
    bool ok = false;
    repl->started = false;
    repl_setup_compiler( repl );
    if ( !begin_iterate_program( &repl->comp.iter, &repl->pgm ) ) {
        repl_report( repl, "? Empty program" );
    } else {
        comp_set_tokens( &repl->comp, repl->comp.iter.tok, repl->comp.iter.hdr.length - LINEHDR_SIZE );
        repl->keeprt = keepvars ? &old : 0;
        ok = repl_execute( repl, true, label, line );
        repl->keeprt = 0;
    }
    if ( keepvars ) {
        if ( !ok && !repl->started ) {
            repl_restore_runtime( repl, &old );     // the program didn't compile (or the target is missing): the variables stay
        } else {
            cleanup_runtime( &old );
        }
    }
    return ok;
}

static bool repl_start_program( repl_t* repl, const char* label, int32_t line ) {
    return repl_start_program_ex( repl, label, line, false );
}

// -- LOAD ------------------------------------------------------------------------------

// reads the program in the file (binary, or a text listing) into <pgm>; reports the error and returns false
static bool repl_read_program( repl_t* repl, FILE* f, program_t* pgm ) {
    uint8_t version = 0;
    if ( read_program_header( f, &version ) ) {
        uint8_t hdrbytes[LINEHDR_SIZE];
        size_t got;
        while ( ( got = fread( hdrbytes, 1U, LINEHDR_SIZE, f ) ) == LINEHDR_SIZE ) {
            linehdr_t hdr; const uint8_t* p = hdrbytes;
            clear_linehdr( &hdr );
            read_linehdr_raw( &p, &hdr );
            if ( hdr.length <= LINEHDR_SIZE || hdr.lineno > LINENO_MAX ) {
                repl_report( repl, "? Bad program file" );
                return false;
            }
            uint8_t* line = (uint8_t*) malloc( hdr.length );
            if ( line == 0 ) {
                repl_report( repl, "? Out of memory" );
                return false;
            }
            memcpy( line, hdrbytes, LINEHDR_SIZE );
            size_t toklen = hdr.length - LINEHDR_SIZE;
            bool ok = fread( line + LINEHDR_SIZE, 1U, toklen, f ) == toklen && line[ hdr.length - 1U ] == TOK_EOL;
            if ( !ok ) {
                repl_report( repl, "? Bad program file" );
            } else if ( !enter_line( pgm, line, 0 ) ) {
                repl_report( repl, "? Out of memory" );
                ok = false;
            }
            free( line );
            if ( !ok ) {
                return false;
            }
        }
        if ( got != 0 ) {
            repl_report( repl, "? Bad program file" );
            return false;
        }
        return true;
    }
    if ( version != 0 ) {
        repl_report( repl, "? Unsupported program file version" );
        return false;
    }
    // a text listing: lines with line numbers
    buf_t* tokbuf = create_buffer();
    if ( tokbuf == 0 ) {
        repl_report( repl, "? Out of memory" );
        return false;
    }
    bool ok = true;
    char* text = 0; size_t cap = 0; ssize_t len;
    while ( ok && ( len = getline( &text, &cap, f ) ) >= 0 ) {
        while ( len > 0 && ( text[ len - 1 ] == '\n' || text[ len - 1 ] == '\r' ) ) {
            text[ --len ] = '\0';
        }
        preprocess_buffer( text );
        const char* q = text;
        while ( *q == ' ' ) {
            ++q;
        }
        if ( *q == '\0' ) {
            continue;
        }
        linehdr_t hdr;
        clear_buffer( tokbuf );
        if ( !tokenize_line( text, tokbuf, &hdr ) ) {
            repl_report( repl, "? Syntax error in program file" );
            ok = false;
        } else if ( hdr.lineno == LINENO_NONE ) {
            repl_report( repl, "? Line number missing in program file" );
            ok = false;
        } else if ( !enter_line( pgm, (const uint8_t*) tokbuf->buffer, 0 ) ) {
            repl_report( repl, "? Out of memory" );
            ok = false;
        }
    }
    free( text );
    delete_buffer( tokbuf );
    return ok;
}

// -- default file extensions ---------------------------------------------------------

// does the last component of the path have an extension (a '.' that isn't its first character)?
static bool repl_has_extension( const char* name ) {
    const char* base = strrchr( name, '/' );
    base = base != 0 ? base + 1 : name;
    const char* dot = strrchr( base, '.' );
    return dot != 0 && dot != base;
}

// the name with an extension appended (malloc'ed), 0 if out of memory
static char* repl_add_extension( const char* name, const char* ext ) {
    size_t len = strlen( name ) + strlen( ext ) + 1U;
    char* full = (char*) malloc( len );
    if ( full != 0 ) {
        snprintf( full, len, "%s%s", name, ext );
    }
    return full;
}

// LOAD name: a name without an extension is looked up as it is, then with .bas, .e99, .BAS and .E99 (the first that
// exists wins); the result is malloc'ed (the name itself if nothing was found, so that the error is about what was typed)
static char* repl_find_program_file( const repl_t* repl, const char* name ) {
    const char* def = repl->vm.addrcur;
    if ( !repl_has_extension( name ) && !iodev_exists( name, def ) ) {
        static const char* const exts[] = { ".bas", ".e99", ".BAS", ".E99" };
        for ( size_t i=0; i < sizeof(exts) / sizeof(exts[0]); ++i ) {
            char* full = repl_add_extension( name, exts[i] );
            if ( full != 0 && iodev_exists( full, def ) ) {
                return full;
            }
            free( full );
        }
    }
    return strdup( name );
}

// reads the program file (through the drivers; the part of the name behind a "driver:" prefix is what the extension search is about)
// into a memory stream for repl_read_program(); 0 after a message. *pfilename receives the name that was found (malloc'ed).
static FILE* repl_open_program( repl_t* repl, const char* name, char** pfilename, uint8_t** pdata ) {
    char* filename = repl_find_program_file( repl, name );
    if ( filename == 0 ) {
        repl_report( repl, "? Out of memory" );
        return 0;
    }
    const char* err = 0;
    uint8_t* data = 0; size_t len = 0;
    if ( !iodev_read_all( filename, repl->vm.addrcur, &data, &len, &err ) ) {
        char msg[100];
        snprintf( msg, sizeof(msg), "? %s", err ? err : "Can't open file" );
        repl_report( repl, msg );
        free( filename );
        return 0;
    }
    FILE* f = len > 0 ? fmemopen( data, len, "rb" ) : fopen( "/dev/null", "rb" );
    if ( f == 0 ) {
        repl_report( repl, "? Out of memory" );
        free( data );
        free( filename );
        return 0;
    }
    *pfilename = filename;
    *pdata = data;
    return f;
}

// LOAD: replaces the stored program; the old one stays if anything goes wrong
static bool repl_load_program( repl_t* repl, const char* name ) {
    char* filename = 0; uint8_t* data = 0;
    FILE* f = repl_open_program( repl, name, &filename, &data );
    if ( f == 0 ) {
        return false;
    }
    program_t loaded;
    init_program( &loaded );
    bool ok = repl_read_program( repl, f, &loaded );
    if ( ok && ferror( f ) ) {
        repl_report( repl, "? Read error" );
        ok = false;
    }
    fclose( f );
    free( data );
    if ( !ok ) {
        free_program( &loaded );
        free( filename );
        return false;
    }
    free_program( &repl->pgm );
    repl->pgm = loaded;
    repl->linesstale = true;
    repl_set_filename( repl, filename );
    free( filename );
    return true;
}

// MERGE name [, APPEND]: enters the lines of the file into the stored program; the stored program changes only if everything
// is fine. append: the file's lines are numbered 10 apart behind the last stored line (the stored program is not renumbered).
static bool repl_merge_program( repl_t* repl, const char* name, bool append ) {
    char* filename = 0; uint8_t* data = 0;
    FILE* f = repl_open_program( repl, name, &filename, &data );
    if ( f == 0 ) {
        return false;
    }
    free( filename );
    program_t loaded;
    init_program( &loaded );
    bool ok = repl_read_program( repl, f, &loaded );
    if ( ok && ferror( f ) ) {
        repl_report( repl, "? Read error" );
        ok = false;
    }
    fclose( f );
    free( data );
    if ( ok && append && loaded.first != 0 ) {
        uint32_t start = repl->pgm.last != 0 ? ( repl->pgm.last->lineno / 10U + 1U ) * 10U : 10U;
        int status = start > LINENO_MAX ? RENUM_OVERFLOW : renumber_program( &loaded, LINENO_NONE, LINENO_NONE, start, 10U );
        if ( status != RENUM_OK ) {
            repl_vm_error( repl, status == RENUM_OVERFLOW ? "Line number too large" : status == RENUM_MEMORY ? "Out of memory" :
                "Renumbering failed", 0 );
            ok = false;
        }
    }
    char label[80];
    if ( ok && find_label_clash( &repl->pgm, &loaded, label, sizeof(label) ) ) {
        repl_vm_error( repl, "Duplicate label", label );
        ok = false;
    }
    if ( ok ) {
        merge_programs( &repl->pgm, &loaded );
        repl->linesstale = true;
    }
    free_program( &loaded );
    return ok;
}

static bool repl_vm_merge( struct _vm_t* vm, void* usrdata, const char* name, bool append ) {
    return repl_merge_program( (repl_t*) usrdata, name, append );
}

// -- FILES -----------------------------------------------------------------------------

typedef struct _filesout_t {
    repl_t*     repl;
    int         lines;      // on the current page
    bool        stopped;    // the user stopped the listing (not an error)
    bool        broken;     // Break
} filesout_t;

static bool repl_files_emit( void* usrdata, const char* line ) {
    filesout_t* out = (filesout_t*) usrdata;
    repl_t* repl = out->repl;
    if ( repl->interrupt && repl->interrupt( &repl->vm, repl ) ) {
        out->broken = true;
        return false;
    }
    if ( repl->vm.channel == 0 && repl->pagesize > 1 && repl->pause != 0 && out->lines >= repl->pagesize - 1 ) {
        out->lines = 0;
        if ( !repl->pause( repl ) ) {
            out->stopped = true;
            out->broken = repl->interrupt && repl->interrupt( &repl->vm, repl );
            return false;
        }
    }
    ++out->lines;
    if ( repl->print ) {
        repl->print( &repl->vm, repl, repl->vm.channel, line );
        repl->print( &repl->vm, repl, repl->vm.channel, "\n" );
    } else {
        puts( line );
    }
    return true;
}

// FILES [spec]: oldest first like "ls -ltr" (basfiles.c); colored on the screen channel only
static bool repl_vm_files( struct _vm_t* vm, void* usrdata, const char* spec ) {
    repl_t* repl = (repl_t*) usrdata;
    filesout_t out = { repl, 0, false, false };
    char err[100];
    err[0] = '\0';
    bool ok = iodev_list( spec, repl->vm.addrcur, repl->vm.channel == 0 ? repl->hlstyle : 0, repl, repl_files_emit, &out, err, sizeof(err) );
    if ( !ok ) {
        repl_vm_error( repl, err, 0 );
    } else if ( out.broken ) {
        repl_vm_error( repl, "Break", 0 );
        ok = false;
    }
    return ok;
}

// -- EDIT ------------------------------------------------------------------------------

static bool repl_list_end( repl_t* repl, const char* label, int32_t line, uint32_t* plineno );

// EDIT [line|label]: lets the host edit the text of a stored line; the result is entered like a typed line afterwards
// (repl->pending), not here, so that nothing nests. Cancelling is not an error.
static bool repl_edit_line( repl_t* repl, const char* label, int32_t line ) {
    uint32_t lineno = LINENO_NONE;
    if ( label[0] == '\0' && line < 0 ) {
        repl->fulledit = true;      // the host opens the full-screen editor when repl_enter() has returned
        return true;
    }
    if ( !repl_list_end( repl, label, line, &lineno ) ) {
        return false;
    }
    line_t* found = 0; line_t* prev = 0;
    if ( !find_line( &repl->pgm, lineno, &found, &prev ) ) {
        char num[16];
        snprintf( num, sizeof(num), "%u", (unsigned) lineno );
        repl_vm_error( repl, "Undefined line number", num );
        return false;
    }
    if ( repl->edit == 0 ) {
        repl_vm_error( repl, "No line editor", 0 );
        return false;
    }
    buf_t* text = create_buffer();
    if ( text == 0 ) {
        repl_report( repl, "? Out of memory" );
        return false;
    }
    pgmiter_t iter; clear_iter( &iter, &repl->pgm );
    bool ok = iter_load_line( &iter, found ) && detokenize_line( text, iter.tok, &iter.hdr );
    if ( ok ) {
        repl->pending = repl->edit( repl, text->buffer );
    } else {
        repl_report( repl, "? Out of memory" );
    }
    delete_buffer( text );
    return ok;
}

// carries out the LOAD / RUN requests the program run left in the VM, until a run ends without one (iteratively:
// a program that ends with RUN must not nest); a LOAD typed in direct mode only loads (direct = the run was a direct line)
static bool repl_finish( repl_t* repl, bool ok, bool direct ) {
    bool chained = false;       // the run was started by a request, not by the host
    while ( ok && repl->vm.request != VMREQ_NONE ) {
        int kind = repl->vm.request;
        char* name = strdup( repl->vm.reqname ? repl->vm.reqname : "" );
        int32_t line = repl->vm.reqline;
        char* chainlabel = kind == VMREQ_CHAIN ? strdup( repl->vm.reqlabel ? repl->vm.reqlabel : "" ) : 0;
        bool chainmerge = repl->vm.reqmerge;
        vm_clear_request( &repl->vm );
        if ( name == 0 || ( kind == VMREQ_CHAIN && chainlabel == 0 ) ) {
            free( name );
            free( chainlabel );
            repl_report( repl, "? Out of memory" );
            return false;
        }
        // each run is too short for the VM's own polling when a program restarts itself endlessly (10 RUN)
        if ( chained && repl->interrupt && repl->interrupt( &repl->vm, repl ) ) {
            repl_vm_error( repl, "Break", 0 );
            free( name );
            return false;
        }
        if ( kind == VMREQ_EDIT ) {
            ok = repl_edit_line( repl, name, line );    // back to the host: nothing runs afterwards
            free( name );
            return ok;
        }
        if ( kind == VMREQ_MONITOR ) {
            repl->monitor = true;       // back to the host: nothing runs afterwards
            free( name );
            return true;
        }
        if ( kind == VMREQ_NEW ) {
            repl_new( repl );       // back to the host: nothing runs afterwards
            free( name );
            return true;
        }
        if ( kind == VMREQ_LOAD ) {
            ok = repl_load_program( repl, name );
            if ( direct && !chained ) {
                if ( ok ) {
                    repl_reset_runtime( repl );     // variables are cleared, the program is compiled by RUN
                }
                free( name );
                return ok;
            }
            name[0] = '\0';        // the loaded program runs from its beginning
            line = -1;
        }
        bool keepvars = false;
        if ( kind == VMREQ_CHAIN ) {
            ok = chainmerge ? repl_merge_program( repl, name, false ) : repl_load_program( repl, name );
            free( name );
            name = chainlabel;      // the program starts at the chain target
            keepvars = true;
        }
        if ( ok ) {
            ok = repl_start_program_ex( repl, name, line, keepvars );
        }
        chained = true;
        free( name );
    }
    return ok;
}

static bool repl_run_direct( repl_t* repl, const uint8_t* tokens, size_t size ) {
    repl_setup_compiler( repl );
    repl->comp.iter.hdr.lineno = LINENO_NONE;
    comp_set_tokens( &repl->comp, tokens, size );
    return repl_finish( repl, repl_execute( repl, false, 0, -1 ), true );
}

static bool repl_enter_pending( repl_t* repl, bool ok );

bool repl_run( repl_t* repl ) {
    bool ok = repl_finish( repl, repl_start_program( repl, "", -1 ), false );
    return repl->pending != 0 ? repl_enter_pending( repl, ok ) : ok;     // a program that ended in EDIT
}

// -- debugging (the monitor) ---------------------------------------------------------

// prepares a run of the code segment as it is (it may have been patched by the monitor) at addr: the VM is suspended
// there, with empty stacks; variables are kept
bool repl_vm_start( repl_t* repl, uint32_t addr ) {
    if ( addr >= repl->rt.codegen.codesize ) {
        repl_report( repl, "? Address out of range" );
        return false;
    }
    repl_setup_vm( repl );
    return vm_begin( &repl->vm, addr );
}

// continues the suspended run (until the end of the code, a breakpoint, BRK) or executes one instruction. A run that
// ended is finished like after RUN: a request (LOAD, RUN, NEW, EDIT, MONITOR) is carried on. false if an error was reported.
// repl->vm.stopreason tells whether the VM is still suspended.
bool repl_vm_resume( repl_t* repl, bool step, const uint32_t* breakpoints, size_t numbreakpoints ) {
    repl_setup_vm( repl );
    vm_t* vm = &repl->vm;
    vm->debug = true;
    vm->breakpoints = breakpoints;
    vm->nbreakpoints = numbreakpoints;
    bool ok = false;
    if ( setjmp( repl->jmp ) == 0 ) {
        ok = vm_resume( vm, step );
    }
    vm->debug = false;
    vm->breakpoints = 0;
    vm->nbreakpoints = 0;
    if ( ok && vm->suspended ) {
        return true;        // suspended: nothing to finish
    }
    ok = repl_finish( repl, ok, false );
    return repl->pending != 0 ? repl_enter_pending( repl, ok ) : ok;
}

void repl_new( repl_t* repl ) {
    free_program( &repl->pgm );
    init_program( &repl->pgm );
    repl_reset_runtime( repl );
    free( repl->filename );
    repl->filename = 0;
}

// LIST output goes through the host print callback (list_program's printer has no user data parameter)
static repl_t* listing_repl = 0;

static void repl_list_printer( const char* text ) {
    repl_t* repl = listing_repl;
    // LIST writes to the channel the program selected (the VM's current one)
    if ( repl->print ) {
        repl->print( &repl->vm, repl, repl->vm.channel, text );
        repl->print( &repl->vm, repl, repl->vm.channel, "\n" );
    } else {
        puts( text );
    }
}

void repl_list( repl_t* repl, uint32_t first, uint32_t last ) {
    listing_repl = repl;
    // colors only for the screen channel: other channels (files) get plain text
    list_program_hl( &repl->pgm, first, last, repl_list_printer, repl->vm.channel == 0 ? repl->hlstyle : 0, repl );
}

// one end of a LIST range: a label name (the line it is in) or a line number; LINENO_NONE = unbounded
static bool repl_list_end( repl_t* repl, const char* label, int32_t line, uint32_t* plineno ) {
    *plineno = LINENO_NONE;
    if ( label[0] != '\0' ) {
        if ( !find_label_line( &repl->pgm, label, plineno ) ) {
            repl_vm_error( repl, "Undefined label", label );
            return false;
        }
    } else if ( line >= 0 ) {
        *plineno = (uint32_t) line;
    }
    return true;
}

// the LIST statement
static bool repl_vm_list( struct _vm_t* vm, void* usrdata, const char* firstlabel, int32_t firstline,
    const char* lastlabel, int32_t lastline ) {
    repl_t* repl = (repl_t*) usrdata;
    uint32_t first, last;
    if ( !repl_list_end( repl, firstlabel, firstline, &first ) || !repl_list_end( repl, lastlabel, lastline, &last ) ) {
        return false;
    }
    repl_list( repl, first, last );
    return true;
}

// the DELETE statement
static bool repl_vm_delete( struct _vm_t* vm, void* usrdata, const char* firstlabel, int32_t firstline,
    const char* lastlabel, int32_t lastline ) {
    repl_t* repl = (repl_t*) usrdata;
    uint32_t first, last;
    if ( !repl_list_end( repl, firstlabel, firstline, &first ) || !repl_list_end( repl, lastlabel, lastline, &last ) ) {
        return false;
    }
    delete_lines( &repl->pgm, first, last );
    repl->linesstale = true;
    return true;
}

// the RENUM statement: the new numbers start at the first line of the range (10 without a start)
static bool repl_vm_renumber( struct _vm_t* vm, void* usrdata, const char* firstlabel, int32_t firstline,
    const char* lastlabel, int32_t lastline, int32_t step ) {
    repl_t* repl = (repl_t*) usrdata;
    uint32_t first, last, start = 10;
    if ( !repl_list_end( repl, firstlabel, firstline, &first ) || !repl_list_end( repl, lastlabel, lastline, &last ) ) {
        return false;
    }
    if ( first != LINENO_NONE ) {
        start = first;
    }
    repl->linesstale = true;
    switch ( renumber_program( &repl->pgm, first, last, start, (uint32_t) step ) ) {
        case RENUM_OK:
            return true;
        case RENUM_OVERLAP:
            repl_vm_error( repl, "Renumbering would overlap other lines", 0 );
            break;
        case RENUM_OVERFLOW:
            repl_vm_error( repl, "Line number too large", 0 );
            break;
        case RENUM_MEMORY:
            repl_vm_error( repl, "Out of memory", 0 );
            break;
        default:
            repl_vm_error( repl, "Renumbering failed", 0 );
            break;
    }
    return false;
}

// SAVE name: a name without an extension gets .bas (ASCII listing) or .e99 (binary, tokenized)
bool repl_save_program( repl_t* repl, const char* name, bool ascii ) {
    char* filename = repl_has_extension( name ) ? strdup( name ) : repl_add_extension( name, ascii ? ".bas" : ".e99" );
    if ( filename == 0 ) {
        return false;
    }
    char* buf = 0; size_t size = 0;
    FILE* f = open_memstream( &buf, &size );
    if ( f == 0 ) {
        free( filename );
        return false;
    }
    bool ok = save_program( &repl->pgm, f, ascii );
    if ( fclose( f ) != 0 ) {
        ok = false;
    }
    const char* err = 0;
    if ( ok && !iodev_write_all( filename, repl->vm.addrcur, buf, size, &err ) ) {
        char msg[100];
        snprintf( msg, sizeof(msg), "? %s", err ? err : "Write error" );
        repl_report( repl, msg );
        ok = false;
    }
    free( buf );
    if ( ok ) {
        repl_set_filename( repl, filename );
    }
    free( filename );
    return ok;
}

// -- input lines ---------------------------------------------------------------------

static bool repl_enter_line( repl_t* repl, const char* text ) {
    char* copy = strdup( text );
    if ( copy == 0 ) {
        repl_report( repl, "? Out of memory" );
        return false;
    }
    preprocess_buffer( copy );
    bool ok = true;
    linehdr_t hdr;
    clear_buffer( repl->tokbuf );
    if ( !tokenize_line( copy, repl->tokbuf, &hdr ) ) {
        repl_report( repl, "? Syntax error" );
        ok = false;
    } else {
        const uint8_t* tokline = (const uint8_t*) repl->tokbuf->buffer;
        if ( basdbg_on( BASDBG_TOKENIZE ) ) {
            basdbg_print_tokens( tokline );
        }
        if ( basdbg_on( BASDBG_DETOKENIZE ) ) {
            basdbg_print_detokenized( tokline );
        }
        if ( hdr.lineno == LINENO_NONE ) {
            if ( hdr.length > LINEHDR_SIZE + 1U ) {     // more than the end-of-line token
                ok = repl_run_direct( repl, tokline + LINEHDR_SIZE, hdr.length - LINEHDR_SIZE );
            }
        } else {
            ok = enter_line( &repl->pgm, tokline, 0 );
            repl->linesstale = true;
            if ( !ok ) {
                repl_report( repl, "? Out of memory" );
            }
        }
    }
    free( copy );
    return ok;
}

// enters the lines the line editor left behind (an edited line may be a direct mode line that runs EDIT again, which
// leaves the next one: a loop, not a recursion)
static bool repl_enter_pending( repl_t* repl, bool ok ) {
    while ( repl->pending != 0 ) {
        char* line = repl->pending;
        repl->pending = 0;
        ok = repl_enter_line( repl, line );
        free( line );
    }
    return ok;
}

bool repl_enter( repl_t* repl, const char* text ) {
    return repl_enter_pending( repl, repl_enter_line( repl, text ) );
}
