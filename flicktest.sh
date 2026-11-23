#!/bin/sh
# Test of the window host's layer flip (engine1999, SDL): a program draws a frame number as ten bars into one of two graphics layers and flips between them as
# demos/lib/w3dkit.lib does (W3K_SHOW: one layer on, the other off, FRAME, GFX TARGET); the picture of the screen is recorded (ffmpeg, x11grab) and the numbers are
# read from the recorded frames: a number that is smaller than the one before it is a stale picture (the other view of the double buffer), the glitch of the owner's
# report of 2026-11-23. The screen thread used to read the on / off flags of the layers twice in a frame (once to convert the layer's texture, once to draw), so a layer
# that the program switched on in between was drawn with a texture of two frames ago. The flags are now read once a frame (sdllay_snapshot).
# The race is rare, so the test makes it frequent: E1999_SCREENDELAY=25 (a pause of 25 ms between the conversion and the drawing) and a program that wastes random
# amounts of time between the flips. Run 1 (E1999_NOSNAP=1 reads the flags again after the pause, as the old code did) must show stale pictures (otherwise the test proves nothing:
# it is reported as "not sensitive", not as a failure), run 2 (the real code) none.
# Needs: Xvfb, ffmpeg, perl, a C compiler with the X11 and Xtst headers, ./engine1999 (make). If one is missing the test is SKIPPED (exit status 0); a failure is exit status 1.
# Not part of "make check": run it with "make flicktest" or "sh flicktest.sh" (about 40 s).
LC_ALL=C; export LC_ALL
cd "$(dirname "$0")" || exit 1
skip() { echo "flicktest: skipped ($1)"; exit 0; }
command -v Xvfb > /dev/null 2>&1 || skip "no Xvfb"
command -v ffmpeg > /dev/null 2>&1 || skip "no ffmpeg"
command -v perl > /dev/null 2>&1 || skip "no perl"
[ -x ./engine1999 ] || skip "no ./engine1999: run make first"
T=$(mktemp -d) || exit 1
ENGPID=""; XPID=""; WMPID=""
cleanup() {
    [ -n "$ENGPID" ] && pkill -9 -P "$ENGPID" > /dev/null 2>&1
    for p in $ENGPID $WMPID $XPID; do [ -n "$p" ] && kill -9 "$p" > /dev/null 2>&1; done
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
cc -Wall -o "$T/xtype" xtype.c -lX11 -lXtst > /dev/null 2>&1 || skip "no X11 / Xtst development files"
ENG=$PWD/engine1999
cat > "$T/flip.bas" <<'EOP'
10 GFX LAYER 0,OFF:GFX LAYER 3,OFF:GFX LAYER 1,OFF:GFX LAYER 2,OFF
20 GFX BACKDROP RGB(0,0,0):B=0:GFX TARGET 3
40 FOR N=1 TO 1200
50   GFX CLEAR 0:FOR J=1 TO INT(RND(1)*60000):NEXT J
60   FOR K=0 TO 9
70     IF (N AND 2^K)<>0 THEN GFX LINE 20+K*40,100,20+K*40+30,160,15,BF
80   NEXT K
120   GFX LAYER 3-B,ON:GFX LAYER B,OFF:B=3-B
140   FRAME
150   GFX TARGET 3-B
160 NEXT N
170 BYE
EOP
# the numbers of the recorded frames (raw gray 320 x 150: the bars of bit k are 18 pixels wide from x = 50 + 25 k)
cat > "$T/dec.pl" <<'EOP'
open( my $f, "<:raw", $ARGV[0] ) or die; local $/; my $d = <$f>;
my ( $W, $H ) = ( 320, 150 ); my $n = int( length( $d ) / ( $W * $H ) );
my @seq; my $back = 0;
for my $i ( 0 .. $n - 1 ) {
    my $v = 0;
    for my $k ( 0 .. 9 ) { $v |= 1 << $k if ord( substr( $d, $i * $W * $H + 90 * $W + 50 + 25 * $k + 9, 1 ) ) > 100; }
    next if $v == 0 || $v == 1023;                    # (nothing drawn yet / the text screen)
    ++$back if @seq && $v < $seq[-1];
    push @seq, $v;
}
print scalar( @seq ), " $back\n";
EOP
N=90
while [ -e "/tmp/.X11-unix/X$N" ] && [ $N -lt 120 ]; do N=$((N+1)); done
Xvfb ":$N" -screen 0 800x600x24 > /dev/null 2>&1 &
XPID=$!
i=0
while [ ! -e "/tmp/.X11-unix/X$N" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
[ -e "/tmp/.X11-unix/X$N" ] || skip "Xvfb did not start"
if command -v metacity > /dev/null 2>&1; then DISPLAY=":$N" metacity > /dev/null 2>&1 & WMPID=$!; sleep 1; fi
run() {      # run NOSNAP: prints "frames stale"
    ( [ "$1" = 1 ] && export E1999_NOSNAP=1; E1999_SCREENDELAY=25 HOME="$T" DISPLAY=":$N" SDL_AUDIODRIVER=dummy timeout 60 "$ENG" --allow=all > "$T/win.out" 2>&1 ) &
    ENGPID=$!
    sleep 4
    DISPLAY=":$N" "$T/xtype" 'LOAD "'"$T"'/flip"
'
    sleep 1
    DISPLAY=":$N" ffmpeg -loglevel error -y -f x11grab -framerate 60 -video_size 800x600 -i ":$N" -t 12 -vf "crop=640:300:80:100,scale=320:150,format=gray" -f rawvideo "$T/rec.raw" &
    FP=$!
    sleep 1
    DISPLAY=":$N" "$T/xtype" 'RUN
'
    wait $FP
    pkill -9 -P "$ENGPID" > /dev/null 2>&1; kill -9 "$ENGPID" > /dev/null 2>&1; ENGPID=""
    sleep 1
    perl "$T/dec.pl" "$T/rec.raw"
}
fail=0
set -- $(run 1); echo "flicktest: old behaviour (flags read twice): $1 pictures, $2 stale"
if [ "$2" -lt 3 ]; then echo "flicktest: not sensitive (the old behaviour showed fewer than 3 stale pictures in this run)"; fi
set -- $(run 0); echo "flicktest: the snapshot of the flags: $1 pictures, $2 stale"
if [ "$1" -lt 20 ]; then echo "FAILED: flicktest (only $1 pictures were recorded)"; fail=$((fail+1)); fi
if [ "$2" -ne 0 ]; then echo "FAILED: flicktest ($2 stale pictures)"; fail=$((fail+1)); fi
[ $fail -eq 0 ] && echo "flicktest: ok"
exit $fail
