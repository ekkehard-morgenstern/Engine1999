#pragma once

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

#ifndef FULLEDIT_H
#define FULLEDIT_H      1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#ifndef BASREPL_H
#include "basrepl.h"
#endif

/*
Full-screen program editor (EDIT without a target), host side: it needs the SDL screen and the REPL. It runs on text
screen 1 (BASIC keeps screen 0, which is left untouched) and is built from viewports: a title bar at the top, a line
number pane, the text pane, a scroll bar (informational only, there is no mouse support yet) and a key help bar at the
bottom. The program lines are shown with the syntax highlighting of repl->hlstyle; the current line is edited in
place (plain text) with the keys of the one-line editor. Changes are made by entering lines through repl_enter(), like
typed lines, so the editor must not be called from inside a REPL callback (see repl_t.fulledit).

Keys: UP/DOWN/PGUP/PGDN/CTRL+HOME/CTRL+END change the line (an edited line is entered first), LEFT/RIGHT/HOME/END/
BACKSPACE/DELETE/characters edit it, RETURN at the end of the line inserts a new line (dots in the line number pane:
type the number, SPACE/TAB/RETURN go on to the text, no number = chosen automatically, ESC cancels), RETURN elsewhere
enters the line and goes to the next one, CTRL+Y deletes the line, ESC leaves the editor. Emptying the text of an
existing line deletes it, as in BASIC. The position is remembered for the next call.
*/

// returns when the user leaves the editor (or the screen is closed)
void fulledit_run( repl_t* repl, void (*vblank_handler)( void* ), void* usrdata );

#endif
