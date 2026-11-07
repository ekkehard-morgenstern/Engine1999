1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' the helpers that the demo games share are in the libraries demos/lib/util.lib, input.lib, hiscore.lib and text.lib
20 LOAD LIBRARY "util"
30 LOAD LIBRARY "input"
40 LOAD LIBRARY "hiscore"
50 LOAD LIBRARY "text"
60 ' ORCHARD DEFENSE III - the top-notch edition of the fruit games: Orchard Defense II with a day that passes, wind, swaying fruit, power-ups and two players.
70 ' Keys: one player: LEFT / RIGHT or A / D walk (only while the key is held), SPACE, UP or W throws a ball. Two players: player 1 A / D and W (or SPACE), pad 1;
80 ' player 2 LEFT / RIGHT and UP (or RETURN), pad 2. P = pause, Q = quit, M = music on / off.
90 ' Game controller (optional, JOY): the D-pad or the left stick walks, A or the right trigger throws, START pauses, BACK quits.
100 ' Hit the fruit before it reaches the bushes. Cherries are worth 50 points, strawberries 40, apples 30, oranges 20, lemons 10, coconuts 100 (they take two hits),
110 ' the bird 50 to 300, the balloon 200. Bananas give rapid fire, the balloon drops crates with a power-up: two balls at once, a big ball or an umbrella
120 ' that keeps the seeds off. Every fifth wave a giant melon comes; when it is half beaten it splits in two.
130 ' What is new compared to Orchard Defense II (orchard2.bas):
140 '   * the day passes with the waves: dawn, morning, noon, afternoon, sunset and night; the palettes of the sky, the hills, the grass and the bushes fade
150 '     from one time of day to the next (GFX PALETTE in steps), the sun moves, at night there are stars, a moon, fireflies and shooting stars
160 '   * wind: a windsock (its picture follows the wind) shows it, the seeds drift with it and the tile clouds drift with it; the clouds also move a little
170 '     when the gardener walks (parallax: sky, tile clouds, cloud sprites and the fruit move against each other)
180 '   * the fruit sways on its branches (every row has its own phase), coconuts take two hits and show a crack after the first
190 '   * a balloon (a sprite group 2 x 2) on a sprite path with events (E) and ON SPRITE: it drops a power-up crate in the middle of the sky
200 '   * power-ups with timers, a giant melon (sprite group 4 x 2) that splits in two groups of 2 x 2 when it is half beaten
210 '   * one or two players at the same time (arrays indexed by player; two keyboards layouts and two pads), ON KEY for the music switch
220 '   * sound: the rustle, footsteps on the grass, birdsong by day and crickets by night
230 '   * the pictures of the balloon, the windsock, the melons, the umbrella and the stars are drawn by the program (SPRITE DEF)
240 '
250 ' --- global variables -----------------------------------------------------------------------------------------------------------------
260 ' NPL players (1 or 2); arrays by player 0..1: PX position, PD direction, FQ throw request, LV lives, SCO score, NX score of the next extra life,
270 ' TP frames of throwing pose left, GS picture (0 stands, 1 walks, 2 throws), RAP frames of rapid fire left, PWK power-up (0 none, 1 two balls, 2 big ball,
280 ' 3 umbrella) and PWT frames left, ALV 1 = in the game; SC the team score, HI high score, WV wave, OV game over, QT quit, LANDED the fruit has landed
290 ' balls (4 slots, slot S belongs to player S\2): GA active, GX, GY place, GK kind (0 ball, 1 rapid fire, 2 big); seeds (6): SEEDON, SEEDX, SEEDY
300 ' FX,FY the formation (where it should be), FXV,FYV where it is drawn (it glides there), SWA the phase of the swaying, DX direction, SD step width,
310 ' SP steps per move, NA fruits alive, IV(row,column) 0 = gone, 1 = alive, 2 = a coconut that has not been hit yet, TK(row,column) 1 = coconut
320 ' WIND (a number from -2 to 2) and WTGT where it goes; PH time of day (0..5), PHP the one before; BOSS 1 = the melon, 2 = its halves
330 ' UA,UX,UY,UD the bird, BAL the balloon, CR,CRX,CRY,CRK the crate that falls, PU,PUX,PUY a banana, EP() explosion timers, PL() particle lives
340 ' sprites: 0-39 fruit, 40-41 gardeners, 42-45 balls, 46-51 seeds, 52 bird, 53 crate, 54 banana, 55-56 hats, 59-62 explosions, 63 windsock,
350 ' 64-127 flying bits, 130-138 cloud groups, 140-143 balloon, 150-159 lives, 160-167 melon, 170-177 halves, 180-185 stars and fireflies, 190-193 umbrellas
360 ' --------------------------------------------------------------------------------------------------------------------------------------
370 ' the arrays are made first: a FUNC that uses an array has to find it already declared (DIM n makes n elements, 0 to n-1)
380 DIM IV(6,8),TK(6,8),GA(4),GX(4),GY(4),GK(4),SEEDON(6),SEEDX(6),SEEDY(6),EP(4),PL(64),FW(6),RP(6),IMG(6),BASSF(4),CH3(12),PAT(8),SV(4)
390 DIM PX(2),PD(2),FQ(2),LV(2),SCO(2),NX(2),TP(2),GS(2),RAP(2),PWK(2),PWT(2),ALV(2),WK(2),ROWX(6),TOD(6,24),B2P(2)
400 ' (a FUNC has to be defined before the lines that call it)
410 ' the number of fruits that are still alive (FOR EACH goes over the whole array, a row after the other)
420 FUNC COUNTALIVE(G())
430   LOCAL N,V
440   FOR EACH V IN G()
450     IF V>0 THEN N=N+1
460   NEXT
470   RETURN N
480 END FUNC
490 ' MIXV(A,B,F): the value between A and B, F = 0 gives A, F = 1 gives B (the fades of the day)
500 FUNC MIXV(A,B,F)
510   RETURN INT(A+(B-A)*F+0.5)
520 END FUNC
530 ' FIMG(RW,C): the picture of the fruit in row RW, column C: a coconut shows a crack after the first hit
540 FUNC FIMG(RW,C)
550   IF TK(RW,C)=1 THEN
560     IF IV(RW,C)=2 THEN RETURN 41+FA
570     RETURN 43+FA
580   END IF
590   RETURN IMG(RW)+FA
600 END FUNC
610 ' the place of the gardener of the player P at the start of a wave
620 FUNC STARTX(P)
630   IF NPL=1 THEN RETURN 312
640   RETURN 180+P*260
650 END FUNC
660 ' ====================================================================== main program
670 CALL INIT
680 HI=FN HS_LOAD("orchard3.hi")
690 QT=0
700 ON KEY GOSUB KEYEVENT
710 ON SPRITE GOSUB SPRITEEVENT
720 REPEAT
730   CALL TITLE
740   IF QT=0 THEN CALL PLAYGAME:QT=0
750   IF SC>=HI AND SC>0 THEN
760     HI=SC
770     ON ERROR GOTO HIERR2
780     CALL HS_SAVE("orchard3.hi",HI)
790     LABEL HIDONE2
800     ON ERROR OFF
810   END IF
820 UNTIL QT=1
830 CALL TX_BYE("ORCHARD DEFENSE III")
840 END
850 ' the library hiscore reads the high score (a missing or empty file is 0, no error); a file that can't be written is ignored: the error handler is here in the main program, where RESUME <label> can continue (inside a PROC the frame would be gone)
860 LABEL HIERR2
870 RESUME HIDONE2
880 ' the AFTER, EVERY and event handlers: they only call a procedure (the procedure uses LOCAL variables, so it can interrupt anything)
890 LABEL BARTIMER
900 CALL BAR
910 RETURN
920 LABEL BIRDTIMER
930 BIRDREADY=1
940 RETURN
950 LABEL BALTIMER
960 BALREADY=1
970 RETURN
980 ' ON KEY: a key went down or was let go (KEY$("EVENT") tells which)
990 LABEL KEYEVENT
1000 CALL KEYEVENTS
1010 RETURN
1020 ' ON SPRITE: a sprite path has reached an E command (the balloon drops its crate in the middle of the sky, and tells that it has left)
1030 LABEL SPRITEEVENT
1040 CALL SPRITEEVENTS
1050 RETURN
1060 ' ====================================================================== set up (once)
1070 PROC INIT
1080   LOCAL I,K
1090   TX_PAPER=0:TX_SHADOW=11   ' the colors of the text library: no paper behind the text, a dark blue shadow
1100   RANDOMIZE
1110   GFX RESET ALL
1120   RESTORE FRUITDATA
1130   FOR I=1 TO 5
1140     READ FW(I),IMG(I),RP(I)
1150   NEXT I
1160   FOR I=0 TO 3
1170     READ BASSF(I),SV(I)
1180   NEXT I
1190   FOR I=0 TO 11
1200     READ CH3(I)
1210   NEXT I
1220   FOR I=0 TO 7
1230     READ PAT(I)
1240   NEXT I
1250   RESTORE TODDATA
1260   FOR I=0 TO 5
1270     FOR K=0 TO 22
1280       READ TOD(I,K)
1290     NEXT K
1300   NEXT I
1310   SX=36:SY=26:SHY=216:PY=265:UY=14
1320   NPL=1:MUTE=0:PH=1:PHP=1:PX(0)=312:PX(1)=312:WIND=0:WTGT=0:WCT=100:AMT=0:WSI=-1:LASTP=0:HITF=0
1330   CALL SOUNDS
1340   CALL PALETTES
1350   CALL DEFSPRITES
1360   CALL DEFART
1370   CALL BALLOONINIT
1380   CALL DEFTILES
1390   CALL BACKGROUND(1)
1400   CALL TODAPPLY(1,1,1)
1410   CALL CLOUDMAP
1420   CALL CLOUDSPRITES
1430   CALL AMBIENTINIT
1440   SPRITE SEQ 1,CHR$(10)+CHR$(11)+CHR$(12)+CHR$(11)
1450   SPRITE SEQ 2,CHR$(19)+CHR$(20)+CHR$(21)+CHR$(20)
1460   SPRITE SEQ 3,CHR$(22)+CHR$(23)
1470   SPRITE SEQ 4,CHR$(14)+CHR$(15)+CHR$(16)+CHR$(15)
1480   SPRITE SEQ 5,CHR$(24)+CHR$(25)+CHR$(26)+CHR$(27)
1490 END PROC
1500 PROC SOUNDS
1510   SFX 1,"W1 V0.28 ML X0,0.01,0.4,0.05 S1900,260,12"
1520   SFX 2,"W3 V0.55 ML X0,0.01,0.25,0.08 S1500,120,16"
1530   SFX 3,"W3 V1 ML X0,0.02,0.5,0.6 S900,30,3"
1540   SFX 4,"W1 V0.4 ML X0,0.01,0.3,0.05 H988,16 H1319,6"
1550   SFX 5,"W1 V0.4 ML H523,10 H659,10 H784,10 H1047,6"
1560   SFX 6,"W3 V0.35 ML X0,0.005,0.2,0.04 S500,90,32"
1570   SFX 7,"W2 V0.15 ML S800,420,10"
1580   SFX 8,"W1 V0.35 ML ~10,0.4 S300,1800,6"
1590   SFX 9,"W2 V0.4 ML H784,12 H988,12 H1319,6"
1600   SFX 10,"W1 V0.2 ML H1568,10 H2093,10"
1610   SFX 11,"W3 V0.3 ML X0.1,0.2,0.3,0.3 S200,1200,2"
1620   ' the rustle of the leaves when the fruit marches: two different noises one after the other
1630   SFX 12,"W3 V0.2 ML X0.005,0.02,0.3,0.06 S5000,3500,6"
1640   SFX 13,"W3 V0.2 ML X0.005,0.02,0.3,0.06 S3800,2800,6"
1650   ' footsteps in the grass, the clunk of a coconut, the boing of the umbrella, two bird chirps (17, 19), a cricket (the number after the note is the note value: 32 = 1/32 note, a chirp lasts well under half a second)
1660   SFX 14,"W3 V0.1 ML X0,0.002,0.1,0.02 S1800,500,32"
1670   SFX 15,"W1 V0.3 ML X0,0.005,0.2,0.05 S300,140,12"
1680   SFX 16,"W2 V0.3 ML H1568,16 H1175,8"
1690   SFX 17,"W0 V0.12 ML H2600,32 H3200,32 H2800,32 H3400,16"
1695   SFX 19,"W0 V0.1 ML H3000,48 H3600,48 H3000,48 H3600,48 H3300,24"
1700   SFX 18,"W0 V0.06 ML H4300,64 R64 H4300,64 R64 H4300,64 R64 H4300,64"
1710 END PROC
1720 ' ====================================================================== colors
1730 ' the sprite colors are in windows of 16 palette entries: the pictures use the digits 1..9 and A..C, and DESIGN adds the base of the window
1740 PROC LOADPAL
1750   LOCAL B,I,R,GG,BL
1760   READ B
1770   FOR I=1 TO 12
1780     READ R,GG,BL
1790     GFX PALETTE 4,B+I,RGB(R,GG,BL)
1800   NEXT I
1810 END PROC
1820 PROC PALETTES
1830   LOCAL I
1840   RESTORE PALDATA
1850   FOR I=1 TO 15
1860     CALL LOADPAL
1870   NEXT I
1880   ' the giant melon (196..202), its halves, and the shooting stars (203..206): colors that the pictures made by the program use directly
1890   GFX PALETTE 4,196,RGB(25,110,40):GFX PALETTE 4,197,RGB(70,170,60):GFX PALETTE 4,198,RGB(225,40,60):GFX PALETTE 4,199,RGB(255,110,120)
1900   GFX PALETTE 4,200,RGB(20,10,20):GFX PALETTE 4,201,RGB(250,250,235):GFX PALETTE 4,202,RGB(190,225,170)
1910   GFX PALETTE 4,203,RGB(255,255,255):GFX PALETTE 4,204,RGB(200,220,255):GFX PALETTE 4,205,RGB(120,140,220):GFX PALETTE 4,206,RGB(60,70,140)
1920   ' the tiles: clouds (white with a bluish shadow)
1930   GFX PALETTE 1,1,RGB(255,255,255):GFX PALETTE 1,2,RGB(205,220,240)
1940   ' graphics layer: the colors 1 to 6 (bushes, grass) belong to the time of day (PROC TODAPPLY); flowers (7..9), title (10..13)
1950   GFX PALETTE 3,7,RGB(250,70,100):GFX PALETTE 3,8,RGB(255,235,70):GFX PALETTE 3,9,RGB(255,255,255)
1960   GFX PALETTE 3,10,RGB(255,255,255):GFX PALETTE 3,11,RGB(20,45,100):GFX PALETTE 3,12,RGB(255,215,60):GFX PALETTE 3,13,RGB(255,255,255)
1970   ' stars on the background layer
1980   GFX PALETTE 0,70,RGB(255,255,255):GFX PALETTE 0,71,RGB(200,210,255):GFX PALETTE 0,72,RGB(255,240,200)
1990   GFX BACKDROP RGB(110,180,255)
2000 END PROC
2010 ' ====================================================================== the day
2020 ' TOD(phase,k): 0..2 top of the sky, 3..5 horizon, 6..8 far hills, 9..11 near hills, 12..14 grass, 15..17 sun / moon, 18 brightness of the bushes (percent),
2030 ' 19, 20 place of the sun, 21 night (stars, moon), 22 light text
2040 ' TODAPPLY(A,B,F): all the colors that depend on the time of day, mixed between the phase A and the phase B (F = 0 is A, F = 1 is B)
2050 PROC TODAPPLY(A,B,F)
2060   LOCAL I,K,U,TR,TG,TB,HR,HG,HB,SR,SG,SB,BR,GR,GG,GB
2070   TR=FN MIXV(TOD(A,0),TOD(B,0),F):TG=FN MIXV(TOD(A,1),TOD(B,1),F):TB=FN MIXV(TOD(A,2),TOD(B,2),F)
2080   HR=FN MIXV(TOD(A,3),TOD(B,3),F):HG=FN MIXV(TOD(A,4),TOD(B,4),F):HB=FN MIXV(TOD(A,5),TOD(B,5),F)
2090   FOR I=1 TO 24
2100     U=I/24
2110     GFX PALETTE 0,I,RGB(FN MIXV(TR,HR,U),FN MIXV(TG,HG,U),FN MIXV(TB,HB,U))
2120   NEXT I
2130   SR=FN MIXV(TOD(A,15),TOD(B,15),F):SG=FN MIXV(TOD(A,16),TOD(B,16),F):SB=FN MIXV(TOD(A,17),TOD(B,17),F)
2140   FOR K=0 TO 5
2150     GFX PALETTE 0,40+K,RGB(FN MIXV(HR,SR,(K+1)/6),FN MIXV(HG,SG,(K+1)/6),FN MIXV(HB,SB,(K+1)/6))
2160   NEXT K
2170   GFX PALETTE 0,60,RGB(FN MIXV(TOD(A,6),TOD(B,6),F),FN MIXV(TOD(A,7),TOD(B,7),F),FN MIXV(TOD(A,8),TOD(B,8),F))
2180   GFX PALETTE 0,61,RGB(FN MIXV(TOD(A,9),TOD(B,9),F),FN MIXV(TOD(A,10),TOD(B,10),F),FN MIXV(TOD(A,11),TOD(B,11),F))
2190   GR=FN MIXV(TOD(A,12),TOD(B,12),F):GG=FN MIXV(TOD(A,13),TOD(B,13),F):GB=FN MIXV(TOD(A,14),TOD(B,14),F)
2200   GFX PALETTE 3,4,RGB(GR,GG,GB)
2210   GFX PALETTE 3,5,RGB(FN CLAMP(GR+45,0,255),FN CLAMP(GG+30,0,255),FN CLAMP(GB+25,0,255))
2220   BR=FN MIXV(TOD(A,18),TOD(B,18),F)/100
2230   GFX PALETTE 3,1,RGB(INT(30*BR),INT(110*BR),INT(45*BR))
2240   GFX PALETTE 3,2,RGB(INT(60*BR),INT(165*BR),INT(65*BR))
2250   GFX PALETTE 3,3,RGB(INT(130*BR),INT(220*BR),INT(95*BR))
2260   GFX PALETTE 3,6,RGB(INT(110*BR),INT(70*BR),INT(40*BR))
2270   ' the clouds get darker and bluer when it is dark (the tile clouds: layer 1, the cloud sprites: the window 192)
2280   U=0.35+0.65*BR
2290   GFX PALETTE 1,1,RGB(INT(255*U*0.85),INT(255*U*0.9),INT(255*U))
2300   GFX PALETTE 1,2,RGB(INT(205*U*0.85),INT(220*U*0.9),INT(240*U))
2310   GFX PALETTE 4,193,RGB(INT(255*U*0.85),INT(255*U*0.9),INT(255*U))
2320   GFX PALETTE 4,194,RGB(INT(205*U*0.85),INT(220*U*0.9),INT(240*U))
2330   GFX PALETTE 4,195,RGB(INT(170*U*0.85),INT(195*U*0.9),INT(225*U))
2340   ' the text: dark letters on the day sky, light ones when it is dark
2350   K=TOD(A,22)
2360   IF F>=0.5 THEN K=TOD(B,22)
2370   IF K=1 THEN
2380     GFX PALETTE 2,200,RGB(255,255,255):GFX PALETTE 2,201,RGB(255,215,60):GFX PALETTE 2,202,RGB(110,220,255):GFX PALETTE 2,203,RGB(255,110,110)
2390     GFX PALETTE 2,204,RGB(130,255,130):GFX PALETTE 2,205,RGB(190,200,240):GFX PALETTE 2,206,RGB(255,150,210):GFX PALETTE 2,207,RGB(255,170,60)
2400   ELSE
2410     GFX PALETTE 2,200,RGB(20,30,80):GFX PALETTE 2,201,RGB(150,80,0):GFX PALETTE 2,202,RGB(0,70,150):GFX PALETTE 2,203,RGB(190,30,30)
2420     GFX PALETTE 2,204,RGB(0,110,40):GFX PALETTE 2,205,RGB(50,60,120):GFX PALETTE 2,206,RGB(170,30,110):GFX PALETTE 2,207,RGB(190,90,0)
2430   END IF
2440 END PROC
2450 ' the flash when a gardener is hit: T = 1 makes the sky pale, T = 0 puts the colors of the time of day back
2460 PROC SKYFLASH(T)
2470   LOCAL I
2480   IF T=0 THEN
2490     CALL TODAPPLY(PH,PH,1)
2500     EXIT PROC
2510   END IF
2520   FOR I=1 TO 24
2530     GFX PALETTE 0,I,RGB(FN CLAMP(120+I*5,0,255),FN CLAMP(170+I*3,0,255),255)
2540   NEXT I
2550 END PROC
2560 ' ====================================================================== background layer: sky, sun or moon, hills
2570 ' the pictures of the phase P (the colors are put in by TODAPPLY): bands, stars at night, the sun or the moon, two ridges of hills
2580 PROC BACKGROUND(P)
2590   LOCAL I,X,H,SXX,SYY,RD
2600   GFX TARGET 0
2610   GFX CLEAR
2620   FOR I=0 TO 23
2630     GFX LINE 0,I*13,639,I*13+12,I+1,BF
2640   NEXT I
2650   IF TOD(P,21)=1 THEN
2660     FOR I=1 TO 80
2670       GFX PSET FN RN(640),FN RN(200),70+FN RN(3)
2680     NEXT I
2690   END IF
2700   SXX=TOD(P,19):SYY=TOD(P,20)
2710   RD=40
2720   IF TOD(P,21)=1 THEN RD=30
2730   FOR I=0 TO 5
2740     CALL DISK(SXX,SYY,RD-I*(RD\8),40+I)
2750   NEXT I
2760   FOR X=0 TO 638 STEP 2
2770     H=INT(34+16*SIN(X/41)+8*SIN(X/13+1))
2780     GFX LINE X,299-H,X+1,299,60,BF
2790   NEXT X
2800   FOR X=0 TO 638 STEP 2
2810     H=INT(14+8*SIN(X/29+2)+4*SIN(X/9))
2820     GFX LINE X,299-H,X+1,299,61,BF
2830   NEXT X
2840   GFX TARGET 3
2850 END PROC
2860 ' ====================================================================== night: shooting stars, fireflies, birds and crickets
2870 PROC AMBIENTINIT
2880   LOCAL I
2890   FOR I=180 TO 185
2900     SPRITE I,0,0,60
2910     SPRITE I,OFF
2920   NEXT I
2930   SPRITE 180,IMAGE 82:SPRITE 181,IMAGE 82
2940 END PROC
2950 PROC NIGHTON
2960   LOCAL I,N,A,B,C,P$
2970   SPRITE 180,620,10,82:SPRITE 180,PRIORITY 1
2980   SPRITE 180,PATH "H W 200 X=620 Y=10 S [ M -7,2 ]90 H W 350 R"
2990   SPRITE 180,ON
3000   SPRITE 181,520,30,82:SPRITE 181,PRIORITY 1
3010   SPRITE 181,PATH "H W 520 X=520 Y=40 S [ M -9,3 ]70 H W 260 R"
3020   SPRITE 181,ON
3030   ' four fireflies that wander (a path that returns to its start, they blink with H and S)
3040   FOR I=0 TO 3
3050     N=182+I
3060     A=10+FN RN(30):B=10+FN RN(30):C=10+FN RN(30)
3070     SPRITE N,100+FN RN(440),150+FN RN(90),60:SPRITE N,PRIORITY 4
3080     P$="T 70 [ M "+STR$(A)+",-4 H W 10 S M "+STR$(-B)+",9 M "+STR$(C)+",-7 H W 10 S M "+STR$(B-A-C)+",2 ]9999"
3090     SPRITE N,PATH P$
3100     SPRITE N,ON
3110   NEXT I
3120 END PROC
3130 PROC NIGHTOFF
3140   LOCAL I
3150   FOR I=180 TO 185
3160     SPRITE I,PATH ""
3170     SPRITE I,OFF
3180   NEXT I
3190 END PROC
3200 ' called every frame: crickets at night (and the stars twinkle), birds by day
3210 PROC AMBIENT
3220   AMT=AMT+1
3230   IF PH=5 THEN
3240     IF FN RN(110)=0 THEN SFX 18
3250     IF AMT MOD 8=0 THEN
3260       GFX PALETTE 0,70,RGB(255,255,FN CLAMP(190+FN RN(66),0,255))
3270       GFX PALETTE 0,71,RGB(FN CLAMP(120+FN RN(100),0,255),FN CLAMP(130+FN RN(100),0,255),255)
3280     END IF
3290   ELSE
3300     IF FN RN(300)=0 THEN SFX 17+2*FN RN(2)
3310   END IF
3320 END PROC
3330 PROC DEFSPRITES
3340   LOCAL R
3350   DESIGN SPRITE 0,16
3360   @ "................"
3370   @ "................"
3380   @ ".......6666....."
3390   @ "......67.7.8...."
3400   @ ".....67....78..."
3410   @ "....67......78.."
3420   @ "...67........8.."
3430   @ "................"
3440   @ "...67..........."
3450   @ "...6.......8...."
3460   @ "..11111....78..."
3470   @ ".1555441.11111.."
3480   @ "155444331555441."
3490   @ "1599433155444331"
3500   @ "14A9333159943331"
3510   @ "143332214A933321"
3520   @ ".133222143332221"
3530   @ "..11111.1332221."
3540   @ ".........11111.."
3550   FOR R=1 TO 5:@ "................":NEXT R
3560   DESIGN SPRITE 1,16
3570   @ "................"
3580   @ "................"
3590   @ ".......6666....."
3600   @ "......67.7.8...."
3610   @ ".....67....78..."
3620   @ "....67......78.."
3630   @ "...67........8.."
3640   @ "................"
3650   @ "...........8...."
3660   @ "...67......78..."
3670   @ "...6.....11111.."
3680   @ "..11111.1555441."
3690   @ ".155544155444331"
3700   @ "1554443159943331"
3710   @ "1599433149A33321"
3720   @ "149A333143332221"
3730   @ "143332221332221."
3740   @ ".1332221.11111.."
3750   @ "..11111........."
3760   FOR R=1 TO 5:@ "................":NEXT R
3770   DESIGN SPRITE 2,32
3780   FOR R=1 TO 4:@ "................":NEXT R
3790   @ "......6.8..6...."
3800   @ ".....6788.8786.."
3810   @ "......67878876.."
3820   @ ".11111116786111."
3830   @ ".15555555544441."
3840   @ ".15555554444431."
3850   @ ".1555994C99C331."
3860   @ ".1555A944A93331."
3870   @ "..154A944A9331.."
3880   @ "..1444C33C3321.."
3890   @ "..1444A33A3221.."
3900   @ "...1333AA3221..."
3910   @ "...1333C22C21..."
3920   @ "....13322221...."
3930   @ ".....122221....."
3940   @ "......11C1......"
3950   FOR R=1 TO 4:@ "................":NEXT R
3960   DESIGN SPRITE 3,32
3970   FOR R=1 TO 3:@ "................":NEXT R
3980   @ "......6.8..6...."
3990   @ ".....6788.8786.."
4000   @ "......67878876.."
4010   @ ".11111116786111."
4020   @ ".15555555544441."
4030   @ ".15555554444431."
4040   @ ".1555994C99C331."
4050   @ ".15559A449A3331."
4060   @ "..1549A449A331.."
4070   @ "..1444C33C3321.."
4080   @ "..14443AA33221.."
4090   @ "...133AAAA221..."
4100   @ "...1333C22C21..."
4110   @ "....13322221...."
4120   @ ".....122221....."
4130   @ "......11C1......"
4140   FOR R=1 TO 5:@ "................":NEXT R
4150   DESIGN SPRITE 4,48
4160   FOR R=1 TO 3:@ "................":NEXT R
4170   @ ".......6.7788..."
4180   @ ".......6.8888..."
4190   @ ".......6..88...."
4200   @ "................"
4210   @ ".....1....1....."
4220   @ "...1151..1411..."
4230   @ "..155551144441.."
4240   @ ".15559944994331."
4250   @ ".1555A944A93331."
4260   @ ".1554A944A93331."
4270   @ ".14444433333221."
4280   @ ".14444333332221."
4290   @ ".14433A33A22221."
4300   @ ".143333AA222221."
4310   @ "..133332222221.."
4320   @ "...1122222211..."
4330   @ ".....111111....."
4340   FOR R=1 TO 4:@ "................":NEXT R
4350   DESIGN SPRITE 5,48
4360   @ "................"
4370   @ "................"
4380   @ ".......6.7788..."
4390   @ ".......6.8888..."
4400   @ ".......6..88...."
4410   @ "................"
4420   @ ".....1....1....."
4430   @ "...1151..1411..."
4440   @ "..155551144441.."
4450   @ ".15559944994331."
4460   @ ".15559A449A3331."
4470   @ ".15549A449A3331."
4480   @ ".14444433333221."
4490   @ ".14444333332221."
4500   @ ".144333AA322221."
4510   @ ".14333AAAA22221."
4520   @ "..133332222221.."
4530   @ "...1122222211..."
4540   @ ".....111111....."
4550   FOR R=1 TO 5:@ "................":NEXT R
4560   DESIGN SPRITE 6,64
4570   FOR R=1 TO 3:@ "................":NEXT R
4580   @ ".......6788....."
4590   @ ".......6........"
4600   @ "................"
4610   @ "......1111......"
4620   @ "....11C55511...."
4630   @ "...15555544C1..."
4640   @ "..15C554444431.."
4650   @ ".15559944993331."
4660   @ ".1555A944A93331."
4670   @ ".1554A944A93331."
4680   @ ".14444433333C21."
4690   @ ".14C44333332221."
4700   @ ".14443A33A22221."
4710   @ ".144333AA222221."
4720   @ "..13333222C221.."
4730   @ "...1332C22221..."
4740   @ "....11222211...."
4750   @ "......1111......"
4760   FOR R=1 TO 3:@ "................":NEXT R
4770   DESIGN SPRITE 7,64
4780   @ "................"
4790   @ "................"
4800   @ ".......6788....."
4810   @ ".......6........"
4820   @ "................"
4830   @ "......1111......"
4840   @ "....11C55511...."
4850   @ "...15555544C1..."
4860   @ "..15C554444431.."
4870   @ ".15559944993331."
4880   @ ".15559A449A3331."
4890   @ ".15549A449A3331."
4900   @ ".14444433333C21."
4910   @ ".14C44333332221."
4920   @ ".144433AA322221."
4930   @ ".14433AAAA22221."
4940   @ "..13333222C221.."
4950   @ "...1332C22221..."
4960   @ "....11222211...."
4970   @ "......1111......"
4980   FOR R=1 TO 4:@ "................":NEXT R
4990   DESIGN SPRITE 8,80
5000   FOR R=1 TO 3:@ "................":NEXT R
5010   @ ".......6788....."
5020   @ ".......6.88....."
5030   FOR R=1 TO 3:@ "................":NEXT R
5040   @ "....11111111...."
5050   @ "...1555554441..."
5060   @ "..155994499441.."
5070   @ ".1555A944A93331."
5080   @ ".1544A944A93331."
5090   @ "1444444333333221"
5100   @ ".14443333332221."
5110   @ ".14333A33A22221."
5120   @ "..13333AA22221.."
5130   @ "...1322222221..."
5140   @ "....11111111...."
5150   FOR R=1 TO 5:@ "................":NEXT R
5160   DESIGN SPRITE 9,80
5170   @ "................"
5180   @ "................"
5190   @ ".......6788....."
5200   @ ".......6.88....."
5210   FOR R=1 TO 3:@ "................":NEXT R
5220   @ "....11111111...."
5230   @ "...1555554441..."
5240   @ "..155994499441.."
5250   @ ".15559A449A3331."
5260   @ ".15449A449A3331."
5270   @ "1444444333333221"
5280   @ ".14443333332221."
5290   @ ".143333AA222221."
5300   @ "..1333AAAA2221.."
5310   @ "...1322222221..."
5320   @ "....11111111...."
5330   FOR R=1 TO 6:@ "................":NEXT R
5340   ' image 10-12: the bird (wings up, level, down)
5350   DESIGN SPRITE 10,96
5360   FOR R=1 TO 5:@ "................":NEXT R
5370   @ ".5............5."
5380   @ ".55..........55."
5390   @ ".555........555."
5400   @ "..555......555.."
5410   @ "..555.1111.555.."
5420   @ ".....123321....."
5430   @ ".....139931....."
5440   @ "......1771......"
5450   @ ".....124421....."
5460   @ ".....124421....."
5470   @ "......1441......"
5480   @ "......1AA1......"
5490   @ ".....1AAAA1....."
5500   @ "......1AA1......"
5510   FOR R=1 TO 5:@ "................":NEXT R
5520   DESIGN SPRITE 11,96
5530   FOR R=1 TO 9:@ "................":NEXT R
5540   @ "......1111......"
5550   @ ".....123321....."
5560   @ "5555.139931.5555"
5570   @ "55555.1771.55555"
5580   @ ".555.124421.555."
5590   @ ".....124421....."
5600   @ "......1441......"
5610   @ "......1AA1......"
5620   @ ".....1AAAA1....."
5630   @ "......1AA1......"
5640   FOR R=1 TO 5:@ "................":NEXT R
5650   DESIGN SPRITE 12,96
5660   FOR R=1 TO 9:@ "................":NEXT R
5670   @ "......1111......"
5680   @ ".....123321....."
5690   @ ".....139931....."
5700   @ "......1771......"
5710   @ ".....124421....."
5720   @ "..5..124421..5.."
5730   @ "..55..1441..55.."
5740   @ ".555..1AA1..555."
5750   @ ".5555.1AA1.5555."
5760   @ "..55..1AA1..55.."
5770   FOR R=1 TO 5:@ "................":NEXT R
5780   ' image 13-16: the gardener (standing, three walking pictures); images 34 (throwing), 35 (the hat), 36 (hit)
5790   DESIGN SPRITE 13,112
5800   @ "................"
5810   @ "......2222......"
5820   @ "....22222223...."
5830   @ "..2222222222333."
5840   @ "...3333333333..."
5850   @ "....44444455...."
5860   @ "....4C4444C5...."
5870   @ "....4441145....."
5880   @ ".....444555....."
5890   @ "...6666666666..."
5900   @ "..666666666677.."
5910   @ "..666666666677.."
5920   @ "..468888888864.."
5930   @ "..4.88888888.4.."
5940   @ "..4.88888888.4.."
5950   @ "....88888899...."
5960   @ "....88888899...."
5970   @ "....888..999...."
5980   @ "....888..999...."
5990   @ "....888..999...."
6000   @ "....888..999...."
6010   @ "....AAA..AAA...."
6020   @ "...AAAA..AAAA..."
6030   @ "................"
6040   DESIGN SPRITE 14,112
6050   @ "................"
6060   @ "......2222......"
6070   @ "....22222223...."
6080   @ "..2222222222333."
6090   @ "...3333333333..."
6100   @ "....44444455...."
6110   @ "....4C4444C5...."
6120   @ "....4441145....."
6130   @ ".....444555....."
6140   @ "...6666666666..."
6150   @ "..666666666677.."
6160   @ "..666666666677.."
6170   @ "..468888888864.."
6180   @ "..4.88888888.4.."
6190   @ "..4.88888888.4.."
6200   @ "....88888899...."
6210   @ "....88888899...."
6220   @ "..888.......999."
6230   @ "..888.......999."
6240   @ "..888.......999."
6250   @ "..888.......999."
6260   @ "..AAA.......AAA."
6270   @ ".AAAA.......AAAA"
6280   @ "................"
6290   DESIGN SPRITE 15,112
6300   @ "................"
6310   @ "......2222......"
6320   @ "....22222223...."
6330   @ "..2222222222333."
6340   @ "...3333333333..."
6350   @ "....44444455...."
6360   @ "....4C4444C5...."
6370   @ "....4441145....."
6380   @ ".....444555....."
6390   @ "...6666666666..."
6400   @ "..666666666677.."
6410   @ "..666666666677.."
6420   @ "..468888888864.."
6430   @ "..4.88888888.4.."
6440   @ "..4.88888888.4.."
6450   @ "....88888899...."
6460   @ "....88888899...."
6470   @ "....888..999...."
6480   @ "....888..999...."
6490   @ "....888..999...."
6500   @ "....888..999...."
6510   @ "....AAA..AAA...."
6520   @ "...AAAA..AAAA..."
6530   @ "................"
6540   DESIGN SPRITE 16,112
6550   @ "................"
6560   @ "......2222......"
6570   @ "....22222223...."
6580   @ "..2222222222333."
6590   @ "...3333333333..."
6600   @ "....44444455...."
6610   @ "....4C4444C5...."
6620   @ "....4441145....."
6630   @ ".....444555....."
6640   @ "...6666666666..."
6650   @ "..666666666677.."
6660   @ "..666666666677.."
6670   @ "..468888888864.."
6680   @ "..4.88888888.4.."
6690   @ "..4.88888888.4.."
6700   @ "....88888899...."
6710   @ "....88888899...."
6720   @ ".888.......999.."
6730   @ ".888.......999.."
6740   @ ".888.......999.."
6750   @ ".888.......999.."
6760   @ ".AAA.......AAA.."
6770   @ "AAAA.......AAAA."
6780   @ "................"
6790   ' image 17-18: the ball (red, and blue for rapid fire)
6800   DESIGN SPRITE 17,128
6810   FOR R=1 TO 6:@ "................":NEXT R
6820   @ "......1111......"
6830   @ ".....122221....."
6840   @ "....12234221...."
6850   @ "....12222221...."
6860   @ "....12222221...."
6870   @ ".....122221....."
6880   @ "......1111......"
6890   FOR R=1 TO 11:@ "................":NEXT R
6900   DESIGN SPRITE 18,128
6910   FOR R=1 TO 6:@ "................":NEXT R
6920   @ "......5555......"
6930   @ ".....566665....."
6940   @ "....56674665...."
6950   @ "....56666665...."
6960   @ "....56666665...."
6970   @ ".....566665....."
6980   @ "......5555......"
6990   FOR R=1 TO 11:@ "................":NEXT R
7000   ' image 19-21: the seed turns while it falls
7010   DESIGN SPRITE 19,144
7020   FOR R=1 TO 7:@ "................":NEXT R
7030   @ ".......11......."
7040   @ "......1221......"
7050   @ ".....123321....."
7060   @ ".....123321....."
7070   @ ".....123321....."
7080   @ "......1221......"
7090   @ "......1221......"
7100   @ ".......11......."
7110   @ ".......1........"
7120   FOR R=1 TO 8:@ "................":NEXT R
7130   DESIGN SPRITE 20,144
7140   FOR R=1 TO 7:@ "................":NEXT R
7150   @ ".......11......."
7160   @ "......1221......"
7170   @ "......1231......"
7180   @ "......1231......"
7190   @ "......1231......"
7200   @ "......1221......"
7210   @ "......1221......"
7220   @ ".......11......."
7230   @ ".......1........"
7240   FOR R=1 TO 8:@ "................":NEXT R
7250   DESIGN SPRITE 21,144
7260   FOR R=1 TO 7:@ "................":NEXT R
7270   @ ".......11......."
7280   @ "......1221......"
7290   @ ".....122321....."
7300   @ ".....122321....."
7310   @ ".....122321....."
7320   @ "......1221......"
7330   @ "......1221......"
7340   @ ".......11......."
7350   @ ".......1........"
7360   FOR R=1 TO 8:@ "................":NEXT R
7370   DESIGN SPRITE 22,160
7380   FOR R=1 TO 3:@ "................":NEXT R
7390   @ ".............5.."
7400   @ "..1111111111555."
7410   @ ".14644444433351."
7420   @ ".14444443333331."
7430   @ "1444443333333331"
7440   @ "1444333333333331"
7450   @ "1433311111133321"
7460   @ "13331......12221"
7470   @ "1331........1221"
7480   @ "131..........121"
7490   @ ".11..........11."
7500   @ ".11..........11."
7510   @ "..1..........1.."
7520   FOR R=1 TO 8:@ "................":NEXT R
7530   DESIGN SPRITE 23,160
7540   FOR R=1 TO 4:@ "................":NEXT R
7550   @ "..111111111111.."
7560   @ ".14644444433361."
7570   @ ".14444443333331."
7580   @ "1444443333333331"
7590   @ "1444333333333331"
7600   @ "1433311111133321"
7610   @ "13331......12221"
7620   @ "1331........1221"
7630   @ "1315.........121"
7640   @ ".1555........11."
7650   @ ".115.........11."
7660   @ "..1..........1.."
7670   FOR R=1 TO 8:@ "................":NEXT R
7680   DESIGN SPRITE 24,176
7690   FOR R=1 TO 9:@ "................":NEXT R
7700   @ ".......33......."
7710   @ "......3553......"
7720   @ ".....355443....."
7730   @ ".....354443....."
7740   @ "......3443......"
7750   @ ".......33......."
7760   FOR R=1 TO 9:@ "................":NEXT R
7770   DESIGN SPRITE 25,176
7780   FOR R=1 TO 5:@ "................":NEXT R
7790   @ "........4......."
7800   @ "................"
7810   @ "...4.222222..4.."
7820   @ "....25555442...."
7830   @ "...2555444432..."
7840   @ "...2554444332..."
7850   @ "...2544433332..."
7860   @ ".4.2444333332..4"
7870   @ "...2443333332..."
7880   @ "...2333333332..."
7890   @ "....23333332...."
7900   @ ".....222222....."
7910   @ "...4.........4.."
7920   @ "................"
7930   @ "........4......."
7940   FOR R=1 TO 4:@ "................":NEXT R
7950   DESIGN SPRITE 26,176
7960   FOR R=1 TO 3:@ "................":NEXT R
7970   @ "......5........."
7980   @ ".......22......."
7990   @ "....22222222...."
8000   @ "...2223333222..5"
8010   @ "..223333333322.."
8020   @ ".22334444443322."
8030   @ "522344444444322."
8040   @ ".23344....44332."
8050   @ "223344....443322"
8060   @ "223344....443322"
8070   @ ".23344....44332."
8080   @ ".22344444444322."
8090   @ ".22334444443322."
8100   @ "..223333333322.."
8110   @ "...2223333222..."
8120   @ ".5..22222222...."
8130   @ ".......22......."
8140   @ "................"
8150   @ "..........5....."
8160   @ "................"
8170   @ "................"
8180   DESIGN SPRITE 27,176
8190   FOR R=1 TO 4:@ "................":NEXT R
8200   @ ".........78....."
8210   @ ".........8......"
8220   @ "....78.........."
8230   @ "....8..........."
8240   @ ".........8......"
8250   @ "................"
8260   @ "................"
8270   @ "....8..........."
8280   @ "...............7"
8290   @ "............8..8"
8300   @ "................"
8310   @ "78.............."
8320   @ "8......8........"
8330   @ "................"
8340   @ ".....78........7"
8350   @ ".....8.........8"
8360   @ "................"
8370   @ "................"
8380   @ "..........78...."
8390   @ "..........8....."
8400   DESIGN SPRITE 28,16
8410   FOR R=1 TO 11:@ "................":NEXT R
8420   @ "........5......."
8430   @ ".......534......"
8440   @ "........3......."
8450   FOR R=1 TO 10:@ "................":NEXT R
8460   DESIGN SPRITE 29,32
8470   FOR R=1 TO 11:@ "................":NEXT R
8480   @ "........5......."
8490   @ ".......534......"
8500   @ "........3......."
8510   FOR R=1 TO 10:@ "................":NEXT R
8520   DESIGN SPRITE 30,48
8530   FOR R=1 TO 11:@ "................":NEXT R
8540   @ "........5......."
8550   @ ".......534......"
8560   @ "........3......."
8570   FOR R=1 TO 10:@ "................":NEXT R
8580   DESIGN SPRITE 31,64
8590   FOR R=1 TO 11:@ "................":NEXT R
8600   @ "........5......."
8610   @ ".......534......"
8620   @ "........3......."
8630   FOR R=1 TO 10:@ "................":NEXT R
8640   DESIGN SPRITE 32,80
8650   FOR R=1 TO 11:@ "................":NEXT R
8660   @ "........5......."
8670   @ ".......534......"
8680   @ "........3......."
8690   FOR R=1 TO 10:@ "................":NEXT R
8700   DESIGN SPRITE 33,176
8710   FOR R=1 TO 11:@ "................":NEXT R
8720   @ "........6......."
8730   @ ".......656......"
8740   @ "........6......."
8750   FOR R=1 TO 10:@ "................":NEXT R
8760   DESIGN SPRITE 34,112
8770   @ "................"
8780   @ "......2222......"
8790   @ "....22222223...."
8800   @ "..2222222222333."
8810   @ "...3333333333..."
8820   @ "....44444455.44."
8830   @ "....4C4444C5..44"
8840   @ "....4441145.66.."
8850   @ ".....444555.66.."
8860   @ "...666666666666."
8870   @ "..666666666677.."
8880   @ "..666666666677.."
8890   @ "..468888888888.."
8900   @ "..4.88888888...."
8910   @ "..4.88888888...."
8920   @ "....88888899...."
8930   @ "....88888899...."
8940   @ "....888..999...."
8950   @ "....888..999...."
8960   @ "....888..999...."
8970   @ "....888..999...."
8980   @ "....AAA..AAA...."
8990   @ "...AAAA..AAAA..."
9000   @ "................"
9010   DESIGN SPRITE 35,112
9020   FOR R=1 TO 6:@ "................":NEXT R
9030   @ "......2222......"
9040   @ "....22222223...."
9050   @ "..2222222222333."
9060   @ "...3333333333..."
9070   FOR R=1 TO 14:@ "................":NEXT R
9080   DESIGN SPRITE 36,112
9090   @ "................"
9100   @ "......1111......"
9110   @ "....11111111...."
9120   @ "...1111111111..."
9130   @ "...1144444411..."
9140   @ ".44.44444455.44."
9150   @ ".66.4C4C4CC5.66."
9160   @ ".66.4441C45.66.."
9170   @ ".66..444555.66.."
9180   @ "..666666666666.."
9190   @ "..666666666677.."
9200   @ "..666666666677.."
9210   @ "..4.88888888.4.."
9220   @ "..4.88888888.4.."
9230   @ "....88888888...."
9240   @ "....88888899...."
9250   @ "....88888899...."
9260   @ "....888..999...."
9270   @ "....888..999...."
9280   @ "....888..999...."
9290   @ "....888..999...."
9300   @ "....AAA..AAA...."
9310   @ "...AAAA..AAAA..."
9320   @ "................"
9330   ' image 37-39: pieces of the clouds that fly by in front of the tile clouds
9340   DESIGN SPRITE 37,192
9350   FOR R=1 TO 8:@ "................":NEXT R
9360   @ "........1111...."
9370   @ "......11111111.."
9380   @ "....111111111111"
9390   @ "...1111111111111"
9400   @ "..11111111111111"
9410   @ ".111111111111111"
9420   @ ".111111111111111"
9430   @ "1111111111111111"
9440   @ "1111111111111111"
9450   @ ".222222222222222"
9460   @ "..22222222222222"
9470   FOR R=1 TO 5:@ "................":NEXT R
9480   DESIGN SPRITE 38,192
9490   FOR R=1 TO 5:@ "................":NEXT R
9500   @ "......1111......"
9510   @ "....11111111...."
9520   @ "...1111111111..."
9530   @ "..111111111111.."
9540   @ ".11111111111111."
9550   @ "1111111111111111"
9560   @ "1111111111111111"
9570   @ "1111111111111111"
9580   @ "1111111111111111"
9590   @ "2222222222222222"
9600   @ "2222222222222222"
9610   FOR R=1 TO 8:@ "................":NEXT R
9620   DESIGN SPRITE 39,192
9630   FOR R=1 TO 8:@ "................":NEXT R
9640   @ "....1111........"
9650   @ "..11111111......"
9660   @ "111111111111...."
9670   @ "1111111111111..."
9680   @ "11111111111111.."
9690   @ "111111111111111."
9700   @ "111111111111111."
9710   @ "1111111111111111"
9720   @ "1111111111111111"
9730   @ "2222222222222222"
9740   @ ".222222222222222"
9750   @ "..2222222222222."
9760   FOR R=1 TO 4:@ "................":NEXT R
9770   ' image 40: a leaf that flies off a bush
9780   DESIGN SPRITE 40,208
9790   FOR R=1 TO 11:@ "................":NEXT R
9800   @ ".......1........"
9810   @ "......121......."
9820   @ ".....12321......"
9830   @ "......121......."
9840   @ ".......1........"
9850   FOR R=1 TO 8:@ "................":NEXT R
9860   ' image 41-44: the coconut (two frames) and the cracked coconut (two frames)
9870   DESIGN SPRITE 41,212
9880   FOR R=1 TO 5:@ "................":NEXT R
9890   @ ".....111111....."
9900   @ "....11222211...."
9910   @ "...1333229221..."
9920   @ "..133333222221.."
9930   @ ".11333332292211."
9940   @ ".12233322222221."
9950   @ ".12227722277221."
9960   @ ".12228722287221."
9970   @ ".12222222222921."
9980   @ ".12222292222221."
9990   @ ".11222244222211."
10000   @ "..122442944221.."
10010   @ "...1222222221..."
10020   @ "....11222911...."
10030   @ ".....111111....."
10040   FOR R=1 TO 4:@ "................":NEXT R
10050   DESIGN SPRITE 42,212
10060   FOR R=1 TO 5:@ "................":NEXT R
10070   @ ".....111111....."
10080   @ "....11222211...."
10090   @ "...1333229221..."
10100   @ "..133333222221.."
10110   @ ".11333332292211."
10120   @ ".12233322222221."
10130   @ ".12227822278221."
10140   @ ".12227822278221."
10150   @ ".12222222222921."
10160   @ ".12222292222221."
10170   @ ".11222244222211."
10180   @ "..122442944221.."
10190   @ "...1222222221..."
10200   @ "....11222911...."
10210   @ ".....111111....."
10220   FOR R=1 TO 4:@ "................":NEXT R
10230   DESIGN SPRITE 43,212
10240   FOR R=1 TO 5:@ "................":NEXT R
10250   @ ".....11CA11....."
10260   @ "....1122CA11...."
10270   @ "...1333CA9221..."
10280   @ "..133333CA2221.."
10290   @ ".11333332CA2211."
10300   @ ".1223332CA22221."
10310   @ ".12227722277221."
10320   @ ".12228722287221."
10330   @ ".12222222222921."
10340   @ ".12222292222221."
10350   @ ".11222244222211."
10360   @ "..122442944221.."
10370   @ "...1222222221..."
10380   @ "....11222911...."
10390   @ ".....111111....."
10400   FOR R=1 TO 4:@ "................":NEXT R
10410   DESIGN SPRITE 44,212
10420   FOR R=1 TO 5:@ "................":NEXT R
10430   @ ".....11CA11....."
10440   @ "....1122CA11...."
10450   @ "...1333CA9221..."
10460   @ "..133333CA2221.."
10470   @ ".11333332CA2211."
10480   @ ".1223332CA22221."
10490   @ ".12227822278221."
10500   @ ".12227822278221."
10510   @ ".12222222222921."
10520   @ ".12222292222221."
10530   @ ".11222244222211."
10540   @ "..122442944221.."
10550   @ "...1222222221..."
10560   @ "....11222911...."
10570   @ ".....111111....."
10580   FOR R=1 TO 4:@ "................":NEXT R
10590   ' image 50-52: the power-up crates (two balls, big ball, umbrella)
10600   DESIGN SPRITE 50,224
10610   FOR R=1 TO 7:@ "................":NEXT R
10620   @ "..111111111111.."
10630   @ "..122222222221.."
10640   @ "..122222222221.."
10650   @ "..124422244221.."
10660   @ "..124422244221.."
10670   @ "..124422244221.."
10680   @ "..122222222221.."
10690   @ "..122222222221.."
10700   @ "..133333333331.."
10710   @ "..133333333331.."
10720   @ "..111111111111.."
10730   FOR R=1 TO 6:@ "................":NEXT R
10740   DESIGN SPRITE 51,224
10750   FOR R=1 TO 7:@ "................":NEXT R
10760   @ "..111111111111.."
10770   @ "..122222222221.."
10780   @ "..122266662221.."
10790   @ "..122656666221.."
10800   @ "..122666666221.."
10810   @ "..122666666221.."
10820   @ "..122266662221.."
10830   @ "..122222222221.."
10840   @ "..133333333331.."
10850   @ "..133333333331.."
10860   @ "..111111111111.."
10870   FOR R=1 TO 6:@ "................":NEXT R
10880   DESIGN SPRITE 52,224
10890   FOR R=1 TO 7:@ "................":NEXT R
10900   @ "..111111111111.."
10910   @ "..122222222221.."
10920   @ "..122454545221.."
10930   @ "..125454545421.."
10940   @ "..122229922221.."
10950   @ "..122229922221.."
10960   @ "..122299222221.."
10970   @ "..122222222221.."
10980   @ "..133333333331.."
10990   @ "..133333333331.."
11000   @ "..111111111111.."
11010   FOR R=1 TO 6:@ "................":NEXT R
11020   ' image 53: the big ball
11030   DESIGN SPRITE 53,128
11040   FOR R=1 TO 6:@ "................":NEXT R
11050   @ ".....111111....."
11060   @ "....12222221...."
11070   @ "...1444432221..."
11080   @ "..124444322221.."
11090   @ "..124444322221.."
11100   @ "..123333322221.."
11110   @ "..123333322221.."
11120   @ "..122222222221.."
11130   @ "..122222222221.."
11140   @ "...1222222221..."
11150   @ "....12222221...."
11160   @ ".....111111....."
11170   FOR R=1 TO 6:@ "................":NEXT R
11180   ' image 60: a firefly
11190   DESIGN SPRITE 60,224
11200   FOR R=1 TO 10:@ "................":NEXT R
11210   @ ".......AA......."
11220   @ "......ABBA......"
11230   @ "......ABBA......"
11240   @ ".......AA......."
11250   FOR R=1 TO 10:@ "................":NEXT R
11260 END PROC
11270 ' ====================================================================== pictures that the program draws (SPRITE DEF n,a$: 16 x 24 bytes, a palette index each)
11280 PROC DEFART
11290   CALL DEFSOCK
11300   CALL DEFUMB
11310   CALL DEFBALLOON
11320   CALL DEFMELON(70,4,1)
11330   CALL DEFMELON(78,2,0.5)
11340   CALL DEFSTAR
11350 END PROC
11360 ' images 45..49: the windsock for the wind -2, -1, 0, 1, 2: a pole and three pieces of the sock that point downwind; the stronger the wind, the flatter it is
11370 PROC DEFSOCK
11380   LOCAL W,X,Y,K,A$,C,S,PXX,ANG,CX,CY
11390   FOR W=-2 TO 2
11400     S=1
11410     IF W<0 THEN S=-1
11420     PXX=3
11430     IF W<0 THEN PXX=12
11440     ANG=(1-ABS(W)/2)*1.45
11450     A$=""
11460     FOR Y=0 TO 23
11470       FOR X=0 TO 15
11480         C=0
11490         IF (X=PXX OR X=PXX-1) AND Y>=3 THEN C=233
11500         FOR K=0 TO 2
11510           CX=PXX-0.5+S*COS(ANG)*(K*4+3)
11520           CY=3+SIN(ANG)*(K*4+3)
11530           IF ABS(X-CX)<=2 AND ABS(Y-CY)<=2 THEN C=228+(K MOD 2)
11540         NEXT K
11550         A$=A$+CHR$(C)
11560       NEXT X
11570     NEXT Y
11580     SPRITE DEF 47+W,A$
11590   NEXT W
11600 END PROC
11610 ' images 54, 55: the umbrella (a group of 2 x 1 sprites): a dome with red and white stripes and a short handle
11620 PROC DEFUMB
11630   LOCAL H,X,Y,A$,C,QX,U,TY
11640   FOR H=0 TO 1
11650     A$=""
11660     FOR Y=0 TO 23
11670       FOR X=0 TO 15
11680         QX=H*16+X
11690         C=0
11700         U=(QX-15.5)/16
11710         TY=6+8*(1-SQR(1-U*U))
11720         IF Y>=TY AND Y<=13 THEN C=228+(INT(QX/5) MOD 2)
11730         IF Y>=TY AND Y<TY+1 THEN C=225
11740         IF (QX=15 OR QX=16) AND Y>=14 AND Y<=17 THEN C=233
11750         A$=A$+CHR$(C)
11760       NEXT X
11770     NEXT Y
11780     SPRITE DEF 54+H,A$
11790   NEXT H
11800 END PROC
11810 ' images 56..59: the balloon (a group of 2 x 2 sprites, 32 x 48 pixels): a striped envelope, ropes and a basket
11820 PROC DEFBALLOON
11830   LOCAL K,X,Y,QX,QY,A$,C,D,DX,DY,T,XL,XR
11840   FOR K=0 TO 3
11850     A$=""
11860     FOR Y=0 TO 23
11870       FOR X=0 TO 15
11880         QX=(K MOD 2)*16+X:QY=(K\2)*24+Y:C=0
11890         DX=(QX-15.5)/15.5:DY=(QY-17)/17
11900         D=DX*DX+DY*DY
11910         IF D<1 THEN
11920           C=228
11930           T=INT((QX-1)/6) MOD 3
11940           IF T=1 THEN C=229
11950           IF T=2 THEN C=226
11960           IF D>0.86 THEN C=225
11970         END IF
11980         IF QY>=34 AND QY<=39 THEN
11990           T=(QY-33)/7
12000           XL=9+3*T:XR=22-3*T
12010           IF ABS(QX-XL)<0.7 OR ABS(QX-XR)<0.7 THEN C=225
12020         END IF
12030         IF QX>=11 AND QX<=20 AND QY>=40 AND QY<=46 THEN
12040           C=233
12050           IF (QX+QY) MOD 4=0 THEN C=225
12060           IF QY=40 OR QY=46 OR QX=11 OR QX=20 THEN C=225
12070         END IF
12080         A$=A$+CHR$(C)
12090       NEXT X
12100     NEXT Y
12110     SPRITE DEF 56+K,A$
12120   NEXT K
12130 END PROC
12140 ' a melon of GW x 2 sprites (K0 is the first image): GW = 4 is the giant one (S = 1), GW = 2 one of the two halves it splits into (S = 0.5)
12150 PROC DEFMELON(K0,GW,S)
12160   LOCAL K,X,Y,QX,QY,D,C,A$,DX,DY,AN,CXM,EX1,EX2,EY,RYM
12170   CXM=GW*8
12180   RYM=23
12190   IF GW=2 THEN RYM=CXM-1
12200   EY=24-5*S:EX1=CXM-10*S:EX2=CXM+10*S
12210   FOR K=0 TO GW*2-1
12220     A$=""
12230     FOR Y=0 TO 23
12240       FOR X=0 TO 15
12250         QX=(K MOD GW)*16+X:QY=(K\GW)*24+Y:C=0
12260         DX=(QX-CXM)/(CXM-1):DY=(QY-24)/RYM
12270         D=DX*DX+DY*DY
12280         IF D<1 THEN
12290           AN=INT((ATAN2(QY-24,QX-CXM)+PI)*8/PI)
12300           C=196+(AN MOD 2)
12310           IF D<0.72 THEN C=202
12320           IF D<0.62 THEN
12330             C=198
12340             IF DX+DY<-0.5 THEN C=199
12350           END IF
12360           IF D<0.5 AND ((QX*7+QY*13) MOD 23=0) THEN C=200
12370           IF (QX-EX1)^2+(QY-EY)^2<20*S*S OR (QX-EX2)^2+(QY-EY)^2<20*S*S THEN C=201
12380           IF (QX-EX1-S)^2+(QY-EY-S)^2<5*S*S OR (QX-EX2+S)^2+(QY-EY-S)^2<5*S*S THEN C=200
12390           IF S=1 AND QY>=12 AND QY<=16 AND ((QX>=CXM-16 AND QX<=CXM-4 AND QY=12+(QX-CXM+16)\3) OR (QX>=CXM+4 AND QX<=CXM+16 AND QY=16-(QX-CXM-4)\3)) THEN C=200
12400           IF ((QX-CXM)/(11*S))^2+((QY-24-11*S)/(5*S))^2<1 AND QY>=24+9*S THEN
12410             C=200
12420             IF QY<=24+10*S AND (QX MOD 4)<2 THEN C=201
12430           END IF
12440         END IF
12450         A$=A$+CHR$(C)
12460       NEXT X
12470     NEXT Y
12480     SPRITE DEF K0+K,A$
12490   NEXT K
12500 END PROC
12510 ' image 82: a shooting star (a short streak that gets darker towards the tail)
12520 PROC DEFSTAR
12530   LOCAL X,Y,C,A$
12540   A$=""
12550   FOR Y=0 TO 23
12560     FOR X=0 TO 15
12570       C=0
12580       IF Y>=10 AND Y<=11 THEN C=203+X\5
12590       IF Y=10 AND X<3 THEN C=203
12600       A$=A$+CHR$(C)
12610     NEXT X
12620   NEXT Y
12630   SPRITE DEF 82,A$
12640 END PROC
12650 PROC DEFTILES
12660   LOCAL R
12670   DESIGN TILE 1
12680   FOR R=1 TO 9:@ "................":NEXT R
12690   @ "........1111...."
12700   @ "......11111111.."
12710   @ ".....1111111111."
12720   @ "....111111111111"
12730   @ "...1111111111111"
12740   @ "...1111111111111"
12750   @ "..11111111111111"
12760   @ "..11111111111111"
12770   @ ".111111111111111"
12780   @ "1111111111111111"
12790   @ ".111111111111111"
12800   @ "..22222222222222"
12810   @ "...2222222222222"
12820   FOR R=1 TO 2:@ "................":NEXT R
12830   DESIGN TILE 2
12840   FOR R=1 TO 4:@ "................":NEXT R
12850   @ "......1111......"
12860   @ "....11111111...."
12870   @ "...1111111111..."
12880   @ "..111111111111.."
12890   @ ".11111111111111."
12900   @ ".11111111111111."
12910   @ "1111111111111111"
12920   @ "1111111111111111"
12930   @ "1111111111111111"
12940   @ "1111111111111111"
12950   @ "1111111111111111"
12960   @ "1111111111111111"
12970   @ "1111111111111111"
12980   @ "1111111111111111"
12990   @ "1111111111111111"
13000   @ "1111111111111111"
13010   @ "1111111111111111"
13020   @ "2222222222222222"
13030   @ "2222222222222222"
13040   @ "................"
13050   DESIGN TILE 3
13060   FOR R=1 TO 10:@ "................":NEXT R
13070   @ ".11111111......."
13080   @ "11111111111....."
13090   @ "111111111111...."
13100   @ "1111111111111..."
13110   @ "1111111111111..."
13120   @ "11111111111111.."
13130   @ "11111111111111.."
13140   @ "111111111111111."
13150   @ "111111111111111."
13160   @ "1111111111111111"
13170   @ "11111111111111.."
13180   @ "22222222222222.."
13190   @ "222222222222...."
13200   @ "................"
13210   DESIGN TILE 4
13220   FOR R=1 TO 13:@ "................":NEXT R
13230   @ "......1111......"
13240   @ "....11111111...."
13250   @ "..111111111111.."
13260   @ ".11111111111111."
13270   @ "1111111111111111"
13280   @ "1111111111111111"
13290   @ "1111111111111111"
13300   @ "2222222222222222"
13310   @ ".22222222222222."
13320   FOR R=1 TO 2:@ "................":NEXT R
13330 END PROC
13340 ' the clouds of the tile layer live in a world of 64 columns that wraps around (CW$, 13 rows after each other, tile numbers as characters: 0 empty,
13350 ' 1 left, 2 middle, 3 right part of a cloud, 4 a small puff); only rows 1, 3, 5 and 7 have clouds
13360 PROC CLOUDMAP
13370   LOCAL R,C,K,L,I,RW$,CL$
13380   CW$=""
13390   FOR R=0 TO 12
13400     RW$=STRING$(64,0)
13410     IF R=1 OR R=3 OR R=5 OR R=7 THEN
13420       FOR K=1 TO 3
13430         L=1+FN RN(4)
13440         IF L=1 THEN
13450           CL$=CHR$(4)
13460         ELSE
13470           CL$=CHR$(1)
13480           FOR I=2 TO L-1
13490             CL$=CL$+CHR$(2)
13500           NEXT I
13510           CL$=CL$+CHR$(3)
13520         END IF
13530         C=FN RN(64-L)
13540         RW$=LEFT$(RW$,C)+CL$+MID$(RW$,C+LEN(CL$)+1)
13550       NEXT K
13560     END IF
13570     CW$=CW$+RW$
13580   NEXT R
13590   CAMF=100:CAMX=0:CLASTC=-1
13600 END PROC
13610 ' the clouds drift: the camera CAMF moves with the wind; it also follows the gardeners a little (CAMX = CAMF + position \ 6): the tile clouds, the cloud sprites
13620 ' and the fruit move against each other. Every 16 pixels the 41 x 13 cells are refilled from the world (cell I of a row shows the world column (CAMX-1)\16 + 1 + I,
13630 ' the last cell the column before), in between TILE SCROLL moves the picture
13640 PROC CLOUDS
13650   LOCAL R,I,CC,M$,K,NX0,AP
13660   CAMF=CAMF+0.7+WIND*0.3
13670   AP=PX(0)
13680   IF NPL=2 THEN AP=(PX(0)+PX(1))\2
13690   NX0=INT(CAMF)+AP\6
13700   IF NX0=CAMX THEN EXIT PROC
13710   CAMX=NX0
13720   CC=(CAMX-1)\16
13730   IF CC<>CLASTC THEN
13740     CLASTC=CC
13750     M$=""
13760     FOR R=0 TO 12
13770       IF R=1 OR R=3 OR R=5 OR R=7 THEN
13780         FOR I=0 TO 40
13790           K=(CC+1+I) MOD 64
13800           IF I=40 THEN K=CC MOD 64
13810           M$=M$+MID$(CW$,R*64+K+1,1)
13820         NEXT I
13830       ELSE
13840         M$=M$+STRING$(41,CHR$(0))
13850       END IF
13860     NEXT R
13870     TILE FILL M$
13880   END IF
13890   CSX=15-(CAMX-1) MOD 16
13900   TILE SCROLL CSX,0
13910 END PROC
13920 ' three cloud groups in front of the tile clouds, nearer ones faster: sprite groups of 3 x 1 sprites that fly on paths (the engine moves them, the program does nothing);
13930 ' priority 1 and 2 put them behind the fruit and the gardeners, which have the standard priority 3
13940 PROC CLOUDSPRITES
13950   LOCAL I,N,Y,V,X0
13960   FOR I=0 TO 2
13970     N=130+I*3
13980     SPRITE N,0,0,37:SPRITE N+1,0,0,38:SPRITE N+2,0,0,39
13990     SPRITE N,SIZE 3,1
14000     SPRITE N,PRIORITY 1+I\2
14010     SPRITE N,ON
14020     Y=50+I*60:V=1+I:X0=60+I*230
14030     SPRITE N,PATH "X="+STR$(X0)+" Y="+STR$(Y)+" X+"+STR$(V)+"*"+STR$(INT((680-X0)/V))+" [ X=-60 X+"+STR$(V)+"*"+STR$(INT(740/V))+" ]9999"
14040   NEXT I
14050 END PROC
14060 ' ====================================================================== the screen
14070 PROC CLEARALL
14080   LOCAL I
14090   EVERY OFF
14100   AFTER OFF
14110   SOUND OFF
14120   CLS
14130   GFX CLEAR
14140   FOR I=0 TO 127
14150     SPRITE I,OFF
14160   NEXT I
14170   SPRITE 140,PATH "":SPRITE 140,OFF
14180   SPRITE 160,PATH "":SPRITE 160,OFF
14190   SPRITE 170,PATH "":SPRITE 170,OFF
14200   SPRITE 174,PATH "":SPRITE 174,OFF
14210   SPRITE 190,OFF:SPRITE 192,OFF
14220   FOR I=150 TO 159
14230     SPRITE I,OFF
14240   NEXT I
14250   FOR I=0 TO 3
14260     EP(I)=0
14270   NEXT I
14280   FOR I=0 TO 63
14290     PL(I)=0
14300   NEXT I
14310   BOSS=0:BAL=0:CR=0:PU=0:UA=0
14320 END PROC
14330 ' the grass strip at the bottom of the screen, on the graphics layer, with a few flowers
14340 PROC GROUND
14350   LOCAL I,X,Y
14360   GFX LINE 0,288,639,299,4,BF
14370   GFX LINE 0,288,639,289,5,BF
14380   FOR I=1 TO 90
14390     GFX PSET FN RN(640),290+FN RN(10),5
14400   NEXT I
14410   FOR I=1 TO 45
14420     X=FN RN(640):Y=291+FN RN(8)
14430     GFX PSET X,Y,7+FN RN(3)
14440   NEXT I
14450 END PROC
14460 ' the four round bushes: every bush is four disks, drawn three times: a dark shadow, the green, and light leaves on top
14470 PROC BUSHES
14480   LOCAL I,B,P,DX,DY,R
14490   FOR I=0 TO 3
14500     B=90+I*140
14510     RESTORE BUSHDATA
14520     FOR P=0 TO 3
14530       READ DX,DY,R
14540       CALL DISK(B+DX+1,SHY+DY+2,R,1)
14550     NEXT P
14560     RESTORE BUSHDATA
14570     FOR P=0 TO 3
14580       READ DX,DY,R
14590       CALL DISK(B+DX,SHY+DY,R,2)
14600     NEXT P
14610     CALL DISK(B-12,SHY+10,3,3):CALL DISK(B,SHY+5,5,3):CALL DISK(B+11,SHY+10,3,3)
14620     CALL DISK(B-5,SHY+14,2,3):CALL DISK(B+6,SHY+16,2,3)
14630   NEXT I
14640 END PROC
14650 LABEL BUSHDATA
14660 DATA -11,13,9, 11,13,9, 0,9,11, 0,14,9
14670 ' ====================================================================== the score line, the lives (small hats) and the power-ups
14680 PROC HUD
14690   LOCAL I,P,X,T$
14700   IF NPL=1 THEN
14710     CALL TXTAT(1,0,"SCORE",205)
14720     CALL TXTAT(7,0,FN PAD$(SCO(0),6),201)
14730     CALL TXTAT(22,0,"HI",205)
14740     CALL TXTAT(25,0,FN PAD$(HI,6),200)
14750     CALL TXTAT(38,0,"WAVE",205)
14760     CALL TXTAT(43,0,STR$(WV)+"  ",202)
14770     CALL TXTAT(58,0,"LIVES",205)
14780   ELSE
14790     CALL TXTAT(1,0,"P1",205)
14800     CALL TXTAT(4,0,FN PAD$(SCO(0),6),201)
14810     CALL TXTAT(24,0,"HI",205)
14820     CALL TXTAT(27,0,FN PAD$(HI,6),200)
14830     CALL TXTAT(38,0,"WAVE",205)
14840     CALL TXTAT(43,0,STR$(WV)+"  ",202)
14850     CALL TXTAT(52,0,"P2",205)
14860     CALL TXTAT(55,0,FN PAD$(SCO(1),6),201)
14870   END IF
14880   FOR P=0 TO 1
14890     FOR I=0 TO 4
14900       IF P<NPL AND I<LV(P) THEN
14910         IF NPL=1 THEN
14920           SPRITE 150+P*5+I,524+I*20,-3,35
14930         ELSE
14940           SPRITE 150+P*5+I,96+P*408+I*13,-3,35
14950         END IF
14960         SPRITE 150+P*5+I,ON
14970       ELSE
14980         SPRITE 150+P*5+I,OFF
14990       END IF
15000     NEXT I
15010   NEXT P
15020   FOR P=0 TO NPL-1
15030     T$=""
15040     IF RAP(P)>0 THEN T$="RAPID FIRE"
15050     IF PWT(P)>0 THEN
15060       IF PWK(P)=1 THEN T$="TWO BALLS "
15070       IF PWK(P)=2 THEN T$="BIG BALL "
15080       IF PWK(P)=3 THEN T$="UMBRELLA "
15090       T$=T$+STR$(PWT(P)\60+1)
15100     END IF
15110     X=1
15120     IF P=1 THEN X=52
15130     CALL TXTAT(X,1,LEFT$(T$+"                    ",20),203)
15140   NEXT P
15150   HF=0
15160 END PROC
15170 ' ====================================================================== keys
15180 ' one player: the arrows or A / D walk, SPACE, UP or W throw, the pad 1; two players: A / D and W (or SPACE) with the pad 1, the arrows and UP (or RETURN) with the pad 2
15190 PROC KEYS
15200   LOCAL P,L,R,T
15210   FOR P=0 TO NPL-1
15220     IF ALV(P)=1 THEN
15230       IF NPL=1 THEN
15240         L=KEY("LEFT") OR KEY("A") OR JOY("LEFT",1) OR JOY("LX",1)<-0.4
15250         R=KEY("RIGHT") OR KEY("D") OR JOY("RIGHT",1) OR JOY("LX",1)>0.4
15260         T=KEY("SPACE") OR KEY("UP") OR KEY("W") OR JOY("A",1) OR JOY("RT",1)>0.4
15270       ELSE
15280         IF P=0 THEN
15290           L=KEY("A") OR JOY("LEFT",1) OR JOY("LX",1)<-0.4
15300           R=KEY("D") OR JOY("RIGHT",1) OR JOY("LX",1)>0.4
15310           T=KEY("W") OR KEY("SPACE") OR JOY("A",1) OR JOY("RT",1)>0.4
15320         ELSE
15330           L=KEY("LEFT") OR JOY("LEFT",2) OR JOY("LX",2)<-0.4
15340           R=KEY("RIGHT") OR JOY("RIGHT",2) OR JOY("LX",2)>0.4
15350           T=KEY("UP") OR KEY("RETURN") OR JOY("A",2) OR JOY("RT",2)>0.4
15360         END IF
15370       END IF
15380       PD(P)=0
15390       IF L AND NOT R THEN PD(P)=-1
15400       IF R AND NOT L THEN PD(P)=1
15410       IF T THEN FQ(P)=1
15420     END IF
15430   NEXT P
15440   IF FN IN_SERVICE() THEN CALL PAUSE
15450   IF IN_QUIT THEN QT=1
15460 END PROC
15470 PROC PAUSE
15480   CALL TXTAT(36,17,"PAUSED",201)
15490   CALL IN_PAUSELOOP
15500   IF IN_QUIT THEN QT=1
15510   TXT FILL 36,17,41,17,32
15520 END PROC
15530 ' the keys that went down or up (ON KEY): M switches the music on and off
15540 PROC KEYEVENTS
15550   LOCAL E$
15560   REPEAT
15570     E$=KEY$("EVENT")
15580     IF E$="+M" THEN CALL MUTETOGGLE
15590   UNTIL E$=""
15600 END PROC
15610 PROC MUTETOGGLE
15620   MUTE=1-MUTE
15630   IF MUTE=1 THEN
15640     SOUND OFF 3
15650     SOUND OFF 4
15660   END IF
15670 END PROC
15680 ' ====================================================================== the gardeners and their balls
15690 PROC MOVEGARDENERS
15700   LOCAL P
15710   FOR P=0 TO NPL-1
15720     IF ALV(P)=1 THEN CALL MOVEONE(P)
15730   NEXT P
15740 END PROC
15750 PROC MOVEONE(P)
15760   LOCAL W
15770   IF RAP(P)>0 THEN
15780     RAP(P)=RAP(P)-1
15790     IF RAP(P)=0 THEN HF=1
15800   END IF
15810   IF PWT(P)>0 THEN
15820     PWT(P)=PWT(P)-1
15830     IF PWT(P) MOD 60=0 THEN HF=1
15840     IF PWT(P)=0 THEN
15850       IF PWK(P)=3 THEN SPRITE 190+P*2,OFF
15860       PWK(P)=0
15870     END IF
15880   END IF
15890   ' the picture of the gardener: throwing (2) while TP counts down, else walking (1: the animation sequence 4) or standing (0)
15900   IF TP(P)>0 THEN TP(P)=TP(P)-1
15910   W=0
15920   IF PD(P)<>0 THEN W=1
15930   IF TP(P)>0 THEN W=2
15940   IF W<>GS(P) THEN
15950     GS(P)=W
15960     IF W=2 THEN SPRITE 40+P,IMAGE 34
15970     IF W=1 THEN SPRITE 40+P,ANIM 4,4,5
15980     IF W=0 THEN SPRITE 40+P,IMAGE 13
15990   END IF
16000   IF PD(P)<>0 THEN
16010     PX(P)=FN CLAMP(PX(P)+PD(P)*3,4,620)
16020     SPRITE 40+P,PX(P),PY
16030     WK(P)=WK(P)+1
16040     IF WK(P) MOD 12=0 THEN SFX 14
16050   END IF
16060   IF PWK(P)=3 THEN SPRITE 190+P*2,PX(P)-8,PY-14
16070 END PROC
16080 PROC BALLS
16090   LOCAL P,S
16100   FOR P=0 TO NPL-1
16110     IF FQ(P)=1 THEN CALL THROW(P)
16120   NEXT P
16130   FOR S=0 TO 3
16140     IF GA(S)=1 THEN CALL BALLMOVE(S)
16150   NEXT S
16160 END PROC
16170 ' a throw: one ball (two with rapid fire), or two at once while the power-up TWO BALLS lasts
16180 PROC THROW(P)
16190   LOCAL A,K,S0
16200   FQ(P)=0
16210   IF ALV(P)=0 THEN EXIT PROC
16220   S0=2*P
16230   A=GA(S0)+GA(S0+1)
16240   K=0
16250   IF RAP(P)>0 THEN K=1
16260   IF PWK(P)=2 THEN K=2
16270   IF PWK(P)=1 THEN
16280     IF A=0 THEN
16290       CALL LAUNCH(S0,P,-6,K)
16300       CALL LAUNCH(S0+1,P,6,K)
16310     END IF
16320   ELSE
16330     IF A=0 THEN CALL LAUNCH(S0,P,0,K)
16340     IF A=1 AND RAP(P)>0 THEN
16350       IF GA(S0)=0 THEN CALL LAUNCH(S0,P,0,K) ELSE CALL LAUNCH(S0+1,P,0,K)
16360     END IF
16370   END IF
16380 END PROC
16390 PROC LAUNCH(S,P,DXO,K)
16400   GA(S)=1:GX(S)=PX(P)+DXO:GY(S)=PY-8:GK(S)=K
16410   SPRITE 42+S,GX(S),GY(S),17
16420   IF K=1 THEN SPRITE 42+S,IMAGE 18
16430   IF K=2 THEN SPRITE 42+S,IMAGE 53
16440   SPRITE 42+S,ON
16450   TP(P)=8
16460   SFX 1
16470 END PROC
16480 ' the ball S flies up; (TX,TY) is its top: what does it touch?
16490 PROC BALLMOVE(S)
16500   LOCAL TX,TY,P,BX0,BY0
16510   P=S\2
16520   GY(S)=GY(S)-7
16530   IF GY(S)<8 THEN
16540     CALL KILLBALL(S)
16550     EXIT PROC
16560   END IF
16570   SPRITE 42+S,GX(S),GY(S)
16580   TX=GX(S)+7:TY=GY(S)+6
16590   IF POINT(TX,TY)>0 THEN
16600     CALL BUSHHIT(TX,TY)
16610     CALL KILLBALL(S)
16620     EXIT PROC
16630   END IF
16640   IF UA=1 AND TX>=UX AND TX<UX+16 AND TY>=UY+2 AND TY<UY+22 THEN
16650     CALL BIRDHIT(S,P)
16660     EXIT PROC
16670   END IF
16680   IF BAL=1 THEN
16690     BX0=SPRITE("X",140):BY0=SPRITE("Y",140)
16700     IF TX>=BX0 AND TX<BX0+32 AND TY>=BY0+2 AND TY<BY0+44 THEN
16710       CALL BALLOONHIT(S,P)
16720       EXIT PROC
16730     END IF
16740   END IF
16750   IF BOSS>0 THEN
16760     CALL BOSSCHECK(S,P,TX,TY)
16770     EXIT PROC
16780   END IF
16790   HITF=0
16800   CALL FRUITHIT(P,TX,TY)
16810   IF GK(S)=2 THEN
16820     CALL FRUITHIT(P,TX-7,TY)
16830     CALL FRUITHIT(P,TX+7,TY)
16840   END IF
16850   IF HITF=1 THEN CALL KILLBALL(S)
16860 END PROC
16870 PROC KILLBALL(S)
16880   GA(S)=0
16890   SPRITE 42+S,OFF
16900 END PROC
16910 ' did a ball at (TX,TY) hit a fruit of the formation? the rows sway, so the place of a row is ROWX(row) (set by SWAYFRUIT)
16920 PROC FRUITHIT(P,TX,TY)
16930   LOCAL C,RW,RX,RY,FY0
16940   FY0=INT(FYV)
16950   IF TY<FY0 THEN EXIT PROC
16960   RW=(TY-FY0)\SY+1
16970   IF RW<1 OR RW>5 THEN EXIT PROC
16980   RY=TY-FY0-(RW-1)*SY
16990   IF RY<3 OR RY>=21 THEN EXIT PROC
17000   RX=TX-ROWX(RW)
17010   IF RX<0 THEN EXIT PROC
17020   C=RX\SX
17030   RX=RX-C*SX
17040   IF C<=7 AND RX<16 THEN
17050     IF IV(RW,C)>0 THEN CALL SPLAT(P,RW,C)
17060   END IF
17070 END PROC
17080 ' a fruit was hit: a coconut cracks the first time (IV 2 -> 1), every other fruit bursts into juice
17090 PROC SPLAT(P,RW,C)
17100   LOCAL X,Y,PT,N
17110   N=(RW-1)*8+C
17120   X=ROWX(RW)+C*SX+8:Y=INT(FYV)+(RW-1)*SY+12
17130   HITF=1
17140   IF IV(RW,C)=2 THEN
17150     IV(RW,C)=1
17160     SPRITE N,IMAGE FN FIMG(RW,C)
17170     CALL BOOM(X,Y,-1)
17180     SFX 15
17190     CALL SCORED(P,10)
17200     EXIT PROC
17210   END IF
17220   IV(RW,C)=0
17230   NA=NA-1
17240   PT=RP(RW)
17250   IF TK(RW,C)=1 THEN PT=100
17260   SPRITE N,OFF
17270   CALL BOOM(X,Y,RW-1)
17280   SFX 2
17290   CALL SCORED(P,PT)
17300   SP=NA\2+2-WV
17310   IF SP<2 THEN SP=2
17320   ' now and then a banana falls out
17330   IF PU=0 AND FN RN(9)=0 THEN CALL BANANA(X,Y)
17340 END PROC
17350 ' score changed: an extra life for the player every 1500 points, the high score, the score line needs to be drawn again
17360 PROC SCORED(P,PTS)
17370   SCO(P)=SCO(P)+PTS
17380   SC=SC+PTS
17390   LASTP=P
17400   IF SCO(P)>=NX(P) THEN
17410     LV(P)=FN CLAMP(LV(P)+1,0,5)
17420     NX(P)=NX(P)+1500
17430     SFX 5
17440   END IF
17450   IF SC>HI THEN HI=SC
17460   HF=1
17470 END PROC
17480 ' ====================================================================== the bananas and the crates (power-ups that fall)
17490 PROC BANANA(X,Y)
17500   PU=1:PUX=X-8:PUY=Y-12
17510   SPRITE 54,PUX,PUY,22
17520   SPRITE 54,ANIM 3,2,8
17530   SPRITE 54,ON
17540   SFX 10
17550 END PROC
17560 ' a crate with a power-up falls from (X,Y); the kind CRK: 1 two balls, 2 big ball, 3 umbrella
17570 PROC DROPCRATE(X,Y)
17580   IF CR=1 THEN EXIT PROC
17590   CR=1:CRX=X-8:CRY=Y:CRK=1+FN RN(3)
17600   SPRITE 53,CRX,CRY,49+CRK
17610   SPRITE 53,ON
17620   SFX 10
17630 END PROC
17640 PROC POWERUP
17650   LOCAL P
17660   IF PU=1 THEN
17670     PUY=PUY+2
17680     SPRITE 54,PUX,PUY
17690     FOR P=0 TO NPL-1
17700       IF ALV(P)=1 AND PU=1 AND PUY>PY-14 AND PUY<PY+16 AND PUX>PX(P)-12 AND PUX<PX(P)+12 THEN
17710         PU=0
17720         SPRITE 54,OFF
17730         RAP(P)=600
17740         SFX 9
17750         CALL BOOM(PX(P)+8,PY+8,-1)
17760         CALL SCORED(P,100)
17770       END IF
17780     NEXT P
17790     IF PU=1 AND PUY>284 THEN
17800       PU=0
17810       SPRITE 54,OFF
17820     END IF
17830   END IF
17840   IF CR=1 THEN
17850     CRY=CRY+2
17860     CRX=CRX+WIND*0.4
17870     SPRITE 53,INT(CRX),CRY
17880     FOR P=0 TO NPL-1
17890       IF ALV(P)=1 AND CR=1 AND CRY>PY-14 AND CRY<PY+16 AND CRX>PX(P)-12 AND CRX<PX(P)+12 THEN
17900         CR=0
17910         SPRITE 53,OFF
17920         CALL GIVEPOWER(P,CRK)
17930       END IF
17940     NEXT P
17950     IF CR=1 AND CRY>284 THEN
17960       CR=0
17970       SPRITE 53,OFF
17980     END IF
17990   END IF
18000 END PROC
18010 PROC GIVEPOWER(P,K)
18020   IF PWK(P)=3 AND K<>3 THEN SPRITE 190+P*2,OFF
18030   PWK(P)=K:PWT(P)=900
18040   IF K=3 THEN
18050     SPRITE 190+P*2,PX(P)-8,PY-14
18060     SPRITE 190+P*2,ON
18070   END IF
18080   SFX 9
18090   CALL BOOM(PX(P)+8,PY+8,-1)
18100   CALL SCORED(P,50)
18110   HF=1
18120 END PROC
18130 ' ====================================================================== the seeds the fruit spits
18140 PROC SEEDS
18150   LOCAL I,N,MAXS
18160   FOR I=0 TO 5
18170     IF SEEDON(I)=1 THEN CALL SEEDMOVE(I)
18180   NEXT I
18190   IF BOSS=0 THEN
18200     MAXS=FN CLAMP(2+WV\2,3,6)
18210     N=SEEDON(0)+SEEDON(1)+SEEDON(2)+SEEDON(3)+SEEDON(4)+SEEDON(5)
18220     IF N<MAXS AND FN RN(BR)=0 THEN CALL SPAWN
18230   END IF
18240 END PROC
18250 PROC SEEDMOVE(I)
18260   LOCAL TX,TY,J,P
18270   ' the wind carries the seed sideways
18280   SEEDX(I)=FN CLAMP(INT(SEEDX(I)+WIND*0.35),0,624)
18290   SEEDY(I)=SEEDY(I)+BS
18300   IF SEEDY(I)>268 THEN
18320     CALL KILLSEED(I)
18330     EXIT PROC
18340   END IF
18350   SPRITE 46+I,INT(SEEDX(I)),INT(SEEDY(I))
18360   TX=INT(SEEDX(I))+7:TY=INT(SEEDY(I))+15
18370   IF TY<246 AND POINT(TX,TY)>0 THEN
18380     CALL BUSHHIT(TX,TY)
18390     CALL KILLSEED(I)
18400     EXIT PROC
18410   END IF
18420   FOR P=0 TO NPL-1
18430     IF ALV(P)=1 THEN
18440       IF PWK(P)=3 AND TY>=PY-8 AND TY<=PY AND TX>=PX(P)-8 AND TX<=PX(P)+24 THEN
18450         SFX 16
18460         CALL BOOM(TX,TY-4,-1)
18470         CALL KILLSEED(I)
18480         EXIT PROC
18490       END IF
18500       IF TY>=PY+4 AND TX>=PX(P)+3 AND TX<=PX(P)+12 THEN
18510         CALL GARDENERHIT(P)
18520         EXIT PROC
18530       END IF
18540     END IF
18550   NEXT P
18560   ' a ball that meets a seed destroys it
18570   FOR J=0 TO 3
18580     IF GA(J)=1 AND TX>=GX(J)+4 AND TX<=GX(J)+11 AND TY>=GY(J)+6 AND TY<=GY(J)+18 THEN
18590       CALL KILLSEED(I)
18600       CALL KILLBALL(J)
18610       CALL BOOM(TX,TY,-1)
18620       SFX 6
18630       EXIT PROC
18640     END IF
18650   NEXT J
18660 END PROC
18720 PROC KILLSEED(I)
18730   SEEDON(I)=0
18740   SPRITE 46+I,OFF
18750 END PROC
18760 ' a seed from (X,Y), if one of the six is free
18770 PROC SPAWNSEED(X,Y)
18780   LOCAL J,Z
18790   J=-1
18800   FOR Z=0 TO 5
18810     IF SEEDON(Z)=0 THEN J=Z
18820   NEXT Z
18830   IF J<0 THEN EXIT PROC
18840   SEEDON(J)=1:SEEDX(J)=X:SEEDY(J)=Y
18850   SPRITE 46+J,X,Y,19
18860   SPRITE 46+J,ANIM 2,4,4
18870   SPRITE 46+J,ON
18880   SFX 7
18890 END PROC
18900 ' a new seed from the lowest fruit of a random column that still has one
18910 PROC SPAWN
18920   LOCAL Z,C,RW
18930   C=FN RN(8)
18940   FOR Z=1 TO 8
18950     RW=5
18960     WHILE IV(RW,C)=0 AND RW>0
18970       RW=RW-1
18980     WEND
18990     IF RW>0 THEN
19000       CALL SPAWNSEED(ROWX(RW)+C*SX,INT(FYV)+(RW-1)*SY+12)
19010       EXIT PROC
19020     END IF
19030     C=C+1
19040     IF C>7 THEN C=0
19050   NEXT Z
19060 END PROC
19070 ' a hit on a bush at (TX,TY): a round hole and a few leaves that fall
19080 PROC BUSHHIT(TX,TY)
19090   LOCAL Z,XX,YY
19100   CALL DISK(TX,TY,4,0)
19110   FOR Z=1 TO 6
19120     XX=TX-7+FN RN(14)
19130     YY=TY-7+FN RN(14)
19140     GFX PSET XX,YY,0
19150   NEXT Z
19160   CALL LEAVES(TX,TY)
19170   SFX 6
19180 END PROC
19190 ' ====================================================================== the bird
19200 PROC BIRD
19210   IF UA=1 THEN
19220     UX=UX+UD*2
19230     UY=14+INT(5*SIN(UX/22))
19240     SPRITE 52,UX,UY
19250     IF UX<-20 OR UX>640 THEN CALL BIRDGONE
19260     EXIT PROC
19270   END IF
19280   IF BIRDREADY=1 AND NA>6 AND BOSS=0 THEN CALL BIRDSTART
19290 END PROC
19300 PROC BIRDSTART
19310   BIRDREADY=0
19320   UA=1
19330   IF FN RN(2)=0 THEN
19340     UX=-16:UD=1
19350   ELSE
19360     UX=640:UD=-1
19370   END IF
19380   SPRITE 52,UX,UY,10
19390   SPRITE 52,ANIM 1,4,2
19400   SPRITE 52,ON
19410   WAVE 2,0:VIBRATO 2,14,6:SOUND 2,1800,-1,0.06
19420 END PROC
19430 PROC BIRDGONE
19440   UA=0
19450   SPRITE 52,OFF
19460   SOUND OFF 2
19470   AFTER 700+FN RN(600),2 GOSUB BIRDTIMER
19480 END PROC
19490 PROC BIRDHIT(S,P)
19500   LOCAL V
19510   V=SV(FN RN(4))
19520   CALL BOOM(UX+8,UY+12,4)
19530   CALL BOOM(UX+8,UY+12,-1)
19540   CALL KILLBALL(S)
19550   UA=0
19560   SPRITE 52,OFF
19570   SOUND OFF 2
19580   SFX 4
19590   AFTER 700+FN RN(600),2 GOSUB BIRDTIMER
19600   CALL SCORED(P,V)
19610 END PROC
19620 ' ====================================================================== the balloon: a sprite group 2 x 2 that flies on a path with events
19630 ' the path has two E commands: event 1 in the middle of the sky (a crate falls out of the basket), event 2 at the end (the balloon is gone); ON SPRITE runs the handler
19640 PROC BALLOONINIT
19650   SPRITE 140,0,0,56:SPRITE 141,0,0,57:SPRITE 142,0,0,58:SPRITE 143,0,0,59
19660   SPRITE 140,SIZE 2,2
19670   SPRITE 140,OFF
19680   SPRITE 170,0,0,78:SPRITE 171,0,0,79:SPRITE 172,0,0,80:SPRITE 173,0,0,81
19690   SPRITE 170,SIZE 2,2
19700   SPRITE 170,OFF
19710   SPRITE 174,0,0,78:SPRITE 175,0,0,79:SPRITE 176,0,0,80:SPRITE 177,0,0,81
19720   SPRITE 174,SIZE 2,2
19730   SPRITE 174,OFF
19740   SPRITE 160,0,0,70:SPRITE 161,0,0,71:SPRITE 162,0,0,72:SPRITE 163,0,0,73
19750   SPRITE 164,0,0,74:SPRITE 165,0,0,75:SPRITE 166,0,0,76:SPRITE 167,0,0,77
19760   SPRITE 160,SIZE 4,2
19770   SPRITE 160,OFF
19780   SPRITE 190,0,0,54:SPRITE 191,0,0,55:SPRITE 190,SIZE 2,1:SPRITE 190,PRIORITY 4:SPRITE 190,OFF
19790   SPRITE 192,0,0,54:SPRITE 193,0,0,55:SPRITE 192,SIZE 2,1:SPRITE 192,PRIORITY 4:SPRITE 192,OFF
19800 END PROC
19810 PROC BALLOON
19820   IF BAL=0 AND BALREADY=1 AND NA>6 AND BOSS=0 THEN CALL BALLOONSTART
19830 END PROC
19840 PROC BALLOONSTART
19850   LOCAL Y0,P$
19860   BALREADY=0
19870   BAL=1
19880   Y0=40+FN RN(40)
19890   IF FN RN(2)=0 THEN
19900     P$="X=-40 Y="+STR$(Y0)+" T 30 [ M 90,30 M 90,-30 ]2 E 1 [ M 90,30 M 90,-30 ]2 E 2"
19910   ELSE
19920     P$="X=640 Y="+STR$(Y0)+" T 30 [ M -90,30 M -90,-30 ]2 E 1 [ M -90,30 M -90,-30 ]2 E 2"
19930   END IF
19940   SPRITE 140,PATH P$
19950   SPRITE 140,PRIORITY 3
19960   SPRITE 140,ON
19970   SFX 8
19980 END PROC
19990 PROC BALLOONGONE
20000   BAL=0
20010   SPRITE 140,PATH ""
20020   SPRITE 140,OFF
20030   AFTER 900+FN RN(600),3 GOSUB BALTIMER
20040 END PROC
20050 PROC BALLOONHIT(S,P)
20060   LOCAL X,Y
20070   X=SPRITE("X",140):Y=SPRITE("Y",140)
20080   CALL KILLBALL(S)
20090   CALL BOOM(X+16,Y+16,-1)
20100   CALL BOOM(X+16,Y+24,3)
20110   SFX 4
20120   CALL DROPCRATE(X+16,Y+40)
20130   CALL BALLOONGONE
20140   CALL SCORED(P,200)
20150 END PROC
20160 ' the events of the paths (ON SPRITE)
20170 PROC SPRITEEVENTS
20180   LOCAL E
20190   REPEAT
20200     E=SPRITE("EVENT")
20210     IF E=1 AND BAL=1 THEN CALL DROPCRATE(SPRITE("X",140)+16,SPRITE("Y",140)+40)
20220     IF E=2 AND BAL=1 THEN CALL BALLOONGONE
20230   UNTIL E<0
20240 END PROC
20250 ' ====================================================================== the formation: it sways on its branches
20260 PROC FORM
20270   ST=ST+1
20280   IF ST<SP THEN EXIT PROC
20290   ST=0
20300   CALL MARCH
20310 END PROC
20320 PROC MARCH
20330   LOCAL C,RW,MN,MX,LR,N
20340   MN=8:MX=-1:LR=0
20350   FOR C=0 TO 7
20360     FOR RW=1 TO 5
20370       IF IV(RW,C)>0 THEN
20380         IF MN=8 THEN MN=C
20390         MX=C
20400         IF RW>LR THEN LR=RW
20410       END IF
20420     NEXT RW
20430   NEXT C
20440   IF (DX=1 AND FX+MX*SX+16+SD>622) OR (DX=-1 AND FX+MN*SX-SD<16) THEN
20450     DX=0-DX:FY=FY+10
20460   ELSE
20470     FX=FX+DX*SD
20480   END IF
20490   FA=1-FA
20500   ' the rustle of the leaves (the formation steps faster when fewer fruits are left)
20510   SFX 12+FA
20520   FOR RW=1 TO 5
20530     FOR C=0 TO 7
20540       IF IV(RW,C)>0 THEN SPRITE (RW-1)*8+C,IMAGE FN FIMG(RW,C)
20550     NEXT C
20560   NEXT RW
20570   IF FY+(LR-1)*SY+20>=SHY-2 THEN
20580     OV=1:LANDED=1
20590   END IF
20600 END PROC
20610 ' every frame: the drawn formation glides to the place of the formation, every row sways with a phase of its own, every column bobs a little
20620 PROC SWAYFRUIT
20630   LOCAL RW,C,SO
20640   SWA=SWA+0.045
20650   FXV=FXV+(FX-FXV)*0.25
20660   FYV=FYV+(FY-FYV)*0.07
20670   FOR RW=1 TO 5
20680     SO=INT(5*SIN(SWA+RW*0.7))
20690     ROWX(RW)=INT(FXV)+SO
20700     FOR C=0 TO 7
20710       IF IV(RW,C)>0 THEN SPRITE (RW-1)*8+C,ROWX(RW)+C*SX,INT(FYV)+(RW-1)*SY+INT(2*SIN(SWA*1.7+C))
20720     NEXT C
20730   NEXT RW
20740 END PROC
20750 ' ====================================================================== the wind
20760 PROC WINDMOVE
20770   LOCAL MW,I
20780   WCT=WCT-1
20790   IF WCT<=0 THEN
20800     MW=FN CLAMP(0.6+WV*0.3,0,2)
20810     WTGT=(FN RN(5)-2)/2*MW
20820     WCT=200+FN RN(300)
20830   END IF
20840   WIND=WIND+(WTGT-WIND)*0.01
20850   I=45+INT(WIND+2.5)
20860   IF I<>WSI THEN
20870     WSI=I
20880     SPRITE 63,IMAGE I
20890   END IF
20900 END PROC
20910 ' ====================================================================== the melon: a boss every fifth wave, it splits in two when it is half beaten
20920 PROC BOSSSTART
20930   BOSS=1
20940   BHM=10+WV*2:BHP=BHM:BARM=BHM:BFL=0
20950   SPRITE 160,8,40
20960   SPRITE 160,PATH "[ M 4,1*35 M 4,-1*35 M 4,1*35 M 4,-1*35 M -4,1*35 M -4,-1*35 M -4,1*35 M -4,-1*35 ]100"
20970   SPRITE 160,PRIORITY 3
20980   SPRITE 160,ON
20990   CALL BOSSBAR
21000 END PROC
21010 ' every frame: the white flash of a hit goes out, now and then a seed falls from the melon
21020 PROC BOSSMOVE
21030   IF BOSS=0 THEN EXIT PROC
21040   IF BFL>0 THEN
21050     BFL=BFL-1
21060     IF BFL=0 THEN
21070       GFX PALETTE 4,198,RGB(225,40,60):GFX PALETTE 4,199,RGB(255,110,120)
21080     END IF
21090   END IF
21100   IF FN RN(35)=0 THEN CALL BOSSSEED
21110 END PROC
21120 PROC BOSSSEED
21130   LOCAL H
21140   IF BOSS=1 THEN
21150     CALL SPAWNSEED(SPRITE("X",160)+8+FN RN(40),SPRITE("Y",160)+40)
21160   ELSE
21170     H=FN RN(2)
21180     IF B2P(H)<=0 THEN H=1-H
21190     IF B2P(H)>0 THEN CALL SPAWNSEED(SPRITE("X",170+H*4)+4+FN RN(20),SPRITE("Y",170+H*4)+34)
21200   END IF
21210 END PROC
21220 ' a ball may have hit the melon (BOSS 1: the big one, 2: one of the halves)
21230 PROC BOSSCHECK(S,P,TX,TY)
21240   LOCAL H,X0,Y0
21250   IF BOSS=1 THEN
21260     X0=SPRITE("X",160):Y0=SPRITE("Y",160)
21270     IF TX>=X0 AND TX<X0+64 AND TY>=Y0+3 AND TY<Y0+45 THEN CALL BOSSHIT(S,P,TX,TY,0)
21280     EXIT PROC
21290   END IF
21300   FOR H=0 TO 1
21310     IF B2P(H)>0 THEN
21320       X0=SPRITE("X",170+H*4):Y0=SPRITE("Y",170+H*4)
21330       IF TX>=X0 AND TX<X0+32 AND TY>=Y0+8 AND TY<Y0+40 THEN
21340         CALL BOSSHIT(S,P,TX,TY,H+1)
21350         EXIT PROC
21360       END IF
21370     END IF
21380   NEXT H
21390 END PROC
21400 PROC BOSSHIT(S,P,TX,TY,H)
21410   CALL KILLBALL(S)
21420   CALL BOOM(TX,TY,-1)
21430   SFX 6
21440   CALL SCORED(P,10)
21450   BFL=3
21460   GFX PALETTE 4,198,RGB(255,255,255):GFX PALETTE 4,199,RGB(255,255,255)
21470   IF H=0 THEN
21480     BHP=BHP-1
21490     IF BHP<=BHM\2 THEN
21500       CALL BOSSSPLIT
21510       EXIT PROC
21520     END IF
21530   ELSE
21540     B2P(H-1)=B2P(H-1)-1
21550     BHP=B2P(0)+B2P(1)
21560     IF B2P(H-1)<=0 THEN
21570       CALL BOOM(SPRITE("X",166+H*4)+16,SPRITE("Y",166+H*4)+24,2)
21580       SPRITE 166+H*4,OFF
21590       SPRITE 166+H*4,PATH ""
21600       SFX 4
21610     END IF
21620   END IF
21630   CALL BOSSBAR
21640   IF BOSS=2 AND B2P(0)<=0 AND B2P(1)<=0 THEN CALL BOSSDEAD
21650 END PROC
21660 ' the melon splits in two: each half is a sprite group of 2 x 2 on a path of its own
21670 PROC BOSSSPLIT
21680   LOCAL X,Y,Z
21690   X=SPRITE("X",160):Y=SPRITE("Y",160)
21700   BOSS=2
21710   B2P(0)=BHM\4+1:B2P(1)=BHM\4+1
21720   BHP=B2P(0)+B2P(1):BARM=BHP
21730   FOR Z=1 TO 3
21740     CALL BOOM(X+8+FN RN(48),Y+8+FN RN(32),FN RN(5))
21750   NEXT Z
21760   SFX 3
21770   SPRITE 160,PATH ""
21780   SPRITE 160,OFF
21790   X=FN CLAMP(X,170,450)
21800   SPRITE 170,X-40,Y
21810   SPRITE 170,PATH "[ M 3,1*20 M 3,-1*20 M -3,1*20 M -3,-1*20 M -3,1*20 M -3,-1*20 M 3,1*20 M 3,-1*20 ]9999"
21820   SPRITE 170,PRIORITY 3
21830   SPRITE 170,ON
21840   SPRITE 174,X+40,Y
21850   SPRITE 174,PATH "[ M -3,1*20 M -3,-1*20 M 3,1*20 M 3,-1*20 M 3,1*20 M 3,-1*20 M -3,1*20 M -3,-1*20 ]9999"
21860   SPRITE 174,PRIORITY 3
21870   SPRITE 174,ON
21880   CALL BOSSBAR
21890 END PROC
21900 ' the health bar in the third text row: spaces on a red paper
21910 PROC BOSSBAR
21920   LOCAL N
21930   TXT FILL 24,2,56,2,32,0,0
21940   N=INT(30*BHP/BARM)
21950   IF N<0 THEN N=0
21960   IF N>0 THEN TXT FILL 25,2,24+N,2,32,0,203
21970 END PROC
21980 PROC BOSSDEAD
21990   LOCAL I
22000   BOSS=0
22010   SPRITE 160,PATH "":SPRITE 160,OFF
22020   SPRITE 170,PATH "":SPRITE 170,OFF
22030   SPRITE 174,PATH "":SPRITE 174,OFF
22040   GFX PALETTE 4,198,RGB(225,40,60):GFX PALETTE 4,199,RGB(255,110,120)
22050   CALL SCORED(LASTP,1000)
22060   FOR I=0 TO 5
22070     CALL BOOM(100+FN RN(440),50+FN RN(60),FN RN(5))
22080     SFX 4
22090     CALL WAITFRAMES(6)
22100   NEXT I
22110   TXT FILL 24,2,56,2,32,0,0
22120   NA=0
22130 END PROC
22140 ' ====================================================================== the gardener was hit
22150 ' his hat flies off, the sky flashes, the clouds shake; the player is out of the game when the lives are gone
22160 PROC GARDENERHIT(P)
22170   LOCAL Z,HX,S
22180   LV(P)=LV(P)-1
22190   SFX 3
22200   FOR S=0 TO 3
22210     CALL KILLBALL(S)
22220   NEXT S
22230   FOR Z=0 TO 5
22240     CALL KILLSEED(Z)
22250   NEXT Z
22260   PWK(P)=0:PWT(P)=0:RAP(P)=0
22270   SPRITE 190+P*2,OFF
22280   SPRITE 40+P,IMAGE 36
22290   HX=FN CLAMP(PX(P)+FN RN(80)-40,0,620)
22300   SPRITE 55+P,PX(P),PY-5,35:SPRITE 55+P,ON
22310   SPRITE 55+P,MOVETO HX,PY-75,24
22320   CALL BOOM(PX(P)+8,PY+6,-1)
22330   CALL SKYFLASH(1)
22340   FOR Z=1 TO 70
22350     IF Z=6 THEN CALL SKYFLASH(0)
22360     IF Z=26 THEN SPRITE 55+P,MOVETO HX,278,26
22370     TILE SCROLL FN CLAMP(CSX+FN RN(5)-2,0,15),FN RN(4)
22380     IF Z=30 THEN CALL BOOM(PX(P)+8,PY+6,-1)
22390     CALL EFFECTS
22400     GFX VSYNC
22410   NEXT Z
22420   SPRITE 55+P,OFF
22430   TILE SCROLL CSX,0
22440   GS(P)=0:TP(P)=0:PD(P)=0
22450   IF LV(P)<=0 THEN
22460     ALV(P)=0
22470     SPRITE 40+P,OFF
22480     IF ALV(0)+ALV(1)=0 THEN OV=1
22490   ELSE
22500     PX(P)=FN STARTX(P)
22510     SPRITE 40+P,PX(P),PY,13:SPRITE 40+P,ON
22520   END IF
22530   HF=1
22540 END PROC
22550 PROC LEAVES(X,Y)
22560   LOCAL I,A,D,S
22570   FOR I=0 TO 3
22580     S=NP
22590     NP=(NP+1) MOD 64
22600     A=3.14+FN RN(314)/100
22610     D=8+FN RN(14)
22620     SPRITE 64+S,X-8,Y-12,40
22630     SPRITE 64+S,ON
22640     SPRITE 64+S,MOVETO INT(X-8+COS(A)*D),INT(Y-12+SIN(A)*D+16),24
22650     PL(S)=28
22660   NEXT I
22670 END PROC
22680 PROC BOOM(X,Y,K)
22690   LOCAL I,A,D,S,E,M
22700   E=-1
22710   FOR I=0 TO 3
22720     IF EP(I)=0 THEN
22730       E=I
22740       EXIT
22750     END IF
22760   NEXT I
22770   IF E>=0 THEN
22780     EP(E)=14
22790     SPRITE 59+E,X-8,Y-12,24
22800     SPRITE 59+E,ANIM 5,4,3
22810     SPRITE 59+E,ON
22820   END IF
22830   FOR I=0 TO 9
22840     S=NP
22850     NP=(NP+1) MOD 64
22860     A=I*0.628+FN RN(100)/160
22870     D=14+FN RN(26)
22880     M=28+K
22890     IF K<0 THEN M=33
22900     SPRITE 64+S,X-8,Y-12,M
22910     SPRITE 64+S,ON
22920     SPRITE 64+S,MOVETO INT(X-8+COS(A)*D),INT(Y-12+SIN(A)*D),18
22930     PL(S)=26
22940   NEXT I
22950 END PROC
22960 PROC EFFECTS
22970   LOCAL K,V
22980   FOR EACH K,V IN EP()
22990     IF V=0 THEN ITERATE
23000     EP(K)=V-1
23010     IF V=1 THEN SPRITE 59+K,OFF
23020   NEXT
23030   FOR EACH K,V IN PL()
23040     IF V=0 THEN ITERATE
23050     PL(K)=V-1
23060     IF V=1 THEN SPRITE 64+K,OFF
23070   NEXT
23080 END PROC
23090 ' ====================================================================== waves
23100 PROC NEWWAVE
23110   LOCAL RW,C,Q,N,I,P
23120   PHP=PH
23130   PH=WV MOD 6
23140   BOSS=0
23150   IF WV MOD 5=0 THEN BOSS=1
23160   FOR RW=1 TO 5
23170     FOR C=0 TO 7
23180       IV(RW,C)=1-BOSS
23190       TK(RW,C)=0
23200       IF BOSS=0 AND ((RW=1 AND WV>=2) OR (RW=2 AND WV>=4)) THEN
23210         TK(RW,C)=1
23220         IV(RW,C)=2
23230       END IF
23240     NEXT C
23250   NEXT RW
23260   NA=FN COUNTALIVE(IV())
23270   IF BOSS=1 THEN NA=1
23280   Q=FN CLAMP(WV-1,0,4)
23290   FX=40:FY=34+Q*8:DX=1:SD=6
23300   FXV=FX:FYV=FY-190
23310   SP=FN CLAMP(22-WV,10,22)
23320   ST=0:FA=0
23330   BS=FN CLAMP(2+WV\2,2,5)
23340   BR=FN CLAMP(50-WV*5,14,50)
23350   GFX CLEAR
23360   CALL GROUND
23370   CALL BUSHES
23380   CALL BACKGROUND(PH)
23390   IF PH=5 THEN CALL NIGHTON ELSE CALL NIGHTOFF
23400   FOR I=0 TO 5
23410     CALL KILLSEED(I)
23420   NEXT I
23430   FOR I=0 TO 3
23440     CALL KILLBALL(I)
23450   NEXT I
23460   UA=0:BIRDREADY=0:SPRITE 52,OFF:SOUND OFF 2
23470   BAL=0:BALREADY=0:SPRITE 140,PATH "":SPRITE 140,OFF
23480   PU=0:SPRITE 54,OFF
23490   CR=0:SPRITE 53,OFF
23500   AFTER 500+FN RN(600),2 GOSUB BIRDTIMER
23510   AFTER 900+FN RN(600),3 GOSUB BALTIMER
23520   WIND=0:WTGT=0:WCT=100:WSI=-1
23530   SPRITE 63,596,PY,47:SPRITE 63,ON
23540   ' the fruit fly in from above (SWAYFRUIT glides them to their places)
23550   FOR RW=1 TO 5
23560     FOR C=0 TO 7
23570       N=(RW-1)*8+C
23580       IF IV(RW,C)>0 THEN
23590         SPRITE N,FX+C*SX,FY+(RW-1)*SY-190,FN FIMG(RW,C)
23600         SPRITE N,ON
23610       ELSE
23620         SPRITE N,OFF
23630       END IF
23640     NEXT C
23650   NEXT RW
23660   FOR P=0 TO NPL-1
23670     IF ALV(P)=1 THEN
23680       PX(P)=FN STARTX(P):PD(P)=0:FQ(P)=0:GS(P)=0:TP(P)=0
23690       SPRITE 40+P,PX(P),PY,13:SPRITE 40+P,ON
23700       IF PWK(P)=3 THEN
23710         SPRITE 190+P*2,PX(P)-8,PY-14
23720         SPRITE 190+P*2,ON
23730       END IF
23740     END IF
23750   NEXT P
23760   CALL HUD
23770   SFX 11
23780   IF BOSS=1 THEN
23790     CALL BIGTEXT(120,"BOSS WAVE "+STR$(WV),12,5)
23800   ELSE
23810     CALL BIGTEXT(120,"WAVE "+STR$(WV),12,5)
23820   END IF
23830   CALL FADEIN
23840   GFX LINE 60,112,580,190,0,BF
23850   CALL STARTMUSIC
23860   IF BOSS=1 THEN CALL BOSSSTART
23870 END PROC
23880 ' the colors go from the time of day of the last wave to the one of this wave, while the fruit flies in
23890 PROC FADEIN
23900   LOCAL Z
23910   FOR Z=1 TO 80
23920     IF Z MOD 4=0 AND Z<=68 THEN CALL TODAPPLY(PHP,PH,Z/68)
23930     GFX VSYNC
23940     CALL CLOUDS
23950     CALL SWAYFRUIT
23960     CALL EFFECTS
23970     CALL AMBIENT
23980   NEXT Z
23990   CALL TODAPPLY(PHP,PH,1)
24000 END PROC
24010 PROC WAITFRAMES(N)
24020   LOCAL Z
24030   FOR Z=1 TO N
24040     GFX VSYNC
24050     CALL CLOUDS
24060     CALL EFFECTS
24070     CALL AMBIENT
24080   NEXT Z
24090 END PROC
24100 PROC WAVEDONE
24110   LOCAL P
24120   EVERY OFF
24130   SOUND OFF
24140   PLAY "T200 W1 V0.3 O5 C8 E8 G8 >C4","T200 W2 V0.25 O4 C8 E8 G8 >C4","T200 W1 V0.15 O3 C4 G4"
24150   FOR P=0 TO NPL-1
24160     IF ALV(P)=1 THEN CALL SCORED(P,100)
24170   NEXT P
24180   CALL BIGTEXT(110,"WAVE CLEARED",13,4)
24190   CALL BIGTEXT(160,"BONUS 100",12,3)
24200   CALL WAITFRAMES(100)
24210   GFX LINE 60,100,580,200,0,BF
24220   WV=WV+1
24230   CALL NEWWAVE
24240 END PROC
24250 ' ====================================================================== the music: two voices on the channels 3 and 4, a bar after the other
24260 ' the notes of the array NT(), N of them, one after the other on channel CH, BT beats each, volume VL (PROC PLAYSEQ of the library util)
24270 ' one bar (4 beats): an arpeggio of the chord on channel 3 and two half notes of bass on channel 4 (M switches the music off)
24280 PROC BAR
24290   LOCAL I,TA(8)
24300   IF MUTE=1 THEN EXIT PROC
24310   FOR I=0 TO 7
24320     TA(I)=CH3(BI*3+PAT(I))
24330   NEXT I
24340   CALL PLAYSEQ(TA(),8,3,0.5,0.07)
24350   SOUND 4,BASSF(BI),2,0.2
24360   SOUND 4,BASSF(BI),2,0.2
24370   BI=(BI+1) MOD 4
24380 END PROC
24390 ' starts the music for the wave: the tempo goes up with the wave, the bars are timed by EVERY (a little longer than a bar)
24400 PROC STARTMUSIC
24410   LOCAL TB
24420   TB=FN CLAMP(112+WV*6,112,176)
24430   TEMPO TB
24440   WAVE 3,1:WAVE 4,2
24450   BI=0
24460   CALL BAR
24470   EVERY INT(14400/TB)+2,1 GOSUB BARTIMER
24480 END PROC
24490 ' ====================================================================== title and game over
24500 PROC TITLE
24510   LOCAL I,T,K$,F
24520   CALL CLEARALL
24530   PH=1:PHP=1
24540   CALL BACKGROUND(1)
24550   CALL NIGHTOFF
24560   CALL TODAPPLY(1,1,1)
24570   CALL GROUND
24580   CALL BIGTEXT(14,"ORCHARD",10,8)
24590   CALL BIGTEXT(112,"DEFENSE III",12,4)
24600   FOR I=0 TO 6
24610     SPRITE I,ON
24620   NEXT I
24630   FOR I=0 TO 6
24640     CALL TXTAT(6+I*11,17,MID$("50  40  30  20  10  100 50+ ",I*4+1,4),205)
24650   NEXT I
24660   CALL TXTAT(18,18,"HIGH SCORE "+FN PAD$(HI,6),201)
24670   CALL TXTAT(27,20,"Press SPACE or A to start",203)
24680   CALL TXTAT(6,21,"Arrows or A / D walk, SPACE / UP / W throw   P pause  Q quit  M music",200)
24690   CALL TXTAT(6,22,"Two players: P1 A / D + W (pad 1), P2 LEFT / RIGHT + UP (pad 2)",206)
24700   CALL TXTAT(6,23,"Bananas: rapid fire. Balloon crates: two balls, big ball, umbrella",202)
24710   PLAY "T150 W1 V0.3 O4 E8 G8 >C8 E4 D8 C8 <B8 >C4","T150 W2 V0.25 O3 C4 G4 C4 G4","T150 W1 V0.12 O5 C16 E16 G16 >C16 C16 E16 G16 >C16"
24720   QT=0
24730   CALL IN_WAITRELEASE
24740   REPEAT
24750     GFX VSYNC
24760     T=T+1
24770     IF T MOD 20=0 THEN F=1-F
24780     ' the title shimmers through the colors, the fruit bob up and down and dance
24790     GFX PALETTE 3,10,RGB(INT(180+75*SIN(T/10)),INT(180+75*SIN(T/10+2.1)),INT(180+75*SIN(T/10+4.2)))
24800     FOR I=0 TO 4
24810       SPRITE I,44+I*88,166+INT(7*SIN(T/9+I)),IMG(I+1)+F
24820     NEXT I
24830     SPRITE 5,44+5*88,166+INT(7*SIN(T/9+5)),41+F
24840     SPRITE 6,44+6*88,166+INT(7*SIN(T/9+6)),10+F
24850     CALL CLOUDS
24860     CALL AMBIENT
24870     K$=INKEY$
24880     IF K$="1" THEN NPL=1
24890     IF K$="2" THEN NPL=2
24900     CALL TXTAT(20,19,"Players: "+STR$(NPL)+"   (key 1 or 2)",204)
24910   UNTIL K$=" " OR K$="q" OR K$="Q" OR JOY("A") OR JOY("START") OR JOY("BACK") OR JOY("A",2)
24920   IF K$="q" OR K$="Q" OR JOY("BACK") THEN QT=1
24930   IF JOY("A",2) THEN NPL=2
24940 END PROC
24950 PROC PLAYGAME
24960   LOCAL P
24970   SC=0:WV=1:OV=0:LANDED=0:QT=0:IN_QUIT=0
24980   BI=0:NP=0:PH=1:LASTP=0
24990   FOR P=0 TO 1
25000     SCO(P)=0:LV(P)=3:NX(P)=1500:ALV(P)=0:PWK(P)=0:PWT(P)=0:RAP(P)=0:FQ(P)=0:PD(P)=0:TP(P)=0:GS(P)=0:WK(P)=0
25010     IF P<NPL THEN ALV(P)=1
25020   NEXT P
25030   CALL CLEARALL
25040   CALL NEWWAVE
25050   WHILE OV=0
25060     CALL KEYS
25070     IF QT=1 THEN OV=1
25080     CALL MOVEGARDENERS
25090     CALL BALLS
25100     CALL SEEDS
25110     CALL BIRD
25120     CALL BALLOON
25130     CALL POWERUP
25140     IF BOSS=0 THEN
25150       CALL FORM
25160       CALL SWAYFRUIT
25170     ELSE
25180       CALL BOSSMOVE
25190     END IF
25200     CALL EFFECTS
25210     CALL CLOUDS
25220     CALL WINDMOVE
25230     CALL AMBIENT
25240     IF HF=1 THEN CALL HUD
25250     IF NA=0 AND OV=0 THEN CALL WAVEDONE
25260     GFX VSYNC
25270   WEND
25280   EVERY OFF
25290   AFTER OFF
25300   CALL NIGHTOFF
25310   CALL OVER
25320 END PROC
25330 PROC OVER
25340   LOCAL I,Z,K$
25350   SOUND OFF
25360   FOR I=0 TO 127
25370     SPRITE I,OFF
25380   NEXT I
25390   SPRITE 140,OFF:SPRITE 160,OFF:SPRITE 170,OFF:SPRITE 174,OFF:SPRITE 190,OFF:SPRITE 192,OFF
25400   FOR I=150 TO 159
25410     SPRITE I,OFF
25420   NEXT I
25430   TXT FILL 24,2,56,2,32,0,0
25440   CALL BIGTEXT(100,"GAME OVER",12,5)
25450   IF LANDED=1 THEN CALL TXTAT(29,16,"The fruit has landed!",207)
25460   IF NPL=1 THEN
25470     CALL TXTAT(31,18,"Score "+STR$(SC),200)
25480   ELSE
25490     CALL TXTAT(24,18,"Player 1 "+STR$(SCO(0))+"   Player 2 "+STR$(SCO(1)),200)
25500   END IF
25510   IF SC>=HI AND SC>0 THEN CALL TXTAT(29,20,"A NEW HIGH SCORE!",201)
25520   PLAY "T100 W2 V0.5 O4 C4 <B4 A4 G2","T100 W1 V0.2 O3 C4 G4 E4 C2"
25530   FOR Z=1 TO 170
25540     GFX VSYNC
25550     CALL CLOUDS
25560     CALL EFFECTS
25570     K$=INKEY$
25580   NEXT Z
25590 END PROC
25600 LABEL FRUITDATA
25610 DATA 16,0,50, 32,2,40, 48,4,30, 64,6,20, 80,8,10
25620 DATA 110,50, 87,100, 131,150, 98,300
25630 ' the chords (Am, F, C, G) as three notes each, and the order of an arpeggio
25640 DATA 220,262,330, 175,220,262, 262,330,392, 196,247,294
25650 DATA 0,1,2,1,0,1,2,1
25660 LABEL PALDATA
25670 DATA 16,60,0,15,150,10,30,215,25,45,245,70,80,255,160,160,40,90,20,70,150,40,120,200,70,255,255,255,20,20,40,255,120,140,255,230,230
25680 DATA 32,70,0,20,170,15,40,230,40,55,255,90,90,255,175,175,30,100,30,60,160,50,130,210,80,255,255,255,20,20,40,255,150,150,255,235,120
25690 DATA 48,20,60,15,50,130,40,90,190,50,150,225,80,225,255,170,90,55,25,50,140,40,110,200,70,255,255,255,20,30,20,240,120,120,200,255,120
25700 DATA 64,110,45,0,210,100,0,255,150,10,255,190,60,255,235,170,30,90,20,60,150,40,110,200,60,255,255,255,40,20,0,255,110,90,190,80,0
25710 DATA 80,110,90,0,210,180,0,255,230,20,255,245,100,255,255,200,30,100,30,60,160,50,130,210,80,255,255,255,40,30,0,255,150,100,190,150,0
25720 DATA 96,30,45,80,70,120,210,125,175,245,235,242,255,50,90,170,25,40,90,255,170,30,255,255,255,10,10,20,40,70,140,255,150,150,255,230,100
25730 DATA 112,60,40,30,230,195,95,190,150,60,245,200,160,210,155,115,120,180,245,70,125,205,50,85,160,30,55,115,120,75,35,255,255,255,35,30,40
25740 DATA 128,110,20,20,235,50,50,255,130,130,255,255,255,20,40,120,60,110,240,150,190,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
25750 DATA 144,50,30,10,120,75,30,190,140,70,245,230,200,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
25760 DATA 160,100,70,0,210,170,0,255,225,30,255,245,120,255,255,255,90,50,10,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
25770 DATA 176,90,10,0,200,50,0,255,120,0,255,190,30,255,240,150,255,255,255,150,150,150,90,90,90,255,255,255,255,255,255,255,255,255,255,255,255
25780 DATA 192,255,255,255,205,220,240,170,195,225,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
25790 DATA 208,30,110,45,70,180,60,140,230,90,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
25800 DATA 212,40,25,10,120,75,35,170,115,60,60,35,15,250,245,235,205,160,100,255,255,255,20,20,20,90,55,25,255,235,200,255,255,255,30,20,10
25810 DATA 224,60,40,20,255,215,80,220,170,40,230,50,50,255,255,255,60,110,240,150,190,255,60,180,70,140,90,40,255,255,120,200,255,120,120,120,120
25820 ' the times of day: top of the sky, horizon, far hills, near hills, grass, sun (r,g,b each), brightness of the bushes (percent), place of the sun, night, light text
25830 LABEL TODDATA
25840 DATA 70,70,150, 255,170,120, 150,130,150, 100,100,120, 70,130,70, 255,190,120, 85, 90,205, 0, 1
25850 DATA 60,130,230, 170,215,250, 115,190,125, 75,150,90, 75,175,55, 255,235,150, 100, 170,120, 0, 0
25860 DATA 45,110,225, 150,205,250, 115,190,125, 75,150,90, 75,175,55, 255,250,200, 100, 320,45, 0, 0
25870 DATA 70,125,215, 190,215,235, 130,185,110, 90,150,75, 90,170,50, 255,225,140, 100, 480,95, 0, 0
25880 DATA 90,70,150, 255,140,80, 140,110,120, 90,80,100, 70,110,60, 255,120,60, 80, 560,200, 0, 1
25890 DATA 10,15,50, 40,50,110, 35,55,80, 20,40,60, 25,70,45, 225,230,245, 50, 500,70, 1, 1
