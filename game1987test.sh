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
wait 2
state
shot walkpose
f 0
goto -45 -172 90
f 1
wait 2.5
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
use
wait 1
goto 600 -500 0
f 0
t 0
run 0
fa 0.05
wait 1
state
fa 1
wait 0.3
state
wait 2
state
fa 0
wait 0.3
state
wait 2
state
fa 0
f 1
wait 0.3
state
wait 4
state
f 0
wait 0.3
state
wait 2
state
f 0
fa 0.05
run 0.5
wait 2
state
fa 0
f 1
t 0.4
run 1
wait 11.5
state
f 0
t 0
run 0
wait 4
state
f 1
run 1
wait 1.5
state
f 0
run 0
bike 700 400 0
goto 690 400 0
use
wait 0.6
fa 0.5
wait 5
state
fa 0
run 1
wait 6
state
fa 0
run 0
wait 1
use
wait 1
goto -2564 644 0
fa 0.35
wait 2.5
state
goto 3300 1500 0
wait 4
state
drones
wait 30
drones
wait 60
drones
shot drones
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
expect "walk: a wall stops John dead" "$(val state 3 jv)" 0 0
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
expect "sprint: faster, but not above 30 km/h (260 units a second)" "$(val state 7 bv)" "$(val state 6 bv)" 261
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
# 11c. the drones hover high up and fly their route (they move, stay in the world, keep their altitude)
expect "drone altitude" "$(val drone 1 z)" 340 450
expect "drone flew on" "$(awk -v a="$(val drone 1 x)" -v b="$(val drone 2 x)" -v c="$(val drone 1 y)" -v d="$(val drone 2 y)" 'BEGIN { print sqrt((a-b)^2+(c-d)^2) }')" 500 3000
expect "drone stays in the world" "$(val drone 3 x)" -3200 3200
expect "drone reached a later way point" "$(val drone 3 t)" 0 7
# 11d. variable walking speed: a barely deflected stick gives slow walking (22 + 133 * 0.05 / 0.95 = 29), a fully pushed one the top speed (155), accelerating
#      steadily (150 per second: 45 in 0.3 s); letting go slows down steadily (170 per second) to a stop; a held key raises the target from 22 to 155
#      (60 per second: 22 + 18 after 0.3 s) and letting go stops the same way
expect "analog: barely above the dead zone is slow" "$(val state 17 jv)" 25 33
expect "analog: accelerating towards the top speed" "$(val state 18 jv)" 60 90
expect "analog: top speed reached and held" "$(val state 19 jv)" 154 156
expect "analog: released, slowing down" "$(val state 20 jv)" 85 125
expect "analog: released, standing" "$(val state 21 jv)" 0 0
expect "key: target speed starts low and rises" "$(val state 22 jv)" 20 45
expect "key: top speed after holding" "$(val state 23 jv)" 154 156
expect "key: released, slowing down" "$(val state 24 jv)" 85 125
expect "key: released, standing" "$(val state 25 jv)" 0 0
# 11e. the trigger is analog: pulled half way it sets the target speed 22 + 133 * 0.5 = 88 on foot (with the stick barely deflected); a sprint on foot
#      tires John (the drain is 0.1 a second at the top speed), used up he walks slowly (22), after a rest (0.12 a second) he runs again; on the bicycle the stick's
#      half deflection sets the target 40 + 220 * 0.5 / 0.95 = 156 (WBMIN .. WBMAX), the trigger alone pulled all the way the top speed 260
expect "trigger: half pulled sets the target speed" "$(val state 26 jv)" 80 95
expect "exhausted: only the slow walk" "$(val state 27 jv)" 15 32
expect "exhausted: stamina below the recovery level" "$(val state 27 st)" 0 24
expect "rested: stamina is back" "$(val state 28 st)" 25 100
expect "rested: running again" "$(val state 29 jv)" 100 156
expect "bicycle: half stick gives a target between the minimum and the top speed" "$(val state 30 bv)" 148 160
expect "bicycle: the trigger alone, all the way, gives the top speed" "$(val state 31 bv)" 245 261
expect "bicycle: the sprint cost stamina" "$(val state 31 st)" 0 99
# 11f. no invisible walls: here a barrel of 301 times its size once stood (a ring round the yard, 2400 .. 3300 units out); walking on open ground goes on;
#      the edge of the world at 3420 stops John dead (the red tiles lie just outside it)
expect "open ground: no wall at (-2564, 644)" "$(val state 32 jx)" -2480 -2380
expect "edge: stopped at the line" "$(val state 33 jx)" 3419 3421
expect "edge: stopped dead" "$(val state 33 jv)" 0 0
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
