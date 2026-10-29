# Sound

Software synthesizer, split in two layers so the core can be tested without a sound device:

| File | Role |
|---|---|
| `synth.[ch]` | The synthesizer core, no SDL: 8 channels (`SYNTH_NCHAN`), oscillators (sine, square, triangle, noise), mixer, a time-ordered event queue and the per-channel time cursors. All times are nanosecond time stamps passed in by the caller; it has no clock and no locking. Tested by `synthtest1`. |
| `sdlaudio.[ch]` | The host side: opens the SDL audio device (mono, float, 44100 Hz, 1024-sample buffers), runs a worker thread that applies the queued events when their time has come (looks at the queue at least every millisecond, using `sdlutil_getnsec/nanosleep`), and the audio callback that renders the synth. One mutex guards the synth. The `sdlaud_*` calls do nothing if no audio device could be opened (`sdl_init()` then prints a warning and runs without sound). |

## API (`sdlaudio.h`)

`sdlaud_playnote( chan, note, vol, beats )`, `sdlaud_playfreq( chan, freq, vol, beats )`, `sdlaud_stopchan( chan, beats )`,
`sdlaud_setwave( chan, wave )`, `sdlaud_setbpm( bpm )`, `sdlaud_syncchans( mask )`, `sdlaud_silenceall()`.

- **Notes** 0..71 (0 = 110 Hz = A2, 12 per octave, up to ~6.6 kHz); **frequencies** 20..20000 Hz; **volume** 0..1; invalid arguments are ignored.
- **Time.** Every channel has a *cursor*: the time its next event is due. Each call takes the event's start time from the cursor and moves the cursor on by `beats` (1 beat = 60/bpm s; default 60 bpm = 1 s). So a melody is queued by simply calling `sdlaud_playnote()` once per note with its length; the calls return at once and the worker plays them at the right times. A rest is `sdlaud_stopchan( chan, beats )`. A cursor that has fallen behind the clock (the sequence has run out) restarts at the current time. `sdlaud_syncchans( mask )` moves the cursors of the channels in the mask to the latest of them, so a chord (or several voices) can be started together: sync, then queue the voices.
- **Order.** The queue is sorted by time (equal times keep the order queued), so a channel with events far ahead does not hold up the others.
- **Wave types**: `SYNTH_SINE`, `SYNTH_SQUARE`, `SYNTH_TRIANGLE`, `SYNTH_NOISE` (sample-and-hold white noise that changes at the channel's frequency). `sdlaud_setwave()` is a queued event too (no duration). Square and noise are scaled to ±0.5 so that all waves are of similar loudness at the same volume; square/triangle are naive (not band-limited), so high notes alias a bit.
- **No clicks**: volume changes are slewed over 2.5 ms; a pitch change on a sounding note keeps the phase; a new note starts at phase 0. The mixer sums the channels and scales the sum down when the channel volumes add up to more than 1 (no clipping).
- **Precision.** Event times are exact to the worker's wake-up (≈1 ms), not sample-exact; the pitch is exact (phase accumulator).

## Proposal: `SOUND` and `PLAY` *(needs the owner's decision, nothing of this exists yet)*

Channels are numbered 1..8 in BASIC (like `#ch`), 0..7 in the host. Everything is queued and returns at once, as the host API does.

```
TEMPO bpm                         tempo for all channels, default 60 (1 beat = 1 s)
WAVE ch, SINE|SQUARE|TRIANGLE|NOISE   (or WAVE ch, 0..3) wave type from the next sound on
SOUND ch, freq, beats [, vol]     tone of freq Hz (20..20000) for "beats", vol 0..1 (default 0.5)
                                  beats = 0: no end, until the next SOUND on that channel or SOUND OFF
SOUND ch, 0, beats                a rest of "beats"
SOUND OFF [ch]                    silence one channel / all of them now, forget what is queued
SOUND SYNC ch [, ch ...]          line up the channels' time (start them together)
```

`PLAY` takes a music string per voice (voice n = channel n); all strings start together:

```
PLAY a$ [, b$ [, c$ ...]]

  A-G           note (# or + sharp, - flat), optionally followed by a length (C4 = quarter note)
  R or P        rest
  O n           octave 0..8 (O4 starts at middle C), < and > one octave down / up
  L n           default note length: 1 = whole, 2 = half, 4 = quarter, 8 ... ; a dot after a note lengthens it by half
  T n           tempo in quarter notes per minute
  V n           volume 0..15
  W n           wave 0..3 (sine, square, triangle, noise), later: I n = instrument
  [CEG]4        chord: the notes sound together for the length that follows (uses further channels)
  ML / MN / MS  legato / normal / staccato (note length 100 % / 85 % / 60 %: the rest is silence)
```

Example: `PLAY "T120 O4 L8 CDEF G4 G4", "T120 O2 L2 C G"` — melody and bass line. Notes are mapped to frequencies
(A4 = 440 Hz), so the whole octave range of 20 Hz..20 kHz is usable, not just notes 0..71. A `PLAY` string is parsed
completely before anything is queued (a syntax error queues nothing: "Illegal argument").

Open questions: whether `PLAY` should block until the music is over (QBasic's default) or return at once (suggested, with a
function such as `PLAYING( ch )` to ask whether a channel still has queued events — needs a small `sdlaud_busy()` in the host
API); whether `SOUND` and `PLAY` on the same channel queue after each other (suggested: yes, both use the channel's cursor);
volume scale 0..15 (CPC style) or 0..1.

## Instruments (planned)

The goal is a software FM synthesizer so games can have proper music and sound effects. The structure is prepared for it:
`synth_osc_t` (one oscillator: wave, phase, step) is separate from `synth_chan_t` (gain, fade), so an instrument can later
be a set of oscillators per channel with operator routing (modulation = one oscillator's output added to another's phase),
a frequency ratio and level per operator, and an ADSR envelope per operator or voice instead of the fixed 2.5 ms slew.
Instruments would be defined by a statement (`INSTRUMENT n, ...`) or loaded from data, selected by `PLAY`'s `I n` and by
`sdlaud_setinstrument()`; the basic wave types stay as instrument 0..3. Further steps: a polyphonic voice allocator (more
simultaneous notes than channels), band-limited square/triangle, per-channel panning once the output is stereo.
