1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' SPRITE AND TILE EDITOR - edits the 16 x 24 pictures of the sprites and the tiles with the mouse, on the UI subsystem (UI CANVAS, ON UI, the file
20 ' requester of the library filereq, dialogs of the library uikit). Needs the window (engine1999). ESC cancels a dialog; Ctrl+C stops the program.
30 '   Size: a picture can be a group of up to 8 pictures drawn side by side (the sprite groups of SPRITE n,SIZE w,h; for tiles the blocks of the library
40 '   tilegrp): the slider "Size" chooses 1 x 1, 2 x 1, 1 x 2, 2 x 2, 3 x 1, 1 x 3, 3 x 2, 2 x 3, 4 x 1, 1 x 4, 4 x 2 or 2 x 4 pictures; the canvas then shows the
50 '   whole block (16 x 24 pixels per picture), the pictures are the numbers Image, Image + 1, ... row by row. Changing the size reloads the block from
60 '   the engine (it asks first if there are changes). The preview shows the block as a real sprite group.
70 '   Left button: paint with the current color (the Pen) or fill an area (the Fill tool); right button: erase (color 0, transparent).
80 '   Colors: the 16 colors of a palette window (the slider "Window" moves it over the 224 colors of the layer: sprites use the palette of the sprite
90 '   layer, tiles that of the tile layer); a click on a color picks it. Cell 0 of a window is transparent, cell n is color base + n, as in
100 '   DESIGN SPRITE n, base.
110 '   Image: the number of the (first) picture (sprites 0..247, tiles 0..255; the sprite pictures 248..255 are the scratch pictures of the preview). Revert loads the picture from the
120 '   engine again, Store puts the edited picture into the engine (SPRITE DEF / TILE DEF), so that your program sees it at once.
130 '   Undo / Redo: 30 steps (a stroke = a held mouse button, a transformation, Revert); switching or opening a picture starts a new history.
140 '   Open... / Save...: a picture as a DESIGN listing (a comment ' GROUP w,h, then for each picture DESIGN SPRITE n, base and 24 rows @ "...") that you
150 '   can MERGE or LOAD into a program; Open reads such a file (DESIGN SPRITE / DESIGN TILE, the rows of @ statements). A block can be saved only when
160 '   its colors fit into one window of 15 colors. The preview shows the block as a real sprite group (a tile uses the sprite palette there).
161 '   Import BMP... / Export BMP...: Windows BMP files (roadmap item 53: SPRITE LOAD / SAVE, TILE LOAD / SAVE). Import cuts a BMP picture into 16 x 24 pictures (row by row)
162 '   and puts them into the engine from the picture Image on (the palette question: Yes takes the colors of the file as the palette of the layer, No matches the
163 '   colors of the file to the present palette; transparent pixels and color 0 are transparent), then loads the block into the editor. Export stores the edited block
164 '   first and writes its pictures as a sheet as wide as the block (a 2 x 3 block gives a sheet of 2 x 3 cells); a name without an extension gets .bmp.
170 LOAD LIBRARY "filereq"
180 LOAD LIBRARY "uikit"
190 BREAK OFF
200 GFX RESET ALL
210 CLS
220 DIM SX(7000)
230 DIM SY(7000)
240 DIM MM$(8)
250 DIM UD$(32)
260 DIM RD$(32)
270 MD=0
280 IMG=1
290 GW=1
300 GH=1
310 BW=16
320 BH=24
330 NB=1
340 SZ=0
350 PW$="121231324142"
360 PH$="112213231424"
370 BL$=""
380 M$=""
390 WB=0
400 COL=1
410 TOOL=0
420 DT=0
430 Q=0
440 UC=0
450 RC=0
460 PNT=0
470 DR$=""
480 UI OPEN
490 CALL MAKEFORM
500 CALL PALCOPY
510 CALL SHOWWIN
520 CALL SETCOL(1)
530 CALL SETRANGE
540 CALL LOADIMG
550 ON ERROR GOTO ERRH
560 LABEL LOOPTOP
570 ON ERROR GOTO ERRH
580 ON UI GOSUB HANDLER
590 REPEAT
600   GFX VSYNC
610   UI UPDATE
620   IF MOUSE("B")=0 THEN PNT=0
630 UNTIL Q=1
640 ON UI OFF
650 ON ERROR OFF
660 SPRITE 240,SIZE 1,1
670 SPRITE 240,OFF
680 UI CLOSE
690 GFX RESET
700 CLS
710 PRINT "Sprite and tile editor finished."
720 END
730 LABEL ERRH
740 ON UI OFF
750 CALL UK_MSG("Error",ERR$)
760 RESUME LOOPTOP
770 ' ====================================================================== the form
780 PROC MAKEFORM
790   UI PANEL 99,0,0,640,300,"Sprite and tile editor"
800   UI CANVAS 100,10,24,164,244,16,24
810   UI LABEL 122,200,24,116,14,"Colors"
820   UI CANVAS 101,200,40,116,116,4,4
830   UI LABEL 123,200,160,116,14,"Window"
840   UI SLIDER 102,200,174,116,18
850   UI RANGE 102,0,13
860   UI CANVAS 103,200,198,48,30,1,1
870   UI LABEL 104,254,206,70,14,"Color 0"
880   UI RADIO 105,330,28,90,16,"Sprite",1
890   UI RADIO 106,430,28,90,16,"Tile",1
900   UI VALUE 105,1
910   UI LABEL 107,330,54,45,14,"Image"
920   UI SLIDER 108,378,50,200,18
930   UI RANGE 108,0,254
940   UI VALUE 108,1
950   UI LABEL 109,586,54,40,14,"1"
960   UI RADIO 110,330,80,90,16,"Pen",2
970   UI RADIO 111,430,80,90,16,"Fill",2
980   UI VALUE 110,1
990   CALL UK_SLIDER(126,330,102,290,"Size",0,11,0)
1000   CALL UK_READ(126,"1 x 1")
1010   UI LABEL 113,330,132,60,14,"Preview"
1020   UI BUTTON 124,436,130,90,22,"Undo"
1030   UI BUTTON 125,534,130,90,22,"Redo"
1040   UI BUTTON 114,436,156,90,22,"Clear"
1050   UI BUTTON 115,534,156,90,22,"Flip H"
1060   UI BUTTON 116,436,182,90,22,"Flip V"
1070   UI BUTTON 117,534,182,90,22,"Revert"
1080   UI BUTTON 118,436,208,90,22,"Store"
1085   UI BUTTON 130,534,208,90,22,"Import BMP"
1090   UI BUTTON 119,436,234,90,22,"Open..."
1100   UI BUTTON 120,534,234,90,22,"Save..."
1110   UI BUTTON 121,534,260,90,22,"Quit"
1115   UI BUTTON 131,436,260,90,22,"Export BMP"
1120   UI LABEL 112,10,276,420,14,"Left button paints, right button erases. Store puts the picture into the engine."
1130 END PROC
1140 PROC STATUS(T$)
1150   UI CAPTION 112,T$
1160 END PROC
1170 ' ====================================================================== the colors
1180 ' the palette of the layer that holds the pictures (sprite layer 4, tile layer 1) is copied into the graphics layer (3) that the UI draws on
1190 PROC PALCOPY
1200   LOCAL I,L
1210   L=4
1220   IF MD=1 THEN L=1
1230   FOR I=1 TO 223
1240     GFX PALETTE 3,I,GFX("PALETTE",L,I)
1250   NEXT I
1260 END PROC
1270 PROC SHOWWIN
1280   LOCAL I
1290   FOR I=0 TO 15
1300     IF I=0 THEN UI PIXEL 101,0,0,0 ELSE UI PIXEL 101,I MOD 4,I\4,WB+I
1310   NEXT I
1320   UI CAPTION 123,"Window "+STR$(WB)+" .. "+STR$(WB+15)
1330 END PROC
1340 PROC SETCOL(C)
1350   COL=C
1360   UI PIXEL 103,0,0,C
1370   UI CAPTION 104,"Color "+STR$(C)
1380 END PROC
1390 ' ====================================================================== the picture
1400 PROC PREVIEW
1410   LOCAL K
1420   UI GETIMAGE 100,BL$
1430   FOR K=0 TO NB-1
1440     CALL GETMEMBER(K)
1450     SPRITE DEF 248+K,M$
1460   NEXT K
1470   SPRITE 240,IMAGE 248
1480   SPRITE 240,338,152
1490   SPRITE 240,ON
1500 END PROC
1510 PROC LOADIMG
1520   LOCAL K
1530   UC=0
1540   RC=0
1550   BL$=STRING$(BW*BH,0)
1560   FOR K=0 TO NB-1
1570     IF MD=0 THEN SPRITE GET IMG+K,M$ ELSE TILE GET IMG+K,M$
1580     CALL PUTMEMBER(K)
1590   NEXT K
1600   UI IMAGE 100,BL$
1610   DT=0
1620   CALL PREVIEW
1630   CALL STATUS("Picture "+STR$(IMG)+" loaded from the engine ("+MID$("sprite tile  ",1+MD*7,6)+").")
1640 END PROC
1650 PROC STOREIMG
1660   LOCAL K
1670   UI GETIMAGE 100,BL$
1680   FOR K=0 TO NB-1
1690     CALL GETMEMBER(K)
1700     IF MD=0 THEN SPRITE DEF IMG+K,M$ ELSE TILE DEF IMG+K,M$
1710   NEXT K
1720   DT=0
1730   CALL STATUS("Picture "+STR$(IMG)+" stored: your programs see it now.")
1740 END PROC
1750 ' member K (column K mod GW, row K \ GW) of the block BL$ into M$ (384 bytes)
1760 PROC GETMEMBER(K)
1770   LOCAL Y,C,R
1780   C=K MOD GW
1790   R=K\GW
1800   M$=""
1810   FOR Y=0 TO 23
1820     M$=M$+MID$(BL$,(R*24+Y)*BW+C*16+1,16)
1830   NEXT Y
1840 END PROC
1850 ' M$ into the place of member K of the block BL$
1860 PROC PUTMEMBER(K)
1870   LOCAL Y,C,R
1880   C=K MOD GW
1890   R=K\GW
1900   FOR Y=0 TO 23
1910     MID$(BL$,(R*24+Y)*BW+C*16+1,16)=MID$(M$,Y*16+1,16)
1920   NEXT Y
1930 END PROC
1940 ' the range of the image slider: the block must fit below the scratch pictures of the preview (sprites) or the end of the tiles
1950 PROC SETRANGE
1960   LOCAL MX
1970   MX=248-NB
1980   IF MD=1 THEN MX=256-NB
1990   UI RANGE 108,0,MX
2000   IF IMG>MX THEN
2010     IMG=MX
2020     UI VALUE 108,IMG
2030     UI CAPTION 109,STR$(IMG)
2040   END IF
2050 END PROC
2060 ' a new size of the block (preset I of the table): a new canvas, the preview group; the block is loaded by the caller
2070 PROC RESIZE(I)
2080   SZ=I
2090   GW=VAL(MID$(PW$,I+1,1))
2100   GH=VAL(MID$(PH$,I+1,1))
2110   BW=16*GW
2120   BH=24*GH
2130   NB=GW*GH
2140   UI REMOVE 100
2150   UI CANVAS 100,10,24,164,244,BW,BH
2160   SPRITE 240,SIZE 1,1
2170   SPRITE 240,SIZE GW,GH
2180   CALL UK_SET(126,I)
2190   CALL UK_READ(126,STR$(GW)+" x "+STR$(GH))
2200   CALL SETRANGE
2210 END PROC
2220 PROC NEWSIZE(I)
2230   IF DT<>0 THEN
2240     IF FN UK_ASK("Size","Changing the size reloads the block from the engine.|Discard the changes?")=0 THEN
2250       CALL UK_SET(126,SZ)
2260       CALL UK_READ(126,STR$(GW)+" x "+STR$(GH))
2270       EXIT PROC
2280     END IF
2290   END IF
2300   CALL RESIZE(I)
2310   CALL LOADIMG
2320 END PROC
2330 PROC FLOOD(X,Y,C)
2340   LOCAL T,SP,D,NX,NY
2350   T=UI("PIXEL",100,X,Y)
2360   IF T=C THEN EXIT PROC
2370   UI PIXEL 100,X,Y,C
2380   SX(0)=X
2390   SY(0)=Y
2400   SP=1
2410   WHILE SP>0
2420     SP=SP-1
2430     X=SX(SP)
2440     Y=SY(SP)
2450     FOR D=0 TO 3
2460       NX=X-(D=0)+(D=1)
2470       NY=Y-(D=2)+(D=3)
2480       IF NX>=0 AND NX<BW AND NY>=0 AND NY<BH THEN
2490         IF UI("PIXEL",100,NX,NY)=T THEN
2500           UI PIXEL 100,NX,NY,C
2510           SX(SP)=NX
2520           SY(SP)=NY
2530           SP=SP+1
2540         END IF
2550       END IF
2560     NEXT D
2570   WEND
2580 END PROC
2590 PROC PAINT(CELL,C)
2600   LOCAL X,Y
2610   IF PNT=0 THEN CALL PUSHU
2620   PNT=1
2630   X=CELL MOD BW
2640   Y=CELL\BW
2650   IF TOOL=1 THEN CALL FLOOD(X,Y,C) ELSE UI PIXEL 100,X,Y,C
2660   DT=-1
2670   CALL PREVIEW
2680 END PROC
2690 PROC TRANSFORM(K)
2700   LOCAL B$,R$,X,Y
2710   CALL PUSHU
2720   UI GETIMAGE 100,BL$
2730   B$=""
2740   FOR Y=0 TO BH-1
2750     IF K=1 THEN
2760       R$=""
2770       FOR X=BW-1 DOWNTO 0
2780         R$=R$+MID$(BL$,Y*BW+X+1,1)
2790       NEXT X
2800       B$=B$+R$
2810     END IF
2820     IF K=2 THEN B$=B$+MID$(BL$,(BH-1-Y)*BW+1,BW)
2830     IF K=3 THEN B$=B$+STRING$(BW,0)
2840   NEXT Y
2850   UI IMAGE 100,B$
2860   DT=-1
2870   CALL PREVIEW
2880 END PROC
2890 ' ====================================================================== undo and redo
2900 ' every stroke (a held mouse button), every transformation and Revert push the picture as it was before; 30 steps
2910 PROC PUSHSNAP(S$)
2920   LOCAL I
2930   IF UC>=30 THEN
2940     FOR I=1 TO 29
2950       UD$(I-1)=UD$(I)
2960     NEXT I
2970     UC=29
2980   END IF
2990   UD$(UC)=S$
3000   UC=UC+1
3010   RC=0
3020 END PROC
3030 PROC PUSHU
3040   LOCAL A$
3050   UI GETIMAGE 100,A$
3060   CALL PUSHSNAP(A$)
3070 END PROC
3080 PROC DOUNDO
3090   LOCAL A$
3100   IF UC=0 THEN
3110     CALL STATUS("Nothing to undo.")
3120     EXIT PROC
3130   END IF
3140   UI GETIMAGE 100,A$
3150   RD$(RC)=A$
3160   RC=RC+1
3170   UC=UC-1
3180   UI IMAGE 100,UD$(UC)
3190   DT=-1
3200   CALL PREVIEW
3210   CALL STATUS("Undone ("+STR$(UC)+" more steps).")
3220 END PROC
3230 PROC DOREDO
3240   LOCAL A$
3250   IF RC=0 THEN
3260     CALL STATUS("Nothing to redo.")
3270     EXIT PROC
3280   END IF
3290   UI GETIMAGE 100,A$
3300   UD$(UC)=A$
3310   UC=UC+1
3320   RC=RC-1
3330   UI IMAGE 100,RD$(RC)
3340   DT=-1
3350   CALL PREVIEW
3360   CALL STATUS("Redone ("+STR$(RC)+" more steps).")
3370 END PROC
3380 PROC REVERT
3390   LOCAL B$
3400   UI GETIMAGE 100,B$
3410   CALL LOADIMG
3420   CALL PUSHSNAP(B$)
3430 END PROC
3440 ' ====================================================================== files (DESIGN listings)
3450 ' writes the picture as numbered lines; false text in R$ says why not
3460 PROC EXPORT(F$)
3470   LOCAL I,V,MN,MX,B,Y,X,R$,K$,K,N
3480   UI GETIMAGE 100,BL$
3490   MN=999
3500   MX=0
3510   FOR I=1 TO BW*BH
3520     V=ASC(MID$(BL$,I,1))
3530     IF V>0 THEN
3540       IF V<MN THEN MN=V
3550       IF V>MX THEN MX=V
3560     END IF
3570   NEXT I
3580   B=0
3590   IF MX>0 THEN B=MN-1
3600   IF MX-B>15 THEN
3610     CALL STATUS("Not saved: the colors span more than one window of 15 colors.")
3620     EXIT PROC
3630   END IF
3640   K$="SPRITE"
3650   IF MD=1 THEN K$="TILE"
3660   OPEN F$ FOR OUTPUT AS #1
3670   N=10
3680   PRINT #1,STR$(N)+" ' GROUP "+STR$(GW)+","+STR$(GH)
3690   FOR K=0 TO NB-1
3700     CALL GETMEMBER(K)
3710     N=N+10
3720     PRINT #1,STR$(N)+" DESIGN "+K$+" "+STR$(IMG+K)+","+STR$(B)
3730     FOR Y=0 TO 23
3740       R$=""
3750       FOR X=0 TO 15
3760         V=ASC(MID$(M$,Y*16+X+1,1))
3770         IF V=0 THEN R$=R$+"." ELSE R$=R$+MID$("0123456789ABCDEF",V-B+1,1)
3780       NEXT X
3790       N=N+10
3800       PRINT #1,STR$(N)+" @ "+CHR$(34)+R$+CHR$(34)
3810     NEXT Y
3820   NEXT K
3830   CLOSE #1
3840   CALL STATUS("Saved "+F$+" ("+K$+" "+STR$(IMG)+", "+STR$(GW)+" x "+STR$(GH)+", base "+STR$(B)+").")
3850 END PROC
3860 ' the value of a row character
3870 FUNC HEXV(C$)
3880   LOCAL V
3890   V=ASC(C$)
3900   IF V>=48 AND V<=57 THEN RETURN V-48
3910   IF V>=65 AND V<=70 THEN RETURN V-55
3920   IF V>=97 AND V<=102 THEN RETURN V-87
3930   RETURN 0
3940 END FUNC
3950 PROC IMPORT(F$)
3960   LOCAL L$,N,I,J,Y,X,B,P,R$,K,KIND,NUM,NM,W,H,G
3970   FOR I=0 TO 7
3980     MM$(I)=STRING$(384,0)
3990   NEXT I
4000   NM=0
4010   Y=24
4020   KIND=-1
4030   B=0
4040   W=0
4050   H=0
4060   NUM=IMG
4070   OPEN F$ FOR INPUT AS #1
4080   WHILE EOF(1)=0
4090     LINE INPUT #1,L$
4100     N=LEN(L$)
4110     G=0
4120     J=0
4130     FOR I=1 TO N
4140       IF MID$(L$,I,5)="GROUP" AND G=0 THEN
4150         G=I
4160         W=VAL(MID$(L$,I+5))
4170         FOR K=I+5 TO N
4180           IF MID$(L$,K,1)="," THEN H=VAL(MID$(L$,K+1))
4190         NEXT K
4200       END IF
4210       IF MID$(L$,I,6)="SPRITE" AND G=0 AND KIND<=0 THEN
4220         KIND=0
4230         J=I+6
4240       END IF
4250       IF MID$(L$,I,4)="TILE" AND G=0 AND KIND<=0 THEN
4260         KIND=1
4270         J=I+4
4280       END IF
4290     NEXT I
4300     IF J>0 AND G=0 AND Y>=24 AND NM<8 THEN
4310       IF NM=0 THEN NUM=VAL(MID$(L$,J))
4320       B=0
4330       FOR I=J TO N
4340         IF MID$(L$,I,1)="," THEN B=VAL(MID$(L$,I+1))
4350       NEXT I
4360       NM=NM+1
4370       Y=0
4380     END IF
4390     P=0
4400     FOR I=1 TO N
4410       IF MID$(L$,I,1)="@" AND P=0 THEN P=I
4420     NEXT I
4430     IF P>0 AND Y<24 AND NM>0 THEN
4440       K=0
4450       FOR I=P TO N
4460         IF MID$(L$,I,1)=CHR$(34) AND K=0 THEN K=I
4470       NEXT I
4480       IF K>0 THEN
4490         R$=MID$(L$,K+1,16)
4500         FOR X=0 TO 15
4510           IF X<LEN(R$) THEN
4520             I=FN HEXV(MID$(R$,X+1,1))
4530             IF I>0 THEN I=I+B
4540             MID$(MM$(NM-1),Y*16+X+1,1)=CHR$(I)
4550           END IF
4560         NEXT X
4570         Y=Y+1
4580       END IF
4590     END IF
4600   WEND
4610   CLOSE #1
4620   IF Y<24 OR NM=0 OR KIND<0 THEN
4630     CALL STATUS("Not a DESIGN listing of a picture (found "+STR$(NM)+" pictures).")
4640     EXIT PROC
4650   END IF
4660   P=-1
4670   FOR I=0 TO 11
4680     IF VAL(MID$(PW$,I+1,1))=W AND VAL(MID$(PH$,I+1,1))=H AND W*H=NM THEN P=I
4690   NEXT I
4700   IF P<0 THEN
4710     FOR I=11 DOWNTO 0
4720       IF VAL(MID$(PW$,I+1,1))*VAL(MID$(PH$,I+1,1))=NM AND VAL(MID$(PW$,I+1,1))>=VAL(MID$(PH$,I+1,1)) THEN P=I
4730     NEXT I
4740   END IF
4750   IF P<0 THEN
4760     CALL STATUS("No block size for "+STR$(NM)+" pictures.")
4770     EXIT PROC
4780   END IF
4790   MD=KIND
4800   IMG=NUM
4810   IF IMG<0 THEN IMG=0
4820   UI VALUE 105+MD,1
4830   CALL PALCOPY
4840   CALL RESIZE(P)
4850   UI VALUE 108,IMG
4860   UI CAPTION 109,STR$(IMG)
4870   BL$=STRING$(BW*BH,0)
4880   FOR K=0 TO NB-1
4890     M$=MM$(K)
4900     CALL PUTMEMBER(K)
4910   NEXT K
4920   UC=0
4930   RC=0
4940   UI IMAGE 100,BL$
4950   DT=0
4960   CALL PREVIEW
4970   CALL STATUS("Opened "+F$+" ("+STR$(GW)+" x "+STR$(GH)+").")
4980 END PROC
4990 ' ====================================================================== what happens
5000 LABEL HANDLER
5010 E=UI("EVENT")
5020 WHILE E<>0
5030   K=UI("KIND")
5040   V=UI("VALUE")
5050   IF E=100 AND K=7 THEN CALL PAINT(V,COL)
5060   IF E=100 AND K=8 THEN CALL PAINT(V,0)
5070   IF E=101 AND K=7 THEN
5080     IF V=0 THEN CALL SETCOL(0) ELSE CALL SETCOL(WB+V)
5090   END IF
5100   IF E=102 THEN
5110     WB=V*16
5120     CALL SHOWWIN
5130   END IF
5140   IF (E=105 OR E=106) AND K=2 THEN
5150     MD=E-105
5160     CALL PALCOPY
5170     CALL SETRANGE
5180     CALL LOADIMG
5190   END IF
5200   IF E=108 THEN
5210     IMG=V
5220     UI CAPTION 109,STR$(V)
5230     CALL LOADIMG
5240   END IF
5250   IF E=126 AND V<>SZ THEN CALL NEWSIZE(V)
5260   IF (E=110 OR E=111) AND K=2 THEN TOOL=E-110
5270   IF E=114 AND K=1 THEN CALL TRANSFORM(3)
5280   IF E=115 AND K=1 THEN CALL TRANSFORM(1)
5290   IF E=116 AND K=1 THEN CALL TRANSFORM(2)
5300   IF E=117 AND K=1 THEN CALL REVERT
5310   IF E=124 AND K=1 THEN CALL DOUNDO
5320   IF E=125 AND K=1 THEN CALL DOREDO
5330   IF E=118 AND K=1 THEN CALL STOREIMG
5340   IF E=119 AND K=1 THEN
5350     F$=FN FR_PICK$("Open a picture",DR$,"*.bas",0)
5360     DR$=FR_DIR$
5370     IF F$<>"" THEN CALL IMPORT(F$)
5380   END IF
5390   IF E=120 AND K=1 THEN
5400     F$=FN FR_PICK$("Save the picture as",DR$,"*.bas",-1)
5410     DR$=FR_DIR$
5420     IF F$<>"" THEN CALL EXPORT(F$)
5430   END IF
5431   IF E=130 AND K=1 THEN
5432     F$=FN FR_PICK$("Import a BMP picture",DR$,"*.[bB][mM][pP]",0)
5433     DR$=FR_DIR$:IF F$<>"" THEN CALL IMPBMP(F$)
5434   END IF
5435   IF E=131 AND K=1 THEN
5436     F$=FN FR_PICK$("Export the picture as BMP",DR$,"*.[bB][mM][pP]",-1)
5437     DR$=FR_DIR$:IF F$<>"" THEN CALL EXPBMP(F$)
5438   END IF
5440   IF E=121 AND K=1 THEN
5450     IF DT=0 THEN
5460       Q=1
5470     ELSE
5480       IF FN UK_ASK("Quit","The picture has changes that were not stored.|Quit anyway?")<>0 THEN Q=1
5490     END IF
5500   END IF
5510   E=UI("EVENT")
5520 WEND
5530 RETURN
6000 ' a BMP file becomes pictures of the engine, starting at the picture Image (a sheet: 16 x 24 cells, row by row), then the block is loaded into the editor
6010 PROC IMPBMP(F$)
6020   LOCAL P
6030   IF DT<>0 THEN
6040     IF FN UK_ASK("Import BMP","The picture has changes that were not stored.|Import anyway?")=0 THEN EXIT PROC
6050   END IF
6060   P=FN UK_ASK("Import BMP","Take over the colors of the file, too?|Yes: its palette becomes the palette of the layer.|No: the colors are matched to the present palette.")
6070   IF MD=0 THEN
6080     IF P THEN SPRITE LOAD F$,IMG,,PALETTE ELSE SPRITE LOAD F$,IMG
6090   ELSE
6100     IF P THEN TILE LOAD F$,IMG,,PALETTE ELSE TILE LOAD F$,IMG
6110   END IF
6120   CALL PALCOPY
6130   CALL SHOWWIN
6140   CALL LOADIMG
6150   CALL STATUS("BMP "+F$+" imported from the picture "+STR$(IMG)+" on.")
6160 END PROC
6170 ' the edited block is stored in the engine, then its pictures are written as a sheet as wide as the block
6180 PROC EXPBMP(F$)
6190   CALL STOREIMG
6200   IF MD=0 THEN SPRITE SAVE F$,IMG,NB,GW ELSE TILE SAVE F$,IMG,NB,GW
6210   CALL STATUS("Exported "+STR$(NB)+" picture(s) to "+F$+" (sheet "+STR$(GW)+" wide).")
6220 END PROC
