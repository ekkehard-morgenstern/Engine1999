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

// Automated test of the game controller state (joy.c, no SDL, no device): slots, buttons and their latches, axes and the dead zone, the event counter, names.

#include "stdtypes.h"
#include "joy.h"

static int total = 0, failures = 0;

static void check( bool cond, const char* name ) {
    ++total;
    if ( !cond ) {
        ++failures;
        printf( "FAIL  %s\n", name );
    }
}

static bool near( double a, double b ) { return a - b < 0.0005 && b - a < 0.0005; }

static void test_slots( void ) {
    joy_init();
    check( joy_count() == 0 && !joy_connected( 0 ) && joy_events() == 0U, "no pad at the start" );
    check( joy_buttons( 0 ) == 0U && joy_axis( 0, JOYA_LX ) == 0.0 && joy_pressed( 0 ) == 0U && joy_released( 0 ) == 0U, "a slot without a pad reads zero" );
    check( strcmp( joy_name( 0 ), "" ) == 0 && !joy_connected( -1 ) && !joy_connected( 4 ) && joy_buttons( 9 ) == 0U, "bad slots read zero, no crash" );
    check( joy_attach( "Pad One" ) == 0 && joy_attach( "Pad Two" ) == 1 && joy_count() == 2, "pads take the lowest free slots" );
    check( strcmp( joy_name( 1 ), "Pad Two" ) == 0 && joy_events() == 2U, "the name and the plug-in events" );
    check( joy_attach( "Three" ) == 2 && joy_attach( "Four" ) == 3 && joy_attach( "Five" ) == -1 && joy_count() == 4, "four players, no fifth" );
    joy_detach( 1 );
    check( !joy_connected( 1 ) && joy_connected( 0 ) && joy_connected( 2 ) && joy_count() == 3, "unplugging frees one slot, the others keep their numbers" );
    check( joy_attach( "Again" ) == 1 && strcmp( joy_name( 1 ), "Again" ) == 0, "the next pad takes the free slot" );
    uint32_t ev = joy_events();
    joy_detach( 1 ); joy_detach( 1 ); joy_detach( 7 );
    check( joy_events() == ev + 1U, "unplugging a pad that is not there is nothing" );
    joy_init();
    check( joy_count() == 0, "joy_init forgets the pads" );
}

static void test_buttons( void ) {
    joy_init();
    joy_attach( "P" );
    uint32_t e0 = joy_events();
    joy_event_button( 0, JOYB_A, true );
    check( joy_buttons( 0 ) == JOYB_A && joy_events() == e0 + 1U, "a button press" );
    joy_event_button( 0, JOYB_A, true );
    check( joy_events() == e0 + 1U, "the same press again is no event" );
    joy_event_button( 0, JOYB_START | JOYB_UP, true );
    check( joy_buttons( 0 ) == ( JOYB_A | JOYB_START | JOYB_UP ), "several buttons at once" );
    check( joy_pressed( 0 ) == ( JOYB_A | JOYB_START | JOYB_UP ) && joy_pressed( 0 ) == 0U, "pressed is latched and cleared by reading" );
    joy_event_button( 0, JOYB_A, false );
    check( joy_buttons( 0 ) == ( JOYB_START | JOYB_UP ) && joy_released( 0 ) == JOYB_A && joy_released( 0 ) == 0U, "a release is latched, too" );
    joy_event_button( 0, JOYB_B, false );
    check( joy_released( 0 ) == 0U, "releasing a button that was not down is nothing" );
    joy_event_button( 0, JOYB_X, true ); joy_event_button( 0, JOYB_X, false );
    check( joy_buttons( 0 ) == ( JOYB_START | JOYB_UP ) && joy_pressed( 0 ) == JOYB_X && joy_released( 0 ) == JOYB_X,
        "a quick press and release between two reads is not lost" );
    joy_event_button( 1, JOYB_A, true );
    joy_event_button( 0, 0U, true );
    joy_event_button( 0, 0x80000000U, true );
    check( joy_buttons( 1 ) == 0U && joy_buttons( 0 ) == ( JOYB_START | JOYB_UP ), "events for a slot without a pad and for no button are ignored" );
    uint32_t e1 = joy_events();
    joy_detach( 0 );
    check( joy_buttons( 0 ) == 0U && joy_released( 0 ) == ( JOYB_START | JOYB_UP ) && joy_events() == e1 + 1U, "unplugging releases the buttons (latched)" );
    joy_attach( "Q" );
    check( joy_buttons( 0 ) == 0U && joy_pressed( 0 ) == 0U, "a new pad starts clean" );
    joy_event_button( 0, JOYB_A, true );                // (left in the latches, not read)
    joy_detach( 0 );
    joy_attach( "R" );
    check( joy_buttons( 0 ) == 0U && joy_pressed( 0 ) == 0U && joy_released( 0 ) == 0U, "... also of what the pad before it left in the latches" );
}

static void test_axes( void ) {
    joy_init();
    joy_attach( "P" );
    check( near( joy_getdeadzone(), 0.2 ), "the default dead zone" );
    joy_event_axis( 0, JOYA_LX, 32767 );
    check( near( joy_axis( 0, JOYA_LX ), 1.0 ), "full right" );
    joy_event_axis( 0, JOYA_LX, -32768 );
    check( near( joy_axis( 0, JOYA_LX ), -1.0 ), "full left" );
    joy_event_axis( 0, JOYA_LY, 3000 );
    check( joy_axis( 0, JOYA_LY ) == 0.0, "inside the dead zone it is zero" );
    joy_event_axis( 0, JOYA_LY, -6000 );
    check( joy_axis( 0, JOYA_LY ) == 0.0, "(0.18 is still inside 0.2)" );
    joy_event_axis( 0, JOYA_RX, 16384 );
    check( near( joy_axis( 0, JOYA_RX ), ( 0.5 - 0.2 ) / 0.8 ), "the rest of the way is scaled to the full range: 0.5 becomes 0.375" );
    joy_event_axis( 0, JOYA_RY, -16384 );
    check( near( joy_axis( 0, JOYA_RY ), -( 0.5 - 0.2 ) / 0.8 ), "the same on the negative side" );
    joy_setdeadzone( 0.0 );
    joy_event_axis( 0, JOYA_LY, 3277 );
    check( near( joy_axis( 0, JOYA_LY ), 0.1 ), "no dead zone: the raw value" );
    joy_setdeadzone( 5.0 );
    check( near( joy_getdeadzone(), 0.9 ), "the dead zone is limited to 0.9" );
    joy_setdeadzone( -1.0 );
    check( joy_getdeadzone() == 0.0, "and not below 0" );
    joy_setdeadzone( 0.2 );
    joy_event_axis( 0, JOYA_LT, 32767 ); joy_event_axis( 0, JOYA_RT, 16384 );
    check( near( joy_axis( 0, JOYA_LT ), 1.0 ) && near( joy_axis( 0, JOYA_RT ), 0.5 ), "triggers are 0 .. 1 and have no dead zone" );
    joy_event_axis( 0, JOYA_LT, -5000 );
    check( joy_axis( 0, JOYA_LT ) == 0.0, "a trigger is never negative" );
    joy_event_axis( 0, JOYA_LX, 99999 ); joy_event_axis( 0, JOYA_RX, -99999 );
    check( near( joy_axis( 0, JOYA_LX ), 1.0 ) && near( joy_axis( 0, JOYA_RX ), -1.0 ), "out of range raw values are clamped" );
    joy_event_axis( 0, 6, 100 ); joy_event_axis( 0, -1, 100 ); joy_event_axis( 2, JOYA_LX, 100 );
    check( joy_axis( 0, 6 ) == 0.0 && joy_axis( 0, -1 ) == 0.0 && joy_axis( 2, JOYA_LX ) == 0.0, "bad axes and slots are ignored" );
    uint32_t ev = joy_events();
    joy_event_axis( 0, JOYA_LX, 20000 );
    check( joy_events() == ev, "moving a stick is no event" );
    joy_detach( 0 );
    check( joy_axis( 0, JOYA_LX ) == 0.0, "an unplugged pad reads zero" );
    joy_attach( "R" );
    check( joy_axis( 0, JOYA_LX ) == 0.0 && joy_axis( 0, JOYA_RT ) == 0.0, "a new pad starts with the sticks in the middle" );
}

static void test_names( void ) {
    check( joy_button_by_name( "A", 1U ) == (int) JOYB_A && joy_button_by_name( "right", 5U ) == (int) JOYB_RIGHT && joy_button_by_name( "Guide", 5U ) == (int) JOYB_GUIDE, "button names, any case" );
    check( joy_button_by_name( "LX", 2U ) == 0 && joy_button_by_name( "", 0U ) == 0 && joy_button_by_name( "AA", 2U ) == 0, "no such button" );
    check( joy_button_by_name( "UP", 2U ) == (int) JOYB_UP && joy_button_by_name( "START", 5U ) == (int) JOYB_START && joy_button_by_name( "BACK", 4U ) == (int) JOYB_BACK, "more buttons" );
    unsigned all = 0;
    static const char* const names[] = { "A", "B", "X", "Y", "LB", "RB", "BACK", "START", "GUIDE", "LS", "RS", "UP", "DOWN", "LEFT", "RIGHT", "SHARE" };
    for ( size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i ) {
        all |= (unsigned) joy_button_by_name( names[i], strlen( names[i] ) );
    }
    check( all == JOYB_ALL && JOYB_COUNT == 16 && joy_button_by_name( "share", 5U ) == (int) JOYB_SHARE, "the 16 names make the 16 bits" );
    check( joy_axis_by_name( "lx", 2U ) == JOYA_LX && joy_axis_by_name( "RT", 2U ) == JOYA_RT && joy_axis_by_name( "LY", 2U ) == JOYA_LY, "axis names" );
    check( joy_axis_by_name( "A", 1U ) == -1 && joy_axis_by_name( "", 0U ) == -1, "no such axis" );
}

int main( void ) {
    test_slots();
    test_buttons();
    test_axes();
    test_names();
    printf( "%d of %d checks passed\n", total - failures, total );
    return failures;
}
