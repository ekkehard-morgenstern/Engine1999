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

#include "sprscreen.h"

typedef struct _sprctrl_t {
    unsigned long long
        anim_index       : 8,
        anim_pos         : 4,
        anim_speed_count : 5,
        anim_speed_reset : 5,
        screen_x         : 9,
        screen_y         : 9,
        priority         : 3,
        active           : 1,
        spr_spr_coll_ena : 1,
        spr_spr_coll_ind : 1,
        spr_til_coll_ena : 1,
        spr_til_coll_ind : 1,
                         : 0;
} sprctrl_t;

static sprctrl_t    sprctrl[MAX_SPRITES];
static uint8_t      spranim[MAX_SPRITES][MAX_SPRANIM];

void sprscr_init() {
    memset( &sprctrl[0], 0, sizeof(sprctrl_t) * MAX_SPRITES );
    memset( &spranim[0][0], 0, MAX_SPRITES * MAX_SPRANIM );
}
