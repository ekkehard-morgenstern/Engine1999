# Controller and joystick support

**Status: planned, not implemented** (roadmap item 4; the mouse part of that item is done, the joystick is next). This page collects what is known about game
controllers on a PC and how the support will be tested, so that it does not have to be worked out again. Written 2026-11-01. The statements about drivers and
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

Points to settle in the design (nothing is decided):
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
