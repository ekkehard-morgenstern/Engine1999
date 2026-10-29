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


#include "sdlaudio.h"
#include "sdltypes.h"
#include "sdlevent.h"
#include "sdlutil.h"

// The synthesizer itself is in synth.c (no SDL). Here: the audio device, whose callback renders the synth, and a worker
// thread that applies the queued events when their time has come. One mutex guards the synth.

#define SDLAUD_POLL_NSEC    UINT64_C(1000000)       // the worker looks at the queue at least this often (new events)

static SDL_AudioSpec sdlaud_spec_in, sdlaud_spec_out;
static SDL_AudioDeviceID sdlaud_id;
static synth_t sdlaud_synth;
static SDL_mutex* sdlaud_mtx = 0;
static bool sdlaud_cleanup_warning = false;
static volatile bool sdlaud_initok = false;
static volatile bool sdlaud_request_exit = false;
static SDL_Thread* sdlaud_workerthr = 0;

static void sdlaud_callback( void* userdata, Uint8* stream, int len ) {
    (void) userdata;
    if ( sdlaud_cleanup_warning ) {
        memset( stream, 0, len );
        return;
    }
    SDL_LockMutex( sdlaud_mtx );
    synth_render( &sdlaud_synth, (float*) stream, len / (int) sizeof(float) );
    SDL_UnlockMutex( sdlaud_mtx );
}

static bool sdlaud_init_sdl( void ) {
    sdlaud_cleanup_warning = false;
    memset( &sdlaud_spec_in , 0, sizeof(SDL_AudioSpec) );
    memset( &sdlaud_spec_out, 0, sizeof(SDL_AudioSpec) );
    sdlaud_spec_in.freq     = SDLAUD_BUFFREQ;
    sdlaud_spec_in.format   = AUDIO_F32SYS;
    sdlaud_spec_in.channels = UINT8_C(1);
    sdlaud_spec_in.samples  = SDLAUD_BUFSAMPLES;
    sdlaud_spec_in.callback = sdlaud_callback;
    sdlaud_spec_in.userdata = 0;
    sdlaud_id = SDL_OpenAudioDevice( 0, 0, &sdlaud_spec_in, &sdlaud_spec_out, 0 );
    if ( sdlaud_id == 0 ) {
        fprintf( stderr, "failed to open SDL audio device: %s\n", SDL_GetError() );
        return false;
    }
    synth_init( &sdlaud_synth, SDLAUD_BUFFREQ );
    SDL_PauseAudioDevice( sdlaud_id, 0 );
    return true;
}

static void sdlaud_cleanup_sdl( void ) {
    sdlaud_cleanup_warning = true;
    SDL_PauseAudioDevice( sdlaud_id, 1 );
    SDL_CloseAudioDevice( sdlaud_id ); sdlaud_id = 0;       // returns when the callback is not running any more
    synth_cleanup( &sdlaud_synth );
}

static int sdlaud_worker( void* arg ) {
    (void) arg;
    sdlaud_initok = false;
    sdlaud_request_exit = false;
    sdlaud_mtx = SDL_CreateMutex();
    if ( sdlaud_mtx == 0 ) {
        fprintf( stderr, "sdlaud_worker: failed to create mutex: %s\n", SDL_GetError() );
        goto FAIL;
    }
    if ( !sdlaud_init_sdl() ) {
        goto ERR2;
    }
    sdlaud_initok = true;
    sdlev_raise( SDLEV_AUDIOWORKERINITDONE );

    while ( !sdlaud_request_exit ) {
        uint64_t now = sdlutil_getnsec( 0 ), next = 0;
        bool queued;
        SDL_LockMutex( sdlaud_mtx );
        synth_runevents( &sdlaud_synth, now );
        queued = synth_nextevent( &sdlaud_synth, &next );
        SDL_UnlockMutex( sdlaud_mtx );
        uint64_t wait = SDLAUD_POLL_NSEC;
        if ( queued && next > now && next - now < wait ) {
            wait = next - now;
        }
        if ( queued && next <= now ) {
            continue;       // more events are due already
        }
        sdlutil_nanosleep( wait, 0 );
    }

    sdlaud_initok = false;
    sdlaud_cleanup_sdl();
ERR2:   SDL_DestroyMutex( sdlaud_mtx ); sdlaud_mtx = 0;
        // a failed start must not leave sdlaud_init() waiting: it gets the "init done" signal with sdlaud_initok false
FAIL:   if ( !sdlaud_initok && !sdlaud_request_exit ) {
            sdlev_raise( SDLEV_AUDIOWORKERINITDONE );
        } else {
            sdlev_raise( SDLEV_AUDIOWORKERFINISHED );
        }
        return 0;
}

bool sdlaud_init( void ) {
    sdlaud_workerthr = SDL_CreateThread( sdlaud_worker, "sdlaud_worker", 0 );
    if ( sdlaud_workerthr == 0 ) {
        fprintf( stderr, "failed to create audio worker: %s\n", SDL_GetError() );
        return false;
    }
    sdlev_wait( SDLEV_AUDIOWORKERINITDONE );
    if ( !sdlaud_initok ) {
        // wait for thread to terminate and reap the thread status
        int rv = 0;
        SDL_WaitThread( sdlaud_workerthr, &rv );
        sdlaud_workerthr = 0;
        return false;
    }
    return true;
}

void sdlaud_cleanup( void ) {

    if ( sdlaud_workerthr == 0 ) return;

    if ( sdlaud_initok ) {
        // the thread is running: set the exit flag and wait for the handshake (thread termination)
        sdlaud_request_exit = true;
        sdlev_wait( SDLEV_AUDIOWORKERFINISHED );
    }

    // wait for thread to terminate and reap the thread status
    int rv = 0;
    SDL_WaitThread( sdlaud_workerthr, &rv ); sdlaud_workerthr = 0;
}

// -- the API: all of it does nothing when there is no audio device -----------------------

static void sdlaud_event( int chan, float beats, synth_evtype_t type, float freq, float vol, synth_wave_t wave ) {
    if ( !sdlaud_initok ) return;
    uint64_t now = sdlutil_getnsec( 0 );
    SDL_LockMutex( sdlaud_mtx );
    uint64_t when = synth_slot( &sdlaud_synth, chan, beats, now );
    if ( when != ~UINT64_C(0) ) {
        synth_schedule( &sdlaud_synth, when, type, chan, freq, vol, wave );
    }
    SDL_UnlockMutex( sdlaud_mtx );
}

void sdlaud_playnote( int chan, int note, float vol, float beats ) {
    float freq = synth_notefreq( note );
    if ( freq > 0.0f ) {
        sdlaud_event( chan, beats, SYNTH_EV_PLAY, freq, vol, SYNTH_SINE );
    }
}

void sdlaud_playfreq( int chan, float freq, float vol, float beats ) {
    sdlaud_event( chan, beats, SYNTH_EV_PLAY, freq, vol, SYNTH_SINE );
}

void sdlaud_stopchan( int chan, float beats ) {
    sdlaud_event( chan, beats, SYNTH_EV_STOP, 0.0f, 0.0f, SYNTH_SINE );
}

void sdlaud_setwave( int chan, int wave ) {
    sdlaud_event( chan, 0.0f, SYNTH_EV_WAVE, 0.0f, 0.0f, (synth_wave_t) wave );
}

void sdlaud_setbpm( int bpm ) {
    if ( !sdlaud_initok ) return;
    SDL_LockMutex( sdlaud_mtx );
    synth_setbpm( &sdlaud_synth, bpm );
    SDL_UnlockMutex( sdlaud_mtx );
}

void sdlaud_syncchans( unsigned chanmask ) {
    if ( !sdlaud_initok ) return;
    uint64_t now = sdlutil_getnsec( 0 );
    SDL_LockMutex( sdlaud_mtx );
    synth_sync( &sdlaud_synth, chanmask, now );
    SDL_UnlockMutex( sdlaud_mtx );
}

void sdlaud_silenceall( void ) {
    if ( !sdlaud_initok ) return;
    uint64_t now = sdlutil_getnsec( 0 );
    SDL_LockMutex( sdlaud_mtx );
    synth_silence( &sdlaud_synth, now );
    SDL_UnlockMutex( sdlaud_mtx );
}
