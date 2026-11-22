1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' HOUSEGEN - a generator of German family houses (Einfamilienhaus) for the game 1987, drawn as contact sheets: the front, the gable side, the ground floor and the upper floor of a house,
20 ' intact and damaged. It is the design study for the ruins of the static world (docs/world-subsystem.md "Ruins"): it fixes WHAT a house is before the 3D builder is written.
30 ' Run it in the window version: LOAD "tools/housegen", RUN. Every page is shown (SPACE: next page, Q: quit) and saved as housegen-1.bmp ... housegen-5.bmp in the current folder
40 ' (the environment variable HOUSEGEN_DIR names another folder, HOUSEGEN_AUTO=1 shows no wait, HOUSEGEN_SEED=n shifts all seeds). Units: 1 dm = 10 cm; a drawing unit is 1.2 pixels.
50 ' THE HOUSE: 10 .. 12 m (long side, the ridge runs along it) by 8 .. 10 m, two storeys of 2.6 m (plinth 0.3 m, slab 0.3 m), a knee wall of 0 .. 0.6 m, a gable roof of 27 .. 43 degrees,
60 ' the ridge 8.5 .. 10 m high, a chimney. Walls 36.5 cm of gray brick, plaster painted outside (6 colors) and inside (6 colors). The plan: a hall column with the stairs along one wall
70 ' and a WC (the bathroom above), a living side (living room at the garden side, dining room at the street side, open to each other) and a service side (kitchen, study).
80 ' Upstairs: one long room over the living side, the bedroom and a children's room on the service side. Furniture is wooden boxes. A damage value D (0 .. 1) burns it:
90 ' 0 intact, 0.3 scorched, 0.6 roof burnt, the upper floor falling in, 0.9 a shell, 1 rubble; BEAM adds the round hole with a glassy rim of a directed energy weapon.
100 DIM RM(2,8,6),RMN(2),DOR(2,16,4),DON(2),WIN(40,7),FU(2,64,5),FUC(2,64),FUN(2),CL(2,24,4),CLN(2),NM$(10),FLZ(2),PGS(6,2,3)
110 ON ERROR GOTO FAILED
120 SC=1.2:TW=4:FLZ(0)=3:FLZ(1)=32:RC=0:SD=1:WN=0:PW=0:OKP=0
130 GOTO MAIN
140 ' ------------------------------------------------------------------------------------------------ random numbers (from the seed and a counter: the same house every time)
150 FUNC RN()
160   RC=RC+1
170   RETURN HASH(SD*7919,RC,17)
180 END FUNC
190 FUNC RI(A,B)
200   RETURN A+INT(FN RN()*(B-A+1))
210 END FUNC
220 ' the pixel column of the horizontal house coordinate A (dm) in the view being drawn (EVM 0 front, 1 the gable side seen from the west: y runs to the left), and the pixel row of a height Z
230 FUNC XP(A)
240   IF EVM=0 THEN RETURN EOX+INT(A*SC+0.5)
250   RETURN EOX+INT((EAW-A)*SC+0.5)
260 END FUNC
270 FUNC ZP(Z)
280   RETURN EGY-INT(Z*SC+0.5)
290 END FUNC
300 ' the plan's pixel column / row (the street side, y = 0, is at the bottom)
310 FUNC PX(A)
320   RETURN POX+INT(A*SC+0.5)
330 END FUNC
340 FUNC PY(A)
350   RETURN POY+INT((DY-A)*SC+0.5)
360 END FUNC
370 ' is the box free of furniture and of the door and stair clearances of floor F? (-1 yes, 0 no)
380 FUNC FREEBOX(F,X1,Y1,X2,Y2)
390   LOCAL I
400   FOR I=0 TO FUN(F)-1
410     IF X1<FU(F,I,2) AND FU(F,I,0)<X2 AND Y1<FU(F,I,3) AND FU(F,I,1)<Y2 THEN RETURN 0
420   NEXT I
430   FOR I=0 TO CLN(F)-1
440     IF X1<CL(F,I,2) AND CL(F,I,0)<X2 AND Y1<CL(F,I,3) AND CL(F,I,1)<Y2 THEN RETURN 0
450   NEXT I
460   RETURN -1
470 END FUNC
480 ' ------------------------------------------------------------------------------------------------ the house: parts
490 PROC ADDROOM(F,X1,Y1,X2,Y2,T)
500   LOCAL N
510   N=RMN(F)
520   RM(F,N,0)=X1:RM(F,N,1)=Y1:RM(F,N,2)=X2:RM(F,N,3)=Y2:RM(F,N,4)=T:RM(F,N,5)=20+FN RI(0,5)
530   RMN(F)=N+1
540 END PROC
550 ' a door in a partition: AX 0 a wall at x = X (1 dm thick) with the door centred at y = Y, AX 1 a wall at y = Y with the door centred at x = X; W wide
560 PROC ADDDOOR(F,AX,X,Y,W)
570   LOCAL N,C
580   N=DON(F):DOR(F,N,0)=AX:DOR(F,N,1)=X:DOR(F,N,2)=Y:DOR(F,N,3)=W:DON(F)=N+1
590   C=CLN(F)
600   IF AX=0 THEN
610     CL(F,C,0)=X-9:CL(F,C,1)=Y-W/2:CL(F,C,2)=X+10:CL(F,C,3)=Y+W/2
620   ELSE
630     CL(F,C,0)=X-W/2:CL(F,C,1)=Y-9:CL(F,C,2)=X+W/2:CL(F,C,3)=Y+10
640   END IF
650   CLN(F)=C+1
660 END PROC
670 ' a window or door in an outer wall: SIDE 0 front (y = 0), 1 back, 2 west (x = 0), 3 east; POS along the wall; kind 0 window, 1 terrace door, 2 front door
680 PROC ADDWIN(F,SIDE,PP,W,SL,HD,K)
690   IF WN>=40 THEN EXIT PROC
700   WIN(WN,0)=F:WIN(WN,1)=SIDE:WIN(WN,2)=PP:WIN(WN,3)=W:WIN(WN,4)=SL:WIN(WN,5)=HD:WIN(WN,6)=K
710   WN=WN+1
720 END PROC
730 ' windows along one wall of a room from A1 to A2 (a terrace door instead when K = 1 and the wall is wide enough)
740 PROC WALLWIN(F,SIDE,A1,A2,W,SL,HD,K)
750   LOCAL N,P,W2,S2,H2,K2
760   N=INT((A2-A1)/32):IF N<1 THEN N=1
770   W2=W:S2=SL:H2=HD:K2=K
780   IF K2=1 THEN
790     IF A2-A1<28 THEN
800       K2=0
810     ELSE
820       N=1:W2=20:S2=0:H2=22
830     END IF
840   END IF
850   FOR P=0 TO N-1
860     CALL ADDWIN(F,SIDE,A1+(A2-A1)*(P+0.5)/N,W2,S2,H2,K2)
870   NEXT P
880 END PROC
890 PROC ROOMWIN(F,I)
900   LOCAL X1,Y1,X2,Y2,T,W,SL,HD,K
910   X1=RM(F,I,0):Y1=RM(F,I,1):X2=RM(F,I,2):Y2=RM(F,I,3):T=RM(F,I,4)
920   W=12:SL=9:HD=21
930   IF T=2 OR T=9 THEN W=6:SL=14:HD=22
940   IF Y1=TW THEN CALL WALLWIN(F,0,X1,X2,W,SL,HD,0)
950   IF Y2=DY-TW THEN
960     K=0:IF T=3 THEN K=1
970     CALL WALLWIN(F,1,X1,X2,W,SL,HD,K)
980   END IF
990   IF X1=TW THEN CALL WALLWIN(F,2,Y1,Y2,W,SL,HD,0)
1000   IF X2=LX-TW THEN CALL WALLWIN(F,3,Y1,Y2,W,SL,HD,0)
1010 END PROC
1020 PROC ADDFU(F,X1,Y1,X2,Y2,C)
1030   LOCAL N
1040   N=FUN(F)
1050   IF N>=64 THEN EXIT PROC
1060   FU(F,N,0)=X1:FU(F,N,1)=Y1:FU(F,N,2)=X2:FU(F,N,3)=Y2:FUC(F,N)=C:FUN(F)=N+1
1070 END PROC
1080 ' a piece of furniture against a wall of room I: W along the wall, H deep; WALL 0 south (y1 side), 1 north, 2 west, 3 east, tried from WALL on; PF 0 .. 1 where along the wall
1090 ' (the next walls and places are tried when it does not fit); sets OKP and PW (the wall used) and LQ1 .. LQ4 (the box)
1100 PROC SETF(F,I,W,H,WALL,PF,C)
1110   LOCAL T,WL,RX1,RY1,RX2,RY2,LNG,Q,A,X1,Y1,X2,Y2
1120   RX1=RM(F,I,0):RY1=RM(F,I,1):RX2=RM(F,I,2):RY2=RM(F,I,3)
1130   OKP=0
1140   FOR T=0 TO 7
1150     WL=(WALL+T) MOD 4
1160     IF WL<2 THEN LNG=RX2-RX1-W ELSE LNG=RY2-RY1-W
1170     IF LNG>=0 THEN
1180       Q=PF+T*0.37:Q=Q-INT(Q)
1190       A=Q*LNG
1200       IF WL=0 THEN X1=RX1+A:Y1=RY1:X2=X1+W:Y2=Y1+H
1210       IF WL=1 THEN X1=RX1+A:Y2=RY2:X2=X1+W:Y1=Y2-H
1220       IF WL=2 THEN Y1=RY1+A:X1=RX1:Y2=Y1+W:X2=X1+H
1230       IF WL=3 THEN Y1=RY1+A:X2=RX2:Y2=Y1+W:X1=X2-H
1240       IF X1>=RX1 AND X2<=RX2 AND Y1>=RY1 AND Y2<=RY2 THEN
1250         IF FN FREEBOX(F,X1,Y1,X2,Y2) THEN
1260           CALL ADDFU(F,X1,Y1,X2,Y2,C)
1270           LQ1=X1:LQ2=Y1:LQ3=X2:LQ4=Y2:PW=WL:OKP=-1
1280           EXIT PROC
1290         END IF
1300       END IF
1310     END IF
1320   NEXT T
1330 END PROC
1340 ' a piece of furniture in the open (the middle of the room, 5 dm from the walls)
1350 PROC SETC(F,I,W,H,C)
1360   LOCAL T,RX1,RY1,RX2,RY2,X1,Y1,X2,Y2,AX,AY
1370   RX1=RM(F,I,0)+5:RY1=RM(F,I,1)+5:RX2=RM(F,I,2)-5:RY2=RM(F,I,3)-5
1380   OKP=0
1390   IF RX2-RX1<W OR RY2-RY1<H THEN EXIT PROC
1400   AX=RX2-RX1-W:AY=RY2-RY1-H
1410   FOR T=0 TO 7
1420     X1=RX1+AX*(0.5+(FN RN()-0.5)*0.5):Y1=RY1+AY*(0.5+(FN RN()-0.5)*0.5):X2=X1+W:Y2=Y1+H
1430     IF FN FREEBOX(F,X1,Y1,X2,Y2) THEN
1440       CALL ADDFU(F,X1,Y1,X2,Y2,C)
1450       LQ1=X1:LQ2=Y1:LQ3=X2:LQ4=Y2:OKP=-1
1460       EXIT PROC
1470     END IF
1480   NEXT T
1490 END PROC
1500 ' the furniture of room I of floor F by its kind: 1 hall, 2 WC, 3 living room, 4 dining room, 5 kitchen, 6 study, 7 bedroom, 8 children's room, 9 bathroom
1510 PROC FITROOM(F,I)
1520   LOCAL T,K,W,WL,OP,C,TX1,TY1,TX2,TY2,N
1530   T=RM(F,I,4)
1540   IF T=1 THEN
1550     CALL SETF(F,I,10,3.5,FN RI(0,3),FN RN(),41)
1560     CALL SETF(F,I,6,2,FN RI(0,3),FN RN(),39)
1570   END IF
1580   IF T=2 THEN
1590     CALL SETF(F,I,4.5,7,1,0.5,54)
1600     CALL SETF(F,I,4,3.5,FN RI(0,3),FN RN(),54)
1610   END IF
1620   IF T=3 THEN
1630     C=42:IF FN RN()<0.4 THEN C=53
1640     IF FN RN()<0.25 THEN C=58
1650     CALL SETF(F,I,20,9,FN RI(0,3),FN RN(),C)
1660     IF OKP THEN
1670       OP=PW+1-2*(PW MOD 2)
1680       CALL SETF(F,I,15,4.5,OP,0.5,40)
1690       CALL SETC(F,I,10,6,41)
1700     END IF
1710     CALL SETF(F,I,12,3.5,FN RI(0,3),FN RN(),40)
1720     CALL SETF(F,I,8,8,FN RI(0,3),FN RN(),C)
1730   END IF
1740   IF T=4 THEN
1750     CALL SETC(F,I,16,9,40)
1760     IF OKP THEN
1770       TX1=LQ1:TY1=LQ2:TX2=LQ3:TY2=LQ4
1780       FOR N=0 TO 1
1790         CALL ADDFU(F,TX1+3+N*7,TY1-3.5,TX1+6+N*7,TY1-0.5,41)
1800         CALL ADDFU(F,TX1+3+N*7,TY2+0.5,TX1+6+N*7,TY2+3.5,41)
1810       NEXT N
1820       IF FN RN()<0.5 THEN CALL ADDFU(F,TX1-3.5,TY1+3,TX1-0.5,TY1+6,41)
1830       IF FN RN()<0.5 THEN CALL ADDFU(F,TX2+0.5,TY1+3,TX2+3.5,TY1+6,41)
1840     END IF
1850     CALL SETF(F,I,14,4,FN RI(0,3),FN RN(),41)
1860   END IF
1870   IF T=5 THEN
1880     CALL SETF(F,I,RM(F,I,2)-RM(F,I,0)-8,6,FN RI(0,3),0.5,54)
1890     WL=PW
1900     IF OKP THEN
1910       IF WL<2 THEN
1920         CALL SETF(F,I,18,6,2+FN RI(0,1),0.5,54)
1930       ELSE
1940         CALL SETF(F,I,18,6,FN RI(0,1),0.5,54)
1950       END IF
1960     END IF
1970     CALL SETF(F,I,6,6,FN RI(0,3),FN RN(),54)
1980     CALL SETC(F,I,10,8,40)
1990     IF OKP THEN
2000       TX1=LQ1:TY1=LQ2:TX2=LQ3:TY2=LQ4
2010       CALL ADDFU(F,TX1+1,TY1-3.5,TX1+4,TY1-0.5,41):CALL ADDFU(F,TX2-4,TY2+0.5,TX2-1,TY2+3.5,41)
2020     END IF
2030   END IF
2040   IF T=6 THEN
2050     CALL SETF(F,I,14,7,FN RI(0,3),FN RN(),40)
2060     CALL SETF(F,I,12,3.5,FN RI(0,3),FN RN(),41)
2070     CALL SETF(F,I,12,3.5,FN RI(0,3),FN RN(),41)
2080     CALL SETF(F,I,16,8,FN RI(0,3),FN RN(),42)
2090   END IF
2100   IF T=7 THEN
2110     CALL SETF(F,I,18,21,FN RI(0,3),FN RN(),53)
2120     CALL SETF(F,I,4.5,4.5,FN RI(0,3),FN RN(),40)
2130     CALL SETF(F,I,4.5,4.5,FN RI(0,3),FN RN(),40)
2140     CALL SETF(F,I,20,6.5,FN RI(0,3),FN RN(),39)
2150     CALL SETF(F,I,12,4.5,FN RI(0,3),FN RN(),41)
2160   END IF
2170   IF T=8 THEN
2180     CALL SETF(F,I,9,20,FN RI(0,3),FN RN(),42)
2190     CALL SETF(F,I,12,6,FN RI(0,3),FN RN(),39)
2200     CALL SETF(F,I,12,6,FN RI(0,3),FN RN(),40)
2210     IF RM(F,I,3)-RM(F,I,1)>50 THEN CALL SETF(F,I,9,20,FN RI(0,3),FN RN(),58):CALL SETF(F,I,16,3.5,FN RI(0,3),FN RN(),41)
2220   END IF
2230   IF T=9 THEN
2240     CALL SETF(F,I,17,7.5,FN RI(0,1),0.5,54)
2250     CALL SETF(F,I,6,5,FN RI(0,3),FN RN(),54)
2260     CALL SETF(F,I,4.5,7,FN RI(0,3),FN RN(),54)
2270   END IF
2280 END PROC
2290 ' ------------------------------------------------------------------------------------------------ making a house from the seed SD
2300 PROC GENHOUSE
2310   LOCAL I,IW,LW,RW,ZLO,ZHI,YE,XE,C,C2,Q,XC,XD,XA1,XA2,XB1,XB2,YF1,YF2,YC
2320   RC=0:WN=0
2330   FOR I=0 TO 1:RMN(I)=0:DON(I)=0:FUN(I)=0:CLN(I)=0:NEXT I
2340   LX=FN RI(100,120):DY=FN RI(80,100)
2350   HX=FN RI(26,30):IW=LX-2*TW
2360   LW=INT((IW-HX-2)*(0.45+FN RN()*0.1)):RW=IW-HX-2-LW
2370   FD=FN RI(28,36):IF FD>DY-50 THEN FD=DY-50
2380   WCD=FN RI(20,26)
2390   XA1=TW+LW:XA2=XA1+1:XB1=XA2+HX:XB2=XB1+1:XE=LX-TW:YE=DY-TW
2400   YF1=TW+FD:YF2=YF1+1:WY=YE-WCD-1
2410   SS=FN RI(0,1)
2420   ' the stairs: 10 dm wide, 31 long, along the hall wall of the living side, ending at the WC / bathroom wall
2430   IF SS=0 THEN SX1=XA2:SX2=XA2+10 ELSE SX1=XB1-10:SX2=XB1
2440   SY1=WY-31:SY2=WY
2450   FOR I=0 TO 1:C=CLN(I):CL(I,C,0)=SX1-1:CL(I,C,1)=SY1-1:CL(I,C,2)=SX2+1:CL(I,C,3)=SY2:CLN(I)=C+1:NEXT I
2460   XC=(XA2+XB1)/2:XD=XC+4:IF SS=1 THEN XD=XC-4
2470   ' ground floor rooms: 0 hall, 1 WC, 2 west front, 3 west back, 4 east front, 5 east back
2480   CALL ADDROOM(0,XA2,TW,XB1,WY,1):CALL ADDROOM(0,XA2,WY+1,XB1,YE,2)
2490   IF SS=0 THEN
2500     CALL ADDROOM(0,TW,TW,XA1,YF1,4):CALL ADDROOM(0,TW,YF2,XA1,YE,3)
2510     CALL ADDROOM(0,XB2,TW,XE,YF1,6):CALL ADDROOM(0,XB2,YF2,XE,YE,5)
2520   ELSE
2530     CALL ADDROOM(0,TW,TW,XA1,YF1,6):CALL ADDROOM(0,TW,YF2,XA1,YE,5)
2540     CALL ADDROOM(0,XB2,TW,XE,YF1,4):CALL ADDROOM(0,XB2,YF2,XE,YE,3)
2550   END IF
2560   ' doors of the ground floor: the front door, the WC, the dining room (to the hall), living and dining room open to each other, the kitchen and the study to the hall
2570   CALL ADDWIN(0,0,XD,10,0,21,2)
2580   CALL ADDDOOR(0,1,XD,WY,8)
2590   IF SS=0 THEN
2600     CALL ADDDOOR(0,0,XA1,FN RI(TW+5,YF1-5),9)
2610     CALL ADDDOOR(0,1,(TW+XA1)/2,YF1,16)
2620     CALL ADDDOOR(0,0,XB1,FN RI(TW+5,YF1-5),9)
2630     CALL ADDDOOR(0,0,XB1,FN RI(YF2+5,WY-5),9)
2640   ELSE
2650     CALL ADDDOOR(0,0,XB1,FN RI(TW+5,YF1-5),9)
2660     CALL ADDDOOR(0,1,(XB2+XE)/2,YF1,16)
2670     CALL ADDDOOR(0,0,XA1,FN RI(TW+5,YF1-5),9)
2680     CALL ADDDOOR(0,0,XA1,FN RI(YF2+5,WY-5),9)
2690   END IF
2700   ' upper floor: 0 landing, 1 bathroom, 2 the long room over the living side, 3 service side front, 4 service side back
2710   CALL ADDROOM(1,XA2,TW,XB1,WY,1):CALL ADDROOM(1,XA2,WY+1,XB1,YE,9)
2720   IF SS=0 THEN
2730     CALL ADDROOM(1,TW,TW,XA1,YE,8)
2740     CALL ADDROOM(1,XB2,TW,XE,YF1,8):CALL ADDROOM(1,XB2,YF2,XE,YE,7)
2750   ELSE
2760     CALL ADDROOM(1,XB2,TW,XE,YE,8)
2770     CALL ADDROOM(1,TW,TW,XA1,YF1,8):CALL ADDROOM(1,TW,YF2,XA1,YE,7)
2780   END IF
2790   CALL ADDWIN(1,0,XC,10,9,21,0)
2800   CALL ADDDOOR(1,1,XD,WY,8)
2810   IF SS=0 THEN
2820     CALL ADDDOOR(1,0,XA1,FN RI(TW+5,SY1-5),9)
2830     CALL ADDDOOR(1,0,XB1,FN RI(TW+5,YF1-5),9)
2840     CALL ADDDOOR(1,0,XB1,FN RI(YF2+5,WY-5),9)
2850   ELSE
2860     CALL ADDDOOR(1,0,XB1,FN RI(TW+5,SY1-5),9)
2870     CALL ADDDOOR(1,0,XA1,FN RI(TW+5,YF1-5),9)
2880     CALL ADDDOOR(1,0,XA1,FN RI(YF2+5,WY-5),9)
2890   END IF
2900   ' windows (the hall of the ground floor has the door instead)
2910   FOR C=0 TO 1
2920     FOR I=0 TO RMN(C)-1
2930       IF I>0 THEN CALL ROOMWIN(C,I)
2940     NEXT I
2950   NEXT C
2960   ' the roof: the eaves, the ridge 8.5 .. 10 m high with a pitch of 27 .. 43 degrees
2970   KN=FN RI(0,6):EZ=FLZ(1)+26+KN
2980   ZLO=MAX(85,INT(EZ+0.52*DY/2)):ZHI=MIN(100,INT(EZ+0.95*DY/2)):IF ZHI<ZLO THEN ZHI=ZLO
2990   ZR=FN RI(ZLO,ZHI):CHX=FN RI(INT(LX*0.25),INT(LX*0.65))
3000   PLC=8+FN RI(0,5):RFC=26+FN RI(0,2):ATT=FN RI(0,1)
3010   ' the furniture
3020   FOR C=0 TO 1
3030     FOR I=0 TO RMN(C)-1
3040       CALL FITROOM(C,I)
3050     NEXT I
3060   NEXT C
3070 END PROC
3080 ' ------------------------------------------------------------------------------------------------ drawing helpers
3090 PROC DISC(CX,CY,R,C)
3100   LOCAL J,HW
3110   FOR J=-R TO R
3120     HW=INT(SQR(R*R-J*J)+0.5)
3130     GFX LINE CX-HW,CY+J,CX+HW,CY+J,C
3140   NEXT J
3150 END PROC
3160 ' a filled box in elevation coordinates (horizontal A1 .. A2, heights Z1 .. Z2)
3170 PROC ER(A1,Z1,A2,Z2,C)
3180   LOCAL XA,XB,T
3190   XA=FN XP(A1):XB=FN XP(A2)
3200   IF XA>XB THEN T=XA:XA=XB:XB=T
3210   XB=XB-1:IF XB<XA THEN XB=XA
3220   GFX LINE XA,FN ZP(Z2),XB,MAX(FN ZP(Z2),FN ZP(Z1)-1),C,BF
3230 END PROC
3240 ' a filled box in plan coordinates
3250 PROC PR(X1,Y1,X2,Y2,C)
3260   LOCAL XA,XB,YA,YB
3270   XA=FN PX(X1):XB=FN PX(X2)-1:YA=FN PY(Y2):YB=FN PY(Y1)-1
3280   IF XB<XA THEN XB=XA
3290   IF YB<YA THEN YB=YA
3300   GFX LINE XA,YA,XB,YB,C,BF
3310 END PROC
3320 ' soot above a window: a tongue of black that grows narrower and lighter
3330 PROC SOOT(A,Z,W,H)
3340   LOCAL R,HW,Y0,X0,N
3350   X0=FN XP(A):Y0=FN ZP(Z):N=INT(H*SC)
3360   FOR R=0 TO N
3370     IF Y0-R>=FN ZP(EZ) THEN
3380       HW=INT((W*SC/2)*(1-0.5*R/(N+1))+1)
3390       IF R<N*0.55 THEN GFX LINE X0-HW,Y0-R,X0+HW,Y0-R,45 ELSE GFX LINE X0-HW,Y0-R,X0+HW,Y0-R,59
3400     END IF
3410   NEXT R
3420 END PROC
3430 ' a wall with its plaster knocked off: the gray brick shows
3440 PROC BRICKS(A1,Z1,A2,Z2)
3450   LOCAL Y,XA,XB,T
3460   CALL ER(A1,Z1,A2,Z2,37)
3470   XA=FN XP(A1):XB=FN XP(A2):IF XA>XB THEN T=XA:XA=XB:XB=T
3480   FOR Y=FN ZP(Z2) TO FN ZP(Z1) STEP 2
3490     GFX LINE XA,Y,XB-1,Y,38
3500   NEXT Y
3510 END PROC
3520 ' ------------------------------------------------------------------------------------------------ an elevation: VM 0 the long side seen from the street (south), 1 the gable seen from the west
3530 PROC ELEV(VM,OX,GY,D)
3540   LOCAL I,F,A,W,Z1,Z2,K,N,H,BAD,X0,AW2,CZ,E,CU,Z,C,Y,XA,XB,X1,X2,BX,BZ,R,HW
3550   EVM=VM:EOX=OX:EGY=GY
3560   IF VM=0 THEN EAW=LX ELSE EAW=DY
3570   AW2=EAW
3580   ' the ground, the plaster and the plinth
3590   GFX LINE OX-12,GY,OX+INT(AW2*SC)+12,GY+8,2,BF
3600   CALL ER(0,0,AW2,EZ,PLC):CALL ER(0,0,AW2,6,PLC+6)
3610   IF VM=1 THEN
3620     FOR Z=EZ TO ZR-1 STEP 1
3630       E=(AW2/2)*(1-(Z-EZ)/(ZR-EZ))
3640       CALL ER(AW2/2-E,Z,AW2/2+E,Z+1,PLC)
3650     NEXT Z
3660   END IF
3670   RC=500000+VM*1000
3680   ' the plaster knocked off in places (D above 0.3): gray brick
3690   IF D>0.3 THEN
3700     FOR I=1 TO INT(D*12)
3710       A=FN RN()*AW2:Z1=8+FN RN()*(EZ-18):W=4+FN RN()*10:H=3+FN RN()*7
3720       IF A+W<AW2 THEN CALL BRICKS(A,Z1,A+W,Z1+H)
3730     NEXT I
3740   END IF
3750   ' windows and doors of this side
3760   IF VM=0 THEN K=0 ELSE K=2
3770   FOR I=0 TO WN-1
3780     IF WIN(I,1)=K THEN
3790       F=WIN(I,0):A=WIN(I,2):W=WIN(I,3):Z1=FLZ(F)+WIN(I,4):Z2=FLZ(F)+WIN(I,5)
3800       IF WIN(I,6)=2 THEN
3810         CALL ER(A-W/2,Z1,A+W/2,Z2,33):CALL ER(A-W/2+1,Z1,A+W/2-1,Z2-1,34)
3820         CALL ER(A+W/2-3,Z1+9,A+W/2-2,Z1+10,50)
3830         CALL ER(A-W/2-3,Z2+1,A+W/2+3,Z2+3,55):CALL ER(A-W/2-2,Z2+3,A+W/2+2,Z2+4,6)
3840         CALL ER(A-W/2-3,Z2+1,A-W/2-2,Z1,55):CALL ER(A+W/2+2,Z2+1,A+W/2+3,Z1,55)
3850       ELSE
3860         CALL ER(A-W/2,Z1,A+W/2,Z2,33):CALL ER(A-W/2-0.5,Z2,A+W/2+0.5,Z2+2,PLC+6)
3870         BAD=0:IF D>0.15 AND FN RN()<(D-0.1)*1.6 THEN BAD=-1
3880         IF BAD THEN CALL ER(A-W/2+1,Z1+1,A+W/2-1,Z2-1,51) ELSE CALL ER(A-W/2+1,Z1+1,A+W/2-1,Z2-1,32)
3890         CALL ER(A-0.4,Z1,A+0.4,Z2,33)
3900         IF Z2-Z1>14 THEN CALL ER(A-W/2,(Z1+Z2)/2-0.4,A+W/2,(Z1+Z2)/2+0.4,33)
3910         IF BAD THEN CALL SOOT(A,Z2,W,6+FN RN()*16*(0.5+D))
3920       END IF
3930     END IF
3940   NEXT I
3950   ' a small window in the gable
3960   IF VM=1 AND ATT THEN
3970     CALL ER(AW2/2-3,EZ+6,AW2/2+3,EZ+14,33):CALL ER(AW2/2-2,EZ+7,AW2/2+2,EZ+13,32)
3980     IF D>0.3 THEN CALL ER(AW2/2-2,EZ+7,AW2/2+2,EZ+13,51):CALL SOOT(AW2/2,EZ+14,6,10)
3990   END IF
4000   ' the roof
4010   IF VM=0 THEN
4020     CALL ER(-3,EZ-1,AW2+3,ZR,RFC)
4030     FOR Z=EZ+2 TO ZR STEP 3
4040       CALL ER(-3,Z,AW2+3,Z+0.5,RFC+3)
4050     NEXT Z
4060     CALL ER(CHX,ZR-6,CHX+6,ZR+9,36):CALL ER(CHX-0.5,ZR+9,CHX+6.5,ZR+10,38)
4070     IF D>0.3 THEN
4080       N=INT((D-0.25)*14)
4090       FOR I=1 TO N
4100         A=FN RN()*(AW2+6)-3:W=8+FN RN()*(10+D*30):Z1=EZ+FN RN()*(ZR-EZ-4):H=5+FN RN()*(ZR-EZ)*(0.3+D*0.7)
4110         IF Z1+H>ZR+1 THEN H=ZR+1-Z1
4120         CALL ER(A,Z1,A+W,Z1+H,1)
4130         IF D<0.8 THEN
4140           FOR Y=INT(A/7)*7 TO INT(A+W) STEP 7
4150             IF FN RN()<0.75 AND Y>=A THEN CALL ER(Y,Z1,Y+0.8,Z1+H,44)
4160           NEXT Y
4170           IF FN RN()<0.5 THEN CALL ER(A,Z1+H-0.8,A+W,Z1+H,44)
4180         END IF
4190       NEXT I
4200     END IF
4210   ELSE
4220     FOR I=0 TO 2
4230       GFX LINE FN XP(-3),FN ZP(EZ-1)-I,FN XP(AW2/2),FN ZP(ZR+2.5)-I,RFC
4240       GFX LINE FN XP(AW2/2),FN ZP(ZR+2.5)-I,FN XP(AW2+3),FN ZP(EZ-1)-I,RFC
4250     NEXT I
4260     IF D>0.35 THEN
4270       FOR I=1 TO INT(D*6)
4280         A=FN RN()*AW2:Z=EZ+FN RN()*(ZR-EZ)*0.9:CALL ER(A-5,Z-2,A+5,Z+4,1)
4290       NEXT I
4300     END IF
4310   END IF
4320   ' the walls fall down from the top (D above 0.55): in columns of 2 dm, the roof and the gable with them
4330   IF D>0.6 THEN
4340     CU=(D-0.55)*2.4*(EZ-8):IF CU>EZ-8 THEN CU=EZ-8
4350     FOR X0=-3 TO AW2+3 STEP 2
4360       Z=FN RN()*CU
4370       IF X0<0 OR X0>=AW2 THEN Z=CU
4380       CALL ER(X0,EZ-Z,X0+2,EZ+1,1)
4390       IF Z>2 OR FN RN()<(D-0.55)*2.4 THEN CALL ER(X0,EZ,X0+2,ZR+14,1)
4400     NEXT X0
4410   END IF
4420   ' rubble heaps at the foot, the charred ends of the beams
4430   IF D>0.45 THEN
4440     FOR X0=-6 TO INT(AW2)+6 STEP 3
4450       H=FN RN()*(D-0.35)*9
4460       IF H>0.5 THEN
4470         CALL ER(X0,0,X0+3,H,47)
4480         IF FN RN()<0.4 THEN CALL ER(X0,H,X0+3,H+0.8,44)
4490       END IF
4500     NEXT X0
4510   END IF
4520   ' the hole of the beam: a clean round hole with a glassy, glowing rim, a scorched streak above it
4530   IF BM THEN
4540     RC=600000+VM
4550     BX=AW2*(0.25+FN RN()*0.5):BZ=10+FN RN()*(EZ-20):R=9+INT(FN RN()*6)
4560     FOR I=0 TO INT(R*2.2)
4570       HW=INT(R*0.9*(1-I/(R*2.4))+1):C=45:IF I<R*1.1 THEN C=56
4580       GFX LINE FN XP(BX)-HW,FN ZP(BZ)-R-I,FN XP(BX)+HW,FN ZP(BZ)-R-I,C
4590     NEXT I
4600     CALL DISC(FN XP(BX),FN ZP(BZ),R+3,49):CALL DISC(FN XP(BX),FN ZP(BZ),R+1,48):CALL DISC(FN XP(BX),FN ZP(BZ),R,51):CALL DISC(FN XP(BX),FN ZP(BZ),INT(R*0.4),1)
4610   END IF
4620 END PROC
4630 ' ------------------------------------------------------------------------------------------------ a floor plan
4640 PROC PLAN(F,OX,OY,D)
4650   LOCAL I,A,X1,Y1,X2,Y2,AX,W,C,N,T,S,L,XA,YA,K,XC,YC,V
4660   POX=OX:POY=OY
4670   GFX LINE OX-4,OY-4,OX+INT(LX*SC)+4,OY+INT(DY*SC)+4,4,BF
4680   CALL PR(0,0,LX,DY,6):CALL PR(TW,TW,LX-TW,DY-TW,7)
4690   FOR I=0 TO RMN(F)-1
4700     CALL PR(RM(F,I,0),RM(F,I,1),RM(F,I,2),RM(F,I,3),RM(F,I,5))
4710   NEXT I
4720   ' the stairs (ground floor) or the stairwell (upper floor)
4730   IF F=0 THEN
4740     CALL PR(SX1,SY1,SX2,SY2,52)
4750     FOR A=0 TO 12
4760       Y1=SY1+A*2.4:GFX LINE FN PX(SX1),FN PY(Y1),FN PX(SX2)-1,FN PY(Y1),41
4770     NEXT A
4780   ELSE
4790     CALL PR(SX1,SY1,SX2,SY2,51)
4800     FOR A=0 TO 12
4810       Y1=SY1+A*2.4:GFX LINE FN PX(SX1),FN PY(Y1),FN PX(SX2)-1,FN PY(Y1),44
4820     NEXT A
4830   END IF
4840   ' doors: a gap in the partition and the leaf
4850   FOR I=0 TO DON(F)-1
4860     AX=DOR(F,I,0):X1=DOR(F,I,1):Y1=DOR(F,I,2):W=DOR(F,I,3)
4870     IF AX=0 THEN
4880       CALL PR(X1,Y1-W/2,X1+1,Y1+W/2,57):GFX LINE FN PX(X1),FN PY(Y1+W/2),FN PX(X1)-INT(W*SC*0.8),FN PY(Y1+W/2),41
4890     ELSE
4900       CALL PR(X1-W/2,Y1,X1+W/2,Y1+1,57):GFX LINE FN PX(X1-W/2),FN PY(Y1),FN PX(X1-W/2),FN PY(Y1)-INT(W*SC*0.8),41
4910     END IF
4920   NEXT I
4930   ' windows and doors in the outer walls
4940   FOR I=0 TO WN-1
4950     IF WIN(I,0)=F THEN
4960       S=WIN(I,1):A=WIN(I,2):W=WIN(I,3):K=WIN(I,6)
4970       C=32:IF K=2 THEN C=34
4980       IF S=0 THEN CALL PR(A-W/2,0,A+W/2,TW,C)
4990       IF S=1 THEN CALL PR(A-W/2,DY-TW,A+W/2,DY,C)
5000       IF S=2 THEN CALL PR(0,A-W/2,TW,A+W/2,C)
5010       IF S=3 THEN CALL PR(LX-TW,A-W/2,LX,A+W/2,C)
5020     END IF
5030   NEXT I
5040   ' the names of the rooms
5050   FOR I=0 TO RMN(F)-1
5060     T=RM(F,I,4):L=LEN(NM$(T)):X1=FN PX(RM(F,I,0)):X2=FN PX(RM(F,I,2)):Y1=FN PY(RM(F,I,3)):Y2=FN PY(RM(F,I,1))
5070     IF X2-X1>L*8+4 AND Y2-Y1>16 AND D<0.5 THEN GFX TEXT (X1+X2)\2-L*4,(Y1+Y2)\2-6,NM$(T),5
5080   NEXT I
5090   ' furniture (burnt and gone with D)
5100   RC=700000+F*1000
5110   FOR I=0 TO FUN(F)-1
5120     C=FUC(F,I)
5130     IF D>0.35 AND FN RN()<D*1.2 THEN C=44:IF D>0.7 THEN C=43
5140     IF D>0.85 AND FN RN()<0.4 THEN C=0
5150     IF C>0 THEN
5160       X1=FU(F,I,0):Y1=FU(F,I,1):X2=FU(F,I,2):Y2=FU(F,I,3)
5170       CALL PR(X1,Y1,X2,Y2,C)
5180       IF X2-X1>8 OR Y2-Y1>8 THEN GFX LINE FN PX(X1),FN PY(Y2),FN PX(X2)-1,FN PY(Y1)-1,5,B
5190     END IF
5200   NEXT I
5210   ' fire: scorched floors, rubble, the upper floor fallen in
5220   IF D>0.2 THEN
5230     N=INT(D*1500)
5240     FOR I=1 TO N
5250       XA=OX+4+INT(FN RN()*(LX*SC-8)):YA=OY+4+INT(FN RN()*(DY*SC-8))
5260       V=POINT(XA,YA)
5270       IF V>=20 AND V<=25 OR V=57 THEN GFX PSET XA,YA,45
5280     NEXT I
5290   END IF
5300   IF D>0.45 THEN
5310     N=INT((D-0.35)*500)
5320     FOR I=1 TO N
5330       XA=OX+4+INT(FN RN()*(LX*SC-8)):YA=OY+4+INT(FN RN()*(DY*SC-8))
5340       C=47:IF FN RN()<0.3 THEN C=46
5350       GFX LINE XA,YA,XA+1,YA+1,C,BF
5360     NEXT I
5370   END IF
5380   IF F=1 AND D>0.5 THEN
5390     FOR I=1 TO INT((D-0.35)*12)
5400       X1=TW+FN RN()*(LX-2*TW-16):Y1=TW+FN RN()*(DY-2*TW-16):W=8+FN RN()*16
5410       CALL PR(X1,Y1,X1+W,Y1+W*(0.5+FN RN()),51)
5420     NEXT I
5430   END IF
5440   IF D>0.8 THEN
5450     FOR I=1 TO INT((D-0.6)*14)
5460       IF FN RN()<0.5 THEN
5470         X1=FN RN()*(LX-14):CALL PR(X1,0,X1+6+FN RN()*8,TW,4):CALL PR(X1,DY-TW,X1+6+FN RN()*8,DY,4)
5480       ELSE
5490         Y1=FN RN()*(DY-14):CALL PR(0,Y1,TW,Y1+6+FN RN()*8,4):CALL PR(LX-TW,Y1,LX,Y1+6+FN RN()*8,4)
5500       END IF
5510     NEXT I
5520   END IF
5530 END PROC
5540 ' ------------------------------------------------------------------------------------------------ one row of a sheet (OY the top of the row): the four views of the house SD
5550 PROC ROW(OY,D,B)
5560   CALL GENHOUSE
5570   BM=B
5580   GFX LINE 0,OY,639,OY+149,1,BF
5590   CALL ELEV(0,10,OY+140,D)
5600   CALL ELEV(1,172,OY+140,D)
5610   CALL PLAN(0,302,OY+20,D)
5620   CALL PLAN(1,474,OY+20,D)
5630   GFX TEXT 4,OY+2,"SEED "+STR$(SD)+"  D "+STR$(INT(D*100)/100)+" "+STR$(INT(LX))+"X"+STR$(INT(DY))+"DM  H"+STR$(INT(ZR)),5
5640   IF BM THEN GFX TEXT 4,OY+14,"BEAM",48
5650   GFX TEXT 302,OY+2,"GROUND FLOOR",5
5660   GFX TEXT 474,OY+2,"UPPER FLOOR",5
5670 END PROC
5680 ' the self test (HOUSEGEN_TEST=1, no graphics, run by engine1999c): the invariants of 300 houses; the exit status is the number of failures
5690 PROC SELFTEST
5700   LOCAL N,F,I,J,X1,Y1,X2,Y2,PIT,BAD,NF
5710   NF=0
5720   FOR N=1 TO 300
5730     SD=N:CALL GENHOUSE
5740     BAD=0
5750     PIT=ATAN2(ZR-EZ,DY/2)*180/PI
5760     IF LX<100 OR LX>120 OR DY<80 OR DY>100 OR ZR<85 OR ZR>100 OR PIT<26 OR PIT>44 THEN BAD=1
5770     IF DON(0)<>5 OR DON(1)<>4 OR WN>40 OR WN<14 THEN BAD=2
5780     IF SY1<TW+1 OR SX1<TW OR SX2>LX-TW THEN BAD=3
5790     FOR F=0 TO 1
5800       FOR I=0 TO RMN(F)-1
5810         IF RM(F,I,0)<TW OR RM(F,I,1)<TW OR RM(F,I,2)>LX-TW OR RM(F,I,3)>DY-TW OR RM(F,I,2)<=RM(F,I,0) OR RM(F,I,3)<=RM(F,I,1) THEN BAD=4
5820         FOR J=I+1 TO RMN(F)-1
5830           IF RM(F,I,0)<RM(F,J,2) AND RM(F,J,0)<RM(F,I,2) AND RM(F,I,1)<RM(F,J,3) AND RM(F,J,1)<RM(F,I,3) THEN BAD=5
5840         NEXT J
5850       NEXT I
5860       IF FUN(F)<10 THEN BAD=6
5870       FOR I=0 TO FUN(F)-1
5880         IF FU(F,I,0)<TW OR FU(F,I,1)<TW OR FU(F,I,2)>LX-TW OR FU(F,I,3)>DY-TW THEN BAD=7
5890       NEXT I
5900     NEXT F
5910     FOR I=0 TO WN-1
5920       IF WIN(I,1)<2 THEN
5930         IF WIN(I,2)<TW OR WIN(I,2)>LX-TW THEN BAD=8
5940       ELSE
5950         IF WIN(I,2)<TW OR WIN(I,2)>DY-TW THEN BAD=8
5960       END IF
5970     NEXT I
5980     IF BAD>0 THEN PRINT "house";N;" fails check";BAD:NF=NF+1
5990   NEXT N
6000   PRINT "housegen selftest: 300 houses, ";NF;" failures"
6010   IF NF>0 THEN BYE 1
6020   BYE 0
6030 END PROC
6040 PROC SETPAL
6050   LOCAL I,R,G,B
6060   RESTORE PALDATA
6070   FOR I=0 TO 59
6080     READ R,G,B
6090     GFX PALETTE 3,I,RGB(R,G,B)
6100   NEXT I
6110 END PROC
6120 ' ------------------------------------------------------------------------------------------------ main
6130 LABEL MAIN
6140 BREAK OFF
6150 IF ENV$("HOUSEGEN_TEST")<>"" THEN CALL SELFTEST
6160 RESTORE NAMES
6170 FOR I=1 TO 9:READ NM$(I):NEXT I
6180 RESTORE PAGES
6190 FOR I=1 TO 5:FOR J=0 TO 1:READ PGS(I,J,0),PGS(I,J,1),PGS(I,J,2):NEXT J:NEXT I
6200 DR$=ENV$("HOUSEGEN_DIR"):IF DR$<>"" THEN DR$=DR$+"/"
6210 AUT=0:IF ENV$("HOUSEGEN_AUTO")<>"" THEN AUT=-1
6220 SOFF=VAL(ENV$("HOUSEGEN_SEED"))
6230 GFX TARGET 3
6240 CALL SETPAL
6250 QT=0
6260 FOR PG=1 TO 5
6270   IF NOT QT THEN
6280     GFX CLEAR 1
6290     FOR J=0 TO 1
6300       SD=PGS(PG,J,0)+SOFF
6310       CALL ROW(J*150,PGS(PG,J,1)/100,PGS(PG,J,2))
6320     NEXT J
6330     GFX SAVE DR$+"housegen-"+STR$(PG)+".bmp"
6340     IF NOT AUT THEN
6350       K$=""
6360       REPEAT
6370         GFX VSYNC
6380         K$=INKEY$
6390       UNTIL K$<>""
6400       IF K$="q" OR K$="Q" THEN QT=-1
6410     END IF
6420   END IF
6430 NEXT PG
6440 GFX RESET
6450 CLS
6460 END
6470 LABEL FAILED
6480 GFX LAYER 3,OFF
6490 PRINT "Error: ";ERR$;" in line ";ERL
6500 END
6510 LABEL NAMES
6520 DATA "FLUR","WC","WOHN","ESS","KUECHE","ARBEIT","SCHLAF","KIND","BAD"
6530 LABEL PAGES
6540 DATA 1,0,0, 2,0,0
6550 DATA 3,0,0, 4,0,0
6560 DATA 1,35,0, 2,60,0
6570 DATA 3,85,0, 4,100,0
6580 DATA 1,50,-1, 3,65,-1
6590 LABEL PALDATA
6600 DATA 0,0,0, 176,206,232, 108,148,78, 76,108,54, 236,233,222, 22,22,24, 70,70,74, 104,104,108
6610 DATA 238,236,228, 238,224,176, 216,178,98, 224,164,142, 178,206,162, 168,178,204
6620 DATA 171,170,164, 171,161,127, 156,128,71, 161,118,102, 128,148,117, 121,128,147
6630 DATA 244,236,208, 246,236,170, 206,222,190, 244,214,190, 198,216,234, 222,208,230
6640 DATA 176,72,52, 122,80,58, 72,74,82, 130,50,36, 88,56,40, 46,48,54
6650 DATA 138,170,198, 244,244,244, 112,72,42, 0,0,0, 150,92,72, 152,152,154, 124,124,128
6660 DATA 204,164,114, 164,112,72, 114,74,46, 108,130,156, 30,28,26, 64,42,32, 52,50,48, 96,96,98, 134,130,124
6670 DATA 232,122,32, 206,238,238, 255,206,86, 12,12,14, 196,176,136, 154,62,62, 238,238,238, 172,170,162, 92,52,34, 224,220,206, 96,130,100, 90,88,86
