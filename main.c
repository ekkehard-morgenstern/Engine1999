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

#include "sdlmain.h"
#include "sdlscreen.h"
#include "sdlevent.h"
#include "sdlaudio.h"

#define BLU 2
#define WHT 140
#define GOL 123

#define TRN 0
#define RED 76
#define GRN 12

static const uint8_t tiletest1[TILE_WIDTH * TILE_HEIGHT] = {
    GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, BLU, WHT, WHT, WHT, WHT, WHT, WHT, BLU, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, BLU, WHT, WHT, WHT, WHT, BLU, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, BLU, WHT, WHT, BLU, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, BLU, WHT, WHT, BLU, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, BLU, WHT, WHT, WHT, WHT, BLU, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, BLU, WHT, WHT, WHT, WHT, WHT, WHT, BLU, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL
};

static const uint8_t sprtest1[SPRITE_WIDTH * SPRITE_HEIGHT] = {
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN,
    TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN,
    TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN,
    TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN,
    TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN,
    TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN
};

static const uint8_t sprtest1b[SPRITE_WIDTH * SPRITE_HEIGHT] = {
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN,
    TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN,
    TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN,
    TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN,
    TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN,
    TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN
};

static const uint8_t sprtest1c[SPRITE_WIDTH * SPRITE_HEIGHT] = {
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, GRN, GRN, GRN, TRN, TRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, GRN, GRN, GRN, TRN, TRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN,
    TRN, TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN,
    TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN,
    TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN,
    TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN,
    TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN
};

static const uint8_t sprtest1d[SPRITE_WIDTH * SPRITE_HEIGHT] = {
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN,
    TRN, TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN, TRN,
    TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN,
    TRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, TRN,
    TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN,
    TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN
};

static const uint8_t sprtest1e[SPRITE_WIDTH * SPRITE_HEIGHT] = {
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, GRN, TRN, GRN, GRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, GRN, TRN, GRN, GRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, GRN, TRN, GRN, GRN, GRN, GRN, TRN, GRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, GRN, TRN, GRN, GRN, GRN, GRN, TRN, GRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, GRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, GRN, TRN, TRN, TRN,
    TRN, TRN, TRN, GRN, TRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN, GRN, TRN, TRN, TRN,
    TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN,
    TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN,
    TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN,
    TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN
};

static const uint8_t sprtest1f[SPRITE_WIDTH * SPRITE_HEIGHT] = {
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, GRN, TRN, TRN, GRN, GRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, GRN, TRN, TRN, GRN, GRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN,
    TRN, TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN, TRN,
    TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN,
    TRN, TRN, GRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, GRN, TRN, TRN,
    TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN,
    TRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, GRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, RED, RED, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN,
    TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN, TRN
};


int main( int argc, char** argv ) {

    sdl_init();

    srand( time(0) );

    sdlscr_printf( 0, 0, 0, 1, "Hello world!\nThere!" );

    sdlscr_writetile( 0, tiletest1 );

    sdlscr_writesprite( 0, sprtest1  );
    sdlscr_writesprite( 1, sprtest1b );
    sdlscr_writesprite( 2, sprtest1c );
    sdlscr_writesprite( 3, sprtest1d );
    sdlscr_writesprite( 4, sprtest1e );
    sdlscr_writesprite( 5, sprtest1f );
    static const uint8_t animseq[6] = { 0, 1, 2, 3, 4, 5 };
    sdlscr_spriteanimdata( 0, &animseq[0], 6U );

#define NSPR 10
    int spx[NSPR], spy[NSPR];
    for ( int i=0; i < NSPR; ++i ) {
        spx[i] = rand() % SPRTGT_WIDTH;
        spy[i] = rand() % SPRTGT_HEIGHT;
        sdlscr_spriteanimcfg( i, 0, 6, 20 );
        sdlscr_spriteprio( i, 0 );
        sdlscr_movesprite( i, spx[i], spy[i] );
        sdlscr_showsprite( i, true );
    }

    sdlaud_playnote( 0, 12, 0.75f, 0.5f );
    sdlaud_stopchan( 0, 0 );
    sdlaud_playnote( 0, 14, 0.75f, 0.5f );
    sdlaud_stopchan( 0, 0 );
    sdlaud_playnote( 0, 16, 0.75f, 0.5f );
    sdlaud_stopchan( 0, 0 );
    sdlaud_playnote( 0, 12, 0.75f, 0.5f );
    sdlaud_playnote( 1, 14, 0.75f, 0.5f );
    sdlaud_playnote( 2, 16, 0.75f, 0.5f );
    sdlaud_stopchan( 0, 0 );
    sdlaud_stopchan( 1, 0 );
    sdlaud_stopchan( 2, 0 );

    // uint64_t last = sdlscr_getnsec(0);
    int scrollx = 0, scrolly = 0;
    while ( !sdlscr_term() ) {
        scrollx = ( scrollx + 1 ) % TILE_WIDTH;
        scrolly = ( scrolly + 1 ) % TILE_HEIGHT;
        sdlscr_scrolltiles( scrollx, scrolly );
        for ( int i=0; i < NSPR; ++i ) {
            if ( --spx[i] < -SPRITE_WIDTH  ) spx[i] = SPRTGT_WIDTH;
            if ( --spy[i] < -SPRITE_HEIGHT ) spy[i] = SPRTGT_HEIGHT;
            sdlscr_movesprite( i, spx[i], spy[i] );
        }
        int r = rand() % 100;
        if ( r < 10 ) {
            int c    = 4 + ( rand() & 3 );
            int from = 110 + ( rand() % 331 );
            int to   = 110 + ( rand() % 331 );
            if ( from > to ) {
                for ( int i=from; i >= to; --i ) {
                    sdlaud_playfreq( c, i, 0.5f, 0.0078125f );
                }
                sdlaud_stopchan( c, 0 );
            } else if ( from < to ) {
                for ( int i=from; i <= to; ++i ) {
                    sdlaud_playfreq( c, i, 0.5f, 0.0078125f );
                }
                sdlaud_stopchan( c, 0 );
            }
        }
        int ev = sdlev_wait( SDLEV_VBLANK | SDLEV_SCREENWORKERFINISHED );
        if ( ev & SDLEV_SCREENWORKERFINISHED ) break;
    }

    return EXIT_SUCCESS;
}
