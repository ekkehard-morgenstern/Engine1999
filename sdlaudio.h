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

#ifndef SDLAUDIO_H
#define SDLAUDIO_H  1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#include "synth.h"

#define SDLAUD_BUFFREQ      44100
#define SDLAUD_BUFSAMPLES   1024

#define SDLAUD_LOGCHAN      SYNTH_NCHAN

// Wave types for sdlaud_setwave(): SYNTH_SINE, SYNTH_SQUARE, SYNTH_TRIANGLE, SYNTH_NOISE (see synth.h).
// Everything is queued for a time: "beats" is the duration of the event, i.e. the delay before the next event of the
// same channel (the channel's cursor, see synth.h); a lagging cursor restarts at the current time. All calls do
// nothing if there is no audio device (sdlaud_init() failed).
//   sequence:  sdlaud_playnote( 0, 12, 0.5f, 1.0f ); sdlaud_playnote( 0, 16, 0.5f, 1.0f ); sdlaud_stopchan( 0, 0.0f );
//   chord:     sdlaud_syncchans( 7 ); then play on channels 0, 1, 2 with the same beats
bool sdlaud_init( void );
void sdlaud_cleanup( void );
void sdlaud_playnote( int chan, int note, float vol, float beats );     // note 0..71 (0 = 110 Hz), vol 0..1
void sdlaud_playfreq( int chan, float freq, float vol, float beats );   // freq 20..20000 Hz
void sdlaud_stopchan( int chan, float beats );                          // fades the channel out
void sdlaud_setwave( int chan, int wave );                              // at the channel's cursor, takes no time
void sdlaud_setbpm( int bpm );                                          // tempo, default 60 (1 beat = 1 s)
void sdlaud_syncchans( unsigned chanmask );                             // bit n = channel n; lines up their cursors
void sdlaud_silenceall( void );                                         // now: drops everything queued, fades all out

#endif
