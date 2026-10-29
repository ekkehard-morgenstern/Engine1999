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

#ifndef MML_H
#define MML_H       1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

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
    W n                    wave 0..3 (0 sine, 1 square, 2 triangle, 3 noise); the channel keeps its wave otherwise
    ML, MN, MS             legato (the note fills its length), normal (7/8, then silence; the default) and staccato (3/4)
    blanks and ;           ignored; letters may be lower case

Commands for sound effects (numbers may have decimals; the values are separated by commas, and frequencies are 20..20000 Hz):

    H f [, n] [.]          a tone of f Hz (also for noise) with the length n (default L); articulation as for notes
    S f1, f2 [, n] [.]     a sweep: the pitch glides from f1 to f2 during the length n
    X a, d, s, r           envelope: attack, decay (seconds, 0..60), sustain level 0..1, release (seconds)
    XR                     back to the default envelope (a short fade in and out)
    ~ rate, depth          vibrato: rate in Hz 0..100, depth in semitones 0..24 (0 = off)
    ^ rate, depth          tremolo: rate in Hz 0..100, depth 0..1 (0 = off)

The effect commands stay in effect on the channel until changed (like W); stored effects (SFX) reset them first.

The frequencies must be within 20..20000 Hz (an octave 0 C is too low).
*/

#define MML_MAXEVENTS   100000
#define MML_ERRLEN      64

typedef enum _mml_kind_t {
    MML_TONE,           // freq, vol for "seconds", then silence for "gap" seconds; with freq2 != 0 the pitch glides to it
    MML_REST,           // silence for "seconds"
    MML_WAVE,           // select the wave type
    MML_ENV,            // envelope p[0..3] = attack, decay, sustain, release; p[0] < 0 = default
    MML_VIB,            // vibrato p[0] = rate, p[1] = depth
    MML_TREM            // tremolo p[0] = rate, p[1] = depth
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

#endif
