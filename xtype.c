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

// Test helper of hosttest.sh: types a text into the X display with the XTest extension (as a keyboard would), one key every 60 ms.
// Usage: DISPLAY=:N ./xtype 'text with \n for Return'. Only the characters the tests need: letters, digits, space, quotes, period, slash, minus, underscore, the byte 1 = the Print key and the byte 2 = Ctrl+Shift+P.

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main( int argc, char** argv ) {
    if ( argc != 2 ) {
        fprintf( stderr, "usage: xtype 'text'\n" );
        return 2;
    }
    Display* d = XOpenDisplay( 0 );
    if ( d == 0 ) {
        fprintf( stderr, "xtype: cannot open the display\n" );
        return 1;
    }
    int bad = 0;
    for ( const char* p = argv[1]; *p; ++p ) {
        KeySym ks = NoSymbol;
        bool shift = false, ctrl = false;
        switch ( *p ) {
            case '\n': ks = XK_Return; break;
            case '\001': ks = XK_Print; break;       // the screenshot key
            case '\002': ks = XK_p; shift = true; ctrl = true; break;       // Ctrl+Shift+P, the other screenshot key
            case ' ':  ks = XK_space; break;
            case '"':  ks = XK_quotedbl; shift = true; break;
            case '.':  ks = XK_period; break;
            case '/':  ks = XK_slash; break;
            case '-':  ks = XK_minus; break;
            case '_':  ks = XK_underscore; shift = true; break;
            default: {
                char name[2] = { *p, 0 };
                ks = XStringToKeysym( name );
                shift = *p >= 'A' && *p <= 'Z';
                break;
            }
        }
        KeyCode kc = ks == NoSymbol ? 0 : XKeysymToKeycode( d, ks );
        if ( kc == 0 ) {
            fprintf( stderr, "xtype: no key for '%c'\n", *p );
            ++bad;
            continue;
        }
        KeyCode sh = XKeysymToKeycode( d, XK_Shift_L );
        KeyCode ct = XKeysymToKeycode( d, XK_Control_L );
        if ( ctrl ) XTestFakeKeyEvent( d, ct, True, 0 );
        if ( shift ) XTestFakeKeyEvent( d, sh, True, 0 );
        XTestFakeKeyEvent( d, kc, True, 0 );
        XTestFakeKeyEvent( d, kc, False, 0 );
        if ( shift ) XTestFakeKeyEvent( d, sh, False, 0 );
        if ( ctrl ) XTestFakeKeyEvent( d, ct, False, 0 );
        XFlush( d );
        usleep( 60000 );
    }
    XCloseDisplay( d );
    return bad != 0;
}
