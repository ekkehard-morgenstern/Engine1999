1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
5 DIM N(10)
10 ' PATHS - sprite paths. SPRITE n,PATH a$ gives a sprite a little program that the engine runs by itself, one step per frame, like a PLAY string: the BASIC
20 ' program does not move the sprites at all. A path is a string of commands: X+3*20 moves 3 pixels a frame for 20 frames, W 10 waits 10 frames, [ ... ]n
30 ' repeats, R starts again, E n reports an event to the program (ON SPRITE GOSUB, SPRITE("EVENT")). Needs the window (engine1999). Any key stops it.
40 GFX RESET ALL
50 CLS
60 BREAK OFF
70 GFX PALETTE 4,1,RGB(230,60,60)
80 GFX PALETTE 4,2,RGB(250,220,60)
90 GFX PALETTE 4,3,RGB(60,200,90)
100 GFX PALETTE 4,4,RGB(90,140,255)
110 FOR K=0 TO 3
120   A$=""
130   FOR Y=0 TO 23
140     FOR X=0 TO 15
150       C=0
160       IF (X-8)^2+(Y-12)^2<60 THEN C=K+1
170       A$=A$+CHR$(C)
180     NEXT X
190   NEXT Y
200   SPRITE DEF K,A$
210 NEXT K
220 PRINT "Four sprites run on paths; the program only counts the events."
230 PRINT "A: back and forth   B: a square   C: a staircase that repeats   D: waits, hides and shows"
240 FOR K=0 TO 3
250   SPRITE K,IMAGE K
260   SPRITE K,ON
270 NEXT K
280 SPRITE 0,40,120
290 SPRITE 0,PATH "[ X+4*50 E 1 X-4*50 E 2 ]100"
300 SPRITE 1,300,100
310 SPRITE 1,PATH "X+3*20 W 5 Y+3*20 W 5 X-3*20 W 5 Y-3*20 W 5 E 3 R"
320 SPRITE 2,40,250
330 SPRITE 2,PATH "[ X+8*4 Y-6*4 ]20 X=40 Y=250 R"
340 SPRITE 3,500,200
350 SPRITE 3,PATH "[ H W 30 S W 30 X-10 ]20 E 4 R"
360 ON SPRITE GOSUB COUNT
370 REPEAT
380   GFX VSYNC
390   LOCATE 0,5
400   PRINT "events: A right";N(1);" left";N(2);"  B lap";N(3);"  D";N(4);"   "
410 UNTIL INKEY$<>""
420 ON SPRITE OFF
430 FOR K=0 TO 3
440   SPRITE K,PATH ""
450   SPRITE K,OFF
460 NEXT K
470 GFX RESET ALL
480 CLS
490 END
500 LABEL COUNT
510 REPEAT
520   E=SPRITE("EVENT")
530   IF E>=0 THEN N(E)=N(E)+1
540 UNTIL E<0
550 RETURN
