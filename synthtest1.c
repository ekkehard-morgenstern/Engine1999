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
    check( !synth_schedule( &s, 0, SYNTH_EV_PLAY, SYNTH_NCHAN, 440.0f, 0.5f, SYNTH_SINE ), "the channel after the last one is refused" );
    check( synth_schedule( &s, 0, SYNTH_EV_PLAY, SYNTH_NCHAN - 1, 440.0f, 0.5f, SYNTH_SINE ) && s.nevents == 1, "channel 16 is accepted" );
    synth_silence( &s, 0 );
    check( !synth_schedule( &s, 0, SYNTH_EV_WAVE, 0, 0, 0, (synth_wave_t) SYNTH_NINST ), "unknown wave / instrument refused" );
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
    check( mml_error( "Z", "Unknown music command 'Z'" ), "mml: unknown command" );
    check( mml_error( "C D Z", "'Z'" ), "mml: error in the middle" );
    check( mml_error( "O9", "out of range" ) && mml_error( "O", "out of range" ), "mml: bad octave" );
    check( mml_error( "L0", "out of range" ) && mml_error( "L65", "out of range" ), "mml: bad default length" );
    check( mml_error( "T0", "out of range" ) && mml_error( "T1001", "out of range" ), "mml: bad tempo" );
    check( mml_error( "V2", "out of range" ) && mml_error( "V", "out of range" ), "mml: bad volume" );
    check( mml_error( "W32", "out of range" ) && mml_error( "I32", "out of range" ) && mml_error( "I", "out of range" ), "mml: bad wave / instrument" );
    {
        mml_t m; char err[MML_ERRLEN];
        check( mml_parse( "W4 C I31 D w0 E", &m, err ) && m.count == 6 + 0 && m.events[0].kind == MML_WAVE && m.events[0].wave == 4 &&
               m.events[2].wave == 31 && m.events[4].wave == 0, "mml: W and I select 0..31" );
        mml_free( &m );
    }
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

// frequency in a window of samples (zero crossings, interpolated); 0 if there are fewer than 3 crossings
static double measure_window( const float* buf, int from, int to ) {
    int first = -1, last = -1, cnt = 0;
    double pfirst = 0, plast = 0;
    for ( int i=from; i < to-1; ++i ) {
        if ( buf[i] < 0.0f && buf[i+1] >= 0.0f ) {
            double pos = i + (double)( -buf[i] ) / (double)( buf[i+1] - buf[i] );
            if ( first < 0 ) { first = i; pfirst = pos; }
            last = i; plast = pos; ++cnt;
        }
    }
    return cnt > 2 && last > first ? RATE * (double)( cnt - 1 ) / ( plast - pfirst ) : 0.0;
}

static synth_evspec_t spec( synth_evtype_t type, float freq, float vol ) {
    synth_evspec_t sp;
    memset( &sp, 0, sizeof(sp) );
    sp.type = type; sp.freq = freq; sp.vol = vol;
    return sp;
}

static void test_glide( void ) {
    synth_t s; synth_init( &s, RATE );
    synth_evspec_t sp = spec( SYNTH_EV_PLAY, 400.0f, 0.5f );
    sp.freq2 = 800.0f; sp.dur = SEC;
    check( synth_schedule_ev( &s, 0, &sp ), "glide: event accepted" );
    synth_runevents( &s, 0 );
    float* buf = (float*) malloc( sizeof(float) * 2 * RATE );
    synth_render( &s, buf, 2 * RATE );
    double f1 = measure_window( buf, 2000, 4000 ), fm = measure_window( buf, 20000, 24000 );
    double f2 = measure_window( buf, 40000, 42000 ), f3 = measure_window( buf, 60000, 88000 );
    check( near( f1, 400.0 * pow( 2.0, 0.07 ), 8.0 ), "glide: starts at the first frequency" );
    check( near( fm, 400.0 * sqrt( 2.0 ), 8.0 ), "glide: exponential, the middle is the geometric mean" );
    check( near( f2, 400.0 * pow( 2.0, 0.93 ), 12.0 ), "glide: nears the end frequency" );
    check( near( f3, 800.0, 0.5 ), "glide: stays on the end frequency" );
    free( buf );
    sp.freq2 = 800.0f; sp.dur = 0;
    check( !synth_schedule_ev( &s, 0, &sp ), "glide: needs a duration" );
    sp.dur = SEC; sp.freq2 = 10.0f;
    check( !synth_schedule_ev( &s, 0, &sp ), "glide: end frequency range" );
    // a glide down, and silencing stops it
    sp.freq2 = 100.0f; sp.freq = 1600.0f;
    synth_schedule_ev( &s, 0, &sp );
    synth_runevents( &s, 0 );
    check( s.chan[0].glide_left > 0, "glide: running" );
    synth_silence( &s, 0 );
    check( s.chan[0].glide_left == 0 && !s.chan[0].on, "glide: synth_silence() stops it" );
    synth_cleanup( &s );
}

static void test_envelope( void ) {
    synth_t s; synth_init( &s, RATE );
    synth_evspec_t env = spec( SYNTH_EV_ENV, 0, 0 );
    env.p[0] = 0.1f; env.p[1] = 0.1f; env.p[2] = 0.5f; env.p[3] = 0.2f;
    check( synth_schedule_ev( &s, 0, &env ), "envelope: event accepted" );
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 441.0f, 1.0f, SYNTH_SINE );
    synth_runevents( &s, 0 );
    float* buf = (float*) malloc( sizeof(float) * RATE * 2 );
    synth_render( &s, buf, RATE / 2 );          // 0.5 s: attack 0.1, decay 0.1, then sustain
    check( peak( buf, 2000, 2400 ) > 0.4f && peak( buf, 2000, 2400 ) < 0.6f, "envelope: halfway up at half the attack" );
    check( peak( buf, 4300, 4700 ) > 0.95f, "envelope: full level at the end of the attack" );
    check( near( peak( buf, 13000, 13500 ), 0.5, 0.03 ) && near( peak( buf, 20000, 22000 ), 0.5, 0.01 ), "envelope: decays to the sustain level and holds it" );
    synth_schedule( &s, 0, SYNTH_EV_STOP, 0, 0, 0, SYNTH_SINE );
    synth_runevents( &s, 0 );
    check( s.chan[0].on && synth_busy( &s, 0, 0 ), "envelope: during the release the channel is still on" );
    synth_render( &s, buf + RATE / 2, RATE );
    check( near( peak( buf, RATE / 2 + 4300, RATE / 2 + 4700 ), 0.25, 0.04 ), "envelope: halfway down at half the release" );
    check( peak( buf, RATE / 2 + 10000, RATE / 2 + 20000 ) == 0.0f, "envelope: silent after the release" );
    check( !s.chan[0].on && !synth_busy( &s, 0, 0 ), "envelope: the channel is idle after the release" );
    // back to the default
    env.p[0] = -1.0f;
    synth_schedule_ev( &s, 0, &env );
    synth_runevents( &s, 0 );
    check( !s.chan[0].env_on, "envelope: off again" );
    env.p[0] = 0.0f; env.p[2] = 1.5f;
    check( !synth_schedule_ev( &s, 0, &env ), "envelope: sustain level range" );
    env.p[2] = 0.5f; env.p[3] = 61.0f;
    check( !synth_schedule_ev( &s, 0, &env ), "envelope: release time range" );
    // zero times: the level jumps; a retriggered note during the release goes up from where it is
    env.p[0] = 0.0f; env.p[1] = 0.0f; env.p[2] = 0.8f; env.p[3] = 0.0f;
    synth_schedule_ev( &s, 0, &env );
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 441.0f, 1.0f, SYNTH_SINE );
    synth_runevents( &s, 0 );
    synth_render( &s, buf, 5000 );
    check( near( peak( buf, 1000, 5000 ), 0.8, 0.02 ), "envelope: attack and decay of 0 give the sustain level at once" );
    synth_cleanup( &s );
    free( buf );
}

static void test_lfos( void ) {
    synth_t s; synth_init( &s, RATE );
    synth_evspec_t sp = spec( SYNTH_EV_VIB, 0, 0 );
    sp.p[0] = 5.0f; sp.p[1] = 1.0f;
    check( synth_schedule_ev( &s, 0, &sp ), "vibrato: event accepted" );
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 1000.0f, 1.0f, SYNTH_SINE );
    synth_runevents( &s, 0 );
    float* buf = (float*) malloc( sizeof(float) * RATE );
    synth_render( &s, buf, RATE );
    double lo = 1e9, hi = 0, sum = 0; int n = 0;
    for ( int w=2000; w + 400 < RATE; w += 400 ) {
        double f = measure_window( buf, w, w + 400 );
        if ( f > 0 ) { if ( f < lo ) lo = f; if ( f > hi ) hi = f; sum += f; ++n; }
    }
    check( lo < 950.0 && lo > 930.0 && hi > 1050.0 && hi < 1070.0, "vibrato: the pitch swings by +-1 semitone" );
    check( near( sum / n, 1000.0, 15.0 ), "vibrato: around the note's pitch" );
    sp.p[1] = 0.0f;
    synth_schedule_ev( &s, 0, &sp );
    synth_runevents( &s, 0 );
    synth_render( &s, buf, RATE / 2 );
    check( near( measure_window( buf, 1000, 20000 ), 1000.0, 1.0 ), "vibrato: depth 0 = off" );
    sp.p[0] = 5.0f; sp.p[1] = 25.0f;
    check( !synth_schedule_ev( &s, 0, &sp ), "vibrato: depth range" );
    sp.p[0] = 101.0f; sp.p[1] = 1.0f;
    check( !synth_schedule_ev( &s, 0, &sp ), "vibrato: rate range" );
    synth_cleanup( &s );
    // tremolo
    synth_init( &s, RATE );
    synth_evspec_t tr = spec( SYNTH_EV_TREM, 0, 0 );
    tr.p[0] = 4.0f; tr.p[1] = 0.5f;
    check( synth_schedule_ev( &s, 0, &tr ), "tremolo: event accepted" );
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 1000.0f, 1.0f, SYNTH_SINE );
    synth_runevents( &s, 0 );
    synth_render( &s, buf, RATE );
    float mn = 9.0f, mx = 0.0f;
    for ( int w=3000; w + 200 < RATE; w += 200 ) {
        float pk = peak( buf, w, w + 200 );
        if ( pk < mn ) mn = pk;
        if ( pk > mx ) mx = pk;
    }
    check( mx > 0.97f && mn > 0.45f && mn < 0.56f, "tremolo: the volume swings between 1 and 1 - depth" );
    tr.p[1] = 1.5f;
    check( !synth_schedule_ev( &s, 0, &tr ), "tremolo: depth range" );
    synth_cleanup( &s );
    free( buf );
}

static void test_immediate( void ) {
    synth_t s; synth_init( &s, RATE );
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 441.0f, 0.5f, SYNTH_SINE );
    synth_runevents( &s, 0 );
    float* buf = (float*) malloc( sizeof(float) * RATE );
    synth_render( &s, buf, 10000 );
    synth_setpitch( &s, 0, 882.0f );
    synth_render( &s, buf + 10000, 10000 );
    check( near( measure_window( buf, 3000, 9000 ), 441.0, 1.0 ) && near( measure_window( buf, 12000, 19000 ), 882.0, 1.0 ), "setpitch: changes the pitch of a sounding channel at once" );
    float maxstep = 0.0f;
    for ( int i=9990; i < 10100; ++i ) {
        if ( fabsf( buf[i] - buf[i-1] ) > maxstep ) maxstep = fabsf( buf[i] - buf[i-1] );
    }
    check( maxstep < 0.12f, "setpitch: no jump (the phase goes on)" );
    synth_setpitch( &s, 0, 19.0f );
    synth_setpitch( &s, 9, 440.0f );
    check( s.chan[0].osc.step * RATE > 881.0, "setpitch: invalid values are ignored" );
    synth_setvolume( &s, 0, 0.25f );
    synth_render( &s, buf, 5000 );
    check( near( peak( buf, 2000, 5000 ), 0.25, 0.005 ), "setvolume: changes the volume (faded in)" );
    synth_setvolume( &s, 0, 0.0f );
    synth_render( &s, buf, 5000 );
    check( peak( buf, 1000, 5000 ) == 0.0f && s.chan[0].on && synth_busy( &s, 0, 0 ), "setvolume 0: silent, but the note is still on" );
    synth_setvolume( &s, 0, 0.5f );
    synth_render( &s, buf, 5000 );
    check( near( peak( buf, 2000, 5000 ), 0.5, 0.01 ), "setvolume: the note can be made audible again" );
    synth_setvolume( &s, 0, 2.0f );
    check( s.chan[0].vol == 0.5f, "setvolume: invalid values are ignored" );
    // an event then sets the volume again; a stopped channel isn't started by setvolume
    synth_schedule( &s, 0, SYNTH_EV_STOP, 0, 0, 0, SYNTH_SINE );
    synth_runevents( &s, 0 );
    synth_render( &s, buf, 1000 );
    synth_setvolume( &s, 0, 0.9f );
    synth_render( &s, buf, 1000 );
    check( peak( buf, 0, 1000 ) == 0.0f && !s.chan[0].on, "setvolume: doesn't start a stopped channel" );
    synth_cleanup( &s );
    free( buf );
}

static void test_mml_effects( void ) {
    mml_t m; char err[MML_ERRLEN];
    check( mml_parse( "ML H440", &m, err ) && m.count == 1 && m.events[0].kind == MML_TONE && near( m.events[0].freq, 440.0, 0.01 ) &&
           m.events[0].freq2 == 0.0f && near( m.events[0].seconds, 0.5, 1e-6 ), "mml: H plays a frequency" );
    mml_free( &m );
    check( mml_parse( "ML H440.5,2.", &m, err ) && near( m.events[0].freq, 440.5, 0.01 ) && near( m.events[0].seconds, 1.5, 1e-6 ), "mml: H with decimals, a length and a dot" );
    mml_free( &m );
    check( mml_parse( "ML S200 , 3000 , 8", &m, err ) && m.count == 1 && near( m.events[0].freq, 200.0, 0.01 ) && near( m.events[0].freq2, 3000.0, 0.1 ) &&
           near( m.events[0].seconds, 0.25, 1e-6 ), "mml: S sweeps, blanks around the commas" );
    mml_free( &m );
    check( mml_parse( "S100,200", &m, err ) && near( m.events[0].seconds + m.events[0].gap, 0.5, 1e-6 ) && m.events[0].gap > 0.0f, "mml: S has the articulation too" );
    mml_free( &m );
    check( mml_parse( "X0.01,0.1,0.5,0.3 XR xr", &m, err ) && m.count == 3 && m.events[0].kind == MML_ENV && near( m.events[0].p[0], 0.01, 1e-6 ) &&
           near( m.events[0].p[3], 0.3, 1e-6 ) && m.events[1].p[0] < 0.0f && m.events[2].p[0] < 0.0f, "mml: X and XR" );
    mml_free( &m );
    check( mml_parse( "~5,0.5 ^8,0.25", &m, err ) && m.count == 2 && m.events[0].kind == MML_VIB && near( m.events[0].p[1], 0.5, 1e-6 ) &&
           m.events[1].kind == MML_TREM && near( m.events[1].p[0], 8.0, 1e-6 ), "mml: vibrato and tremolo" );
    mml_free( &m );
    check( mml_error( "H10", "'H'" ) && mml_error( "H20001", "'H'" ) && mml_error( "H", "'H'" ), "mml: H frequency range" );
    check( mml_error( "S100", "'S'" ) && mml_error( "S100,5", "'S'" ), "mml: S needs two frequencies" );
    check( mml_error( "H440,", "length" ) && mml_error( "H440,x", "length" ), "mml: H length after the comma" );
    check( mml_error( "X1,2,3", "'X'" ) && mml_error( "X61,0,1,0", "'X'" ) && mml_error( "X0,0,2,0", "'X'" ), "mml: X values" );
    check( mml_error( "~5", "'~'" ) && mml_error( "~5,25", "'~'" ) && mml_error( "^5,1.5", "'^'" ) && mml_error( "^101,1", "'^'" ), "mml: vibrato and tremolo values" );
}

// -- instruments -----------------------------------------------------------------------

// amplitude of the frequency f in buf[from..to) (Goertzel)
static double amp_at( const float* buf, int from, int to, double f ) {
    double w = 2.0 * 3.14159265358979 * f / RATE, c = 2.0 * cos( w ), s1 = 0, s2 = 0;
    for ( int i=from; i < to; ++i ) {
        double s0 = buf[i] + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    double re = s1 - s2 * cos( w ), im = s2 * sin( w );
    return 2.0 * sqrt( re * re + im * im ) / (double)( to - from );
}

// plays freq on channel 0 with instrument inst for hold seconds, then releases; renders total seconds
static float* inst_note( synth_t* s, int inst, float freq, float vol, double hold, double total_s, int* n ) {
    synth_init( s, RATE );
    synth_evspec_t sp; memset( &sp, 0, sizeof(sp) );
    sp.type = SYNTH_EV_INST; sp.p[0] = (float) inst;
    synth_schedule_ev( s, 0, &sp );
    synth_schedule( s, 0, SYNTH_EV_PLAY, 0, freq, vol, SYNTH_SINE );
    synth_schedule( s, (uint64_t)( hold * 1e9 ), SYNTH_EV_STOP, 0, 0, 0, SYNTH_SINE );
    synth_runevents( s, 0 );
    *n = (int)( total_s * RATE );
    float* buf = (float*) malloc( sizeof(float) * (size_t) *n );
    int done = 0;
    while ( done < *n ) {
        int chunk = *n - done < 441 ? *n - done : 441;
        synth_runevents( s, (uint64_t)( (double) done * 1e9 / RATE ) );
        synth_render( s, buf + done, chunk );
        done += chunk;
    }
    return buf;
}

static void test_instruments( void ) {
    synth_t s; synth_init( &s, RATE );
    // the table
    bool names_ok = true;
    for ( int i=0; i < SYNTH_NINST; ++i ) {
        const char* nm = synth_instname( &s, i );
        if ( nm[0] == '\0' ) names_ok = false;
        for ( int k=0; k < i; ++k ) {
            if ( i < SYNTH_NBASIC + SYNTH_NPRESET && strcmp( nm, synth_instname( &s, k ) ) == 0 ) names_ok = false;
        }
    }
    check( names_ok, "32 instruments have names, the first 20 distinct" );
    check( strcmp( synth_instname( &s, 2 ), "TRIANGLE" ) == 0 && strcmp( synth_instname( &s, 4 ), "BASS" ) == 0, "names of basic waves and presets" );
    check( synth_instname( &s, 32 )[0] == '\0' && synth_instname( &s, -1 )[0] == '\0', "names out of range are empty" );
    check( synth_getinst( &s, 3 ) == 0 && synth_getinst( &s, 4 ) != 0 && synth_getinst( &s, 31 ) != 0 && synth_getinst( &s, 32 ) == 0, "getinst range" );
    synth_cleanup( &s );

    // every preset sounds (finite, audible, not clipping), reaches silence after the release, and is not a copy of another
    double rms[SYNTH_NINST];
    for ( int i=SYNTH_NBASIC; i < SYNTH_NBASIC + SYNTH_NPRESET; ++i ) {
        int n; float* b = inst_note( &s, i, i == 13 || i == 16 ? 55.0f : 220.0f, 1.0f, 0.5, 6.0, &n );
        bool finite = true; float pk = 0.0f; double e = 0;
        for ( int k=0; k < n; ++k ) {
            if ( !isfinite( b[k] ) ) finite = false;
            if ( fabsf( b[k] ) > pk ) pk = fabsf( b[k] );
        }
        for ( int k=0; k < RATE / 2; ++k ) e += (double) b[k] * b[k];
        rms[i] = sqrt( e / ( RATE / 2 ) );
        char name[100];
        snprintf( name, sizeof(name), "preset %d %s sounds (peak %.2f)", i, synth_instname( &s, i ), pk );
        check( finite && pk > 0.05f && pk <= 1.0f && rms[i] > 0.005, name );
        snprintf( name, sizeof(name), "preset %d %s is silent after its release", i, synth_instname( &s, i ) );
        check( peak( b, n - 2000, n ) == 0.0f && !synth_busy( &s, 0, (uint64_t) 6 * SEC ), name );
        free( b ); synth_cleanup( &s );
    }

    // pitch of a user instrument: a sine at ratio 2 sounds an octave up; detune by 1200 cents the same
    synth_inst_t d; synth_instdefault( &d );
    d.osc[0].ratio = 2.0f;
    synth_init( &s, RATE );
    check( synth_setinst( &s, 20, &d ), "define instrument 20" );
    synth_evspec_t sp; memset( &sp, 0, sizeof(sp) ); sp.type = SYNTH_EV_INST; sp.p[0] = 20.0f;
    synth_schedule_ev( &s, 0, &sp );
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 440.0f, 0.5f, SYNTH_SINE );
    synth_runevents( &s, 0 );
    float* buf = (float*) malloc( sizeof(float) * RATE );
    synth_render( &s, buf, RATE );
    check( near( measure( buf, RATE ), 880.0, 0.5 ), "ratio 2 sounds an octave up" );
    check( near( peak( buf, 2000, RATE ), 0.5, 0.03 ) && peak( buf, 2000, RATE ) <= 0.5f, "a plain instrument peaks at about the volume (the soft limiter takes a few % off the top)" );
    synth_cleanup( &s );
    d.osc[0].ratio = 1.0f; d.osc[0].cents = 1200.0f;
    synth_init( &s, RATE ); synth_setinst( &s, 20, &d );
    synth_schedule_ev( &s, 0, &sp );
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 440.0f, 0.5f, SYNTH_SINE );
    synth_runevents( &s, 0 );
    synth_render( &s, buf, RATE );
    check( near( measure( buf, RATE ), 880.0, 0.5 ), "1200 cents sounds an octave up" );
    synth_cleanup( &s );

    // band-limited saw: the alias of the 4th harmonic of 5000 Hz (20000 -> folds to 24100 - 22050 ...) stays small
    synth_instdefault( &d ); d.osc[0].wave = SYNTH_SAW;
    synth_init( &s, RATE ); synth_setinst( &s, 20, &d );
    synth_schedule_ev( &s, 0, &sp );
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 5000.0f, 1.0f, SYNTH_SINE );
    synth_runevents( &s, 0 );
    synth_render( &s, buf, RATE );
    double fund = amp_at( buf, 2000, RATE, 5000.0 );
    double alias = amp_at( buf, 2000, RATE, 44100.0 - 5.0 * 5000.0 );        // 5th harmonic 25000 folds to 19100
    char nm[120]; snprintf( nm, sizeof(nm), "saw: fundamental %.3f (2/pi = 0.637), alias %.4f", fund, alias );
    check( near( fund, 0.637, 0.04 ) && alias < fund * 0.1, nm );
    check( near( measure( buf, RATE ), 5000.0, 2.0 ), "saw pitch" );
    synth_cleanup( &s );
    // pulse width: a 25 % pulse has a 2nd harmonic, a 50 % one hardly any
    synth_instdefault( &d ); d.osc[0].wave = SYNTH_SQUARE; d.osc[0].pw = 0.5f;
    synth_init( &s, RATE ); synth_setinst( &s, 20, &d );
    synth_schedule_ev( &s, 0, &sp ); synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 400.0f, 1.0f, SYNTH_SINE );
    synth_runevents( &s, 0 ); synth_render( &s, buf, RATE );
    double h2sq = amp_at( buf, 2000, RATE, 800.0 ), h1sq = amp_at( buf, 2000, RATE, 400.0 );
    synth_cleanup( &s );
    d.osc[0].pw = 0.25f;
    synth_init( &s, RATE ); synth_setinst( &s, 20, &d );
    synth_schedule_ev( &s, 0, &sp ); synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 400.0f, 1.0f, SYNTH_SINE );
    synth_runevents( &s, 0 ); synth_render( &s, buf, RATE );
    double h2pw = amp_at( buf, 2000, RATE, 800.0 );
    synth_cleanup( &s );
    check( h2sq < h1sq * 0.02 && h2pw > h1sq * 0.4, "pulse width 0.25 adds the even harmonics" );

    // low-pass: a saw at 110 Hz with cutoff 2 x the note: the harmonic at 3300 Hz is far weaker than without a filter
    double hi[2];
    for ( int f=0; f < 2; ++f ) {
        synth_instdefault( &d ); d.osc[0].wave = SYNTH_SAW; d.cutmul = f == 0 ? 0.0f : 2.0f;
        synth_init( &s, RATE ); synth_setinst( &s, 20, &d );
        synth_schedule_ev( &s, 0, &sp ); synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 110.0f, 1.0f, SYNTH_SINE );
        synth_runevents( &s, 0 ); synth_render( &s, buf, RATE );
        hi[f] = amp_at( buf, 4000, RATE, 3300.0 );
        if ( f == 1 ) check( near( measure( buf, RATE ), 110.0, 0.2 ), "filtered saw keeps its pitch" );
        synth_cleanup( &s );
    }
    snprintf( nm, sizeof(nm), "low-pass: 3300 Hz %.4f -> %.5f", hi[0], hi[1] );
    check( hi[1] < hi[0] * 0.05, nm );
    // the filter follows the note: the same instrument one octave up has the same relative spectrum
    // resonance makes a peak at the cutoff but stays stable (no blow up) at the highest setting
    synth_instdefault( &d ); d.osc[0].wave = SYNTH_SAW; d.cutmul = 64.0f; d.res = 0.95f; d.fenv = 6.0f; d.fdecay = 0.01f;
    synth_init( &s, RATE ); synth_setinst( &s, 20, &d );
    synth_schedule_ev( &s, 0, &sp ); synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 4000.0f, 1.0f, SYNTH_SINE );
    synth_runevents( &s, 0 ); synth_render( &s, buf, RATE );
    bool fin = true; for ( int k=0; k < RATE; ++k ) if ( !isfinite( buf[k] ) ) fin = false;
    check( fin && peak( buf, 0, RATE ) < 8.0f, "resonant filter at the limits stays finite" );
    synth_cleanup( &s );

    // pitch envelope: starts an octave up, settles on the note
    synth_instdefault( &d ); d.pitch = 12.0f; d.pitchtime = 0.05f;
    synth_init( &s, RATE ); synth_setinst( &s, 20, &d );
    synth_schedule_ev( &s, 0, &sp ); synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 200.0f, 1.0f, SYNTH_SINE );
    synth_runevents( &s, 0 ); synth_render( &s, buf, RATE );
    // count rising zero crossings in the first 20 ms and in the last half second
    int c0 = 0, c1 = 0;
    for ( int k=1; k < RATE / 50; ++k ) if ( buf[k-1] < 0 && buf[k] >= 0 ) ++c0;
    for ( int k=RATE/2; k < RATE; ++k ) if ( buf[k-1] < 0 && buf[k] >= 0 ) ++c1;
    snprintf( nm, sizeof(nm), "pitch envelope: %d cycles in the first 20 ms (>= 5), %d in 0.5 s (100)", c0, c1 );
    check( c0 >= 5 && c1 >= 99 && c1 <= 101, nm );
    synth_cleanup( &s );

    // default envelope of the instrument, overridden by ENVELOPE, restored by ENVELOPE default
    synth_instdefault( &d ); d.env = true; d.a = 0.2f; d.d = 0.0f; d.s = 1.0f; d.r = 0.1f;
    for ( int mode=0; mode < 3; ++mode ) {
        synth_init( &s, RATE ); synth_setinst( &s, 20, &d );
        synth_schedule_ev( &s, 0, &sp );
        synth_evspec_t e; memset( &e, 0, sizeof(e) ); e.type = SYNTH_EV_ENV; e.chan = 0;
        if ( mode == 1 ) { e.p[0] = 0.0f; e.p[1] = 0.0f; e.p[2] = 1.0f; e.p[3] = 0.0f; synth_schedule_ev( &s, 0, &e ); }
        if ( mode == 2 ) {
            e.p[0] = 0.0f; e.p[1] = 0.0f; e.p[2] = 1.0f; e.p[3] = 0.0f; synth_schedule_ev( &s, 0, &e );
            e.p[0] = -1.0f; synth_schedule_ev( &s, 0, &e );
        }
        synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 440.0f, 1.0f, SYNTH_SINE );
        synth_runevents( &s, 0 ); synth_render( &s, buf, RATE );
        float early = peak( buf, 0, 441 );          // first 10 ms
        snprintf( nm, sizeof(nm), "instrument envelope mode %d: first 10 ms peak %.3f", mode, early );
        check( mode == 1 ? early > 0.9f : early < 0.2f, nm );
        check( mode == 0 ? near( peak( buf, RATE - 2000, RATE ), 1.0, 0.06 ) : true, "attack reaches full level" );
        synth_cleanup( &s );
    }

    // WAVE selects a basic wave again: the signal is the one of the plain wave
    synth_init( &s, RATE );
    synth_schedule_ev( &s, 0, &sp );
    sp.p[0] = 0.0f;
    synth_schedule( &s, 0, SYNTH_EV_WAVE, 0, 0, 0, SYNTH_SQUARE );
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 441.0f, 1.0f, SYNTH_SINE );
    synth_runevents( &s, 0 ); synth_render( &s, buf, 20000 );
    float* ref = tone( SYNTH_SQUARE, 441.0f, 1.0f, 20000 );
    bool same = true; for ( int k=0; k < 20000; ++k ) if ( buf[k] != ref[k] ) same = false;
    check( same, "WAVE after an instrument is the plain wave again, sample for sample" );
    free( ref ); synth_cleanup( &s );
    sp.p[0] = 20.0f;

    // validation
    synth_inst_t bad; synth_instdefault( &bad );
    synth_init( &s, RATE );
    check( !synth_setinst( &s, 0, &bad ) && !synth_setinst( &s, 3, &bad ) && !synth_setinst( &s, 32, &bad ) && !synth_setinst( &s, -1, &bad ), "instruments 0..3 and out of range can't be defined" );
    check( synth_setinst( &s, 31, &bad ) && synth_setinst( &s, 4, &bad ), "4 and 31 can" );
    bad.nosc = 0; check( !synth_setinst( &s, 5, &bad ), "0 oscillators refused" );
    bad.nosc = 4; check( !synth_setinst( &s, 5, &bad ), "4 oscillators refused" );
    synth_instdefault( &bad ); bad.osc[0].ratio = 0.0f; check( !synth_setinst( &s, 5, &bad ), "ratio 0 refused except for noise" );
    bad.osc[0].wave = SYNTH_NOISE; check( synth_setinst( &s, 5, &bad ), "noise ratio 0 is white noise" );
    synth_instdefault( &bad ); bad.osc[0].pw = 0.0f; check( !synth_setinst( &s, 5, &bad ), "pulse width 0 refused" );
    synth_instdefault( &bad ); bad.res = 1.0f; check( !synth_setinst( &s, 5, &bad ), "resonance 1 refused" );
    synth_instdefault( &bad ); bad.cutmul = (float) NAN; check( !synth_setinst( &s, 5, &bad ), "NaN refused" );
    synth_instdefault( &bad ); bad.s = 1.5f; check( !synth_setinst( &s, 5, &bad ), "sustain 1.5 refused" );
    synth_instdefault( &bad ); bad.osc[0].wave = SYNTH_NOSCWAVES; check( !synth_setinst( &s, 5, &bad ), "unknown wave refused" );
    check( synth_schedule( &s, 0, SYNTH_EV_INST, 0, 0, 0, SYNTH_SINE ), "instrument 0 is selectable" );
    sp.p[0] = 32.0f; check( !synth_schedule_ev( &s, 0, &sp ), "instrument 32 refused" );
    sp.p[0] = -1.0f; check( !synth_schedule_ev( &s, 0, &sp ), "instrument -1 refused" );
    synth_instpreset( 4, &bad ); check( strcmp( bad.name, "BASS" ) == 0 && bad.nosc == 2 && bad.osc[1].mul == 0.5f, "preset copy (derived ratio filled in)" );
    synth_cleanup( &s );
    free( buf );

    // the definition string parser
    {
        synth_inst_t in; char err[MML_ERRLEN];
        check( mml_parse_inst( "", &in, err ) && in.nosc == 1 && in.osc[0].wave == SYNTH_SINE && in.cutmul == 0.0f && !in.env && in.name[0] == '\0', "inst string: empty = plain sine" );
        check( mml_parse_inst( "name bass1; OSC saw 0.8; osc SQUARE,0.6,0.5,7,0.25 ; Filter 3 0.3 2 0.15;pitch 12 0.05;ENV 0.01 0.2 0.6 0.1;vib 5 0.1;gain 0.6", &in, err ) &&
               strcmp( in.name, "BASS1" ) == 0 && in.nosc == 2 && in.osc[0].wave == SYNTH_SAW && near( in.osc[0].level, 0.8, 1e-6 ) &&
               in.osc[1].wave == SYNTH_SQUARE && near( in.osc[1].ratio, 0.5, 1e-6 ) && near( in.osc[1].cents, 7.0, 1e-6 ) && near( in.osc[1].pw, 0.25, 1e-6 ) &&
               near( in.cutmul, 3.0, 1e-6 ) && near( in.res, 0.3, 1e-6 ) && near( in.fenv, 2.0, 1e-6 ) && near( in.fdecay, 0.15, 1e-6 ) &&
               near( in.pitch, 12.0, 1e-6 ) && in.env && near( in.s, 0.6, 1e-6 ) && near( in.vibrate, 5.0, 1e-6 ) && near( in.gain, 0.6, 1e-6 ), "inst string: every command" );
        check( mml_parse_inst( "OSC NOISE", &in, err ) && in.osc[0].ratio == 0.0f, "inst string: noise defaults to white" );
        check( mml_parse_inst( "OSC 4;FILTER 2", &in, err ) && in.osc[0].wave == SYNTH_SAW && near( in.fdecay, 0.2, 1e-6 ), "inst string: wave by number, default filter decay" );
        check( mml_parse_inst( "OSC SINE -3", &in, err ) == false && strstr( err, "out of range" ) != 0, "inst string: negative level refused" );
        check( mml_parse_inst( "OSC SINE 1 1 -1200", &in, err ) && near( in.osc[0].cents, -1200.0, 1e-6 ), "inst string: negative cents" );
        check( !mml_parse_inst( "BOGUS 1", &in, err ) && strstr( err, "BOGUS" ) != 0, "inst string: unknown command named" );
        check( !mml_parse_inst( "OSC SAW; OSC SAW; OSC SAW; OSC SAW", &in, err ), "inst string: 4 oscillators refused" );
        check( !mml_parse_inst( "OSC SINE 1 1 0 0.5 9", &in, err ), "inst string: too many values" );
        check( !mml_parse_inst( "ENV 1 2 3", &in, err ), "inst string: ENV needs 4 values" );
        check( !mml_parse_inst( "NAME", &in, err ), "inst string: NAME needs a word" );
        check( !mml_parse_inst( "GAIN 1 ?", &in, err ), "inst string: stray character" );
        check( !mml_parse_inst( "FILTER 70", &in, err ), "inst string: cutoff range" );
        check( mml_parse_inst( "NAME ABCDEFGHIJKLMNOP", &in, err ) && strlen( in.name ) == SYNTH_INST_NAMELEN - 1, "inst string: long name is cut" );
    }
}

static void test_preset_loudness( void ) {
    // the presets are calibrated: a one-second note at volume 1 (A3, A1 for the low drums) has the same RMS (about 0.34) in every preset and never exceeds the volume (a soft limiter in the instrument path),
    // so that the volume V of a PLAY string means about the same for every instrument
    synth_t s;
    for ( int i=SYNTH_NBASIC; i < SYNTH_NBASIC + SYNTH_NPRESET; ++i ) {
        int n; float* b = inst_note( &s, i, i == 13 || i == 16 ? 55.0f : 220.0f, 1.0f, 1.0, 4.0, &n );
        double e = 0; long cnt = 0; float pk = 0.0f;
        for ( int k=0; k < n; ++k ) {
            if ( fabsf( b[k] ) > 0.0005f ) { e += (double) b[k] * b[k]; ++cnt; }
            if ( fabsf( b[k] ) > pk ) pk = fabsf( b[k] );
        }
        double rms = cnt > 0 ? sqrt( e / (double) cnt ) : 0.0;
        char name[100]; snprintf( name, sizeof(name), "preset %d %s loudness: rms %.3f (0.30..0.38), peak %.3f (<= 1.0)", i, synth_instname( &s, i ), rms, (double) pk );
        check( rms >= 0.30 && rms <= 0.38 && pk <= 1.0f, name );
        free( b ); synth_cleanup( &s );
    }
}

static void test_instrument_snapshots( void ) {
    // INSTDEF is a queued snapshot: notes before it keep the old definition, notes after it use the new one
    synth_t s; synth_init( &s, RATE );
    synth_inst_t a, b; synth_instdefault( &a ); synth_instdefault( &b ); b.osc[0].ratio = 2.0f;
    synth_evspec_t sp; memset( &sp, 0, sizeof(sp) ); sp.type = SYNTH_EV_INSTDEF; sp.p[0] = 20.0f; sp.def = &a;
    check( synth_schedule_ev( &s, 0, &sp ), "INSTDEF accepted" );
    synth_schedule( &s, 0, SYNTH_EV_PLAY, 0, 440.0f, 0.5f, SYNTH_SINE );
    synth_schedule( &s, 1 * SEC, SYNTH_EV_STOP, 0, 0, 0, SYNTH_SINE );
    sp.def = &b;
    check( synth_schedule_ev( &s, 3 * SEC / 2, &sp ), "second INSTDEF accepted" );
    synth_schedule( &s, 3 * SEC / 2, SYNTH_EV_PLAY, 0, 440.0f, 0.5f, SYNTH_SINE );
    synth_schedule( &s, 3 * SEC, SYNTH_EV_STOP, 0, 0, 0, SYNTH_SINE );
    sp.def = &a; b.osc[0].ratio = 3.0f;         // changing the caller's struct afterwards changes nothing (the event holds a copy)
    synth_instpreset( 20, &a );
    int n = RATE * 3;
    float* buf = (float*) malloc( sizeof(float) * (size_t) n );
    for ( int done = 0; done < n; done += 441 ) {
        synth_runevents( &s, (uint64_t)( (double) done * 1e9 / RATE ) );
        synth_render( &s, buf + done, 441 );
    }
    double f1 = measure( buf + RATE / 5, RATE / 2 ), f2 = measure( buf + RATE * 17 / 10, RATE / 2 );
    char nm[100]; snprintf( nm, sizeof(nm), "snapshot: 440 Hz before, 880 Hz after the new definition (%.1f / %.1f)", f1, f2 );
    check( near( f1, 440.0, 1.0 ) && near( f2, 880.0, 2.0 ), nm );
    free( buf );
    synth_runevents( &s, 3 * SEC );
    check( s.nevents == 0, "snapshot events are used up" );
    sp.def = 0; check( !synth_schedule_ev( &s, 0, &sp ), "INSTDEF without a definition refused" );
    sp.def = &b; sp.p[0] = 3.0f; check( !synth_schedule_ev( &s, 0, &sp ), "INSTDEF for a basic wave refused" );
    b.res = 2.0f; sp.p[0] = 20.0f; check( !synth_schedule_ev( &s, 0, &sp ), "INSTDEF with a bad definition refused" );
    // events dropped with the queue free their snapshots (checked by ASan)
    b.res = 0.0f; b.osc[0].ratio = 1.0f;
    synth_schedule_ev( &s, 10 * SEC, &sp ); synth_silencechan( &s, 0, 0 );
    synth_schedule_ev( &s, 10 * SEC, &sp ); synth_silence( &s, 0 );
    synth_schedule_ev( &s, 10 * SEC, &sp );
    synth_cleanup( &s );
}

static void test_instrument_cpu( void ) {
    // 16 channels of the most expensive presets (3 oscillators + filter) for 4 seconds of audio
    synth_t s; synth_init( &s, RATE );
    for ( int c=0; c < SYNTH_NCHAN; ++c ) {
        synth_evspec_t sp; memset( &sp, 0, sizeof(sp) );
        sp.type = SYNTH_EV_INST; sp.chan = c; sp.p[0] = (float)( c % 2 == 0 ? 6 : 9 );     // PAD / STRINGS
        synth_schedule_ev( &s, 0, &sp );
        synth_schedule( &s, 0, SYNTH_EV_PLAY, c, 110.0f * (float) pow( 2.0, c / 12.0 ), 0.5f, SYNTH_SINE );
    }
    synth_runevents( &s, 0 );
    int n = RATE * 4;
    float* buf = (float*) malloc( sizeof(float) * (size_t) n );
    struct timespec t0, t1; clock_gettime( CLOCK_MONOTONIC, &t0 );
    synth_render( &s, buf, n );
    clock_gettime( CLOCK_MONOTONIC, &t1 );
    double used = (double)( t1.tv_sec - t0.tv_sec ) + (double)( t1.tv_nsec - t0.tv_nsec ) / 1e9;
    printf( "      (16 channels x 3 oscillators + filter: %.3f s CPU for 4 s of audio = %.1f %% of one core)\n", used, used / 4.0 * 100.0 );
    check( used < 4.0 * 0.25, "16 instrument voices need less than 25 % of a core" );
    free( buf ); synth_cleanup( &s );
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
    test_glide();
    test_envelope();
    test_lfos();
    test_immediate();
    test_mml_effects();
    test_instruments();
    test_preset_loudness();
    test_instrument_snapshots();
    test_instrument_cpu();
    printf( "%d checks, %d failure(s)\n", total, failures );
    return failures;
}
