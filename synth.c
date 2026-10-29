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

// -- the synth -------------------------------------------------------------------------

void synth_init( synth_t* s, int rate ) {
    memset( s, 0, sizeof(*s) );
    s->rate = rate;
    s->slewstep = 1.0f / ( SYNTH_SLEW_SEC * (float) rate );
    s->bpm = SYNTH_DEFAULT_BPM;
    for ( int i=0; i < SYNTH_NCHAN; ++i ) {
        synth_osc_init( &s->chan[i].osc, 0x9E3779B9U * (uint32_t)( i + 1 ) );
        s->chan[i].osc.held = synth_noise( &s->chan[i].osc );
    }
}

static void synth_dropevents( synth_t* s ) {
    while ( s->events != 0 ) {
        synth_event_t* ev = s->events;
        s->events = ev->next;
        free( ev );
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

bool synth_schedule( synth_t* s, uint64_t when, synth_evtype_t type, int chan, float freq, float vol, synth_wave_t wave ) {
    if ( chan < 0 || chan >= SYNTH_NCHAN || s->nevents >= SYNTH_MAXEVENTS ) {
        return false;
    }
    switch ( type ) {
        case SYNTH_EV_PLAY:
            if ( !( freq >= SYNTH_FREQ_MIN && freq <= SYNTH_FREQ_MAX ) || !( vol >= 0.0f && vol <= 1.0f ) ) {
                return false;
            }
            break;
        case SYNTH_EV_WAVE:
            if ( (int) wave < 0 || (int) wave >= SYNTH_NWAVES ) {
                return false;
            }
            break;
        case SYNTH_EV_STOP:
            break;
        default:
            return false;
    }
    synth_event_t* ev = (synth_event_t*) malloc( sizeof(*ev) );
    if ( ev == 0 ) {
        return false;
    }
    ev->next = 0; ev->when = when; ev->type = type; ev->chan = chan; ev->freq = freq; ev->vol = vol; ev->wave = wave;
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

static void synth_apply( synth_t* s, const synth_event_t* ev ) {
    synth_chan_t* c = &s->chan[ev->chan];
    switch ( ev->type ) {
        case SYNTH_EV_PLAY:
            if ( c->gain == 0.0f && c->target == 0.0f ) {
                c->osc.phase = 0.0;             // a new note starts at the beginning of the cycle; a sounding one goes on
            }
            c->osc.step = (double) ev->freq / (double) s->rate;
            c->target = ev->vol;
            break;
        case SYNTH_EV_STOP:
            c->target = 0.0f;
            break;
        case SYNTH_EV_WAVE:
            c->osc.wave = ev->wave;
            break;
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
        free( ev );
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
    if ( s->chan[chan].target != 0.0f || s->chan[chan].gain != 0.0f || s->cursor[chan] > now ) {
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
            free( ev );
            --s->nevents;
        } else {
            s->tail = ev;
            pp = &ev->next;
        }
    }
    s->chan[chan].target = 0.0f;
    s->cursor[chan] = now;
}

void synth_silence( synth_t* s, uint64_t now ) {
    synth_dropevents( s );
    for ( int i=0; i < SYNTH_NCHAN; ++i ) {
        s->chan[i].target = 0.0f;
        s->cursor[i] = now;
    }
}

void synth_render( synth_t* s, float* buf, int count ) {
    for ( int j=0; j < count; ++j ) {
        float sum = 0.0f, sumgain = 0.0f;
        for ( int i=0; i < SYNTH_NCHAN; ++i ) {
            synth_chan_t* c = &s->chan[i];
            if ( c->gain == 0.0f && c->target == 0.0f ) {
                continue;
            }
            if ( c->gain < c->target ) {
                c->gain += s->slewstep;
                if ( c->gain > c->target ) c->gain = c->target;
            } else if ( c->gain > c->target ) {
                c->gain -= s->slewstep;
                if ( c->gain < c->target ) c->gain = c->target;
            }
            sum += synth_osc_next( &c->osc ) * c->gain;
            sumgain += c->gain;
        }
        buf[j] = sumgain > 1.0f ? sum / sumgain : sum;      // loud chords are scaled down, not clipped
    }
}
