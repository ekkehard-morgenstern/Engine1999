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

typedef struct _noteent_t {
    const char* name;
    float       freq;
} noteent_t;

#define NUM_NOTEDEFS    60

static const noteent_t sdlaud_notedefs[NUM_NOTEDEFS] = {
    { "A 0", 110.0f   }, { "A#0", 116.541f }, { "B 0", 123.471f }, { "C 0", 130.813f }, { "C#0", 138.591f }, { "D 0", 146.832f },
    { "D#0", 155.563f }, { "E 0", 164.814f }, { "F 0", 174.614f }, { "F#0", 184.997f }, { "G 0", 195.998f }, { "G#0", 207.652f },
    { "A 1", 220.0f   }, { "A#1", 233.082f }, { "B 1", 246.942f }, { "C 1", 261.626f }, { "C#1", 277.183f }, { "D 1", 293.665f },
    { "D#1", 311.127f }, { "E 1", 329.628f }, { "F 1", 349.228f }, { "F#1", 369.994f }, { "G 1", 391.995f }, { "G#1", 415.305f },
    { "A 2", 440.0f   }, { "A#2", 466.164f }, { "B 2", 493.883f }, { "C 2", 523.251f }, { "C#2", 554.365f }, { "D 2", 587.33f  },
    { "D#2", 622.254f }, { "E 2", 659.255f }, { "F 2", 698.456f }, { "F#2", 739.989f }, { "G 2", 783.991f }, { "G#2", 830.609f },
    { "A 3", 880.0f   }, { "A#3", 932.328f }, { "B 3", 987.767f }, { "C 3", 1046.5f  }, { "C#3", 1108.73f }, { "D 3", 1174.66f },
    { "D#3", 1244.51f }, { "E 3", 1318.51f }, { "F 3", 1396.91f }, { "F#3", 1479.98f }, { "G 3", 1567.98f }, { "G#3", 1661.22f },
    { "A 4", 1760.0f  }, { "A#4", 1864.66f }, { "B 4", 1975.53f }, { "C 4", 2093.0f  }, { "C#4", 2217.46f }, { "D 4", 2349.32f },
    { "D#4", 2489.02f }, { "E 4", 2637.02f }, { "F 4", 2793.83f }, { "F#4", 2959.96f }, { "G 4", 3135.96f }, { "G#4", 3322.44f }
};

typedef struct _sdlaud_wave_t {
    float   buf[SDLAUD_BUFSAMPLES];
    float   amp;
    float   freq;
    int     nsamp;
    int     rpos;
} sdlaud_wave_t;

#define SAMPLEDUR   2.26757369614e-05f

static int sdlaud_samplecnt( float freq ) {
    float cycdur = 1.0f / freq;
    float numcyc = cycdur / SAMPLEDUR;
    return (int) truncf( numcyc );
}

static void sdlaud_renderwave( float* buf, int nsamp, float vol ) {
    float anglestep = ( 2.0f * 3.14159265f ) / ( (float) nsamp );
    float angle = 0.0f;
    float* ptr = buf;
    for ( int i=0; i < nsamp; ++i ) {
        *ptr++ = sinf( angle ) * vol;
        angle += anglestep;
    }
}

static void sdlaud_initwave( sdlaud_wave_t* wave, float freq, float vol ) {
    wave->nsamp = sdlaud_samplecnt( freq );
    if ( wave->nsamp > SDLAUD_BUFSAMPLES ) {
        wave->nsamp = SDLAUD_BUFSAMPLES;
    }
    wave->freq = freq;
    wave->amp  = vol;
    sdlaud_renderwave( wave->buf, wave->nsamp, vol );
}

static void sdlaud_silencewave( sdlaud_wave_t* wave ) {
    sdlaud_initwave( wave, 440.0f, 0.0f );
}

static void sdlaud_readwave( sdlaud_wave_t* wave, float* buf, int count ) {
    int pos = 0;
    while ( pos < count ) {
        int remain_target = count - pos;
        int remain_source = wave->nsamp - wave->rpos;
        int tocopy        = remain_source < remain_target ? remain_source : remain_target;
        memcpy( &buf[pos], &wave->buf[wave->rpos], sizeof(float) * tocopy );
        pos += tocopy;
        wave->rpos += tocopy;
        if ( wave->rpos >= wave->nsamp ) {
            wave->rpos = 0;
        }
    }
}

typedef struct _sdlaud_inst_t {
    sdlaud_wave_t   wave;
    float           vol;
    float           freq;
    int             note;
} sdlaud_inst_t;

static void sdlaud_initinst( sdlaud_inst_t* inst, int note, float freq, float vol ) {
    if ( note == -1 ) {
        inst->freq = freq;
        inst->note = -1;
    } else {
        if ( note < 0 ) {
            inst->note = 0;
        } else if ( note >= NUM_NOTEDEFS ) {
            inst->note = NUM_NOTEDEFS-1;
        } else {
            inst->note = note;
        }
        inst->freq = sdlaud_notedefs[ inst->note ].freq;
    }
    inst->vol = vol;
    sdlaud_initwave( &inst->wave, inst->freq, inst->vol );
}

static void sdlaud_silenceinst( sdlaud_inst_t* inst ) {
    inst->freq = 440.0f;
    inst->note = -1;
    inst->vol  = 0.0f;
    sdlaud_silencewave( &inst->wave );
}

static void sdlaud_readinst( sdlaud_inst_t* inst, float* buf, int count ) {
    sdlaud_readwave( &inst->wave, buf, count );
}

typedef struct _sdlaud_chan_t {
    sdlaud_inst_t   inst;
    float           vol;
} sdlaud_chan_t;

static void sdlaud_initchan( sdlaud_chan_t* chan, int note, float freq, float vol ) {
    chan->vol = vol;
    sdlaud_initinst( &chan->inst, note, freq, vol );
}

static void sdlaud_silencechan( sdlaud_chan_t* chan ) {
    chan->vol = 0.0f;
    sdlaud_silenceinst( &chan->inst );
}

static void sdlaud_readchan( sdlaud_chan_t* chan, float* buf, int count ) {
    sdlaud_readinst( &chan->inst, buf, count );
}

#define MIXBUF_SAMP     256

typedef struct _sdlaud_mixer_t {
    sdlaud_chan_t chan[SDLAUD_LOGCHAN];
    float         chnbuf[SDLAUD_LOGCHAN][MIXBUF_SAMP];
    float         sumbuf[MIXBUF_SAMP];
    int           fill;
    int           rpos;
} sdlaud_mixer_t;

static void sdlaud_initmixer( sdlaud_mixer_t* mixer ) {
    for ( int i=0; i < SDLAUD_LOGCHAN; ++i ) {
        sdlaud_silencechan( &mixer->chan[i] );
    }
    memset( &mixer->chnbuf[0][0], 0, sizeof(float) * SDLAUD_LOGCHAN * MIXBUF_SAMP );
    memset( &mixer->sumbuf[0], 0, sizeof(float) * MIXBUF_SAMP );
    mixer->rpos = 0;
    mixer->fill = 0;
}

static void sdlaud_mixer_playnote( sdlaud_mixer_t* mixer, int chan, int note, float vol ) {
    if ( chan < 0 || chan >= SDLAUD_LOGCHAN ) {
        return;
    }
    if ( note < 0 || note >= NUM_NOTEDEFS ) {
        return;
    }
    if ( vol < 0.0f || vol > 1.0f ) {
        return;
    }
    sdlaud_initchan( &mixer->chan[chan], note, 0.0f, vol );
}

static void sdlaud_mixer_playfreq( sdlaud_mixer_t* mixer, int chan, float freq, float vol ) {
    if ( chan < 0 || chan >= SDLAUD_LOGCHAN ) {
        return;
    }
    if ( freq < 110.0f || freq > 3322.44f ) {
        return;
    }
    if ( vol < 0.0f || vol > 1.0f ) {
        return;
    }
    sdlaud_initchan( &mixer->chan[chan], -1, freq, vol );
}

static void sdlaud_mixer_stopchan( sdlaud_mixer_t* mixer, int chan ) {
    if ( chan < 0 || chan >= SDLAUD_LOGCHAN ) {
        return;
    }
    sdlaud_silencechan( &mixer->chan[chan] );
}

static void sdlaud_mixer_mix( sdlaud_mixer_t* mixer, int maxsamp ) {
    float sumvol = 0.0f;
    for ( int i=0; i < SDLAUD_LOGCHAN; ++i ) {
        sdlaud_readchan( &mixer->chan[i], &mixer->chnbuf[i][0], maxsamp );
        sumvol += mixer->chan[i].vol;
    }
    float mult;
    if ( sumvol > 1.0f ) {
        mult = 1.0f / sumvol;
    } else if ( sumvol != 0.0f ) {
        mult = 1.0f;
    } else {
        mult = 0.0f;
    }
    memset( &mixer->sumbuf[0], 0, sizeof(float) * maxsamp );
    for ( int i=0; i < SDLAUD_LOGCHAN; ++i ) {
        for ( int j=0; j < maxsamp; ++j ) {
            mixer->sumbuf[j] += mixer->chnbuf[i][j];
        }
    }
    for ( int j=0; j < maxsamp; ++j ) {
        mixer->sumbuf[j] *= mult;
    }
    mixer->fill = maxsamp;
}

static void sdlaud_mixer_read( sdlaud_mixer_t* mixer, float* buf, int count ) {
    if ( mixer == 0 || buf == 0 || count <= 0 ) return;
    int pos = 0;
    while ( pos < count ) {
        int remain_target = count - pos;
        if ( mixer->rpos == 0 ) {
            int cnt = remain_target > MIXBUF_SAMP ? MIXBUF_SAMP : remain_target;
            sdlaud_mixer_mix( mixer, cnt );
        }
        int remain_source = mixer->fill - mixer->rpos;
        int tocopy = remain_source < remain_target ? remain_source : remain_target;
        memcpy( &buf[pos], &mixer->sumbuf[mixer->rpos], sizeof(float) * tocopy );
        pos += tocopy;
        mixer->rpos += tocopy;
        if ( mixer->rpos >= mixer->fill ) {
            mixer->rpos = 0;
        }
    }
}

static SDL_AudioSpec sdlaud_spec_in, sdlaud_spec_out;
static SDL_AudioDeviceID sdlaud_id;
static sdlaud_mixer_t sdlaud_mixer;

static void sdlaud_callback( void* userdata, Uint8* stream, int len ) {
    sdlaud_mixer_read( &sdlaud_mixer, (float*) stream, len / sizeof(float) );
}

static bool sdlaud_init_sdl( void ) {
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
    sdlaud_initmixer( &sdlaud_mixer );
    SDL_PauseAudioDevice( sdlaud_id, 0 );
    return true;
}

static void sdlaud_cleanup_sdl( void ) {
    SDL_PauseAudioDevice( sdlaud_id, 1 );
    SDL_CloseAudioDevice( sdlaud_id ); sdlaud_id = 0;
}

static bool sdlaud_initok = false;
static bool sdlaud_request_exit = false;
static SDL_Thread* sdlaud_workerthr = 0;

static int sdlaud_worker( void* arg ) {
    sdlaud_initok = false;
    sdlaud_request_exit = false;
    if ( !sdlaud_init_sdl() ) {
        goto ERR1;
    }
    sdlaud_initok = true;
    sdlev_raise( SDLEV_AUDIOWORKERINITDONE );

    while ( !sdlaud_request_exit ) {
        SDL_Delay( 20 );
    }

    sdlaud_initok = false;
    sdlaud_cleanup_sdl();
    sdlev_raise( SDLEV_AUDIOWORKERFINISHED );
    return 0;

ERR2:   sdlaud_cleanup_sdl();
ERR1:   return 0;
}

bool sdlaud_init( void ) {
    sdlaud_workerthr = SDL_CreateThread( sdlaud_worker, "sdlaud_worker", 0 );
    if ( sdlaud_workerthr == 0 ) {
        fprintf( stderr, "failed to create audio worker: %s\n", SDL_GetError() );
        return false;
    }
    for (;;) {
        int ev = sdlev_wait();
        switch ( ev ) {
            case SDLEV_ERROR:
                fprintf( stderr, "sdlaud_init(): sdlev_wait() returned error\n" );
                return false;
            case SDLEV_AUDIOWORKERINITDONE:
                return sdlaud_initok;
            default:    // SDLEV_SIGNAL, SDLEV_TIMEOUT, SDLEV_NONE
                break;
        }
    }
}

static bool sdlaud_cleanup2( void ) {

    int recent = sdlev_recent( 1 << SDLEV_AUDIOWORKERFINISHED );
    if ( recent & SDLEV_AUDIOWORKERFINISHED ) {
        goto THREAD_DONE;
    }

    sdlaud_request_exit = true;

    // wait for handshake (thread termination)
    for (;;) {
        int ev = sdlev_wait();
        switch ( ev ) {
            case SDLEV_ERROR:
                fprintf( stderr, "sdlaud_cleanup2(): sdlev_wait() returned error\n" );
                // thread might still be running!
                return false;
            case SDLEV_AUDIOWORKERFINISHED:
                goto THREAD_DONE;
            default:    // SDLEV_SIGNAL, SDLEV_TIMEOUT, SDLEV_NONE
                break;
        }
    }

    // wait for thread to terminate and reap the thread status
THREAD_DONE:
    int rv = 0;
    SDL_WaitThread( sdlaud_workerthr, &rv ); sdlaud_workerthr = 0;
    return true;
}

void sdlaud_cleanup( void ) {
    for (;;) {
        if ( sdlaud_cleanup2() ) break;
        // check if the thread ran into cleanup processing
        if ( !sdlaud_initok ) {
            // yes: wait for thread to terminate
            int rv = 0;
            SDL_WaitThread( sdlaud_workerthr, &rv ); sdlaud_workerthr = 0;
            break;
        }
        // event processing failed, cannot exit
        SDL_Delay( 1000 );
    }
}
