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

#include "mouse.h"
#include "cursors.h"

// -- state ------------------------------------------------------------------------------------------------------------
// Words that two threads touch are only accessed with the __atomic builtins; the plain statics belong to the screen thread (mouse_tick()).

#define NOREQ   (-1)

// events (written by the screen thread, taken by mouse_tick(), in the same thread; atomic anyway so that they can be posted from elsewhere)
static int e_x, e_y;
static int e_moved;                 // 1: the position changed
static int e_activity;              // 1: a button or the wheel
// requests of the program
static int r_vis;                   // 0 none, 1 show, 2 hide (the last one wins)
static uint32_t r_pos;              // 0 none, else 0x80000000 | x << 16 | y
static int r_shape;                 // NOREQ or a shape
static int r_timeout;               // NOREQ (nothing) or milliseconds
// published by mouse_tick(), read by mouse_get()
static int p_x, p_y, p_buttons, p_shape, p_visible;
static int p_events;                                    // counts the presses, releases and wheel steps (ON MOUSE)
static int p_pressed, p_released, p_wheel;              // latched / accumulated by the screen thread, cleared by the reader
// the screen thread's own
static int      timeout_ms = MOUSE_DEFAULT_TIMEOUT;
static uint64_t deadline;           // 0: no hide is due
static int      cur_x, cur_y;
static int      warp_x, warp_y;
static int      warp_pending;

static int area_w = 640, area_h = 300;

static int clampi( int v, int lo, int hi ) {
    return v < lo ? lo : v > hi ? hi : v;
}

static int ld( int* p ) {
    return __atomic_load_n( p, __ATOMIC_ACQUIRE );
}

static void st( int* p, int v ) {
    __atomic_store_n( p, v, __ATOMIC_RELEASE );
}

void mouse_setarea( int w, int h ) {
    __atomic_store_n( &area_w, clampi( w, 1, MOUSE_AREA_MAXW ), __ATOMIC_RELEASE );
    __atomic_store_n( &area_h, clampi( h, 1, MOUSE_AREA_MAXH ), __ATOMIC_RELEASE );
}

// -- init -----------------------------------------------------------------------------------------------------------
void mouse_init( void ) {
    st( &e_x, 0 ); st( &e_y, 0 ); st( &e_moved, 0 ); st( &e_activity, 0 );
    st( &r_vis, 0 ); __atomic_store_n( &r_pos, 0U, __ATOMIC_RELEASE ); st( &r_shape, NOREQ ); st( &r_timeout, NOREQ );
    st( &p_x, 0 ); st( &p_y, 0 ); st( &p_buttons, 0 ); st( &p_shape, CUR_ARROW ); st( &p_visible, 0 );
    st( &p_pressed, 0 ); st( &p_released, 0 ); st( &p_wheel, 0 ); st( &p_events, 0 );
    timeout_ms = MOUSE_DEFAULT_TIMEOUT;
    deadline = 0;
    cur_x = cur_y = 0;
    warp_pending = 0;
}

// -- events -------------------------------------------------------------------------------------------------------
void mouse_event_motion( int x, int y ) {
    st( &e_x, clampi( x, 0, ld( &area_w ) - 1 ) );
    st( &e_y, clampi( y, 0, ld( &area_h ) - 1 ) );
    st( &e_moved, 1 );
}

void mouse_event_button( int button, bool down ) {
    button &= 7;
    if ( button == 0 ) {
        return;
    }
    if ( down ) {
        __atomic_fetch_or( &p_buttons, button, __ATOMIC_ACQ_REL );
        __atomic_fetch_or( &p_pressed, button, __ATOMIC_ACQ_REL );
    } else {
        __atomic_fetch_and( &p_buttons, ~button, __ATOMIC_ACQ_REL );
        __atomic_fetch_or( &p_released, button, __ATOMIC_ACQ_REL );
    }
    __atomic_fetch_add( &p_events, 1, __ATOMIC_ACQ_REL );
    st( &e_activity, 1 );
}

void mouse_event_wheel( int steps ) {
    if ( steps != 0 ) {
        __atomic_fetch_add( &p_wheel, steps, __ATOMIC_ACQ_REL );
        __atomic_fetch_add( &p_events, 1, __ATOMIC_ACQ_REL );
        st( &e_activity, 1 );
    }
}

// -- requests -----------------------------------------------------------------------------------------------------
void mouse_request_show( void ) {
    st( &r_vis, 1 );
}

void mouse_request_hide( void ) {
    st( &r_vis, 2 );
}

void mouse_request_pos( int x, int y ) {
    x = clampi( x, 0, ld( &area_w ) - 1 ); y = clampi( y, 0, ld( &area_h ) - 1 );
    __atomic_store_n( &r_pos, UINT32_C(0x80000000) | ( (uint32_t) ( x & 0x7FFF ) << 16 ) | (uint32_t)( y & 0x7FFF ), __ATOMIC_RELEASE );
}

void mouse_request_shape( int shape ) {
    if ( shape >= 0 && shape < CUR_COUNT ) {
        st( &r_shape, shape );
    }
}

void mouse_request_timeout( int ms ) {
    st( &r_timeout, ms < 0 ? MOUSE_DEFAULT_TIMEOUT : ms );
}

// -- the frame --------------------------------------------------------------------------------------------------
bool mouse_tick( uint64_t now_ms ) {
    bool act = false, changed = false;

    int tmo = __atomic_exchange_n( &r_timeout, NOREQ, __ATOMIC_ACQ_REL );
    if ( tmo != NOREQ ) {
        timeout_ms = tmo;
    }
    if ( __atomic_exchange_n( &e_moved, 0, __ATOMIC_ACQ_REL ) ) {
        cur_x = ld( &e_x ); cur_y = ld( &e_y );
        cursors_move( cur_x, cur_y );
        act = true;
    }
    if ( __atomic_exchange_n( &e_activity, 0, __ATOMIC_ACQ_REL ) ) {
        act = true;
    }
    uint32_t rp = __atomic_exchange_n( &r_pos, 0U, __ATOMIC_ACQ_REL );
    if ( ( rp & UINT32_C(0x80000000) ) != 0 ) {
        cur_x = (int)( ( rp >> 16 ) & 0x7FFFU ); cur_y = (int)( rp & 0x7FFFU );
        cursors_move( cur_x, cur_y );
        warp_x = cur_x; warp_y = cur_y; warp_pending = 1;
        act = true;
    }
    int sh = __atomic_exchange_n( &r_shape, NOREQ, __ATOMIC_ACQ_REL );
    if ( sh != NOREQ && cursors_set( sh ) ) {
        st( &p_shape, sh );
        act = true;
    }
    if ( act ) {
        st( &p_x, cur_x ); st( &p_y, cur_y );
    }
    int vis = __atomic_exchange_n( &r_vis, 0, __ATOMIC_ACQ_REL );
    if ( vis == 1 ) {
        act = true;
    }
    if ( vis == 2 ) {
        act = false;
        deadline = 0;
        if ( cursors_visible() ) {
            cursors_show( false );
            changed = true;
        }
        st( &p_visible, 0 );
    }
    if ( act ) {
        if ( !cursors_visible() ) {
            cursors_show( true );
        }
        st( &p_visible, 1 );
        deadline = timeout_ms > 0 ? now_ms + (uint64_t) timeout_ms : 0;
        changed = true;
    } else if ( deadline != 0 && now_ms >= deadline && timeout_ms > 0 ) {
        deadline = 0;
        if ( cursors_visible() ) {
            cursors_show( false );
            changed = true;
        }
        st( &p_visible, 0 );
    }
    return changed;
}

bool mouse_warp_wanted( int* x, int* y ) {
    if ( !warp_pending ) {
        return false;
    }
    warp_pending = 0;
    *x = warp_x; *y = warp_y;
    return true;
}

// -- the program's view ----------------------------------------------------------------------------------------
// A request that the screen thread has not taken yet is already true for the program (MOUSE HIDE followed by MOUSE("VISIBLE") is 0, MOUSE POS followed by
// MOUSE("X") is the new X): the values look at the pending requests first. (The events of the window are not requests: they are seen a frame later.)
int mouse_get( int what ) {
    uint32_t rp = __atomic_load_n( &r_pos, __ATOMIC_ACQUIRE );
    switch ( what ) {
        case MOUSEV_X:          return ( rp & UINT32_C(0x80000000) ) != 0 ? (int)( ( rp >> 16 ) & 0x7FFFU ) : ld( &p_x );
        case MOUSEV_Y:          return ( rp & UINT32_C(0x80000000) ) != 0 ? (int)( rp & 0x7FFFU ) : ld( &p_y );
        case MOUSEV_BUTTONS:    return ld( &p_buttons );
        case MOUSEV_PRESSED:    return __atomic_exchange_n( &p_pressed, 0, __ATOMIC_ACQ_REL );
        case MOUSEV_RELEASED:   return __atomic_exchange_n( &p_released, 0, __ATOMIC_ACQ_REL );
        case MOUSEV_WHEEL:      return __atomic_exchange_n( &p_wheel, 0, __ATOMIC_ACQ_REL );
        case MOUSEV_EVENTS:     return ld( &p_events ) & 0x7FFFFFFF;
        case MOUSEV_SHAPE: {
            int sh = ld( &r_shape );
            return sh != NOREQ ? sh : ld( &p_shape );
        }
        case MOUSEV_VISIBLE: {
            int vis = ld( &r_vis );
            return vis == 2 ? 0 : vis == 1 || ( rp & UINT32_C(0x80000000) ) != 0 || ld( &r_shape ) != NOREQ ? 1 : ld( &p_visible );
        }
        default:                return 0;
    }
}
