#!/bin/sh
# Test of the window host's sound path (engine1999, SDL), without a screen or a sound card: an X server without a display (Xvfb), a program typed in
# with the XTest extension (xtype.c), SDL's "disk" audio driver (the sound goes to a file) and the sound log (ENGINE1999_SOUNDLOG).
# The program defines instrument 20 as a sine, plays a note, redefines the instrument as a square wave and plays the note again
# (the sound log shows three selections of instrument 20 on channel 1: the first I20, the snapshot that the redefinition queues for the channel that uses it, and the second I20): the recorded audio must be a
# pure sine first (no 3rd harmonic) and a square wave after it (3rd harmonic = 1/3 of the fundamental). That proves the whole chain: INSTRUMENT, the queued
# snapshot per channel, sdlaud_setinst(), the audio thread and the oscillator.
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
50 WAIT 240
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
while [ "$(grep -c 'instrument 20 ' "$T/snd.log" 2> /dev/null)" != 3 ] && [ $i -lt 150 ]; do sleep 0.1; i=$((i+1)); done
sleep 4
n=$(grep -c 'instrument 20 ' "$T/snd.log" 2> /dev/null)
if [ "$n" != 3 ]; then echo "FAILED: hosttest (the sound log has $n selections of instrument 20, expected 3; see the host's output: $(head -c 300 "$T/win.out"))"; exit 1; fi
[ -s "$T/out.raw" ] || { echo "FAILED: hosttest (no audio was recorded)"; exit 1; }
# the recording: 32-bit float, mono, 44100 Hz. Find the start of the sound, then the amplitudes of 220 Hz and 660 Hz in a window of the first note (0.2 .. 0.7 s)
# and of the second one (1.2 .. 1.7 s), with the Goertzel algorithm.
od -An -v -f -w4 "$T/out.raw" | awk '
    { x[NR-1] = $1 + 0 }
    function amp(from, f,    w, c, s0, s1, s2, i, re, im, n) {
        n = 22050; w = 2 * 3.14159265358979 * f / 44100; c = 2 * cos(w); s1 = 0; s2 = 0
        for (i = from; i < from + n; i++) { s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0 }
        re = s1 - s2 * cos(w); im = s2 * sin(w)
        return 2 * sqrt(re * re + im * im) / n
    }
    END {
        st = -1
        for (i = 0; i < NR; i++) if (x[i] > 0.01 || x[i] < -0.01) { st = i; break }
        if (st < 0 || NR < st + 44100 * 2) { print "FAILED: hosttest (the recording holds no two seconds of sound)"; exit 1 }
        f1 = amp(st + 8820, 220); h1 = amp(st + 8820, 660)
        f2 = amp(st + 52920, 220); h2 = amp(st + 52920, 660)
        printf "first note: fundamental %.3f, 3rd harmonic %.3f; second note: fundamental %.3f, 3rd harmonic %.3f\n", f1, h1, f2, h2
        ok = (f1 > 0.3 && h1 < 0.05 * f1 && f2 > 0.4 && h2 > 0.25 * f2 && h2 < 0.42 * f2)
        if (!ok) { print "FAILED: hosttest (expected a sine, then a square wave)"; exit 1 }
        print "hosttest ok"
    }'
exit $?
