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
#include "keystate.h"
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
        { KS_BACKTAB, 0, "\033[Z" }, { KS_BACKTAB, KSMOD_CTRL, "\033[1;5Z" },
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
    check( keyseq_parse( "\033x", 2U, &k, &m ) == 0 && keyseq_parse( "\033[Y", 3U, &k, &m ) == 0 && keyseq_parse( "\033[99~", 6U, &k, &m ) == 0,
        "unknown sequences" );
    check( keyseq_parse( "\033[1;0A", 7U, &k, &m ) == 0 && keyseq_parse( "\033[1;17A", 8U, &k, &m ) == 0, "bad modifier numbers" );
    check( keyseq_parse( "\033[1;;A", 7U, &k, &m ) == 0 && keyseq_parse( "\033[1;2;3A", 9U, &k, &m ) == 0, "bad parameter lists" );
    check( keyseq_parse( "\033[2A", 4U, &k, &m ) == 0, "a repeat count is no key" );
    check( keyseq_parse( "\033[111111111111111111~", 20U, &k, &m ) == 0, "long numbers are refused" );
    check( keyseq_parse( "\033[M", 3U, &k, &m ) == 0, "mouse reports are not keys" );
}

static void test_charset( void ) {
    check( keyseq_latin9( 'a' ) == 'a' && keyseq_latin9( 0xE4 ) == 0xE4 && keyseq_latin9( 0xFF ) == 0xFF && keyseq_latin9( 0xA0 ) == 0xA0, "Unicode -> ISO-8859-15: ASCII and the common Latin-1 letters are the same" );
    check( keyseq_latin9( 0x20AC ) == 0xA4 && keyseq_latin9( 0x160 ) == 0xA6 && keyseq_latin9( 0x161 ) == 0xA8 && keyseq_latin9( 0x17D ) == 0xB4 &&
        keyseq_latin9( 0x17E ) == 0xB8 && keyseq_latin9( 0x152 ) == 0xBC && keyseq_latin9( 0x153 ) == 0xBD && keyseq_latin9( 0x178 ) == 0xBE, "the eight changes of ISO-8859-15" );
    check( keyseq_latin9( 0xA4 ) == -1 && keyseq_latin9( 0xBC ) == -1 && keyseq_latin9( 0x85 ) == -1 && keyseq_latin9( 0x3042 ) == -1 && keyseq_latin9( 0x100 ) == -1,
        "characters the set doesn't have (the Latin-1 signs it replaced, C1 controls, other scripts)" );
    int bad = 0;
    for ( int c = 0; c < 256; ++c ) {
        if ( c >= 0x80 && c < 0xA0 ) {
            continue;
        }
        if ( keyseq_latin9( keyseq_unicode( c ) ) != c ) {
            ++bad;
        }
    }
    check( bad == 0 && keyseq_unicode( 0x90 ) == 0xFFFD, "ISO-8859-15 -> Unicode -> ISO-8859-15 for all printable codes; C1 gives U+FFFD" );
    char in[256], u[3 * 256], back[256];
    size_t n = 0;
    for ( int c = 1; c < 256; ++c ) {
        if ( c < 0x80 || c >= 0xA0 ) {
            in[n++] = (char) c;
        }
    }
    size_t un = keyseq_latin9_to_utf8( in, n, u );
    check( keyseq_utf8_to_latin9( u, un, back ) == n && memcmp( in, back, n ) == 0, "text round trip through UTF-8" );
    check( keyseq_latin9_to_utf8( "\xe4\xa4", 2U, u ) == 5U && memcmp( u, "\xc3\xa4\xe2\x82\xac", 5U ) == 0, "UTF-8 of ä and the euro sign" );
    n = keyseq_utf8_to_latin9( "a\xc3\xa4\xe3\x81\x82\x80z\xc3", 8U, back );
    check( n == 3U && memcmp( back, "a\xe4z", 3U ) == 0, "UTF-8 -> ISO-8859-15 drops other scripts, stray bytes and cut-off characters" );
    // listings: UTF-8 on disk
    check( keyseq_listing_is_utf8( "a\xc3\xa4", 3U ) && !keyseq_listing_is_utf8( "plain", 5U ) && !keyseq_listing_is_utf8( "a\xe4z", 3U ) &&
        !keyseq_listing_is_utf8( "\xc3", 1U ) && !keyseq_listing_is_utf8( "\xc0\x80", 2U ) && !keyseq_listing_is_utf8( "\xed\xa0\x80", 3U ) &&
        !keyseq_listing_is_utf8( "\xc3\xa4\xe4", 3U ), "a listing is UTF-8 only if it has non-ASCII characters and is well formed" );
    size_t lost = 0;
    n = keyseq_listing_from_utf8( "\xc3\xa4\xe2\x82\xac\xe3\x81\x82\xc2\x85", 10U, back, &lost );
    check( n == 4U && memcmp( back, "\xe4\xa4?\x85", 4U ) == 0 && lost == 1U, "listing from UTF-8: umlaut, euro, a lost character as '?', a C1 code kept" );
    unsigned char all[256]; char l9[256 * 3 + 1], l9b[256 * 3 + 1];
    for ( int c2 = 0; c2 < 256; ++c2 ) {
        all[c2] = (unsigned char) c2;
    }
    size_t ul = keyseq_listing_to_utf8( (const char*) all, 256U, l9 );
    lost = 0;
    check( keyseq_listing_is_utf8( l9, ul ) && keyseq_listing_from_utf8( l9, ul, l9b, &lost ) == 256U && memcmp( l9b, all, 256U ) == 0 && lost == 0U,
        "all 256 codes survive a round trip through a UTF-8 listing" );
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
    check( sdlkeys_sequence( SDLK_TAB, 0, b ) == 0 && sdlkeys_sequence( SDLK_TAB, KMOD_LSHIFT, b ) > 0 && strcmp( b, "\033[Z" ) == 0, "SDL: Tab is one byte, Shift+Tab is a sequence" );
    check( sdlkeys_sequence( SDLK_a, 0, b ) == 0 && sdlkeys_sequence( SDLK_RETURN, 0, b ) == 0 && sdlkeys_sequence( SDLK_ESCAPE, 0, b ) == 0 &&
        sdlkeys_sequence( SDLK_PAUSE, 0, b ) == 0, "SDL: letters, Return, Escape and Pause have no sequence" );
}

static void test_keystate( void ) {
    #define KID(s) keystate_id( s, strlen( s ) )
    check( KID( "A" ) == 'a' && KID( "a" ) == 'a' && KID( "z" ) == 'z' && KID( "5" ) == '5' && KID( "," ) == ',' && KID( "/" ) == '/', "key names: characters (letters in lower case)" );
    check( KID( " " ) == 32 && KID( "space" ) == 32 && KID( "SPACE" ) == 32, "key names: SPACE" );
    check( KID( "left" ) == KEYID_KS + KS_LEFT && KID( "Up" ) == KEYID_KS + KS_UP && KID( "PGDN" ) == KEYID_KS + KS_PGDN && KID( "pagedown" ) == KEYID_KS + KS_PGDN, "key names: the cursor keys, case does not matter" );
    check( KID( "F1" ) == KEYID_KS + KS_F1 && KID( "f12" ) == KEYID_KS + KS_F12 && KID( "F13" ) < 0, "key names: function keys F1..F12" );
    check( KID( "ESC" ) == KEYID_ESC && KID( "escape" ) == KEYID_ESC && KID( "return" ) == KEYID_RETURN && KID( "enter" ) == KEYID_RETURN && KID( "tab" ) == KEYID_TAB && KID( "BackSpace" ) == KEYID_BACKSPACE, "key names: ESC, RETURN, TAB, BACKSPACE" );
    check( KID( "shift" ) == KEYID_SHIFT && KID( "lshift" ) == KEYID_LSHIFT && KID( "RCTRL" ) == KEYID_RCTRL && KID( "alt" ) == KEYID_ALT && KID( "ctrl" ) == KEYID_CTRL, "key names: the modifiers" );
    check( KID( "KP0" ) == KEYID_KP0 && KID( "kp9" ) == KEYID_KP0 + 9 && KID( "KPENTER" ) == KEYID_KPENTER && KID( "KP+" ) == KEYID_KPPLUS && KID( "kp." ) == KEYID_KPDOT, "key names: the keypad" );
    check( KID( "ANY" ) == KEYID_ANY && KID( "any" ) == KEYID_ANY, "key names: ANY" );
    check( KID( "" ) < 0 && KID( "BANANA" ) < 0 && KID( "LEF" ) < 0 && KID( "\x01" ) < 0 && KID( "ABCDEFGHIJKLMNOP" ) < 0, "key names: anything else is no key" );
    check( keystate_id( 0, 0 ) < 0 && keystate_id( "A\0B", 3 ) < 0, "key names: no name, a zero byte" );
    check( keystate_ks( KID( "DOWN" ) ) == KS_DOWN && keystate_ks( KID( "a" ) ) == KS_NONE && keystate_ks( KEYID_ESC ) == KS_NONE, "key names: the keyseq key of an id" );
    check( KEYID_MAX <= 1024, "key ids fit the tables of the console host" );
    #undef KID
}

// the names of key ids, the key event queue (ON KEY, KEY$("EVENT")) and the SDL keys' ids
static void test_events( void ) {
    check( strcmp( keystate_name( 'a' ), "A" ) == 0 && strcmp( keystate_name( '5' ), "5" ) == 0 && strcmp( keystate_name( ',' ), "," ) == 0 && strcmp( keystate_name( 32 ), "SPACE" ) == 0, "key event names: characters, SPACE" );
    check( strcmp( keystate_name( KEYID_RETURN ), "RETURN" ) == 0 && strcmp( keystate_name( KEYID_ESC ), "ESC" ) == 0 && strcmp( keystate_name( KEYID_BACKSPACE ), "BACKSPACE" ) == 0 &&
        strcmp( keystate_name( KEYID_KS + KS_LEFT ), "LEFT" ) == 0 && strcmp( keystate_name( KEYID_KS + KS_F5 ), "F5" ) == 0 && strcmp( keystate_name( KEYID_KS + KS_PGDN ), "PGDN" ) == 0,
        "key event names: RETURN (not ENTER), ESC, the cursor and function keys" );
    check( strcmp( keystate_name( KEYID_LSHIFT ), "LSHIFT" ) == 0 && strcmp( keystate_name( KEYID_KP0 + 5 ), "KP5" ) == 0 && strcmp( keystate_name( KEYID_KPPLUS ), "KP+" ) == 0 &&
        strcmp( keystate_name( 9999 ), "?" ) == 0 && strcmp( keystate_name( 0 ), "?" ) == 0, "key event names: modifiers, the keypad, no such key" );
    bool roundtrip = true;
    for ( int id = 1; id < KEYID_MAX; ++id ) {
        if ( id >= 'A' && id <= 'Z' ) {
            continue;                       // (the ids of the letters are lower case)
        }
        const char* n = keystate_name( id );
        if ( strcmp( n, "?" ) != 0 && keystate_id( n, strlen( n ) ) != id && id != KEYID_ANY ) {
            roundtrip = false;
            printf( "  id %d is called %s, which is id %d\n", id, n, keystate_id( n, strlen( n ) ) );
        }
    }
    check( roundtrip, "key event names: every name is the name of its id" );
    int id = 0; bool down = false;
    keystate_event_clear();
    uint32_t c0 = keystate_event_count();
    check( !keystate_event_pop( &id, &down ), "key events: none at the start" );
    keystate_event_push( 'a', true ); keystate_event_push( 'a', false ); keystate_event_push( KEYID_KS + KS_LEFT, true );
    check( keystate_event_count() == c0 + 3U, "key events: counted" );
    check( keystate_event_pop( &id, &down ) && id == 'a' && down && keystate_event_pop( &id, &down ) && id == 'a' && !down &&
        keystate_event_pop( &id, &down ) && id == KEYID_KS + KS_LEFT && down && !keystate_event_pop( &id, &down ), "key events: in the order they came" );
    keystate_event_push( 'b', true ); keystate_event_push( 'c', true );
    keystate_event_clear();
    check( !keystate_event_pop( &id, &down ), "key events: clear forgets them" );
    for ( int i = 0; i < KEYEV_SIZE + 10; ++i ) {
        keystate_event_push( 'a' + ( i % 26 ), i % 2 == 0 );
    }
    int n = 0; bool order = true;
    while ( keystate_event_pop( &id, &down ) ) {
        if ( id != 'a' + ( n % 26 ) || down != ( n % 2 == 0 ) ) order = false;
        ++n;
    }
    check( n == KEYEV_SIZE && order, "key events: a full ring drops the new ones, the old ones stay in order" );
    keystate_event_push( 0, true ); keystate_event_push( -5, true ); keystate_event_push( 70000, true );
    check( !keystate_event_pop( &id, &down ), "key events: bad ids are ignored" );
    check( sdlkeys_keyid( SDLK_a ) == 'a' && sdlkeys_keyid( SDLK_SPACE ) == 32 && sdlkeys_keyid( SDLK_COMMA ) == ',' && sdlkeys_keyid( SDLK_5 ) == '5', "SDL key ids: characters" );
    check( sdlkeys_keyid( SDLK_RETURN ) == KEYID_RETURN && sdlkeys_keyid( SDLK_ESCAPE ) == KEYID_ESC && sdlkeys_keyid( SDLK_TAB ) == KEYID_TAB && sdlkeys_keyid( SDLK_BACKSPACE ) == KEYID_BACKSPACE, "SDL key ids: RETURN, ESC, TAB, BACKSPACE" );
    check( sdlkeys_keyid( SDLK_LEFT ) == KEYID_KS + KS_LEFT && sdlkeys_keyid( SDLK_F5 ) == KEYID_KS + KS_F5 && sdlkeys_keyid( SDLK_PAGEDOWN ) == KEYID_KS + KS_PGDN && sdlkeys_keyid( SDLK_HOME ) == KEYID_KS + KS_HOME, "SDL key ids: the cursor and function keys" );
    check( sdlkeys_keyid( SDLK_LSHIFT ) == KEYID_LSHIFT && sdlkeys_keyid( SDLK_RCTRL ) == KEYID_RCTRL && sdlkeys_keyid( SDLK_LALT ) == KEYID_LALT, "SDL key ids: the modifiers" );
    check( sdlkeys_keyid( SDLK_KP_0 ) == KEYID_KP0 && sdlkeys_keyid( SDLK_KP_5 ) == KEYID_KP0 + 5 && sdlkeys_keyid( SDLK_KP_9 ) == KEYID_KP0 + 9 && sdlkeys_keyid( SDLK_KP_ENTER ) == KEYID_KPENTER && sdlkeys_keyid( SDLK_KP_PLUS ) == KEYID_KPPLUS, "SDL key ids: the keypad" );
    check( sdlkeys_keyid( SDLK_PRINTSCREEN ) == 0 && sdlkeys_keyid( SDLK_CAPSLOCK ) == 0 && sdlkeys_keyid( SDLK_PAUSE ) == 0, "SDL key ids: keys without a name give 0" );
}

int main( void ) {
    test_keystate();
    test_events();
    test_make();
    test_roundtrip();
    test_dialects();
    test_rejects();
    test_names();
    test_charset();
    test_format();
    test_sdl();
    printf( "%d of %d checks passed\n", total - failures, total );
    return failures;
}
