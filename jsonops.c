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


#include "jsonops.h"
#include "gfxops.h"
#include "iodev.h"

static json_node_t* doc = 0;

json_node_t* jsonops_root( void ) { return doc; }
json_node_t** jsonops_rootref( void ) { return &doc; }

void jsonops_free( void ) {
    json_node_free( doc );
    doc = 0;
}

// a file name: ".json" is added if the last part has no extension
static bool json_name( const char* s, size_t slen, char* buf, size_t bufsz, const char** err ) {
    if ( strlen( s ) != slen || slen == 0 || slen + 6U > bufsz ) {
        *err = "Illegal argument (file name)";
        return false;
    }
    memcpy( buf, s, slen + 1U );
    const char* base = strrchr( buf, '/' );
    base = base ? base + 1 : buf;
    const char* colon = strrchr( base, ':' );
    if ( colon ) {
        base = colon + 1;
    }
    if ( strchr( base, '.' ) == 0 && *base != 0 ) {
        strcat( buf, ".json" );
    }
    return true;
}

static bool put_out( const char* v, size_t len, uint8_t* out, size_t outsz, size_t* outlen, const char** err ) {
    if ( len > outsz - 1U || len > 65535U ) {
        *err = "String too long";
        return false;
    }
    memcpy( out, v, len );
    *outlen = len;
    return true;
}

static bool put_text( const char* v, uint8_t* out, size_t outsz, size_t* outlen, const char** err ) {
    return put_out( v, strlen( v ), out, outsz, outlen, err );
}

bool jsonops_run( int op, const double* a, int na, const char* s, size_t slen, uint8_t* out, size_t outsz, size_t* outlen, const char* defdriver,
    const char** err ) {
    static char errbuf[200];
    (void) na;
    *outlen = 0;
    const char* path = s != 0 ? s : "";
    size_t plen = s != 0 ? slen : 0;
    const char* value = 0; size_t vlen = 0;
    if ( op == GFXOP_JSONSETS ) {                   // path, NUL, text
        const char* nul = s != 0 ? (const char*) memchr( s, '\0', slen ) : 0;
        if ( nul == 0 ) {
            *err = "Illegal argument";
            return false;
        }
        plen = (size_t)( nul - s );
        value = nul + 1;
        vlen = slen - plen - 1U;
    }
    switch ( op ) {
        case GFXOP_JSONCLEAR:
            jsonops_free();
            return true;
        case GFXOP_JSONLOAD: {
            char name[1100];
            if ( !json_name( s, slen, name, sizeof(name), err ) ) {
                return false;
            }
            uint8_t* data = 0; size_t len = 0;
            const char* ioerr = 0;
            if ( !iodev_read_all( name, defdriver, &data, &len, &ioerr ) ) {
                snprintf( errbuf, sizeof(errbuf), "%s", ioerr ? ioerr : "File not found" );
                *err = errbuf;
                return false;
            }
            json_node_t* n = 0;
            char perr[120];
            bool ok = json_parse( (const char*) data, len, &n, perr, sizeof(perr) );
            free( data );
            if ( !ok ) {
                snprintf( errbuf, sizeof(errbuf), "JSON: %s", perr );
                *err = errbuf;
                return false;
            }
            jsonops_free();
            doc = n;
            return true;
        }
        case GFXOP_JSONPARSE: {
            json_node_t* n = 0;
            char perr[120];
            if ( !json_parse( s, slen, &n, perr, sizeof(perr) ) ) {
                snprintf( errbuf, sizeof(errbuf), "JSON: %s", perr );
                *err = errbuf;
                return false;
            }
            jsonops_free();
            doc = n;
            return true;
        }
        case GFXOP_JSONSAVE: {
            char name[1100];
            if ( !json_name( s, slen, name, sizeof(name), err ) ) {
                return false;
            }
            if ( doc == 0 ) {
                *err = "No JSON document";
                return false;
            }
            size_t len = 0;
            char* text = json_print( doc, !( a[0] > -1.0e8 && a[0] == 0.0 ), &len );
            if ( text == 0 ) {
                *err = "Out of memory";
                return false;
            }
            const char* ioerr = 0;
            bool ok = iodev_write_all( name, defdriver, text, len, &ioerr );
            free( text );
            if ( !ok ) {
                snprintf( errbuf, sizeof(errbuf), "%s", ioerr ? ioerr : "Cannot write the file" );
                *err = errbuf;
            }
            return ok;
        }
        case GFXOP_JSONTEXT: {
            json_node_t* n = json_find( doc, path, plen );
            if ( n == 0 ) {
                *err = "No such JSON element";
                return false;
            }
            size_t len = 0;
            char* text = json_print( n, false, &len );
            if ( text == 0 ) {
                *err = "Out of memory";
                return false;
            }
            bool ok = put_out( text, len, out, outsz, outlen, err );
            free( text );
            return ok;
        }
        case GFXOP_JSONGET: {
            json_node_t* n = json_find( doc, path, plen );
            size_t len = 0;
            char* text = json_valuetext( n, &len );
            if ( text == 0 ) {
                *err = "Out of memory";
                return false;
            }
            bool ok = put_out( text, len, out, outsz, outlen, err );
            free( text );
            return ok;
        }
        case GFXOP_JSONTYPE:
            return put_text( json_typename( json_find( doc, path, plen ) ), out, outsz, outlen, err );
        case GFXOP_JSONCOUNT: {
            json_node_t* n = json_find( doc, path, plen );
            char t[24];
            snprintf( t, sizeof(t), "%u", n != 0 && ( n->type == JSON_ARR || n->type == JSON_OBJ ) ? (unsigned) n->nkids : 0U );
            return put_text( t, out, outsz, outlen, err );
        }
        case GFXOP_JSONKEY: {
            json_node_t* n = json_find( doc, path, plen );
            const char* k = "";
            if ( n != 0 && n->type == JSON_OBJ && a[0] >= 0.0 && a[0] < (double) n->nkids && n->kids[(size_t) a[0]]->key != 0 ) {
                k = n->kids[(size_t) a[0]]->key;
            }
            return put_text( k, out, outsz, outlen, err );
        }
        case GFXOP_JSONDELETE:
            json_delete( &doc, path, plen );
            return true;
        default:
            break;
    }
    // the statements that write
    const char* merr = 0;
    json_node_t* n = json_make( &doc, path, plen, &merr );
    if ( n == 0 ) {
        *err = merr != 0 ? merr : "Illegal argument";
        return false;
    }
    switch ( op ) {
        case GFXOP_JSONSET:
            json_node_reset( n, JSON_NUM );
            n->num = a[0];
            break;
        case GFXOP_JSONSETS:
            if ( !json_node_setstr( n, value, vlen ) ) {
                *err = "Out of memory";
                return false;
            }
            break;
        case GFXOP_JSONSETB:
            json_node_reset( n, JSON_BOOL );
            n->num = a[0] != 0.0 ? 1.0 : 0.0;
            break;
        case GFXOP_JSONNULL:
            json_node_reset( n, JSON_NULL );
            break;
        case GFXOP_JSONNEW:
            json_node_reset( n, a[0] == 0.0 ? JSON_OBJ : JSON_ARR );
            break;
        default:
            *err = "Internal error (JSON operation)";
            return false;
    }
    return true;
}
