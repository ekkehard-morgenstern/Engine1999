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
70 ' How it is made: the top 4/5 of the screen (640 x 240 pixels of the graphics layer) show the room as a simple 3D world: every room is a box
80 ' (x = west to east 0..600, z = depth 0..400, y = height 0..170) seen from the south with a perspective camera; furniture is made of boxes whose
90 ' faces are filled with shaded colors from a 16 hues x 16 shades palette. The scene is drawn into the layer that is not shown and then swapped
100 ' in, so nothing flickers. The bottom 5 text rows (80 x 5) are the message window and the input line. Everything is turn-based: only actions
110 ' that change the world pass a turn, and Deqq's comings and goings count turns.
120 ' Window only. Environment for testing: DEQQ_PLAY=file (commands are read from a file), DEQQ_LOG=file (transcript), DEQQ_HEADLESS=1 (no graphics).
130 ' Written in the style of the other demos: PROC / FUNC / CALL / LOCAL, DATA tables for the world, labels for the main loop.
140 DIM OROOM(130),OIN(130),OREL(130),OSIZE(130),OOPEN(130),OHID(130),OSEEN(130),OFX(130),OFZ(130),OICON(130),OCOL(130),OTYP(130)
150 DIM ONAME$(130),OWORDS$(130),ODESC$(130),OKEY$(130),OPK$(130),OFL$(130)
160 DIM RNAME$(20),RDESC$(20),RKEY$(20),REX$(20),EX(20,10),RSEEN(20),RHIDE$(20)
170 DIM W$(30),LOGT$(300),LOGC(300),PQ$(80),PQC(80),HIST$(20),VW$(200),VID(200)
180 DIM HBR(16),HBG(16),HBB(16),HINT$(10),CMT$(20),DN$(9),SLX(6),SLZ(6),FC(20),PW$(14),PARTK(9),MNX(9),MNY(9),DS(20),WC(9)
190 GOTO MAIN
200 ' ------------------------------------------------------------------------------------------------ small helpers
210 FUNC UC$(A$)
220   LOCAL I,C,R$
230   R$=A$
240   FOR I=1 TO LEN(A$)
250     C=ASC(MID$(A$,I,1))
260     IF C>=97 AND C<=122 THEN R$=LEFT$(R$,I-1)+CHR$(C-32)+MID$(R$,I+1)
270   NEXT I
280   RETURN R$
290 END FUNC
300 ' SPOS: position of N$ in H$ (1 = first), 0 if it is not there
310 FUNC SPOS(H$,N$)
320   LOCAL I,L
330   L=LEN(N$)
340   FOR I=1 TO LEN(H$)-L+1
350     IF MID$(H$,I,L)=N$ THEN RETURN I
360   NEXT I
370   RETURN 0
380 END FUNC
390 ' RN: a random number 0 .. N-1
400 FUNC RN(N)
410   RETURN INT(RND(1)*N)
420 END FUNC
430 FUNC MAXI(A,B)
440   IF A>B THEN RETURN A
450   RETURN B
460 END FUNC
470 FUNC MINI(A,B)
480   IF A<B THEN RETURN A
490   RETURN B
500 END FUNC
510 ' the input line cut into words (upper case, only letters and digits); NW = number of words, W$(1..NW)
520 PROC SPLITW(L$)
530   LOCAL I,C,CH$,CUR$
540   NW=0
550   CUR$=""
560   FOR I=1 TO LEN(L$)+1
570     IF I<=LEN(L$) THEN CH$=FN UC$(MID$(L$,I,1)) ELSE CH$=" "
580     C=ASC(CH$)
590     IF (C>=65 AND C<=90) OR (C>=48 AND C<=57) OR C>=192 THEN
600       CUR$=CUR$+CH$
610     ELSE
620       IF CUR$<>"" AND NW<28 THEN
630         NW=NW+1
640         W$(NW)=CUR$
650       END IF
660       CUR$=""
670     END IF
680   NEXT I
690 END PROC
700 ' a word that is only digits?
710 FUNC ISNUM(A$)
720   LOCAL I,C
730   IF A$="" THEN RETURN 0
740   FOR I=1 TO LEN(A$)
750     C=ASC(MID$(A$,I,1))
760     IF C<48 OR C>57 THEN RETURN 0
770   NEXT I
780   RETURN -1
790 END FUNC
800 ' is word W$ in the blank-separated word list L$ (which starts and ends with a blank)?
810 FUNC INLIST(W$,L$)
820   IF W$="" THEN RETURN 0
830   RETURN FN SPOS(L$," "+W$+" ")>0
840 END FUNC
850 ' ------------------------------------------------------------------------------------------------ palette
860 ' Palette index = hue * 16 + shade (shade 0 = nearly black ... 10 = the color ... 15 = pale). Index 0 is transparent. Hue numbers:
870 ' 0 gray, 1 wood, 2 red, 3 orange, 4 yellow, 5 green, 6 teal, 7 sky, 8 blue, 9 purple, 10 pink, 11 cream, 12 violet, 13 steel, 14 lime, 15 glow
880 FUNC CC(H,S)
890   RETURN H*16+S
900 END FUNC
910 PROC SETPAL(BR)
920   LOCAL H,S,I,T,F,M,R,G,B
930   IF HL THEN EXIT PROC
940   FOR H=0 TO 15
950     FOR S=0 TO 15
960       I=H*16+S
970       IF I>0 THEN
980         T=S/15
990         IF T<0.7 THEN
1000           F=0.08+0.92*(T/0.7)
1010           M=0
1020         ELSE
1030           F=1
1040           M=(T-0.7)/0.3*0.6
1050         END IF
1060         R=FN MINI(255,INT((HBR(H)*F*(1-M)+255*M)*BR*TNR))
1070         G=FN MINI(255,INT((HBG(H)*F*(1-M)+255*M)*BR*TNG))
1080         B=FN MINI(255,INT((HBB(H)*F*(1-M)+255*M)*BR*TNB))
1090         GFX PALETTE 0,I,RGB(R,G,B)
1100         GFX PALETTE 3,I,RGB(R,G,B)
1110       END IF
1120     NEXT S
1130   NEXT H
1140 END PROC
1150 ' the text layer's colors: 20 paper, 21 text, 22 Deqq (mint), 23 warning (amber), 24 danger (red), 25 hint (cyan), 26 dim, 27 white
1160 PROC TEXTPAL
1170   IF HL THEN EXIT PROC
1180   GFX PALETTE 2,20,RGB(10,12,26)
1190   GFX PALETTE 2,21,RGB(205,210,225)
1200   GFX PALETTE 2,22,RGB(110,255,190)
1210   GFX PALETTE 2,23,RGB(255,190,70)
1220   GFX PALETTE 2,24,RGB(255,90,80)
1230   GFX PALETTE 2,25,RGB(110,200,255)
1240   GFX PALETTE 2,26,RGB(120,125,150)
1250   GFX PALETTE 2,27,RGB(255,255,255)
1260 END PROC
1270 ' ------------------------------------------------------------------------------------------------ double buffer
1280 ' the scene is drawn into the hidden graphics layer (BK) and then swapped in
1290 PROC BEGINDRAW
1300   IF HL THEN EXIT PROC
1310   GFX TARGET BK
1320   GFX CLEAR
1330 END PROC
1340 PROC ENDDRAW
1350   LOCAL T
1360   IF HL THEN EXIT PROC
1370   GFX LAYER BK,ON
1380   GFX LAYER FR,OFF
1390   T=BK:BK=FR:FR=T
1400 END PROC
1410 ' ------------------------------------------------------------------------------------------------ projection
1420 ' room coordinates: x west -> east 0..600, y up 0..170, z front -> back 0..400. Camera in front of the room, looking down.
1430 FUNC PX(X,Z)
1440   RETURN INT(320+(X-300)*0.96*560/(560+Z)+0.5)
1450 END FUNC
1460 FUNC PY(Y,Z)
1470   RETURN INT(-41+(273-Y)*560/(560+Z)+0.5)
1480 END FUNC
1490 FUNC SCALE(Z)
1500   RETURN 560/(560+Z)
1510 END FUNC
1520 ' a filled rectangle with an outline (screen coordinates)
1530 PROC RECTO(X0,Y0,X1,Y1,C)
1540   GFX LINE X0,Y0,X1,Y1,C,BF
1550   GFX LINE X0,Y0,X1,Y1,1,B
1560 END PROC
1570 ' a filled ellipse (screen coordinates)
1580 PROC FELL(CX,CY,RX,RY,C)
1590   LOCAL DY,HW
1600   IF RY<1 THEN RY=1
1610   FOR DY=-RY TO RY
1620     HW=INT(RX*SQR(FN MAXI(0,1-(DY*DY)/(RY*RY)))+0.5)
1630     GFX LINE CX-HW,CY+DY,CX+HW,CY+DY,C
1640   NEXT DY
1650 END PROC
1660 PROC FELLO(CX,CY,RX,RY,C)
1670   CALL FELL(CX,CY,RX+1,RY+1,1)
1680   CALL FELL(CX,CY,RX,RY,C)
1690 END PROC
1700 ' a symmetric trapezoid: rows Y0..Y1, half width W0 at the top and W1 at the bottom, around CX
1710 PROC TRAPH(CX,Y0,Y1,W0,W1,C)
1720   LOCAL Y,HW
1730   FOR Y=Y0 TO Y1
1740     IF Y1>Y0 THEN HW=INT(W0+(W1-W0)*(Y-Y0)/(Y1-Y0)+0.5) ELSE HW=W0
1750     GFX LINE CX-HW,Y,CX+HW,Y,C
1760   NEXT Y
1770   GFX LINE CX-W0,Y0,CX-W1,Y1,1
1780   GFX LINE CX+W0,Y0,CX+W1,Y1,1
1790   GFX LINE CX-W0,Y0,CX+W0,Y0,1
1800   GFX LINE CX-W1,Y1,CX+W1,Y1,1
1810 END PROC
1820 ' the face of a box that looks at the camera (constant z): a screen rectangle
1830 PROC FRONTF(X0,X1,Y0,Y1,Z,C)
1840   LOCAL A,B,D,E
1850   IF HL THEN EXIT PROC
1860   A=FN PX(X0,Z):B=FN PX(X1,Z):D=FN PY(Y1,Z):E=FN PY(Y0,Z)
1870   CALL RECTO(A,D,B,E,C)
1880 END PROC
1890 ' the top face (constant y): a trapezoid with horizontal edges
1900 PROC TOPF(X0,X1,Y,Z0,Z1,C)
1910   LOCAL YN,YT,L0,R0,L1,R1,R,T,XL,XR
1920   IF HL THEN EXIT PROC
1930   YN=FN PY(Y,Z0):YT=FN PY(Y,Z1)
1940   L0=FN PX(X0,Z0):R0=FN PX(X1,Z0):L1=FN PX(X0,Z1):R1=FN PX(X1,Z1)
1950   FOR R=YT TO YN
1960     IF YN>YT THEN T=(R-YT)/(YN-YT) ELSE T=1
1970     XL=INT(L1+(L0-L1)*T+0.5):XR=INT(R1+(R0-R1)*T+0.5)
1980     GFX LINE XL,R,XR,R,C
1990   NEXT R
2000   GFX LINE L0,YN,R0,YN,1
2010   GFX LINE L1,YT,R1,YT,1
2020   GFX LINE L0,YN,L1,YT,1
2030   GFX LINE R0,YN,R1,YT,1
2040 END PROC
2050 ' a side face (constant x): a trapezoid with vertical edges
2060 PROC SIDEF(X,Y0,Y1,Z0,Z1,C)
2070   LOCAL XN,XF,TN,BN,TF,BFA,A,B,CX,T,TP,BT
2080   IF HL THEN EXIT PROC
2090   XN=FN PX(X,Z0):XF=FN PX(X,Z1)
2100   TN=FN PY(Y1,Z0):BN=FN PY(Y0,Z0):TF=FN PY(Y1,Z1):BFA=FN PY(Y0,Z1)
2110   A=FN MINI(XN,XF):B=FN MAXI(XN,XF)
2120   FOR CX=A TO B
2130     IF XF<>XN THEN T=(CX-XN)/(XF-XN) ELSE T=0
2140     TP=INT(TN+(TF-TN)*T+0.5):BT=INT(BN+(BFA-BN)*T+0.5)
2150     GFX LINE CX,TP,CX,BT,C
2160   NEXT CX
2170   GFX LINE XN,TN,XN,BN,1
2180   GFX LINE XF,TF,XF,BFA,1
2190   GFX LINE XN,TN,XF,TF,1
2200   GFX LINE XN,BN,XF,BFA,1
2210 END PROC
2220 ' a box with the faces that the camera sees: front, top and the side that points to the middle of the room
2230 PROC BOX(X0,X1,Y0,Y1,Z0,Z1,H)
2240   IF HL THEN EXIT PROC
2250   IF X1<=300 THEN CALL SIDEF(X1,Y0,Y1,Z0,Z1,H*16+7)
2260   IF X0>=300 THEN CALL SIDEF(X0,Y0,Y1,Z0,Z1,H*16+8)
2270   CALL TOPF(X0,X1,Y1,Z0,Z1,H*16+13)
2280   CALL FRONTF(X0,X1,Y0,Y1,Z0,H*16+10)
2290 END PROC
2300 ' a line in room coordinates
2310 PROC LINE3(X0,Y0,Z0,X1,Y1,Z1,C)
2320   IF HL THEN EXIT PROC
2330   GFX LINE FN PX(X0,Z0),FN PY(Y0,Z0),FN PX(X1,Z1),FN PY(Y1,Z1),C
2340 END PROC
2350 ' text with a shadow on the graphics layer
2360 PROC PUTLBL(X,Y,T$,C)
2370   IF HL THEN EXIT PROC
2380   GFX TEXT X+1,Y+1,T$,1
2390   GFX TEXT X,Y,T$,C
2400 END PROC
2410 ' ------------------------------------------------------------------------------------------------ the room shell
2420 ' the box of a room: back wall, side walls, floor and the low wall in front (with a gap if there is an exit to the south).
2430 ' WH = wall hue, FH = floor hue, FP = floor pattern (1 planks, 2 tiles, 3 gravel/none), SEXIT = gap for an exit to the south
2440 PROC SHELL(WH,FH,FP,SEXIT)
2450   LOCAL K
2460   IF HL THEN EXIT PROC
2470   GFX LINE 0,0,639,239,FN CC(0,2),BF
2480   CALL FRONTF(0,600,0,170,400,FN CC(WH,11))
2490   CALL SIDEF(0,0,170,0,400,FN CC(WH,8))
2500   CALL SIDEF(600,0,170,0,400,FN CC(WH,9))
2510   CALL TOPF(0,600,0,0,400,FN CC(FH,9))
2520   IF FP=1 THEN
2530     FOR K=1 TO 5
2540       CALL LINE3(K*100,0,0,K*100,0,400,FN CC(FH,6))
2550     NEXT K
2560   END IF
2570   IF FP=2 THEN
2580     FOR K=1 TO 5
2590       CALL LINE3(K*100,0,0,K*100,0,400,FN CC(FH,11))
2600     NEXT K
2610     FOR K=1 TO 3
2620       CALL LINE3(0,0,K*100,600,0,K*100,FN CC(FH,11))
2630     NEXT K
2640   END IF
2650   GFX LINE 0,233,639,239,FN CC(1,3),BF
2660   GFX LINE 0,233,639,233,1
2670 END PROC
2680 ' the low wall in the foreground, drawn after the furniture; a gap if there is a way south
2690 PROC FRONTWALL(WH,SEXIT)
2700   IF HL THEN EXIT PROC
2710   IF SEXIT THEN
2720     CALL TOPF(0,250,26,0,12,FN CC(WH,13))
2730     CALL FRONTF(0,250,0,26,0,FN CC(WH,10))
2740     CALL TOPF(350,600,26,0,12,FN CC(WH,13))
2750     CALL FRONTF(350,600,0,26,0,FN CC(WH,10))
2760     IF SEXIT=2 THEN
2770       CALL FRONTF(250,350,0,26,0,FN CC(1,9))
2780       CALL FRONTF(256,344,2,24,0,FN CC(1,11))
2790     ELSE
2800       CALL TOPF(250,350,0,0,40,FN CC(1,5))
2810     END IF
2820   ELSE
2830     CALL TOPF(0,600,26,0,12,FN CC(WH,13))
2840     CALL FRONTF(0,600,0,26,0,FN CC(WH,10))
2850   END IF
2860 END PROC
2870 ' a door in the back wall (x0..x1), the left wall (z0..z1) or the right wall; OP = open
2880 PROC DOORB(X0,X1,H,HUE,OP)
2890   IF HL THEN EXIT PROC
2900   IF OP THEN
2910     CALL FRONTF(X0,X1,0,H,400,FN CC(12,2))
2920     CALL FRONTF(X0+8,X1-8,0,H-8,400,FN CC(12,3))
2930   ELSE
2940     CALL FRONTF(X0,X1,0,H,400,FN CC(HUE,9))
2950     CALL FRONTF(X0+10,X1-10,12,H-14,400,FN CC(HUE,11))
2960     GFX CIRCLE FN PX(X1-14,400),FN PY(H*0.45,400),2,FN CC(4,12)
2970   END IF
2980 END PROC
2990 PROC DOORS(X,Z0,Z1,H,HUE,OP)
3000   IF HL THEN EXIT PROC
3010   IF OP THEN
3020     CALL SIDEF(X,0,H,Z0,Z1,FN CC(12,2))
3030   ELSE
3040     CALL SIDEF(X,0,H,Z0,Z1,FN CC(HUE,9))
3050     CALL SIDEF(X,12,H-14,Z0+10,Z1-10,FN CC(HUE,11))
3060   END IF
3070 END PROC
3080 ' a window in the back wall or in a side wall: sky color S (shade) of hue 7, a cross of frame
3090 PROC WINB(X0,X1,Y0,Y1,S)
3100   IF HL THEN EXIT PROC
3110   CALL FRONTF(X0,X1,Y0,Y1,400,FN CC(7,S))
3120   CALL LINE3((X0+X1)/2,Y0,400,(X0+X1)/2,Y1,400,1)
3130   CALL LINE3(X0,(Y0+Y1)/2,400,X1,(Y0+Y1)/2,400,1)
3140 END PROC
3150 PROC WINS(X,Z0,Z1,Y0,Y1,S)
3160   IF HL THEN EXIT PROC
3170   CALL SIDEF(X,Y0,Y1,Z0,Z1,FN CC(7,S))
3180   CALL LINE3(X,Y0,(Z0+Z1)/2,X,Y1,(Z0+Z1)/2,1)
3190   CALL LINE3(X,(Y0+Y1)/2,Z0,X,(Y0+Y1)/2,Z1,1)
3200 END PROC
3210 ' stairs going up along the right wall (they run away from the camera), and a hole in the floor with steps going down
3220 PROC STAIRSUP(H)
3230   LOCAL K
3240   IF HL THEN EXIT PROC
3250   FOR K=6 TO 0 STEP -1
3260     CALL BOX(500,600,0,14*(K+1),70+K*40,110+K*40,H)
3270   NEXT K
3280 END PROC
3290 PROC STAIRSDOWN(X0,X1,Z0,Z1)
3300   LOCAL K,ZA
3310   IF HL THEN EXIT PROC
3320   CALL TOPF(X0,X1,0,Z0,Z1,FN CC(12,2))
3330   FOR K=0 TO 4
3340     ZA=Z0+(Z1-Z0)*K/5
3350     CALL TOPF(X0+6,X1-6,0,ZA,ZA+(Z1-Z0)/5-2,FN CC(1,9-K*2))
3360   NEXT K
3370 END PROC
3380 ' ------------------------------------------------------------------------------------------------ Deqq
3390 ' Deqq stands at floor position X,Z: a tall cloaked alien with a big head and big black eyes (E = 0 normal, 1 eyes glow brighter)
3400 PROC DEQQFIG(X,Z,E)
3410   LOCAL S,BX,BY,HT,T
3420   IF HL THEN EXIT PROC
3430   S=FN SCALE(Z)
3440   BX=FN PX(X,Z):BY=FN PY(0,Z)
3450   HT=INT(112*S)
3460   CALL TRAPH(BX,BY-HT,BY,INT(9*S)+1,INT(25*S),FN CC(12,5))
3470   GFX LINE BX-INT(3*S),BY-HT+2,BX-INT(8*S),BY-2,FN CC(12,3)
3480   GFX LINE BX-INT(25*S),BY-INT(50*S),BX-INT(20*S),BY-INT(8*S),1
3490   GFX LINE BX+INT(25*S),BY-INT(50*S),BX+INT(20*S),BY-INT(8*S),1
3500   CALL FELLO(BX,BY-HT-INT(16*S),INT(13*S)+1,INT(19*S)+1,FN CC(6,11))
3510   CALL FELLO(BX-INT(6*S),BY-HT-INT(15*S),INT(4*S)+1,INT(7*S)+1,FN CC(0,1))
3520   CALL FELLO(BX+INT(6*S),BY-HT-INT(15*S),INT(4*S)+1,INT(7*S)+1,FN CC(0,1))
3530   T=FN CC(15,12+E*3)
3540   GFX PSET BX-INT(6*S),BY-HT-INT(17*S),T
3550   GFX PSET BX+INT(6*S),BY-HT-INT(17*S),T
3560   GFX PSET BX-INT(5*S),BY-HT-INT(17*S),T
3570   GFX PSET BX+INT(7*S),BY-HT-INT(17*S),T
3580   GFX LINE BX-INT(3*S),BY-HT-INT(5*S),BX+INT(3*S),BY-HT-INT(5*S),FN CC(6,6)
3590 END PROC
3600 ' ------------------------------------------------------------------------------------------------ small things
3610 ' ICON draws a thing that lies or stands at the screen position CX,CY (bottom middle), SC = size (the camera scale of its place), HU = hue
3620 FUNC UU(N)
3630   RETURN INT(N*SCL+0.5)
3640 END FUNC
3650 PROC ICON(K,CX,CY,SC,HU)
3660   LOCAL I,A
3670   IF HL THEN EXIT PROC
3680   SCL=SC*1.15
3690   IF K=1 THEN
3700     ' key: a ring, a shaft and teeth
3710     CALL FELLO(CX-FN UU(6),CY-FN UU(14),FN UU(4),FN UU(4),FN CC(4,12))
3720     GFX LINE CX-FN UU(2),CY-FN UU(14),CX+FN UU(9),CY-FN UU(14),FN CC(4,12)
3730     GFX LINE CX-FN UU(2),CY-FN UU(13),CX+FN UU(9),CY-FN UU(13),FN CC(4,9)
3740     GFX LINE CX+FN UU(7),CY-FN UU(14),CX+FN UU(7),CY-FN UU(9),FN CC(4,12)
3750     GFX LINE CX+FN UU(10),CY-FN UU(14),CX+FN UU(10),CY-FN UU(10),FN CC(4,12)
3760   END IF
3770   IF K=2 THEN
3780     ' crank handle
3790     CALL RECTO(CX-FN UU(9),CY-FN UU(5),CX+FN UU(4),CY-FN UU(2),FN CC(13,9))
3800     CALL RECTO(CX+FN UU(2),CY-FN UU(18),CX+FN UU(5),CY-FN UU(2),FN CC(13,9))
3810     CALL RECTO(CX+FN UU(0),CY-FN UU(23),CX+FN UU(7),CY-FN UU(18),FN CC(2,10))
3820   END IF
3830   IF K=3 THEN
3840     ' screwdriver / hammer: a handle in the hue of HU and a steel shaft
3850     CALL RECTO(CX-FN UU(4),CY-FN UU(22),CX+FN UU(4),CY-FN UU(11),FN CC(HU,10))
3860     GFX LINE CX,CY-FN UU(11),CX,CY-FN UU(2),FN CC(13,13)
3870     GFX LINE CX+1,CY-FN UU(11),CX+1,CY-FN UU(2),FN CC(13,9)
3880   END IF
3890   IF K=4 THEN
3900     ' lamp
3910     CALL RECTO(CX-FN UU(7),CY-FN UU(4),CX+FN UU(7),CY-FN UU(1),FN CC(13,8))
3920     CALL RECTO(CX-FN UU(6),CY-FN UU(18),CX+FN UU(6),CY-FN UU(4),FN CC(HU,13))
3930     CALL RECTO(CX-FN UU(4),CY-FN UU(23),CX+FN UU(4),CY-FN UU(18),FN CC(13,8))
3940     GFX LINE CX,CY-FN UU(23),CX,CY-FN UU(27),1
3950     GFX LINE CX-FN UU(3),CY-FN UU(14),CX+FN UU(3),CY-FN UU(14),FN CC(4,15)
3960   END IF
3970   IF K=5 THEN
3980     ' jar with something in it
3990     CALL RECTO(CX-FN UU(8),CY-FN UU(20),CX+FN UU(8),CY-FN UU(1),FN CC(7,13))
4000     CALL FELL(CX,CY-FN UU(9),FN UU(6),FN UU(6),FN CC(HU,11))
4010     CALL RECTO(CX-FN UU(9),CY-FN UU(24),CX+FN UU(9),CY-FN UU(20),FN CC(13,9))
4020   END IF
4030   IF K=6 THEN
4040     ' bottle
4050     CALL RECTO(CX-FN UU(6),CY-FN UU(18),CX+FN UU(6),CY-FN UU(1),FN CC(HU,13))
4060     CALL RECTO(CX-FN UU(3),CY-FN UU(26),CX+FN UU(3),CY-FN UU(18),FN CC(HU,12))
4070     GFX LINE CX-FN UU(3),CY-FN UU(27),CX+FN UU(3),CY-FN UU(27),FN CC(2,10)
4080   END IF
4090   IF K=7 THEN
4100     ' block of ice with something inside
4110     CALL RECTO(CX-FN UU(11),CY-FN UU(20),CX+FN UU(11),CY-FN UU(1),FN CC(7,14))
4120     GFX LINE CX-FN UU(8),CY-FN UU(17),CX-FN UU(3),CY-FN UU(17),FN CC(0,15)
4130     GFX LINE CX-FN UU(8),CY-FN UU(16),CX-FN UU(8),CY-FN UU(12),FN CC(0,15)
4140     CALL FELLO(CX+FN UU(2),CY-FN UU(9),FN UU(4),FN UU(4),FN CC(4,10))
4150   END IF
4160   IF K=8 THEN
4170     ' compass
4180     CALL FELLO(CX,CY-FN UU(10),FN UU(9),FN UU(9),FN CC(4,11))
4190     CALL FELL(CX,CY-FN UU(10),FN UU(6),FN UU(6),FN CC(11,15))
4200     GFX LINE CX,CY-FN UU(10),CX+FN UU(4),CY-FN UU(15),FN CC(2,10)
4210     GFX LINE CX,CY-FN UU(10),CX-FN UU(4),CY-FN UU(5),FN CC(8,10)
4220   END IF
4230   IF K=9 THEN
4240     ' coil of copper wire
4250     FOR I=0 TO 4
4260       CALL FELLO(CX,CY-FN UU(4+I*4),FN UU(10),FN UU(3),FN CC(3,10-(I AND 1)))
4270     NEXT I
4280     GFX LINE CX-FN UU(10),CY-FN UU(4),CX-FN UU(14),CY-FN UU(2),FN CC(0,3)
4290   END IF
4300   IF K=10 THEN
4310     ' crystal: a glowing diamond
4320     CALL FELL(CX,CY-FN UU(14),FN UU(14),FN UU(14),FN CC(15,6))
4330     CALL TRAPH(CX,CY-FN UU(28),CY-FN UU(14),FN UU(2),FN UU(9),FN CC(9,13))
4340     CALL TRAPH(CX,CY-FN UU(14),CY-FN UU(1),FN UU(9),FN UU(1),FN CC(9,9))
4350     GFX LINE CX-FN UU(3),CY-FN UU(24),CX-FN UU(5),CY-FN UU(16),FN CC(9,15)
4360   END IF
4370   IF K=11 THEN
4380     ' thruster nozzle
4390     CALL TRAPH(CX,CY-FN UU(26),CY-FN UU(2),FN UU(7),FN UU(17),FN CC(13,10))
4400     CALL FELLO(CX,CY-FN UU(3),FN UU(13),FN UU(3),FN CC(13,6))
4410     CALL FELL(CX,CY-FN UU(4),FN UU(9),FN UU(2),FN CC(0,2))
4420     GFX LINE CX-FN UU(5),CY-FN UU(20),CX-FN UU(10),CY-FN UU(6),FN CC(13,15)
4430   END IF
4440   IF K=12 THEN
4450     ' fuel cell: a cylinder
4460     CALL FELL(CX,CY-FN UU(2),FN UU(8),FN UU(3),FN CC(5,8))
4470     CALL RECTO(CX-FN UU(8),CY-FN UU(24),CX+FN UU(8),CY-FN UU(2),FN CC(5,11))
4480     CALL FELLO(CX,CY-FN UU(24),FN UU(8),FN UU(3),FN CC(15,13))
4490     GFX LINE CX-FN UU(3),CY-FN UU(20),CX-FN UU(3),CY-FN UU(6),FN CC(15,15)
4500   END IF
4510   IF K=13 THEN
4520     ' hull plate
4530     CALL RECTO(CX-FN UU(18),CY-FN UU(8),CX+FN UU(18),CY-FN UU(2),FN CC(13,9))
4540     CALL RECTO(CX-FN UU(16),CY-FN UU(14),CX+FN UU(16),CY-FN UU(8),FN CC(13,12))
4550     GFX PSET CX-FN UU(14),CY-FN UU(11),FN CC(0,3)
4560     GFX PSET CX+FN UU(14),CY-FN UU(11),FN CC(0,3)
4570     GFX PSET CX,CY-FN UU(11),FN CC(0,3)
4580   END IF
4590   IF K=14 THEN
4600     ' ignition glyph: a glowing token
4610     CALL FELL(CX,CY-FN UU(11),FN UU(13),FN UU(13),FN CC(15,5))
4620     CALL FELLO(CX,CY-FN UU(11),FN UU(8),FN UU(8),FN CC(15,12))
4630     GFX LINE CX-FN UU(4),CY-FN UU(8),CX,CY-FN UU(15),FN CC(0,2)
4640     GFX LINE CX,CY-FN UU(15),CX+FN UU(4),CY-FN UU(8),FN CC(0,2)
4650     GFX LINE CX-FN UU(2),CY-FN UU(11),CX+FN UU(2),CY-FN UU(11),FN CC(0,2)
4660   END IF
4670   IF K=15 THEN
4680     ' antenna: a rod with a dish
4690     GFX LINE CX,CY-FN UU(2),CX,CY-FN UU(26),FN CC(13,13)
4700     GFX LINE CX+1,CY-FN UU(2),CX+1,CY-FN UU(26),FN CC(13,8)
4710     CALL FELLO(CX,CY-FN UU(22),FN UU(10),FN UU(4),FN CC(13,12))
4720     GFX LINE CX-FN UU(10),CY-FN UU(14),CX+FN UU(10),CY-FN UU(14),FN CC(13,10)
4730     CALL FELL(CX,CY-FN UU(28),FN UU(2),FN UU(2),FN CC(2,11))
4740   END IF
4750   IF K=16 THEN
4760     ' chair
4770     GFX LINE CX-FN UU(12),CY-FN UU(1),CX-FN UU(12),CY-FN UU(17),FN CC(1,7)
4780     GFX LINE CX+FN UU(12),CY-FN UU(1),CX+FN UU(12),CY-FN UU(17),FN CC(1,7)
4790     CALL RECTO(CX-FN UU(14),CY-FN UU(22),CX+FN UU(14),CY-FN UU(16),FN CC(1,11))
4800     CALL RECTO(CX-FN UU(13),CY-FN UU(46),CX-FN UU(9),CY-FN UU(22),FN CC(1,9))
4810     CALL RECTO(CX+FN UU(9),CY-FN UU(46),CX+FN UU(13),CY-FN UU(22),FN CC(1,9))
4820     CALL RECTO(CX-FN UU(13),CY-FN UU(46),CX+FN UU(13),CY-FN UU(38),FN CC(1,10))
4830   END IF
4840   IF K=17 THEN
4850     ' framed photo
4860     CALL RECTO(CX-FN UU(10),CY-FN UU(24),CX+FN UU(10),CY-FN UU(2),FN CC(1,9))
4870     CALL RECTO(CX-FN UU(7),CY-FN UU(21),CX+FN UU(7),CY-FN UU(5),FN CC(7,12))
4880     CALL FELL(CX-FN UU(2),CY-FN UU(14),FN UU(2),FN UU(3),FN CC(6,11))
4890     CALL FELL(CX+FN UU(3),CY-FN UU(14),FN UU(2),FN UU(3),FN CC(5,11))
4900   END IF
4910   IF K=18 THEN
4920     ' paper, letter or book (HU = color)
4930     CALL RECTO(CX-FN UU(10),CY-FN UU(5),CX+FN UU(10),CY-FN UU(2),FN CC(HU,11))
4940     CALL RECTO(CX-FN UU(9),CY-FN UU(8),CX+FN UU(9),CY-FN UU(5),FN CC(11,14))
4950     GFX LINE CX-FN UU(7),CY-FN UU(7),CX+FN UU(7),CY-FN UU(7),FN CC(0,5)
4960   END IF
4970   IF K=19 THEN
4980     ' round thing: fruit, ball, bread, coin (HU = color)
4990     CALL FELLO(CX,CY-FN UU(9),FN UU(8),FN UU(8),FN CC(HU,11))
5000     GFX PSET CX-FN UU(3),CY-FN UU(12),FN CC(HU,15)
5010     GFX PSET CX-FN UU(2),CY-FN UU(12),FN CC(HU,15)
5020   END IF
5030   IF K=20 THEN
5040     ' cloth, folded (HU = color)
5050     CALL RECTO(CX-FN UU(11),CY-FN UU(9),CX+FN UU(11),CY-FN UU(2),FN CC(HU,11))
5060     CALL RECTO(CX-FN UU(9),CY-FN UU(14),CX+FN UU(9),CY-FN UU(9),FN CC(HU,13))
5070   END IF
5080   IF K=21 THEN
5090     ' cup, pan, can (HU = color)
5100     CALL RECTO(CX-FN UU(7),CY-FN UU(14),CX+FN UU(7),CY-FN UU(2),FN CC(HU,10))
5110     GFX LINE CX-FN UU(7),CY-FN UU(14),CX+FN UU(7),CY-FN UU(14),FN CC(0,15)
5120     GFX LINE CX+FN UU(7),CY-FN UU(11),CX+FN UU(11),CY-FN UU(11),1
5130   END IF
5140   IF K=22 THEN
5150     ' umbrella
5160     CALL TRAPH(CX,CY-FN UU(34),CY-FN UU(24),FN UU(2),FN UU(14),FN CC(HU,10))
5170     GFX LINE CX,CY-FN UU(24),CX,CY-FN UU(3),FN CC(0,4)
5180     GFX LINE CX,CY-FN UU(3),CX+FN UU(4),CY-FN UU(2),FN CC(0,4)
5190   END IF
5200   IF K=23 THEN
5210     ' hat
5220     CALL TRAPH(CX,CY-FN UU(22),CY-FN UU(6),FN UU(5),FN UU(9),FN CC(HU,9))
5230     CALL FELLO(CX,CY-FN UU(5),FN UU(14),FN UU(3),FN CC(HU,7))
5240   END IF
5250   IF K=24 THEN
5260     ' pillow
5270     CALL FELLO(CX,CY-FN UU(6),FN UU(13),FN UU(5),FN CC(0,14))
5280   END IF
5290   IF K=25 THEN
5300     ' bucket
5310     CALL TRAPH(CX,CY-FN UU(20),CY-FN UU(2),FN UU(11),FN UU(8),FN CC(1,9))
5320     GFX LINE CX-FN UU(11),CY-FN UU(20),CX,CY-FN UU(30),FN CC(13,9)
5330     GFX LINE CX+FN UU(11),CY-FN UU(20),CX,CY-FN UU(30),FN CC(13,9)
5340   END IF
5350 END PROC
5360 ' ------------------------------------------------------------------------------------------------ the message window (text rows 20..24)
5370 ' SAY adds a message (word-wrapped to 76 columns) to the log; FLUSH shows the last four lines in rows 20..23 (a message of more than four
5380 ' lines is shown in pages, "-- more --"). Colors: 1 normal, 2 Deqq's voice, 3 warning, 4 danger, 5 hint, 6 dim, 7 white.
5390 FUNC TCOL(C)
5400   RETURN 20+C
5410 END FUNC
5420 PROC LOGFILE(T$)
5430   IF LOGON THEN PRINT #3,T$
5440 END PROC
5450 PROC ADDLINE(T$,C)
5460   LOGT$(LOGN MOD 300)=T$
5470   LOGC(LOGN MOD 300)=C
5480   LOGN=LOGN+1
5490   CALL LOGFILE(T$)
5500   IF HL THEN PRINT T$
5510 END PROC
5520 PROC SAY(T$,C)
5530   LOCAL L$,P,Q
5540   L$=T$
5550   WHILE LEN(L$)>76
5560     P=77
5570     WHILE P>1 AND MID$(L$,P,1)<>" "
5580       P=P-1
5590     WEND
5600     IF P<=1 THEN P=77
5610     CALL ADDLINE(LEFT$(L$,P-1),C)
5620     L$=MID$(L$,P)
5630     WHILE LEFT$(L$,1)=" "
5640       L$=MID$(L$,2)
5650     WEND
5660   WEND
5670   CALL ADDLINE(L$,C)
5680 END PROC
5690 PROC TXTROW(R,T$,C)
5700   IF HL THEN EXIT PROC
5710   LOCATE 0,20+R
5720   COLOR FN TCOL(C),20
5730   PRINT " ";LEFT$(T$+SPACE$(78),78);
5740 END PROC
5750 ' shows four log lines ending at log entry E (inclusive); M = 1: the last row says "-- more --"
5760 PROC SHOWTAIL(E,M)
5770   LOCAL R,I
5780   IF HL THEN EXIT PROC
5790   FOR R=0 TO 3
5800     I=E-3+R
5810     IF I>=0 AND I>=LOGN-300 THEN
5820       CALL TXTROW(R,LOGT$(I MOD 300),LOGC(I MOD 300))
5830     ELSE
5840       CALL TXTROW(R,"",1)
5850     END IF
5860   NEXT R
5870   IF M THEN CALL TXTROW(3,"-- more --  (press a key)",3)
5880 END PROC
5890 PROC WAITKEY
5900   LOCAL K$
5910   IF HL OR PLAYON THEN EXIT PROC
5920   REPEAT
5930     K$=INKEY$
5940     IF K$="" THEN WAIT 2
5950   UNTIL K$<>""
5960 END PROC
5970 PROC FLUSH
5980   LOCAL PS
5990   IF HL THEN
6000     PSTART=LOGN
6010     EXIT PROC
6020   END IF
6030   PS=PSTART
6040   WHILE LOGN-PS>4
6050     CALL SHOWTAIL(PS+2,1)
6060     CALL WAITKEY
6070     PS=PS+3
6080   WEND
6090   CALL SHOWTAIL(LOGN-1,0)
6100   PSTART=LOGN
6110 END PROC
6120 ' the text screen as a whole: a dark band for the message window; the rest of the text layer stays empty
6130 PROC TEXTINIT
6140   IF HL THEN EXIT PROC
6150   CALL TEXTPAL
6160   COLOR 21,0
6170   CLS
6180   TXT FILL 0,20,79,24,32,21,20
6190   LOCATE 0,0
6200 END PROC
6210 ' ------------------------------------------------------------------------------------------------ the input line
6220 PROC PROMPT(B$)
6230   IF HL THEN EXIT PROC
6240   LOCATE 0,24
6250   COLOR FN TCOL(7),20
6260   PRINT "> ";LEFT$(B$+"_"+SPACE$(76),77);
6270 END PROC
6280 ' the log on the whole screen (page up): the scene is hidden while it is shown
6290 PROC HISTORY
6300   LOCAL R,I,N
6310   IF HL OR PLAYON THEN EXIT PROC
6320   GFX LAYER FR,OFF
6330   TXT FILL 0,0,79,19,32,21,20
6340   N=FN MINI(23,LOGN)
6350   FOR R=0 TO N-1
6360     I=LOGN-N+R
6370     LOCATE 0,R
6380     COLOR FN TCOL(LOGC(I MOD 300)),20
6390     PRINT " ";LEFT$(LOGT$(I MOD 300)+SPACE$(78),78);
6400   NEXT R
6410   CALL WAITKEY
6420   TXT FILL 0,0,79,19,32,21,0
6430   GFX LAYER FR,ON
6440   CALL SHOWTAIL(LOGN-1,0)
6450 END PROC
6460 ' one line from the player (or from the play-back file when testing)
6470 FUNC GETLINE$
6480   LOCAL B$,K$,C,H,DN
6490   IF PLAYON THEN
6500     IF EOF(4) THEN
6510       PLAYEND=-1
6520       RETURN "quit"
6530     END IF
6540     LINE INPUT #4,B$
6550     RETURN B$
6560   END IF
6570   IF HL THEN
6580     LINE INPUT B$
6590     RETURN B$
6600   END IF
6610   B$=""
6620   H=NH
6630   DN=0
6640   CALL PROMPT(B$)
6650   REPEAT
6660     K$=INKEY$
6670     IF K$="" THEN
6680       WAIT 2
6690     ELSE
6700       IF K$=CHR$(27) THEN
6710         B$="quit"
6720         DN=-1
6730       ELSE
6740       IF K$=CHR$(13) OR K$=CHR$(10) THEN
6750         DN=-1
6760       ELSE
6770         IF K$=CHR$(8) OR K$=CHR$(127) THEN
6780           IF LEN(B$)>0 THEN B$=LEFT$(B$,LEN(B$)-1)
6790         ELSE
6800           IF K$=CHR$(27)+"[A" THEN
6810             IF H>0 THEN
6820               H=H-1
6830               B$=HIST$(H)
6840             END IF
6850           ELSE
6860             IF K$=CHR$(27)+"[B" THEN
6870               IF H<NH-1 THEN
6880                 H=H+1
6890                 B$=HIST$(H)
6900               ELSE
6910                 H=NH
6920                 B$=""
6930               END IF
6940             ELSE
6950               IF K$=CHR$(27)+"[5~" THEN
6960                 CALL HISTORY
6970               ELSE
6980                 IF LEN(K$)=1 AND LEN(B$)<70 AND ASC(K$)>=32 THEN B$=B$+K$
6990               END IF
7000             END IF
7010           END IF
7020         END IF
7030       END IF
7040       END IF
7050       CALL PROMPT(B$)
7060     END IF
7070   UNTIL DN
7080   IF B$<>"" THEN
7090     IF NH>=20 THEN
7100       FOR C=1 TO 19
7110         HIST$(C-1)=HIST$(C)
7120       NEXT C
7130       NH=19
7140     END IF
7150     HIST$(NH)=B$
7160     NH=NH+1
7170   END IF
7180   RETURN B$
7190 END FUNC
7200 ' ------------------------------------------------------------------------------------------------ the world: rooms and things (DATA)
7210 ' Rooms:   DATA key, name, description, exits ("W:WESTG N:WELLG IN:HALL": the directions N S E W U D IN OUT and the room they lead to)
7220 ' Things:  DATA key, name, words, size, flags, room, parent, relation, icon, hue, description
7230 '   size 0 = fixed (can't be taken), 1.. = takes that many of the 6 inventory slots.   relation of a thing inside / on / under its parent: 1 / 2 / 3
7240 '   flags: C = container that opens and closes, A = container that is always open, S = surface (things lie on it), U = things can lie under it,
7250 '          H = a high surface (reach it only standing on a chair), P = a part of the spaceship, X = a place to hide, Z = hidden until it is revealed, O = starts open
7260 LABEL ROOMDATA
7270 DATA "FRONT","Front garden","You stand on the gravel road in front of Deqq's house, a crooked grey building with one round window and a front door in a color that has no name. A signpost leans by the gate. The road runs around the house: west and north.","W:WESTG N:WELLG IN:HALL"
7280 DATA "WESTG","West side of the house","A bare strip of garden along the west wall. A rusty hatch is set into the ground against the wall, a rain barrel and a rotting bench stand by it. The road goes east and north.","E:FRONT N:BACKYD D:CELLAR IN:CELLAR"
7290 DATA "BACKYD","Back yard","Behind the house: a back door, a washing line with grey capes, and a big flowerpot. The road goes south and east. A trampled track leads north into the dark, toward a smell of scorched metal.","S:WESTG E:WELLG N:CRASH IN:KITCHEN"
7300 DATA "WELLG","North-east corner","An old stone well with a windlass stands here, and an apple tree that bears odd fruit. East of the well is a small tool shed. The road runs west and south.","W:BACKYD S:FRONT E:SHED IN:SHED"
7310 DATA "SHED","Tool shed","A cluttered shed that smells of oil. A big blueprint is pinned over the workbench, a red toolbox stands under it and a shelf holds old cans.","W:WELLG OUT:WELLG"
7320 DATA "CRASH","Crash site","A scorched hollow in the ground. In its middle lies the wreck of a spaceship: a skeleton of beams with eight gaps where parts are missing. The only way back is south.","S:BACKYD"
7330 DATA "HALL","Hall","A narrow hall with a creaking floor and a wall clock. The front door is to the south, stairs lead up, a coat closet and a spindly table stand by the wall. Doors go east and west.","E:LIVING W:KITCHEN U:LANDING S:FRONT OUT:FRONT"
7340 DATA "LIVING","Living room","A sagging sofa, a round table, a bookshelf, a television full of static and a very tall cupboard fill the room. The only door leads west, back to the hall.","W:HALL OUT:HALL"
7350 DATA "KITCHEN","Kitchen","A kitchen: fridge, oven, sink, a table with chairs and a pantry. A back door leads north, stone steps go down into the cellar, and the hall is to the east.","E:HALL N:BACKYD OUT:BACKYD D:CELLAR"
7360 DATA "LANDING","Landing","A dim landing. A door to the east, a door to the west, the stairs lead down, and a ladder goes up to the attic. A painting hangs crooked on the wall.","D:HALL E:BEDROOM W:BATH U:ATTIC"
7370 DATA "BEDROOM","Bedroom","A bedroom with a huge bed, a wardrobe, a nightstand and a toy chest. Something under the bed is breathing. The door is to the west.","W:LANDING OUT:LANDING"
7380 DATA "BATH","Bathroom","A bathroom in sickly green tiles: a bathtub with a plastic curtain, a washbasin, a medicine cabinet and a mirror made of something dull. The door is to the east.","E:LANDING OUT:LANDING"
7390 DATA "CELLAR","Cellar","Damp stone walls. In the corner a washing machine rattles, there are barrels, a dusty shelf and a fuse box. Steps lead up to the kitchen, and a ladder to a hatch in the west garden.","U:KITCHEN OUT:WESTG"
7400 DATA "ATTIC","Attic","Under the sloping roof: old boxes, a trunk, a telescope at the skylight, a star globe and a safe set into the chimney wall. The ladder leads down.","D:LANDING"
7410 DATA "END","","",""
7420 LABEL OBJDATA
7430 DATA "FRONTDOORO","the front door"," FRONT DOOR ENTRANCE ",0,"","FRONT","",0,0,0,"The front door."
7440 DATA "MAILBOX","a mailbox"," MAILBOX MAIL BOX LETTERBOX POSTBOX ",0,"C","FRONT","",0,0,0,"A dented mailbox in the shape of a small rocket."
7450 DATA "LETTER","a letter"," LETTER ENVELOPE MAIL ",1,"","","MAILBOX",1,18,11,"An envelope for 'The Resident'. Inside it says: FINAL NOTICE. Your orbit permit has expired. Pay within 3 moons."
7460 DATA "GNOME","a garden gnome"," GNOME STATUE FIGURE ",0,"","FRONT","",0,0,0,"A garden gnome with a red cap. Its painted eyes seem to follow you around the garden. You are almost sure they did not do that before."
7470 DATA "SIGN","a signpost"," SIGN SIGNPOST BOARD ",0,"","FRONT","",0,0,0,"Hand-painted: DEQQ - KEEP OUT. (THIS MEANS YOU.) Below, smaller: Spare key? Look where the plants sit crooked."
7480 DATA "HATCHO","the cellar hatch"," HATCH TRAPDOOR PADLOCK ",0,"","WESTG","",0,0,0,"The hatch."
7490 DATA "BARREL","a rain barrel"," BARREL RAIN WATER ",0,"A","WESTG","",0,0,0,"A rain barrel full of greenish water. Something small swims in it. Probably."
7500 DATA "BENCH","a rotting bench"," BENCH SEAT ",0,"S","WESTG","",0,0,0,"A garden bench with more holes than slats."
7510 DATA "TEACUP","a teacup"," TEACUP CUP MUG ",1,"","","BENCH",2,21,0,"A chipped teacup. The tea in it is still steaming, though nobody has been here for days."
7520 DATA "BACKDOORO","the back door"," BACK DOOR ",0,"","BACKYD","",0,0,0,"The back door."
7530 DATA "POT","a flowerpot"," POT FLOWERPOT PLANT FLOWER ",0,"U","BACKYD","",0,0,0,"A big clay flowerpot with a withered plant. It sits crooked, as if it had been lifted many times."
7540 DATA "KEY","a spare key"," KEY SPARE BRASS ",1,"Z","","POT",3,1,0,"A brass key on a string. The tag says: SHED, BACK DOOR, HATCH. Deqq trusts nobody, not even himself."
7550 DATA "WASHLINE","a washing line"," LINE WASHING CLOTHESLINE CAPES CAPE ",0,"","BACKYD","",0,0,0,"A washing line with three identical grey capes. They move, although there is no wind."
7560 DATA "WELL","the old well"," WELL SHAFT WINDLASS ROPE WINCH CRANK ",0,"","WELLG","",0,0,0,"The well."
7570 DATA "BUCKET","an iron bucket"," BUCKET PAIL ",0,"AZ","WELLG","",0,0,0,"A rusty iron bucket on the well's rope."
7580 DATA "ANTENNA","an antenna"," ANTENNA AERIAL ROD DISH ",1,"P","","BUCKET",1,15,0,"A telescopic antenna with a small dish. One of the eight parts of Deqq's ship: the antenna."
7590 DATA "TREE","an old tree"," TREE BRANCH BRANCHES ",0,"S","WELLG","",0,0,0,"A gnarled tree. Its fruit are not apples. They hum quietly."
7600 DATA "FRUIT","a glowing fruit"," FRUIT APPLE ORANGE ",1,"","","TREE",2,19,3,"A fruit the size of an apple that glows orange from inside. It hums a little. It smells like cinnamon and thunder."
7610 DATA "SHEDDOORO","the shed door"," SHED DOOR PADLOCK ",0,"","WELLG","",0,0,0,"The shed door."
7620 DATA "TOOLBOX","a toolbox"," TOOLBOX BOX TOOLS ",0,"C","SHED","",0,0,0,"A big red toolbox, dented in several places."
7630 DATA "CRANK","a crank handle"," CRANK HANDLE ",1,"","","TOOLBOX",1,2,0,"A rusty crank handle with a red knob. It looks as if it would fit a windlass."
7640 DATA "SCREWDRIVER","a screwdriver"," SCREWDRIVER DRIVER ",1,"","","TOOLBOX",1,3,2,"A screwdriver with a strange four-pointed tip. For alien screws, presumably."
7650 DATA "HAMMER","a hammer"," HAMMER MALLET ",1,"","","TOOLBOX",1,3,1,"A heavy hammer. Its head is warm for no reason."
7660 DATA "SHELF","a shelf"," SHELF SHELVES RACK ",0,"S","SHED","",0,0,0,"A wobbly shelf of old cans."
7670 DATA "PAINT","a can of paint"," CAN PAINT TIN ",1,"","","SHELF",2,21,6,"A can of paint labelled CAMOUFLAGE - LOOKS LIKE NOTHING. The lid is stuck fast."
7680 DATA "BENCHW","a workbench"," WORKBENCH VICE BENCH ",0,"S","SHED","",0,0,0,"A workbench covered in half-finished gadgets that do nothing at all."
7690 DATA "BLUEPRINT","a blueprint"," BLUEPRINT DRAWING PLAN POSTER PLANS ",0,"","SHED","",0,0,0,"A big drawing of a small spaceship. Eight parts are circled and named: FLUX COIL, STAR COMPASS, WARP CRYSTAL, THRUSTER NOZZLE, FUEL CELL, HULL PLATE, IGNITION GLYPH, ANTENNA. A note: 'Build at the wreck. All eight. Then go home.'"
7700 DATA "SHIP","the wreck"," SHIP WRECK SPACECRAFT SPACESHIP CRAFT ROCKET BEAMS ",0,"","CRASH","",0,0,0,"The wreck."
7710 DATA "HALLDOOR","the front door"," FRONT DOOR ENTRANCE ",0,"","HALL","",0,0,0,"The front door."
7720 DATA "HALLTABLE","a spindly table"," TABLE SPINDLY ",0,"S","HALL","",0,0,0,"A spindly table by the door. One leg is shorter than the others and propped on a folded paper."
7730 DATA "LAMP","a lantern"," LAMP LANTERN LIGHT TORCH FLASHLIGHT ",1,"","","HALLTABLE",2,4,4,"A lantern with a small glass chimney and a switch on its base."
7740 DATA "CLOSET","a coat closet"," CLOSET COATS CUPBOARD ",0,"CX","HALL","",0,0,0,"A narrow closet full of identical grey capes. Plenty of room behind them to stand very still."
7750 DATA "UMBRELLA","an umbrella"," UMBRELLA BROLLY ",1,"","","CLOSET",1,22,2,"A black umbrella with a hook handle and a faint smell of ozone."
7760 DATA "CLOCK","a wall clock"," CLOCK WATCH ",0,"","HALL","",0,0,0,"A wall clock with eleven hours and no minute hand. It says: a bit later."
7770 DATA "STAIRSH","the stairs"," STAIRS STAIRCASE STEPS ",0,"","HALL","",0,0,0,"Wooden stairs lead up to the landing. The third step creaks. You know because you can hear it from here."
7780 DATA "SOFA","a sofa"," SOFA COUCH SETTEE ",0,"XU","LIVING","",0,0,0,"A saggy sofa with a pattern that changes when you look away. Behind it there is room enough to hide."
7790 DATA "COIN","a strange coin"," COIN MONEY ",1,"Z","","SOFA",3,19,4,"A coin with a hole in the middle and another hole at the edge."
7800 DATA "CUPBOARD","a tall cupboard"," CUPBOARD CABINET DRESSER SIDEBOARD ",0,"CH","LIVING","",0,0,0,"A very tall cupboard of dark wood, much taller than a person. The top is out of reach."
7810 DATA "CAPE","a grey cape"," CAPE CLOAK ",1,"","","CUPBOARD",1,20,0,"A grey cape. They all look the same. This one smells slightly of Deqq."
7820 DATA "CRYSTAL","a warp crystal"," CRYSTAL WARP GEM STONE ",1,"P","","CUPBOARD",2,10,9,"A fist-sized crystal that glows violet. The air around it ripples. One of the eight parts of Deqq's ship: the warp crystal."
7830 DATA "LTABLE","a round table"," TABLE ROUND ",0,"S","LIVING","",0,0,0,"A round table with a lace cloth. It's set for one. One what is unclear."
7840 DATA "SANDWICH","a sandwich"," SANDWICH SNACK ",1,"","","LTABLE",2,19,3,"A sandwich of uncertain filling. The filling moves when you look away."
7850 DATA "BOOKSHELF","a bookshelf"," BOOKSHELF SHELF SHELVES BOOKS ",0,"S","LIVING","",0,0,0,"A bookshelf of crooked books in many alphabets."
7860 DATA "PHOTO","a framed photo"," PHOTO PHOTOGRAPH FRAME PICTURE ",1,"","","BOOKSHELF",2,17,0,"A framed photo of three tall figures in front of a silver ship, waving. The frame is old and heavy; the back is held by little clips."
7870 DATA "BOOK","a thick book"," BOOK COOKBOOK ",1,"","","BOOKSHELF",2,18,2,"THE JOY OF GLORPS. Chapter 4: 'Feeding. A glorp is loyal to whoever feeds it green jelly. Never give it anything else. Never.'"
7880 DATA "TV","a television"," TV TELEVISION SCREEN ",0,"","LIVING","",0,0,0,"A television showing static. Now and then the static forms a face. It looks bored."
7890 DATA "FRIDGE","the fridge"," FRIDGE REFRIGERATOR ICEBOX FREEZER ",0,"C","KITCHEN","",0,0,0,"A tall fridge that hums in two keys at once."
7900 DATA "MILK","a bottle of milk"," MILK BOTTLE ",1,"","","FRIDGE",1,6,0,"A bottle of blue milk. Not from cows, and probably not from anything on this planet."
7910 DATA "JELLY","green jelly"," JELLY JAR GREEN ",1,"","","FRIDGE",1,5,5,"A jar of wobbly green jelly. It wobbles when nothing touches it. The label: GLORP SNACK. Handle with respect."
7920 DATA "ICE","a block of ice"," ICE BLOCK ICECUBE CUBE ",1,"","","FRIDGE",1,7,0,"A block of ice as big as a loaf of bread. Something small and metallic is frozen inside it."
7930 DATA "COMPASS","a star compass"," COMPASS STAR NEEDLE ",1,"P","","ICE",1,8,0,"A brass compass whose needle points at a star that is not in this sky. One of the eight parts of Deqq's ship: the star compass."
7940 DATA "OVEN","the oven"," OVEN STOVE COOKER KNOB ",0,"C","KITCHEN","",0,0,0,"The oven."
7950 DATA "COIL","a flux coil"," COIL FLUX COPPER ",1,"P","","OVEN",1,9,0,"A coil of thick copper wire around a glowing core. One of the eight parts of Deqq's ship: the flux coil."
7960 DATA "SINK","the sink"," SINK BASIN ",0,"A","KITCHEN","",0,0,0,"A deep stone sink."
7970 DATA "TAP","the tap"," TAP FAUCET WATER ",0,"","KITCHEN","",0,0,0,"The tap."
7980 DATA "KTABLE","a kitchen table"," TABLE KITCHEN ",0,"S","KITCHEN","",0,0,0,"A scrubbed wooden table with scorch marks in the shape of a hand."
7990 DATA "BREAD","a loaf of bread"," BREAD LOAF ",1,"","","KTABLE",2,19,11,"A loaf of bread, very hard. It has been here since before the crash."
8000 DATA "CHAIR","a wooden chair"," CHAIR STOOL ",2,"","KITCHEN","",0,16,0,"A sturdy wooden chair. Heavy, but you can carry it, if you have two free slots. You could stand on it."
8010 DATA "KCUPBOARD","a kitchen cupboard"," CUPBOARD CABINET ",0,"C","KITCHEN","",0,0,0,"A low kitchen cupboard with a sticking door."
8020 DATA "PAN","a frying pan"," PAN FRYINGPAN ",1,"","","KCUPBOARD",1,21,13,"A heavy frying pan. It has a dent the shape of a head."
8030 DATA "PANTRY","a pantry"," PANTRY LARDER ",0,"CX","KITCHEN","",0,0,0,"A narrow pantry with shelves of tins. Behind the sacks there is just room for a person."
8040 DATA "FLOUR","a bag of flour"," FLOUR BAG SACK ",1,"","","PANTRY",1,20,11,"A bag of flour. Or something white and powdery that behaves like flour."
8050 DATA "BACKDOORI","the back door"," BACK DOOR ",0,"","KITCHEN","",0,0,0,"The back door."
8060 DATA "CSTAIRS","the cellar steps"," STEPS STAIRS CELLAR ",0,"","KITCHEN","",0,0,0,"Stone steps lead down into the dark."
8070 DATA "LINEN","a linen chest"," CHEST LINEN BASKET ",0,"CX","LANDING","",0,0,0,"A big linen chest. There is room for a person inside, if that person is thin and unhappy."
8080 DATA "SHEET","a bed sheet"," SHEET LINEN ",1,"","","LINEN",1,20,0,"A bed sheet with a hole in it. The hole is the shape of a ghost."
8090 DATA "LADDER","the attic ladder"," LADDER ",0,"","LANDING","",0,0,0,"A wooden ladder leads up to the attic."
8100 DATA "PICTURE","a painting"," PAINTING PICTURE ART ",0,"","LANDING","",0,0,0,"A painting of this house. In the painting the house has one more window than the real one. The painted window is lit."
8110 DATA "BED","a big bed"," BED MATTRESS ",0,"SU","BEDROOM","",0,0,0,"A huge bed with a blanket in a color that hurts a little."
8120 DATA "PILLOW","a pillow"," PILLOW CUSHION ",1,"","","BED",2,24,0,"A star-shaped pillow. It's damp. From what, you'd rather not know."
8130 DATA "GLORP","a glorp"," GLORP CREATURE BLOB PET MONSTER ",0,"Z","","BED",3,0,0,"A glorp."
8140 DATA "NOZZLE","a thruster nozzle"," NOZZLE THRUSTER ENGINE BELL ",2,"PZ","","BED",3,11,0,"A bell-shaped nozzle, scorched black inside. One of the eight parts of Deqq's ship: the thruster nozzle. It is heavy: it takes two slots."
8150 DATA "WARDROBE","a wardrobe"," WARDROBE CLOSET ",0,"CX","BEDROOM","",0,0,0,"A dark wardrobe full of grey capes. There is room to stand between them."
8160 DATA "HAT","a top hat"," HAT TOPHAT ",1,"","","WARDROBE",1,23,9,"A purple top hat. Something inside it purrs."
8170 DATA "NIGHTSTAND","a nightstand"," NIGHTSTAND STAND DRAWER ",0,"C","BEDROOM","",0,0,0,"A little nightstand with a drawer."
8180 DATA "NOTE","a crumpled note"," NOTE PAPER LIST ",1,"","","NIGHTSTAND",1,18,11,"A shopping list in curly writing: GREEN JELLY (for the glorp - it eats nothing else!), BLUE MILK, 3 BATTERIES, PATIENCE."
8190 DATA "TOYCHEST","a toy chest"," TOYCHEST CHEST TOYS ",0,"C","BEDROOM","",0,0,0,"A wooden toy chest painted with stars."
8200 DATA "TOYROCKET","a toy rocket"," ROCKET TOY TOYROCKET ",1,"","","TOYCHEST",1,19,2,"A tin toy rocket. Disappointingly NOT one of the eight parts. Wind it up and it plays a tune."
8210 DATA "BALL","a rubber ball"," BALL RUBBER ",1,"","","TOYCHEST",1,19,2,"A rubber ball. It bounces a little too high."
8220 DATA "TUB","a bathtub"," TUB BATHTUB CURTAIN SHOWER ",0,"X","BATH","",0,0,0,"A bathtub with a plastic curtain. The curtain would hide a person nicely."
8230 DATA "MIRROR","a mirror"," MIRROR GLASS SCREWS SCREW ",0,"","BATH","",0,0,0,"The mirror."
8240 DATA "PLATE","a hull plate"," PLATE HULL METAL PANEL ",2,"PZ","BATH","",0,13,0,"A curved metal plate, polished like a mirror on one side and burnt on the other. One of the eight parts of Deqq's ship: the hull plate. It is big: it takes two slots."
8250 DATA "BSINK","a washbasin"," BASIN WASHBASIN SINK ",0,"A","BATH","",0,0,0,"A green washbasin."
8260 DATA "SOAP","a bar of soap"," SOAP BAR ",1,"","","BSINK",1,19,14,"A bar of soap that smells of lime and mild disappointment."
8270 DATA "CABINET","a medicine cabinet"," CABINET MEDICINE ",0,"C","BATH","",0,0,0,"A medicine cabinet over the basin."
8280 DATA "PILLS","some pills"," PILLS BOTTLE ",1,"","","CABINET",1,6,9,"A bottle of purple pills. The label: DO NOT. That is all it says."
8290 DATA "TOWEL","a towel"," TOWEL ",1,"","BATH","",0,20,8,"A blue towel. It's still damp. Tick. Tock."
8300 DATA "WASHER","the washing machine"," WASHER WASHING MACHINE LAUNDRY DRUM ",0,"C","CELLAR","",0,0,0,"The washing machine."
8310 DATA "FUELCELL","a fuel cell"," CELL FUEL BATTERY CYLINDER ",1,"P","","WASHER",1,12,5,"A glowing green cylinder, warm and humming. One of the eight parts of Deqq's ship: the fuel cell."
8320 DATA "SOCKS","some wet socks"," SOCKS SOCK ",1,"","","WASHER",1,20,5,"Three wet socks. They are all left ones."
8330 DATA "PLUG","the plug"," PLUG CABLE CORD SOCKET ",0,"","CELLAR","",0,0,0,"The plug."
8340 DATA "CBARRELS","some barrels"," BARRELS BARREL CASKS ",0,"X","CELLAR","",0,0,0,"A few old barrels. Between them and the wall there is room to hide."
8350 DATA "CSHELF","a dusty shelf"," SHELF SHELVES ",0,"S","CELLAR","",0,0,0,"A dusty shelf."
8360 DATA "PICKLES","a jar of pickles"," PICKLES JAR ",1,"","","CSHELF",2,5,14,"A jar of pickles. The pickles are looking back."
8370 DATA "FUSEBOX","a fuse box"," FUSEBOX FUSE FUSES SWITCH ",0,"","CELLAR","",0,0,0,"A fuse box with fuses labelled in dots. One of them says DO NOT. You don't."
8380 DATA "HATCHI","the hatch"," HATCH LADDER ",0,"","CELLAR","",0,0,0,"The hatch."
8390 DATA "SAFE","a safe"," SAFE VAULT DIAL ",0,"C","ATTIC","",0,0,0,"The safe."
8400 DATA "GLYPH","an ignition glyph"," GLYPH IGNITION TOKEN ",1,"P","","SAFE",1,14,0,"A palm-sized token with a glowing symbol. One of the eight parts of Deqq's ship: the ignition glyph."
8410 DATA "TRUNK","an old trunk"," TRUNK CHEST ",0,"CX","ATTIC","",0,0,0,"An old trunk with brass corners, big enough to hide in."
8420 DATA "SCARF","a moth-eaten scarf"," SCARF ",1,"","","TRUNK",1,20,3,"A scarf, eaten by moths. Large ones: the holes are the size of coins."
8430 DATA "TELESCOPE","a telescope"," TELESCOPE SCOPE ",0,"","ATTIC","",0,0,0,"A brass telescope aimed at one particular star. Through it you see a small blue light, far away, blinking. It's a light of home."
8440 DATA "GLOBE","a star globe"," GLOBE MAP STARS HOLOGRAM ",0,"","ATTIC","",0,0,0,"A hologram globe of stars. One is marked with a tiny house. Another with a tiny question mark. The question mark seems to be looking at you."
8450 DATA "DEQQ","Deqq"," DEQQ ALIEN HOST OWNER ",0,"","","",0,0,0,"Deqq."
8460 DATA "END","","",0,"","","",0,0,0,""
8470 ' how Deqq talks (the translator box), what he mutters and the creepy remarks
8480 LABEL CMTDATA
8490 DATA "You are being watched by an otherworldly presence."
8500 DATA "Somewhere, something breathes in time with you."
8510 DATA "The shadows in the corner lean a little closer."
8520 DATA "Deqq says nothing. His eyes say a great deal."
8530 DATA "The translator crackles: 'Careful. The house remembers where things go.'"
8540 DATA "You feel a stare on the back of your neck, from a long way up."
8550 DATA "A cold finger of thought touches your mind and goes away again."
8560 DATA "Deqq tilts his head the wrong way. Then the right way."
8570 DATA "The floor creaks behind you. Only Deqq is standing there. Nobody else."
8580 DATA "The translator hums: 'Take your time. I have all of mine.'"
8590 DATA "Deqq blinks. It takes much longer than a blink should."
8600 DATA "'Interesting,' says a voice in your head. It isn't yours."
8610 DATA "END"
8620 ' ------------------------------------------------------------------------------------------------ reading the world
8630 FUNC OID(K$)
8640   LOCAL I
8650   FOR I=1 TO NOBJ
8660     IF OKEY$(I)=K$ THEN RETURN I
8670   NEXT I
8680   PRINT "Internal error: no thing called ";K$
8690   RETURN 0
8700 END FUNC
8710 FUNC RID(K$)
8720   LOCAL I
8730   FOR I=1 TO NROOM
8740     IF RKEY$(I)=K$ THEN RETURN I
8750   NEXT I
8760   IF K$<>"" THEN PRINT "Internal error: no room called ";K$
8770   RETURN 0
8780 END FUNC
8790 ' direction number of a name: N S E W U D IN OUT = 1..8
8800 FUNC DIRNUM(D$)
8810   LOCAL I
8820   FOR I=1 TO 8
8830     IF D$=DN$(I) THEN RETURN I
8840   NEXT I
8850   RETURN 0
8860 END FUNC
8870 ' puts thing I on one of the six places on the floor of room R in front of the furniture
8880 PROC FLOORSPOT(I,R)
8890   LOCAL N
8900   N=FC(R) MOD 6
8910   FC(R)=FC(R)+1
8920   OFX(I)=SLX(N):OFZ(I)=SLZ(N)
8930 END PROC
8940 PROC LOADWORLD
8950   LOCAL K$,N$,D$,E$,WD$,SZ,FL$,RK$,PK$,RL,IC,HU,I,J,P,T$,Q$
8960   DN$(1)="N":DN$(2)="S":DN$(3)="E":DN$(4)="W":DN$(5)="U":DN$(6)="D":DN$(7)="IN":DN$(8)="OUT"
8970   SLX(0)=110:SLX(1)=240:SLX(2)=370:SLX(3)=500:SLX(4)=170:SLX(5)=430
8980   SLZ(0)=60:SLZ(1)=70:SLZ(2)=60:SLZ(3)=70:SLZ(4)=130:SLZ(5)=130
8990   RESTORE ROOMDATA
9000   NROOM=0
9010   READ K$,N$,D$,E$
9020   WHILE K$<>"END"
9030     NROOM=NROOM+1
9040     RKEY$(NROOM)=K$:RNAME$(NROOM)=N$:RDESC$(NROOM)=D$:REX$(NROOM)=E$
9050     READ K$,N$,D$,E$
9060   WEND
9070   FOR I=1 TO NROOM
9080     E$=REX$(I)+" "
9090     WHILE LEN(E$)>1
9100       P=FN SPOS(E$," ")
9110       T$=LEFT$(E$,P-1)
9120       E$=MID$(E$,P+1)
9130       Q$=T$
9140       J=FN SPOS(Q$,":")
9150       IF J>0 THEN EX(I,FN DIRNUM(LEFT$(Q$,J-1)))=FN RID(MID$(Q$,J+1))
9160     WEND
9170   NEXT I
9180   RESTORE OBJDATA
9190   NOBJ=0
9200   READ K$,N$,WD$,SZ,FL$,RK$,PK$,RL,IC,HU,D$
9210   WHILE K$<>"END"
9220     NOBJ=NOBJ+1
9230     OKEY$(NOBJ)=K$:ONAME$(NOBJ)=N$:OWORDS$(NOBJ)=WD$:OSIZE(NOBJ)=SZ:OFL$(NOBJ)=FL$
9240     OPK$(NOBJ)=PK$:OREL(NOBJ)=RL:OICON(NOBJ)=IC:OCOL(NOBJ)=HU:ODESC$(NOBJ)=D$
9250     OROOM(NOBJ)=FN RID(RK$)
9260     OHID(NOBJ)=0
9270     IF FN SPOS(FL$,"Z")>0 THEN OHID(NOBJ)=-1
9280     OOPEN(NOBJ)=0
9290     IF FN SPOS(FL$,"O")>0 THEN OOPEN(NOBJ)=-1
9300     READ K$,N$,WD$,SZ,FL$,RK$,PK$,RL,IC,HU,D$
9310   WEND
9320   FOR I=1 TO NOBJ
9330     IF OPK$(I)<>"" THEN
9340       OIN(I)=FN OID(OPK$(I))
9350       OROOM(I)=0
9360     ELSE
9370       OIN(I)=0
9380       IF OROOM(I)>0 AND OSIZE(I)>0 THEN CALL FLOORSPOT(I,OROOM(I))
9390     END IF
9400   NEXT I
9410   RESTORE VERBDATA
9420   NVERB=0
9430   READ K$,P
9440   WHILE K$<>"END"
9450     NVERB=NVERB+1
9460     VW$(NVERB)=K$:VID(NVERB)=P
9470     READ K$,P
9480   WEND
9490   RESTORE CMTDATA
9500   NCMT=0
9510   READ K$
9520   WHILE K$<>"END"
9530     CMT$(NCMT)=K$
9540     NCMT=NCMT+1
9550     READ K$
9560   WEND
9570   ' the constants: R_<room> and K_<thing>
9580   R_FRONT=FN RID("FRONT")
9590   R_WESTG=FN RID("WESTG")
9600   R_BACKYD=FN RID("BACKYD")
9610   R_WELLG=FN RID("WELLG")
9620   R_SHED=FN RID("SHED")
9630   R_CRASH=FN RID("CRASH")
9640   R_HALL=FN RID("HALL")
9650   R_LIVING=FN RID("LIVING")
9660   R_KITCHEN=FN RID("KITCHEN")
9670   R_LANDING=FN RID("LANDING")
9680   R_BEDROOM=FN RID("BEDROOM")
9690   R_BATH=FN RID("BATH")
9700   R_CELLAR=FN RID("CELLAR")
9710   R_ATTIC=FN RID("ATTIC")
9720   K_FRONTDOORO=FN OID("FRONTDOORO")
9730   K_MAILBOX=FN OID("MAILBOX")
9740   K_LETTER=FN OID("LETTER")
9750   K_GNOME=FN OID("GNOME")
9760   K_SIGN=FN OID("SIGN")
9770   K_HATCHO=FN OID("HATCHO")
9780   K_BARREL=FN OID("BARREL")
9790   K_BENCH=FN OID("BENCH")
9800   K_TEACUP=FN OID("TEACUP")
9810   K_BACKDOORO=FN OID("BACKDOORO")
9820   K_POT=FN OID("POT")
9830   K_KEY=FN OID("KEY")
9840   K_WASHLINE=FN OID("WASHLINE")
9850   K_WELL=FN OID("WELL")
9860   K_BUCKET=FN OID("BUCKET")
9870   K_ANTENNA=FN OID("ANTENNA")
9880   K_TREE=FN OID("TREE")
9890   K_FRUIT=FN OID("FRUIT")
9900   K_SHEDDOORO=FN OID("SHEDDOORO")
9910   K_TOOLBOX=FN OID("TOOLBOX")
9920   K_CRANK=FN OID("CRANK")
9930   K_SCREWDRIVER=FN OID("SCREWDRIVER")
9940   K_HAMMER=FN OID("HAMMER")
9950   K_SHELF=FN OID("SHELF")
9960   K_PAINT=FN OID("PAINT")
9970   K_BENCHW=FN OID("BENCHW")
9980   K_BLUEPRINT=FN OID("BLUEPRINT")
9990   K_SHIP=FN OID("SHIP")
10000   K_HALLDOOR=FN OID("HALLDOOR")
10010   K_HALLTABLE=FN OID("HALLTABLE")
10020   K_LAMP=FN OID("LAMP")
10030   K_CLOSET=FN OID("CLOSET")
10040   K_UMBRELLA=FN OID("UMBRELLA")
10050   K_CLOCK=FN OID("CLOCK")
10060   K_STAIRSH=FN OID("STAIRSH")
10070   K_SOFA=FN OID("SOFA")
10080   K_COIN=FN OID("COIN")
10090   K_CUPBOARD=FN OID("CUPBOARD")
10100   K_CAPE=FN OID("CAPE")
10110   K_CRYSTAL=FN OID("CRYSTAL")
10120   K_LTABLE=FN OID("LTABLE")
10130   K_SANDWICH=FN OID("SANDWICH")
10140   K_BOOKSHELF=FN OID("BOOKSHELF")
10150   K_PHOTO=FN OID("PHOTO")
10160   K_BOOK=FN OID("BOOK")
10170   K_TV=FN OID("TV")
10180   K_FRIDGE=FN OID("FRIDGE")
10190   K_MILK=FN OID("MILK")
10200   K_JELLY=FN OID("JELLY")
10210   K_ICE=FN OID("ICE")
10220   K_COMPASS=FN OID("COMPASS")
10230   K_OVEN=FN OID("OVEN")
10240   K_COIL=FN OID("COIL")
10250   K_SINK=FN OID("SINK")
10260   K_TAP=FN OID("TAP")
10270   K_KTABLE=FN OID("KTABLE")
10280   K_BREAD=FN OID("BREAD")
10290   K_CHAIR=FN OID("CHAIR")
10300   K_KCUPBOARD=FN OID("KCUPBOARD")
10310   K_PAN=FN OID("PAN")
10320   K_PANTRY=FN OID("PANTRY")
10330   K_FLOUR=FN OID("FLOUR")
10340   K_BACKDOORI=FN OID("BACKDOORI")
10350   K_CSTAIRS=FN OID("CSTAIRS")
10360   K_LINEN=FN OID("LINEN")
10370   K_SHEET=FN OID("SHEET")
10380   K_LADDER=FN OID("LADDER")
10390   K_PICTURE=FN OID("PICTURE")
10400   K_BED=FN OID("BED")
10410   K_PILLOW=FN OID("PILLOW")
10420   K_GLORP=FN OID("GLORP")
10430   K_NOZZLE=FN OID("NOZZLE")
10440   K_WARDROBE=FN OID("WARDROBE")
10450   K_HAT=FN OID("HAT")
10460   K_NIGHTSTAND=FN OID("NIGHTSTAND")
10470   K_NOTE=FN OID("NOTE")
10480   K_TOYCHEST=FN OID("TOYCHEST")
10490   K_TOYROCKET=FN OID("TOYROCKET")
10500   K_BALL=FN OID("BALL")
10510   K_TUB=FN OID("TUB")
10520   K_MIRROR=FN OID("MIRROR")
10530   K_PLATE=FN OID("PLATE")
10540   K_BSINK=FN OID("BSINK")
10550   K_SOAP=FN OID("SOAP")
10560   K_CABINET=FN OID("CABINET")
10570   K_PILLS=FN OID("PILLS")
10580   K_TOWEL=FN OID("TOWEL")
10590   K_WASHER=FN OID("WASHER")
10600   K_FUELCELL=FN OID("FUELCELL")
10610   K_SOCKS=FN OID("SOCKS")
10620   K_PLUG=FN OID("PLUG")
10630   K_CBARRELS=FN OID("CBARRELS")
10640   K_CSHELF=FN OID("CSHELF")
10650   K_PICKLES=FN OID("PICKLES")
10660   K_FUSEBOX=FN OID("FUSEBOX")
10670   K_HATCHI=FN OID("HATCHI")
10680   K_SAFE=FN OID("SAFE")
10690   K_GLYPH=FN OID("GLYPH")
10700   K_TRUNK=FN OID("TRUNK")
10710   K_SCARF=FN OID("SCARF")
10720   K_TELESCOPE=FN OID("TELESCOPE")
10730   K_GLOBE=FN OID("GLOBE")
10740   K_DEQQ=FN OID("DEQQ")
10750 END PROC
10760 ' ------------------------------------------------------------------------------------------------ the words of the commands
10770 ' verbs: 1..8 the directions N S E W U D IN OUT, 9 go, 10 look, 11 examine, 12 take, 13 drop, 14 inventory, 15 open, 16 close, 17 turn,
10780 ' 18 push, 19 pull, 20 climb / stand, 21 hide, 22 wait, 23 give / feed, 24 read, 25 unlock, 26 dial a code, 27 install, 28 help, 29 hint,
10790 ' 30 save, 31 load, 32 quit, 33 status, 34 put, 35 unplug, 36 unscrew / remove, 37 search, 38 listen, 39 smell, 40 leave, 41 light, 42 talk,
10800 ' 43 eat, 44 noise (jump, sing ...), 45 look through / peek, 46 knock, 47 lock, 48 wind (wind up, crank)
10810 LABEL VERBDATA
10820 DATA "N",1,"NORTH",1,"S",2,"SOUTH",2,"E",3,"EAST",3,"W",4,"WEST",4,"U",5,"UP",5,"UPSTAIRS",5,"D",6,"DOWN",6,"DOWNSTAIRS",6,"IN",7,"INSIDE",7,"ENTER",7,"OUT",8,"OUTSIDE",8
10830 DATA "GO",9,"WALK",9,"RUN",9,"HEAD",9,"MOVE",18
10840 DATA "LOOK",10,"L",10,"EXAMINE",11,"X",11,"INSPECT",11,"CHECK",11,"STUDY",11,"DESCRIBE",11,"WATCH",11
10850 DATA "TAKE",12,"GET",12,"GRAB",12,"PICK",12,"CARRY",12,"HOLD",12,"STEAL",12,"COLLECT",12
10860 DATA "DROP",13,"DISCARD",13,"RELEASE",13,"THROW",13,"PLACE",34,"PUT",34,"INSERT",34,"STORE",34,"LAY",34,"STASH",34
10870 DATA "INVENTORY",14,"INV",14,"I",14,"ITEMS",14,"POCKETS",14
10880 DATA "OPEN",15,"CLOSE",16,"SHUT",16
10890 DATA "TURN",17,"SWITCH",17,"FLIP",17,"TOGGLE",17,"ROTATE",17,"TWIST",17,"STOP",17,"START",17,"PRESS",17,"PUSH",18,"SHOVE",18,"PULL",19,"YANK",19,"TUG",19
10900 DATA "CLIMB",20,"STAND",20,"MOUNT",20,"HIDE",21,"CONCEAL",21,"WAIT",22,"Z",22,"REST",22,"SLEEP",22
10910 DATA "GIVE",23,"FEED",23,"OFFER",23,"HAND",23,"READ",24,"UNLOCK",25,"DIAL",26,"TYPE",26,"PUNCH",26,"SET",26
10920 DATA "INSTALL",27,"ATTACH",27,"FIT",27,"BUILD",27,"REPAIR",27,"USE",27,"CONNECT",27
10930 DATA "HELP",28,"COMMANDS",28,"?",28,"HINT",29,"CLUE",29,"SAVE",30,"LOAD",31,"RESTORE",31,"QUIT",32,"BYE",32,"EXIT",40,"LEAVE",40,"COME",40
10940 DATA "STATUS",33,"SCORE",33,"PARTS",33,"PROGRESS",33,"UNPLUG",35,"DISCONNECT",35,"UNSCREW",36,"REMOVE",36,"DETACH",36,"DISMANTLE",36
10950 DATA "SEARCH",37,"LISTEN",38,"SMELL",39,"SNIFF",39,"LIGHT",41,"IGNITE",41,"TALK",42,"SPEAK",42,"ASK",42,"SAY",42,"GREET",42,"HELLO",42
10960 DATA "EAT",43,"TASTE",43,"DRINK",43,"LICK",43,"JUMP",44,"DANCE",44,"SING",44,"SHOUT",44,"YELL",44,"SCREAM",44,"WHISTLE",44,"PEEK",45
10970 DATA "KNOCK",46,"LOCK",47,"WIND",48,"CRANK",48,"END",0
10980 ' ------------------------------------------------------------------------------------------------ what can be seen and reached
10990 ' ROOMOF: the room of a thing (-1 = carried, 0 = nowhere, 100 = built into the wreck); things inside other things follow their parent
11000 FUNC ROOMOF(I)
11010   IF OIN(I)>0 THEN RETURN FN ROOMOF(OIN(I))
11020   RETURN OROOM(I)
11030 END FUNC
11040 FUNC HASFL(I,F$)
11050   RETURN FN SPOS(OFL$(I),F$)>0
11060 END FUNC
11070 FUNC LAMPOK
11080   IF NOT LAMPLIT THEN RETURN 0
11090   RETURN FN ROOMOF(K_LAMP)=-1 OR FN ROOMOF(K_LAMP)=ROOM
11100 END FUNC
11110 FUNC ISDARK
11120   IF ROOM=R_CELLAR AND NOT FN LAMPOK THEN RETURN -1
11130   RETURN 0
11140 END FUNC
11150 FUNC ISOPEN(P)
11160   IF FN HASFL(P,"A") THEN RETURN -1
11170   IF FN HASFL(P,"C") THEN RETURN OOPEN(P)<>0
11180   RETURN 0
11190 END FUNC
11200 ' VIS: is the thing here and in view (in the room or carried; in an open container; on a surface (high ones: standing on a chair); under something that was looked under)
11210 FUNC VIS(I)
11220   LOCAL P
11230   IF I<=0 THEN RETURN 0
11240   IF OHID(I) THEN RETURN 0
11250   P=OIN(I)
11260   IF P=0 THEN
11270     IF OROOM(I)=-1 THEN RETURN -1
11280     IF OROOM(I)=ROOM THEN RETURN NOT FN ISDARK
11290     RETURN 0
11300   END IF
11310   IF NOT FN VIS(P) THEN RETURN 0
11320   IF OREL(I)=1 THEN RETURN FN ISOPEN(P)
11330   IF OREL(I)=2 THEN
11340     IF FN HASFL(P,"H") AND NOT ONCHAIR THEN RETURN 0
11350     RETURN -1
11360   END IF
11370   RETURN OSEEN(P)<>0
11380 END FUNC
11390 FUNC USED
11400   LOCAL I,S
11410   S=0
11420   FOR I=1 TO NOBJ
11430     IF OROOM(I)=-1 AND OIN(I)=0 THEN S=S+OSIZE(I)
11440   NEXT I
11450   RETURN S
11460 END FUNC
11470 ' the number of parts built into the wreck
11480 FUNC BUILT
11490   LOCAL I,S
11500   S=0
11510   FOR I=1 TO NOBJ
11520     IF OROOM(I)=100 AND FN HASFL(I,"P") THEN S=S+1
11530   NEXT I
11540   RETURN S
11550 END FUNC
11560 FUNC HIDESPOT(R)
11570   LOCAL I
11580   FOR I=1 TO NOBJ
11590     IF OROOM(I)=R AND OIN(I)=0 AND FN HASFL(I,"X") THEN RETURN I
11600   NEXT I
11610   RETURN 0
11620 END FUNC
11630 ' the names of the visible things inside / on / under P (relation REL), "a, b and c"
11640 FUNC KIDS$(P,REL)
11650   LOCAL I,N,R$,L$
11660   N=0
11670   R$=""
11680   FOR I=1 TO NOBJ
11690     IF OIN(I)=P AND OREL(I)=REL AND FN VIS(I) THEN
11700       N=N+1
11710       IF N>1 THEN R$=R$+", "
11720       R$=R$+ONAME$(I)
11730     END IF
11740   NEXT I
11750   IF N>1 THEN
11760     L$=R$
11770     I=LEN(L$)
11780     WHILE I>0 AND MID$(L$,I,1)<>","
11790       I=I-1
11800     WEND
11810     R$=LEFT$(L$,I-1)+" and"+MID$(L$,I+1)
11820   END IF
11830   RETURN R$
11840 END FUNC
11850 FUNC DIRWORD$(D)
11860   IF D=1 THEN RETURN "north"
11870   IF D=2 THEN RETURN "south"
11880   IF D=3 THEN RETURN "east"
11890   IF D=4 THEN RETURN "west"
11900   IF D=5 THEN RETURN "up"
11910   IF D=6 THEN RETURN "down"
11920   IF D=7 THEN RETURN "in"
11930   RETURN "out"
11940 END FUNC
11950 ' ------------------------------------------------------------------------------------------------ describing things
11960 FUNC INSIDEHOUSE(R)
11970   RETURN R>=R_HALL
11980 END FUNC
11990 FUNC DYN$(I)
12000   LOCAL T$,N,K
12010   IF I=K_FRONTDOORO OR I=K_HALLDOOR THEN
12020     IF FRONTOPEN THEN RETURN "The front door stands open. Daylight falls into the hall in a sharp rectangle."
12030     RETURN "The front door is shut, and bolted from the inside. You could rattle it all day."
12040   END IF
12050   IF I=K_BACKDOORO THEN
12060     IF BACKOPEN THEN RETURN "The back door stands open."
12070     IF BACKLOCK THEN RETURN "A plain back door, locked. The lock is brass; the key, you suspect, is a spare somebody hid."
12080     RETURN "The back door is closed, but not locked."
12090   END IF
12100   IF I=K_BACKDOORI THEN
12110     IF BACKOPEN THEN RETURN "The back door stands open."
12120     IF BACKLOCK THEN RETURN "The back door is locked, but from inside you can turn the lock yourself."
12130     RETURN "The back door is closed, but not locked."
12140   END IF
12150   IF I=K_HATCHO THEN
12160     IF HATCHOPEN THEN RETURN "The hatch is open: a ladder leads down into the cellar."
12170     IF HATCHLOCK THEN RETURN "A heavy iron hatch with a brass padlock."
12180     RETURN "The hatch is unlocked but closed."
12190   END IF
12200   IF I=K_HATCHI THEN
12210     IF HATCHOPEN THEN RETURN "A ladder leads up to the open hatch and the west garden."
12220     RETURN "A ladder leads up to a closed hatch."
12230   END IF
12240   IF I=K_SHEDDOORO THEN
12250     IF SHEDOPEN THEN RETURN "The shed door is open."
12260     IF SHEDLOCK THEN RETURN "A plank door, shut with a brass padlock."
12270     RETURN "The shed door is closed; the padlock hangs open."
12280   END IF
12290   IF I=K_WELL THEN
12300     IF WELLUP THEN RETURN "The old well. The windlass has its crank, and the iron bucket hangs at the rim."
12310     IF CRANKED THEN RETURN "The old well. The crank handle is fitted to the windlass now: you could turn it to wind up the bucket."
12320     RETURN "The old well. Far down in the dark an iron bucket hangs on the rope. The windlass has no crank handle, only a rusty stub where one should be."
12330   END IF
12340   IF I=K_FRIDGE THEN
12350     IF OOPEN(I) THEN RETURN "The fridge hums in two keys at once. Its door is open and a cold light shines out."
12360     RETURN "A tall fridge that hums in two keys at once. The door is closed."
12370   END IF
12380   IF I=K_OVEN THEN
12390     IF OVENON THEN RETURN "The oven. A big knob is set to ON and the glass shows a red glow inside; the door is far too hot to touch."
12400     IF OVENHOT THEN RETURN "The oven. The knob is set to OFF, but the door is still too hot to touch."
12410     IF OOPEN(I) THEN RETURN "The oven, cold now, with its door open."
12420     RETURN "The oven. The knob is set to OFF and the door has cooled down."
12430   END IF
12440   IF I=K_TAP THEN
12450     IF TAPON THEN RETURN "A brass tap. Hot water is running into the sink and steaming."
12460     RETURN "A brass tap over the sink. The water is off."
12470   END IF
12480   IF I=K_WASHER THEN
12490     IF WASHON THEN RETURN "A big old washing machine. Its drum spins with a hum, and something bright glows through the round window. It is plugged in. The door is locked while it runs."
12500     IF OOPEN(I) THEN RETURN "The washing machine, unplugged. The round door is open."
12510     RETURN "The washing machine has stopped. The round door is closed."
12520   END IF
12530   IF I=K_PLUG THEN
12540     IF WASHON THEN RETURN "A thick plug in a socket in the wall. The cable runs to the washing machine."
12550     RETURN "The plug is out of its socket and hangs on its cable."
12560   END IF
12570   IF I=K_MIRROR THEN
12580     IF MIRRORON THEN RETURN "It is not glass at all, but polished metal, held to the tiles by four screws with four-pointed heads. Your reflection looks slightly annoyed."
12590     RETURN "Four screw holes in the tiles and a clean rectangle on the wall."
12600   END IF
12610   IF I=K_SAFE THEN
12620     IF SAFEOPEN THEN RETURN "The safe stands open."
12630     RETURN "A small, heavy safe in the chimney wall. It has a dial with three digits and is locked."
12640   END IF
12650   IF I=K_GLORP THEN
12660     IF GLORPHERE THEN RETURN "A fat purple blob with three eyes and a mouth full of small teeth. It lies under the bed and guards something shiny behind it, growling like a kettle. It looks hungry."
12670     RETURN "The glorp sits on the landing and smiles with all three eyes. It looks happy and a little too round."
12680   END IF
12690   IF I=K_ICE THEN
12700     RETURN "A block of ice as big as a loaf of bread. A small metallic thing is frozen inside it: a compass! It would have to melt first."
12710   END IF
12720   IF I=K_LAMP THEN
12730     IF LAMPLIT THEN RETURN "A lantern. It glows with a warm yellow light."
12740     RETURN "A lantern with a small glass chimney and a switch on its base. It is off."
12750   END IF
12760   IF I=K_PHOTO THEN
12770     IF PHOTOBACK THEN RETURN "A photo of three tall figures in front of a silver ship, waving. You turned it over: scratched into the back with a pin are three digits: 7 3 1."
12780     RETURN ODESC$(I)+" You wonder what is on the back."
12790   END IF
12800   IF I=K_SHIP THEN
12810     N=FN BUILT
12820     T$="The wreck of Deqq's ship: a skeleton of beams in a burnt hollow. "+STR$(N)+" of the 8 parts are built in."
12830     IF N=0 THEN T$=T$+" You could INSTALL a part here once you have one."
12840     RETURN T$
12850   END IF
12860   IF I=K_DEQQ THEN
12870     RETURN "Deqq is tall and grey-green, with a long neck, a cloak the color of a bruise and huge black eyes with a faint glow deep inside. He does not blink. When he moves you do not see him move."
12880   END IF
12890   IF I=K_CHAIR THEN
12900     IF ONCHAIR THEN RETURN "You are standing on the chair. It wobbles a little. GET DOWN to step off."
12910   END IF
12920   IF I=K_CLOSET OR I=K_WARDROBE OR I=K_PANTRY OR I=K_LINEN OR I=K_TRUNK THEN
12930     RETURN ODESC$(I)
12940   END IF
12950   RETURN ODESC$(I)
12960 END FUNC
12970 PROC EXAMINE(I)
12980   LOCAL T$,K$
12990   CALL SAY(FN DYN$(I),1)
13000   IF FN HASFL(I,"A") OR (FN HASFL(I,"C") AND OOPEN(I)) THEN
13010     K$=FN KIDS$(I,1)
13020     IF K$<>"" THEN CALL SAY("Inside: "+K$+".",1)
13030     IF K$="" AND NOT (I=K_WASHER) THEN CALL SAY("It is empty.",6)
13040   END IF
13050   K$=FN KIDS$(I,2)
13060   IF K$<>"" THEN CALL SAY("On it: "+K$+".",1)
13070   IF OSEEN(I) THEN
13080     K$=FN KIDS$(I,3)
13090     IF K$<>"" THEN CALL SAY("Under it: "+K$+".",1)
13100   END IF
13110 END PROC
13120 ' room description: FULL = always the long text, else only the first time
13130 PROC LOOKROOM(FULL)
13140   LOCAL I,L$,N,E$
13150   IF FN ISDARK THEN
13160     CALL SAY("Cellar. It is pitch dark. You can't see a thing, only feel the cold stone steps behind you. A light would help.",1)
13170     CALL SAY("Exits: up.",6)
13180     EXIT PROC
13190   END IF
13200   IF FULL OR NOT RSEEN(ROOM) THEN
13210     CALL SAY(RNAME$(ROOM)+". "+RDESC$(ROOM),1)
13220   ELSE
13230     CALL SAY(RNAME$(ROOM)+".",1)
13240   END IF
13250   RSEEN(ROOM)=-1
13260   L$=""
13270   N=0
13280   FOR I=1 TO NOBJ
13290     IF OSIZE(I)>0 AND OROOM(I)<>-1 AND FN VIS(I) THEN
13300       N=N+1
13310       IF N>1 THEN L$=L$+", "
13320       L$=L$+ONAME$(I)
13330     END IF
13340   NEXT I
13350   IF N>1 THEN
13360     I=LEN(L$)
13370     WHILE I>0 AND MID$(L$,I,1)<>","
13380       I=I-1
13390     WEND
13400     L$=LEFT$(L$,I-1)+" and"+MID$(L$,I+1)
13410   END IF
13420   IF N>0 THEN CALL SAY("You see "+L$+".",1)
13430   IF FN VIS(K_GLORP) AND ROOM=R_LANDING THEN CALL SAY("The glorp is here, purring.",1)
13440   IF DQHOME AND DQROOM=ROOM AND ROOM>=R_HALL THEN CALL SAY("Deqq is here, watching you.",2)
13450   IF HIDING THEN CALL SAY("You are hiding in "+ONAME$(HIDEOBJ)+".",3)
13460   E$=""
13470   FOR I=1 TO 8
13480     IF EX(ROOM,I)>0 THEN
13490       IF E$<>"" THEN E$=E$+", "
13500       E$=E$+FN DIRWORD$(I)
13510     END IF
13520   NEXT I
13530   CALL SAY("Exits: "+E$+".",6)
13540 END PROC
13550 ' ------------------------------------------------------------------------------------------------ finding the things the player means
13560 FUNC RESOLVE(P$)
13570   LOCAL I,J,N,K,BEST,BS,SC,PASS,T$
13580   IF P$="" THEN RETURN 0
13590   N=0
13600   T$=P$+" "
13610   WHILE LEN(T$)>0
13620     K=FN SPOS(T$," ")
13630     N=N+1
13640     PW$(N)=LEFT$(T$,K-1)
13650     T$=MID$(T$,K+1)
13660   WEND
13670   IF PW$(N)="IT" OR PW$(N)="THEM" THEN
13680     IF LASTOBJ>0 THEN
13690       IF FN VIS(LASTOBJ) THEN RETURN LASTOBJ
13700     END IF
13710     RETURN 0
13720   END IF
13730   BEST=0
13740   BS=0
13750   FOR PASS=1 TO 2
13760     FOR I=1 TO NOBJ
13770       IF (PASS=1 AND OROOM(I)=-1) OR (PASS=2 AND OROOM(I)<>-1) THEN
13780         IF FN INLIST(PW$(N),OWORDS$(I)) THEN
13790           IF FN VIS(I) THEN
13800             SC=1
13810             FOR J=1 TO N-1
13820               IF FN INLIST(PW$(J),OWORDS$(I)) THEN SC=SC+1
13830             NEXT J
13840             IF SC>BS THEN
13850               BS=SC
13860               BEST=I
13870             END IF
13880           END IF
13890         END IF
13900       END IF
13910     NEXT I
13920   NEXT PASS
13930   RETURN BEST
13940 END FUNC
13950 FUNC FINDVERB(W$)
13960   LOCAL I
13970   FOR I=1 TO NVERB
13980     IF VW$(I)=W$ THEN RETURN VID(I)
13990   NEXT I
14000   RETURN 0
14010 END FUNC
14020 ' the command line -> V (verb), A and B (things), PREP$ (IN ON UNDER BEHIND WITH TO FROM AT THROUGH), PART$ (ON OFF UP DOWN OVER ...), NUM$ (digits)
14030 PROC PARSE(L$)
14040   LOCAL I,J,S,P,Z$
14050   CALL SPLITW(L$)
14060   J=0
14070   FOR I=1 TO NW
14080     IF NOT FN INLIST(W$(I)," THE A AN SOME MY OF PLEASE ") THEN
14090       J=J+1
14100       W$(J)=W$(I)
14110     END IF
14120   NEXT I
14130   NW=J
14140   V=0:A=0:B=0:PREP$="":PART$="":P1$="":P2$="":NUM$=""
14150   IF NW=0 THEN EXIT PROC
14160   V=FN FINDVERB(W$(1))
14170   IF V=0 THEN EXIT PROC
14180   S=2
14190   IF V=9 AND NW>=2 THEN
14200     V=FN FINDVERB(W$(2))
14210     S=3
14220     IF V<1 OR V>8 THEN V=-1
14230   END IF
14240   IF V<=8 AND NOT (V=7 AND NW>=2) THEN EXIT PROC
14250   ' particles that belong to the verb: TURN ON x, TURN x ON, PICK UP x, PICK x UP, TURN x OVER ...
14260   IF V=17 OR V=12 OR V=34 OR V=13 OR V=20 THEN
14270     P=0
14280     FOR I=S TO NW
14290       IF P=0 AND FN INLIST(W$(I)," ON OFF OVER AROUND UP DOWN ") THEN
14300         IF (I=S) OR (I=NW) OR V=17 THEN P=I
14310       END IF
14320     NEXT I
14330     IF P>0 THEN
14340       PART$=W$(P)
14350       FOR J=P TO NW-1
14360         W$(J)=W$(J+1)
14370       NEXT J
14380       NW=NW-1
14390     END IF
14400   END IF
14410   IF S<=NW THEN
14420     IF FN INLIST(W$(S)," AT OUT ") AND PART$="" THEN
14430       PART$=W$(S)
14440       S=S+1
14450     END IF
14460   END IF
14470   P=0
14480   FOR I=S TO NW
14490     IF P=0 AND FN INLIST(W$(I)," IN INTO INSIDE ONTO ON UNDER BENEATH BELOW BEHIND WITH USING TO TOWARD TOWARDS FROM THROUGH AT ") THEN P=I
14500   NEXT I
14510   FOR I=S TO NW
14520     IF FN ISNUM(W$(I)) THEN
14530       NUM$=NUM$+W$(I)
14540     ELSE
14550       IF P=0 OR I<P THEN
14560         IF P1$<>"" THEN P1$=P1$+" "
14570         P1$=P1$+W$(I)
14580       ELSE
14590         IF I>P THEN
14600           IF P2$<>"" THEN P2$=P2$+" "
14610           P2$=P2$+W$(I)
14620         END IF
14630       END IF
14640     END IF
14650   NEXT I
14660   IF P>0 THEN
14670     Z$=W$(P)
14680     PREP$=Z$
14690     IF Z$="INTO" OR Z$="INSIDE" THEN PREP$="IN"
14700     IF Z$="ONTO" THEN PREP$="ON"
14710     IF Z$="BENEATH" OR Z$="BELOW" THEN PREP$="UNDER"
14720     IF Z$="USING" THEN PREP$="WITH"
14730     IF Z$="TOWARD" OR Z$="TOWARDS" THEN PREP$="TO"
14740   END IF
14750   IF V=42 OR V=44 OR V=30 OR V=31 OR V=26 THEN EXIT PROC
14760   A=FN RESOLVE(P1$)
14770   B=FN RESOLVE(P2$)
14780 END PROC
14790 ' ------------------------------------------------------------------------------------------------ the commands
14800 PROC SND(N)
14810   IF HL THEN EXIT PROC
14820   SFX N
14830 END PROC
14840 PROC NOSEE
14850   IF P1$="" AND P2$="" THEN
14860     CALL SAY("Which thing do you mean?",1)
14870   ELSE
14880     CALL SAY("You don't see any such thing here.",1)
14890   END IF
14900 END PROC
14910 PROC UNHIDE
14920   IF NOT HIDING THEN
14930     CALL SAY("You are not hiding.",1)
14940     EXIT PROC
14950   END IF
14960   CALL SAY("You crawl out of "+ONAME$(HIDEOBJ)+" and brush the dust off.",1)
14970   HIDING=0
14980   TCOST=1
14990   REDRAW=-1
15000 END PROC
15010 ' ---- moving
15020 FUNC DOORCHECK$(F,T)
15030   IF F=R_FRONT AND T=R_HALL THEN
15040     IF NOT FRONTOPEN THEN RETURN "The front door is shut and bolted from the inside. Somebody does not want visitors."
15050   END IF
15060   IF F=R_HALL AND T=R_FRONT THEN
15070     IF NOT FRONTOPEN THEN
15080       FRONTOPEN=-1
15090       CALL SAY("You slide the bolt back and open the front door.",1)
15100     END IF
15110   END IF
15120   IF F=R_BACKYD AND T=R_KITCHEN THEN
15130     IF BACKLOCK THEN RETURN "The back door is locked."
15140     IF NOT BACKOPEN THEN
15150       BACKOPEN=-1
15160       CALL SAY("You open the back door.",1)
15170     END IF
15180   END IF
15190   IF F=R_KITCHEN AND T=R_BACKYD THEN
15200     IF BACKLOCK THEN
15210       BACKLOCK=0
15220       CALL SAY("You turn the lock from inside.",1)
15230     END IF
15240     IF NOT BACKOPEN THEN
15250       BACKOPEN=-1
15260       CALL SAY("You open the back door.",1)
15270     END IF
15280   END IF
15290   IF F=R_WESTG AND T=R_CELLAR THEN
15300     IF HATCHLOCK THEN RETURN "The hatch is shut with a heavy padlock."
15310     IF NOT HATCHOPEN THEN
15320       HATCHOPEN=-1
15330       CALL SAY("You heave the hatch open.",1)
15340     END IF
15350   END IF
15360   IF F=R_CELLAR AND T=R_WESTG THEN
15370     IF HATCHLOCK THEN
15380       HATCHLOCK=0
15390       CALL SAY("You unhook the padlock from inside.",1)
15400     END IF
15410     IF NOT HATCHOPEN THEN
15420       HATCHOPEN=-1
15430       CALL SAY("You push the hatch open.",1)
15440     END IF
15450   END IF
15460   IF F=R_WELLG AND T=R_SHED THEN
15470     IF SHEDLOCK THEN RETURN "The shed door is shut with a brass padlock."
15480     IF NOT SHEDOPEN THEN
15490       SHEDOPEN=-1
15500       CALL SAY("You open the shed door.",1)
15510     END IF
15520   END IF
15530   RETURN ""
15540 END FUNC
15550 PROC DOMOVE(D)
15560   LOCAL T,M$
15570   IF HIDING THEN
15580     IF D=8 THEN
15590       CALL UNHIDE
15600     ELSE
15610       CALL SAY("You are hiding. COME OUT first (or just wait, if you dare).",3)
15620     END IF
15630     EXIT PROC
15640   END IF
15650   T=EX(ROOM,D)
15660   IF T=0 THEN
15670     CALL SAY("You can't go that way.",1)
15680     EXIT PROC
15690   END IF
15700   M$=FN DOORCHECK$(ROOM,T)
15710   IF M$<>"" THEN
15720     CALL SAY(M$,1)
15730     EXIT PROC
15740   END IF
15750   IF ONCHAIR THEN ONCHAIR=0
15760   ROOM=T
15770   TCOST=1
15780   REDRAW=-1
15790   CALL SND(1)
15800   CALL LOOKROOM(0)
15810 END PROC
15820 ' ---- looking
15830 PROC REVEAL(T)
15840   LOCAL I,K$
15850   OSEEN(T)=-1
15860   FOR I=1 TO NOBJ
15870     IF OIN(I)=T AND OREL(I)=3 AND OHID(I) THEN
15880       IF NOT (I=K_NOZZLE AND GLORPHERE) THEN OHID(I)=0
15890     END IF
15900   NEXT I
15910   K$=FN KIDS$(T,3)
15920   IF K$="" THEN
15930     CALL SAY("You look under "+ONAME$(T)+": nothing but dust and an old sock.",1)
15940   ELSE
15950     CALL SAY("You look under "+ONAME$(T)+" and find: "+K$+".",1)
15960   END IF
15970   IF T=K_BED AND GLORPHERE THEN
15980     CALL SAY("Two small eyes open in the dark. Then a third. Something growls, and it isn't the bed. Behind it a metal shape gleams.",4)
15990   END IF
16000   TCOST=1
16010   REDRAW=-1
16020 END PROC
16030 PROC DOLOOK
16040   LOCAL T
16050   IF HIDING THEN
16060     CALL SAY("You peek through a crack.",6)
16070     CALL LOOKROOM(0)
16080     EXIT PROC
16090   END IF
16100   IF P1$="" AND P2$="" AND PART$<>"AT" THEN
16110     CALL LOOKROOM(-1)
16120     EXIT PROC
16130   END IF
16140   IF PREP$="" THEN
16150     IF A>0 THEN
16160       LASTOBJ=A
16170       CALL EXAMINE(A)
16180     ELSE
16190       CALL NOSEE
16200     END IF
16210     EXIT PROC
16220   END IF
16230   T=B
16240   IF A>0 AND B=0 THEN T=A
16250   IF P1$<>"" AND A>0 AND B>0 THEN T=B
16260   IF T<=0 THEN
16270     CALL NOSEE
16280     EXIT PROC
16290   END IF
16300   LASTOBJ=T
16310   IF PREP$="UNDER" THEN
16320     IF FN HASFL(T,"U") THEN
16330       CALL REVEAL(T)
16340     ELSE
16350       CALL SAY("There is nothing under "+ONAME$(T)+".",1)
16360     END IF
16370     EXIT PROC
16380   END IF
16390   IF PREP$="BEHIND" THEN
16400     IF T=K_PHOTO THEN
16410       CALL DOTURNPHOTO
16420     ELSE
16430       IF FN HASFL(T,"X") THEN
16440         CALL SAY("There is room to hide behind it.",1)
16450       ELSE
16460         CALL SAY("Nothing behind "+ONAME$(T)+" but a wall.",1)
16470       END IF
16480     END IF
16490     EXIT PROC
16500   END IF
16510   IF PREP$="IN" OR PREP$="THROUGH" THEN
16520     IF FN HASFL(T,"A") OR (FN HASFL(T,"C") AND OOPEN(T)) THEN
16530       CALL EXAMINE(T)
16540     ELSE
16550       IF FN HASFL(T,"C") THEN
16560         CALL SAY("It is closed. OPEN it first.",1)
16570       ELSE
16580         CALL SAY("You can't look inside that.",1)
16590       END IF
16600     END IF
16610     EXIT PROC
16620   END IF
16630   CALL EXAMINE(T)
16640 END PROC
16650 PROC DOTURNPHOTO
16660   IF PHOTOBACK THEN
16670     CALL SAY("The back says: 7 3 1. You already know.",1)
16680   ELSE
16690     PHOTOBACK=-1
16700     CALL SAY("You bend the little clips and look at the back of the photo. Scratched into the cardboard with a pin: 7 3 1.",5)
16710     TCOST=1
16720   END IF
16730 END PROC
16740 PROC DOINV
16750   LOCAL I,K$,N
16760   K$=""
16770   N=0
16780   FOR I=1 TO NOBJ
16790     IF OROOM(I)=-1 AND OIN(I)=0 THEN
16800       N=N+1
16810       IF N>1 THEN K$=K$+", "
16820       K$=K$+ONAME$(I)
16830       IF OSIZE(I)>1 THEN K$=K$+" (2 slots)"
16840     END IF
16850   NEXT I
16860   IF N=0 THEN
16870     CALL SAY("You are carrying nothing. ("+STR$(CAP)+" slots free)",1)
16880   ELSE
16890     CALL SAY("You carry: "+K$+". ("+STR$(FN USED)+" of "+STR$(CAP)+" slots used)",1)
16900   END IF
16910 END PROC
16920 ' ---- taking and dropping
16930 FUNC TAKEBLOCK$(I)
16940   IF I=K_COMPASS AND OIN(I)=K_ICE THEN RETURN "The compass is frozen into the block of ice. You could take the whole block, though."
16950   IF I=K_GLORP THEN RETURN "The glorp growls. You decide you would rather keep your fingers."
16960   IF I=K_MIRROR THEN RETURN "The mirror is held to the wall by four screws."
16970   IF I=K_DEQQ THEN RETURN "You think about it for exactly one second."
16980   IF OSIZE(I)=0 THEN RETURN "You can't take "+ONAME$(I)+"."
16990   RETURN ""
17000 END FUNC
17010 PROC DOTAKE
17020   LOCAL M$
17030   IF A=0 THEN
17040     IF PART$="DOWN" AND ONCHAIR THEN
17050       ONCHAIR=0
17060       REDRAW=-1
17070       CALL SAY("You step down from the chair.",1)
17080       TCOST=1
17090       EXIT PROC
17100     END IF
17110     IF PART$="OUT" OR PART$="UP" THEN
17120       IF HIDING THEN CALL UNHIDE
17130       EXIT PROC
17140     END IF
17150     IF P1$="" THEN
17160       CALL SAY("What do you want to take?",1)
17170     ELSE
17180       CALL NOSEE
17190     END IF
17200     EXIT PROC
17210   END IF
17220   LASTOBJ=A
17230   IF OROOM(A)=-1 AND OIN(A)=0 THEN
17240     CALL SAY("You already have "+ONAME$(A)+".",1)
17250     EXIT PROC
17260   END IF
17270   M$=FN TAKEBLOCK$(A)
17280   IF M$<>"" THEN
17290     CALL SAY(M$,1)
17300     EXIT PROC
17310   END IF
17320   IF FN USED+OSIZE(A)>CAP THEN
17330     CALL SAY("You can't carry that as well. You have "+STR$(CAP)+" slots and "+ONAME$(A)+" needs "+STR$(OSIZE(A))+". DROP something first.",1)
17340     EXIT PROC
17350   END IF
17360   OIN(A)=0
17370   OROOM(A)=-1
17380   CALL SAY("Taken: "+ONAME$(A)+".",1)
17390   CALL SND(2)
17400   TCOST=1
17410   REDRAW=-1
17420 END PROC
17430 PROC DODROP
17440   IF A=0 THEN
17450     IF P1$="" THEN CALL SAY("What do you want to drop?",1) ELSE CALL NOSEE
17460     EXIT PROC
17470   END IF
17480   IF OROOM(A)<>-1 OR OIN(A)<>0 THEN
17490     CALL SAY("You are not carrying "+ONAME$(A)+".",1)
17500     EXIT PROC
17510   END IF
17520   OROOM(A)=ROOM
17530   CALL FLOORSPOT(A,ROOM)
17540   LASTOBJ=A
17550   CALL SAY("Dropped: "+ONAME$(A)+".",1)
17560   TCOST=1
17570   REDRAW=-1
17580 END PROC
17590 PROC DOPUT
17600   LOCAL T,R,M$
17610   IF A=0 THEN
17620     IF P1$="" THEN CALL SAY("What do you want to put?",1) ELSE CALL NOSEE
17630     EXIT PROC
17640   END IF
17650   IF PREP$="" AND B=0 THEN
17660     IF PART$="DOWN" THEN
17670       CALL DODROP
17680     ELSE
17690       CALL SAY("Put "+ONAME$(A)+" where? (PUT it IN, ON or UNDER something.)",1)
17700     END IF
17710     EXIT PROC
17720   END IF
17730   IF B<=0 THEN
17740     CALL SAY("Put it where? You don't see that here.",1)
17750     EXIT PROC
17760   END IF
17770   LASTOBJ=A
17780   IF B=K_SHIP THEN
17790     CALL DOINSTALL
17800     EXIT PROC
17810   END IF
17820   IF A=K_CRANK AND B=K_WELL THEN
17830     CALL ATTACHCRANK
17840     EXIT PROC
17850   END IF
17860   IF A=K_JELLY AND (B=K_GLORP OR (B=K_BED AND PREP$="UNDER")) THEN
17870     CALL FEEDGLORP
17880     EXIT PROC
17890   END IF
17900   IF A=B THEN
17910     CALL SAY("That would be a neat trick.",1)
17920     EXIT PROC
17930   END IF
17940   IF OROOM(A)<>-1 OR OIN(A)<>0 THEN
17950     ' not carried: take it first if it fits
17960     IF OSIZE(A)=0 THEN
17970       CALL SAY("You can't move "+ONAME$(A)+".",1)
17980       EXIT PROC
17990     END IF
18000     IF FN USED+OSIZE(A)>CAP THEN
18010       CALL SAY("Your hands are full. DROP something first.",1)
18020       EXIT PROC
18030     END IF
18040     M$=FN TAKEBLOCK$(A)
18050     IF M$<>"" THEN
18060       CALL SAY(M$,1)
18070       EXIT PROC
18080     END IF
18090   END IF
18100   R=1
18110   IF PREP$="ON" THEN R=2
18120   IF PREP$="UNDER" THEN R=3
18130   IF R=1 THEN
18140     IF NOT FN ISOPEN(B) THEN
18150       IF FN HASFL(B,"C") THEN
18160         CALL SAY(ONAME$(B)+" is closed.",1)
18170       ELSE
18180         CALL SAY("You can't put anything in there.",1)
18190       END IF
18200       EXIT PROC
18210     END IF
18220   END IF
18230   IF R=2 THEN
18240     IF NOT FN HASFL(B,"S") THEN
18250       CALL SAY("Nothing would stay on "+ONAME$(B)+".",1)
18260       EXIT PROC
18270     END IF
18280     IF FN HASFL(B,"H") AND NOT ONCHAIR THEN
18290       CALL SAY("You can't reach the top of "+ONAME$(B)+".",1)
18300       EXIT PROC
18310     END IF
18320   END IF
18330   IF R=3 THEN
18340     IF NOT FN HASFL(B,"U") THEN
18350       CALL SAY("You can't put anything under "+ONAME$(B)+".",1)
18360       EXIT PROC
18370     END IF
18380     OSEEN(B)=-1
18390   END IF
18400   OIN(A)=B
18410   OREL(A)=R
18420   OROOM(A)=0
18430   IF R=1 THEN CALL SAY("You put "+ONAME$(A)+" in "+ONAME$(B)+".",1)
18440   IF R=2 THEN CALL SAY("You put "+ONAME$(A)+" on "+ONAME$(B)+".",1)
18450   IF R=3 THEN CALL SAY("You push "+ONAME$(A)+" under "+ONAME$(B)+".",1)
18460   IF A=K_ICE AND B=K_SINK THEN CALL SAY("The block of ice sits in the stone sink. It could do with some warm water.",5)
18470   TCOST=1
18480   REDRAW=-1
18490 END PROC
18500 ' ---- opening and closing
18510 PROC DOOPEN
18520   LOCAL K$
18530   IF A=0 THEN
18540     IF P1$="" THEN CALL SAY("What do you want to open?",1) ELSE CALL NOSEE
18550     EXIT PROC
18560   END IF
18570   LASTOBJ=A
18580   IF NUM$<>"" AND A=K_SAFE THEN
18590     CALL DODIAL
18600     EXIT PROC
18610   END IF
18620   IF A=K_FRONTDOORO OR A=K_HALLDOOR THEN
18630     IF FRONTOPEN THEN
18640       CALL SAY("It is open already.",1)
18650     ELSE
18660       IF ROOM=R_HALL THEN
18670         FRONTOPEN=-1
18680         CALL SAY("You slide the bolt back and open the front door.",1)
18690         TCOST=1
18700         REDRAW=-1
18710       ELSE
18720         CALL SAY("The front door is bolted from the inside.",1)
18730       END IF
18740     END IF
18750     EXIT PROC
18760   END IF
18770   IF A=K_BACKDOORO OR A=K_BACKDOORI THEN
18780     IF BACKOPEN THEN
18790       CALL SAY("It is open already.",1)
18800     ELSE
18810       IF BACKLOCK AND ROOM=R_BACKYD THEN
18820         CALL SAY("The back door is locked.",1)
18830       ELSE
18840         BACKLOCK=0
18850         BACKOPEN=-1
18860         CALL SAY("You open the back door.",1)
18870         TCOST=1
18880         REDRAW=-1
18890       END IF
18900     END IF
18910     EXIT PROC
18920   END IF
18930   IF A=K_HATCHO OR A=K_HATCHI THEN
18940     IF HATCHOPEN THEN
18950       CALL SAY("It is open already.",1)
18960     ELSE
18970       IF HATCHLOCK AND ROOM=R_WESTG THEN
18980         CALL SAY("The hatch is shut with a heavy padlock.",1)
18990       ELSE
19000         HATCHLOCK=0
19010         HATCHOPEN=-1
19020         CALL SAY("You heave the hatch open.",1)
19030         TCOST=1
19040         REDRAW=-1
19050       END IF
19060     END IF
19070     EXIT PROC
19080   END IF
19090   IF A=K_SHEDDOORO THEN
19100     IF SHEDOPEN THEN
19110       CALL SAY("It is open already.",1)
19120     ELSE
19130       IF SHEDLOCK THEN
19140         CALL SAY("The shed door is shut with a brass padlock.",1)
19150       ELSE
19160         SHEDOPEN=-1
19170         CALL SAY("You open the shed door.",1)
19180         TCOST=1
19190         REDRAW=-1
19200       END IF
19210     END IF
19220     EXIT PROC
19230   END IF
19240   IF NOT FN HASFL(A,"C") THEN
19250     IF FN HASFL(A,"A") THEN
19260       CALL SAY("It has no lid or door. You can see right in.",1)
19270       CALL EXAMINE(A)
19280     ELSE
19290       CALL SAY("You can't open "+ONAME$(A)+".",1)
19300     END IF
19310     EXIT PROC
19320   END IF
19330   IF OOPEN(A) THEN
19340     CALL SAY("It is open already.",1)
19350     EXIT PROC
19360   END IF
19370   IF A=K_OVEN AND (OVENON OR OVENHOT) THEN
19380     CALL SAY("Ouch! You touch the oven door and jerk your hand back. It is far too hot. Something should be done about the heat first.",4)
19390     EXIT PROC
19400   END IF
19410   IF A=K_WASHER AND WASHON THEN
19420     CALL SAY("The round door is locked. The drum spins behind it, and something glows in there. The machine would have to stop first.",1)
19430     EXIT PROC
19440   END IF
19450   IF A=K_SAFE AND NOT SAFEOPEN THEN
19460     CALL SAY("The safe is locked. The dial wants three digits: DIAL 123, for instance.",1)
19470     EXIT PROC
19480   END IF
19490   OOPEN(A)=-1
19500   CALL SND(3)
19510   CALL SAY("You open "+ONAME$(A)+".",1)
19520   K$=FN KIDS$(A,1)
19530   IF K$<>"" THEN
19540     CALL SAY("Inside: "+K$+".",1)
19550   ELSE
19560     CALL SAY("It is empty.",6)
19570   END IF
19580   TCOST=1
19590   REDRAW=-1
19600 END PROC
19610 PROC DOCLOSE
19620   IF A=0 THEN
19630     IF P1$="" THEN CALL SAY("What do you want to close?",1) ELSE CALL NOSEE
19640     EXIT PROC
19650   END IF
19660   LASTOBJ=A
19670   IF A=K_BACKDOORO OR A=K_BACKDOORI THEN
19680     IF BACKOPEN THEN
19690       BACKOPEN=0
19700       CALL SAY("You close the back door.",1)
19710       TCOST=1
19720       REDRAW=-1
19730     ELSE
19740       CALL SAY("It is closed already.",1)
19750     END IF
19760     EXIT PROC
19770   END IF
19780   IF A=K_HATCHO OR A=K_HATCHI THEN
19790     IF HATCHOPEN THEN
19800       HATCHOPEN=0
19810       CALL SAY("You lower the hatch.",1)
19820       TCOST=1
19830       REDRAW=-1
19840     ELSE
19850       CALL SAY("It is closed already.",1)
19860     END IF
19870     EXIT PROC
19880   END IF
19890   IF A=K_SHEDDOORO THEN
19900     IF SHEDOPEN THEN
19910       SHEDOPEN=0
19920       CALL SAY("You close the shed door.",1)
19930       TCOST=1
19940       REDRAW=-1
19950     ELSE
19960       CALL SAY("It is closed already.",1)
19970     END IF
19980     EXIT PROC
19990   END IF
20000   IF A=K_FRONTDOORO OR A=K_HALLDOOR THEN
20010     CALL SAY("The front door doesn't like to be touched by anyone but Deqq.",1)
20020     EXIT PROC
20030   END IF
20040   IF NOT FN HASFL(A,"C") THEN
20050     CALL SAY("You can't close "+ONAME$(A)+".",1)
20060     EXIT PROC
20070   END IF
20080   IF NOT OOPEN(A) THEN
20090     CALL SAY("It is closed already.",1)
20100     EXIT PROC
20110   END IF
20120   OOPEN(A)=0
20130   CALL SAY("You close "+ONAME$(A)+".",1)
20140   TCOST=1
20150   REDRAW=-1
20160 END PROC
20170 ' ---- turning things on and off
20180 PROC DOTURN
20190   LOCAL T
20200   IF A=0 AND NUM$<>"" THEN
20210     CALL DODIAL
20220     EXIT PROC
20230   END IF
20240   IF A=0 THEN
20250     IF P1$="" THEN CALL SAY("Turn what?",1) ELSE CALL NOSEE
20260     EXIT PROC
20270   END IF
20280   LASTOBJ=A
20290   IF A=K_OVEN THEN
20300     IF PART$="ON" OR (PART$="" AND NOT OVENON) THEN
20310       OVENON=-1
20320       OVENHOT=-1
20330       OVENCOOL=0
20340       IF OOPEN(A) THEN OOPEN(A)=0
20350       CALL SAY("You turn the knob to ON. A red glow starts inside the oven.",1)
20360     ELSE
20370       IF OVENON THEN
20380         OVENON=0
20390         OVENCOOL=3
20400         CALL SAY("You turn the knob to OFF. The red glow fades. It will be a while before the oven has cooled down.",1)
20410       ELSE
20420         CALL SAY("The oven is off already.",1)
20430         EXIT PROC
20440       END IF
20450     END IF
20460     TCOST=1
20470     REDRAW=-1
20480     EXIT PROC
20490   END IF
20500   IF A=K_TAP THEN
20510     IF PART$="OFF" OR (PART$="" AND TAPON) THEN
20520       TAPON=0
20530       CALL SAY("You turn the tap off.",1)
20540     ELSE
20550       TAPON=-1
20560       CALL SAY("You turn the tap on. Hot water splashes into the sink and steams.",1)
20570       IF FN VIS(K_ICE) AND OIN(K_ICE)=K_SINK THEN CALL SAY("The water runs over the block of ice, which begins to creak.",5)
20580     END IF
20590     TCOST=1
20600     REDRAW=-1
20610     EXIT PROC
20620   END IF
20630   IF A=K_LAMP THEN
20640     IF PART$="OFF" OR (PART$="" AND LAMPLIT) THEN
20650       LAMPLIT=0
20660       CALL SAY("You switch the lantern off.",1)
20670     ELSE
20680       LAMPLIT=-1
20690       CALL SAY("You switch the lantern on. A warm yellow light spreads.",1)
20700     END IF
20710     TCOST=1
20720     REDRAW=-1
20730     EXIT PROC
20740   END IF
20750   IF A=K_WASHER THEN
20760     IF WASHON THEN
20770       CALL SAY("You press the STOP button. The drum does not care. The machine seems to be stuck in its cycle. Maybe it needs to lose its power.",1)
20780     ELSE
20790       CALL SAY("The machine is dead already.",1)
20800     END IF
20810     EXIT PROC
20820   END IF
20830   IF A=K_PLUG THEN
20840     CALL DOUNPLUG
20850     EXIT PROC
20860   END IF
20870   IF A=K_PHOTO THEN
20880     CALL DOTURNPHOTO
20890     EXIT PROC
20900   END IF
20910   IF A=K_SAFE THEN
20920     CALL DODIAL
20930     EXIT PROC
20940   END IF
20950   IF A=K_WELL OR A=K_CRANK THEN
20960     CALL DOWIND
20970     EXIT PROC
20980   END IF
20990   IF A=K_TV THEN
21000     CALL SAY("The television goes dark. For a moment something looks out of the black glass, then the picture is back.",1)
21010     EXIT PROC
21020   END IF
21030   CALL SAY("Nothing happens.",1)
21040 END PROC
21050 PROC DOUNPLUG
21060   IF ROOM<>R_CELLAR THEN
21070     CALL SAY("There is nothing here to unplug.",1)
21080     EXIT PROC
21090   END IF
21100   IF WASHON THEN
21110     WASHON=0
21120     CALL SAY("You pull the plug out of the wall. The washing machine shudders, the drum slows down and stops. The glow behind the window keeps shining.",1)
21130     CALL SND(5)
21140     TCOST=1
21150     REDRAW=-1
21160   ELSE
21170     CALL SAY("It is unplugged already.",1)
21180   END IF
21190 END PROC
21200 PROC ATTACHCRANK
21210   IF OROOM(K_CRANK)<>-1 THEN
21220     CALL SAY("You don't have the crank handle.",1)
21230     EXIT PROC
21240   END IF
21250   IF ROOM<>R_WELLG THEN
21260     CALL SAY("There is nothing to put it on here.",1)
21270     EXIT PROC
21280   END IF
21290   IF CRANKED THEN
21300     CALL SAY("The crank is fitted already.",1)
21310     EXIT PROC
21320   END IF
21330   CRANKED=-1
21340   OROOM(K_CRANK)=0
21350   CALL SAY("The crank handle fits the stub of the windlass exactly, as if it had been waiting. You can TURN it to wind up the bucket.",5)
21360   TCOST=1
21370   REDRAW=-1
21380 END PROC
21390 PROC DOWIND
21400   IF ROOM<>R_WELLG THEN
21410     CALL SAY("There is nothing to wind here.",1)
21420     EXIT PROC
21430   END IF
21440   IF NOT CRANKED THEN
21450     IF OROOM(K_CRANK)=-1 THEN
21460       CALL ATTACHCRANK
21470     ELSE
21480       CALL SAY("Without a crank handle there is nothing to turn. Only a rusty stub.",1)
21490     END IF
21500     EXIT PROC
21510   END IF
21520   IF WELLUP THEN
21530     CALL SAY("The bucket is up already.",1)
21540     EXIT PROC
21550   END IF
21560   WELLUP=-1
21570   OHID(K_BUCKET)=0
21580   CALL SAY("You turn the crank. The rope creaks, the old windlass groans, and after a long time an iron bucket rises into the daylight, dripping.",1)
21590   CALL SAY("Inside: "+FN KIDS$(K_BUCKET,1)+".",1)
21600   CALL SND(3)
21610   TCOST=1
21620   REDRAW=-1
21630 END PROC
21640 PROC DOUNSCREW
21650   IF A<>K_MIRROR THEN
21660     IF A>0 AND OSIZE(A)>0 THEN
21670       CALL DOTAKE
21680     ELSE
21690       CALL SAY("You can't unscrew that.",1)
21700     END IF
21710     EXIT PROC
21720   END IF
21730   IF NOT MIRRORON THEN
21740     CALL SAY("It is off the wall already.",1)
21750     EXIT PROC
21760   END IF
21770   IF OROOM(K_SCREWDRIVER)<>-1 THEN
21780     CALL SAY("The screws have four-pointed heads. Your fingernails are not the right shape.",1)
21790     EXIT PROC
21800   END IF
21810   MIRRORON=0
21820   OHID(K_PLATE)=0
21830   CALL FLOORSPOT(K_PLATE,R_BATH)
21840   CALL SAY("The strange screwdriver fits the four-pointed screws perfectly. One, two, three, four. The 'mirror' comes off the wall: a curved metal plate, scorched on its back.",5)
21850   CALL SND(2)
21860   TCOST=1
21870   REDRAW=-1
21880 END PROC
21890 ' ---- climbing, hiding, waiting
21900 PROC DOCLIMB
21910   IF A=0 THEN
21920     IF PART$="UP" THEN
21930       CALL DOMOVE(5)
21940       EXIT PROC
21950     END IF
21960     IF PART$="DOWN" THEN
21970       IF ONCHAIR THEN
21980         ONCHAIR=0
21990         REDRAW=-1
22000         CALL SAY("You step down from the chair.",1)
22010         TCOST=1
22020       ELSE
22030         CALL DOMOVE(6)
22040       END IF
22050       EXIT PROC
22060     END IF
22070     IF V=20 AND ONCHAIR THEN
22080       ONCHAIR=0
22090       REDRAW=-1
22100       CALL SAY("You step down from the chair.",1)
22110       TCOST=1
22120       EXIT PROC
22130     END IF
22140     IF V=20 AND HIDING THEN
22150       CALL UNHIDE
22160       EXIT PROC
22170     END IF
22180     CALL SAY("Climb what?",1)
22190     EXIT PROC
22200   END IF
22210   LASTOBJ=A
22220   IF A=K_CHAIR THEN
22230     IF HIDING THEN
22240       CALL SAY("Not while you hide.",1)
22250       EXIT PROC
22260     END IF
22270     IF ONCHAIR THEN
22280       CALL SAY("You are standing on it already.",1)
22290       EXIT PROC
22300     END IF
22310     IF OROOM(A)=-1 THEN
22320       OROOM(A)=ROOM
22330       CALL FLOORSPOT(A,ROOM)
22340     END IF
22350     ONCHAIR=-1
22360     REDRAW=-1
22370     CALL SAY("You put the chair down and climb onto it. It wobbles, but holds.",1)
22380     IF ROOM=R_LIVING THEN
22390       CALL SAY("From up here you can see the top of the tall cupboard: "+FN KIDS$(K_CUPBOARD,2)+".",5)
22400     ELSE
22410       CALL SAY("Not much to see up here. The ceiling is as dusty as everything else.",1)
22420     END IF
22430     TCOST=1
22440     EXIT PROC
22450   END IF
22460   IF A=K_LADDER OR A=K_STAIRSH THEN
22470     CALL DOMOVE(5)
22480     EXIT PROC
22490   END IF
22500   IF A=K_CSTAIRS THEN
22510     CALL DOMOVE(6)
22520     EXIT PROC
22530   END IF
22540   IF A=K_CUPBOARD THEN
22550     CALL SAY("You are not a squirrel. Standing on a chair would do better.",1)
22560     EXIT PROC
22570   END IF
22580   CALL SAY("You can't climb that.",1)
22590 END PROC
22600 PROC DOHIDE
22610   LOCAL S
22620   IF HIDING THEN
22630     CALL SAY("You are hiding already.",1)
22640     EXIT PROC
22650   END IF
22660   S=FN HIDESPOT(ROOM)
22670   IF A>0 AND FN HASFL(A,"X") THEN S=A
22680   IF S=0 THEN
22690     CALL SAY("There is no good place to hide here.",1)
22700     EXIT PROC
22710   END IF
22720   IF ONCHAIR THEN ONCHAIR=0
22730   HIDING=-1
22740   HIDEOBJ=S
22750   CALL SAY("You squeeze into hiding in "+ONAME$(S)+" and hold very still. You can peek out (LOOK) or COME OUT.",3)
22760   TCOST=1
22770   REDRAW=-1
22780 END PROC
22790 ' ---- giving, feeding, eating
22800 PROC FEEDGLORP
22810   IF NOT GLORPHERE THEN
22820     CALL SAY("The glorp is full already.",1)
22830     EXIT PROC
22840   END IF
22850   GLORPHERE=0
22860   OROOM(K_JELLY)=0
22870   OIN(K_JELLY)=0
22880   OROOM(K_GLORP)=R_LANDING
22890   OIN(K_GLORP)=0
22900   OHID(K_GLORP)=0
22910   OHID(K_NOZZLE)=0
22920   CALL SAY("You push the jar of green jelly toward the glorp. All three eyes light up. It slurps the jelly down, jar and all, burps in three colors and rolls happily out from under the bed, out of the door and onto the landing.",5)
22930   CALL SAY("Behind the place where it lay, something large and metallic is revealed.",5)
22940   CALL SND(4)
22950   TCOST=1
22960   REDRAW=-1
22970 END PROC
22980 PROC DOGIVE
22990   IF A=0 THEN
23000     CALL SAY("Give what to whom?",1)
23010     EXIT PROC
23020   END IF
23030   IF B=0 THEN
23040     IF A=K_JELLY AND FN VIS(K_GLORP) THEN
23050       CALL FEEDGLORP
23060     ELSE
23070       CALL SAY("Give it to whom? There is nobody here who would take it.",1)
23080     END IF
23090     EXIT PROC
23100   END IF
23110   IF B=K_GLORP THEN
23120     IF A=K_JELLY THEN
23130       CALL FEEDGLORP
23140     ELSE
23150       CALL SAY("The glorp sniffs "+ONAME$(A)+" and turns up its nose. Nose, one of them. It wants something green and wobbly.",1)
23160     END IF
23170     EXIT PROC
23180   END IF
23190   IF B=K_DEQQ THEN
23200     CALL SAY("Deqq looks at what you offer, then at you, and does not take it.",2)
23210     EXIT PROC
23220   END IF
23230   CALL SAY(ONAME$(B)+" doesn't want it.",1)
23240 END PROC
23250 PROC DOEAT
23260   IF A=0 THEN
23270     CALL SAY("Eat what?",1)
23280     EXIT PROC
23290   END IF
23300   IF A=K_FRUIT THEN
23310     OROOM(A)=0
23320     OIN(A)=0
23330     CALL SAY("You bite into the fruit. It tastes of cinnamon and thunder. Your hair stands on end and a small spark jumps from your ear. Nothing else happens.",1)
23340     TCOST=1
23350     REDRAW=-1
23360     EXIT PROC
23370   END IF
23380   CALL SAY("You'd rather not eat "+ONAME$(A)+". Not in this house.",1)
23390 END PROC
23400 PROC DONOISE
23410   IF HIDING THEN
23420     CALL SAY("You bite your tongue instead. Hiding people keep quiet.",3)
23430     EXIT PROC
23440   END IF
23450   CALL SAY("You make a noise. The house does not applaud.",1)
23460   IF DQHOME AND ROOM>=R_HALL THEN
23470     DQALERT=-1
23480     CALL SAY("Somewhere in the house, a floorboard answers.",4)
23490   END IF
23500   TCOST=1
23510 END PROC
23520 PROC DOTALK
23530   IF DQHOME AND DQROOM=ROOM AND ROOM>=R_HALL AND NOT HIDING THEN
23540     CALL SAY("The translator on Deqq's collar crackles: '"+CMT$(FN RN(NCMT))+"'",2)
23550   ELSE
23560     CALL SAY("You talk to yourself. The house listens politely.",1)
23570   END IF
23580   TCOST=1
23590 END PROC
23600 PROC DOLISTEN
23610   LOCAL N
23620   IF HIDING AND DQHOME THEN
23630     CALL SAY("Your own heartbeat. And somewhere, slow footsteps.",3)
23640     EXIT PROC
23650   END IF
23660   IF DQHOME AND ROOM>=R_HALL THEN
23670     CALL SAY("The house creaks. Something moves through it, taking its time.",3)
23680     EXIT PROC
23690   END IF
23700   IF DQHOME THEN
23710     CALL SAY("From inside the house you hear a faint, slow humming.",3)
23720     EXIT PROC
23730   END IF
23740   CALL SAY("Quiet. A clock ticks somewhere. The wind plays with the washing line.",1)
23750 END PROC
23760 PROC DOSMELL
23770   IF ROOM=R_KITCHEN THEN
23780     CALL SAY("Cinnamon, burnt metal and something that cannot be cabbage.",1)
23790   ELSE
23800     IF ROOM=R_CRASH THEN
23810       CALL SAY("Scorched metal and ozone.",1)
23820     ELSE
23830       CALL SAY("Dust, old wood, and a trace of ozone, like after lightning.",1)
23840     END IF
23850   END IF
23860 END PROC
23870 ' ---- keys, safe, dial
23880 PROC DOUNLOCK
23890   LOCAL T
23900   IF NUM$<>"" THEN
23910     CALL DODIAL
23920     EXIT PROC
23930   END IF
23940   T=A
23950   IF T=0 OR T=K_KEY THEN
23960     IF B>0 THEN T=B
23970   END IF
23980   IF T=0 THEN
23990     IF ROOM=R_BACKYD THEN T=K_BACKDOORO
24000     IF ROOM=R_WESTG THEN T=K_HATCHO
24010     IF ROOM=R_WELLG THEN T=K_SHEDDOORO
24020     IF ROOM=R_ATTIC THEN T=K_SAFE
24030   END IF
24040   IF T=0 THEN
24050     CALL SAY("Unlock what?",1)
24060     EXIT PROC
24070   END IF
24080   LASTOBJ=T
24090   IF T=K_SAFE THEN
24100     CALL SAY("The safe has no keyhole, only a dial with three digits: DIAL 123.",1)
24110     EXIT PROC
24120   END IF
24130   IF T=K_BACKDOORI OR T=K_HATCHI OR T=K_FRONTDOORO OR T=K_HALLDOOR THEN
24140     CALL SAY("You can open that without a key or a bolt. Or can't, but not with a key.",1)
24150     EXIT PROC
24160   END IF
24170   IF T<>K_BACKDOORO AND T<>K_HATCHO AND T<>K_SHEDDOORO THEN
24180     CALL SAY("You can't unlock that.",1)
24190     EXIT PROC
24200   END IF
24210   IF OROOM(K_KEY)<>-1 THEN
24220     CALL SAY("You need a key for that. Spare keys are hidden in the oddest places.",1)
24230     EXIT PROC
24240   END IF
24250   IF T=K_BACKDOORO THEN
24260     IF NOT BACKLOCK THEN
24270       CALL SAY("It is unlocked already.",1)
24280       EXIT PROC
24290     END IF
24300     BACKLOCK=0
24310     CALL SAY("The spare key turns with a click. The back door is unlocked.",1)
24320   END IF
24330   IF T=K_HATCHO THEN
24340     IF NOT HATCHLOCK THEN
24350       CALL SAY("It is unlocked already.",1)
24360       EXIT PROC
24370     END IF
24380     HATCHLOCK=0
24390     CALL SAY("The spare key opens the padlock. The hatch can be opened now.",1)
24400   END IF
24410   IF T=K_SHEDDOORO THEN
24420     IF NOT SHEDLOCK THEN
24430       CALL SAY("It is unlocked already.",1)
24440       EXIT PROC
24450     END IF
24460     SHEDLOCK=0
24470     CALL SAY("The spare key opens the padlock. The shed door can be opened now.",1)
24480   END IF
24490   CALL SND(3)
24500   TCOST=1
24510 END PROC
24520 PROC DODIAL
24530   IF ROOM<>R_ATTIC OR NOT FN VIS(K_SAFE) THEN
24540     CALL SAY("There is no dial here.",1)
24550     EXIT PROC
24560   END IF
24570   IF NUM$="" THEN
24580     CALL SAY("The dial takes three digits. DIAL 123, for instance.",1)
24590     EXIT PROC
24600   END IF
24610   IF SAFEOPEN THEN
24620     CALL SAY("The safe is open already.",1)
24630     EXIT PROC
24640   END IF
24650   TCOST=1
24660   IF NUM$="731" THEN
24670     SAFEOPEN=-1
24680     OOPEN(K_SAFE)=-1
24690     CALL SAY("You turn the dial to 7, to 3, to 1. Click, clack, clunk: the heavy door swings open. Inside: "+FN KIDS$(K_SAFE,1)+".",5)
24700     CALL SND(5)
24710     REDRAW=-1
24720   ELSE
24730     CALL SAY("You dial "+NUM$+". The dial clicks and nothing else happens. Somewhere there is a clue to the right numbers.",1)
24740   END IF
24750 END PROC
24760 ' ---- push and pull
24770 PROC DOPUSH
24780   IF A=0 THEN
24790     CALL SAY("Push what?",1)
24800     EXIT PROC
24810   END IF
24820   LASTOBJ=A
24830   IF A=K_PLUG AND V=19 THEN
24840     CALL DOUNPLUG
24850     EXIT PROC
24860   END IF
24870   IF A=K_WELL AND V=19 THEN
24880     CALL SAY("The rope is heavy and the windlass wants to be turned, not pulled.",1)
24890     EXIT PROC
24900   END IF
24910   IF A=K_WASHER THEN
24920     CALL DOTURN
24930     EXIT PROC
24940   END IF
24950   IF A=K_CHAIR THEN
24960     CALL SAY("The chair scrapes across the floor. That doesn't help anyone.",1)
24970     EXIT PROC
24980   END IF
24990   IF OSIZE(A)>0 THEN
25000     CALL SAY("Nothing happens.",1)
25010   ELSE
25020     CALL SAY("It doesn't budge.",1)
25030   END IF
25040 END PROC
25050 ' ------------------------------------------------------------------------------------------------ building the ship
25060 PROC INSTALLPART(I)
25070   LOCAL N
25080   OROOM(I)=100
25090   OIN(I)=0
25100   IF I=K_COIL THEN CALL SAY("The flux coil slides into the engine mount and begins to hum.",5)
25110   IF I=K_COMPASS THEN CALL SAY("The star compass clicks into the helm. Its needle swings and locks onto a point very far away.",5)
25120   IF I=K_CRYSTAL THEN CALL SAY("The warp crystal drops into its cradle. The air around the wreck ripples.",5)
25130   IF I=K_NOZZLE THEN CALL SAY("The thruster nozzle bolts onto the stern with a clang that rolls over the hills.",5)
25140   IF I=K_FUELCELL THEN CALL SAY("The fuel cell slots into the engine bay and a green light comes on somewhere inside the wreck.",5)
25150   IF I=K_PLATE THEN CALL SAY("The hull plate fits exactly. For the first time the wreck looks like a ship and not like a skeleton.",5)
25160   IF I=K_GLYPH THEN CALL SAY("The ignition glyph settles into the console and glows steadily.",5)
25170   IF I=K_ANTENNA THEN CALL SAY("The antenna rises from the roof and turns slowly, searching the sky.",5)
25180   CALL SND(6)
25190   N=FN BUILT
25200   CALL SAY("Parts built in: "+STR$(N)+" of 8.",7)
25210   IF N=HALF THEN
25220     CALL SAY("A deep note rolls over the hills. The wreck looks like a ship again. Far away, something turns its head toward you.",3)
25230   END IF
25240   IF N=8 THEN ENDREQ=-1
25250   REDRAW=-1
25260 END PROC
25270 FUNC LASTW$(P$)
25280   LOCAL I
25290   FOR I=LEN(P$) TO 1 STEP -1
25300     IF MID$(P$,I,1)=" " THEN RETURN MID$(P$,I+1)
25310   NEXT I
25320   RETURN P$
25330 END FUNC
25340 PROC DOTAKEALL
25350   LOCAL I,N,K$
25360   N=0
25370   FOR I=1 TO NOBJ
25380     IF OSIZE(I)>0 AND OROOM(I)<>-1 AND FN VIS(I) AND FN TAKEBLOCK$(I)="" AND FN USED+OSIZE(I)<=CAP THEN
25390       OIN(I)=0
25400       OROOM(I)=-1
25410       N=N+1
25420       IF N>1 THEN K$=K$+", "
25430       K$=K$+ONAME$(I)
25440     END IF
25450   NEXT I
25460   IF N=0 THEN
25470     CALL SAY("There is nothing here you can take, or your hands are full.",1)
25480   ELSE
25490     CALL SAY("Taken: "+K$+".",1)
25500     CALL SND(2)
25510     TCOST=1
25520     REDRAW=-1
25530   END IF
25540 END PROC
25550 PROC DODROPALL
25560   LOCAL I,N
25570   N=0
25580   FOR I=1 TO NOBJ
25590     IF OROOM(I)=-1 AND OIN(I)=0 THEN
25600       OROOM(I)=ROOM
25610       CALL FLOORSPOT(I,ROOM)
25620       N=N+1
25630     END IF
25640   NEXT I
25650   IF N=0 THEN
25660     CALL SAY("You carry nothing.",1)
25670   ELSE
25680     CALL SAY("You put everything down.",1)
25690     TCOST=1
25700     REDRAW=-1
25710   END IF
25720 END PROC
25730 PROC DOINSTALL
25740   LOCAL I,N
25750   IF ROOM<>R_CRASH THEN
25760     CALL SAY("There is no ship here to build parts into. The wreck lies north of the back yard.",1)
25770     EXIT PROC
25780   END IF
25790   IF A>0 AND A<>K_SHIP THEN
25800     IF NOT FN HASFL(A,"P") THEN
25810       CALL SAY(ONAME$(A)+" doesn't belong to the ship.",1)
25820       EXIT PROC
25830     END IF
25840     IF OROOM(A)<>-1 THEN
25850       CALL SAY("You must carry the part to build it in.",1)
25860       EXIT PROC
25870     END IF
25880     CALL INSTALLPART(A)
25890     TCOST=1
25900     EXIT PROC
25910   END IF
25920   N=0
25930   FOR I=1 TO NOBJ
25940     IF OROOM(I)=-1 AND OIN(I)=0 AND FN HASFL(I,"P") THEN
25950       CALL INSTALLPART(I)
25960       N=N+1
25970     END IF
25980   NEXT I
25990   IF N=0 THEN
26000     CALL SAY("You carry no part of the ship. Find them in the house and the garden.",1)
26010   ELSE
26020     TCOST=1
26030   END IF
26040 END PROC
26050 ' USE x: does the obvious thing
26060 PROC DOUSE
26070   IF ROOM=R_CRASH AND (A=0 OR A=K_SHIP OR FN HASFL(A,"P")) THEN
26080     CALL DOINSTALL
26090     EXIT PROC
26100   END IF
26110   IF A=0 THEN
26120     CALL SAY("Use what?",1)
26130     EXIT PROC
26140   END IF
26150   LASTOBJ=A
26160   IF A=K_CRANK THEN
26170     CALL ATTACHCRANK
26180     EXIT PROC
26190   END IF
26200   IF A=K_SCREWDRIVER THEN
26210     IF ROOM=R_BATH AND MIRRORON THEN
26220       A=K_MIRROR
26230       CALL DOUNSCREW
26240     ELSE
26250       CALL SAY("There is nothing here with four-pointed screws.",1)
26260     END IF
26270     EXIT PROC
26280   END IF
26290   IF A=K_KEY THEN
26300     A=0
26310     CALL DOUNLOCK
26320     EXIT PROC
26330   END IF
26340   IF A=K_LAMP THEN
26350     CALL DOTURN
26360     EXIT PROC
26370   END IF
26380   IF A=K_CHAIR THEN
26390     CALL DOCLIMB
26400     EXIT PROC
26410   END IF
26420   IF A=K_JELLY AND FN VIS(K_GLORP) THEN
26430     CALL FEEDGLORP
26440     EXIT PROC
26450   END IF
26460   IF A=K_PLUG THEN
26470     CALL DOUNPLUG
26480     EXIT PROC
26490   END IF
26500   CALL SAY("You can't see how to use that here. Try a more exact command.",1)
26510 END PROC
26520 ' ------------------------------------------------------------------------------------------------ help, hints and status
26530 PROC DOHELP
26540   CALL SAY("Moving: N S E W, U (up), D (down), IN, OUT. LOOK (L), EXAMINE x (X), INVENTORY (I), TAKE x, DROP x, PUT x IN/ON/UNDER y, OPEN/CLOSE x.",5)
26550   CALL SAY("More: TURN x ON/OFF, GIVE x TO y, CLIMB x, HIDE, COME OUT, WAIT, READ x, LOOK UNDER/IN/BEHIND x, UNLOCK, DIAL 123, INSTALL x, USE x, UNPLUG x.",5)
26560   CALL SAY("Also HINT, STATUS, SAVE, LOAD, QUIT. Page up shows the older messages. Only actions that change something take a turn; Deqq counts turns.",5)
26570   CALL SAY("Goal: find the 8 parts of Deqq's ship and carry them to the wreck (north of the back yard). You can carry 6 slots.",5)
26580 END PROC
26590 PROC DOHINT
26600   LOCAL I,K
26610   K=0
26620   FOR I=1 TO 8
26630     IF K=0 AND OROOM(PARTK(I))<>100 AND OROOM(PARTK(I))<>-1 THEN K=I
26640   NEXT I
26650   IF K=0 THEN
26660     CALL SAY("You hold all the parts that are left. Carry them to the wreck, north of the back yard, and INSTALL them.",5)
26670   ELSE
26680     CALL SAY(HINT$(K),5)
26690   END IF
26700 END PROC
26710 PROC DOSTATUS
26720   CALL SAY("Parts built into the wreck: "+STR$(FN BUILT)+" of 8. Carrying "+STR$(FN USED)+" of "+STR$(CAP)+" slots. Turn "+STR$(TURN)+".",1)
26730 END PROC
26740 ' ------------------------------------------------------------------------------------------------ one command
26750 PROC EXECUTE(L$)
26760   TCOST=0
26770   CALL PARSE(L$)
26780   IF NW=0 THEN EXIT PROC
26790   IF V=0 THEN
26800     CALL SAY("I don't know the word '"+W$(1)+"'. Type HELP for the commands.",1)
26810     EXIT PROC
26820   END IF
26830   IF V=-1 THEN
26840     CALL SAY("Go where? (N S E W U D IN OUT)",1)
26850     EXIT PROC
26860   END IF
26870   IF HIDING THEN
26880     IF NOT (V=10 OR V=45 OR V=22 OR V=40 OR V=8 OR V=14 OR V=28 OR V=29 OR V=30 OR V=31 OR V=32 OR V=33 OR V=38 OR V=11) THEN
26890       CALL SAY("You are hiding. WAIT, peek (LOOK) or COME OUT.",3)
26900       EXIT PROC
26910     END IF
26920   END IF
26930   IF V=12 AND (P1$="ALL" OR P1$="EVERYTHING") THEN
26940     CALL DOTAKEALL
26950     EXIT PROC
26960   END IF
26970   IF V=9 THEN
26980     CALL SAY("Go where? (N S E W U D IN OUT)",1)
26990     EXIT PROC
27000   END IF
27010   IF V=10 AND (P1$="AROUND" OR P1$="HERE" OR P1$="ROUND") THEN
27020     CALL LOOKROOM(-1)
27030     EXIT PROC
27040   END IF
27050   IF V=13 AND (P1$="ALL" OR P1$="EVERYTHING") THEN
27060     CALL DODROPALL
27070     EXIT PROC
27080   END IF
27090   IF V=11 AND (P1$="ME" OR P1$="SELF" OR P1$="MYSELF") THEN
27100     CALL SAY("You look like somebody who should not be here, and you are carrying "+STR$(FN USED)+" of "+STR$(CAP)+" slots of things that are not yours.",1)
27110     EXIT PROC
27120   END IF
27130   IF (V=11 OR V=10) AND FN INLIST(FN LASTW$(P1$)," SKY HOUSE ROAD WALL WALLS FLOOR CEILING WINDOW WINDOWS GARDEN GATE ROOF GRASS GROUND ROOM ") THEN
27140     CALL SAY("Nothing special about it. This house has enough special things already.",1)
27150     EXIT PROC
27160   END IF
27170   IF V>8 AND NOT FN INLIST(STR$(V)," 42 44 30 31 26 28 29 33 32 ") THEN
27180     IF (P1$<>"" AND A=0) OR (P2$<>"" AND B=0) THEN
27190       CALL NOSEE
27200       EXIT PROC
27210     END IF
27220   END IF
27230   IF V>=1 AND V<=8 THEN
27240     IF V=7 AND NUM$<>"" THEN
27250       CALL DODIAL
27260     ELSE
27270       CALL DOMOVE(V)
27280     END IF
27290   END IF
27300   IF V=10 OR V=45 THEN CALL DOLOOK
27310   IF V=11 THEN
27320     IF A>0 THEN
27330       LASTOBJ=A
27340       CALL EXAMINE(A)
27350     ELSE
27360       CALL SAY("Examine what?",1)
27370     END IF
27380   END IF
27390   IF V=12 THEN CALL DOTAKE
27400   IF V=13 THEN CALL DODROP
27410   IF V=14 THEN CALL DOINV
27420   IF V=15 THEN CALL DOOPEN
27430   IF V=16 THEN CALL DOCLOSE
27440   IF V=17 THEN CALL DOTURN
27450   IF V=41 THEN
27460     PART$="ON"
27470     CALL DOTURN
27480   END IF
27490   IF V=18 OR V=19 THEN CALL DOPUSH
27500   IF V=20 THEN CALL DOCLIMB
27510   IF V=21 THEN CALL DOHIDE
27520   IF V=22 THEN
27530     IF HIDING THEN
27540       CALL SAY("You hold your breath and wait.",3)
27550     ELSE
27560       CALL SAY("Time passes.",6)
27570     END IF
27580     TCOST=1
27590   END IF
27600   IF V=23 THEN CALL DOGIVE
27610   IF V=24 THEN
27620     IF A>0 THEN
27630       LASTOBJ=A
27640       IF A=K_PHOTO THEN
27650         CALL DOTURNPHOTO
27660       ELSE
27670         CALL EXAMINE(A)
27680       END IF
27690     ELSE
27700       CALL SAY("Read what?",1)
27710     END IF
27720   END IF
27730   IF V=25 THEN CALL DOUNLOCK
27740   IF V=26 THEN CALL DODIAL
27750   IF V=27 THEN CALL DOUSE
27760   IF V=28 THEN CALL DOHELP
27770   IF V=29 THEN CALL DOHINT
27780   IF V=30 THEN REQSAVE=1
27790   IF V=31 THEN REQLOAD=1
27800   IF V=32 THEN QUITREQ=1
27810   IF V=33 THEN CALL DOSTATUS
27820   IF V=34 THEN CALL DOPUT
27830   IF V=35 THEN CALL DOUNPLUG
27840   IF V=36 THEN CALL DOUNSCREW
27850   IF V=37 THEN
27860     IF A>0 THEN
27870       LASTOBJ=A
27880       IF FN HASFL(A,"U") AND NOT OSEEN(A) THEN
27890         CALL REVEAL(A)
27900       ELSE
27910         CALL EXAMINE(A)
27920       END IF
27930     ELSE
27940       CALL LOOKROOM(-1)
27950     END IF
27960   END IF
27970   IF V=38 THEN CALL DOLISTEN
27980   IF V=39 THEN CALL DOSMELL
27990   IF V=40 THEN
28000     IF HIDING THEN
28010       CALL UNHIDE
28020     ELSE
28030       IF ONCHAIR THEN
28040         ONCHAIR=0
28050         REDRAW=-1
28060         CALL SAY("You step down from the chair.",1)
28070         TCOST=1
28080       ELSE
28090         IF EX(ROOM,8)>0 THEN CALL DOMOVE(8) ELSE CALL SAY("Leave by which way?",1)
28100       END IF
28110     END IF
28120   END IF
28130   IF V=42 THEN CALL DOTALK
28140   IF V=43 THEN CALL DOEAT
28150   IF V=44 THEN CALL DONOISE
28160   IF V=46 THEN CALL SAY("Knock, knock. Nobody answers. Or too many somebodies answer at once.",1)
28170   IF V=47 THEN CALL SAY("There is nothing here you need to lock.",1)
28180   IF V=48 THEN CALL DOWIND
28190 END PROC
28200 ' ------------------------------------------------------------------------------------------------ Deqq
28210 ' Deqq is either away (DQHOME = 0, DQT turns until he comes back) or at home inside the house (DQROOM, DQSTAY turns until he goes out again).
28220 ' While fewer than HALF parts are built into the wreck he hunts for the player: in the same room he throws the player out (or finds the
28230 ' hiding place with some luck), bolts the front door and leaves it open only when he goes out again. With HALF parts or more he only watches.
28240 ' The step toward a room: distances through the house graph (rooms HALL .. ATTIC; stairs and doors, not the garden).
28250 FUNC NEXTSTEP(F,T)
28260   LOCAL R,D,N,K,CH,BEST
28270   FOR R=0 TO 19
28280     DS(R)=-1
28290   NEXT R
28300   DS(T)=0
28310   CH=-1
28320   WHILE CH
28330     CH=0
28340     FOR R=R_HALL TO R_ATTIC
28350       IF DS(R)<0 THEN
28360         FOR D=1 TO 6
28370           N=EX(R,D)
28380           IF N>=R_HALL AND DS(N)>=0 AND DS(R)<0 THEN
28390             DS(R)=DS(N)+1
28400             CH=-1
28410           END IF
28420         NEXT D
28430       END IF
28440     NEXT R
28450   WEND
28460   BEST=F
28470   FOR D=1 TO 6
28480     N=EX(F,D)
28490     IF N>=R_HALL AND DS(N)>=0 AND DS(N)<DS(F) THEN BEST=N
28500   NEXT D
28510   RETURN BEST
28520 END FUNC
28530 ' a random neighbor of room F inside the house
28540 FUNC WANDER(F)
28550   LOCAL D,N,K
28560   K=0
28570   FOR D=1 TO 6
28580     N=EX(F,D)
28590     IF N>=R_HALL THEN
28600       K=K+1
28610       WC(K)=N
28620     END IF
28630   NEXT D
28640   IF K=0 THEN RETURN F
28650   RETURN WC(1+FN RN(K))
28660 END FUNC
28670 FUNC NEIGHBOR(F,T)
28680   LOCAL D
28690   FOR D=1 TO 6
28700     IF EX(F,D)=T THEN RETURN -1
28710   NEXT D
28720   RETURN 0
28730 END FUNC
28740 PROC THROWOUT(FOUND)
28750   IF FOUND THEN
28760     CALL SAY("The door of your hiding place opens without a sound. A long grey face looks in, with eyes like two wells at midnight. 'FOUND,' says the translator on his collar.",4)
28770   ELSE
28780     CALL SAY("Deqq turns his head all the way toward you. A long grey hand closes on your collar. 'MY HOUSE,' says the translator on his collar, quite calmly.",4)
28790   END IF
28800   CALL SAY("You fly through the hall, across the doorstep, into the gravel. The front door slams. A bolt slides shut.",4)
28810   CALL SND(7)
28820   ROOM=R_FRONT
28830   HIDING=0
28840   ONCHAIR=0
28850   FRONTOPEN=0
28860   DQROOM=R_HALL
28870   REDRAW=-1
28880   CALL LOOKROOM(0)
28890   CALL SAY("(Deqq will go out again after a while, and leave the door open. The back door and the hatch are other ways in, if you have the key. Come back with more of the ship built and he will only watch.)",6)
28900 END PROC
28910 PROC WARN(T)
28920   IF ROOM>=R_HALL THEN
28930     IF T=3 THEN CALL SAY("A thin, high humming comes from outside, rising and falling. Like something small landing.",3)
28940     IF T=2 THEN CALL SAY("Gravel crunches on the road. Slow, heavy steps are coming around the house.",3)
28950     IF T=1 THEN CALL SAY("The handle of the front door turns. Deqq is coming in! HIDE, or get out by another door.",4)
28960   ELSE
28970     IF ROOM=R_CRASH THEN
28980       IF T=3 THEN CALL SAY("A thin humming on the wind. Far off a lantern bobs along the road.",3)
28990       IF T=2 THEN CALL SAY("A tall figure passes the foot of the track. It does not look up. It does not need to.",3)
29000       IF T=1 THEN CALL SAY("In the distance a door closes: the front door of the house.",3)
29010     ELSE
29020       IF T=3 THEN CALL SAY("A thin humming on the wind. A pale light flickers over the hills.",3)
29030       IF T=2 THEN CALL SAY("Footsteps on the gravel somewhere around the house. A tall shadow slides across the windows.",3)
29040       IF T=1 THEN CALL SAY("A tall grey figure comes around the corner of the house, heading for the front door.",4)
29050     END IF
29060   END IF
29070   CALL SND(8)
29080 END PROC
29090 PROC HUM(HF)
29100   IF HL THEN EXIT PROC
29110   IF HF THEN
29120     WAVE 1,0
29130     VIBRATO 1,0.4,0.25
29140     SOUND 1,55,-1,0.09
29150   ELSE
29160     SOUND OFF 1
29170     VIBRATO 1,0,0
29180   END IF
29190 END PROC
29200 PROC ARRIVE
29210   DQHOME=-1
29220   CALL HUM(-1)
29230   DQROOM=R_HALL
29240   IF FN BUILT>=HALF THEN
29250     FRONTOPEN=-1
29260     DQSTAY=45+FN RN(15)
29270     IF ROOM>=R_HALL THEN
29280       CALL SAY("Deqq comes in through the front door and stops. He looks at you for a long moment, nods once, and stays in the shadows to watch.",2)
29290     ELSE
29300       CALL SAY("Deqq goes into the house. He leaves the front door open behind him.",3)
29310     END IF
29320   ELSE
29330     FRONTOPEN=0
29340     DQSTAY=16+FN RN(8)
29350     IF ROOM>=R_HALL THEN
29360       CALL SAY("The front door bangs open. Heavy steps in the hall. Deqq is home, and he is looking for somebody.",4)
29370     ELSE
29380       CALL SAY("Deqq goes into the house. The front door slams and a bolt slides shut behind him.",4)
29390     END IF
29400   END IF
29410   CALL SND(7)
29420   REDRAW=-1
29430 END PROC
29440 PROC DQLEAVE
29450   DQHOME=0
29460   CALL HUM(0)
29470   DQT=34+FN RN(18)
29480   FRONTOPEN=-1
29490   IF ROOM>=R_HALL THEN
29500     IF DQROOM=ROOM AND NOT HIDING THEN
29510       CALL SAY("Deqq looks at you one last time, then at the door, and goes out. He does not close it behind him.",2)
29520     ELSE
29530       CALL SAY("Heavy steps cross the hall. The front door opens and closes, no, it does not close: it stays open. The steps crunch away across the gravel.",3)
29540     END IF
29550   ELSE
29560     IF ROOM=R_FRONT THEN
29570       CALL SAY("The front door opens. Deqq steps out, looks around slowly, and walks off up the road. He leaves the door open.",3)
29580     ELSE
29590       CALL SAY("Somewhere a door opens. Footsteps crunch away along the road, and the humming fades.",3)
29600     END IF
29610   END IF
29620   DQROOM=0
29630   REDRAW=-1
29640 END PROC
29650 ' what happens when Deqq is in the same room as the player (and the player is inside the house)
29660 PROC ENCOUNTER
29670   LOCAL R
29680   IF NOT DQHOME OR ROOM<R_HALL THEN EXIT PROC
29690   IF DQROOM=ROOM THEN
29700     IF FN BUILT>=HALF THEN
29710       IF DQCOOL<=0 AND FN RN(100)<40 THEN
29720         CALL SAY(CMT$(FN RN(NCMT)),2)
29730         DQCOOL=3
29740       END IF
29750     ELSE
29760       IF HIDING THEN
29770         IF FN RN(100)<28 THEN
29780           CALL THROWOUT(-1)
29790         ELSE
29800           R=FN RN(4)
29810           IF R=0 THEN CALL SAY("A shadow slides across the crack. Deqq is standing right in front of your hiding place. You stop breathing.",3)
29820           IF R=1 THEN CALL SAY("Deqq sniffs the air. Slowly. Like a dog that has found something and is deciding whether to tell you.",3)
29830           IF R=2 THEN CALL SAY("The floorboards creak under very large feet. The steps stop. Start again. Stop.",3)
29840           IF R=3 THEN CALL SAY("Two black eyes sweep over the room like lamps. They move on. Barely.",3)
29850         END IF
29860       ELSE
29870         CALL THROWOUT(0)
29880       END IF
29890     END IF
29900   ELSE
29910     IF FN NEIGHBOR(ROOM,DQROOM) AND FN BUILT<HALF AND FN RN(100)<45 THEN
29920       CALL SAY("Heavy footsteps in the next room. They are coming closer.",3)
29930     END IF
29940   END IF
29950 END PROC
29960 PROC DQMOVE
29970   LOCAL TG,WATCH
29980   WATCH=FN BUILT>=HALF
29990   TG=0
30000   IF ROOM>=R_HALL THEN
30010     IF WATCH THEN
30020       TG=ROOM
30030     ELSE
30040       IF NOT HIDING AND (DQALERT OR FN RN(100)<50) THEN TG=ROOM
30050       IF HIDING AND FN RN(100)<20 THEN TG=ROOM
30060     END IF
30070   END IF
30080   DQALERT=0
30090   IF TG>0 AND DQROOM<>TG THEN
30100     IF NOT WATCH OR FN RN(100)<75 THEN DQROOM=FN NEXTSTEP(DQROOM,TG)
30110   ELSE
30120     IF TG=0 AND FN RN(100)<55 THEN DQROOM=FN WANDER(DQROOM)
30130   END IF
30140 END PROC
30150 PROC DEQQTURN
30160   IF NOT DQHOME THEN
30170     DQT=DQT-1
30180     IF DQT>0 AND DQT<=3 THEN CALL WARN(DQT)
30190     IF DQT<=0 THEN CALL ARRIVE
30200   ELSE
30210     DQSTAY=DQSTAY-1
30220     DQCOOL=DQCOOL-1
30230     IF FN BUILT>=HALF AND NOT FRONTOPEN THEN
30240       FRONTOPEN=-1
30250       CALL SAY("Somewhere in the house a bolt slides back. The front door swings open by itself.",3)
30260     END IF
30270     CALL DQMOVE
30280     CALL ENCOUNTER
30290     IF DQSTAY<=0 THEN CALL DQLEAVE
30300   END IF
30310   IF DQHOME THEN OROOM(K_DEQQ)=DQROOM ELSE OROOM(K_DEQQ)=0
30320 END PROC
30330 ' ------------------------------------------------------------------------------------------------ the world's clock
30340 PROC DAYLIGHT
30350   LOCAL PH,NB
30360   PH=FN MINI(100,TURN/5)
30370   NB=INT(PH/8)
30380   IF NB=DAYSTEP THEN EXIT PROC
30390   DAYSTEP=NB
30400   TNR=1-0.003*PH
30410   TNG=1-0.0018*PH
30420   TNB=1
30430   CALL SETPAL(1-0.0045*PH)
30440   REDRAW=-1
30450 END PROC
30460 PROC ENDTURN
30470   TURN=TURN+1
30480   IF OVENCOOL>0 THEN
30490     OVENCOOL=OVENCOOL-1
30500     IF OVENCOOL=0 AND NOT OVENON THEN
30510       OVENHOT=0
30520       IF ROOM=R_KITCHEN THEN CALL SAY("The oven door has cooled down. You could open it now.",6)
30530     END IF
30540   END IF
30550   IF OIN(K_ICE)=K_SINK AND TAPON THEN
30560     MELT=MELT+1
30570     IF MELT>=2 THEN
30580       OROOM(K_ICE)=0
30590       OIN(K_ICE)=0
30600       OIN(K_COMPASS)=K_SINK
30610       OREL(K_COMPASS)=1
30620       OROOM(K_COMPASS)=0
30630       IF ROOM=R_KITCHEN THEN
30640         CALL SAY("The ice cracks, slides apart and melts away in the warm water. A brass star compass lies in the sink!",5)
30650         REDRAW=-1
30660       END IF
30670     ELSE
30680       IF ROOM=R_KITCHEN THEN CALL SAY("The block of ice shrinks in the hot water.",6)
30690     END IF
30700   END IF
30710   CALL DEQQTURN
30720   CALL DAYLIGHT
30730 END PROC
30740 ' ------------------------------------------------------------------------------------------------ drawing the rooms
30750 ' the things that lie inside / on / under P (relation REL) are drawn around the room position X,Y,Z
30760 PROC KIDS(P,REL,X,Y,Z)
30770   LOCAL I,N,K
30780   IF HL THEN EXIT PROC
30790   N=0
30800   FOR I=1 TO NOBJ
30810     IF OIN(I)=P AND OREL(I)=REL AND OICON(I)>0 THEN
30820       IF FN VIS(I) THEN N=N+1
30830     END IF
30840   NEXT I
30850   IF N=0 THEN EXIT PROC
30860   K=0
30870   FOR I=1 TO NOBJ
30880     IF OIN(I)=P AND OREL(I)=REL AND OICON(I)>0 THEN
30890       IF FN VIS(I) THEN
30900         CALL ICON(OICON(I),FN PX(X+(K-(N-1)/2)*32,Z),FN PY(Y,Z),FN SCALE(Z),OCOL(I))
30910         K=K+1
30920       END IF
30930     END IF
30940   NEXT I
30950 END PROC
30960 ' things that lie on the floor of this room
30970 PROC FLOORITEMS
30980   LOCAL I
30990   IF HL THEN EXIT PROC
31000   FOR I=1 TO NOBJ
31010     IF OIN(I)=0 AND OROOM(I)=ROOM AND OICON(I)>0 THEN
31020       IF FN VIS(I) THEN CALL ICON(OICON(I),FN PX(OFX(I),OFZ(I)),FN PY(0,OFZ(I)),FN SCALE(OFZ(I)),OCOL(I))
31030     END IF
31040   NEXT I
31050 END PROC
31060 PROC TABLE(X0,X1,Z0,Z1,H,HU)
31070   IF HL THEN EXIT PROC
31080   CALL BOX(X0+4,X0+12,0,H-6,Z1-12,Z1-4,HU)
31090   CALL BOX(X1-12,X1-4,0,H-6,Z1-12,Z1-4,HU)
31100   CALL BOX(X0+4,X0+12,0,H-6,Z0+4,Z0+12,HU)
31110   CALL BOX(X1-12,X1-4,0,H-6,Z0+4,Z0+12,HU)
31120   CALL BOX(X0,X1,H-6,H,Z0,Z1,HU)
31130 END PROC
31140 ' an open container: a dark opening on the front face
31150 PROC DARKF(X0,X1,Y0,Y1,Z)
31160   CALL FRONTF(X0,X1,Y0,Y1,Z,FN CC(12,2))
31170 END PROC
31180 PROC HUD
31190   LOCAL T$
31200   IF HL THEN EXIT PROC
31210   CALL PUTLBL(5,3,RNAME$(ROOM),FN CC(11,15))
31220   T$="Parts "+STR$(FN BUILT)+"/8   Pack "+STR$(FN USED)+"/"+STR$(CAP)
31230   CALL PUTLBL(5,14,T$,FN CC(15,14))
31240   IF ONCHAIR THEN CALL PUTLBL(5,25,"(standing on the chair)",FN CC(4,14))
31250   IF HIDING THEN CALL PUTLBL(250,225,"hiding - COME OUT to leave",FN CC(4,14))
31260 END PROC
31270 ' ------------------------------------------------------------------------------------------------ interior rooms
31280 PROC DR_HALL
31290   CALL SHELL(11,1,1,1)
31300   CURWH=11
31310   FWG=1
31320   IF FRONTOPEN THEN FWG=1 ELSE FWG=2
31330   CALL DOORS(0,140,230,135,1,1)
31340   CALL DOORB(470,560,135,1,1)
31350   CALL STAIRSUP(1)
31360   ' coat closet
31370   CALL BOX(180,300,0,150,300,396,1)
31380   IF OOPEN(K_CLOSET) THEN
31390     CALL DARKF(188,292,8,142,300)
31400     CALL KIDS(K_CLOSET,1,240,60,300)
31410   ELSE
31420     CALL FRONTF(188,238,8,142,300,FN CC(1,11))
31430     CALL FRONTF(242,292,8,142,300,FN CC(1,11))
31440   END IF
31450   ' wall clock
31460   CALL FELLO(FN PX(390,400),FN PY(125,400),12,12,FN CC(11,15))
31470   GFX LINE FN PX(390,400),FN PY(125,400),FN PX(390,400),FN PY(125,400)-8,1
31480   GFX LINE FN PX(390,400),FN PY(125,400),FN PX(390,400)+5,FN PY(125,400)+2,1
31490   ' table by the wall
31500   CALL TABLE(30,120,250,330,70,1)
31510   CALL KIDS(K_HALLTABLE,2,75,70,290)
31520 END PROC
31530 PROC DR_LIVING
31540   CALL SHELL(6,1,1,0)
31550   CURWH=6
31560   FWG=0
31570   CALL TOPF(180,420,0,50,300,FN CC(2,7))
31580   CALL DOORS(0,300,380,135,1,1)
31590   ' bookshelf and tall cupboard on the back wall
31600   CALL BOX(80,250,0,120,350,396,1)
31610   CALL BOX(86,244,70,74,350,390,1)
31620   CALL BOX(86,244,100,104,350,390,1)
31630   CALL BOX(430,580,0,165,330,396,1)
31640   IF OOPEN(K_CUPBOARD) THEN
31650     CALL DARKF(438,572,8,157,330)
31660     CALL KIDS(K_CUPBOARD,1,505,70,330)
31670   ELSE
31680     CALL FRONTF(438,500,8,157,330,FN CC(1,11))
31690     CALL FRONTF(508,572,8,157,330,FN CC(1,11))
31700   END IF
31710   CALL KIDS(K_CUPBOARD,2,505,165,362)
31720   CALL KIDS(K_BOOKSHELF,2,165,120,375)
31730   ' sofa along the right wall
31740   CALL BOX(555,600,0,80,90,250,9)
31750   CALL BOX(500,555,0,38,90,250,9)
31760   CALL KIDS(K_SOFA,3,480,0,100)
31770   ' television in the left corner
31780   CALL BOX(0,45,40,110,100,200,13)
31790   GFX LINE FN PX(45,100)-0,FN PY(105,100),FN PX(45,100),FN PY(45,100),FN CC(7,12)
31800   ' round table
31810   CALL TABLE(190,330,150,260,60,1)
31820   CALL KIDS(K_LTABLE,2,260,60,205)
31830 END PROC
31840 PROC DR_KITCHEN
31850   CALL SHELL(4,0,2,0)
31860   CURWH=4
31870   FWG=0
31880   CALL DOORS(600,150,230,135,1,1)
31890   ' back door (north), stone steps down (floor hole), pantry
31900   CALL DOORB(90,170,135,1,BACKOPEN)
31910   CALL STAIRSDOWN(40,150,110,230)
31920   CALL BOX(0,60,0,140,320,396,1)
31930   IF OOPEN(K_PANTRY) THEN
31940     CALL SIDEF(60,8,132,330,390,FN CC(12,2))
31950     CALL KIDS(K_PANTRY,1,30,50,370)
31960   END IF
31970   ' fridge
31980   CALL BOX(230,310,0,125,330,396,13)
31990   IF OOPEN(K_FRIDGE) THEN
32000     CALL FRONTF(238,302,10,117,330,FN CC(0,14))
32010     CALL LINE3(238,60,330,302,60,330,FN CC(13,9))
32020     CALL KIDS(K_FRIDGE,1,270,65,330)
32030   ELSE
32040     CALL LINE3(230,88,330,310,88,330,1)
32050     CALL LINE3(300,40,330,300,70,330,FN CC(0,3))
32060   END IF
32070   ' oven
32080   CALL BOX(330,430,0,85,330,396,13)
32090   IF OOPEN(K_OVEN) THEN
32100     CALL DARKF(340,420,8,60,330)
32110     CALL KIDS(K_OVEN,1,380,20,330)
32120   ELSE
32130     IF OVENON THEN
32140       CALL FRONTF(344,416,12,56,330,FN CC(2,12))
32150     ELSE
32160       IF OVENHOT THEN
32170         CALL FRONTF(344,416,12,56,330,FN CC(3,10))
32180       ELSE
32190         CALL FRONTF(344,416,12,56,330,FN CC(7,6))
32200       END IF
32210     END IF
32220   END IF
32230   CALL FELLO(FN PX(360,330),FN PY(72,330),3,3,FN CC(0,14))
32240   CALL FELLO(FN PX(400,330),FN PY(72,330),3,3,FN CC(0,14))
32250   ' sink
32260   CALL BOX(450,590,0,85,330,396,13)
32270   CALL TOPF(465,575,85,340,390,FN CC(13,5))
32280   IF TAPON THEN CALL TOPF(470,570,85,345,385,FN CC(7,12))
32290   CALL LINE3(520,85,392,520,115,392,FN CC(13,12))
32300   CALL LINE3(520,115,392,520,115,370,FN CC(13,12))
32310   CALL KIDS(K_SINK,1,520,85,365)
32320   ' table
32330   CALL TABLE(230,380,170,260,60,1)
32340   CALL KIDS(K_KTABLE,2,305,60,215)
32350   ' low cupboard along the left wall
32360   CALL BOX(0,40,0,55,240,310,1)
32370   IF OOPEN(K_KCUPBOARD) THEN
32380     CALL SIDEF(40,6,50,248,302,FN CC(12,2))
32390     CALL KIDS(K_KCUPBOARD,1,60,40,275)
32400   END IF
32410 END PROC
32420 PROC DR_LANDING
32430   CALL SHELL(9,1,1,0)
32440   CURWH=9
32450   FWG=0
32460   CALL DOORS(0,140,230,135,1,1)
32470   CALL DOORS(600,140,230,135,1,1)
32480   CALL STAIRSDOWN(60,170,120,260)
32490   ' ladder to the attic on the back wall
32500   CALL DARKF(280,360,100,165,400)
32510   CALL LINE3(300,0,396,300,165,396,FN CC(1,12))
32520   CALL LINE3(340,0,396,340,165,396,FN CC(1,12))
32530   CALL LINE3(300,40,396,340,40,396,FN CC(1,12))
32540   CALL LINE3(300,80,396,340,80,396,FN CC(1,12))
32550   CALL LINE3(300,120,396,340,120,396,FN CC(1,12))
32560   ' crooked painting
32570   CALL FRONTF(120,200,80,140,400,FN CC(1,10))
32580   CALL FRONTF(126,194,86,134,400,FN CC(7,11))
32590   CALL FRONTF(150,176,86,110,400,FN CC(11,12))
32600   CALL FRONTF(158,168,92,102,400,FN CC(4,14))
32610   ' linen chest
32620   CALL BOX(430,560,0,50,300,396,1)
32630   IF OOPEN(K_LINEN) THEN
32640     CALL DARKF(440,550,8,44,300)
32650     CALL KIDS(K_LINEN,1,495,30,300)
32660   END IF
32670   IF FN VIS(K_GLORP) THEN CALL DRAWGLORP(250,150,0)
32680 END PROC
32690 PROC DRAWGLORP(X,Z,UNDER)
32700   LOCAL SX,SY,S
32710   IF HL THEN EXIT PROC
32720   S=FN SCALE(Z)
32730   SX=FN PX(X,Z):SY=FN PY(0,Z)
32740   CALL FELLO(SX,SY-INT(14*S),INT(24*S)+1,INT(15*S)+1,FN CC(9,9))
32750   CALL FELLO(SX-INT(9*S),SY-INT(20*S),INT(3*S)+1,INT(3*S)+1,FN CC(4,15))
32760   CALL FELLO(SX,SY-INT(23*S),INT(3*S)+1,INT(3*S)+1,FN CC(4,15))
32770   CALL FELLO(SX+INT(9*S),SY-INT(20*S),INT(3*S)+1,INT(3*S)+1,FN CC(4,15))
32780   GFX LINE SX-INT(8*S),SY-INT(8*S),SX+INT(8*S),SY-INT(8*S),1
32790   IF UNDER THEN GFX LINE SX-INT(7*S),SY-INT(8*S),SX+INT(7*S),SY-INT(8*S),FN CC(0,15)
32800 END PROC
32810 PROC DR_BEDROOM
32820   CALL SHELL(10,1,1,0)
32830   CURWH=10
32840   FWG=0
32850   CALL DOORS(0,140,230,135,1,1)
32860   ' bed against the back wall
32870   CALL BOX(290,570,0,30,200,396,1)
32880   CALL BOX(290,570,30,52,200,396,10)
32890   CALL BOX(290,570,52,95,390,396,1)
32900   CALL KIDS(K_BED,2,440,52,355)
32910   IF OSEEN(K_BED) THEN
32920     CALL DARKF(300,560,2,26,200)
32930     IF FN VIS(K_GLORP) THEN CALL DRAWGLORP(380,215,1)
32940     CALL KIDS(K_BED,3,510,2,215)
32950   END IF
32960   ' wardrobe
32970   CALL BOX(40,140,0,150,320,396,1)
32980   IF OOPEN(K_WARDROBE) THEN
32990     CALL DARKF(48,132,8,142,320)
33000     CALL KIDS(K_WARDROBE,1,90,60,320)
33010   ELSE
33020     CALL FRONTF(48,88,8,142,320,FN CC(1,11))
33030     CALL FRONTF(92,132,8,142,320,FN CC(1,11))
33040   END IF
33050   ' nightstand
33060   CALL BOX(200,265,0,55,340,396,1)
33070   IF OOPEN(K_NIGHTSTAND) THEN
33080     CALL BOX(206,259,28,44,300,340,1)
33090     CALL KIDS(K_NIGHTSTAND,1,232,44,320)
33100   ELSE
33110     CALL FRONTF(206,259,28,50,340,FN CC(1,11))
33120   END IF
33130   ' toy chest at the left wall
33140   CALL BOX(0,55,0,38,90,200,9)
33150   IF OOPEN(K_TOYCHEST) THEN
33160     CALL SIDEF(55,34,36,100,190,FN CC(12,2))
33170     CALL KIDS(K_TOYCHEST,1,28,40,145)
33180   END IF
33190 END PROC
33200 PROC DR_BATH
33210   CALL SHELL(6,7,2,0)
33220   CURWH=6
33230   FWG=0
33240   CALL DOORS(600,140,230,135,1,1)
33250   ' bathtub with curtain
33260   CALL BOX(330,590,0,48,300,396,0)
33270   CALL TOPF(342,578,48,308,390,FN CC(7,8))
33280   CALL BOX(330,590,48,150,296,300,10)
33290   ' washbasin and mirror
33300   CALL BOX(105,135,0,60,350,380,0)
33310   CALL BOX(70,170,60,72,330,396,0)
33320   CALL TOPF(80,160,72,340,390,FN CC(7,7))
33330   CALL KIDS(K_BSINK,1,120,72,365)
33340   IF MIRRORON THEN
33350     CALL FRONTF(75,165,95,155,400,FN CC(13,14))
33360     CALL FRONTF(85,155,102,148,400,FN CC(13,11))
33370     GFX PSET FN PX(78,400),FN PY(98,400),1
33380   ELSE
33390     CALL FRONTF(75,165,95,155,400,FN CC(6,10))
33400     GFX PSET FN PX(78,400),FN PY(98,400),FN CC(0,1)
33410     GFX PSET FN PX(162,400),FN PY(98,400),FN CC(0,1)
33420     GFX PSET FN PX(78,400),FN PY(152,400),FN CC(0,1)
33430     GFX PSET FN PX(162,400),FN PY(152,400),FN CC(0,1)
33440   END IF
33450   ' medicine cabinet on the left wall
33460   CALL BOX(0,22,100,150,260,330,13)
33470   IF OOPEN(K_CABINET) THEN
33480     CALL SIDEF(22,106,144,266,324,FN CC(12,2))
33490     CALL KIDS(K_CABINET,1,35,106,295)
33500   END IF
33510 END PROC
33520 PROC DR_CELLAR
33530   CALL SHELL(13,0,3,0)
33540   CURWH=13
33550   FWG=0
33560   IF FN ISDARK THEN
33570     GFX LINE 0,0,639,239,FN CC(0,1),BF
33580     CALL PUTLBL(200,110,"It is pitch dark down here.",FN CC(0,9))
33590     EXIT PROC
33600   END IF
33610   ' stairs up on the right wall, ladder to the hatch on the left wall
33620   CALL STAIRSUP(13)
33630   CALL SIDEF(0,60,165,120,200,FN CC(12,2))
33640   IF HATCHOPEN THEN CALL SIDEF(0,60,165,125,195,FN CC(7,14))
33650   CALL LINE3(0,0,130,0,165,130,FN CC(1,11))
33660   CALL LINE3(0,0,190,0,165,190,FN CC(1,11))
33670   CALL LINE3(0,40,130,0,40,190,FN CC(1,11))
33680   CALL LINE3(0,90,130,0,90,190,FN CC(1,11))
33690   CALL LINE3(0,140,130,0,140,190,FN CC(1,11))
33700   ' washing machine
33710   CALL BOX(130,240,0,100,330,396,0)
33720   CALL FELLO(FN PX(185,330),FN PY(52,330),22,22,FN CC(13,7))
33730   IF WASHON THEN
33740     CALL FELL(FN PX(185,330),FN PY(52,330),15,15,FN CC(15,13))
33750   ELSE
33760     IF OOPEN(K_WASHER) THEN
33770       CALL FELL(FN PX(185,330),FN PY(52,330),15,15,FN CC(12,1))
33780       CALL KIDS(K_WASHER,1,185,35,330)
33790     ELSE
33800       CALL FELL(FN PX(185,330),FN PY(52,330),15,15,FN CC(15,9))
33810     END IF
33820   END IF
33830   IF WASHON THEN
33840     CALL LINE3(240,12,340,330,12,340,FN CC(1,3))
33850     CALL LINE3(330,12,340,330,50,396,FN CC(1,3))
33860   ELSE
33870     CALL LINE3(240,2,340,310,2,200,FN CC(1,3))
33880   END IF
33890   ' barrels, shelf, fuse box
33900   CALL BOX(380,450,0,60,340,396,1)
33910   CALL BOX(460,530,0,65,330,396,1)
33920   CALL BOX(270,370,78,84,375,396,1)
33930   CALL KIDS(K_CSHELF,2,320,84,385)
33940   CALL BOX(594,600,70,120,200,260,13)
33950 END PROC
33960 PROC DR_ATTIC
33970   CALL SHELL(1,1,1,0)
33980   CURWH=1
33990   FWG=0
34000   CALL STAIRSDOWN(440,550,110,230)
34010   ' a skylight in the slope of the roof
34020   CALL FRONTF(250,350,95,160,400,FN CC(7,12))
34030   CALL LINE3(300,95,400,300,160,400,1)
34040   ' telescope at the skylight
34050   CALL LINE3(260,0,330,300,70,300,FN CC(4,10))
34060   CALL LINE3(300,70,300,330,100,330,FN CC(4,12))
34070   ' safe
34080   CALL BOX(50,150,30,125,350,396,13)
34090   IF SAFEOPEN THEN
34100     CALL DARKF(58,142,38,117,350)
34110     CALL KIDS(K_SAFE,1,100,40,352)
34120   ELSE
34130     CALL FELLO(FN PX(100,350),FN PY(78,350),9,9,FN CC(0,12))
34140     GFX LINE FN PX(100,350),FN PY(78,350),FN PX(100,350)+4,FN PY(78,350)-4,1
34150   END IF
34160   ' trunk, boxes, globe
34170   CALL BOX(430,560,0,50,300,396,1)
34180   CALL FRONTF(430,560,36,40,300,FN CC(4,10))
34190   IF OOPEN(K_TRUNK) THEN
34200     CALL DARKF(440,550,8,30,300)
34210     CALL KIDS(K_TRUNK,1,495,20,300)
34220   END IF
34230   CALL BOX(200,290,0,55,320,396,1)
34240   CALL BOX(170,230,0,40,250,300,1)
34250   CALL FELLO(FN PX(380,330),FN PY(70,330),14,14,FN CC(8,9))
34260   CALL BOX(372,388,0,50,322,338,1)
34270 END PROC
34280 PROC DR_SHED
34290   CALL SHELL(1,13,3,0)
34300   CURWH=1
34310   FWG=0
34320   CALL DOORS(0,140,240,135,1,SHEDOPEN)
34330   ' blueprint
34340   CALL FRONTF(220,400,70,150,400,FN CC(7,14))
34350   CALL FRONTF(224,396,74,146,400,FN CC(7,11))
34360   CALL LINE3(260,95,400,360,95,400,FN CC(0,14))
34370   CALL LINE3(300,95,400,300,135,400,FN CC(0,14))
34380   CALL FELL(FN PX(300,400),FN PY(115,400),14,5,FN CC(0,14))
34390   ' workbench and toolbox
34400   CALL TABLE(330,560,300,390,60,1)
34410   CALL BOX(340,420,60,85,330,380,2)
34420   IF OOPEN(K_TOOLBOX) THEN
34430     CALL TOPF(344,416,85,336,376,FN CC(12,2))
34440     CALL KIDS(K_TOOLBOX,1,380,86,356)
34450   END IF
34460   CALL KIDS(K_BENCHW,2,500,60,345)
34470   ' shelf of cans
34480   CALL BOX(40,170,80,86,370,396,1)
34490   CALL KIDS(K_SHELF,2,105,86,385)
34500 END PROC
34510 ' ------------------------------------------------------------------------------------------------ outdoor scenes
34520 ' sky with a gradient, two ridges of hills, grass and the gravel road in the foreground
34530 PROC EXTBASE(N)
34540   LOCAL Y,X,H
34550   IF HL THEN EXIT PROC
34560   FOR Y=0 TO 118
34570     GFX LINE 0,Y,639,Y,FN CC(7,5+INT(Y*8/118)-N*3)
34580   NEXT Y
34590   FOR X=0 TO 639 STEP 2
34600     H=104+INT(12*SIN(X/70)+7*SIN(X/23))
34610     GFX LINE X,H,X+1,118,FN CC(8,6-(N=0)*2)
34620   NEXT X
34630   FOR X=0 TO 639 STEP 2
34640     H=112+INT(7*SIN(X/50+1)+4*SIN(X/17))
34650     GFX LINE X,H,X+1,119,FN CC(5,7-(N=0)*2)
34660   NEXT X
34670   GFX LINE 0,118,639,239,FN CC(5,9),BF
34680   CALL TOPF(-300,900,0,0,40,FN CC(0,10))
34690   CALL TOPF(-300,900,0,40,110,FN CC(1,10))
34700   GFX LINE 0,233,639,239,FN CC(1,3),BF
34710 END PROC
34720 PROC EXTHOUSE(DX,DK)
34730   LOCAL YB
34740   IF HL THEN EXIT PROC
34750   CALL BOX(90,510,0,140,300,350,11)
34760   YB=FN PY(140,300)
34770   CALL TRAPH(FN PX(300,325),YB-42,YB,30,FN PX(530,300)-FN PX(300,300),FN CC(2,8))
34780   CALL BOX(430,470,140,185,310,340,2)
34790   CALL FRONTF(130,200,60,110,300,FN CC(7,13))
34800   CALL FRONTF(400,470,60,110,300,FN CC(7,13))
34810   CALL LINE3(165,60,300,165,110,300,1)
34820   CALL LINE3(435,60,300,435,110,300,1)
34830   IF DK=1 THEN
34840     CALL FRONTF(DX-30,DX+30,0,95,300,FN CC(1,9))
34850     CALL FRONTF(DX-22,DX+22,8,87,300,FN CC(1,11))
34860   END IF
34870   IF DK=2 THEN CALL DARKF(DX-30,DX+30,0,95,300)
34880   IF DK=3 THEN
34890     CALL FRONTF(DX-30,DX+30,0,95,300,FN CC(1,9))
34900     CALL FELLO(FN PX(DX+18,300),FN PY(45,300),2,2,FN CC(4,12))
34910   END IF
34920 END PROC
34930 PROC DR_FRONT
34940   FWG=-1
34950   CALL EXTBASE(0)
34960   IF FRONTOPEN THEN CALL EXTHOUSE(300,2) ELSE CALL EXTHOUSE(300,1)
34970   CALL FELLO(FN PX(300,300),FN PY(125,300),9,9,FN CC(7,13))
34980   CALL TOPF(270,330,0,60,300,FN CC(11,9))
34990   ' signpost
35000   CALL BOX(55,65,0,80,140,150,1)
35010   CALL BOX(30,95,55,80,138,142,1)
35020   ' mailbox
35030   CALL BOX(500,530,30,62,150,190,2)
35040   CALL BOX(512,518,0,30,164,170,1)
35050   IF OOPEN(K_MAILBOX) THEN
35060     CALL DARKF(504,526,34,58,150)
35070     CALL KIDS(K_MAILBOX,1,515,40,150)
35080   END IF
35090   ' the gnome
35100   CALL TRAPH(FN PX(400,170),FN PY(30,170),FN PY(0,170),4,9,FN CC(5,10))
35110   CALL FELLO(FN PX(400,170),FN PY(38,170),5,5,FN CC(10,13))
35120   CALL TRAPH(FN PX(400,170),FN PY(55,170),FN PY(42,170),1,6,FN CC(2,11))
35130 END PROC
35140 PROC DR_WESTG
35150   FWG=-1
35160   CALL EXTBASE(0)
35170   CALL EXTHOUSE(300,0)
35180   ' hatch in the ground by the wall
35190   CALL BOX(220,380,0,5,230,290,13)
35200   IF HATCHOPEN THEN
35210     CALL TOPF(228,372,5,238,282,FN CC(12,1))
35220   ELSE
35230     CALL FELLO(FN PX(300,230),FN PY(5,230),3,3,FN CC(4,12))
35240   END IF
35250   ' rain barrel
35260   CALL BOX(70,130,0,62,170,230,1)
35270   CALL TOPF(75,125,62,175,225,FN CC(5,6))
35280   ' bench
35290   CALL BOX(440,560,28,34,170,230,1)
35300   CALL BOX(445,455,0,28,175,185,1)
35310   CALL BOX(545,555,0,28,175,185,1)
35320   CALL KIDS(K_BENCH,2,500,34,200)
35330 END PROC
35340 PROC DR_BACKYD
35350   FWG=-1
35360   CALL EXTBASE(0)
35370   IF BACKOPEN THEN CALL EXTHOUSE(300,2) ELSE CALL EXTHOUSE(300,3)
35380   ' flowerpot
35390   CALL BOX(110,170,0,48,180,240,3)
35400   CALL LINE3(140,48,210,125,85,210,FN CC(5,8))
35410   CALL LINE3(140,48,210,160,80,210,FN CC(5,8))
35420   CALL FELLO(FN PX(125,210),FN PY(85,210),3,2,FN CC(5,10))
35430   IF OSEEN(K_POT) THEN
35440     CALL DARKF(115,165,2,38,180)
35450     CALL KIDS(K_POT,3,140,2,182)
35460   END IF
35470   ' washing line with capes
35480   CALL BOX(380,386,0,100,170,176,1)
35490   CALL BOX(550,556,0,100,170,176,1)
35500   CALL LINE3(383,95,173,553,95,173,FN CC(0,6))
35510   CALL BOX(410,440,50,95,170,172,0)
35520   CALL BOX(460,490,50,95,170,172,0)
35530   CALL BOX(510,540,50,95,170,172,0)
35540   ' the track north
35550   CALL TOPF(10,70,0,200,400,FN CC(1,7))
35560   CALL PUTLBL(12,95,"North",FN CC(1,12))
35570 END PROC
35580 PROC DR_WELLG
35590   FWG=-1
35600   CALL EXTBASE(0)
35610   CALL EXTHOUSE(300,0)
35620   ' tree with glowing fruit
35630   CALL BOX(75,100,0,120,200,225,1)
35640   CALL FELLO(FN PX(88,200),FN PY(145,210),44,28,FN CC(5,8))
35650   CALL FELLO(FN PX(60,200),FN PY(130,210),26,20,FN CC(5,9))
35660   CALL FELLO(FN PX(116,200),FN PY(130,210),26,20,FN CC(5,9))
35670   CALL KIDS(K_TREE,2,88,125,212)
35680   ' the well
35690   CALL BOX(235,345,0,38,160,250,0)
35700   CALL TOPF(250,330,38,170,240,FN CC(12,1))
35710   CALL BOX(235,245,38,110,195,205,1)
35720   CALL BOX(335,345,38,110,195,205,1)
35730   CALL BOX(235,345,106,114,195,205,1)
35740   IF CRANKED THEN
35750     CALL LINE3(345,95,200,370,95,200,FN CC(13,12))
35760     CALL LINE3(370,95,200,370,70,200,FN CC(2,10))
35770   ELSE
35780     CALL LINE3(345,95,200,352,95,200,FN CC(13,5))
35790   END IF
35800   IF WELLUP THEN
35810     CALL LINE3(290,110,200,290,60,200,FN CC(1,7))
35820     CALL ICON(25,FN PX(290,200),FN PY(60,200),FN SCALE(200),0)
35830     CALL KIDS(K_BUCKET,1,290,52,200)
35840   END IF
35850   ' the shed
35860   CALL BOX(430,590,0,100,150,270,1)
35870   CALL TRAPH(FN PX(510,150),FN PY(100,150)-30,FN PY(100,150),30,INT((FN PX(590,150)-FN PX(430,150))/2)+10,FN CC(1,5))
35880   IF SHEDOPEN THEN
35890     CALL DARKF(485,535,0,80,150)
35900   ELSE
35910     CALL FRONTF(485,535,0,80,150,FN CC(1,11))
35920     IF SHEDLOCK THEN CALL FELLO(FN PX(530,150),FN PY(40,150),2,2,FN CC(4,12))
35930   END IF
35940 END PROC
35950 PROC DR_CRASH
35960   LOCAL N,I,X,Y,SC,K
35970   FWG=-1
35980   CALL EXTBASE(1)
35990   GFX LINE 0,118,639,239,FN CC(1,6),BF
36000   CALL TOPF(-300,900,0,0,400,FN CC(1,6))
36010   CALL FELL(320,150,230,32,FN CC(0,3))
36020   N=FN BUILT
36030   SC=FN SCALE(200)
36040   ' the wreck: a burnt fuselage, ribs, and a mount for each part (a faint ring while the part is missing)
36050   IF N=8 THEN K=13 ELSE K=5
36060   CALL FELLO(FN PX(300,200),FN PY(58,200),INT(290*SC),INT(52*SC),FN CC(13,K))
36070   CALL FELL(FN PX(300,200),FN PY(66,200),INT(260*SC),INT(34*SC),FN CC(13,K+2))
36080   FOR I=1 TO 5
36090     X=120+I*60
36100     GFX LINE FN PX(X,200),FN PY(10,200),FN PX(X,200),FN PY(104,200),FN CC(13,3)
36110   NEXT I
36120   CALL BOX(250,350,0,12,150,230,0)
36130   FOR I=1 TO 8
36140     X=MNX(I)
36150     Y=MNY(I)
36160     IF OROOM(PARTK(I))=100 THEN
36170       CALL ICON(OICON(PARTK(I)),FN PX(X,200),FN PY(Y,200),SC*1.3,0)
36180     ELSE
36190       GFX CIRCLE FN PX(X,200),FN PY(Y+14,200),INT(14*SC),FN CC(0,6)
36200     END IF
36210   NEXT I
36220   IF N=8 THEN CALL FELLO(FN PX(300,200),FN PY(58,200),INT(80*SC),INT(20*SC),FN CC(15,13))
36230   CALL PUTLBL(190,225,"The wreck: "+STR$(N)+" of 8 parts built in",FN CC(15,14))
36240 END PROC
36250 ' ------------------------------------------------------------------------------------------------ the whole scene
36260 PROC DRAWSCENE
36270   LOCAL X,Z
36280   IF HL THEN EXIT PROC
36290   CALL BEGINDRAW
36300   CURWH=11
36310   FWG=0
36320   IF ROOM=R_FRONT THEN CALL DR_FRONT
36330   IF ROOM=R_WESTG THEN CALL DR_WESTG
36340   IF ROOM=R_BACKYD THEN CALL DR_BACKYD
36350   IF ROOM=R_WELLG THEN CALL DR_WELLG
36360   IF ROOM=R_SHED THEN CALL DR_SHED
36370   IF ROOM=R_CRASH THEN CALL DR_CRASH
36380   IF ROOM=R_HALL THEN CALL DR_HALL
36390   IF ROOM=R_LIVING THEN CALL DR_LIVING
36400   IF ROOM=R_KITCHEN THEN CALL DR_KITCHEN
36410   IF ROOM=R_LANDING THEN CALL DR_LANDING
36420   IF ROOM=R_BEDROOM THEN CALL DR_BEDROOM
36430   IF ROOM=R_BATH THEN CALL DR_BATH
36440   IF ROOM=R_CELLAR THEN CALL DR_CELLAR
36450   IF ROOM=R_ATTIC THEN CALL DR_ATTIC
36460   IF NOT FN ISDARK THEN
36470     CALL FLOORITEMS
36480     IF ONCHAIR THEN CALL ICON(16,FN PX(300,60),FN PY(0,60),FN SCALE(60)*1.8,0)
36490     IF DQHOME AND DQROOM=ROOM AND ROOM>=R_HALL THEN
36500       X=110
36510       Z=290
36520       IF ROOM=R_KITCHEN OR ROOM=R_LIVING THEN
36530         X=330
36540         Z=290
36550       END IF
36560       CALL DEQQFIG(X,Z,-(FN BUILT>=HALF))
36570     END IF
36580   END IF
36590   IF FWG>=0 THEN CALL FRONTWALL(CURWH,FWG)
36600   IF HIDING THEN
36610     GFX LINE 0,0,639,84,1,BF
36620     GFX LINE 0,160,639,239,1,BF
36630     GFX LINE 0,85,50,159,1,BF
36640     GFX LINE 590,85,639,159,1,BF
36650   END IF
36660   CALL HUD
36670   CALL ENDDRAW
36680 END PROC
36690 ' ------------------------------------------------------------------------------------------------ start of a game
36700 PROC INITHUES
36710   LOCAL I
36720   FOR I=0 TO 15
36730     READ HBR(I),HBG(I),HBB(I)
36740   NEXT I
36750 END PROC
36760 LABEL HUEDATA
36770 DATA 170,172,180, 150,98,52, 205,52,48, 235,140,40, 226,192,84, 64,150,70, 50,150,150, 96,160,230
36780 DATA 60,80,170, 130,70,170, 230,120,165, 222,205,165, 80,50,110, 120,140,160, 150,205,70, 70,240,170
36790 PROC INITSOUND
36800   IF HL THEN EXIT PROC
36810   SFX 1,"W3 V0.15 ML X0.002,0.01,0.2,0.05 S700,300,24"
36820   SFX 2,"W2 V0.3 ML X0,0.01,0.3,0.08 S600,1000,16"
36830   SFX 3,"W1 V0.15 ML ~6,0.3 X0.01,0.05,0.3,0.1 S180,120,6"
36840   SFX 4,"W2 V0.4 ML ~8,0.3 S300,900,4"
36850   SFX 5,"W3 V0.4 ML X0,0.02,0.4,0.3 S300,60,8"
36860   SFX 6,"W2 V0.4 ML X0,0.01,0.5,0.4 S800,1600,5"
36870   SFX 7,"W3 V0.7 ML X0,0.01,0.5,0.4 S200,40,3"
36880   SFX 8,"W0 V0.3 ML ~3,0.5 X0.3,0.2,0.5,0.8 S60,90,2"
36890 END PROC
36900 PROC NEWGAME
36910   LOCAL I
36920   FOR I=1 TO NROOM
36930     RSEEN(I)=0
36940   NEXT I
36950   ROOM=R_FRONT
36960   TURN=0
36970   CAP=6
36980   HALF=4
36990   FRONTOPEN=-1
37000   BACKLOCK=-1:BACKOPEN=0
37010   HATCHLOCK=-1:HATCHOPEN=0
37020   SHEDLOCK=-1:SHEDOPEN=0
37030   OVENON=-1:OVENHOT=-1:OVENCOOL=0
37040   TAPON=0:MELT=0
37050   WASHON=-1
37060   GLORPHERE=-1
37070   MIRRORON=-1
37080   SAFEOPEN=0
37090   CRANKED=0:WELLUP=0
37100   LAMPLIT=0
37110   PHOTOBACK=0
37120   ONCHAIR=0:HIDING=0:HIDEOBJ=0
37130   DQHOME=0:DQROOM=0:DQT=30+FN RN(10):DQSTAY=0:DQCOOL=0:DQALERT=0:DQEYES=0
37140   LASTOBJ=0
37150   REDRAW=-1
37160   ENDREQ=0
37170   NH=0
37180   TNR=1:TNG=1:TNB=1
37190   DAYSTEP=-1
37200   PARTK(1)=K_COIL:PARTK(2)=K_COMPASS:PARTK(3)=K_CRYSTAL:PARTK(4)=K_NOZZLE
37210   MNX(1)=450:MNY(1)=40:MNX(2)=150:MNY(2)=78:MNX(3)=300:MNY(3)=62:MNX(4)=555:MNY(4)=44
37220   MNX(5)=240:MNY(5)=24:MNX(6)=385:MNY(6)=92:MNX(7)=200:MNY(7)=42:MNX(8)=335:MNY(8)=110
37230   PARTK(5)=K_FUELCELL:PARTK(6)=K_PLATE:PARTK(7)=K_GLYPH:PARTK(8)=K_ANTENNA
37240   HINT$(1)="The oven is the warmest place in the house, and warm things stay hidden. It is hot, though: turn the knob off and be patient."
37250   HINT$(2)="The fridge holds something frozen. Warm water would help: the kitchen has a sink with a tap."
37260   HINT$(3)="Something glows on top of the tall cupboard in the living room. You will need something to stand on."
37270   HINT$(4)="Look under the bed. The creature there guards something, and it eats only one kind of food: the cookbook and the shopping list say which."
37280   HINT$(5)="The cellar is dark: the lantern in the hall helps. The washing machine is running, and machines stop when they lose their power."
37290   HINT$(6)="The bathroom mirror is not glass. Four screws hold it, and they need an alien screwdriver. Sheds have tools."
37300   HINT$(7)="The safe in the attic wants a three-digit code. Somebody scratched it onto the back of something in the living room."
37310   HINT$(8)="The bucket is stuck deep in the well, and the windlass has no crank. The shed has one, the spare key is under something crooked in the back yard."
37320 END PROC
37330 ' ------------------------------------------------------------------------------------------------ saving and loading
37340 PROC SAVEGAME
37350   LOCAL I
37360   OPEN "local:deqq.sav" FOR OUTPUT AS #1
37370   PRINT #1,"DEQQ1"
37380   WRITE #1,ROOM,TURN,FRONTOPEN,BACKLOCK,BACKOPEN,HATCHLOCK,HATCHOPEN,SHEDLOCK,SHEDOPEN
37390   WRITE #1,OVENON,OVENHOT,OVENCOOL,TAPON,MELT,WASHON,GLORPHERE,MIRRORON,SAFEOPEN
37400   WRITE #1,CRANKED,WELLUP,LAMPLIT,PHOTOBACK,ONCHAIR,HIDING,HIDEOBJ,DQHOME,DQROOM
37410   WRITE #1,DQT,DQSTAY,DQCOOL,LASTOBJ
37420   FOR I=1 TO NROOM
37430     PRINT #1,RSEEN(I)
37440   NEXT I
37450   FOR I=1 TO NOBJ
37460     WRITE #1,OROOM(I),OIN(I),OREL(I),OOPEN(I),OHID(I),OSEEN(I),OFX(I),OFZ(I)
37470   NEXT I
37480   CLOSE #1
37490 END PROC
37500 FUNC LOADGAME
37510   LOCAL I,H$
37520   IF NOT EXISTS("local:deqq.sav") THEN RETURN 0
37530   OPEN "local:deqq.sav" FOR INPUT AS #1
37540   LINE INPUT #1,H$
37550   IF H$<>"DEQQ1" THEN
37560     CLOSE #1
37570     RETURN 0
37580   END IF
37590   INPUT #1,ROOM,TURN,FRONTOPEN,BACKLOCK,BACKOPEN,HATCHLOCK,HATCHOPEN,SHEDLOCK,SHEDOPEN
37600   INPUT #1,OVENON,OVENHOT,OVENCOOL,TAPON,MELT,WASHON,GLORPHERE,MIRRORON,SAFEOPEN
37610   INPUT #1,CRANKED,WELLUP,LAMPLIT,PHOTOBACK,ONCHAIR,HIDING,HIDEOBJ,DQHOME,DQROOM
37620   INPUT #1,DQT,DQSTAY,DQCOOL,LASTOBJ
37630   FOR I=1 TO NROOM
37640     INPUT #1,RSEEN(I)
37650   NEXT I
37660   FOR I=1 TO NOBJ
37670     INPUT #1,OROOM(I),OIN(I),OREL(I),OOPEN(I),OHID(I),OSEEN(I),OFX(I),OFZ(I)
37680   NEXT I
37690   CLOSE #1
37700   DAYSTEP=-1
37710   CALL HUM(DQHOME)
37720   RETURN -1
37730 END FUNC
37740 ' ------------------------------------------------------------------------------------------------ title and ending
37750 PROC TITLE
37760   LOCAL I,X,Y
37770   IF HL THEN EXIT PROC
37780   CALL BEGINDRAW
37790   FOR Y=0 TO 239
37800     GFX LINE 0,Y,639,Y,FN CC(8,1+INT(Y/22))
37810   NEXT Y
37820   FOR I=1 TO 90
37830     GFX PSET FN RN(640),FN RN(150),FN CC(0,10+FN RN(6))
37840   NEXT I
37850   ' a crooked house on a hill, a small saucer in the sky
37860   GFX LINE 0,200,639,239,FN CC(5,3),BF
37870   CALL FELL(320,215,330,28,FN CC(5,4))
37880   CALL RECTO(250,150,390,205,FN CC(12,4))
37890   CALL TRAPH(320,115,150,20,95,FN CC(12,3))
37900   CALL RECTO(305,170,335,205,FN CC(1,5))
37910   CALL FELLO(320,135,9,9,FN CC(4,13))
37920   CALL RECTO(268,162,288,180,FN CC(4,12))
37930   CALL RECTO(352,162,372,180,FN CC(4,11))
37940   CALL FELLO(560,110,30,9,FN CC(13,9))
37950   CALL FELLO(560,103,14,10,FN CC(7,13))
37960   GFX LINE 545,120,490,170,FN CC(4,9)
37970   GFX LINE 575,120,610,190,FN CC(4,9)
37980   GFX TEXT 82,12,"DEQQ'S HOUSE",1,5,5
37990   GFX TEXT 80,10,"DEQQ'S HOUSE",FN CC(15,14),5,5
38000   CALL PUTLBL(210,222,"A graphics adventure",FN CC(0,13))
38010   CALL ENDDRAW
38020 END PROC
38030 PROC INTRO
38040   CALL TITLE
38050   IF NOT HL THEN PLAY "T50 W2 V0.22 O3 L2 A E- A F","T50 W0 V0.15 O2 L1 A E"
38060   CALL SAY("Deqq the alien crash-landed here and hid the eight parts of his ship in and around his house. Build the ship at the wreck behind the house.",1)
38070   CALL SAY("He is out on an errand and left the front door open, but he comes home now and then. HELP lists the commands. Press a key.",5)
38080   CALL FLUSH
38090   CALL WAITKEY
38100   IF PLAYON AND ENV$("DEQQ_SHOTDIR")<>"" THEN CALL MARK("TITLE")
38110 END PROC
38120 PROC MARK(N$)
38130   OPEN ENV$("DEQQ_SHOTDIR")+"/"+N$+".mark" FOR OUTPUT AS #5
38140   PRINT #5,"x"
38150   CLOSE #5
38160   WAIT 45
38170 END PROC
38180 PROC LIFTOFF(WITHDEQQ)
38190   LOCAL K,Y,X,I
38200   IF HL THEN EXIT PROC
38210   FOR K=0 TO 70
38220     CALL BEGINDRAW
38230     FOR Y=0 TO 239
38240       GFX LINE 0,Y,639,Y,FN CC(8,1+INT(Y/30))
38250     NEXT Y
38260     FOR I=1 TO 40
38270       X=(I*97+K*3) MOD 640
38280       GFX PSET X,(I*53) MOD 200,FN CC(0,8+(I MOD 7))
38290     NEXT I
38300     GFX LINE 0,220,639,239,FN CC(5,3),BF
38310     Y=200-K*4
38320     X=320
38330     CALL TRAPH(X,Y-34,Y,3,12,FN CC(0,13))
38340     CALL FELLO(X,Y+4,12,8,FN CC(13,9))
38350     CALL FELLO(X,Y-26,5,5,FN CC(7,13))
38360     CALL TRAPH(X,Y+10,Y+10+INT(20+FN RN(18)),9,1,FN CC(4,13))
38370     CALL TRAPH(X,Y+10,Y+10+INT(10+FN RN(8)),5,1,FN CC(15,15))
38380     CALL ENDDRAW
38390     IF K=1 THEN CALL SND(5)
38400     WAIT 2
38410   NEXT K
38420 END PROC
38430 PROC ENDSCENE
38440   CALL HUM(0)
38450   IF DQHOME THEN
38460     CALL SAY("Footsteps in the gravel behind you. Deqq stands at the edge of the hollow, taller than you remembered, and for a long time he says nothing.",1)
38470     CALL SAY("'You found them,' says the translator at last. 'All eight. I hid them well. Nobody ever...' The big black eyes shine. 'Thank you.'",2)
38480     CALL SAY("He walks to the ship, puts a long hand on the hull as if on the neck of an animal, and climbs in. The hatch seals. The engines wake with a sound like a huge, happy cat.",1)
38490     CALL SAY("The ship rises over the hills and is gone. On the gatepost of the house a key is lying now, and a note: 'YOURS. DON'T OPEN THE BATHROOM CABINET.'",2)
38500     CALL FLUSH
38510     CALL LIFTOFF(-1)
38520   ELSE
38530     CALL SAY("The eighth part clicks into place and the wreck wakes. Lights run through the hull, the engines hum, the hatch opens like a welcome.",1)
38540     CALL SAY("You climb in. The console knows what you want before you do. The ship rises, shakes the dust of the hollow from its hull, and climbs.",1)
38550     CALL SAY("Far below on the road a tall grey figure with a shopping bag stands still and looks up. 'I hid them SO well,' says a very small voice in your head.",2)
38560     CALL FLUSH
38570     CALL LIFTOFF(0)
38580   END IF
38590   IF NOT HL THEN PLAY "T120 W2 V0.3 O4 L8 C E G >C4 <G >C2","T120 W0 V0.2 O2 L2 C G C"
38600   CALL SAY("THE END.  You built the ship in "+STR$(TURN)+" turns. Thanks for playing Deqq's House!",7)
38610   CALL FLUSH
38620   CALL WAITKEY
38630   RUNNING=0
38640 END PROC
38650 ' ------------------------------------------------------------------------------------------------ test commands (only when a play-back file is used)
38660 PROC DEBUGCMD(L$)
38670   LOCAL C$,P,K$,I
38680   CALL SPLITW(MID$(L$,2))
38690   C$=W$(1)
38700   IF C$="T" THEN
38710     DQT=VAL(W$(2))
38720     CALL SAY("[deqq in "+STR$(DQT)+" turns]",6)
38730   END IF
38740   IF C$="STAY" THEN DQSTAY=VAL(W$(2))
38750   IF C$="DEQQ" THEN
38760     DQHOME=-1
38770     DQROOM=FN RID(W$(2))
38780     DQSTAY=999
38790     OROOM(K_DEQQ)=DQROOM
38800     REDRAW=-1
38810   END IF
38820   IF C$="GOTO" THEN
38830     ROOM=FN RID(W$(2))
38840     REDRAW=-1
38850     HIDING=0
38860     CALL LOOKROOM(0)
38870   END IF
38880   IF C$="HAVE" THEN
38890     I=FN OID(W$(2))
38900     OROOM(I)=-1
38910     OIN(I)=0
38920     OHID(I)=0
38930   END IF
38940   IF C$="WAIT" THEN WAIT VAL(W$(2))
38950   IF C$="STATE" THEN
38960     CALL SAY("[room "+STR$(ROOM)+" turn "+STR$(TURN)+" built "+STR$(FN BUILT)+" deqq home "+STR$(DQHOME)+" in "+STR$(DQROOM)+" t="+STR$(DQT)+" stay="+STR$(DQSTAY)+" front open "+STR$(FRONTOPEN)+" hiding "+STR$(HIDING)+"]",6)
38970   END IF
38980   IF C$="SEED" THEN RANDOMIZE VAL(W$(2))
38990   IF C$="TIME" THEN
39000     I=TI
39010     CALL DRAWSCENE
39020     CALL SAY("[drawing took "+STR$(TI-I)+" jiffies]",6)
39030   END IF
39040   IF C$="SHOT" THEN
39050     IF REDRAW THEN CALL DRAWSCENE
39060     REDRAW=0
39070     CALL FLUSH
39080     CALL MARK(W$(2))
39090   END IF
39100 END PROC
39110 ' ------------------------------------------------------------------------------------------------ main program
39120 LABEL MAIN
39130   HL=ENV$("DEQQ_HEADLESS")<>""
39140   IF ENV$("DEQQ_SYNTAXONLY")<>"" THEN BYE
39150   RANDOMIZE
39160   PLAYON=0
39170   LOGON=0
39180   IF ENV$("DEQQ_PLAY")<>"" THEN
39190     OPEN ENV$("DEQQ_PLAY") FOR INPUT AS #4
39200     PLAYON=-1
39210   END IF
39220   IF ENV$("DEQQ_LOG")<>"" THEN
39230     OPEN ENV$("DEQQ_LOG") FOR OUTPUT AS #3
39240     LOGON=-1
39250   END IF
39260   RESTORE HUEDATA
39270   CALL INITHUES
39280   BK=3
39290   FR=0
39300   IF NOT HL THEN
39310     GFX RESET
39320     GFX BACKDROP RGB(6,6,14)
39330     GFX LAYER 0,ON
39340     GFX LAYER 3,ON
39350     MOUSE HIDE
39360     BREAK OFF
39370   END IF
39380   CALL LOADWORLD
39390   CALL NEWGAME
39400   CALL INITSOUND
39410   CALL TEXTINIT
39420   CALL SETPAL(1)
39430   CALL INTRO
39440   RUNNING=-1
39450   CALL LOOKROOM(-1)
39460   CALL DRAWSCENE
39470   REDRAW=0
39480   CALL FLUSH
39490   WHILE RUNNING
39500     LN$=FN GETLINE$
39510     TCOST=0
39520     IF LN$<>"" THEN
39530       CALL ADDLINE("> "+LN$,6)
39540       IF PLAYON AND LEFT$(LN$,1)="#" THEN
39550         CALL DEBUGCMD(LN$)
39560       ELSE
39570         CALL EXECUTE(LN$)
39580       END IF
39590       IF REQSAVE THEN
39600         REQSAVE=0
39610         ON ERROR GOTO SVERR
39620         CALL SAVEGAME
39630         ON ERROR OFF
39640         CALL SAY("Game saved.",1)
39650       END IF
39660       LABEL SVDONE
39670       IF REQLOAD THEN
39680         REQLOAD=0
39690         IF FN LOADGAME THEN
39700           CALL SAY("Game loaded.",1)
39710           REDRAW=-1
39720           CALL LOOKROOM(-1)
39730         ELSE
39740           CALL SAY("There is no saved game.",1)
39750         END IF
39760       END IF
39770       IF QUITREQ THEN
39780         QUITREQ=0
39790         CALL SAY("Really quit the game? (y/n)",3)
39800         CALL FLUSH
39810         LN$=FN GETLINE$
39820         IF FN UC$(LEFT$(LN$,1))="Y" THEN RUNNING=0
39830       END IF
39840       IF TCOST THEN CALL ENDTURN
39850       IF ENDREQ AND RUNNING THEN
39860         CALL DRAWSCENE
39870         CALL ENDSCENE
39880       END IF
39890       IF REDRAW AND RUNNING THEN
39900         CALL DRAWSCENE
39910         REDRAW=0
39920       END IF
39930       CALL FLUSH
39940     END IF
39950     IF PLAYEND THEN RUNNING=0
39960   WEND
39970   IF LOGON THEN CLOSE #3
39980   IF PLAYON THEN CLOSE #4
39990   IF NOT HL THEN
40000     SOUND OFF
40010     BREAK ON
40020     GFX RESET
40030     CLS
40040   END IF
40050   PRINT "Thanks for playing Deqq's House!"
40060   END
40070 LABEL SVERR
40080   ON ERROR OFF
40090   CALL SAY("The game could not be saved here.",3)
40100   RESUME SVDONE
