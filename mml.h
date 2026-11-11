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

#ifndef MML_H
#define MML_H       1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif
#include "synth.h"

/*
Parser for the music strings of the PLAY statement ("music macro language"). No SDL dependency: it turns a string into
a list of events with times in seconds; the VM hands them to the host's sound callbacks (see vm.h).

    A..G [#|+|-] [n] [.]   a note: # or + sharp, - flat; n = its length (1 = whole, 2 = half, 4 = quarter, ... up to 64),
                           each dot makes it half as long again; without n the default length (L) applies
    R or P [n] [.]         a rest
    O n                    octave 0..8 (O4 holds middle C; A in O4 = 440 Hz); < and > go one octave down / up
    L n                    default note length 1..64 (starts as 4)
    T n                    tempo in quarter notes per minute, 1..1000 (starts as 120)
    V x                    volume 0..1, may have decimals (starts as 0.5)
    W n  or  I n           wave 0..3 (0 sine, 1 square, 2 triangle, 3 noise) or instrument 4..63 (the presets, see synth.h and
                           INSTRUMENT); the channel keeps its wave / instrument otherwise
    ML, MN, MS             legato (the note fills its length), normal (7/8, then silence; the default) and staccato (3/4)
    blanks and ;           ignored; letters may be lower case

Commands for sound effects (numbers may have decimals; the values are separated by commas, and frequencies are 20..20000 Hz):

    H f [, n] [.]          a tone of f Hz (also for noise) with the length n (default L); articulation as for notes
    S f1, f2 [, n] [.]     a sweep: the pitch glides from f1 to f2 during the length n
    X a, d, s, r           envelope: attack, decay (seconds, 0..60), sustain level 0..1, release (seconds)
    XR                     back to the default envelope (a short fade in and out)
    [CEG]n                 a chord: the notes (A..G with # + -, and < > for the octave) sound together for the length n (and dots); an instrument with
                           POLY gives each a voice (on a single-voice instrument only the last one is heard)
    ~ rate, depth          vibrato: rate in Hz 0..100, depth in semitones 0..24 (0 = off)
    ^ rate, depth          tremolo: rate in Hz 0..100, depth 0..1 (0 = off)
    K mult [, seconds]     filter cutoff of an instrument (see CUTOFF): glides to mult (0.01..64) times the note frequency within the seconds
                           (0..60, default 0 = at once); KR [, seconds] = back to the instrument's own cutoff

The effect commands stay in effect on the channel until changed (like W); stored effects (SFX) reset them first.

The frequencies must be within 20..20000 Hz (an octave 0 C is too low).
*/

#define MML_MAXEVENTS   100000
#define MML_MAXCHORD    16
#define MML_ERRLEN      64

typedef enum _mml_kind_t {
    MML_TONE,           // freq, vol for "seconds", then silence for "gap" seconds; with freq2 != 0 the pitch glides to it
    MML_REST,           // silence for "seconds"
    MML_WAVE,           // select the wave type
    MML_ENV,            // envelope p[0..3] = attack, decay, sustain, release; p[0] < 0 = default
    MML_VIB,            // vibrato p[0] = rate, p[1] = depth
    MML_TREM,           // tremolo p[0] = rate, p[1] = depth
    MML_CUT,            // filter cutoff p[0] = multiple of the note (-1 = the instrument's own), p[1] = seconds to get there
    MML_CHORD           // a note (freq, vol) that starts at once without taking time: the notes of a chord before its last one (that one is an MML_TONE)
} mml_kind_t;

typedef struct _mml_event_t {
    mml_kind_t  kind;
    float       freq, freq2, vol;
    float       seconds, gap;
    int         wave;
    float       p[4];
} mml_event_t;

typedef struct _mml_t {
    mml_event_t*    events;     // malloc'ed
    size_t          count;
} mml_t;

// parses "text"; false with a message in err (and nothing in mml) if it is wrong. The caller frees with mml_free().
bool mml_parse( const char* text, mml_t* mml, char err[MML_ERRLEN] );
void mml_free( mml_t* mml );

/*
Parser for the definition string of INSTRUMENT n, a$: commands separated by ';', values separated by blanks or commas, words
and wave names in any case. A definition starts from a plain sine instrument.

    NAME w                           the name (up to 11 letters / digits / _)
    OSC wave [level [ratio [cents [pw]]]] [FM index] [MOD] [POST]   an oscillator (up to 3): wave = SINE SQUARE TRIANGLE NOISE SAW (or 0..4), level 0..2 (1),
                                     frequency ratio to the note 0..64 (1; noise: 0 = white noise), detune in cents (0), pulse width of SQUARE (0.5);
                                     a NEGATIVE ratio is a fixed frequency of -ratio Hz (1..20000: it ignores the note; noise: the rate of the noise), POST
                                     lets the oscillator bypass the filter (it is mixed in behind it); FM: the phase of the oscillator is modulated by
                                     the NEXT oscillator of the definition with this index in radians 0..20 (the modulator's level scales it; a chain
                                     osc 1 <- osc 2 <- osc 3 works), MOD: the oscillator is a modulator only (it is not mixed: it is heard through the
                                     oscillator it modulates)
    FILTER cutmul [res [fenv [fdecay]]]    low-pass at note frequency * cutmul, resonance 0..0.95, a filter envelope starting fenv octaves
                                     higher and decaying with the time constant fdecay seconds (default 0.2)
    PITCH semitones seconds          pitch envelope: starts semitones off the note and decays with the time constant
    ENV a d s r                      default envelope (an ENVELOPE of the program wins)
    VIB rate depth                   default vibrato (a VIBRATO of the program wins)
    FMDECAY seconds                  the FM indexes fall exponentially (time constant 0..10 s; 0 = they stay): bells, electric pianos, plucks
    POLY n                           polyphony 1..8 (1): a channel using the instrument gives every note its own voice, so that a chord ([CEG] in
                                     PLAY), the release tails of earlier notes and a held note under the next one sound together (up to n at a time, the
                                     oldest is taken over); the voices come from a pool of 32 shared by all channels
    GAIN g                           overall level 0..4 (1)
*/
bool mml_parse_inst( const char* text, synth_inst_t* inst, char err[MML_ERRLEN] );

// the definition string of an instrument (what mml_parse_inst() reads back to the same instrument; "NAME USER" if it has no name); buf must hold MML_INSTSTRLEN bytes
#define MML_INSTSTRLEN  1024
void mml_inst_string( const synth_inst_t* inst, char* buf, size_t size );

#endif
