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


#include "scrdrv.h"
#include "textscreen.h"
#include "palette.h"
#include <strings.h>

// -- editing keys ----------------------------------------------------------------------------

bool scr_editkey( scredit_t* ed, const scrkey_t* key ) {
    char* work = ed->buf;
    if ( key->kind == SK_KEY ) {
        switch ( key->key ) {
            case KS_LEFT:
                if ( ed->pos > 0 ) { --ed->pos; return true; }
                break;
            case KS_RIGHT:
                if ( ed->pos < ed->len ) { ++ed->pos; return true; }
                break;
            case KS_HOME:
                if ( ed->pos != 0 ) { ed->pos = 0; return true; }
                break;
            case KS_END:
                if ( ed->pos != ed->len ) { ed->pos = ed->len; return true; }
                break;
            case KS_DELETE:
                if ( ed->pos < ed->len ) {
                    memmove( work + ed->pos, work + ed->pos + 1U, ed->len - ed->pos );
                    --ed->len;
                    return true;
                }
                break;
            default:
                break;
        }
    } else if ( key->kind == SK_BACKSPACE ) {
        if ( ed->pos > 0 ) {
            memmove( work + ed->pos - 1U, work + ed->pos, ed->len - ed->pos + 1U );
            --ed->pos; --ed->len;
            return true;
        }
    } else if ( key->kind == SK_CHAR && ( key->mods & KSMOD_CTRL ) == 0 ) {
        if ( SCR_PRINTABLE( key->ch ) && ed->len < ed->maxlen ) {
            memmove( work + ed->pos + 1U, work + ed->pos, ed->len - ed->pos + 1U );
            work[ ed->pos++ ] = (char) key->ch; ++ed->len;
            return true;
        }
    }
    return false;
}

// -- the one-line editor ---------------------------------------------------------------------

// draws the text of the editor from (startx, starty) in the viewport, wipes what a shorter text leaves behind, puts the
// cursor at pos and returns the row the text starts at now (it moves up when printing scrolled the screen)
static int scr_editdraw( const scrdrv_t* drv, const char* text, size_t len, size_t oldlen, size_t pos, int startx, int starty ) {
    int sx = 0, sy = 0;
    drv->getsize( &sx, &sy );
    drv->print( starty, startx, -1, -1, text );
    int ey = starty + (int)( ( (size_t) startx + len ) / (size_t) sx );
    if ( ey > sy - 1 ) {
        starty -= ey - ( sy - 1 );      // printing scrolled the screen
    }
    int cx = 0, cy = 0;
    drv->getcursor( &cx, &cy );
    // what a shorter text leaves behind (a burst of BACKSPACEs or DELETEs removes several characters before the screen is drawn): at least one blank
    size_t blanks = oldlen > len ? oldlen - len : 1U;
    static const char spaces[] = "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                ";
    while ( blanks > 0 && cy < sy ) {
        int room = sx - cx;
        int n = (size_t) room < blanks ? room : (int) blanks;
        if ( n > (int) sizeof(spaces) - 1 ) {
            n = (int) sizeof(spaces) - 1;
        }
        drv->write( cy, cx, -1, -1, spaces, n );
        blanks -= (size_t) n;
        cx = 0; ++cy;
    }
    drv->getcursor( &cx, &cy );
    int row = starty + (int)( ( (size_t) startx + pos ) / (size_t) sx );
    int col = (int)( ( (size_t) startx + pos ) % (size_t) sx );
    drv->locate( col, row );
    drv->changed();
    return starty;
}

int scr_lineedit( const scrdrv_t* drv, char* buf, size_t bufsz, void (*onresize)( void* ), void* usrdata ) {
    if ( buf == 0 || bufsz < 2U ) {
        return SCREDIT_TOOLONG;
    }
    int state = drv->begin();               // (first: a terminal looks where its cursor is now)
    int sx = 0, sy = 0;
    drv->getsize( &sx, &sy );
    int startx = 0, starty = 0;
    drv->getcursor( &startx, &starty );
    // the line must leave at least one row free, so that scrolling can't push its start off the screen
    size_t maxlen = (size_t) sx * (size_t)( sy - 1 ) - (size_t) startx;
    if ( maxlen > bufsz - 1U ) {
        maxlen = bufsz - 1U;
    }
    size_t len = strlen( buf );
    char* work = len <= maxlen ? (char*) malloc( bufsz ) : 0;
    if ( work == 0 ) {
        drv->end( state );
        return SCREDIT_TOOLONG;
    }
    memcpy( work, buf, len + 1U );
    scredit_t ed = { work, len, len, maxlen };
    int result = SCREDIT_CLOSED;
    starty = scr_editdraw( drv, work, ed.len, ed.len, ed.pos, startx, starty );
    int vx = 0, vy = 0, vw = 0, vh = 0;
    drv->getviewport( &vx, &vy, &vw, &vh );
    int absy = vy + starty;                // where the text starts on the screen (a resize moves it)
    size_t drawnlen = ed.len;              // the length of the text on the screen
    bool finished = false;
    while ( !finished ) {
        scrkey_t key;
        if ( !drv->nextkey( &key ) ) {
            break;
        }
        bool changed = false;
        for ( ;; ) {
            if ( key.kind == SK_RETURN ) {
                result = SCREDIT_ACCEPTED; finished = true;
            } else if ( key.kind == SK_EOF ) {
                // the end of the input: what was typed is a line (like the last line of a file without a line end); nothing typed: EOF
                result = ed.len > 0 ? SCREDIT_ACCEPTED : SCREDIT_EOF; finished = true;
            } else if ( key.kind == SK_ESCAPE ) {
                result = SCREDIT_CANCELLED; finished = true;
            } else if ( key.kind == SK_BREAK ) {
                result = SCREDIT_BREAK; finished = true;
            } else if ( key.kind == SK_CHAR && key.ch == 'd' && ( key.mods & KSMOD_CTRL ) != 0 && ed.len == 0 ) {
                result = SCREDIT_EOF; finished = true;
            } else if ( key.kind == SK_RESIZE ) {
                // the screen changed its size: the caller lays its viewport out again; the line (whose start moved up with the contents if
                // the cursor row would have fallen off) is drawn again at the same place in the new viewport, the rows below it are wiped
                int abs_start = drv->restart( absy, startx, ed.pos );
                if ( onresize != 0 ) {
                    onresize( usrdata );
                }
                int nx = 0, ny = 0, nw = 0, nh = 0;
                drv->getviewport( &nx, &ny, &nw, &nh );
                starty = abs_start - ny;
                if ( starty < 0 ) {
                    starty = 0;
                }
                if ( starty > nh - 1 ) {
                    starty = nh - 1;
                }
                if ( startx > nw - 1 ) {
                    startx = nw - 1;
                }
                static char spaces[TXTSCR_MAXW];
                memset( spaces, ' ', sizeof(spaces) );
                for ( int y = starty + 1; y < nh; ++y ) {
                    drv->write( y, 0, -1, -1, spaces, nw < (int) sizeof(spaces) ? nw : (int) sizeof(spaces) );
                }
                changed = true;
            } else if ( scr_editkey( &ed, &key ) ) {
                changed = true;
            }
            if ( finished || !drv->pending() || !drv->nextkey( &key ) ) {
                break;
            }
        }
        if ( changed || finished ) {
            starty = scr_editdraw( drv, work, ed.len, drawnlen, finished ? ed.len : ed.pos, startx, starty );
            drawnlen = ed.len;
            drv->getviewport( &vx, &vy, &vw, &vh );
            absy = vy + starty;
        }
    }
    if ( result == SCREDIT_ACCEPTED ) {
        memcpy( buf, work, ed.len + 1U );
    }
    if ( result != SCREDIT_CLOSED ) {
        drv->print( -1, -1, -1, -1, "\n" );      // the cursor is at the end of the text now
        drv->changed();
    }
    drv->end( state );
    free( work );
    return result;
}

// -- the screen editor -----------------------------------------------------------------------

/*
All positions are cell indices i = y * w + x of the editor area (the viewport of the selected screen, whose top left corner is the
screen's). A row link exists between the last cell of a row and the first cell of the next one only if the last cell has WRAP.
*/

typedef struct _se_t {
    int     w, h, n;            // the editor area
    int     cx, cy;             // the cursor
    bool    ins;                // typing mode
} se_t;

typedef struct _secell_t {
    uint8_t ch, fg, bg;
    bool    empty;
} secell_t;

static bool se_empty( const se_t* e, int i ) { return txtscr_isempty( i / e->w, i % e->w ); }

static secell_t se_get( const se_t* e, int i ) {
    secell_t c;
    textcell_t t = txtscr_getcell( i / e->w, i % e->w );
    c.ch = TXTSCR_CELL_CHR( t ); c.fg = TXTSCR_CELL_FG( t ); c.bg = TXTSCR_CELL_BG( t );
    c.empty = se_empty( e, i );
    return c;
}

static void se_put( const se_t* e, int i, const secell_t* c ) {
    int y = i / e->w, x = i % e->w;
    if ( c->empty ) {
        txtscr_setempty( y, x );
    } else {
        char ch = (char) c->ch;
        txtscr_write( y, x, c->bg, c->fg, &ch, 1 );
    }
}

static bool se_prevlinked( const se_t* e, int i ) {
    return i > 0 && ( i % e->w != 0 || txtscr_iswrapped( i / e->w - 1, e->w - 1 ) );
}

static bool se_nextlinked( const se_t* e, int i ) {
    return i + 1 < e->n && ( ( i + 1 ) % e->w != 0 || txtscr_iswrapped( i / e->w, e->w - 1 ) );
}

// the logical line of the cursor cell: its first and last cell
static bool se_line( const se_t* e, int cur, int* start, int* end ) {
    int a = -1;
    if ( !se_empty( e, cur ) ) {
        a = cur;
    } else if ( se_prevlinked( e, cur ) && !se_empty( e, cur - 1 ) ) {
        a = cur - 1;
    }
    if ( a < 0 ) {
        return false;
    }
    int s = a, t = a;
    while ( se_prevlinked( e, s ) && !se_empty( e, s - 1 ) ) --s;
    while ( se_nextlinked( e, t ) && !se_empty( e, t + 1 ) ) ++t;
    *start = s; *end = t;
    return true;
}

static void se_area( se_t* e, int cx, int cy ) {
    int vx, vy;
    txtscr_getviewport( &vx, &vy, &e->w, &e->h );
    (void) vx; (void) vy;
    e->n = e->w * e->h;
    e->cx = cx; e->cy = cy;
}

bool scr_screenline_extent( int x, int y, int* startx, int* starty, int* endx, int* endy ) {
    se_t e;
    se_area( &e, x, y );
    int s, t;
    if ( x < 0 || y < 0 || x >= e.w || y >= e.h || !se_line( &e, y * e.w + x, &s, &t ) ) {
        return false;
    }
    if ( startx ) *startx = s % e.w;
    if ( starty ) *starty = s / e.w;
    if ( endx ) *endx = t % e.w;
    if ( endy ) *endy = t / e.w;
    return true;
}

static size_t se_text( const se_t* e, int s, int t, char* out, size_t outsz ) {
    size_t n = 0;
    for ( int i = s; i <= t && n + 1U < outsz; ++i ) {
        out[n++] = (char) TXTSCR_CELL_CHR( txtscr_getcell( i / e->w, i % e->w ) );
    }
    if ( outsz > 0 ) {
        out[n] = '\0';
    }
    return n;
}

size_t scr_screenline_text( int x, int y, char* out, size_t outsz ) {
    se_t e;
    se_area( &e, x, y );
    int s, t;
    if ( outsz > 0 ) out[0] = '\0';
    if ( x < 0 || y < 0 || x >= e.w || y >= e.h || !se_line( &e, y * e.w + x, &s, &t ) ) {
        return 0;
    }
    return se_text( &e, s, t, out, outsz );
}

// the edit unit at the cursor (docs/screen-editor-proposal.md 5.3): 1 = the logical line [us, ue] with the cursor at pos in us..ue+1,
// 2 = the rest of a screen row that is not blank (cursor to the last column), 0 = nothing
static int se_unit( const se_t* e, int* us, int* ue, int* pos ) {
    int cur = e->cy * e->w + e->cx;
    if ( se_line( e, cur, us, ue ) ) {
        *pos = cur;
        return 1;
    }
    for ( int x = 0; x < e->w; ++x ) {
        if ( !se_empty( e, e->cy * e->w + x ) ) {
            *us = cur; *ue = e->cy * e->w + e->w - 1; *pos = cur;
            return 2;
        }
    }
    return 0;
}

// puts newn cells at 'from' where oldn cells were; the rest of the old range becomes empty; in a logical line (links) the row ends inside
// the new text get WRAP
static void se_store( const se_t* e, int from, int oldn, const secell_t* cells, int newn, bool links ) {
    for ( int k = 0; k < newn; ++k ) {
        se_put( e, from + k, &cells[k] );
    }
    for ( int k = newn; k < oldn; ++k ) {
        txtscr_setempty( ( from + k ) / e->w, ( from + k ) % e->w );
    }
    if ( links ) {
        int total = oldn > newn ? oldn : newn;
        for ( int k = 0; k < total; ++k ) {
            int i = from + k;
            if ( i % e->w == e->w - 1 ) {
                txtscr_setwrap( i / e->w, e->w - 1, k < newn - 1 );
            }
        }
    }
}

// opens a blank screen row at 'row': the rows from there down move down one row (the bottom row of the area is lost)
static void se_openrow( const se_t* e, int row ) {
    if ( row < e->h - 1 ) {
        txtscr_copy( 0, row, e->w - 1, e->h - 2, 0, row + 1 );
    }
    for ( int x = 0; x < e->w; ++x ) {
        txtscr_setempty( row, x );
    }
}

static bool se_rowblank( const se_t* e, int row ) {
    for ( int x = 0; x < e->w; ++x ) {
        if ( !se_empty( e, row * e->w + x ) ) {
            return false;
        }
    }
    return true;
}

// -- cursor movement

static void se_left( se_t* e ) {
    if ( e->cx > 0 ) {
        --e->cx;
    } else if ( e->cy > 0 ) {
        --e->cy; e->cx = e->w - 1;
    } else {
        txtscr_scrolldown();
        e->cx = e->w - 1;
    }
}

static void se_right( se_t* e ) {
    if ( e->cx < e->w - 1 ) {
        ++e->cx;
    } else if ( e->cy < e->h - 1 ) {
        e->cx = 0; ++e->cy;
    } else {
        txtscr_scrollup();
        e->cx = 0;
    }
}

static void se_up( se_t* e ) {
    if ( e->cy > 0 ) --e->cy; else txtscr_scrolldown();
}

static void se_down( se_t* e ) {
    if ( e->cy < e->h - 1 ) ++e->cy; else txtscr_scrollup();
}

// -- editing

// a cell with the current colors
static secell_t se_newcell( int ch ) {
    secell_t c;
    int bg, fg;
    txtscr_getcolors( &bg, &fg );
    c.ch = (uint8_t) ch; c.fg = (uint8_t) fg; c.bg = (uint8_t) bg; c.empty = false;
    return c;
}

// inserts a cell at the cursor and shifts the rest of the edit unit right; with ch >= 0 the cell is that character (typing) and the cursor
// moves behind it. Returns false if nothing could be done.
static bool se_insert( se_t* e, int ch ) {
    int us, ue, pos;
    int kind = se_unit( e, &us, &ue, &pos );
    secell_t nc = se_newcell( ch >= 0 ? ch : 32 );
    if ( kind == 0 ) {
        if ( ch < 0 ) {
            return false;
        }
        se_put( e, e->cy * e->w + e->cx, &nc );
        se_right( e );
        return true;
    }
    int len = ue - us + 1;
    secell_t* cells = (secell_t*) malloc( ( (size_t) len + 1U ) * sizeof(secell_t) );
    if ( cells == 0 ) {
        return false;
    }
    int k = pos - us;
    if ( kind == 1 ) {
        // the line grows by one cell into the empty cell behind it (a screen scrolls up if that is behind the last cell)
        int slot = ue + 1;
        if ( slot < e->n && !se_empty( e, slot ) ) {
            se_openrow( e, slot / e->w );       // the next row begins with another line: a blank screen line makes room
        }
        if ( slot >= e->n ) {
            if ( us < e->w ) {
                free( cells );
                return false;   // the line fills the whole screen
            }
            txtscr_scrollup();
            us -= e->w; ue -= e->w; pos -= e->w; --e->cy;
        }
        for ( int i = 0; i < len; ++i ) cells[i >= k ? i + 1 : i] = se_get( e, us + i );
        cells[k] = nc;
        se_store( e, us, len, cells, len + 1, true );
    } else {
        // the rest of a screen row: what is pushed over the last column is lost
        for ( int i = 0; i < len; ++i ) cells[i] = se_get( e, us + i );
        memmove( cells + 1, cells, (size_t)( len - 1 ) * sizeof(secell_t) );
        cells[0] = nc;
        se_store( e, us, len, cells, len, false );
    }
    free( cells );
    if ( ch >= 0 ) {
        se_right( e );
    }
    return true;
}

// removes the cell at index 'at' of the edit unit [us, ue] and pulls the rest left
static bool se_remove( se_t* e, int kind, int us, int ue, int at ) {
    int len = ue - us + 1;
    secell_t* cells = (secell_t*) malloc( (size_t) len * sizeof(secell_t) );
    if ( cells == 0 ) {
        return false;
    }
    for ( int i = 0; i < len; ++i ) cells[i] = se_get( e, us + i );
    memmove( cells + ( at - us ), cells + ( at - us ) + 1, (size_t)( ue - at ) * sizeof(secell_t) );
    if ( kind == 2 ) {
        memset( &cells[len - 1], 0, sizeof(secell_t) );
        cells[len - 1].empty = true;
        se_store( e, us, len, cells, len, false );
    } else {
        se_store( e, us, len, cells, len - 1, true );
    }
    free( cells );
    return true;
}

static bool se_delete( se_t* e ) {
    int us, ue, pos;
    int kind = se_unit( e, &us, &ue, &pos );
    if ( kind == 0 || pos > ue ) {
        return false;
    }
    return se_remove( e, kind, us, ue, pos );
}

static bool se_backspace( se_t* e ) {
    int us, ue, pos;
    int kind = se_unit( e, &us, &ue, &pos );
    if ( kind == 1 && pos > us ) {
        if ( !se_remove( e, 1, us, ue, pos - 1 ) ) {
            return false;
        }
        --pos;
        e->cy = pos / e->w; e->cx = pos % e->w;
        return true;
    }
    if ( kind == 2 && e->cx > 0 ) {
        // the gap left of the cursor is closed: the rest of the row moves left
        if ( !se_remove( e, 2, us - 1, ue, us - 1 ) ) {
            return false;
        }
        --e->cx;
        return true;
    }
    se_left( e );       // at the start of the unit: nothing is deleted, the line is abandoned
    return true;
}

// a typed character
static bool se_type( se_t* e, int ch ) {
    if ( e->ins ) {
        return se_insert( e, ch );
    }
    secell_t nc = se_newcell( ch );
    se_put( e, e->cy * e->w + e->cx, &nc );
    if ( e->cx == e->w - 1 ) {
        txtscr_setwrap( e->cy, e->cx, true );
    }
    se_right( e );
    return true;
}

static void se_home( se_t* e, bool top ) {
    int us, ue, pos;
    if ( top ) {
        e->cx = e->cy = 0;
    } else if ( se_unit( e, &us, &ue, &pos ) == 1 ) {
        e->cy = us / e->w; e->cx = us % e->w;
    } else {
        e->cx = 0;
    }
}

static void se_end( se_t* e, bool bottom ) {
    int us, ue, pos;
    if ( bottom ) {
        e->cx = 0; e->cy = e->h - 1;
    } else if ( se_unit( e, &us, &ue, &pos ) == 1 ) {
        if ( ue % e->w != e->w - 1 ) {
            e->cy = ( ue + 1 ) / e->w; e->cx = ( ue + 1 ) % e->w;
        } else {
            // the line ends on the last cell of a row: it goes on in a new screen line below (the text below moves down), the cursor at its start
            int row = ue / e->w;
            if ( row == e->h - 1 ) {
                txtscr_scrollup();
                --row;
            } else if ( !se_rowblank( e, row + 1 ) ) {
                se_openrow( e, row + 1 );
            }
            txtscr_setwrap( row, e->w - 1, true );
            e->cy = row + 1; e->cx = 0;
        }
    } else {
        e->cx = e->w - 1;
    }
}

// the cursor to the start of row 'row' of the area (scrolling up if that is below it)
static void se_gotorow( se_t* e, int row ) {
    while ( row >= e->h ) {
        txtscr_scrollup();
        --row;
    }
    e->cy = row; e->cx = 0;
}

// -- status line and highlight

static int se_color( const char* name, int dflt ) {
    for ( int i = 0; i < 256; ++i ) {
        if ( strcasecmp( pal_title( i ), name ) == 0 ) {
            return i;
        }
    }
    return dflt;
}

static void se_overlays( const se_t* e, bool ins ) {
    int us, ue, pos;
    int kind = se_unit( e, &us, &ue, &pos );
    char lineno[16] = "-";
    if ( kind == 1 ) {
        int i = us, n = 0;
        while ( i <= ue && TXTSCR_CELL_CHR( txtscr_getcell( i / e->w, i % e->w ) ) == ' ' ) ++i;
        while ( i <= ue && n < 9 ) {
            int c = TXTSCR_CELL_CHR( txtscr_getcell( i / e->w, i % e->w ) );
            if ( c < '0' || c > '9' ) break;
            lineno[n++] = (char) c; ++i;
        }
        if ( n > 0 ) lineno[n] = '\0';
    }
    char pos_part[64], text[TXTSCR_MAXW + 80];
    snprintf( pos_part, sizeof(pos_part), "Row %d  Col %d  Line %s", e->cy + 1, e->cx + 1, lineno );
    snprintf( text, sizeof(text), "%s  F12=%s     %s", ins ? "INS" : "OVR", ins ? "OVR" : "INS", pos_part );
    if ( (int) strlen( text ) > e->w ) {
        snprintf( text, sizeof(text), "%s", pos_part );     // narrow: the position is what stays
    }
    // white on translucent dark red (the backdrop and the layers show through), unless the text colors are those (then yellow on it)
    int bg, fg;
    txtscr_getcolors( &bg, &fg );
    int sp = se_color( "TransparentDarkRed", 190 ), sf = se_color( "White", 140 );
    if ( bg == sp && fg == sf ) {
        sf = se_color( "Yellow", 252 );
    }
    txtscr_setstatus( text, sp, sf );
    if ( kind == 1 ) {
        txtscr_sethighlight( us / e->w, ue / e->w );
    } else if ( kind == 2 ) {
        txtscr_sethighlight( e->cy, e->cy );
    } else {
        txtscr_sethighlight( -1, -1 );
    }
}

static void se_setmap( const scrdrv_t* drv ) {
    uint32_t pal[256], backdrop = PAL_DEFAULT_BACKDROP;
    uint8_t map[256];
    if ( drv->palette == 0 ) {
        txtscr_sethighlightmap( 0 );
        return;
    }
    drv->palette( pal, &backdrop );
    pal_lighter_map( pal, 30, backdrop, map );
    txtscr_sethighlightmap( map );
}

// -- the editor

static void se_enter( const scrdrv_t* drv, se_t* e ) {
    int sw = 0, sh = 0, cx = 0, cy = 0;
    drv->screensize( &sw, &sh );
    txtscr_getabscursor( &cx, &cy );        // (before: setting a viewport moves the cursor to its corner)
    drv->resetviewport();
    if ( cy >= sh - 1 ) {
        txtscr_scrollup();          // the cursor row moves up: the bottom row is the status line
        cy = sh - 2;
    }
    for ( int x = 0; x < sw; ++x ) {
        txtscr_setempty( sh - 1, x );
    }
    drv->setviewport( 0, 0, sw, sh - 1 );
    drv->reserve( 1 );
    se_area( e, cx < sw ? cx : sw - 1, cy );
    se_setmap( drv );
}

static void se_leave( const scrdrv_t* drv, const se_t* e ) {
    txtscr_setstatus( 0, 0, 0 );
    txtscr_sethighlight( -1, -1 );
    drv->resetviewport();
    drv->reserve( 0 );
    drv->locate( e->cx, e->cy );
    drv->changed();
}

int scr_screenedit( const scrdrv_t* drv, char* buf, size_t bufsz, bool* insert, void (*onresize)( void* ), void* usrdata ) {
    if ( buf == 0 || bufsz < 2U ) {
        return SCREDIT_TOOLONG;
    }
    buf[0] = '\0';
    int state = drv->begin();
    int sw = 0, sh = 0;
    drv->screensize( &sw, &sh );
    if ( sh < 3 ) {
        drv->end( state );
        return SCREDIT_CANCELLED;       // too small for a status line: the caller uses the one-line editor
    }
    bool localins = false;
    bool* ins = insert != 0 ? insert : &localins;
    se_t e;
    se_enter( drv, &e );
    e.ins = *ins;
    int result = SCREDIT_CLOSED;
    bool finished = false;
    se_overlays( &e, e.ins );
    drv->locate( e.cx, e.cy );
    drv->changed();
    while ( !finished ) {
        scrkey_t key;
        if ( !drv->nextkey( &key ) ) {
            break;
        }
        for ( ;; ) {
            if ( key.kind == SK_RETURN ) {
                int us, ue, pos;
                int cur = e.cy * e.w + e.cx;
                if ( se_line( &e, cur, &us, &ue ) ) {
                    size_t n = se_text( &e, us, ue, buf, bufsz );
                    while ( n > 0 && buf[n - 1] == ' ' ) buf[--n] = '\0';
                    if ( ue % e.w == e.w - 1 ) {
                        txtscr_setwrap( ue / e.w, e.w - 1, false );
                    }
                    se_gotorow( &e, ue / e.w + 1 );
                    result = SCREDIT_ACCEPTED; finished = true;
                } else {
                    (void) pos;
                    se_gotorow( &e, e.cy + 1 );     // no line: just the next row
                }
            } else if ( key.kind == SK_BREAK ) {
                // nothing runs: Break has no effect, whatever
            } else if ( key.kind == SK_EOF ) {
                // the end of the input: the only way out besides a closed screen, without any visible effect (an open question, see the roadmap)
                result = SCREDIT_EOF; finished = true;
            } else if ( key.kind == SK_RESIZE ) {
                // the screens have the new size (the cursor row kept above the last row, see reserve()); the viewport is the whole screen again
                int cx = e.cx, cy = e.cy - txtscr_resizeshift();      // (the screen's own cursor lags behind within a burst of keys)
                if ( cy < 0 ) cy = 0;
                drv->screensize( &sw, &sh );
                if ( sh < 3 ) {
                    se_leave( drv, &e );
                    drv->end( state );
                    return SCREDIT_CANCELLED;
                }
                drv->resetviewport();
                for ( int x = 0; x < sw; ++x ) {
                    txtscr_setempty( sh - 1, x );
                }
                drv->setviewport( 0, 0, sw, sh - 1 );
                drv->reserve( 1 );
                se_area( &e, cx < sw ? cx : sw - 1, cy < sh - 1 ? cy : sh - 2 );
                se_setmap( drv );
                if ( onresize != 0 ) {
                    onresize( usrdata );
                }
            } else if ( key.kind == SK_CHAR && ( key.mods & KSMOD_CTRL ) == 0 ) {
                if ( SCR_PRINTABLE( key.ch ) ) {
                    se_type( &e, key.ch );
                }
            } else if ( key.kind == SK_BACKSPACE ) {
                se_backspace( &e );
            } else if ( key.kind == SK_KEY ) {
                bool ctrl = ( key.mods & KSMOD_CTRL ) != 0;
                switch ( key.key ) {
                    case KS_LEFT:   se_left( &e ); break;
                    case KS_RIGHT:  se_right( &e ); break;
                    case KS_UP:     se_up( &e ); break;
                    case KS_DOWN:   se_down( &e ); break;
                    case KS_HOME:   se_home( &e, ctrl ); break;
                    case KS_END:    se_end( &e, ctrl ); break;
                    case KS_DELETE: se_delete( &e ); break;
                    case KS_INSERT:
                        if ( ( key.mods & KSMOD_SHIFT ) != 0 ) {
                            e.ins = !e.ins;
                        } else {
                            se_insert( &e, -1 );
                        }
                        break;
                    case KS_F12:    e.ins = !e.ins; break;
                    default:        break;
                }
            }
            // (ESC, TAB, Ctrl+letters, the other keys: ignored)
            if ( finished || !drv->pending() ) {
                break;
            }
            drv->locate( e.cx, e.cy );      // (a resize in the burst looks at the screen's cursor)
            if ( !drv->nextkey( &key ) ) {
                break;
            }
        }
        if ( !finished ) {
            se_overlays( &e, e.ins );
            drv->locate( e.cx, e.cy );
            drv->changed();
        }
    }
    *ins = e.ins;
    if ( result != SCREDIT_CLOSED ) {
        se_leave( drv, &e );
    }
    drv->end( state );
    return result;
}
