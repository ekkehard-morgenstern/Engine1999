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

#ifndef SDLLAYER_H
#define SDLLAYER_H  1

#ifndef SDLTYPES_H
#include "sdltypes.h"
#endif

struct _sdllayer_t;

typedef void (*sdllaycb_t)( struct _sdllayer_t*, void* );

typedef struct _sdllayer_t {
    const char*  title;     // string constant
    SDL_Texture* texture;   // always screen sized
    uint8_t*     memory;    // always screen sized
    uint32_t     palette[256];  // ARGB8888
    bool         modified;  // modified flag
    bool         disabled;  // disabled flag (not rendered)
    uint8_t      priority;  // render priority
    sdllaycb_t   callback;  // memory renderer callback
    void*        userdata;  // memory renderer userdata
} sdllayer_t;

bool sdllay_init( sdllayer_t* lay, const char* title, uint8_t priority, SDL_Renderer* renderer );
void sdllay_cleanup( sdllayer_t* lay );

void sdllay_setcall( sdllayer_t* lay, sdllaycb_t callback, void* userdata );

bool sdllay_init_many( sdllayer_t* lay, size_t cnt, const char* const* titles, SDL_Renderer* renderer );
void sdllay_cleanup_many( sdllayer_t* lay, size_t cnt );

bool sdllay_needsredraw( const sdllayer_t* lay, size_t cnt );

bool sdllay_to_texture( sdllayer_t* lay );
bool sdllay_to_texture_many( sdllayer_t* lay, size_t cnt );

void sdllay_draw_texture( const sdllayer_t* lay, SDL_Renderer* renderer  );
void sdllay_draw_texture_many( const sdllayer_t* lay, size_t cnt, SDL_Renderer* renderer  );

void sdllay_set_modified( sdllayer_t* lay );
void sdllay_enable( sdllayer_t* lay );
void sdllay_disable( sdllayer_t* lay );
bool sdllay_enabled( const sdllayer_t* lay );
void sdllay_switch_priority( sdllayer_t* lay, size_t cnt, uint8_t index1, uint8_t index2 );

#endif
