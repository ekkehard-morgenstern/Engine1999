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



#include "m3dworld.h"
#include "gfxops.h"

static char mw_err[240];

static bool fail( const char* fmt, ... ) {
    va_list ap;
    va_start( ap, fmt );
    vsnprintf( mw_err, sizeof(mw_err), fmt, ap );
    va_end( ap );
    return false;
}

static const json_node_t* member( const json_node_t* o, const char* key ) {
    if ( o == 0 || o->type != JSON_OBJ ) {
        return 0;
    }
    for ( size_t i=0; i < o->nkids; ++i ) {
        if ( o->kids[i]->key != 0 && strcmp( o->kids[i]->key, key ) == 0 ) {
            return o->kids[i];
        }
    }
    return 0;
}

static bool num( const json_node_t* o, const char* key, const char* what, double def, double* v ) {
    const json_node_t* n = member( o, key );
    *v = def;
    if ( n == 0 ) {
        return true;
    }
    if ( n->type != JSON_NUM ) {
        return fail( "%s: \"%s\" is not a number", what, key );
    }
    *v = n->num;
    return true;
}

static bool need( const json_node_t* o, const char* key, const char* what, double* v ) {
    if ( member( o, key ) == 0 ) {
        return fail( "%s: \"%s\" is missing", what, key );
    }
    return num( o, key, what, 0.0, v );
}

// n numbers of an array member (when it is there)
static bool numarr( const json_node_t* a, const char* what, size_t n, double* v ) {
    if ( a->type != JSON_ARR || a->nkids != n ) {
        return fail( "%s needs %u numbers", what, (unsigned) n );
    }
    for ( size_t i=0; i < n; ++i ) {
        if ( a->kids[i]->type != JSON_NUM ) {
            return fail( "%s needs %u numbers", what, (unsigned) n );
        }
        v[i] = a->kids[i]->num;
    }
    return true;
}

static int flagbit( const char* s ) {
    if ( strcmp( s, "masked" ) == 0 ) { return M3D_OF_MASKED; }
    if ( strcmp( s, "translucent" ) == 0 ) { return M3D_OF_TRANS; }
    if ( strcmp( s, "pass" ) == 0 ) { return M3D_OF_PASS; }
    if ( strcmp( s, "seethru" ) == 0 ) { return M3D_OF_SEETHRU; }
    if ( strcmp( s, "twosided" ) == 0 ) { return M3D_OF_TWOSIDED; }
    return -1;
}

static bool flags( const json_node_t* o, const char* what, int* out ) {
    const json_node_t* f = member( o, "flags" );
    *out = 0;
    if ( f == 0 ) {
        return true;
    }
    if ( f->type == JSON_NUM ) {
        *out = (int) f->num;
        return true;
    }
    if ( f->type != JSON_ARR ) {
        return fail( "%s: \"flags\" is a number or a list of names", what );
    }
    for ( size_t i=0; i < f->nkids; ++i ) {
        int b = f->kids[i]->type == JSON_STR ? flagbit( f->kids[i]->str ) : -1;
        if ( b < 0 ) {
            return fail( "%s: unknown flag (masked, translucent, pass, seethru, twosided)", what );
        }
        *out |= b;
    }
    return true;
}

static bool wop9( m3d_t* m, const char* what, int op, int na, double a0, double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8 ) {
    const double a[9] = { a0, a1, a2, a3, a4, a5, a6, a7, a8 };
    const char* e = 0;
    if ( !m3d_op( m, op, a, na, 0, 0, &e ) ) {
        return fail( "%s: the renderer refused (operation %d): %s", what, op, e != 0 ? e : "bad argument" );
    }
    return true;
}

static bool wop( m3d_t* m, const char* what, int op, int na, double a0, double a1, double a2, double a3, double a4, double a5, double a6, double a7 ) {
    return wop9( m, what, op, na, a0, a1, a2, a3, a4, a5, a6, a7, 0 );
}

static bool readobject( m3d_t* m, const json_node_t* o, size_t i ) {
    char what[64];
    snprintf( what, sizeof(what), "objects[%u]", (unsigned) i );
    double id, light, pos[3] = { 0, 0, 0 }, yaw;
    int fl;
    if ( o->type != JSON_OBJ ) {
        return fail( "%s is not an object", what );
    }
    if ( !need( o, "id", what, &id ) || !num( o, "light", what, 255, &light ) || !num( o, "yaw", what, 0, &yaw ) || !flags( o, what, &fl ) ) {
        return false;
    }
    const json_node_t* p = member( o, "pos" );
    if ( p != 0 && !numarr( p, what, 3, pos ) ) {
        return false;
    }
    if ( !wop( m, what, GFXOP_M3DOBJECT, 3, id, fl, light, 0, 0, 0, 0, 0 ) || !wop( m, what, GFXOP_M3DOBJPOS, 5, id, pos[0], pos[1], pos[2], yaw, 0, 0, 0 ) ) {
        return false;
    }
    const json_node_t* bx = member( o, "boxes" ), * qs = member( o, "quads" );
    if ( bx != 0 && bx->type != JSON_ARR ) {
        return fail( "%s: \"boxes\" must be an array", what );
    }
    for ( size_t k=0; bx != 0 && k < bx->nkids; ++k ) {
        double b[8] = { 0, 0, 0, 0, 0, 0, 0, 63 };
        char w2[96];
        snprintf( w2, sizeof(w2), "%s.boxes[%u]", what, (unsigned) k );
        const json_node_t* bk = bx->kids[k];
        size_t nb = bk->type == JSON_ARR && bk->nkids == 8 ? 8 : 7;       // the optional 8th number: the faces to build (bit mask, default 63)
        if ( !numarr( bk, w2, nb, b ) || !wop9( m, w2, GFXOP_M3DBOX, 9, id, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7] ) ) {
            return false;
        }
    }
    if ( qs != 0 && qs->type != JSON_ARR ) {
        return fail( "%s: \"quads\" must be an array", what );
    }
    for ( size_t k=0; qs != 0 && k < qs->nkids; ++k ) {
        double q[13] = { 0 };
        char w2[96];
        snprintf( w2, sizeof(w2), "%s.quads[%u]", what, (unsigned) k );
        if ( !numarr( qs->kids[k], w2, 13, q ) ) {
            return false;
        }
        double base = m3d_nverts( m, (int) id );
        for ( int v=0; v < 4; ++v ) {
            if ( !wop( m, w2, GFXOP_M3DVERTEX, 4, id, q[v*3], q[v*3+1], q[v*3+2], 0, 0, 0, 0 ) ) {
                return false;
            }
        }
        if ( !wop( m, w2, GFXOP_M3DQUAD, 6, id, base, base + 1, base + 2, base + 3, q[12], 0, 0 ) ) {
            return false;
        }
    }
    return true;
}

bool m3dworld_load( m3d_t* m, const json_node_t* root, const char** err ) {
    *err = mw_err;
    mw_err[0] = '\0';
    if ( root == 0 || root->type != JSON_OBJ ) {
        return fail( "there is no world document (JSON LOAD first)" );
    }
    const json_node_t* fmt = member( root, "format" );
    if ( fmt == 0 || fmt->type != JSON_STR || strcmp( fmt->str, "e99-mesh-world" ) != 0 ) {
        return fail( "not a mesh world document: \"format\" must be \"e99-mesh-world\"" );
    }
    const json_node_t* objs = member( root, "objects" ), * zones = member( root, "zones" ), * things = member( root, "things" ), * env = member( root, "environment" );
    if ( ( objs != 0 && objs->type != JSON_ARR ) || ( zones != 0 && zones->type != JSON_ARR ) || ( things != 0 && things->type != JSON_ARR ) ) {
        return fail( "\"objects\", \"zones\" and \"things\" must be arrays" );
    }
    if ( !wop( m, "world", GFXOP_M3DCLEAR, 0, 0, 0, 0, 0, 0, 0, 0, 0 ) ) {
        return false;
    }
    for ( size_t i=0; objs != 0 && i < objs->nkids; ++i ) {
        if ( !readobject( m, objs->kids[i], i ) ) {
            return false;
        }
    }
    for ( size_t i=0; zones != 0 && i < zones->nkids; ++i ) {
        const json_node_t* z = zones->kids[i];
        char what[48];
        double id, tag, b[4] = { 0, 0, 0, 0 }, zz[2] = { 0, 0 };
        snprintf( what, sizeof(what), "zones[%u]", (unsigned) i );
        const json_node_t* bn = member( z, "box" ), * zn = member( z, "z" );
        if ( !need( z, "id", what, &id ) || !num( z, "tag", what, 0, &tag ) || bn == 0 || zn == 0 || !numarr( bn, what, 4, b ) || !numarr( zn, what, 2, zz ) ||
             !wop( m, what, GFXOP_M3DZONE, 8, id, b[0], b[1], b[2], b[3], zz[0], zz[1], tag ) ) {
            if ( mw_err[0] == '\0' ) { fail( "%s: needs id, box [x1,y1,x2,y2] and z [z1,z2]", what ); }
            return false;
        }
    }
    for ( size_t i=0; things != 0 && i < things->nkids; ++i ) {
        const json_node_t* t = things->kids[i];
        char what[48];
        double id, tex, x, y, z, w, h;
        snprintf( what, sizeof(what), "things[%u]", (unsigned) i );
        if ( !need( t, "id", what, &id ) || !need( t, "tex", what, &tex ) || !need( t, "x", what, &x ) || !need( t, "y", what, &y ) || !num( t, "z", what, 0, &z ) ||
             !need( t, "w", what, &w ) || !need( t, "h", what, &h ) || !wop( m, what, GFXOP_M3DTHING, 7, id, x, y, z, tex, w, h, 0 ) ) {
            return false;
        }
    }
    if ( env != 0 ) {
        double v;
        const json_node_t* sky = member( env, "sky" );
        if ( !num( env, "fog", "environment", GFX_NONE, &v ) ) { return false; }
        if ( v > -1.0e8 && !wop( m, "environment", GFXOP_M3DFOG, 1, v, 0, 0, 0, 0, 0, 0, 0 ) ) { return false; }
        if ( sky != 0 && sky->type == JSON_NUM && !wop( m, "environment", GFXOP_M3DSKY, 1, sky->num, 0, 0, 0, 0, 0, 0, 0 ) ) { return false; }
    }
    const json_node_t* st = member( root, "start" );
    if ( st != 0 ) {
        double x, y, z, a;
        if ( !need( st, "x", "start", &x ) || !need( st, "y", "start", &y ) || !num( st, "z", "start", 0, &z ) || !num( st, "angle", "start", 0, &a ) ||
             !wop( m, "start", GFXOP_M3DCAMERA, 4, x, y, z, a, 0, 0, 0, 0 ) ) {
            return false;
        }
    }
    return true;
}
