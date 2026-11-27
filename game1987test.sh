#!/bin/sh
# Automated test of the demo game demos/1987.bas ("1987": John, his house, the wheat field and his bicycle), run headless in the console host (the 3D mesh
# renderer's logic works there without a picture) with a play-back file:
#   G1987_HEADLESS=1 (no graphics), G1987_PLAY=file (commands: f v, t v, run v, brake v, use, wait seconds, goto x y heading, bike x y heading, state, cam,
#   ray x y z yaw pitch, chunks, quit; "state", "cam" and "chunks" print key=value lines that this script reads).
# It checks the walk (through the open door, against the house wall), getting on and off the bicycle, pedalling from a standstill, the sprint and the stamina,
# rolling on, braking and reversing, the turning radius (a bicycle only walks round on the spot, slowly), a crash into a fence, the camera, a ray against the house, and that
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
bike 40 -176 270
goto 20 -176 270
use
wait 1
state
t 1
wait 2
state
t -1
wait 2
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
t -1
wait 3
t 0
f 1
run 1
wait 2.5
state
t 1
wait 2
state
t 0
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
chunks
bike 600 -500 0
goto 560 -540 0
wait 2
state
wait 2.5
state
goto 590 -500 0
use
wait 1
state
f 1
run 1
wait 4
state
f 0
run 0
use
wait 1.5
state
wait 2.5
state
wait 5
state
goto 1500 -900 0
hide
f 0
fa 0
run 0
t 0
rigs 1
wait 4
state
wait 5
state
f 1
wait 2
state
rigs 0
f 0
run 0
fa 0
t 0
wait 0.5
goto 0 -172 270
f 1
wait 1.6
state
f 0
goto 20 -240 0
f 1
wait 1.2
state
f 0
goto 600 -500 0
wait 3
cam
orbit 1
wait 1.5
cam
orbit 0
wait 5
cam
wait 5
state
f 1
wait 0.3
state
f 0
drones
wait 30
drones
wait 60
drones
shot drones
poles
cam
ray 0 -300 20 90 0
ray 500 -300 20 90 10
goto 20000 -15000 0
chunks
f 0
fa 1
goto 11200 0 0
wait 40
state
fa 0
goto 2000 -5000 0
fa 1
wait 30
state
fa 0
chunks
goto 1350 8550 0
wait 1
chunks
goto -9000 7000 0
wait 2
chunks
goto 0 -100 90
wait 1
state
goto 0 -172 90
wait 1
state
goto -1500 -1500 0
wait 1
state
goto 1100 650 0
wait 1
state
goto 13500 0 0
wait 20
state
goto 5000 -5000 0
wait 5
state
goto 3500 -5000 0
wait 3
state
goto 6800 -5000 0
wait 3
state
goto 21750 500 0
wait 4
state
goto -5628 -45700 0
wait 3
chunks
goto -6000 -9000 0
wait 5
pop 300 5
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
# 4. steering at a standstill turns the bicycle slowly on the spot (45 degrees a second, walking it round; 2 s of steering: about 90 degrees), so a rider can always turn away
expect "slow turning on the spot (2 s: about 90 degrees)" "$(awk -v a="$(val state 4 bh)" -v b="$(val state 5 bh)" 'BEGIN { d = b - a; if (d < 0) d = -d; print d }')" 70 100
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
# 11. at a standstill steering walks the bike round slowly (45 degrees a second: 3 s about 135, the heading is 0 at the start); at speed a turn changes it by a sensible amount
expect "standstill: slow turning, not a spin (3 s)" "$(awk -v h="$(val state 13 bh)" 'BEGIN { if (h > 180) h = 360 - h; print h }')" 100 160
turned=$(awk -v a="$(val state 14 bh)" -v b="$(val state 15 bh)" 'BEGIN { d = b - a; if (d < 0) d = -d; print d }')
expect "turn at speed: heading changed" "$turned" 20 240
# 11b. blocked by the fence, pressing back reverses out of it
expect "reverse out of the fence" "$(val state 16 bv)" -81 -20
expect "reverse: moved away" "$(val state 16 by)" -400 -240
# 11c. the drones live (owner, 2026-11-25): they hold their height above the ground, move round on their own (cruise, scan, look), stay near John, and never make
#      more than 3 in one chunk (500 m: `peak` is the largest number seen every quarter second, `maxcell` the number now)
expect "drone count" "$(val drone 1 n)" 6 6
expect "drone height above the ground" "$(awk -v z="$(val drone 1 z)" -v g="$(val drone 1 gz)" 'BEGIN { print z - g }')" 140 520
expect "drone height above the ground, later" "$(awk -v z="$(val drone 3 z)" -v g="$(val drone 3 gz)" 'BEGIN { print z - g }')" 140 900
expect "drone moved on its own" "$(awk -v a="$(val drone 1 x)" -v b="$(val drone 3 x)" -v c="$(val drone 1 y)" -v d="$(val drone 3 y)" 'BEGIN { print sqrt((a-b)^2+(c-d)^2) }')" 50 30000
expect "drone has a mode" "$(val drone 3 m)" 0 2
expect "drone stays near John" "$(val drone 3 x)" -20000 20000
expect "at most 3 drones in a chunk now" "$(val drone 3 maxcell)" 1 3
expect "at most 3 drones in a chunk, ever" "$(val drone 3 peak)" 1 3
# 11c2. the telephone poles stand on the ground (a ray at 100 units above the ground hits, one at 170 passes over the top)
expect "poles stand on the ground" "$(val pole 1 hit)" 7 9
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
#      there is no edge of the world any more (since the chunks, roadmap item 95): walking east goes on past the old line at 3420, and the chunks round John are built as he goes
expect "open ground: no wall at (-2564, 644)" "$(val state 32 jx)" -2480 -2380
expect "no edge: John walks on past the old line at 3420" "$(val state 33 jx)" 3430 4000
expect "no edge: John is still moving" "$(val state 33 jv)" 1 200
expect "chunks: the 17 x 17 window round John is complete after the walk" "$(val chunks 1 have)" 289 289
expect "chunks: none missing after the walk" "$(val chunks 1 missing)" 0 0
expect "chunks: none missing or of the wrong level after the walk (levels change as John goes)" "$(val chunks 1 stale)" 0 0
expect "chunks: the 5 x 5 chunks round John are fine (level 0), the others coarse" "$(val chunks 1 fine)" 25 25
expect "chunks: trees grow in the forests outside the hand-made area" "$(val chunks 1 trees)" 1 1176
# 11f2. a jump far away (a goto builds all missing chunks at once): the window is complete there too, the number of trees stays within the 24 a chunk may carry, and the same
#       place built again gives the same chunks (the chunks line is part of the two-run comparison at the end)
expect "chunks: far jump (20000, -15000): window complete" "$(val chunks 2 have)" 289 289
expect "chunks: far jump: none missing" "$(val chunks 2 missing)" 0 0
expect "chunks: far jump: right levels at once" "$(val chunks 2 stale)" 0 0
expect "chunks: far jump: 25 fine chunks" "$(val chunks 2 fine)" 25 25
expect "chunks: far jump: trees within the budget" "$(val chunks 2 trees)" 0 1176
expect "chunks: second far jump (-9000, 7000) and 2 s: window complete" "$(val chunks 5 have)" 289 289
expect "chunks: second far jump: nothing stale" "$(val chunks 5 stale)" 0 0
# 11f3. hills, mountains and lakes (outside the hand-made area): ridged hills (rocky, walkable), real mountains (up to 1190 m: 37000 units of 3.2 cm; steep walls 14000 .. 22000 units from the house) and real lakes (basins 350 deep, 100 .. 190 m across) in the ground function; John walks over the rocky hills (every slope up to 72 degrees is walkable) but does not wade into
#       deep water: walking east from (11200, 0) he crosses the mountain whose top is at (13500, 0) and is beyond it after 40 s; walking east from (2000, -5000) he stops dead at the shore of a lake (water level -12, wading is possible down to -24)
expect "lake: the chunks round the shore have water (translucent planes at the water level)" "$(val chunks 3 water)" 1 49
# 11f4. a city ruin site (big region of 7200 units, a chance of 30 %): buildings of brick walls; roads between the region points; hay bales in the fields; the chunks of the window around (1350, 8550)
expect "ruins: the chunks round (1350, 8550) have buildings of a city ruin" "$(val chunks 4 ruins)" 3 49
expect "roads: some chunks have a road" "$(val chunks 4 roads)" 1 289
expect "bales: hay bales in the fields" "$(val chunks 4 bales)" 1 49
expect "mountain: John walks over the rocky hill (the top is at 13500)" "$(val state 48 jx)" 13800 20000
expect "lake: John stops at the shore (the water starts at x = 2300 on this line: a pond west of a dirt road)" "$(val state 49 jx)" 2230 2400
expect "lake: stopped dead" "$(val state 49 jv)" 0 0
expect "lake: the ground under him is not deeper than the limit (water level -12, 12 below it is too deep)" "$(val state 49 gh)" -26 -12
# 11i. the surface under the feet comes from the pictures (M3D("FLOORTEX"): FN SURF, state "sf": 0 grass, 1 sand / mud, 2 wood / hard, 3 wheat), not from the height
expect "surface: the house floor (tiles) is hard" "$(val state 50 sf)" 2 2
expect "surface: the sand road in the yard" "$(val state 51 sf)" 1 1
expect "surface: grass" "$(val state 52 sf)" 0 0
expect "surface: the wheat field" "$(val state 53 sf)" 3 3
expect "surface: the rock of a mountain is hard" "$(val state 54 sf)" 2 2
expect "surface: the mud of a lake bed is soft sand" "$(val state 55 sf)" 1 1
# 11f5. real sizes: a lake is 100 m (3125 units) across or more: both ends of a line of 3300 units across the lake at y = -5000 are under water (the water level is -12); the highest mountain is more
#       than 1000 m (31250 units) high; the four rings of the far ground are built (jobs of the worker thread, all taken)
expect "lake: the west end of a line across it is under water" "$(val state 56 gh)" -400 -13
expect "lake: the east end, 3300 units (105 m) away, too" "$(val state 57 gh)" -400 -13
expect "mountain: more than 1000 m (31250 units) high" "$(val state 58 gh)" 31250 45000
expect "rings: the three rings of the far ground are there" "$(val chunks 5 rings)" 3 3
expect "rings: no ground job left" "$(val chunks 5 busy)" 0 0
# 11f7. no popping: walking 300 steps of 5 units (1500 units) over hilly ground, the ground ahead (40 places 700 .. 6400 units away, read from the chunks' meshes) never changes by more than 6 units from
#       one step to the next: the chunks morph between their levels of detail, so no hill rises or goes in front of the player (before the morph the levels changed with a step of up to 100 units)
expect "popping: the ground ahead changes by less than 6 units a step" "$(awk '$1=="poptest" { split($3,a,"="); print a[2] }' $T/out)" 0 6
expect "popping: samples were taken" "$(awk '$1=="poptest" { split($2,a,"="); print a[2] }' $T/out)" 1000 100000
# 11f6. tarred roads (440 wide, 2 lanes each way with edge lines and dashes, picture 327) with reflector posts every 50 m, and trees that keep away from the roads: the chunks round (-5628, -45700): a tarred road curves through them, far from an intersection (the old test place (29400, -17400) lost its road when roads learned to keep out of the mountains)
expect "tarred road: the floor under the road is the tar picture (hard surface)" "$(val chunks 6 roads)" 1 289
expect "tarred road: reflector posts beside it" "$(val chunks 6 posts)" 1 289
expect "chunks: second far jump: some forest" "$(val chunks 5 trees)" 1 1176
expect "chunks: a different place has other chunks than the first" "$(awk -v a="$(val chunks 2 sig)" -v b="$(val chunks 5 sig)" 'BEGIN { print (a != b) ? 1 : 0 }')" 1 1
# 11g. a bicycle cannot stand by itself: at the start it leans against the yard's fence (bst 1) and E picks it up (riding, upright); a riderless one rolls on for 3 s and then falls over
#      (bst 3), slides on its side and turns (bst 4) and stops (bst 5, lying); a lying one is picked up like before; E near a falling or sliding one does nothing
expect "start: the bicycle leans against the fence" "$(val state 1 bst)" 1 1
expect "riderless, standing still: upright after 2 s" "$(val state 34 bst)" 0 0
expect "riderless, standing still: lying after 4.5 s" "$(val state 35 bst)" 5 5
expect "lying: picked up, riding" "$(val state 36 mode)" 1 1
expect "lying: picked up, upright" "$(val state 36 bst)" 0 0
expect "riding fast" "$(val state 37 bv)" 200 261
expect "riderless, moving: still rolling upright after 1.5 s" "$(val state 38 bst)" 0 0
expect "riderless, moving: still moving after 1.5 s" "$(val state 38 bv)" 120 261
expect "riderless, moving: fell over (falling or sliding) after 4 s" "$(val state 39 bst)" 3 4
expect "riderless, moving: lying after 9 s" "$(val state 40 bst)" 5 5
expect "riderless, moving: stopped" "$(val state 40 bv)" 0 0
expect "riderless, moving: it turned while sliding" "$(awk -v a="$(val state 38 bh)" -v b="$(val state 40 bh)" 'BEGIN { d = b - a; if (d < 0) d = -d; print d }')" 20 1000
# 11h. the figure: the head turns to look (it follows a turn: t 1 gives about 22 degrees to the left unless a drone is in sight, then up to 80 either way), it breathes (the phase advances)
expect "breathing: the phase advances" "$(awk -v a="$(val state 1 brp)" -v b="$(val state 40 brp)" 'BEGIN { print (b > a) ? 1 : 0 }')" 1 1
expect "head: yaw and pitch stay in range" "$(awk -v y="$(val state 40 hly)" -v p="$(val state 40 hlp)" 'BEGIN { print (y >= -80 && y <= 80 && p >= -15 && p <= 40) ? 1 : 0 }')" 1 1
# 11i. mesh cache: a John who stands still (only the breath changes, rounded to a quarter of its range) and a head whose rounded angles stay the same are not built again every
#      frame; 300 frames of standing still build the body only a few times, walking builds it every frame
expect "cache: standing still 5 s (300 frames): few body builds" "$(awk -v a="$(val state 41 nj)" -v b="$(val state 42 nj)" 'BEGIN { print b - a }')" 0 40
expect "cache: standing still 5 s (300 frames): few head builds" "$(awk -v a="$(val state 41 nh)" -v b="$(val state 42 nh)" 'BEGIN { print b - a }')" 0 8
expect "cache: walking 2 s (120 frames): the body is built about every frame" "$(awk -v a="$(val state 42 nj)" -v b="$(val state 43 nj)" 'BEGIN { print b - a }')" 100 125
expect "cache: the bicycle at rest keeps its spoke meshes" "$(awk -v a="$(val state 41 nb)" -v b="$(val state 42 nb)" 'BEGIN { print b - a }')" 0 3
# 11j. the bicycle leans against the outside of the yard's south fence, east of the gate (130, -214): walking out of the gate and along the fence takes John within reach of it
expect "gate: John walked out of the yard through the gate" "$(val state 44 jy)" -300 -205
expect "fence: John at the bicycle's place within reach (70 units)" "$(awk -v x="$(val state 45 jx)" -v y="$(val state 45 jy)" 'BEGIN { print sqrt((x-130)^2+(y+214)^2) }')" 0 69
# 11k. the camera is an agent: it follows John from behind and above; an orbit instruction (as if Z were held) takes it round him (it accelerates: it is not there at once); when the
#      instruction stops it goes back behind him; left alone for a while (7 s) it acts on its own (cmb 2: circling, 3: watching a point of interest); the player's input ends that at once
expect "camera: behind John, looking the way he faces" "$(val cam 1 yaw)" -10 10
expect "camera: above the ground" "$(val cam 1 z)" 20 400
expect "camera: orbiting takes it round John" "$(awk -v y="$(val cam 2 yaw)" 'BEGIN { print (y > 60 || y < -60) ? 1 : 0 }')" 1 1
expect "camera: goes back behind him when the instruction stops" "$(val cam 3 yaw)" -10 10
expect "camera: acts on its own when the player is idle" "$(val state 46 cmb)" 2 3
expect "camera: the player's input ends it" "$(val state 47 cmb)" 0 0
# 12. the camera stays above the ground, rays: the house wall is an object, open ground is not hit
expect "camera: z above the ground" "$(val cam 4 z)" 20 400
expect "ray at the house" "$(val ray 1 obj)" 300 300
expect "ray over open ground" "$(val ray 2 kind)" 0 1
# 13. the game ran without an error
if grep -q '^Error' $T/out; then echo "FAILED: the game stopped with: $(grep '^Error' $T/out)"; fail=$((fail+1)); fi
# 14. the world and the game are the same every run
first=$(grep -E '^(state|cam|ray|chunks)' $T/out | md5sum); run; second=$(grep -E '^(state|cam|ray|chunks)' $T/out | md5sum)
[ "$first" = "$second" ] || { echo "FAILED: two runs differ"; fail=$((fail+1)); }
# 14b. no line number twice in the game and its libraries (a duplicate silently replaces a line: once it removed the CALL that applies a setting)
for f in demos/1987.bas demos/lib/prefs3d.lib demos/lib/w3dkit.lib demos/lib/input.lib; do
    d=$(awk '$1 ~ /^[0-9]+$/ { print $1 }' $f | sort -n | uniq -d | head -3 | tr '\n' ' ')
    if [ -n "$d" ]; then echo "FAILED: $f has line numbers twice: $d"; fail=$((fail+1)); fi
done
# 16. the beam that makes John black out (owner, 2026-11-25 / 26): a drone that has found John locks its beam on him (the spot chases at 1200 units a second), the beam grows for a second or two and
#     he collapses (off the bicycle, if he rides); the drone then turns the beam on the camera (mode 4), 1.5 s later the camera falls and lies on the ground (cmf 2), only then the picture goes black;
#     he lies for 10 .. 14 seconds, comes to, the camera rises (cmf 0) and the drone moves on; a scanning drone finds John when its spot stays on him; a drone finds him when he is within 2 m of it;
#     two drones never overlap; the beam's spot lies on the highest thing below (a roof), not on the ground under it. `catch k x` puts drone k over John (x units east) in the focus mode, `scanat`
#     a scanning drone's spot on him, `above k` a cruising drone 30 units from him
cat > $T/play <<'PLAY'
bike 40 -176 270
goto 20 -176 270
use
wait 1
state
catch 0 0
wait 1
state
drones 0
wait 1.2
state
drones 0
wait 2.2
state
wait 12
state
drones 0
scanat
wait 0.5
drones 0
spotz 0 40
quit
PLAY
HOME=$T G1987_HEADLESS=1 G1987_PLAY=$T/play timeout 300 sh -c 'printf "LOAD \"demos/1987\"\nRUN\nBYE\n" | ./engine1999c --allow=all' > $T/out 2>&1
expect "blackout: John rides the bicycle first" "$(val state 1 mode)" 1 1
expect "blackout: the meter fills while he is in the beam (1 s)" "$(val state 2 bl)" 20 65
expect "blackout: the drone focuses on him and the beam grows" "$(val drone 1 ri)" 45 95
expect "blackout: the drone is in the focus mode" "$(val drone 1 m)" 3 3
expect "blackout: John lies unconscious after about 2 seconds" "$(val state 3 mode)" 5 5
expect "blackout: the camera still stands (the picture is not dark yet)" "$(val state 3 cmf)" 0 0
expect "blackout: the drone turns the beam on the camera" "$(val drone 2 m)" 4 4
expect "blackout: he fell off the bicycle (beside it)" "$(awk -v a="$(val state 3 jx)" -v b="$(val state 3 bx)" -v c="$(val state 3 jy)" -v d="$(val state 3 by)" 'BEGIN { print sqrt((a-b)^2+(c-d)^2) }')" 20 60
expect "blackout: the camera lies on the ground" "$(val state 4 cmf)" 2 2
expect "blackout: the camera is low (a little above the ground)" "$(val state 4 cmg)" 0 25
expect "blackout: he comes to" "$(val state 5 mode)" 0 0
expect "blackout: the camera is up again" "$(val state 5 cmf)" 0 0
expect "blackout: the meter is empty again" "$(val state 5 bl)" 0 0
expect "blackout: the drone lost interest and moved on" "$(val drone 3 m)" 0 2
expect "blackout: a scanning drone notices him when its spot is on him" "$(val drone 4 m)" 3 3
expect "beam: the spot lies on the roof of the house, not on the ground under it" "$(val spotz 1 z)" 60 400
expect "beam: the ground there is low" "$(val spotz 1 gz)" -50 50
if grep -q '^Error' $T/out; then echo "FAILED: the blackout run stopped with: $(grep '^Error' $T/out)"; fail=$((fail+1)); fi
# on foot, two drones at once, and a drone that John walks under
cat > $T/play <<'PLAY'
goto 20 -176 270
wait 0.5
above 2
wait 0.3
drones 2
state
catch 0 0
catch 1 20
wait 1
drones 0
drones 1
wait 2
state
quit
PLAY
HOME=$T G1987_HEADLESS=1 G1987_PLAY=$T/play timeout 300 sh -c 'printf "LOAD \"demos/1987\"\nRUN\nBYE\n" | ./engine1999c --allow=all' > $T/out 2>&1
expect "drones: a drone finds John when he is within 2 m of it" "$(val drone 1 m)" 3 3
expect "drones: two drones focus on John on foot: he collapses" "$(val state 2 mode)" 5 5
sep=$(awk -v ax="$(val drone 2 x)" -v bx="$(val drone 3 x)" -v ay="$(val drone 2 y)" -v by="$(val drone 3 y)" 'BEGIN { dx = ax - bx; dy = ay - by; if (dx < 0) dx = -dx; if (dy < 0) dy = -dy; print (dx > dy) ? dx : dy }')
expect "drones: two drones over John do not overlap (their boxes are 120 apart)" "$sep" 100 400
if grep -q '^Error' $T/out; then echo "FAILED: the two-drone run stopped with: $(grep '^Error' $T/out)"; fail=$((fail+1)); fi
# a drone within 10 m finds John, one within 50 m flies toward him, and in the quiet time (PROT) after he came to it steers away (owner, 2026-11-26)
cat > $T/play <<'PLAY'
hide
goto 20 -176 270
wait 0.5
place 0 280
wait 0.4
dist 0
place 1 1300
wait 0.1
dist 1
wait 3
dist 1
prot 30
place 2 900
wait 0.1
dist 2
wait 3
dist 2
quit
PLAY
HOME=$T G1987_HEADLESS=1 G1987_PLAY=$T/play timeout 300 sh -c 'printf "LOAD \"demos/1987\"\nRUN\nBYE\n" | ./engine1999c --allow=all' > $T/out 2>&1
expect "near: a drone 280 units (9 m) away finds John" "$(val dist 1 m)" 3 3
expect "near: a drone 1300 units (42 m) away flies toward him (closer after 3 s)" "$(awk -v a="$(val dist 2 d)" -v b="$(val dist 3 d)" 'BEGIN { print a - b }')" 60 2000
expect "near: in the quiet time a drone 900 units away steers away (farther after 3 s)" "$(awk -v a="$(val dist 4 d)" -v b="$(val dist 5 d)" 'BEGIN { print b - a }')" 60 2000
if grep -q '^Error' $T/out; then echo "FAILED: the drone-near run stopped with: $(grep '^Error' $T/out)"; fail=$((fail+1)); fi
# 17. day and night (owner, 2026-11-26): the daylight M3D AMBIENT follows the hour (1 by day, 0.16 at night), the bicycle's headlamp (M3D LAMP 0) is on while John rides in the dark and
#     off by day and on foot; the time passes (a day is G1987_DAYLEN seconds); `time h` sets the hour
cat > $T/play <<'PLAY'
time 12
state
time 0
state
time 6
state
time 18
state
time 21
state
bike 40 -176 270
goto 20 -176 270
use
wait 1
state
f 1
wait 3
state
time 12
state
time 23
state
f 0
brake 1
wait 0.3
brake 0
wait 2
state
quit
PLAY
HOME=$T G1987_DAYLEN=60 G1987_HEADLESS=1 G1987_PLAY=$T/play timeout 300 sh -c 'printf "LOAD \"demos/1987\"\nRUN\nBYE\n" | ./engine1999c --allow=all' > $T/out 2>&1
expect "day: noon is full daylight" "$(val state 1 amb)" 100 100
expect "night: midnight is dark" "$(val state 2 amb)" 10 20
expect "dawn: 6 o'clock is in between" "$(val state 3 amb)" 25 80
expect "dusk: 18 o'clock is in between" "$(val state 4 amb)" 25 80
expect "night: 21 o'clock is dark" "$(val state 5 amb)" 10 20
expect "lamp: off while John walks" "$(val state 5 lamp)" 0 0
expect "lamp: off while John stands still on the bicycle (the dynamo gives nothing)" "$(val state 6 lamp)" 0 0
expect "lamp: on while John rides in the dark" "$(val state 7 lamp)" -1 -1
expect "dynamo: gives current while he rides" "$(val state 7 dyn)" 20 100
expect "lamp: off by day" "$(val state 8 lamp)" 0 0
expect "lamp: on again at night" "$(val state 9 lamp)" -1 -1
expect "lamp: out again when he stops" "$(val state 10 lamp)" 0 0
expect "time: it passes (60 s a day: 1 s is 0.4 h; the hour has moved on)" "$(awk -v a="$(val state 9 tod)" -v b="$(val state 10 tod)" 'BEGIN { d = b - a; if (d < 0) d += 2400; print d }')" 20 400
if grep -q '^Error' $T/out; then echo "FAILED: the day / night run stopped with: $(grep '^Error' $T/out)"; fail=$((fail+1)); fi
# 18. a bicycle cannot stand by itself (owner, 2026-11-27): standing still on it John puts a foot down (FD goes to 1, the bicycle leans), pedalling takes it up again
cat > $T/play <<'PLAY'
bike 40 -176 270
goto 20 -176 270
use
wait 0.1
state
wait 1
state
f 1
wait 1.5
state
f 0
quit
PLAY
HOME=$T G1987_HEADLESS=1 G1987_PLAY=$T/play timeout 300 sh -c 'printf "LOAD \"demos/1987\"\nRUN\nBYE\n" | ./engine1999c --allow=all' > $T/out 2>&1
expect "foot: not down at the moment he gets on" "$(val state 1 fd)" 0 0
expect "foot: down after a second of standing still" "$(val state 2 fd)" 100 100
expect "foot: up again when he pedals" "$(val state 3 fd)" 0 0
if grep -q '^Error' $T/out; then echo "FAILED: the foot-down run stopped with: $(grep '^Error' $T/out)"; fail=$((fail+1)); fi
# 15. the static world (world mode: G1987_WORLD names the folder of the world, G1987_WORLDSIZE its size in chunks): the world is made (a progress loop without a picture), the game runs on it
#     (jumps, walking on the ground of the world), the position is saved in the folder when the game ends and read again by the next start
W=$T/world
rm -rf "$W" 2>/dev/null
cat > $T/play <<'PLAY'
state
goto 0 -172 90
f 1
wait 1
state
goto 9000 7000 45
wait 1
state
goto -30000 12000 200
wait 1
state
reset
state
coords -1
poles
quit
PLAY
HOME=$T G1987_WORLD=$W G1987_WORLDSIZE=16 G1987_HEADLESS=1 G1987_PLAY=$T/play timeout 300 sh -c 'printf "LOAD \"demos/1987\"\nRUN\nBYE\n" | ./engine1999c --allow=all' > $T/out 2>&1
[ -s "$W/world.dat" ] || { echo "FAILED: world mode made no world"; fail=$((fail+1)); }
expect "world: John at the house" "$(val state 2 jx)" -5 60
expect "world: after a jump" "$(val state 3 jx)" 8000 10000
expect "world: John stands on the ground of the world" "$(awk -v z="$(val state 3 jz)" 'BEGIN { print (z > -400 && z < 4000) ? 1 : 0 }')" 1 1
grep -q '^coords X  *-*[0-9.]*  Y  *-*[0-9.]*  Z  *-*[0-9.]* M  CHUNK  *8,  *[78]$' $T/out || { echo "FAILED: the coordinates line: $(grep '^coords' $T/out)"; fail=$((fail+1)); }
expect "world: reset puts John back at the start" "$(val state 5 jx)" -5 60
expect "world: the poles stand on the raised ground" "$(val pole 1 hit)" 9 9
expect "world: reset puts John back at the start (y)" "$(val state 5 jy)" -200 -140
if grep -q '^Error' $T/out; then echo "FAILED: the game in world mode stopped with: $(grep '^Error' $T/out)"; fail=$((fail+1)); fi
# the position (play mode): the game starts from player.dat (NEWGAME reads it), `state` shows where John is, `wsave` writes it again
printf 'state\nwsave\nquit\n' > $T/play2
runplay() { HOME=$T G1987_WORLD=$W G1987_WORLDSIZE=16 G1987_HEADLESS=1 G1987_PLAY=$T/play2 timeout 300 sh -c 'printf "LOAD \"demos/1987\"\nRUN\nBYE\n" | ./engine1999c --allow=all' > $T/out 2>&1; }
printf '4000\n-3000\n90\n4100\n-3100\n' > "$W/player.dat"
runplay
expect "world: the saved position was loaded" "$(val state 1 jx)" 3900 4100
saved=$(head -1 "$W/player.dat" 2>/dev/null)
expect "world: the saved position was written again" "${saved:-0}" 3900 4100
# a saved position that is no place to start from (outside the world) is not used
printf '99999999\n-3000\n90\n4100\n-3100\n' > "$W/player.dat"
runplay
expect "world: an unfit saved position falls back to the start" "$(val state 1 jx)" -200 200
# a new world forgets the old saved position (the file of an earlier world is left over when the world is made again)
rm -f "$W/world.dat"; printf '4000\n-3000\n90\n4100\n-3100\n' > "$W/player.dat"
runplay
expect "world: a newly made world starts at the start" "$(val state 1 jx)" -200 200
[ -s "$W/world.dat" ] || { echo "FAILED: the world was not made again"; fail=$((fail+1)); }
rm -rf "$W" 2>/dev/null
if [ $fail -ne 0 ]; then echo "$fail failure(s)"; fi
exit $fail
