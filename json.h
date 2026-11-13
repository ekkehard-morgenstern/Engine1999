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

    ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
        applies. Programs written in the built-in programming language, and
        executables made from them with BUILD, do not fall under this license
        as the exception describes (see the file LICENSE-EXCEPTION).

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/


#ifndef JSON_H
#define JSON_H      1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
JSON trees (roadmap: renderer independent world, docs/json-subsystem.md; no SDL). A document is a tree of nodes: null, boolean, number,
string, array, object. Objects keep their keys in the order they were written. The BASIC statements JSON ... work on one current document
through paths: "a.b.c" is element c of element b of element a; array elements are addressed by their index, counting from 0: "rooms.3.name"
(also "rooms[3].name"). The empty path is the root. Keys that contain a dot or a bracket can't be addressed (they are kept when loading).
*/

#define JSON_MAXDEPTH   100             // nesting depth accepted by the parser

typedef enum { JSON_NULL = 0, JSON_BOOL, JSON_NUM, JSON_STR, JSON_ARR, JSON_OBJ } json_type_t;

typedef struct _json_node_t json_node_t;
struct _json_node_t {
    json_type_t     type;
    char*           key;                // the key in the parent object (heap; 0 elsewhere)
    double          num;                // JSON_NUM; JSON_BOOL: 0 or 1
    char*           str;                // JSON_STR: bytes (heap, always terminated), slen long
    size_t          slen;
    json_node_t**   kids;               // JSON_ARR / JSON_OBJ
    size_t          nkids, maxkids;
};

json_node_t* json_node_new( json_type_t type );
void json_node_free( json_node_t* n );                          // the node and everything below it
void json_node_reset( json_node_t* n, json_type_t type );       // empties n and gives it another type (keeps its key)
bool json_node_setstr( json_node_t* n, const char* s, size_t len );     // n becomes a string
bool json_node_add( json_node_t* parent, json_node_t* kid );    // append (an object kid must have a key); false: out of memory

// paths ("a.b.c", "rooms.3.name"); plen bytes of path
json_node_t* json_find( json_node_t* root, const char* path, size_t plen );          // 0 if there is no such element
// the element at path, made if it is missing (a missing intermediate element becomes an object, or an array when the next component is a number;
// an array index may be the number of elements = append). 0 if the path does not fit (a name used on an array ...) or memory is out.
// *proot is made when it is 0. The result is a null node when new. err (may be 0) gets a short message.
json_node_t* json_make( json_node_t** proot, const char* path, size_t plen, const char** err );
bool json_delete( json_node_t** proot, const char* path, size_t plen );              // removes the element (the root: clears); false if there is none

// text <-> tree
bool json_parse( const char* text, size_t len, json_node_t** out, char* err, size_t errsz );   // strict JSON (RFC 8259), \u escapes become UTF-8; err: "message at line n"
char* json_print( const json_node_t* n, bool pretty, size_t* plen );   // heap string (terminated; length in *plen); 0 if out of memory
// the value as BASIC text: a string as it is, a number as the shortest text that reads back, true / false, null as "", an array or object as JSON
char* json_valuetext( const json_node_t* n, size_t* plen );
const char* json_typename( const json_node_t* n );             // "null" "boolean" "number" "string" "array" "object"; "" for 0

#endif
