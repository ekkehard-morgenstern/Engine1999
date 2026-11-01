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

#include "keystate.h"

typedef struct _keyname_t {
    const char* name;
    int         id;
} keyname_t;

static const keyname_t keynames[] = {
    { "ANY", KEYID_ANY }, { "SPACE", 32 }, { "RETURN", KEYID_RETURN }, { "ENTER", KEYID_RETURN }, { "ESC", KEYID_ESC }, { "ESCAPE", KEYID_ESC },
    { "TAB", KEYID_TAB }, { "BACKSPACE", KEYID_BACKSPACE }, { "BS", KEYID_BACKSPACE },
    { "SHIFT", KEYID_SHIFT }, { "LSHIFT", KEYID_LSHIFT }, { "RSHIFT", KEYID_RSHIFT },
    { "CTRL", KEYID_CTRL }, { "CONTROL", KEYID_CTRL }, { "LCTRL", KEYID_LCTRL }, { "RCTRL", KEYID_RCTRL },
    { "ALT", KEYID_ALT }, { "LALT", KEYID_LALT }, { "RALT", KEYID_RALT },
    { "UP", KEYID_KS + KS_UP }, { "DOWN", KEYID_KS + KS_DOWN }, { "LEFT", KEYID_KS + KS_LEFT }, { "RIGHT", KEYID_KS + KS_RIGHT },
    { "HOME", KEYID_KS + KS_HOME }, { "END", KEYID_KS + KS_END }, { "INSERT", KEYID_KS + KS_INSERT }, { "DELETE", KEYID_KS + KS_DELETE },
    { "PGUP", KEYID_KS + KS_PGUP }, { "PAGEUP", KEYID_KS + KS_PGUP }, { "PGDN", KEYID_KS + KS_PGDN }, { "PAGEDOWN", KEYID_KS + KS_PGDN },
    { "F1", KEYID_KS + KS_F1 }, { "F2", KEYID_KS + KS_F2 }, { "F3", KEYID_KS + KS_F3 }, { "F4", KEYID_KS + KS_F4 },
    { "F5", KEYID_KS + KS_F5 }, { "F6", KEYID_KS + KS_F6 }, { "F7", KEYID_KS + KS_F7 }, { "F8", KEYID_KS + KS_F8 },
    { "F9", KEYID_KS + KS_F9 }, { "F10", KEYID_KS + KS_F10 }, { "F11", KEYID_KS + KS_F11 }, { "F12", KEYID_KS + KS_F12 },
    { "KP0", KEYID_KP0 }, { "KP1", KEYID_KP0 + 1 }, { "KP2", KEYID_KP0 + 2 }, { "KP3", KEYID_KP0 + 3 }, { "KP4", KEYID_KP0 + 4 },
    { "KP5", KEYID_KP0 + 5 }, { "KP6", KEYID_KP0 + 6 }, { "KP7", KEYID_KP0 + 7 }, { "KP8", KEYID_KP0 + 8 }, { "KP9", KEYID_KP0 + 9 },
    { "KPENTER", KEYID_KPENTER }, { "KP+", KEYID_KPPLUS }, { "KP-", KEYID_KPMINUS }, { "KP*", KEYID_KPMUL }, { "KP/", KEYID_KPDIV }, { "KP.", KEYID_KPDOT },
    { 0, 0 }
};

int keystate_id( const char* name, size_t len ) {
    if ( name == 0 || len == 0 || len > 12U ) {
        return -1;
    }
    if ( len == 1U ) {
        unsigned char c = (unsigned char) name[0];
        if ( c >= 'A' && c <= 'Z' ) {
            return c - 'A' + 'a';
        }
        if ( c > 32 && c < 127 ) {
            return c;
        }
        if ( c == 32 ) {
            return 32;
        }
        return -1;
    }
    char up[16];
    for ( size_t i = 0; i < len; ++i ) {
        unsigned char c = (unsigned char) name[i];
        if ( c == 0 ) {
            return -1;
        }
        up[i] = (char)( c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c );
    }
    up[len] = '\0';
    for ( const keyname_t* k = keynames; k->name != 0; ++k ) {
        if ( strcmp( k->name, up ) == 0 ) {
            return k->id;
        }
    }
    return -1;
}

const char* keystate_name( int id ) {
    static char one[2][2];
    static int n = 0;
    if ( id > 32 && id < 127 ) {
        char* p = one[ n++ & 1 ];                    // (a letter in upper case; two buffers so that two names can be used at once)
        p[0] = (char)( id >= 'a' && id <= 'z' ? id - 'a' + 'A' : id );
        p[1] = '\0';
        return p;
    }
    for ( const keyname_t* k = keynames; k->name != 0; ++k ) {
        if ( k->id == id && id != KEYID_ANY ) {
            return k->name;                          // (the first name of an id is its canonical one: RETURN before ENTER)
        }
    }
    return "?";
}

static uint16_t  evid[KEYEV_SIZE];
static uint8_t   evdown[KEYEV_SIZE];
static uint32_t  evhead = 0, evtail = 0, evtotal = 0;

void keystate_event_push( int id, bool down ) {
    uint32_t h = __atomic_load_n( &evhead, __ATOMIC_RELAXED ), t = __atomic_load_n( &evtail, __ATOMIC_ACQUIRE );
    if ( h - t >= KEYEV_SIZE || id <= 0 || id > 65535 ) {
        return;
    }
    evid[ h % KEYEV_SIZE ] = (uint16_t) id;
    evdown[ h % KEYEV_SIZE ] = down ? 1 : 0;
    __atomic_store_n( &evhead, h + 1U, __ATOMIC_RELEASE );
    __atomic_and_fetch( &evtotal, UINT32_C(0x7FFFFFFF), __ATOMIC_ACQ_REL );
    __atomic_add_fetch( &evtotal, 1U, __ATOMIC_ACQ_REL );
}

bool keystate_event_pop( int* id, bool* down ) {
    uint32_t t = __atomic_load_n( &evtail, __ATOMIC_RELAXED ), h = __atomic_load_n( &evhead, __ATOMIC_ACQUIRE );
    if ( t == h ) {
        return false;
    }
    *id = evid[ t % KEYEV_SIZE ];
    *down = evdown[ t % KEYEV_SIZE ] != 0;
    __atomic_store_n( &evtail, t + 1U, __ATOMIC_RELEASE );
    return true;
}

uint32_t keystate_event_count( void ) {
    return __atomic_load_n( &evtotal, __ATOMIC_ACQUIRE );
}

void keystate_event_clear( void ) {
    __atomic_store_n( &evtail, __atomic_load_n( &evhead, __ATOMIC_ACQUIRE ), __ATOMIC_RELEASE );
}

int keystate_ks( int id ) {
    return id >= KEYID_KS && id < KEYID_KS + KS_COUNT ? id - KEYID_KS : KS_NONE;
}
