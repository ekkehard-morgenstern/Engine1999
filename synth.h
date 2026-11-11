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

    ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
        applies. Programs written in the built-in programming language, and
        executables made from them with BUILD, do not fall under this license
        as the exception describes (see the file LICENSE-EXCEPTION).

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

#define SYNTH_NVOICES       32          // extra voice slots for the polyphonic channels (an instrument with POLY n > 1): see synth.c
#define SYNTH_MAXPOLY       8           // voices of one channel (POLY)
#define SYNTH_NCHAN         16          // 1..8 music, 9..16 effects (the split is made by the VM: nodetype.h SND_NMUSIC)
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
    SYNTH_SAW,                          // only inside instruments (not selectable by SYNTH_EV_WAVE)
    SYNTH_NOSCWAVES
} synth_wave_t;
#define SYNTH_NWAVES        4           // the basic waves 0..3 = instruments 0..3

// -- instruments ------------------------------------------------------------------------
// Instruments 0..3 are the four basic waves (a plain oscillator, the original signal path, unchanged). Instruments 4..31 are
// "subtractive / layered": up to SYNTH_INST_NOSC band-limited oscillators (wave, level, frequency ratio, detune in cents, pulse
// width of the square wave), one resonant low-pass filter whose cutoff follows the note (cutoff = note frequency * cutmul, with
// an envelope that starts "fenv" octaves higher and decays), a pitch envelope (starts "pitch" semitones off and decays to the
// note), a default ADSR envelope (an ENVELOPE of the program overrides it) and a default vibrato (a VIBRATO overrides it).
// 4..19 are the presets of synth_init(); all of 4..31 can be redefined with synth_setinst().
#define SYNTH_NINST         32
#define SYNTH_NBASIC        4
#define SYNTH_NPRESET       16          // instruments 4 .. 4 + SYNTH_NPRESET - 1
#define SYNTH_INST_NOSC     3
#define SYNTH_INST_NAMELEN  12

typedef struct _synth_iosc_t {
    synth_wave_t    wave;               // SYNTH_SINE .. SYNTH_SAW
    float           level;              // 0..2
    float           ratio;              // frequency = note * ratio (> 0, up to 64); negative = a FIXED frequency of -ratio Hz (1..20000: it ignores the note, the
                                        // pitch envelope and the vibrato; for noise it is the rate of the sample-and-hold); noise: 0 = white noise at the sample rate
    float           cents;              // detune -2400..2400
    float           pw;                 // pulse width of the square wave 0.02..0.98 (0.5 = square)
    float           mul;                // derived (ratio * 2^(cents/1200), negative for a fixed frequency); set by synth_setinst()
    bool            post;               // the oscillator bypasses the low-pass filter (it is mixed in behind it): breath noise, a chiff, a fixed hiss
    float           fm;                 // FM: the oscillator's phase is modulated by the NEXT oscillator (index + 1) with this index in radians (0 = none, 0..20)
    bool            mod;                // a modulator: the oscillator is only heard through the oscillator it modulates (it is not mixed)
} synth_iosc_t;

typedef struct _synth_inst_t {
    char            name[SYNTH_INST_NAMELEN];
    int             nosc;               // 1..SYNTH_INST_NOSC
    synth_iosc_t    osc[SYNTH_INST_NOSC];
    float           gain;               // 0..4, the drive into a soft limiter (untouched up to 0.8, never above 1.0): louder on average, peaks within the volume
    float           cutmul;             // low-pass cutoff = note frequency * cutmul (0 = no filter), 0..64
    float           res;                // resonance 0..0.95
    float           fenv, fdecay;       // filter envelope: starts fenv octaves (-6..6) above the cutoff, decays with time constant fdecay s (0..10)
    float           pitch, pitchtime;   // pitch envelope: starts at "pitch" semitones (-48..48) off, decays with time constant pitchtime s (0..10)
    bool            env;                // a default ADSR envelope ...
    float           a, d, s, r;         // ... attack / decay / release seconds 0..60, sustain 0..1
    float           vibrate, vibdepth;  // default vibrato: rate 0..100 Hz, depth 0..24 semitones (0 = none)
    float           fmdecay;            // the FM indexes fall exponentially with this time constant (s, 0..10; 0 = they stay)
    int             poly;               // voices: 0 or 1 = one note at a time on a channel using the instrument (the channel itself sounds), 2..8 = every note of the channel
                                        // gets its own voice from the pool (SYNTH_NVOICES slots), so chords and the release tails of earlier notes sound together
} synth_inst_t;

typedef enum _synth_evtype_t {
    SYNTH_EV_PLAY,                      // start (or change) the tone: freq, vol
    SYNTH_EV_STOP,                      // fade the channel out
    SYNTH_EV_WAVE,                      // select the wave type 0..3, or the instrument 4..31 (takes effect at once, also while sounding)
    SYNTH_EV_ENV,                       // envelope: p[0..3] = attack s, decay s, sustain level, release s; p[0] < 0 = off
    SYNTH_EV_VIB,                       // vibrato: p[0] = rate Hz, p[1] = depth in semitones (0 = off)
    SYNTH_EV_TREM,                      // tremolo: p[0] = rate Hz, p[1] = depth 0..1 (0 = off)
    SYNTH_EV_INST,                      // select the instrument p[0] = 0 .. SYNTH_NINST-1 (0..3: the basic waves; 4..31: the channel takes a copy of the
                                        // synth's table entry n at that moment)
    SYNTH_EV_CUT,                       // filter cutoff of the channel's instrument: p[0] = multiple of the note 0.01..64 (-1 = the instrument's own), p[1] = seconds to glide there (0..60)
    SYNTH_EV_INSTDEF                    // select the instrument p[0] = 4..31 with the definition in "def" (a snapshot: the channel keeps its own copy,
                                        // so a later definition of the same number does not change the notes that are queued before it)
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
    int             inst;               // the instrument (0..3 = the basic wave in osc.wave)
    synth_inst_t    cinst;              // the channel's own copy of the instrument in use (inst >= SYNTH_NBASIC)
    synth_osc_t     iosc[SYNTH_INST_NOSC];  // instrument oscillators
    double          fic1, fic2;         // low-pass state (TPT state variable filter: unconditionally stable)
    double          fa1, fa2, fa3;      // its coefficients (set every 32 samples)
    int             fcount;             // samples until the cutoff is computed again
    float           fenvlvl;            // filter envelope level 1 -> 0
    bool            cut_on;             // CUT: the cutoff multiple is set by the program ...
    bool            cut_glide, cut_restore;     // ... and is on its way to cut_to (restore: the instrument's own cutoff; the override ends there)
    float           cut_val, cut_from, cut_to;
    double          cut_pos, cut_total; // samples gone / samples of the glide
    float           fmlvl;              // FM index scale 1 -> 0
    double          fmdec;              // its factor per sample
    int             owner;              // voice slots (chan[SYNTH_NCHAN ..]): the channel (1..) the note belongs to, 0 = never used
    bool            held;               // voice slots: the note has not been released yet
    uint64_t        seq;                // voice slots: the order the notes were started in (the oldest is stolen first)
    uint64_t        poly_when;          // a polyphonic channel: the time of its last PLAY (events of the same time form a chord) ...
    bool            poly_any;           // ... and whether there was one
    double          pe, pdec;           // pitch envelope: semitones left, factor per sample
    bool            uenv_on;            // the envelope set by the program (SYNTH_EV_ENV); else the instrument's
    float           uenv[4];
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
    float                   p[4];       // ENV, VIB, TREM, INST
    synth_inst_t*           def;        // INSTDEF: the snapshot (owned)
} synth_event_t;

typedef struct _synth_evspec_t {        // what synth_schedule_ev() queues (unused fields 0)
    synth_evtype_t          type;
    int                     chan;
    float                   freq, freq2, vol;
    synth_wave_t            wave;
    uint64_t                dur;
    float                   p[4];
    const synth_inst_t*     def;        // INSTDEF (copied)
} synth_evspec_t;

typedef struct _synth_t {
    int             rate;               // samples per second
    float           slewstep;           // gain change per sample
    int             bpm;
    synth_chan_t    chan[SYNTH_NCHAN + SYNTH_NVOICES];      // the channels, then the voice slots of the polyphonic ones
    uint64_t        voiceseq;
    synth_inst_t    inst[SYNTH_NINST];
    uint64_t        cursor[SYNTH_NCHAN];
    synth_event_t*  events;             // sorted by time
    synth_event_t*  tail;               // the last one (events queued in time order are appended without a search)
    int             nevents;
    float           peak[SYNTH_NCHAN];  // debugging: the loudest sample each channel (and the mix) produced since the owner last reset these
    float           outpeak;
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

// instruments: 0..3 are fixed; 4..31 can be redefined. The table is the default of SYNTH_EV_INST / SYNTH_EV_WAVE with a number; a channel
// plays with its own copy (see SYNTH_EV_INSTDEF). (false if n or a field is out of range; "name" may be 0)
bool synth_setinst( synth_t* s, int n, const synth_inst_t* def );
bool synth_instvalid( const synth_inst_t* def );                // every field in range (what synth_setinst() checks)
const synth_inst_t* synth_getinst( const synth_t* s, int n );   // 0 if n is out of range or 0..3
const char* synth_instname( const synth_t* s, int n );          // "SINE" .. "NOISE", the preset name, "" if out of range
void synth_instdefault( synth_inst_t* def );                    // a plain instrument: one sine, no filter, no envelope
void synth_instpreset( int n, synth_inst_t* def );              // the factory definition of 4..19 (else the plain one)

void synth_render( synth_t* s, float* buf, int count );     // mono, count samples

#endif
