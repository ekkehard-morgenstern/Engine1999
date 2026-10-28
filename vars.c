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
    vmem->report     = 0;
    vmem->halt       = 0;
    vmem->userdata   = 0;
    vmem->table      = 0;
    vmem->numslots   = 0;
    vmem->maxslots   = 0;
    vmem->firstfree  = 0;
    vmem->numvars    = 0;
    vmem->buckets    = 0;
    vmem->numbuckets = 0;
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

// -- variables ---------------------------------------------------------------

static void vmem_free_var( var_t* var ) {
    if ( var->type & VARTYPEF_ARRAY ) {
        if ( ( var->type & VARTYPEM_BASE ) == VARTYPEV_STR && var->elems.strs ) {
            for ( size_t i=0; i < var->numelems; ++i ) {
                free( var->elems.strs[i].ptr );
            }
        }
        free( var->elems.any );
    } else if ( var->type & VARTYPEF_FUNC ) {
        for ( uint8_t i=0; i < var->numparams; ++i ) {
            free( var->params[i].name );
        }
    } else if ( ( var->type & VARTYPEM_BASE ) == VARTYPEV_STR ) {
        free( var->v.str.ptr );
    }
    free( var->name );
    free( var );
}

void cleanup_varmem( varmem_t* vmem ) {
    for ( size_t i=0; i < vmem->numslots; ++i ) {
        if ( vmem->table[i] ) {
            vmem_free_var( vmem->table[i] );
        }
    }
    free( vmem->table );
    free( vmem->buckets );
    vmem->table      = 0;
    vmem->numslots   = 0;
    vmem->maxslots   = 0;
    vmem->firstfree  = 0;
    vmem->numvars    = 0;
    vmem->buckets    = 0;
    vmem->numbuckets = 0;
}

var_t* vmem_var( varmem_t* vmem, uint16_t varnum ) {
    return varnum < vmem->numslots ? vmem->table[ varnum ] : 0;
}

// -- hash table --------------------------------------------------------------

static size_t vmem_hash( uint8_t type, const char* name ) {
    size_t h = (size_t) 14695981039346656037ULL ^ type;
    for ( const char* p = name; *p; ++p ) {
        h = ( h ^ (uint8_t) *p ) * (size_t) 1099511628211ULL;
    }
    return h;
}

static bool vmem_rehash( varmem_t* vmem, size_t newbuckets ) {
    var_t** buckets = (var_t**) calloc( newbuckets, sizeof(var_t*) );
    if ( buckets == 0 ) {
        return false;
    }
    for ( size_t i=0; i < vmem->numslots; ++i ) {
        var_t* var = vmem->table[i];
        if ( var ) {
            size_t b = vmem_hash( var->type, var->name ) & ( newbuckets - 1U );
            var->hashnext = buckets[b];
            buckets[b] = var;
        }
    }
    free( vmem->buckets );
    vmem->buckets = buckets;
    vmem->numbuckets = newbuckets;
    return true;
}

static var_t* vmem_find( varmem_t* vmem, uint8_t vartype, const char* name ) {
    if ( vmem->numbuckets == 0 ) {
        return 0;
    }
    size_t b = vmem_hash( vartype, name ) & ( vmem->numbuckets - 1U );
    for ( var_t* var = vmem->buckets[b]; var; var = var->hashnext ) {
        // (in BASIC, it's permitted to have variables with same name but different type, like 'a$' and 'a' are distinct)
        if ( var->type == vartype && strcmp( var->name, name ) == 0 ) {
            return var;
        }
    }
    return 0;
}

bool vmem_lookup_var( varmem_t* vmem, uint8_t vartype, const char* name, uint16_t* poutoffs ) {
    if ( name == 0 || name[0] == '\0' ) {
        // it's an error to have a zero-length name
        return false;
    }
    var_t* var = vmem_find( vmem, vartype, name );
    *poutoffs = var ? var->num : VAROFFS_NONE;
    return true;
}

// -- creating and deleting ---------------------------------------------------

static size_t get_elemsize( uint8_t vartype ) {
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:    return sizeof(double);
        case VARTYPEV_INT:      return sizeof(int16_t);
        case VARTYPEV_STR:      return sizeof(vstr_t);
    }
    return 0;
}

bool vmem_create_var( varmem_t* vmem, uint8_t vartype, const char* name, uint8_t numdims, const uint16_t* dims,
    uint8_t numparams, const usrparam_t* params, uint16_t* poutoffs ) {
    // some sanity checks
    if ( ( vartype & VARTYPEM_INVAL ) != 0 || name == 0 || name[0] == '\0' || ( numdims != 0 && dims == 0 ) ||
        numdims > MAXDIM || ( numparams != 0 && params == 0 ) || numparams > MAXPARAM || poutoffs == 0 ) {
INTERR: vmem_error( vmem, "Internal error (invalid parameter to vmem_create_var())" );
        return false;
    }
    if ( ( vartype & ( VARTYPEF_ARRAY | VARTYPEF_FUNC ) ) == ( VARTYPEF_ARRAY | VARTYPEF_FUNC ) ) {
        // cannot be array and function at the same time
        goto INTERR;
    }
    bool isarray = ( vartype & VARTYPEF_ARRAY ) != 0;
    if ( isarray != ( numdims != 0 ) ) {
        goto INTERR;
    }
    if ( isarray && ( vartype & VARTYPEM_BASE ) == VARTYPEV_LABEL ) {
        goto INTERR;    // arrays of labels aren't supported
    }
    size_t numelems = 1;
    for ( uint8_t i=0; i < numdims; ++i ) {
        if ( dims[i] == 0 ) {
            goto INTERR;
        }
        if ( numelems > MAXELEMS / dims[i] ) {
TOOLARGE:   vmem_error( vmem, "Array too large" );
            return false;
        }
        numelems *= dims[i];
    }
    for ( uint8_t i=0; i < numparams; ++i ) {
        if ( params[i].paramname == 0 || params[i].paramname[0] == '\0' || ( params[i].paramtype & VARTYPEM_INVAL ) != 0 ) {
            goto INTERR;
        }
    }
    // parameters seem OK, attempt to look up the variable
    if ( vmem_find( vmem, vartype, name ) ) {
        vmem_error( vmem, "Variable already exists" );
        return false;
    }
    // find a variable number
    size_t num = vmem->firstfree;
    while ( num < vmem->numslots && vmem->table[ num ] ) {
        ++num;
    }
    if ( num >= MAXVARS ) {
        vmem_error( vmem, "Too many variables" );
        return false;
    }
    if ( num == vmem->numslots && vmem->numslots == vmem->maxslots ) {
        size_t newmax = vmem->maxslots ? vmem->maxslots * 2U : 64U;
        var_t** table = (var_t**) realloc( vmem->table, newmax * sizeof(var_t*) );
        if ( table == 0 ) {
OOM:        vmem_error( vmem, "Out of memory" );
            return false;
        }
        vmem->table = table;
        vmem->maxslots = newmax;
    }
    if ( vmem->numbuckets == 0 || vmem->numvars >= vmem->numbuckets * 2U ) {
        if ( !vmem_rehash( vmem, vmem->numbuckets ? vmem->numbuckets * 2U : 64U ) ) {
            goto OOM;
        }
    }

    // create the variable
    var_t* var = (var_t*) calloc( 1U, sizeof(var_t) );
    if ( var == 0 ) {
        goto OOM;
    }
    var->type = vartype;
    var->num  = (uint16_t) num;
    var->name = strdup( name );
    if ( var->name == 0 ) {
        free( var );
        goto OOM;
    }
    if ( isarray ) {
        var->numdims  = numdims;
        var->numelems = numelems;
        size_t slice = 1;
        for ( uint8_t i = numdims; i > 0; --i ) {
            var->dims[ i - 1U ]  = dims[ i - 1U ];
            var->slice[ i - 1U ] = slice;
            slice *= dims[ i - 1U ];
        }
        size_t elemsize = get_elemsize( vartype );
        if ( numelems > SIZE_MAX / elemsize ) {
            free( var->name );
            free( var );
            goto TOOLARGE;
        }
        var->elems.any = calloc( numelems, elemsize );  // zero, 0.0, empty strings
        if ( var->elems.any == 0 ) {
            free( var->name );
            free( var );
            goto OOM;
        }
    } else if ( vartype & VARTYPEF_FUNC ) {
        var->v.codeoffs = CODEOFFS_NONE;
        for ( uint8_t i=0; i < numparams; ++i ) {
            var->params[i].type = params[i].paramtype;
            var->params[i].name = strdup( params[i].paramname );
            if ( var->params[i].name == 0 ) {
                var->numparams = i;
                vmem_free_var( var );
                goto OOM;
            }
        }
        var->numparams = numparams;
    } else if ( ( vartype & VARTYPEM_BASE ) == VARTYPEV_LABEL ) {
        var->v.codeoffs = CODEOFFS_NONE;
    }

    // enter it into the table and the hash table
    vmem->table[ num ] = var;
    if ( num == vmem->numslots ) {
        ++vmem->numslots;
    }
    vmem->firstfree = num + 1U;
    ++vmem->numvars;
    size_t b = vmem_hash( vartype, name ) & ( vmem->numbuckets - 1U );
    var->hashnext = vmem->buckets[b];
    vmem->buckets[b] = var;

    *poutoffs = (uint16_t) num;
    return true;
}

bool vmem_delete_var( varmem_t* vmem, uint16_t varoffs ) {
    var_t* var = vmem_var( vmem, varoffs );
    if ( var == 0 ) {
        return false;
    }
    // unlink from the hash table
    size_t b = vmem_hash( var->type, var->name ) & ( vmem->numbuckets - 1U );
    for ( var_t** pp = &vmem->buckets[b]; *pp; pp = &(*pp)->hashnext ) {
        if ( *pp == var ) {
            *pp = var->hashnext;
            break;
        }
    }
    vmem->table[ varoffs ] = 0;
    if ( varoffs < vmem->firstfree ) {
        vmem->firstfree = varoffs;
    }
    --vmem->numvars;
    vmem_free_var( var );
    return true;
}

// -- values ------------------------------------------------------------------

// returns the address of the element
static bool vmem_elem_index( const var_t* var, uint8_t numdims, const uint16_t* diminx, size_t* pindex ) {
    if ( var->numdims != numdims || diminx == 0 ) {
        return false;
    }
    size_t index = 0;
    for ( uint8_t i=0; i < numdims; ++i ) {
        if ( diminx[i] >= var->dims[i] ) {
            return false;
        }
        index += diminx[i] * var->slice[i];
    }
    *pindex = index;
    return true;
}

bool vmem_get_var( varmem_t* vmem, uint16_t varoffs, uint8_t vartype, uint8_t numdims, const uint16_t* diminx,
    varvalue_t* pvalue ) {
    if ( pvalue == 0 || ( vartype & VARTYPEM_INVAL ) != 0 ) {
        return false;
    }
    var_t* var = vmem_var( vmem, varoffs );
    if ( var == 0 || var->type != vartype ) {
        return false;
    }
    if ( vartype & VARTYPEF_ARRAY ) {
        size_t index = 0;
        if ( !vmem_elem_index( var, numdims, diminx, &index ) ) {
            return false;
        }
        switch ( vartype & VARTYPEM_BASE ) {
            case VARTYPEV_FLOAT:
                pvalue->dblval = var->elems.dbls[ index ];
                return true;
            case VARTYPEV_INT:
                pvalue->intval = var->elems.ints[ index ];
                return true;
            case VARTYPEV_STR:
                pvalue->strptr = var->elems.strs[ index ].ptr;
                pvalue->strlen = var->elems.strs[ index ].len;
                return true;
        }
        return false;
    }
    if ( vartype & VARTYPEF_FUNC ) {
        pvalue->codeoffs = var->v.codeoffs;
        return true;
    }
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:
            pvalue->dblval = var->v.dbl;
            break;
        case VARTYPEV_INT:
            pvalue->intval = var->v.intval;
            break;
        case VARTYPEV_STR:
            pvalue->strptr = var->v.str.ptr;
            pvalue->strlen = var->v.str.len;
            break;
        case VARTYPEV_LABEL:
            pvalue->lbloffs = var->v.codeoffs;
            break;
    }
    return true;
}

static void vmem_set_str( varmem_t* vmem, vstr_t* dest, const char* ptr, size_t len ) {
    char* copy = 0;
    if ( ptr && len ) {
        copy = (char*) malloc( len + 1U );
        if ( copy == 0 ) {
            vmem_error( vmem, "Out of memory" );    // doesn't return normally
            return;
        }
        memcpy( copy, ptr, len );
        copy[ len ] = '\0';
    } else {
        len = 0;
    }
    free( dest->ptr );
    dest->ptr = copy;
    dest->len = len;
}

bool vmem_set_var( varmem_t* vmem, uint16_t varoffs, uint8_t vartype, uint8_t numdims, const uint16_t* diminx,
    const varvalue_t* pvalue ) {
    if ( pvalue == 0 || ( vartype & VARTYPEM_INVAL ) != 0 ) {
        return false;
    }
    var_t* var = vmem_var( vmem, varoffs );
    if ( var == 0 || var->type != vartype ) {
        return false;
    }
    if ( vartype & VARTYPEF_ARRAY ) {
        size_t index = 0;
        if ( !vmem_elem_index( var, numdims, diminx, &index ) ) {
            return false;
        }
        switch ( vartype & VARTYPEM_BASE ) {
            case VARTYPEV_FLOAT:
                var->elems.dbls[ index ] = pvalue->dblval;
                return true;
            case VARTYPEV_INT:
                var->elems.ints[ index ] = pvalue->intval;
                return true;
            case VARTYPEV_STR:
                vmem_set_str( vmem, &var->elems.strs[ index ], pvalue->strptr, pvalue->strlen );
                return true;
        }
        return false;
    }
    if ( vartype & VARTYPEF_FUNC ) {
        var->v.codeoffs = pvalue->codeoffs;
        return true;
    }
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:
            var->v.dbl = pvalue->dblval;
            break;
        case VARTYPEV_INT:
            var->v.intval = pvalue->intval;
            break;
        case VARTYPEV_STR:
            vmem_set_str( vmem, &var->v.str, pvalue->strptr, pvalue->strlen );
            break;
        case VARTYPEV_LABEL:
            var->v.codeoffs = pvalue->lbloffs;
            break;
    }
    return true;
}

// -- user-defined function variables -------------------------------------------

bool vmem_get_func_info( varmem_t* vmem, uint16_t varoffs, uint8_t* pfunctype, uint8_t* pnumparams, uint16_t* pcodeoffs ) {
    var_t* var = vmem_var( vmem, varoffs );
    if ( var == 0 || ( var->type & VARTYPEF_FUNC ) == 0 ) {
        return false;
    }
    *pfunctype  = var->type;
    *pnumparams = var->numparams;
    *pcodeoffs  = var->v.codeoffs;
    return true;
}

bool vmem_get_func_param( varmem_t* vmem, uint16_t varoffs, uint8_t index, uint8_t* pparamtype, const char** pname ) {
    var_t* var = vmem_var( vmem, varoffs );
    if ( var == 0 || ( var->type & VARTYPEF_FUNC ) == 0 || index >= var->numparams ) {
        return false;
    }
    *pparamtype = var->params[ index ].type;
    *pname      = var->params[ index ].name;
    return true;
}

bool vmem_get_var_name( varmem_t* vmem, uint16_t varoffs, const char** pname ) {
    var_t* var = vmem_var( vmem, varoffs );
    if ( var == 0 ) {
        return false;
    }
    *pname = var->name;
    return true;
}

void vmem_clear_vars( varmem_t* vmem ) {
    for ( size_t i=0; i < vmem->numslots; ++i ) {
        var_t* var = vmem->table[i];
        if ( var == 0 || var->name[0] == '@' || ( var->type & VARTYPEF_FUNC ) != 0 ||
             ( var->type & VARTYPEM_BASE ) == VARTYPEV_LABEL ) {
            continue;
        }
        if ( var->type & VARTYPEF_ARRAY ) {
            vmem_erase_array( vmem, var->num );
        } else if ( ( var->type & VARTYPEM_BASE ) == VARTYPEV_STR ) {
            free( var->v.str.ptr );
            var->v.str.ptr = 0;
            var->v.str.len = 0;
        } else if ( ( var->type & VARTYPEM_BASE ) == VARTYPEV_INT ) {
            var->v.intval = 0;
        } else {
            var->v.dbl = 0.0;
        }
    }
}

bool vmem_erase_array( varmem_t* vmem, uint16_t varoffs ) {
    var_t* var = vmem_var( vmem, varoffs );
    if ( var == 0 || ( var->type & VARTYPEF_ARRAY ) == 0 ) {
        return false;
    }
    if ( ( var->type & VARTYPEM_BASE ) == VARTYPEV_STR ) {
        for ( size_t i=0; i < var->numelems; ++i ) {
            free( var->elems.strs[i].ptr );
        }
    }
    memset( var->elems.any, 0, var->numelems * get_elemsize( var->type ) );
    return true;
}
