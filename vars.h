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
The variable segment contains all the variables in the program (or direct mode line).

There is a maximum of 1024 variables. The first 2 KiB of memory contains the real offsets of the variables.

Variables are stored one after the other. There's no list mechanism to chain them together.

If the memory is full and another variable is to be allocated, the variable space is compacted
first before making another attempt. The variable index at the beginning of the variable memory
is updated accordingly.

Strings are stored in a an extra space to avoid rapid exhaustion of the variable space.
If string space is exhausted when another string is allocated, the string space is compacted
first before making another attempt.

A variable is regarded as deleted when its index entry contains a special offset value.

The variable header is as follows:

    <size.16> <type.8> <namelen.8> <name...> [ <numdims.8> <arraydims...> | <numargs> <argdesc...> ] <data...>

    The size field is a 16-bit field in network byte order.

    The type field contains information about the variable type:

        +---+---+---+---++---+---+---+---+
        | . | . | f | s || e | e | e | e |
        +---+---+---+---++---+---+---+---+

        f - user-defined function (supertype 0 only)
        s - supertype (0=regular, 1=array)
        e - element type
            0000 - floating-point (64 bit IEEE)
            0001 - integer (16 bit)
            0010 - string (16 bit offset into string memory + 16 bit length)
            0011 - label (16 bit offset into code memory)
            0100 .. 1111 reserved

    An array variable has an additional arraydims field. Each dimension is stored as a
    16-bit value (in network byte order), succeeded by another 16-bit value in network byte
    order specifying the slice size in bytes (to simplify addressing in multidimensional arrays).

    A user-defined function variable has N argument descriptor fields. Each argument descriptor contains a type field and a name field (with preceding length byte).
*/

#define VARTYPEF_FUNC   UINT8_C(0X20)
#define VARTYPEF_ARRAY  UINT8_C(0X10)
#define VARTYPEM_BASE   UINT8_C(0X03)
#define VARTYPEM_INVAL  UINT8_C(0XCC)
#define VARTYPEV_FLOAT  UINT8_C(0X00)
#define VARTYPEV_INT    UINT8_C(0X01)
#define VARTYPEV_STR    UINT8_C(0X02)
#define VARTYPEV_LABEL  UINT8_C(0X03)

#define VAROFFS_NONE    UINT16_C(0XFFFF)
#define STROFFS_NONE    UINT16_C(0XFFFF)

#define VEXTRACT16( vmem, offs ) \
    ( ( ((uint16_t)( (vmem)->vars[ offs ] )) << UINT8_C(8) ) | \
    ( (vmem)->vars[ (offs) + 1U ] ) )

#define VWRITE16( vmem, offs, value ) \
    { \
        (vmem)->vars[ offs ] = (uint8_t)( (value) >> UINT8_C(8) ); \
        (vmem)->vars[ (offs) + 1U ] = (uint8_t) (value); \
    }

#define VTWRITE16( tmp, offs, value ) \
    { \
        (tmp)[ offs ] = (uint8_t)( (value) >> UINT8_C(8) ); \
        (tmp)[ (offs) + 1U ] = (uint8_t) (value); \
    }

typedef struct _usrparam_t {
    const char* paramname;
    uint8_t     paramtype;
} usrparam_t;

typedef union _varvalue_t {
    struct {
        uint16_t    stroffs;
        uint16_t    strsize;
    };
    double      dblval;
    int16_t     intval;
    uint16_t    lbloffs;
    uint16_t    codeoffs;
} varvalue_t;

#define VARSSIZE_MAX    65536U
#define STRSSIZE_MAX    65536U
#define MAXDIM          6U
#define MAXPARAM        8U
#define MAXVARS         1024U

typedef struct _varmem_t {
    void*           userdata;
    void            (*report)( struct _varmem_t*, void*, const char* );
    void            (*halt)( struct _varmem_t*, void* ) ATTR_NORETURN;
    uint8_t         vars[VARSSIZE_MAX];
    uint8_t         strs[STRSSIZE_MAX];
    uint32_t        varssize;
    uint32_t        strssize;
    uint16_t        numvars;
} varmem_t;

void init_varmem( varmem_t* vmem );

bool vmem_alloc_vars( varmem_t* vmem, uint16_t size, uint16_t* poffs );
bool vmem_lookup_var( varmem_t* vmem, uint8_t vartype, const char* name, uint16_t* poutoffs );
bool vmem_create_var( varmem_t* vmem, uint8_t vartype, const char* name, uint8_t numdims, const uint16_t* dims,
    uint8_t numparams, const usrparam_t* params, uint16_t* poutoffs );
bool vmem_delete_var( varmem_t* vmem, uint16_t varoffs );
bool vmem_get_var( varmem_t* vmem, uint16_t varoffs, uint8_t vartype, uint8_t numdims, const uint16_t* diminx,
    varvalue_t* pvalue );
bool vmem_set_var( varmem_t* vmem, uint16_t varoffs, uint8_t vartype, uint8_t numdims, const uint16_t* diminx,
    const varvalue_t* pvalue );

bool vmem_alloc_strs( varmem_t* vmem, uint16_t size, uint16_t* poffs );


#endif
