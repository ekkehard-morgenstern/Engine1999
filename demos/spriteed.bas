1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' SPRITE AND TILE EDITOR - edits the 16 x 24 pictures of the sprites and the tiles with the mouse, on the UI subsystem (UI CANVAS, ON UI, the file
20 ' requester of the library filereq, dialogs of the library uikit). Needs the window (engine1999). ESC cancels a dialog; Ctrl+C stops the program.
30 '   Left button: paint with the current color (the Pen) or fill an area (the Fill tool); right button: erase (color 0, transparent).
40 '   Colors: the 16 colors of a palette window (the slider "Window" moves it over the 224 colors of the layer: sprites use the palette of the sprite
50 '   layer, tiles that of the tile layer); a click on a color picks it. Cell 0 of a window is transparent, cell n is color base + n, as in
60 '   DESIGN SPRITE n, base.
70 '   Image: the number of the picture (0..254; sprite picture 255 is the scratch picture of the preview). Revert loads the picture from the
80 '   engine again, Store puts the edited picture into the engine (SPRITE DEF / TILE DEF), so that your program sees it at once.
85 '   Undo / Redo: 30 steps (a stroke = a held mouse button, a transformation, Revert); switching or opening a picture starts a new history.
90 '   Open... / Save...: a picture as a DESIGN listing (10 DESIGN SPRITE n, base, then 24 rows @ "...") that you can MERGE or LOAD into a
100 '   program; Open reads such a file (DESIGN SPRITE / DESIGN TILE, the rows of @ statements). A picture can be saved only when its colors
110 '   fit into one window of 15 colors. The preview shows the picture as a real sprite (a tile uses the sprite palette there).
120 LOAD LIBRARY "filereq"
125 LOAD LIBRARY "uikit"
126 BREAK OFF
130 GFX RESET ALL
140 CLS
150 DIM SX(400)
160 DIM SY(400)
170 DIM UD$(32)
180 DIM RD$(32)
190 MD=0
200 IMG=1
210 WB=0
220 COL=1
230 TOOL=0
240 DT=0
250 Q=0
260 UC=0
270 RC=0
280 PNT=0
290 DR$=""
300 UI OPEN
310 CALL MAKEFORM
320 CALL PALCOPY
330 CALL SHOWWIN
340 CALL SETCOL(1)
350 CALL LOADIMG
360 ON ERROR GOTO ERRH
370 LABEL LOOPTOP
380 ON ERROR GOTO ERRH
390 ON UI GOSUB HANDLER
400 REPEAT
410   GFX VSYNC
420   UI UPDATE
430   IF MOUSE("B")=0 THEN PNT=0
440 UNTIL Q=1
450 ON UI OFF
460 ON ERROR OFF
470 SPRITE 247,OFF
480 UI CLOSE
490 GFX RESET
500 CLS
510 PRINT "Sprite and tile editor finished."
520 END
530 LABEL ERRH
535 ON UI OFF
540 CALL UK_MSG("Error",ERR$)
550 RESUME LOOPTOP
560 ' ====================================================================== the form
570 PROC MAKEFORM
580   UI PANEL 99,0,0,640,300,"Sprite and tile editor"
590   UI CANVAS 100,10,24,164,244,16,24
600   UI LABEL 122,200,24,116,14,"Colors"
610   UI CANVAS 101,200,40,116,116,4,4
620   UI LABEL 123,200,160,116,14,"Window"
630   UI SLIDER 102,200,174,116,18
640   UI RANGE 102,0,13
650   UI CANVAS 103,200,198,48,30,1,1
660   UI LABEL 104,254,206,70,14,"Color 0"
670   UI RADIO 105,330,28,90,16,"Sprite",1
680   UI RADIO 106,430,28,90,16,"Tile",1
690   UI VALUE 105,1
700   UI LABEL 107,330,54,45,14,"Image"
710   UI SLIDER 108,378,50,200,18
720   UI RANGE 108,0,254
730   UI VALUE 108,1
740   UI LABEL 109,586,54,40,14,"1"
750   UI RADIO 110,330,80,90,16,"Pen",2
760   UI RADIO 111,430,80,90,16,"Fill",2
770   UI VALUE 110,1
780   UI LABEL 113,330,106,60,14,"Preview"
790   UI BUTTON 114,330,196,90,22,"Clear"
800   UI BUTTON 115,430,196,90,22,"Flip H"
810   UI BUTTON 116,530,196,90,22,"Flip V"
820   UI BUTTON 117,330,224,90,22,"Revert"
830   UI BUTTON 118,430,224,90,22,"Store"
840   UI BUTTON 119,330,252,90,22,"Open..."
850   UI BUTTON 120,430,252,90,22,"Save..."
860   UI BUTTON 121,530,252,90,22,"Quit"
870   UI BUTTON 124,430,126,90,22,"Undo"
880   UI BUTTON 125,530,126,90,22,"Redo"
890   UI LABEL 112,10,276,620,14,"Left button paints, right button erases. Store puts the picture into the engine."
900 END PROC
910 PROC STATUS(T$)
920   UI CAPTION 112,T$
930 END PROC
940 ' ====================================================================== the colors
950 ' the palette of the layer that holds the pictures (sprite layer 4, tile layer 1) is copied into the graphics layer (3) that the UI draws on
960 PROC PALCOPY
970   LOCAL I,L
980   L=4
990   IF MD=1 THEN L=1
1000   FOR I=1 TO 223
1010     GFX PALETTE 3,I,GFX("PALETTE",L,I)
1020   NEXT I
1030 END PROC
1040 PROC SHOWWIN
1050   LOCAL I
1060   FOR I=0 TO 15
1070     IF I=0 THEN UI PIXEL 101,0,0,0 ELSE UI PIXEL 101,I MOD 4,I\4,WB+I
1080   NEXT I
1090   UI CAPTION 123,"Window "+STR$(WB)+" .. "+STR$(WB+15)
1100 END PROC
1110 PROC SETCOL(C)
1120   COL=C
1130   UI PIXEL 103,0,0,C
1140   UI CAPTION 104,"Color "+STR$(C)
1150 END PROC
1160 ' ====================================================================== the picture
1170 PROC PREVIEW
1180   LOCAL A$
1190   UI GETIMAGE 100,A$
1200   SPRITE DEF 255,A$
1210   SPRITE 247,IMAGE 255
1220   SPRITE 247,346,126
1230   SPRITE 247,ON
1240 END PROC
1250 PROC LOADIMG
1260   LOCAL A$
1270   UC=0
1280   RC=0
1290   IF MD=0 THEN SPRITE GET IMG,A$ ELSE TILE GET IMG,A$
1300   UI IMAGE 100,A$
1310   DT=0
1320   CALL PREVIEW
1330   CALL STATUS("Picture "+STR$(IMG)+" loaded from the engine ("+MID$("sprite tile  ",1+MD*7,6)+").")
1340 END PROC
1350 PROC STOREIMG
1360   LOCAL A$
1370   UI GETIMAGE 100,A$
1380   IF MD=0 THEN SPRITE DEF IMG,A$ ELSE TILE DEF IMG,A$
1390   DT=0
1400   CALL STATUS("Picture "+STR$(IMG)+" stored: your programs see it now.")
1410 END PROC
1420 PROC FLOOD(X,Y,C)
1430   LOCAL T,SP,D,NX,NY
1440   T=UI("PIXEL",100,X,Y)
1450   IF T=C THEN EXIT PROC
1460   UI PIXEL 100,X,Y,C
1470   SX(0)=X
1480   SY(0)=Y
1490   SP=1
1500   WHILE SP>0
1510     SP=SP-1
1520     X=SX(SP)
1530     Y=SY(SP)
1540     FOR D=0 TO 3
1550       NX=X-(D=0)+(D=1)
1560       NY=Y-(D=2)+(D=3)
1570       IF NX>=0 AND NX<16 AND NY>=0 AND NY<24 THEN
1580         IF UI("PIXEL",100,NX,NY)=T THEN
1590           UI PIXEL 100,NX,NY,C
1600           SX(SP)=NX
1610           SY(SP)=NY
1620           SP=SP+1
1630         END IF
1640       END IF
1650     NEXT D
1660   WEND
1670 END PROC
1680 PROC PAINT(CELL,C)
1690   LOCAL X,Y
1700   IF PNT=0 THEN CALL PUSHU
1710   PNT=1
1720   X=CELL MOD 16
1730   Y=CELL\16
1740   IF TOOL=1 THEN CALL FLOOD(X,Y,C) ELSE UI PIXEL 100,X,Y,C
1750   DT=-1
1760   CALL PREVIEW
1770 END PROC
1780 PROC TRANSFORM(K)
1790   LOCAL A$,B$,X,Y
1800   CALL PUSHU
1810   UI GETIMAGE 100,A$
1820   B$=""
1830   FOR Y=0 TO 23
1840     FOR X=0 TO 15
1850       IF K=1 THEN B$=B$+MID$(A$,Y*16+15-X+1,1)
1860       IF K=2 THEN B$=B$+MID$(A$,(23-Y)*16+X+1,1)
1870       IF K=3 THEN B$=B$+CHR$(0)
1880     NEXT X
1890   NEXT Y
1900   UI IMAGE 100,B$
1910   DT=-1
1920   CALL PREVIEW
1930 END PROC
1940 ' ====================================================================== undo and redo
1950 ' every stroke (a held mouse button), every transformation and Revert push the picture as it was before; 30 steps
1960 PROC PUSHSNAP(S$)
1970   LOCAL I
1980   IF UC>=30 THEN
1990     FOR I=1 TO 29
2000       UD$(I-1)=UD$(I)
2010     NEXT I
2020     UC=29
2030   END IF
2040   UD$(UC)=S$
2050   UC=UC+1
2060   RC=0
2070 END PROC
2080 PROC PUSHU
2090   LOCAL A$
2100   UI GETIMAGE 100,A$
2110   CALL PUSHSNAP(A$)
2120 END PROC
2130 PROC DOUNDO
2140   LOCAL A$
2150   IF UC=0 THEN
2160     CALL STATUS("Nothing to undo.")
2170     EXIT PROC
2180   END IF
2190   UI GETIMAGE 100,A$
2200   RD$(RC)=A$
2210   RC=RC+1
2220   UC=UC-1
2230   UI IMAGE 100,UD$(UC)
2240   DT=-1
2250   CALL PREVIEW
2260   CALL STATUS("Undone ("+STR$(UC)+" more steps).")
2270 END PROC
2280 PROC DOREDO
2290   LOCAL A$
2300   IF RC=0 THEN
2310     CALL STATUS("Nothing to redo.")
2320     EXIT PROC
2330   END IF
2340   UI GETIMAGE 100,A$
2350   UD$(UC)=A$
2360   UC=UC+1
2370   RC=RC-1
2380   UI IMAGE 100,RD$(RC)
2390   DT=-1
2400   CALL PREVIEW
2410   CALL STATUS("Redone ("+STR$(RC)+" more steps).")
2420 END PROC
2430 PROC REVERT
2440   LOCAL B$
2450   UI GETIMAGE 100,B$
2460   CALL LOADIMG
2470   CALL PUSHSNAP(B$)
2480 END PROC
2490 ' ====================================================================== files (DESIGN listings)
2500 ' writes the picture as numbered lines; false text in R$ says why not
2510 PROC EXPORT(F$)
2520   LOCAL A$,I,V,MN,MX,B,Y,X,R$,K$
2530   UI GETIMAGE 100,A$
2540   MN=999
2550   MX=0
2560   FOR I=1 TO 384
2570     V=ASC(MID$(A$,I,1))
2580     IF V>0 THEN
2590       IF V<MN THEN MN=V
2600       IF V>MX THEN MX=V
2610     END IF
2620   NEXT I
2630   B=0
2640   IF MX>0 THEN B=MN-1
2650   IF MX-B>15 THEN
2660     CALL STATUS("Not saved: the colors span more than one window of 15 colors.")
2670     EXIT PROC
2680   END IF
2690   K$="SPRITE"
2700   IF MD=1 THEN K$="TILE"
2710   OPEN F$ FOR OUTPUT AS #1
2720   PRINT #1,"10 DESIGN "+K$+" "+STR$(IMG)+","+STR$(B)
2730   FOR Y=0 TO 23
2740     R$=""
2750     FOR X=0 TO 15
2760       V=ASC(MID$(A$,Y*16+X+1,1))
2770       IF V=0 THEN R$=R$+"." ELSE R$=R$+MID$("0123456789ABCDEF",V-B+1,1)
2780     NEXT X
2790     PRINT #1,STR$(20+Y*10)+" @ "+CHR$(34)+R$+CHR$(34)
2800   NEXT Y
2810   CLOSE #1
2820   CALL STATUS("Saved "+F$+" ("+K$+" "+STR$(IMG)+", base "+STR$(B)+").")
2830 END PROC
2840 ' the value of a row character
2850 FUNC HEXV(C$)
2860   LOCAL V
2870   V=ASC(C$)
2880   IF V>=48 AND V<=57 THEN RETURN V-48
2890   IF V>=65 AND V<=70 THEN RETURN V-55
2900   IF V>=97 AND V<=102 THEN RETURN V-87
2910   RETURN 0
2920 END FUNC
2930 PROC IMPORT(F$)
2940   LOCAL L$,A$,N,I,J,Y,X,B,P,R$,K,KIND,NUM
2950   A$=STRING$(384,0)
2960   Y=0
2970   KIND=-1
2980   B=0
2990   NUM=IMG
3000   OPEN F$ FOR INPUT AS #1
3010   WHILE EOF(1)=0
3020     LINE INPUT #1,L$
3030     N=LEN(L$)
3040     FOR I=1 TO N
3050       IF MID$(L$,I,6)="SPRITE" AND KIND<0 THEN
3060         KIND=0
3070         J=I+6
3080       END IF
3090       IF MID$(L$,I,4)="TILE" AND KIND<0 THEN
3100         KIND=1
3110         J=I+4
3120       END IF
3130     NEXT I
3140     IF KIND>=0 AND Y=0 AND J>0 THEN
3150       NUM=VAL(MID$(L$,J))
3160       B=0
3170       FOR I=J TO N
3180         IF MID$(L$,I,1)="," THEN B=VAL(MID$(L$,I+1))
3190       NEXT I
3200       J=0
3210     END IF
3220     P=0
3230     FOR I=1 TO N
3240       IF MID$(L$,I,1)="@" AND P=0 THEN P=I
3250     NEXT I
3260     IF P>0 AND Y<24 THEN
3270       K=0
3280       FOR I=P TO N
3290         IF MID$(L$,I,1)=CHR$(34) AND K=0 THEN K=I
3300       NEXT I
3310       IF K>0 THEN
3320         R$=MID$(L$,K+1,16)
3330         FOR X=0 TO 15
3340           IF X<LEN(R$) THEN
3350             I=FN HEXV(MID$(R$,X+1,1))
3360             IF I>0 THEN I=I+B
3370             MID$(A$,Y*16+X+1,1)=CHR$(I)
3380           END IF
3390         NEXT X
3400         Y=Y+1
3410       END IF
3420     END IF
3430   WEND
3440   CLOSE #1
3450   IF Y<24 OR KIND<0 THEN
3460     CALL STATUS("Not a DESIGN listing of a picture (found "+STR$(Y)+" rows).")
3470     EXIT PROC
3480   END IF
3490   MD=KIND
3500   IMG=NUM
3510   IF IMG<0 OR IMG>254 THEN IMG=0
3520   UI VALUE 105+MD,1
3530   UI VALUE 108,IMG
3540   UI CAPTION 109,STR$(IMG)
3550   CALL PALCOPY
3560   UC=0
3570   RC=0
3580   UI IMAGE 100,A$
3590   DT=0
3600   CALL PREVIEW
3610   CALL STATUS("Opened "+F$+".")
3620 END PROC
3630 ' ====================================================================== what happens
3640 LABEL HANDLER
3650 E=UI("EVENT")
3660 WHILE E<>0
3670   K=UI("KIND")
3680   V=UI("VALUE")
3690   IF E=100 AND K=7 THEN CALL PAINT(V,COL)
3700   IF E=100 AND K=8 THEN CALL PAINT(V,0)
3710   IF E=101 AND K=7 THEN
3720     IF V=0 THEN CALL SETCOL(0) ELSE CALL SETCOL(WB+V)
3730   END IF
3740   IF E=102 THEN
3750     WB=V*16
3760     CALL SHOWWIN
3770   END IF
3780   IF (E=105 OR E=106) AND K=2 THEN
3790     MD=E-105
3800     CALL PALCOPY
3810     CALL LOADIMG
3820   END IF
3830   IF E=108 THEN
3840     IMG=V
3850     UI CAPTION 109,STR$(V)
3860     CALL LOADIMG
3870   END IF
3880   IF (E=110 OR E=111) AND K=2 THEN TOOL=E-110
3890   IF E=114 AND K=1 THEN CALL TRANSFORM(3)
3900   IF E=115 AND K=1 THEN CALL TRANSFORM(1)
3910   IF E=116 AND K=1 THEN CALL TRANSFORM(2)
3920   IF E=117 AND K=1 THEN CALL REVERT
3930   IF E=124 AND K=1 THEN CALL DOUNDO
3940   IF E=125 AND K=1 THEN CALL DOREDO
3950   IF E=118 AND K=1 THEN CALL STOREIMG
3960   IF E=119 AND K=1 THEN
3970     F$=FN FR_PICK$("Open a picture",DR$,"*.bas",0)
3980     DR$=FR_DIR$
3990     IF F$<>"" THEN CALL IMPORT(F$)
4000   END IF
4010   IF E=120 AND K=1 THEN
4020     F$=FN FR_PICK$("Save the picture as",DR$,"*.bas",-1)
4030     DR$=FR_DIR$
4040     IF F$<>"" THEN CALL EXPORT(F$)
4050   END IF
4060   IF E=121 AND K=1 THEN
4061     IF DT=0 THEN
4062       Q=1
4063     ELSE
4064       IF FN UK_ASK("Quit","The picture has changes that were not stored.|Quit anyway?")<>0 THEN Q=1
4065     END IF
4066   END IF
4070   E=UI("EVENT")
4080 WEND
4090 RETURN
