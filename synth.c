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

#include "synth.h"

#define TWOPI   6.283185307179586

// -- helpers ---------------------------------------------------------------------------

float synth_notefreq( int note ) {
    if ( note < 0 || note >= SYNTH_NOTES ) {
        return 0.0f;
    }
    return (float)( 110.0 * pow( 2.0, (double) note / 12.0 ) );
}

uint64_t synth_beatsnsec( const synth_t* s, float beats ) {
    if ( beats <= 0.0f ) {
        return 0;
    }
    return (uint64_t)( (double) beats * 60.0e9 / (double) s->bpm );
}

// -- osc -------------------------------------------------------------------------------

static float synth_noise( synth_osc_t* o ) {        // xorshift32, -1 .. 1
    uint32_t x = o->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    o->rng = x;
    return (float)( (int32_t) x ) / 2147483648.0f;
}

static void synth_osc_init( synth_osc_t* o, uint32_t seed ) {
    memset( o, 0, sizeof(*o) );
    o->wave = SYNTH_SINE;
    o->rng  = seed != 0 ? seed : 1U;
}

// one sample, then advances the phase
static float synth_osc_next( synth_osc_t* o ) {
    float v;
    switch ( o->wave ) {
        case SYNTH_SQUARE:
            v = o->phase < 0.5 ? 0.5f : -0.5f;
            break;
        case SYNTH_TRIANGLE:
            v = (float)( o->phase < 0.5 ? 4.0 * o->phase - 1.0 : 3.0 - 4.0 * o->phase );
            break;
        case SYNTH_NOISE:
            v = o->held * 0.5f;
            break;
        case SYNTH_SINE:
        default:
            v = (float) sin( TWOPI * o->phase );
            break;
    }
    o->phase += o->step;
    if ( o->phase >= 1.0 ) {
        o->phase -= 1.0;
        if ( o->phase >= 1.0 ) {        // a step larger than a cycle can't happen (freq <= 20 kHz), but stay in range
            o->phase = 0.0;
        }
        o->held = synth_noise( o );
    }
    return v;
}

// -- instruments -------------------------------------------------------------------------

#define O( w, lv, ra, ce, pwid )    { (w), (lv), (ra), (ce), (pwid), 0.0f, false }
#define OP( w, lv, ra, ce, pwid )   { (w), (lv), (ra), (ce), (pwid), 0.0f, true }       // an oscillator behind the filter
#define NOOSC                       O( SYNTH_SINE, 0.0f, 1.0f, 0.0f, 0.5f )

// the factory instruments 4..19 (names and sounds are a first draft)
static const synth_inst_t synth_presets[SYNTH_NPRESET] = {
    // name, nosc, osc[3], gain, cutmul, res, fenv, fdecay, pitch, pitchtime, env, a, d, s, r, vibrate, vibdepth
    { "BASS", 2, { O( SYNTH_SAW, 0.8f, 1.0f, 0.0f, 0.5f ), O( SYNTH_SQUARE, 0.6f, 0.5f, 0.0f, 0.5f ), NOOSC },
      0.76f, 3.0f, 0.30f, 2.0f, 0.15f, 0.0f, 0.0f, true, 0.005f, 0.20f, 0.60f, 0.10f, 0.0f, 0.0f },
    { "LEAD", 2, { O( SYNTH_SQUARE, 0.6f, 1.0f, 0.0f, 0.25f ), O( SYNTH_SAW, 0.4f, 1.0f, 7.0f, 0.5f ), NOOSC },
      0.83f, 6.0f, 0.10f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0.01f, 0.10f, 0.70f, 0.15f, 5.5f, 0.15f },
    { "PAD", 3, { O( SYNTH_SAW, 0.33f, 1.0f, -9.0f, 0.5f ), O( SYNTH_SAW, 0.33f, 1.0f, 0.0f, 0.5f ), O( SYNTH_SAW, 0.33f, 1.0f, 9.0f, 0.5f ) },
      2.50f, 3.0f, 0.10f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0.50f, 0.30f, 0.80f, 0.80f, 0.0f, 0.0f },
    { "PLUCK", 2, { O( SYNTH_SAW, 0.7f, 1.0f, 0.0f, 0.5f ), O( SYNTH_SQUARE, 0.4f, 1.0f, 5.0f, 0.5f ), NOOSC },
      2.86f, 8.0f, 0.20f, 2.5f, 0.12f, 0.0f, 0.0f, true, 0.0f, 0.25f, 0.0f, 0.10f, 0.0f, 0.0f },
    { "ORGAN", 3, { O( SYNTH_SINE, 0.5f, 1.0f, 0.0f, 0.5f ), O( SYNTH_SINE, 0.3f, 2.0f, 0.0f, 0.5f ), O( SYNTH_SINE, 0.2f, 3.0f, 0.0f, 0.5f ) },
      0.79f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0.01f, 0.0f, 1.0f, 0.05f, 0.0f, 0.0f },
    { "STRINGS", 3, { O( SYNTH_SAW, 0.33f, 1.0f, -12.0f, 0.5f ), O( SYNTH_SAW, 0.33f, 1.0f, 0.0f, 0.5f ), O( SYNTH_SAW, 0.33f, 1.0f, 12.0f, 0.5f ) },
      2.03f, 4.0f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0.35f, 0.20f, 0.85f, 0.40f, 5.0f, 0.10f },
    { "BRASS", 2, { O( SYNTH_SAW, 0.5f, 1.0f, -4.0f, 0.5f ), O( SYNTH_SAW, 0.5f, 1.0f, 4.0f, 0.5f ), NOOSC },
      1.33f, 2.0f, 0.10f, 2.5f, 0.25f, 0.0f, 0.0f, true, 0.06f, 0.15f, 0.80f, 0.10f, 0.0f, 0.0f },
    { "FLUTE", 2, { O( SYNTH_SINE, 0.9f, 1.0f, 0.0f, 0.5f ), OP( SYNTH_NOISE, 0.08f, -5000.0f, 0.0f, 0.5f ), NOOSC },
      0.67f, 4.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0.08f, 0.05f, 0.90f, 0.15f, 5.0f, 0.20f },
    { "BELL", 3, { O( SYNTH_SINE, 0.5f, 1.0f, 0.0f, 0.5f ), O( SYNTH_SINE, 0.3f, 2.76f, 0.0f, 0.5f ), O( SYNTH_SINE, 0.2f, 5.4f, 0.0f, 0.5f ) },
      1.77f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0.0f, 1.50f, 0.0f, 1.00f, 0.0f, 0.0f },
    { "KICK", 1, { O( SYNTH_SINE, 1.0f, 1.0f, 0.0f, 0.5f ), NOOSC, NOOSC },
      0.84f, 0.0f, 0.0f, 0.0f, 0.0f, 24.0f, 0.06f, true, 0.0f, 0.25f, 0.0f, 0.02f, 0.0f, 0.0f },
    { "SNARE", 2, { O( SYNTH_NOISE, 0.8f, 0.0f, 0.0f, 0.5f ), O( SYNTH_TRIANGLE, 0.4f, 1.0f, 0.0f, 0.5f ), NOOSC },
      1.22f, 0.0f, 0.0f, 0.0f, 0.0f, 6.0f, 0.03f, true, 0.0f, 0.18f, 0.0f, 0.02f, 0.0f, 0.0f },
    { "HIHAT", 1, { O( SYNTH_NOISE, 1.0f, 0.0f, 0.0f, 0.5f ), NOOSC, NOOSC },
      1.09f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0.0f, 0.05f, 0.0f, 0.01f, 0.0f, 0.0f },
    { "TOM", 1, { O( SYNTH_SINE, 1.0f, 1.0f, 0.0f, 0.5f ), NOOSC, NOOSC },
      0.84f, 0.0f, 0.0f, 0.0f, 0.0f, 12.0f, 0.10f, true, 0.0f, 0.35f, 0.0f, 0.02f, 0.0f, 0.0f },
    { "CLAP", 1, { O( SYNTH_NOISE, 1.0f, 0.0f, 0.0f, 0.5f ), NOOSC, NOOSC },
      3.42f, 8.0f, 0.50f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0.0f, 0.12f, 0.0f, 0.02f, 0.0f, 0.0f },
    { "ZAP", 2, { O( SYNTH_SAW, 0.7f, 1.0f, 0.0f, 0.5f ), O( SYNTH_SQUARE, 0.4f, 1.0f, 0.0f, 0.3f ), NOOSC },
      2.44f, 6.0f, 0.40f, 1.5f, 0.10f, 36.0f, 0.10f, true, 0.0f, 0.20f, 0.0f, 0.05f, 0.0f, 0.0f },
    { "EPIANO", 2, { O( SYNTH_SINE, 0.7f, 1.0f, 0.0f, 0.5f ), O( SYNTH_SINE, 0.25f, 14.0f, 0.0f, 0.5f ), NOOSC },
      1.42f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0.0f, 0.80f, 0.15f, 0.30f, 0.0f, 0.0f }
};

void synth_instdefault( synth_inst_t* d ) {
    memset( d, 0, sizeof(*d) );
    d->nosc = 1;
    for ( int i=0; i < SYNTH_INST_NOSC; ++i ) {
        d->osc[i].wave = SYNTH_SINE; d->osc[i].ratio = 1.0f; d->osc[i].pw = 0.5f; d->osc[i].mul = 1.0f;
        d->osc[i].level = i == 0 ? 1.0f : 0.0f;
    }
    d->gain = 1.0f;
}

static void synth_inst_derive( synth_inst_t* d ) {
    for ( int i=0; i < SYNTH_INST_NOSC; ++i ) {
        d->osc[i].mul = d->osc[i].ratio * (float) pow( 2.0, (double) d->osc[i].cents / 1200.0 );
    }
}

bool synth_instvalid( const synth_inst_t* d ) {
    #define RNG( v, lo, hi )    ( (v) >= (lo) && (v) <= (hi) )
    if ( d->nosc < 1 || d->nosc > SYNTH_INST_NOSC ) return false;
    for ( int i=0; i < d->nosc; ++i ) {
        const synth_iosc_t* o = &d->osc[i];
        if ( (int) o->wave < 0 || (int) o->wave >= SYNTH_NOSCWAVES ) return false;
        if ( !RNG( o->level, 0.0f, 2.0f ) || !RNG( o->cents, -2400.0f, 2400.0f ) || !RNG( o->pw, 0.02f, 0.98f ) ) return false;
        if ( !( o->ratio > 0.0f && o->ratio <= 64.0f ) && !( o->ratio >= -20000.0f && o->ratio <= -1.0f ) && !( o->wave == SYNTH_NOISE && o->ratio == 0.0f ) ) return false;
    }
    return RNG( d->gain, 0.0f, 4.0f ) && RNG( d->cutmul, 0.0f, 64.0f ) && RNG( d->res, 0.0f, 0.95f ) &&
        RNG( d->fenv, -6.0f, 6.0f ) && RNG( d->fdecay, 0.0f, 10.0f ) && RNG( d->pitch, -48.0f, 48.0f ) &&
        RNG( d->pitchtime, 0.0f, 10.0f ) && RNG( d->a, 0.0f, 60.0f ) && RNG( d->d, 0.0f, 60.0f ) &&
        RNG( d->s, 0.0f, 1.0f ) && RNG( d->r, 0.0f, 60.0f ) && RNG( d->vibrate, 0.0f, 100.0f ) && RNG( d->vibdepth, 0.0f, 24.0f );
    #undef RNG
}

void synth_instpreset( int n, synth_inst_t* d ) {
    if ( n >= SYNTH_NBASIC && n < SYNTH_NBASIC + SYNTH_NPRESET ) {
        *d = synth_presets[n - SYNTH_NBASIC];
        synth_inst_derive( d );
    } else {
        synth_instdefault( d );
    }
}

bool synth_setinst( synth_t* s, int n, const synth_inst_t* def ) {
    if ( n < SYNTH_NBASIC || n >= SYNTH_NINST || !synth_instvalid( def ) ) {
        return false;
    }
    synth_inst_t* d = &s->inst[n];
    *d = *def;
    d->name[SYNTH_INST_NAMELEN-1] = '\0';
    synth_inst_derive( d );
    return true;
}

const synth_inst_t* synth_getinst( const synth_t* s, int n ) {
    return n >= SYNTH_NBASIC && n < SYNTH_NINST ? &s->inst[n] : 0;
}

const char* synth_instname( const synth_t* s, int n ) {
    static const char* const basic[SYNTH_NBASIC] = { "SINE", "SQUARE", "TRIANGLE", "NOISE" };
    if ( n < 0 || n >= SYNTH_NINST ) return "";
    if ( n < SYNTH_NBASIC ) return basic[n];
    return s->inst[n].name[0] != '\0' ? s->inst[n].name : "USER";
}

// band-limited edge (polyBLEP): t = phase 0..1, dt = phase step
static float synth_blep( double t, double dt ) {
    if ( t < dt ) {
        t /= dt;
        return (float)( t + t - t * t - 1.0 );
    }
    if ( t > 1.0 - dt ) {
        t = ( t - 1.0 ) / dt;
        return (float)( t * t + t + t + 1.0 );
    }
    return 0.0f;
}

// soft limiter of the instrument path: untouched up to 0.8, then compressed smoothly towards 1.0 (never above), so that an instrument with a
// high gain (drive) is louder on average but its peaks stay within the volume asked for
static float synth_softlimit( float x ) {
    float a = fabsf( x );
    if ( a <= 0.8f ) {
        return x;
    }
    float r = 0.8f + 0.2f * tanhf( ( a - 0.8f ) * 5.0f );
    return x < 0.0f ? -r : r;
}

// one sample of an instrument oscillator (-1 .. 1), then advances its phase; step = the note's phase step
static float synth_iosc_next( synth_osc_t* o, const synth_iosc_t* d, double step, double invrate ) {
    double dt = d->mul < 0.0f ? -(double) d->mul * invrate : step * (double) d->mul;      // a negative factor is a fixed frequency in Hz
    if ( dt > 0.45 ) dt = 0.45;
    float v;
    switch ( d->wave ) {
        case SYNTH_SQUARE: {
            double t2 = o->phase - (double) d->pw;
            if ( t2 < 0.0 ) t2 += 1.0;
            v = ( o->phase < (double) d->pw ? 1.0f : -1.0f ) + synth_blep( o->phase, dt ) - synth_blep( t2, dt );
            break;
        }
        case SYNTH_SAW:
            v = (float)( 2.0 * o->phase - 1.0 ) - synth_blep( o->phase, dt );
            break;
        case SYNTH_TRIANGLE:
            v = (float)( o->phase < 0.5 ? 4.0 * o->phase - 1.0 : 3.0 - 4.0 * o->phase );
            break;
        case SYNTH_NOISE:
            if ( d->mul == 0.0f ) {         // white noise: a new value every sample (a negative factor is a fixed rate)
                return synth_noise( o );
            }
            v = o->held;
            break;
        case SYNTH_SINE:
        default:
            v = (float) sin( TWOPI * o->phase );
            break;
    }
    o->phase += dt;
    if ( o->phase >= 1.0 ) {
        o->phase -= 1.0;
        if ( o->phase >= 1.0 ) o->phase = 0.0;
        o->held = synth_noise( o );
    }
    return v;
}

// -- the synth -------------------------------------------------------------------------

void synth_init( synth_t* s, int rate ) {
    memset( s, 0, sizeof(*s) );
    s->rate = rate;
    s->slewstep = 1.0f / ( SYNTH_SLEW_SEC * (float) rate );
    s->bpm = SYNTH_DEFAULT_BPM;
    for ( int i=0; i < SYNTH_NCHAN; ++i ) {
        synth_osc_init( &s->chan[i].osc, 0x9E3779B9U * (uint32_t)( i + 1 ) );
        s->chan[i].osc.held = synth_noise( &s->chan[i].osc );
        s->chan[i].env = 1.0f;
        for ( int k=0; k < SYNTH_INST_NOSC; ++k ) {
            synth_osc_init( &s->chan[i].iosc[k], 0x85EBCA6BU * (uint32_t)( i * SYNTH_INST_NOSC + k + 1 ) );
            s->chan[i].iosc[k].held = synth_noise( &s->chan[i].iosc[k] );
        }
    }
    for ( int n=SYNTH_NBASIC; n < SYNTH_NINST; ++n ) {
        synth_instpreset( n, &s->inst[n] );
    }
}

static void synth_ev_free( synth_event_t* ev ) {
    free( ev->def );
    free( ev );
}

static void synth_dropevents( synth_t* s ) {
    while ( s->events != 0 ) {
        synth_event_t* ev = s->events;
        s->events = ev->next;
        synth_ev_free( ev );
    }
    s->tail = 0;
    s->nevents = 0;
}

void synth_cleanup( synth_t* s ) {
    synth_dropevents( s );
}

void synth_setbpm( synth_t* s, int bpm ) {
    if ( bpm >= 1 && bpm <= 1000 ) {
        s->bpm = bpm;
    }
}

uint64_t synth_slot( synth_t* s, int chan, float beats, uint64_t now ) {
    if ( chan < 0 || chan >= SYNTH_NCHAN ) {
        return ~UINT64_C(0);
    }
    if ( s->cursor[chan] < now ) {
        s->cursor[chan] = now;
    }
    uint64_t start = s->cursor[chan];
    s->cursor[chan] += synth_beatsnsec( s, beats );
    return start;
}

void synth_sync( synth_t* s, unsigned chanmask, uint64_t now ) {
    uint64_t latest = now;
    for ( int i=0; i < SYNTH_NCHAN; ++i ) {
        if ( ( chanmask & ( 1U << i ) ) != 0 && s->cursor[i] > latest ) {
            latest = s->cursor[i];
        }
    }
    for ( int i=0; i < SYNTH_NCHAN; ++i ) {
        if ( ( chanmask & ( 1U << i ) ) != 0 ) {
            s->cursor[i] = latest;
        }
    }
}

bool synth_schedule_ev( synth_t* s, uint64_t when, const synth_evspec_t* sp ) {
    if ( sp->chan < 0 || sp->chan >= SYNTH_NCHAN || s->nevents >= SYNTH_MAXEVENTS ) {
        return false;
    }
    switch ( sp->type ) {
        case SYNTH_EV_PLAY:
            if ( !( sp->freq >= SYNTH_FREQ_MIN && sp->freq <= SYNTH_FREQ_MAX ) || !( sp->vol >= 0.0f && sp->vol <= 1.0f ) ) {
                return false;
            }
            if ( sp->freq2 != 0.0f && ( !( sp->freq2 >= SYNTH_FREQ_MIN && sp->freq2 <= SYNTH_FREQ_MAX ) || sp->dur == 0 ) ) {
                return false;
            }
            break;
        case SYNTH_EV_WAVE:
            if ( (int) sp->wave < 0 || (int) sp->wave >= SYNTH_NINST ) {      // 0..3 a basic wave, 4..31 an instrument
                return false;
            }
            break;
        case SYNTH_EV_STOP:
            break;
        case SYNTH_EV_ENV:
            if ( !( sp->p[0] < 0.0f ) && !( sp->p[0] >= 0.0f && sp->p[0] <= 60.0f && sp->p[1] >= 0.0f && sp->p[1] <= 60.0f &&
                    sp->p[2] >= 0.0f && sp->p[2] <= 1.0f && sp->p[3] >= 0.0f && sp->p[3] <= 60.0f ) ) {
                return false;
            }
            break;
        case SYNTH_EV_INST:
            if ( !( sp->p[0] >= 0.0f && sp->p[0] < (float) SYNTH_NINST ) ) {
                return false;
            }
            break;
        case SYNTH_EV_INSTDEF:
            if ( !( sp->p[0] >= (float) SYNTH_NBASIC && sp->p[0] < (float) SYNTH_NINST ) || sp->def == 0 || !synth_instvalid( sp->def ) ) {
                return false;
            }
            break;
        case SYNTH_EV_VIB:
            if ( !( sp->p[0] >= 0.0f && sp->p[0] <= 100.0f && sp->p[1] >= 0.0f && sp->p[1] <= 24.0f ) ) {
                return false;
            }
            break;
        case SYNTH_EV_TREM:
            if ( !( sp->p[0] >= 0.0f && sp->p[0] <= 100.0f && sp->p[1] >= 0.0f && sp->p[1] <= 1.0f ) ) {
                return false;
            }
            break;
        default:
            return false;
    }
    synth_event_t* ev = (synth_event_t*) malloc( sizeof(*ev) );
    if ( ev == 0 ) {
        return false;
    }
    ev->next = 0; ev->when = when; ev->type = sp->type; ev->chan = sp->chan; ev->freq = sp->freq; ev->vol = sp->vol;
    ev->wave = sp->wave; ev->freq2 = sp->freq2; ev->dur = sp->dur; memcpy( ev->p, sp->p, sizeof(ev->p) );
    ev->def = 0;
    if ( sp->type == SYNTH_EV_INSTDEF ) {
        ev->def = (synth_inst_t*) malloc( sizeof(*ev->def) );
        if ( ev->def == 0 ) {
            free( ev );
            return false;
        }
        *ev->def = *sp->def;
        ev->def->name[SYNTH_INST_NAMELEN-1] = '\0';
        synth_inst_derive( ev->def );
    }
    if ( s->tail == 0 || s->tail->when <= when ) {     // the usual case: a sequence is queued in time order
        if ( s->tail == 0 ) s->events = ev; else s->tail->next = ev;
        s->tail = ev;
    } else {
        synth_event_t** pp = &s->events;
        while ( *pp != 0 && (*pp)->when <= when ) {     // after the events of the same time: they stay in the order queued
            pp = &(*pp)->next;
        }
        ev->next = *pp; *pp = ev;       // not the last one (the tail is later)
    }
    ++s->nevents;
    return true;
}

bool synth_schedule( synth_t* s, uint64_t when, synth_evtype_t type, int chan, float freq, float vol, synth_wave_t wave ) {
    synth_evspec_t sp;
    memset( &sp, 0, sizeof(sp) );
    sp.type = type; sp.chan = chan; sp.freq = freq; sp.vol = vol; sp.wave = wave;
    return synth_schedule_ev( s, when, &sp );
}

static void synth_release( synth_t* s, synth_chan_t* c ) {      // the note ends: the channel fades out (envelope: its release)
    if ( c->env_on && c->on ) {
        if ( c->envstate != SYNTH_ENV_RELEASE ) {
            c->envstate = SYNTH_ENV_RELEASE;
            c->env_rinc = c->env_r > 0.0f ? c->env / ( c->env_r * (float) s->rate ) : 2.0f;
        }
    } else {
        c->on = false;
        c->target = 0.0f;
    }
}

// sets the effective envelope of the channel: on = false switches it off (a release in progress is cut short)
static void synth_env_config( synth_t* s, synth_chan_t* c, bool on, float a, float d, float sus, float r ) {
    if ( !on ) {
        if ( c->env_on && c->on && c->envstate == SYNTH_ENV_RELEASE ) {     // the release is cut short
            c->on = false; c->target = 0.0f;
        }
        c->env_on = false; c->env = 1.0f; c->envstate = SYNTH_ENV_IDLE;
        return;
    }
    bool wasenv = c->env_on;
    c->env_on = true;
    c->env_a = a; c->env_d = d; c->env_s = sus; c->env_r = r;
    float rate = (float) s->rate;
    c->env_ainc = c->env_a > 0.0f ? 1.0f / ( c->env_a * rate ) : 2.0f;
    c->env_dinc = c->env_d > 0.0f ? ( 1.0f - c->env_s ) / ( c->env_d * rate ) : 2.0f;
    if ( !wasenv ) {        // a note that is sounding already stays at its level
        c->env = 1.0f;
        c->envstate = c->on ? SYNTH_ENV_SUSTAIN : SYNTH_ENV_IDLE;
    }
}

// the envelope of the program (ENVELOPE) wins over the one of the instrument
static void synth_env_resolve( synth_t* s, synth_chan_t* c ) {
    if ( c->uenv_on ) {
        synth_env_config( s, c, true, c->uenv[0], c->uenv[1], c->uenv[2], c->uenv[3] );
    } else if ( c->inst >= SYNTH_NBASIC && c->cinst.env ) {
        const synth_inst_t* d = &c->cinst;
        synth_env_config( s, c, true, d->a, d->d, d->s, d->r );
    } else {
        synth_env_config( s, c, false, 0.0f, 0.0f, 0.0f, 0.0f );
    }
}

// the channel uses instrument n: a basic wave, or (4..31) a copy of def (0 = the synth's table entry)
static void synth_select( synth_t* s, synth_chan_t* c, int n, const synth_inst_t* def ) {
    if ( n < SYNTH_NBASIC ) {
        c->osc.wave = (synth_wave_t) n;
    } else {
        c->cinst = def != 0 ? *def : s->inst[n];
    }
    c->inst = n;
    synth_env_resolve( s, c );
}

static void synth_apply( synth_t* s, const synth_event_t* ev ) {
    synth_chan_t* c = &s->chan[ev->chan];
    switch ( ev->type ) {
        case SYNTH_EV_PLAY: {
            bool wason = c->on;
            if ( !wason && c->gain == 0.0f ) {
                c->osc.phase = 0.0;             // a new note starts at the beginning of the cycle; a sounding one goes on
                c->vib_phase = 0.0; c->trem_phase = 0.0;
            }
            if ( c->inst >= SYNTH_NBASIC ) {
                const synth_inst_t* in = &c->cinst;
                bool retrig = !wason || ( c->env_on && ( c->envstate == SYNTH_ENV_RELEASE || c->envstate == SYNTH_ENV_IDLE ) );
                if ( !wason && c->gain == 0.0f ) {
                    for ( int k=0; k < SYNTH_INST_NOSC; ++k ) c->iosc[k].phase = 0.0;
                }
                if ( retrig ) {
                    c->pe = (double) in->pitch;
                    c->pdec = in->pitchtime > 0.0f ? exp( -1.0 / ( (double) in->pitchtime * (double) s->rate ) ) : 0.0;
                    c->fenvlvl = 1.0f;
                    c->fcount = 0;
                    if ( !wason ) { c->fic1 = 0.0; c->fic2 = 0.0; }
                }
            }
            c->basestep = (double) ev->freq / (double) s->rate;
            c->osc.step = c->basestep;
            c->glide_left = 0;
            if ( ev->freq2 > 0.0f && ev->dur > 0 ) {
                double samples = floor( (double) ev->dur * (double) s->rate / 1.0e9 );
                if ( samples < 1.0 ) samples = 1.0;
                c->glide_ratio = pow( (double) ev->freq2 / (double) ev->freq, 1.0 / samples );
                c->glide_left = (int) samples;
                c->glide_end = (double) ev->freq2 / (double) s->rate;
            }
            c->vol = ev->vol;
            c->target = ev->vol;
            c->on = true;
            if ( c->env_on && ( !wason || c->envstate == SYNTH_ENV_RELEASE || c->envstate == SYNTH_ENV_IDLE ) ) {
                if ( !wason ) c->env = 0.0f;
                c->envstate = SYNTH_ENV_ATTACK;
            }
            break;
        }
        case SYNTH_EV_STOP:
            synth_release( s, c );
            break;
        case SYNTH_EV_WAVE:
            synth_select( s, c, (int) ev->wave, 0 );
            break;
        case SYNTH_EV_INST:
            synth_select( s, c, (int) ev->p[0], 0 );
            break;
        case SYNTH_EV_INSTDEF:
            synth_select( s, c, (int) ev->p[0], ev->def );
            break;
        case SYNTH_EV_ENV:
            if ( ev->p[0] < 0.0f ) {
                c->uenv_on = false;
            } else {
                c->uenv_on = true;
                for ( int k=0; k < 4; ++k ) c->uenv[k] = ev->p[k];
            }
            synth_env_resolve( s, c );
            break;
        case SYNTH_EV_VIB:
            c->vib_rate = ev->p[0]; c->vib_depth = ev->p[1];
            break;
        case SYNTH_EV_TREM:
            c->trem_rate = ev->p[0]; c->trem_depth = ev->p[1];
            break;
    }
}

void synth_setpitch( synth_t* s, int chan, float freq ) {
    if ( chan < 0 || chan >= SYNTH_NCHAN || !( freq >= SYNTH_FREQ_MIN && freq <= SYNTH_FREQ_MAX ) ) {
        return;
    }
    synth_chan_t* c = &s->chan[chan];
    c->basestep = (double) freq / (double) s->rate;
    c->osc.step = c->basestep;
    c->glide_left = 0;
}

void synth_setvolume( synth_t* s, int chan, float vol ) {
    if ( chan < 0 || chan >= SYNTH_NCHAN || !( vol >= 0.0f && vol <= 1.0f ) ) {
        return;
    }
    synth_chan_t* c = &s->chan[chan];
    c->vol = vol;
    if ( c->on ) {
        c->target = vol;
    }
}

int synth_runevents( synth_t* s, uint64_t now ) {
    int n = 0;
    while ( s->events != 0 && s->events->when <= now ) {
        synth_event_t* ev = s->events;
        s->events = ev->next;
        if ( s->events == 0 ) s->tail = 0;
        --s->nevents;
        synth_apply( s, ev );
        synth_ev_free( ev );
        ++n;
    }
    return n;
}

bool synth_nextevent( const synth_t* s, uint64_t* when ) {
    if ( s->events == 0 ) {
        return false;
    }
    *when = s->events->when;
    return true;
}

bool synth_busy( const synth_t* s, int chan, uint64_t now ) {
    if ( chan < 0 || chan >= SYNTH_NCHAN ) {
        return false;
    }
    if ( s->chan[chan].on || s->chan[chan].target != 0.0f || s->chan[chan].gain != 0.0f || s->cursor[chan] > now ) {
        return true;
    }
    for ( const synth_event_t* ev = s->events; ev != 0; ev = ev->next ) {
        if ( ev->chan == chan ) {
            return true;
        }
    }
    return false;
}

void synth_silencechan( synth_t* s, int chan, uint64_t now ) {
    if ( chan < 0 || chan >= SYNTH_NCHAN ) {
        return;
    }
    synth_event_t** pp = &s->events;
    s->tail = 0;
    while ( *pp != 0 ) {
        synth_event_t* ev = *pp;
        if ( ev->chan == chan ) {
            *pp = ev->next;
            synth_ev_free( ev );
            --s->nevents;
        } else {
            s->tail = ev;
            pp = &ev->next;
        }
    }
    s->chan[chan].on = false;
    s->chan[chan].target = 0.0f;
    s->chan[chan].glide_left = 0;
    s->chan[chan].envstate = SYNTH_ENV_IDLE;
    s->cursor[chan] = now;
}

void synth_silence( synth_t* s, uint64_t now ) {
    synth_dropevents( s );
    for ( int i=0; i < SYNTH_NCHAN; ++i ) {
        s->chan[i].on = false;
        s->chan[i].target = 0.0f;
        s->chan[i].glide_left = 0;
        s->chan[i].envstate = SYNTH_ENV_IDLE;
        s->cursor[i] = now;
    }
}

// the envelope level for the next sample (only called with env_on)
static float synth_env_next( synth_chan_t* c ) {
    switch ( c->envstate ) {
        case SYNTH_ENV_ATTACK:
            c->env += c->env_ainc;
            if ( c->env >= 1.0f ) {
                c->env = 1.0f; c->envstate = SYNTH_ENV_DECAY;
            }
            break;
        case SYNTH_ENV_DECAY:
            c->env -= c->env_dinc;
            if ( c->env <= c->env_s ) {
                c->env = c->env_s; c->envstate = SYNTH_ENV_SUSTAIN;
            }
            break;
        case SYNTH_ENV_RELEASE:
            c->env -= c->env_rinc;
            if ( c->env <= 0.0f ) {         // the note is over
                c->env = 0.0f; c->envstate = SYNTH_ENV_IDLE;
                c->on = false; c->target = 0.0f;
            }
            break;
        case SYNTH_ENV_SUSTAIN:
        case SYNTH_ENV_IDLE:
        default:
            break;
    }
    return c->env;
}

void synth_render( synth_t* s, float* buf, int count ) {
    const double invrate = 1.0 / (double) s->rate;
    for ( int j=0; j < count; ++j ) {
        float sum = 0.0f, sumgain = 0.0f;
        for ( int i=0; i < SYNTH_NCHAN; ++i ) {
            synth_chan_t* c = &s->chan[i];
            if ( !c->on && c->gain == 0.0f && c->target == 0.0f ) {
                continue;
            }
            if ( c->gain < c->target ) {
                c->gain += s->slewstep;
                if ( c->gain > c->target ) c->gain = c->target;
            } else if ( c->gain > c->target ) {
                c->gain -= s->slewstep;
                if ( c->gain < c->target ) c->gain = c->target;
            }
            float level = c->gain;
            if ( c->env_on ) {
                level *= synth_env_next( c );
            }
            if ( c->glide_left > 0 ) {
                c->basestep *= c->glide_ratio;
                if ( --c->glide_left == 0 ) {
                    c->basestep = c->glide_end;
                }
            }
            const synth_inst_t* in = c->inst >= SYNTH_NBASIC ? &c->cinst : 0;
            float vdepth = c->vib_depth, vrate = c->vib_rate;
            if ( vdepth <= 0.0f && in != 0 ) {      // the instrument's own vibrato, if the program set none
                vdepth = in->vibdepth; vrate = in->vibrate;
            }
            double step = c->basestep;
            if ( vdepth > 0.0f ) {
                step = c->basestep * exp2( (double)( vdepth / 12.0f ) * sin( TWOPI * c->vib_phase ) );
                c->vib_phase += (double) vrate / (double) s->rate;
                if ( c->vib_phase >= 1.0 ) c->vib_phase -= floor( c->vib_phase );
            }
            if ( c->trem_depth > 0.0f ) {
                level *= 1.0f - c->trem_depth * 0.5f * (float)( 1.0 - cos( TWOPI * c->trem_phase ) );
                c->trem_phase += (double) c->trem_rate / (double) s->rate;
                if ( c->trem_phase >= 1.0 ) c->trem_phase -= floor( c->trem_phase );
            }
            float v;
            if ( in == 0 ) {
                c->osc.step = step;
                v = synth_osc_next( &c->osc ) * level;
            } else {
                if ( c->pe != 0.0 ) {
                    step *= exp2( c->pe / 12.0 );
                    c->pe *= c->pdec;
                    if ( fabs( c->pe ) < 0.005 ) c->pe = 0.0;
                }
                float x = 0.0f, xpost = 0.0f;
                for ( int k=0; k < in->nosc; ++k ) {
                    float o = synth_iosc_next( &c->iosc[k], &in->osc[k], step, invrate ) * in->osc[k].level;
                    if ( in->osc[k].post ) xpost += o; else x += o;
                }
                if ( in->cutmul > 0.0f ) {
                    if ( --c->fcount <= 0 ) {       // the cutoff is computed every 32 samples
                        c->fcount = 32;
                        double hz = step * (double) s->rate * (double) in->cutmul * exp2( (double)( in->fenv * c->fenvlvl ) );
                        if ( hz < 20.0 ) hz = 20.0;
                        if ( hz > (double) s->rate * 0.45 ) hz = (double) s->rate * 0.45;
                        double g = tan( 3.14159265358979 * hz / (double) s->rate ), k = 2.0 * ( 1.0 - (double) in->res );
                        c->fa1 = 1.0 / ( 1.0 + g * ( g + k ) ); c->fa2 = g * c->fa1; c->fa3 = g * c->fa2;
                        c->fenvlvl = in->fdecay > 0.0f ? c->fenvlvl * (float) exp( -32.0 / ( (double) in->fdecay * (double) s->rate ) ) : 0.0f;
                    }
                    double v3 = (double) x - c->fic2;
                    double v1 = c->fa1 * c->fic1 + c->fa2 * v3;
                    double v2 = c->fic2 + c->fa2 * c->fic1 + c->fa3 * v3;
                    c->fic1 = 2.0 * v1 - c->fic1;
                    c->fic2 = 2.0 * v2 - c->fic2;
                    x = (float) v2;
                }
                v = synth_softlimit( ( x + xpost ) * in->gain ) * level;       // xpost: the oscillators behind the filter
            }
            sum += v;
            if ( fabsf( v ) > s->peak[i] ) s->peak[i] = fabsf( v );
            sumgain += c->gain;
        }
        buf[j] = sumgain > 1.0f ? sum / sumgain : sum;      // loud chords are scaled down, not clipped
        if ( fabsf( buf[j] ) > s->outpeak ) s->outpeak = fabsf( buf[j] );
    }
}
