# Controller and joystick support

**Status: implemented 2026-11-01** (roadmap item 4; see "What is implemented" at the end; the language is in `docs/basic-language.md`, "Game controllers"). This page collects what is known about game
controllers on a PC and how the support is tested. Written 2026-11-01, before the implementation; the design points below are marked with what was decided. The statements about drivers and
brands are from general knowledge of the Linux kernel and SDL, **not verified on hardware by this project**; the checks in "Trying a real controller" are there to do exactly that.

## Do the console controllers work on a PC?

Yes. On Linux (Debian 12, kernel 6.1, SDL 2.26 are what the project is built on) all of the common ones are recognized by the kernel and by SDL:

| Controller | USB cable | Bluetooth | Notes |
|---|---|---|---|
| **Xbox Series X\|S** (also Xbox One) | works out of the box (kernel driver `xpad`) | works with BlueZ; the third-party driver `xpadneo` gives the full feature set (rumble, all buttons) | the Xbox wireless dongle needs the third-party driver `xone` |
| **PS4 (DualShock 4)** | works (kernel driver `hid-sony`) | works | the touchpad and the motion sensor appear as separate input devices, which the Engine ignores (the PS5 DualSense: `hid-playstation`) |
| **Nintendo Switch Pro Controller** | works (kernel driver `hid-nintendo`, in kernels since 5.16) | works | **A/B and X/Y sit in swapped positions compared with Xbox** (see below); Joy-Cons show up as two separate devices unless a tool such as `joycond` joins them |
| Generic USB gamepads and joysticks | work (`usbhid` / `joydev`) | | no standard layout: their buttons and axes are numbered, not named |

## What the Engine will build on

**SDL's game controller API** (`SDL_GameController...`), not the raw joystick API alone. It maps every known pad to **one standard layout** (the four face buttons by
position, two shoulder buttons, two triggers, the D-pad, two sticks, Start / Back / Guide, the stick clicks) using SDL's controller database, so that a program sees the same
numbers on an Xbox, a PS4 and a Switch pad. Pads SDL does not know about are still reachable through the plain joystick API (numbered axes and buttons).

Points of the design (decided when it was implemented, see the end):
- **Face button naming.** SDL can name the buttons by *position* (bottom, right, left, top) or by *label* (A, B, X, Y: a Nintendo pad then swaps A and B). Position is the safer
  basis for games; the hint is `SDL_GAMECONTROLLER_USE_BUTTON_LABELS`.
- **The words.** Probably a function `JOY(player, "name")` in the style of `KEY("name")` and `MOUSE("X")` (names for buttons, stick axes -1..1, triggers 0..1, D-pad), `ON JOY GOSUB` like
  `ON MOUSE`, and a way to ask how many controllers are connected (`JOY("COUNT")`). New words go in as extension words (see `docs/basic-language.md`, "MIN, MAX, ATAN2, SPACE$, STRING$").
- **Dead zone and drift**: sticks never rest at exactly zero; a dead zone (adjustable) belongs in the module, not in every program.
- **Several players, hot plugging**: controller numbers and what happens when one is pulled out or plugged in during a program.
- **Rumble**: not planned for the first version.
- **The console host** has no SDL window and no event loop for devices. Reading `/dev/input/js0` (the kernel's `joydev`) directly would be possible; to be decided, as the mouse
  there already is "screen mode only".
- **Threads**: as for the mouse, the screen thread alone reads the device events and publishes state through atomic words; the program thread reads (no locks; `docs/display-engine.md`, "Threading").

## How it will be tested

No physical controller is needed for most of it, in three layers:

1. **The core without SDL.** A module `joy.[ch]` (like `mouse.[ch]`) holds button and axis state, dead zones, latched presses and the event counter. A test program `joytest1`
   feeds it scripted events and checks the values: buttons, axis scaling, dead zone, latching, several players, a controller that disappears.
2. **The BASIC level** through the existing logging test host (`vmtest1`, `repltest1`): `JOY(...)` reads, `ON JOY`, error cases (a player that does not exist), the instructions in the assembler tests.
3. **The real SDL path without hardware.** SDL 2.26 has **virtual joysticks** (`SDL_JoystickAttachVirtual`, since SDL 2.0.14): a test or scratch program creates a gamepad inside its own process,
   sets its buttons and axes, and the Engine sees it like a real one (SDL's event loop, the game controller mapping, hot plugging and removal). This runs under Xvfb the way the
   mouse and keyboard checks do (an injector in the session's scratch area, not part of the repository). `/dev/uinput` (a virtual device made through the kernel) also exists on
   this machine, but needs permissions, so it is not relied on.

What cannot be tested without a real pad: the **feel** (dead zones, drift, trigger ranges), the actual mapping of a given pad in SDL's database, Bluetooth behaviour, and the
Nintendo A/B swap. That part is for whoever owns a controller; a wired Xbox or PS4 pad is the safest first one.

## Trying a real controller

```
ls /dev/input/js*                      # the kernel made a joystick device
sudo apt install joystick evtest sdl2-jstest
jstest /dev/input/js0                  # raw axes and buttons
sdl2-jstest --list                     # what SDL sees, and whether it has a mapping for it
```

If the pad shows up in `sdl2-jstest --list` with a mapping, the Engine will see it with the standard layout. A pad without a mapping can be given one with the
environment variable `SDL_GAMECONTROLLERCONFIG` (SDL's mapping string format; the SDL Game Controller DB is the usual source); a mapping that works should be reported and added to the Engine.

## What is implemented

- **`joy.[ch]`** (no SDL): four slots, atomic words, latched `PRESSED` / `RELEASED` bits, an event counter, the dead zone (default 0.2, `JOY DEADZONE`), names. **`sdljoy.[ch]`**: SDL's game controller API with the plain joystick API as the fallback
  (axes 0, 1 = left stick, 2, 3 = right stick, buttons 0 .. 9, the first hat = the D-pad); the events are given to it by the screen thread's loop (`sdlscreen.c`). Both pads with and without a mapping, hot plugging and removal work.
- **BASIC**: `JOY(name$ [, player])`, `JOY DEADZONE d`, `ON JOY GOSUB|GOTO|OFF` (one new extension word, `JOY`); a player without a pad reads 0 (no error), the console host reads 0 everywhere (`JOY("COUNT")` is 0). Details: `docs/basic-language.md`, "Game controllers".
- **Decisions I took** (revisit if unwanted): face buttons **by position** (SDL's hint `SDL_GAMECONTROLLER_USE_BUTTON_LABELS` is set to 0 before `SDL_Init`, so a Nintendo pad does not swap A and B); up is negative on the sticks; the sticks have **no radial dead zone** (each axis on its own);
  the dead zone is global, not per pad; a pad keeps its player number until it is unplugged and a new pad takes the lowest free number; four pads at most; `ON JOY` fires on button presses and releases and on plugging in or out, not on stick motion; `JOY("PRESSED")` and `("RELEASED")` are latched per player like the mouse's.
- **Quirks found while testing**: SDL **delays the release of the Guide button by 250 ms** (so that a quick tap is seen); the trigger axes of a **virtual** pad are rescaled by SDL (a raw 16384 reads 0.75); SDL turns Ctrl-C and SIGTERM into `SDL_QUIT` events (a program that polls events must look for it).
- **Tested**: `joytest1` (the state, 43 checks), `vmtest1` (the BASIC level with injected state: 36 cases, `ON JOY` through control codes in the test host's `INKEY$`), `sdltest4 --selftest` (SDL events with virtual pads: a pad with a mapping, a plain joystick, all 15 buttons, sticks,
  triggers, the hat, unplugging, taking a free slot again: 20 checks), `asmtest1`, `repltest1`. **A real pad**: an **Xbox Series X|S controller on USB** was plugged into the development machine during the work (the kernel's `xpad` driver, SDL: "Xbox Series X Controller"); it is found as player 1 by `sdltest4` and by `demos/joy.bas` in the real window,
  and the self test runs with it plugged in. What was **not** tried by me: pressing its buttons and moving its sticks (nobody was at the pad), PlayStation and Switch pads, Bluetooth, hot plugging of a real pad while a program runs.
- **Not done**: the pad's name as a string, rumble, motion sensors, touchpads, joysticks in the console host (reading `/dev/input/js*` would do it), a configuration of the mapping from inside the Engine, an analog "radial" dead zone.
