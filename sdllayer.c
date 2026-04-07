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

#include "sdllayer.h"
#include "sdlscreen.h"

typedef struct _colinit_t {
    uint32_t argb;
    const char* title;
} colinit_t;

/*
    Colors partially taken from "Web colors" Wikipedia article,
    https://en.wikipedia.org/wiki/Web_colors

    Some transparent colors were culled due to the 256 palette limit.
*/
static const colinit_t colinittab[256] = {
    { UINT32_C(0X00000000), "Transparent" },
    { UINT32_C(0XFF000000), "Black" },
    { UINT32_C(0XFF000080), "Navy" },
    { UINT32_C(0XFF00008B), "DarkBlue" },
    { UINT32_C(0XFF0000CD), "MediumBlue" },
    { UINT32_C(0XFF0000FF), "Blue" },
    { UINT32_C(0XFF006400), "DarkGreen" },
    { UINT32_C(0XFF008000), "Green" },
    { UINT32_C(0XFF008080), "Teal" },
    { UINT32_C(0XFF008B8B), "DarkCyan" },
    { UINT32_C(0XFF00BFFF), "DeepSkyBlue" },
    { UINT32_C(0XFF00CED1), "DarkTurquoise" },
    { UINT32_C(0XFF00FA9A), "MediumSpringGreen" },
    { UINT32_C(0XFF00FF00), "Lime" },
    { UINT32_C(0XFF00FF7F), "SpringGreen" },
    { UINT32_C(0XFF00FFFF), "Aqua" },
    { UINT32_C(0XFF00FFFF), "Cyan" },
    { UINT32_C(0XFF191970), "MidnightBlue" },
    { UINT32_C(0XFF1E90FF), "DodgerBlue" },
    { UINT32_C(0XFF20B2AA), "LightSeaGreen" },
    { UINT32_C(0XFF228B22), "ForestGreen" },
    { UINT32_C(0XFF2E8B57), "SeaGreen" },
    { UINT32_C(0XFF2F4F4F), "DarkSlateGray" },
    { UINT32_C(0XFF32CD32), "LimeGreen" },
    { UINT32_C(0XFF3CB371), "MediumSeaGreen" },
    { UINT32_C(0XFF40E0D0), "Turquoise" },
    { UINT32_C(0XFF4169E1), "RoyalBlue" },
    { UINT32_C(0XFF4682B4), "SteelBlue" },
    { UINT32_C(0XFF483D8B), "DarkSlateBlue" },
    { UINT32_C(0XFF48D1CC), "MediumTurquoise" },
    { UINT32_C(0XFF4B0082), "Indigo" },
    { UINT32_C(0XFF556B2F), "DarkOliveGreen" },
    { UINT32_C(0XFF5F9EA0), "CadetBlue" },
    { UINT32_C(0XFF6495ED), "CornflowerBlue" },
    { UINT32_C(0XFF66CDAA), "MediumAquamarine" },
    { UINT32_C(0XFF696969), "DimGray" },
    { UINT32_C(0XFF6A5ACD), "SlateBlue" },
    { UINT32_C(0XFF6B8E23), "OliveDrab" },
    { UINT32_C(0XFF708090), "SlateGray" },
    { UINT32_C(0XFF778899), "LightSlateGray" },
    { UINT32_C(0XFF7B68EE), "MediumSlateBlue" },
    { UINT32_C(0XFF7CFC00), "LawnGreen" },
    { UINT32_C(0XFF7FFF00), "Chartreuse" },
    { UINT32_C(0XFF7FFFD4), "Aquamarine" },
    { UINT32_C(0XFF800000), "Maroon" },
    { UINT32_C(0XFF800080), "Purple" },
    { UINT32_C(0XFF808000), "Olive" },
    { UINT32_C(0XFF808080), "Gray" },
    { UINT32_C(0XFF87CEEB), "SkyBlue" },
    { UINT32_C(0XFF87CEFA), "LightSkyBlue" },
    { UINT32_C(0XFF8A2BE2), "BlueViolet" },
    { UINT32_C(0XFF8B0000), "DarkRed" },
    { UINT32_C(0XFF8B008B), "DarkMagenta" },
    { UINT32_C(0XFF8B4513), "SaddleBrown" },
    { UINT32_C(0XFF8FBC8F), "DarkSeaGreen" },
    { UINT32_C(0XFF90EE90), "LightGreen" },
    { UINT32_C(0XFF9370DB), "MediumPurple" },
    { UINT32_C(0XFF9400D3), "DarkViolet" },
    { UINT32_C(0XFF98FB98), "PaleGreen" },
    { UINT32_C(0XFF9932CC), "DarkOrchid" },
    { UINT32_C(0XFF9ACD32), "YellowGreen" },
    { UINT32_C(0XFFA0522D), "Sienna" },
    { UINT32_C(0XFFA52A2A), "Brown" },
    { UINT32_C(0XFFA9A9A9), "DarkGray" },
    { UINT32_C(0XFFADD8E6), "LightBlue" },
    { UINT32_C(0XFFADFF2F), "GreenYellow" },
    { UINT32_C(0XFFAFEEEE), "PaleTurquoise" },
    { UINT32_C(0XFFB0C4DE), "LightSteelBlue" },
    { UINT32_C(0XFFB0E0E6), "PowderBlue" },
    { UINT32_C(0XFFB22222), "Firebrick" },
    { UINT32_C(0XFFB8860B), "DarkGoldenrod" },
    { UINT32_C(0XFFBA55D3), "MediumOrchid" },
    { UINT32_C(0XFFBC8F8F), "RosyBrown" },
    { UINT32_C(0XFFBDB76B), "DarkKhaki" },
    { UINT32_C(0XFFC0C0C0), "Silver" },
    { UINT32_C(0XFFC71585), "MediumVioletRed" },
    { UINT32_C(0XFFCD5C5C), "IndianRed" },
    { UINT32_C(0XFFCD853F), "Peru" },
    { UINT32_C(0XFFD2691E), "Chocolate" },
    { UINT32_C(0XFFD2B48C), "Tan" },
    { UINT32_C(0XFFD3D3D3), "LightGray" },
    { UINT32_C(0XFFD8BFD8), "Thistle" },
    { UINT32_C(0XFFDA70D6), "Orchid" },
    { UINT32_C(0XFFDAA520), "Goldenrod" },
    { UINT32_C(0XFFDB7093), "PaleVioletRed" },
    { UINT32_C(0XFFDC143C), "Crimson" },
    { UINT32_C(0XFFDCDCDC), "Gainsboro" },
    { UINT32_C(0XFFDDA0DD), "Plum" },
    { UINT32_C(0XFFDEB887), "Burlywood" },
    { UINT32_C(0XFFE0FFFF), "LightCyan" },
    { UINT32_C(0XFFE6E6FA), "Lavender" },
    { UINT32_C(0XFFE9967A), "DarkSalmon" },
    { UINT32_C(0XFFEE82EE), "Violet" },
    { UINT32_C(0XFFEEE8AA), "PaleGoldenrod" },
    { UINT32_C(0XFFF08080), "LightCoral" },
    { UINT32_C(0XFFF0E68C), "Khaki" },
    { UINT32_C(0XFFF0F8FF), "AliceBlue" },
    { UINT32_C(0XFFF0FFF0), "Honeydew" },
    { UINT32_C(0XFFF0FFFF), "Azure" },
    { UINT32_C(0XFFF4A460), "SandyBrown" },
    { UINT32_C(0XFFF5DEB3), "Wheat" },
    { UINT32_C(0XFFF5F5DC), "Beige" },
    { UINT32_C(0XFFF5F5F5), "WhiteSmoke" },
    { UINT32_C(0XFFF5FFFA), "MintCream" },
    { UINT32_C(0XFFF8F8FF), "GhostWhite" },
    { UINT32_C(0XFFFA8072), "Salmon" },
    { UINT32_C(0XFFFAEBD7), "AntiqueWhite" },
    { UINT32_C(0XFFFAF0E6), "Linen" },
    { UINT32_C(0XFFFAFAD2), "LightGoldenrodYellow" },
    { UINT32_C(0XFFFDF5E6), "OldLace" },
    { UINT32_C(0XFFFF0000), "Red" },
    { UINT32_C(0XFFFF00FF), "Fuchsia" },
    { UINT32_C(0XFFFF00FF), "Magenta" },
    { UINT32_C(0XFFFF1493), "DeepPink" },
    { UINT32_C(0XFFFF4500), "OrangeRed" },
    { UINT32_C(0XFFFF6347), "Tomato" },
    { UINT32_C(0XFFFF69B4), "HotPink" },
    { UINT32_C(0XFFFF7F50), "Coral" },
    { UINT32_C(0XFFFF8C00), "DarkOrange" },
    { UINT32_C(0XFFFFA07A), "LightSalmon" },
    { UINT32_C(0XFFFFA500), "Orange" },
    { UINT32_C(0XFFFFB6C1), "LightPink" },
    { UINT32_C(0XFFFFC0CB), "Pink" },
    { UINT32_C(0XFFFFD700), "Gold" },
    { UINT32_C(0XFFFFDAB9), "PeachPuff" },
    { UINT32_C(0XFFFFDEAD), "NavajoWhite" },
    { UINT32_C(0XFFFFE4B5), "Moccasin" },
    { UINT32_C(0XFFFFE4C4), "Bisque" },
    { UINT32_C(0XFFFFE4E1), "MistyRose" },
    { UINT32_C(0XFFFFEBCD), "BlanchedAlmond" },
    { UINT32_C(0XFFFFEFD5), "PapayaWhip" },
    { UINT32_C(0XFFFFF0F5), "LavenderBlush" },
    { UINT32_C(0XFFFFF5EE), "Seashell" },
    { UINT32_C(0XFFFFF8DC), "Cornsilk" },
    { UINT32_C(0XFFFFFACD), "LemonChiffon" },
    { UINT32_C(0XFFFFFAF0), "FloralWhite" },
    { UINT32_C(0XFFFFFAFA), "Snow" },
    { UINT32_C(0XFFFFFF00), "Yellow" },
    { UINT32_C(0XFFFFFFE0), "LightYellow" },
    { UINT32_C(0XFFFFFFF0), "Ivory" },
    { UINT32_C(0XFFFFFFFF), "White" },
    { UINT32_C(0X88000000), "TransparentBlack" },
    { UINT32_C(0X88000080), "TransparentNavy" },
    { UINT32_C(0X8800008B), "TransparentDarkBlue" },
    { UINT32_C(0X880000CD), "TransparentMediumBlue" },
    { UINT32_C(0X880000FF), "TransparentBlue" },
    { UINT32_C(0X88006400), "TransparentDarkGreen" },
    { UINT32_C(0X88008000), "TransparentGreen" },
    { UINT32_C(0X88008080), "TransparentTeal" },
    { UINT32_C(0X88008B8B), "TransparentDarkCyan" },
    { UINT32_C(0X8800BFFF), "TransparentDeepSkyBlue" },
    { UINT32_C(0X8800CED1), "TransparentDarkTurquoise" },
    { UINT32_C(0X8800FA9A), "TransparentMediumSpringGreen" },
    { UINT32_C(0X8800FF00), "TransparentLime" },
    { UINT32_C(0X8800FF7F), "TransparentSpringGreen" },
    { UINT32_C(0X8800FFFF), "TransparentCyan" },
    { UINT32_C(0X88191970), "TransparentMidnightBlue" },
    { UINT32_C(0X881E90FF), "TransparentDodgerBlue" },
    { UINT32_C(0X8820B2AA), "TransparentLightSeaGreen" },
    { UINT32_C(0X88228B22), "TransparentForestGreen" },
    { UINT32_C(0X882E8B57), "TransparentSeaGreen" },
    { UINT32_C(0X882F4F4F), "TransparentDarkSlateGray" },
    { UINT32_C(0X8832CD32), "TransparentLimeGreen" },
    { UINT32_C(0X883CB371), "TransparentMediumSeaGreen" },
    { UINT32_C(0X8840E0D0), "TransparentTurquoise" },
    { UINT32_C(0X884169E1), "TransparentRoyalBlue" },
    { UINT32_C(0X884682B4), "TransparentSteelBlue" },
    { UINT32_C(0X88483D8B), "TransparentDarkSlateBlue" },
    { UINT32_C(0X8848D1CC), "TransparentMediumTurquoise" },
    { UINT32_C(0X884B0082), "TransparentIndigo" },
    { UINT32_C(0X88556B2F), "TransparentDarkOliveGreen" },
    { UINT32_C(0X885F9EA0), "TransparentCadetBlue" },
    { UINT32_C(0X886495ED), "TransparentCornflowerBlue" },
    { UINT32_C(0X8866CDAA), "TransparentMediumAquamarine" },
    { UINT32_C(0X88696969), "TransparentDimGray" },
    { UINT32_C(0X886A5ACD), "TransparentSlateBlue" },
    { UINT32_C(0X886B8E23), "TransparentOliveDrab" },
    { UINT32_C(0X88708090), "TransparentSlateGray" },
    { UINT32_C(0X88778899), "TransparentLightSlateGray" },
    { UINT32_C(0X887B68EE), "TransparentMediumSlateBlue" },
    { UINT32_C(0X887CFC00), "TransparentLawnGreen" },
    { UINT32_C(0X887FFF00), "TransparentChartreuse" },
    { UINT32_C(0X887FFFD4), "TransparentAquamarine" },
    { UINT32_C(0X88800000), "TransparentMaroon" },
    { UINT32_C(0X88800080), "TransparentPurple" },
    { UINT32_C(0X88808000), "TransparentOlive" },
    { UINT32_C(0X88808080), "TransparentGray" },
    { UINT32_C(0X8887CEEB), "TransparentSkyBlue" },
    { UINT32_C(0X8887CEFA), "TransparentLightSkyBlue" },
    { UINT32_C(0X888A2BE2), "TransparentBlueViolet" },
    { UINT32_C(0X888B0000), "TransparentDarkRed" },
    { UINT32_C(0X888B4513), "TransparentSaddleBrown" },
    { UINT32_C(0X888FBC8F), "TransparentDarkSeaGreen" },
    { UINT32_C(0X8890EE90), "TransparentLightGreen" },
    { UINT32_C(0X889400D3), "TransparentDarkViolet" },
    { UINT32_C(0X8898FB98), "TransparentPaleGreen" },
    { UINT32_C(0X889932CC), "TransparentDarkOrchid" },
    { UINT32_C(0X889ACD32), "TransparentYellowGreen" },
    { UINT32_C(0X88A0522D), "TransparentSienna" },
    { UINT32_C(0X88A52A2A), "TransparentBrown" },
    { UINT32_C(0X88A9A9A9), "TransparentDarkGray" },
    { UINT32_C(0X88ADD8E6), "TransparentLightBlue" },
    { UINT32_C(0X88ADFF2F), "TransparentGreenYellow" },
    { UINT32_C(0X88AFEEEE), "TransparentPaleTurquoise" },
    { UINT32_C(0X88B0C4DE), "TransparentLightSteelBlue" },
    { UINT32_C(0X88B0E0E6), "TransparentPowderBlue" },
    { UINT32_C(0X88BA55D3), "TransparentMediumOrchid" },
    { UINT32_C(0X88BDB76B), "TransparentDarkKhaki" },
    { UINT32_C(0X88C0C0C0), "TransparentSilver" },
    { UINT32_C(0X88C71585), "TransparentMediumVioletRed" },
    { UINT32_C(0X88CD5C5C), "TransparentIndianRed" },
    { UINT32_C(0X88CD853F), "TransparentPeru" },
    { UINT32_C(0X88D2691E), "TransparentChocolate" },
    { UINT32_C(0X88D2B48C), "TransparentTan" },
    { UINT32_C(0X88D3D3D3), "TransparentLightGray" },
    { UINT32_C(0X88DAA520), "TransparentGoldenrod" },
    { UINT32_C(0X88DC143C), "TransparentCrimson" },
    { UINT32_C(0X88DDA0DD), "TransparentPlum" },
    { UINT32_C(0X88E0FFFF), "TransparentLightCyan" },
    { UINT32_C(0X88E6E6FA), "TransparentLavender" },
    { UINT32_C(0X88EE82EE), "TransparentViolet" },
    { UINT32_C(0X88F0E68C), "TransparentKhaki" },
    { UINT32_C(0X88F0F8FF), "TransparentAliceBlue" },
    { UINT32_C(0X88F0FFF0), "TransparentHoneydew" },
    { UINT32_C(0X88F0FFFF), "TransparentAzure" },
    { UINT32_C(0X88F4A460), "TransparentSandyBrown" },
    { UINT32_C(0X88F5DEB3), "TransparentWheat" },
    { UINT32_C(0X88F5F5DC), "TransparentBeige" },
    { UINT32_C(0X88F5F5F5), "TransparentWhiteSmoke" },
    { UINT32_C(0X88F8F8FF), "TransparentGhostWhite" },
    { UINT32_C(0X88FA8072), "TransparentSalmon" },
    { UINT32_C(0X88FAF0E6), "TransparentLinen" },
    { UINT32_C(0X88FAFAD2), "TransparentLightGoldenrodYellow" },
    { UINT32_C(0X88FDF5E6), "TransparentOldLace" },
    { UINT32_C(0X88FF0000), "TransparentRed" },
    { UINT32_C(0X88FF00FF), "TransparentFuchsia" },
    { UINT32_C(0X88FF00FF), "TransparentMagenta" },
    { UINT32_C(0X88FF4500), "TransparentOrangeRed" },
    { UINT32_C(0X88FF69B4), "TransparentHotPink" },
    { UINT32_C(0X88FF7F50), "TransparentCoral" },
    { UINT32_C(0X88FF8C00), "TransparentDarkOrange" },
    { UINT32_C(0X88FFA07A), "TransparentLightSalmon" },
    { UINT32_C(0X88FFA500), "TransparentOrange" },
    { UINT32_C(0X88FFC0CB), "TransparentPink" },
    { UINT32_C(0X88FFD700), "TransparentGold" },
    { UINT32_C(0X88FFDAB9), "TransparentPeachPuff" },
    { UINT32_C(0X88FFE4E1), "TransparentMistyRose" },
    { UINT32_C(0X88FFF0F5), "TransparentLavenderBlush" },
    { UINT32_C(0X88FFF5EE), "TransparentSeashell" },
    { UINT32_C(0X88FFF8DC), "TransparentCornsilk" },
    { UINT32_C(0X88FFFAF0), "TransparentFloralWhite" },
    { UINT32_C(0X88FFFAFA), "TransparentSnow" },
    { UINT32_C(0X88FFFF00), "TransparentYellow" },
    { UINT32_C(0X88FFFFE0), "TransparentLightYellow" },
    { UINT32_C(0X88FFFFF0), "TransparentIvory" },
    { UINT32_C(0X88FFFFFF), "TransparentWhite" }
};

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
        lay->palette[i] = colinittab[i].argb;
    }
    // NOTE that the color names aren't currently used, which might change in a future version.
    lay->modified = true;
    lay->disabled = false;
    lay->priority = priority;
    lay->callback = 0;
    lay->userdata = 0;
    return true;
}

void sdllay_cleanup( sdllayer_t* lay ) {
    free( lay->memory ); lay->memory = 0;
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
        if ( lay[i].modified && !lay[i].disabled ) return true;
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
    if ( !lay->modified || lay->disabled ) {
        return true;
    }
    lay->modified = false;
    if ( lay->callback ) {
        lay->callback( lay, lay->userdata );
    }
    const uint8_t* source = lay->memory;
    const uint32_t* palette = lay->palette;
    uint32_t* target = buf;
    size_t pixcnt = SDL_SCREENWIDTH * SDL_SCREENHEIGHT;
    while ( pixcnt-- ) {
        *target++ = palette[*source++];
    }
    int rv = SDL_UpdateTexture(
        lay->texture,
        0,
        buf,
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

void sdllay_set_modified( sdllayer_t* lay ) {
    lay->modified = true;
}

void sdllay_enable( sdllayer_t* lay ) {
    lay->disabled = false;
}

void sdllay_disable( sdllayer_t* lay ) {
    lay->disabled = true;
}

void sdllay_switch_priority( sdllayer_t* lay, size_t cnt, uint8_t index1, uint8_t index2 ) {
    if ( index1 >= cnt || index2 >= cnt || index1 == index2 ) {
        return;
    }
    uint8_t t = lay[index1].priority;
    lay[index1].priority = lay[index2].priority;
    lay[index2].priority = t;
}
