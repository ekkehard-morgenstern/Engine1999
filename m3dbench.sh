#!/bin/sh
# Speed of the two M3D renderers on this machine, with the world of Deqq's House II (640 x 264, 120 frames turning in place at four places,
# software renderer and, when OpenGL is available, the hardware renderer). Run it in a graphical session: it opens the Engine1999 window;
# type the two lines it tells you to (LOAD, RUN); the program draws, writes the times and ends by itself. Usage: sh m3dbench.sh
# ENGINE1999_NOGL=1 would skip the hardware renderer. Compare "ms/frame": less is faster (16.7 = 60 pictures a second).
cd "$(dirname "$0")" || exit 1
[ -x ./engine1999 ] || { echo "m3dbench: no ./engine1999 (run make first)"; exit 1; }
out=${TMPDIR:-/tmp}/m3dbench.$$.txt
echo "m3dbench: in the window that opens, type:"
echo '    LOAD "demos/deqq2"'
echo '    RUN'
echo "(it takes about half a minute; the window closes by itself)"
DEQQ2_BENCH="$out" ./engine1999 --allow=all
if [ -s "$out" ]; then cat "$out"; rm -f "$out"; else echo "m3dbench: no result (the program did not run to the end)"; fi
