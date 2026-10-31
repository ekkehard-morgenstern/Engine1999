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
#include "textscreen.h"      // (only the TXTSCR_* constants)

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
    if ( len > maxlen ) {
        return SCREDIT_TOOLONG;
    }
    char* work = (char*) malloc( bufsz );
    if ( work == 0 ) {
        return SCREDIT_TOOLONG;
    }
    memcpy( work, buf, len + 1U );
    scredit_t ed = { work, len, len, maxlen };
    int result = SCREDIT_CLOSED;
    int state = drv->begin();
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
            } else if ( key.kind == SK_ESCAPE ) {
                result = SCREDIT_CANCELLED; finished = true;
            } else if ( key.kind == SK_RESIZE ) {
                // the screen changed its size: the caller lays its viewport out again; the line (whose start moved up with the contents if
                // the cursor row would have fallen off) is drawn again at the same place in the new viewport, the rows below it are wiped
                int abs_start = absy - drv->resizeshift();
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
