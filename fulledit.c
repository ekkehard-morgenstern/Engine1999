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

#include "fulledit.h"
#include "scrdrv.h"
#include "textscreen.h"      // (only the TXTSCR_* constants; every call goes through the screen driver)
#include "ansi.h"
#include "baspgm.h"

// -- layout and colors -----------------------------------------------------------------

#define ED_SCREEN       1               // text screen of the editor
#define ED_PANE_Y       1               // first row of the panes (row 0: title bar)
#define ED_NUM_W        7               // line number pane: 6 digits and a space
#define ED_TEXT_X       ED_NUM_W
#define ED_NUMDIGITS    6
#define ED_MINCOLS      24              // smaller screens only show a message
#define ED_MINROWS      6
// The rest follows the size of the screen (the window's is 80 x 25: 23 pane rows, 72 text columns, the scroll bar in column 79): see ed_layout().

// palette entries (colinittab in sdllayer.c)
#define C_BLACK         1
#define C_NAVY          2
#define C_DARKRED       51
#define C_DARKSLATEGRAY 22
#define C_DARKSLATEBLUE 28
#define C_DIMGRAY       35
#define C_LIGHTGRAY     80
#define C_GAINSBORO     86
#define C_WHITESMOKE    102
#define C_GOLD          123
#define C_YELLOW        137
#define C_LIGHTYELLOW   138
#define C_WHITE         140

#define C_TEXT_BG       C_WHITESMOKE    // program lines
#define C_TEXT_FG       C_BLACK
#define C_CUR_BG        C_LIGHTYELLOW   // the line being edited
#define C_EMPTY_BG      C_GAINSBORO     // below the last line
#define C_NUM_BG        C_NAVY          // line number pane
#define C_NUM_FG        C_GOLD
#define C_NUMEMPTY_BG   C_DARKSLATEGRAY
#define C_CURNUM_BG     C_GOLD
#define C_CURNUM_FG     C_BLACK
#define C_BAR_BG        C_NAVY          // title bar
#define C_BAR_FG        C_WHITE
#define C_HELP_BG       C_DARKSLATEGRAY // help bar
#define C_HELP_FG       C_WHITE
#define C_HELPKEY_FG    C_YELLOW
#define C_MSG_BG        C_DARKRED
#define C_MSG_FG        C_YELLOW
#define C_SBTRACK_BG    C_LIGHTGRAY
#define C_SBTHUMB_BG    C_DARKSLATEBLUE

// -- state -----------------------------------------------------------------------------

typedef struct _ed_t {
    const scrdrv_t* drv;
    int         cols, rows;             // the screen
    int         pane_h;                 // rows of the panes (the row below them is the help bar)
    int         text_w;                 // columns of the text pane (the scroll bar follows)
    size_t      bufmax;                 // longest line that can be edited (the rest of the pane is free)
    bool        toosmall;               // the screen is too small for the panes
    repl_t*     repl;
    program_t*  pgm;
    line_t*     top;                    // first line shown
    line_t*     cur;                    // the current line; while inserting: the line the new one follows (0 = none)
    bool        ins;                    // a new line is being inserted after cur
    bool        numphase;               // inserting: the line number is being typed
    char        num[ED_NUMDIGITS + 1];
    int         numlen;
    uint32_t    newno;                  // inserting, text phase: the number of the new line
    char*       buf;                    // text of the current / new line, without its number (room for the largest screen)
    scredit_t   ed;
    bool        dirty;                  // the text was changed
    bool        readonly;               // the line is too long to edit
    bool        escwarn;                // ESC failed once because of an error: the next one discards
    char        msg[96];                // error shown in the help bar until the next key
    bool        quit;
    int         sbtrack, sbsize, sbmaxtop;  // the scroll bar as last drawn: track rows (without the arrows), thumb size, the last place of the view (index of the top line; 0 = nothing to scroll)
    bool        sbdrag;                 // the left button went down on the scroll bar and is still held
} ed_t;

// remembered between calls: line numbers (the lines may be gone or replaced)
static uint32_t ed_memcur = LINENO_NONE, ed_memtop = LINENO_NONE;

// message of a failed repl_enter()
static char ed_repmsg[96];

static void ed_report( struct _repl_t* repl, const char* text ) {
    if ( text[0] == '?' && text[1] == ' ' ) {
        text += 2;
    }
    snprintf( ed_repmsg, sizeof(ed_repmsg), "%s", text );
    size_t n = strlen( ed_repmsg );
    while ( n > 0 && ( ed_repmsg[n-1] == '\n' || ed_repmsg[n-1] == '\r' ) ) {
        ed_repmsg[--n] = '\0';
    }
}

static void ed_setmsg( ed_t* e, const char* text ) {
    snprintf( e->msg, sizeof(e->msg), "%s", text );
}

// -- program access --------------------------------------------------------------------

// the line with this number, else the next one, else the last one, else 0
static line_t* ed_at( ed_t* e, uint32_t lineno ) {
    for ( line_t* ln = e->pgm->first; ln != 0; ln = ln->next ) {
        if ( ln->lineno >= lineno ) {
            return ln;
        }
    }
    return e->pgm->last;
}

// the text of a line as the listing shows it; *pfx receives the length of "number + space" at the start. Returns
// a malloc'ed string (hl: with the escape sequences of the highlighting style), 0 if out of memory
static char* ed_linetext( ed_t* e, line_t* ln, bool hl, size_t* pfx ) {
    buf_t* b = create_buffer();
    if ( b == 0 ) {
        return 0;
    }
    pgmiter_t iter; clear_iter( &iter, e->pgm );
    char* text = 0;
    if ( iter_load_line( &iter, ln ) && ( hl && e->repl->hlstyle != 0
            ? detokenize_line_hl( b, iter.tok, &iter.hdr, e->repl->hlstyle, 0 )
            : detokenize_line( b, iter.tok, &iter.hdr ) ) ) {
        text = strdup( b->buffer );
    }
    delete_buffer( b );
    if ( text != 0 && pfx != 0 ) {      // only meaningful for the plain text
        char num[16];
        size_t n = (size_t) snprintf( num, sizeof(num), "%u", (unsigned) ln->lineno );
        *pfx = text[n] == ' ' ? n + 1U : n;
    }
    return text;
}

// text of the line without its number (plain), malloc'ed
static char* ed_bodytext( ed_t* e, line_t* ln ) {
    size_t pfx = 0;
    char* text = ed_linetext( e, ln, false, &pfx );
    if ( text == 0 ) {
        return 0;
    }
    memmove( text, text + pfx, strlen( text + pfx ) + 1U );
    return text;
}

static int ed_rowsfor( ed_t* e, size_t len ) {
    return len == 0 ? 1 : (int)( ( len + (size_t) e->text_w - 1U ) / (size_t) e->text_w );
}

static int ed_linerows( ed_t* e, line_t* ln ) {
    char* text = ed_bodytext( e, ln );
    int rows = text != 0 ? ed_rowsfor( e, strlen( text ) ) : 1;
    free( text );
    return rows;
}

// rows of the edited text (the cursor may sit behind the last character, on a row of its own)
static int ed_editrows( ed_t* e ) {
    return (int)( e->ed.len / (size_t) e->text_w ) + 1;
}

// -- the edited text -------------------------------------------------------------------

static void ed_settext( ed_t* e, const char* text, size_t pos ) {
    size_t len = strlen( text );
    e->readonly = len > e->bufmax;
    if ( e->readonly ) {
        len = 0;
        text = "";
    }
    memcpy( e->buf, text, len + 1U );
    e->ed.buf = e->buf; e->ed.len = len; e->ed.maxlen = e->bufmax;
    e->ed.pos = pos < len ? pos : len;
    e->dirty = false;
}

// loads the text of the current line
static void ed_loadcur( ed_t* e, size_t pos ) {
    char* text = e->cur != 0 ? ed_bodytext( e, e->cur ) : 0;
    ed_settext( e, text != 0 ? text : "", pos );
    if ( e->cur != 0 && text != 0 && strlen( text ) > e->bufmax ) {
        ed_setmsg( e, "Line too long to edit here" );
    }
    free( text );
}

// -- drawing ---------------------------------------------------------------------------

typedef struct _put_t {
    ed_t*   e;
    int     y0;         // first row of the entry in the text pane
    int     pos;        // characters put so far
    size_t  skip;       // characters of the line number prefix still to skip
} put_t;

// puts text into the text pane, wrapping at its right edge (the pane clips at the bottom)
static void ed_put( void* usrdata, int bg, int fg, const char* text, size_t len ) {
    put_t* p = (put_t*) usrdata;
    if ( p->skip > 0 ) {
        size_t n = len < p->skip ? len : p->skip;
        p->skip -= n; text += n; len -= n;
    }
    while ( len > 0 ) {
        int row = p->pos / p->e->text_w, col = p->pos % p->e->text_w;
        int n = p->e->text_w - col;
        if ( (size_t) n > len ) {
            n = (int) len;
        }
        p->e->drv->write( p->y0 + row, col, bg, fg, text, n );
        p->pos += n; text += n; len -= (size_t) n;
    }
}

typedef struct _slot_t {
    line_t* line;       // 0 = the new line being inserted
    int     y, rows;
    bool    edited;
} slot_t;

// makes the edited entry visible: scrolls so that the lines from top to the end of the edited one fit the panes
static void ed_scroll( ed_t* e ) {
    if ( e->pgm->first == 0 ) {
        e->top = 0;
        return;
    }
    if ( e->cur == 0 ) {
        e->cur = e->pgm->first;
    }
    if ( e->top == 0 ) {
        e->top = e->pgm->first;
    }
    int rows = 0; bool found = false;
    for ( line_t* ln = e->top; ln != 0; ln = ln->next ) {
        rows += ln == e->cur && !e->ins ? ed_editrows( e ) : ed_linerows( e, ln );
        if ( ln == e->cur ) {
            found = true;
            break;
        }
    }
    if ( !found ) {
        e->top = e->cur;
        return;
    }
    if ( e->ins ) {
        rows += ed_editrows( e );
    }
    while ( rows > e->pane_h && e->top != e->cur ) {
        rows -= ed_linerows( e, e->top );
        e->top = e->top->next;
    }
}

static int ed_countlines( ed_t* e, line_t* upto ) {      // number of lines before upto (0 = all lines)
    int n = 0;
    for ( line_t* ln = e->pgm->first; ln != 0 && ln != upto; ln = ln->next ) {
        ++n;
    }
    return n;
}

static void ed_drawbars( ed_t* e ) {
    const scrdrv_t* d = e->drv;
    d->setviewport( 0, 0, e->cols, 1 );
    d->setblank( C_BAR_BG, C_BAR_FG );
    d->clear();
    d->write( 0, 1, C_BAR_BG, C_BAR_FG, "Text Editor", -1 );
    if ( e->repl->filename != 0 ) {
        const char* name = e->repl->filename;
        int len = (int) strlen( name );
        int maxname = e->cols - 16 < 40 ? e->cols - 16 : 40;
        if ( maxname < 0 ) {
            maxname = 0;
        }
        if ( len > maxname ) {
            name += len - maxname; len = maxname;     // the end of a long name is the telling part
        }
        d->write( 0, e->cols - 1 - len, C_BAR_BG, C_YELLOW, name, len );
    }
    d->setviewport( 0, ED_PANE_Y + e->pane_h, e->cols, 1 );
    if ( e->msg[0] != '\0' ) {
        d->setblank( C_MSG_BG, C_MSG_FG );
        d->clear();
        d->write( 0, 1, C_MSG_BG, C_MSG_FG, e->msg, -1 );
        return;
    }
    d->setblank( C_HELP_BG, C_HELP_FG );
    d->clear();
    static const struct { const char* key; const char* what; } help[] = {
        { "ESC", "Close" }, { "^Y", "Del line" }, { "RET", "New line" }, { "PgUp/PgDn", "Scroll" },
        { "^Home/^End", "First/last" },
    };
    int x = 1;
    for ( size_t i=0; i < sizeof(help) / sizeof(help[0]); ++i ) {
        x += d->write( 0, x, C_HELP_BG, C_HELPKEY_FG, help[i].key, -1 );
        x += d->write( 0, x, C_HELP_BG, C_HELP_FG, " ", 1 );
        x += d->write( 0, x, C_HELP_BG, C_HELP_FG, help[i].what, -1 );
        x += 2;
    }
}

// the last place of the view: the index of the first line shown when the end of the program is at the bottom of the panes (0 if it all fits)
static int ed_maxtop( ed_t* e ) {
    int rows = 0, count = 0;
    for ( line_t* ln = e->pgm->last; ln != 0; ln = ln->prev ) {
        rows += ed_linerows( e, ln );
        if ( rows > e->pane_h ) {
            break;
        }
        ++count;
    }
    int total = ed_countlines( e, 0 );
    return total > count ? total - count : 0;
}

static void ed_drawscrollbar( ed_t* e, int shown ) {
    const scrdrv_t* d = e->drv;
    d->setviewport( ED_TEXT_X + e->text_w, ED_PANE_Y, 1, e->pane_h );
    d->setblank( C_SBTRACK_BG, C_BLACK );
    d->clear();
    d->write( 0, 0, C_SBTHUMB_BG, C_WHITE, "^", 1 );
    d->write( e->pane_h - 1, 0, C_SBTHUMB_BG, C_WHITE, "v", 1 );
    int track = e->pane_h - 2;
    int total = ed_countlines( e, 0 );
    int size = track, pos = 0;
    int maxtop = ed_maxtop( e );
    e->sbtrack = track; e->sbmaxtop = maxtop;
    if ( total > shown && total > 0 ) {
        size = track * shown / total;
        if ( size < 1 ) {
            size = 1;
        }
        // the thumb shows the place of the view (the first line shown) in the program
        int idx = e->top != 0 ? ed_countlines( e, e->top ) : 0;
        pos = maxtop > 0 ? ( track - size ) * idx / maxtop : 0;
        if ( pos > track - size ) {
            pos = track - size;
        }
    }
    for ( int i=0; i < size; ++i ) {
        d->write( 1 + pos + i, 0, C_SBTHUMB_BG, C_WHITE, " ", 1 );
    }
    e->sbsize = size;
}

static void ed_numtext( ed_t* e, const slot_t* s, char* out ) {      // 7 characters + NUL
    if ( s->line == 0 ) {
        if ( e->numphase ) {
            memset( out, '.', ED_NUMDIGITS );
            memcpy( out, e->num, (size_t) e->numlen );
            out[ED_NUMDIGITS] = ' '; out[ED_NUM_W] = '\0';
        } else {
            snprintf( out, ED_NUM_W + 1, "%6u ", (unsigned) e->newno );
        }
    } else {
        snprintf( out, ED_NUM_W + 1, "%6u ", (unsigned) s->line->lineno );
    }
}

// redraws everything and places the cursor
static void ed_draw( ed_t* e ) {
    const scrdrv_t* d = e->drv;
    if ( e->toosmall ) {
        d->resetviewport();
        d->setblank( C_MSG_BG, C_MSG_FG );
        d->clear();
        d->write( 0, 0, C_MSG_BG, C_MSG_FG, "Too small", e->cols < 9 ? e->cols : 9 );
        d->changed();
        return;
    }
    ed_scroll( e );
    // layout: the entries from top on (the new line follows cur)
    slot_t* slots = (slot_t*) malloc( ( (size_t) e->pane_h + 1U ) * sizeof(slot_t) );
    if ( slots == 0 ) {
        return;
    }
    int nslots = 0, y = 0;
    line_t* ln = e->top;
    bool pendingnew = e->ins && e->cur == 0;
    for ( ;; ) {
        slot_t s;
        if ( pendingnew ) {
            s.line = 0; s.edited = true; s.rows = ed_editrows( e );
            pendingnew = false;
        } else if ( ln != 0 ) {
            s.line = ln; s.edited = !e->ins && ln == e->cur;
            s.rows = s.edited ? ed_editrows( e ) : ed_linerows( e, ln );
            if ( e->ins && ln == e->cur ) {
                pendingnew = true;      // the new line comes next; then the loop continues with the following line
            }
            ln = ln->next;
        } else {
            break;
        }
        if ( y >= e->pane_h ) {
            break;
        }
        s.y = y; y += s.rows;
        slots[nslots++] = s;
        if ( nslots > e->pane_h ) {
            break;
        }
    }
    char numtext[ED_NUM_W + 1];
    // line number pane
    d->setviewport( 0, ED_PANE_Y, ED_NUM_W, e->pane_h );
    d->setblank( C_NUMEMPTY_BG, C_NUM_FG );
    d->clear();
    for ( int i=0; i < nslots; ++i ) {
        const slot_t* s = &slots[i];
        int bg = s->edited ? C_CURNUM_BG : C_NUM_BG, fg = s->edited ? C_CURNUM_FG : C_NUM_FG;
        for ( int r=0; r < s->rows; ++r ) {
            if ( r == 0 ) {
                ed_numtext( e, s, numtext );
            } else {
                strcpy( numtext, "       " );
            }
            d->write( s->y + r, 0, bg, fg, numtext, ED_NUM_W );
        }
    }
    // text pane
    d->setviewport( ED_TEXT_X, ED_PANE_Y, e->text_w, e->pane_h );
    d->setblank( C_EMPTY_BG, C_DIMGRAY );
    d->clear();
    char* blanks = (char*) malloc( (size_t) e->text_w );
    if ( blanks == 0 ) {
        free( slots );
        return;
    }
    memset( blanks, ' ', (size_t) e->text_w );
    int curx = 0, cury = 0;
    for ( int i=0; i < nslots; ++i ) {
        const slot_t* s = &slots[i];
        int bg = s->edited ? C_CUR_BG : C_TEXT_BG;
        for ( int r=0; r < s->rows; ++r ) {
            d->write( s->y + r, 0, bg, C_TEXT_FG, blanks, e->text_w );
        }
        put_t p = { e, s->y, 0, 0 };
        if ( s->edited ) {
            ed_put( &p, bg, C_TEXT_FG, e->buf, strlen( e->buf ) );
            curx = (int)( e->ed.pos % (size_t) e->text_w ); cury = s->y + (int)( e->ed.pos / (size_t) e->text_w );
        } else {
            size_t pfx = 0;
            char* plain = ed_linetext( e, s->line, false, &pfx );
            if ( plain == 0 ) {
                continue;
            }
            p.skip = pfx;
            if ( e->repl->hlstyle != 0 ) {
                char* hl = ed_linetext( e, s->line, true, 0 );
                if ( hl != 0 ) {
                    ansi_t a;
                    ansi_init( &a, C_TEXT_FG, bg );
                    ansi_feed( &a, hl, ed_put, &p );
                    free( hl );
                }
            } else {
                ed_put( &p, bg, C_TEXT_FG, plain, strlen( plain ) );
            }
            free( plain );
        }
    }
    // scroll bar, bars
    int shown = 0;
    for ( int i=0; i < nslots; ++i ) {
        if ( slots[i].line != 0 ) {
            ++shown;
        }
    }
    ed_drawscrollbar( e, shown );
    ed_drawbars( e );
    // cursor: in the number pane while the number is typed, else in the text pane
    if ( e->ins && e->numphase ) {
        for ( int i=0; i < nslots; ++i ) {
            if ( slots[i].line == 0 ) {
                d->setviewport( 0, ED_PANE_Y, ED_NUM_W, e->pane_h );
                d->locate( e->numlen, slots[i].y );
            }
        }
    } else {
        d->setviewport( ED_TEXT_X, ED_PANE_Y, e->text_w, e->pane_h );
        d->locate( curx, cury );
    }
    free( blanks );
    free( slots );
    d->changed();
}

// -- changing the program --------------------------------------------------------------

// enters "text" like a typed line; false (and e->msg) if the REPL refused it
static bool ed_enter( ed_t* e, const char* text ) {
    void (*oldreport)( struct _repl_t*, const char* ) = e->repl->report;
    e->repl->report = ed_report;
    ed_repmsg[0] = '\0';
    bool ok = repl_enter( e->repl, text );
    e->repl->report = oldreport;
    if ( !ok || ed_repmsg[0] != '\0' ) {
        ed_setmsg( e, ed_repmsg[0] != '\0' ? ed_repmsg : "Line rejected" );
        return false;
    }
    return true;
}

static bool ed_enterline( ed_t* e, uint32_t lineno ) {      // the edited text as line "lineno"
    char* text = (char*) malloc( e->ed.len + 16U );
    if ( text == 0 ) {
        ed_setmsg( e, "Out of memory" );
        return false;
    }
    if ( e->ed.len > 0 ) {
        sprintf( text, "%u %s", (unsigned) lineno, e->buf );
    } else {
        sprintf( text, "%u", (unsigned) lineno );       // a line number alone deletes the line
    }
    bool ok = ed_enter( e, text );
    free( text );
    return ok;
}

// the edited text of the current line is entered if it was changed
static bool ed_commit( ed_t* e ) {
    if ( e->ins || e->cur == 0 || !e->dirty || e->readonly ) {
        return true;
    }
    uint32_t no = e->cur->lineno;
    uint32_t topno = e->top != 0 ? e->top->lineno : 0;
    if ( !ed_enterline( e, no ) ) {
        return false;
    }
    e->dirty = false;
    e->cur = ed_at( e, no );      // the line may be gone (emptied) or replaced
    e->top = ed_at( e, topno );
    if ( e->cur != 0 && e->cur->lineno != no ) {
        // deleted: continue with the next line, as with CTRL+Y
        ed_loadcur( e, 0 );
    }
    return true;
}

// moves to another line (the edited one is entered first); the column stays
static void ed_moveto( ed_t* e, line_t* ln ) {
    if ( ln == 0 || ln == e->cur ) {
        return;
    }
    size_t pos = e->ed.pos;
    uint32_t no = ln->lineno;
    if ( !ed_commit( e ) ) {
        return;
    }
    e->cur = ed_at( e, no );
    ed_loadcur( e, pos );
}

static void ed_move( ed_t* e, int delta ) {
    line_t* ln = e->cur;
    while ( ln != 0 && delta > 0 ) {
        if ( ln->next == 0 ) break;
        ln = ln->next; --delta;
    }
    while ( ln != 0 && delta < 0 ) {
        if ( ln->prev == 0 ) break;
        ln = ln->prev; ++delta;
    }
    ed_moveto( e, ln );
}

// the number for a new line after cur: the next multiple of 10, else the middle of the gap; 0 = no room
static uint32_t ed_autonumber( ed_t* e ) {
    line_t* next = e->cur != 0 ? e->cur->next : e->pgm->first;
    uint32_t base = e->cur != 0 ? e->cur->lineno : 0;
    uint32_t cand = e->cur != 0 ? ( base / 10U + 1U ) * 10U : 10U;
    if ( next != 0 && cand >= next->lineno ) {
        cand = base + ( next->lineno - base ) / 2U;
        if ( cand <= base || cand >= next->lineno ) {
            return 0;
        }
    }
    if ( cand > LINENO_MAX ) {
        cand = base + 1U;
        if ( cand > LINENO_MAX || ( next != 0 && cand >= next->lineno ) ) {
            return 0;
        }
    }
    return cand;
}

// room for a new line after the current one; the cursor goes to the line number pane
static void ed_beginins( ed_t* e ) {
    if ( !ed_commit( e ) ) {
        return;
    }
    e->ins = true; e->numphase = true; e->numlen = 0; e->num[0] = '\0';
    ed_settext( e, "", 0 );
}

static void ed_cancelins( ed_t* e ) {
    e->ins = false;
    ed_loadcur( e, 0 );
}

// the number is complete: typed, or chosen automatically; then the text follows
static void ed_endnumber( ed_t* e ) {
    uint32_t no;
    if ( e->numlen == 0 ) {
        no = ed_autonumber( e );
        if ( no == 0 ) {
            ed_setmsg( e, "No room for a new line here" );
            ed_cancelins( e );
            return;
        }
    } else {
        no = (uint32_t) strtoul( e->num, 0, 10 );
        line_t* found = 0; line_t* prev = 0;
        if ( find_line( e->pgm, no, &found, &prev ) ) {
            ed_setmsg( e, "That line number exists already" );
            return;
        }
    }
    e->newno = no;
    e->numphase = false;
}

// the new line is entered (an empty one is dropped); then go on with another new line after it if more is true
static void ed_finishins( ed_t* e, bool more ) {
    if ( e->ed.len == 0 ) {
        ed_cancelins( e );
        return;
    }
    if ( !ed_enterline( e, e->newno ) ) {
        return;
    }
    uint32_t topno = e->top != 0 ? e->top->lineno : 0;
    e->ins = false;
    e->cur = ed_at( e, e->newno );
    e->top = ed_at( e, topno );
    ed_loadcur( e, 0 );
    if ( more ) {
        ed_beginins( e );
    }
}

// leaves the insertion for a cursor movement, as if RETURN had been pressed: the new line is entered (an empty one is
// dropped, as is one whose number hasn't been completed); false if the REPL refused it, the insertion goes on then
static bool ed_endins( ed_t* e ) {
    if ( !e->ins ) {
        return true;
    }
    if ( e->numphase ) {
        ed_cancelins( e );
    } else {
        ed_finishins( e, false );
    }
    return !e->ins;
}

static void ed_deleteline( ed_t* e ) {
    if ( e->ins ) {
        ed_cancelins( e );
        return;
    }
    if ( e->cur == 0 ) {
        return;
    }
    uint32_t no = e->cur->lineno;
    uint32_t topno = e->top != 0 ? e->top->lineno : 0;
    char text[16];
    snprintf( text, sizeof(text), "%u", (unsigned) no );
    if ( !ed_enter( e, text ) ) {
        return;
    }
    e->top = ed_at( e, topno );
    e->cur = ed_at( e, no );        // the next line, else the last
    ed_loadcur( e, 0 );
}

// -- keys ------------------------------------------------------------------------------

// ESC: the new line and the edited line are entered first; if that fails, the next ESC discards them
static void ed_leave( ed_t* e ) {
    if ( e->ins ) {
        if ( e->ed.len > 0 && !e->escwarn ) {
            ed_finishins( e, false );
            if ( e->ins ) {         // refused
                e->escwarn = true;
                return;
            }
        } else {
            e->ins = false;
        }
    }
    if ( !e->escwarn && !ed_commit( e ) ) {
        e->escwarn = true;
        return;
    }
    e->quit = true;
}

// editing keys on the text; the line counts as changed when its length does (cursor keys don't change it)
static void ed_edit( ed_t* e, const scrkey_t* k ) {
    if ( e->readonly ) {
        return;
    }
    size_t oldlen = e->ed.len;
    scr_editkey( &e->ed, k );
    if ( e->ed.len != oldlen ) {
        e->dirty = true;
    }
}

static void ed_key( ed_t* e, const scrkey_t* k ) {
    bool ctrl = ( k->mods & KSMOD_CTRL ) != 0;
    if ( k->kind != SK_ESCAPE ) {
        e->escwarn = false;
    }
    if ( k->kind == SK_CHAR && !ctrl ) {
        if ( e->ins && e->numphase ) {
            if ( k->ch >= '0' && k->ch <= '9' && e->numlen < ED_NUMDIGITS ) {
                e->num[e->numlen++] = (char) k->ch; e->num[e->numlen] = '\0';
            } else if ( k->ch == ' ' ) {
                ed_endnumber( e );
            }
        } else if ( e->ins || e->cur != 0 ) {
            ed_edit( e, k );
        }
        return;
    }
    if ( k->kind == SK_CHAR && ctrl && k->ch == 'y' ) {
        ed_deleteline( e );
        return;
    }
    switch ( k->kind ) {
        case SK_ESCAPE:
            if ( e->ins && e->numphase && e->cur != 0 ) {
                ed_cancelins( e );
            } else {        // without any line there is nothing to go back to
                ed_leave( e );
            }
            return;
        case SK_RETURN:
        case SK_TAB:
            if ( e->ins && e->numphase ) {
                ed_endnumber( e );
            } else if ( k->kind == SK_TAB ) {
                // nothing: TAB only ends the line number
            } else if ( e->ins ) {
                ed_finishins( e, e->ed.pos == e->ed.len );
            } else if ( e->cur == 0 || e->ed.pos == e->ed.len ) {
                ed_beginins( e );
            } else {
                ed_move( e, 1 );
            }
            return;
        case SK_BACKSPACE:
            if ( e->ins && e->numphase ) {
                if ( e->numlen > 0 ) {
                    e->num[--e->numlen] = '\0';
                }
                return;
            }
            break;
        case SK_KEY:
            switch ( k->key ) {
                case KS_BACKTAB:        // like TAB
                    if ( e->ins && e->numphase ) {
                        ed_endnumber( e );
                    }
                    return;
                case KS_UP:
                    if ( ed_endins( e ) ) ed_move( e, -1 );
                    return;
                case KS_DOWN:
                    if ( ed_endins( e ) ) ed_move( e, 1 );
                    return;
                case KS_PGUP:
                    if ( ed_endins( e ) ) ed_move( e, -( e->pane_h - 1 ) );
                    return;
                case KS_PGDN:
                    if ( ed_endins( e ) ) ed_move( e, e->pane_h - 1 );
                    return;
                case KS_HOME:
                    if ( ctrl ) {
                        if ( ed_endins( e ) ) ed_moveto( e, e->pgm->first );
                        return;
                    }
                    break;
                case KS_END:
                    if ( ctrl ) {
                        if ( ed_endins( e ) ) ed_moveto( e, e->pgm->last );
                        return;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            return;
    }
    if ( e->ins && e->numphase ) {
        return;
    }
    if ( e->ins || e->cur != 0 ) {
        ed_edit( e, k );
    }
}

// -- the mouse -------------------------------------------------------------------------
//
// The scroll bar (the last column of the panes) and the wheel scroll the VIEW; the cursor stays on its line as long as that is shown and is
// pulled to the first or last line shown when the view moves past it (the thumb shows the place of the view). The wheel scrolls
// ED_WHEELLINES lines per notch, anywhere on the screen; a click on the up / down arrow one line; a click on the track puts the thumb there, and
// it can be dragged while the button is held. An insertion in progress is entered first, as for the cursor keys.

#define ED_WHEELLINES   3

// the view starts at line index idx (clamped to 0 .. ed_maxtop()); the cursor moves to the nearest line that is shown if it is not any more
static void ed_setview( ed_t* e, int idx ) {
    if ( e->pgm->first == 0 || e->toosmall ) {
        return;
    }
    ed_scroll( e );
    int maxtop = ed_maxtop( e );
    idx = idx < 0 ? 0 : idx > maxtop ? maxtop : idx;
    line_t* top = e->pgm->first;
    for ( int i=0; i < idx && top->next != 0; ++i ) {
        top = top->next;
    }
    uint32_t topno = top->lineno;
    // the line the cursor should be on: where it is while it is in view, else the nearest line that is (the first, or the last one shown completely)
    line_t* target = top;
    if ( e->cur != 0 ) {
        int curidx = ed_countlines( e, e->cur );
        if ( curidx >= idx ) {
            int rows = 0;
            for ( line_t* ln = top; ln != 0; ln = ln->next ) {
                rows += ed_linerows( e, ln );
                if ( rows > e->pane_h ) {
                    break;
                }
                target = ln;
                if ( ln == e->cur ) {
                    break;
                }
            }
        }
    }
    if ( !ed_endins( e ) ) {
        return;
    }
    if ( target != e->cur && target != 0 ) {
        ed_moveto( e, target );             // (enters the edited line first: the nodes are looked up again by number below)
    }
    e->top = ed_at( e, topno );
}

static void ed_viewby( ed_t* e, int delta ) {
    ed_scroll( e );                     // (the view as it is drawn: the cursor is in it)
    ed_setview( e, ( e->top != 0 ? ed_countlines( e, e->top ) : 0 ) + delta );
}

// the thumb is put so that its middle is at track row t (0 = the first row below the up arrow)
static void ed_scrolltrack( ed_t* e, int t ) {
    int room = e->sbtrack - e->sbsize;          // the places the thumb's top can have: 0..room
    if ( e->sbmaxtop < 1 || room <= 0 ) {
        return;                                 // everything is shown: nothing to scroll
    }
    int pos = t - e->sbsize / 2;
    if ( pos < 0 ) {
        pos = 0;
    }
    if ( pos > room ) {
        pos = room;
    }
    ed_setview( e, ( pos * e->sbmaxtop + room / 2 ) / room );       // (the inverse of the drawing: pos = room * idx / maxtop)
}

static void ed_mouse( ed_t* e, const scrkey_t* k ) {
    int sbx = ED_TEXT_X + e->text_w;
    bool inpanes = k->my >= ED_PANE_Y && k->my < ED_PANE_Y + e->pane_h;
    switch ( k->mouse ) {
        case SM_WHEEL:
            ed_viewby( e, -k->wheel * ED_WHEELLINES );
            break;
        case SM_PRESS:
            e->sbdrag = false;
            if ( k->button != 1 || k->mx != sbx || !inpanes ) {
                break;
            }
            {
                int r = k->my - ED_PANE_Y;
                if ( r == 0 ) {
                    ed_viewby( e, -1 );
                } else if ( r == e->pane_h - 1 ) {
                    ed_viewby( e, 1 );
                } else {
                    ed_scrolltrack( e, r - 1 );
                    e->sbdrag = true;
                }
            }
            break;
        case SM_DRAG:
            if ( e->sbdrag ) {
                int t = k->my - ED_PANE_Y - 1;          // (above or below the bar: the first or the last place)
                ed_scrolltrack( e, t < 0 ? 0 : t >= e->sbtrack ? e->sbtrack - 1 : t );
            }
            break;
        case SM_RELEASE:
            e->sbdrag = false;
            break;
        default:
            break;
    }
}

// without any line the editor is always inserting: the cursor waits in the line number pane
static void ed_idle( ed_t* e ) {
    if ( !e->quit && !e->ins && e->cur == 0 ) {
        ed_beginins( e );
    }
}

// -- the editor ------------------------------------------------------------------------

// the layout for the size of the screen: the panes, the text width, the longest line that can be edited
static void ed_layout( ed_t* e ) {
    e->drv->screensize( &e->cols, &e->rows );
    e->toosmall = e->cols < ED_MINCOLS || e->rows < ED_MINROWS;
    e->pane_h = e->rows - 2;
    e->text_w = e->cols - ED_NUM_W - 1;
    if ( e->toosmall ) {
        e->pane_h = 1; e->text_w = 1;
    }
    e->bufmax = (size_t) e->text_w * (size_t)( e->pane_h - 1 );
    if ( e->bufmax > (size_t) TXTSCR_MAXW * TXTSCR_MAXH ) {
        e->bufmax = (size_t) TXTSCR_MAXW * TXTSCR_MAXH;
    }
    e->ed.maxlen = e->bufmax;
}

void fulledit_run( repl_t* repl, const scrdrv_t* drv ) {
    ed_t* e = (ed_t*) calloc( 1, sizeof(ed_t) );
    if ( e == 0 ) {
        return;
    }
    e->buf = (char*) malloc( (size_t) TXTSCR_MAXW * TXTSCR_MAXH + 1U );
    if ( e->buf == 0 ) {
        free( e );
        return;
    }
    e->drv = drv;
    e->repl = repl; e->pgm = &repl->pgm;
    int oldscreen = drv->selected();
    drv->select( ED_SCREEN );
    ed_layout( e );
    e->top = ed_memtop != LINENO_NONE ? ed_at( e, ed_memtop ) : e->pgm->first;
    e->cur = ed_memcur != LINENO_NONE ? ed_at( e, ed_memcur ) : e->pgm->first;
    if ( e->cur == 0 ) {
        e->top = 0;
    }
    ed_loadcur( e, 0 );
    ed_idle( e );
    int state = drv->begin();
    if ( drv->wantmouse != 0 ) {
        drv->wantmouse( true );
    }
    ed_draw( e );
    while ( !e->quit ) {
        scrkey_t k;
        if ( !drv->nextkey( &k ) ) {
            break;
        }
        bool any = false;
        for ( ;; ) {
            if ( k.kind == SK_BREAK ) {
                k.kind = SK_ESCAPE;         // (the break key leaves the editor like ESC)
            }
            if ( k.kind == SK_RESIZE ) {
                ed_layout( e );
                any = true;
            } else if ( k.kind == SK_CLOSED || k.kind == SK_EOF ) {
                e->quit = true;
            } else if ( k.kind == SK_MOUSE ) {
                if ( !e->toosmall ) {
                    ed_mouse( e, &k );
                    ed_idle( e );
                    any = true;
                }
            } else if ( e->toosmall ) {
                if ( k.kind == SK_ESCAPE ) {
                    e->quit = true;
                }
            } else {
                e->msg[0] = '\0';
                ed_key( e, &k );
                ed_idle( e );
                any = true;
            }
            if ( e->quit || !drv->pending() || !drv->nextkey( &k ) ) {
                break;
            }
        }
        if ( any ) {
            ed_draw( e );
        }
    }
    if ( drv->wantmouse != 0 ) {
        drv->wantmouse( false );
    }
    drv->end( state );
    ed_memcur = e->cur != 0 ? e->cur->lineno : LINENO_NONE;
    ed_memtop = e->top != 0 ? e->top->lineno : LINENO_NONE;
    drv->resetviewport();
    drv->select( oldscreen );
    drv->changed();
    free( e->buf );
    free( e );
}
