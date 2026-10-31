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
#include "version.h"

/*
The console host of the Engine: the BASIC system without a window, for shell scripts and servers (docs/console-host.md).

    engine1999c [options] file [arguments]     loads the program file (.bas listing, .e99 binary; a first line #! is skipped), runs it
                                               and ends; the exit status is the code of BYE / QUIT, 0 when the program just ends, 1 after
                                               an error, 2 if the file can't be loaded
    engine1999c [options] -                    reads BASIC lines from standard input (see below)
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

// Ctrl-C is Break while the BASIC system runs something; between lines the default action (end the process) is in effect
static void con_catch_break( bool on ) {
    struct sigaction sa;
    memset( &sa, 0, sizeof(sa) );
    sa.sa_handler = on ? con_sigint : SIG_DFL;      // no SA_RESTART: a blocked read ends and the VM sees the Break
    sigemptyset( &sa.sa_mask );
    sigaction( SIGINT, &sa, 0 );
    if ( on ) {
        con_break = 0;
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
static bool con_midline = false;        // the last character written to standard output was not a newline

static void con_print( struct _vm_t* vm, void* usrdata, int channel, const char* text ) {
    (void) vm; (void) usrdata;
    if ( channel == 0 && text[0] != '\0' ) {
        fputs( text, stdout );
        con_midline = text[ strlen( text ) - 1U ] != '\n';
    }
}

// binary-safe output: the zero bytes of CHR$(0) reach the output as they are
static void con_printn( struct _vm_t* vm, void* usrdata, int channel, const char* text, size_t len ) {
    (void) vm; (void) usrdata;
    if ( channel == 0 && len > 0 ) {
        fwrite( text, 1U, len, stdout );
        con_midline = text[ len - 1U ] != '\n';
    }
}

static void con_report( struct _repl_t* repl, const char* text ) {
    (void) repl;
    if ( con_midline && con_stdout_tty ) {      // on a terminal the message starts on a line of its own (not in a pipe: that is data)
        fputc( '\n', stdout );
        con_midline = false;
    }
    fflush( stdout );           // keep the order of the two streams when both go to the same place
    fputs( text, stderr );
}

// INPUT: a line from standard input; the prompt has been printed already, so it is flushed first
static char* con_input( struct _vm_t* vm, void* usrdata, int channel ) {
    (void) vm; (void) usrdata;
    if ( channel != 0 ) {
        return 0;
    }
    fflush( stdout );
    con_set_raw( false );
    char* line = 0; size_t cap = 0;
    ssize_t n = getline( &line, &cap, stdin );
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

// a key code (RETURN 13, BACKSPACE 8, TAB 9 and the printable characters, as in the window), 0 = none yet, -1 = end of input / Break.
// wait: block until there is one. Other keys (escape sequences of the cursor keys, control codes) are skipped.
static int con_readkey( bool wait ) {
    fflush( stdout );
    con_set_raw( true );
    for (;;) {
        if ( con_break ) {
            return -1;
        }
        struct pollfd pfd = { 0, POLLIN, 0 };
        int rv = poll( &pfd, 1, wait ? 100 : 0 );
        if ( rv < 0 ) {
            if ( errno == EINTR ) {
                continue;
            }
            return -1;
        }
        if ( rv == 0 ) {
            if ( wait ) {
                continue;
            }
            return 0;
        }
        unsigned char c;
        ssize_t n = read( 0, &c, 1U );
        if ( n == 0 ) {
            return wait ? -1 : 0;           // end of input: INKEY$ sees "no key", GET ends
        }
        if ( n < 0 ) {
            if ( errno == EINTR ) {
                continue;
            }
            return -1;
        }
        if ( c == '\n' || c == '\r' ) return 13;
        if ( c == 8 || c == 127 ) return 8;
        if ( c == '\t' ) return 9;
        if ( c >= 0x20 && c < 0x7F ) return c;
        // anything else is skipped
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
    return con_break != 0;
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
    repl->save      = con_save;
    repl->gfx       = &con_gfx;
    repl->userdata  = repl;
    // no sound, no editor, no pager (a listing is not paged), no syntax colors
    return true;
}

// -- the two modes ---------------------------------------------------------------------------

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
        ssize_t n = getline( &line, &cap, stdin );
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
        if ( repl->quit ) {
            break;
        }
        if ( interactive && !stored ) {
            printf( "Ready.\n" );
        }
    }
    free( line );
}

static void usage( FILE* out, const char* prog ) {
    fprintf( out,
        "Usage: %s [options] [file [arguments]]\n"
        "\n"
        "  file         a BASIC program (.bas listing or .e99 file): it is loaded, run, and the exit status is\n"
        "               that of BYE / QUIT (0 if the program just ends, 1 after an error, 2 if it can't be loaded)\n"
        "  -            read BASIC lines from standard input (the default; interactive on a terminal)\n"
        "\n"
        "Options:\n"
        "  -i           after running the file, go on with the lines from standard input\n"
        "  --version    show the version\n"
        "  -h, --help   show this text\n"
        "\n"
        "Debug output (to standard output, off by default):\n", prog );
    basdbg_usage( out );
}

int main( int argc, char** argv ) {
    bool interact = false;
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
        if ( strcmp( arg, "-i" ) == 0 ) {
            interact = true;
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
    // the rest of the command line is for the program (not used yet: see the roadmap)
    con_stdout_tty = isatty( 1 );
    atexit( con_restore_tty );
    // lines (getline) and single keys (read) both come from standard input: without a buffer of ours in between they stay in step
    setvbuf( stdin, 0, _IONBF, 0 );

    static repl_t repl;
    if ( !con_init( &repl ) ) {
        return 2;
    }
    int status = EXIT_SUCCESS;
    if ( file != 0 ) {
        if ( !repl_load_file( &repl, file ) ) {
            cleanup_repl( &repl );
            return 2;
        }
        con_catch_break( true );
        bool ok = repl_run( &repl );
        con_catch_break( false );
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
    return status;
}
