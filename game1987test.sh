#!/bin/sh
# Automated test of the demo game demos/1987.bas ("1987": John, his house, the wheat field and his bicycle), run headless in the console host (the 3D mesh
# renderer's logic works there without a picture) with a play-back file:
#   G1987_HEADLESS=1 (no graphics), G1987_PLAY=file (commands: f v, t v, run v, brake v, use, wait seconds, goto x y heading, bike x y heading, state, cam,
#   ray x y z yaw pitch, quit; "state" and "cam" print key=value lines that this script reads).
# It checks the walk (through the open door, against the house wall), getting on and off the bicycle, pedalling from a standstill, the sprint and the stamina,
# rolling on, braking and reversing, the turning radius (a bicycle does not turn on the spot), a crash into a fence, the camera, a ray against the house, and that
# the world and the game are the same in every run. Exit status = number of failures. HOME is a temporary folder.
cd "$(dirname "$0")" || exit 1
T=$(mktemp -d) || exit 1
cleanup() {
    rm -f "$T/play" "$T/out"
    rmdir "$T/.local/share" "$T/.local" "$T/.config" "$T" 2>/dev/null
}
trap cleanup EXIT
fail=0
run() {
    HOME=$T G1987_HEADLESS=1 G1987_PLAY=$T/play timeout 300 sh -c 'printf "LOAD \"demos/1987\"\nRUN\nBYE\n" | ./engine1999c --allow=all' > $T/out 2>&1
}
# val WORD N KEY: the value of KEY in the Nth line that starts with WORD ("state", "cam", "ray")
val() {
    awk -v w="$1" -v n="$2" -v k="$3" '$1==w { c++; if (c==n) { for (i=2;i<=NF;i++) { split($i,a,"="); if (a[1]==k) { print a[2]; exit } } } }' $T/out
}
# expect NAME VALUE LOW HIGH: LOW <= VALUE <= HIGH
expect() {
    if [ -z "$2" ] || ! awk -v v="$2" -v l="$3" -v h="$4" 'BEGIN { exit !(v+0 >= l+0 && v+0 <= h+0) }'; then
        echo "FAILED: $1 (got '$2', expected $3 .. $4)"; fail=$((fail+1))
    fi
}
cat > $T/play <<'PLAY'
state
goto 0 -172 90
f 1
wait 3
state
shot walkpose
f 0
goto -60 -172 90
f 1
wait 2
state
f 0
goto 20 -176 270
use
wait 1
state
t 1
wait 2
state
t 0
f 1
wait 3
shot ridepose
state
run 1
wait 3
state
run 0
f 0
wait 3
state
f -1
wait 3
state
f 0
use
wait 1
state
bike 100 -300 90
goto 90 -290 90
use
wait 0.6
f 1
run 1
wait 4
state
f 0
run 0
use
wait 1
bike 700 400 0
goto 690 400 0
use
wait 1
state
t 1
wait 3
state
t 0
f 1
run 1
wait 2.5
state
t 1
wait 2
state
f 0
run 0
use
wait 1
bike 100 -300 90
goto 90 -290 90
use
wait 0.6
f 1
run 1
wait 3
f -1
run 0
wait 2
state
cam
ray 0 -300 20 90 0
ray 500 -300 20 90 10
quit
PLAY
run
# 1. the start; walking north from the start goes through the open door into the house
expect "start: John is on foot" "$(val state 1 mode)" 0 0
expect "start: John x" "$(val state 1 jx)" 0 0
expect "start: the bicycle stands" "$(val state 1 bv)" 0 0
expect "walk: through the open door into the house" "$(val state 2 jy)" -90 60
# 2. walking at the wall west of the door stops at the wall (y = -100 less the body radius 12), on the porch (z 7)
expect "walk: John stopped at the wall" "$(val state 3 jy)" -125 -100
expect "walk: John is on the porch" "$(val state 3 jz)" 5 9
# 3. getting on the bicycle: John sits on it
expect "mount: riding" "$(val state 4 mode)" 1 1
expect "mount: John is on the bicycle" "$(val state 4 jx)" 38 42
# 4. steering at a standstill does not turn a bicycle on the spot
expect "no turning on the spot" "$(val state 5 bh)" 268 272
# 5. pedalling from a standstill gets up to a cruising speed (it takes time), the bicycle rolls south down the road
expect "pedal: speed after 3 s" "$(val state 6 bv)" 70 190
expect "pedal: rolled south" "$(val state 6 by)" -700 -250
# 6. sprinting is faster and tires you
expect "sprint: faster" "$(val state 7 bv)" "$(val state 6 bv)" 340
expect "sprint: stamina used" "$(val state 7 st)" 0 85
# 7. rolling on: it slows down slowly, it does not stop at once
expect "coast: still rolling after 3 s" "$(val state 8 bv)" 20 "$(val state 7 bv)"
# 8. braking stops it, then you reverse slowly
expect "brake: stopped or reversing" "$(val state 9 bv)" -81 1
# 9. getting off: on foot again
expect "dismount: on foot" "$(val state 10 mode)" 0 0
# 10. a crash into the fence at speed (the fence is at y = -200, x >= 60): the bicycle is stopped
expect "crash: stopped by the fence" "$(val state 11 by)" -300 -180
expect "crash: speed after the hit" "$(val state 11 bv)" -60 40
# 11. at a standstill the heading stays; at speed a turn changes it by a sensible amount
expect "standstill: heading unchanged" "$(val state 13 bh)" -1 0
turned=$(awk -v a="$(val state 14 bh)" -v b="$(val state 15 bh)" 'BEGIN { d = b - a; if (d < 0) d = -d; print d }')
expect "turn at speed: heading changed" "$turned" 20 240
# 11b. blocked by the fence, pressing back reverses out of it
expect "reverse out of the fence" "$(val state 16 bv)" -81 -20
expect "reverse: moved away" "$(val state 16 by)" -400 -240
# 12. the camera stays above the ground, rays: the house wall is an object, open ground is not hit
expect "camera: z above the ground" "$(val cam 1 z)" 20 400
expect "ray at the house" "$(val ray 1 obj)" 300 300
expect "ray over open ground" "$(val ray 2 kind)" 0 1
# 13. the game ran without an error
if grep -q '^Error' $T/out; then echo "FAILED: the game stopped with: $(grep '^Error' $T/out)"; fail=$((fail+1)); fi
# 14. the world and the game are the same every run
first=$(grep -E '^(state|cam|ray)' $T/out | md5sum); run; second=$(grep -E '^(state|cam|ray)' $T/out | md5sum)
[ "$first" = "$second" ] || { echo "FAILED: two runs differ"; fail=$((fail+1)); }
if [ $fail -ne 0 ]; then echo "$fail failure(s)"; fi
exit $fail
