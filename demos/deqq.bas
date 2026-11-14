1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' DEQQ'S HOUSE - a graphics adventure for Engine1999.
20 ' Deqq, an alien, crash-landed and lives in an abandoned house. Parts of his spaceship are hidden around the house and the garden, each behind a
30 ' puzzle. Find all eight, carry them to the wreck behind the house and build the ship. But Deqq comes home now and then: while you have
40 ' fewer than four parts built into the wreck he throws you out and bolts the door; hide, or leave in time. With four or more he only watches.
50 ' You type commands (N S E W U D IN OUT, LOOK, EXAMINE, TAKE, DROP, PUT x IN/ON/UNDER y, OPEN, TURN ON/OFF, HIDE, HINT, SAVE ...; HELP lists them).
60 '
70 ' How it is made: the top 4/5 of the screen (640 x 240 pixels of the graphics layer) show the place as a picture of a small 3D world drawn by the sector
80 ' renderer of the engine (W3D): every place is a box of its own (x = west to east 0..600, z = depth 0..400, y = height 0..170) seen from the south, like a
90 ' doll's house; the walls, floors and furniture are textured blocks (the level is written by demos/levels/deqq1.bas), the things you can pick up, the doors
100 ' and drawers, Deqq and the glorp are sprites painted with GFX statements and grabbed as W3D textures. The scene is drawn into the layer that is not shown
110 ' and then swapped in, so nothing flickers. The bottom 5 text rows (80 x 5) are the message window and the input line. Everything is turn-based: only actions
120 ' that change the world pass a turn, and Deqq's comings and goings count turns.
130 ' Window only. Environment for testing: DEQQ_PLAY=file (commands are read from a file), DEQQ_LOG=file (transcript), DEQQ_HEADLESS=1 (no graphics).
140 ' Written in the style of the other demos: PROC / FUNC / CALL / LOCAL, DATA tables for the world, labels for the main loop.
150 LOAD LIBRARY "w3dkit"
160 LOAD LIBRARY "deqqart":LOAD LIBRARY "banner3d":IF ENV$("DEQQ_HEADLESS")="" THEN CALL B3_S3D("DEQQ'S HOUSE")
170 DIM OROOM(130),OIN(130),OREL(130),OSIZE(130),OOPEN(130),OHID(130),OSEEN(130),OFX(130),OFZ(130),OICON(130),OCOL(130),OTYP(130)
180 DIM ONAME$(130),OWORDS$(130),ODESC$(130),OKEY$(130),OPK$(130),OFL$(130)
190 DIM RNAME$(20),RDESC$(20),RKEY$(20),REX$(20),EX(20,10),RSEEN(20),RHIDE$(20)
200 DIM W$(30),LOGT$(300),LOGC(300),PQ$(80),PQC(80),HIST$(20),VW$(200),VID(200)
210 DIM HBR(16),HBG(16),HBB(16),HINT$(10),CMT$(20),DN$(9),SLX(6),SLZ(6),FC(20),PW$(14),PARTK(9),MNX(9),MNY(9),DS(20),WC(9)
220 DIM MHT(16),OSPR(130),AKX(130,4),AKY(130,4),AKZ(130,4)
230 GOTO MAIN
240 ' ------------------------------------------------------------------------------------------------ small helpers
250 FUNC UC$(A$)
260   LOCAL I,C,R$
270   R$=A$
280   FOR I=1 TO LEN(A$)
290     C=ASC(MID$(A$,I,1))
300     IF C>=97 AND C<=122 THEN R$=LEFT$(R$,I-1)+CHR$(C-32)+MID$(R$,I+1)
310   NEXT I
320   RETURN R$
330 END FUNC
340 ' SPOS: position of N$ in H$ (1 = first), 0 if it is not there
350 FUNC SPOS(H$,N$)
360   LOCAL I,L
370   L=LEN(N$)
380   FOR I=1 TO LEN(H$)-L+1
390     IF MID$(H$,I,L)=N$ THEN RETURN I
400   NEXT I
410   RETURN 0
420 END FUNC
430 ' RN: a random number 0 .. N-1
440 FUNC RN(N)
450   RETURN INT(RND(1)*N)
460 END FUNC
470 FUNC MAXI(A,B)
480   IF A>B THEN RETURN A
490   RETURN B
500 END FUNC
510 FUNC MINI(A,B)
520   IF A<B THEN RETURN A
530   RETURN B
540 END FUNC
550 ' the input line cut into words (upper case, only letters and digits); NW = number of words, W$(1..NW)
560 PROC SPLITW(L$)
570   LOCAL I,C,CH$,CUR$
580   NW=0
590   CUR$=""
600   FOR I=1 TO LEN(L$)+1
610     IF I<=LEN(L$) THEN CH$=FN UC$(MID$(L$,I,1)) ELSE CH$=" "
620     C=ASC(CH$)
630     IF (C>=65 AND C<=90) OR (C>=48 AND C<=57) OR C>=192 THEN
640       CUR$=CUR$+CH$
650     ELSE
660       IF CUR$<>"" AND NW<28 THEN
670         NW=NW+1
680         W$(NW)=CUR$
690       END IF
700       CUR$=""
710     END IF
720   NEXT I
730 END PROC
740 ' a word that is only digits?
750 FUNC ISNUM(A$)
760   LOCAL I,C
770   IF A$="" THEN RETURN 0
780   FOR I=1 TO LEN(A$)
790     C=ASC(MID$(A$,I,1))
800     IF C<48 OR C>57 THEN RETURN 0
810   NEXT I
820   RETURN -1
830 END FUNC
840 ' is word W$ in the blank-separated word list L$ (which starts and ends with a blank)?
850 FUNC INLIST(W$,L$)
860   IF W$="" THEN RETURN 0
870   RETURN FN SPOS(L$," "+W$+" ")>0
880 END FUNC
890 ' ------------------------------------------------------------------------------------------------ palette
900 ' Palette index = hue * 16 + shade (shade 0 = nearly black ... 10 = the color ... 15 = pale). Index 0 is transparent. Hue numbers:
910 ' 0 gray, 1 wood, 2 red, 3 orange, 4 yellow, 5 green, 6 teal, 7 sky, 8 blue, 9 purple, 10 pink, 11 cream, 12 violet, 13 steel, 14 lime, 15 glow
920 FUNC CC(H,S)
930   RETURN 16+MHT(H)*16+S
940 END FUNC
950 PROC SETPAL(BR)
960   LOCAL L,H,S,F,R,G,B
970   IF HL THEN EXIT PROC
980   FOR L=0 TO 3 STEP 3
990     RESTORE W3K_HUES
1000     FOR H=0 TO 14
1010       READ R,G,B
1020       FOR S=0 TO 15
1030         F=0.06+0.94*S/15
1040         GFX PALETTE L,16+H*16+S,RGB(FN MINI(255,INT(R*F*BR*TNR)),FN MINI(255,INT(G*F*BR*TNG)),FN MINI(255,INT(B*F*BR*TNB)))
1050       NEXT S
1060     NEXT H
1070     FOR S=1 TO 15
1080       F=0.06+0.94*S/15
1090       GFX PALETTE L,S,RGB(FN MINI(255,INT(170*F*BR*TNR)),FN MINI(255,INT(172*F*BR*TNG)),FN MINI(255,INT(180*F*BR*TNB)))
1100     NEXT S
1110   NEXT L
1120   IF V3ON THEN W3D COLORMAP
1130 END PROC
1140 ' the text layer's colors: 20 paper, 21 text, 22 Deqq (mint), 23 warning (amber), 24 danger (red), 25 hint (cyan), 26 dim, 27 white
1150 PROC TEXTPAL
1160   IF HL THEN EXIT PROC
1170   GFX PALETTE 2,20,RGB(10,12,26)
1180   GFX PALETTE 2,21,RGB(205,210,225)
1190   GFX PALETTE 2,22,RGB(110,255,190)
1200   GFX PALETTE 2,23,RGB(255,190,70)
1210   GFX PALETTE 2,24,RGB(255,90,80)
1220   GFX PALETTE 2,25,RGB(110,200,255)
1230   GFX PALETTE 2,26,RGB(120,125,150)
1240   GFX PALETTE 2,27,RGB(255,255,255)
1250 END PROC
1260 ' ------------------------------------------------------------------------------------------------ double buffer
1270 ' the scene is drawn into the hidden graphics layer (BK) and then swapped in
1280 PROC BEGINDRAW
1290   IF HL THEN EXIT PROC
1300   GFX TARGET BK
1310   GFX CLEAR
1320 END PROC
1330 PROC ENDDRAW
1340   LOCAL T
1350   IF HL THEN EXIT PROC
1360   GFX LAYER BK,ON
1370   GFX LAYER FR,OFF
1380   T=BK:BK=FR:FR=T
1390 END PROC
1400 ' ------------------------------------------------------------------------------------------------ projection
1410 ' room coordinates: x west -> east 0..600, y up 0..170, z front -> back 0..400. The sprites are painted with this old projection (a camera in front of the room).
1420 FUNC PX(X,Z)
1430   RETURN INT(320+(X-300)*0.96*560/(560+Z)+0.5)
1440 END FUNC
1450 FUNC PY(Y,Z)
1460   RETURN INT(-41+(273-Y)*560/(560+Z)+0.5)
1470 END FUNC
1480 FUNC SCALE(Z)
1490   RETURN 560/(560+Z)
1500 END FUNC
1510 ' a filled rectangle with an outline (screen coordinates)
1520 PROC RECTO(X0,Y0,X1,Y1,C)
1530   GFX LINE X0,Y0,X1,Y1,C,BF
1540   GFX LINE X0,Y0,X1,Y1,1,B
1550 END PROC
1560 ' a filled ellipse (screen coordinates)
1570 PROC FELL(CX,CY,RX,RY,C)
1580   LOCAL DY,HW
1590   IF RY<1 THEN RY=1
1600   FOR DY=-RY TO RY
1610     HW=INT(RX*SQR(FN MAXI(0,1-(DY*DY)/(RY*RY)))+0.5)
1620     GFX LINE CX-HW,CY+DY,CX+HW,CY+DY,C
1630   NEXT DY
1640 END PROC
1650 PROC FELLO(CX,CY,RX,RY,C)
1660   CALL FELL(CX,CY,RX+1,RY+1,1)
1670   CALL FELL(CX,CY,RX,RY,C)
1680 END PROC
1690 ' a symmetric trapezoid: rows Y0..Y1, half width W0 at the top and W1 at the bottom, around CX
1700 PROC TRAPH(CX,Y0,Y1,W0,W1,C)
1710   LOCAL Y,HW
1720   FOR Y=Y0 TO Y1
1730     IF Y1>Y0 THEN HW=INT(W0+(W1-W0)*(Y-Y0)/(Y1-Y0)+0.5) ELSE HW=W0
1740     GFX LINE CX-HW,Y,CX+HW,Y,C
1750   NEXT Y
1760   GFX LINE CX-W0,Y0,CX-W1,Y1,1
1770   GFX LINE CX+W0,Y0,CX+W1,Y1,1
1780   GFX LINE CX-W0,Y0,CX+W0,Y0,1
1790   GFX LINE CX-W1,Y1,CX+W1,Y1,1
1800 END PROC
1810 ' text with a shadow on the graphics layer
1820 PROC PUTLBL(X,Y,T$,C)
1830   IF HL THEN EXIT PROC
1840   GFX TEXT X+1,Y+1,T$,1
1850   GFX TEXT X,Y,T$,C
1860 END PROC
1870 ' ------------------------------------------------------------------------------------------------ Deqq
1880 ' Deqq stands at floor position X,Z: a tall cloaked alien with a big head and big black eyes (E = 0 normal, 1 eyes glow brighter)
1890 PROC DEQQFIG(X,Z,E)
1900   LOCAL S,BX,BY,HT,T
1910   IF HL THEN EXIT PROC
1920   S=FN SCALE(Z)
1930   BX=FN PX(X,Z):BY=FN PY(0,Z)
1940   HT=INT(112*S)
1950   CALL TRAPH(BX,BY-HT,BY,INT(9*S)+1,INT(25*S),FN CC(12,5))
1960   GFX LINE BX-INT(3*S),BY-HT+2,BX-INT(8*S),BY-2,FN CC(12,3)
1970   GFX LINE BX-INT(25*S),BY-INT(50*S),BX-INT(20*S),BY-INT(8*S),1
1980   GFX LINE BX+INT(25*S),BY-INT(50*S),BX+INT(20*S),BY-INT(8*S),1
1990   CALL FELLO(BX,BY-HT-INT(16*S),INT(13*S)+1,INT(19*S)+1,FN CC(6,11))
2000   CALL FELLO(BX-INT(6*S),BY-HT-INT(15*S),INT(4*S)+1,INT(7*S)+1,FN CC(0,1))
2010   CALL FELLO(BX+INT(6*S),BY-HT-INT(15*S),INT(4*S)+1,INT(7*S)+1,FN CC(0,1))
2020   T=FN CC(15,12+E*3)
2030   GFX PSET BX-INT(6*S),BY-HT-INT(17*S),T
2040   GFX PSET BX+INT(6*S),BY-HT-INT(17*S),T
2050   GFX PSET BX-INT(5*S),BY-HT-INT(17*S),T
2060   GFX PSET BX+INT(7*S),BY-HT-INT(17*S),T
2070   GFX LINE BX-INT(3*S),BY-HT-INT(5*S),BX+INT(3*S),BY-HT-INT(5*S),FN CC(6,6)
2080 END PROC
2090 ' ------------------------------------------------------------------------------------------------ small things
2100 ' ICON draws a thing that lies or stands at the screen position CX,CY (bottom middle), SC = size (the camera scale of its place), HU = hue
2110 FUNC UU(N)
2120   RETURN INT(N*SCL+0.5)
2130 END FUNC
2140 PROC ICON(K,CX,CY,SC,HU)
2150   LOCAL I,A
2160   IF HL THEN EXIT PROC
2170   SCL=SC*1.15
2180   IF K=1 THEN
2190     ' key: a ring, a shaft and teeth
2200     CALL FELLO(CX-FN UU(6),CY-FN UU(14),FN UU(4),FN UU(4),FN CC(4,12))
2210     GFX LINE CX-FN UU(2),CY-FN UU(14),CX+FN UU(9),CY-FN UU(14),FN CC(4,12)
2220     GFX LINE CX-FN UU(2),CY-FN UU(13),CX+FN UU(9),CY-FN UU(13),FN CC(4,9)
2230     GFX LINE CX+FN UU(7),CY-FN UU(14),CX+FN UU(7),CY-FN UU(9),FN CC(4,12)
2240     GFX LINE CX+FN UU(10),CY-FN UU(14),CX+FN UU(10),CY-FN UU(10),FN CC(4,12)
2250   END IF
2260   IF K=2 THEN
2270     ' crank handle
2280     CALL RECTO(CX-FN UU(9),CY-FN UU(5),CX+FN UU(4),CY-FN UU(2),FN CC(13,9))
2290     CALL RECTO(CX+FN UU(2),CY-FN UU(18),CX+FN UU(5),CY-FN UU(2),FN CC(13,9))
2300     CALL RECTO(CX+FN UU(0),CY-FN UU(23),CX+FN UU(7),CY-FN UU(18),FN CC(2,10))
2310   END IF
2320   IF K=3 THEN
2330     ' screwdriver / hammer: a handle in the hue of HU and a steel shaft
2340     CALL RECTO(CX-FN UU(4),CY-FN UU(22),CX+FN UU(4),CY-FN UU(11),FN CC(HU,10))
2350     GFX LINE CX,CY-FN UU(11),CX,CY-FN UU(2),FN CC(13,13)
2360     GFX LINE CX+1,CY-FN UU(11),CX+1,CY-FN UU(2),FN CC(13,9)
2370   END IF
2380   IF K=4 THEN
2390     ' lamp
2400     CALL RECTO(CX-FN UU(7),CY-FN UU(4),CX+FN UU(7),CY-FN UU(1),FN CC(13,8))
2410     CALL RECTO(CX-FN UU(6),CY-FN UU(18),CX+FN UU(6),CY-FN UU(4),FN CC(HU,13))
2420     CALL RECTO(CX-FN UU(4),CY-FN UU(23),CX+FN UU(4),CY-FN UU(18),FN CC(13,8))
2430     GFX LINE CX,CY-FN UU(23),CX,CY-FN UU(27),1
2440     GFX LINE CX-FN UU(3),CY-FN UU(14),CX+FN UU(3),CY-FN UU(14),FN CC(4,15)
2450   END IF
2460   IF K=5 THEN
2470     ' jar with something in it
2480     CALL RECTO(CX-FN UU(8),CY-FN UU(20),CX+FN UU(8),CY-FN UU(1),FN CC(7,13))
2490     CALL FELL(CX,CY-FN UU(9),FN UU(6),FN UU(6),FN CC(HU,11))
2500     CALL RECTO(CX-FN UU(9),CY-FN UU(24),CX+FN UU(9),CY-FN UU(20),FN CC(13,9))
2510   END IF
2520   IF K=6 THEN
2530     ' bottle
2540     CALL RECTO(CX-FN UU(6),CY-FN UU(18),CX+FN UU(6),CY-FN UU(1),FN CC(HU,13))
2550     CALL RECTO(CX-FN UU(3),CY-FN UU(26),CX+FN UU(3),CY-FN UU(18),FN CC(HU,12))
2560     GFX LINE CX-FN UU(3),CY-FN UU(27),CX+FN UU(3),CY-FN UU(27),FN CC(2,10)
2570   END IF
2580   IF K=7 THEN
2590     ' block of ice with something inside
2600     CALL RECTO(CX-FN UU(11),CY-FN UU(20),CX+FN UU(11),CY-FN UU(1),FN CC(7,14))
2610     GFX LINE CX-FN UU(8),CY-FN UU(17),CX-FN UU(3),CY-FN UU(17),FN CC(0,15)
2620     GFX LINE CX-FN UU(8),CY-FN UU(16),CX-FN UU(8),CY-FN UU(12),FN CC(0,15)
2630     CALL FELLO(CX+FN UU(2),CY-FN UU(9),FN UU(4),FN UU(4),FN CC(4,10))
2640   END IF
2650   IF K=8 THEN
2660     ' compass
2670     CALL FELLO(CX,CY-FN UU(10),FN UU(9),FN UU(9),FN CC(4,11))
2680     CALL FELL(CX,CY-FN UU(10),FN UU(6),FN UU(6),FN CC(11,15))
2690     GFX LINE CX,CY-FN UU(10),CX+FN UU(4),CY-FN UU(15),FN CC(2,10)
2700     GFX LINE CX,CY-FN UU(10),CX-FN UU(4),CY-FN UU(5),FN CC(8,10)
2710   END IF
2720   IF K=9 THEN
2730     ' coil of copper wire
2740     FOR I=0 TO 4
2750       CALL FELLO(CX,CY-FN UU(4+I*4),FN UU(10),FN UU(3),FN CC(3,10-(I AND 1)))
2760     NEXT I
2770     GFX LINE CX-FN UU(10),CY-FN UU(4),CX-FN UU(14),CY-FN UU(2),FN CC(0,3)
2780   END IF
2790   IF K=10 THEN
2800     ' crystal: a glowing diamond
2810     CALL FELL(CX,CY-FN UU(14),FN UU(14),FN UU(14),FN CC(15,6))
2820     CALL TRAPH(CX,CY-FN UU(28),CY-FN UU(14),FN UU(2),FN UU(9),FN CC(9,13))
2830     CALL TRAPH(CX,CY-FN UU(14),CY-FN UU(1),FN UU(9),FN UU(1),FN CC(9,9))
2840     GFX LINE CX-FN UU(3),CY-FN UU(24),CX-FN UU(5),CY-FN UU(16),FN CC(9,15)
2850   END IF
2860   IF K=11 THEN
2870     ' thruster nozzle
2880     CALL TRAPH(CX,CY-FN UU(26),CY-FN UU(2),FN UU(7),FN UU(17),FN CC(13,10))
2890     CALL FELLO(CX,CY-FN UU(3),FN UU(13),FN UU(3),FN CC(13,6))
2900     CALL FELL(CX,CY-FN UU(4),FN UU(9),FN UU(2),FN CC(0,2))
2910     GFX LINE CX-FN UU(5),CY-FN UU(20),CX-FN UU(10),CY-FN UU(6),FN CC(13,15)
2920   END IF
2930   IF K=12 THEN
2940     ' fuel cell: a cylinder
2950     CALL FELL(CX,CY-FN UU(2),FN UU(8),FN UU(3),FN CC(5,8))
2960     CALL RECTO(CX-FN UU(8),CY-FN UU(24),CX+FN UU(8),CY-FN UU(2),FN CC(5,11))
2970     CALL FELLO(CX,CY-FN UU(24),FN UU(8),FN UU(3),FN CC(15,13))
2980     GFX LINE CX-FN UU(3),CY-FN UU(20),CX-FN UU(3),CY-FN UU(6),FN CC(15,15)
2990   END IF
3000   IF K=13 THEN
3010     ' hull plate
3020     CALL RECTO(CX-FN UU(18),CY-FN UU(8),CX+FN UU(18),CY-FN UU(2),FN CC(13,9))
3030     CALL RECTO(CX-FN UU(16),CY-FN UU(14),CX+FN UU(16),CY-FN UU(8),FN CC(13,12))
3040     GFX PSET CX-FN UU(14),CY-FN UU(11),FN CC(0,3)
3050     GFX PSET CX+FN UU(14),CY-FN UU(11),FN CC(0,3)
3060     GFX PSET CX,CY-FN UU(11),FN CC(0,3)
3070   END IF
3080   IF K=14 THEN
3090     ' ignition glyph: a glowing token
3100     CALL FELL(CX,CY-FN UU(11),FN UU(13),FN UU(13),FN CC(15,5))
3110     CALL FELLO(CX,CY-FN UU(11),FN UU(8),FN UU(8),FN CC(15,12))
3120     GFX LINE CX-FN UU(4),CY-FN UU(8),CX,CY-FN UU(15),FN CC(0,2)
3130     GFX LINE CX,CY-FN UU(15),CX+FN UU(4),CY-FN UU(8),FN CC(0,2)
3140     GFX LINE CX-FN UU(2),CY-FN UU(11),CX+FN UU(2),CY-FN UU(11),FN CC(0,2)
3150   END IF
3160   IF K=15 THEN
3170     ' antenna: a rod with a dish
3180     GFX LINE CX,CY-FN UU(2),CX,CY-FN UU(26),FN CC(13,13)
3190     GFX LINE CX+1,CY-FN UU(2),CX+1,CY-FN UU(26),FN CC(13,8)
3200     CALL FELLO(CX,CY-FN UU(22),FN UU(10),FN UU(4),FN CC(13,12))
3210     GFX LINE CX-FN UU(10),CY-FN UU(14),CX+FN UU(10),CY-FN UU(14),FN CC(13,10)
3220     CALL FELL(CX,CY-FN UU(28),FN UU(2),FN UU(2),FN CC(2,11))
3230   END IF
3240   IF K=16 THEN
3250     ' chair
3260     GFX LINE CX-FN UU(12),CY-FN UU(1),CX-FN UU(12),CY-FN UU(17),FN CC(1,7)
3270     GFX LINE CX+FN UU(12),CY-FN UU(1),CX+FN UU(12),CY-FN UU(17),FN CC(1,7)
3280     CALL RECTO(CX-FN UU(14),CY-FN UU(22),CX+FN UU(14),CY-FN UU(16),FN CC(1,11))
3290     CALL RECTO(CX-FN UU(13),CY-FN UU(46),CX-FN UU(9),CY-FN UU(22),FN CC(1,9))
3300     CALL RECTO(CX+FN UU(9),CY-FN UU(46),CX+FN UU(13),CY-FN UU(22),FN CC(1,9))
3310     CALL RECTO(CX-FN UU(13),CY-FN UU(46),CX+FN UU(13),CY-FN UU(38),FN CC(1,10))
3320   END IF
3330   IF K=17 THEN
3340     ' framed photo
3350     CALL RECTO(CX-FN UU(10),CY-FN UU(24),CX+FN UU(10),CY-FN UU(2),FN CC(1,9))
3360     CALL RECTO(CX-FN UU(7),CY-FN UU(21),CX+FN UU(7),CY-FN UU(5),FN CC(7,12))
3370     CALL FELL(CX-FN UU(2),CY-FN UU(14),FN UU(2),FN UU(3),FN CC(6,11))
3380     CALL FELL(CX+FN UU(3),CY-FN UU(14),FN UU(2),FN UU(3),FN CC(5,11))
3390   END IF
3400   IF K=18 THEN
3410     ' paper, letter or book (HU = color)
3420     CALL RECTO(CX-FN UU(10),CY-FN UU(5),CX+FN UU(10),CY-FN UU(2),FN CC(HU,11))
3430     CALL RECTO(CX-FN UU(9),CY-FN UU(8),CX+FN UU(9),CY-FN UU(5),FN CC(11,14))
3440     GFX LINE CX-FN UU(7),CY-FN UU(7),CX+FN UU(7),CY-FN UU(7),FN CC(0,5)
3450   END IF
3460   IF K=19 THEN
3470     ' round thing: fruit, ball, bread, coin (HU = color)
3480     CALL FELLO(CX,CY-FN UU(9),FN UU(8),FN UU(8),FN CC(HU,11))
3490     GFX PSET CX-FN UU(3),CY-FN UU(12),FN CC(HU,15)
3500     GFX PSET CX-FN UU(2),CY-FN UU(12),FN CC(HU,15)
3510   END IF
3520   IF K=20 THEN
3530     ' cloth, folded (HU = color)
3540     CALL RECTO(CX-FN UU(11),CY-FN UU(9),CX+FN UU(11),CY-FN UU(2),FN CC(HU,11))
3550     CALL RECTO(CX-FN UU(9),CY-FN UU(14),CX+FN UU(9),CY-FN UU(9),FN CC(HU,13))
3560   END IF
3570   IF K=21 THEN
3580     ' cup, pan, can (HU = color)
3590     CALL RECTO(CX-FN UU(7),CY-FN UU(14),CX+FN UU(7),CY-FN UU(2),FN CC(HU,10))
3600     GFX LINE CX-FN UU(7),CY-FN UU(14),CX+FN UU(7),CY-FN UU(14),FN CC(0,15)
3610     GFX LINE CX+FN UU(7),CY-FN UU(11),CX+FN UU(11),CY-FN UU(11),1
3620   END IF
3630   IF K=22 THEN
3640     ' umbrella
3650     CALL TRAPH(CX,CY-FN UU(34),CY-FN UU(24),FN UU(2),FN UU(14),FN CC(HU,10))
3660     GFX LINE CX,CY-FN UU(24),CX,CY-FN UU(3),FN CC(0,4)
3670     GFX LINE CX,CY-FN UU(3),CX+FN UU(4),CY-FN UU(2),FN CC(0,4)
3680   END IF
3690   IF K=23 THEN
3700     ' hat
3710     CALL TRAPH(CX,CY-FN UU(22),CY-FN UU(6),FN UU(5),FN UU(9),FN CC(HU,9))
3720     CALL FELLO(CX,CY-FN UU(5),FN UU(14),FN UU(3),FN CC(HU,7))
3730   END IF
3740   IF K=24 THEN
3750     ' pillow
3760     CALL FELLO(CX,CY-FN UU(6),FN UU(13),FN UU(5),FN CC(0,14))
3770   END IF
3780   IF K=25 THEN
3790     ' bucket
3800     CALL TRAPH(CX,CY-FN UU(20),CY-FN UU(2),FN UU(11),FN UU(8),FN CC(1,9))
3810     GFX LINE CX-FN UU(11),CY-FN UU(20),CX,CY-FN UU(30),FN CC(13,9)
3820     GFX LINE CX+FN UU(11),CY-FN UU(20),CX,CY-FN UU(30),FN CC(13,9)
3830   END IF
3840 END PROC
3850 ' ------------------------------------------------------------------------------------------------ the message window (text rows 20..24)
3860 ' SAY adds a message (word-wrapped to 76 columns) to the log; FLUSH shows the last four lines in rows 20..23 (a message of more than four
3870 ' lines is shown in pages, "-- more --"). Colors: 1 normal, 2 Deqq's voice, 3 warning, 4 danger, 5 hint, 6 dim, 7 white.
3880 FUNC TCOL(C)
3890   RETURN 20+C
3900 END FUNC
3910 PROC LOGFILE(T$)
3920   IF LOGON THEN PRINT #3,T$
3930 END PROC
3940 PROC ADDLINE(T$,C)
3950   LOGT$(LOGN MOD 300)=T$
3960   LOGC(LOGN MOD 300)=C
3970   LOGN=LOGN+1
3980   CALL LOGFILE(T$)
3990   IF HL THEN PRINT T$
4000 END PROC
4010 PROC SAY(T$,C)
4020   LOCAL L$,P,Q
4030   L$=T$
4040   WHILE LEN(L$)>76
4050     P=77
4060     WHILE P>1 AND MID$(L$,P,1)<>" "
4070       P=P-1
4080     WEND
4090     IF P<=1 THEN P=77
4100     CALL ADDLINE(LEFT$(L$,P-1),C)
4110     L$=MID$(L$,P)
4120     WHILE LEFT$(L$,1)=" "
4130       L$=MID$(L$,2)
4140     WEND
4150   WEND
4160   CALL ADDLINE(L$,C)
4170 END PROC
4180 PROC TXTROW(R,T$,C)
4190   IF HL THEN EXIT PROC
4200   LOCATE 0,20+R
4210   COLOR FN TCOL(C),20
4220   PRINT " ";LEFT$(T$+SPACE$(78),78);
4230 END PROC
4240 ' shows four log lines ending at log entry E (inclusive); M = 1: the last row says "-- more --"
4250 PROC SHOWTAIL(E,M)
4260   LOCAL R,I
4270   IF HL THEN EXIT PROC
4280   FOR R=0 TO 3
4290     I=E-3+R
4300     IF I>=0 AND I>=LOGN-300 THEN
4310       CALL TXTROW(R,LOGT$(I MOD 300),LOGC(I MOD 300))
4320     ELSE
4330       CALL TXTROW(R,"",1)
4340     END IF
4350   NEXT R
4360   IF M THEN CALL TXTROW(3,"-- more --  (press a key)",3)
4370 END PROC
4380 PROC WAITKEY
4390   LOCAL K$
4400   IF HL OR PLAYON THEN EXIT PROC
4410   REPEAT
4420     K$=INKEY$
4430     IF K$="" THEN WAIT 2
4440   UNTIL K$<>""
4450 END PROC
4460 PROC FLUSH
4470   LOCAL PS
4480   IF HL THEN
4490     PSTART=LOGN
4500     EXIT PROC
4510   END IF
4520   PS=PSTART
4530   WHILE LOGN-PS>4
4540     CALL SHOWTAIL(PS+2,1)
4550     CALL WAITKEY
4560     PS=PS+3
4570   WEND
4580   CALL SHOWTAIL(LOGN-1,0)
4590   PSTART=LOGN
4600 END PROC
4610 ' the text screen as a whole: a dark band for the message window; the rest of the text layer stays empty
4620 PROC TEXTINIT
4630   IF HL THEN EXIT PROC
4640   CALL TEXTPAL
4650   COLOR 21,0
4660   CLS
4670   TXT FILL 0,20,79,24,32,21,20
4680   LOCATE 0,0
4690 END PROC
4700 ' ------------------------------------------------------------------------------------------------ the input line
4710 PROC PROMPT(B$)
4720   IF HL THEN EXIT PROC
4730   LOCATE 0,24
4740   COLOR FN TCOL(7),20
4750   PRINT "> ";LEFT$(B$+"_"+SPACE$(76),77);
4760 END PROC
4770 ' the log on the whole screen (page up): the scene is hidden while it is shown
4780 PROC HISTORY
4790   LOCAL R,I,N
4800   IF HL OR PLAYON THEN EXIT PROC
4810   GFX LAYER FR,OFF
4820   TXT FILL 0,0,79,19,32,21,20
4830   N=FN MINI(23,LOGN)
4840   FOR R=0 TO N-1
4850     I=LOGN-N+R
4860     LOCATE 0,R
4870     COLOR FN TCOL(LOGC(I MOD 300)),20
4880     PRINT " ";LEFT$(LOGT$(I MOD 300)+SPACE$(78),78);
4890   NEXT R
4900   CALL WAITKEY
4910   TXT FILL 0,0,79,19,32,21,0
4920   GFX LAYER FR,ON
4930   CALL SHOWTAIL(LOGN-1,0)
4940 END PROC
4950 ' one line from the player (or from the play-back file when testing)
4960 FUNC GETLINE$
4970   LOCAL B$,K$,C,H,DN
4980   IF PLAYON THEN
4990     IF EOF(4) THEN
5000       PLAYEND=-1
5010       RETURN "quit"
5020     END IF
5030     LINE INPUT #4,B$
5040     RETURN B$
5050   END IF
5060   IF HL THEN
5070     LINE INPUT B$
5080     RETURN B$
5090   END IF
5100   B$=""
5110   H=NH
5120   DN=0
5130   CALL PROMPT(B$)
5140   REPEAT
5150     K$=INKEY$
5160     IF K$="" THEN
5170       WAIT 2
5180     ELSE
5190       IF K$=CHR$(27) THEN
5200         B$="quit"
5210         DN=-1
5220       ELSE
5230       IF K$=CHR$(13) OR K$=CHR$(10) THEN
5240         DN=-1
5250       ELSE
5260         IF K$=CHR$(8) OR K$=CHR$(127) THEN
5270           IF LEN(B$)>0 THEN B$=LEFT$(B$,LEN(B$)-1)
5280         ELSE
5290           IF K$=CHR$(27)+"[A" THEN
5300             IF H>0 THEN
5310               H=H-1
5320               B$=HIST$(H)
5330             END IF
5340           ELSE
5350             IF K$=CHR$(27)+"[B" THEN
5360               IF H<NH-1 THEN
5370                 H=H+1
5380                 B$=HIST$(H)
5390               ELSE
5400                 H=NH
5410                 B$=""
5420               END IF
5430             ELSE
5440               IF K$=CHR$(27)+"[5~" THEN
5450                 CALL HISTORY
5460               ELSE
5470                 IF LEN(K$)=1 AND LEN(B$)<70 AND ASC(K$)>=32 THEN B$=B$+K$
5480               END IF
5490             END IF
5500           END IF
5510         END IF
5520       END IF
5530       END IF
5540       CALL PROMPT(B$)
5550     END IF
5560   UNTIL DN
5570   IF B$<>"" THEN
5580     IF NH>=20 THEN
5590       FOR C=1 TO 19
5600         HIST$(C-1)=HIST$(C)
5610       NEXT C
5620       NH=19
5630     END IF
5640     HIST$(NH)=B$
5650     NH=NH+1
5660   END IF
5670   RETURN B$
5680 END FUNC
5690 ' ------------------------------------------------------------------------------------------------ the world: rooms and things (DATA)
5700 ' Rooms:   DATA key, name, description, exits ("W:WESTG N:WELLG IN:HALL": the directions N S E W U D IN OUT and the room they lead to)
5710 ' Things:  DATA key, name, words, size, flags, room, parent, relation, icon, hue, description
5720 '   size 0 = fixed (can't be taken), 1.. = takes that many of the 6 inventory slots.   relation of a thing inside / on / under its parent: 1 / 2 / 3
5730 '   flags: C = container that opens and closes, A = container that is always open, S = surface (things lie on it), U = things can lie under it,
5740 '          H = a high surface (reach it only standing on a chair), P = a part of the spaceship, X = a place to hide, Z = hidden until it is revealed, O = starts open
5750 LABEL ROOMDATA
5760 DATA "FRONT","Front garden","You stand on the gravel road in front of Deqq's house, a crooked grey building with one round window and a front door in a color that has no name. A signpost leans by the gate. The road runs around the house: west and north.","W:WESTG N:WELLG IN:HALL"
5770 DATA "WESTG","West side of the house","A bare strip of garden along the west wall. A rusty hatch is set into the ground against the wall, a rain barrel and a rotting bench stand by it. The road goes east and north.","E:FRONT N:BACKYD D:CELLAR IN:CELLAR"
5780 DATA "BACKYD","Back yard","Behind the house: a back door, a washing line with grey capes, and a big flowerpot. The road goes south and east. A trampled track leads north into the dark, toward a smell of scorched metal.","S:WESTG E:WELLG N:CRASH IN:KITCHEN"
5790 DATA "WELLG","North-east corner","An old stone well with a windlass stands here, and an apple tree that bears odd fruit. East of the well is a small tool shed. The road runs west and south.","W:BACKYD S:FRONT E:SHED IN:SHED"
5800 DATA "SHED","Tool shed","A cluttered shed that smells of oil. A big blueprint is pinned over the workbench, a red toolbox stands under it and a shelf holds old cans.","W:WELLG OUT:WELLG"
5810 DATA "CRASH","Crash site","A scorched hollow in the ground. In its middle lies the wreck of a spaceship: a skeleton of beams with eight gaps where parts are missing. The only way back is south.","S:BACKYD"
5820 DATA "HALL","Hall","A narrow hall with a creaking floor and a wall clock. The front door is to the south, stairs lead up, a coat closet and a spindly table stand by the wall. Doors go east and west.","E:LIVING W:KITCHEN U:LANDING S:FRONT OUT:FRONT"
5830 DATA "LIVING","Living room","A sagging sofa, a round table, a bookshelf, a television full of static and a very tall cupboard fill the room. The only door leads west, back to the hall.","W:HALL OUT:HALL"
5840 DATA "KITCHEN","Kitchen","A kitchen: fridge, oven, sink, a table with chairs and a pantry. A back door leads north, stone steps go down into the cellar, and the hall is to the east.","E:HALL N:BACKYD OUT:BACKYD D:CELLAR"
5850 DATA "LANDING","Landing","A dim landing. A door to the east, a door to the west, the stairs lead down, and a ladder goes up to the attic. A painting hangs crooked on the wall.","D:HALL E:BEDROOM W:BATH U:ATTIC"
5860 DATA "BEDROOM","Bedroom","A bedroom with a huge bed, a wardrobe, a nightstand and a toy chest. Something under the bed is breathing. The door is to the west.","W:LANDING OUT:LANDING"
5870 DATA "BATH","Bathroom","A bathroom in sickly green tiles: a bathtub with a plastic curtain, a washbasin, a medicine cabinet and a mirror made of something dull. The door is to the east.","E:LANDING OUT:LANDING"
5880 DATA "CELLAR","Cellar","Damp stone walls. In the corner a washing machine rattles, there are barrels, a dusty shelf and a fuse box. Steps lead up to the kitchen, and a ladder to a hatch in the west garden.","U:KITCHEN OUT:WESTG"
5890 DATA "ATTIC","Attic","Under the sloping roof: old boxes, a trunk, a telescope at the skylight, a star globe and a safe set into the chimney wall. The ladder leads down.","D:LANDING"
5900 DATA "END","","",""
5910 LABEL OBJDATA
5920 DATA "FRONTDOORO","the front door"," FRONT DOOR ENTRANCE ",0,"","FRONT","",0,0,0,"The front door."
5930 DATA "MAILBOX","a mailbox"," MAILBOX MAIL BOX LETTERBOX POSTBOX ",0,"C","FRONT","",0,0,0,"A dented mailbox in the shape of a small rocket."
5940 DATA "LETTER","a letter"," LETTER ENVELOPE MAIL ",1,"","","MAILBOX",1,18,11,"An envelope for 'The Resident'. Inside it says: FINAL NOTICE. Your orbit permit has expired. Pay within 3 moons."
5950 DATA "GNOME","a garden gnome"," GNOME STATUE FIGURE ",0,"","FRONT","",0,0,0,"A garden gnome with a red cap. Its painted eyes seem to follow you around the garden. You are almost sure they did not do that before."
5960 DATA "SIGN","a signpost"," SIGN SIGNPOST BOARD ",0,"","FRONT","",0,0,0,"Hand-painted: DEQQ - KEEP OUT. (THIS MEANS YOU.) Below, smaller: Spare key? Look where the plants sit crooked."
5970 DATA "HATCHO","the cellar hatch"," HATCH TRAPDOOR PADLOCK ",0,"","WESTG","",0,0,0,"The hatch."
5980 DATA "BARREL","a rain barrel"," BARREL RAIN WATER ",0,"A","WESTG","",0,0,0,"A rain barrel full of greenish water. Something small swims in it. Probably."
5990 DATA "BENCH","a rotting bench"," BENCH SEAT ",0,"S","WESTG","",0,0,0,"A garden bench with more holes than slats."
6000 DATA "TEACUP","a teacup"," TEACUP CUP MUG ",1,"","","BENCH",2,21,0,"A chipped teacup. The tea in it is still steaming, though nobody has been here for days."
6010 DATA "BACKDOORO","the back door"," BACK DOOR ",0,"","BACKYD","",0,0,0,"The back door."
6020 DATA "POT","a flowerpot"," POT FLOWERPOT PLANT FLOWER ",0,"U","BACKYD","",0,0,0,"A big clay flowerpot with a withered plant. It sits crooked, as if it had been lifted many times."
6030 DATA "KEY","a spare key"," KEY SPARE BRASS ",1,"Z","","POT",3,1,0,"A brass key on a string. The tag says: SHED, BACK DOOR, HATCH. Deqq trusts nobody, not even himself."
6040 DATA "WASHLINE","a washing line"," LINE WASHING CLOTHESLINE CAPES CAPE ",0,"","BACKYD","",0,0,0,"A washing line with three identical grey capes. They move, although there is no wind."
6050 DATA "WELL","the old well"," WELL SHAFT WINDLASS ROPE WINCH CRANK ",0,"","WELLG","",0,0,0,"The well."
6060 DATA "BUCKET","an iron bucket"," BUCKET PAIL ",0,"AZ","WELLG","",0,0,0,"A rusty iron bucket on the well's rope."
6070 DATA "ANTENNA","an antenna"," ANTENNA AERIAL ROD DISH ",1,"P","","BUCKET",1,15,0,"A telescopic antenna with a small dish. One of the eight parts of Deqq's ship: the antenna."
6080 DATA "TREE","an old tree"," TREE BRANCH BRANCHES ",0,"S","WELLG","",0,0,0,"A gnarled tree. Its fruit are not apples. They hum quietly."
6090 DATA "FRUIT","a glowing fruit"," FRUIT APPLE ORANGE ",1,"","","TREE",2,19,3,"A fruit the size of an apple that glows orange from inside. It hums a little. It smells like cinnamon and thunder."
6100 DATA "SHEDDOORO","the shed door"," SHED DOOR PADLOCK ",0,"","WELLG","",0,0,0,"The shed door."
6110 DATA "TOOLBOX","a toolbox"," TOOLBOX BOX TOOLS ",0,"C","SHED","",0,0,0,"A big red toolbox, dented in several places."
6120 DATA "CRANK","a crank handle"," CRANK HANDLE ",1,"","","TOOLBOX",1,2,0,"A rusty crank handle with a red knob. It looks as if it would fit a windlass."
6130 DATA "SCREWDRIVER","a screwdriver"," SCREWDRIVER DRIVER ",1,"","","TOOLBOX",1,3,2,"A screwdriver with a strange four-pointed tip. For alien screws, presumably."
6140 DATA "HAMMER","a hammer"," HAMMER MALLET ",1,"","","TOOLBOX",1,3,1,"A heavy hammer. Its head is warm for no reason."
6150 DATA "SHELF","a shelf"," SHELF SHELVES RACK ",0,"S","SHED","",0,0,0,"A wobbly shelf of old cans."
6160 DATA "PAINT","a can of paint"," CAN PAINT TIN ",1,"","","SHELF",2,21,6,"A can of paint labelled CAMOUFLAGE - LOOKS LIKE NOTHING. The lid is stuck fast."
6170 DATA "BENCHW","a workbench"," WORKBENCH VICE BENCH ",0,"S","SHED","",0,0,0,"A workbench covered in half-finished gadgets that do nothing at all."
6180 DATA "BLUEPRINT","a blueprint"," BLUEPRINT DRAWING PLAN POSTER PLANS ",0,"","SHED","",0,0,0,"A big drawing of a small spaceship. Eight parts are circled and named: FLUX COIL, STAR COMPASS, WARP CRYSTAL, THRUSTER NOZZLE, FUEL CELL, HULL PLATE, IGNITION GLYPH, ANTENNA. A note: 'Build at the wreck. All eight. Then go home.'"
6190 DATA "SHIP","the wreck"," SHIP WRECK SPACECRAFT SPACESHIP CRAFT ROCKET BEAMS ",0,"","CRASH","",0,0,0,"The wreck."
6200 DATA "HALLDOOR","the front door"," FRONT DOOR ENTRANCE ",0,"","HALL","",0,0,0,"The front door."
6210 DATA "HALLTABLE","a spindly table"," TABLE SPINDLY ",0,"S","HALL","",0,0,0,"A spindly table by the door. One leg is shorter than the others and propped on a folded paper."
6220 DATA "LAMP","a lantern"," LAMP LANTERN LIGHT TORCH FLASHLIGHT ",1,"","","HALLTABLE",2,4,4,"A lantern with a small glass chimney and a switch on its base."
6230 DATA "CLOSET","a coat closet"," CLOSET COATS CUPBOARD ",0,"CX","HALL","",0,0,0,"A narrow closet full of identical grey capes. Plenty of room behind them to stand very still."
6240 DATA "UMBRELLA","an umbrella"," UMBRELLA BROLLY ",1,"","","CLOSET",1,22,2,"A black umbrella with a hook handle and a faint smell of ozone."
6250 DATA "CLOCK","a wall clock"," CLOCK WATCH ",0,"","HALL","",0,0,0,"A wall clock with eleven hours and no minute hand. It says: a bit later."
6260 DATA "STAIRSH","the stairs"," STAIRS STAIRCASE STEPS ",0,"","HALL","",0,0,0,"Wooden stairs lead up to the landing. The third step creaks. You know because you can hear it from here."
6270 DATA "SOFA","a sofa"," SOFA COUCH SETTEE ",0,"XU","LIVING","",0,0,0,"A saggy sofa with a pattern that changes when you look away. Behind it there is room enough to hide."
6280 DATA "COIN","a strange coin"," COIN MONEY ",1,"Z","","SOFA",3,19,4,"A coin with a hole in the middle and another hole at the edge."
6290 DATA "CUPBOARD","a tall cupboard"," CUPBOARD CABINET DRESSER SIDEBOARD ",0,"CH","LIVING","",0,0,0,"A very tall cupboard of dark wood, much taller than a person. The top is out of reach."
6300 DATA "CAPE","a grey cape"," CAPE CLOAK ",1,"","","CUPBOARD",1,20,0,"A grey cape. They all look the same. This one smells slightly of Deqq."
6310 DATA "CRYSTAL","a warp crystal"," CRYSTAL WARP GEM STONE ",1,"P","","CUPBOARD",2,10,9,"A fist-sized crystal that glows violet. The air around it ripples. One of the eight parts of Deqq's ship: the warp crystal."
6320 DATA "LTABLE","a round table"," TABLE ROUND ",0,"S","LIVING","",0,0,0,"A round table with a lace cloth. It's set for one. One what is unclear."
6330 DATA "SANDWICH","a sandwich"," SANDWICH SNACK ",1,"","","LTABLE",2,19,3,"A sandwich of uncertain filling. The filling moves when you look away."
6340 DATA "BOOKSHELF","a bookshelf"," BOOKSHELF SHELF SHELVES BOOKS ",0,"S","LIVING","",0,0,0,"A bookshelf of crooked books in many alphabets."
6350 DATA "PHOTO","a framed photo"," PHOTO PHOTOGRAPH FRAME PICTURE ",1,"","","BOOKSHELF",2,17,0,"A framed photo of three tall figures in front of a silver ship, waving. The frame is old and heavy; the back is held by little clips."
6360 DATA "BOOK","a thick book"," BOOK COOKBOOK ",1,"","","BOOKSHELF",2,18,2,"THE JOY OF GLORPS. Chapter 4: 'Feeding. A glorp is loyal to whoever feeds it green jelly. Never give it anything else. Never.'"
6370 DATA "TV","a television"," TV TELEVISION SCREEN ",0,"","LIVING","",0,0,0,"A television showing static. Now and then the static forms a face. It looks bored."
6380 DATA "FRIDGE","the fridge"," FRIDGE REFRIGERATOR ICEBOX FREEZER ",0,"C","KITCHEN","",0,0,0,"A tall fridge that hums in two keys at once."
6390 DATA "MILK","a bottle of milk"," MILK BOTTLE ",1,"","","FRIDGE",1,6,0,"A bottle of blue milk. Not from cows, and probably not from anything on this planet."
6400 DATA "JELLY","green jelly"," JELLY JAR GREEN ",1,"","","FRIDGE",1,5,5,"A jar of wobbly green jelly. It wobbles when nothing touches it. The label: GLORP SNACK. Handle with respect."
6410 DATA "ICE","a block of ice"," ICE BLOCK ICECUBE CUBE ",1,"","","FRIDGE",1,7,0,"A block of ice as big as a loaf of bread. Something small and metallic is frozen inside it."
6420 DATA "COMPASS","a star compass"," COMPASS STAR NEEDLE ",1,"P","","ICE",1,8,0,"A brass compass whose needle points at a star that is not in this sky. One of the eight parts of Deqq's ship: the star compass."
6430 DATA "OVEN","the oven"," OVEN STOVE COOKER KNOB ",0,"C","KITCHEN","",0,0,0,"The oven."
6440 DATA "COIL","a flux coil"," COIL FLUX COPPER ",1,"P","","OVEN",1,9,0,"A coil of thick copper wire around a glowing core. One of the eight parts of Deqq's ship: the flux coil."
6450 DATA "SINK","the sink"," SINK BASIN ",0,"A","KITCHEN","",0,0,0,"A deep stone sink."
6460 DATA "TAP","the tap"," TAP FAUCET WATER ",0,"","KITCHEN","",0,0,0,"The tap."
6470 DATA "KTABLE","a kitchen table"," TABLE KITCHEN ",0,"S","KITCHEN","",0,0,0,"A scrubbed wooden table with scorch marks in the shape of a hand."
6480 DATA "BREAD","a loaf of bread"," BREAD LOAF ",1,"","","KTABLE",2,19,11,"A loaf of bread, very hard. It has been here since before the crash."
6490 DATA "CHAIR","a wooden chair"," CHAIR STOOL ",2,"","KITCHEN","",0,16,0,"A sturdy wooden chair. Heavy, but you can carry it, if you have two free slots. You could stand on it."
6500 DATA "KCUPBOARD","a kitchen cupboard"," CUPBOARD CABINET ",0,"C","KITCHEN","",0,0,0,"A low kitchen cupboard with a sticking door."
6510 DATA "PAN","a frying pan"," PAN FRYINGPAN ",1,"","","KCUPBOARD",1,21,13,"A heavy frying pan. It has a dent the shape of a head."
6520 DATA "PANTRY","a pantry"," PANTRY LARDER ",0,"CX","KITCHEN","",0,0,0,"A narrow pantry with shelves of tins. Behind the sacks there is just room for a person."
6530 DATA "FLOUR","a bag of flour"," FLOUR BAG SACK ",1,"","","PANTRY",1,20,11,"A bag of flour. Or something white and powdery that behaves like flour."
6540 DATA "BACKDOORI","the back door"," BACK DOOR ",0,"","KITCHEN","",0,0,0,"The back door."
6550 DATA "CSTAIRS","the cellar steps"," STEPS STAIRS CELLAR ",0,"","KITCHEN","",0,0,0,"Stone steps lead down into the dark."
6560 DATA "LINEN","a linen chest"," CHEST LINEN BASKET ",0,"CX","LANDING","",0,0,0,"A big linen chest. There is room for a person inside, if that person is thin and unhappy."
6570 DATA "SHEET","a bed sheet"," SHEET LINEN ",1,"","","LINEN",1,20,0,"A bed sheet with a hole in it. The hole is the shape of a ghost."
6580 DATA "LADDER","the attic ladder"," LADDER ",0,"","LANDING","",0,0,0,"A wooden ladder leads up to the attic."
6590 DATA "PICTURE","a painting"," PAINTING PICTURE ART ",0,"","LANDING","",0,0,0,"A painting of this house. In the painting the house has one more window than the real one. The painted window is lit."
6600 DATA "BED","a big bed"," BED MATTRESS ",0,"SU","BEDROOM","",0,0,0,"A huge bed with a blanket in a color that hurts a little."
6610 DATA "PILLOW","a pillow"," PILLOW CUSHION ",1,"","","BED",2,24,0,"A star-shaped pillow. It's damp. From what, you'd rather not know."
6620 DATA "GLORP","a glorp"," GLORP CREATURE BLOB PET MONSTER ",0,"Z","","BED",3,0,0,"A glorp."
6630 DATA "NOZZLE","a thruster nozzle"," NOZZLE THRUSTER ENGINE BELL ",2,"PZ","","BED",3,11,0,"A bell-shaped nozzle, scorched black inside. One of the eight parts of Deqq's ship: the thruster nozzle. It is heavy: it takes two slots."
6640 DATA "WARDROBE","a wardrobe"," WARDROBE CLOSET ",0,"CX","BEDROOM","",0,0,0,"A dark wardrobe full of grey capes. There is room to stand between them."
6650 DATA "HAT","a top hat"," HAT TOPHAT ",1,"","","WARDROBE",1,23,9,"A purple top hat. Something inside it purrs."
6660 DATA "NIGHTSTAND","a nightstand"," NIGHTSTAND STAND DRAWER ",0,"C","BEDROOM","",0,0,0,"A little nightstand with a drawer."
6670 DATA "NOTE","a crumpled note"," NOTE PAPER LIST ",1,"","","NIGHTSTAND",1,18,11,"A shopping list in curly writing: GREEN JELLY (for the glorp - it eats nothing else!), BLUE MILK, 3 BATTERIES, PATIENCE."
6680 DATA "TOYCHEST","a toy chest"," TOYCHEST CHEST TOYS ",0,"C","BEDROOM","",0,0,0,"A wooden toy chest painted with stars."
6690 DATA "TOYROCKET","a toy rocket"," ROCKET TOY TOYROCKET ",1,"","","TOYCHEST",1,19,2,"A tin toy rocket. Disappointingly NOT one of the eight parts. Wind it up and it plays a tune."
6700 DATA "BALL","a rubber ball"," BALL RUBBER ",1,"","","TOYCHEST",1,19,2,"A rubber ball. It bounces a little too high."
6710 DATA "TUB","a bathtub"," TUB BATHTUB CURTAIN SHOWER ",0,"X","BATH","",0,0,0,"A bathtub with a plastic curtain. The curtain would hide a person nicely."
6720 DATA "MIRROR","a mirror"," MIRROR GLASS SCREWS SCREW ",0,"","BATH","",0,0,0,"The mirror."
6730 DATA "PLATE","a hull plate"," PLATE HULL METAL PANEL ",2,"PZ","BATH","",0,13,0,"A curved metal plate, polished like a mirror on one side and burnt on the other. One of the eight parts of Deqq's ship: the hull plate. It is big: it takes two slots."
6740 DATA "BSINK","a washbasin"," BASIN WASHBASIN SINK ",0,"A","BATH","",0,0,0,"A green washbasin."
6750 DATA "SOAP","a bar of soap"," SOAP BAR ",1,"","","BSINK",1,19,14,"A bar of soap that smells of lime and mild disappointment."
6760 DATA "CABINET","a medicine cabinet"," CABINET MEDICINE ",0,"C","BATH","",0,0,0,"A medicine cabinet over the basin."
6770 DATA "PILLS","some pills"," PILLS BOTTLE ",1,"","","CABINET",1,6,9,"A bottle of purple pills. The label: DO NOT. That is all it says."
6780 DATA "TOWEL","a towel"," TOWEL ",1,"","BATH","",0,20,8,"A blue towel. It's still damp. Tick. Tock."
6790 DATA "WASHER","the washing machine"," WASHER WASHING MACHINE LAUNDRY DRUM ",0,"C","CELLAR","",0,0,0,"The washing machine."
6800 DATA "FUELCELL","a fuel cell"," CELL FUEL BATTERY CYLINDER ",1,"P","","WASHER",1,12,5,"A glowing green cylinder, warm and humming. One of the eight parts of Deqq's ship: the fuel cell."
6810 DATA "SOCKS","some wet socks"," SOCKS SOCK ",1,"","","WASHER",1,20,5,"Three wet socks. They are all left ones."
6820 DATA "PLUG","the plug"," PLUG CABLE CORD SOCKET ",0,"","CELLAR","",0,0,0,"The plug."
6830 DATA "CBARRELS","some barrels"," BARRELS BARREL CASKS ",0,"X","CELLAR","",0,0,0,"A few old barrels. Between them and the wall there is room to hide."
6840 DATA "CSHELF","a dusty shelf"," SHELF SHELVES ",0,"S","CELLAR","",0,0,0,"A dusty shelf."
6850 DATA "PICKLES","a jar of pickles"," PICKLES JAR ",1,"","","CSHELF",2,5,14,"A jar of pickles. The pickles are looking back."
6860 DATA "FUSEBOX","a fuse box"," FUSEBOX FUSE FUSES SWITCH ",0,"","CELLAR","",0,0,0,"A fuse box with fuses labelled in dots. One of them says DO NOT. You don't."
6870 DATA "HATCHI","the hatch"," HATCH LADDER ",0,"","CELLAR","",0,0,0,"The hatch."
6880 DATA "SAFE","a safe"," SAFE VAULT DIAL ",0,"C","ATTIC","",0,0,0,"The safe."
6890 DATA "GLYPH","an ignition glyph"," GLYPH IGNITION TOKEN ",1,"P","","SAFE",1,14,0,"A palm-sized token with a glowing symbol. One of the eight parts of Deqq's ship: the ignition glyph."
6900 DATA "TRUNK","an old trunk"," TRUNK CHEST ",0,"CX","ATTIC","",0,0,0,"An old trunk with brass corners, big enough to hide in."
6910 DATA "SCARF","a moth-eaten scarf"," SCARF ",1,"","","TRUNK",1,20,3,"A scarf, eaten by moths. Large ones: the holes are the size of coins."
6920 DATA "TELESCOPE","a telescope"," TELESCOPE SCOPE ",0,"","ATTIC","",0,0,0,"A brass telescope aimed at one particular star. Through it you see a small blue light, far away, blinking. It's a light of home."
6930 DATA "GLOBE","a star globe"," GLOBE MAP STARS HOLOGRAM ",0,"","ATTIC","",0,0,0,"A hologram globe of stars. One is marked with a tiny house. Another with a tiny question mark. The question mark seems to be looking at you."
6940 DATA "DEQQ","Deqq"," DEQQ ALIEN HOST OWNER ",0,"","","",0,0,0,"Deqq."
6950 DATA "END","","",0,"","","",0,0,0,""
6960 ' how Deqq talks (the translator box), what he mutters and the creepy remarks
6970 LABEL CMTDATA
6980 DATA "You are being watched by an otherworldly presence."
6990 DATA "Somewhere, something breathes in time with you."
7000 DATA "The shadows in the corner lean a little closer."
7010 DATA "Deqq says nothing. His eyes say a great deal."
7020 DATA "The translator crackles: 'Careful. The house remembers where things go.'"
7030 DATA "You feel a stare on the back of your neck, from a long way up."
7040 DATA "A cold finger of thought touches your mind and goes away again."
7050 DATA "Deqq tilts his head the wrong way. Then the right way."
7060 DATA "The floor creaks behind you. Only Deqq is standing there. Nobody else."
7070 DATA "The translator hums: 'Take your time. I have all of mine.'"
7080 DATA "Deqq blinks. It takes much longer than a blink should."
7090 DATA "'Interesting,' says a voice in your head. It isn't yours."
7100 DATA "END"
7110 ' ------------------------------------------------------------------------------------------------ reading the world
7120 FUNC OID(K$)
7130   LOCAL I
7140   FOR I=1 TO NOBJ
7150     IF OKEY$(I)=K$ THEN RETURN I
7160   NEXT I
7170   PRINT "Internal error: no thing called ";K$
7180   RETURN 0
7190 END FUNC
7200 FUNC RID(K$)
7210   LOCAL I
7220   FOR I=1 TO NROOM
7230     IF RKEY$(I)=K$ THEN RETURN I
7240   NEXT I
7250   IF K$<>"" THEN PRINT "Internal error: no room called ";K$
7260   RETURN 0
7270 END FUNC
7280 ' direction number of a name: N S E W U D IN OUT = 1..8
7290 FUNC DIRNUM(D$)
7300   LOCAL I
7310   FOR I=1 TO 8
7320     IF D$=DN$(I) THEN RETURN I
7330   NEXT I
7340   RETURN 0
7350 END FUNC
7360 ' puts thing I on one of the six places on the floor of room R in front of the furniture
7370 PROC FLOORSPOT(I,R)
7380   LOCAL N
7390   N=FC(R) MOD 6
7400   FC(R)=FC(R)+1
7410   OFX(I)=SLX(N):OFZ(I)=SLZ(N)
7420 END PROC
7430 PROC LOADWORLD
7440   LOCAL K$,N$,D$,E$,WD$,SZ,FL$,RK$,PK$,RL,IC,HU,I,J,P,T$,Q$
7450   DN$(1)="N":DN$(2)="S":DN$(3)="E":DN$(4)="W":DN$(5)="U":DN$(6)="D":DN$(7)="IN":DN$(8)="OUT"
7460   SLX(0)=110:SLX(1)=240:SLX(2)=370:SLX(3)=500:SLX(4)=170:SLX(5)=430
7470   SLZ(0)=60:SLZ(1)=70:SLZ(2)=60:SLZ(3)=70:SLZ(4)=130:SLZ(5)=130
7480   RESTORE ROOMDATA
7490   NROOM=0
7500   READ K$,N$,D$,E$
7510   WHILE K$<>"END"
7520     NROOM=NROOM+1
7530     RKEY$(NROOM)=K$:RNAME$(NROOM)=N$:RDESC$(NROOM)=D$:REX$(NROOM)=E$
7540     READ K$,N$,D$,E$
7550   WEND
7560   FOR I=1 TO NROOM
7570     E$=REX$(I)+" "
7580     WHILE LEN(E$)>1
7590       P=FN SPOS(E$," ")
7600       T$=LEFT$(E$,P-1)
7610       E$=MID$(E$,P+1)
7620       Q$=T$
7630       J=FN SPOS(Q$,":")
7640       IF J>0 THEN EX(I,FN DIRNUM(LEFT$(Q$,J-1)))=FN RID(MID$(Q$,J+1))
7650     WEND
7660   NEXT I
7670   RESTORE OBJDATA
7680   NOBJ=0
7690   READ K$,N$,WD$,SZ,FL$,RK$,PK$,RL,IC,HU,D$
7700   WHILE K$<>"END"
7710     NOBJ=NOBJ+1
7720     OKEY$(NOBJ)=K$:ONAME$(NOBJ)=N$:OWORDS$(NOBJ)=WD$:OSIZE(NOBJ)=SZ:OFL$(NOBJ)=FL$
7730     OPK$(NOBJ)=PK$:OREL(NOBJ)=RL:OICON(NOBJ)=IC:OCOL(NOBJ)=HU:ODESC$(NOBJ)=D$
7740     OROOM(NOBJ)=FN RID(RK$)
7750     OHID(NOBJ)=0
7760     IF FN SPOS(FL$,"Z")>0 THEN OHID(NOBJ)=-1
7770     OOPEN(NOBJ)=0
7780     IF FN SPOS(FL$,"O")>0 THEN OOPEN(NOBJ)=-1
7790     READ K$,N$,WD$,SZ,FL$,RK$,PK$,RL,IC,HU,D$
7800   WEND
7810   FOR I=1 TO NOBJ
7820     IF OPK$(I)<>"" THEN
7830       OIN(I)=FN OID(OPK$(I))
7840       OROOM(I)=0
7850     ELSE
7860       OIN(I)=0
7870       IF OROOM(I)>0 AND OSIZE(I)>0 THEN CALL FLOORSPOT(I,OROOM(I))
7880     END IF
7890   NEXT I
7900   RESTORE VERBDATA
7910   NVERB=0
7920   READ K$,P
7930   WHILE K$<>"END"
7940     NVERB=NVERB+1
7950     VW$(NVERB)=K$:VID(NVERB)=P
7960     READ K$,P
7970   WEND
7980   RESTORE CMTDATA
7990   NCMT=0
8000   READ K$
8010   WHILE K$<>"END"
8020     CMT$(NCMT)=K$
8030     NCMT=NCMT+1
8040     READ K$
8050   WEND
8060   ' the constants: R_<room> and K_<thing>
8070   R_FRONT=FN RID("FRONT")
8080   R_WESTG=FN RID("WESTG")
8090   R_BACKYD=FN RID("BACKYD")
8100   R_WELLG=FN RID("WELLG")
8110   R_SHED=FN RID("SHED")
8120   R_CRASH=FN RID("CRASH")
8130   R_HALL=FN RID("HALL")
8140   R_LIVING=FN RID("LIVING")
8150   R_KITCHEN=FN RID("KITCHEN")
8160   R_LANDING=FN RID("LANDING")
8170   R_BEDROOM=FN RID("BEDROOM")
8180   R_BATH=FN RID("BATH")
8190   R_CELLAR=FN RID("CELLAR")
8200   R_ATTIC=FN RID("ATTIC")
8210   K_FRONTDOORO=FN OID("FRONTDOORO")
8220   K_MAILBOX=FN OID("MAILBOX")
8230   K_LETTER=FN OID("LETTER")
8240   K_GNOME=FN OID("GNOME")
8250   K_SIGN=FN OID("SIGN")
8260   K_HATCHO=FN OID("HATCHO")
8270   K_BARREL=FN OID("BARREL")
8280   K_BENCH=FN OID("BENCH")
8290   K_TEACUP=FN OID("TEACUP")
8300   K_BACKDOORO=FN OID("BACKDOORO")
8310   K_POT=FN OID("POT")
8320   K_KEY=FN OID("KEY")
8330   K_WASHLINE=FN OID("WASHLINE")
8340   K_WELL=FN OID("WELL")
8350   K_BUCKET=FN OID("BUCKET")
8360   K_ANTENNA=FN OID("ANTENNA")
8370   K_TREE=FN OID("TREE")
8380   K_FRUIT=FN OID("FRUIT")
8390   K_SHEDDOORO=FN OID("SHEDDOORO")
8400   K_TOOLBOX=FN OID("TOOLBOX")
8410   K_CRANK=FN OID("CRANK")
8420   K_SCREWDRIVER=FN OID("SCREWDRIVER")
8430   K_HAMMER=FN OID("HAMMER")
8440   K_SHELF=FN OID("SHELF")
8450   K_PAINT=FN OID("PAINT")
8460   K_BENCHW=FN OID("BENCHW")
8470   K_BLUEPRINT=FN OID("BLUEPRINT")
8480   K_SHIP=FN OID("SHIP")
8490   K_HALLDOOR=FN OID("HALLDOOR")
8500   K_HALLTABLE=FN OID("HALLTABLE")
8510   K_LAMP=FN OID("LAMP")
8520   K_CLOSET=FN OID("CLOSET")
8530   K_UMBRELLA=FN OID("UMBRELLA")
8540   K_CLOCK=FN OID("CLOCK")
8550   K_STAIRSH=FN OID("STAIRSH")
8560   K_SOFA=FN OID("SOFA")
8570   K_COIN=FN OID("COIN")
8580   K_CUPBOARD=FN OID("CUPBOARD")
8590   K_CAPE=FN OID("CAPE")
8600   K_CRYSTAL=FN OID("CRYSTAL")
8610   K_LTABLE=FN OID("LTABLE")
8620   K_SANDWICH=FN OID("SANDWICH")
8630   K_BOOKSHELF=FN OID("BOOKSHELF")
8640   K_PHOTO=FN OID("PHOTO")
8650   K_BOOK=FN OID("BOOK")
8660   K_TV=FN OID("TV")
8670   K_FRIDGE=FN OID("FRIDGE")
8680   K_MILK=FN OID("MILK")
8690   K_JELLY=FN OID("JELLY")
8700   K_ICE=FN OID("ICE")
8710   K_COMPASS=FN OID("COMPASS")
8720   K_OVEN=FN OID("OVEN")
8730   K_COIL=FN OID("COIL")
8740   K_SINK=FN OID("SINK")
8750   K_TAP=FN OID("TAP")
8760   K_KTABLE=FN OID("KTABLE")
8770   K_BREAD=FN OID("BREAD")
8780   K_CHAIR=FN OID("CHAIR")
8790   K_KCUPBOARD=FN OID("KCUPBOARD")
8800   K_PAN=FN OID("PAN")
8810   K_PANTRY=FN OID("PANTRY")
8820   K_FLOUR=FN OID("FLOUR")
8830   K_BACKDOORI=FN OID("BACKDOORI")
8840   K_CSTAIRS=FN OID("CSTAIRS")
8850   K_LINEN=FN OID("LINEN")
8860   K_SHEET=FN OID("SHEET")
8870   K_LADDER=FN OID("LADDER")
8880   K_PICTURE=FN OID("PICTURE")
8890   K_BED=FN OID("BED")
8900   K_PILLOW=FN OID("PILLOW")
8910   K_GLORP=FN OID("GLORP")
8920   K_NOZZLE=FN OID("NOZZLE")
8930   K_WARDROBE=FN OID("WARDROBE")
8940   K_HAT=FN OID("HAT")
8950   K_NIGHTSTAND=FN OID("NIGHTSTAND")
8960   K_NOTE=FN OID("NOTE")
8970   K_TOYCHEST=FN OID("TOYCHEST")
8980   K_TOYROCKET=FN OID("TOYROCKET")
8990   K_BALL=FN OID("BALL")
9000   K_TUB=FN OID("TUB")
9010   K_MIRROR=FN OID("MIRROR")
9020   K_PLATE=FN OID("PLATE")
9030   K_BSINK=FN OID("BSINK")
9040   K_SOAP=FN OID("SOAP")
9050   K_CABINET=FN OID("CABINET")
9060   K_PILLS=FN OID("PILLS")
9070   K_TOWEL=FN OID("TOWEL")
9080   K_WASHER=FN OID("WASHER")
9090   K_FUELCELL=FN OID("FUELCELL")
9100   K_SOCKS=FN OID("SOCKS")
9110   K_PLUG=FN OID("PLUG")
9120   K_CBARRELS=FN OID("CBARRELS")
9130   K_CSHELF=FN OID("CSHELF")
9140   K_PICKLES=FN OID("PICKLES")
9150   K_FUSEBOX=FN OID("FUSEBOX")
9160   K_HATCHI=FN OID("HATCHI")
9170   K_SAFE=FN OID("SAFE")
9180   K_GLYPH=FN OID("GLYPH")
9190   K_TRUNK=FN OID("TRUNK")
9200   K_SCARF=FN OID("SCARF")
9210   K_TELESCOPE=FN OID("TELESCOPE")
9220   K_GLOBE=FN OID("GLOBE")
9230   K_DEQQ=FN OID("DEQQ")
9240 END PROC
9250 ' ------------------------------------------------------------------------------------------------ the words of the commands
9260 ' verbs: 1..8 the directions N S E W U D IN OUT, 9 go, 10 look, 11 examine, 12 take, 13 drop, 14 inventory, 15 open, 16 close, 17 turn,
9270 ' 18 push, 19 pull, 20 climb / stand, 21 hide, 22 wait, 23 give / feed, 24 read, 25 unlock, 26 dial a code, 27 install, 28 help, 29 hint,
9280 ' 30 save, 31 load, 32 quit, 33 status, 34 put, 35 unplug, 36 unscrew / remove, 37 search, 38 listen, 39 smell, 40 leave, 41 light, 42 talk,
9290 ' 43 eat, 44 noise (jump, sing ...), 45 look through / peek, 46 knock, 47 lock, 48 wind (wind up, crank)
9300 LABEL VERBDATA
9310 DATA "N",1,"NORTH",1,"S",2,"SOUTH",2,"E",3,"EAST",3,"W",4,"WEST",4,"U",5,"UP",5,"UPSTAIRS",5,"D",6,"DOWN",6,"DOWNSTAIRS",6,"IN",7,"INSIDE",7,"ENTER",7,"OUT",8,"OUTSIDE",8
9320 DATA "GO",9,"WALK",9,"RUN",9,"HEAD",9,"MOVE",18
9330 DATA "LOOK",10,"L",10,"EXAMINE",11,"X",11,"INSPECT",11,"CHECK",11,"STUDY",11,"DESCRIBE",11,"WATCH",11
9340 DATA "TAKE",12,"GET",12,"GRAB",12,"PICK",12,"CARRY",12,"HOLD",12,"STEAL",12,"COLLECT",12
9350 DATA "DROP",13,"DISCARD",13,"RELEASE",13,"THROW",13,"PLACE",34,"PUT",34,"INSERT",34,"STORE",34,"LAY",34,"STASH",34
9360 DATA "INVENTORY",14,"INV",14,"I",14,"ITEMS",14,"POCKETS",14
9370 DATA "OPEN",15,"CLOSE",16,"SHUT",16
9380 DATA "TURN",17,"SWITCH",17,"FLIP",17,"TOGGLE",17,"ROTATE",17,"TWIST",17,"STOP",17,"START",17,"PRESS",17,"PUSH",18,"SHOVE",18,"PULL",19,"YANK",19,"TUG",19
9390 DATA "CLIMB",20,"STAND",20,"MOUNT",20,"HIDE",21,"CONCEAL",21,"WAIT",22,"Z",22,"REST",22,"SLEEP",22
9400 DATA "GIVE",23,"FEED",23,"OFFER",23,"HAND",23,"READ",24,"UNLOCK",25,"DIAL",26,"TYPE",26,"PUNCH",26,"SET",26
9410 DATA "INSTALL",27,"ATTACH",27,"FIT",27,"BUILD",27,"REPAIR",27,"USE",27,"CONNECT",27
9420 DATA "HELP",28,"COMMANDS",28,"?",28,"HINT",29,"CLUE",29,"SAVE",30,"LOAD",31,"RESTORE",31,"QUIT",32,"BYE",32,"EXIT",40,"LEAVE",40,"COME",40
9430 DATA "STATUS",33,"SCORE",33,"PARTS",33,"PROGRESS",33,"UNPLUG",35,"DISCONNECT",35,"UNSCREW",36,"REMOVE",36,"DETACH",36,"DISMANTLE",36
9440 DATA "SEARCH",37,"LISTEN",38,"SMELL",39,"SNIFF",39,"LIGHT",41,"IGNITE",41,"TALK",42,"SPEAK",42,"ASK",42,"SAY",42,"GREET",42,"HELLO",42
9450 DATA "EAT",43,"TASTE",43,"DRINK",43,"LICK",43,"JUMP",44,"DANCE",44,"SING",44,"SHOUT",44,"YELL",44,"SCREAM",44,"WHISTLE",44,"PEEK",45
9460 DATA "KNOCK",46,"LOCK",47,"WIND",48,"CRANK",48,"END",0
9470 ' ------------------------------------------------------------------------------------------------ what can be seen and reached
9480 ' ROOMOF: the room of a thing (-1 = carried, 0 = nowhere, 100 = built into the wreck); things inside other things follow their parent
9490 FUNC ROOMOF(I)
9500   IF OIN(I)>0 THEN RETURN FN ROOMOF(OIN(I))
9510   RETURN OROOM(I)
9520 END FUNC
9530 FUNC HASFL(I,F$)
9540   RETURN FN SPOS(OFL$(I),F$)>0
9550 END FUNC
9560 FUNC LAMPOK
9570   IF NOT LAMPLIT THEN RETURN 0
9580   RETURN FN ROOMOF(K_LAMP)=-1 OR FN ROOMOF(K_LAMP)=ROOM
9590 END FUNC
9600 FUNC ISDARK
9610   IF ROOM=R_CELLAR AND NOT FN LAMPOK THEN RETURN -1
9620   RETURN 0
9630 END FUNC
9640 FUNC ISOPEN(P)
9650   IF FN HASFL(P,"A") THEN RETURN -1
9660   IF FN HASFL(P,"C") THEN RETURN OOPEN(P)<>0
9670   RETURN 0
9680 END FUNC
9690 ' VIS: is the thing here and in view (in the room or carried; in an open container; on a surface (high ones: standing on a chair); under something that was looked under)
9700 FUNC VIS(I)
9710   LOCAL P
9720   IF I<=0 THEN RETURN 0
9730   IF OHID(I) THEN RETURN 0
9740   P=OIN(I)
9750   IF P=0 THEN
9760     IF OROOM(I)=-1 THEN RETURN -1
9770     IF OROOM(I)=ROOM THEN RETURN NOT FN ISDARK
9780     RETURN 0
9790   END IF
9800   IF NOT FN VIS(P) THEN RETURN 0
9810   IF OREL(I)=1 THEN RETURN FN ISOPEN(P)
9820   IF OREL(I)=2 THEN
9830     IF FN HASFL(P,"H") AND NOT ONCHAIR THEN RETURN 0
9840     RETURN -1
9850   END IF
9860   RETURN OSEEN(P)<>0
9870 END FUNC
9880 FUNC USED
9890   LOCAL I,S
9900   S=0
9910   FOR I=1 TO NOBJ
9920     IF OROOM(I)=-1 AND OIN(I)=0 THEN S=S+OSIZE(I)
9930   NEXT I
9940   RETURN S
9950 END FUNC
9960 ' the number of parts built into the wreck
9970 FUNC BUILT
9980   LOCAL I,S
9990   S=0
10000   FOR I=1 TO NOBJ
10010     IF OROOM(I)=100 AND FN HASFL(I,"P") THEN S=S+1
10020   NEXT I
10030   RETURN S
10040 END FUNC
10050 FUNC HIDESPOT(R)
10060   LOCAL I
10070   FOR I=1 TO NOBJ
10080     IF OROOM(I)=R AND OIN(I)=0 AND FN HASFL(I,"X") THEN RETURN I
10090   NEXT I
10100   RETURN 0
10110 END FUNC
10120 ' the names of the visible things inside / on / under P (relation REL), "a, b and c"
10130 FUNC KIDS$(P,REL)
10140   LOCAL I,N,R$,L$
10150   N=0
10160   R$=""
10170   FOR I=1 TO NOBJ
10180     IF OIN(I)=P AND OREL(I)=REL AND FN VIS(I) THEN
10190       N=N+1
10200       IF N>1 THEN R$=R$+", "
10210       R$=R$+ONAME$(I)
10220     END IF
10230   NEXT I
10240   IF N>1 THEN
10250     L$=R$
10260     I=LEN(L$)
10270     WHILE I>0 AND MID$(L$,I,1)<>","
10280       I=I-1
10290     WEND
10300     R$=LEFT$(L$,I-1)+" and"+MID$(L$,I+1)
10310   END IF
10320   RETURN R$
10330 END FUNC
10340 FUNC DIRWORD$(D)
10350   IF D=1 THEN RETURN "north"
10360   IF D=2 THEN RETURN "south"
10370   IF D=3 THEN RETURN "east"
10380   IF D=4 THEN RETURN "west"
10390   IF D=5 THEN RETURN "up"
10400   IF D=6 THEN RETURN "down"
10410   IF D=7 THEN RETURN "in"
10420   RETURN "out"
10430 END FUNC
10440 ' ------------------------------------------------------------------------------------------------ describing things
10450 FUNC INSIDEHOUSE(R)
10460   RETURN R>=R_HALL
10470 END FUNC
10480 FUNC DYN$(I)
10490   LOCAL T$,N,K
10500   IF I=K_FRONTDOORO OR I=K_HALLDOOR THEN
10510     IF FRONTOPEN THEN RETURN "The front door stands open. Daylight falls into the hall in a sharp rectangle."
10520     RETURN "The front door is shut, and bolted from the inside. You could rattle it all day."
10530   END IF
10540   IF I=K_BACKDOORO THEN
10550     IF BACKOPEN THEN RETURN "The back door stands open."
10560     IF BACKLOCK THEN RETURN "A plain back door, locked. The lock is brass; the key, you suspect, is a spare somebody hid."
10570     RETURN "The back door is closed, but not locked."
10580   END IF
10590   IF I=K_BACKDOORI THEN
10600     IF BACKOPEN THEN RETURN "The back door stands open."
10610     IF BACKLOCK THEN RETURN "The back door is locked, but from inside you can turn the lock yourself."
10620     RETURN "The back door is closed, but not locked."
10630   END IF
10640   IF I=K_HATCHO THEN
10650     IF HATCHOPEN THEN RETURN "The hatch is open: a ladder leads down into the cellar."
10660     IF HATCHLOCK THEN RETURN "A heavy iron hatch with a brass padlock."
10670     RETURN "The hatch is unlocked but closed."
10680   END IF
10690   IF I=K_HATCHI THEN
10700     IF HATCHOPEN THEN RETURN "A ladder leads up to the open hatch and the west garden."
10710     RETURN "A ladder leads up to a closed hatch."
10720   END IF
10730   IF I=K_SHEDDOORO THEN
10740     IF SHEDOPEN THEN RETURN "The shed door is open."
10750     IF SHEDLOCK THEN RETURN "A plank door, shut with a brass padlock."
10760     RETURN "The shed door is closed; the padlock hangs open."
10770   END IF
10780   IF I=K_WELL THEN
10790     IF WELLUP THEN RETURN "The old well. The windlass has its crank, and the iron bucket hangs at the rim."
10800     IF CRANKED THEN RETURN "The old well. The crank handle is fitted to the windlass now: you could turn it to wind up the bucket."
10810     RETURN "The old well. Far down in the dark an iron bucket hangs on the rope. The windlass has no crank handle, only a rusty stub where one should be."
10820   END IF
10830   IF I=K_FRIDGE THEN
10840     IF OOPEN(I) THEN RETURN "The fridge hums in two keys at once. Its door is open and a cold light shines out."
10850     RETURN "A tall fridge that hums in two keys at once. The door is closed."
10860   END IF
10870   IF I=K_OVEN THEN
10880     IF OVENON THEN RETURN "The oven. A big knob is set to ON and the glass shows a red glow inside; the door is far too hot to touch."
10890     IF OVENHOT THEN RETURN "The oven. The knob is set to OFF, but the door is still too hot to touch."
10900     IF OOPEN(I) THEN RETURN "The oven, cold now, with its door open."
10910     RETURN "The oven. The knob is set to OFF and the door has cooled down."
10920   END IF
10930   IF I=K_TAP THEN
10940     IF TAPON THEN RETURN "A brass tap. Hot water is running into the sink and steaming."
10950     RETURN "A brass tap over the sink. The water is off."
10960   END IF
10970   IF I=K_WASHER THEN
10980     IF WASHON THEN RETURN "A big old washing machine. Its drum spins with a hum, and something bright glows through the round window. It is plugged in. The door is locked while it runs."
10990     IF OOPEN(I) THEN RETURN "The washing machine, unplugged. The round door is open."
11000     RETURN "The washing machine has stopped. The round door is closed."
11010   END IF
11020   IF I=K_PLUG THEN
11030     IF WASHON THEN RETURN "A thick plug in a socket in the wall. The cable runs to the washing machine."
11040     RETURN "The plug is out of its socket and hangs on its cable."
11050   END IF
11060   IF I=K_MIRROR THEN
11070     IF MIRRORON THEN RETURN "It is not glass at all, but polished metal, held to the tiles by four screws with four-pointed heads. Your reflection looks slightly annoyed."
11080     RETURN "Four screw holes in the tiles and a clean rectangle on the wall."
11090   END IF
11100   IF I=K_SAFE THEN
11110     IF SAFEOPEN THEN RETURN "The safe stands open."
11120     RETURN "A small, heavy safe in the chimney wall. It has a dial with three digits and is locked."
11130   END IF
11140   IF I=K_GLORP THEN
11150     IF GLORPHERE THEN RETURN "A fat purple blob with three eyes and a mouth full of small teeth. It lies under the bed and guards something shiny behind it, growling like a kettle. It looks hungry."
11160     RETURN "The glorp sits on the landing and smiles with all three eyes. It looks happy and a little too round."
11170   END IF
11180   IF I=K_ICE THEN
11190     RETURN "A block of ice as big as a loaf of bread. A small metallic thing is frozen inside it: a compass! It would have to melt first."
11200   END IF
11210   IF I=K_LAMP THEN
11220     IF LAMPLIT THEN RETURN "A lantern. It glows with a warm yellow light."
11230     RETURN "A lantern with a small glass chimney and a switch on its base. It is off."
11240   END IF
11250   IF I=K_PHOTO THEN
11260     IF PHOTOBACK THEN RETURN "A photo of three tall figures in front of a silver ship, waving. You turned it over: scratched into the back with a pin are three digits: 7 3 1."
11270     RETURN ODESC$(I)+" You wonder what is on the back."
11280   END IF
11290   IF I=K_SHIP THEN
11300     N=FN BUILT
11310     T$="The wreck of Deqq's ship: a skeleton of beams in a burnt hollow. "+STR$(N)+" of the 8 parts are built in."
11320     IF N=0 THEN T$=T$+" You could INSTALL a part here once you have one."
11330     RETURN T$
11340   END IF
11350   IF I=K_DEQQ THEN
11360     RETURN "Deqq is tall and grey-green, with a long neck, a cloak the color of a bruise and huge black eyes with a faint glow deep inside. He does not blink. When he moves you do not see him move."
11370   END IF
11380   IF I=K_CHAIR THEN
11390     IF ONCHAIR THEN RETURN "You are standing on the chair. It wobbles a little. GET DOWN to step off."
11400   END IF
11410   IF I=K_CLOSET OR I=K_WARDROBE OR I=K_PANTRY OR I=K_LINEN OR I=K_TRUNK THEN
11420     RETURN ODESC$(I)
11430   END IF
11440   RETURN ODESC$(I)
11450 END FUNC
11460 PROC EXAMINE(I)
11470   LOCAL T$,K$
11480   CALL SAY(FN DYN$(I),1)
11490   IF FN HASFL(I,"A") OR (FN HASFL(I,"C") AND OOPEN(I)) THEN
11500     K$=FN KIDS$(I,1)
11510     IF K$<>"" THEN CALL SAY("Inside: "+K$+".",1)
11520     IF K$="" AND NOT (I=K_WASHER) THEN CALL SAY("It is empty.",6)
11530   END IF
11540   K$=FN KIDS$(I,2)
11550   IF K$<>"" THEN CALL SAY("On it: "+K$+".",1)
11560   IF OSEEN(I) THEN
11570     K$=FN KIDS$(I,3)
11580     IF K$<>"" THEN CALL SAY("Under it: "+K$+".",1)
11590   END IF
11600 END PROC
11610 ' room description: FULL = always the long text, else only the first time
11620 PROC LOOKROOM(FULL)
11630   LOCAL I,L$,N,E$
11640   IF FN ISDARK THEN
11650     CALL SAY("Cellar. It is pitch dark. You can't see a thing, only feel the cold stone steps behind you. A light would help.",1)
11660     CALL SAY("Exits: up.",6)
11670     EXIT PROC
11680   END IF
11690   IF FULL OR NOT RSEEN(ROOM) THEN
11700     CALL SAY(RNAME$(ROOM)+". "+RDESC$(ROOM),1)
11710   ELSE
11720     CALL SAY(RNAME$(ROOM)+".",1)
11730   END IF
11740   RSEEN(ROOM)=-1
11750   L$=""
11760   N=0
11770   FOR I=1 TO NOBJ
11780     IF OSIZE(I)>0 AND OROOM(I)<>-1 AND FN VIS(I) THEN
11790       N=N+1
11800       IF N>1 THEN L$=L$+", "
11810       L$=L$+ONAME$(I)
11820     END IF
11830   NEXT I
11840   IF N>1 THEN
11850     I=LEN(L$)
11860     WHILE I>0 AND MID$(L$,I,1)<>","
11870       I=I-1
11880     WEND
11890     L$=LEFT$(L$,I-1)+" and"+MID$(L$,I+1)
11900   END IF
11910   IF N>0 THEN CALL SAY("You see "+L$+".",1)
11920   IF FN VIS(K_GLORP) AND ROOM=R_LANDING THEN CALL SAY("The glorp is here, purring.",1)
11930   IF DQHOME AND DQROOM=ROOM AND ROOM>=R_HALL THEN CALL SAY("Deqq is here, watching you.",2)
11940   IF HIDING THEN CALL SAY("You are hiding in "+ONAME$(HIDEOBJ)+".",3)
11950   E$=""
11960   FOR I=1 TO 8
11970     IF EX(ROOM,I)>0 THEN
11980       IF E$<>"" THEN E$=E$+", "
11990       E$=E$+FN DIRWORD$(I)
12000     END IF
12010   NEXT I
12020   CALL SAY("Exits: "+E$+".",6)
12030 END PROC
12040 ' ------------------------------------------------------------------------------------------------ finding the things the player means
12050 FUNC RESOLVE(P$)
12060   LOCAL I,J,N,K,BEST,BS,SC,PASS,T$
12070   IF P$="" THEN RETURN 0
12080   N=0
12090   T$=P$+" "
12100   WHILE LEN(T$)>0
12110     K=FN SPOS(T$," ")
12120     N=N+1
12130     PW$(N)=LEFT$(T$,K-1)
12140     T$=MID$(T$,K+1)
12150   WEND
12160   IF PW$(N)="IT" OR PW$(N)="THEM" THEN
12170     IF LASTOBJ>0 THEN
12180       IF FN VIS(LASTOBJ) THEN RETURN LASTOBJ
12190     END IF
12200     RETURN 0
12210   END IF
12220   BEST=0
12230   BS=0
12240   FOR PASS=1 TO 2
12250     FOR I=1 TO NOBJ
12260       IF (PASS=1 AND OROOM(I)=-1) OR (PASS=2 AND OROOM(I)<>-1) THEN
12270         IF FN INLIST(PW$(N),OWORDS$(I)) THEN
12280           IF FN VIS(I) THEN
12290             SC=1
12300             FOR J=1 TO N-1
12310               IF FN INLIST(PW$(J),OWORDS$(I)) THEN SC=SC+1
12320             NEXT J
12330             IF SC>BS THEN
12340               BS=SC
12350               BEST=I
12360             END IF
12370           END IF
12380         END IF
12390       END IF
12400     NEXT I
12410   NEXT PASS
12420   RETURN BEST
12430 END FUNC
12440 FUNC FINDVERB(W$)
12450   LOCAL I
12460   FOR I=1 TO NVERB
12470     IF VW$(I)=W$ THEN RETURN VID(I)
12480   NEXT I
12490   RETURN 0
12500 END FUNC
12510 ' the command line -> V (verb), A and B (things), PREP$ (IN ON UNDER BEHIND WITH TO FROM AT THROUGH), PART$ (ON OFF UP DOWN OVER ...), NUM$ (digits)
12520 PROC PARSE(L$)
12530   LOCAL I,J,S,P,Z$
12540   CALL SPLITW(L$)
12550   J=0
12560   FOR I=1 TO NW
12570     IF NOT FN INLIST(W$(I)," THE A AN SOME MY OF PLEASE ") THEN
12580       J=J+1
12590       W$(J)=W$(I)
12600     END IF
12610   NEXT I
12620   NW=J
12630   V=0:A=0:B=0:PREP$="":PART$="":P1$="":P2$="":NUM$=""
12640   IF NW=0 THEN EXIT PROC
12650   V=FN FINDVERB(W$(1))
12660   IF V=0 THEN EXIT PROC
12670   S=2
12680   IF V=9 AND NW>=2 THEN
12690     V=FN FINDVERB(W$(2))
12700     S=3
12710     IF V<1 OR V>8 THEN V=-1
12720   END IF
12730   IF V<=8 AND NOT (V=7 AND NW>=2) THEN EXIT PROC
12740   ' particles that belong to the verb: TURN ON x, TURN x ON, PICK UP x, PICK x UP, TURN x OVER ...
12750   IF V=17 OR V=12 OR V=34 OR V=13 OR V=20 THEN
12760     P=0
12770     FOR I=S TO NW
12780       IF P=0 AND FN INLIST(W$(I)," ON OFF OVER AROUND UP DOWN ") THEN
12790         IF (I=S) OR (I=NW) OR V=17 THEN P=I
12800       END IF
12810     NEXT I
12820     IF P>0 THEN
12830       PART$=W$(P)
12840       FOR J=P TO NW-1
12850         W$(J)=W$(J+1)
12860       NEXT J
12870       NW=NW-1
12880     END IF
12890   END IF
12900   IF S<=NW THEN
12910     IF FN INLIST(W$(S)," AT OUT ") AND PART$="" THEN
12920       PART$=W$(S)
12930       S=S+1
12940     END IF
12950   END IF
12960   P=0
12970   FOR I=S TO NW
12980     IF P=0 AND FN INLIST(W$(I)," IN INTO INSIDE ONTO ON UNDER BENEATH BELOW BEHIND WITH USING TO TOWARD TOWARDS FROM THROUGH AT ") THEN P=I
12990   NEXT I
13000   FOR I=S TO NW
13010     IF FN ISNUM(W$(I)) THEN
13020       NUM$=NUM$+W$(I)
13030     ELSE
13040       IF P=0 OR I<P THEN
13050         IF P1$<>"" THEN P1$=P1$+" "
13060         P1$=P1$+W$(I)
13070       ELSE
13080         IF I>P THEN
13090           IF P2$<>"" THEN P2$=P2$+" "
13100           P2$=P2$+W$(I)
13110         END IF
13120       END IF
13130     END IF
13140   NEXT I
13150   IF P>0 THEN
13160     Z$=W$(P)
13170     PREP$=Z$
13180     IF Z$="INTO" OR Z$="INSIDE" THEN PREP$="IN"
13190     IF Z$="ONTO" THEN PREP$="ON"
13200     IF Z$="BENEATH" OR Z$="BELOW" THEN PREP$="UNDER"
13210     IF Z$="USING" THEN PREP$="WITH"
13220     IF Z$="TOWARD" OR Z$="TOWARDS" THEN PREP$="TO"
13230   END IF
13240   IF V=42 OR V=44 OR V=30 OR V=31 OR V=26 THEN EXIT PROC
13250   A=FN RESOLVE(P1$)
13260   B=FN RESOLVE(P2$)
13270 END PROC
13280 ' ------------------------------------------------------------------------------------------------ the commands
13290 PROC SND(N)
13300   IF HL THEN EXIT PROC
13310   SFX N
13320 END PROC
13330 PROC NOSEE
13340   IF P1$="" AND P2$="" THEN
13350     CALL SAY("Which thing do you mean?",1)
13360   ELSE
13370     CALL SAY("You don't see any such thing here.",1)
13380   END IF
13390 END PROC
13400 PROC UNHIDE
13410   IF NOT HIDING THEN
13420     CALL SAY("You are not hiding.",1)
13430     EXIT PROC
13440   END IF
13450   CALL SAY("You crawl out of "+ONAME$(HIDEOBJ)+" and brush the dust off.",1)
13460   HIDING=0
13470   TCOST=1
13480   REDRAW=-1
13490 END PROC
13500 ' ---- moving
13510 FUNC DOORCHECK$(F,T)
13520   IF F=R_FRONT AND T=R_HALL THEN
13530     IF NOT FRONTOPEN THEN RETURN "The front door is shut and bolted from the inside. Somebody does not want visitors."
13540   END IF
13550   IF F=R_HALL AND T=R_FRONT THEN
13560     IF NOT FRONTOPEN THEN
13570       FRONTOPEN=-1
13580       CALL SAY("You slide the bolt back and open the front door.",1)
13590     END IF
13600   END IF
13610   IF F=R_BACKYD AND T=R_KITCHEN THEN
13620     IF BACKLOCK THEN RETURN "The back door is locked."
13630     IF NOT BACKOPEN THEN
13640       BACKOPEN=-1
13650       CALL SAY("You open the back door.",1)
13660     END IF
13670   END IF
13680   IF F=R_KITCHEN AND T=R_BACKYD THEN
13690     IF BACKLOCK THEN
13700       BACKLOCK=0
13710       CALL SAY("You turn the lock from inside.",1)
13720     END IF
13730     IF NOT BACKOPEN THEN
13740       BACKOPEN=-1
13750       CALL SAY("You open the back door.",1)
13760     END IF
13770   END IF
13780   IF F=R_WESTG AND T=R_CELLAR THEN
13790     IF HATCHLOCK THEN RETURN "The hatch is shut with a heavy padlock."
13800     IF NOT HATCHOPEN THEN
13810       HATCHOPEN=-1
13820       CALL SAY("You heave the hatch open.",1)
13830     END IF
13840   END IF
13850   IF F=R_CELLAR AND T=R_WESTG THEN
13860     IF HATCHLOCK THEN
13870       HATCHLOCK=0
13880       CALL SAY("You unhook the padlock from inside.",1)
13890     END IF
13900     IF NOT HATCHOPEN THEN
13910       HATCHOPEN=-1
13920       CALL SAY("You push the hatch open.",1)
13930     END IF
13940   END IF
13950   IF F=R_WELLG AND T=R_SHED THEN
13960     IF SHEDLOCK THEN RETURN "The shed door is shut with a brass padlock."
13970     IF NOT SHEDOPEN THEN
13980       SHEDOPEN=-1
13990       CALL SAY("You open the shed door.",1)
14000     END IF
14010   END IF
14020   RETURN ""
14030 END FUNC
14040 PROC DOMOVE(D)
14050   LOCAL T,M$
14060   IF HIDING THEN
14070     IF D=8 THEN
14080       CALL UNHIDE
14090     ELSE
14100       CALL SAY("You are hiding. COME OUT first (or just wait, if you dare).",3)
14110     END IF
14120     EXIT PROC
14130   END IF
14140   T=EX(ROOM,D)
14150   IF T=0 THEN
14160     CALL SAY("You can't go that way.",1)
14170     EXIT PROC
14180   END IF
14190   M$=FN DOORCHECK$(ROOM,T)
14200   IF M$<>"" THEN
14210     CALL SAY(M$,1)
14220     EXIT PROC
14230   END IF
14240   IF ONCHAIR THEN ONCHAIR=0
14250   ROOM=T
14260   TCOST=1
14270   REDRAW=-1
14280   CALL SND(1)
14290   CALL LOOKROOM(0)
14300 END PROC
14310 ' ---- looking
14320 PROC REVEAL(T)
14330   LOCAL I,K$
14340   OSEEN(T)=-1
14350   FOR I=1 TO NOBJ
14360     IF OIN(I)=T AND OREL(I)=3 AND OHID(I) THEN
14370       IF NOT (I=K_NOZZLE AND GLORPHERE) THEN OHID(I)=0
14380     END IF
14390   NEXT I
14400   K$=FN KIDS$(T,3)
14410   IF K$="" THEN
14420     CALL SAY("You look under "+ONAME$(T)+": nothing but dust and an old sock.",1)
14430   ELSE
14440     CALL SAY("You look under "+ONAME$(T)+" and find: "+K$+".",1)
14450   END IF
14460   IF T=K_BED AND GLORPHERE THEN
14470     CALL SAY("Two small eyes open in the dark. Then a third. Something growls, and it isn't the bed. Behind it a metal shape gleams.",4)
14480   END IF
14490   TCOST=1
14500   REDRAW=-1
14510 END PROC
14520 PROC DOLOOK
14530   LOCAL T
14540   IF HIDING THEN
14550     CALL SAY("You peek through a crack.",6)
14560     CALL LOOKROOM(0)
14570     EXIT PROC
14580   END IF
14590   IF P1$="" AND P2$="" AND PART$<>"AT" THEN
14600     CALL LOOKROOM(-1)
14610     EXIT PROC
14620   END IF
14630   IF PREP$="" THEN
14640     IF A>0 THEN
14650       LASTOBJ=A
14660       CALL EXAMINE(A)
14670     ELSE
14680       CALL NOSEE
14690     END IF
14700     EXIT PROC
14710   END IF
14720   T=B
14730   IF A>0 AND B=0 THEN T=A
14740   IF P1$<>"" AND A>0 AND B>0 THEN T=B
14750   IF T<=0 THEN
14760     CALL NOSEE
14770     EXIT PROC
14780   END IF
14790   LASTOBJ=T
14800   IF PREP$="UNDER" THEN
14810     IF FN HASFL(T,"U") THEN
14820       CALL REVEAL(T)
14830     ELSE
14840       CALL SAY("There is nothing under "+ONAME$(T)+".",1)
14850     END IF
14860     EXIT PROC
14870   END IF
14880   IF PREP$="BEHIND" THEN
14890     IF T=K_PHOTO THEN
14900       CALL DOTURNPHOTO
14910     ELSE
14920       IF FN HASFL(T,"X") THEN
14930         CALL SAY("There is room to hide behind it.",1)
14940       ELSE
14950         CALL SAY("Nothing behind "+ONAME$(T)+" but a wall.",1)
14960       END IF
14970     END IF
14980     EXIT PROC
14990   END IF
15000   IF PREP$="IN" OR PREP$="THROUGH" THEN
15010     IF FN HASFL(T,"A") OR (FN HASFL(T,"C") AND OOPEN(T)) THEN
15020       CALL EXAMINE(T)
15030     ELSE
15040       IF FN HASFL(T,"C") THEN
15050         CALL SAY("It is closed. OPEN it first.",1)
15060       ELSE
15070         CALL SAY("You can't look inside that.",1)
15080       END IF
15090     END IF
15100     EXIT PROC
15110   END IF
15120   CALL EXAMINE(T)
15130 END PROC
15140 PROC DOTURNPHOTO
15150   IF PHOTOBACK THEN
15160     CALL SAY("The back says: 7 3 1. You already know.",1)
15170   ELSE
15180     PHOTOBACK=-1
15190     CALL SAY("You bend the little clips and look at the back of the photo. Scratched into the cardboard with a pin: 7 3 1.",5)
15200     TCOST=1
15210   END IF
15220 END PROC
15230 PROC DOINV
15240   LOCAL I,K$,N
15250   K$=""
15260   N=0
15270   FOR I=1 TO NOBJ
15280     IF OROOM(I)=-1 AND OIN(I)=0 THEN
15290       N=N+1
15300       IF N>1 THEN K$=K$+", "
15310       K$=K$+ONAME$(I)
15320       IF OSIZE(I)>1 THEN K$=K$+" (2 slots)"
15330     END IF
15340   NEXT I
15350   IF N=0 THEN
15360     CALL SAY("You are carrying nothing. ("+STR$(CAP)+" slots free)",1)
15370   ELSE
15380     CALL SAY("You carry: "+K$+". ("+STR$(FN USED)+" of "+STR$(CAP)+" slots used)",1)
15390   END IF
15400 END PROC
15410 ' ---- taking and dropping
15420 FUNC TAKEBLOCK$(I)
15430   IF I=K_COMPASS AND OIN(I)=K_ICE THEN RETURN "The compass is frozen into the block of ice. You could take the whole block, though."
15440   IF I=K_GLORP THEN RETURN "The glorp growls. You decide you would rather keep your fingers."
15450   IF I=K_MIRROR THEN RETURN "The mirror is held to the wall by four screws."
15460   IF I=K_DEQQ THEN RETURN "You think about it for exactly one second."
15470   IF OSIZE(I)=0 THEN RETURN "You can't take "+ONAME$(I)+"."
15480   RETURN ""
15490 END FUNC
15500 PROC DOTAKE
15510   LOCAL M$
15520   IF A=0 THEN
15530     IF PART$="DOWN" AND ONCHAIR THEN
15540       ONCHAIR=0
15550       REDRAW=-1
15560       CALL SAY("You step down from the chair.",1)
15570       TCOST=1
15580       EXIT PROC
15590     END IF
15600     IF PART$="OUT" OR PART$="UP" THEN
15610       IF HIDING THEN CALL UNHIDE
15620       EXIT PROC
15630     END IF
15640     IF P1$="" THEN
15650       CALL SAY("What do you want to take?",1)
15660     ELSE
15670       CALL NOSEE
15680     END IF
15690     EXIT PROC
15700   END IF
15710   LASTOBJ=A
15720   IF OROOM(A)=-1 AND OIN(A)=0 THEN
15730     CALL SAY("You already have "+ONAME$(A)+".",1)
15740     EXIT PROC
15750   END IF
15760   M$=FN TAKEBLOCK$(A)
15770   IF M$<>"" THEN
15780     CALL SAY(M$,1)
15790     EXIT PROC
15800   END IF
15810   IF FN USED+OSIZE(A)>CAP THEN
15820     CALL SAY("You can't carry that as well. You have "+STR$(CAP)+" slots and "+ONAME$(A)+" needs "+STR$(OSIZE(A))+". DROP something first.",1)
15830     EXIT PROC
15840   END IF
15850   OIN(A)=0
15860   OROOM(A)=-1
15870   CALL SAY("Taken: "+ONAME$(A)+".",1)
15880   CALL SND(2)
15890   TCOST=1
15900   REDRAW=-1
15910 END PROC
15920 PROC DODROP
15930   IF A=0 THEN
15940     IF P1$="" THEN CALL SAY("What do you want to drop?",1) ELSE CALL NOSEE
15950     EXIT PROC
15960   END IF
15970   IF OROOM(A)<>-1 OR OIN(A)<>0 THEN
15980     CALL SAY("You are not carrying "+ONAME$(A)+".",1)
15990     EXIT PROC
16000   END IF
16010   OROOM(A)=ROOM
16020   CALL FLOORSPOT(A,ROOM)
16030   LASTOBJ=A
16040   CALL SAY("Dropped: "+ONAME$(A)+".",1)
16050   TCOST=1
16060   REDRAW=-1
16070 END PROC
16080 PROC DOPUT
16090   LOCAL T,R,M$
16100   IF A=0 THEN
16110     IF P1$="" THEN CALL SAY("What do you want to put?",1) ELSE CALL NOSEE
16120     EXIT PROC
16130   END IF
16140   IF PREP$="" AND B=0 THEN
16150     IF PART$="DOWN" THEN
16160       CALL DODROP
16170     ELSE
16180       CALL SAY("Put "+ONAME$(A)+" where? (PUT it IN, ON or UNDER something.)",1)
16190     END IF
16200     EXIT PROC
16210   END IF
16220   IF B<=0 THEN
16230     CALL SAY("Put it where? You don't see that here.",1)
16240     EXIT PROC
16250   END IF
16260   LASTOBJ=A
16270   IF B=K_SHIP THEN
16280     CALL DOINSTALL
16290     EXIT PROC
16300   END IF
16310   IF A=K_CRANK AND B=K_WELL THEN
16320     CALL ATTACHCRANK
16330     EXIT PROC
16340   END IF
16350   IF A=K_JELLY AND (B=K_GLORP OR (B=K_BED AND PREP$="UNDER")) THEN
16360     CALL FEEDGLORP
16370     EXIT PROC
16380   END IF
16390   IF A=B THEN
16400     CALL SAY("That would be a neat trick.",1)
16410     EXIT PROC
16420   END IF
16430   IF OROOM(A)<>-1 OR OIN(A)<>0 THEN
16440     ' not carried: take it first if it fits
16450     IF OSIZE(A)=0 THEN
16460       CALL SAY("You can't move "+ONAME$(A)+".",1)
16470       EXIT PROC
16480     END IF
16490     IF FN USED+OSIZE(A)>CAP THEN
16500       CALL SAY("Your hands are full. DROP something first.",1)
16510       EXIT PROC
16520     END IF
16530     M$=FN TAKEBLOCK$(A)
16540     IF M$<>"" THEN
16550       CALL SAY(M$,1)
16560       EXIT PROC
16570     END IF
16580   END IF
16590   R=1
16600   IF PREP$="ON" THEN R=2
16610   IF PREP$="UNDER" THEN R=3
16620   IF R=1 THEN
16630     IF NOT FN ISOPEN(B) THEN
16640       IF FN HASFL(B,"C") THEN
16650         CALL SAY(ONAME$(B)+" is closed.",1)
16660       ELSE
16670         CALL SAY("You can't put anything in there.",1)
16680       END IF
16690       EXIT PROC
16700     END IF
16710   END IF
16720   IF R=2 THEN
16730     IF NOT FN HASFL(B,"S") THEN
16740       CALL SAY("Nothing would stay on "+ONAME$(B)+".",1)
16750       EXIT PROC
16760     END IF
16770     IF FN HASFL(B,"H") AND NOT ONCHAIR THEN
16780       CALL SAY("You can't reach the top of "+ONAME$(B)+".",1)
16790       EXIT PROC
16800     END IF
16810   END IF
16820   IF R=3 THEN
16830     IF NOT FN HASFL(B,"U") THEN
16840       CALL SAY("You can't put anything under "+ONAME$(B)+".",1)
16850       EXIT PROC
16860     END IF
16870     OSEEN(B)=-1
16880   END IF
16890   OIN(A)=B
16900   OREL(A)=R
16910   OROOM(A)=0
16920   IF R=1 THEN CALL SAY("You put "+ONAME$(A)+" in "+ONAME$(B)+".",1)
16930   IF R=2 THEN CALL SAY("You put "+ONAME$(A)+" on "+ONAME$(B)+".",1)
16940   IF R=3 THEN CALL SAY("You push "+ONAME$(A)+" under "+ONAME$(B)+".",1)
16950   IF A=K_ICE AND B=K_SINK THEN CALL SAY("The block of ice sits in the stone sink. It could do with some warm water.",5)
16960   TCOST=1
16970   REDRAW=-1
16980 END PROC
16990 ' ---- opening and closing
17000 PROC DOOPEN
17010   LOCAL K$
17020   IF A=0 THEN
17030     IF P1$="" THEN CALL SAY("What do you want to open?",1) ELSE CALL NOSEE
17040     EXIT PROC
17050   END IF
17060   LASTOBJ=A
17070   IF NUM$<>"" AND A=K_SAFE THEN
17080     CALL DODIAL
17090     EXIT PROC
17100   END IF
17110   IF A=K_FRONTDOORO OR A=K_HALLDOOR THEN
17120     IF FRONTOPEN THEN
17130       CALL SAY("It is open already.",1)
17140     ELSE
17150       IF ROOM=R_HALL THEN
17160         FRONTOPEN=-1
17170         CALL SAY("You slide the bolt back and open the front door.",1)
17180         TCOST=1
17190         REDRAW=-1
17200       ELSE
17210         CALL SAY("The front door is bolted from the inside.",1)
17220       END IF
17230     END IF
17240     EXIT PROC
17250   END IF
17260   IF A=K_BACKDOORO OR A=K_BACKDOORI THEN
17270     IF BACKOPEN THEN
17280       CALL SAY("It is open already.",1)
17290     ELSE
17300       IF BACKLOCK AND ROOM=R_BACKYD THEN
17310         CALL SAY("The back door is locked.",1)
17320       ELSE
17330         BACKLOCK=0
17340         BACKOPEN=-1
17350         CALL SAY("You open the back door.",1)
17360         TCOST=1
17370         REDRAW=-1
17380       END IF
17390     END IF
17400     EXIT PROC
17410   END IF
17420   IF A=K_HATCHO OR A=K_HATCHI THEN
17430     IF HATCHOPEN THEN
17440       CALL SAY("It is open already.",1)
17450     ELSE
17460       IF HATCHLOCK AND ROOM=R_WESTG THEN
17470         CALL SAY("The hatch is shut with a heavy padlock.",1)
17480       ELSE
17490         HATCHLOCK=0
17500         HATCHOPEN=-1
17510         CALL SAY("You heave the hatch open.",1)
17520         TCOST=1
17530         REDRAW=-1
17540       END IF
17550     END IF
17560     EXIT PROC
17570   END IF
17580   IF A=K_SHEDDOORO THEN
17590     IF SHEDOPEN THEN
17600       CALL SAY("It is open already.",1)
17610     ELSE
17620       IF SHEDLOCK THEN
17630         CALL SAY("The shed door is shut with a brass padlock.",1)
17640       ELSE
17650         SHEDOPEN=-1
17660         CALL SAY("You open the shed door.",1)
17670         TCOST=1
17680         REDRAW=-1
17690       END IF
17700     END IF
17710     EXIT PROC
17720   END IF
17730   IF NOT FN HASFL(A,"C") THEN
17740     IF FN HASFL(A,"A") THEN
17750       CALL SAY("It has no lid or door. You can see right in.",1)
17760       CALL EXAMINE(A)
17770     ELSE
17780       CALL SAY("You can't open "+ONAME$(A)+".",1)
17790     END IF
17800     EXIT PROC
17810   END IF
17820   IF OOPEN(A) THEN
17830     CALL SAY("It is open already.",1)
17840     EXIT PROC
17850   END IF
17860   IF A=K_OVEN AND (OVENON OR OVENHOT) THEN
17870     CALL SAY("Ouch! You touch the oven door and jerk your hand back. It is far too hot. Something should be done about the heat first.",4)
17880     EXIT PROC
17890   END IF
17900   IF A=K_WASHER AND WASHON THEN
17910     CALL SAY("The round door is locked. The drum spins behind it, and something glows in there. The machine would have to stop first.",1)
17920     EXIT PROC
17930   END IF
17940   IF A=K_SAFE AND NOT SAFEOPEN THEN
17950     CALL SAY("The safe is locked. The dial wants three digits: DIAL 123, for instance.",1)
17960     EXIT PROC
17970   END IF
17980   OOPEN(A)=-1
17990   CALL SND(3)
18000   CALL SAY("You open "+ONAME$(A)+".",1)
18010   K$=FN KIDS$(A,1)
18020   IF K$<>"" THEN
18030     CALL SAY("Inside: "+K$+".",1)
18040   ELSE
18050     CALL SAY("It is empty.",6)
18060   END IF
18070   TCOST=1
18080   REDRAW=-1
18090 END PROC
18100 PROC DOCLOSE
18110   IF A=0 THEN
18120     IF P1$="" THEN CALL SAY("What do you want to close?",1) ELSE CALL NOSEE
18130     EXIT PROC
18140   END IF
18150   LASTOBJ=A
18160   IF A=K_BACKDOORO OR A=K_BACKDOORI THEN
18170     IF BACKOPEN THEN
18180       BACKOPEN=0
18190       CALL SAY("You close the back door.",1)
18200       TCOST=1
18210       REDRAW=-1
18220     ELSE
18230       CALL SAY("It is closed already.",1)
18240     END IF
18250     EXIT PROC
18260   END IF
18270   IF A=K_HATCHO OR A=K_HATCHI THEN
18280     IF HATCHOPEN THEN
18290       HATCHOPEN=0
18300       CALL SAY("You lower the hatch.",1)
18310       TCOST=1
18320       REDRAW=-1
18330     ELSE
18340       CALL SAY("It is closed already.",1)
18350     END IF
18360     EXIT PROC
18370   END IF
18380   IF A=K_SHEDDOORO THEN
18390     IF SHEDOPEN THEN
18400       SHEDOPEN=0
18410       CALL SAY("You close the shed door.",1)
18420       TCOST=1
18430       REDRAW=-1
18440     ELSE
18450       CALL SAY("It is closed already.",1)
18460     END IF
18470     EXIT PROC
18480   END IF
18490   IF A=K_FRONTDOORO OR A=K_HALLDOOR THEN
18500     CALL SAY("The front door doesn't like to be touched by anyone but Deqq.",1)
18510     EXIT PROC
18520   END IF
18530   IF NOT FN HASFL(A,"C") THEN
18540     CALL SAY("You can't close "+ONAME$(A)+".",1)
18550     EXIT PROC
18560   END IF
18570   IF NOT OOPEN(A) THEN
18580     CALL SAY("It is closed already.",1)
18590     EXIT PROC
18600   END IF
18610   OOPEN(A)=0
18620   CALL SAY("You close "+ONAME$(A)+".",1)
18630   TCOST=1
18640   REDRAW=-1
18650 END PROC
18660 ' ---- turning things on and off
18670 PROC DOTURN
18680   LOCAL T
18690   IF A=0 AND NUM$<>"" THEN
18700     CALL DODIAL
18710     EXIT PROC
18720   END IF
18730   IF A=0 THEN
18740     IF P1$="" THEN CALL SAY("Turn what?",1) ELSE CALL NOSEE
18750     EXIT PROC
18760   END IF
18770   LASTOBJ=A
18780   IF A=K_OVEN THEN
18790     IF PART$="ON" OR (PART$="" AND NOT OVENON) THEN
18800       OVENON=-1
18810       OVENHOT=-1
18820       OVENCOOL=0
18830       IF OOPEN(A) THEN OOPEN(A)=0
18840       CALL SAY("You turn the knob to ON. A red glow starts inside the oven.",1)
18850     ELSE
18860       IF OVENON THEN
18870         OVENON=0
18880         OVENCOOL=3
18890         CALL SAY("You turn the knob to OFF. The red glow fades. It will be a while before the oven has cooled down.",1)
18900       ELSE
18910         CALL SAY("The oven is off already.",1)
18920         EXIT PROC
18930       END IF
18940     END IF
18950     TCOST=1
18960     REDRAW=-1
18970     EXIT PROC
18980   END IF
18990   IF A=K_TAP THEN
19000     IF PART$="OFF" OR (PART$="" AND TAPON) THEN
19010       TAPON=0
19020       CALL SAY("You turn the tap off.",1)
19030     ELSE
19040       TAPON=-1
19050       CALL SAY("You turn the tap on. Hot water splashes into the sink and steams.",1)
19060       IF FN VIS(K_ICE) AND OIN(K_ICE)=K_SINK THEN CALL SAY("The water runs over the block of ice, which begins to creak.",5)
19070     END IF
19080     TCOST=1
19090     REDRAW=-1
19100     EXIT PROC
19110   END IF
19120   IF A=K_LAMP THEN
19130     IF PART$="OFF" OR (PART$="" AND LAMPLIT) THEN
19140       LAMPLIT=0
19150       CALL SAY("You switch the lantern off.",1)
19160     ELSE
19170       LAMPLIT=-1
19180       CALL SAY("You switch the lantern on. A warm yellow light spreads.",1)
19190     END IF
19200     TCOST=1
19210     REDRAW=-1
19220     EXIT PROC
19230   END IF
19240   IF A=K_WASHER THEN
19250     IF WASHON THEN
19260       CALL SAY("You press the STOP button. The drum does not care. The machine seems to be stuck in its cycle. Maybe it needs to lose its power.",1)
19270     ELSE
19280       CALL SAY("The machine is dead already.",1)
19290     END IF
19300     EXIT PROC
19310   END IF
19320   IF A=K_PLUG THEN
19330     CALL DOUNPLUG
19340     EXIT PROC
19350   END IF
19360   IF A=K_PHOTO THEN
19370     CALL DOTURNPHOTO
19380     EXIT PROC
19390   END IF
19400   IF A=K_SAFE THEN
19410     CALL DODIAL
19420     EXIT PROC
19430   END IF
19440   IF A=K_WELL OR A=K_CRANK THEN
19450     CALL DOWIND
19460     EXIT PROC
19470   END IF
19480   IF A=K_TV THEN
19490     CALL SAY("The television goes dark. For a moment something looks out of the black glass, then the picture is back.",1)
19500     EXIT PROC
19510   END IF
19520   CALL SAY("Nothing happens.",1)
19530 END PROC
19540 PROC DOUNPLUG
19550   IF ROOM<>R_CELLAR THEN
19560     CALL SAY("There is nothing here to unplug.",1)
19570     EXIT PROC
19580   END IF
19590   IF WASHON THEN
19600     WASHON=0
19610     CALL SAY("You pull the plug out of the wall. The washing machine shudders, the drum slows down and stops. The glow behind the window keeps shining.",1)
19620     CALL SND(5)
19630     TCOST=1
19640     REDRAW=-1
19650   ELSE
19660     CALL SAY("It is unplugged already.",1)
19670   END IF
19680 END PROC
19690 PROC ATTACHCRANK
19700   IF OROOM(K_CRANK)<>-1 THEN
19710     CALL SAY("You don't have the crank handle.",1)
19720     EXIT PROC
19730   END IF
19740   IF ROOM<>R_WELLG THEN
19750     CALL SAY("There is nothing to put it on here.",1)
19760     EXIT PROC
19770   END IF
19780   IF CRANKED THEN
19790     CALL SAY("The crank is fitted already.",1)
19800     EXIT PROC
19810   END IF
19820   CRANKED=-1
19830   OROOM(K_CRANK)=0
19840   CALL SAY("The crank handle fits the stub of the windlass exactly, as if it had been waiting. You can TURN it to wind up the bucket.",5)
19850   TCOST=1
19860   REDRAW=-1
19870 END PROC
19880 PROC DOWIND
19890   IF ROOM<>R_WELLG THEN
19900     CALL SAY("There is nothing to wind here.",1)
19910     EXIT PROC
19920   END IF
19930   IF NOT CRANKED THEN
19940     IF OROOM(K_CRANK)=-1 THEN
19950       CALL ATTACHCRANK
19960     ELSE
19970       CALL SAY("Without a crank handle there is nothing to turn. Only a rusty stub.",1)
19980     END IF
19990     EXIT PROC
20000   END IF
20010   IF WELLUP THEN
20020     CALL SAY("The bucket is up already.",1)
20030     EXIT PROC
20040   END IF
20050   WELLUP=-1
20060   OHID(K_BUCKET)=0
20070   CALL SAY("You turn the crank. The rope creaks, the old windlass groans, and after a long time an iron bucket rises into the daylight, dripping.",1)
20080   CALL SAY("Inside: "+FN KIDS$(K_BUCKET,1)+".",1)
20090   CALL SND(3)
20100   TCOST=1
20110   REDRAW=-1
20120 END PROC
20130 PROC DOUNSCREW
20140   IF A<>K_MIRROR THEN
20150     IF A>0 AND OSIZE(A)>0 THEN
20160       CALL DOTAKE
20170     ELSE
20180       CALL SAY("You can't unscrew that.",1)
20190     END IF
20200     EXIT PROC
20210   END IF
20220   IF NOT MIRRORON THEN
20230     CALL SAY("It is off the wall already.",1)
20240     EXIT PROC
20250   END IF
20260   IF OROOM(K_SCREWDRIVER)<>-1 THEN
20270     CALL SAY("The screws have four-pointed heads. Your fingernails are not the right shape.",1)
20280     EXIT PROC
20290   END IF
20300   MIRRORON=0
20310   OHID(K_PLATE)=0
20320   CALL FLOORSPOT(K_PLATE,R_BATH)
20330   CALL SAY("The strange screwdriver fits the four-pointed screws perfectly. One, two, three, four. The 'mirror' comes off the wall: a curved metal plate, scorched on its back.",5)
20340   CALL SND(2)
20350   TCOST=1
20360   REDRAW=-1
20370 END PROC
20380 ' ---- climbing, hiding, waiting
20390 PROC DOCLIMB
20400   IF A=0 THEN
20410     IF PART$="UP" THEN
20420       CALL DOMOVE(5)
20430       EXIT PROC
20440     END IF
20450     IF PART$="DOWN" THEN
20460       IF ONCHAIR THEN
20470         ONCHAIR=0
20480         REDRAW=-1
20490         CALL SAY("You step down from the chair.",1)
20500         TCOST=1
20510       ELSE
20520         CALL DOMOVE(6)
20530       END IF
20540       EXIT PROC
20550     END IF
20560     IF V=20 AND ONCHAIR THEN
20570       ONCHAIR=0
20580       REDRAW=-1
20590       CALL SAY("You step down from the chair.",1)
20600       TCOST=1
20610       EXIT PROC
20620     END IF
20630     IF V=20 AND HIDING THEN
20640       CALL UNHIDE
20650       EXIT PROC
20660     END IF
20670     CALL SAY("Climb what?",1)
20680     EXIT PROC
20690   END IF
20700   LASTOBJ=A
20710   IF A=K_CHAIR THEN
20720     IF HIDING THEN
20730       CALL SAY("Not while you hide.",1)
20740       EXIT PROC
20750     END IF
20760     IF ONCHAIR THEN
20770       CALL SAY("You are standing on it already.",1)
20780       EXIT PROC
20790     END IF
20800     IF OROOM(A)=-1 THEN
20810       OROOM(A)=ROOM
20820       CALL FLOORSPOT(A,ROOM)
20830     END IF
20840     ONCHAIR=-1
20850     REDRAW=-1
20860     CALL SAY("You put the chair down and climb onto it. It wobbles, but holds.",1)
20870     IF ROOM=R_LIVING THEN
20880       CALL SAY("From up here you can see the top of the tall cupboard: "+FN KIDS$(K_CUPBOARD,2)+".",5)
20890     ELSE
20900       CALL SAY("Not much to see up here. The ceiling is as dusty as everything else.",1)
20910     END IF
20920     TCOST=1
20930     EXIT PROC
20940   END IF
20950   IF A=K_LADDER OR A=K_STAIRSH THEN
20960     CALL DOMOVE(5)
20970     EXIT PROC
20980   END IF
20990   IF A=K_CSTAIRS THEN
21000     CALL DOMOVE(6)
21010     EXIT PROC
21020   END IF
21030   IF A=K_CUPBOARD THEN
21040     CALL SAY("You are not a squirrel. Standing on a chair would do better.",1)
21050     EXIT PROC
21060   END IF
21070   CALL SAY("You can't climb that.",1)
21080 END PROC
21090 PROC DOHIDE
21100   LOCAL S
21110   IF HIDING THEN
21120     CALL SAY("You are hiding already.",1)
21130     EXIT PROC
21140   END IF
21150   S=FN HIDESPOT(ROOM)
21160   IF A>0 AND FN HASFL(A,"X") THEN S=A
21170   IF S=0 THEN
21180     CALL SAY("There is no good place to hide here.",1)
21190     EXIT PROC
21200   END IF
21210   IF ONCHAIR THEN ONCHAIR=0
21220   HIDING=-1
21230   HIDEOBJ=S
21240   CALL SAY("You squeeze into hiding in "+ONAME$(S)+" and hold very still. You can peek out (LOOK) or COME OUT.",3)
21250   TCOST=1
21260   REDRAW=-1
21270 END PROC
21280 ' ---- giving, feeding, eating
21290 PROC FEEDGLORP
21300   IF NOT GLORPHERE THEN
21310     CALL SAY("The glorp is full already.",1)
21320     EXIT PROC
21330   END IF
21340   GLORPHERE=0
21350   OROOM(K_JELLY)=0
21360   OIN(K_JELLY)=0
21370   OROOM(K_GLORP)=R_LANDING
21380   OIN(K_GLORP)=0
21390   OHID(K_GLORP)=0
21400   OHID(K_NOZZLE)=0
21410   CALL SAY("You push the jar of green jelly toward the glorp. All three eyes light up. It slurps the jelly down, jar and all, burps in three colors and rolls happily out from under the bed, out of the door and onto the landing.",5)
21420   CALL SAY("Behind the place where it lay, something large and metallic is revealed.",5)
21430   CALL SND(4)
21440   TCOST=1
21450   REDRAW=-1
21460 END PROC
21470 PROC DOGIVE
21480   IF A=0 THEN
21490     CALL SAY("Give what to whom?",1)
21500     EXIT PROC
21510   END IF
21520   IF B=0 THEN
21530     IF A=K_JELLY AND FN VIS(K_GLORP) THEN
21540       CALL FEEDGLORP
21550     ELSE
21560       CALL SAY("Give it to whom? There is nobody here who would take it.",1)
21570     END IF
21580     EXIT PROC
21590   END IF
21600   IF B=K_GLORP THEN
21610     IF A=K_JELLY THEN
21620       CALL FEEDGLORP
21630     ELSE
21640       CALL SAY("The glorp sniffs "+ONAME$(A)+" and turns up its nose. Nose, one of them. It wants something green and wobbly.",1)
21650     END IF
21660     EXIT PROC
21670   END IF
21680   IF B=K_DEQQ THEN
21690     CALL SAY("Deqq looks at what you offer, then at you, and does not take it.",2)
21700     EXIT PROC
21710   END IF
21720   CALL SAY(ONAME$(B)+" doesn't want it.",1)
21730 END PROC
21740 PROC DOEAT
21750   IF A=0 THEN
21760     CALL SAY("Eat what?",1)
21770     EXIT PROC
21780   END IF
21790   IF A=K_FRUIT THEN
21800     OROOM(A)=0
21810     OIN(A)=0
21820     CALL SAY("You bite into the fruit. It tastes of cinnamon and thunder. Your hair stands on end and a small spark jumps from your ear. Nothing else happens.",1)
21830     TCOST=1
21840     REDRAW=-1
21850     EXIT PROC
21860   END IF
21870   CALL SAY("You'd rather not eat "+ONAME$(A)+". Not in this house.",1)
21880 END PROC
21890 PROC DONOISE
21900   IF HIDING THEN
21910     CALL SAY("You bite your tongue instead. Hiding people keep quiet.",3)
21920     EXIT PROC
21930   END IF
21940   CALL SAY("You make a noise. The house does not applaud.",1)
21950   IF DQHOME AND ROOM>=R_HALL THEN
21960     DQALERT=-1
21970     CALL SAY("Somewhere in the house, a floorboard answers.",4)
21980   END IF
21990   TCOST=1
22000 END PROC
22010 PROC DOTALK
22020   IF DQHOME AND DQROOM=ROOM AND ROOM>=R_HALL AND NOT HIDING THEN
22030     CALL SAY("The translator on Deqq's collar crackles: '"+CMT$(FN RN(NCMT))+"'",2)
22040   ELSE
22050     CALL SAY("You talk to yourself. The house listens politely.",1)
22060   END IF
22070   TCOST=1
22080 END PROC
22090 PROC DOLISTEN
22100   LOCAL N
22110   IF HIDING AND DQHOME THEN
22120     CALL SAY("Your own heartbeat. And somewhere, slow footsteps.",3)
22130     EXIT PROC
22140   END IF
22150   IF DQHOME AND ROOM>=R_HALL THEN
22160     CALL SAY("The house creaks. Something moves through it, taking its time.",3)
22170     EXIT PROC
22180   END IF
22190   IF DQHOME THEN
22200     CALL SAY("From inside the house you hear a faint, slow humming.",3)
22210     EXIT PROC
22220   END IF
22230   CALL SAY("Quiet. A clock ticks somewhere. The wind plays with the washing line.",1)
22240 END PROC
22250 PROC DOSMELL
22260   IF ROOM=R_KITCHEN THEN
22270     CALL SAY("Cinnamon, burnt metal and something that cannot be cabbage.",1)
22280   ELSE
22290     IF ROOM=R_CRASH THEN
22300       CALL SAY("Scorched metal and ozone.",1)
22310     ELSE
22320       CALL SAY("Dust, old wood, and a trace of ozone, like after lightning.",1)
22330     END IF
22340   END IF
22350 END PROC
22360 ' ---- keys, safe, dial
22370 PROC DOUNLOCK
22380   LOCAL T
22390   IF NUM$<>"" THEN
22400     CALL DODIAL
22410     EXIT PROC
22420   END IF
22430   T=A
22440   IF T=0 OR T=K_KEY THEN
22450     IF B>0 THEN T=B
22460   END IF
22470   IF T=0 THEN
22480     IF ROOM=R_BACKYD THEN T=K_BACKDOORO
22490     IF ROOM=R_WESTG THEN T=K_HATCHO
22500     IF ROOM=R_WELLG THEN T=K_SHEDDOORO
22510     IF ROOM=R_ATTIC THEN T=K_SAFE
22520   END IF
22530   IF T=0 THEN
22540     CALL SAY("Unlock what?",1)
22550     EXIT PROC
22560   END IF
22570   LASTOBJ=T
22580   IF T=K_SAFE THEN
22590     CALL SAY("The safe has no keyhole, only a dial with three digits: DIAL 123.",1)
22600     EXIT PROC
22610   END IF
22620   IF T=K_BACKDOORI OR T=K_HATCHI OR T=K_FRONTDOORO OR T=K_HALLDOOR THEN
22630     CALL SAY("You can open that without a key or a bolt. Or can't, but not with a key.",1)
22640     EXIT PROC
22650   END IF
22660   IF T<>K_BACKDOORO AND T<>K_HATCHO AND T<>K_SHEDDOORO THEN
22670     CALL SAY("You can't unlock that.",1)
22680     EXIT PROC
22690   END IF
22700   IF OROOM(K_KEY)<>-1 THEN
22710     CALL SAY("You need a key for that. Spare keys are hidden in the oddest places.",1)
22720     EXIT PROC
22730   END IF
22740   IF T=K_BACKDOORO THEN
22750     IF NOT BACKLOCK THEN
22760       CALL SAY("It is unlocked already.",1)
22770       EXIT PROC
22780     END IF
22790     BACKLOCK=0
22800     CALL SAY("The spare key turns with a click. The back door is unlocked.",1)
22810   END IF
22820   IF T=K_HATCHO THEN
22830     IF NOT HATCHLOCK THEN
22840       CALL SAY("It is unlocked already.",1)
22850       EXIT PROC
22860     END IF
22870     HATCHLOCK=0
22880     CALL SAY("The spare key opens the padlock. The hatch can be opened now.",1)
22890   END IF
22900   IF T=K_SHEDDOORO THEN
22910     IF NOT SHEDLOCK THEN
22920       CALL SAY("It is unlocked already.",1)
22930       EXIT PROC
22940     END IF
22950     SHEDLOCK=0
22960     CALL SAY("The spare key opens the padlock. The shed door can be opened now.",1)
22970   END IF
22980   CALL SND(3)
22990   TCOST=1
23000 END PROC
23010 PROC DODIAL
23020   IF ROOM<>R_ATTIC OR NOT FN VIS(K_SAFE) THEN
23030     CALL SAY("There is no dial here.",1)
23040     EXIT PROC
23050   END IF
23060   IF NUM$="" THEN
23070     CALL SAY("The dial takes three digits. DIAL 123, for instance.",1)
23080     EXIT PROC
23090   END IF
23100   IF SAFEOPEN THEN
23110     CALL SAY("The safe is open already.",1)
23120     EXIT PROC
23130   END IF
23140   TCOST=1
23150   IF NUM$="731" THEN
23160     SAFEOPEN=-1
23170     OOPEN(K_SAFE)=-1
23180     CALL SAY("You turn the dial to 7, to 3, to 1. Click, clack, clunk: the heavy door swings open. Inside: "+FN KIDS$(K_SAFE,1)+".",5)
23190     CALL SND(5)
23200     REDRAW=-1
23210   ELSE
23220     CALL SAY("You dial "+NUM$+". The dial clicks and nothing else happens. Somewhere there is a clue to the right numbers.",1)
23230   END IF
23240 END PROC
23250 ' ---- push and pull
23260 PROC DOPUSH
23270   IF A=0 THEN
23280     CALL SAY("Push what?",1)
23290     EXIT PROC
23300   END IF
23310   LASTOBJ=A
23320   IF A=K_PLUG AND V=19 THEN
23330     CALL DOUNPLUG
23340     EXIT PROC
23350   END IF
23360   IF A=K_WELL AND V=19 THEN
23370     CALL SAY("The rope is heavy and the windlass wants to be turned, not pulled.",1)
23380     EXIT PROC
23390   END IF
23400   IF A=K_WASHER THEN
23410     CALL DOTURN
23420     EXIT PROC
23430   END IF
23440   IF A=K_CHAIR THEN
23450     CALL SAY("The chair scrapes across the floor. That doesn't help anyone.",1)
23460     EXIT PROC
23470   END IF
23480   IF OSIZE(A)>0 THEN
23490     CALL SAY("Nothing happens.",1)
23500   ELSE
23510     CALL SAY("It doesn't budge.",1)
23520   END IF
23530 END PROC
23540 ' ------------------------------------------------------------------------------------------------ building the ship
23550 PROC INSTALLPART(I)
23560   LOCAL N
23570   OROOM(I)=100
23580   OIN(I)=0
23590   IF I=K_COIL THEN CALL SAY("The flux coil slides into the engine mount and begins to hum.",5)
23600   IF I=K_COMPASS THEN CALL SAY("The star compass clicks into the helm. Its needle swings and locks onto a point very far away.",5)
23610   IF I=K_CRYSTAL THEN CALL SAY("The warp crystal drops into its cradle. The air around the wreck ripples.",5)
23620   IF I=K_NOZZLE THEN CALL SAY("The thruster nozzle bolts onto the stern with a clang that rolls over the hills.",5)
23630   IF I=K_FUELCELL THEN CALL SAY("The fuel cell slots into the engine bay and a green light comes on somewhere inside the wreck.",5)
23640   IF I=K_PLATE THEN CALL SAY("The hull plate fits exactly. For the first time the wreck looks like a ship and not like a skeleton.",5)
23650   IF I=K_GLYPH THEN CALL SAY("The ignition glyph settles into the console and glows steadily.",5)
23660   IF I=K_ANTENNA THEN CALL SAY("The antenna rises from the roof and turns slowly, searching the sky.",5)
23670   CALL SND(6)
23680   N=FN BUILT
23690   CALL SAY("Parts built in: "+STR$(N)+" of 8.",7)
23700   IF N=HALF THEN
23710     CALL SAY("A deep note rolls over the hills. The wreck looks like a ship again. Far away, something turns its head toward you.",3)
23720   END IF
23730   IF N=8 THEN ENDREQ=-1
23740   REDRAW=-1
23750 END PROC
23760 FUNC LASTW$(P$)
23770   LOCAL I
23780   FOR I=LEN(P$) TO 1 STEP -1
23790     IF MID$(P$,I,1)=" " THEN RETURN MID$(P$,I+1)
23800   NEXT I
23810   RETURN P$
23820 END FUNC
23830 PROC DOTAKEALL
23840   LOCAL I,N,K$
23850   N=0
23860   FOR I=1 TO NOBJ
23870     IF OSIZE(I)>0 AND OROOM(I)<>-1 AND FN VIS(I) AND FN TAKEBLOCK$(I)="" AND FN USED+OSIZE(I)<=CAP THEN
23880       OIN(I)=0
23890       OROOM(I)=-1
23900       N=N+1
23910       IF N>1 THEN K$=K$+", "
23920       K$=K$+ONAME$(I)
23930     END IF
23940   NEXT I
23950   IF N=0 THEN
23960     CALL SAY("There is nothing here you can take, or your hands are full.",1)
23970   ELSE
23980     CALL SAY("Taken: "+K$+".",1)
23990     CALL SND(2)
24000     TCOST=1
24010     REDRAW=-1
24020   END IF
24030 END PROC
24040 PROC DODROPALL
24050   LOCAL I,N
24060   N=0
24070   FOR I=1 TO NOBJ
24080     IF OROOM(I)=-1 AND OIN(I)=0 THEN
24090       OROOM(I)=ROOM
24100       CALL FLOORSPOT(I,ROOM)
24110       N=N+1
24120     END IF
24130   NEXT I
24140   IF N=0 THEN
24150     CALL SAY("You carry nothing.",1)
24160   ELSE
24170     CALL SAY("You put everything down.",1)
24180     TCOST=1
24190     REDRAW=-1
24200   END IF
24210 END PROC
24220 PROC DOINSTALL
24230   LOCAL I,N
24240   IF ROOM<>R_CRASH THEN
24250     CALL SAY("There is no ship here to build parts into. The wreck lies north of the back yard.",1)
24260     EXIT PROC
24270   END IF
24280   IF A>0 AND A<>K_SHIP THEN
24290     IF NOT FN HASFL(A,"P") THEN
24300       CALL SAY(ONAME$(A)+" doesn't belong to the ship.",1)
24310       EXIT PROC
24320     END IF
24330     IF OROOM(A)<>-1 THEN
24340       CALL SAY("You must carry the part to build it in.",1)
24350       EXIT PROC
24360     END IF
24370     CALL INSTALLPART(A)
24380     TCOST=1
24390     EXIT PROC
24400   END IF
24410   N=0
24420   FOR I=1 TO NOBJ
24430     IF OROOM(I)=-1 AND OIN(I)=0 AND FN HASFL(I,"P") THEN
24440       CALL INSTALLPART(I)
24450       N=N+1
24460     END IF
24470   NEXT I
24480   IF N=0 THEN
24490     CALL SAY("You carry no part of the ship. Find them in the house and the garden.",1)
24500   ELSE
24510     TCOST=1
24520   END IF
24530 END PROC
24540 ' USE x: does the obvious thing
24550 PROC DOUSE
24560   IF ROOM=R_CRASH AND (A=0 OR A=K_SHIP OR FN HASFL(A,"P")) THEN
24570     CALL DOINSTALL
24580     EXIT PROC
24590   END IF
24600   IF A=0 THEN
24610     CALL SAY("Use what?",1)
24620     EXIT PROC
24630   END IF
24640   LASTOBJ=A
24650   IF A=K_CRANK THEN
24660     CALL ATTACHCRANK
24670     EXIT PROC
24680   END IF
24690   IF A=K_SCREWDRIVER THEN
24700     IF ROOM=R_BATH AND MIRRORON THEN
24710       A=K_MIRROR
24720       CALL DOUNSCREW
24730     ELSE
24740       CALL SAY("There is nothing here with four-pointed screws.",1)
24750     END IF
24760     EXIT PROC
24770   END IF
24780   IF A=K_KEY THEN
24790     A=0
24800     CALL DOUNLOCK
24810     EXIT PROC
24820   END IF
24830   IF A=K_LAMP THEN
24840     CALL DOTURN
24850     EXIT PROC
24860   END IF
24870   IF A=K_CHAIR THEN
24880     CALL DOCLIMB
24890     EXIT PROC
24900   END IF
24910   IF A=K_JELLY AND FN VIS(K_GLORP) THEN
24920     CALL FEEDGLORP
24930     EXIT PROC
24940   END IF
24950   IF A=K_PLUG THEN
24960     CALL DOUNPLUG
24970     EXIT PROC
24980   END IF
24990   CALL SAY("You can't see how to use that here. Try a more exact command.",1)
25000 END PROC
25010 ' ------------------------------------------------------------------------------------------------ help, hints and status
25020 PROC DOHELP
25030   CALL SAY("Moving: N S E W, U (up), D (down), IN, OUT. LOOK (L), EXAMINE x (X), INVENTORY (I), TAKE x, DROP x, PUT x IN/ON/UNDER y, OPEN/CLOSE x.",5)
25040   CALL SAY("More: TURN x ON/OFF, GIVE x TO y, CLIMB x, HIDE, COME OUT, WAIT, READ x, LOOK UNDER/IN/BEHIND x, UNLOCK, DIAL 123, INSTALL x, USE x, UNPLUG x.",5)
25050   CALL SAY("Also HINT, STATUS, SAVE, LOAD, QUIT. Page up shows the older messages. Only actions that change something take a turn; Deqq counts turns.",5)
25060   CALL SAY("Goal: find the 8 parts of Deqq's ship and carry them to the wreck (north of the back yard). You can carry 6 slots.",5)
25070 END PROC
25080 PROC DOHINT
25090   LOCAL I,K
25100   K=0
25110   FOR I=1 TO 8
25120     IF K=0 AND OROOM(PARTK(I))<>100 AND OROOM(PARTK(I))<>-1 THEN K=I
25130   NEXT I
25140   IF K=0 THEN
25150     CALL SAY("You hold all the parts that are left. Carry them to the wreck, north of the back yard, and INSTALL them.",5)
25160   ELSE
25170     CALL SAY(HINT$(K),5)
25180   END IF
25190 END PROC
25200 PROC DOSTATUS
25210   CALL SAY("Parts built into the wreck: "+STR$(FN BUILT)+" of 8. Carrying "+STR$(FN USED)+" of "+STR$(CAP)+" slots. Turn "+STR$(TURN)+".",1)
25220 END PROC
25230 ' ------------------------------------------------------------------------------------------------ one command
25240 PROC EXECUTE(L$)
25250   TCOST=0
25260   CALL PARSE(L$)
25270   IF NW=0 THEN EXIT PROC
25280   IF V=0 THEN
25290     CALL SAY("I don't know the word '"+W$(1)+"'. Type HELP for the commands.",1)
25300     EXIT PROC
25310   END IF
25320   IF V=-1 THEN
25330     CALL SAY("Go where? (N S E W U D IN OUT)",1)
25340     EXIT PROC
25350   END IF
25360   IF HIDING THEN
25370     IF NOT (V=10 OR V=45 OR V=22 OR V=40 OR V=8 OR V=14 OR V=28 OR V=29 OR V=30 OR V=31 OR V=32 OR V=33 OR V=38 OR V=11) THEN
25380       CALL SAY("You are hiding. WAIT, peek (LOOK) or COME OUT.",3)
25390       EXIT PROC
25400     END IF
25410   END IF
25420   IF V=12 AND (P1$="ALL" OR P1$="EVERYTHING") THEN
25430     CALL DOTAKEALL
25440     EXIT PROC
25450   END IF
25460   IF V=9 THEN
25470     CALL SAY("Go where? (N S E W U D IN OUT)",1)
25480     EXIT PROC
25490   END IF
25500   IF V=10 AND (P1$="AROUND" OR P1$="HERE" OR P1$="ROUND") THEN
25510     CALL LOOKROOM(-1)
25520     EXIT PROC
25530   END IF
25540   IF V=13 AND (P1$="ALL" OR P1$="EVERYTHING") THEN
25550     CALL DODROPALL
25560     EXIT PROC
25570   END IF
25580   IF V=11 AND (P1$="ME" OR P1$="SELF" OR P1$="MYSELF") THEN
25590     CALL SAY("You look like somebody who should not be here, and you are carrying "+STR$(FN USED)+" of "+STR$(CAP)+" slots of things that are not yours.",1)
25600     EXIT PROC
25610   END IF
25620   IF (V=11 OR V=10) AND FN INLIST(FN LASTW$(P1$)," SKY HOUSE ROAD WALL WALLS FLOOR CEILING WINDOW WINDOWS GARDEN GATE ROOF GRASS GROUND ROOM ") THEN
25630     CALL SAY("Nothing special about it. This house has enough special things already.",1)
25640     EXIT PROC
25650   END IF
25660   IF V>8 AND NOT FN INLIST(STR$(V)," 42 44 30 31 26 28 29 33 32 ") THEN
25670     IF (P1$<>"" AND A=0) OR (P2$<>"" AND B=0) THEN
25680       CALL NOSEE
25690       EXIT PROC
25700     END IF
25710   END IF
25720   IF V>=1 AND V<=8 THEN
25730     IF V=7 AND NUM$<>"" THEN
25740       CALL DODIAL
25750     ELSE
25760       CALL DOMOVE(V)
25770     END IF
25780   END IF
25790   IF V=10 OR V=45 THEN CALL DOLOOK
25800   IF V=11 THEN
25810     IF A>0 THEN
25820       LASTOBJ=A
25830       CALL EXAMINE(A)
25840     ELSE
25850       CALL SAY("Examine what?",1)
25860     END IF
25870   END IF
25880   IF V=12 THEN CALL DOTAKE
25890   IF V=13 THEN CALL DODROP
25900   IF V=14 THEN CALL DOINV
25910   IF V=15 THEN CALL DOOPEN
25920   IF V=16 THEN CALL DOCLOSE
25930   IF V=17 THEN CALL DOTURN
25940   IF V=41 THEN
25950     PART$="ON"
25960     CALL DOTURN
25970   END IF
25980   IF V=18 OR V=19 THEN CALL DOPUSH
25990   IF V=20 THEN CALL DOCLIMB
26000   IF V=21 THEN CALL DOHIDE
26010   IF V=22 THEN
26020     IF HIDING THEN
26030       CALL SAY("You hold your breath and wait.",3)
26040     ELSE
26050       CALL SAY("Time passes.",6)
26060     END IF
26070     TCOST=1
26080   END IF
26090   IF V=23 THEN CALL DOGIVE
26100   IF V=24 THEN
26110     IF A>0 THEN
26120       LASTOBJ=A
26130       IF A=K_PHOTO THEN
26140         CALL DOTURNPHOTO
26150       ELSE
26160         CALL EXAMINE(A)
26170       END IF
26180     ELSE
26190       CALL SAY("Read what?",1)
26200     END IF
26210   END IF
26220   IF V=25 THEN CALL DOUNLOCK
26230   IF V=26 THEN CALL DODIAL
26240   IF V=27 THEN CALL DOUSE
26250   IF V=28 THEN CALL DOHELP
26260   IF V=29 THEN CALL DOHINT
26270   IF V=30 THEN REQSAVE=1
26280   IF V=31 THEN REQLOAD=1
26290   IF V=32 THEN QUITREQ=1
26300   IF V=33 THEN CALL DOSTATUS
26310   IF V=34 THEN CALL DOPUT
26320   IF V=35 THEN CALL DOUNPLUG
26330   IF V=36 THEN CALL DOUNSCREW
26340   IF V=37 THEN
26350     IF A>0 THEN
26360       LASTOBJ=A
26370       IF FN HASFL(A,"U") AND NOT OSEEN(A) THEN
26380         CALL REVEAL(A)
26390       ELSE
26400         CALL EXAMINE(A)
26410       END IF
26420     ELSE
26430       CALL LOOKROOM(-1)
26440     END IF
26450   END IF
26460   IF V=38 THEN CALL DOLISTEN
26470   IF V=39 THEN CALL DOSMELL
26480   IF V=40 THEN
26490     IF HIDING THEN
26500       CALL UNHIDE
26510     ELSE
26520       IF ONCHAIR THEN
26530         ONCHAIR=0
26540         REDRAW=-1
26550         CALL SAY("You step down from the chair.",1)
26560         TCOST=1
26570       ELSE
26580         IF EX(ROOM,8)>0 THEN CALL DOMOVE(8) ELSE CALL SAY("Leave by which way?",1)
26590       END IF
26600     END IF
26610   END IF
26620   IF V=42 THEN CALL DOTALK
26630   IF V=43 THEN CALL DOEAT
26640   IF V=44 THEN CALL DONOISE
26650   IF V=46 THEN CALL SAY("Knock, knock. Nobody answers. Or too many somebodies answer at once.",1)
26660   IF V=47 THEN CALL SAY("There is nothing here you need to lock.",1)
26670   IF V=48 THEN CALL DOWIND
26680 END PROC
26690 ' ------------------------------------------------------------------------------------------------ Deqq
26700 ' Deqq is either away (DQHOME = 0, DQT turns until he comes back) or at home inside the house (DQROOM, DQSTAY turns until he goes out again).
26710 ' While fewer than HALF parts are built into the wreck he hunts for the player: in the same room he throws the player out (or finds the
26720 ' hiding place with some luck), bolts the front door and leaves it open only when he goes out again. With HALF parts or more he only watches.
26730 ' The step toward a room: distances through the house graph (rooms HALL .. ATTIC; stairs and doors, not the garden).
26740 FUNC NEXTSTEP(F,T)
26750   LOCAL R,D,N,K,CH,BEST
26760   FOR R=0 TO 19
26770     DS(R)=-1
26780   NEXT R
26790   DS(T)=0
26800   CH=-1
26810   WHILE CH
26820     CH=0
26830     FOR R=R_HALL TO R_ATTIC
26840       IF DS(R)<0 THEN
26850         FOR D=1 TO 6
26860           N=EX(R,D)
26870           IF N>=R_HALL AND DS(N)>=0 AND DS(R)<0 THEN
26880             DS(R)=DS(N)+1
26890             CH=-1
26900           END IF
26910         NEXT D
26920       END IF
26930     NEXT R
26940   WEND
26950   BEST=F
26960   FOR D=1 TO 6
26970     N=EX(F,D)
26980     IF N>=R_HALL AND DS(N)>=0 AND DS(N)<DS(F) THEN BEST=N
26990   NEXT D
27000   RETURN BEST
27010 END FUNC
27020 ' a random neighbor of room F inside the house
27030 FUNC WANDER(F)
27040   LOCAL D,N,K
27050   K=0
27060   FOR D=1 TO 6
27070     N=EX(F,D)
27080     IF N>=R_HALL THEN
27090       K=K+1
27100       WC(K)=N
27110     END IF
27120   NEXT D
27130   IF K=0 THEN RETURN F
27140   RETURN WC(1+FN RN(K))
27150 END FUNC
27160 FUNC NEIGHBOR(F,T)
27170   LOCAL D
27180   FOR D=1 TO 6
27190     IF EX(F,D)=T THEN RETURN -1
27200   NEXT D
27210   RETURN 0
27220 END FUNC
27230 PROC THROWOUT(FOUND)
27240   IF FOUND THEN
27250     CALL SAY("The door of your hiding place opens without a sound. A long grey face looks in, with eyes like two wells at midnight. 'FOUND,' says the translator on his collar.",4)
27260   ELSE
27270     CALL SAY("Deqq turns his head all the way toward you. A long grey hand closes on your collar. 'MY HOUSE,' says the translator on his collar, quite calmly.",4)
27280   END IF
27290   CALL SAY("You fly through the hall, across the doorstep, into the gravel. The front door slams. A bolt slides shut.",4)
27300   CALL SND(7)
27310   ROOM=R_FRONT
27320   HIDING=0
27330   ONCHAIR=0
27340   FRONTOPEN=0
27350   DQROOM=R_HALL
27360   REDRAW=-1
27370   CALL LOOKROOM(0)
27380   CALL SAY("(Deqq will go out again after a while, and leave the door open. The back door and the hatch are other ways in, if you have the key. Come back with more of the ship built and he will only watch.)",6)
27390 END PROC
27400 PROC WARN(T)
27410   IF ROOM>=R_HALL THEN
27420     IF T=3 THEN CALL SAY("A thin, high humming comes from outside, rising and falling. Like something small landing.",3)
27430     IF T=2 THEN CALL SAY("Gravel crunches on the road. Slow, heavy steps are coming around the house.",3)
27440     IF T=1 THEN CALL SAY("The handle of the front door turns. Deqq is coming in! HIDE, or get out by another door.",4)
27450   ELSE
27460     IF ROOM=R_CRASH THEN
27470       IF T=3 THEN CALL SAY("A thin humming on the wind. Far off a lantern bobs along the road.",3)
27480       IF T=2 THEN CALL SAY("A tall figure passes the foot of the track. It does not look up. It does not need to.",3)
27490       IF T=1 THEN CALL SAY("In the distance a door closes: the front door of the house.",3)
27500     ELSE
27510       IF T=3 THEN CALL SAY("A thin humming on the wind. A pale light flickers over the hills.",3)
27520       IF T=2 THEN CALL SAY("Footsteps on the gravel somewhere around the house. A tall shadow slides across the windows.",3)
27530       IF T=1 THEN CALL SAY("A tall grey figure comes around the corner of the house, heading for the front door.",4)
27540     END IF
27550   END IF
27560   CALL SND(8)
27570 END PROC
27580 PROC HUM(HF)
27590   IF HL THEN EXIT PROC
27600   IF HF THEN
27610     WAVE 1,0
27620     VIBRATO 1,0.4,0.25
27630     SOUND 1,55,-1,0.09
27640   ELSE
27650     SOUND OFF 1
27660     VIBRATO 1,0,0
27670   END IF
27680 END PROC
27690 PROC ARRIVE
27700   DQHOME=-1
27710   CALL HUM(-1)
27720   DQROOM=R_HALL
27730   IF FN BUILT>=HALF THEN
27740     FRONTOPEN=-1
27750     DQSTAY=45+FN RN(15)
27760     IF ROOM>=R_HALL THEN
27770       CALL SAY("Deqq comes in through the front door and stops. He looks at you for a long moment, nods once, and stays in the shadows to watch.",2)
27780     ELSE
27790       CALL SAY("Deqq goes into the house. He leaves the front door open behind him.",3)
27800     END IF
27810   ELSE
27820     FRONTOPEN=0
27830     DQSTAY=16+FN RN(8)
27840     IF ROOM>=R_HALL THEN
27850       CALL SAY("The front door bangs open. Heavy steps in the hall. Deqq is home, and he is looking for somebody.",4)
27860     ELSE
27870       CALL SAY("Deqq goes into the house. The front door slams and a bolt slides shut behind him.",4)
27880     END IF
27890   END IF
27900   CALL SND(7)
27910   REDRAW=-1
27920 END PROC
27930 PROC DQLEAVE
27940   DQHOME=0
27950   CALL HUM(0)
27960   DQT=34+FN RN(18)
27970   FRONTOPEN=-1
27980   IF ROOM>=R_HALL THEN
27990     IF DQROOM=ROOM AND NOT HIDING THEN
28000       CALL SAY("Deqq looks at you one last time, then at the door, and goes out. He does not close it behind him.",2)
28010     ELSE
28020       CALL SAY("Heavy steps cross the hall. The front door opens and closes, no, it does not close: it stays open. The steps crunch away across the gravel.",3)
28030     END IF
28040   ELSE
28050     IF ROOM=R_FRONT THEN
28060       CALL SAY("The front door opens. Deqq steps out, looks around slowly, and walks off up the road. He leaves the door open.",3)
28070     ELSE
28080       CALL SAY("Somewhere a door opens. Footsteps crunch away along the road, and the humming fades.",3)
28090     END IF
28100   END IF
28110   DQROOM=0
28120   REDRAW=-1
28130 END PROC
28140 ' what happens when Deqq is in the same room as the player (and the player is inside the house)
28150 PROC ENCOUNTER
28160   LOCAL R
28170   IF NOT DQHOME OR ROOM<R_HALL THEN EXIT PROC
28180   IF DQROOM=ROOM THEN
28190     IF FN BUILT>=HALF THEN
28200       IF DQCOOL<=0 AND FN RN(100)<40 THEN
28210         CALL SAY(CMT$(FN RN(NCMT)),2)
28220         DQCOOL=3
28230       END IF
28240     ELSE
28250       IF HIDING THEN
28260         IF FN RN(100)<28 THEN
28270           CALL THROWOUT(-1)
28280         ELSE
28290           R=FN RN(4)
28300           IF R=0 THEN CALL SAY("A shadow slides across the crack. Deqq is standing right in front of your hiding place. You stop breathing.",3)
28310           IF R=1 THEN CALL SAY("Deqq sniffs the air. Slowly. Like a dog that has found something and is deciding whether to tell you.",3)
28320           IF R=2 THEN CALL SAY("The floorboards creak under very large feet. The steps stop. Start again. Stop.",3)
28330           IF R=3 THEN CALL SAY("Two black eyes sweep over the room like lamps. They move on. Barely.",3)
28340         END IF
28350       ELSE
28360         CALL THROWOUT(0)
28370       END IF
28380     END IF
28390   ELSE
28400     IF FN NEIGHBOR(ROOM,DQROOM) AND FN BUILT<HALF AND FN RN(100)<45 THEN
28410       CALL SAY("Heavy footsteps in the next room. They are coming closer.",3)
28420     END IF
28430   END IF
28440 END PROC
28450 PROC DQMOVE
28460   LOCAL TG,WATCH
28470   WATCH=FN BUILT>=HALF
28480   TG=0
28490   IF ROOM>=R_HALL THEN
28500     IF WATCH THEN
28510       TG=ROOM
28520     ELSE
28530       IF NOT HIDING AND (DQALERT OR FN RN(100)<50) THEN TG=ROOM
28540       IF HIDING AND FN RN(100)<20 THEN TG=ROOM
28550     END IF
28560   END IF
28570   DQALERT=0
28580   IF TG>0 AND DQROOM<>TG THEN
28590     IF NOT WATCH OR FN RN(100)<75 THEN DQROOM=FN NEXTSTEP(DQROOM,TG)
28600   ELSE
28610     IF TG=0 AND FN RN(100)<55 THEN DQROOM=FN WANDER(DQROOM)
28620   END IF
28630 END PROC
28640 PROC DEQQTURN
28650   IF NOT DQHOME THEN
28660     DQT=DQT-1
28670     IF DQT>0 AND DQT<=3 THEN CALL WARN(DQT)
28680     IF DQT<=0 THEN CALL ARRIVE
28690   ELSE
28700     DQSTAY=DQSTAY-1
28710     DQCOOL=DQCOOL-1
28720     IF FN BUILT>=HALF AND NOT FRONTOPEN THEN
28730       FRONTOPEN=-1
28740       CALL SAY("Somewhere in the house a bolt slides back. The front door swings open by itself.",3)
28750     END IF
28760     CALL DQMOVE
28770     CALL ENCOUNTER
28780     IF DQSTAY<=0 THEN CALL DQLEAVE
28790   END IF
28800   IF DQHOME THEN OROOM(K_DEQQ)=DQROOM ELSE OROOM(K_DEQQ)=0
28810 END PROC
28820 ' ------------------------------------------------------------------------------------------------ the world's clock
28830 PROC DAYLIGHT
28840   LOCAL PH,NB
28850   PH=FN MINI(100,TURN/5)
28860   NB=INT(PH/8)
28870   IF NB=DAYSTEP THEN EXIT PROC
28880   DAYSTEP=NB
28890   TNR=1-0.003*PH
28900   TNG=1-0.0018*PH
28910   TNB=1
28920   CALL SETPAL(1-0.0045*PH)
28930   REDRAW=-1
28940 END PROC
28950 PROC ENDTURN
28960   TURN=TURN+1
28970   IF OVENCOOL>0 THEN
28980     OVENCOOL=OVENCOOL-1
28990     IF OVENCOOL=0 AND NOT OVENON THEN
29000       OVENHOT=0
29010       IF ROOM=R_KITCHEN THEN CALL SAY("The oven door has cooled down. You could open it now.",6)
29020     END IF
29030   END IF
29040   IF OIN(K_ICE)=K_SINK AND TAPON THEN
29050     MELT=MELT+1
29060     IF MELT>=2 THEN
29070       OROOM(K_ICE)=0
29080       OIN(K_ICE)=0
29090       OIN(K_COMPASS)=K_SINK
29100       OREL(K_COMPASS)=1
29110       OROOM(K_COMPASS)=0
29120       IF ROOM=R_KITCHEN THEN
29130         CALL SAY("The ice cracks, slides apart and melts away in the warm water. A brass star compass lies in the sink!",5)
29140         REDRAW=-1
29150       END IF
29160     ELSE
29170       IF ROOM=R_KITCHEN THEN CALL SAY("The block of ice shrinks in the hot water.",6)
29180     END IF
29190   END IF
29200   CALL DEQQTURN
29210   CALL DAYLIGHT
29220 END PROC
29230 ' ------------------------------------------------------------------------------------------------ drawing the rooms
29240 ' the things that lie inside / on / under P (relation REL) are drawn around the room position X,Y,Z
29250 ' things that lie on the floor of this room
29260 ' an open container: a dark opening on the front face
29270 PROC HUD
29280   LOCAL T$
29290   IF HL THEN EXIT PROC
29300   CALL PUTLBL(5,3,RNAME$(ROOM),FN CC(11,15))
29310   T$="Parts "+STR$(FN BUILT)+"/8   Pack "+STR$(FN USED)+"/"+STR$(CAP)
29320   CALL PUTLBL(5,14,T$,FN CC(15,14))
29330   IF ONCHAIR THEN CALL PUTLBL(5,25,"(standing on the chair)",FN CC(4,14))
29340   IF HIDING THEN CALL PUTLBL(250,225,"hiding - COME OUT to leave",FN CC(4,14))
29350 END PROC
29360 PROC DRAWGLORP(X,Z,UNDER)
29370   LOCAL SX,SY,S
29380   IF HL THEN EXIT PROC
29390   S=FN SCALE(Z)
29400   SX=FN PX(X,Z):SY=FN PY(0,Z)
29410   CALL FELLO(SX,SY-INT(14*S),INT(24*S)+1,INT(15*S)+1,FN CC(9,9))
29420   CALL FELLO(SX-INT(9*S),SY-INT(20*S),INT(3*S)+1,INT(3*S)+1,FN CC(4,15))
29430   CALL FELLO(SX,SY-INT(23*S),INT(3*S)+1,INT(3*S)+1,FN CC(4,15))
29440   CALL FELLO(SX+INT(9*S),SY-INT(20*S),INT(3*S)+1,INT(3*S)+1,FN CC(4,15))
29450   GFX LINE SX-INT(8*S),SY-INT(8*S),SX+INT(8*S),SY-INT(8*S),1
29460   IF UNDER THEN GFX LINE SX-INT(7*S),SY-INT(8*S),SX+INT(7*S),SY-INT(8*S),FN CC(0,15)
29470 END PROC
29480 ' ------------------------------------------------------------------------------------------------ the pictures (the W3D renderer)
29490 ' Since roadmap item 91 the places of the house are pictures of a small 3D world (the renderer W3D; demos/levels/deqq1.bas writes the level that is pasted at
29500 ' LEVELDATA): every place is a box of its own (place number R: x = (R mod 5) * 800, y = (R \ 5) * 700 of the plan), the furniture are blocks, the things you can
29510 ' pick up and Deqq are sprites (billboards) that are drawn with the old ICON / DEQQFIG procedures and grabbed as textures. The camera always stands in front of the
29520 ' box (the "dollhouse" view of the old pictures). The old drawing procedures (PX / PY / SCALE, ICON, DEQQFIG, DRAWGLORP ...) are still used to paint the sprites. In the console host
29530 ' (DEQQ_HEADLESS) nothing of this runs.
29540 FUNC PLOX(R)
29550   RETURN (R MOD 5)*800
29560 END FUNC
29570 FUNC PLOY(R)
29580   RETURN (R\5)*700
29590 END FUNC
29600 ' grabs the rectangle X0,Y0 .. X1,Y1 of the drawing layer as texture N (index 0 = see-through)
29610 PROC GRABAT(N,X0,Y0,X1,Y1)
29620   LOCAL A$
29630   GFX GRAB X0,Y0,X1,Y1,A$
29640   W3D TEXTURE N,A$
29650 END PROC
29660 ' plaster for walls and furniture of hue H (a game hue) as texture 100 + the library's hue: 64 x 170, a darker lower third, a light rim at the top
29670 PROC V3_PLAIN(N,H)
29680   LOCAL I
29690   GFX CLEAR FN CC(H,10)
29700   FOR I=1 TO 300
29710     GFX PSET INT(RND(1)*64),INT(RND(1)*170),FN CC(H,8+INT(RND(1)*4))
29720   NEXT I
29730   GFX LINE 0,0,63,40,FN CC(H,7),BF
29740   GFX LINE 0,40,63,41,FN CC(H,4),BF
29750   GFX LINE 0,160,63,162,FN CC(H,13),BF
29760   CALL GRABAT(N,0,0,63,169)
29770 END PROC
29780 ' a billboard picture: the frame (outline color 1) around a filled panel
29790 PROC V3_PANEL(N,W,H,C1,C2)
29800   GFX CLEAR
29810   GFX LINE 0,0,W-1,H-1,1,BF
29820   GFX LINE 2,2,W-3,H-3,C1,BF
29830   GFX LINE 5,5,W-6,H-6,C2,BF
29840   CALL GRABAT(N,0,0,W-1,H-1)
29850 END PROC
29860 PROC V3_TEXTURES
29870   LOCAL H,I,X,Y
29880   ' the plasters, the floors, the doors
29890   FOR H=0 TO 15
29900     IF H<>12 AND H<>13 THEN CALL V3_PLAIN(100+MHT(H),H)
29910   NEXT H
29920   CALL V3_PLAIN(100,0)
29930   CALL W3K_PLANKS(118)
29940   CALL W3K_CARPET(119,1)
29950   CALL W3K_TILES(120,10,0)
29960   CALL W3K_STONE(121,0)
29970   CALL W3K_GRASS(122)
29980   CALL W3K_STONE(123,13)
29990   CALL DA_FACADE
30000   CALL DA_FENCEBACK
30010   CALL DA_SPRITES
30020   CALL W3K_DOORTEX(130,12)
30030   ' 131: a doorway (dark brown, the door stands open)
30040   GFX CLEAR FN CC(12,3)
30050   GFX LINE 6,6,57,160,FN CC(12,1),BF
30060   GFX LINE 0,0,63,169,FN CC(1,5),B
30070   CALL GRABAT(131,0,0,63,169)
30080   ' 180: the night sky behind the walls (256 x 64)
30090   GFX CLEAR FN CC(8,1)
30100   FOR I=1 TO 60
30110     GFX PSET INT(RND(1)*256),INT(RND(1)*64),FN CC(0,6+INT(RND(1)*8))
30120   NEXT I
30130   CALL GRABAT(180,0,0,255,63)
30140   W3D SKY 180
30150   ' 190: dark opening, 191: closed double doors (brown), 192: drawer front
30160   CALL V3_PANEL(190,64,64,FN CC(1,3),FN CC(12,1))
30170   CALL V3_PANEL(191,64,64,FN CC(1,8),FN CC(1,10))
30180   GFX LINE 31,5,32,58,1,BF
30190   GFX CIRCLE 27,32,2,FN CC(4,12)
30200   GFX CIRCLE 36,32,2,FN CC(4,12)
30210   CALL GRABAT(191,0,0,63,63)
30220   CALL V3_PANEL(192,64,32,FN CC(1,8),FN CC(1,10))
30230   GFX LINE 20,14,43,17,FN CC(4,12),BF
30240   CALL GRABAT(192,0,0,63,31)
30250   ' 193 fridge closed, 194 fridge open (shelves), 195 oven off, 196 oven on, 197 oven hot
30260   CALL V3_PANEL(193,64,64,FN CC(0,11),FN CC(0,13))
30270   GFX LINE 52,12,54,52,FN CC(0,4),BF
30280   GFX LINE 5,28,58,29,FN CC(0,6),BF
30290   CALL GRABAT(193,0,0,63,63)
30300   CALL V3_PANEL(194,64,64,FN CC(0,9),FN CC(0,2))
30310   GFX LINE 5,20,58,21,FN CC(0,10),BF
30320   GFX LINE 5,36,58,37,FN CC(0,10),BF
30330   GFX LINE 5,52,58,53,FN CC(0,10),BF
30340   CALL GRABAT(194,0,0,63,63)
30350   CALL V3_PANEL(195,64,40,FN CC(7,6),FN CC(7,4))
30360   CALL V3_PANEL(196,64,40,FN CC(2,12),FN CC(2,9))
30370   CALL V3_PANEL(197,64,40,FN CC(3,10),FN CC(3,7))
30380   ' 198 washing machine door closed, 199 open, 200 running, 201 safe closed (dial)
30390   GFX CLEAR
30400   CALL FELLO(24,24,22,22,FN CC(13,7))
30410   CALL FELL(24,24,15,15,FN CC(15,9))
30420   CALL GRABAT(198,0,0,47,47)
30430   GFX CLEAR
30440   CALL FELLO(24,24,22,22,FN CC(13,7))
30450   CALL FELL(24,24,15,15,FN CC(12,1))
30460   CALL GRABAT(199,0,0,47,47)
30470   GFX CLEAR
30480   CALL FELLO(24,24,22,22,FN CC(13,7))
30490   CALL FELL(24,24,15,15,FN CC(15,13))
30500   CALL GRABAT(200,0,0,47,47)
30510   CALL V3_PANEL(201,64,64,FN CC(13,8),FN CC(13,10))
30520   CALL FELLO(32,32,10,10,FN CC(0,12))
30530   GFX LINE 32,32,37,27,1
30540   CALL GRABAT(201,0,0,63,63)
30550   ' 202 mirror (dull, with four screws), 203 mirror (clean)
30560   CALL V3_PANEL(202,64,48,FN CC(6,10),FN CC(6,12))
30570   CALL GRABAT(202,0,0,63,47)
30580   CALL V3_PANEL(203,64,48,FN CC(13,14),FN CC(13,11))
30590   CALL GRABAT(203,0,0,63,47)
30600   ' 204 clock, 205 painting of the house, 206 skylight, 207 blueprint, 208 ladder
30610   GFX CLEAR
30620   CALL FELLO(16,16,12,12,FN CC(11,15))
30630   GFX LINE 16,16,16,8,1
30640   GFX LINE 16,16,21,18,1
30650   CALL GRABAT(204,0,0,31,31)
30660   CALL V3_PANEL(205,64,48,FN CC(1,10),FN CC(7,11))
30670   GFX LINE 20,20,44,40,FN CC(11,12),BF
30680   GFX LINE 20,20,32,12,FN CC(2,8)
30690   GFX LINE 32,12,44,20,FN CC(2,8)
30700   CALL GRABAT(205,0,0,63,47)
30710   CALL V3_PANEL(206,64,48,FN CC(7,12),FN CC(7,14))
30720   GFX LINE 31,5,32,42,1,BF
30730   GFX LINE 5,23,58,24,1,BF
30740   CALL GRABAT(206,0,0,63,47)
30750   CALL V3_PANEL(207,64,40,FN CC(7,14),FN CC(7,11))
30760   GFX LINE 12,12,50,13,FN CC(0,14),BF
30770   GFX LINE 24,12,25,30,FN CC(0,14),BF
30780   CALL FELL(40,24,10,5,FN CC(0,14))
30790   CALL GRABAT(207,0,0,63,39)
30800   GFX CLEAR
30810   GFX LINE 4,0,6,63,FN CC(12,9),BF
30820   GFX LINE 33,0,35,63,FN CC(12,9),BF
30830   FOR Y=4 TO 60 STEP 14
30840     GFX LINE 4,Y,35,Y+2,FN CC(12,12),BF
30850   NEXT Y
30860   CALL GRABAT(208,0,0,39,63)
30870   ' 209 the telescope, 210 the star globe
30880   GFX CLEAR
30890   GFX LINE 8,60,28,20,FN CC(4,10)
30900   GFX LINE 9,60,29,20,FN CC(4,10)
30910   GFX LINE 7,60,27,20,FN CC(4,12)
30920   GFX LINE 18,40,12,63,FN CC(4,12)
30930   GFX LINE 18,40,24,63,FN CC(4,12)
30940   CALL GRABAT(209,0,0,39,63)
30950   GFX CLEAR
30960   CALL FELLO(16,14,12,12,FN CC(8,9))
30970   GFX PSET 12,10,FN CC(3,15)
30980   GFX PSET 20,16,FN CC(3,15)
30990   GFX LINE 14,26,18,31,FN CC(1,8),BF
31000   CALL GRABAT(210,0,0,31,31)
31010   ' 211: Deqq, 212: Deqq with glowing eyes (the old figure, drawn at scale 1 on the floor line 232 and grabbed)
31020   GFX CLEAR
31030   CALL DEQQFIG(300,0,0)
31040   CALL GRABAT(211,290,80,350,235)
31050   GFX CLEAR
31060   CALL DEQQFIG(300,0,1)
31070   CALL GRABAT(212,290,80,350,235)
31080   ' 213: the glorp, 214: the glorp under the bed, 215: you standing on the chair
31090   GFX CLEAR
31100   CALL DRAWGLORP(300,0,0)
31110   CALL GRABAT(213,294,200,346,234)
31120   GFX CLEAR
31130   CALL DRAWGLORP(300,0,1)
31140   CALL GRABAT(214,294,200,346,234)
31150   GFX CLEAR
31160   CALL ICON(16,60,110,1.8,0)
31170   CALL GRABAT(215,16,20,105,111)
31180 END PROC
31190 ' the sprite of an object (made when it is first needed): 48 x 48, the icon at the bottom middle
31200 FUNC V3_OBJTEX(I)
31210   IF OSPR(I)=0 THEN
31220     GFX CLEAR
31230     CALL ICON(OICON(I),24,46,1,OCOL(I))
31240     CALL GRABAT(300+I,0,0,47,47)
31250     OSPR(I)=-1
31260   END IF
31270   RETURN 300+I
31280 END FUNC
31290 ' reads the level (DATA at LEVELDATA: the same lines the other W3D demos use)
31300 PROC V3_LOADLEVEL
31310   LOCAL NS,I,S,F,C,FT,CT,L,TG,NW,J,X1,Y1,X2,Y2,T,NX,FL,UP,LO,XO,YO
31320   RESTORE LEVELDATA
31330   READ NS
31340   FOR I=1 TO NS
31350     READ S,F,C,FT,CT,L,TG,NW
31360     W3D SECTOR S,F,C,FT,CT,L
31370     W3D TAG S,TG
31380     FOR J=0 TO NW-1
31390       READ X1,Y1,X2,Y2,T,NX,FL,UP,LO,XO,YO
31400       W3D WALL S,X1,Y1,X2,Y2,T,NX
31410       IF UP>=0 OR LO>=0 THEN W3D WALLTEX S,J,-1,UP,LO
31420       IF FL<>0 THEN W3D WALLFLAG S,J,FL
31430       IF XO<>0 OR YO<>0 THEN W3D WALLOFF S,J,XO,YO
31440     NEXT J
31450   NEXT I
31460 END PROC
31470 ' the anchors of the things that lie in / on / under furniture: parent, relation (1 in, 2 on, 3 under), x, height, depth (the place's own coordinates)
31480 PROC V3_ANCHORS
31490   LOCAL K$,P,R,X,Y,Z
31500   RESTORE ANCHORDATA
31510   READ K$
31520   WHILE K$<>"END"
31530     READ R,X,Y,Z
31540     P=FN OID(K$)
31550     AKX(P,R)=X:AKY(P,R)=Y:AKZ(P,R)=Z
31560     READ K$
31570   WEND
31580 END PROC
31590 LABEL ANCHORDATA
31600 DATA "CLOSET",1,240,60,292
31610 DATA "HALLTABLE",2,75,70,290
31620 DATA "CUPBOARD",1,505,70,322
31630 DATA "CUPBOARD",2,505,165,362
31640 DATA "BOOKSHELF",2,165,120,375
31650 DATA "SOFA",3,480,0,100
31660 DATA "LTABLE",2,260,60,205
31670 DATA "PANTRY",1,30,50,312
31680 DATA "FRIDGE",1,270,65,322
31690 DATA "OVEN",1,380,20,322
31700 DATA "SINK",1,520,85,365
31710 DATA "KTABLE",2,305,60,215
31720 DATA "KCUPBOARD",1,60,40,232
31730 DATA "LINEN",1,495,30,292
31740 DATA "BED",2,440,52,355
31750 DATA "BED",3,510,2,190
31760 DATA "WARDROBE",1,90,60,312
31770 DATA "NIGHTSTAND",1,232,44,332
31780 DATA "TOYCHEST",1,28,40,84
31790 DATA "BSINK",1,120,72,365
31800 DATA "CABINET",1,35,106,250
31810 DATA "WASHER",1,185,35,322
31820 DATA "CSHELF",2,320,84,385
31830 DATA "SAFE",1,100,40,342
31840 DATA "TRUNK",1,495,20,292
31850 DATA "TOOLBOX",1,380,86,356
31860 DATA "BENCHW",2,500,60,345
31870 DATA "SHELF",2,105,86,385
31880 DATA "MAILBOX",1,515,40,142
31890 DATA "BENCH",2,500,34,200
31900 DATA "POT",3,140,2,172
31910 DATA "TREE",2,88,125,212
31920 DATA "BUCKET",1,290,52,200
31930 DATA "END"
31940 ' one thing of the picture: N = the thing number, X / Z / Y = place, height of the feet, texture, size
31950 PROC V3_PUT(N,X,Z,Y,TX,W,H)
31960   LOCAL OX,OY,FL
31970   OX=FN PLOX(ROOM):OY=FN PLOY(ROOM)
31980   W3D THING N,OX+X,OY+Z,TX,W,H
31990   FL=W3D("FLOORAT",OX+X,OY+Z)
32000   W3D THINGZ N,Y-FL
32010 END PROC
32020 ' the front of a container / appliance as a flat picture on its face: N = thing number, state decides the texture
32030 PROC V3_DECAL(N,X,Z,Y,W,H,TX)
32040   IF TX>0 THEN CALL V3_PUT(N,X,Z,Y,TX,W,H)
32050 END PROC
32060 ' the decals (front faces whose picture depends on the state) of the place R
32070 PROC V3_DECALS(R)
32080   LOCAL T
32090   IF R=R_FRONT THEN
32100     IF FRONTOPEN THEN T=190 ELSE T=130
32110     CALL V3_DECAL(220,300,296,0,60,95,T)
32120     CALL V3_DECAL(221,400,170,0,20,32,62)
32130     CALL V3_DECAL(222,60,140,0,22,54,63)
32140   END IF
32150   IF R=R_BACKYD THEN
32160     IF BACKOPEN THEN T=190 ELSE T=130
32170     CALL V3_DECAL(220,300,296,0,60,95,T)
32180     CALL V3_DECAL(221,468,168,40,110,70,84)
32190   END IF
32200   IF R=R_WELLG THEN
32210     IF SHEDOPEN THEN T=190 ELSE T=130
32220     CALL V3_DECAL(220,510,148,0,50,80,T)
32230     CALL V3_DECAL(221,88,212,0,66,100,61)
32240   END IF
32250   IF R=R_CRASH THEN CALL V3_DECAL(220,300,200,0,220,120,45)
32260   IF R=R_HALL THEN
32270     IF OOPEN(K_CLOSET) THEN T=190 ELSE T=191
32280     CALL V3_DECAL(220,240,297,8,104,134,T)
32290     CALL V3_DECAL(221,390,395,115,26,26,204)
32300   END IF
32310   IF R=R_LIVING THEN
32320     IF OOPEN(K_CUPBOARD) THEN T=190 ELSE T=191
32330     CALL V3_DECAL(220,505,327,8,134,149,T)
32340   END IF
32350   IF R=R_KITCHEN THEN
32360     IF OOPEN(K_FRIDGE) THEN T=194 ELSE T=193
32370     CALL V3_DECAL(220,270,327,10,64,107,T)
32380     IF OOPEN(K_OVEN) THEN
32390       T=190
32400     ELSE
32410       T=195
32420       IF OVENHOT THEN T=197
32430       IF OVENON THEN T=196
32440     END IF
32450     CALL V3_DECAL(221,380,327,12,72,44,T)
32460     IF OOPEN(K_PANTRY) THEN CALL V3_DECAL(222,30,317,10,48,120,190)
32470     IF OOPEN(K_KCUPBOARD) THEN CALL V3_DECAL(223,20,237,6,32,44,190)
32480     IF BACKOPEN THEN CALL V3_DECAL(224,130,392,0,80,135,190)
32490   END IF
32500   IF R=R_LANDING THEN
32510     IF OOPEN(K_LINEN) THEN CALL V3_DECAL(220,495,297,8,110,36,190)
32520     CALL V3_DECAL(221,320,390,0,40,165,208)
32530     CALL V3_DECAL(222,160,396,80,80,60,205)
32540   END IF
32550   IF R=R_BEDROOM THEN
32560     IF OOPEN(K_WARDROBE) THEN T=190 ELSE T=191
32570     CALL V3_DECAL(220,90,317,8,84,134,T)
32580     IF OOPEN(K_NIGHTSTAND) THEN T=190 ELSE T=192
32590     CALL V3_DECAL(221,232,337,28,53,22,T)
32600     IF OOPEN(K_TOYCHEST) THEN CALL V3_DECAL(222,28,87,2,55,36,190)
32610     IF OSEEN(K_BED) THEN CALL V3_DECAL(223,430,198,2,250,24,190)
32620   END IF
32630   IF R=R_BATH THEN
32640     IF MIRRORON THEN T=203 ELSE T=202
32650     CALL V3_DECAL(220,120,395,95,90,60,T)
32660   END IF
32670   IF R=R_CELLAR THEN
32680     IF WASHON THEN
32690       T=200
32700     ELSE
32710       IF OOPEN(K_WASHER) THEN T=199 ELSE T=198
32720     END IF
32730     CALL V3_DECAL(220,185,327,30,44,44,T)
32740   END IF
32750   IF R=R_ATTIC THEN
32760     IF SAFEOPEN THEN T=190 ELSE T=201
32770     CALL V3_DECAL(220,100,347,38,84,80,T)
32780     CALL V3_DECAL(221,300,395,95,100,65,206)
32790     CALL V3_DECAL(222,300,330,0,40,64,209)
32800     CALL V3_DECAL(223,380,330,50,32,32,210)
32810     IF OOPEN(K_TRUNK) THEN CALL V3_DECAL(224,495,297,8,110,22,190)
32820   END IF
32830   IF R=R_SHED THEN
32840     CALL V3_DECAL(220,310,395,70,180,80,207)
32850   END IF
32860 END PROC
32870 ' the things, Deqq and the glorp in place R
32880 PROC V3_THINGS(R)
32890   LOCAL I,P,REL,K,N,X,Z
32900   FOR I=1 TO NOBJ
32910     IF OICON(I)>0 THEN
32920       IF FN VIS(I) THEN
32930         IF OIN(I)=0 THEN
32940           IF OROOM(I)=R THEN CALL V3_PUT(I,OFX(I),OFZ(I),0,FN V3_OBJTEX(I),40,40)
32950         ELSE
32960           P=OIN(I):REL=OREL(I)
32970           N=0
32980           FOR K=1 TO NOBJ
32990             IF OIN(K)=P AND OREL(K)=REL AND OICON(K)>0 THEN
33000               IF FN VIS(K) THEN N=N+1
33010             END IF
33020           NEXT K
33030           ' my number among them
33040           K=0
33050           FOR P=1 TO I-1
33060             IF OIN(P)=OIN(I) AND OREL(P)=REL AND OICON(P)>0 THEN
33070               IF FN VIS(P) THEN K=K+1
33080             END IF
33090           NEXT P
33100           P=OIN(I)
33110           IF OROOM(P)=R THEN CALL V3_PUT(I,AKX(P,REL)+(K-(N-1)/2)*32,AKZ(P,REL),AKY(P,REL),FN V3_OBJTEX(I),40,40)
33120         END IF
33130       END IF
33140     END IF
33150   NEXT I
33160   IF R=R_CRASH THEN
33170     FOR I=1 TO 8
33180       IF OROOM(PARTK(I))=100 THEN CALL V3_PUT(230+I,MNX(I),200,MNY(I),FN V3_OBJTEX(PARTK(I)),40,40)
33190     NEXT I
33200   END IF
33210   IF NOT FN ISDARK THEN
33220     IF ONCHAIR THEN CALL V3_PUT(240,300,60,0,215,90,92)
33230     IF FN VIS(K_GLORP) THEN
33240       IF R=R_LANDING THEN CALL V3_PUT(241,250,150,0,213,53,35)
33250       IF R=R_BEDROOM THEN CALL V3_PUT(241,380,190,0,214,53,35)
33260     END IF
33270     IF DQHOME AND DQROOM=R THEN
33280       X=110:Z=290
33290       IF R=R_KITCHEN OR R=R_LIVING THEN
33300         X=330:Z=290
33310       END IF
33320       IF FN BUILT>=HALF THEN P=212 ELSE P=211
33330       CALL V3_PUT(242,X,Z,0,P,61,156)
33340     END IF
33350   END IF
33360 END PROC
33370 ' draws place R with the camera in front of it
33380 PROC V3_DRAW(R)
33390   LOCAL I
33400   FOR I=0 TO 260
33410     W3D THINGOFF I
33420   NEXT I
33430   CALL V3_DECALS(R)
33440   CALL V3_THINGS(R)
33450   GFX CLEAR
33460   W3D CAMERA FN PLOX(R)+300,FN PLOY(R)-540,90,-14,217,R*40+39
33470   W3D RENDER
33480 END PROC
33490 LABEL LEVELDATA
33500 DATA 110
33510 DATA 40,0,300,123,-1,255,1,12
33520 DATA 800,0,1400,0,29,79,0,-1,-1,0,0
33530 DATA 1400,0,1400,400,29,-1,0,-1,-1,0,0
33540 DATA 1400,400,800,400,29,-1,0,-1,-1,0,0
33550 DATA 800,400,800,0,29,-1,0,-1,-1,0,0
33560 DATA 890,300,890,350,27,41,0,-1,-1,0,0
33570 DATA 890,350,1310,350,27,41,0,-1,-1,0,0
33580 DATA 1310,350,1310,300,27,41,0,-1,-1,0,0
33590 DATA 1310,300,890,300,27,41,0,-1,-1,0,0
33600 DATA 1300,150,1300,190,101,42,0,-1,-1,0,0
33610 DATA 1300,190,1330,190,101,42,0,-1,-1,0,0
33620 DATA 1330,190,1330,150,101,42,0,-1,-1,0,0
33630 DATA 1330,150,1300,150,101,42,0,-1,-1,0,0
33640 DATA 41,200,300,27,-1,255,0,4
33650 DATA 890,300,1310,300,27,40,0,-1,-1,0,0
33660 DATA 1310,300,1310,350,27,40,0,-1,-1,0,0
33670 DATA 1310,350,890,350,27,40,0,-1,-1,0,0
33680 DATA 890,350,890,300,27,40,0,-1,-1,0,0
33690 DATA 42,62,300,101,-1,255,0,4
33700 DATA 1300,150,1330,150,101,40,0,-1,-1,0,0
33710 DATA 1330,150,1330,190,101,40,0,-1,-1,0,0
33720 DATA 1330,190,1300,190,101,40,0,-1,-1,0,0
33730 DATA 1300,190,1300,150,101,40,0,-1,-1,0,0
33740 DATA 79,0,400,123,-1,255,99,4
33750 DATA 800,-560,1400,-560,29,-1,0,-1,-1,0,0
33760 DATA 1400,-560,1400,0,29,-1,0,-1,-1,0,0
33770 DATA 1400,0,800,0,29,40,0,-1,-1,0,0
33780 DATA 800,0,800,-560,29,-1,0,-1,-1,0,0
33790 DATA 80,0,300,122,-1,255,2,20
33800 DATA 1600,0,2200,0,29,119,0,-1,-1,0,0
33810 DATA 2200,0,2200,400,29,-1,0,-1,-1,0,0
33820 DATA 2200,400,1600,400,29,-1,0,-1,-1,0,0
33830 DATA 1600,400,1600,0,29,-1,0,-1,-1,0,0
33840 DATA 1690,300,1690,350,27,81,0,-1,-1,0,0
33850 DATA 1690,350,2110,350,27,81,0,-1,-1,0,0
33860 DATA 2110,350,2110,300,27,81,0,-1,-1,0,0
33870 DATA 2110,300,1690,300,27,81,0,-1,-1,0,0
33880 DATA 1820,230,1820,290,100,82,0,-1,-1,0,0
33890 DATA 1820,290,1980,290,100,82,0,-1,-1,0,0
33900 DATA 1980,290,1980,230,100,82,0,-1,-1,0,0
33910 DATA 1980,230,1820,230,100,82,0,-1,-1,0,0
33920 DATA 1670,170,1670,230,112,83,0,-1,-1,0,0
33930 DATA 1670,230,1730,230,112,83,0,-1,-1,0,0
33940 DATA 1730,230,1730,170,112,83,0,-1,-1,0,0
33950 DATA 1730,170,1670,170,112,83,0,-1,-1,0,0
33960 DATA 2040,170,2040,230,112,84,0,-1,-1,0,0
33970 DATA 2040,230,2160,230,112,84,0,-1,-1,0,0
33980 DATA 2160,230,2160,170,112,84,0,-1,-1,0,0
33990 DATA 2160,170,2040,170,112,84,0,-1,-1,0,0
34000 DATA 81,200,300,27,-1,255,0,4
34010 DATA 1690,300,2110,300,27,80,0,-1,-1,0,0
34020 DATA 2110,300,2110,350,27,80,0,-1,-1,0,0
34030 DATA 2110,350,1690,350,27,80,0,-1,-1,0,0
34040 DATA 1690,350,1690,300,27,80,0,-1,-1,0,0
34050 DATA 82,5,300,100,-1,255,0,4
34060 DATA 1820,230,1980,230,100,80,0,-1,-1,0,0
34070 DATA 1980,230,1980,290,100,80,0,-1,-1,0,0
34080 DATA 1980,290,1820,290,100,80,0,-1,-1,0,0
34090 DATA 1820,290,1820,230,100,80,0,-1,-1,0,0
34100 DATA 83,62,300,112,-1,255,0,4
34110 DATA 1670,170,1730,170,112,80,0,-1,-1,0,0
34120 DATA 1730,170,1730,230,112,80,0,-1,-1,0,0
34130 DATA 1730,230,1670,230,112,80,0,-1,-1,0,0
34140 DATA 1670,230,1670,170,112,80,0,-1,-1,0,0
34150 DATA 84,34,300,112,-1,255,0,4
34160 DATA 2040,170,2160,170,112,80,0,-1,-1,0,0
34170 DATA 2160,170,2160,230,112,80,0,-1,-1,0,0
34180 DATA 2160,230,2040,230,112,80,0,-1,-1,0,0
34190 DATA 2040,230,2040,170,112,80,0,-1,-1,0,0
34200 DATA 119,0,400,122,-1,255,99,4
34210 DATA 1600,-560,2200,-560,29,-1,0,-1,-1,0,0
34220 DATA 2200,-560,2200,0,29,-1,0,-1,-1,0,0
34230 DATA 2200,0,1600,0,29,80,0,-1,-1,0,0
34240 DATA 1600,0,1600,-560,29,-1,0,-1,-1,0,0
34250 DATA 120,0,300,122,-1,255,3,20
34260 DATA 2400,0,3000,0,29,159,0,-1,-1,0,0
34270 DATA 3000,0,3000,400,29,-1,0,-1,-1,0,0
34280 DATA 3000,400,2400,400,29,-1,0,-1,-1,0,0
34290 DATA 2400,400,2400,0,29,-1,0,-1,-1,0,0
34300 DATA 2490,300,2490,350,27,121,0,-1,-1,0,0
34310 DATA 2490,350,2910,350,27,121,0,-1,-1,0,0
34320 DATA 2910,350,2910,300,27,121,0,-1,-1,0,0
34330 DATA 2910,300,2490,300,27,121,0,-1,-1,0,0
34340 DATA 2510,180,2510,240,102,122,0,-1,-1,0,0
34350 DATA 2510,240,2570,240,102,122,0,-1,-1,0,0
34360 DATA 2570,240,2570,180,102,122,0,-1,-1,0,0
34370 DATA 2570,180,2510,180,102,122,0,-1,-1,0,0
34380 DATA 2780,170,2780,176,112,123,0,-1,-1,0,0
34390 DATA 2780,176,2786,176,112,123,0,-1,-1,0,0
34400 DATA 2786,176,2786,170,112,123,0,-1,-1,0,0
34410 DATA 2786,170,2780,170,112,123,0,-1,-1,0,0
34420 DATA 2950,170,2950,176,112,124,0,-1,-1,0,0
34430 DATA 2950,176,2956,176,112,124,0,-1,-1,0,0
34440 DATA 2956,176,2956,170,112,124,0,-1,-1,0,0
34450 DATA 2956,170,2950,170,112,124,0,-1,-1,0,0
34460 DATA 121,200,300,27,-1,255,0,4
34470 DATA 2490,300,2910,300,27,120,0,-1,-1,0,0
34480 DATA 2910,300,2910,350,27,120,0,-1,-1,0,0
34490 DATA 2910,350,2490,350,27,120,0,-1,-1,0,0
34500 DATA 2490,350,2490,300,27,120,0,-1,-1,0,0
34510 DATA 122,48,300,102,-1,255,0,4
34520 DATA 2510,180,2570,180,102,120,0,-1,-1,0,0
34530 DATA 2570,180,2570,240,102,120,0,-1,-1,0,0
34540 DATA 2570,240,2510,240,102,120,0,-1,-1,0,0
34550 DATA 2510,240,2510,180,102,120,0,-1,-1,0,0
34560 DATA 123,100,300,112,-1,255,0,4
34570 DATA 2780,170,2786,170,112,120,0,-1,-1,0,0
34580 DATA 2786,170,2786,176,112,120,0,-1,-1,0,0
34590 DATA 2786,176,2780,176,112,120,0,-1,-1,0,0
34600 DATA 2780,176,2780,170,112,120,0,-1,-1,0,0
34610 DATA 124,100,300,112,-1,255,0,4
34620 DATA 2950,170,2956,170,112,120,0,-1,-1,0,0
34630 DATA 2956,170,2956,176,112,120,0,-1,-1,0,0
34640 DATA 2956,176,2950,176,112,120,0,-1,-1,0,0
34650 DATA 2950,176,2950,170,112,120,0,-1,-1,0,0
34660 DATA 159,0,400,122,-1,255,99,4
34670 DATA 2400,-560,3000,-560,29,-1,0,-1,-1,0,0
34680 DATA 3000,-560,3000,0,29,-1,0,-1,-1,0,0
34690 DATA 3000,0,2400,0,29,120,0,-1,-1,0,0
34700 DATA 2400,0,2400,-560,29,-1,0,-1,-1,0,0
34710 DATA 160,0,300,122,-1,255,4,24
34720 DATA 3200,0,3800,0,29,199,0,-1,-1,0,0
34730 DATA 3800,0,3800,400,29,-1,0,-1,-1,0,0
34740 DATA 3800,400,3200,400,29,-1,0,-1,-1,0,0
34750 DATA 3200,400,3200,0,29,-1,0,-1,-1,0,0
34760 DATA 3290,300,3290,350,27,161,0,-1,-1,0,0
34770 DATA 3290,350,3710,350,27,161,0,-1,-1,0,0
34780 DATA 3710,350,3710,300,27,161,0,-1,-1,0,0
34790 DATA 3710,300,3290,300,27,161,0,-1,-1,0,0
34800 DATA 3450,170,3450,240,100,162,0,-1,-1,0,0
34810 DATA 3450,240,3530,240,100,162,0,-1,-1,0,0
34820 DATA 3530,240,3530,170,100,162,0,-1,-1,0,0
34830 DATA 3530,170,3450,170,100,162,0,-1,-1,0,0
34840 DATA 3438,195,3438,205,112,163,0,-1,-1,0,0
34850 DATA 3438,205,3446,205,112,163,0,-1,-1,0,0
34860 DATA 3446,205,3446,195,112,163,0,-1,-1,0,0
34870 DATA 3446,195,3438,195,112,163,0,-1,-1,0,0
34880 DATA 3534,195,3534,205,112,164,0,-1,-1,0,0
34890 DATA 3534,205,3542,205,112,164,0,-1,-1,0,0
34900 DATA 3542,205,3542,195,112,164,0,-1,-1,0,0
34910 DATA 3542,195,3534,195,112,164,0,-1,-1,0,0
34920 DATA 3630,150,3630,270,112,165,0,-1,-1,0,0
34930 DATA 3630,270,3790,270,112,165,0,-1,-1,0,0
34940 DATA 3790,270,3790,150,112,165,0,-1,-1,0,0
34950 DATA 3790,150,3630,150,112,165,0,-1,-1,0,0
34960 DATA 161,200,300,27,-1,255,0,4
34970 DATA 3290,300,3710,300,27,160,0,-1,-1,0,0
34980 DATA 3710,300,3710,350,27,160,0,-1,-1,0,0
34990 DATA 3710,350,3290,350,27,160,0,-1,-1,0,0
35000 DATA 3290,350,3290,300,27,160,0,-1,-1,0,0
35010 DATA 162,38,300,100,-1,255,0,4
35020 DATA 3450,170,3530,170,100,160,0,-1,-1,0,0
35030 DATA 3530,170,3530,240,100,160,0,-1,-1,0,0
35040 DATA 3530,240,3450,240,100,160,0,-1,-1,0,0
35050 DATA 3450,240,3450,170,100,160,0,-1,-1,0,0
35060 DATA 163,110,300,112,-1,255,0,4
35070 DATA 3438,195,3446,195,112,160,0,-1,-1,0,0
35080 DATA 3446,195,3446,205,112,160,0,-1,-1,0,0
35090 DATA 3446,205,3438,205,112,160,0,-1,-1,0,0
35100 DATA 3438,205,3438,195,112,160,0,-1,-1,0,0
35110 DATA 164,110,300,112,-1,255,0,4
35120 DATA 3534,195,3542,195,112,160,0,-1,-1,0,0
35130 DATA 3542,195,3542,205,112,160,0,-1,-1,0,0
35140 DATA 3542,205,3534,205,112,160,0,-1,-1,0,0
35150 DATA 3534,205,3534,195,112,160,0,-1,-1,0,0
35160 DATA 165,100,300,112,-1,255,0,4
35170 DATA 3630,150,3790,150,112,160,0,-1,-1,0,0
35180 DATA 3790,150,3790,270,112,160,0,-1,-1,0,0
35190 DATA 3790,270,3630,270,112,160,0,-1,-1,0,0
35200 DATA 3630,270,3630,150,112,160,0,-1,-1,0,0
35210 DATA 199,0,400,122,-1,255,99,4
35220 DATA 3200,-560,3800,-560,29,-1,0,-1,-1,0,0
35230 DATA 3800,-560,3800,0,29,-1,0,-1,-1,0,0
35240 DATA 3800,0,3200,0,29,160,0,-1,-1,0,0
35250 DATA 3200,0,3200,-560,29,-1,0,-1,-1,0,0
35260 DATA 200,0,170,121,-1,255,5,20
35270 DATA 0,700,600,700,112,239,0,-1,-1,0,0
35280 DATA 600,700,600,1100,112,-1,0,-1,-1,0,0
35290 DATA 600,1100,0,1100,112,-1,0,-1,-1,0,0
35300 DATA 0,1100,0,700,112,-1,0,-1,-1,0,0
35310 DATA 430,1000,430,1090,112,201,0,-1,-1,0,0
35320 DATA 430,1090,560,1090,112,201,0,-1,-1,0,0
35330 DATA 560,1090,560,1000,112,201,0,-1,-1,0,0
35340 DATA 560,1000,430,1000,112,201,0,-1,-1,0,0
35350 DATA 340,1030,340,1080,101,202,0,-1,-1,0,0
35360 DATA 340,1080,420,1080,101,202,0,-1,-1,0,0
35370 DATA 420,1080,420,1030,101,202,0,-1,-1,0,0
35380 DATA 420,1030,340,1030,101,202,0,-1,-1,0,0
35390 DATA 40,1070,40,1096,112,203,0,-1,-1,0,0
35400 DATA 40,1096,170,1096,112,203,0,-1,-1,0,0
35410 DATA 170,1096,170,1070,112,203,0,-1,-1,0,0
35420 DATA 170,1070,40,1070,112,203,0,-1,-1,0,0
35430 DATA 2,840,2,940,130,204,0,-1,-1,0,0
35440 DATA 2,940,6,940,130,204,0,-1,-1,0,0
35450 DATA 6,940,6,840,130,204,0,-1,-1,0,0
35460 DATA 6,840,2,840,130,204,0,-1,-1,0,0
35470 DATA 201,60,170,112,-1,255,0,4
35480 DATA 430,1000,560,1000,112,200,0,-1,-1,0,0
35490 DATA 560,1000,560,1090,112,200,0,-1,-1,0,0
35500 DATA 560,1090,430,1090,112,200,0,-1,-1,0,0
35510 DATA 430,1090,430,1000,112,200,0,-1,-1,0,0
35520 DATA 202,85,170,101,-1,255,0,4
35530 DATA 340,1030,420,1030,101,200,0,-1,-1,0,0
35540 DATA 420,1030,420,1080,101,200,0,-1,-1,0,0
35550 DATA 420,1080,340,1080,101,200,0,-1,-1,0,0
35560 DATA 340,1080,340,1030,101,200,0,-1,-1,0,0
35570 DATA 203,86,170,112,-1,255,0,4
35580 DATA 40,1070,170,1070,112,200,0,-1,-1,0,0
35590 DATA 170,1070,170,1096,112,200,0,-1,-1,0,0
35600 DATA 170,1096,40,1096,112,200,0,-1,-1,0,0
35610 DATA 40,1096,40,1070,112,200,0,-1,-1,0,0
35620 DATA 204,135,170,130,-1,255,0,4
35630 DATA 2,840,6,840,130,200,0,-1,-1,0,0
35640 DATA 6,840,6,940,130,200,0,-1,-1,0,0
35650 DATA 6,940,2,940,130,200,0,-1,-1,0,0
35660 DATA 2,940,2,840,130,200,0,-1,-1,0,0
35670 DATA 239,0,400,121,-1,255,99,4
35680 DATA 0,140,600,140,112,-1,0,-1,-1,0,0
35690 DATA 600,140,600,700,112,-1,0,-1,-1,0,0
35700 DATA 600,700,0,700,112,200,0,-1,-1,0,0
35710 DATA 0,700,0,140,112,-1,0,-1,-1,0,0
35720 DATA 240,0,300,121,-1,255,6,8
35730 DATA 800,700,1400,700,29,279,0,-1,-1,0,0
35740 DATA 1400,700,1400,1100,29,-1,0,-1,-1,0,0
35750 DATA 1400,1100,800,1100,29,-1,0,-1,-1,0,0
35760 DATA 800,1100,800,700,29,-1,0,-1,-1,0,0
35770 DATA 1050,850,1050,930,100,241,0,-1,-1,0,0
35780 DATA 1050,930,1150,930,100,241,0,-1,-1,0,0
35790 DATA 1150,930,1150,850,100,241,0,-1,-1,0,0
35800 DATA 1150,850,1050,850,100,241,0,-1,-1,0,0
35810 DATA 241,12,300,100,-1,255,0,4
35820 DATA 1050,850,1150,850,100,240,0,-1,-1,0,0
35830 DATA 1150,850,1150,930,100,240,0,-1,-1,0,0
35840 DATA 1150,930,1050,930,100,240,0,-1,-1,0,0
35850 DATA 1050,930,1050,850,100,240,0,-1,-1,0,0
35860 DATA 279,0,400,121,-1,255,99,4
35870 DATA 800,140,1400,140,29,-1,0,-1,-1,0,0
35880 DATA 1400,140,1400,700,29,-1,0,-1,-1,0,0
35890 DATA 1400,700,800,700,29,240,0,-1,-1,0,0
35900 DATA 800,700,800,140,29,-1,0,-1,-1,0,0
35910 DATA 280,0,170,118,-1,255,7,48
35920 DATA 1600,700,2200,700,113,319,0,-1,-1,0,0
35930 DATA 2200,700,2200,1100,113,-1,0,-1,-1,0,0
35940 DATA 2200,1100,1600,1100,113,-1,0,-1,-1,0,0
35950 DATA 1600,1100,1600,700,113,-1,0,-1,-1,0,0
35960 DATA 2100,770,2100,808,112,281,0,-1,-1,0,0
35970 DATA 2100,808,2198,808,112,281,0,-1,-1,0,0
35980 DATA 2198,808,2198,770,112,281,0,-1,-1,0,0
35990 DATA 2198,770,2100,770,112,281,0,-1,-1,0,0
36000 DATA 2100,810,2100,848,112,282,0,-1,-1,0,0
36010 DATA 2100,848,2198,848,112,282,0,-1,-1,0,0
36020 DATA 2198,848,2198,810,112,282,0,-1,-1,0,0
36030 DATA 2198,810,2100,810,112,282,0,-1,-1,0,0
36040 DATA 2100,850,2100,888,112,283,0,-1,-1,0,0
36050 DATA 2100,888,2198,888,112,283,0,-1,-1,0,0
36060 DATA 2198,888,2198,850,112,283,0,-1,-1,0,0
36070 DATA 2198,850,2100,850,112,283,0,-1,-1,0,0
36080 DATA 2100,890,2100,928,112,284,0,-1,-1,0,0
36090 DATA 2100,928,2198,928,112,284,0,-1,-1,0,0
36100 DATA 2198,928,2198,890,112,284,0,-1,-1,0,0
36110 DATA 2198,890,2100,890,112,284,0,-1,-1,0,0
36120 DATA 2100,930,2100,968,112,285,0,-1,-1,0,0
36130 DATA 2100,968,2198,968,112,285,0,-1,-1,0,0
36140 DATA 2198,968,2198,930,112,285,0,-1,-1,0,0
36150 DATA 2198,930,2100,930,112,285,0,-1,-1,0,0
36160 DATA 2100,970,2100,1008,112,286,0,-1,-1,0,0
36170 DATA 2100,1008,2198,1008,112,286,0,-1,-1,0,0
36180 DATA 2198,1008,2198,970,112,286,0,-1,-1,0,0
36190 DATA 2198,970,2100,970,112,286,0,-1,-1,0,0
36200 DATA 2100,1010,2100,1048,112,287,0,-1,-1,0,0
36210 DATA 2100,1048,2198,1048,112,287,0,-1,-1,0,0
36220 DATA 2198,1048,2198,1010,112,287,0,-1,-1,0,0
36230 DATA 2198,1010,2100,1010,112,287,0,-1,-1,0,0
36240 DATA 1780,1000,1780,1096,112,288,0,-1,-1,0,0
36250 DATA 1780,1096,1900,1096,112,288,0,-1,-1,0,0
36260 DATA 1900,1096,1900,1000,112,288,0,-1,-1,0,0
36270 DATA 1900,1000,1780,1000,112,288,0,-1,-1,0,0
36280 DATA 1630,950,1630,1030,112,289,0,-1,-1,0,0
36290 DATA 1630,1030,1720,1030,112,289,0,-1,-1,0,0
36300 DATA 1720,1030,1720,950,112,289,0,-1,-1,0,0
36310 DATA 1720,950,1630,950,112,289,0,-1,-1,0,0
36320 DATA 2070,1094,2070,1098,130,290,0,-1,-1,0,0
36330 DATA 2070,1098,2160,1098,130,290,0,-1,-1,0,0
36340 DATA 2160,1098,2160,1094,130,290,0,-1,-1,0,0
36350 DATA 2160,1094,2070,1094,130,290,0,-1,-1,0,0
36360 DATA 1602,840,1602,930,130,291,0,-1,-1,0,0
36370 DATA 1602,930,1606,930,130,291,0,-1,-1,0,0
36380 DATA 1606,930,1606,840,130,291,0,-1,-1,0,0
36390 DATA 1606,840,1602,840,130,291,0,-1,-1,0,0
36400 DATA 281,14,170,112,-1,255,0,4
36410 DATA 2100,770,2198,770,112,280,0,-1,-1,0,0
36420 DATA 2198,770,2198,808,112,280,0,-1,-1,0,0
36430 DATA 2198,808,2100,808,112,280,0,-1,-1,0,0
36440 DATA 2100,808,2100,770,112,280,0,-1,-1,0,0
36450 DATA 282,28,170,112,-1,255,0,4
36460 DATA 2100,810,2198,810,112,280,0,-1,-1,0,0
36470 DATA 2198,810,2198,848,112,280,0,-1,-1,0,0
36480 DATA 2198,848,2100,848,112,280,0,-1,-1,0,0
36490 DATA 2100,848,2100,810,112,280,0,-1,-1,0,0
36500 DATA 283,42,170,112,-1,255,0,4
36510 DATA 2100,850,2198,850,112,280,0,-1,-1,0,0
36520 DATA 2198,850,2198,888,112,280,0,-1,-1,0,0
36530 DATA 2198,888,2100,888,112,280,0,-1,-1,0,0
36540 DATA 2100,888,2100,850,112,280,0,-1,-1,0,0
36550 DATA 284,56,170,112,-1,255,0,4
36560 DATA 2100,890,2198,890,112,280,0,-1,-1,0,0
36570 DATA 2198,890,2198,928,112,280,0,-1,-1,0,0
36580 DATA 2198,928,2100,928,112,280,0,-1,-1,0,0
36590 DATA 2100,928,2100,890,112,280,0,-1,-1,0,0
36600 DATA 285,70,170,112,-1,255,0,4
36610 DATA 2100,930,2198,930,112,280,0,-1,-1,0,0
36620 DATA 2198,930,2198,968,112,280,0,-1,-1,0,0
36630 DATA 2198,968,2100,968,112,280,0,-1,-1,0,0
36640 DATA 2100,968,2100,930,112,280,0,-1,-1,0,0
36650 DATA 286,84,170,112,-1,255,0,4
36660 DATA 2100,970,2198,970,112,280,0,-1,-1,0,0
36670 DATA 2198,970,2198,1008,112,280,0,-1,-1,0,0
36680 DATA 2198,1008,2100,1008,112,280,0,-1,-1,0,0
36690 DATA 2100,1008,2100,970,112,280,0,-1,-1,0,0
36700 DATA 287,98,170,112,-1,255,0,4
36710 DATA 2100,1010,2198,1010,112,280,0,-1,-1,0,0
36720 DATA 2198,1010,2198,1048,112,280,0,-1,-1,0,0
36730 DATA 2198,1048,2100,1048,112,280,0,-1,-1,0,0
36740 DATA 2100,1048,2100,1010,112,280,0,-1,-1,0,0
36750 DATA 288,150,170,112,-1,255,0,4
36760 DATA 1780,1000,1900,1000,112,280,0,-1,-1,0,0
36770 DATA 1900,1000,1900,1096,112,280,0,-1,-1,0,0
36780 DATA 1900,1096,1780,1096,112,280,0,-1,-1,0,0
36790 DATA 1780,1096,1780,1000,112,280,0,-1,-1,0,0
36800 DATA 289,70,170,112,-1,255,0,4
36810 DATA 1630,950,1720,950,112,280,0,-1,-1,0,0
36820 DATA 1720,950,1720,1030,112,280,0,-1,-1,0,0
36830 DATA 1720,1030,1630,1030,112,280,0,-1,-1,0,0
36840 DATA 1630,1030,1630,950,112,280,0,-1,-1,0,0
36850 DATA 290,135,170,130,-1,255,0,4
36860 DATA 2070,1094,2160,1094,130,280,0,-1,-1,0,0
36870 DATA 2160,1094,2160,1098,130,280,0,-1,-1,0,0
36880 DATA 2160,1098,2070,1098,130,280,0,-1,-1,0,0
36890 DATA 2070,1098,2070,1094,130,280,0,-1,-1,0,0
36900 DATA 291,135,170,130,-1,255,0,4
36910 DATA 1602,840,1606,840,130,280,0,-1,-1,0,0
36920 DATA 1606,840,1606,930,130,280,0,-1,-1,0,0
36930 DATA 1606,930,1602,930,130,280,0,-1,-1,0,0
36940 DATA 1602,930,1602,840,130,280,0,-1,-1,0,0
36950 DATA 319,0,400,118,-1,255,99,4
36960 DATA 1600,140,2200,140,113,-1,0,-1,-1,0,0
36970 DATA 2200,140,2200,700,113,-1,0,-1,-1,0,0
36980 DATA 2200,700,1600,700,113,280,0,-1,-1,0,0
36990 DATA 1600,700,1600,140,113,-1,0,-1,-1,0,0
37000 DATA 320,0,170,118,-1,255,8,36
37010 DATA 2400,700,3000,700,106,359,0,-1,-1,0,0
37020 DATA 3000,700,3000,1100,106,-1,0,-1,-1,0,0
37030 DATA 3000,1100,2400,1100,106,-1,0,-1,-1,0,0
37040 DATA 2400,1100,2400,700,106,-1,0,-1,-1,0,0
37050 DATA 2480,1050,2480,1096,112,321,0,-1,-1,0,0
37060 DATA 2480,1096,2650,1096,112,321,0,-1,-1,0,0
37070 DATA 2650,1096,2650,1050,112,321,0,-1,-1,0,0
37080 DATA 2650,1050,2480,1050,112,321,0,-1,-1,0,0
37090 DATA 2830,1030,2830,1096,112,322,0,-1,-1,0,0
37100 DATA 2830,1096,2980,1096,112,322,0,-1,-1,0,0
37110 DATA 2980,1096,2980,1030,112,322,0,-1,-1,0,0
37120 DATA 2980,1030,2830,1030,112,322,0,-1,-1,0,0
37130 DATA 2955,790,2955,950,110,323,0,-1,-1,0,0
37140 DATA 2955,950,2998,950,110,323,0,-1,-1,0,0
37150 DATA 2998,950,2998,790,110,323,0,-1,-1,0,0
37160 DATA 2998,790,2955,790,110,323,0,-1,-1,0,0
37170 DATA 2900,790,2900,950,110,324,0,-1,-1,0,0
37180 DATA 2900,950,2953,950,110,324,0,-1,-1,0,0
37190 DATA 2953,950,2953,790,110,324,0,-1,-1,0,0
37200 DATA 2953,790,2900,790,110,324,0,-1,-1,0,0
37210 DATA 2402,800,2402,900,100,325,0,-1,-1,0,0
37220 DATA 2402,900,2445,900,100,325,0,-1,-1,0,0
37230 DATA 2445,900,2445,800,100,325,0,-1,-1,0,0
37240 DATA 2445,800,2402,800,100,325,0,-1,-1,0,0
37250 DATA 2590,850,2590,960,112,326,0,-1,-1,0,0
37260 DATA 2590,960,2730,960,112,326,0,-1,-1,0,0
37270 DATA 2730,960,2730,850,112,326,0,-1,-1,0,0
37280 DATA 2730,850,2590,850,112,326,0,-1,-1,0,0
37290 DATA 2580,750,2580,830,119,327,0,-1,-1,0,0
37300 DATA 2580,830,2820,830,119,327,0,-1,-1,0,0
37310 DATA 2820,830,2820,750,119,327,0,-1,-1,0,0
37320 DATA 2820,750,2580,750,119,327,0,-1,-1,0,0
37330 DATA 2402,1000,2402,1080,130,328,0,-1,-1,0,0
37340 DATA 2402,1080,2406,1080,130,328,0,-1,-1,0,0
37350 DATA 2406,1080,2406,1000,130,328,0,-1,-1,0,0
37360 DATA 2406,1000,2402,1000,130,328,0,-1,-1,0,0
37370 DATA 321,120,170,112,-1,255,0,4
37380 DATA 2480,1050,2650,1050,112,320,0,-1,-1,0,0
37390 DATA 2650,1050,2650,1096,112,320,0,-1,-1,0,0
37400 DATA 2650,1096,2480,1096,112,320,0,-1,-1,0,0
37410 DATA 2480,1096,2480,1050,112,320,0,-1,-1,0,0
37420 DATA 322,165,170,112,-1,255,0,4
37430 DATA 2830,1030,2980,1030,112,320,0,-1,-1,0,0
37440 DATA 2980,1030,2980,1096,112,320,0,-1,-1,0,0
37450 DATA 2980,1096,2830,1096,112,320,0,-1,-1,0,0
37460 DATA 2830,1096,2830,1030,112,320,0,-1,-1,0,0
37470 DATA 323,80,170,110,-1,255,0,4
37480 DATA 2955,790,2998,790,110,320,0,-1,-1,0,0
37490 DATA 2998,790,2998,950,110,320,0,-1,-1,0,0
37500 DATA 2998,950,2955,950,110,320,0,-1,-1,0,0
37510 DATA 2955,950,2955,790,110,320,0,-1,-1,0,0
37520 DATA 324,38,170,110,-1,255,0,4
37530 DATA 2900,790,2953,790,110,320,0,-1,-1,0,0
37540 DATA 2953,790,2953,950,110,320,0,-1,-1,0,0
37550 DATA 2953,950,2900,950,110,320,0,-1,-1,0,0
37560 DATA 2900,950,2900,790,110,320,0,-1,-1,0,0
37570 DATA 325,110,170,100,-1,255,0,4
37580 DATA 2402,800,2445,800,100,320,0,-1,-1,0,0
37590 DATA 2445,800,2445,900,100,320,0,-1,-1,0,0
37600 DATA 2445,900,2402,900,100,320,0,-1,-1,0,0
37610 DATA 2402,900,2402,800,100,320,0,-1,-1,0,0
37620 DATA 326,60,170,112,-1,255,0,4
37630 DATA 2590,850,2730,850,112,320,0,-1,-1,0,0
37640 DATA 2730,850,2730,960,112,320,0,-1,-1,0,0
37650 DATA 2730,960,2590,960,112,320,0,-1,-1,0,0
37660 DATA 2590,960,2590,850,112,320,0,-1,-1,0,0
37670 DATA 327,1,170,119,-1,255,0,4
37680 DATA 2580,750,2820,750,119,320,0,-1,-1,0,0
37690 DATA 2820,750,2820,830,119,320,0,-1,-1,0,0
37700 DATA 2820,830,2580,830,119,320,0,-1,-1,0,0
37710 DATA 2580,830,2580,750,119,320,0,-1,-1,0,0
37720 DATA 328,135,170,130,-1,255,0,4
37730 DATA 2402,1000,2406,1000,130,320,0,-1,-1,0,0
37740 DATA 2406,1000,2406,1080,130,320,0,-1,-1,0,0
37750 DATA 2406,1080,2402,1080,130,320,0,-1,-1,0,0
37760 DATA 2402,1080,2402,1000,130,320,0,-1,-1,0,0
37770 DATA 359,0,400,118,-1,255,99,4
37780 DATA 2400,140,3000,140,106,-1,0,-1,-1,0,0
37790 DATA 3000,140,3000,700,106,-1,0,-1,-1,0,0
37800 DATA 3000,700,2400,700,106,320,0,-1,-1,0,0
37810 DATA 2400,700,2400,140,106,-1,0,-1,-1,0,0
37820 DATA 360,0,170,120,-1,255,9,40
37830 DATA 3200,700,3800,700,103,399,0,-1,-1,0,0
37840 DATA 3800,700,3800,1100,103,-1,0,-1,-1,0,0
37850 DATA 3800,1100,3200,1100,103,-1,0,-1,-1,0,0
37860 DATA 3200,1100,3200,700,103,-1,0,-1,-1,0,0
37870 DATA 3202,1020,3202,1096,112,361,0,-1,-1,0,0
37880 DATA 3202,1096,3260,1096,112,361,0,-1,-1,0,0
37890 DATA 3260,1096,3260,1020,112,361,0,-1,-1,0,0
37900 DATA 3260,1020,3202,1020,112,361,0,-1,-1,0,0
37910 DATA 3430,1030,3430,1096,100,362,0,-1,-1,0,0
37920 DATA 3430,1096,3510,1096,100,362,0,-1,-1,0,0
37930 DATA 3510,1096,3510,1030,100,362,0,-1,-1,0,0
37940 DATA 3510,1030,3430,1030,100,362,0,-1,-1,0,0
37950 DATA 3530,1030,3530,1096,100,363,0,-1,-1,0,0
37960 DATA 3530,1096,3630,1096,100,363,0,-1,-1,0,0
37970 DATA 3630,1096,3630,1030,100,363,0,-1,-1,0,0
37980 DATA 3630,1030,3530,1030,100,363,0,-1,-1,0,0
37990 DATA 3650,1030,3650,1096,100,364,0,-1,-1,0,0
38000 DATA 3650,1096,3790,1096,100,364,0,-1,-1,0,0
38010 DATA 3790,1096,3790,1030,100,364,0,-1,-1,0,0
38020 DATA 3790,1030,3650,1030,100,364,0,-1,-1,0,0
38030 DATA 3430,870,3430,960,112,365,0,-1,-1,0,0
38040 DATA 3430,960,3580,960,112,365,0,-1,-1,0,0
38050 DATA 3580,960,3580,870,112,365,0,-1,-1,0,0
38060 DATA 3580,870,3430,870,112,365,0,-1,-1,0,0
38070 DATA 3202,940,3202,1010,112,366,0,-1,-1,0,0
38080 DATA 3202,1010,3240,1010,112,366,0,-1,-1,0,0
38090 DATA 3240,1010,3240,940,112,366,0,-1,-1,0,0
38100 DATA 3240,940,3202,940,112,366,0,-1,-1,0,0
38110 DATA 3794,850,3794,930,130,367,0,-1,-1,0,0
38120 DATA 3794,930,3798,930,130,367,0,-1,-1,0,0
38130 DATA 3798,930,3798,850,130,367,0,-1,-1,0,0
38140 DATA 3798,850,3794,850,130,367,0,-1,-1,0,0
38150 DATA 3290,1094,3290,1098,130,368,0,-1,-1,0,0
38160 DATA 3290,1098,3370,1098,130,368,0,-1,-1,0,0
38170 DATA 3370,1098,3370,1094,130,368,0,-1,-1,0,0
38180 DATA 3370,1094,3290,1094,130,368,0,-1,-1,0,0
38190 DATA 3240,810,3240,930,121,369,0,-1,-1,0,0
38200 DATA 3240,930,3350,930,121,369,0,-1,-1,0,0
38210 DATA 3350,930,3350,810,121,369,0,-1,-1,0,0
38220 DATA 3350,810,3240,810,121,369,0,-1,-1,0,0
38230 DATA 361,140,170,112,-1,255,0,4
38240 DATA 3202,1020,3260,1020,112,360,0,-1,-1,0,0
38250 DATA 3260,1020,3260,1096,112,360,0,-1,-1,0,0
38260 DATA 3260,1096,3202,1096,112,360,0,-1,-1,0,0
38270 DATA 3202,1096,3202,1020,112,360,0,-1,-1,0,0
38280 DATA 362,125,170,100,-1,255,0,4
38290 DATA 3430,1030,3510,1030,100,360,0,-1,-1,0,0
38300 DATA 3510,1030,3510,1096,100,360,0,-1,-1,0,0
38310 DATA 3510,1096,3430,1096,100,360,0,-1,-1,0,0
38320 DATA 3430,1096,3430,1030,100,360,0,-1,-1,0,0
38330 DATA 363,85,170,100,-1,255,0,4
38340 DATA 3530,1030,3630,1030,100,360,0,-1,-1,0,0
38350 DATA 3630,1030,3630,1096,100,360,0,-1,-1,0,0
38360 DATA 3630,1096,3530,1096,100,360,0,-1,-1,0,0
38370 DATA 3530,1096,3530,1030,100,360,0,-1,-1,0,0
38380 DATA 364,85,170,100,-1,255,0,4
38390 DATA 3650,1030,3790,1030,100,360,0,-1,-1,0,0
38400 DATA 3790,1030,3790,1096,100,360,0,-1,-1,0,0
38410 DATA 3790,1096,3650,1096,100,360,0,-1,-1,0,0
38420 DATA 3650,1096,3650,1030,100,360,0,-1,-1,0,0
38430 DATA 365,60,170,112,-1,255,0,4
38440 DATA 3430,870,3580,870,112,360,0,-1,-1,0,0
38450 DATA 3580,870,3580,960,112,360,0,-1,-1,0,0
38460 DATA 3580,960,3430,960,112,360,0,-1,-1,0,0
38470 DATA 3430,960,3430,870,112,360,0,-1,-1,0,0
38480 DATA 366,55,170,112,-1,255,0,4
38490 DATA 3202,940,3240,940,112,360,0,-1,-1,0,0
38500 DATA 3240,940,3240,1010,112,360,0,-1,-1,0,0
38510 DATA 3240,1010,3202,1010,112,360,0,-1,-1,0,0
38520 DATA 3202,1010,3202,940,112,360,0,-1,-1,0,0
38530 DATA 367,135,170,130,-1,255,0,4
38540 DATA 3794,850,3798,850,130,360,0,-1,-1,0,0
38550 DATA 3798,850,3798,930,130,360,0,-1,-1,0,0
38560 DATA 3798,930,3794,930,130,360,0,-1,-1,0,0
38570 DATA 3794,930,3794,850,130,360,0,-1,-1,0,0
38580 DATA 368,135,170,130,-1,255,0,4
38590 DATA 3290,1094,3370,1094,130,360,0,-1,-1,0,0
38600 DATA 3370,1094,3370,1098,130,360,0,-1,-1,0,0
38610 DATA 3370,1098,3290,1098,130,360,0,-1,-1,0,0
38620 DATA 3290,1098,3290,1094,130,360,0,-1,-1,0,0
38630 DATA 369,-40,170,121,-1,255,0,4
38640 DATA 3240,810,3350,810,121,360,0,-1,-1,0,0
38650 DATA 3350,810,3350,930,121,360,0,-1,-1,0,0
38660 DATA 3350,930,3240,930,121,360,0,-1,-1,0,0
38670 DATA 3240,930,3240,810,121,360,0,-1,-1,0,0
38680 DATA 399,0,400,120,-1,255,99,4
38690 DATA 3200,140,3800,140,103,-1,0,-1,-1,0,0
38700 DATA 3800,140,3800,700,103,-1,0,-1,-1,0,0
38710 DATA 3800,700,3200,700,103,360,0,-1,-1,0,0
38720 DATA 3200,700,3200,140,103,-1,0,-1,-1,0,0
38730 DATA 400,0,170,118,-1,255,10,24
38740 DATA 0,1400,600,1400,110,439,0,-1,-1,0,0
38750 DATA 600,1400,600,1800,110,-1,0,-1,-1,0,0
38760 DATA 600,1800,0,1800,110,-1,0,-1,-1,0,0
38770 DATA 0,1800,0,1400,110,-1,0,-1,-1,0,0
38780 DATA 430,1700,430,1796,112,401,0,-1,-1,0,0
38790 DATA 430,1796,560,1796,112,401,0,-1,-1,0,0
38800 DATA 560,1796,560,1700,112,401,0,-1,-1,0,0
38810 DATA 560,1700,430,1700,112,401,0,-1,-1,0,0
38820 DATA 300,1792,300,1798,133,402,0,-1,-1,0,0
38830 DATA 300,1798,340,1798,133,402,0,-1,-1,0,0
38840 DATA 340,1798,340,1792,133,402,0,-1,-1,0,0
38850 DATA 340,1792,300,1792,133,402,0,-1,-1,0,0
38860 DATA 2,1540,2,1630,130,403,0,-1,-1,0,0
38870 DATA 2,1630,6,1630,130,403,0,-1,-1,0,0
38880 DATA 6,1630,6,1540,130,403,0,-1,-1,0,0
38890 DATA 6,1540,2,1540,130,403,0,-1,-1,0,0
38900 DATA 594,1540,594,1630,130,404,0,-1,-1,0,0
38910 DATA 594,1630,598,1630,130,404,0,-1,-1,0,0
38920 DATA 598,1630,598,1540,130,404,0,-1,-1,0,0
38930 DATA 598,1540,594,1540,130,404,0,-1,-1,0,0
38940 DATA 60,1520,60,1660,121,405,0,-1,-1,0,0
38950 DATA 60,1660,170,1660,121,405,0,-1,-1,0,0
38960 DATA 170,1660,170,1520,121,405,0,-1,-1,0,0
38970 DATA 170,1520,60,1520,121,405,0,-1,-1,0,0
38980 DATA 401,50,170,112,-1,255,0,4
38990 DATA 430,1700,560,1700,112,400,0,-1,-1,0,0
39000 DATA 560,1700,560,1796,112,400,0,-1,-1,0,0
39010 DATA 560,1796,430,1796,112,400,0,-1,-1,0,0
39020 DATA 430,1796,430,1700,112,400,0,-1,-1,0,0
39030 DATA 402,165,170,133,-1,255,0,4
39040 DATA 300,1792,340,1792,133,400,0,-1,-1,0,0
39050 DATA 340,1792,340,1798,133,400,0,-1,-1,0,0
39060 DATA 340,1798,300,1798,133,400,0,-1,-1,0,0
39070 DATA 300,1798,300,1792,133,400,0,-1,-1,0,0
39080 DATA 403,135,170,130,-1,255,0,4
39090 DATA 2,1540,6,1540,130,400,0,-1,-1,0,0
39100 DATA 6,1540,6,1630,130,400,0,-1,-1,0,0
39110 DATA 6,1630,2,1630,130,400,0,-1,-1,0,0
39120 DATA 2,1630,2,1540,130,400,0,-1,-1,0,0
39130 DATA 404,135,170,130,-1,255,0,4
39140 DATA 594,1540,598,1540,130,400,0,-1,-1,0,0
39150 DATA 598,1540,598,1630,130,400,0,-1,-1,0,0
39160 DATA 598,1630,594,1630,130,400,0,-1,-1,0,0
39170 DATA 594,1630,594,1540,130,400,0,-1,-1,0,0
39180 DATA 405,-40,170,121,-1,255,0,4
39190 DATA 60,1520,170,1520,121,400,0,-1,-1,0,0
39200 DATA 170,1520,170,1660,121,400,0,-1,-1,0,0
39210 DATA 170,1660,60,1660,121,400,0,-1,-1,0,0
39220 DATA 60,1660,60,1520,121,400,0,-1,-1,0,0
39230 DATA 439,0,400,118,-1,255,99,4
39240 DATA 0,840,600,840,110,-1,0,-1,-1,0,0
39250 DATA 600,840,600,1400,110,-1,0,-1,-1,0,0
39260 DATA 600,1400,0,1400,110,400,0,-1,-1,0,0
39270 DATA 0,1400,0,840,110,-1,0,-1,-1,0,0
39280 DATA 440,0,170,119,-1,255,11,28
39290 DATA 800,1400,1400,1400,114,479,0,-1,-1,0,0
39300 DATA 1400,1400,1400,1800,114,-1,0,-1,-1,0,0
39310 DATA 1400,1800,800,1800,114,-1,0,-1,-1,0,0
39320 DATA 800,1800,800,1400,114,-1,0,-1,-1,0,0
39330 DATA 1090,1600,1090,1788,114,441,0,-1,-1,0,0
39340 DATA 1090,1788,1370,1788,114,441,0,-1,-1,0,0
39350 DATA 1370,1788,1370,1600,114,441,0,-1,-1,0,0
39360 DATA 1370,1600,1090,1600,114,441,0,-1,-1,0,0
39370 DATA 1090,1790,1090,1796,112,442,0,-1,-1,0,0
39380 DATA 1090,1796,1370,1796,112,442,0,-1,-1,0,0
39390 DATA 1370,1796,1370,1790,112,442,0,-1,-1,0,0
39400 DATA 1370,1790,1090,1790,112,442,0,-1,-1,0,0
39410 DATA 840,1720,840,1796,112,443,0,-1,-1,0,0
39420 DATA 840,1796,940,1796,112,443,0,-1,-1,0,0
39430 DATA 940,1796,940,1720,112,443,0,-1,-1,0,0
39440 DATA 940,1720,840,1720,112,443,0,-1,-1,0,0
39450 DATA 1000,1740,1000,1796,112,444,0,-1,-1,0,0
39460 DATA 1000,1796,1065,1796,112,444,0,-1,-1,0,0
39470 DATA 1065,1796,1065,1740,112,444,0,-1,-1,0,0
39480 DATA 1065,1740,1000,1740,112,444,0,-1,-1,0,0
39490 DATA 802,1490,802,1600,110,445,0,-1,-1,0,0
39500 DATA 802,1600,855,1600,110,445,0,-1,-1,0,0
39510 DATA 855,1600,855,1490,110,445,0,-1,-1,0,0
39520 DATA 855,1490,802,1490,110,445,0,-1,-1,0,0
39530 DATA 802,1540,802,1630,130,446,0,-1,-1,0,0
39540 DATA 802,1630,806,1630,130,446,0,-1,-1,0,0
39550 DATA 806,1630,806,1540,130,446,0,-1,-1,0,0
39560 DATA 806,1540,802,1540,130,446,0,-1,-1,0,0
39570 DATA 441,52,170,114,-1,255,0,4
39580 DATA 1090,1600,1370,1600,114,440,0,-1,-1,0,0
39590 DATA 1370,1600,1370,1788,114,440,0,-1,-1,0,0
39600 DATA 1370,1788,1090,1788,114,440,0,-1,-1,0,0
39610 DATA 1090,1788,1090,1600,114,440,0,-1,-1,0,0
39620 DATA 442,95,170,112,-1,255,0,4
39630 DATA 1090,1790,1370,1790,112,440,0,-1,-1,0,0
39640 DATA 1370,1790,1370,1796,112,440,0,-1,-1,0,0
39650 DATA 1370,1796,1090,1796,112,440,0,-1,-1,0,0
39660 DATA 1090,1796,1090,1790,112,440,0,-1,-1,0,0
39670 DATA 443,150,170,112,-1,255,0,4
39680 DATA 840,1720,940,1720,112,440,0,-1,-1,0,0
39690 DATA 940,1720,940,1796,112,440,0,-1,-1,0,0
39700 DATA 940,1796,840,1796,112,440,0,-1,-1,0,0
39710 DATA 840,1796,840,1720,112,440,0,-1,-1,0,0
39720 DATA 444,55,170,112,-1,255,0,4
39730 DATA 1000,1740,1065,1740,112,440,0,-1,-1,0,0
39740 DATA 1065,1740,1065,1796,112,440,0,-1,-1,0,0
39750 DATA 1065,1796,1000,1796,112,440,0,-1,-1,0,0
39760 DATA 1000,1796,1000,1740,112,440,0,-1,-1,0,0
39770 DATA 445,38,170,110,-1,255,0,4
39780 DATA 802,1490,855,1490,110,440,0,-1,-1,0,0
39790 DATA 855,1490,855,1600,110,440,0,-1,-1,0,0
39800 DATA 855,1600,802,1600,110,440,0,-1,-1,0,0
39810 DATA 802,1600,802,1490,110,440,0,-1,-1,0,0
39820 DATA 446,135,170,130,-1,255,0,4
39830 DATA 802,1540,806,1540,130,440,0,-1,-1,0,0
39840 DATA 806,1540,806,1630,130,440,0,-1,-1,0,0
39850 DATA 806,1630,802,1630,130,440,0,-1,-1,0,0
39860 DATA 802,1630,802,1540,130,440,0,-1,-1,0,0
39870 DATA 479,0,400,119,-1,255,99,4
39880 DATA 800,840,1400,840,114,-1,0,-1,-1,0,0
39890 DATA 1400,840,1400,1400,114,-1,0,-1,-1,0,0
39900 DATA 1400,1400,800,1400,114,440,0,-1,-1,0,0
39910 DATA 800,1400,800,840,114,-1,0,-1,-1,0,0
39920 DATA 480,0,170,120,-1,255,12,24
39930 DATA 1600,1400,2200,1400,106,519,0,-1,-1,0,0
39940 DATA 2200,1400,2200,1800,106,-1,0,-1,-1,0,0
39950 DATA 2200,1800,1600,1800,106,-1,0,-1,-1,0,0
39960 DATA 1600,1800,1600,1400,106,-1,0,-1,-1,0,0
39970 DATA 1930,1700,1930,1796,100,481,0,-1,-1,0,0
39980 DATA 1930,1796,2190,1796,100,481,0,-1,-1,0,0
39990 DATA 2190,1796,2190,1700,100,481,0,-1,-1,0,0
40000 DATA 2190,1700,1930,1700,100,481,0,-1,-1,0,0
40010 DATA 1930,1694,1930,1698,114,482,0,-1,-1,0,0
40020 DATA 1930,1698,2190,1698,114,482,0,-1,-1,0,0
40030 DATA 2190,1698,2190,1694,114,482,0,-1,-1,0,0
40040 DATA 2190,1694,1930,1694,114,482,0,-1,-1,0,0
40050 DATA 1670,1730,1670,1796,100,483,0,-1,-1,0,0
40060 DATA 1670,1796,1770,1796,100,483,0,-1,-1,0,0
40070 DATA 1770,1796,1770,1730,100,483,0,-1,-1,0,0
40080 DATA 1770,1730,1670,1730,100,483,0,-1,-1,0,0
40090 DATA 1602,1660,1602,1730,100,484,0,-1,-1,0,0
40100 DATA 1602,1730,1622,1730,100,484,0,-1,-1,0,0
40110 DATA 1622,1730,1622,1660,100,484,0,-1,-1,0,0
40120 DATA 1622,1660,1602,1660,100,484,0,-1,-1,0,0
40130 DATA 2194,1540,2194,1630,130,485,0,-1,-1,0,0
40140 DATA 2194,1630,2198,1630,130,485,0,-1,-1,0,0
40150 DATA 2198,1630,2198,1540,130,485,0,-1,-1,0,0
40160 DATA 2198,1540,2194,1540,130,485,0,-1,-1,0,0
40170 DATA 481,48,170,100,-1,255,0,4
40180 DATA 1930,1700,2190,1700,100,480,0,-1,-1,0,0
40190 DATA 2190,1700,2190,1796,100,480,0,-1,-1,0,0
40200 DATA 2190,1796,1930,1796,100,480,0,-1,-1,0,0
40210 DATA 1930,1796,1930,1700,100,480,0,-1,-1,0,0
40220 DATA 482,150,170,114,-1,255,0,4
40230 DATA 1930,1694,2190,1694,114,480,0,-1,-1,0,0
40240 DATA 2190,1694,2190,1698,114,480,0,-1,-1,0,0
40250 DATA 2190,1698,1930,1698,114,480,0,-1,-1,0,0
40260 DATA 1930,1698,1930,1694,114,480,0,-1,-1,0,0
40270 DATA 483,72,170,100,-1,255,0,4
40280 DATA 1670,1730,1770,1730,100,480,0,-1,-1,0,0
40290 DATA 1770,1730,1770,1796,100,480,0,-1,-1,0,0
40300 DATA 1770,1796,1670,1796,100,480,0,-1,-1,0,0
40310 DATA 1670,1796,1670,1730,100,480,0,-1,-1,0,0
40320 DATA 484,150,170,100,-1,255,0,4
40330 DATA 1602,1660,1622,1660,100,480,0,-1,-1,0,0
40340 DATA 1622,1660,1622,1730,100,480,0,-1,-1,0,0
40350 DATA 1622,1730,1602,1730,100,480,0,-1,-1,0,0
40360 DATA 1602,1730,1602,1660,100,480,0,-1,-1,0,0
40370 DATA 485,135,170,130,-1,255,0,4
40380 DATA 2194,1540,2198,1540,130,480,0,-1,-1,0,0
40390 DATA 2198,1540,2198,1630,130,480,0,-1,-1,0,0
40400 DATA 2198,1630,2194,1630,130,480,0,-1,-1,0,0
40410 DATA 2194,1630,2194,1540,130,480,0,-1,-1,0,0
40420 DATA 519,0,400,120,-1,255,99,4
40430 DATA 1600,840,2200,840,106,-1,0,-1,-1,0,0
40440 DATA 2200,840,2200,1400,106,-1,0,-1,-1,0,0
40450 DATA 2200,1400,1600,1400,106,480,0,-1,-1,0,0
40460 DATA 1600,1400,1600,840,106,-1,0,-1,-1,0,0
40470 DATA 520,0,170,121,-1,140,13,52
40480 DATA 2400,1400,3000,1400,100,559,0,-1,-1,0,0
40490 DATA 3000,1400,3000,1800,100,-1,0,-1,-1,0,0
40500 DATA 3000,1800,2400,1800,100,-1,0,-1,-1,0,0
40510 DATA 2400,1800,2400,1400,100,-1,0,-1,-1,0,0
40520 DATA 2900,1470,2900,1508,100,521,0,-1,-1,0,0
40530 DATA 2900,1508,2998,1508,100,521,0,-1,-1,0,0
40540 DATA 2998,1508,2998,1470,100,521,0,-1,-1,0,0
40550 DATA 2998,1470,2900,1470,100,521,0,-1,-1,0,0
40560 DATA 2900,1510,2900,1548,100,522,0,-1,-1,0,0
40570 DATA 2900,1548,2998,1548,100,522,0,-1,-1,0,0
40580 DATA 2998,1548,2998,1510,100,522,0,-1,-1,0,0
40590 DATA 2998,1510,2900,1510,100,522,0,-1,-1,0,0
40600 DATA 2900,1550,2900,1588,100,523,0,-1,-1,0,0
40610 DATA 2900,1588,2998,1588,100,523,0,-1,-1,0,0
40620 DATA 2998,1588,2998,1550,100,523,0,-1,-1,0,0
40630 DATA 2998,1550,2900,1550,100,523,0,-1,-1,0,0
40640 DATA 2900,1590,2900,1628,100,524,0,-1,-1,0,0
40650 DATA 2900,1628,2998,1628,100,524,0,-1,-1,0,0
40660 DATA 2998,1628,2998,1590,100,524,0,-1,-1,0,0
40670 DATA 2998,1590,2900,1590,100,524,0,-1,-1,0,0
40680 DATA 2900,1630,2900,1668,100,525,0,-1,-1,0,0
40690 DATA 2900,1668,2998,1668,100,525,0,-1,-1,0,0
40700 DATA 2998,1668,2998,1630,100,525,0,-1,-1,0,0
40710 DATA 2998,1630,2900,1630,100,525,0,-1,-1,0,0
40720 DATA 2900,1670,2900,1708,100,526,0,-1,-1,0,0
40730 DATA 2900,1708,2998,1708,100,526,0,-1,-1,0,0
40740 DATA 2998,1708,2998,1670,100,526,0,-1,-1,0,0
40750 DATA 2998,1670,2900,1670,100,526,0,-1,-1,0,0
40760 DATA 2900,1710,2900,1748,100,527,0,-1,-1,0,0
40770 DATA 2900,1748,2998,1748,100,527,0,-1,-1,0,0
40780 DATA 2998,1748,2998,1710,100,527,0,-1,-1,0,0
40790 DATA 2998,1710,2900,1710,100,527,0,-1,-1,0,0
40800 DATA 2530,1730,2530,1796,100,528,0,-1,-1,0,0
40810 DATA 2530,1796,2640,1796,100,528,0,-1,-1,0,0
40820 DATA 2640,1796,2640,1730,100,528,0,-1,-1,0,0
40830 DATA 2640,1730,2530,1730,100,528,0,-1,-1,0,0
40840 DATA 2780,1740,2780,1796,112,529,0,-1,-1,0,0
40850 DATA 2780,1796,2850,1796,112,529,0,-1,-1,0,0
40860 DATA 2850,1796,2850,1740,112,529,0,-1,-1,0,0
40870 DATA 2850,1740,2780,1740,112,529,0,-1,-1,0,0
40880 DATA 2860,1730,2860,1796,112,530,0,-1,-1,0,0
40890 DATA 2860,1796,2930,1796,112,530,0,-1,-1,0,0
40900 DATA 2930,1796,2930,1730,112,530,0,-1,-1,0,0
40910 DATA 2930,1730,2860,1730,112,530,0,-1,-1,0,0
40920 DATA 2670,1775,2670,1796,112,531,0,-1,-1,0,0
40930 DATA 2670,1796,2770,1796,112,531,0,-1,-1,0,0
40940 DATA 2770,1796,2770,1775,112,531,0,-1,-1,0,0
40950 DATA 2770,1775,2670,1775,112,531,0,-1,-1,0,0
40960 DATA 2994,1600,2994,1660,100,532,0,-1,-1,0,0
40970 DATA 2994,1660,2998,1660,100,532,0,-1,-1,0,0
40980 DATA 2998,1660,2998,1600,100,532,0,-1,-1,0,0
40990 DATA 2998,1600,2994,1600,100,532,0,-1,-1,0,0
41000 DATA 521,14,170,100,-1,255,0,4
41010 DATA 2900,1470,2998,1470,100,520,0,-1,-1,0,0
41020 DATA 2998,1470,2998,1508,100,520,0,-1,-1,0,0
41030 DATA 2998,1508,2900,1508,100,520,0,-1,-1,0,0
41040 DATA 2900,1508,2900,1470,100,520,0,-1,-1,0,0
41050 DATA 522,28,170,100,-1,255,0,4
41060 DATA 2900,1510,2998,1510,100,520,0,-1,-1,0,0
41070 DATA 2998,1510,2998,1548,100,520,0,-1,-1,0,0
41080 DATA 2998,1548,2900,1548,100,520,0,-1,-1,0,0
41090 DATA 2900,1548,2900,1510,100,520,0,-1,-1,0,0
41100 DATA 523,42,170,100,-1,255,0,4
41110 DATA 2900,1550,2998,1550,100,520,0,-1,-1,0,0
41120 DATA 2998,1550,2998,1588,100,520,0,-1,-1,0,0
41130 DATA 2998,1588,2900,1588,100,520,0,-1,-1,0,0
41140 DATA 2900,1588,2900,1550,100,520,0,-1,-1,0,0
41150 DATA 524,56,170,100,-1,255,0,4
41160 DATA 2900,1590,2998,1590,100,520,0,-1,-1,0,0
41170 DATA 2998,1590,2998,1628,100,520,0,-1,-1,0,0
41180 DATA 2998,1628,2900,1628,100,520,0,-1,-1,0,0
41190 DATA 2900,1628,2900,1590,100,520,0,-1,-1,0,0
41200 DATA 525,70,170,100,-1,255,0,4
41210 DATA 2900,1630,2998,1630,100,520,0,-1,-1,0,0
41220 DATA 2998,1630,2998,1668,100,520,0,-1,-1,0,0
41230 DATA 2998,1668,2900,1668,100,520,0,-1,-1,0,0
41240 DATA 2900,1668,2900,1630,100,520,0,-1,-1,0,0
41250 DATA 526,84,170,100,-1,255,0,4
41260 DATA 2900,1670,2998,1670,100,520,0,-1,-1,0,0
41270 DATA 2998,1670,2998,1708,100,520,0,-1,-1,0,0
41280 DATA 2998,1708,2900,1708,100,520,0,-1,-1,0,0
41290 DATA 2900,1708,2900,1670,100,520,0,-1,-1,0,0
41300 DATA 527,98,170,100,-1,255,0,4
41310 DATA 2900,1710,2998,1710,100,520,0,-1,-1,0,0
41320 DATA 2998,1710,2998,1748,100,520,0,-1,-1,0,0
41330 DATA 2998,1748,2900,1748,100,520,0,-1,-1,0,0
41340 DATA 2900,1748,2900,1710,100,520,0,-1,-1,0,0
41350 DATA 528,100,170,100,-1,255,0,4
41360 DATA 2530,1730,2640,1730,100,520,0,-1,-1,0,0
41370 DATA 2640,1730,2640,1796,100,520,0,-1,-1,0,0
41380 DATA 2640,1796,2530,1796,100,520,0,-1,-1,0,0
41390 DATA 2530,1796,2530,1730,100,520,0,-1,-1,0,0
41400 DATA 529,60,170,112,-1,255,0,4
41410 DATA 2780,1740,2850,1740,112,520,0,-1,-1,0,0
41420 DATA 2850,1740,2850,1796,112,520,0,-1,-1,0,0
41430 DATA 2850,1796,2780,1796,112,520,0,-1,-1,0,0
41440 DATA 2780,1796,2780,1740,112,520,0,-1,-1,0,0
41450 DATA 530,65,170,112,-1,255,0,4
41460 DATA 2860,1730,2930,1730,112,520,0,-1,-1,0,0
41470 DATA 2930,1730,2930,1796,112,520,0,-1,-1,0,0
41480 DATA 2930,1796,2860,1796,112,520,0,-1,-1,0,0
41490 DATA 2860,1796,2860,1730,112,520,0,-1,-1,0,0
41500 DATA 531,84,170,112,-1,255,0,4
41510 DATA 2670,1775,2770,1775,112,520,0,-1,-1,0,0
41520 DATA 2770,1775,2770,1796,112,520,0,-1,-1,0,0
41530 DATA 2770,1796,2670,1796,112,520,0,-1,-1,0,0
41540 DATA 2670,1796,2670,1775,112,520,0,-1,-1,0,0
41550 DATA 532,120,170,100,-1,255,0,4
41560 DATA 2994,1600,2998,1600,100,520,0,-1,-1,0,0
41570 DATA 2998,1600,2998,1660,100,520,0,-1,-1,0,0
41580 DATA 2998,1660,2994,1660,100,520,0,-1,-1,0,0
41590 DATA 2994,1660,2994,1600,100,520,0,-1,-1,0,0
41600 DATA 559,0,400,121,-1,140,99,4
41610 DATA 2400,840,3000,840,100,-1,0,-1,-1,0,0
41620 DATA 3000,840,3000,1400,100,-1,0,-1,-1,0,0
41630 DATA 3000,1400,2400,1400,100,520,0,-1,-1,0,0
41640 DATA 2400,1400,2400,840,100,-1,0,-1,-1,0,0
41650 DATA 560,0,170,118,-1,255,14,28
41660 DATA 3200,1400,3800,1400,112,599,0,-1,-1,0,0
41670 DATA 3800,1400,3800,1800,112,-1,0,-1,-1,0,0
41680 DATA 3800,1800,3200,1800,112,-1,0,-1,-1,0,0
41690 DATA 3200,1800,3200,1400,112,-1,0,-1,-1,0,0
41700 DATA 3640,1510,3640,1630,121,561,0,-1,-1,0,0
41710 DATA 3640,1630,3750,1630,121,561,0,-1,-1,0,0
41720 DATA 3750,1630,3750,1510,121,561,0,-1,-1,0,0
41730 DATA 3750,1510,3640,1510,121,561,0,-1,-1,0,0
41740 DATA 3250,1750,3250,1796,100,562,0,-1,-1,0,0
41750 DATA 3250,1796,3350,1796,100,562,0,-1,-1,0,0
41760 DATA 3350,1796,3350,1750,100,562,0,-1,-1,0,0
41770 DATA 3350,1750,3250,1750,100,562,0,-1,-1,0,0
41780 DATA 3630,1700,3630,1796,112,563,0,-1,-1,0,0
41790 DATA 3630,1796,3760,1796,112,563,0,-1,-1,0,0
41800 DATA 3760,1796,3760,1700,112,563,0,-1,-1,0,0
41810 DATA 3760,1700,3630,1700,112,563,0,-1,-1,0,0
41820 DATA 3400,1720,3400,1796,112,564,0,-1,-1,0,0
41830 DATA 3400,1796,3490,1796,112,564,0,-1,-1,0,0
41840 DATA 3490,1796,3490,1720,112,564,0,-1,-1,0,0
41850 DATA 3490,1720,3400,1720,112,564,0,-1,-1,0,0
41860 DATA 3370,1650,3370,1700,112,565,0,-1,-1,0,0
41870 DATA 3370,1700,3430,1700,112,565,0,-1,-1,0,0
41880 DATA 3430,1700,3430,1650,112,565,0,-1,-1,0,0
41890 DATA 3430,1650,3370,1650,112,565,0,-1,-1,0,0
41900 DATA 3572,1722,3572,1738,112,566,0,-1,-1,0,0
41910 DATA 3572,1738,3588,1738,112,566,0,-1,-1,0,0
41920 DATA 3588,1738,3588,1722,112,566,0,-1,-1,0,0
41930 DATA 3588,1722,3572,1722,112,566,0,-1,-1,0,0
41940 DATA 561,-40,170,121,-1,255,0,4
41950 DATA 3640,1510,3750,1510,121,560,0,-1,-1,0,0
41960 DATA 3750,1510,3750,1630,121,560,0,-1,-1,0,0
41970 DATA 3750,1630,3640,1630,121,560,0,-1,-1,0,0
41980 DATA 3640,1630,3640,1510,121,560,0,-1,-1,0,0
41990 DATA 562,125,170,100,-1,255,0,4
42000 DATA 3250,1750,3350,1750,100,560,0,-1,-1,0,0
42010 DATA 3350,1750,3350,1796,100,560,0,-1,-1,0,0
42020 DATA 3350,1796,3250,1796,100,560,0,-1,-1,0,0
42030 DATA 3250,1796,3250,1750,100,560,0,-1,-1,0,0
42040 DATA 563,50,170,112,-1,255,0,4
42050 DATA 3630,1700,3760,1700,112,560,0,-1,-1,0,0
42060 DATA 3760,1700,3760,1796,112,560,0,-1,-1,0,0
42070 DATA 3760,1796,3630,1796,112,560,0,-1,-1,0,0
42080 DATA 3630,1796,3630,1700,112,560,0,-1,-1,0,0
42090 DATA 564,55,170,112,-1,255,0,4
42100 DATA 3400,1720,3490,1720,112,560,0,-1,-1,0,0
42110 DATA 3490,1720,3490,1796,112,560,0,-1,-1,0,0
42120 DATA 3490,1796,3400,1796,112,560,0,-1,-1,0,0
42130 DATA 3400,1796,3400,1720,112,560,0,-1,-1,0,0
42140 DATA 565,40,170,112,-1,255,0,4
42150 DATA 3370,1650,3430,1650,112,560,0,-1,-1,0,0
42160 DATA 3430,1650,3430,1700,112,560,0,-1,-1,0,0
42170 DATA 3430,1700,3370,1700,112,560,0,-1,-1,0,0
42180 DATA 3370,1700,3370,1650,112,560,0,-1,-1,0,0
42190 DATA 566,50,170,112,-1,255,0,4
42200 DATA 3572,1722,3588,1722,112,560,0,-1,-1,0,0
42210 DATA 3588,1722,3588,1738,112,560,0,-1,-1,0,0
42220 DATA 3588,1738,3572,1738,112,560,0,-1,-1,0,0
42230 DATA 3572,1738,3572,1722,112,560,0,-1,-1,0,0
42240 DATA 599,0,400,118,-1,255,99,4
42250 DATA 3200,840,3800,840,112,-1,0,-1,-1,0,0
42260 DATA 3800,840,3800,1400,112,-1,0,-1,-1,0,0
42270 DATA 3800,1400,3200,1400,112,560,0,-1,-1,0,0
42280 DATA 3200,1400,3200,840,112,-1,0,-1,-1,0,0
42290 ' the start of the pictures (not in the console host): the renderer, the textures, the level, the anchors
42300 PROC V3_SETUP
42310   IF HL THEN EXIT PROC
42320   CALL W3K_SETUP
42330   CALL W3K_SETPREFS(0,0,1,62,100)
42340   CALL W3K_VIEW(0,0,640,240,2)
42350   W3D FOG 0
42360   CALL V3_TEXTURES
42370   CALL V3_LOADLEVEL
42380   CALL V3_ANCHORS
42390   V3ON=-1
42400   GFX CLEAR
42410   GFX BACKDROP RGB(6,6,14)
42420   GFX LAYER 0,ON
42430   GFX LAYER 3,ON
42440   GFX LAYER 2,ON
42450 END PROC
42460 PROC DRAWSCENE
42470   LOCAL X,Z
42480   IF HL THEN EXIT PROC
42490   CALL BEGINDRAW
42500   CURWH=11
42510   FWG=0
42520   IF FN ISDARK THEN
42530     GFX LINE 0,0,639,239,FN CC(0,1),BF
42540     CALL PUTLBL(200,110,"It is pitch dark down here.",FN CC(0,9))
42550   ELSE
42560     CALL V3_DRAW(ROOM)
42570     IF ROOM=R_CRASH THEN CALL PUTLBL(190,225,"The wreck: "+STR$(FN BUILT)+" of 8 parts built in",FN CC(15,14))
42580   END IF
42590   IF HIDING THEN
42600     GFX LINE 0,0,639,84,1,BF
42610     GFX LINE 0,160,639,239,1,BF
42620     GFX LINE 0,85,50,159,1,BF
42630     GFX LINE 590,85,639,159,1,BF
42640   END IF
42650   CALL HUD
42660   CALL ENDDRAW
42670 END PROC
42680 ' ------------------------------------------------------------------------------------------------ start of a game
42690 PROC INITHUES
42700   LOCAL I
42710   FOR I=0 TO 15
42720     READ MHT(I)
42730   NEXT I
42740 END PROC
42750 LABEL HUEDATA
42760 DATA 0,12,1,2,3,5,6,7,9,10,14,13,10,0,4,6
42770 PROC INITSOUND
42780   IF HL THEN EXIT PROC
42790   SFX 1,"W3 V0.15 ML X0.002,0.01,0.2,0.05 S700,300,24"
42800   SFX 2,"W2 V0.3 ML X0,0.01,0.3,0.08 S600,1000,16"
42810   SFX 3,"W1 V0.15 ML ~6,0.3 X0.01,0.05,0.3,0.1 S180,120,6"
42820   SFX 4,"W2 V0.4 ML ~8,0.3 S300,900,4"
42830   SFX 5,"W3 V0.4 ML X0,0.02,0.4,0.3 S300,60,8"
42840   SFX 6,"W2 V0.4 ML X0,0.01,0.5,0.4 S800,1600,5"
42850   SFX 7,"W3 V0.7 ML X0,0.01,0.5,0.4 S200,40,3"
42860   SFX 8,"W0 V0.3 ML ~3,0.5 X0.3,0.2,0.5,0.8 S60,90,2"
42870 END PROC
42880 PROC NEWGAME
42890   LOCAL I
42900   FOR I=1 TO NROOM
42910     RSEEN(I)=0
42920   NEXT I
42930   ROOM=R_FRONT
42940   TURN=0
42950   CAP=6
42960   HALF=4
42970   FRONTOPEN=-1
42980   BACKLOCK=-1:BACKOPEN=0
42990   HATCHLOCK=-1:HATCHOPEN=0
43000   SHEDLOCK=-1:SHEDOPEN=0
43010   OVENON=-1:OVENHOT=-1:OVENCOOL=0
43020   TAPON=0:MELT=0
43030   WASHON=-1
43040   GLORPHERE=-1
43050   MIRRORON=-1
43060   SAFEOPEN=0
43070   CRANKED=0:WELLUP=0
43080   LAMPLIT=0
43090   PHOTOBACK=0
43100   ONCHAIR=0:HIDING=0:HIDEOBJ=0
43110   DQHOME=0:DQROOM=0:DQT=30+FN RN(10):DQSTAY=0:DQCOOL=0:DQALERT=0:DQEYES=0
43120   LASTOBJ=0
43130   REDRAW=-1
43140   ENDREQ=0
43150   NH=0
43160   TNR=1:TNG=1:TNB=1
43170   DAYSTEP=-1
43180   PARTK(1)=K_COIL:PARTK(2)=K_COMPASS:PARTK(3)=K_CRYSTAL:PARTK(4)=K_NOZZLE
43190   MNX(1)=450:MNY(1)=40:MNX(2)=150:MNY(2)=78:MNX(3)=300:MNY(3)=62:MNX(4)=555:MNY(4)=44
43200   MNX(5)=240:MNY(5)=24:MNX(6)=385:MNY(6)=92:MNX(7)=200:MNY(7)=42:MNX(8)=335:MNY(8)=110
43210   PARTK(5)=K_FUELCELL:PARTK(6)=K_PLATE:PARTK(7)=K_GLYPH:PARTK(8)=K_ANTENNA
43220   HINT$(1)="The oven is the warmest place in the house, and warm things stay hidden. It is hot, though: turn the knob off and be patient."
43230   HINT$(2)="The fridge holds something frozen. Warm water would help: the kitchen has a sink with a tap."
43240   HINT$(3)="Something glows on top of the tall cupboard in the living room. You will need something to stand on."
43250   HINT$(4)="Look under the bed. The creature there guards something, and it eats only one kind of food: the cookbook and the shopping list say which."
43260   HINT$(5)="The cellar is dark: the lantern in the hall helps. The washing machine is running, and machines stop when they lose their power."
43270   HINT$(6)="The bathroom mirror is not glass. Four screws hold it, and they need an alien screwdriver. Sheds have tools."
43280   HINT$(7)="The safe in the attic wants a three-digit code. Somebody scratched it onto the back of something in the living room."
43290   HINT$(8)="The bucket is stuck deep in the well, and the windlass has no crank. The shed has one, the spare key is under something crooked in the back yard."
43300 END PROC
43310 ' ------------------------------------------------------------------------------------------------ saving and loading
43320 PROC SAVEGAME
43330   LOCAL I
43340   OPEN "local:deqq.sav" FOR OUTPUT AS #1
43350   PRINT #1,"DEQQ1"
43360   WRITE #1,ROOM,TURN,FRONTOPEN,BACKLOCK,BACKOPEN,HATCHLOCK,HATCHOPEN,SHEDLOCK,SHEDOPEN
43370   WRITE #1,OVENON,OVENHOT,OVENCOOL,TAPON,MELT,WASHON,GLORPHERE,MIRRORON,SAFEOPEN
43380   WRITE #1,CRANKED,WELLUP,LAMPLIT,PHOTOBACK,ONCHAIR,HIDING,HIDEOBJ,DQHOME,DQROOM
43390   WRITE #1,DQT,DQSTAY,DQCOOL,LASTOBJ
43400   FOR I=1 TO NROOM
43410     PRINT #1,RSEEN(I)
43420   NEXT I
43430   FOR I=1 TO NOBJ
43440     WRITE #1,OROOM(I),OIN(I),OREL(I),OOPEN(I),OHID(I),OSEEN(I),OFX(I),OFZ(I)
43450   NEXT I
43460   CLOSE #1
43470 END PROC
43480 FUNC LOADGAME
43490   LOCAL I,H$
43500   IF NOT EXISTS("local:deqq.sav") THEN RETURN 0
43510   OPEN "local:deqq.sav" FOR INPUT AS #1
43520   LINE INPUT #1,H$
43530   IF H$<>"DEQQ1" THEN
43540     CLOSE #1
43550     RETURN 0
43560   END IF
43570   INPUT #1,ROOM,TURN,FRONTOPEN,BACKLOCK,BACKOPEN,HATCHLOCK,HATCHOPEN,SHEDLOCK,SHEDOPEN
43580   INPUT #1,OVENON,OVENHOT,OVENCOOL,TAPON,MELT,WASHON,GLORPHERE,MIRRORON,SAFEOPEN
43590   INPUT #1,CRANKED,WELLUP,LAMPLIT,PHOTOBACK,ONCHAIR,HIDING,HIDEOBJ,DQHOME,DQROOM
43600   INPUT #1,DQT,DQSTAY,DQCOOL,LASTOBJ
43610   FOR I=1 TO NROOM
43620     INPUT #1,RSEEN(I)
43630   NEXT I
43640   FOR I=1 TO NOBJ
43650     INPUT #1,OROOM(I),OIN(I),OREL(I),OOPEN(I),OHID(I),OSEEN(I),OFX(I),OFZ(I)
43660   NEXT I
43670   CLOSE #1
43680   DAYSTEP=-1
43690   CALL HUM(DQHOME)
43700   RETURN -1
43710 END FUNC
43720 ' ------------------------------------------------------------------------------------------------ title and ending
43730 PROC TITLE
43740   LOCAL I,X,Y
43750   IF HL THEN EXIT PROC
43760   CALL BEGINDRAW
43770   FOR Y=0 TO 239
43780     GFX LINE 0,Y,639,Y,FN CC(8,1+INT(Y/22))
43790   NEXT Y
43800   FOR I=1 TO 90
43810     GFX PSET FN RN(640),FN RN(150),FN CC(0,10+FN RN(6))
43820   NEXT I
43830   ' a crooked house on a hill, a small saucer in the sky
43840   GFX LINE 0,200,639,239,FN CC(5,3),BF
43850   CALL FELL(320,215,330,28,FN CC(5,4))
43860   CALL RECTO(250,150,390,205,FN CC(12,4))
43870   CALL TRAPH(320,115,150,20,95,FN CC(12,3))
43880   CALL RECTO(305,170,335,205,FN CC(1,5))
43890   CALL FELLO(320,135,9,9,FN CC(4,13))
43900   CALL RECTO(268,162,288,180,FN CC(4,12))
43910   CALL RECTO(352,162,372,180,FN CC(4,11))
43920   CALL FELLO(560,110,30,9,FN CC(13,9))
43930   CALL FELLO(560,103,14,10,FN CC(7,13))
43940   GFX LINE 545,120,490,170,FN CC(4,9)
43950   GFX LINE 575,120,610,190,FN CC(4,9)
43960   GFX TEXT 82,12,"DEQQ'S HOUSE",1,5,5
43970   GFX TEXT 80,10,"DEQQ'S HOUSE",FN CC(15,14),5,5
43980   CALL PUTLBL(210,222,"A graphics adventure",FN CC(0,13))
43990   CALL ENDDRAW
44000 END PROC
44010 PROC INTRO
44020   CALL TITLE
44030   IF NOT HL THEN PLAY "T50 W2 V0.22 O3 L2 A E- A F","T50 W0 V0.15 O2 L1 A E"
44040   CALL SAY("Deqq the alien crash-landed here and hid the eight parts of his ship in and around his house. Build the ship at the wreck behind the house.",1)
44050   CALL SAY("He is out on an errand and left the front door open, but he comes home now and then. HELP lists the commands. Press a key.",5)
44060   CALL FLUSH
44070   CALL WAITKEY
44080   IF PLAYON AND ENV$("DEQQ_SHOTDIR")<>"" THEN CALL MARK("TITLE")
44090 END PROC
44100 PROC MARK(N$)
44110   OPEN ENV$("DEQQ_SHOTDIR")+"/"+N$+".mark" FOR OUTPUT AS #5
44120   PRINT #5,"x"
44130   CLOSE #5
44140   WAIT 45
44150 END PROC
44160 PROC LIFTOFF(WITHDEQQ)
44170   LOCAL K,Y,X,I
44180   IF HL THEN EXIT PROC
44190   FOR K=0 TO 70
44200     CALL BEGINDRAW
44210     FOR Y=0 TO 239
44220       GFX LINE 0,Y,639,Y,FN CC(8,1+INT(Y/30))
44230     NEXT Y
44240     FOR I=1 TO 40
44250       X=(I*97+K*3) MOD 640
44260       GFX PSET X,(I*53) MOD 200,FN CC(0,8+(I MOD 7))
44270     NEXT I
44280     GFX LINE 0,220,639,239,FN CC(5,3),BF
44290     Y=200-K*4
44300     X=320
44310     CALL TRAPH(X,Y-34,Y,3,12,FN CC(0,13))
44320     CALL FELLO(X,Y+4,12,8,FN CC(13,9))
44330     CALL FELLO(X,Y-26,5,5,FN CC(7,13))
44340     CALL TRAPH(X,Y+10,Y+10+INT(20+FN RN(18)),9,1,FN CC(4,13))
44350     CALL TRAPH(X,Y+10,Y+10+INT(10+FN RN(8)),5,1,FN CC(15,15))
44360     CALL ENDDRAW
44370     IF K=1 THEN CALL SND(5)
44380     WAIT 2
44390   NEXT K
44400 END PROC
44410 PROC ENDSCENE
44420   CALL HUM(0)
44430   IF DQHOME THEN
44440     CALL SAY("Footsteps in the gravel behind you. Deqq stands at the edge of the hollow, taller than you remembered, and for a long time he says nothing.",1)
44450     CALL SAY("'You found them,' says the translator at last. 'All eight. I hid them well. Nobody ever...' The big black eyes shine. 'Thank you.'",2)
44460     CALL SAY("He walks to the ship, puts a long hand on the hull as if on the neck of an animal, and climbs in. The hatch seals. The engines wake with a sound like a huge, happy cat.",1)
44470     CALL SAY("The ship rises over the hills and is gone. On the gatepost of the house a key is lying now, and a note: 'YOURS. DON'T OPEN THE BATHROOM CABINET.'",2)
44480     CALL FLUSH
44490     CALL LIFTOFF(-1)
44500   ELSE
44510     CALL SAY("The eighth part clicks into place and the wreck wakes. Lights run through the hull, the engines hum, the hatch opens like a welcome.",1)
44520     CALL SAY("You climb in. The console knows what you want before you do. The ship rises, shakes the dust of the hollow from its hull, and climbs.",1)
44530     CALL SAY("Far below on the road a tall grey figure with a shopping bag stands still and looks up. 'I hid them SO well,' says a very small voice in your head.",2)
44540     CALL FLUSH
44550     CALL LIFTOFF(0)
44560   END IF
44570   IF NOT HL THEN PLAY "T120 W2 V0.3 O4 L8 C E G >C4 <G >C2","T120 W0 V0.2 O2 L2 C G C"
44580   CALL SAY("THE END.  You built the ship in "+STR$(TURN)+" turns. Thanks for playing Deqq's House!",7)
44590   CALL FLUSH
44600   CALL WAITKEY
44610   RUNNING=0
44620 END PROC
44630 ' ------------------------------------------------------------------------------------------------ test commands (only when a play-back file is used)
44640 PROC DEBUGCMD(L$)
44650   LOCAL C$,P,K$,I
44660   CALL SPLITW(MID$(L$,2))
44670   C$=W$(1)
44680   IF C$="T" THEN
44690     DQT=VAL(W$(2))
44700     CALL SAY("[deqq in "+STR$(DQT)+" turns]",6)
44710   END IF
44720   IF C$="STAY" THEN DQSTAY=VAL(W$(2))
44730   IF C$="DEQQ" THEN
44740     DQHOME=-1
44750     DQROOM=FN RID(W$(2))
44760     DQSTAY=999
44770     OROOM(K_DEQQ)=DQROOM
44780     REDRAW=-1
44790   END IF
44800   IF C$="GOTO" THEN
44810     ROOM=FN RID(W$(2))
44820     REDRAW=-1
44830     HIDING=0
44840     CALL LOOKROOM(0)
44850   END IF
44860   IF C$="HAVE" THEN
44870     I=FN OID(W$(2))
44880     OROOM(I)=-1
44890     OIN(I)=0
44900     OHID(I)=0
44910   END IF
44920   IF C$="WAIT" THEN WAIT VAL(W$(2))
44930   IF C$="STATE" THEN
44940     CALL SAY("[room "+STR$(ROOM)+" turn "+STR$(TURN)+" built "+STR$(FN BUILT)+" deqq home "+STR$(DQHOME)+" in "+STR$(DQROOM)+" t="+STR$(DQT)+" stay="+STR$(DQSTAY)+" front open "+STR$(FRONTOPEN)+" hiding "+STR$(HIDING)+"]",6)
44950   END IF
44960   IF C$="SEED" THEN RANDOMIZE VAL(W$(2))
44970   IF C$="TIME" THEN
44980     I=TI
44990     CALL DRAWSCENE
45000     CALL SAY("[drawing took "+STR$(TI-I)+" jiffies]",6)
45010   END IF
45020   IF C$="SHOT" THEN
45030     IF REDRAW THEN CALL DRAWSCENE
45040     REDRAW=0
45050     CALL FLUSH
45060     CALL MARK(W$(2))
45070   END IF
45080 END PROC
45090 ' ------------------------------------------------------------------------------------------------ main program
45100 LABEL MAIN
45110   HL=ENV$("DEQQ_HEADLESS")<>""
45120   IF ENV$("DEQQ_SYNTAXONLY")<>"" THEN BYE
45130   RANDOMIZE
45140   PLAYON=0
45150   LOGON=0
45160   IF ENV$("DEQQ_PLAY")<>"" THEN
45170     OPEN ENV$("DEQQ_PLAY") FOR INPUT AS #4
45180     PLAYON=-1
45190   END IF
45200   IF ENV$("DEQQ_LOG")<>"" THEN
45210     OPEN ENV$("DEQQ_LOG") FOR OUTPUT AS #3
45220     LOGON=-1
45230   END IF
45240   RESTORE HUEDATA
45250   CALL INITHUES
45260   BK=3
45270   FR=0
45280   IF NOT HL THEN
45290     GFX RESET
45300     GFX BACKDROP RGB(6,6,14)
45310     GFX LAYER 0,ON
45320     GFX LAYER 3,ON
45330     MOUSE HIDE
45340     BREAK OFF
45350   END IF
45360   CALL LOADWORLD
45370   CALL V3_SETUP
45380   CALL NEWGAME
45390   CALL INITSOUND
45400   CALL TEXTINIT
45410   CALL SETPAL(1)
45420   CALL INTRO
45430   RUNNING=-1
45440   CALL LOOKROOM(-1)
45450   CALL DRAWSCENE
45460   REDRAW=0
45470   CALL FLUSH
45480   WHILE RUNNING
45490     LN$=FN GETLINE$
45500     TCOST=0
45510     IF LN$<>"" THEN
45520       CALL ADDLINE("> "+LN$,6)
45530       IF PLAYON AND LEFT$(LN$,1)="#" THEN
45540         CALL DEBUGCMD(LN$)
45550       ELSE
45560         CALL EXECUTE(LN$)
45570       END IF
45580       IF REQSAVE THEN
45590         REQSAVE=0
45600         ON ERROR GOTO SVERR
45610         CALL SAVEGAME
45620         ON ERROR OFF
45630         CALL SAY("Game saved.",1)
45640       END IF
45650       LABEL SVDONE
45660       IF REQLOAD THEN
45670         REQLOAD=0
45680         IF FN LOADGAME THEN
45690           CALL SAY("Game loaded.",1)
45700           REDRAW=-1
45710           CALL LOOKROOM(-1)
45720         ELSE
45730           CALL SAY("There is no saved game.",1)
45740         END IF
45750       END IF
45760       IF QUITREQ THEN
45770         QUITREQ=0
45780         CALL SAY("Really quit the game? (y/n)",3)
45790         CALL FLUSH
45800         LN$=FN GETLINE$
45810         IF FN UC$(LEFT$(LN$,1))="Y" THEN RUNNING=0
45820       END IF
45830       IF TCOST THEN CALL ENDTURN
45840       IF ENDREQ AND RUNNING THEN
45850         CALL DRAWSCENE
45860         CALL ENDSCENE
45870       END IF
45880       IF REDRAW AND RUNNING THEN
45890         CALL DRAWSCENE
45900         REDRAW=0
45910       END IF
45920       CALL FLUSH
45930     END IF
45940     IF PLAYEND THEN RUNNING=0
45950   WEND
45960   IF LOGON THEN CLOSE #3
45970   IF PLAYON THEN CLOSE #4
45980   IF NOT HL THEN
45990     SOUND OFF
46000     BREAK ON
46010     GFX RESET
46020     CLS
46030   END IF
46040   PRINT "Thanks for playing Deqq's House!"
46050   END
46060 LABEL SVERR
46070   ON ERROR OFF
46080   CALL SAY("The game could not be saved here.",3)
46090   RESUME SVDONE
