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

#ifndef SYNTH_H
#define SYNTH_H     1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
Software synthesizer core: oscillators, a mixer and a time-ordered event queue. It has no SDL dependency and no clock of
its own (all times are nanosecond time stamps handed in by the caller), so it can be tested without a sound device.
sdlaudio.c drives it: a worker thread applies the due events, the audio callback renders. The synth itself is not
thread safe; the caller serializes the calls.

Channels: SYNTH_NCHAN voices, each with a wave type, a frequency and a volume (0..1). Wave types are the basic ones
(sine, square, triangle, noise); the structure is meant to grow into instruments / FM operators later: the oscillator
(synth_osc_t) is separate from the channel (gain, envelope), so an instrument can use several oscillators per channel.
Volume changes are slewed over a few milliseconds, so starting and stopping a note doesn't click.
Effects, all computed per sample in synth_render() (so an effect costs one event, not one per step): a glide (the pitch of
a tone moves exponentially from one frequency to another during the tone), an ADSR envelope per channel (attack, decay, sustain
level, release; off by default: then a note just fades in and out within SYNTH_SLEW_SEC), vibrato (pitch LFO, depth in semitones)
and tremolo (volume LFO, depth 0..1). synth_setpitch() / synth_setvolume() change a sounding channel at once (no queue). Noise is
sample-and-hold white noise that changes with the channel's frequency (low frequency = coarse, high = fine).

Time: every channel has a cursor (the time its next event is due). synth_slot() hands out the start time of the next
event of a channel and advances the cursor by the duration in beats; that is how a sequence is queued in one go. A
cursor that lags behind the clock (the sequence has run out) is set to "now" first. synth_sync() lines up the cursors
of several channels (for chords). Events are applied in time order (stable for equal times), whatever order they were
queued in, so one channel's far-ahead events don't hold up another channel's.
*/

#define SYNTH_NCHAN         8
#define SYNTH_NOTES         72          // note 0 = A at 110 Hz, 12 notes per octave, up to ~6.6 kHz
#define SYNTH_FREQ_MIN      20.0f
#define SYNTH_FREQ_MAX      20000.0f
#define SYNTH_MAXEVENTS     262144
#define SYNTH_DEFAULT_BPM   60
#define SYNTH_SLEW_SEC      0.0025f     // time a channel needs to fade between silence and full volume

typedef enum _synth_wave_t {
    SYNTH_SINE,
    SYNTH_SQUARE,
    SYNTH_TRIANGLE,
    SYNTH_NOISE,
    SYNTH_NWAVES
} synth_wave_t;

typedef enum _synth_evtype_t {
    SYNTH_EV_PLAY,                      // start (or change) the tone: freq, vol
    SYNTH_EV_STOP,                      // fade the channel out
    SYNTH_EV_WAVE,                      // select the wave type (takes effect at once, also while sounding)
    SYNTH_EV_ENV,                       // envelope: p[0..3] = attack s, decay s, sustain level, release s; p[0] < 0 = off
    SYNTH_EV_VIB,                       // vibrato: p[0] = rate Hz, p[1] = depth in semitones (0 = off)
    SYNTH_EV_TREM                       // tremolo: p[0] = rate Hz, p[1] = depth 0..1 (0 = off)
} synth_evtype_t;

typedef enum _synth_envstate_t {
    SYNTH_ENV_IDLE, SYNTH_ENV_ATTACK, SYNTH_ENV_DECAY, SYNTH_ENV_SUSTAIN, SYNTH_ENV_RELEASE
} synth_envstate_t;

typedef struct _synth_osc_t {           // one oscillator (the building block of future instruments)
    synth_wave_t    wave;
    double          phase;              // 0 <= phase < 1
    double          step;               // phase increment per sample (freq / rate)
    uint32_t        rng;                // noise generator state
    float           held;               // noise: the current sample-and-hold value
} synth_osc_t;

typedef struct _synth_chan_t {
    synth_osc_t     osc;
    bool            on;                 // a note is on (with an envelope: until its release is over)
    float           vol;                // the note's volume
    float           target;             // volume the channel is fading to (0 = stopped)
    float           gain;               // volume right now
    double          basestep;           // phase step without vibrato (osc.step is the one in use)
    double          glide_ratio;        // glide: the step is multiplied by this every sample ...
    int             glide_left;         // ... for this many samples, ending at glide_end
    double          glide_end;
    bool            env_on;             // envelope: see SYNTH_EV_ENV
    float           env_a, env_d, env_s, env_r;
    synth_envstate_t envstate;
    float           env;                // envelope level 0..1
    float           env_ainc, env_dinc, env_rinc;   // level change per sample in the attack, decay and release
    float           vib_rate, vib_depth, trem_rate, trem_depth;
    double          vib_phase, trem_phase;
} synth_chan_t;

typedef struct _synth_event_t {
    struct _synth_event_t*  next;
    uint64_t                when;       // ns
    synth_evtype_t          type;
    int                     chan;
    float                   freq, vol;
    synth_wave_t            wave;
    float                   freq2;      // PLAY: the glide ends at this frequency (0 = no glide) after dur ns
    uint64_t                dur;
    float                   p[4];       // ENV, VIB, TREM
} synth_event_t;

typedef struct _synth_evspec_t {        // what synth_schedule_ev() queues (unused fields 0)
    synth_evtype_t          type;
    int                     chan;
    float                   freq, freq2, vol;
    synth_wave_t            wave;
    uint64_t                dur;
    float                   p[4];
} synth_evspec_t;

typedef struct _synth_t {
    int             rate;               // samples per second
    float           slewstep;           // gain change per sample
    int             bpm;
    synth_chan_t    chan[SYNTH_NCHAN];
    uint64_t        cursor[SYNTH_NCHAN];
    synth_event_t*  events;             // sorted by time
    synth_event_t*  tail;               // the last one (events queued in time order are appended without a search)
    int             nevents;
} synth_t;

void synth_init( synth_t* s, int rate );
void synth_cleanup( synth_t* s );                   // frees the queued events

float synth_notefreq( int note );                   // Hz, 0 if the note is out of range
void synth_setbpm( synth_t* s, int bpm );           // 1..1000, else ignored
uint64_t synth_beatsnsec( const synth_t* s, float beats );

// the start time of the channel's next event; the cursor then moves on by "beats". ~0 if the channel doesn't exist.
uint64_t synth_slot( synth_t* s, int chan, float beats, uint64_t now );
void synth_sync( synth_t* s, unsigned chanmask, uint64_t now );     // all cursors in the mask go to the latest of them

// queues an event; false if the arguments are invalid (channel, frequency 20..20000, volume 0..1, wave) or the queue is full
bool synth_schedule( synth_t* s, uint64_t when, synth_evtype_t type, int chan, float freq, float vol, synth_wave_t wave );
bool synth_schedule_ev( synth_t* s, uint64_t when, const synth_evspec_t* spec );    // the same with all fields (glide, effects)
// at once, no queue: the pitch of a channel (stops a glide) / the volume of its note (also 0: the note goes on silently)
void synth_setpitch( synth_t* s, int chan, float freq );
void synth_setvolume( synth_t* s, int chan, float vol );
int synth_runevents( synth_t* s, uint64_t now );    // applies the events due at "now"; returns how many
bool synth_nextevent( const synth_t* s, uint64_t* when );   // time of the earliest queued event; false if none
bool synth_busy( const synth_t* s, int chan, uint64_t now );   // the channel sounds, fades, or has events queued / a sequence still running
void synth_silencechan( synth_t* s, int chan, uint64_t now );   // the same for one channel
void synth_silence( synth_t* s, uint64_t now );     // drops all queued events, fades all channels out, resets the cursors

void synth_render( synth_t* s, float* buf, int count );     // mono, count samples

#endif
