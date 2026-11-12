#!/bin/sh
# Test of the map file library (demos/lib/w3dmap.lib) in the console host: a world with slopes, a portal, flags, a thing is saved, loaded
# (WM_LOAD clears the world first) and saved again; the two files must be the same, and the loaded world must answer like the original.
# Exit status = number of failures.
cd "$(dirname "$0")" || exit 1
T=$(mktemp -d) || exit 1
trap 'rm -rf "$T"' EXIT
cat > "$T/maptest.bas" <<'BAS'
10 LOAD LIBRARY "w3dmap"
20 W3D RESET
30 W3D SECTOR 0,0,128,20,30,255
40 W3D WALL 0,0,0,400,0,10
50 W3D WALL 0,400,0,400,400,12,1
60 W3D WALL 0,400,400,0,400,14
70 W3D WALL 0,0,400,0,0,15
80 W3D WALLOFF 0,1,3.5,-2
90 W3D WALLFLAG 0,1,18
100 W3D SECTOR 1,0,128,21,-1,200
110 W3D WALL 1,400,0,600,0,11
120 W3D WALL 1,600,0,600,400,11
130 W3D WALL 1,600,400,400,400,11
140 W3D WALL 1,400,400,400,0,13,0
150 W3D SLOPE 0,0,0.05,-0.025
160 W3D TAG 1,77
170 W3D THING 5,100,100,60,20,40
180 W3D THINGZ 5,3
190 W3D THINGTRANS 5,1
200 CALL WM_SAVE("mem:a")
210 N=FN WM_LOAD("mem:a")
220 CALL WM_SAVE("mem:b")
230 BAD=0
240 OPEN "mem:a" FOR INPUT AS #1:OPEN "mem:b" FOR INPUT AS #2
250 WHILE NOT EOF(1) AND NOT EOF(2)
260   LINE INPUT #1,A$
280   LINE INPUT #2,B$
290   IF A$<>B$ THEN BAD=BAD+1
300 WEND
310 IF NOT EOF(2) OR NOT EOF(1) THEN BAD=BAD+1
320 CLOSE #1:CLOSE #2
330 PRINT "sectors";N;" diffs";BAD;" floor";W3D("FLOORAT",300,100);" tag";W3D("TAG",1);" thing";W3D("TON",5);W3D("TZ",5);" walls";W3D("WALLS",0);W3D("WALLS",1)
340 PRINT "flags";W3D("WALLINFO",0,1,10);" off";W3D("WALLINFO",0,1,8);W3D("WALLINFO",0,1,9);" trans";W3D("THINGINFO",5,7)
BAS
cp demos/lib/w3dmap.lib "$T/w3dmap.lib"
out=$(cd "$T" && timeout 60 "$OLDPWD/engine1999c" --allow=all maptest.bas 2>&1)
want="sectors2 diffs0 floor7.5 tag77 thing-13 walls44
flags18 off3.5-2 trans-1"
if [ "$out" = "$want" ]; then
    echo "w3dmap     ok"
    exit 0
fi
echo "FAILED: w3dmaptest (output: $out)"
exit 1
