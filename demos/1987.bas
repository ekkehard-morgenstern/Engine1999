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
110 ' right stick turns the camera, START pauses, BACK quits, SHARE takes a screenshot.
120 ' The bicycle behaves like the ones in GTA San Andreas: it has to be pedalled up to speed (slow at first, a sprint tires), it rolls on when you stop pedalling, hills speed
130 ' it up and slow it down, the turning radius grows with the speed (a bicycle cannot turn on the spot), you can brake and reverse, and a wall at speed is a hard stop.
140 ' Test mode (no keyboard needed): the environment variable G1987_PLAY names a file of commands, G1987_HEADLESS=1 runs it in the console host without any picture (see PLAYBACK).
150 LOAD LIBRARY "w3dkit"
160 LOAD LIBRARY "input"
170 LOAD LIBRARY "text"
180 LOAD LIBRARY "prefs3d"
190 LOAD LIBRARY "music3d":LOAD LIBRARY "banner3d":IF ENV$("G1987_HEADLESS")="" THEN CALL B3_M3D("1987")
200 LOAD LIBRARY "chunks"
210 BREAK OFF
220 ON ERROR GOTO FAILED
230 HL=0:IF ENV$("G1987_HEADLESS")<>"" THEN HL=-1
240 SXY=0:LDS=0:LDON=0:LDLAST=0:NOLOAD=0:WLD=0:IF ENV$("G1987_WORLD")<>"" OR ( ENV$("G1987_DYNAMIC")="" AND NOT HL ) THEN WLD=-1
250 DIM PV(60),SGL(300),SGH(300),SGX(300),SGY(300),SGU(300),SGV(300),SGK(300),SGD(300),SGE(300),SGI(300),RGX(6),RGY(6),RGP(6),RGQ(6),CKN(400),CHB(400),CKRX(9),CKRY(9),CKRT(9),CTX(24),CTY(24),GNS(8),GGX(160),FLJ$(81),TIH(33),RPX(300),RPY(300),RPF(300),CPX(40),CPY(40),SMX(120),SMY(120),PLX(40),PLY(40),TRX(520),TRY(520),LRR(8),LRZ(8),LRV(8),W$(8),GPX(2),GPZ(2),GPS(2),DWX(40),DWY(40),DRX(10),DRY(10),DRZ(10),DRH(10),DRT(10),DRA(10),DRM(10),DRTM(10),DRTX(10),DRTY(10),DRBM(10),DRV(10),DDD(10),DRP(10),DSX(10),DSY(10),DRI(10),DRW(10),DRL(10),DRSC(10)
260 DIM QX1(14),QY1(14),QX2(14),QY2(14)
270 DIM FPS(9),FPX(9),FPC(9),FPB(9)
280 GOTO MAIN
1000 ' ------------------------------------------------------------------------------------------------ small helpers
1100 FUNC RN(M)
1110   RETURN INT(RND(1)*M)
1120 END FUNC
1200 FUNC CC(H,S)
1210   RETURN 16+H*16+S
1220 END FUNC
1300 ' the difference of two angles in degrees, -180 .. 180
1400 FUNC ADIFF(A,B)
1410   LOCAL D
1420   D=A-B
1430   D=D-INT((D+180)/360)*360
1440   RETURN D
1450 END FUNC
1500 ' the height of the ground: gentle hills, flat round the house and in the wheat field; the engine's ground function (SETGROUND defines it: fractal noise in C, since 2026-11-20;
1510 ' before it was a sum of sines in BASIC)
1600 FUNC HT(X,Y)
1610   RETURN M3D("GROUND",X,Y)
1620 END FUNC
1630 ' the time of day as hh:mm (H in hours, 0 .. 24; day and night, section 60000)
1640 FUNC CLOCK$(H)
1650   LOCAL HH,MM
1660   HH=INT(H):MM=INT((H-HH)*60)
1670   RETURN MID$(STR$(100+HH),2)+":"+MID$(STR$(100+MM),2)
1680 END FUNC
1700 ' the floor under a point (the ground, the porch, the floor of the house)
1800 FUNC FLR(X,Y)
1810   RETURN M3D("FLOORAT",X,Y,FN HT(X,Y)+40)
1820 END FUNC
1900 ' the ground under a point: 0 grass, 1 sand road, 2 wood (the porch and the floor of the house), 3 wheat
1910 ' how fast John walks, 0 (the slow walk WSLOW or slower) .. 1 (the top speed WFAST)
2000 FUNC SPD1(V)
2010   RETURN MAX(0,MIN(1,(ABS(V)-WSLOW)/(WFAST-WSLOW)))
2020 END FUNC
2100 FUNC SURF(X,Y,Z)
2110   LOCAL T
2120   ' what the ground is made of: the picture of the floor under the feet (the engine's M3D("FLOORTEX"): the highest solid triangle below, so roads, planks, tiles and
2130   ' the soil of the lakes are told by their pictures, not by the height); 0 grass (301 .. 303, carpet 323, anything else), 1 sand (road 304, mud 320), 2 wood / hard
2140   ' (planks 305, tiles 306, stone 307, rock 324, brick 326), 3 wheat (308, 321, and the soil of the field)
2150   T=M3D("FLOORTEX",X,Y,Z+3)
2160   IF T=320 AND ABS(X-1100)<700 AND ABS(Y-650)<500 THEN RETURN 3
2170   IF T=308 OR T=321 THEN RETURN 3
2180   IF T=304 OR T=320 THEN RETURN 1
2190   IF T=305 OR T=306 OR T=307 OR T=324 OR T=326 OR T=327 THEN RETURN 2
2200   RETURN 0
2210 END FUNC
2300 ' which picture covers the ground cell around X,Y: 1 grass, 2 dry grass, 3 lush grass, 20 the soil of the wheat field
2400 FUNC CELLTEX(X,Y)
2410   LOCAL N
2420   IF ABS(X-1100)<700 AND ABS(Y-650)<500 THEN RETURN 320
2430   N=SIN(X*0.0023+1)*COS(Y*0.0031)+SIN((X-Y)*0.0017)
2440   IF N>0.55 THEN RETURN 303
2450   IF N<-0.5 THEN RETURN 302
2460   RETURN 301
2470 END FUNC
2500 PROC SND(N)
2510   IF NOT HL THEN SFX N
2520 END PROC
3000 ' ------------------------------------------------------------------------------------------------ pictures
3100 PROC MKGRASS(N,H1,H2,FL)
3110   LOCAL I,X,Y
3120   GFX CLEAR FN CC(H1,5)
3130   FOR I=1 TO 520
3140     X=FN RN(64):Y=FN RN(64)
3150     GFX LINE X,Y,X+FN RN(3)-1,Y-1-FN RN(3),FN CC(H1+FN RN(2)*(H2-H1),3+FN RN(8))
3160   NEXT I
3170   FOR I=1 TO FL
3180     GFX PSET FN RN(64),FN RN(64),FN CC(3+FN RN(2)*8,13+FN RN(3))
3190   NEXT I
3200   CALL W3K_GRAB(N,64,64)
3210 END PROC
3300 ' the sand road (pale sand, hue 13): the left and right edge are see-through (index 0), two wheel ruts, a ridge of grass between them
3400 PROC MKROAD
3410   LOCAL I,Y,L,R,X
3420   GFX CLEAR 0
3430   FOR Y=0 TO 63
3440     L=3+FN RN(4):R=60-FN RN(4)
3450     GFX LINE L,Y,R,Y,FN CC(13,11+FN RN(3))
3460     GFX LINE 15,Y,22,Y,FN CC(13,8+FN RN(3))
3470     GFX LINE 41,Y,48,Y,FN CC(13,8+FN RN(3))
3480   NEXT Y
3490   FOR I=1 TO 16
3500     X=27+FN RN(10):Y=FN RN(64)
3510     GFX LINE X,Y,X+1,Y-2-FN RN(3),FN CC(4,5+FN RN(4))
3520   NEXT I
3530   FOR I=1 TO 70
3540     GFX PSET 5+FN RN(54),FN RN(64),FN CC(13,12+FN RN(4))
3550   NEXT I
3560   FOR I=1 TO 40
3570     GFX PSET 5+FN RN(54),FN RN(64),FN CC(13,6+FN RN(3))
3580   NEXT I
3590   CALL W3K_GRAB(304,64,64)
3600 END PROC
3700 ' wheat seen from the side: stalks with heavy ears, the rest of the picture is see-through
3800 PROC MKWHEAT
3810   LOCAL I,X,H,D,Y
3820   GFX CLEAR 0
3830   FOR I=1 TO 70
3840     X=FN RN(64):H=24+FN RN(34):D=FN RN(5)-2
3850     GFX LINE X,63,X+D,63-H,FN CC(13,7+FN RN(4))
3860     GFX LINE X+1,63,X+1+D,63-H,FN CC(12,6+FN RN(3))
3870     Y=63-H
3880     GFX LINE X+D-1,Y,X+D+1,Y+9,FN CC(3,10+FN RN(4))
3890     GFX LINE X+D,Y-3,X+D,Y+8,FN CC(2,9+FN RN(4))
3900   NEXT I
3910   FOR I=1 TO 60
3920     GFX PSET FN RN(64),52+FN RN(12),FN CC(12,4+FN RN(4))
3930   NEXT I
3940   CALL W3K_GRAB(308,64,64)
3950 END PROC
4000 ' the top of the wheat, seen from above
4100 PROC MKWHEATTOP
4110   LOCAL I
4120   GFX CLEAR FN CC(3,8)
4130   FOR I=1 TO 500
4140     GFX LINE FN RN(64),FN RN(64),FN RN(64),FN RN(64),FN CC(3+FN RN(2)*10,8+FN RN(5))
4150   NEXT I
4160   FOR I=1 TO 400
4170     GFX PSET FN RN(64),FN RN(64),FN CC(12,6+FN RN(4))
4180   NEXT I
4190   CALL W3K_GRAB(321,64,64)
4200 END PROC
4300 ' the plowed soil under the wheat
4400 PROC MKSOIL
4410   LOCAL I,Y
4420   GFX CLEAR FN CC(12,4)
4430   FOR Y=0 TO 7
4440     GFX LINE 0,Y*8+2,63,Y*8+3,FN CC(12,2)
4450     GFX LINE 0,Y*8+5,63,Y*8+5,FN CC(12,6)
4460   NEXT Y
4470   FOR I=1 TO 160
4480     GFX PSET FN RN(64),FN RN(64),FN CC(13,4+FN RN(5))
4490   NEXT I
4500   CALL W3K_GRAB(320,64,64)
4510 END PROC
4600 ' leaves: dark at the bottom (row 63), light at the top (row 0)
4700 PROC MKLEAVES(N,H1,H2)
4710   LOCAL I,Y,X,S
4720   FOR Y=0 TO 63
4730     GFX LINE 0,Y,63,Y,FN CC(H1,2+(63-Y)\10)
4740   NEXT Y
4750   FOR I=1 TO 380
4760     X=FN RN(64):Y=FN RN(64):S=(63-Y)\10
4770     GFX LINE X,Y,X+1+FN RN(3),Y+FN RN(3)-1,FN CC(H1+FN RN(2)*(H2-H1),MAX(1,S-1+FN RN(5)))
4780   NEXT I
4790   CALL W3K_GRAB(N,64,64)
4800 END PROC
4900 PROC MKBARK
4910   LOCAL I,X
4920   GFX CLEAR FN CC(12,3)
4930   FOR I=1 TO 40
4940     X=FN RN(64)
4950     GFX LINE X,0,X+FN RN(3)-1,63,FN CC(12,1+FN RN(5))
4960   NEXT I
4970   FOR I=1 TO 90
4980     GFX PSET FN RN(64),FN RN(64),FN CC(12,5+FN RN(3))
4990   NEXT I
5000   CALL W3K_GRAB(312,64,64)
5010 END PROC
5100 ' the sky: 256 x 64; which end of the picture is up is found by looking
5200 PROC MKSKY
5210   LOCAL Y,I,X,CY,RX
5220   FOR Y=0 TO 63
5230     GFX LINE 0,Y,255,Y,FN CC(8,7+Y\9),BF
5240   NEXT Y
5250   FOR I=1 TO 14
5260     X=FN RN(256):CY=10+FN RN(40):RX=8+FN RN(18)
5270     GFX CIRCLE X,CY,RX,FN CC(0,13),-1,-1,0.3
5280     GFX CIRCLE X-256,CY,RX,FN CC(0,13),-1,-1,0.3
5290     GFX PAINT X,CY,FN CC(0,15),FN CC(0,13)
5300     GFX PAINT X-256,CY,FN CC(0,15),FN CC(0,13)
5310   NEXT I
5320   CALL W3K_GRAB(311,256,64)
5330 END PROC
5400 PROC MAKETEX
5410   RANDOMIZE 1987
5420   CALL MKGRASS(301,5,4,0)
5430   CALL MKGRASS(302,4,3,0)
5440   CALL MKGRASS(303,5,4,30)
5450   CALL MKROAD
5460   CALL W3K_PLANKS(305)
5470   CALL W3K_TILES(306,12,12)
5480   CALL W3K_STONE(307,0)
5490   CALL MKWHEAT
5500   CALL MKLEAVES(309,5,4)
5510   CALL MKLEAVES(310,5,6)
5520   CALL MKSKY
5530   CALL MKBARK
5540   CALL W3K_WINDOW(313):CALL MKWINDOWG(370)
5550   CALL MKSOIL
5560   CALL MKWHEATTOP
5570   CALL MKLEAVES(317,4,3)
5580   CALL W3K_CARPET(323,1)
5590   GFX CLEAR 0
5600   GFX PSET 0,0,0
5610   CALL W3K_GRAB(299,1,1)
5620   CALL MKROCK:CALL MKWATER:CALL W3K_BRICK(326,1):CALL MKTAR
5630   CALL MKPLASTER(361,13,12):CALL MKPLASTER(362,3,13):CALL MKPLASTER(363,8,12):CALL MKPLASTER(364,14,12):CALL MKPLASTER(365,5,12):CALL MKPLASTER(366,0,13)
5640   M3D SKY 311
5650 END PROC
5700 ' the plaster of the settlements' walls: the colour of the house with a speckle and a few streaks (picture 361 + slot)
5800 PROC MKPLASTER(N,H,S)
5810   LOCAL I,X,Y
5820   GFX CLEAR FN W3K_C(H,S)
5830   FOR I=1 TO 260:GFX PSET FN W3K_R(64),FN W3K_R(64),FN W3K_C(H,MIN(15,MAX(1,S-3+FN W3K_R(7)))):NEXT I
5840   FOR I=1 TO 8:X=FN W3K_R(56):Y=FN W3K_R(64):GFX LINE X,Y,X+3+FN W3K_R(6),Y+FN W3K_R(2),FN W3K_C(H,MAX(1,S-4)):NEXT I
5850   CALL W3K_GRAB(N,64,64)
5860 END PROC
6000 ' ------------------------------------------------------------------------------------------------ mesh helpers
6100 PROC MKWINDOWG(N)
6110   LOCAL I
6120   GFX CLEAR FN CC(9,3)
6130   GFX LINE 0,0,63,95,FN CC(12,8),B:GFX LINE 1,1,62,94,FN CC(12,6),B:GFX LINE 2,2,61,93,FN CC(12,9),B:GFX LINE 3,3,60,92,FN CC(12,5),B
6140   GFX LINE 29,3,34,92,FN CC(12,7),BF:GFX LINE 3,44,60,49,FN CC(12,7),BF
6150   FOR I=0 TO 3:GFX LINE 8+I*2,12+I*2,12+I*2,16+I*2,FN CC(7,14):NEXT I
6160   CALL W3K_GRAB(N,64,96)
6170 END PROC
6200 ' (MKWINDOWG: picture 370, 64 x 96, for the glass of the settlements' houses: dark blue glass, opaque because nothing is behind a pane, with the brown frame, the cross of mullions and a reflection of W3K_WINDOW; the frame is in the picture, not geometry)
6300 PROC BX(O,X1,Y1,Z1,X2,Y2,Z2,TX)
6310   IF XON THEN CALL BXX(O,X1,Y1,Z1,X2,Y2,Z2,TX) ELSE M3D BOX O,X1,Y1,Z1,X2,Y2,Z2,TX
6320 END PROC
6400 ' a lathe solid with 8 sides from the rings LRR (radius), LRZ (height), LRV (picture row) 0 .. NR-1, scaled by SC
6500 PROC LATHE(O,NR,TX,SC)
6510   LOCAL J,K,B,A,P,R
6520   B=M3D("VERTS",O)
6530   FOR J=0 TO NR-1
6540     R=LRR(J)*SC:IF R<0.6 THEN R=0.6
6550     FOR K=0 TO 8
6560       A=K*0.7853982
6570       M3D VERTEX O,R*COS(A),R*SIN(A),LRZ(J)*SC,K*8,LRV(J)
6580     NEXT K
6590   NEXT J
6600   FOR J=0 TO NR-2
6610     FOR K=0 TO 7
6620       P=B+J*9+K
6630       M3D QUAD O,P,P+1,P+10,P+9,TX
6640     NEXT K
6650   NEXT J
6660 END PROC
6700 ' a square rod between two points, half width H, the picture (or flat color) TBT
6800 PROC TUBE(O,X1,Y1,Z1,X2,Y2,Z2,H)
6810   LOCAL AX,AY,AZ,L,PX,PY,PZ,QX,QY,QZ,B,I,SA,SB,VX,VY,VZ
6820   AX=X2-X1:AY=Y2-Y1:AZ=Z2-Z1
6830   L=SQR(AX*AX+AY*AY+AZ*AZ):IF L<0.01 THEN EXIT PROC
6840   AX=AX/L:AY=AY/L:AZ=AZ/L
6850   PX=AY:PY=-AX:PZ=0
6860   L=SQR(PX*PX+PY*PY)
6870   IF L<0.001 THEN
6880     PX=1:PY=0
6890   ELSE
6900     PX=PX/L:PY=PY/L
6910   END IF
6920   QX=AY*PZ-AZ*PY:QY=AZ*PX-AX*PZ:QZ=AX*PY-AY*PX
6930   B=M3D("VERTS",O)
6940   FOR I=0 TO 7
6950     SA=1:IF (I AND 1) THEN SA=-1
6960     SB=1:IF (I AND 2) THEN SB=-1
6970     IF I<4 THEN
6980       VX=X1+H*SA*PX+H*SB*QX:VY=Y1+H*SA*PY+H*SB*QY:VZ=Z1+H*SA*PZ+H*SB*QZ
6990     ELSE
7000       VX=X2+H*SA*PX+H*SB*QX:VY=Y2+H*SA*PY+H*SB*QY:VZ=Z2+H*SA*PZ+H*SB*QZ
7010     END IF
7020     IF XON THEN CALL PXV(O,VX,VY,VZ) ELSE M3D VERTEX O,VX,VY,VZ
7030   NEXT I
7040   M3D QUAD O,B,B+1,B+5,B+4,TBT
7050   M3D QUAD O,B+1,B+3,B+7,B+5,TBT
7060   M3D QUAD O,B+3,B+2,B+6,B+7,TBT
7070   M3D QUAD O,B+2,B,B+4,B+6,TBT
7080   M3D QUAD O,B+4,B+5,B+7,B+6,TBT
7090   M3D QUAD O,B,B+2,B+3,B+1,TBT
7100 END PROC
7200 ' a vertical picture in the plane y = Y (a window pane) or x = X
7300 PROC PANEY(O,X1,X2,Y,Z1,Z2,TX)
7310   LOCAL B
7320   B=M3D("VERTS",O)
7330   M3D VERTEX O,X1,Y,Z1,0,95:M3D VERTEX O,X2,Y,Z1,63,95:M3D VERTEX O,X2,Y,Z2,63,0:M3D VERTEX O,X1,Y,Z2,0,0
7340   M3D QUAD O,B,B+1,B+2,B+3,TX
7350 END PROC
7400 PROC PANEX(O,X,Y1,Y2,Z1,Z2,TX)
7410   LOCAL B
7420   B=M3D("VERTS",O)
7430   M3D VERTEX O,X,Y1,Z1,0,95:M3D VERTEX O,X,Y2,Z1,63,95:M3D VERTEX O,X,Y2,Z2,63,0:M3D VERTEX O,X,Y1,Z2,0,0
7440   M3D QUAD O,B,B+1,B+2,B+3,TX
7450 END PROC
7500 ' a ring (tyre) of 12 rods in the plane y = CY
7600 PROC RING(O,CX,CY,CZ,R)
7610   LOCAL I,A1,A2
7620   FOR I=0 TO 11
7630     A1=I*PI/6:A2=(I+1)*PI/6
7640     CALL TUBE(O,CX+R*COS(A1),CY,CZ+R*SIN(A1),CX+R*COS(A2),CY,CZ+R*SIN(A2),1.4)
7650   NEXT I
7660 END PROC
7700 ' a vertex of a spoke ribbon (in the plane y = 0), through the tilt while XON is set
7800 PROC SPV(O,X,Z)
7810   IF XON THEN CALL PXV(O,X,0,Z) ELSE M3D VERTEX O,X,0,Z
7820 END PROC
7900 ' spokes: three ribbons through the hub, turned by ANG (radians); they are rebuilt each frame, so the wheel seems to turn
8000 PROC SPOKES(O,CX,CZ,ANG)
8010   LOCAL I,A,B,DX,DZ,TX,TZ,R
8020   R=9.6
8030   FOR I=0 TO 2
8040     A=ANG+I*PI/3
8050     DX=COS(A):DZ=SIN(A):TX=-DZ*0.7:TZ=DX*0.7
8060     B=M3D("VERTS",O)
8070     CALL SPV(O,CX-R*DX-TX,CZ-R*DZ-TZ):CALL SPV(O,CX+R*DX-TX,CZ+R*DZ-TZ)
8080     CALL SPV(O,CX+R*DX+TX,CZ+R*DZ+TZ):CALL SPV(O,CX-R*DX+TX,CZ-R*DZ+TZ)
8090     M3D QUAD O,B,B+1,B+2,B+3,TBT
8100   NEXT I
8110   CALL BX(O,CX-1.2,-1.6,CZ-1.2,CX+1.2,1.6,CZ+1.2,FN CC(0,11))
8120 END PROC
9000 ' ------------------------------------------------------------------------------------------------ the terrain, the roads
9010 ' ------------------------------------------------------------------------------------------------ the world in chunks (roadmap item 95, step 1: a prototype)
9020 ' The ground is cut into square chunks of 900 units (9 x 9 cells of 100). Only the chunks round John exist (7 x 7: 3 in every direction, so the nearest hole is more than 2700
9030 ' units away, beyond the far plane of 2400). A chunk is a pure function of its numbers cx,cy: the same chunk always comes out the same, however often it is built again. It owns a
9040 ' fixed block of object numbers by its slot, (cx mod 8) + 8 (cy mod 8): the ground 100 + slot, up to 24 trees 2000 + 24 slot + n; building a chunk overwrites the one that was
9050 ' in the slot, nothing is allocated or freed. The chunks cx, cy = -4 .. 3 are the hand-made area (the house, the roads, the groves, the field); the rest is made by the rules
9060 ' below. One chunk is built per frame (the nearest missing one first); a jump (NEWGAME, goto) builds all that are missing at once.
9070 ' what is near a road: the segments of the roads round the chunk (CHSEGS): -1 when X,Y is nearer than 100 to a dirt road's middle (55 wide, plus room) or than 310 to a tarred road's (220 wide
9080 ' and the posts beside it): no trees, no bales there
9100 FUNC NEARROAD(X,Y)
9110   LOCAL I,CL,UX,UY,TT,DX,DY
9120   FOR I=0 TO NSG-1
9130     CL=100:IF SGK(I) THEN CL=310
9140     IF X>MIN(SGX(I),SGU(I))-CL AND X<MAX(SGX(I),SGU(I))+CL AND Y>MIN(SGY(I),SGV(I))-CL AND Y<MAX(SGY(I),SGV(I))+CL THEN
9150       UX=SGU(I)-SGX(I):UY=SGV(I)-SGY(I)
9160       TT=((X-SGX(I))*UX+(Y-SGY(I))*UY)/(UX*UX+UY*UY):TT=MAX(0,MIN(1,TT))
9170       DX=SGX(I)+TT*UX-X:DY=SGY(I)+TT*UY-Y
9180       IF DX*DX+DY*DY<CL*CL THEN RETURN -1
9190     END IF
9200   NEXT I
9210   RETURN 0
9220 END FUNC
9300 ' the kind of ground at X,Y: the kind of the nearest of the nine feature points CKRX / CKRY / CKRT (set by CHREGIONS for the chunk): 0 plain, 1 meadow, 2 field, 3 forest
9400 FUNC GKIND(X,Y)
9410   LOCAL K,B,D,BD
9420   BD=1000000000000:B=0
9430   FOR K=0 TO 8
9440     D=(CKRX(K)-X)*(CKRX(K)-X)+(CKRY(K)-Y)*(CKRY(K)-Y)
9450     IF D<BD THEN BD=D:B=K
9460   NEXT K
9470   RETURN CKRT(B)
9480 END FUNC
9500 ' the picture of the ground cell round X,Y: the hand-made area keeps its CELLTEX; outside it rock (324) on high or steep ground, mud (320) in a lake, else the kind of ground: meadow lush grass, field soil, forest dry grass (needles)
9600 FUNC OCELL(X,Y)
9610   LOCAL K,H,S
9620   IF X>=-3600 AND X<3600 AND Y>=-3600 AND Y<3600 THEN RETURN FN CELLTEX(X,Y)
9630   H=FN HT(X,Y):S=ABS(FN HT(X+50,Y)-FN HT(X-50,Y))+ABS(FN HT(X,Y+50)-FN HT(X,Y-50))
9640   IF H>70 OR S>45 THEN RETURN 324
9650   IF H<WZ+1.5 THEN RETURN 320
9660   K=FN GKIND(X,Y):IF K>0 THEN RETURN VAL(MID$("303320302",K*3-2,3))
9670   RETURN FN CELLTEX(X,Y)
9680 END FUNC
9700 ' the feature points of the regions (1800 units = 2 chunks square) round the chunk's region: one point in each, anywhere in it, with a kind (38 % forest, 28 % meadow, 20 % field, 14 % plain)
9800 PROC CHREGIONS(CX,CY)
9810   LOCAL RX,RY,I,J,K,U
9820   RX=INT(CX/2):RY=INT(CY/2):K=0
9830   FOR J=-1 TO 1
9840     FOR I=-1 TO 1
9850       CKRX(K)=(RX+I)*1800+HASH(RX+I,RY+J,1)*1800
9860       CKRY(K)=(RY+J)*1800+HASH(RX+I,RY+J,2)*1800
9870       U=HASH(RX+I,RY+J,3)
9880       CKRT(K)=0
9890       IF U<0.86 THEN CKRT(K)=2
9900       IF U<0.66 THEN CKRT(K)=1
9910       IF U<0.38 THEN CKRT(K)=3
9920       K=K+1
9930     NEXT I
9940   NEXT J
9950 END PROC
10000 ' the chunk's decorations are freed (ruins, bales, posts) when it is made again for a coarser level or for another place
10100 PROC CHCLEAR(SL)
10110   M3D OBJFREE 14000+SL*32,14000+SL*32+31:M3D OBJFREE 11200+SL*6,11200+SL*6+5:M3D OBJFREE 8800+SL*6,8800+SL*6+5:CHB(SL)=0
10120 END PROC
10200 ' builds the chunk cx,cy into the slot SL at the level LV (0: 9 x 9 cells of 100 units up to 2 chunks away, 1: 3 x 3 of 300 up to 5, 2: one cell of 900; the chunks MORPH: level 0 from 900 to 1800 units from the player into the grid of 300, level 1 from 2700 to 4500 into the grid of 900; NEWC: it is a new chunk, not only a new level): the ground, the road
10210 ' ribbons and the trees are made by the engine's worker thread (M3D RING mode 1: the hand-made area, 2: with trees, 3: with cheap trees: the chunks 4 or more away), taken by M3D POLL;
10220 ' the water plane of a new chunk, and for the chunks up to 3 away the ruins, hay bales and reflector posts (made once, freed again when the chunk goes farther away)
10300 PROC CHUNKBUILD(CX,CY,SL,LV,NEWC)
10310   LOCAL O,X0,Y0,T0,N,ST,MD,HM,MA,MB
10320   T0=TI
10330   CALL CHREGIONS(CX,CY):IF NEWC THEN CHB(SL)=0:CALL CHWATER(CX,CY,SL)
10340   O=8000+SL
10350   IF M3D("OBJON",O)=0 THEN M3D OBJECT O,1,235
10360   X0=CX*900:Y0=CY*900
10370   HM=(CX>=-4 AND CX<=3 AND CY>=-4 AND CY<=3)
10380   N=9:ST=100:MD=2:MA=900:MB=1800:IF LV=1 THEN N=3:ST=300:MD=3:MA=2700:MB=4500
10390   IF LV=2 THEN N=1:ST=900:MD=3:MA=0:MB=0
10400   IF HM THEN MD=1
10410   M3D RING O,X0,Y0,N,ST,200,MD,0,0,0,MA,MB
10420   IF HM THEN
10430     IF NEWC THEN CALL CHCLEAR(SL)
10440   ELSE
10450     IF LV=0 AND NOT CHB(SL) THEN
10460       CALL CHSEGS(CX,CY):CALL CHRUINS(CX,CY,SL):CALL CHROADS(CX,CY,SL):CALL CHBALES(CX,CY,SL):CHB(SL)=-1
10470     END IF
10480     IF LV>=1 AND CHB(SL) THEN CALL CHCLEAR(SL)
10490   END IF
10500   T0=TI-T0:CKCNT=CKCNT+1:CKTOT=CKTOT+T0:IF T0>CKMAX THEN CKMAX=T0
10510 END PROC
10600 ' makes sure the chunks round PX,PY exist: builds up to MAXN of the missing ones (or those whose level changed), the nearest first (64 = all); chunks.lib keeps the book
10700 PROC CHUNKS(PX,PY,MAXN)
10710   LOCAL N
10720   CALL CK_UPDATE(PX,PY)
10730   FOR N=1 TO MAXN
10740     IF NOT FN CK_NEXT() THEN EXIT
10750     CALL CHUNKBUILD(FN CK_CX(),FN CK_CY(),FN CK_CSLOT(),FN CK_CLEVEL(),FN CK_CNEW())
10760     CALL CK_DONE
10770   NEXT N
10780 END PROC
10800 ' the first world: the tree kinds, the four rings, then the chunks round the start (a window of 17 x 17 chunks of 900 units)
10900 PROC BUILDTERRAIN
10910   LOCAL S
10920   IF NOT WLD THEN CALL CK_SETUP(900,8,2,5)
10930   FOR S=0 TO 399
10940     CKN(S)=0:CHB(S)=0
10950   NEXT S
10960   FOR K=0 TO 5:RGX(K)=-999999:RGY(K)=-999999:RGP(K)=-999999:RGQ(K)=-999999:NEXT K:CKCNT=0:CKTOT=0:CKMAX=0:CALL TREEPROTOS:IF NOT WLD THEN CALL RINGOBJS
10970   CALL TERRAININIT
10980 END PROC
11000 ' test mode: what the window round PX,PY looks like (for game1987test.sh): the chunks present, missing or of the wrong level, those of level 0, the chunks with forest (a mesh of more than 40
11010 ' triangles), a signature (the triangles of every chunk), the ones with water / ruins / a road near / bales / posts, the rings and the ground jobs not taken yet
11100 PROC CHUNKSTAT(PX,PY)
11110   LOCAL PCX,PCY,I,J,CX,CY,SL,N,M,NT,SG,NF,NW,NR,NO,NB,NFT,K,NPS,NMD,TR
11120   PCX=INT(PX/900):PCY=INT(PY/900)
11130   FOR J=-8 TO 8
11140     FOR I=-8 TO 8
11150       CX=PCX+I:CY=PCY+J:SL=FN CK_SLOT(CX,CY)
11160       IF FN CK_HAS(CX,CY) THEN
11170         TR=M3D("TRIS",8000+SL):N=N+1:NT=NT-(TR>40):SG=SG+TR*(SL+1)
11180         NW=NW-(M3D("OBJON",8400+SL)<>0):NR=NR-(M3D("OBJON",14000+SL*32)<>0):NB=NB-(M3D("OBJON",11200+SL*6)<>0):NPS=NPS-(M3D("OBJON",8800+SL*6)<>0)
11190         NO=NO-(MIN(M3D("ROADD",CX*900+450,CY*900+450),M3D("ROADT",CX*900+450,CY*900+450))<500)
11200         IF FN CK_LEVELAT(CX,CY)=0 THEN NF=NF+1
11210       ELSE
11220         M=M+1
11230       END IF
11240     NEXT I
11250   NEXT J
11260   FOR K=2 TO 4:NFT=NFT-(M3D("OBJON",27000+K)<>0 AND M3D("TRIS",27000+K)>0):NEXT K:NMD=M3D("RINGBUSY")
11270   PRINT "chunks have=";N;" missing=";M;" trees=";NT;" sig=";SG;" cx=";PCX;" cy=";PCY;" fine=";NF;" stale=";FN CK_STALE();" water=";NW;" ruins=";NR;" roads=";NO;" bales=";NB;" rings=";NFT;" posts=";NPS;" busy=";NMD
11280   PRINT "chunktime built=";CKCNT;" ticks=";CKTOT;" max=";CKMAX
11290 END PROC
11300 ' the ground function of the engine: hills of fractal noise (about 900 units across, 4 octaves, up to about 30 high), flattened round the house and over the wheat field
11400 PROC SETGROUND
11410   IF NOT WLD THEN M3D GROUND 1987,900,22,4,8
11420   IF NOT WLD THEN M3D GROUNDFLAT 0,0,0,260,200,420:M3D GROUNDFLAT 1,1100,650,700,500,300
11430   RH=70:IF WLD THEN RH=900
11440   ' a unit is 3.2 cm: HILLS (ridged noise up to 260 = 8 m high, rocky and walkable), real MOUNTAINS (peaks up to 37000 = 1190 m high, steep walls, 14000 .. 22000 units from the house: ridged noise where a very slow noise is high) and real LAKES (basins 350 = 11 m deep and 3000 .. 6000 = 100 .. 190 m across); none over the hand-made area (3900 units round the house, fading in over 1200)
11450   IF NOT WLD THEN M3D GROUNDQUIET 0,0,3900,3900,1200:M3D ROADNET 1800,0.45,140,36000,0.85,900:M3D HILLS 1987,500,260,5,0.05,0.4:M3D MOUNTAINS 7,9000,80000,4,0.25,0.6,14000,22000
11460   M3D RINGPIC 325,324,301,327,328,320,303,320,302,WZ,RH,304:M3D TREEPIC 312,309,310,317:IF WLD THEN M3D POSTPIC FN CC(0,13),FN CC(0,15),FN CC(1,11),321
11470   IF WLD THEN M3D HOUSEPIC 0,361:M3D HOUSEPIC 1,362:M3D HOUSEPIC 2,363:M3D HOUSEPIC 3,364:M3D HOUSEPIC 4,365:M3D HOUSEPIC 5,366:M3D HOUSEPIC 6,306:M3D HOUSEPIC 7,306:M3D HOUSEPIC 8,306:M3D HOUSEPIC 10,326:M3D HOUSEPIC 12,305:M3D HOUSEPIC 13,305:M3D HOUSEPIC 14,307:M3D HOUSEPIC 11,370 ELSE M3D LAKES 4711,9000,350,0.3,0.5
11480 END PROC
11500 ' what stops John and the bicycle: only water too deep to wade (12 below the water level WZ); the rocky hills are walkable and rideable (the engine lets a body climb every slope up to 72 degrees and lifts one that got buried)
11600 FUNC BLOCKED(X,Y)
11610   RETURN FN HT(X,Y)<WZ-12
11620 END FUNC
11700 ' the water of a chunk: a translucent plane at the water level WZ over the chunk when the ground dips below it somewhere (5 x 5 samples), else none (object 4000 + slot)
11800 PROC CHWATER(CX,CY,SL)
11810   LOCAL I,J,W,MN,X0,Y0
11820   W=8400+SL:X0=CX*900:Y0=CY*900:MN=1000
11830   FOR J=0 TO 4
11840     FOR I=0 TO 4
11850       MN=MIN(MN,FN HT(X0+I*225,Y0+J*225))
11860     NEXT I
11870   NEXT J
11880   IF MN<WZ THEN
11890     M3D OBJECT W,30,235
11900     M3D VERTEX W,X0,Y0,WZ,X0*0.5,Y0*0.5:M3D VERTEX W,X0+900,Y0,WZ,(X0+900)*0.5,Y0*0.5
11910     M3D VERTEX W,X0+900,Y0+900,WZ,(X0+900)*0.5,(Y0+900)*0.5:M3D VERTEX W,X0,Y0+900,WZ,X0*0.5,(Y0+900)*0.5
11920     M3D QUAD W,0,1,2,3,325
11930   ELSE
11940     M3D OBJFREE W,W
11950   END IF
11960 END PROC
12000 LABEL ROADDATA
12010 DATA 8, 0,-165, 0,-450, 250,-800, 800,-1000, 1500,-950, 2200,-700, 3000,-800, 3600,-1100
12020 DATA 9, -40,150, -300,500, -350,1000, -200,1600, 300,2200, 1000,2600, 1800,2500, 2600,2800, 3600,3000
12030 DATA 5, 1500,-950, 2300,-300, 2500,500, 2200,1500, 1800,2480
12040 DATA 0
12100 ' sand roads: Catmull-Rom curves through the control points, a strip of quads laid 3 units above the ground (see-through edges)
12110 PROC BUILDROADS
12120   LOCAL R,N,I,K,T,T2,T3,A,B,NS,O,X,Y,DX,DY,L,Z,D,V,X0,Y0
12130   RPN=0:NPL=0
12140   RESTORE ROADDATA
12150   R=0
12160   READ N
12170   WHILE N>0
12180     FOR I=0 TO N-1
12190       READ CPX(I),CPY(I)
12200     NEXT I
12210     NS=0
12220     FOR I=0 TO N-2
12230       A=I-1:IF A<0 THEN A=0
12240       B=I+2:IF B>N-1 THEN B=N-1
12250       FOR K=0 TO 5
12260         T=K/6:T2=T*T:T3=T2*T
12270         SMX(NS)=0.5*(2*CPX(I)+(CPX(I+1)-CPX(A))*T+(2*CPX(A)-5*CPX(I)+4*CPX(I+1)-CPX(B))*T2+(3*CPX(I)-CPX(A)-3*CPX(I+1)+CPX(B))*T3)
12280         SMY(NS)=0.5*(2*CPY(I)+(CPY(I+1)-CPY(A))*T+(2*CPY(A)-5*CPY(I)+4*CPY(I+1)-CPY(B))*T2+(3*CPY(I)-CPY(A)-3*CPY(I+1)+CPY(B))*T3)
12290         NS=NS+1
12300       NEXT K
12310     NEXT I
12320     SMX(NS)=CPX(N-1):SMY(NS)=CPY(N-1):NS=NS+1
12330     O=200+R
12340     M3D OBJECT O,1,235
12350     D=0
12360     FOR K=0 TO NS-1
12370       A=K-1:IF A<0 THEN A=0
12380       B=K+1:IF B>NS-1 THEN B=NS-1
12390       DX=SMX(B)-SMX(A):DY=SMY(B)-SMY(A)
12400       L=SQR(DX*DX+DY*DY):DX=DX/L:DY=DY/L
12410       IF K>0 THEN D=D+SQR((SMX(K)-SMX(K-1))^2+(SMY(K)-SMY(K-1))^2)
12420       Z=FN HT(SMX(K),SMY(K))+3+R
12430       X0=SMX(K):Y0=SMY(K)
12440       M3D VERTEX O,X0+DY*55,Y0-DX*55,FN HT(X0+DY*55,Y0-DX*55)+3+R,64,D*0.58
12450       M3D VERTEX O,X0-DY*55,Y0+DX*55,FN HT(X0-DY*55,Y0+DX*55)+3+R,0,D*0.58
12460       IF RPN<300 THEN RPX(RPN)=X0:RPY(RPN)=Y0:RPF(RPN)=(K>0):RPN=RPN+1
12470       IF R=0 AND K MOD 5=2 AND NPL<39 THEN PLX(NPL)=X0-DY*92:PLY(NPL)=Y0+DX*92:NPL=NPL+1
12480     NEXT K
12490     FOR K=0 TO NS-2
12500       M3D QUAD O,K*2,K*2+2,K*2+3,K*2+1,304
12510     NEXT K
12520     R=R+1
12530     READ N
12540   WEND
12550 END PROC
13000 ' ------------------------------------------------------------------------------------------------ trees
13010 ' the three kinds of tree (K: 0 broadleaf, 1 pine, 2 poplar) are built once, hidden, as the objects 340 .. 342; TREE makes object O an instance of one (it shares the mesh, M3D INSTANCE)
13020 ' scaled by S, standing at X,Y: the crown starts above 72 units, so you can walk and ride under it
13100 PROC TREEPROTO(K)
13110   LOCAL J,O,S
13120   O=340+K:S=1
13130   M3D OBJECT O,24,235
13140   LRR(0)=5:LRZ(0)=0:LRV(0)=63:LRR(1)=3.2:LRZ(1)=100:LRV(1)=0
13150   IF K=2 THEN LRZ(1)=80
13160   CALL LATHE(O,2,312,S)
13170   IF K=0 THEN
13180     LRR(0)=14:LRZ(0)=86:LRV(0)=63:LRR(1)=46:LRZ(1)=108:LRV(1)=50:LRR(2)=54:LRZ(2)=140:LRV(2)=32
13190     LRR(3)=38:LRZ(3)=182:LRV(3)=14:LRR(4)=0.6:LRZ(4)=206:LRV(4)=0
13200     CALL LATHE(O,5,309,S)
13210   END IF
13220   IF K=1 THEN
13230     FOR J=0 TO 2
13240       LRR(0)=38-J*7:LRZ(0)=74+J*38:LRV(0)=63:LRR(1)=0.6:LRZ(1)=134+J*38:LRV(1)=0
13250       CALL LATHE(O,2,310,S)
13260     NEXT J
13270   END IF
13280   IF K=2 THEN
13290     LRR(0)=10:LRZ(0)=74:LRV(0)=63:LRR(1)=22:LRZ(1)=120:LRV(1)=45:LRR(2)=24:LRZ(2)=180:LRV(2)=25
13300     LRR(3)=14:LRZ(3)=240:LRV(3)=8:LRR(4)=0.6:LRZ(4)=285:LRV(4)=0
13310     CALL LATHE(O,5,317,S)
13320   END IF
13330   M3D OBJSHOW O,0
13340 END PROC
13400 PROC TREE(O,X,Y,S,K)
13410   LOCAL YW
13420   YW=TYW:IF YW<0 THEN YW=FN RN(360)
13430   M3D INSTANCE O,340+K,X,Y,FN HT(X,Y)-2,YW,S
13440 END PROC
13500 PROC TREEPROTOS
13510   LOCAL K
13520   FOR K=0 TO 2:CALL TREEPROTO(K):NEXT K:CALL MKBALE:CALL MKPOST
13530 END PROC
13600 ' may a tree stand here? inside the limit LIM, not in the wheat field or the yard, not on a road, not too near another tree
13700 FUNC TREEOK(X,Y,LIM)
13710   LOCAL I,DX,DY
13720   IF ABS(X)>LIM OR ABS(Y)>LIM THEN RETURN 0
13730   IF ABS(X-1100)<790 AND ABS(Y-650)<590 THEN RETURN 0
13740   IF ABS(X)<330 AND ABS(Y)<300 THEN RETURN 0
13750   FOR I=0 TO RPN-1
13760     DX=RPX(I)-X:DY=RPY(I)-Y
13770     IF DX*DX+DY*DY<6400 THEN RETURN 0
13780   NEXT I
13790   FOR I=0 TO NTR-1
13800     DX=TRX(I)-X:DY=TRY(I)-Y
13810     IF DX*DX+DY*DY<4900 THEN RETURN 0
13820   NEXT I
13830   RETURN -1
13840 END FUNC
13900 LABEL GROVES
13910 DATA -1800,1800,700,40,2, -2600,-600,600,34,2, -900,-2000,500,30,0, 2400,-1900,700,30,1, 2800,1800,600,30,2, 200,3000,600,26,0, -450,70,140,7,0, 3000,200,500,18,1
13920 DATA -1200,300,260,10,0, 1800,-300,200,8,2, 0,0,0,0,0
13930 PROC BUILDTREES
13940   LOCAL CX,CY,R,N,K,I,T,A,D,X,Y,O,P,S,E
13950   NTR=0:O=1000
13960   RANDOMIZE 1987
13970   RESTORE GROVES
13980   READ CX,CY,R,N,K
13990   WHILE N>0
14000     FOR I=1 TO N*4
14010       IF NTR<520 THEN
14020         A=RND(1)*6.2832:D=SQR(RND(1))*R
14030         X=CX+D*COS(A):Y=CY+D*SIN(A)
14040         IF FN TREEOK(X,Y,3300) THEN
14050           TRX(NTR)=X:TRY(NTR)=Y:NTR=NTR+1
14060           T=K:IF K=2 THEN T=FN RN(3)
14070           S=0.9+RND(1)*0.5:IF T=1 THEN S=1+RND(1)*0.4
14080           CALL TREE(O,X,Y,S,T):O=O+1
14090         END IF
14100       END IF
14110     NEXT I
14120     READ CX,CY,R,N,K
14130   WEND
14140   ' a big oak and a birch by the house
14150   CALL TREE(O,-345,-70,1.4,0):O=O+1
14160   CALL TREE(O,330,190,1.25,0):O=O+1
14170 END PROC
15000 ' ------------------------------------------------------------------------------------------------ the house
15100 PROC BUILDHOUSE
15110   LOCAL K,Z,H
15120   M3D OBJECT 300,0,235
15130   CALL BX(300,-130,-100,0,130,100,5,305)
15140   ' south wall: the door, a window
15150   CALL BX(300,-130,-100,5,-100,-92,106,305):CALL BX(300,-100,-100,5,-50,-92,44,305):CALL BX(300,-100,-100,84,-50,-92,106,305)
15160   CALL BX(300,-50,-100,5,-24,-92,106,305):CALL BX(300,24,-100,5,130,-92,106,305):CALL BX(300,-24,-100,70,24,-92,106,305)
15170   ' north wall: a window
15180   CALL BX(300,-130,92,5,-70,100,106,305):CALL BX(300,-70,92,5,-20,100,44,305):CALL BX(300,-70,92,84,-20,100,106,305):CALL BX(300,-20,92,5,130,100,106,305)
15190   ' west wall, east wall with a window
15200   CALL BX(300,-130,-92,5,-122,92,106,305)
15210   CALL BX(300,122,-92,5,130,-20,106,305):CALL BX(300,122,30,5,130,92,106,305):CALL BX(300,122,-20,5,130,30,44,305):CALL BX(300,122,-20,84,130,30,106,305)
15220   CALL BX(300,-130,-100,100,130,100,104,305)
15230   ' chimney
15240   CALL BX(300,78,18,120,98,38,200,307)
15250   ' the roof (two sided) and the gables
15260   M3D OBJECT 301,16,230
15270   M3D VERTEX 301,-142,-118,96,-71,0:M3D VERTEX 301,142,-118,96,71,0:M3D VERTEX 301,142,0,165,71,69:M3D VERTEX 301,-142,0,165,-71,69
15280   M3D QUAD 301,0,1,2,3,306
15290   M3D VERTEX 301,142,118,96,71,0:M3D VERTEX 301,-142,118,96,-71,0:M3D VERTEX 301,-142,0,165,-71,69:M3D VERTEX 301,142,0,165,71,69
15300   M3D QUAD 301,4,5,6,7,306
15310   M3D OBJECT 302,16,235
15320   FOR K=0 TO 1
15330     Z=-130+K*260
15340     M3D VERTEX 302,Z,-100,106,0,59:M3D VERTEX 302,Z,100,106,100,59:M3D VERTEX 302,Z,0,165,50,0
15350     M3D FACE 302,K*3,K*3+1,K*3+2,305
15360   NEXT K
15370   ' the porch
15380   M3D OBJECT 304,0,235
15390   CALL BX(304,-70,-150,0,70,-100,7,305)
15400   CALL BX(304,-68,-148,7,-62,-142,92,305):CALL BX(304,62,-148,7,68,-142,92,305)
15410   CALL BX(304,-76,-158,92,76,-96,97,306)
15420   ' glass: panes of the windows (see-through frames)
15430   M3D OBJECT 306,17,235
15440   CALL PANEY(306,-100,-50,-96,44,84,313):CALL PANEY(306,-70,-20,96,44,84,313):CALL PANEX(306,126,-20,30,44,84,313)
15450   ' the front door, standing open
15460   M3D OBJECT 305,20,235
15470   CALL BX(305,0,-2,5,44,2,68,305)
15480   M3D OBJPOS 305,-22,-96,0,100
15490   ' furniture
15500   M3D OBJECT 307,0,235
15510   CALL BX(307,-122,20,5,-78,92,15,305):CALL BX(307,-121,21,15,-79,91,23,FN CC(9,7)):CALL BX(307,-120,70,23,-82,90,28,FN CC(0,14)):CALL BX(307,-121,21,23,-79,66,26,FN CC(1,6))
15520   CALL BX(307,-80,50,26,-30,85,29,305)
15530   CALL BX(307,-78,52,5,-74,56,26,305):CALL BX(307,-36,52,5,-32,56,26,305):CALL BX(307,-78,79,5,-74,83,26,305):CALL BX(307,-36,79,5,-32,83,26,305)
15540   CALL BX(307,-61,33,5,-49,45,17,305)
15550   CALL BX(307,76,16,5,104,44,41,FN CC(0,2)):CALL BX(307,86,26,41,94,34,100,FN CC(0,3))
15560   CALL BX(307,32,90,5,110,92,84,305):CALL BX(307,30,80,5,32,92,84,305):CALL BX(307,110,80,5,112,92,84,305)
15570   FOR Z=28 TO 76 STEP 24
15580     CALL BX(307,32,80,Z,110,92,Z+2,305)
15590     FOR K=0 TO 7
15600       H=1+FN RN(8)
15610       CALL BX(307,34+K*9.5,83,Z+2,34+K*9.5+7,89,Z+2+14+FN RN(6),FN CC(H,7+FN RN(3)))
15620     NEXT K
15630   NEXT Z
15640   CALL BX(307,-50,-40,5,30,30,6,323)
15650 END PROC
15700 ' the yard: a fence with two gates, the woodpile, mailbox, barrel, telephone poles, hay, the scarecrow
15800 PROC BUILDYARD
15810   LOCAL X,Y,K
15820   M3D OBJECT 320,8,235
15830   FOR X=-250 TO 250 STEP 40
15840     IF ABS(X)>=60 THEN CALL BX(320,X-2,-202,0,X+2,-198,44,305)
15850     IF X<-130 OR X>-20 THEN CALL BX(320,X-2,198,0,X+2,202,44,305)
15860   NEXT X
15870   CALL BX(320,-250,-201,20,-60,-199,26,305):CALL BX(320,-250,-201,34,-60,-199,40,305):CALL BX(320,60,-201,20,250,-199,26,305):CALL BX(320,60,-201,34,250,-199,40,305)
15880   CALL BX(320,-250,199,20,-130,201,26,305):CALL BX(320,-250,199,34,-130,201,40,305):CALL BX(320,-20,199,20,250,201,26,305):CALL BX(320,-20,199,34,250,201,40,305)
15890   FOR Y=-160 TO 160 STEP 40
15900     CALL BX(320,-252,Y-2,0,-248,Y+2,44,305):CALL BX(320,248,Y-2,0,252,Y+2,44,305)
15910   NEXT Y
15920   CALL BX(320,-251,-200,20,-249,200,26,305):CALL BX(320,-251,-200,34,-249,200,40,305):CALL BX(320,249,-200,20,251,200,26,305):CALL BX(320,249,-200,34,251,200,40,305)
15930   M3D OBJECT 321,8,235
15940   FOR K=0 TO 5
15950     CALL BX(321,135,-60+K*8,0,165,-54+K*8,10,FN CC(12,4+K))
15960     IF K<5 THEN CALL BX(321,138,-56+K*8,10,162,-50+K*8,20,FN CC(12,6+K))
15970   NEXT K
15980   CALL BX(321,90,-185,0,93,-182,34,312):CALL BX(321,84,-190,34,100,-176,46,FN CC(9,5))
15990   M3D OBJECT 322,8,235
16000   LRR(0)=9:LRZ(0)=0:LRV(0)=0:LRR(1)=11:LRZ(1)=8:LRV(1)=0:LRR(2)=11:LRZ(2)=24:LRV(2)=0:LRR(3)=9:LRZ(3)=32:LRV(3)=0
16010   CALL LATHE(322,4,FN CC(9,4),1)
16020   M3D OBJPOS 322,-150,-135,0
16030   M3D OBJECT 420,8,235
16040   FOR K=0 TO NPL-1
16050     PZG=FN HT(PLX(K),PLY(K))
16060     CALL BX(420,PLX(K)-1.6,PLY(K)-1.6,PZG-6,PLX(K)+1.6,PLY(K)+1.6,PZG+150,312)
16070     CALL BX(420,PLX(K)-12,PLY(K)-1.2,PZG+138,PLX(K)+12,PLY(K)+1.2,PZG+141,312)
16080   NEXT K
16090   M3D OBJECT 411,8,235
16100   FOR K=0 TO 2
16110     CALL BX(411,340,170+K*40,0,380,196+K*40,24,FN CC(3,9+K))
16120   NEXT K
16130   CALL BX(411,345,210,24,375,236,46,FN CC(3,11))
16140   ' the scarecrow in the field
16150   M3D OBJECT 410,8,235
16160   CALL BX(410,-2,-2,0,2,2,72,312):CALL BX(410,-24,-1.5,50,24,1.5,54,312):CALL BX(410,-8,-4,32,8,4,56,FN CC(9,5))
16170   CALL BX(410,-5,-5,58,5,5,68,FN CC(14,11)):CALL BX(410,-9,-9,68,9,9,71,312):CALL BX(410,-5,-5,71,5,5,80,312)
16180   M3D OBJPOS 410,1100,650,0,200
16190 END PROC
17000 ' ------------------------------------------------------------------------------------------------ the wheat field: 16 blocks of crossing see-through rows with a top
17100 PROC BUILDWHEAT
17110   LOCAL BI,BJ,O,X1,Y1,X2,Y2,X,Y,B
17120   FOR BJ=0 TO 3
17130     FOR BI=0 TO 3
17140       O=400+BI+BJ*4
17150       M3D OBJECT O,29,235
17160       X1=400+BI*350:X2=X1+350:Y1=150+BJ*250:Y2=Y1+250
17170       M3D VERTEX O,X1,Y1,34,X1*0.5,Y1*0.5:M3D VERTEX O,X2,Y1,34,X2*0.5,Y1*0.5:M3D VERTEX O,X2,Y2,34,X2*0.5,Y2*0.5:M3D VERTEX O,X1,Y2,34,X1*0.5,Y2*0.5
17180       M3D QUAD O,0,1,2,3,321
17190       Y=Y1+3
17200       WHILE Y<Y2
17210         B=M3D("VERTS",O)
17220         M3D VERTEX O,X1,Y,0,X1*1.3333,63:M3D VERTEX O,X2,Y,0,X2*1.3333,63:M3D VERTEX O,X2,Y,46,X2*1.3333,0:M3D VERTEX O,X1,Y,46,X1*1.3333,0
17230         M3D QUAD O,B,B+1,B+2,B+3,308
17240         Y=Y+30+FN RN(6)
17250       WEND
17260       X=X1+5
17270       WHILE X<X2
17280         B=M3D("VERTS",O)
17290         M3D VERTEX O,X,Y1,0,Y1*1.3333,63:M3D VERTEX O,X,Y2,0,Y2*1.3333,63:M3D VERTEX O,X,Y2,46,Y2*1.3333,0:M3D VERTEX O,X,Y1,46,Y1*1.3333,0
17300         M3D QUAD O,B,B+1,B+2,B+3,308
17310         X=X+60+FN RN(8)
17320       WEND
17330     NEXT BI
17340   NEXT BJ
17350 END PROC
18000 ' ------------------------------------------------------------------------------------------------ the bicycle
18010 ' the bicycle: the frame (x forward, y left, z up, the origin on the ground halfway between the axles) and the front (fork, stem, handlebar and wheel, turning about the
18020 ' steering axis; origin: on the ground below it, 12 in front of the frame's origin) are procedures of an object O, so that they can be built upright (objects 510 and 511) and
18030 ' tilted by the transform of BUILDLEAN into one object (514) while the bicycle leans against a wall, falls, slides or lies
18100 PROC BIKEFRAME(O)
18110   TBT=FN CC(1,9)
18120   CALL TUBE(O,-2,0,9,-8,0,25,1.3)
18130   CALL TUBE(O,-8,0,24,12,0,26,1.2)
18140   CALL TUBE(O,-2,0,9,12,0,22,1.3)
18150   CALL TUBE(O,12,0,21,12,0,29,1.6)
18160   CALL TUBE(O,-2,0,9,-17,0,11,0.8)
18170   CALL TUBE(O,-8,0,24,-17,0,11,0.8)
18180   TBT=FN CC(0,6)
18190   CALL TUBE(O,-8,0,25,-9,0,30,0.9)
18200   CALL BX(O,-14,-2.6,29.4,-4,2.6,31,FN CC(12,4))
18210   CALL BX(O,-18,-1,0,-16,1,2,FN CC(0,3))
18220   TBT=FN CC(0,2)
18230   CALL RING(O,-17,0,11,10.5)
18240   CALL BX(O,-18.5,-3,15,-15.5,3,16.5,FN CC(0,4))
18245   CALL BX(O,-16.2,-2,28,-14,2,32,FN CC(0,3))
18250 END PROC
18300 PROC BIKEFRONT(O)
18310   TBT=FN CC(0,9)
18320   CALL TUBE(O,0,0,26,6,0,11,0.8)
18330   CALL TUBE(O,0,0,26,-1,0,31,0.9)
18340   CALL TUBE(O,-2,-10,31,-2,10,31,0.8)
18350   TBT=FN CC(0,2)
18360   CALL RING(O,6,0,11,10.5)
18370   CALL BX(O,-3.5,-12,29.8,-0.5,-8,32.2,FN CC(0,1)):CALL BX(O,-3.5,8,29.8,-0.5,12,32.2,FN CC(0,1))
18380   CALL BX(O,3,-1,36,5,1,38,FN CC(3,14))
18385   CALL BX(O,0.5,-2.8,23.2,4,2.8,28.8,FN CC(0,3))
18390 END PROC
18400 PROC BUILDBIKE
18410   M3D OBJECT 510,28,235:CALL BIKEFRAME(510)
18420   M3D OBJECT 511,28,235:CALL BIKEFRONT(511)
18430   M3D OBJECT 512,28,235
18440   M3D OBJECT 513,28,235
18450   M3D OBJECT 514,28,235:M3D OBJSHOW 514,0
18460   M3D OBJECT 530,30,235:CALL BX(530,-9,-8,0,9,8,0.3,FN CC(0,0))
18470   M3D OBJECT 531,30,235:CALL BX(531,-27,-6,0,27,6,0.3,FN CC(0,0))
18475   CALL BUILDLENS
18480   M3D OBJPOS 510,0,0,0,0:M3D OBJPOS 511,12,0,0,0:M3D OBJPOS 512,0,0,0,0:M3D OBJPOS 513,12,0,0,0
18490 END PROC
18500 ' the parts of the bicycle that are built again each frame: the rear wheel's spokes with the cranks and pedals (object O in the frame's coordinates) ...
18600 PROC BIKEREAR(O)
18610   LOCAL A,PX,PZ,Q
18620   TBT=FN CC(0,11)
18630   CALL SPOKES(O,-17,11,-WA)
18640   TBT=FN CC(0,10)
18650   FOR Q=0 TO 1
18660     A=-(CA+Q*PI)
18670     PX=-2+5.5*COS(A):PZ=9+5.5*SIN(A)
18680     IF Q=0 THEN
18690       CALL TUBE(O,-2,3.5,9,PX,3.5,PZ,0.7)
18700       CALL BX(O,PX-3,3.6,PZ-0.7,PX+3,7.6,PZ+0.7,FN CC(0,3))
18710     ELSE
18720       CALL TUBE(O,-2,-3.5,9,PX,-3.5,PZ,0.7)
18730       CALL BX(O,PX-3,-7.6,PZ-0.7,PX+3,-3.6,PZ+0.7,FN CC(0,3))
18740     END IF
18750   NEXT Q
18760 END PROC
18800 ' ... and the front wheel's spokes (in the front's coordinates)
18900 PROC BIKEFR(O)
18910   TBT=FN CC(0,10)
18920   CALL SPOKES(O,6,11,-WA)
18930 END PROC
19000 ' places the bicycle's objects at its position and heading (upright: the objects 510 .. 513, the spokes and the pedals are built again each frame; leaning, falling,
19010 ' sliding or lying: the one tilted object 514, see PLACELEAN)
19100 PROC PLACEBIKE
19110   LOCAL C,S,FX,FY
19120   IF BST<>0 THEN
19130     CALL PLACELEAN
19140     EXIT PROC
19150   END IF
19160   IF BVIS=0 THEN
19170     BVIS=-1:M3D OBJSHOW 514,0:M3D OBJSHOW 510,1:M3D OBJSHOW 511,1:M3D OBJSHOW 512,1:M3D OBJSHOW 513,1
19180   END IF
19190   C=COS(BH*PI/180):S=SIN(BH*PI/180)
19200   FX=BX+12*C:FY=BY+12*S
19210   M3D OBJPOS 510,BX,BY,BZ,BH
19220   ' the spokes and the pedals depend on the wheel angle WA and the crank angle CA only: a bicycle that stands still keeps its meshes
19230   IF WA<>PWA OR CA<>PCA THEN
19240     PWA=WA:PCA=CA:CNB=CNB+1
19250     M3D OBJECT 512,28,235:CALL BIKEREAR(512)
19260     M3D OBJECT 513,28,235:CALL BIKEFR(513)
19270   END IF
19280   M3D OBJPOS 512,BX,BY,BZ,BH
19290   M3D OBJPOS 511,FX,FY,BZ,BH+BSA
19300   M3D OBJPOS 513,FX,FY,BZ,BH+BSA
19310 END PROC
19400 ' the tilt of the bicycle: every vertex of the parts built while XON is set goes through PXV: the front's parts (XFR) are first turned about the steering axis (12 in front of
19410 ' the frame's origin) by the angle SD, then everything is rolled about the line where the wheels touch the ground (the x axis) by the lean LA (positive: the top goes to the left,
19420 ' +y) and lifted by XLF so that the handlebar does not sink into the ground
19500 PROC PXV(O,X,Y,Z)
19510   LOCAL XX,YY
19520   XX=X:YY=Y
19530   IF XFR THEN XX=12+X*XFC-Y*XFS:YY=X*XFS+Y*XFC
19540   M3D VERTEX O,XX,YY*XRC+Z*XRS,Z*XRC-YY*XRS+XLF
19550 END PROC
19600 ' a box through the tilt (the six faces are quads: a rolled box is not axis aligned any more)
19700 PROC BXX(O,X1,Y1,Z1,X2,Y2,Z2,TX)
19710   LOCAL B
19720   B=M3D("VERTS",O)
19730   CALL PXV(O,X1,Y1,Z1):CALL PXV(O,X2,Y1,Z1):CALL PXV(O,X2,Y2,Z1):CALL PXV(O,X1,Y2,Z1)
19740   CALL PXV(O,X1,Y1,Z2):CALL PXV(O,X2,Y1,Z2):CALL PXV(O,X2,Y2,Z2):CALL PXV(O,X1,Y2,Z2)
19750   M3D QUAD O,B,B+1,B+2,B+3,TX:M3D QUAD O,B+4,B+5,B+6,B+7,TX
19760   M3D QUAD O,B,B+1,B+5,B+4,TX:M3D QUAD O,B+1,B+2,B+6,B+5,TX
19770   M3D QUAD O,B+2,B+3,B+7,B+6,TX:M3D QUAD O,B+3,B,B+4,B+7,TX
19780 END PROC
19800 ' the whole bicycle in one object (514), rolled by the lean LA (radians) with the front wheel turned by SD degrees
19900 PROC BUILDLEAN(LA,SD)
19910   LOCAL Y1,Y2
19920   XRS=SIN(LA):XRC=COS(LA):XFS=SIN(SD*PI/180):XFC=COS(SD*PI/180)
19930   Y1=-2*XFS+12*XFC:Y2=-2*XFS-12*XFC
19940   XLF=-MIN(0,MIN(31*XRC-Y1*XRS,MIN(31*XRC-Y2*XRS,-1.4*ABS(XRS))))
19950   M3D OBJECT 514,28,235
19960   XON=-1:XFR=0
19970   CALL BIKEFRAME(514):CALL BIKEREAR(514)
19980   XFR=-1
19990   CALL BIKEFRONT(514):CALL BIKEFR(514)
20000   XON=0:XFR=0
20010 END PROC
20100 PROC PLACELEAN
20110   IF BVIS<>0 THEN
20120     BVIS=0:M3D OBJSHOW 510,0:M3D OBJSHOW 511,0:M3D OBJSHOW 512,0:M3D OBJSHOW 513,0:M3D OBJSHOW 514,1
20130   END IF
20140   IF BDR<>0 OR BST=3 OR BST=6 THEN
20150     CALL BUILDLEAN(BLS*BLN,BSTR):BDR=0
20160   END IF
20170   M3D OBJPOS 514,BX,BY,BZ,BH
20180 END PROC
20200 ' a bicycle cannot stand by itself. BST: 0 upright (ridden, or rolling on without a rider for WFALL seconds), 1 leaning against the house (the start), 3 falling over (the lean BLN
20210 ' grows to 90 degrees, to the side BLS), 4 sliding on its side and turning (BYW) while it slows down (WSLIDE), 5 lying, 6 being picked up (E: the lean goes back to 0 in the time of getting on).
20220 ' A slide goes straight on in the direction BSD while the bicycle turns about itself.
20300 PROC BIKEFALL(DT)
20310   LOCAL A
20320   IF MODE=1 THEN
20330     BST=0:BRT=0
20340     EXIT PROC
20350   END IF
20360   IF BST=0 THEN
20370     IF MODE=2 THEN
20380       BRT=0
20390     ELSE
20400       BRT=BRT+DT
20410       IF BRT>=WFALL THEN
20420         BST=3:BLN=0:BLW=0.4:BLS=1:IF (INT(BX+BY) AND 1)<>0 THEN BLS=-1
20430         BSTR=BLS*55:BDR=-1
20440       END IF
20450     END IF
20460   END IF
20470   IF BST=3 THEN
20480     BLW=BLW+9*DT:BLN=BLN+BLW*DT
20490     IF BLN>=1.5708 THEN
20500       BLN=1.5708:BST=4:BSD=BH:BDR=-1
20510       A=BX-JX:IF A*A+(BY-JY)*(BY-JY)<490000 THEN CALL SND(4)
20520     END IF
20530   END IF
20540   IF BST=4 THEN
20550     IF BV>0 THEN BV=MAX(0,BV-WSLIDE*DT)
20560     IF BV<0 THEN BV=MIN(0,BV+WSLIDE*DT)
20570     BYW=BLS*2.6*MIN(1,ABS(BV)/120)
20580     BH=BH+BYW*DT*180/PI
20590     IF ABS(BV)<1 THEN BV=0:BYW=0:BST=5
20600   END IF
20610   IF BST=6 THEN
20620     A=MIN(1,MT/0.45):BLN=BLN0*(1-A):BSTR=BSTR0*(1-A)
20630     IF A>=1 THEN BST=0:BRT=0:BLN=0
20640   END IF
20650   IF BST=1 OR BST=5 OR BST=6 THEN BV=0
20660 END PROC
21000 ' ------------------------------------------------------------------------------------------------ John
21010 ' a two-bone limb: from the joint H to the end F, both bones L long, the middle joint bends towards the pole (PX,PY,PZ); the middle joint is left in KX,KY,KZ
21100 PROC IK(HX,HY,HZ,FX,FY,FZ,L,Z)
21110   LOCAL DX,DY,DZ,D,UX,UY,UZ,A,H,DP,NX,NY,NZ,NL
21120   DX=FX-HX:DY=FY-HY:DZ=FZ-HZ
21130   D=SQR(DX*DX+DY*DY+DZ*DZ)
21140   IF D>2*L-0.05 THEN D=2*L-0.05
21150   IF D<0.5 THEN D=0.5
21160   UX=DX/SQR(DX*DX+DY*DY+DZ*DZ+0.0001):UY=DY/SQR(DX*DX+DY*DY+DZ*DZ+0.0001):UZ=DZ/SQR(DX*DX+DY*DY+DZ*DZ+0.0001)
21170   A=D/2
21180   H=SQR(MAX(0,L*L-A*A))
21190   DP=PLX0*UX+PLY0*UY+PLZ0*UZ
21200   NX=PLX0-DP*UX:NY=PLY0-DP*UY:NZ=PLZ0-DP*UZ
21210   NL=SQR(NX*NX+NY*NY+NZ*NZ):IF NL<0.001 THEN NL=1
21220   KX=HX+A*UX+H*NX/NL:KY=HY+A*UY+H*NY/NL:KZ=HZ+A*UZ+H*NZ/NL
21230 END PROC
21300 ' a limb with a pole, drawn as two rods; TBT is the color of the upper, TBU of the lower bone
21400 PROC LIMB(O,HX,HY,HZ,FX,FY,FZ,L)
21410   CALL IK(HX,HY,HZ,FX,FY,FZ,L,0)
21420   CALL TUBE(O,HX,HY,HZ,KX,KY,KZ,LH)
21430   TBT=TBU
21440   CALL TUBE(O,KX,KY,KZ,FX,FY,FZ,LH*0.8)
21450 END PROC
21500 ' builds John into object 520: the body is in coordinates of the object (x forward, y left, z up; the origin on the ground below the hips when standing)
21510 ' pose 0: standing / walking (PH = the walk phase, AM = the stride 0 .. 1), pose 1: riding (in the coordinates of the bicycle; CA = the cranks, BS = the steering)
21600 PROC BUILDJOHN(POSE,PH,AM)
21610   LOCAL Q,SG,FX,FZ,HX,HZ,PX,PZ,A,HY,GX,GY,LN,HIPX,HIPZ,SHX,SHZ,GXL,GYL,HH,SWY,STY,BRS
21620   CC1=FN CC(5,6):CC2=FN CC(8,5):CC3=FN CC(14,11):CC4=FN CC(0,12):CC5=FN CC(0,14)
21630   ' the mesh cache: the body (object 520, without the head) depends on nothing but the pose, the walk phase, the stride, the speed (the sway), the pedals, the steering and the breath
21640   ' (rounded: the breath to a quarter of its range, the crank angle to 0.025 radians, the steering angle to 0.2 degrees), so when all of these are as at the last build the mesh is still right and is not built again (the object keeps its triangles)
21650   BRS=INT(SIN(BRP)*4+0.5)/4
21660   IF POSE=BJ1 AND PH=BJ2 AND AM=BJ3 AND INT(ABS(JV))=BJ4 AND INT(CA*40)=BJ5 AND INT(BSA*5)=BJ6 AND BRS=BJ7 THEN EXIT PROC
21670   BJ1=POSE:BJ2=PH:BJ3=AM:BJ4=INT(ABS(JV)):BJ5=INT(CA*40):BJ6=INT(BSA*5):BJ7=BRS:CNJ=CNJ+1
21680   M3D OBJECT 520,28,235
21685   IF POSE=2 THEN CALL BUILDLYING:EXIT PROC
21690   IF POSE=0 THEN
21700     SD=6+AM*12
21710     FOR Q=0 TO 1
21720       A=PH/(2*PI)+Q*0.5:A=A-INT(A)
21730       IF A<0.6 THEN
21740         GPX(Q)=SD-2*SD*A/0.6:GPZ(Q)=2.8:GPS(Q)=-1
21750       ELSE
21760         HZ=(A-0.6)/0.4:HY=HZ*HZ*(3-2*HZ):GPX(Q)=-SD+2*SD*HY:GPZ(Q)=2.8+MIN(1,AM*4)*5*SIN(PI*HZ):GPS(Q)=0
21770       END IF
21780     NEXT Q
21790     HIPZ=60
21800     FOR Q=0 TO 1
21810       IF GPS(Q)<>0 THEN
21820         HH=2.8+SQR(MAX(1,27.6*27.6-0.7744*GPX(Q)*GPX(Q)))
21830         IF HH<HIPZ THEN HIPZ=HH
21840       END IF
21850     NEXT Q
21860     HIPX=0:SHX=0:SHZ=HIPZ+16+0.35*BRS
21870     ' the sway: the pelvis moves sideways over the leg that carries the weight (that leg is on the ground from phase 0 to 0.6, its middle at 0.3), the upper body follows
21880     ' with a little over half of it, and each hip goes forward with its leg (the pelvis turns: 0.12 of the foot's distance)
21890     SWY=MIN(1,ABS(JV)/WSLOW)*(0.8+0.8*AM)*SIN(PH-0.314):STY=SWY*0.55
21900     ' the pelvis: a trouser colored block wider than the torso's waist, under it; the legs join it at the sides
21910     ' (its top is exactly the torso's bottom: 3.6 x 6.0 half size; it flares to 3.9 x 6.5 at the thighs; a belt covers the seam)
21920     F1X=3.9:F1Y=6.5:F2X=3.6:F2Y=6.0:FC1X=0:FC1Y=SWY:FC2X=0:FC2Y=SWY
21930     CALL FRUST(520,HIPZ-4.5,HIPZ+0.5,CC2)
21940     CALL BX(520,-3.75,-6.15+SWY,HIPZ-0.4,3.75,6.15+SWY,HIPZ+0.9,BELT)
21950     ' the torso narrows to the waist (3.6 x 6.0) and widens to the chest (4.4 x 7.4)
21960     F1X=3.6:F1Y=6.0:F2X=4.4*(1+0.03*BRS):F2Y=7.4*(1+0.03*BRS):FC1X=0:FC1Y=SWY:FC2X=0:FC2Y=STY
21970     CALL FRUST(520,HIPZ+0.5,SHZ+0.5,CC1)
21980     CALL COLLAR(520,0,STY,SHZ+0.5):HOX=0:HOY=STY:HOZ=SHZ+2.5
21990     FOR Q=0 TO 1
22000       SG=1:IF Q=1 THEN SG=-1
22010       FX=GPX(Q):FZ=GPZ(Q)
22020       PLX0=1:PLY0=0:PLZ0=0.1:LH=2.6:TBT=FN CC(8,5):TBU=TBT
22030       CALL LIMB(520,0.12*FX,SG*3.6+SWY,HIPZ,FX,SG*4,FZ,14)
22040       CALL SHOE(520,FX-3.5,SG*4,FZ-2.8)
22050       HX=-0.7*FX
22060       PLX0=-1:PLY0=SG*0.35:PLZ0=-0.1:LH=2.3:TBT=CC1:TBU=CC3
22070       CALL LIMB(520,0,SG*8.5+STY,SHZ-1.2,HX,SG*9.5+STY,19+2.8,11)
22080       ' the sleeve ends at the elbow with a cuff (a lighter green band)
22090       TBT=FN CC(5,10):CALL TUBE(520,KX+(0-KX)*0.22,KY+(SG*8.5+STY-KY)*0.22,KZ+(SHZ-1.2-KZ)*0.22,KX,KY,KZ,2.9)
22100       CALL HAND(520,HX,SG*9.5+STY,21.8,SG)
22110     NEXT Q
22120   ELSE
22130     HIPX=-9.5:HIPZ=32.4:SHX=-3:SHZ=46.4
22140     TBT=CC1
22150     CALL BX(520,HIPX-4.2,-6.8,HIPZ-3.5,HIPX+4.2,6.8,HIPZ+3.5,CC2)
22160     CALL TUBE(520,HIPX,3.4,HIPZ,SHX,3.4,SHZ,3.4):CALL TUBE(520,HIPX,-3.4,HIPZ,SHX,-3.4,SHZ,3.4)
22170     CALL COLLAR(520,SHX,0,SHZ+0.5):HOX=SHX:HOY=0:HOZ=SHZ+2.5
22180     FOR Q=0 TO 1
22190       SG=1:IF Q=1 THEN SG=-1
22200       A=-(CA+Q*PI)
22210       PX=-2+5.5*COS(A):PZ=9+5.5*SIN(A)
22220       PLX0=1:PLY0=0:PLZ0=0.15:LH=2.6:TBT=FN CC(8,5):TBU=TBT
22230       CALL LIMB(520,HIPX,SG*3.8,HIPZ,PX-0.5,SG*5.6,PZ+4.2,14.8)
22240       CALL SHOE(520,PX-3.5,SG*5.6,PZ+0.8)
22250       GXL=-2:GYL=SG*10
22260       GX=12+GXL*COS(BSA*PI/180)-GYL*SIN(BSA*PI/180)-12:GY=GXL*SIN(BSA*PI/180)+GYL*COS(BSA*PI/180)
22270       PLX0=-0.4:PLY0=SG*0.9:PLZ0=-0.3:LH=2.3:TBT=CC1:TBU=CC3
22280       CALL LIMB(520,SHX,SG*7,SHZ-1.5,12+GX,GY,31.8,11)
22290       TBT=FN CC(5,10):CALL TUBE(520,KX+(SHX-KX)*0.22,KY+(SG*7-KY)*0.22,KZ+(SHZ-1.5-KZ)*0.22,KX,KY,KZ,2.9)
22300       CALL FIST(520,12+GX,GY)
22310     NEXT Q
22320   END IF
22330 END PROC
22400 ' John lying on his back (he fell backwards: the head towards -x, the feet towards +x; the face looks up, HEADLOOK turns the head up): in the coordinates of the body, the
22401 ' ground at z = 0; HOX, HOY, HOZ put the chin at the end of the torso
22410 PROC BUILDLYING
22420   LOCAL Q,SG
22430   CALL BX(520,-4.5,-6.5,0.3,4.5,6.5,7.2,CC2)
22440   CALL BX(520,-17.5,-7.4,0.3,-4.4,7.4,8.4,CC1)
22450   FOR Q=0 TO 1
22460     SG=1:IF Q=1 THEN SG=-1
22470     TBT=CC2:CALL TUBE(520,2,SG*3.6,3.4,15,SG*4.4,3.2,2.6):CALL TUBE(520,15,SG*4.4,3.2,29,SG*5,3,2.1)
22480     CALL BX(520,28,SG*5-2.6,0.3,36,SG*5+2.6,5.2,CC4)
22490     TBT=CC1:CALL TUBE(520,-15,SG*8.5,5,-10,SG*11.5,2.6,2.6)
22500     TBT=CC3:CALL TUBE(520,-10,SG*11.5,2.6,-2,SG*10.5,2.2,2.2)
22510     CALL BX(520,-3,SG*10.5-1.7,0.4,2.5,SG*10.5+1.7,3.4,CC3)
22520   NEXT Q
22530   HOX=-18.5:HOY=0:HOZ=4.6
22540 END PROC
23000 ' ------------------------------------------------------------------------------------------------ the drones
23010 ' black, egg-shaped scouts with a life of their own (owner, 2026-11-25): they have no route. A drone cruises to a place it picks, then scans the ground there with a red beam,
23020 ' or hangs and looks round, or flies on. A drone belongs to no chunk: it may fly from one to the next as long as the chunk (DCS = 15625 units = 500 m) holds fewer than DCAP = 3
23030 ' drones (counting where they are and where they are going). The pool of NDR drones (6; G1987_DRONES = 0 .. 10) lives round John: one that gets farther than 17000 units away is made
23040 ' again on a ring of 13000 .. 15000 units, so the land seems full of drones. They are objects 600 + n (body), 610 + n (beam and its spot, rebuilt each frame), 620 + n (shadow).
23050 ' Modes DRM: 0 cruise to (DRTX,DRTY), half of the cruising drones scan on the way (DRSC), 1 scan (the beam on, hangs low), 2 look (hangs and turns), 3 found John, 4 beam on the camera (section 58000). Two drones never overlap (DRSEP).
23100 FUNC DCELL(X)
23110   RETURN INT(X/DCS)
23120 END FUNC
23200 ' how many other drones than K are in the chunk (CX,CY) or on their way to it
23300 FUNC DCOUNT(CX,CY,K)
23310   LOCAL J,C
23320   C=0
23330   FOR J=0 TO NDR-1
23340     IF J<>K THEN
23350       IF FN DCELL(DRX(J))=CX AND FN DCELL(DRY(J))=CY THEN
23360         C=C+1
23370       ELSE
23380         IF DRM(J)=0 THEN IF FN DCELL(DRTX(J))=CX AND FN DCELL(DRTY(J))=CY THEN C=C+1
23390       END IF
23400     END IF
23410   NEXT J
23420   RETURN C
23430 END FUNC
23500 ' the largest number of drones that are in one chunk (a test value)
23600 FUNC DMAXCELL()
23610   LOCAL J,C,M
23620   M=0
23630   FOR J=0 TO NDR-1
23640     C=FN DCOUNT(FN DCELL(DRX(J)),FN DCELL(DRY(J)),J)+1
23650     IF C>M THEN M=C
23660   NEXT J
23670   RETURN M
23680 END FUNC
23700 PROC BUILDDRONES
23710   LOCAL K,J,O
23720   NDR=6:GT=0:DCAP=3:DCS=15625:DINIT=-1:DPK=0:DCT=0:BL=0:UT=0:UTM=8:PROT=0:VLV=0:DFTS=0:DFIN=0:VT=-1:DSW=120:DSZ=90:CMF=0:CMFT=0:CMFV=0:CMFS=0:IF ENV$("G1987_VEILTEST")<>"" THEN VT=VAL(ENV$("G1987_VEILTEST"))
23730   IF ENV$("G1987_DRONES")<>"" THEN NDR=MAX(0,MIN(10,VAL(ENV$("G1987_DRONES"))))
23740   FOR K=0 TO NDR-1
23750     DRX(K)=JX+3000+K*400:DRY(K)=JY:DRH(K)=K*70:DRA(K)=300+((K*37) MOD 150):DRZ(K)=FN HT(DRX(K),DRY(K))+DRA(K):DRM(K)=2:DRTM(K)=1:DRBM(K)=0:DRV(K)=0:DRI(K)=0:DRW(K)=0:DRL(K)=0:DRSC(K)=0:DSX(K)=DRX(K):DSY(K)=DRY(K):DRTX(K)=DRX(K):DRTY(K)=DRY(K)
23760     O=600+K
23770     M3D OBJECT O,28,235
23780     LRR(0)=0.6:LRZ(0)=-30:LRV(0)=0:LRR(1)=13:LRZ(1)=-26:LRV(1)=0:LRR(2)=24:LRZ(2)=-12:LRV(2)=0:LRR(3)=29:LRZ(3)=6:LRV(3)=0
23790     LRR(4)=25:LRZ(4)=22:LRV(4)=0:LRR(5)=14:LRZ(5)=34:LRV(5)=0:LRR(6)=0.6:LRZ(6)=40:LRV(6)=0
23800     CALL LATHE(O,7,FN CC(0,1),2)
23810     LRR(0)=30:LRZ(0)=1:LRR(1)=30.2:LRZ(1)=5
23820     CALL LATHE(O,2,FN CC(0,3),2)
23830     LRR(0)=9:LRZ(0)=-26:LRR(1)=5:LRZ(1)=-31:LRR(2)=0.6:LRZ(2)=-32
23840     CALL LATHE(O,3,FN CC(1,15),2)
23850     CALL DRONEARMS(O,K)
23860     M3D OBJECT 610+K,62,255
23870     M3D OBJECT 620+K,62,255:CALL BX(620+K,-50,-50,0,50,50,0.3,FN CC(0,0))
23880   NEXT K
23890 END PROC
23900 ' the drone's armor and weapons, none of it symmetric: a crest of jagged blades on the back, four claws underneath (the fourth is shorter), a gun pod with a red muzzle on one side,
23910 ' a broken fin on the other, two small red eyes of different size, a bent antenna; K varies the blades from drone to drone
24000 PROC DRONEARMS(O,K)
24010   LOCAL I,A,B,R0,Z0,L,S
24020   FOR I=0 TO 7
24030     A=(I*45+K*23+(I MOD 3)*9)*PI/180:L=1+(I*5+K) MOD 3:S=0.3+0.12*(I MOD 2)
24040     R0=46:Z0=14+(I MOD 3)*6
24050     B=M3D("VERTS",O)
24060     M3D VERTEX O,R0*COS(A),R0*SIN(A),Z0:M3D VERTEX O,R0*COS(A+S),R0*SIN(A+S),Z0+8
24070     M3D VERTEX O,(R0+16+L*12)*COS(A+S*0.5),(R0+16+L*12)*SIN(A+S*0.5),Z0+22+L*16
24080     M3D FACE O,B,B+1,B+2,FN CC(0,2)
24090   NEXT I
24100   TBT=FN CC(0,1)
24110   FOR I=0 TO 3
24120     A=I*PI/2+0.5+K*0.3
24130     CALL TUBE(O,24*COS(A),24*SIN(A),-46,58*COS(A),58*SIN(A),-76,4)
24140     L=-102:IF I=3 THEN L=-86
24150     CALL TUBE(O,58*COS(A),58*SIN(A),-76,50*COS(A+0.25),50*SIN(A+0.25),L,2.6)
24160   NEXT I
24170   CALL BX(O,-16,46,-18,18,68,8,FN CC(0,1))
24180   TBT=FN CC(0,2)
24190   CALL TUBE(O,-6,58,-6,56,58,-6,5)
24200   CALL BX(O,56,54,-10,62,62,-2,FN CC(1,15))
24210   CALL BX(O,-10,56,8,12,70,12,FN CC(0,1))
24220   B=M3D("VERTS",O)
24230   M3D VERTEX O,-26,-50,2:M3D VERTEX O,10,-54,26:M3D VERTEX O,-38,-100,18
24240   M3D VERTEX O,14,-52,24:M3D VERTEX O,34,-50,-4:M3D VERTEX O,24,-98,-10
24250   M3D VERTEX O,-22,-52,-6:M3D VERTEX O,-6,-54,-24:M3D VERTEX O,-30,-84,-30
24260   M3D FACE O,B,B+1,B+2,FN CC(0,1):M3D FACE O,B+3,B+4,B+5,FN CC(0,2):M3D FACE O,B+6,B+7,B+8,FN CC(0,1)
24270   CALL BX(O,44,14,-6,53,24,3,FN CC(1,15)):CALL BX(O,50,-14,6,55,-8,12,FN CC(1,14))
24280   TBT=FN CC(0,2)
24290   CALL TUBE(O,0,0,76,10,-6,118,1.6):CALL TUBE(O,10,-6,118,2,-14,150,1.2)
24300   CALL BX(O,0,-17,148,5,-11,153,FN CC(1,15))
24310 END PROC
24400 ' a drone that lives: it makes a new place to go (DRPICK), arrives, and then scans, looks or flies on (DRNEXT); it keeps its height above the ground (and above the ground
24410 ' ahead: mountains), bobs, and does not enter a chunk that already holds DCAP drones
24500 PROC DRSPAWN(K,FIRST)
24510   LOCAL I,A,R,X,Y,OK
24520   I=0:OK=0
24530   WHILE NOT OK AND I<10
24540     A=RND*2*PI
24550     IF FIRST THEN R=2200+RND*9000 ELSE R=13000+RND*2000
24560     X=JX+R*COS(A):Y=JY+R*SIN(A)
24570     IF FN DCOUNT(FN DCELL(X),FN DCELL(Y),K)<DCAP THEN OK=-1
24580     I=I+1
24590   WEND
24600   DRX(K)=X:DRY(K)=Y:DRH(K)=RND*360:DRA(K)=300+RND*150:DRV(K)=60:DRBM(K)=0
24610   DRZ(K)=FN HT(X,Y)+DRA(K):DRM(K)=2:DRTM(K)=0.5:DRTX(K)=X:DRTY(K)=Y:DRSC(K)=0:DRI(K)=0:DRL(K)=0
24620 END PROC
24700 PROC DRPICK(K)
24710   LOCAL I,A,R,X,Y,OK,DX,DY
24720   I=0:OK=0
24730   WHILE NOT OK AND I<6
24740     A=DRH(K)+(RND-0.5)*140
24750     DX=JX-DRX(K):DY=JY-DRY(K)
24760     IF DX*DX+DY*DY>56250000 THEN A=ATAN2(DY,DX)*180/PI+(RND-0.5)*100
24770     R=1800+RND*4200
24780     X=DRX(K)+R*COS(A*PI/180):Y=DRY(K)+R*SIN(A*PI/180)
24790     IF FN DCOUNT(FN DCELL(X),FN DCELL(Y),K)<DCAP THEN OK=-1
24800     I=I+1
24810   WEND
24820   IF OK THEN
24830     DRTX(K)=X:DRTY(K)=Y:DRM(K)=0:DRSC(K)=0:IF RND<0.5 THEN DRSC(K)=-1
24840   ELSE
24850     DRM(K)=2:DRTM(K)=1.5+RND*2
24860   END IF
24870 END PROC
24900 ' arrived: scan the ground (70 %), look round (12 %) or fly on (cruising drones scan on the way too, half of them)
25000 PROC DRNEXT(K)
25010   LOCAL R
25020   R=RND
25030   IF R<0.7 THEN
25040     DRM(K)=1:DRTM(K)=7+RND*9
25050   ELSE
25060     IF R<0.82 THEN DRM(K)=2:DRTM(K)=3+RND*4 ELSE CALL DRPICK(K)
25070   END IF
25080 END PROC
25100 PROC DRONESIM(DT)
25110   LOCAL K,DX,DY,L,TA,GA,GB,TS,AL,NX,NY,BI,BF
25120   GT=GT+DT
25130   IF PROT>0 THEN PROT=PROT-DT
25140   IF DINIT THEN
25150     FOR K=0 TO NDR-1
25160       CALL DRSPAWN(K,-1)
25170     NEXT K
25180     DINIT=0
25190   END IF
25200   BI=0:BF=0
25210   FOR K=0 TO NDR-1
25220     DX=DRX(K)-JX:DY=DRY(K)-JY
25230     IF DX*DX+DY*DY>289000000 THEN CALL DRSPAWN(K,0)
25240     ' John near a drone: within 10 m it has found him, within 50 m it turns toward him (DRNEAR)
25250     IF DRM(K)<3 THEN CALL DRNEAR(K,DX*DX+DY*DY)
25260     TS=0:AL=DRA(K)
25270     IF DRM(K)>=3 THEN
25280       IF DRM(K)=3 THEN CALL DRFOCUS(K,DT) ELSE CALL DRCAM(K,DT)
25290       TS=DFTS:AL=190
25300       IF DRM(K)=3 AND DFIN THEN BF=-1:BI=MIN(1.5,BI+DRI(K))
25310     ELSE
25320       DSX(K)=DRX(K)+300*COS(GT*0.5+K*2):DSY(K)=DRY(K)+300*SIN(GT*0.7+K*2)
25330       IF DRM(K)=1 OR (DRM(K)=0 AND DRSC(K)) THEN CALL DRCATCH(K,DT) ELSE DRW(K)=0
25340       IF DRM(K)=0 THEN
25350         DX=DRTX(K)-DRX(K):DY=DRTY(K)-DRY(K):L=SQR(DX*DX+DY*DY)
25360         IF L<150 THEN
25370           CALL DRNEXT(K)
25380         ELSE
25390           TA=ATAN2(DY,DX)*180/PI
25400           DRH(K)=DRH(K)+FN ADIFF(TA,DRH(K))*MIN(1,1.1*DT)
25410           TS=110
25420         END IF
25430       ELSE
25440         IF DRM(K)<3 THEN
25450           DRTM(K)=DRTM(K)-DT
25460           IF DRM(K)=1 THEN
25470             TS=12:AL=170+((K*29) MOD 50):DRH(K)=DRH(K)+10*SIN(GT*0.3+K)*DT
25480           ELSE
25490             DRH(K)=DRH(K)+35*SIN(GT*0.5+K*1.7)*DT
25500           END IF
25510           IF DRTM(K)<=0 THEN CALL DRPICK(K)
25520         END IF
25530       END IF
25540     END IF
25550     IF DRM(K)=1 OR DRM(K)>=3 OR (DRM(K)=0 AND DRSC(K)) THEN
25560       DRBM(K)=MIN(1,DRBM(K)+1.2*DT):IF DRM(K)>=3 THEN DRBM(K)=MIN(1,DRBM(K)+1.8*DT)
25570     ELSE
25580       DRBM(K)=MAX(0,DRBM(K)-2*DT)
25590     END IF
25600     DRV(K)=DRV(K)+(TS-DRV(K))*MIN(1,0.8*DT)
25610     NX=DRX(K)+COS(DRH(K)*PI/180)*DRV(K)*DT:NY=DRY(K)+SIN(DRH(K)*PI/180)*DRV(K)*DT
25620     IF FN DCELL(NX)<>FN DCELL(DRX(K)) OR FN DCELL(NY)<>FN DCELL(DRY(K)) THEN
25630       IF FN DCOUNT(FN DCELL(NX),FN DCELL(NY),K)>=DCAP THEN
25640         NX=DRX(K):NY=DRY(K):DRV(K)=0:IF DRM(K)<3 THEN CALL DRPICK(K)
25650       END IF
25660     END IF
25670     DRX(K)=NX:DRY(K)=NY
25680     GA=FN HT(NX,NY):GB=FN HT(NX+COS(DRH(K)*PI/180)*700,NY+SIN(DRH(K)*PI/180)*700):IF GB>GA THEN GA=GB
25690     DRZ(K)=DRZ(K)+(GA+AL+12*SIN(GT*0.9+K)-DRZ(K))*MIN(1,1.2*DT)
25700   NEXT K
25710   CALL DRSEP(DT)
25720   DCT=DCT-DT:IF DCT<=0 THEN DCT=0.25:DPN=FN DMAXCELL():IF DPN>DPK THEN DPK=DPN
25730   ' the meter of the black-out: it fills while a drone's beam is on John (faster with two), and empties again when he is out of it
25740   IF MODE<>5 THEN
25750     IF BF THEN BL=MIN(1,BL+(0.2+0.6*BI)*DT) ELSE BL=MAX(0,BL-0.3*DT)
25760     IF BL>=1 AND (MODE=0 OR MODE=1) THEN CALL BLACKOUT
25770   END IF
25780 END PROC
25800 ' the height of what the beam's spot lies on: the ground or the top of anything standing on it (a roof, a fence, the bicycle): the drones study the surfaces of things
25810 FUNC BEAMZ(X,Y,Z)
25820   RETURN MAX(M3D("FLOORAT",X,Y,Z),FN HT(X,Y))+4
25830 END FUNC
26100 PROC DRONERIG
26110   LOCAL K,O,TX,TY,GZ,A,I,DX,DY,B,SC,Z0,C1,C2
26120   FOR K=0 TO NDR-1
26130     M3D OBJPOS 600+K,DRX(K),DRY(K),DRZ(K),GT*20+K*50
26140     M3D OBJPOS 620+K,DRX(K),DRY(K),FN HT(DRX(K),DRY(K))+3.5,0
26150     DX=DRX(K)-M3D("X"):DY=DRY(K)-M3D("Y")
26160     O=610+K
26170     M3D OBJECT O,62,255
26180     IF DX*DX+DY*DY<2600*2600 AND DRBM(K)>0.04 THEN
26190       SC=(0.35+0.65*DRBM(K))*(1+0.45*DRI(K))
26200       C1=FN CC(1,13):C2=FN CC(1,12):IF DRI(K)>0.5 THEN C1=FN CC(1,15):C2=FN CC(1,14)
26210       TX=DSX(K):TY=DSY(K):Z0=DRZ(K)-62
26220       GZ=FN BEAMZ(TX,TY,Z0)
26230       M3D VERTEX O,DRX(K),DRY(K),Z0
26240       FOR I=0 TO 3
26250         A=I*PI/2+GT
26260         M3D VERTEX O,TX+26*SC*COS(A),TY+26*SC*SIN(A),FN BEAMZ(TX+26*SC*COS(A),TY+26*SC*SIN(A),Z0)
26270       NEXT I
26280       FOR I=1 TO 4
26290         M3D FACE O,0,I,1+(I MOD 4),C1
26300       NEXT I
26310       B=M3D("VERTS",O)
26320       M3D VERTEX O,TX,TY,GZ+0.3
26330       FOR I=0 TO 7
26340         A=I*PI/4
26350         M3D VERTEX O,TX+48*SC*COS(A),TY+48*SC*SIN(A),FN BEAMZ(TX+48*SC*COS(A),TY+48*SC*SIN(A),Z0)+0.3
26360       NEXT I
26370       FOR I=0 TO 7
26380         M3D FACE O,B,B+1+I,B+1+((I+1) MOD 8),C2
26390       NEXT I
26395     END IF
26398   NEXT K
26399 END PROC
26500 ' two drones never overlap (owner, 2026-11-26): each has a box of 240 x 240 units (the width of a body, its claws and a margin) and 180 high; when two boxes meet (their centers closer than DSW on both
26510 ' horizontal axes and than DSZ vertically) the two are pushed apart along the axis of the smaller overlap, half each. They keep their height themselves, so only the horizontal axes are used
26520 PROC DRSEP(DT)
26530   LOCAL K,J,DX,DY,PX,PY,S
26540   FOR K=0 TO NDR-2
26550     FOR J=K+1 TO NDR-1
26560       IF ABS(DRZ(J)-DRZ(K))<DSZ THEN
26570         DX=DRX(J)-DRX(K):DY=DRY(J)-DRY(K)
26580         IF ABS(DX)<DSW AND ABS(DY)<DSW THEN
26590           IF DX=0 AND DY=0 THEN DX=COS(K*2.1+J):DY=SIN(K*2.1+J)
26600           PX=DSW-ABS(DX):PY=DSW-ABS(DY)
26610           IF PX<PY THEN
26620             S=SGN(DX)*PX*0.5*MIN(1,8*DT):DRX(J)=DRX(J)+S:DRX(K)=DRX(K)-S
26630           ELSE
26640             S=SGN(DY)*PY*0.5*MIN(1,8*DT):DRY(J)=DRY(J)+S:DRY(K)=DRY(K)-S
26650           END IF
26660         END IF
26670       END IF
26680     NEXT J
26690   NEXT K
26700 END PROC
27000 ' ------------------------------------------------------------------------------------------------ sound
27010 ' The drones are anti-gravity machines: no rotors, no engines, an electric hum (like a transformer: 100 Hz and its octave, a faint high whine) on the music channels 1 .. 4 (a held note each, slightly different pitches; VOLUME follows the distance to the camera,
27020 ' PITCH drifts a little), the tyres' rolling noise is a held noise on channel 5 (louder with speed, near silent on grass, sand roads, fields and wheat, a little quieter on hard ground); footsteps are effects, one at each heel strike
27030 ' of the walk cycle, by the ground (SFX 2 grass, 5 sand, 6 wood, 7 wheat); the freewheel ticks (SFX 3), the bell (SFX 1) and a thud (SFX 4) are as before.
27100 PROC SOUNDSTART
27110   LOCAL K
27120   INSTRUMENT 41,"NAME DRONEHUM; OSC SINE 0.6 1; OSC SINE 0.3 2 3; OSC SINE 0.035 -2350; ENV 0.8 0.1 1 0.5; GAIN 0.8"
27130   INSTRUMENT 42,"NAME TYRE; OSC NOISE 1; FILTER 3 0.1; ENV 0.2 0.1 1 0.3; GAIN 1.4"
27140   FOR K=0 TO 3
27150     WAVE K+1,41
27160     SOUND K+1,100+K*2.7,-1,0
27170   NEXT K
27180   INSTRUMENT 43,"NAME POLEHUM; OSC SINE 0.6 1; OSC SINE 0.35 2; OSC SINE 0.2 3; ENV 0.8 0.1 1 0.5; GAIN 0.8"
27190   WAVE 6,43
27200   SOUND 6,100,-1,0
27210   WAVE 5,42
27220   SOUND 5,400,-1,0
27230 END PROC
27300 ' Sounds that belong to a place (owner, 2026-11-25: "ambient sounds that grow louder if we are close to an object that emits a sound ... also the drones"): EMIT sets the
27310 ' volume of a channel from the distance between the listener (the camera) and the source (up and down: the square of 1 - distance / radius) and puts it left or right (PAN) by the
27320 ' direction. Used for the drones' hum (channels 1 .. 4: the four nearest drones, the others are too far to hear) and the telephone poles' hum (channel 6: the nearest pole).
27400 PROC EMIT(CH,EX,EY,EZ,RNG,AMP)
27410   LOCAL DX,DY,DZ,D,V,AG
27420   DX=EX-M3D("X"):DY=EY-M3D("Y"):DZ=EZ-M3D("Z")-46
27430   D=SQR(DX*DX+DY*DY+DZ*DZ):V=1-D/RNG:IF V<0 THEN V=0
27440   VOLUME CH,AMP*V*V
27450   AG=FN ADIFF(ATAN2(DY,DX)*180/PI,M3D("YAW")):PAN CH,-SIN(AG*PI/180)*MIN(1,D/400)
27460 END PROC
27500 PROC AMBIENT
27510   LOCAL K,C,B,DX,DY,DZ,D,S,TV,SF,BD
27520   FOR K=0 TO NDR-1
27530     DX=DRX(K)-M3D("X"):DY=DRY(K)-M3D("Y"):DZ=DRZ(K)-M3D("Z")-46
27540     DDD(K)=SQR(DX*DX+DY*DY+DZ*DZ):DRP(K)=0
27550   NEXT K
27560   FOR C=1 TO 4
27570     B=-1:BD=1000000000
27580     FOR K=0 TO NDR-1
27590       IF DRP(K)=0 AND DDD(K)<BD THEN BD=DDD(K):B=K
27600     NEXT K
27610     IF B<0 THEN
27620       VOLUME C,0
27630     ELSE
27640       DRP(B)=1
27650       CALL EMIT(C,DRX(B),DRY(B),DRZ(B),2600,0.8)
27660       PITCH C,(100+B*2.7)*(1+0.002*SIN(GT*0.9+B))
27670     END IF
27680   NEXT C
27690   BD=1000000000:B=-1
27700   FOR K=0 TO NPL-1
27710     DX=PLX(K)-M3D("X"):DY=PLY(K)-M3D("Y")
27720     D=DX*DX+DY*DY
27730     IF D<BD THEN BD=D:B=K
27740   NEXT K
27750   IF B>=0 AND BD<1000000 THEN CALL EMIT(6,PLX(B),PLY(B),FN HT(PLX(B),PLY(B))+130,900,0.5) ELSE VOLUME 6,0
27760   S=ABS(BV)/250:IF S>1 THEN S=1
27770   SF=FN SURF(BX,BY,BZ)
27780   TV=S*0.1
27790   IF SF=1 THEN TV=S*0.1
27800   IF SF=3 THEN TV=S*0.1
27810   IF SF=2 THEN TV=S*0.08
27820   DX=BX-JX:DY=BY-JY:D=SQR(DX*DX+DY*DY)
27830   IF D>600 THEN TV=0
27840   VOLUME 5,TV
27850 END PROC
27900 PROC SOUNDMUTE
27910   LOCAL K
27920   FOR K=1 TO 6
27930     VOLUME K,0
27940   NEXT K
27950 END PROC
28000 PROC FOOTSTEP
28010   LOCAL S
28020   S=FN SURF(JX,JY,JZ)
28030   IF S=0 THEN CALL SND(2)
28040   IF S=1 THEN CALL SND(5)
28050   IF S=2 THEN CALL SND(6)
28060   IF S=3 THEN CALL SND(7)
28070 END PROC
29000 ' ------------------------------------------------------------------------------------------------ the camera
29010 ' the camera agent advances by DT in equal steps of at most 1/57 s (a long frame behaves like several short ones: the spring and the rays see the same motion at any frame rate)
29100 PROC CAMSTEPS(DT)
29110   LOCAL CSN,CSK
29120   CSN=MAX(1,CEIL(DT*57))
29130   FOR CSK=1 TO CSN:CALL CAMUPDATE(DT/CSN):NEXT CSK
29140 END PROC
29200 PROC CAMUPDATE(DT)
29210   CALL CAMAGENT(DT)
29220 END PROC
30000 ' ------------------------------------------------------------------------------------------------ the physics
30100 PROC SIM(DT)
30110   LOCAL TV,A,DX,DY,TH,C,S,SL,TP,DM,OM,MX,AC,SD,DD,BT,TS,TL,DRN
30120   ' ---- the bicycle (also when nobody rides it: it rolls out)
30130   TH=0:IF MODE=1 THEN TH=KF:IF KF=0 AND KLT>0 THEN TH=KLT
30140   C=COS(BH*PI/180):S=SIN(BH*PI/180)
30150   AC=0:SPRN=0:DRN=0
30160   ' pedalling has a target speed BT, from WBMIN (a barely deflected stick or a light pull of the trigger) up to the top speed WBMAX (all the way): the stick's
30170   ' deflection and the left trigger (an alternate control: it also works alone) give it the same way, the larger wins; the forward key means plain pedalling WPED, with
30180   ' SHIFT the top speed. Thrust stops when the target is reached (a lower target is reached by coasting; the brake is A / S). A target above WPED is a sprint: it drains
30190   ' stamina in proportion (0.2 a second at the top speed); used up (EXH) the target is at most WPED. WBMIN = 40, WPED = 140 units a second
30200   IF TH>0 THEN
30210     BT=0
30220     IF KFD>0 THEN BT=WPED:IF KSH<>0 THEN BT=WBMAX
30230     IF KFA>0 THEN TS=WBMIN+(WBMAX-WBMIN)*MIN(1,KFA/0.95):IF TS>BT THEN BT=TS
30240     IF KLT>0 THEN TS=WBMIN+(WBMAX-WBMIN)*KLT:IF TS>BT THEN BT=TS
30250     IF EXH AND BT>WPED THEN BT=WPED
30260     IF BT>WPED THEN SPRN=-1:DRN=0.2*(BT-WPED)/(WBMAX-WPED)
30270     IF BV<BT THEN
30280       IF SPRN THEN AC=200*(1-BV/(1.25*WBMAX)) ELSE AC=78*(1-BV/190)
30290     END IF
30300     IF BV<0 THEN AC=AC+130
30310   END IF
30320   IF AC<0 THEN AC=0
30330   SL=(FN HT(BX+C*24,BY+S*24)-FN HT(BX,BY))/24
30340   BV=BV+AC*DT-260*SL*DT
30350   ' the top speed of the bicycle: 30 km/h, also downhill (a sprint on the flat ends up at about this speed by itself)
30360   IF BV>WBMAX THEN BV=WBMAX
30370   IF BV>0 THEN BV=BV-(9+0.05*BV)*DT
30380   IF BV<0 THEN BV=BV+(9-0.05*BV)*DT
30390   IF ABS(BV)<2 AND AC=0 AND ABS(SL)<0.02 AND NOT (MODE=1 AND KF<0) THEN BV=0
30400   IF MODE=1 AND (KB<>0 OR KF<0) THEN
30410     IF BV>0.5 THEN
30420       BV=BV-230*DT:IF BV<0 THEN BV=0
30430     ELSE
30440       BV=BV-110*DT:IF BV<-80 THEN BV=-80
30450     END IF
30460   END IF
30470   ' steering: the angle of the front wheel shrinks with the speed, the turning rate follows from the wheel base (36 units)
30480   TP=0:IF MODE=1 THEN TP=KT
30490   BS=BS+(TP-BS)*MIN(1,7*DT)
30500   DM=0.6/(1+(BV/95)^2)
30510   OM=BV/36*TAN(DM*BS)*180/PI:BSA=DM*BS*180/PI
30520   ' walking the bike round: at a walking pace (below 25 units a second) the steering also turns it on the spot, WSTAND degrees a second at a standstill (so a rider pinned against something can always turn away)
30530   IF MODE=1 THEN OM=OM+BS*WSTAND*MAX(0,1-ABS(BV)/25)
30540   BH=BH+OM*DT
30550   A=BH:IF BST=4 THEN A=BSD
30560   DX=COS(A*PI/180)*BV*DT:DY=SIN(A*PI/180)*BV*DT
30570   IF ABS(BV)>0.3 THEN
30580     M3D THINGMOVE 1,DX,DY
30590     IF M3D("HIT")<>0 THEN
30600       IF ABS(BV)>120 THEN
30610         CALL SND(4):IF MODE=1 THEN ST=MAX(0,ST-0.25):CALL SAY("OUCH. THAT WAS A WALL.")
30620         BV=-BV*0.2
30630       ELSE
30640         BV=BV*0.35
30650       END IF
30660     END IF
30670   END IF
30680   IF ABS(BV)>0.3 AND FN BLOCKED(M3D("TX",1),M3D("TY",1)) THEN M3D THINGPOS 1,BX,BY,BZ:BV=0
30690   BX=M3D("TX",1):BY=M3D("TY",1):BZ=M3D("TZ",1)
30700   IF ABS(BX)>WLIM OR ABS(BY)>WLIM THEN
30710     BX=MAX(-WLIM,MIN(WLIM,BX)):BY=MAX(-WLIM,MIN(WLIM,BY))
30720     M3D THINGPOS 1,BX,BY,BZ:BV=0
30730   END IF
30740   WA=WA+BV*DT/11
30750   IF TH>0 OR (MODE=1 AND BV<20 AND TH>0) THEN CA=CA+MAX(BV*DT/69*2*PI/2.3,TH*0.9*DT*3)
30760   IF TH<0 AND BV<-0.5 THEN CA=CA+BV*DT/69*2*PI/2.3
30770   IF WA>1000 THEN WA=WA-1000*PI
30780   IF CA>100 THEN CA=CA-100
30790   TK=TK+ABS(BV)*DT
30800   IF MODE<>1 AND BST=0 AND TK>70 AND ABS(BV)>25 THEN TK=0:CALL SND(3)
30810   IF MODE=1 AND TH<=0 AND TK>70 AND BV>25 THEN TK=0:CALL SND(3)
30820   CALL BIKEFALL(DT)
30830   ' ---- John
30840   IF MODE=0 THEN
30850     ' the target speed TV: a stick gives it by its deflection (just past the dead zone: WSLOW, fully pushed: WFAST); a key raises it from WSLOW to WFAST
30860     ' while it is held (WRAMP per second; SHIFT / the left trigger go straight to WFAST); letting go sets it to 0. John accelerates towards it (WACC),
30870     ' and slows down (WDEC) to a lower target or to a stop: no acceleration once it is reached.
30880     TV=0
30890     IF KFA>0 THEN
30900       TV=WSLOW+(WFAST-WSLOW)*MIN(1,KFA/0.95):KTS=0
30910     ELSE
30920       IF KFA<0 THEN
30930         TV=-WBACK*MIN(1,-KFA/0.95):KTS=0
30940       ELSE
30950         IF KFD>0 THEN
30960           IF KTS<WSLOW THEN KTS=WSLOW
30970           KTS=MIN(WFAST,KTS+WRAMP*DT):TV=KTS
30980         ELSE
30990           KTS=0:IF KFD<0 THEN TV=-WBACK
31000         END IF
31010       END IF
31020     END IF
31030     ' the trigger (or SHIFT) pulled a part of the way sets the target speed between WSLOW and WFAST while he means to go forward; the larger target wins
31040     IF (KFA>0 OR KFD>0 OR KLT>0) AND KRA>0 THEN
31050       TL=WSLOW+(WFAST-WSLOW)*KRA
31060       IF TL>TV THEN TV=TL
31070     END IF
31080     ' used up: only the slow walk until he has got his breath back
31090     IF EXH AND TV>WSLOW THEN TV=WSLOW:KTS=MIN(KTS,WSLOW)
31100     ACL=WDEC:IF ABS(TV)>ABS(JV) AND TV*JV>=0 THEN ACL=WACC
31110     JV=JV+SGN(TV-JV)*MIN(ABS(TV-JV),ACL*DT)
31120     ' fast walking tires: above WTIRE the drain grows to 0.1 a second at the top speed
31130     IF ABS(JV)>WTIRE THEN DRN=0.1*(ABS(JV)-WTIRE)/(WFAST-WTIRE)
31140     JH=JH+KT*(150-JV*0.25)*DT
31150     DX=COS(JH*PI/180)*JV*DT:DY=SIN(JH*PI/180)*JV*DT
31160     IF ABS(JV)>0.3 THEN
31170       M3D THINGMOVE 0,DX,DY
31180       ' a wall that stops him (he gets less than half of the way he meant to go) stops him dead (speed and the keys' target speed 0); sliding along a wall does not
31190       IF M3D("HIT")<>0 AND SQR((M3D("TX",0)-JX)^2+(M3D("TY",0)-JY)^2)<0.5*SQR(DX*DX+DY*DY) THEN JV=0:KTS=0
31200       IF FN BLOCKED(M3D("TX",0),M3D("TY",0)) THEN M3D THINGPOS 0,JX,JY,JZ:JV=0:KTS=0
31210       JX=M3D("TX",0):JY=M3D("TY",0):JZ=M3D("TZ",0)
31220       IF ABS(JX)>WLIM OR ABS(JY)>WLIM THEN
31230         JX=MAX(-WLIM,MIN(WLIM,JX)):JY=MAX(-WLIM,MIN(WLIM,JY))
31240         M3D THINGPOS 0,JX,JY,JZ:JV=0:KTS=0
31250       END IF
31260     ELSE
31270       JV=0
31280     END IF
31290     ' the stride (the distance of one step) grows with the speed from 10 units (about 30 cm) to 30 units (about 90 cm); a unit is about 3.2 cm (John is 58 units tall)
31300     PH=PH+JV*DT*PI/(10+20*FN SPD1(JV))
31310     NS=INT(PH/PI):IF NS<>NSL THEN NSL=NS:IF ABS(JV)>8 THEN CALL FOOTSTEP
31320     ' get on
31330     DX=BX-JX:DY=BY-JY
31340     NEAR=0:IF DX*DX+DY*DY<4900 THEN NEAR=-1
31350     IF KU<>0 AND NEAR AND (BST=0 OR BST=1 OR BST=5) THEN
31360       MODE=2:MT=0:MFX=JX:MFY=JY:MFZ=JZ:CALL SND(2)
31370       IF BST>0 THEN BST=6:BLN0=BLN:BSTR0=BSTR
31380     END IF
31390   END IF
31400   IF MODE=2 THEN
31410     MT=MT+DT
31420     A=MT/0.45:IF A>1 THEN A=1
31430     A=A*A*(3-2*A)
31440     JX=MFX+(BX-MFX)*A:JY=MFY+(BY-MFY)*A:JZ=MFZ+(BZ-MFZ)*A
31450     JH=JH+FN ADIFF(BH,JH)*MIN(1,12*DT)
31460     PH=PH+DT*7
31470     IF MT>=0.45 THEN MODE=1:JV=0:CALL SAY("W: PEDAL   SHIFT: SPRINT   S: BRAKE   E: GET OFF   B: BELL")
31480   END IF
31490   IF MODE=1 THEN
31500     JX=BX:JY=BY:JZ=BZ:JH=BH
31510     M3D THINGPOS 0,JX,JY,JZ
31520     IF KU<>0 THEN
31530       MODE=3:MT=0:MFX=BX:MFY=BY:MFZ=BZ
31540       DX=-SIN(BH*PI/180)*38:DY=COS(BH*PI/180)*38
31550       M3D THINGPOS 0,BX,BY,BZ:M3D THINGMOVE 0,DX,DY
31560       MGX=M3D("TX",0):MGY=M3D("TY",0):MGZ=M3D("TZ",0)
31570       M3D THINGPOS 0,MFX,MFY,MFZ:CALL SND(2)
31580     END IF
31590   END IF
31600   IF MODE=3 THEN
31610     MT=MT+DT
31620     A=MT/0.4:IF A>1 THEN A=1
31630     A=A*A*(3-2*A)
31640     JX=MFX+(MGX-MFX)*A:JY=MFY+(MGY-MFY)*A:JZ=MFZ+(MGZ-MFZ)*A
31650     PH=PH+DT*7
31660     IF MT>=0.4 THEN
31670       MODE=0:JV=0:M3D THINGPOS 0,MGX,MGY,MGZ:JX=MGX:JY=MGY:JZ=MGZ
31680     END IF
31690   END IF
31700   ' (after the stamina: breathing, about 0.25 breaths a second at rest, faster when out of breath or moving fast, and the head looking around) stamina: it drains while sprinting (bicycle) or walking fast, else 0.12 a second come back; at 0.02 he is used up, until it is back at 0.25
31710   IF DRN>0 THEN ST=MAX(0,ST-DRN*DT) ELSE ST=MIN(1,ST+0.12*DT)
31720   IF ST<=0.02 THEN EXH=-1
31730   IF ST>=0.25 THEN EXH=0
31740   BRP=BRP+DT*2*PI*(0.25+0.45*(1-ST)+0.25*MIN(1,ABS(JV)/WFAST)):IF BRP>1000 THEN BRP=BRP-1000*PI
31750   CALL DRONESIM(DT):CALL HEADLOOK(DT):CALL UNCON(DT)
31760   IF MSGT>0 THEN MSGT=MSGT-DT
31770 END PROC
32000 ' ------------------------------------------------------------------------------------------------ messages, input, the picture
32100 PROC SAY(T$)
32110   MSG$=T$:MSGT=5
32120 END PROC
32200 PROC READKEYS
32210   LOCAL U,B,K,L
32220   KF=0:KT=0:KFD=0
32230   IF KEY("UP") OR KEY("W") THEN KFD=KFD+1
32240   IF KEY("DOWN") OR KEY("S") THEN KFD=KFD-1
32250   KFA=-JOY("LY"):KF=KFD
32260   IF KEY("LEFT") OR KEY("A") THEN KT=KT+1
32270   IF KEY("RIGHT") OR KEY("D") THEN KT=KT-1
32280   KF=KF+KFA:KT=KT-JOY("LX")
32290   IF KF>1 THEN KF=1
32300   IF KF<-1 THEN KF=-1
32310   IF KT>1 THEN KT=1
32320   IF KT<-1 THEN KT=-1
32330   ' KSH: SHIFT; KLT: the left trigger, analog 0 .. 1 (the first 8 % and the last 7 % of the pull are cut off); KRA: the run / sprint pull, the larger of the two (SHIFT is all the way)
32340   KSH=0:IF KEY("SHIFT") THEN KSH=-1
32350   L=JOY("LT"):KLT=0
32360   IF L>0.08 THEN
32370     KLT=MIN(1,(L-0.08)/0.85)
32380   END IF
32390   KRA=KLT:IF KSH<>0 THEN KRA=1
32400   KR=0:IF KRA>0 THEN KR=-1
32410   KB=0:IF KEY("SPACE") OR JOY("A") THEN KB=-1
32420   U=0:IF KEY("E") OR KEY("RETURN") OR JOY("X") THEN U=-1
32430   KU=0:IF U<>0 AND UL=0 THEN KU=-1
32440   UL=U
32450   B=0:IF KEY("B") OR JOY("B") THEN B=-1
32460   KBL=0:IF B<>0 AND BKL=0 THEN KBL=-1
32470   BKL=B
32480   ' the Share button of a pad takes a screenshot (shot-YYYYMMDD-HHMMSS-n.bmp in the current folder), once per press
32490   SHB=0:IF JOY("SHARE") THEN SHB=-1
32500   IF SHB<>0 AND SHP=0 THEN IF HL=0 THEN SCREENSHOT
32510   SHP=SHB
32520   K=0
32530   IF KEY("Z") THEN K=K+1
32540   IF KEY("X") THEN K=K-1
32550   IF W3K_INVX THEN K=K+JOY("RX") ELSE K=K-JOY("RX")
32560   IF K<>0 THEN CO=CO+K*80*DT0
32570   IF KEY("R") THEN CLP=CLP+45*DT0
32580   IF KEY("F") THEN CLP=CLP-45*DT0
32590   IF KEY("PGUP") THEN CP=CP+40*DT0
32600   IF KEY("PGDN") THEN CP=CP-40*DT0
32610   IF W3K_INVY THEN CLP=CLP+JOY("RY")*40*DT0 ELSE CLP=CLP-JOY("RY")*40*DT0
32620   CLP=MAX(-5,MIN(45,CLP)):IF CP>45 THEN CP=45
32630   IF CP<-8 THEN CP=-8
32640   ' the right mouse button drags the camera
32650   IF (MOUSE("B") AND 2)<>0 THEN
32660     IF MB<>0 THEN
32670       CO=CO-(MOUSE("X")-MX)*0.35*(1+2*(W3K_INVX<>0)):CLP=CLP-(MOUSE("Y")-MY)*0.25*(1+2*(W3K_INVY<>0))
32680     END IF
32690     MB=-1
32700   ELSE
32710     MB=0
32720   END IF
32730   MX=MOUSE("X"):MY=MOUSE("Y")
32740   ' the camera controls are instructions to the camera (an agent, see CAMAGENT): CMIN says that one is given now; letting go lets the camera go back to its own business
32750   CMIN=0
32760   IF K<>0 OR KEY("R") OR KEY("F") OR KEY("PGUP") OR KEY("PGDN") OR JOY("RY")<>0 OR (MOUSE("B") AND 2)<>0 THEN CMIN=-1
32770   IF KBL<>0 THEN CALL SND(1)
32780 END PROC
32800 PROC HUD
32810   LOCAL I,S,X
32820   CALL W3K_HUDBAR(264,36)
32830   IF MODE=1 THEN
32840     GFX TEXT 8,269,"ON THE BICYCLE",FN CC(3,15)
32850     S=INT(ABS(BV)*0.1152)
32860     GFX TEXT 8,283,"SPEED "+STR$(S)+" KM/H",FN CC(0,13)
32870   ELSE
32880     IF MODE=5 THEN GFX TEXT 8,269,"UNCONSCIOUS",FN CC(1,15) ELSE GFX TEXT 8,269,"ON FOOT",FN CC(3,15)
32890     GFX TEXT 8,283,"JOHN",FN CC(0,13)
32900   END IF
32910   GFX TEXT 230,269,"STAMINA",FN CC(0,12)
32920   GFX LINE 230,284,339,293,FN CC(0,3),BF
32930   GFX LINE 232,286,232+INT(105*ST),291,FN CC(4,12),BF
32940   GFX TEXT 380,269,"1987",FN CC(2,13)
32950   GFX TEXT 380,283,"THE SKY IS QUIET",FN CC(0,9)
32955   GFX TEXT 424,269,FN CLOCK$(DAYH),FN CC(0,13)
32960   IF MODE=0 AND NEAR THEN
32970     GFX TEXT 8,22,"E: GET ON THE BICYCLE",FN CC(3,15)
32980   END IF
32990   IF MSGT>0 AND (MODE<>0 OR NOT NEAR) THEN
33000     GFX TEXT (640-LEN(MSG$)*8)\2+1,23,MSG$,FN CC(0,1)
33010     GFX TEXT (640-LEN(MSG$)*8)\2,22,MSG$,FN CC(3,15)
33020   END IF
33030   CALL CAMHUD
33040   IF SXY THEN CALL COORDS
33050   CALL COMPASS
33060 END PROC
33100 PROC RIGS
33110   CALL DRONERIG
33120   IF MODE=1 THEN
33130     CALL BUILDJOHN(1,PH,0)
33140     M3D OBJPOS 520,BX,BY,BZ,BH:CALL PLACEHEAD(BX,BY,BZ,BH)
33150   ELSE
33160     AM=FN SPD1(JV):IF MODE=3 OR MODE=2 THEN AM=0.4
33170     IF MODE=5 THEN CALL BUILDJOHN(2,0,0) ELSE CALL BUILDJOHN(0,PH,AM)
33180     M3D OBJPOS 520,JX,JY,JZ,JH:CALL PLACEHEAD(JX,JY,JZ,JH)
33190   END IF
33200   CALL PLACEBIKE
33210   M3D OBJPOS 531,BX,BY,FN FLR(BX,BY)+0.8,BH
33220   IF MODE=1 THEN
33230     M3D OBJSHOW 530,0
33240   ELSE
33250     M3D OBJSHOW 530,1:M3D OBJPOS 530,JX,JY,FN FLR(JX,JY)+0.8,JH
33260   END IF
33265   CALL VEIL
33270 END PROC
33300 PROC DRAWFRAME
33310   CALL CAMSTEPS(DTF)
33320   IF FL$<>"" THEN CALL FLPH(4)
33330   CALL RIGS
33340   IF FL$<>"" THEN CALL FLPH(5)
33350   M3D RENDER
33360   IF FL$<>"" THEN CALL FLPH(6)
33370   CALL HUD
33380   IF FL$<>"" THEN CALL FLPH(7)
33390   CALL W3K_SHOW
33400   IF FL$<>"" THEN CALL FLPH(8)
33410 END PROC
34000 ' ------------------------------------------------------------------------------------------------ the game
34100 PROC NEWGAME
34110   JX=0:JY=-172:JH=270:JV=0:CALL TERRAINJUMP(JX,JY):JZ=FN FLR(JX,JY)
34120   BX=130:BY=-214:BH=0:BV=0:BS=0:BSA=0:BZ=FN FLR(BX,BY):WA=0:CA=0:ST=1
34130   ' the bicycle leans against the south fence of the yard, on the outside, east of the gate, the handlebar turned towards the fence (bike state 1, see BIKEFALL)
34140   BST=1:BLN=0.35:BLS=1:BSTR=50:BRT=0:BYW=0:BSD=0:BDR=-1:BVIS=-1:BLW=0:BLN0=0:BSTR0=0:IF NOT NOLOAD THEN CALL WLOAD
34150   M3D THINGPOS 0,JX,JY,JZ:M3D THINGPOS 1,BX,BY,BZ
34160   CMINIT=0:CMB=0:CMT=0:CMIDLE=0:CMNEXT=7:CMHOLD=0:CMAZ=0:CMEL=0:CMIN=0:CMLASTB=0:CMPC=-1:DWI=0:ZOI=0:CDI=0:CZI=0:CZM=0:CLP=0:NSL=0:MODE=0:BL=0:UT=0:PROT=0:CMF=0:MT=0:PH=0:KTS=0:KFA=0:KFD=0:KRA=0:KSH=0:KLT=0:EXH=0:ST=1:STD=0:TK=0:CYW=JH:CO=0:CP=0:KU=0:UL=0:BL=0:NEAR=0:MSGT=0:MB=0
34170   CALL SAY("WALK WITH W A S D. THE BICYCLE LEANS AGAINST THE FENCE OUTSIDE THE GATE, TO THE EAST: STAND NEAR IT AND PRESS E.")
34180   MSGT=14
34190 END PROC
34200 PROC PLAYLOOP
34210   LOCAL T0,FR,Y,YL,ST0,FTL,FTS,FTI,FTD,FTC,FTK,FTT,FTR,FTA,FTN,FTP
34211   IF ENV$("G1987_LIETEST")<>"" THEN MODE=5:UT=1.9:UTM=100000
34212   IF ENV$("G1987_RIDE")<>"" THEN MODE=1:JX=BX:JY=BY:JZ=BZ:JH=BH:M3D THINGPOS 0,JX,JY,JZ
34220   VT$=ENV$("G1987_SHOT"):SHF=60:IF ENV$("G1987_SHOTFRAME")<>"" THEN SHF=VAL(ENV$("G1987_SHOTFRAME"))
34221   T0=TI:YL=0:NFR=0:FL$=ENV$("G1987_FRAMELOG"):FLN=0:FLN2=10000:IF ENV$("G1987_FRAMES")<>"" THEN FLN2=VAL(ENV$("G1987_FRAMES"))
34230   CALL SOUNDSTART:FTL=TIMER:FTC=GFX("FRAME"):FTP=1/60:FTA=0:FTN=0
34240   REPEAT
34250     ' the frame time. TI counts whole 1/60 s ticks (a frame showed 0, 1 or 2 of them by turns, so the time of a frame used to be the average of the last 32 frames, and the world moved by that
34260     ' average however long the frame really took); TIMER is the precise clock. But what the eye sees is the time between two DISPLAYS: a finished picture is shown at the next refresh
34270     ' of the screen (the screen thread counts them, GFX("FRAME"); W3K_SHOW waits for one), so the time between the starts of two frames carries the jitter of the render time and of the
34280     ' thread wake-up, which shows as a jump. The world advances by the number of refreshes that passed times the refresh period (measured over 90 refreshes: FTP); only where the count
34290     ' does not fit the clock (no refresh counted, another rate not measured yet, a pause) the real time is used. The physics and the camera agent advance in equal steps of at most 1/57 s
34300     NFR=NFR+1:T0=TI
34310     FTT=TIMER:FTR=FTT-FTL:FTL=FTT:FTK=GFX("FRAME")-FTC:FTC=FTC+FTK
34320     FTA=FTA+FTR:FTN=FTN+FTK:IF FTN>=90 AND FTA>0 THEN FTP=FTA/FTN:FTA=0:FTN=0
34330     DT0=FTR
34340     IF FTK>=1 AND FTP>0.004 AND FTP<0.04 THEN
34350       IF ABS(FTK*FTP-FTR)<0.6*FTP THEN DT0=FTK*FTP
34360     END IF
34370     IF DT0>0.1 THEN DT0=0.1
34380     IF DT0<0.001 THEN DT0=0.001
34390     IF FL$<>"" THEN CALL FRAMELOG
34400     IF FL$<>"" THEN CALL FLFOLD(FTK):CALL FLPH(0)
34410     DTF=DT0
34420     CALL READKEYS
34430     IF FL$<>"" THEN CALL FLPH(1)
34440     IF KEY("ESC") THEN IN_QUIT=1
34450     FTS=MAX(1,CEIL(DT0*57)):FTD=DT0/FTS
34460     FOR FTI=1 TO FTS
34470       CALL SIM(FTD)
34480     NEXT FTI
34490     IF FL$<>"" THEN CALL FLPH(2)
34500     CALL AMBIENT:CALL TERRAINUPDATE(JX,JY):CALL DAYNIGHT(DT0)
34510     IF FL$<>"" THEN CALL FLPH(3)
34520     CALL DRAWFRAME
34521     IF ENV$("G1987_CATCHTEST")<>"" AND NFR=VAL(ENV$("G1987_CATCHTEST")) THEN PROT=0:DRX(0)=JX+60:DRY(0)=JY:DRM(0)=3:DRI(0)=0:DRL(0)=0:DSX(0)=JX:DSY(0)=JY:DRBM(0)=1:DRZ(0)=FN HT(JX,JY)+190
34522     IF VT$<>"" AND NFR=SHF THEN SCREENSHOT VT$:IN_QUIT=1
34530     IF FN IN_SERVICE() THEN
34540       CALL SOUNDMUTE
34550       CALL PAUSEMENU
34560       T0=TI:W3K_T0=TI:NFR=0:FTL=TIMER:FTC=GFX("FRAME"):FTA=0:FTN=0
34570     END IF
34580     Y=JOY("Y")
34590     IF IN_KEY=79 OR (Y<>0 AND YL=0) THEN
34600       CALL P3_DIALOG
34610       T0=TI:NFR=0:FTL=TIMER:FTC=GFX("FRAME"):FTA=0:FTN=0
34620     END IF
34630     YL=Y
34640   UNTIL IN_QUIT
34650 END PROC
34700 ' the title: the camera circles John's house
34800 PROC TITLE
34810   LOCAL TG,A,T0,C,I,X$,Y,Q,K,H,X1,Y1,X2,Y2,QK,QL,QA,QX,QY,QU,QV,QF
34820   CALL LOADEND
34830   T0=TI:WNEW=0:IN_QUIT=0
34832   ' (owner, 2026-11-26) the key that confirmed the permission dialog of the save, or any other key still down or waiting, must not start the game again: wait until the keys are up, empty the
34834   ' key buffer, and take no key for the first half second
34836   WHILE KEY("SPACE") OR KEY("RETURN") OR KEY("ESC")
34837     GFX VSYNC
34838   WEND
34839   CALL IN_WAITRELEASE:TG=TI
34840   ' a still scene (no 3D, no world needed: it shows before the world is made or opened, and when a game is left) with a Qix: a line that moves, turns and changes its length, with the
34850   ' last 14 positions drawn as a fading trail in a slowly changing color (the old arcade game's mystery)
34860   QX=320:QY=150:QU=2.9:QV=2.1:QA=0:QL=100:QK=0:QF=0
34870   FOR K=0 TO 13
34880     QX1(K)=QX:QY1(K)=QY:QX2(K)=QX:QY2(K)=QY
34890   NEXT K
34900   M3D HIDE:CALL IN_WAITRELEASE
34910   REPEAT
34920     QX=QX+QU:QY=QY+QV
34930     IF QX<70 THEN QU=ABS(QU)
34940     IF QX>570 THEN QU=-ABS(QU)
34950     IF QY<50 THEN QV=ABS(QV)
34960     IF QY>250 THEN QV=-ABS(QV)
34970     QA=QA+0.045+0.03*SIN((TI-T0)/70):QL=120+80*SIN((TI-T0)/53):QF=QF+1
34980     IF QF MOD 3=0 THEN
34990       QK=(QK+1) MOD 14:QX1(QK)=QX+QL*COS(QA):QY1(QK)=QY+QL*SIN(QA)*0.8:QX2(QK)=QX-QL*COS(QA):QY2(QK)=QY-QL*SIN(QA)*0.8
35000     END IF
35010     GFX LINE 0,0,639,299,FN CC(0,0),BF
35020     H=1+INT((TI-T0)/45) MOD 14
35030     FOR K=0 TO 13
35040       I=(QK+1+K) MOD 14
35050       X1=MAX(0,MIN(639,QX1(I))):Y1=MAX(0,MIN(299,QY1(I))):X2=MAX(0,MIN(639,QX2(I))):Y2=MAX(0,MIN(299,QY2(I)))
35060       GFX LINE X1,Y1,X2,Y2,FN CC(H,2+K)
35070     NEXT K
35080     GFX LINE 0,205,639,222,FN CC(0,1),BF
35090     GFX LINE 0,244,639,261,FN CC(0,1),BF
35100     CALL BIGTEXT(66,"1987",FN CC(3,15),6)
35110     CALL BIGTEXT(150,"AFTER THE ARRIVAL",FN CC(0,13),2)
35120     GFX LINE 0,264,639,299,FN CC(0,1),BF
35130     CALL BIGTEXT(212,"THE SKY IS QUIET.  THE FIELD IS RIPE.  NOBODY IS LEFT TO HARVEST IT.",FN CC(0,13),1)
35140     IF WLD THEN
35150       IF EXISTS(WDIR$+"/world.dat") THEN X$="SPACE: CONTINUE      N: A NEW WORLD      Q: QUIT" ELSE X$="SPACE: MAKE THE WORLD AND START      Q: QUIT"
35160     ELSE
35170       X$="SPACE: START      Q: QUIT"
35180     END IF
35190     CALL BIGTEXT(250,X$,FN CC(0,13),1)
35200     IF (TI-T0) MOD 60<40 THEN CALL BIGTEXT(274,"PRESS SPACE OR RETURN",FN CC(3,15),1)
35210     CALL W3K_SHOW
35220     IF FN IN_SERVICE() THEN Q=Q
35230     C=0
35240     IF IN_KEY=32 OR IN_KEY=13 THEN C=-1
35250     IF WLD AND ( IN_KEY=78 OR IN_KEY=110 ) THEN C=-1:WNEW=-1
35260     IF KEY("SPACE") OR KEY("RETURN") OR JOY("A") OR JOY("START") THEN C=-1
35270     IF KEY("ESC") THEN IN_QUIT=1
35275     IF TI-TG<30 THEN C=0:WNEW=0:IN_QUIT=0
35280   UNTIL C OR IN_QUIT
35290   CALL IN_WAITRELEASE
35300 END PROC
36000 ' ------------------------------------------------------------------------------------------------ test mode
36100 PROC SPLIT(L$)
36110   LOCAL I,N,C$,CUR$
36120   FOR I=0 TO 7
36130     W$(I)=""
36140   NEXT I
36150   N=0:CUR$=""
36160   FOR I=1 TO LEN(L$)
36170     C$=MID$(L$,I,1)
36180     IF C$=" " THEN
36190       IF CUR$<>"" AND N<7 THEN W$(N)=CUR$:N=N+1
36200       CUR$=""
36210     ELSE
36220       CUR$=CUR$+C$
36230     END IF
36240   NEXT I
36250   IF CUR$<>"" AND N<7 THEN W$(N)=CUR$
36260 END PROC
36300 ' commands, one per line of the file G1987_PLAY: f v (forward -1..1), t v (turn), run v, brake v, use (get on / off), wait seconds, goto x y heading,
36310 ' bike x y heading, state (prints), shot name (a screenshot in the window host), say text, quit
36400 PROC PLAYBACK
36410   LOCAL L$,C$,I,N,PF,PT,PR,PB,T0,PSH,PRG,PCO
36420   PF=0:PFA=0:PT=0:PR=0:PB=0:PSH=0:PRG=0:PCO=0
36430   WHILE NOT EOF(4)
36440     LINE INPUT #4,L$
36450     CALL SPLIT(L$)
36460     C$=W$(0)
36470     IF C$="f" THEN PF=VAL(W$(1))
36480     IF C$="fa" THEN PFA=VAL(W$(1))
36490     IF C$="t" THEN PT=VAL(W$(1))
36500     IF C$="run" THEN PR=VAL(W$(1))
36510     IF C$="shift" THEN PSH=VAL(W$(1))
36520     IF C$="brake" THEN PB=VAL(W$(1))
36530     IF C$="use" THEN KU=-1
36540     IF C$="goto" THEN
36550       JX=VAL(W$(1)):JY=VAL(W$(2)):JH=VAL(W$(3)):CALL TERRAINJUMP(JX,JY):JZ=FN FLR(JX,JY):JV=0:M3D THINGPOS 0,JX,JY,JZ:CYW=JH:CMINIT=0
36560     END IF
36570     IF C$="bike" THEN
36580       BX=VAL(W$(1)):BY=VAL(W$(2)):BH=VAL(W$(3)):BZ=FN FLR(BX,BY):BV=0:BST=0:BRT=0:BLN=0:BDR=-1:M3D THINGPOS 1,BX,BY,BZ
36590     END IF
36600     IF C$="wait" THEN
36610       N=INT(VAL(W$(1))*60)
36620       FOR I=1 TO N
36630         KFD=PF:KFA=PFA:KF=MAX(-1,MIN(1,PF+PFA)):KT=PT:KLT=PR:KSH=0:IF PSH<>0 THEN KSH=-1
36640         KRA=KLT:IF KSH<>0 THEN KRA=1
36650         KR=0:IF KRA>0 THEN KR=-1
36660         KB=PB
36670         CMIN=0:IF PCO<>0 THEN CO=CO+PCO*80/60:CMIN=-1
36680         CALL SIM(1/60):CALL TERRAINUPDATE(JX,JY):CALL DAYNIGHT(1/60)
36690         IF PRG<>0 THEN CALL RIGS
36700         KU=0
36710         DTF=1/60
36720         CALL CAMUPDATE(1/60)
36730       NEXT I
36740     END IF
36750     IF C$="wsave" AND WLD THEN CALL WSAVE
36760     IF C$="reset" THEN CALL RESETPOS
36770     IF C$="coords" THEN SXY=VAL(W$(1)):CALL COORDTXT:PRINT "coords ";CT$
36780     IF C$="chunks" THEN CALL CHUNKSTAT(JX,JY)
36790     IF C$="sweep" THEN CALL SWEEPCAM(VAL(W$(1)),VAL(W$(2)))
36800     IF C$="state" THEN
36810       PRINT "state mode=";MODE;" bl=";INT(BL*100);" ut=";INT(UT);" jx=";INT(JX);" jy=";INT(JY);" jz=";INT(JZ+0.5);" jh=";INT(JH);" jv=";INT(JV);" bst=";BST;" nj=";CNJ;" nh=";CNH;" nb=";CNB;" cmb=";CMB;" hly=";INT(HLY);" hlp=";INT(HLP);" brp=";INT(BRP*100);" bx=";INT(BX);" by=";INT(BY);" bz=";INT(BZ+0.5);" bh=";INT(BH);" bv=";INT(BV);" st=";INT(ST*100);" gh=";INT(FN HT(JX,JY));" sf=";FN SURF(JX,JY,JZ);" cmf=";CMF;" cmg=";INT(CMZ-FN HT(CMX,CMY));" vlv=";VLV;" tod=";INT(DAYH*100);" amb=";INT(M3D("AMBIENT")*100+0.5);" lamp=";LAMPON;" dyn=";INT(DYN*100)
36820     END IF
36821     IF C$="catch" THEN TQ=VAL(W$(1)):PROT=0:DRX(TQ)=JX+VAL(W$(2)):DRY(TQ)=JY:DRM(TQ)=3:DRI(TQ)=0:DRL(TQ)=0:DSX(TQ)=JX:DSY(TQ)=JY:DRBM(TQ)=1:DRZ(TQ)=FN HT(JX,JY)+190
36822     IF C$="scanat" THEN PROT=0:DRM(0)=1:DRBM(0)=1:DRTM(0)=60:DRX(0)=JX-300*COS(GT*0.5):DRY(0)=JY-300*SIN(GT*0.7)
36823     IF C$="spotz" THEN PRINT "spotz z=";INT(FN BEAMZ(VAL(W$(1)),VAL(W$(2)),2000));" gz=";INT(FN HT(VAL(W$(1)),VAL(W$(2))))
36824     IF C$="hide" THEN FOR TQ=0 TO NDR-1:DRX(TQ)=JX+16500:DRY(TQ)=JY+TQ*300:DRM(TQ)=2:DRTM(TQ)=15:DRBM(TQ)=0:DRI(TQ)=0:NEXT TQ
36825     IF C$="above" THEN TQ=VAL(W$(1)):PROT=0:DRX(TQ)=JX+30:DRY(TQ)=JY:DRM(TQ)=0:DRSC(TQ)=0:DRBM(TQ)=0:DRTX(TQ)=JX+9000:DRTY(TQ)=JY:DRZ(TQ)=FN HT(JX,JY)+300
36826     IF C$="place" THEN TQ=VAL(W$(1)):DRX(TQ)=JX+VAL(W$(2)):DRY(TQ)=JY:DRM(TQ)=2:DRTM(TQ)=100:DRSC(TQ)=0:DRBM(TQ)=0:DRZ(TQ)=FN HT(JX,JY)+300:DRV(TQ)=0
36827     IF C$="prot" THEN PROT=VAL(W$(1))
36828     IF C$="dist" THEN TQ=VAL(W$(1)):PRINT "dist d=";INT(SQR((DRX(TQ)-JX)^2+(DRY(TQ)-JY)^2));" m=";DRM(TQ)
36829     IF C$="time" THEN DAYH=VAL(W$(1)):CALL DAYNIGHT(0)
36830     IF C$="ray" THEN
36840       M3D CAMERA VAL(W$(1)),VAL(W$(2)),VAL(W$(3))-46,VAL(W$(4)),VAL(W$(5))
36850       N=M3D("SHOOT",600)
36860       PRINT "ray kind=";M3D("SHOTKIND");" obj=";M3D("SHOTOBJ");" dist=";INT(M3D("SHOTDIST"))
36870     END IF
36880     IF C$="poles" THEN
36890       PH=0:PU=0
36900       FOR PK=0 TO NPL-1
36910         PGZ=FN HT(PLX(PK),PLY(PK)):M3D CAMERA PLX(PK)-60,PLY(PK),PGZ+100-46,0,0:P1=M3D("USE",600)
36920         M3D CAMERA PLX(PK)-60,PLY(PK),PGZ+170-46,0,0:IF P1=420 AND M3D("USE",600)<>420 THEN PH=PH+1
36930       NEXT PK
36940       PRINT "pole n=";NPL;" hit=";PH
36950     END IF
36960     IF C$="bench" AND NOT HL THEN
36970       N=INT(VAL(W$(1))):T0=TI
36980       FOR I=1 TO N
36990         CALL SIM(1/60):CALL DRAWFRAME
37000       NEXT I
37010       PRINT "bench";N;" frames, ms per frame";INT((TI-T0)*1000/60/N);" renderer";M3D("RENDERER");" drawn";M3D("DRAWN")
37020       OPEN "bench.txt" FOR APPEND AS #5:PRINT #5,"bench";N;" ms per frame";INT((TI-T0)*1000/60/N);" renderer";M3D("RENDERER");" drawn";M3D("DRAWN"):CLOSE #5
37030     END IF
37040     IF C$="drones" THEN TQ=VAL(W$(1)):PRINT "drone x=";INT(DRX(TQ));" y=";INT(DRY(TQ));" z=";INT(DRZ(TQ));" m=";DRM(TQ);" gz=";INT(FN HT(DRX(TQ),DRY(TQ)));" n=";NDR;" maxcell=";FN DMAXCELL();" peak=";DPK;" ri=";INT(DRI(TQ)*100);" sc=";DRSC(TQ)
37050     IF C$="clp" THEN CLP=VAL(W$(1)):CMINIT=0
37060     IF C$="pop" THEN CALL POPTEST(VAL(W$(1)),VAL(W$(2)))
37070     IF C$="zoom" THEN CZM=VAL(W$(1)):CMINIT=0
37080     IF C$="co" THEN CO=VAL(W$(1)):CYW=JH:CMINIT=0
37090     IF C$="cp" THEN CP=VAL(W$(1)):CMINIT=0
37100     IF C$="orbit" THEN PCO=VAL(W$(1))
37110     IF C$="look" THEN HLY=VAL(W$(1)):HLP=VAL(W$(2))
37120     IF C$="rigs" THEN PRG=VAL(W$(1))
37130     IF C$="playloop" THEN CALL PLAYLOOP
37140     IF C$="cam" THEN
37150       PRINT "ds=";INT(DSV)
37160       PRINT "cam x=";INT(M3D("X"));" y=";INT(M3D("Y"));" z=";INT(M3D("Z")+46);" yaw=";INT(M3D("YAW"));" pitch=";INT(M3D("PITCH"))
37170     END IF
37180     IF C$="shot" AND HL THEN CALL RIGS:CALL SOUNDSTART:CALL AMBIENT:CALL FOOTSTEP
37190     IF C$="shot" AND NOT HL THEN
37200       CALL DRAWFRAME
37210       CALL DRAWFRAME
37220       SCREENSHOT W$(1)
37230     END IF
37240     IF C$="quit" THEN EXIT PROC
37250   WEND
37260 END PROC
38000 ' ------------------------------------------------------------------------------------------------ main
38100 LABEL MAIN
38110 IN_QUIT=0
38120 ' walking: speeds in units per second (a unit is about 3.2 cm), accelerations in units per second squared
38130 WSLOW=22:WFAST=155:WBACK=38:WACC=150:WDEC=170:WRAMP=60:ACL=0
38140 ' the bicycle's top speed: 30 km/h = 8.33 m/s = 260 units/s at 3.2 cm a unit (the scale of docs/1987.md)
38150 WBMAX=30/3.6/0.032
38160 WPED=140:WTIRE=100:WBMIN=40:BELT=FN CC(12,3)
38170 IF NOT HL THEN CALL W3K_SETUP:CALL W3K_MESH:M3D RESET:CALL LOADING("PREPARING",0.02,""):CALL MAKETEX
38180 ' the limit John and the bicycle are kept inside (far away: the world is made of chunks as you go, see CHUNKS); TYW = -1: a tree gets a random yaw
38190 WLIM=50000:TYW=-1:WZ=-12:MTLIM=120:SLLIM=22:WSTAND=45
38200 ' the bicycle without a rider: it rolls on upright for WFALL seconds, then falls over and slides on its side (deceleration WSLIDE units a second squared); XON / XFR: tilt on
38210 BKL=0:CGI=0:CGT=0:CMX=0:CMY=0:CMZ=0:CMVX=0:CMVY=0:CMVZ=0:CMYW=0:CMPT=0:CMF=0:CMFT=0:CMFV=0:CMFS=0:CMWY=0:CMWP=0:CMPX=0:CMPY=0:CMPZ=0:CMPX0=0:CMPY0=0:CMPZ0=0:CMTVX=0:CMTVY=0:CMTVZ=0:CMPI=0:CMPK=0:CMDUR=0:CMAZR=0:HLY=0:HLP=0:BRP=0:HQY=0:HQP=0:HKEY=99999:HOX=0:HOY=0:HOZ=0:BJ1=-1:BJ2=0:BJ3=0:BJ4=0:BJ5=0:BJ6=0:BJ7=0:PWA=-99999:PCA=-99999:CNJ=0:CNH=0:CNB=0:HCX=0:HCY=0:HCZ=0:HYS=0:HYC=1:HPS=0:HPC=1:WFALL=3:WSLIDE=120:XON=0:XFR=0:XRS=0:XRC=1:XFS=0:XFC=1:XLF=0
38215 DYN=0:LENSON=0:DAYH=10:DAYSEC=1440:DAYLV=-1:LAMPON=0:DAYA=1:IF ENV$("G1987_TIME")<>"" THEN DAYH=VAL(ENV$("G1987_TIME"))
38216 IF ENV$("G1987_DAYLEN")<>"" THEN DAYSEC=MAX(10,VAL(ENV$("G1987_DAYLEN")))
38217 DYNF=-1:IF ENV$("G1987_DYNAMO")<>"" THEN DYNF=VAL(ENV$("G1987_DYNAMO"))
38220 CALL SETGROUND
38230 ' the sun: terrain cells get pictures shaded by their slope (M3D SUN: steps 301.. + 100, 200, 300; needs the palette's color map first)
38240 IF NOT HL THEN M3D COLORMAP
38250 M3D SUN -0.55,-0.35,0.75,0.55,4,100
38260 CALL WORLDDIR:WNEW=0:IF NOT HL AND ENV$("G1987_PLAY")="" AND ENV$("G1987_FRAMELOG")="" THEN CALL TITLE:IF IN_QUIT THEN GOTO BYEBYE
38270 CALL WORLDPREP:CALL LOADING("OPENING THE WORLD",0.1,""):CALL BUILDTERRAIN
38280 CALL LOADING("LAYING THE ROADS",0.25,""):CALL BUILDROADS
38290 CALL LOADING("PLANTING TREES",0.4,""):CALL BUILDTREES
38300 CALL LOADING("BUILDING THE HOUSE",0.55,""):CALL BUILDHOUSE
38310 CALL LOADING("FENCING THE YARD",0.65,""):CALL BUILDYARD
38320 CALL LOADING("SOWING THE WHEAT",0.75,""):CALL BUILDWHEAT
38330 CALL LOADING("OILING THE BICYCLE",0.85,""):CALL BUILDBIKE
38340 CALL LOADING("WAKING THE DRONES",0.95,""):CALL BUILDDRONES
38350 ' the drone stays on screen until the game starts (SESSION calls LOADEND): P3_LOAD and NEWGAME read files, which can raise a permission dialog
38360 M3D THING 0,0,0,0,299,30,56:M3D THING 1,0,0,0,299,35,40
38370 M3D PLAYER 14,56,46,24
38380 M3D FAR 160000:M3D FOG 156000,FN CC(8,14),100000
38390 IF NOT HL THEN
38400   CALL W3K_VIEW(0,0,640,264,2)
38410   CALL VEILPAL:M3D COLORMAP
38420   CALL P3_LOAD
38430   CALL LOADING("WAKING THE DRONES",0.99,"")
38440   SFX 1,"I12 V0.45 T200 ML O6 L16 C R32 C"
38450   SFX 2,"W3 V0.12 ML X0,0.004,0.25,0.03 H420,32"
38460   SFX 3,"W3 V0.05 ML X0,0.002,0.2,0.01 H3500,64"
38470   SFX 4,"I4 V0.35 ML X0.2,0.3,0.5,0.4 S80,150,20 S150,70,30"
38480   SFX 5,"W3 V0.16 ML X0,0.004,0.25,0.03 H1500,32 R64 H900,32"
38490   SFX 6,"I16 V0.3 ML O3 C32"
38500   SFX 7,"W3 V0.1 ML X0.01,0.06,0.3,0.06 H3200,16"
38510 END IF
38520 IF ENV$("G1987_PLAY")<>"" THEN OPEN ENV$("G1987_PLAY") FOR INPUT AS #4:CALL NEWGAME:DT0=1/60:CALL PLAYBACK:IF NOT HL THEN CALL W3K_DONE
38530 IF ENV$("G1987_PLAY")<>"" THEN BYE
38540 IF IN_QUIT=0 THEN CALL SESSION
38600 LABEL BYEBYE
38610 CALL W3K_DONE
38620 CALL TX_BYE("1987")
38630 END
38700 LABEL FAILED
38710 IF NOT HL THEN CALL W3K_DONE
38720 PRINT "Error: ";ERR$;" in line ";ERL
39000 ' ------------------------------------------------------------------------------------------------ John's head and torso
39010 ' a frustum (a box that is smaller at one end): the bottom z = Z1 with the half sizes F1X, F1Y round the centre (FC1X, FC1Y), the top z = Z2 with F2X, F2Y round (FC2X, FC2Y)
39100 PROC FRUST(O,Z1,Z2,TX)
39110   LOCAL B
39120   B=M3D("VERTS",O)
39130   M3D VERTEX O,FC1X-F1X,FC1Y-F1Y,Z1:M3D VERTEX O,FC1X+F1X,FC1Y-F1Y,Z1:M3D VERTEX O,FC1X+F1X,FC1Y+F1Y,Z1:M3D VERTEX O,FC1X-F1X,FC1Y+F1Y,Z1
39140   M3D VERTEX O,FC2X-F2X,FC2Y-F2Y,Z2:M3D VERTEX O,FC2X+F2X,FC2Y-F2Y,Z2:M3D VERTEX O,FC2X+F2X,FC2Y+F2Y,Z2:M3D VERTEX O,FC2X-F2X,FC2Y+F2Y,Z2
39150   M3D QUAD O,B,B+1,B+2,B+3,TX:M3D QUAD O,B+4,B+5,B+6,B+7,TX
39160   M3D QUAD O,B,B+1,B+5,B+4,TX:M3D QUAD O,B+1,B+2,B+6,B+5,TX
39170   M3D QUAD O,B+2,B+3,B+7,B+6,TX:M3D QUAD O,B+3,B,B+4,B+7,TX
39180 END PROC
39200 ' the radius of the head at the fraction T (0 chin .. 1 crown) of its height, from the profile in LRZ / LRR (set by HEAD)
39300 FUNC HPRF(T)
39310   LOCAL J
39320   IF T<=LRZ(0) THEN RETURN LRR(0)
39330   FOR J=1 TO 6
39340     IF T<=LRZ(J) THEN RETURN LRR(J-1)+(LRR(J)-LRR(J-1))*(T-LRZ(J-1))/(LRZ(J)-LRZ(J-1))
39350   NEXT J
39360   RETURN LRR(6)
39370 END FUNC
39400 ' the head: an egg standing on its narrow end (broad cranium, narrow chin; 10.5 high, 8.6 wide) with the chin at CZ, centred on (CX, CY), the face towards +x, turned by the yaw
39410 ' HQY and the pitch HQP (the head's yaw and pitch rounded to whole degrees; the pivot is the chin / neck): every vertex goes through HV. A cap of jagged gray hair that starts at the forehead in front, over the ears at the sides
39420 ' and at the nape at the back, ears, a neck (which does not turn), glasses with thin frames and temples, eyes (white with a dark pupil), eyebrows, a nose and a mouth
39500 PROC HV(O,X,Y,Z)
39510   LOCAL X1,Z1
39520   X1=X*HPC-Z*HPS:Z1=X*HPS+Z*HPC
39530   M3D VERTEX O,HCX+X1*HYC-Y*HYS,HCY+X1*HYS+Y*HYC,HCZ+Z1
39540 END PROC
39600 ' a box in the head's coordinates (it turns with the head, so it is built from six quads)
39700 PROC HBX(O,X1,Y1,Z1,X2,Y2,Z2,TX)
39710   LOCAL B
39720   B=M3D("VERTS",O)
39730   CALL HV(O,X1,Y1,Z1):CALL HV(O,X2,Y1,Z1):CALL HV(O,X2,Y2,Z1):CALL HV(O,X1,Y2,Z1)
39740   CALL HV(O,X1,Y1,Z2):CALL HV(O,X2,Y1,Z2):CALL HV(O,X2,Y2,Z2):CALL HV(O,X1,Y2,Z2)
39750   M3D QUAD O,B,B+1,B+2,B+3,TX:M3D QUAD O,B+4,B+5,B+6,B+7,TX
39760   M3D QUAD O,B,B+1,B+5,B+4,TX:M3D QUAD O,B+1,B+2,B+6,B+5,TX
39770   M3D QUAD O,B+2,B+3,B+7,B+6,TX:M3D QUAD O,B+3,B,B+4,B+7,TX
39780 END PROC
39800 PROC HEAD(O,CX,CY,CZ)
39810   LOCAL J,K,B,A,R,T,F,H,NS,SK,SK2,JG,SG,GL
39820   H=10.5:NS=12:SK=FN CC(14,11):SK2=FN CC(14,9):GL=FN CC(0,3)
39830   HCX=CX:HCY=CY:HCZ=CZ
39840   HYS=SIN(HQY*PI/180):HYC=COS(HQY*PI/180):HPS=SIN(HQP*PI/180):HPC=COS(HQP*PI/180)
39850   LRZ(0)=0:LRZ(1)=0.12:LRZ(2)=0.3:LRZ(3)=0.55:LRZ(4)=0.78:LRZ(5)=0.93:LRZ(6)=1
39860   LRR(0)=0.5:LRR(1)=2.6:LRR(2)=3.7:LRR(3)=4.3:LRR(4)=4.2:LRR(5)=3.1:LRR(6)=0.7
39870   B=M3D("VERTS",O)
39880   FOR J=0 TO 6
39890     FOR K=0 TO NS-1
39900       A=K*2*PI/NS
39910       CALL HV(O,LRR(J)*1.05*COS(A),LRR(J)*0.95*SIN(A),LRZ(J)*H)
39920     NEXT K
39930   NEXT J
39940   FOR J=0 TO 5
39950     FOR K=0 TO NS-1
39960       M3D QUAD O,B+J*NS+K,B+J*NS+(K+1) MOD NS,B+(J+1)*NS+(K+1) MOD NS,B+(J+1)*NS+K,SK
39970     NEXT K
39980   NEXT J
39990   ' the hair: five rings, the lowest one at a height that depends on the side and is ragged, the others pushed out by 0.55 and ragged in radius
40000   B=M3D("VERTS",O)
40010   FOR J=0 TO 4
40020     FOR K=0 TO NS-1
40030       A=K*2*PI/NS:F=(1-COS(A))/2:JG=((K*5) MOD 3)-1
40040       IF J=0 THEN T=0.70-0.40*F+0.06*JG ELSE T=0.70+0.085*J
40050       IF J=4 THEN T=1.02
40060       R=FN HPRF(MIN(T,1))+0.55
40070       IF J>=2 THEN R=R*(1+0.06*JG)
40080       IF J=4 THEN R=0.6
40090       CALL HV(O,R*1.05*COS(A),R*0.95*SIN(A),T*H)
40100     NEXT K
40110   NEXT J
40120   FOR J=0 TO 3
40130     FOR K=0 TO NS-1
40140       M3D QUAD O,B+J*NS+K,B+J*NS+(K+1) MOD NS,B+(J+1)*NS+(K+1) MOD NS,B+(J+1)*NS+K,CC4+(K MOD 2)*2
40150     NEXT K
40160   NEXT J
40170   ' the neck (it stays upright and faces the front) and the ears
40180   CALL BX(O,CX-1.8,CY-1.8,CZ-2.2,CX+1.8,CY+1.8,CZ+1.4,SK2)
40190   CALL HBX(O,-1,3.9,4.8,1.4,4.8,7.2,SK2):CALL HBX(O,-1,-4.8,4.8,1.4,-3.9,7.2,SK2)
40200   ' the face: eyes, eyebrows, nose, mouth
40210   CALL HBX(O,3.5,1.2,6.2,4.25,2.8,7.4,FN CC(0,15)):CALL HBX(O,3.5,-2.8,6.2,4.25,-1.2,7.4,FN CC(0,15))
40220   CALL HBX(O,4.0,1.7,6.4,4.35,2.5,7.2,FN CC(9,2)):CALL HBX(O,4.0,-2.5,6.4,4.35,-1.7,7.2,FN CC(9,2))
40230   CALL HBX(O,3.6,1,7.9,4.2,3.1,8.5,CC5):CALL HBX(O,3.6,-3.1,7.9,4.2,-1,8.5,CC5)
40240   CALL HBX(O,3.7,-0.8,3.5,5.3,0.8,5.8,SK2)
40250   CALL HBX(O,3.2,-1.7,2.2,3.85,1.7,2.9,FN CC(1,5))
40260   ' glasses: two thin rectangular rims, a bridge over the nose and the temples back to the ears
40270   FOR SG=-1 TO 1 STEP 2
40280     CALL HBX(O,4.3,SG*0.7-0.15,5.7,4.6,SG*3.4+0.15,5.95,GL):CALL HBX(O,4.3,SG*0.7-0.15,7.75,4.6,SG*3.4+0.15,8,GL)
40290     CALL HBX(O,4.3,SG*0.7-0.15,5.7,4.6,SG*0.7+0.15,8,GL):CALL HBX(O,4.3,SG*3.4-0.15,5.7,4.6,SG*3.4+0.15,8,GL)
40300     CALL HBX(O,0.8,SG*3.3-0.15,7.6,4.6,SG*3.3+0.15,7.9,GL)
40310   NEXT SG
40320   CALL HBX(O,4.3,-0.75,7.2,4.6,0.75,7.5,GL)
40330 END PROC
40400 ' the collar: a ring of eight flaps that stands up a little and folds out round the neck, open at the front (a V)
40500 PROC COLLAR(O,CX,CY,CZ)
40510   LOCAL K,B,A,R1,R2,C
40520   C=FN CC(5,12)
40530   B=M3D("VERTS",O)
40540   FOR K=0 TO 11
40550     A=(K+0.5)*2*PI/12
40560     M3D VERTEX O,CX+2.4*COS(A),CY+2.6*SIN(A),CZ
40570     M3D VERTEX O,CX+3.3*COS(A),CY+3.6*SIN(A),CZ+1.6
40580   NEXT K
40590   FOR K=0 TO 10
40600     M3D QUAD O,B+K*2,B+K*2+1,B+(K+1)*2+1,B+(K+1)*2,C
40610   NEXT K
40620 END PROC
40700 ' a hand at the wrist (WX, WY, WZ) hanging at the side, the palm to the thigh: a palm, four fingers (the middle one longest) curled a little forwards, a thumb; SG: 1 left, -1 right
40800 PROC HAND(O,WX,WY,WZ,SG)
40810   LOCAL I,L,FO
40820   TBT=FN CC(14,11)
40830   CALL BX(O,WX-1.3,WY-0.8,WZ-2.6,WX+1.3,WY+0.8,WZ+0.1,TBT)
40840   FOR I=0 TO 3
40850     FO=0.95-I*0.63:L=3.2:IF I=1 THEN L=3.6
40860     IF I=3 THEN L=2.6
40870     CALL TUBE(O,WX+FO,WY,WZ-2.6,WX+FO+0.5,WY,WZ-2.6-L,0.3)
40880   NEXT I
40890   CALL TUBE(O,WX+1.3,WY-SG*0.3,WZ-0.4,WX+2.4,WY-SG*0.3,WZ-3,0.38)
40900 END PROC
41000 ' a fist round the handlebar's grip at (GXX, GYY) (the grip's centre; the fingers wrap round its front)
41100 PROC FIST(O,GXX,GYY)
41110   LOCAL I
41120   CALL BX(O,GXX-1.6,GYY-1.6,31.2,GXX+1.4,GYY+1.6,33.4,FN CC(14,11))
41130   FOR I=0 TO 3
41140     CALL BX(O,GXX+1.4,GYY-1.5+I*0.8,31.2,GXX+2.5,GYY-0.7+I*0.8,33.3,FN CC(14,9))
41150   NEXT I
41160   CALL BX(O,GXX-1.2,GYY-0.5,33.4,GXX+1.2,GYY+0.5,34.3,FN CC(14,11))
41170 END PROC
41200 ' a shoe: the heel at XH, the middle of its width at YC, the underside of the sole at ZB; 9.5 long, a dark sole, a gray upper with a high ankle, a tongue and three white laces
41300 PROC SHOE(O,XH,YC,ZB)
41310   LOCAL I
41320   CALL BX(O,XH-0.2,YC-3.2,ZB,XH+9.7,YC+3.2,ZB+1,FN CC(0,4))
41330   CALL BX(O,XH,YC-2.9,ZB+1,XH+4.3,YC+2.9,ZB+4.8,FN CC(0,13))
41340   CALL BX(O,XH,YC-2.95,ZB+4.4,XH+4.3,YC+2.95,ZB+4.85,FN CC(0,7))
41350   CALL BX(O,XH+4.3,YC-2.8,ZB+1,XH+6.7,YC+2.8,ZB+3.9,FN CC(0,13))
41360   CALL BX(O,XH+6.7,YC-2.6,ZB+1,XH+9.5,YC+2.6,ZB+3,FN CC(0,13))
41370   CALL BX(O,XH+4.3,YC-1,ZB+3.9,XH+6.5,YC+1,ZB+4.2,FN CC(0,9))
41380   FOR I=0 TO 2
41390     CALL BX(O,XH+4.5+I*0.7,YC-1.8,ZB+3.9,XH+4.9+I*0.7,YC+1.8,ZB+4.3,FN CC(0,15))
41400   NEXT I
41410 END PROC
41500 ' the head looks at things: at the nearest drone (within 1400 units and not behind John) with the pitch up to it, else at the bicycle when it is near, else it leads into a turn; the yaw
41510 ' (HLY, degrees to the left, at most 80) and the pitch (HLP, at most 40 up, 15 down) follow the target smoothly
41600 PROC HEADLOOK(DT)
41610   LOCAL I,D,DX,DY,DZ,AG,TY,TP,BD,BI
41620   TY=KT*22:TP=0:BD=1000000
41630   FOR I=0 TO NDR-1
41640     DX=DRX(I)-JX:DY=DRY(I)-JY:D=SQR(DX*DX+DY*DY)
41650     AG=FN ADIFF(ATAN2(DY,DX)*180/PI,JH)
41660     IF D<1400 AND ABS(AG)<100 AND D<BD THEN
41670       BD=D:TY=MAX(-80,MIN(80,AG)):DZ=DRZ(I)-(JZ+54):TP=MAX(0,MIN(40,ATAN2(DZ,D)*180/PI))
41680     END IF
41690   NEXT I
41700   IF BD>=1000000 AND MODE=0 THEN
41710     DX=BX-JX:DY=BY-JY:D=SQR(DX*DX+DY*DY)
41720     AG=FN ADIFF(ATAN2(DY,DX)*180/PI,JH)
41730     IF D<260 AND ABS(AG)<110 THEN TY=MAX(-80,MIN(80,AG)):TP=-15
41740   END IF
41745   IF MODE=5 THEN TY=0:TP=88
41750   HLY=HLY+(TY-HLY)*MIN(1,5*DT):HLP=HLP+(TP-HLP)*MIN(1,5*DT)
41760 END PROC
41800 ' the head is its own object (521, built in its own coordinates with the chin at the origin) so that it is built again only when the rounded yaw / pitch change (the head
41810 ' moves with the body by its position and heading alone); HOX, HOY, HOZ: where the chin is in the body's coordinates (set by BUILDJOHN), OX, OY, OZ, OHD: the body's place
41900 PROC PLACEHEAD(OX,OY,OZ,OHD)
41910   LOCAL K,C,S
41920   HQY=INT(HLY+0.5):HQP=INT(HLP+0.5):K=HQY*1000+HQP
41930   IF K<>HKEY THEN
41940     HKEY=K:CNH=CNH+1
41950     M3D OBJECT 521,28,235:CALL HEAD(521,0,0,0)
41960   END IF
41970   C=COS(OHD*PI/180):S=SIN(OHD*PI/180)
41980   M3D OBJPOS 521,OX+C*HOX-S*HOY,OY+S*HOX+C*HOY,OZ+HOZ,OHD
41990 END PROC
42000 ' G1987_FRAMELOG=file: skips the title, plays 10000 frames (G1987_FRAMES=n changes the number) and writes how many ticks (1/60 s) the frames took (raw, as TI sees them) and the smallest, largest and mean frame time that the
42010 ' game used (the smoothed DT0) and the frames where the camera or John's height jumped (by more than 2 units / 1.5 degrees from one frame to the next; the first 12 with what changed), then quits: a way to see whether the frames come evenly and the picture quietly
42100 PROC FRAMELOG
42110   LOCAL D
42120   IF NFR<=40 THEN EXIT PROC
42130   D=0
42140   IF FLN=0 THEN FLN=1:FLMN=9:FLMX=0:FLS=0:FLC=0:FLT0=0:FLT1=0:FLT2=0:FLT3=0:FLK=TI:PD1=0:PD2=0:PD3=0:PD4=0:PD5=0:FLJ=0:FLQ=0:FL1=M3D("X"):FL2=M3D("Y"):FL3=M3D("Z"):FL4=M3D("YAW"):FL5=M3D("PITCH"):FL6=JZ
42150   ' jumps of the picture: a sudden change of the camera's movement from one frame to the next (the second difference of its position and angles above 1.5 units / 1.2 degrees),
42160   ' (John's own height steps are not counted: the camera follows them smoothly; the step is printed); turning or walking steadily does not count
42170   DX1=M3D("X")-FL1:DY1=M3D("Y")-FL2:DZ1=M3D("Z")-FL3:DA1=FN ADIFF(M3D("YAW"),FL4):DP1=M3D("PITCH")-FL5:DJ1=JZ-FL6
42180   IF FLC>1 AND (ABS(DX1-PD1)>1.5 OR ABS(DY1-PD2)>1.5 OR ABS(DZ1-PD3)>1.5 OR ABS(DA1-PD4)>1.2 OR ABS(DP1-PD5)>1.2) THEN
42190     FLJ=FLJ+1:IF FLJ<=80 THEN FLJ$(FLJ)="frame "+STR$(FLC)+": cam moved "+STR$(INT(DX1))+" "+STR$(INT(DY1))+" "+STR$(INT(DZ1))+" (before "+STR$(INT(PD1))+" "+STR$(INT(PD2))+" "+STR$(INT(PD3))+") yaw "+STR$(INT(DA1))+" (before "+STR$(INT(PD4))+") pitch "+STR$(INT(DP1))+" (before "+STR$(INT(PD5))+") John z "+STR$(INT(DJ1*10)/10)+" distance "+STR$(INT(DSV))+" turn input "+STR$(KT)+" forward "+STR$(KF)+" mode "+STR$(MODE)+" pulled "+STR$(CMPUL)+" behavior "+STR$(CMB)+" shot "+STR$(INT(CMSH))
42200   END IF
42210   PD1=DX1:PD2=DY1:PD3=DZ1:PD4=DA1:PD5=DP1:FL1=M3D("X"):FL2=M3D("Y"):FL3=M3D("Z"):FL4=M3D("YAW"):FL5=M3D("PITCH"):FL6=JZ
42220   D=TI-FLK:FLK=TI
42230   IF D=0 THEN FLT0=FLT0+1
42240   IF D=1 THEN FLT1=FLT1+1
42250   IF D=2 THEN FLT2=FLT2+1
42260   IF D>2 THEN FLT3=FLT3+1
42270   IF DT0<FLMN THEN FLMN=DT0
42280   IF DT0>FLMX THEN FLMX=DT0
42290   FLS=FLS+DT0:FLC=FLC+1
42300   IF FLC>=FLN2 THEN
42310     OPEN FL$ FOR OUTPUT AS #7
42320     PRINT #7,"ticks per frame: 0:";FLT0;" 1:";FLT1;" 2:";FLT2;" 3+:";FLT3
42330     PRINT #7,"frame time used: min ";FLMN;" max ";FLMX;" mean ";FLS/FLC
42340     PRINT #7,"jumps of the picture (a sudden change of the camera's movement): ";FLJ
42350     FOR FLQ=1 TO MIN(FLJ,80)
42360       PRINT #7,FLJ$(FLQ)
42370     NEXT FLQ
42380     PRINT #7,"phases of a frame in ms: mean, largest, mean in the frames that took 2 or more refreshes (";FPM;" of ";FPN;"):"
42390     FOR FLQ=0 TO 8
42400       PRINT #7,MID$("other   keys    SIM     ambient camera  rigs    RENDER  HUD     show    ",FLQ*8+1,8);": ";INT(FPS(FLQ)/MAX(1,FPN)*10000)/10;"  ";INT(FPX(FLQ)*10000)/10;"  ";INT(FPC(FLQ)/MAX(1,FPM)*10000)/10
42410     NEXT FLQ
42420     CLOSE #7
42430     IN_QUIT=1
42440   END IF
42450 END PROC
42500 ' the phases of a frame (only with G1987_FRAMELOG): FLPH(K) adds the time since the last mark to phase K of the running frame (0 the rest of the loop, 1 the keys, 2 SIM,
42510 ' 3 ambient sound and terrain, 4 camera, 5 the rigs, 6 M3D RENDER, 7 the HUD, 8 showing the picture and waiting for the display), FLFOLD(K) adds a finished frame that took K
42520 ' refreshes to the sums (the first 40 frames are left out)
42600 PROC FLPH(K)
42610   LOCAL FPT
42620   FPT=TIMER:FPB(K)=FPT-FLPT:FLPT=FPT
42630 END PROC
42700 PROC FLFOLD(K)
42710   LOCAL FPI
42720   IF NFR<=40 THEN EXIT PROC
42730   FPN=FPN+1:IF K>=2 THEN FPM=FPM+1
42740   FOR FPI=0 TO 8
42750     FPS(FPI)=FPS(FPI)+FPB(FPI):IF FPB(FPI)>FPX(FPI) THEN FPX(FPI)=FPB(FPI)
42760     IF K>=2 THEN FPC(FPI)=FPC(FPI)+FPB(FPI)
42770   NEXT FPI
42780 END PROC
43000 ' ------------------------------------------------------------------------------------------------ the camera: an agent
43010 ' The camera is a creature of its own (the owner's idea, 2026-11-15): it has a position, a velocity and an acceleration limit (a point mass chasing the place it wants to be at with a
43020 ' spring and a damper, plus the velocity of that place so that it does not lag), and a view direction with an angular velocity (the same kind of controller), and a behavior:
43030 ' CMB 0 follow: it goes to a place behind John and above him and looks at his head, the way it has always done (the azimuth follows his heading with a delay, CYW); 1 directed: the player
43040 ' uses the camera controls (orbit Z / X, the right stick, the right mouse button; look up / down R / F; raise / lower PGUP / PGDN), which are only instructions (CO the azimuth, CP the elevation,
43050 ' CLP the look pitch, as wished offsets): the camera carries them out with its own acceleration, a circling course round John; when they are let go it waits a second and then
43060 ' goes back to its own business, CO and the others die away; 2 circling (on its own, when the player has done nothing for a while): it circles round John, a little higher, watching him;
43070 ' 3 watching a point of interest (the bicycle when John is on foot near it, a drone, the house, the scarecrow, the gate): it looks at it for a few seconds. Any input of the player (also
43080 ' walking or riding) ends 2 and 3 at once. Whatever it does it keeps clear of walls: three rays from John's head to the wished place (straight and 9 degrees to each side) shorten the
43090 ' distance, and a ray from John's head to the camera itself pulls it in front of anything it would be behind. REC and the red glyphs (CAMHUD) show that it acts on its own.
43100 PROC CAMAGENT(DT)
43110   LOCAL QD,QV,TX,TY,TZ,TG,K,D,E,C,S,DS,R,PX,PY,PZ,EX,EY,EZ,AX,AY,AZ,SP,LX,LY,LZ,H,FZ,PM,YW,PT,ER,DX,DY,DZ,D2,HH,KP,KD
43120   IF DT<0.002 THEN EXIT PROC
43125   IF CMF>0 THEN CALL CAMFALL(DT):EXIT PROC
43130   IF CZI=0 OR ABS(JZ-CZS)>40 THEN CZS=JZ:CZI=-1
43140   CZS=CZS+(JZ-CZS)*MIN(1,9*DT)
43150   ZT=36:IF MODE=1 THEN ZT=40
43160   IF ZOI=0 THEN ZOS=ZT:ZOI=-1
43170   ZOS=ZOS+(ZT-ZOS)*MIN(1,5*DT)
43180   TX=JX:TY=JY:TZ=CZS+ZOS
43190   IF MODE=1 THEN TG=BH ELSE TG=JH
43200   K=3.4:IF MODE=1 THEN K=2.4
43210   CYW=CYW+FN ADIFF(TG,CYW)*MIN(1,K*DT)
43220   ' ---- what the player does, and what the camera does about it
43230   PM=0:IF KFA<>0 OR KFD<>0 OR KT<>0 OR KLT>0 OR KSH<>0 OR KB<>0 OR KU<>0 OR MODE>=2 THEN PM=-1
43240   IF CMIN<>0 THEN
43250     CMB=1:CMHOLD=1:CMIDLE=0
43260   ELSE
43270     IF CMB=1 THEN
43280       CMHOLD=CMHOLD-DT
43290       IF CMHOLD<=0 THEN CMB=0:CMT=0
43300     END IF
43310     IF PM THEN
43320       CMIDLE=0:IF CMB>=2 THEN CMB=0:CMT=0
43330     ELSE
43340       CMIDLE=CMIDLE+DT
43350     END IF
43360   END IF
43370   CMT=CMT+DT
43380   IF CMB=0 AND CMIDLE>CMNEXT THEN CALL CAMSTART
43390   IF CMB=0 THEN
43400     ' its own business: the wished offsets die away (the elevation only while John moves, as before)
43410     IF ABS(CO)>180 THEN CO=CO-INT((CO+180)/360)*360
43420     CO=CO*(1-MIN(1,1.1*DT)):CMAZ=CMAZ*(1-MIN(1,1.1*DT)):CMEL=CMEL*(1-MIN(1,1.1*DT))
43430     IF PM THEN CP=CP*(1-MIN(1,0.6*DT))
43440     CLP=CLP*(1-MIN(1,0.7*DT))
43450   END IF
43460   IF CMB=2 THEN
43470     CMAZ=CMAZ+CMAZR*DT:CMEL=CMEL+(9-CMEL)*MIN(1,1.5*DT)
43480     IF CMT>CMDUR THEN CMB=0:CMT=0:CMNEXT=CMIDLE+6+RND*6
43490   END IF
43500   IF CMB=3 THEN
43510     CMAZ=CMAZ*(1-MIN(1,0.8*DT)):CMEL=CMEL+(4-CMEL)*MIN(1,1.5*DT)
43520     IF CMPK=1 THEN CMPX=DRX(CMPI):CMPY=DRY(CMPI):CMPZ=DRZ(CMPI)
43530     IF CMT>CMDUR THEN CMB=0:CMT=0:CMNEXT=CMIDLE+6+RND*6
43540   END IF
43550   CP=MAX(-8,MIN(45,CP)):CLP=MAX(-5,MIN(45,CLP))
43560   ' ---- the place it wants to be at: behind John (as seen from where it looks), above him, as far as the walls allow
43570   YW=CYW+CO+CMAZ
43580   D=125:IF MODE=1 THEN D=155+MAX(0,BV)*0.18
43590   IF CZM>0 THEN D=CZM
43600   E=12+CP+CMEL
43610   C=COS(YW*PI/180):S=SIN(YW*PI/180)
43620   DS=D
43630   FOR K=-1 TO 1
43640     M3D CAMERA TX,TY,TZ-46,YW+180+K*9,E
43650     R=M3D("SHOOT",D+16)
43660     IF M3D("SHOTKIND")=1 THEN DS=MIN(DS,MAX(20,M3D("SHOTDIST")-12))
43670   NEXT K
43680   PX=TX-C*DS*COS(E*PI/180):PY=TY-S*DS*COS(E*PI/180):PZ=TZ+DS*SIN(E*PI/180)
43690   ' ---- where it wants to look: at John's head, or at a point of interest
43700   LX=TX:LY=TY:LZ=TZ
43710   IF CMB=3 THEN LX=CMPX:LY=CMPY:LZ=CMPZ
43720   ' ---- first time, or a teleport (a test command, a long fall): it is simply there
43730   EX=PX-CMX:EY=PY-CMY:EZ=PZ-CMZ
43740   IF CMINIT=0 OR EX*EX+EY*EY+EZ*EZ>490000 THEN
43750     CMX=PX:CMY=PY:CMZ=PZ:CMVX=0:CMVY=0:CMVZ=0:CMPX0=PX:CMPY0=PY:CMPZ0=PZ:CMTVX=0:CMTVY=0:CMTVZ=0
43760     DX=LX-CMX:DY=LY-CMY:DZ=LZ-CMZ:HH=SQR(DX*DX+DY*DY)
43770     CMYW=ATAN2(DY,DX)*180/PI:CMPT=ATAN2(DZ,HH)*180/PI+CLP:CMWY=0:CMWP=0:CMINIT=-1
43780     EX=0:EY=0:EZ=0
43790   END IF
43800   ' ---- movement: the velocity of the wished place is known (so a camera behind a bicycle does not lag), the rest is a spring and a damper; the acceleration and the speed are limited
43810   CMTVX=CMTVX+((PX-CMPX0)/DT-CMTVX)*MIN(1,12*DT):CMTVY=CMTVY+((PY-CMPY0)/DT-CMTVY)*MIN(1,12*DT):CMTVZ=CMTVZ+((PZ-CMPZ0)/DT-CMTVZ)*MIN(1,12*DT)
43820   CMPX0=PX:CMPY0=PY:CMPZ0=PZ
43830   ' (backward Euler: a frame longer than ~50 ms made the plain spring and damper ring from one frame to the next, the picture jumped between two views; this form is stable for any frame time and the same for short ones)
43840   QD=1+10.2*DT+26*DT*DT:QV=10.2+26*DT
43850   AX=(26*EX+10.2*CMTVX-QV*CMVX)/QD:AY=(26*EY+10.2*CMTVY-QV*CMVY)/QD:AZ=(26*EZ+10.2*CMTVZ-QV*CMVZ)/QD
43860   SP=SQR(AX*AX+AY*AY+AZ*AZ):IF SP>1800 THEN AX=AX*1800/SP:AY=AY*1800/SP:AZ=AZ*1800/SP
43870   CMVX=CMVX+AX*DT:CMVY=CMVY+AY*DT:CMVZ=CMVZ+AZ*DT
43880   SP=SQR(CMVX*CMVX+CMVY*CMVY+CMVZ*CMVZ):IF SP>900 THEN CMVX=CMVX*900/SP:CMVY=CMVY*900/SP:CMVZ=CMVZ*900/SP
43890   CMX=CMX+CMVX*DT:CMY=CMY+CMVY*DT:CMZ=CMZ+CMVZ*DT
43900   ' ---- it never gets behind anything: a ray from John's head to the camera
43910   DX=CMX-TX:DY=CMY-TY:DZ=CMZ-TZ:D2=SQR(DX*DX+DY*DY+DZ*DZ):HH=SQR(DX*DX+DY*DY)
43920   IF D2>30 THEN
43930     M3D CAMERA TX,TY,TZ-46,ATAN2(DY,DX)*180/PI,ATAN2(DZ,HH)*180/PI
43940     R=M3D("SHOOT",D2+4):CMPUL=0:CMSH=M3D("SHOTDIST")-D2
43950     IF M3D("SHOTKIND")=1 THEN
43960       IF M3D("SHOTDIST")<D2+2 THEN
43970         CMPUL=-1:D=MAX(20,M3D("SHOTDIST")-12):CMX=TX+DX/D2*D:CMY=TY+DY/D2*D:CMZ=TZ+DZ/D2*D:CMVX=CMVX*0.3:CMVY=CMVY*0.3:CMVZ=CMVZ*0.3
43980       END IF
43990     END IF
44000   END IF
44010   FZ=MAX(M3D("FLOORAT",CMX,CMY,CMZ),FN HT(CMX,CMY))+10
44020   IF CMZ<FZ THEN
44030     CMZ=FZ:IF CMVZ<0 THEN CMVZ=0
44040   END IF
44050   ' ---- looking: the same kind of controller for the yaw and the pitch (degrees, degrees a second)
44060   DX=LX-CMX:DY=LY-CMY:DZ=LZ-CMZ:HH=SQR(DX*DX+DY*DY)
44070   YW=ATAN2(DY,DX)*180/PI:PT=ATAN2(DZ,HH)*180/PI+CLP
44080   ' (a lazier look, a glance, when it turns to a point of interest)
44090   KP=90:KD=19:IF CMB=3 THEN KP=28:KD=10.6
44100   QD=1+KD*DT+KP*DT*DT:ER=FN ADIFF(YW,CMYW):CMWY=(CMWY+KP*ER*DT)/QD
44110   IF ABS(CMWY)>540 THEN CMWY=SGN(CMWY)*540
44120   CMYW=CMYW+CMWY*DT
44130   ER=PT-CMPT:CMWP=(CMWP+KP*ER*DT)/QD
44140   IF ABS(CMWP)>360 THEN CMWP=SGN(CMWP)*360
44150   CMPT=CMPT+CMWP*DT
44160   M3D CAMERA CMX,CMY,CMZ-46,CMYW,CMPT
44170   DSV=SQR((CMX-TX)^2+(CMY-TY)^2+(CMZ-TZ)^2)
44180 END PROC
44200 ' the camera decides to do something on its own: watch a point of interest (the bicycle when John is on foot and near it, the nearest drone, the house, the scarecrow, the gate: the next
44210 ' one that is of use, round and round) or, after one, circle round John
44300 PROC CAMSTART
44310   LOCAL T,DX,DY,D,BD,I,OK
44320   IF CMLASTB=3 AND RND<0.6 THEN
44330     OK=0
44340   ELSE
44350     OK=0:T=0
44360     WHILE T<5 AND OK=0
44370       T=T+1:CMPC=(CMPC+1) MOD 5
44380       IF CMPC=0 AND MODE=0 THEN
44390         DX=BX-JX:DY=BY-JY
44400         IF DX*DX+DY*DY<810000 THEN OK=-1:CMPK=0:CMPX=BX:CMPY=BY:CMPZ=BZ+18
44410       END IF
44420       IF CMPC=1 THEN
44430         BD=1800:CMPI=-1
44440         FOR I=0 TO NDR-1
44450           DX=DRX(I)-JX:DY=DRY(I)-JY:D=SQR(DX*DX+DY*DY)
44460           IF D<BD THEN BD=D:CMPI=I
44470         NEXT I
44480         IF CMPI>=0 THEN OK=-1:CMPK=1:CMPX=DRX(CMPI):CMPY=DRY(CMPI):CMPZ=DRZ(CMPI)
44490       END IF
44500       IF CMPC=2 THEN
44510         IF JX*JX+JY*JY<1210000 THEN OK=-1:CMPK=0:CMPX=0:CMPY=0:CMPZ=90
44520       END IF
44530       IF CMPC=3 THEN
44540         DX=1100-JX:DY=650-JY
44550         IF DX*DX+DY*DY<1690000 THEN OK=-1:CMPK=0:CMPX=1100:CMPY=650:CMPZ=45
44560       END IF
44570       IF CMPC=4 THEN
44580         DX=-JX:DY=-200-JY
44590         IF DX*DX+DY*DY<1000000 THEN OK=-1:CMPK=0:CMPX=0:CMPY=-200:CMPZ=40
44600       END IF
44610     WEND
44620   END IF
44630   IF OK THEN
44640     CMB=3:CMDUR=4+RND*2.5:CMLASTB=3
44650   ELSE
44660     CMB=2:CMDUR=8+RND*4:CMAZR=14+RND*10:IF RND<0.5 THEN CMAZR=-CMAZR
44670     CMLASTB=2
44680   END IF
44690   CMT=0
44700 END PROC
44800 ' the red glyphs of an alien script: a glyph is three to five strokes between points of a 7 x 10 grid, each one a stroke after the other from where the last one ended or from a new point
44900 PROC NEWGLYPH(G)
44910   LOCAL N,S,X0,Y0
44920   N=3+INT(RND*3):GNS(G)=N:X0=INT(RND*7):Y0=INT(RND*10)
44930   FOR S=0 TO N-1
44940     GGX(G*20+S*4)=X0:GGX(G*20+S*4+1)=Y0
44950     IF RND<0.5 THEN X0=INT(RND*7) ELSE Y0=INT(RND*10)
44960     IF RND<0.35 THEN X0=INT(RND*7):Y0=INT(RND*10)
44970     GGX(G*20+S*4+2)=X0:GGX(G*20+S*4+3)=Y0
44980   NEXT S
44990 END PROC
45000 ' the camera shows that it records (owner, 2026-11-26: also while the player only gives it instructions): a red dot that blinks with REC in the upper left corner, and in the lower right corner a row of red
45010 ' glyphs of an alien script, one of which is made new from time to time (more often when the camera is busy with something)
45100 PROC CAMHUD
45110   LOCAL G,S,X,Y,C1,C2
45115   ' the camera is out (CMF 1, 2): no recording light, no glyphs; they come back when it comes to
45116   IF CMF>0 THEN EXIT PROC
45130   IF CGI=0 THEN
45140     FOR G=0 TO 6
45150       CALL NEWGLYPH(G)
45160     NEXT G
45170     CGI=-1:CGT=1
45180   END IF
45190   CGT=CGT-DTF
45200   IF CGT<=0 THEN
45210     CALL NEWGLYPH(INT(RND*7)):CGT=0.25+RND*1.6
45220     IF CMB>=2 THEN CGT=0.08+RND*0.5
45230   END IF
45240   C1=FN CC(1,13):C2=FN CC(1,8)
45250   IF (TI MOD 72)<48 THEN
45260     GFX LINE 9,8,19,12,C1,BF:GFX LINE 10,6,18,14,C1,BF:GFX LINE 12,5,16,15,C1,BF
45270   END IF
45280   GFX TEXT 24,6,"REC",C1
45290   FOR G=0 TO 6
45300     X=546+G*12:Y=271
45310     FOR S=0 TO GNS(G)-1
45320       GFX LINE X+GGX(G*20+S*4)+1,Y+GGX(G*20+S*4+1)+1,X+GGX(G*20+S*4+2)+1,Y+GGX(G*20+S*4+3)+1,C2
45330       GFX LINE X+GGX(G*20+S*4),Y+GGX(G*20+S*4+1),X+GGX(G*20+S*4+2),Y+GGX(G*20+S*4+3),C1
45340     NEXT S
45350   NEXT G
45360 END PROC
45400 ' the rock of the mountains (picture 324) and the water of the lakes (325: the water plane is translucent, so it tints what lies under it)
45500 PROC MKROCK
45510   LOCAL I,X,Y
45520   GFX CLEAR FN CC(0,5)
45530   FOR I=1 TO 40
45540     X=FN RN(56):Y=FN RN(56)
45550     GFX LINE X,Y,X+4+FN RN(12),Y+4+FN RN(12),FN CC(0,3+FN RN(7)),BF
45560   NEXT I
45570   FOR I=1 TO 30
45580     X=FN RN(60):Y=FN RN(60)
45590     GFX LINE X,Y,X+FN RN(10)-5,Y+FN RN(10)-5,FN CC(0,2)
45600   NEXT I
45610   CALL W3K_GRAB(324,64,64)
45620 END PROC
45700 PROC MKWATER
45710   LOCAL I,X,Y
45720   GFX CLEAR FN CC(8,9)
45730   FOR I=1 TO 40
45740     X=FN RN(56):Y=FN RN(64)
45750     GFX LINE X,Y,X+4+FN RN(8),Y,FN CC(8,11+FN RN(4))
45760   NEXT I
45770   CALL W3K_GRAB(325,64,64)
45780 END PROC
45800 ' too high or too steep for a ruin, a road or a bale (MTLIM: 120 units high, SLLIM: 22 units of height difference over 20 units: about 48 degrees) or too deep (12 below the water level WZ): the old test of "BLOCKED", now only used to place things
45900 FUNC ROUGH(X,Y)
45910   LOCAL H,S
45920   H=FN HT(X,Y)
45930   IF H>MTLIM OR H<WZ-12 THEN RETURN -1
45940   S=ABS(FN HT(X+20,Y)-H)+ABS(FN HT(X,Y+20)-H)
45950   RETURN S>SLLIM
45960 END FUNC
46000 ' ------------------------------------------------------------------------------------------------ city ruins, roads, hay bales of the wild world (roadmap item 95)
46010 ' A city ruin site: big regions of 7200 units (8 chunks) each have one with a chance of 30 %, at a hashed place with a radius of 450 .. 900 units; the ground inside is cut into lots of 180 units,
46020 ' 70 % of them hold a ruin (up to four brick walls of 8 units thickness, each there with 78 %, broken off at 30 .. 160 units, taller towards the middle, and three heaps of rubble);
46030 ' the lots on water, on steep or high ground stay empty. The buildings of a chunk are the objects 14000 + 32 slot + n (up to 25 of them), freed first.
46100 PROC CHCITY(CX,CY,SL,SX,SY,SR)
46110   LOCAL GI,GJ,X,Y,Z,D,K,WH,O,Q,X1,Y1,X2,Y2,HY,RB,RC
46120   FOR GJ=0 TO 4
46130     FOR GI=0 TO 4
46140       X=CX*900+GI*180+90:Y=CY*900+GJ*180+90:D=SQR((X-SX)*(X-SX)+(Y-SY)*(Y-SY)):Q=CX*5+GI:HY=CY*5+GJ
46150       IF D<SR AND CRN<32 THEN
46160         IF HASH(Q,HY,80)<0.7 AND NOT FN ROUGH(X,Y) AND FN HT(X,Y)>WZ+3 THEN
46170           Z=MIN(MIN(FN HT(X-60,Y-60),FN HT(X+60,Y-60)),MIN(FN HT(X-60,Y+60),FN HT(X+60,Y+60)))-6
46180           O=14000+SL*32+CRN:CRN=CRN+1
46190           M3D OBJECT O,0,235
46200           FOR K=0 TO 3
46210             IF HASH(Q,HY,81+K)<0.78 THEN
46220               WH=(30+HASH(Q,HY,90+K)*130)*(1.3-D/SR)+12
46230               X1=X-60:Y1=Y-60:X2=X+60:Y2=Y+60
46240               IF K=0 THEN Y2=Y-52
46250               IF K=1 THEN X1=X+52
46260               IF K=2 THEN Y1=Y+52
46270               IF K=3 THEN X2=X-52
46280               M3D BOX O,X1,Y1,Z,X2,Y2,Z+WH,326
46290             END IF
46300           NEXT K
46310           FOR K=0 TO 2
46320             RB=X-45+HASH(Q,HY,100+K)*90:RC=Y-45+HASH(Q,HY,110+K)*90
46330             M3D BOX O,RB-7,RC-7,Z,RB+7,RC+7,Z+8+HASH(Q,HY,120+K)*14,326
46340           NEXT K
46350         END IF
46360       END IF
46370     NEXT GI
46380   NEXT GJ
46390 END PROC
46400 PROC CHRUINS(CX,CY,SL)
46410   LOCAL RX,RY,I,J,SX,SY,SR
46420   CRN=0:RX=INT(CX/8):RY=INT(CY/8)
46430   FOR J=-1 TO 1
46440     FOR I=-1 TO 1
46450       IF HASH(RX+I,RY+J,70)<0.3 THEN
46460         SX=(RX+I)*7200+HASH(RX+I,RY+J,71)*7200:SY=(RY+J)*7200+HASH(RX+I,RY+J,72)*7200:SR=450+HASH(RX+I,RY+J,73)*450
46470         IF ABS(SX-CX*900-450)<SR+640 AND ABS(SY-CY*900-450)<SR+640 THEN CALL CHCITY(CX,CY,SL,SX,SY,SR)
46480       END IF
46490     NEXT I
46500   NEXT J
46510 END PROC
46600 ' hay bales in the fields: up to 6 a chunk, instances of the bale 343 (a box), objects 3600 + 6 slot + n
46700 PROC CHBALES(CX,CY,SL)
46710   LOCAL I,N,X,Y
46720   M3D OBJFREE 11200+SL*6,11200+SL*6+5
46730   N=0
46740   FOR I=0 TO 11
46750     X=CX*900+HASH(CX,CY,700+I)*900:Y=CY*900+HASH(CX,CY,800+I)*900
46760     IF N<6 AND FN GKIND(X,Y)=2 AND HASH(CX,CY,900+I)<0.5 AND NOT FN NEARROAD(X,Y) AND NOT FN ROUGH(X,Y) AND FN HT(X,Y)>WZ+3 THEN
46770       M3D INSTANCE 11200+SL*6+N,343,X,Y,FN HT(X,Y)-1,HASH(CX,CY,950+I)*360,0.8+HASH(CX,CY,980+I)*0.5:N=N+1
46780     END IF
46790   NEXT I
46800 END PROC
46900 PROC MKBALE
46910   M3D OBJECT 343,0,235:M3D BOX 343,-22,-14,0,22,14,22,321:M3D OBJSHOW 343,0
46920 END PROC
47000 ' ------------------------------------------------------------------------------------------------ roads of the wild world: dirt tracks and tarred roads
47010 ' The region points (1800 units, see CHREGIONS) are joined to their east and south neighbours with a chance of 45 %: a path of 50 unit steps that wanders (noise) and meets the points
47020 ' exactly. 40 % of the roads are TARRED (a real road: 4 lanes of 3.1 m (100 units) = 400 and a shoulder of 20 on each side, 440 wide, picture 327 with the markings; 3 m is about 94 units),
47030 ' the rest dirt tracks (110 wide, picture 304, see-through edges). No road over water, steep or high ground. A step is a SEGMENT; CHSEGS lists the segments that start near the chunk
47040 ' (so that trees and bales keep away from them: NEARROAD), CHROADS builds the ones that start in it
47100 PROC CHSEGS(CX,CY)
47110   LOCAL NT,SP,RX,RY,I,J,E,L,NS,K,T,AX,AY,CX2,CY2,X0,Y0,TR,T0,DT,KA,KB,XB,WA,R1X,R1Y,R2X,R2Y,CL,TL,HB,A0,A1
47120   X0=CX*900:Y0=CY*900:NSG=0
47130   FOR NT=1 TO 0 STEP -1
47140     SP=1800:TR=0:XB=160
47150     IF NT THEN SP=36000:TR=-1:XB=380
47160     RX=INT(X0/SP):RY=INT(Y0/SP)
47170     FOR J=-1 TO 1
47180       FOR I=-1 TO 1
47190         FOR E=0 TO 1
47200           IF M3D("ROADLINK",NT,RX+I,RY+J,E) THEN
47210             R1X=M3D("ROADPT",NT,RX+I,RY+J,E,0,0):R1Y=M3D("ROADPT",NT,RX+I,RY+J,E,0,1)
47220             R2X=M3D("ROADPT",NT,RX+I,RY+J,E,1,0):R2Y=M3D("ROADPT",NT,RX+I,RY+J,E,1,1)
47230             WA=M3D("ROADINFO",NT,RX+I,RY+J,E,4)
47240             IF MAX(R1X,R2X)>X0-WA-520 AND MIN(R1X,R2X)<X0+WA+1420 AND MAX(R1Y,R2Y)>Y0-WA-520 AND MIN(R1Y,R2Y)<Y0+WA+1420 THEN
47250               L=M3D("ROADINFO",NT,RX+I,RY+J,E,1):NS=M3D("ROADINFO",NT,RX+I,RY+J,E,0):CL=SQR((R2X-R1X)^2+(R2Y-R1Y)^2)
47260               HB=M3D("ROADINFO",NT,RX+I,RY+J,E,2)+2*M3D("ROADINFO",NT,RX+I,RY+J,E,3):TL=-1
47270               T0=((X0+450-R1X)*(R2X-R1X)+(Y0+450-R1Y)*(R2Y-R1Y))/(CL*CL):DT=(WA+1500)/CL
47280               KA=MAX(0,INT((T0-DT)*NS)):KB=MIN(NS-1,INT((T0+DT)*NS)+1)
47290               FOR K=KA TO KB
47300                 T=K/NS:AX=M3D("ROADPT",NT,RX+I,RY+J,E,T,0):AY=M3D("ROADPT",NT,RX+I,RY+J,E,T,1)
47310                 IF AX>=X0-XB AND AX<X0+900+XB AND AY>=Y0-XB AND AY<Y0+900+XB AND NSG<299 THEN
47320                   T=(K+1)/NS:CX2=M3D("ROADPT",NT,RX+I,RY+J,E,T,0):CY2=M3D("ROADPT",NT,RX+I,RY+J,E,T,1)
47330                   IF M3D("QUIET",AX,AY)>=0.99 THEN
47340                     SGX(NSG)=AX:SGY(NSG)=AY:SGU(NSG)=CX2:SGV(NSG)=CY2:SGK(NSG)=TR:SGD(NSG)=0:SGE(NSG)=0:SGL(NSG)=L:SGH(NSG)=HB
47350                     SGI(NSG)=(AX>=X0 AND AX<X0+900 AND AY>=Y0 AND AY<Y0+900)
47360                     IF TR AND SGI(NSG) THEN SGD(NSG)=M3D("ROADARC",NT,RX+I,RY+J,E,K/NS):SGE(NSG)=M3D("ROADARC",NT,RX+I,RY+J,E,T)
47370                     NSG=NSG+1
47380                   END IF
47390                 END IF
47400               NEXT K
47410             END IF
47420           END IF
47430         NEXT E
47440       NEXT I
47450     NEXT J
47460   NEXT NT
47470 END PROC
47500 ' builds the reflector posts of the chunk's tarred roads (instances of the post 344, 8800 + 6 slot + n): a post every 50 m (1562.5 units, measured along the straight line between the two region points and put exactly on the road, not at the end of the step) on both edges (232 units from the middle: just outside the 440 wide tar); the post on the right-hand side of the
47510 ' direction of the road shows a white reflector to the traffic that comes from behind and a red one to the oncoming traffic, the one on the left the other way round: the right-hand side of
47520 ' each driver has the white ones, the left-hand side the red ones. (The road itself, tar and dirt, is a ribbon in the mesh of the chunk's ground job.)
47600 PROC CHROADS(CX,CY,SL)
47610   LOCAL I,P0,NP,CX2,CY2,DX,DY,D,YW,PX,PY,NK,F,MX,MY,PD,OK
47620   P0=8800+SL*6:NP=0
47630   M3D OBJFREE P0,P0+5
47640   FOR I=0 TO NSG-1
47650     IF SGI(I) AND SGK(I) THEN
47660       CX2=SGU(I):CY2=SGV(I):DX=CX2-SGX(I):DY=CY2-SGY(I):D=SQR(DX*DX+DY*DY):DX=DX/D:DY=DY/D
47670       NK=INT(SGE(I)/1562.5):PD=NK*1562.5
47680       IF INT(SGD(I)/1562.5)<>NK AND NP<5 THEN
47690         OK=-1:IF (SGH(I) AND 1) AND PD<520 THEN OK=0
47700         IF (SGH(I) AND 2) AND PD>SGL(I)-520 THEN OK=0
47710         IF OK THEN
47720           F=(PD-SGD(I))/(SGE(I)-SGD(I)):MX=SGX(I)+(CX2-SGX(I))*F:MY=SGY(I)+(CY2-SGY(I))*F
47730           YW=ATAN2(DY,DX)*180/PI
47740           PX=MX+DY*232:PY=MY-DX*232:M3D INSTANCE P0+NP,344,PX,PY,FN HT(PX,PY),YW,1:NP=NP+1
47750           PX=MX-DY*232:PY=MY+DX*232:M3D INSTANCE P0+NP,344,PX,PY,FN HT(PX,PY),YW+180,1:NP=NP+1
47760         END IF
47770       END IF
47780     END IF
47790   NEXT I
47800 END PROC
47900 ' the picture of a tarred road: 64 x 64 texels, across the road (440 units, about 6.9 units a texel) and along it (64 texels = 376 units = 12 m); dark tar with speckles, a continuous
47910 ' white edge line on each side (x 3 and 60), and a dashed white middle line (x 31 / 32: 4.5 m dashes, 7.5 m gaps); no lane lines (owner 2026-11-22: only the one dashed line in the middle)
48000 PROC MKTAR
48010   LOCAL I
48020   GFX LINE 0,0,63,63,FN CC(0,3),BF
48030   FOR I=1 TO 220
48040     GFX PSET FN RN(64),FN RN(64),FN CC(0,2+FN RN(3))
48050   NEXT I
48060   GFX LINE 3,0,3,63,FN CC(0,15):GFX LINE 60,0,60,63,FN CC(0,15)
48070   GFX LINE 31,0,32,23,FN CC(0,15),BF
48080   CALL W3K_GRAB(327,64,64)
48090   GFX LINE 0,0,3,3,FN CC(0,4),BF:CALL W3K_GRAB(328,4,4)
48100 END PROC
48200 ' the reflector post (object 344, hidden, passable): a white post of 36 units (1.1 m) with a white reflector on its -x face and a red one on its +x face; the road's direction is its +x
48300 PROC MKPOST
48310   M3D OBJECT 344,4,255
48320   M3D BOX 344,-2.5,-2.5,0,2.5,2.5,36,FN CC(0,13)
48330   M3D BOX 344,-3.5,-2,21,-2.5,2,31,FN CC(0,15)
48340   M3D BOX 344,2.5,-2,21,3.5,2,31,FN CC(1,11)
48350   M3D OBJSHOW 344,0
48360 END PROC
49000 ' ------------------------------------------------------------------------------------------------ the rings of the far ground (M3D RING jobs of the engine's worker thread)
49010 ' Round the 17 x 17 chunks of CHUNKS (15300 units) the ground goes on in three rings of cells that grow with the distance, each a square grid with a hole where the finer one is:
49020 ' ring 2 cells of 900 (45 x 45 = 40500, the hole is the window: 17 x 17 cells), ring 3 cells of 2700 (36 x 36 = 97200, hole = ring 2), ring 4 cells of 8100 (40 x 40 = 324000 = 10 km,
49030 ' hole = ring 3; the view reaches 5 km). The cells of the chunks at 4 or more away are the cells of ring 2, so they meet without a step. Rings 2 and 3 MORPH: their vertices blend into the coarser
49040 ' ring's surface by their distance from the player (M3D MORPH, every frame; ring 2 from 9000 to 17550 units, ring 3 from 23000 to 40500), so no hill appears or goes with a change of ring.
49050 ' A ring lies on the grid of the next coarser ring (its corner a multiple of 2700 / 8100 / 8100), so it is made again only when the player has moved so far (or the finer ring did; ring 2 also
49060 ' when the window moved a chunk: its hole): objects 27002 .. 27004; the jobs run on the worker thread and are taken together by M3D POLL
49100 PROC RINGUPDATE(PX,PY)
49110   LOCAL PCX,PCY,O2X,O2Y,O3X,O3Y,O4X,O4Y,C2,C3,C4
49120   PCX=INT(PX/900):PCY=INT(PY/900)
49130   O2X=INT((PX-20250)/2700)*2700:O2Y=INT((PY-20250)/2700)*2700
49140   O3X=INT((PX-48600)/8100)*8100:O3Y=INT((PY-48600)/8100)*8100
49150   O4X=INT((PX-162000)/8100)*8100:O4Y=INT((PY-162000)/8100)*8100
49160   C2=(O2X<>RGX(2) OR O2Y<>RGY(2) OR PCX<>RGP(2) OR PCY<>RGQ(2))
49170   C3=(O3X<>RGX(3) OR O3Y<>RGY(3) OR O2X<>RGP(3) OR O2Y<>RGQ(3))
49180   C4=(O4X<>RGX(4) OR O4Y<>RGY(4) OR O3X<>RGP(4) OR O3Y<>RGQ(4))
49190   IF C2 THEN M3D RING 27002,O2X,O2Y,45,900,60,0,(PCX-8)*900,(PCY-8)*900,17,9000,17550:RGX(2)=O2X:RGY(2)=O2Y:RGP(2)=PCX:RGQ(2)=PCY
49200   IF C3 THEN M3D RING 27003,O3X,O3Y,36,2700,150,0,O2X,O2Y,15,23000,40500:RGX(3)=O3X:RGY(3)=O3Y:RGP(3)=O2X:RGQ(3)=O2Y
49210   IF C4 THEN M3D RING 27004,O4X,O4Y,40,8100,400,0,O3X,O3Y,12:RGX(4)=O4X:RGY(4)=O4Y:RGP(4)=O3X:RGQ(4)=O3Y
49220 END PROC
49300 ' test mode (play-back command "pop n step"): John walks n steps of "step" units east and after every step the height of the ground (the chunks' meshes) at 40 fixed places 700 .. 6400 units
49310 ' ahead is read again: how much the largest change from one step to the next is (a hill that pops up in front of the player shows as a big one); prints "poptest samples= max= over3="
49400 PROC POPTEST(N,SS)
49410   LOCAL I,K,D,X,Y,H,MX,CNT,BIG,SK,X0,Y0
49420   X0=INT(JX/300)*300+700:Y0=INT(JY/300)*300:MX=0:CNT=0:BIG=0
49430   FOR I=1 TO N
49440     JX=JX+SS
49450     CALL TERRAINUPDATE(JX,JY)
49460     SK=0
49470     FOR K=0 TO 19
49480       FOR D=0 TO 1
49490         X=X0+K*300:Y=Y0+D*200:H=M3D("FLOORAT",X,Y,9000)
49500         IF I>5 AND PV(SK)>-100000 AND H>-100000 THEN
49510           IF ABS(H-PV(SK))>MX THEN MX=ABS(H-PV(SK))
49520           IF ABS(H-PV(SK))>3 THEN BIG=BIG+1
49530           CNT=CNT+1
49540         END IF
49550         PV(SK)=H:SK=SK+1
49560       NEXT D
49570     NEXT K
49580   NEXT I
49590   PRINT "poptest samples=";CNT;" max=";INT(MX*10)/10;" over3=";BIG
49600 END PROC
50000 ' ------------------------------------------------------------------------------------------------ the static world (docs/world-subsystem.md)
50010 ' The ground is a world made once (M3D WORLDMAKE: 300 x 300 chunks of 500 m by default (up to 1000 x 1000), kept in the folder WDIR$, by default ~/.engine1987-world; G1987_WORLD names another folder, G1987_WORLDSIZE the
50020 ' size) and only drawn while you play (M3D WORLDVIEW): nothing is decided or changed in front of you. The old dynamic chunks are still there: G1987_DYNAMIC=1 (and every headless test).
50030 ' The position is saved in the folder when the game ends (player.dat) and loaded again by NEWGAME; N at the title makes a new world.
50100 PROC RINGOBJS
50110   LOCAL K
50120   FOR K=2 TO 4:M3D OBJECT 27000+K,12,235:NEXT K
50130 END PROC
50200 PROC TERRAININIT
50210   IF WLD THEN
50220     CALL WORLDOPENUP
50230   ELSE
50240     CALL CHUNKS(0,0,400):CALL RINGUPDATE(0,0):M3D RINGWAIT
50250   END IF
50260 END PROC
50300 PROC TERRAINUPDATE(PX,PY)
50310   IF WLD THEN
50320     M3D WORLDVIEW PX,PY
50330     M3D OBJPOS 250,INT(PX/512)*512,INT(PY/512)*512,WZ
50340   ELSE
50350     CALL CHUNKS(PX,PY,1):CALL RINGUPDATE(PX,PY):M3D POLL:M3D MORPH PX,PY
50360   END IF
50370 END PROC
50400 PROC TERRAINJUMP(PX,PY)
50410   IF WLD THEN
50420     M3D WORLDVIEW PX,PY:M3D WORLDWAIT
50430     M3D OBJPOS 250,INT(PX/512)*512,INT(PY/512)*512,WZ
50440   ELSE
50450     CALL CHUNKS(PX,PY,400):CALL RINGUPDATE(PX,PY):M3D RINGWAIT:M3D MORPH PX,PY
50460   END IF
50470 END PROC
50500 ' opens the world; the water is one translucent plane at the water level that follows the player in steps of 512 units (the picture repeats every 128), seen only where the ground dips below it
50600 PROC WORLDOPENUP
50610   LOCAL R
50620   M3D WORLDOPEN WDIR$,5000,1,255
50630   WN=M3D("WORLDINFO",1):WLIM=WN*15625/2-6*15625
50640   R=170000
50650   M3D OBJECT 250,30,235
50660   M3D VERTEX 250,-R,-R,0,-R*0.5,-R*0.5:M3D VERTEX 250,R,-R,0,R*0.5,-R*0.5:M3D VERTEX 250,R,R,0,R*0.5,R*0.5:M3D VERTEX 250,-R,R,0,-R*0.5,R*0.5
50670   M3D QUAD 250,0,1,2,3,325
50680   M3D WORLDVIEW 0,0:M3D WORLDWAIT
50690   M3D OBJPOS 250,0,0,WZ
50700 END PROC
50800 ' where the world is and how big a new one is (before the title: it tells whether there is a world)
50900 PROC WORLDDIR
50910   IF WLD THEN
50920     WDIR$=ENV$("G1987_WORLD"):IF WDIR$="" THEN WDIR$=ENV$("HOME")+"/.engine1987-world"
50930     WSZ=300:IF ENV$("G1987_WORLDSIZE")<>"" THEN WSZ=VAL(ENV$("G1987_WORLDSIZE"))
50940   END IF
50950 END PROC
51000 ' after the title: the world is made now when there is none or a new one was asked for (with a bar: 300 x 300 chunks take about 20 s), later starts only open it
51100 PROC WORLDPREP
51110   IF WLD THEN
51120     IF WNEW OR NOT EXISTS(WDIR$+"/world.dat") THEN CALL WORLDMAKEUP
51130   END IF
51140 END PROC
51200 PROC WORLDMAKEUP
51210   LOCAL P,T0
51220   RANDOMIZE TI
51230   IF EXISTS(WDIR$+"/player.dat") THEN KILL WDIR$+"/player.dat"
51240   M3D WORLDMAKE WDIR$,WSZ,INT(RND(1)*1000000)+1
51250   T0=TI
51260   REPEAT
51270     P=M3D("WORLDINFO",5)
51280     IF NOT HL THEN
51290       CALL LOADING("MAKING THE WORLD",P/1000,STR$(WSZ)+" x "+STR$(WSZ)+" CHUNKS OF 500 M, ABOUT "+STR$(INT((TI-T0)/60))+" SECONDS SO FAR")
51300     ELSE
51310       FRAME 30
51320     END IF
51330   UNTIL M3D("WORLDINFO",6)=1 OR P<0
51340   IF P<0 THEN PRINT "the world could not be made in ";WDIR$:IN_QUIT=1
51350 END PROC
51400 ' N at the title: a new world (the old one is replaced), the saved position is forgotten (WORLDMAKEUP does that for every new world)
51500 PROC WORLDREMAKE
51510   M3D WORLDCLOSE
51520   IF EXISTS(WDIR$+"/player.dat") THEN KILL WDIR$+"/player.dat"
51530   CALL WORLDMAKEUP
51540   IF IN_QUIT=0 THEN CALL WORLDOPENUP
51550   CALL LOADEND
51560 END PROC
51600 PROC WSAVE
51610   OPEN WDIR$+"/player.dat" FOR OUTPUT AS #6
51620   PRINT #6,JX:PRINT #6,JY:PRINT #6,JH:PRINT #6,BX:PRINT #6,BY:PRINT #6,SXY:PRINT #6,DAYH
51630   CLOSE #6
51640 END PROC
51700 ' a place that is fit to start from: inside the walls of the world, on dry ground that is not a mountain (a saved position of another world may be anywhere)
51800 FUNC SAFESPOT(X,Y)
51810   LOCAL G
51820   IF ABS(X)>WLIM OR ABS(Y)>WLIM THEN RETURN 0
51830   G=M3D("GROUND",X,Y)
51840   IF G<WZ+4 OR G>1200 THEN RETURN 0
51850   RETURN -1
51860 END FUNC
51900 ' the saved position (if there is one): John and the bicycle are there again
52000 PROC WLOAD
52010   IF WLD AND NOT WNEW THEN
52020     IF EXISTS(WDIR$+"/player.dat") THEN
52030       OPEN WDIR$+"/player.dat" FOR INPUT AS #6
52040       INPUT #6,JX:INPUT #6,JY:INPUT #6,JH:INPUT #6,BX:INPUT #6,BY:IF NOT EOF(6) THEN INPUT #6,SXY
52045       IF NOT EOF(6) THEN INPUT #6,DAYH
52050       CLOSE #6
52060       IF NOT FN SAFESPOT(JX,JY) THEN JX=0:JY=-172:JH=270
52070       IF NOT FN SAFESPOT(BX,BY) THEN BX=130:BY=-214
52080       CALL TERRAINJUMP(JX,JY):JZ=FN FLR(JX,JY):BZ=FN FLR(BX,BY)
52090     END IF
52100   END IF
52110 END PROC
53000 ' ------------------------------------------------------------------------------------------------ pause menu, reset, world coordinates
53010 ' John and the bicycle go back to where a new game starts them (the saved position is not read)
53100 PROC RESETPOS
53110   NOLOAD=-1:CALL NEWGAME:NOLOAD=0
53120 END PROC
53200 ' the world coordinates of the player (John, or the bicycle when he rides it) in metres from the origin of the world (3.2 cm a unit), the height and the chunk
53300 PROC COORDTXT
53310   LOCAL X,Y,Z,A$,CX,CY
53320   IF MODE=1 THEN X=BX:Y=BY ELSE X=JX:Y=JY
53330   Z=FN FLR(X,Y)
53340   A$="X "+FORMAT$(X*0.032,"#######.##")+"  Y "+FORMAT$(Y*0.032,"#######.##")+"  Z "+FORMAT$(Z*0.032,"####.##")+" M"
53350   IF WLD THEN
53360     CX=M3D("WORLDCHUNK",X,Y,0):CY=M3D("WORLDCHUNK",X,Y,1)
53370     IF CX>=0 THEN A$=A$+"  CHUNK "+FORMAT$(CX,"###")+","+FORMAT$(CY,"###")
53380   END IF
53390   CT$=A$
53400 END PROC
53500 PROC COORDS
53510   CALL COORDTXT
53520   GFX TEXT 640-LEN(CT$)*8-5,33,CT$,FN CC(0,1)
53530   GFX TEXT 640-LEN(CT$)*8-6,32,CT$,FN CC(3,15)
53540   CALL AXES
53550 END PROC
53600 FUNC PADX(X)
53610   RETURN JOY("X")<>0
53620 END FUNC
53700 FUNC PADY(X)
53710   RETURN JOY("Y")<>0
53720 END FUNC
53800 ' the pause menu: P / Q / START / BACK go on with the game, R / X reset the positions, C / Y show or hide the coordinates (the game is left only while playing: Q, BACK or ESC)
53900 PROC PAUSEMENU
53910   LOCAL K$,K,DONE,LX,LY,D,PT0
53920   WHILE FN IN_ANYSTART(0)
53930     GFX VSYNC
53940   WEND
53950   D=-1:DONE=0:LX=-1:LY=-1:PT0=TI
53955   ' a held P repeats: the characters of the repeat must neither close the pause at once nor open it again. The dialog is drawn straight onto the layer that is on show (W3K_B), once (again when C
53956   ' changes it), and the other layer is never shown: showing the two layers by turns (W3K_SHOW) flickered, they hold the last two frames, which differ (the scrolling message, the camera)
53960   REPEAT
53970     IF D THEN
53975       GFX TARGET W3K_B
53980       CALL BIGTEXT(110,"PAUSED",FN CC(0,15),3)
53990       GFX LINE 0,146,639,222,FN CC(0,1),BF
54000       CALL BIGTEXT(150,"P: CONTINUE",FN CC(3,15),1)
54010       CALL BIGTEXT(164,"R: RESET JOHN AND THE BICYCLE TO THE START",FN CC(0,13),1)
54020       IF SXY THEN CALL BIGTEXT(178,"C: WORLD COORDINATES ON THE SCREEN: ON",FN CC(0,13),1) ELSE CALL BIGTEXT(178,"C: WORLD COORDINATES ON THE SCREEN: OFF",FN CC(0,13),1)
54030       CALL BIGTEXT(192,"(Q OR ESC WHILE PLAYING: LEAVE THE GAME)",FN CC(0,9),1)
54040       CALL BIGTEXT(208,"PAD: START CONTINUE  X RESET  Y COORDINATES",FN CC(0,9),1)
54050       GFX TARGET 3-W3K_B
54060       D=0
54070     END IF
54080     GFX VSYNC
54090     K$=INKEY$:K=0
54100     IF K$<>"" THEN K=ASC(K$)
54105     IF TI-PT0<25 AND K<>0 THEN K=0
54110     IF K>=97 AND K<=122 THEN K=K-32
54120     IF K=67 OR (FN PADY(0) AND NOT LY) THEN SXY=NOT SXY:D=-1
54130     IF K=82 OR (FN PADX(0) AND NOT LX) THEN CALL RESETPOS:CALL SAY("BACK AT THE START."):DONE=-1
54140     LX=FN PADX(0):LY=FN PADY(0)
54150     IF K=80 OR FN IN_ANYSTART(0) THEN DONE=-1
54160     IF K=81 OR FN IN_ANYBACK(0) THEN DONE=-1
54170   UNTIL DONE
54180   WHILE FN IN_ANYSTART(0)
54190     GFX VSYNC
54200   WEND
54210   WHILE KEY("P")
54212     GFX VSYNC
54214   WEND
54216   WHILE INKEY$<>""
54218   WEND
54219   IN_PSK=0
54220 END PROC
54300 ' the game sessions: play, save, back to the title (Q, BACK or ESC leave the game; Q, BACK or ESC at the title end the program); SPACE there resumes the game exactly as it was (no new game: John, the bicycle,
54310 ' the camera and the state of everything stay); N at the title makes a new world and starts a new game in it
54400 PROC SESSION
54410   NEWG=-1
54420   REPEAT
54430     IF NEWG THEN CALL NEWGAME:NEWG=0
54440     CALL LOADEND
54450     CALL PLAYLOOP
54460     IF WLD THEN CALL WSAVE
54470     IF ENV$("G1987_FRAMELOG")<>"" THEN
54480       IN_QUIT=1
54490     ELSE
54500       CALL SOUNDMUTE
54510       CALL TITLE
54520       IF IN_QUIT=0 THEN CALL W3K_REAPPLY
54530       IF WNEW AND IN_QUIT=0 THEN CALL WORLDREMAKE:NEWG=-1
54540     END IF
54550   UNTIL IN_QUIT
54560 END PROC
54600 ' the loading screen: a colored background and a cute little drone (the black drones of the game, as a friendly egg with big eyes and an anti-gravity glow under it) that bobs and blinks; P 0 .. 1 draws a bar (below 0: none),
54610 ' N$ a small line under it; every call is one frame, so the drone moves as long as the program calls it now and then
54700 PROC LOADING(M$,P,N$)
54710   LOCAL I,T
54720   IF HL THEN EXIT PROC
54730   M3D HIDE
54740   ' a permission dialog (or anything else that held the program up for 1.5 s) can leave the drone gone: after such a gap the sprites are made and shown again
54750   IF LDON AND TI-LDLAST>90 THEN LDS=0:LDON=0
54760   IF LDS=0 THEN CALL LDSPRITES
54770   IF NOT LDON THEN CALL LDSHOW
54780   LDLAST=TI
54790   LDT=LDT+3:T=LDT
54800   FOR I=0 TO 14
54810     GFX LINE 0,I*20,639,I*20+19,FN CC(6,2+I\2),BF
54820   NEXT I
54830   ' the shadow on the ground (the drone itself is a group of sprites that the engine animates and moves by itself)
54840   GFX CIRCLE 320,196,30-INT(3*SIN(T/9)),FN CC(6,1),0,360,0.22:GFX PAINT 320,196,FN CC(6,1),FN CC(6,1)
54850   CALL BIGTEXT(8,"1987",FN CC(3,15),3)
54860   CALL BIGTEXT(176,M$,FN CC(3,15),2)
54870   IF P>=0 THEN
54880     GFX LINE 100,222,539,236,FN CC(0,1),BF:GFX LINE 100,222,539,236,FN CC(0,15),B
54890     IF P>0 THEN GFX LINE 102,224,102+INT(436*MIN(1,P)),234,FN CC(3,15),BF
54900   END IF
54910   IF N$<>"" THEN CALL BIGTEXT(250,N$,FN CC(0,15),1)
54920   CALL W3K_SHOW
54930 END PROC
55000 ' the loading drone: three groups of sprites that the engine animates by itself (SPRITE SEQ / ANIM; SPRITE PATH bobs them all the same way): the body (head 200, 8 x 5 sprites, still: an anti-gravity
55010 ' drone has no rotors), the face (head 240, 3 x 2: the eyes look about and blink) and the field under it (190, 3 x 1 sprites: a glowing disc that pulses). The pictures are drawn once with GFX and grabbed; the sprite palette takes the graphics palette
55020 ' (the sprite palette takes entries 0..219 only: 220..255 are the mouse pointer's glass colors, and a program's change of them is ignored)
55100 PROC LDSPRITES
55110   LOCAL V,K
55120   FOR K=0 TO 219:GFX PALETTE 4,K,GFX("PALETTE",0,K):NEXT K
55130   CALL LDBODY(64,60):CALL LDGRAB(0,8,5)
55140   FOR V=0 TO 3
55150     K=0:IF V=1 THEN K=-3
55160     IF V=2 THEN K=3
55170     CALL LDEYES(24,30,K,V=3):CALL LDGRAB(120+V*6,3,2)
55180   NEXT V
55190   FOR V=0 TO 2
55200     CALL LDFIELD(24,12,17+2*V):CALL LDGRAB(144+V*3,3,1)
55210   NEXT V
55220   SPRITE 200,SIZE 8,5:SPRITE 200,PRIORITY 5
55230   SPRITE 240,SIZE 3,2:SPRITE 240,PRIORITY 6
55240   SPRITE 190,SIZE 3,1:SPRITE 190,PRIORITY 4
55250   SPRITE SEQ 21,CHR$(120)+CHR$(120)+CHR$(126)+CHR$(126)+CHR$(120)+CHR$(120)+CHR$(132)+CHR$(132)+CHR$(132)+CHR$(120)+CHR$(120)+CHR$(120)+CHR$(120)+CHR$(138)
55260   SPRITE SEQ 22,CHR$(144)+CHR$(147)+CHR$(150)+CHR$(147)
55270   SPRITE 200,256,44,0:SPRITE 240,296,74,120:SPRITE 190,296,132,144
55280   SPRITE 240,ANIM 21,14,6:SPRITE 190,ANIM 22,4,2
55290   LDS=-1:LDON=0
55300 END PROC
55400 ' the pictures of the groups are in the rows 0 .. 23 and columns 0 .. 127 of the graphics layer: COLS x ROWS sprites from the image IM on
55500 PROC LDGRAB(IM,COLS,ROWS)
55510   LOCAL R,C,G$
55520   FOR R=0 TO ROWS-1:FOR C=0 TO COLS-1
55530     GFX GRAB C*16,R*24,C*16+15,R*24+23,G$
55540     SPRITE DEF IM+R*COLS+C,MID$(G$,5)
55550   NEXT C:NEXT R
55560 END PROC
55600 ' the three groups go on and off together (the paths start in the same frame, so they bob alike)
55700 PROC LDSHOW
55710   SPRITE 200,ON:SPRITE 240,ON:SPRITE 190,ON
55720   SPRITE 200,PATH "T 30 Y-7 Y+14 Y-7 R":SPRITE 240,PATH "T 30 Y-7 Y+14 Y-7 R":SPRITE 190,PATH "T 30 Y-7 Y+14 Y-7 R"
55730   LDON=-1
55740 END PROC
55800 PROC LOADEND
55810   IF LDON THEN SPRITE 200,OFF:SPRITE 240,OFF:SPRITE 190,OFF:LDON=0
55820 END PROC
55900 ' the body of the egg drone at CX, CY on the graphics layer: dark, with a shine, a gray belt, a red antenna ball and the plate of the anti-gravity emitter underneath
56000 PROC LDBODY(CX,CY)
56010   GFX LINE 0,0,127,119,0,BF
56020   GFX CIRCLE CX,CY,27,FN CC(0,3),0,360,1.25:GFX PAINT CX,CY,FN CC(0,3),FN CC(0,3)
56030   GFX CIRCLE CX-12,CY-20,6,FN CC(0,8),0,360,0.7:GFX PAINT CX-12,CY-20,FN CC(0,8),FN CC(0,8)
56040   GFX LINE CX-26,CY+10,CX+26,CY+16,FN CC(0,9),BF
56050   GFX CIRCLE CX,CY+31,12,FN CC(0,6),0,360,0.3:GFX PAINT CX,CY+31,FN CC(0,6),FN CC(0,6):GFX CIRCLE CX,CY+31,12,FN CC(8,12),0,360,0.3
56060   GFX LINE CX,CY-34,CX,CY-46,FN CC(0,9):GFX CIRCLE CX,CY-50,4,FN CC(1,12):GFX PAINT CX,CY-50,FN CC(1,12),FN CC(1,12)
56070 END PROC
56100 ' the face (CX, CY is the body's middle in the face picture): big eyes with pupils shifted by PX (BL: closed), cheeks and a smile
56200 PROC LDEYES(CX,CY,PX,BL)
56210   LOCAL I,ER
56220   GFX LINE 0,0,127,119,0,BF
56230   ER=1:IF BL THEN ER=0.2
56240   FOR I=-1 TO 1 STEP 2
56250     GFX CIRCLE CX+I*11,CY-6,8,FN CC(0,15),0,360,ER:GFX PAINT CX+I*11,CY-6,FN CC(0,15),FN CC(0,15)
56260     IF NOT BL THEN GFX CIRCLE CX+I*11+PX,CY-6,4,FN CC(0,1):GFX PAINT CX+I*11+PX,CY-6,FN CC(0,1),FN CC(0,1)
56270     GFX CIRCLE CX+I*19,CY+3,4,FN CC(1,12),0,360,0.6:GFX PAINT CX+I*19,CY+3,FN CC(1,12),FN CC(1,12)
56280   NEXT I
56290   GFX CIRCLE CX,CY+2,9,FN CC(0,15),215,325
56300 END PROC
56400 ' the field under the egg (the anti-gravity glow): a flat disc, R wide, pale blue with a white core
56500 PROC LDFIELD(CX,CY,R)
56510   GFX LINE 0,0,47,23,0,BF
56520   GFX CIRCLE CX,CY,R,FN CC(8,8),0,360,0.3:GFX PAINT CX,CY,FN CC(8,8),FN CC(8,8)
56530   GFX CIRCLE CX,CY,R*0.7,FN CC(8,13),0,360,0.3:GFX PAINT CX,CY,FN CC(8,13),FN CC(8,13)
56540   GFX CIRCLE CX,CY,R*0.35,FN CC(0,15),0,360,0.3:GFX PAINT CX,CY,FN CC(0,15),FN CC(0,15)
56550 END PROC
56600 ' the compass: a tape at the top left of the screen (centre x 160, 1.5 pixels a degree, 60 degrees to each side) that shows where N, E, S and W are; the world's +x is east and +y north
56610 ' (the camera's yaw 0 looks east, 90 north), so the bearing is 90 minus the yaw; ticks every 15 degrees, the letters at the four directions, small ticks at the corners
56700 PROC COMPASS
56710   LOCAL B,K,D,X
56720   B=90-M3D("YAW")
56730   GFX LINE 70,3,250,15,FN CC(0,3),BF
56740   FOR K=0 TO 23
56750     D=FN ADIFF(K*15,B)
56760     IF ABS(D)<=56 THEN
56770       X=160+INT(D*1.5)
56780       IF K MOD 6=0 THEN
56790         X=X-4
56800         IF K=0 THEN GFX TEXT X,3,"N",FN CC(2,13)
56810         IF K=6 THEN GFX TEXT X,3,"E",FN CC(3,15)
56820         IF K=12 THEN GFX TEXT X,3,"S",FN CC(3,15)
56830         IF K=18 THEN GFX TEXT X,3,"W",FN CC(3,15)
56840       ELSE
56850         GFX LINE X,11,X,14,FN CC(0,13)
56860       END IF
56870     END IF
56880   NEXT K
56890   GFX LINE 160,16,160,19,FN CC(3,15)
56900 END PROC
57000 ' SWEEP DT SECONDS (a test command): the camera is swept round John for a second to one side, a second to the other (as if Z, then X were held), with a frame time of DT; prints the camera of every frame
57100 PROC SWEEPCAM(SD,SEC)
57110   LOCAL SN,SK,SS
57120   SN=INT(SEC/SD)
57130   FOR SK=1 TO SN
57140     SS=1:IF INT(SK*SD) MOD 2=1 THEN SS=-1
57150     KFD=0:KFA=0:KF=0:KT=0:KLT=0:KSH=0:KRA=0:KR=0:KB=0:KU=0:CO=CO+SS*80*SD:CMIN=-1
57160     CALL SIM(SD):CALL TERRAINUPDATE(JX,JY):DTF=SD:CALL CAMUPDATE(SD)
57170     PRINT "sweep ";SK;" ";INT(M3D("X")*10)/10;" ";INT(M3D("Y")*10)/10;" ";INT(M3D("Z")*10)/10;" ";INT(M3D("YAW")*10)/10;" ";INT(M3D("PITCH")*10)/10;" ";CMPUL
57180   NEXT SK
57190 END PROC
57200 ' the axis legend under the coordinates: the world's X (red), Y (green) and Z (blue) axes seen from the camera (X east, Y north, Z up; the short gray stubs are the negative ends)
57300 PROC AXES
57310   LOCAL AXY,AXP,AXK,AXC,AXH,AXV,AXX,AXZ,AXT$,AXN,AXU,AXW
57320   AXY=M3D("YAW")*PI/180:AXP=CMPT*PI/180
57330   GFX LINE 566,58,634,122,FN CC(0,3),BF
57340   FOR AXK=0 TO 2
57350     IF AXK=0 THEN AXH=SIN(AXY):AXV=-COS(AXY)*SIN(AXP):AXT$="X":AXC=FN CC(2,12)
57360     IF AXK=1 THEN AXH=-COS(AXY):AXV=-SIN(AXY)*SIN(AXP):AXT$="Y":AXC=FN CC(5,12)
57370     IF AXK=2 THEN AXH=0:AXV=COS(AXP):AXT$="Z":AXC=FN CC(8,12)
57380     AXX=600+INT(AXH*24):AXZ=90-INT(AXV*24)
57390     GFX LINE 600,90,600-INT(AXH*9),90+INT(AXV*9),FN CC(0,9)
57400     GFX LINE 600,90,AXX,AXZ,AXC
57410     AXN=SQR(AXH*AXH+AXV*AXV):AXU=0:AXW=-1
57420     IF AXN>0.15 THEN
57430       AXU=AXH/AXN:AXW=-AXV/AXN
57440       GFX LINE AXX,AXZ,INT(AXX-AXU*7-AXW*3.5),INT(AXZ-AXW*7+AXU*3.5),AXC
57450       GFX LINE AXX,AXZ,INT(AXX-AXU*7+AXW*3.5),INT(AXZ-AXW*7-AXU*3.5),AXC
57460     END IF
57470     GFX TEXT AXX+INT(AXU*9)-4,AXZ+INT(AXW*9)-4,AXT$,AXC
57480   NEXT AXK
57490 END PROC
58000 ' ------------------------------------------------------------------------------------------------ the black-out: a drone finds John, the beam, the fall of John and of the camera
58010 ' (owner, 2026-11-26) A drone finds John when its spot stays on him (DRCATCH) or when he is within 10 m of it (DRNEAR, DRFOUND); its spot then locks on him (DRFOCUS), the beam grows for a second or two and
58020 ' John collapses (BLACKOUT): off the bicycle, or to the ground. The drone then turns the beam on the camera (DRCAM, mode 4): the camera is a drone too, it is the viewer. Its beam holds on the camera
58030 ' for 1.5 s, the camera falls and lies on the ground (CAMDOWN, CAMFALL: CMF 1 falling, 2 lying), and only then the picture goes black (VEIL). John comes to after UTM seconds (UNCON), the camera
58040 ' rises again, and for 25 s (PROT) no drone looks at him.
58100 ' a drone has found John: its beam locks on him
58110 PROC DRFOUND(K)
58120   DRM(K)=3:DRI(K)=0:DRL(K)=0:DRW(K)=0:DRSC(K)=0
58130   CALL SAY("THE DRONE HAS FOUND YOU."):MSGT=5
58140 END PROC
58200 ' a drone that has found John: it hangs over him, its spot chases him (1200 units a second: no bicycle gets away); while he is in the spot the beam grows stronger (DRI 0 .. 1 in 1.4 s), out of
58210 ' it the beam weakens and after 2.5 s without him the drone loses interest. DFTS: its speed, DFIN: John is in the spot
58220 PROC DRFOCUS(K,DT)
58230   LOCAL DX,DY,L,S
58240   DX=JX-DRX(K):DY=JY-DRY(K):L=SQR(DX*DX+DY*DY)
58250   DRH(K)=DRH(K)+FN ADIFF(ATAN2(DY,DX)*180/PI,DRH(K))*MIN(1,2*DT)
58260   DFTS=MIN(260,MAX(0,(L-40)*1.3))
58270   DX=JX-DSX(K):DY=JY-DSY(K):L=SQR(DX*DX+DY*DY)
58280   IF L>0.01 THEN S=MIN(L,1200*DT):DSX(K)=DSX(K)+DX/L*S:DSY(K)=DSY(K)+DY/L*S
58290   DX=JX-DSX(K):DY=JY-DSY(K):DFIN=(DX*DX+DY*DY<8100)
58300   IF DFIN THEN
58310     DRI(K)=MIN(1,DRI(K)+0.7*DT):DRL(K)=0
58320   ELSE
58330     DRI(K)=MAX(0,DRI(K)-0.25*DT):DRL(K)=DRL(K)+DT
58340   END IF
58350   IF DRL(K)>2.5 OR MODE=5 THEN DRM(K)=2:DRTM(K)=2+RND*2:DRI(K)=0:DRL(K)=0
58360 END PROC
58400 ' a scanning (or cruising, with the beam on) drone finds John when its spot stays on him for 0.12 s; not while he is unconscious or in the quiet time (PROT) after he came to
58410 PROC DRCATCH(K,DT)
58420   LOCAL DX,DY
58430   IF PROT>0 OR MODE=5 OR DRBM(K)<0.7 THEN DRW(K)=0:EXIT PROC
58440   DX=JX-DSX(K):DY=JY-DSY(K)
58450   IF DX*DX+DY*DY<8100 THEN DRW(K)=DRW(K)+DT ELSE DRW(K)=0
58460   IF DRW(K)>0.12 THEN CALL DRFOUND(K)
58470 END PROC
58500 ' mode 4: John is down and the drone has turned its beam on the camera. The spot lies where the line from the drone through the camera meets the ground, so the camera is in the beam; after 1.5 s
58510 ' the camera goes down (CAMDOWN); the drone stays until it has lain there for 1.5 s, then it moves on (or at once when John is up)
58520 PROC DRCAM(K,DT)
58530   LOCAL DX,DY,L,S,Z0,GZ,T,SX,SY
58540   DX=JX-DRX(K):DY=JY-DRY(K):L=SQR(DX*DX+DY*DY)
58550   DRH(K)=DRH(K)+FN ADIFF(ATAN2(DY,DX)*180/PI,DRH(K))*MIN(1,2*DT)
58560   DFTS=MIN(260,MAX(0,(L-40)*1.3))
58570   Z0=DRZ(K)-62:GZ=FN HT(CMX,CMY)
58580   T=(Z0-GZ)/MAX(20,Z0-CMZ):T=MAX(1,MIN(4,T))
58590   SX=DRX(K)+(CMX-DRX(K))*T:SY=DRY(K)+(CMY-DRY(K))*T
58600   DX=SX-DSX(K):DY=SY-DSY(K):L=SQR(DX*DX+DY*DY)
58610   IF L>0.01 THEN S=MIN(L,1200*DT):DSX(K)=DSX(K)+DX/L*S:DSY(K)=DSY(K)+DY/L*S
58620   DRI(K)=MIN(1,DRI(K)+0.7*DT):DRL(K)=DRL(K)+DT
58630   IF DRL(K)>1.5 AND CMF=0 THEN CALL CAMDOWN
58640   IF (CMF=2 AND CMFT>1.5) OR MODE<>5 THEN DRM(K)=2:DRTM(K)=3+RND*3:DRI(K)=0:DRL(K)=0
58650 END PROC
58700 ' BL (0 .. 1) is how far John is gone: it grows while he is in a beam (faster the stronger the beam, and with two drones), shrinks when he is out of it; at 1 he collapses (BLACKOUT): he falls
58710 ' off the bicycle (the bicycle rolls on and falls by itself) or to the ground, the drones turn their beams on the camera, he lies for UTM seconds (10 .. 14) and comes to (UNCON), then no drone
58720 ' looks at him for 25 s (PROT)
58730 PROC BLACKOUT
58740   LOCAL K,DX,DY
58750   FOR K=0 TO NDR-1
58760     IF DRM(K)=3 THEN DRM(K)=4:DRL(K)=0
58770   NEXT K
58780   IF MODE=1 THEN
58790     DX=-SIN(BH*PI/180)*38:DY=COS(BH*PI/180)*38
58800     M3D THINGPOS 0,BX,BY,BZ:M3D THINGMOVE 0,DX,DY
58810     JX=M3D("TX",0):JY=M3D("TY",0):JZ=M3D("TZ",0)
58820   END IF
58830   MODE=5:UT=0:UTM=10+RND*4:JV=0:KTS=0:BL=1:CMF=0
58840   CALL SND(4):CALL SAY("JOHN COLLAPSES."):MSGT=5
58850 END PROC
58900 PROC UNCON(DT)
58910   IF MODE<>5 THEN EXIT PROC
58920   UT=UT+DT:JV=0
58930   IF CMF=0 AND UT>1.8 THEN CALL CAMDOWN
58940   IF UT>=UTM THEN
58950     MODE=0:BL=0:UT=0:PROT=25:M3D THINGPOS 0,JX,JY,JZ
58960     CMF=0:CMWY=0:CMWP=0:CMVX=0:CMVY=0:CMVZ=0
58970     CALL SAY("JOHN COMES TO."):MSGT=5
58980   END IF
58990 END PROC
59000 ' drones near John (owner, 2026-11-26): within 10 m (312 units, horizontally) a drone finds him at once; within 50 m (1600) it flies toward him with its beam on (curious); in the quiet
59005 ' time after he came to (PROT) the drones within 50 m steer away from him instead. D2: the squared distance of the drone from John
59010 PROC DRNEAR(K,D2)
59020   LOCAL L,DX,DY
59030   IF MODE=5 OR D2>2560000 THEN EXIT PROC
59040   IF PROT>0 THEN
59050     L=SQR(D2):IF L<1 THEN L=1:DX=1:DY=0 ELSE DX=(DRX(K)-JX)/L:DY=(DRY(K)-JY)/L
59055     DRTX(K)=DRX(K)+DX*3000:DRTY(K)=DRY(K)+DY*3000:DRM(K)=0:DRSC(K)=0
59060   ELSE
59070     IF D2<97656 THEN CALL DRFOUND(K) ELSE DRTX(K)=JX:DRTY(K)=JY:DRM(K)=0:DRSC(K)=-1
59080   END IF
59090 END PROC
59100 ' the camera is hit by the beam and falls (owner, 2026-11-26: the viewer is the camera drone): CMF 1 it falls (gravity, a spin, the view tips down; it bounces once), 2 it lies on the ground
59110 ' (CMFT: the seconds since it landed). CAMAGENT leaves everything to CAMFALL while CMF is not 0; UNCON sets CMF to 0 when John comes to and the camera's own spring lifts it back
59120 PROC CAMDOWN
59130   CMF=1:CMFT=0:CMFV=0:CMFS=80+RND*60:IF RND<0.5 THEN CMFS=-CMFS
59140   CALL SAY("THE CAMERA DRONE GOES DOWN.")
59150 END PROC
59200 PROC CAMFALL(DT)
59210   LOCAL FZ
59220   CMFT=CMFT+DT
59230   FZ=MAX(M3D("FLOORAT",CMX,CMY,CMZ),FN HT(CMX,CMY))+10
59240   IF CMF=1 THEN
59250     CMFV=CMFV-650*DT
59260     CMX=CMX+CMVX*DT:CMY=CMY+CMVY*DT:CMZ=CMZ+CMFV*DT
59270     CMVX=CMVX*(1-MIN(1,1.2*DT)):CMVY=CMVY*(1-MIN(1,1.2*DT))
59280     CMYW=CMYW+CMFS*DT:CMFS=CMFS*(1-MIN(1,0.8*DT))
59290     CMPT=CMPT+(-38-CMPT)*MIN(1,2.5*DT)
59300     IF CMZ<=FZ THEN
59310       CMZ=FZ
59320       IF CMFV<-140 THEN
59330         CMFV=-CMFV*0.3:CALL SND(4)
59340       ELSE
59350         CMF=2:CMFT=0:CMFV=0:CMVX=0:CMVY=0
59360       END IF
59370     END IF
59380   ELSE
59390     CMZ=FZ:CMYW=CMYW+CMFS*DT:CMFS=CMFS*(1-MIN(1,2*DT))
59400   END IF
59410   M3D CAMERA CMX,CMY,CMZ-46,CMYW,CMPT
59420   DSV=SQR((CMX-JX)^2+(CMY-JY)^2+(CMZ-JZ-36)^2)
59430 END PROC
59500 ' the veil: the vision going dark, as a quad in front of the camera that is translucent black. The palette entries 2 .. 15 (levels 1 .. 14: black with the alpha 17 .. 238, VEILPAL; the
59510 ' game's colors start at 16, W3K_PALETTE leaves them alone; set before the color map is made) are the veil's; level 15 is the opaque one (entry 1, black). Object 720, made new each frame while the veil is there. V: 0 until the camera lies on the ground (CMF=2), then 1 in 0.8 s, 1 until 1.6 s, 0.62 from 2.6 s,
59520 ' while he lies, then fading out in the last 3 seconds. G1987_VEILTEST=level (0 .. 15) holds a level for a picture test
59530 PROC VEILPAL
59540   LOCAL K
59550   FOR K=1 TO 14
59560     GFX PALETTE 0,1+K,RGB(0,0,0,K*17):GFX PALETTE 3,1+K,RGB(0,0,0,K*17)
59570   NEXT K
59580 END PROC
59600 PROC VEIL
59610   LOCAL V,L,X,Y,Z,CY,SY,CP,SP,FX,FY,FZ,RX,RY,UX,UY,UZ,CX,CZ,F
59620   V=0
59630   IF CMF=2 THEN
59640     V=MIN(1,CMFT/0.8)
59650     IF CMFT>1.6 THEN V=0.62+0.38*MAX(0,1-(CMFT-1.6))
59660     IF MODE=5 AND UT>UTM-3 THEN V=V*MAX(0,(UTM-UT)/3)
59670   END IF
59680   IF VT>=0 THEN V=VT/15
59690   L=INT(V*15+0.5):IF L>15 THEN L=15
59700   IF L<=0 THEN
59710     IF VLV<>0 THEN M3D OBJECT 720,62,235:VLV=0
59720     EXIT PROC
59730   END IF
59740   VLV=L
59750   X=M3D("X"):Y=M3D("Y"):Z=M3D("Z")+46
59760   CY=COS(M3D("YAW")*PI/180):SY=SIN(M3D("YAW")*PI/180):CP=COS(M3D("PITCH")*PI/180):SP=SIN(M3D("PITCH")*PI/180)
59770   FX=CP*CY:FY=CP*SY:FZ=SP:RX=SY:RY=-CY:UX=-SP*CY:UY=-SP*SY:UZ=CP
59780   CX=X+3*FX:CY=Y+3*FY:CZ=Z+3*FZ
59790   F=62:IF L>=15 THEN F=60:L=0
59800   M3D OBJECT 720,F,255
59810   M3D VERTEX 720,CX-40*RX-40*UX,CY-40*RY-40*UY,CZ-40*UZ
59820   M3D VERTEX 720,CX+40*RX-40*UX,CY+40*RY-40*UY,CZ-40*UZ
59830   M3D VERTEX 720,CX+40*RX+40*UX,CY+40*RY+40*UY,CZ+40*UZ
59840   M3D VERTEX 720,CX-40*RX+40*UX,CY-40*RY+40*UY,CZ+40*UZ
59850   M3D QUAD 720,0,1,2,3,1+L
59860 END PROC
60000 ' ------------------------------------------------------------------------------------------------ day and night
60010 ' (owner, 2026-11-26) A day lasts DAYSEC seconds (24 minutes; G1987_DAYLEN), the game starts at 10 o'clock (G1987_TIME). DAYH is the hour 0 .. 24 (kept in player.dat). The daylight DAYAMB(h) is the sun's height
60020 ' as a factor (0.16 at night, 1 by day, a short dusk and dawn round 6 and 18 o'clock): M3D AMBIENT darkens every object, the things and the sky (not the objects with the glow flag 32: the drones' beams, the
60030 ' shadows, the veil) before the lamps add theirs; the haze of the fog (M3D FOG) gets a darker color of the same hue (DAYLV). The bicycle has a headlamp (M3D LAMP 0: a cone of 24 degrees, 3000 units
60040 ' long, tilted down a little) that glows as the daylight gets low (below 85 %) while John rides. Pickups (a flashlight) come later.
60100 FUNC DAYAMB(H)
60110   LOCAL S,X
60120   S=SIN((H-6)*PI/12)
60130   X=(S+0.25)/0.6:IF X<0 THEN X=0
60140   IF X>1 THEN X=1
60150   RETURN 0.16+0.84*X*X*(3-2*X)
60160 END FUNC
60300 PROC DAYNIGHT(DT)
60310   LOCAL A,LV,DX,DY,LA,T,C,S,L
60320   DAYH=DAYH+DT*24/DAYSEC:IF DAYH>=24 THEN DAYH=DAYH-24
60330   A=FN DAYAMB(DAYH):DAYA=A
60340   M3D AMBIENT A
60350   LV=INT(2+12*(A-0.16)/0.84+0.5)
60360   IF LV<>DAYLV THEN DAYLV=LV:M3D FOG 156000,FN CC(8,LV),100000
60400   ' the bicycle's lights: a dynamo on the wheel makes the current, so they glow brighter the faster John rides (DYN 0 .. 1) and go out when he stops; the white headlamp
60402   ' throws a cone, the red tail lamp is a point light (the lenses are the glowing objects 515 and 516); lights are on in the dusk and the dark, while John rides
60410   LA=(0.85-A)/0.3:IF LA>1 THEN LA=1
60412   T=(ABS(BV)-8)/110:IF T<0 THEN T=0
60414   IF T>1 THEN T=1
60416   DYN=DYN+(T-DYN)*MIN(1,10*DT)
60418   IF DT=0 THEN DYN=T
60419   IF DYNF>=0 THEN DYN=DYNF
60420   IF MODE=1 AND BST=0 AND LA>0 THEN
60422     IF LENSON=0 THEN LENSON=-1:M3D OBJSHOW 515,1:M3D OBJSHOW 516,1
60424     C=COS(BH*PI/180):S=SIN(BH*PI/180)
60426     L=40+215*DYN:M3D OBJLIGHT 515,L:M3D OBJLIGHT 516,L
60428     M3D OBJPOS 515,BX+12*C,BY+12*S,BZ,BH+BSA:M3D OBJPOS 516,BX,BY,BZ,BH
60430     IF DYN>0.02 THEN
60432       DX=COS((BH+BSA)*PI/180):DY=SIN((BH+BSA)*PI/180)
60434       M3D LAMP 0,BX+12*C+DX*6,BY+12*S+DY*6,BZ+25,1000+2000*DYN,255*LA*DYN,DX,DY,-0.14,24
60436       M3D LAMP 1,BX-18*C,BY-18*S,BZ+30,260,150*LA*DYN
60438       LAMPON=-1
60440     ELSE
60442       IF LAMPON THEN M3D LAMP 0,0,0,0,1,0:M3D LAMP 1,0,0,0,1,0:LAMPON=0
60444     END IF
60450   ELSE
60460     IF LENSON THEN LENSON=0:M3D OBJSHOW 515,0:M3D OBJSHOW 516,0
60470     IF LAMPON THEN M3D LAMP 0,0,0,0,1,0:M3D LAMP 1,0,0,0,1,0:LAMPON=0
60480   END IF
60490 END PROC
60500 ' the lenses of the lamps: octagons (the head lens in the front's coordinates, 4.4 in front of the steering axis, the tail lens in the frame's coordinates behind the carrier)
60510 PROC BUILDLENS
60520   LOCAL I,A
60530   M3D OBJECT 515,52,255:M3D VERTEX 515,4.4,0,26
60540   FOR I=0 TO 7:A=I*PI/4:M3D VERTEX 515,4.4,2.3*COS(A),26+2.3*SIN(A):NEXT I
60550   FOR I=1 TO 8:M3D FACE 515,0,I,1+(I MOD 8),FN CC(0,15):NEXT I
60560   M3D OBJECT 516,52,255:M3D VERTEX 516,-16.4,0,30
60570   FOR I=0 TO 7:A=I*PI/4:M3D VERTEX 516,-16.4,1.4*COS(A),30+1.4*SIN(A):NEXT I
60580   FOR I=1 TO 8:M3D FACE 516,0,I,1+(I MOD 8),FN CC(1,14):NEXT I
60590   M3D OBJSHOW 515,0:M3D OBJSHOW 516,0
60600 END PROC
