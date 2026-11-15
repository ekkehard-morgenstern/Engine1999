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


#include "sdlaudio.h"
#include "sdltypes.h"
#include "sdlevent.h"
#include "sdlutil.h"

// The synthesizer itself is in synth.c (no SDL). Here: the audio device, whose callback renders the synth, and a worker
// thread that applies the queued events when their time has come. One mutex guards the synth.

#define SDLAUD_IDLE_MSEC    50                      // the worker (sound log only) wakes this often (or when an event is queued)
#define SDLAUD_BLOCK        32                      // events are applied with a resolution of this many frames (0.7 ms)
#define SDLAUD_MINBUF       256
#define SDLAUD_MAXBUF       16384

static SDL_AudioSpec sdlaud_spec_in, sdlaud_spec_out;
static SDL_AudioDeviceID sdlaud_id;
static synth_t sdlaud_synth;
static SDL_mutex* sdlaud_mtx = 0;
static SDL_cond* sdlaud_cond = 0;       // signalled when an event is queued (the worker sleeps on it while the queue is empty)
static bool sdlaud_cleanup_warning = false;
static volatile bool sdlaud_initok = false;
static volatile bool sdlaud_request_exit = false;
static SDL_Thread* sdlaud_workerthr = 0;
static FILE* volatile sdlaud_log = 0;
static uint64_t sdlaud_logstart = 0;      // sound log (ENGINE1999_SOUNDLOG, see engine1999.c)
static uint64_t sdlaud_cbtime = 0, sdlaud_cbgap = 0;     // sound log: time of the last callback, longest gap since the last state line
static int sdlaud_cbcount = 0, sdlaud_cblen = 0;
static int sdlaud_bufsamples = SDLAUD_BUFSAMPLES;       // device buffer size in samples (ENGINE1999_AUDIOBUF; the size the device granted after opening)
static uint64_t sdlaud_frames = 0;      // the audio clock: frames rendered so far; the event times are applied against base + frames / rate
static uint64_t sdlaud_base = 0;        // wall clock time (ns) of frame 0 of the audio clock; 0 = not set yet (first callback)

static void sdlaud_callback( void* userdata, Uint8* stream, int len ) {
    (void) userdata;
    if ( sdlaud_cleanup_warning ) {
        memset( stream, 0, len );
        return;
    }
    SDL_LockMutex( sdlaud_mtx );
    if ( sdlaud_log != 0 ) {        // sound log: how often the device calls, in how big pieces, and the longest pause between two calls
        uint64_t t = sdlutil_getnsec( 0 );
        if ( sdlaud_cbtime != 0 && t - sdlaud_cbtime > sdlaud_cbgap ) sdlaud_cbgap = t - sdlaud_cbtime;
        sdlaud_cbtime = t;
        ++sdlaud_cbcount;
        sdlaud_cblen = len / (int)( 2 * sizeof(float) );
    }
    // Events are applied inside the callback against the audio clock, in small blocks, so a note starts at its own sample, not at the start
    // of the next device buffer: the size of the buffer only changes the latency, not the timing. The audio clock follows the wall clock
    // (the time base of the queued events) within a window: a device that stalls (underrun, virtual machine) or runs fast is resynchronised.
    int frames = len / (int)( 2 * sizeof(float) );
    int64_t wall = (int64_t) sdlutil_getnsec( 0 );
    int64_t dur = (int64_t)( (double) frames * 1.0e9 / (double) SDLAUD_BUFFREQ );
    if ( sdlaud_base == 0 ) {
        sdlaud_base = (uint64_t) wall;
        sdlaud_frames = 0;
    }
    int64_t t0 = (int64_t) sdlaud_base + (int64_t)( (double) sdlaud_frames * 1.0e9 / (double) SDLAUD_BUFFREQ );
    int64_t lowest = wall - dur - INT64_C(50000000), highest = wall + 4 * dur + INT64_C(50000000);
    if ( t0 < lowest ) {            // the audio clock fell behind (stalled device): jump forward, the missed events are applied at once
        sdlaud_base += (uint64_t)( lowest - t0 ); t0 = lowest;
    } else if ( t0 > highest ) {    // the device runs faster than the wall clock: pull the audio clock back
        sdlaud_base -= (uint64_t)( t0 - highest ); t0 = highest;
    }
    synth_render_timed( &sdlaud_synth, (float*) stream, frames, (uint64_t) t0, SDLAUD_BLOCK );
    sdlaud_frames += (uint64_t) frames;
    SDL_UnlockMutex( sdlaud_mtx );
}

static bool sdlaud_init_sdl( void ) {
    sdlaud_cleanup_warning = false;
    memset( &sdlaud_spec_in , 0, sizeof(SDL_AudioSpec) );
    memset( &sdlaud_spec_out, 0, sizeof(SDL_AudioSpec) );
    sdlaud_spec_in.freq     = SDLAUD_BUFFREQ;
    sdlaud_spec_in.format   = AUDIO_F32SYS;
    sdlaud_spec_in.channels = UINT8_C(2);        // stereo: the channels have a pan position
    sdlaud_base = 0; sdlaud_frames = 0;
    sdlaud_bufsamples = SDLAUD_BUFSAMPLES;
    const char* bs = getenv( "ENGINE1999_AUDIOBUF" );       // a bigger buffer helps on machines that stall the callback (virtual machines)
    if ( bs != 0 && *bs != 0 ) {
        char* endp = 0;
        long v = strtol( bs, &endp, 10 );
        if ( *endp == 0 && v >= SDLAUD_MINBUF && v <= SDLAUD_MAXBUF ) {
            sdlaud_bufsamples = (int) v;
        } else {
            fprintf( stderr, "ENGINE1999_AUDIOBUF ignored: use %d to %d samples\n", SDLAUD_MINBUF, SDLAUD_MAXBUF );
        }
    }
    sdlaud_spec_in.samples  = (Uint16) sdlaud_bufsamples;
    sdlaud_spec_in.callback = sdlaud_callback;
    sdlaud_spec_in.userdata = 0;
    sdlaud_id = SDL_OpenAudioDevice( 0, 0, &sdlaud_spec_in, &sdlaud_spec_out, 0 );
    if ( sdlaud_id == 0 ) {
        fprintf( stderr, "failed to open SDL audio device: %s\n", SDL_GetError() );
        return false;
    }
    sdlaud_bufsamples = sdlaud_spec_out.samples;
    if ( sdlaud_spec_out.samples != sdlaud_spec_in.samples ) {
        fprintf( stderr, "audio: the device granted a buffer of %d frames instead of %d\n", (int) sdlaud_spec_out.samples, (int) sdlaud_spec_in.samples );
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
    sdlaud_cond = SDL_CreateCond();
    if ( sdlaud_cond == 0 ) {
        fprintf( stderr, "sdlaud_worker: failed to create condition: %s\n", SDL_GetError() );
        goto ERR2;
    }
    if ( !sdlaud_init_sdl() ) {
        goto ERR3;
    }
    sdlaud_initok = true;
    sdlev_raise( SDLEV_AUDIOWORKERINITDONE );

    while ( !sdlaud_request_exit ) {
        uint64_t now = sdlutil_getnsec( 0 );
        SDL_LockMutex( sdlaud_mtx );       // the events themselves are applied by the audio callback (sample accurate); the worker only writes the sound log
        FILE* lg = sdlaud_log;
        if ( lg != 0 ) {        // the state of the channels twice a second: on / gain / how far ahead of now the channel is queued (s) or idle, queue length
            static uint64_t lastlog = 0;
            if ( now - lastlog >= UINT64_C(500000000) ) {
                lastlog = now;
                fprintf( lg, "%.3f state bpm %d queued %d", (double)( now - sdlaud_logstart ) / 1.0e9, sdlaud_synth.bpm, sdlaud_synth.nevents );
                for ( int i=0; i < SYNTH_NCHAN; ++i ) {
                    const synth_chan_t* c = &sdlaud_synth.chan[i];
                    if ( sdlaud_synth.cursor[i] > now ) {
                        fprintf( lg, " | %d %s g%.2f +%.2f pk%.2f", i + 1, c->on ? "on" : "off", (double) c->gain,
                                 ( (double) sdlaud_synth.cursor[i] - (double) now ) / 1.0e9, (double) sdlaud_synth.peak[i] );
                    } else {
                        fprintf( lg, " | %d %s g%.2f idle pk%.2f", i + 1, c->on ? "on" : "off", (double) c->gain, (double) sdlaud_synth.peak[i] );
                    }
                    sdlaud_synth.peak[i] = 0.0f;
                }
                fprintf( lg, " | mix pk%.2f callbacks %d of %d frames, longest gap %.0f ms", (double) sdlaud_synth.outpeak, sdlaud_cbcount,
                         sdlaud_cblen, (double) sdlaud_cbgap / 1.0e6 );
                sdlaud_synth.outpeak = 0.0f; sdlaud_cbcount = 0; sdlaud_cbgap = 0;
                fputc( '\n', lg );
            }
        }
        SDL_CondWaitTimeout( sdlaud_cond, sdlaud_mtx, SDLAUD_IDLE_MSEC );
        SDL_UnlockMutex( sdlaud_mtx );
    }

    sdlaud_initok = false;
    sdlaud_cleanup_sdl();
ERR3:   SDL_DestroyCond( sdlaud_cond ); sdlaud_cond = 0;
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
        SDL_LockMutex( sdlaud_mtx );
        SDL_CondSignal( sdlaud_cond );
        SDL_UnlockMutex( sdlaud_mtx );
        sdlev_wait( SDLEV_AUDIOWORKERFINISHED );
    }

    // wait for thread to terminate and reap the thread status
    int rv = 0;
    SDL_WaitThread( sdlaud_workerthr, &rv ); sdlaud_workerthr = 0;
}

// -- the API: all of it does nothing when there is no audio device -----------------------

// queues an event at the channel's time line; the cursor moves on by "beats"
static void sdlaud_queue( int chan, float beats, synth_evspec_t* sp ) {
    if ( !sdlaud_initok ) return;
    uint64_t now = sdlutil_getnsec( 0 );
    SDL_LockMutex( sdlaud_mtx );
    uint64_t when = synth_slot( &sdlaud_synth, chan, beats, now );
    if ( when != ~UINT64_C(0) ) {
        sp->chan = chan;
        sp->dur = synth_beatsnsec( &sdlaud_synth, beats );
        synth_schedule_ev( &sdlaud_synth, when, sp );
        SDL_CondSignal( sdlaud_cond );
    }
    SDL_UnlockMutex( sdlaud_mtx );
}

static void sdlaud_event( int chan, float beats, synth_evtype_t type, float freq, float vol, synth_wave_t wave ) {
    synth_evspec_t sp;
    memset( &sp, 0, sizeof(sp) );
    sp.type = type; sp.freq = freq; sp.vol = vol; sp.wave = wave;
    sdlaud_queue( chan, beats, &sp );
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

void sdlaud_playchord( int chan, float freq, float vol ) {
    sdlaud_event( chan, 0.0f, SYNTH_EV_PLAY, freq, vol, SYNTH_SINE );
}

void sdlaud_playsweep( int chan, float freq, float freq2, float vol, float beats ) {
    synth_evspec_t sp;
    memset( &sp, 0, sizeof(sp) );
    sp.type = SYNTH_EV_PLAY; sp.freq = freq; sp.freq2 = freq2; sp.vol = vol;
    sdlaud_queue( chan, beats, &sp );
}

void sdlaud_setlog( FILE* f, uint64_t start ) {
    sdlaud_logstart = start;
    if ( f != 0 ) {         // what the device really granted (it may differ from ENGINE1999_AUDIOBUF)
        const char* drv = SDL_GetCurrentAudioDriver();
        fprintf( f, "0.000 audio driver %s, %d Hz, %d channels, buffer %d frames granted (asked for %d)\n", drv != 0 ? drv : "?",
                 (int) sdlaud_spec_out.freq, (int) sdlaud_spec_out.channels, (int) sdlaud_spec_out.samples, (int) sdlaud_spec_in.samples );
    }
    sdlaud_log = f;
}

void sdlaud_stopchan( int chan, float beats ) {
    sdlaud_event( chan, beats, SYNTH_EV_STOP, 0.0f, 0.0f, SYNTH_SINE );
}

void sdlaud_setwave( int chan, int wave ) {
    sdlaud_event( chan, 0.0f, SYNTH_EV_WAVE, 0.0f, 0.0f, (synth_wave_t) wave );
}

// the channel uses instrument n (4..63) with this definition from the channel's cursor on (a snapshot: the audio engine keeps a copy)
void sdlaud_setinst( int chan, int n, const synth_inst_t* def ) {
    synth_evspec_t sp;
    memset( &sp, 0, sizeof(sp) );
    sp.type = SYNTH_EV_INSTDEF; sp.p[0] = (float) n; sp.def = def;
    sdlaud_queue( chan, 0.0f, &sp );
}

static void sdlaud_setfx( int chan, synth_evtype_t type, float p0, float p1, float p2, float p3 ) {
    synth_evspec_t sp;
    memset( &sp, 0, sizeof(sp) );
    sp.type = type; sp.p[0] = p0; sp.p[1] = p1; sp.p[2] = p2; sp.p[3] = p3;
    sdlaud_queue( chan, 0.0f, &sp );
}

void sdlaud_setenvelope( int chan, float attack, float decay, float sustain, float release ) {
    sdlaud_setfx( chan, SYNTH_EV_ENV, attack, decay, sustain, release );
}

void sdlaud_setvibrato( int chan, float rate, float depth ) {
    sdlaud_setfx( chan, SYNTH_EV_VIB, rate, depth, 0.0f, 0.0f );
}

void sdlaud_setcutoff( int chan, float mult, float seconds ) {
    sdlaud_setfx( chan, SYNTH_EV_CUT, mult, seconds, 0.0f, 0.0f );
}

void sdlaud_setpan( int chan, float pos ) {
    sdlaud_setfx( chan, SYNTH_EV_PAN, pos, 0.0f, 0.0f, 0.0f );
}

void sdlaud_settremolo( int chan, float rate, float depth ) {
    sdlaud_setfx( chan, SYNTH_EV_TREM, rate, depth, 0.0f, 0.0f );
}

void sdlaud_setpitch( int chan, float freq ) {
    if ( !sdlaud_initok ) return;
    SDL_LockMutex( sdlaud_mtx );
    synth_setpitch( &sdlaud_synth, chan, freq );
    SDL_UnlockMutex( sdlaud_mtx );
}

void sdlaud_setvolume( int chan, float vol ) {
    if ( !sdlaud_initok ) return;
    SDL_LockMutex( sdlaud_mtx );
    synth_setvolume( &sdlaud_synth, chan, vol );
    SDL_UnlockMutex( sdlaud_mtx );
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

void sdlaud_silencechan( int chan ) {
    if ( !sdlaud_initok ) return;
    uint64_t now = sdlutil_getnsec( 0 );
    SDL_LockMutex( sdlaud_mtx );
    synth_silencechan( &sdlaud_synth, chan, now );
    SDL_UnlockMutex( sdlaud_mtx );
}

bool sdlaud_busy( int chan ) {
    if ( !sdlaud_initok ) return false;
    uint64_t now = sdlutil_getnsec( 0 );
    SDL_LockMutex( sdlaud_mtx );
    bool busy = synth_busy( &sdlaud_synth, chan, now );
    SDL_UnlockMutex( sdlaud_mtx );
    return busy;
}

int sdlaud_getbpm( void ) {
    if ( !sdlaud_initok ) return 0;
    SDL_LockMutex( sdlaud_mtx );
    int bpm = sdlaud_synth.bpm;
    SDL_UnlockMutex( sdlaud_mtx );
    return bpm;
}
