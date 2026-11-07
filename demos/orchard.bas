1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' ORCHARD DEFENSE - the simple edition: an easy to read game for Engine1999 (see orchard2 and orchard3 for the bigger ones).
20 ' A gardener walks on the grass under a blue sky and throws balls at the fruit that marches down from the trees.
30 ' The fruit spits seeds back. Round bushes protect the gardener and lose their leaves where a seed or a ball hits.
40 ' Keys: LEFT / RIGHT or A / D walk (only while the key is held), SPACE or UP throws a ball (hold it for rapid fire), P = pause, Q = quit.
50 ' Game controller (optional, JOY): the D-pad or the left stick walks, A or the right trigger throws, START pauses, BACK quits.
60 ' Cherries are worth 30 points, apples 20, oranges 10. Clear the field before the fruit reaches the bushes.
70 ' How it is made (read the program from the top; every part is a LABEL that is called with GOSUB):
80 '   the sky is the background layer (palette entries drawn as bands), the clouds are tiles (layer 1), the grass and the bushes are
90 '   drawn on the graphics layer (layer 3, POINT tells what is there), the pictures are DESIGN rows (sprites, layer 4),
100 '   the sounds are SFX effects, the rustling of the leaves is noise.
110 GOTO MAIN
120 ' ---------------------------------------------------------------------------------------------- random numbers
130 ' RNG: R = a random number 0 .. RM-1
140 LABEL RNG
150 R=INT(RND(1)*RM)
160 RETURN
170 ' ---------------------------------------------------------------------------------------------- a filled disc (used for bushes, hills and the sun)
180 ' DISC: center CX,CY, radius CR, color CC on the target layer; one horizontal line per row
190 LABEL DISC
200 FOR DY=-CR TO CR
210   HW=INT(SQR(CR*CR-DY*DY)+0.5)
220   GFX LINE CX-HW,CY+DY,CX+HW,CY+DY,CC
230 NEXT DY
240 RETURN
250 ' ---------------------------------------------------------------------------------------------- set up (once)
260 LABEL INIT
270 RANDOMIZE
280 ' (DIM n makes n elements, 0 to n-1)
290 DIM A(6,8),SEEDX(3),SEEDY(3),SEEDON(3),IM(6),ET(2)
300 IM(1)=0:IM(2)=2:IM(3)=2:IM(4)=4:IM(5)=4
310 ' SX,SY = distance of the fruit in the formation, SHY = top of the bushes, PY = top of the gardener
320 SX=36:SY=26:SHY=216:PY=264
330 HI=0
340 ' sounds: 1 throw, 2 splat, 3 gardener hit, 5 extra life, 6 seed in a bush, 7 and 8 the rustle of the marching fruit
350 SFX 1,"W3 V0.25 ML X0,0.01,0.3,0.08 S1200,3200,8"
360 SFX 2,"W3 V0.5 ML X0,0.01,0.3,0.1 S900,150,10"
370 SFX 3,"W0 V0.9 ML X0,0.01,0.5,0.3 S160,40,10"
380 SFX 5,"W2 V0.5 ML ~7,0.3 S300,1500,4"
390 SFX 6,"W3 V0.35 ML X0,0.005,0.2,0.04 S500,90,32"
400 SFX 7,"W3 V0.2 ML X0.005,0.02,0.3,0.06 S5000,3500,6"
410 SFX 8,"W3 V0.2 ML X0.005,0.02,0.3,0.06 S3800,2800,6"
420 ' colors of the sprites (layer 4): the fruit, the gardener and the splashes
430 GFX PALETTE 4,1,RGB(255,50,50):GFX PALETTE 4,2,RGB(90,200,60):GFX PALETTE 4,3,RGB(30,130,40)
440 GFX PALETTE 4,4,RGB(255,150,0):GFX PALETTE 4,5,RGB(255,230,0):GFX PALETTE 4,6,RGB(255,255,255)
450 GFX PALETTE 4,7,RGB(140,90,40):GFX PALETTE 4,8,RGB(150,160,170):GFX PALETTE 4,9,RGB(40,40,50)
460 GFX PALETTE 4,10,RGB(225,230,235):GFX PALETTE 4,11,RGB(240,190,150):GFX PALETTE 4,12,RGB(110,170,240)
470 GFX PALETTE 4,13,RGB(225,190,90):GFX PALETTE 4,15,RGB(40,70,140)
480 ' colors of the graphics layer (3): the bushes and the grass
490 GFX PALETTE 3,1,RGB(30,110,40):GFX PALETTE 3,2,RGB(50,150,50):GFX PALETTE 3,3,RGB(110,200,80)
500 GFX PALETTE 3,4,RGB(70,170,50):GFX PALETTE 3,5,RGB(40,120,30):GFX PALETTE 3,6,RGB(240,70,90)
510 GFX PALETTE 3,7,RGB(255,230,60):GFX PALETTE 3,8,RGB(255,255,255)
520 ' colors of the tiles (layer 1): the clouds
530 GFX PALETTE 1,1,RGB(255,255,255):GFX PALETTE 1,2,RGB(200,215,235)
540 GFX BACKDROP RGB(110,180,255)
550 GOSUB SKY
560 DESIGN SPRITE 0
570 FOR R=1 TO 5:@ "................":NEXT R
580 @ "........7.33...."
590 @ ".......7.333...."
600 @ "......7..33....."
610 @ ".....7...7......"
620 @ "....7.....7....."
630 @ "...7.......7...."
640 @ "..1111....1111.."
650 @ ".111111..111111."
660 @ ".116111..116111."
670 @ ".111111..111111."
680 @ ".111111..111111."
690 @ "..1111....1111.."
700 FOR R=1 TO 7:@ "................":NEXT R
710 DESIGN SPRITE 1
720 FOR R=1 TO 5:@ "................":NEXT R
730 @ "....33.7........"
740 @ "....333.7......."
750 @ ".....33..7......"
760 @ "......7...7....."
770 @ ".....7.....7...."
780 @ "....7.......7..."
790 @ "..1111....1111.."
800 @ ".111111..111111."
810 @ ".111611..111611."
820 @ ".111111..111111."
830 @ ".111111..111111."
840 @ "..1111....1111.."
850 FOR R=1 TO 7:@ "................":NEXT R
860 DESIGN SPRITE 2
870 FOR R=1 TO 4:@ "................":NEXT R
880 @ ".......7..33...."
890 @ ".......7.333...."
900 @ "...2222.72222..."
910 @ "..222222222222.."
920 @ ".22622222222222."
930 @ ".22622222222222."
940 FOR R=1 TO 3:@ ".22222222222222.":NEXT R
950 @ "..222222222222.."
960 @ "..222222222222.."
970 @ "...2222..2222..."
980 FOR R=1 TO 8:@ "................":NEXT R
990 DESIGN SPRITE 3
1000 FOR R=1 TO 4:@ "................":NEXT R
1010 @ "....33..7......."
1020 @ "....333.7......."
1030 @ "...22227.2222..."
1040 @ "..222222222222.."
1050 @ ".22222222222622."
1060 @ ".22222222222622."
1070 FOR R=1 TO 3:@ ".22222222222222.":NEXT R
1080 @ "..222222222222.."
1090 @ "..222222222222.."
1100 @ "...2222..2222..."
1110 FOR R=1 TO 8:@ "................":NEXT R
1120 DESIGN SPRITE 4
1130 FOR R=1 TO 5:@ "................":NEXT R
1140 @ ".......3.33....."
1150 @ "......333......."
1160 @ "....44444444...."
1170 @ "...4444444444..."
1180 @ "..444644444444.."
1190 @ ".44464444444444."
1200 FOR R=1 TO 3:@ ".44444444444444.":NEXT R
1210 @ "..444444444444.."
1220 @ "...4444444444..."
1230 @ "....44444444...."
1240 FOR R=1 TO 7:@ "................":NEXT R
1250 DESIGN SPRITE 5
1260 FOR R=1 TO 5:@ "................":NEXT R
1270 @ ".....33.3......."
1280 @ ".......333......"
1290 @ "....44444444...."
1300 @ "...4444444444..."
1310 @ "..444444446444.."
1320 @ ".44444444446444."
1330 FOR R=1 TO 3:@ ".44444444444444.":NEXT R
1340 @ "..444444444444.."
1350 @ "...4444444444..."
1360 @ "....44444444...."
1370 FOR R=1 TO 7:@ "................":NEXT R
1380 ' image 6: the gardener (straw hat, blue shirt, dark blue overalls, brown boots)
1390 DESIGN SPRITE 6
1400 @ "................"
1410 @ "......DDDD......"
1420 @ "....DDDDDDDD...."
1430 @ "..DDDDDDDDDDDD.."
1440 @ "....BBBBBBBB...."
1450 @ "....B9BBBB9B...."
1460 @ "....BBBBBBBB...."
1470 @ "....BBB11BBB...."
1480 @ ".....BBBBBB....."
1490 @ "...CCCCCCCCCC..."
1500 @ "..CCCCCCCCCCCC.."
1510 @ "..CCCCCCCCCCCC.."
1520 @ "..BCFFFFFFFFCB.."
1530 @ "..B.FFFFFFFF.B.."
1540 @ "..B.FFFFFFFF.B.."
1550 @ "....FFFFFFFF...."
1560 @ "....FFFFFFFF...."
1570 FOR R=1 TO 4:@ "....FFF..FFF....":NEXT R
1580 @ "....777..777...."
1590 @ "...7777..7777..."
1600 @ "................"
1610 ' image 7: the ball
1620 DESIGN SPRITE 7
1630 FOR R=1 TO 8:@ "................":NEXT R
1640 @ "......1111......"
1650 @ ".....111111....."
1660 @ ".....116111....."
1670 @ ".....111111....."
1680 @ "......1111......"
1690 FOR R=1 TO 11:@ "................":NEXT R
1700 DESIGN SPRITE 8
1710 FOR R=1 TO 8:@ "................":NEXT R
1720 @ ".......77......."
1730 FOR R=1 TO 3:@ "......7777......":NEXT R
1740 FOR R=1 TO 3:@ ".......77.......":NEXT R
1750 @ ".......7........"
1760 FOR R=1 TO 8:@ "................":NEXT R
1770 DESIGN SPRITE 10
1780 FOR R=1 TO 8:@ "................":NEXT R
1790 @ "......5..5......"
1800 @ "....5..55..5...."
1810 @ ".....5.55.5....."
1820 @ "...55.6666.55..."
1830 @ ".5555.6666.5555."
1840 @ "...55.6666.55..."
1850 @ ".....5.55.5....."
1860 @ "....5..55..5...."
1870 @ "......5..5......"
1880 FOR R=1 TO 7:@ "................":NEXT R
1890 DESIGN SPRITE 11
1900 FOR R=1 TO 6:@ "................":NEXT R
1910 @ "......4..4......"
1920 @ "..4....44....4.."
1930 @ "...4.5.55.5.4..."
1940 @ "....5.5555.5...."
1950 @ ".4.55.6666.55.4."
1960 @ "..5555666655555."
1970 @ "4.555.6666.555.4"
1980 @ "..5555666655555."
1990 @ ".4.55.6666.55.4."
2000 @ "....5.5555.5...."
2010 @ "...4.5.55.5.4..."
2020 @ "..4....44....4.."
2030 @ "......4..4......"
2040 FOR R=1 TO 5:@ "................":NEXT R
2050 ' tiles 1 to 3: the left, middle and right part of a cloud (16 x 24 pixels each)
2060 DESIGN TILE 1
2070 FOR R=1 TO 9:@ "................":NEXT R
2080 @ "........1111...."
2090 @ "......11111111.."
2100 @ ".....1111111111."
2110 @ "....111111111111"
2120 @ "...1111111111111"
2130 @ "...1111111111111"
2140 @ "..11111111111111"
2150 @ "..11111111111111"
2160 @ ".111111111111111"
2170 @ ".111111111111111"
2180 @ "1111111111111111"
2190 @ ".111111111111111"
2200 @ "..22222222222222"
2210 @ "...2222222222222"
2220 @ "................"
2230 DESIGN TILE 2
2240 FOR R=1 TO 4:@ "................":NEXT R
2250 @ "......1111......"
2260 @ "....11111111...."
2270 @ "...1111111111..."
2280 @ "..111111111111.."
2290 @ ".11111111111111."
2300 @ ".11111111111111."
2310 FOR R=1 TO 11:@ "1111111111111111":NEXT R
2320 @ "2222222222222222"
2330 @ "2222222222222222"
2340 @ "................"
2350 DESIGN TILE 3
2360 FOR R=1 TO 10:@ "................":NEXT R
2370 @ ".11111111......."
2380 @ "11111111111....."
2390 @ "111111111111...."
2400 @ "1111111111111..."
2410 @ "1111111111111..."
2420 @ "11111111111111.."
2430 @ "11111111111111.."
2440 @ "111111111111111."
2450 @ "111111111111111."
2460 @ "1111111111111111"
2470 @ "11111111111111.."
2480 @ "22222222222222.."
2490 @ "222222222222...."
2500 @ "................"
2510 ' the clouds: TILE SET x,y,tile puts a tile into the cell (x 0..40, y 0..12)
2520 FOR I=0 TO 4
2530   READ CX,CY
2540   TILE SET CX,CY,1:TILE SET CX+1,CY,2:TILE SET CX+2,CY,3
2550 NEXT I
2560 DATA 2,2, 12,6, 21,1, 30,5, 35,2
2570 RETURN
2580 ' ---------------------------------------------------------------------------------------------- the sky (background layer 0, drawn once)
2590 LABEL SKY
2600 ' twelve bands from deep blue at the top to pale blue at the horizon: palette entries 10..21 of layer 0
2610 FOR I=0 TO 11
2620   GFX PALETTE 0,10+I,RGB(70+I*9,150+I*6,240+I)
2630 NEXT I
2640 GFX PALETTE 0,22,RGB(130,200,140):GFX PALETTE 0,23,RGB(90,170,100)
2650 GFX PALETTE 0,24,RGB(255,250,190):GFX PALETTE 0,25,RGB(255,235,110)
2660 GFX TARGET 0
2670 FOR I=0 TO 11
2680   GFX LINE 0,I*25,639,I*25+24,10+I,BF
2690 NEXT I
2700 ' the sun, then the hills (discs that are mostly below the grass)
2710 CC=24:CX=585:CY=62:CR=36:GOSUB DISC
2720 CC=25:CR=28:GOSUB DISC
2730 CC=22:CX=90:CY=360:CR=100:GOSUB DISC
2740 CX=430:CY=372:CR=112:GOSUB DISC
2750 CX=620:CY=350:CR=82:GOSUB DISC
2760 CC=23:CX=260:CY=378:CR=102:GOSUB DISC
2770 GFX TARGET 3
2780 RETURN
2790 ' ---------------------------------------------------------------------------------------------- the screen
2800 LABEL CLEARALL
2810 SOUND OFF
2820 CLS
2830 GFX CLEAR
2840 FOR I=0 TO 48
2850   SPRITE I,OFF
2860 NEXT I
2870 GOSUB GROUND
2880 RETURN
2890 ' the grass strip at the bottom of the screen, on the graphics layer, with a few flowers
2900 LABEL GROUND
2910 GFX LINE 0,286,639,299,4,BF
2920 GFX LINE 0,286,639,287,3,BF
2930 FOR I=1 TO 90
2940   RM=640:GOSUB RNG:XX=R
2950   RM=11:GOSUB RNG:YY=288+R
2960   GFX PSET XX,YY,5
2970 NEXT I
2980 FOR I=1 TO 40
2990   RM=640:GOSUB RNG:XX=R
3000   RM=10:GOSUB RNG:YY=289+R
3010   RM=3:GOSUB RNG
3020   GFX PSET XX,YY,6+R
3030 NEXT I
3040 RETURN
3050 ' the four round bushes, on the graphics layer: three discs for the shape, a few light leaves
3060 LABEL BUSHES
3070 FOR I=0 TO 3
3080   BCX=90+I*140
3090   CC=2
3100   CX=BCX-11:CY=SHY+13:CR=9:GOSUB DISC
3110   CX=BCX+11:GOSUB DISC
3120   CX=BCX:CY=SHY+9:CR=11:GOSUB DISC
3130   CY=SHY+14:CR=9:GOSUB DISC
3140   CC=3:CR=2
3150   CX=BCX-4:CY=SHY+6:GOSUB DISC
3160   CX=BCX+5:CY=SHY+10:GOSUB DISC
3170   CX=BCX-12:CY=SHY+15:GOSUB DISC
3180   CX=BCX+12:CY=SHY+14:GOSUB DISC
3190   CX=BCX:CY=SHY+17:GOSUB DISC
3200 NEXT I
3210 RETURN
3220 ' the score line
3230 LABEL HUD
3240 COLOR 137
3250 S$=RIGHT$("00000"+STR$(SC),5):H$=RIGHT$("00000"+STR$(HI),5)
3260 LOCATE 1,0:PRINT "SCORE ";S$;"   HI ";H$;"   WAVE ";WV;"   LIVES ";LV;"  ";
3270 COLOR 140
3280 HF=0
3290 RETURN
3300 ' ---------------------------------------------------------------------------------------------- keys
3310 LABEL KEYS
3320 ' the gardener walks while LEFT / RIGHT (or A / D, the D-pad or the left stick) is held; SPACE (or UP, A, the right trigger) throws; START pauses, BACK quits
3330 PD=0
3340 IF KEY("LEFT") OR KEY("A") OR JOY("LEFT") OR JOY("LX")<-0.4 THEN PD=PD-1
3350 IF KEY("RIGHT") OR KEY("D") OR JOY("RIGHT") OR JOY("LX")>0.4 THEN PD=PD+1
3360 IF KEY("SPACE") OR KEY("UP") OR JOY("A") OR JOY("RT")>0.4 THEN FQ=1
3370 PS0=PSK:PSK=JOY("START"):IF PSK AND NOT PS0 THEN GOSUB PAUSE
3380 IF JOY("BACK") THEN QT=1
3390 N=0
3400 REPEAT
3410   K$=INKEY$
3420   IF K$<>"" THEN GOSUB KEYACT
3430   N=N+1
3440 UNTIL K$="" OR N>=6
3450 RETURN
3460 LABEL KEYACT
3470 K=ASC(K$):IF K>=97 AND K<=122 THEN K=K-32
3480 IF K=81 THEN QT=1
3490 IF K=80 THEN GOSUB PAUSE
3500 RETURN
3510 LABEL PAUSE
3520 COLOR 140:LOCATE 36,21:PRINT "PAUSED"; :COLOR 140
3530 WHILE JOY("START"):GFX VSYNC:WEND
3540 REPEAT
3550   GFX VSYNC
3560   K$=INKEY$
3570 UNTIL K$="p" OR K$="P" OR K$="q" OR K$="Q" OR JOY("START") OR JOY("BACK")
3580 IF K$="q" OR K$="Q" OR JOY("BACK") THEN QT=1
3590 WHILE JOY("START"):GFX VSYNC:WEND
3600 TXT FILL 36,21,41,21,32
3610 RETURN
3620 ' ---------------------------------------------------------------------------------------------- the gardener and the ball
3630 LABEL MOVEPLAYER
3640 IF PD=0 THEN RETURN
3650 PX=PX+PD*3
3660 IF PX<4 THEN PX=4:PD=0
3670 IF PX>620 THEN PX=620:PD=0
3680 SPRITE 40,PX,PY
3690 RETURN
3700 ' the ball flies up; TX,TY is its top: what does it touch?
3710 LABEL BALLFLIGHT
3720 IF FQ=1 THEN GOSUB THROW
3730 IF PBA=0 THEN RETURN
3740 PBY=PBY-7
3750 IF PBY<8 THEN PBA=0:SPRITE 41,OFF:RETURN
3760 SPRITE 41,PBX,PBY
3770 TX=PBX+7:TY=PBY+8
3780 IF POINT(TX,TY)>0 THEN GOSUB BUSHHIT:PBA=0:SPRITE 41,OFF:RETURN
3790 IF TX>=FX AND TY>=FY THEN GOSUB FRUITHIT
3800 RETURN
3810 LABEL THROW
3820 FQ=0
3830 IF PBA=1 THEN RETURN
3840 PBA=1:PBX=PX:PBY=PY-12
3850 SPRITE 41,PBX,PBY,7:SPRITE 41,ON
3860 SFX 1
3870 RETURN
3880 ' did the ball (TX,TY) hit a fruit? C = column, RW = row in the formation
3890 LABEL FRUITHIT
3900 C=(TX-FX)\SX:RX=TX-FX-C*SX
3910 RW=(TY-FY)\SY+1:RY=TY-FY-(RW-1)*SY
3920 IF C<=7 AND RW<=5 AND RX<16 AND RY>=4 AND RY<20 THEN
3930   IF A(RW,C)=1 THEN GOSUB SPLAT
3940 ENDIF
3950 RETURN
3960 LABEL SPLAT
3970 A(RW,C)=0:NA=NA-1
3980 IF RW=1 THEN SC=SC+30
3990 IF RW=2 OR RW=3 THEN SC=SC+20
4000 IF RW>=4 THEN SC=SC+10
4010 N=(RW-1)*8+C
4020 SPRITE N,OFF
4030 EI=1-EI
4040 SPRITE 46+EI,FX+C*SX,FY+(RW-1)*SY,10:SPRITE 46+EI,ON:ET(EI)=8
4050 SFX 2
4060 PBA=0:SPRITE 41,OFF
4070 GOSUB SCORED
4080 SP=NA\2+2-WV:IF SP<2 THEN SP=2
4090 RETURN
4100 ' score changed: an extra life every 1500 points, the high score, redraw the score line
4110 LABEL SCORED
4120 IF SC>=NX THEN LV=LV+1:NX=NX+1500:SFX 5
4130 IF SC>HI THEN HI=SC
4140 HF=1
4150 RETURN
4160 ' ---------------------------------------------------------------------------------------------- the seeds the fruit spits
4170 LABEL SEEDS
4180 FOR I=0 TO 2
4190   IF SEEDON(I)=1 THEN GOSUB SEEDMOVE
4200 NEXT I
4210 RM=BR:GOSUB RNG
4220 IF R=0 THEN GOSUB SPAWN
4230 RETURN
4240 LABEL SEEDMOVE
4250 SEEDY(I)=SEEDY(I)+BS
4260 IF SEEDY(I)>268 THEN SEEDON(I)=0:SPRITE 42+I,OFF:RETURN
4270 SPRITE 42+I,SEEDX(I),SEEDY(I)
4280 TX=SEEDX(I)+7:TY=SEEDY(I)+16
4290 IF TY<246 AND POINT(TX,TY)>0 THEN GOSUB BUSHHIT:SEEDON(I)=0:SPRITE 42+I,OFF:RETURN
4300 IF TY>=PY+2 AND TX>=PX+2 AND TX<=PX+13 THEN GOSUB PLAYERHIT:RETURN
4310 IF PBA=1 AND TX>=PBX+4 AND TX<=PBX+11 AND TY>=PBY+6 AND TY<=PBY+14 THEN SEEDON(I)=0:SPRITE 42+I,OFF:PBA=0:SPRITE 41,OFF:SFX 6
4320 RETURN
4330 ' a new seed from the lowest fruit of a random column that still has one
4340 LABEL SPAWN
4350 J=-1
4360 FOR Z=0 TO 2
4370   IF SEEDON(Z)=0 THEN J=Z
4380 NEXT Z
4390 IF J<0 THEN RETURN
4400 RM=8:GOSUB RNG:C=R
4410 FOR Z=1 TO 8
4420   RW=5
4430   WHILE A(RW,C)=0 AND RW>0
4440     RW=RW-1
4450   WEND
4460   IF RW>0 THEN GOSUB DROPSEED:RETURN
4470   C=C+1:IF C>7 THEN C=0
4480 NEXT Z
4490 RETURN
4500 LABEL DROPSEED
4510 SEEDON(J)=1:SEEDX(J)=FX+C*SX:SEEDY(J)=FY+(RW-1)*SY+12
4520 SPRITE 42+J,SEEDX(J),SEEDY(J),8:SPRITE 42+J,ON
4530 RETURN
4540 ' a hit on a bush at (TX,TY): a round hole and a few scattered leaves
4550 LABEL BUSHHIT
4560 CX=TX:CY=TY:CR=4:CC=0:GOSUB DISC
4570 FOR Z=1 TO 5
4580   RM=14:GOSUB RNG:XX=TX-7+R
4590   RM=14:GOSUB RNG:GFX PSET XX,TY-7+R,0
4600 NEXT Z
4610 SFX 6
4620 RETURN
4630 ' ---------------------------------------------------------------------------------------------- the formation
4640 LABEL FORM
4650 ST=ST+1
4660 IF ST<SP THEN RETURN
4670 ST=0
4680 GOSUB MARCH
4690 RETURN
4700 LABEL MARCH
4710 MN=8:MX=-1:LR=0
4720 FOR C=0 TO 7
4730   FOR RW=1 TO 5
4740     IF A(RW,C)=1 AND MN=8 THEN MN=C
4750     IF A(RW,C)=1 THEN MX=C
4760     IF A(RW,C)=1 AND RW>LR THEN LR=RW
4770   NEXT RW
4780 NEXT C
4790 IF (DX=1 AND FX+MX*SX+16+SD>630) OR (DX=-1 AND FX+MN*SX-SD<8) THEN
4800   DX=0-DX:FY=FY+10
4810 ELSE
4820   FX=FX+DX*SD
4830 ENDIF
4840 FA=1-FA
4850 ' the rustle of the leaves, two different noises one after the other (the march gets faster when fewer fruit are left)
4860 SFX 7+FA
4870 FOR RW=1 TO 5
4880   FOR C=0 TO 7
4890     IF A(RW,C)=1 THEN SPRITE (RW-1)*8+C,FX+C*SX,FY+(RW-1)*SY,IM(RW)+FA
4900   NEXT C
4910 NEXT RW
4920 IF FY+(LR-1)*SY+20>=SHY-2 THEN OV=1:LANDED=1
4930 RETURN
4940 ' ---------------------------------------------------------------------------------------------- splashes
4950 LABEL EFFECTS
4960 FOR I=0 TO 1
4970   IF ET(I)>0 THEN GOSUB ETICK
4980 NEXT I
4990 RETURN
5000 LABEL ETICK
5010 ET(I)=ET(I)-1
5020 IF ET(I)=0 THEN SPRITE 46+I,OFF
5030 RETURN
5040 ' the gardener was hit by a seed
5050 LABEL PLAYERHIT
5060 LV=LV-1
5070 SFX 3
5080 PBA=0:SPRITE 41,OFF
5090 FOR Z=0 TO 2
5100   SEEDON(Z)=0:SPRITE 42+Z,OFF
5110 NEXT Z
5120 SPRITE 40,OFF
5130 SPRITE 48,PX-8,PY-4,11:SPRITE 48,ON
5140 FOR Z=1 TO 70
5150   IF Z=36 THEN SPRITE 48,IMAGE 10
5160   GFX VSYNC
5170   K$=INKEY$
5180 NEXT Z
5190 SPRITE 48,OFF
5200 IF LV<=0 THEN OV=1:RETURN
5210 PX=312:PD=0
5220 SPRITE 40,PX,PY,6:SPRITE 40,ON
5230 HF=1
5240 RETURN
5250 ' ---------------------------------------------------------------------------------------------- waves
5260 LABEL NEWWAVE
5270 FOR RW=1 TO 5
5280   FOR C=0 TO 7
5290     A(RW,C)=1
5300   NEXT C
5310 NEXT RW
5320 NA=40
5330 Q=WV-1:IF Q>4 THEN Q=4
5340 FX=40:FY=34+Q*8:DX=1:SD=6
5350 SP=22-WV:IF SP<10 THEN SP=10
5360 ST=0:FA=0
5370 BS=2+WV\2:IF BS>5 THEN BS=5
5380 BR=50-WV*5:IF BR<14 THEN BR=14
5390 GFX CLEAR
5400 GOSUB GROUND
5410 GOSUB BUSHES
5420 FOR I=0 TO 2
5430   SEEDON(I)=0:SPRITE 42+I,OFF
5440 NEXT I
5450 PBA=0:SPRITE 41,OFF
5460 FOR RW=1 TO 5
5470   FOR C=0 TO 7
5480     SPRITE (RW-1)*8+C,FX+C*SX,FY+(RW-1)*SY,IM(RW)
5490     SPRITE (RW-1)*8+C,ON
5500   NEXT C
5510 NEXT RW
5520 PX=312:PD=0:FQ=0
5530 SPRITE 40,PX,PY,6:SPRITE 40,ON
5540 COLOR 140:LOCATE 35,21:PRINT "WAVE ";WV; :COLOR 140
5550 GOSUB HUD
5560 FOR Z=1 TO 75
5570   GFX VSYNC
5580   K$=INKEY$
5590 NEXT Z
5600 TXT FILL 35,21,44,21,32
5610 RETURN
5620 LABEL WAVEDONE
5630 SOUND OFF
5640 PLAY "T200 W1 V0.35 O5 C8 E8 G8 >C4","T200 W2 V0.3 O4 C8 E8 G8 >C4"
5650 FOR Z=1 TO 90
5660   GFX VSYNC
5670 NEXT Z
5680 SC=SC+100:GOSUB SCORED
5690 WV=WV+1
5700 GOSUB NEWWAVE
5710 RETURN
5720 ' ---------------------------------------------------------------------------------------------- the game
5730 LABEL TITLE
5740 GOSUB CLEARALL
5750 GFX TEXT 152,20,"ORCHARD",137,6,6
5760 GFX TEXT 180,100,"DEFENSE",110,5,5
5770 SPRITE 0,ON:SPRITE 0,176,172,0
5780 SPRITE 1,ON:SPRITE 1,272,172,2
5790 SPRITE 2,ON:SPRITE 2,368,172,4
5800 SPRITE 3,ON:SPRITE 3,464,PY-76,6
5810 COLOR 140
5820 LOCATE 25,15:PRINT "=30         =20         =10";
5830 LOCATE 22,18:PRINT "LEFT / RIGHT or A / D   walk";
5840 LOCATE 22,19:PRINT "SPACE or UP   throw a ball (hold for rapid fire)";
5850 LOCATE 22,20:PRINT "P pause     Q quit     (pad: START, BACK)";
5860 LOCATE 22,21:PRINT "Controller: D-pad or stick walks, A throws";
5870 COLOR 137:LOCATE 24,23:PRINT "Press SPACE or A to start";
5880 COLOR 140
5890 PLAY "T160 W1 V0.3 O4 E8 G8 >C8 E4","T160 W2 V0.3 O3 C4 G4"
5900 QT=0
5910 WHILE JOY("A") OR JOY("START") OR JOY("BACK"):GFX VSYNC:WEND
5920 REPEAT
5930   GFX VSYNC
5940   K$=INKEY$
5950 UNTIL K$=" " OR K$="q" OR K$="Q" OR JOY("A") OR JOY("START") OR JOY("BACK")
5960 IF K$="q" OR K$="Q" OR JOY("BACK") THEN QT=1
5970 RETURN
5980 LABEL PLAYGAME
5990 SC=0:LV=3:WV=1:NX=1500:OV=0:LANDED=0:QT=0
6000 PBA=0:EI=0:ET(0)=0:ET(1)=0
6010 GOSUB CLEARALL
6020 GOSUB NEWWAVE
6030 WHILE OV=0
6040   GOSUB KEYS
6050   IF QT=1 THEN OV=1
6060   GOSUB MOVEPLAYER
6070   GOSUB BALLFLIGHT
6080   GOSUB SEEDS
6090   GOSUB FORM
6100   GOSUB EFFECTS
6110   IF HF=1 THEN GOSUB HUD
6120   IF NA=0 AND OV=0 THEN GOSUB WAVEDONE
6130   GFX VSYNC
6140 WEND
6150 GOSUB OVER
6160 RETURN
6170 LABEL OVER
6180 SOUND OFF
6190 FOR I=0 TO 48
6200   SPRITE I,OFF
6210 NEXT I
6220 GFX TEXT 168,110,"GAME OVER",110,5,5
6230 IF LANDED=1 THEN COLOR 140:LOCATE 28,18:PRINT "The fruit has landed!";
6240 COLOR 140:LOCATE 30,20:PRINT "Score ";SC; :COLOR 140
6250 PLAY "T100 W2 V0.5 O4 C4 <B4 A4 G2"
6260 FOR Z=1 TO 150
6270   GFX VSYNC
6280   K$=INKEY$
6290 NEXT Z
6300 RETURN
6310 ' ---------------------------------------------------------------------------------------------- main
6320 LABEL MAIN
6330 GOSUB INIT
6340 QT=0
6350 REPEAT
6360   GOSUB TITLE
6370   IF QT=0 THEN GOSUB PLAYGAME:QT=0
6380 UNTIL QT=1
6390 SOUND OFF
6400 GFX RESET
6410 CLS
6420 PRINT "Thanks for playing ORCHARD DEFENSE!"
6430 END
