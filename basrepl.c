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

static void repl_say( repl_t* repl, const char* text ) {
    if ( repl->print ) {
        repl->print( &repl->vm, repl, 0, text );
    } else {
        fputs( text, stdout );
    }
}

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
}

// -- running -----------------------------------------------------------------------

// compiles what the compiler has been set up with, and runs the result; reentry point of error jumps.
// fromstart: run from the beginning (RUN: DATA pointer reset) instead of from the new code (direct mode).
static bool repl_execute( repl_t* repl, bool fromstart ) {
    bool ok = false;
    codegen_t* cgen = &repl->rt.codegen;
    uint32_t start = fromstart ? UINT32_C(0) : cgen->codesize;
    uint16_t prevlast = cgen->datalast;
    repl_setup_vm( repl );
    if ( setjmp( repl->jmp ) == 0 ) {
        run_compiler( &repl->comp );
        if ( basdbg_on( BASDBG_TREE ) ) {
            basdbg_print_tree( &repl->comp );
        }
        ok = cgen_from_program( cgen, &repl->comp );
        if ( ok && !fromstart && repl->vm.datapos == DATAOFFS_NONE && cgen->datalast != prevlast ) {
            // DATA items were added behind the last one that was read: the next READ gets the first new item
            repl->vm.datapos = prevlast == DATAOFFS_NONE ? cgen->datafirst :
                (uint16_t)( ( ( (uint16_t) cgen->data[ prevlast + 1U ] ) << UINT8_C(8) ) | cgen->data[ prevlast + 2U ] );
        }
        if ( ok ) {
            ok = fromstart ? vm_run( &repl->vm ) : vm_run_at( &repl->vm, start );
        }
    }
    return ok;
}

static bool repl_run_direct( repl_t* repl, const uint8_t* tokens, size_t size ) {
    repl_setup_compiler( repl );
    repl->comp.iter.hdr.lineno = LINENO_NONE;
    comp_set_tokens( &repl->comp, tokens, size );
    return repl_execute( repl, false );
}

bool repl_run( repl_t* repl ) {
    repl_reset_runtime( repl );
    repl_setup_compiler( repl );
    if ( !begin_iterate_program( &repl->comp.iter, &repl->pgm ) ) {
        repl_report( repl, "? Empty program" );
        return false;
    }
    comp_set_tokens( &repl->comp, repl->comp.iter.tok, repl->comp.iter.hdr.length - LINEHDR_SIZE );
    return repl_execute( repl, true );
}

void repl_new( repl_t* repl ) {
    free_program( &repl->pgm );
    init_program( &repl->pgm );
    repl_reset_runtime( repl );
}

// LIST output goes through the host print callback (list_program's printer has no user data parameter)
static repl_t* listing_repl = 0;

static void repl_list_printer( const char* text ) {
    repl_say( listing_repl, text );
    repl_say( listing_repl, "\n" );
}

void repl_list( repl_t* repl, uint32_t first, uint32_t last ) {
    listing_repl = repl;
    list_program( &repl->pgm, first, last, repl_list_printer );
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

// RUN, NEW, LIST [first[-last]] (the text is preprocessed: upper case, single blanks); false if it is none of them
static bool repl_command( repl_t* repl, const char* text ) {
    while ( *text == ' ' ) {
        ++text;
    }
    if ( strcmp( text, "RUN" ) == 0 || strcmp( text, "RUN " ) == 0 ) {
        repl_run( repl );
        return true;
    }
    if ( strcmp( text, "NEW" ) == 0 || strcmp( text, "NEW " ) == 0 ) {
        repl_new( repl );
        return true;
    }
    if ( strncmp( text, "LIST", 4U ) == 0 && ( text[4] == '\0' || text[4] == ' ' ) ) {
        const char* p = text + 4;
        uint32_t first = LINENO_NONE, last = LINENO_NONE;
        while ( *p == ' ' ) {
            ++p;
        }
        if ( *p >= '0' && *p <= '9' ) {
            first = last = (uint32_t) strtoul( p, (char**) &p, 10 );
            if ( *p == '-' ) {
                ++p;
                last = LINENO_NONE;
                if ( *p >= '0' && *p <= '9' ) {
                    last = (uint32_t) strtoul( p, (char**) &p, 10 );
                }
            }
        } else if ( *p == '-' && p[1] >= '0' && p[1] <= '9' ) {
            last = (uint32_t) strtoul( p + 1, (char**) &p, 10 );
        }
        while ( *p == ' ' ) {
            ++p;
        }
        if ( *p != '\0' ) {
            repl_report( repl, "? Syntax error" );
        } else {
            repl_list( repl, first, last );
        }
        return true;
    }
    return false;
}

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
    if ( repl_command( repl, copy ) ) {
        // done
    } else if ( !tokenize_line( copy, repl->tokbuf, &hdr ) ) {
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
