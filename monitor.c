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
#include "monitor.h"
#include "sdlscreen.h"
#include "textscreen.h"
#include "vmasm.h"
#include "codegen.h"
#include <strings.h>

// -- layout and colors -----------------------------------------------------------------

#define MON_SCREEN      2
#define MON_OUT_Y       1               // the output area: rows 1..23 (row 0: title bar, row 24: help bar)
#define MON_OUT_H       23
#define MON_BAR_Y       ( MON_OUT_Y + MON_OUT_H )

// palette entries (colinittab in sdllayer.c)
#define C_BLACK         1
#define C_DIMGRAY       35
#define C_GRAY          47
#define C_DARKGRAY      63
#define C_SILVER        74
#define C_AQUA          15
#define C_LIME          13
#define C_GOLD          123
#define C_YELLOW        137
#define C_RED           110
#define C_ORANGE        120
#define C_WHITE         140
#define C_DARKSLATEGRAY 22
#define C_DARKSLATEBLUE 28

#define C_BG            C_BLACK
#define C_TEXT          C_SILVER
#define C_ADDR          C_AQUA
#define C_HEX           C_WHITE
#define C_HEXZERO       C_GRAY
#define C_ASCII         C_GOLD
#define C_MNEM          C_YELLOW
#define C_OPER          C_LIME
#define C_COMMENT       C_GRAY
#define C_ERR           C_RED
#define C_PROMPT        C_AQUA
#define C_KEY           C_YELLOW
#define C_TITLE_BG      C_DARKSLATEBLUE
#define C_TITLE_FG      C_WHITE
#define C_HELP_BG       C_DARKSLATEGRAY

typedef enum _region_t { R_CODE, R_DATA, R_DSTACK, R_RSTACK, R_COUNT } region_t;

static const char* const regionnames[R_COUNT] = { "code", "data", "dstack", "rstack" };

typedef struct _mon_t {
    repl_t*     repl;
    void        (*vblank)( void* );
    void*       usrdata;
    region_t    region;
    uint32_t    memaddr[R_COUNT];       // where the next dump of the region continues
    uint32_t    disaddr;                // where the next disassembly / assembly continues
    bool        quit;
} mon_t;

static bool mon_inited = false;
static mon_t mon;

// -- output ----------------------------------------------------------------------------

static void out( int fg, const char* fmt, ... ) {
    char buf[512];
    va_list ap;
    va_start( ap, fmt );
    vsnprintf( buf, sizeof(buf), fmt, ap );
    va_end( ap );
    sdlscr_printf( -1, -1, -1, fg, "%s", buf );
}

static void outerr( const char* fmt, ... ) {
    char buf[200];
    va_list ap;
    va_start( ap, fmt );
    vsnprintf( buf, sizeof(buf), fmt, ap );
    va_end( ap );
    sdlscr_printf( -1, -1, -1, C_ERR, "%s\n", buf );
}

static void newline_unless_at_start( void ) {
    int x = 0, y = 0;
    sdlscr_gettextcursor( &x, &y );
    if ( x != 0 ) {
        out( C_TEXT, "\n" );
    }
}

// -- the panels ------------------------------------------------------------------------

static const codegen_t* mon_cgen( const mon_t* m ) {
    return &m->repl->rt.codegen;
}

static void mon_setout( int x, int y ) {
    txtscr_setviewport( 0, MON_OUT_Y, TXTSCR_WIDTH, MON_OUT_H );
    txtscr_setblank( C_BG, C_TEXT );        // what scrolling fills with (the bars changed it)
    txtscr_locate( x, y );
}

// title bar: the region and the sizes; the output cursor is kept
static void mon_title( const mon_t* m ) {
    int cx = 0, cy = 0;
    txtscr_getcursor( &cx, &cy );
    txtscr_setviewport( 0, 0, TXTSCR_WIDTH, 1 );
    txtscr_setblank( C_TITLE_BG, C_TITLE_FG );
    txtscr_clear();
    const codegen_t* cg = mon_cgen( m );
    char buf[100];
    txtscr_write( 0, 1, C_TITLE_BG, C_TITLE_FG, "Machine Monitor", -1 );
    snprintf( buf, sizeof(buf), "%s", regionnames[m->region] );
    int x = 19;
    x += txtscr_write( 0, x, C_TITLE_BG, C_GRAY, "region ", -1 );
    x += txtscr_write( 0, x, C_TITLE_BG, C_YELLOW, buf, -1 );
    snprintf( buf, sizeof(buf), "   code $%X  data $%X  dstack %u  rstack %u", (unsigned) cg->codesize, (unsigned) cg->datasize,
        (unsigned) m->repl->vm.dsp, (unsigned) m->repl->vm.rsp );
    txtscr_write( 0, x, C_TITLE_BG, C_TITLE_FG, buf, -1 );
    mon_setout( cx, cy );
    sdlscr_textchanged();
}

static void mon_helpbar( void ) {
    txtscr_setviewport( 0, MON_BAR_Y, TXTSCR_WIDTH, 1 );
    txtscr_setblank( C_HELP_BG, C_TEXT );
    txtscr_clear();
    static const struct { const char* key; const char* what; } help[] = {
        { "?", "help" }, { "M", "dump" }, { "U", "unasm" }, { "A", "asm" }, { "E", "edit" }, { "F", "fill" }, { "R", "region" },
        { "L", "line" }, { "C", "compile" }, { "G", "go" }, { "Q", "quit" },
    };
    int x = 1;
    for ( size_t i=0; i < sizeof(help) / sizeof(help[0]); ++i ) {
        x += txtscr_write( 0, x, C_HELP_BG, C_KEY, help[i].key, -1 );
        x += txtscr_write( 0, x, C_HELP_BG, C_WHITE, help[i].what, -1 );
        x += 2;
    }
}

// -- arguments -------------------------------------------------------------------------

// hexadecimal by default; $ or 0x prefix: hexadecimal, # prefix: decimal
static bool mon_number( const char* text, uint32_t* pvalue ) {
    int base = 16;
    if ( *text == '#' ) {
        base = 10; ++text;
    } else if ( *text == '$' ) {
        ++text;
    } else if ( text[0] == '0' && ( text[1] == 'x' || text[1] == 'X' ) ) {
        text += 2;
    }
    if ( *text == '\0' ) {
        return false;
    }
    uint64_t v = 0;
    for ( ; *text != '\0'; ++text ) {
        int d;
        char c = *text;
        if ( c >= '0' && c <= '9' ) {
            d = c - '0';
        } else if ( c >= 'a' && c <= 'f' ) {
            d = c - 'a' + 10;
        } else if ( c >= 'A' && c <= 'F' ) {
            d = c - 'A' + 10;
        } else {
            return false;
        }
        if ( d >= base ) {
            return false;
        }
        v = v * (uint64_t) base + (uint64_t) d;
        if ( v > UINT32_MAX ) {
            return false;
        }
    }
    *pvalue = (uint32_t) v;
    return true;
}

// splits at blanks and commas (in place)
static int mon_split( char* s, char* argv[], int max ) {
    int n = 0;
    while ( *s != '\0' && n < max ) {
        while ( *s == ' ' || *s == '\t' || *s == ',' ) {
            ++s;
        }
        if ( *s == '\0' ) {
            break;
        }
        argv[n++] = s;
        while ( *s != '\0' && *s != ' ' && *s != '\t' && *s != ',' ) {
            ++s;
        }
        if ( *s != '\0' ) {
            *s++ = '\0';
        }
    }
    return n;
}

static bool mon_arg_number( const char* text, uint32_t* pvalue ) {
    if ( !mon_number( text, pvalue ) ) {
        outerr( "Bad number: %s", text );
        return false;
    }
    return true;
}

// -- regions ---------------------------------------------------------------------------

// the bytes of a byte region (code, data); 0 for the stacks
static uint8_t* mon_bytes( const mon_t* m, region_t r, uint32_t* psize ) {
    const codegen_t* cg = mon_cgen( m );
    if ( r == R_CODE ) {
        *psize = cg->codesize;
        return cg->code;
    }
    if ( r == R_DATA ) {
        *psize = cg->datasize;
        return cg->data;
    }
    *psize = 0;
    return 0;
}

static int addrwidth( uint32_t size ) {
    return size > 0xFFFFFFU ? 8 : size > 0xFFFFU ? 6 : 4;
}

static void mon_out_addr( uint32_t addr, int width ) {
    out( C_ADDR, "%0*X", width, (unsigned) addr );
    out( C_GRAY, ": " );
}

// -- commands --------------------------------------------------------------------------

static void cmd_help( void ) {
    static const struct { const char* key; const char* what; } help[] = {
        { "?",            "this help" },
        { "R [region]",   "region to look at: code data dstack rstack (c d ds rs); none: list" },
        { "M [s [e]]",    "dump code/data, or list a stack; default: continue the last dump" },
        { "U [s [e]]",    "unassemble code (D is the same)" },
        { "A [s]",        "assemble at s, one instruction per line, an empty line ends" },
        { "E s b ...",    "enter bytes at s (code or data)" },
        { "F s e b",      "fill s..e with the byte b" },
        { "L n",          "find BASIC line n in the code" },
        { "C",            "compile the stored program again (discards patches)" },
        { "G [s]",        "run the code from s (default 0) as it is; ESC breaks" },
        { "Q",            "leave the monitor" },
    };
    for ( size_t i=0; i < sizeof(help) / sizeof(help[0]); ++i ) {
        out( C_KEY, "%-12s", help[i].key );
        out( C_TEXT, "%s\n", help[i].what );
    }
    out( C_GRAY, "Numbers are hex ($1F, 0x1F, 1F), #31 is decimal; assembler operands are decimal unless $ or 0x.\n" );
    out( C_GRAY, "Patches to the code are kept for G only: RUN compiles the program again.\n" );
}

static bool parse_region( const char* name, region_t* pr ) {
    static const struct { const char* name; region_t r; } names[] = {
        { "c", R_CODE }, { "code", R_CODE }, { "d", R_DATA }, { "data", R_DATA }, { "ds", R_DSTACK }, { "dstack", R_DSTACK },
        { "rs", R_RSTACK }, { "rstack", R_RSTACK },
    };
    for ( size_t i=0; i < sizeof(names) / sizeof(names[0]); ++i ) {
        if ( strcasecmp( name, names[i].name ) == 0 ) {
            *pr = names[i].r;
            return true;
        }
    }
    return false;
}

static void cmd_region( mon_t* m, int argc, char** argv ) {
    if ( argc > 0 ) {
        region_t r;
        if ( !parse_region( argv[0], &r ) ) {
            outerr( "Unknown region %s (code, data, dstack, rstack)", argv[0] );
            return;
        }
        m->region = r;
    }
    const codegen_t* cg = mon_cgen( m );
    for ( int r=0; r < R_COUNT; ++r ) {
        out( r == (int) m->region ? C_YELLOW : C_GRAY, "%c ", r == (int) m->region ? '*' : ' ' );
        out( r == (int) m->region ? C_YELLOW : C_TEXT, "%-8s", regionnames[r] );
        switch ( r ) {
            case R_CODE:   out( C_TEXT, "%u bytes ($%X)\n", (unsigned) cg->codesize, (unsigned) cg->codesize ); break;
            case R_DATA:   out( C_TEXT, "%u bytes ($%X)\n", (unsigned) cg->datasize, (unsigned) cg->datasize ); break;
            case R_DSTACK: out( C_TEXT, "%u items\n", (unsigned) m->repl->vm.dsp ); break;
            default:       out( C_TEXT, "%u items\n", (unsigned) m->repl->vm.rsp ); break;
        }
    }
}

static void dump_row( const uint8_t* bytes, uint32_t addr, uint32_t count, int width ) {
    mon_out_addr( addr, width );
    for ( uint32_t i=0; i < 16U; ++i ) {
        if ( i < count ) {
            out( bytes[i] == 0 ? C_HEXZERO : C_HEX, "%02X ", bytes[i] );
        } else {
            out( C_TEXT, "   " );
        }
        if ( i == 7U ) {
            out( C_TEXT, " " );
        }
    }
    out( C_TEXT, " " );
    char text[17];
    for ( uint32_t i=0; i < count; ++i ) {
        text[i] = bytes[i] >= 32 && bytes[i] < 127 ? (char) bytes[i] : '.';
    }
    text[count] = '\0';
    out( C_ASCII, "%s\n", text );
}

static void list_stack( mon_t* m, bool data, uint32_t first, uint32_t count ) {
    const vm_t* vm = &m->repl->vm;
    size_t n = data ? vm->dsp : vm->rsp;
    if ( n == 0 ) {
        out( C_GRAY, "(empty)\n" );
        return;
    }
    for ( uint32_t i = first; i < n && count > 0; ++i, --count ) {      // index 0 is the top
        size_t at = n - 1U - i;
        out( C_ADDR, "#%-3u ", (unsigned) i );
        if ( data ) {
            const vmval_t* v = &vm->dstack[at];
            if ( v->isstr ) {
                out( C_TEXT, "string  " );
                out( C_ASCII, "\"%.60s\"\n", v->str ? v->str : "" );
            } else {
                out( C_TEXT, "number  " );
                out( C_OPER, "%.15g\n", v->num );
            }
        } else {
            out( C_HEX, "$%08X\n", (unsigned) vm->rstack[at] );
        }
    }
}

static void cmd_dump( mon_t* m, int argc, char** argv ) {
    uint32_t size = 0;
    uint8_t* bytes = mon_bytes( m, m->region, &size );
    uint32_t start = m->memaddr[m->region], end = 0;
    bool hasend = false;
    if ( argc > 0 && !mon_arg_number( argv[0], &start ) ) {
        return;
    }
    if ( argc > 1 ) {
        if ( !mon_arg_number( argv[1], &end ) ) {
            return;
        }
        hasend = true;
    }
    if ( bytes == 0 ) {          // a stack: start = the item index from the top, e = the last one
        uint32_t first = argc > 0 ? start : 0;
        list_stack( m, m->region == R_DSTACK, first, hasend ? ( end >= first ? end - first + 1U : 1U ) : 16U );
        return;
    }
    if ( size == 0 ) {
        out( C_GRAY, "(the %s segment is empty)\n", regionnames[m->region] );
        return;
    }
    if ( start >= size ) {
        outerr( "Address $%X is outside of the %s segment ($%X bytes)", (unsigned) start, regionnames[m->region], (unsigned) size );
        return;
    }
    if ( !hasend || end < start ) {
        end = start + 127U;
    }
    if ( end >= size ) {
        end = size - 1U;
    }
    int width = addrwidth( size );
    uint32_t a = start;
    while ( a <= end ) {
        uint32_t n = end - a + 1U;
        if ( n > 16U ) {
            n = 16U;
        }
        dump_row( bytes + a, a, n, width );
        a += n;
        if ( a == 0 ) {
            break;
        }
    }
    m->memaddr[m->region] = a;
}

static void unasm_line( const uint8_t* code, uint32_t size, uint32_t offs, int width, uint32_t* pnext ) {
    vminsn_t in;
    vmasm_decode( code, size, offs, &in );
    char mnem[24], oper[40], comment[40];
    vmasm_format( &in, mnem, sizeof(mnem), oper, sizeof(oper), comment, sizeof(comment) );
    mon_out_addr( offs, width );
    for ( uint32_t i=0; i < VMASM_MAXLEN; ++i ) {
        if ( i < in.size ) {
            out( code[ offs + i ] == 0 ? C_HEXZERO : C_HEX, "%02X ", code[ offs + i ] );
        } else {
            out( C_TEXT, "   " );
        }
    }
    bool unknown = strcmp( in.name, "???" ) == 0 || !in.valid;
    out( unknown ? C_ERR : C_MNEM, "%-9s", mnem );
    out( C_OPER, "%-12s", oper );
    if ( comment[0] != '\0' ) {
        out( C_COMMENT, "; %s", comment );
    }
    out( C_TEXT, "\n" );
    *pnext = offs + in.size;
}

static bool need_code( const mon_t* m ) {
    if ( m->region != R_CODE ) {
        outerr( "Only for the code region (R C)" );
        return false;
    }
    return true;
}

static void cmd_unasm( mon_t* m, int argc, char** argv ) {
    if ( !need_code( m ) ) {
        return;
    }
    const codegen_t* cg = mon_cgen( m );
    uint32_t start = m->disaddr, end = 0;
    bool hasend = false;
    if ( argc > 0 && !mon_arg_number( argv[0], &start ) ) {
        return;
    }
    if ( argc > 1 ) {
        if ( !mon_arg_number( argv[1], &end ) ) {
            return;
        }
        hasend = true;
    }
    if ( cg->codesize == 0 ) {
        out( C_GRAY, "(the code segment is empty)\n" );
        return;
    }
    if ( start >= cg->codesize ) {
        outerr( "Address $%X is outside of the code segment ($%X bytes)", (unsigned) start, (unsigned) cg->codesize );
        return;
    }
    int width = addrwidth( cg->codesize );
    uint32_t a = start;
    for ( int lines = 0; a < cg->codesize && ( hasend ? a <= end : lines < 16 ); ++lines ) {
        unasm_line( cg->code, cg->codesize, a, width, &a );
    }
    m->disaddr = a;
}

// writes bytes into the code segment (inside it, or appending at its end) or the data segment (inside it)
static bool write_bytes( mon_t* m, region_t r, uint32_t addr, const uint8_t* bytes, uint32_t len ) {
    codegen_t* cg = &m->repl->rt.codegen;
    uint32_t size = r == R_CODE ? cg->codesize : cg->datasize;
    bool fits = addr <= size && len <= size - addr;
    if ( !fits && !( r == R_CODE && addr == size ) ) {
        if ( addr >= size ) {
            outerr( "Address $%X is outside of the %s segment ($%X bytes)", (unsigned) addr, regionnames[r], (unsigned) size );
        } else {
            outerr( "The bytes would run past the end of the %s segment ($%X)", regionnames[r], (unsigned) size );
        }
        return false;
    }
    if ( !fits ) {      // appending at the end of the code
        uint32_t offs = 0;
        if ( !cgen_alloc_code( cg, len, &offs ) || offs != addr ) {
            outerr( "Out of code memory" );
            return false;
        }
    }
    memcpy( ( r == R_CODE ? cg->code : cg->data ) + addr, bytes, len );
    return true;
}

static void cmd_asm( mon_t* m, int argc, char** argv ) {
    if ( !need_code( m ) ) {
        return;
    }
    uint32_t addr = m->disaddr;
    if ( argc > 0 && !mon_arg_number( argv[0], &addr ) ) {
        return;
    }
    out( C_GRAY, "Assembling at $%X; an empty line ends.\n", (unsigned) addr );
    for ( ;; ) {
        int width = addrwidth( mon_cgen( m )->codesize );
        out( C_ADDR, "%0*X", width, (unsigned) addr );
        out( C_PROMPT, "  " );
        char line[100]; line[0] = '\0';
        int r = sdlscr_lineedit( line, sizeof(line), m->vblank, m->usrdata );
        if ( r == SDLSCR_EDIT_CLOSED ) {
            m->quit = true;
            return;
        }
        const char* p = line;
        while ( *p == ' ' ) {
            ++p;
        }
        if ( r != SDLSCR_EDIT_ACCEPTED || *p == '\0' ) {
            break;
        }
        uint8_t bytes[VMASM_MAXLEN]; uint32_t len = 0; char err[80];
        if ( !vmasm_assemble( line, bytes, &len, err ) ) {
            outerr( "%s", err );
            continue;
        }
        if ( !write_bytes( m, R_CODE, addr, bytes, len ) ) {
            continue;
        }
        addr += len;
    }
    m->disaddr = addr;
}

static void cmd_enter( mon_t* m, int argc, char** argv ) {
    if ( m->region != R_CODE && m->region != R_DATA ) {
        outerr( "Only for the code and data regions" );
        return;
    }
    uint32_t addr = 0;
    if ( argc < 2 ) {
        outerr( "Usage: E address byte ..." );
        return;
    }
    if ( !mon_arg_number( argv[0], &addr ) ) {
        return;
    }
    uint8_t bytes[16];
    if ( argc - 1 > (int) sizeof(bytes) ) {
        outerr( "At most %u bytes at a time", (unsigned) sizeof(bytes) );
        return;
    }
    for ( int i=1; i < argc; ++i ) {
        uint32_t v = 0;
        if ( !mon_arg_number( argv[i], &v ) ) {
            return;
        }
        if ( v > 255U ) {
            outerr( "Not a byte: %s", argv[i] );
            return;
        }
        bytes[i-1] = (uint8_t) v;
    }
    write_bytes( m, m->region, addr, bytes, (uint32_t)( argc - 1 ) );
}

static void cmd_fill( mon_t* m, int argc, char** argv ) {
    if ( m->region != R_CODE && m->region != R_DATA ) {
        outerr( "Only for the code and data regions" );
        return;
    }
    uint32_t start = 0, end = 0, v = 0, size = 0;
    uint8_t* bytes = mon_bytes( m, m->region, &size );
    if ( argc < 3 ) {
        outerr( "Usage: F start end byte" );
        return;
    }
    if ( !mon_arg_number( argv[0], &start ) || !mon_arg_number( argv[1], &end ) || !mon_arg_number( argv[2], &v ) ) {
        return;
    }
    if ( v > 255U ) {
        outerr( "Not a byte: %s", argv[2] );
        return;
    }
    if ( start > end || end >= size ) {
        outerr( "Range $%X..$%X is outside of the %s segment ($%X bytes)", (unsigned) start, (unsigned) end, regionnames[m->region], (unsigned) size );
        return;
    }
    memset( bytes + start, (int) v, (size_t)( end - start ) + 1U );
}

static void cmd_line( mon_t* m, int argc, char** argv ) {
    if ( argc < 1 ) {
        outerr( "Usage: L line" );
        return;
    }
    char* endp = 0;
    unsigned long n = strtoul( argv[0], &endp, 10 );
    uint32_t addr = 0;
    if ( *endp != '\0' || !cgen_find_line( mon_cgen( m ), (uint32_t) n, &addr ) ) {
        outerr( "Line %s is not in the code", argv[0] );
        return;
    }
    out( C_TEXT, "line %lu is at ", n );
    out( C_ADDR, "$%X\n", (unsigned) addr );
    m->disaddr = addr;
    m->memaddr[R_CODE] = addr;
}

static void cmd_compile( mon_t* m ) {
    if ( repl_compile( m->repl ) ) {
        const codegen_t* cg = mon_cgen( m );
        out( C_TEXT, "Compiled: code $%X bytes, data $%X bytes\n", (unsigned) cg->codesize, (unsigned) cg->datasize );
    }
    m->disaddr = 0;
    m->memaddr[R_CODE] = 0; m->memaddr[R_DATA] = 0;
}

static void cmd_go( mon_t* m, int argc, char** argv ) {
    uint32_t addr = 0;
    if ( argc > 0 && !mon_arg_number( argv[0], &addr ) ) {
        return;
    }
    out( C_GRAY, "Running from $%X (ESC breaks)\n", (unsigned) addr );
    sdlscr_pollkeys( true );
    repl_go( m->repl, addr );
    sdlscr_pollkeys( false );
    newline_unless_at_start();
    if ( m->repl->fulledit || m->repl->monitor ) {      // the program ended in EDIT / MONITOR: not carried out from here
        m->repl->fulledit = false; m->repl->monitor = false;
        out( C_GRAY, "(the program ended in EDIT or MONITOR)\n" );
    }
}

static void execute( mon_t* m, char* line ) {
    char* p = line;
    while ( *p == ' ' || *p == '\t' ) {
        ++p;
    }
    if ( *p == '\0' ) {
        return;
    }
    char cmd = (char) tolower( (unsigned char) *p++ );
    char* argv[8];
    int argc = mon_split( p, argv, 8 );
    switch ( cmd ) {
        case '?': case 'h': cmd_help(); break;
        case 'r': cmd_region( m, argc, argv ); break;
        case 'm': cmd_dump( m, argc, argv ); break;
        case 'u': case 'd': cmd_unasm( m, argc, argv ); break;
        case 'a': cmd_asm( m, argc, argv ); break;
        case 'e': cmd_enter( m, argc, argv ); break;
        case 'f': cmd_fill( m, argc, argv ); break;
        case 'l': cmd_line( m, argc, argv ); break;
        case 'c': cmd_compile( m ); break;
        case 'g': cmd_go( m, argc, argv ); break;
        case 'q': case 'x': m->quit = true; break;
        default:
            outerr( "Unknown command %c (? for help)", cmd );
            break;
    }
}

// -- the monitor -----------------------------------------------------------------------

static void mon_firstuse( mon_t* m ) {
    memset( m, 0, sizeof(*m) );
    txtscr_setcolors( C_BG, C_TEXT );
    txtscr_setblank( C_BG, C_TEXT );
    txtscr_resetviewport();
    txtscr_clear();
    mon_helpbar();
    mon_setout( 0, 0 );
    txtscr_clear();
    mon_inited = true;
}

void monitor_run( repl_t* repl, void (*vblank_handler)( void* ), void* usrdata ) {
    int oldscreen = txtscr_selected();
    txtscr_select( MON_SCREEN );
    if ( !mon_inited ) {
        mon_firstuse( &mon );
        out( C_TITLE_FG, "Machine monitor" );
        out( C_GRAY, " - ? for help, Q to leave\n" );
    }
    mon.repl = repl; mon.vblank = vblank_handler; mon.usrdata = usrdata;
    mon.quit = false;
    if ( mon_cgen( &mon )->codesize == 0 && repl->pgm.first != 0 ) {
        out( C_GRAY, "Compiling the program...\n" );
        cmd_compile( &mon );
    }
    sdlscr_textchanged();
    while ( !mon.quit ) {
        mon_title( &mon );
        out( C_PROMPT, "%s> ", regionnames[mon.region] );
        char line[200]; line[0] = '\0';
        int r = sdlscr_lineedit( line, sizeof(line), vblank_handler, usrdata );
        if ( r == SDLSCR_EDIT_CLOSED ) {
            break;
        }
        if ( r == SDLSCR_EDIT_ACCEPTED ) {
            execute( &mon, line );
        }
    }
    txtscr_select( oldscreen );
    sdlscr_textchanged();
}
