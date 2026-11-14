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

#include <strings.h>
#include "sdllayer.h"
#include "sdlscreen.h"
#include "palette.h"

bool sdllay_init( sdllayer_t* lay, const char* title, uint8_t priority, SDL_Renderer* renderer ) {
    lay->title = title;
    lay->texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SDL_SCREENWIDTH,
        SDL_SCREENHEIGHT
    );
    if ( lay->texture == 0 ) {
        fprintf( stderr, "sdllay_init(): failed to create layer: %s\n",
            SDL_GetError() );
        return false;
    }
    SDL_SetTextureBlendMode( lay->texture, SDL_BLENDMODE_BLEND );
    lay->memory = (uint8_t*) malloc( SDL_SCREENWIDTH * SDL_SCREENHEIGHT );
    if ( lay->memory == 0 ) {
        perror( "malloc(3)" );
        SDL_DestroyTexture( lay->texture ); lay->texture = 0;
        fprintf( stderr, "sdllay_init(): failed to create texture\n" );
        return false;
    }
    memset( lay->memory, 0, SDL_SCREENWIDTH * SDL_SCREENHEIGHT );
    for ( int i=0; i < 256; ++i ) {
        lay->palette[i] = pal_argb( i );
    }
    // NOTE that the color names aren't currently used, which might change in a future version.
    __atomic_store_n( &lay->modified, true, __ATOMIC_RELEASE );
    lay->disabled = false;
    lay->priority = priority;
    lay->argb = 0;
    lay->callback = 0;
    lay->userdata = 0;
    return true;
}

bool sdllay_init32( sdllayer_t* lay, const char* title, uint8_t priority, SDL_Renderer* renderer ) {
    if ( !sdllay_init( lay, title, priority, renderer ) ) {
        return false;
    }
    free( lay->memory );
    lay->memory = 0;
    lay->argb = (uint32_t*) calloc( (size_t) SDL_SCREENWIDTH * SDL_SCREENHEIGHT, sizeof(uint32_t) );
    if ( lay->argb == 0 ) {
        perror( "calloc(3)" );
        SDL_DestroyTexture( lay->texture ); lay->texture = 0;
        return false;
    }
    return true;
}

void sdllay_cleanup( sdllayer_t* lay ) {
    free( lay->memory ); lay->memory = 0;
    free( lay->argb ); lay->argb = 0;
    SDL_DestroyTexture( lay->texture ); lay->texture = 0;
}

void sdllay_setcall( sdllayer_t* lay, sdllaycb_t callback, void* userdata ) {
    lay->callback = callback;
    lay->userdata = userdata;
}

bool sdllay_init_many( sdllayer_t* lay, size_t cnt, const char* const* titles, SDL_Renderer* renderer ) {
    bool ok = true;
    for ( size_t i=0; i < cnt; ++i ) {
        if ( !sdllay_init( &lay[i], titles[i], (uint8_t) i, renderer ) ) {
            if ( i > 0U ) {
                sdllay_cleanup_many( lay, i - 1U );
            }
            ok = false;
            break;
        }
    }
    return ok;
}

void sdllay_cleanup_many( sdllayer_t* lay, size_t cnt ) {
    while ( cnt-- ) {
        sdllay_cleanup( &lay[cnt] );
    }
}

bool sdllay_needsredraw( const sdllayer_t* lay, size_t cnt ) {
    for ( size_t i=0; i < cnt; ++i ) {
        if ( __atomic_load_n( &lay[i].modified, __ATOMIC_ACQUIRE ) && !lay[i].disabled ) return true;
    }
    return false;
}

bool sdllay_to_texture( sdllayer_t* lay ) {
    static uint32_t* buf = 0;
    if ( buf == 0 ) {
        buf = (uint32_t*) malloc( sizeof(uint32_t) * SDL_SCREENWIDTH * SDL_SCREENHEIGHT );
        if ( buf == 0 ) {
            fprintf( stderr, "can't allocate texture buffer\n" );
            return false;
        }
    }
    if ( !__atomic_load_n( &lay->modified, __ATOMIC_ACQUIRE ) || lay->disabled ) {
        return true;
    }
    // The flag is cleared *before* the layer memory is read, with an atomic exchange (a full fence): a change that the other thread makes while the
    // layer is converted sets the flag again and is drawn in the next frame, never lost. (There are no locks: the memory itself may be torn.)
    if ( !__atomic_exchange_n( &lay->modified, false, __ATOMIC_ACQ_REL ) ) {
        return true;
    }
    if ( lay->callback ) {
        lay->callback( lay, lay->userdata );
    }
    const uint32_t* pixels = buf;
    if ( lay->argb != 0 ) {
        pixels = lay->argb;                         // (a 32 bit layer: nothing to convert)
    } else {
        const uint8_t* source = lay->memory;
        const uint32_t* palette = lay->palette;
        uint32_t* target = buf;
        size_t pixcnt = SDL_SCREENWIDTH * SDL_SCREENHEIGHT;
        while ( pixcnt-- ) {
            *target++ = palette[*source++];
        }
    }
    int rv = SDL_UpdateTexture(
        lay->texture,
        0,
        pixels,
        SDL_SCREENWIDTH * sizeof(uint32_t)
    );
    if ( rv < 0 ) {
        fprintf( stderr, "layer %d: SDL_UpdateTexture failed with: %s\n", (int) lay->priority, SDL_GetError() );
        return false;
    }
    return true;
}

bool sdllay_to_texture_many( sdllayer_t* lay, size_t cnt ) {
    for ( size_t i=0; i < cnt; ++i ) {
        if ( !sdllay_to_texture( &lay[i] ) ) {
            return false;
        }
    }
    return true;
}

void sdllay_draw_texture( const sdllayer_t* lay, SDL_Renderer* renderer ) {
    if ( lay->disabled ) {
        return;
    }
    SDL_RenderCopy(
        renderer,
        lay->texture,
        0,
        0
    );
}

void sdllay_draw_texture_many( const sdllayer_t* lay, size_t cnt, SDL_Renderer* renderer ) {
    for ( size_t i=0; i < cnt; ++i ) {
        const sdllayer_t* l = &lay[0];
        bool found = false;
        for ( uint8_t j=0; j < (uint8_t) cnt; ++j, ++l ) {
            if ( l->priority == (uint8_t) i ) {
                found = true;
                break;
            }
        }
        if ( found ) {
            sdllay_draw_texture( l, renderer );
        }
    }
}

bool sdllay_findcolor( const char* name, uint32_t* pargb ) {
    return pal_find( name, pargb );
}

void sdllay_reset_palette( sdllayer_t* lay ) {
    for ( int i=0; i < 256; ++i ) {
        lay->palette[i] = pal_argb( i );
    }
    __atomic_store_n( &lay->modified, true, __ATOMIC_RELEASE );
}

void sdllay_set_modified( sdllayer_t* lay ) {
    __atomic_store_n( &lay->modified, true, __ATOMIC_RELEASE );        // (after the data was written)
}

void sdllay_enable( sdllayer_t* lay ) {
    lay->disabled = false;
}

void sdllay_disable( sdllayer_t* lay ) {
    lay->disabled = true;
}

bool sdllay_enabled( const sdllayer_t* lay ) {
    return !lay->disabled;
}

void sdllay_switch_priority( sdllayer_t* lay, size_t cnt, uint8_t index1, uint8_t index2 ) {
    if ( index1 >= cnt || index2 >= cnt || index1 == index2 ) {
        return;
    }
    uint8_t t = lay[index1].priority;
    lay[index1].priority = lay[index2].priority;
    lay[index2].priority = t;
}
