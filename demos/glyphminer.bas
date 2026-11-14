10 ' License: free for everybody to use, copy, change and build into executables (BUILD).
20 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
30 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
40 ' in the executable (it shows with the command line option --engine1999-about).
50 ' the helpers that the demo games share are in the libraries demos/lib/util.lib, input.lib, hiscore.lib and text.lib
60 LOAD LIBRARY "util"
70 LOAD LIBRARY "input"
80 LOAD LIBRARY "hiscore"
90 LOAD LIBRARY "text":LOAD LIBRARY "banner":CALL BN_2D("GLYPH MINER")
100 ' GLYPH MINER - a cave digger in the spirit of the 8-bit classics (no likeness of any of them), for Engine1999. Window version only.
110 ' The cave is the TEXT SCREEN: 40 x 12 cells, each cell is 2 x 2 redefined characters (SYMBOL): dirt, steel, brick, boulder, gem, door.
120 ' The miner, the fireflies and the worms are sprites that glide from cell to cell, one cell is exactly the size of a sprite (16 x 24 pixels).
130 ' Keys: arrows or W A S D dig and walk (SPACE + a direction grabs dirt or a gem without stepping), P pause, M music, Q quit.
140 ' Game controller (optional): the D-pad or the left stick moves, A grabs, START pauses, BACK quits.
150 ' Dig through the dirt, collect enough gems and run into the door before the time is up. Boulders and gems fall when there is nothing
160 ' under them: you can push a boulder sideways, but a falling one crushes you. A firefly that is crushed bursts into gems (and so does
170 ' everything around it, you included, if you stand too close); worms tunnel through the dirt after you.
180 ' What it shows: redefined characters (SYMBOL, DESIGN CHAR) drawn from a canvas array; animation by redefining the glyphs (the gems glint
190 ' on every cell at once, the door glows by a palette change); sprites with engine-driven animation (SPRITE SEQ / ANIM) and MOVETO;
200 ' sound effects (SFX), a chain of rising pickup tones (SOUND), music made up bar by bar (PLAY, EVERY).
210 ' --- global variables -----------------------------------------------------------------------------------------------------------------
220 ' C(x,y) the cave: 0 empty, 1 dirt, 2 steel, 3 brick, 4 boulder, 5 gem, 6 door; FL falling, MD moved in tick TKN, SH what is shown now
230 ' MX,MY miner cell, MDEAD, WONF, GM gems, NEED needed, DOPEN door open, TM time left (s), ENX,ENY,ED,ET,ES,ENA the enemies 1..NE
240 ' (the arrays are declared before the FUNCs that use them)
250 DIM C(40,12),FL(40,12),MD(40,12),SH(40,12),PX(16,24),GB(16,24)
260 DIM CT(10),CB(10),CK(10),DXT(4),DYT(4),EXT(9)
270 DIM ENX(9),ENY(9),ED(9),ET(9),ES(9),ENA(9),LD$(4),BS$(4)
280 ' (a FUNC has to be defined before the lines that call it)
290 FUNC ENEMYAT(X,Y)
300   LOCAL I
310   FOR I=1 TO NE
320     IF ENA(I)=1 AND ENX(I)=X AND ENY(I)=Y THEN RETURN I
330   NEXT I
340   RETURN 0
350 END FUNC
360 ' 1 if the miner or an enemy is in the cell
370 FUNC OCC(X,Y)
380   IF MDEAD=0 AND WONF=0 AND MX=X AND MY=Y THEN RETURN 1
390   IF FN ENEMYAT(X,Y)>0 THEN RETURN 1
400   RETURN 0
410 END FUNC
420 ' ====================================================================== main program
430 CALL INIT
440 HI=FN HS_LOAD("glyphminer.hi")
450 QT=0
460 REPEAT
470   CALL TITLE
480   IF QT=0 THEN
490     CALL PLAYGAME
500     QT=0
510   END IF
520   IF SC>=HI AND SC>0 THEN
530     HI=SC
540     ON ERROR GOTO HIERR
550     CALL HS_SAVE("glyphminer.hi",HI)
560     LABEL HIDONE
570     ON ERROR OFF
580   END IF
590 UNTIL QT=1
600 EVERY OFF
610 CALL TX_BYE("GLYPH MINER")
620 END
630 ' a high score file that can't be written is ignored (the handler is here in the main program, where RESUME <label> works)
640 LABEL HIERR
650 RESUME HIDONE
660 ' the EVERY handler: it only calls a procedure
670 LABEL MUSICTIMER
680 CALL MUSICTICK
690 RETURN
700 ' ====================================================================== set up (once)
710 PROC INIT
720   TX_PAPER=200:TX_SHADOW=11
730   LOCAL I
740   RANDOMIZE
750   GFX RESET ALL
760   FOR I=0 TO 3
770     READ DXT(I),DYT(I)
780   NEXT I
790   GW=40:GH=12:TK=6:MS=2:FS=3:WS=4:NE=0:MON=1:MQ=0:MB=0:TM=999:DOPEN=0:MDEAD=1:WONF=0
800   LD$(0)="E R E G A R G E"
810   LD$(1)="F R A >C R <A F A"
820   LD$(2)="G R E G >C R <B G"
830   LD$(3)="E R G+ B >E R D C"
840   BS$(0)="A E A E"
850   BS$(1)="F C F C"
860   BS$(2)="C G C G"
870   BS$(3)="E B E B"
880   CALL PALETTES
890   CALL SOUNDS
900   CALL DEFGLYPHS
910   CALL DEFSPRITES
920   SPRITE SEQ 1,CHR$(2)+CHR$(3)
930   SPRITE SEQ 2,CHR$(4)+CHR$(5)
940   SPRITE SEQ 3,CHR$(6)+CHR$(7)
950   EVERY 60,1 GOSUB MUSICTIMER
960 END PROC
970 ' directions: 0 up, 1 right, 2 down, 3 left
980 DATA 0,-1,1,0,0,1,-1,0
990 PROC SOUNDS
1000   SFX 1,"W3 V0.22 ML X0,0.005,0.1,0.03 S900,300,2"
1010   SFX 3,"W3 V0.45 ML X0,0.005,0.2,0.1 S300,60,3"
1020   SFX 4,"W3 V0.25 ML S220,110,2"
1030   SFX 5,"W3 V1 ML X0,0.02,0.5,0.6 S900,30,3"
1040   SFX 6,"W1 V0.4 ML H523,10 H659,10 H784,10 H1047,6"
1050   SFX 7,"W1 V0.4 ML H523,8 H659,8 H784,8 H1047,8 H1319,8 H1568,14"
1060   SFX 8,"W2 V0.4 ML S800,80,40"
1070   SFX 9,"W1 V0.15 ML H1568,3"
1080   SFX 10,"W1 V0.35 ML H988,16 H1319,6"
1090 END PROC
1100 ' ====================================================================== colors
1110 PROC PALETTES
1120   LOCAL I,R,G,B
1130   ' text layer (2): the colors of the glyphs and of the text lines
1140   GFX PALETTE 2,200,RGB(0,0,0):GFX PALETTE 2,201,RGB(255,255,255):GFX PALETTE 2,202,RGB(255,215,60):GFX PALETTE 2,203,RGB(255,90,90)
1150   GFX PALETTE 2,204,RGB(110,220,255):GFX PALETTE 2,205,RGB(170,180,220)
1160   GFX PALETTE 2,210,RGB(58,34,16):GFX PALETTE 2,211,RGB(118,76,34):GFX PALETTE 2,212,RGB(92,56,26)
1170   GFX PALETTE 2,213,RGB(26,30,52):GFX PALETTE 2,214,RGB(132,152,194)
1180   GFX PALETTE 2,215,RGB(112,34,26):GFX PALETTE 2,216,RGB(200,190,170)
1190   GFX PALETTE 2,217,RGB(196,196,206):GFX PALETTE 2,218,RGB(140,140,156):GFX PALETTE 2,219,RGB(96,96,112)
1200   GFX PALETTE 2,220,RGB(90,240,255):GFX PALETTE 2,221,RGB(255,110,230):GFX PALETTE 2,222,RGB(110,255,120)
1210   GFX PALETTE 2,223,RGB(30,170,205):GFX PALETTE 2,224,RGB(200,50,170):GFX PALETTE 2,225,RGB(50,190,70)
1220   GFX PALETTE 2,226,RGB(232,172,60):GFX PALETTE 2,227,RGB(60,40,20):GFX PALETTE 2,229,RGB(255,250,200):GFX PALETTE 2,230,RGB(24,10,44)
1230   GFX PALETTE 2,231,RGB(255,60,70):GFX PALETTE 2,232,RGB(90,240,255)
1240   ' graphics layer (3): the big letters
1250   GFX PALETTE 3,10,RGB(255,255,255):GFX PALETTE 3,11,RGB(20,10,50):GFX PALETTE 3,12,RGB(255,215,60)
1260   ' sprite layer (4): the pictures use the digits 1..9 and A..D, DESIGN adds the base 16
1270   RESTORE SPRPAL
1280   FOR I=1 TO 13
1290     READ R,G,B
1300     GFX PALETTE 4,16+I,RGB(R,G,B)
1310   NEXT I
1320   GFX BACKDROP RGB(0,0,0)
1330 END PROC
1340 ' skin, helmet, outline, overalls, white, red, orange, green, light green, lamp, brown, gray, yellow
1350 LABEL SPRPAL
1360 DATA 255,205,160,255,200,30,40,30,60,60,110,230,255,255,255,230,50,50,255,140,30,60,200,80,170,255,120,255,250,190,130,80,40,150,150,160,255,240,100
1370 ' ====================================================================== the redefined characters
1380 ' every glyph is drawn on the canvas PX (16 x 24 pixels, 0 / 1) and cut into four characters of 8 x 12 pixels:
1390 ' the glyph k is the characters 160+4k (top left), +1 (top right), +2 (bottom left), +3 (bottom right)
1400 PROC PXCLEAR
1410   LOCAL X,Y
1420   FOR Y=0 TO 23
1430     FOR X=0 TO 15
1440       PX(X,Y)=0
1450     NEXT X
1460   NEXT Y
1470 END PROC
1480 PROC GLYPH(K)
1490   LOCAL Q,R,B,I,A$,QX,QY
1500   FOR Q=0 TO 3
1510     QX=(Q MOD 2)*8
1520     QY=(Q\2)*12
1530     A$=""
1540     FOR R=0 TO 11
1550       B=0
1560       FOR I=0 TO 7
1570         B=B*2+PX(QX+I,QY+R)
1580       NEXT I
1590       A$=A$+CHR$(B)
1600     NEXT R
1610     SYMBOL 160+K*4+Q,A$
1620   NEXT Q
1630 END PROC
1640 PROC MAKEDIRT(K)
1650   LOCAL I,X,Y
1660   CALL PXCLEAR
1670   FOR I=1 TO 22
1680     X=FN RN(15):Y=FN RN(23)
1690     PX(X,Y)=1:PX(X+1,Y)=1
1700     IF FN RN(2)=0 THEN PX(X,Y+1)=1
1710   NEXT I
1720   CALL GLYPH(K)
1730 END PROC
1740 PROC MAKESTEEL
1750   LOCAL X,Y,I
1760   CALL PXCLEAR
1770   FOR Y=0 TO 23
1780     FOR X=0 TO 15
1790       IF X=0 OR X=15 OR Y=0 OR Y=23 THEN PX(X,Y)=1
1800     NEXT X
1810   NEXT Y
1820   FOR Y=0 TO 1
1830     FOR X=0 TO 1
1840       PX(2+X,3+Y)=1:PX(12+X,3+Y)=1:PX(2+X,19+Y)=1:PX(12+X,19+Y)=1
1850     NEXT X
1860   NEXT Y
1870   CALL GLYPH(2)
1880 END PROC
1890 PROC MAKEBRICK
1900   LOCAL X,Y,BD,BJ
1910   CALL PXCLEAR
1920   FOR Y=0 TO 23
1930     BD=Y\6
1940     BJ=(BD MOD 2)*4
1950     FOR X=0 TO 15
1960       IF Y MOD 6=0 THEN PX(X,Y)=1
1970       IF (X-BJ) MOD 8=0 THEN PX(X,Y)=1
1980     NEXT X
1990   NEXT Y
2000   CALL GLYPH(3)
2010 END PROC
2020 PROC MAKEBOULDER
2030   LOCAL X,Y,I,DX,DY
2040   CALL PXCLEAR
2050   FOR Y=0 TO 23
2060     FOR X=0 TO 15
2070       DX=(X-7.5)/7.6:DY=(Y-11.5)/11.2
2080       IF DX*DX+DY*DY<=1 THEN PX(X,Y)=1
2090     NEXT X
2100   NEXT Y
2110   FOR I=0 TO 4
2120     PX(5+I,5+I)=0:PX(9+I\2,13+I)=0
2130   NEXT I
2140   PX(4,15)=0:PX(5,16)=0:PX(6,16)=0:PX(11,6)=0
2150   CALL GLYPH(4)
2160 END PROC
2170 ' the gem without its sparkle is kept in GB, MAKEGEM(F) adds the sparkle number F (0..3, a cross at one of four places) and redefines the glyph
2180 PROC GEMBASE
2190   LOCAL X,Y
2200   FOR Y=0 TO 23
2210     FOR X=0 TO 15
2220       GB(X,Y)=0
2230       IF ABS(X-7.5)/7.5+ABS(Y-11.5)/11.5<=1 THEN GB(X,Y)=1
2240     NEXT X
2250   NEXT Y
2260   FOR X=0 TO 15
2270     GB(X,8)=0
2280   NEXT X
2290   FOR Y=9 TO 22
2300     GB(7,Y)=0:GB(8,Y)=0
2310   NEXT Y
2320   FOR Y=3 TO 7
2330     GB(7,Y)=0:GB(8,Y)=0
2340   NEXT Y
2350 END PROC
2360 PROC MAKEGEM(F)
2370   LOCAL X,Y,SX,SY
2380   FOR Y=0 TO 23
2390     FOR X=0 TO 15
2400       PX(X,Y)=GB(X,Y)
2410     NEXT X
2420   NEXT Y
2430   IF F=0 THEN SX=2:SY=3
2440   IF F=1 THEN SX=13:SY=6
2450   IF F=2 THEN SX=13:SY=19
2460   IF F=3 THEN SX=2:SY=16
2470   PX(SX,SY)=1:PX(SX-1,SY)=1:PX(SX+1,SY)=1:PX(SX,SY-1)=1:PX(SX,SY+1)=1
2480   CALL GLYPH(5)
2490 END PROC
2500 PROC MAKEDOORS
2510   LOCAL X,Y,I
2520   CALL PXCLEAR
2530   FOR Y=1 TO 23
2540     FOR X=2 TO 13
2550       PX(X,Y)=1
2560     NEXT X
2570   NEXT Y
2580   FOR X=4 TO 11
2590     PX(X,3)=0:PX(X,10)=0:PX(X,13)=0:PX(X,21)=0
2600   NEXT X
2610   FOR Y=3 TO 10
2620     PX(4,Y)=0:PX(11,Y)=0
2630   NEXT Y
2640   FOR Y=13 TO 21
2650     PX(4,Y)=0:PX(11,Y)=0
2660   NEXT Y
2670   PX(10,11)=0:PX(10,12)=0
2680   CALL GLYPH(6)
2690   CALL PXCLEAR
2700   FOR Y=1 TO 23
2710     PX(2,Y)=1:PX(3,Y)=1:PX(12,Y)=1:PX(13,Y)=1
2720   NEXT Y
2730   FOR X=2 TO 13
2740     PX(X,1)=1:PX(X,2)=1
2750   NEXT X
2760   FOR I=0 TO 4
2770     FOR X=4 TO 11
2780       IF X MOD 2=0 THEN PX(X,21-I*4)=1
2790     NEXT X
2800   NEXT I
2810   CALL GLYPH(7)
2820 END PROC
2830 PROC DEFGLYPHS
2840   CALL MAKEDIRT(0)
2850   CALL MAKEDIRT(1)
2860   CALL MAKESTEEL
2870   CALL MAKEBRICK
2880   CALL MAKEBOULDER
2890   CALL GEMBASE
2900   GF=0
2910   CALL MAKEGEM(0)
2920   CALL MAKEDOORS
2930   ' the colors of the glyphs: the pen of the top and of the bottom row of characters, and the paper (the gem's pen depends on the cell)
2940   CT(0)=211:CB(0)=211:CK(0)=210
2950   CT(1)=212:CB(1)=212:CK(1)=210
2960   CT(2)=214:CB(2)=214:CK(2)=213
2970   CT(3)=216:CB(3)=216:CK(3)=215
2980   CT(4)=217:CB(4)=219:CK(4)=200
2990   CT(5)=220:CB(5)=223:CK(5)=200
3000   CT(6)=226:CB(6)=226:CK(6)=227
3010   CT(7)=229:CB(7)=229:CK(7)=230
3020   ' small signs for the status line: a heart (192) and a gem (193)
3030   DESIGN CHAR 192
3040   @ "........"
3050   @ "........"
3060   @ ".11..11."
3070   @ "11111111"
3080   @ "11111111"
3090   @ "11111111"
3100   @ ".111111."
3110   @ "..1111.."
3120   @ "...11..."
3130   @ "........"
3140   @ "........"
3150   @ "........"
3160   DESIGN CHAR 193
3170   @ "........"
3180   @ "........"
3190   @ "..1111.."
3200   @ ".111111."
3210   @ "11111111"
3220   @ "11111111"
3230   @ ".111111."
3240   @ "..1111.."
3250   @ "...11..."
3260   @ "........"
3270   @ "........"
3280   @ "........"
3290 END PROC
3300 ' ====================================================================== the sprites (made with a small generator, 16 x 24 pixels, base 16)
3310 ' 0, 1 miner standing / walking, 2, 3 firefly (wings up / flat), 4, 5 worm (wiggling), 6, 7 explosion
3320 PROC DEFSPRITES
3330   DESIGN SPRITE 0,16
3340   @ "................"
3350   @ ".....222222....."
3360   @ "....22222222...."
3370   @ "...2222AA2222..."
3380   @ "...2222AA2222..."
3390   @ "..333333333333.."
3400   @ "....11111111...."
3410   @ "....11311311...."
3420   @ "....11111111...."
3430   @ "....11166111...."
3440   @ ".....111111....."
3450   @ "......4444......"
3460   @ "....44444444...."
3470   @ "...4444444444..."
3480   @ "..1.44444444.1.."
3490   @ "..1.44555544.1.."
3500   @ "..1.44444444.1.."
3510   @ "....44444444...."
3520   @ "....44444444...."
3530   @ "....444..444...."
3540   @ "....444..444...."
3550   @ "....444..444...."
3560   @ "...BBBB..BBBB..."
3570   @ "...BBBB..BBBB..."
3580   DESIGN SPRITE 1,16
3590   @ "................"
3600   @ ".....222222....."
3610   @ "....22222222...."
3620   @ "...2222AA2222..."
3630   @ "...2222AA2222..."
3640   @ "..333333333333.."
3650   @ "....11111111...."
3660   @ "....11311311...."
3670   @ "....11111111...."
3680   @ "....11166111...."
3690   @ ".....111111....."
3700   @ "......4444......"
3710   @ "....44444444...."
3720   @ "...4444444444..."
3730   @ "..1.44444444.1.."
3740   @ "..1.44555544.1.."
3750   @ "..1.44444444.1.."
3760   @ "....44444444...."
3770   @ "....44444444...."
3780   @ ".....4444444...."
3790   @ ".....444.444...."
3800   @ "....444...444..."
3810   @ "...BBBB...BBBB.."
3820   @ "...BBBB...BBBB.."
3830   DESIGN SPRITE 2,16
3840   @ "................"
3850   @ "................"
3860   @ "................"
3870   @ "................"
3880   @ "..55........55.."
3890   @ ".5555......5555."
3900   @ ".5555..77..5555."
3910   @ "..555.7777.555.."
3920   @ "...5.777777.5..."
3930   @ ".....7DDDD7....."
3940   @ "....77DDDD77...."
3950   @ "....7DD77DD7...."
3960   @ "....7DDDDDD7...."
3970   @ "....77DDDD77...."
3980   @ ".....7DDDD7....."
3990   @ ".....77DD77....."
4000   @ "......7777......"
4010   @ ".......77......."
4020   @ "................"
4030   @ "................"
4040   @ "................"
4050   @ "................"
4060   @ "................"
4070   @ "................"
4080   DESIGN SPRITE 3,16
4090   @ "................"
4100   @ "................"
4110   @ "................"
4120   @ "................"
4130   @ "................"
4140   @ "................"
4150   @ "55555..77..55555"
4160   @ "55555.7777.55555"
4170   @ ".555.777777.555."
4180   @ ".....7DDDD7....."
4190   @ "....77DDDD77...."
4200   @ "....7DD77DD7...."
4210   @ "....7DDDDDD7...."
4220   @ "....77DDDD77...."
4230   @ ".....7DDDD7....."
4240   @ ".....77DD77....."
4250   @ "......7777......"
4260   @ ".......77......."
4270   @ "................"
4280   @ "................"
4290   @ "................"
4300   @ "................"
4310   @ "................"
4320   @ "................"
4330   DESIGN SPRITE 4,16
4340   @ "................"
4350   @ "................"
4360   @ ".....888888....."
4370   @ "....88999988...."
4380   @ "...8899999988..."
4390   @ "...8959959988..."
4400   @ "...8999999988..."
4410   @ "...8899669988..."
4420   @ "....88999988...."
4430   @ ".....888888....."
4440   @ "......888888...."
4450   @ ".....88999988..."
4460   @ ".....88999988..."
4470   @ "......888888...."
4480   @ "....888888......"
4490   @ "...88999988....."
4500   @ "...88999988....."
4510   @ "....888888......"
4520   @ "......888888...."
4530   @ ".....88999988..."
4540   @ ".....88999988..."
4550   @ "......888888...."
4560   @ "................"
4570   @ "................"
4580   DESIGN SPRITE 5,16
4590   @ "................"
4600   @ "................"
4610   @ ".....888888....."
4620   @ "....88999988...."
4630   @ "...8899999988..."
4640   @ "...8959959988..."
4650   @ "...8999999988..."
4660   @ "...8899669988..."
4670   @ "....88999988...."
4680   @ ".....888888....."
4690   @ "....888888......"
4700   @ "...88999988....."
4710   @ "...88999988....."
4720   @ "....888888......"
4730   @ "......888888...."
4740   @ ".....88999988..."
4750   @ ".....88999988..."
4760   @ "......888888...."
4770   @ "....888888......"
4780   @ "...88999988....."
4790   @ "...88999988....."
4800   @ "....888888......"
4810   @ "................"
4820   @ "................"
4830   DESIGN SPRITE 6,16
4840   @ "................"
4850   @ "................"
4860   @ "................"
4870   @ "....66....66...."
4880   @ "....66....66...."
4890   @ "....676..676...."
4900   @ ".....77..77....."
4910   @ ".....77..77....."
4920   @ ".....6D77D6....."
4930   @ "......DDDD......"
4940   @ "...66.6DD6.66..."
4950   @ ".6677DDDDDD7766."
4960   @ ".6677DDDDDD7766."
4970   @ "...66.6DD6.66..."
4980   @ "......DDDD......"
4990   @ ".....6D77D6....."
5000   @ ".....77..77....."
5010   @ ".....77..77....."
5020   @ "....676..676...."
5030   @ "....66....66...."
5040   @ "....66....66...."
5050   @ "................"
5060   @ "................"
5070   @ "................"
5080   DESIGN SPRITE 7,16
5090   @ ".........66....."
5100   @ ".........66....."
5110   @ "..6.....676....."
5120   @ "..66....676....."
5130   @ "..666...776....."
5140   @ "..6776..776....."
5150   @ "..6777..77......"
5160   @ "...6776.D7....6."
5170   @ "....7DD.D7.66666"
5180   @ "....6DD6D6777766"
5190   @ ".....6DDDDDD7766"
5200   @ "....667DDDD776.."
5210   @ "..677DDDD766...."
5220   @ "6677DDDDDD6....."
5230   @ "6677776D6DD6...."
5240   @ "66666.7D.DD7...."
5250   @ ".6....7D.6776..."
5260   @ "......77..7776.."
5270   @ ".....677..6776.."
5280   @ ".....677...666.."
5290   @ ".....676....66.."
5300   @ ".....676.....6.."
5310   @ ".....66........."
5320   @ ".....66........."
5330 END PROC
5340 ' ====================================================================== drawing the cave
5350 PROC SETC(X,Y,V)
5360   C(X,Y)=V
5370   CALL DRAWCELL(X,Y)
5380 END PROC
5390 ' draws one cell as 2 x 2 characters, only when what it shows has changed
5400 PROC DRAWCELL(X,Y)
5410   LOCAL V,K,B,CODE,VR,FT,FB
5420   V=C(X,Y)
5430   K=-1
5440   IF V=1 THEN K=(X*7+Y*3) MOD 2
5450   IF V>=2 AND V<=5 THEN K=V
5460   IF V=6 THEN K=6+DOPEN
5470   CODE=K+1
5480   VR=(X+Y*2) MOD 3
5490   IF V=5 THEN CODE=CODE+16*VR
5500   IF SH(X,Y)=CODE THEN EXIT PROC
5510   SH(X,Y)=CODE
5520   IF K<0 THEN
5530     COLOR 200,200
5540     LOCATE X*2,Y*2:PRINT "  ";
5550     LOCATE X*2,Y*2+1:PRINT "  ";
5560   ELSE
5570     B=160+K*4
5580     FT=CT(K):FB=CB(K)
5590     IF K=5 THEN FT=220+VR:FB=223+VR
5600     COLOR FT,CK(K)
5610     LOCATE X*2,Y*2:PRINT CHR$(B);CHR$(B+1);
5620     COLOR FB,CK(K)
5630     LOCATE X*2,Y*2+1:PRINT CHR$(B+2);CHR$(B+3);
5640   END IF
5650 END PROC
5660 PROC DRAWALL
5670   LOCAL X,Y
5680   FOR Y=0 TO GH-1
5690     FOR X=0 TO GW-1
5700       SH(X,Y)=-1
5710       CALL DRAWCELL(X,Y)
5720     NEXT X
5730   NEXT Y
5740 END PROC
5750 PROC HUD
5760   LOCAL I,H$
5770   FOR I=1 TO LV
5780     H$=H$+CHR$(192)
5790   NEXT I
5800   CALL TXTAT(0,24,"SCORE "+FN PAD$(SC,6),201)
5810   COLOR 232,200:PRINT " ";CHR$(193);
5820   CALL TXTAT(21,24," "+FN PAD$(GM,2)+"/"+FN PAD$(NEED,2)+" ",204)
5830   COLOR 231,200:PRINT H$;"    ";
5840   CALL TXTAT(40,24,"TIME "+FN PAD$(TM,3),202)
5850   CALL TXTAT(52,24,"LEVEL "+STR$(LEVEL)+"  ",201)
5860   CALL TXTAT(64,24,"HI "+FN PAD$(HI,6),205)
5870 END PROC
5880 ' prints T$ centered in row Y
5890 PROC CTR(Y,T$,C)
5900   CALL TXTAT((80-LEN(T$))\2,Y,T$,C)
5910 END PROC
5920 PROC CLEARSCREEN
5930   LOCAL I,X,Y
5940   CLS
5950   GFX CLEAR
5960   FOR I=0 TO 40
5970     SPRITE I,OFF
5980   NEXT I
5990   FOR Y=0 TO GH-1
6000     FOR X=0 TO GW-1
6010       SH(X,Y)=-1
6020     NEXT X
6030   NEXT Y
6040   FOR I=0 TO 8
6050     EXT(I)=0
6060   NEXT I
6070 END PROC
6080 ' ====================================================================== title
6090 PROC TITLE
6100   LOCAL T,K$,I,F,X,Y
6110   CALL CLEARSCREEN
6120   NE=0:MDEAD=1:TM=999
6130   CALL BIGTEXT(6,"GLYPH",10,5)
6140   CALL BIGTEXT(70,"MINER",12,5)
6150   ' a strip of cave made of the redefined characters: dirt with gems and boulders, a tunnel, a steel floor
6160   FOR X=0 TO GW-1
6170     C(X,6)=1:C(X,7)=0:C(X,8)=2
6180     IF X MOD 7=3 THEN C(X,6)=5
6190     IF X MOD 11=5 THEN C(X,6)=4
6200     IF X>=34 AND X<=38 THEN C(X,6)=3
6210     IF X MOD 9=0 THEN C(X,7)=3:C(X,6)=1
6220   NEXT X
6230   FOR Y=6 TO 8
6240     FOR X=0 TO GW-1
6250       CALL DRAWCELL(X,Y)
6260     NEXT X
6270   NEXT Y
6280   CALL CTR(18,"Arrows / WASD move and dig   SPACE + direction: grab, no step",205)
6290   CALL CTR(19,"Boulders and gems fall: don't stand under one while it falls!",201)
6300   CALL CTR(20,"Crush a firefly and it bursts into gems - keep your distance.",201)
6310   CALL CTR(21,"Collect enough gems, then the door opens. Beat the clock.",201)
6320   CALL CTR(22,"P pause  M music  Q quit    Pad: stick/D-pad, A grab, START, BACK",205)
6330   CALL CTR(23,"HIGH SCORE "+FN PAD$(HI,6),202)
6340   CALL CTR(24,"Press SPACE or A to start",203)
6350   FOR I=0 TO 2
6360     SPRITE I,ON:SPRITE I,PRIORITY 4
6370   NEXT I
6380   SPRITE 1,ANIM 1,2,10
6390   SPRITE 2,ANIM 2,2,12
6400   QT=0
6410   MQ=0:MB=0
6420   CALL IN_WAITRELEASE
6430   REPEAT
6440     FRAME
6450     T=T+1
6460     IF T MOD 8=0 THEN F=1-F
6470     IF T MOD 6=0 THEN
6480       GF=(GF+1) MOD 4
6490       CALL MAKEGEM(GF)
6500     END IF
6510     GFX PALETTE 3,10,RGB(INT(180+75*SIN(T/10)),INT(180+75*SIN(T/10+2.1)),INT(180+75*SIN(T/10+4.2)))
6520     SPRITE 0,16+INT(270*(1+SIN(T/55))),168,F
6530     SPRITE 1,16+INT(290*(1+COS(T/70))),150+INT(6*SIN(T/9))
6540     SPRITE 2,16+INT(110*(1+SIN(T/90+2))),168
6550     K$=INKEY$
6560   UNTIL K$=" " OR K$="q" OR K$="Q" OR FN IN_ANYA(0) OR FN IN_ANYSTART(0) OR FN IN_ANYBACK(0)
6570   IF K$="q" OR K$="Q" OR FN IN_ANYBACK(0) THEN QT=1
6580   SOUND OFF
6590   MQ=0
6600 END PROC
6610 ' ====================================================================== a new level
6620 PROC NEWLEVEL
6630   LOCAL X,Y,R,I,N,BP,GP,AX,AY,L,V,GT,NF,NW
6640   CALL CLEARSCREEN
6650   BP=12+FN CLAMP(LEVEL*2,0,14)
6660   GP=7
6670   FOR Y=0 TO GH-1
6680     FOR X=0 TO GW-1
6690       FL(X,Y)=0:MD(X,Y)=0
6700       IF X=0 OR Y=0 OR X=GW-1 OR Y=GH-1 THEN
6710         C(X,Y)=2
6720       ELSE
6730         C(X,Y)=1
6740         R=FN RN(100)
6750         IF R<BP THEN C(X,Y)=4
6760         IF R>=BP AND R<BP+GP THEN C(X,Y)=5
6770         IF R>=BP+GP AND R<BP+GP+3 THEN C(X,Y)=0
6780       END IF
6790     NEXT X
6800   NEXT Y
6810   N=FN CLAMP(2+LEVEL\2,2,8)
6820   FOR I=1 TO N
6830     AY=FN RN(8)+2:AX=FN RN(30)+4:L=FN RN(5)+4
6840     FOR X=AX TO AX+L-1
6850       IF X<GW-1 THEN C(X,AY)=3
6860     NEXT X
6870   NEXT I
6880   ' the miner's corner: empty, no boulders above
6890   FOR Y=1 TO 2
6900     FOR X=1 TO 3
6910       C(X,Y)=0
6920     NEXT X
6930   NEXT Y
6940   DXC=30+FN RN(8):DYC=6+FN RN(5)
6950   C(DXC,DYC)=6
6960   GT=0
6970   FOR Y=1 TO GH-2
6980     FOR X=1 TO GW-2
6990       IF C(X,Y)=5 THEN GT=GT+1
7000     NEXT X
7010   NEXT Y
7020   NEED=FN CLAMP(6+LEVEL*2,6,24)
7030   WHILE GT<NEED+4
7040     AX=FN RN(GW-2)+1:AY=FN RN(GH-2)+1
7050     IF C(AX,AY)=1 THEN
7060       C(AX,AY)=5
7070       GT=GT+1
7080     END IF
7090   WEND
7100   ' the enemies: fireflies (1) and worms (2), each in a little pocket far from the miner
7110   NF=FN CLAMP((LEVEL+1)\2,1,5)
7120   NW=FN CLAMP(LEVEL\2,0,3)
7130   NE=NF+NW
7140   FOR I=1 TO NE
7150     REPEAT
7160       AX=FN RN(26)+10:AY=FN RN(GH-2)+1
7170       V=C(AX,AY)
7180     UNTIL V=0 OR V=1
7190     C(AX,AY)=0
7200     IF C(AX+1,AY)=1 THEN C(AX+1,AY)=0
7210     IF C(AX-1,AY)=1 THEN C(AX-1,AY)=0
7220     IF C(AX,AY-1)=4 OR C(AX,AY-1)=5 THEN C(AX,AY-1)=1
7230     ENX(I)=AX:ENY(I)=AY:ED(I)=FN RN(4):ES(I)=0:ENA(I)=1
7240     ET(I)=1
7250     IF I>NF THEN ET(I)=2
7260   NEXT I
7270   GM=0:DOPEN=0:TM=FN CLAMP(170-LEVEL*5,90,170):MX=1:MY=1:MDEAD=0:WONF=0
7280   WF=0:WALKING=0:LDX=0:LDY=0:LB=0:CHN=0:LASTG=-999:FRN=0:TKN=0:ENDT=0:ENDRES=0
7290   CALL DRAWALL
7300   CALL HUD
7310   SPRITE 0,ON:SPRITE 0,PRIORITY 4:SPRITE 0,16*MX,24*MY,0
7320   FOR I=1 TO NE
7330     SPRITE I,ON:SPRITE I,PRIORITY 3
7340     SPRITE I,16*ENX(I),24*ENY(I)
7350     IF ET(I)=1 THEN SPRITE I,ANIM 1,2,6 ELSE SPRITE I,ANIM 2,2,10
7360   NEXT I
7370   FOR I=0 TO 8
7380     SPRITE 20+I,PRIORITY 7
7390   NEXT I
7400   CALL BIGTEXT(100,"LEVEL "+STR$(LEVEL),12,5)
7410   FOR I=1 TO 60
7420     FRAME
7430   NEXT I
7440   GFX CLEAR
7450 END PROC
7460 ' ====================================================================== a level
7470 PROC RUNLEVEL
7480   RES=-1
7490   WHILE RES<0
7500     CALL IN_POLL
7510     IF IN_DX<>0 OR IN_DY<>0 THEN LDX=IN_DX:LDY=IN_DY
7520     IF IN_BTN THEN LB=1
7530     IF FN IN_SERVICE() THEN CALL PAUSEGAME
7540     IF IN_KEY=77 THEN CALL MUSICSWITCH
7550     IF IN_QUIT THEN RES=2
7560     IF RES<0 THEN
7570       FRN=FRN+1
7580       CALL ANIMATE
7590       IF FRN MOD TK=0 THEN CALL TICK
7600       IF FRN MOD 60=0 THEN CALL SECOND
7610     END IF
7620     FRAME
7630   WEND
7640 END PROC
7650 PROC PAUSEGAME
7660   SOUND OFF
7670   MQ=0
7680   CALL BIGTEXT(110,"PAUSED",12,5)
7690   CALL IN_PAUSELOOP
7700   GFX CLEAR
7710 END PROC
7720 PROC MUSICSWITCH
7730   MON=1-MON
7740   IF MON=0 THEN
7750     SOUND OFF
7760     MQ=0
7770   END IF
7780 END PROC
7790 ' one game step (6 frames): the miner, the falling things, the enemies
7800 PROC TICK
7810   TKN=TKN+1
7820   OMX=MX:OMY=MY
7830   IF MDEAD=0 AND WONF=0 AND TKN MOD MS=0 THEN CALL MOVEMINER
7840   CALL PHYSICS
7850   CALL ENEMIES
7860   CALL CHECKHITS
7870 END PROC
7880 ' every frame: the gems glint (the four characters of the gem are redefined, every gem on the screen changes at once),
7890 ' the door's frame glows (a palette change), the explosion sprites go out, the status line, the end of the level
7900 PROC ANIMATE
7910   LOCAL I,V
7920   IF FRN MOD 6=0 THEN
7930     GF=(GF+1) MOD 4
7940     CALL MAKEGEM(GF)
7950   END IF
7960   IF DOPEN=1 AND FRN MOD 2=0 THEN
7970     V=INT(165+90*SIN(FRN/5))
7980     GFX PALETTE 2,229,RGB(V,V,INT(V*0.7))
7990   END IF
8000   FOR I=0 TO 8
8010     IF EXT(I)>0 THEN
8020       EXT(I)=EXT(I)-1
8030       IF EXT(I)=0 THEN SPRITE 20+I,OFF
8040     END IF
8050   NEXT I
8060   IF FRN MOD 6=3 THEN CALL HUD
8070   IF ENDT>0 THEN
8080     ENDT=ENDT-1
8090     IF WONF=1 AND ENDT<ENDWAIT-14 THEN SPRITE 0,OFF
8100     IF ENDT=0 THEN RES=ENDRES
8110   END IF
8120 END PROC
8130 PROC SECOND
8140   IF MDEAD=1 OR WONF=1 THEN EXIT PROC
8150   TM=TM-1
8160   IF TM<=0 THEN
8170     TM=0
8180     CALL KILLMINER
8190   ELSE
8200     IF TM<=15 THEN SFX 9
8210   END IF
8220 END PROC
8230 PROC ADDSCORE(N)
8240   SC=SC+N
8250   IF SC>=NXL THEN
8260     LV=LV+1
8270     NXL=NXL+2000
8280     SFX 10
8290   END IF
8300 END PROC
8310 ' ---------------------------------------------------------------------- the miner
8320 PROC MOVEMINER
8330   LOCAL DX,DY,TX,TY,V,MV,B
8340   DX=LDX:DY=LDY:B=LB
8350   LDX=0:LDY=0:LB=0
8360   IF DX<>0 THEN DY=0
8370   IF DX=0 AND DY=0 THEN
8380     IF WALKING=1 THEN
8390       SPRITE 0,IMAGE 0
8400       WALKING=0
8410     END IF
8420     EXIT PROC
8430   END IF
8440   TX=MX+DX:TY=MY+DY
8450   V=C(TX,TY)
8460   IF B=1 THEN
8470     ' SPACE: take what is there without stepping
8480     IF V=1 THEN
8490       CALL SETC(TX,TY,0)
8500       SFX 1
8510     END IF
8520     IF V=5 THEN CALL TAKEGEM(TX,TY)
8530     EXIT PROC
8540   END IF
8550   MV=0
8560   IF V=0 THEN MV=1
8570   IF V=1 THEN
8580     CALL SETC(TX,TY,0)
8590     SFX 1
8600     MV=1
8610   END IF
8620   IF V=5 THEN
8630     CALL TAKEGEM(TX,TY)
8640     MV=1
8650   END IF
8660   IF V=4 AND DY=0 THEN
8670     IF C(TX+DX,TY)=0 AND FN OCC(TX+DX,TY)=0 THEN
8680       CALL SETC(TX+DX,TY,4)
8690       CALL SETC(TX,TY,0)
8700       MD(TX+DX,TY)=TKN
8710       SFX 4
8720       MV=1
8730     END IF
8740   END IF
8750   IF V=6 AND DOPEN=1 THEN
8760     MV=1
8770     CALL ENTERDOOR
8780   END IF
8790   IF MV=1 THEN
8800     MX=TX:MY=TY
8810     SPRITE 0,MOVETO MX*16,MY*24,TK*MS
8820     WF=1-WF
8830     SPRITE 0,IMAGE WF
8840     WALKING=1
8850   END IF
8860 END PROC
8870 PROC TAKEGEM(X,Y)
8880   LOCAL F
8890   CALL SETC(X,Y,0)
8900   GM=GM+1
8910   IF DOPEN=1 THEN CALL ADDSCORE(20) ELSE CALL ADDSCORE(10)
8920   ' a chain of pickups rises by a semitone each (up to an octave)
8930   IF FRN-LASTG<70 THEN CHN=CHN+1 ELSE CHN=0
8940   IF CHN>12 THEN CHN=12
8950   LASTG=FRN
8960   F=660*EXP(CHN*0.0577623)
8970   ' the pickup tone is part of the music: louder than the music (V0.1) for one second after each pitch change, then back to music volume
8980   SOUND OFF 7
8990   WAVE 7,1
9000   ENVELOPE 7
9010   SOUND 7,F,1,0.3
9020   SOUND 7,F,1.5,0.1
9030   IF GM>=NEED AND DOPEN=0 THEN CALL OPENDOOR
9040 END PROC
9050 PROC OPENDOOR
9060   DOPEN=1
9070   SFX 6
9080   CALL SETC(DXC,DYC,6)
9090 END PROC
9100 PROC ENTERDOOR
9110   WONF=1
9120   SFX 7
9130   SPRITE 0,MOVETO DXC*16,DYC*24,TK
9140   ENDWAIT=60:ENDT=60:ENDRES=1
9150 END PROC
9160 PROC KILLMINER
9170   IF MDEAD=1 OR WONF=1 THEN EXIT PROC
9180   MDEAD=1
9190   SPRITE 0,OFF
9200   CALL BOOM(MX,MY,0)
9210   SFX 8
9220   ENDWAIT=90:ENDT=90:ENDRES=0
9230 END PROC
9240 ' ---------------------------------------------------------------------- falling things
9250 ' boulders and gems fall, roll off round things (boulders and gems) and crush the miner and the enemies they hit while falling
9260 PROC PHYSICS
9270   LOCAL X,Y,V,B,F,MVD
9280   FOR Y=GH-2 DOWNTO 1
9290     FOR X=1 TO GW-2
9300       V=C(X,Y)
9310       IF (V=4 OR V=5) AND MD(X,Y)<>TKN THEN
9320         F=FL(X,Y)
9330         B=C(X,Y+1)
9340         MVD=0
9350         IF B=0 THEN
9360           IF FN OCC(X,Y+1)=1 THEN
9370             IF F=1 THEN CALL CRUSH(X,Y+1)
9380           ELSE
9390             CALL MOVEOBJ(X,Y,X,Y+1)
9400             MVD=1
9410           END IF
9420         END IF
9430         IF B=4 OR B=5 THEN
9440           IF C(X-1,Y)=0 AND C(X-1,Y+1)=0 AND FN OCC(X-1,Y)=0 AND FN OCC(X-1,Y+1)=0 THEN
9450             CALL MOVEOBJ(X,Y,X-1,Y)
9460             MVD=1
9470           END IF
9480           IF MVD=0 AND C(X+1,Y)=0 AND C(X+1,Y+1)=0 AND FN OCC(X+1,Y)=0 AND FN OCC(X+1,Y+1)=0 THEN
9490             CALL MOVEOBJ(X,Y,X+1,Y)
9500             MVD=1
9510           END IF
9520         END IF
9530         IF MVD=0 AND F=1 THEN
9540           FL(X,Y)=0
9550           SFX 3
9560         END IF
9570       END IF
9580     NEXT X
9590   NEXT Y
9600 END PROC
9610 PROC MOVEOBJ(X1,Y1,X2,Y2)
9620   LOCAL V
9630   V=C(X1,Y1)
9640   CALL SETC(X1,Y1,0)
9650   CALL SETC(X2,Y2,V)
9660   FL(X1,Y1)=0:FL(X2,Y2)=1:MD(X2,Y2)=TKN
9670 END PROC
9680 PROC CRUSH(X,Y)
9690   LOCAL E
9700   E=FN ENEMYAT(X,Y)
9710   IF E>0 THEN CALL KILLENEMY(E)
9720   IF MDEAD=0 AND WONF=0 AND MX=X AND MY=Y THEN CALL KILLMINER
9730 END PROC
9740 ' ---------------------------------------------------------------------- explosions
9750 ' G=1: the 3 x 3 cells around X,Y turn into gems (a firefly), G=0: into empty cells (the miner), G=2: only the middle one flashes (a worm)
9760 PROC BOOM(X,Y,G)
9770   LOCAL I,J,A,B,E,R,V
9780   SFX 5
9790   R=1
9800   IF G=2 THEN R=0
9810   FOR J=-R TO R
9820     FOR I=-R TO R
9830       A=X+I:B=Y+J
9840       IF A>=1 AND A<=GW-2 AND B>=1 AND B<=GH-2 THEN
9850         V=C(A,B)
9860         IF V<>2 AND V<>6 THEN
9870           IF G=1 THEN CALL SETC(A,B,5)
9880           IF G=0 THEN CALL SETC(A,B,0)
9890           FL(A,B)=0
9900         END IF
9910         E=FN ENEMYAT(A,B)
9920         IF E>0 AND G<>2 THEN CALL KILLENEMY(E)
9930         IF G<>2 AND MDEAD=0 AND WONF=0 AND A=MX AND B=MY THEN CALL KILLMINER
9940         CALL SPARK((J+1)*3+I+1,A,B)
9950       END IF
9960     NEXT I
9970   NEXT J
9980 END PROC
9990 PROC SPARK(N,X,Y)
10000   SPRITE 20+N,X*16,24*Y
10010   SPRITE 20+N,ON
10020   SPRITE 20+N,ANIM 3,2,5
10030   EXT(N)=24
10040 END PROC
10050 ' ---------------------------------------------------------------------- the enemies
10060 PROC KILLENEMY(E)
10070   IF ENA(E)=0 THEN EXIT PROC
10080   ENA(E)=0
10090   SPRITE E,OFF
10100   IF ET(E)=1 THEN
10110     CALL ADDSCORE(100)
10120     CALL BOOM(ENX(E),ENY(E),1)
10130   ELSE
10140     CALL ADDSCORE(50)
10150     CALL BOOM(ENX(E),ENY(E),2)
10160   END IF
10170 END PROC
10180 PROC ENEMIES
10190   LOCAL I
10200   FOR I=1 TO NE
10210     IF ENA(I)=1 THEN
10220       ES(I)=ES(I)+1
10230       IF ET(I)=1 THEN
10240         IF ES(I) MOD FS=0 THEN CALL FIREFLY(I)
10250       ELSE
10260         IF ES(I) MOD WS=0 THEN CALL WORM(I)
10270       END IF
10280     END IF
10290   NEXT I
10300 END PROC
10310 ' a firefly keeps the wall at its left hand: left if it can, else straight on, else right, else back
10320 PROC FIREFLY(I)
10330   LOCAL K,D,NX,NY,OX,OY
10340   OX=ENX(I):OY=ENY(I)
10350   FOR K=0 TO 3
10360     D=(ED(I)+3+K) MOD 4
10370     NX=OX+DXT(D):NY=OY+DYT(D)
10380     IF C(NX,NY)=0 AND FN ENEMYAT(NX,NY)=0 THEN
10390       ED(I)=D:ENX(I)=NX:ENY(I)=NY
10400       SPRITE I,MOVETO NX*16,NY*24,TK*FS
10410       CALL SWAPCHECK(OX,OY,NX,NY)
10420       EXIT
10430     END IF
10440   NEXT K
10450 END PROC
10460 ' a worm heads for the miner and eats the dirt in its way
10470 PROC WORM(I)
10480   LOCAL K,D,NX,NY,OX,OY,DX,DY,TD(4),N,V
10490   OX=ENX(I):OY=ENY(I)
10500   DX=SGN(MX-OX):DY=SGN(MY-OY)
10510   N=0
10520   TD(0)=-1:TD(1)=-1
10530   IF DX>0 THEN TD(0)=1
10540   IF DX<0 THEN TD(0)=3
10550   IF DY>0 THEN TD(1)=2
10560   IF DY<0 THEN TD(1)=0
10570   IF ABS(MY-OY)>ABS(MX-OX) THEN
10580     K=TD(0):TD(0)=TD(1):TD(1)=K
10590   END IF
10600   TD(2)=FN RN(4):TD(3)=(TD(2)+2) MOD 4
10610   IF FN RN(100)<20 THEN
10620     K=TD(0):TD(0)=TD(2):TD(2)=K
10630   END IF
10640   FOR K=0 TO 3
10650     D=TD(K)
10660     IF D>=0 THEN
10670       NX=OX+DXT(D):NY=OY+DYT(D)
10680       V=C(NX,NY)
10690       IF (V=0 OR V=1) AND FN ENEMYAT(NX,NY)=0 THEN
10700         IF V=1 THEN CALL SETC(NX,NY,0)
10710         ENX(I)=NX:ENY(I)=NY
10720         SPRITE I,MOVETO NX*16,NY*24,TK*WS
10730         CALL SWAPCHECK(OX,OY,NX,NY)
10740         EXIT
10750       END IF
10760     END IF
10770   NEXT K
10780 END PROC
10790 ' the miner and an enemy that change places in one step would pass through each other
10800 PROC SWAPCHECK(OX,OY,NX,NY)
10810   IF MDEAD=0 AND WONF=0 AND NX=OMX AND NY=OMY AND OX=MX AND OY=MY THEN CALL KILLMINER
10820 END PROC
10830 PROC CHECKHITS
10840   LOCAL I
10850   FOR I=1 TO NE
10860     IF ENA(I)=1 AND MDEAD=0 AND WONF=0 AND ENX(I)=MX AND ENY(I)=MY THEN CALL KILLMINER
10870   NEXT I
10880 END PROC
10890 ' ====================================================================== the game
10900 PROC PLAYGAME
10910   SC=0:LV=3:LEVEL=1:NXL=2000:QT=0:IN_QUIT=0
10920   CALL IN_WAITRELEASE
10930   WHILE LV>0 AND QT=0
10940     CALL NEWLEVEL
10950     CALL RUNLEVEL
10960     IF RES=1 THEN
10970       CALL LEVELDONE
10980       LEVEL=LEVEL+1
10990     END IF
11000     IF RES=0 THEN LV=LV-1
11010     IF RES=2 THEN QT=1
11020   WEND
11030   SOUND OFF
11040   MQ=0
11050   IF QT=0 THEN CALL OVER
11060 END PROC
11070 PROC LEVELDONE
11080   LOCAL I,B
11090   B=TM*5
11100   CALL ADDSCORE(B)
11110   CALL HUD
11120   CALL BIGTEXT(70,"LEVEL CLEAR",12,4)
11130   CALL CTR(13,"TIME BONUS "+STR$(B),202)
11140   FOR I=1 TO 100
11150     FRAME
11160   NEXT I
11170   GFX CLEAR
11180 END PROC
11190 PROC OVER
11200   LOCAL I
11210   CALL BIGTEXT(90,"GAME OVER",12,6)
11220   CALL CTR(14,"SCORE "+FN PAD$(SC,6),201)
11230   FOR I=1 TO 150
11240     FRAME
11250   NEXT I
11260   GFX CLEAR
11270 END PROC
11280 ' ====================================================================== music
11290 ' made up bar by bar: once a second the handler checks how much is still queued and adds one bar (a lead and a bass on the channels 1 and 2);
11300 ' with little time left the tempo goes up
11310 PROC MUSICTICK
11320   LOCAL TP,BS,A$
11330   IF MON=0 THEN EXIT PROC
11340   MQ=MQ-1
11350   IF MQ<=1.2 THEN
11360     TP=110
11370     IF TM<30 THEN TP=165
11380     BS=240/TP
11390     A$=STR$(TP)
11400     PLAY "T"+A$+" W1 V0.1 O4 L8 "+LD$(MB MOD 4),"T"+A$+" W2 V0.16 O2 L4 "+BS$(MB MOD 4)
11410     MB=MB+1
11420     MQ=MQ+BS
11430   END IF
11440 END PROC
11450 ' ====================================================================== sprites (generated)
