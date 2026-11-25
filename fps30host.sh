#!/bin/sh
# Test of the 30 frames a second mode of the 3D programs (demos/lib/w3dkit.lib, W3K_SETFPS; the setting is in prefs3d) in the real window host (engine1999 under
# Xvfb): a program typed in with the XTest extension (xtype.c) shows 40 pictures with W3K_SHOW at 60 and at 30 frames a second and writes how many screen refreshes
# (GFX("FRAME")) passed between two pictures. At 30 frames a second it must be at least 2 every time (a picture is never shown one refresh after the last), at 60 it
# may be 1; the setting is saved to and read back from the settings file of prefs3d (and an older file without it gives 60); the mean time of the 30 mode must not be shorter than that of the 60 mode. Needs Xvfb, the X11 / Xtst development files and ./engine1999; if one is missing
# the test is SKIPPED (exit status 0). Not part of "make check": run it with "make hosttest" or "sh fps30host.sh".
LC_ALL=C; export LC_ALL
cd "$(dirname "$0")" || exit 1
skip() { echo "fps30host: skipped ($1)"; exit 0; }
command -v Xvfb > /dev/null 2>&1 || skip "no Xvfb"
[ -x ./engine1999 ] || skip "no ./engine1999: run make first"
T=$(mktemp -d) || exit 1
ENGPID=""; XPID=""; WMPID=""
cleanup() {
    [ -n "$ENGPID" ] && pkill -9 -P "$ENGPID" > /dev/null 2>&1
    for p in $ENGPID $WMPID $XPID; do [ -n "$p" ] && kill -9 "$p" > /dev/null 2>&1; done
    [ -n "$HOSTTEST_KEEP" ] && echo "fps30host: files kept in $T" || rm -rf "$T"
}
trap cleanup EXIT INT TERM
echo '#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
int main(void) { return 0; }' > "$T/probe.c"
cc -o "$T/probe" "$T/probe.c" -lX11 -lXtst > /dev/null 2>&1 || skip "no X11 / Xtst development files"
cc -Wall -o "$T/xtype" xtype.c -lX11 -lXtst || { echo "FAILED: fps30host (xtype.c does not compile)"; exit 1; }
ENG=$PWD/engine1999
ln -s "$PWD/demos/lib" "$T/lib"
N=90
while [ -e "/tmp/.X11-unix/X$N" ] && [ $N -lt 120 ]; do N=$((N+1)); done
Xvfb ":$N" -screen 0 800x600x24 > /dev/null 2>&1 &
XPID=$!
i=0
while [ ! -e "/tmp/.X11-unix/X$N" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
[ -e "/tmp/.X11-unix/X$N" ] || skip "Xvfb did not start"
if command -v metacity > /dev/null 2>&1; then DISPLAY=":$N" metacity > /dev/null 2>&1 & WMPID=$!; sleep 1; fi
cat > "$T/f30.bas" <<'EOP'
5 ON ERROR GOTO 300
10 LOAD LIBRARY "w3dkit"
15 LOAD LIBRARY "music3d"
17 LOAD LIBRARY "prefs3d"
20 DIM D(2,41)
30 CALL W3K_SETUP
40 FOR M=0 TO 1
50   CALL W3K_SETFPS(60-30*M)
60   CALL W3K_SHOW:P=GFX("FRAME"):T0=TIMER
70   FOR I=1 TO 40
80     CALL W3K_SHOW:Q=GFX("FRAME"):D(M,I)=Q-P:P=Q
90   NEXT I
100  D(M,0)=TIMER-T0
110 NEXT M
120 OPEN "res.txt" FOR OUTPUT AS #1
130 FOR M=0 TO 1
140   MN=99:SM=0
150   FOR I=1 TO 40
160     IF D(M,I)<MN THEN MN=D(M,I)
170     SM=SM+D(M,I)
180   NEXT I
190   PRINT #1,MN;" ";SM;" ";INT(D(M,0)*1000)
200 NEXT M
210 CLOSE #1
220 CALL P3_LOAD:P3_FPS=30:CALL P3_SAVE:P3_FPS=60:CALL P3_LOAD:A=P3_FPS
230 OPEN "local:3dprefs.cfg" FOR OUTPUT AS #1:PRINT #1,"3DPREFS 1"
240 PRINT #1,0:PRINT #1,0:PRINT #1,1:PRINT #1,90:PRINT #1,100:PRINT #1,0:PRINT #1,2:PRINT #1,0
250 CLOSE #1:CALL P3_LOAD:B=P3_FPS
260 OPEN "res2.txt" FOR OUTPUT AS #1:PRINT #1,A;" ";B:CLOSE #1
270 END
300 OPEN "res.txt" FOR OUTPUT AS #1:PRINT #1,"ERR ";ERR$;" in ";ERL:CLOSE #1:END
EOP
mkdir "$T/home"
cd "$T" || exit 1
HOME="$T/home" DISPLAY=":$N" SDL_AUDIODRIVER=dummy timeout 90 "$ENG" --allow=all > "$T/win.out" 2>&1 &
ENGPID=$!
sleep 3
DISPLAY=":$N" "$T/xtype" 'LOAD "f30"
' || { echo "FAILED: fps30host (typing failed)"; exit 1; }
sleep 1
DISPLAY=":$N" "$T/xtype" 'RUN
' || { echo "FAILED: fps30host (typing failed)"; exit 1; }
i=0
while [ ! -s "$T/res.txt" ] && [ $i -lt 400 ]; do sleep 0.2; i=$((i+1)); done
sleep 1
grep -q ERR "$T/res.txt" && { echo "FAILED: fps30host (program error: $(cat "$T/res.txt"))"; exit 1; }
[ -s "$T/res.txt" ] || { echo "FAILED: fps30host (no result; host output: $(head -c 300 "$T/win.out"))"; exit 1; }
set -- $(sed -n 1p "$T/res.txt"); MIN60=$1; SUM60=$2; MS60=$3
set -- $(sed -n 2p "$T/res.txt"); MIN30=$1; SUM30=$2; MS30=$3
echo "fps30host: 60 mode: fewest refreshes between pictures $MIN60, in all $SUM60, $MS60 ms; 30 mode: fewest $MIN30, in all $SUM30, $MS30 ms"
[ "$MIN30" -ge 2 ] || { echo "FAILED: fps30host (a picture came one refresh after the last at 30 frames a second)"; exit 1; }
[ "$SUM30" -ge "$SUM60" ] || { echo "FAILED: fps30host (the 30 mode took fewer refreshes than the 60 mode)"; exit 1; }
i=0
while [ ! -s "$T/res2.txt" ] && [ $i -lt 100 ]; do sleep 0.2; i=$((i+1)); done
set -- $(cat "$T/res2.txt" 2> /dev/null)
echo "fps30host: the setting saved as 30 reads back as ${1:-?}; a settings file of the old format (eight values) reads as ${2:-?}"
[ "$1" = 30 ] || { echo "FAILED: fps30host (the saved 30 frames a second setting is not read back)"; exit 1; }
[ "$2" = 60 ] || { echo "FAILED: fps30host (an old settings file does not give the default 60)"; exit 1; }
echo "fps30host: ok"
