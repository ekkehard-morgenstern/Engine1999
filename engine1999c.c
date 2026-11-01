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
#include "ansi.h"
#include "scrdrv.h"
#include "conscr.h"
#include "textscreen.h"
#include "fulledit.h"
#include "monitor.h"
#include "keyseq.h"
#include "version.h"

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

static bool con_fullscreen = false;             // the full-screen editor or the monitor is open: program output and messages go to the text screen
static bool con_pumping = false;                // a program is running on a terminal: look for keys (see con_pump())
static bool con_escbreak = true;                // ESC is the break key (BREAK ON); with BREAK OFF it is an ordinary key (Ctrl-C always breaks)
static size_t con_inqpos = 0, con_inqlen = 0;   // the keys that arrived while it ran (see con_pump())
static void con_set_raw( bool raw );

// Ctrl-C is Break while the BASIC system runs something; between lines the default action (end the process) is in effect. On a terminal a lone ESC
// is Break as well (as in the window), which needs the keys to arrive without the Return of the line mode: the terminal is in raw mode while a
// program runs, and the keys typed meanwhile are queued for it (con_pump())
static void con_catch_break( bool on ) {
    struct sigaction sa;
    memset( &sa, 0, sizeof(sa) );
    sa.sa_handler = on ? con_sigint : SIG_DFL;      // no SA_RESTART: a blocked read ends and the VM sees the Break
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
    if ( !isatty( 0 ) || raw == con_raw ) {
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
    con_set_raw( false );
}

// -- host callbacks -----------------------------------------------------------------------

static bool con_stdout_tty = false;
static bool con_color = false;
static bool con_midline = false;        // the last character written to standard output was not a newline
static int con_col = 0;                 // the column after the last output (a guess: printable bytes since the last newline), for a terminal that does not answer ESC [ 6 n

// Text is ISO-8859-15 inside the Engine. On a terminal (which speaks UTF-8) the characters from 128 up are converted; in a pipe or a file the bytes
// go out as they are (binary output must stay intact), and so they come in: see con_readkey().
static void con_write( const char* text, size_t len ) {
    if ( con_fullscreen ) {
        conscr_write( text, len );
        conscr_paint();
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
        conscr_paint();
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
static volatile sig_atomic_t con_winch = 0;     // the terminal was resized (SIGWINCH)
static bool con_wantresize = false;             // a full-screen session is waiting for keys: a resize is reported as -3

static void con_sigwinch( int sig ) {
    (void) sig;
    con_winch = 1;
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
        if ( n == 1 && chunk[0] == 033 && con_escbreak ) {
            con_break = 1;                      // a lone ESC
            continue;
        }
        for ( ssize_t i = 0; i < n && con_inqlen < sizeof(con_inq); ++i ) {
            con_inq[ con_inqlen++ ] = chunk[i];
        }
    }
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
        if ( con_wantresize && con_winch ) {
            return -3;
        }
        struct pollfd pfd = { 0, POLLIN, 0 };
        int rv = poll( &pfd, 1, timeout < 0 ? 100 : timeout );
        if ( rv < 0 ) {
            if ( errno == EINTR ) {
                continue;
            }
            return -1;
        }
        if ( rv == 0 ) {
            if ( timeout < 0 ) {
                continue;
            }
            return -2;
        }
        unsigned char c;
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

static bool con_linemode = false;       // a line is being edited: the terminal's mode is the line editor's business (see con_getline())
static bool con_aftercr = false;        // the last key was a CR: a LF right behind it (a file with CR LF line ends) is part of it

static int con_readkey( bool wait ) {
    fflush( stdout );
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

static bool con_interrupt( struct _vm_t* vm, void* usrdata ) {
    (void) vm; (void) usrdata;
    static unsigned counter = 0;
    if ( con_pumping && ( ++counter & 63U ) == 0 ) {
        con_pump();
    }
    return con_break != 0;
}

static void con_breakesc( struct _vm_t* vm, void* usrdata, bool on ) {
    (void) vm; (void) usrdata;
    con_escbreak = on;
}

static bool con_save( struct _vm_t* vm, void* usrdata, const char* filename, bool ascii ) {
    (void) vm;
    return repl_save_program( (repl_t*) usrdata, filename, ascii );
}

// the display statements: the text ones that a terminal can do, an error for the rest
static bool con_gfx_op( void* usrdata, int op, const double* a, int na, const char* s, size_t slen, uint8_t* out, size_t outsz,
    size_t* outlen, const char** err ) {
    (void) usrdata; (void) na; (void) s; (void) slen; (void) out; (void) outsz; (void) outlen;
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
        case GFXOP_COLOR:
            return true;            // the Engine's palette is not the terminal's: use ANSI sequences in PRINT for colors
        default:
            *err = "Not available in the console host";
            return false;
    }
}

static bool con_gfx_query( void* usrdata, int q, const double* a, int na, double* result ) {
    (void) usrdata; (void) a; (void) na;
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

static bool con_init( repl_t* repl ) {
    if ( !init_repl( repl ) ) {
        fprintf( stderr, "failed to initialize the BASIC system\n" );
        return false;
    }
    repl->report    = con_report;
    repl->print     = con_print;
    repl->printn    = con_printn;
    repl->inkey     = con_inkey;
    repl->input     = con_input;
    repl->getkey    = con_getkey;
    repl->interrupt = con_interrupt;
    repl->breakesc  = con_breakesc;
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
    if ( con_pendpos < con_pendlen || con_inqpos < con_inqlen || con_winch ) {
        return true;
    }
    struct pollfd pfd = { 0, POLLIN, 0 };
    return poll( &pfd, 1, 0 ) > 0 && ( pfd.revents & POLLIN ) != 0;
}

static bool con_drv_nextkey( scrkey_t* k ) {
    for (;;) {
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
            continue;
        }
        if ( con_scrkey( c, k ) ) {
            return true;
        }
    }
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

static const scrdrv_t con_drv = {
    con_drv_screensize, con_drv_getsize,
    con_drv_select, txtscr_selected, txtscr_setviewport, txtscr_resetviewport, con_drv_getviewport,
    txtscr_setcolors, txtscr_setblank, txtscr_clear, txtscr_write, con_drv_print, txtscr_locate, con_drv_getcursor,
    con_drv_changed, txtscr_setresizereserve, con_drv_restart,
    con_drv_begin, con_drv_end, con_drv_nextkey, con_drv_pending, con_drv_pollkeys
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
    int r = scr_lineedit( &con_linedrv, work, bufsz, 0, 0 );
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
    if ( con_midline && con_stdout_tty ) {       // the editor starts on a line of its own
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
    repl->monitor = false;
    if ( !isatty( 0 ) || !isatty( 1 ) ) {
        con_report( repl, "? EDIT and MONITOR need a terminal\n" );
        return;
    }
    fflush( stdout );
    struct sigaction winch, oldwinch;
    memset( &winch, 0, sizeof(winch) );
    winch.sa_handler = con_sigwinch;
    sigemptyset( &winch.sa_mask );
    sigaction( SIGWINCH, &winch, &oldwinch );
    con_catch_break( true );                    // Ctrl-C does not kill the process while the terminal is changed
    con_pumping = false;                        // (the monitor asks for it while it runs a program)
    con_set_raw( true );
    con_inqpos = con_inqlen = 0;
    con_pendpos = con_pendlen = 0;
    con_winch = 1;                              // the first thing is to look at the size
    if ( conscr_begin() ) {
        con_fullscreen = true;
        if ( ed ) {
            fulledit_run( repl, &con_drv );
        }
        if ( mon ) {
            monitor_run( repl, &con_drv );
        }
        con_fullscreen = false;
        conscr_end();
    } else {
        con_report( repl, "? EDIT and MONITOR need a terminal\n" );
    }
    con_wantctrl = false;
    con_wantresize = false;
    con_set_raw( false );
    con_catch_break( false );
    sigaction( SIGWINCH, &oldwinch, 0 );
    repl->fulledit = false;
    repl->monitor = false;
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
    int status = EXIT_SUCCESS;
    if ( repl_load_image( &repl, embedded_program_start, (size_t)( embedded_program_end - embedded_program_start ) ) ) {
        con_catch_break( true );
        bool ok = repl_run( &repl );
        con_catch_break( false );
        con_run_windows( &repl );
        fflush( stdout );
        status = repl.quit ? repl.exitcode : ok ? EXIT_SUCCESS : 1;
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
            return EXIT_SUCCESS;
        }
        if ( strcmp( arg, "-i" ) == 0 ) {
            interact = true;
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
        if ( interact && !repl.quit ) {
            con_repl( &repl, isatty( 0 ) );
            if ( repl.quit ) {
                status = repl.exitcode;
            }
        }
    } else {
        con_repl( &repl, isatty( 0 ) );
        if ( repl.quit ) {
            status = repl.exitcode;
        }
    }
    fflush( stdout );
    cleanup_repl( &repl );
    free( progargs );
    return status;
}
