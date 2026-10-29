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
Volume changes are slewed over a few milliseconds, so starting and stopping a note doesn't click. Noise is
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
#define SYNTH_MAXEVENTS     16384
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
    SYNTH_EV_WAVE                       // select the wave type (takes effect at once, also while sounding)
} synth_evtype_t;

typedef struct _synth_osc_t {           // one oscillator (the building block of future instruments)
    synth_wave_t    wave;
    double          phase;              // 0 <= phase < 1
    double          step;               // phase increment per sample (freq / rate)
    uint32_t        rng;                // noise generator state
    float           held;               // noise: the current sample-and-hold value
} synth_osc_t;

typedef struct _synth_chan_t {
    synth_osc_t     osc;
    float           target;             // volume the channel is fading to (0 = stopped)
    float           gain;               // volume right now
} synth_chan_t;

typedef struct _synth_event_t {
    struct _synth_event_t*  next;
    uint64_t                when;       // ns
    synth_evtype_t          type;
    int                     chan;
    float                   freq, vol;
    synth_wave_t            wave;
} synth_event_t;

typedef struct _synth_t {
    int             rate;               // samples per second
    float           slewstep;           // gain change per sample
    int             bpm;
    synth_chan_t    chan[SYNTH_NCHAN];
    uint64_t        cursor[SYNTH_NCHAN];
    synth_event_t*  events;             // sorted by time
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
int synth_runevents( synth_t* s, uint64_t now );    // applies the events due at "now"; returns how many
bool synth_nextevent( const synth_t* s, uint64_t* when );   // time of the earliest queued event; false if none
void synth_silence( synth_t* s, uint64_t now );     // drops all queued events, fades all channels out, resets the cursors

void synth_render( synth_t* s, float* buf, int count );     // mono, count samples

#endif
