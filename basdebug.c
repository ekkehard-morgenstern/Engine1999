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


#include "basdebug.h"

uint32_t basdbg_flags = UINT32_C(0);

// -- tokens --------------------------------------------------------------------

static void basdbg_describe_line( const uint8_t* tokline, linehdr_t* phdr ) {
    const uint8_t* p = tokline;
    clear_linehdr( phdr );
    read_linehdr_raw( &p, phdr );
}

void basdbg_print_tokens( const uint8_t* tokline ) {
    linehdr_t hdr;
    basdbg_describe_line( tokline, &hdr );
    if ( hdr.lineno == LINENO_NONE ) {
        printf( "[tokenize] direct mode, %u bytes:", (unsigned) hdr.length );
    } else {
        printf( "[tokenize] line %u, %u bytes:", (unsigned) hdr.lineno, (unsigned) hdr.length );
    }
    for ( uint32_t i=LINEHDR_SIZE; i < hdr.length; ++i ) {
        uint8_t b = tokline[i];
        printf( " %02X", (unsigned) b );
        if ( b >= UINT8_C(32) && b <= UINT8_C(126) ) {
            printf( "'%c'", (int) b );
        }
    }
    printf( "\n" );
    fflush( stdout );
}

void basdbg_print_detokenized( const uint8_t* tokline ) {
    linehdr_t hdr;
    basdbg_describe_line( tokline, &hdr );
    buf_t* buf = create_buffer();
    if ( buf == 0 ) {
        return;
    }
    if ( detokenize_line( buf, tokline + LINEHDR_SIZE, &hdr ) ) {
        printf( "[detokenize] %s\n", buf->buffer );
    } else {
        printf( "[detokenize] failed\n" );
    }
    fflush( stdout );
    delete_buffer( buf );
}

// -- syntax tree ---------------------------------------------------------------

static void basdbg_dump_data( const uint8_t* data, uint16_t len, int indent ) {
    // 0000:  00 00 00 00  00 00 00 00  .... ....
    for ( uint16_t offs=0; offs < len; offs += UINT16_C(8) ) {
        uint16_t cnt = len - offs;
        if ( cnt > UINT16_C(8) ) {
            cnt = UINT16_C(8);
        }
        printf( "[tree] %*s%04X: ", indent, "", (unsigned) offs );
        for ( uint16_t i=0; i < UINT16_C(8); ++i ) {
            if ( i == UINT16_C(4) ) {
                printf( " " );
            }
            if ( i < cnt ) {
                printf( " %02X", (unsigned) data[ offs + i ] );
            } else {
                printf( "   " );
            }
        }
        printf( "  " );
        for ( uint16_t i=0; i < cnt; ++i ) {
            uint8_t b = data[ offs + i ];
            putchar( b >= UINT8_C(32) && b <= UINT8_C(126) ? b : '.' );
        }
        printf( "\n" );
    }
}

typedef struct _basdbg_treedata_t {
    compiler_t* comp;
    int         indent;
} basdbg_treedata_t;

static void basdbg_print_node( compiler_t* comp, node_t* node, int indent );

static bool basdbg_print_branch( void* userdata, node_t* node ) {
    basdbg_treedata_t* data = (basdbg_treedata_t*) userdata;
    basdbg_print_node( data->comp, node, data->indent );
    return true;
}

static void basdbg_print_node( compiler_t* comp, node_t* node, int indent ) {
    if ( node == NODE_NONE ) {
        return;
    }
    printf( "[tree] %*s%s\n", indent, "", nodename( node->type ) );
    if ( node->datalen ) {
        basdbg_dump_data( node->data, node->datalen, indent + 2 );
    }
    if ( node->numbranches ) {
        basdbg_treedata_t data = { comp, indent + 2 };
        comp_node_iter_branches( comp, node, &data, basdbg_print_branch );
    }
}

void basdbg_print_tree( compiler_t* comp ) {
    if ( comp->syntree != NODE_NONE ) {
        basdbg_print_node( comp, comp->syntree, 0 );
    } else {
        printf( "[tree] no syntax tree\n" );
    }
    fflush( stdout );
}
