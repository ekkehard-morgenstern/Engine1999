#!/bin/sh
# Test of the library demos/lib/chunks.lib (the book of a world made of chunks, roadmap item 95), run in the console host: a program in a temporary folder with a link to
# demos/lib checks the window of 7 x 7 chunks, the order (nearest first), the slots (a chunk takes the slot of the one that left), the levels of detail (ring distance),
# the new / level-change flag, moving the player (only the new column and the level changes are stale), FORGET and the errors. Exit status = number of failures.
cd "$(dirname "$0")" || exit 1
R=$PWD
T=$(mktemp -d) || exit 1
trap 'rm -f "$T/lib" "$T/ck.bas" "$T/out"; rmdir "$T"' EXIT
ln -s "$R/demos/lib" "$T/lib"
cat > "$T/ck.bas" <<'BAS'
10 LOAD LIBRARY "chunks"
20 DIM SEEN(64),LV(64)
30 ON ERROR GOTO 900
40 CALL CK_SETUP(900,3,1,3)
50 CALL CK_UPDATE(100,100)
60 CALL T("a fresh window is all stale",FN CK_STALE()=49)
70 ' the whole window, nearest first, all new
80 N=0:LAST=-1:OKORD=-1:NNEW=0:N0=0:N1=0:OKSLOT=-1
90 WHILE FN CK_NEXT()
100   N=N+1:D=FN CK_CX()*FN CK_CX()+FN CK_CY()*FN CK_CY()
110   IF D<LAST THEN OKORD=0
120   LAST=D
130   S=FN CK_CSLOT():IF S<>FN CK_SLOT(FN CK_CX(),FN CK_CY()) THEN OKSLOT=0
140   IF SEEN(S)<>0 THEN OKSLOT=0
150   SEEN(S)=1
160   IF FN CK_CNEW() THEN NNEW=NNEW+1
170   IF FN CK_CLEVEL()=0 THEN N0=N0+1
180   IF FN CK_CLEVEL()=1 THEN N1=N1+1
190   CALL CK_DONE
200 WEND
210 CALL T("49 chunks to build",N=49)
220 CALL T("nearest first",OKORD)
230 CALL T("49 different slots, the slot is the one of CK_SLOT",OKSLOT)
240 CALL T("all new",NNEW=49)
250 CALL T("9 fine (up to 1 chunk away), 40 coarse",N0=9 AND N1=40)
260 CALL T("nothing stale afterwards",FN CK_STALE()=0)
270 CALL T("the chunks are there",FN CK_HAS(0,0) AND FN CK_HAS(-3,-3) AND FN CK_HAS(3,3))
280 CALL T("others are not",NOT FN CK_HAS(4,0) AND NOT FN CK_HAS(-4,0))
290 CALL T("levels",FN CK_LEVELAT(0,0)=0 AND FN CK_LEVELAT(1,1)=0 AND FN CK_LEVELAT(2,0)=1 AND FN CK_LEVELAT(-3,3)=1 AND FN CK_LEVELAT(9,9)=-1)
300 CALL T("the player's chunk",FN CK_PCX()=0 AND FN CK_PCY()=0)
310 ' a step east: the column cx = 4 is new, the chunks that came within one chunk are finer, the ones that left the ring get coarser
320 CALL CK_UPDATE(1000,100)
330 CALL T("the player's chunk moved",FN CK_PCX()=1)
340 CALL T("13 stale: 7 new, 3 finer, 3 coarser",FN CK_STALE()=13)
350 N=0:NNEW=0:NCH=0
360 WHILE FN CK_NEXT()
370   N=N+1:IF FN CK_CNEW() THEN NNEW=NNEW+1 ELSE NCH=NCH+1
380   CALL CK_DONE
390 WEND
400 CALL T("13 built, 7 of them new and 6 only a level change",N=13 AND NNEW=7 AND NCH=6)
410 CALL T("nothing stale",FN CK_STALE()=0)
420 CALL T("a chunk took the slot of the one that left",FN CK_HAS(4,0) AND NOT FN CK_HAS(-4,0) AND FN CK_SLOT(4,0)=FN CK_SLOT(-4,0))
430 CALL T("a chunk that left stays in its slot until the slot is needed",FN CK_HAS(-3,0))
440 CALL T("new levels: (2,0) fine, (-2,0) coarse",FN CK_LEVELAT(2,0)=0 AND FN CK_LEVELAT(-2,0)=1)
450 ' FORGET
460 CALL CK_FORGET
470 CALL T("forgotten: all stale again",FN CK_STALE()=49 AND NOT FN CK_HAS(1,0))
480 ' the window follows the player over negative numbers and far away
490 CALL CK_UPDATE(-123456,98765)
500 N=0:WHILE FN CK_NEXT():N=N+1:CALL CK_DONE:WEND
510 CALL T("far away, negative: 49 chunks",N=49 AND FN CK_STALE()=0 AND FN CK_PCX()=-138 AND FN CK_PCY()=109)
520 ' a radius 1 window
530 CALL CK_SETUP(500,1,0,1)
540 CALL CK_UPDATE(0,0)
550 N=0:WHILE FN CK_NEXT():N=N+1:CALL CK_DONE:WEND
560 CALL T("radius 1: 9 chunks",N=9)
570 CALL T("radius 1, fine 0: only the middle one is level 0",FN CK_LEVELAT(0,0)=0 AND FN CK_LEVELAT(1,0)=1 AND FN CK_LEVELAT(-1,-1)=1)
580 ' errors
590 ERRS=0
600 A=0:B=3:C=1:D=3:GOSUB 800
610 A=900:B=0:C=1:D=3:GOSUB 800
620 A=900:B=4:C=1:D=3:GOSUB 800
630 A=900:B=3:C=-1:D=3:GOSUB 800
640 A=900:B=3:C=2:D=1:GOSUB 800
650 CALL T("bad settings are errors",ERRS=5)
660 PRINT "done"
670 END
700 PROC T(N$,C)
710   IF C THEN PRINT "ok ";N$ ELSE PRINT "FAILED ";N$
720 END PROC
800 ON ERROR GOTO 900
810 CALL CK_SETUP(A,B,C,D)
820 PRINT "FAILED setup accepted ";A;B;C;D
830 RETURN
900 ERRS=ERRS+1:RESUME 830
BAS
HOME=$T ./engine1999c --allow=all "$T/ck.bas" > "$T/out" 2>&1 < /dev/null
fail=$(grep -c '^FAILED' "$T/out")
if ! grep -q '^done' "$T/out"; then echo "FAILED: the test program did not finish:"; tail -5 "$T/out"; fail=$((fail+1)); fi
grep '^FAILED' "$T/out"
[ $fail -ne 0 ] && echo "$fail failure(s)"
exit $fail
