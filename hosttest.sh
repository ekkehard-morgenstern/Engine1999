#!/bin/sh
# Test of the window host's sound path (engine1999, SDL), without a screen or a sound card: an X server without a display (Xvfb), a program typed in
# with the XTest extension (xtype.c), SDL's "disk" audio driver (the sound goes to a file) and the sound log (ENGINE1999_SOUNDLOG).
# The program defines instrument 20 as a sine, plays a note, redefines the instrument as a square wave and plays the note again
# (the sound log shows three selections of instrument 20 on channel 1: the first I20, the snapshot that the redefinition queues for the channel that uses it, and the second I20): the recorded audio must be a
# pure sine first (no 3rd harmonic) and a square wave after it (3rd harmonic = 1/3 of the fundamental). That proves the whole chain: INSTRUMENT, the queued
# snapshot per channel, sdlaud_setinst(), the audio thread and the oscillator.
# It goes on with a chord [ACE] on an instrument with POLY 4 (three notes of 0.1 each: the voice allocator) and a saw whose cutoff is opened by K40 (the filter cutoff event).
# Needs: Xvfb, a C compiler with the X11 and Xtst headers, ./engine1999 (make). If one is missing the test is SKIPPED (exit status 0); a failure is exit status 1.
# Not part of "make check" (it needs the X server tools): run it with "make hosttest" or "sh hosttest.sh".
LC_ALL=C; export LC_ALL        # od prints decimal commas in some locales
cd "$(dirname "$0")" || exit 1
skip() { echo "hosttest: skipped ($1)"; exit 0; }
command -v Xvfb > /dev/null 2>&1 || skip "no Xvfb"
[ -x ./engine1999 ] || skip "no ./engine1999: run make first"
T=$(mktemp -d) || exit 1
ENGPID=""; XPID=""; WMPID=""
cleanup() {
    # the engine is a child of "timeout" and ignores SIGTERM: kill the child first, then the wrapper, the window manager and the X server
    [ -n "$ENGPID" ] && pkill -9 -P "$ENGPID" > /dev/null 2>&1
    for p in $ENGPID $WMPID $XPID; do [ -n "$p" ] && kill -9 "$p" > /dev/null 2>&1; done
    [ -n "$HOSTTEST_KEEP" ] && echo "hosttest: files kept in $T" || rm -rf "$T"
}
trap cleanup EXIT INT TERM
echo '#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
int main(void) { return 0; }' > "$T/probe.c"
cc -o "$T/probe" "$T/probe.c" -lX11 -lXtst > /dev/null 2>&1 || skip "no X11 / Xtst development files"
cc -Wall -o "$T/xtype" xtype.c -lX11 -lXtst || { echo "FAILED: hosttest (xtype.c does not compile)"; exit 1; }
ENG=$PWD/engine1999
# a free display number
N=90
while [ -e "/tmp/.X11-unix/X$N" ] && [ $N -lt 120 ]; do N=$((N+1)); done
Xvfb ":$N" -screen 0 800x600x24 > /dev/null 2>&1 &
XPID=$!
i=0
while [ ! -e "/tmp/.X11-unix/X$N" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
[ -e "/tmp/.X11-unix/X$N" ] || skip "Xvfb did not start"
if command -v metacity > /dev/null 2>&1; then DISPLAY=":$N" metacity > /dev/null 2>&1 & WMPID=$!; sleep 1; fi
cat > "$T/insttest.bas" <<'EOP'
10 INSTRUMENT 20,"OSC SINE"
20 PLAY "I20 T240 O3 L1 A"
30 INSTRUMENT 20,"OSC SQUARE"
40 PLAY "I20 T240 O3 L1 A"
45 INSTRUMENT 21,"OSC SINE 0.3; ENV 0.01 0 1 0.3; POLY 4"
46 INSTRUMENT 22,"OSC SAW 0.5; FILTER 1"
47 PLAY "I21 T240 O3 L1 [ACE]"
48 PLAY "I22 T240 O3 L4 K1 A K40 A"
49 PLAY "I22 T240 O3 L4 K40 Y-1 A Y1 A"
50 WAIT 480
60 END
EOP
cd "$T" || exit 1
DISPLAY=":$N" SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE="$T/out.raw" ENGINE1999_SOUNDLOG="$T/snd.log" timeout 60 "$ENG" > "$T/win.out" 2>&1 &
ENGPID=$!
sleep 3
DISPLAY=":$N" "$T/xtype" 'LOAD "insttest"
' || { echo "FAILED: hosttest (typing failed)"; exit 1; }
sleep 1
DISPLAY=":$N" "$T/xtype" 'RUN
' || { echo "FAILED: hosttest (typing failed)"; exit 1; }
# wait for the three selections in the sound log, then for the notes to play (2 s) plus the audio file to be written
i=0
while [ "$(grep -c 'instrument 22 ' "$T/snd.log" 2> /dev/null)" != 1 ] && [ $i -lt 150 ]; do sleep 0.1; i=$((i+1)); done
sleep 6
n=$(grep -c 'instrument 20 ' "$T/snd.log" 2> /dev/null)
if [ "$n" != 3 ]; then echo "FAILED: hosttest (the sound log has $n selections of instrument 20, expected 3; see the host's output: $(head -c 300 "$T/win.out"))"; exit 1; fi
[ -s "$T/out.raw" ] || { echo "FAILED: hosttest (no audio was recorded)"; exit 1; }
# screenshots (roadmap item 82): the statement at the prompt, then the Print key; the program has ended, the prompt is on the screen
shotcheck() {       # file: a 24 bit BMP of 640 x 300 (576054 bytes) whose pixels are not all the same
    [ -s "$1" ] || { echo "FAILED: hosttest (no screenshot file $1)"; exit 1; }
    sz=$(wc -c < "$1")
    [ "$sz" = 576054 ] || { echo "FAILED: hosttest (screenshot $1 has $sz bytes, expected 576054)"; exit 1; }
    [ "$(head -c 2 "$1")" = BM ] || { echo "FAILED: hosttest (screenshot $1 is no BMP)"; exit 1; }
    kinds=$(od -An -v -tu1 -j54 "$1" | tr -s ' ' '\n' | sort -u | wc -l)
    [ "$kinds" -gt 4 ] || { echo "FAILED: hosttest (screenshot $1 is a flat picture)"; exit 1; }
}
sleep 5            # (the program's WAIT is over: the prompt reads the keys)
DISPLAY=":$N" "$T/xtype" 'SCREENSHOT "shota"
' || { echo "FAILED: hosttest (typing failed)"; exit 1; }
sleep 2
shotcheck "$T/shota.bmp"
DISPLAY=":$N" "$T/xtype" "$(printf '\001')" || { echo "FAILED: hosttest (typing failed)"; exit 1; }
sleep 2
set -- "$T"/shot-*.bmp
[ -e "$1" ] || { echo "FAILED: hosttest (the Print key made no shot-*.bmp file)"; exit 1; }
shotcheck "$1"
DISPLAY=":$N" "$T/xtype" "$(printf '\002')" || { echo "FAILED: hosttest (typing failed)"; exit 1; }
sleep 2
n=$(ls "$T"/shot-*.bmp 2> /dev/null | wc -l)
[ "$n" -ge 2 ] || { echo "FAILED: hosttest (Ctrl+Shift+P made no second shot-*.bmp file)"; exit 1; }
echo "screenshots: SCREENSHOT \"shota\" and the keys Print and Ctrl+Shift+P ($(basename "$1")) ok"
# the recording: 32-bit float, stereo (left, right), 44100 Hz. Find the start of the sound, then the amplitudes of 220 Hz and 660 Hz in a window of the first note (0.2 .. 0.7 s)
# and of the second one (1.2 .. 1.7 s), with the Goertzel algorithm.
od -An -v -f -w8 "$T/out.raw" | awk '
    { x[NR-1] = $1 + 0; y[NR-1] = $2 + 0 }
    function amp(from, f, n,    w, c, s0, s1, s2, i, re, im) {
        return amps(x, from, f, n)
    }
    function amps(x, from, f, n,    w, c, s0, s1, s2, i, re, im) {
        if (n == 0) n = 22050; w = 2 * 3.14159265358979 * f / 44100; c = 2 * cos(w); s1 = 0; s2 = 0
        for (i = from; i < from + n; i++) { s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0 }
        re = s1 - s2 * cos(w); im = s2 * sin(w)
        return 2 * sqrt(re * re + im * im) / n
    }
    END {
        st = -1
        for (i = 0; i < NR; i++) if (x[i] > 0.01 || x[i] < -0.01) { st = i; break }
        if (st < 0 || NR < st + 44100 * 4.2) { print "FAILED: hosttest (the recording holds no four seconds of sound)"; exit 1 }
        f1 = amp(st + 8820, 220); h1 = amp(st + 8820, 660)
        f2 = amp(st + 52920, 220); h2 = amp(st + 52920, 660)
        printf "first note: fundamental %.3f, 3rd harmonic %.3f; second note: fundamental %.3f, 3rd harmonic %.3f\n", f1, h1, f2, h2
        ok = (f1 > 0.3 && h1 < 0.05 * f1 && f2 > 0.4 && h2 > 0.25 * f2 && h2 < 0.42 * f2)
        if (!ok) { print "FAILED: hosttest (expected a sine, then a square wave)"; exit 1 }
        # the chord [ACE] on a POLY instrument (2.1 .. 2.9 s after the start): all three notes at once (A3 220, C3 130.81, E3 164.81 Hz: 0.3 x 0.5 / 1.5 = 0.1 each)
        ca = amp(st + 92610, 220, 35000); cc = amp(st + 92610, 130.81, 35000); ce = amp(st + 92610, 164.81, 35000)
        printf "chord: A %.3f, C %.3f, E %.3f\n", ca, cc, ce
        if (!(ca > 0.07 && cc > 0.07 && ce > 0.07 && ca < 0.14)) { print "FAILED: hosttest (expected a chord of three notes)"; exit 1 }
        # the stereo position: "K1 A K40 A K40 Y-1 A Y1 A" (quarter notes after the chord): the third note sounds only in the left channel (the right one is exactly 0),
        # the fourth only in the right one; p = the first sample of the third note, found in the recording (the start of the sound varies a little with the device)
        p = -1
        for (i = st + 2 * 44100; i < NR; i++) if (x[i] > 0.05 && y[i] == 0) { p = i; break }
        if (p < 0) { print "FAILED: hosttest (no sound only in the left channel)"; exit 1 }
        l1 = amps(x, p + 2205, 220, 7000); r1 = amps(y, p + 2205, 220, 7000)
        l2 = amps(x, p + 13230, 220, 7000); r2 = amps(y, p + 13230, 220, 7000)
        printf "pan: first note left %.3f right %.4f; second note left %.4f right %.3f\n", l1, r1, l2, r2
        if (!(l1 > 0.05 && r1 < 0.001 && r2 > 0.05 && l2 < 0.001)) { print "FAILED: hosttest (expected the notes at the left and at the right)"; exit 1 }
        # the cutoff: a saw with its filter at 1 x the note, then K40 (opened): the 8th harmonic of 220 Hz (1760) is absent in the first quarter note, strong in the second
        d1 = amps(x, p - 19845, 1760, 7000); b1 = amps(x, p - 8820, 1760, 7000)
        printf "cutoff: 8th harmonic %.4f before K40, %.4f after\n", d1, b1
        if (!(d1 < 0.005 && b1 > 0.01 && b1 > 5 * d1)) { print "FAILED: hosttest (expected the cutoff to open)"; exit 1 }
        print "hosttest ok"
    }'
exit $?
