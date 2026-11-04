1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' SPRITE AND TILE EDITOR - edits the 16 x 24 pictures of the sprites and the tiles with the mouse, on the UI subsystem (UI CANVAS, ON UI, the file
20 ' requester of the library filereq). Needs the window (engine1999). ESC stops the program, as always.
30 '   Left button: paint with the current color (the Pen) or fill an area (the Fill tool); right button: erase (color 0, transparent).
40 '   Colors: the 16 colors of a palette window (the slider "Window" moves it over the 224 colors of the layer: sprites use the palette of the sprite
50 '   layer, tiles that of the tile layer); a click on a color picks it. Cell 0 of a window is transparent, cell n is color base + n, as in
60 '   DESIGN SPRITE n, base.
70 '   Image: the number of the picture (0..254; sprite picture 255 is the scratch picture of the preview). Revert loads the picture from the
80 '   engine again, Store puts the edited picture into the engine (SPRITE DEF / TILE DEF), so that your program sees it at once.
90 '   Open... / Save...: a picture as a DESIGN listing (10 DESIGN SPRITE n, base, then 24 rows @ "...") that you can MERGE or LOAD into a
100 '   program; Open reads such a file (DESIGN SPRITE / DESIGN TILE, the rows of @ statements). A picture can be saved only when its colors
110 '   fit into one window of 15 colors. The preview shows the picture as a real sprite (a tile uses the sprite palette there).
120 LOAD LIBRARY "filereq"
130 GFX RESET ALL
140 CLS
150 DIM SX(400)
160 DIM SY(400)
170 MD=0
180 IMG=1
190 WB=0
200 COL=1
210 TOOL=0
220 DT=0
230 Q=0
240 DR$=""
250 UI OPEN
260 CALL MAKEFORM
270 CALL PALCOPY
280 CALL SHOWWIN
290 CALL SETCOL(1)
300 CALL LOADIMG
310 ON ERROR GOTO ERRH
320 LABEL LOOPTOP
330 ON ERROR GOTO ERRH
340 ON UI GOSUB HANDLER
350 REPEAT
360   GFX VSYNC
370   UI UPDATE
380 UNTIL Q=1
390 ON UI OFF
400 ON ERROR OFF
410 SPRITE 247,OFF
420 UI CLOSE
430 GFX RESET
440 CLS
450 PRINT "Sprite and tile editor finished."
460 END
470 LABEL ERRH
480 UI CAPTION 112,"Error: "+ERR$
490 RESUME LOOPTOP
500 ' ====================================================================== the form
510 PROC MAKEFORM
520   UI PANEL 99,0,0,640,300,"Sprite and tile editor"
530   UI CANVAS 100,10,24,164,244,16,24
540   UI LABEL 122,200,24,116,14,"Colors"
550   UI CANVAS 101,200,40,116,116,4,4
560   UI LABEL 123,200,160,116,14,"Window"
570   UI SLIDER 102,200,174,116,18
580   UI RANGE 102,0,13
590   UI CANVAS 103,200,198,48,30,1,1
600   UI LABEL 104,254,206,70,14,"Color 0"
610   UI RADIO 105,330,28,90,16,"Sprite",1
620   UI RADIO 106,430,28,90,16,"Tile",1
630   UI VALUE 105,1
640   UI LABEL 107,330,54,45,14,"Image"
650   UI SLIDER 108,378,50,200,18
660   UI RANGE 108,0,254
670   UI VALUE 108,1
680   UI LABEL 109,586,54,40,14,"1"
690   UI RADIO 110,330,80,90,16,"Pen",2
700   UI RADIO 111,430,80,90,16,"Fill",2
710   UI VALUE 110,1
720   UI LABEL 113,330,106,60,14,"Preview"
730   UI BUTTON 114,330,196,90,22,"Clear"
740   UI BUTTON 115,430,196,90,22,"Flip H"
750   UI BUTTON 116,530,196,90,22,"Flip V"
760   UI BUTTON 117,330,224,90,22,"Revert"
770   UI BUTTON 118,430,224,90,22,"Store"
780   UI BUTTON 119,330,252,90,22,"Open..."
790   UI BUTTON 120,430,252,90,22,"Save..."
800   UI BUTTON 121,530,252,90,22,"Quit"
810   UI LABEL 112,10,276,620,14,"Left button paints, right button erases. Store puts the picture into the engine."
820 END PROC
830 PROC STATUS(T$)
840   UI CAPTION 112,T$
850 END PROC
860 ' ====================================================================== the colors
870 ' the palette of the layer that holds the pictures (sprite layer 4, tile layer 1) is copied into the graphics layer (3) that the UI draws on
880 PROC PALCOPY
890   LOCAL I,L
900   L=4
910   IF MD=1 THEN L=1
920   FOR I=1 TO 223
930     GFX PALETTE 3,I,GFX("PALETTE",L,I)
940   NEXT I
950 END PROC
960 PROC SHOWWIN
970   LOCAL I
980   FOR I=0 TO 15
990     IF I=0 THEN UI PIXEL 101,0,0,0 ELSE UI PIXEL 101,I MOD 4,I\4,WB+I
1000   NEXT I
1010   UI CAPTION 123,"Window "+STR$(WB)+" .. "+STR$(WB+15)
1020 END PROC
1030 PROC SETCOL(C)
1040   COL=C
1050   UI PIXEL 103,0,0,C
1060   UI CAPTION 104,"Color "+STR$(C)
1070 END PROC
1080 ' ====================================================================== the picture
1090 PROC PREVIEW
1100   LOCAL A$
1110   UI GETIMAGE 100,A$
1120   SPRITE DEF 255,A$
1130   SPRITE 247,IMAGE 255
1140   SPRITE 247,346,126
1150   SPRITE 247,ON
1160 END PROC
1170 PROC LOADIMG
1180   LOCAL A$
1190   IF MD=0 THEN SPRITE GET IMG,A$ ELSE TILE GET IMG,A$
1200   UI IMAGE 100,A$
1210   DT=0
1220   CALL PREVIEW
1230   CALL STATUS("Picture "+STR$(IMG)+" loaded from the engine ("+MID$("sprite tile  ",1+MD*7,6)+").")
1240 END PROC
1250 PROC STOREIMG
1260   LOCAL A$
1270   UI GETIMAGE 100,A$
1280   IF MD=0 THEN SPRITE DEF IMG,A$ ELSE TILE DEF IMG,A$
1290   DT=0
1300   CALL STATUS("Picture "+STR$(IMG)+" stored: your programs see it now.")
1310 END PROC
1320 PROC FLOOD(X,Y,C)
1330   LOCAL T,SP,D,NX,NY
1340   T=UI("PIXEL",100,X,Y)
1350   IF T=C THEN EXIT PROC
1360   UI PIXEL 100,X,Y,C
1370   SX(0)=X
1380   SY(0)=Y
1390   SP=1
1400   WHILE SP>0
1410     SP=SP-1
1420     X=SX(SP)
1430     Y=SY(SP)
1440     FOR D=0 TO 3
1450       NX=X-(D=0)+(D=1)
1460       NY=Y-(D=2)+(D=3)
1470       IF NX>=0 AND NX<16 AND NY>=0 AND NY<24 THEN
1480         IF UI("PIXEL",100,NX,NY)=T THEN
1490           UI PIXEL 100,NX,NY,C
1500           SX(SP)=NX
1510           SY(SP)=NY
1520           SP=SP+1
1530         END IF
1540       END IF
1550     NEXT D
1560   WEND
1570 END PROC
1580 PROC PAINT(CELL,C)
1590   LOCAL X,Y
1600   X=CELL MOD 16
1610   Y=CELL\16
1620   IF TOOL=1 THEN CALL FLOOD(X,Y,C) ELSE UI PIXEL 100,X,Y,C
1630   DT=-1
1640   CALL PREVIEW
1650 END PROC
1660 PROC TRANSFORM(K)
1670   LOCAL A$,B$,X,Y
1680   UI GETIMAGE 100,A$
1690   B$=""
1700   FOR Y=0 TO 23
1710     FOR X=0 TO 15
1720       IF K=1 THEN B$=B$+MID$(A$,Y*16+15-X+1,1)
1730       IF K=2 THEN B$=B$+MID$(A$,(23-Y)*16+X+1,1)
1740       IF K=3 THEN B$=B$+CHR$(0)
1750     NEXT X
1760   NEXT Y
1770   UI IMAGE 100,B$
1780   DT=-1
1790   CALL PREVIEW
1800 END PROC
1810 ' ====================================================================== files (DESIGN listings)
1820 ' writes the picture as numbered lines; false text in R$ says why not
1830 PROC EXPORT(F$)
1840   LOCAL A$,I,V,MN,MX,B,Y,X,R$,K$
1850   UI GETIMAGE 100,A$
1860   MN=999
1870   MX=0
1880   FOR I=1 TO 384
1890     V=ASC(MID$(A$,I,1))
1900     IF V>0 THEN
1910       IF V<MN THEN MN=V
1920       IF V>MX THEN MX=V
1930     END IF
1940   NEXT I
1950   B=0
1960   IF MX>0 THEN B=MN-1
1970   IF MX-B>15 THEN
1980     CALL STATUS("Not saved: the colors span more than one window of 15 colors.")
1990     EXIT PROC
2000   END IF
2010   K$="SPRITE"
2020   IF MD=1 THEN K$="TILE"
2030   OPEN F$ FOR OUTPUT AS #1
2040   PRINT #1,"10 DESIGN "+K$+" "+STR$(IMG)+","+STR$(B)
2050   FOR Y=0 TO 23
2060     R$=""
2070     FOR X=0 TO 15
2080       V=ASC(MID$(A$,Y*16+X+1,1))
2090       IF V=0 THEN R$=R$+"." ELSE R$=R$+MID$("0123456789ABCDEF",V-B+1,1)
2100     NEXT X
2110     PRINT #1,STR$(20+Y*10)+" @ "+CHR$(34)+R$+CHR$(34)
2120   NEXT Y
2130   CLOSE #1
2140   CALL STATUS("Saved "+F$+" ("+K$+" "+STR$(IMG)+", base "+STR$(B)+").")
2150 END PROC
2160 ' the value of a row character
2170 FUNC HEXV(C$)
2180   LOCAL V
2190   V=ASC(C$)
2200   IF V>=48 AND V<=57 THEN RETURN V-48
2210   IF V>=65 AND V<=70 THEN RETURN V-55
2220   IF V>=97 AND V<=102 THEN RETURN V-87
2230   RETURN 0
2240 END FUNC
2250 PROC IMPORT(F$)
2260   LOCAL L$,A$,N,I,J,Y,X,B,P,R$,K,KIND,NUM
2270   A$=STRING$(384,0)
2280   Y=0
2290   KIND=-1
2300   B=0
2310   NUM=IMG
2320   OPEN F$ FOR INPUT AS #1
2330   WHILE EOF(1)=0
2340     LINE INPUT #1,L$
2350     N=LEN(L$)
2360     FOR I=1 TO N
2370       IF MID$(L$,I,6)="SPRITE" AND KIND<0 THEN
2380         KIND=0
2390         J=I+6
2400       END IF
2410       IF MID$(L$,I,4)="TILE" AND KIND<0 THEN
2420         KIND=1
2430         J=I+4
2440       END IF
2450     NEXT I
2460     IF KIND>=0 AND Y=0 AND J>0 THEN
2470       NUM=VAL(MID$(L$,J))
2480       B=0
2490       FOR I=J TO N
2500         IF MID$(L$,I,1)="," THEN B=VAL(MID$(L$,I+1))
2510       NEXT I
2520       J=0
2530     END IF
2540     P=0
2550     FOR I=1 TO N
2560       IF MID$(L$,I,1)="@" AND P=0 THEN P=I
2570     NEXT I
2580     IF P>0 AND Y<24 THEN
2590       K=0
2600       FOR I=P TO N
2610         IF MID$(L$,I,1)=CHR$(34) AND K=0 THEN K=I
2620       NEXT I
2630       IF K>0 THEN
2640         R$=MID$(L$,K+1,16)
2650         FOR X=0 TO 15
2660           IF X<LEN(R$) THEN
2670             I=FN HEXV(MID$(R$,X+1,1))
2680             IF I>0 THEN I=I+B
2690             MID$(A$,Y*16+X+1,1)=CHR$(I)
2700           END IF
2710         NEXT X
2720         Y=Y+1
2730       END IF
2740     END IF
2750   WEND
2760   CLOSE #1
2770   IF Y<24 OR KIND<0 THEN
2780     CALL STATUS("Not a DESIGN listing of a picture (found "+STR$(Y)+" rows).")
2790     EXIT PROC
2800   END IF
2810   MD=KIND
2820   IMG=NUM
2830   IF IMG<0 OR IMG>254 THEN IMG=0
2840   UI VALUE 105+MD,1
2850   UI VALUE 108,IMG
2860   UI CAPTION 109,STR$(IMG)
2870   CALL PALCOPY
2880   UI IMAGE 100,A$
2890   DT=0
2900   CALL PREVIEW
2910   CALL STATUS("Opened "+F$+".")
2920 END PROC
2930 ' ====================================================================== what happens
2940 LABEL HANDLER
2950 E=UI("EVENT")
2960 WHILE E<>0
2970   K=UI("KIND")
2980   V=UI("VALUE")
2990   IF E=100 AND K=7 THEN CALL PAINT(V,COL)
3000   IF E=100 AND K=8 THEN CALL PAINT(V,0)
3010   IF E=101 AND K=7 THEN
3020     IF V=0 THEN CALL SETCOL(0) ELSE CALL SETCOL(WB+V)
3030   END IF
3040   IF E=102 THEN
3050     WB=V*16
3060     CALL SHOWWIN
3070   END IF
3080   IF (E=105 OR E=106) AND K=2 THEN
3090     MD=E-105
3100     CALL PALCOPY
3110     CALL LOADIMG
3120   END IF
3130   IF E=108 THEN
3140     IMG=V
3150     UI CAPTION 109,STR$(V)
3160     CALL LOADIMG
3170   END IF
3180   IF (E=110 OR E=111) AND K=2 THEN TOOL=E-110
3190   IF E=114 AND K=1 THEN CALL TRANSFORM(3)
3200   IF E=115 AND K=1 THEN CALL TRANSFORM(1)
3210   IF E=116 AND K=1 THEN CALL TRANSFORM(2)
3220   IF E=117 AND K=1 THEN CALL LOADIMG
3230   IF E=118 AND K=1 THEN CALL STOREIMG
3240   IF E=119 AND K=1 THEN
3250     F$=FN FR_PICK$("Open a picture",DR$,"*.bas",0)
3260     DR$=FR_DIR$
3270     IF F$<>"" THEN CALL IMPORT(F$)
3280   END IF
3290   IF E=120 AND K=1 THEN
3300     F$=FN FR_PICK$("Save the picture as",DR$,"*.bas",-1)
3310     DR$=FR_DIR$
3320     IF F$<>"" THEN CALL EXPORT(F$)
3330   END IF
3340   IF E=121 AND K=1 THEN Q=1
3350   E=UI("EVENT")
3360 WEND
3370 RETURN
