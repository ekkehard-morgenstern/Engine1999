#!/bin/sh
# Test of the world loader (W3D WORLD, w3dworld.c, docs/world-model.md): the world document demos/deqq2.world.json (the source of Deqq's House II) builds, every
# room, solid and thing has a name, and the built world passes worldlint.awk (no wall of zero depth with a face on each side, no portal that leads nowhere). Then the error messages of the loader.
# Exit status = number of failures.
cd "$(dirname "$0")" || exit 1
T=$(mktemp -d) || exit 1
trap 'rm -rf "$T"' EXIT
fail=0
ok() { printf '%-28s ok\n' "$1"; }
bad() { echo "FAILED: $1"; fail=$((fail+1)); }
cp demos/lib/w3dmap.lib "$T/"
# 1. the document builds a world; the sanity check (worldlint.awk): no wall of zero depth with a face on each side; sizes of the world
mkdir "$T/worlds" && cp demos/deqq2.world.json "$T/worlds/"      # JSON LOAD finds it in worlds/ (the well-known folders)
cat > "$T/w.bas" <<'BAS'
10 LOAD LIBRARY "w3dmap"
20 JSON LOAD "deqq2.world.json"
30 W3D WORLD
40 CALL WM_SAVE("a.map")
50 REM every room, solid and thing has a name
60 FOR K=0 TO 2
75   IF K=0 THEN P$="rooms"
76   IF K=1 THEN P$="solids"
77   IF K=2 THEN P$="things"
80   JSON COUNT P$,C$
90   FOR I=0 TO VAL(C$)-1
100     JSON GET P$+"."+STR$(I)+".name",V$
110     IF V$="" THEN PRINT "UNNAMED ";P$;" ";I
120   NEXT I
130 NEXT K
140 BYE 0
BAS
if ( cd "$T" && timeout 300 "$OLDPWD/engine1999c" w.bas > out 2> err ); then
    if grep -q UNNAMED "$T/out"; then bad "unnamed elements: $(grep UNNAMED "$T/out" | head -n 5 | tr '\n' ' ')"; else ok "all rooms, solids and things have names"; fi
    if awk -f worldlint.awk "$T/a.map" > "$T/lint.txt"; then ok "no zero-depth wall, no lost portal ($(grep -c '^SECTOR' "$T/a.map") sectors, $(grep -c '^WALL' "$T/a.map") walls, $(grep -c '^THING' "$T/a.map") things)"; else bad "world faults: $(head -n 5 "$T/lint.txt" | tr '\n' ';')"; fi
else bad "the world program failed: $(head -n 3 "$T/err" "$T/out")"; fi
# 2. the check finds what it should: two rooms side by side with a solid wall between them (a zero-depth partition with a face on each side)
printf 'W3DMAP,1\nSECTOR,1,0,100,1,1,255,0\nSECTOR,2,0,100,1,1,255,0\nWALL,1,10,0,10,10,2,-1,-1,-1,0,0,0\nWALL,2,10,10,10,0,3,-1,-1,-1,0,0,0\n' > "$T/z.map"
if awk -f worldlint.awk "$T/z.map" > /dev/null; then bad "worldlint.awk missed a zero-depth partition"; else ok "worldlint finds a zero-depth partition"; fi
# ... and a portal that leads nowhere: room 1's east wall opens into sector 2, whose walls are somewhere else (a ray would be lost there)
printf 'W3DMAP,1\nSECTOR,1,0,100,1,1,255,0\nSECTOR,2,0,100,1,1,255,0\nWALL,1,0,0,10,0,2,-1,-1,-1,0,0,0\nWALL,1,10,0,10,10,2,2,-1,-1,0,0,0\nWALL,1,10,10,0,10,2,-1,-1,-1,0,0,0\nWALL,1,0,10,0,0,2,-1,-1,-1,0,0,0\nWALL,2,50,0,60,0,2,-1,-1,-1,0,0,0\nWALL,2,60,0,60,10,2,-1,-1,-1,0,0,0\nWALL,2,60,10,50,10,2,-1,-1,-1,0,0,0\nWALL,2,50,10,50,0,2,-1,-1,-1,0,0,0\n' > "$T/l.map"
if awk -f worldlint.awk "$T/l.map" > "$T/l.txt"; then bad "worldlint.awk missed a portal that leads nowhere"; else grep -q 'lost portal' "$T/l.txt" && ok "worldlint finds a lost portal" || bad "worldlint: wrong message for a lost portal"; fi
# 3. the error messages
errtest() {   # name json expected-text
    echo "$2" > "$T/e.json"
    printf '10 JSON LOAD "e.json"\n20 ON ERROR GOTO 100\n30 W3D WORLD\n40 PRINT "no error":BYE 0\n100 PRINT ERR$:BYE 0\n' > "$T/e.bas"
    out=$( cd "$T" && timeout 60 "$OLDPWD/engine1999c" e.bas 2>&1 )
    case "$out" in *"$3"*) ok "error: $1";; *) bad "error $1: expected '$3', got '$out'";; esac
}
errtest "not a world" '{"format":"x"}' 'not a world document'
errtest "bad box" '{"format":"e99-world","rooms":[{"id":1,"box":[5,5,1,1],"floor":{"z":0},"ceiling":{"z":10}}]}' 'x1 < x2'
errtest "no floor" '{"format":"e99-world","rooms":[{"id":1,"box":[0,0,9,9]}]}' '"floor" and "ceiling"'
errtest "unknown room" '{"format":"e99-world","rooms":[{"id":1,"box":[0,0,9,9],"floor":{"z":0},"ceiling":{"z":10}}],"openings":[{"from":1,"to":2,"side":"N","range":[0,9]}]}' 'no such room 2'
errtest "rooms do not touch" '{"format":"e99-world","rooms":[{"id":1,"box":[0,0,9,9],"floor":{"z":0},"ceiling":{"z":10}},{"id":2,"box":[0,20,9,29],"floor":{"z":0},"ceiling":{"z":10}}],"openings":[{"from":1,"to":2,"side":"N","range":[0,9]}]}' 'do not touch'
errtest "outline" '{"format":"e99-world","rooms":[{"id":1,"outline":[[0,0],[1,0],[0,1]],"floor":{"z":0},"ceiling":{"z":10}}]}' 'not supported yet'
errtest "a small world" '{"format":"e99-world","rooms":[{"id":1,"box":[0,0,9,9],"floor":{"z":0,"tex":1},"ceiling":{"z":10}},{"id":2,"box":[0,9,9,19],"floor":{"z":0,"tex":1},"ceiling":{"z":10}}],"openings":[{"from":1,"to":2,"side":"N","range":[2,6],"flags":["block"]}]}' 'no error'
exit $fail
