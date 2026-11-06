1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
6 ' A tool of Engine1999: start it with TOOL "fonted" (or TOOL "fonted","file" to open a font file) from the prompt or from a program;
7 ' the program you are working on stays as it is. It can also be run as a program of its own (LOAD "tools/fonted", RUN; ARG$(1) is read the same way).
10 ' FONT EDITOR - edits the fixed-width font of the text screen (256 characters of 8 x 12 pixels), the characters that SYMBOL redefines.
20 ' Every stroke is sent to the engine at once with SYMBOL, so the program you stopped (or the text on the screen behind the tool) shows the new
30 ' character when the tool ends; the font is NOT reset when the tool ends. Needs the window (engine1999); written on the UI subsystem.
40 '   The table on the left shows all 256 characters at their real size: click one to edit it. The big grid is the character: the left button
50 '   sets a pixel, the right button clears it. The slider (or the < and > buttons) walks through the codes. Clear, Invert, Flip H / V and the four
60 '   shift buttons change the whole character; Copy / Paste move a character to another code; Built-in brings back the character of the built-in
70 '   font, All built-in the whole font; Undo takes back the last 30 changes. Save writes the changed characters (or all with the check box) as a BASIC
80 '   listing  DESIGN CHAR n  +  12 rows  @ "........"  that you can MERGE into a program or LOAD / RUN to get the font back; Open reads such a file.
90 ' widget ids: 99 form, 100 table, 101 big grid, 102 preview, 104 code slider, 105 code text, 106 / 107 < >, 110..122 buttons, 130 check box, 140 status
100 LOAD LIBRARY "filereq"
110 LOAD LIBRARY "uikit"
120 REM the graphics of the calling program stay (no GFX RESET)
130 CLS
140 BREAK OFF
150 DIM UC(32)
160 DIM UG$(32)
170 DIM SP(6)
180 UN=0
190 CC=65
200 CG$=""
210 CP$=""
220 PNT=0
230 Q=0
240 DR$=""
250 ' ====================================================================== functions (they must stand before the lines that use them)
260 ' the 12 bytes of character C in the font now
270 FUNC GETG$(C)
280   LOCAL R,G$
290   G$=""
300   FOR R=0 TO 11
310     G$=G$+CHR$(TXT("GLYPH",C,R))
320   NEXT R
330   RETURN G$
340 END FUNC
350 ' the 12 bytes of character C in the built-in font
360 FUNC BUILT$(C)
370   LOCAL R,G$
380   G$=""
390   FOR R=0 TO 11
400     G$=G$+CHR$(TXT("BUILTIN",C,R))
410   NEXT R
420   RETURN G$
430 END FUNC
440 ' the 8 palette indices of a row byte: FGC where the bit is set, BGC where not (bit 7 is the left pixel)
450 FUNC ROW8$(B,FGC,BGC)
460   LOCAL X,R$
470   R$=""
480   FOR X=0 TO 7
490     IF (B SHR (7-X)) AND 1 THEN R$=R$+CHR$(FGC) ELSE R$=R$+CHR$(BGC)
500   NEXT X
510   RETURN R$
520 END FUNC
530 FUNC HEX2$(C)
540   IF C<16 THEN RETURN "0"+HEX$(C)
550   RETURN HEX$(C)
560 END FUNC
570 FUNC DESC$(C)
580   LOCAL T$
590   T$="Code "+STR$(C)+"   $"+FN HEX2$(C)
600   IF C>32 AND C<127 THEN T$=T$+"   '"+CHR$(C)+"'"
610   IF FN GETG$(C)<>FN BUILT$(C) THEN T$=T$+"   (changed)"
620   RETURN T$
630 END FUNC
640 ' a row of a file: the byte of the 8 characters after the quote at P (any character but . and blank is a set pixel)
650 FUNC ROWBYTE(L$,P)
660   LOCAL X,B,C$
670   B=0
680   FOR X=1 TO 8
690     C$=MID$(L$,P+X,1)
700     B=B*2
710     IF C$<>"." AND C$<>" " AND C$<>CHR$(34) AND C$<>"" THEN B=B+1
720   NEXT X
730   RETURN B
740 END FUNC
750 ' ====================================================================== main program
760 FOR I=1 TO 5
770   SP(I)=GFX("PALETTE",3,I)
780 NEXT I
790 UI OPEN
800 GFX PALETTE 3,1,RGB(255,255,255)
810 GFX PALETTE 3,2,RGB(40,40,70)
820 GFX PALETTE 3,4,RGB(255,220,0)
830 GFX PALETTE 3,5,RGB(20,60,150)
840 CALL MAKEFORM
850 CALL BUILDTAB
860 CALL SELCH(CC)
870 ON ERROR GOTO ERRH
880 ARGLOAD$=""
890 IF ARGC>=1 THEN ARGLOAD$=ARG$(1)
900 IF ARGLOAD$<>"" THEN CALL LOADFILE(ARGLOAD$)
910 LABEL LOOPTOP
920 ON ERROR GOTO ERRH
930 ON UI GOSUB HANDLER
940 REPEAT
950   GFX VSYNC
960   UI UPDATE
970   IF MOUSE("B")=0 THEN PNT=0
980 UNTIL Q=1
990 ON UI OFF
1000 ON ERROR OFF
1010 UI CLOSE
1020 FOR I=1 TO 5
1030   GFX PALETTE 3,I,SP(I)
1040 NEXT I
1050 REM no GFX RESET: the font now is the one you edited
1060 CLS
1070 PRINT "Font editor finished."
1080 END
1090 LABEL ERRH
1100 ON UI OFF
1110 CALL UK_MSG("Error",ERR$)
1120 RESUME LOOPTOP
1130 ' ====================================================================== the form
1140 PROC MAKEFORM
1150   UI PANEL 99,0,0,640,300,"Font editor"
1160   UI CANVAS 100,8,24,138,200,128,192
1170   UI CANVAS 101,150,24,136,204,8,12
1180   UI LABEL 108,150,232,60,14,"Preview"
1190   UI CANVAS 102,150,248,48,24,24,12
1200   UI SLIDER 104,300,24,252,18
1210   UI RANGE 104,0,255
1220   UI VALUE 104,CC
1230   UI BUTTON 106,556,22,34,22,"<"
1240   UI BUTTON 107,594,22,34,22,">"
1250   UI LABEL 105,300,50,330,14,""
1260   UI BUTTON 110,300,76,104,22,"Clear"
1270   UI BUTTON 111,408,76,104,22,"Invert"
1280   UI BUTTON 112,516,76,104,22,"Undo"
1290   UI BUTTON 113,300,102,104,22,"Flip H"
1300   UI BUTTON 114,408,102,104,22,"Flip V"
1310   UI BUTTON 115,516,102,104,22,"Built-in"
1320   UI BUTTON 116,300,128,104,22,"Left"
1330   UI BUTTON 117,408,128,104,22,"Right"
1340   UI BUTTON 118,516,128,104,22,"Up"
1350   UI BUTTON 119,300,154,104,22,"Down"
1360   UI BUTTON 120,408,154,104,22,"Copy"
1370   UI BUTTON 121,516,154,104,22,"Paste"
1380   UI BUTTON 122,300,190,104,22,"All built-in"
1390   UI BUTTON 123,408,190,104,22,"Open..."
1400   UI BUTTON 124,516,190,104,22,"Save..."
1410   UI CHECK 130,300,220,200,16,"Save all characters"
1420   UI BUTTON 125,516,250,104,22,"Quit"
1430   UI LABEL 140,10,278,620,14,"Left button sets a pixel, right button clears it. Every change goes to the engine at once."
1440 END PROC
1450 PROC STATUS(T$)
1460   UI CAPTION 140,T$
1470 END PROC
1480 ' ====================================================================== drawing
1490 ' the table: all 256 characters, 16 x 16 of 8 x 12 pixels, at their real size
1500 PROC BUILDTAB
1510   LOCAL C,R,T$
1520   T$=STRING$(128*192,2)
1530   FOR C=0 TO 255
1540     FOR R=0 TO 11
1550       MID$(T$,((C\16)*12+R)*128+(C MOD 16)*8+1,8)=FN ROW8$(TXT("GLYPH",C,R),1,2)
1560     NEXT R
1570   NEXT C
1580   UI IMAGE 100,T$
1590   CALL DRAWCELL(CC,1)
1600 END PROC
1610 ' one character in the table; SEL = 1 draws it as the selected one
1620 PROC DRAWCELL(C,SEL)
1630   LOCAL R,X,B,FGC,BGC
1640   FGC=1
1650   BGC=2
1660   IF SEL THEN
1670     FGC=4
1680     BGC=5
1690   END IF
1700   FOR R=0 TO 11
1710     B=TXT("GLYPH",C,R)
1720     FOR X=0 TO 7
1730       IF (B SHR (7-X)) AND 1 THEN UI PIXEL 100,(C MOD 16)*8+X,(C\16)*12+R,FGC ELSE UI PIXEL 100,(C MOD 16)*8+X,(C\16)*12+R,BGC
1740     NEXT X
1750   NEXT R
1760 END PROC
1770 ' the big grid and the preview from CG$
1780 PROC DRAWCAN
1790   LOCAL R,I$,P$,W$
1800   I$=""
1810   P$=""
1820   FOR R=0 TO 11
1830     W$=FN ROW8$(ASC(MID$(CG$,R+1,1)),1,2)
1840     I$=I$+W$
1850     P$=P$+W$+W$+W$
1860   NEXT R
1870   UI IMAGE 101,I$
1880   UI IMAGE 102,P$
1890 END PROC
1900 ' the character now in CG$ goes to the engine; the table, the preview and the texts follow (the big grid is already right after a stroke)
1910 PROC LIGHT
1920   LOCAL R,P$,W$
1930   SYMBOL CC,CG$
1940   CALL DRAWCELL(CC,1)
1950   P$=""
1960   FOR R=0 TO 11
1970     W$=FN ROW8$(ASC(MID$(CG$,R+1,1)),1,2)
1980     P$=P$+W$+W$+W$
1990   NEXT R
2000   UI IMAGE 102,P$
2010   UI CAPTION 105,FN DESC$(CC)
2020 END PROC
2030 PROC COMMIT
2040   CALL LIGHT
2050   CALL DRAWCAN
2060 END PROC
2070 ' character C becomes the one that is edited
2080 PROC SELCH(C)
2090   IF C<>CC THEN CALL DRAWCELL(CC,0)
2100   CC=C
2110   CG$=FN GETG$(C)
2120   CALL DRAWCELL(CC,1)
2130   CALL DRAWCAN
2140   UI CAPTION 105,FN DESC$(CC)
2150   UI VALUE 104,CC
2160 END PROC
2170 ' ====================================================================== changes
2180 ' the undo stack: the character and its 12 bytes before a change
2190 PROC PUSHU
2200   LOCAL I
2210   IF UN=32 THEN
2220     FOR I=1 TO 31
2230       UC(I-1)=UC(I)
2240       UG$(I-1)=UG$(I)
2250     NEXT I
2260     UN=31
2270   END IF
2280   UC(UN)=CC
2290   UG$(UN)=CG$
2300   UN=UN+1
2310 END PROC
2320 PROC DOUNDO
2330   LOCAL C,G$
2340   IF UN=0 THEN
2350     CALL STATUS("Nothing to undo.")
2360     EXIT PROC
2370   END IF
2380   UN=UN-1
2390   C=UC(UN)
2400   G$=UG$(UN)
2410   SYMBOL C,G$
2420   IF C=CC THEN
2430     CG$=G$
2440     CALL COMMIT
2450   ELSE
2460     CALL DRAWCELL(C,0)
2470     CALL SELCH(C)
2480   END IF
2490   CALL STATUS("Undone ("+STR$(UN)+" more).")
2500 END PROC
2510 ' a cell of the big grid: V = 1 sets, 2 clears
2520 PROC PAINT(CELL,V)
2530   LOCAL X,Y,R,B
2540   IF PNT=0 THEN CALL PUSHU
2550   PNT=1
2560   X=CELL MOD 8
2570   Y=CELL\8
2580   UI PIXEL 101,X,Y,V
2590   B=0
2600   FOR X=0 TO 7
2610     B=B*2
2620     IF UI("PIXEL",101,X,Y)=1 THEN B=B+1
2630   NEXT X
2640   MID$(CG$,Y+1,1)=CHR$(B)
2650   CALL LIGHT
2660 END PROC
2670 ' K: 1 clear, 2 invert, 3 flip H, 4 flip V, 5 left, 6 right, 7 up, 8 down
2680 PROC TRANSFORM(K)
2690   LOCAL R,X,B,N,G$
2700   CALL PUSHU
2710   G$=""
2720   FOR R=1 TO 12
2730     B=ASC(MID$(CG$,R,1))
2740     IF K=1 THEN B=0
2750     IF K=2 THEN B=255-B
2760     IF K=3 THEN
2770       N=0
2780       FOR X=0 TO 7
2790         N=N*2+((B SHR X) AND 1)
2800       NEXT X
2810       B=N
2820     END IF
2830     IF K=5 THEN B=(B*2) AND 255
2840     IF K=6 THEN B=B\2
2850     G$=G$+CHR$(B)
2860   NEXT R
2870   IF K=4 THEN
2880     G$=""
2890     FOR R=12 TO 1 STEP -1
2900       G$=G$+MID$(CG$,R,1)
2910     NEXT R
2920   END IF
2930   IF K=7 THEN G$=MID$(CG$,2,11)+CHR$(0)
2940   IF K=8 THEN G$=CHR$(0)+LEFT$(CG$,11)
2950   CG$=G$
2960   CALL COMMIT
2970 END PROC
2980 PROC DOBUILT
2990   CALL PUSHU
3000   CG$=FN BUILT$(CC)
3010   CALL COMMIT
3020   CALL STATUS("Character "+STR$(CC)+" is the built-in one again.")
3030 END PROC
3040 PROC ALLBUILTIN
3050   LOCAL C
3060   IF FN UK_ASK("All built-in","The whole font goes back to the built-in one.|The undo steps are lost. Continue?")=0 THEN EXIT PROC
3070   FOR C=0 TO 255
3080     SYMBOL C,FN BUILT$(C)
3090   NEXT C
3100   UN=0
3110   CALL BUILDTAB
3120   CALL SELCH(CC)
3130   CALL STATUS("The font is the built-in font again.")
3140 END PROC
3150 PROC COPYCHAR
3160   CP$=CG$
3170   CALL STATUS("Character "+STR$(CC)+" copied.")
3180 END PROC
3190 PROC PASTECHAR
3200   IF CP$="" THEN
3210     CALL STATUS("Nothing copied yet.")
3220     EXIT PROC
3230   END IF
3240   CALL PUSHU
3250   CG$=CP$
3260   CALL COMMIT
3270   CALL STATUS("Pasted into character "+STR$(CC)+".")
3280 END PROC
3290 ' ====================================================================== files
3300 ' a BASIC listing: for each character  DESIGN CHAR n  and 12 rows  @ "........"  (. is a clear pixel, 1 a set one)
3310 PROC SAVEFILE(F$)
3320   LOCAL C,R,X,B,N,CNT,ALLC,R$,G$
3330   ALLC=UI("GET",130)
3340   CNT=0
3350   FOR C=0 TO 255
3360     IF ALLC<>0 OR FN GETG$(C)<>FN BUILT$(C) THEN CNT=CNT+1
3370   NEXT C
3380   IF CNT=0 THEN
3390     CALL STATUS("Not saved: no character differs from the built-in font (check 'Save all characters' to save them anyway).")
3400     EXIT PROC
3410   END IF
3420   OPEN F$ FOR OUTPUT AS #1
3430   N=10
3440   PRINT #1,STR$(N)+" ' FONT "+STR$(CNT)+" characters (made with the font editor)"
3450   FOR C=0 TO 255
3460     G$=FN GETG$(C)
3470     IF ALLC<>0 OR G$<>FN BUILT$(C) THEN
3480       N=N+10
3490       PRINT #1,STR$(N)+" DESIGN CHAR "+STR$(C)
3500       FOR R=0 TO 11
3510         B=ASC(MID$(G$,R+1,1))
3520         R$=""
3530         FOR X=0 TO 7
3540           IF (B SHR (7-X)) AND 1 THEN R$=R$+"1" ELSE R$=R$+"."
3550         NEXT X
3560         N=N+10
3570         PRINT #1,STR$(N)+" @ "+CHR$(34)+R$+CHR$(34)
3580       NEXT R
3590     END IF
3600   NEXT C
3610   CLOSE #1
3620   CALL STATUS("Saved "+STR$(CNT)+" characters to "+F$+".")
3630 END PROC
3640 PROC LOADFILE(F$)
3650   LOCAL L$,N,I,P,Y,NUM,CNT,G$
3660   NUM=-1
3670   Y=12
3680   CNT=0
3690   G$=""
3700   OPEN F$ FOR INPUT AS #1
3710   WHILE EOF(1)=0
3720     LINE INPUT #1,L$
3730     N=LEN(L$)
3740     FOR I=1 TO N-10
3750       IF MID$(L$,I,11)="DESIGN CHAR" THEN
3760         NUM=VAL(MID$(L$,I+11))
3770         IF NUM<0 OR NUM>255 THEN NUM=-1
3780         Y=0
3790         G$=""
3800       END IF
3810     NEXT I
3820     P=0
3830     FOR I=1 TO N
3840       IF MID$(L$,I,1)=CHR$(34) AND P=0 THEN P=I
3850     NEXT I
3860     IF P>0 AND Y<12 AND NUM>=0 THEN
3870       G$=G$+CHR$(FN ROWBYTE(L$,P))
3880       Y=Y+1
3890       IF Y=12 THEN
3900         SYMBOL NUM,G$
3910         CNT=CNT+1
3920       END IF
3930     END IF
3940   WEND
3950   CLOSE #1
3960   UN=0
3970   CALL BUILDTAB
3980   CALL SELCH(CC)
3990   CALL STATUS("Read "+STR$(CNT)+" characters from "+F$+" (this cannot be undone).")
4000 END PROC
4010 ' ====================================================================== events
4020 LABEL HANDLER
4030 E=UI("EVENT")
4040 WHILE E<>0
4050   K=UI("KIND")
4060   V=UI("VALUE")
4070   IF E=100 AND (K=7 OR K=8) THEN CALL SELCH((V\128\12)*16+(V MOD 128)\8)
4080   IF E=101 AND K=7 THEN CALL PAINT(V,1)
4090   IF E=101 AND K=8 THEN CALL PAINT(V,2)
4100   IF E=104 AND V<>CC THEN CALL SELCH(V)
4110   IF E=106 AND K=1 THEN CALL SELCH((CC+255) MOD 256)
4120   IF E=107 AND K=1 THEN CALL SELCH((CC+1) MOD 256)
4130   IF E=110 AND K=1 THEN CALL TRANSFORM(1)
4140   IF E=111 AND K=1 THEN CALL TRANSFORM(2)
4150   IF E=112 AND K=1 THEN CALL DOUNDO
4160   IF E=113 AND K=1 THEN CALL TRANSFORM(3)
4170   IF E=114 AND K=1 THEN CALL TRANSFORM(4)
4180   IF E=115 AND K=1 THEN CALL DOBUILT
4190   IF E=116 AND K=1 THEN CALL TRANSFORM(5)
4200   IF E=117 AND K=1 THEN CALL TRANSFORM(6)
4210   IF E=118 AND K=1 THEN CALL TRANSFORM(7)
4220   IF E=119 AND K=1 THEN CALL TRANSFORM(8)
4230   IF E=120 AND K=1 THEN CALL COPYCHAR
4240   IF E=121 AND K=1 THEN CALL PASTECHAR
4250   IF E=122 AND K=1 THEN CALL ALLBUILTIN
4260   IF E=123 AND K=1 THEN
4270     F$=FN FR_PICK$("Open a font file",DR$,"*.bas",0)
4280     DR$=FR_DIR$
4290     IF F$<>"" THEN CALL LOADFILE(F$)
4300   END IF
4310   IF E=124 AND K=1 THEN
4320     F$=FN FR_PICK$("Save the font as",DR$,"*.bas",-1)
4330     DR$=FR_DIR$
4340     IF F$<>"" THEN CALL SAVEFILE(F$)
4350   END IF
4360   IF E=125 AND K=1 THEN Q=1
4370   E=UI("EVENT")
4380 WEND
4390 RETURN
