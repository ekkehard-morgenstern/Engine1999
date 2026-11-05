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



#include "keyseq.h"

// the final byte or tilde number of each key; tilde == 0: ESC [ 1 ; m <final> (or ESC O <final>), else ESC [ <tilde> ; m ~
typedef struct _keyseq_def_t {
    const char* name;
    char        final;
    int         tilde;
} keyseq_def_t;

static const keyseq_def_t defs[KS_COUNT] = {
    { "",         0,   0 },
    { "Up",       'A', 0 }, { "Down",     'B', 0 }, { "Right",    'C', 0 }, { "Left",     'D', 0 },
    { "Home",     'H', 0 }, { "End",      'F', 0 },
    { "Insert",   0,   2 }, { "Delete",   0,   3 }, { "PageUp",   0,   5 }, { "PageDown", 0,   6 },
    { "F1",       'P', 0 }, { "F2",       'Q', 0 }, { "F3",       'R', 0 }, { "F4",       'S', 0 },
    { "F5",       0,  15 }, { "F6",       0,  17 }, { "F7",       0,  18 }, { "F8",       0,  19 },
    { "F9",       0,  20 }, { "F10",      0,  21 }, { "F11",      0,  23 }, { "F12",      0,  24 },
    { "BackTab",  'Z', 0 },
};

const char* keyseq_keyname( int key ) {
    return key > KS_NONE && key < KS_COUNT ? defs[key].name : "";
}

void keyseq_name( int key, int mods, char* buf ) {
    buf[0] = '\0';
    if ( key <= KS_NONE || key >= KS_COUNT ) {
        return;
    }
    size_t n = 0;
    static const struct { int bit; const char* name; } m[] = {
        { KSMOD_CTRL, "Ctrl+" }, { KSMOD_ALT, "Alt+" }, { KSMOD_SHIFT, "Shift+" }, { KSMOD_META, "Meta+" } };
    for ( size_t i = 0; i < sizeof(m) / sizeof(m[0]); ++i ) {
        if ( ( mods & m[i].bit ) != 0 ) {
            n += (size_t) snprintf( buf + n, KEYSEQ_NAMELEN - n, "%s", m[i].name );
        }
    }
    snprintf( buf + n, KEYSEQ_NAMELEN - n, "%s", defs[key].name );
}

size_t keyseq_make( int key, int mods, char* buf ) {
    buf[0] = '\0';
    if ( key <= KS_NONE || key >= KS_COUNT || mods < 0 || mods > KSMOD_ALL ) {
        return 0;
    }
    const keyseq_def_t* d = &defs[key];
    int n;
    if ( d->tilde != 0 ) {
        n = mods ? snprintf( buf, KEYSEQ_MAXLEN, "\033[%d;%d~", d->tilde, 1 + mods ) : snprintf( buf, KEYSEQ_MAXLEN, "\033[%d~", d->tilde );
    } else if ( mods ) {
        n = snprintf( buf, KEYSEQ_MAXLEN, "\033[1;%d%c", 1 + mods, d->final );
    } else if ( key >= KS_F1 && key <= KS_F4 ) {
        n = snprintf( buf, KEYSEQ_MAXLEN, "\033O%c", d->final );
    } else {
        n = snprintf( buf, KEYSEQ_MAXLEN, "\033[%c", d->final );
    }
    return n > 0 ? (size_t) n : 0;
}

// the key of a final byte (ESC [ 1 ; m <final> / ESC O <final>); KS_NONE if there is none
static int by_final( char c ) {
    for ( int k = KS_UP; k < KS_COUNT; ++k ) {
        if ( defs[k].tilde == 0 && defs[k].final == c ) {
            return k;
        }
    }
    return KS_NONE;
}

// the key of a number in ESC [ n ~
static int by_tilde( int n ) {
    switch ( n ) {
        case 1: case 7:     return KS_HOME;
        case 4: case 8:     return KS_END;
        case 11:            return KS_F1;
        case 12:            return KS_F2;
        case 13:            return KS_F3;
        case 14:            return KS_F4;
        default:
            break;
    }
    for ( int k = KS_UP; k < KS_COUNT; ++k ) {
        if ( defs[k].tilde == n ) {
            return k;
        }
    }
    return KS_NONE;
}

static bool mods_of( int p, int* mods ) {
    if ( p < 1 || p > 16 ) {
        return false;
    }
    *mods = p - 1;
    return true;
}

int keyseq_parse( const char* s, size_t len, int* key, int* mods ) {
    *key = KS_NONE; *mods = 0;
    if ( len == 0 || s[0] != 033 ) {
        return 0;
    }
    if ( len == 1 ) {
        return -1;
    }
    if ( s[1] == 'O' ) {                            // SS3: ESC O [m] <final>
        size_t i = 2;
        int p = 1;
        if ( i < len && s[i] >= '1' && s[i] <= '9' ) {
            p = s[i++] - '0';
            while ( i < len && s[i] >= '0' && s[i] <= '9' && p < 100 ) {
                p = p * 10 + ( s[i++] - '0' );
            }
        }
        if ( i >= len ) {
            return -1;
        }
        int k = by_final( s[i] );
        int m;
        if ( k == KS_NONE || !mods_of( p, &m ) ) {
            return 0;
        }
        *key = k; *mods = m;
        return (int) i + 1;
    }
    if ( s[1] != '[' ) {
        return 0;
    }
    if ( len == 2 ) {
        return -1;
    }
    if ( s[2] == '[' ) {                            // the linux console: ESC [ [ A..E = F1..F5
        if ( len == 3 ) {
            return -1;
        }
        if ( s[3] >= 'A' && s[3] <= 'E' ) {
            *key = KS_F1 + ( s[3] - 'A' );
            return 4;
        }
        return 0;
    }
    int p[2] = { 0, 0 };
    int np = 0;
    bool havenum = false;
    size_t i = 2;
    for ( ; i < len; ++i ) {
        char c = s[i];
        if ( c >= '0' && c <= '9' ) {
            if ( np > 1 || p[np] > 1000 ) {
                return 0;
            }
            p[np] = p[np] * 10 + ( c - '0' );
            havenum = true;
        } else if ( c == ';' ) {
            if ( np > 0 ) {
                return 0;
            }
            ++np;
            havenum = false;
        } else {
            break;
        }
    }
    if ( i >= len ) {
        return -1;
    }
    char fin = s[i];
    int m = 0, k;
    if ( np == 1 && havenum && !mods_of( p[1], &m ) ) {
        return 0;
    }
    if ( fin == '~' ) {
        k = by_tilde( p[0] );
    } else if ( np == 0 && havenum ) {
        return 0;                                   // ESC [ n <final> without modifiers: not a key sequence
    } else {
        if ( havenum == false && np == 1 ) {
            return 0;                               // ESC [ 1 ; <final>: the modifier is missing
        }
        k = by_final( fin );
        if ( p[0] > 1 ) {
            return 0;
        }
    }
    if ( k == KS_NONE ) {
        return 0;
    }
    *key = k; *mods = m;
    return (int) i + 1;
}

void keyseq_hex( const char* s, size_t len, char* out, size_t cap ) {
    size_t n = 0;
    if ( cap == 0 ) {
        return;
    }
    for ( size_t i = 0; i < len && n + 4U < cap; ++i ) {
        n += (size_t) snprintf( out + n, cap - n, i ? " %02x" : "%02x", (unsigned) (unsigned char) s[i] );
    }
    out[n] = '\0';
}

void keyseq_escape( const char* s, size_t len, char* out, size_t cap ) {
    size_t n = 0;
    if ( cap == 0 ) {
        return;
    }
    for ( size_t i = 0; i < len && n + 5U < cap; ++i ) {
        unsigned char c = (unsigned char) s[i];
        switch ( c ) {
            case 033:   n += (size_t) snprintf( out + n, cap - n, "\\e" );   break;
            case '\n':  n += (size_t) snprintf( out + n, cap - n, "\\n" );   break;
            case '\r':  n += (size_t) snprintf( out + n, cap - n, "\\r" );   break;
            case '\t':  n += (size_t) snprintf( out + n, cap - n, "\\t" );   break;
            case '\\': n += (size_t) snprintf( out + n, cap - n, "\\\\" );  break;
            case '"':   n += (size_t) snprintf( out + n, cap - n, "\\\"" );  break;
            default:
                if ( c >= 0x20 && c < 0x7F ) {
                    out[n++] = (char) c;
                } else {
                    n += (size_t) snprintf( out + n, cap - n, "\\x%02x", (unsigned) c );
                }
                break;
        }
    }
    out[n] = '\0';
}

// -- the character set ---------------------------------------------------------------------

static const struct { uint32_t cp; int code; } latin9_diff[] = {
    { 0x20AC, 0xA4 }, { 0x0160, 0xA6 }, { 0x0161, 0xA8 }, { 0x017D, 0xB4 }, { 0x017E, 0xB8 }, { 0x0152, 0xBC }, { 0x0153, 0xBD }, { 0x0178, 0xBE } };

int keyseq_latin9( uint32_t cp ) {
    if ( cp < 0x80 ) {
        return (int) cp;
    }
    for ( size_t i = 0; i < sizeof(latin9_diff) / sizeof(latin9_diff[0]); ++i ) {
        if ( latin9_diff[i].cp == cp ) {
            return latin9_diff[i].code;
        }
        if ( cp == latin9_diff[i].code ) {
            return -1;                  // ISO-8859-1 has this character at the place where ISO-8859-15 has another one
        }
    }
    return cp >= 0xA0 && cp <= 0xFF ? (int) cp : -1;
}

uint32_t keyseq_unicode( int code ) {
    if ( code < 0x80 ) {
        return (uint32_t) code;
    }
    if ( code < 0xA0 ) {
        return 0xFFFD;
    }
    for ( size_t i = 0; i < sizeof(latin9_diff) / sizeof(latin9_diff[0]); ++i ) {
        if ( latin9_diff[i].code == code ) {
            return latin9_diff[i].cp;
        }
    }
    return (uint32_t) code;
}

size_t keyseq_utf8_to_latin9( const char* in, size_t len, char* out ) {
    size_t n = 0;
    const unsigned char* s = (const unsigned char*) in;
    size_t pos = 0;
    while ( pos < len ) {
        unsigned char c = s[pos];
        uint32_t cp;
        size_t need;
        if ( c < 0x80 )         { cp = c;        need = 1U; }
        else if ( c >= 0xF0 )   { cp = c & 7U;   need = 4U; }
        else if ( c >= 0xE0 )   { cp = c & 15U;  need = 3U; }
        else if ( c >= 0xC0 )   { cp = c & 31U;  need = 2U; }
        else                    { ++pos; continue; }        // a stray continuation byte
        size_t i = 1U;
        for ( ; i < need && pos + i < len && ( s[pos + i] & 0xC0 ) == 0x80; ++i ) {
            cp = ( cp << 6 ) | ( s[pos + i] & 63U );
        }
        pos += i;
        int code = i == need ? keyseq_latin9( cp ) : -1;
        if ( code >= 0 ) {
            out[n++] = (char) code;
        }
    }
    return n;
}

size_t keyseq_latin9_to_utf8( const char* in, size_t len, char* out ) {
    size_t n = 0;
    for ( size_t i = 0; i < len; ++i ) {
        uint32_t cp = keyseq_unicode( (unsigned char) in[i] );
        if ( cp < 0x80 ) {
            out[n++] = (char) cp;
        } else if ( cp < 0x800 ) {
            out[n++] = (char) ( 0xC0 | ( cp >> 6 ) );
            out[n++] = (char) ( 0x80 | ( cp & 63U ) );
        } else {
            out[n++] = (char) ( 0xE0 | ( cp >> 12 ) );
            out[n++] = (char) ( 0x80 | ( ( cp >> 6 ) & 63U ) );
            out[n++] = (char) ( 0x80 | ( cp & 63U ) );
        }
    }
    return n;
}

// -- program listings -----------------------------------------------------------------------------------------------------------------------

// decodes one UTF-8 character at s[pos]; returns its length (0 = malformed) and sets *cp
static size_t listing_decode( const unsigned char* s, size_t len, size_t pos, uint32_t* cp ) {
    unsigned char c = s[pos];
    size_t need; uint32_t v, min;
    if ( c < 0x80 )         { *cp = c; return 1U; }
    else if ( c >= 0xF8 )   { return 0; }
    else if ( c >= 0xF0 )   { v = c & 7U;  need = 4U; min = 0x10000U; }
    else if ( c >= 0xE0 )   { v = c & 15U; need = 3U; min = 0x800U; }
    else if ( c >= 0xC0 )   { v = c & 31U; need = 2U; min = 0x80U; }
    else                    { return 0; }
    if ( pos + need > len ) {
        return 0;
    }
    for ( size_t i = 1U; i < need; ++i ) {
        if ( ( s[pos + i] & 0xC0 ) != 0x80 ) {
            return 0;
        }
        v = ( v << 6 ) | ( s[pos + i] & 63U );
    }
    if ( v < min || v > 0x10FFFFU || ( v >= 0xD800U && v <= 0xDFFFU ) ) {
        return 0;
    }
    *cp = v;
    return need;
}

bool keyseq_listing_is_utf8( const char* in, size_t len ) {
    const unsigned char* s = (const unsigned char*) in;
    bool high = false;
    for ( size_t pos = 0; pos < len; ) {
        uint32_t cp = 0;
        size_t n = listing_decode( s, len, pos, &cp );
        if ( n == 0 ) {
            return false;
        }
        if ( n > 1U ) {
            high = true;
        }
        pos += n;
    }
    return high;
}

size_t keyseq_listing_from_utf8( const char* in, size_t len, char* out, size_t* lost ) {
    const unsigned char* s = (const unsigned char*) in;
    size_t n = 0;
    for ( size_t pos = 0; pos < len; ) {
        uint32_t cp = 0;
        size_t k = listing_decode( s, len, pos, &cp );
        if ( k == 0 ) {                         // not UTF-8 after all: keep the byte
            out[n++] = (char) s[pos++];
            continue;
        }
        pos += k;
        int code = cp >= 0x80 && cp < 0xA0 ? (int) cp : keyseq_latin9( cp );
        if ( code < 0 ) {
            code = '?';
            if ( lost != 0 ) {
                ++*lost;
            }
        }
        out[n++] = (char) code;
    }
    return n;
}

size_t keyseq_listing_to_utf8( const char* in, size_t len, char* out ) {
    size_t n = 0;
    for ( size_t i = 0; i < len; ++i ) {
        int code = (unsigned char) in[i];
        uint32_t cp = code >= 0x80 && code < 0xA0 ? (uint32_t) code : keyseq_unicode( code );
        if ( cp < 0x80 ) {
            out[n++] = (char) cp;
        } else if ( cp < 0x800 ) {
            out[n++] = (char) ( 0xC0 | ( cp >> 6 ) );
            out[n++] = (char) ( 0x80 | ( cp & 63U ) );
        } else {
            out[n++] = (char) ( 0xE0 | ( cp >> 12 ) );
            out[n++] = (char) ( 0x80 | ( ( cp >> 6 ) & 63U ) );
            out[n++] = (char) ( 0x80 | ( cp & 63U ) );
        }
    }
    return n;
}
