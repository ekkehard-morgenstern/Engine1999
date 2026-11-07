#!/bin/sh
# Automated test of the demo game demos/deqq.bas (roadmap item 76), run headless in the console host with a play-back file:
#   DEQQ_HEADLESS=1 (no graphics), DEQQ_PLAY=file (the commands; lines starting with # are test commands of the game).
# It plays the whole game (all eight parts, the ending), Deqq's behavior (he comes, bolts the door, hunts, throws out, leaves, watches)
# and saving / loading. Exit status = number of failures. HOME is a temporary folder, so no saved game of the user is touched.
cd "$(dirname "$0")" || exit 1
T=$(mktemp -d) || exit 1
# the temporary folder holds only what this script makes: the play file, the output and the saved game of the test
cleanup() {
    rm -f "$T/play" "$T/out" "$T/.local/share/deqq/deqq.sav"
    rmdir "$T/.local/share/deqq" "$T/.local/share" "$T/.local" "$T/.config/deqq" "$T/.config" "$T" 2>/dev/null
}
trap cleanup EXIT
fail=0
run() {
    HOME=$T DEQQ_HEADLESS=1 DEQQ_PLAY=$T/play timeout 120 sh -c 'printf "LOAD \"demos/deqq\"\nRUN\nBYE\n" | ./engine1999c --allow=all' > $T/out 2>&1
}
check() {
    if grep -q -- "$1" $T/out; then :; else echo "FAILED: $2 (no line matching: $1)"; fail=$((fail+1)); fi
}
nocheck() {
    if grep -q -- "$1" $T/out; then echo "FAILED: $2 (found: $1)"; fail=$((fail+1)); fi
}
# 1. the whole game, the ending without Deqq
cat > $T/play <<'PLAY'
#t 9999
w
n
look under pot
take key
e
unlock shed
e
open toolbox
take crank
take screwdriver
w
put crank on well
turn crank
take antenna
w
unlock door
in
open fridge
take jelly
take ice
put ice in sink
turn on tap
wait
wait
take compass
turn off tap
turn off oven
wait
wait
wait
open oven
take coil
n
n
install
s
in
drop key
take chair
e
take lamp
e
climb chair
take crystal
look behind photo
w
u
e
look under bed
give jelly to glorp
take nozzle
w
u
dial 731
take glyph
d
d
w
n
n
install
s
in
e
u
w
unscrew mirror
take plate
e
d
w
turn on lamp
d
pull plug
open washer
take cell
u
n
n
install
PLAY
run
check "Parts built in: 8 of 8" "walkthrough: all parts built"
check "THE END" "walkthrough: the ending"
check "I hid them SO well" "walkthrough: ending without Deqq"
nocheck "^? " "walkthrough: a runtime error"
# 2. Deqq arrives while the player is outside: he bolts the door, leaves it open when he goes
cat > $T/play <<'PLAY'
#seed 5
#t 4
wait
wait
wait
wait
in
#stay 5
wait
wait
wait
wait
wait
in
PLAY
run
check "A thin humming on the wind" "deqq: first warning"
check "front door slams and a bolt slides shut" "deqq: arrival bolts the door"
check "shut and bolted from the inside" "deqq: the bolted door"
check "He leaves the door open" "deqq: leaves the door open"
check "^Hall\\." "deqq: the door can be used again"
# 3. Deqq hunts a player who is in the house and throws him out
cat > $T/play <<'PLAY'
#seed 5
#goto HALL
#t 1
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
wait
PLAY
run
check "MY HOUSE" "hunt: thrown out"
check "^Front garden" "hunt: put in the front garden"
# 4. with four parts built in Deqq only watches
cat > $T/play <<'PLAY'
#seed 5
#goto CRASH
#have COIL
#have COMPASS
#have CRYSTAL
#have ANTENNA
install
#goto LIVING
#deqq HALL
#stay 30
w
wait
wait
wait
wait
wait
wait
wait
wait
PLAY
run
check "turns its head toward you" "watch: the ship is half built"
check "Deqq is here, watching you" "watch: Deqq watches"
nocheck "MY HOUSE" "watch: thrown out although half built"
# 5. the ending with Deqq at home, hiding, saving and loading
cat > $T/play <<'PLAY'
#goto BEDROOM
hide
look
come out
#have JELLY
save
drop all
load
inventory
#goto CRASH
#deqq HALL
#have COIL
#have COMPASS
#have CRYSTAL
#have ANTENNA
#have NOZZLE
#have PLATE
#have FUELCELL
#have GLYPH
install
PLAY
run
check "You are hiding in" "hiding: peeking while hiding"
check "Game saved" "save"
check "Game loaded" "load"
check "You carry: green jelly" "load: the inventory is back"
check "Thank you" "ending with Deqq: he thanks the player"
[ $fail -eq 0 ] && echo "deqqtest: all passed"
exit $fail
