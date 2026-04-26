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

#include "baslin.h"
#include "baspgm.h"
#include "bastok.h"

// -- linenode_t ------------------------------------------------------------

void init_linenode( linenode_t* node ) {
    node->nextoffs = LINEOFFS_NONE;
    node->prevoffs = LINEOFFS_NONE;
}

void emit_linenode_raw( uint8_t** pp, const linenode_t* source ) {
    uint8_t* p = *pp;
    emit_uint16( &p, source->nextoffs );
    emit_uint16( &p, source->prevoffs );
    *pp = p;
}

bool emit_linenode( struct _program_t* pgm, uint16_t offs, const linenode_t* source ) {
    if ( offs == LINEOFFS_NONE || offs >= pgm->fillpos ) {
        return false;
    }
    uint8_t* p = &pgm->memory[ offs ];
    emit_linenode_raw( &p, source );
    return true;
}

void read_linenode_raw( const uint8_t** pp, linenode_t* target ) {
    const uint8_t* p = *pp;
    read_uint16( &p, &target->nextoffs );
    read_uint16( &p, &target->prevoffs );
    *pp = p;
}

bool read_linenode( struct _program_t* pgm, uint16_t offs, linenode_t* target ) {
    if ( offs == LINEOFFS_NONE || offs >= pgm->fillpos ) {
        return false;
    }
    const uint8_t* p = &pgm->memory[ offs ];
    read_linenode_raw( &p, target );
    return true;
}

bool remove_linenode( struct _program_t* pgm, uint16_t pos, linenode_t* outnode ) {
    // don't remove already removed node, and also don't remove head or tail nodes
    linenode_t node = LINENODE_INIT;
    if ( !read_linenode( pgm, pos, &node ) ) {
        return false;
    }
    uint16_t prevpos = node.prevoffs, nextpos = node.nextoffs;
    linenode_t prev = LINENODE_INIT, next = LINENODE_INIT;
    if ( !read_linenode( pgm, prevpos, &prev ) || !read_linenode( pgm, nextpos, &next ) ) {
        return false;
    }
    prev.nextoffs = node.nextoffs;
    next.prevoffs = node.prevoffs;
    node.nextoffs = LINEOFFS_NONE;
    node.prevoffs = LINEOFFS_NONE;
    if ( !emit_linenode( pgm, prevpos, &prev ) || !emit_linenode( pgm, nextpos, &next ) ) {
        return false;
    }
    if ( !emit_linenode( pgm, pos, &node ) ) {
        return false;
    }
    if ( outnode ) {
        *outnode = node;
    }
    return true;
}

bool emplace_linenode( struct _program_t* pgm, uint16_t prevpos, uint16_t pos, uint16_t nextpos, linenode_t* outnode ) {
    linenode_t node = LINENODE_INIT;
    if ( !read_linenode( pgm, pos, &node ) ) {
        return false;
    }
    linenode_t prev = LINENODE_INIT, next = LINENODE_INIT;
    if ( !read_linenode( pgm, prevpos, &prev ) || !read_linenode( pgm, nextpos, &next ) ) {
        return false;
    }
    prev.nextoffs = pos;
    node.prevoffs = prevpos;
    node.nextoffs = nextpos;
    next.prevoffs = pos;
    if ( !emit_linenode( pgm, prevpos, &prev ) || !emit_linenode( pgm, nextpos, &next ) ) {
        return false;
    }
    if ( !emit_linenode( pgm, pos, &node ) ) {
        return false;
    }
    if ( outnode ) {
        *outnode = node;
    }
    return true;
}

// -- linelist_t ------------------------------------------------------------

void init_linelist( linelist_t* list, uint16_t pos ) {
    list->head.nextoffs = pos + sizeof(linenode_t);
    list->head.prevoffs = LINEOFFS_NONE;
    list->tail.nextoffs = LINEOFFS_NONE;
    list->tail.prevoffs = pos;
}

bool emit_linelist( struct _program_t* pgm, uint16_t offs, const linelist_t* source ) {
    if ( offs == LINEOFFS_NONE || offs >= pgm->fillpos ) {
        return false;
    }
    if ( !emit_linenode( pgm, offs, &source->head ) ) {
        return false;
    }
    if ( !emit_linenode( pgm, offs + sizeof(linenode_t), &source->tail ) ) {
        return false;
    }
    return true;
}

bool read_linelist( struct _program_t* pgm, uint16_t offs, linelist_t* target ) {
    if ( offs == LINEOFFS_NONE || offs >= pgm->fillpos ) {
        return false;
    }
    if ( !read_linenode( pgm, offs, &target->head ) ) {
        return false;
    }
    if ( !read_linenode( pgm, offs + sizeof(linenode_t), &target->tail ) ) {
        return false;
    }
    return true;
}

bool linelist_empty( struct _program_t* pgm, uint16_t offs, bool* pempty ) {
    // NOTE return value indicates success/error not state
    linelist_t list = LINELIST_INIT;
    if ( !read_linelist( pgm, offs, &list ) ) {
        return false;
    }
    if ( list.head.nextoffs == offs + sizeof(linenode_t) ) {    // head points to tail => list empty
        *pempty = true;
    } else {
        *pempty = false;
    }
    return true;
}

bool linelist_firstnode( struct _program_t* pgm, uint16_t listpos, uint16_t* pnodepos ) {
    linelist_t list = LINELIST_INIT;
    if ( !read_linelist( pgm, listpos, &list ) ) {
        return false;
    }
    if ( list.head.nextoffs == listpos + sizeof(linenode_t) ) {    // head points to tail => list empty
        *pnodepos = LINEOFFS_NONE;
    } else {
        *pnodepos = list.head.nextoffs;
    }
    return true;
}

bool linelist_lastnode( struct _program_t* pgm, uint16_t listpos, uint16_t* pnodepos, bool wanthead ) {
    linelist_t list = LINELIST_INIT;
    if ( !read_linelist( pgm, listpos, &list ) ) {
        return false;
    }
    if ( wanthead ) {   // gimme the head node
        *pnodepos = list.tail.prevoffs;
    } else if ( list.tail.prevoffs == listpos ) {    // tail points to head => list empty
        *pnodepos = LINEOFFS_NONE;
    } else {
        *pnodepos = list.head.prevoffs;
    }
    return true;
}

bool linelist_nextnode( struct _program_t* pgm, uint16_t nodepos, uint16_t* pnextpos, linenode_t* outnode ) {
    linenode_t node = LINENODE_INIT;
    if ( !read_linenode( pgm, nodepos, &node ) ) {
        return false;
    }
    uint16_t nextpos = node.nextoffs;
    if ( nextpos == LINEOFFS_NONE ) {   // tail node??
        return false;
    }
    linenode_t next = LINENODE_INIT;
    if ( !read_linenode( pgm, nextpos, &next ) ) {
        return false;
    }
    if ( next.nextoffs == LINEOFFS_NONE ) { // next node is the tail node
        *pnextpos = LINEOFFS_NONE;
    } else {
        *pnextpos = nextpos;
    }
    if ( outnode ) {
        *outnode = node;
    }
    return true;
}

bool linelist_prevnode( struct _program_t* pgm, uint16_t nodepos, uint16_t* pprevpos, linenode_t* outnode ) {
    linenode_t node = LINENODE_INIT;
    if ( !read_linenode( pgm, nodepos, &node ) ) {
        return false;
    }
    uint16_t prevpos = node.prevoffs;
    if ( prevpos == LINEOFFS_NONE ) {   // head node??
        return false;
    }
    linenode_t prev = LINENODE_INIT;
    if ( !read_linenode( pgm, prevpos, &prev ) ) {
        return false;
    }
    if ( prev.prevoffs == LINEOFFS_NONE ) { // previous node is the head node
        *pprevpos = LINEOFFS_NONE;
    } else {
        *pprevpos = prevpos;
    }
    if ( outnode ) {
        *outnode = node;
    }
    return true;
}

bool linelist_addtail( struct _program_t* pgm, uint16_t listoffs, uint16_t nodeoffs, linenode_t* outnode ) {
    uint16_t lastpos = LINEOFFS_NONE;
    if ( !linelist_lastnode( pgm, listoffs, &lastpos, true ) ) {
        return false;
    }
    // the previous position is the last node in the list, or the head node of the list
    uint16_t prevpos = lastpos;
    // the next position is always the tail node of the list
    uint16_t nextpos = listoffs + sizeof(linenode_t);
    // emplace the current node between these two nodes
    return emplace_linenode( pgm, prevpos, nodeoffs, nextpos, outnode );
}

// -- linehdr_t -------------------------------------------------------------

void clear_linehdr( linehdr_t* hdr ) {
    init_linenode( &hdr->node );
    hdr->lineno = LINENO_NONE;
    hdr->length = UINT16_C(0);
    hdr->alloc  = UINT16_C(0);
}

void emit_linehdr_raw( uint8_t** pp, const linehdr_t* source ) {
    emit_linenode_raw( pp, &source->node );
    emit_uint16( pp, source->lineno );
    emit_uint16( pp, source->length );
    emit_uint16( pp, source->alloc  );
}

bool emit_linehdr( struct _program_t* pgm, uint16_t pos, const linehdr_t* source, uint8_t** outp ) {
    if ( pos == LINEOFFS_NONE || pos < sizeof(linelist_t) || pos >= pgm->fillpos ) {
        return false;
    }
    uint8_t* p = &pgm->memory[ pos ];
    emit_linehdr_raw( &p, source );
    if ( outp ) {
        *outp = p;
    }
    return true;
}

void read_linehdr_raw( const uint8_t** pp, linehdr_t* target ) {
    read_linenode_raw( pp, &target->node );
    read_uint16( pp, &target->lineno );
    read_uint16( pp, &target->length );
    read_uint16( pp, &target->alloc  );
}

bool read_linehdr( struct _program_t* pgm, uint16_t pos, linehdr_t* target, const uint8_t** outp ) {
    if ( pos == LINEOFFS_NONE || pos < sizeof(linelist_t) || pos >= pgm->fillpos ) {
        return false;
    }
    const uint8_t* p = &pgm->memory[ pos ];
    read_linehdr_raw( &p, target );
    if ( outp ) {
        *outp = p;
    }
    return true;
}
