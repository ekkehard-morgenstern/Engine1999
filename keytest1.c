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


#include "stdtypes.h"
#include "keyseq.h"
#include "sdlkeys.h"

/*
Automated test of the key sequences (keyseq.c, sdlkeys.c): the canonical sequences, the dialects the parser knows, prefixes, garbage, and the
mapping of SDL key codes. No display needed. Exit status = number of failed checks.
*/

static int failures = 0, total = 0;

static void check( bool cond, const char* name ) {
    ++total;
    if ( !cond ) {
        printf( "FAIL  %s\n", name );
        ++failures;
    }
}

// parses text completely: the key and modifiers, or -1 if it is not one complete sequence
static bool parses( const char* text, int key, int mods ) {
    int k, m;
    size_t len = strlen( text );
    return keyseq_parse( text, len, &k, &m ) == (int) len && k == key && m == mods;
}

static void test_make( void ) {
    char b[KEYSEQ_MAXLEN];
    static const struct { int key; int mods; const char* seq; } t[] = {
        { KS_UP, 0, "\033[A" }, { KS_DOWN, 0, "\033[B" }, { KS_RIGHT, 0, "\033[C" }, { KS_LEFT, 0, "\033[D" },
        { KS_HOME, 0, "\033[H" }, { KS_END, 0, "\033[F" }, { KS_INSERT, 0, "\033[2~" }, { KS_DELETE, 0, "\033[3~" },
        { KS_PGUP, 0, "\033[5~" }, { KS_PGDN, 0, "\033[6~" },
        { KS_F1, 0, "\033OP" }, { KS_F2, 0, "\033OQ" }, { KS_F3, 0, "\033OR" }, { KS_F4, 0, "\033OS" },
        { KS_F5, 0, "\033[15~" }, { KS_F6, 0, "\033[17~" }, { KS_F7, 0, "\033[18~" }, { KS_F8, 0, "\033[19~" },
        { KS_F9, 0, "\033[20~" }, { KS_F10, 0, "\033[21~" }, { KS_F11, 0, "\033[23~" }, { KS_F12, 0, "\033[24~" },
        { KS_UP, KSMOD_SHIFT, "\033[1;2A" }, { KS_LEFT, KSMOD_CTRL, "\033[1;5D" }, { KS_RIGHT, KSMOD_ALT, "\033[1;3C" },
        { KS_DOWN, KSMOD_SHIFT | KSMOD_CTRL, "\033[1;6B" }, { KS_HOME, KSMOD_META, "\033[1;9H" },
        { KS_END, KSMOD_ALL, "\033[1;16F" }, { KS_F1, KSMOD_SHIFT, "\033[1;2P" }, { KS_F4, KSMOD_CTRL, "\033[1;5S" },
        { KS_DELETE, KSMOD_CTRL, "\033[3;5~" }, { KS_PGUP, KSMOD_SHIFT, "\033[5;2~" }, { KS_F5, KSMOD_ALT, "\033[15;3~" },
        { KS_F12, KSMOD_SHIFT | KSMOD_ALT | KSMOD_CTRL | KSMOD_META, "\033[24;16~" },
    };
    bool ok = true;
    for ( size_t i = 0; i < sizeof(t) / sizeof(t[0]); ++i ) {
        size_t n = keyseq_make( t[i].key, t[i].mods, b );
        if ( n != strlen( t[i].seq ) || strcmp( b, t[i].seq ) != 0 ) {
            printf( "FAIL  keyseq_make( %s, %d ) gave '%s', expected '%s'\n", keyseq_keyname( t[i].key ), t[i].mods, b, t[i].seq );
            ok = false;
        }
    }
    ++total;
    if ( !ok ) {
        ++failures;
    }
    check( keyseq_make( KS_NONE, 0, b ) == 0 && keyseq_make( KS_COUNT, 0, b ) == 0 && keyseq_make( KS_UP, 16, b ) == 0 && keyseq_make( KS_UP, -1, b ) == 0,
        "no sequence for an invalid key or modifiers" );
}

static void test_roundtrip( void ) {
    char b[KEYSEQ_MAXLEN];
    int bad = 0;
    for ( int key = KS_UP; key < KS_COUNT; ++key ) {
        for ( int mods = 0; mods <= KSMOD_ALL; ++mods ) {
            size_t n = keyseq_make( key, mods, b );
            int k, m;
            if ( n == 0 || n >= KEYSEQ_MAXLEN || keyseq_parse( b, n, &k, &m ) != (int) n || k != key || m != mods ) {
                ++bad;
            }
            // every proper prefix is "incomplete", never a key
            for ( size_t cut = 1U; cut < n; ++cut ) {
                if ( keyseq_parse( b, cut, &k, &m ) != -1 ) {
                    ++bad;
                }
            }
        }
    }
    check( bad == 0, "make -> parse round trip of all keys with all modifiers, and every prefix is incomplete" );
}

static void test_dialects( void ) {
    check( parses( "\033OA", KS_UP, 0 ) && parses( "\033OB", KS_DOWN, 0 ) && parses( "\033OC", KS_RIGHT, 0 ) && parses( "\033OD", KS_LEFT, 0 ),
        "application mode cursor keys (SS3)" );
    check( parses( "\033OH", KS_HOME, 0 ) && parses( "\033OF", KS_END, 0 ), "application mode Home and End" );
    check( parses( "\033[1~", KS_HOME, 0 ) && parses( "\033[7~", KS_HOME, 0 ) && parses( "\033[4~", KS_END, 0 ) && parses( "\033[8~", KS_END, 0 ),
        "Home and End as ESC [ 1 ~ / 7 ~ and 4 ~ / 8 ~ (VT220, rxvt, linux console)" );
    check( parses( "\033[11~", KS_F1, 0 ) && parses( "\033[12~", KS_F2, 0 ) && parses( "\033[13~", KS_F3, 0 ) && parses( "\033[14~", KS_F4, 0 ),
        "F1..F4 as ESC [ 11 ~ .. 14 ~ (rxvt)" );
    check( parses( "\033[[A", KS_F1, 0 ) && parses( "\033[[E", KS_F5, 0 ), "F1..F5 of the linux console (ESC [ [ A..E)" );
    check( parses( "\033[P", KS_F1, 0 ) && parses( "\033[1;2P", KS_F1, KSMOD_SHIFT ) && parses( "\033O2P", KS_F1, KSMOD_SHIFT ) && parses( "\033O5S", KS_F4, KSMOD_CTRL ),
        "F1..F4 as CSI finals and with the modifier after ESC O" );
    check( parses( "\033[1;3A", KS_UP, KSMOD_ALT ) && parses( "\033[1;8H", KS_HOME, KSMOD_SHIFT | KSMOD_ALT | KSMOD_CTRL ), "modifiers on cursor keys and Home" );
    int k, m;
    check( keyseq_parse( "\033[Axyz", 6U, &k, &m ) == 3 && k == KS_UP, "only the sequence is taken, not the text behind it" );
}

static void test_rejects( void ) {
    int k, m;
    check( keyseq_parse( "", 0, &k, &m ) == 0 && keyseq_parse( "a", 1U, &k, &m ) == 0 && keyseq_parse( "\033", 1U, &k, &m ) == -1, "plain text is no sequence, a lone ESC may continue" );
    check( keyseq_parse( "\033\033", 2U, &k, &m ) == 0, "ESC ESC is no sequence (it is the break key)" );
    check( keyseq_parse( "\033x", 2U, &k, &m ) == 0 && keyseq_parse( "\033[Z", 3U, &k, &m ) == 0 && keyseq_parse( "\033[99~", 6U, &k, &m ) == 0,
        "unknown sequences" );
    check( keyseq_parse( "\033[1;0A", 7U, &k, &m ) == 0 && keyseq_parse( "\033[1;17A", 8U, &k, &m ) == 0, "bad modifier numbers" );
    check( keyseq_parse( "\033[1;;A", 7U, &k, &m ) == 0 && keyseq_parse( "\033[1;2;3A", 9U, &k, &m ) == 0, "bad parameter lists" );
    check( keyseq_parse( "\033[2A", 4U, &k, &m ) == 0, "a repeat count is no key" );
    check( keyseq_parse( "\033[111111111111111111~", 20U, &k, &m ) == 0, "long numbers are refused" );
    check( keyseq_parse( "\033[M", 3U, &k, &m ) == 0, "mouse reports are not keys" );
}

static void test_names( void ) {
    char b[KEYSEQ_NAMELEN];
    keyseq_name( KS_UP, 0, b );
    check( strcmp( b, "Up" ) == 0, "name of a key" );
    keyseq_name( KS_PGDN, KSMOD_SHIFT | KSMOD_CTRL, b );
    check( strcmp( b, "Ctrl+Shift+PageDown" ) == 0, "name with modifiers, in a fixed order" );
    keyseq_name( KS_F12, KSMOD_ALL, b );
    check( strcmp( b, "Ctrl+Alt+Shift+Meta+F12" ) == 0 && strlen( b ) < KEYSEQ_NAMELEN, "the longest name fits" );
    keyseq_name( KS_NONE, 0, b );
    check( b[0] == '\0', "no name for no key" );
}

static void test_format( void ) {
    char b[64];
    keyseq_hex( "\033[1;2A", 6U, b, sizeof(b) );
    check( strcmp( b, "1b 5b 31 3b 32 41" ) == 0, "hex dump" );
    keyseq_escape( "\033[A a\"\\\n\x01\xc3", 10U, b, sizeof(b) );
    check( strcmp( b, "\\e[A a\\\"\\\\\\n\\x01\\xc3" ) == 0, "escaped dump" );
    keyseq_hex( "abcdefgh", 8U, b, 10U );
    check( strlen( b ) < 10U, "the hex dump stays inside its buffer" );
    keyseq_escape( "\001\001\001\001\001", 5U, b, 8U );
    check( strlen( b ) < 8U, "the escaped dump stays inside its buffer" );
}

static void test_sdl( void ) {
    char b[KEYSEQ_MAXLEN];
    check( sdlkeys_sequence( SDLK_UP, 0, b ) == 3U && strcmp( b, "\033[A" ) == 0, "SDL: Up" );
    check( sdlkeys_sequence( SDLK_RIGHT, KMOD_LSHIFT, b ) > 0 && strcmp( b, "\033[1;2C" ) == 0, "SDL: Shift+Right (left shift key)" );
    check( sdlkeys_sequence( SDLK_LEFT, KMOD_RCTRL, b ) > 0 && strcmp( b, "\033[1;5D" ) == 0, "SDL: Ctrl+Left (right ctrl key)" );
    check( sdlkeys_sequence( SDLK_F1, 0, b ) > 0 && strcmp( b, "\033OP" ) == 0, "SDL: F1" );
    check( sdlkeys_sequence( SDLK_F9, KMOD_LALT | KMOD_LSHIFT, b ) > 0 && strcmp( b, "\033[20;4~" ) == 0, "SDL: Alt+Shift+F9" );
    check( sdlkeys_sequence( SDLK_DELETE, KMOD_GUI, b ) > 0 && strcmp( b, "\033[3;9~" ) == 0, "SDL: Meta+Delete" );
    check( sdlkeys_sequence( SDLK_UP, KMOD_CAPS | KMOD_NUM | KMOD_MODE, b ) > 0 && strcmp( b, "\033[A" ) == 0, "SDL: Caps Lock, Num Lock and AltGr are no modifiers" );
    check( sdlkeys_sequence( SDLK_KP_8, 0, b ) > 0 && strcmp( b, "\033[A" ) == 0 && sdlkeys_sequence( SDLK_KP_3, 0, b ) > 0 && strcmp( b, "\033[6~" ) == 0,
        "SDL: keypad keys without Num Lock are the cursor block" );
    check( sdlkeys_sequence( SDLK_KP_8, KMOD_NUM, b ) == 0 && b[0] == '\0', "SDL: keypad digits with Num Lock are not sequences" );
    check( sdlkeys_sequence( SDLK_a, 0, b ) == 0 && sdlkeys_sequence( SDLK_RETURN, 0, b ) == 0 && sdlkeys_sequence( SDLK_ESCAPE, 0, b ) == 0 &&
        sdlkeys_sequence( SDLK_PAUSE, 0, b ) == 0, "SDL: letters, Return, Escape and Pause have no sequence" );
}

int main( void ) {
    test_make();
    test_roundtrip();
    test_dialects();
    test_rejects();
    test_names();
    test_format();
    test_sdl();
    printf( "%d of %d checks passed\n", total - failures, total );
    return failures;
}
