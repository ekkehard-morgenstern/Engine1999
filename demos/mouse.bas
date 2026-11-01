1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' MOUSE: a small paint program. Left button draws, right button erases, the wheel or a click on the color bar chooses the pen. (Window version only.)
20 GFX RESET:CLS
30 MOUSE TIMEOUT 0
40 PRINT "MOUSE  left: draw  right: erase  wheel or bar: pen  C: clear  Q: quit"
50 PEN=2:LX=-1:LY=-1
60 GOSUB 1000
65 ON MOUSE GOSUB 3000
70 REPEAT
80   X=MOUSE("X"):Y=MOUSE("Y"):B=MOUSE("B")
90   IF Y>=280 THEN MOUSE SHAPE HAND ELSE IF Y<24 THEN MOUSE SHAPE ARROW ELSE MOUSE SHAPE CROSS
160  IF Y>=24 THEN IF Y<280 THEN GOSUB 2000
170  IF Y<24 OR Y>=280 THEN LX=-1
180  A$=INKEY$
190  IF A$="c" OR A$="C" THEN GFX CLEAR:GOSUB 1000
200  WAIT 1
210 UNTIL A$="q" OR A$="Q"
220 ON MOUSE OFF:MOUSE TIMEOUT 3
230 GFX RESET:CLS
240 END
1000 ' the color bar with the chosen pen marked
1010 FOR I=1 TO 16
1020   GFX LINE (I-1)*40,280,I*40-1,299,I,BF
1030   IF I=PEN THEN GFX LINE (I-1)*40+14,284,(I-1)*40+25,295,0,BF
1040 NEXT
1050 RETURN
2000 ' drawing and erasing on the canvas
2010 IF B AND 1 THEN GOTO 2100
2020 IF B AND 2 THEN GFX LINE X-4,Y-4,X+4,Y+4,0,BF
2030 LX=-1
2040 RETURN
2100 IF LX<0 THEN GFX PSET X,Y,PEN ELSE GFX LINE LX,LY,X,Y,PEN
2110 LX=X:LY=Y
2120 RETURN
3000 ' ON MOUSE: a button was pressed or let go, or the wheel turned (it is not called for motion)
3010 P=MOUSE("PRESSED"):W=MOUSE("WHEEL"):R=MOUSE("RELEASED")
3020 IF W<>0 THEN PEN=PEN+W:IF PEN<1 THEN PEN=16
3030 IF W<>0 THEN IF PEN>16 THEN PEN=1
3040 IF P AND 1 THEN IF MOUSE("Y")>=280 THEN PEN=MOUSE("X")\40+1
3050 IF W<>0 OR P<>0 THEN GOSUB 1000
3060 RETURN
