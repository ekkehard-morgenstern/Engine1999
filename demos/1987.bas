1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' 1987 - a third-person adventure toy for Engine1999, made with the software / hardware 3D mesh renderer (M3D ...). First step: the world and the two ways to move.
20 ' The fictional year 1987. After the invasion nearly everybody on Earth was taken, up into the ships or away by beams of light. Only a few are left. One of them is John,
30 ' a lean, gray-haired former teacher of physics and mathematics who likes comfortable clothes. He lives alone in a small wooden house in the country, next to a wheat field
40 ' that is ripe for the harvest and has nobody to harvest it. Black, egg-shaped drones hover over the land and shoot red beams at whatever looks suspicious (they are not in
50 ' this first step). John has his bicycle, and he loves it.
60 ' What there is: rolling country with grass, sand roads, groves of trees, the wooden house (you can walk in, with the bicycle too), the wheat field with a scarecrow, John
70 ' (a skeleton-animated figure: walk cycle and pedalling) and the bicycle (it rolls on, pedals turn, wheels spin, handlebars steer).
80 ' Keys: W / UP forward (on the bicycle: pedal), S / DOWN back (on the bicycle: brake, then reverse slowly), A D / LEFT RIGHT turn (on the bicycle: steer), SHIFT runs (on the
90 ' bicycle: sprint, it tires you), SPACE brakes the bicycle, E or RETURN gets on and off the bicycle (stand close to it), B rings the bell, Z X or the right mouse button + drag
100 ' turn the camera round John, PGUP PGDN tilt it, O opens the 3D settings, P pauses, Q or ESC quits. Game controller: left stick, LT, A brake, B bell, X use, Y settings,
110 ' right stick turns the camera, START pauses, BACK quits.
120 ' The bicycle behaves like the ones in GTA San Andreas: it has to be pedalled up to speed (slow at first, a sprint tires), it rolls on when you stop pedalling, hills speed
130 ' it up and slow it down, the turning radius grows with the speed (a bicycle cannot turn on the spot), you can brake and reverse, and a wall at speed is a hard stop.
140 ' Test mode (no keyboard needed): the environment variable G1987_PLAY names a file of commands, G1987_HEADLESS=1 runs it in the console host without any picture (see PLAYBACK).
150 LOAD LIBRARY "w3dkit"
160 LOAD LIBRARY "input"
170 LOAD LIBRARY "text"
180 LOAD LIBRARY "prefs3d"
190 LOAD LIBRARY "music3d":LOAD LIBRARY "banner3d":IF ENV$("G1987_HEADLESS")="" THEN CALL B3_M3D("1987")
200 BREAK OFF
210 ON ERROR GOTO FAILED
220 HL=0:IF ENV$("G1987_HEADLESS")<>"" THEN HL=-1
230 DIM RPX(300),RPY(300),RPF(300),CPX(40),CPY(40),SMX(120),SMY(120),PLX(40),PLY(40),TRX(520),TRY(520),LRR(8),LRZ(8),LRV(8),W$(8),GPX(2),GPZ(2),GPS(2),DWX(40),DWY(40),DRX(5),DRY(5),DRZ(5),DRH(5),DRT(5),DRA(5)
240 GOTO MAIN
250 ' ------------------------------------------------------------------------------------------------ small helpers
260 FUNC RN(M)
270   RETURN INT(RND(1)*M)
280 END FUNC
290 FUNC CC(H,S)
300   RETURN 16+H*16+S
310 END FUNC
320 ' the difference of two angles in degrees, -180 .. 180
330 FUNC ADIFF(A,B)
340   LOCAL D
350   D=A-B
360   D=D-INT((D+180)/360)*360
370   RETURN D
380 END FUNC
390 ' 1 inside a box (centre CX,CY, half sizes RX,RY), falling to 0 over the margin M outside it (smooth)
400 FUNC PW(X,Y,CX,CY,RX,RY,M)
410   LOCAL DX,DY,D
420   DX=ABS(X-CX)-RX:IF DX<0 THEN DX=0
430   DY=ABS(Y-CY)-RY:IF DY<0 THEN DY=0
440   D=SQR(DX*DX+DY*DY)/M
450   IF D>=1 THEN RETURN 0
460   RETURN (1-D)*(1-D)*(1+2*D)
470 END FUNC
480 ' the height of the ground: gentle hills, flat round the house and in the wheat field
490 FUNC HT(X,Y)
500   LOCAL H
510   H=15*SIN(X/610+1)*COS(Y/530)+10*SIN((X+Y)/340)+7*COS(X/170-Y/230)+5*SIN(Y/95+X/130)+12
520   RETURN 0.65*H*(1-FN PW(X,Y,0,0,260,200,420))*(1-FN PW(X,Y,1100,650,700,500,300))
530 END FUNC
540 ' the floor under a point (the ground, the porch, the floor of the house)
550 FUNC FLR(X,Y)
560   RETURN M3D("FLOORAT",X,Y,FN HT(X,Y)+40)
570 END FUNC
580 ' the ground under a point: 0 grass, 1 sand road, 2 wood (the porch and the floor of the house), 3 wheat
582 ' how fast John walks, 0 (the slow walk WSLOW or slower) .. 1 (the top speed WFAST)
584 FUNC SPD1(V)
586   RETURN MAX(0,MIN(1,(ABS(V)-WSLOW)/(WFAST-WSLOW)))
588 END FUNC
590 FUNC SURF(X,Y,Z)
600   LOCAL I,DX,DY,UX,UY,TT,E
605   ' the roads lie 3 .. 5 units above the ground (the walker stands on them), the porch 7, the floor of the house 5 (and no road reaches it)
610   E=Z-FN HT(X,Y):IF E>6.5 THEN RETURN 2
620   IF ABS(X-1100)<700 AND ABS(Y-650)<500 THEN RETURN 3
630   FOR I=0 TO RPN-1
640     IF RPF(I) THEN
641       UX=RPX(I)-RPX(I-1):UY=RPY(I)-RPY(I-1)
642       TT=((X-RPX(I-1))*UX+(Y-RPY(I-1))*UY)/(UX*UX+UY*UY)
643       IF TT<0 THEN TT=0
644       IF TT>1 THEN TT=1
645       DX=RPX(I-1)+TT*UX-X:DY=RPY(I-1)+TT*UY-Y
646       IF DX*DX+DY*DY<2025 THEN RETURN 1
650     END IF
660   NEXT I
665   IF E>2.5 THEN RETURN 2
670   RETURN 0
680 END FUNC
690 ' which picture covers the ground cell around X,Y: 1 grass, 2 dry grass, 3 lush grass, 20 the soil of the wheat field
700 FUNC CELLTEX(X,Y)
710   LOCAL N
720   IF ABS(X-1100)<700 AND ABS(Y-650)<500 THEN RETURN 320
730   N=SIN(X*0.0023+1)*COS(Y*0.0031)+SIN((X-Y)*0.0017)
740   IF N>0.55 THEN RETURN 303
750   IF N<-0.5 THEN RETURN 302
760   RETURN 301
770 END FUNC
780 PROC SND(N)
790   IF NOT HL THEN SFX N
800 END PROC
810 ' ------------------------------------------------------------------------------------------------ pictures
820 PROC MKGRASS(N,H1,H2,FL)
830   LOCAL I,X,Y
840   GFX CLEAR FN CC(H1,5)
850   FOR I=1 TO 520
860     X=FN RN(64):Y=FN RN(64)
870     GFX LINE X,Y,X+FN RN(3)-1,Y-1-FN RN(3),FN CC(H1+FN RN(2)*(H2-H1),3+FN RN(8))
880   NEXT I
890   FOR I=1 TO FL
900     GFX PSET FN RN(64),FN RN(64),FN CC(3+FN RN(2)*8,13+FN RN(3))
910   NEXT I
920   CALL W3K_GRAB(N,64,64)
930 END PROC
940 ' the sand road (pale sand, hue 13): the left and right edge are see-through (index 0), two wheel ruts, a ridge of grass between them
950 PROC MKROAD
960   LOCAL I,Y,L,R,X
970   GFX CLEAR 0
980   FOR Y=0 TO 63
990     L=3+FN RN(4):R=60-FN RN(4)
1000     GFX LINE L,Y,R,Y,FN CC(13,11+FN RN(3))
1010     GFX LINE 15,Y,22,Y,FN CC(13,8+FN RN(3))
1020     GFX LINE 41,Y,48,Y,FN CC(13,8+FN RN(3))
1030   NEXT Y
1040   FOR I=1 TO 16
1050     X=27+FN RN(10):Y=FN RN(64)
1060     GFX LINE X,Y,X+1,Y-2-FN RN(3),FN CC(4,5+FN RN(4))
1070   NEXT I
1080   FOR I=1 TO 70
1090     GFX PSET 5+FN RN(54),FN RN(64),FN CC(13,12+FN RN(4))
1100   NEXT I
1110   FOR I=1 TO 40
1120     GFX PSET 5+FN RN(54),FN RN(64),FN CC(13,6+FN RN(3))
1130   NEXT I
1140   CALL W3K_GRAB(304,64,64)
1150 END PROC
1160 ' wheat seen from the side: stalks with heavy ears, the rest of the picture is see-through
1170 PROC MKWHEAT
1180   LOCAL I,X,H,D,Y
1190   GFX CLEAR 0
1200   FOR I=1 TO 70
1210     X=FN RN(64):H=24+FN RN(34):D=FN RN(5)-2
1220     GFX LINE X,63,X+D,63-H,FN CC(13,7+FN RN(4))
1230     GFX LINE X+1,63,X+1+D,63-H,FN CC(12,6+FN RN(3))
1240     Y=63-H
1250     GFX LINE X+D-1,Y,X+D+1,Y+9,FN CC(3,10+FN RN(4))
1260     GFX LINE X+D,Y-3,X+D,Y+8,FN CC(2,9+FN RN(4))
1270   NEXT I
1280   FOR I=1 TO 60
1290     GFX PSET FN RN(64),52+FN RN(12),FN CC(12,4+FN RN(4))
1300   NEXT I
1310   CALL W3K_GRAB(308,64,64)
1320 END PROC
1330 ' the top of the wheat, seen from above
1340 PROC MKWHEATTOP
1350   LOCAL I
1360   GFX CLEAR FN CC(3,8)
1370   FOR I=1 TO 500
1380     GFX LINE FN RN(64),FN RN(64),FN RN(64),FN RN(64),FN CC(3+FN RN(2)*10,8+FN RN(5))
1390   NEXT I
1400   FOR I=1 TO 400
1410     GFX PSET FN RN(64),FN RN(64),FN CC(12,6+FN RN(4))
1420   NEXT I
1430   CALL W3K_GRAB(321,64,64)
1440 END PROC
1450 ' the plowed soil under the wheat
1460 PROC MKSOIL
1470   LOCAL I,Y
1480   GFX CLEAR FN CC(12,4)
1490   FOR Y=0 TO 7
1500     GFX LINE 0,Y*8+2,63,Y*8+3,FN CC(12,2)
1510     GFX LINE 0,Y*8+5,63,Y*8+5,FN CC(12,6)
1520   NEXT Y
1530   FOR I=1 TO 160
1540     GFX PSET FN RN(64),FN RN(64),FN CC(13,4+FN RN(5))
1550   NEXT I
1560   CALL W3K_GRAB(320,64,64)
1570 END PROC
1580 ' leaves: dark at the bottom (row 63), light at the top (row 0)
1590 PROC MKLEAVES(N,H1,H2)
1600   LOCAL I,Y,X,S
1610   FOR Y=0 TO 63
1620     GFX LINE 0,Y,63,Y,FN CC(H1,2+(63-Y)\10)
1630   NEXT Y
1640   FOR I=1 TO 380
1650     X=FN RN(64):Y=FN RN(64):S=(63-Y)\10
1660     GFX LINE X,Y,X+1+FN RN(3),Y+FN RN(3)-1,FN CC(H1+FN RN(2)*(H2-H1),MAX(1,S-1+FN RN(5)))
1670   NEXT I
1680   CALL W3K_GRAB(N,64,64)
1690 END PROC
1700 PROC MKBARK
1710   LOCAL I,X
1720   GFX CLEAR FN CC(12,3)
1730   FOR I=1 TO 40
1740     X=FN RN(64)
1750     GFX LINE X,0,X+FN RN(3)-1,63,FN CC(12,1+FN RN(5))
1760   NEXT I
1770   FOR I=1 TO 90
1780     GFX PSET FN RN(64),FN RN(64),FN CC(12,5+FN RN(3))
1790   NEXT I
1800   CALL W3K_GRAB(312,64,64)
1810 END PROC
1820 ' the sky: 256 x 64; which end of the picture is up is found by looking
1830 PROC MKSKY
1840   LOCAL Y,I,X,CY,RX
1850   FOR Y=0 TO 63
1860     GFX LINE 0,Y,255,Y,FN CC(8,7+Y\9),BF
1870   NEXT Y
1880   FOR I=1 TO 14
1890     X=FN RN(256):CY=10+FN RN(40):RX=8+FN RN(18)
1900     GFX CIRCLE X,CY,RX,FN CC(0,13),-1,-1,0.3
1910     GFX CIRCLE X-256,CY,RX,FN CC(0,13),-1,-1,0.3
1920     GFX PAINT X,CY,FN CC(0,15),FN CC(0,13)
1930     GFX PAINT X-256,CY,FN CC(0,15),FN CC(0,13)
1940   NEXT I
1950   CALL W3K_GRAB(311,256,64)
1960 END PROC
1970 PROC MAKETEX
1980   RANDOMIZE 1987
1990   CALL MKGRASS(301,5,4,0)
2000   CALL MKGRASS(302,4,3,0)
2010   CALL MKGRASS(303,5,4,30)
2020   CALL MKROAD
2030   CALL W3K_PLANKS(305)
2040   CALL W3K_TILES(306,12,12)
2050   CALL W3K_STONE(307,0)
2060   CALL MKWHEAT
2070   CALL MKLEAVES(309,5,4)
2080   CALL MKLEAVES(310,5,6)
2090   CALL MKSKY
2100   CALL MKBARK
2110   CALL W3K_WINDOW(313)
2120   CALL MKSOIL
2130   CALL MKWHEATTOP
2140   CALL MKLEAVES(317,4,3)
2150   CALL W3K_CARPET(323,1)
2160   GFX CLEAR 0
2170   GFX PSET 0,0,0
2180   CALL W3K_GRAB(299,1,1)
2190   M3D SKY 311
2200 END PROC
2210 ' ------------------------------------------------------------------------------------------------ mesh helpers
2220 PROC BX(O,X1,Y1,Z1,X2,Y2,Z2,TX)
2230   M3D BOX O,X1,Y1,Z1,X2,Y2,Z2,TX
2240 END PROC
2250 ' a lathe solid with 8 sides from the rings LRR (radius), LRZ (height), LRV (picture row) 0 .. NR-1, scaled by SC
2260 PROC LATHE(O,NR,TX,SC)
2270   LOCAL J,K,B,A,P,R
2280   B=M3D("VERTS",O)
2290   FOR J=0 TO NR-1
2300     R=LRR(J)*SC:IF R<0.6 THEN R=0.6
2310     FOR K=0 TO 8
2320       A=K*0.7853982
2330       M3D VERTEX O,R*COS(A),R*SIN(A),LRZ(J)*SC,K*8,LRV(J)
2340     NEXT K
2350   NEXT J
2360   FOR J=0 TO NR-2
2370     FOR K=0 TO 7
2380       P=B+J*9+K
2390       M3D QUAD O,P,P+1,P+10,P+9,TX
2400     NEXT K
2410   NEXT J
2420 END PROC
2430 ' a square rod between two points, half width H, the picture (or flat color) TBT
2440 PROC TUBE(O,X1,Y1,Z1,X2,Y2,Z2,H)
2450   LOCAL AX,AY,AZ,L,PX,PY,PZ,QX,QY,QZ,B,I,SA,SB
2460   AX=X2-X1:AY=Y2-Y1:AZ=Z2-Z1
2470   L=SQR(AX*AX+AY*AY+AZ*AZ):IF L<0.01 THEN EXIT PROC
2480   AX=AX/L:AY=AY/L:AZ=AZ/L
2490   PX=AY:PY=-AX:PZ=0
2500   L=SQR(PX*PX+PY*PY)
2510   IF L<0.001 THEN
2520     PX=1:PY=0
2530   ELSE
2540     PX=PX/L:PY=PY/L
2550   END IF
2560   QX=AY*PZ-AZ*PY:QY=AZ*PX-AX*PZ:QZ=AX*PY-AY*PX
2570   B=M3D("VERTS",O)
2580   FOR I=0 TO 7
2590     SA=1:IF (I AND 1) THEN SA=-1
2600     SB=1:IF (I AND 2) THEN SB=-1
2610     IF I<4 THEN
2620       M3D VERTEX O,X1+H*SA*PX+H*SB*QX,Y1+H*SA*PY+H*SB*QY,Z1+H*SA*PZ+H*SB*QZ
2630     ELSE
2640       M3D VERTEX O,X2+H*SA*PX+H*SB*QX,Y2+H*SA*PY+H*SB*QY,Z2+H*SA*PZ+H*SB*QZ
2650     END IF
2660   NEXT I
2670   M3D QUAD O,B,B+1,B+5,B+4,TBT
2680   M3D QUAD O,B+1,B+3,B+7,B+5,TBT
2690   M3D QUAD O,B+3,B+2,B+6,B+7,TBT
2700   M3D QUAD O,B+2,B,B+4,B+6,TBT
2710   M3D QUAD O,B+4,B+5,B+7,B+6,TBT
2720   M3D QUAD O,B,B+2,B+3,B+1,TBT
2730 END PROC
2740 ' a vertical picture in the plane y = Y (a window pane) or x = X
2750 PROC PANEY(O,X1,X2,Y,Z1,Z2,TX)
2760   LOCAL B
2770   B=M3D("VERTS",O)
2780   M3D VERTEX O,X1,Y,Z1,0,95:M3D VERTEX O,X2,Y,Z1,63,95:M3D VERTEX O,X2,Y,Z2,63,0:M3D VERTEX O,X1,Y,Z2,0,0
2790   M3D QUAD O,B,B+1,B+2,B+3,TX
2800 END PROC
2810 PROC PANEX(O,X,Y1,Y2,Z1,Z2,TX)
2820   LOCAL B
2830   B=M3D("VERTS",O)
2840   M3D VERTEX O,X,Y1,Z1,0,95:M3D VERTEX O,X,Y2,Z1,63,95:M3D VERTEX O,X,Y2,Z2,63,0:M3D VERTEX O,X,Y1,Z2,0,0
2850   M3D QUAD O,B,B+1,B+2,B+3,TX
2860 END PROC
2870 ' a ring (tyre) of 12 rods in the plane y = CY
2880 PROC RING(O,CX,CY,CZ,R)
2890   LOCAL I,A1,A2
2900   FOR I=0 TO 11
2910     A1=I*PI/6:A2=(I+1)*PI/6
2920     CALL TUBE(O,CX+R*COS(A1),CY,CZ+R*SIN(A1),CX+R*COS(A2),CY,CZ+R*SIN(A2),1.4)
2930   NEXT I
2940 END PROC
2950 ' spokes: three ribbons through the hub, turned by ANG (radians); they are rebuilt each frame, so the wheel seems to turn
2960 PROC SPOKES(O,CX,CZ,ANG)
2970   LOCAL I,A,B,DX,DZ,TX,TZ,R
2980   R=9.6
2990   FOR I=0 TO 2
3000     A=ANG+I*PI/3
3010     DX=COS(A):DZ=SIN(A):TX=-DZ*0.7:TZ=DX*0.7
3020     B=M3D("VERTS",O)
3030     M3D VERTEX O,CX-R*DX-TX,0,CZ-R*DZ-TZ:M3D VERTEX O,CX+R*DX-TX,0,CZ+R*DZ-TZ
3040     M3D VERTEX O,CX+R*DX+TX,0,CZ+R*DZ+TZ:M3D VERTEX O,CX-R*DX+TX,0,CZ-R*DZ+TZ
3050     M3D QUAD O,B,B+1,B+2,B+3,TBT
3060   NEXT I
3070   CALL BX(O,CX-1.2,-1.6,CZ-1.2,CX+1.2,1.6,CZ+1.2,FN CC(0,11))
3080 END PROC
3090 ' ------------------------------------------------------------------------------------------------ the terrain, the roads
3100 PROC BUILDTERRAIN
3110   LOCAL CI,CJ,O,I,J,X,Y,X0,Y0,B
3120   FOR CJ=0 TO 7
3130     FOR CI=0 TO 7
3140       O=100+CI+CJ*8
3150       M3D OBJECT O,0,235
3160       X0=-3600+CI*900:Y0=-3600+CJ*900
3170       FOR J=0 TO 9
3180         FOR I=0 TO 9
3190           X=X0+I*100:Y=Y0+J*100
3200           M3D VERTEX O,X,Y,FN HT(X,Y),X*0.5,Y*0.5
3210         NEXT I
3220       NEXT J
3230       FOR J=0 TO 8
3240         FOR I=0 TO 8
3250           B=J*10+I
3260           M3D QUAD O,B,B+1,B+11,B+10,FN CELLTEX(X0+I*100+50,Y0+J*100+50)
3270         NEXT I
3280       NEXT J
3290     NEXT CI
3300   NEXT CJ
3310 END PROC
3320 LABEL ROADDATA
3330 DATA 8, 0,-165, 0,-450, 250,-800, 800,-1000, 1500,-950, 2200,-700, 3000,-800, 3600,-1100
3340 DATA 9, -40,150, -300,500, -350,1000, -200,1600, 300,2200, 1000,2600, 1800,2500, 2600,2800, 3600,3000
3350 DATA 5, 1500,-950, 2300,-300, 2500,500, 2200,1500, 1800,2480
3360 DATA 0
3370 ' sand roads: Catmull-Rom curves through the control points, a strip of quads laid 3 units above the ground (see-through edges)
3380 PROC BUILDROADS
3390   LOCAL R,N,I,K,T,T2,T3,A,B,NS,O,X,Y,DX,DY,L,Z,D,V,X0,Y0
3400   RPN=0:NPL=0
3410   RESTORE ROADDATA
3420   R=0
3430   READ N
3440   WHILE N>0
3450     FOR I=0 TO N-1
3460       READ CPX(I),CPY(I)
3470     NEXT I
3480     NS=0
3490     FOR I=0 TO N-2
3500       A=I-1:IF A<0 THEN A=0
3510       B=I+2:IF B>N-1 THEN B=N-1
3520       FOR K=0 TO 5
3530         T=K/6:T2=T*T:T3=T2*T
3540         SMX(NS)=0.5*(2*CPX(I)+(CPX(I+1)-CPX(A))*T+(2*CPX(A)-5*CPX(I)+4*CPX(I+1)-CPX(B))*T2+(3*CPX(I)-CPX(A)-3*CPX(I+1)+CPX(B))*T3)
3550         SMY(NS)=0.5*(2*CPY(I)+(CPY(I+1)-CPY(A))*T+(2*CPY(A)-5*CPY(I)+4*CPY(I+1)-CPY(B))*T2+(3*CPY(I)-CPY(A)-3*CPY(I+1)+CPY(B))*T3)
3560         NS=NS+1
3570       NEXT K
3580     NEXT I
3590     SMX(NS)=CPX(N-1):SMY(NS)=CPY(N-1):NS=NS+1
3600     O=200+R
3610     M3D OBJECT O,1,235
3620     D=0
3630     FOR K=0 TO NS-1
3640       A=K-1:IF A<0 THEN A=0
3650       B=K+1:IF B>NS-1 THEN B=NS-1
3660       DX=SMX(B)-SMX(A):DY=SMY(B)-SMY(A)
3670       L=SQR(DX*DX+DY*DY):DX=DX/L:DY=DY/L
3680       IF K>0 THEN D=D+SQR((SMX(K)-SMX(K-1))^2+(SMY(K)-SMY(K-1))^2)
3690       Z=FN HT(SMX(K),SMY(K))+3+R
3700       X0=SMX(K):Y0=SMY(K)
3710       M3D VERTEX O,X0+DY*55,Y0-DX*55,FN HT(X0+DY*55,Y0-DX*55)+3+R,64,D*0.58
3720       M3D VERTEX O,X0-DY*55,Y0+DX*55,FN HT(X0-DY*55,Y0+DX*55)+3+R,0,D*0.58
3730       IF RPN<300 THEN RPX(RPN)=X0:RPY(RPN)=Y0:RPF(RPN)=(K>0):RPN=RPN+1
3740       IF R=0 AND K MOD 5=2 AND NPL<39 THEN PLX(NPL)=X0-DY*92:PLY(NPL)=Y0+DX*92:NPL=NPL+1
3750     NEXT K
3760     FOR K=0 TO NS-2
3770       M3D QUAD O,K*2,K*2+2,K*2+3,K*2+1,304
3780     NEXT K
3790     R=R+1
3800     READ N
3810   WEND
3820 END PROC
3830 ' ------------------------------------------------------------------------------------------------ trees
3840 ' a tree of the kind K (0 broadleaf, 1 pine, 2 poplar), scaled by S, standing at X,Y: the crown starts above 72 units, so you can walk and ride under it
3850 PROC TREE(O,X,Y,S,K)
3860   LOCAL J
3870   M3D OBJECT O,24,235
3880   LRR(0)=5:LRZ(0)=0:LRV(0)=63:LRR(1)=3.2:LRZ(1)=100:LRV(1)=0
3890   IF K=2 THEN LRZ(1)=80
3900   CALL LATHE(O,2,312,S)
3910   IF K=0 THEN
3920     LRR(0)=14:LRZ(0)=86:LRV(0)=63:LRR(1)=46:LRZ(1)=108:LRV(1)=50:LRR(2)=54:LRZ(2)=140:LRV(2)=32
3930     LRR(3)=38:LRZ(3)=182:LRV(3)=14:LRR(4)=0.6:LRZ(4)=206:LRV(4)=0
3940     CALL LATHE(O,5,309,S)
3950   END IF
3960   IF K=1 THEN
3970     FOR J=0 TO 2
3980       LRR(0)=38-J*7:LRZ(0)=74+J*38:LRV(0)=63:LRR(1)=0.6:LRZ(1)=134+J*38:LRV(1)=0
3990       CALL LATHE(O,2,310,S)
4000     NEXT J
4010   END IF
4020   IF K=2 THEN
4030     LRR(0)=10:LRZ(0)=74:LRV(0)=63:LRR(1)=22:LRZ(1)=120:LRV(1)=45:LRR(2)=24:LRZ(2)=180:LRV(2)=25
4040     LRR(3)=14:LRZ(3)=240:LRV(3)=8:LRR(4)=0.6:LRZ(4)=285:LRV(4)=0
4050     CALL LATHE(O,5,317,S)
4060   END IF
4070   M3D OBJPOS O,X,Y,FN HT(X,Y)-2,FN RN(360)
4080 END PROC
4090 ' may a tree stand here? inside the limit LIM, not in the wheat field or the yard, not on a road, not too near another tree
4100 FUNC TREEOK(X,Y,LIM)
4110   LOCAL I,DX,DY
4120   IF ABS(X)>LIM OR ABS(Y)>LIM THEN RETURN 0
4130   IF ABS(X-1100)<790 AND ABS(Y-650)<590 THEN RETURN 0
4140   IF ABS(X)<330 AND ABS(Y)<300 THEN RETURN 0
4150   FOR I=0 TO RPN-1
4160     DX=RPX(I)-X:DY=RPY(I)-Y
4170     IF DX*DX+DY*DY<6400 THEN RETURN 0
4180   NEXT I
4190   FOR I=0 TO NTR-1
4200     DX=TRX(I)-X:DY=TRY(I)-Y
4210     IF DX*DX+DY*DY<4900 THEN RETURN 0
4220   NEXT I
4230   RETURN -1
4240 END FUNC
4250 LABEL GROVES
4260 DATA -1800,1800,700,40,2, -2600,-600,600,34,2, -900,-2000,500,30,0, 2400,-1900,700,30,1, 2800,1800,600,30,2, 200,3000,600,26,0, -450,70,140,7,0, 3000,200,500,18,1
4270 DATA -1200,300,260,10,0, 1800,-300,200,8,2, 0,0,0,0,0
4280 PROC BUILDTREES
4290   LOCAL CX,CY,R,N,K,I,T,A,D,X,Y,O,P,S,E
4300   NTR=0:O=1000
4310   RANDOMIZE 1987
4320   RESTORE GROVES
4330   READ CX,CY,R,N,K
4340   WHILE N>0
4350     FOR I=1 TO N*4
4360       IF NTR<520 THEN
4370         A=RND(1)*6.2832:D=SQR(RND(1))*R
4380         X=CX+D*COS(A):Y=CY+D*SIN(A)
4390         IF FN TREEOK(X,Y,3300) THEN
4400           TRX(NTR)=X:TRY(NTR)=Y:NTR=NTR+1
4410           T=K:IF K=2 THEN T=FN RN(3)
4420           S=0.9+RND(1)*0.5:IF T=1 THEN S=1+RND(1)*0.4
4430           CALL TREE(O,X,Y,S,T):O=O+1
4440         END IF
4450       END IF
4460     NEXT I
4470     READ CX,CY,R,N,K
4480   WEND
4490   ' the border: a ring of pines and poplars that closes the world
4500   FOR E=0 TO 3
4510     FOR P=-3500 TO 3500 STEP 150
4520       A=FN RN(80)-40
4530       IF E=0 THEN X=P+A:Y=-3470+FN RN(60)
4540       IF E=1 THEN X=P+A:Y=3470-FN RN(60)
4550       IF E=2 THEN X=-3470+FN RN(60):Y=P+A
4560       IF E=3 THEN X=3470-FN RN(60):Y=P+A
4570       IF NTR<520 THEN
4580         IF FN TREEOK(X,Y,3580) THEN
4590           TRX(NTR)=X:TRY(NTR)=Y:NTR=NTR+1
4600           T=1+FN RN(2)
4610           CALL TREE(O,X,Y,1+RND(1)*0.5,T):O=O+1
4620         END IF
4630       END IF
4640     NEXT P
4650   NEXT E
4660   ' a big oak and a birch by the house
4670   CALL TREE(O,-345,-70,1.4,0):O=O+1
4680   CALL TREE(O,330,190,1.25,0):O=O+1
4690 END PROC
4700 ' ------------------------------------------------------------------------------------------------ the house
4710 PROC BUILDHOUSE
4720   LOCAL K,Z,H
4730   M3D OBJECT 300,0,235
4740   CALL BX(300,-130,-100,0,130,100,5,305)
4750   ' south wall: the door, a window
4760   CALL BX(300,-130,-100,5,-100,-92,106,305):CALL BX(300,-100,-100,5,-50,-92,44,305):CALL BX(300,-100,-100,84,-50,-92,106,305)
4770   CALL BX(300,-50,-100,5,-24,-92,106,305):CALL BX(300,24,-100,5,130,-92,106,305):CALL BX(300,-24,-100,70,24,-92,106,305)
4780   ' north wall: a window
4790   CALL BX(300,-130,92,5,-70,100,106,305):CALL BX(300,-70,92,5,-20,100,44,305):CALL BX(300,-70,92,84,-20,100,106,305):CALL BX(300,-20,92,5,130,100,106,305)
4800   ' west wall, east wall with a window
4810   CALL BX(300,-130,-92,5,-122,92,106,305)
4820   CALL BX(300,122,-92,5,130,-20,106,305):CALL BX(300,122,30,5,130,92,106,305):CALL BX(300,122,-20,5,130,30,44,305):CALL BX(300,122,-20,84,130,30,106,305)
4830   CALL BX(300,-130,-100,100,130,100,104,305)
4840   ' chimney
4850   CALL BX(300,78,18,120,98,38,200,307)
4860   ' the roof (two sided) and the gables
4870   M3D OBJECT 301,16,230
4880   M3D VERTEX 301,-142,-118,96,-71,0:M3D VERTEX 301,142,-118,96,71,0:M3D VERTEX 301,142,0,165,71,69:M3D VERTEX 301,-142,0,165,-71,69
4890   M3D QUAD 301,0,1,2,3,306
4900   M3D VERTEX 301,142,118,96,71,0:M3D VERTEX 301,-142,118,96,-71,0:M3D VERTEX 301,-142,0,165,-71,69:M3D VERTEX 301,142,0,165,71,69
4910   M3D QUAD 301,4,5,6,7,306
4920   M3D OBJECT 302,16,235
4930   FOR K=0 TO 1
4940     Z=-130+K*260
4950     M3D VERTEX 302,Z,-100,106,0,59:M3D VERTEX 302,Z,100,106,100,59:M3D VERTEX 302,Z,0,165,50,0
4960     M3D FACE 302,K*3,K*3+1,K*3+2,305
4970   NEXT K
4980   ' the porch
4990   M3D OBJECT 304,0,235
5000   CALL BX(304,-70,-150,0,70,-100,7,305)
5010   CALL BX(304,-68,-148,7,-62,-142,92,305):CALL BX(304,62,-148,7,68,-142,92,305)
5020   CALL BX(304,-76,-158,92,76,-96,97,306)
5030   ' glass: panes of the windows (see-through frames)
5040   M3D OBJECT 306,17,235
5050   CALL PANEY(306,-100,-50,-96,44,84,313):CALL PANEY(306,-70,-20,96,44,84,313):CALL PANEX(306,126,-20,30,44,84,313)
5060   ' the front door, standing open
5070   M3D OBJECT 305,20,235
5080   CALL BX(305,0,-2,5,44,2,68,305)
5090   M3D OBJPOS 305,-22,-96,0,100
5100   ' furniture
5110   M3D OBJECT 307,0,235
5120   CALL BX(307,-122,20,5,-78,92,15,305):CALL BX(307,-121,21,15,-79,91,23,FN CC(9,7)):CALL BX(307,-120,70,23,-82,90,28,FN CC(0,14)):CALL BX(307,-121,21,23,-79,66,26,FN CC(1,6))
5130   CALL BX(307,-80,50,26,-30,85,29,305)
5140   CALL BX(307,-78,52,5,-74,56,26,305):CALL BX(307,-36,52,5,-32,56,26,305):CALL BX(307,-78,79,5,-74,83,26,305):CALL BX(307,-36,79,5,-32,83,26,305)
5150   CALL BX(307,-61,33,5,-49,45,17,305)
5160   CALL BX(307,76,16,5,104,44,41,FN CC(0,2)):CALL BX(307,86,26,41,94,34,100,FN CC(0,3))
5170   CALL BX(307,32,90,5,110,92,84,305):CALL BX(307,30,80,5,32,92,84,305):CALL BX(307,110,80,5,112,92,84,305)
5180   FOR Z=28 TO 76 STEP 24
5190     CALL BX(307,32,80,Z,110,92,Z+2,305)
5200     FOR K=0 TO 7
5210       H=1+FN RN(8)
5220       CALL BX(307,34+K*9.5,83,Z+2,34+K*9.5+7,89,Z+2+14+FN RN(6),FN CC(H,7+FN RN(3)))
5230     NEXT K
5240   NEXT Z
5250   CALL BX(307,-50,-40,5,30,30,6,323)
5260 END PROC
5270 ' the yard: a fence with two gates, the woodpile, mailbox, barrel, telephone poles, hay, the scarecrow
5280 PROC BUILDYARD
5290   LOCAL X,Y,K
5300   M3D OBJECT 320,8,235
5310   FOR X=-250 TO 250 STEP 40
5320     IF ABS(X)>=60 THEN CALL BX(320,X-2,-202,0,X+2,-198,44,305)
5330     IF X<-130 OR X>-20 THEN CALL BX(320,X-2,198,0,X+2,202,44,305)
5340   NEXT X
5350   CALL BX(320,-250,-201,20,-60,-199,26,305):CALL BX(320,-250,-201,34,-60,-199,40,305):CALL BX(320,60,-201,20,250,-199,26,305):CALL BX(320,60,-201,34,250,-199,40,305)
5360   CALL BX(320,-250,199,20,-130,201,26,305):CALL BX(320,-250,199,34,-130,201,40,305):CALL BX(320,-20,199,20,250,201,26,305):CALL BX(320,-20,199,34,250,201,40,305)
5370   FOR Y=-160 TO 160 STEP 40
5380     CALL BX(320,-252,Y-2,0,-248,Y+2,44,305):CALL BX(320,248,Y-2,0,252,Y+2,44,305)
5390   NEXT Y
5400   CALL BX(320,-251,-200,20,-249,200,26,305):CALL BX(320,-251,-200,34,-249,200,40,305):CALL BX(320,249,-200,20,251,200,26,305):CALL BX(320,249,-200,34,251,200,40,305)
5410   M3D OBJECT 321,8,235
5420   FOR K=0 TO 5
5430     CALL BX(321,135,-60+K*8,0,165,-54+K*8,10,FN CC(12,4+K))
5440     IF K<5 THEN CALL BX(321,138,-56+K*8,10,162,-50+K*8,20,FN CC(12,6+K))
5450   NEXT K
5460   CALL BX(321,90,-185,0,93,-182,34,312):CALL BX(321,84,-190,34,100,-176,46,FN CC(9,5))
5470   M3D OBJECT 322,8,235
5480   LRR(0)=9:LRZ(0)=0:LRV(0)=0:LRR(1)=11:LRZ(1)=8:LRV(1)=0:LRR(2)=11:LRZ(2)=24:LRV(2)=0:LRR(3)=9:LRZ(3)=32:LRV(3)=0
5490   CALL LATHE(322,4,FN CC(9,4),1)
5500   M3D OBJPOS 322,-150,-135,0
5510   M3D OBJECT 420,8,235
5520   FOR K=0 TO NPL-1
5530     CALL BX(420,PLX(K)-1.6,PLY(K)-1.6,0,PLX(K)+1.6,PLY(K)+1.6,150,312)
5540     CALL BX(420,PLX(K)-12,PLY(K)-1.2,138,PLX(K)+12,PLY(K)+1.2,141,312)
5550   NEXT K
5560   M3D OBJECT 411,8,235
5570   FOR K=0 TO 2
5580     CALL BX(411,340,170+K*40,0,380,196+K*40,24,FN CC(3,9+K))
5590   NEXT K
5600   CALL BX(411,345,210,24,375,236,46,FN CC(3,11))
5610   ' the scarecrow in the field
5620   M3D OBJECT 410,8,235
5630   CALL BX(410,-2,-2,0,2,2,72,312):CALL BX(410,-24,-1.5,50,24,1.5,54,312):CALL BX(410,-8,-4,32,8,4,56,FN CC(9,5))
5640   CALL BX(410,-5,-5,58,5,5,68,FN CC(14,11)):CALL BX(410,-9,-9,68,9,9,71,312):CALL BX(410,-5,-5,71,5,5,80,312)
5650   M3D OBJPOS 410,1100,650,0,200
5660 END PROC
5670 ' ------------------------------------------------------------------------------------------------ the wheat field: 16 blocks of crossing see-through rows with a top
5680 PROC BUILDWHEAT
5690   LOCAL BI,BJ,O,X1,Y1,X2,Y2,X,Y,B
5700   FOR BJ=0 TO 3
5710     FOR BI=0 TO 3
5720       O=400+BI+BJ*4
5730       M3D OBJECT O,29,235
5740       X1=400+BI*350:X2=X1+350:Y1=150+BJ*250:Y2=Y1+250
5750       M3D VERTEX O,X1,Y1,34,X1*0.5,Y1*0.5:M3D VERTEX O,X2,Y1,34,X2*0.5,Y1*0.5:M3D VERTEX O,X2,Y2,34,X2*0.5,Y2*0.5:M3D VERTEX O,X1,Y2,34,X1*0.5,Y2*0.5
5760       M3D QUAD O,0,1,2,3,321
5770       Y=Y1+3
5780       WHILE Y<Y2
5790         B=M3D("VERTS",O)
5800         M3D VERTEX O,X1,Y,0,X1*1.3333,63:M3D VERTEX O,X2,Y,0,X2*1.3333,63:M3D VERTEX O,X2,Y,46,X2*1.3333,0:M3D VERTEX O,X1,Y,46,X1*1.3333,0
5810         M3D QUAD O,B,B+1,B+2,B+3,308
5820         Y=Y+30+FN RN(6)
5830       WEND
5840       X=X1+5
5850       WHILE X<X2
5860         B=M3D("VERTS",O)
5870         M3D VERTEX O,X,Y1,0,Y1*1.3333,63:M3D VERTEX O,X,Y2,0,Y2*1.3333,63:M3D VERTEX O,X,Y2,46,Y2*1.3333,0:M3D VERTEX O,X,Y1,46,Y1*1.3333,0
5880         M3D QUAD O,B,B+1,B+2,B+3,308
5890         X=X+60+FN RN(8)
5900       WEND
5910     NEXT BI
5920   NEXT BJ
5930 END PROC
5940 ' ------------------------------------------------------------------------------------------------ the bicycle
5950 PROC BUILDBIKE
5960   ' the frame: x forward, y left, z up, the origin on the ground halfway between the axles
5970   M3D OBJECT 510,28,235
5980   TBT=FN CC(1,9)
5990   CALL TUBE(510,-2,0,9,-8,0,25,1.3)
6000   CALL TUBE(510,-8,0,24,12,0,26,1.2)
6010   CALL TUBE(510,-2,0,9,12,0,22,1.3)
6020   CALL TUBE(510,12,0,21,12,0,29,1.6)
6030   CALL TUBE(510,-2,0,9,-17,0,11,0.8)
6040   CALL TUBE(510,-8,0,24,-17,0,11,0.8)
6050   TBT=FN CC(0,6)
6060   CALL TUBE(510,-8,0,25,-9,0,30,0.9)
6070   CALL BX(510,-14,-2.6,29.4,-4,2.6,31,FN CC(12,4))
6080   CALL BX(510,-18,-1,0,-16,1,2,FN CC(0,3))
6090   TBT=FN CC(0,2)
6100   CALL RING(510,-17,0,11,10.5)
6110   CALL BX(510,-18.5,-3,15,-15.5,3,16.5,FN CC(0,4))
6120   ' the front: fork, stem, handlebar and wheel, turning about the steering axis (origin: on the ground below it, 12 in front of the frame's origin)
6130   M3D OBJECT 511,28,235
6140   TBT=FN CC(0,9)
6150   CALL TUBE(511,0,0,26,6,0,11,0.8)
6160   CALL TUBE(511,0,0,26,-1,0,31,0.9)
6170   CALL TUBE(511,-2,-10,31,-2,10,31,0.8)
6180   TBT=FN CC(0,2)
6190   CALL RING(511,6,0,11,10.5)
6200   CALL BX(511,-3.5,-12,29.8,-0.5,-8,32.2,FN CC(0,1)):CALL BX(511,-3.5,8,29.8,-0.5,12,32.2,FN CC(0,1))
6210   CALL BX(511,3,-1,36,5,1,38,FN CC(3,14))
6220   M3D OBJECT 512,28,235
6230   M3D OBJECT 513,28,235
6240   M3D OBJECT 530,30,235:CALL BX(530,-9,-8,0,9,8,0.3,FN CC(0,0))
6250   M3D OBJECT 531,30,235:CALL BX(531,-27,-6,0,27,6,0.3,FN CC(0,0))
6260   M3D OBJPOS 510,0,0,0,0:M3D OBJPOS 511,12,0,0,0:M3D OBJPOS 512,0,0,0,0:M3D OBJPOS 513,12,0,0,0
6270 END PROC
6280 ' places the bicycle's objects at its position and heading; the wheels' spokes and the pedals are built again each frame
6290 PROC PLACEBIKE
6300   LOCAL C,S,FX,FY,A,PX,PZ,Q
6310   C=COS(BH*PI/180):S=SIN(BH*PI/180)
6320   FX=BX+12*C:FY=BY+12*S
6330   M3D OBJPOS 510,BX,BY,BZ,BH
6340   M3D OBJECT 512,28,235
6350   TBT=FN CC(0,11)
6360   CALL SPOKES(512,-17,11,-WA)
6370   TBT=FN CC(0,10)
6380   FOR Q=0 TO 1
6390     A=-(CA+Q*PI)
6400     PX=-2+5.5*COS(A):PZ=9+5.5*SIN(A)
6410     IF Q=0 THEN
6420       CALL TUBE(512,-2,3.5,9,PX,3.5,PZ,0.7)
6430       CALL BX(512,PX-3,3.6,PZ-0.7,PX+3,7.6,PZ+0.7,FN CC(0,3))
6440     ELSE
6450       CALL TUBE(512,-2,-3.5,9,PX,-3.5,PZ,0.7)
6460       CALL BX(512,PX-3,-7.6,PZ-0.7,PX+3,-3.6,PZ+0.7,FN CC(0,3))
6470     END IF
6480   NEXT Q
6490   M3D OBJECT 513,28,235
6500   CALL SPOKES(513,6,11,-WA)
6510   M3D OBJPOS 512,BX,BY,BZ,BH
6520   M3D OBJPOS 511,FX,FY,BZ,BH+BSA
6530   M3D OBJPOS 513,FX,FY,BZ,BH+BSA
6540 END PROC
6550 ' ------------------------------------------------------------------------------------------------ John
6560 ' a two-bone limb: from the joint H to the end F, both bones L long, the middle joint bends towards the pole (PX,PY,PZ); the middle joint is left in KX,KY,KZ
6570 PROC IK(HX,HY,HZ,FX,FY,FZ,L,Z)
6580   LOCAL DX,DY,DZ,D,UX,UY,UZ,A,H,DP,NX,NY,NZ,NL
6590   DX=FX-HX:DY=FY-HY:DZ=FZ-HZ
6600   D=SQR(DX*DX+DY*DY+DZ*DZ)
6610   IF D>2*L-0.05 THEN D=2*L-0.05
6620   IF D<0.5 THEN D=0.5
6630   UX=DX/SQR(DX*DX+DY*DY+DZ*DZ+0.0001):UY=DY/SQR(DX*DX+DY*DY+DZ*DZ+0.0001):UZ=DZ/SQR(DX*DX+DY*DY+DZ*DZ+0.0001)
6640   A=D/2
6650   H=SQR(MAX(0,L*L-A*A))
6660   DP=PLX0*UX+PLY0*UY+PLZ0*UZ
6670   NX=PLX0-DP*UX:NY=PLY0-DP*UY:NZ=PLZ0-DP*UZ
6680   NL=SQR(NX*NX+NY*NY+NZ*NZ):IF NL<0.001 THEN NL=1
6690   KX=HX+A*UX+H*NX/NL:KY=HY+A*UY+H*NY/NL:KZ=HZ+A*UZ+H*NZ/NL
6700 END PROC
6710 ' a limb with a pole, drawn as two rods; TBT is the color of the upper, TBU of the lower bone
6720 PROC LIMB(O,HX,HY,HZ,FX,FY,FZ,L)
6730   CALL IK(HX,HY,HZ,FX,FY,FZ,L,0)
6740   CALL TUBE(O,HX,HY,HZ,KX,KY,KZ,LH)
6750   TBT=TBU
6760   CALL TUBE(O,KX,KY,KZ,FX,FY,FZ,LH*0.8)
6770 END PROC
6780 ' builds John into object 520: the body is in coordinates of the object (x forward, y left, z up; the origin on the ground below the hips when standing)
6790 ' pose 0: standing / walking (PH = the walk phase, AM = the stride 0 .. 1), pose 1: riding (in the coordinates of the bicycle; CA = the cranks, BS = the steering)
6800 PROC BUILDJOHN(POSE,PH,AM)
6810   LOCAL Q,SG,FX,FZ,HX,HZ,PX,PZ,A,HY,GX,GY,LN,HIPX,HIPZ,SHX,SHZ,GXL,GYL,HH,SWY,STY
6820   M3D OBJECT 520,28,235
6830   CC1=FN CC(5,6):CC2=FN CC(8,5):CC3=FN CC(14,11):CC4=FN CC(0,12):CC5=FN CC(0,14)
6840   IF POSE=0 THEN
6850     SD=6+AM*12
6860     FOR Q=0 TO 1
6870       A=PH/(2*PI)+Q*0.5:A=A-INT(A)
6880       IF A<0.6 THEN
6890         GPX(Q)=SD-2*SD*A/0.6:GPZ(Q)=2.8:GPS(Q)=-1
6900       ELSE
6910         HZ=(A-0.6)/0.4:HY=HZ*HZ*(3-2*HZ):GPX(Q)=-SD+2*SD*HY:GPZ(Q)=2.8+MIN(1,AM*4)*5*SIN(PI*HZ):GPS(Q)=0
6920       END IF
6930     NEXT Q
6940     HIPZ=60
6950     FOR Q=0 TO 1
6960       IF GPS(Q)<>0 THEN
6970         HH=2.8+SQR(MAX(1,27.6*27.6-0.7744*GPX(Q)*GPX(Q)))
6980         IF HH<HIPZ THEN HIPZ=HH
6990       END IF
7000     NEXT Q
7010     HIPX=0:SHX=0:SHZ=HIPZ+16
7012     ' the sway: the pelvis moves sideways over the leg that carries the weight (that leg is on the ground from phase 0 to 0.6, its middle at 0.3), the upper body follows
7013     ' with a little over half of it, and each hip goes forward with its leg (the pelvis turns: 0.12 of the foot's distance)
7014     SWY=MIN(1,ABS(JV)/WSLOW)*(0.8+0.8*AM)*SIN(PH-0.314):STY=SWY*0.55
7015     ' the pelvis: a trouser colored block wider than the torso's waist, under it; the legs join it at the sides
7016     CALL BX(520,-4.8,-8.2+SWY,HIPZ-4.5,4.8,8.2+SWY,HIPZ+3.5,CC2)
7020     CALL BX(520,-4,-7+STY,HIPZ+0.5,4,7+STY,SHZ+0.5,CC1)
7030     CALL BX(520,-4.2,-4+STY,SHZ+2,4.2,4+STY,SHZ+11,CC3):CALL BX(520,-4.8,-4.6+STY,SHZ+7,3.6,4.6+STY,SHZ+12.5,CC4):CALL BX(520,4.2,-3.6+STY,SHZ+5.4,5.1,3.6+STY,SHZ+7,FN CC(0,1))
7040     FOR Q=0 TO 1
7050       SG=1:IF Q=1 THEN SG=-1
7060       FX=GPX(Q):FZ=GPZ(Q)
7070       PLX0=1:PLY0=0:PLZ0=0.1:LH=2.6:TBT=FN CC(8,5):TBU=TBT
7080       CALL LIMB(520,0.12*FX,SG*4+SWY,HIPZ,FX,SG*4,FZ,14)
7090       CALL BX(520,FX-3.5,SG*4-3,FZ-2.8,FX+6,SG*4+3,FZ+1.5,FN CC(0,13))
7100       HX=-0.7*FX
7110       PLX0=-1:PLY0=SG*0.35:PLZ0=-0.1:LH=2.3:TBT=CC1:TBU=CC3
7120       CALL LIMB(520,0,SG*8.5+STY,SHZ-1.2,HX,SG*9.5+STY,19+2.8,11)
7130       CALL BX(520,HX-1.8,SG*9.5-1.8+STY,FZ+16,HX+1.8,SG*9.5+1.8+STY,FZ+20,CC3)
7140     NEXT Q
7150   ELSE
7160     HIPX=-9.5:HIPZ=32.4:SHX=-3:SHZ=46.4
7170     TBT=CC1
7175     CALL BX(520,HIPX-4.8,-7,HIPZ-3.5,HIPX+4.8,7,HIPZ+3.5,CC2)
7180     CALL TUBE(520,HIPX,3.4,HIPZ,SHX,3.4,SHZ,3.4):CALL TUBE(520,HIPX,-3.4,HIPZ,SHX,-3.4,SHZ,3.4)
7190     CALL BX(520,SHX-0.5,-4,SHZ+2,SHX+8.5,4,SHZ+11,CC3):CALL BX(520,SHX-1.2,-4.6,SHZ+7,SHX+7.4,4.6,SHZ+12.5,CC4):CALL BX(520,SHX+8.5,-3.6,SHZ+5.4,SHX+9.4,3.6,SHZ+7,FN CC(0,1))
7200     FOR Q=0 TO 1
7210       SG=1:IF Q=1 THEN SG=-1
7220       A=-(CA+Q*PI)
7230       PX=-2+5.5*COS(A):PZ=9+5.5*SIN(A)
7240       PLX0=1:PLY0=0:PLZ0=0.15:LH=2.6:TBT=FN CC(8,5):TBU=TBT
7250       CALL LIMB(520,HIPX,SG*3.8,HIPZ,PX-0.5,SG*5.6,PZ+4.2,14.8)
7260       CALL BX(520,PX-3.5,SG*5.6-2.8,PZ+0.8,PX+6,SG*5.6+2.8,PZ+4.5,FN CC(0,13))
7270       GXL=-2:GYL=SG*10
7280       GX=12+GXL*COS(BSA*PI/180)-GYL*SIN(BSA*PI/180)-12:GY=GXL*SIN(BSA*PI/180)+GYL*COS(BSA*PI/180)
7290       PLX0=-0.4:PLY0=SG*0.9:PLZ0=-0.3:LH=2.3:TBT=CC1:TBU=CC3
7300       CALL LIMB(520,SHX,SG*7,SHZ-1.5,12+GX,GY,31.8,11)
7310       CALL BX(520,12+GX-1.8,GY-1.8,31,12+GX+1.8,GY+1.8,33.4,CC3)
7320     NEXT Q
7330   END IF
7340 END PROC
7350 ' ------------------------------------------------------------------------------------------------ the drones
7360 ' black, egg-shaped scouts that hover high above the land, fly their routes (eight way points each) and sweep a red beam over the ground. They are objects 600 + n
7370 ' (the body), 610 + n (the beam and its spot on the ground, rebuilt each frame), 620 + n (a shadow). This step: they scout; they do not shoot at anything yet.
7380 PROC BUILDDRONES
7390   LOCAL K,J,O
7400   NDR=4:GT=0
7410   FOR K=0 TO NDR-1
7420     FOR J=0 TO 7
7430       DWX(K*8+J)=INT(2500*SIN(K*2.1+J*1.3+0.5)):DWY(K*8+J)=INT(2500*COS(K*1.3+J*1.7+K))
7440     NEXT J
7450   NEXT K
7460   RESTORE DRONEROUTE
7470   FOR J=0 TO 7
7480     READ DWX(J),DWY(J)
7490   NEXT J
7500   FOR K=0 TO NDR-1
7510     DRX(K)=DWX(K*8):DRY(K)=DWY(K*8):DRT(K)=1:DRH(K)=K*70:DRA(K)=380+K*35:DRZ(K)=DRA(K)
7520     O=600+K
7530     M3D OBJECT O,28,235
7540     LRR(0)=0.6:LRZ(0)=-30:LRV(0)=0:LRR(1)=13:LRZ(1)=-26:LRV(1)=0:LRR(2)=24:LRZ(2)=-12:LRV(2)=0:LRR(3)=29:LRZ(3)=6:LRV(3)=0
7550     LRR(4)=25:LRZ(4)=22:LRV(4)=0:LRR(5)=14:LRZ(5)=34:LRV(5)=0:LRR(6)=0.6:LRZ(6)=40:LRV(6)=0
7560     CALL LATHE(O,7,FN CC(0,1),2)
7570     LRR(0)=30:LRZ(0)=1:LRR(1)=30.2:LRZ(1)=5
7580     CALL LATHE(O,2,FN CC(0,3),2)
7590     LRR(0)=9:LRZ(0)=-26:LRR(1)=5:LRZ(1)=-31:LRR(2)=0.6:LRZ(2)=-32
7600     CALL LATHE(O,3,FN CC(1,15),2)
7610     CALL DRONEARMS(O,K)
7620     M3D OBJECT 610+K,30,235
7630     M3D OBJECT 620+K,30,235:CALL BX(620+K,-50,-50,0,50,50,0.3,FN CC(0,0))
7640   NEXT K
7650 END PROC
7660 ' the drone's armor and weapons, none of it symmetric: a crest of jagged blades on the back, four claws underneath (the fourth is shorter), a gun pod with a red muzzle on one side,
7670 ' a broken fin on the other, two small red eyes of different size, a bent antenna; K varies the blades from drone to drone
7680 PROC DRONEARMS(O,K)
7690   LOCAL I,A,B,R0,Z0,L,S
7700   FOR I=0 TO 7
7710     A=(I*45+K*23+(I MOD 3)*9)*PI/180:L=1+(I*5+K) MOD 3:S=0.3+0.12*(I MOD 2)
7720     R0=46:Z0=14+(I MOD 3)*6
7730     B=M3D("VERTS",O)
7740     M3D VERTEX O,R0*COS(A),R0*SIN(A),Z0:M3D VERTEX O,R0*COS(A+S),R0*SIN(A+S),Z0+8
7750     M3D VERTEX O,(R0+16+L*12)*COS(A+S*0.5),(R0+16+L*12)*SIN(A+S*0.5),Z0+22+L*16
7760     M3D FACE O,B,B+1,B+2,FN CC(0,2)
7770   NEXT I
7780   TBT=FN CC(0,1)
7790   FOR I=0 TO 3
7800     A=I*PI/2+0.5+K*0.3
7810     CALL TUBE(O,24*COS(A),24*SIN(A),-46,58*COS(A),58*SIN(A),-76,4)
7820     L=-102:IF I=3 THEN L=-86
7830     CALL TUBE(O,58*COS(A),58*SIN(A),-76,50*COS(A+0.25),50*SIN(A+0.25),L,2.6)
7840   NEXT I
7850   CALL BX(O,-16,46,-18,18,68,8,FN CC(0,1))
7860   TBT=FN CC(0,2)
7870   CALL TUBE(O,-6,58,-6,56,58,-6,5)
7880   CALL BX(O,56,54,-10,62,62,-2,FN CC(1,15))
7890   CALL BX(O,-10,56,8,12,70,12,FN CC(0,1))
7900   B=M3D("VERTS",O)
7910   M3D VERTEX O,-26,-50,2:M3D VERTEX O,10,-54,26:M3D VERTEX O,-38,-100,18
7920   M3D VERTEX O,14,-52,24:M3D VERTEX O,34,-50,-4:M3D VERTEX O,24,-98,-10
7930   M3D VERTEX O,-22,-52,-6:M3D VERTEX O,-6,-54,-24:M3D VERTEX O,-30,-84,-30
7940   M3D FACE O,B,B+1,B+2,FN CC(0,1):M3D FACE O,B+3,B+4,B+5,FN CC(0,2):M3D FACE O,B+6,B+7,B+8,FN CC(0,1)
7950   CALL BX(O,44,14,-6,53,24,3,FN CC(1,15)):CALL BX(O,50,-14,6,55,-8,12,FN CC(1,14))
7960   TBT=FN CC(0,2)
7970   CALL TUBE(O,0,0,76,10,-6,118,1.6):CALL TUBE(O,10,-6,118,2,-14,150,1.2)
7980   CALL BX(O,0,-17,148,5,-11,153,FN CC(1,15))
7990 END PROC
8000 LABEL DRONEROUTE
8010 DATA 300,-300, 1100,650, 1700,300, 1100,1000, 400,900, -500,300, -1100,-400, -300,-900
8020 ' flies the drones: each turns towards its next way point (a curve, not a corner) and bobs a little; the beam's target sweeps round below it
8030 PROC DRONESIM(DT)
8040   LOCAL K,W,DX,DY,L,TA
8050   GT=GT+DT
8060   FOR K=0 TO NDR-1
8070     W=K*8+DRT(K)
8080     DX=DWX(W)-DRX(K):DY=DWY(W)-DRY(K):L=SQR(DX*DX+DY*DY)
8090     IF L<80 THEN
8100       DRT(K)=(DRT(K)+1) MOD 8
8110     ELSE
8120       TA=ATAN2(DY,DX)*180/PI
8130       DRH(K)=DRH(K)+FN ADIFF(TA,DRH(K))*MIN(1,1.1*DT)
8140     END IF
8150     DRX(K)=DRX(K)+COS(DRH(K)*PI/180)*85*DT:DRY(K)=DRY(K)+SIN(DRH(K)*PI/180)*85*DT
8160     DRZ(K)=DRA(K)+12*SIN(GT*0.9+K)
8170   NEXT K
8180 END PROC
8190 ' places the drones and builds the red beams (only for the drones near the camera)
8200 PROC DRONERIG
8210   LOCAL K,O,TX,TY,GZ,A,I,DX,DY,B
8220   FOR K=0 TO NDR-1
8230     M3D OBJPOS 600+K,DRX(K),DRY(K),DRZ(K),GT*20+K*50
8240     M3D OBJPOS 620+K,DRX(K),DRY(K),FN HT(DRX(K),DRY(K))+3.5,0
8250     DX=DRX(K)-M3D("X"):DY=DRY(K)-M3D("Y")
8260     O=610+K
8270     M3D OBJECT O,30,235
8280     IF DX*DX+DY*DY<2600*2600 THEN
8290       TX=DRX(K)+300*COS(GT*0.5+K*2):TY=DRY(K)+300*SIN(GT*0.7+K*2)
8300       GZ=FN HT(TX,TY)+4
8310       M3D VERTEX O,DRX(K),DRY(K),DRZ(K)-62
8320       FOR I=0 TO 3
8330         A=I*PI/2+GT
8340         M3D VERTEX O,TX+26*COS(A),TY+26*SIN(A),GZ
8350       NEXT I
8360       FOR I=1 TO 4
8370         M3D FACE O,0,I,1+(I MOD 4),FN CC(1,13)
8380       NEXT I
8390       B=M3D("VERTS",O)
8400       M3D VERTEX O,TX,TY,GZ+0.3
8410       FOR I=0 TO 7
8420         A=I*PI/4
8430         M3D VERTEX O,TX+48*COS(A),TY+48*SIN(A),GZ+0.3
8440       NEXT I
8450       FOR I=0 TO 7
8460         M3D FACE O,B,B+1+I,B+1+((I+1) MOD 8),FN CC(1,12)
8470       NEXT I
8480     END IF
8490   NEXT K
8500 END PROC
8510 ' ------------------------------------------------------------------------------------------------ sound
8520 ' The drones hum on the music channels 1 .. 4 (a held low note each, slightly different pitches, so they beat against each other; VOLUME follows the distance to the camera,
8530 ' PITCH wobbles a little), the tyres' rolling noise is a held noise on channel 5 (louder with speed, on a sand road, in the wheat); footsteps are effects, one at each heel strike
8540 ' of the walk cycle, by the ground (SFX 2 grass, 5 sand, 6 wood, 7 wheat); the freewheel ticks (SFX 3), the bell (SFX 1) and a thud (SFX 4) are as before.
8550 PROC SOUNDSTART
8560   LOCAL K
8570   INSTRUMENT 41,"NAME DRONEHUM; OSC SINE 0.7 1; OSC SAW 0.6 2.003 5; OSC SINE 0.45 3 -4; FILTER 6 0.2; ENV 0.8 0.1 1 0.5; GAIN 0.8"
8580   INSTRUMENT 42,"NAME TYRE; OSC NOISE 1; FILTER 3 0.1; ENV 0.2 0.1 1 0.3; GAIN 1.4"
8590   FOR K=0 TO NDR-1
8600     WAVE K+1,41
8610     SOUND K+1,52+K*3.3,-1,0
8620   NEXT K
8630   WAVE 5,42
8640   SOUND 5,400,-1,0
8650 END PROC
8660 PROC AMBIENT
8670   LOCAL K,DX,DY,DZ,D,V,S,TV,SF
8680   FOR K=0 TO NDR-1
8690     DX=DRX(K)-M3D("X"):DY=DRY(K)-M3D("Y"):DZ=DRZ(K)-M3D("Z")-46
8700     D=SQR(DX*DX+DY*DY+DZ*DZ)
8710     V=1-D/2600:IF V<0 THEN V=0
8720     VOLUME K+1,0.8*V*V
8730     PITCH K+1,(52+K*3.3)*(1+0.012*SIN(GT*1.3+K))
8740   NEXT K
8750   S=ABS(BV)/250:IF S>1 THEN S=1
8760   SF=FN SURF(BX,BY,BZ)
8770   TV=S*0.1
8780   IF SF=1 THEN TV=S*0.2
8790   IF SF=3 THEN TV=S*0.26
8800   IF SF=2 THEN TV=S*0.08
8810   DX=BX-JX:DY=BY-JY:D=SQR(DX*DX+DY*DY)
8820   IF D>600 THEN TV=0
8830   VOLUME 5,TV
8840 END PROC
8850 PROC SOUNDMUTE
8860   LOCAL K
8870   FOR K=1 TO 5
8880     VOLUME K,0
8890   NEXT K
8900 END PROC
8910 PROC FOOTSTEP
8920   LOCAL S
8930   S=FN SURF(JX,JY,JZ)
8940   IF S=0 THEN CALL SND(2)
8950   IF S=1 THEN CALL SND(5)
8960   IF S=2 THEN CALL SND(6)
8970   IF S=3 THEN CALL SND(7)
8980 END PROC
8990 ' ------------------------------------------------------------------------------------------------ the camera
9000 PROC CAMUPDATE(DT)
9010   LOCAL TX,TY,TZ,D,E,K,C,S,DS,CX,CY,CZ,TG,H,FZ,R
9020   IF MODE=1 THEN TG=BH ELSE TG=JH
9030   K=3.4:IF MODE=1 THEN K=2.4
9040   CYW=CYW+FN ADIFF(TG+CO,CYW)*MIN(1,K*DT)
9050   TX=JX:TY=JY:TZ=JZ+36:IF MODE=1 THEN TZ=JZ+40
9060   D=125:IF MODE=1 THEN D=155+MAX(0,BV)*0.18
9070   IF CZM>0 THEN D=CZM
9080   E=(12+CP)*PI/180
9090   C=COS(CYW*PI/180):S=SIN(CYW*PI/180)
9100   ' a ray from John's head back to the wished place: whatever is in the way pulls the camera in
9110   M3D CAMERA TX,TY,TZ-46,CYW+180,12+CP
9120   R=M3D("SHOOT",D+16)
9130   DS=D
9140   IF M3D("SHOTKIND")=1 THEN DS=MIN(D,MAX(20,M3D("SHOTDIST")-12))
9150   DSV=DS
9160   CX=TX-C*DS*COS(E):CY=TY-S*DS*COS(E):CZ=TZ+DS*SIN(E)
9170   FZ=MAX(M3D("FLOORAT",CX,CY,CZ),FN HT(CX,CY))+10
9180   IF CZ<FZ THEN CZ=FZ
9190   H=SQR((TX-CX)^2+(TY-CY)^2)
9200   M3D CAMERA CX,CY,CZ-46,CYW,ATAN2(TZ-CZ,H)*180/PI+CLP
9210 END PROC
9220 ' ------------------------------------------------------------------------------------------------ the physics
9230 PROC SIM(DT)
9240   LOCAL TV,A,DX,DY,TH,C,S,SL,TP,DM,OM,MX,AC,SD,DD,BT,TS,TL,DRN
9250   ' ---- the bicycle (also when nobody rides it: it rolls out)
9260   TH=0:IF MODE=1 THEN TH=KF:IF KF=0 AND KLT>0 THEN TH=KLT
9270   C=COS(BH*PI/180):S=SIN(BH*PI/180)
9280   AC=0:SPRN=0:DRN=0
9281   ' pedalling has a target speed BT, from WBMIN (a barely deflected stick or a light pull of the trigger) up to the top speed WBMAX (all the way): the stick's
9282   ' deflection and the left trigger (an alternate control: it also works alone) give it the same way, the larger wins; the forward key means plain pedalling WPED, with
9283   ' SHIFT the top speed. Thrust stops when the target is reached (a lower target is reached by coasting; the brake is A / S). A target above WPED is a sprint: it drains
9284   ' stamina in proportion (0.2 a second at the top speed); used up (EXH) the target is at most WPED. WBMIN = 40, WPED = 140 units a second
9290   IF TH>0 THEN
9300     BT=0
9310     IF KFD>0 THEN BT=WPED:IF KSH<>0 THEN BT=WBMAX
9320     IF KFA>0 THEN TS=WBMIN+(WBMAX-WBMIN)*MIN(1,KFA/0.95):IF TS>BT THEN BT=TS
9330     IF KLT>0 THEN TS=WBMIN+(WBMAX-WBMIN)*KLT:IF TS>BT THEN BT=TS
9340     IF EXH AND BT>WPED THEN BT=WPED
9345     IF BT>WPED THEN SPRN=-1:DRN=0.2*(BT-WPED)/(WBMAX-WPED)
9350     IF BV<BT THEN
9360       IF SPRN THEN AC=200*(1-BV/(1.25*WBMAX)) ELSE AC=78*(1-BV/190)
9365     END IF
9370     IF BV<0 THEN AC=AC+130
9375   END IF
9380   IF AC<0 THEN AC=0
9430   SL=(FN HT(BX+C*24,BY+S*24)-FN HT(BX,BY))/24
9440   BV=BV+AC*DT-260*SL*DT
9445   ' the top speed of the bicycle: 30 km/h, also downhill (a sprint on the flat ends up at about this speed by itself)
9446   IF BV>WBMAX THEN BV=WBMAX
9450   IF BV>0 THEN BV=BV-(9+0.05*BV)*DT
9460   IF BV<0 THEN BV=BV+(9-0.05*BV)*DT
9470   IF ABS(BV)<2 AND AC=0 AND ABS(SL)<0.02 AND NOT (MODE=1 AND KF<0) THEN BV=0
9480   IF MODE=1 AND (KB<>0 OR KF<0) THEN
9490     IF BV>0.5 THEN
9500       BV=BV-230*DT:IF BV<0 THEN BV=0
9510     ELSE
9520       BV=BV-110*DT:IF BV<-80 THEN BV=-80
9530     END IF
9540   END IF
9550   ' steering: the angle of the front wheel shrinks with the speed, the turning rate follows from the wheel base (36 units)
9560   TP=0:IF MODE=1 THEN TP=KT
9570   BS=BS+(TP-BS)*MIN(1,7*DT)
9580   DM=0.6/(1+(BV/95)^2)
9590   OM=BV/36*TAN(DM*BS)*180/PI:BSA=DM*BS*180/PI
9600   BH=BH+OM*DT
9610   DX=COS(BH*PI/180)*BV*DT:DY=SIN(BH*PI/180)*BV*DT
9620   IF ABS(BV)>0.3 THEN
9630     M3D THINGMOVE 1,DX,DY
9640     IF M3D("HIT")<>0 THEN
9650       IF ABS(BV)>120 THEN
9660         CALL SND(4):IF MODE=1 THEN ST=MAX(0,ST-0.25):CALL SAY("OUCH. THAT WAS A WALL.")
9670         BV=-BV*0.2
9680       ELSE
9690         BV=BV*0.35
9700       END IF
9710     END IF
9720   END IF
9730   BX=M3D("TX",1):BY=M3D("TY",1):BZ=M3D("TZ",1)
9740   IF ABS(BX)>WLIM OR ABS(BY)>WLIM THEN
9750     BX=MAX(-WLIM,MIN(WLIM,BX)):BY=MAX(-WLIM,MIN(WLIM,BY))
9760     M3D THINGPOS 1,BX,BY,BZ:BV=0
9770   END IF
9780   WA=WA+BV*DT/11
9790   IF TH>0 OR (MODE=1 AND BV<20 AND TH>0) THEN CA=CA+MAX(BV*DT/69*2*PI/2.3,TH*0.9*DT*3)
9800   IF TH<0 AND BV<-0.5 THEN CA=CA+BV*DT/69*2*PI/2.3
9810   IF WA>1000 THEN WA=WA-1000*PI
9820   IF CA>100 THEN CA=CA-100
9830   TK=TK+ABS(BV)*DT
9840   IF MODE<>1 AND TK>70 AND ABS(BV)>25 THEN TK=0:CALL SND(3)
9850   IF MODE=1 AND TH<=0 AND TK>70 AND BV>25 THEN TK=0:CALL SND(3)
9860   ' ---- John
9870   IF MODE=0 THEN
9880     ' the target speed TV: a stick gives it by its deflection (just past the dead zone: WSLOW, fully pushed: WFAST); a key raises it from WSLOW to WFAST
9881     ' while it is held (WRAMP per second; SHIFT / the left trigger go straight to WFAST); letting go sets it to 0. John accelerates towards it (WACC),
9882     ' and slows down (WDEC) to a lower target or to a stop: no acceleration once it is reached.
9883     TV=0
9884     IF KFA>0 THEN
9885       TV=WSLOW+(WFAST-WSLOW)*MIN(1,KFA/0.95):KTS=0
9886     ELSE
9887       IF KFA<0 THEN
9888         TV=-WBACK*MIN(1,-KFA/0.95):KTS=0
9889       ELSE
9890         IF KFD>0 THEN
9891           IF KTS<WSLOW THEN KTS=WSLOW
9892           KTS=MIN(WFAST,KTS+WRAMP*DT):TV=KTS
9893         ELSE
9894           KTS=0:IF KFD<0 THEN TV=-WBACK
9895         END IF
9896       END IF
9897     END IF
9898     ' the trigger (or SHIFT) pulled a part of the way sets the target speed between WSLOW and WFAST while he means to go forward; the larger target wins
9899     IF (KFA>0 OR KFD>0 OR KLT>0) AND KRA>0 THEN
9900       TL=WSLOW+(WFAST-WSLOW)*KRA
9901       IF TL>TV THEN TV=TL
9903     END IF
9904     ' used up: only the slow walk until he has got his breath back
9905     IF EXH AND TV>WSLOW THEN TV=WSLOW:KTS=MIN(KTS,WSLOW)
9906     ACL=WDEC:IF ABS(TV)>ABS(JV) AND TV*JV>=0 THEN ACL=WACC
9930     JV=JV+SGN(TV-JV)*MIN(ABS(TV-JV),ACL*DT)
9931     ' fast walking tires: above WTIRE the drain grows to 0.1 a second at the top speed
9932     IF ABS(JV)>WTIRE THEN DRN=0.1*(ABS(JV)-WTIRE)/(WFAST-WTIRE)
9940     JH=JH+KT*(150-JV*0.25)*DT
9950     DX=COS(JH*PI/180)*JV*DT:DY=SIN(JH*PI/180)*JV*DT
9960     IF ABS(JV)>0.3 THEN
9970       M3D THINGMOVE 0,DX,DY
9975       ' a wall that stops him (he gets less than half of the way he meant to go) stops him dead (speed and the keys' target speed 0); sliding along a wall does not
9980       IF M3D("HIT")<>0 AND SQR((M3D("TX",0)-JX)^2+(M3D("TY",0)-JY)^2)<0.5*SQR(DX*DX+DY*DY) THEN JV=0:KTS=0
9990       JX=M3D("TX",0):JY=M3D("TY",0):JZ=M3D("TZ",0)
10000       IF ABS(JX)>WLIM OR ABS(JY)>WLIM THEN
10010         JX=MAX(-WLIM,MIN(WLIM,JX)):JY=MAX(-WLIM,MIN(WLIM,JY))
10020         M3D THINGPOS 0,JX,JY,JZ:JV=0:KTS=0
10030       END IF
10040     ELSE
10050       JV=0
10060     END IF
10070     ' the stride (the distance of one step) grows with the speed from 10 units (about 30 cm) to 30 units (about 90 cm); a unit is about 3.2 cm (John is 58 units tall)
10075     PH=PH+JV*DT*PI/(10+20*FN SPD1(JV))
10080     NS=INT(PH/PI):IF NS<>NSL THEN NSL=NS:IF ABS(JV)>8 THEN CALL FOOTSTEP
10090     ' get on
10100     DX=BX-JX:DY=BY-JY
10110     NEAR=0:IF DX*DX+DY*DY<4900 THEN NEAR=-1
10120     IF KU<>0 AND NEAR THEN
10130       MODE=2:MT=0:MFX=JX:MFY=JY:MFZ=JZ:CALL SND(2)
10140     END IF
10150   END IF
10160   IF MODE=2 THEN
10170     MT=MT+DT
10180     A=MT/0.45:IF A>1 THEN A=1
10190     JX=MFX+(BX-MFX)*A:JY=MFY+(BY-MFY)*A:JZ=MFZ+(BZ-MFZ)*A
10200     JH=JH+FN ADIFF(BH,JH)*MIN(1,12*DT)
10210     PH=PH+DT*7
10220     IF MT>=0.45 THEN MODE=1:JV=0:CALL SAY("W: PEDAL   SHIFT: SPRINT   S: BRAKE   E: GET OFF   B: BELL")
10230   END IF
10240   IF MODE=1 THEN
10250     JX=BX:JY=BY:JZ=BZ:JH=BH
10260     M3D THINGPOS 0,JX,JY,JZ
10270     IF KU<>0 THEN
10280       MODE=3:MT=0:MFX=BX:MFY=BY:MFZ=BZ
10290       DX=-SIN(BH*PI/180)*38:DY=COS(BH*PI/180)*38
10300       M3D THINGPOS 0,BX,BY,BZ:M3D THINGMOVE 0,DX,DY
10310       MGX=M3D("TX",0):MGY=M3D("TY",0):MGZ=M3D("TZ",0)
10320       M3D THINGPOS 0,MFX,MFY,MFZ:CALL SND(2)
10330     END IF
10340   END IF
10350   IF MODE=3 THEN
10360     MT=MT+DT
10370     A=MT/0.4:IF A>1 THEN A=1
10380     JX=MFX+(MGX-MFX)*A:JY=MFY+(MGY-MFY)*A:JZ=MFZ+(MGZ-MFZ)*A
10390     PH=PH+DT*7
10400     IF MT>=0.4 THEN
10410       MODE=0:JV=0:M3D THINGPOS 0,MGX,MGY,MGZ:JX=MGX:JY=MGY:JZ=MGZ
10420     END IF
10430   END IF
10435   ' stamina: it drains while sprinting (bicycle) or walking fast, else 0.12 a second come back; at 0.02 he is used up, until it is back at 0.25
10436   IF DRN>0 THEN ST=MAX(0,ST-DRN*DT) ELSE ST=MIN(1,ST+0.12*DT)
10437   IF ST<=0.02 THEN EXH=-1
10438   IF ST>=0.25 THEN EXH=0
10440   CALL DRONESIM(DT)
10450   IF MSGT>0 THEN MSGT=MSGT-DT
10460 END PROC
10470 ' ------------------------------------------------------------------------------------------------ messages, input, the picture
10480 PROC SAY(T$)
10490   MSG$=T$:MSGT=5
10500 END PROC
10510 PROC READKEYS
10520   LOCAL U,B,K,L
10530   KF=0:KT=0:KFD=0
10540   IF KEY("UP") OR KEY("W") THEN KFD=KFD+1
10550   IF KEY("DOWN") OR KEY("S") THEN KFD=KFD-1
10555   KFA=-JOY("LY"):KF=KFD
10560   IF KEY("LEFT") OR KEY("A") THEN KT=KT+1
10570   IF KEY("RIGHT") OR KEY("D") THEN KT=KT-1
10580   KF=KF+KFA:KT=KT-JOY("LX")
10590   IF KF>1 THEN KF=1
10600   IF KF<-1 THEN KF=-1
10610   IF KT>1 THEN KT=1
10620   IF KT<-1 THEN KT=-1
10630   ' KSH: SHIFT; KLT: the left trigger, analog 0 .. 1 (the first 8 % and the last 7 % of the pull are cut off); KRA: the run / sprint pull, the larger of the two (SHIFT is all the way)
10631   KSH=0:IF KEY("SHIFT") THEN KSH=-1
10632   L=JOY("LT"):KLT=0
10633   IF L>0.08 THEN
10634     KLT=MIN(1,(L-0.08)/0.85)
10635   END IF
10636   KRA=KLT:IF KSH<>0 THEN KRA=1
10637   KR=0:IF KRA>0 THEN KR=-1
10640   KB=0:IF KEY("SPACE") OR JOY("A") THEN KB=-1
10650   U=0:IF KEY("E") OR KEY("RETURN") OR JOY("X") THEN U=-1
10660   KU=0:IF U<>0 AND UL=0 THEN KU=-1
10670   UL=U
10680   B=0:IF KEY("B") OR JOY("B") THEN B=-1
10690   KBL=0:IF B<>0 AND BL=0 THEN KBL=-1
10700   BL=B
10710   K=0
10720   IF KEY("Z") THEN K=K+1
10730   IF KEY("X") THEN K=K-1
10740   IF W3K_INVX THEN K=K+JOY("RX") ELSE K=K-JOY("RX")
10750   IF K<>0 THEN CO=CO+K*80*DT0
10760   IF KEY("R") THEN CLP=CLP+45*DT0
10770   IF KEY("F") THEN CLP=CLP-45*DT0
10780   IF KEY("PGUP") THEN CP=CP+40*DT0
10790   IF KEY("PGDN") THEN CP=CP-40*DT0
10800   IF W3K_INVY THEN CLP=CLP+JOY("RY")*40*DT0 ELSE CLP=CLP-JOY("RY")*40*DT0
10810   CLP=MAX(-5,MIN(45,CLP)):IF CP>45 THEN CP=45
10820   IF CP<-8 THEN CP=-8
10830   ' the right mouse button drags the camera
10840   IF (MOUSE("B") AND 2)<>0 THEN
10850     IF MB<>0 THEN
10860       CO=CO-(MOUSE("X")-MX)*0.35*(1+2*(W3K_INVX<>0)):CLP=CLP-(MOUSE("Y")-MY)*0.25*(1+2*(W3K_INVY<>0))
10870     END IF
10880     MB=-1
10890   ELSE
10900     MB=0
10910   END IF
10920   MX=MOUSE("X"):MY=MOUSE("Y")
10930   ' the camera swings back behind John when you leave it alone
10940   IF K=0 AND MB=0 AND NOT KEY("PGUP") AND NOT KEY("PGDN") THEN
10950     CO=CO*(1-MIN(1,0.8*DT0))
10960     IF ABS(KF)>0 THEN CP=CP*(1-MIN(1,0.6*DT0))
10970     IF NOT KEY("R") AND NOT KEY("F") AND (MOUSE("B") AND 2)=0 THEN CLP=CLP*(1-MIN(1,0.7*DT0))
10980   END IF
10990   IF KBL<>0 THEN CALL SND(1)
11000 END PROC
11010 PROC HUD
11020   LOCAL I,S,X
11030   CALL W3K_HUDBAR(264,36)
11040   IF MODE=1 THEN
11050     GFX TEXT 8,269,"ON THE BICYCLE",FN CC(3,15)
11060     S=INT(ABS(BV)*0.1152)
11070     GFX TEXT 8,283,"SPEED "+STR$(S)+" KM/H",FN CC(0,13)
11080   ELSE
11090     GFX TEXT 8,269,"ON FOOT",FN CC(3,15)
11100     GFX TEXT 8,283,"JOHN",FN CC(0,13)
11110   END IF
11120   GFX TEXT 230,269,"STAMINA",FN CC(0,12)
11130   GFX LINE 230,284,339,293,FN CC(0,3),BF
11140   GFX LINE 232,286,232+INT(105*ST),291,FN CC(4,12),BF
11150   GFX TEXT 380,269,"1987",FN CC(2,13)
11160   GFX TEXT 380,283,"THE SKY IS QUIET",FN CC(0,9)
11170   IF MODE=0 AND NEAR THEN
11180     GFX TEXT 8,6,"E: GET ON THE BICYCLE",FN CC(3,15)
11190   END IF
11200   IF MSGT>0 AND (MODE<>0 OR NOT NEAR) THEN
11210     GFX TEXT (640-LEN(MSG$)*8)\2+1,7,MSG$,FN CC(0,1)
11220     GFX TEXT (640-LEN(MSG$)*8)\2,6,MSG$,FN CC(3,15)
11230   END IF
11240 END PROC
11250 PROC RIGS
11260   CALL DRONERIG
11270   IF MODE=1 THEN
11280     CALL BUILDJOHN(1,PH,0)
11290     M3D OBJPOS 520,BX,BY,BZ,BH
11300   ELSE
11310     AM=FN SPD1(JV):IF MODE>=2 THEN AM=0.4
11320     CALL BUILDJOHN(0,PH,AM)
11330     M3D OBJPOS 520,JX,JY,JZ,JH
11340   END IF
11350   CALL PLACEBIKE
11360   M3D OBJPOS 531,BX,BY,FN FLR(BX,BY)+0.8,BH
11370   IF MODE=1 THEN
11380     M3D OBJSHOW 530,0
11390   ELSE
11400     M3D OBJSHOW 530,1:M3D OBJPOS 530,JX,JY,FN FLR(JX,JY)+0.8,JH
11410   END IF
11420 END PROC
11430 PROC DRAWFRAME
11440   CALL CAMUPDATE(DTF)
11450   CALL RIGS
11460   M3D RENDER
11470   CALL HUD
11480   CALL W3K_SHOW
11490 END PROC
11500 ' ------------------------------------------------------------------------------------------------ the game
11510 PROC NEWGAME
11520   JX=0:JY=-172:JH=90:JV=0:JZ=FN FLR(JX,JY)
11530   BX=40:BY=-176:BH=270:BV=0:BS=0:BSA=0:BZ=FN FLR(BX,BY):WA=0:CA=0:ST=1
11540   M3D THINGPOS 0,JX,JY,JZ:M3D THINGPOS 1,BX,BY,BZ
11550   CZM=0:CLP=0:NSL=0:MODE=0:MT=0:PH=0:KTS=0:KFA=0:KFD=0:KRA=0:KSH=0:KLT=0:EXH=0:ST=1:STD=0:TK=0:CYW=JH:CO=0:CP=0:KU=0:UL=0:BL=0:NEAR=0:MSGT=0:MB=0
11560   CALL SAY("WALK WITH W A S D. THE BICYCLE IS BY THE PORCH: STAND NEAR IT AND PRESS E.")
11570   MSGT=14
11580 END PROC
11590 PROC PLAYLOOP
11600   LOCAL T0,FR,Y,YL,ST0
11610   T0=TI:YL=0:CALL SOUNDSTART
11620   REPEAT
11630     DT0=(TI-T0)/60:T0=TI
11640     IF DT0>0.1 THEN DT0=0.1
11650     DTF=DT0
11660     CALL READKEYS
11670     IF KEY("ESC") THEN IN_QUIT=1
11680     CALL SIM(DT0):CALL AMBIENT
11690     CALL DRAWFRAME
11700     IF FN IN_SERVICE() THEN
11710       CALL BIGTEXT(110,"PAUSED",FN CC(0,15),3)
11720       CALL W3K_SHOW
11730       CALL SOUNDMUTE
11740       CALL IN_PAUSELOOP
11750       T0=TI:W3K_T0=TI
11760     END IF
11770     Y=JOY("Y")
11780     IF IN_KEY=79 OR (Y<>0 AND YL=0) THEN
11790       CALL P3_DIALOG
11800       T0=TI
11810     END IF
11820     YL=Y
11830   UNTIL IN_QUIT
11840 END PROC
11850 ' the title: the camera circles John's house
11860 PROC TITLE
11870   LOCAL A,T0,C,Q,X,Y
11880   T0=TI:Q=0
11890   CALL NEWGAME
11900   REPEAT
11910     A=4.712389+1.05*SIN((TI-T0)/260)
11920     X=20+240*COS(A):Y=-150+240*SIN(A)
11930     M3D CAMERA X,Y,100-46,(A+PI)*180/PI,-10
11940     CALL DRONESIM(1/60)
11950     CALL RIGS
11960     M3D RENDER
11970     GFX LINE 0,60,639,150,FN CC(0,1),BF
11980     CALL BIGTEXT(66,"1987",FN CC(3,15),6)
11990     CALL BIGTEXT(150,"AFTER THE ARRIVAL",FN CC(0,13),2)
12000     GFX LINE 0,264,639,299,FN CC(0,1),BF
12010     CALL BIGTEXT(212,"THE SKY IS QUIET.  THE FIELD IS RIPE.  NOBODY IS LEFT TO HARVEST IT.",FN CC(0,13),1)
12020     IF (TI-T0) MOD 60<40 THEN CALL BIGTEXT(274,"PRESS SPACE OR RETURN",FN CC(3,15),1)
12030     CALL W3K_SHOW
12040     IF FN IN_SERVICE() THEN Q=Q
12050     C=0
12060     IF IN_KEY=32 OR IN_KEY=13 THEN C=-1
12070     IF KEY("SPACE") OR KEY("RETURN") OR JOY("A") OR JOY("START") THEN C=-1
12080   UNTIL C OR IN_QUIT
12090   CALL IN_WAITRELEASE
12100 END PROC
12110 ' ------------------------------------------------------------------------------------------------ test mode
12120 PROC SPLIT(L$)
12130   LOCAL I,N,C$,CUR$
12140   FOR I=0 TO 7
12150     W$(I)=""
12160   NEXT I
12170   N=0:CUR$=""
12180   FOR I=1 TO LEN(L$)
12190     C$=MID$(L$,I,1)
12200     IF C$=" " THEN
12210       IF CUR$<>"" AND N<7 THEN W$(N)=CUR$:N=N+1
12220       CUR$=""
12230     ELSE
12240       CUR$=CUR$+C$
12250     END IF
12260   NEXT I
12270   IF CUR$<>"" AND N<7 THEN W$(N)=CUR$
12280 END PROC
12290 ' commands, one per line of the file G1987_PLAY: f v (forward -1..1), t v (turn), run v, brake v, use (get on / off), wait seconds, goto x y heading,
12300 ' bike x y heading, state (prints), shot name (a screenshot in the window host), say text, quit
12310 PROC PLAYBACK
12320   LOCAL L$,C$,I,N,PF,PT,PR,PB,T0,PSH
12330   PF=0:PFA=0:PT=0:PR=0:PB=0:PSH=0
12340   WHILE NOT EOF(4)
12350     LINE INPUT #4,L$
12360     CALL SPLIT(L$)
12370     C$=W$(0)
12380     IF C$="f" THEN PF=VAL(W$(1))
12385     IF C$="fa" THEN PFA=VAL(W$(1))
12390     IF C$="t" THEN PT=VAL(W$(1))
12400     IF C$="run" THEN PR=VAL(W$(1))
12401     IF C$="shift" THEN PSH=VAL(W$(1))
12410     IF C$="brake" THEN PB=VAL(W$(1))
12420     IF C$="use" THEN KU=-1
12430     IF C$="goto" THEN
12440       JX=VAL(W$(1)):JY=VAL(W$(2)):JH=VAL(W$(3)):JZ=FN FLR(JX,JY):JV=0:M3D THINGPOS 0,JX,JY,JZ
12450     END IF
12460     IF C$="bike" THEN
12470       BX=VAL(W$(1)):BY=VAL(W$(2)):BH=VAL(W$(3)):BZ=FN FLR(BX,BY):BV=0:M3D THINGPOS 1,BX,BY,BZ
12480     END IF
12490     IF C$="wait" THEN
12500       N=INT(VAL(W$(1))*60)
12510       FOR I=1 TO N
12520         KFD=PF:KFA=PFA:KF=MAX(-1,MIN(1,PF+PFA)):KT=PT:KLT=PR:KSH=0:IF PSH<>0 THEN KSH=-1
12521         KRA=KLT:IF KSH<>0 THEN KRA=1
12522         KR=0:IF KRA>0 THEN KR=-1
12523         KB=PB
12530         CALL SIM(1/60)
12540         KU=0
12550         DTF=1/60
12560         CALL CAMUPDATE(1/60)
12570       NEXT I
12580     END IF
12590     IF C$="state" THEN
12600       PRINT "state mode=";MODE;" jx=";INT(JX);" jy=";INT(JY);" jz=";INT(JZ+0.5);" jh=";INT(JH);" jv=";INT(JV);" bx=";INT(BX);" by=";INT(BY);" bz=";INT(BZ+0.5);" bh=";INT(BH);" bv=";INT(BV);" st=";INT(ST*100)
12610     END IF
12620     IF C$="ray" THEN
12630       M3D CAMERA VAL(W$(1)),VAL(W$(2)),VAL(W$(3))-46,VAL(W$(4)),VAL(W$(5))
12640       N=M3D("SHOOT",600)
12650       PRINT "ray kind=";M3D("SHOTKIND");" obj=";M3D("SHOTOBJ");" dist=";INT(M3D("SHOTDIST"))
12660     END IF
12670     IF C$="bench" AND NOT HL THEN
12680       N=INT(VAL(W$(1))):T0=TI
12690       FOR I=1 TO N
12700         CALL SIM(1/60):CALL DRAWFRAME
12710       NEXT I
12720       PRINT "bench";N;" frames, ms per frame";INT((TI-T0)*1000/60/N);" renderer";M3D("RENDERER");" drawn";M3D("DRAWN")
12730       OPEN "bench.txt" FOR APPEND AS #5:PRINT #5,"bench";N;" ms per frame";INT((TI-T0)*1000/60/N);" renderer";M3D("RENDERER");" drawn";M3D("DRAWN"):CLOSE #5
12740     END IF
12750     IF C$="drones" THEN PRINT "drone x=";INT(DRX(0));" y=";INT(DRY(0));" z=";INT(DRZ(0));" t=";DRT(0)
12760     IF C$="clp" THEN CLP=VAL(W$(1))
12770     IF C$="zoom" THEN CZM=VAL(W$(1))
12780     IF C$="co" THEN CO=VAL(W$(1)):CYW=JH+CO
12790     IF C$="cp" THEN CP=VAL(W$(1))
12800     IF C$="cam" THEN
12810       PRINT "ds=";INT(DSV)
12820       PRINT "cam x=";INT(M3D("X"));" y=";INT(M3D("Y"));" z=";INT(M3D("Z")+46);" yaw=";INT(M3D("YAW"));" pitch=";INT(M3D("PITCH"))
12830     END IF
12840     IF C$="shot" AND HL THEN CALL RIGS:CALL SOUNDSTART:CALL AMBIENT:CALL FOOTSTEP
12850     IF C$="shot" AND NOT HL THEN
12860       CALL DRAWFRAME
12870       CALL DRAWFRAME
12880       SCREENSHOT W$(1)
12890     END IF
12900     IF C$="quit" THEN EXIT PROC
12910   WEND
12920 END PROC
12930 ' ------------------------------------------------------------------------------------------------ main
12940 LABEL MAIN
12950 IN_QUIT=0
12955 ' walking: speeds in units per second (a unit is about 3.2 cm), accelerations in units per second squared
12956 WSLOW=22:WFAST=155:WBACK=38:WACC=150:WDEC=170:WRAMP=60:ACL=0
12957 ' the bicycle's top speed: 30 km/h = 8.33 m/s = 260 units/s at 3.2 cm a unit (the scale of docs/1987.md)
12958 WBMAX=30/3.6/0.032
12959 WPED=140:WTIRE=100:WBMIN=40
12961 ' the edge of the world (the only invisible-looking limit; the red tiles of BUILDEDGE show it)
12962 WLIM=3420
12960 IF NOT HL THEN CALL W3K_SETUP:CALL W3K_MESH:M3D RESET:CALL MAKETEX
12970 CALL BUILDTERRAIN
12980 CALL BUILDROADS
12990 CALL BUILDTREES
13000 CALL BUILDHOUSE
13010 CALL BUILDYARD
13015 CALL BUILDEDGE
13020 CALL BUILDWHEAT
13030 CALL BUILDBIKE
13040 CALL BUILDDRONES
13050 M3D THING 0,0,0,0,299,30,56:M3D THING 1,0,0,0,299,35,40
13060 M3D PLAYER 14,56,46,24
13070 M3D FAR 2400
13080 IF NOT HL THEN
13090   CALL W3K_VIEW(0,0,640,264,2)
13100   M3D COLORMAP
13110   CALL P3_LOAD
13120   SFX 1,"I12 V0.45 T200 ML O6 L16 C R32 C"
13130   SFX 2,"W3 V0.12 ML X0,0.004,0.25,0.03 H420,32"
13140   SFX 3,"W3 V0.05 ML X0,0.002,0.2,0.01 H3500,64"
13150   SFX 4,"I4 V0.35 ML X0.2,0.3,0.5,0.4 S80,150,20 S150,70,30"
13160   SFX 5,"W3 V0.16 ML X0,0.004,0.25,0.03 H1500,32 R64 H900,32"
13170   SFX 6,"I16 V0.3 ML O3 C32"
13180   SFX 7,"W3 V0.1 ML X0.01,0.06,0.3,0.06 H3200,16"
13190 END IF
13200 IF ENV$("G1987_PLAY")<>"" THEN OPEN ENV$("G1987_PLAY") FOR INPUT AS #4:CALL NEWGAME:DT0=1/60:CALL PLAYBACK:IF NOT HL THEN CALL W3K_DONE
13210 IF ENV$("G1987_PLAY")<>"" THEN BYE
13220 CALL TITLE
13230 IF IN_QUIT=0 THEN CALL NEWGAME
13240 IF IN_QUIT=0 THEN CALL PLAYLOOP
13250 CALL W3K_DONE
13260 CALL TX_BYE("1987")
13270 END
13280 LABEL FAILED
13290 IF NOT HL THEN CALL W3K_DONE
13300 PRINT "Error: ";ERR$;" in line ";ERL
13310 ' the edge of the world: a band of red tiles (alternating bright and dark) on the ground just outside the line WLIM that John and the bicycle cannot cross; four passable
13320 ' objects 330 .. 333, one for each side, the tiles 100 units long and 30 wide, laid 2.5 above the ground
13330 PROC BUILDEDGE
13340   LOCAL S,K,O,B,T1,T2,X1,X2,Y1,Y2,C
13350   FOR S=0 TO 3
13360     O=330+S:M3D OBJECT O,20,255
13370     FOR K=0 TO 68
13380       T1=-3450+K*100:T2=T1+100
13390       IF S=0 THEN X1=WLIM:X2=WLIM+30:Y1=T1:Y2=T2
13400       IF S=1 THEN X1=-WLIM-30:X2=-WLIM:Y1=T1:Y2=T2
13410       IF S=2 THEN X1=T1:X2=T2:Y1=WLIM:Y2=WLIM+30
13420       IF S=3 THEN X1=T1:X2=T2:Y1=-WLIM-30:Y2=-WLIM
13430       C=FN CC(1,13):IF K MOD 2=1 THEN C=FN CC(1,7)
13440       B=M3D("VERTS",O)
13450       M3D VERTEX O,X1,Y1,FN HT(X1,Y1)+2.5,0,0:M3D VERTEX O,X2,Y1,FN HT(X2,Y1)+2.5,0,0
13460       M3D VERTEX O,X2,Y2,FN HT(X2,Y2)+2.5,0,0:M3D VERTEX O,X1,Y2,FN HT(X1,Y2)+2.5,0,0
13470       M3D QUAD O,B,B+1,B+2,B+3,C
13480     NEXT K
13490   NEXT S
13500 END PROC
