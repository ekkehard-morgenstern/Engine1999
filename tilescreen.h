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

#ifndef TILESCREEN_H
#define TILESCREEN_H    1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#define TILEMAP_WIDTH   256
#define TILEMAP_HEIGHT  384
#define TILEMAP_CELLSX  16
#define TILEMAP_CELLSY  16

#define TILE_WIDTH  16
#define TILE_HEIGHT 24

#define TILESCR_WIDTH   41
#define TILESCR_HEIGHT  13

#define TILETGT_WIDTH   640
#define TILETGT_HEIGHT  300

void tilescr_init( void );

void tilescr_render( uint8_t* target );

void tilescr_writetile( int tileno, const uint8_t data[TILE_WIDTH * TILE_HEIGHT]);
void tilescr_scroll( int sx, int sy );

#endif