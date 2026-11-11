# Sound

Software synthesizer, split in two layers so the core can be tested without a sound device:

| File | Role |
|---|---|
| `mml.[ch]` | Parser of the `PLAY` music strings (no SDL): string → events with times in seconds. |
| `synth.[ch]` | The synthesizer core, no SDL: 16 channels (`SYNTH_NCHAN`: 1..8 music, 9..16 effects), oscillators (sine, square, triangle, noise), mixer, a time-ordered event queue and the per-channel time cursors. All times are nanosecond time stamps passed in by the caller; it has no clock and no locking. Tested by `synthtest1`. |
| `sdlaudio.[ch]` | The host side: opens the SDL audio device (mono, float, 44100 Hz, 1024-sample buffers), runs a worker thread that applies the queued events when their time has come (looks at the queue at least every millisecond, using `sdlutil_getnsec/nanosleep`), and the audio callback that renders the synth. One mutex guards the synth. The `sdlaud_*` calls do nothing (`sdlaud_busy()` is false) if no audio device could be opened (`sdl_init()` then prints a warning and runs without sound). |

## API (`sdlaudio.h`)

`sdlaud_playnote( chan, note, vol, beats )`, `sdlaud_playfreq( chan, freq, vol, beats )`, `sdlaud_stopchan( chan, beats )`,
`sdlaud_setwave( chan, wave )`, `sdlaud_setbpm( bpm )` / `sdlaud_getbpm()`, `sdlaud_syncchans( mask )`, `sdlaud_silencechan( chan )`,
`sdlaud_silenceall()`, `sdlaud_busy( chan )` (the channel sounds, fades, has events queued or a sequence still running).

- **Notes** 0..71 (0 = 110 Hz = A2, 12 per octave, up to ~6.6 kHz); **frequencies** 20..20000 Hz; **volume** 0..1; invalid arguments are ignored.
- **Time.** Every channel has a *cursor*: the time its next event is due. Each call takes the event's start time from the cursor and moves the cursor on by `beats` (1 beat = 60/bpm s; default 60 bpm = 1 s). So a melody is queued by simply calling `sdlaud_playnote()` once per note with its length; the calls return at once and the worker plays them at the right times. A rest is `sdlaud_stopchan( chan, beats )`. A cursor that has fallen behind the clock (the sequence has run out) restarts at the current time. `sdlaud_syncchans( mask )` moves the cursors of the channels in the mask to the latest of them, so a chord (or several voices) can be started together: sync, then queue the voices.
- **Order.** The queue is sorted by time (equal times keep the order queued), so a channel with events far ahead does not hold up the others.
- **Wave types**: `SYNTH_SINE`, `SYNTH_SQUARE`, `SYNTH_TRIANGLE`, `SYNTH_NOISE` (sample-and-hold white noise that changes at the channel's frequency). `sdlaud_setwave()` is a queued event too (no duration). Square and noise are scaled to ±0.5 so that all waves are of similar loudness at the same volume; square/triangle are naive (not band-limited), so high notes alias a bit.
- **No clicks**: volume changes are slewed over 2.5 ms; a pitch change on a sounding note keeps the phase; a new note starts at phase 0. The mixer sums the channels and scales the sum down when the channel volumes add up to more than 1 (no clipping).
- **Precision.** Event times are exact to the worker's wake-up (≈1 ms), not sample-exact; the pitch is exact (phase accumulator).

## BASIC statements (implemented)

Channels are numbered 1..16 in BASIC (0..15 in the host): **1..8 are the music channels** (`PLAY` voice n plays on channel n; `SOUND`, `WAVE`, ... as you like) and **9..16 the effects channels**, which `SFX` without a channel uses (the highest free one, 16 down to 9; a music channel is never taken; with all eight busy it takes channel 16 and the effect waits behind what that channel has queued). Any statement may use any channel 1..16, `SFX n, ch` too (the split is a convention, not a restriction; before 2026-11-07 there were 8 channels and `SFX` took the highest free one of them, which could hijack a music channel). Programs that use only channels 1..8 are not affected. Everything is queued and returns at once; nothing blocks until the music is over.

```
TEMPO bpm                        tempo of the beats of SOUND, 1..1000, default 60 (1 beat = 1 s)
WAVE ch, w                       wave type: 0 sine, 1 square, 2 triangle, 3 noise, or the instrument 4..63 (see "Instruments"); applies to the sounds queued after it
SOUND ch, freq, beats [, vol [, f2]]  a tone of freq Hz (20..20000) for "beats", volume 0..1 (default 0.5), gliding to f2 (see effects).
                                 freq 0 = a rest of "beats". The tone ends after its beats (like a note of sheet music; a rest adds silence on top).
                                 beats -1 = no end: the tone goes on until the next SOUND on the
                                 channel (or SOUND OFF); beats 0 = a note of zero length: nothing sounds, but the
                                 statement's state changes happen (an earlier held tone ends); SOUND on a channel always follows what the channel has queued
SOUND OFF [ch]                   silences the channel (no channel or 0 = all) now and forgets what is queued for it
SOUND SYNC ch [, ch ...]         lines up the time of the channels, so that what is queued next starts together on all of them
PLAY a$ [, b$ ...]               music strings, up to 8; string n plays on channel n, all voices start together (PLAY syncs
                                 the voices' channels first, so they start after what these channels still have queued)
PLAYING( ch )                    -1 while the channel sounds or has something queued (also a rest that is still to come), else 0
```

A wrong argument is "Illegal argument". `PLAY` parses all its strings before it queues anything: a mistake is reported (`? Unknown
music command 'X' in line 20`) and nothing plays. Without an audio device (or in `repltest1`, which has no sound output) the
statements check their arguments and do nothing, and `PLAYING` is 0. `WAVE` is an event at the channel's current time (it takes
no time itself), so `WAVE 1,1: SOUND 1,440,1` and `SOUND 1,440,1: WAVE 1,1: SOUND 1,880,1` do what they say: in the second the
first note is a sine and the second one a square.

Music strings (`mml.[ch]`, case-insensitive, blanks and `;` are ignored):

```
A..G [#|+|-] [n] [.]   a note (# or + sharp, - flat), n = length: 1 whole, 2 half, 4 quarter ... 64; each dot adds half of what
                       is before; without n the default length (L) applies
R or P [n] [.]         a rest
O n    octave 0..8 (O4: middle C = C4, A4 = 440 Hz); <  >   one octave down / up
L n    default length 1..64 (4)      T n   quarter notes per minute 1..1000 (120)
V x    volume 0..1 with decimals (0.5)      W n  or  I n   wave 0..3, or the instrument 4..63 (the channel keeps its wave / instrument otherwise)
ML MN MS   the note fills its length / sounds 7/8 of it (default) / 3/4 of it; the rest is silence, so equal notes in a row are heard separately
[CEG]n     a chord (2.. 16 notes: A..G with # + -, and < > for the octave): the notes start together and last for the length n (dots too); an instrument with
           POLY gives each note a voice (see "Polyphony"), a single-voice instrument plays only the last one
K m[,s]    the filter cutoff of an instrument: m times the note frequency (0.01..64), reached within s seconds (0..60, default at once); KR[,s]: the instrument's own again
```

Example: `PLAY "T120 O4 L8 CDEF G4 G4", "T120 O2 L2 C G"` (melody and bass line), `PLAY "W3 V1 ML O1 C16"` (a drum-like noise burst).
The duration in a music string is in seconds (from its `T`), converted to the host's beats with the tempo of `TEMPO`, so `TEMPO`
doesn't change how fast a `PLAY` string plays. Notes below 20 Hz (octave 0 C..) are errors. A string may have 100000 events at most.
Chords: use one string per note (`PLAY "C","E","G"`), voices of one `PLAY` start together. The commands for sound effects (`H`, `S`, `X`, `~`, `^`) are described below.

Instructions: `SND` (parameter = `SNDK_*`: 1 SOUND, 2 PLAY, 3 WAVE, 4 TEMPO, 5 OFF, 6 SYNC, 7..13 effects) and `PLNG`; see `instset.h`. The VM calls the
host through `vm_sound_t` (`vm.h`: tone, setwave, setbpm, getbpm, sync, silence, busy), `repl_t.sound` hands it to the VM,
`engine1999.c` maps it to `sdlaud_*`. Tests: `vmtest1` (sound output that logs the calls), `repltest1` (no sound output),
`synthtest1` (`mml`, queue, busy), `asmtest1` (encodings, round trip).

## Cost of many short notes

Every `SOUND` / note of a `PLAY` string becomes one queued event (one allocation, a lock, an insertion at the end of the
time-ordered queue). Measured with `SDL_AUDIODRIVER=dummy`: 3000 notes of 1.2 ms (a "high BPM effect") cost 0.001 s CPU to queue and
about 0.1 s CPU for the 3.6 s they play (the audio callback itself is most of it); an idle audio engine costs <0.5 % of a core
(the worker sleeps on a condition variable while the queue is empty). Queueing in time order is O(1) (queue tail); the
queue holds 262144 events. So thousands of events per second are fine for the host, but a BASIC program that issues `SOUND` from a
loop per step pays the VM cost per call, and a rapid pitch sweep made of notes is a clumsy way to describe a smooth effect.
Hence the proposal below: describe effects by a few parameters and let the synth do the per-sample work.

## Sound effects (implemented)

Effects like laser shots, explosions, engine hum or power-ups are continuous changes (glide, wobble, fade), not note sequences. They cost
one statement (one event), and the synth does the per-sample work (`synth_render()`), so a smooth effect doesn't need thousands of notes.

```
PITCH ch, freq                 at once (no queue): the pitch of the channel (20..20000 Hz); stops a glide
VOLUME ch, vol                 at once: the volume of the channel's note (0..1; 0 = silent, but the note goes on and can be raised again)
SOUND ch, f1, beats, vol, f2   the pitch glides from f1 to f2 (exponentially: evenly in musical terms) during the tone; f2 20..20000, beats > 0
ENVELOPE ch, a, d, s, r        attack, decay (seconds 0..60), sustain level 0..1, release (seconds) for the tones that follow; ENVELOPE ch = default
VIBRATO ch, rate, depth        pitch LFO: rate 0..100 Hz, depth 0..24 semitones (0 = off)
TREMOLO ch, rate, depth        volume LFO: rate 0..100 Hz, depth 0..1 (0 = off)
SFX n, a$                      stores the music string a$ as effect n (1..64); it is parsed at once (a mistake is an error now)
SFX n [, ch]                   plays effect n on channel ch; without ch on a free effects channel (the highest of 9..16 that isn't busy, else 16)
```

- `PITCH`/`VOLUME` take effect immediately: no allocation, no queue (a lock and a few stores), so calling them every frame (60 per second,
  for instance from an `EVERY` timer once there is one) is cheap. Typical use: `SOUND 1,100,0` starts a tone without an end, then the
  game loop does `PITCH 1,100+speed*3` (and `WAIT 1` for a frame without busy-waiting; see `demos/sfx.bas`). `VOLUME` changes the volume of the sounding note (smoothly, no click).
- `ENVELOPE`, `VIBRATO`, `TREMOLO`, `WAVE` and the glide are events on the channel's time line, so they apply at the right place in a
  sequence. They stay in effect until changed. Without an `ENVELOPE` a note fades in and out within 2.5 ms (no click); with one the
  note rises in `a` seconds, falls to the sustain level in `d` seconds and holds it until its end, then falls to silence in `r` seconds
  (the channel counts as busy during the release; a new note during the release goes up from where the level is; times of 0 are instant).
- **`CUTOFF ch, mult [, seconds]`** (roadmap item 87, token 76) is the statement form of `K`: the filter cutoff of the instrument on the channel glides (geometrically: the same number of octaves a second) to `mult` x the note frequency within `seconds` (default 0 = at once); `mult` -1 = the instrument's own cutoff again (the glide goes there and the override ends). It is an event on the channel's time line like `VIBRATO`, so a sweep in the middle of a `PLAY` sequence is `PLAY "I20 K1 C K30,2 D"`. An instrument without a filter is wide open (64 x the note) until a `CUTOFF` gives it one; selecting another instrument drops the setting; `SFX` gives it back like the other settings. Values: `mult` -1 or 0.01..64, `seconds` 0..60, else "Illegal argument". Host: `vm_sound_t.setcut`, `sdlaud_setcutoff`, `SYNTH_EV_CUT`; instruction `SND` kind 16.
- **Music string commands** for effects (`mml.[ch]`): `H f[,n]` (a tone of f Hz, for noise), `S f1,f2[,n]` (sweep), `X a,d,s,r` / `XR`
  (envelope / default), `~ rate,depth` (vibrato), `^ rate,depth` (tremolo). Numbers may have decimals, values are separated by
  commas. So a laser is `SFX 1,"W1 ML S3000,200,8"`, an explosion `SFX 2,"W3 ML X0,0.02,0.4,0.5 S800,60,2"`, a power-up
  `SFX 3,"W2 ML ~8,0.4 S300,1200,4"`.
- **`SFX n, ch`** starts from a clean channel: it first resets wave (sine), envelope, vibrato and tremolo of the channel; the
  when the effect is over (after the release of its envelope) it gives the channel back as the program had set it before the effect (the wave, envelope, vibrato and tremolo of an earlier `WAVE`, `ENVELOPE`, `VIBRATO`, `TREMOLO` or of the `W` `X` `~` `^` commands of a `PLAY`; the defaults if it had set none), so a following `SOUND` on that channel sounds as before; the channel counts as busy until then. (`PLAY` leaves its settings, like `WAVE`.) The VM keeps what it set on each channel (`vm_t.chanstate`) and restores only what the effect changed; before 2026-11-07 the effect reset the channel to the defaults, which silenced `orchard2`'s march (a low sine instead of the square wave a `PLAY` had left on the channel). An `SFX` without a channel takes the highest free channel, so this matters when all higher ones are busy. Effects are stored in
  the VM: they stay across direct mode lines, but `RUN`, `NEW` and `LOAD` clear them (define them at the start of the program).
  The host receives the events like those of `PLAY`; the event count is small because sweeps and envelopes are single events.
- Wrong arguments are "Illegal argument", an `SFX` that was never defined is "Undefined effect".

Host interface: `vm_sound_t` has `tone( chan, freq, freq2, vol, beats )` (freq2 != 0: glide), `setenv`, `setvib`, `settrem`, `setpitch`,
`setvolume`, `setinst`, `setcut`, `chord`; `sdlaud_playsweep`, `sdlaud_setenvelope/setvibrato/settremolo` (queued) and `sdlaud_setpitch/setvolume` (at once);
`synth_schedule_ev()` with `synth_evspec_t`, `synth_setpitch/setvolume`. Instructions: `SND` kinds 7..13 (see `instset.h`).

Not done: stereo, `SFX` with parameters (a filter, a pulse width and a pitch envelope exist now as parts of the instruments in `synth.[ch]`, see "Instruments", but have no statements yet).

## Sound log (debugging)

`ENGINE1999_SOUNDLOG=file engine1999 ...` (or `-` for standard error) writes a text log of the sound output of the window host, off by default: one line for every call the VM makes to the host (`tone freq f freq2 f2 vol v beats b` where beats -1 = held and 0 = a note of zero length, `wave`, `envelope`, `vibrato`, `tremolo`, `pitch`, `volume`, `tempo`, `sync mask`, `silence`), each with the time in seconds since the first sound call and the channel (`ch1`..`ch8`, `all`), and twice a second a `state` line: tempo, length of the event queue and for each channel `on` / `off`, the gain, and `+s` (how many seconds ahead of now the channel is queued) or `idle`, and `pkX`: the loudest sample the channel actually *rendered* since the last state line (so `pk0.00` on a channel that is asked for tones = nothing audible came out), then for the mix `mix pkX`, the number of calls of the audio device's callback since the last line, their size in samples, and the longest pause between two calls. (The state lines come every 0.5 s: a sound that repeats every 0.25 s is always seen in the same phase, so look at `pk`, not at `on` / `off`.) The state lines are written by the audio worker (`sdlaud_setlog()`). Use it to see whether a missing sound is never asked for (no `tone` line), asked for but not played (state of the channel), or queued far ahead. With `SDL_AUDIODRIVER=disk` (`SDL_DISKAUDIOFILE=out.raw`, 32-bit float mono, 44100 Hz) the sound can also be recorded without a sound card. Not available in the console host (it has no sound).

## Instruments (roadmap item 81; core 2026-11-10, language the same day)

`synth.[ch]` has instruments: instruments 0..3 are the four basic waves (the original signal path, unchanged sample for sample),
instruments 4..63 are *subtractive / layered* voices. An instrument (`synth_inst_t`) has:

| Part | Fields |
|---|---|
| 1..3 oscillators | wave (sine, square, triangle, noise, **saw**), level 0..2, frequency ratio to the note (noise: 0 = white noise at the sample rate), detune in cents, pulse width 0.02..0.98 for the square; **a negative ratio is a fixed frequency** of -ratio Hz (1..20000: the oscillator ignores the note, the pitch envelope and the vibrato; for noise it is the rate of the sample-and-hold) and the flag **`POST`** lets the oscillator bypass the filter (it is mixed in behind it). Square, pulse and saw are **band-limited** (polyBLEP), so high notes do not alias much. |
| low-pass filter | cutoff = note frequency x `cutmul` (follows the note and its glide / vibrato; 0 = no filter), resonance 0..0.95, a filter envelope (`fenv` octaves above the cutoff at the note start, decaying with the time constant `fdecay`). A state variable filter in TPT form: stable at every setting (the first version, Chamberlin, blew up at high cutoff with damping: found by the `PLUCK` preset). The coefficients are recomputed every 32 samples. |
| pitch envelope | `pitch` semitones off at the note start, decaying with the time constant `pitchtime` (kick, tom, zap). |
| default ADSR | used while the program has set no `ENVELOPE` on the channel; an `ENVELOPE` wins, `ENVELOPE ch` (default) gives the instrument's back. |
| default vibrato | used while the program has set no `VIBRATO` (depth 0) on the channel. |
| gain | the drive into a soft limiter (0..4): the signal is untouched up to 0.8 and compressed smoothly towards 1.0 above it, so the instrument's output never exceeds the volume asked for (a plain gain-1 sine peaks at about 95 % of it) and a high gain makes the instrument louder on average without clipping. |

**Selecting** (BASIC): `WAVE ch, n` with n = 0..31 (0..3 the basic waves, 4..63 instruments), `W n` or `I n` in a `PLAY` string. It is an event on
the channel's time line like `WAVE` always was, so it applies at the right place in a sequence; `vm_chanstate_t` keeps it and `SFX` gives it back.
**Defining**: `INSTRUMENT n, a$` (n = 4..63; 0..3 are fixed: "Illegal argument") parses the definition string at once (a mistake is an error now) and hands the instrument to the host;
`INSTRUMENT n` puts the factory instrument back (4..31 a preset, 32..63 a plain sine named `USER`).

**Redefining is queued, like `PLAY` (2026-11-10, owner's design).** The VM keeps the program's definitions. Every time a channel selects an instrument 4..63 (`WAVE`, `W` / `I` in `PLAY`)
it gets a **snapshot** of the definition as it is *at that moment of the program*, queued on the channel's time line, and the channel plays with its own copy (`SYNTH_EV_INSTDEF`).
`INSTRUMENT n, def$` stores the new definition and then queues a fresh snapshot, at each channel's own cursor, on every channel that currently uses instrument n. So notes
queued *before* the statement keep the old sound and notes queued *after* it use the new one, however far ahead the music has been queued and whatever the other channels have queued:

```
PLAY "I20 O2 L8 CDEF GAB>C"           ' plays with the instrument as it is now
INSTRUMENT 20, "OSC SQUARE 0.5 1 0 0.2; FILTER 2"
PLAY "I20 O2 L8 CDEF GAB>C"           ' starts after the first one and uses the new definition (a PLAY without I20 does too: channel 1 still uses 20)
```
Channels that use another number are not touched. A change comes at a note boundary, so a note that sounds does not change under you. A program's instruments are put back to
factory when the program run ends or is replaced (`RUN`, `NEW`, `LOAD`; `CHAIN` keeps them like the `SFX` definitions); channels keep the snapshots they hold until they select something else.
Without a sound output the statement only checks its arguments.
(C side: the synth's own table `synth_setinst()` is only the default of `SYNTH_EV_INST` / `SYNTH_EV_WAVE` with a number; the VM uses `SYNTH_EV_INSTDEF` through
`vm_sound_t.setinst( chan, n, def )` and `sdlaud_setinst( chan, n, def )`.)

Definition string (`mml_parse_inst()`, `mml.h`): commands separated by `;`, values by blanks or commas, case does not matter. It starts from a plain sine instrument:

```
NAME w                                 up to 11 letters / digits / _
OSC wave [level [ratio [cents [pw]]]] [FM index] [MOD] [POST]
                                       up to 3; wave = SINE SQUARE TRIANGLE NOISE SAW (or 0..4); level 0..2 (1), ratio 0..64 (1; noise 0 = white noise;
                                       NEGATIVE = a fixed frequency of -ratio Hz, 1..20000, independent of the note), cents -2400..2400 (0), pulse width
                                       of SQUARE 0.02..0.98 (0.5); POST = the oscillator bypasses the filter (mixed in behind it);
                                       FM index (0..20 radians) = the NEXT oscillator modulates the phase of this one; MOD = a modulator only (not mixed)
FILTER cutmul [res [fenv [fdecay]]]    low-pass at note frequency * cutmul (0..64); resonance 0..0.95; fenv -6..6 octaves; fdecay 0..10 s (0.2)
PITCH semitones seconds                pitch envelope (-48..48, 0..10)
ENV a d s r                            default envelope (seconds 0..60, sustain 0..1)
VIB rate depth                         default vibrato (0..100 Hz, 0..24 semitones)
FMDECAY seconds                        the FM indexes fall exponentially with this time constant (0..10; 0 = they stay)
POLY n                                 voices 1..8 (1): see "Polyphony"
GAIN g                                 overall level 0..4 (1)
```
Example: `INSTRUMENT 20, "NAME MYBASS; OSC SAW 0.8; OSC SQUARE 0.6 0.5 0 0.3; FILTER 3 0.3 2 0.15; ENV 0.005 0.2 0.6 0.1; GAIN 0.6"`, then `PLAY "I20 O2 L8 CDEF"`.
Errors: "Unknown instrument command 'X'", "Wave name expected after OSC ...", "POST, MOD or FM expected after the OSC values", "A number is missing after FM", "At most 3 oscillators", "Wrong number of values in the instrument definition",
"Instrument value out of range".

A new note (a channel that was not sounding, or one in its release) restarts the pitch and filter envelopes; a legato note (a `PLAY` on a sounding channel without an
envelope release in between) only changes the pitch. C API: `synth_setinst( s, n, def )` (4..31, every field range checked), `synth_getinst`,
`synth_instname`, `synth_instdefault`, `synth_instpreset`; the instruments live in `synth_t`, so every synth starts with the presets.

**Presets** (a first draft: names and sounds are to be agreed with the owner, he has not heard them): 4 BASS, 5 LEAD, 6 PAD, 7 PLUCK, 8 ORGAN,
9 STRINGS, 10 BRASS, 11 FLUTE, 12 BELL, 13 KICK, 14 SNARE, 15 HIHAT, 16 TOM, 17 CLAP, 18 ZAP, 19 EPIANO 20 ACOUSTIC, 21 METAL, 22 DISTGUIT, 23 METALLOPHONE, 24 MARIMBA, 25 HARP, 26 CLARINET, 27 ACIDBASS, 28..31 UNASSIGNED (a plain sine; placeholders for later presets) (32..63 are free for the user's own instruments;
they start as a plain sine named `USER`). **Second set 2026-11-11 (not heard by me):** ACOUSTIC steel-string guitar (saw + octave, falling filter, `POLY 6`), METAL palm-muted rhythm guitar (FM saw on saw, `POLY 3` for power chords), DISTGUIT sustained distortion lead (FM index 3, vibrato, `POLY 2`), METALLOPHONE (FM bar, `POLY 6`), MARIMBA (FM with a 4 x modulator whose index dies in 60 ms, `POLY 6`), HARP (`POLY 8`), CLARINET (square + triangle, filter, vibrato), ACIDBASS (resonant saw, filter envelope). Distortion is made with FM (the gain is only the drive of a soft limiter, and every preset is calibrated to RMS 0.34). The names may be 15 characters long now. **Revised 2026-11-11 with FM and polyphony (not heard by me):** BELL is an FM bell (carrier 1 modulated by a 3.5 x sine, index 4 falling with `FMDECAY 0.8`, plus the 5.4 x partial), EPIANO an FM electric piano (1:1, index 2 falling in 0.5 s, plus a quiet 14 x tine), BRASS an FM brass (index 3 falling in 0.6 s, a detuned saw added); PAD, ORGAN, STRINGS, BELL and EPIANO are `POLY 4`, PLUCK `POLY 3` (chords with `[CEG]`, notes ring out under the next one). Gains recalibrated (BRASS 1.24, BELL 1.89, EPIANO 2.14). BASS, LEAD, FLUTE and the percussion are unchanged and stay one voice. The percussion presets want a note of about 55 to 220 Hz (`O1 A` .. `O3 A`); their pitch envelopes are relative.

**The instrument editor (2026-11-10, roadmap item 85):** `tools/insted.bas` (`TOOL "insted"`) is a UI program in the style of `sfxed` that builds the definition string from sliders and plays it: tabs for the three oscillators (wave, level, ratio or **Fixed** Hz, cents, pulse width, **Post**, On), the filter and pitch envelope, and the envelope, vibrato and gain; `<` / `>` step through the factory instruments, `New` starts from a plain sine; **Play**, **Chord** (root, major third, fifth on channels 1 to 3), **Hold** (the note sounds until Stop and every change is heard at once: the instrument is redefined while it sounds, and the Note slider bends the pitch with `PITCH`), play on change; the string can be edited and applied with Return; Save / Open (`.ins`: one line), Export (`INSTRUMENT n,"..."` as a BASIC line), Copy (clipboard). It uses instrument 31. Details and the tests: `docs/ui-subsystem.md`, `docs/build-and-test.md` (`instedtest.sh`, `synthtest1`).

**Loudness of the presets (2026-11-10):** the gains of the 16 presets are calibrated so that a one-second note at volume 1 (A3; A1 for KICK and TOM) has an RMS of about **0.34** in every preset (a basic sine has 0.71, a triangle 0.58, a square 0.5) and a peak below 1.0: the volume `V` of a `PLAY` string means about the same for every instrument, and an instrument's `V` can be chosen like a wave's. `synthtest1` checks it (RMS 0.30 .. 0.38, peak <= 1). The calibration was done by measuring (an offline renderer replaying a music string like the VM and the host do; it was a scratchpad tool); redo it after changing a preset's sound (the gain is the first number on the second line of its row in `synth_presets`).

**Fixed-frequency oscillators and `POST` (2026-11-10, owner's request after listening to FLUTE):** the first FLUTE had its breath noise tied to the note (a noise oscillator at 8 x the note, then the key-following filter at 4 x the note), so its breath was a low rumble in low octaves (measured: the noise centred at about 190 Hz at A2, 800 Hz at A4, 3 kHz at A6) and only sounded like breath above octave 4. Now the breath is `OSC NOISE 0.08 -5000 POST`: a sample-and-hold noise at a fixed 5000 Hz, behind the filter, so it is the same at every note (measured: identical level and 2500 sign changes per second at A2 and A6). The same two features suit a pitch-independent hiss (a hi-hat), a chiff, or a fixed formant. A noise oscillator with ratio 0 is still white noise (a new value every sample).

Cost (`synthtest1`, one core, optimized build): 16 channels with the three-oscillator presets + filter = **about 5 % of a core**.
Tests: `synthtest1` (369 checks): table / names, every preset (finite, audible, peak <= 1, silent after its release), ratio and cents (pitch measured), saw alias level and
amplitude, pulse width harmonics, low-pass attenuation measured with a Goertzel filter, resonance at the limits, pitch envelope, envelope
precedence, `WAVE` after an instrument identical to the plain wave, validation; also under ASan / UBSan. `vmtest1` (definitions and errors, `WAVE` 4..63, `I` / `W` in `PLAY`, the `SFX` give-back, the order of a redefinition between two `PLAY`s),
`repltest1`, `asmtest1` (`SND 14` / `15`), `contest1` (the console host checks the instrument statements and strings, 7 cases). **The basic waves are pinned by a golden test** in `synthtest1`: 4 waves x 5 settings (plain, envelope, vibrato + tremolo, glide, wave change mid-note) were rendered with the synthesizer as it was before the instruments (commit `7b7361b`) and the expected sums, RMS and sample values are in the test (tolerance for other CPUs / compilers); the old and the new code are bit-identical on those scenarios, and a deliberate 2 % change of the square wave makes seven checks fail. **`make hosttest`** (`hosttest.sh`, not in `make check`) runs the window host under Xvfb with the disk audio driver and checks the recorded audio (sine, then a redefined square wave): `docs/build-and-test.md`. The sound log (`ENGINE1999_SOUNDLOG`) shows an `instrument n NAME ...` line on the channel for each selection / redefinition.

**Tests of roadmap item 87:** `synthtest1` +36 (FM sidebands / `MOD` / `FMDECAY` / chains, the cutoff event at once / gliding / `KR` / on an unfiltered instrument, `mml_inst_string()` round trip of all presets, polyphony: chords, tails, voice limit, stealing, busy / silence, vibrato hand-over, a 600-note stress run; also under ASan / UBSan), `vmtest1` +36 (`CUTOFF`, `K`, chords, `INSTRUMENT$`, `POLY`, FM errors, the `SFX` give-back of the cutoff), `repltest1`, `asmtest1`, and `hosttest.sh` now also records a `[ACE]` chord on a `POLY` instrument (0.100 per note) and a `K1` / `K40` cutoff opening (the 8th harmonic 0.0001 -> 0.0195) through the real window host.

**FM operators (roadmap item 87, 2026-11-11).** An oscillator with `FM index` has its phase modulated by the *next* oscillator of the definition (phase modulation, the
"operator" form of FM): the phase offset in radians is `index x (the modulator's output x its level)`, so the modulator's level scales the index too, and a chain
`osc 1 <- osc 2 <- osc 3` is a three-operator stack. `MOD` keeps a modulator out of the mix (it is heard only through the oscillator it modulates), so the
ordinary algorithms are: a carrier + a MOD modulator (two operators), a chain with MOD on the inner ones (stack), or one carrier + a modulator that is also
mixed (a "feedback-free" additive blend). `FMDECAY t` lets all indexes fall exponentially with the time constant t at every new note (the brightness of an
electric piano, a bell, a pluck falls with the note; 0 = constant). Ratios and cents of a modulator work as for any oscillator (ratio 1 = harmonic sidebands,
non-integer ratios = bell-like). Examples: `INSTRUMENT 20, "OSC SINE 0.5 1 0 0.5 FM 3; OSC SINE 1 2 0 0.5 MOD; FMDECAY 0.4; ENV 0 0.8 0 0.3; GAIN 1.8"` (an electric piano
like tone), `"OSC SINE 0.4 1 0 0.5 FM 6; OSC SINE 1 3.5 0 0.5 MOD; FMDECAY 1.5; ENV 0 1.5 0 1; GAIN 1.7"` (a bell). Measured (`synthtest1`): carrier 1000 Hz, modulator 500 Hz,
index 2: the sidebands have the amplitudes of the Bessel functions (0.5 x J1(2) = 0.288 at 1500 Hz), the modulator's level scales the index, `FMDECAY` removes the
sidebands (0.2 s: 0.22 at the start, 0.01 after 0.9 s), a three-operator chain stays within the level. Only the waves' phase is shifted: the band-limiting of
square and saw uses the shifted phase too, so strong FM of those waves can alias a little (sines are the normal carriers and modulators). The editor
`tools/insted.bas` keeps FM / MOD / FMDECAY / POLY of a definition (it has no sliders for them: edit them in its string field and press Return, **Hold** plays it).

**Polyphony (roadmap item 87).** A channel plays one note at a time, with the instrument's whole signal path: a new note while one sounds is legato, a note that follows a
note in its release takes the channel over, and a chord needs one channel per note. An instrument with **`POLY n`** (2..8) changes that for the channels that use it:
the channel then keeps only the *settings* (instrument, envelope, vibrato, tremolo, cutoff) and **every note is played by a voice** that takes a copy of them, from a pool of
`SYNTH_NVOICES` = 32 voice slots shared by all channels (the slots are ordinary channel structures after the 16 channels, so the whole signal path is the same code; the
mixer scales a loud sum down as before). Rules (`synth.c`, `synth_apply`):
- **A `PLAY` event at another time than the channel's last one ends the notes the channel holds** (they *release*: their envelope tails go on sounding under the new note); **`PLAY`
  events at the same time are a chord**; a `STOP` (the end of a note, a rest) releases all held notes. So `PLAY "I20 O3 L8 CDEFG"` with a long release is a run of overlapping tails, and
  `PLAY "I20 [CEG]2 [DFA]2"` is two chords.
- At most `n` notes of a channel sound at once (releasing ones count): the next note takes over the quietest releasing note, else the oldest. A full pool takes the channel's own
  oldest voice, or drops the note if the channel has none (a note is never stolen from another channel).
- A change of **vibrato, tremolo or `CUTOFF`** reaches the notes that sound; **`WAVE`, `ENVELOPE`, `INSTRUMENT`** apply to the notes started after them (a note keeps its envelope);
  `VOLUME` changes all sounding notes, `PITCH` the newest one. Selecting a single-voice instrument releases the notes the channel holds. `PLAYING`, `SOUND OFF`, `SOUND OFF ch` count and end the voices too.
- `[CEG]n` in a `PLAY` string (`mml.c`) emits `MML_CHORD` events for all notes but the last (the VM's `vm_sound_t.chord`, `sdlaud_playchord`: a `PLAY` event that does not move the
  channel's time) and the last as the note with the length; a host without `chord` plays only the last note. Chord notes of a single-voice instrument therefore replace each other
  (only the last is heard) — define `POLY` for chords.
Measured (`synthtest1`, `hosttest.sh`): a chord of three sines of level 0.3 at volume 0.5 gives 0.1 per note (the sum of the gains 1.5 scales it, exactly as before for three channels);
the tail of a note 0.5 s earlier (release 0.6 s) is 0.07 under the next note with POLY and absent without it; 600 random notes on 16 channels with a too small pool are finite, within range and ASan / UBSan clean.

**`INSTRUMENT$(n)` (roadmap item 87):** the definition string of instrument n (0..63) as it is now, the program's own or the factory one (`mml_inst_string()`; the format is the one `INSTRUMENT n, a$` reads,
all the parts a definition has: `PRINT INSTRUMENT$(9)` gives `NAME STRINGS; OSC SAW 0.33 1 -12 0.5; ...; GAIN 2.03`; a name-less one is `NAME USER`; 0..3, the basic waves, give only `NAME SQUARE` etc.
because they are not definable; out of range "Illegal argument"). `INSTRUMENT 20, INSTRUMENT$(11)` copies a preset to take it apart; the name is `MID$(INSTRUMENT$(n),6,...)` up to the first `;`.
Instruction `INSS` (0x190), token 75 in an expression.

**Not done (roadmap 87 / 81):** a review of the presets by ear (the instrument editor now makes tuning easy; the preset gains were calibrated for loudness only), panning once the
output is stereo, an FM feedback path (an operator modulating itself), algorithms beyond the chain (one modulator for two carriers).
