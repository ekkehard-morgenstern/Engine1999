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
#include "vmasm.h"
#include <strings.h>
#include <ctype.h>
#include "codegen.h"
#include "instset.h"

// -- decoding --------------------------------------------------------------------------

bool vmasm_decode( const uint8_t* area, uint32_t size, uint32_t offs, vminsn_t* out ) {
    memset( out, 0, sizeof(*out) );
    out->offs = offs;
    if ( offs >= size ) {
        return false;
    }
    uint8_t ins = area[ offs ];
    uint32_t need = 1U + ( ( ins & INSF_E ) ? 1U : 0U ) + ( ( ins & INSF_P ) ? ( ( ins & INSF_W ) ? 4U : 2U ) : 0U );
    out->size = need;
    out->valid = true;
    if ( size - offs < need ) {
        out->valid = false;
        out->size = size - offs;
    }
    out->code = ( ins & INSF_C ) != 0;
    out->wide = ( ins & INSF_W ) != 0 && ( ins & INSF_P ) != 0;
    out->hasparam = ( ins & INSF_P ) != 0;
    out->opcode = ins & INSM_I;
    uint32_t p = offs + 1U;
    if ( out->valid ) {
        if ( ins & INSF_E ) {
            out->opcode = (uint16_t)( ( out->opcode << 8 ) | area[ p++ ] );
        }
        if ( out->hasparam ) {
            if ( out->wide ) {
                out->param = ( (uint32_t) area[p] << 24 ) | ( (uint32_t) area[p+1U] << 16 ) | ( (uint32_t) area[p+2U] << 8 ) | area[p+3U];
            } else {
                out->param = ( (uint32_t) area[p] << 8 ) | area[p+1U];
            }
        }
    }
    out->name = cgen_opcode_name( out->opcode );
    uint32_t v = out->param;
    if ( !out->wide && out->code && out->hasparam ) {
        v |= UINT32_C(0x10000);     // the C bit is the 17th bit of PHIM, BRIA and BRIR
    }
    if ( out->opcode == INS_PHIM || out->opcode == INS_BRIR ) {
        if ( !out->wide && ( v & UINT32_C(0x10000) ) ) {
            v |= UINT32_C(0xFFFE0000);      // sign extension from 17 bits
        }
        out->value = (int32_t) v;
    } else {
        out->value = (int32_t) v;
    }
    return true;
}

static bool hasimplicitc( uint16_t opcode ) {
    return opcode == INS_JPCC || opcode == INS_JUMP || opcode == INS_RRLV || opcode == INS_WRLV;
}

void vmasm_format( const vminsn_t* insn, char* mnem, size_t mnemsz, char* oper, size_t opersz, char* comment, size_t commentsz ) {
    oper[0] = '\0'; comment[0] = '\0';
    if ( !insn->valid ) {
        snprintf( mnem, mnemsz, "(cut off)" );
        return;
    }
    const char* suffix = "";
    uint16_t op = insn->opcode;
    if ( cgen_has_cd( op ) ) {
        suffix = insn->code ? ".C" : ".D";
    } else if ( insn->code && !hasimplicitc( op ) && op != INS_PHIM && op != INS_BRIA && op != INS_BRIR ) {
        suffix = ".C";
    }
    snprintf( mnem, mnemsz, "%s%s", insn->name, suffix );
    if ( !insn->hasparam ) {
        return;
    }
    if ( op == INS_PHIM ) {
        snprintf( oper, opersz, "%d", (int) insn->value );
    } else if ( op == INS_BRIR ) {
        snprintf( oper, opersz, "%+d", (int) insn->value );
        snprintf( comment, commentsz, "-> $%04X", (unsigned)( insn->offs + insn->size + (uint32_t) insn->value ) );
    } else if ( op == INS_BRIA || op == INS_PHPA || op == INS_RSTR ) {
        uint32_t v = op == INS_BRIA ? (uint32_t) insn->value : insn->param;
        snprintf( oper, opersz, v > 0xFFFFU ? "$%08X" : "$%04X", (unsigned) v );
    } else {
        snprintf( oper, opersz, "%u", (unsigned) insn->param );
    }
}

// -- assembling ------------------------------------------------------------------------

bool vmasm_parse_number( const char* text, int64_t* pvalue ) {
    bool neg = false;
    if ( *text == '-' || *text == '+' ) {
        neg = *text == '-'; ++text;
    }
    int base = 10;
    if ( *text == '$' ) {
        base = 16; ++text;
    } else if ( text[0] == '0' && ( text[1] == 'x' || text[1] == 'X' ) ) {
        base = 16; text += 2;
    }
    if ( *text == '\0' ) {
        return false;
    }
    int64_t v = 0;
    for ( ; *text != '\0'; ++text ) {
        int d;
        char c = *text;
        if ( c >= '0' && c <= '9' ) {
            d = c - '0';
        } else if ( base == 16 && c >= 'a' && c <= 'f' ) {
            d = c - 'a' + 10;
        } else if ( base == 16 && c >= 'A' && c <= 'F' ) {
            d = c - 'A' + 10;
        } else {
            return false;
        }
        v = v * base + d;
        if ( v > INT64_C(0xFFFFFFFFFF) ) {
            return false;
        }
    }
    *pvalue = neg ? -v : v;
    return true;
}

static bool lookup( const char* name, uint16_t* popcode ) {
    for ( uint16_t op = 0; op < 0x400U; ++op ) {
        const char* n = cgen_opcode_name( op );
        if ( strcmp( n, "???" ) != 0 && strcasecmp( n, name ) == 0 ) {
            *popcode = op;
            return true;
        }
    }
    return false;
}

static bool needsoperand( uint16_t op ) {
    return op == INS_PHPA || op == INS_PHIM || op == INS_BRIA || op == INS_BRIR || op == INS_LINE;
}

bool vmasm_assemble( const char* line, uint8_t out[VMASM_MAXLEN], uint32_t* plen, char err[80] ) {
    char buf[128];
    size_t n = 0;
    for ( ; line[n] != '\0' && line[n] != ';' && n < sizeof(buf) - 1U; ++n ) {
        buf[n] = line[n];
    }
    while ( n > 0 && ( buf[n-1] == ' ' || buf[n-1] == '\t' ) ) {
        --n;
    }
    buf[n] = '\0';
    const char* p = buf;
    while ( *p == ' ' || *p == '\t' ) {
        ++p;
    }
    if ( *p == '\0' ) {
        snprintf( err, 80U, "Nothing to assemble" );
        return false;
    }
    char word[24]; size_t w = 0;
    while ( *p != '\0' && *p != ' ' && *p != '\t' && w < sizeof(word) - 1U ) {
        word[w++] = *p++;
    }
    word[w] = '\0';
    while ( *p == ' ' || *p == '\t' ) {
        ++p;
    }
    // the suffix
    char suffix = '\0';
    char* dot = strchr( word, '.' );
    if ( dot != 0 ) {
        char c = (char) toupper( (unsigned char) dot[1] );
        if ( dot[1] == '\0' || dot[2] != '\0' || ( c != 'C' && c != 'D' && c != 'W' ) ) {
            snprintf( err, 80U, "Bad suffix (use .C .D .W)" );
            return false;
        }
        suffix = c;
        *dot = '\0';
    }
    uint16_t op = 0;
    if ( !lookup( word, &op ) ) {
        snprintf( err, 80U, "Unknown instruction %s", word );
        return false;
    }
    bool hasoper = *p != '\0';
    int64_t value = 0;
    if ( hasoper ) {
        if ( !vmasm_parse_number( p, &value ) ) {
            snprintf( err, 80U, "Bad operand %.40s", p );
            return false;
        }
    } else if ( needsoperand( op ) ) {
        snprintf( err, 80U, "%s needs an operand", cgen_opcode_name( op ) );
        return false;
    }
    bool code = suffix == 'C' || hasimplicitc( op );
    bool wide = suffix == 'W';
    uint32_t param = 0;
    if ( hasoper ) {
        bool isbranch = op == INS_BRIA || op == INS_BRIR;
        bool issigned = op == INS_PHIM || op == INS_BRIR;
        if ( issigned ? ( value < INT64_C(-2147483648) || value > INT64_C(2147483647) ) : ( value < 0 || value > INT64_C(4294967295) ) ) {
            snprintf( err, 80U, "Operand out of range" );
            return false;
        }
        param = (uint32_t) value;
        if ( isbranch || ( op == INS_PHPA && code ) || op == INS_RSTR ) {
            wide = true;        // code addresses: wide, like the code generator
        }
        if ( op == INS_PHIM ) {
            if ( value >= -65536 && value <= 65535 && !wide ) {
                code = ( param & UINT32_C(0x10000) ) != 0;
                param &= 0xFFFFU;
            } else {
                wide = true; code = false;
            }
        } else if ( param > 0xFFFFU ) {
            wide = true;
        }
    }
    uint8_t ins = 0, ext = 0;
    if ( op < 16U ) {
        ins = (uint8_t) op;
    } else {
        ins = (uint8_t)( ( op >> 8 ) & INSM_I ) | INSF_E;
        ext = (uint8_t) op;
    }
    if ( hasoper ) {
        ins |= INSF_P;
        if ( wide ) {
            ins |= INSF_W;
        }
    }
    if ( code ) {
        ins |= INSF_C;
    }
    uint32_t len = 0;
    out[len++] = ins;
    if ( ins & INSF_E ) {
        out[len++] = ext;
    }
    if ( ins & INSF_P ) {
        if ( ins & INSF_W ) {
            out[len++] = (uint8_t)( param >> 24 );
            out[len++] = (uint8_t)( param >> 16 );
        }
        out[len++] = (uint8_t)( param >> 8 );
        out[len++] = (uint8_t) param;
    }
    *plen = len;
    return true;
}
