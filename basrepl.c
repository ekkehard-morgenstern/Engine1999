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

#include "basrepl.h"
#include "basdebug.h"

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
static void repl_reset_runtime( repl_t* repl ) {
    if ( repl->vmready ) {
        cleanup_vm( &repl->vm );
    }
    cleanup_runtime( &repl->rt );
    init_runtime( &repl->rt );
    repl->rt.userdata = repl;
    repl->rt.report   = repl_rt_report;
    repl->rt.halt     = repl_rt_halt;
    init_vm( &repl->vm, &repl->rt.codegen, &repl->rt.varmem );
    repl->vmready = true;
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

// hands the host callbacks to the VM
static void repl_setup_vm( repl_t* repl ) {
    vm_t* vm = &repl->vm;
    vm->userdata  = repl;
    vm->report    = repl_vm_report;
    vm->print     = repl->print;
    vm->inkey     = repl->inkey;
    vm->input     = repl->input;
    vm->getkey    = repl->getkey;
    vm->interrupt = repl->interrupt;
    vm->save      = repl->save;
    vm->list      = repl_vm_list;
    vm->deletelines = repl_vm_delete;
    vm->renumber  = repl_vm_renumber;
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
    uint32_t prevlast = cgen->datalast;
    repl_setup_vm( repl );
    if ( setjmp( repl->jmp ) == 0 ) {
        run_compiler( &repl->comp );
        if ( basdbg_on( BASDBG_TREE ) ) {
            basdbg_print_tree( &repl->comp );
        }
        ok = cgen_from_program( cgen, &repl->comp );
        if ( ok && !fromstart && repl->vm.datapos == DATAOFFS_NONE && cgen->datalast != prevlast ) {
            // DATA items were added behind the last one that was read: the next READ gets the first new item
            repl->vm.datapos = prevlast == DATAOFFS_NONE ? cgen->datafirst : cgen_datarec_next( cgen->data, prevlast );
        }
        if ( ok && fromstart ) {
            uint32_t target = 0;
            ok = repl_find_start( repl, label, line, &target );
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
    return ok;
}

// RUN [target]: starts the stored program with all variables cleared
static bool repl_start_program( repl_t* repl, const char* label, int32_t line ) {
    repl_reset_runtime( repl );
    repl_setup_compiler( repl );
    if ( !begin_iterate_program( &repl->comp.iter, &repl->pgm ) ) {
        repl_report( repl, "? Empty program" );
        return false;
    }
    comp_set_tokens( &repl->comp, repl->comp.iter.tok, repl->comp.iter.hdr.length - LINEHDR_SIZE );
    return repl_execute( repl, true, label, line );
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

// LOAD: replaces the stored program; the old one stays if anything goes wrong
static bool repl_load_program( repl_t* repl, const char* filename ) {
    FILE* f = fopen( filename, "rb" );
    if ( f == 0 ) {
        repl_report( repl, errno == ENOENT ? "? File not found" : "? Can't open file" );
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
    if ( !ok ) {
        free_program( &loaded );
        return false;
    }
    free_program( &repl->pgm );
    repl->pgm = loaded;
    return true;
}

// carries out the LOAD / RUN requests the program run left in the VM, until a run ends without one (iteratively:
// a program that ends with RUN must not nest)
static bool repl_finish( repl_t* repl, bool ok ) {
    bool chained = false;       // the run was started by a request, not by the host
    while ( ok && repl->vm.request != VMREQ_NONE ) {
        int kind = repl->vm.request;
        char* name = strdup( repl->vm.reqname ? repl->vm.reqname : "" );
        int32_t line = repl->vm.reqline;
        vm_clear_request( &repl->vm );
        if ( name == 0 ) {
            repl_report( repl, "? Out of memory" );
            return false;
        }
        // each run is too short for the VM's own polling when a program restarts itself endlessly (10 RUN)
        if ( chained && repl->interrupt && repl->interrupt( &repl->vm, repl ) ) {
            repl_vm_error( repl, "Break", 0 );
            free( name );
            return false;
        }
        if ( kind == VMREQ_NEW ) {
            repl_new( repl );       // back to the host: nothing runs afterwards
            free( name );
            return true;
        }
        if ( kind == VMREQ_LOAD ) {
            ok = repl_load_program( repl, name );
            name[0] = '\0';        // the loaded program runs from its beginning
            line = -1;
        }
        if ( ok ) {
            ok = repl_start_program( repl, name, line );
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
    return repl_finish( repl, repl_execute( repl, false, 0, -1 ) );
}

bool repl_run( repl_t* repl ) {
    return repl_finish( repl, repl_start_program( repl, "", -1 ) );
}

void repl_new( repl_t* repl ) {
    free_program( &repl->pgm );
    init_program( &repl->pgm );
    repl_reset_runtime( repl );
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
    list_program( &repl->pgm, first, last, repl_list_printer );
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

bool repl_save_program( repl_t* repl, const char* filename, bool ascii ) {
    FILE* f = fopen( filename, ascii ? "w" : "wb" );
    if ( f == 0 ) {
        return false;
    }
    bool ok = save_program( &repl->pgm, f, ascii );
    if ( fclose( f ) != 0 ) {
        ok = false;
    }
    return ok;
}

// -- input lines ---------------------------------------------------------------------

bool repl_enter( repl_t* repl, const char* text ) {
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
            if ( !ok ) {
                repl_report( repl, "? Out of memory" );
            }
        }
    }
    free( copy );
    return ok;
}
