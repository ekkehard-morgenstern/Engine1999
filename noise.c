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

// no fused multiply-add: the same results on every machine and with every compiler (the golden values of noisetest1 were computed independently, in Python)
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize ("fp-contract=off")
#endif

// -- hash ---------------------------------------------------------------------------------------------------------

uint64_t nz_mix( uint64_t x ) {
    x ^= x >> 30; x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27; x *= UINT64_C(0x94d049bb133111eb);
    x ^= x >> 31;
    return x;
}

uint64_t nz_hash64( uint64_t seed, int64_t a, int64_t b, int64_t c, int64_t d ) {
    const uint64_t gold = UINT64_C(0x9e3779b97f4a7c15);
    uint64_t h = nz_mix( seed + gold );
    h = nz_mix( ( h ^ (uint64_t) a ) + gold );
    h = nz_mix( ( h ^ (uint64_t) b ) + gold );
    h = nz_mix( ( h ^ (uint64_t) c ) + gold );
    h = nz_mix( ( h ^ (uint64_t) d ) + gold );
    return h;
}

double nz_hash( int64_t a, int64_t b, int64_t c, int64_t d ) {
    return (double)( nz_hash64( 0, a, b, c, d ) >> 11 ) * ( 1.0 / 9007199254740992.0 );     // 53 bits
}

// -- gradient noise -----------------------------------------------------------------------------------------------

// 2 D: 32 unit vectors, 11.25 degrees apart (a table, so that no sin / cos of the C library is needed)
static const double grad2[32][2] = {
    { 1, 0 },
    { 0.98078528040323043, 0.19509032201612825 },
    { 0.92387953251128674, 0.38268343236508978 },
    { 0.83146961230254524, 0.55557023301960218 },
    { 0.70710678118654757, 0.70710678118654746 },
    { 0.55557023301960229, 0.83146961230254524 },
    { 0.38268343236508984, 0.92387953251128674 },
    { 0.19509032201612833, 0.98078528040323043 },
    { 6.123233995736766e-17, 1 },
    { -0.19509032201612819, 0.98078528040323043 },
    { -0.38268343236508973, 0.92387953251128674 },
    { -0.55557023301960196, 0.83146961230254546 },
    { -0.70710678118654746, 0.70710678118654757 },
    { -0.83146961230254535, 0.55557023301960218 },
    { -0.92387953251128674, 0.38268343236508989 },
    { -0.98078528040323043, 0.19509032201612861 },
    { -1, 1.2246467991473532e-16 },
    { -0.98078528040323043, -0.19509032201612836 },
    { -0.92387953251128685, -0.38268343236508967 },
    { -0.83146961230254546, -0.55557023301960196 },
    { -0.70710678118654768, -0.70710678118654746 },
    { -0.55557023301960218, -0.83146961230254524 },
    { -0.38268343236509034, -0.92387953251128652 },
    { -0.19509032201612866, -0.98078528040323032 },
    { -1.8369701987210297e-16, -1 },
    { 0.1950903220161283, -0.98078528040323043 },
    { 0.38268343236509, -0.92387953251128663 },
    { 0.55557023301960184, -0.83146961230254546 },
    { 0.70710678118654735, -0.70710678118654768 },
    { 0.83146961230254524, -0.55557023301960218 },
    { 0.92387953251128652, -0.38268343236509039 },
    { 0.98078528040323032, -0.19509032201612872 }
};
// 3 D: the twelve edge directions of a cube (and four of them again to make 16), unit length
#define R2  0.70710678118654757
static const double grad3[16][3] = {
    {  R2,  R2, 0 }, { -R2,  R2, 0 }, {  R2, -R2, 0 }, { -R2, -R2, 0 },
    {  R2, 0,  R2 }, { -R2, 0,  R2 }, {  R2, 0, -R2 }, { -R2, 0, -R2 },
    { 0,  R2,  R2 }, { 0, -R2,  R2 }, { 0,  R2, -R2 }, { 0, -R2, -R2 },
    {  R2,  R2, 0 }, { -R2,  R2, 0 }, { 0, -R2,  R2 }, { 0, -R2, -R2 }
};

// the scale that makes the result span about [-1, 1]: 1 D reaches 0.5 exactly; 2 D is bounded by sqrt(2)/2 (unit gradients; a dense scan of 40 seeds reached 0.69);
// the 3 D gradient set reached 0.706 in the same scan (and the result is clamped to [-1, 1] anyway)
#define NZ_MAX1     0.5
#define NZ_MAX2     0.70710678118654757
#define NZ_MAX3     0.70710678118654757

static double fade( double t ) { return t * t * t * ( t * ( t * 6.0 - 15.0 ) + 10.0 ); }
static double lerp( double a, double b, double t ) { return a + t * ( b - a ); }

static uint64_t lat( uint64_t seed, int dims, int64_t x, int64_t y, int64_t z ) {
    return nz_hash64( seed + (uint64_t) dims * UINT64_C(0x632be59bd9b4e019), x, y, z, 0 );
}

static double dot1( uint64_t seed, int64_t x, double dx ) {
    return ( lat( seed, 1, x, 0, 0 ) & 1U ) ? dx : -dx;
}

static double dot2( uint64_t seed, int64_t x, int64_t y, double dx, double dy ) {
    const double* g = grad2[ lat( seed, 2, x, y, 0 ) >> 59 ];                   // the top five bits
    return g[0] * dx + g[1] * dy;
}

static double dot3( uint64_t seed, int64_t x, int64_t y, int64_t z, double dx, double dy, double dz ) {
    const double* g = grad3[ lat( seed, 3, x, y, z ) >> 60 ];                   // the top four bits
    return g[0] * dx + g[1] * dy + g[2] * dz;
}

double nz_noise( uint64_t seed, int dims, double x, double y, double z ) {
    double fx = floor( x ), tx = x - fx; int64_t ix = (int64_t) fx;
    double u = fade( tx );
    if ( dims <= 1 ) {
        double r = lerp( dot1( seed, ix, tx ), dot1( seed, ix + 1, tx - 1.0 ), u ) * ( 1.0 / NZ_MAX1 );
        return r > 1.0 ? 1.0 : r < -1.0 ? -1.0 : r;
    }
    double fy = floor( y ), ty = y - fy; int64_t iy = (int64_t) fy;
    double v = fade( ty );
    if ( dims == 2 ) {
        double a = lerp( dot2( seed, ix, iy,     tx,       ty ),       dot2( seed, ix + 1, iy,     tx - 1.0, ty ),       u );
        double b = lerp( dot2( seed, ix, iy + 1, tx,       ty - 1.0 ), dot2( seed, ix + 1, iy + 1, tx - 1.0, ty - 1.0 ), u );
        double r = lerp( a, b, v ) * ( 1.0 / NZ_MAX2 );
        return r > 1.0 ? 1.0 : r < -1.0 ? -1.0 : r;
    }
    double fz = floor( z ), tz = z - fz; int64_t iz = (int64_t) fz;
    double w = fade( tz );
    double a0 = lerp( dot3( seed, ix, iy,     iz,     tx, ty,       tz ),       dot3( seed, ix + 1, iy,     iz,     tx - 1.0, ty,       tz ),       u );
    double b0 = lerp( dot3( seed, ix, iy + 1, iz,     tx, ty - 1.0, tz ),       dot3( seed, ix + 1, iy + 1, iz,     tx - 1.0, ty - 1.0, tz ),       u );
    double a1 = lerp( dot3( seed, ix, iy,     iz + 1, tx, ty,       tz - 1.0 ), dot3( seed, ix + 1, iy,     iz + 1, tx - 1.0, ty,       tz - 1.0 ), u );
    double b1 = lerp( dot3( seed, ix, iy + 1, iz + 1, tx, ty - 1.0, tz - 1.0 ), dot3( seed, ix + 1, iy + 1, iz + 1, tx - 1.0, ty - 1.0, tz - 1.0 ), u );
    double r = lerp( lerp( a0, b0, v ), lerp( a1, b1, v ), w ) * ( 1.0 / NZ_MAX3 );
    return r > 1.0 ? 1.0 : r < -1.0 ? -1.0 : r;
}

// -- fractal noise ------------------------------------------------------------------------------------------------

double nz_fbm( uint64_t seed, double x, double y, int octaves, double gain, double lacunarity ) {
    if ( octaves < 1 ) {
        octaves = 1;
    }
    if ( octaves > NZ_MAXOCTAVES ) {
        octaves = NZ_MAXOCTAVES;
    }
    double sum = 0.0, norm = 0.0, amp = 1.0, freq = 1.0;
    for ( int o = 0; o < octaves; ++o ) {
        // every octave has its own noise and its own offset (all octaves would be 0 at the origin otherwise)
        uint64_t h = nz_hash64( seed, o, 0x4f43, 0, 0 );
        double offx = (double)( h >> 40 ) * ( 1.0 / 16384.0 ), offy = (double)( ( h >> 16 ) & UINT64_C(0xffffff) ) * ( 1.0 / 16384.0 );
        sum += amp * nz_noise( h, 2, x * freq + offx, y * freq + offy, 0.0 );
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return norm > 0.0 ? sum / norm : 0.0;
}

double nz_ridged( uint64_t seed, double x, double y, int octaves, double gain, double lacunarity ) {
    if ( octaves < 1 ) {
        octaves = 1;
    }
    if ( octaves > NZ_MAXOCTAVES ) {
        octaves = NZ_MAXOCTAVES;
    }
    double sum = 0.0, norm = 0.0, amp = 1.0, freq = 1.0;
    for ( int o = 0; o < octaves; ++o ) {
        uint64_t h = nz_hash64( seed, o, 0x5244, 0, 0 );
        double offx = (double)( h >> 40 ) * ( 1.0 / 16384.0 ), offy = (double)( ( h >> 16 ) & UINT64_C(0xffffff) ) * ( 1.0 / 16384.0 );
        double r = 1.0 - fabs( nz_noise( h, 2, x * freq + offx, y * freq + offy, 0.0 ) );
        sum += amp * r * r;
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return norm > 0.0 ? sum / norm : 0.0;
}
