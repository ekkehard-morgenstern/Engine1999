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

// sdltest4: the game controllers on the real SDL path (sdljoy.c -> joy.c), without a window.
//
//   ./sdltest4              a monitor for real pads: prints the pads that are plugged in or out and every change of a button, then the sticks (Ctrl-C ends it)
//   ./sdltest4 --selftest   an automated test with virtual pads (SDL_JoystickAttachVirtual, SDL 2.0.14 or later): one with a game controller mapping and one without
//                           (the plain joystick fallback); exit status = number of failures

#include "stdtypes.h"
#include "sdljoy.h"
#include "joy.h"
#include <signal.h>

static volatile sig_atomic_t stop = 0;

static int total = 0, failures = 0;
static void check( bool cond, const char* name ) {
    ++total;
    if ( !cond ) {
        ++failures;
        printf( "FAIL  %s\n", name );
    }
}

static void pump( void ) {
    SDL_Event ev;
    SDL_PumpEvents();
    while ( SDL_PollEvent( &ev ) ) {
        if ( ev.type == SDL_QUIT ) {            // (SDL turns Ctrl-C / SIGTERM into this)
            stop = 1;
        }
        sdljoy_handle_event( &ev );
    }
}

static void pump_ms( int ms ) {
    for ( int t = 0; t < ms; t += 5 ) {
        pump();
        SDL_Delay( 5 );
    }
}

static bool near( double a, double b ) { return a - b < 0.01 && b - a < 0.01; }

static bool init_sdl( void ) {
    SDL_setenv( "SDL_VIDEODRIVER", "dummy", 0 );
    SDL_SetHint( SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1" );     // (there is no window with the focus)
    sdljoy_hints();
    if ( SDL_Init( SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS ) != 0 ) {
        fprintf( stderr, "SDL_Init: %s\n", SDL_GetError() );
        return false;
    }
    sdljoy_init();
    return true;
}

// a virtual pad with the standard layout (SDL makes its own mapping for it: the buttons in the order of SDL_GameControllerButton, six axes)
static int attach_gc( void ) {
    SDL_VirtualJoystickDesc d;
    SDL_zero( d );
    d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    d.naxes = 6; d.nbuttons = 15; d.nhats = 0;
    d.name = "Virtual Test Pad";
    int index = SDL_JoystickAttachVirtualEx( &d );
    if ( index >= 0 && SDL_IsGameController( index ) != SDL_TRUE ) {
        // no mapping made by SDL: give it the standard one
        char guid[64], map[512];
        SDL_JoystickGetGUIDString( SDL_JoystickGetDeviceGUID( index ), guid, sizeof(guid) );
        snprintf( map, sizeof(map), "%s,Virtual Test Pad,a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,guide:b8,leftstick:b9,rightstick:b10,"
            "dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5", guid );
        SDL_GameControllerAddMapping( map );
    }
    return index;
}

static int attach_plain( void ) {
    SDL_VirtualJoystickDesc d;
    SDL_zero( d );
    d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    d.type = SDL_JOYSTICK_TYPE_UNKNOWN;
    d.naxes = 4; d.nbuttons = 12; d.nhats = 1;
    d.name = "Virtual Plain Joystick";
    return SDL_JoystickAttachVirtualEx( &d );
}

static int selftest( void ) {
    if ( !init_sdl() ) {
        return 1;
    }
    pump_ms( 50 );
    // (a real pad that is plugged in takes its slot first: the test works with the slots behind it)
    int base = joy_count();
    if ( base > 2 ) {
        printf( "(%d real pads are plugged in: unplug some for this test)\n", base );
        SDL_Quit();
        return 1;
    }
    if ( base > 0 ) {
        printf( "(%d real pad(s) plugged in: the virtual pads are players %d and %d)\n", base, base + 1, base + 2 );
    }

    // a pad with a mapping: it gets a slot, its buttons and axes arrive
    int gi = attach_gc();
    check( gi >= 0, "a virtual game controller can be attached" );
    if ( gi < 0 ) {
        SDL_Quit();
        return failures + 1;
    }
    SDL_Joystick* g = SDL_JoystickOpen( gi );
    check( g != 0, "... and opened" );
    pump_ms( 100 );
    check( joy_count() == base + 1 && joy_connected( base ) && strcmp( joy_name( base ), "Virtual Test Pad" ) == 0, "the controller has a slot with its name" );
    uint32_t ev0 = joy_events();
    SDL_JoystickSetVirtualButton( g, 0, 1 );
    pump_ms( 30 );
    check( joy_buttons( base ) == JOYB_A && joy_pressed( base ) == JOYB_A && joy_events() == ev0 + 1U, "button 0 is A (the bottom face button)" );
    SDL_JoystickSetVirtualButton( g, 0, 0 );
    pump_ms( 30 );
    check( joy_buttons( base ) == 0U && joy_released( base ) == JOYB_A, "released" );
    // (SDL's own order for a virtual pad: A B X Y Back Guide Start LeftStick RightStick LeftShoulder RightShoulder Up Down Left Right)
    static const unsigned expect[15] = { JOYB_A, JOYB_B, JOYB_X, JOYB_Y, JOYB_BACK, JOYB_GUIDE, JOYB_START, JOYB_LS, JOYB_RS, JOYB_LB, JOYB_RB, JOYB_UP, JOYB_DOWN, JOYB_LEFT, JOYB_RIGHT };
    bool all = true;
    for ( int b = 0; b < 15; ++b ) {
        SDL_JoystickSetVirtualButton( g, b, 1 );
        pump_ms( 15 );
        if ( joy_buttons( base ) != expect[b] ) {
            all = false;
            printf( "  button %d: got %u, expected %u\n", b, (unsigned) joy_buttons( base ), expect[b] );
        }
        SDL_JoystickSetVirtualButton( g, b, 0 );
        pump_ms( b == 5 ? 300 : 15 );       // (SDL delays the release of the Guide button by 250 ms, so that a quick tap is not missed)
    }
    check( all, "all 15 buttons map to their names" );
    joy_pressed( base ); joy_released( base );
    SDL_JoystickSetVirtualAxis( g, 0, 32767 );
    SDL_JoystickSetVirtualAxis( g, 1, -32768 );
    SDL_JoystickSetVirtualAxis( g, 2, 16384 );
    SDL_JoystickSetVirtualAxis( g, 4, 32767 );
    SDL_JoystickSetVirtualAxis( g, 5, 16384 );
    pump_ms( 30 );
    check( near( joy_axis( base, JOYA_LX ), 1.0 ) && near( joy_axis( base, JOYA_LY ), -1.0 ), "the left stick: right and up" );
    check( near( joy_axis( base, JOYA_RX ), ( 0.5 - 0.2 ) / 0.8 ), "the right stick, the dead zone applied" );
    // (a virtual trigger axis has the whole range -32768 .. 32767, SDL turns it into 0 .. 32767: 16384 becomes 0.75)
    check( near( joy_axis( base, JOYA_LT ), 1.0 ) && near( joy_axis( base, JOYA_RT ), 0.75 ), "the triggers" );

    // a pad without a mapping: the plain joystick fallback
    int pi = attach_plain();
    check( pi >= 0 && SDL_IsGameController( pi ) != SDL_TRUE, "a virtual joystick without a mapping can be attached" );
    SDL_Joystick* p = pi >= 0 ? SDL_JoystickOpen( pi ) : 0;
    pump_ms( 100 );
    check( joy_count() == base + 2 && joy_connected( base + 1 ) && strcmp( joy_name( base + 1 ), "Virtual Plain Joystick" ) == 0, "it gets the next slot" );
    if ( p != 0 ) {
        SDL_JoystickSetVirtualButton( p, 1, 1 );
        SDL_JoystickSetVirtualButton( p, 7, 1 );
        SDL_JoystickSetVirtualAxis( p, 0, -32768 );
        SDL_JoystickSetVirtualAxis( p, 3, 32767 );
        SDL_JoystickSetVirtualHat( p, 0, SDL_HAT_UP | SDL_HAT_LEFT );
        pump_ms( 40 );
        check( joy_buttons( base + 1 ) == ( JOYB_B | JOYB_START | JOYB_UP | JOYB_LEFT ), "fallback: buttons 1 and 7, the hat as the D-pad" );
        check( near( joy_axis( base + 1, JOYA_LX ), -1.0 ) && near( joy_axis( base + 1, JOYA_RY ), 1.0 ), "fallback: axes 0 and 3" );
        SDL_JoystickSetVirtualHat( p, 0, SDL_HAT_CENTERED );
        pump_ms( 30 );
        check( joy_buttons( base + 1 ) == ( JOYB_B | JOYB_START ), "the hat back in the middle" );
        joy_pressed( base + 1 ); joy_released( base + 1 );
    }
    check( joy_buttons( base ) == 0U, "the first pad was not touched by the second" );

    // unplugging
    uint32_t ev1 = joy_events();
    SDL_JoystickSetVirtualButton( g, 3, 1 );
    pump_ms( 30 );
    if ( p != 0 ) { SDL_JoystickClose( p ); }
    SDL_JoystickDetachVirtual( pi );
    pump_ms( 100 );
    check( joy_count() == base + 1 && !joy_connected( base + 1 ) && joy_connected( base ), "the second pad is gone, the first one stays" );
    check( joy_released( base + 1 ) == ( JOYB_B | JOYB_START ), "its held buttons were released" );
    check( joy_events() > ev1, "plugging out is an event" );
    int pi2 = attach_plain();
    SDL_Joystick* p2 = pi2 >= 0 ? SDL_JoystickOpen( pi2 ) : 0;
    pump_ms( 100 );
    check( joy_connected( base + 1 ) && joy_buttons( base + 1 ) == 0U, "a new pad takes the free slot, clean" );
    if ( p2 != 0 ) { SDL_JoystickClose( p2 ); }
    SDL_JoystickDetachVirtual( pi2 );
    SDL_JoystickClose( g );
    SDL_JoystickDetachVirtual( gi );
    pump_ms( 100 );
    check( joy_count() == base && joy_released( base ) == JOYB_Y, "all unplugged, the held button was released" );
    sdljoy_shutdown();
    SDL_Quit();
    printf( "%d of %d checks passed\n", total - failures, total );
    return failures;
}

static void monitor( void ) {
    SDL_SetHint( SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1" );
    if ( !init_sdl() ) {
        return;
    }
    printf( "Game controller monitor: plug a pad in, press its buttons, move its sticks. Ctrl-C ends.\n"
            "(%d joystick device(s) seen by SDL at the start)\n", SDL_NumJoysticks() );
    int lastcount = -1;
    uint32_t lastbtn[JOY_MAXPLAYERS] = { 0, 0, 0, 0 };
    double lastaxis[JOY_MAXPLAYERS][JOYA_COUNT];
    memset( lastaxis, 0, sizeof(lastaxis) );
    static const char* const bnames[JOYB_COUNT] = { "A", "B", "X", "Y", "LB", "RB", "BACK", "START", "GUIDE", "LS", "RS", "UP", "DOWN", "LEFT", "RIGHT" };
    static const char* const anames[JOYA_COUNT] = { "LX", "LY", "RX", "RY", "LT", "RT" };
    while ( !stop ) {
        pump();
        if ( joy_count() != lastcount ) {
            lastcount = joy_count();
            printf( "-- %d pad(s):", lastcount );
            for ( int s = 0; s < JOY_MAXPLAYERS; ++s ) {
                if ( joy_connected( s ) ) {
                    printf( "  player %d = \"%s\"", s + 1, joy_name( s ) );
                }
            }
            printf( "\n" );
            for ( int s = 0; s < JOY_MAXPLAYERS; ++s ) {
                lastbtn[s] = 0;
            }
        }
        for ( int s = 0; s < JOY_MAXPLAYERS; ++s ) {
            if ( !joy_connected( s ) ) {
                continue;
            }
            uint32_t b = joy_buttons( s );
            if ( b != lastbtn[s] ) {
                printf( "player %d buttons:", s + 1 );
                for ( int i = 0; i < JOYB_COUNT; ++i ) {
                    if ( b & ( 1U << i ) ) {
                        printf( " %s", bnames[i] );
                    }
                }
                printf( "%s\n", b == 0U ? " (none)" : "" );
                lastbtn[s] = b;
            }
            for ( int a = 0; a < JOYA_COUNT; ++a ) {
                double v = joy_axis( s, a );
                if ( v - lastaxis[s][a] > 0.1 || lastaxis[s][a] - v > 0.1 || ( v == 0.0 && lastaxis[s][a] != 0.0 ) ) {
                    printf( "player %d %s = %+.2f\n", s + 1, anames[a], v );
                    lastaxis[s][a] = v;
                }
            }
        }
        fflush( stdout );
        SDL_Delay( 10 );
    }
    sdljoy_shutdown();
    SDL_Quit();
}

int main( int argc, char** argv ) {
    if ( argc > 1 && strcmp( argv[1], "--selftest" ) == 0 ) {
        return selftest();
    }
    if ( argc > 1 ) {
        fprintf( stderr, "usage: %s [--selftest]\n", argv[0] );
        return 2;
    }
    monitor();
    return 0;
}
