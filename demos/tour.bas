1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' A TOUR OF THE LANGUAGE - ten short lessons for somebody who has never used this BASIC: each one shows a few lines, then runs them.
20 ' Read this listing, too: the program is written the way the lessons say (procedures, functions, labels, comments).
30 ' Keys: any key goes on, Q ends the tour. Run it in the window: RUN (the graphics and the sound lessons need it).
40 ' (a FUNC has to be defined before the lines that call it, so this one stands at the top: it is used in lesson 4)
50 FUNC FACT(N)
60   IF N<=1 THEN RETURN 1
70   RETURN N*FN FACT(N-1)
80 END FUNC
90 ' ====================================================================== the main program: ten lessons, one after the other
100 Q$=CHR$(34)
110 QT=0
120 CALL SETUP
130 GOSUB INTRO
140 IF QT=0 THEN GOSUB L1
150 IF QT=0 THEN GOSUB L2
160 IF QT=0 THEN GOSUB L3
170 IF QT=0 THEN GOSUB L4
180 IF QT=0 THEN GOSUB L5
190 IF QT=0 THEN GOSUB L6
200 IF QT=0 THEN GOSUB L7
210 IF QT=0 THEN GOSUB L8
220 IF QT=0 THEN GOSUB L9
230 IF QT=0 THEN GOSUB L10
240 IF QT=0 THEN GOSUB OUTRO
250 GFX RESET
260 CLS
270 PRINT "That was the tour. Type LIST to read this program, or LOAD one of the games."
280 END
290 ' ====================================================================== helpers (procedures; they may stand behind the lines that call them)
300 PROC SETUP
310   GFX RESET ALL
320   CLS
330   ' colors: the text palette (layer 2) 200..208, the graphics layer (3) 1..6, the sprites (4) 1..4
340   GFX PALETTE 2,200,RGB(255,255,255):GFX PALETTE 2,201,RGB(255,220,90):GFX PALETTE 2,202,RGB(120,220,255):GFX PALETTE 2,203,RGB(255,120,110)
350   GFX PALETTE 2,204,RGB(150,255,150):GFX PALETTE 2,205,RGB(180,190,220):GFX PALETTE 2,208,RGB(10,20,50)
360   GFX PALETTE 3,1,RGB(255,255,255):GFX PALETTE 3,2,RGB(255,220,0):GFX PALETTE 3,3,RGB(255,90,70):GFX PALETTE 3,4,RGB(90,230,110)
370   GFX PALETTE 3,5,RGB(90,140,255):GFX PALETTE 3,6,RGB(120,230,255)
380   GFX PALETTE 4,1,RGB(255,220,0):GFX PALETTE 4,2,RGB(0,0,0):GFX PALETTE 4,3,RGB(255,64,64)
390   GFX BACKDROP RGB(10,20,50)
400 END PROC
410 ' a new page: the screen cleared, a title bar
420 PROC HEAD(T$)
430   CLS
440   TXT FILL 0,0,79,24,32,200,208
450   TXT FILL 0,0,79,0,32,208,201
460   COLOR 208,201:LOCATE 1,0:PRINT T$;
470   COLOR 200,208:LOCATE 0,2
480 END PROC
490 ' a line of explanation (colors 200 white, 201 yellow, 204 green) and a line of program (cyan), indented
500 PROC SAY(T$,C)
510   LOCAL P
520   COLOR C,208
530   ' long lines are broken at a blank (a procedure of its own: a good exercise to read)
540   WHILE LEN(T$)>76
550     P=76
560     WHILE P>1 AND MID$(T$,P,1)<>" "
570       P=P-1
580     WEND
590     PRINT LEFT$(T$,P-1)
600     T$=MID$(T$,P+1)
610   WEND
620   PRINT T$
630 END PROC
640 PROC CODE(T$)
650   COLOR 202,208:PRINT "     ";T$
660 END PROC
670 PROC OUT(T$)
680   COLOR 204,208:PRINT "  => ";T$
690 END PROC
700 ' wait for a key: Q ends the tour (QT = 1)
710 PROC KEYWAIT
720   LOCAL K$
730   COLOR 205,208:PRINT:PRINT "  [ any key goes on, Q ends the tour ]";
740   K$=""
750   WHILE K$=""
760     WAIT 2
770     K$=INKEY$
780   WEND
790   IF K$="q" OR K$="Q" THEN QT=1
800 END PROC
810 ' ====================================================================== the lessons: each is a labelled section that ends with RETURN
820 LABEL INTRO
830 CALL HEAD("A TOUR OF THE LANGUAGE")
840 CALL SAY("Engine1999 has its own BASIC: lines with numbers, like it was in the 1980s, but with procedures, functions, local variables, a compiler under the hood and statements for sprites and sound.",200)
850 CALL SAY("",200)
860 CALL SAY("Every lesson shows some lines (in cyan), then runs them (the result is in green).",200)
870 CALL SAY("Lines start with a number; a comment starts with an apostrophe (or REM); several statements on a line are separated by a colon. Upper and lower case do not matter.",200)
880 CALL SAY("",200)
890 CALL SAY("  1 variables   2 loops and decisions   3 arrays and DATA   4 procedures and functions",201)
900 CALL SAY("  5 errors   6 files   7 graphics and sprites   8 sound   9 timers   10 keys",201)
910 CALL KEYWAIT
920 RETURN
930 LABEL L1
940 CALL HEAD("1. VARIABLES: NUMBERS AND TEXT")
950 CALL SAY("A name holds a number; a name that ends in $ holds text. PRINT shows them.",200)
960 CALL SAY("",200)
970 CALL CODE("A=7:B=6")
980 CALL CODE("PRINT "+Q$+"A*B ="+Q$+";A*B")
990 A=7:B=6
1000 CALL OUT("A*B = "+STR$(A*B))
1010 CALL SAY("",200)
1020 CALL CODE("N$="+Q$+"Engine"+Q$+"+"+Q$+"1999"+Q$)
1030 CALL CODE("PRINT LEN(N$);LEFT$(N$,6);MID$(N$,7,4);RIGHT$(N$,2)")
1040 N$="Engine"+"1999"
1050 CALL OUT(STR$(LEN(N$))+" letters: "+LEFT$(N$,6)+" / "+MID$(N$,7,4)+" / "+RIGHT$(N$,2))
1060 CALL SAY("",200)
1070 CALL CODE("PRINT SQR(2);INT(7/2);7 MOD 3;2^10;ABS(-5)")
1080 CALL OUT(STR$(SQR(2))+"  "+STR$(INT(7/2))+"  "+STR$(7 MOD 3)+"  "+STR$(2^10)+"  "+STR$(ABS(-5)))
1090 CALL KEYWAIT
1100 RETURN
1110 LABEL L2
1120 CALL HEAD("2. LOOPS AND DECISIONS")
1130 CALL SAY("FOR repeats a number of times, WHILE and REPEAT while a condition holds. IF decides.",200)
1140 CALL SAY("",200)
1150 CALL CODE("FOR I=1 TO 12")
1160 CALL CODE("  IF I MOD 3=0 THEN PRINT "+Q$+"Fizz "+Q$+"; ELSE PRINT I;"+Q$+" "+Q$+";")
1170 CALL CODE("NEXT I")
1180 COLOR 204,208:PRINT "  => ";
1190 FOR I=1 TO 12
1200   IF I MOD 3=0 THEN PRINT "Fizz ";  ELSE PRINT I;" ";
1210 NEXT I
1220 PRINT
1230 CALL SAY("",200)
1240 CALL CODE("N=1:WHILE N<500:N=N*3:PRINT N;"+Q$+" "+Q$+";:WEND")
1250 COLOR 204,208:PRINT "  => ";
1260 N=1:WHILE N<500:N=N*3:PRINT N;" ";:WEND
1270 PRINT
1280 CALL SAY("",200)
1290 CALL CODE("REPEAT:N=N-100:UNTIL N<0       ' ends when the condition is true")
1300 REPEAT:N=N-100:UNTIL N<0
1310 CALL OUT("N ended as "+STR$(N))
1320 CALL SAY("",200)
1330 CALL SAY("EXIT leaves a loop early, ITERATE goes to the next round (see demos/orchard2.bas).",200)
1340 CALL KEYWAIT
1350 RETURN
1360 LABEL L3
1370 CALL HEAD("3. ARRAYS AND DATA")
1380 CALL SAY("An array holds many values under one name (DIM n makes n elements, 0 to n-1).",200)
1390 CALL SAY("DATA lines hold values in the program, READ fetches them one by one.",200)
1400 CALL SAY("",200)
1410 CALL CODE("DIM C$(5)")
1420 CALL CODE("RESTORE COLORS:FOR I=0 TO 4:READ C$(I):NEXT I")
1430 CALL CODE("FOR EACH X$ IN C$():PRINT X$;"+Q$+" "+Q$+";:NEXT")
1440 DIM C$(5)
1450 RESTORE COLORS
1460 FOR I=0 TO 4:READ C$(I):NEXT I
1470 COLOR 204,208:PRINT "  => ";
1480 FOR EACH X$ IN C$()
1490   PRINT X$;" ";
1500 NEXT
1510 PRINT
1520 CALL SAY("",200)
1530 CALL CODE("DIM M(3,3):FOR I=0 TO 2:FOR J=0 TO 2:M(I,J)=I*J:NEXT:NEXT")
1540 DIM M(3,3)
1550 FOR I=0 TO 2:FOR J=0 TO 2:M(I,J)=I*J:NEXT J:NEXT I
1560 CALL OUT("M(2,2) ="+STR$(M(2,2))+"    M(1,2) ="+STR$(M(1,2)))
1570 CALL SAY("",200)
1580 CALL SAY("Strings are arrays of bytes, too: PEEK and POKE reach a block of RAM (demos/climb.bas).",200)
1590 CALL KEYWAIT
1600 RETURN
1610 LABEL COLORS
1620 DATA "RED","GREEN","BLUE","CYAN","GOLD"
1630 LABEL L4
1640 CALL HEAD("4. PROCEDURES AND FUNCTIONS")
1650 CALL SAY("A PROC does something, a FUNC gives back a value; LOCAL variables belong to one call.",200)
1660 CALL SAY("Both can call themselves. (A FUNC has to be defined before the lines that call it.)",200)
1670 CALL SAY("",200)
1680 CALL CODE("FUNC FACT(N)")
1690 CALL CODE("  IF N<=1 THEN RETURN 1")
1700 CALL CODE("  RETURN N*FN FACT(N-1)")
1710 CALL CODE("END FUNC")
1720 CALL CODE("FOR I=1 TO 9:PRINT FN FACT(I);:NEXT")
1730 COLOR 204,208:PRINT "  => ";
1740 FOR I=1 TO 9
1750   PRINT FN FACT(I);" ";
1760 NEXT I
1770 PRINT
1780 CALL SAY("",200)
1790 CALL CODE("PROC STARS(N):LOCAL I")
1800 CALL CODE("  FOR I=1 TO N:PRINT "+Q$+"*"+Q$+";:NEXT:PRINT")
1810 CALL CODE("END PROC")
1820 CALL CODE("CALL STARS(20)")
1830 COLOR 204,208:PRINT "  => ";
1840 CALL STARS(20)
1850 CALL SAY("",200)
1860 CALL SAY("Parameters are copies; an array parameter (PROC P(A())) is the array itself.",200)
1870 CALL SAY("The big demos are built from procedures: demos/madrun.bas, demos/river.bas.",200)
1880 CALL KEYWAIT
1890 RETURN
1900 PROC STARS(N)
1910   LOCAL I
1920   FOR I=1 TO N
1930     PRINT "*";
1940   NEXT I
1950   PRINT
1960 END PROC
1970 LABEL L5
1980 CALL HEAD("5. WHEN SOMETHING GOES WRONG")
1990 CALL SAY("ON ERROR GOTO sends an error to a handler of your own instead of ending the program.",200)
2000 CALL SAY("The handler can look at ERR$ (the text) and ERL (the line), then RESUME NEXT goes on.",200)
2010 CALL SAY("",200)
2020 CALL CODE("ON ERROR GOTO HANDLER")
2030 CALL CODE("PRINT 10/0")
2040 CALL CODE("PRINT "+Q$+"...and the program goes on"+Q$)
2050 CALL CODE("HANDLER: PRINT ERR$;ERL:RESUME NEXT")
2060 ON ERROR GOTO HANDLER
2070 PRINT "  => ";
2080 PRINT 10/0
2090 CALL OUT("...and the program goes on")
2100 ON ERROR OFF
2110 CALL SAY("",200)
2120 CALL SAY("(An error inside a PROC cannot be resumed: keep a handler in the main program,",200)
2130 CALL SAY(" or test first: EXISTS(file$) says whether a file is there without an error.)",200)
2140 CALL KEYWAIT
2150 RETURN
2160 LABEL HANDLER
2170 COLOR 203,208:PRINT "caught: ";ERR$;" in line ";ERL
2180 RESUME NEXT
2190 LABEL L6
2200 CALL HEAD("6. FILES")
2210 CALL SAY("OPEN gives a file a channel number; PRINT # writes, LINE INPUT # reads, EOF tells the end.",200)
2220 CALL SAY("A name may start with a driver: mem: is a RAM disk, file: a real file, sh: a shell command, tcp: a network connection, res: read-only resources. This lesson uses mem:.",200)
2230 CALL SAY("",200)
2240 CALL CODE("OPEN "+Q$+"mem:tour"+Q$+" FOR OUTPUT AS #1")
2250 CALL CODE("FOR I=1 TO 3:PRINT #1,"+Q$+"line "+Q$+";I:NEXT:CLOSE #1")
2260 CALL CODE("OPEN "+Q$+"mem:tour"+Q$+" FOR INPUT AS #1")
2270 CALL CODE("WHILE NOT EOF(1):LINE INPUT #1,L$:PRINT L$:WEND:CLOSE #1")
2280 OPEN "mem:tour" FOR OUTPUT AS #1
2290 FOR I=1 TO 3:PRINT #1,"line ";I:NEXT I
2300 CLOSE #1
2310 OPEN "mem:tour" FOR INPUT AS #1
2320 WHILE NOT EOF(1)
2330   LINE INPUT #1,L$
2340   CALL OUT(L$)
2350 WEND
2360 CLOSE #1
2370 CALL SAY("",200)
2380 CALL CODE("PRINT EXISTS("+Q$+"mem:tour"+Q$+");EXISTS("+Q$+"mem:nothing"+Q$+")")
2390 CALL OUT(STR$(EXISTS("mem:tour"))+"  "+STR$(EXISTS("mem:nothing"))+"      (-1 = yes, 0 = no)")
2400 CALL SAY("",200)
2410 CALL SAY("SAVE and LOAD, MERGE and CHAIN work on programs, KILL and FILES on files.",200)
2420 CALL KEYWAIT
2430 RETURN
2440 LABEL L7
2450 CALL HEAD("7. GRAPHICS AND SPRITES")
2460 CALL SAY("The graphics layer is a 640 x 300 picture with its own palette: GFX LINE, CIRCLE, PAINT.",200)
2470 CALL SAY("A sprite is a small picture (16 x 24) drawn by DESIGN SPRITE with rows of numbers and moved by the engine itself: SPRITE n,MOVETO x,y,ticks.",200)
2480 CALL SAY("",200)
2490 CALL CODE("GFX LINE 40,215,200,255,2,BF      ' a filled box")
2500 CALL CODE("GFX CIRCLE 300,240,40,3           ' a circle, then GFX PAINT 300,240,4,3")
2510 CALL CODE("DESIGN SPRITE 0 ... SPRITE 0,MOVETO 580,235,120")
2520 GFX LAYER 3,ON
2530 GFX LINE 40,215,200,255,2,BF
2540 GFX LINE 40,260,200,266,5,BF
2550 GFX CIRCLE 300,240,40,3
2560 GFX PAINT 300,240,4,3
2570 GFX CIRCLE 288,232,6,1:GFX PAINT 288,232,1,1
2580 GFX CIRCLE 312,232,6,1:GFX PAINT 312,232,1,1
2590 GFX LINE 285,250,315,250,1
2600 DESIGN SPRITE 0
2610 @ "....11111111...."
2620 @ "...1111111111..."
2630 @ "..111111111111.."
2640 @ ".11111111111111."
2650 @ ".11122111122111."
2660 @ "1111221111221111"
2670 @ "1111221111221111"
2680 @ "1111111111111111"
2690 @ "1111111111111111"
2700 @ "1113111111113111"
2710 @ "1111311111131111"
2720 @ ".11113333331111."
2730 @ ".11111111111111."
2740 @ "..111111111111.."
2750 @ "...1111111111..."
2760 @ "....11111111...."
2770 FOR R=17 TO 24:@ "................":NEXT R
2780 SPRITE 0,ON:SPRITE 0,380,235:SPRITE 0,PRIORITY 1
2790 SPRITE 0,MOVETO 580,235,120
2800 CALL SAY("",200)
2810 CALL SAY("Tiles make a scrolling world (TILE SCROLL: demos/river.bas), a sprite can have animations and collisions, the text screen is a layer, too (TXT, LOCATE, COLOR).",200)
2820 CALL KEYWAIT
2830 SPRITE 0,OFF
2840 GFX CLEAR
2850 RETURN
2860 LABEL L8
2870 CALL HEAD("8. SOUND")
2880 CALL SAY("PLAY takes music strings, one for each voice: notes A to G, O octave, T tempo, L length, < and > change the octave, R is a rest. SOUND plays one tone, SFX keeps a sound effect.",200)
2890 CALL SAY("",200)
2900 CALL CODE("PLAY "+Q$+"T140 O4 E8 G8 >C8 E4 D8 C8 <B8 >C4"+Q$+","+Q$+"T140 W2 V0.25 O3 C4 G4 C4 G4"+Q$)
2910 PLAY "T140 W1 V0.3 O4 E8 G8 >C8 E4 D8 C8 <B8 >C4","T140 W2 V0.25 O3 C4 G4 C4 G4"
2920 CALL OUT("playing... (two voices on two channels)")
2930 WHILE PLAYING(1)
2940   WAIT 5
2950 WEND
2960 CALL SAY("",200)
2970 CALL CODE("SFX 1,"+Q$+"W3 V0.5 X0,0.02,0.4,0.5 S900,60,2"+Q$)
2980 CALL CODE("SFX 1                     ' an explosion")
2990 SFX 1,"W3 V0.5 X0,0.02,0.4,0.5 S900,60,2"
3000 SFX 1
3010 WAIT 70
3020 CALL SAY("",200)
3030 CALL SAY("Waves, envelopes, vibrato, tremolo, glides: demos/sfx.bas is a sound board, demos/madrun.bas and demos/river.bas make their music while they run.",200)
3040 CALL KEYWAIT
3050 SOUND OFF
3060 RETURN
3070 LABEL L9
3080 CALL HEAD("9. TIMERS")
3090 CALL SAY("EVERY runs a subroutine again and again in the background, AFTER once; the unit is 1/60 s. WAIT sleeps without using the processor; TI counts the time.",200)
3100 CALL SAY("",200)
3110 CALL CODE("EVERY 30,0 GOSUB TICK       ' twice a second")
3120 CALL CODE("TICK: CNT=CNT+1:PRINT CNT:RETURN")
3130 CALL CODE("WAIT 150:EVERY OFF 0")
3140 CNT=0
3150 COLOR 204,208:PRINT "  => ";
3160 EVERY 30,0 GOSUB TICK
3170 WAIT 150
3180 EVERY OFF 0
3190 PRINT
3200 CALL OUT("the main program only waited, "+STR$(CNT)+" ticks came by themselves")
3210 CALL SAY("",200)
3220 CALL SAY("The big games use timers for their music and for events (demos/climb.bas, orchard2.bas).",200)
3230 CALL KEYWAIT
3240 RETURN
3250 LABEL TICK
3260 CNT=CNT+1
3270 COLOR 204,208:PRINT CNT;" ";
3280 RETURN
3290 LABEL L10
3300 CALL HEAD("10. KEYS AND CONTROLLERS")
3310 CALL SAY("INKEY$ gives the key typed, KEY(name$) tells whether a key is held down right now (any number at once), JOY(name$) does the same for a game controller.",200)
3320 CALL SAY("",200)
3330 CALL CODE("PRINT KEY("+Q$+"SPACE"+Q$+");KEY("+Q$+"LEFT"+Q$+");JOY("+Q$+"A"+Q$+");JOY("+Q$+"LX"+Q$+")")
3340 CALL SAY("",200)
3350 CALL SAY("Hold SPACE, the arrow keys or a button for a few seconds (-1 = held, 0 = not):",204)
3360 FOR I=1 TO 180
3370   LOCATE 4,14
3380   COLOR 201,208
3390   PRINT "SPACE ";KEY("SPACE");"   LEFT ";KEY("LEFT");"   RIGHT ";KEY("RIGHT");"   pad A ";JOY("A");"   stick ";JOY("LX");"     ";
3400   WAIT 2
3410 NEXT I
3420 LOCATE 0,16
3430 CALL SAY("",200)
3440 CALL SAY("demos/keys.bas and demos/joy.bas show every key and every button of a controller.",200)
3450 CALL KEYWAIT
3460 RETURN
3470 LABEL OUTRO
3480 CALL HEAD("WHERE TO GO FROM HERE")
3490 CALL SAY("Small demos, one idea each:",201)
3500 CALL SAY("   demos/gfx.bas  sfx.bas  ode.bas  prelude.bas",200)
3510 CALL SAY("   demos/mouse.bas  keys.bas  joy.bas",200)
3520 CALL SAY("Old style, line numbers and GOSUB (it runs in the console, too):",201)
3530 CALL SAY("   demos/wumpus.bas  (Hunt the Wumpus)",200)
3540 CALL SAY("Games made of procedures and libraries:",201)
3550 CALL SAY("   demos/orchard2.bas  climb.bas  madrun.bas  river.bas   and demos/lib/*.lib",200)
3560 CALL SAY("",200)
3570 CALL SAY("demos/README.md tells what each demo shows and in which line to look.",204)
3580 CALL SAY("docs/basic-language.md is the manual, docs/grammar.html the whole syntax on one page.",204)
3590 CALL SAY("Edit this program: change a line, RUN again. LIST, EDIT and SAVE are your friends.",204)
3600 CALL KEYWAIT
3610 RETURN
