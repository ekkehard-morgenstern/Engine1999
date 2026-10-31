
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

#include "ansi.h"

/*
Automated test of the ANSI escape sequence decoder (ansi.[ch]) without a screen. Each case feeds one or more texts and
compares what the decoder hands to the output callback, written as "[bg,fg]text" per run. Default colors are fg 1,
bg 0 (the text screen's). The exit status is the number of failed cases.
*/

typedef struct _testcase_t {
    const char* name;
    const char* text1;
    const char* text2;      // fed after text1 with the state kept (optional)
    const char* expected;
} testcase_t;

static const testcase_t cases[] = {
    { "plain text",                 "Hello",                    0,          "[0,1]Hello" },
    { "newline is text",            "A\nB",                     0,          "[0,1]A\nB" },
    { "foreground",                 "\033[31mX",                0,          "[0,44]X" },
    { "background",                 "\033[42mX",                0,          "[7,1]X" },
    { "both in one sequence",       "\033[31;42mX",             0,          "[7,44]X" },
    { "bright foreground",          "\033[91mX",                0,          "[0,110]X" },
    { "bright background",          "\033[107mX",               0,          "[140,1]X" },
    { "text before and after",      "A\033[32mB\033[0mC",       0,          "[0,1]A[0,7]B[0,1]C" },
    { "ESC [ m resets",             "\033[31mA\033[mB",         0,          "[0,44]A[0,1]B" },
    { "39 and 49 are the defaults", "\033[31;42mA\033[39mB\033[49mC", 0,   "[7,44]A[7,1]B[0,1]C" },
    { "256 color foreground",       "\033[38;5;200mX",          0,          "[0,200]X" },
    { "256 color background",       "\033[48;5;99mX",           0,          "[99,1]X" },
    { "256 color out of range",     "\033[38;5;300mX",          0,          "[0,1]X" },
    { "true color is skipped",      "\033[38;2;1;2;3;41mX",     0,          "[44,1]X" },
    { "unknown attributes",         "\033[1;4;31mX",            0,          "[0,44]X" },
    { "colors stay between texts",  "\033[34mA",                "B",        "[0,2]A[0,2]B" },
    { "sequence cut off",           "A\033[3",                  "1mB",      "[0,1]A[0,44]B" },
    { "ESC at the end",             "A\033",                    "[32mB",    "[0,1]A[0,7]B" },
    { "other CSI is skipped",       "A\033[2JB\033[10;5HC",     0,          "[0,1]A[0,1]B[0,1]C" },
    { "private sequence is skipped","\033[?25lA\033[?31mB",     0,          "[0,1]A[0,1]B" },
    { "other ESC is skipped",       "A\033cB",                  0,          "[0,1]AB" },
    { "aborted sequence",           "\033[3\nA",               0,          "[0,1]\nA" },
    { "empty parameter is 0",       "\033[31m\033[;42mX",       0,          "[7,1]X" },
    { "too many parameters",        "\033[0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;31mX", 0, "[0,1]X" },
    { "long number",                "\033[99999999999999m\033[31mX", 0,      "[0,44]X" },
};

typedef struct _collect_t {
    char text[2048];
} collect_t;

static void collect( void* usrdata, int bg, int fg, const char* text, size_t len ) {
    collect_t* c = (collect_t*) usrdata;
    size_t n = strlen( c->text );
    snprintf( c->text + n, sizeof(c->text) - n, "[%d,%d]%.*s", bg, fg, (int) len, text );
}

// like collect(), but a zero byte is shown as <0>
static void collect_esc( void* usrdata, int bg, int fg, const char* text, size_t len ) {
    collect_t* c = (collect_t*) usrdata;
    size_t n = strlen( c->text );
    n += (size_t) snprintf( c->text + n, sizeof(c->text) - n, "[%d,%d]", bg, fg );
    for ( size_t i=0; i < len && n + 4U < sizeof(c->text); ++i ) {
        n += (size_t) snprintf( c->text + n, sizeof(c->text) - n, text[i] == '\0' ? "<0>" : "%c", text[i] );
    }
}

int main( void ) {
    int failures = 0;
    for ( size_t i=0; i < sizeof(cases) / sizeof(cases[0]); ++i ) {
        const testcase_t* tc = &cases[i];
        ansi_t a; collect_t c; c.text[0] = '\0';
        ansi_init( &a, 1, 0 );
        ansi_feed( &a, tc->text1, collect, &c );
        if ( tc->text2 != 0 ) {
            ansi_feed( &a, tc->text2, collect, &c );
        }
        bool pass = strcmp( c.text, tc->expected ) == 0;
        printf( "%s  %s\n", pass ? "PASS" : "FAIL", tc->name );
        if ( !pass ) {
            printf( "      expected: %s\n      got:      %s\n", tc->expected, c.text );
            ++failures;
        }
    }
    // text longer than a run is handed over in pieces of ANSI_RUNMAX characters
    {
        char text[301], expected[2048];
        memset( text, 'x', 300 ); text[300] = '\0';
        snprintf( expected, sizeof(expected), "[0,1]%.*s[0,1]%.*s", ANSI_RUNMAX, text, 300 - ANSI_RUNMAX, text );
        ansi_t a; collect_t c; c.text[0] = '\0';
        ansi_init( &a, 1, 0 );
        ansi_feed( &a, text, collect, &c );
        bool pass = strcmp( c.text, expected ) == 0;
        printf( "%s  long text is split\n", pass ? "PASS" : "FAIL" );
        failures += pass ? 0 : 1;
    }
    // zero bytes are text: they reach the callback inside the run
    {
        ansi_t a; collect_t c; c.text[0] = '\0';
        ansi_init( &a, 1, 0 );
        ansi_feed_n( &a, "A\0B\033[31mC", 9, collect_esc, &c );
        bool pass = strcmp( c.text, "[0,1]A<0>B[0,44]C" ) == 0;
        printf( "%s  zero bytes in a run (length based)\n", pass ? "PASS" : "FAIL" );
        failures += pass ? 0 : 1;
    }
    printf( "%d failure(s)\n", failures );
    return failures;
}
