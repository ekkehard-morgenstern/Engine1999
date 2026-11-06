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

#include "stdtypes.h"
#include "printusing.h"

// -- scanning the format ---------------------------------------------------------------------

// does a number field start at fmt[i]? fills f and *pend
static bool pu_numfield( const char* fmt, size_t len, size_t i, pu_field_t* f, size_t* pend ) {
    memset( f, 0, sizeof(*f) );
    size_t j = i;
    unsigned hashes = 0;
    if ( j < len && fmt[j] == '+' ) {
        f->leadsign = true;
        ++j;
    }
    if ( j + 1 < len && fmt[j] == '*' && fmt[j+1] == '*' ) {
        f->stars = true;
        f->nint += 2U;
        j += 2U;
        if ( j < len && fmt[j] == '$' ) {
            f->dollar = true;
            ++j;                                // the dollar sign takes a place of its own
        }
    } else if ( j + 1 < len && fmt[j] == '$' && fmt[j+1] == '$' ) {
        f->dollar = true;
        f->nint += 1U;                          // one of the two characters is the digit position
        j += 2U;
    }
    for (;;) {
        if ( j < len && fmt[j] == '#' ) {
            ++f->nint;
            ++hashes;
            ++j;
        } else if ( j < len && fmt[j] == ',' ) {
            size_t k = j;
            while ( k < len && fmt[k] == ',' ) {
                ++k;
            }
            if ( k < len && ( fmt[k] == '#' || ( fmt[k] == '.' && hashes > 0 ) ) ) {   // a comma only counts among digit positions, also in front of the point
                f->comma = true;
                j = k;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    if ( j < len && fmt[j] == '.' && ( hashes > 0 || ( j + 1 < len && fmt[j+1] == '#' ) ) ) {
        ++j;
        f->point = true;
        while ( j < len && fmt[j] == '#' ) {
            ++f->ndec;
            ++hashes;
            ++j;
        }
    }
    if ( hashes == 0 ) {
        return false;                           // a lone sign, "**" or "$$" is text
    }
    if ( f->nint > 300U || f->ndec > 300U ) {
        return false;
    }
    if ( j + 3 < len && fmt[j] == '^' && fmt[j+1] == '^' && fmt[j+2] == '^' && fmt[j+3] == '^' ) {
        f->expo = true;
        j += 4U;
    }
    if ( !f->leadsign && j < len && ( fmt[j] == '+' || fmt[j] == '-' ) ) {
        f->trail = (uint8_t) fmt[j];
        ++j;
    }
    f->kind = PU_NUMBER;
    f->width = (uint16_t)( j - i );
    *pend = j;
    return true;
}

size_t pu_next( const char* fmt, size_t len, size_t pos, char* lit, size_t* litlen, pu_field_t* f ) {
    size_t n = 0, i = pos;
    memset( f, 0, sizeof(*f) );
    while ( i < len ) {
        char c = fmt[i];
        if ( c == '_' && i + 1 < len ) {
            lit[n++] = fmt[i+1];
            i += 2U;
        } else if ( c == '!' ) {
            f->kind = PU_STRING;
            f->width = 1U;
            f->start = i;
            *litlen = n;
            return i + 1U;
        } else if ( c == '&' ) {
            f->kind = PU_STRING;
            f->width = 0U;
            f->start = i;
            *litlen = n;
            return i + 1U;
        } else if ( c == '\\' ) {
            size_t j = i + 1U;
            while ( j < len && fmt[j] == ' ' ) {
                ++j;
            }
            if ( j < len && fmt[j] == '\\' ) {
                f->kind = PU_STRING;
                f->width = (uint16_t)( j - i + 1U );
                f->start = i;
                *litlen = n;
                return j + 1U;
            }
            lit[n++] = c;
            ++i;
        } else {
            size_t end = 0;
            if ( ( c == '#' || c == '.' || c == '+' || c == '*' || c == '$' || c == ',' ) && pu_numfield( fmt, len, i, f, &end ) ) {
                f->start = i;
                *litlen = n;
                return end;
            }
            memset( f, 0, sizeof(*f) );
            lit[n++] = c;
            ++i;
        }
    }
    *litlen = n;
    return len;
}

bool pu_has_field( const char* fmt, size_t len ) {
    pu_field_t f;
    size_t n = 0;
    char* lit = (char*) malloc( len + 1U );
    if ( lit == 0 ) {
        return true;
    }
    pu_next( fmt, len, 0, lit, &n, &f );
    free( lit );
    return f.kind != PU_NONE;
}

// -- numbers ---------------------------------------------------------------------------------------

// digits of fabs( x ) rounded to ndec decimals: int part into ip, fraction into fp (both terminated)
static void pu_split( double x, unsigned ndec, char* ip, size_t ipmax, char* fp, size_t fpmax ) {
    char buf[PU_NUMBER_MAX];
    // rounds half away from zero, as BASIC does (printf rounds an exact tie to the even digit: 2.25 -> 2.2)
    double y = fabs( x ), scale = pow( 10.0, (double) ndec );
    if ( ndec <= 15U && y * scale < 1e15 ) {
        y = floor( y * scale + 0.5 ) / scale;
    }
    snprintf( buf, sizeof(buf), "%.*f", (int) ndec, y );
    const char* dot = strchr( buf, '.' );
    size_t il = dot ? (size_t)( dot - buf ) : strlen( buf );
    if ( il >= ipmax ) {
        il = ipmax - 1U;
    }
    memcpy( ip, buf, il );
    ip[il] = '\0';
    snprintf( fp, fpmax, "%s", dot ? dot + 1 : "" );
}

size_t pu_number( const pu_field_t* f, double x, char* out ) {
    char ip[PU_NUMBER_MAX], fp[PU_NUMBER_MAX], body[PU_NUMBER_MAX], core[PU_NUMBER_MAX];
    char etext[16];
    etext[0] = '\0';
    unsigned nint = f->nint, ndec = f->ndec;
    int e = 0;
    if ( f->expo ) {
        if ( x != 0.0 ) {
            e = (int) floor( log10( fabs( x ) ) ) + 1 - (int) nint;
        }
        for ( int pass = 0; pass < 2; ++pass ) {
            pu_split( x == 0.0 ? 0.0 : fabs( x ) / pow( 10.0, e ), ndec, ip, sizeof(ip), fp, sizeof(fp) );
            size_t digits = ( ip[0] == '0' && ip[1] == '\0' ) ? 0U : strlen( ip );
            if ( digits > nint && x != 0.0 ) {
                ++e;                            // rounding carried into another digit
            } else {
                break;
            }
        }
        snprintf( etext, sizeof(etext), "E%c%02d", e < 0 ? '-' : '+', e < 0 ? -e : e );
    } else {
        pu_split( x, ndec, ip, sizeof(ip), fp, sizeof(fp) );
    }
    // a zero in front of the point is only there if there is a digit position for it
    if ( ip[0] == '0' && ip[1] == '\0' && nint == 0 ) {
        ip[0] = '\0';
    }
    // is the number zero after rounding? then no minus sign
    bool nonzero = false;
    for ( const char* p = ip; *p; ++p ) { if ( *p != '0' ) nonzero = true; }
    for ( const char* p = fp; *p; ++p ) { if ( *p != '0' ) nonzero = true; }
    bool neg = x < 0.0 && nonzero;
    // thousands separators
    size_t il = strlen( ip ), n = 0;
    for ( size_t k = 0; k < il; ++k ) {
        if ( f->comma && k > 0 && ( il - k ) % 3U == 0 ) {
            body[n++] = ',';
        }
        body[n++] = ip[k];
    }
    if ( f->point ) {
        body[n++] = '.';
        for ( const char* p = fp; *p; ++p ) {
            body[n++] = *p;
        }
    }
    body[n] = '\0';
    // the sign, the dollar sign, the number, the exponent, the trailing sign
    size_t m = 0;
    if ( f->leadsign ) {
        core[m++] = neg ? '-' : '+';
    } else if ( f->trail == 0 && neg ) {
        core[m++] = '-';
    }
    if ( f->dollar ) {
        core[m++] = '$';
    }
    memcpy( &core[m], body, n );
    m += n;
    for ( const char* p = etext; *p; ++p ) {
        core[m++] = *p;
    }
    if ( f->trail != 0 ) {
        core[m++] = f->trail == '+' ? ( neg ? '-' : '+' ) : ( neg ? '-' : ' ' );
    }
    core[m] = '\0';
    size_t o = 0;
    if ( m > f->width ) {
        out[o++] = '%';                         // does not fit: the number in full
    } else {
        char fill = f->stars ? '*' : ' ';
        for ( size_t k = m; k < f->width; ++k ) {
            out[o++] = fill;
        }
    }
    memcpy( &out[o], core, m );
    o += m;
    out[o] = '\0';
    return o;
}

size_t pu_string( const pu_field_t* f, const char* str, size_t slen, char* out ) {
    if ( f->width == 0 ) {
        memcpy( out, str, slen );
        return slen;
    }
    size_t k = 0;
    for ( ; k < f->width; ++k ) {
        out[k] = k < slen ? str[k] : ' ';
    }
    return k;
}
