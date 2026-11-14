#!/bin/sh
# Test of the hardware 3D renderer in the real window host (engine1999 under Xvfb): a program typed in with the XTest extension (xtype.c) builds a
# small scene, draws it with the software renderer (SCREENSHOT "soft"), switches with M3D HARDWARE and draws it again (SCREENSHOT "hard"). With a
# sky color and light at full brightness, both renderers give the exact palette colors, so the two screenshots must be almost the same (they may
# differ along the edges of the surfaces). It proves the whole chain: the probe in the screen thread, M3D HARDWARE, the context on the program thread,
# the 32 bit layer, its place in the composed picture. Needs Xvfb (OpenGL by Mesa), the X11 / Xtst development files and ./engine1999; if one is
# missing the test is SKIPPED (exit status 0). Not part of "make check": run it with "make hosttest" or "sh m3dglhost.sh".
LC_ALL=C; export LC_ALL
cd "$(dirname "$0")" || exit 1
skip() { echo "m3dglhost: skipped ($1)"; exit 0; }
command -v Xvfb > /dev/null 2>&1 || skip "no Xvfb"
[ -x ./engine1999 ] || skip "no ./engine1999: run make first"
T=$(mktemp -d) || exit 1
ENGPID=""; XPID=""; WMPID=""
cleanup() {
    [ -n "$ENGPID" ] && pkill -9 -P "$ENGPID" > /dev/null 2>&1
    for p in $ENGPID $WMPID $XPID; do [ -n "$p" ] && kill -9 "$p" > /dev/null 2>&1; done
    [ -n "$HOSTTEST_KEEP" ] && echo "m3dglhost: files kept in $T" || rm -rf "$T"
}
trap cleanup EXIT INT TERM
echo '#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
int main(void) { return 0; }' > "$T/probe.c"
cc -o "$T/probe" "$T/probe.c" -lX11 -lXtst > /dev/null 2>&1 || skip "no X11 / Xtst development files"
cc -Wall -o "$T/xtype" xtype.c -lX11 -lXtst || { echo "FAILED: m3dglhost (xtype.c does not compile)"; exit 1; }
ENG=$PWD/engine1999
N=90
while [ -e "/tmp/.X11-unix/X$N" ] && [ $N -lt 120 ]; do N=$((N+1)); done
Xvfb ":$N" -screen 0 800x600x24 > /dev/null 2>&1 &
XPID=$!
i=0
while [ ! -e "/tmp/.X11-unix/X$N" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
[ -e "/tmp/.X11-unix/X$N" ] || skip "Xvfb did not start"
if command -v metacity > /dev/null 2>&1; then DISPLAY=":$N" metacity > /dev/null 2>&1 & WMPID=$!; sleep 1; fi
cat > "$T/m3dglh.bas" <<'EOP'
10 R$=STRING$(4,"x")+STRING$(4,"P")
20 A$=MKI$(8)+MKI$(8)
30 FOR I=1 TO 8:A$=A$+R$:NEXT I
40 M3D TEXTURE 2,A$
50 B$=MKI$(8)+MKI$(8)+STRING$(64,"P")
60 M3D TEXTURE 1,B$
70 M3D OBJECT 1:M3D BOX 1,-500,-500,-10,500,500,0,1
80 M3D OBJECT 2:M3D BOX 2,-100,200,0,100,220,120,2
85 M3D SKY 60
90 M3D CAMERA 0,0,0,90,0
100 M3D RENDER
110 SCREENSHOT "soft"
120 IF M3D("HWAVAIL")=0 THEN SCREENSHOT "nohw":END
130 M3D HARDWARE
140 M3D RENDER
150 IF M3D("RENDERER")=1 THEN SCREENSHOT "hard"
160 M3D SOFTWARE
170 M3D RENDER
180 SCREENSHOT "soft2"
190 END
EOP
cd "$T" || exit 1
DISPLAY=":$N" SDL_AUDIODRIVER=dummy timeout 60 "$ENG" --allow=all > "$T/win.out" 2>&1 &
ENGPID=$!
sleep 3
DISPLAY=":$N" "$T/xtype" 'LOAD "m3dglh"
' || { echo "FAILED: m3dglhost (typing failed)"; exit 1; }
sleep 1
DISPLAY=":$N" "$T/xtype" 'RUN
' || { echo "FAILED: m3dglhost (typing failed)"; exit 1; }
i=0
while [ ! -s "$T/soft2.bmp" ] && [ ! -s "$T/nohw.bmp" ] && [ $i -lt 200 ]; do sleep 0.2; i=$((i+1)); done
sleep 1
[ -s "$T/nohw.bmp" ] && skip "no OpenGL 2 in the window host (M3D(\"HWAVAIL\") = 0)"
for f in soft hard soft2; do
    [ -s "$T/$f.bmp" ] || { echo "FAILED: m3dglhost (no $f.bmp; host output: $(head -c 300 "$T/win.out"))"; exit 1; }
done
bad=$(cmp -l "$T/soft.bmp" "$T/hard.bmp" | wc -l)
tot=$(wc -c < "$T/soft.bmp")
kinds=$(od -An -v -tu1 -j54 "$T/hard.bmp" | tr -s ' ' '\n' | sort -u | wc -l)
back=$(cmp -l "$T/soft.bmp" "$T/soft2.bmp" | wc -l)
echo "m3dglhost: $bad of $tot bytes differ between the software and the hardware picture; $back between the software pictures before and after"
[ "$kinds" -gt 4 ] || { echo "FAILED: m3dglhost (the hardware picture is flat)"; exit 1; }
[ "$bad" -lt $((tot / 25)) ] || { echo "FAILED: m3dglhost (the pictures differ too much)"; exit 1; }
[ "$back" -lt $((tot / 100)) ] || { echo "FAILED: m3dglhost (back on the software renderer the picture is not the same)"; exit 1; }
echo "m3dglhost: ok"
