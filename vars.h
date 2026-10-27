#pragma once
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

#ifndef VARS_H
#define VARS_H  1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The variable memory holds all the variables of the program (or direct mode line).

Every variable is a separate heap allocation (a var_t) and is addressed by its variable number, an index into
a table of pointers. The variable number is what the code generator stores in the instructions (and what the
compiler stores in the syntax tree), so it stays valid for as long as the variable exists, regardless of what
happens to the heap. The numbers of deleted variables are reused.

There are no fixed memory areas any more: variable names, strings, array elements and function parameter
lists are heap allocated and limited only by available memory. The only limit is the variable number: it is
a 16-bit value (VAROFFS_NONE is reserved), so there can be at most 65535 variables.

Variable types:

        +---+---+---+---++---+---+---+---+
        | . | . | f | s || e | e | e | e |
        +---+---+---+---++---+---+---+---+

        f - user-defined function (supertype 0 only)
        s - supertype (0=regular, 1=array)
        e - element type
            0000 - floating-point (64 bit IEEE)
            0001 - integer (16 bit)
            0010 - string (heap allocated, with a length)
            0011 - label (16 bit offset into code memory)
            0100 .. 1111 reserved

Variables with the same name but a different type are distinct (like A and A$ and A%, or A and A(...)).
A user-defined function variable has a list of parameters (type and name) and a code offset.
Arrays are addressed by one index per dimension, each in 0 .. dimension-1.
New variables are initialized to zero, the empty string, or CODEOFFS_NONE (labels and functions).
*/

#define VARTYPEF_FUNC   UINT8_C(0X20)
#define VARTYPEF_ARRAY  UINT8_C(0X10)
#define VARTYPEM_BASE   UINT8_C(0X03)
#define VARTYPEM_INVAL  UINT8_C(0XCC)
#define VARTYPEV_FLOAT  UINT8_C(0X00)
#define VARTYPEV_INT    UINT8_C(0X01)
#define VARTYPEV_STR    UINT8_C(0X02)
#define VARTYPEV_LABEL  UINT8_C(0X03)

#define VAROFFS_NONE    UINT16_C(0XFFFF)    // no variable (variable numbers are 0 .. 65534)

#define MAXDIM          6U
#define MAXPARAM        8U
#define MAXVARS         65535U              // maximum number of variables
#define MAXELEMS        UINT32_C(0X10000000)    // maximum number of elements of an array

typedef struct _usrparam_t {
    const char* paramname;
    uint8_t     paramtype;
} usrparam_t;

typedef union _varvalue_t {
    struct {
        const char* strptr;     // vmem_get_var(): points into the variable (valid until it is modified), NULL if empty
        size_t      strlen;     // vmem_set_var(): the string is copied, NULL and 0 stand for the empty string
    };
    double      dblval;
    int16_t     intval;
    uint16_t    lbloffs;
    uint16_t    codeoffs;
} varvalue_t;

typedef struct _vstr_t {
    char*       ptr;            // NUL-terminated copy, NULL if empty
    size_t      len;
} vstr_t;

typedef struct _var_t {
    struct _var_t*  hashnext;   // next variable in the hash bucket
    uint16_t        num;        // the variable number
    uint8_t         type;
    char*           name;
    union {                     // regular variable, label or function
        double      dbl;
        int16_t     intval;
        vstr_t      str;
        uint16_t    codeoffs;   // label or function
    } v;
    uint8_t         numdims;    // array
    uint16_t        dims[MAXDIM];
    size_t          slice[MAXDIM];  // elements to skip for one step in the dimension
    size_t          numelems;
    union {
        double*     dbls;
        int16_t*    ints;
        vstr_t*     strs;
        void*       any;
    } elems;
    uint8_t         numparams;  // function
    struct {
        uint8_t     type;
        char*       name;
    } params[MAXPARAM];
} var_t;

typedef struct _varmem_t {
    void*           userdata;
    void            (*report)( struct _varmem_t*, void*, const char* );
    void            (*halt)( struct _varmem_t*, void* ) ATTR_NORETURN;
    var_t**         table;      // variable number -> variable (NULL if not in use)
    size_t          numslots;   // used part of the table
    size_t          maxslots;   // allocated size of the table
    size_t          firstfree;  // no free slot below this number
    size_t          numvars;    // number of variables in use
    var_t**         buckets;    // hash table
    size_t          numbuckets;
} varmem_t;

void init_varmem( varmem_t* vmem );         // doesn't free anything: call cleanup_varmem() first if it is in use
void cleanup_varmem( varmem_t* vmem );      // deletes all variables

var_t* vmem_var( varmem_t* vmem, uint16_t varnum );     // NULL if there's no such variable

/*
WARNING: The return value of vmem_lookup_var() doesn't indicate find success.
If it returns true with VAROFFS_NONE, the variable was not found.
*/
bool vmem_lookup_var( varmem_t* vmem, uint8_t vartype, const char* name, uint16_t* poutoffs );
bool vmem_create_var( varmem_t* vmem, uint8_t vartype, const char* name, uint8_t numdims, const uint16_t* dims,
    uint8_t numparams, const usrparam_t* params, uint16_t* poutoffs );
bool vmem_delete_var( varmem_t* vmem, uint16_t varoffs );
bool vmem_erase_array( varmem_t* vmem, uint16_t varoffs );     // resets all elements: 0 or the empty string
bool vmem_get_var( varmem_t* vmem, uint16_t varoffs, uint8_t vartype, uint8_t numdims, const uint16_t* diminx,
    varvalue_t* pvalue );
bool vmem_set_var( varmem_t* vmem, uint16_t varoffs, uint8_t vartype, uint8_t numdims, const uint16_t* diminx,
    const varvalue_t* pvalue );

// user-defined function variables: type (incl. VARTYPEF_FUNC), number of parameters, code offset (CODEOFFS_NONE if undefined)
bool vmem_get_func_info( varmem_t* vmem, uint16_t varoffs, uint8_t* pfunctype, uint8_t* pnumparams, uint16_t* pcodeoffs );
// type and name of a parameter (the name belongs to the variable)
bool vmem_get_func_param( varmem_t* vmem, uint16_t varoffs, uint8_t index, uint8_t* pparamtype, const char** pname );

#endif
