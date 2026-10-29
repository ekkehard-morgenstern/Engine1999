
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
#include "vm.h"
#include "mml.h"
#include "instset.h"

// -- helpers ---------------------------------------------------------------

void init_vm( vm_t* vm, codegen_t* cgen, varmem_t* vmem ) {
    vm->cgen     = cgen;
    vm->vmem     = vmem;
    vm->dstack   = 0;
    vm->dsp      = 0;
    vm->dmax     = 0;
    vm->rstack   = 0;
    vm->rsp      = 0;
    vm->rmax     = 0;
    vm->ctxs     = 0;
    vm->nctx     = 0;
    vm->maxctx   = 0;
    vm->pc       = UINT32_C(0);
    vm->datapos  = cgen->datafirst;
    vm->line     = LINENO_NONE;
    vm->maxsteps = UINT64_C(0);
    vm->debug = false;
    vm->suspended = false;
    vm->breakpoints = 0;
    vm->nbreakpoints = 0;
    vm->stopreason = VMSTOP_NONE;
    vm->brkhit = false;
    vm->channel  = 0;
    vm->column   = 0;
    vm->userdata = 0;
    vm->print    = 0;
    vm->report   = 0;
    vm->inkey    = 0;
    vm->sound    = 0;
    memset( vm->sfx, 0, sizeof(vm->sfx) );
    memset( vm->sfx_defined, 0, sizeof(vm->sfx_defined) );
    vm->input    = 0;
    vm->interrupt = 0;
    vm->getkey   = 0;
    vm->inbuf    = 0;
    vm->inlen    = vm->inpos = 0;
    vm->inredo   = 0;
    vm->save     = 0;
    vm->list     = 0;
    vm->deletelines = 0;
    vm->renumber = 0;
    vm->request  = VMREQ_NONE;
    vm->reqname  = 0;
    vm->reqline  = -1;
    clock_gettime( CLOCK_MONOTONIC, &vm->started );
}

static void vm_clear_stacks( vm_t* vm ) {
    while ( vm->dsp ) {
        vmval_t* v = &vm->dstack[ --vm->dsp ];
        if ( v->isstr ) {
            free( v->str );
        }
    }
    vm->rsp = 0;
    free( vm->inbuf );
    vm->inbuf = 0;
    // function contexts still open (after an error): free the saved strings
    while ( vm->nctx ) {
        vm_context_t* ctx = &vm->ctxs[ --vm->nctx ];
        for ( uint8_t i=0; i < ctx->numsaved; ++i ) {
            if ( ctx->saved[i].value.isstr ) {
                free( ctx->saved[i].value.str );
            }
        }
    }
}

void vm_clear_request( vm_t* vm ) {
    vm->request = VMREQ_NONE;
    free( vm->reqname );
    vm->reqname = 0;
    vm->reqline = -1;
}

void cleanup_vm( vm_t* vm ) {
    for ( int i=0; i < SND_MAXSFX; ++i ) {
        mml_free( &vm->sfx[i] );
        vm->sfx_defined[i] = false;
    }
    vm_clear_request( vm );
    vm_clear_stacks( vm );
    free( vm->dstack );
    free( vm->rstack );
    free( vm->ctxs );
    vm->ctxs = 0; vm->nctx = vm->maxctx = 0;
    vm->dstack = 0; vm->dsp = vm->dmax = 0;
    vm->rstack = 0; vm->rsp = vm->rmax = 0;
}

static bool vm_error( vm_t* vm, const char* text ) {
    char buf[160];
    if ( vm->line != LINENO_NONE ) {
        snprintf( buf, sizeof(buf), "? %s in line %u\n", text, (unsigned) vm->line );
    } else {
        snprintf( buf, sizeof(buf), "? %s\n", text );
    }
    if ( vm->report ) {
        vm->report( vm, vm->userdata, buf );
    } else {
        fputs( buf, stderr );
    }
    return false;
}

static void vm_out( vm_t* vm, const char* text ) {
    if ( vm->print ) {
        vm->print( vm, vm->userdata, vm->channel, text );
    } else {
        fputs( text, stdout );
    }
    for ( const char* p = text; *p; ++p ) {
        if ( *p == '\n' ) {
            vm->column = 0;
        } else {
            ++vm->column;
        }
    }
}

// -- stacks ----------------------------------------------------------------

static bool vm_dpush( vm_t* vm, vmval_t val ) {
    if ( vm->dsp == vm->dmax ) {
        size_t newmax = vm->dmax ? vm->dmax * 2U : 64U;
        vmval_t* p = (vmval_t*) realloc( vm->dstack, newmax * sizeof(vmval_t) );
        if ( p == 0 ) {
            return vm_error( vm, "Out of memory" );
        }
        vm->dstack = p;
        vm->dmax = newmax;
    }
    vm->dstack[ vm->dsp++ ] = val;
    return true;
}

static bool vm_dpush_num( vm_t* vm, double num ) {
    vmval_t v; v.isstr = false; v.num = num;
    return vm_dpush( vm, v );
}

static bool vm_dpop( vm_t* vm, vmval_t* pval ) {
    if ( vm->dsp == 0 ) {
        return vm_error( vm, "Data stack underflow" );
    }
    *pval = vm->dstack[ --vm->dsp ];
    return true;
}

static bool vm_dpop_num( vm_t* vm, double* pnum ) {
    vmval_t v;
    if ( !vm_dpop( vm, &v ) ) {
        return false;
    }
    if ( v.isstr ) {
        free( v.str );
        return vm_error( vm, "Type mismatch (number expected)" );
    }
    *pnum = v.num;
    return true;
}

// pops a string; the caller owns the result
static bool vm_dpop_str( vm_t* vm, char** pstr ) {
    vmval_t v;
    if ( !vm_dpop( vm, &v ) ) {
        return false;
    }
    if ( !v.isstr ) {
        return vm_error( vm, "Type mismatch (string expected)" );
    }
    *pstr = v.str;
    return true;
}

static bool vm_rpush( vm_t* vm, uint32_t addr ) {
    if ( vm->rsp == vm->rmax ) {
        size_t newmax = vm->rmax ? vm->rmax * 2U : 64U;
        uint32_t* p = (uint32_t*) realloc( vm->rstack, newmax * sizeof(uint32_t) );
        if ( p == 0 ) {
            return vm_error( vm, "Out of memory" );
        }
        vm->rstack = p;
        vm->rmax = newmax;
    }
    vm->rstack[ vm->rsp++ ] = addr;
    return true;
}

static bool vm_rpop( vm_t* vm, uint32_t* paddr ) {
    if ( vm->rsp == 0 ) {
        return vm_error( vm, "Return stack underflow" );
    }
    *paddr = vm->rstack[ --vm->rsp ];
    return true;
}

// -- values ----------------------------------------------------------------

static int32_t vm_to_int32( double num ) {
    if ( num != num ) {
        return 0;
    }
    if ( num >= 2147483647.0 ) {
        return INT32_MAX;
    }
    if ( num <= -2147483648.0 ) {
        return INT32_MIN;
    }
    return (int32_t) num;
}

static char* vm_new_string( vm_t* vm, const char* text, size_t len ) {
    if ( len > UINT16_MAX ) {
        vm_error( vm, "String too long" );
        return 0;
    }
    char* s = (char*) malloc( len + 1U );
    if ( s == 0 ) {
        vm_error( vm, "Out of memory" );
        return 0;
    }
    if ( len ) {
        memcpy( s, text, len );
    }
    s[ len ] = '\0';
    return s;
}

static bool vm_push_string( vm_t* vm, char* s ) {
    // takes ownership of s
    vmval_t v; v.isstr = true; v.str = s;
    if ( !vm_dpush( vm, v ) ) {
        free( s );
        return false;
    }
    return true;
}

static double vm_extract_float( const uint8_t* mem ) {
    uint64_t val = 0;
    for ( unsigned i=0; i < 8U; ++i ) {
        val = ( val << UINT8_C(8) ) | mem[i];
    }
    union { uint64_t ui64; double dbl; } u;
    u.ui64 = val;
    return u.dbl;
}

// returns the segment (code or data) and its size
static const uint8_t* vm_segment( const vm_t* vm, bool code, uint32_t* psize ) {
    if ( code ) {
        *psize = vm->cgen->codesize;
        return vm->cgen->code;
    }
    *psize = vm->cgen->datasize;
    return vm->cgen->data;
}

// pops an address from the data stack and checks that <size> bytes are available there
static bool vm_pop_address( vm_t* vm, bool code, uint32_t size, const uint8_t** pmem, uint32_t* pavail ) {
    double addr;
    if ( !vm_dpop_num( vm, &addr ) ) {
        return false;
    }
    uint32_t segsize = 0;
    const uint8_t* seg = vm_segment( vm, code, &segsize );
    if ( !( addr >= 0.0 && addr < (double) segsize ) || (uint32_t) addr + size > segsize ) {
        return vm_error( vm, "Address out of range" );
    }
    *pmem = &seg[ (uint32_t) addr ];
    *pavail = segsize - (uint32_t) addr;
    return true;
}

// -- numbers and printing --------------------------------------------------

static void vm_format_number( double num, char buf[40] ) {
    snprintf( buf, 40U, "%.15g", num );
}

static bool vm_print_sep( vm_t* vm, bool hassep, uint16_t sep ) {
    if ( !hassep || sep == VM_PRINT_SEP_NONE ) {
        vm_out( vm, "\n" );
    } else if ( sep == VM_PRINT_SEP_COMMA ) {
        char pad[ VM_ZONE_WIDTH + 1U ];
        unsigned n = VM_ZONE_WIDTH - ( vm->column % VM_ZONE_WIDTH );
        memset( pad, ' ', n );
        pad[n] = '\0';
        vm_out( vm, pad );
    } else if ( sep != VM_PRINT_SEP_SEMIC ) {
        return vm_error( vm, "Bad print separator" );
    }
    return true;
}

static bool vm_branch_to( vm_t* vm, uint32_t target );

// -- variables -------------------------------------------------------------

// reads a regular variable or an array element into a value (a string is copied to the heap)
static bool vm_get_value( vm_t* vm, uint16_t varoffs, uint8_t vartype, uint8_t numdims, const uint16_t* diminx, vmval_t* pval ) {
    varvalue_t value;
    if ( !vmem_get_var( vm->vmem, varoffs, vartype, numdims, diminx, &value ) ) {
        return vm_error( vm, numdims ? "Subscript out of range" : "Undefined variable" );
    }
    pval->isstr = false;
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:
            pval->num = value.dblval;
            return true;
        case VARTYPEV_INT:
            pval->num = (double) value.intval;
            return true;
        case VARTYPEV_STR:
            if ( value.strlen > UINT16_MAX ) {
                return vm_error( vm, "String too long" );
            }
            pval->isstr = true;
            pval->str = vm_new_string( vm, value.strptr ? value.strptr : "", value.strptr ? value.strlen : 0U );
            return pval->str != 0;
        default:
            break;
    }
    return vm_error( vm, "Unsupported variable type" );
}

// writes a value to a regular variable or an array element; takes ownership of a string value
static bool vm_set_value( vm_t* vm, uint16_t varoffs, uint8_t vartype, uint8_t numdims, const uint16_t* diminx, vmval_t val ) {
    varvalue_t value;
    memset( &value, 0, sizeof(value) );
    bool wantstr = ( vartype & VARTYPEM_BASE ) == VARTYPEV_STR;
    if ( val.isstr != wantstr ) {
        if ( val.isstr ) {
            free( val.str );
        }
        return vm_error( vm, wantstr ? "Type mismatch (string expected)" : "Type mismatch (number expected)" );
    }
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:
            value.dblval = val.num;
            break;
        case VARTYPEV_INT:
            if ( !( val.num > -32769.0 && val.num < 32768.0 ) ) {
                return vm_error( vm, "Integer out of range" );
            }
            value.intval = (int16_t) val.num;
            break;
        case VARTYPEV_STR: {
            size_t len = strlen( val.str );
            if ( len > UINT16_MAX ) {
                free( val.str );
                return vm_error( vm, "String too long" );
            }
            value.strptr = val.str;     // the variable makes its own copy
            value.strlen = len;
            if ( !vmem_set_var( vm->vmem, varoffs, vartype, numdims, diminx, &value ) ) {
                free( val.str );
                return vm_error( vm, numdims ? "Subscript out of range" : "Undefined variable" );
            }
            free( val.str );
            return true;
        }
        default:
            return vm_error( vm, "Unsupported variable type" );
    }
    if ( !vmem_set_var( vm->vmem, varoffs, vartype, numdims, diminx, &value ) ) {
        return vm_error( vm, numdims ? "Subscript out of range" : "Undefined variable" );
    }
    return true;
}

static bool vm_rpush( vm_t* vm, uint32_t addr );

static bool vm_read_var( vm_t* vm, uint16_t varoffs, uint8_t vartype ) {
    if ( ( vartype & VARTYPEM_BASE ) == VARTYPEV_LABEL ) {
        // a label is a code address; it goes on the return stack
        varvalue_t value;
        if ( !vmem_get_var( vm->vmem, varoffs, vartype, 0, 0, &value ) ) {
            return vm_error( vm, "Undefined variable" );
        }
        if ( value.lbloffs == CODEOFFS_NONE ) {
            return vm_error( vm, "Undefined label" );
        }
        return vm_rpush( vm, value.lbloffs );
    }
    vmval_t val;
    if ( !vm_get_value( vm, varoffs, vartype, 0, 0, &val ) ) {
        return false;
    }
    return val.isstr ? vm_push_string( vm, val.str ) : vm_dpush_num( vm, val.num );
}

static bool vm_write_var( vm_t* vm, uint16_t varoffs, uint8_t vartype ) {
    vmval_t val;
    return vm_dpop( vm, &val ) && vm_set_value( vm, varoffs, vartype, 0, 0, val );
}

// pops the subscripts of an array element access: ( inx ... ndim -- ), ndim is 0 for a regular variable
static bool vm_pop_subscripts( vm_t* vm, uint8_t* pnumdims, uint16_t diminx[MAXDIM] ) {
    double num;
    if ( !vm_dpop_num( vm, &num ) ) {
        return false;
    }
    if ( !( num >= 0.0 && num <= (double) MAXDIM ) ) {
        return vm_error( vm, "Bad number of subscripts" );
    }
    uint8_t numdims = (uint8_t) num;
    for ( uint8_t i = numdims; i > 0; --i ) {
        if ( !vm_dpop_num( vm, &num ) ) {
            return false;
        }
        if ( !( num > -1.0 && num < 65536.0 ) ) {
            return vm_error( vm, "Subscript out of range" );
        }
        diminx[ i - 1U ] = (uint16_t) num;
    }
    *pnumdims = numdims;
    return true;
}

static bool vm_read_elem( vm_t* vm, uint16_t varoffs, uint8_t basetype ) {
    uint8_t numdims = 0; uint16_t diminx[MAXDIM]; vmval_t val;
    if ( !vm_pop_subscripts( vm, &numdims, diminx ) ||
         !vm_get_value( vm, varoffs, VARTYPEF_ARRAY | basetype, numdims, diminx, &val ) ) {
        return false;
    }
    return val.isstr ? vm_push_string( vm, val.str ) : vm_dpush_num( vm, val.num );
}

static bool vm_write_elem( vm_t* vm, uint16_t varoffs, uint8_t basetype ) {
    // ( n inx ... ndim -- )
    uint8_t numdims = 0; uint16_t diminx[MAXDIM]; vmval_t val;
    if ( !vm_pop_subscripts( vm, &numdims, diminx ) || !vm_dpop( vm, &val ) ) {
        return false;
    }
    return vm_set_value( vm, varoffs, VARTYPEF_ARRAY | basetype, numdims, diminx, val );
}

// SLFT, SRGT, SMID: ( s n inx ... ndim -- ) or ( s start len inx ... ndim -- ), see instset.h
static bool vm_set_substring( vm_t* vm, uint16_t opcode, uint16_t varoffs ) {
    uint8_t numdims = 0; uint16_t diminx[MAXDIM];
    double a = 0.0, b = 0.0; char* s = 0; vmval_t target;
    if ( !vm_pop_subscripts( vm, &numdims, diminx ) ) {
        return false;
    }
    if ( ( opcode == INS_SMID && !vm_dpop_num( vm, &b ) ) || !vm_dpop_num( vm, &a ) || !vm_dpop_str( vm, &s ) ) {
        return false;
    }
    uint8_t vartype = VARTYPEV_STR | ( numdims ? VARTYPEF_ARRAY : UINT8_C(0) );
    if ( !vm_get_value( vm, varoffs, vartype, numdims, diminx, &target ) ) {
        free( s );
        return false;
    }
    size_t tlen = strlen( target.str ), slen = strlen( s ), pos = 0, count = 0;
    bool ok = true;
    if ( opcode == INS_SMID ) {
        if ( !( a >= 1.0 ) || a > (double) tlen ) {
            ok = vm_error( vm, "Illegal argument" );
        } else {
            pos = (size_t) a - 1U;
            size_t avail = tlen - pos;
            count = b >= 0.0 && b < (double) avail ? (size_t) b : avail;
        }
    } else if ( a < 0.0 ) {
        ok = vm_error( vm, "Illegal argument" );
    } else {
        count = a < (double) tlen ? (size_t) a : tlen;
        if ( opcode == INS_SRGT ) {
            pos = 0;    // set below, when the number of characters is known
        }
    }
    if ( ok ) {
        if ( count > slen ) {
            count = slen;
        }
        if ( opcode == INS_SRGT ) {
            pos = tlen - ( count < tlen ? count : tlen );
        }
        memcpy( target.str + pos, s, count );
    }
    free( s );
    if ( !ok ) {
        free( target.str );
        return false;
    }
    return vm_set_value( vm, varoffs, vartype, numdims, diminx, target );
}

// -- DATA ----------------------------------------------------------------------

// RDNM, RDST: ( -- n ) or ( -- s ): reads the next DATA item, see instset.h
static bool vm_read_data( vm_t* vm, bool wantstr ) {
    const codegen_t* cgen = vm->cgen;
    uint32_t pos = vm->datapos;
    if ( pos == DATAOFFS_NONE || pos > cgen->datasize || DATAREC_HEADER > cgen->datasize - pos ) {
        return vm_error( vm, "Out of DATA" );
    }
    uint8_t type = cgen->data[ pos ];
    uint32_t next = cgen_datarec_next( cgen->data, pos );
    const uint8_t* payload = &cgen->data[ pos + DATAREC_HEADER ];
    if ( ( type != 0 && type != 1 ) || ( wantstr ? type != 1 : type != 0 ) ) {
        return vm_error( vm, wantstr ? "Type mismatch (DATA item is not a string)" : "Type mismatch (DATA item is not a number)" );
    }
    if ( !wantstr ) {
        if ( pos + DATAREC_HEADER + 8U > cgen->datasize ) {
            return vm_error( vm, "Bad DATA item" );
        }
        if ( !vm_dpush_num( vm, vm_extract_float( payload ) ) ) {
            return false;
        }
    } else {
        const uint8_t* end = memchr( payload, '\0', cgen->datasize - ( pos + DATAREC_HEADER ) );
        if ( end == 0 ) {
            return vm_error( vm, "Bad DATA item" );
        }
        char* s = vm_new_string( vm, (const char*) payload, (size_t)( end - payload ) );
        if ( s == 0 || !vm_push_string( vm, s ) ) {
            return false;
        }
    }
    vm->datapos = next;
    return true;
}

// -- INPUT, GET ----------------------------------------------------------------

static bool vm_interrupted( vm_t* vm ) {
    return vm->interrupt && vm->interrupt( vm, vm->userdata );
}

// the line from the host (malloc'd, no newline), 0 = end of input
static char* vm_get_line( vm_t* vm ) {
    if ( vm->input ) {
        return vm->input( vm, vm->userdata, vm->channel );
    }
    char* line = 0; size_t cap = 0;
    ssize_t n = getline( &line, &cap, stdin );
    if ( n < 0 ) {
        free( line );
        return 0;
    }
    while ( n > 0 && ( line[ n - 1 ] == '\n' || line[ n - 1 ] == '\r' ) ) {
        line[ --n ] = '\0';
    }
    return line;
}

// INPL: prints the prompt, reads the line
static bool vm_input_line( vm_t* vm, uint32_t promptoffs, uint32_t inspc ) {
    const codegen_t* cgen = vm->cgen;
    if ( promptoffs >= cgen->datasize || memchr( &cgen->data[ promptoffs ], '\0', cgen->datasize - promptoffs ) == 0 ) {
        return vm_error( vm, "Bad prompt" );
    }
    vm_out( vm, (const char*) &cgen->data[ promptoffs ] );
    free( vm->inbuf );
    vm->inbuf = vm_get_line( vm );
    if ( vm->inbuf == 0 ) {
        return vm_error( vm, vm_interrupted( vm ) ? "Break" : "End of input" );
    }
    vm->inlen  = strlen( vm->inbuf );
    vm->inpos  = 0;
    vm->inredo = inspc;
    vm->column = 0;
    return true;
}

// is the whole text a decimal number: [+-] digits [. digits] [E [+-] digits], or empty?
static bool vm_is_number( const char* s ) {
    if ( *s == '+' || *s == '-' ) {
        ++s;
    }
    unsigned digits = 0;
    while ( isdigit( (unsigned char) *s ) ) { ++s; ++digits; }
    if ( *s == '.' ) {
        ++s;
        while ( isdigit( (unsigned char) *s ) ) { ++s; ++digits; }
    }
    if ( digits == 0 ) {
        return false;
    }
    if ( *s == 'E' || *s == 'e' ) {
        ++s;
        if ( *s == '+' || *s == '-' ) {
            ++s;
        }
        if ( !isdigit( (unsigned char) *s ) ) {
            return false;
        }
        while ( isdigit( (unsigned char) *s ) ) { ++s; }
    }
    return *s == '\0';
}

// INPN, INPS: ( -- n ) or ( -- s ): takes the next field of the input line, see instset.h
static bool vm_input_field( vm_t* vm, bool wantstr ) {
    if ( vm->inbuf == 0 ) {
        return vm_error( vm, "INPUT field without INPUT" );
    }
    bool redo = vm->inpos > vm->inlen;      // no more fields
    char* field = 0;
    if ( !redo ) {
        const char* p = vm->inbuf + vm->inpos;
        const char* end;
        while ( *p == ' ' || *p == '\t' ) {
            ++p;
        }
        const char* text = p;
        size_t tlen;
        if ( *p == '"' ) {
            text = ++p;
            while ( *p && *p != '"' ) {
                ++p;
            }
            tlen = (size_t)( p - text );
            if ( *p == '"' ) {
                ++p;
            }
            while ( *p && *p != ',' ) {     // ignore what follows the closing quote
                ++p;
            }
            end = p;
        } else {
            while ( *p && *p != ',' ) {
                ++p;
            }
            end = p;
            while ( end > text && ( end[-1] == ' ' || end[-1] == '\t' ) ) {
                --end;
            }
            tlen = (size_t)( end - text );
            end = p;
        }
        field = vm_new_string( vm, text, tlen );
        if ( field == 0 ) {
            return false;
        }
        vm->inpos = *end == ',' ? (size_t)( end - vm->inbuf ) + 1U : vm->inlen + 1U;
    }
    if ( !redo && !wantstr ) {
        char* q = field;
        while ( *q == ' ' || *q == '\t' ) {
            ++q;
        }
        size_t n = strlen( q );
        while ( n > 0 && ( q[ n - 1 ] == ' ' || q[ n - 1 ] == '\t' ) ) {
            q[ --n ] = '\0';
        }
        if ( n == 0 ) {
            free( field );
            return vm_dpush_num( vm, 0.0 );
        }
        redo = !vm_is_number( q );
        if ( !redo ) {
            double val = strtod( q, 0 );
            free( field );
            return vm_dpush_num( vm, val );
        }
    }
    if ( redo ) {
        free( field );
        vm_out( vm, "?Redo from start\n" );
        vm->pc = vm->inredo;
        return true;
    }
    return vm_push_string( vm, field );
}

// INPE: reports leftover fields, releases the line
static bool vm_input_end( vm_t* vm ) {
    if ( vm->inbuf && vm->inpos <= vm->inlen ) {
        vm_out( vm, "?Extra ignored\n" );
    }
    free( vm->inbuf );
    vm->inbuf = 0;
    return true;
}

// GETK: ( -- s ): waits for a key
static bool vm_get_key( vm_t* vm ) {
    int key;
    if ( vm->getkey ) {
        key = vm->getkey( vm, vm->userdata, vm->channel );
    } else if ( vm->inkey ) {
        struct timespec delay = { 0, 1000000L };
        while ( ( key = vm->inkey( vm, vm->userdata ) ) == 0 ) {
            nanosleep( &delay, 0 );
        }
    } else {
        key = getchar();
    }
    if ( key <= 0 || key > 255 ) {
        return vm_error( vm, vm_interrupted( vm ) ? "Break" : "End of input" );
    }
    char buf[2] = { (char) key, '\0' };
    char* s = vm_new_string( vm, buf, 1U );
    return s && vm_push_string( vm, s );
}

// -- user function calls -----------------------------------------------------

// pushes a context for a call: the values of the parameter variables before the call are saved,
// to be restored by RETF. The return stack gets <retoffs> <context index>.
static bool vm_call_function( vm_t* vm, uint16_t funcoffs, uint32_t retaddr ) {
    double num;
    if ( !vm_dpop_num( vm, &num ) ) {
        return false;
    }
    uint8_t functype = 0, numparams = 0; uint32_t codeoffs = CODEOFFS_NONE;
    if ( !vmem_get_func_info( vm->vmem, funcoffs, &functype, &numparams, &codeoffs ) ) {
        return vm_error( vm, "Undefined function" );
    }
    if ( codeoffs == CODEOFFS_NONE ) {
        return vm_error( vm, "Undefined function" );
    }
    if ( num != (double) numparams ) {
        return vm_error( vm, "Argument count mismatch" );
    }
    // the arguments, last one on top
    vmval_t args[MAXPARAM];
    for ( uint8_t i = numparams; i > 0; --i ) {
        if ( !vm_dpop( vm, &args[ i - 1U ] ) ) {
            for ( uint8_t j = i; j < numparams; ++j ) {
                if ( args[j].isstr ) {
                    free( args[j].str );
                }
            }
            return false;
        }
    }
    // make room for the context
    if ( vm->nctx == vm->maxctx ) {
        size_t newmax = vm->maxctx ? vm->maxctx * 2U : 16U;
        if ( newmax > UINT16_MAX ) {
            newmax = UINT16_MAX;
        }
        vm_context_t* p = vm->nctx < newmax ? (vm_context_t*) realloc( vm->ctxs, newmax * sizeof(vm_context_t) ) : 0;
        if ( p == 0 ) {
            goto FAIL;
        }
        vm->ctxs = p;
        vm->maxctx = newmax;
    }
    vm_context_t* ctx = &vm->ctxs[ vm->nctx ];
    ctx->numsaved = 0;
    for ( uint8_t i=0; i < numparams; ++i ) {
        uint8_t ptype = 0; const char* name = 0; uint16_t poffs = VAROFFS_NONE;
        if ( !vmem_get_func_param( vm->vmem, funcoffs, i, &ptype, &name ) ) {
            vm_error( vm, "Bad function parameter" );
            goto FAILCTX;
        }
        if ( ptype & VARTYPEF_ARRAY ) {
            vm_error( vm, "Array parameters are not supported yet" );
            goto FAILCTX;
        }
        if ( !vmem_lookup_var( vm->vmem, ptype, name, &poffs ) || poffs == VAROFFS_NONE ) {
            vm_error( vm, "Undefined parameter variable" );
            goto FAILCTX;
        }
        vm_savedvar_t* sv = &ctx->saved[ ctx->numsaved ];
        sv->varoffs = poffs;
        sv->vartype = ptype;
        if ( !vm_get_value( vm, poffs, ptype, 0, 0, &sv->value ) ) {
            goto FAILCTX;
        }
        ++ctx->numsaved;
        // the argument is consumed by the assignment
        vmval_t arg = args[i];
        args[i].isstr = false;
        args[i].num = 0.0;
        if ( !vm_set_value( vm, poffs, ptype, 0, 0, arg ) ) {
            goto FAILCTX;
        }
    }
    if ( !vm_rpush( vm, retaddr ) || !vm_rpush( vm, (uint32_t) vm->nctx ) ) {
        goto FAILCTX;
    }
    ++vm->nctx;
    return vm_branch_to( vm, codeoffs );

FAILCTX:
    for ( uint8_t i=0; i < ctx->numsaved; ++i ) {
        if ( ctx->saved[i].value.isstr ) {
            free( ctx->saved[i].value.str );
        }
    }
FAIL:
    for ( uint8_t i=0; i < numparams; ++i ) {
        if ( args[i].isstr ) {
            free( args[i].str );
        }
    }
    return false;
}

static bool vm_return_function( vm_t* vm ) {
    uint32_t ctxindex = 0, retaddr = 0;
    if ( !vm_rpop( vm, &ctxindex ) || !vm_rpop( vm, &retaddr ) ) {
        return false;
    }
    if ( vm->nctx == 0 || ctxindex != vm->nctx - 1U ) {
        return vm_error( vm, "RETF without function call" );
    }
    vm_context_t* ctx = &vm->ctxs[ --vm->nctx ];
    // the result stays on the data stack; restore the parameter variables in reverse order
    bool ok = true;
    for ( uint8_t i = ctx->numsaved; i > 0; --i ) {
        vm_savedvar_t* sv = &ctx->saved[ i - 1U ];
        ok = vm_set_value( vm, sv->varoffs, sv->vartype, 0, 0, sv->value ) && ok;
    }
    ctx->numsaved = 0;
    return ok && vm_branch_to( vm, retaddr );
}

// -- system functions --------------------------------------------------------

// ASC, LEN, VAL: ( s -- n )
static bool vm_str_to_num( vm_t* vm, uint16_t opcode ) {
    char* s;
    if ( !vm_dpop_str( vm, &s ) ) {
        return false;
    }
    double r = 0.0;
    bool ok = true;
    switch ( opcode ) {
        case INS_LEN:
            r = (double) strlen( s );
            break;
        case INS_ASC:
            if ( s[0] == '\0' ) {
                ok = vm_error( vm, "Illegal argument" );
            } else {
                r = (double)(uint8_t) s[0];
            }
            break;
        default: {  // INS_VAL: the leading decimal number of the string, 0 if there is none
            const char* p = s;
            while ( *p == ' ' ) {
                ++p;
            }
            const char* start = p;
            if ( *p == '+' || *p == '-' ) {
                ++p;
            }
            unsigned digits = 0;
            while ( *p >= '0' && *p <= '9' ) {
                ++p; ++digits;
            }
            if ( *p == '.' ) {
                ++p;
                while ( *p >= '0' && *p <= '9' ) {
                    ++p; ++digits;
                }
            }
            if ( digits && ( *p == 'e' || *p == 'E' ) ) {
                const char* q = p + 1;
                if ( *q == '+' || *q == '-' ) {
                    ++q;
                }
                if ( *q >= '0' && *q <= '9' ) {
                    while ( *q >= '0' && *q <= '9' ) {
                        ++q;
                    }
                    p = q;
                }
            }
            if ( digits ) {
                char* tmp = vm_new_string( vm, start, (size_t)( p - start ) );
                if ( tmp == 0 ) {
                    free( s );
                    return false;
                }
                r = strtod( tmp, 0 );
                free( tmp );
            }
            break;
        }
    }
    free( s );
    return ok && vm_dpush_num( vm, r );
}

// BINS, QUAS, OCTS, HEXS, DECS: ( n digits -- s ), the number is converted as a 32 bit integer and padded with zeros
// to at least <digits> digits (0 = no padding); DECS is signed, the others show negative numbers as 32 bit patterns
static bool vm_radix_string( vm_t* vm, unsigned base, bool issigned ) {
    double num, digits;
    if ( !vm_dpop_num( vm, &digits ) || !vm_dpop_num( vm, &num ) ) {
        return false;
    }
    if ( digits > 255.0 ) {
        return vm_error( vm, "Illegal argument" );
    }
    unsigned pad = digits > 0.0 ? (unsigned) digits : 0U;     // negative: not given
    int32_t ival = vm_to_int32( num );
    bool neg = issigned && ival < 0;
    uint32_t u = neg ? (uint32_t)( -(int64_t) ival ) : (uint32_t) ival;
    char rev[40]; unsigned len = 0;
    do {
        rev[ len++ ] = "0123456789ABCDEF"[ u % base ];
        u /= base;
    } while ( u );
    char out[ 1U + 255U + 40U ]; unsigned n = 0;
    if ( neg ) {
        out[ n++ ] = '-';
    }
    for ( unsigned i = len; i < pad; ++i ) {
        out[ n++ ] = '0';
    }
    while ( len ) {
        out[ n++ ] = rev[ --len ];
    }
    char* s = vm_new_string( vm, out, n );
    return s && vm_push_string( vm, s );
}

// LFTS, RGTS: ( s n -- s ), MIDS: ( s start len -- s ), start counts from 1
static bool vm_substring( vm_t* vm, uint16_t opcode ) {
    double n, m = 0.0; char* s;
    if ( opcode == INS_MIDS && !vm_dpop_num( vm, &m ) ) {
        return false;
    }
    if ( !vm_dpop_num( vm, &n ) || !vm_dpop_str( vm, &s ) ) {
        return false;
    }
    size_t len = strlen( s ), from = 0, count = 0;
    bool ok = true;
    if ( n < 0.0 || ( opcode == INS_MIDS && n < 1.0 ) ) {
        ok = vm_error( vm, "Illegal argument" );
    } else if ( opcode == INS_LFTS ) {
        count = n < (double) len ? (size_t) n : len;
    } else if ( opcode == INS_RGTS ) {
        count = n < (double) len ? (size_t) n : len;
        from = len - count;
    } else {
        from = n - 1.0 < (double) len ? (size_t)( n - 1.0 ) : len;
        count = m >= 0.0 && m < (double)( len - from ) ? (size_t) m : len - from;     // negative: rest of the string
    }
    char* r = ok ? vm_new_string( vm, s + from, count ) : 0;
    free( s );
    return r && vm_push_string( vm, r );
}

// SHEX: ( s -- s ) runs the shell command and returns what it wrote to its standard output, without the
// trailing newlines (like a shell command substitution); the exit status is ignored
static bool vm_shell_execute( vm_t* vm ) {
    char* cmd;
    if ( !vm_dpop_str( vm, &cmd ) ) {
        return false;
    }
    fflush( stdout );
    FILE* pipe = popen( cmd, "r" );
    free( cmd );
    if ( pipe == 0 ) {
        return vm_error( vm, "Failed to run shell command" );
    }
    size_t size = 0, alloc = 256;
    char* text = (char*) malloc( alloc );
    bool toolong = false;
    while ( text ) {
        if ( size + 128U > alloc ) {
            alloc *= 2U;
            char* p = (char*) realloc( text, alloc );
            if ( p == 0 ) {
                free( text );
                text = 0;
                break;
            }
            text = p;
        }
        size_t rd = fread( text + size, 1U, 128U, pipe );
        size += rd;
        if ( size > UINT16_MAX + 1024U ) {      // keep reading, but not storing, so the command can finish
            toolong = true;
            size = UINT16_MAX + 1U;
        }
        if ( rd < 128U ) {
            break;
        }
    }
    pclose( pipe );
    if ( text == 0 ) {
        return vm_error( vm, "Out of memory" );
    }
    while ( size && text[ size - 1U ] == '\n' ) {
        --size;
    }
    char* s = toolong || size > UINT16_MAX ? 0 : vm_new_string( vm, text, strnlen( text, size ) );
    bool tl = toolong || size > UINT16_MAX;
    free( text );
    if ( tl ) {
        return vm_error( vm, "String too long" );
    }
    return s && vm_push_string( vm, s );
}

// SAVE: ( mode name -- ) the program belongs to the host, so it does the saving through the save callback
static bool vm_save_program( vm_t* vm ) {
    char* name; char* mode;
    if ( !vm_dpop_str( vm, &name ) ) {
        return false;
    }
    if ( !vm_dpop_str( vm, &mode ) ) {
        free( name );
        return false;
    }
    bool ok = true;
    bool ascii = false;
    if ( strcmp( mode, "A" ) == 0 ) {
        ascii = true;
    } else if ( mode[0] != '\0' && strcmp( mode, "B" ) != 0 ) {
        ok = vm_error( vm, "Bad SAVE mode" );
    }
    if ( ok && name[0] == '\0' ) {
        ok = vm_error( vm, "Illegal argument" );
    }
    if ( ok && vm->save == 0 ) {
        ok = vm_error( vm, "SAVE is not available" );
    }
    if ( ok && !vm->save( vm, vm->userdata, name, ascii ) ) {
        ok = vm_error( vm, "Save failed" );
    }
    free( name );
    free( mode );
    return ok;
}

// LOAD ( s -- ): leaves a request for the host and ends the run
static bool vm_load_program( vm_t* vm ) {
    char* name;
    if ( !vm_dpop_str( vm, &name ) ) {
        return false;
    }
    if ( name[0] == '\0' ) {
        free( name );
        return vm_error( vm, "Illegal argument" );
    }
    vm_clear_request( vm );
    vm->request = VMREQ_LOAD;
    vm->reqname = name;
    vm->pc = vm->cgen->codesize;
    return true;
}

// RUNP ( s n -- ): label name ("" = none) and line number (-1 = none)
static bool vm_run_program( vm_t* vm ) {
    double num; char* name;
    if ( !vm_dpop_num( vm, &num ) ) {
        return false;
    }
    if ( !vm_dpop_str( vm, &name ) ) {
        return false;
    }
    if ( num < -1.0 || num > (double) LINENO_MAX ) {
        free( name );
        return vm_error( vm, "Illegal argument" );
    }
    vm_clear_request( vm );
    vm->request = VMREQ_RUN;
    vm->reqname = name;
    vm->reqline = (int32_t) num;
    vm->pc = vm->cgen->codesize;
    return true;
}

// EDIT ( s n -- ): label name ("" = none) and line number (-1 = none)
static bool vm_edit_request( vm_t* vm ) {
    double num; char* name;
    if ( !vm_dpop_num( vm, &num ) ) {
        return false;
    }
    if ( !vm_dpop_str( vm, &name ) ) {
        return false;
    }
    if ( num < -1.0 || num > (double) LINENO_MAX ) {
        free( name );
        return vm_error( vm, "Illegal argument" );
    }
    vm_clear_request( vm );
    vm->request = VMREQ_EDIT;
    vm->reqname = name;
    vm->reqline = (int32_t) num;
    vm->pc = vm->cgen->codesize;
    return true;
}

// NEW ( -- ): leaves a request for the host and ends the run
static bool vm_new_program( vm_t* vm ) {
    vm_clear_request( vm );
    vm->request = VMREQ_NEW;
    vm->pc = vm->cgen->codesize;
    return true;
}

// MON ( -- ): leaves a request for the host and ends the run
static bool vm_monitor_request( vm_t* vm ) {
    vm_clear_request( vm );
    vm->request = VMREQ_MONITOR;
    vm->pc = vm->cgen->codesize;
    return true;
}

// the channel number 1..8 -> 0..7; false after reporting an error
static bool vm_sound_channel( vm_t* vm, double num, int* pchan ) {
    if ( !( num >= 1.0 && num < 9.0 ) ) {
        return vm_error( vm, "Illegal argument" );
    }
    *pchan = (int) num - 1;
    return true;
}

// the events of a music string on a channel (the caller has synced the channels); the voice ends with a stop
static void vm_queue_voice( vm_t* vm, int chan, const mml_t* m, double tobeats ) {
    const vm_sound_t* snd = vm->sound;
    void* ud = vm->userdata;
    for ( size_t k=0; k < m->count; ++k ) {
        const mml_event_t* ev = &m->events[k];
        switch ( ev->kind ) {
            case MML_TONE:
                snd->tone( ud, chan, ev->freq, ev->freq2, ev->vol, (float)( ev->seconds * tobeats ) );
                if ( ev->gap > 0.0f ) {
                    snd->tone( ud, chan, 0.0f, 0.0f, 0.0f, (float)( ev->gap * tobeats ) );
                }
                break;
            case MML_REST:
                snd->tone( ud, chan, 0.0f, 0.0f, 0.0f, (float)( ev->seconds * tobeats ) );
                break;
            case MML_WAVE:
                snd->setwave( ud, chan, ev->wave );
                break;
            case MML_ENV:
                snd->setenv( ud, chan, ev->p[0], ev->p[1], ev->p[2], ev->p[3] );
                break;
            case MML_VIB:
                snd->setvib( ud, chan, ev->p[0], ev->p[1] );
                break;
            case MML_TREM:
                snd->settrem( ud, chan, ev->p[0], ev->p[1] );
                break;
        }
    }
    snd->tone( ud, chan, 0.0f, 0.0f, 0.0f, 0.0f );      // the end of the voice: nothing goes on sounding
}

// the host's beats per second of music-string time
static double vm_sound_tobeats( vm_t* vm ) {
    int bpm = vm->sound->getbpm( vm->userdata );
    return (double)( bpm > 0 ? bpm : 60 ) / 60.0;
}

// PLAY ( s1 .. sn n -- ): the voices are parsed first, then queued
static bool vm_play( vm_t* vm ) {
    double count = 0; char* strs[SND_MAXVOICES]; mml_t mml[SND_MAXVOICES]; int n = 0, parsed = 0;
    bool ok = vm_dpop_num( vm, &count );
    if ( ok && !( count >= 1.0 && count <= (double) SND_MAXVOICES ) ) {
        ok = vm_error( vm, "Illegal argument" );
    }
    if ( ok ) {
        n = (int) count;
        int popped = 0;
        for ( int i=n-1; ok && i >= 0; --i ) {      // the last string is on top
            ok = vm_dpop_str( vm, &strs[i] );
            if ( ok ) ++popped;
        }
        if ( !ok ) {        // the strings that were popped belong to the voices n-popped .. n-1
            for ( int i=n-popped; i < n; ++i ) free( strs[i] );
            n = 0;
        }
    }
    for ( int i=0; ok && i < n; ++i ) {
        char err[MML_ERRLEN];
        if ( mml_parse( strs[i], &mml[i], err ) ) {
            ++parsed;
        } else {
            ok = vm_error( vm, err );
        }
    }
    if ( ok && vm->sound != 0 ) {
        double tobeats = vm_sound_tobeats( vm );
        vm->sound->sync( vm->userdata, ( 1U << n ) - 1U );
        for ( int v=0; v < n; ++v ) {
            vm_queue_voice( vm, v, &mml[v], tobeats );
        }
    }
    for ( int i=0; i < parsed; ++i ) mml_free( &mml[i] );
    for ( int i=0; i < n; ++i ) free( strs[i] );
    return ok;
}

// SFX n, a$ ( n s -- ): parses the string and stores it as effect n
static bool vm_sfx_define( vm_t* vm ) {
    double n = 0; char* str = 0;
    if ( !vm_dpop_str( vm, &str ) ) {
        return false;
    }
    if ( !vm_dpop_num( vm, &n ) ) {
        free( str );
        return false;
    }
    bool ok = true; mml_t m; char err[MML_ERRLEN];
    if ( !( n >= 1.0 && n < (double)( SND_MAXSFX + 1 ) ) ) {
        ok = vm_error( vm, "Illegal argument" );
    } else if ( !mml_parse( str, &m, err ) ) {
        ok = vm_error( vm, err );
    } else {
        mml_free( &vm->sfx[ (int) n - 1 ] );
        vm->sfx[ (int) n - 1 ] = m;
        vm->sfx_defined[ (int) n - 1 ] = true;
    }
    free( str );
    return ok;
}

// SFX n [, ch] ( n ch -- ): plays the effect on the channel; 0 = a free one (the highest that isn't busy)
static bool vm_sfx_play( vm_t* vm ) {
    double n = 0, ch = 0;
    if ( !vm_dpop_num( vm, &ch ) || !vm_dpop_num( vm, &n ) ) {
        return false;
    }
    if ( !( n >= 1.0 && n < (double)( SND_MAXSFX + 1 ) ) || !( ch >= 0.0 && ch < 9.0 ) ) {
        return vm_error( vm, "Illegal argument" );
    }
    const mml_t* m = &vm->sfx[ (int) n - 1 ];
    if ( !vm->sfx_defined[ (int) n - 1 ] ) {
        return vm_error( vm, "Undefined effect" );
    }
    if ( vm->sound == 0 ) {
        return true;
    }
    const vm_sound_t* snd = vm->sound;
    int chan = (int) ch - 1;
    if ( chan < 0 ) {
        chan = SND_MAXVOICES - 1;
        for ( int c=SND_MAXVOICES-1; c >= 0; --c ) {
            if ( !snd->busy( vm->userdata, c ) ) {
                chan = c;
                break;
            }
        }
    }
    double tobeats = vm_sound_tobeats( vm );
    snd->sync( vm->userdata, 1U << chan );
    snd->setwave( vm->userdata, chan, 0 );                  // an effect starts from a clean channel
    snd->setenv( vm->userdata, chan, -1.0f, 0.0f, 0.0f, 0.0f );
    snd->setvib( vm->userdata, chan, 0.0f, 0.0f );
    snd->settrem( vm->userdata, chan, 0.0f, 0.0f );
    vm_queue_voice( vm, chan, m, tobeats );
    return true;
}

// WAIT ( n -- ): sleeps n/60 s; asks the interrupt callback every 10 ms
static bool vm_wait( vm_t* vm ) {
    double ticks = 0;
    if ( !vm_dpop_num( vm, &ticks ) ) {
        return false;
    }
    if ( !( ticks >= 0.0 && ticks <= 216000.0 ) ) {
        return vm_error( vm, "Illegal argument" );
    }
    struct timespec now, end;
    clock_gettime( CLOCK_MONOTONIC, &now );
    double secs = ticks / 60.0;
    end.tv_sec = now.tv_sec + (time_t) secs;
    end.tv_nsec = now.tv_nsec + (long)( ( secs - floor( secs ) ) * 1.0e9 );
    if ( end.tv_nsec >= 1000000000L ) {
        end.tv_nsec -= 1000000000L;
        ++end.tv_sec;
    }
    for (;;) {
        clock_gettime( CLOCK_MONOTONIC, &now );
        long long left = (long long)( end.tv_sec - now.tv_sec ) * 1000000000LL + ( end.tv_nsec - now.tv_nsec );
        if ( left <= 0 ) {
            return true;
        }
        if ( vm_interrupted( vm ) ) {
            return vm_error( vm, "Break" );
        }
        struct timespec nap;
        long long slice = left < 10000000LL ? left : 10000000LL;
        nap.tv_sec = 0; nap.tv_nsec = (long) slice;
        nanosleep( &nap, 0 );
    }
}

// SND ( args -- ), kind = SNDK_*
static bool vm_sound( vm_t* vm, uint16_t kind ) {
    double a = 0, b = 0, c = 0, d = 0, e = 0; int chan = 0;
    const vm_sound_t* snd = vm->sound;
    switch ( kind ) {
        case SNDK_SOUND:
            if ( !vm_dpop_num( vm, &e ) || !vm_dpop_num( vm, &d ) || !vm_dpop_num( vm, &c ) || !vm_dpop_num( vm, &b ) ||
                 !vm_dpop_num( vm, &a ) || !vm_sound_channel( vm, a, &chan ) ) {
                return false;
            }
            if ( !( ( b == 0.0 || ( b >= 20.0 && b <= 20000.0 ) ) && c >= 0.0 && c <= 1.0e6 && d >= 0.0 && d <= 1.0 &&
                    ( e == 0.0 || ( e >= 20.0 && e <= 20000.0 && c > 0.0 ) ) ) ) {
                return vm_error( vm, "Illegal argument" );
            }
            if ( snd ) snd->tone( vm->userdata, chan, (float) b, b == 0.0 ? 0.0f : (float) e, b == 0.0 ? 0.0f : (float) d, (float) c );
            return true;
        case SNDK_PLAY:
            return vm_play( vm );
        case SNDK_SFXDEF:
            return vm_sfx_define( vm );
        case SNDK_SFXPLAY:
            return vm_sfx_play( vm );
        case SNDK_WAVE:
            if ( !vm_dpop_num( vm, &b ) || !vm_dpop_num( vm, &a ) || !vm_sound_channel( vm, a, &chan ) ) {
                return false;
            }
            if ( !( b >= 0.0 && b < 4.0 ) ) {
                return vm_error( vm, "Illegal argument" );
            }
            if ( snd ) snd->setwave( vm->userdata, chan, (int) b );
            return true;
        case SNDK_PITCH:
        case SNDK_VOLUME:
            if ( !vm_dpop_num( vm, &b ) || !vm_dpop_num( vm, &a ) || !vm_sound_channel( vm, a, &chan ) ) {
                return false;
            }
            if ( kind == SNDK_PITCH ? !( b >= 20.0 && b <= 20000.0 ) : !( b >= 0.0 && b <= 1.0 ) ) {
                return vm_error( vm, "Illegal argument" );
            }
            if ( snd ) {
                if ( kind == SNDK_PITCH ) snd->setpitch( vm->userdata, chan, (float) b );
                else snd->setvolume( vm->userdata, chan, (float) b );
            }
            return true;
        case SNDK_ENVELOPE:
            if ( !vm_dpop_num( vm, &e ) || !vm_dpop_num( vm, &d ) || !vm_dpop_num( vm, &c ) || !vm_dpop_num( vm, &b ) ||
                 !vm_dpop_num( vm, &a ) ) {
                return false;
            }
            if ( !vm_sound_channel( vm, a, &chan ) ) {      // popped: release, sustain, decay, attack, channel
                return false;
            }
            if ( b >= 0.0 && !( b <= 60.0 && c >= 0.0 && c <= 60.0 && d >= 0.0 && d <= 1.0 && e >= 0.0 && e <= 60.0 ) ) {
                return vm_error( vm, "Illegal argument" );
            }
            if ( snd ) snd->setenv( vm->userdata, chan, b < 0.0 ? -1.0f : (float) b, (float) c, (float) d, (float) e );
            return true;
        case SNDK_VIBRATO:
        case SNDK_TREMOLO:
            if ( !vm_dpop_num( vm, &c ) || !vm_dpop_num( vm, &b ) || !vm_dpop_num( vm, &a ) || !vm_sound_channel( vm, a, &chan ) ) {
                return false;
            }
            if ( !( b >= 0.0 && b <= 100.0 && c >= 0.0 && c <= ( kind == SNDK_VIBRATO ? 24.0 : 1.0 ) ) ) {
                return vm_error( vm, "Illegal argument" );
            }
            if ( snd ) {
                if ( kind == SNDK_VIBRATO ) snd->setvib( vm->userdata, chan, (float) b, (float) c );
                else snd->settrem( vm->userdata, chan, (float) b, (float) c );
            }
            return true;
        case SNDK_TEMPO:
            if ( !vm_dpop_num( vm, &a ) ) {
                return false;
            }
            if ( !( a >= 1.0 && a <= 1000.0 ) ) {
                return vm_error( vm, "Illegal argument" );
            }
            if ( snd ) snd->setbpm( vm->userdata, (int) a );
            return true;
        case SNDK_OFF:
            if ( !vm_dpop_num( vm, &a ) ) {
                return false;
            }
            if ( a == 0.0 ) {
                chan = -1;
            } else if ( !vm_sound_channel( vm, a, &chan ) ) {
                return false;
            }
            if ( snd ) snd->silence( vm->userdata, chan );
            return true;
        case SNDK_SYNC: {
            if ( !vm_dpop_num( vm, &a ) ) {
                return false;
            }
            if ( !( a >= 1.0 && a <= (double) SND_MAXVOICES ) ) {
                return vm_error( vm, "Illegal argument" );
            }
            int n = (int) a; unsigned mask = 0; bool ok = true;
            for ( int i=0; i < n; ++i ) {       // all are popped, even after an error
                double ch = 0; int cn = 0;
                if ( !vm_dpop_num( vm, &ch ) ) {
                    return false;
                }
                if ( ok && !vm_sound_channel( vm, ch, &cn ) ) {
                    ok = false;
                } else if ( ok ) {
                    mask |= 1U << cn;
                }
            }
            if ( ok && snd ) snd->sync( vm->userdata, mask );
            return ok;
        }
        default:
            break;
    }
    return vm_error( vm, "Internal error (bad sound statement)" );
}

// LIST ( s1 n1 s2 n2 -- )
static bool vm_list_program( vm_t* vm ) {
    double n1 = 0, n2 = 0; char* s1 = 0; char* s2 = 0;
    bool ok = vm_dpop_num( vm, &n2 ) && vm_dpop_str( vm, &s2 ) && vm_dpop_num( vm, &n1 ) && vm_dpop_str( vm, &s1 );
    if ( ok && ( n1 < -1.0 || n1 > (double) LINENO_MAX || n2 < -1.0 || n2 > (double) LINENO_MAX ) ) {
        ok = vm_error( vm, "Illegal argument" );
    }
    if ( ok && vm->list == 0 ) {
        ok = vm_error( vm, "LIST is not available" );
    }
    if ( ok ) {
        ok = vm->list( vm, vm->userdata, s1, (int32_t) n1, s2, (int32_t) n2 );
        vm->column = 0;
    }
    free( s1 );
    free( s2 );
    return ok;
}

// DELE ( s1 n1 s2 n2 -- ) and RENM ( s1 n1 s2 n2 step -- )
static bool vm_edit_program( vm_t* vm, bool renumber ) {
    double step = 10.0, n1 = 0, n2 = 0; char* s1 = 0; char* s2 = 0;
    bool ok = ( !renumber || vm_dpop_num( vm, &step ) ) &&
        vm_dpop_num( vm, &n2 ) && vm_dpop_str( vm, &s2 ) && vm_dpop_num( vm, &n1 ) && vm_dpop_str( vm, &s1 );
    if ( ok && ( n1 < -1.0 || n1 > (double) LINENO_MAX || n2 < -1.0 || n2 > (double) LINENO_MAX ||
                 step < 1.0 || step > (double) LINENO_MAX ) ) {
        ok = vm_error( vm, "Illegal argument" );
    }
    if ( ok && ( renumber ? vm->renumber == 0 : vm->deletelines == 0 ) ) {
        ok = vm_error( vm, renumber ? "RENUM is not available" : "DELETE is not available" );
    }
    if ( ok ) {
        ok = renumber ? vm->renumber( vm, vm->userdata, s1, (int32_t) n1, s2, (int32_t) n2, (int32_t) step ) :
            vm->deletelines( vm, vm->userdata, s1, (int32_t) n1, s2, (int32_t) n2 );
        vm->pc = vm->cgen->codesize;        // the program changed: the run ends
    }
    free( s1 );
    free( s2 );
    return ok;
}

// FRE: the memory that is available for allocation, in bytes (Linux: MemAvailable from /proc/meminfo, else 0)
static double vm_free_memory( void ) {
    FILE* f = fopen( "/proc/meminfo", "r" );
    if ( f == 0 ) {
        return 0.0;
    }
    char line[128]; double result = 0.0;
    while ( fgets( line, sizeof(line), f ) ) {
        unsigned long long kb = 0;
        if ( sscanf( line, "MemAvailable: %llu kB", &kb ) == 1 ) {
            result = (double) kb * 1024.0;
            break;
        }
    }
    fclose( f );
    return result;
}

static bool vm_system_function( vm_t* vm, uint16_t opcode ) {
    double num; char buf[40];
    switch ( opcode ) {
        case INS_TI: {
            // the system timer counts 1/60 second ticks since the VM was created
            struct timespec now;
            clock_gettime( CLOCK_MONOTONIC, &now );
            double secs = (double)( now.tv_sec - vm->started.tv_sec ) + (double)( now.tv_nsec - vm->started.tv_nsec ) / 1e9;
            return vm_dpush_num( vm, floor( secs * 60.0 ) );
        }
        case INS_INKY: {
            int key = vm->inkey ? vm->inkey( vm, vm->userdata ) : 0;
            buf[0] = (char) key;
            buf[1] = '\0';
            char* s = vm_new_string( vm, buf, key > 0 && key < 256 ? 1U : 0U );
            return s && vm_push_string( vm, s );
        }
        case INS_FRE:
            // the argument is ignored; the result is the available physical memory
            if ( !vm_dpop_num( vm, &num ) ) {
                return false;
            }
            return vm_dpush_num( vm, vm_free_memory() );
        case INS_STRS: {
            if ( !vm_dpop_num( vm, &num ) ) {
                return false;
            }
            vm_format_number( num, buf );
            char* s = vm_new_string( vm, buf, strlen( buf ) );
            return s && vm_push_string( vm, s );
        }
        case INS_PLNG: {
            int chan = 0;
            if ( !vm_dpop_num( vm, &num ) || !vm_sound_channel( vm, num, &chan ) ) {
                return false;
            }
            return vm_dpush_num( vm, vm->sound != 0 && vm->sound->busy( vm->userdata, chan ) ? -1.0 : 0.0 );
        }
        case INS_CHRS: {
            if ( !vm_dpop_num( vm, &num ) ) {
                return false;
            }
            int32_t code = vm_to_int32( num );
            if ( code < 1 || code > 255 ) {
                return vm_error( vm, "Illegal argument" );
            }
            char c = (char) code;
            char* s = vm_new_string( vm, &c, 1U );
            return s && vm_push_string( vm, s );
        }
        case INS_ASC: case INS_LEN: case INS_VAL:
            return vm_str_to_num( vm, opcode );
        case INS_BINS:  return vm_radix_string( vm, 2U, false );
        case INS_QUAS:  return vm_radix_string( vm, 4U, false );
        case INS_OCTS:  return vm_radix_string( vm, 8U, false );
        case INS_HEXS:  return vm_radix_string( vm, 16U, false );
        case INS_DECS:  return vm_radix_string( vm, 10U, true );
        case INS_LFTS: case INS_RGTS: case INS_MIDS:
            return vm_substring( vm, opcode );
        default:
            break;
    }
    return vm_error( vm, "Internal error (bad system function)" );
}

// -- execution -------------------------------------------------------------

static bool vm_binary_num( vm_t* vm, uint16_t opcode ) {
    double a, b, r = 0.0;
    if ( !vm_dpop_num( vm, &b ) || !vm_dpop_num( vm, &a ) ) {
        return false;
    }
    int32_t ia = vm_to_int32( a ), ib = vm_to_int32( b );
    switch ( opcode ) {
        case INS_LSH:   r = ib >= 0 && ib < 32 ? (double)(int32_t)( (uint32_t) ia << ib ) : 0.0; break;
        case INS_RSH:   r = (double)( ib >= 0 && ib < 32 ? ia >> ib : ( ia < 0 ? -1 : 0 ) ); break;
        case INS_ADD:   r = a + b; break;
        case INS_SUB:   r = a - b; break;
        case INS_MUL:   r = a * b; break;
        case INS_DIV:
            if ( b == 0.0 ) {
                return vm_error( vm, "Division by zero" );
            }
            r = a / b;
            break;
        case INS_AND:   r = (double)( ia & ib ); break;
        case INS_NAND:  r = (double)( ~( ia & ib ) ); break;
        case INS_OR:    r = (double)( ia | ib ); break;
        case INS_NOR:   r = (double)( ~( ia | ib ) ); break;
        case INS_XOR:   r = (double)( ia ^ ib ); break;
        case INS_XNOR:  r = (double)( ~( ia ^ ib ) ); break;
        case INS_POW:   r = pow( a, b ); break;
        case INS_CNEQ:  r = a == b ? -1.0 : 0.0; break;
        case INS_CNNE:  r = a != b ? -1.0 : 0.0; break;
        case INS_CNGE:  r = a >= b ? -1.0 : 0.0; break;
        case INS_CNLE:  r = a <= b ? -1.0 : 0.0; break;
        case INS_CNGT:  r = a >  b ? -1.0 : 0.0; break;
        case INS_CNLT:  r = a <  b ? -1.0 : 0.0; break;
        default:        return vm_error( vm, "Internal error (bad binary operator)" );
    }
    return vm_dpush_num( vm, r );
}

static bool vm_binary_str( vm_t* vm, uint16_t opcode ) {
    char* a; char* b;
    if ( !vm_dpop_str( vm, &b ) ) {
        return false;
    }
    if ( !vm_dpop_str( vm, &a ) ) {
        free( b );
        return false;
    }
    bool ok = true;
    if ( opcode == INS_CON ) {
        size_t la = strlen( a ), lb = strlen( b );
        char* s = 0;
        if ( la + lb > UINT16_MAX ) {
            ok = vm_error( vm, "String too long" );
        } else if ( ( s = (char*) malloc( la + lb + 1U ) ) == 0 ) {
            ok = vm_error( vm, "Out of memory" );
        } else {
            memcpy( s, a, la );
            memcpy( s + la, b, lb + 1U );
        }
        free( a );
        free( b );
        return ok && vm_push_string( vm, s );
    }
    int cmp = strcmp( a, b );
    free( a );
    free( b );
    double r = 0.0;
    switch ( opcode ) {
        case INS_CSEQ:  r = cmp == 0; break;
        case INS_CSNE:  r = cmp != 0; break;
        case INS_CSGE:  r = cmp >= 0; break;
        case INS_CSLE:  r = cmp <= 0; break;
        case INS_CSGT:  r = cmp >  0; break;
        case INS_CSLT:  r = cmp <  0; break;
        default:        return vm_error( vm, "Internal error (bad string operator)" );
    }
    return vm_dpush_num( vm, r != 0.0 ? -1.0 : 0.0 );
}

static bool vm_branch_to( vm_t* vm, uint32_t target ) {
    if ( target > vm->cgen->codesize ) {
        return vm_error( vm, "Branch out of range" );
    }
    vm->pc = target;
    return true;
}

static bool vm_step( vm_t* vm ) {
    const codegen_t* cgen = vm->cgen;
    uint32_t pc = vm->pc;
    const uint32_t inspc = pc;
    // fetch and decode
    uint8_t ins = cgen->code[ pc++ ];
    uint8_t ext = 0; uint32_t param = 0;
    bool wide = ( ins & INSF_P ) != 0 && ( ins & INSF_W ) != 0;
    uint32_t need = ( ( ins & INSF_E ) ? 1U : 0U ) + ( ( ins & INSF_P ) ? ( wide ? 4U : 2U ) : 0U );
    if ( need > cgen->codesize - pc ) {
        return vm_error( vm, "Truncated instruction" );
    }
    if ( ins & INSF_E ) {
        ext = cgen->code[ pc++ ];
    }
    if ( wide ) {
        param = ( ( (uint32_t) cgen->code[ pc ] ) << UINT8_C(24) ) | ( ( (uint32_t) cgen->code[ pc + 1U ] ) << UINT8_C(16) ) |
            ( ( (uint32_t) cgen->code[ pc + 2U ] ) << UINT8_C(8) ) | cgen->code[ pc + 3U ];
        pc += 4U;
    } else if ( ins & INSF_P ) {
        param = (uint32_t)( ( ( (uint16_t) cgen->code[ pc ] ) << UINT8_C(8) ) | cgen->code[ pc + 1U ] );
        pc += 2U;
    }
    vm->pc = pc;    // address of the following instruction
    bool hasp = ( ins & INSF_P ) != 0, code = ( ins & INSF_C ) != 0;
    uint16_t opcode = ins & INSM_I;
    if ( ins & INSF_E ) {
        opcode = (uint16_t)( ( opcode << UINT8_C(8) ) | ext );
    }
    // immediate: 17 bit, the C bit is the uppermost bit; or the whole 32 bit parameter of a wide instruction
    uint32_t imm17 = wide ? param : ( param | ( code ? UINT32_C(0x10000) : UINT32_C(0) ) );
    int32_t  simm17 = wide ? (int32_t) param :
        ( imm17 & UINT32_C(0x10000) ? (int32_t)( imm17 | UINT32_C(0xFFFE0000) ) : (int32_t) imm17 );
    double num; const uint8_t* mem; uint32_t avail; uint32_t addr; char* str;
    // only these take a parameter above 16 bits (the others have variable numbers, counts, ...)
    if ( param > UINT16_MAX && opcode != INS_PHPA && opcode != INS_PHIM && opcode != INS_BRIA && opcode != INS_BRIR &&
         opcode != INS_LINE && opcode != INS_RSTR && opcode != INS_INPL ) {
        return vm_error( vm, "Parameter out of range" );
    }
    const uint16_t param16 = (uint16_t) param;

    switch ( opcode ) {
        case INS_BRK:
            if ( !vm->debug ) {
                return vm_error( vm, "Breakpoint" );
            }
            vm->brkhit = true;
            return true;
        case INS_NOP:
            return true;
        case INS_PHPA:
            if ( !hasp ) {
                return vm_error( vm, "Parameter missing" );
            }
            return code ? vm_rpush( vm, param ) : vm_dpush_num( vm, (double) param );
        case INS_PHIM:
            return vm_dpush_num( vm, (double) simm17 );
        case INS_BRIA:
            return vm_branch_to( vm, imm17 );
        case INS_BRIR: {
            int64_t target = (int64_t) pc + simm17;
            if ( target < 0 || target > (int64_t) cgen->codesize ) {
                return vm_error( vm, "Branch out of range" );
            }
            return vm_branch_to( vm, (uint32_t) target );
        }
        case INS_JPCC:
            if ( !vm_dpop_num( vm, &num ) || !vm_rpop( vm, &addr ) ) {
                return false;
            }
            return vm_to_int32( num ) != 0 ? vm_branch_to( vm, addr ) : true;
        case INS_JUMP:
            return vm_rpop( vm, &addr ) && vm_branch_to( vm, addr );
        case INS_DROP: {
            size_t cnt = hasp ? param : 1U;
            size_t* sp = code ? &vm->rsp : &vm->dsp;
            if ( cnt > *sp ) {
                return vm_error( vm, code ? "Return stack underflow" : "Data stack underflow" );
            }
            if ( !code ) {
                while ( cnt-- ) {   // strings are freed, numbers just dropped
                    vmval_t* item = &vm->dstack[ --vm->dsp ];
                    if ( item->isstr ) {
                        free( item->str );
                    }
                }
                return true;
            }
            *sp -= cnt;
            return true;
        }
        case INS_LINE:
            vm->line = param;
            return true;
        case INS_LAN:
            return vm_pop_address( vm, code, 8U, &mem, &avail ) && vm_dpush_num( vm, vm_extract_float( mem ) );
        case INS_LIAN:
            return vm_pop_address( vm, code, 2U, &mem, &avail ) &&
                vm_dpush_num( vm, (double)(int16_t)( ( ( (uint16_t) mem[0] ) << UINT8_C(8) ) | mem[1] ) );
        case INS_LUAN:
            return vm_pop_address( vm, code, 2U, &mem, &avail ) &&
                vm_dpush_num( vm, (double)(uint16_t)( ( ( (uint16_t) mem[0] ) << UINT8_C(8) ) | mem[1] ) );
        case INS_LAS: {
            if ( !vm_pop_address( vm, code, 1U, &mem, &avail ) ) {
                return false;
            }
            const uint8_t* end = memchr( mem, '\0', avail );
            if ( end == 0 ) {
                return vm_error( vm, "Unterminated string" );
            }
            char* s = vm_new_string( vm, (const char*) mem, (size_t)( end - mem ) );
            return s && vm_push_string( vm, s );
        }
        case INS_FRES:
            if ( !vm_dpop_str( vm, &str ) ) {
                return false;
            }
            free( str );
            return true;
        case INS_NEG:
            return vm_dpop_num( vm, &num ) && vm_dpush_num( vm, -num );
        case INS_NOT:
            return vm_dpop_num( vm, &num ) && vm_dpush_num( vm, (double)( ~vm_to_int32( num ) ) );
        case INS_LSH: case INS_RSH: case INS_ADD: case INS_SUB: case INS_MUL: case INS_DIV: case INS_AND: case INS_NAND:
        case INS_OR: case INS_NOR: case INS_XOR: case INS_XNOR: case INS_POW: case INS_CNEQ: case INS_CNNE:
        case INS_CNGE: case INS_CNLE: case INS_CNGT: case INS_CNLT:
            return vm_binary_num( vm, opcode );
        case INS_CON: case INS_CSEQ: case INS_CSNE: case INS_CSGE: case INS_CSLE: case INS_CSGT: case INS_CSLT:
            return vm_binary_str( vm, opcode );
        case INS_RRNV: return vm_read_var( vm, param16, VARTYPEV_FLOAT );
        case INS_RRIV: return vm_read_var( vm, param16, VARTYPEV_INT );
        case INS_RRSV: return vm_read_var( vm, param16, VARTYPEV_STR );
        case INS_RRLV: return vm_read_var( vm, param16, VARTYPEV_LABEL );
        case INS_RNAE: return vm_read_elem( vm, param16, VARTYPEV_FLOAT );
        case INS_RIAE: return vm_read_elem( vm, param16, VARTYPEV_INT );
        case INS_RSAE: return vm_read_elem( vm, param16, VARTYPEV_STR );
        case INS_WNAE: return vm_write_elem( vm, param16, VARTYPEV_FLOAT );
        case INS_WIAE: return vm_write_elem( vm, param16, VARTYPEV_INT );
        case INS_WSAE: return vm_write_elem( vm, param16, VARTYPEV_STR );
        case INS_TI: case INS_INKY: case INS_FRE: case INS_STRS: case INS_CHRS: case INS_ASC: case INS_LEN: case INS_VAL:
        case INS_PLNG:
        case INS_BINS: case INS_QUAS: case INS_OCTS: case INS_HEXS: case INS_DECS:
        case INS_LFTS: case INS_RGTS: case INS_MIDS:
            return vm_system_function( vm, opcode );
        case INS_SHEX: return vm_shell_execute( vm );
        case INS_SAVE: return vm_save_program( vm );
        case INS_LOAD: return vm_load_program( vm );
        case INS_RUNP: return vm_run_program( vm );
        case INS_LIST: return vm_list_program( vm );
        case INS_NEW: return vm_new_program( vm );
        case INS_MON: return vm_monitor_request( vm );
        case INS_SND: return vm_sound( vm, param16 );
        case INS_WAIT: return vm_wait( vm );
        case INS_EDIT: return vm_edit_request( vm );
        case INS_DELE: return vm_edit_program( vm, false );
        case INS_RENM: return vm_edit_program( vm, true );
        case INS_CLR:
            vmem_clear_vars( vm->vmem );
            return true;
        case INS_SLFT: case INS_SRGT: case INS_SMID:
            return vm_set_substring( vm, opcode, param16 );
        case INS_ERAS:
            return vmem_erase_array( vm->vmem, param16 ) || vm_error( vm, "Not an array" );
        case INS_RDNM: return vm_read_data( vm, false );
        case INS_RDST: return vm_read_data( vm, true );
        case INS_RSTR:
            vm->datapos = hasp ? param : cgen->datafirst;
            return true;
        case INS_CALF: return vm_call_function( vm, param16, pc );
        case INS_RETF: return vm_return_function( vm );
        case INS_WRNV: return vm_write_var( vm, param16, VARTYPEV_FLOAT );
        case INS_WRIV: return vm_write_var( vm, param16, VARTYPEV_INT );
        case INS_WRSV: return vm_write_var( vm, param16, VARTYPEV_STR );
        case INS_INPL: return vm_input_line( vm, param, inspc );
        case INS_INPN: return vm_input_field( vm, false );
        case INS_INPS: return vm_input_field( vm, true );
        case INS_INPE: return vm_input_end( vm );
        case INS_GETK: return vm_get_key( vm );
        case INS_PCHN:
            if ( !vm_dpop_num( vm, &num ) ) {
                return false;
            }
            vm->channel = (int) vm_to_int32( num );
            return true;
        case INS_PNUM: {
            char buf[40];
            if ( !vm_dpop_num( vm, &num ) ) {
                return false;
            }
            vm_format_number( num, buf );
            vm_out( vm, buf );
            return vm_print_sep( vm, hasp, param16 );
        }
        case INS_PSTR:
            if ( !vm_dpop_str( vm, &str ) ) {
                return false;
            }
            vm_out( vm, str );
            free( str );
            return vm_print_sep( vm, hasp, param16 );
        default:
            break;
    }
    char buf[64];
    snprintf( buf, sizeof(buf), "Instruction %03X not implemented", (unsigned) opcode );
    return vm_error( vm, buf );
}

bool vm_run( vm_t* vm ) {
    vm->datapos = vm->cgen->datafirst;      // like RESTORE: a program starts with the first DATA item
    return vm_run_at( vm, UINT32_C(0) );
}

bool vm_begin( vm_t* vm, uint32_t start ) {
    vm_clear_stacks( vm );
    vm_clear_request( vm );
    vm->pc = start;
    vm->line = LINENO_NONE;
    vm->column = 0;
    vm->stopreason = VMSTOP_NONE;
    vm->brkhit = false;
    vm->suspended = true;
    return true;
}

static bool vm_at_breakpoint( const vm_t* vm ) {
    for ( size_t i=0; i < vm->nbreakpoints; ++i ) {
        if ( vm->breakpoints[i] == vm->pc ) {
            return true;
        }
    }
    return false;
}

bool vm_resume( vm_t* vm, bool singlestep ) {
    uint64_t steps = UINT64_C(0);
    vm->stopreason = VMSTOP_NONE;
    vm->brkhit = false;
    vm->suspended = false;
    while ( vm->pc < vm->cgen->codesize ) {
        if ( steps > 0 && vm_at_breakpoint( vm ) ) {
            vm->stopreason = VMSTOP_BREAKPOINT;
            vm->suspended = true;
            return true;
        }
        ++steps;
        if ( vm->maxsteps && steps > vm->maxsteps ) {
            return vm_error( vm, "Step limit exceeded" );
        }
        if ( vm->interrupt && steps % VM_INTERRUPT_STEPS == 0 && vm_interrupted( vm ) ) {
            return vm_error( vm, "Break" );
        }
        if ( !vm_step( vm ) ) {
            return false;
        }
        if ( vm->brkhit ) {
            vm->brkhit = false;
            vm->stopreason = VMSTOP_BRK;
            vm->suspended = true;
            return true;
        }
        if ( singlestep && vm->pc < vm->cgen->codesize ) {
            vm->stopreason = VMSTOP_STEP;
            vm->suspended = true;
            return true;
        }
    }
    return true;
}

bool vm_run_at( vm_t* vm, uint32_t start ) {
    vm_begin( vm, start );
    return vm_resume( vm, false );
}
