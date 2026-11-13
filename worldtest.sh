#!/bin/sh
# Test of the world loader (W3D WORLD, w3dworld.c, docs/world-model.md): the world document demos/deqq2.world.json (written by demos/levels/deqq2.bas) must build
# the same world as the DATA lines of that generator (what demos/deqq2.bas loads): sectors, walls, flags and things are compared as map files (w3dmap.lib),
# and the document the generator writes must be the one in demos/. Then the error messages of the loader. Exit status = number of failures.
cd "$(dirname "$0")" || exit 1
T=$(mktemp -d) || exit 1
trap 'rm -rf "$T"' EXIT
fail=0
ok() { printf '%-28s ok\n' "$1"; }
bad() { echo "FAILED: $1"; fail=$((fail+1)); }
cp demos/lib/w3dmap.lib "$T/"
# 1. the generator writes the same document as the one in demos/
if timeout 120 ./engine1999c --allow=write demos/levels/deqq2.bas "$T/gen.json" > "$T/data.txt" 2> "$T/err"; then
    if cmp -s "$T/gen.json" demos/deqq2.world.json; then ok "document is current"; else bad "demos/deqq2.world.json is not what demos/levels/deqq2.bas writes (run it again)"; fi
else bad "the generator failed: $(head -n 2 "$T/err")"; fi
# 2. the same world through DATA lines and through the document
{
cat <<'BAS'
LOAD LIBRARY "w3dmap"
PROC LOADLEVEL
  LOCAL NS,I,S,F,C,FT,CT,L,TG,NW,J,X1,Y1,X2,Y2,T,NX,FL,UP,LO,XO,YO
  RESTORE LEVELDATA
  READ NS
  FOR I=1 TO NS
    READ S,F,C,FT,CT,L,TG,NW
    W3D SECTOR S,F,C,FT,CT,L
    W3D TAG S,TG
    IF TG=97 THEN W3D GHOST S,1
    FOR J=0 TO NW-1
      READ X1,Y1,X2,Y2,T,NX,FL,UP,LO,XO,YO
      W3D WALL S,X1,Y1,X2,Y2,T,NX
      IF UP>=0 OR LO>=0 THEN W3D WALLTEX S,J,-1,UP,LO
      IF FL<>0 THEN W3D WALLFLAG S,J,FL
      IF XO<>0 OR YO<>0 THEN W3D WALLOFF S,J,XO,YO
    NEXT J
  NEXT I
  READ S
  WHILE S>=0
    READ X1,Y1,T,NX,FL,UP
    W3D THING S,X1,Y1,T,NX,FL,W3D("SECTORZ",X1,Y1,1)
    IF UP<>0 THEN W3D THINGZ S,UP
    READ S
  WEND
END PROC
CALL LOADLEVEL
CALL WM_SAVE("a.map")
W3D CLEAR
JSON LOAD "deqq2.world.json"
W3D WORLD
CALL WM_SAVE("b.map")
PRINT "SECTORS ";W3D("SECTORS")
BYE 0
LABEL LEVELDATA
BAS
cat "$T/data.txt"
} | awk '{ printf "%d %s\n", NR*10, $0 }' > "$T/cmp.bas"
mkdir "$T/worlds" && cp demos/deqq2.world.json "$T/worlds/"      # JSON LOAD finds it in worlds/ (the well-known folders)
if ( cd "$T" && timeout 300 "$OLDPWD/engine1999c" cmp.bas > out 2> err ); then
    if cmp -s "$T/a.map" "$T/b.map"; then ok "same world as the DATA lines ($(grep -c '^SECTOR' "$T/a.map") sectors, $(grep -c '^WALL' "$T/a.map") walls, $(grep -c '^THING' "$T/a.map") things)"; else bad "the world differs: $(diff "$T/a.map" "$T/b.map" | head -n 6 | tr '\n' ' ')"; fi
else bad "the comparison program failed: $(head -n 3 "$T/err" "$T/out")"; fi
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
