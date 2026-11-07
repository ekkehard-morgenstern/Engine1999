1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
5 ' the helpers that the demo games share are in the libraries demos/lib/util.lib, input.lib, hiscore.lib and text.lib
6 LOAD LIBRARY "util"
7 LOAD LIBRARY "input"
8 LOAD LIBRARY "hiscore"
9 LOAD LIBRARY "text"
10 ' BEING A RUSTBALL - a labyrinth game for one to four players, for Engine1999.
20 ' You are a rust ball, and the labyrinth is full of construction workers. They run away when they see you, so creep up on one from behind: whoever you touch
30 ' on the back is covered in rust, turns into a statue and crumbles. Screws, nuts and bolts give you power: five of them make you one size bigger (up to three
40 ' times as big, and the bigger you are the faster you roll, and a ball of size 2 or 3 crushes the walls: keep pushing against one for a moment).
50 ' If you eat nothing for a while, you dry out and shrink. An arrow at the edge of the picture points to the nearest worker out of sight.
60 ' Workers who have seen fruit forget about you and walk to it to eat it: a good moment for a sneak attack. Watch out for the foremen: they carry a hose
70 ' and wash you down when they see you from the front - catch them from behind, too (500 points). Rust a whole chain of workers quickly for more points.
80 ' Keys: player 1 WASD (with one player the cursor keys work, too), player 2 the cursor keys. Controllers: players 1 to 4, left stick or D-pad;
90 ' a player appears when a pad is connected and goes away when it is removed. Everywhere: P pauses (START on a pad), Q quits (BACK on a pad), M music.
100 ' What this program shows of the language:
110 '   * a labyrinth made at random (depth first search) that lives in the RAM (POKE / PEEK, PEEK$ + TILE FILL for the visible part) and scrolls smoothly
120 '   * balls of three sizes as sprite groups (SPRITE n,SIZE s,s) whose pictures are drawn at start-up with GFX and DISK and grabbed with GFX GRAB
130 '   * a breadth first search over the rooms of the labyrinth (the distance fields that the workers use to run away from the balls or to walk to the fruit)
140 '   * up to four players on one screen with a shared camera (the players are kept inside the picture), keyboard (KEY) and controllers (JOY)
150 '   * PROC / FUNC / LOCAL, DATA / READ, EVERY (the music), SFX and PLAY, a high score file
160 ' (a FUNC has to be defined before the lines that call it, so the arrays and the functions come first, then the main program, then the procedures)
170 ' --- global variables ----------------------------------------------------------------------------------------------------------------------
180 ' The labyrinth is RW x RH rooms of 3 x 3 tiles (a tile is 16 x 24 pixels) with walls one tile thick between them: WW x WH tiles, one byte each in the RAM.
190 ' Tile codes: 1, 2 floor, 3, 4 wall, 5 bolt, 6 nut, 7 screw. DE(r), DEO(r) the door to the east of room r (width 0 = wall, offset), DS(r), DSO(r) to the south.
200 ' DIS(slot * NR + r) the distance in rooms from the room of ball p (slot p) or of fruit f (slot 4 + f), 99 = unreachable.
210 ' Balls (p = 1..4): BST 0 absent / 1 rolling / 2 destroyed / 3 out, BX,BY the centre in world pixels, BS the size 1..3, BIC items eaten towards the next size,
220 ' BDRY frames since it last ate, BSC score, BLV lives, BINV frames of protection, BDX,BDY the direction wanted.
230 ' Entities (i = 0..NE-1; the workers first, then the foremen): HT 0 worker / 1 foreman, HST 0 walks / 1 flees / 2 walks to fruit / 3 eats / 4 rusted /
240 ' 5 cornered / 6 sprays (foreman) / 9 gone, HX,HY the top left corner, HRM the room it is in, HNR the room it walks to, HSEG the part of the way (3 = at a centre).
250 DIM DE(150),DEO(150),DS(150),DSO(150),DIS(1350),QU(160),VIS(150),STK(160),CND(4)
260 DIM BST(5),BX(5),BY(5),BS(5),BIC(5),BDRY(5),BSC(5),BLV(5),BINV(5),BDX(5),BDY(5),BPLD(5),BMV(5),BDT(5),BRM(5),BOF(4)
270 DIM HT(20),HST(20),HX(20),HY(20),HRM(20),HNR(20),HDR(20),HSEG(20),HTX(20),HTY(20),HDIR(20),HFH(20),HSPD(20),HTM(20),HALR(20),HTG(20),HFC(20),HCD(20),HLD(20)
280 DIM BCR(5),FS(4),FK(4),FRMR(4),FT(4),SH(256),DXV(4),DYV(4),MELN(32),BASSN(8),PERC(8)
290 ' the frequency of a MIDI note number (69 = the A of 440 Hz), 0 = a rest
300 FUNC FREQ(M)
310   IF M<=0 THEN RETURN 0
320   RETURN 440*EXP(LOG(2)*(M-69)/12)
330 END FUNC
340 ' the tile code at a place of the labyrinth: the world is in the RAM, one byte per tile; outside of it is wall
350 FUNC TL(TX,TY)
360   IF TX<0 OR TX>=WW OR TY<0 OR TY>=WH THEN RETURN 3
370   RETURN PEEK(TY*WW+TX)
380 END FUNC
390 ' 1 if a ball of size S with its centre at CX2,CY2 is clear of the walls
400 FUNC BOXFREE(CX2,CY2,S)
410   LOCAL HW,X0,X1,Y0,Y1,X,Y,T
420   HW=8*S-3
430   X0=INT((CX2-HW)/16):X1=INT((CX2+HW)/16)
440   Y0=INT((CY2-HW)/24):Y1=INT((CY2+HW)/24)
450   FOR Y=Y0 TO Y1
460     FOR X=X0 TO X1
470       T=FN TL(X,Y)
480       IF T=3 OR T=4 THEN RETURN 0
490     NEXT X
500   NEXT Y
510   RETURN 1
520 END FUNC
530 ' may ball P go to NX,NY? (clear of the walls, and not out of the picture while moving outwards: VX, VY tell the direction)
540 FUNC CANGO(P,NX,NY,VX,VY)
550   LOCAL S
560   S=BS(P)
570   IF NX<CX+8*S+2 AND VX<0 THEN RETURN 0
580   IF NX>CX+638-8*S AND VX>0 THEN RETURN 0
590   IF NY<CY+24+8*S AND VY<0 THEN RETURN 0
600   IF NY>CY+298-8*S AND VY>0 THEN RETURN 0
610   RETURN FN BOXFREE(NX,NY,S)
620 END FUNC
630 ' the room next to room R in direction D (0 east, 1 south, 2 west, 3 north) if there is a door, else -1
640 FUNC NBR(R,D)
650   LOCAL RX
660   RX=R MOD RW
670   IF D=0 AND RX<RW-1 THEN
680     IF DE(R)>0 THEN RETURN R+1
690   END IF
700   IF D=1 AND R+RW<NR THEN
710     IF DS(R)>0 THEN RETURN R+RW
720   END IF
730   IF D=2 AND RX>0 THEN
740     IF DE(R-1)>0 THEN RETURN R-1
750   END IF
760   IF D=3 AND R>=RW THEN
770     IF DS(R-RW)>0 THEN RETURN R-RW
780   END IF
790   RETURN -1
800 END FUNC
810 ' the room next to room R in direction D whether there is a door or not, -1 at the edge of the labyrinth
820 FUNC GNB(R,D)
830   LOCAL RX
840   RX=R MOD RW
850   IF D=0 AND RX<RW-1 THEN RETURN R+1
860   IF D=1 AND R+RW<NR THEN RETURN R+RW
870   IF D=2 AND RX>0 THEN RETURN R-1
880   IF D=3 AND R>=RW THEN RETURN R-RW
890   RETURN -1
900 END FUNC
910 ' the first tile row (east / west doors) or column (north / south doors) of the door of room R in direction D
920 FUNC DOORT(R,D)
930   LOCAL RX,RY
940   RX=R MOD RW:RY=R\RW
950   IF D=0 THEN RETURN RY*4+1+DEO(R)
960   IF D=2 THEN RETURN RY*4+1+DEO(R-1)
970   IF D=1 THEN RETURN RX*4+1+DSO(R)
980   RETURN RX*4+1+DSO(R-RW)
990 END FUNC
1000 ' the room that contains a point of the world
1010 FUNC ROOMAT(PX,PY)
1020   LOCAL RX,RY
1030   RX=INT(MAX(INT(PX/16)-1,0)/4)
1040   RY=INT(MAX(INT(PY/24)-1,0)/4)
1050   IF RX>RW-1 THEN RX=RW-1
1060   IF RY>RH-1 THEN RY=RH-1
1070   RETURN RY*RW+RX
1080 END FUNC
1090 ' the distance in rooms from room R to the nearest ball that is rolling (99 if there is none)
1100 FUNC MINDIS(R)
1110   LOCAL P,D,M
1120   M=99
1130   FOR P=1 TO 4
1140     IF BST(P)=1 THEN
1150       D=DIS(P*NR+R)
1160       IF D<M THEN M=D
1170     END IF
1180   NEXT P
1190   RETURN M
1200 END FUNC
1210 ' ====================================================================== main program
1220 CALL INIT
1230 HI=FN HS_LOAD("rustball.hi")
1240 HI0=HI
1250 QT=0
1260 REPEAT
1270   CALL TITLE
1280   IF QT=0 THEN CALL PLAYGAME:QT=0
1290   IF HI>HI0 THEN
1300     ON ERROR GOTO HIERR2
1310     CALL HS_SAVE("rustball.hi",HI)
1320     LABEL HIDONE2
1330     ON ERROR OFF
1340   END IF
1350   HI0=HI
1360 UNTIL QT=1
1370 CALL TX_BYE("BEING A RUSTBALL")
1380 END
1390 ' the library hiscore reads the best score (a missing or empty file is 0, no error); a file that can't be written is ignored: the error handler is here in the main program, where RESUME <label> can continue
1400 LABEL HIERR2
1410 RESUME HIDONE2
1420 ' the EVERY handler only calls a procedure
1430 LABEL BARTIMER
1440 CALL BAR
1450 RETURN
1460 ' ====================================================================== set up (once)
1470 PROC INIT
1480   LOCAL I
1490   RANDOMIZE
1500   GFX RESET ALL
1510   RW=12:RH=8:NR=RW*RH:WW=RW*4+1:WH=RH*4+1
1520   DRYT=720
1530   DXV(0)=1:DXV(1)=0:DXV(2)=-1:DXV(3)=0
1540   DYV(0)=0:DYV(1)=1:DYV(2)=0:DYV(3)=-1
1550   BOF(1)=0:BOF(2)=1:BOF(3)=5
1560   FOR I=0 TO 7
1570     READ BASSN(I)
1580   NEXT I
1590   FOR I=0 TO 31
1600     READ MELN(I)
1610   NEXT I
1620   FOR I=0 TO 7
1630     READ PERC(I)
1640   NEXT I
1650   CALL SOUNDS
1660   CALL PALETTES
1670   CALL MAKETILES
1680   CALL MAKEIMAGES
1690   TX_PAPER=212
1700   FOR I=0 TO 3
1710     SPRITE 20+I*9,PRIORITY 5
1720   NEXT I
1730   FOR I=0 TO 19
1740     SPRITE 60+I,PRIORITY 3
1750   NEXT I
1760   FOR I=0 TO 23
1770     SPRITE 80+I,PRIORITY 6
1780   NEXT I
1790   FOR I=0 TO 3
1800     SPRITE 110+I,PRIORITY 2
1810   NEXT I
1820   SPRITE 114,PRIORITY 7
1830 END PROC
1840 ' the notes of the music: 8 bass notes (2 per bar), 32 melody notes (8 per bar), 8 percussion hits (2 per bar) as MIDI numbers
1850 DATA 33,33,29,29,31,31,28,28
1860 DATA 57,60,64,69,64,60,57,60, 53,57,60,65,60,57,53,57, 55,59,62,67,62,59,55,59, 52,56,59,64,59,56,52,56
1870 DATA 1,0,1,0,1,0,1,1
1880 PROC SOUNDS
1890   SFX 1,"W2 V0.25 ML X0,0.003,0.1,0.04 S600,1200,5"
1900   SFX 2,"W1 V0.3 ML X0,0.01,0.3,0.1 S200,900,16"
1910   SFX 3,"W3 V0.25 ML X0,0.005,0.2,0.1 S1500,300,12"
1920   SFX 4,"W3 V0.3 ML X0,0.005,0.3,0.2 S3000,500,20"
1930   SFX 5,"W1 V0.35 ML X0,0.01,0.4,0.3 S600,50,30"
1940   SFX 6,"W2 V0.12 ML H400,3 H300,3 H400,3 H300,3"
1950   SFX 7,"W2 V0.2 ML S800,200,12"
1960   SFX 8,"W1 V0.2 ML X0,0.005,0.1,0.05 S400,900,4 S900,400,4"
1970   SFX 9,"W3 V0.3 ML X0,0.005,0.25,0.2 S900,120,14"
1980 END PROC
1990 ' ====================================================================== the colors
2000 PROC PALETTES
2010   LOCAL P,I,R,G,B
2020   ' sprites (layer 4): 1 outline, 2 skin, 3 helmet, 4 vest, 5 trousers, 6 white, 7 boots, 8 foreman's vest, 9..11 rusted statue, 12 / 13 water
2030   GFX PALETTE 4,1,RGB(24,14,10):GFX PALETTE 4,2,RGB(240,190,150):GFX PALETTE 4,3,RGB(250,210,40):GFX PALETTE 4,4,RGB(240,110,20)
2040   GFX PALETTE 4,5,RGB(50,80,160):GFX PALETTE 4,6,RGB(255,255,255):GFX PALETTE 4,7,RGB(70,45,25):GFX PALETTE 4,8,RGB(40,160,70)
2050   GFX PALETTE 4,9,RGB(120,55,22):GFX PALETTE 4,10,RGB(170,85,35):GFX PALETTE 4,11,RGB(215,125,55)
2060   GFX PALETTE 4,12,RGB(170,225,255):GFX PALETTE 4,13,RGB(70,140,230):GFX PALETTE 4,14,RGB(230,60,50):GFX PALETTE 4,15,RGB(60,170,60)
2070   ' the balls 16..35: four colors of rust, five shades each, dark to light
2080   RESTORE BALLPAL
2090   FOR I=16 TO 35
2100     READ R,G,B
2110     GFX PALETTE 4,I,RGB(R,G,B)
2120   NEXT I
2130   GFX PALETTE 4,40,RGB(250,225,60):GFX PALETTE 4,41,RGB(200,160,30):GFX PALETTE 4,42,RGB(250,150,30):GFX PALETTE 4,43,RGB(210,100,20)
2140   GFX PALETTE 4,44,RGB(130,50,160):GFX PALETTE 4,45,RGB(185,115,215):GFX PALETTE 4,46,RGB(255,150,140):GFX PALETTE 4,47,RGB(110,70,30)
2150   ' tiles (layer 1): 1..3 floor, 4..8 wall, 9..11 metal, 12 / 13 gold
2160   GFX PALETTE 1,1,RGB(46,42,50):GFX PALETTE 1,2,RGB(60,55,64):GFX PALETTE 1,3,RGB(30,28,34)
2170   GFX PALETTE 1,4,RGB(112,58,34):GFX PALETTE 1,5,RGB(158,88,50):GFX PALETTE 1,6,RGB(70,34,20):GFX PALETTE 1,7,RGB(205,150,95):GFX PALETTE 1,8,RGB(90,44,26)
2180   GFX PALETTE 1,9,RGB(215,220,230):GFX PALETTE 1,10,RGB(140,146,162):GFX PALETTE 1,11,RGB(70,72,86)
2190   GFX PALETTE 1,12,RGB(240,200,70):GFX PALETTE 1,13,RGB(160,120,30)
2200   ' graphics layer (the big titles): 1 white, 2 dark, 3 gold, 4 red
2210   GFX PALETTE 3,1,RGB(255,255,255):GFX PALETTE 3,2,RGB(20,20,40):GFX PALETTE 3,3,RGB(255,200,30):GFX PALETTE 3,4,RGB(255,90,60)
2220   ' text (layer 2): 200..207 the colors of the messages, 208..211 the players, 212 an opaque dark paper
2230   GFX PALETTE 2,200,RGB(255,255,255):GFX PALETTE 2,201,RGB(255,210,50):GFX PALETTE 2,202,RGB(120,220,255):GFX PALETTE 2,203,RGB(255,100,90)
2240   GFX PALETTE 2,204,RGB(140,255,140):GFX PALETTE 2,205,RGB(180,190,230):GFX PALETTE 2,206,RGB(255,150,210):GFX PALETTE 2,207,RGB(255,170,40)
2250   GFX PALETTE 2,208,RGB(240,160,80):GFX PALETTE 2,209,RGB(240,110,100):GFX PALETTE 2,210,RGB(195,215,100):GFX PALETTE 2,211,RGB(195,155,230)
2260   GFX PALETTE 2,212,RGB(14,10,16)
2270   GFX BACKDROP RGB(0,0,0)
2280 END PROC
2290 LABEL BALLPAL
2300 DATA 60,25,10, 105,45,15, 150,75,25, 195,110,40, 240,160,80
2310 DATA 60,10,15, 105,20,25, 150,35,35, 195,60,55, 240,110,100
2320 DATA 35,45,10, 65,85,20, 100,125,35, 145,165,55, 195,215,100
2330 DATA 40,25,60, 70,45,100, 105,70,145, 145,105,185, 195,155,230
2340 ' ====================================================================== the pictures
2350 ' every picture is drawn on the graphics layer (hidden meanwhile), grabbed (4 bytes of size, then 16 x 24 pixels) and defined as a sprite or a tile
2360 PROC GRABSPRITE(IM)
2370   LOCAL G$
2380   GFX GRAB 0,0,15,23,G$
2390   SPRITE DEF IM,MID$(G$,5)
2400 END PROC
2410 PROC GRABTILE(N)
2420   LOCAL G$
2430   GFX GRAB 0,0,15,23,G$
2440   TILE DEF N,MID$(G$,5)
2450 END PROC
2460 PROC MAKEIMAGES
2470   LOCAL CO,DR,F,P,S,K,J,G$,IB
2480   GFX LAYER 3,OFF
2490   GFX TARGET 3
2500   ' images 0..15 the people (costume * 8 + direction * 4 + frame), 16 / 17 the rusted statue
2510   FOR CO=0 TO 1
2520     FOR DR=0 TO 1
2530       FOR F=0 TO 3
2540         CALL DRAWMAN(CO,DR,F,0)
2550         CALL GRABSPRITE(CO*8+DR*4+F)
2560       NEXT F
2570     NEXT DR
2580   NEXT CO
2590   FOR DR=0 TO 1
2600     CALL DRAWMAN(0,DR,0,1)
2610     CALL GRABSPRITE(16+DR)
2620   NEXT DR
2630   ' 18 / 19 the water jet sideways, 20 / 21 up and down
2640   FOR F=0 TO 1
2650     CALL DRAWJET(0,F)
2660     CALL GRABSPRITE(18+F)
2670     CALL DRAWJET(1,F)
2680     CALL GRABSPRITE(20+F)
2690   NEXT F
2700   ' 24..27 the fruit: apple, banana, orange, grapes
2710   FOR K=0 TO 3
2720     CALL DRAWFRUIT(K)
2730     CALL GRABSPRITE(24+K)
2740   NEXT K
2750   ' 100.. the balls: for each player and each of two frames 14 images (1 + 4 + 9)
2760   FOR P=1 TO 4
2770     FOR F=0 TO 1
2780       FOR S=1 TO 3
2790         CALL DRAWBALL(S,P)
2800         IB=100+((P-1)*2+F)*14+BOF(S)
2810         FOR K=0 TO S-1
2820           FOR J=0 TO S-1
2830             GFX GRAB J*16,K*24,J*16+15,K*24+23,G$
2840             SPRITE DEF IB+K*S+J,MID$(G$,5)
2850           NEXT J
2860         NEXT K
2870       NEXT S
2880     NEXT F
2890   NEXT P
2900   ' 30..37 the arrow of the radar, 8 directions (0 east, 2 south, clockwise)
2910   FOR K=0 TO 7
2920     CALL DRAWARROW(K)
2930     CALL GRABSPRITE(30+K)
2940   NEXT K
2950   GFX CLEAR
2960   GFX LAYER 3,ON
2970 END PROC
2980 ' an arrow in a 16 x 24 picture that points in direction K * 45 degrees (screen coordinates, y down)
2990 PROC DRAWARROW(K)
3000   LOCAL A,OX,OY
3010   GFX LINE 0,0,15,23,0,BF
3020   A=K*0.7854
3030   FOR OY=-1 TO 1
3040     FOR OX=-1 TO 1
3050       CALL ARROWLINES(A,OX,OY,1)
3060     NEXT OX
3070   NEXT OY
3080   CALL ARROWLINES(A,0,0,3)
3090 END PROC
3100 PROC ARROWLINES(A,OX,OY,C)
3110   LOCAL TX,TY
3120   TX=8+OX+INT(COS(A)*7+0.5):TY=12+OY+INT(SIN(A)*7+0.5)
3130   GFX LINE 8+OX-INT(COS(A)*6+0.5),12+OY-INT(SIN(A)*6+0.5),TX,TY,C
3140   GFX LINE TX,TY,TX-INT(COS(A+0.6)*5+0.5),TY-INT(SIN(A+0.6)*5+0.5),C
3150   GFX LINE TX,TY,TX-INT(COS(A-0.6)*5+0.5),TY-INT(SIN(A-0.6)*5+0.5),C
3160 END PROC
3170 ' a box in a 16 x 24 picture, mirrored if MRR is 1 (a person looks to the right in the drawing and to the left when mirrored); STM 1 draws a statue
3180 PROC MB(X1,Y1,X2,Y2,C)
3190   LOCAL CC
3200   CC=C
3210   IF STM=1 THEN
3220     CC=10
3230     IF C=1 OR C=7 THEN CC=9
3240     IF C=2 OR C=6 OR C=3 THEN CC=11
3250   END IF
3260   IF MRR=1 THEN
3270     GFX LINE 15-X2,Y1,15-X1,Y2,CC,BF
3280   ELSE
3290     GFX LINE X1,Y1,X2,Y2,CC,BF
3300   END IF
3310 END PROC
3320 ' a person: CO 0 worker / 1 foreman, DR 0 right / 1 left, F 0 and 1 walking, 2 eating (worker) or spraying (foreman), 3 scared; SM 1 a rusted statue
3330 PROC DRAWMAN(CO,DR,F,SM)
3340   LOCAL VEST,CAP
3350   MRR=DR:STM=SM
3360   VEST=4:CAP=3
3370   IF CO=1 THEN VEST=8:CAP=6
3380   GFX LINE 0,0,15,23,0,BF
3390   IF F=1 THEN
3400     CALL MB(4,15,6,22,5):CALL MB(10,15,12,22,5):CALL MB(3,21,6,23,7):CALL MB(10,21,13,23,7)
3410   ELSE
3420     CALL MB(6,15,8,22,5):CALL MB(9,15,11,22,5):CALL MB(6,21,9,23,7):CALL MB(9,21,12,23,7)
3430   END IF
3440   CALL MB(5,7,10,14,VEST)
3450   CALL MB(5,10,10,10,6)
3460   CALL MB(5,13,10,14,7)
3470   CALL MB(6,2,11,6,2)
3480   CALL MB(5,0,12,2,CAP):CALL MB(11,2,13,2,CAP)
3490   CALL MB(10,4,10,4,1)
3500   IF F=2 THEN
3510     IF CO=0 THEN
3520       CALL MB(6,8,9,9,VEST):CALL MB(9,5,12,8,2):CALL MB(11,3,13,5,14)
3530     ELSE
3540       CALL MB(6,8,12,9,VEST):CALL MB(13,7,15,10,13):CALL MB(15,8,15,9,12)
3550     END IF
3560   ELSE
3570     IF F=3 THEN
3580       CALL MB(2,1,3,8,VEST):CALL MB(2,0,3,1,2):CALL MB(12,1,13,8,VEST):CALL MB(12,0,13,1,2):CALL MB(9,6,10,6,1)
3590     ELSE
3600       CALL MB(6,8,8,12,VEST):CALL MB(6,12,8,13,2)
3610       IF CO=1 THEN
3620         CALL MB(8,10,12,11,13):CALL MB(13,10,14,11,12)
3630       END IF
3640     END IF
3650   END IF
3660 END PROC
3670 ' the water of the hose: DIRN 0 sideways, 1 up and down; F 0 / 1 the drops are in two places
3680 PROC DRAWJET(DIRN,F)
3690   LOCAL K,X,Y
3700   GFX LINE 0,0,15,23,0,BF
3710   FOR K=0 TO 3
3720     IF DIRN=0 THEN
3730       X=(F*4+K*5) MOD 14
3740       GFX LINE X,8,X+2,9,12,BF:GFX LINE (X+7) MOD 14,12,(X+7) MOD 14+2,13,13,BF:GFX LINE (X+3) MOD 14,15,(X+3) MOD 14+2,16,12,BF
3750     ELSE
3760       Y=(F*6+K*7) MOD 21
3770       GFX LINE 4,Y,5,Y+2,12,BF:GFX LINE 8,(Y+10) MOD 21,9,(Y+10) MOD 21+2,13,BF:GFX LINE 11,(Y+5) MOD 21,12,(Y+5) MOD 21+2,12,BF
3780     END IF
3790   NEXT K
3800 END PROC
3810 ' K 0 apple, 1 banana, 2 orange, 3 grapes
3820 PROC DRAWFRUIT(K)
3830   GFX LINE 0,0,15,23,0,BF
3840   IF K=0 THEN
3850     CALL DISK(8,14,6,14):CALL DISK(10,16,3,14):GFX LINE 4,10,6,11,46,BF:GFX LINE 8,6,8,8,47,BF:GFX LINE 9,5,12,6,15,BF
3860   END IF
3870   IF K=1 THEN
3880     GFX LINE 3,16,6,19,40,BF:GFX LINE 5,13,9,16,40,BF:GFX LINE 8,10,12,13,40,BF:GFX LINE 11,7,13,10,40,BF
3890     GFX LINE 3,19,6,19,41,BF:GFX LINE 5,16,9,16,41,BF:GFX LINE 12,13,12,13,41:GFX LINE 13,6,14,7,47,BF
3900   END IF
3910   IF K=2 THEN
3920     CALL DISK(8,14,6,42):CALL DISK(10,16,3,43):GFX LINE 5,10,6,11,40,BF:GFX LINE 8,7,9,8,15,BF
3930   END IF
3940   IF K=3 THEN
3950     CALL DISK(5,11,3,44):CALL DISK(11,11,3,44):CALL DISK(8,14,3,44):CALL DISK(5,17,3,44):CALL DISK(11,17,3,44):CALL DISK(8,20,3,44)
3960     GFX LINE 4,10,5,10,45:GFX LINE 10,10,11,10,45:GFX LINE 7,13,8,13,45:GFX LINE 8,5,8,8,47,BF:GFX LINE 9,4,12,5,15,BF
3970   END IF
3980 END PROC
3990 ' a ball of size S for player P in the box 16 S x 24 S at the top left corner of the graphics layer: a rusty ball with a cross face
4000 PROC DRAWBALL(S,P)
4010   LOCAL B0,R,CX3,CY3,K,A,RR,X,Y
4020   B0=16+(P-1)*5
4030   R=8*S-1:CX3=8*S:CY3=12*S
4040   GFX LINE 0,0,47,71,0,BF
4050   CALL DISK(CX3,CY3,R,B0)
4060   CALL DISK(CX3-INT(R/12)-1,CY3-INT(R/12)-1,INT(R*0.92),B0+1)
4070   CALL DISK(CX3-INT(R/6)-1,CY3-INT(R/6)-1,INT(R*0.76),B0+2)
4080   CALL DISK(CX3-INT(R/4)-1,CY3-INT(R/4)-1,INT(R*0.52),B0+3)
4090   CALL DISK(CX3-INT(R/3)-1,CY3-INT(R/3)-1,INT(R*0.24),B0+4)
4100   ' rust pits and flakes
4110   FOR K=1 TO 7*S
4120     A=RND(1)*6.2832:RR=RND(1)*R*0.85
4130     X=CX3+INT(COS(A)*RR):Y=CY3+INT(SIN(A)*RR)
4140     GFX LINE X,Y,X+S-1,Y,B0+(K MOD 2)*3
4150   NEXT K
4160   ' the face
4170   CALL DISK(CX3-3*S,CY3-S,2*S,6):CALL DISK(CX3+3*S,CY3-S,2*S,6)
4180   CALL DISK(CX3-3*S+S,CY3-S,S,1):CALL DISK(CX3+3*S-S,CY3-S,S,1)
4190   GFX LINE CX3-5*S,CY3-4*S,CX3-S,CY3-2*S-1,1:GFX LINE CX3+5*S,CY3-4*S,CX3+S,CY3-2*S-1,1
4200   IF S>1 THEN
4210     GFX LINE CX3-4*S,CY3+3*S,CX3+4*S,CY3+3*S,1,BF
4220     FOR K=0 TO 3
4230       GFX LINE CX3-3*S+K*2*S,CY3+3*S,CX3-3*S+K*2*S+S-1,CY3+4*S-1,6,BF
4240     NEXT K
4250   ELSE
4260     GFX LINE 4,16,11,16,1
4270     GFX PSET 6,17,6:GFX PSET 9,17,6
4280   END IF
4290 END PROC
4300 ' ====================================================================== the tiles
4310 PROC FLOORTILE(N)
4320   LOCAL K
4330   GFX LINE 0,0,15,23,N,BF
4340   GFX LINE 15,0,15,23,3:GFX LINE 0,23,15,23,3
4350   FOR K=1 TO 7
4360     GFX PSET INT(RND(1)*14),INT(RND(1)*22),1+(K MOD 3)
4370   NEXT K
4380 END PROC
4390 PROC MAKETILES
4400   LOCAL K,X,Y
4410   GFX LAYER 3,OFF
4420   GFX TARGET 3
4430   CALL FLOORTILE(1):CALL GRABTILE(1)
4440   CALL FLOORTILE(2):CALL GRABTILE(2)
4450   ' steel plates, rusty, with rivets
4460   FOR K=3 TO 4
4470     GFX LINE 0,0,15,23,4,BF
4480     GFX LINE 0,0,15,1,5,BF:GFX LINE 0,0,0,23,5:GFX LINE 0,22,15,23,6,BF:GFX LINE 15,0,15,23,6
4490     GFX LINE 2,3,3,4,7,BF:GFX LINE 12,3,13,4,7,BF:GFX LINE 2,19,3,20,7,BF:GFX LINE 12,19,13,20,7,BF
4500     IF K=4 THEN
4510       GFX LINE 1,11,14,12,6,BF:GFX LINE 1,10,14,10,5
4520     ELSE
4530       GFX LINE 3,8,12,15,8,BF:GFX LINE 3,8,12,8,5
4540     END IF
4550     FOR X=1 TO 9
4560       Y=INT(RND(1)*14)+1
4570       GFX LINE Y,INT(RND(1)*10)+4,Y,INT(RND(1)*10)+11,6
4580     NEXT X
4590     CALL GRABTILE(K)
4600   NEXT K
4610   ' 5 a bolt, 6 a nut, 7 a screw, on the floor
4620   CALL FLOORTILE(1)
4630   GFX LINE 3,7,8,16,11,BF:GFX LINE 4,8,7,14,10,BF:GFX LINE 4,8,7,9,9,BF:GFX LINE 9,9,14,14,11,BF:GFX LINE 9,10,14,13,10,BF
4640   GFX LINE 10,9,10,14,11:GFX LINE 12,9,12,14,11
4650   CALL GRABTILE(5)
4660   CALL FLOORTILE(1)
4670   CALL DISK(8,12,7,11):CALL DISK(8,12,6,10):CALL DISK(8,12,3,11):CALL DISK(8,12,2,1)
4680   GFX LINE 3,8,8,6,9:GFX LINE 4,9,5,15,9
4690   CALL GRABTILE(6)
4700   CALL FLOORTILE(1)
4710   CALL DISK(8,7,5,13):CALL DISK(7,6,4,12):GFX LINE 4,7,11,7,13:GFX LINE 7,12,9,21,13,BF:GFX LINE 7,12,7,21,12
4720   FOR K=0 TO 3
4730     GFX LINE 7,13+K*2,9,14+K*2,13
4740   NEXT K
4750   CALL GRABTILE(7)
4760   GFX CLEAR
4770   GFX LAYER 3,ON
4780 END PROC
4790 ' ====================================================================== the labyrinth
4800 PROC OPENDOOR(R,D)
4810   LOCAL Q,W,O
4820   Q=FN RN(10)
4830   W=3
4840   IF Q<3 THEN W=1
4850   IF Q>=3 AND Q<6 THEN W=2
4860   O=FN RN(4-W)
4870   IF D=0 THEN DE(R)=W:DEO(R)=O
4880   IF D=1 THEN DS(R)=W:DSO(R)=O
4890   IF D=2 THEN DE(R-1)=W:DEO(R-1)=O
4900   IF D=3 THEN DS(R-RW)=W:DSO(R-RW)=O
4910 END PROC
4920 ' the rooms are joined by a random spanning tree (depth first search), then a few more walls are opened so that there are loops
4930 PROC GENMAZE
4940   LOCAL R,D,N,SP,CNT,K
4950   FOR R=0 TO NR-1
4960     DE(R)=0:DS(R)=0:DEO(R)=0:DSO(R)=0:VIS(R)=0
4970   NEXT R
4980   STK(0)=SRM:VIS(SRM)=1:SP=1
4990   WHILE SP>0
5000     R=STK(SP-1)
5010     CNT=0
5020     FOR D=0 TO 3
5030       N=FN GNB(R,D)
5040       IF N>=0 THEN
5050         IF VIS(N)=0 THEN
5060           CND(CNT)=D:CNT=CNT+1
5070         END IF
5080       END IF
5090     NEXT D
5100     IF CNT=0 THEN
5110       SP=SP-1
5120     ELSE
5130       D=CND(FN RN(CNT))
5140       N=FN GNB(R,D)
5150       CALL OPENDOOR(R,D)
5160       VIS(N)=1:STK(SP)=N:SP=SP+1
5170     END IF
5180   WEND
5190   FOR K=1 TO NR\8
5200     R=FN RN(NR):D=FN RN(4)
5210     N=FN GNB(R,D)
5220     IF N>=0 THEN
5230       IF FN NBR(R,D)<0 THEN CALL OPENDOOR(R,D)
5240     END IF
5250   NEXT K
5260 END PROC
5270 ' the tiles of the labyrinth into the RAM
5280 PROC BUILDWORLD
5290   LOCAL X,Y,R,RX,RY,I,T
5300   FOR Y=0 TO WH-1
5310     FOR X=0 TO WW-1
5320       T=3
5330       IF (X*7+Y*13) MOD 5=0 THEN T=4
5340       POKE Y*WW+X,T,1
5350     NEXT X
5360   NEXT Y
5370   FOR R=0 TO NR-1
5380     RX=R MOD RW:RY=R\RW
5390     FOR Y=1 TO 3
5400       FOR X=1 TO 3
5410         POKE (RY*4+Y)*WW+RX*4+X,1+((RX*4+X+RY*4+Y) MOD 2),1
5420       NEXT X
5430     NEXT Y
5440     IF DE(R)>0 THEN
5450       FOR I=0 TO DE(R)-1
5460         POKE (RY*4+1+DEO(R)+I)*WW+(RX+1)*4,1,1
5470       NEXT I
5480     END IF
5490     IF DS(R)>0 THEN
5500       FOR I=0 TO DS(R)-1
5510         POKE (RY+1)*4*WW+RX*4+1+DSO(R)+I,1,1
5520       NEXT I
5530     END IF
5540   NEXT R
5550 END PROC
5560 ' N new screws, nuts and bolts on free floor tiles
5570 PROC SPAWNITEMS(N)
5580   LOCAL K,R,X,Y,A,T
5590   FOR K=1 TO N
5600     R=FN RN(NR)
5610     X=(R MOD RW)*4+1+FN RN(3):Y=(R\RW)*4+1+FN RN(3)
5620     A=Y*WW+X
5630     T=PEEK(A)
5640     IF T=1 OR T=2 THEN
5650       POKE A,5+FN RN(3),1
5660       ITM=ITM+1
5670       CALL SETTILE(X,Y)
5680     END IF
5690   NEXT K
5700 END PROC
5710 ' breadth first search: the distance in rooms from room S to every room, into DIS(B * NR + room)
5720 PROC BFS(S,B)
5730   LOCAL H,T,R,D,N
5740   FOR R=0 TO NR-1
5750     DIS(B*NR+R)=99
5760   NEXT R
5770   DIS(B*NR+S)=0
5780   QU(0)=S:H=0:T=1
5790   WHILE H<T
5800     R=QU(H):H=H+1
5810     FOR D=0 TO 3
5820       N=FN NBR(R,D)
5830       IF N>=0 THEN
5840         IF DIS(B*NR+N)=99 THEN
5850           DIS(B*NR+N)=DIS(B*NR+R)+1
5860           QU(T)=N:T=T+1
5870         END IF
5880       END IF
5890     NEXT D
5900   WEND
5910 END PROC
5920 ' ====================================================================== the camera and the map
5930 ' The tile layer is a window of 41 x 13 cells; TILE SCROLL moves it by up to 15 / 23 pixels, and what is pushed in at the left or top comes from the
5940 ' last column / row. So that the world tile in column W is at x = W*16 - CX exactly, the cell I holds the world column OCOL+1+I (I = 0..39), the last
5950 ' cell (it is off the screen at the right) holds OCOL for the strip that is pushed in at the left, and the rows work the same way (the strip at the top is
5960 ' hidden behind the two text lines of the score bar). PEEK$ cuts the 13 rows out of the RAM, TILE FILL shows them.
5970 PROC DRAWMAP
5980   LOCAL J,M$,A
5990   FOR J=0 TO 12
6000     A=(OROW+1+J)*WW+OCOL
6010     M$=M$+PEEK$(A+1,40)+PEEK$(A,1)
6020   NEXT J
6030   TILE FILL M$
6040 END PROC
6050 PROC SHOWCAM
6060   LOCAL NC,NR2
6070   NC=(CX-1)\16:NR2=(CY-1)\24
6080   IF NC<>OCOL OR NR2<>OROW THEN
6090     OCOL=NC:OROW=NR2
6100     CALL DRAWMAP
6110   END IF
6120   TILE SCROLL 15-(CX-1) MOD 16,23-(CY-1) MOD 24
6130 END PROC
6140 ' the camera follows the middle of the balls that are in the game
6150 PROC CAMERA
6160   LOCAL P,N,SX,SY,TX,TY
6170   N=0:SX=0:SY=0
6180   FOR P=1 TO 4
6190     IF BST(P)=1 OR BST(P)=2 THEN
6200       N=N+1:SX=SX+BX(P):SY=SY+BY(P)
6210     END IF
6220   NEXT P
6230   IF N=0 THEN EXIT PROC
6240   TX=FN CLAMP(INT(SX/N)-320,1,(WW-41)*16)
6250   TY=FN CLAMP(INT(SY/N)-150,1,(WH-13)*24)
6260   CX=FN CLAMP(CX+(TX-CX)\5,1,(WW-41)*16)
6270   CY=FN CLAMP(CY+(TY-CY)\5,1,(WH-13)*24)
6280   CALL SHOWCAM
6290 END PROC
6300 ' the camera jumps to a place at once
6310 PROC CAMSNAP(X,Y)
6320   CX=FN CLAMP(X,1,(WW-41)*16)
6330   CY=FN CLAMP(Y,1,(WH-13)*24)
6340   OCOL=-99
6350   CALL SHOWCAM
6360 END PROC
6370 ' a tile of the world has changed: show it if it is on the screen
6380 PROC SETTILE(TC,TR)
6390   IF TC=OCOL THEN CALL DRAWMAP
6400   IF TC>OCOL AND TC<=OCOL+40 AND TR>OROW AND TR<=OROW+13 THEN TILE SET TC-OCOL-1,TR-OROW-1,PEEK(TR*WW+TC)
6410 END PROC
6420 ' a sprite at a place of the world: it is hidden if that is not on the screen
6430 PROC PLACE(N,X,Y,IMG)
6440   LOCAL SX,SY
6450   SX=INT(X-CX):SY=INT(Y-CY)
6460   IF SX<-40 OR SX>650 OR SY<24 OR SY>330 THEN
6470     CALL HIDE(N)
6480     EXIT PROC
6490   END IF
6500   SPRITE N,SX,SY,IMG
6510   IF SH(N)=0 THEN
6520     SPRITE N,ON
6530     SH(N)=1
6540   END IF
6550 END PROC
6560 PROC HIDE(N)
6570   IF SH(N)=1 THEN
6580     SPRITE N,OFF
6590     SH(N)=0
6600   END IF
6610 END PROC
6620 ' ====================================================================== text
6630 PROC HUD
6640   LOCAL P,T$
6650   TXT FILL 0,0,79,1,32,205,212
6660   FOR P=1 TO 4
6670     IF BPLD(P)=1 THEN
6680       T$="P"+STR$(P)+" "+FN PAD$(BSC(P),6)+" L"+STR$(BLV(P))+" "+LEFT$("ooooo",BIC(P))+LEFT$(".....",5-BIC(P))
6690       CALL TXTAT((P-1)*20,0,T$,207+P)
6700     END IF
6710   NEXT P
6720   T$=" LEVEL "+STR$(LEVL)+"   HUMANS LEFT "+STR$(HLEFT)+"   CHAIN x"+STR$(CHN)+"   BEST "+FN PAD$(HI,6)+"  "
6730   CALL TXTAT(0,1,T$,205)
6740   LOCATE 79,24
6750   HF=0
6760 END PROC
6770 ' ====================================================================== keys and pads
6780 PROC CTRL
6790   IF FN IN_SERVICE() THEN CALL PAUSE
6800   IF IN_KEY=77 THEN CALL MUSICSWITCH
6810   IF IN_QUIT THEN QT=1
6820 END PROC
6830 PROC PAUSE
6840   CALL MUSICOFF
6850   CALL TXTAT(33,10,"  PAUSED  ",201)
6860   CALL IN_PAUSELOOP
6870   IF IN_QUIT THEN QT=1
6880   TXT FILL 33,10,42,10,32,0,0
6890   IF QT=0 THEN CALL MUSICON
6900 END PROC
6910 ' controller mode: a pad that is connected brings its player, a pad that is removed takes the player away (lives and score stay)
6920 PROC JOINS
6930   LOCAL P
6940   IF GM<>2 THEN EXIT PROC
6950   FOR P=1 TO 4
6960     IF JOY("CONNECTED",P) THEN
6970       IF BST(P)=0 AND BLV(P)>0 THEN
6980         BPLD(P)=1
6990         CALL SPAWNBALL(P)
7000         SFX 8
7010         HF=1
7020       END IF
7030     ELSE
7040       IF BST(P)=1 OR BST(P)=2 THEN
7050         BST(P)=0
7060         CALL HIDE(20+(P-1)*9)
7070       END IF
7080     END IF
7090   NEXT P
7100 END PROC
7110 PROC READIN
7120   LOCAL P,X,Y,M,L,R,U,D
7130   FOR P=1 TO 4
7140     BDX(P)=0:BDY(P)=0
7150   NEXT P
7160   IF GM=1 THEN
7170     L=KEY("A"):R=KEY("D"):U=KEY("W"):D=KEY("S")
7180     IF NK=1 THEN
7190       L=L OR KEY("LEFT"):R=R OR KEY("RIGHT"):U=U OR KEY("UP"):D=D OR KEY("DOWN")
7200     END IF
7210     IF R THEN BDX(1)=1
7220     IF L THEN BDX(1)=BDX(1)-1
7230     IF D THEN BDY(1)=1
7240     IF U THEN BDY(1)=BDY(1)-1
7250     IF NK=2 THEN
7260       IF KEY("RIGHT") THEN BDX(2)=1
7270       IF KEY("LEFT") THEN BDX(2)=BDX(2)-1
7280       IF KEY("DOWN") THEN BDY(2)=1
7290       IF KEY("UP") THEN BDY(2)=BDY(2)-1
7300     END IF
7310     FOR P=1 TO NK
7320       IF BDX(P)<>0 AND BDY(P)<>0 THEN BDX(P)=BDX(P)*0.72:BDY(P)=BDY(P)*0.72
7330     NEXT P
7340   ELSE
7350     FOR P=1 TO 4
7360       IF BST(P)=1 THEN
7370         X=JOY("LX",P):Y=JOY("LY",P)
7380         IF JOY("LEFT",P) THEN X=-1
7390         IF JOY("RIGHT",P) THEN X=1
7400         IF JOY("UP",P) THEN Y=-1
7410         IF JOY("DOWN",P) THEN Y=1
7420         M=SQR(X*X+Y*Y)
7430         IF M>1 THEN X=X/M:Y=Y/M
7440         BDX(P)=X:BDY(P)=Y
7450       END IF
7460     NEXT P
7470   END IF
7480 END PROC
7490 ' ====================================================================== the balls
7500 PROC SPAWNBALL(P)
7510   LOCAL SX,SY
7520   SX=(SRM MOD RW*4+2)*16+8:SY=(SRM\RW*4+2)*24+12
7530   BX(P)=SX+((P-1) MOD 2)*24-12:BY(P)=SY+((P-1)\2)*30-15
7540   BS(P)=1:BIC(P)=0:BDRY(P)=0:BCR(P)=0:BINV(P)=150:BMV(P)=0:BRM(P)=-1:BST(P)=1:BPLD(P)=1
7550   SPRITE 20+(P-1)*9,SIZE 1,1
7560   HF=1
7570 END PROC
7580 PROC OUTBALL(P)
7590   BST(P)=3
7600   CALL HIDE(20+(P-1)*9)
7610   HF=1
7620 END PROC
7630 PROC UPDBALL(P)
7640   IF BST(P)=0 OR BST(P)=3 THEN EXIT PROC
7650   IF BST(P)=2 THEN
7660     BDT(P)=BDT(P)-1
7670     IF BDT(P)<=0 THEN
7680       IF BLV(P)>0 THEN CALL SPAWNBALL(P) ELSE CALL OUTBALL(P)
7690     END IF
7700     EXIT PROC
7710   END IF
7720   IF BINV(P)>0 THEN BINV(P)=BINV(P)-1
7730   CALL MOVEBALL(P)
7740   CALL EATITEMS(P)
7750   BDRY(P)=BDRY(P)+1
7760   IF BDRY(P)>=DRYT THEN CALL DRYOUT(P)
7770 END PROC
7780 PROC MOVEBALL(P)
7790   LOCAL S,SP,VX,VY,NX,NY,BL
7800   S=BS(P)
7810   SP=1.5+0.65*(S-1)
7820   VX=BDX(P)*SP:VY=BDY(P)*SP
7830   BMV(P)=0:BL=0
7840   IF VX<>0 OR VY<>0 THEN BMV(P)=1
7850   IF VX<>0 THEN
7860     NX=BX(P)+VX
7870     IF FN CANGO(P,NX,BY(P),VX,0) THEN
7880       BX(P)=NX
7890     ELSE
7900       BL=1
7910       CALL NUDGE(P,NX,BY(P),0)
7920     END IF
7930   END IF
7940   IF VY<>0 THEN
7950     NY=BY(P)+VY
7960     IF FN CANGO(P,BX(P),NY,0,VY) THEN
7970       BY(P)=NY
7980     ELSE
7990       BL=1
8000       CALL NUDGE(P,BX(P),NY,1)
8010     END IF
8020   END IF
8030   ' a ball of size 2 or 3 that keeps pushing against a wall crushes it
8040   IF BL=1 AND S>=2 THEN
8050     BCR(P)=BCR(P)+1
8060     IF BCR(P)>=45 THEN CALL CRUSH(P)
8070   ELSE
8080     BCR(P)=0
8090   END IF
8100 END PROC
8110 ' the wall tiles in front of the ball (the box moved on by 4 pixels in the direction it pushes) turn into floor; the outer wall of the labyrinth stays
8120 PROC CRUSH(P)
8130   LOCAL S,HW,CX2,CY2,X,Y,X0,X1,Y0,Y1,T,N
8140   S=BS(P):HW=8*S-3
8150   CX2=BX(P)+SGN(BDX(P))*4:CY2=BY(P)+SGN(BDY(P))*4
8160   X0=INT((CX2-HW)/16):X1=INT((CX2+HW)/16)
8170   Y0=INT((CY2-HW)/24):Y1=INT((CY2+HW)/24)
8180   N=0
8190   FOR Y=Y0 TO Y1
8200     FOR X=X0 TO X1
8210       IF X>0 AND X<WW-1 AND Y>0 AND Y<WH-1 THEN
8220         T=FN TL(X,Y)
8230         IF T=3 OR T=4 THEN
8240           POKE Y*WW+X,1+((X+Y) MOD 2),1
8250           CALL SETTILE(X,Y)
8260           N=N+1
8270         END IF
8280       END IF
8290     NEXT X
8300   NEXT Y
8310   BCR(P)=0
8320   IF N>0 THEN
8330     SFX 9
8340     BSC(P)=BSC(P)+N*2
8350     HF=1
8360   END IF
8370 END PROC
8380 ' the ball is blocked at a corner: it slides sideways when that gets it round (AX 0: blocked going sideways, slide up or down; 1: the other way round)
8390 PROC NUDGE(P,NX,NY,AX)
8400   LOCAL K,S,UPK,DNK,STP
8410   S=BS(P):UPK=0:DNK=0:STP=1.2
8420   FOR K=2 TO 12 STEP 2
8430     IF AX=0 THEN
8440       IF UPK=0 AND FN BOXFREE(NX,NY-K,S) THEN UPK=K
8450       IF DNK=0 AND FN BOXFREE(NX,NY+K,S) THEN DNK=K
8460     ELSE
8470       IF UPK=0 AND FN BOXFREE(NX-K,NY,S) THEN UPK=K
8480       IF DNK=0 AND FN BOXFREE(NX+K,NY,S) THEN DNK=K
8490     END IF
8500   NEXT K
8510   IF UPK>0 AND (DNK=0 OR UPK<=DNK) THEN
8520     IF AX=0 THEN
8530       IF FN BOXFREE(BX(P),BY(P)-STP,S) THEN BY(P)=BY(P)-STP
8540     ELSE
8550       IF FN BOXFREE(BX(P)-STP,BY(P),S) THEN BX(P)=BX(P)-STP
8560     END IF
8570   ELSE
8580     IF DNK>0 THEN
8590       IF AX=0 THEN
8600         IF FN BOXFREE(BX(P),BY(P)+STP,S) THEN BY(P)=BY(P)+STP
8610       ELSE
8620         IF FN BOXFREE(BX(P)+STP,BY(P),S) THEN BX(P)=BX(P)+STP
8630       END IF
8640     END IF
8650   END IF
8660 END PROC
8670 ' the ball eats the screws, nuts and bolts it touches; five of them make it one size bigger, if there is room for that
8680 PROC EATITEMS(P)
8690   LOCAL X,Y,X0,X1,Y0,Y1,T,HW,S
8700   S=BS(P):HW=8*S-1
8710   X0=INT((BX(P)-HW)/16):X1=INT((BX(P)+HW)/16)
8720   Y0=INT((BY(P)-HW)/24):Y1=INT((BY(P)+HW)/24)
8730   FOR Y=Y0 TO Y1
8740     FOR X=X0 TO X1
8750       T=FN TL(X,Y)
8760       IF T>=5 AND T<=7 THEN
8770         POKE Y*WW+X,1+((X+Y) MOD 2),1
8780         CALL SETTILE(X,Y)
8790         ITM=ITM-1
8800         BDRY(P)=0
8810         IF BIC(P)<5 THEN
8820           BIC(P)=BIC(P)+1
8830           BSC(P)=BSC(P)+10
8840         ELSE
8850           BSC(P)=BSC(P)+5
8860         END IF
8870         SFX 1
8880         HF=1
8890       END IF
8900     NEXT X
8910   NEXT Y
8920   IF BIC(P)>=5 AND S<3 THEN
8930     IF FN BOXFREE(BX(P),BY(P),S+1) THEN
8940       BS(P)=S+1:BIC(P)=0
8950       SPRITE 20+(P-1)*9,SIZE S+1,S+1
8960       SFX 2
8970       HF=1
8980     END IF
8990   END IF
9000 END PROC
9010 PROC DRYOUT(P)
9020   BDRY(P)=0
9030   IF BIC(P)>0 THEN
9040     BIC(P)=BIC(P)-1
9050     HF=1
9060   ELSE
9070     IF BS(P)>1 THEN
9080       BS(P)=BS(P)-1:BIC(P)=4
9090       SPRITE 20+(P-1)*9,SIZE BS(P),BS(P)
9100       SFX 7
9110       HF=1
9120     END IF
9130   END IF
9140 END PROC
9150 ' a ball is hit by the water of a hose: one size smaller, or (as the smallest) destroyed
9160 PROC HURT(P)
9170   IF BINV(P)>0 THEN EXIT PROC
9180   SFX 4
9190   IF BS(P)>1 THEN
9200     BS(P)=BS(P)-1:BIC(P)=0
9210     SPRITE 20+(P-1)*9,SIZE BS(P),BS(P)
9220     BINV(P)=120
9230   ELSE
9240     BLV(P)=BLV(P)-1
9250     BST(P)=2:BDT(P)=100:BINV(P)=0
9260     SFX 5
9270   END IF
9280   HF=1
9290 END PROC
9300 ' the rooms of the balls: a new distance field when a ball has come into another room
9310 PROC UPDDIS
9320   LOCAL P,R
9330   FOR P=1 TO 4
9340     IF BST(P)=1 THEN
9350       R=FN ROOMAT(BX(P),BY(P))
9360       IF R<>BRM(P) THEN
9370         BRM(P)=R
9380         CALL BFS(R,P)
9390       END IF
9400     END IF
9410   NEXT P
9420 END PROC
9430 ' ====================================================================== the people
9440 ' the next place to walk to on the way from the centre of room HRM to the centre of room HNR through the door in direction HDR (three parts)
9450 PROC SETWP(I)
9460   LOCAL A,B,D,AX,AY,BX2,BY2,DR
9470   A=HRM(I):B=HNR(I):D=HDR(I)
9480   AX=(A MOD RW)*4+2:AY=(A\RW)*4+2:BX2=(B MOD RW)*4+2:BY2=(B\RW)*4+2
9490   DR=FN DOORT(A,D)
9500   IF D=0 OR D=2 THEN
9510     IF HSEG(I)=0 THEN HTX(I)=AX*16:HTY(I)=DR*24
9520     IF HSEG(I)=1 THEN HTX(I)=BX2*16:HTY(I)=DR*24
9530     IF HSEG(I)=2 THEN HTX(I)=BX2*16:HTY(I)=BY2*24
9540   ELSE
9550     IF HSEG(I)=0 THEN HTX(I)=DR*16:HTY(I)=AY*24
9560     IF HSEG(I)=1 THEN HTX(I)=DR*16:HTY(I)=BY2*24
9570     IF HSEG(I)=2 THEN HTX(I)=BX2*16:HTY(I)=BY2*24
9580   END IF
9590 END PROC
9600 ' a worker who is not afraid looks for fruit that is not too far away
9610 PROC LOOKFRUIT(I)
9620   LOCAL F,BD,D
9630   BD=11
9640   FOR F=0 TO NFR-1
9650     IF FS(F)=1 THEN
9660       D=DIS((5+F)*NR+HRM(I))
9670       IF D<BD THEN BD=D:HTG(I)=F:HST(I)=2
9680     END IF
9690   NEXT F
9700 END PROC
9710 ' at the centre of a room: decide where to go next. A frightened one goes where the balls are farthest away (cornered: it freezes),
9720 ' one that wants fruit goes the shortest way to it, the others walk about at random without turning round
9730 PROC CHOOSE(I)
9740   LOCAL R,D,N,BD,BC,SC,CUR,K,F,D0
9750   R=HRM(I)
9760   IF HT(I)=0 AND HST(I)=0 THEN CALL LOOKFRUIT(I)
9770   BD=-1
9780   IF HST(I)=2 THEN
9790     F=HTG(I)
9800     IF FS(F)<>1 THEN
9810       HST(I)=0
9820     ELSE
9830       IF DIS((5+F)*NR+R)=0 THEN
9840         HST(I)=3:HTM(I)=110
9850         EXIT PROC
9860       END IF
9870       BC=99
9880       FOR D=0 TO 3
9890         N=FN NBR(R,D)
9900         IF N>=0 THEN
9910           IF DIS((5+F)*NR+N)<BC THEN BC=DIS((5+F)*NR+N):BD=D
9920         END IF
9930       NEXT D
9940     END IF
9950   END IF
9960   IF HST(I)=1 THEN
9970     CUR=FN MINDIS(R)
9980     BC=-1
9990     D0=FN RN(4)
10000     FOR K=0 TO 3
10010       D=(D0+K) MOD 4
10020       N=FN NBR(R,D)
10030       IF N>=0 THEN
10040         SC=FN MINDIS(N)
10050         IF SC>BC THEN BC=SC:BD=D
10060       END IF
10070     NEXT K
10080     IF BD<0 OR BC<CUR THEN
10090       HST(I)=5:HTM(I)=50
10100       EXIT PROC
10110     END IF
10120   END IF
10130   IF BD<0 THEN
10140     D0=FN RN(4)
10150     FOR K=0 TO 3
10160       D=(D0+K) MOD 4
10170       N=FN NBR(R,D)
10180       IF N>=0 THEN
10190         IF D<>(HLD(I)+2) MOD 4 AND BD<0 THEN BD=D
10200       END IF
10210     NEXT K
10220   END IF
10230   IF BD<0 THEN
10240     FOR D=0 TO 3
10250       IF FN NBR(R,D)>=0 AND BD<0 THEN BD=D
10260     NEXT D
10270   END IF
10280   HDR(I)=BD:HLD(I)=BD:HNR(I)=FN NBR(R,BD):HSEG(I)=0
10290   CALL SETWP(I)
10300 END PROC
10310 ' one step towards the next place to walk to; the ways are made of straight parts, so it goes along one axis at a time
10320 PROC MOVEH(I)
10330   LOCAL DXX,DYY,SP
10340   SP=HSPD(I)
10350   IF HST(I)=1 THEN SP=SP*1.8
10360   DXX=HTX(I)-HX(I):DYY=HTY(I)-HY(I)
10370   HFC(I)=HFC(I)+1
10380   IF DXX<>0 THEN
10390     IF ABS(DXX)<=SP THEN
10400       HX(I)=HTX(I)
10410     ELSE
10420       HX(I)=HX(I)+SP*SGN(DXX)
10430     END IF
10440     IF DXX>0 THEN HDIR(I)=0:HFH(I)=0 ELSE HDIR(I)=2:HFH(I)=1
10450   ELSE
10460     IF DYY<>0 THEN
10470       IF ABS(DYY)<=SP THEN
10480         HY(I)=HTY(I)
10490       ELSE
10500         HY(I)=HY(I)+SP*SGN(DYY)
10510       END IF
10520       IF DYY>0 THEN HDIR(I)=1 ELSE HDIR(I)=3
10530     ELSE
10540       HSEG(I)=HSEG(I)+1
10550       IF HSEG(I)<=2 THEN
10560         CALL SETWP(I)
10570       ELSE
10580         HRM(I)=HNR(I)
10590       END IF
10600     END IF
10610   END IF
10620 END PROC
10630 ' does a worker see a ball? Only a ball in front of it (not behind) within about two rooms; a worker busy with fruit only sees a ball right in front of it
10640 PROC NOTICE(I)
10650   LOCAL P,DXX,DYY,DOT,DD,SN,R
10660   IF HT(I)=1 THEN
10670     CALL GUARDLOOK(I)
10680     EXIT PROC
10690   END IF
10700   SN=0
10710   FOR P=1 TO 4
10720     IF BST(P)=1 THEN
10730       DXX=BX(P)-(HX(I)+8):DYY=BY(P)-(HY(I)+12)
10740       DOT=DXV(HDIR(I))*DXX+DYV(HDIR(I))*DYY
10750       DD=ABS(DXX)+ABS(DYY)
10760       R=DIS(P*NR+HRM(I))
10770       IF R<=2 AND DOT>0 AND DD<260 THEN
10780         IF NOT ((HST(I)=2 OR HST(I)=3) AND DD>70) THEN SN=1
10790       END IF
10800     END IF
10810   NEXT P
10820   IF SN=1 THEN
10830     IF HST(I)<>1 THEN SFX 8
10840     HST(I)=1:HALR(I)=150
10850   END IF
10860 END PROC
10870 ' the straight line of water of foreman I: JL tiles long (up to 6, ends at a wall), the rectangle JX0..JX1, JY0..JY1 in world pixels
10880 PROC JETRECT(I)
10890   LOCAL D,GX,GY,K,T
10900   D=HDIR(I)
10910   GX=INT((HX(I)+8)/16):GY=INT((HY(I)+12)/24)
10920   JL=0
10930   FOR K=1 TO 6
10940     T=FN TL(GX+DXV(D)*K,GY+DYV(D)*K)
10950     IF T=3 OR T=4 THEN EXIT
10960     JL=K
10970   NEXT K
10980   IF D=0 THEN JX0=(GX+1)*16:JX1=(GX+1+JL)*16-1:JY0=GY*24:JY1=GY*24+23
10990   IF D=2 THEN JX0=(GX-JL)*16:JX1=GX*16-1:JY0=GY*24:JY1=GY*24+23
11000   IF D=1 THEN JX0=GX*16:JX1=GX*16+15:JY0=(GY+1)*24:JY1=(GY+1+JL)*24-1
11010   IF D=3 THEN JX0=GX*16:JX1=GX*16+15:JY0=(GY-JL)*24:JY1=GY*24-1
11020 END PROC
11030 ' a foreman who has a ball in the line of his hose starts to spray
11040 PROC GUARDLOOK(I)
11050   LOCAL P,HW
11060   IF HCD(I)>0 THEN EXIT PROC
11070   CALL JETRECT(I)
11080   IF JL=0 THEN EXIT PROC
11090   FOR P=1 TO 4
11100     IF BST(P)=1 THEN
11110       HW=8*BS(P)-3
11120       IF BX(P)+HW>=JX0 AND BX(P)-HW<=JX1 AND BY(P)+HW>=JY0 AND BY(P)-HW<=JY1 THEN
11130         HST(I)=6:HTM(I)=75:HCD(I)=280
11140         SFX 8
11150       END IF
11160     END IF
11170   NEXT P
11180 END PROC
11190 PROC ATEFRUIT(I)
11200   LOCAL F
11210   F=HTG(I)
11220   IF FS(F)=1 THEN
11230     FS(F)=2:FT(F)=240+FN RN(240)
11240     SFX 6
11250   END IF
11260   HST(I)=0
11270 END PROC
11280 PROC UPDATEH(I)
11290   LOCAL S
11300   S=HST(I)
11310   IF S=9 THEN EXIT PROC
11320   IF S=4 THEN
11330     HTM(I)=HTM(I)-1
11340     IF HTM(I)<=0 THEN HST(I)=9
11350     EXIT PROC
11360   END IF
11370   IF HCD(I)>0 THEN HCD(I)=HCD(I)-1
11380   IF S=3 THEN
11390     HTM(I)=HTM(I)-1
11400     IF (FRM+I) MOD 4=0 THEN CALL NOTICE(I)
11410     IF HST(I)=3 AND HTM(I)<=0 THEN CALL ATEFRUIT(I)
11420     EXIT PROC
11430   END IF
11440   IF S=5 THEN
11450     HTM(I)=HTM(I)-1
11460     IF HTM(I)<=0 THEN
11470       HST(I)=1:HALR(I)=60
11480       CALL CHOOSE(I)
11490     END IF
11500     EXIT PROC
11510   END IF
11520   IF S=6 THEN
11530     HTM(I)=HTM(I)-1
11540     IF HTM(I)<=0 THEN HST(I)=0
11550     EXIT PROC
11560   END IF
11570   IF S=1 THEN
11580     HALR(I)=HALR(I)-1
11590     IF HALR(I)<=0 THEN HST(I)=0
11600   END IF
11610   IF (FRM+I) MOD 4=0 THEN CALL NOTICE(I)
11620   IF HST(I)=6 THEN EXIT PROC
11630   IF HSEG(I)>=3 THEN
11640     CALL CHOOSE(I)
11650   ELSE
11660     CALL MOVEH(I)
11670   END IF
11680 END PROC
11690 ' ====================================================================== touching, rusting, the hoses
11700 PROC RUST(I,P)
11710   LOCAL PTS
11720   HST(I)=4:HTM(I)=60
11730   CHN=MIN(CHN+1,5):CHT=240
11740   IF HT(I)=0 THEN
11750     PTS=100*CHN:HLEFT=HLEFT-1
11760   ELSE
11770     PTS=500*CHN
11780   END IF
11790   BSC(P)=BSC(P)+PTS
11800   IF BIC(P)<5 THEN BIC(P)=BIC(P)+1
11810   BDRY(P)=0
11820   SFX 3
11830   HF=1
11840 END PROC
11850 PROC CHECKS
11860   LOCAL P,I,HW,DXX,DYY,DOT,S
11870   FOR I=0 TO NE-1
11880     S=HST(I)
11890     IF S<>4 AND S<>9 THEN
11900       FOR P=1 TO 4
11910         IF BST(P)=1 AND HST(I)<>4 THEN
11920           HW=8*BS(P)-3
11930           DXX=BX(P)-(HX(I)+8):DYY=BY(P)-(HY(I)+12)
11940           IF ABS(DXX)<7+HW AND ABS(DYY)<10+HW THEN
11950             DOT=DXV(HDIR(I))*DXX+DYV(HDIR(I))*DYY
11960             IF HT(I)=0 THEN
11970               IF DOT<=0 OR S=5 THEN
11980                 CALL RUST(I,P)
11990               ELSE
12000                 IF S<>1 THEN HST(I)=1:HALR(I)=150
12010               END IF
12020             ELSE
12030               IF DOT<=0 THEN
12040                 CALL RUST(I,P)
12050               ELSE
12060                 CALL HURT(P)
12070               END IF
12080             END IF
12090           END IF
12100         END IF
12110       NEXT P
12120     END IF
12130     IF HT(I)=1 AND HST(I)=6 AND HTM(I)<55 THEN
12140       CALL JETRECT(I)
12150       FOR P=1 TO 4
12160         IF BST(P)=1 THEN
12170           HW=8*BS(P)-3
12180           IF BX(P)+HW>=JX0 AND BX(P)-HW<=JX1 AND BY(P)+HW>=JY0 AND BY(P)-HW<=JY1 THEN CALL HURT(P)
12190         END IF
12200       NEXT P
12210     END IF
12220   NEXT I
12230 END PROC
12240 ' ====================================================================== fruit
12250 PROC SPAWNFRUIT(F)
12260   LOCAL R,K,OK,T
12270   T=0
12280   REPEAT
12290     R=FN RN(NR):OK=1:T=T+1
12300     IF R=SRM THEN OK=0
12310     FOR K=0 TO 3
12320       IF K<>F AND FS(K)=1 AND FRMR(K)=R THEN OK=0
12330     NEXT K
12340   UNTIL OK=1 OR T>50
12350   FRMR(F)=R:FK(F)=FN RN(4):FS(F)=1
12360   CALL BFS(R,5+F)
12370 END PROC
12380 PROC FRUITS
12390   LOCAL F
12400   FOR F=0 TO NFR-1
12410     IF FS(F)=2 THEN
12420       FT(F)=FT(F)-1
12430       IF FT(F)<=0 THEN CALL SPAWNFRUIT(F)
12440     END IF
12450   NEXT F
12460 END PROC
12470 ' now and then new screws, the chain of rusted workers runs out
12480 PROC EXTRAS
12490   IF CHT>0 THEN
12500     CHT=CHT-1
12510     IF CHT=0 THEN CHN=0:HF=1
12520   END IF
12530   IF FRM MOD 90=0 AND ITM<75 THEN CALL SPAWNITEMS(3)
12540   IF FRM MOD 1200=600 AND ARR<6 THEN CALL NEWCOMER
12550 END PROC
12560 ' ====================================================================== sprites
12570 PROC PLACEBALL(P)
12580   LOCAL S,H,SX,SY,F,IB,V
12590   H=20+(P-1)*9
12600   IF BST(P)=0 OR BST(P)=3 THEN
12610     CALL HIDE(H)
12620     EXIT PROC
12630   END IF
12640   V=1
12650   IF BST(P)=2 AND (FRM\3) MOD 2=0 THEN V=0
12660   IF BST(P)=1 AND BINV(P)>0 AND (FRM\3) MOD 2=0 THEN V=0
12670   S=BS(P)
12680   SX=INT(BX(P)-CX-8*S):SY=INT(BY(P)-CY-12*S)
12690   F=0
12700   IF BMV(P)=1 THEN F=(FRM\5) MOD 2
12710   IB=100+((P-1)*2+F)*14+BOF(S)
12720   IF V=0 OR SX<-100 OR SX>700 OR SY<-120 OR SY>400 THEN
12730     CALL HIDE(H)
12740   ELSE
12750     SPRITE H,SX,SY,IB
12760     IF SH(H)=0 THEN
12770       SPRITE H,ON
12780       SH(H)=1
12790     END IF
12800   END IF
12810 END PROC
12820 PROC PLACEENT(I)
12830   LOCAL S,F,IM,N
12840   N=60+I
12850   S=HST(I)
12860   IF S=9 THEN
12870     CALL HIDE(N)
12880     EXIT PROC
12890   END IF
12900   IF S=4 THEN
12910     IF HTM(I)<24 AND (HTM(I)\3) MOD 2=0 THEN
12920       CALL HIDE(N)
12930     ELSE
12940       CALL PLACE(N,HX(I),HY(I),16+HFH(I))
12950     END IF
12960     EXIT PROC
12970   END IF
12980   F=(HFC(I)\8) MOD 2
12990   IF S=3 THEN F=2
13000   IF S=5 THEN F=3
13010   IF S=6 THEN F=2
13020   IF S=1 AND (FRM\6) MOD 2=0 THEN F=3
13030   IM=HT(I)*8+HFH(I)*4+F
13040   CALL PLACE(N,HX(I),HY(I),IM)
13050 END PROC
13060 ' an arrow at the edge of the picture points to the nearest worker that is out of sight
13070 PROC RADAR
13080   LOCAL I,BD,D,DX2,DY2,S2,K
13090   IF FRM MOD 8=0 THEN
13100     BD=99999:RADN=0
13110     FOR I=0 TO NW-1
13120       IF HST(I)<>4 AND HST(I)<>9 THEN
13130         DX2=HX(I)+8-(CX+320):DY2=HY(I)+12-(CY+162)
13140         IF ABS(DX2)>=320 OR ABS(DY2)>=138 THEN
13150           D=ABS(DX2)+ABS(DY2)*1.5
13160           IF D<BD THEN BD=D:RWX=HX(I)+8:RWY=HY(I)+12:RADN=1
13170         END IF
13180       END IF
13190     NEXT I
13200   END IF
13210   IF RADN=0 THEN
13220     CALL HIDE(114)
13230     EXIT PROC
13240   END IF
13250   DX2=RWX-(CX+320):DY2=RWY-(CY+162)
13260   IF ABS(DX2)<320 AND ABS(DY2)<138 THEN
13270     CALL HIDE(114)
13280     EXIT PROC
13290   END IF
13300   S2=MIN(296/MAX(ABS(DX2),1),122/MAX(ABS(DY2),1))
13310   K=INT(ATAN2(DY2,DX2)/6.2832*8+8.5) MOD 8
13320   SPRITE 114,INT(320+DX2*S2)-8,INT(162+DY2*S2)-12,30+K
13330   IF SH(114)=0 THEN
13340     SPRITE 114,ON
13350     SH(114)=1
13360   END IF
13370 END PROC
13380 PROC PLACEALL
13390   LOCAL P,I,K,G,N,IM,GX,GY,D
13400   CALL RADAR
13410   FOR P=1 TO 4
13420     CALL PLACEBALL(P)
13430   NEXT P
13440   FOR I=0 TO NE-1
13450     CALL PLACEENT(I)
13460   NEXT I
13470   FOR I=NW TO NE-1
13480     G=I-NW
13490     IF HST(I)=6 THEN
13500       CALL JETRECT(I)
13510       D=HDIR(I)
13520       GX=INT((HX(I)+8)/16):GY=INT((HY(I)+12)/24)
13530       IM=18+(D MOD 2)*2+(FRM\4) MOD 2
13540       FOR K=0 TO 5
13550         N=80+G*6+K
13560         IF K<JL AND HTM(I)<68 THEN
13570           CALL PLACE(N,(GX+DXV(D)*(K+1))*16,(GY+DYV(D)*(K+1))*24,IM)
13580         ELSE
13590           CALL HIDE(N)
13600         END IF
13610       NEXT K
13620     ELSE
13630       FOR K=0 TO 5
13640         CALL HIDE(80+G*6+K)
13650       NEXT K
13660     END IF
13670   NEXT I
13680   FOR K=0 TO 3
13690     IF K<NFR AND FS(K)=1 THEN
13700       CALL PLACE(110+K,((FRMR(K) MOD RW)*4+2)*16,((FRMR(K)\RW)*4+2)*24,24+FK(K))
13710     ELSE
13720       CALL HIDE(110+K)
13730     END IF
13740   NEXT K
13750 END PROC
13760 ' ====================================================================== music
13770 PROC BAR
13780   LOCAL I,TA(8),TB(2),TC(8)
13790   FOR I=0 TO 7
13800     TA(I)=FN FREQ(MELN(BI*8+I))
13810     TC(I)=PERC(I)*120
13820   NEXT I
13830   TB(0)=FN FREQ(BASSN(BI*2)):TB(1)=FN FREQ(BASSN(BI*2+1))
13840   CALL PLAYSEQ(TA(),8,3,0.5,0.15)
13850   CALL PLAYSEQ(TB(),2,4,2,0.36)
13860   CALL PLAYSEQ(TC(),8,5,0.5,0.1)
13870   BI=(BI+1) MOD 4
13880 END PROC
13890 PROC MUSICON
13900   LOCAL TP
13910   EVERY OFF 0
13920   IF MUS=0 THEN EXIT PROC
13930   TP=104
13940   TEMPO TP
13950   WAVE 3,2:WAVE 4,0:WAVE 5,3
13960   BI=0
13970   CALL BAR
13980   EVERY INT(14400/TP)+2,0 GOSUB BARTIMER
13990 END PROC
14000 PROC MUSICOFF
14010   EVERY OFF 0
14020   SOUND OFF 3:SOUND OFF 4:SOUND OFF 5
14030 END PROC
14040 PROC MUSICSWITCH
14050   MUS=1-MUS
14060   IF MUS=1 THEN CALL MUSICON ELSE CALL MUSICOFF
14070 END PROC
14080 ' ====================================================================== the screens
14090 PROC CLEARALL
14100   LOCAL I
14110   EVERY OFF
14120   AFTER OFF
14130   SOUND OFF
14140   CLS
14150   GFX CLEAR
14160   FOR I=0 TO 127
14170     SPRITE I,OFF
14180     SH(I)=0
14190   NEXT I
14200 END PROC
14210 ' a new labyrinth with the screws, nuts and bolts in it
14220 PROC MAKEWORLD
14230   OCOL=-99
14240   SRM=(RH\2)*RW+RW\2
14250   CALL GENMAZE
14260   CALL BUILDWORLD
14270   ITM=0
14280   CALL SPAWNITEMS(110)
14290 END PROC
14300 ' the workers and foremen, each in a room at least three rooms from the start
14310 PROC INITENT(I,R)
14320   HRM(I)=R:HNR(I)=R:HX(I)=((R MOD RW)*4+2)*16:HY(I)=((R\RW)*4+2)*24
14330   HST(I)=0:HSEG(I)=3:HDIR(I)=FN RN(4):HFH(I)=FN RN(2):HALR(I)=0:HTM(I)=0:HCD(I)=60:HLD(I)=FN RN(4):HFC(I)=FN RN(16):HTG(I)=0
14340   IF HDIR(I)=0 THEN HFH(I)=0
14350   IF HDIR(I)=2 THEN HFH(I)=1
14360   IF I<NW THEN
14370     HT(I)=0:HSPD(I)=0.8+RND(1)*0.2
14380   ELSE
14390     HT(I)=1:HSPD(I)=0.7
14400   END IF
14410 END PROC
14420 PROC SPAWNENT
14430   LOCAL I,R,T
14440   CALL BFS(SRM,0)
14450   FOR I=0 TO NE-1
14460     T=0
14470     REPEAT
14480       R=FN RN(NR):T=T+1
14490     UNTIL DIS(R)>=3 OR T>200
14500     CALL INITENT(I,R)
14510   NEXT I
14520 END PROC
14530 ' when fewer than half of the workers are left, a new one arrives (at least four rooms from every ball), at most six times in a level
14540 PROC NEWCOMER
14550   LOCAL I,R,T,F,LW
14560   F=-1:LW=0
14570   FOR I=0 TO NW-1
14580     IF HST(I)=9 AND F<0 THEN F=I
14590     IF HST(I)<>9 AND HST(I)<>4 THEN LW=LW+1
14600   NEXT I
14610   IF F<0 OR LW>=NW\2 THEN EXIT PROC
14620   T=0
14630   REPEAT
14640     R=FN RN(NR):T=T+1
14650   UNTIL FN MINDIS(R)>=4 OR T>100
14660   CALL INITENT(F,R)
14670   HLEFT=HLEFT+1:ARR=ARR+1
14680   HF=1
14690 END PROC
14700 PROC STARTLEVEL
14710   LOCAL P,I
14720   CALL CLEARALL
14730   CALL MAKEWORLD
14740   NW=MIN(8+LEVL*2,16):NG=MIN((LEVL+1)\2,4):NE=NW+NG:HLEFT=NW
14750   CALL SPAWNENT
14760   NFR=MIN(2+LEVL\2,4)
14770   FOR I=0 TO 3
14780     FS(I)=0
14790   NEXT I
14800   FOR I=0 TO NFR-1
14810     CALL SPAWNFRUIT(I)
14820   NEXT I
14830   FOR P=1 TO 4
14840     BRM(P)=-1
14850     IF BPLD(P)=1 AND BLV(P)>0 AND (GM=1 OR JOY("CONNECTED",P)) THEN
14860       CALL SPAWNBALL(P)
14870     ELSE
14880       BST(P)=0
14890       IF BPLD(P)=1 AND BLV(P)<=0 THEN BST(P)=3
14900     END IF
14910   NEXT P
14920   CHN=0:CHT=0:ARR=0:RADN=0
14930   CALL CAMSNAP((SRM MOD RW*4+2)*16-320,(SRM\RW*4+2)*24-150)
14940   CALL UPDDIS
14950   CALL CAMERA
14960   CALL PLACEALL
14970   CALL HUD
14980   CALL BIGTEXT(110,"LEVEL "+STR$(LEVL),3,5)
14990   CALL MUSICON
15000   FOR I=1 TO 70
15010     GFX VSYNC
15020   NEXT I
15030   GFX LINE 60,100,580,180,0,BF
15040 END PROC
15050 PROC TITLE
15060   LOCAL I,T,K$,DONE,N
15070   CALL CLEARALL
15080   GM=1:NK=1
15090   FOR I=1 TO 4
15100     BST(I)=0:BPLD(I)=0:BLV(I)=0
15110   NEXT I
15120   LEVL=1:NW=10:NG=2:NE=12:HLEFT=10:NFR=2
15130   CALL MAKEWORLD
15140   CALL SPAWNENT
15150   FOR I=0 TO 3
15160     FS(I)=0
15170   NEXT I
15180   CALL SPAWNFRUIT(0):CALL SPAWNFRUIT(1)
15190   CX=1:CY=1:OCOL=-99
15200   CALL SHOWCAM
15210   CALL BIGTEXT(10,"BEING A",3,4)
15220   CALL BIGTEXT(60,"RUSTBALL",4,6)
15230   CALL TXTAT(9,13,"  Sneak up on the workers from behind and cover them in rust!       ",200)
15240   CALL TXTAT(9,14,"  Screws, nuts and bolts make you bigger and faster. Beware of the  ",200)
15250   CALL TXTAT(9,15,"  foremen and their hoses, and drying out. Big balls crush walls.   ",200)
15260   CALL TXTAT(9,17,"  1   one player: WASD or the cursor keys                           ",204)
15270   CALL TXTAT(9,18,"  2   two players: WASD and the cursor keys                         ",204)
15280   CALL TXTAT(9,19,"  C   controllers: up to four players, or press A / START on a pad  ",204)
15290   CALL TXTAT(9,20,"  P pauses, M music, Q quits.      BEST "+FN PAD$(HI,6)+"                      ",202)
15300   CALL MUSICOFF
15310   QT=0:DONE=0:T=0
15319   CALL IN_WAITRELEASE
15320   REPEAT
15330     GFX VSYNC
15340     T=T+1
15350     ' the labyrinth drifts by, the workers walk about
15360     CX=FN CLAMP(INT(320+300*SIN(T/230)),1,(WW-41)*16)
15370     CY=FN CLAMP(INT(450+420*SIN(T/310)),1,(WH-13)*24)
15380     CALL SHOWCAM
15390     FRM=T
15400     FOR I=0 TO NE-1
15410       CALL UPDATEH(I)
15420     NEXT I
15430     FOR I=0 TO NE-1
15440       CALL PLACEENT(I)
15450     NEXT I
15460     FOR I=0 TO 1
15470       IF FS(I)=1 THEN CALL PLACE(110+I,((FRMR(I) MOD RW)*4+2)*16,((FRMR(I)\RW)*4+2)*24,24+FK(I))
15480     NEXT I
15490     CALL FRUITS
15500     K$=INKEY$
15510     IF K$<>"" THEN
15520       N=ASC(K$)
15530       IF N>=97 AND N<=122 THEN N=N-32
15540       IF N=67 THEN GM=2:DONE=-1
15550       IF N=49 THEN NK=1:GM=1:DONE=-1
15560       IF N=50 THEN NK=2:GM=1:DONE=-1
15570       IF N=81 THEN QT=1:DONE=-1
15580     END IF
15590     IF FN IN_ANYA(0) OR FN IN_ANYSTART(0) THEN GM=2:DONE=-1
15600     IF FN IN_ANYBACK(0) THEN QT=1:DONE=-1
15610   UNTIL DONE
15620   WHILE FN IN_ANYA(0) OR FN IN_ANYSTART(0) OR FN IN_ANYBACK(0)
15630     GFX VSYNC
15640   WEND
15650 END PROC
15660 PROC PLAYGAME
15670   LOCAL P,I,ALIVE
15680   FOR P=1 TO 4
15690     BSC(P)=0:BLV(P)=3:BST(P)=0:BPLD(P)=0
15700   NEXT P
15710   IF GM=1 THEN
15720     BPLD(1)=1
15730     IF NK=2 THEN BPLD(2)=1
15740   ELSE
15750     FOR P=1 TO 4
15760       IF JOY("CONNECTED",P) THEN BPLD(P)=1
15770     NEXT P
15780   END IF
15790   LEVL=1:OV=0:QT=0:MUS=1:HF=1:FRM=0:IN_QUIT=0
15800   CALL STARTLEVEL
15810   WHILE OV=0
15820     FRM=FRM+1
15830     CALL CTRL
15840     IF QT=1 THEN OV=1
15850     CALL READIN
15860     CALL JOINS
15870     FOR P=1 TO 4
15880       CALL UPDBALL(P)
15890     NEXT P
15900     CALL UPDDIS
15910     FOR I=0 TO NE-1
15920       CALL UPDATEH(I)
15930     NEXT I
15940     CALL CHECKS
15950     CALL FRUITS
15960     CALL EXTRAS
15970     CALL CAMERA
15980     CALL PLACEALL
15990     IF HF=1 OR FRM MOD 30=0 THEN CALL HUD
16000     ALIVE=0
16010     FOR P=1 TO 4
16020       IF BST(P)=1 OR BST(P)=2 THEN ALIVE=ALIVE+1
16030     NEXT P
16040     IF ALIVE=0 THEN OV=1
16050     IF HLEFT<=0 AND OV=0 THEN CALL LEVELWIN
16060     GFX VSYNC
16070   WEND
16080   EVERY OFF
16090   AFTER OFF
16100   SOUND OFF
16110   FOR P=1 TO 4
16120     IF BSC(P)>HI THEN HI=BSC(P)
16130   NEXT P
16140   CALL OVER
16150 END PROC
16160 PROC LEVELWIN
16170   LOCAL P,I
16180   CALL MUSICOFF
16190   SOUND OFF
16200   FOR P=1 TO 4
16210     IF BST(P)=1 THEN BSC(P)=BSC(P)+500
16220   NEXT P
16230   PLAY "T170 W1 V0.35 O5 C8 E8 G8 >C4 <G8 >C8 E8 G4","T170 W2 V0.3 O4 C8 E8 G8 >C4 <G8 >C8 E8 G4","T170 W1 V0.15 O3 C4 G4 C4 G4"
16240   CALL BIGTEXT(80,"ALL RUSTED!",3,5)
16250   CALL TXTAT(30,13,"  Bonus 500 for every ball  ",200)
16260   FOR I=1 TO 170
16270     GFX VSYNC
16280   NEXT I
16290   LEVL=LEVL+1
16300   CALL STARTLEVEL
16310 END PROC
16320 PROC OVER
16330   LOCAL I,Z,K$
16350   FOR I=0 TO 127
16360     SPRITE I,OFF
16370   NEXT I
16380   CALL BIGTEXT(90,"GAME OVER",3,6)
16390   FOR I=1 TO 4
16400     IF BPLD(I)=1 THEN CALL TXTAT(30,15+I,"Player "+STR$(I)+"   Score "+STR$(BSC(I)),207+I)
16410   NEXT I
16420   PLAY "T100 W2 V0.5 O4 C4 <B4 A4 G2","T100 W1 V0.2 O3 C4 G4 E4 C2"
16430   FOR Z=1 TO 170
16440     GFX VSYNC
16450     K$=INKEY$
16460   NEXT Z
16470 END PROC
