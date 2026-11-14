#!/bin/sh
# Test of the 32 bit layer of the hardware 3D renderer in the real window host code (sdlscreen.c), in an X server without a display (Xvfb):
# runs ./gl3dtest1, which puts a picture into the layer and reads the composed screen back (see gl3dtest1.c). Needs Xvfb and ./gl3dtest1 (make
# gl3dtest1); if one is missing the test is SKIPPED (exit status 0). Not part of "make check" (the program opens a full-screen window): run it
# with "make hosttest" or "sh gl3dtest.sh".
cd "$(dirname "$0")" || exit 1
skip() { echo "gl3dtest: skipped ($1)"; exit 0; }
command -v Xvfb > /dev/null 2>&1 || skip "no Xvfb"
[ -x ./gl3dtest1 ] || skip "no ./gl3dtest1: run make gl3dtest1 first"
XPID=""; TPID=""
cleanup() {
    [ -n "$TPID" ] && pkill -9 -P "$TPID" > /dev/null 2>&1
    for p in $TPID $XPID; do [ -n "$p" ] && kill -9 "$p" > /dev/null 2>&1; done
}
trap cleanup EXIT INT TERM
D=:$((60 + $$ % 40))
Xvfb "$D" -screen 0 1280x800x24 > /dev/null 2>&1 &
XPID=$!
sleep 2
DISPLAY=$D SDL_AUDIODRIVER=dummy timeout 60 ./gl3dtest1 &
TPID=$!
wait "$TPID"
rc=$?
TPID=""
[ "$rc" -eq 0 ] || { echo "FAILED: gl3dtest (status $rc)"; exit 1; }
echo "gl3dtest: ok"
