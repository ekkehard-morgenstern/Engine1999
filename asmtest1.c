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
// Automated test of the VM assembler / decoder (vmasm.c): exit status = number of failures.

#include "vmasm.h"
#include "basrepl.h"
#include "codegen.h"
#include "instset.h"

static int total = 0, failures = 0;

static void check( bool cond, const char* name ) {
    ++total;
    if ( !cond ) {
        printf( "FAIL  %s\n", name );
        ++failures;
    }
}

// assembles text and compares the bytes with the expected ones
static void asmcase( const char* text, const char* expected ) {
    uint8_t out[VMASM_MAXLEN]; uint32_t len = 0; char err[80];
    char hex[3 * VMASM_MAXLEN + 1]; hex[0] = '\0';
    bool ok = vmasm_assemble( text, out, &len, err );
    for ( uint32_t i=0; ok && i < len; ++i ) {
        char b[4]; snprintf( b, sizeof(b), "%s%02X", i ? " " : "", out[i] );
        strcat( hex, b );
    }
    char name[200]; snprintf( name, sizeof(name), "assemble \"%s\" = %s (got %s)", text, expected, ok ? hex : err );
    check( ok && strcmp( hex, expected ) == 0, name );
}

static void errcase( const char* text, const char* part ) {
    uint8_t out[VMASM_MAXLEN]; uint32_t len = 0; char err[80];
    bool ok = vmasm_assemble( text, out, &len, err );
    char name[200]; snprintf( name, sizeof(name), "\"%s\" is refused (%s; got %s)", text, part, ok ? "accepted" : err );
    check( !ok && strstr( err, part ) != 0, name );
}

static void test_encodings( void ) {
    asmcase( "NOP", "01" );
    asmcase( "nop", "01" );
    asmcase( "  BRK  ; a comment", "00" );
    asmcase( "PHIM 5", "83 00 05" );
    asmcase( "PHIM -1", "C3 FF FF" );
    asmcase( "PHIM 65535", "83 FF FF" );
    asmcase( "PHIM 65536", "93 00 01 00 00" );
    asmcase( "PHIM -65536", "C3 00 00" );
    asmcase( "PHIM -65537", "93 FF FE FF FF" );
    asmcase( "PHIM $10", "83 00 10" );
    asmcase( "PHIM 0x10", "83 00 10" );
    asmcase( "PHIM.W 5", "93 00 00 00 05" );
    asmcase( "BRIA $1234", "94 00 00 12 34" );
    asmcase( "BRIR -6", "95 FF FF FF FA" );
    asmcase( "BRIR +6", "95 00 00 00 06" );
    asmcase( "JPCC", "46" );
    asmcase( "JUMP", "47" );
    asmcase( "ADD", "20 14" );
    asmcase( "con", "20 1F" );
    asmcase( "PHPA.D 10", "82 00 0A" );
    asmcase( "PHPA.C 10", "D2 00 00 00 0A" );
    asmcase( "PHPA.D 70000", "92 00 01 11 70" );
    asmcase( "DROP.D 3", "88 00 03" );
    asmcase( "DROP.C", "48" );
    asmcase( "DROP", "08" );
    asmcase( "LINE 100", "89 00 64" );
    asmcase( "LINE 100000", "99 00 01 86 A0" );
    asmcase( "LAN.D", "0A" );
    asmcase( "LAN.C", "4A" );
    asmcase( "RRNV 5", "A1 00 00 05" );
    asmcase( "RRLV 5", "E1 03 00 05" );
    asmcase( "SND 2", "A1 42 00 02" );
    asmcase( "snd 6", "A1 42 00 06" );
    asmcase( "PLNG", "21 43" );
}

static void test_errors( void ) {
    errcase( "", "Nothing" );
    errcase( "; just a comment", "Nothing" );
    errcase( "FROB", "Unknown instruction" );
    errcase( "PHIM", "needs an operand" );
    errcase( "BRIA", "needs an operand" );
    errcase( "PHIM abc", "Bad operand" );
    errcase( "PHIM $", "Bad operand" );
    errcase( "PHPA.X 1", "Bad suffix" );
    errcase( "BRIA -1", "out of range" );
    errcase( "PHIM 3000000000", "out of range" );
    errcase( "LINE 99999999999", "out of range" );
}

static void test_decode( void ) {
    static const uint8_t code[] = { 0x83, 0x00, 0x05, 0xC3, 0xFF, 0xFF, 0x20, 0x14, 0x95, 0x00, 0x00, 0x00, 0x04, 0x82, 0x00 };
    vminsn_t in; char m[24], o[40], c[40];
    check( vmasm_decode( code, sizeof(code), 0, &in ) && in.valid && in.size == 3 && in.opcode == INS_PHIM && in.value == 5, "decode PHIM 5" );
    vmasm_format( &in, m, sizeof(m), o, sizeof(o), c, sizeof(c) );
    check( strcmp( m, "PHIM" ) == 0 && strcmp( o, "5" ) == 0 && c[0] == '\0', "format PHIM 5" );
    check( vmasm_decode( code, sizeof(code), 3, &in ) && in.value == -1, "decode PHIM -1 (C bit as 17th bit)" );
    check( vmasm_decode( code, sizeof(code), 6, &in ) && in.size == 2 && in.opcode == INS_ADD && strcmp( in.name, "ADD" ) == 0, "decode ADD" );
    check( vmasm_decode( code, sizeof(code), 8, &in ) && in.size == 5 && in.opcode == INS_BRIR && in.value == 4, "decode BRIR" );
    vmasm_format( &in, m, sizeof(m), o, sizeof(o), c, sizeof(c) );
    check( strcmp( o, "+4" ) == 0 && strcmp( c, "-> $0011" ) == 0, "format BRIR shows the target" );
    check( vmasm_decode( code, sizeof(code), 13, &in ) && !in.valid && in.size == 2, "a cut off instruction is not valid" );
    check( !vmasm_decode( code, sizeof(code), 15, &in ), "nothing behind the end" );
    int64_t v = 0;
    check( vmasm_parse_number( "$ff", &v ) && v == 255 && vmasm_parse_number( "-12", &v ) && v == -12 && !vmasm_parse_number( "1x", &v ), "numbers" );
}

// every instruction of a compiled program must survive decode -> format -> assemble with the same bytes
static void test_roundtrip( void ) {
    static const char* const program[] = {
        "10 DIM A(5), B$(3)",
        "20 FOR I=1 TO 5:A(I)=I*2.5:NEXT I",
        "30 S$=\"HI\"+CHR$(65):PRINT S$;LEN(S$);-7;70000;4000000000",
        "40 GOSUB 100",
        "50 DEF FN F(X)=X*X+1",
        "60 PRINT FN F(3)",
        "70 DATA 1,2,\"x\"",
        "80 READ B:READ T$:RESTORE 70",
        "90 IF B>0 THEN PRINT \"POS\" ELSE PRINT \"NEG\"",
        "92 ON B GOTO 90,100",
        "94 WHILE B<3:B=B+1:WEND:REPEAT:B=B-1:UNTIL B<1",
        "95 LABEL L:END",
        "96 SOUND 1,440,1:SOUND 2,0,2,0.25:SOUND OFF:SOUND OFF 3:SOUND SYNC 1,2:WAVE 1,3:TEMPO 90:PLAY \"CDE\",\"R\":PRINT PLAYING(1)",
        "100 PRINT LEFT$(\"ABC\",2);MID$(\"ABC\",2,1);HEX$(255,4):RETURN",
        0 };
    repl_t repl;
    check( init_repl( &repl ), "init_repl" );
    for ( int i=0; program[i]; ++i ) {
        repl_enter( &repl, program[i] );
    }
    check( repl_compile( &repl ), "the test program compiles" );
    const codegen_t* cg = &repl.rt.codegen;
    uint32_t offs = 0, count = 0, bad = 0, unknown = 0;
    while ( offs < cg->codesize ) {
        vminsn_t in;
        if ( !vmasm_decode( cg->code, cg->codesize, offs, &in ) || !in.valid ) {
            ++bad;
            break;
        }
        char m[24], o[40], c[40], text[100], err[80];
        vmasm_format( &in, m, sizeof(m), o, sizeof(o), c, sizeof(c) );
        snprintf( text, sizeof(text), "%s %s", m, o );
        uint8_t out[VMASM_MAXLEN]; uint32_t len = 0;
        if ( strcmp( in.name, "???" ) == 0 ) {
            ++unknown;
        } else if ( !vmasm_assemble( text, out, &len, err ) || len != in.size || memcmp( out, cg->code + offs, len ) != 0 ) {
            printf( "  roundtrip differs at $%04X: %s (%s)\n", (unsigned) offs, text, err );
            ++bad;
        }
        offs += in.size;
        ++count;
    }
    check( bad == 0, "disassembled instructions assemble to the same bytes" );
    check( unknown == 0, "no unknown instructions in compiled code" );
    check( offs == cg->codesize && count > 100, "the decoder walks the whole code segment" );
    cleanup_repl( &repl );
}

int main( void ) {
    test_encodings();
    test_errors();
    test_decode();
    test_roundtrip();
    printf( "%d of %d checks passed\n", total - failures, total );
    return failures;
}
