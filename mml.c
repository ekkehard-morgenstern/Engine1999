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

#include "mml.h"

typedef struct _mml_state_t {
    const char* p;
    int         octave;
    int         length;
    int         tempo;
    float       vol;
    float       legato;         // part of a note that sounds
} mml_state_t;

static bool mml_add( mml_t* mml, size_t* alloc, const mml_event_t* ev, char err[MML_ERRLEN] ) {
    if ( mml->count >= MML_MAXEVENTS ) {
        snprintf( err, MML_ERRLEN, "Music string too long" );
        return false;
    }
    if ( mml->count >= *alloc ) {
        size_t na = *alloc == 0 ? 64U : *alloc * 2U;
        mml_event_t* ne = (mml_event_t*) realloc( mml->events, na * sizeof(mml_event_t) );
        if ( ne == 0 ) {
            snprintf( err, MML_ERRLEN, "Out of memory" );
            return false;
        }
        mml->events = ne; *alloc = na;
    }
    mml->events[ mml->count++ ] = *ev;
    return true;
}

static void mml_skip( mml_state_t* st ) {
    while ( *st->p == ' ' || *st->p == '\t' || *st->p == ';' || *st->p == '\n' || *st->p == '\r' ) {
        ++st->p;
    }
}

// an unsigned number (digits, with decimals if allowed) after a command; false if there is none
static bool mml_number( mml_state_t* st, bool decimals, double* pval ) {
    mml_skip( st );
    if ( !( *st->p >= '0' && *st->p <= '9' ) && !( decimals && *st->p == '.' ) ) {
        return false;
    }
    double v = 0.0;
    while ( *st->p >= '0' && *st->p <= '9' ) {
        v = v * 10.0 + (double)( *st->p++ - '0' );
        if ( v > 1.0e6 ) v = 1.0e6;
    }
    if ( decimals && *st->p == '.' ) {
        ++st->p;
        double scale = 0.1;
        while ( *st->p >= '0' && *st->p <= '9' ) {
            v += scale * (double)( *st->p++ - '0' );
            scale *= 0.1;
        }
    }
    *pval = v;
    return true;
}

// optional length and dots after a note or rest: seconds
static bool mml_duration( mml_state_t* st, float* pseconds, char err[MML_ERRLEN] ) {
    int len = st->length;
    if ( *st->p >= '0' && *st->p <= '9' ) {
        double v = 0.0;
        mml_number( st, false, &v );
        if ( v < 1.0 || v > 64.0 ) {
            snprintf( err, MML_ERRLEN, "Note length must be 1..64" );
            return false;
        }
        len = (int) v;
    }
    double quarters = 4.0 / (double) len, add = quarters;
    while ( *st->p == '.' ) {
        ++st->p;
        add /= 2.0;
        quarters += add;
    }
    *pseconds = (float)( quarters * 60.0 / (double) st->tempo );
    return true;
}

// a comma between values (blanks around it are skipped)
static bool mml_comma( mml_state_t* st ) {
    mml_skip( st );
    if ( *st->p == ',' ) {
        ++st->p;
        return true;
    }
    return false;
}

// the length that may follow the frequencies of H and S: ", n" and dots, else the default length
static bool mml_effect_duration( mml_state_t* st, float* pseconds, char err[MML_ERRLEN] ) {
    if ( mml_comma( st ) ) {
        mml_skip( st );
        if ( !( *st->p >= '0' && *st->p <= '9' ) ) {
            snprintf( err, MML_ERRLEN, "Note length expected" );
            return false;
        }
    }
    return mml_duration( st, pseconds, err );
}

// n numbers separated by commas, each within lo..hi; false (nothing set) if one is missing or out of range
static bool mml_values( mml_state_t* st, int n, const double* lo, const double* hi, double* out ) {
    for ( int i=0; i < n; ++i ) {
        if ( i > 0 && !mml_comma( st ) ) {
            return false;
        }
        if ( !mml_number( st, true, &out[i] ) || out[i] < lo[i] || out[i] > hi[i] ) {
            return false;
        }
    }
    return true;
}

bool mml_parse( const char* text, mml_t* mml, char err[MML_ERRLEN] ) {
    mml->events = 0; mml->count = 0;
    size_t alloc = 0;
    mml_state_t st = { text, 4, 4, 120, 0.5f, 0.875f };
    static const int semis[7] = { 9, 11, 0, 2, 4, 5, 7 };      // A B C D E F G
    char c = 0;
    for (;;) {
        mml_skip( &st );
        c = *st.p;
        if ( c == '\0' ) {
            return true;
        }
        ++st.p;
        if ( c >= 'a' && c <= 'z' ) {
            c = (char)( c - 'a' + 'A' );
        }
        mml_event_t ev;
        memset( &ev, 0, sizeof(ev) );
        double v = 0.0;
        if ( c >= 'A' && c <= 'G' ) {
            int semi = semis[ c - 'A' ];
            if ( *st.p == '#' || *st.p == '+' ) {
                ++semi; ++st.p;
            } else if ( *st.p == '-' ) {
                --semi; ++st.p;
            }
            if ( !mml_duration( &st, &ev.seconds, err ) ) {
                goto FAIL;
            }
            int midi = 12 * ( st.octave + 1 ) + semi;
            double freq = 440.0 * pow( 2.0, (double)( midi - 69 ) / 12.0 );
            if ( freq < 20.0 || freq > 20000.0 ) {
                snprintf( err, MML_ERRLEN, "Note out of range" );
                goto FAIL;
            }
            ev.kind = MML_TONE; ev.freq = (float) freq; ev.vol = st.vol;
            ev.gap = ev.seconds * ( 1.0f - st.legato );
            ev.seconds -= ev.gap;
        } else if ( c == 'R' || c == 'P' ) {
            if ( !mml_duration( &st, &ev.seconds, err ) ) {
                goto FAIL;
            }
            ev.kind = MML_REST;
        } else {
            switch ( c ) {
                case 'O':
                    if ( !mml_number( &st, false, &v ) || v > 8.0 ) goto RANGE;
                    st.octave = (int) v;
                    continue;
                case '<':
                    if ( st.octave > 0 ) --st.octave;
                    continue;
                case '>':
                    if ( st.octave < 8 ) ++st.octave;
                    continue;
                case 'L':
                    if ( !mml_number( &st, false, &v ) || v < 1.0 || v > 64.0 ) goto RANGE;
                    st.length = (int) v;
                    continue;
                case 'T':
                    if ( !mml_number( &st, false, &v ) || v < 1.0 || v > 1000.0 ) goto RANGE;
                    st.tempo = (int) v;
                    continue;
                case 'V':
                    if ( !mml_number( &st, true, &v ) || v > 1.0 ) goto RANGE;
                    st.vol = (float) v;
                    continue;
                case 'W':
                    if ( !mml_number( &st, false, &v ) || v > 3.0 ) goto RANGE;
                    ev.kind = MML_WAVE; ev.wave = (int) v;
                    break;
                case 'H': case 'S': {       // a tone / a sweep in Hz
                    static const double lo[2] = { 20.0, 20.0 }, hi[2] = { 20000.0, 20000.0 };
                    double f[2];
                    if ( !mml_values( &st, c == 'S' ? 2 : 1, lo, hi, f ) ) goto RANGE;
                    if ( !mml_effect_duration( &st, &ev.seconds, err ) ) goto FAIL;
                    ev.kind = MML_TONE; ev.freq = (float) f[0]; ev.freq2 = c == 'S' ? (float) f[1] : 0.0f; ev.vol = st.vol;
                    ev.gap = ev.seconds * ( 1.0f - st.legato );
                    ev.seconds -= ev.gap;
                    break;
                }
                case 'X': {
                    if ( *st.p == 'R' || *st.p == 'r' ) {
                        ++st.p;
                        ev.kind = MML_ENV; ev.p[0] = -1.0f;
                        break;
                    }
                    static const double lo[4] = { 0, 0, 0, 0 }, hi[4] = { 60, 60, 1, 60 };
                    double v4[4];
                    if ( !mml_values( &st, 4, lo, hi, v4 ) ) goto RANGE;
                    ev.kind = MML_ENV;
                    for ( int k=0; k < 4; ++k ) ev.p[k] = (float) v4[k];
                    break;
                }
                case '~': case '^': {
                    static const double lo[2] = { 0, 0 }, hi_v[2] = { 100, 24 }, hi_t[2] = { 100, 1 };
                    double v2[2];
                    if ( !mml_values( &st, 2, lo, c == '~' ? hi_v : hi_t, v2 ) ) goto RANGE;
                    ev.kind = c == '~' ? MML_VIB : MML_TREM; ev.p[0] = (float) v2[0]; ev.p[1] = (float) v2[1];
                    break;
                }
                case 'M': {
                    char m = *st.p++;
                    if ( m >= 'a' && m <= 'z' ) m = (char)( m - 'a' + 'A' );
                    if ( m == 'L' ) st.legato = 1.0f;
                    else if ( m == 'N' ) st.legato = 0.875f;
                    else if ( m == 'S' ) st.legato = 0.75f;
                    else {
                        snprintf( err, MML_ERRLEN, "ML, MN or MS expected" );
                        goto FAIL;
                    }
                    continue;
                }
                default:
                    snprintf( err, MML_ERRLEN, "Unknown music command '%c'", c );
                    goto FAIL;
            }
        }
        if ( !mml_add( mml, &alloc, &ev, err ) ) {
            goto FAIL;
        }
    }
RANGE:
    snprintf( err, MML_ERRLEN, "Value missing or out of range after '%c'", c );
FAIL:
    mml_free( mml );
    return false;
}

void mml_free( mml_t* mml ) {
    free( mml->events );
    mml->events = 0; mml->count = 0;
}
