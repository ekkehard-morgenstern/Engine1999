1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
5 ' the helpers that the demo games share are in the libraries demos/lib/util.lib, input.lib, hiscore.lib and text.lib
6 LOAD LIBRARY "util"
7 LOAD LIBRARY "input"
8 LOAD LIBRARY "hiscore"
9 LOAD LIBRARY "text"
10 ' MADRUN II - the second edition of the crossing game for one to four players, for Engine1999: a traffic helicopter and pigeons that run on sprite paths.
20 ' A telephone booth stands on the island in the middle of a busy crossing, and the phone is ringing! Get there before the caller hangs up.
30 ' Cars, trucks, buses, motorbikes and (later) ambulances drive on sixteen lanes (two in each direction on each road). They stop for each other and for the crossing, but never for you.
40 ' The four corners and the blocks between the roads are safe. If a vehicle hits you, you start again in a corner. The first player at the booth
50 ' scores the level (more points for a fast answer), then the next level starts with more and faster traffic. A call you miss costs everybody a life.
60 ' Keyboard: player 1 WASD (with one player the cursor keys work, too), player 2 the cursor keys. Controllers: players 1 to 4, left stick or D-pad;
70 ' a player appears when a pad is connected and goes away when it is removed. Everywhere: P pauses (START on a pad), Q quits (BACK on a pad).
80 ' Pick-ups: whistle (the traffic stops for five seconds), boots (fast for ten seconds), heart (an extra life). A close call scores 5 points.
90 ' What this program shows of the language:
100 '   * sprites made at start-up: the pictures are drawn on the graphics layer with GFX LINE, grabbed with GFX GRAB and defined with SPRITE DEF
110 '   * a traffic simulation with arrays (a lane table, a leader for each vehicle), reservations of the crossings, braking from the stopping distance
120 '   * four players on the keyboard (KEY) and on game controllers (JOY, JOY("CONNECTED"): players come and go while the game runs)
130 '   * PROC / FUNC / LOCAL, text screen areas for the status lines, a map drawn once on the background layer, SFX and PLAY, a high score file
131 '   * NEW in II: a traffic helicopter of 4 x 3 sprites (SPRITE n,SIZE 4,3) with a translucent spinning rotor and a translucent shadow flies over the crossing on a PATH (SPRITE n,PATH a$)
132 '     now and then, and pigeons walk about on the grass blocks on paths of their own; the program does not move any of them, it only reads the events (E n) the paths report with SPRITE("EVENT")
140 ' --- global variables ---------------------------------------------------------------------------------------------------------------------
150 ' GM 1 keyboard / 2 controllers, NK players on the keyboard, LEVEL, TST 0 -- 1 ringing / 2 answered / 3 too late, TLEFT frames left, FRZ traffic stopped
160 ' PST(p) 0 absent / 1 playing / 2 hit / 3 out, PX,PY position, LV lives, SCP score, PLD(p) has taken part, DXI,DYI the direction wanted
170 ' V... the vehicles 1..NV: VK kind, VL lane, VS position of the front along the lane, VSP speed, VMX top speed, VLN length, VLD the vehicle ahead,
180 ' VR the crossing reserved + 1; lanes 0..7 (LDIR, LAX axis, LPOS, LLEN, CA crossing entries, CID crossing numbers); OCC(crossing, axis) occupied
190 ' --------------------------------------------------------------------------------------------------------------------------------------------
200 ' (a FUNC has to be defined before the lines that call it)
570 ' ====================================================================== main program
580 CALL INIT
590 HI=FN HS_LOAD("madrun2.hi")
660 QT=0
670 REPEAT
680   CALL TITLE
690   IF QT=0 THEN
700     CALL PLAYGAME
710     IF BEST>HI THEN
720       HI=BEST
730       ON ERROR GOTO HIERR2
740       CALL HS_SAVE("madrun2.hi",HI)
770       LABEL HIDONE2
780       ON ERROR OFF
790     END IF
800   END IF
810 UNTIL QT=1
820 CALL TX_BYE("MADRUN II")
830 END
840 ' the library hiscore reads the high score (a missing or empty file is 0, no error); a file that can't be written is ignored: the error handler is here in the main program, where RESUME <label> can continue (inside a PROC the frame would be gone)
880 LABEL HIERR2
890 RESUME HIDONE2
891 LABEL MUSICTIMER
892 CALL MBAR
893 RETURN
900 ' ====================================================================== set up (once)
910 PROC INIT
920   LOCAL K
930   RANDOMIZE
940   GFX RESET ALL
950   CLS
960   ' (DIM n makes n elements, 0 to n-1)
970   DIM VK(100),VVR(100),VLN(100),VWD(100),VL(100),VS(100),VSP(100),VMX(100),VLD(100),VR(100),VX1(100),VY1(100),VX2(100),VY2(100),VSTL(100)
980   DIM OCC(4,2),WT(4,2),WN(4,2),LDIR(16),LAX(16),LPOS(16),LLEN(16),LSPF(16),CA(16,2),CID(16,2),IMGB(5,6,2)
990   DIM NVAR(5),NSEG(5),VLEN(5),VWID(5),VSPF(5),SPX(5),SPY(5),PSPX(8),PSPY(8),TS(9)
1000   DIM PST(5),PX(5),PY(5),PHT(5),PINV(5),PBT(5),SCP(5),LV(5),PLD(5),NXL(5),PWF(5),DXI(5),DYI(5),PCOL(5),NM(5,100)
1005   DIM AMS(5),AMX(5),AMD(5),AMT(5)
1010   ' kinds of vehicle: variants, sprites (16 pixels each), length, width, speed factor
1020   FOR K=0 TO 4
1030     READ NVAR(K),NSEG(K),VLEN(K),VWID(K),VSPF(K)
1040   NEXT K
1050   FOR K=1 TO 4
1060     READ SPX(K),SPY(K)
1070   NEXT K
1080   FOR K=0 TO 7
1090     READ PSPX(K),PSPY(K)
1100   NEXT K
1110   PCOL(1)=203:PCOL(2)=208:PCOL(3)=204:PCOL(4)=201
1120   GAP=4:ZONE=56:STOPGAP=5:DCL=0.12:ACL=0.05
1130   NV=0:FRM=0:GM=1:NK=1:GQ=0
1140   CALL PALETTES
1150   CALL SOUNDS
1155   CALL SOUNDS2
1160   CALL LANES
1170   CALL MAKEIMAGES
1180   CALL DRAWMAP
1190   FOR K=0 TO 3
1200     SPRITE K,PRIORITY 6
1210   NEXT K
1220   SPRITE 4,PRIORITY 2
1230   SPRITE 5,PRIORITY 5
1240   FOR K=6 TO 9
1250     SPRITE K,PRIORITY 5
1260   NEXT K
1270   FOR K=12 TO 191
1280     SPRITE K,PRIORITY 3
1290   NEXT K
1300   SPRITE 4,ON:SPRITE 10,PRIORITY 2:SPRITE 10,ON
1301   FOR K=192 TO 207
1302     SPRITE K,PRIORITY 2:SPRITE K,OFF
1303   NEXT K
1304   RINGT=0:ANSP=0:ABON=0:PMO=0
1305   FOR K=208 TO 211:SPRITE K,PRIORITY 5:SPRITE K,OFF:NEXT K
1306   HKT=0:ANYAMB=0:MUSON=0
1309   CALL AMBSTART
1310 END PROC
1320 PROC SOUNDS
1340   SFX 2,"W3 V0.7 ML X0,0.005,0.35,0.3 S1400,70,4"
1350   SFX 3,"W1 V0.4 ML H784,6 H988,6 H1319,8"
1360   SFX 4,"W2 V0.3 ML H196,10 H220,12"
1370   SFX 5,"W1 V0.3 ML H523,5 H659,5"
1380   SFX 6,"W1 V0.4 ML H523,6 H659,6 H784,6 H1047,10"
1390   SFX 7,"W3 V0.4 ML X0,0.01,0.3,0.4 S900,100,24"
1400   SFX 8,"W1 V0.35 ML ~12,0.5 H2200,16 H1900,16"
1410   SFX 9,"W2 V0.2 ML H330,3"
1411   SFX 1,"W3 V0.5 X0,0.003,0.2,0.012 H2600,64 H700,32 H1400,64 R64 H300,32"
1412   SFX 10,"W3 V0.5 X0,0.004,0.1,0.01 H900,32 H400,16"
1413   SFX 12,"W3 V0.16 X0,0.002,0,0.02 H700,64"
1414   SFX 13,"W3 V0.16 X0,0.002,0,0.02 H900,64"
1415   SFX 14,"W3 V0.16 X0,0.002,0,0.02 H1100,64"
1416   SFX 15,"W3 V0.16 X0,0.002,0,0.02 H1400,64"
1420 END PROC
1421 PROC RING
1422   ' The telephone: a real bell, as measured on a recording of an old telephone: three partials that are not harmonics of each other (1290,
1423   ' 500 and 3360 Hz), a clapper that strikes about 20 times a second (the slight tremolo), 2 seconds of ringing. The 1290 and 3360 Hz
1424   RINGT=125:PLAY "W0 V0.3 ML ^20,0.15 X0.005,0,1,1.3 H1290,1 R1 XR ^0,0","W0 V0.28 ML ~15,1.5 ^20,0.5 X0.005,0,1,0.03 H500,1 R1 XR ~0,0 ^0,0","W0 V0.09 ML ^20,0.2 X0.005,0,1,1.6 H3360,1 R1 XR ^0,0"
1425 END PROC
1426 PROC HANGUP
1427   SOUND OFF 1:SOUND OFF 2:SOUND OFF 3
1428   ENVELOPE 1:ENVELOPE 2:ENVELOPE 3:TREMOLO 1,0,0:TREMOLO 2,0,0:TREMOLO 3,0,0:VIBRATO 2,0,0
1429 END PROC
1430 ' ====================================================================== colors
1440 PROC PALETTES
1450   LOCAL I,R,GG,BL
1460   ' sprites (layer 4): 1 black, 2 glass, 3 headlight, 4 red, 5 white, 6 grey, 7..12 car colors, 13..15 trailers, 16 bus, 18 skin, 19 hair,
1470   ' 20..23 shirts of the players, 24 trousers, 25 shoes, 26..29 booth, 30 dark grey, 31 blue light, 32 gold, 33 brown, 34 silver
1480   FOR I=1 TO 34
1490     READ R,GG,BL
1500     GFX PALETTE 4,I,RGB(R,GG,BL)
1510   NEXT I
1520   ' background (layer 0): 1 asphalt, 2 speck, 3 sidewalk, 4 curb, 5 grass, 6 dark grass, 7 white, 8 yellow, 9 plaza, 10..12 flowers, 13..15 tree,
1530   ' 16 plaza edge, 17..20 the colors of the players
1540   FOR I=1 TO 20
1550     READ R,GG,BL
1560     GFX PALETTE 0,I,RGB(R,GG,BL)
1570   NEXT I
1580   GFX PALETTE 3,1,RGB(255,255,255):GFX PALETTE 3,2,RGB(20,20,40):GFX PALETTE 3,3,RGB(255,200,30):GFX PALETTE 3,4,RGB(255,90,60)
1590   GFX PALETTE 3,5,RGB(120,220,255):GFX PALETTE 3,250,RGB(0,0,20,185)
1600   GFX PALETTE 2,200,RGB(255,255,255):GFX PALETTE 2,201,RGB(255,210,50):GFX PALETTE 2,202,RGB(120,220,255):GFX PALETTE 2,203,RGB(255,100,90)
1610   GFX PALETTE 2,204,RGB(140,255,140):GFX PALETTE 2,205,RGB(190,200,230):GFX PALETTE 2,206,RGB(255,150,210):GFX PALETTE 2,207,RGB(255,170,40)
1620   GFX PALETTE 2,208,RGB(110,160,255):GFX PALETTE 2,210,RGB(0,0,20,150)
1630 END PROC
1640 ' the eight lanes: road row 0/1 (y 64, y 204), road column 0/1 (x 200, x 408); every lane has two crossings, CA = where the front enters one,
1650 ' CID = its number (row * 2 + column); a lane is a circle: 64 pixels longer than the screen so that vehicles wrap while out of sight
1660 PROC LANES
1667   LOCAL K,R,C,J
1674   ' 16 lanes: each of the four roads is 64 pixels wide with two lanes per direction (lanes 0 and 1 of a road drive west / south, 2 and 3 east / north)
1681   ' LAX 0 = along x (the horizontal roads, lanes 0..7), 1 = along y (the vertical roads, lanes 8..15); CA the places where the lane meets a crossing
1688   FOR K=0 TO 15
1695     IF K<8 THEN
1702       R=K\4:J=K MOD 4
1709       LAX(K)=0:LLEN(K)=704
1716       LPOS(K)=48+R*140+J*16
1723       CA(K,0)=200:CA(K,1)=440
1730       IF J<2 THEN
1737         LDIR(K)=1:CID(K,0)=R*2+1:CID(K,1)=R*2
1744       ELSE
1751         LDIR(K)=0:CID(K,0)=R*2:CID(K,1)=R*2+1
1758       END IF
1765     ELSE
1772       C=(K-8)\4:J=(K-8) MOD 4
1779       LAX(K)=1:LLEN(K)=364
1786       LPOS(K)=168+C*240+J*16
1793       CA(K,0)=80:CA(K,1)=220
1800       IF J<2 THEN
1807         LDIR(K)=2:CID(K,0)=C:CID(K,1)=2+C
1814       ELSE
1821         LDIR(K)=3:CID(K,0)=2+C:CID(K,1)=C
1828       END IF
1835     END IF
1842     ' the lanes next to the middle line are the fast ones
1849     LSPF(K)=1
1856     IF J=1 OR J=2 THEN LSPF(K)=1.12
1863   NEXT K
1870 END PROC
1900 ' ====================================================================== pictures
1910 ' a rectangle in vehicle coordinates: u along the vehicle (the front is 15), v across (0 to 9); DD = direction 0 E, 1 W, 2 S, 3 N
1920 PROC RR(U1,V1,U2,V2,C)
1930   LOCAL X1,Y1,X2,Y2
1940   IF DD=0 THEN X1=U1:X2=U2:Y1=V1+7:Y2=V2+7
1950   IF DD=1 THEN X1=15-U1:X2=15-U2:Y1=V1+7:Y2=V2+7
1960   IF DD=2 THEN X1=V1+3:X2=V2+3:Y1=U1:Y2=U2
1970   IF DD=3 THEN X1=V1+3:X2=V2+3:Y1=15-U1:Y2=15-U2
1980   GFX LINE X1,Y1,X2,Y2,C,BF
1990 END PROC
2000 PROC DRAWVEH(KD,VR2,PT)
2010   LOCAL C
2020   IF KD=0 THEN
2030     C=7+VR2
2040     CALL RR(0,0,15,9,1)
2050     CALL RR(1,1,14,8,C)
2060     CALL RR(10,2,12,7,2)
2070     CALL RR(4,2,6,7,2)
2080     CALL RR(15,1,15,2,3):CALL RR(15,7,15,8,3)
2090     CALL RR(0,1,0,2,4):CALL RR(0,7,0,8,4)
2100   END IF
2110   IF KD=1 AND PT=0 THEN
2120     C=7+VR2
2130     CALL RR(0,0,15,9,1)
2140     CALL RR(1,1,14,8,C)
2150     CALL RR(9,2,12,7,2)
2160     CALL RR(0,0,0,9,30)
2170     CALL RR(15,1,15,2,3):CALL RR(15,7,15,8,3)
2180   END IF
2190   IF KD=1 AND PT=1 THEN
2200     C=13+VR2
2210     CALL RR(0,0,15,9,1)
2220     CALL RR(1,1,14,8,C)
2230     CALL RR(5,1,5,8,6):CALL RR(9,1,9,8,6):CALL RR(13,1,13,8,6)
2240     CALL RR(0,1,0,2,4):CALL RR(0,7,0,8,4)
2250   END IF
2260   IF KD=2 THEN
2270     C=16
2280     IF VR2=1 THEN C=9
2290     CALL RR(0,0,15,9,1)
2300     CALL RR(1,1,14,8,C)
2310     CALL RR(3,3,5,6,6):CALL RR(8,3,10,6,6)
2320     IF PT=0 THEN
2330       CALL RR(12,2,13,7,2)
2340       CALL RR(15,1,15,2,3):CALL RR(15,7,15,8,3)
2350     ELSE
2360       CALL RR(1,2,1,7,2)
2370       CALL RR(0,1,0,2,4):CALL RR(0,7,0,8,4)
2380     END IF
2390   END IF
2400   IF KD=3 THEN
2410     C=20+VR2
2420     CALL RR(14,4,15,5,1)
2430     CALL RR(4,4,5,5,1)
2440     CALL RR(6,4,13,5,30)
2450     CALL RR(7,2,10,7,C)
2460     CALL RR(10,4,11,5,19)
2470     CALL RR(15,4,15,5,3)
2480   END IF
2490   IF KD=4 THEN
2500     CALL RR(0,0,15,9,1)
2510     CALL RR(1,1,14,8,5)
2520     CALL RR(6,2,8,7,4):CALL RR(4,4,10,5,4)
2530     CALL RR(11,2,13,7,2)
2540     IF VR2=0 THEN
2550       CALL RR(9,1,10,2,4):CALL RR(9,7,10,8,31)
2560     ELSE
2570       CALL RR(9,1,10,2,31):CALL RR(9,7,10,8,4)
2580     END IF
2590     CALL RR(15,1,15,2,3):CALL RR(15,7,15,8,3)
2600     CALL RR(0,1,0,2,4):CALL RR(0,7,0,8,4)
2610   END IF
2620 END PROC
2630 ' every picture is drawn on the graphics layer (hidden meanwhile), grabbed (4 bytes of size, then 16 x 24 pixels) and defined as a sprite
2640 PROC GRABSPRITE(IM)
2650   GFX GRAB 0,0,15,23,G$
2660   SPRITE DEF IM,MID$(G$,5)
2670 END PROC
2680 PROC MAKEIMAGES
2690   LOCAL KD,VR2,PT,D,N,P,F
2700   GFX LAYER 3,OFF
2710   GFX TARGET 3
2720   N=0
2730   FOR KD=0 TO 4
2740     FOR VR2=0 TO NVAR(KD)-1
2750       FOR PT=0 TO NSEG(KD)-1
2760         IMGB(KD,VR2,PT)=N
2770         FOR D=0 TO 3
2780           DD=D
2790           GFX LINE 0,0,15,23,0,BF
2800           CALL DRAWVEH(KD,VR2,PT)
2810           CALL GRABSPRITE(N*4+D)
2820         NEXT D
2830         N=N+1
2840       NEXT PT
2850     NEXT VR2
2860   NEXT KD
2870   FOR P=1 TO 4
2880     FOR F=0 TO 3
2890       CALL MKPLAYER(P,F)
2900       CALL GRABSPRITE(100+(P-1)*4+F)
2910     NEXT F
2920   NEXT P
2930   FOR F=0 TO 4
2931     CALL MKBOOTH(F)
2932     CALL GRABSPRITE(126+F)
2933   NEXT F
2934   FOR P=1 TO 4
2935     FOR F=4 TO 6
2936       CALL MKPLAYER(P,F)
2937       CALL GRABSPRITE(131+(P-1)*3+F-4)
2938     NEXT F
2939   NEXT P
2940   FOR F=0 TO 7
2941     CALL MKRAY(F)
2942     CALL GRABSPRITE(143+F)
2943   NEXT F
2970   FOR F=1 TO 3
2980     CALL MKICON(F)
2990     CALL GRABSPRITE(121+F)
3000   NEXT F
3010   GFX LINE 0,0,15,23,0,BF
3020   GFX TEXT 1,1,"?",1,2,2
3030   GFX TEXT 0,0,"?",32,2,2
3040   CALL GRABSPRITE(125)
3050   GFX CLEAR
3060   GFX LAYER 3,ON
3070 END PROC
3080 ' a walking person (FR 0 stands, 1 and 2 walk, 3 has been hit)
3090 PROC MKPLAYER(P2,FR)
3100   LOCAL SH,LO,RO
3110   SH=19+P2
3120   GFX LINE 0,0,15,23,0,BF
3130   IF FR=3 THEN
3140     GFX LINE 0,14,5,20,1,BF
3150     GFX LINE 1,15,4,19,18,BF
3160     GFX LINE 1,15,4,16,19,BF
3170     GFX LINE 5,13,11,21,1,BF
3180     GFX LINE 6,14,10,20,SH,BF
3190     GFX LINE 11,14,15,17,24,BF
3200     GFX LINE 11,18,15,21,24,BF
3210     GFX LINE 3,4,7,4,32:GFX LINE 5,2,5,6,32
3220     GFX LINE 10,2,14,2,32:GFX LINE 12,0,12,4,32
3230     GFX LINE 1,9,3,9,32:GFX LINE 2,8,2,10,32
3240   ELSE
3250     LO=0:RO=0
3260     IF FR=1 THEN LO=2
3270     IF FR=2 THEN RO=2
3280     GFX LINE 5,0,10,7,1,BF
3290     GFX LINE 6,2,9,6,18,BF
3300     GFX LINE 6,1,9,2,19,BF
3310     GFX PSET 6,4,1:GFX PSET 9,4,1
3320     GFX LINE 3,8,12,15,1,BF
3330     GFX LINE 4,9,11,14,SH,BF
3340     GFX LINE 1,8,2,13,1,BF:GFX LINE 13,8,14,13,1,BF
3350     GFX LINE 1,9,1,11,SH,BF:GFX LINE 14,9,14,11,SH,BF
3360     GFX LINE 1,12,2,13,18,BF:GFX LINE 13,12,14,13,18,BF
3370     GFX LINE 4,16,7,20-LO,24,BF:GFX LINE 8,16,11,20-RO,24,BF
3380     GFX LINE 4,21-LO,7,23-LO,25,BF:GFX LINE 8,21-RO,11,23-RO,25,BF
3381   IF FR>=4 THEN
3382     GFX LINE 0,0,3,13,0,BF
3383     GFX LINE 0,5,2,13,1,BF:GFX LINE 1,9,1,13,SH:GFX LINE 1,6,1,8,18
3384     IF FR=4 THEN GFX LINE 0,2,2,5,1,BF:GFX LINE 1,3,1,4,18
3385     IF FR>=5 THEN GFX LINE 2,0,4,8,1,BF:GFX LINE 3,1,3,7,6:GFX LINE 0,4,2,6,1,BF:GFX PSET 1,5,18
3386     IF FR=5 THEN GFX LINE 7,6,8,6,19
3387     IF FR=6 THEN GFX LINE 7,5,8,6,1,BF
3388   END IF
3390   END IF
3400 END PROC
3410 PROC MKBOOTH(FR)
3413   LOCAL I
3416   GFX LINE 0,0,15,23,0,BF
3419   ' the roof and the lit sign (the same in both halves)
3422   GFX LINE 0,0,15,0,26:GFX LINE 0,1,15,3,27,BF:GFX LINE 0,4,15,6,5,BF
3425   GFX LINE 0,7,15,7,27
3428   FOR I=1 TO 13 STEP 4
3431     GFX LINE I,5,I+1,5,27
3434   NEXT I
3437   IF FR=4 THEN
3440     ' the right half: the open door (swung out towards the viewer: a panel that gets taller to the right), the inside left of it
3441     GFX LINE 0,8,7,21,30,BF:GFX LINE 0,22,7,23,6,BF
3442     GFX LINE 8,8,9,23,27,BF:GFX LINE 8,8,8,23,26
3443     FOR I=10 TO 15
3444       GFX LINE I,8-(I-10)\2,I,23,27
3445       GFX LINE I,10-(I-10)\2,I,21,26
3446     NEXT I
3447     GFX LINE 11,12,14,15,28,BF:GFX LINE 11,17,14,20,28,BF
3448     GFX LINE 11,12,11,14,5:GFX LINE 11,17,11,19,5
3449     GFX PSET 10,16,16
3461   ELSE
3464     ' the left half: the post, the inside wall and the coin telephone with its receiver
3467     GFX LINE 0,8,1,23,27,BF:GFX LINE 0,8,0,23,26
3470     GFX LINE 2,8,15,21,30,BF:GFX LINE 2,22,15,23,6,BF
3473     GFX LINE 5,11,13,20,1,BF:GFX LINE 6,12,12,19,6,BF
3476     GFX LINE 6,12,12,12,17
3479     GFX LINE 10,13,11,15,16,BF:GFX PSET 10,16,1:GFX PSET 11,16,1
3482     GFX PSET 7,17,17:GFX PSET 9,17,17:GFX PSET 7,19,17:GFX PSET 9,19,17:GFX PSET 11,18,17
3485     IF FR=0 THEN
3488       GFX LINE 3,8,5,11,1,BF:GFX LINE 13,8,15,11,1,BF
3491       GFX LINE 6,9,12,10,1,BF:GFX LINE 6,9,12,9,17
3494     END IF
3497     IF FR=1 THEN
3500       ' the receiver jumps up and tilts to the left (exaggerated)
3503       GFX LINE 2,3,4,7,1,BF:GFX LINE 13,7,15,10,1,BF
3506       GFX LINE 5,6,13,8,1:GFX LINE 5,7,13,9,1:GFX LINE 5,5,13,7,17
3509     END IF
3512     IF FR=2 THEN
3515       GFX LINE 3,7,5,10,1,BF:GFX LINE 13,2,15,6,1,BF
3518       GFX LINE 5,8,13,5,1:GFX LINE 5,9,13,6,1:GFX LINE 5,7,13,4,17
3521     END IF
3524     IF FR=3 THEN
3527       ' off the hook: the empty cradle
3530       GFX LINE 4,9,5,10,6,BF:GFX LINE 12,9,13,10,6,BF
3533       GFX LINE 9,10,9,12,1
3536     END IF
3539   END IF
3542 END PROC
3550 ' the pick-ups: 1 whistle, 2 boots, 3 heart
3560 PROC MKICON(F)
3570   GFX LINE 0,0,15,23,0,BF
3580   IF F=1 THEN
3590     GFX LINE 2,8,11,14,1,BF
3600     GFX LINE 3,9,10,13,34,BF
3610     GFX LINE 10,10,15,12,1,BF:GFX LINE 11,11,14,11,34
3620     GFX LINE 5,10,6,11,1,BF
3630     GFX LINE 0,6,3,9,32:GFX LINE 1,6,1,9,32
3640   END IF
3650   IF F=2 THEN
3660     GFX LINE 3,5,8,15,1,BF:GFX LINE 4,6,7,15,33,BF
3670     GFX LINE 3,14,14,19,1,BF:GFX LINE 4,15,13,18,33,BF
3680     GFX LINE 2,19,14,20,1,BF
3690     GFX LINE 5,8,6,9,32:GFX LINE 5,11,6,12,32
3700   END IF
3710   IF F=3 THEN
3720     GFX LINE 2,6,6,10,1,BF:GFX LINE 9,6,13,10,1,BF
3730     GFX LINE 1,9,14,12,1,BF:GFX LINE 2,13,13,14,1,BF:GFX LINE 4,15,11,16,1,BF:GFX LINE 6,17,9,18,1,BF
3740     GFX LINE 3,7,5,9,4,BF:GFX LINE 10,7,12,9,4,BF
3750     GFX LINE 2,10,13,12,4,BF:GFX LINE 3,13,12,13,4:GFX LINE 5,14,10,15,4,BF:GFX LINE 7,16,8,17,4,BF
3760     GFX LINE 3,7,4,8,5,BF
3770   END IF
3780 END PROC
3790 ' ====================================================================== the map (background layer)
3800 PROC BLOCK(X1,Y1,X2,Y2)
3810   LOCAL I,X,Y
3820   GFX LINE X1,Y1,X2,Y2,3,BF
3830   GFX LINE X1,Y1,X2,Y2,4,B
3840   GFX LINE X1+7,Y1+7,X2-7,Y2-7,5,BF
3850   FOR I=1 TO (X2-X1)*(Y2-Y1)\400
3860     X=X1+9+FN RN(X2-X1-18):Y=Y1+9+FN RN(Y2-Y1-18)
3870     GFX LINE X,Y,X+1+FN RN(3),Y+1,6,BF
3880   NEXT I
3890   FOR I=1 TO (X2-X1)*(Y2-Y1)\500
3900     GFX PSET X1+9+FN RN(X2-X1-18),Y1+9+FN RN(Y2-Y1-18),10+FN RN(3)
3910   NEXT I
3920 END PROC
3930 PROC TREE(X,Y)
3940   GFX CIRCLE X,Y,9,13
3950   GFX PAINT X,Y,14,13
3960   GFX CIRCLE X-2,Y-2,3,15
3970 END PROC
3980 PROC DRAWMAP
3988   LOCAL I,X,Y,R,C,X0,Y0
3996   GFX TARGET 0
4004   GFX CLEAR 5
4012   ' the roads (64 pixels wide: two lanes in each direction), two across and two down
4020   GFX LINE 0,48,639,111,1,BF:GFX LINE 0,188,639,251,1,BF
4028   GFX LINE 168,0,231,299,1,BF:GFX LINE 408,0,471,299,1,BF
4036   FOR I=1 TO 700
4044     GFX PSET FN RN(640),FN RN(300),2
4052   NEXT I
4060   ' the nine blocks between the roads
4068   FOR R=0 TO 2
4076     FOR C=0 TO 2
4084       X0=0:IF C=1 THEN X0=232
4092       IF C=2 THEN X0=472
4100       Y0=0:IF R=1 THEN Y0=112
4108       IF R=2 THEN Y0=252
4116       X=167:IF C=1 THEN X=407
4124       IF C=2 THEN X=639
4132       Y=47:IF R=1 THEN Y=187
4140       IF R=2 THEN Y=299
4148       CALL BLOCK(X0,Y0,X,Y)
4156     NEXT C
4164   NEXT R
4172   ' the dashed lines between the lanes of one direction and the double line between the directions (not in the crossings)
4180   FOR R=0 TO 1
4188     Y0=48+R*140
4196     FOR X=0 TO 639 STEP 24
4204       IF NOT (X+11>=164 AND X<=235) AND NOT (X+11>=404 AND X<=475) THEN
4212         GFX LINE X,Y0+15,X+11,Y0+16,7,BF:GFX LINE X,Y0+47,X+11,Y0+48,7,BF
4220       END IF
4228     NEXT X
4236     GFX LINE 0,Y0+31,163,Y0+32,8,BF:GFX LINE 236,Y0+31,403,Y0+32,8,BF:GFX LINE 476,Y0+31,639,Y0+32,8,BF
4244   NEXT R
4252   FOR C=0 TO 1
4260     X0=168+C*240
4268     FOR Y=0 TO 299 STEP 24
4276       IF NOT (Y+11>=44 AND Y<=115) AND NOT (Y+11>=184 AND Y<=255) THEN
4284         GFX LINE X0+15,Y,X0+16,Y+11,7,BF:GFX LINE X0+47,Y,X0+48,Y+11,7,BF
4292       END IF
4300     NEXT Y
4308     GFX LINE X0+31,0,X0+32,43,8,BF:GFX LINE X0+31,116,X0+32,183,8,BF:GFX LINE X0+31,256,X0+32,299,8,BF
4316   NEXT C
4324   ' the stop lines in front of the crossings (across the two lanes that come in)
4332   FOR R=0 TO 1
4340     FOR C=0 TO 1
4348       X0=168+C*240:Y0=48+R*140
4356       GFX LINE X0-4,Y0+32,X0-3,Y0+63,7,BF
4364       GFX LINE X0+66,Y0,X0+67,Y0+31,7,BF
4372       GFX LINE X0,Y0-4,X0+31,Y0-3,7,BF
4380       GFX LINE X0+32,Y0+66,X0+63,Y0+67,7,BF
4388     NEXT C
4396   NEXT R
4404   ' trees, the plaza around the phone booth, the starting rings of the four players
4412   CALL TREE(24,124):CALL TREE(144,124):CALL TREE(24,176):CALL TREE(144,176)
4420   CALL TREE(496,124):CALL TREE(616,124):CALL TREE(496,176):CALL TREE(616,176)
4428   CALL TREE(252,124):CALL TREE(388,124):CALL TREE(252,176):CALL TREE(388,176)
4436   GFX LINE 294,124,345,175,9,BF:GFX LINE 294,124,345,175,16,B:GFX LINE 297,127,342,172,16,B
4444   FOR I=1 TO 4
4452     GFX CIRCLE SPX(I)+8,SPY(I)+22,12,16+I,0,360,0.4
4460     GFX CIRCLE SPX(I)+8,SPY(I)+22,13,16+I,0,360,0.4
4468   NEXT I
4476   GFX TARGET 3
4484 END PROC
4520 ' ====================================================================== start screen
4590 ' a message on the graphics layer; N frames (0 = until it is cleared)
4600 PROC BANNER(T1$,T2$,N)
4610   LOCAL S
4620   GFX CLEAR
4630   GFX LINE 90,96,549,194,250,BF
4640   GFX LINE 90,96,549,194,3,B
4650   S=3:IF LEN(T1$)*24>430 THEN S=2
4660   CALL BIGTEXT(112,T1$,3,S)
4670   CALL BIGTEXT(164,T2$,1,1)
4680   BN=N
4690 END PROC
4700 PROC DRAWMENU(DM,NP)
4710   GFX CLEAR
4720   GFX LINE 70,8,569,216,250,BF
4730   GFX LINE 70,8,569,216,3,B
4740   CALL BIGTEXT(16,"MADRUN II",3,5)
4750   CALL BIGTEXT(106,"Reach the ringing phone booth in the middle!",1,1)
4760   IF DM=0 THEN
4770     CALL BIGTEXT(134,"K   keyboard      (1 or 2 players)",5,1)
4780     CALL BIGTEXT(152,"C   controllers   (1 to 4 players)",5,1)
4790     IF NP>0 THEN CALL BIGTEXT(168,"-- "+STR$(NP)+" connected, press A or START --",4,1)
4800     CALL BIGTEXT(184,"Q   quit",5,1)
4810   ELSE
4820     CALL BIGTEXT(130,"Keyboard: how many players?",1,1)
4830     CALL BIGTEXT(150,"1   one player      (WASD or cursor keys)",5,1)
4840     CALL BIGTEXT(166,"2   two players     (WASD and cursor keys)",5,1)
4850     CALL BIGTEXT(184,"Q   back",5,1)
4860   END IF
4870   CALL BIGTEXT(200,"HIGH SCORE "+FN PAD$(HI,6)+"      P pause   Q quit",3,1)
4880 END PROC
4890 PROC TITLE
4900   LOCAL K$,K,ST,N,N0,I,DONE,RD
4910   FOR I=0 TO 9
4920     SPRITE I,OFF
4930   NEXT I
4931   FOR I=208 TO 211:SPRITE I,OFF:NEXT I
4940   SPRITE 4,304,138,126:SPRITE 10,320,138,130
4950   SPRITE 4,ON:SPRITE 10,ON:CALL RAYSON(0)
4960   SPRITE 5,OFF
4970   CALL NEWTRAFFIC(3)
4980   FRZ=0:BN=0
4990   ST=0:DONE=0:N0=-1:RD=-1
5000   CALL METAL
5010   REPEAT
5020     GFX VSYNC
5021     CALL AMBIENT
5030     FRM=FRM+1
5031     IF PLAYING(5)=0 AND PLAYING(1)=0 THEN CALL METAL
5040     CALL TRAFFIC
5050     N=JOY("COUNT")
5060     IF N<>N0 THEN N0=N:RD=-1
5070     IF RD THEN CALL DRAWMENU(ST,N):RD=0
5080     K$=INKEY$
5090     K=0:IF K$<>"" THEN K=ASC(K$)
5100     IF K>=97 AND K<=122 THEN K=K-32
5110     IF ST=0 THEN
5120       IF K=75 THEN ST=1:RD=-1
5130       IF K=67 OR FN IN_ANYA(0) OR FN IN_ANYSTART(0) THEN GM=2:DONE=-1
5140       IF K=81 OR FN IN_ANYBACK(0) THEN QT=1:DONE=-1
5150     ELSE
5160       IF K=49 THEN NK=1:GM=1:DONE=-1
5170       IF K=50 THEN NK=2:GM=1:DONE=-1
5180       IF K=81 OR K=66 THEN ST=0:RD=-1
5190     END IF
5200   UNTIL DONE
5210   WHILE FN IN_ANYSTART(0) OR FN IN_ANYBACK(0) OR FN IN_ANYA(0)
5220     GFX VSYNC
5230   WEND
5240   IN_PSK=0
5250   CALL SNDRESET
5260   GFX CLEAR
5270 END PROC
5280 ' ====================================================================== a game
5290 PROC PLAYGAME
5300   LOCAL P
5310   CALL STARTGAME
5320   REPEAT
5330     GFX VSYNC
5331   CALL AMBIENT
5340     FRM=FRM+1
5350     CALL CTRL
5360     IF GQ=0 THEN
5370       CALL JOINS
5380       CALL READIN
5390       CALL MOVEPLAYERS
5400       CALL TRAFFIC
5410       CALL PHONE
5420       CALL PICKUPS
5430       CALL MARKS
5440       IF FRM MOD 4=0 THEN CALL HUD
5450       IF BN>0 THEN
5460         BN=BN-1
5470         IF BN=0 THEN GFX CLEAR
5480       END IF
5490       CALL CHECKOVER
5500     END IF
5510   UNTIL GQ=1 OR OV=1
5520   BEST=0
5530   FOR P=1 TO 4
5540     IF SCP(P)>BEST THEN BEST=SCP(P)
5550   NEXT P
5560   IF OV=1 THEN CALL GAMEOVER
5570   WHILE FN IN_ANYSTART(0) OR FN IN_ANYBACK(0)
5580     GFX VSYNC
5590   WEND
5600   CALL MUSICOFF:CALL SNDRESET
5610   GFX CLEAR
5620   TXT FILL 0,0,79,24,32,0,0
5630 END PROC
5640 PROC STARTGAME
5650   LOCAL P
5660   FOR P=1 TO 4
5670     SCP(P)=0:LV(P)=5:PST(P)=0:PLD(P)=0:NXL(P)=3000:PBT(P)=0:PWF(P)=0
5680     SPRITE P-1,OFF:SPRITE 207+P,OFF:AMS(P)=0
5690   NEXT P
5700   IF GM=1 THEN
5710     PST(1)=1:PLD(1)=1
5720     IF NK=2 THEN PST(2)=1:PLD(2)=1
5730   END IF
5740   GQ=0:OV=0:BN=0:IN_QUIT=0:PUK=0
5750   CALL MUSICON:CALL NEWLEVEL(1)
5760 END PROC
5770 PROC NEWLEVEL(LV2)
5780   LOCAL P
5790   LEVEL=LV2
5800   CALL NEWTRAFFIC(LV2)
5810   FOR P=1 TO 4
5820     IF PST(P)=1 OR PST(P)=2 THEN CALL SPAWN(P,P)
5830   NEXT P
5840   TLIM=MAX(18,40-2*LV2)*60
5850   TLEFT=TLIM
5860   TST=1:FRZ=0:PUK=0:PUTM=300+FN RN(300)
5870   SPRITE 5,OFF:CALL RAYSON(-1):ANSP=0
5871   IF MUSON=0 THEN CALL MUSICON
5880   MSG$="THE PHONE IS RINGING!"
5890   CALL BANNER("LEVEL "+STR$(LV2),"The phone is ringing - hurry!",150)
5900   CALL RING
5910   CALL HUD
5920 END PROC
5930 PROC SPAWN(P2,C)
5940   PX(P2)=SPX(C)+(P2-1)*3-4
5950   PY(P2)=SPY(C)
5960   PST(P2)=1:PINV(P2)=90:PHT(P2)=0:AMS(P2)=0
5961   SPRITE 207+P2,OFF:SFX 16
5970   SPRITE P2-1,INT(PX(P2)),INT(PY(P2)),100+(P2-1)*4
5980   SPRITE P2-1,ON
5990 END PROC
6000 ' P and Q on the keyboard, START and BACK on any pad
6010 PROC CTRL
6020   IF FN IN_SERVICE() THEN CALL PAUSE
6030   IF IN_QUIT THEN GQ=1
6040 END PROC
6200 PROC PAUSE
6210   CALL MUSICOFF:CALL BANNER("PAUSED","P or START continues      Q or BACK quits",0)
6220   CALL IN_PAUSELOOP
6230   IF IN_QUIT THEN GQ=1
6240   GFX CLEAR:IF GQ=0 THEN CALL MUSICON
6250   BN=0
6260 END PROC
6380 ' ====================================================================== the players
6390 ' controller mode: a pad that is connected brings its player, a pad that is removed takes the player away (lives and score stay)
6400 PROC JOINS
6410   LOCAL P
6420   IF GM<>2 THEN EXIT PROC
6430   FOR P=1 TO 4
6440     IF JOY("CONNECTED",P) THEN
6450       IF PST(P)=0 AND LV(P)>0 THEN
6460         PLD(P)=1
6470         CALL SPAWN(P,P)
6480         SFX 5
6490       END IF
6500     ELSE
6510       IF PST(P)=1 OR PST(P)=2 THEN
6520         PST(P)=0
6530         SPRITE P-1,OFF
6540       END IF
6550     END IF
6560   NEXT P
6570 END PROC
6580 PROC READIN
6590   LOCAL P,X,Y,M,L,R,U,D
6600   FOR P=1 TO 4
6610     DXI(P)=0:DYI(P)=0
6620   NEXT P
6630   IF GM=1 THEN
6640     L=KEY("A"):R=KEY("D"):U=KEY("W"):D=KEY("S")
6650     IF NK=1 THEN
6660       L=L OR KEY("LEFT"):R=R OR KEY("RIGHT"):U=U OR KEY("UP"):D=D OR KEY("DOWN")
6670     END IF
6680     IF R THEN DXI(1)=1
6690     IF L THEN DXI(1)=DXI(1)-1
6700     IF D THEN DYI(1)=1
6710     IF U THEN DYI(1)=DYI(1)-1
6720     IF NK=2 THEN
6730       IF KEY("RIGHT") THEN DXI(2)=1
6740       IF KEY("LEFT") THEN DXI(2)=DXI(2)-1
6750       IF KEY("DOWN") THEN DYI(2)=1
6760       IF KEY("UP") THEN DYI(2)=DYI(2)-1
6770     END IF
6780     FOR P=1 TO NK
6790       IF DXI(P)<>0 AND DYI(P)<>0 THEN DXI(P)=DXI(P)*0.72:DYI(P)=DYI(P)*0.72
6800     NEXT P
6810   ELSE
6820     FOR P=1 TO 4
6830       IF PST(P)=1 THEN
6840         X=JOY("LX",P):Y=JOY("LY",P)
6850         IF JOY("LEFT",P) THEN X=-1
6860         IF JOY("RIGHT",P) THEN X=1
6870         IF JOY("UP",P) THEN Y=-1
6880         IF JOY("DOWN",P) THEN Y=1
6890         M=SQR(X*X+Y*Y)
6900         IF M>1 THEN X=X/M:Y=Y/M
6910         DXI(P)=X:DYI(P)=Y
6920       END IF
6930     NEXT P
6940   END IF
6950 END PROC
6960 PROC HITPLAYER(P2)
6970   PST(P2)=2:PHT(P2)=0:AMS(P2)=0
6980   LV(P2)=LV(P2)-1
6990   SFX 2:SFX 18
7000   SPRITE P2-1,INT(PX(P2)),INT(PY(P2)),100+(P2-1)*4+3
7010   SPRITE P2-1,ON
7020 END PROC
7030 PROC EXTRALIFE(P2)
7040   IF SCP(P2)>=NXL(P2) THEN
7050     NXL(P2)=NXL(P2)+3000
7060     IF LV(P2)<9 THEN LV(P2)=LV(P2)+1
7070     SFX 6
7080     MSG$="P"+STR$(P2)+" EXTRA LIFE!"
7090   END IF
7100 END PROC
7110 PROC MOVEPLAYERS
7120   LOCAL P,V,SP2,X,Y,MV,X1,Y1,X2,Y2,HIT,FR,IM
7125   ANYAMB=0
7130   FOR P=1 TO 4
7140     IF PST(P)=1 AND NOT (TST=2 AND P=ANSP) THEN
7150       SP2=1.45
7160       IF PBT(P)>0 THEN SP2=2.1:PBT(P)=PBT(P)-1
7170       X=DXI(P):Y=DYI(P)
7180       PX(P)=FN CLAMP(PX(P)+X*SP2,0,624)
7190       PY(P)=FN CLAMP(PY(P)+Y*SP2,0,276)
7200       MV=ABS(X)+ABS(Y)
7210       IF MV>0.1 THEN PWF(P)=PWF(P)+1 ELSE PWF(P)=0
7211       IF MV>0.1 AND PWF(P) MOD 8=1 THEN SFX 11+P
7220       X1=PX(P)+4:X2=PX(P)+12:Y1=PY(P)+13:Y2=PY(P)+23
7230       HIT=0
7240       IF PINV(P)>0 THEN
7250         PINV(P)=PINV(P)-1
7260       ELSE
7270         FOR V=1 TO NV
7280           IF X2+3>VX1(V) AND X1-3<VX2(V) AND Y2+3>VY1(V) AND Y1-3<VY2(V) THEN
7290             IF X2>VX1(V) AND X1<VX2(V) AND Y2>VY1(V) AND Y1<VY2(V) THEN
7300               IF FRZ=0 THEN HIT=-1
7310             ELSE
7320               IF NM(P,V)=0 AND VSP(V)>0.8 THEN
7330                 NM(P,V)=1:SCP(P)=SCP(P)+5
7340                 IF FN RN(3)=0 THEN SFX 9
7350               END IF
7360             END IF
7370           ELSE
7380             NM(P,V)=0
7390           END IF
7400         NEXT V
7410       END IF
7420       IF HIT THEN
7430         CALL HITPLAYER(P)
7440       ELSE
7450         FR=0
7460         IF MV>0.1 THEN FR=1+((PWF(P)\6) MOD 2)
7470         IM=100+(P-1)*4+FR
7480         IF PINV(P)>0 AND (FRM\3) MOD 2=0 THEN
7490           SPRITE P-1,OFF
7500         ELSE
7510           SPRITE P-1,ON
7520           SPRITE P-1,INT(PX(P)),INT(PY(P)),IM
7530         END IF
7540       END IF
7550     END IF
7560     IF PST(P)=2 THEN
7570       CALL RESCUE(P)
7670     END IF
7680   NEXT P
7690 END PROC
7700 ' ====================================================================== the phone
7710 PROC PHONE
7720   LOCAL P,NA,FR
7730   NA=0
7740   FOR P=1 TO 4
7750     IF PST(P)=1 OR PST(P)=2 THEN NA=NA+1
7760   NEXT P
7770   IF TST=1 THEN
7780     IF NA>0 THEN TLEFT=TLEFT-1
7790     IF NA>0 AND TLEFT<TLIM AND (TLIM-TLEFT) MOD 240=0 THEN CALL RING
7800     FR=0
7801     IF RINGT>0 THEN RINGT=RINGT-1:FR=1+(FRM\2) MOD 2
7802     CALL BOOTH(FR)
7803     CALL RAYS
7810     FOR P=1 TO 4
7820       IF PST(P)=1 THEN
7830         IF PX(P)+12>306 AND PX(P)+4<334 AND PY(P)+23>138 AND PY(P)+13<172 AND TST=1 THEN CALL ANSWER(P)
7840       END IF
7850     NEXT P
7860     IF TLEFT<=0 AND TST=1 THEN CALL TOOLATE
7870   ELSE
7880     IF TST=2 THEN CALL CALLANIM ELSE CALL BOOTH(0)
7890     TCNT=TCNT-1
7900     IF TCNT<=0 THEN
7910       IF TST=2 THEN
7920         CALL NEWLEVEL(LEVEL+1)
7930       ELSE
7940         CALL NEWLEVEL(LEVEL)
7950       END IF
7960     END IF
7970   END IF
7980 END PROC
7990 PROC ANSWER(P2)
8000   LOCAL BONUS
8010   TST=2:TCNT=400:ANSP=P2:PMO=0
8011   CALL HANGUP:CALL RAYSON(0)
8020   BONUS=100+(TLEFT\60)*10+LEVEL*50
8030   SCP(P2)=SCP(P2)+BONUS
8040   ABON=BONUS:CALL EXTRALIFE(P2)
8050   MSG$="P"+STR$(P2)+" ANSWERED! +"+STR$(BONUS)
8080   CALL HUD
8090 END PROC
8100 PROC TOOLATE
8110   LOCAL P
8120   TST=3:TCNT=170
8121   CALL HANGUP:CALL RAYSON(0)
8130   FOR P=1 TO 4
8140     IF PST(P)=1 OR PST(P)=2 THEN
8150       LV(P)=LV(P)-1
8160       IF LV(P)<=0 THEN LV(P)=0:PST(P)=3:SPRITE P-1,OFF
8170     END IF
8180   NEXT P
8190   MSG$="TOO LATE! HUNG UP."
8200   CALL BANNER("TOO LATE!","The caller hung up - everybody loses a life",170)
8210   SFX 7
8220   CALL HUD
8230 END PROC
8240 PROC CHECKOVER
8250   LOCAL P,ANY,ALIVE
8260   ANY=0:ALIVE=0
8270   FOR P=1 TO 4
8280     IF PLD(P)=1 THEN
8290       ANY=-1
8300       IF LV(P)>0 OR PST(P)=2 THEN ALIVE=-1
8310     END IF
8320   NEXT P
8330   IF ANY AND NOT ALIVE THEN OV=1
8340 END PROC
8350 PROC GAMEOVER
8360   LOCAL I
8370   CALL MUSICOFF:CALL BANNER("GAME OVER","Best score "+FN PAD$(BEST,6),0)
8380   PLAY "T110 W1 V0.3 O4 G4 E4 C4 <G2","T110 W2 V0.2 O3 C4 <G4 E4 C2"
8390   FOR I=1 TO 220
8400     GFX VSYNC
8410     CALL TRAFFIC
8420   NEXT I
8430 END PROC
8440 ' ====================================================================== the pick-ups
8450 PROC PICKUPS
8460   LOCAL P,R,NA,S
8470   NA=0
8480   FOR P=1 TO 4
8490     IF PST(P)=1 THEN NA=NA+1
8500   NEXT P
8510   IF PUK=0 THEN
8520     IF NA>0 THEN PUTM=PUTM-1
8530     IF PUTM<=0 THEN
8540       R=FN RN(10)
8550       PUK=3
8560       IF R<4 THEN PUK=1
8570       IF R>=4 AND R<8 THEN PUK=2
8580       S=FN RN(8)
8590       PUX=PSPX(S):PUY=PSPY(S):PUTM=720
8600       SPRITE 5,PUX,PUY,121+PUK
8610       SPRITE 5,ON
8620     END IF
8630   ELSE
8640     PUTM=PUTM-1
8650     IF PUTM<=0 THEN
8660       PUK=0:PUTM=600+FN RN(600)
8670       SPRITE 5,OFF
8680     ELSE
8690       IF PUTM<120 AND (FRM\4) MOD 2=0 THEN SPRITE 5,OFF ELSE SPRITE 5,ON
8700       FOR P=1 TO 4
8710         IF PST(P)=1 AND PUK>0 THEN
8720           IF PX(P)+12>PUX+2 AND PX(P)+4<PUX+14 AND PY(P)+23>PUY+4 AND PY(P)+13<PUY+20 THEN CALL TAKE(P)
8730         END IF
8740       NEXT P
8750     END IF
8760   END IF
8770 END PROC
8780 PROC TAKE(P2)
8790   SCP(P2)=SCP(P2)+25:SFX 17
8800   IF PUK=1 THEN
8810     FRZ=300:SFX 8:MSG$="TRAFFIC STOPPED!"
8820   END IF
8830   IF PUK=2 THEN
8840     PBT(P2)=600:SFX 3:MSG$="P"+STR$(P2)+" TURBO BOOTS!"
8850   END IF
8860   IF PUK=3 THEN
8870     IF LV(P2)<9 THEN LV(P2)=LV(P2)+1
8880     SFX 6:MSG$="P"+STR$(P2)+" EXTRA LIFE!"
8890   END IF
8900   CALL EXTRALIFE(P2)
8910   PUK=0:PUTM=600+FN RN(600)
8920   SPRITE 5,OFF
8930 END PROC
8940 ' the blinking question mark in the corner of a player who is not there (controller mode): connect a pad!
8950 PROC MARKS
8960   LOCAL P
8970   FOR P=1 TO 4
8980     IF GM=2 AND PST(P)=0 AND (FRM\24) MOD 2=0 THEN
8990       SPRITE 5+P,SPX(P),SPY(P),125
9000       SPRITE 5+P,ON
9010     ELSE
9020       SPRITE 5+P,OFF
9030     END IF
9040   NEXT P
9050 END PROC
9060 ' ====================================================================== the text screen
9120 PROC HUD
9130   LOCAL P,X,Y,S
9140   FOR P=1 TO 4
9150     X=1:IF P=2 OR P=4 THEN X=56
9160     Y=0:IF P>2 THEN Y=23
9170     TXT FILL X,Y,X+22,Y+1,32,0,0
9180     IF PLD(P)=1 THEN
9190       CALL TXTAT(X,Y,"P"+STR$(P)+" "+FN PAD$(SCP(P),6),PCOL(P))
9200       IF PST(P)=3 THEN
9210         CALL TXTAT(X,Y+1,"OUT OF LIVES",203)
9220       ELSE
9230         IF PST(P)=0 THEN
9240           CALL TXTAT(X,Y+1,"LIVES "+STR$(LV(P))+"  PAD LOST",206)
9250         ELSE
9260           CALL TXTAT(X,Y+1,"LIVES "+STR$(LV(P)),205)
9270         END IF
9280       END IF
9290     ELSE
9300       IF GM=2 THEN
9310         S=201:IF (FRM\30) MOD 2=0 THEN S=206
9320         CALL TXTAT(X,Y,"P"+STR$(P)+" CONNECT A PAD",S)
9330       END IF
9340     END IF
9350   NEXT P
9360   TXT FILL 29,0,50,3,32,0,0
9370   TXT FILL 29,23,50,24,32,0,0
9380   CALL TXTAT(29,0,"LEVEL "+STR$(LEVEL),202)
9390   CALL TXTAT(29,1,"TIME "+STR$(TLEFT\60+1),200)
9400   CALL TXTAT(29,2,"HIGH "+FN PAD$(HI,6),201)
9410   CALL TXTAT(29,23,MSG$,200)
9420 END PROC
9430 ' ====================================================================== the traffic
9440 PROC NEWTRAFFIC(LV2)
9450   LOCAL K,S,KD,R,LN,OK,J,A,FIRST,PREV,V,GX2,SA
9460   FOR V=1 TO NV
9470     SA=12+2*(V-1)
9480     SPRITE SA,OFF:SPRITE SA+1,OFF
9490   NEXT V
9500   NV=0
9510   SPD=MIN(2.7,1.1+0.12*LV2)
9520   GX2=MAX(36,170-LV2*9)
9530   FOR K=0 TO 15
9540     FIRST=0:PREV=0
9550     S=30+FN RN(60)
9560     WHILE S<LLEN(K)-50
9570       R=FN RN(100)
9580       KD=0
9590       IF LV2>=2 AND R<50 THEN KD=1
9600       IF LV2>=3 AND R<30 THEN KD=3
9610       IF LV2>=4 AND R<14 THEN KD=2
9620       IF LV2>=6 AND R<4 THEN KD=4
9630       LN=VLEN(KD)
9640       OK=-1
9650       FOR J=0 TO 1
9660         A=CA(K,J)
9670         IF S>A-8 AND S-LN<A+72 THEN OK=0
9680       NEXT J
9690       IF OK AND NV<90 THEN
9700         NV=NV+1
9710         VK(NV)=KD:VVR(NV)=FN RN(NVAR(KD)):VLN(NV)=LN:VWD(NV)=VWID(KD):VL(NV)=K:VS(NV)=S:VSP(NV)=0
9720         VMX(NV)=MIN(3,SPD*VSPF(KD)*LSPF(K)*(0.9+RND(1)*0.2)):VR(NV)=0:VSTL(NV)=0:VLD(NV)=NV
9730         IF PREV<>0 THEN VLD(PREV)=NV ELSE FIRST=NV
9740         PREV=NV
9750         S=S+LN+GX2+FN RN(60)
9760       ELSE
9770         S=S+8
9780       END IF
9790     WEND
9800     IF PREV<>0 THEN VLD(PREV)=FIRST
9810   NEXT K
9820   FOR V=1 TO NV
9830     SA=12+2*(V-1)
9840     SPRITE SA,ON
9850     IF NSEG(VK(V))=2 THEN SPRITE SA+1,ON ELSE SPRITE SA+1,OFF
9860     CALL SHOWVEH(V)
9870   NEXT V
9880   FOR K=0 TO 4
9890     FOR V=0 TO 99
9900       NM(K,V)=0
9910     NEXT V
9920   NEXT K
9930 END PROC
9940 PROC TRAFFIC
9950   LOCAL V,I
9960   FOR I=0 TO 3
9970     OCC(I,0)=0:OCC(I,1)=0
9971     WT(I,0)=WN(I,0):WT(I,1)=WN(I,1):WN(I,0)=0:WN(I,1)=0
9980   NEXT I
9990   FOR V=1 TO NV
10000     IF VR(V)>0 THEN OCC(VR(V)-1,LAX(VL(V)))=OCC(VR(V)-1,LAX(VL(V)))+1
10010   NEXT V
10020   IF FRZ>0 THEN FRZ=FRZ-1
10025   IF HKT>0 THEN HKT=HKT-1
10030   FOR V=1 TO NV
10040     CALL MOVEVEH(V)
10050     CALL SHOWVEH(V)
10060   NEXT V
10070 END PROC
10080 ' one vehicle: P is the farthest the front may go (behind the vehicle ahead, or at the stop line of a crossing it may not enter yet);
10090 ' it brakes so that it can stop there (v^2 / 2a), accelerates otherwise
10100 PROC MOVEVEH(VI)
10110   LOCAL K,S,L,P,G,J,A,C,AX,SP,LD,DST,T,VA,RM,NS,DD2
10120   K=VL(VI):S=VS(VI):L=LLEN(K):SP=VSP(VI):AX=LAX(K)
10130   G=100000
10140   LD=VLD(VI)
10150   IF LD<>VI THEN
10160     G=VS(LD)-VLN(LD)-S
10170     IF G<0 THEN G=G+L
10180   END IF
10190   P=S+G-GAP
10200   FOR J=0 TO 1
10210     A=CA(K,J):C=CID(K,J)
10220     IF VR(VI)=C+1 THEN
10230       IF S-VLN(VI)>=A+64 THEN VR(VI)=0
10240     ELSE
10250       DD2=A-S
10260       IF DD2>=0 AND DD2<=ZONE THEN
10270         ' enter a crossing only if no vehicle of the other axis uses it and there is room behind it for the whole vehicle
10280         RM=A+72+VLN(VI)+GAP-S
10290         IF OCC(C,1-AX)=0 AND G>=RM AND (WT(C,1-AX)=0 OR WT(C,AX)>0) THEN
10300           VR(VI)=C+1
10310           OCC(C,AX)=OCC(C,AX)+1
10320         ELSE
10330           P=MIN(P,A-STOPGAP)
10335           IF VSTL(VI)>=40 THEN WN(C,AX)=1
10340         END IF
10350       END IF
10360     END IF
10370   NEXT J
10380   DST=P-S
10390   T=VMX(VI)
10400   IF DST<1 THEN
10410     T=0
10420   ELSE
10430     VA=SQR(2*DCL*(DST-1))
10440     IF VA<T THEN T=VA
10450   END IF
10460   IF FRZ>0 THEN T=0
10470   IF SP<T THEN
10480     SP=SP+ACL
10490     IF SP>T THEN SP=T
10500   ELSE
10510     SP=SP-DCL
10520     IF SP<T THEN SP=T
10530   END IF
10540   NS=S+SP
10550   IF NS>P THEN
10560     NS=P
10570     IF NS<S THEN NS=S
10580     SP=NS-S
10590   END IF
10600   IF NS>=L THEN NS=NS-L
10610   VS(VI)=NS:VSP(VI)=SP
10620   IF SP<0.01 THEN VSTL(VI)=VSTL(VI)+1 ELSE VSTL(VI)=0
10621   IF VSTL(VI)=20 AND G<30 AND HKT<=0 AND ANYAMB=0 THEN CALL HONK(VK(VI))
10630 END PROC
10640 ' the rectangle of the vehicle (for the collisions) and its sprites; the front is at VS, the lane is a circle of 64 pixels more than the screen
10650 PROC SHOWVEH(VI)
10660   LOCAL K,S,D,P0,W,OF2,FX,FY,X1,Y1,X2,Y2,IM,KD,VR2,SA,SX,SY,SX2,SY2
10670   K=VL(VI):S=VS(VI):D=LDIR(K):P0=LPOS(K):W=VWD(VI):OF2=P0+(16-W)\2
10680   KD=VK(VI):VR2=VVR(VI)
10690   IF KD=4 THEN VR2=(FRM\8) MOD 2
10700   SA=12+2*(VI-1)
10710   IM=IMGB(KD,VR2,0)*4+D
10720   IF D=0 THEN
10730     FX=S-32
10740     X1=FX-VLN(VI):X2=FX:Y1=OF2:Y2=OF2+W
10750     SX=FX-16:SY=P0-4:SX2=FX-32:SY2=SY
10760   END IF
10770   IF D=1 THEN
10780     FX=672-S
10790     X1=FX:X2=FX+VLN(VI):Y1=OF2:Y2=OF2+W
10800     SX=FX:SY=P0-4:SX2=FX+16:SY2=SY
10810   END IF
10820   IF D=2 THEN
10830     FY=S-32
10840     Y1=FY-VLN(VI):Y2=FY:X1=OF2:X2=OF2+W
10850     SX=P0:SY=FY-16:SX2=SX:SY2=FY-32
10860   END IF
10870   IF D=3 THEN
10880     FY=332-S
10890     Y1=FY:Y2=FY+VLN(VI):X1=OF2:X2=OF2+W
10900     SX=P0:SY=FY:SX2=SX:SY2=FY+16
10910   END IF
10920   VX1(VI)=X1:VY1(VI)=Y1:VX2(VI)=X2:VY2(VI)=Y2
10930   SPRITE SA,INT(SX),INT(SY),IM
10940   IF NSEG(KD)=2 THEN SPRITE SA+1,INT(SX2),INT(SY2),IMGB(KD,VR2,1)*4+D
10950 END PROC
11020 ' ====================================================================== the data
11030 ' vehicle kinds (car, truck, bus, motorbike, ambulance): variants, sprites, length, width, speed factor
11040 DATA 6,1,16,10,1, 3,2,32,10,0.8, 2,2,32,10,0.85, 3,1,12,6,1.35, 2,1,16,10,1.5
11050 ' where the four players start (the sprite's corner), then eight places for the pick-ups
11060 DATA 92,12, 532,12, 92,264, 532,264
11070 DATA 310,12, 310,264, 60,128, 140,150, 500,128, 580,150, 260,128, 364,160
11080 ' the sprite colors 1 to 34
11090 DATA 20,20,25, 140,200,235, 255,245,160, 230,40,40, 240,240,240, 150,150,155
11100 DATA 220,50,50, 60,100,230, 60,180,80, 235,200,50, 240,140,40, 160,80,210
11110 DATA 225,225,205, 80,130,180, 120,170,120, 245,190,40, 200,200,205, 240,190,150
11120 DATA 100,60,30, 230,60,60, 70,120,240, 70,200,90, 240,200,60, 50,60,120
11130 DATA 40,30,30, 210,30,30, 130,15,15, 170,220,240, 255,230,80, 90,90,100
11140 DATA 60,100,255, 255,215,60, 120,70,30, 200,205,215
11150 ' the background colors 1 to 20
11160 DATA 62,62,70, 50,50,58, 170,170,175, 120,120,128, 60,150,70, 50,130,60
11170 DATA 235,235,235, 240,200,50, 200,185,150, 230,70,70, 250,230,90, 250,250,250
11180 DATA 30,100,45, 70,170,80, 110,75,40, 150,135,105, 230,60,60, 70,120,240
11190 DATA 70,200,90, 240,200,60
12000 ' ====================================================================== the booth: rays, the receiver, the call
12010 ' the open booth in two sprites: 4 is the left half (the coin telephone; the image says what the receiver does: 0 rests, 1 and 2 jump, 3 is off the hook), 10 the open door
12020 PROC BOOTH(FR)
12030   SPRITE 4,304,138,126+FR
12040 END PROC
12050 ' one ray: a short line through the middle of its 16 x 24 picture, in eight directions (the other eight rays are the same pictures on the other side)
12060 PROC MKRAY(K)
12070   LOCAL A,X1,Y1,X2,Y2,DX,DY
12080   A=K*PI/8
12090   DX=INT(6.5*COS(A)+0.5):DY=INT(6.5*SIN(A)+0.5)
12100   X1=8-DX:Y1=12-DY:X2=8+DX:Y2=12+DY
12110   GFX LINE 0,0,15,23,0,BF
12120   GFX LINE X1+1,Y1,X2+1,Y2,29:GFX LINE X1,Y1+1,X2,Y2+1,29:GFX LINE X1+1,Y1+1,X2+1,Y2+1,29
12130   GFX LINE X1,Y1,X2,Y2,5
12140 END PROC
12150 ' sixteen rays that run outwards from the middle of the booth (320,150) all the time the phone rings: you can see from anywhere that something is up
12160 PROC RAYS
12170   LOCAL K,A,R
12180   FOR K=0 TO 15
12190     A=K*PI/8
12200     R=22+((FRM*2+K*7) MOD 30)
12210     SPRITE 192+K,INT(320+R*COS(A)-8),INT(150+R*SIN(A)-12),143+(K MOD 8)
12220   NEXT K
12230 END PROC
12240 PROC RAYSON(F)
12250   LOCAL K
12260   FOR K=192 TO 207
12270     IF F THEN SPRITE K,ON ELSE SPRITE K,OFF
12280   NEXT K
12290 END PROC
12300 ' the call (E counts the frames from the pick-up): the player steps to the open booth, lifts the receiver (the clatter), listens to the caller (thin
12310 ' squeaks), talks (low, wobbling syllables with the mouth moving), puts the receiver back (the click); then the points are shown
12320 PROC CALLANIM
12330   LOCAL E,F,K,P
12340   E=400-TCNT:P=ANSP
12350   IF E=0 THEN PX(P)=312:PY(P)=156:CALL BOOTH(0):CALL MUSICOFF
12360   IF E=6 THEN SFX 1:CALL BOOTH(3)
12370   IF E=14 THEN
12380     WAVE 1,1:ENVELOPE 1,0.01,0.04,0.5,0.03:VIBRATO 1,6,0.3
12390     WAVE 2,1:ENVELOPE 2,0.005,0.03,0.6,0.02:VIBRATO 2,9,0.5
12400   END IF
12410   IF E>=14 AND E<200 AND E MOD 7=0 THEN
12420     IF E>=70 AND E<150 THEN
12430       IF FN RN(5)>0 THEN
12440         F=110+FN RN(130)
12450         SOUND 1,F,0.25,0.3,F*(0.7+RND(1)*0.6)
12460         PMO=5
12470       END IF
12480     ELSE
12490       IF FN RN(5)>0 THEN
12500         F=330+FN RN(260)
12510         SOUND 2,F,0.2,0.12,F*(0.8+RND(1)*0.5)
12520       END IF
12530     END IF
12540   END IF
12550   IF PMO>0 THEN PMO=PMO-1
12560   K=0
12570   IF E>=14 AND E<200 THEN
12580     K=1:IF PMO>0 THEN K=2
12590   END IF
12600   IF E=200 THEN CALL BOOTH(0)
12610   IF E=206 THEN SFX 10:CALL HANGUP
12620   IF E=212 THEN
12630     CALL BANNER("PLAYER "+STR$(P)+" ANSWERS","+"+STR$(ABON)+" points",150)
12640     PLAY "T150 W1 V0.3 O5 C8 E8 G8 >C4","T150 W2 V0.2 O3 C4 G4"
12650   END IF
12660   IF E>=206 THEN
12670     SPRITE P-1,312,156,100+(P-1)*4
12680   ELSE
12690     SPRITE P-1,312,156,131+(P-1)*3+K
12700   END IF
12710   SPRITE P-1,ON
12720 END PROC
12800 ' ====================================================================== music
12810 ' The title tune: heavy metal. Seven voices: two square wave guitars (a power chord: the root and the fifth), a bass, a lead guitar that comes in
12820 ' for the second four bars and ends in a dive-bomb, a kick drum (a quick sine sweep), a snare and a hi-hat (noise). E minor, 152 beats a minute, 8 bars.
12830 PROC METAL
12840   LOCAL A$,B$,C$,D$,E$,F$,G$
12850   A$="T152 W1 V0.2 MS H164.8,8 H164.8,8 H164.8,8 H164.8,8 H196,8 H164.8,8 H174.6,8 H164.8,8 H164.8,8 H164.8,8 H164.8,8 H164.8,8 H220,8 H196,8 H174.6,8 H164.8,8 H130.8,8 H130.8,8 H130.8,8 H130.8,8 H164.8,8 H130.8,8 H146.8,8 H130.8,8 H246.9,8 H246.9,8 H246.9,8 H246.9,8 H146.8,8 H246.9,8 H130.8,8 H246.9,8 H164.8,8 H164.8,8 H164.8,8 H164.8,8 H196,8 H164.8,8 H174.6,8 H164.8,8 H164.8,8 H164.8,8 H164.8,8 H164.8,8 H220,8 H196,8 H174.6,8 H164.8,8 H130.8,8 H130.8,8 H130.8,8 H130.8,8 H164.8,8 H130.8,8 H146.8,8 H130.8,8 H246.9,8 H246.9,8 H246.9,8 H246.9,8 H146.8,8 H246.9,8 H130.8,8 H246.9,8"
12860   B$="T152 W1 V0.14 MS H246.9,8 H246.9,8 H246.9,8 H246.9,8 H293.7,8 H246.9,8 H261.6,8 H246.9,8 H246.9,8 H246.9,8 H246.9,8 H246.9,8 H329.6,8 H293.7,8 H261.6,8 H246.9,8 H196,8 H196,8 H196,8 H196,8 H246.9,8 H196,8 H220,8 H196,8 H370,8 H370,8 H370,8 H370,8 H220,8 H370,8 H196,8 H370,8 H246.9,8 H246.9,8 H246.9,8 H246.9,8 H293.7,8 H246.9,8 H261.6,8 H246.9,8 H246.9,8 H246.9,8 H246.9,8 H246.9,8 H329.6,8 H293.7,8 H261.6,8 H246.9,8 H196,8 H196,8 H196,8 H196,8 H246.9,8 H196,8 H220,8 H196,8 H370,8 H370,8 H370,8 H370,8 H220,8 H370,8 H196,8 H370,8"
12870   C$="T152 W2 V0.34 ML H82.4,4 H82.4,4 H82.4,4 H82.4,4 H65.4,4 H65.4,4 H123.5,4 H123.5,4 H82.4,4 H82.4,4 H82.4,4 H82.4,4 H65.4,4 H65.4,4 H123.5,4 H123.5,4"
12880   D$="T152 W1 V0.17 ML ~5.5,0.35 R1 R1 R1 R1 H329.6,2 H392,4 H440,4 H493.9,2. R4 H659.3,2 H587.3,4 H493.9,4 H493.9,4 H440,4 S1400,200,2"
12890   E$="T152 W0 V0.6 MN X0,0.002,0,0.05 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8 S170,45,8 R8 S170,45,8 S170,45,8"
12900   F$="T152 W3 V0.3 MN X0,0.002,0,0.045 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8 R8 R8 H3000,8 R8"
12910   G$="T152 W3 V0.1 MN X0,0.001,0,0.012 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16 H9000,16 R16"
12920   PLAY A$,B$,C$,D$,E$,F$,G$
12930 END PROC
12940 ' All channels back to their plain state (after the tune, the talking and the game music)
12950 PROC SNDRESET
12960   LOCAL C
12970   SOUND OFF
12980   FOR C=1 TO 8
12990     ENVELOPE C:VIBRATO C,0,0:TREMOLO C,0,0:WAVE C,0
13000   NEXT C
13010 END PROC
13020 ' The game music is a theremin in a 1950s horror film: a sine wave with a wide, slow vibrato and long swoops between the notes of a scale made of
13030 ' a minor second and a tritone (E F G G# Bb B C D E), over a low drone that now and then slips up a semitone, and a thin ghostly whistle. One bar
13040 ' (4 beats, 2 seconds) is made at a time by the EVERY handler (the label MUSICTIMER in the main program) on the channels 4 to 6 (1 to 3 are the telephone and the
13050 ' voices, 7 and 8 the sound effects).
13060 PROC MUSICON
13070   TS(0)=329.63:TS(1)=349.23:TS(2)=392:TS(3)=415.3:TS(4)=466.16:TS(5)=493.88:TS(6)=523.25:TS(7)=587.33:TS(8)=659.26
13080   MI=3:BR=0
13090   WAVE 4,0:VIBRATO 4,5.5,0.7:ENVELOPE 4,0.25,0.3,0.8,0.5
13100   WAVE 5,2:TREMOLO 5,0.6,0.4:ENVELOPE 5,0.6,0,1,0.8
13110   WAVE 6,0:VIBRATO 6,7,1.5:ENVELOPE 6,0.1,0.2,0.5,0.6
13120   CALL MBAR
13130   EVERY 120,0 GOSUB MUSICTIMER
13135   MUSON=-1
13140 END PROC
13150 PROC MUSICOFF
13160   EVERY OFF 0
13170   SOUND OFF 4:SOUND OFF 5:SOUND OFF 6
13175   MUSON=0
13180 END PROC
13190 PROC MBAR
13200   LOCAL PT,N1,N2,N3,DR
13210   N1=MI:MI=FN CLAMP(MI+FN RN(5)-2,0,8):N2=MI:MI=FN CLAMP(MI+FN RN(5)-2,0,8):N3=MI
13220   PT=FN RN(4)
13230   IF PT=0 THEN SOUND 4,TS(N1),2,0.22,TS(N2):SOUND 4,TS(N2),2,0.22,TS(N3)
13240   IF PT=1 THEN SOUND 4,0,1:SOUND 4,TS(N1),3,0.24,TS(N2)
13250   IF PT=2 THEN SOUND 4,TS(N1),1.5,0.2,TS(N1)*2:SOUND 4,TS(N1)*2,1.5,0.2,TS(N2):SOUND 4,TS(N2),1,0.2,TS(N2)*0.5
13260   IF PT=3 THEN SOUND 4,TS(N1),4,0.22,TS(N1)*1.06
13270   DR=82.41:IF BR MOD 4=3 THEN DR=87.31
13280   SOUND 5,DR,4,0.16
13290   IF FN RN(3)=0 THEN SOUND 6,0,2:SOUND 6,TS(N3)*2,2,0.07,TS(N3) ELSE SOUND 6,0,4
13300   BR=BR+1
13310 END PROC
13320 ' ====================================================================== more sounds, the honking, the ambulance
13330 PROC SOUNDS2
13340   SFX 16,"W2 V0.3 X0.005,0.04,0.5,0.1 S250,1400,6 H1800,24"
13350   SFX 17,"W1 V0.3 ML H1319,16 H1760,12"
13360   SFX 18,"W2 V0.35 ML S600,110,3"
13370   ' the horns, one for each kind of vehicle: a car beeps twice, a truck booms deeply, a bus has two notes, a motorbike meeps high, an ambulance has its two-tone siren
13380   SFX 20,"W1 V0.22 ML H440,16 R16 H440,16"
13390   SFX 21,"W1 V0.4 ML ~3,0.1 H98,4"
13400   SFX 22,"W1 V0.3 ML H196,6 H247,6"
13410   SFX 23,"W1 V0.2 ML H880,24 R24 H1175,16"
13420   SFX 24,"W2 V0.3 ML H700,6 H930,6 H700,6 H930,6"
13430 END PROC
13440 ' a vehicle that had to stop behind another one blows its horn (at most one horn every second or two; not while the ambulance is about)
13450 PROC HONK(KD)
13460   HKT=60+FN RN(120)
13470   SFX 20+KD,7
13480 END PROC
13490 ' a player who was hit lies there for a second, then an ambulance drives in from the nearer side, stops next to him, takes him away and drives off;
13500 ' only when it has left the screen does the player come back (or is out, with no lives left). AMS: 0 not started, 1 waiting, 2 coming, 3 loading, 4 leaving
13510 PROC RESCUE(P)
13520   LOCAL A,SG,TG,IM
13530   A=AMS(P)
13540   IF A=0 THEN
13550     AMS(P)=1:AMT(P)=60
13560     EXIT PROC
13570   END IF
13580   IF A=1 THEN
13590     AMT(P)=AMT(P)-1
13600     IF AMT(P)<=0 THEN
13610       IF PX(P)<312 THEN AMD(P)=0:AMX(P)=-20 ELSE AMD(P)=1:AMX(P)=660
13620       AMS(P)=2:SPRITE 207+P,ON
13630     END IF
13640     EXIT PROC
13650   END IF
13660   ANYAMB=-1
13670   SG=1:IF AMD(P)=1 THEN SG=-1
13680   TG=PX(P)+8-SG*26
13690   IF A=2 THEN
13700     AMX(P)=AMX(P)+3.6*SG
13710     IF (AMX(P)-TG)*SG>=0 THEN AMX(P)=TG:AMS(P)=3:AMT(P)=55:SFX 10
13720   END IF
13730   IF A=3 THEN
13740     AMT(P)=AMT(P)-1
13750     IF AMT(P)=30 THEN SPRITE P-1,OFF:SFX 10
13760     IF AMT(P)<=0 THEN AMS(P)=4
13770   END IF
13780   IF A=4 THEN
13790     AMX(P)=AMX(P)+4.2*SG
13800     IF AMX(P)<-30 OR AMX(P)>670 THEN
13810       SPRITE 207+P,OFF:AMS(P)=0
13820       IF LV(P)<=0 THEN
13830         LV(P)=0:PST(P)=3
13840       ELSE
13850         CALL SPAWN(P,1+FN RN(4))
13860       END IF
13870       EXIT PROC
13880     END IF
13890   END IF
13900   IF FRM MOD 80=0 THEN SFX 24,7
13910   IM=IMGB(4,(FRM\8) MOD 2,0)*4+AMD(P)
13920   SPRITE 207+P,INT(AMX(P)-8),INT(PY(P)+6),IM
13930 END PROC
14000 ' ====================================================================== new in Madrun II: the helicopter and the pigeons
14010 ' the colors (translucent ones for the rotor and the shadow), the pictures, the group, and the paths: they run from now on, in the title and in the game
14020 PROC AMBSTART
14030   LOCAL I
14040   GFX PALETTE 4,200,RGB(40,90,200):GFX PALETTE 4,201,RGB(150,215,255):GFX PALETTE 4,202,RGB(90,95,105):GFX PALETTE 4,203,RGB(230,230,235)
14050   GFX PALETTE 4,204,RGB(255,200,40):GFX PALETTE 4,205,RGB(20,50,130)
14060   GFX PALETTE 4,207,RGB(255,255,255,70):GFX PALETTE 4,208,RGB(0,0,0,110):GFX PALETTE 4,213,RGB(235,235,240,150)
14070   GFX PALETTE 4,209,RGB(135,140,160):GFX PALETTE 4,210,RGB(190,195,215):GFX PALETTE 4,211,RGB(255,150,40):GFX PALETTE 4,212,RGB(90,95,120)
14080   SFX 30,"W3 V0.1 ML ^ 17,0.9 H85,30"
14090   CALL DEFAMB
14100   SPRITE 220,SIZE 4,3
14110   SPRITE SEQ 10,CHR$(160)+CHR$(172)
14120   SPRITE SEQ 11,CHR$(184)+CHR$(185)
14130   SPRITE 220,-70,60,160
14140   SPRITE 220,PRIORITY 7
14150   SPRITE 220,ANIM 10,2,2
14160   SPRITE 220,PATH "H W 400 X=-70 Y=50 S E 1 [ M 3,1*20 M 3,-1*20 ]7 H E 2 W 1500 R"
14170   SPRITE 220,ON
14180   ' pigeons on the grass blocks at the bottom of the map: walk a few steps, stand still, walk back; every one has its own rhythm
14190   FOR I=0 TO 5
14200     SPRITE 232+I,PRIORITY 1
14210     SPRITE 232+I,PATH "W "+STR$(10+I*13)+" [ A 11,2,5 X+1*"+STR$(14+I*3)+" D 184 W "+STR$(35+I*9)+" A 11,2,5 X-1*"+STR$(14+I*3)+" D 185 W "+STR$(50-I*4)+" ]1000"
14220     SPRITE 232+I,ON
14230   NEXT I
14240   SPRITE 232,40,262,184:SPRITE 233,96,268,184:SPRITE 234,300,264,184:SPRITE 235,350,270,184:SPRITE 236,520,262,184:SPRITE 237,570,268,184
14250 END PROC
14260 ' the pictures: the helicopter (4 x 3 images, two rotor positions: 160..171 and 172..183, seen from above, flying to the right) and the pigeon (184, 185)
14270 PROC DEFAMB
14280   LOCAL F,K,X,Y,GX,GY,C,A$,D,AN
14290   FOR F=0 TO 1
14300     FOR K=0 TO 11
14310       A$=""
14320       FOR Y=0 TO 23
14330         FOR X=0 TO 15
14340           GX=(K MOD 4)*16+X:GY=(K\4)*24+Y:C=0
14350           ' the shadow: a translucent ellipse below and to the right of the body
14360           IF ((GX-44)/15)^2+((GY-56)/7)^2<1 THEN C=208
14370           ' the rotor: a translucent disc around the mast, with two opaque blades that turn
14380           D=SQR((GX-32)^2+(GY-30)^2)
14390           IF D<28 THEN C=207
14400           IF D<28 THEN
14410             AN=ATAN2(GY-30,GX-32)
14420             IF F=0 AND ABS(SIN(AN-0.5))<0.07 THEN C=213
14430             IF F=1 AND ABS(SIN(AN-1.2))<0.07 THEN C=213
14440           END IF
14450           ' the tail boom with its small rotor and the body
14460           IF GY>=29 AND GY<=31 AND GX>=6 AND GX<=26 THEN C=202
14470           IF (GX-6)^2+(GY-30)^2<9 THEN C=203
14480           IF ((GX-33)/9)^2+((GY-30)/6)^2<1 THEN
14490             C=200
14500             IF GX>36 AND ((GX-39)/4)^2+((GY-30)/4)^2<1 THEN C=201
14510             IF GY<26 THEN C=205
14520           END IF
14530           IF (GX-32)^2+(GY-30)^2<4 THEN C=204
14540           A$=A$+CHR$(C)
14550         NEXT X
14560       NEXT Y
14570       SPRITE DEF 160+F*12+K,A$
14580     NEXT K
14590   NEXT F
14600   FOR F=0 TO 1
14610     A$=""
14620     FOR Y=0 TO 23
14630       FOR X=0 TO 15
14640         C=0
14650         IF ((X-7)/4.5)^2+((Y-12)/3)^2<1 THEN C=209
14660         IF ((X-11)/2.2)^2+((Y-12)/2)^2<1 THEN C=210
14670         IF X=13 AND Y=12 THEN C=211
14680         IF F=1 AND ABS(Y-12)>=3 AND ABS(Y-12)<=5 AND X>=4 AND X<=8 THEN C=212
14690         IF F=0 AND X=3 AND Y=12 THEN C=212
14700         A$=A$+CHR$(C)
14710       NEXT X
14720     NEXT Y
14730     SPRITE DEF 184+F,A$
14740   NEXT F
14750 END PROC
14760 ' the events of the paths: the helicopter reports that it has appeared (E 1: the rotor noise) and that it has gone (E 2)
14770 PROC AMBIENT
14780   LOCAL E
14790   E=SPRITE("EVENT")
14800   WHILE E>=0
14810     IF E=1 THEN SFX 30
14820     E=SPRITE("EVENT")
14830   WEND
14840 END PROC
