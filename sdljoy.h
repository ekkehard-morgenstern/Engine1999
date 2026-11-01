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

    ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
        applies. Programs written in the built-in programming language, and
        executables made from them with BUILD, do not fall under this license
        as the exception describes (see the file LICENSE-EXCEPTION).

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#ifndef SDLJOY_H
#define SDLJOY_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#include <SDL2/SDL.h>

/*
The game controllers of SDL (SDL_GameController, with the plain joystick API as a fallback for pads SDL has no mapping for) as joy.h state. The screen thread's
event loop gives every SDL event to sdljoy_handle_event(); it takes the controller and joystick events: a pad that is plugged in gets a slot (joy_attach), the
buttons and axes become joy_event_*; a pad that is unplugged frees its slot. Pads that are plugged in before the Engine starts arrive as events, too.

The face buttons are the positions of the Xbox layout whatever the brand (SDL's hint SDL_GAMECONTROLLER_USE_BUTTON_LABELS is set to 0 by sdljoy_hints(), so a
Nintendo pad does not swap A and B): see docs/controller-support.md.
*/

void sdljoy_hints( void );                      // before SDL_Init()
void sdljoy_init( void );                       // after SDL_Init(): joy_init() and an empty device table
void sdljoy_shutdown( void );                   // closes the pads
bool sdljoy_handle_event( const SDL_Event* ev );   // true if it was a controller or joystick event (it has been dealt with)

#endif
