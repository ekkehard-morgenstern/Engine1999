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

#include "fulledit.h"
#include "sdlscreen.h"
#include "sdlevent.h"
#include "textscreen.h"
#include "ansi.h"
#include "baspgm.h"

// -- layout and colors -----------------------------------------------------------------

#define ED_SCREEN       1               // text screen of the editor
#define ED_PANE_Y       1               // first row of the panes (row 0: title bar)
#define ED_PANE_H       23              // rows of the panes (row 24: help bar)
#define ED_NUM_W        7               // line number pane: 6 digits and a space
#define ED_TEXT_X       ED_NUM_W
#define ED_TEXT_W       72
#define ED_SB_X         ( ED_TEXT_X + ED_TEXT_W )
#define ED_BAR_Y        ( ED_PANE_Y + ED_PANE_H )
#define ED_BUFMAX       ( ED_TEXT_W * ( ED_PANE_H - 1 ) )   // longest line that can be edited (the rest of the pane is free)
#define ED_NUMDIGITS    6

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
    repl_t*     repl;
    program_t*  pgm;
    line_t*     top;                    // first line shown
    line_t*     cur;                    // the current line; while inserting: the line the new one follows (0 = none)
    bool        ins;                    // a new line is being inserted after cur
    bool        numphase;               // inserting: the line number is being typed
    char        num[ED_NUMDIGITS + 1];
    int         numlen;
    uint32_t    newno;                  // inserting, text phase: the number of the new line
    char        buf[ED_BUFMAX + 1];     // text of the current / new line, without its number
    sdlscr_edit_t ed;
    bool        dirty;                  // the text was changed
    bool        readonly;               // the line is too long to edit
    bool        escwarn;                // ESC failed once because of an error: the next one discards
    char        msg[96];                // error shown in the help bar until the next key
    bool        quit;
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

static int ed_rowsfor( size_t len ) {
    return len == 0 ? 1 : (int)( ( len + ED_TEXT_W - 1U ) / ED_TEXT_W );
}

static int ed_linerows( ed_t* e, line_t* ln ) {
    char* text = ed_bodytext( e, ln );
    int rows = text != 0 ? ed_rowsfor( strlen( text ) ) : 1;
    free( text );
    return rows;
}

// rows of the edited text (the cursor may sit behind the last character, on a row of its own)
static int ed_editrows( ed_t* e ) {
    return (int)( e->ed.len / ED_TEXT_W ) + 1;
}

// -- the edited text -------------------------------------------------------------------

static void ed_settext( ed_t* e, const char* text, size_t pos ) {
    size_t len = strlen( text );
    e->readonly = len > ED_BUFMAX;
    if ( e->readonly ) {
        len = 0;
        text = "";
    }
    memcpy( e->buf, text, len + 1U );
    e->ed.buf = e->buf; e->ed.len = len; e->ed.maxlen = ED_BUFMAX;
    e->ed.pos = pos < len ? pos : len;
    e->dirty = false;
}

// loads the text of the current line
static void ed_loadcur( ed_t* e, size_t pos ) {
    char* text = e->cur != 0 ? ed_bodytext( e, e->cur ) : 0;
    ed_settext( e, text != 0 ? text : "", pos );
    if ( e->cur != 0 && text != 0 && strlen( text ) > ED_BUFMAX ) {
        ed_setmsg( e, "Line too long to edit here" );
    }
    free( text );
}

// -- drawing ---------------------------------------------------------------------------

typedef struct _put_t {
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
        int row = p->pos / ED_TEXT_W, col = p->pos % ED_TEXT_W;
        int n = ED_TEXT_W - col;
        if ( (size_t) n > len ) {
            n = (int) len;
        }
        txtscr_write( p->y0 + row, col, bg, fg, text, n );
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
    while ( rows > ED_PANE_H && e->top != e->cur ) {
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
    txtscr_setviewport( 0, 0, TXTSCR_WIDTH, 1 );
    txtscr_setblank( C_BAR_BG, C_BAR_FG );
    txtscr_clear();
    txtscr_write( 0, 1, C_BAR_BG, C_BAR_FG, "Text Editor", -1 );
    if ( e->repl->filename != 0 ) {
        const char* name = e->repl->filename;
        int len = (int) strlen( name );
        if ( len > 40 ) {
            name += len - 40; len = 40;     // the end of a long name is the telling part
        }
        txtscr_write( 0, TXTSCR_WIDTH - 1 - len, C_BAR_BG, C_YELLOW, name, len );
    }
    txtscr_setviewport( 0, ED_BAR_Y, TXTSCR_WIDTH, 1 );
    if ( e->msg[0] != '\0' ) {
        txtscr_setblank( C_MSG_BG, C_MSG_FG );
        txtscr_clear();
        txtscr_write( 0, 1, C_MSG_BG, C_MSG_FG, e->msg, -1 );
        return;
    }
    txtscr_setblank( C_HELP_BG, C_HELP_FG );
    txtscr_clear();
    static const struct { const char* key; const char* what; } help[] = {
        { "ESC", "Close" }, { "^Y", "Del line" }, { "RET", "New line" }, { "PgUp/PgDn", "Scroll" },
        { "^Home/^End", "First/last" },
    };
    int x = 1;
    for ( size_t i=0; i < sizeof(help) / sizeof(help[0]); ++i ) {
        x += txtscr_write( 0, x, C_HELP_BG, C_HELPKEY_FG, help[i].key, -1 );
        x += txtscr_write( 0, x, C_HELP_BG, C_HELP_FG, " ", 1 );
        x += txtscr_write( 0, x, C_HELP_BG, C_HELP_FG, help[i].what, -1 );
        x += 2;
    }
}

static void ed_drawscrollbar( ed_t* e, int shown ) {
    txtscr_setviewport( ED_SB_X, ED_PANE_Y, 1, ED_PANE_H );
    txtscr_setblank( C_SBTRACK_BG, C_BLACK );
    txtscr_clear();
    txtscr_write( 0, 0, C_SBTHUMB_BG, C_WHITE, "^", 1 );
    txtscr_write( ED_PANE_H - 1, 0, C_SBTHUMB_BG, C_WHITE, "v", 1 );
    int track = ED_PANE_H - 2;
    int total = ed_countlines( e, 0 );
    int size = track, pos = 0;
    if ( total > shown && total > 0 ) {
        size = track * shown / total;
        if ( size < 1 ) {
            size = 1;
        }
        int idx = e->cur != 0 ? ed_countlines( e, e->cur ) + ( e->ins ? 1 : 0 ) : 0;
        pos = total > 1 ? ( track - size ) * idx / ( total - 1 ) : 0;
        if ( pos > track - size ) {
            pos = track - size;
        }
    }
    for ( int i=0; i < size; ++i ) {
        txtscr_write( 1 + pos + i, 0, C_SBTHUMB_BG, C_WHITE, " ", 1 );
    }
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
    ed_scroll( e );
    // layout: the entries from top on (the new line follows cur)
    slot_t slots[ED_PANE_H + 1];
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
        if ( y >= ED_PANE_H ) {
            break;
        }
        s.y = y; y += s.rows;
        slots[nslots++] = s;
        if ( nslots > ED_PANE_H ) {
            break;
        }
    }
    char numtext[ED_NUM_W + 1];
    // line number pane
    txtscr_setviewport( 0, ED_PANE_Y, ED_NUM_W, ED_PANE_H );
    txtscr_setblank( C_NUMEMPTY_BG, C_NUM_FG );
    txtscr_clear();
    for ( int i=0; i < nslots; ++i ) {
        const slot_t* s = &slots[i];
        int bg = s->edited ? C_CURNUM_BG : C_NUM_BG, fg = s->edited ? C_CURNUM_FG : C_NUM_FG;
        for ( int r=0; r < s->rows; ++r ) {
            if ( r == 0 ) {
                ed_numtext( e, s, numtext );
            } else {
                strcpy( numtext, "       " );
            }
            txtscr_write( s->y + r, 0, bg, fg, numtext, ED_NUM_W );
        }
    }
    // text pane
    txtscr_setviewport( ED_TEXT_X, ED_PANE_Y, ED_TEXT_W, ED_PANE_H );
    txtscr_setblank( C_EMPTY_BG, C_DIMGRAY );
    txtscr_clear();
    char blanks[ED_TEXT_W];
    memset( blanks, ' ', sizeof(blanks) );
    int curx = 0, cury = 0;
    for ( int i=0; i < nslots; ++i ) {
        const slot_t* s = &slots[i];
        int bg = s->edited ? C_CUR_BG : C_TEXT_BG;
        for ( int r=0; r < s->rows; ++r ) {
            txtscr_write( s->y + r, 0, bg, C_TEXT_FG, blanks, ED_TEXT_W );
        }
        put_t p = { s->y, 0, 0 };
        if ( s->edited ) {
            ed_put( &p, bg, C_TEXT_FG, e->buf, strlen( e->buf ) );
            curx = (int)( e->ed.pos % ED_TEXT_W ); cury = s->y + (int)( e->ed.pos / ED_TEXT_W );
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
                txtscr_setviewport( 0, ED_PANE_Y, ED_NUM_W, ED_PANE_H );
                txtscr_locate( e->numlen, slots[i].y );
            }
        }
    } else {
        txtscr_setviewport( ED_TEXT_X, ED_PANE_Y, ED_TEXT_W, ED_PANE_H );
        txtscr_locate( curx, cury );
    }
    sdlscr_textchanged();
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
static void ed_edit( ed_t* e, const sdlscr_inputmsg_t* msg ) {
    if ( e->readonly ) {
        return;
    }
    size_t oldlen = e->ed.len;
    sdlscr_editkey( &e->ed, msg );
    if ( e->ed.len != oldlen ) {
        e->dirty = true;
    }
}

static void ed_key( ed_t* e, const sdlscr_inputmsg_t* msg ) {
    if ( msg->inputtype == SDLSCR_INPUT_TEXT ) {
        if ( e->ins && e->numphase ) {
            char c = msg->text[0];
            if ( c >= '0' && c <= '9' && msg->text[1] == '\0' && e->numlen < ED_NUMDIGITS ) {
                e->num[e->numlen++] = c; e->num[e->numlen] = '\0';
            } else if ( c == ' ' ) {
                ed_endnumber( e );
            }
        } else if ( e->ins || e->cur != 0 ) {
            ed_edit( e, msg );
        }
        return;
    }
    if ( msg->inputtype != SDLSCR_INPUT_KEYPRESS ) {
        return;
    }
    SDL_Keycode key = msg->symbol;
    bool ctrl = ( msg->mods & KMOD_CTRL ) != 0;
    if ( key != SDLK_ESCAPE ) {
        e->escwarn = false;
    }
    if ( ctrl && key == SDLK_y ) {
        ed_deleteline( e );
        return;
    }
    switch ( key ) {
        case SDLK_ESCAPE:
            if ( e->ins && e->numphase && e->cur != 0 ) {
                ed_cancelins( e );
            } else {        // without any line there is nothing to go back to
                ed_leave( e );
            }
            return;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_TAB:
            if ( e->ins && e->numphase ) {
                ed_endnumber( e );
            } else if ( key == SDLK_TAB ) {
                // nothing: TAB only ends the line number
            } else if ( e->ins ) {
                ed_finishins( e, e->ed.pos == e->ed.len );
            } else if ( e->cur == 0 || e->ed.pos == e->ed.len ) {
                ed_beginins( e );
            } else {
                ed_move( e, 1 );
            }
            return;
        case SDLK_BACKSPACE:
            if ( e->ins && e->numphase ) {
                if ( e->numlen > 0 ) {
                    e->num[--e->numlen] = '\0';
                }
                return;
            }
            break;
        case SDLK_UP:
            if ( ed_endins( e ) ) ed_move( e, -1 );
            return;
        case SDLK_DOWN:
            if ( ed_endins( e ) ) ed_move( e, 1 );
            return;
        case SDLK_PAGEUP:
            if ( ed_endins( e ) ) ed_move( e, -( ED_PANE_H - 1 ) );
            return;
        case SDLK_PAGEDOWN:
            if ( ed_endins( e ) ) ed_move( e, ED_PANE_H - 1 );
            return;
        case SDLK_HOME:
            if ( ctrl ) {
                if ( ed_endins( e ) ) ed_moveto( e, e->pgm->first );
                return;
            }
            break;
        case SDLK_END:
            if ( ctrl ) {
                if ( ed_endins( e ) ) ed_moveto( e, e->pgm->last );
                return;
            }
            break;
    }
    if ( e->ins && e->numphase ) {
        return;
    }
    if ( e->ins || e->cur != 0 ) {
        ed_edit( e, msg );
    }
}

// without any line the editor is always inserting: the cursor waits in the line number pane
static void ed_idle( ed_t* e ) {
    if ( !e->quit && !e->ins && e->cur == 0 ) {
        ed_beginins( e );
    }
}

// -- the editor ------------------------------------------------------------------------

void fulledit_run( repl_t* repl, void (*vblank_handler)( void* ), void* usrdata ) {
    ed_t* e = (ed_t*) calloc( 1, sizeof(ed_t) );
    if ( e == 0 ) {
        return;
    }
    e->repl = repl; e->pgm = &repl->pgm;
    e->top = ed_memtop != LINENO_NONE ? ed_at( e, ed_memtop ) : e->pgm->first;
    e->cur = ed_memcur != LINENO_NONE ? ed_at( e, ed_memcur ) : e->pgm->first;
    if ( e->cur == 0 ) {
        e->top = 0;
    }
    ed_loadcur( e, 0 );
    ed_idle( e );
    int oldscreen = txtscr_selected();
    txtscr_select( ED_SCREEN );
    bool oldpoll = sdlscr_editmode_begin();
    ed_draw( e );
    while ( !e->quit ) {
        int ev = sdlev_wait( SDLEV_VBLANK | SDLEV_SCREENWORKERFINISHED | SDLEV_INPUT );
        if ( ev & SDLEV_SCREENWORKERFINISHED ) {
            break;
        }
        if ( ev & SDLEV_VBLANK ) {
            if ( vblank_handler ) {
                vblank_handler( usrdata );
            }
        }
        if ( !( ev & SDLEV_INPUT ) ) {
            continue;
        }
        sdlscr_inputmsg_t* msg;
        bool any = false;
        while ( !e->quit && ( msg = sdlscr_dequeueinputmsg() ) != 0 ) {
            if ( msg->inputtype == SDLSCR_INPUT_KEYPRESS ) {
                e->msg[0] = '\0';
            }
            ed_key( e, msg );
            ed_idle( e );
            sdlscr_deleteinputmsg( msg );
            any = true;
        }
        if ( any ) {
            ed_draw( e );
        }
    }
    sdlscr_editmode_end( oldpoll );
    ed_memcur = e->cur != 0 ? e->cur->lineno : LINENO_NONE;
    ed_memtop = e->top != 0 ? e->top->lineno : LINENO_NONE;
    txtscr_resetviewport();
    txtscr_select( oldscreen );
    sdlscr_textchanged();
    free( e );
}
