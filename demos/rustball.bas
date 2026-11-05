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
40 ' times as big, and the bigger you are the faster you roll, but you no longer fit through the narrow doors). If you eat nothing for a while, you dry out and shrink.
50 ' Workers who have seen fruit forget about you and walk to it to eat it: a good moment for a sneak attack. Watch out for the foremen: they carry a hose
60 ' and wash you down when they see you from the front - catch them from behind, too (500 points). Rust a whole chain of workers quickly for more points.
70 ' Keys: player 1 WASD (with one player the cursor keys work, too), player 2 the cursor keys. Controllers: players 1 to 4, left stick or D-pad;
80 ' a player appears when a pad is connected and goes away when it is removed. Everywhere: P pauses (START on a pad), Q quits (BACK on a pad), M music.
90 ' What this program shows of the language:
100 '   * a labyrinth made at random (depth first search) that lives in the RAM (POKE / PEEK, PEEK$ + TILE FILL for the visible part) and scrolls smoothly
110 '   * balls of three sizes as sprite groups (SPRITE n,SIZE s,s) whose pictures are drawn at start-up with GFX and DISK and grabbed with GFX GRAB
120 '   * a breadth first search over the rooms of the labyrinth (the distance fields that the workers use to run away from the balls or to walk to the fruit)
130 '   * up to four players on one screen with a shared camera (the players are kept inside the picture), keyboard (KEY) and controllers (JOY)
140 '   * PROC / FUNC / LOCAL, DATA / READ, EVERY (the music), SFX and PLAY, a high score file
150 ' (a FUNC has to be defined before the lines that call it, so the arrays and the functions come first, then the main program, then the procedures)
160 ' --- global variables ----------------------------------------------------------------------------------------------------------------------
170 ' The labyrinth is RW x RH rooms of 3 x 3 tiles (a tile is 16 x 24 pixels) with walls one tile thick between them: WW x WH tiles, one byte each in the RAM.
180 ' Tile codes: 1, 2 floor, 3, 4 wall, 5 bolt, 6 nut, 7 screw. DE(r), DEO(r) the door to the east of room r (width 0 = wall, offset), DS(r), DSO(r) to the south.
190 ' DIS(slot * NR + r) the distance in rooms from the room of ball p (slot p) or of fruit f (slot 4 + f), 99 = unreachable.
200 ' Balls (p = 1..4): BST 0 absent / 1 rolling / 2 destroyed / 3 out, BX,BY the centre in world pixels, BS the size 1..3, BIC items eaten towards the next size,
210 ' BDRY frames since it last ate, BSC score, BLV lives, BINV frames of protection, BDX,BDY the direction wanted.
220 ' Entities (i = 0..NE-1; the workers first, then the foremen): HT 0 worker / 1 foreman, HST 0 walks / 1 flees / 2 walks to fruit / 3 eats / 4 rusted /
230 ' 5 cornered / 6 sprays (foreman) / 9 gone, HX,HY the top left corner, HRM the room it is in, HNR the room it walks to, HSEG the part of the way (3 = at a centre).
240 DIM DE(150),DEO(150),DS(150),DSO(150),DIS(1350),QU(160),VIS(150),STK(160),CND(4)
250 DIM BST(5),BX(5),BY(5),BS(5),BIC(5),BDRY(5),BSC(5),BLV(5),BINV(5),BDX(5),BDY(5),BPLD(5),BMV(5),BDT(5),BRM(5),BOF(4)
260 DIM HT(20),HST(20),HX(20),HY(20),HRM(20),HNR(20),HDR(20),HSEG(20),HTX(20),HTY(20),HDIR(20),HFH(20),HSPD(20),HTM(20),HALR(20),HTG(20),HFC(20),HCD(20),HLD(20)
270 DIM FS(4),FK(4),FRMR(4),FT(4),SH(256),DXV(4),DYV(4),MELN(32),BASSN(8),PERC(8)
280 ' the frequency of a MIDI note number (69 = the A of 440 Hz), 0 = a rest
290 FUNC FREQ(M)
300   IF M<=0 THEN RETURN 0
310   RETURN 440*EXP(LOG(2)*(M-69)/12)
320 END FUNC
330 ' the tile code at a place of the labyrinth: the world is in the RAM, one byte per tile; outside of it is wall
340 FUNC TL(TX,TY)
350   IF TX<0 OR TX>=WW OR TY<0 OR TY>=WH THEN RETURN 3
360   RETURN PEEK(TY*WW+TX)
370 END FUNC
380 ' 1 if a ball of size S with its centre at CX2,CY2 is clear of the walls
390 FUNC BOXFREE(CX2,CY2,S)
400   LOCAL HW,X0,X1,Y0,Y1,X,Y,T
410   HW=8*S-3
420   X0=INT((CX2-HW)/16):X1=INT((CX2+HW)/16)
430   Y0=INT((CY2-HW)/24):Y1=INT((CY2+HW)/24)
440   FOR Y=Y0 TO Y1
450     FOR X=X0 TO X1
460       T=FN TL(X,Y)
470       IF T=3 OR T=4 THEN RETURN 0
480     NEXT X
490   NEXT Y
500   RETURN 1
510 END FUNC
520 ' may ball P go to NX,NY? (clear of the walls, and not out of the picture while moving outwards: VX, VY tell the direction)
530 FUNC CANGO(P,NX,NY,VX,VY)
540   LOCAL S
550   S=BS(P)
560   IF NX<CX+8*S+2 AND VX<0 THEN RETURN 0
570   IF NX>CX+638-8*S AND VX>0 THEN RETURN 0
580   IF NY<CY+24+8*S AND VY<0 THEN RETURN 0
590   IF NY>CY+298-8*S AND VY>0 THEN RETURN 0
600   RETURN FN BOXFREE(NX,NY,S)
610 END FUNC
620 ' the room next to room R in direction D (0 east, 1 south, 2 west, 3 north) if there is a door, else -1
630 FUNC NBR(R,D)
640   LOCAL RX
650   RX=R MOD RW
660   IF D=0 AND RX<RW-1 THEN
670     IF DE(R)>0 THEN RETURN R+1
680   END IF
690   IF D=1 AND R+RW<NR THEN
700     IF DS(R)>0 THEN RETURN R+RW
710   END IF
720   IF D=2 AND RX>0 THEN
730     IF DE(R-1)>0 THEN RETURN R-1
740   END IF
750   IF D=3 AND R>=RW THEN
760     IF DS(R-RW)>0 THEN RETURN R-RW
770   END IF
780   RETURN -1
790 END FUNC
800 ' the room next to room R in direction D whether there is a door or not, -1 at the edge of the labyrinth
810 FUNC GNB(R,D)
820   LOCAL RX
830   RX=R MOD RW
840   IF D=0 AND RX<RW-1 THEN RETURN R+1
850   IF D=1 AND R+RW<NR THEN RETURN R+RW
860   IF D=2 AND RX>0 THEN RETURN R-1
870   IF D=3 AND R>=RW THEN RETURN R-RW
880   RETURN -1
890 END FUNC
900 ' the first tile row (east / west doors) or column (north / south doors) of the door of room R in direction D
910 FUNC DOORT(R,D)
920   LOCAL RX,RY
930   RX=R MOD RW:RY=R\RW
940   IF D=0 THEN RETURN RY*4+1+DEO(R)
950   IF D=2 THEN RETURN RY*4+1+DEO(R-1)
960   IF D=1 THEN RETURN RX*4+1+DSO(R)
970   RETURN RX*4+1+DSO(R-RW)
980 END FUNC
990 ' the room that contains a point of the world
1000 FUNC ROOMAT(PX,PY)
1010   LOCAL RX,RY
1020   RX=INT(MAX(INT(PX/16)-1,0)/4)
1030   RY=INT(MAX(INT(PY/24)-1,0)/4)
1040   IF RX>RW-1 THEN RX=RW-1
1050   IF RY>RH-1 THEN RY=RH-1
1060   RETURN RY*RW+RX
1070 END FUNC
1080 ' the distance in rooms from room R to the nearest ball that is rolling (99 if there is none)
1090 FUNC MINDIS(R)
1100   LOCAL P,D,M
1110   M=99
1120   FOR P=1 TO 4
1130     IF BST(P)=1 THEN
1140       D=DIS(P*NR+R)
1150       IF D<M THEN M=D
1160     END IF
1170   NEXT P
1180   RETURN M
1190 END FUNC
1200 ' ====================================================================== main program
1210 CALL INIT
1220 HI=FN HS_LOAD("rustball.hi")
1230 HI0=HI
1240 QT=0
1250 REPEAT
1260   CALL TITLE
1270   IF QT=0 THEN CALL PLAYGAME
1280   IF HI>HI0 THEN
1290     ON ERROR GOTO HIERR2
1300     CALL HS_SAVE("rustball.hi",HI)
1310     LABEL HIDONE2
1320     ON ERROR OFF
1330   END IF
1340   HI0=HI
1350 UNTIL QT=1
1360 CALL TX_BYE("BEING A RUSTBALL")
1370 END
1380 ' the library hiscore reads the best score (a missing or empty file is 0, no error); a file that can't be written is ignored: the error handler is here in the main program, where RESUME <label> can continue
1390 LABEL HIERR2
1400 RESUME HIDONE2
1410 ' the EVERY handler only calls a procedure
1420 LABEL BARTIMER
1430 CALL BAR
1440 RETURN
1450 ' ====================================================================== set up (once)
1460 PROC INIT
1470   LOCAL I
1480   RANDOMIZE
1490   GFX RESET ALL
1500   RW=15:RH=10:NR=RW*RH:WW=RW*4+1:WH=RH*4+1
1510   DRYT=720
1520   DXV(0)=1:DXV(1)=0:DXV(2)=-1:DXV(3)=0
1530   DYV(0)=0:DYV(1)=1:DYV(2)=0:DYV(3)=-1
1540   BOF(1)=0:BOF(2)=1:BOF(3)=5
1550   FOR I=0 TO 7
1560     READ BASSN(I)
1570   NEXT I
1580   FOR I=0 TO 31
1590     READ MELN(I)
1600   NEXT I
1610   FOR I=0 TO 7
1620     READ PERC(I)
1630   NEXT I
1640   CALL SOUNDS
1650   CALL PALETTES
1660   CALL MAKETILES
1670   CALL MAKEIMAGES
1680   TX_PAPER=212
1690   FOR I=0 TO 3
1700     SPRITE 20+I*9,PRIORITY 5
1710   NEXT I
1720   FOR I=0 TO 19
1730     SPRITE 60+I,PRIORITY 3
1740   NEXT I
1750   FOR I=0 TO 23
1760     SPRITE 80+I,PRIORITY 6
1770   NEXT I
1780   FOR I=0 TO 3
1790     SPRITE 110+I,PRIORITY 2
1800   NEXT I
1810 END PROC
1820 ' the notes of the music: 8 bass notes (2 per bar), 32 melody notes (8 per bar), 8 percussion hits (2 per bar) as MIDI numbers
1830 DATA 33,33,29,29,31,31,28,28
1840 DATA 57,60,64,69,64,60,57,60, 53,57,60,65,60,57,53,57, 55,59,62,67,62,59,55,59, 52,56,59,64,59,56,52,56
1850 DATA 1,0,1,0,1,0,1,1
1860 PROC SOUNDS
1870   SFX 1,"W2 V0.25 ML X0,0.003,0.1,0.04 S600,1200,5"
1880   SFX 2,"W1 V0.3 ML X0,0.01,0.3,0.1 S200,900,16"
1890   SFX 3,"W3 V0.25 ML X0,0.005,0.2,0.1 S1500,300,12"
1900   SFX 4,"W3 V0.3 ML X0,0.005,0.3,0.2 S3000,500,20"
1910   SFX 5,"W1 V0.35 ML X0,0.01,0.4,0.3 S600,50,30"
1920   SFX 6,"W2 V0.12 ML H400,3 H300,3 H400,3 H300,3"
1930   SFX 7,"W2 V0.2 ML S800,200,12"
1940   SFX 8,"W1 V0.2 ML X0,0.005,0.1,0.05 S400,900,4 S900,400,4"
1950 END PROC
1960 ' ====================================================================== the colors
1970 PROC PALETTES
1980   LOCAL P,I,R,G,B
1990   ' sprites (layer 4): 1 outline, 2 skin, 3 helmet, 4 vest, 5 trousers, 6 white, 7 boots, 8 foreman's vest, 9..11 rusted statue, 12 / 13 water
2000   GFX PALETTE 4,1,RGB(24,14,10):GFX PALETTE 4,2,RGB(240,190,150):GFX PALETTE 4,3,RGB(250,210,40):GFX PALETTE 4,4,RGB(240,110,20)
2010   GFX PALETTE 4,5,RGB(50,80,160):GFX PALETTE 4,6,RGB(255,255,255):GFX PALETTE 4,7,RGB(70,45,25):GFX PALETTE 4,8,RGB(40,160,70)
2020   GFX PALETTE 4,9,RGB(120,55,22):GFX PALETTE 4,10,RGB(170,85,35):GFX PALETTE 4,11,RGB(215,125,55)
2030   GFX PALETTE 4,12,RGB(170,225,255):GFX PALETTE 4,13,RGB(70,140,230):GFX PALETTE 4,14,RGB(230,60,50):GFX PALETTE 4,15,RGB(60,170,60)
2040   ' the balls 16..35: four colors of rust, five shades each, dark to light
2050   RESTORE BALLPAL
2060   FOR I=16 TO 35
2070     READ R,G,B
2080     GFX PALETTE 4,I,RGB(R,G,B)
2090   NEXT I
2100   GFX PALETTE 4,40,RGB(250,225,60):GFX PALETTE 4,41,RGB(200,160,30):GFX PALETTE 4,42,RGB(250,150,30):GFX PALETTE 4,43,RGB(210,100,20)
2110   GFX PALETTE 4,44,RGB(130,50,160):GFX PALETTE 4,45,RGB(185,115,215):GFX PALETTE 4,46,RGB(255,150,140):GFX PALETTE 4,47,RGB(110,70,30)
2120   ' tiles (layer 1): 1..3 floor, 4..8 wall, 9..11 metal, 12 / 13 gold
2130   GFX PALETTE 1,1,RGB(46,42,50):GFX PALETTE 1,2,RGB(60,55,64):GFX PALETTE 1,3,RGB(30,28,34)
2140   GFX PALETTE 1,4,RGB(112,58,34):GFX PALETTE 1,5,RGB(158,88,50):GFX PALETTE 1,6,RGB(70,34,20):GFX PALETTE 1,7,RGB(205,150,95):GFX PALETTE 1,8,RGB(90,44,26)
2150   GFX PALETTE 1,9,RGB(215,220,230):GFX PALETTE 1,10,RGB(140,146,162):GFX PALETTE 1,11,RGB(70,72,86)
2160   GFX PALETTE 1,12,RGB(240,200,70):GFX PALETTE 1,13,RGB(160,120,30)
2170   ' graphics layer (the big titles): 1 white, 2 dark, 3 gold, 4 red
2180   GFX PALETTE 3,1,RGB(255,255,255):GFX PALETTE 3,2,RGB(20,20,40):GFX PALETTE 3,3,RGB(255,200,30):GFX PALETTE 3,4,RGB(255,90,60)
2190   ' text (layer 2): 200..207 the colors of the messages, 208..211 the players, 212 an opaque dark paper
2200   GFX PALETTE 2,200,RGB(255,255,255):GFX PALETTE 2,201,RGB(255,210,50):GFX PALETTE 2,202,RGB(120,220,255):GFX PALETTE 2,203,RGB(255,100,90)
2210   GFX PALETTE 2,204,RGB(140,255,140):GFX PALETTE 2,205,RGB(180,190,230):GFX PALETTE 2,206,RGB(255,150,210):GFX PALETTE 2,207,RGB(255,170,40)
2220   GFX PALETTE 2,208,RGB(240,160,80):GFX PALETTE 2,209,RGB(240,110,100):GFX PALETTE 2,210,RGB(195,215,100):GFX PALETTE 2,211,RGB(195,155,230)
2230   GFX PALETTE 2,212,RGB(14,10,16)
2240   GFX BACKDROP RGB(0,0,0)
2250 END PROC
2260 LABEL BALLPAL
2270 DATA 60,25,10, 105,45,15, 150,75,25, 195,110,40, 240,160,80
2280 DATA 60,10,15, 105,20,25, 150,35,35, 195,60,55, 240,110,100
2290 DATA 35,45,10, 65,85,20, 100,125,35, 145,165,55, 195,215,100
2300 DATA 40,25,60, 70,45,100, 105,70,145, 145,105,185, 195,155,230
2310 ' ====================================================================== the pictures
2320 ' every picture is drawn on the graphics layer (hidden meanwhile), grabbed (4 bytes of size, then 16 x 24 pixels) and defined as a sprite or a tile
2330 PROC GRABSPRITE(IM)
2340   LOCAL G$
2350   GFX GRAB 0,0,15,23,G$
2360   SPRITE DEF IM,MID$(G$,5)
2370 END PROC
2380 PROC GRABTILE(N)
2390   LOCAL G$
2400   GFX GRAB 0,0,15,23,G$
2410   TILE DEF N,MID$(G$,5)
2420 END PROC
2430 PROC MAKEIMAGES
2440   LOCAL CO,DR,F,P,S,K,J,G$,IB
2450   GFX LAYER 3,OFF
2460   GFX TARGET 3
2470   ' images 0..15 the people (costume * 8 + direction * 4 + frame), 16 / 17 the rusted statue
2480   FOR CO=0 TO 1
2490     FOR DR=0 TO 1
2500       FOR F=0 TO 3
2510         CALL DRAWMAN(CO,DR,F,0)
2520         CALL GRABSPRITE(CO*8+DR*4+F)
2530       NEXT F
2540     NEXT DR
2550   NEXT CO
2560   FOR DR=0 TO 1
2570     CALL DRAWMAN(0,DR,0,1)
2580     CALL GRABSPRITE(16+DR)
2590   NEXT DR
2600   ' 18 / 19 the water jet sideways, 20 / 21 up and down
2610   FOR F=0 TO 1
2620     CALL DRAWJET(0,F)
2630     CALL GRABSPRITE(18+F)
2640     CALL DRAWJET(1,F)
2650     CALL GRABSPRITE(20+F)
2660   NEXT F
2670   ' 24..27 the fruit: apple, banana, orange, grapes
2680   FOR K=0 TO 3
2690     CALL DRAWFRUIT(K)
2700     CALL GRABSPRITE(24+K)
2710   NEXT K
2720   ' 100.. the balls: for each player and each of two frames 14 images (1 + 4 + 9)
2730   FOR P=1 TO 4
2740     FOR F=0 TO 1
2750       FOR S=1 TO 3
2760         CALL DRAWBALL(S,P)
2770         IB=100+((P-1)*2+F)*14+BOF(S)
2780         FOR K=0 TO S-1
2790           FOR J=0 TO S-1
2800             GFX GRAB J*16,K*24,J*16+15,K*24+23,G$
2810             SPRITE DEF IB+K*S+J,MID$(G$,5)
2820           NEXT J
2830         NEXT K
2840       NEXT S
2850     NEXT F
2860   NEXT P
2870   GFX CLEAR
2880   GFX LAYER 3,ON
2890 END PROC
2900 ' a box in a 16 x 24 picture, mirrored if MRR is 1 (a person looks to the right in the drawing and to the left when mirrored); STM 1 draws a statue
2910 PROC MB(X1,Y1,X2,Y2,C)
2920   LOCAL CC
2930   CC=C
2940   IF STM=1 THEN
2950     CC=10
2960     IF C=1 OR C=7 THEN CC=9
2970     IF C=2 OR C=6 OR C=3 THEN CC=11
2980   END IF
2990   IF MRR=1 THEN
3000     GFX LINE 15-X2,Y1,15-X1,Y2,CC,BF
3010   ELSE
3020     GFX LINE X1,Y1,X2,Y2,CC,BF
3030   END IF
3040 END PROC
3050 ' a person: CO 0 worker / 1 foreman, DR 0 right / 1 left, F 0 and 1 walking, 2 eating (worker) or spraying (foreman), 3 scared; SM 1 a rusted statue
3060 PROC DRAWMAN(CO,DR,F,SM)
3070   LOCAL VEST,CAP
3080   MRR=DR:STM=SM
3090   VEST=4:CAP=3
3100   IF CO=1 THEN VEST=8:CAP=6
3110   GFX LINE 0,0,15,23,0,BF
3120   IF F=1 THEN
3130     CALL MB(4,15,6,22,5):CALL MB(10,15,12,22,5):CALL MB(3,21,6,23,7):CALL MB(10,21,13,23,7)
3140   ELSE
3150     CALL MB(6,15,8,22,5):CALL MB(9,15,11,22,5):CALL MB(6,21,9,23,7):CALL MB(9,21,12,23,7)
3160   END IF
3170   CALL MB(5,7,10,14,VEST)
3180   CALL MB(5,10,10,10,6)
3190   CALL MB(5,13,10,14,7)
3200   CALL MB(6,2,11,6,2)
3210   CALL MB(5,0,12,2,CAP):CALL MB(11,2,13,2,CAP)
3220   CALL MB(10,4,10,4,1)
3230   IF F=2 THEN
3240     IF CO=0 THEN
3250       CALL MB(6,8,9,9,VEST):CALL MB(9,5,12,8,2):CALL MB(11,3,13,5,14)
3260     ELSE
3270       CALL MB(6,8,12,9,VEST):CALL MB(13,7,15,10,13):CALL MB(15,8,15,9,12)
3280     END IF
3290   ELSE
3300     IF F=3 THEN
3310       CALL MB(2,1,3,8,VEST):CALL MB(2,0,3,1,2):CALL MB(12,1,13,8,VEST):CALL MB(12,0,13,1,2):CALL MB(9,6,10,6,1)
3320     ELSE
3330       CALL MB(6,8,8,12,VEST):CALL MB(6,12,8,13,2)
3340       IF CO=1 THEN
3350         CALL MB(8,10,12,11,13):CALL MB(13,10,14,11,12)
3360       END IF
3370     END IF
3380   END IF
3390 END PROC
3400 ' the water of the hose: DIRN 0 sideways, 1 up and down; F 0 / 1 the drops are in two places
3410 PROC DRAWJET(DIRN,F)
3420   LOCAL K,X,Y
3430   GFX LINE 0,0,15,23,0,BF
3440   FOR K=0 TO 3
3450     IF DIRN=0 THEN
3460       X=(F*4+K*5) MOD 14
3470       GFX LINE X,8,X+2,9,12,BF:GFX LINE (X+7) MOD 14,12,(X+7) MOD 14+2,13,13,BF:GFX LINE (X+3) MOD 14,15,(X+3) MOD 14+2,16,12,BF
3480     ELSE
3490       Y=(F*6+K*7) MOD 21
3500       GFX LINE 4,Y,5,Y+2,12,BF:GFX LINE 8,(Y+10) MOD 21,9,(Y+10) MOD 21+2,13,BF:GFX LINE 11,(Y+5) MOD 21,12,(Y+5) MOD 21+2,12,BF
3510     END IF
3520   NEXT K
3530 END PROC
3540 ' K 0 apple, 1 banana, 2 orange, 3 grapes
3550 PROC DRAWFRUIT(K)
3560   GFX LINE 0,0,15,23,0,BF
3570   IF K=0 THEN
3580     CALL DISK(8,14,6,14):CALL DISK(10,16,3,14):GFX LINE 4,10,6,11,46,BF:GFX LINE 8,6,8,8,47,BF:GFX LINE 9,5,12,6,15,BF
3590   END IF
3600   IF K=1 THEN
3610     GFX LINE 3,16,6,19,40,BF:GFX LINE 5,13,9,16,40,BF:GFX LINE 8,10,12,13,40,BF:GFX LINE 11,7,13,10,40,BF
3620     GFX LINE 3,19,6,19,41,BF:GFX LINE 5,16,9,16,41,BF:GFX LINE 12,13,12,13,41:GFX LINE 13,6,14,7,47,BF
3630   END IF
3640   IF K=2 THEN
3650     CALL DISK(8,14,6,42):CALL DISK(10,16,3,43):GFX LINE 5,10,6,11,40,BF:GFX LINE 8,7,9,8,15,BF
3660   END IF
3670   IF K=3 THEN
3680     CALL DISK(5,11,3,44):CALL DISK(11,11,3,44):CALL DISK(8,14,3,44):CALL DISK(5,17,3,44):CALL DISK(11,17,3,44):CALL DISK(8,20,3,44)
3690     GFX LINE 4,10,5,10,45:GFX LINE 10,10,11,10,45:GFX LINE 7,13,8,13,45:GFX LINE 8,5,8,8,47,BF:GFX LINE 9,4,12,5,15,BF
3700   END IF
3710 END PROC
3720 ' a ball of size S for player P in the box 16 S x 24 S at the top left corner of the graphics layer: a rusty ball with a cross face
3730 PROC DRAWBALL(S,P)
3740   LOCAL B0,R,CX3,CY3,K,A,RR,X,Y
3750   B0=16+(P-1)*5
3760   R=8*S-1:CX3=8*S:CY3=12*S
3770   GFX LINE 0,0,47,71,0,BF
3780   CALL DISK(CX3,CY3,R,B0)
3790   CALL DISK(CX3-INT(R/12)-1,CY3-INT(R/12)-1,INT(R*0.92),B0+1)
3800   CALL DISK(CX3-INT(R/6)-1,CY3-INT(R/6)-1,INT(R*0.76),B0+2)
3810   CALL DISK(CX3-INT(R/4)-1,CY3-INT(R/4)-1,INT(R*0.52),B0+3)
3820   CALL DISK(CX3-INT(R/3)-1,CY3-INT(R/3)-1,INT(R*0.24),B0+4)
3830   ' rust pits and flakes
3840   FOR K=1 TO 7*S
3850     A=RND(1)*6.2832:RR=RND(1)*R*0.85
3860     X=CX3+INT(COS(A)*RR):Y=CY3+INT(SIN(A)*RR)
3870     GFX LINE X,Y,X+S-1,Y,B0+(K MOD 2)*3
3880   NEXT K
3890   ' the face
3900   CALL DISK(CX3-3*S,CY3-S,2*S,6):CALL DISK(CX3+3*S,CY3-S,2*S,6)
3910   CALL DISK(CX3-3*S+S,CY3-S,S,1):CALL DISK(CX3+3*S-S,CY3-S,S,1)
3920   GFX LINE CX3-5*S,CY3-4*S,CX3-S,CY3-2*S-1,1:GFX LINE CX3+5*S,CY3-4*S,CX3+S,CY3-2*S-1,1
3930   IF S>1 THEN
3940     GFX LINE CX3-4*S,CY3+3*S,CX3+4*S,CY3+3*S,1,BF
3950     FOR K=0 TO 3
3960       GFX LINE CX3-3*S+K*2*S,CY3+3*S,CX3-3*S+K*2*S+S-1,CY3+4*S-1,6,BF
3970     NEXT K
3980   ELSE
3990     GFX LINE 4,16,11,16,1
4000     GFX PSET 6,17,6:GFX PSET 9,17,6
4010   END IF
4020 END PROC
4030 ' ====================================================================== the tiles
4040 PROC FLOORTILE(N)
4050   LOCAL K
4060   GFX LINE 0,0,15,23,N,BF
4070   GFX LINE 15,0,15,23,3:GFX LINE 0,23,15,23,3
4080   FOR K=1 TO 7
4090     GFX PSET INT(RND(1)*14),INT(RND(1)*22),1+(K MOD 3)
4100   NEXT K
4110 END PROC
4120 PROC MAKETILES
4130   LOCAL K,X,Y
4140   GFX LAYER 3,OFF
4150   GFX TARGET 3
4160   CALL FLOORTILE(1):CALL GRABTILE(1)
4170   CALL FLOORTILE(2):CALL GRABTILE(2)
4180   ' steel plates, rusty, with rivets
4190   FOR K=3 TO 4
4200     GFX LINE 0,0,15,23,4,BF
4210     GFX LINE 0,0,15,1,5,BF:GFX LINE 0,0,0,23,5:GFX LINE 0,22,15,23,6,BF:GFX LINE 15,0,15,23,6
4220     GFX LINE 2,3,3,4,7,BF:GFX LINE 12,3,13,4,7,BF:GFX LINE 2,19,3,20,7,BF:GFX LINE 12,19,13,20,7,BF
4230     IF K=4 THEN
4240       GFX LINE 1,11,14,12,6,BF:GFX LINE 1,10,14,10,5
4250     ELSE
4260       GFX LINE 3,8,12,15,8,BF:GFX LINE 3,8,12,8,5
4270     END IF
4280     FOR X=1 TO 9
4290       Y=INT(RND(1)*14)+1
4300       GFX LINE Y,INT(RND(1)*10)+4,Y,INT(RND(1)*10)+11,6
4310     NEXT X
4320     CALL GRABTILE(K)
4330   NEXT K
4340   ' 5 a bolt, 6 a nut, 7 a screw, on the floor
4350   CALL FLOORTILE(1)
4360   GFX LINE 3,7,8,16,11,BF:GFX LINE 4,8,7,14,10,BF:GFX LINE 4,8,7,9,9,BF:GFX LINE 9,9,14,14,11,BF:GFX LINE 9,10,14,13,10,BF
4370   GFX LINE 10,9,10,14,11:GFX LINE 12,9,12,14,11
4380   CALL GRABTILE(5)
4390   CALL FLOORTILE(1)
4400   CALL DISK(8,12,7,11):CALL DISK(8,12,6,10):CALL DISK(8,12,3,11):CALL DISK(8,12,2,1)
4410   GFX LINE 3,8,8,6,9:GFX LINE 4,9,5,15,9
4420   CALL GRABTILE(6)
4430   CALL FLOORTILE(1)
4440   CALL DISK(8,7,5,13):CALL DISK(7,6,4,12):GFX LINE 4,7,11,7,13:GFX LINE 7,12,9,21,13,BF:GFX LINE 7,12,7,21,12
4450   FOR K=0 TO 3
4460     GFX LINE 7,13+K*2,9,14+K*2,13
4470   NEXT K
4480   CALL GRABTILE(7)
4490   GFX CLEAR
4500   GFX LAYER 3,ON
4510 END PROC
4520 ' ====================================================================== the labyrinth
4530 PROC OPENDOOR(R,D)
4540   LOCAL Q,W,O
4550   Q=FN RN(10)
4560   W=3
4570   IF Q<3 THEN W=1
4580   IF Q>=3 AND Q<6 THEN W=2
4590   O=FN RN(4-W)
4600   IF D=0 THEN DE(R)=W:DEO(R)=O
4610   IF D=1 THEN DS(R)=W:DSO(R)=O
4620   IF D=2 THEN DE(R-1)=W:DEO(R-1)=O
4630   IF D=3 THEN DS(R-RW)=W:DSO(R-RW)=O
4640 END PROC
4650 ' the rooms are joined by a random spanning tree (depth first search), then a few more walls are opened so that there are loops
4660 PROC GENMAZE
4670   LOCAL R,D,N,SP,CNT,K
4680   FOR R=0 TO NR-1
4690     DE(R)=0:DS(R)=0:DEO(R)=0:DSO(R)=0:VIS(R)=0
4700   NEXT R
4710   STK(0)=SRM:VIS(SRM)=1:SP=1
4720   WHILE SP>0
4730     R=STK(SP-1)
4740     CNT=0
4750     FOR D=0 TO 3
4760       N=FN GNB(R,D)
4770       IF N>=0 THEN
4780         IF VIS(N)=0 THEN
4790           CND(CNT)=D:CNT=CNT+1
4800         END IF
4810       END IF
4820     NEXT D
4830     IF CNT=0 THEN
4840       SP=SP-1
4850     ELSE
4860       D=CND(FN RN(CNT))
4870       N=FN GNB(R,D)
4880       CALL OPENDOOR(R,D)
4890       VIS(N)=1:STK(SP)=N:SP=SP+1
4900     END IF
4910   WEND
4920   FOR K=1 TO NR\8
4930     R=FN RN(NR):D=FN RN(4)
4940     N=FN GNB(R,D)
4950     IF N>=0 THEN
4960       IF FN NBR(R,D)<0 THEN CALL OPENDOOR(R,D)
4970     END IF
4980   NEXT K
4990 END PROC
5000 ' the tiles of the labyrinth into the RAM
5010 PROC BUILDWORLD
5020   LOCAL X,Y,R,RX,RY,I,T
5030   FOR Y=0 TO WH-1
5040     FOR X=0 TO WW-1
5050       T=3
5060       IF (X*7+Y*13) MOD 5=0 THEN T=4
5070       POKE Y*WW+X,T,1
5080     NEXT X
5090   NEXT Y
5100   FOR R=0 TO NR-1
5110     RX=R MOD RW:RY=R\RW
5120     FOR Y=1 TO 3
5130       FOR X=1 TO 3
5140         POKE (RY*4+Y)*WW+RX*4+X,1+((RX*4+X+RY*4+Y) MOD 2),1
5150       NEXT X
5160     NEXT Y
5170     IF DE(R)>0 THEN
5180       FOR I=0 TO DE(R)-1
5190         POKE (RY*4+1+DEO(R)+I)*WW+(RX+1)*4,1,1
5200       NEXT I
5210     END IF
5220     IF DS(R)>0 THEN
5230       FOR I=0 TO DS(R)-1
5240         POKE (RY+1)*4*WW+RX*4+1+DSO(R)+I,1,1
5250       NEXT I
5260     END IF
5270   NEXT R
5280 END PROC
5290 ' N new screws, nuts and bolts on free floor tiles
5300 PROC SPAWNITEMS(N)
5310   LOCAL K,R,X,Y,A,T
5320   FOR K=1 TO N
5330     R=FN RN(NR)
5340     X=(R MOD RW)*4+1+FN RN(3):Y=(R\RW)*4+1+FN RN(3)
5350     A=Y*WW+X
5360     T=PEEK(A)
5370     IF T=1 OR T=2 THEN
5380       POKE A,5+FN RN(3),1
5390       ITM=ITM+1
5400       CALL SETTILE(X,Y)
5410     END IF
5420   NEXT K
5430 END PROC
5440 ' breadth first search: the distance in rooms from room S to every room, into DIS(B * NR + room)
5450 PROC BFS(S,B)
5460   LOCAL H,T,R,D,N
5470   FOR R=0 TO NR-1
5480     DIS(B*NR+R)=99
5490   NEXT R
5500   DIS(B*NR+S)=0
5510   QU(0)=S:H=0:T=1
5520   WHILE H<T
5530     R=QU(H):H=H+1
5540     FOR D=0 TO 3
5550       N=FN NBR(R,D)
5560       IF N>=0 THEN
5570         IF DIS(B*NR+N)=99 THEN
5580           DIS(B*NR+N)=DIS(B*NR+R)+1
5590           QU(T)=N:T=T+1
5600         END IF
5610       END IF
5620     NEXT D
5630   WEND
5640 END PROC
5650 ' ====================================================================== the camera and the map
5660 ' The tile layer is a window of 41 x 13 cells; TILE SCROLL moves it by up to 15 / 23 pixels, and what is pushed in at the left or top comes from the
5670 ' last column / row. So that the world tile in column W is at x = W*16 - CX exactly, the cell I holds the world column OCOL+1+I (I = 0..39), the last
5680 ' cell (it is off the screen at the right) holds OCOL for the strip that is pushed in at the left, and the rows work the same way (the strip at the top is
5690 ' hidden behind the two text lines of the score bar). PEEK$ cuts the 13 rows out of the RAM, TILE FILL shows them.
5700 PROC DRAWMAP
5710   LOCAL J,M$,A
5720   FOR J=0 TO 12
5730     A=(OROW+1+J)*WW+OCOL
5740     M$=M$+PEEK$(A+1,40)+PEEK$(A,1)
5750   NEXT J
5760   TILE FILL M$
5770 END PROC
5780 PROC SHOWCAM
5790   LOCAL NC,NR2
5800   NC=(CX-1)\16:NR2=(CY-1)\24
5810   IF NC<>OCOL OR NR2<>OROW THEN
5820     OCOL=NC:OROW=NR2
5830     CALL DRAWMAP
5840   END IF
5850   TILE SCROLL 15-(CX-1) MOD 16,23-(CY-1) MOD 24
5860 END PROC
5870 ' the camera follows the middle of the balls that are in the game
5880 PROC CAMERA
5890   LOCAL P,N,SX,SY,TX,TY
5900   N=0:SX=0:SY=0
5910   FOR P=1 TO 4
5920     IF BST(P)=1 OR BST(P)=2 THEN
5930       N=N+1:SX=SX+BX(P):SY=SY+BY(P)
5940     END IF
5950   NEXT P
5960   IF N=0 THEN EXIT PROC
5970   TX=FN CLAMP(INT(SX/N)-320,1,(WW-41)*16)
5980   TY=FN CLAMP(INT(SY/N)-150,1,(WH-13)*24)
5990   CX=FN CLAMP(CX+(TX-CX)\5,1,(WW-41)*16)
6000   CY=FN CLAMP(CY+(TY-CY)\5,1,(WH-13)*24)
6010   CALL SHOWCAM
6020 END PROC
6030 ' the camera jumps to a place at once
6040 PROC CAMSNAP(X,Y)
6050   CX=FN CLAMP(X,1,(WW-41)*16)
6060   CY=FN CLAMP(Y,1,(WH-13)*24)
6070   OCOL=-99
6080   CALL SHOWCAM
6090 END PROC
6100 ' a tile of the world has changed: show it if it is on the screen
6110 PROC SETTILE(TC,TR)
6120   IF TC=OCOL THEN CALL DRAWMAP
6130   IF TC>OCOL AND TC<=OCOL+40 AND TR>OROW AND TR<=OROW+13 THEN TILE SET TC-OCOL-1,TR-OROW-1,PEEK(TR*WW+TC)
6140 END PROC
6150 ' a sprite at a place of the world: it is hidden if that is not on the screen
6160 PROC PLACE(N,X,Y,IMG)
6170   LOCAL SX,SY
6180   SX=INT(X-CX):SY=INT(Y-CY)
6190   IF SX<-40 OR SX>650 OR SY<24 OR SY>330 THEN
6200     CALL HIDE(N)
6210     EXIT PROC
6220   END IF
6230   SPRITE N,SX,SY,IMG
6240   IF SH(N)=0 THEN
6250     SPRITE N,ON
6260     SH(N)=1
6270   END IF
6280 END PROC
6290 PROC HIDE(N)
6300   IF SH(N)=1 THEN
6310     SPRITE N,OFF
6320     SH(N)=0
6330   END IF
6340 END PROC
6350 ' ====================================================================== text
6360 PROC HUD
6370   LOCAL P,T$
6380   TXT FILL 0,0,79,1,32,205,212
6390   FOR P=1 TO 4
6400     IF BPLD(P)=1 THEN
6410       T$="P"+STR$(P)+" "+FN PAD$(BSC(P),6)+" L"+STR$(BLV(P))+" "+LEFT$("ooooo",BIC(P))+LEFT$(".....",5-BIC(P))
6420       CALL TXTAT((P-1)*20,0,T$,207+P)
6430     END IF
6440   NEXT P
6450   T$=" LEVEL "+STR$(LEVL)+"   HUMANS LEFT "+STR$(HLEFT)+"   CHAIN x"+STR$(CHN)+"   BEST "+FN PAD$(HI,6)+"  "
6460   CALL TXTAT(0,1,T$,205)
6470   LOCATE 79,24
6480   HF=0
6490 END PROC
6500 ' ====================================================================== keys and pads
6510 PROC CTRL
6520   IF FN IN_SERVICE() THEN CALL PAUSE
6530   IF IN_KEY=77 THEN CALL MUSICSWITCH
6540   IF IN_QUIT THEN QT=1
6550 END PROC
6560 PROC PAUSE
6570   CALL MUSICOFF
6580   CALL TXTAT(33,10,"  PAUSED  ",201)
6590   CALL IN_PAUSELOOP
6600   IF IN_QUIT THEN QT=1
6610   TXT FILL 33,10,42,10,32,0,0
6620   IF QT=0 THEN CALL MUSICON
6630 END PROC
6640 ' controller mode: a pad that is connected brings its player, a pad that is removed takes the player away (lives and score stay)
6650 PROC JOINS
6660   LOCAL P
6670   IF GM<>2 THEN EXIT PROC
6680   FOR P=1 TO 4
6690     IF JOY("CONNECTED",P) THEN
6700       IF BST(P)=0 AND BLV(P)>0 THEN
6710         BPLD(P)=1
6720         CALL SPAWNBALL(P)
6730         SFX 8
6740         HF=1
6750       END IF
6760     ELSE
6770       IF BST(P)=1 OR BST(P)=2 THEN
6780         BST(P)=0
6790         CALL HIDE(20+(P-1)*9)
6800       END IF
6810     END IF
6820   NEXT P
6830 END PROC
6840 PROC READIN
6850   LOCAL P,X,Y,M,L,R,U,D
6860   FOR P=1 TO 4
6870     BDX(P)=0:BDY(P)=0
6880   NEXT P
6890   IF GM=1 THEN
6900     L=KEY("A"):R=KEY("D"):U=KEY("W"):D=KEY("S")
6910     IF NK=1 THEN
6920       L=L OR KEY("LEFT"):R=R OR KEY("RIGHT"):U=U OR KEY("UP"):D=D OR KEY("DOWN")
6930     END IF
6940     IF R THEN BDX(1)=1
6950     IF L THEN BDX(1)=BDX(1)-1
6960     IF D THEN BDY(1)=1
6970     IF U THEN BDY(1)=BDY(1)-1
6980     IF NK=2 THEN
6990       IF KEY("RIGHT") THEN BDX(2)=1
7000       IF KEY("LEFT") THEN BDX(2)=BDX(2)-1
7010       IF KEY("DOWN") THEN BDY(2)=1
7020       IF KEY("UP") THEN BDY(2)=BDY(2)-1
7030     END IF
7040     FOR P=1 TO NK
7050       IF BDX(P)<>0 AND BDY(P)<>0 THEN BDX(P)=BDX(P)*0.72:BDY(P)=BDY(P)*0.72
7060     NEXT P
7070   ELSE
7080     FOR P=1 TO 4
7090       IF BST(P)=1 THEN
7100         X=JOY("LX",P):Y=JOY("LY",P)
7110         IF JOY("LEFT",P) THEN X=-1
7120         IF JOY("RIGHT",P) THEN X=1
7130         IF JOY("UP",P) THEN Y=-1
7140         IF JOY("DOWN",P) THEN Y=1
7150         M=SQR(X*X+Y*Y)
7160         IF M>1 THEN X=X/M:Y=Y/M
7170         BDX(P)=X:BDY(P)=Y
7180       END IF
7190     NEXT P
7200   END IF
7210 END PROC
7220 ' ====================================================================== the balls
7230 PROC SPAWNBALL(P)
7240   LOCAL SX,SY
7250   SX=(SRM MOD RW*4+2)*16+8:SY=(SRM\RW*4+2)*24+12
7260   BX(P)=SX+((P-1) MOD 2)*24-12:BY(P)=SY+((P-1)\2)*30-15
7270   BS(P)=1:BIC(P)=0:BDRY(P)=0:BINV(P)=150:BMV(P)=0:BRM(P)=-1:BST(P)=1:BPLD(P)=1
7280   SPRITE 20+(P-1)*9,SIZE 1,1
7290   HF=1
7300 END PROC
7310 PROC OUTBALL(P)
7320   BST(P)=3
7330   CALL HIDE(20+(P-1)*9)
7340   HF=1
7350 END PROC
7360 PROC UPDBALL(P)
7370   IF BST(P)=0 OR BST(P)=3 THEN EXIT PROC
7380   IF BST(P)=2 THEN
7390     BDT(P)=BDT(P)-1
7400     IF BDT(P)<=0 THEN
7410       IF BLV(P)>0 THEN CALL SPAWNBALL(P) ELSE CALL OUTBALL(P)
7420     END IF
7430     EXIT PROC
7440   END IF
7450   IF BINV(P)>0 THEN BINV(P)=BINV(P)-1
7460   CALL MOVEBALL(P)
7470   CALL EATITEMS(P)
7480   BDRY(P)=BDRY(P)+1
7490   IF BDRY(P)>=DRYT THEN CALL DRYOUT(P)
7500 END PROC
7510 PROC MOVEBALL(P)
7520   LOCAL S,SP,VX,VY,NX,NY
7530   S=BS(P)
7540   SP=1.5+0.65*(S-1)
7550   VX=BDX(P)*SP:VY=BDY(P)*SP
7560   BMV(P)=0
7570   IF VX<>0 OR VY<>0 THEN BMV(P)=1
7580   IF VX<>0 THEN
7590     NX=BX(P)+VX
7600     IF FN CANGO(P,NX,BY(P),VX,0) THEN
7610       BX(P)=NX
7620     ELSE
7630       CALL NUDGE(P,NX,BY(P),0)
7640     END IF
7650   END IF
7660   IF VY<>0 THEN
7670     NY=BY(P)+VY
7680     IF FN CANGO(P,BX(P),NY,0,VY) THEN
7690       BY(P)=NY
7700     ELSE
7710       CALL NUDGE(P,BX(P),NY,1)
7720     END IF
7730   END IF
7740 END PROC
7750 ' the ball is blocked at a corner: it slides sideways when that gets it round (AX 0: blocked going sideways, slide up or down; 1: the other way round)
7760 PROC NUDGE(P,NX,NY,AX)
7770   LOCAL K,S,UPK,DNK,STP
7780   S=BS(P):UPK=0:DNK=0:STP=1.2
7790   FOR K=2 TO 12 STEP 2
7800     IF AX=0 THEN
7810       IF UPK=0 AND FN BOXFREE(NX,NY-K,S) THEN UPK=K
7820       IF DNK=0 AND FN BOXFREE(NX,NY+K,S) THEN DNK=K
7830     ELSE
7840       IF UPK=0 AND FN BOXFREE(NX-K,NY,S) THEN UPK=K
7850       IF DNK=0 AND FN BOXFREE(NX+K,NY,S) THEN DNK=K
7860     END IF
7870   NEXT K
7880   IF UPK>0 AND (DNK=0 OR UPK<=DNK) THEN
7890     IF AX=0 THEN
7900       IF FN BOXFREE(BX(P),BY(P)-STP,S) THEN BY(P)=BY(P)-STP
7910     ELSE
7920       IF FN BOXFREE(BX(P)-STP,BY(P),S) THEN BX(P)=BX(P)-STP
7930     END IF
7940   ELSE
7950     IF DNK>0 THEN
7960       IF AX=0 THEN
7970         IF FN BOXFREE(BX(P),BY(P)+STP,S) THEN BY(P)=BY(P)+STP
7980       ELSE
7990         IF FN BOXFREE(BX(P)+STP,BY(P),S) THEN BX(P)=BX(P)+STP
8000       END IF
8010     END IF
8020   END IF
8030 END PROC
8040 ' the ball eats the screws, nuts and bolts it touches; five of them make it one size bigger, if there is room for that
8050 PROC EATITEMS(P)
8060   LOCAL X,Y,X0,X1,Y0,Y1,T,HW,S
8070   S=BS(P):HW=8*S-1
8080   X0=INT((BX(P)-HW)/16):X1=INT((BX(P)+HW)/16)
8090   Y0=INT((BY(P)-HW)/24):Y1=INT((BY(P)+HW)/24)
8100   FOR Y=Y0 TO Y1
8110     FOR X=X0 TO X1
8120       T=FN TL(X,Y)
8130       IF T>=5 AND T<=7 THEN
8140         POKE Y*WW+X,1+((X+Y) MOD 2),1
8150         CALL SETTILE(X,Y)
8160         ITM=ITM-1
8170         BDRY(P)=0
8180         IF BIC(P)<5 THEN
8190           BIC(P)=BIC(P)+1
8200           BSC(P)=BSC(P)+10
8210         ELSE
8220           BSC(P)=BSC(P)+5
8230         END IF
8240         SFX 1
8250         HF=1
8260       END IF
8270     NEXT X
8280   NEXT Y
8290   IF BIC(P)>=5 AND S<3 THEN
8300     IF FN BOXFREE(BX(P),BY(P),S+1) THEN
8310       BS(P)=S+1:BIC(P)=0
8320       SPRITE 20+(P-1)*9,SIZE S+1,S+1
8330       SFX 2
8340       HF=1
8350     END IF
8360   END IF
8370 END PROC
8380 PROC DRYOUT(P)
8390   BDRY(P)=0
8400   IF BIC(P)>0 THEN
8410     BIC(P)=BIC(P)-1
8420     HF=1
8430   ELSE
8440     IF BS(P)>1 THEN
8450       BS(P)=BS(P)-1:BIC(P)=4
8460       SPRITE 20+(P-1)*9,SIZE BS(P),BS(P)
8470       SFX 7
8480       HF=1
8490     END IF
8500   END IF
8510 END PROC
8520 ' a ball is hit by the water of a hose: one size smaller, or (as the smallest) destroyed
8530 PROC HURT(P)
8540   IF BINV(P)>0 THEN EXIT PROC
8550   SFX 4
8560   IF BS(P)>1 THEN
8570     BS(P)=BS(P)-1:BIC(P)=0
8580     SPRITE 20+(P-1)*9,SIZE BS(P),BS(P)
8590     BINV(P)=120
8600   ELSE
8610     BLV(P)=BLV(P)-1
8620     BST(P)=2:BDT(P)=100:BINV(P)=0
8630     SFX 5
8640   END IF
8650   HF=1
8660 END PROC
8670 ' the rooms of the balls: a new distance field when a ball has come into another room
8680 PROC UPDDIS
8690   LOCAL P,R
8700   FOR P=1 TO 4
8710     IF BST(P)=1 THEN
8720       R=FN ROOMAT(BX(P),BY(P))
8730       IF R<>BRM(P) THEN
8740         BRM(P)=R
8750         CALL BFS(R,P)
8760       END IF
8770     END IF
8780   NEXT P
8790 END PROC
8800 ' ====================================================================== the people
8810 ' the next place to walk to on the way from the centre of room HRM to the centre of room HNR through the door in direction HDR (three parts)
8820 PROC SETWP(I)
8830   LOCAL A,B,D,AX,AY,BX2,BY2,DR
8840   A=HRM(I):B=HNR(I):D=HDR(I)
8850   AX=(A MOD RW)*4+2:AY=(A\RW)*4+2:BX2=(B MOD RW)*4+2:BY2=(B\RW)*4+2
8860   DR=FN DOORT(A,D)
8870   IF D=0 OR D=2 THEN
8880     IF HSEG(I)=0 THEN HTX(I)=AX*16:HTY(I)=DR*24
8890     IF HSEG(I)=1 THEN HTX(I)=BX2*16:HTY(I)=DR*24
8900     IF HSEG(I)=2 THEN HTX(I)=BX2*16:HTY(I)=BY2*24
8910   ELSE
8920     IF HSEG(I)=0 THEN HTX(I)=DR*16:HTY(I)=AY*24
8930     IF HSEG(I)=1 THEN HTX(I)=DR*16:HTY(I)=BY2*24
8940     IF HSEG(I)=2 THEN HTX(I)=BX2*16:HTY(I)=BY2*24
8950   END IF
8960 END PROC
8970 ' a worker who is not afraid looks for fruit that is not too far away
8980 PROC LOOKFRUIT(I)
8990   LOCAL F,BD,D
9000   BD=11
9010   FOR F=0 TO NFR-1
9020     IF FS(F)=1 THEN
9030       D=DIS((5+F)*NR+HRM(I))
9040       IF D<BD THEN BD=D:HTG(I)=F:HST(I)=2
9050     END IF
9060   NEXT F
9070 END PROC
9080 ' at the centre of a room: decide where to go next. A frightened one goes where the balls are farthest away (cornered: it freezes),
9090 ' one that wants fruit goes the shortest way to it, the others walk about at random without turning round
9100 PROC CHOOSE(I)
9110   LOCAL R,D,N,BD,BC,SC,CUR,K,F,D0
9120   R=HRM(I)
9130   IF HT(I)=0 AND HST(I)=0 THEN CALL LOOKFRUIT(I)
9140   BD=-1
9150   IF HST(I)=2 THEN
9160     F=HTG(I)
9170     IF FS(F)<>1 THEN
9180       HST(I)=0
9190     ELSE
9200       IF DIS((5+F)*NR+R)=0 THEN
9210         HST(I)=3:HTM(I)=110
9220         EXIT PROC
9230       END IF
9240       BC=99
9250       FOR D=0 TO 3
9260         N=FN NBR(R,D)
9270         IF N>=0 THEN
9280           IF DIS((5+F)*NR+N)<BC THEN BC=DIS((5+F)*NR+N):BD=D
9290         END IF
9300       NEXT D
9310     END IF
9320   END IF
9330   IF HST(I)=1 THEN
9340     CUR=FN MINDIS(R)
9350     BC=-1
9360     D0=FN RN(4)
9370     FOR K=0 TO 3
9380       D=(D0+K) MOD 4
9390       N=FN NBR(R,D)
9400       IF N>=0 THEN
9410         SC=FN MINDIS(N)
9420         IF SC>BC THEN BC=SC:BD=D
9430       END IF
9440     NEXT K
9450     IF BD<0 OR BC<CUR THEN
9460       HST(I)=5:HTM(I)=50
9470       EXIT PROC
9480     END IF
9490   END IF
9500   IF BD<0 THEN
9510     D0=FN RN(4)
9520     FOR K=0 TO 3
9530       D=(D0+K) MOD 4
9540       N=FN NBR(R,D)
9550       IF N>=0 THEN
9560         IF D<>(HLD(I)+2) MOD 4 AND BD<0 THEN BD=D
9570       END IF
9580     NEXT K
9590   END IF
9600   IF BD<0 THEN
9610     FOR D=0 TO 3
9620       IF FN NBR(R,D)>=0 AND BD<0 THEN BD=D
9630     NEXT D
9640   END IF
9650   HDR(I)=BD:HLD(I)=BD:HNR(I)=FN NBR(R,BD):HSEG(I)=0
9660   CALL SETWP(I)
9670 END PROC
9680 ' one step towards the next place to walk to; the ways are made of straight parts, so it goes along one axis at a time
9690 PROC MOVEH(I)
9700   LOCAL DXX,DYY,SP
9710   SP=HSPD(I)
9720   IF HST(I)=1 THEN SP=SP*2.2
9730   DXX=HTX(I)-HX(I):DYY=HTY(I)-HY(I)
9740   HFC(I)=HFC(I)+1
9750   IF DXX<>0 THEN
9760     IF ABS(DXX)<=SP THEN
9770       HX(I)=HTX(I)
9780     ELSE
9790       HX(I)=HX(I)+SP*SGN(DXX)
9800     END IF
9810     IF DXX>0 THEN HDIR(I)=0:HFH(I)=0 ELSE HDIR(I)=2:HFH(I)=1
9820   ELSE
9830     IF DYY<>0 THEN
9840       IF ABS(DYY)<=SP THEN
9850         HY(I)=HTY(I)
9860       ELSE
9870         HY(I)=HY(I)+SP*SGN(DYY)
9880       END IF
9890       IF DYY>0 THEN HDIR(I)=1 ELSE HDIR(I)=3
9900     ELSE
9910       HSEG(I)=HSEG(I)+1
9920       IF HSEG(I)<=2 THEN
9930         CALL SETWP(I)
9940       ELSE
9950         HRM(I)=HNR(I)
9960       END IF
9970     END IF
9980   END IF
9990 END PROC
10000 ' does a worker see a ball? Only a ball in front of it (not behind) within about two rooms; a worker busy with fruit only sees a ball right in front of it
10010 PROC NOTICE(I)
10020   LOCAL P,DXX,DYY,DOT,DD,SN,R
10030   IF HT(I)=1 THEN
10040     CALL GUARDLOOK(I)
10050     EXIT PROC
10060   END IF
10070   SN=0
10080   FOR P=1 TO 4
10090     IF BST(P)=1 THEN
10100       DXX=BX(P)-(HX(I)+8):DYY=BY(P)-(HY(I)+12)
10110       DOT=DXV(HDIR(I))*DXX+DYV(HDIR(I))*DYY
10120       DD=ABS(DXX)+ABS(DYY)
10130       R=DIS(P*NR+HRM(I))
10140       IF R<=2 AND DOT>0 AND DD<260 THEN
10150         IF NOT ((HST(I)=2 OR HST(I)=3) AND DD>70) THEN SN=1
10160       END IF
10170     END IF
10180   NEXT P
10190   IF SN=1 THEN
10200     IF HST(I)<>1 THEN SFX 8
10210     HST(I)=1:HALR(I)=150
10220   END IF
10230 END PROC
10240 ' the straight line of water of foreman I: JL tiles long (up to 6, ends at a wall), the rectangle JX0..JX1, JY0..JY1 in world pixels
10250 PROC JETRECT(I)
10260   LOCAL D,GX,GY,K,T
10270   D=HDIR(I)
10280   GX=INT((HX(I)+8)/16):GY=INT((HY(I)+12)/24)
10290   JL=0
10300   FOR K=1 TO 6
10310     T=FN TL(GX+DXV(D)*K,GY+DYV(D)*K)
10320     IF T=3 OR T=4 THEN EXIT
10330     JL=K
10340   NEXT K
10350   IF D=0 THEN JX0=(GX+1)*16:JX1=(GX+1+JL)*16-1:JY0=GY*24:JY1=GY*24+23
10360   IF D=2 THEN JX0=(GX-JL)*16:JX1=GX*16-1:JY0=GY*24:JY1=GY*24+23
10370   IF D=1 THEN JX0=GX*16:JX1=GX*16+15:JY0=(GY+1)*24:JY1=(GY+1+JL)*24-1
10380   IF D=3 THEN JX0=GX*16:JX1=GX*16+15:JY0=(GY-JL)*24:JY1=GY*24-1
10390 END PROC
10400 ' a foreman who has a ball in the line of his hose starts to spray
10410 PROC GUARDLOOK(I)
10420   LOCAL P,HW
10430   IF HCD(I)>0 THEN EXIT PROC
10440   CALL JETRECT(I)
10450   IF JL=0 THEN EXIT PROC
10460   FOR P=1 TO 4
10470     IF BST(P)=1 THEN
10480       HW=8*BS(P)-3
10490       IF BX(P)+HW>=JX0 AND BX(P)-HW<=JX1 AND BY(P)+HW>=JY0 AND BY(P)-HW<=JY1 THEN
10500         HST(I)=6:HTM(I)=75:HCD(I)=280
10510         SFX 8
10520       END IF
10530     END IF
10540   NEXT P
10550 END PROC
10560 PROC ATEFRUIT(I)
10570   LOCAL F
10580   F=HTG(I)
10590   IF FS(F)=1 THEN
10600     FS(F)=2:FT(F)=240+FN RN(240)
10610     SFX 6
10620   END IF
10630   HST(I)=0
10640 END PROC
10650 PROC UPDATEH(I)
10660   LOCAL S
10670   S=HST(I)
10680   IF S=9 THEN EXIT PROC
10690   IF S=4 THEN
10700     HTM(I)=HTM(I)-1
10710     IF HTM(I)<=0 THEN HST(I)=9
10720     EXIT PROC
10730   END IF
10740   IF HCD(I)>0 THEN HCD(I)=HCD(I)-1
10750   IF S=3 THEN
10760     HTM(I)=HTM(I)-1
10770     IF (FRM+I) MOD 4=0 THEN CALL NOTICE(I)
10780     IF HST(I)=3 AND HTM(I)<=0 THEN CALL ATEFRUIT(I)
10790     EXIT PROC
10800   END IF
10810   IF S=5 THEN
10820     HTM(I)=HTM(I)-1
10830     IF HTM(I)<=0 THEN
10840       HST(I)=1:HALR(I)=60
10850       CALL CHOOSE(I)
10860     END IF
10870     EXIT PROC
10880   END IF
10890   IF S=6 THEN
10900     HTM(I)=HTM(I)-1
10910     IF HTM(I)<=0 THEN HST(I)=0
10920     EXIT PROC
10930   END IF
10940   IF S=1 THEN
10950     HALR(I)=HALR(I)-1
10960     IF HALR(I)<=0 THEN HST(I)=0
10970   END IF
10980   IF (FRM+I) MOD 4=0 THEN CALL NOTICE(I)
10990   IF HST(I)=6 THEN EXIT PROC
11000   IF HSEG(I)>=3 THEN
11010     CALL CHOOSE(I)
11020   ELSE
11030     CALL MOVEH(I)
11040   END IF
11050 END PROC
11060 ' ====================================================================== touching, rusting, the hoses
11070 PROC RUST(I,P)
11080   LOCAL PTS
11090   HST(I)=4:HTM(I)=60
11100   CHN=MIN(CHN+1,5):CHT=240
11110   IF HT(I)=0 THEN
11120     PTS=100*CHN:HLEFT=HLEFT-1
11130   ELSE
11140     PTS=500*CHN
11150   END IF
11160   BSC(P)=BSC(P)+PTS
11170   IF BIC(P)<5 THEN BIC(P)=BIC(P)+1
11180   BDRY(P)=0
11190   SFX 3
11200   HF=1
11210 END PROC
11220 PROC CHECKS
11230   LOCAL P,I,HW,DXX,DYY,DOT,S
11240   FOR I=0 TO NE-1
11250     S=HST(I)
11260     IF S<>4 AND S<>9 THEN
11270       FOR P=1 TO 4
11280         IF BST(P)=1 AND HST(I)<>4 THEN
11290           HW=8*BS(P)-3
11300           DXX=BX(P)-(HX(I)+8):DYY=BY(P)-(HY(I)+12)
11310           IF ABS(DXX)<7+HW AND ABS(DYY)<10+HW THEN
11320             DOT=DXV(HDIR(I))*DXX+DYV(HDIR(I))*DYY
11330             IF HT(I)=0 THEN
11340               IF DOT<=0 OR S=5 THEN
11350                 CALL RUST(I,P)
11360               ELSE
11370                 IF S<>1 THEN HST(I)=1:HALR(I)=150
11380               END IF
11390             ELSE
11400               IF DOT<=0 THEN
11410                 CALL RUST(I,P)
11420               ELSE
11430                 CALL HURT(P)
11440               END IF
11450             END IF
11460           END IF
11470         END IF
11480       NEXT P
11490     END IF
11500     IF HT(I)=1 AND HST(I)=6 AND HTM(I)<55 THEN
11510       CALL JETRECT(I)
11520       FOR P=1 TO 4
11530         IF BST(P)=1 THEN
11540           HW=8*BS(P)-3
11550           IF BX(P)+HW>=JX0 AND BX(P)-HW<=JX1 AND BY(P)+HW>=JY0 AND BY(P)-HW<=JY1 THEN CALL HURT(P)
11560         END IF
11570       NEXT P
11580     END IF
11590   NEXT I
11600 END PROC
11610 ' ====================================================================== fruit
11620 PROC SPAWNFRUIT(F)
11630   LOCAL R,K,OK,T
11640   T=0
11650   REPEAT
11660     R=FN RN(NR):OK=1:T=T+1
11670     IF R=SRM THEN OK=0
11680     FOR K=0 TO 3
11690       IF K<>F AND FS(K)=1 AND FRMR(K)=R THEN OK=0
11700     NEXT K
11710   UNTIL OK=1 OR T>50
11720   FRMR(F)=R:FK(F)=FN RN(4):FS(F)=1
11730   CALL BFS(R,5+F)
11740 END PROC
11750 PROC FRUITS
11760   LOCAL F
11770   FOR F=0 TO NFR-1
11780     IF FS(F)=2 THEN
11790       FT(F)=FT(F)-1
11800       IF FT(F)<=0 THEN CALL SPAWNFRUIT(F)
11810     END IF
11820   NEXT F
11830 END PROC
11840 ' now and then new screws, the chain of rusted workers runs out
11850 PROC EXTRAS
11860   IF CHT>0 THEN
11870     CHT=CHT-1
11880     IF CHT=0 THEN CHN=0:HF=1
11890   END IF
11900   IF FRM MOD 90=0 AND ITM<75 THEN CALL SPAWNITEMS(3)
11910 END PROC
11920 ' ====================================================================== sprites
11930 PROC PLACEBALL(P)
11940   LOCAL S,H,SX,SY,F,IB,V
11950   H=20+(P-1)*9
11960   IF BST(P)=0 OR BST(P)=3 THEN
11970     CALL HIDE(H)
11980     EXIT PROC
11990   END IF
12000   V=1
12010   IF BST(P)=2 AND (FRM\3) MOD 2=0 THEN V=0
12020   IF BST(P)=1 AND BINV(P)>0 AND (FRM\3) MOD 2=0 THEN V=0
12030   S=BS(P)
12040   SX=INT(BX(P)-CX-8*S):SY=INT(BY(P)-CY-12*S)
12050   F=0
12060   IF BMV(P)=1 THEN F=(FRM\5) MOD 2
12070   IB=100+((P-1)*2+F)*14+BOF(S)
12080   IF V=0 OR SX<-100 OR SX>700 OR SY<-120 OR SY>400 THEN
12090     CALL HIDE(H)
12100   ELSE
12110     SPRITE H,SX,SY,IB
12120     IF SH(H)=0 THEN
12130       SPRITE H,ON
12140       SH(H)=1
12150     END IF
12160   END IF
12170 END PROC
12180 PROC PLACEENT(I)
12190   LOCAL S,F,IM,N
12200   N=60+I
12210   S=HST(I)
12220   IF S=9 THEN
12230     CALL HIDE(N)
12240     EXIT PROC
12250   END IF
12260   IF S=4 THEN
12270     IF HTM(I)<24 AND (HTM(I)\3) MOD 2=0 THEN
12280       CALL HIDE(N)
12290     ELSE
12300       CALL PLACE(N,HX(I),HY(I),16+HFH(I))
12310     END IF
12320     EXIT PROC
12330   END IF
12340   F=(HFC(I)\8) MOD 2
12350   IF S=3 THEN F=2
12360   IF S=5 THEN F=3
12370   IF S=6 THEN F=2
12380   IF S=1 AND (FRM\6) MOD 2=0 THEN F=3
12390   IM=HT(I)*8+HFH(I)*4+F
12400   CALL PLACE(N,HX(I),HY(I),IM)
12410 END PROC
12420 PROC PLACEALL
12430   LOCAL P,I,K,G,N,IM,GX,GY,D
12440   FOR P=1 TO 4
12450     CALL PLACEBALL(P)
12460   NEXT P
12470   FOR I=0 TO NE-1
12480     CALL PLACEENT(I)
12490   NEXT I
12500   FOR I=NW TO NE-1
12510     G=I-NW
12520     IF HST(I)=6 THEN
12530       CALL JETRECT(I)
12540       D=HDIR(I)
12550       GX=INT((HX(I)+8)/16):GY=INT((HY(I)+12)/24)
12560       IM=18+(D MOD 2)*2+(FRM\4) MOD 2
12570       FOR K=0 TO 5
12580         N=80+G*6+K
12590         IF K<JL AND HTM(I)<68 THEN
12600           CALL PLACE(N,(GX+DXV(D)*(K+1))*16,(GY+DYV(D)*(K+1))*24,IM)
12610         ELSE
12620           CALL HIDE(N)
12630         END IF
12640       NEXT K
12650     ELSE
12660       FOR K=0 TO 5
12670         CALL HIDE(80+G*6+K)
12680       NEXT K
12690     END IF
12700   NEXT I
12710   FOR K=0 TO 3
12720     IF K<NFR AND FS(K)=1 THEN
12730       CALL PLACE(110+K,((FRMR(K) MOD RW)*4+2)*16,((FRMR(K)\RW)*4+2)*24,24+FK(K))
12740     ELSE
12750       CALL HIDE(110+K)
12760     END IF
12770   NEXT K
12780 END PROC
12790 ' ====================================================================== music
12800 PROC BAR
12810   LOCAL I,TA(8),TB(2),TC(8)
12820   FOR I=0 TO 7
12830     TA(I)=FN FREQ(MELN(BI*8+I))
12840     TC(I)=PERC(I)*120
12850   NEXT I
12860   TB(0)=FN FREQ(BASSN(BI*2)):TB(1)=FN FREQ(BASSN(BI*2+1))
12870   CALL PLAYSEQ(TA(),8,3,0.5,0.07)
12880   CALL PLAYSEQ(TB(),2,4,2,0.18)
12890   CALL PLAYSEQ(TC(),8,5,0.5,0.03)
12900   BI=(BI+1) MOD 4
12910 END PROC
12920 PROC MUSICON
12930   LOCAL TP
12940   EVERY OFF 0
12950   IF MUS=0 THEN EXIT PROC
12960   TP=104
12970   TEMPO TP
12980   WAVE 3,2:WAVE 4,0:WAVE 5,3
12990   BI=0
13000   CALL BAR
13010   EVERY INT(14400/TP)+2,0 GOSUB BARTIMER
13020 END PROC
13030 PROC MUSICOFF
13040   EVERY OFF 0
13050   SOUND OFF 3:SOUND OFF 4:SOUND OFF 5
13060 END PROC
13070 PROC MUSICSWITCH
13080   MUS=1-MUS
13090   IF MUS=1 THEN CALL MUSICON ELSE CALL MUSICOFF
13100 END PROC
13110 ' ====================================================================== the screens
13120 PROC CLEARALL
13130   LOCAL I
13140   EVERY OFF
13150   AFTER OFF
13160   SOUND OFF
13170   CLS
13180   GFX CLEAR
13190   FOR I=0 TO 127
13200     SPRITE I,OFF
13210     SH(I)=0
13220   NEXT I
13230 END PROC
13240 ' a new labyrinth with the screws, nuts and bolts in it
13250 PROC MAKEWORLD
13260   OCOL=-99
13270   SRM=(RH\2)*RW+RW\2
13280   CALL GENMAZE
13290   CALL BUILDWORLD
13300   ITM=0
13310   CALL SPAWNITEMS(110)
13320 END PROC
13330 ' the workers and foremen, each in a room at least three rooms from the start
13340 PROC SPAWNENT
13350   LOCAL I,R,T
13360   CALL BFS(SRM,0)
13370   FOR I=0 TO NE-1
13380     T=0
13390     REPEAT
13400       R=FN RN(NR):T=T+1
13410     UNTIL DIS(R)>=3 OR T>200
13420     HRM(I)=R:HNR(I)=R:HX(I)=((R MOD RW)*4+2)*16:HY(I)=((R\RW)*4+2)*24
13430     HST(I)=0:HSEG(I)=3:HDIR(I)=FN RN(4):HFH(I)=FN RN(2):HALR(I)=0:HTM(I)=0:HCD(I)=60:HLD(I)=FN RN(4):HFC(I)=FN RN(16):HTG(I)=0
13440     IF HDIR(I)=0 THEN HFH(I)=0
13450     IF HDIR(I)=2 THEN HFH(I)=1
13460     IF I<NW THEN
13470       HT(I)=0:HSPD(I)=0.8+RND(1)*0.2
13480     ELSE
13490       HT(I)=1:HSPD(I)=0.7
13500     END IF
13510   NEXT I
13520 END PROC
13530 PROC STARTLEVEL
13540   LOCAL P,I
13550   CALL CLEARALL
13560   CALL MAKEWORLD
13570   NW=MIN(6+LEVL*2,14):NG=MIN((LEVL+1)\2,4):NE=NW+NG:HLEFT=NW
13580   CALL SPAWNENT
13590   NFR=MIN(2+LEVL\2,4)
13600   FOR I=0 TO 3
13610     FS(I)=0
13620   NEXT I
13630   FOR I=0 TO NFR-1
13640     CALL SPAWNFRUIT(I)
13650   NEXT I
13660   FOR P=1 TO 4
13670     BRM(P)=-1
13680     IF BPLD(P)=1 AND BLV(P)>0 AND (GM=1 OR JOY("CONNECTED",P)) THEN
13690       CALL SPAWNBALL(P)
13700     ELSE
13710       BST(P)=0
13720       IF BPLD(P)=1 AND BLV(P)<=0 THEN BST(P)=3
13730     END IF
13740   NEXT P
13750   CHN=0:CHT=0
13760   CALL CAMSNAP((SRM MOD RW*4+2)*16-320,(SRM\RW*4+2)*24-150)
13770   CALL UPDDIS
13780   CALL CAMERA
13790   CALL PLACEALL
13800   CALL HUD
13810   CALL BIGTEXT(110,"LEVEL "+STR$(LEVL),3,5)
13820   CALL MUSICON
13830   FOR I=1 TO 70
13840     GFX VSYNC
13850   NEXT I
13860   GFX LINE 60,100,580,180,0,BF
13870 END PROC
13880 PROC TITLE
13890   LOCAL I,T,K$,DONE,N
13900   CALL CLEARALL
13910   GM=1:NK=1
13920   FOR I=1 TO 4
13930     BST(I)=0:BPLD(I)=0:BLV(I)=0
13940   NEXT I
13950   LEVL=1:NW=10:NG=2:NE=12:HLEFT=10:NFR=2
13960   CALL MAKEWORLD
13970   CALL SPAWNENT
13980   FOR I=0 TO 3
13990     FS(I)=0
14000   NEXT I
14010   CALL SPAWNFRUIT(0):CALL SPAWNFRUIT(1)
14020   CX=1:CY=1:OCOL=-99
14030   CALL SHOWCAM
14040   CALL BIGTEXT(10,"BEING A",3,4)
14050   CALL BIGTEXT(60,"RUSTBALL",4,6)
14060   CALL TXTAT(9,13,"  Sneak up on the workers from behind and cover them in rust!       ",200)
14070   CALL TXTAT(9,14,"  Screws, nuts and bolts make you bigger and faster. Beware of the  ",200)
14080   CALL TXTAT(9,15,"  foremen with their hoses, and of drying out.                      ",200)
14090   CALL TXTAT(9,17,"  1   one player: WASD or the cursor keys                           ",204)
14100   CALL TXTAT(9,18,"  2   two players: WASD and the cursor keys                         ",204)
14110   CALL TXTAT(9,19,"  C   controllers: up to four players, or press A / START on a pad  ",204)
14120   CALL TXTAT(9,20,"  P pauses, M music, Q quits.      BEST "+FN PAD$(HI,6)+"                      ",202)
14130   CALL MUSICOFF
14140   QT=0:DONE=0:T=0
14150   REPEAT
14160     GFX VSYNC
14170     T=T+1
14180     ' the labyrinth drifts by, the workers walk about
14190     CX=FN CLAMP(INT(320+300*SIN(T/230)),1,(WW-41)*16)
14200     CY=FN CLAMP(INT(450+420*SIN(T/310)),1,(WH-13)*24)
14210     CALL SHOWCAM
14220     FRM=T
14230     FOR I=0 TO NE-1
14240       CALL UPDATEH(I)
14250     NEXT I
14260     FOR I=0 TO NE-1
14270       CALL PLACEENT(I)
14280     NEXT I
14290     FOR I=0 TO 1
14300       IF FS(I)=1 THEN CALL PLACE(110+I,((FRMR(I) MOD RW)*4+2)*16,((FRMR(I)\RW)*4+2)*24,24+FK(I))
14310     NEXT I
14320     CALL FRUITS
14330     K$=INKEY$
14340     IF K$<>"" THEN
14350       N=ASC(K$)
14360       IF N>=97 AND N<=122 THEN N=N-32
14370       IF N=67 THEN GM=2:DONE=-1
14380       IF N=49 THEN NK=1:GM=1:DONE=-1
14390       IF N=50 THEN NK=2:GM=1:DONE=-1
14400       IF N=81 THEN QT=1:DONE=-1
14410     END IF
14420     IF FN IN_ANYA(0) OR FN IN_ANYSTART(0) THEN GM=2:DONE=-1
14430     IF FN IN_ANYBACK(0) THEN QT=1:DONE=-1
14440   UNTIL DONE
14450   WHILE FN IN_ANYA(0) OR FN IN_ANYSTART(0) OR FN IN_ANYBACK(0)
14460     GFX VSYNC
14470   WEND
14480 END PROC
14490 PROC PLAYGAME
14500   LOCAL P,I,ALIVE
14510   FOR P=1 TO 4
14520     BSC(P)=0:BLV(P)=3:BST(P)=0:BPLD(P)=0
14530   NEXT P
14540   IF GM=1 THEN
14550     BPLD(1)=1
14560     IF NK=2 THEN BPLD(2)=1
14570   ELSE
14580     FOR P=1 TO 4
14590       IF JOY("CONNECTED",P) THEN BPLD(P)=1
14600     NEXT P
14610   END IF
14620   LEVL=1:OV=0:QT=0:MUS=1:HF=1:FRM=0:IN_QUIT=0
14630   CALL STARTLEVEL
14640   WHILE OV=0
14650     FRM=FRM+1
14660     CALL CTRL
14670     IF QT=1 THEN OV=1
14680     CALL READIN
14690     CALL JOINS
14700     FOR P=1 TO 4
14710       CALL UPDBALL(P)
14720     NEXT P
14730     CALL UPDDIS
14740     FOR I=0 TO NE-1
14750       CALL UPDATEH(I)
14760     NEXT I
14770     CALL CHECKS
14780     CALL FRUITS
14790     CALL EXTRAS
14800     CALL CAMERA
14810     CALL PLACEALL
14820     IF HF=1 OR FRM MOD 30=0 THEN CALL HUD
14830     ALIVE=0
14840     FOR P=1 TO 4
14850       IF BST(P)=1 OR BST(P)=2 THEN ALIVE=ALIVE+1
14860     NEXT P
14870     IF ALIVE=0 THEN OV=1
14880     IF HLEFT<=0 AND OV=0 THEN CALL LEVELWIN
14890     GFX VSYNC
14900   WEND
14910   EVERY OFF
14920   AFTER OFF
14930   SOUND OFF
14940   FOR P=1 TO 4
14950     IF BSC(P)>HI THEN HI=BSC(P)
14960   NEXT P
14970   CALL OVER
14980 END PROC
14990 PROC LEVELWIN
15000   LOCAL P,I
15010   CALL MUSICOFF
15020   SOUND OFF
15030   FOR P=1 TO 4
15040     IF BST(P)=1 THEN BSC(P)=BSC(P)+500
15050   NEXT P
15060   PLAY "T170 W1 V0.35 O5 C8 E8 G8 >C4 <G8 >C8 E8 G4","T170 W2 V0.3 O4 C8 E8 G8 >C4 <G8 >C8 E8 G4","T170 W1 V0.15 O3 C4 G4 C4 G4"
15070   CALL BIGTEXT(80,"ALL RUSTED!",3,5)
15080   CALL TXTAT(30,13,"  Bonus 500 for every ball  ",200)
15090   FOR I=1 TO 170
15100     GFX VSYNC
15110   NEXT I
15120   LEVL=LEVL+1
15130   CALL STARTLEVEL
15140 END PROC
15150 PROC OVER
15160   LOCAL I,Z,K$
15170   IF QT=1 THEN EXIT PROC
15180   FOR I=0 TO 127
15190     SPRITE I,OFF
15200   NEXT I
15210   CALL BIGTEXT(90,"GAME OVER",3,6)
15220   FOR I=1 TO 4
15230     IF BPLD(I)=1 THEN CALL TXTAT(30,15+I,"Player "+STR$(I)+"   Score "+STR$(BSC(I)),207+I)
15240   NEXT I
15250   PLAY "T100 W2 V0.5 O4 C4 <B4 A4 G2","T100 W1 V0.2 O3 C4 G4 E4 C2"
15260   FOR Z=1 TO 170
15270     GFX VSYNC
15280     K$=INKEY$
15290   NEXT Z
15300 END PROC
