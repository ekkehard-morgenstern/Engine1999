
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

#include "vm.h"
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
    vm->pc       = UINT32_C(0);
    vm->line     = LINENO_NONE;
    vm->maxsteps = UINT64_C(0);
    vm->channel  = 0;
    vm->column   = 0;
    vm->userdata = 0;
    vm->print    = 0;
    vm->report   = 0;
}

static void vm_clear_stacks( vm_t* vm ) {
    while ( vm->dsp ) {
        vmval_t* v = &vm->dstack[ --vm->dsp ];
        if ( v->isstr ) {
            free( v->str );
        }
    }
    vm->rsp = 0;
}

void cleanup_vm( vm_t* vm ) {
    vm_clear_stacks( vm );
    free( vm->dstack );
    free( vm->rstack );
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

static bool vm_rpush( vm_t* vm, uint16_t addr ) {
    if ( vm->rsp == vm->rmax ) {
        size_t newmax = vm->rmax ? vm->rmax * 2U : 64U;
        uint16_t* p = (uint16_t*) realloc( vm->rstack, newmax * sizeof(uint16_t) );
        if ( p == 0 ) {
            return vm_error( vm, "Out of memory" );
        }
        vm->rstack = p;
        vm->rmax = newmax;
    }
    vm->rstack[ vm->rsp++ ] = addr;
    return true;
}

static bool vm_rpop( vm_t* vm, uint16_t* paddr ) {
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

// -- variables -------------------------------------------------------------

static bool vm_rpush( vm_t* vm, uint16_t addr );

static bool vm_read_var( vm_t* vm, uint16_t varoffs, uint8_t vartype ) {
    varvalue_t value;
    if ( !vmem_get_var( vm->vmem, varoffs, vartype, 0, 0, &value ) ) {
        return vm_error( vm, "Undefined variable" );
    }
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_LABEL:
            // a label is a code address; it goes on the return stack
            if ( value.lbloffs == CODEOFFS_NONE ) {
                return vm_error( vm, "Undefined label" );
            }
            return vm_rpush( vm, value.lbloffs );
        case VARTYPEV_FLOAT:
            return vm_dpush_num( vm, value.dblval );
        case VARTYPEV_INT:
            return vm_dpush_num( vm, (double) value.intval );
        case VARTYPEV_STR: {
            if ( value.strsize == 0 || value.stroffs == STROFFS_NONE ) {
                char* s = vm_new_string( vm, "", 0 );
                return s && vm_push_string( vm, s );
            }
            if ( (uint32_t) value.stroffs + value.strsize > vm->vmem->strssize ) {
                return vm_error( vm, "Bad string variable" );
            }
            char* s = vm_new_string( vm, (const char*) &vm->vmem->strs[ value.stroffs ], value.strsize );
            return s && vm_push_string( vm, s );
        }
        default:
            break;
    }
    return vm_error( vm, "Unsupported variable type" );
}

static bool vm_write_var( vm_t* vm, uint16_t varoffs, uint8_t vartype ) {
    varvalue_t value;
    memset( &value, 0, sizeof(value) );
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT: {
            if ( !vm_dpop_num( vm, &value.dblval ) ) {
                return false;
            }
            break;
        }
        case VARTYPEV_INT: {
            double num;
            if ( !vm_dpop_num( vm, &num ) ) {
                return false;
            }
            if ( !( num > -32769.0 && num < 32768.0 ) ) {
                return vm_error( vm, "Integer out of range" );
            }
            value.intval = (int16_t) num;
            break;
        }
        case VARTYPEV_STR: {
            char* str;
            if ( !vm_dpop_str( vm, &str ) ) {
                return false;
            }
            size_t len = strlen( str );
            uint16_t offs = STROFFS_NONE;
            if ( len > UINT16_MAX || ( len && !vmem_alloc_strs( vm->vmem, (uint16_t) len, &offs ) ) ) {
                free( str );
                return vm_error( vm, len > UINT16_MAX ? "String too long" : "Out of string space" );
            }
            if ( len ) {
                memcpy( &vm->vmem->strs[ offs ], str, len );
            }
            free( str );
            value.stroffs = len ? offs : STROFFS_NONE;
            value.strsize = (uint16_t) len;
            break;
        }
        default:
            return vm_error( vm, "Unsupported variable type" );
    }
    if ( !vmem_set_var( vm->vmem, varoffs, vartype, 0, 0, &value ) ) {
        return vm_error( vm, "Undefined variable" );
    }
    return true;
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

static bool vm_branch( vm_t* vm, uint32_t target ) {
    if ( target > vm->cgen->codesize ) {
        return vm_error( vm, "Branch out of range" );
    }
    vm->pc = target;
    return true;
}

static bool vm_step( vm_t* vm ) {
    const codegen_t* cgen = vm->cgen;
    uint32_t pc = vm->pc;
    // fetch and decode
    uint8_t ins = cgen->code[ pc++ ];
    uint8_t ext = 0; uint16_t param = 0;
    uint32_t need = ( ( ins & INSF_E ) ? 1U : 0U ) + ( ( ins & INSF_P ) ? 2U : 0U );
    if ( need > cgen->codesize - pc ) {
        return vm_error( vm, "Truncated instruction" );
    }
    if ( ins & INSF_E ) {
        ext = cgen->code[ pc++ ];
    }
    if ( ins & INSF_P ) {
        param = (uint16_t)( ( ( (uint16_t) cgen->code[ pc ] ) << UINT8_C(8) ) | cgen->code[ pc + 1U ] );
        pc += 2U;
    }
    vm->pc = pc;    // address of the following instruction
    bool hasp = ( ins & INSF_P ) != 0, code = ( ins & INSF_C ) != 0;
    uint16_t opcode = ins & INSM_I;
    if ( ins & INSF_E ) {
        opcode = (uint16_t)( ( opcode << UINT8_C(8) ) | ext );
    }
    // 17 bit immediate: the C bit is the uppermost bit
    uint32_t imm17 = ( (uint32_t) param ) | ( code ? UINT32_C(0x10000) : UINT32_C(0) );
    int32_t  simm17 = imm17 & UINT32_C(0x10000) ? (int32_t)( imm17 | UINT32_C(0xFFFE0000) ) : (int32_t) imm17;
    double num; const uint8_t* mem; uint32_t avail; uint16_t addr; char* str;

    switch ( opcode ) {
        case INS_BRK:
            return vm_error( vm, "Breakpoint" );
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
            return vm_branch( vm, imm17 );
        case INS_BRIR: {
            int64_t target = (int64_t) pc + simm17;
            if ( target < 0 || target > (int64_t) cgen->codesize ) {
                return vm_error( vm, "Branch out of range" );
            }
            return vm_branch( vm, (uint32_t) target );
        }
        case INS_JPCC:
            if ( !vm_dpop_num( vm, &num ) || !vm_rpop( vm, &addr ) ) {
                return false;
            }
            return vm_to_int32( num ) != 0 ? vm_branch( vm, addr ) : true;
        case INS_JUMP:
            return vm_rpop( vm, &addr ) && vm_branch( vm, addr );
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
        case INS_RRNV: return vm_read_var( vm, param, VARTYPEV_FLOAT );
        case INS_RRIV: return vm_read_var( vm, param, VARTYPEV_INT );
        case INS_RRSV: return vm_read_var( vm, param, VARTYPEV_STR );
        case INS_RRLV: return vm_read_var( vm, param, VARTYPEV_LABEL );
        case INS_WRNV: return vm_write_var( vm, param, VARTYPEV_FLOAT );
        case INS_WRIV: return vm_write_var( vm, param, VARTYPEV_INT );
        case INS_WRSV: return vm_write_var( vm, param, VARTYPEV_STR );
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
            snprintf( buf, sizeof(buf), "%.15g", num );
            vm_out( vm, buf );
            return vm_print_sep( vm, hasp, param );
        }
        case INS_PSTR:
            if ( !vm_dpop_str( vm, &str ) ) {
                return false;
            }
            vm_out( vm, str );
            free( str );
            return vm_print_sep( vm, hasp, param );
        default:
            break;
    }
    char buf[64];
    snprintf( buf, sizeof(buf), "Instruction %03X not implemented", (unsigned) opcode );
    return vm_error( vm, buf );
}

bool vm_run( vm_t* vm ) {
    vm_clear_stacks( vm );
    vm->pc = UINT32_C(0);
    vm->line = LINENO_NONE;
    vm->column = 0;
    uint64_t steps = UINT64_C(0);
    while ( vm->pc < vm->cgen->codesize ) {
        if ( vm->maxsteps && ++steps > vm->maxsteps ) {
            return vm_error( vm, "Step limit exceeded" );
        }
        if ( !vm_step( vm ) ) {
            return false;
        }
    }
    return true;
}
