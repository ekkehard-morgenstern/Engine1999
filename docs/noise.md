# Hash and noise functions: `HASH`, `NOISE`, `FBM`

*2026-11-20, roadmap item 95 (the large procedurally generated world), owner: "Start with the noise and hash functions in C". Code: `noise.[ch]` (no SDL, no global state), the three functions of the language, tests `noisetest1` and the `HASH` / `NOISE` / `FBM` cases of `vmtest1`.*

A world that is made up on the way (chunks, see `docs/1987.md` "The world in chunks") needs two things from the language: a **random number that is a function of a place** (so that a chunk can be built again and comes out the same) and a **smooth noise of the position** (hills, moisture, forests, lakes). The BASIC prototype had a hash written in BASIC (`FN HSH`, three rounds of a congruence) and the terrain was a sum of sines; `HASH` replaces the BASIC hash (the prototype of `1987` uses it now; its forests changed, because the numbers are other ones). **Speed:** building a chunk was not faster with it (130 chunks took 40 .. 44 ticks of 1/60 s in the console host, before 32 with the BASIC hash but with fewer trees: the cost is the `M3D VERTEX` / `QUAD` calls and the trees, not the hash), so the gain is one definition that is the same everywhere, 53 bits, and less code in the program. The terrain grid primitive (next in item 95) is what will save the calls.

## The functions

| Function | Result |
|---|---|
| `HASH(a [,b [,c [,d]]])` | a number `0 <= h < 1` from up to four integers. The integer part (the floor) of each argument is used; **missing arguments are 0**, so `HASH(7)` is `HASH(7,0,0,0)`. The order matters: `HASH(1,2)` and `HASH(2,1)` differ. 53 bits (a multiple of 2^-53). |
| `NOISE(seed, x)` | smooth **gradient noise** ("Perlin noise", improved fade curve `6t^5 - 15t^4 + 10t^3`) along a line: `-1 .. 1`, exactly 0 at every whole `x`, mean 0. |
| `NOISE(seed, x, y)` | the same in the plane (32 gradient directions). |
| `NOISE(seed, x, y, z)` | the same in space (the 12 cube edge directions). |
| `RIDGED(seed, x, y, octaves [,gain [,lacunarity]])` | **ridged** fractal noise (added 2026-11-20): `(1 - |noise|)^2` per octave, so the crests are sharp ridges, `0 .. 1` (mean about 0.6); the same arguments, defaults and limits as `FBM`, its own offsets. For mountain ranges (`M3D MOUNTAINS` uses it). |
| `FBM(seed, x, y, octaves [,gain [,lacunarity]])` | **fractal noise**: the sum of `octaves` (1 .. 16) layers of the 2 D noise, each layer `lacunarity` times (default 2) finer and `gain` times (default 0.5) weaker than the one before, divided by the sum of the weights: `-1 .. 1`. Every layer has its own noise and its own offset (the plain noise is 0 at the origin, so without offsets all layers would be). `gain` 0 .. 4, `lacunarity` 1 .. 16, else "Illegal argument". |

The `seed` selects another noise (any integer; different seeds give unrelated noises): use one seed for the height, another for the moisture, another for the forests. All arguments must be within +-4.4e9 ("Illegal argument" beyond). `NOISE` with one argument and `HASH()` are "Too few arguments", more than four (`NOISE`), four (`HASH`) or six (`FBM`) are "Too many arguments".

```
H=NOISE(7,X/400,Y/400)*60               ' hills 60 units high, one hill every 400 units or so
H=FBM(7,X/800,Y/800,5)*100              ' the same with finer detail on top
IF HASH(CX,CY,3)<0.2 THEN ...           ' a chunk has a ruin with a chance of one in five
```

**New reserved words** (extension words 36 .. 39: `HASH NOISE FBM RIDGED`): programs that use `HASH`, `NOISE` or `FBM` as the name of a variable, procedure or function must be changed. Of the programs in this repository only `river` and `river2` did (a `FUNC HASH`, now `RHASH`).

## Pure and portable

All three are pure functions: no state, no `RND`, so building a chunk does not disturb the game's random numbers, and **the same arguments give the same number on every machine**. This is done with integer arithmetic for the hash and `+ - *` and `floor` on doubles for the noise; the C library's `sin` / `cos` / `pow` are not used (the 2 D gradients are a table), and `noise.c` is compiled without fused multiply-add (`#pragma GCC optimize ("fp-contract=off")`), which would change the last bit on some CPUs. `noisetest1` checks golden values that were computed independently in Python from the definition below.

## The definition

- **Mixer** (`nz_mix`): the SplitMix64 finalizer, `x ^= x >> 30; x *= 0xbf58476d1ce4e5b9; x ^= x >> 27; x *= 0x94d049bb133111eb; x ^= x >> 31` (a bijection of the 64 bit numbers).
- **64 bit hash** (`nz_hash64(seed, a, b, c, d)`): `h = mix(seed + G)` with `G = 0x9e3779b97f4a7c15`, then for each of `a, b, c, d`: `h = mix((h ^ v) + G)`. `HASH` is `nz_hash64(0, ...) >> 11` times 2^-53.
- **Noise**: the lattice point `(ix, iy, ...)` gets a gradient from the hash of the seed (changed per dimension), the coordinates and nothing else (top bits choose the direction); the value is the fade-weighted interpolation of the dot products of each gradient with the vector from its lattice point, scaled so that the extreme is 1 (1 D: 1 / 0.5, 2 D: 1 / (sqrt(2) / 2), 3 D the same factor; measured reach with a dense scan: 1 D 1.0, 2 D 0.975, 3 D 0.998), clamped to -1 .. 1.
- **FBM**: octave `o` has the seed `nz_hash64(seed, o, 0x4f43, 0, 0)` and the offset `(h >> 40, (h >> 16) & 0xffffff) / 16384` added to the coordinates (0 .. 1024).

## Measured

`noisetest1` (this machine, `-O3 -march=native`): `HASH` 25 ns, 2 D noise 155 ns, `FBM` of 6 octaves 1.2 microseconds per call in C. Hash: uniform in 16 bins (chi-square), every one of the 53 bits balanced within 1 %, an input bit flip changes 32.0 of 64 output bits on average. Noise: range, reach, mean (below 0.02 off zero), standard deviation 0.2 .. 0.6, zeros at all lattice points, smooth (a step of 1e-4 changes it by far less than 1e-3), no seam at the origin, finite at +-4e9, the same results twice.

## Not done / ideas

- Value and simplex noise, domain warping, a 3 D `FBM`, a `NOISE` with a period (tiling); a `HASH` over strings.
- Done next (2026-11-20): the terrain grid primitive, `M3D GROUND` / `M3D TERRAIN` (`docs/3d-mesh-subsystem.md`): a chunk's ground is built in C from `FBM`.
