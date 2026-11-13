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

#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include "basrepl.h"
#include "basdebug.h"
#include "gfxops.h"
#include "w3d.h"
#include "jsonops.h"
#include "w3dworld.h"
#include "mouse.h"
#include "cursors.h"
#include "sprscreen.h"
#include "ansi.h"
#include "scrdrv.h"
#include "conscr.h"
#include "textscreen.h"
#include "fulledit.h"
#include "monitor.h"
#include "policy.h"
#include "keyseq.h"
#include "clipboard.h"
#include "keystate.h"
#include "version.h"

static char* con_vm_clipget( struct _vm_t* vm, void* usrdata, size_t* len );        // CLIPBOARD$ / CLIPBOARD a$ (defined with the clipboard of the editors)
static void con_vm_clipset( struct _vm_t* vm, void* usrdata, const char* text, size_t len );

/*
The console host of the Engine: the BASIC system without a window, for shell scripts and servers (docs/console-host.md).

    engine1999c [options] file [arguments]     loads the program file (.bas listing, .e99 binary; a first line #! is skipped), runs it
                                               and ends; the exit status is the code of BYE / QUIT, 0 when the program just ends, 1 after
                                               an error, 2 if the file can't be loaded
    engine1999c [options] - [arguments]        reads BASIC lines from standard input (see below)
    engine1999c [options]                      the same, interactively when standard input is a terminal: a banner and "Ready."

Lines from standard input are entered like typed lines: numbered lines are stored, the others run at once; the INPUT, GET and INKEY$
of a running program read the same input. Errors and messages go to standard error, program output to standard output. Ctrl-C is
Break while a program runs. No SDL: sound statements do nothing (their arguments are checked), CLS and LOCATE use ANSI escape sequences
when standard output is a terminal, the other display statements fail with "Not available in the console host".
*/

// -- signals and the terminal ---------------------------------------------------------------

static volatile sig_atomic_t con_break = 0;

static void con_sigint( int sig ) {
    (void) sig;
    con_break = 1;
}

static bool con_fullscreen = false;             // the BASIC session or the full-screen editor / monitor is on the text screen: program output and messages go there
static bool con_screen = false;                 // screen mode: the whole session runs on text screen 0 on the alternate screen (see "Screen mode" below)
static int  con_screenopt = -1;                 // -1 automatic, 0 --line, 1 --screen
static bool con_dump = true;                    // screen mode: the screen is written to the normal terminal when the session ends
static bool con_dirty = false;                  // the text screen was changed and not painted yet
static struct timespec con_lastpaint;
static void con_screen_end( bool dump );
static void con_flush_paint( void );
static bool con_pumping = false;                // a program is running on a terminal: look for keys (see con_pump())
static bool con_escbreak = true;                // ESC is the break key (BREAK ON); with BREAK OFF it is an ordinary key (Ctrl-C always breaks)
static size_t con_inqpos = 0, con_inqlen = 0;   // the keys that arrived while it ran (see con_pump())
static void con_set_raw( bool raw );
static bool con_mouse = false;                  // screen mode on a terminal with the mouse reported (see "The mouse" below)
static void con_mouse_poll( void );
static ssize_t con_mouse_filter( unsigned char* b, ssize_t n, size_t cap );

// Ctrl-C is Break while the BASIC system runs something; between lines the default action (end the process) is in effect. On a terminal a lone ESC
// is Break as well (as in the window), which needs the keys to arrive without the Return of the line mode: the terminal is in raw mode while a
// program runs, and the keys typed meanwhile are queued for it (con_pump())
static void con_catch_break( bool on ) {
    struct sigaction sa;
    memset( &sa, 0, sizeof(sa) );
    sa.sa_handler = on || con_screen ? con_sigint : SIG_DFL;      // no SA_RESTART: a blocked read ends and the VM sees the Break (screen mode: the terminal must be left in order, so always)
    sigemptyset( &sa.sa_mask );
    sigaction( SIGINT, &sa, 0 );
    if ( on ) {
        con_break = 0;
    }
    con_escbreak = true;                        // a run starts and ends with BREAK ON
    if ( !on ) {
        con_inqpos = con_inqlen = 0;            // the keys typed ahead during the run are dropped, as in the window
    }
    con_pumping = on && isatty( 0 ) && isatty( 1 );
    if ( con_pumping ) {
        con_set_raw( true );
    }
}

static struct termios con_saved_tio;
static bool con_tio_saved = false, con_raw = false;

// GET and INKEY$ on a terminal take single keys without echo; lines (INPUT, the REPL) the usual way
static void con_set_raw( bool raw ) {
    if ( !isatty( 0 ) || raw == con_raw || ( !raw && con_screen ) ) {      // (screen mode: the terminal stays raw until the session ends)
        return;
    }
    if ( !con_tio_saved ) {
        if ( tcgetattr( 0, &con_saved_tio ) != 0 ) {
            return;
        }
        con_tio_saved = true;
    }
    struct termios t = con_saved_tio;
    if ( raw ) {
        t.c_lflag &= ~( (tcflag_t) ( ICANON | ECHO ) );     // ISIG stays: Ctrl-C is still a signal
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
    }
    if ( tcsetattr( 0, TCSANOW, raw ? &t : &con_saved_tio ) == 0 ) {
        con_raw = raw;
    }
}

static void con_restore_tty( void ) {
    con_screen_end( false );            // (does nothing outside of screen mode)
    con_set_raw( false );
}

// -- host callbacks -----------------------------------------------------------------------

static bool con_stdout_tty = false;
static bool con_color = false;
static bool con_midline = false;        // the last character written to standard output was not a newline
static int con_col = 0;                 // the column after the last output (a guess: printable bytes since the last newline), for a terminal that does not answer ESC [ 6 n

// Painting the screen is not done per character: a burst of output is drawn at most every 10 ms, and always when the program waits for a key, a line
// (con_flush_paint()) and when it ends.
static volatile sig_atomic_t con_winch = 0;     // the terminal was resized (SIGWINCH)
static bool con_wantresize;                     // an editor is waiting for keys: it is told about a resize (-3 / SK_RESIZE)

static void con_paint_now( void ) {
    if ( con_winch && !con_wantresize ) {       // (an editor that waits for keys learns of the resize itself)
        con_winch = 0;
        conscr_syncsize();
    }
    conscr_paint();
    con_dirty = false;
    clock_gettime( CLOCK_MONOTONIC, &con_lastpaint );
}

static void con_paint_soon( void ) {
    con_dirty = true;
    struct timespec now;
    clock_gettime( CLOCK_MONOTONIC, &now );
    long ms = (long)( now.tv_sec - con_lastpaint.tv_sec ) * 1000L + ( now.tv_nsec - con_lastpaint.tv_nsec ) / 1000000L;
    if ( ms >= 10L || ms < 0 ) {
        con_paint_now();
    }
}

static void con_flush_paint( void ) {
    if ( con_fullscreen && con_dirty ) {
        con_paint_now();
    }
}

// Text is ISO-8859-15 inside the Engine. On a terminal (which speaks UTF-8) the characters from 128 up are converted; in a pipe or a file the bytes
// go out as they are (binary output must stay intact), and so they come in: see con_readkey().
static void con_write( const char* text, size_t len ) {
    if ( con_fullscreen ) {
        conscr_write( text, len );
        con_paint_soon();
        return;
    }
    bool high = false;
    for ( size_t i = 0; i < len; ++i ) {                  // (the column: printable characters since the last newline; escape sequences are skipped roughly)
        unsigned char ch = (unsigned char) text[i];
        if ( ch == '\n' ) con_col = 0;
        else if ( ch == 033 ) { while ( i + 1U < len && !( text[i + 1U] >= 0x40 && text[i + 1U] <= 0x7E && text[i + 1U] != '[' ) ) ++i; if ( i + 1U < len ) ++i; }
        else if ( ch >= 0x20 ) ++con_col;
        if ( ch >= 0x80 && con_stdout_tty ) high = true;
    }
    if ( high ) {
        char* u = (char*) malloc( 3U * len );
        if ( u != 0 ) {
            fwrite( u, 1U, keyseq_latin9_to_utf8( text, len, u ), stdout );
            free( u );
            return;
        }
    }
    fwrite( text, 1U, len, stdout );
}

static void con_print( struct _vm_t* vm, void* usrdata, int channel, const char* text ) {
    (void) vm; (void) usrdata;
    if ( channel == 0 && text[0] != '\0' ) {
        size_t len = strlen( text );
        con_write( text, len );
        con_midline = text[ len - 1U ] != '\n';
    }
}

// binary-safe output: the zero bytes of CHR$(0) reach the output as they are
static void con_printn( struct _vm_t* vm, void* usrdata, int channel, const char* text, size_t len ) {
    (void) vm; (void) usrdata;
    if ( channel == 0 && len > 0 ) {
        con_write( text, len );
        con_midline = text[ len - 1U ] != '\n';
    }
}

static void con_report( struct _repl_t* repl, const char* text ) {
    (void) repl;
    if ( con_fullscreen ) {
        conscr_write( text, strlen( text ) );
        con_paint_soon();
        return;
    }
    if ( con_midline && con_stdout_tty ) {      // on a terminal the message starts on a line of its own (not in a pipe: that is data)
        fputc( '\n', stdout );
        con_midline = false;
    }
    fflush( stdout );           // keep the order of the two streams when both go to the same place
    fputs( text, stderr );
}

#define CON_PROMPTMAX   4095U       // the longest line at the prompt (the window's: PROMPT_BUFSIZE - 1 in engine1999.c)
static ssize_t con_getline( char** line, size_t* cap, size_t maxlen, bool input );
static char* con_edit( struct _repl_t* repl, const char* text );

// INPUT: a line from standard input; the prompt has been printed already, so it is flushed first
static char* con_input( struct _vm_t* vm, void* usrdata, int channel ) {
    (void) vm; (void) usrdata;
    if ( channel != 0 ) {
        return 0;
    }
    fflush( stdout );
    con_set_raw( false );
    char* line = 0; size_t cap = 0;
    ssize_t n = con_getline( &line, &cap, 1023U, true );
    if ( n < 0 ) {
        free( line );
        clearerr( stdin );
        return 0;
    }
    while ( n > 0 && ( line[ n - 1 ] == '\n' || line[ n - 1 ] == '\r' ) ) {
        line[ --n ] = '\0';
    }
    return line;
}

// Keys from the terminal, as the window delivers them: RETURN 13, BACKSPACE 8, TAB 9, the printable characters (UTF-8 from a terminal becomes
// ISO-8859-15; from a pipe the bytes stay as they are), and the cursor and function keys as the canonical escape sequence (keyseq.h), whatever
// dialect the terminal speaks: the bytes after an ESC are collected until they make a known sequence (keyseq_parse()), which is delivered in its
// canonical form, byte by byte. Unknown CSI / SS3 sequences (mouse reports, ...) are skipped. An ESC that is not followed by more bytes within
// 50 ms, or by something that is no sequence, is a plain ESC (the bytes behind it are delivered as they are).
// 0 = none yet (INKEY$), -1 = end of input / Break; wait: block until there is a key.

#define CON_ESCWAIT     50      // ms to wait for the rest of an escape sequence

static bool con_wantctrl = false;       // the line editor wants the control codes (Ctrl-A, Ctrl-E, ...), too
static char con_pend[ KEYSEQ_MAXLEN * 2 ];
static size_t con_pendpos = 0, con_pendlen = 0;

// Keys that arrived while the program was running (the window queues them the same way): the terminal is looked at from time to time
// (con_pump(), called by the interrupt check) and the bytes wait here until a program reads a key. A lone ESC is not queued: it is the
// break key (like Ctrl-C), as in the window.
static unsigned char con_inq[1024];
static void con_sigwinch( int sig ) {
    (void) sig;
    con_winch = 1;
}

// KEY("name") on a terminal: DISABLED (2026-11-01, the owner's decision after trying it: the state flickers, which is not acceptable). KEY() is an error in
// this host ("KEY is not available in this host"); the code below is kept for the owner's review, it is compiled only with -DCON_KEYSTATE (and then
// repl->keydown must be set again in con_init()). The idea: a terminal tells when a key is pressed, never when it is released, so a key counts as held for a short time after the
// last byte it sent (the keyboard's auto repeat keeps it going; its first repeat comes after the repeat delay, usually 250 to 500 ms, so a key that is held
// flickers once at the start: the window version has the real state). Letters typed with Shift are noted with SHIFT, Ctrl+letter with CTRL. Only
// bytes seen by con_pump() are noted (while a program runs on a terminal).
#ifdef CON_KEYSTATE
#define CON_KEYHOLD     140         // ms

static uint64_t con_keyseen[KEYID_MAX];

static uint64_t con_now_ms( void ) {
    struct timespec ts;
    clock_gettime( CLOCK_MONOTONIC, &ts );
    return (uint64_t) ts.tv_sec * 1000U + (uint64_t) ts.tv_nsec / 1000000U + 1U;     // (never 0: 0 is "never seen")
}

static void con_note_key( int id, uint64_t now ) {
    if ( id > 0 && id < KEYID_MAX ) {
        con_keyseen[id] = now;
        con_keyseen[KEYID_ANY] = now;
    }
}

static void con_note_bytes( const unsigned char* b, ssize_t n ) {
    uint64_t now = con_now_ms();
    for ( ssize_t i = 0; i < n; ++i ) {
        unsigned char c = b[i];
        if ( c == 033 ) {
            int key, mods;
            int r = keyseq_parse( (const char*) b + i, (size_t)( n - i ), &key, &mods );
            if ( r > 0 ) {
                con_note_key( KEYID_KS + key, now );
                if ( mods & KSMOD_SHIFT ) { con_note_key( KEYID_SHIFT, now ); }
                if ( mods & KSMOD_CTRL ) { con_note_key( KEYID_CTRL, now ); }
                if ( mods & KSMOD_ALT ) { con_note_key( KEYID_ALT, now ); }
                i += (ssize_t) r - 1;
            } else {
                con_note_key( KEYID_ESC, now );
            }
        } else if ( c == '\r' || c == '\n' ) {
            con_note_key( KEYID_RETURN, now );
        } else if ( c == 8 || c == 127 ) {
            con_note_key( KEYID_BACKSPACE, now );
        } else if ( c == '\t' ) {
            con_note_key( KEYID_TAB, now );
        } else if ( c >= 1 && c <= 26 ) {
            con_note_key( 'a' + c - 1, now );            // Ctrl+letter
            con_note_key( KEYID_CTRL, now );
        } else if ( c >= 32 && c < 127 ) {
            if ( c >= 'A' && c <= 'Z' ) {
                con_note_key( KEYID_SHIFT, now );
                c = (unsigned char)( c - 'A' + 'a' );
            }
            con_note_key( c, now );
        }
    }
}

#endif      // CON_KEYSTATE

// -- the mouse ----------------------------------------------------------------------------
//
// In screen mode on a terminal the mouse is reported by the terminal (conscr_mouse(): xterm modes 1003 and 1006, SGR coordinates):
//     ESC [ < b ; col ; row M   (press, motion, wheel)      ESC [ < b ; col ; row m   (release)
// b: 0 left, 1 middle, 2 right, 3 none (motion); +4 Shift, +8 Meta, +16 Ctrl, +32 motion, +64 wheel (64 up, 65 down; 66 / 67 sideways: ignored).
// col and row are cells (from 1); a cell is 8 x 12 pixels of the "area" that mouse.h works in, the centre of the cell is the position the program sees.
// The reports come in the stream of the keys: con_mouse_filter() takes them out of what was read and gives them to mouse.c; a program polls the
// state with MOUSE(), the pointer is shown by conscr (a cell with swapped colors) and its shape is the terminal's own (OSC 22). A report that is cut in two
// reads is waited for briefly; one that was split before its third byte reaches the key reader and is skipped there like any unknown sequence.

static uint64_t con_mouse_ms( void ) {
    struct timespec ts;
    clock_gettime( CLOCK_MONOTONIC, &ts );
    return (uint64_t) ts.tv_sec * 1000U + (uint64_t) ts.tv_nsec / 1000000U;
}

// the mouse report at b[i]: its length (> 0) and the numbers, 0 if there is none, -1 if it may be one but is not complete
static ssize_t con_mouse_scan( const unsigned char* b, ssize_t n, ssize_t i, int* cb, int* cx, int* cy, char* fin ) {
    if ( n - i < 3 || b[i] != 033 || b[i + 1] != '[' || b[i + 2] != '<' ) {
        return 0;
    }
    int v[3] = { 0, 0, 0 };
    int k = 0;
    bool digit = false;
    for ( ssize_t j = i + 3; j < n; ++j ) {
        unsigned char c = b[j];
        if ( c >= '0' && c <= '9' ) {
            v[k] = v[k] * 10 + ( c - '0' );
            if ( v[k] > 100000 ) {
                return 0;
            }
            digit = true;
        } else if ( c == ';' && digit && k < 2 ) {
            ++k;
            digit = false;
        } else if ( ( c == 'M' || c == 'm' ) && digit && k == 2 ) {
            *cb = v[0]; *cx = v[1]; *cy = v[2]; *fin = (char) c;
            return j - i + 1;
        } else {
            return 0;
        }
    }
    return -1;
}

// Events for the full-screen editor (scrdrv.h SK_MOUSE; con_drv.wantmouse): the left button and the wheel, in a small ring that con_drv_nextkey() empties.
// A move with the button held replaces a move that is still waiting.
static bool con_wantmouse = false;
static scrkey_t con_mq[32];
static unsigned con_mqr = 0, con_mqn = 0;
static bool con_lbdown = false;                 // the left button is held: SM_REPEAT after SM_REPEAT_DELAY_MS and every SM_REPEAT_MS, at the cell the pointer is on
static uint64_t con_lbnext = 0;
static int con_lbx = 1, con_lby = 1;

static void con_mq_push( int mouse, int cx, int cy, int button, int wheel ) {
    if ( !con_wantmouse ) {
        return;
    }
    if ( mouse == SM_DRAG && con_mqn > 0 && con_mq[ ( con_mqr + con_mqn - 1U ) % 32U ].mouse == SM_DRAG ) {
        --con_mqn;                              // (only the last place counts)
    }
    if ( con_mqn >= 32U ) {
        return;
    }
    scrkey_t* k = &con_mq[ ( con_mqr + con_mqn++ ) % 32U ];
    memset( k, 0, sizeof(*k) );
    k->kind = SK_MOUSE; k->mouse = mouse; k->mx = cx - 1; k->my = cy - 1; k->button = button; k->wheel = wheel;
}

static void con_mouse_report( int cb, int cx, int cy, char fin ) {
    if ( con_wantmouse && ( cb & 64 ) == 0 && ( ( cb & 3 ) == 0 || ( cb & 32 ) != 0 ) ) {
        if ( ( cb & 32 ) == 0 ) {                           // the left button went down / up
            con_lbdown = fin == 'M';
            con_lbnext = con_mouse_ms() + SM_REPEAT_DELAY_MS;
        }
        con_lbx = cx; con_lby = cy;
    }
    if ( ( cb & 64 ) != 0 ) {
        if ( ( cb & 3 ) == 0 ) con_mq_push( SM_WHEEL, cx, cy, 0, 1 );
        else if ( ( cb & 3 ) == 1 ) con_mq_push( SM_WHEEL, cx, cy, 0, -1 );
    } else if ( ( cb & 32 ) != 0 ) {
        if ( ( cb & 3 ) == 0 ) con_mq_push( SM_DRAG, cx, cy, 1, 0 );
    } else if ( ( cb & 3 ) == 0 ) {
        con_mq_push( fin == 'M' ? SM_PRESS : SM_RELEASE, cx, cy, 1, 0 );
    }
    mouse_event_motion( ( cx - 1 ) * 8 + 4, ( cy - 1 ) * 12 + 6 );
    if ( ( cb & 64 ) != 0 ) {
        if ( ( cb & 3 ) == 0 ) mouse_event_wheel( 1 );
        else if ( ( cb & 3 ) == 1 ) mouse_event_wheel( -1 );
    } else if ( ( cb & 32 ) == 0 ) {
        int bit = ( cb & 3 ) == 0 ? MOUSE_LEFT : ( cb & 3 ) == 1 ? MOUSE_MIDDLE : ( cb & 3 ) == 2 ? MOUSE_RIGHT : 0;
        mouse_event_button( bit, fin == 'M' );
    }
}

static ssize_t con_mouse_filter( unsigned char* b, ssize_t n, size_t cap ) {
    ssize_t o = 0, i = 0;
    int waits = 0;
    while ( i < n ) {
        if ( b[i] != 033 ) {
            b[o++] = b[i++];
            continue;
        }
        int cb, cx, cy;
        char fin;
        ssize_t len = con_mouse_scan( b, n, i, &cb, &cx, &cy, &fin );
        if ( len > 0 ) {
            con_mouse_report( cb, cx, cy, fin );
            i += len;
            continue;
        }
        if ( len < 0 && waits < 2 && (size_t) n < cap ) {
            // a report that is cut off at the end of the read: wait a moment for the rest, which is appended behind the data
            ++waits;
            struct pollfd pfd = { 0, POLLIN, 0 };
            if ( poll( &pfd, 1, 20 ) > 0 ) {
                ssize_t m = read( 0, b + n, cap - (size_t) n );
                if ( m > 0 ) {
                    n += m;
                    continue;
                }
            }
        }
        b[o++] = b[i++];
    }
    return o;
}

// the clock of the pointer's 3 seconds and the picture: called whenever the host looks at the terminal
static void con_mouse_poll( void ) {
    if ( !con_mouse ) {
        return;
    }
    int w = 80, h = 25;
    txtscr_getscreensize( &w, &h );
    mouse_setarea( w * 8, h * 12 );
    mouse_tick( con_mouse_ms() );
    int x = 0, y = 0;
    cursors_position( &x, &y );
    conscr_setpointer( cursors_visible(), x / 8, y / 12 );
    conscr_pointer_shape( cursors_shape() );
    if ( conscr_pointer_changed() ) {
        con_paint_soon();
    }
}

// the repeats of a held left button (for the full-screen editor); called whenever the host waits for the terminal
static void con_mouse_repeat( void ) {
    if ( !con_lbdown ) {
        return;
    }
    if ( !con_wantmouse ) {
        con_lbdown = false;
        return;
    }
    uint64_t now = con_mouse_ms();
    if ( now >= con_lbnext ) {
        if ( con_mqn == 0 ) {
            con_mq_push( SM_REPEAT, con_lbx, con_lby, 1, 0 );
        }
        con_lbnext = now + SM_REPEAT_MS;
    }
}

static void con_pump( void ) {
    if ( !con_pumping ) {
        return;
    }
    unsigned char chunk[256];
    struct pollfd pfd = { 0, POLLIN, 0 };
    while ( poll( &pfd, 1, 0 ) > 0 && ( pfd.revents & POLLIN ) != 0 ) {
        ssize_t n = read( 0, chunk, sizeof(chunk) );
        if ( n <= 0 ) {
            break;
        }
        if ( con_mouse ) {
            n = con_mouse_filter( chunk, n, sizeof(chunk) );      // (the mouse reports are taken out; what is left is keys)
            if ( n <= 0 ) {
                continue;
            }
        }
#ifdef CON_KEYSTATE
        con_note_bytes( chunk, n );
#endif
        if ( n == 1 && chunk[0] == 033 && con_escbreak ) {
            con_break = 1;                      // a lone ESC
            continue;
        }
        for ( ssize_t i = 0; i < n && con_inqlen < sizeof(con_inq); ++i ) {
            con_inq[ con_inqlen++ ] = chunk[i];
        }
    }
    con_mouse_poll();
}

// one byte of the terminal: >= 0 the byte, -1 end of input / error, -2 timeout (timeout in ms; negative = wait)
static int con_rawbyte( int timeout ) {
    for (;;) {
        if ( con_break ) {
            return -1;
        }
        if ( con_inqpos < con_inqlen ) {
            unsigned char q = con_inq[ con_inqpos++ ];
            if ( con_inqpos == con_inqlen ) {
                con_inqpos = con_inqlen = 0;
            }
            return q;
        }
        con_mouse_repeat();
        if ( ( con_wantresize && con_winch ) || con_mqn > 0 ) {
            return -3;
        }
        con_mouse_poll();
        struct pollfd pfd = { 0, POLLIN, 0 };
        int pollms = timeout < 0 ? ( con_dirty ? 12 : 100 ) : timeout;
        if ( con_lbdown && con_wantmouse && ( pollms < 0 || pollms > 20 ) ) {
            pollms = 20;                        // (a held button repeats: look at the clock often)
        }
        int rv = poll( &pfd, 1, pollms );     // (a pointer that moved is painted a moment after the last report)
        if ( rv < 0 ) {
            if ( errno == EINTR ) {
                continue;
            }
            return -1;
        }
        if ( rv == 0 ) {
            if ( timeout < 0 ) {
                if ( con_fullscreen && con_winch && !con_wantresize ) {
                    con_paint_now();            // a program waits for a key and the terminal was resized: draw it again at its new size
                }
                continue;
            }
            return -2;
        }
        unsigned char c;
        if ( con_mouse ) {
            // the terminal reports the mouse in the same stream as the keys: read what is there, take the reports out, queue the keys
            unsigned char chunk[256];
            ssize_t m = read( 0, chunk, sizeof(chunk) );
            if ( m == 0 ) {
                return -1;
            }
            if ( m < 0 ) {
                if ( errno == EINTR ) {
                    continue;
                }
                return -1;
            }
            m = con_mouse_filter( chunk, m, sizeof(chunk) );
            for ( ssize_t i = 0; i < m && con_inqlen < sizeof(con_inq); ++i ) {
                con_inq[ con_inqlen++ ] = chunk[i];
            }
            con_mouse_poll();
            continue;
        }
        ssize_t n = read( 0, &c, 1U );
        if ( n == 0 ) {
            return -1;
        }
        if ( n < 0 ) {
            if ( errno == EINTR ) {
                continue;
            }
            return -1;
        }
        return c;
    }
}

static bool con_pastepending = false;  // a bracketed paste arrived (con_readpaste()): the next con_drv_nextkey() returns SK_PASTE

// the text between ESC [ 200 ~ and ESC [ 201 ~ (the start was read): converted to ISO-8859-15 and made the internal clipboard
static void con_readpaste( void ) {
    size_t cap = 4096, n = 0;
    char* text = (char*) malloc( cap );
    if ( text == 0 ) {
        return;
    }
    static const char endseq[] = "\033[201~";
    for ( ;; ) {
        int d = con_rawbyte( 2000 );
        if ( d < 0 ) {
            break;                                      // the end marker never came: take what there is
        }
        if ( n + 1U >= cap ) {
            if ( cap >= (size_t) 4 * 1024 * 1024 ) {
                continue;                               // (too much: the rest is dropped up to the end marker)
            }
            char* bigger = (char*) realloc( text, cap * 2U );
            if ( bigger == 0 ) {
                break;
            }
            text = bigger; cap *= 2U;
        }
        text[n++] = (char) d;
        if ( n >= 6U && memcmp( text + n - 6U, endseq, 6U ) == 0 ) {
            n -= 6U;
            break;
        }
    }
    char* latin = (char*) malloc( n + 1U );
    if ( latin != 0 ) {
        size_t lost = 0;
        size_t m = n > 0 ? keyseq_listing_from_utf8( text, n, latin, &lost ) : 0;
        m = clip_normalize( latin, m );
        clip_internal_set( latin, m );
        con_pastepending = true;
        free( latin );
    }
    free( text );
}

static bool con_linemode = false;       // a line is being edited: the terminal's mode is the line editor's business (see con_getline())
static bool con_aftercr = false;        // the last key was a CR: a LF right behind it (a file with CR LF line ends) is part of it

static int con_readkey( bool wait ) {
    fflush( stdout );
    con_flush_paint();
    if ( !con_linemode ) {
        con_set_raw( true );
    }
    for (;;) {
        if ( con_pendpos < con_pendlen ) {
            return (unsigned char) con_pend[ con_pendpos++ ];
        }
        int c = con_rawbyte( wait ? -1 : 0 );
        if ( c == -3 ) {
            return -3;                  // (only while a full-screen session asks for it)
        }
        if ( c == -1 ) {
            return wait ? -1 : ( con_break ? -1 : 0 );      // end of input: INKEY$ sees "no key", GET ends
        }
        if ( c == -2 ) {
            return 0;
        }
        if ( c == '\n' && con_aftercr ) {
            con_aftercr = false;                // (CR LF is one line end)
            continue;
        }
        con_aftercr = c == '\r';
        if ( c == '\n' || c == '\r' ) return 13;
        if ( c == 8 || c == 127 ) return 8;
        if ( c == '\t' ) return 9;
        if ( c >= 0x20 && c < 0x7F ) return c;
        if ( c == 033 ) {
            char buf[KEYSEQ_MAXLEN];
            size_t n = 0;
            buf[n++] = (char) c;
            bool skip = false;
            for ( ;; ) {
                int key, mods;
                int r = keyseq_parse( buf, n, &key, &mods );
                if ( r > 0 ) {
                    con_pendlen = keyseq_make( key, mods, con_pend );       // the canonical form, whatever the terminal sent
                    con_pendpos = 1U;
                    return (unsigned char) con_pend[0];
                }
                if ( r == 0 ) {
                    if ( n == 6U && memcmp( buf, "\033[200~", 6U ) == 0 ) {
                        con_readpaste();                // bracketed paste: the text is the clipboard now, the editor is told by SK_PASTE
                        return 0;
                    }
                    // no known sequence: an unknown CSI / SS3 sequence is skipped, anything else is an ESC and the bytes behind it
                    if ( n >= 2U && ( buf[1] == '[' || buf[1] == 'O' ) ) {
                        skip = true;
                    } else {
                        memcpy( con_pend, buf + 1, n - 1U );
                        con_pendlen = n - 1U;
                        con_pendpos = 0;
                        return 033;
                    }
                    break;
                }
                int d = con_rawbyte( CON_ESCWAIT );
                if ( d < 0 ) {
                    if ( n == 1U && con_pumping && con_escbreak ) {
                        con_break = 1;                       // a lone ESC while a program runs is Break (as in the window), also for GET and INKEY$
                        return -1;
                    }
                    memcpy( con_pend, buf + 1, n - 1U );     // cut off: a plain ESC, the rest as it is
                    con_pendlen = n - 1U;
                    con_pendpos = 0;
                    return 033;
                }
                if ( n < sizeof(buf) ) {
                    buf[n++] = (char) d;
                } else {
                    skip = true;
                    break;
                }
            }
            if ( skip ) {
                // an unknown sequence: drop it up to its final byte (0x40..0x7E, behind at least "ESC [" and one more byte)
                unsigned char last = (unsigned char) buf[n - 1U];
                size_t count = n;
                while ( count < 3U || last < 0x40 || last > 0x7E ) {
                    int d = con_rawbyte( CON_ESCWAIT );
                    if ( d < 0 ) {
                        break;
                    }
                    last = (unsigned char) d;
                    ++count;
                }
            }
            if ( !wait ) {
                return 0;
            }
            continue;
        }
        if ( c < 0x20 && con_wantctrl ) {
            return c;
        }
        if ( c >= 0x80 ) {
            if ( !isatty( 0 ) ) {
                return c;                       // from a pipe: the byte as it is
            }
            char u[4];
            size_t need = c >= 0xF0 ? 4U : c >= 0xE0 ? 3U : c >= 0xC0 ? 2U : 1U;
            u[0] = (char) c;
            size_t got = 1U;
            while ( got < need ) {
                int d = con_rawbyte( CON_ESCWAIT );
                if ( d < 0 ) {
                    break;
                }
                u[got++] = (char) d;
            }
            char out[4];
            if ( need > 1U && keyseq_utf8_to_latin9( u, got, out ) == 1U ) {
                return (unsigned char) out[0];
            }
        }
        // anything else (control codes, characters the set doesn't have) is skipped
        if ( !wait ) {
            return 0;
        }
    }
}

static int con_getkey( struct _vm_t* vm, void* usrdata, int channel ) {
    (void) vm; (void) usrdata;
    return channel == 0 ? con_readkey( true ) : -1;
}

static int con_inkey( struct _vm_t* vm, void* usrdata ) {
    (void) vm; (void) usrdata;
    int key = con_readkey( false );
    return key > 0 ? key : 0;
}

#ifdef CON_KEYSTATE
// KEY("name"): held down if the terminal sent it a moment ago (see con_note_bytes()); without a terminal no key is ever down
static bool con_keydown( struct _vm_t* vm, void* usrdata, int keyid ) {
    (void) vm; (void) usrdata;
    if ( !con_pumping || keyid <= 0 || keyid >= KEYID_MAX ) {
        return false;
    }
    con_pump();
    uint64_t seen = con_keyseen[keyid];
    return seen != 0 && con_now_ms() - seen <= CON_KEYHOLD;
}
#endif      // CON_KEYSTATE

static bool con_interrupt( struct _vm_t* vm, void* usrdata ) {
    (void) vm; (void) usrdata;
    static unsigned counter = 0;
    if ( ( ++counter & 63U ) == 0 ) {
        if ( con_pumping ) {
            con_pump();
        }
        if ( con_dirty ) {
            con_paint_soon();
        }
    }
    return con_break != 0;
}

static void con_breakesc( struct _vm_t* vm, void* usrdata, bool on ) {
    (void) vm; (void) usrdata;
    con_escbreak = on;
}

static bool con_save( struct _vm_t* vm, void* usrdata, const char* filename, bool ascii ) {
    (void) ascii;
    return repl_save_program_ex( (repl_t*) usrdata, filename, vm->savemode );
}

// an argument of a display statement as an int; an omitted one is a huge negative number (see gfxops.h)
static int con_gi( double v, int def ) {
    if ( v < -1.0e8 ) {
        return def;
    }
    return v > 1.0e6 ? 1000000 : v < -1.0e6 ? -1000000 : (int) v;
}

// the display statements: the text ones that a terminal can do, an error for the rest
// the 3D sector renderer has no picture here, but the world, the movement and the functions work (scripts and tests can run a game headless)
static w3d_t* con_w3d;

static void con_w3d_free( void ) {
    w3d_free( con_w3d );
    con_w3d = 0;
}

static bool con_w3d_get( void ) {
    if ( con_w3d == 0 ) {
        con_w3d = w3d_new();
        if ( con_w3d != 0 ) {
            atexit( con_w3d_free );
        }
    }
    return con_w3d != 0;
}

static bool con_gfx_op( void* usrdata, int op, const double* a, int na, const char* s, size_t slen, uint8_t* out, size_t outsz,
    size_t* outlen, const char** err ) {
    (void) na; (void) s;
    if ( GFXOP_IS_JSON( op ) ) {
        return jsonops_run( op, a, na, s, slen, out, outsz, outlen, ( (repl_t*) usrdata )->vm.addrcur, err );
    }
    if ( GFXOP_IS_W3D( op ) ) {
        if ( !con_w3d_get() ) {
            *err = "Out of memory";
            return false;
        }
        if ( op == GFXOP_W3DRENDER ) {
            w3d_render( con_w3d );          // (nothing to draw on, but the viewer's height follows the floor as in the window: logic that reads it behaves the same)
            return true;
        }
        if ( op == GFXOP_W3DCOLORMAP ) {
            return true;
        }
        if ( op == GFXOP_W3DWORLD ) {
            return w3dworld_load( con_w3d, jsonops_root(), err );
        }
        return w3d_op( con_w3d, op, a, na, s, slen, err );
    }
    if ( op >= GFXOP_MOUSESHOW && op <= GFXOP_MOUSETMO ) {
        if ( !con_mouse ) {
            *err = "Not available in the console host";
            return false;
        }
        switch ( op ) {
            case GFXOP_MOUSESHOW:   mouse_request_show(); break;
            case GFXOP_MOUSEHIDE:   mouse_request_hide(); break;
            case GFXOP_MOUSEPOS:    mouse_request_pos( con_gi( a[0], 0 ), con_gi( a[1], 0 ) ); break;
            case GFXOP_MOUSESHAPE:  mouse_request_shape( con_gi( a[0], 0 ) ); break;
            default:                mouse_request_timeout( (int)( a[0] * 1000.0 + 0.5 ) ); break;
        }
        con_mouse_poll();
        return true;
    }
    if ( con_screen ) {
        // screen mode: the text statements work on the text screen, as in the window
        switch ( op ) {
            case GFXOP_CLS:         txtscr_clear(); break;
            case GFXOP_LOCATE:      txtscr_locate( con_gi( a[0], 0 ), con_gi( a[1], 0 ) ); break;
            case GFXOP_HTAB: case GFXOP_VTAB: {
                int cx = 0, cy = 0;
                txtscr_getcursor( &cx, &cy );
                if ( op == GFXOP_HTAB ) {
                    txtscr_locate( con_gi( a[0], 0 ), cy );
                } else {
                    txtscr_locate( cx, con_gi( a[0], 0 ) );
                }
                break;
            }
            case GFXOP_COLOR:       conscr_text_setcolors( con_gi( a[1], -1 ), con_gi( a[0], -1 ) ); break;
            case GFXOP_TXTFILL:     txtscr_fill( con_gi( a[0], 0 ), con_gi( a[1], 0 ), con_gi( a[2], 0 ), con_gi( a[3], 0 ), con_gi( a[4], -1 ), con_gi( a[5], -1 ), con_gi( a[6], -1 ) ); break;
            case GFXOP_TXTCOPY:     txtscr_copy( con_gi( a[0], 0 ), con_gi( a[1], 0 ), con_gi( a[2], 0 ), con_gi( a[3], 0 ), con_gi( a[4], 0 ), con_gi( a[5], 0 ) ); break;
            case GFXOP_TXTINVERT:   txtscr_invert( con_gi( a[0], 0 ), con_gi( a[1], 0 ), con_gi( a[2], 0 ), con_gi( a[3], 0 ) ); break;
            case GFXOP_TXTSAVE:     *outlen = txtscr_save( out, outsz ); break;
            case GFXOP_TXTLOAD:     txtscr_restore( (const uint8_t*) s, slen ); break;
            default:
                *err = "Not available in the console host";
                return false;
        }
        con_paint_soon();
        return true;
    }
    (void) slen; (void) out; (void) outsz; (void) outlen;
    switch ( op ) {
        case GFXOP_CLS:
            if ( con_stdout_tty ) {
                fputs( "\033[2J\033[H", stdout );
            }
            return true;
        case GFXOP_LOCATE:
            if ( con_stdout_tty ) {
                // the columns and rows of the screen start at 0, the terminal's at 1; an omitted one is a huge negative number
                printf( "\033[%d;%dH", a[1] > -1.0e8 ? (int) a[1] + 1 : 1, a[0] > -1.0e8 ? (int) a[0] + 1 : 1 );
            }
            return true;
        case GFXOP_HTAB:
            if ( con_stdout_tty ) {
                printf( "\033[%dG", (int) a[0] + 1 );        // the terminal's column, from 1
            }
            return true;
        case GFXOP_VTAB:
            if ( con_stdout_tty ) {
                printf( "\033[%dd", (int) a[0] + 1 );
            }
            return true;
        case GFXOP_COLOR:
            return true;            // the Engine's palette is not the terminal's: use ANSI sequences in PRINT for colors
        default:
            *err = "Not available in the console host";
            return false;
    }
}

static bool con_gfx_query( void* usrdata, int q, const double* a, int na, double* result ) {
    (void) usrdata; (void) na;
    if ( q >= GFXQ_W3DFIRST && q <= GFXQ_W3DLAST ) {
        return con_w3d_get() && w3d_query( con_w3d, q, a, na, result );
    }
    if ( q == GFXQ_FRAME ) {                // (no screen refresh: 60 frames a second by the clock, so that FRAME works as a short wait)
        *result = (double)( con_mouse_ms() * 60U / 1000U );
        return true;
    }
    if ( ( ( q >= GFXQ_MOUSEX && q <= GFXQ_MOUSEVISIBLE ) || q == GFXQ_MOUSEEVENT ) && con_mouse ) {
        con_pump();                         // (what the mouse did since the last look)
        con_mouse_poll();
        switch ( q ) {
            case GFXQ_MOUSEX:       *result = mouse_get( MOUSEV_X ); break;
            case GFXQ_MOUSEY:       *result = mouse_get( MOUSEV_Y ); break;
            case GFXQ_MOUSEB:       *result = mouse_get( MOUSEV_BUTTONS ); break;
            case GFXQ_MOUSEPRESSED: *result = mouse_get( MOUSEV_PRESSED ); break;
            case GFXQ_MOUSERELEASED: *result = mouse_get( MOUSEV_RELEASED ); break;
            case GFXQ_MOUSEWHEEL:   *result = mouse_get( MOUSEV_WHEEL ); break;
            case GFXQ_MOUSESHAPE:   *result = mouse_get( MOUSEV_SHAPE ); break;
            case GFXQ_MOUSEEVENT:   *result = mouse_get( MOUSEV_EVENTS ); break;
            default:                *result = mouse_get( MOUSEV_VISIBLE ) ? -1.0 : 0.0; break;
        }
        return true;
    }
    if ( con_screen ) {
        int x = 0, y = 0, w = 0, h = 0, bg = 0, fg = 0;
        textcell_t c;
        switch ( q ) {
            case GFXQ_TXTX:         txtscr_getcursor( &x, &y ); *result = x; return true;
            case GFXQ_TXTY:         txtscr_getcursor( &x, &y ); *result = y; return true;
            case GFXQ_TXTFG:        txtscr_getcolors( &bg, &fg ); *result = fg; return true;
            case GFXQ_TXTBG:        txtscr_getcolors( &bg, &fg ); *result = bg; return true;
            case GFXQ_TXTCHAR:      c = txtscr_getcell( con_gi( a[1], 0 ), con_gi( a[0], 0 ) ); *result = TXTSCR_CELL_CHR( c ); return true;
            case GFXQ_TXTFGAT:      c = txtscr_getcell( con_gi( a[1], 0 ), con_gi( a[0], 0 ) ); *result = TXTSCR_CELL_FG( c ); return true;
            case GFXQ_TXTBGAT:      c = txtscr_getcell( con_gi( a[1], 0 ), con_gi( a[0], 0 ) ); *result = TXTSCR_CELL_BG( c ); return true;
            case GFXQ_TXTGLYPH:     *result = ( con_gi( a[0], -1 ) | con_gi( a[1], -1 ) ) < 0 || con_gi( a[0], 0 ) > 255 || con_gi( a[1], 0 ) > 11 ? -1 : txtscr_font()[ (size_t) con_gi( a[0], 0 ) * 12U + (size_t) con_gi( a[1], 0 ) ]; return true;
            case GFXQ_TXTBUILTIN:   *result = ( con_gi( a[0], -1 ) | con_gi( a[1], -1 ) ) < 0 || con_gi( a[0], 0 ) > 255 || con_gi( a[1], 0 ) > 11 ? -1 : txtscr_builtin_font()[ (size_t) con_gi( a[0], 0 ) * 12U + (size_t) con_gi( a[1], 0 ) ]; return true;
            case GFXQ_TXTWIDTH:     txtscr_getsize( &w, &h ); *result = w; return true;
            case GFXQ_TXTHEIGHT:    txtscr_getsize( &w, &h ); *result = h; return true;
            default:                return false;
        }
    }
    struct winsize ws;
    bool have = isatty( 1 ) && ioctl( 1, TIOCGWINSZ, &ws ) == 0 && ws.ws_col > 0 && ws.ws_row > 0;
    switch ( q ) {
        case GFXQ_TXTWIDTH:     *result = have ? ws.ws_col : 80; return true;
        case GFXQ_TXTHEIGHT:    *result = have ? ws.ws_row : 25; return true;
        default:                return false;
    }
}

static const vm_gfx_t con_gfx = { con_gfx_op, con_gfx_query };

// --color=always|never|auto: syntax colors in LIST and FILES on standard output. auto = on a terminal, unless NO_COLOR is set (no-color.org)
static bool con_want_color( const char* mode ) {
    if ( strcmp( mode, "always" ) == 0 ) {
        return true;
    }
    if ( strcmp( mode, "never" ) == 0 ) {
        return false;
    }
    const char* nc = getenv( "NO_COLOR" );
    return con_stdout_tty && ( nc == 0 || nc[0] == '\0' );
}

// the security policy (policy.h): the running program wants a power it does not have. On the screen of screen mode the question is the status line, in line
// mode on a terminal it is written to standard error and the key comes from the terminal; without a terminal the answer is no (--allow= decides then)
static int con_policy_ask( void* ud, int category, const char* what ) {
    (void) ud;
    char q[400];
    snprintf( q, sizeof(q), "The program wants to %s. Allow (%s)? n no, s session, d 24 hours, a always", what, policy_category_name( category ) );
    int c = 0;
    if ( con_screen ) {
        txtscr_setstatus( q, 190, 140 );
        con_paint_now();
        do {
            c = con_readkey( true );
        } while ( c == 0 || c == -3 );
        txtscr_setstatus( 0, 0, 0 );
        con_dirty = true;
    } else if ( con_pumping ) {
        fprintf( stderr, "\n%s: ", q );
        fflush( stderr );
        do {
            c = con_readkey( true );
        } while ( c == 0 || c == -3 );
        fprintf( stderr, "%c\n", c >= 32 && c < 127 ? c : '?' );
    } else {
        FILE* tty = isatty( 1 ) || isatty( 2 ) ? fopen( "/dev/tty", "r+" ) : 0;      // (a run without any terminal output, a script or a test, never asks)
        if ( tty == 0 ) {
            return POL_ANS_DENY;
        }
        fprintf( tty, "%s: ", q );
        fflush( tty );
        char line[16];
        c = fgets( line, sizeof(line), tty ) != 0 ? line[0] : 0;
        fclose( tty );
    }
    switch ( c ) {
        case 's': case 'S': return POL_ANS_SESSION;
        case 'd': case 'D': return POL_ANS_DAY;
        case 'a': case 'A': return POL_ANS_ALWAYS;
        default: return POL_ANS_DENY;
    }
}

static bool con_init( repl_t* repl ) {
    if ( !init_repl( repl ) ) {
        fprintf( stderr, "failed to initialize the BASIC system\n" );
        return false;
    }
    policy_enable( true );
    policy_set_asker( con_policy_ask, 0 );
    repl->report    = con_report;
    repl->print     = con_print;
    repl->printn    = con_printn;
    repl->inkey     = con_inkey;
    repl->input     = con_input;
    repl->getkey    = con_getkey;
    repl->interrupt = con_interrupt;
    repl->breakesc  = con_breakesc;
#ifdef CON_KEYSTATE
    repl->keydown   = con_keydown;
#endif
    repl->clipget   = con_vm_clipget;
    repl->clipset   = con_vm_clipset;                                      // (without a keydown callback KEY() is a run-time error: "KEY is not available in this host")
    repl->save      = con_save;
    repl->edit      = con_edit;
    repl->gfx       = &con_gfx;
    repl->hlstyle   = con_color ? ansi_hlstyle : 0;     // syntax colors for LIST / FILES (channel 0 only, see the REPL)
    repl->userdata  = repl;
    // no sound, no pager (a listing is not paged)
    return true;
}

// -- the two modes ---------------------------------------------------------------------------

// -- the full-screen editor and the monitor on the terminal ---------------------------------------------------
//
// EDIT without a target and MONITOR run the same code as in the window (fulledit.c, monitor.c) through the screen driver below: the screens are
// the text screens of textscreen.c, painted on the terminal by conscr.c (the alternate screen, no curses), the keys are those of GET and
// INKEY$ (the canonical escape sequences), and a resize of the terminal (SIGWINCH) is reported to the editors, which lay themselves out again.

// the bytes of one key press -> a key of the editors; false if it is not one (the caller reads on)
static bool con_scrkey( int c, scrkey_t* k ) {
    memset( k, 0, sizeof(*k) );
    if ( c == 13 ) { k->kind = SK_RETURN; return true; }
    if ( c == 8 )  { k->kind = SK_BACKSPACE; return true; }
    if ( c == 9 )  { k->kind = SK_TAB; return true; }
    if ( c == 033 ) {
        char seq[KEYSEQ_MAXLEN];
        size_t n = 1U;
        seq[0] = 033;
        while ( con_pendpos + n - 1U < con_pendlen && n + 1U < sizeof(seq) ) {
            seq[n] = con_pend[ con_pendpos + n - 1U ];
            ++n;
        }
        int key, mods;
        int r = keyseq_parse( seq, n, &key, &mods );
        if ( r > 0 ) {
            con_pendpos += (size_t) r - 1U;
            k->kind = SK_KEY; k->key = key; k->mods = mods;
            return true;
        }
        if ( n == 1U || seq[1] < 0x20 ) {
            k->kind = SK_ESCAPE;                // a lone ESC (also when a control code follows at once, as RETURN does in a file: it stays for the next key)
            return true;
        }
        return false;                           // ESC and a character: the ESC is dropped
    }
    if ( c >= 1 && c <= 26 ) {                  // Ctrl+letter
        k->kind = SK_CHAR; k->ch = 'a' + c - 1; k->mods = KSMOD_CTRL;
        return true;
    }
    if ( SCR_PRINTABLE( c ) ) {
        k->kind = SK_CHAR; k->ch = c;
        return true;
    }
    return false;
}

static bool con_drv_pending( void ) {
    if ( con_pendpos < con_pendlen || con_inqpos < con_inqlen || con_winch || con_mqn > 0 ) {
        return true;
    }
    struct pollfd pfd = { 0, POLLIN, 0 };
    return poll( &pfd, 1, 0 ) > 0 && ( pfd.revents & POLLIN ) != 0;
}

static bool con_drv_nextkey( scrkey_t* k ) {
    for (;;) {
        if ( con_mqn > 0 ) {                    // a mouse event (only while the full-screen editor wants them)
            *k = con_mq[ con_mqr ];
            con_mqr = ( con_mqr + 1U ) % 32U;
            --con_mqn;
            return true;
        }
        if ( con_winch ) {
            con_winch = 0;
            if ( conscr_syncsize() ) {
                memset( k, 0, sizeof(*k) );
                k->kind = SK_RESIZE;
                return true;
            }
        }
        if ( !con_drv_pending() ) {
            conscr_paint();                     // (a burst of keys is drawn once)
        }
        int c = con_readkey( true );
        if ( c == -3 ) {
            continue;
        }
        if ( c < 0 ) {
            if ( con_break ) {                  // Ctrl-C (or the break key while a program ran)
                con_break = 0;
                memset( k, 0, sizeof(*k) );
                k->kind = SK_BREAK;
                return true;
            }
            memset( k, 0, sizeof(*k) );
            k->kind = SK_CLOSED;                // the end of the input
            return false;
        }
        if ( c == 0 ) {
            if ( con_pastepending ) {
                con_pastepending = false;
                memset( k, 0, sizeof(*k) );
                k->kind = SK_PASTE;
                return true;
            }
            continue;
        }
        if ( con_scrkey( c, k ) ) {
            return true;
        }
    }
}

static void con_drv_wantmouse( bool enable ) {
    con_wantmouse = enable && con_mouse;
    if ( con_stdout_tty ) {
        conscr_paste( enable );                 // (the full-screen editor takes a bracketed paste; only while the terminal is on the alternate screen)
    }
    con_lbdown = false;                         // (reports come in screen mode only)
    con_mqr = con_mqn = 0;
}

static int con_drv_select( int n ) { return txtscr_select( n ) ? n : txtscr_selected(); }
static void con_drv_screensize( int* cols, int* rows ) { txtscr_getscreensize( cols, rows ); }
static void con_drv_getsize( int* cols, int* rows ) { txtscr_getsize( cols, rows ); }
static void con_drv_getviewport( int* x, int* y, int* w, int* h ) { txtscr_getviewport( x, y, w, h ); }
static void con_drv_getcursor( int* x, int* y ) { txtscr_getcursor( x, y ); }
static void con_drv_print( int y, int x, int bg, int fg, const char* text ) { txtscr_print( y, x, bg, fg, text ); }
static void con_drv_changed( void ) { conscr_paint(); }
static int con_drv_restart( int absy, int startx, size_t pos ) { (void) startx; (void) pos; return absy - txtscr_resizeshift(); }

static int con_drv_begin( void ) {
    int state = ( con_wantctrl ? 1 : 0 ) | ( con_wantresize ? 2 : 0 );
    con_wantctrl = true;                        // Ctrl+letter keys are keys here
    con_wantresize = true;
    return state;
}

static void con_drv_end( int state ) {
    con_wantctrl = ( state & 1 ) != 0;
    con_wantresize = ( state & 2 ) != 0;
}

// the monitor runs programs: keys are looked at while they run, and the lone ESC / Ctrl-C then break them (see con_pump())
static void con_drv_pollkeys( bool enable ) {
    con_escbreak = true;                        // (a run starts, and ends, with BREAK ON)
    con_pumping = enable && isatty( 0 );
    if ( enable ) {
        con_break = 0;
    }
}

// the clipboard of the editors: the internal one; copying also goes to the terminal's clipboard (OSC 52, E1999_CLIPBOARD=0 switches that off),
// pasting is the terminal's own (a bracketed paste fills the internal clipboard)
static char* con_drv_clip_get( size_t* len ) { return clip_internal_get( len ); }

static void con_drv_clip_set( const char* text, size_t len ) {
    clip_internal_set( text, len );
    const char* cb = getenv( "E1999_CLIPBOARD" );
    if ( con_stdout_tty && !( cb != 0 && cb[0] == '0' ) && len > 0 ) {
        char* u = (char*) malloc( 3U * len + 1U );
        if ( u != 0 ) {
            size_t n = keyseq_latin9_to_utf8( text, len, u );
            conscr_setclipboard( u, n );
            free( u );
        }
    }
}

static char* con_vm_clipget( struct _vm_t* vm, void* usrdata, size_t* len ) { (void) vm; (void) usrdata; return con_drv_clip_get( len ); }
static void con_vm_clipset( struct _vm_t* vm, void* usrdata, const char* text, size_t len ) { (void) vm; (void) usrdata; con_drv_clip_set( text, len ); }

static const scrdrv_t con_drv = {
    con_drv_screensize, con_drv_getsize,
    con_drv_select, txtscr_selected, txtscr_setviewport, txtscr_resetviewport, con_drv_getviewport,
    txtscr_setcolors, txtscr_setblank, txtscr_clear, txtscr_write, con_drv_print, txtscr_locate, con_drv_getcursor,
    con_drv_changed, txtscr_setresizereserve, con_drv_restart,
    con_drv_begin, con_drv_end, con_drv_nextkey, con_drv_pending, con_drv_pollkeys, 0, con_drv_wantmouse, con_drv_clip_get, con_drv_clip_set
};

// -- the one-line editor on the terminal -------------------------------------------------------------------
//
// The prompt, INPUT and the monitor use the same editor in both hosts (scredit.c: type, LEFT / RIGHT / HOME / END, BACKSPACE / DELETE, RETURN accepts,
// ESC cancels, Ctrl-D on an empty line is the end of the input). On the normal terminal it draws with the model of conscr.c (line mode): when a line
// starts the terminal is asked where its cursor is (ESC [ 6 n).

// where the terminal's cursor is (0-based); false if it did not answer in time. Keys that were typed before the answer stay in the queue.
static bool con_cursorpos( int* row, int* col ) {
    unsigned char held[128];
    size_t nh = 0;
    char reply[32];
    size_t nr = 0;
    bool got = false;
    fputs( "\033[6n", stdout );
    fflush( stdout );
    for ( ;; ) {
        int c = con_rawbyte( 250 );
        if ( c < 0 ) {
            break;
        }
        bool fits = false;
        if ( nr == 0 ) {
            fits = c == 033;
        } else if ( nr == 1U ) {
            fits = c == '[';
        } else {
            fits = ( c >= '0' && c <= '9' ) || c == ';' || c == 'R';
        }
        if ( fits && nr + 1U < sizeof(reply) ) {
            reply[nr++] = (char) c;
            if ( c == 'R' ) {
                int r, cl;
                reply[nr] = '\0';
                if ( sscanf( reply, "\033[%d;%dR", &r, &cl ) == 2 && r > 0 && cl > 0 ) {
                    *row = r - 1; *col = cl - 1;
                    got = true;
                    break;
                }
                for ( size_t i = 0; i < nr && nh < sizeof(held); ++i ) held[nh++] = (unsigned char) reply[i];
                nr = 0;
            }
        } else {
            // not the answer: a key that was typed meanwhile
            for ( size_t i = 0; i < nr && nh < sizeof(held); ++i ) held[nh++] = (unsigned char) reply[i];
            nr = 0;
            if ( c == 033 ) {
                reply[nr++] = 033;
            } else if ( nh < sizeof(held) ) {
                held[nh++] = (unsigned char) c;
            }
        }
    }
    for ( size_t i = 0; i < nr && nh < sizeof(held); ++i ) held[nh++] = (unsigned char) reply[i];
    if ( nh > 0 ) {                                 // the keys go to the front of the queue
        size_t room = sizeof(con_inq) - con_inqlen;
        if ( nh > room ) nh = room;
        memmove( con_inq + nh, con_inq + con_inqpos, con_inqlen - con_inqpos );
        memcpy( con_inq, held, nh );
        con_inqlen = nh + ( con_inqlen - con_inqpos );
        con_inqpos = 0;
    }
    return got;
}

// the model of the terminal (size, cursor) for a line that starts now
static void con_line_sync( void ) {
    int cols, rows, row, col;
    conscr_termsize( &cols, &rows );
    conscr_line_setsilent( !( isatty( 0 ) && isatty( 1 ) ) );       // a redirected input or output: nothing is drawn, the editor works on its model
    if ( !( isatty( 0 ) && isatty( 1 ) ) ) {
        // nothing is drawn: the model screen is as tall as a line of the longest length needs, so only the length limit counts
        if ( rows < (int)( CON_PROMPTMAX / (unsigned) cols ) + 3 ) {
            rows = (int)( CON_PROMPTMAX / (unsigned) cols ) + 3;
        }
        conscr_line_init( cols, rows, 0, 0 );
        return;
    }
    if ( !con_cursorpos( &row, &col ) ) {
        row = rows - 1;                              // (the bottom row: where the prompt usually is)
        col = con_col % cols;
    }
    conscr_line_init( cols, rows, col, row );
}

static int con_line_begin( void ) {
    int state = ( con_wantctrl ? 1 : 0 ) | ( con_wantresize ? 2 : 0 );
    con_wantctrl = true;
    con_wantresize = true;
    con_line_sync();
    return state;
}

static bool con_line_nextkey( scrkey_t* k ) {
    for (;;) {
        if ( con_winch ) {
            con_winch = 0;
            con_line_sync();                         // the size, and where the cursor is after the terminal reflowed the text
            memset( k, 0, sizeof(*k) );
            k->kind = SK_RESIZE;
            return true;
        }
        int c = con_readkey( true );
        if ( c == -3 ) {
            continue;
        }
        if ( c < 0 ) {
            memset( k, 0, sizeof(*k) );
            if ( con_break ) {                       // Ctrl-C
                con_break = 0;
                k->kind = SK_BREAK;
                return true;
            }
            k->kind = SK_EOF;                        // the end of the input (a redirected one, or the terminal's)
            return true;
        }
        if ( c == 0 ) {
            continue;
        }
        if ( con_scrkey( c, k ) ) {
            return true;
        }
    }
}

static int con_line_select( int n ) { (void) n; return 0; }
static int con_line_selected( void ) { return 0; }
static bool con_line_setviewport( int x, int y, int w, int h ) { (void) x; (void) y; (void) w; (void) h; return true; }
static void con_line_nothing( void ) { }
static void con_line_colors( int bg, int fg ) { (void) bg; (void) fg; }
static void con_line_reserve( int rows ) { (void) rows; }

static const scrdrv_t con_linedrv = {
    conscr_line_getsize, conscr_line_getsize,
    con_line_select, con_line_selected, con_line_setviewport, con_line_nothing, conscr_line_getviewport,
    con_line_colors, con_line_colors, con_line_nothing, conscr_line_write, conscr_line_print, conscr_line_locate, conscr_line_getcursor,
    conscr_line_changed, con_line_reserve, conscr_line_restart,
    con_line_begin, con_drv_end, con_line_nextkey, con_drv_pending, con_drv_pollkeys
};

// A line from standard input: always through the editor above (at most maxlen characters), whatever the input is: the keys come from the terminal, or
// from a file or a pipe (no special case: this allows input from another source than the console; the bytes are the keys, a line end is RETURN, the
// end of the input completes a last line without one). ESC (and Ctrl-C) cancel it: at the prompt the line is empty, at INPUT (input is true) that is a
// Break; Ctrl-D on an empty line is the end of the input. Nothing is drawn if the input or the output is not a terminal. Returns the length, -1 at the end.
// runs the one-line editor on work (the text to edit, bufsz bytes including the NUL) with the terminal set up for it; the result is a SCREDIT_* code
static int con_runedit( char* work, size_t bufsz ) {
    // Ctrl-C must leave the terminal in order: a handler of ours is in effect while the line is typed (the editor sees ESC)
    struct sigaction mine, old;
    memset( &mine, 0, sizeof(mine) );
    mine.sa_handler = con_sigint;
    sigemptyset( &mine.sa_mask );
    sigaction( SIGINT, &mine, &old );
    con_break = 0;
    // the terminal is in raw mode only if the editor draws on it; with a redirected output the terminal's own line mode echoes
    con_linemode = true;
    con_set_raw( isatty( 0 ) && isatty( 1 ) );
    // screen mode: the line is edited on the text screen, where the program's output is (no cursor position query)
    int r = con_screen ? scr_lineedit( &con_drv, work, bufsz, 0, 0 ) : scr_lineedit( &con_linedrv, work, bufsz, 0, 0 );
    con_linemode = false;
    con_set_raw( false );
    sigaction( SIGINT, &old, 0 );
    return r;
}

static ssize_t con_getline( char** line, size_t* cap, size_t maxlen, bool input ) {
    char* work = (char*) malloc( maxlen + 1U );
    if ( work == 0 ) {
        return -1;
    }
    work[0] = '\0';
    int r = con_runedit( work, maxlen + 1U );
    ssize_t n = -1;
    // ESC at INPUT is Break unless BREAK OFF made it an ordinary key (then the line is empty); Ctrl-C always is Break; at the prompt both drop the line
    bool empty = r == SCREDIT_CANCELLED && input && !con_escbreak;
    if ( r == SCREDIT_ACCEPTED || ( ( r == SCREDIT_CANCELLED || r == SCREDIT_BREAK ) && !input ) || r == SCREDIT_TOOLONG || empty ) {
        if ( r != SCREDIT_ACCEPTED ) {
            work[0] = '\0';
        }
        n = (ssize_t) strlen( work );
        if ( *cap < (size_t) n + 2U ) {
            char* p = (char*) realloc( *line, (size_t) n + 2U );
            if ( p == 0 ) {
                free( work );
                return -1;
            }
            *line = p; *cap = (size_t) n + 2U;
        }
        memcpy( *line, work, (size_t) n + 1U );
    } else if ( r == SCREDIT_CANCELLED || r == SCREDIT_BREAK ) {
        con_break = 1;                           // ESC (BREAK ON) and Ctrl-C at INPUT are Break
    }
    free( work );
    return n;
}

// EDIT n / EDIT label: the line (with its number) is edited with the same editor as in the window; the result is entered like a typed line, ESC
// cancels (nothing is entered)
static char* con_edit( struct _repl_t* repl, const char* text ) {
    char buf[2048];                              // as in the window
    if ( strlen( text ) >= sizeof(buf) ) {
        con_report( repl, "? Line too long to edit\n" );
        return 0;
    }
    strcpy( buf, text );
    if ( con_midline && con_stdout_tty && !con_screen ) {       // the editor starts on a line of its own
        fputc( '\n', stdout );
        con_midline = false;
        con_col = 0;
    }
    int r = con_runedit( buf, sizeof(buf) );
    if ( r == SCREDIT_TOOLONG ) {
        con_report( repl, "? Line too long to edit\n" );
        return 0;
    }
    return r == SCREDIT_ACCEPTED ? strdup( buf ) : 0;
}

// EDIT / MONITOR: the REPL only sets flags for the host; this opens the screens. Without a terminal there is nothing to show.
static void con_run_windows( repl_t* repl ) {
    if ( !repl->fulledit && !repl->monitor ) {
        return;
    }
    bool ed = repl->fulledit, mon = repl->monitor;
    repl->fulledit = false;
    repl->editwin = false;                       // (EDIT WINDOW: the console has the text editor only)
    repl->monitor = false;
    if ( !isatty( 0 ) || !isatty( 1 ) ) {
        con_report( repl, "? EDIT and MONITOR need a terminal\n" );
        return;
    }
    fflush( stdout );
    bool own = !con_screen;                     // screen mode: the terminal is on its alternate screen and set up already
    struct sigaction winch, oldwinch;
    memset( &winch, 0, sizeof(winch) );
    winch.sa_handler = con_sigwinch;
    sigemptyset( &winch.sa_mask );
    if ( own ) {
        sigaction( SIGWINCH, &winch, &oldwinch );
    }
    con_catch_break( true );                    // Ctrl-C does not kill the process while the terminal is changed
    con_pumping = false;                        // (the monitor asks for it while it runs a program)
    con_set_raw( true );
    con_inqpos = con_inqlen = 0;
    con_pendpos = con_pendlen = 0;
    con_winch = 1;                              // the first thing is to look at the size
    if ( !own || conscr_begin() ) {
        con_flush_paint();
        con_fullscreen = true;
        if ( ed ) {
            fulledit_run( repl, &con_drv );
        }
        if ( mon ) {
            monitor_run( repl, &con_drv );
        }
        con_fullscreen = con_screen;
        if ( own ) {
            conscr_end();
        } else {
            txtscr_select( 0 );                 // back on the BASIC screen: it is painted again
            con_dirty = true;
        }
    } else {
        con_report( repl, "? EDIT and MONITOR need a terminal\n" );
    }
    con_wantctrl = false;
    con_wantresize = false;
    con_set_raw( false );
    con_catch_break( false );
    if ( own ) {
        sigaction( SIGWINCH, &oldwinch, 0 );
    }
    repl->fulledit = false;
    repl->monitor = false;
}

// -- screen mode -------------------------------------------------------------------------------------------------
//
// When standard input and output are terminals the BASIC session runs on text screen 0, painted on the terminal's alternate screen by conscr.c, exactly as
// in the window: the prompt is the screen editor (scr_screenedit()), INPUT and EDIT n the one-line editor on the screen, programs print to the screen
// (colors, CLS, LOCATE, COLOR and the TXT statements work), and the screen is written to the normal terminal when the session ends (the "exit dump"), so
// that its text stays in the shell's scroll-back. --line (or E1999_SCREEN=0) selects the classic line mode, where the terminal scrolls as usual.

static void con_sigfatal( int sig ) {       // a killed session leaves the terminal in order
    static const char seq[] = "\033[0m\033[?7h\033[?25h\033[?1049l";
    if ( write( 1, seq, sizeof(seq) - 1U ) < 0 ) { }
    if ( con_tio_saved ) {
        tcsetattr( 0, TCSANOW, &con_saved_tio );
    }
    signal( sig, SIG_DFL );
    raise( sig );
}

// does this session run in screen mode: both ends are a terminal that can do it, and nothing asked for the line mode
static bool con_want_screen( void ) {
    int opt = con_screenopt;
    const char* e = getenv( "E1999_SCREEN" );
    if ( opt < 0 && e != 0 && e[0] != '\0' ) {
        opt = e[0] == '0' ? 0 : 1;
    }
    const char* d = getenv( "E1999_DUMP" );
    if ( d != 0 && d[0] == '0' ) {
        con_dump = false;
    }
    const char* term = getenv( "TERM" );
    return opt != 0 && isatty( 0 ) && isatty( 1 ) && term != 0 && term[0] != '\0' && strcmp( term, "dumb" ) != 0;
}

static bool con_screen_begin( void ) {
    if ( con_screen ) {
        return true;
    }
    struct sigaction sa;
    memset( &sa, 0, sizeof(sa) );
    sigemptyset( &sa.sa_mask );
    sa.sa_handler = con_sigwinch;
    sigaction( SIGWINCH, &sa, 0 );
    sa.sa_handler = con_sigfatal;
    sigaction( SIGTERM, &sa, 0 );
    sigaction( SIGHUP, &sa, 0 );
    sigaction( SIGQUIT, &sa, 0 );
    con_set_raw( true );
    if ( !conscr_begin() ) {
        con_set_raw( false );
        return false;
    }
    con_screen = true;
    con_fullscreen = true;
    const char* mo = getenv( "E1999_MOUSE" );
    if ( mo == 0 || mo[0] != '0' ) {            // (E1999_MOUSE=0: no mouse reporting; the MOUSE statements then give an error)
        sprscr_init();                          // (the pointer is a sprite of the engine, drawn here as a cell: see conscr_setpointer())
        mouse_init();
        con_mouse = true;
        conscr_mouse( true );
    }
    txtscr_select( 0 );
    con_inqpos = con_inqlen = 0;
    con_pendpos = con_pendlen = 0;
    con_catch_break( false );                   // (screen mode: the handler stays installed)
    con_dirty = true;
    return true;
}

static void con_screen_end( bool dump ) {
    if ( !con_screen ) {
        return;
    }
    con_flush_paint();
    con_mouse = false;
    conscr_end();                       // (switches the mouse reporting off, too)
    con_screen = false;
    con_fullscreen = false;
    con_set_raw( false );
    if ( dump && con_dump ) {
        conscr_dump();
    }
}

// an error ended the program: the message stays on the screen until a key is pressed (the hint is an overlay, not text of the screen)
static void con_wait_key( void ) {
    txtscr_setstatus( "Press a key to leave", 190, 140 );
    con_paint_now();
    int c;
    do {
        c = con_readkey( true );
    } while ( c == 0 || c == -3 );
    txtscr_setstatus( 0, 0, 0 );
    con_dirty = true;
}

// AUTO [start[,step]]: while repl->autoline is set the prompt shows the next line number and the text typed behind it is entered as that line (the same as
// the window's loop in engine1999.c); an empty line, ESC or Ctrl-C ends it. visible = the prompt may be written (a screen, a terminal or "-i"; a script that
// reads lines from a pipe gets no numbers in its output). Returns false when the input is gone.
static bool con_auto_prompt( repl_t* repl, bool visible, char** line, size_t* cap ) {
    if ( repl->autoline > LINENO_MAX ) {
        repl->autoline = LINENO_NONE;
        if ( visible ) {
            con_write( "Ready.\n", 7U );
        }
        return true;
    }
    char num[16];
    int numlen = snprintf( num, sizeof(num), "%u ", (unsigned) repl->autoline );
    if ( visible ) {
        con_write( num, (size_t) numlen );
        if ( !con_screen ) {
            fflush( stdout );
        }
    }
    ssize_t n = con_getline( line, cap, CON_PROMPTMAX, false );
    if ( n < 0 ) {
        return false;
    }
    while ( n > 0 && ( (*line)[ n - 1 ] == '\n' || (*line)[ n - 1 ] == '\r' ) ) {
        (*line)[ --n ] = '\0';
    }
    const char* t = *line;
    while ( *t == ' ' || *t == '\t' ) {
        ++t;
    }
    if ( *t == '\0' ) {                       // an empty line ends AUTO
        repl->autoline = LINENO_NONE;
        if ( visible ) {
            con_write( "Ready.\n", 7U );
        }
        return true;
    }
    size_t blen = strlen( t ) + 16U;
    char* buf = (char*) malloc( blen );
    if ( buf == 0 ) {
        return false;
    }
    snprintf( buf, blen, "%u %s", (unsigned) repl->autoline, t );
    uint32_t entered = repl->autoline;
    con_catch_break( true );
    repl_enter( repl, buf );
    con_catch_break( false );
    con_run_windows( repl );
    free( buf );
    if ( repl->autoline == entered ) {          // (the line may have been an AUTO of its own)
        repl->autoline += repl->autostep;
    }
    return true;
}

// the prompt in screen mode: lines typed or picked from the screen are entered like lines from standard input
static void con_repl_screen( repl_t* repl, bool banner ) {
    static bool insert = false;
    char* line = (char*) malloc( CON_PROMPTMAX + 2U );
    if ( line == 0 ) {
        return;
    }
    if ( banner ) {
        char text[300];
        snprintf( text, sizeof(text), "Engine 1999 %s (c) Copyright 2026 Ekkehard Morgenstern.\n"
                "Console host. BYE or QUIT ends it. The license: SHOW COPYING in the window version, or the file LICENSE.\n\nReady.\n", ENGINE_VERSION );
        con_write( text, strlen( text ) );
    }
    char* autoline = 0; size_t autocap = 0;
    for (;;) {
        con_flush_paint();
        if ( repl->autoline != LINENO_NONE ) {
            if ( !con_auto_prompt( repl, true, &autoline, &autocap ) || repl->quit ) {
                break;
            }
            continue;
        }
        int r = scr_screenedit( &con_drv, line, CON_PROMPTMAX + 2U, &insert, 0, 0 );
        if ( r == SCREDIT_CANCELLED ) {                      // (a screen under 3 rows: the one-line editor, on the screen)
            line[0] = '\0';
            r = scr_lineedit( &con_drv, line, CON_PROMPTMAX + 2U, 0, 0 );
        }
        if ( r != SCREDIT_ACCEPTED ) {
            if ( r == SCREDIT_CLOSED || r == SCREDIT_EOF ) {
                break;                                       // the input is gone
            }
            continue;
        }
        const char* p = line;
        while ( *p == ' ' || *p == '\t' ) {
            ++p;
        }
        if ( *p == '\0' ) {
            continue;
        }
        bool stored = *p >= '0' && *p <= '9';
        con_catch_break( true );
        repl_enter( repl, line );
        con_catch_break( false );
        con_run_windows( repl );
        if ( repl->quit ) {
            break;
        }
        if ( !stored ) {
            con_write( "Ready.\n", 7U );
        }
    }
    free( autoline );
    free( line );
}

// reads lines from standard input and enters them; returns when the input ends or BYE / QUIT asks for the end
static void con_repl( repl_t* repl, bool interactive ) {
    char* line = 0; size_t cap = 0;
    if ( interactive ) {
        printf( "Engine 1999 %s (c) Copyright 2026 Ekkehard Morgenstern.\n"
                "Console host. BYE or QUIT ends it. The license: SHOW COPYING in the window version, or the file LICENSE.\n\nReady.\n",
                ENGINE_VERSION );
    }
    for (;;) {
        fflush( stdout );
        con_set_raw( false );
        if ( repl->autoline != LINENO_NONE ) {
            if ( !con_auto_prompt( repl, interactive || con_stdout_tty, &line, &cap ) || repl->quit ) {
                break;
            }
            continue;
        }
        ssize_t n = con_getline( &line, &cap, CON_PROMPTMAX, false );
        if ( n < 0 ) {
            break;
        }
        while ( n > 0 && ( line[ n - 1 ] == '\n' || line[ n - 1 ] == '\r' ) ) {
            line[ --n ] = '\0';
        }
        const char* p = line;
        while ( *p == ' ' || *p == '\t' ) {
            ++p;
        }
        if ( *p == '\0' ) {
            continue;
        }
        bool stored = *p >= '0' && *p <= '9';
        con_catch_break( true );
        repl_enter( repl, line );
        con_catch_break( false );
        con_run_windows( repl );
        if ( repl->quit ) {
            break;
        }
        if ( interactive && !stored ) {
            printf( "Ready.\n" );
        }
    }
    free( line );
}

// -- built executables without a window (BUILD ... , CONSOLE; basbuild.c) ---------------------------------
//
// BUILD links this object file with the program, which ld -r -b binary turns into an object that defines the symbols below. In the console host
// itself they are not defined (weak: their address is 0) and main() is the console host.

extern const unsigned char embedded_program_start[] __asm__( "_binary_program_e99_start" ) __attribute__((weak));
extern const unsigned char embedded_program_end[] __asm__( "_binary_program_e99_end" ) __attribute__((weak));

// the license notice of the runtime (the text of engine1999.c's about_text: keep them in sync, see LICENSE-EXCEPTION section 3 b)
static const char about_text[] =
    "Engine 1999 " ENGINE_VERSION " (c) Copyright 2026 Ekkehard Morgenstern.\n"
    "\n"
    "This program contains the console runtime of Engine 1999, free software that is licensed under the GNU General\n"
    "Public License, version 3 or later, with the Engine1999 Runtime Exception (file LICENSE-EXCEPTION of\n"
    "Engine 1999). The program that it runs is not covered by that license; it has its own terms.\n"
    "\n"
    "In short, the exception allows this executable to be distributed under terms of the author's choice if:\n"
    "  - the runtime in it is the unmodified runtime of Engine 1999 (if Engine 1999 code was modified, the GPL\n"
    "    applies to the whole executable),\n"
    "  - this notice stays in the executable and keeps showing with --engine1999-about,\n"
    "  - the runtime itself stays under the GPL: parts taken out of the executable, or the Engine on its own,\n"
    "    may not be conveyed under other terms.\n"
    "The C library (LGPL) is linked dynamically and keeps its own license.\n"
    "\n"
    "The runtime comes with ABSOLUTELY NO WARRANTY.\n"
    "Contact: ekkehard@ekkehardmorgenstern.de\n";

// runs the embedded program and ends, with the exit status of the console host (0, BYE n, 1 after an error); the whole command line is the program's
// (ARG$(0) is the executable), except for --engine1999-about as the first argument
static int con_run_embedded( int argc, char** argv ) {
    // the switches of the runtime come first and are not the program's: --engine1999-screen, --engine1999-line, --engine1999-no-dump
    int shift = 0;
    for ( ; argc > 1 + shift; ++shift ) {
        const char* a = argv[ 1 + shift ];
        if ( strcmp( a, "--engine1999-screen" ) == 0 ) con_screenopt = 1;
        else if ( strcmp( a, "--engine1999-line" ) == 0 ) con_screenopt = 0;
        else if ( strcmp( a, "--engine1999-no-dump" ) == 0 ) con_dump = false;
        else if ( strncmp( a, "--engine1999-allow=", 19U ) == 0 || strncmp( a, "--engine1999-deny=", 18U ) == 0 ) {
            bool allow = a[13] == 'a';
            char err[100];
            if ( !policy_set_option( a + ( allow ? 19 : 18 ), allow, err, sizeof(err) ) ) {
                fprintf( stderr, "%s: %s\n", argv[0], err );
                return 2;
            }
        }
        else break;
    }
    if ( shift > 0 ) {
        argv[shift] = argv[0];
        argv += shift;
        argc -= shift;
    }
    if ( argc > 1 && strcmp( argv[1], "--engine1999-about" ) == 0 ) {
        fputs( about_text, stdout );
        return EXIT_SUCCESS;
    }
    con_stdout_tty = isatty( 1 );
    con_color = con_want_color( "auto" );
    atexit( con_restore_tty );
    setvbuf( stdin, 0, _IONBF, 0 );
    static repl_t repl;
    if ( !con_init( &repl ) ) {
        return 2;
    }
    repl.buildconsole = true;
    repl.argc = argc;
    repl.argv = (const char* const*) argv;
    static char selfexe[4096];
    ssize_t selflen = readlink( "/proc/self/exe", selfexe, sizeof(selfexe) - 1U );
    if ( selflen > 0 ) {                    // the security policy: the program is this executable, its folder is the executable's folder
        selfexe[selflen] = '\0';
        static char selfdir[4096];
        snprintf( selfdir, sizeof(selfdir), "%s", selfexe );
        char* sl = strrchr( selfdir, '/' );
        if ( sl != 0 ) { if ( sl == selfdir ) selfdir[1] = '\0'; else *sl = '\0'; }
        repl.policyfile = selfexe;
        repl.policyimage = embedded_program_start;
        repl.policyimagelen = (size_t)( embedded_program_end - embedded_program_start );
        repl.policyroot = selfdir;
    }
    int status = EXIT_SUCCESS;
    if ( repl_load_image( &repl, embedded_program_start, (size_t)( embedded_program_end - embedded_program_start ) ) ) {
        bool scr = con_want_screen() && con_screen_begin();
        con_catch_break( true );
        bool ok = repl_run( &repl );
        con_catch_break( false );
        con_run_windows( &repl );
        fflush( stdout );
        status = repl.quit ? repl.exitcode : ok ? EXIT_SUCCESS : 1;
        if ( scr && !ok && !repl.quit ) {
            con_wait_key();
        }
        con_screen_end( true );
    } else {
        status = 2;
    }
    fflush( stdout );
    cleanup_repl( &repl );
    return status;
}

// -- key dump (--debug-keys) -------------------------------------------------------------------
//
// Shows what the terminal sends for each key, byte by byte, so the sequences of a terminal can be compared with the canonical ones
// (keyseq.h): the bytes of a key press that arrive within 30 ms are one burst; each item of a burst is shown as hex, as a C string and
// with what it is. No BASIC runs. Works with a pipe as well (the bytes of the pipe). Ctrl-D (or the end of the input) stops.

static bool con_dump_getbyte( unsigned char* c, int timeout ) {
    for (;;) {
        struct pollfd pfd = { 0, POLLIN, 0 };
        int rv = poll( &pfd, 1, timeout );
        if ( rv < 0 && errno == EINTR ) {
            continue;
        }
        if ( rv <= 0 ) {
            return false;
        }
        ssize_t n = read( 0, c, 1U );
        if ( n < 0 && errno == EINTR ) {
            continue;
        }
        return n == 1;
    }
}

static const char* const con_ctrlnames[32] = {
    "Ctrl-@ (NUL)", "Ctrl-A", "Ctrl-B", "Ctrl-C", "Ctrl-D", "Ctrl-E", "Ctrl-F", "Ctrl-G (BEL)", "Ctrl-H (BS)", "Tab (Ctrl-I)", "Ctrl-J (LF)", "Ctrl-K",
    "Ctrl-L", "Enter (Ctrl-M, CR)", "Ctrl-N", "Ctrl-O", "Ctrl-P", "Ctrl-Q", "Ctrl-R", "Ctrl-S", "Ctrl-T", "Ctrl-U", "Ctrl-V", "Ctrl-W", "Ctrl-X", "Ctrl-Y", "Ctrl-Z",
    "ESC (Ctrl-[)", "Ctrl-\\", "Ctrl-]", "Ctrl-^", "Ctrl-_" };

static void con_dump_item( const char* s, size_t len, const char* what ) {
    char hex[64], esc[64];
    keyseq_hex( s, len, hex, sizeof(hex) );
    keyseq_escape( s, len, esc, sizeof(esc) );
    char q[72];
    snprintf( q, sizeof(q), "\"%s\"", esc );
    printf( "    %-24s %-22s %s\n", hex, q, what );
}

// the items of one burst
static void con_dump_burst( const char* b, size_t n, bool* stop ) {
    size_t pos = 0;
    while ( pos < n ) {
        unsigned char c = (unsigned char) b[pos];
        char what[96];
        if ( c == 033 ) {
            int key, mods;
            int used = keyseq_parse( b + pos, n - pos, &key, &mods );
            if ( used > 0 ) {
                char name[KEYSEQ_NAMELEN];
                keyseq_name( key, mods, name );
                char canon[KEYSEQ_MAXLEN];
                keyseq_make( key, mods, canon );
                snprintf( what, sizeof(what), "%s%s", name, strncmp( canon, b + pos, (size_t) used ) == 0 && canon[used] == '\0' ? " (canonical)" : " (other form; canonical is not this)" );
                con_dump_item( b + pos, (size_t) used, what );
                pos += (size_t) used;
                continue;
            }
            size_t end = pos + 1U;
            if ( used == 0 && end < n && ( b[end] == '[' || b[end] == 'O' ) ) {
                // an unknown CSI / SS3 sequence: up to the final byte
                ++end;
                while ( end < n && ( ( (unsigned char) b[end] >= 0x20 && (unsigned char) b[end] <= 0x3F ) ) ) {
                    ++end;
                }
                if ( end < n ) {
                    ++end;
                }
                con_dump_item( b + pos, end - pos, "unknown escape sequence" );
            } else if ( used == -1 ) {
                con_dump_item( b + pos, n - pos, n - pos == 1U ? "ESC alone (or the start of a sequence cut off)" : "incomplete escape sequence" );
                end = n;
            } else if ( end < n && (unsigned char) b[end] >= 0x20 && (unsigned char) b[end] < 0x7F ) {
                ++end;
                snprintf( what, sizeof(what), "ESC + '%c' (Alt+%c on many terminals)", b[end - 1U], b[end - 1U] );
                con_dump_item( b + pos, end - pos, what );
            } else {
                con_dump_item( b + pos, 1U, "ESC" );
            }
            pos = end;
            continue;
        }
        if ( c < 0x20 ) {
            con_dump_item( b + pos, 1U, con_ctrlnames[c] );
            if ( c == 4 ) {
                *stop = true;
                return;             // what follows in the same burst is not shown
            }
            ++pos;
        } else if ( c == 0x7F ) {
            con_dump_item( b + pos, 1U, "DEL (Backspace on many terminals)" );
            ++pos;
        } else if ( c < 0x80 ) {
            snprintf( what, sizeof(what), "'%c' (U+%04X)", c, (unsigned) c );
            con_dump_item( b + pos, 1U, what );
            ++pos;
        } else {
            // UTF-8
            size_t need = c >= 0xF0 ? 4U : c >= 0xE0 ? 3U : c >= 0xC0 ? 2U : 1U;
            if ( pos + need > n ) {
                need = n - pos;
            }
            unsigned cp = need == 4U ? c & 7U : need == 3U ? c & 15U : need == 2U ? c & 31U : c;
            for ( size_t i = 1; i < need; ++i ) {
                cp = ( cp << 6 ) | ( (unsigned char) b[pos + i] & 63U );
            }
            snprintf( what, sizeof(what), need > 1U ? "U+%04X (UTF-8)" : "byte >= 0x80 (not UTF-8: Latin-1 or a lone byte?)", cp );
            con_dump_item( b + pos, need, what );
            pos += need;
        }
    }
}

static int con_keydump( void ) {
    const char* term = getenv( "TERM" );
    printf( "Key dump: the bytes this terminal sends. TERM=%s, standard input is %s.\n"
            "Press keys; bytes within 30 ms count as one key press. Ctrl-D (or the end of the input) stops.\n\n",
            term != 0 ? term : "(unset)", isatty( 0 ) ? "a terminal" : "not a terminal" );
    fflush( stdout );
    atexit( con_restore_tty );
    if ( isatty( 0 ) ) {
        struct termios t;
        if ( tcgetattr( 0, &con_saved_tio ) == 0 ) {
            con_tio_saved = true;
            t = con_saved_tio;
            t.c_lflag &= ~( (tcflag_t) ( ICANON | ECHO | ISIG | IEXTEN ) );
            t.c_iflag &= ~( (tcflag_t) ( ICRNL | INLCR | IGNCR | IXON | ISTRIP | BRKINT | PARMRK | INPCK ) );
            t.c_cc[VMIN] = 1;
            t.c_cc[VTIME] = 0;
            if ( tcsetattr( 0, TCSANOW, &t ) == 0 ) {
                con_raw = true;
            }
        }
    }
    struct timespec last;
    clock_gettime( CLOCK_MONOTONIC, &last );
    bool stop = false;
    while ( !stop ) {
        char b[256];
        size_t n = 0;
        unsigned char c;
        if ( !con_dump_getbyte( &c, -1 ) ) {
            break;
        }
        struct timespec now;
        clock_gettime( CLOCK_MONOTONIC, &now );
        long ms = (long)( now.tv_sec - last.tv_sec ) * 1000L + ( now.tv_nsec - last.tv_nsec ) / 1000000L;
        b[n++] = (char) c;
        while ( n < sizeof(b) && con_dump_getbyte( &c, 30 ) ) {
            b[n++] = (char) c;
        }
        printf( "%zu byte%s, %ld ms after the previous key press\n", n, n == 1U ? "" : "s", ms );
        con_dump_burst( b, n, &stop );
        fflush( stdout );
        clock_gettime( CLOCK_MONOTONIC, &last );
    }
    printf( "\nEnd of the key dump.\n" );
    return EXIT_SUCCESS;
}

static void usage( FILE* out, const char* prog ) {
    fprintf( out,
        "Usage: %s [options] [file | - [arguments]]\n"
        "\n"
        "  file         a BASIC program (.bas listing or .e99 file): it is loaded, run, and the exit status is\n"
        "               that of BYE / QUIT (0 if the program just ends, 1 after an error, 2 if it can't be loaded)\n"
        "  -            read BASIC lines from standard input (the default; interactive on a terminal)\n"
        "  arguments    for the program: ARGC is their number, ARG$(n) is the n-th (ARG$(0) is the file), ENV$(\"NAME\") an\n"
        "               environment variable\n"
        "\n"
        "Options:\n"
        "  -i           after running the file, go on with the lines from standard input\n"
        "  --color[=when]  syntax colors in LIST and FILES: always, never or auto (the default: on a terminal, unless NO_COLOR is set)\n"
        "  --screen     run the session on a screen on the terminal's alternate screen (the default if standard input and output are terminals)\n"
        "  --line       the classic line mode: the terminal scrolls as usual (also E1999_SCREEN=0)\n"
        "  --no-dump    in screen mode, do not write the screen to the terminal when the session ends (also E1999_DUMP=0)\n"
        "  --allow=list  let programs use these powers without asking: read, write (outside their folder), shell, net, or all (see the manual)\n"
        "  --deny=list   refuse these powers without asking (the same words)\n"
        "  --list-keywords  print the reserved words of the language, one per line\n"
        "  --version    show the version\n"
        "  -h, --help   show this text\n"
        "\n"
        "Debug output (to standard output, off by default):\n", prog );
    basdbg_usage( out );
}

int main( int argc, char** argv ) {
    if ( embedded_program_start != 0 ) {
        return con_run_embedded( argc, argv );
    }
    bool interact = false;
    const char* colormode = "auto";
    const char* file = 0;
    int i = 1;
    for ( ; i < argc; ++i ) {
        const char* arg = argv[i];
        if ( strcmp( arg, "-h" ) == 0 || strcmp( arg, "--help" ) == 0 ) {
            usage( stdout, argv[0] );
            return EXIT_SUCCESS;
        }
        if ( strcmp( arg, "--version" ) == 0 ) {
            printf( "Engine 1999 %s (console host)\n", ENGINE_VERSION );
            return EXIT_SUCCESS;
        }
        if ( strcmp( arg, "--list-keywords" ) == 0 ) {      // the reserved words, one per line (the test of the man page uses it)
            for ( int tok = 1; tok < 255; ++tok ) {
                const char* kw = keyword_name( (uint8_t) tok );
                if ( kw != 0 && ( ( kw[0] >= 'A' && kw[0] <= 'Z' ) ) ) {
                    puts( kw );
                }
            }
            for ( int i = 0; keyword_alias_name( i ) != 0; ++i ) {       // the alternative spellings
                puts( keyword_alias_name( i ) );
            }
            for ( int xid = 1; xid < 255; ++xid ) {          // the extension words (TOK_XFUNC)
                const char* kw = xkeyword_name( (uint8_t) xid );
                if ( kw != 0 ) {
                    puts( kw );
                }
            }
            return EXIT_SUCCESS;
        }
        if ( strcmp( arg, "-i" ) == 0 ) {
            interact = true;
        } else if ( strcmp( arg, "--screen" ) == 0 ) {
            con_screenopt = 1;
        } else if ( strcmp( arg, "--line" ) == 0 ) {
            con_screenopt = 0;
        } else if ( strcmp( arg, "--no-dump" ) == 0 ) {
            con_dump = false;
        } else if ( strncmp( arg, "--allow=", 8U ) == 0 || strncmp( arg, "--deny=", 7U ) == 0 ) {
            bool allow = arg[2] == 'a';
            char err[100];
            if ( !policy_set_option( arg + ( allow ? 8 : 7 ), allow, err, sizeof(err) ) ) {
                fprintf( stderr, "%s: %s\n", argv[0], err );
                return 2;
            }
        } else if ( strcmp( arg, "--color" ) == 0 ) {
            colormode = "always";
        } else if ( strncmp( arg, "--color=", 8U ) == 0 ) {
            colormode = arg + 8;
            if ( strcmp( colormode, "always" ) != 0 && strcmp( colormode, "never" ) != 0 && strcmp( colormode, "auto" ) != 0 ) {
                fprintf( stderr, "%s: bad value '%s' for --color (always, never or auto)\n", argv[0], colormode );
                return 2;
            }
        } else if ( strcmp( arg, "-" ) == 0 ) {
            file = 0;
            ++i;
            break;
        } else if ( basdbg_option( arg ) ) {
            // debug flag set
        } else if ( arg[0] == '-' && arg[1] != '\0' ) {
            fprintf( stderr, "%s: unknown option '%s'\n", argv[0], arg );
            usage( stderr, argv[0] );
            return 2;
        } else {
            file = arg;
            ++i;
            break;
        }
    }
    if ( basdbg_on( BASDBG_KEYS ) ) {
        return con_keydump();
    }
    // the rest of the command line is for the program: ARGC, ARG$(n); ARG$(0) is the file ("-" for standard input)
    const char** progargs = 0;
    int numprogargs = 0;
    if ( file != 0 || i > 1 ) {
        progargs = (const char**) calloc( (size_t)( argc - i ) + 2U, sizeof(char*) );
        if ( progargs == 0 ) {
            fprintf( stderr, "out of memory\n" );
            return 2;
        }
        progargs[ numprogargs++ ] = file != 0 ? file : "-";
        for ( ; i < argc; ++i ) {
            progargs[ numprogargs++ ] = argv[i];
        }
    }
    con_stdout_tty = isatty( 1 );
    con_color = con_want_color( colormode );
    atexit( con_restore_tty );
    // lines (getline) and single keys (read) both come from standard input: without a buffer of ours in between they stay in step
    setvbuf( stdin, 0, _IONBF, 0 );

    static repl_t repl;
    if ( !con_init( &repl ) ) {
        return 2;
    }
    repl.buildconsole = true;       // BUILD makes executables without a window here (BUILD "x", WINDOW makes the other kind)
    repl.argc = numprogargs;
    repl.argv = progargs;
    int status = EXIT_SUCCESS;
    if ( file != 0 ) {
        if ( !repl_load_file( &repl, file ) ) {
            cleanup_repl( &repl );
            return 2;
        }
        bool scr = con_want_screen() && con_screen_begin();
        con_catch_break( true );
        bool ok = repl_run( &repl );
        con_catch_break( false );
        con_run_windows( &repl );
        fflush( stdout );
        if ( repl.quit ) {
            status = repl.exitcode;
        } else if ( !ok ) {
            status = 1;
        }
        if ( scr && !ok && !repl.quit && !interact ) {
            con_wait_key();
        }
        if ( interact && !repl.quit ) {
            if ( scr ) {
                con_repl_screen( &repl, false );
            } else {
                con_repl( &repl, isatty( 0 ) );
            }
            if ( repl.quit ) {
                status = repl.exitcode;
            }
        }
    } else if ( con_want_screen() && con_screen_begin() ) {
        con_repl_screen( &repl, true );
        if ( repl.quit ) {
            status = repl.exitcode;
        }
    } else {
        con_repl( &repl, isatty( 0 ) );
        if ( repl.quit ) {
            status = repl.exitcode;
        }
    }
    con_screen_end( true );
    fflush( stdout );
    cleanup_repl( &repl );
    free( progargs );
    return status;
}
