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
195 LOAD LIBRARY "chunks"
190 LOAD LIBRARY "music3d":LOAD LIBRARY "banner3d":IF ENV$("G1987_HEADLESS")="" THEN CALL B3_M3D("1987")
200 BREAK OFF
210 ON ERROR GOTO FAILED
220 HL=0:IF ENV$("G1987_HEADLESS")<>"" THEN HL=-1
230 DIM PV(60),SGL(300),SGH(300),SGX(300),SGY(300),SGU(300),SGV(300),SGK(300),SGD(300),SGE(300),SGI(300),RGX(6),RGY(6),RGP(6),RGQ(6),CKN(400),CHB(400),CKRX(9),CKRY(9),CKRT(9),CTX(24),CTY(24),GNS(8),GGX(160),FLJ$(81),TIH(33),RPX(300),RPY(300),RPF(300),CPX(40),CPY(40),SMX(120),SMY(120),PLX(40),PLY(40),TRX(520),TRY(520),LRR(8),LRZ(8),LRV(8),W$(8),GPX(2),GPZ(2),GPS(2),DWX(40),DWY(40),DRX(5),DRY(5),DRZ(5),DRH(5),DRT(5),DRA(5)
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
480 ' the height of the ground: gentle hills, flat round the house and in the wheat field; the engine's ground function (SETGROUND defines it: fractal noise in C, since 2026-11-20;
490 ' before it was a sum of sines in BASIC)
500 FUNC HT(X,Y)
510   RETURN M3D("GROUND",X,Y)
520 END FUNC
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
600   LOCAL T
602   ' what the ground is made of: the picture of the floor under the feet (the engine's M3D("FLOORTEX"): the highest solid triangle below, so roads, planks, tiles and
604   ' the soil of the lakes are told by their pictures, not by the height); 0 grass (301 .. 303, carpet 323, anything else), 1 sand (road 304, mud 320), 2 wood / hard
606   ' (planks 305, tiles 306, stone 307, rock 324, brick 326), 3 wheat (308, 321, and the soil of the field)
610   T=M3D("FLOORTEX",X,Y,Z+3)
620   IF T=320 AND ABS(X-1100)<700 AND ABS(Y-650)<500 THEN RETURN 3
630   IF T=308 OR T=321 THEN RETURN 3
640   IF T=304 OR T=320 THEN RETURN 1
650   IF T=305 OR T=306 OR T=307 OR T=324 OR T=326 OR T=327 THEN RETURN 2
660   RETURN 0
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
2185   CALL MKROCK:CALL MKWATER:CALL W3K_BRICK(326,1):CALL MKTAR
2190   M3D SKY 311
2200 END PROC
2210 ' ------------------------------------------------------------------------------------------------ mesh helpers
2220 PROC BX(O,X1,Y1,Z1,X2,Y2,Z2,TX)
2230   IF XON THEN CALL BXX(O,X1,Y1,Z1,X2,Y2,Z2,TX) ELSE M3D BOX O,X1,Y1,Z1,X2,Y2,Z2,TX
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
2450   LOCAL AX,AY,AZ,L,PX,PY,PZ,QX,QY,QZ,B,I,SA,SB,VX,VY,VZ
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
2620       VX=X1+H*SA*PX+H*SB*QX:VY=Y1+H*SA*PY+H*SB*QY:VZ=Z1+H*SA*PZ+H*SB*QZ
2630     ELSE
2640       VX=X2+H*SA*PX+H*SB*QX:VY=Y2+H*SA*PY+H*SB*QY:VZ=Z2+H*SA*PZ+H*SB*QZ
2650     END IF
2655     IF XON THEN CALL PXV(O,VX,VY,VZ) ELSE M3D VERTEX O,VX,VY,VZ
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
2945 ' a vertex of a spoke ribbon (in the plane y = 0), through the tilt while XON is set
2946 PROC SPV(O,X,Z)
2947   IF XON THEN CALL PXV(O,X,0,Z) ELSE M3D VERTEX O,X,0,Z
2948 END PROC
2950 ' spokes: three ribbons through the hub, turned by ANG (radians); they are rebuilt each frame, so the wheel seems to turn
2960 PROC SPOKES(O,CX,CZ,ANG)
2970   LOCAL I,A,B,DX,DZ,TX,TZ,R
2980   R=9.6
2990   FOR I=0 TO 2
3000     A=ANG+I*PI/3
3010     DX=COS(A):DZ=SIN(A):TX=-DZ*0.7:TZ=DX*0.7
3020     B=M3D("VERTS",O)
3030     CALL SPV(O,CX-R*DX-TX,CZ-R*DZ-TZ):CALL SPV(O,CX+R*DX-TX,CZ+R*DZ-TZ)
3040     CALL SPV(O,CX+R*DX+TX,CZ+R*DZ+TZ):CALL SPV(O,CX-R*DX+TX,CZ-R*DZ+TZ)
3050     M3D QUAD O,B,B+1,B+2,B+3,TBT
3060   NEXT I
3070   CALL BX(O,CX-1.2,-1.6,CZ-1.2,CX+1.2,1.6,CZ+1.2,FN CC(0,11))
3080 END PROC
3090 ' ------------------------------------------------------------------------------------------------ the terrain, the roads
3100 ' ------------------------------------------------------------------------------------------------ the world in chunks (roadmap item 95, step 1: a prototype)
3101 ' The ground is cut into square chunks of 900 units (9 x 9 cells of 100). Only the chunks round John exist (7 x 7: 3 in every direction, so the nearest hole is more than 2700
3102 ' units away, beyond the far plane of 2400). A chunk is a pure function of its numbers cx,cy: the same chunk always comes out the same, however often it is built again. It owns a
3103 ' fixed block of object numbers by its slot, (cx mod 8) + 8 (cy mod 8): the ground 100 + slot, up to 24 trees 2000 + 24 slot + n; building a chunk overwrites the one that was
3104 ' in the slot, nothing is allocated or freed. The chunks cx, cy = -4 .. 3 are the hand-made area (the house, the roads, the groves, the field); the rest is made by the rules
3105 ' below. One chunk is built per frame (the nearest missing one first); a jump (NEWGAME, goto) builds all that are missing at once.
3106 ' what is near a road: the segments of the roads round the chunk (CHSEGS): -1 when X,Y is nearer than 100 to a dirt road's middle (55 wide, plus room) or than 310 to a tarred road's (220 wide
3107 ' and the posts beside it): no trees, no bales there
3108 FUNC NEARROAD(X,Y)
3109   LOCAL I,CL,UX,UY,TT,DX,DY
3110   FOR I=0 TO NSG-1
3111     CL=100:IF SGK(I) THEN CL=310
3112     IF X>MIN(SGX(I),SGU(I))-CL AND X<MAX(SGX(I),SGU(I))+CL AND Y>MIN(SGY(I),SGV(I))-CL AND Y<MAX(SGY(I),SGV(I))+CL THEN
3113       UX=SGU(I)-SGX(I):UY=SGV(I)-SGY(I)
3114       TT=((X-SGX(I))*UX+(Y-SGY(I))*UY)/(UX*UX+UY*UY):TT=MAX(0,MIN(1,TT))
3115       DX=SGX(I)+TT*UX-X:DY=SGY(I)+TT*UY-Y
3116       IF DX*DX+DY*DY<CL*CL THEN RETURN -1
3117     END IF
3118   NEXT I
3119   RETURN 0
3120 END FUNC
3121 ' the kind of ground at X,Y: the kind of the nearest of the nine feature points CKRX / CKRY / CKRT (set by CHREGIONS for the chunk): 0 plain, 1 meadow, 2 field, 3 forest
3122 FUNC GKIND(X,Y)
3123   LOCAL K,B,D,BD
3124   BD=1000000000000:B=0
3125   FOR K=0 TO 8
3126     D=(CKRX(K)-X)*(CKRX(K)-X)+(CKRY(K)-Y)*(CKRY(K)-Y)
3127     IF D<BD THEN BD=D:B=K
3128   NEXT K
3129   RETURN CKRT(B)
3130 END FUNC
3131 ' the picture of the ground cell round X,Y: the hand-made area keeps its CELLTEX; outside it rock (324) on high or steep ground, mud (320) in a lake, else the kind of ground: meadow lush grass, field soil, forest dry grass (needles)
3132 FUNC OCELL(X,Y)
3133   LOCAL K,H,S
3134   IF X>=-3600 AND X<3600 AND Y>=-3600 AND Y<3600 THEN RETURN FN CELLTEX(X,Y)
3135   H=FN HT(X,Y):S=ABS(FN HT(X+50,Y)-FN HT(X-50,Y))+ABS(FN HT(X,Y+50)-FN HT(X,Y-50))
3136   IF H>70 OR S>45 THEN RETURN 324
3137   IF H<WZ+1.5 THEN RETURN 320
3138   K=FN GKIND(X,Y):IF K>0 THEN RETURN VAL(MID$("303320302",K*3-2,3))
3139   RETURN FN CELLTEX(X,Y)
3140 END FUNC
3141 ' the feature points of the regions (1800 units = 2 chunks square) round the chunk's region: one point in each, anywhere in it, with a kind (38 % forest, 28 % meadow, 20 % field, 14 % plain)
3142 PROC CHREGIONS(CX,CY)
3143   LOCAL RX,RY,I,J,K,U
3144   RX=INT(CX/2):RY=INT(CY/2):K=0
3145   FOR J=-1 TO 1
3146     FOR I=-1 TO 1
3147       CKRX(K)=(RX+I)*1800+HASH(RX+I,RY+J,1)*1800
3148       CKRY(K)=(RY+J)*1800+HASH(RX+I,RY+J,2)*1800
3149       U=HASH(RX+I,RY+J,3)
3150       CKRT(K)=0
3151       IF U<0.86 THEN CKRT(K)=2
3152       IF U<0.66 THEN CKRT(K)=1
3153       IF U<0.38 THEN CKRT(K)=3
3154       K=K+1
3155     NEXT I
3156   NEXT J
3157 END PROC
3158 ' the chunk's decorations are freed (ruins, bales, posts) when it is made again for a coarser level or for another place
3159 PROC CHCLEAR(SL)
3160   M3D OBJFREE 14000+SL*32,14000+SL*32+31:M3D OBJFREE 11200+SL*6,11200+SL*6+5:M3D OBJFREE 8800+SL*6,8800+SL*6+5:CHB(SL)=0
3165 END PROC
3166 ' builds the chunk cx,cy into the slot SL at the level LV (0: 9 x 9 cells of 100 units up to 2 chunks away, 1: 3 x 3 of 300 up to 5, 2: one cell of 900; the chunks MORPH: level 0 from 900 to 1800 units from the player into the grid of 300, level 1 from 2700 to 4500 into the grid of 900; NEWC: it is a new chunk, not only a new level): the ground, the road
3167 ' ribbons and the trees are made by the engine's worker thread (M3D RING mode 1: the hand-made area, 2: with trees, 3: with cheap trees: the chunks 4 or more away), taken by M3D POLL;
3168 ' the water plane of a new chunk, and for the chunks up to 3 away the ruins, hay bales and reflector posts (made once, freed again when the chunk goes farther away)
3169 PROC CHUNKBUILD(CX,CY,SL,LV,NEWC)
3170   LOCAL O,X0,Y0,T0,N,ST,MD,HM,MA,MB
3171   T0=TI
3172   CALL CHREGIONS(CX,CY):IF NEWC THEN CHB(SL)=0:CALL CHWATER(CX,CY,SL)
3173   O=8000+SL
3174   IF M3D("OBJON",O)=0 THEN M3D OBJECT O,1,235
3175   X0=CX*900:Y0=CY*900
3176   HM=(CX>=-4 AND CX<=3 AND CY>=-4 AND CY<=3)
3177   N=9:ST=100:MD=2:MA=900:MB=1800:IF LV=1 THEN N=3:ST=300:MD=3:MA=2700:MB=4500
3178   IF LV=2 THEN N=1:ST=900:MD=3:MA=0:MB=0
3179   IF HM THEN MD=1
3180   M3D RING O,X0,Y0,N,ST,200,MD,0,0,0,MA,MB
3181   IF HM THEN
3182     IF NEWC THEN CALL CHCLEAR(SL)
3183   ELSE
3184     IF LV=0 AND NOT CHB(SL) THEN
3185       CALL CHSEGS(CX,CY):CALL CHRUINS(CX,CY,SL):CALL CHROADS(CX,CY,SL):CALL CHBALES(CX,CY,SL):CHB(SL)=-1
3186     END IF
3187     IF LV>=1 AND CHB(SL) THEN CALL CHCLEAR(SL)
3188   END IF
3189   T0=TI-T0:CKCNT=CKCNT+1:CKTOT=CKTOT+T0:IF T0>CKMAX THEN CKMAX=T0
3190 END PROC
3224 ' makes sure the chunks round PX,PY exist: builds up to MAXN of the missing ones (or those whose level changed), the nearest first (64 = all); chunks.lib keeps the book
3225 PROC CHUNKS(PX,PY,MAXN)
3226   LOCAL N
3227   CALL CK_UPDATE(PX,PY)
3228   FOR N=1 TO MAXN
3229     IF NOT FN CK_NEXT() THEN EXIT
3230     CALL CHUNKBUILD(FN CK_CX(),FN CK_CY(),FN CK_CSLOT(),FN CK_CLEVEL(),FN CK_CNEW())
3231     CALL CK_DONE
3232   NEXT N
3233 END PROC
3234 ' the first world: the tree kinds, the four rings, then the chunks round the start (a window of 17 x 17 chunks of 900 units)
3235 PROC BUILDTERRAIN
3236   LOCAL S
3237   CALL CK_SETUP(900,8,2,5)
3238   FOR S=0 TO 399
3239     CKN(S)=0:CHB(S)=0
3240   NEXT S
3241   FOR K=0 TO 5:RGX(K)=-999999:RGY(K)=-999999:RGP(K)=-999999:RGQ(K)=-999999:NEXT K:FOR K=2 TO 4:M3D OBJECT 27000+K,12,235:NEXT K:CKCNT=0:CKTOT=0:CKMAX=0:CALL TREEPROTOS
3242   CALL CHUNKS(0,0,400):CALL RINGUPDATE(0,0):M3D RINGWAIT
3243 END PROC
3244 ' test mode: what the window round PX,PY looks like (for game1987test.sh): the chunks present, missing or of the wrong level, those of level 0, the chunks with forest (a mesh of more than 40
3245 ' triangles), a signature (the triangles of every chunk), the ones with water / ruins / a road near / bales / posts, the rings and the ground jobs not taken yet
3246 PROC CHUNKSTAT(PX,PY)
3247   LOCAL PCX,PCY,I,J,CX,CY,SL,N,M,NT,SG,NF,NW,NR,NO,NB,NFT,K,NPS,NMD,TR
3248   PCX=INT(PX/900):PCY=INT(PY/900)
3249   FOR J=-8 TO 8
3250     FOR I=-8 TO 8
3251       CX=PCX+I:CY=PCY+J:SL=FN CK_SLOT(CX,CY)
3252       IF FN CK_HAS(CX,CY) THEN
3253         TR=M3D("TRIS",8000+SL):N=N+1:NT=NT-(TR>40):SG=SG+TR*(SL+1)
3254         NW=NW-(M3D("OBJON",8400+SL)<>0):NR=NR-(M3D("OBJON",14000+SL*32)<>0):NB=NB-(M3D("OBJON",11200+SL*6)<>0):NPS=NPS-(M3D("OBJON",8800+SL*6)<>0)
3255         NO=NO-(MIN(M3D("ROADD",CX*900+450,CY*900+450),M3D("ROADT",CX*900+450,CY*900+450))<500)
3256         IF FN CK_LEVELAT(CX,CY)=0 THEN NF=NF+1
3257       ELSE
3258         M=M+1
3259       END IF
3260     NEXT I
3261   NEXT J
3262   FOR K=2 TO 4:NFT=NFT-(M3D("OBJON",27000+K)<>0 AND M3D("TRIS",27000+K)>0):NEXT K:NMD=M3D("RINGBUSY")
3263   PRINT "chunks have=";N;" missing=";M;" trees=";NT;" sig=";SG;" cx=";PCX;" cy=";PCY;" fine=";NF;" stale=";FN CK_STALE();" water=";NW;" ruins=";NR;" roads=";NO;" bales=";NB;" rings=";NFT;" posts=";NPS;" busy=";NMD
3264   PRINT "chunktime built=";CKCNT;" ticks=";CKTOT;" max=";CKMAX
3265 END PROC
3270 ' the ground function of the engine: hills of fractal noise (about 900 units across, 4 octaves, up to about 30 high), flattened round the house and over the wheat field
3271 PROC SETGROUND
3272   M3D GROUND 1987,900,22,4,8
3273   M3D GROUNDFLAT 0,0,0,260,200,420
3274   M3D GROUNDFLAT 1,1100,650,700,500,300
3275   ' a unit is 3.2 cm: HILLS (ridged noise up to 260 = 8 m high, rocky and walkable), real MOUNTAINS (peaks up to 37000 = 1190 m high, steep walls, 14000 .. 22000 units from the house: ridged noise where a very slow noise is high) and real LAKES (basins 350 = 11 m deep and 3000 .. 6000 = 100 .. 190 m across); none over the hand-made area (3900 units round the house, fading in over 1200)
3276   M3D GROUNDQUIET 0,0,3900,3900,1200
3277   M3D ROADNET 1800,0.45,140,36000,0.85,900:M3D RINGPIC 325,324,301,327,328,320,303,320,302,WZ,70,304:M3D TREEPIC 312,309,310,317:M3D HILLS 1987,500,260,5,0.05,0.4:M3D MOUNTAINS 7,9000,80000,4,0.25,0.6,14000,22000
3278   M3D LAKES 4711,9000,350,0.3,0.5
3279 END PROC
3280 ' what stops John and the bicycle: only water too deep to wade (12 below the water level WZ); the rocky hills are walkable and rideable (the engine lets a body climb every slope up to 72 degrees and lifts one that got buried)
3281 FUNC BLOCKED(X,Y)
3284   RETURN FN HT(X,Y)<WZ-12
3287 END FUNC
3288 ' the water of a chunk: a translucent plane at the water level WZ over the chunk when the ground dips below it somewhere (5 x 5 samples), else none (object 4000 + slot)
3289 PROC CHWATER(CX,CY,SL)
3290   LOCAL I,J,W,MN,X0,Y0
3291   W=8400+SL:X0=CX*900:Y0=CY*900:MN=1000
3292   FOR J=0 TO 4
3293     FOR I=0 TO 4
3294       MN=MIN(MN,FN HT(X0+I*225,Y0+J*225))
3295     NEXT I
3296   NEXT J
3297   IF MN<WZ THEN
3298     M3D OBJECT W,30,235
3299     M3D VERTEX W,X0,Y0,WZ,X0*0.5,Y0*0.5:M3D VERTEX W,X0+900,Y0,WZ,(X0+900)*0.5,Y0*0.5
3300     M3D VERTEX W,X0+900,Y0+900,WZ,(X0+900)*0.5,(Y0+900)*0.5:M3D VERTEX W,X0,Y0+900,WZ,X0*0.5,(Y0+900)*0.5
3301     M3D QUAD W,0,1,2,3,325
3302   ELSE
3303     M3D OBJFREE W,W
3304   END IF
3305 END PROC
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
3840 ' the three kinds of tree (K: 0 broadleaf, 1 pine, 2 poplar) are built once, hidden, as the objects 340 .. 342; TREE makes object O an instance of one (it shares the mesh, M3D INSTANCE)
3841 ' scaled by S, standing at X,Y: the crown starts above 72 units, so you can walk and ride under it
3850 PROC TREEPROTO(K)
3860   LOCAL J,O,S
3865   O=340+K:S=1
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
4070   M3D OBJSHOW O,0
4080 END PROC
4081 PROC TREE(O,X,Y,S,K)
4082   LOCAL YW
4083   YW=TYW:IF YW<0 THEN YW=FN RN(360)
4084   M3D INSTANCE O,340+K,X,Y,FN HT(X,Y)-2,YW,S
4085 END PROC
4086 PROC TREEPROTOS
4087   LOCAL K
4088   FOR K=0 TO 2:CALL TREEPROTO(K):NEXT K:CALL MKBALE:CALL MKPOST
4089 END PROC
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
5950 ' the bicycle: the frame (x forward, y left, z up, the origin on the ground halfway between the axles) and the front (fork, stem, handlebar and wheel, turning about the
5951 ' steering axis; origin: on the ground below it, 12 in front of the frame's origin) are procedures of an object O, so that they can be built upright (objects 510 and 511) and
5952 ' tilted by the transform of BUILDLEAN into one object (514) while the bicycle leans against a wall, falls, slides or lies
5953 PROC BIKEFRAME(O)
5960   TBT=FN CC(1,9)
5962   CALL TUBE(O,-2,0,9,-8,0,25,1.3)
5964   CALL TUBE(O,-8,0,24,12,0,26,1.2)
5966   CALL TUBE(O,-2,0,9,12,0,22,1.3)
5968   CALL TUBE(O,12,0,21,12,0,29,1.6)
5970   CALL TUBE(O,-2,0,9,-17,0,11,0.8)
5972   CALL TUBE(O,-8,0,24,-17,0,11,0.8)
5974   TBT=FN CC(0,6)
5976   CALL TUBE(O,-8,0,25,-9,0,30,0.9)
5978   CALL BX(O,-14,-2.6,29.4,-4,2.6,31,FN CC(12,4))
5980   CALL BX(O,-18,-1,0,-16,1,2,FN CC(0,3))
5982   TBT=FN CC(0,2)
5984   CALL RING(O,-17,0,11,10.5)
5986   CALL BX(O,-18.5,-3,15,-15.5,3,16.5,FN CC(0,4))
5988 END PROC
5990 PROC BIKEFRONT(O)
5992   TBT=FN CC(0,9)
5994   CALL TUBE(O,0,0,26,6,0,11,0.8)
5996   CALL TUBE(O,0,0,26,-1,0,31,0.9)
5998   CALL TUBE(O,-2,-10,31,-2,10,31,0.8)
6000   TBT=FN CC(0,2)
6002   CALL RING(O,6,0,11,10.5)
6004   CALL BX(O,-3.5,-12,29.8,-0.5,-8,32.2,FN CC(0,1)):CALL BX(O,-3.5,8,29.8,-0.5,12,32.2,FN CC(0,1))
6006   CALL BX(O,3,-1,36,5,1,38,FN CC(3,14))
6008 END PROC
6010 PROC BUILDBIKE
6013   M3D OBJECT 510,28,235:CALL BIKEFRAME(510)
6016   M3D OBJECT 511,28,235:CALL BIKEFRONT(511)
6019   M3D OBJECT 512,28,235
6022   M3D OBJECT 513,28,235
6025   M3D OBJECT 514,28,235:M3D OBJSHOW 514,0
6028   M3D OBJECT 530,30,235:CALL BX(530,-9,-8,0,9,8,0.3,FN CC(0,0))
6031   M3D OBJECT 531,30,235:CALL BX(531,-27,-6,0,27,6,0.3,FN CC(0,0))
6034   M3D OBJPOS 510,0,0,0,0:M3D OBJPOS 511,12,0,0,0:M3D OBJPOS 512,0,0,0,0:M3D OBJPOS 513,12,0,0,0
6037 END PROC
6040 ' the parts of the bicycle that are built again each frame: the rear wheel's spokes with the cranks and pedals (object O in the frame's coordinates) ...
6043 PROC BIKEREAR(O)
6046   LOCAL A,PX,PZ,Q
6049   TBT=FN CC(0,11)
6052   CALL SPOKES(O,-17,11,-WA)
6055   TBT=FN CC(0,10)
6058   FOR Q=0 TO 1
6061     A=-(CA+Q*PI)
6064     PX=-2+5.5*COS(A):PZ=9+5.5*SIN(A)
6067     IF Q=0 THEN
6070       CALL TUBE(O,-2,3.5,9,PX,3.5,PZ,0.7)
6073       CALL BX(O,PX-3,3.6,PZ-0.7,PX+3,7.6,PZ+0.7,FN CC(0,3))
6076     ELSE
6079       CALL TUBE(O,-2,-3.5,9,PX,-3.5,PZ,0.7)
6082       CALL BX(O,PX-3,-7.6,PZ-0.7,PX+3,-3.6,PZ+0.7,FN CC(0,3))
6085     END IF
6088   NEXT Q
6091 END PROC
6094 ' ... and the front wheel's spokes (in the front's coordinates)
6097 PROC BIKEFR(O)
6100   TBT=FN CC(0,10)
6103   CALL SPOKES(O,6,11,-WA)
6106 END PROC
6109 ' places the bicycle's objects at its position and heading (upright: the objects 510 .. 513, the spokes and the pedals are built again each frame; leaning, falling,
6112 ' sliding or lying: the one tilted object 514, see PLACELEAN)
6115 PROC PLACEBIKE
6118   LOCAL C,S,FX,FY
6121   IF BST<>0 THEN
6124     CALL PLACELEAN
6127     EXIT PROC
6130   END IF
6133   IF BVIS=0 THEN
6136     BVIS=-1:M3D OBJSHOW 514,0:M3D OBJSHOW 510,1:M3D OBJSHOW 511,1:M3D OBJSHOW 512,1:M3D OBJSHOW 513,1
6139   END IF
6142   C=COS(BH*PI/180):S=SIN(BH*PI/180)
6145   FX=BX+12*C:FY=BY+12*S
6148   M3D OBJPOS 510,BX,BY,BZ,BH
6150   ' the spokes and the pedals depend on the wheel angle WA and the crank angle CA only: a bicycle that stands still keeps its meshes
6151   IF WA<>PWA OR CA<>PCA THEN
6152     PWA=WA:PCA=CA:CNB=CNB+1
6153     M3D OBJECT 512,28,235:CALL BIKEREAR(512)
6154     M3D OBJECT 513,28,235:CALL BIKEFR(513)
6155   END IF
6157   M3D OBJPOS 512,BX,BY,BZ,BH
6160   M3D OBJPOS 511,FX,FY,BZ,BH+BSA
6163   M3D OBJPOS 513,FX,FY,BZ,BH+BSA
6166 END PROC
6169 ' the tilt of the bicycle: every vertex of the parts built while XON is set goes through PXV: the front's parts (XFR) are first turned about the steering axis (12 in front of
6172 ' the frame's origin) by the angle SD, then everything is rolled about the line where the wheels touch the ground (the x axis) by the lean LA (positive: the top goes to the left,
6175 ' +y) and lifted by XLF so that the handlebar does not sink into the ground
6178 PROC PXV(O,X,Y,Z)
6181   LOCAL XX,YY
6184   XX=X:YY=Y
6187   IF XFR THEN XX=12+X*XFC-Y*XFS:YY=X*XFS+Y*XFC
6190   M3D VERTEX O,XX,YY*XRC+Z*XRS,Z*XRC-YY*XRS+XLF
6193 END PROC
6196 ' a box through the tilt (the six faces are quads: a rolled box is not axis aligned any more)
6199 PROC BXX(O,X1,Y1,Z1,X2,Y2,Z2,TX)
6202   LOCAL B
6205   B=M3D("VERTS",O)
6208   CALL PXV(O,X1,Y1,Z1):CALL PXV(O,X2,Y1,Z1):CALL PXV(O,X2,Y2,Z1):CALL PXV(O,X1,Y2,Z1)
6211   CALL PXV(O,X1,Y1,Z2):CALL PXV(O,X2,Y1,Z2):CALL PXV(O,X2,Y2,Z2):CALL PXV(O,X1,Y2,Z2)
6214   M3D QUAD O,B,B+1,B+2,B+3,TX:M3D QUAD O,B+4,B+5,B+6,B+7,TX
6217   M3D QUAD O,B,B+1,B+5,B+4,TX:M3D QUAD O,B+1,B+2,B+6,B+5,TX
6220   M3D QUAD O,B+2,B+3,B+7,B+6,TX:M3D QUAD O,B+3,B,B+4,B+7,TX
6223 END PROC
6226 ' the whole bicycle in one object (514), rolled by the lean LA (radians) with the front wheel turned by SD degrees
6229 PROC BUILDLEAN(LA,SD)
6232   LOCAL Y1,Y2
6235   XRS=SIN(LA):XRC=COS(LA):XFS=SIN(SD*PI/180):XFC=COS(SD*PI/180)
6238   Y1=-2*XFS+12*XFC:Y2=-2*XFS-12*XFC
6241   XLF=-MIN(0,MIN(31*XRC-Y1*XRS,MIN(31*XRC-Y2*XRS,-1.4*ABS(XRS))))
6244   M3D OBJECT 514,28,235
6247   XON=-1:XFR=0
6250   CALL BIKEFRAME(514):CALL BIKEREAR(514)
6253   XFR=-1
6256   CALL BIKEFRONT(514):CALL BIKEFR(514)
6259   XON=0:XFR=0
6262 END PROC
6265 PROC PLACELEAN
6268   IF BVIS<>0 THEN
6271     BVIS=0:M3D OBJSHOW 510,0:M3D OBJSHOW 511,0:M3D OBJSHOW 512,0:M3D OBJSHOW 513,0:M3D OBJSHOW 514,1
6274   END IF
6277   IF BDR<>0 OR BST=3 OR BST=6 THEN
6280     CALL BUILDLEAN(BLS*BLN,BSTR):BDR=0
6283   END IF
6286   M3D OBJPOS 514,BX,BY,BZ,BH
6289 END PROC
6292 ' a bicycle cannot stand by itself. BST: 0 upright (ridden, or rolling on without a rider for WFALL seconds), 1 leaning against the house (the start), 3 falling over (the lean BLN
6295 ' grows to 90 degrees, to the side BLS), 4 sliding on its side and turning (BYW) while it slows down (WSLIDE), 5 lying, 6 being picked up (E: the lean goes back to 0 in the time of getting on).
6298 ' A slide goes straight on in the direction BSD while the bicycle turns about itself.
6301 PROC BIKEFALL(DT)
6304   LOCAL A
6307   IF MODE=1 THEN
6310     BST=0:BRT=0
6313     EXIT PROC
6316   END IF
6319   IF BST=0 THEN
6322     IF MODE=2 THEN
6325       BRT=0
6328     ELSE
6331       BRT=BRT+DT
6334       IF BRT>=WFALL THEN
6337         BST=3:BLN=0:BLW=0.4:BLS=1:IF (INT(BX+BY) AND 1)<>0 THEN BLS=-1
6340         BSTR=BLS*55:BDR=-1
6343       END IF
6346     END IF
6349   END IF
6352   IF BST=3 THEN
6355     BLW=BLW+9*DT:BLN=BLN+BLW*DT
6358     IF BLN>=1.5708 THEN
6361       BLN=1.5708:BST=4:BSD=BH:BDR=-1
6364       A=BX-JX:IF A*A+(BY-JY)*(BY-JY)<490000 THEN CALL SND(4)
6367     END IF
6370   END IF
6373   IF BST=4 THEN
6376     IF BV>0 THEN BV=MAX(0,BV-WSLIDE*DT)
6379     IF BV<0 THEN BV=MIN(0,BV+WSLIDE*DT)
6382     BYW=BLS*2.6*MIN(1,ABS(BV)/120)
6385     BH=BH+BYW*DT*180/PI
6388     IF ABS(BV)<1 THEN BV=0:BYW=0:BST=5
6391   END IF
6394   IF BST=6 THEN
6397     A=MIN(1,MT/0.45):BLN=BLN0*(1-A):BSTR=BSTR0*(1-A)
6400     IF A>=1 THEN BST=0:BRT=0:BLN=0
6403   END IF
6406   IF BST=1 OR BST=5 OR BST=6 THEN BV=0
6409 END PROC
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
6810   LOCAL Q,SG,FX,FZ,HX,HZ,PX,PZ,A,HY,GX,GY,LN,HIPX,HIPZ,SHX,SHZ,GXL,GYL,HH,SWY,STY,BRS
6830   CC1=FN CC(5,6):CC2=FN CC(8,5):CC3=FN CC(14,11):CC4=FN CC(0,12):CC5=FN CC(0,14)
6831   ' the mesh cache: the body (object 520, without the head) depends on nothing but the pose, the walk phase, the stride, the speed (the sway), the pedals, the steering and the breath
6832   ' (rounded: the breath to a quarter of its range, the crank angle to 0.025 radians, the steering angle to 0.2 degrees), so when all of these are as at the last build the mesh is still right and is not built again (the object keeps its triangles)
6833   BRS=INT(SIN(BRP)*4+0.5)/4
6834   IF POSE=BJ1 AND PH=BJ2 AND AM=BJ3 AND INT(ABS(JV))=BJ4 AND INT(CA*40)=BJ5 AND INT(BSA*5)=BJ6 AND BRS=BJ7 THEN EXIT PROC
6835   BJ1=POSE:BJ2=PH:BJ3=AM:BJ4=INT(ABS(JV)):BJ5=INT(CA*40):BJ6=INT(BSA*5):BJ7=BRS:CNJ=CNJ+1
6836   M3D OBJECT 520,28,235
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
7010     HIPX=0:SHX=0:SHZ=HIPZ+16+0.35*BRS
7012     ' the sway: the pelvis moves sideways over the leg that carries the weight (that leg is on the ground from phase 0 to 0.6, its middle at 0.3), the upper body follows
7013     ' with a little over half of it, and each hip goes forward with its leg (the pelvis turns: 0.12 of the foot's distance)
7014     SWY=MIN(1,ABS(JV)/WSLOW)*(0.8+0.8*AM)*SIN(PH-0.314):STY=SWY*0.55
7015     ' the pelvis: a trouser colored block wider than the torso's waist, under it; the legs join it at the sides
7016     ' (its top is exactly the torso's bottom: 3.6 x 6.0 half size; it flares to 3.9 x 6.5 at the thighs; a belt covers the seam)
7017     F1X=3.9:F1Y=6.5:F2X=3.6:F2Y=6.0:FC1X=0:FC1Y=SWY:FC2X=0:FC2Y=SWY
7018     CALL FRUST(520,HIPZ-4.5,HIPZ+0.5,CC2)
7019     CALL BX(520,-3.75,-6.15+SWY,HIPZ-0.4,3.75,6.15+SWY,HIPZ+0.9,BELT)
7020     ' the torso narrows to the waist (3.6 x 6.0) and widens to the chest (4.4 x 7.4)
7021     F1X=3.6:F1Y=6.0:F2X=4.4*(1+0.03*BRS):F2Y=7.4*(1+0.03*BRS):FC1X=0:FC1Y=SWY:FC2X=0:FC2Y=STY
7022     CALL FRUST(520,HIPZ+0.5,SHZ+0.5,CC1)
7030     CALL COLLAR(520,0,STY,SHZ+0.5):HOX=0:HOY=STY:HOZ=SHZ+2.5
7040     FOR Q=0 TO 1
7050       SG=1:IF Q=1 THEN SG=-1
7060       FX=GPX(Q):FZ=GPZ(Q)
7070       PLX0=1:PLY0=0:PLZ0=0.1:LH=2.6:TBT=FN CC(8,5):TBU=TBT
7080       CALL LIMB(520,0.12*FX,SG*3.6+SWY,HIPZ,FX,SG*4,FZ,14)
7090       CALL SHOE(520,FX-3.5,SG*4,FZ-2.8)
7100       HX=-0.7*FX
7110       PLX0=-1:PLY0=SG*0.35:PLZ0=-0.1:LH=2.3:TBT=CC1:TBU=CC3
7120       CALL LIMB(520,0,SG*8.5+STY,SHZ-1.2,HX,SG*9.5+STY,19+2.8,11)
7125       ' the sleeve ends at the elbow with a cuff (a lighter green band)
7126       TBT=FN CC(5,10):CALL TUBE(520,KX+(0-KX)*0.22,KY+(SG*8.5+STY-KY)*0.22,KZ+(SHZ-1.2-KZ)*0.22,KX,KY,KZ,2.9)
7130       CALL HAND(520,HX,SG*9.5+STY,21.8,SG)
7140     NEXT Q
7150   ELSE
7160     HIPX=-9.5:HIPZ=32.4:SHX=-3:SHZ=46.4
7170     TBT=CC1
7175     CALL BX(520,HIPX-4.2,-6.8,HIPZ-3.5,HIPX+4.2,6.8,HIPZ+3.5,CC2)
7180     CALL TUBE(520,HIPX,3.4,HIPZ,SHX,3.4,SHZ,3.4):CALL TUBE(520,HIPX,-3.4,HIPZ,SHX,-3.4,SHZ,3.4)
7190     CALL COLLAR(520,SHX,0,SHZ+0.5):HOX=SHX+4:HOY=0:HOZ=SHZ+2.5
7200     FOR Q=0 TO 1
7210       SG=1:IF Q=1 THEN SG=-1
7220       A=-(CA+Q*PI)
7230       PX=-2+5.5*COS(A):PZ=9+5.5*SIN(A)
7240       PLX0=1:PLY0=0:PLZ0=0.15:LH=2.6:TBT=FN CC(8,5):TBU=TBT
7250       CALL LIMB(520,HIPX,SG*3.8,HIPZ,PX-0.5,SG*5.6,PZ+4.2,14.8)
7260       CALL SHOE(520,PX-3.5,SG*5.6,PZ+0.8)
7270       GXL=-2:GYL=SG*10
7280       GX=12+GXL*COS(BSA*PI/180)-GYL*SIN(BSA*PI/180)-12:GY=GXL*SIN(BSA*PI/180)+GYL*COS(BSA*PI/180)
7290       PLX0=-0.4:PLY0=SG*0.9:PLZ0=-0.3:LH=2.3:TBT=CC1:TBU=CC3
7300       CALL LIMB(520,SHX,SG*7,SHZ-1.5,12+GX,GY,31.8,11)
7305       TBT=FN CC(5,10):CALL TUBE(520,KX+(SHX-KX)*0.22,KY+(SG*7-KY)*0.22,KZ+(SHZ-1.5-KZ)*0.22,KX,KY,KZ,2.9)
7310       CALL FIST(520,12+GX,GY)
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
9010   CALL CAMAGENT(DT)
9020 END PROC
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
9592   ' walking the bike round: at a walking pace (below 25 units a second) the steering also turns it on the spot, WSTAND degrees a second at a standstill (so a rider pinned against something can always turn away)
9594   IF MODE=1 THEN OM=OM+BS*WSTAND*MAX(0,1-ABS(BV)/25)
9600   BH=BH+OM*DT
9605   A=BH:IF BST=4 THEN A=BSD
9610   DX=COS(A*PI/180)*BV*DT:DY=SIN(A*PI/180)*BV*DT
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
9725   IF ABS(BV)>0.3 AND FN BLOCKED(M3D("TX",1),M3D("TY",1)) THEN M3D THINGPOS 1,BX,BY,BZ:BV=0
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
9840   IF MODE<>1 AND BST=0 AND TK>70 AND ABS(BV)>25 THEN TK=0:CALL SND(3)
9850   IF MODE=1 AND TH<=0 AND TK>70 AND BV>25 THEN TK=0:CALL SND(3)
9855   CALL BIKEFALL(DT)
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
9985       IF FN BLOCKED(M3D("TX",0),M3D("TY",0)) THEN M3D THINGPOS 0,JX,JY,JZ:JV=0:KTS=0
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
10120     IF KU<>0 AND NEAR AND (BST=0 OR BST=1 OR BST=5) THEN
10130       MODE=2:MT=0:MFX=JX:MFY=JY:MFZ=JZ:CALL SND(2)
10135       IF BST>0 THEN BST=6:BLN0=BLN:BSTR0=BSTR
10140     END IF
10150   END IF
10160   IF MODE=2 THEN
10170     MT=MT+DT
10180     A=MT/0.45:IF A>1 THEN A=1
10185     A=A*A*(3-2*A)
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
10375     A=A*A*(3-2*A)
10380     JX=MFX+(MGX-MFX)*A:JY=MFY+(MGY-MFY)*A:JZ=MFZ+(MGZ-MFZ)*A
10390     PH=PH+DT*7
10400     IF MT>=0.4 THEN
10410       MODE=0:JV=0:M3D THINGPOS 0,MGX,MGY,MGZ:JX=MGX:JY=MGY:JZ=MGZ
10420     END IF
10430   END IF
10435   ' (after the stamina: breathing, about 0.25 breaths a second at rest, faster when out of breath or moving fast, and the head looking around) stamina: it drains while sprinting (bicycle) or walking fast, else 0.12 a second come back; at 0.02 he is used up, until it is back at 0.25
10436   IF DRN>0 THEN ST=MAX(0,ST-DRN*DT) ELSE ST=MIN(1,ST+0.12*DT)
10437   IF ST<=0.02 THEN EXH=-1
10438   IF ST>=0.25 THEN EXH=0
10439   BRP=BRP+DT*2*PI*(0.25+0.45*(1-ST)+0.25*MIN(1,ABS(JV)/WFAST)):IF BRP>1000 THEN BRP=BRP-1000*PI
10440   CALL DRONESIM(DT):CALL HEADLOOK(DT)
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
10930   ' the camera controls are instructions to the camera (an agent, see CAMAGENT): CMIN says that one is given now; letting go lets the camera go back to its own business
10940   CMIN=0
10950   IF K<>0 OR KEY("R") OR KEY("F") OR KEY("PGUP") OR KEY("PGDN") OR JOY("RY")<>0 OR (MOUSE("B") AND 2)<>0 THEN CMIN=-1
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
11180     GFX TEXT 8,22,"E: GET ON THE BICYCLE",FN CC(3,15)
11190   END IF
11200   IF MSGT>0 AND (MODE<>0 OR NOT NEAR) THEN
11210     GFX TEXT (640-LEN(MSG$)*8)\2+1,23,MSG$,FN CC(0,1)
11220     GFX TEXT (640-LEN(MSG$)*8)\2,22,MSG$,FN CC(3,15)
11230   END IF
11235   CALL CAMHUD
11240 END PROC
11250 PROC RIGS
11260   CALL DRONERIG
11270   IF MODE=1 THEN
11280     CALL BUILDJOHN(1,PH,0)
11290     M3D OBJPOS 520,BX,BY,BZ,BH:CALL PLACEHEAD(BX,BY,BZ,BH)
11300   ELSE
11310     AM=FN SPD1(JV):IF MODE>=2 THEN AM=0.4
11320     CALL BUILDJOHN(0,PH,AM)
11330     M3D OBJPOS 520,JX,JY,JZ,JH:CALL PLACEHEAD(JX,JY,JZ,JH)
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
11520   JX=0:JY=-172:JH=90:JV=0:CALL CHUNKS(JX,JY,400):CALL RINGUPDATE(JX,JY):M3D RINGWAIT:M3D MORPH JX,JY:JZ=FN FLR(JX,JY)
11530   BX=130:BY=-214:BH=0:BV=0:BS=0:BSA=0:BZ=FN FLR(BX,BY):WA=0:CA=0:ST=1
11535   ' the bicycle leans against the south fence of the yard, on the outside, east of the gate, the handlebar turned towards the fence (bike state 1, see BIKEFALL)
11536   BST=1:BLN=0.35:BLS=1:BSTR=50:BRT=0:BYW=0:BSD=0:BDR=-1:BVIS=-1:BLW=0:BLN0=0:BSTR0=0
11540   M3D THINGPOS 0,JX,JY,JZ:M3D THINGPOS 1,BX,BY,BZ
11550   CMINIT=0:CMB=0:CMT=0:CMIDLE=0:CMNEXT=7:CMHOLD=0:CMAZ=0:CMEL=0:CMIN=0:CMLASTB=0:CMPC=-1:DWI=0:ZOI=0:CDI=0:CZI=0:CZM=0:CLP=0:NSL=0:MODE=0:MT=0:PH=0:KTS=0:KFA=0:KFD=0:KRA=0:KSH=0:KLT=0:EXH=0:ST=1:STD=0:TK=0:CYW=JH:CO=0:CP=0:KU=0:UL=0:BL=0:NEAR=0:MSGT=0:MB=0
11560   CALL SAY("WALK WITH W A S D. THE BICYCLE LEANS AGAINST THE FENCE OUTSIDE THE GATE, TO THE EAST: STAND NEAR IT AND PRESS E.")
11570   MSGT=14
11580 END PROC
11590 PROC PLAYLOOP
11600   LOCAL T0,FR,Y,YL,ST0
11610   T0=TI:YL=0:NFR=0:FL$=ENV$("G1987_FRAMELOG"):FLN=0:FLN2=10000:IF ENV$("G1987_FRAMES")<>"" THEN FLN2=VAL(ENV$("G1987_FRAMES"))
11612   CALL SOUNDSTART
11620   REPEAT
11622     ' the frame time: TI counts whole 1/60 s ticks, and a loop that waits for the display sees 0, 1 or 2 ticks in a frame by turns (measured under Xvfb: 0 in 36 %, 1 in 28 %,
11624     ' 2 in 36 % of the frames), so everything that moves by the frame time stutters, or jumps now and then; the time of a frame is the average over the last 32 frames
11626     NFR=NFR+1:TIK=NFR MOD 32
11630     DT0=(TI-T0)/60:T0=TI
11632     IF NFR>32 THEN DT0=(TI-TIH(TIK))/32/60
11634     TIH(TIK)=TI
11640     IF DT0>0.1 THEN DT0=0.1
11641     IF FL$<>"" THEN CALL FRAMELOG
11650     DTF=DT0
11660     CALL READKEYS
11670     IF KEY("ESC") THEN IN_QUIT=1
11680     CALL SIM(DT0):CALL AMBIENT:CALL CHUNKS(JX,JY,1):CALL RINGUPDATE(JX,JY):M3D POLL:M3D MORPH JX,JY
11690     CALL DRAWFRAME
11700     IF FN IN_SERVICE() THEN
11710       CALL BIGTEXT(110,"PAUSED",FN CC(0,15),3)
11720       CALL W3K_SHOW
11730       CALL SOUNDMUTE
11740       CALL IN_PAUSELOOP
11750       T0=TI:W3K_T0=TI:NFR=0
11760     END IF
11770     Y=JOY("Y")
11780     IF IN_KEY=79 OR (Y<>0 AND YL=0) THEN
11790       CALL P3_DIALOG
11800       T0=TI:NFR=0
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
12320   LOCAL L$,C$,I,N,PF,PT,PR,PB,T0,PSH,PRG,PCO
12330   PF=0:PFA=0:PT=0:PR=0:PB=0:PSH=0:PRG=0:PCO=0
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
12440       JX=VAL(W$(1)):JY=VAL(W$(2)):JH=VAL(W$(3)):CALL CHUNKS(JX,JY,400):CALL RINGUPDATE(JX,JY):M3D RINGWAIT:M3D MORPH JX,JY:JZ=FN FLR(JX,JY):JV=0:M3D THINGPOS 0,JX,JY,JZ:CYW=JH:CMINIT=0
12450     END IF
12460     IF C$="bike" THEN
12470       BX=VAL(W$(1)):BY=VAL(W$(2)):BH=VAL(W$(3)):BZ=FN FLR(BX,BY):BV=0:BST=0:BRT=0:BLN=0:BDR=-1:M3D THINGPOS 1,BX,BY,BZ
12480     END IF
12490     IF C$="wait" THEN
12500       N=INT(VAL(W$(1))*60)
12510       FOR I=1 TO N
12520         KFD=PF:KFA=PFA:KF=MAX(-1,MIN(1,PF+PFA)):KT=PT:KLT=PR:KSH=0:IF PSH<>0 THEN KSH=-1
12521         KRA=KLT:IF KSH<>0 THEN KRA=1
12522         KR=0:IF KRA>0 THEN KR=-1
12523         KB=PB
12524         CMIN=0:IF PCO<>0 THEN CO=CO+PCO*80/60:CMIN=-1
12530         CALL SIM(1/60):CALL CHUNKS(JX,JY,1):CALL RINGUPDATE(JX,JY):M3D POLL:M3D MORPH JX,JY
12535         IF PRG<>0 THEN CALL RIGS
12540         KU=0
12550         DTF=1/60
12560         CALL CAMUPDATE(1/60)
12570       NEXT I
12580     END IF
12585     IF C$="chunks" THEN CALL CHUNKSTAT(JX,JY)
12590     IF C$="state" THEN
12600       PRINT "state mode=";MODE;" jx=";INT(JX);" jy=";INT(JY);" jz=";INT(JZ+0.5);" jh=";INT(JH);" jv=";INT(JV);" bst=";BST;" nj=";CNJ;" nh=";CNH;" nb=";CNB;" cmb=";CMB;" hly=";INT(HLY);" hlp=";INT(HLP);" brp=";INT(BRP*100);" bx=";INT(BX);" by=";INT(BY);" bz=";INT(BZ+0.5);" bh=";INT(BH);" bv=";INT(BV);" st=";INT(ST*100);" gh=";INT(FN HT(JX,JY));" sf=";FN SURF(JX,JY,JZ)
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
12760     IF C$="clp" THEN CLP=VAL(W$(1)):CMINIT=0
12761     IF C$="pop" THEN CALL POPTEST(VAL(W$(1)),VAL(W$(2)))
12770     IF C$="zoom" THEN CZM=VAL(W$(1)):CMINIT=0
12780     IF C$="co" THEN CO=VAL(W$(1)):CYW=JH:CMINIT=0
12790     IF C$="cp" THEN CP=VAL(W$(1)):CMINIT=0
12791     IF C$="orbit" THEN PCO=VAL(W$(1))
12795     IF C$="look" THEN HLY=VAL(W$(1)):HLP=VAL(W$(2))
12796     IF C$="rigs" THEN PRG=VAL(W$(1))
12797     IF C$="playloop" THEN CALL PLAYLOOP
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
12959 WPED=140:WTIRE=100:WBMIN=40:BELT=FN CC(12,3)
12961 ' the limit John and the bicycle are kept inside (far away: the world is made of chunks as you go, see CHUNKS); TYW = -1: a tree gets a random yaw
12962 WLIM=50000:TYW=-1:WZ=-12:MTLIM=120:SLLIM=22:WSTAND=45
12963 ' the bicycle without a rider: it rolls on upright for WFALL seconds, then falls over and slides on its side (deceleration WSLIDE units a second squared); XON / XFR: tilt on
12964 CGI=0:CGT=0:CMX=0:CMY=0:CMZ=0:CMVX=0:CMVY=0:CMVZ=0:CMYW=0:CMPT=0:CMWY=0:CMWP=0:CMPX=0:CMPY=0:CMPZ=0:CMPX0=0:CMPY0=0:CMPZ0=0:CMTVX=0:CMTVY=0:CMTVZ=0:CMPI=0:CMPK=0:CMDUR=0:CMAZR=0:HLY=0:HLP=0:BRP=0:HQY=0:HQP=0:HKEY=99999:HOX=0:HOY=0:HOZ=0:BJ1=-1:BJ2=0:BJ3=0:BJ4=0:BJ5=0:BJ6=0:BJ7=0:PWA=-99999:PCA=-99999:CNJ=0:CNH=0:CNB=0:HCX=0:HCY=0:HCZ=0:HYS=0:HYC=1:HPS=0:HPC=1:WFALL=3:WSLIDE=120:XON=0:XFR=0:XRS=0:XRC=1:XFS=0:XFC=1:XLF=0
12960 IF NOT HL THEN CALL W3K_SETUP:CALL W3K_MESH:M3D RESET:CALL MAKETEX
12965 CALL SETGROUND
12966 ' the sun: terrain cells get pictures shaded by their slope (M3D SUN: steps 301.. + 100, 200, 300; needs the palette's color map first)
12967 IF NOT HL THEN M3D COLORMAP
12968 M3D SUN -0.55,-0.35,0.75,0.55,4,100
12970 CALL BUILDTERRAIN
12980 CALL BUILDROADS
12990 CALL BUILDTREES
13000 CALL BUILDHOUSE
13010 CALL BUILDYARD
13020 CALL BUILDWHEAT
13030 CALL BUILDBIKE
13040 CALL BUILDDRONES
13050 M3D THING 0,0,0,0,299,30,56:M3D THING 1,0,0,0,299,35,40
13060 M3D PLAYER 14,56,46,24
13070 M3D FAR 160000:M3D FOG 156000,FN CC(8,14),100000
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
13220 IF ENV$("G1987_FRAMELOG")="" THEN CALL TITLE
13230 IF IN_QUIT=0 THEN CALL NEWGAME
13240 IF IN_QUIT=0 THEN CALL PLAYLOOP
13250 CALL W3K_DONE
13260 CALL TX_BYE("1987")
13270 END
13280 LABEL FAILED
13290 IF NOT HL THEN CALL W3K_DONE
13300 PRINT "Error: ";ERR$;" in line ";ERL
13510 ' ------------------------------------------------------------------------------------------------ John's head and torso
13520 ' a frustum (a box that is smaller at one end): the bottom z = Z1 with the half sizes F1X, F1Y round the centre (FC1X, FC1Y), the top z = Z2 with F2X, F2Y round (FC2X, FC2Y)
13530 PROC FRUST(O,Z1,Z2,TX)
13540   LOCAL B
13550   B=M3D("VERTS",O)
13560   M3D VERTEX O,FC1X-F1X,FC1Y-F1Y,Z1:M3D VERTEX O,FC1X+F1X,FC1Y-F1Y,Z1:M3D VERTEX O,FC1X+F1X,FC1Y+F1Y,Z1:M3D VERTEX O,FC1X-F1X,FC1Y+F1Y,Z1
13570   M3D VERTEX O,FC2X-F2X,FC2Y-F2Y,Z2:M3D VERTEX O,FC2X+F2X,FC2Y-F2Y,Z2:M3D VERTEX O,FC2X+F2X,FC2Y+F2Y,Z2:M3D VERTEX O,FC2X-F2X,FC2Y+F2Y,Z2
13580   M3D QUAD O,B,B+1,B+2,B+3,TX:M3D QUAD O,B+4,B+5,B+6,B+7,TX
13590   M3D QUAD O,B,B+1,B+5,B+4,TX:M3D QUAD O,B+1,B+2,B+6,B+5,TX
13600   M3D QUAD O,B+2,B+3,B+7,B+6,TX:M3D QUAD O,B+3,B,B+4,B+7,TX
13610 END PROC
13620 ' the radius of the head at the fraction T (0 chin .. 1 crown) of its height, from the profile in LRZ / LRR (set by HEAD)
13630 FUNC HPRF(T)
13640   LOCAL J
13650   IF T<=LRZ(0) THEN RETURN LRR(0)
13660   FOR J=1 TO 6
13670     IF T<=LRZ(J) THEN RETURN LRR(J-1)+(LRR(J)-LRR(J-1))*(T-LRZ(J-1))/(LRZ(J)-LRZ(J-1))
13680   NEXT J
13690   RETURN LRR(6)
13700 END FUNC
13710 ' the head: an egg standing on its narrow end (broad cranium, narrow chin; 10.5 high, 8.6 wide) with the chin at CZ, centred on (CX, CY), the face towards +x, turned by the yaw
13720 ' HQY and the pitch HQP (the head's yaw and pitch rounded to whole degrees; the pivot is the chin / neck): every vertex goes through HV. A cap of jagged gray hair that starts at the forehead in front, over the ears at the sides
13730 ' and at the nape at the back, ears, a neck (which does not turn), glasses with thin frames and temples, eyes (white with a dark pupil), eyebrows, a nose and a mouth
13740 PROC HV(O,X,Y,Z)
13750   LOCAL X1,Z1
13760   X1=X*HPC-Z*HPS:Z1=X*HPS+Z*HPC
13770   M3D VERTEX O,HCX+X1*HYC-Y*HYS,HCY+X1*HYS+Y*HYC,HCZ+Z1
13780 END PROC
13790 ' a box in the head's coordinates (it turns with the head, so it is built from six quads)
13800 PROC HBX(O,X1,Y1,Z1,X2,Y2,Z2,TX)
13810   LOCAL B
13820   B=M3D("VERTS",O)
13830   CALL HV(O,X1,Y1,Z1):CALL HV(O,X2,Y1,Z1):CALL HV(O,X2,Y2,Z1):CALL HV(O,X1,Y2,Z1)
13840   CALL HV(O,X1,Y1,Z2):CALL HV(O,X2,Y1,Z2):CALL HV(O,X2,Y2,Z2):CALL HV(O,X1,Y2,Z2)
13850   M3D QUAD O,B,B+1,B+2,B+3,TX:M3D QUAD O,B+4,B+5,B+6,B+7,TX
13860   M3D QUAD O,B,B+1,B+5,B+4,TX:M3D QUAD O,B+1,B+2,B+6,B+5,TX
13870   M3D QUAD O,B+2,B+3,B+7,B+6,TX:M3D QUAD O,B+3,B,B+4,B+7,TX
13880 END PROC
13890 PROC HEAD(O,CX,CY,CZ)
13900   LOCAL J,K,B,A,R,T,F,H,NS,SK,SK2,JG,SG,GL
13910   H=10.5:NS=12:SK=FN CC(14,11):SK2=FN CC(14,9):GL=FN CC(0,3)
13920   HCX=CX:HCY=CY:HCZ=CZ
13930   HYS=SIN(HQY*PI/180):HYC=COS(HQY*PI/180):HPS=SIN(HQP*PI/180):HPC=COS(HQP*PI/180)
13940   LRZ(0)=0:LRZ(1)=0.12:LRZ(2)=0.3:LRZ(3)=0.55:LRZ(4)=0.78:LRZ(5)=0.93:LRZ(6)=1
13950   LRR(0)=0.5:LRR(1)=2.6:LRR(2)=3.7:LRR(3)=4.3:LRR(4)=4.2:LRR(5)=3.1:LRR(6)=0.7
13960   B=M3D("VERTS",O)
13970   FOR J=0 TO 6
13980     FOR K=0 TO NS-1
13990       A=K*2*PI/NS
14000       CALL HV(O,LRR(J)*1.05*COS(A),LRR(J)*0.95*SIN(A),LRZ(J)*H)
14010     NEXT K
14020   NEXT J
14030   FOR J=0 TO 5
14040     FOR K=0 TO NS-1
14050       M3D QUAD O,B+J*NS+K,B+J*NS+(K+1) MOD NS,B+(J+1)*NS+(K+1) MOD NS,B+(J+1)*NS+K,SK
14060     NEXT K
14070   NEXT J
14080   ' the hair: five rings, the lowest one at a height that depends on the side and is ragged, the others pushed out by 0.55 and ragged in radius
14090   B=M3D("VERTS",O)
14100   FOR J=0 TO 4
14110     FOR K=0 TO NS-1
14120       A=K*2*PI/NS:F=(1-COS(A))/2:JG=((K*5) MOD 3)-1
14130       IF J=0 THEN T=0.70-0.40*F+0.06*JG ELSE T=0.70+0.085*J
14140       IF J=4 THEN T=1.02
14150       R=FN HPRF(MIN(T,1))+0.55
14160       IF J>=2 THEN R=R*(1+0.06*JG)
14170       IF J=4 THEN R=0.6
14180       CALL HV(O,R*1.05*COS(A),R*0.95*SIN(A),T*H)
14190     NEXT K
14200   NEXT J
14210   FOR J=0 TO 3
14220     FOR K=0 TO NS-1
14230       M3D QUAD O,B+J*NS+K,B+J*NS+(K+1) MOD NS,B+(J+1)*NS+(K+1) MOD NS,B+(J+1)*NS+K,CC4+(K MOD 2)*2
14240     NEXT K
14250   NEXT J
14260   ' the neck (it stays upright and faces the front) and the ears
14270   CALL BX(O,CX-1.8,CY-1.8,CZ-2.2,CX+1.8,CY+1.8,CZ+1.4,SK2)
14280   CALL HBX(O,-1,3.9,4.8,1.4,4.8,7.2,SK2):CALL HBX(O,-1,-4.8,4.8,1.4,-3.9,7.2,SK2)
14290   ' the face: eyes, eyebrows, nose, mouth
14300   CALL HBX(O,3.5,1.2,6.2,4.25,2.8,7.4,FN CC(0,15)):CALL HBX(O,3.5,-2.8,6.2,4.25,-1.2,7.4,FN CC(0,15))
14310   CALL HBX(O,4.0,1.7,6.4,4.35,2.5,7.2,FN CC(9,2)):CALL HBX(O,4.0,-2.5,6.4,4.35,-1.7,7.2,FN CC(9,2))
14320   CALL HBX(O,3.6,1,7.9,4.2,3.1,8.5,CC5):CALL HBX(O,3.6,-3.1,7.9,4.2,-1,8.5,CC5)
14330   CALL HBX(O,3.7,-0.8,3.5,5.3,0.8,5.8,SK2)
14340   CALL HBX(O,3.2,-1.7,2.2,3.85,1.7,2.9,FN CC(1,5))
14350   ' glasses: two thin rectangular rims, a bridge over the nose and the temples back to the ears
14360   FOR SG=-1 TO 1 STEP 2
14370     CALL HBX(O,4.3,SG*0.7-0.15,5.7,4.6,SG*3.4+0.15,5.95,GL):CALL HBX(O,4.3,SG*0.7-0.15,7.75,4.6,SG*3.4+0.15,8,GL)
14380     CALL HBX(O,4.3,SG*0.7-0.15,5.7,4.6,SG*0.7+0.15,8,GL):CALL HBX(O,4.3,SG*3.4-0.15,5.7,4.6,SG*3.4+0.15,8,GL)
14390     CALL HBX(O,0.8,SG*3.3-0.15,7.6,4.6,SG*3.3+0.15,7.9,GL)
14400   NEXT SG
14410   CALL HBX(O,4.3,-0.75,7.2,4.6,0.75,7.5,GL)
14420 END PROC
14430 ' the collar: a ring of eight flaps that stands up a little and folds out round the neck, open at the front (a V)
14440 PROC COLLAR(O,CX,CY,CZ)
14450   LOCAL K,B,A,R1,R2,C
14460   C=FN CC(5,12)
14470   B=M3D("VERTS",O)
14480   FOR K=0 TO 11
14490     A=(K+0.5)*2*PI/12
14500     M3D VERTEX O,CX+2.4*COS(A),CY+2.6*SIN(A),CZ
14510     M3D VERTEX O,CX+3.3*COS(A),CY+3.6*SIN(A),CZ+1.6
14520   NEXT K
14530   FOR K=0 TO 10
14540     M3D QUAD O,B+K*2,B+K*2+1,B+(K+1)*2+1,B+(K+1)*2,C
14550   NEXT K
14560 END PROC
14570 ' a hand at the wrist (WX, WY, WZ) hanging at the side, the palm to the thigh: a palm, four fingers (the middle one longest) curled a little forwards, a thumb; SG: 1 left, -1 right
14580 PROC HAND(O,WX,WY,WZ,SG)
14590   LOCAL I,L,FO
14600   TBT=FN CC(14,11)
14610   CALL BX(O,WX-1.3,WY-0.8,WZ-2.6,WX+1.3,WY+0.8,WZ+0.1,TBT)
14620   FOR I=0 TO 3
14630     FO=0.95-I*0.63:L=3.2:IF I=1 THEN L=3.6
14640     IF I=3 THEN L=2.6
14650     CALL TUBE(O,WX+FO,WY,WZ-2.6,WX+FO+0.5,WY,WZ-2.6-L,0.3)
14660   NEXT I
14670   CALL TUBE(O,WX+1.3,WY-SG*0.3,WZ-0.4,WX+2.4,WY-SG*0.3,WZ-3,0.38)
14680 END PROC
14690 ' a fist round the handlebar's grip at (GXX, GYY) (the grip's centre; the fingers wrap round its front)
14700 PROC FIST(O,GXX,GYY)
14710   LOCAL I
14720   CALL BX(O,GXX-1.6,GYY-1.6,31.2,GXX+1.4,GYY+1.6,33.4,FN CC(14,11))
14730   FOR I=0 TO 3
14740     CALL BX(O,GXX+1.4,GYY-1.5+I*0.8,31.2,GXX+2.5,GYY-0.7+I*0.8,33.3,FN CC(14,9))
14750   NEXT I
14760   CALL BX(O,GXX-1.2,GYY-0.5,33.4,GXX+1.2,GYY+0.5,34.3,FN CC(14,11))
14770 END PROC
14780 ' a shoe: the heel at XH, the middle of its width at YC, the underside of the sole at ZB; 9.5 long, a dark sole, a gray upper with a high ankle, a tongue and three white laces
14790 PROC SHOE(O,XH,YC,ZB)
14800   LOCAL I
14810   CALL BX(O,XH-0.2,YC-3.2,ZB,XH+9.7,YC+3.2,ZB+1,FN CC(0,4))
14820   CALL BX(O,XH,YC-2.9,ZB+1,XH+4.3,YC+2.9,ZB+4.8,FN CC(0,13))
14830   CALL BX(O,XH,YC-2.95,ZB+4.4,XH+4.3,YC+2.95,ZB+4.85,FN CC(0,7))
14840   CALL BX(O,XH+4.3,YC-2.8,ZB+1,XH+6.7,YC+2.8,ZB+3.9,FN CC(0,13))
14850   CALL BX(O,XH+6.7,YC-2.6,ZB+1,XH+9.5,YC+2.6,ZB+3,FN CC(0,13))
14860   CALL BX(O,XH+4.3,YC-1,ZB+3.9,XH+6.5,YC+1,ZB+4.2,FN CC(0,9))
14870   FOR I=0 TO 2
14880     CALL BX(O,XH+4.5+I*0.7,YC-1.8,ZB+3.9,XH+4.9+I*0.7,YC+1.8,ZB+4.3,FN CC(0,15))
14890   NEXT I
14900 END PROC
14910 ' the head looks at things: at the nearest drone (within 1400 units and not behind John) with the pitch up to it, else at the bicycle when it is near, else it leads into a turn; the yaw
14920 ' (HLY, degrees to the left, at most 80) and the pitch (HLP, at most 40 up, 15 down) follow the target smoothly
14930 PROC HEADLOOK(DT)
14940   LOCAL I,D,DX,DY,DZ,AG,TY,TP,BD,BI
14950   TY=KT*22:TP=0:BD=1000000
14960   FOR I=0 TO NDR-1
14970     DX=DRX(I)-JX:DY=DRY(I)-JY:D=SQR(DX*DX+DY*DY)
14980     AG=FN ADIFF(ATAN2(DY,DX)*180/PI,JH)
14990     IF D<1400 AND ABS(AG)<100 AND D<BD THEN
15000       BD=D:TY=MAX(-80,MIN(80,AG)):DZ=DRZ(I)-(JZ+54):TP=MAX(0,MIN(40,ATAN2(DZ,D)*180/PI))
15010     END IF
15020   NEXT I
15030   IF BD>=1000000 AND MODE=0 THEN
15040     DX=BX-JX:DY=BY-JY:D=SQR(DX*DX+DY*DY)
15050     AG=FN ADIFF(ATAN2(DY,DX)*180/PI,JH)
15060     IF D<260 AND ABS(AG)<110 THEN TY=MAX(-80,MIN(80,AG)):TP=-15
15070   END IF
15080   HLY=HLY+(TY-HLY)*MIN(1,5*DT):HLP=HLP+(TP-HLP)*MIN(1,5*DT)
15090 END PROC
15100 ' the head is its own object (521, built in its own coordinates with the chin at the origin) so that it is built again only when the rounded yaw / pitch change (the head
15110 ' moves with the body by its position and heading alone); HOX, HOY, HOZ: where the chin is in the body's coordinates (set by BUILDJOHN), OX, OY, OZ, OHD: the body's place
15120 PROC PLACEHEAD(OX,OY,OZ,OHD)
15130   LOCAL K,C,S
15140   HQY=INT(HLY+0.5):HQP=INT(HLP+0.5):K=HQY*1000+HQP
15150   IF K<>HKEY THEN
15160     HKEY=K:CNH=CNH+1
15170     M3D OBJECT 521,28,235:CALL HEAD(521,0,0,0)
15180   END IF
15190   C=COS(OHD*PI/180):S=SIN(OHD*PI/180)
15200   M3D OBJPOS 521,OX+C*HOX-S*HOY,OY+S*HOX+C*HOY,OZ+HOZ,OHD
15210 END PROC
15300 ' G1987_FRAMELOG=file: skips the title, plays 10000 frames (G1987_FRAMES=n changes the number) and writes how many ticks (1/60 s) the frames took (raw, as TI sees them) and the smallest, largest and mean frame time that the
15310 ' game used (the smoothed DT0) and the frames where the camera or John's height jumped (by more than 2 units / 1.5 degrees from one frame to the next; the first 12 with what changed), then quits: a way to see whether the frames come evenly and the picture quietly
15320 PROC FRAMELOG
15330   LOCAL D
15340   IF NFR<=40 THEN EXIT PROC
15350   D=0
15360   IF FLN=0 THEN FLN=1:FLMN=9:FLMX=0:FLS=0:FLC=0:FLT0=0:FLT1=0:FLT2=0:FLT3=0:FLK=TI:PD1=0:PD2=0:PD3=0:PD4=0:PD5=0:FLJ=0:FLQ=0:FL1=M3D("X"):FL2=M3D("Y"):FL3=M3D("Z"):FL4=M3D("YAW"):FL5=M3D("PITCH"):FL6=JZ
15362   ' jumps of the picture: a sudden change of the camera's movement from one frame to the next (the second difference of its position and angles above 1.5 units / 1.2 degrees),
15363   ' (John's own height steps are not counted: the camera follows them smoothly; the step is printed); turning or walking steadily does not count
15364   DX1=M3D("X")-FL1:DY1=M3D("Y")-FL2:DZ1=M3D("Z")-FL3:DA1=FN ADIFF(M3D("YAW"),FL4):DP1=M3D("PITCH")-FL5:DJ1=JZ-FL6
15365   IF FLC>1 AND (ABS(DX1-PD1)>1.5 OR ABS(DY1-PD2)>1.5 OR ABS(DZ1-PD3)>1.5 OR ABS(DA1-PD4)>1.2 OR ABS(DP1-PD5)>1.2) THEN
15366     FLJ=FLJ+1:IF FLJ<=80 THEN FLJ$(FLJ)="frame "+STR$(FLC)+": cam moved "+STR$(INT(DX1))+" "+STR$(INT(DY1))+" "+STR$(INT(DZ1))+" (before "+STR$(INT(PD1))+" "+STR$(INT(PD2))+" "+STR$(INT(PD3))+") yaw "+STR$(INT(DA1))+" (before "+STR$(INT(PD4))+") pitch "+STR$(INT(DP1))+" (before "+STR$(INT(PD5))+") John z "+STR$(INT(DJ1*10)/10)+" distance "+STR$(INT(DSV))+" turn input "+STR$(KT)+" forward "+STR$(KF)+" mode "+STR$(MODE)
15368   END IF
15369   PD1=DX1:PD2=DY1:PD3=DZ1:PD4=DA1:PD5=DP1:FL1=M3D("X"):FL2=M3D("Y"):FL3=M3D("Z"):FL4=M3D("YAW"):FL5=M3D("PITCH"):FL6=JZ
15370   D=TI-FLK:FLK=TI
15380   IF D=0 THEN FLT0=FLT0+1
15390   IF D=1 THEN FLT1=FLT1+1
15400   IF D=2 THEN FLT2=FLT2+1
15410   IF D>2 THEN FLT3=FLT3+1
15420   IF DT0<FLMN THEN FLMN=DT0
15430   IF DT0>FLMX THEN FLMX=DT0
15440   FLS=FLS+DT0:FLC=FLC+1
15450   IF FLC>=FLN2 THEN
15460     OPEN FL$ FOR OUTPUT AS #7
15470     PRINT #7,"ticks per frame: 0:";FLT0;" 1:";FLT1;" 2:";FLT2;" 3+:";FLT3
15480     PRINT #7,"frame time used: min ";FLMN;" max ";FLMX;" mean ";FLS/FLC
15481     PRINT #7,"jumps of the picture (a sudden change of the camera's movement): ";FLJ
15482     FOR FLQ=1 TO MIN(FLJ,80)
15483       PRINT #7,FLJ$(FLQ)
15484     NEXT FLQ
15490     CLOSE #7
15500     IN_QUIT=1
15510   END IF
15520 END PROC
15600 ' ------------------------------------------------------------------------------------------------ the camera: an agent
15610 ' The camera is a creature of its own (the owner's idea, 2026-11-15): it has a position, a velocity and an acceleration limit (a point mass chasing the place it wants to be at with a
15620 ' spring and a damper, plus the velocity of that place so that it does not lag), and a view direction with an angular velocity (the same kind of controller), and a behavior:
15630 ' CMB 0 follow: it goes to a place behind John and above him and looks at his head, the way it has always done (the azimuth follows his heading with a delay, CYW); 1 directed: the player
15640 ' uses the camera controls (orbit Z / X, the right stick, the right mouse button; look up / down R / F; raise / lower PGUP / PGDN), which are only instructions (CO the azimuth, CP the elevation,
15650 ' CLP the look pitch, as wished offsets): the camera carries them out with its own acceleration, a circling course round John; when they are let go it waits a second and then
15660 ' goes back to its own business, CO and the others die away; 2 circling (on its own, when the player has done nothing for a while): it circles round John, a little higher, watching him;
15670 ' 3 watching a point of interest (the bicycle when John is on foot near it, a drone, the house, the scarecrow, the gate): it looks at it for a few seconds. Any input of the player (also
15680 ' walking or riding) ends 2 and 3 at once. Whatever it does it keeps clear of walls: three rays from John's head to the wished place (straight and 9 degrees to each side) shorten the
15690 ' distance, and a ray from John's head to the camera itself pulls it in front of anything it would be behind. REC and the red glyphs (CAMHUD) show that it acts on its own.
15700 PROC CAMAGENT(DT)
15710   LOCAL TX,TY,TZ,TG,K,D,E,C,S,DS,R,PX,PY,PZ,EX,EY,EZ,AX,AY,AZ,SP,LX,LY,LZ,H,FZ,PM,YW,PT,ER,DX,DY,DZ,D2,HH,KP,KD
15715   IF DT<0.002 THEN EXIT PROC
15720   IF CZI=0 OR ABS(JZ-CZS)>40 THEN CZS=JZ:CZI=-1
15730   CZS=CZS+(JZ-CZS)*MIN(1,9*DT)
15740   ZT=36:IF MODE=1 THEN ZT=40
15750   IF ZOI=0 THEN ZOS=ZT:ZOI=-1
15760   ZOS=ZOS+(ZT-ZOS)*MIN(1,5*DT)
15770   TX=JX:TY=JY:TZ=CZS+ZOS
15780   IF MODE=1 THEN TG=BH ELSE TG=JH
15790   K=3.4:IF MODE=1 THEN K=2.4
15800   CYW=CYW+FN ADIFF(TG,CYW)*MIN(1,K*DT)
15810   ' ---- what the player does, and what the camera does about it
15820   PM=0:IF KFA<>0 OR KFD<>0 OR KT<>0 OR KLT>0 OR KSH<>0 OR KB<>0 OR KU<>0 OR MODE>=2 THEN PM=-1
15830   IF CMIN<>0 THEN
15840     CMB=1:CMHOLD=1:CMIDLE=0
15850   ELSE
15860     IF CMB=1 THEN
15870       CMHOLD=CMHOLD-DT
15880       IF CMHOLD<=0 THEN CMB=0:CMT=0
15890     END IF
15900     IF PM THEN
15910       CMIDLE=0:IF CMB>=2 THEN CMB=0:CMT=0
15920     ELSE
15930       CMIDLE=CMIDLE+DT
15940     END IF
15950   END IF
15960   CMT=CMT+DT
15970   IF CMB=0 AND CMIDLE>CMNEXT THEN CALL CAMSTART
15980   IF CMB=0 THEN
15990     ' its own business: the wished offsets die away (the elevation only while John moves, as before)
16000     IF ABS(CO)>180 THEN CO=CO-INT((CO+180)/360)*360
16010     CO=CO*(1-MIN(1,1.1*DT)):CMAZ=CMAZ*(1-MIN(1,1.1*DT)):CMEL=CMEL*(1-MIN(1,1.1*DT))
16020     IF PM THEN CP=CP*(1-MIN(1,0.6*DT))
16030     CLP=CLP*(1-MIN(1,0.7*DT))
16040   END IF
16050   IF CMB=2 THEN
16060     CMAZ=CMAZ+CMAZR*DT:CMEL=CMEL+(9-CMEL)*MIN(1,1.5*DT)
16070     IF CMT>CMDUR THEN CMB=0:CMT=0:CMNEXT=CMIDLE+6+RND*6
16080   END IF
16090   IF CMB=3 THEN
16100     CMAZ=CMAZ*(1-MIN(1,0.8*DT)):CMEL=CMEL+(4-CMEL)*MIN(1,1.5*DT)
16110     IF CMPK=1 THEN CMPX=DRX(CMPI):CMPY=DRY(CMPI):CMPZ=DRZ(CMPI)
16120     IF CMT>CMDUR THEN CMB=0:CMT=0:CMNEXT=CMIDLE+6+RND*6
16130   END IF
16140   CP=MAX(-8,MIN(45,CP)):CLP=MAX(-5,MIN(45,CLP))
16150   ' ---- the place it wants to be at: behind John (as seen from where it looks), above him, as far as the walls allow
16160   YW=CYW+CO+CMAZ
16170   D=125:IF MODE=1 THEN D=155+MAX(0,BV)*0.18
16180   IF CZM>0 THEN D=CZM
16190   E=12+CP+CMEL
16200   C=COS(YW*PI/180):S=SIN(YW*PI/180)
16210   DS=D
16220   FOR K=-1 TO 1
16230     M3D CAMERA TX,TY,TZ-46,YW+180+K*9,E
16240     R=M3D("SHOOT",D+16)
16250     IF M3D("SHOTKIND")=1 THEN DS=MIN(DS,MAX(20,M3D("SHOTDIST")-12))
16260   NEXT K
16270   PX=TX-C*DS*COS(E*PI/180):PY=TY-S*DS*COS(E*PI/180):PZ=TZ+DS*SIN(E*PI/180)
16280   ' ---- where it wants to look: at John's head, or at a point of interest
16290   LX=TX:LY=TY:LZ=TZ
16300   IF CMB=3 THEN LX=CMPX:LY=CMPY:LZ=CMPZ
16310   ' ---- first time, or a teleport (a test command, a long fall): it is simply there
16320   EX=PX-CMX:EY=PY-CMY:EZ=PZ-CMZ
16330   IF CMINIT=0 OR EX*EX+EY*EY+EZ*EZ>490000 THEN
16340     CMX=PX:CMY=PY:CMZ=PZ:CMVX=0:CMVY=0:CMVZ=0:CMPX0=PX:CMPY0=PY:CMPZ0=PZ:CMTVX=0:CMTVY=0:CMTVZ=0
16350     DX=LX-CMX:DY=LY-CMY:DZ=LZ-CMZ:HH=SQR(DX*DX+DY*DY)
16360     CMYW=ATAN2(DY,DX)*180/PI:CMPT=ATAN2(DZ,HH)*180/PI+CLP:CMWY=0:CMWP=0:CMINIT=-1
16370     EX=0:EY=0:EZ=0
16380   END IF
16390   ' ---- movement: the velocity of the wished place is known (so a camera behind a bicycle does not lag), the rest is a spring and a damper; the acceleration and the speed are limited
16400   CMTVX=CMTVX+((PX-CMPX0)/DT-CMTVX)*MIN(1,12*DT):CMTVY=CMTVY+((PY-CMPY0)/DT-CMTVY)*MIN(1,12*DT):CMTVZ=CMTVZ+((PZ-CMPZ0)/DT-CMTVZ)*MIN(1,12*DT)
16410   CMPX0=PX:CMPY0=PY:CMPZ0=PZ
16420   AX=26*EX+10.2*(CMTVX-CMVX):AY=26*EY+10.2*(CMTVY-CMVY):AZ=26*EZ+10.2*(CMTVZ-CMVZ)
16430   SP=SQR(AX*AX+AY*AY+AZ*AZ):IF SP>1800 THEN AX=AX*1800/SP:AY=AY*1800/SP:AZ=AZ*1800/SP
16440   CMVX=CMVX+AX*DT:CMVY=CMVY+AY*DT:CMVZ=CMVZ+AZ*DT
16450   SP=SQR(CMVX*CMVX+CMVY*CMVY+CMVZ*CMVZ):IF SP>900 THEN CMVX=CMVX*900/SP:CMVY=CMVY*900/SP:CMVZ=CMVZ*900/SP
16460   CMX=CMX+CMVX*DT:CMY=CMY+CMVY*DT:CMZ=CMZ+CMVZ*DT
16470   ' ---- it never gets behind anything: a ray from John's head to the camera
16480   DX=CMX-TX:DY=CMY-TY:DZ=CMZ-TZ:D2=SQR(DX*DX+DY*DY+DZ*DZ):HH=SQR(DX*DX+DY*DY)
16490   IF D2>30 THEN
16500     M3D CAMERA TX,TY,TZ-46,ATAN2(DY,DX)*180/PI,ATAN2(DZ,HH)*180/PI
16510     R=M3D("SHOOT",D2+4)
16520     IF M3D("SHOTKIND")=1 THEN
16530       IF M3D("SHOTDIST")<D2+2 THEN
16540         D=MAX(20,M3D("SHOTDIST")-12):CMX=TX+DX/D2*D:CMY=TY+DY/D2*D:CMZ=TZ+DZ/D2*D:CMVX=CMVX*0.3:CMVY=CMVY*0.3:CMVZ=CMVZ*0.3
16550       END IF
16560     END IF
16570   END IF
16580   FZ=MAX(M3D("FLOORAT",CMX,CMY,CMZ),FN HT(CMX,CMY))+10
16590   IF CMZ<FZ THEN
16600     CMZ=FZ:IF CMVZ<0 THEN CMVZ=0
16610   END IF
16620   ' ---- looking: the same kind of controller for the yaw and the pitch (degrees, degrees a second)
16630   DX=LX-CMX:DY=LY-CMY:DZ=LZ-CMZ:HH=SQR(DX*DX+DY*DY)
16640   YW=ATAN2(DY,DX)*180/PI:PT=ATAN2(DZ,HH)*180/PI+CLP
16645   ' (a lazier look, a glance, when it turns to a point of interest)
16646   KP=90:KD=19:IF CMB=3 THEN KP=28:KD=10.6
16650   ER=FN ADIFF(YW,CMYW):CMWY=CMWY+(KP*ER-KD*CMWY)*DT
16660   IF ABS(CMWY)>540 THEN CMWY=SGN(CMWY)*540
16670   CMYW=CMYW+CMWY*DT
16680   ER=PT-CMPT:CMWP=CMWP+(KP*ER-KD*CMWP)*DT
16690   IF ABS(CMWP)>360 THEN CMWP=SGN(CMWP)*360
16700   CMPT=CMPT+CMWP*DT
16710   M3D CAMERA CMX,CMY,CMZ-46,CMYW,CMPT
16720   DSV=SQR((CMX-TX)^2+(CMY-TY)^2+(CMZ-TZ)^2)
16730 END PROC
16740 ' the camera decides to do something on its own: watch a point of interest (the bicycle when John is on foot and near it, the nearest drone, the house, the scarecrow, the gate: the next
16750 ' one that is of use, round and round) or, after one, circle round John
16760 PROC CAMSTART
16770   LOCAL T,DX,DY,D,BD,I,OK
16780   IF CMLASTB=3 AND RND<0.6 THEN
16790     OK=0
16800   ELSE
16810     OK=0:T=0
16820     WHILE T<5 AND OK=0
16830       T=T+1:CMPC=(CMPC+1) MOD 5
16840       IF CMPC=0 AND MODE=0 THEN
16850         DX=BX-JX:DY=BY-JY
16860         IF DX*DX+DY*DY<810000 THEN OK=-1:CMPK=0:CMPX=BX:CMPY=BY:CMPZ=BZ+18
16870       END IF
16880       IF CMPC=1 THEN
16890         BD=1800:CMPI=-1
16900         FOR I=0 TO NDR-1
16910           DX=DRX(I)-JX:DY=DRY(I)-JY:D=SQR(DX*DX+DY*DY)
16920           IF D<BD THEN BD=D:CMPI=I
16930         NEXT I
16940         IF CMPI>=0 THEN OK=-1:CMPK=1:CMPX=DRX(CMPI):CMPY=DRY(CMPI):CMPZ=DRZ(CMPI)
16950       END IF
16960       IF CMPC=2 THEN
16970         IF JX*JX+JY*JY<1210000 THEN OK=-1:CMPK=0:CMPX=0:CMPY=0:CMPZ=90
16980       END IF
16990       IF CMPC=3 THEN
17000         DX=1100-JX:DY=650-JY
17010         IF DX*DX+DY*DY<1690000 THEN OK=-1:CMPK=0:CMPX=1100:CMPY=650:CMPZ=45
17020       END IF
17030       IF CMPC=4 THEN
17040         DX=-JX:DY=-200-JY
17050         IF DX*DX+DY*DY<1000000 THEN OK=-1:CMPK=0:CMPX=0:CMPY=-200:CMPZ=40
17060       END IF
17070     WEND
17080   END IF
17090   IF OK THEN
17100     CMB=3:CMDUR=4+RND*2.5:CMLASTB=3
17110   ELSE
17120     CMB=2:CMDUR=8+RND*4:CMAZR=14+RND*10:IF RND<0.5 THEN CMAZR=-CMAZR
17130     CMLASTB=2
17140   END IF
17150   CMT=0
17160 END PROC
17170 ' the red glyphs of an alien script: a glyph is three to five strokes between points of a 7 x 10 grid, each one a stroke after the other from where the last one ended or from a new point
17180 PROC NEWGLYPH(G)
17190   LOCAL N,S,X0,Y0
17200   N=3+INT(RND*3):GNS(G)=N:X0=INT(RND*7):Y0=INT(RND*10)
17210   FOR S=0 TO N-1
17220     GGX(G*20+S*4)=X0:GGX(G*20+S*4+1)=Y0
17230     IF RND<0.5 THEN X0=INT(RND*7) ELSE Y0=INT(RND*10)
17240     IF RND<0.35 THEN X0=INT(RND*7):Y0=INT(RND*10)
17250     GGX(G*20+S*4+2)=X0:GGX(G*20+S*4+3)=Y0
17260   NEXT S
17270 END PROC
17280 ' the camera shows that it acts on its own (it does not when the player directs it): a red dot that blinks with REC in the upper left corner, and in the lower right corner a row of red
17290 ' glyphs of an alien script, one of which is made new from time to time (more often when the camera is busy with something)
17300 PROC CAMHUD
17310   LOCAL G,S,X,Y,C1,C2
17320   IF CMB=1 THEN EXIT PROC
17330   IF CGI=0 THEN
17340     FOR G=0 TO 6
17350       CALL NEWGLYPH(G)
17360     NEXT G
17370     CGI=-1:CGT=1
17380   END IF
17390   CGT=CGT-DTF
17400   IF CGT<=0 THEN
17410     CALL NEWGLYPH(INT(RND*7)):CGT=0.25+RND*1.6
17420     IF CMB>=2 THEN CGT=0.08+RND*0.5
17430   END IF
17440   C1=FN CC(1,13):C2=FN CC(1,8)
17450   IF (TI MOD 72)<48 THEN
17460     GFX LINE 9,8,19,12,C1,BF:GFX LINE 10,6,18,14,C1,BF:GFX LINE 12,5,16,15,C1,BF
17470   END IF
17480   GFX TEXT 24,6,"REC",C1
17490   FOR G=0 TO 6
17500     X=546+G*12:Y=271
17510     FOR S=0 TO GNS(G)-1
17520       GFX LINE X+GGX(G*20+S*4)+1,Y+GGX(G*20+S*4+1)+1,X+GGX(G*20+S*4+2)+1,Y+GGX(G*20+S*4+3)+1,C2
17530       GFX LINE X+GGX(G*20+S*4),Y+GGX(G*20+S*4+1),X+GGX(G*20+S*4+2),Y+GGX(G*20+S*4+3),C1
17540     NEXT S
17550   NEXT G
17560 END PROC
17600 ' the rock of the mountains (picture 324) and the water of the lakes (325: the water plane is translucent, so it tints what lies under it)
17610 PROC MKROCK
17620   LOCAL I,X,Y
17630   GFX CLEAR FN CC(0,5)
17640   FOR I=1 TO 40
17650     X=FN RN(56):Y=FN RN(56)
17660     GFX LINE X,Y,X+4+FN RN(12),Y+4+FN RN(12),FN CC(0,3+FN RN(7)),BF
17670   NEXT I
17680   FOR I=1 TO 30
17690     X=FN RN(60):Y=FN RN(60)
17700     GFX LINE X,Y,X+FN RN(10)-5,Y+FN RN(10)-5,FN CC(0,2)
17710   NEXT I
17720   CALL W3K_GRAB(324,64,64)
17730 END PROC
17740 PROC MKWATER
17750   LOCAL I,X,Y
17760   GFX CLEAR FN CC(8,9)
17770   FOR I=1 TO 40
17780     X=FN RN(56):Y=FN RN(64)
17790     GFX LINE X,Y,X+4+FN RN(8),Y,FN CC(8,11+FN RN(4))
17800   NEXT I
17810   CALL W3K_GRAB(325,64,64)
17820 END PROC
17830 ' too high or too steep for a ruin, a road or a bale (MTLIM: 120 units high, SLLIM: 22 units of height difference over 20 units: about 48 degrees) or too deep (12 below the water level WZ): the old test of "BLOCKED", now only used to place things
17840 FUNC ROUGH(X,Y)
17850   LOCAL H,S
17860   H=FN HT(X,Y)
17870   IF H>MTLIM OR H<WZ-12 THEN RETURN -1
17880   S=ABS(FN HT(X+20,Y)-H)+ABS(FN HT(X,Y+20)-H)
17890   RETURN S>SLLIM
17900 END FUNC
18000 ' ------------------------------------------------------------------------------------------------ city ruins, roads, hay bales of the wild world (roadmap item 95)
18010 ' A city ruin site: big regions of 7200 units (8 chunks) each have one with a chance of 30 %, at a hashed place with a radius of 450 .. 900 units; the ground inside is cut into lots of 180 units,
18020 ' 70 % of them hold a ruin (up to four brick walls of 8 units thickness, each there with 78 %, broken off at 30 .. 160 units, taller towards the middle, and three heaps of rubble);
18030 ' the lots on water, on steep or high ground stay empty. The buildings of a chunk are the objects 14000 + 32 slot + n (up to 25 of them), freed first.
18040 PROC CHCITY(CX,CY,SL,SX,SY,SR)
18050   LOCAL GI,GJ,X,Y,Z,D,K,WH,O,Q,X1,Y1,X2,Y2,HY,RB,RC
18060   FOR GJ=0 TO 4
18070     FOR GI=0 TO 4
18080       X=CX*900+GI*180+90:Y=CY*900+GJ*180+90:D=SQR((X-SX)*(X-SX)+(Y-SY)*(Y-SY)):Q=CX*5+GI:HY=CY*5+GJ
18090       IF D<SR AND CRN<32 THEN
18100         IF HASH(Q,HY,80)<0.7 AND NOT FN ROUGH(X,Y) AND FN HT(X,Y)>WZ+3 THEN
18110           Z=MIN(MIN(FN HT(X-60,Y-60),FN HT(X+60,Y-60)),MIN(FN HT(X-60,Y+60),FN HT(X+60,Y+60)))-6
18120           O=14000+SL*32+CRN:CRN=CRN+1
18130           M3D OBJECT O,0,235
18140           FOR K=0 TO 3
18150             IF HASH(Q,HY,81+K)<0.78 THEN
18160               WH=(30+HASH(Q,HY,90+K)*130)*(1.3-D/SR)+12
18170               X1=X-60:Y1=Y-60:X2=X+60:Y2=Y+60
18180               IF K=0 THEN Y2=Y-52
18190               IF K=1 THEN X1=X+52
18200               IF K=2 THEN Y1=Y+52
18210               IF K=3 THEN X2=X-52
18220               M3D BOX O,X1,Y1,Z,X2,Y2,Z+WH,326
18230             END IF
18240           NEXT K
18250           FOR K=0 TO 2
18260             RB=X-45+HASH(Q,HY,100+K)*90:RC=Y-45+HASH(Q,HY,110+K)*90
18270             M3D BOX O,RB-7,RC-7,Z,RB+7,RC+7,Z+8+HASH(Q,HY,120+K)*14,326
18280           NEXT K
18290         END IF
18300       END IF
18310     NEXT GI
18320   NEXT GJ
18330 END PROC
18340 PROC CHRUINS(CX,CY,SL)
18350   LOCAL RX,RY,I,J,SX,SY,SR
18360   CRN=0:RX=INT(CX/8):RY=INT(CY/8)
18370   FOR J=-1 TO 1
18380     FOR I=-1 TO 1
18390       IF HASH(RX+I,RY+J,70)<0.3 THEN
18400         SX=(RX+I)*7200+HASH(RX+I,RY+J,71)*7200:SY=(RY+J)*7200+HASH(RX+I,RY+J,72)*7200:SR=450+HASH(RX+I,RY+J,73)*450
18410         IF ABS(SX-CX*900-450)<SR+640 AND ABS(SY-CY*900-450)<SR+640 THEN CALL CHCITY(CX,CY,SL,SX,SY,SR)
18420       END IF
18430     NEXT I
18440   NEXT J
18450 END PROC
18840 ' hay bales in the fields: up to 6 a chunk, instances of the bale 343 (a box), objects 3600 + 6 slot + n
18850 PROC CHBALES(CX,CY,SL)
18860   LOCAL I,N,X,Y
18870   M3D OBJFREE 11200+SL*6,11200+SL*6+5
18880   N=0
18890   FOR I=0 TO 11
18900     X=CX*900+HASH(CX,CY,700+I)*900:Y=CY*900+HASH(CX,CY,800+I)*900
18910     IF N<6 AND FN GKIND(X,Y)=2 AND HASH(CX,CY,900+I)<0.5 AND NOT FN NEARROAD(X,Y) AND NOT FN ROUGH(X,Y) AND FN HT(X,Y)>WZ+3 THEN
18920       M3D INSTANCE 11200+SL*6+N,343,X,Y,FN HT(X,Y)-1,HASH(CX,CY,950+I)*360,0.8+HASH(CX,CY,980+I)*0.5:N=N+1
18930     END IF
18940   NEXT I
18950 END PROC
18960 PROC MKBALE
18970   M3D OBJECT 343,0,235:M3D BOX 343,-22,-14,0,22,14,22,321:M3D OBJSHOW 343,0
18980 END PROC
19500 ' ------------------------------------------------------------------------------------------------ roads of the wild world: dirt tracks and tarred roads
19510 ' The region points (1800 units, see CHREGIONS) are joined to their east and south neighbours with a chance of 45 %: a path of 50 unit steps that wanders (noise) and meets the points
19520 ' exactly. 40 % of the roads are TARRED (a real road: 4 lanes of 3.1 m (100 units) = 400 and a shoulder of 20 on each side, 440 wide, picture 327 with the markings; 3 m is about 94 units),
19530 ' the rest dirt tracks (110 wide, picture 304, see-through edges). No road over water, steep or high ground. A step is a SEGMENT; CHSEGS lists the segments that start near the chunk
19540 ' (so that trees and bales keep away from them: NEARROAD), CHROADS builds the ones that start in it
19550 PROC CHSEGS(CX,CY)
19560   LOCAL NT,SP,RX,RY,I,J,E,L,NS,K,T,AX,AY,CX2,CY2,X0,Y0,TR,T0,DT,KA,KB,XB,WA,R1X,R1Y,R2X,R2Y,CL,TL,HB,A0,A1
19570   X0=CX*900:Y0=CY*900:NSG=0
19572   FOR NT=1 TO 0 STEP -1
19574     SP=1800:TR=0:XB=160
19576     IF NT THEN SP=36000:TR=-1:XB=380
19580     RX=INT(X0/SP):RY=INT(Y0/SP)
19582     FOR J=-1 TO 1
19590       FOR I=-1 TO 1
19600         FOR E=0 TO 1
19610           IF M3D("ROADLINK",NT,RX+I,RY+J,E) THEN
19620             R1X=M3D("ROADPT",NT,RX+I,RY+J,E,0,0):R1Y=M3D("ROADPT",NT,RX+I,RY+J,E,0,1)
19640             R2X=M3D("ROADPT",NT,RX+I,RY+J,E,1,0):R2Y=M3D("ROADPT",NT,RX+I,RY+J,E,1,1)
19645             WA=M3D("ROADINFO",NT,RX+I,RY+J,E,4)
19650             IF MAX(R1X,R2X)>X0-WA-520 AND MIN(R1X,R2X)<X0+WA+1420 AND MAX(R1Y,R2Y)>Y0-WA-520 AND MIN(R1Y,R2Y)<Y0+WA+1420 THEN
19660               L=M3D("ROADINFO",NT,RX+I,RY+J,E,1):NS=M3D("ROADINFO",NT,RX+I,RY+J,E,0):CL=SQR((R2X-R1X)^2+(R2Y-R1Y)^2)
19672               HB=M3D("ROADINFO",NT,RX+I,RY+J,E,2)+2*M3D("ROADINFO",NT,RX+I,RY+J,E,3):TL=-1
19674               T0=((X0+450-R1X)*(R2X-R1X)+(Y0+450-R1Y)*(R2Y-R1Y))/(CL*CL):DT=(WA+1500)/CL
19676               KA=MAX(0,INT((T0-DT)*NS)):KB=MIN(NS-1,INT((T0+DT)*NS)+1)
19680               FOR K=KA TO KB
19690                 T=K/NS:AX=M3D("ROADPT",NT,RX+I,RY+J,E,T,0):AY=M3D("ROADPT",NT,RX+I,RY+J,E,T,1)
19710                 IF AX>=X0-XB AND AX<X0+900+XB AND AY>=Y0-XB AND AY<Y0+900+XB AND NSG<299 THEN
19720                   T=(K+1)/NS:CX2=M3D("ROADPT",NT,RX+I,RY+J,E,T,0):CY2=M3D("ROADPT",NT,RX+I,RY+J,E,T,1)
19740                   IF M3D("QUIET",AX,AY)>=0.99 THEN
19750                     SGX(NSG)=AX:SGY(NSG)=AY:SGU(NSG)=CX2:SGV(NSG)=CY2:SGK(NSG)=TR:SGD(NSG)=0:SGE(NSG)=0:SGL(NSG)=L:SGH(NSG)=HB
19760                     SGI(NSG)=(AX>=X0 AND AX<X0+900 AND AY>=Y0 AND AY<Y0+900)
19762                     IF TR AND SGI(NSG) THEN SGD(NSG)=M3D("ROADARC",NT,RX+I,RY+J,E,K/NS):SGE(NSG)=M3D("ROADARC",NT,RX+I,RY+J,E,T)
19764                     NSG=NSG+1
19770                   END IF
19780                 END IF
19790               NEXT K
19800             END IF
19810           END IF
19820         NEXT E
19830       NEXT I
19840     NEXT J
19842   NEXT NT
19850 END PROC
19860 ' builds the reflector posts of the chunk's tarred roads (instances of the post 344, 8800 + 6 slot + n): a post every 50 m (1562.5 units, measured along the straight line between the two region points and put exactly on the road, not at the end of the step) on both edges (232 units from the middle: just outside the 440 wide tar); the post on the right-hand side of the
19870 ' direction of the road shows a white reflector to the traffic that comes from behind and a red one to the oncoming traffic, the one on the left the other way round: the right-hand side of
19880 ' each driver has the white ones, the left-hand side the red ones. (The road itself, tar and dirt, is a ribbon in the mesh of the chunk's ground job.)
19890 PROC CHROADS(CX,CY,SL)
19900   LOCAL I,P0,NP,CX2,CY2,DX,DY,D,YW,PX,PY,NK,F,MX,MY,PD,OK
19910   P0=8800+SL*6:NP=0
19920   M3D OBJFREE P0,P0+5
19930   FOR I=0 TO NSG-1
19940     IF SGI(I) AND SGK(I) THEN
19950       CX2=SGU(I):CY2=SGV(I):DX=CX2-SGX(I):DY=CY2-SGY(I):D=SQR(DX*DX+DY*DY):DX=DX/D:DY=DY/D
19960       NK=INT(SGE(I)/1562.5):PD=NK*1562.5
19962       IF INT(SGD(I)/1562.5)<>NK AND NP<5 THEN
19963         OK=-1:IF (SGH(I) AND 1) AND PD<520 THEN OK=0
19964         IF (SGH(I) AND 2) AND PD>SGL(I)-520 THEN OK=0
19965         IF OK THEN
19966           F=(PD-SGD(I))/(SGE(I)-SGD(I)):MX=SGX(I)+(CX2-SGX(I))*F:MY=SGY(I)+(CY2-SGY(I))*F
19970           YW=ATAN2(DY,DX)*180/PI
19980           PX=MX+DY*232:PY=MY-DX*232:M3D INSTANCE P0+NP,344,PX,PY,FN HT(PX,PY),YW,1:NP=NP+1
19990           PX=MX-DY*232:PY=MY+DX*232:M3D INSTANCE P0+NP,344,PX,PY,FN HT(PX,PY),YW+180,1:NP=NP+1
19996         END IF
19999       END IF
20010     END IF
20020   NEXT I
20030 END PROC
20220 ' the picture of a tarred road: 64 x 64 texels, across the road (440 units, about 6.9 units a texel) and along it (64 texels = 376 units = 12 m); dark tar with speckles, a continuous
20230 ' white edge line on each side (x 3 and 60), a dashed white middle line (x 31 / 32: 4.5 m dashes, 7.5 m gaps) and dashed white lane lines between the two lanes of a direction (x 17 and 46)
20240 PROC MKTAR
20250   LOCAL I
20260   GFX LINE 0,0,63,63,FN CC(0,3),BF
20270   FOR I=1 TO 220
20280     GFX PSET FN RN(64),FN RN(64),FN CC(0,2+FN RN(3))
20290   NEXT I
20300   GFX LINE 3,0,3,63,FN CC(0,15):GFX LINE 60,0,60,63,FN CC(0,15)
20310   GFX LINE 31,0,32,23,FN CC(0,15),BF
20320   GFX LINE 17,0,17,15,FN CC(0,15):GFX LINE 46,0,46,15,FN CC(0,15)
20330   CALL W3K_GRAB(327,64,64)
20335   GFX LINE 0,0,3,3,FN CC(0,4),BF:CALL W3K_GRAB(328,4,4)
20340 END PROC
20350 ' the reflector post (object 344, hidden, passable): a white post of 36 units (1.1 m) with a white reflector on its -x face and a red one on its +x face; the road's direction is its +x
20360 PROC MKPOST
20370   M3D OBJECT 344,4,255
20380   M3D BOX 344,-2.5,-2.5,0,2.5,2.5,36,FN CC(0,13)
20390   M3D BOX 344,-3.5,-2,21,-2.5,2,31,FN CC(0,15)
20400   M3D BOX 344,2.5,-2,21,3.5,2,31,FN CC(1,11)
20410   M3D OBJSHOW 344,0
20420 END PROC
21200 ' ------------------------------------------------------------------------------------------------ the rings of the far ground (M3D RING jobs of the engine's worker thread)
21210 ' Round the 17 x 17 chunks of CHUNKS (15300 units) the ground goes on in three rings of cells that grow with the distance, each a square grid with a hole where the finer one is:
21220 ' ring 2 cells of 900 (45 x 45 = 40500, the hole is the window: 17 x 17 cells), ring 3 cells of 2700 (36 x 36 = 97200, hole = ring 2), ring 4 cells of 8100 (40 x 40 = 324000 = 10 km,
21230 ' hole = ring 3; the view reaches 5 km). The cells of the chunks at 4 or more away are the cells of ring 2, so they meet without a step. Rings 2 and 3 MORPH: their vertices blend into the coarser
21240 ' ring's surface by their distance from the player (M3D MORPH, every frame; ring 2 from 9000 to 17550 units, ring 3 from 23000 to 40500), so no hill appears or goes with a change of ring.
21250 ' A ring lies on the grid of the next coarser ring (its corner a multiple of 2700 / 8100 / 8100), so it is made again only when the player has moved so far (or the finer ring did; ring 2 also
21260 ' when the window moved a chunk: its hole): objects 27002 .. 27004; the jobs run on the worker thread and are taken together by M3D POLL
21270 PROC RINGUPDATE(PX,PY)
21280   LOCAL PCX,PCY,O2X,O2Y,O3X,O3Y,O4X,O4Y,C2,C3,C4
21290   PCX=INT(PX/900):PCY=INT(PY/900)
21300   O2X=INT((PX-20250)/2700)*2700:O2Y=INT((PY-20250)/2700)*2700
21310   O3X=INT((PX-48600)/8100)*8100:O3Y=INT((PY-48600)/8100)*8100
21320   O4X=INT((PX-162000)/8100)*8100:O4Y=INT((PY-162000)/8100)*8100
21330   C2=(O2X<>RGX(2) OR O2Y<>RGY(2) OR PCX<>RGP(2) OR PCY<>RGQ(2))
21340   C3=(O3X<>RGX(3) OR O3Y<>RGY(3) OR O2X<>RGP(3) OR O2Y<>RGQ(3))
21350   C4=(O4X<>RGX(4) OR O4Y<>RGY(4) OR O3X<>RGP(4) OR O3Y<>RGQ(4))
21360   IF C2 THEN M3D RING 27002,O2X,O2Y,45,900,60,0,(PCX-8)*900,(PCY-8)*900,17,9000,17550:RGX(2)=O2X:RGY(2)=O2Y:RGP(2)=PCX:RGQ(2)=PCY
21370   IF C3 THEN M3D RING 27003,O3X,O3Y,36,2700,150,0,O2X,O2Y,15,23000,40500:RGX(3)=O3X:RGY(3)=O3Y:RGP(3)=O2X:RGQ(3)=O2Y
21380   IF C4 THEN M3D RING 27004,O4X,O4Y,40,8100,400,0,O3X,O3Y,12:RGX(4)=O4X:RGY(4)=O4Y:RGP(4)=O3X:RGQ(4)=O3Y
21390 END PROC
22000 ' test mode (play-back command "pop n step"): John walks n steps of "step" units east and after every step the height of the ground (the chunks' meshes) at 40 fixed places 700 .. 6400 units
22010 ' ahead is read again: how much the largest change from one step to the next is (a hill that pops up in front of the player shows as a big one); prints "poptest samples= max= over3="
22020 PROC POPTEST(N,SS)
22030   LOCAL I,K,D,X,Y,H,MX,CNT,BIG,SK,X0,Y0
22040   X0=INT(JX/300)*300+700:Y0=INT(JY/300)*300:MX=0:CNT=0:BIG=0
22050   FOR I=1 TO N
22060     JX=JX+SS
22070     CALL CHUNKS(JX,JY,1):CALL RINGUPDATE(JX,JY):M3D POLL:M3D MORPH JX,JY
22080     SK=0
22090     FOR K=0 TO 19
22100       FOR D=0 TO 1
22110         X=X0+K*300:Y=Y0+D*200:H=M3D("FLOORAT",X,Y,9000)
22120         IF I>5 AND PV(SK)>-100000 AND H>-100000 THEN
22130           IF ABS(H-PV(SK))>MX THEN MX=ABS(H-PV(SK))
22140           IF ABS(H-PV(SK))>3 THEN BIG=BIG+1
22150           CNT=CNT+1
22160         END IF
22170         PV(SK)=H:SK=SK+1
22180       NEXT D
22190     NEXT K
22200   NEXT I
22210   PRINT "poptest samples=";CNT;" max=";INT(MX*10)/10;" over3=";BIG
22220 END PROC
