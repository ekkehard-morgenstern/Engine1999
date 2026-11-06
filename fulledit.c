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
#include "mouse.h"
#include "cursors.h"
#include "scrdrv.h"
#include "textscreen.h"      // (only the TXTSCR_* constants; every call goes through the screen driver)
#include "ansi.h"
#include "baspgm.h"
#include "clipboard.h"
#include <time.h>

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
#define C_SEL_BG        C_DARKSLATEBLUE // the selection
#define C_SEL_FG        C_WHITE

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
    bool        sbdrag;                 // the left button went down on the scroll bar's track and is still held (the thumb is being dragged)
    int         sbarrow, sbax, sbay;    // the left button went down on an arrow (-1 up, 1 down; 0 = none) at this cell and is still held: it repeats
    int         sbsaveshape;            // the pointer's shape before the drag (the drag shows ED_DRAGSHAPE)
    bool        sel;                    // a selection has an anchor (it is empty while the anchor and the cursor are the same place)
    uint32_t    selno;                  // the anchor: the number of its line ...
    size_t      selpos;                 // ... and the position in the text of that line (the cursor end is cur / ed.pos)
    bool        selwhole;               // the selection is a whole line picked by the mouse (number pane / triple click): copied with its number, deleted as a line
    bool        seldrag;                // the left button went down in the text and is held: the mouse extends the selection
    long        lastclick_ms;           // the time, the cell and the count of the last click (double click = word, triple = line)
    int         lastclickx, lastclicky, clicks;
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

// -- the selection ---------------------------------------------------------------------
//
// The selection runs from an anchor (a line number and a position in the text of that line) to the cursor (the edited line and its position).
// It can reach over several lines, which is why the anchor is kept by line number: the lines other than the edited one are the program's.

static void ed_selclear( ed_t* e ) {
    e->sel = false;
    e->selwhole = false;
    e->seldrag = false;
}

static void ed_selbegin( ed_t* e ) {            // the anchor is the cursor, unless there is one
    if ( !e->sel && e->cur != 0 && !e->ins ) {
        e->sel = true;
        e->selno = e->cur->lineno;
        e->selpos = e->ed.pos;
    }
}

// the selection from its start to its end (false if there is none or it is empty)
static bool ed_selrange( ed_t* e, uint32_t* sno, size_t* spos, uint32_t* eno, size_t* epos ) {
    if ( !e->sel || e->ins || e->cur == 0 ) {
        return false;
    }
    uint32_t cno = e->cur->lineno;
    size_t cpos = e->ed.pos;
    if ( e->selno == cno && e->selpos == cpos ) {
        return false;
    }
    if ( e->selno < cno || ( e->selno == cno && e->selpos < cpos ) ) {
        *sno = e->selno; *spos = e->selpos; *eno = cno; *epos = cpos;
    } else {
        *sno = cno; *spos = cpos; *eno = e->selno; *epos = e->selpos;
    }
    return true;
}

static bool ed_selactive( ed_t* e ) {
    uint32_t a = 0, b = 0;
    size_t c = 0, d = 0;
    return ed_selrange( e, &a, &c, &b, &d );
}

// which characters of the line lineno (blen characters in its text) are selected: [*a, *b); *eol: the selection goes on behind the end of the line
static void ed_slotsel( ed_t* e, uint32_t lineno, size_t blen, size_t* a, size_t* b, bool* eol ) {
    uint32_t sno = 0, eno = 0;
    size_t spos = 0, epos = 0;
    *a = *b = 0;
    *eol = false;
    if ( !ed_selrange( e, &sno, &spos, &eno, &epos ) || lineno < sno || lineno > eno ) {
        return;
    }
    size_t x = lineno == sno ? spos : 0, y = lineno == eno ? epos : blen;
    *a = x > blen ? blen : x;
    *b = y > blen ? blen : y;
    *eol = lineno < eno;
}

// -- drawing ---------------------------------------------------------------------------

typedef struct _put_t {
    ed_t*   e;
    int     y0;         // first row of the entry in the text pane
    int     pos;        // characters put so far
    size_t  skip;       // characters of the line number prefix still to skip
    size_t  sel0, sel1; // the characters [sel0, sel1) of the text are selected (none if sel0 >= sel1)
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
        int wbg = bg, wfg = fg;
        if ( p->sel0 < p->sel1 ) {                  // the selected part is drawn in the selection colors
            size_t at = (size_t) p->pos;
            if ( at < p->sel0 && at + (size_t) n > p->sel0 ) {
                n = (int)( p->sel0 - at );
            } else if ( at >= p->sel0 && at < p->sel1 ) {
                if ( at + (size_t) n > p->sel1 ) {
                    n = (int)( p->sel1 - at );
                }
                wbg = C_SEL_BG; wfg = C_SEL_FG;
            }
        }
        p->e->drv->write( p->y0 + row, col, wbg, wfg, text, n );
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
        { "^C^X^V", "Copy/cut/paste" },
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
        put_t p = { e, s->y, 0, 0, 0, 0 };
        bool eol = false;
        if ( s->edited ) {
            ed_slotsel( e, e->cur != 0 ? e->cur->lineno : 0, e->ed.len, &p.sel0, &p.sel1, &eol );
            ed_put( &p, bg, C_TEXT_FG, e->buf, strlen( e->buf ) );
            curx = (int)( e->ed.pos % (size_t) e->text_w ); cury = s->y + (int)( e->ed.pos / (size_t) e->text_w );
        } else {
            size_t pfx = 0;
            char* plain = ed_linetext( e, s->line, false, &pfx );
            if ( plain == 0 ) {
                continue;
            }
            p.skip = pfx;
            ed_slotsel( e, s->line->lineno, strlen( plain ) - pfx, &p.sel0, &p.sel1, &eol );
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
        if ( eol ) {                            // the selection goes on over the end of this line: its end is marked by a selected blank
            size_t endat = (size_t) p.pos;
            int row = (int)( endat / (size_t) e->text_w );
            if ( row < s->rows ) {
                d->write( s->y + row, (int)( endat % (size_t) e->text_w ), C_SEL_BG, C_SEL_FG, " ", 1 );
            }
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

// -- selection, cut, copy and paste ------------------------------------------------------
//
// What is copied: a selection inside one line is copied as plain text; a selection over several lines, and the current line when nothing is
// selected, as program lines with their numbers (a valid listing: it can be pasted into a file, a chat, or back into the editor).
// What is pasted: text whose every line starts with a line number (and has text after it) is entered like typed lines; any other text goes
// into the current line (one line), or becomes new lines with automatic numbers behind the current one (several lines).

// the text of the line number no, malloc'ed (the edited line: as it is being edited); 0 if there is no such line
static char* ed_bodyof( ed_t* e, uint32_t no ) {
    if ( e->cur != 0 && !e->ins && e->cur->lineno == no ) {
        return strdup( e->buf );
    }
    line_t* found = 0; line_t* prev = 0;
    if ( !find_line( e->pgm, no, &found, &prev ) || found == 0 ) {
        return 0;
    }
    return ed_bodytext( e, found );
}

// the text of the stored line number no (not the edited copy), malloc'ed; 0 if there is no such line
static char* ed_progbody( ed_t* e, uint32_t no ) {
    line_t* found = 0; line_t* prev = 0;
    if ( !find_line( e->pgm, no, &found, &prev ) || found == 0 ) {
        return 0;
    }
    return ed_bodytext( e, found );
}

typedef struct _sbuf_t {
    char*   p;
    size_t  n, cap;
    bool    fail;
} sbuf_t;

static void sbuf_add( sbuf_t* b, const char* s, size_t len ) {
    if ( b->fail ) {
        return;
    }
    if ( b->n + len + 1U > b->cap ) {
        size_t cap = b->cap == 0 ? 256U : b->cap;
        while ( b->n + len + 1U > cap ) {
            cap *= 2U;
        }
        char* np = (char*) realloc( b->p, cap );
        if ( np == 0 ) {
            b->fail = true;
            return;
        }
        b->p = np; b->cap = cap;
    }
    memcpy( b->p + b->n, s, len );
    b->n += len;
    b->p[b->n] = '\0';
}

// the last line a multi-line selection takes (the line the selection ends in, unless it ends at the start of that line)
static uint32_t ed_sellast( ed_t* e, uint32_t eno, size_t epos ) {
    if ( epos > 0 ) {
        return eno;
    }
    line_t* found = 0; line_t* prev = 0;
    if ( find_line( e->pgm, eno, &found, &prev ) && found != 0 && found->prev != 0 ) {
        return found->prev->lineno;
    }
    return eno;
}

// the text for the clipboard (malloc'ed, ISO-8859-15, lines separated by LF); 0 if there is nothing
static char* ed_seltext( ed_t* e, size_t* plen ) {
    uint32_t sno = 0, eno = 0;
    size_t spos = 0, epos = 0;
    sbuf_t b = { 0, 0, 0, false };
    if ( !ed_selrange( e, &sno, &spos, &eno, &epos ) ) {
        if ( e->cur == 0 || e->ins ) {
            return 0;
        }
        char* body = ed_bodyof( e, e->cur->lineno );          // no selection: the current line, with its number
        char num[16];
        int nn = snprintf( num, sizeof(num), "%u ", (unsigned) e->cur->lineno );
        sbuf_add( &b, num, (size_t) nn );
        if ( body != 0 ) {
            sbuf_add( &b, body, strlen( body ) );
        }
        free( body );
    } else if ( sno == eno ) {
        char* body = ed_bodyof( e, sno );
        if ( e->selwhole ) {                                  // a whole line picked by the mouse: with its number
            char num[16];
            int nn = snprintf( num, sizeof(num), "%u ", (unsigned) sno );
            sbuf_add( &b, num, (size_t) nn );
        }
        if ( body != 0 ) {
            size_t len = strlen( body );
            size_t a = spos > len ? len : spos, z = epos > len ? len : epos;
            sbuf_add( &b, body + a, z - a );
            free( body );
        }
    } else {
        uint32_t last = ed_sellast( e, eno, epos );
        line_t* found = 0; line_t* prev = 0;
        find_line( e->pgm, sno, &found, &prev );
        for ( line_t* ln = found; ln != 0 && ln->lineno <= last; ln = ln->next ) {
            char* body = ed_bodyof( e, ln->lineno );
            char num[16];
            int nn = snprintf( num, sizeof(num), "%u ", (unsigned) ln->lineno );
            if ( b.n > 0 ) {
                sbuf_add( &b, "\n", 1U );
            }
            sbuf_add( &b, num, (size_t) nn );
            if ( body != 0 ) {
                sbuf_add( &b, body, strlen( body ) );
            }
            free( body );
        }
    }
    if ( b.fail || b.n == 0 ) {
        free( b.p );
        return 0;
    }
    *plen = b.n;
    return b.p;
}

static void ed_clipset( ed_t* e, const char* text, size_t len ) {
    if ( e->drv->clip_set != 0 ) {
        e->drv->clip_set( text, len );
    } else {
        clip_internal_set( text, len );
    }
}

static void ed_copy( ed_t* e ) {
    size_t len = 0;
    char* text = ed_seltext( e, &len );
    if ( text == 0 ) {
        ed_setmsg( e, "Nothing to copy" );
        return;
    }
    ed_clipset( e, text, len );
    free( text );
}

// the lines from..to (numbers) are deleted; the cursor goes to the line behind them
static void ed_deletelines( ed_t* e, uint32_t from, uint32_t to ) {
    if ( !ed_endins( e ) ) {
        return;
    }
    size_t cnt = 0, cap = 64;
    uint32_t* nums = (uint32_t*) malloc( cap * sizeof(uint32_t) );
    if ( nums == 0 ) {
        return;
    }
    line_t* found = 0; line_t* prev = 0;
    find_line( e->pgm, from, &found, &prev );
    for ( line_t* ln = found; ln != 0 && ln->lineno <= to; ln = ln->next ) {
        if ( ln->lineno < from ) {
            continue;
        }
        if ( cnt == cap ) {
            cap *= 2U;
            uint32_t* bigger = (uint32_t*) realloc( nums, cap * sizeof(uint32_t) );
            if ( bigger == 0 ) {
                break;
            }
            nums = bigger;
        }
        nums[cnt++] = ln->lineno;
    }
    bool curin = e->cur != 0 && e->cur->lineno >= from && e->cur->lineno <= to;
    if ( curin ) {
        e->dirty = false;                       // (it goes away: its changes need not be entered)
    } else if ( !ed_commit( e ) ) {
        free( nums );
        return;
    }
    uint32_t topno = e->top != 0 ? e->top->lineno : 0;
    for ( size_t i = 0; i < cnt; ++i ) {
        char text[16];
        snprintf( text, sizeof(text), "%u", (unsigned) nums[i] );
        if ( !ed_enter( e, text ) ) {
            break;
        }
    }
    free( nums );
    e->top = ed_at( e, topno );
    e->cur = ed_at( e, from );
    ed_loadcur( e, 0 );
}

// deletes the selected text (a selection inside a line: the characters; over several lines: those lines)
static bool ed_seldelete( ed_t* e ) {
    uint32_t sno = 0, eno = 0;
    size_t spos = 0, epos = 0;
    if ( !ed_selrange( e, &sno, &spos, &eno, &epos ) ) {
        return false;
    }
    if ( sno == eno && e->selwhole ) {
        ed_selclear( e );
        ed_deletelines( e, sno, sno );
        return true;
    }
    if ( sno == eno ) {
        if ( !e->readonly && e->cur != 0 && e->cur->lineno == sno && epos <= e->ed.len ) {
            memmove( e->buf + spos, e->buf + epos, e->ed.len - epos + 1U );
            e->ed.len -= epos - spos;
            e->ed.pos = spos;
            e->dirty = true;
        }
        ed_selclear( e );
        return true;
    }
    uint32_t last = ed_sellast( e, eno, epos );
    ed_selclear( e );
    ed_deletelines( e, sno, last );
    return true;
}

static void ed_cut( ed_t* e ) {
    if ( e->ins ) {
        ed_endins( e );
    }
    size_t len = 0;
    char* text = ed_seltext( e, &len );
    if ( text == 0 ) {
        ed_setmsg( e, "Nothing to cut" );
        return;
    }
    ed_clipset( e, text, len );
    free( text );
    if ( !ed_seldelete( e ) ) {
        ed_deleteline( e );                     // nothing selected: the current line
    }
}

// a line that starts with a line number and has text behind it
static bool ed_numberedline( const char* s ) {
    size_t i = 0;
    while ( s[i] >= '0' && s[i] <= '9' && i < 7U ) {
        ++i;
    }
    if ( i == 0 || i > ED_NUMDIGITS || s[i] != ' ' ) {
        return false;
    }
    while ( s[i] == ' ' ) {
        ++i;
    }
    return s[i] != '\0';
}

static void ed_paste( ed_t* e, const char* clip, size_t cliplen ) {
    if ( e->toosmall || ( e->ins && e->numphase ) ) {
        return;
    }
    char* text = (char*) malloc( cliplen + 1U );
    if ( text == 0 ) {
        return;
    }
    memcpy( text, clip, cliplen );
    size_t len = clip_normalize( text, cliplen );
    text[len] = '\0';
    if ( len == 0 ) {
        free( text );
        return;
    }
    // the lines
    size_t nlines = 1;
    for ( size_t i = 0; i < len; ++i ) {
        if ( text[i] == '\n' ) {
            text[i] = '\0';
            ++nlines;
        }
    }
    char** lines = (char**) malloc( nlines * sizeof(char*) );
    if ( lines == 0 ) {
        free( text );
        return;
    }
    size_t nl = 0, nonempty = 0, numbered = 0;
    for ( char* p = text; p < text + len + 1U && nl < nlines; p += strlen( p ) + 1U ) {
        lines[nl++] = p;
        if ( p[0] != '\0' ) {
            ++nonempty;
            if ( ed_numberedline( p ) ) {
                ++numbered;
            }
        }
    }
    if ( nonempty > 0 && numbered == nonempty ) {
        // numbered lines: entered like typed lines
        if ( ed_endins( e ) ) {
            ed_selclear( e );
            if ( ed_commit( e ) ) {
                uint32_t topno = e->top != 0 ? e->top->lineno : 0, lastno = 0;
                unsigned nnew = 0, nrepl = 0, nsame = 0;
                for ( size_t i = 0; i < nl; ++i ) {
                    if ( lines[i][0] == '\0' ) {
                        continue;
                    }
                    uint32_t no = (uint32_t) strtoul( lines[i], 0, 10 );
                    char* before = ed_progbody( e, no );
                    if ( !ed_enter( e, lines[i] ) ) {
                        free( before );
                        break;
                    }
                    char* after = ed_progbody( e, no );
                    if ( before == 0 ) {
                        ++nnew;
                    } else if ( after != 0 && strcmp( before, after ) == 0 ) {
                        ++nsame;
                    } else {
                        ++nrepl;
                    }
                    free( before );
                    free( after );
                    lastno = no;
                    if ( lastno < topno ) {
                        topno = lastno;                 // (a pasted line above the view: the view starts there)
                    }
                }
                e->top = ed_at( e, topno );
                if ( lastno != 0 ) {
                    e->cur = ed_at( e, lastno );
                }
                bool rejected = e->msg[0] != '\0';            // (a refused line has told why: that message stays)
                ed_loadcur( e, 0 );
                if ( !rejected ) {                          // what the paste did: a paste of lines that are there already changes nothing, and says so
                    char m[ sizeof(e->msg) ];
                    int n = snprintf( m, sizeof(m), "Pasted %u line%s:", nnew + nrepl + nsame, nnew + nrepl + nsame == 1U ? "" : "s" );
                    const char* sep = " ";
                    if ( nnew > 0 ) { n += snprintf( m + n, sizeof(m) - (size_t) n, "%s%u new", sep, nnew ); sep = ", "; }
                    if ( nrepl > 0 ) { n += snprintf( m + n, sizeof(m) - (size_t) n, "%s%u replaced", sep, nrepl ); sep = ", "; }
                    if ( nsame > 0 ) { snprintf( m + n, sizeof(m) - (size_t) n, "%s%u unchanged", sep, nsame ); }
                    ed_setmsg( e, m );
                }
            }
        }
    } else if ( nonempty <= 1 ) {
        // one line of text: into the edited line at the cursor, replacing a selection inside the line
        const char* ins = "";
        for ( size_t i = 0; i < nl; ++i ) {
            if ( lines[i][0] != '\0' ) {
                ins = lines[i];
            }
        }
        uint32_t sno = 0, eno = 0;
        size_t spos = 0, epos = 0;
        if ( ed_selrange( e, &sno, &spos, &eno, &epos ) && sno == eno ) {
            ed_seldelete( e );
        } else {
            ed_selclear( e );
        }
        if ( !e->readonly && ( e->ins || e->cur != 0 ) ) {
            size_t n = strlen( ins );
            if ( n > e->ed.maxlen - e->ed.len ) {
                n = e->ed.maxlen - e->ed.len;
            }
            memmove( e->buf + e->ed.pos + n, e->buf + e->ed.pos, e->ed.len - e->ed.pos + 1U );
            memcpy( e->buf + e->ed.pos, ins, n );
            e->ed.pos += n; e->ed.len += n;
            if ( n > 0 ) {
                e->dirty = true;
            }
        }
    } else if ( ed_endins( e ) ) {
        // several lines without numbers: new lines behind the current one
        ed_selclear( e );
        if ( ed_commit( e ) ) {
            uint32_t topno = e->top != 0 ? e->top->lineno : 0;
            for ( size_t i = 0; i < nl; ++i ) {
                if ( lines[i][0] == '\0' ) {
                    continue;
                }
                uint32_t no = ed_autonumber( e );
                if ( no == 0 ) {
                    ed_setmsg( e, "No room for a new line here" );
                    break;
                }
                char* entry = (char*) malloc( strlen( lines[i] ) + 16U );
                if ( entry == 0 ) {
                    break;
                }
                sprintf( entry, "%u %s", (unsigned) no, lines[i] );
                bool ok = ed_enter( e, entry );
                free( entry );
                if ( !ok ) {
                    break;
                }
                e->cur = ed_at( e, no );
                if ( no < topno ) {
                    topno = no;
                }
            }
            e->top = ed_at( e, topno );
            ed_loadcur( e, 0 );
        }
    }
    free( lines );
    free( text );
}

static void ed_pasteclip( ed_t* e ) {
    size_t len = 0;
    char* text = e->drv->clip_get != 0 ? e->drv->clip_get( &len ) : clip_internal_get( &len );
    if ( text == 0 || len == 0 ) {
        free( text );
        ed_setmsg( e, "The clipboard is empty" );
        return;
    }
    ed_paste( e, text, len );
    free( text );
}

static void ed_selectall( ed_t* e ) {
    if ( e->pgm->first == 0 || !ed_endins( e ) ) {
        return;
    }
    uint32_t firstno = e->pgm->first->lineno;
    ed_moveto( e, e->pgm->last );
    if ( e->cur != 0 ) {
        e->ed.pos = e->ed.len;
        e->sel = true; e->selno = firstno; e->selpos = 0;
    }
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
    bool ctrl = ( k->mods & KSMOD_CTRL ) != 0, shift = ( k->mods & KSMOD_SHIFT ) != 0;
    if ( k->kind != SK_ESCAPE ) {
        e->escwarn = false;
    }
    // the clipboard and the selection
    if ( k->kind == SK_PASTE ) {
        ed_pasteclip( e );
        return;
    }
    if ( k->kind == SK_CHAR && ctrl ) {
        switch ( k->ch ) {
            case 'c': ed_copy( e ); return;
            case 'x': ed_cut( e ); return;
            case 'v': ed_pasteclip( e ); return;
            case 'a': ed_selectall( e ); return;
            default:  ed_selclear( e ); break;          // (Ctrl+Y and the like)
        }
    }
    if ( k->kind == SK_KEY ) {
        if ( k->key == KS_INSERT && ctrl ) { ed_copy( e ); return; }
        if ( k->key == KS_INSERT && shift ) { ed_pasteclip( e ); return; }
        if ( k->key == KS_DELETE && shift ) { ed_cut( e ); return; }
        if ( k->key == KS_DELETE && ed_seldelete( e ) ) { return; }
        switch ( k->key ) {
            case KS_LEFT: case KS_RIGHT: case KS_UP: case KS_DOWN: case KS_HOME: case KS_END: case KS_PGUP: case KS_PGDN:
                if ( shift && !e->ins ) {
                    ed_selbegin( e );
                    if ( k->key == KS_LEFT && e->ed.pos == 0 && e->cur != 0 && e->cur->prev != 0 ) {         // over the line end
                        ed_moveto( e, e->cur->prev );
                        e->ed.pos = e->ed.len;
                        return;
                    }
                    if ( k->key == KS_RIGHT && e->ed.pos == e->ed.len && e->cur != 0 && e->cur->next != 0 ) {
                        ed_moveto( e, e->cur->next );
                        e->ed.pos = 0;
                        return;
                    }
                } else {
                    ed_selclear( e );
                }
                break;
            default:
                break;
        }
    } else if ( k->kind == SK_BACKSPACE && ed_seldelete( e ) ) {
        return;
    } else if ( k->kind == SK_ESCAPE && e->sel && !e->ins && ed_selactive( e ) ) {
        ed_selclear( e );                              // (ESC drops a selection before it closes the editor)
        return;
    } else if ( k->kind == SK_RETURN || k->kind == SK_TAB ) {
        ed_selclear( e );
    } else if ( k->kind == SK_CHAR && !ctrl ) {
        uint32_t sno = 0, eno = 0;
        size_t spos = 0, epos = 0;
        if ( ed_selrange( e, &sno, &spos, &eno, &epos ) && sno == eno ) {
            ed_seldelete( e );                         // typing replaces a selection inside a line
        } else {
            ed_selclear( e );
        }
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
#define ED_DRAGSHAPE    CUR_HAND       // the pointer while the thumb is dragged (CUR_GRAB is the closed hand)

static void ed_enddrag( ed_t* e ) {
    if ( e->sbdrag ) {
        mouse_request_shape( e->sbsaveshape );
        e->sbdrag = false;
    }
    e->sbarrow = 0;
}

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

// where the cell (mx, my) of the whole screen is in the panes: the line and the text position under it; false below the last line
static bool ed_hit( ed_t* e, int mx, int my, line_t** pln, size_t* ppos ) {
    int row = my - ED_PANE_Y, y = 0;
    for ( line_t* ln = e->top; ln != 0 && y <= e->pane_h; ln = ln->next ) {
        int rows = !e->ins && ln == e->cur ? ed_editrows( e ) : ed_linerows( e, ln );
        if ( row < y + rows ) {
            int col = mx - ED_TEXT_X;           // (the number pane is column 0 of the line)
            col = col < 0 ? 0 : col >= e->text_w ? e->text_w - 1 : col;
            *pln = ln;
            *ppos = (size_t)( row - y ) * (size_t) e->text_w + (size_t) col;
            return true;
        }
        y += rows;
    }
    return false;
}

// a click in the number or the text pane: the cursor goes there (an insertion in progress is entered first, as for the cursor keys)
static void ed_click( ed_t* e, int mx, int my ) {
    if ( !ed_endins( e ) ) {
        return;
    }
    ed_scroll( e );
    line_t* ln = 0; size_t pos = 0;
    if ( e->cur == 0 || !ed_hit( e, mx, my, &ln, &pos ) ) {
        return;
    }
    if ( ln != e->cur ) {
        uint32_t topno = e->top != 0 ? e->top->lineno : 0;
        ed_moveto( e, ln );                     // (enters the edited line first; the nodes are looked up again by number)
        e->top = ed_at( e, topno );
    }
    e->ed.pos = pos < e->ed.len ? pos : e->ed.len;
}

static long ed_now_ms( void ) {
    struct timespec ts;
    clock_gettime( CLOCK_MONOTONIC, &ts );
    return (long) ts.tv_sec * 1000L + (long)( ts.tv_nsec / 1000000L );
}

static bool ed_wordchar( int c ) {
    return ( c >= '0' && c <= '9' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) || c == '_' || c >= 160;
}

// a click in the panes: the cursor goes there and a selection starts (a double click selects the word, a triple click the line); a double
// click in the number pane selects the line
static void ed_selpress( ed_t* e, int mx, int my ) {
    ed_selclear( e );
    ed_click( e, mx, my );
    if ( e->cur == 0 || e->ins ) {
        return;
    }
    long now = ed_now_ms();
    if ( now - e->lastclick_ms < 450L && e->lastclickx == mx && e->lastclicky == my ) {
        e->clicks = e->clicks >= 3 ? 1 : e->clicks + 1;
    } else {
        e->clicks = 1;
    }
    e->lastclick_ms = now; e->lastclickx = mx; e->lastclicky = my;
    if ( mx < ED_TEXT_X && e->clicks == 1 ) {                       // (a single click in the number pane only puts the cursor there)
        return;
    }
    if ( mx < ED_TEXT_X || e->clicks == 3 ) {                       // the line
        e->sel = true; e->selno = e->cur->lineno; e->selpos = 0;
        e->ed.pos = e->ed.len;
        e->selwhole = e->ed.len > 0;
        return;
    }
    if ( e->clicks == 2 ) {                                         // the word
        size_t a = e->ed.pos, z = e->ed.pos;
        if ( a == e->ed.len && a > 0 ) {
            --a; --z;
        }
        if ( a < e->ed.len && ed_wordchar( (unsigned char) e->buf[a] ) ) {
            while ( a > 0 && ed_wordchar( (unsigned char) e->buf[a-1U] ) ) {
                --a;
            }
            while ( z < e->ed.len && ed_wordchar( (unsigned char) e->buf[z] ) ) {
                ++z;
            }
            e->sel = true; e->selno = e->cur->lineno; e->selpos = a;
            e->ed.pos = z;
            return;
        }
    }
    ed_selbegin( e );
    e->seldrag = true;
}

// the button is held and the pointer is at the cell: the cursor follows (above or below the panes the view scrolls)
static void ed_seldrag( ed_t* e, int mx, int my ) {
    if ( my < ED_PANE_Y ) {
        ed_viewby( e, -1 );
        my = ED_PANE_Y;
    } else if ( my >= ED_PANE_Y + e->pane_h ) {
        ed_viewby( e, 1 );
        my = ED_PANE_Y + e->pane_h - 1;
    }
    if ( mx < ED_TEXT_X ) {
        mx = ED_TEXT_X;
    }
    ed_click( e, mx, my );
}

static void ed_mouse( ed_t* e, const scrkey_t* k ) {
    int sbx = ED_TEXT_X + e->text_w;
    bool inpanes = k->my >= ED_PANE_Y && k->my < ED_PANE_Y + e->pane_h;
    switch ( k->mouse ) {
        case SM_WHEEL:
            ed_viewby( e, -k->wheel * ED_WHEELLINES );
            break;
        case SM_PRESS:
            ed_enddrag( e );
            if ( k->button != 1 || !inpanes ) {
                break;
            }
            if ( k->mx < sbx ) {
                ed_selpress( e, k->mx, k->my );
                break;
            }
            if ( k->mx == sbx ) {
                int r = k->my - ED_PANE_Y;
                if ( r == 0 || r == e->pane_h - 1 ) {
                    e->sbarrow = r == 0 ? -1 : 1;           // (it repeats while the button is held: SM_REPEAT)
                    e->sbax = k->mx; e->sbay = k->my;
                    ed_viewby( e, e->sbarrow );
                } else {
                    e->sbsaveshape = mouse_get( MOUSEV_SHAPE );
                    mouse_request_shape( ED_DRAGSHAPE );
                    e->sbdrag = true;
                    ed_scrolltrack( e, r - 1 );
                }
            }
            break;
        case SM_REPEAT:
            if ( e->sbarrow != 0 && k->mx == e->sbax && k->my == e->sbay ) {
                ed_viewby( e, e->sbarrow );
            } else if ( e->seldrag ) {
                ed_seldrag( e, k->mx, k->my );          // (the button is held outside the panes: the view scrolls on)
            }
            break;
        case SM_DRAG:
            if ( e->seldrag ) {
                ed_seldrag( e, k->mx, k->my );
            } else if ( e->sbdrag ) {
                int t = k->my - ED_PANE_Y - 1;          // (above or below the bar: the first or the last place)
                ed_scrolltrack( e, t < 0 ? 0 : t >= e->sbtrack ? e->sbtrack - 1 : t );
            }
            break;
        case SM_RELEASE:
            if ( e->seldrag ) {
                e->seldrag = false;
                uint32_t a = 0, b = 0;
                size_t c = 0, d = 0;
                if ( !ed_selrange( e, &a, &c, &b, &d ) ) {
                    ed_selclear( e );                   // (a click without a drag selects nothing)
                }
            }
            ed_enddrag( e );
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
                if ( k.ch == 'c' && ( k.mods & KSMOD_CTRL ) != 0 ) {
                    k.kind = SK_CHAR;       // (Ctrl+C in the window: copy)
                } else {
                    k.kind = SK_ESCAPE;     // (the break key leaves the editor like ESC)
                }
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
    ed_enddrag( e );
    drv->end( state );
    ed_memcur = e->cur != 0 ? e->cur->lineno : LINENO_NONE;
    ed_memtop = e->top != 0 ? e->top->lineno : LINENO_NONE;
    drv->resetviewport();
    drv->select( oldscreen );
    drv->changed();
    free( e->buf );
    free( e );
}
