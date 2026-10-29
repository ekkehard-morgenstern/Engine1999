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
SOUND ch, freq, beats [, vol]    a tone of freq Hz (20..20000) for "beats", volume 0..1 (default 0.5).
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
Chords: use one string per note (`PLAY "C","E","G"`), voices of one `PLAY` start together.

Instructions: `SND` (parameter = `SNDK_*`: 1 SOUND, 2 PLAY, 3 WAVE, 4 TEMPO, 5 OFF, 6 SYNC) and `PLNG`; see `instset.h`. The VM calls the
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

## Proposal: sound effects *(needs the owner's decision, nothing of this exists yet)*

Effects like laser shots, explosions, engine hum or power-ups are continuous changes (glide, wobble, fade), not note sequences.
They should cost one statement (one event), not hundreds, and the per-sample work belongs into the synth. Suggested, in this order:

1. **Immediate control for game loops** — `PITCH ch, freq` and `VOLUME ch, vol` take effect now (no queue, no allocation; a few
   atomic stores, so 60 calls per frame with `EVERY` are no problem): an engine sound that follows the car's speed.
2. **Glide** — an optional fifth argument of `SOUND`: `SOUND ch, f1, beats, vol, f2`: the tone glides from f1 to f2 (exponentially, i.e.
   evenly in musical terms) during the duration; the synth moves the frequency per sample. A laser is `SOUND 1,2000,0.15,0.6,200`.
3. **Envelope** — `ENVELOPE ch, attack, decay, sustain, release` (seconds and a sustain level 0..1) shapes every following tone of
   the channel (the fixed 2.5 ms fade is the default envelope). A plucked sound or a swell needs nothing else.
4. **Modulation** — `VIBRATO ch, rate, depth` (Hz, semitones) and `TREMOLO ch, rate, depth` (Hz, 0..1): low frequency oscillators in the synth;
   0 switches them off. `WAVE ch, 1, duty` could add a pulse width for the square wave (sweeping it gives the classic chorus effect).
5. **Stored effects** — `SFX n, "W3 V1 E0,.01,0,.3 G8000,200,.4"` defines effect n with the music string language extended by
   effect commands (`E` envelope, `G` glide, `~` vibrato, `^` tremolo); `SFX n` (or `SFX n, ch`) plays it on a channel: the host expands
   it into a handful of events with parameters (not one per step) and queues them in one go. Effects can be defined once at the
   start of a program and triggered by name-less numbers from the game, like `PLAY` without parsing at run time.
6. **Instruments** (the FM plan below) generalize 3 and 4: an instrument = operators + envelopes + modulation; `PLAY`'s `I n` selects it,
   `SFX` strings may use it.

Items 1 to 4 need small extensions of `synth_chan_t` (a target frequency and a glide rate, an envelope state, two LFO phases) and per-sample
code in `synth_render()` only, plus the events/instructions to set them; 5 is a parser in the style of `mml.c`.

## Instruments (planned)

The goal is a software FM synthesizer so games can have proper music and sound effects. The structure is prepared for it:
`synth_osc_t` (one oscillator: wave, phase, step) is separate from `synth_chan_t` (gain, fade), so an instrument can later
be a set of oscillators per channel with operator routing (modulation = one oscillator's output added to another's phase),
a frequency ratio and level per operator, and an ADSR envelope per operator or voice instead of the fixed 2.5 ms slew.
Instruments would be defined by a statement (`INSTRUMENT n, ...`) or loaded from data, selected by `PLAY`'s `I n` and by
`sdlaud_setinstrument()`; the basic wave types stay as instrument 0..3. Further steps: a polyphonic voice allocator (more
simultaneous notes than channels), band-limited square/triangle, per-channel panning once the output is stereo.
