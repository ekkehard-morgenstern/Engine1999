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

#include "basic.h"

#define _CSI "\033["
#define _BLK "0"
#define _RED "1"
#define _GRN "2"
#define _YEL "3"
#define _BLU "4"
#define _MAG "5"
#define _CYA "6"
#define _WHI "7"
#define _FG "3"
#define _BG "4"
#define _SEP ";"
#define _SGR "m"
#define RED _CSI _FG _RED _SGR
#define GRN _CSI _FG _GRN _SGR
#define YEL _CSI _FG _YEL _SGR
#define BLU _CSI _FG _BLU _SGR
#define MAG _CSI _FG _MAG _SGR
#define CYA _CSI _FG _CYA _SGR
#define WHI _CSI _FG _WHI _SGR
#define NRM _CSI _SGR

static jmp_buf jmp_exit, jmp_loop;

static void rt_halt( struct _runtime_t* rt, void* usrdata ) ATTR_NORETURN;

static void rt_halt( struct _runtime_t* rt, void* usrdata ) {
    longjmp( jmp_exit, 1 );
}

static void rt_report( struct _runtime_t* rt, void* usrdata, const char* text ) {
    printf( RED "%s" NRM "\n", text );
}

static void comp_halt( struct _compiler_t* comp, void* usrdata ) ATTR_NORETURN;

static void comp_halt( struct _compiler_t* comp, void* usrdata ) {
    longjmp( jmp_loop, 1 );
}

static void comp_report( struct _compiler_t* comp, void* usrdata, const char* text ) {
    printf( MAG "%s" NRM "\n", text );
}

static void pgm_printer( const char* text ) {
    printf( YEL "%s" NRM "\n", text );
}

static runtime_t rt;
static compiler_t comp;

static void dumpdataline( const uint8_t* ptr, uint16_t offs, uint16_t cnt, int indent ) {
    // 00000000001111111111222222222233333333334444444444
    // 01234567890123456789012345678901234567890123456789
    // 0000:  00 00 00 00  00 00 00 00  .... ....
    static char buf[43];
    static const char hex[] = "0123456789ABCDEF";
    memset( buf, ' ', 42 ); buf[42] = '\0';
    buf[0] = hex[ ( offs >> UINT8_C(12) ) & 15U ];
    buf[1] = hex[ ( offs >> UINT8_C( 8) ) & 15U ];
    buf[2] = hex[ ( offs >> UINT8_C( 4) ) & 15U ];
    buf[3] = hex[ ( offs                ) & 15U ];
    buf[4] = ':';
    for ( uint16_t i=0; i < cnt; ++i ) {
        int pos1, pos2;
        if ( i < UINT16_C(4) ) {
            pos1 = 7 + i * 3;
            pos2 = 33 + i;
        } else {
            pos1 = 20 + ( i - UINT16_C(4) ) * 3;
            pos2 = 38 + ( i - UINT16_C(4) );
        }
        uint8_t b = ptr[offs+i];
        buf[pos1++] = hex[ ( b >> UINT8_C(4) ) & 15U ];
        buf[pos1  ] = hex[   b                 & 15U ];
        buf[pos2  ] = b >= UINT8_C(32) && b <= UINT8_C(126) ? b : UINT8_C(46);
    }
    printf( "%-*.*s" YEL "%s" NRM "\n", indent, indent, "", buf );
}

static void dumpdata( const uint8_t* ptr, uint16_t len, int indent ) {
    for ( uint16_t offs=0; offs < len; offs += UINT16_C(8) ) {
        uint16_t rem = len - offs;
        if ( rem > UINT8_C(8) ) {
            rem = UINT8_C(8);
        }
        dumpdataline( ptr, offs, rem, indent );
    }
}

typedef struct _printnodedata_t {
    int indent;
} printnodedata_t;

static void printnode( node_t* node, int indent );

static bool printbranch( void* userdata, node_t* node ) {
    printnodedata_t* data = (printnodedata_t*) userdata;
    printnode( node, data->indent );
    return true;
}

static void printnode( node_t* node, int indent ) {
    if ( node == NODE_NONE ) {
        return;
    }
    uint8_t  nodetype = node->type;
    uint16_t datalen  = node->datalen;
    printf( "%-*.*s" CYA "%s" NRM "\n", indent, indent, "", nodename( nodetype ) );
    if ( datalen ) {
        dumpdata( node->data, datalen, indent + 2 );
    }
    if ( node->numbranches ) {
        printnodedata_t printdata; printdata.indent = indent + 2;
        comp_node_iter_branches( &comp, node, &printdata, printbranch );
    }
}

static void printtree( void ) {
    node_t* rootnode = comp.syntree;
    if ( rootnode != NODE_NONE ) {
        printnode( rootnode, 0 );
    } else {
        printf( "no syntax tree\n" );
    }
}

static bool directmode( program_t* pgm, const uint8_t* tokens, size_t size ) {
    printf( "direct mode\n" );
    cleanup_compiler( &comp );
    init_compiler( &comp, &rt, 0, false );
    comp.iter.hdr.lineno = LINENO_NONE;
    comp_set_tokens( &comp, tokens, size );
    comp.halt   = comp_halt;
    comp.report = comp_report;
    run_compiler( &comp );
    printtree();
    return true;
}

static bool compile_program( program_t* pgm ) {
    printf( "program mode\n" );
    cleanup_compiler( &comp );
    init_compiler( &comp, &rt, 0, false );
    if ( !begin_iterate_program( &comp.iter, pgm ) ) {
        printf( "failed to init iterator\n" );
        return false;
    }
    comp_set_tokens( &comp, comp.iter.tok, comp.iter.hdr.length - LINEHDR_SIZE );
    comp.halt   = comp_halt;
    comp.report = comp_report;
    run_compiler( &comp );
    printtree();
    return true;
}

int main( int argc, char** argv ) {

    basdbg_flags = BASDBG_PARSE;        // this tool has always shown the compiler's token stream

    static program_t pgm;
    init_program( &pgm );

    static char*  buf = 0;
    static size_t bufsize = 0;
    static buf_t* tokbuf = 0;
    static buf_t* txtbuf = 0;
    tokbuf = create_buffer(); txtbuf = create_buffer();
    if ( tokbuf == 0 || txtbuf == 0 ) {
        return EXIT_FAILURE;
    }

    if ( setjmp( jmp_exit ) ) {
        printf( RED "*** HALTED" NRM "\n" );
        return EXIT_FAILURE;
    }

    init_runtime( &rt );
    rt.halt = rt_halt;
    rt.report = rt_report;

    for (;;) {

        if ( setjmp( jmp_loop ) ) {
            printf( MAG "*** ERROR" NRM "\n" );
        }

        ssize_t rd = getline( &buf, &bufsize, stdin );
        if ( rd < 0 ) {
            break;
        }
        size_t len = (size_t) rd;
        if ( len > 0 && buf[len-1U] == '\n' ) {    // remove LF
            buf[--len] = '\0';
        }
        preprocess_buffer( buf );
        printf( ">>%s<<\n", buf );
        linehdr_t hdr;
        clear_buffer( tokbuf );
        if ( !tokenize_line( buf, tokbuf, &hdr ) ) {
            printf( "? Syntax error\n" );
            continue;
        }

        printf( YEL "[" BLU "%u %u" YEL "]" NRM,
            (unsigned) hdr.lineno,
            (unsigned) hdr.length
        );

        const uint8_t* tokens = (const uint8_t*) tokbuf->buffer;
        len = hdr.length; const uint8_t* p = &tokens[LINEHDR_SIZE];
        const uint8_t* e = &tokens[len];
        while ( p < e ) {
            uint8_t b = *p++;
            uint8_t c = b >= UINT8_C(32) && b <= UINT8_C(126) ? b : UINT8_C(46);
            printf( " " YEL "[" GRN "%02X " MAG "%c" YEL "]" NRM, (int) b, (int) c );
        }

        printf( "\n" );
        clear_buffer( txtbuf ); p = &tokens[LINEHDR_SIZE];
        if ( !detokenize_line( txtbuf, p, &hdr ) ) {
            printf( "? Detok error\n" );
            continue;
        }
        printf( CYA "%s" NRM "\n", txtbuf->buffer );

        if ( !enter_line( &pgm, &tokens[0], directmode ) ) {
            printf( "? Enter failed\n" );
        }

        list_program( &pgm, LINENO_NONE, LINENO_NONE, pgm_printer );
        if ( !compile_program( &pgm ) ) {
            printf( "failed to compile program\n" );
        }
    }

    cleanup_compiler( &comp );
    cleanup_runtime( &rt );
    free_program( &pgm );
    free( buf );
    delete_buffer( tokbuf );
    delete_buffer( txtbuf );

    return 0;
}