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


#include "noise.h"
#include <math.h>
#include <time.h>

/*
Automated test of the hash and noise functions (noise.[ch], no SDL): golden values (computed independently in Python from the same definition, so the
numbers are the same on every machine), the distribution and the avalanche of the hash, the range, smoothness, zeros and mean of the noise in 1, 2 and 3
dimensions, and the fractal noise. The exit status is the number of failures.
*/

static int failures = 0, cases = 0;

#define CHECK(cond) do { ++cases; if ( !(cond) ) { ++failures; printf( "FAIL line %d: %s\n", __LINE__, #cond ); } } while (0)
#define NEAR(a,b,eps) do { ++cases; double _a = (a), _b = (b); if ( !( fabs( _a - _b ) <= (eps) ) ) { ++failures; printf( "FAIL line %d: %s = %.17g, expected %.17g\n", __LINE__, #a, _a, _b ); } } while (0)

static int popcount64( uint64_t v ) { int n = 0; while ( v ) { v &= v - 1; ++n; } return n; }

int main( void ) {
    // -- golden values --------------------------------------------------------------------------------------------
    CHECK( nz_hash64( 0, 1, 2, 3, 4 ) == UINT64_C(0x93de87803cccab31) );
    CHECK( nz_hash64( 12345, -7, 0, 99, 1000000 ) == UINT64_C(0xab49173c1fddcb29) );
    CHECK( nz_hash( 1, 2, 3, 4 ) == 0.57761427765653883 );
    CHECK( nz_hash( 0, 0, 0, 0 ) == 0.47141042966848024 );
    CHECK( nz_mix( 0 ) == 0 );
    // the noise: the same numbers as the independent port (a tolerance of 1e-12 allows for a different libm floor / rounding, there is none in practice)
    NEAR( nz_noise( 42, 2, 3.37, -8.91, 0 ), -0.5790764891942779, 1e-12 );
    NEAR( nz_noise( 7, 2, 0.5, 0.5, 0 ), -0.64960490501457591, 1e-12 );
    NEAR( nz_fbm( 99, 12.34, -56.78, 6, 0.5, 2.0 ), -0.16703415569336791, 1e-12 );
    NEAR( nz_fbm( 5, 0.25, 0.75, 3, 0.7, 3.0 ), -0.07953037665458372, 1e-12 );

    NEAR( nz_ridged( 3, 1.7, -2.2, 4, 0.5, 2.0 ), 0.79763041376779631, 1e-12 );
    NEAR( nz_ridged( 11, 0.3, 0.9, 1, 0.5, 2.0 ), 0.99155365497391401, 1e-12 );
    NEAR( nz_ridged( 5, 0.25, 0.75, 3, 0.7, 3.0 ), 0.5722325150236115, 1e-12 );
    {   // ridged noise: 0 .. 1, sharp crests: more values near the top than fractal noise squared would give, and a different noise per seed
        double mn = 1, mx = 0, sum = 0; const int N = 100000;
        for ( int i = 0; i < N; ++i ) {
            double v = nz_ridged( 21, i * 0.0731 - 3000.0, ( i % 719 ) * 1.37 - 400.0, 5, 0.5, 2.0 );
            CHECK( v >= 0.0 && v <= 1.0 );
            if ( v < mn ) mn = v;
            if ( v > mx ) mx = v;
            sum += v;
        }
        CHECK( mx > 0.9 && mn < 0.5 && sum / N > 0.5 && sum / N < 0.9 );
        CHECK( nz_ridged( 21, 3.3, 4.4, 5, 0.5, 2.0 ) != nz_ridged( 22, 3.3, 4.4, 5, 0.5, 2.0 ) );
        CHECK( nz_ridged( 21, 3.3, 4.4, 0, 0.5, 2.0 ) == nz_ridged( 21, 3.3, 4.4, 1, 0.5, 2.0 ) );
    }
    // -- hash: range, uniform distribution, every bit, order of the arguments, avalanche ------------------------------
    {
        int bins[16] = { 0 }; long bits[53] = { 0 }; const int N = 160000;
        for ( int i = 0; i < N; ++i ) {
            double h = nz_hash( i % 400 - 200, i / 400 - 200, 5, 0 );
            CHECK( h >= 0.0 && h < 1.0 );
            ++bins[ (int)( h * 16.0 ) ];
            uint64_t m = (uint64_t)( h * 9007199254740992.0 );
            for ( int b = 0; b < 53; ++b ) { bits[b] += ( m >> b ) & 1; }
        }
        double chi = 0;
        for ( int k = 0; k < 16; ++k ) { double e = N / 16.0; chi += ( bins[k] - e ) * ( bins[k] - e ) / e; }
        CHECK( chi < 37.7 );                                        // 15 degrees of freedom, p = 0.001
        for ( int b = 0; b < 53; ++b ) { CHECK( bits[b] > N * 0.49 && bits[b] < N * 0.51 ); }
        CHECK( nz_hash( 1, 2, 3, 4 ) != nz_hash( 2, 1, 3, 4 ) );
        CHECK( nz_hash( 1, 2, 3, 4 ) != nz_hash( 1, 2, 4, 3 ) );
        CHECK( nz_hash( 1, 0, 0, 0 ) != nz_hash( 0, 1, 0, 0 ) );
        CHECK( nz_hash( 1, 2, 3, 4 ) == nz_hash( 1, 2, 3, 4 ) );
        CHECK( nz_hash64( 1, 5, 6, 7, 8 ) != nz_hash64( 2, 5, 6, 7, 8 ) );
        long flips = 0, tries = 0;
        for ( int i = 0; i < 2000; ++i ) {
            for ( int b = 0; b < 32; ++b ) {
                flips += popcount64( nz_hash64( 3, i, 7, 9, 11 ) ^ nz_hash64( 3, i ^ ( 1 << b ), 7, 9, 11 ) );
                ++tries;
            }
        }
        double avg = (double) flips / (double) tries;
        CHECK( avg > 31.5 && avg < 32.5 );                          // an ideal hash flips half of the 64 bits
    }

    // -- noise: range, reach, zero at the lattice, smoothness, mean, seeds, far away -----------------------------------
    for ( int dims = 1; dims <= 3; ++dims ) {
        double mn = 0, mx = 0, sum = 0, sum2 = 0, maxjump = 0; long n = 0;
        for ( int i = 0; i < 200000; ++i ) {
            double x = i * 0.0137 - 1200.0, y = ( i % 977 ) * 0.371 - 180.0, z = ( i % 313 ) * 0.53 - 80.0;
            double v = nz_noise( 1234, dims, x, y, z );
            CHECK( v >= -1.0 && v <= 1.0 );
            if ( v < mn ) mn = v;
            if ( v > mx ) mx = v;
            sum += v; sum2 += v * v; ++n;
            double w = nz_noise( 1234, dims, x + 1e-4, y + 1e-4, z + 1e-4 );
            if ( fabs( w - v ) > maxjump ) maxjump = fabs( w - v );
        }
        double mean = sum / n, sd = sqrt( sum2 / n - mean * mean );
        CHECK( mx > 0.8 && mn < -0.8 );                              // it reaches nearly the whole range
        CHECK( fabs( mean ) < 0.02 );
        CHECK( sd > 0.2 && sd < 0.6 );
        CHECK( maxjump < 1e-3 * dims );                              // smooth: a step of 1e-4 in every direction changes it by far less than the range
        for ( int i = -5; i <= 5; ++i ) {
            NEAR( nz_noise( 77, dims, i, i * 3, -i * 2 ), 0.0, 1e-12 );   // zero at every lattice point
        }
        CHECK( nz_noise( 1, dims, 0.4, 0.4, 0.4 ) != nz_noise( 2, dims, 0.4, 0.4, 0.4 ) );
        NEAR( nz_noise( 5, dims, -1e-9, -1e-9, -1e-9 ), nz_noise( 5, dims, 1e-9, 1e-9, 1e-9 ), 1e-6 );   // no seam at the origin
        double far = nz_noise( 5, dims, 4.0e9 + 0.37, -4.0e9 - 0.61, 3.0e9 + 0.2 );
        CHECK( far >= -1.0 && far <= 1.0 );
        CHECK( nz_noise( 5, dims, 3.3, 4.4, 5.5 ) == nz_noise( 5, dims, 3.3, 4.4, 5.5 ) );
    }
    CHECK( nz_noise( 9, 1, 0.37, 55, 66 ) == nz_noise( 9, 1, 0.37, 1, 2 ) );          // 1 D ignores y and z
    CHECK( nz_noise( 9, 2, 0.37, 0.8, 55 ) == nz_noise( 9, 2, 0.37, 0.8, 77 ) );      // 2 D ignores z

    // -- fractal noise --------------------------------------------------------------------------------------------
    {
        double mn = 0, mx = 0;
        for ( int i = 0; i < 100000; ++i ) {
            double v = nz_fbm( 31, i * 0.0731 - 3000.0, ( i % 719 ) * 1.37 - 400.0, 8, 0.5, 2.0 );
            CHECK( v >= -1.0 && v <= 1.0 );
            if ( v < mn ) mn = v;
            if ( v > mx ) mx = v;
        }
        CHECK( mx > 0.4 && mn < -0.4 );
        // one octave is plain noise (its own seed and offset); gain 0 makes the later octaves vanish
        double a = nz_fbm( 4, 3.3, 4.4, 1, 0.5, 2.0 ), b = nz_fbm( 4, 3.3, 4.4, 5, 0.0, 2.0 );
        NEAR( a, b, 1e-12 );
        CHECK( nz_fbm( 4, 3.3, 4.4, 0, 0.5, 2.0 ) == a );                 // fewer than one octave is one
        CHECK( nz_fbm( 4, 3.3, 4.4, 99, 0.5, 2.0 ) == nz_fbm( 4, 3.3, 4.4, NZ_MAXOCTAVES, 0.5, 2.0 ) );
        CHECK( nz_fbm( 4, 3.3, 4.4, 6, 0.5, 2.0 ) != nz_fbm( 5, 3.3, 4.4, 6, 0.5, 2.0 ) );
        CHECK( nz_fbm( 4, 0.0, 0.0, 6, 0.5, 2.0 ) != 0.0 );                // the octaves are offset: not 0 at the origin
        // smooth, and more octaves add detail: the difference between near points grows with the octaves
        double d1 = 0, d6 = 0;
        for ( int i = 0; i < 20000; ++i ) {
            double x = i * 0.29, y = i * 0.17;
            d1 += fabs( nz_fbm( 8, x, y, 1, 0.5, 2.0 ) - nz_fbm( 8, x + 0.01, y, 1, 0.5, 2.0 ) );
            d6 += fabs( nz_fbm( 8, x, y, 6, 0.5, 2.0 ) - nz_fbm( 8, x + 0.01, y, 6, 0.5, 2.0 ) );
        }
        printf( "fbm detail: d1 %.3f d6 %.3f\n", d1, d6 );
        CHECK( d6 > d1 * 1.1 );
    }

    // -- speed (information) --------------------------------------------------------------------------------------
    {
        volatile double sink = 0; clock_t t0 = clock(); const int N = 2000000;
        for ( int i = 0; i < N; ++i ) { sink += nz_hash( i, 3, 4, 5 ); }
        double t1 = (double)( clock() - t0 ) / CLOCKS_PER_SEC; t0 = clock();
        for ( int i = 0; i < N; ++i ) { sink += nz_noise( 1, 2, i * 0.01, i * 0.007, 0 ); }
        double t2 = (double)( clock() - t0 ) / CLOCKS_PER_SEC; t0 = clock();
        for ( int i = 0; i < N / 4; ++i ) { sink += nz_fbm( 1, i * 0.01, i * 0.007, 6, 0.5, 2.0 ); }
        double t3 = (double)( clock() - t0 ) / CLOCKS_PER_SEC;
        printf( "speed: hash %.0f ns, 2 D noise %.0f ns, fbm of 6 octaves %.0f ns\n", t1 * 1e9 / N, t2 * 1e9 / N, t3 * 1e9 / ( N / 4 ) );
    }

    printf( "noisetest1: %d cases, %d failures\n", cases, failures );
    return failures;
}
