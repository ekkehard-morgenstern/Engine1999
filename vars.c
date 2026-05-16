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

#include "vars.h"
#include "codegen.h"

void init_varmem( varmem_t* vmem ) {
    vmem->report   = 0;
    vmem->halt     = 0;
    vmem->userdata = 0;
    vmem->varssize = MAXVARS * 2U;
    vmem->strssize = UINT16_C(0);
    vmem->numvars  = UINT16_C(0);
}

static void vmem_error( varmem_t* vmem, const char* text ) {
    char buf[128];
    snprintf( buf, 128U, "? %s\n", text );
    if ( vmem->report ) {
        vmem->report( vmem, vmem->userdata, buf );
    } else {
        fprintf( stderr, "%s", buf );
    }
    if ( vmem->halt ) {
        vmem->halt( vmem, vmem->userdata );
    } else {
        exit( EXIT_FAILURE );
    }
}

static void vmem_compact_vars( varmem_t* vmem ) {
fprintf( stderr, "*** vmem_compact_vars() called ***\n" );
    // algorithm:
    //  - by scanning the variable offset table, iterate over every variable (regular and array)
    //  - if it's still being used (not marked as deleted), copy it to temporary memory.
    //  - copy the resulting memory back, noting the new size
    static uint8_t tmp[VARSSIZE_MAX];
    uint16_t tmpoffs = MAXVARS * 2U;
    // iterate over the variables
    for ( uint16_t i=0; i < vmem->numvars; ++i ) {
        uint16_t indexpos = i * 2U;
        // get the offset of the variable header stored in the offset cell
        uint16_t offs = VEXTRACT16( vmem, indexpos );
        if ( offs == VAROFFS_NONE ) {
            // variable is unused; store that info
            VTWRITE16( tmp, indexpos, VAROFFS_NONE );
            continue;
        }
        // offset points to a variable header: check
        // <size.16> <type.8> <namelen.8> <name...> [ <arraydims...> | <numargs> <argdesc...> ] <data...>
        uint16_t size = VEXTRACT16( vmem, offs );
        uint16_t begoffs = tmpoffs;
        // we need to trust that the size is correct.
        if ( size ) {
            memcpy( &tmp[ tmpoffs ], &vmem->vars[ offs ], size );
            tmpoffs += size;
        }
        // store the new offset in the target area
        VTWRITE16( tmp, indexpos, begoffs );
    }
    // copy the results back
    memcpy( vmem->vars, tmp, tmpoffs );
    vmem->varssize = tmpoffs;
}

bool vmem_alloc_vars( varmem_t* vmem, uint16_t size, uint16_t* poffs ) {
    if ( size > VARSSIZE_MAX - vmem->varssize ) {
        vmem_compact_vars( vmem );
        if ( size > VARSSIZE_MAX - vmem->varssize ) {
            return false;
        }
    }
    *poffs = (uint16_t) vmem->varssize;
    vmem->varssize += size;
    return true;
}

static bool vmem_create_var_offset( varmem_t* vmem, uint16_t* poffs ) {
    if ( vmem->numvars >= MAXVARS ) {
        for ( uint16_t i=UINT16_C(0); i < MAXVARS; ++i ) {
            uint16_t pos  = i * UINT16_C(2);
            uint16_t offs = VEXTRACT16( vmem, pos );
            if ( offs != VAROFFS_NONE ) {
                continue;
            }
            *poffs = pos;
            return true;
        }
        return false;
    }
    uint16_t pos = vmem->numvars * UINT16_C(2);
    VWRITE16( vmem, pos, VAROFFS_NONE );
    *poffs = pos; ++vmem->numvars;
    return true;
}

static void vmem_get_array_num_elems( varmem_t* vmem, uint16_t offs, uint16_t* pnumelem, uint16_t* pdataoffs ) {
    // call this ONLY for an array type!
    // <numdims.8> <arraydims...>
    uint8_t  numdims  = vmem->vars[ offs++ ];
    uint16_t numelems = UINT16_C(0);
    for ( uint8_t i = UINT8_C(0); i < numdims; ++i ) {
        // <dim.16> <slice.16>
        uint16_t dim = VEXTRACT16( vmem, offs );
        if ( numelems ) {
            numelems *= dim;
        } else {
            numelems += dim;
        }
        offs += UINT16_C(4);
    }
    *pnumelem  = numelems;
    *pdataoffs = offs;
}

bool vmem_lookup_var( varmem_t* vmem, uint8_t vartype, const char* name, uint16_t* poutoffs ) {
    /*
    WARNING: The function return value doesn't indicate find success.
    If the function returns true with an offset of VAROFFS_NONE, the variable was not found.
    */
    size_t namelen = strlen( name );
    if ( namelen == 0 ) {
        // it's an error to have a zero-length name
        return false;
    }
    for ( uint16_t i=0; i < vmem->numvars; ++i ) {
        uint16_t indexpos = i * 2U;
        // get the offset of the variable header stored in the offset cell
        uint16_t offs = VEXTRACT16( vmem, indexpos );
        if ( offs == VAROFFS_NONE ) {
            // variable is unused; skip
            continue;
        }
        // offset points to a variable header: check
        // <size.16> <type.8> <namelen.8> <name...> [ <numdims.8> <arraydims...> | <numargs> <argdesc...> ] <data...>
        uint16_t var_type = VEXTRACT16( vmem, offs + 2U );
        /*
            +---+---+---+---++---+---+---+---+
            | . | . | f | s || e | e | e | e |
            +---+---+---+---++---+---+---+---+
        */
        if ( var_type != vartype ) {
            // wrong variable type, skip ->
            // (in BASIC, it's permitted to have variables with same name but different type, like 'a$' and 'a' are distinct)
            continue;
        }
        uint8_t name_len = vmem->vars[ offs + 3U ];
        if ( name_len != namelen ) {
            // different name length, skip ->
            continue;
        }
        if ( memcmp( &vmem->vars[ offs + 4U ], name, name_len ) != 0 ) {
            // name mismatch, skip ->
            continue;
        }
        // exact match: return variable index offset
        *poutoffs = indexpos;
        return true;
    }
    // not found
    *poutoffs = VAROFFS_NONE;
    return true;
}

static uint16_t get_elemsize( uint8_t vartype ) {
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:    return UINT16_C(8);     // 64 bit float
        case VARTYPEV_INT:      return UINT16_C(2);     // 16 bit integer
        case VARTYPEV_STR:      return UINT16_C(4);     // offset in string space + length
        case VARTYPEV_LABEL:    return UINT16_C(2);     // offset in code space
    }
    return UINT16_C(0);
}

bool vmem_create_var( varmem_t* vmem, uint8_t vartype, const char* name, uint8_t numdims, const uint16_t* dims,
    uint8_t numparams, const usrparam_t* params, uint16_t* poutoffs ) {
    uint16_t indexpos = VAROFFS_NONE;
    size_t namelen = name ? strlen(name) : 0;
    // some sanity checks
    if ( vartype & VARTYPEM_INVAL || name == 0 || namelen == 0 || namelen > 255U || ( numdims != 0 && dims == 0 ) ||
        numdims > MAXDIM || ( numparams != 0 && params == 0 ) || numparams > MAXPARAM || poutoffs == 0 ) {
INTERR: vmem_error( vmem, "Internal error (invalid parameter to vmem_create_var())" );
        return false;
    }
    if ( ( vartype & ( VARTYPEF_ARRAY | VARTYPEF_FUNC ) ) == ( VARTYPEF_ARRAY | VARTYPEF_FUNC ) ) {
        // cannot be array and function at the same time
        goto INTERR;
    }
    size_t dimsize = 0, slicesize[MAXDIM] = { 0, 0, 0, 0, 0, 0 };
    for ( uint8_t i=0; i < numdims; ++i ) {
        if ( i == 0 ) {
            dimsize += dims[i];
        } else {
            dimsize *= dims[i];
        }
        for ( uint8_t j = i + UINT8_C(1); j < numdims; ++j ) {
            if ( j == 0 ) {
                slicesize[i] += dims[j];
            } else {
                slicesize[i] *= dims[j];
            }
        }
        if ( slicesize[i] == 0 ) {
            slicesize[i] = 1;   // final dimension
        }
    }
    if ( ( numdims && dimsize == 0 ) || dimsize > 65535U ) {
        if ( dimsize > 65535U ) {
TOOLARGE:   vmem_error( vmem, "Array too large" );
            return false;
        }
        goto INTERR;
    }
    for ( uint8_t i=0; i < numparams; ++i ) {
        if ( params[i].paramname == 0 || params[i].paramtype & VARTYPEM_INVAL ) {
            goto INTERR;
        }
        size_t nlen = strlen( params[i].paramname );
        if ( nlen == 0U || nlen > 255U ) {
            goto INTERR;
        }
    }
    // parameters seem OK, attempt to look up the variable
    if ( !vmem_lookup_var( vmem, vartype, name, &indexpos ) || indexpos != VAROFFS_NONE ) {
        // error or variable already exists: return error
        vmem_error( vmem, "Variable already exists" );
        return false;
    }
    // variable really does not exist: create
    if ( !vmem_create_var_offset( vmem, &indexpos ) || indexpos == VAROFFS_NONE ) {
        // most likely, variable index is full
        vmem_error( vmem, "Too many variables" );
        return false;
    }
    // We expect that everything concerning the variable parameters has already been checked.
    // Supplying nonsense here WILL result in undefined behavior.
    // I added some simple checks at the beginning to remedy that a bit, but they don't cover all of the cases.

    // <size.16> <type.8> <namelen.8> <name...> [ <numdims.8> <arraydims...> | <numargs> <argdesc...> ] <data...>
    //
    // An array variable has an additional arraydims field. Each dimension is stored as a
    // 16-bit value (in network byte order), succeeded by another 16-bit value in network byte
    // order specifying the slice size in bytes (to simplify addressing in multidimensional arrays).
    //
    // A user-defined function variable has N argument descriptor fields. Each argument descriptor contains a type field
    // and a name field (with preceding length byte).

    size_t elemsize = get_elemsize( vartype );
    size_t varsize = 4U + namelen;
    if ( vartype & VARTYPEF_ARRAY ) {
        if ( numdims == 0U ) {
            goto INTERR;
        }
        varsize += 1U + numdims * 4U + dimsize * elemsize;
    } else if ( vartype & VARTYPEF_FUNC ) {
        varsize += 1U;
        for ( uint8_t i=UINT8_C(0); i < numparams; ++i ) {
            varsize += 2U + strlen( params[i].paramname );
        }
        varsize += 2U;  // offset in code segment
    } else {
        // regular variable
        varsize += elemsize;
    }

    if ( varsize > 65535U ) {
        goto TOOLARGE;
    }

    uint16_t memoffs = VAROFFS_NONE;
    if ( !vmem_alloc_vars( vmem, (uint16_t) varsize, &memoffs ) || memoffs == VAROFFS_NONE ) {
        vmem_error( vmem, "Out of memory (ran out of variable space in vmem_alloc_vars())" );
        return false;
    }

    // got the variable memory: set up variable header

    // <size.16> <type.8> <namelen.8> <name...> [ <numdims.8> <arraydims...> | <numargs> <argdesc...> ] <data...>
    //
    // An array variable has an additional arraydims field. Each dimension is stored as a
    // 16-bit value (in network byte order), succeeded by another 16-bit value in network byte
    // order specifying the slice size in bytes (to simplify addressing in multidimensional arrays).
    //
    // A user-defined function variable has N argument descriptor fields. Each argument descriptor contains a type field
    // and a name field (with preceding length byte).

    uint16_t membeg = memoffs;
    VWRITE16( vmem, memoffs, varsize );
    vmem->vars[ memoffs + 2U ] = vartype;
    vmem->vars[ memoffs + 3U ] = (uint8_t) namelen;
    memoffs += UINT16_C(4);
    memcpy( &vmem->vars[ memoffs ], name, namelen );
    memoffs += (uint16_t) namelen;

    if ( vartype & VARTYPEF_ARRAY ) {
        vmem->vars[ memoffs++ ] = numdims;
        for ( uint8_t i=0; i < numdims; ++i ) {
            uint16_t slice = (uint16_t)( slicesize[i] * elemsize );
            VWRITE16( vmem, memoffs, dims[i] );
            memoffs += UINT16_C(2);
            VWRITE16( vmem, memoffs, slice   ); // note the premultiplied slice info
            memoffs += UINT16_C(2);
        }
    } else if ( vartype & VARTYPEF_FUNC ) {
        vmem->vars[ memoffs++ ] = numparams;
        for ( uint8_t i=0; i < numparams; ++i ) {
            uint8_t nlen = (uint8_t) strlen( params[i].paramname );
            vmem->vars[ memoffs++ ] = params[i].paramtype;
            vmem->vars[ memoffs++ ] = nlen;
            memcpy( &vmem->vars[ memoffs ], params[i].paramname, nlen );
            memoffs += nlen;
        }
    }

    // initialize the data
    if ( vartype & VARTYPEF_ARRAY ) {
        uint16_t numwritten = memoffs - membeg;
        uint16_t numleft    = (uint16_t)( varsize - numwritten );
        switch ( vartype & VARTYPEM_BASE ) {
            case VARTYPEV_FLOAT:
            case VARTYPEV_INT:
                // float and int arrays are initialized to zero
                memset( &vmem->vars[ memoffs ], 0, numleft );
                break;
            case VARTYPEV_STR:
                // string arrays are initialized to STROFFS_NONE (0xFFFF) / zero length
                for ( size_t i=0; i < dimsize; ++i ) {
                    VWRITE16( vmem, memoffs, STROFFS_NONE );
                    memoffs += UINT16_C(2);
                    VWRITE16( vmem, memoffs, 0 );
                    memoffs += UINT16_C(2);
                }
                break;
            case VARTYPEV_LABEL:
                // label arrays do not exist
                goto INTERR;
        }
    } else if ( vartype & VARTYPEF_FUNC ) {
        // a function only has a single offset member
        VWRITE16( vmem, memoffs, CODEOFFS_NONE );
    } else {
        // individual variables
        switch ( vartype & VARTYPEM_BASE ) {
            case VARTYPEV_FLOAT:
                // a float variable is set to zero
                memset( &vmem->vars[ memoffs ], 0, 8U );
                break;
            case VARTYPEV_INT:
                // an int variable is set to zero
                VWRITE16( vmem, memoffs, 0 );
                break;
            case VARTYPEV_STR:
                // a string variable is set to STROFFS_NONE / zero length
                VWRITE16( vmem, memoffs, STROFFS_NONE );
                memoffs += UINT16_C(2);
                VWRITE16( vmem, memoffs, STROFFS_NONE );
                break;
            case VARTYPEV_LABEL:
                // a label variable is set to CODEOFFS_NONE
                VWRITE16( vmem, memoffs, CODEOFFS_NONE );
                break;
        }
    }

    // set the variable index entry
    VWRITE16( vmem, indexpos, membeg );

    // return the variable index position
    *poutoffs = indexpos;

    return true;
}

bool vmem_delete_var( varmem_t* vmem, uint16_t varoffs ) {
    if ( varoffs >= MAXVARS * 2U ) {    // bad varoffs
        return false;
    }
    uint16_t memoffs = VEXTRACT16( vmem, varoffs );
    if ( memoffs == VAROFFS_NONE ) {    // not allocated
        return false;
    }
    // <size.16> <type.8> <namelen.8> <name...> [ <numdims.8> <arraydims...> | <numargs> <argdesc...> ] <data...>
    uint16_t membeg = memoffs;
    uint16_t blksize = VEXTRACT16( vmem, memoffs );
    if ( blksize == UINT16_C(0) ) {
        return false;
    }
    memset( &vmem->vars[ membeg ], 0, blksize );
    VWRITE16( vmem, varoffs, VAROFFS_NONE );
    return true;
}

static bool vmem_get_array_elem( varmem_t* vmem, uint8_t vartype, uint8_t numdims, const uint16_t* diminx, uint16_t* pmemoffs ) {
    uint16_t memoffs = *pmemoffs;
    uint8_t dimcnt = vmem->vars[ memoffs++ ];
    if ( dimcnt != numdims || dimcnt > MAXDIM || diminx == 0 ) {
        return false;
    }
    uint16_t maxdim[MAXDIM], slice[MAXDIM];
    for ( uint8_t i=0; i < dimcnt; ++i ) {
        maxdim[i] = VEXTRACT16( vmem, memoffs );
        memoffs += UINT16_C(2);
        slice[i] = VEXTRACT16( vmem, memoffs );
        memoffs += UINT16_C(2);
    }
    for ( uint8_t i=0; i < dimcnt; ++i ) {
        uint16_t inx = diminx[i];
        if ( inx >= maxdim[i] ) {
            return false;
        }
        memoffs += inx * slice[i];
    }
    *pmemoffs = memoffs;
    return true;
}

static double vmem_extract_float( const uint8_t* mem, uint16_t offs );
static void vmem_write_float( uint8_t* mem, uint16_t offs, double val );

bool vmem_get_var( varmem_t* vmem, uint16_t varoffs, uint8_t vartype, uint8_t numdims, const uint16_t* diminx,
    varvalue_t* pvalue ) {
    if ( pvalue == 0 ) {
        return false;
    }
    if ( varoffs >= MAXVARS * 2U ) {    // bad varoffs
        return false;
    }
    if ( vartype & VARTYPEM_INVAL ) {
        return false;
    }
    uint16_t memoffs = VEXTRACT16( vmem, varoffs );
    if ( memoffs == VAROFFS_NONE ) {    // not allocated
        return false;
    }
    // <size.16> <type.8> <namelen.8> <name...> [ <numdims.8> <arraydims...> | <numargs> <argdesc...> ] <data...>
    uint8_t vartype2 = vmem->vars[ memoffs + 2U ];
    if ( ( vartype2 & VARTYPEM_INVAL ) != 0 || vartype2 != vartype ) {
        return false;
    }
    uint8_t namelen = vmem->vars[ memoffs + 3U ];
    memoffs += UINT16_C(4) + namelen;
    if ( vartype & VARTYPEF_ARRAY ) {
        if ( !vmem_get_array_elem( vmem, vartype, numdims, diminx, &memoffs ) ) {
            return false;
        }
        // array element is addressed by memoffs
    } else if ( vartype & VARTYPEF_FUNC ) {
        // A user-defined function variable has N argument descriptor fields. Each argument descriptor contains a
        // type field and a name field (with preceding length byte).
        uint8_t numargs = vmem->vars[ memoffs++ ];
        if ( numargs > MAXPARAM ) {
            return false;
        }
        for ( uint8_t i=UINT8_C(0); i < numargs; ++i ) {
            ++memoffs; // uint8_t argtype = vmem->vars[ memoffs++ ];
            uint8_t argnlen = vmem->vars[ memoffs++ ];
            memoffs += argnlen;
        }
        // the remaining field is a code offset
        pvalue->codeoffs = VEXTRACT16( vmem, memoffs );
        return true;
    } else {
        // regular variable, memoffs points to data field
    }
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:
            pvalue->dblval = vmem_extract_float( vmem->vars, memoffs );
            break;
        case VARTYPEV_INT:
            pvalue->intval = VEXTRACT16( vmem, memoffs );
            break;
        case VARTYPEV_STR:
            pvalue->stroffs = VEXTRACT16( vmem, memoffs );
            memoffs += UINT16_C(2);
            pvalue->strsize = VEXTRACT16( vmem, memoffs );
            break;
        case VARTYPEV_LABEL:
            pvalue->lbloffs = VEXTRACT16( vmem, memoffs );
            break;
    }
    return true;
}

bool vmem_set_var( varmem_t* vmem, uint16_t varoffs, uint8_t vartype, uint8_t numdims, const uint16_t* diminx,
    const varvalue_t* pvalue ) {
    if ( pvalue == 0 ) {
        return false;
    }
    if ( varoffs >= MAXVARS * 2U ) {    // bad varoffs
        return false;
    }
    if ( vartype & VARTYPEM_INVAL ) {
        return false;
    }
    uint16_t memoffs = VEXTRACT16( vmem, varoffs );
    if ( memoffs == VAROFFS_NONE ) {    // not allocated
        return false;
    }
    // <size.16> <type.8> <namelen.8> <name...> [ <numdims.8> <arraydims...> | <numargs> <argdesc...> ] <data...>
    uint8_t vartype2 = vmem->vars[ memoffs + 2U ];
    if ( ( vartype2 & VARTYPEM_INVAL ) != 0 || vartype2 != vartype ) {
        return false;
    }
    uint8_t namelen = vmem->vars[ memoffs + 3U ];
    memoffs += UINT16_C(4) + namelen;
    if ( vartype & VARTYPEF_ARRAY ) {
        if ( !vmem_get_array_elem( vmem, vartype, numdims, diminx, &memoffs ) ) {
            return false;
        }
        // array element is addressed by memoffs
    } else if ( vartype & VARTYPEF_FUNC ) {
        // A user-defined function variable has N argument descriptor fields. Each argument descriptor contains a
        // type field and a name field (with preceding length byte).
        uint8_t numargs = vmem->vars[ memoffs++ ];
        if ( numargs > MAXPARAM ) {
            return false;
        }
        for ( uint8_t i=UINT8_C(0); i < numargs; ++i ) {
            ++memoffs; // uint8_t argtype = vmem->vars[ memoffs++ ];
            uint8_t argnlen = vmem->vars[ memoffs++ ];
            memoffs += argnlen;
        }
        // the remaining field is a code offset
        VWRITE16( vmem, memoffs, pvalue->codeoffs );
        return true;
    } else {
        // regular variable, memoffs points to data field
    }
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:
            vmem_write_float( vmem->vars, memoffs, pvalue->dblval );
            break;
        case VARTYPEV_INT:
            VWRITE16( vmem, memoffs, pvalue->intval );
            break;
        case VARTYPEV_STR:
            VWRITE16( vmem, memoffs, pvalue->stroffs );
            memoffs += UINT16_C(2);
            VWRITE16( vmem, memoffs, pvalue->strsize );
            break;
        case VARTYPEV_LABEL:
            VWRITE16( vmem, memoffs, pvalue->lbloffs );
            break;
    }
    return true;
}

static void vmem_compact_strs( varmem_t* vmem ) {
fprintf( stderr, "*** vmem_compact_strs() called ***\n" );
    // algorithm:
    //  - iterate over all string variables (incl. every cell of each string array)
    //  - if the variable is non-empty, copy the string it points to to temporary memory
    //  - copy the resulting memory back, noting the new size
    //
    static uint8_t tmp[STRSSIZE_MAX];
    uint16_t tmpoffs = UINT16_C(0);
    // iterate over the variables
    for ( uint16_t i=0; i < vmem->numvars; ++i ) {
        uint16_t indexpos = i * 2U;
        // get the offset of the variable header stored in the offset cell
        uint16_t offs = VEXTRACT16( vmem, indexpos );
        if ( offs == VAROFFS_NONE ) {
            // variable is unused; skip
            continue;
        }
        // offset points to a variable header: check
        // <size.16> <type.8> <namelen.8> <name...> [ <numdims.8> <arraydims...> | <numargs> <argdesc...> ] <data...>
        uint16_t vartype = VEXTRACT16( vmem, offs + 2U );
        // skip intro fields
        uint8_t namelen = vmem->vars[ offs + 3U ];
        offs += UINT16_C(4) + namelen;
        /*
            +---+---+---+---++---+---+---+---+
            | . | . | f | s || e | e | e | e |
            +---+---+---+---++---+---+---+---+
        */
        if ( vartype & VARTYPEF_ARRAY ) {
            if ( ( vartype & VARTYPEM_BASE ) != VARTYPEV_STR ) {
                // not string array: skip
                continue;
            }
            // an array
            uint16_t numelems = UINT16_C(0);
            uint16_t dataoffs = UINT16_C(0);
            vmem_get_array_num_elems( vmem, offs, &numelems, &dataoffs );
            // each element is vmemrised of <stroffs.16> <length.16>
            for ( uint16_t j=UINT16_C(0); j < numelems; ++j ) {
                uint16_t varfld  = dataoffs;
                uint16_t lenfld  = varfld + UINT16_C(2);
                uint16_t stroffs = VEXTRACT16( vmem, varfld );
                uint16_t length  = VEXTRACT16( vmem, lenfld );
                if ( stroffs == STROFFS_NONE ) {
                    // empty string, skip
                    dataoffs += UINT16_C(4);
                    continue;
                }
                // have offset: transfer string to temporary area
                uint16_t tmpbeg = tmpoffs;
                if ( length ) {
                    memcpy( &tmp[ tmpoffs ], &vmem->strs[ stroffs ], length );
                    tmpoffs += length;
                }
                // write new offset
                VWRITE16( vmem, varfld, tmpbeg );
                // next element
                dataoffs += UINT16_C(4);
            }
        } else if ( ( vartype & VARTYPEM_BASE ) == VARTYPEV_STR ) {
            // <size.16> <type.8> <namelen.8> <name...> <stroffs.16> <length.16>
            uint16_t strfld  = offs;
            uint16_t lenfld  = strfld + UINT16_C(2);
            uint16_t stroffs = VEXTRACT16( vmem, strfld );
            uint16_t length  = VEXTRACT16( vmem, lenfld );
            if ( stroffs == STROFFS_NONE ) {
                // empty string, skip
                continue;
            }
            // have offset: transfer string to temporary area
            uint16_t tmpbeg = tmpoffs;
            if ( length ) {
                memcpy( &tmp[ tmpoffs ], &vmem->strs[ stroffs ], length );
                tmpoffs += length;
            }
            // write new offset
            VWRITE16( vmem, strfld, tmpbeg );
        }
    }
    // copy the results back
    memcpy( vmem->strs, tmp, tmpoffs );
    vmem->strssize = tmpoffs;
}

bool vmem_alloc_strs( varmem_t* vmem, uint16_t size, uint16_t* poffs ) {
    if ( size > STRSSIZE_MAX - vmem->strssize ) {
        vmem_compact_strs( vmem );
        if ( size > STRSSIZE_MAX - vmem->strssize ) {
            return false;
        }
    }
    *poffs = (uint16_t) vmem->strssize;
    vmem->strssize += size;
    return true;
}

static double vmem_extract_float( const uint8_t* mem, uint16_t offs ) {
    uint64_t val =
        ( ( (uint64_t) mem[ offs      ] ) << UINT8_C(56) ) |
        ( ( (uint64_t) mem[ offs + 1U ] ) << UINT8_C(48) ) |
        ( ( (uint64_t) mem[ offs + 2U ] ) << UINT8_C(40) ) |
        ( ( (uint64_t) mem[ offs + 3U ] ) << UINT8_C(32) ) |
        ( ( (uint32_t) mem[ offs + 4U ] ) << UINT8_C(24) ) |
        ( ( (uint32_t) mem[ offs + 5U ] ) << UINT8_C(16) ) |
        ( ( (uint16_t) mem[ offs + 6U ] ) << UINT8_C( 8) ) |
                       mem[ offs + 7U ]                    ;
    union {
        uint64_t ui64;
        double   dbl;
    } u;
    u.ui64 = val;
    return u.dbl;
}

static void vmem_write_float( uint8_t* mem, uint16_t offs, double val ) {
    union {
        uint64_t ui64;
        double   dbl;
    } u;
    u.dbl = val;
    mem[ offs      ] = (uint8_t)( u.ui64 >> UINT8_C(56) );
    mem[ offs + 1U ] = (uint8_t)( u.ui64 >> UINT8_C(48) );
    mem[ offs + 2U ] = (uint8_t)( u.ui64 >> UINT8_C(40) );
    mem[ offs + 3U ] = (uint8_t)( u.ui64 >> UINT8_C(32) );
    mem[ offs + 4U ] = (uint8_t)( u.ui64 >> UINT8_C(24) );
    mem[ offs + 5U ] = (uint8_t)( u.ui64 >> UINT8_C(16) );
    mem[ offs + 6U ] = (uint8_t)( u.ui64 >> UINT8_C( 8) );
    mem[ offs + 7U ] = (uint8_t)( u.ui64                );
}
