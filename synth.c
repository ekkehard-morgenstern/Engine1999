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
        s->chan[i].env = 1.0f;
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
            if ( (int) sp->wave < 0 || (int) sp->wave >= SYNTH_NWAVES ) {
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

static void synth_apply( synth_t* s, const synth_event_t* ev ) {
    synth_chan_t* c = &s->chan[ev->chan];
    switch ( ev->type ) {
        case SYNTH_EV_PLAY: {
            bool wason = c->on;
            if ( !wason && c->gain == 0.0f ) {
                c->osc.phase = 0.0;             // a new note starts at the beginning of the cycle; a sounding one goes on
                c->vib_phase = 0.0; c->trem_phase = 0.0;
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
            c->osc.wave = ev->wave;
            break;
        case SYNTH_EV_ENV:
            if ( ev->p[0] < 0.0f ) {
                if ( c->env_on && c->on && c->envstate == SYNTH_ENV_RELEASE ) {     // the release is cut short
                    c->on = false; c->target = 0.0f;
                }
                c->env_on = false; c->env = 1.0f; c->envstate = SYNTH_ENV_IDLE;
            } else {
                bool wasenv = c->env_on;
                c->env_on = true;
                c->env_a = ev->p[0]; c->env_d = ev->p[1]; c->env_s = ev->p[2]; c->env_r = ev->p[3];
                float rate = (float) s->rate;
                c->env_ainc = c->env_a > 0.0f ? 1.0f / ( c->env_a * rate ) : 2.0f;
                c->env_dinc = c->env_d > 0.0f ? ( 1.0f - c->env_s ) / ( c->env_d * rate ) : 2.0f;
                if ( !wasenv ) {        // a note that is sounding already stays at its level
                    c->env = 1.0f;
                    c->envstate = c->on ? SYNTH_ENV_SUSTAIN : SYNTH_ENV_IDLE;
                }
            }
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
            free( ev );
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
            if ( c->vib_depth > 0.0f ) {
                c->osc.step = c->basestep * exp2( (double)( c->vib_depth / 12.0f ) * sin( TWOPI * c->vib_phase ) );
                c->vib_phase += (double) c->vib_rate / (double) s->rate;
                if ( c->vib_phase >= 1.0 ) c->vib_phase -= floor( c->vib_phase );
            } else {
                c->osc.step = c->basestep;
            }
            if ( c->trem_depth > 0.0f ) {
                level *= 1.0f - c->trem_depth * 0.5f * (float)( 1.0 - cos( TWOPI * c->trem_phase ) );
                c->trem_phase += (double) c->trem_rate / (double) s->rate;
                if ( c->trem_phase >= 1.0 ) c->trem_phase -= floor( c->trem_phase );
            }
            sum += synth_osc_next( &c->osc ) * level;
            sumgain += c->gain;
        }
        buf[j] = sumgain > 1.0f ? sum / sumgain : sum;      // loud chords are scaled down, not clipped
    }
}
