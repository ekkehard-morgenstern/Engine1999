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

#include "joy.h"

// -- state -----------------------------------------------------------------------------------

typedef struct _joyslot_t {
    uint32_t    connected;
    uint32_t    buttons;        // held
    uint32_t    pressed;        // latched
    uint32_t    released;       // latched
    int32_t     axis[JOYA_COUNT];       // raw, -32768 .. 32767
    char        name[64];
} joyslot_t;

static joyslot_t slots[JOY_MAXPLAYERS];
static uint32_t  evcount = 0;
static int32_t   deadzone_pm = 200;     // the dead zone in thousandths

#define LD( p )         __atomic_load_n( (p), __ATOMIC_ACQUIRE )
#define ST( p, v )      __atomic_store_n( (p), (v), __ATOMIC_RELEASE )

static void bump( void ) {
    __atomic_add_fetch( &evcount, 1U, __ATOMIC_ACQ_REL );
    __atomic_and_fetch( &evcount, UINT32_C(0x7FFFFFFF), __ATOMIC_ACQ_REL );
}

void joy_init( void ) {
    memset( slots, 0, sizeof(slots) );
    ST( &evcount, 0U );
    ST( &deadzone_pm, (int32_t)( JOY_DEFAULT_DEADZONE * 1000.0 + 0.5 ) );
}

int joy_attach( const char* name ) {
    for ( int i=0; i < JOY_MAXPLAYERS; ++i ) {
        if ( !LD( &slots[i].connected ) ) {
            joyslot_t* s = &slots[i];
            snprintf( s->name, sizeof(s->name), "%s", name != 0 ? name : "" );
            ST( &s->buttons, 0U );
            for ( int a=0; a < JOYA_COUNT; ++a ) {
                ST( &s->axis[a], 0 );
            }
            ST( &s->connected, 1U );
            bump();
            return i;
        }
    }
    return -1;
}

void joy_detach( int slot ) {
    if ( slot < 0 || slot >= JOY_MAXPLAYERS || !LD( &slots[slot].connected ) ) {
        return;
    }
    joyslot_t* s = &slots[slot];
    uint32_t held = __atomic_exchange_n( &s->buttons, 0U, __ATOMIC_ACQ_REL );
    if ( held != 0U ) {
        __atomic_or_fetch( &s->released, held, __ATOMIC_ACQ_REL );
    }
    for ( int a=0; a < JOYA_COUNT; ++a ) {
        ST( &s->axis[a], 0 );
    }
    ST( &s->connected, 0U );
    bump();
}

void joy_event_button( int slot, unsigned button, bool down ) {
    if ( slot < 0 || slot >= JOY_MAXPLAYERS || !LD( &slots[slot].connected ) ) {
        return;
    }
    button &= JOYB_ALL;
    if ( button == 0U ) {
        return;
    }
    joyslot_t* s = &slots[slot];
    if ( down ) {
        uint32_t old = __atomic_fetch_or( &s->buttons, button, __ATOMIC_ACQ_REL );
        if ( ( old & button ) == button ) {
            return;                                 // (no change: a repeat is no event)
        }
        __atomic_or_fetch( &s->pressed, button & ~old, __ATOMIC_ACQ_REL );
    } else {
        uint32_t old = __atomic_fetch_and( &s->buttons, ~button, __ATOMIC_ACQ_REL );
        if ( ( old & button ) == 0U ) {
            return;
        }
        __atomic_or_fetch( &s->released, button & old, __ATOMIC_ACQ_REL );
    }
    bump();
}

void joy_event_axis( int slot, int axis, int raw ) {
    if ( slot < 0 || slot >= JOY_MAXPLAYERS || axis < 0 || axis >= JOYA_COUNT || !LD( &slots[slot].connected ) ) {
        return;
    }
    if ( raw < -32768 ) raw = -32768;
    if ( raw > 32767 ) raw = 32767;
    ST( &slots[slot].axis[axis], (int32_t) raw );
}

bool joy_connected( int slot ) {
    return slot >= 0 && slot < JOY_MAXPLAYERS && LD( &slots[slot].connected ) != 0U;
}

int joy_count( void ) {
    int n = 0;
    for ( int i=0; i < JOY_MAXPLAYERS; ++i ) {
        if ( LD( &slots[i].connected ) ) {
            ++n;
        }
    }
    return n;
}

uint32_t joy_buttons( int slot ) {
    return joy_connected( slot ) ? LD( &slots[slot].buttons ) : 0U;
}

uint32_t joy_pressed( int slot ) {
    return slot >= 0 && slot < JOY_MAXPLAYERS ? __atomic_exchange_n( &slots[slot].pressed, 0U, __ATOMIC_ACQ_REL ) : 0U;
}

uint32_t joy_released( int slot ) {
    return slot >= 0 && slot < JOY_MAXPLAYERS ? __atomic_exchange_n( &slots[slot].released, 0U, __ATOMIC_ACQ_REL ) : 0U;
}

double joy_axis( int slot, int axis ) {
    if ( !joy_connected( slot ) || axis < 0 || axis >= JOYA_COUNT ) {
        return 0.0;
    }
    int raw = (int) LD( &slots[slot].axis[axis] );
    if ( axis >= JOYA_LT ) {                        // a trigger: 0 .. 1
        double t = raw > 0 ? (double) raw / 32767.0 : 0.0;
        return t > 1.0 ? 1.0 : t;
    }
    double v = raw >= 0 ? (double) raw / 32767.0 : (double) raw / 32768.0;
    double dz = (double) LD( &deadzone_pm ) / 1000.0;
    double m = v < 0.0 ? -v : v;
    if ( m <= dz ) {
        return 0.0;
    }
    m = ( m - dz ) / ( 1.0 - dz );                  // the rest of the way is scaled to the full range
    if ( m > 1.0 ) {
        m = 1.0;
    }
    return v < 0.0 ? -m : m;
}

uint32_t joy_events( void ) {
    return LD( &evcount );
}

const char* joy_name( int slot ) {
    return joy_connected( slot ) ? slots[slot].name : "";
}

void joy_setdeadzone( double dz ) {
    if ( !( dz >= 0.0 ) ) {
        dz = 0.0;
    }
    if ( dz > 0.9 ) {
        dz = 0.9;
    }
    ST( &deadzone_pm, (int32_t)( dz * 1000.0 + 0.5 ) );
}

double joy_getdeadzone( void ) {
    return (double) LD( &deadzone_pm ) / 1000.0;
}

// -- names -------------------------------------------------------------------------------------

static bool same( const char* a, const char* name, size_t len ) {
    size_t n = strlen( a );
    if ( n != len ) {
        return false;
    }
    for ( size_t i=0; i < len; ++i ) {
        char c = name[i];
        if ( c >= 'a' && c <= 'z' ) {
            c = (char)( c - 'a' + 'A' );
        }
        if ( c != a[i] ) {
            return false;
        }
    }
    return true;
}

int joy_button_by_name( const char* name, size_t len ) {
    static const struct { const char* name; unsigned bit; } tbl[] = {
        { "A", JOYB_A }, { "B", JOYB_B }, { "X", JOYB_X }, { "Y", JOYB_Y }, { "LB", JOYB_LB }, { "RB", JOYB_RB }, { "BACK", JOYB_BACK },
        { "START", JOYB_START }, { "GUIDE", JOYB_GUIDE }, { "LS", JOYB_LS }, { "RS", JOYB_RS }, { "UP", JOYB_UP }, { "DOWN", JOYB_DOWN },
        { "LEFT", JOYB_LEFT }, { "RIGHT", JOYB_RIGHT }, { 0, 0 }
    };
    for ( int i=0; tbl[i].name != 0; ++i ) {
        if ( same( tbl[i].name, name, len ) ) {
            return (int) tbl[i].bit;
        }
    }
    return 0;
}

int joy_axis_by_name( const char* name, size_t len ) {
    static const char* const tbl[] = { "LX", "LY", "RX", "RY", "LT", "RT" };
    for ( int i=0; i < JOYA_COUNT; ++i ) {
        if ( same( tbl[i], name, len ) ) {
            return i;
        }
    }
    return -1;
}
