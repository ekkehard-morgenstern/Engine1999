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

#define _XOPEN_SOURCE   500     // for strdup(3)

#include "bascomp.h"
#include "stdtypes.h"
#include "unxtypes.h"
#include "sdlutil.h"

/*
TEST for the variable and strings subsystems
*/

typedef enum _tristate_t {
    tri_false,
    tri_true,
    tri_undecided
} tristate_t;

static FILE* fprand = 0;

static int32_t getrand( int32_t modulo ) {
    int32_t val = INT32_C(0);
    if ( fprand == 0 ) {
        fprand = fopen( "/dev/urandom", "rb" );
        if ( fprand == 0 ) {
            perror( "fopen(3) /dev/urandom" );
            exit( EXIT_FAILURE );
        }
    }
    if ( fread( &val, sizeof(int32_t), 1U, fprand ) != 1 ) {
        fprintf( stderr, "/dev/urandom: I/O error\n" );
        exit( EXIT_FAILURE );
    }
    return ( val & INT32_MAX ) % modulo;
}

static void getrandstr( char buf[256], uint8_t* plen ) {
    uint8_t len = (uint8_t) getrand( INT32_C(256) );
    if ( len ) {
        if ( fread( buf, len, 1U, fprand ) != 1 ) {
            fprintf( stderr, "/dev/urandom: I/O error\n" );
            exit( EXIT_FAILURE );
        }
    }
    buf[ len ] = '\0';
    for ( uint8_t i=0; i < len; ++i ) {
        uint8_t b = buf[i];
        if ( b < UINT8_C(32) || b > UINT8_C(126) ) {
            // non-printable character: try again
            b &= UINT8_C(127);
            if ( b < UINT8_C(32) ) {
                b = UINT8_C(32);
            } else if ( b > UINT8_C(126) ) {
                b = UINT8_C(126);
            }
            buf[i] = b;
        }
    }
    if ( plen ) {
        *plen = len;
    }
}

static jmp_buf errorjmp;

static void fatal_error( compiler_t* comp, void* usr ) ATTR_NORETURN;

static void fatal_error( compiler_t* comp, void* usr ) {
    longjmp( errorjmp, 1 );
}

static void report_error( compiler_t* comp, void* user, const char* text ) {
    fprintf( stderr, "? %s\n", text );
}

// book keeping info for verification purposes
typedef union _keepval_t {
    double  dval;
    int16_t ival;
    char*   sval;
} keepval_t;

static void init_keepval( uint8_t vartype, keepval_t* val ) {
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:
            val->dval = 0;
            break;
        case VARTYPEV_INT:
            val->ival = 0;
            break;
        case VARTYPEV_STR:
            val->sval = 0;
            break;
        default:
            val->ival = CODEOFFS_NONE;
            break;
    }
}

/*
static void setrand_keepval( uint8_t vartype, keepval_t* val ) {
    char buf[256];
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:
            val->dval = (double) getrand( INT32_MAX );
            break;
        case VARTYPEV_INT:
            val->ival = (int16_t) getrand( INT16_MAX );
            break;
        case VARTYPEV_STR:
            getrandstr( buf, 0 );
            if ( val->sval ) {
                free( val->sval );
            }
            val->sval = strdup( buf );
            if ( val->sval == 0 ) {
                fprintf( stderr, "? out of memory\n" );
                exit( EXIT_FAILURE );
            }
            break;
        default:
            fprintf( stderr, "? invalid var type\n" );
            exit( EXIT_FAILURE );
    }
}
*/

typedef struct _keepvar_t {
    char*       name;
    uint8_t     vartype;
    keepval_t   value;
    size_t      numcells;
    keepval_t*  cells;
    uint8_t     numdims;
    uint16_t    dims[MAXDIM];
    uint8_t     numparams;
    usrparam_t  params[MAXPARAM];
    uint16_t    varoffs;
    uint32_t    numchecks;
    uint64_t    cumul_nsec_create;
    uint64_t    cumul_nsec_delete;
    uint64_t    cumul_nsec_read;
    uint64_t    cumul_nsec_write;
} keepvar_t;

static void init_keepvar( keepvar_t* var ) {
    memset( var, 0, sizeof(keepvar_t) );
    char buf[256];
    getrandstr( buf, 0 );
    var->name = strdup( buf );
    if ( var->name == 0 ) {
        fprintf( stderr, "? out of memory\n" );
        exit( EXIT_FAILURE );
    }
    var->vartype = ( (uint8_t) getrand( INT32_C(256) ) ) & ~VARTYPEM_INVAL;
    if ( var->vartype & VARTYPEF_ARRAY ) {
        if ( ( var->vartype & VARTYPEM_BASE ) == VARTYPEV_LABEL ) {
            // arrays of labels not allowed
            var->vartype = ( var->vartype & ~VARTYPEM_BASE ) | VARTYPEV_INT;
        }
        // arrays of functions not allowed
        var->vartype &= ~VARTYPEF_FUNC;
        var->numdims = UINT8_C(1) + ( (uint8_t) getrand( MAXDIM - UINT8_C(1) ) );
        static const uint16_t maxmax[MAXDIM] = {
            UINT16_C(24),
            UINT16_C(15),
            UINT16_C(10),
            UINT16_C(5),
            UINT16_C(4),
            UINT16_C(3),
        };
        for ( uint8_t i=UINT8_C(0); i < var->numdims; ++i ) {
            var->dims[i] = UINT16_C(2) + (uint16_t) getrand( maxmax[i] - UINT16_C(2) );
        }
        var->numcells = 0;
        for ( uint8_t i=UINT8_C(0); i < var->numdims; ++i ) {
            if ( i == UINT8_C(0) ) {
                var->numcells += var->dims[i];
            } else {
                var->numcells *= var->dims[i];
            }
        }
        var->cells = (keepval_t*) calloc( var->numcells, sizeof(keepval_t) );
        if ( var->cells == 0 ) {
            fprintf( stderr, "? Out of memory\n" );
            exit( EXIT_FAILURE );
        }
        for ( size_t i=0; i < var->numcells; ++i ) {
            init_keepval( var->vartype & ~VARTYPEF_ARRAY, &var->cells[i] );
        }
    } else if ( var->vartype & VARTYPEF_FUNC ) {
        // function variables hold an integer value (code offset)
        init_keepval( VARTYPEV_LABEL, &var->value );
        // set up arguments
        var->numparams = (uint8_t) getrand( MAXPARAM );
        for ( uint8_t i=UINT8_C(0); i < var->numparams; ++i ) {
            do {
                getrandstr( buf, 0 );
            } while ( buf[0] == '\0' );
            var->params[i].paramname = strdup( buf );
            if ( var->params[i].paramname == 0 ) {
                fprintf( stderr, "? Out of memory\n" );
                exit( EXIT_FAILURE );
            }
            var->params[i].paramtype = (uint8_t) getrand( VARTYPEV_LABEL ); // 0..2
        }
    } else {
        init_keepval( var->vartype, &var->value );
    }
    var->varoffs = VAROFFS_NONE;
}

static bool alloc_keepvar( compiler_t* comp, keepvar_t* var ) {
    if ( var->varoffs != VAROFFS_NONE ) {
        return true;
    }
    uint64_t ti0 = sdlutil_getnsec(0);
    bool ok = comp_create_var( comp, var->vartype, var->name, var->numdims, var->dims, var->numparams, var->params, &var->varoffs );
    uint64_t ti1 = sdlutil_getnsec(0);
    var->cumul_nsec_create += ti1 - ti0;
    if ( !ok || var->varoffs == VAROFFS_NONE ) {
        fprintf( stderr, "comp_create_var() failed\n" );
        return false;
    }
    return true;
}

static bool dealloc_keepvar( compiler_t* comp, keepvar_t* var ) {
    if ( var->varoffs == VAROFFS_NONE ) {
        return true;
    }
    uint64_t ti0 = sdlutil_getnsec(0);
    bool ok = comp_delete_var( comp, var->varoffs );
    var->varoffs = VAROFFS_NONE;
    uint64_t ti1 = sdlutil_getnsec(0);
    var->cumul_nsec_delete += ti1 - ti0;
    if ( !ok ) {
        fprintf( stderr, "comp_delete_var() failed\n" );
        return false;
    }
    return true;
}

static void dump_vartype( uint8_t vartype ) {
    fprintf( stderr, "%02" PRIX8 " ( ", vartype );
    if ( vartype & VARTYPEF_ARRAY ) {
        fprintf( stderr, "array of " );
    } else if ( vartype & VARTYPEF_FUNC ) {
        fprintf( stderr, "function returning " );
    }
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:    fprintf( stderr, "float " ); break;
        case VARTYPEV_INT:      fprintf( stderr, "int " ); break;
        case VARTYPEV_STR:      fprintf( stderr, "string " ); break;
        case VARTYPEV_LABEL:    fprintf( stderr, "label " ); break;
    }
    fprintf( stderr, ")\n" );
}

static void dump_keepvar( const keepvar_t* var ) {
    fprintf( stderr, "%p:\n  name: '%s'\n", (void*) var, var->name );
    fprintf( stderr, "  type: " ); dump_vartype( var->vartype );
    if ( var->vartype & VARTYPEF_ARRAY ) {
        fprintf( stderr, "  dimensions: " );
        for ( uint8_t i=0; i < var->numdims; ++i ) {
            fprintf( stderr, "%s%" PRIu16, i ? ", " : "", var->dims[i] );
        }
        fprintf( stderr, "\n" );
    } else if ( var->vartype & VARTYPEF_FUNC ) {
        fprintf( stderr, "  parameters: " );
        for ( uint8_t i=0 ; i < var->numparams; ++i ) {
            fprintf( stderr, "    name: '%s', type: ", var->params[i].paramname );
            dump_vartype( var->params[i].paramtype );
            fprintf( stderr, "\n" );
        }
    }
}

static tristate_t test_keepvar( compiler_t* comp, keepvar_t* var ) {
    if ( var->varoffs == VAROFFS_NONE ) {
        // no variable space allocated: stop
        return tri_undecided;
    }
    // retrieve variable content (or for arrays, a random cell value)
    uint16_t diminx[MAXDIM]; memset( diminx, 0, sizeof(uint16_t) * MAXDIM );
    for ( uint8_t i=UINT8_C(0); i < var->numdims; ++i ) {
        diminx[i] = (uint16_t) getrand( var->dims[i] );
    }
    varvalue_t value; memset( &value, 0, sizeof(varvalue_t) );
    uint64_t ti0 = sdlutil_getnsec(0);
    bool ok = comp_get_var( comp, var->varoffs, var->vartype, var->numdims, diminx, &value );
    uint64_t ti1 = sdlutil_getnsec(0);
    var->cumul_nsec_read += ti1 - ti0;
    if ( !ok ) {
        fprintf( stderr, "comp_get_var() failed\n" );
        dump_keepvar( var );
        return tri_false;
    }
    // check result against stored (kept) content
    const keepval_t* pval = 0;
    if ( var->vartype & VARTYPEF_ARRAY ) {
        uint16_t offs = UINT16_C(0);
        for ( uint8_t i=0; i < var->numdims; ++i ) {
            // compute slice size (the computed product over all subsequent dimensions)
            uint16_t slice = UINT16_C(0);
            for ( uint8_t j=i+UINT8_C(1); j < var->numdims; ++j ) {
                if ( j == UINT8_C(0) ) {
                    slice += var->dims[j];
                } else {
                    slice *= var->dims[j];
                }
            }
            if ( i == var->numdims - UINT8_C(1) ) {
                // final dimension:
                // add last index
                offs += diminx[i];
            } else {
                // not final dimension:
                // add N x slice to the offset
                offs += diminx[i] * slice;
            }
            if ( offs >= var->numcells ) {
                fprintf( stderr, "? internal error\n" );
                dump_keepvar( var );
                return tri_false;
            }
            pval = &var->cells[offs];
        }
    } else if ( var->vartype & VARTYPEF_FUNC ) {
        pval = 0;
    } else {
        pval = &var->value;
    }
    if ( pval == 0 ) {
        return tri_undecided;    // function variable: not checked
    }
    const char* compstr = 0; char* tmp = 0; tristate_t tri;
    switch ( var->vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:
            if ( value.dblval != pval->dval ) {
                fprintf( stderr, "? Variable error, expected %g but got %g\n", pval->dval, value.dblval );
                dump_keepvar( var );
                return tri_false;
            }
            return true;
        case VARTYPEV_INT:
            if ( value.intval != pval->ival ) {
                fprintf( stderr, "? Variable error, expected %" PRId16 " but got %" PRId16 "\n", pval->ival, value.intval );
                dump_keepvar( var );
                return tri_false;
            }
            return true;
        case VARTYPEV_STR:
            if ( value.stroffs == STROFFS_NONE ) {
                compstr = "";
            } else {
                tmp = (char*) malloc( value.strsize + 1U );
                if ( tmp == 0 ) {
                    fprintf( stderr, "? Out of memory\n" );
                    dump_keepvar( var );
                    return tri_false;
                }
                if ( ( (uint32_t) value.stroffs ) + ( (uint32_t) value.strsize ) >= comp->strssize ) {
                    fprintf( stderr, "? Variable error, offset/size out of bounds (%" PRIu16 "/%" PRIu16 ")\n",
                        value.stroffs, value.strsize );
                    dump_keepvar( var );
                    return tri_false;
                }
                if ( value.strsize ) {
                    memcpy( tmp, &comp->strs[ value.stroffs ], value.strsize );
                }
                tmp[ value.strsize ] = '\0';
                compstr = tmp;
            }
            if ( strcmp( compstr, pval->sval ? pval->sval : "" ) != 0 ) {
                fprintf( stderr, "? Variable error, expected '%s' but got '%s'\n", pval->sval, compstr );
                dump_keepvar( var );
                tri = tri_false;
            } else {
                tri = tri_true;
            }
            if ( compstr == tmp ) {
                free( tmp );
            }
            return tri;
        case VARTYPEV_LABEL:
            if ( value.lbloffs != (uint16_t) pval->ival ) {
                fprintf( stderr, "? Variable error, expected %" PRIu16 " but got %" PRIu16 "\n", value.lbloffs,
                    (uint16_t) pval->ival );
                dump_keepvar( var );
                return tri_false;
            }
            return tri_true;
    }
    fprintf( stderr, "? Internal error\n" );
    dump_keepvar( var );
    return tri_false;
}

static tristate_t modify_keepvar( compiler_t* comp, keepvar_t* var ) {

    if ( var->varoffs == VAROFFS_NONE ) {
        // not allocated
        return tri_undecided;
    }

    // retrieve variable content (or for arrays, a random cell value)
    uint16_t diminx[MAXDIM]; memset( diminx, 0, sizeof(uint16_t) * MAXDIM );
    for ( uint8_t i=UINT8_C(0); i < var->numdims; ++i ) {
        diminx[i] = (uint16_t) getrand( var->dims[i] );
    }
    varvalue_t value; memset( &value, 0, sizeof(varvalue_t) );

    // compute cell address to be modified
    keepval_t* pval = 0;
    if ( var->vartype & VARTYPEF_ARRAY ) {
        uint16_t offs = UINT16_C(0);
        for ( uint8_t i=0; i < var->numdims; ++i ) {
            // compute slice size (the computed product over all subsequent dimensions)
            uint16_t slice = UINT16_C(0);
            for ( uint8_t j=i+UINT8_C(1); j < var->numdims; ++j ) {
                if ( j == UINT8_C(0) ) {
                    slice += var->dims[j];
                } else {
                    slice *= var->dims[j];
                }
            }
            if ( i == var->numdims - UINT8_C(1) ) {
                // final dimension:
                // add last index
                offs += diminx[i];
            } else {
                // not final dimension:
                // add N x slice to the offset
                offs += diminx[i] * slice;
            }
            if ( offs >= var->numcells ) {
                fprintf( stderr, "? internal error\n" );
                return tri_false;
            }
            pval = &var->cells[offs];
        }
    } else if ( var->vartype & VARTYPEF_FUNC ) {
        pval = 0;
    } else {
        pval = &var->value;
    }

    if ( pval == 0 ) {
        // function varable: not modified
        return tri_undecided;
    }

    // modify the value to be modified
    char tmp[256]; tmp[0] = '\0'; uint8_t len = UINT8_C(0); uint64_t ti0, ti1;
    switch ( var->vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:
            value.dblval = pval->dval = (double) getrand( INT32_MAX );
            break;
        case VARTYPEV_INT:
            value.intval = pval->ival = (int16_t) getrand( INT32_MAX );
            break;
        case VARTYPEV_STR:
            getrandstr( tmp, &len );
            if ( pval->sval ) {
                free( pval->sval ); pval->sval = 0;
            }
            pval->sval = strdup( tmp );
            if ( pval->sval == 0 ) {
                fprintf( stderr, "? Out of memory\n" );
                return tri_false;
            }
            if ( len == UINT8_C(0) ) {
                // special case empty string:
                value.stroffs = STROFFS_NONE;
                value.strsize = len;
            } else {
                value.stroffs = STROFFS_NONE;
                value.strsize = UINT8_C(0);
                ti0 = sdlutil_getnsec(0);
                if ( comp_alloc_strs( comp, len, &value.stroffs ) && value.stroffs != STROFFS_NONE ) {
                    value.strsize = len;
                }
                ti1 = sdlutil_getnsec(0);
                var->cumul_nsec_write += ti1 - ti0;
            }
            break;
        case VARTYPEV_LABEL:
            value.lbloffs = pval->ival = CODEOFFS_NONE;
            break;
    }

    ti0 = sdlutil_getnsec(0);
    bool ok = comp_set_var( comp, var->varoffs, var->vartype, var->numdims, diminx, &value );
    ti1 = sdlutil_getnsec(0);
    var->cumul_nsec_write += ti1 - ti0;

    if ( !ok ) {
        fprintf( stderr, "comp_set_var() failed\n" );
        return tri_false;
    }

    return tri_true;
}

#define NKEEPVARS       10
#define MAXITERATIONS   1000

typedef struct _keep_t {
    keepvar_t   kv[NKEEPVARS];
    int         num_create, num_delete, num_read, num_write;
    int         iterations;
} keep_t;

static keep_t   keep;

static void init_keep( keep_t* keep ) {
    memset( keep, 0, sizeof(keep_t) );
    for ( int i=0; i < NKEEPVARS; ++i ) {
        init_keepvar( &keep->kv[i] );
    }
}

static bool keep_iterate( keep_t* keep, compiler_t* comp ) {
    if ( ++keep->iterations > MAXITERATIONS ) {
        --keep->iterations;
        return false;
    }

    // select random keep variable
    int kv = getrand( NKEEPVARS );

    // select random action
    int32_t action = getrand( 10 );

    if ( action < 3 ) {  // allocate or remove a variable
        // 0, 1, 2
        if ( action == 2 ) {
            // remove
            if ( !dealloc_keepvar( comp, &keep->kv[kv] ) ) {
                fprintf( stderr, "dealloc_keepvar() returned false\n" );
                return false;
            }
            ++keep->num_delete;
        } else if ( action == 1 ) {
            // create
            if ( !alloc_keepvar( comp, &keep->kv[kv] ) ) {
                fprintf( stderr, "alloc_keepvar() returned false\n" );
                return false;
            }
            ++keep->num_create;
        }

    } else if ( action < 7 ) {   // manipulate or verify a variable
        // 3, 4, 5, 6

        if ( action < 5 ) {
            tristate_t tri = modify_keepvar( comp, &keep->kv[kv] );
            if ( tri == tri_false ) {
                fprintf( stderr, "modify_keepvar() returned false\n" );
                return false;
            }
            if ( tri == tri_true ) {
                ++keep->num_write;
            }
        } else {
            tristate_t tri = test_keepvar( comp, &keep->kv[kv] );
            if ( tri == tri_false ) {
                fprintf( stderr, "test_keepvar() returned false\n" );
                return false;
            }
            if ( tri == tri_true ) {
                ++keep->num_read;
            }
        }

    } else {    // do nothing
        uint64_t min_sleep = UINT64_C(1000000000) / UINT64_C(10000);
        uint64_t max_sleep = UINT64_C(1000000000) / UINT64_C(100);
        uint64_t max_rand  = max_sleep - min_sleep;
        uint64_t rnd_sleep = min_sleep + getrand( (int32_t) max_rand );
        sdlutil_nanosleep( rnd_sleep, 0 );
    }

    return true;
}

int main( int argc, char** argv ) {

    if ( setjmp( errorjmp ) != 0 ) {
        fprintf( stderr, "*** FATAL, stop\n" );
        return 1;
    }

    compiler_t comp;
    init_compiler( &comp, 0, false );
    comp.halt = fatal_error;
    comp.report = report_error;

    init_keep( &keep );

    for (;;) {
        if ( !keep_iterate( &keep, &comp ) ) {
            break;
        }
    }

    uint64_t cumul_create = 0, cumul_delete = 0, cumul_read = 0, cumul_write = 0;
    for ( int i=0; i < NKEEPVARS; ++i ) {
        cumul_create += keep.kv[i].cumul_nsec_create;
        cumul_delete += keep.kv[i].cumul_nsec_delete;
        cumul_read   += keep.kv[i].cumul_nsec_read;
        cumul_write  += keep.kv[i].cumul_nsec_write;
    }
    uint64_t avg_create = cumul_create / ( (unsigned) ( keep.num_create ? keep.num_create : 1 ) );
    uint64_t avg_delete = cumul_delete / ( (unsigned) ( keep.num_delete ? keep.num_delete : 1 ) );
    uint64_t avg_read   = cumul_read   / ( (unsigned) ( keep.num_read   ? keep.num_read   : 1 ) );
    uint64_t avg_write  = cumul_write  / ( (unsigned) ( keep.num_write  ? keep.num_write  : 1 ) );
    int action_cnt = keep.num_create + keep.num_delete + keep.num_read + keep.num_write;
    int num_idle = keep.iterations - action_cnt;

    printf( "iterations: %d\n", keep.iterations );
    printf( "create: %d (time: %" PRIu64 " ns, avg: %" PRIu64 " ns)\n", keep.num_create, cumul_create, avg_create );
    printf( "delete: %d (time: %" PRIu64 " ns, avg: %" PRIu64 " ns)\n", keep.num_delete, cumul_delete, avg_delete );
    printf( "read  : %d (time: %" PRIu64 " ns, avg: %" PRIu64 " ns)\n", keep.num_read  , cumul_read  , avg_read   );
    printf( "write : %d (time: %" PRIu64 " ns, avg: %" PRIu64 " ns)\n", keep.num_write , cumul_write , avg_write  );
    printf( "idle  : %d\n", num_idle );

    // TBD: output statistics

    return EXIT_SUCCESS;
}