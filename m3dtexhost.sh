#!/bin/sh
# Test of M3D TEXTURE LOAD in the real window host (engine1999 under Xvfb): a typed-in program draws a 16 x 16 pattern, makes picture 1 with GFX GRAB
# and writes the pattern with GFX SAVE; picture 2 is the same file read by M3D TEXTURE LOAD (colors matched to the palette), picture 3 the file read
# with PALETTE. The same wall is drawn with picture 1 and with picture 2 (SCREENSHOT "grab", "load"): the two screenshots must be identical byte for
# byte; a third with picture 3 only has to differ from a flat picture (it has the file's own palette). Needs Xvfb, the X11 / Xtst development files and
# ./engine1999; if one is missing the test is SKIPPED (exit status 0). Not part of "make check": run it with "sh m3dtexhost.sh".
LC_ALL=C; export LC_ALL
cd "$(dirname "$0")" || exit 1
skip() { echo "m3dtexhost: skipped ($1)"; exit 0; }
command -v Xvfb > /dev/null 2>&1 || skip "no Xvfb"
[ -x ./engine1999 ] || skip "no ./engine1999: run make first"
T=$(mktemp -d) || exit 1
ENGPID=""; XPID=""; WMPID=""
cleanup() {
    [ -n "$ENGPID" ] && pkill -9 -P "$ENGPID" > /dev/null 2>&1
    for p in $ENGPID $WMPID $XPID; do [ -n "$p" ] && kill -9 "$p" > /dev/null 2>&1; done
    [ -n "$HOSTTEST_KEEP" ] && echo "m3dtexhost: files kept in $T" || rm -rf "$T"
}
trap cleanup EXIT INT TERM
echo '#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
int main(void) { return 0; }' > "$T/probe.c"
cc -o "$T/probe" "$T/probe.c" -lX11 -lXtst > /dev/null 2>&1 || skip "no X11 / Xtst development files"
cc -Wall -o "$T/xtype" xtype.c -lX11 -lXtst || { echo "FAILED: m3dtexhost (xtype.c does not compile)"; exit 1; }
ENG=$PWD/engine1999
N=90
while [ -e "/tmp/.X11-unix/X$N" ] && [ $N -lt 120 ]; do N=$((N+1)); done
Xvfb ":$N" -screen 0 800x600x24 > /dev/null 2>&1 &
XPID=$!
i=0
while [ ! -e "/tmp/.X11-unix/X$N" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
[ -e "/tmp/.X11-unix/X$N" ] || skip "Xvfb did not start"
if command -v metacity > /dev/null 2>&1; then DISPLAY=":$N" metacity > /dev/null 2>&1 & WMPID=$!; sleep 1; fi
cat > "$T/m3dtexh.bas" <<'EOP'
5 GFX LAYER 1,OFF:GFX LAYER 2,OFF
10 GFX CLEAR 0
20 FOR I=0 TO 15
30 GFX LINE 0,I,15,I,16+I*3
40 GFX LINE I,0,I,15,126-I*3
50 NEXT I
60 GFX PSET 3,4,0
70 GFX GRAB 0,0,15,15,A$
80 M3D TEXTURE 1,A$
90 GFX SAVE "texpat",0,0,15,15
100 M3D TEXTURE LOAD 2,"texpat"
110 M3D TEXTURE LOAD 3,"texpat.bmp",PALETTE
120 IF M3D("TEXW",2)<>16 OR M3D("TEXH",2)<>16 OR M3D("TEXW",3)<>16 THEN SCREENSHOT "badsize":END
130 GFX CLEAR 0:M3D COLORMAP
140 M3D OBJECT 1:M3D BOX 1,-500,-500,-10,500,500,0,1
150 M3D OBJECT 2:M3D BOX 2,-300,200,0,300,220,200,1
160 M3D SKY 60
170 M3D CAMERA 0,0,60,90,0
180 M3D RENDER
190 SCREENSHOT "grab"
200 M3D OBJECT 2:M3D BOX 2,-300,200,0,300,220,200,2
210 M3D OBJECT 1:M3D BOX 1,-500,-500,-10,500,500,0,2
220 M3D RENDER
230 SCREENSHOT "load"
240 M3D OBJECT 2:M3D BOX 2,-300,200,0,300,220,200,3
250 M3D RENDER
260 SCREENSHOT "pal"
270 END
EOP
cd "$T" || exit 1
DISPLAY=":$N" SDL_AUDIODRIVER=dummy timeout 60 "$ENG" --allow=all > "$T/win.out" 2>&1 &
ENGPID=$!
sleep 3
DISPLAY=":$N" "$T/xtype" 'LOAD "m3dtexh"
' || { echo "FAILED: m3dtexhost (typing failed)"; exit 1; }
sleep 1
DISPLAY=":$N" "$T/xtype" 'RUN
' || { echo "FAILED: m3dtexhost (typing failed)"; exit 1; }
i=0
while [ ! -s "$T/pal.bmp" ] && [ ! -s "$T/badsize.bmp" ] && [ $i -lt 200 ]; do sleep 0.2; i=$((i+1)); done
sleep 1
[ -s "$T/badsize.bmp" ] && { echo "FAILED: m3dtexhost (the loaded picture has the wrong size)"; exit 1; }
for f in grab load pal; do
    [ -s "$T/$f.bmp" ] || { echo "FAILED: m3dtexhost (no $f.bmp; host output: $(head -c 300 "$T/win.out"))"; exit 1; }
done
kinds=$(od -An -v -tu1 -j54 "$T/load.bmp" | tr -s ' ' '\n' | sort -u | wc -l)
same=$(cmp -l "$T/grab.bmp" "$T/load.bmp" | wc -l)
echo "m3dtexhost: $same bytes differ between the grabbed and the loaded texture"
[ "$kinds" -gt 8 ] || { echo "FAILED: m3dtexhost (the picture is flat)"; exit 1; }
[ "$same" -eq 0 ] || { echo "FAILED: m3dtexhost (the loaded texture does not look like the grabbed one)"; exit 1; }
echo "m3dtexhost: ok"
