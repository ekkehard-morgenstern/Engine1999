# Sound

Software synthesizer, split in two layers so the core can be tested without a sound device:

| File | Role |
|---|---|
| `mml.[ch]` | Parser of the `PLAY` music strings (no SDL): string → events with times in seconds. |
| `synth.[ch]` | The synthesizer core, no SDL: 8 channels (`SYNTH_NCHAN`), oscillators (sine, square, triangle, noise), mixer, a time-ordered event queue and the per-channel time cursors. All times are nanosecond time stamps passed in by the caller; it has no clock and no locking. Tested by `synthtest1`. |
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

Channels are numbered 1..8 in BASIC (0..7 in the host). Everything is queued and returns at once; nothing blocks until the music is over.

```
TEMPO bpm                        tempo of the beats of SOUND, 1..1000, default 60 (1 beat = 1 s)
WAVE ch, w                       wave type: 0 sine, 1 square, 2 triangle, 3 noise; applies to the sounds queued after it
SOUND ch, freq, beats [, vol [, f2]]  a tone of freq Hz (20..20000) for "beats", volume 0..1 (default 0.5), gliding to f2 (see effects).
                                 freq 0 = a rest of "beats". beats 0 = no end: the tone goes on until the next SOUND on the
                                 channel (or SOUND OFF); SOUND on a channel always follows what the channel has queued
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
V x    volume 0..1 with decimals (0.5)      W n   wave 0..3 (the channel keeps its wave otherwise)
ML MN MS   the note fills its length / sounds 7/8 of it (default) / 3/4 of it; the rest is silence, so equal notes in a row are heard separately
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
SFX n [, ch]                   plays effect n on channel ch; without ch on a free channel (the highest one that isn't busy, else 8)
```

- `PITCH`/`VOLUME` take effect immediately: no allocation, no queue (a lock and a few stores), so calling them every frame (60 per second,
  for instance from an `EVERY` timer once there is one) is cheap. Typical use: `SOUND 1,100,0` starts a tone without an end, then the
  game loop does `PITCH 1,100+speed*3`. `VOLUME` changes the volume of the sounding note (smoothly, no click).
- `ENVELOPE`, `VIBRATO`, `TREMOLO`, `WAVE` and the glide are events on the channel's time line, so they apply at the right place in a
  sequence. They stay in effect until changed. Without an `ENVELOPE` a note fades in and out within 2.5 ms (no click); with one the
  note rises in `a` seconds, falls to the sustain level in `d` seconds and holds it until its end, then falls to silence in `r` seconds
  (the channel counts as busy during the release; a new note during the release goes up from where the level is; times of 0 are instant).
- **Music string commands** for effects (`mml.[ch]`): `H f[,n]` (a tone of f Hz, for noise), `S f1,f2[,n]` (sweep), `X a,d,s,r` / `XR`
  (envelope / default), `~ rate,depth` (vibrato), `^ rate,depth` (tremolo). Numbers may have decimals, values are separated by
  commas. So a laser is `SFX 1,"W1 ML S3000,200,8"`, an explosion `SFX 2,"W3 ML X0,0.02,0.4,0.5 S800,60,2"`, a power-up
  `SFX 3,"W2 ML ~8,0.4 S300,1200,4"`.
- **`SFX n, ch`** starts from a clean channel: it first resets wave (sine), envelope, vibrato and tremolo of the channel; the
  effects the string sets stay afterwards (like for `WAVE`), so a following `SOUND` on that channel has them. Effects are stored in
  the VM: they stay across direct mode lines, but `RUN`, `NEW` and `LOAD` clear them (define them at the start of the program).
  The host receives the events like those of `PLAY`; the event count is small because sweeps and envelopes are single events.
- Wrong arguments are "Illegal argument", an `SFX` that was never defined is "Undefined effect".

Host interface: `vm_sound_t` has `tone( chan, freq, freq2, vol, beats )` (freq2 != 0: glide), `setenv`, `setvib`, `settrem`, `setpitch`,
`setvolume`; `sdlaud_playsweep`, `sdlaud_setenvelope/setvibrato/settremolo` (queued) and `sdlaud_setpitch/setvolume` (at once);
`synth_schedule_ev()` with `synth_evspec_t`, `synth_setpitch/setvolume`. Instructions: `SND` kinds 7..13 (see `instset.h`).

Not done: noise filtering (the noise is white), a pulse width for the square wave, a pitch envelope, stereo, `SFX` with parameters.

## Instruments (planned)

The goal is a software FM synthesizer so games can have proper music and sound effects. The structure is prepared for it:
`synth_osc_t` (one oscillator: wave, phase, step) is separate from `synth_chan_t` (gain, fade), so an instrument can later
be a set of oscillators per channel with operator routing (modulation = one oscillator's output added to another's phase),
a frequency ratio and level per operator, and an ADSR envelope per operator or voice instead of the fixed 2.5 ms slew.
Instruments would be defined by a statement (`INSTRUMENT n, ...`) or loaded from data, selected by `PLAY`'s `I n` and by
`sdlaud_setinstrument()`; the basic wave types stay as instrument 0..3. Further steps: a polyphonic voice allocator (more
simultaneous notes than channels), band-limited square/triangle, per-channel panning once the output is stereo.
