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
// Automated test of the synthesizer core (synth.c, no sound device): exit status = number of failures.

#include "synth.h"
#include "mml.h"

#define RATE    44100
#define MS      UINT64_C(1000000)
#define SEC     UINT64_C(1000000000)

static int total = 0, failures = 0;

static void check( bool cond, const char* name ) {
    ++total;
    if ( !cond ) {
        printf( "FAIL  %s\n", name );
        ++failures;
    }
}

static bool near( double a, double b, double tol ) {
    return fabs( a - b ) <= tol;
}

// renders n samples of a note on channel 0 and returns them (caller frees)
static float* tone( synth_wave_t wave, float freq, float vol, int n ) {
    synth_t s; synth_init( &s, RATE );
    synth_schedule( &s, 0, SYNTH_EV_WAVE, 0, 0, 0, wave );
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, freq, vol, SYNTH_SINE );
    synth_runevents( &s, 0 );
    float* buf = (float*) malloc( sizeof(float) * (size_t) n );
    synth_render( &s, buf, n );
    synth_cleanup( &s );
    return buf;
}

// frequency from the rising zero crossings after the fade-in (interpolated, so it is exact to a fraction of a sample)
static double measure( const float* buf, int n ) {
    int first = -1, last = -1, cnt = 0;
    double pfirst = 0, plast = 0;
    for ( int i=1000; i < n-1; ++i ) {
        if ( buf[i] < 0.0f && buf[i+1] >= 0.0f ) {
            double pos = i + (double)( -buf[i] ) / (double)( buf[i+1] - buf[i] );
            if ( first < 0 ) { first = i; pfirst = pos; }
            last = i; plast = pos; ++cnt;
        }
    }
    return cnt > 2 && last > first ? RATE * (double)( cnt - 1 ) / ( plast - pfirst ) : 0.0;
}

static float peak( const float* buf, int from, int to ) {
    float m = 0.0f;
    for ( int i=from; i < to; ++i ) {
        if ( fabsf( buf[i] ) > m ) m = fabsf( buf[i] );
    }
    return m;
}

static void test_notes( void ) {
    check( near( synth_notefreq( 0 ), 110.0, 0.001 ), "note 0 = 110 Hz" );
    check( near( synth_notefreq( 12 ), 220.0, 0.001 ), "note 12 = 220 Hz" );
    check( near( synth_notefreq( 24 ), 440.0, 0.001 ), "note 24 = 440 Hz" );
    check( synth_notefreq( -1 ) == 0.0f, "note -1 is invalid" );
    check( synth_notefreq( SYNTH_NOTES ) == 0.0f, "note 72 is invalid" );
    check( synth_notefreq( SYNTH_NOTES - 1 ) > 6000.0f, "note 71 is high" );
}

static void test_pitch( void ) {
    static const float freqs[] = { 20.0f, 110.0f, 440.0f, 1000.0f, 3300.0f, 6000.0f, 10000.0f };
    static const synth_wave_t waves[] = { SYNTH_SINE, SYNTH_SQUARE, SYNTH_TRIANGLE };
    static const char* names[] = { "sine", "square", "triangle" };
    for ( int w=0; w < 3; ++w ) {
        for ( size_t f=0; f < sizeof(freqs) / sizeof(freqs[0]); ++f ) {
            int n = freqs[f] < 100.0f ? 44100 * 3 : 44100;
            float* buf = tone( waves[w], freqs[f], 0.5f, n );
            double got = measure( buf, n );
            char name[100]; snprintf( name, sizeof(name), "%s %.0f Hz is exact (got %.3f)", names[w], freqs[f], got );
            check( near( got, freqs[f], freqs[f] * 0.0005 ), name );   // the old generator was up to 2.8 % off
            free( buf );
        }
    }
}

static void test_shapes( void ) {
    int n = 44100;
    float* sine = tone( SYNTH_SINE, 440.0f, 0.5f, n );
    check( near( peak( sine, 2000, n ), 0.5, 0.002 ), "sine peak = volume" );
    free( sine );
    float* tri = tone( SYNTH_TRIANGLE, 440.0f, 0.8f, n );
    check( near( peak( tri, 2000, n ), 0.8, 0.02 ), "triangle peak = volume" );
    free( tri );
    float* sq = tone( SYNTH_SQUARE, 441.0f, 1.0f, n );
    int others = 0;
    for ( int i=2000; i < n; ++i ) {
        if ( !near( fabsf( sq[i] ), 0.5, 1e-6 ) ) ++others;
    }
    check( others == 0, "square has two levels (+-0.5 at full volume)" );
    free( sq );
    float* noise = tone( SYNTH_NOISE, 2000.0f, 1.0f, n );
    double sum = 0; int pos = 0, neg = 0, changes = 0;
    for ( int i=2000; i < n; ++i ) {
        sum += noise[i];
        if ( noise[i] > 0 ) ++pos; else if ( noise[i] < 0 ) ++neg;
        if ( noise[i] != noise[i-1] ) ++changes;
    }
    check( pos > 8000 && neg > 8000, "noise has both signs" );
    check( near( sum / ( n - 2000 ), 0.0, 0.05 ), "noise has no offset" );
    check( changes > 1500 && changes < 2000, "noise changes at the channel's frequency (sample and hold)" );
    check( peak( noise, 2000, n ) <= 0.5f, "noise stays within +-0.5" );
    free( noise );
}

static void test_silence_and_clicks( void ) {
    synth_t s; synth_init( &s, RATE );
    float buf[2048];
    synth_render( &s, buf, 2048 );
    check( peak( buf, 0, 2048 ) == 0.0f, "no events: silence" );
    // a note at full volume fades in without a jump: the largest step between samples is small
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 440.0f, 1.0f, SYNTH_SINE );
    synth_runevents( &s, 0 );
    float big[8192];
    synth_render( &s, big, 8192 );
    float maxstep = 0.0f;
    for ( int i=1; i < 8192; ++i ) {
        if ( fabsf( big[i] - big[i-1] ) > maxstep ) maxstep = fabsf( big[i] - big[i-1] );
    }
    check( maxstep < 0.07f, "starting a note doesn't click" );
    check( peak( big, 500, 8192 ) > 0.99f, "full volume is reached" );
    // stop: fades out quickly and stays silent
    synth_schedule( &s, 0, SYNTH_EV_STOP, 0, 0, 0, SYNTH_SINE );
    synth_runevents( &s, 0 );
    synth_render( &s, big, 8192 );
    maxstep = 0.0f;
    for ( int i=1; i < 8192; ++i ) {
        if ( fabsf( big[i] - big[i-1] ) > maxstep ) maxstep = fabsf( big[i] - big[i-1] );
    }
    check( maxstep < 0.07f, "stopping a note doesn't click" );
    check( peak( big, 300, 8192 ) == 0.0f, "stopped: silent after the fade" );
    // a note on the same channel again starts from the beginning of the cycle
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 440.0f, 0.5f, SYNTH_SINE );
    synth_runevents( &s, 0 );
    synth_render( &s, big, 64 );
    check( fabsf( big[0] ) < 0.001f, "a new note starts at phase 0" );
    synth_cleanup( &s );
}

static void test_legato( void ) {
    // changing the pitch of a sounding note keeps the phase: no jump
    synth_t s; synth_init( &s, RATE );
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 440.0f, 0.5f, SYNTH_SINE );
    synth_runevents( &s, 0 );
    float buf[4000];
    synth_render( &s, buf, 3000 );
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 450.0f, 0.5f, SYNTH_SINE );
    synth_runevents( &s, 0 );
    synth_render( &s, buf + 3000, 1000 );
    float maxstep = 0.0f;
    for ( int i=2900; i < 3100; ++i ) {
        if ( fabsf( buf[i] - buf[i-1] ) > maxstep ) maxstep = fabsf( buf[i] - buf[i-1] );
    }
    check( maxstep < 0.07f, "pitch change of a sounding note is seamless" );
    synth_cleanup( &s );
}

static void test_mix( void ) {
    // 4 channels at full volume: scaled down, never above 1
    synth_t s; synth_init( &s, RATE );
    for ( int c=0; c < 4; ++c ) {
        synth_schedule( &s, 0, SYNTH_EV_PLAY, c, 200.0f * (float)( c + 1 ), 1.0f, SYNTH_SINE );
    }
    synth_runevents( &s, 0 );
    float* buf = (float*) malloc( sizeof(float) * 20000 );
    synth_render( &s, buf, 20000 );
    check( peak( buf, 0, 20000 ) <= 1.0f, "chord of four loud notes doesn't clip" );
    check( peak( buf, 1000, 20000 ) > 0.5f, "chord is audible" );
    free( buf );
    // a quiet chord isn't scaled
    synth_t q; synth_init( &q, RATE );
    synth_schedule( &q, 0, SYNTH_EV_PLAY, 0, 440.0f, 0.2f, SYNTH_SINE );
    synth_schedule( &q, 0, SYNTH_EV_PLAY, 1, 440.0f, 0.2f, SYNTH_SINE );
    synth_runevents( &q, 0 );
    float b2[4000]; synth_render( &q, b2, 4000 );
    check( near( peak( b2, 1000, 4000 ), 0.4, 0.005 ), "two notes of 0.2 add up to 0.4" );
    synth_cleanup( &q ); synth_cleanup( &s );
}

static void test_cursors( void ) {
    synth_t s; synth_init( &s, RATE );
    check( synth_beatsnsec( &s, 1.0f ) == SEC, "1 beat at 60 bpm = 1 s" );
    check( synth_beatsnsec( &s, 0.5f ) == SEC / 2U, "half a beat = 0.5 s" );
    synth_setbpm( &s, 120 );
    check( synth_beatsnsec( &s, 1.0f ) == SEC / 2U, "1 beat at 120 bpm = 0.5 s" );
    synth_setbpm( &s, 0 ); synth_setbpm( &s, 5000 );
    check( s.bpm == 120, "invalid tempo is ignored" );
    synth_setbpm( &s, 60 );
    uint64_t t0 = 10 * SEC;
    check( synth_slot( &s, 0, 1.0f, t0 ) == t0, "first event starts now" );
    check( synth_slot( &s, 0, 2.0f, t0 + MS ) == t0 + SEC, "the next one follows after the duration" );
    check( synth_slot( &s, 0, 1.0f, t0 + 2 * MS ) == t0 + 3 * SEC, "and the next after that" );
    check( synth_slot( &s, 1, 1.0f, t0 + 2 * MS ) == t0 + 2 * MS, "channels have their own cursors" );
    check( synth_slot( &s, -1, 1.0f, t0 ) == ~UINT64_C(0) && synth_slot( &s, SYNTH_NCHAN, 1.0f, t0 ) == ~UINT64_C(0),
        "invalid channels are refused" );
    // stale cursor: the sequence ran out long ago, the next event starts now (not in the past, not in a burst)
    uint64_t late = t0 + 100 * SEC;
    check( synth_slot( &s, 0, 1.0f, late ) == late, "a lagging cursor restarts at the current time" );
    check( synth_slot( &s, 0, 1.0f, late + MS ) == late + SEC, "and the sequence goes on from there" );
    // sync: cursors of the masked channels go to the latest one
    synth_slot( &s, 2, 3.0f, late ); synth_slot( &s, 3, 1.0f, late );
    synth_sync( &s, 0x0C | 0x01, late );        // channels 0, 2, 3
    check( s.cursor[0] == late + 3 * SEC && s.cursor[2] == late + 3 * SEC && s.cursor[3] == late + 3 * SEC,
        "sync: masked cursors line up with the latest" );
    check( s.cursor[1] != late + 3 * SEC, "sync: other channels are left alone" );
    synth_cleanup( &s );
}

static void test_queue( void ) {
    synth_t s; synth_init( &s, RATE );
    uint64_t when = 0;
    check( !synth_nextevent( &s, &when ), "empty queue" );
    // chan 0 has a far-ahead event queued first, chan 1's earlier one afterwards: it must not wait for it
    synth_schedule( &s, 5 * SEC, SYNTH_EV_PLAY, 0, 300.0f, 0.5f, SYNTH_SINE );
    synth_schedule( &s, 1 * SEC, SYNTH_EV_PLAY, 1, 400.0f, 0.5f, SYNTH_SINE );
    synth_schedule( &s, 3 * SEC, SYNTH_EV_STOP, 1, 0, 0, SYNTH_SINE );
    check( synth_nextevent( &s, &when ) && when == 1 * SEC, "earliest event first, whatever the queueing order" );
    check( synth_runevents( &s, 0 ) == 0 && s.chan[1].target == 0.0f, "nothing before its time" );
    check( synth_runevents( &s, 2 * SEC ) == 1 && s.chan[1].target == 0.5f && s.chan[0].target == 0.0f,
        "channel 1 plays while channel 0 waits" );
    check( synth_runevents( &s, 4 * SEC ) == 1 && s.chan[1].target == 0.0f, "stop at 3 s" );
    check( synth_runevents( &s, 6 * SEC ) == 1 && s.chan[0].target == 0.5f, "channel 0 starts at 5 s" );
    // equal times keep the order queued: play, then stop = stopped; stop, then play = playing
    synth_schedule( &s, 7 * SEC, SYNTH_EV_PLAY, 2, 300.0f, 0.5f, SYNTH_SINE );
    synth_schedule( &s, 7 * SEC, SYNTH_EV_STOP, 2, 0, 0, SYNTH_SINE );
    synth_schedule( &s, 7 * SEC, SYNTH_EV_STOP, 3, 0, 0, SYNTH_SINE );
    synth_schedule( &s, 7 * SEC, SYNTH_EV_PLAY, 3, 300.0f, 0.5f, SYNTH_SINE );
    synth_runevents( &s, 7 * SEC );
    check( s.chan[2].target == 0.0f && s.chan[3].target == 0.5f, "events of the same time keep their order" );
    // invalid events
    check( !synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 19.0f, 0.5f, SYNTH_SINE ), "frequency below 20 Hz refused" );
    check( !synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 20001.0f, 0.5f, SYNTH_SINE ), "frequency above 20 kHz refused" );
    check( !synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 440.0f, 1.5f, SYNTH_SINE ), "volume above 1 refused" );
    check( !synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 440.0f, -0.1f, SYNTH_SINE ), "negative volume refused" );
    check( !synth_schedule( &s, 0, SYNTH_EV_PLAY, 8, 440.0f, 0.5f, SYNTH_SINE ), "channel 8 refused" );
    check( !synth_schedule( &s, 0, SYNTH_EV_WAVE, 0, 0, 0, SYNTH_NWAVES ), "unknown wave refused" );
    check( s.nevents == 0, "refused events aren't queued" );
    // the queue is limited
    int ok = 0;
    for ( int i=0; i < SYNTH_MAXEVENTS + 10; ++i ) {
        if ( synth_schedule( &s, (uint64_t) i, SYNTH_EV_STOP, 0, 0, 0, SYNTH_SINE ) ) ++ok;
    }
    check( ok == SYNTH_MAXEVENTS, "the queue holds SYNTH_MAXEVENTS events" );
    // silence: drops the queue, fades out, resets the cursors
    synth_slot( &s, 0, 5.0f, 100 * SEC );
    synth_silence( &s, 200 * SEC );
    check( s.nevents == 0 && s.cursor[0] == 200 * SEC && s.chan[0].target == 0.0f, "synth_silence() clears everything" );
    synth_cleanup( &s );
}

static void test_chord_sequence( void ) {
    // a melody of 3 notes on channel 0 and a bass note on channel 1, queued in one go
    synth_t s; synth_init( &s, RATE );
    uint64_t now = 50 * SEC;
    synth_sync( &s, 0x03, now );
    uint64_t a = synth_slot( &s, 0, 1.0f, now ), b = synth_slot( &s, 0, 1.0f, now ), c = synth_slot( &s, 0, 2.0f, now );
    uint64_t bass = synth_slot( &s, 1, 4.0f, now );
    synth_schedule( &s, a, SYNTH_EV_PLAY, 0, 440.0f, 0.5f, SYNTH_SINE );
    synth_schedule( &s, b, SYNTH_EV_PLAY, 0, 550.0f, 0.5f, SYNTH_SINE );
    synth_schedule( &s, c, SYNTH_EV_PLAY, 0, 660.0f, 0.5f, SYNTH_SINE );
    synth_schedule( &s, synth_slot( &s, 0, 0.0f, now ), SYNTH_EV_STOP, 0, 0, 0, SYNTH_SINE );
    synth_schedule( &s, bass, SYNTH_EV_PLAY, 1, 110.0f, 0.5f, SYNTH_SINE );
    check( a == bass && b == now + SEC && c == now + 2 * SEC, "melody and bass line start together, then follow the beat" );
    synth_runevents( &s, now );
    check( s.chan[0].osc.step * RATE > 439.9 && s.chan[0].osc.step * RATE < 440.1 && s.chan[1].target == 0.5f, "both start at once" );
    synth_runevents( &s, now + SEC );
    check( s.chan[0].osc.step * RATE > 549.9 && s.chan[0].osc.step * RATE < 550.1, "second note after 1 beat" );
    synth_runevents( &s, now + 4 * SEC );
    check( s.chan[0].target == 0.0f, "the stop at the end of the melody" );
    synth_cleanup( &s );
}

static bool mml_one( const char* text, mml_event_t* ev0, size_t* pcount ) {
    mml_t m; char err[MML_ERRLEN];
    bool ok = mml_parse( text, &m, err );
    if ( ok ) {
        *pcount = m.count;
        if ( m.count > 0 && ev0 ) *ev0 = m.events[0];
        mml_free( &m );
    }
    return ok;
}

static bool mml_tone( const char* text, double freq, double seconds, double gap ) {
    mml_event_t ev; size_t n = 0;
    return mml_one( text, &ev, &n ) && n == 1 && ev.kind == MML_TONE && near( ev.freq, freq, freq * 0.0001 ) &&
        near( ev.seconds, seconds, 1e-5 ) && near( ev.gap, gap, 1e-5 );
}

static bool mml_error( const char* text, const char* part ) {
    mml_t m; char err[MML_ERRLEN];
    bool ok = mml_parse( text, &m, err );
    if ( ok ) {
        mml_free( &m );
        return false;
    }
    bool good = strstr( err, part ) != 0 && m.events == 0 && m.count == 0;
    if ( !good ) printf( "  mml \"%s\": got \"%s\"\n", text, err );
    return good;
}

static void test_mml( void ) {
    check( mml_tone( "C", 261.626, 0.4375, 0.0625 ), "mml: C = middle C, quarter note at T120, normal articulation" );
    check( mml_tone( "ML C", 261.626, 0.5, 0.0 ), "mml: legato fills the note" );
    check( mml_tone( "MS C", 261.626, 0.375, 0.125 ), "mml: staccato" );
    check( mml_tone( "ML A", 440.0, 0.5, 0.0 ), "mml: A in octave 4 = 440 Hz" );
    check( mml_tone( "ML O5 A", 880.0, 0.5, 0.0 ) && mml_tone( "ML >A", 880.0, 0.5, 0.0 ) && mml_tone( "ML <A", 220.0, 0.5, 0.0 ),
        "mml: O, > and <" );
    check( mml_tone( "ML C#", 277.183, 0.5, 0.0 ) && mml_tone( "ML C+", 277.183, 0.5, 0.0 ) && mml_tone( "ML D-", 277.183, 0.5, 0.0 ),
        "mml: sharps and flats" );
    check( mml_tone( "ML B#", 523.251, 0.5, 0.0 ), "mml: B# is the C above" );
    check( mml_tone( "ML C8", 261.626, 0.25, 0.0 ) && mml_tone( "ML C1", 261.626, 2.0, 0.0 ) && mml_tone( "ML C64", 261.626, 0.03125, 0.0 ),
        "mml: note lengths" );
    check( mml_tone( "ML C8.", 261.626, 0.375, 0.0 ) && mml_tone( "ML C4..", 261.626, 0.875, 0.0 ), "mml: dots" );
    check( mml_tone( "ML L8 C", 261.626, 0.25, 0.0 ) && mml_tone( "ML L8 C4", 261.626, 0.5, 0.0 ), "mml: default length" );
    check( mml_tone( "T60 ML C", 261.626, 1.0, 0.0 ) && mml_tone( "T240 ML C", 261.626, 0.25, 0.0 ), "mml: tempo" );
    check( mml_tone( "ml c", 261.626, 0.5, 0.0 ), "mml: lower case" );
    mml_event_t ev; size_t n = 0;
    check( mml_one( "R2", &ev, &n ) && n == 1 && ev.kind == MML_REST && near( ev.seconds, 1.0, 1e-6 ), "mml: rest" );
    check( mml_one( "P", &ev, &n ) && ev.kind == MML_REST && near( ev.seconds, 0.5, 1e-6 ), "mml: P is a rest too" );
    check( mml_one( "V0.25 C", &ev, &n ) && near( ev.vol, 0.25, 1e-6 ) && mml_one( "V.5 C", &ev, &n ) && near( ev.vol, 0.5, 1e-6 ) &&
           mml_one( "V1 C", &ev, &n ) && near( ev.vol, 1.0, 1e-6 ), "mml: volume with decimals" );
    check( mml_one( "W2", &ev, &n ) && n == 1 && ev.kind == MML_WAVE && ev.wave == 2, "mml: wave" );
    check( mml_one( "c ; d\n e", &ev, &n ) && n == 3, "mml: blanks, semicolons and newlines are ignored" );
    check( mml_one( "", &ev, &n ) && n == 0 && mml_one( "  ; ", &ev, &n ) && n == 0, "mml: an empty string is fine" );
    check( mml_one( "O8 B", &ev, &n ) && near( ev.freq, 7902.13, 0.1 ), "mml: highest octave" );
    check( mml_one( "O1 C", &ev, &n ) && near( ev.freq, 32.703, 0.01 ), "mml: low octave" );
    check( mml_error( "X", "Unknown music command 'X'" ), "mml: unknown command" );
    check( mml_error( "C D X", "'X'" ), "mml: error in the middle" );
    check( mml_error( "O9", "out of range" ) && mml_error( "O", "out of range" ), "mml: bad octave" );
    check( mml_error( "L0", "out of range" ) && mml_error( "L65", "out of range" ), "mml: bad default length" );
    check( mml_error( "T0", "out of range" ) && mml_error( "T1001", "out of range" ), "mml: bad tempo" );
    check( mml_error( "V2", "out of range" ) && mml_error( "V", "out of range" ), "mml: bad volume" );
    check( mml_error( "W4", "out of range" ), "mml: bad wave" );
    check( mml_error( "C0", "length" ) && mml_error( "C65", "length" ) && mml_error( "R99", "length" ), "mml: bad note length" );
    check( mml_error( "M", "ML, MN or MS" ) && mml_error( "MX", "ML, MN or MS" ), "mml: bad articulation" );
    check( mml_error( "O0 C", "Note out of range" ), "mml: note below 20 Hz" );
    char* big = (char*) malloc( MML_MAXEVENTS + 2U );
    memset( big, 'R', MML_MAXEVENTS + 1U ); big[ MML_MAXEVENTS + 1U ] = '\0';
    check( mml_error( big, "too long" ), "mml: more events than allowed" );
    big[ MML_MAXEVENTS ] = '\0';
    check( mml_one( big, &ev, &n ) && n == MML_MAXEVENTS, "mml: the allowed number of events" );
    free( big );
}

static void test_busy( void ) {
    synth_t s; synth_init( &s, RATE );
    check( !synth_busy( &s, 0, 0 ) && !synth_busy( &s, -1, 0 ) && !synth_busy( &s, SYNTH_NCHAN, 0 ), "busy: a new synth is idle" );
    synth_schedule( &s, 5 * SEC, SYNTH_EV_PLAY, 2, 440.0f, 0.5f, SYNTH_SINE );
    check( synth_busy( &s, 2, 0 ) && !synth_busy( &s, 1, 0 ), "busy: a queued event keeps its channel busy" );
    synth_runevents( &s, 5 * SEC );
    check( synth_busy( &s, 2, 5 * SEC ), "busy: a sounding channel" );
    synth_schedule( &s, 6 * SEC, SYNTH_EV_STOP, 2, 0, 0, SYNTH_SINE );
    synth_runevents( &s, 6 * SEC );
    float buf[500]; synth_render( &s, buf, 500 );
    check( !synth_busy( &s, 2, 6 * SEC ), "busy: idle again after the stop faded out" );
    synth_slot( &s, 3, 2.0f, 10 * SEC );
    check( synth_busy( &s, 3, 11 * SEC ) && !synth_busy( &s, 3, 12 * SEC ), "busy: a sequence that still runs (a rest at the end)" );
    synth_cleanup( &s );
}

static void test_silencechan( void ) {
    synth_t s; synth_init( &s, RATE );
    uint64_t when = 0;
    synth_schedule( &s, 1 * SEC, SYNTH_EV_PLAY, 0, 440.0f, 0.5f, SYNTH_SINE );
    synth_schedule( &s, 2 * SEC, SYNTH_EV_PLAY, 1, 440.0f, 0.5f, SYNTH_SINE );
    synth_schedule( &s, 3 * SEC, SYNTH_EV_STOP, 0, 0, 0, SYNTH_SINE );
    synth_slot( &s, 0, 10.0f, SEC );
    synth_runevents( &s, 1 * SEC );
    synth_silencechan( &s, 0, 1 * SEC );
    check( s.nevents == 1 && s.chan[0].target == 0.0f && s.cursor[0] == 1 * SEC, "silencechan: removes the channel's events, stops it, resets its cursor" );
    check( synth_nextevent( &s, &when ) && when == 2 * SEC, "silencechan: other channels keep theirs" );
    synth_schedule( &s, 4 * SEC, SYNTH_EV_STOP, 1, 0, 0, SYNTH_SINE );        // the tail is the last remaining event
    synth_schedule( &s, 1 * SEC, SYNTH_EV_WAVE, 2, 0, 0, SYNTH_SQUARE );
    check( synth_runevents( &s, 3 * SEC ) == 2 && s.nevents == 1, "silencechan: the queue still works (order, tail)" );
    synth_silencechan( &s, 1, 5 * SEC );
    check( s.nevents == 0 && s.tail == 0 && s.events == 0, "silencechan: emptying the queue clears the tail" );
    synth_silencechan( &s, 9, 0 );
    // out-of-order queueing keeps the order; the tail stays the last
    synth_schedule( &s, 5 * SEC, SYNTH_EV_STOP, 0, 0, 0, SYNTH_SINE );
    synth_schedule( &s, 1 * SEC, SYNTH_EV_STOP, 1, 0, 0, SYNTH_SINE );
    synth_schedule( &s, 3 * SEC, SYNTH_EV_STOP, 2, 0, 0, SYNTH_SINE );
    synth_schedule( &s, 9 * SEC, SYNTH_EV_STOP, 3, 0, 0, SYNTH_SINE );
    synth_schedule( &s, 9 * SEC, SYNTH_EV_STOP, 4, 0, 0, SYNTH_SINE );
    check( s.tail->chan == 4 && s.events->chan == 1 && s.events->next->chan == 2 && s.events->next->next->chan == 0, "queue: sorted, appends go to the tail" );
    synth_cleanup( &s );
}

static void test_queue_speed( void ) {
    // a long sequence in time order must be cheap to queue (no search through the queue)
    synth_t s; synth_init( &s, RATE );
    clock_t t0 = clock();
    for ( int i=0; i < 200000; ++i ) {
        synth_schedule( &s, (uint64_t) i * MS, SYNTH_EV_PLAY, i % 8, 440.0f, 0.5f, SYNTH_SINE );
    }
    double secs = (double)( clock() - t0 ) / CLOCKS_PER_SEC;
    check( s.nevents == 200000 && secs < 1.0, "queueing 200000 events in time order is fast" );
    synth_cleanup( &s );
}

int main( void ) {
    test_notes();
    test_pitch();
    test_shapes();
    test_silence_and_clicks();
    test_legato();
    test_mix();
    test_cursors();
    test_queue();
    test_chord_sequence();
    test_mml();
    test_busy();
    test_silencechan();
    test_queue_speed();
    printf( "%d checks, %d failure(s)\n", total, failures );
    return failures;
}
