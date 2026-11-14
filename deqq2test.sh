#!/bin/sh
# Automated test of the demo game demos/deqq2.bas (Deqq's House II, roadmap item 79), run headless in the console host (the 3D renderer's logic works there
# without a picture) with a play-back file:
#   DEQQ2_HEADLESS=1 (no graphics), DEQQ2_PLAY=file (commands: step n, goto x y angle, walk x y, face x y, use, drop, give n, dial 731, deqq x y, deqqon, deqqoff,
#   state, say text, continue, new, quit; the game's messages come out as lines starting with ">", test output with "#").
# It plays the whole game (all eight parts, locked doors, the carrying limit, the ending), Deqq's round, his catching you, losing your track, hiding, and the saved game.
# Exit status = number of failures. HOME is a temporary folder, so no saved game of the user is touched.
cd "$(dirname "$0")" || exit 1
T=$(mktemp -d) || exit 1
cleanup() {
    rm -f "$T/play" "$T/out" "$T/.local/share/deqq2/deqq2.sav"
    rmdir "$T/.local/share/deqq2" "$T/.local/share" "$T/.local" "$T/.config/deqq2" "$T/.config" "$T" 2>/dev/null
}
trap cleanup EXIT
fail=0
run() {
    HOME=$T DEQQ2_HEADLESS=1 DEQQ2_PLAY=$T/play timeout 300 sh -c 'printf "LOAD \"demos/deqq2\"\nRUN\nBYE\n" | ./engine1999c --allow=all' > $T/out 2>&1
}
check() {
    if grep -q -- "$1" $T/out; then :; else echo "FAILED: $2 (no line matching: $1)"; fail=$((fail+1)); fi
}
nocheck() {
    if grep -q -- "$1" $T/out; then echo "FAILED: $2 (found: $1)"; fail=$((fail+1)); fi
}
count() {
    n=$(grep -c -- "$1" $T/out)
    if [ "$n" != "$2" ]; then echo "FAILED: $3 (expected $2 lines matching '$1', found $n)"; fail=$((fail+1)); fi
}
# 1. the whole game without Deqq
cat > $T/play <<'PLAY'
deqqoff
goto 1232 850 0
face 1260 850
use
step 30
goto -240 880 180
face -300 900
use
use
goto 1232 850 0
face 1260 850
use
step 150
walk 1330 850
goto 1400 885 90
face 1400 860
use
use
face 1388 872
use
goto 1040 880 180
face 1020 880
use
goto 1030 868 180
face 1020 872
use
goto -250 1320 90
face -250 1370
use
goto 480 150 90
walk 480 330
walk 480 365
walk 480 580
walk 480 635
walk 430 640
walk 330 620
walk 260 560
walk 250 460
face 250 420
use
use
use
use
goto 300 560 180 114
drop
goto 250 455 90 114
face 250 426
use
goto -250 1320 90
face -250 1370
use
goto 300 620 90
face 300 665
use
use
step 1850
use
use
goto -250 1320 90
face -250 1370
use
goto 235 395 90
face 225 345
use
use
goto 645 545 90 114
face 650 590
use
goto 725 555 90 114
face 730 590
use
goto -250 1320 90
face -250 1370
use
goto 262 395 90
face 262 372
use
goto 380 600 90
face 385 625
use
step 400
goto 380 585 90
face 380 608
use
goto -250 1320 90
face -250 1370
use
goto 360 395 90
face 360 380
use
goto 660 530 90
face 660 562
use
goto 660 490 90
jump 660 540
face 660 605
use
goto -250 1320 90
face -250 1370
use
state
goto 430 380 90
face 430 350
use
goto -392 452 180
face -420 452
use
step 150
walk -440 452
walk -600 452
walk -600 615
face -550 615
use
walk -860 615
face -900 615
use
goto -860 590 180 -60
face -880 590
use
goto -250 1320 90
face -250 1370
use
goto 770 500 0
face 790 500
use
goto 480 365 90
walk 480 580
walk 480 635
walk 584 650
walk 775 650
walk 775 590
walk 500 550
face 500 590
use
dial 731
use
use
goto -250 1320 90
face -250 1370
use
state
continue
PLAY
run
check "LOCKED"                                  "a locked door without the key"
check "SPARE KEY LIES UNDER"                    "the spare key under the flowerpot"
check "YOU UNLOCK AND OPEN THE DOOR"            "the shed door opens with the key"
check "A CRANK HANDLE AND A SCREWDRIVER"        "the toolbox"
check "THE BUCKET COMES UP WITH AN ANTENNA"     "the well (the antenna)"
check "YOU BUILD IN 1 PART(S) - 1 OF 8 DONE"    "the first part built in"
check "THE LAST SCREW COMES OUT"                "the mirror (the hull plate)"
check "YOU PUT DOWN THE SCREWDRIVER"            "putting something down"
check "THE OVEN HAS COOLED DOWN"                "the oven (the flux coil)"
check "THE GLORP EATS THE GREEN JELLY"          "the glorp (the thruster nozzle)"
check "THE ICE HAS MELTED"                      "the ice (the star compass)"
check "YOU PUT THE CHAIR UNDER THE CUPBOARD"    "the chair"
check "# jump ended at 6[56][0-9],5[34][0-9] z=30 sector=51"  "a jump from the floor gets onto the chair"
check "YOU TAKE THE WARP CRYSTAL"               "the crystal on the cupboard"
check "THE WASHING MACHINE STOPS"               "the fuse box"
check "YOU TAKE THE FUEL CELL"                  "the washing machine (the fuel cell)"
check "SCRATCHED INTO THE BACK WITH A PIN: 7 3 1" "the photo (the code)"
check "CLICK! THE SAFE OPENS"                   "the safe (the ignition glyph)"
check "YOU BUILD IN 1 PART(S) - 8 OF 8 DONE"    "all eight parts built in"
check "^# ENDING"                               "the ending"
check "# state built=8"                         "eight parts"
check "# no saved game"                         "the saved game is deleted at the end"
nocheck "^Error"                                "a run time error"
nocheck "failed at"                             "a walk that got stuck"
# 2. the carrying limit
cat > $T/play <<'PLAY'
deqqoff
goto 235 395 90
face 225 345
use
use
face 262 372
use
goto 360 395 90
face 360 380
use
goto 245 395 90
face 262 372
use
state
PLAY
run
check "CAN CARRY THREE PLACES"                  "the carrying limit (a chair takes two places)"
# 3. the saved game: two parts built in, then "continue"
cat > $T/play <<'PLAY'
deqqoff
give 9
goto -250 1320 90
face -250 1370
use
give 7
use
continue
state
new
state
PLAY
run
check "# continued with mask 160"               "the saved game after two parts"
check "# state built=2"                         "two parts are built in after loading the saved game"
check "# state built=0"                         "a new game starts with nothing built"
# 4. Deqq: his round, being caught, losing your track, hiding
cat > $T/play <<'PLAY'
goto -250 1320 90
deqqon
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
step 150
state
goto 425 540 90
give 10
deqq 425 440
step 240
state
goto 425 540 90
deqq 425 420
step 40
goto -700 500 0 -60
step 400
state
goto 425 705 90
deqq 480 620
step 60
say hidden
goto 480 640 90
step 60
state
goto 425 560 90
deqq 425 470
step 10
state
PLAY
run
check "sec 22"                                  "Deqq's round leads him to the bedroom"
check "sec 23"                                  "Deqq's round leads him to the bathroom"
check "sec 11"                                  "Deqq's round leads him through the hall"
check "DEQQ HAS CAUGHT YOU"                     "being caught"
check "DEQQ MUTTERS, LOSES YOUR TRACK"          "losing Deqq's track"
check "caught=1 dqs"                            "caught once after the first chase"
count "HAS SEEN YOU" 4                          "the alarms (caught, lost track, after hiding, the last chase) but none while hidden"
check "THE PARTS BUILT IN STAY"                 "what you carried is put back"
nocheck "^Error"                                "a run time error"
# 5. the town: Deqq throws you out of the garden (and locks the gate for a while), his walk through the town and home again
cat > $T/play <<'PLAY'
goto 480 120 90
deqqon
deqq 480 60
step 60
state
step 1700
state
PLAY
run
check "sector=418"                              "put down on the pavement outside the gate"
check "THE GATE CLICKS: IT IS UNLOCKED AGAIN"   "the gate is unlocked after a while"
cat > $T/play <<'PLAY'
goto -250 1320 90
deqqaway 66
step 1500
state
deqqleave
step 600
state
step 600
state
step 600
state
step 600
state
step 600
state
step 600
state
step 600
state
step 600
state
step 600
state
step 600
state
step 600
state
step 600
state
step 600
state
step 600
state
PLAY
run
check "THE GATE CREAKS: DEQQ IS COMING HOME"    "the warning at the gate"
check "away=0 wp="                              "Deqq comes home from the town"
check "away=-1 wp=[5-7][0-9]"                       "Deqq leaves the house again and walks through the town"
nocheck "^Error"                                "a run time error"
# 6. the town is open to the player: out through the gate, over the pavement (the ghost lawn's railing must not block it), across the road, along the strip beside it into the east fields
cat > $T/play <<'PLAY'
deqqoff
goto 480 120 90
walk 480 -30
walk 480 -100
state
walk 1950 -240
walk 1950 -600
walk 3000 -600
walk 3000 1000
state
PLAY
run
check "sector=40[0-9]"                          "standing on the road after leaving the premises"
check "sector=42[2-5]"                          "walking in the open country outside the road"
nocheck "failed at"                             "a walk outside that got stuck"
if [ $fail -ne 0 ]; then echo "$fail failure(s)"; fi
exit $fail
