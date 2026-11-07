1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' the helpers that the demo games share are in the libraries demos/lib/util.lib, input.lib, hiscore.lib and text.lib
20 LOAD LIBRARY "util"
30 LOAD LIBRARY "input"
40 LOAD LIBRARY "hiscore"
50 LOAD LIBRARY "text"
60 ' FRUIT INVADERS II - the middle edition of the fruit games, written with procedures. Read Fruit Invaders (fruit.bas) first: this is the same game
70 ' with more of the engine. A gardener walks on the grass under a sunny sky and throws balls at the fruit that marches down; the fruit spits seeds back.
80 ' Keys: LEFT / RIGHT or A / D walk (only while the key is held), SPACE or UP throws a ball, P = pause, Q = quit.
90 ' Game controller (optional, JOY): the D-pad or the left stick walks, A or the right trigger throws, START pauses, BACK quits.
100 ' Hit the fruit before it reaches the bushes. Cherries are worth 50 points, strawberries 40, apples 30, oranges 20, lemons 10, the bird that
110 ' flies by 50 to 300. Now and then a banana falls out of a fruit: catch it with the gardener for rapid fire (two balls at a time).
120 ' What is new compared to Fruit Invaders (fruit.bas):
130 '   * PROC / FUNC / CALL / LOCAL instead of GOSUB (the names of the procedures say what the game does); EXIT PROC, FOR EACH, ITERATE, EXIT,
140 '     DOWNTO, a LOCAL array and an array parameter (the music), RESUME NEXT / RESUME <label> with ON ERROR (the high score file)
150 '   * shaded sprites with faces and two animation frames, engine-driven animations (SPRITE SEQ / ANIM): the gardener has a walking cycle, a throwing
160 '     pose and loses his hat when he is hit (MOVETO), the bird flaps its wings; juice splashes with flying bits (MOVETO), leaves that fly off the bushes
170 '   * a sky of 24 palette bands with a sun and green hills on the background layer; the clouds drift at three speeds: a scrolling tile layer
180 '     (TILE SCROLL, the map is refilled from a longer world every 16 pixels) and two cloud sprites on paths (SPRITE n,PATH, SPRITE n,SIZE)
190 '   * seeds that turn while they fall and vanish in the grass, round bushes, the fruit glides from step to step (MOVETO) and rustles
200 '   * music while you play (two voices on their own channels, timed by EVERY, getting faster with every wave), more sound effects
210 '   * AFTER timer for the bird, a high score that is kept in the file fruit2.hi
220 '
230 ' --- global variables -----------------------------------------------------------------------------------------------------------------
240 ' SC score, HI high score, LV lives, WV wave, NX score of the next extra life, OV game over, QT quit, LANDED the fruit has landed
250 ' PX,PY gardener, PD his direction, GS his picture (0 stands, 1 walks, 2 throws), TP frames of throwing pose left, FQ throw request, RAPID frames of rapid fire left,
260 ' GA/GX/GY the balls, SEEDON/SEEDX/SEEDY the seeds
270 ' FX,FY the formation, DX its direction, SD step width, SP steps per move, NA fruits alive, IV(row,column) 1 = alive
280 ' UA,UX,UY,UD bird, BIRDREADY set by the AFTER timer, PU,PUX,PUY banana, EP() explosion timers, PL() particle lives
290 ' --------------------------------------------------------------------------------------------------------------------------------------
300 ' (a FUNC has to be defined before the lines that call it)
310 ' the number of fruits that are still alive (FOR EACH goes over the whole array, a row after the other)
320 FUNC COUNTALIVE(G())
330   LOCAL N,V
340   FOR EACH V IN G()
350     IF V>0 THEN N=N+1
360   NEXT
370   RETURN N
380 END FUNC
390 ' ====================================================================== main program
400 CALL INIT
410 HI=FN HS_LOAD("fruit2.hi")
420 QT=0
430 REPEAT
440   CALL TITLE
450   IF QT=0 THEN CALL PLAYGAME:QT=0
460   IF SC>=HI AND SC>0 THEN
470     HI=SC
480     ON ERROR GOTO HIERR2
490     CALL HS_SAVE("fruit2.hi",HI)
500     LABEL HIDONE2
510     ON ERROR OFF
520   END IF
530 UNTIL QT=1
540 CALL TX_BYE("FRUIT INVADERS II")
550 END
560 ' the library hiscore reads the high score (a missing or empty file is 0, no error); a file that can't be written is ignored: the error handler is here in the main program, where RESUME <label> can continue (inside a PROC the frame would be gone)
570 LABEL HIERR2
580 RESUME HIDONE2
590 ' the AFTER and EVERY handlers: they only call a procedure (the procedure uses LOCAL variables, so it can interrupt anything)
600 LABEL BARTIMER
610 CALL BAR
620 RETURN
630 LABEL BIRDTIMER
640 BIRDREADY=1
650 RETURN
660 ' ====================================================================== set up (once)
670 PROC INIT
680   TX_PAPER=0:TX_SHADOW=11   ' the colors of the text library: no paper behind the text, a violet shadow
690   LOCAL I
700   RANDOMIZE
710   GFX RESET ALL
720   ' (DIM n makes n elements, 0 to n-1)
730   DIM IV(6,8),GA(2),GX(2),GY(2),SEEDON(4),SEEDX(4),SEEDY(4),EP(4),PL(64),FW(6),RP(6),IMG(6),BASSF(4),CH3(12),PAT(8),SV(4)
740   RESTORE FRUITDATA
750   FOR I=1 TO 5
760     READ FW(I),IMG(I),RP(I)
770   NEXT I
780   FOR I=0 TO 3
790     READ BASSF(I),SV(I)
800   NEXT I
810   FOR I=0 TO 11
820     READ CH3(I)
830   NEXT I
840   FOR I=0 TO 7
850     READ PAT(I)
860   NEXT I
870   SX=36:SY=26:SHY=216:PY=265:UY=14
880   CALL SOUNDS
890   CALL PALETTES
900   CALL DEFSPRITES
910   CALL DEFTILES
920   CALL BACKGROUND
930   CALL CLOUDMAP
940   CALL CLOUDSPRITES
950   SPRITE SEQ 1,CHR$(10)+CHR$(11)+CHR$(12)+CHR$(11)
960   SPRITE SEQ 2,CHR$(19)+CHR$(20)+CHR$(21)+CHR$(20)
970   SPRITE SEQ 3,CHR$(22)+CHR$(23)
980   SPRITE SEQ 4,CHR$(14)+CHR$(15)+CHR$(16)+CHR$(15)
990   SPRITE SEQ 5,CHR$(24)+CHR$(25)+CHR$(26)+CHR$(27)
1000 END PROC
1010 PROC SOUNDS
1020   SFX 1,"W1 V0.28 ML X0,0.01,0.4,0.05 S1900,260,12"
1030   SFX 2,"W3 V0.55 ML X0,0.01,0.25,0.08 S1500,120,16"
1040   SFX 3,"W3 V1 ML X0,0.02,0.5,0.6 S900,30,3"
1050   SFX 4,"W1 V0.4 ML X0,0.01,0.3,0.05 H988,16 H1319,6"
1060   SFX 5,"W1 V0.4 ML H523,10 H659,10 H784,10 H1047,6"
1070   SFX 6,"W3 V0.35 ML X0,0.005,0.2,0.04 S500,90,32"
1080   SFX 7,"W2 V0.15 ML S800,420,10"
1090   SFX 8,"W1 V0.35 ML ~10,0.4 S300,1800,6"
1100   SFX 9,"W2 V0.4 ML H784,12 H988,12 H1319,6"
1110   SFX 10,"W1 V0.2 ML H1568,10 H2093,10"
1120   SFX 11,"W3 V0.3 ML X0.1,0.2,0.3,0.3 S200,1200,2"
1130   ' the rustle of the leaves when the fruit marches: two different noises one after the other
1140   SFX 12,"W3 V0.2 ML X0.005,0.02,0.3,0.06 S5000,3500,6"
1150   SFX 13,"W3 V0.2 ML X0.005,0.02,0.3,0.06 S3800,2800,6"
1160 END PROC
1170 ' ====================================================================== colors
1180 ' the sprite colors are in windows of 16 palette entries: the pictures use the digits 1..9 and A..C, and DESIGN adds the base of the window
1190 PROC LOADPAL
1200   LOCAL B,I,R,GG,BL
1210   READ B
1220   FOR I=1 TO 12
1230     READ R,GG,BL
1240     GFX PALETTE 4,B+I,RGB(R,GG,BL)
1250   NEXT I
1260 END PROC
1270 PROC PALETTES
1280   LOCAL I
1290   RESTORE PALDATA
1300   FOR I=1 TO 13
1310     CALL LOADPAL
1320   NEXT I
1330   ' the tiles: clouds (white with a bluish shadow)
1340   GFX PALETTE 1,1,RGB(255,255,255):GFX PALETTE 1,2,RGB(205,220,240)
1350   ' graphics layer: bushes (1 dark, 2 green, 3 light), grass (4, 5 light line), flowers (7..9), title (10..13)
1360   GFX PALETTE 3,1,RGB(30,110,45):GFX PALETTE 3,2,RGB(60,165,65):GFX PALETTE 3,3,RGB(130,220,95)
1370   GFX PALETTE 3,4,RGB(75,175,55):GFX PALETTE 3,5,RGB(120,205,80):GFX PALETTE 3,6,RGB(110,70,40)
1380   GFX PALETTE 3,7,RGB(250,70,100):GFX PALETTE 3,8,RGB(255,235,70):GFX PALETTE 3,9,RGB(255,255,255)
1390   GFX PALETTE 3,10,RGB(255,255,255):GFX PALETTE 3,11,RGB(20,45,100):GFX PALETTE 3,12,RGB(255,215,60):GFX PALETTE 3,13,RGB(255,255,255)
1400   ' text layer: the colors of the text lines
1410   GFX PALETTE 2,200,RGB(20,30,80):GFX PALETTE 2,201,RGB(150,80,0):GFX PALETTE 2,202,RGB(0,70,150):GFX PALETTE 2,203,RGB(190,30,30)
1420   GFX PALETTE 2,204,RGB(0,110,40):GFX PALETTE 2,205,RGB(50,60,120):GFX PALETTE 2,206,RGB(170,30,110):GFX PALETTE 2,207,RGB(190,90,0)
1430   GFX BACKDROP RGB(110,180,255)
1440 END PROC
1450 ' ====================================================================== background layer: sky, sun, hills
1460 ' the sky: 24 bands from deep blue at the top to pale blue at the horizon, T = 0 normal, 1 = a flash (the gardener was hit)
1470 PROC SKY(T)
1480   LOCAL I
1490   FOR I=1 TO 24
1500     GFX PALETTE 0,I,RGB(FN CLAMP(60+I*5+T*70,0,255),FN CLAMP(130+I*4+T*50,0,255),FN CLAMP(232+I\2+T*20,0,255))
1510   NEXT I
1520 END PROC
1530 PROC BACKGROUND
1540   LOCAL I,X,H
1550   GFX TARGET 0
1560   GFX CLEAR
1570   CALL SKY(0)
1580   FOR I=0 TO 23
1590     GFX LINE 0,I*13,639,I*13+12,I+1,BF
1600   NEXT I
1610   ' the sun: six disks around the same place, from pale (outside) to bright (inside)
1620   GFX PALETTE 0,40,RGB(255,252,215):GFX PALETTE 0,41,RGB(255,246,175):GFX PALETTE 0,42,RGB(255,238,135)
1630   GFX PALETTE 0,43,RGB(255,228,95):GFX PALETTE 0,44,RGB(255,216,65):GFX PALETTE 0,45,RGB(255,202,45)
1640   FOR I=0 TO 5
1650     CALL DISK(560,66,40-I*5,40+I)
1660   NEXT I
1670   ' hills: two ridges made from sine waves, the far one lighter
1680   GFX PALETTE 0,60,RGB(115,190,125):GFX PALETTE 0,61,RGB(75,150,90)
1690   FOR X=0 TO 638 STEP 2
1700     H=INT(34+16*SIN(X/41)+8*SIN(X/13+1))
1710     GFX LINE X,299-H,X+1,299,60,BF
1720   NEXT X
1730   FOR X=0 TO 638 STEP 2
1740     H=INT(14+8*SIN(X/29+2)+4*SIN(X/9))
1750     GFX LINE X,299-H,X+1,299,61,BF
1760   NEXT X
1770   GFX TARGET 3
1780 END PROC
1790 ' ====================================================================== the starfield on the tile layer
1800 PROC DEFSPRITES
1810   LOCAL R
1820   DESIGN SPRITE 0,16
1830   @ "................"
1840   @ "................"
1850   @ ".......6666....."
1860   @ "......67.7.8...."
1870   @ ".....67....78..."
1880   @ "....67......78.."
1890   @ "...67........8.."
1900   @ "................"
1910   @ "...67..........."
1920   @ "...6.......8...."
1930   @ "..11111....78..."
1940   @ ".1555441.11111.."
1950   @ "155444331555441."
1960   @ "1599433155444331"
1970   @ "14A9333159943331"
1980   @ "143332214A933321"
1990   @ ".133222143332221"
2000   @ "..11111.1332221."
2010   @ ".........11111.."
2020   FOR R=1 TO 5:@ "................":NEXT R
2030   DESIGN SPRITE 1,16
2040   @ "................"
2050   @ "................"
2060   @ ".......6666....."
2070   @ "......67.7.8...."
2080   @ ".....67....78..."
2090   @ "....67......78.."
2100   @ "...67........8.."
2110   @ "................"
2120   @ "...........8...."
2130   @ "...67......78..."
2140   @ "...6.....11111.."
2150   @ "..11111.1555441."
2160   @ ".155544155444331"
2170   @ "1554443159943331"
2180   @ "1599433149A33321"
2190   @ "149A333143332221"
2200   @ "143332221332221."
2210   @ ".1332221.11111.."
2220   @ "..11111........."
2230   FOR R=1 TO 5:@ "................":NEXT R
2240   DESIGN SPRITE 2,32
2250   FOR R=1 TO 4:@ "................":NEXT R
2260   @ "......6.8..6...."
2270   @ ".....6788.8786.."
2280   @ "......67878876.."
2290   @ ".11111116786111."
2300   @ ".15555555544441."
2310   @ ".15555554444431."
2320   @ ".1555994C99C331."
2330   @ ".1555A944A93331."
2340   @ "..154A944A9331.."
2350   @ "..1444C33C3321.."
2360   @ "..1444A33A3221.."
2370   @ "...1333AA3221..."
2380   @ "...1333C22C21..."
2390   @ "....13322221...."
2400   @ ".....122221....."
2410   @ "......11C1......"
2420   FOR R=1 TO 4:@ "................":NEXT R
2430   DESIGN SPRITE 3,32
2440   FOR R=1 TO 3:@ "................":NEXT R
2450   @ "......6.8..6...."
2460   @ ".....6788.8786.."
2470   @ "......67878876.."
2480   @ ".11111116786111."
2490   @ ".15555555544441."
2500   @ ".15555554444431."
2510   @ ".1555994C99C331."
2520   @ ".15559A449A3331."
2530   @ "..1549A449A331.."
2540   @ "..1444C33C3321.."
2550   @ "..14443AA33221.."
2560   @ "...133AAAA221..."
2570   @ "...1333C22C21..."
2580   @ "....13322221...."
2590   @ ".....122221....."
2600   @ "......11C1......"
2610   FOR R=1 TO 5:@ "................":NEXT R
2620   DESIGN SPRITE 4,48
2630   FOR R=1 TO 3:@ "................":NEXT R
2640   @ ".......6.7788..."
2650   @ ".......6.8888..."
2660   @ ".......6..88...."
2670   @ "................"
2680   @ ".....1....1....."
2690   @ "...1151..1411..."
2700   @ "..155551144441.."
2710   @ ".15559944994331."
2720   @ ".1555A944A93331."
2730   @ ".1554A944A93331."
2740   @ ".14444433333221."
2750   @ ".14444333332221."
2760   @ ".14433A33A22221."
2770   @ ".143333AA222221."
2780   @ "..133332222221.."
2790   @ "...1122222211..."
2800   @ ".....111111....."
2810   FOR R=1 TO 4:@ "................":NEXT R
2820   DESIGN SPRITE 5,48
2830   @ "................"
2840   @ "................"
2850   @ ".......6.7788..."
2860   @ ".......6.8888..."
2870   @ ".......6..88...."
2880   @ "................"
2890   @ ".....1....1....."
2900   @ "...1151..1411..."
2910   @ "..155551144441.."
2920   @ ".15559944994331."
2930   @ ".15559A449A3331."
2940   @ ".15549A449A3331."
2950   @ ".14444433333221."
2960   @ ".14444333332221."
2970   @ ".144333AA322221."
2980   @ ".14333AAAA22221."
2990   @ "..133332222221.."
3000   @ "...1122222211..."
3010   @ ".....111111....."
3020   FOR R=1 TO 5:@ "................":NEXT R
3030   DESIGN SPRITE 6,64
3040   FOR R=1 TO 3:@ "................":NEXT R
3050   @ ".......6788....."
3060   @ ".......6........"
3070   @ "................"
3080   @ "......1111......"
3090   @ "....11C55511...."
3100   @ "...15555544C1..."
3110   @ "..15C554444431.."
3120   @ ".15559944993331."
3130   @ ".1555A944A93331."
3140   @ ".1554A944A93331."
3150   @ ".14444433333C21."
3160   @ ".14C44333332221."
3170   @ ".14443A33A22221."
3180   @ ".144333AA222221."
3190   @ "..13333222C221.."
3200   @ "...1332C22221..."
3210   @ "....11222211...."
3220   @ "......1111......"
3230   FOR R=1 TO 3:@ "................":NEXT R
3240   DESIGN SPRITE 7,64
3250   @ "................"
3260   @ "................"
3270   @ ".......6788....."
3280   @ ".......6........"
3290   @ "................"
3300   @ "......1111......"
3310   @ "....11C55511...."
3320   @ "...15555544C1..."
3330   @ "..15C554444431.."
3340   @ ".15559944993331."
3350   @ ".15559A449A3331."
3360   @ ".15549A449A3331."
3370   @ ".14444433333C21."
3380   @ ".14C44333332221."
3390   @ ".144433AA322221."
3400   @ ".14433AAAA22221."
3410   @ "..13333222C221.."
3420   @ "...1332C22221..."
3430   @ "....11222211...."
3440   @ "......1111......"
3450   FOR R=1 TO 4:@ "................":NEXT R
3460   DESIGN SPRITE 8,80
3470   FOR R=1 TO 3:@ "................":NEXT R
3480   @ ".......6788....."
3490   @ ".......6.88....."
3500   FOR R=1 TO 3:@ "................":NEXT R
3510   @ "....11111111...."
3520   @ "...1555554441..."
3530   @ "..155994499441.."
3540   @ ".1555A944A93331."
3550   @ ".1544A944A93331."
3560   @ "1444444333333221"
3570   @ ".14443333332221."
3580   @ ".14333A33A22221."
3590   @ "..13333AA22221.."
3600   @ "...1322222221..."
3610   @ "....11111111...."
3620   FOR R=1 TO 5:@ "................":NEXT R
3630   DESIGN SPRITE 9,80
3640   @ "................"
3650   @ "................"
3660   @ ".......6788....."
3670   @ ".......6.88....."
3680   FOR R=1 TO 3:@ "................":NEXT R
3690   @ "....11111111...."
3700   @ "...1555554441..."
3710   @ "..155994499441.."
3720   @ ".15559A449A3331."
3730   @ ".15449A449A3331."
3740   @ "1444444333333221"
3750   @ ".14443333332221."
3760   @ ".143333AA222221."
3770   @ "..1333AAAA2221.."
3780   @ "...1322222221..."
3790   @ "....11111111...."
3800   FOR R=1 TO 6:@ "................":NEXT R
3810   ' image 10-12: the bird (wings up, level, down)
3820   DESIGN SPRITE 10,96
3830   FOR R=1 TO 5:@ "................":NEXT R
3840   @ ".5............5."
3850   @ ".55..........55."
3860   @ ".555........555."
3870   @ "..555......555.."
3880   @ "..555.1111.555.."
3890   @ ".....123321....."
3900   @ ".....139931....."
3910   @ "......1771......"
3920   @ ".....124421....."
3930   @ ".....124421....."
3940   @ "......1441......"
3950   @ "......1AA1......"
3960   @ ".....1AAAA1....."
3970   @ "......1AA1......"
3980   FOR R=1 TO 5:@ "................":NEXT R
3990   DESIGN SPRITE 11,96
4000   FOR R=1 TO 9:@ "................":NEXT R
4010   @ "......1111......"
4020   @ ".....123321....."
4030   @ "5555.139931.5555"
4040   @ "55555.1771.55555"
4050   @ ".555.124421.555."
4060   @ ".....124421....."
4070   @ "......1441......"
4080   @ "......1AA1......"
4090   @ ".....1AAAA1....."
4100   @ "......1AA1......"
4110   FOR R=1 TO 5:@ "................":NEXT R
4120   DESIGN SPRITE 12,96
4130   FOR R=1 TO 9:@ "................":NEXT R
4140   @ "......1111......"
4150   @ ".....123321....."
4160   @ ".....139931....."
4170   @ "......1771......"
4180   @ ".....124421....."
4190   @ "..5..124421..5.."
4200   @ "..55..1441..55.."
4210   @ ".555..1AA1..555."
4220   @ ".5555.1AA1.5555."
4230   @ "..55..1AA1..55.."
4240   FOR R=1 TO 5:@ "................":NEXT R
4250   ' image 13-16: the gardener (standing, three walking pictures); images 34 (throwing), 35 (the hat), 36 (hit)
4260   DESIGN SPRITE 13,112
4270   @ "................"
4280   @ "......2222......"
4290   @ "....22222223...."
4300   @ "..2222222222333."
4310   @ "...3333333333..."
4320   @ "....44444455...."
4330   @ "....4C4444C5...."
4340   @ "....4441145....."
4350   @ ".....444555....."
4360   @ "...6666666666..."
4370   @ "..666666666677.."
4380   @ "..666666666677.."
4390   @ "..468888888864.."
4400   @ "..4.88888888.4.."
4410   @ "..4.88888888.4.."
4420   @ "....88888899...."
4430   @ "....88888899...."
4440   @ "....888..999...."
4450   @ "....888..999...."
4460   @ "....888..999...."
4470   @ "....888..999...."
4480   @ "....AAA..AAA...."
4490   @ "...AAAA..AAAA..."
4500   @ "................"
4510   DESIGN SPRITE 14,112
4520   @ "................"
4530   @ "......2222......"
4540   @ "....22222223...."
4550   @ "..2222222222333."
4560   @ "...3333333333..."
4570   @ "....44444455...."
4580   @ "....4C4444C5...."
4590   @ "....4441145....."
4600   @ ".....444555....."
4610   @ "...6666666666..."
4620   @ "..666666666677.."
4630   @ "..666666666677.."
4640   @ "..468888888864.."
4650   @ "..4.88888888.4.."
4660   @ "..4.88888888.4.."
4670   @ "....88888899...."
4680   @ "....88888899...."
4690   @ "..888.......999."
4700   @ "..888.......999."
4710   @ "..888.......999."
4720   @ "..888.......999."
4730   @ "..AAA.......AAA."
4740   @ ".AAAA.......AAAA"
4750   @ "................"
4760   DESIGN SPRITE 15,112
4770   @ "................"
4780   @ "......2222......"
4790   @ "....22222223...."
4800   @ "..2222222222333."
4810   @ "...3333333333..."
4820   @ "....44444455...."
4830   @ "....4C4444C5...."
4840   @ "....4441145....."
4850   @ ".....444555....."
4860   @ "...6666666666..."
4870   @ "..666666666677.."
4880   @ "..666666666677.."
4890   @ "..468888888864.."
4900   @ "..4.88888888.4.."
4910   @ "..4.88888888.4.."
4920   @ "....88888899...."
4930   @ "....88888899...."
4940   @ "....888..999...."
4950   @ "....888..999...."
4960   @ "....888..999...."
4970   @ "....888..999...."
4980   @ "....AAA..AAA...."
4990   @ "...AAAA..AAAA..."
5000   @ "................"
5010   DESIGN SPRITE 16,112
5020   @ "................"
5030   @ "......2222......"
5040   @ "....22222223...."
5050   @ "..2222222222333."
5060   @ "...3333333333..."
5070   @ "....44444455...."
5080   @ "....4C4444C5...."
5090   @ "....4441145....."
5100   @ ".....444555....."
5110   @ "...6666666666..."
5120   @ "..666666666677.."
5130   @ "..666666666677.."
5140   @ "..468888888864.."
5150   @ "..4.88888888.4.."
5160   @ "..4.88888888.4.."
5170   @ "....88888899...."
5180   @ "....88888899...."
5190   @ ".888.......999.."
5200   @ ".888.......999.."
5210   @ ".888.......999.."
5220   @ ".888.......999.."
5230   @ ".AAA.......AAA.."
5240   @ "AAAA.......AAAA."
5250   @ "................"
5260   ' image 17-18: the ball (red, and blue for rapid fire)
5270   DESIGN SPRITE 17,128
5280   FOR R=1 TO 6:@ "................":NEXT R
5290   @ "......1111......"
5300   @ ".....122221....."
5310   @ "....12234221...."
5320   @ "....12222221...."
5330   @ "....12222221...."
5340   @ ".....122221....."
5350   @ "......1111......"
5360   FOR R=1 TO 11:@ "................":NEXT R
5370   DESIGN SPRITE 18,128
5380   FOR R=1 TO 6:@ "................":NEXT R
5390   @ "......5555......"
5400   @ ".....566665....."
5410   @ "....56674665...."
5420   @ "....56666665...."
5430   @ "....56666665...."
5440   @ ".....566665....."
5450   @ "......5555......"
5460   FOR R=1 TO 11:@ "................":NEXT R
5470   ' image 19-21: the seed turns while it falls
5480   DESIGN SPRITE 19,144
5490   FOR R=1 TO 7:@ "................":NEXT R
5500   @ ".......11......."
5510   @ "......1221......"
5520   @ ".....123321....."
5530   @ ".....123321....."
5540   @ ".....123321....."
5550   @ "......1221......"
5560   @ "......1221......"
5570   @ ".......11......."
5580   @ ".......1........"
5590   FOR R=1 TO 8:@ "................":NEXT R
5600   DESIGN SPRITE 20,144
5610   FOR R=1 TO 7:@ "................":NEXT R
5620   @ ".......11......."
5630   @ "......1221......"
5640   @ "......1231......"
5650   @ "......1231......"
5660   @ "......1231......"
5670   @ "......1221......"
5680   @ "......1221......"
5690   @ ".......11......."
5700   @ ".......1........"
5710   FOR R=1 TO 8:@ "................":NEXT R
5720   DESIGN SPRITE 21,144
5730   FOR R=1 TO 7:@ "................":NEXT R
5740   @ ".......11......."
5750   @ "......1221......"
5760   @ ".....122321....."
5770   @ ".....122321....."
5780   @ ".....122321....."
5790   @ "......1221......"
5800   @ "......1221......"
5810   @ ".......11......."
5820   @ ".......1........"
5830   FOR R=1 TO 8:@ "................":NEXT R
5840   DESIGN SPRITE 22,160
5850   FOR R=1 TO 3:@ "................":NEXT R
5860   @ ".............5.."
5870   @ "..1111111111555."
5880   @ ".14644444433351."
5890   @ ".14444443333331."
5900   @ "1444443333333331"
5910   @ "1444333333333331"
5920   @ "1433311111133321"
5930   @ "13331......12221"
5940   @ "1331........1221"
5950   @ "131..........121"
5960   @ ".11..........11."
5970   @ ".11..........11."
5980   @ "..1..........1.."
5990   FOR R=1 TO 8:@ "................":NEXT R
6000   DESIGN SPRITE 23,160
6010   FOR R=1 TO 4:@ "................":NEXT R
6020   @ "..111111111111.."
6030   @ ".14644444433361."
6040   @ ".14444443333331."
6050   @ "1444443333333331"
6060   @ "1444333333333331"
6070   @ "1433311111133321"
6080   @ "13331......12221"
6090   @ "1331........1221"
6100   @ "1315.........121"
6110   @ ".1555........11."
6120   @ ".115.........11."
6130   @ "..1..........1.."
6140   FOR R=1 TO 8:@ "................":NEXT R
6150   DESIGN SPRITE 24,176
6160   FOR R=1 TO 9:@ "................":NEXT R
6170   @ ".......33......."
6180   @ "......3553......"
6190   @ ".....355443....."
6200   @ ".....354443....."
6210   @ "......3443......"
6220   @ ".......33......."
6230   FOR R=1 TO 9:@ "................":NEXT R
6240   DESIGN SPRITE 25,176
6250   FOR R=1 TO 5:@ "................":NEXT R
6260   @ "........4......."
6270   @ "................"
6280   @ "...4.222222..4.."
6290   @ "....25555442...."
6300   @ "...2555444432..."
6310   @ "...2554444332..."
6320   @ "...2544433332..."
6330   @ ".4.2444333332..4"
6340   @ "...2443333332..."
6350   @ "...2333333332..."
6360   @ "....23333332...."
6370   @ ".....222222....."
6380   @ "...4.........4.."
6390   @ "................"
6400   @ "........4......."
6410   FOR R=1 TO 4:@ "................":NEXT R
6420   DESIGN SPRITE 26,176
6430   FOR R=1 TO 3:@ "................":NEXT R
6440   @ "......5........."
6450   @ ".......22......."
6460   @ "....22222222...."
6470   @ "...2223333222..5"
6480   @ "..223333333322.."
6490   @ ".22334444443322."
6500   @ "522344444444322."
6510   @ ".23344....44332."
6520   @ "223344....443322"
6530   @ "223344....443322"
6540   @ ".23344....44332."
6550   @ ".22344444444322."
6560   @ ".22334444443322."
6570   @ "..223333333322.."
6580   @ "...2223333222..."
6590   @ ".5..22222222...."
6600   @ ".......22......."
6610   @ "................"
6620   @ "..........5....."
6630   @ "................"
6640   @ "................"
6650   DESIGN SPRITE 27,176
6660   FOR R=1 TO 4:@ "................":NEXT R
6670   @ ".........78....."
6680   @ ".........8......"
6690   @ "....78.........."
6700   @ "....8..........."
6710   @ ".........8......"
6720   @ "................"
6730   @ "................"
6740   @ "....8..........."
6750   @ "...............7"
6760   @ "............8..8"
6770   @ "................"
6780   @ "78.............."
6790   @ "8......8........"
6800   @ "................"
6810   @ ".....78........7"
6820   @ ".....8.........8"
6830   @ "................"
6840   @ "................"
6850   @ "..........78...."
6860   @ "..........8....."
6870   DESIGN SPRITE 28,16
6880   FOR R=1 TO 11:@ "................":NEXT R
6890   @ "........5......."
6900   @ ".......534......"
6910   @ "........3......."
6920   FOR R=1 TO 10:@ "................":NEXT R
6930   DESIGN SPRITE 29,32
6940   FOR R=1 TO 11:@ "................":NEXT R
6950   @ "........5......."
6960   @ ".......534......"
6970   @ "........3......."
6980   FOR R=1 TO 10:@ "................":NEXT R
6990   DESIGN SPRITE 30,48
7000   FOR R=1 TO 11:@ "................":NEXT R
7010   @ "........5......."
7020   @ ".......534......"
7030   @ "........3......."
7040   FOR R=1 TO 10:@ "................":NEXT R
7050   DESIGN SPRITE 31,64
7060   FOR R=1 TO 11:@ "................":NEXT R
7070   @ "........5......."
7080   @ ".......534......"
7090   @ "........3......."
7100   FOR R=1 TO 10:@ "................":NEXT R
7110   DESIGN SPRITE 32,80
7120   FOR R=1 TO 11:@ "................":NEXT R
7130   @ "........5......."
7140   @ ".......534......"
7150   @ "........3......."
7160   FOR R=1 TO 10:@ "................":NEXT R
7170   DESIGN SPRITE 33,176
7180   FOR R=1 TO 11:@ "................":NEXT R
7190   @ "........6......."
7200   @ ".......656......"
7210   @ "........6......."
7220   FOR R=1 TO 10:@ "................":NEXT R
7230   DESIGN SPRITE 34,112
7240   @ "................"
7250   @ "......2222......"
7260   @ "....22222223...."
7270   @ "..2222222222333."
7280   @ "...3333333333..."
7290   @ "....44444455.44."
7300   @ "....4C4444C5..44"
7310   @ "....4441145.66.."
7320   @ ".....444555.66.."
7330   @ "...666666666666."
7340   @ "..666666666677.."
7350   @ "..666666666677.."
7360   @ "..468888888888.."
7370   @ "..4.88888888...."
7380   @ "..4.88888888...."
7390   @ "....88888899...."
7400   @ "....88888899...."
7410   @ "....888..999...."
7420   @ "....888..999...."
7430   @ "....888..999...."
7440   @ "....888..999...."
7450   @ "....AAA..AAA...."
7460   @ "...AAAA..AAAA..."
7470   @ "................"
7480   DESIGN SPRITE 35,112
7490   FOR R=1 TO 6:@ "................":NEXT R
7500   @ "......2222......"
7510   @ "....22222223...."
7520   @ "..2222222222333."
7530   @ "...3333333333..."
7540   FOR R=1 TO 14:@ "................":NEXT R
7550   DESIGN SPRITE 36,112
7560   @ "................"
7570   @ "......1111......"
7580   @ "....11111111...."
7590   @ "...1111111111..."
7600   @ "...1144444411..."
7610   @ ".44.44444455.44."
7620   @ ".66.4C4C4CC5.66."
7630   @ ".66.4441C45.66.."
7640   @ ".66..444555.66.."
7650   @ "..666666666666.."
7660   @ "..666666666677.."
7670   @ "..666666666677.."
7680   @ "..4.88888888.4.."
7690   @ "..4.88888888.4.."
7700   @ "....88888888...."
7710   @ "....88888899...."
7720   @ "....88888899...."
7730   @ "....888..999...."
7740   @ "....888..999...."
7750   @ "....888..999...."
7760   @ "....888..999...."
7770   @ "....AAA..AAA...."
7780   @ "...AAAA..AAAA..."
7790   @ "................"
7800   ' image 37-39: pieces of the clouds that fly by in front of the tile clouds
7810   DESIGN SPRITE 37,192
7820   FOR R=1 TO 8:@ "................":NEXT R
7830   @ "........1111...."
7840   @ "......11111111.."
7850   @ "....111111111111"
7860   @ "...1111111111111"
7870   @ "..11111111111111"
7880   @ ".111111111111111"
7890   @ ".111111111111111"
7900   @ "1111111111111111"
7910   @ "1111111111111111"
7920   @ ".222222222222222"
7930   @ "..22222222222222"
7940   FOR R=1 TO 5:@ "................":NEXT R
7950   DESIGN SPRITE 38,192
7960   FOR R=1 TO 5:@ "................":NEXT R
7970   @ "......1111......"
7980   @ "....11111111...."
7990   @ "...1111111111..."
8000   @ "..111111111111.."
8010   @ ".11111111111111."
8020   @ "1111111111111111"
8030   @ "1111111111111111"
8040   @ "1111111111111111"
8050   @ "1111111111111111"
8060   @ "2222222222222222"
8070   @ "2222222222222222"
8080   FOR R=1 TO 8:@ "................":NEXT R
8090   DESIGN SPRITE 39,192
8100   FOR R=1 TO 8:@ "................":NEXT R
8110   @ "....1111........"
8120   @ "..11111111......"
8130   @ "111111111111...."
8140   @ "1111111111111..."
8150   @ "11111111111111.."
8160   @ "111111111111111."
8170   @ "111111111111111."
8180   @ "1111111111111111"
8190   @ "1111111111111111"
8200   @ "2222222222222222"
8210   @ ".222222222222222"
8220   @ "..2222222222222."
8230   FOR R=1 TO 4:@ "................":NEXT R
8240   ' image 40: a leaf that flies off a bush
8250   DESIGN SPRITE 40,208
8260   FOR R=1 TO 11:@ "................":NEXT R
8270   @ ".......1........"
8280   @ "......121......."
8290   @ ".....12321......"
8300   @ "......121......."
8310   @ ".......1........"
8320   FOR R=1 TO 8:@ "................":NEXT R
8330 END PROC
8340 PROC DEFTILES
8350   LOCAL R
8360   DESIGN TILE 1
8370   FOR R=1 TO 9:@ "................":NEXT R
8380   @ "........1111...."
8390   @ "......11111111.."
8400   @ ".....1111111111."
8410   @ "....111111111111"
8420   @ "...1111111111111"
8430   @ "...1111111111111"
8440   @ "..11111111111111"
8450   @ "..11111111111111"
8460   @ ".111111111111111"
8470   @ "1111111111111111"
8480   @ ".111111111111111"
8490   @ "..22222222222222"
8500   @ "...2222222222222"
8510   FOR R=1 TO 2:@ "................":NEXT R
8520   DESIGN TILE 2
8530   FOR R=1 TO 4:@ "................":NEXT R
8540   @ "......1111......"
8550   @ "....11111111...."
8560   @ "...1111111111..."
8570   @ "..111111111111.."
8580   @ ".11111111111111."
8590   @ ".11111111111111."
8600   @ "1111111111111111"
8610   @ "1111111111111111"
8620   @ "1111111111111111"
8630   @ "1111111111111111"
8640   @ "1111111111111111"
8650   @ "1111111111111111"
8660   @ "1111111111111111"
8670   @ "1111111111111111"
8680   @ "1111111111111111"
8690   @ "1111111111111111"
8700   @ "1111111111111111"
8710   @ "2222222222222222"
8720   @ "2222222222222222"
8730   @ "................"
8740   DESIGN TILE 3
8750   FOR R=1 TO 10:@ "................":NEXT R
8760   @ ".11111111......."
8770   @ "11111111111....."
8780   @ "111111111111...."
8790   @ "1111111111111..."
8800   @ "1111111111111..."
8810   @ "11111111111111.."
8820   @ "11111111111111.."
8830   @ "111111111111111."
8840   @ "111111111111111."
8850   @ "1111111111111111"
8860   @ "11111111111111.."
8870   @ "22222222222222.."
8880   @ "222222222222...."
8890   @ "................"
8900   DESIGN TILE 4
8910   FOR R=1 TO 13:@ "................":NEXT R
8920   @ "......1111......"
8930   @ "....11111111...."
8940   @ "..111111111111.."
8950   @ ".11111111111111."
8960   @ "1111111111111111"
8970   @ "1111111111111111"
8980   @ "1111111111111111"
8990   @ "2222222222222222"
9000   @ ".22222222222222."
9010   FOR R=1 TO 2:@ "................":NEXT R
9020 END PROC
9030 ' the clouds of the tile layer live in a world of 64 columns that wraps around (CW$, 13 rows after each other, tile numbers as characters: 0 empty,
9040 ' 1 left, 2 middle, 3 right part of a cloud, 4 a small puff); only rows 1, 3, 5 and 7 have clouds
9050 PROC CLOUDMAP
9060   LOCAL R,C,K,L,I,RW$,CL$
9070   CW$=""
9080   FOR R=0 TO 12
9090     RW$=STRING$(64,0)
9100     IF R=1 OR R=3 OR R=5 OR R=7 THEN
9110       FOR K=1 TO 3
9120         L=1+FN RN(4)
9130         IF L=1 THEN
9140           CL$=CHR$(4)
9150         ELSE
9160           CL$=CHR$(1)
9170           FOR I=2 TO L-1
9180             CL$=CL$+CHR$(2)
9190           NEXT I
9200           CL$=CL$+CHR$(3)
9210         END IF
9220         C=FN RN(64-L)
9230         RW$=LEFT$(RW$,C)+CL$+MID$(RW$,C+LEN(CL$)+1)
9240       NEXT K
9250     END IF
9260     CW$=CW$+RW$
9270   NEXT R
9280   CAMX=1:CT=0:CLASTC=-1
9290 END PROC
9300 ' the clouds drift to the left: the camera CAMX moves a pixel every second frame; every 16 pixels the 41 x 13 cells are refilled from the world
9310 ' (cell I of a row shows the world column (CAMX-1)\16 + 1 + I, the last cell the column before), in between TILE SCROLL moves the picture
9320 PROC CLOUDS
9330   LOCAL R,I,CC,M$,K
9340   CT=CT+1
9350   IF CT MOD 2<>0 THEN EXIT PROC
9360   CAMX=CAMX+1
9370   CC=(CAMX-1)\16
9380   IF CC<>CLASTC THEN
9390     CLASTC=CC
9400     M$=""
9410     FOR R=0 TO 12
9420       IF R=1 OR R=3 OR R=5 OR R=7 THEN
9430         FOR I=0 TO 40
9440           K=(CC+1+I) MOD 64
9450           IF I=40 THEN K=CC MOD 64
9460           M$=M$+MID$(CW$,R*64+K+1,1)
9470         NEXT I
9480       ELSE
9490         M$=M$+STRING$(41,CHR$(0))
9500       END IF
9510     NEXT R
9520     TILE FILL M$
9530   END IF
9540   CSX=15-(CAMX-1) MOD 16
9550   TILE SCROLL CSX,0
9560 END PROC
9570 ' two more clouds in front of the tile clouds, faster: sprite groups of 3 x 1 sprites that fly on paths (the engine moves them, the program does nothing)
9580 ' (priority 1 puts them behind the fruit and the gardener, which have the standard priority 3)
9590 PROC CLOUDSPRITES
9600   LOCAL I,N,Y,V,X0
9610   FOR I=0 TO 1
9620     N=130+I*3
9630     SPRITE N,0,0,37:SPRITE N+1,0,0,38:SPRITE N+2,0,0,39
9640     SPRITE N,SIZE 3,1
9650     SPRITE N,PRIORITY 1
9660     SPRITE N,ON
9670     Y=60+I*70:V=1+I:X0=100+I*300
9680     SPRITE N,PATH "X="+STR$(X0)+" Y="+STR$(Y)+" X+"+STR$(V)+"*"+STR$(INT((680-X0)/V))+" [ X=-60 X+"+STR$(V)+"*"+STR$(INT(740/V))+" ]9999"
9690   NEXT I
9700 END PROC
9710 ' ====================================================================== the screen
9720 PROC CLEARALL
9730   LOCAL I
9740   EVERY OFF
9750   AFTER OFF
9760   SOUND OFF
9770   CLS
9780   GFX CLEAR
9790   FOR I=0 TO 127
9800     SPRITE I,OFF
9810   NEXT I
9820   FOR I=0 TO 3
9830     EP(I)=0
9840   NEXT I
9850   FOR I=0 TO 63
9860     PL(I)=0
9870   NEXT I
9880   CALL SKY(0)
9890 END PROC
9900 ' the grass strip at the bottom of the screen, on the graphics layer, with a few flowers
9910 PROC GROUND
9920   LOCAL I,X,Y
9930   GFX LINE 0,288,639,299,4,BF
9940   GFX LINE 0,288,639,289,5,BF
9950   FOR I=1 TO 90
9960     GFX PSET FN RN(640),290+FN RN(10),5
9970   NEXT I
9980   FOR I=1 TO 45
9990     X=FN RN(640):Y=291+FN RN(8)
10000     GFX PSET X,Y,7+FN RN(3)
10010   NEXT I
10020 END PROC
10030 ' the four round bushes: every bush is four disks, drawn three times: a dark shadow, the green, and light leaves on top
10040 PROC BUSHES
10050   LOCAL I,B,P,DX,DY,R
10060   FOR I=0 TO 3
10070     B=90+I*140
10080     RESTORE BUSHDATA
10090     FOR P=0 TO 3
10100       READ DX,DY,R
10110       CALL DISK(B+DX+1,SHY+DY+2,R,1)
10120     NEXT P
10130     RESTORE BUSHDATA
10140     FOR P=0 TO 3
10150       READ DX,DY,R
10160       CALL DISK(B+DX,SHY+DY,R,2)
10170     NEXT P
10180     CALL DISK(B-12,SHY+10,3,3):CALL DISK(B,SHY+5,5,3):CALL DISK(B+11,SHY+10,3,3)
10190     CALL DISK(B-5,SHY+14,2,3):CALL DISK(B+6,SHY+16,2,3)
10200   NEXT I
10210 END PROC
10220 LABEL BUSHDATA
10230 DATA -11,13,9, 11,13,9, 0,9,11, 0,14,9
10240 ' ====================================================================== text helpers
10250 ' centered text on the graphics layer with a shadow
10260 ' the score line and the lives (small hats)
10270 PROC HUD
10280   LOCAL I
10290   CALL TXTAT(1,0,"SCORE",205)
10300   CALL TXTAT(7,0,FN PAD$(SC,6),201)
10310   CALL TXTAT(22,0,"HI",205)
10320   CALL TXTAT(25,0,FN PAD$(HI,6),200)
10330   CALL TXTAT(38,0,"WAVE",205)
10340   CALL TXTAT(43,0,STR$(WV)+"  ",202)
10350   CALL TXTAT(58,0,"LIVES",205)
10360   FOR I=0 TO 4
10370     IF I<LV THEN
10380       SPRITE 54+I,524+I*20,-3,35
10390       SPRITE 54+I,ON
10400     ELSE
10410       SPRITE 54+I,OFF
10420     END IF
10430   NEXT I
10440   IF RAPID>0 THEN
10450     CALL TXTAT(1,1,"RAPID FIRE",203)
10460   ELSE
10470     CALL TXTAT(1,1,"          ",200)
10480   END IF
10490   HF=0
10500 END PROC
10510 ' ====================================================================== keys
10520 PROC KEYS
10530   CALL IN_POLL
10540   PD=IN_DX
10550   IF IN_BTN OR KEY("UP") OR JOY("RT")>0.4 THEN FQ=1
10560   IF FN IN_SERVICE() THEN CALL PAUSE
10570   IF IN_QUIT THEN QT=1
10580 END PROC
10590 PROC PAUSE
10600   CALL TXTAT(36,17,"PAUSED",201)
10610   CALL IN_PAUSELOOP
10620   IF IN_QUIT THEN QT=1
10630   TXT FILL 36,17,41,17,32
10640 END PROC
10650 ' ====================================================================== the gardener and his balls
10660 PROC MOVEGARDENER
10670   LOCAL W
10680   IF RAPID>0 THEN
10690     RAPID=RAPID-1
10700     IF RAPID=0 THEN HF=1
10710   END IF
10720   ' the picture of the gardener: throwing (2) while TP counts down, else walking (1: the animation sequence 4) or standing (0)
10730   IF TP>0 THEN TP=TP-1
10740   W=0
10750   IF PD<>0 THEN W=1
10760   IF TP>0 THEN W=2
10770   IF W<>GS THEN
10780     GS=W
10790     IF W=2 THEN SPRITE 40,IMAGE 34
10800     IF W=1 THEN SPRITE 40,ANIM 4,4,5
10810     IF W=0 THEN SPRITE 40,IMAGE 13
10820   END IF
10830   IF PD=0 THEN EXIT PROC
10840   PX=FN CLAMP(PX+PD*3,4,620)
10850   IF PX=4 OR PX=620 THEN PD=0
10860   SPRITE 40,PX,PY
10870 END PROC
10880 PROC BALLS
10890   LOCAL I
10900   IF FQ=1 THEN CALL THROW
10910   FOR I=0 TO 1
10920     IF GA(I)=1 THEN CALL BALLMOVE(I)
10930   NEXT I
10940 END PROC
10950 PROC THROW
10960   LOCAL I,M
10970   FQ=0
10980   M=1
10990   IF RAPID>0 THEN M=2
11000   FOR I=0 TO M-1
11010     IF GA(I)=0 THEN
11020       GA(I)=1:GX(I)=PX:GY(I)=PY-8
11030       TP=8
11040       SPRITE 42+I,GX(I),GY(I),17
11050       IF RAPID>0 THEN SPRITE 42+I,IMAGE 18
11060       SPRITE 42+I,ON
11070       SFX 1
11080       EXIT PROC
11090     END IF
11100   NEXT I
11110 END PROC
11120 PROC BALLMOVE(I)
11130   LOCAL TX,TY
11140   GY(I)=GY(I)-7
11150   IF GY(I)<8 THEN
11160     CALL KILLBALL(I)
11170     EXIT PROC
11180   END IF
11190   SPRITE 42+I,GX(I),GY(I)
11200   TX=GX(I)+7:TY=GY(I)+6
11210   IF POINT(TX,TY)>0 THEN
11220     CALL BUSHHIT(TX,TY)
11230     CALL KILLBALL(I)
11240     EXIT PROC
11250   END IF
11260   IF UA=1 AND TX>=UX AND TX<UX+16 AND TY>=UY+2 AND TY<UY+22 THEN
11270     CALL BIRDHIT(I)
11280     EXIT PROC
11290   END IF
11300   IF TX>=FX AND TY>=FY THEN CALL INVADERHIT(I,TX,TY)
11310 END PROC
11320 PROC KILLBALL(I)
11330   GA(I)=0
11340   SPRITE 42+I,OFF
11350 END PROC
11360 ' did the bullet at (TX,TY) hit a fruit of the formation?
11370 PROC INVADERHIT(I,TX,TY)
11380   LOCAL C,RW,RX,RY
11390   C=(TX-FX)\SX
11400   RX=TX-FX-C*SX
11410   RW=(TY-FY)\SY+1
11420   RY=TY-FY-(RW-1)*SY
11430   IF C<=7 AND RW<=5 AND RX<16 AND RY>=3 AND RY<21 THEN
11440     IF IV(RW,C)=1 THEN CALL SPLAT(I,RW,C)
11450   END IF
11460 END PROC
11470 PROC SPLAT(I,RW,C)
11480   LOCAL X,Y
11490   IV(RW,C)=0
11500   NA=NA-1
11510   SC=SC+RP(RW)
11520   SPRITE (RW-1)*8+C,OFF
11530   X=FX+C*SX+8:Y=FY+(RW-1)*SY+12
11540   CALL BOOM(X,Y,RW-1)
11550   SFX 2
11560   CALL KILLBALL(I)
11570   CALL SCORED
11580   SP=NA\2+2-WV
11590   IF SP<2 THEN SP=2
11600   ' now and then a banana falls out
11610   IF PU=0 AND FN RN(9)=0 THEN CALL BANANA(X,Y)
11620 END PROC
11630 ' score changed: an extra life every 1500 points, the high score, the score line needs to be drawn again
11640 PROC SCORED
11650   IF SC>=NX THEN
11660     LV=LV+1:NX=NX+1500
11670     SFX 5
11680   END IF
11690   IF SC>HI THEN HI=SC
11700   HF=1
11710 END PROC
11720 ' ====================================================================== the bananas
11730 PROC BANANA(X,Y)
11740   PU=1:PUX=X-8:PUY=Y-12
11750   SPRITE 49,PUX,PUY,22
11760   SPRITE 49,ANIM 3,2,8
11770   SPRITE 49,ON
11780   SFX 10
11790 END PROC
11800 PROC POWERUP
11810   IF PU=0 THEN EXIT PROC
11820   PUY=PUY+2
11830   SPRITE 49,PUX,PUY
11840   IF PUY>PY-14 AND PUY<PY+16 AND PUX>PX-12 AND PUX<PX+12 THEN
11850     PU=0
11860     SPRITE 49,OFF
11870     RAPID=600
11880     SC=SC+100
11890     SFX 9
11900     CALL BOOM(PX+8,PY+8,-1)
11910     CALL SCORED
11920     EXIT PROC
11930   END IF
11940   IF PUY>284 THEN
11950     PU=0
11960     SPRITE 49,OFF
11970   END IF
11980 END PROC
11990 ' ====================================================================== the seeds the fruit spits
12000 PROC SEEDS
12010   LOCAL I
12020   FOR I=0 TO 3
12030     IF SEEDON(I)=1 THEN CALL SEEDMOVE(I)
12040   NEXT I
12050   IF FN RN(BR)=0 THEN CALL SPAWN
12060 END PROC
12070 PROC SEEDMOVE(I)
12080   LOCAL TX,TY,J
12090   SEEDY(I)=SEEDY(I)+BS
12100   IF SEEDY(I)>268 THEN
12120     CALL KILLSEED(I)
12130     EXIT PROC
12140   END IF
12150   SPRITE 44+I,SEEDX(I),SEEDY(I)
12160   TX=SEEDX(I)+7:TY=SEEDY(I)+15
12170   IF TY<286 AND POINT(TX,TY)>0 THEN
12180     CALL BUSHHIT(TX,TY)
12190     CALL KILLSEED(I)
12200     EXIT PROC
12210   END IF
12220   IF TY>=PY+4 AND TX>=PX+3 AND TX<=PX+12 THEN
12230     CALL GARDENERHIT
12240     EXIT PROC
12250   END IF
12260   ' a ball that meets a seed destroys it
12270   FOR J=0 TO 1
12280     IF GA(J)=1 AND TX>=GX(J)+4 AND TX<=GX(J)+11 AND TY>=GY(J)+6 AND TY<=GY(J)+18 THEN
12290       CALL KILLSEED(I)
12300       CALL KILLBALL(J)
12310       CALL BOOM(TX,TY,-1)
12320       SFX 6
12330       EXIT PROC
12340     END IF
12350   NEXT J
12360 END PROC
12420 PROC KILLSEED(I)
12430   SEEDON(I)=0
12440   SPRITE 44+I,OFF
12450 END PROC
12460 ' a new seed from the lowest fruit of a random column that still has one
12470 PROC SPAWN
12480   LOCAL J,Z,C,RW
12490   J=-1
12500   FOR Z=0 TO 3
12510     IF SEEDON(Z)=0 THEN J=Z
12520   NEXT Z
12530   IF J<0 THEN EXIT PROC
12540   C=FN RN(8)
12550   FOR Z=1 TO 8
12560     RW=5
12570     WHILE IV(RW,C)=0 AND RW>0
12580       RW=RW-1
12590     WEND
12600     IF RW>0 THEN
12610       SEEDON(J)=1:SEEDX(J)=FX+C*SX:SEEDY(J)=FY+(RW-1)*SY+12
12620       SPRITE 44+J,SEEDX(J),SEEDY(J),19
12630       SPRITE 44+J,ANIM 2,4,4
12640       SPRITE 44+J,ON
12650       SFX 7
12660       EXIT PROC
12670     END IF
12680     C=C+1
12690     IF C>7 THEN C=0
12700   NEXT Z
12710 END PROC
12720 ' a hit on a bush at (TX,TY): a round hole and a few leaves that fall
12730 PROC BUSHHIT(TX,TY)
12740   LOCAL Z,XX,YY
12750   CALL DISK(TX,TY,4,0)
12760   FOR Z=1 TO 6
12770     XX=TX-7+FN RN(14)
12780     YY=TY-7+FN RN(14)
12790     GFX PSET XX,YY,0
12800   NEXT Z
12810   CALL LEAVES(TX,TY)
12820   SFX 6
12830 END PROC
12840 ' ====================================================================== the bird
12850 PROC BIRD
12860   IF UA=1 THEN
12870     UX=UX+UD*2
12880     UY=14+INT(5*SIN(UX/22))
12890     SPRITE 48,UX,UY
12900     IF UX<-20 OR UX>640 THEN CALL BIRDGONE
12910     EXIT PROC
12920   END IF
12930   IF BIRDREADY=1 AND NA>6 THEN CALL BIRDSTART
12940 END PROC
12950 PROC BIRDSTART
12960   BIRDREADY=0
12970   UA=1
12980   IF FN RN(2)=0 THEN
12990     UX=-16:UD=1
13000   ELSE
13010     UX=640:UD=-1
13020   END IF
13030   SPRITE 48,UX,UY,10
13040   SPRITE 48,ANIM 1,4,2
13050   SPRITE 48,ON
13060   WAVE 2,0:VIBRATO 2,14,6:SOUND 2,1800,-1,0.06
13070 END PROC
13080 PROC BIRDGONE
13090   UA=0
13100   SPRITE 48,OFF
13110   SOUND OFF 2
13120   AFTER 700+FN RN(600),2 GOSUB BIRDTIMER
13130 END PROC
13140 PROC BIRDHIT(I)
13150   LOCAL P
13160   P=SV(FN RN(4))
13170   SC=SC+P
13180   CALL BOOM(UX+8,UY+12,4)
13190   CALL BOOM(UX+8,UY+12,-1)
13200   CALL KILLBALL(I)
13210   UA=0
13220   SPRITE 48,OFF
13230   SOUND OFF 2
13240   SFX 4
13250   AFTER 700+FN RN(600),2 GOSUB BIRDTIMER
13260   CALL SCORED
13270 END PROC
13280 ' ====================================================================== the formation
13290 PROC FORM
13300   ST=ST+1
13310   IF ST<SP THEN EXIT PROC
13320   ST=0
13330   CALL MARCH
13340 END PROC
13350 PROC MARCH
13360   LOCAL C,RW,MN,MX,LR,N
13370   MN=8:MX=-1:LR=0
13380   FOR C=0 TO 7
13390     FOR RW=1 TO 5
13400       IF IV(RW,C)=1 THEN
13410         IF MN=8 THEN MN=C
13420         MX=C
13430         IF RW>LR THEN LR=RW
13440       END IF
13450     NEXT RW
13460   NEXT C
13470   IF (DX=1 AND FX+MX*SX+16+SD>630) OR (DX=-1 AND FX+MN*SX-SD<8) THEN
13480     DX=0-DX:FY=FY+10
13490   ELSE
13500     FX=FX+DX*SD
13510   END IF
13520   FA=1-FA
13530   ' the rustle of the leaves (the formation steps faster when fewer fruits are left)
13540   SFX 12+FA
13550   ' the fruit slide to the new place in 3 frames and show the other picture
13560   FOR RW=1 TO 5
13570     FOR C=0 TO 7
13580       IF IV(RW,C)=1 THEN
13590         N=(RW-1)*8+C
13600         SPRITE N,IMAGE IMG(RW)+FA
13610         SPRITE N,MOVETO FX+C*SX,FY+(RW-1)*SY,3
13620       END IF
13630     NEXT C
13640   NEXT RW
13650   IF FY+(LR-1)*SY+20>=SHY-2 THEN
13660     OV=1:LANDED=1
13670   END IF
13680 END PROC
13690 ' ====================================================================== explosions and flying bits
13700 ' four leaves that fly up from (X,Y) and sink down again (the same sprites and timers as the bits of an explosion)
13710 PROC LEAVES(X,Y)
13720   LOCAL I,A,D,S
13730   FOR I=0 TO 3
13740     S=NP
13750     NP=(NP+1) MOD 64
13760     A=3.14+FN RN(314)/100
13770     D=8+FN RN(14)
13780     SPRITE 64+S,X-8,Y-12,40
13790     SPRITE 64+S,ON
13800     SPRITE 64+S,MOVETO INT(X-8+COS(A)*D),INT(Y-12+SIN(A)*D+16),24
13810     PL(S)=28
13820   NEXT I
13830 END PROC
13840 ' an explosion with 10 flying bits at (X,Y); K = the fruit (0..4, the bits get its colors) or -1 for sparks
13850 PROC BOOM(X,Y,K)
13860   LOCAL I,A,D,S,E,M
13870   E=-1
13880   FOR I=0 TO 3
13890     IF EP(I)=0 THEN
13900       E=I
13910       EXIT
13920     END IF
13930   NEXT I
13940   IF E>=0 THEN
13950     EP(E)=14
13960     SPRITE 50+E,X-8,Y-12,24
13970     SPRITE 50+E,ANIM 5,4,3
13980     SPRITE 50+E,ON
13990   END IF
14000   FOR I=0 TO 9
14010     S=NP
14020     NP=(NP+1) MOD 64
14030     A=I*0.628+FN RN(100)/160
14040     D=14+FN RN(26)
14050     M=28+K
14060     IF K<0 THEN M=33
14070     SPRITE 64+S,X-8,Y-12,M
14080     SPRITE 64+S,ON
14090     SPRITE 64+S,MOVETO INT(X-8+COS(A)*D),INT(Y-12+SIN(A)*D),18
14100     PL(S)=26
14110   NEXT I
14120 END PROC
14130 ' counts the explosions and the bits down and hides them when they are over
14140 PROC EFFECTS
14150   LOCAL K,V
14160   FOR EACH K,V IN EP()
14170     IF V=0 THEN ITERATE
14180     EP(K)=V-1
14190     IF V=1 THEN SPRITE 50+K,OFF
14200   NEXT
14210   FOR EACH K,V IN PL()
14220     IF V=0 THEN ITERATE
14230     PL(K)=V-1
14240     IF V=1 THEN SPRITE 64+K,OFF
14250   NEXT
14260 END PROC
14270 ' the gardener was hit: his hat flies off, the sky flashes, the clouds shake
14280 PROC GARDENERHIT
14290   LOCAL Z,HX
14300   LV=LV-1
14310   SFX 3
14320   CALL KILLBALL(0):CALL KILLBALL(1)
14330   FOR Z=0 TO 3
14340     CALL KILLSEED(Z)
14350   NEXT Z
14360   ' ouch: the hatless picture, the hat flies up and then falls to the grass, a few sparks, the sky flashes, the clouds shake
14370   SPRITE 40,IMAGE 36
14380   HX=FN CLAMP(PX+FN RN(80)-40,0,620)
14390   SPRITE 41,PX,PY-5,35:SPRITE 41,ON
14400   SPRITE 41,MOVETO HX,PY-75,24
14410   CALL BOOM(PX+8,PY+6,-1)
14420   CALL SKY(1)
14430   FOR Z=1 TO 70
14440     IF Z=6 THEN CALL SKY(0)
14450     IF Z=26 THEN SPRITE 41,MOVETO HX,278,26
14460     TILE SCROLL FN CLAMP(CSX+FN RN(5)-2,0,15),FN RN(4)
14470     IF Z=30 THEN CALL BOOM(PX+8,PY+6,-1)
14480     CALL EFFECTS
14490     GFX VSYNC
14500   NEXT Z
14510   SPRITE 41,OFF
14520   TILE SCROLL CSX,0
14530   IF LV<=0 THEN
14540     OV=1
14550     EXIT PROC
14560   END IF
14570   PX=312:PD=0:RAPID=0:GS=0:TP=0
14580   SPRITE 40,PX,PY,13:SPRITE 40,ON
14590   HF=1
14600 END PROC
14610 ' ====================================================================== waves
14620 PROC NEWWAVE
14630   LOCAL RW,C,Q,N,I
14640   FOR RW=1 TO 5
14650     FOR C=0 TO 7
14660       IV(RW,C)=1
14670     NEXT C
14680   NEXT RW
14690   NA=FN COUNTALIVE(IV())
14700   Q=FN CLAMP(WV-1,0,4)
14710   FX=40:FY=34+Q*8:DX=1:SD=6
14720   SP=FN CLAMP(22-WV,10,22)
14730   ST=0:FA=0
14740   BS=FN CLAMP(2+WV\2,2,5)
14750   BR=FN CLAMP(50-WV*5,14,50)
14760   GFX CLEAR
14770   CALL GROUND
14780   CALL BUSHES
14790   FOR I=0 TO 3
14800     SEEDON(I)=0:SPRITE 44+I,OFF
14810   NEXT I
14820   FOR I=0 TO 1
14830     GA(I)=0:SPRITE 42+I,OFF
14840   NEXT I
14850   UA=0:BIRDREADY=0:SPRITE 48,OFF:SOUND OFF 2
14860   PU=0:SPRITE 49,OFF
14870   AFTER 500+FN RN(600),2 GOSUB BIRDTIMER
14880   ' the fruit fly in from above
14890   SFX 11
14900   FOR RW=1 TO 5
14910     FOR C=0 TO 7
14920       N=(RW-1)*8+C
14930       SPRITE N,FX+C*SX,FY+(RW-1)*SY-190,IMG(RW)
14940       SPRITE N,ON
14950       SPRITE N,MOVETO FX+C*SX,FY+(RW-1)*SY,70
14960     NEXT C
14970   NEXT RW
14980   PX=312:PD=0:FQ=0:RAPID=0
14990   GS=0:TP=0
15000   SPRITE 41,OFF
15010   SPRITE 40,PX,PY,13:SPRITE 40,ON
15020   CALL HUD
15030   CALL BIGTEXT(120,"WAVE "+STR$(WV),12,5)
15040   CALL WAITFRAMES(80)
15050   GFX LINE 60,112,580,190,0,BF
15060   CALL STARTMUSIC
15070 END PROC
15080 PROC WAITFRAMES(N)
15090   LOCAL Z
15100   FOR Z=1 TO N
15110     GFX VSYNC
15120     CALL CLOUDS
15130     CALL EFFECTS
15140   NEXT Z
15150 END PROC
15160 PROC WAVEDONE
15170   EVERY OFF
15180   SOUND OFF
15190   PLAY "T200 W1 V0.3 O5 C8 E8 G8 >C4","T200 W2 V0.25 O4 C8 E8 G8 >C4","T200 W1 V0.15 O3 C4 G4"
15200   SC=SC+100
15210   CALL SCORED
15220   CALL BIGTEXT(110,"WAVE CLEARED",13,4)
15230   CALL BIGTEXT(160,"BONUS 100",12,3)
15240   CALL WAITFRAMES(100)
15250   GFX LINE 60,100,580,200,0,BF
15260   WV=WV+1
15270   CALL NEWWAVE
15280 END PROC
15290 ' ====================================================================== the music: two voices on the channels 3 and 4, a bar after the other
15300 ' the notes of the array NT(), N of them, one after the other on channel CH, BT beats each, volume VL
15310 ' one bar (4 beats): an arpeggio of the chord on channel 3 and two half notes of bass on channel 4
15320 PROC BAR
15330   LOCAL I,TA(8)
15340   FOR I=0 TO 7
15350     TA(I)=CH3(BI*3+PAT(I))
15360   NEXT I
15370   CALL PLAYSEQ(TA(),8,3,0.5,0.07)
15380   SOUND 4,BASSF(BI),2,0.2
15390   SOUND 4,BASSF(BI),2,0.2
15400   BI=(BI+1) MOD 4
15410 END PROC
15420 ' starts the music for the wave: the tempo goes up with the wave, the bars are timed by EVERY (a little longer than a bar)
15430 PROC STARTMUSIC
15440   LOCAL TB
15450   TB=FN CLAMP(112+WV*6,112,176)
15460   TEMPO TB
15470   WAVE 3,1:WAVE 4,2
15480   BI=0
15490   CALL BAR
15500   EVERY INT(14400/TB)+2,1 GOSUB BARTIMER
15510 END PROC
15520 ' ====================================================================== title and game over
15530 PROC TITLE
15540   LOCAL I,T,K$,F
15550   CALL CLEARALL
15560   CALL BIGTEXT(14,"FRUIT",10,8)
15570   CALL BIGTEXT(112,"INVADERS II",12,4)
15580   FOR I=0 TO 5
15590     SPRITE I,ON
15600   NEXT I
15610   FOR I=0 TO 5
15620     CALL TXTAT(12+I*11,17,MID$("50  40  30  20  10  50+",I*4+1,4),205)
15630   NEXT I
15640   CALL TXTAT(18,19,"HIGH SCORE "+FN PAD$(HI,6),201)
15650   CALL TXTAT(10,21,"LEFT / RIGHT or A / D walk   SPACE throw   P pause   Q quit",200)
15660   CALL TXTAT(10,22,"Catch the banana for rapid fire!",202)
15670   CALL TXTAT(10,23,"Controller: stick / D-pad walk, A throw, START pause, BACK quit",206)
15680   CALL TXTAT(17,24,"Press SPACE or A to start",203)
15690   PLAY "T150 W1 V0.3 O4 E8 G8 >C8 E4 D8 C8 <B8 >C4","T150 W2 V0.25 O3 C4 G4 C4 G4","T150 W1 V0.12 O5 C16 E16 G16 >C16 C16 E16 G16 >C16"
15700   QT=0
15710   CALL IN_WAITRELEASE
15720   REPEAT
15730     GFX VSYNC
15740     T=T+1
15750     IF T MOD 20=0 THEN F=1-F
15760     ' the title shimmers through the colors, the fruit bob up and down and dance
15770     GFX PALETTE 3,10,RGB(INT(180+75*SIN(T/10)),INT(180+75*SIN(T/10+2.1)),INT(180+75*SIN(T/10+4.2)))
15780     FOR I=0 TO 4
15790       SPRITE I,96+I*88,166+INT(7*SIN(T/9+I)),IMG(I+1)+F
15800     NEXT I
15810     SPRITE 5,96+5*88,166+INT(7*SIN(T/9+5)),10+F
15820     CALL CLOUDS
15830     K$=INKEY$
15840   UNTIL K$=" " OR K$="q" OR K$="Q" OR JOY("A") OR JOY("START") OR JOY("BACK")
15850   IF K$="q" OR K$="Q" OR JOY("BACK") THEN QT=1
15860 END PROC
15870 PROC PLAYGAME
15880   SC=0:LV=3:WV=1:NX=1500:OV=0:LANDED=0:QT=0:IN_QUIT=0
15890   BI=0:NP=0
15900   CALL CLEARALL
15910   CALL NEWWAVE
15920   WHILE OV=0
15930     CALL KEYS
15940     IF QT=1 THEN OV=1
15950     CALL MOVEGARDENER
15960     CALL BALLS
15970     CALL SEEDS
15980     CALL BIRD
15990     CALL POWERUP
16000     CALL FORM
16010     CALL EFFECTS
16020     CALL CLOUDS
16030     IF HF=1 THEN CALL HUD
16040     IF NA=0 AND OV=0 THEN CALL WAVEDONE
16050     GFX VSYNC
16060   WEND
16070   EVERY OFF
16080   AFTER OFF
16090   CALL OVER
16100 END PROC
16110 PROC OVER
16120   LOCAL I,Z,K$
16130   SOUND OFF
16140   FOR I=0 TO 127
16150     SPRITE I,OFF
16160   NEXT I
16170   CALL BIGTEXT(100,"GAME OVER",12,5)
16180   IF LANDED=1 THEN CALL TXTAT(29,16,"The fruit has landed!",207)
16190   CALL TXTAT(31,18,"Score "+STR$(SC),200)
16200   IF SC>=HI AND SC>0 THEN CALL TXTAT(29,20,"A NEW HIGH SCORE!",201)
16210   PLAY "T100 W2 V0.5 O4 C4 <B4 A4 G2","T100 W1 V0.2 O3 C4 G4 E4 C2"
16220   FOR Z=1 TO 170
16230     GFX VSYNC
16240     CALL CLOUDS
16250     CALL EFFECTS
16260     K$=INKEY$
16270   NEXT Z
16280 END PROC
16290 ' ====================================================================== the data
16300 ' fruit window, first image, points for the rows 1 to 5; the bass of the music and the points of the bird
16310 LABEL FRUITDATA
16320 DATA 16,0,50, 32,2,40, 48,4,30, 64,6,20, 80,8,10
16330 DATA 110,50, 87,100, 131,150, 98,300
16340 ' the chords (Am, F, C, G) as three notes each, and the order of an arpeggio
16350 DATA 220,262,330, 175,220,262, 262,330,392, 196,247,294
16360 DATA 0,1,2,1,0,1,2,1
16370 LABEL PALDATA
16380 DATA 16,60,0,15,150,10,30,215,25,45,245,70,80,255,160,160,40,90,20,70,150,40,120,200,70,255,255,255,20,20,40,255,120,140,255,230,230
16390 DATA 32,70,0,20,170,15,40,230,40,55,255,90,90,255,175,175,30,100,30,60,160,50,130,210,80,255,255,255,20,20,40,255,150,150,255,235,120
16400 DATA 48,20,60,15,50,130,40,90,190,50,150,225,80,225,255,170,90,55,25,50,140,40,110,200,70,255,255,255,20,30,20,240,120,120,200,255,120
16410 DATA 64,110,45,0,210,100,0,255,150,10,255,190,60,255,235,170,30,90,20,60,150,40,110,200,60,255,255,255,40,20,0,255,110,90,190,80,0
16420 DATA 80,110,90,0,210,180,0,255,230,20,255,245,100,255,255,200,30,100,30,60,160,50,130,210,80,255,255,255,40,30,0,255,150,100,190,150,0
16430 DATA 96,30,45,80,70,120,210,125,175,245,235,242,255,50,90,170,25,40,90,255,170,30,255,255,255,10,10,20,40,70,140,255,150,150,255,230,100
16440 DATA 112,60,40,30,230,195,95,190,150,60,245,200,160,210,155,115,120,180,245,70,125,205,50,85,160,30,55,115,120,75,35,255,255,255,35,30,40
16450 DATA 128,110,20,20,235,50,50,255,130,130,255,255,255,20,40,120,60,110,240,150,190,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
16460 DATA 144,50,30,10,120,75,30,190,140,70,245,230,200,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
16470 DATA 160,100,70,0,210,170,0,255,225,30,255,245,120,255,255,255,90,50,10,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
16480 DATA 176,90,10,0,200,50,0,255,120,0,255,190,30,255,240,150,255,255,255,150,150,150,90,90,90,255,255,255,255,255,255,255,255,255,255,255,255
16490 DATA 192,255,255,255,205,220,240,170,195,225,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
16500 DATA 208,30,110,45,70,180,60,140,230,90,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
