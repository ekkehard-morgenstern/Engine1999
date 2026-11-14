1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' Y2K38 - the sequel to Y2K COUNTDOWN, for Engine1999 (window version only). A countdown to 03:14:07 UTC on 19 January 2038, when a signed 32-bit
20 ' time_t runs out and the clock jumps back to 13 December 1901, 20:45:52.
30 ' Nine parts, one after the other, to one dark industrial soundtrack: boot, hex rain, copper bars, a server room flythrough (the 3D sector
40 ' renderer), the timeline, a corrupted tape picture (a BMP file written and loaded back), the countdown, the overflow, the wrap.
50 ' Keys: Q or ESC ends the demo. Start with an argument to begin at a part: engine1999-y2k38 3 (0..8).
60 ' All motion is driven by the clock, so it runs at the same speed on a fast and on a slow machine.
70 ' --- globals: TK ticks since the start, TT seconds, TB beats (150 per minute), PU pulse (1 on the beat, falling to 0) ---
80 DIM PS(9),PE(9),FB(9,6),DPAT$(9),MEL$(24),MV$(9,8),CR(8),CM(8),PENT(5),HY(40),HS(40),RK(60),SG$(25)
90 LOAD LIBRARY "w3dkit"
100 ' (a FUNC has to be defined before the lines that call it)
110 FUNC CLAMP(V,A,B)
120   IF V<A THEN RETURN A
130   IF V>B THEN RETURN B
140   RETURN V
150 END FUNC
160 ' a note as music string text: octave and name (note 48 = C4, 57 = A4 = 440 Hz)
170 FUNC NN$(N)
180   LOCAL A$
190   A$=MID$("C C+D D+E F F+G G+A A+B ",(N MOD 12)*2+1,2)
200   IF RIGHT$(A$,1)=" " THEN A$=LEFT$(A$,1)
210   RETURN "O"+CHR$(48+N\12)+A$
220 END FUNC
230 ' drums: K = kick (KICK), S = snare (SNARE), anything else a rest; 16 sixteenths
240 FUNC DRUMS$(P$)
250   LOCAL I,C$,R$
260   FOR I=1 TO 16
270     C$=MID$(P$,I,1)
280     IF C$="K" THEN R$=R$+"I13 V0.8 O1 A16 "
290     IF C$="S" THEN R$=R$+"I14 V0.45 O3 C16 "
300     IF C$<>"K" AND C$<>"S" THEN R$=R$+"R16 "
310   NEXT I
320   RETURN R$
330 END FUNC
340 FUNC HATS$(P$)
350   LOCAL I,R$
360   FOR I=1 TO 16
370     IF MID$(P$,I,1)="x" THEN R$=R$+"I15 V0.08 O7 C32 R32 " ELSE R$=R$+"R16 "
380   NEXT I
390   RETURN R$
400 END FUNC
410 ' a color value that can't go wrong: every part is limited to 0..255
420 FUNC RGBC(R,G,B)
430   RETURN RGB(FN CLAMP(INT(R),0,255),FN CLAMP(INT(G),0,255),FN CLAMP(INT(B),0,255))
440 END FUNC
450 ' the 32 bits of a number (0 .. 2147483647 = 7FFFFFFF, 2147483648 .. 4294967295 for the wrapped values), highest bit first
460 FUNC BITS$(V)
470   LOCAL I,R$
480   FOR I=31 TO 0 STEP -1
490     IF INT(V/(2^I)) MOD 2=1 THEN R$=R$+"1" ELSE R$=R$+"0"
500   NEXT I
510   RETURN R$
520 END FUNC
530 FUNC HEXN$(V)
540   LOCAL I,D,R$
550   FOR I=7 TO 0 STEP -1
560     D=INT(V/(16^I)) MOD 16
570     R$=R$+MID$("0123456789ABCDEF",D+1,1)
580   NEXT I
590   RETURN R$
600 END FUNC
610 ' ====================================================================== main program
620 BREAK OFF:LOAD LIBRARY "banner":CALL BN_2D("Y2K38")
630 ON ERROR GOTO FAILED
640 RANDOMIZE 2038
650 NP=9
660 S=0
670 FOR I=0 TO NP-1
680   READ B
690   PS(I)=S
700   S=S+B*96
710   PE(I)=S
720 NEXT I
730 ' the bars of each part (a bar is 1.6 seconds)
740 DATA 8,8,8,12,8,8,8,6,6
750 ' the chord progression: roots and 1 = minor (Am Am F F Am Am G E)
760 DATA 9,9,5,5,9,9,7,4
770 DATA 1,1,0,0,1,1,0,0
780 ' the pentatonic scale of the lead
790 DATA 0,3,5,7,10
800 ' the first bar in which each voice plays in each part (lead, drums, bass, arp, pad, hats); 99 = never
810 DATA 99,4,2,0,0,6
820 DATA 99,0,0,0,0,0
830 DATA 0,0,0,0,0,0
840 DATA 4,0,0,0,0,0
850 DATA 0,0,0,0,0,0
860 DATA 99,4,0,0,0,4
870 DATA 99,0,0,0,0,0
880 DATA 0,0,0,2,0,0
890 DATA 99,99,0,99,0,99
900 ' the drum pattern of each part
910 DATA "K...K...K...K...","K...S...K.K.S...","K..KS...K.K.S...","K.K.S...K.K.S.SS","K...S...K.K.S...","K...K...K...K...","K...K...K...K...","SSSSSSSSSSSSSSSS","................"
920 SP=VAL(ARG$(1))
930 IF SP<0 OR SP>=NP THEN SP=0
940 FOR I=0 TO 7:READ CR(I):NEXT I
950 FOR I=0 TO 7:READ CM(I):NEXT I
960 FOR I=0 TO 4:READ PENT(I):NEXT I
970 FOR I=0 TO 8:FOR K=0 TO 5:READ FB(I,K):NEXT K:NEXT I
980 FOR I=0 TO 8:READ DPAT$(I):NEXT I
990 QT=0:FA=0:BF=0:DK=0
1000 CALL SETUP
1010 CALL MUSIC(SP)
1020 FOR P=SP TO NP-1
1030   IF QT=0 THEN
1040     CALL CLOCK
1050     CALL NEWPART(P)
1060     IF P=0 THEN CALL P_BOOT
1070     IF P=1 THEN CALL P_HEX
1080     IF P=2 THEN CALL P_RASTER
1090     IF P=3 THEN CALL P_SERVER
1100     IF P=4 THEN CALL P_TIMELINE
1110     IF P=5 THEN CALL P_TAPE
1120     IF P=6 THEN CALL P_COUNT
1130     IF P=7 THEN CALL P_OVERFLOW
1140     IF P=8 THEN CALL P_WRAP
1150   END IF
1160 NEXT P
1170 CALL FINISH
1180 PRINT "Y2K38 - an Engine1999 demo. Use 64 bits."
1190 END
1200 LABEL FAILED
1210 SOUND OFF
1220 GFX RESET ALL
1230 CLS
1240 PRINT "Error: ";ERR$;" in line ";ERL
1250 END
1260 ' ====================================================================== common procedures
1270 PROC SETUP
1280   GFX RESET ALL
1290   CLS
1300   GFX BACKDROP RGB(0,0,0)
1310   MOUSE HIDE
1320   CALL MAKEBITS
1330   CALL SFXDEF
1340 END PROC
1350 PROC FINISH
1360   SOUND OFF
1370   SPRITE 0,OFF
1380   GFX RESET ALL
1390   CLS
1400   COLOR 140
1410 END PROC
1420 PROC CLOCK
1430   TK=TI-T0
1440   TT=TK/60
1450   TB=TK/24
1460   PU=1-(TB-INT(TB))
1470 END PROC
1480 ' seconds into the part P
1490 FUNC LT(P)
1500   RETURN TT-PS(P)/60
1510 END FUNC
1520 ' sets the clock for a new part: a white flash, all sprites hidden, graphics cleared
1530 PROC NEWPART(P)
1540   LOCAL I
1550   GFX RESET
1560   FOR I=0 TO 247
1570     SPRITE I,OFF
1580   NEXT I
1590   FA=255:DK=0
1600   CALL LAYERS_TOP
1610   CALL PALFIX
1620   GFX TARGET 0:GFX CLEAR:GFX TARGET 3:GFX CLEAR
1630   GFX LAYER 0,ON:GFX LAYER 3,OFF
1640   BF=3:GFX TARGET BF
1650   TXT FILL 0,0,79,24,32,0,0
1660   CLS
1670 END PROC
1680 ' the two graphics layers go above the text screen: order bottom to top = tiles, text, layer 0, layer 3, sprites
1690 PROC LAYERS_TOP
1700   GFX PRIORITY 0,2
1710   GFX PRIORITY 1,0
1720 END PROC
1730 ' fixed colors on both graphics layers: 1 white, 2 black, 3 yellow, 4 cyan, 5 red, 6 green, 7 blue, 8 orange, 9 magenta, 10 grey
1740 PROC PAL(I,R,G,B)
1750   LOCAL C
1760   C=FN RGBC(R,G,B)
1770   GFX PALETTE 0,I,C
1780   GFX PALETTE 3,I,C
1790 END PROC
1800 PROC PALFIX
1810   CALL PAL(1,255,255,255):CALL PAL(2,0,0,0):CALL PAL(3,255,230,0):CALL PAL(4,0,230,255):CALL PAL(5,255,40,40)
1820   CALL PAL(6,40,255,60):CALL PAL(7,60,90,255):CALL PAL(8,255,150,0):CALL PAL(9,255,60,220):CALL PAL(10,130,130,150)
1830   LOCAL I
1840   FOR I=1 TO 10
1850     GFX PALETTE 4,I,GFX("PALETTE",0,I)
1860   NEXT I
1870 END PROC
1880 ' a rainbow color: hue H (radians), brightness V (0..1)
1890 PROC HUE(I,H,V)
1900   CALL PAL(I,INT(V*(127+127*SIN(H))),INT(V*(127+127*SIN(H+2.094))),INT(V*(127+127*SIN(H+4.189))))
1910 END PROC
1920 ' the text screen as a gradient: 25 rows of background color, palette 16..40 of the text layer
1930 PROC BGBANDS(R0,G0,B0,R1,G1,B1)
1940   LOCAL Y,F
1950   FOR Y=0 TO 24
1960     F=Y/24
1970     GFX PALETTE 2,16+Y,RGB(INT(R0+(R1-R0)*F),INT(G0+(G1-G0)*F),INT(B0+(B1-B0)*F))
1980     TXT FILL 0,Y,79,Y,32,0,16+Y
1990   NEXT Y
2000 END PROC
2010 ' end of a frame: a flash that fades, a darkening (DK 0..255), swap the buffers, wait for the display, look at the keys
2020 PROC FLIP
2030   LOCAL K$
2040   IF FA>0 THEN
2050     GFX PALETTE 0,250,RGB(255,255,255,FA)
2060     GFX PALETTE 3,250,RGB(255,255,255,FA)
2070     GFX LINE 0,0,639,299,250,BF
2080     FA=FA-22
2090     IF FA<0 THEN FA=0
2100   END IF
2110   IF DK>0 THEN
2120     GFX PALETTE 0,249,RGB(0,0,0,FN CLAMP(DK,0,255))
2130     GFX PALETTE 3,249,RGB(0,0,0,FN CLAMP(DK,0,255))
2140     GFX LINE 0,0,639,299,249,BF
2150   END IF
2160   GFX LAYER BF,ON
2170   GFX LAYER 3-BF,OFF
2180   BF=3-BF
2190   GFX TARGET BF
2200   GFX CLEAR
2210   K$=INKEY$
2220   IF K$="q" OR K$="Q" OR K$=CHR$(27) THEN QT=1
2230   FRAME
2240   CALL CLOCK
2250 END PROC
2260 ' text centered in a buffer at row Y, scale SX x SY, shadow color 2
2270 PROC CTEXT(Y,A$,C,SX,SY)
2280   LOCAL X
2290   X=320-LEN(A$)*4*SX
2300   GFX TEXT X+SX,Y+SY,A$,2,SX,SY
2310   GFX TEXT X,Y,A$,C,SX,SY
2320 END PROC
2330 ' the 0 and 1 of the falling bits: sprite images 100 (zero) and 101 (one), cut from the built-in font, green with a black edge
2340 PROC MAKEBITS
2350   LOCAL C,X,Y,A$,B,N
2360   DIM GB(18,26)
2370   FOR C=0 TO 1
2380     FOR Y=0 TO 25
2390       FOR X=0 TO 17
2400         GB(X,Y)=0
2410       NEXT X
2420     NEXT Y
2430     FOR Y=0 TO 11
2440       B=TXT("BUILTIN",48+C,Y)
2450       FOR X=0 TO 7
2460         IF (B AND (128 SHR X))<>0 THEN
2470           GB(2+X*2,2+Y*2)=1:GB(3+X*2,2+Y*2)=1:GB(2+X*2,3+Y*2)=1:GB(3+X*2,3+Y*2)=1
2480         END IF
2490       NEXT X
2500     NEXT Y
2510     A$=""
2520     FOR Y=0 TO 23
2530       FOR X=0 TO 15
2540         IF GB(X+1,Y+1)=1 THEN
2550           A$=A$+CHR$(6)
2560         ELSE
2570           N=GB(X,Y+1)+GB(X+2,Y+1)+GB(X+1,Y)+GB(X+1,Y+2)
2580           IF N>0 THEN A$=A$+CHR$(2) ELSE A$=A$+CHR$(0)
2590         END IF
2600       NEXT X
2610     NEXT Y
2620     SPRITE DEF 100+C,A$
2630   NEXT C
2640 END PROC
2650 PROC SFXDEF
2660   SFX 1,"I12 V0.2 ML X0,0.05,0,0.1 H880,16"
2670   SFX 2,"W3 V0.9 ML X0,0.02,0.4,0.5 S800,60,2"
2680   SFX 3,"I12 V0.22 ML X0,0.15,0,0.05 H1319,16"
2690   SFX 4,"W3 V0.3 ML X0,0.01,0.2,0.1 S6000,300,16"
2700   SFX 5,"W3 V0.9 ML X0,0.01,0.8,0.8 S3000,30,8 S2000,60,4"
2710   SFX 6,"I18 V0.25 ML X0,0.01,0.3,0.2 S2400,200,8"
2720 END PROC
2730 ' ====================================================================== part 0: boot (the title and the 32 bit counter)
2740 PROC P_BOOT
2750   LOCAL I,X,Y,DT,R,N,T$,V,B$
2760   CALL BGBANDS(0,6,0,0,24,18)
2770   T$="Y2K38"
2780   REPEAT
2790     ' green static
2800     FOR I=0 TO 79
2810       GFX PSET INT(RND(1)*640),INT(RND(1)*300),6
2820     NEXT I
2830     FOR I=0 TO 4
2840       CALL HUE(17+I,I*0.7+TT*3,0.5+0.5*PU)
2850       DT=TT-(0.4+0.25*I)
2860       IF DT>0 THEN
2870         Y=60-300*EXP(-3.2*DT)*ABS(COS(DT*7.5))
2880         R=FN CLAMP(TT-2.6,0,1)
2890         Y=Y+R*8*SIN(TT*4+I*0.7)
2900         X=40+I*112
2910         GFX TEXT X+7,Y+7,MID$(T$,I+1,1),2,14,8
2920         GFX TEXT X,Y,MID$(T$,I+1,1),17+I,14,8
2930       END IF
2940     NEXT I
2950     IF TT>2.2 THEN CALL CTEXT(176,"THE EPOCH STRIKES BACK",4,3,2)
2960     IF TT>3.6 THEN CALL CTEXT(214,"03:14:07 UTC  19 JANUARY 2038",3,2,2)
2970     ' the counter: the seconds since 1 January 1970, running up to 2147483647
2980     V=2147483647-INT(12000000*EXP(-TT*0.35))
2990     IF V>2147483647 THEN V=2147483647
3000     B$=FN BITS$(V)
3010     GFX TEXT 64+2,262+2,B$,2,2,2
3020     GFX TEXT 64,262,B$,6,2,2
3030     CALL FLIP
3040   UNTIL QT OR TI-T0>=PE(0)
3050 END PROC
3060 ' ====================================================================== part 1: hex rain
3070 PROC P_HEX
3080   LOCAL I,K,X,Y,H,C,LS$,N,T$,G$
3090   FOR I=0 TO 9
3100     CALL PAL(16+I,0,INT(40+200*I/9),INT(20+80*I/9))
3110   NEXT I
3120   CALL BGBANDS(0,2,0,0,10,4)
3130   FOR I=0 TO 39
3140     HY(I)=RND(1)*30-10:HS(I)=6+RND(1)*14
3150   NEXT I
3160   T$="0x7FFFFFFF"
3170   G$="0123456789ABCDEF"
3180   REPEAT
3190     FOR I=0 TO 39
3200       HY(I)=HY(I)+HS(I)/60
3210       IF HY(I)>38 THEN
3220         HY(I)=-RND(1)*10:HS(I)=6+RND(1)*14
3230       END IF
3240       H=INT(HY(I))
3250       FOR K=0 TO 9
3260         Y=H-K
3270         IF Y>=0 AND Y<25 THEN
3280           C=1+INT(RND(1)*16)
3290           IF K=0 THEN
3300             GFX TEXT I*16,Y*12,MID$(G$,C,1),1,2,1
3310           ELSE
3320             GFX TEXT I*16,Y*12,MID$(G$,C,1),25-K,2,1
3330           END IF
3340         END IF
3350       NEXT K
3360     NEXT I
3370     ' the number takes shape, one digit at a time
3380     N=INT(FN LT(1)*2.4)
3390     LS$=""
3400     FOR I=1 TO 10
3410       IF I<=N OR I<=2 THEN
3420         LS$=LS$+MID$(T$,I,1)
3430       ELSE
3440         LS$=LS$+MID$(G$,1+INT(RND(1)*16),1)
3450       END IF
3460     NEXT I
3470     CALL HUE(250,TT*5,1)
3480     IF N>=10 THEN CALL CTEXT(100,LS$,1,6+INT(2*PU),9) ELSE CALL CTEXT(100,LS$,250,6,9)
3490     CALL CTEXT(10,"THE LAST VALUE",3,2,2)
3500     CALL CTEXT(262,"OF A SIGNED 32 BIT TIME_T",4,2,2)
3510     CALL FLIP
3520   UNTIL QT OR TI-T0>=PE(1)
3530 END PROC
3540 ' ====================================================================== part 2: copper bars
3550 ' eight bars of 15 shades each: palette 16 + 16 * bar + shade (both graphics layers); each bar is 15 boxes of two rows
3560 PROC P_RASTER
3570   LOCAL I,PH,Y,X,A$
3580   CALL BARPAL
3590   CALL BGBANDS(8,0,0,40,0,10)
3600   A$="2147483647"
3610   REPEAT
3620     CALL HUE(200,TT*2.5,1)
3630     FOR I=0 TO 7
3640       PH=TT*1.7+I*0.6
3650       IF COS(PH)<0 THEN CALL BAR(I,135+105*SIN(PH))
3660     NEXT I
3670     FOR I=0 TO 7
3680       PH=TT*1.7+I*0.6
3690       IF COS(PH)>=0 THEN CALL BAR(I,135+105*SIN(PH))
3700     NEXT I
3710     FOR I=1 TO 10
3720       X=(I-1)*54+50
3730       Y=110+44*SIN(TT*3.2+I*0.45)
3740       GFX TEXT X+4,Y+4,MID$(A$,I,1),2,5,6
3750       GFX TEXT X,Y,MID$(A$,I,1),200,5,6
3760     NEXT I
3770     CALL CTEXT(6,"SECONDS SINCE 1 JANUARY 1970",1,2,2)
3780     CALL CTEXT(270,"THAT IS ALL THE ROOM THERE IS",3,2,1)
3790     CALL FLIP
3800   UNTIL QT OR TI-T0>=PE(2)
3810 END PROC
3820 PROC BARPAL
3830   LOCAL B,K,F,H,R,G,Q
3840   FOR B=0 TO 7
3850     H=B*0.5+4.2
3860     FOR K=0 TO 14
3870       F=SIN(3.14159*(K+0.5)/15)
3880       Q=F^6
3890       R=MIN(255,INT((127+127*SIN(H))*F+210*Q))
3900       G=MIN(255,INT((127+127*SIN(H+2.094))*F+210*Q))
3910       Q=MIN(255,INT((127+127*SIN(H+4.189))*F+210*Q))
3920       CALL PAL(16+B*16+K,R,G,Q)
3930     NEXT K
3940   NEXT B
3950 END PROC
3960 PROC BAR(B,Y)
3970   LOCAL K
3980   FOR K=0 TO 14
3990     GFX LINE 0,Y+K*2-15,639,Y+K*2-14,16+B*16+K,BF
4000   NEXT K
4010 END PROC
4020 ' ====================================================================== part 3: the server room (the 3D sector renderer)
4030 ' ten corridor sectors of 300 units, racks of blinking lights along both walls; the camera flies down the corridor
4040 PROC P_SERVER
4050   LOCAL I,S,X,T,K,L,AN,CX,CY,A$,F,SH,K$
4060   CALL W3K_SETUP
4070   GFX TARGET 3
4080   CALL W3K_METAL(1,9)
4090   CALL W3K_TILES(2,0,9)
4100   CALL W3K_METAL(3,0)
4110   CALL W3K_METAL(6,1)
4120   GFX CLEAR 0
4130   CALL RACKTEX(4,0)
4140   CALL RACKTEX(5,1)
4150   W3D FOG 700
4160   FOR S=0 TO 9
4170     W3D SECTOR S,0,128,2,3,200
4180     W3D WALL S,S*300,-64,S*300+300,-64,1
4190     IF S<9 THEN W3D WALL S,S*300+300,-64,S*300+300,64,1,S+1 ELSE W3D WALL S,S*300+300,-64,S*300+300,64,6
4200     W3D WALL S,S*300+300,64,S*300,64,1
4210     IF S>0 THEN W3D WALL S,S*300,64,S*300,-64,1,S-1 ELSE W3D WALL S,S*300,64,S*300,-64,6
4220   NEXT S
4230   FOR I=0 TO 25
4240     W3D THING I,60+I*110,-50,4,40,96
4250     W3D THING 26+I,60+I*110,50,4,40,96
4260   NEXT I
4270   CALL W3K_VIEW(0,0,640,264,2)
4280   W3D COLORMAP
4290   F=0
4300   REPEAT
4310     L=FN LT(3)
4320     CX=30+L*150
4330     CY=12*SIN(L*1.3)
4340     AN=5*SIN(L*0.9)
4350     SH=0
4360     IF L>12.6 THEN
4370       SH=(L-12.6)*2.5
4380       CALL JITTER(SH)
4390     END IF
4400     W3D CAMERA CX,CY,AN+RND(1)*SH,-1.2*SH*RND(1)
4410     FOR S=0 TO 9
4420       K=140+80*SIN(L*3+S*1.7)+60*PU
4430       IF L>10 AND RND(1)<0.08 THEN K=20
4440       W3D LIGHT S,FN CLAMP(K,0,255)
4450     NEXT S
4460     F=F+1
4470     IF F MOD 6=0 THEN
4480       FOR I=0 TO 51
4490         W3D THINGTEX I,4+INT(RND(1)*2)
4500       NEXT I
4510     END IF
4520     W3D RENDER
4530     CALL W3K_HUDBAR(264,36)
4540     GFX TEXT 8,6,"UNIX TIME "+MID$(STR$(INT(2147400000+L*3100)),1),FN W3K_C(5,14),1,1
4550     GFX TEXT 8,270,"SERVER ROOM 7  RACK "+MID$(STR$(INT(L*2.2)+1),1)+" OF 26",FN W3K_C(0,13),1,1
4560     IF FA>0 THEN
4570       GFX PALETTE 0,250,RGB(255,255,255,FA)
4580       GFX PALETTE 3,250,RGB(255,255,255,FA)
4590       GFX LINE 0,0,639,299,250,BF
4600       FA=FA-22
4610     END IF
4620     CALL W3K_SHOW
4630     CALL CLOCK
4640     K$=INKEY$
4650     IF K$="q" OR K$="Q" OR K$=CHR$(27) THEN QT=1
4660   UNTIL QT OR TI-T0>=PE(3)
4670 END PROC
4680 ' the corridor shakes at the end: the ceiling jumps
4690 PROC JITTER(SH)
4700   LOCAL S
4710   FOR S=0 TO 9
4720     W3D HEIGHT S,0,FN CLAMP(128-SH*RND(1)*5,70,128)
4730   NEXT S
4740 END PROC
4750 ' a rack with blinking lights, 40 x 96: a frame; the lights are different in the two pictures
4760 PROC RACKTEX(N,F)
4770   LOCAL R,C,X,Y,K
4780   GFX CLEAR 0
4790   GFX LINE 0,0,39,95,FN W3K_C(0,3),BF
4800   GFX LINE 0,0,39,95,FN W3K_C(0,9),B
4810   GFX LINE 3,3,36,92,FN W3K_C(0,2),B
4820   FOR R=0 TO 9
4830     Y=6+R*9
4840     GFX LINE 5,Y,34,Y+6,FN W3K_C(0,6),BF
4850     GFX LINE 5,Y,34,Y,FN W3K_C(0,10)
4860     FOR C=0 TO 5
4870       X=8+C*5
4880       K=(R*7+C*3+F*(1+R MOD 3)) MOD 5
4890       IF K=0 THEN
4900         GFX PSET X,Y+3,FN W3K_C(1,15)
4910       ELSE
4920         IF K<3 THEN GFX PSET X,Y+3,FN W3K_C(5,14) ELSE GFX PSET X,Y+3,FN W3K_C(0,2)
4930       END IF
4940     NEXT C
4950     GFX LINE 27,Y+2,32,Y+4,FN W3K_C(0,2),BF
4960   NEXT R
4970   CALL W3K_GRAB(N,40,96)
4980 END PROC
4990 ' ====================================================================== part 4: the timeline (falling bits on sprite paths)
5000 PROC P_TIMELINE
5010   LOCAL I,X,Y,T,Y0,Y1,YR,PR,LT2,W$
5020   FOR I=0 TO 29
5030     SPRITE I,ON
5040     SPRITE I,IMAGE 100+(I MOD 2)
5050     SPRITE I,PRIORITY 6
5060     T=150+INT(RND(1)*150)
5070     SPRITE I,PATH "T"+MID$(STR$(T),1)+" X="+MID$(STR$(INT(RND(1)*620)),1)+" Y=-24 M 0,330 R"
5080   NEXT I
5090   CALL BGBANDS(0,0,18,14,0,30)
5100   REPEAT
5110     LT2=FN LT(4)
5120     PR=FN CLAMP(LT2/11.5,0,1)
5130     ' the line from 1970 to 2038
5140     GFX LINE 40,150,600,150,1,BF
5150     FOR I=0 TO 6
5160       X=40+I*80
5170       GFX LINE X,142,X,158,1,BF
5180       GFX TEXT X-16,162,MID$(STR$(1970+I*10),1),10,1,1
5190     NEXT I
5200     GFX LINE 40,148,40+PR*560,152,5,BF
5210     X=40+PR*560
5220     GFX LINE X,120,X,150,3,BF
5230     Y=INT(1970+PR*68)
5240     CALL CTEXT(94,MID$(STR$(Y),1),3,4,4)
5250     GFX TEXT 596,162,"2038",5,1,1
5260     CALL CTEXT(8,"THE CLOCK RUNS DOWN",4,3,3)
5270     CALL CTEXT(262,"68 YEARS OF SECONDS, ONE BIT TOO FEW",1,2,2)
5280     IF PR>0.97 THEN FA=255
5290     CALL FLIP
5300   UNTIL QT OR TI-T0>=PE(4)
5310   FOR I=0 TO 29
5320     SPRITE I,PATH ""
5330     SPRITE I,OFF
5340   NEXT I
5350 END PROC
5360 ' ====================================================================== part 5: the tape (a picture written to a BMP file, loaded back and torn into strips)
5370 PROC P_TAPE
5380   LOCAL I,X,Y,A$,AM,LT2,D,F
5390   GFX TARGET 0
5400   GFX CLEAR
5410   GFX LINE 0,0,639,299,2,BF
5420   GFX LINE 10,10,629,289,10,B
5430   GFX LINE 14,14,625,285,7,B
5440   CALL CTEXT(24,"SYSTEM CLOCK",4,4,4)
5450   CALL CTEXT(92,"THU 19 JAN 2038",1,5,5)
5460   CALL CTEXT(160,"03:14:07 UTC",3,5,5)
5470   GFX TEXT 40,240,"1010101111000010101000111101",6,2,2
5480   GFX TEXT 40,262,"STATUS: ALL FINE",6,2,2
5490   GFX SAVE "local:y2k38-tape.bmp"
5500   GFX CLEAR
5510   GFX LOAD "local:y2k38-tape.bmp",0,0
5520   FOR I=0 TO 24
5530     GFX GRAB 0,I*12,639,I*12+11,A$
5540     SG$(I)=A$
5550   NEXT I
5560   GFX CLEAR
5570   GFX TARGET BF
5580   REPEAT
5590     LT2=FN LT(5)
5600     AM=FN CLAMP(LT2-1.5,0,12)*5
5610     FOR I=0 TO 24
5620       D=AM*SIN(TT*9+I*0.55)*SIN(I*0.3+TT)+PU*3
5630       IF RND(1)<0.04 AND AM>0 THEN D=D+RND(1)*140-70
5640       GFX BLIT SG$(I),D,I*12
5650     NEXT I
5660     IF LT2>3 THEN
5670       D=INT(RND(1)*300)
5680       GFX LINE 0,D,639,D,1+INT(RND(1)*10)
5690     END IF
5700     CALL CTEXT(280,"THE TAPE IS WEARING OUT",5,1,1)
5710     CALL FLIP
5720   UNTIL QT OR TI-T0>=PE(5)
5730 END PROC
5740 ' ====================================================================== part 6: the countdown to 03:14:07
5750 PROC P_COUNT
5760   LOCAL N,LN,I,R,SX,X,T$,S,LT2
5770   LN=-1
5780   CALL BGBANDS(8,0,0,60,0,20)
5790   REPEAT
5800     LT2=FN LT(6)
5810     N=12-INT(LT2/1.0667)
5820     IF N<0 THEN N=0
5830     IF N<>LN THEN
5840       SFX 3
5850       LN=N
5860     END IF
5870     FOR I=0 TO 2
5880       R=((1-PU)+I)*140
5890       GFX CIRCLE 320,150,R,5
5900       GFX CIRCLE 320,150,R+1,5
5910     NEXT I
5920     S=7-N
5930     T$="03:14:"
5940     IF S<0 THEN T$="03:13:"
5950     IF S<0 THEN S=S+60
5960     T$=T$+MID$(STR$(100+S),2,2)
5970     SX=9
5980     X=320-LEN(T$)*4*SX
5990     CALL HUE(201,TT*4,1)
6000     GFX TEXT X+7,80+7,T$,2,SX,10
6010     IF N>3 THEN GFX TEXT X,80,T$,201,SX,10 ELSE GFX TEXT X,80,T$,5,SX,10
6020     CALL CTEXT(10,"19 JANUARY 2038",1,3,3)
6030     IF N=1 THEN CALL CTEXT(230,"1 SECOND LEFT",3,3,3) ELSE CALL CTEXT(230,MID$(STR$(N),1)+" SECONDS LEFT",3,3,3)
6040     CALL CTEXT(268,"0x7FFFFFFF  "+FN HEXN$(2147483647-N),4,2,2)
6050     CALL FLIP
6060   UNTIL QT OR TI-T0>=PE(6)
6070 END PROC
6080 ' ====================================================================== part 7: the overflow
6090 PROC P_OVERFLOW
6100   LOCAL I,X,Y,W,C,LT2,B$,V,K,HT
6110   HT=0
6120   REPEAT
6130     LT2=FN LT(7)
6140     V=2147483647
6150     IF LT2>=0.7 THEN V=2147483648
6160     IF LT2>=2.5 THEN V=2147483648+INT(RND(1)*2000000000)
6170     IF LT2>=0.7 AND HT=0 THEN
6180       SFX 5
6190       HT=1
6200     END IF
6210     IF LT2>=0.7 THEN
6220       K=FN CLAMP((LT2-0.7)*40,0,255)
6230       GFX BACKDROP RGB(INT(RND(1)*K*0.5),0,0)
6240       FOR I=0 TO 49
6250         X=RND(1)*640:Y=RND(1)*300
6260         GFX LINE X,Y,X+RND(1)*240,Y+RND(1)*10,1+INT(RND(1)*10),BF
6270       NEXT I
6280       FOR I=0 TO 24
6290         IF RND(1)<0.3 THEN GFX LINE 0,I*12,639,I*12+RND(1)*5,2,BF
6300       NEXT I
6310     END IF
6320     B$=FN BITS$(V)
6330     GFX TEXT 64+2,30+2,B$,2,2,3
6340     IF LT2<0.7 THEN GFX TEXT 64,30,B$,6,2,3 ELSE GFX TEXT 64,30,B$,5,2,3
6350     IF LT2<0.7 THEN
6360       CALL CTEXT(100,"2147483647",6,8,6)
6370       CALL CTEXT(190,"03:14:07",1,6,6)
6380     ELSE
6390       CALL CTEXT(100+INT(RND(1)*8),"-2147483648",5,6,6)
6400       CALL CTEXT(190+INT(RND(1)*8),"OVERFLOW",1,7,6)
6410       CALL CTEXT(262,"SIGN BIT SET  CLOCK RUNS BACKWARDS",3,2,2)
6420     END IF
6430     IF LT2>0.7 AND LT2<0.8 THEN FA=255
6440     CALL FLIP
6450   UNTIL QT OR TI-T0>=PE(7)
6460   GFX BACKDROP RGB(0,0,0)
6470 END PROC
6480 ' ====================================================================== part 8: the wrap
6490 PROC P_WRAP
6500   LOCAL I,X,Y,LT2,N,T$,D$
6510   CALL BGBANDS(0,0,10,0,0,40)
6520   FOR I=0 TO 49
6530     RK(I)=RND(1)*640
6540   NEXT I
6550   T$="FRI 13 DEC 1901  20:45:52"
6560   GFX BACKDROP RGB(0,0,0)
6570   REPEAT
6580     LT2=FN LT(8)
6590     FOR I=0 TO 49
6600       Y=(I*37+TT*18+I*I) MOD 300
6610       GFX PSET RK(I),Y,1+(I MOD 3)*3
6620     NEXT I
6630     N=FN CLAMP(INT(LT2*7),0,LEN(T$))
6640     CALL CTEXT(70,LEFT$(T$,N),4,3,5)
6650     IF LT2>3.8 THEN CALL CTEXT(160,"TIME GOES ON.  USE 64 BITS.",3,3,3)
6660     IF LT2>5 THEN CALL CTEXT(210,"Y2K38 - AN ENGINE1999 DEMO",1,2,2)
6670     DK=FN CLAMP((LT2-7.8)*140,0,255)
6680     CALL FLIP
6690   UNTIL QT OR TI-T0>=PE(8)
6700 END PROC
6710 ' ====================================================================== the music: 150 beats a minute, eight voices (1 lead DISTGUIT, 2 arp ACIDBASS, 3 bass BASS, 4 kick and snare, 5 hats HIHAT, 6 and 7 pad PAD, 8 bells METALLOPHONE)
6720 ' a bar is 16 sixteenths = 1.6 s; every voice's string gets exactly one whole note per bar. All strings are made first, then queued at once.
6730 PROC MUSIC(START)
6740   LOCAL P,V
6750   FOR P=START TO NP-1
6760     CALL SECTION(P)
6770   NEXT P
6780   FOR P=START TO NP-1
6790     PLAY MV$(P,0),MV$(P,1),MV$(P,2),MV$(P,3),MV$(P,4),MV$(P,5),MV$(P,6),MV$(P,7)
6800     IF P=START THEN T0=TI-PS(START)
6810   NEXT P
6820 END PROC
6830 PROC SECTION(P)
6840   LOCAL B,NB
6850   NB=(PE(P)-PS(P))/96
6860   LD$="T150 I22 V0.30 MN ":AR$="T150 I27 V0.12 ML ":BS$="T150 I4 V0.44 ML ":DR$="T150 ML ":HT$="T150 ML "
6870   PA$="T150 I6 V0.11 ML ":PB$=PA$:PC$="T150 I23 V0.12 MN "
6880   ' the arpeggio filter opens over the part
6890   AR$=AR$+"K0.5 K9,"+MID$(STR$(NB*1.6),1)+" "
6900   CALL MKMEL(P,NB)
6910   FOR B=0 TO NB-1
6920     CALL MBAR(P,B,NB)
6930   NEXT B
6940   MV$(P,0)=LD$:MV$(P,1)=AR$:MV$(P,2)=BS$:MV$(P,3)=DR$:MV$(P,4)=HT$:MV$(P,5)=PA$:MV$(P,6)=PB$:MV$(P,7)=PC$
6950 END PROC
6960 ' the lead: a random walk in the pentatonic scale (a made-up generator with a fixed seed, so every run sounds the same); bars 4.. repeat bars 0..
6970 PROC MKMEL(P,NB)
6980   LOCAL B,K,S,IX,PAT,PAT$,N$,NT
6990   S=1000+P*37
7000   IX=4
7010   FOR B=0 TO NB-1
7020     IF B>=4 AND B<NB-1 THEN
7030       MEL$(B)=MEL$(B-4)
7040     ELSE
7050       S=(S*75+74) MOD 65537
7060       PAT=INT(S/65537*4)
7070       PAT$=MID$("xxxxxxxxx.xx.xx.xx.xx.xxx..xx..x",PAT*8+1,8)
7080       N$=""
7090       FOR K=1 TO 8
7100         IF MID$(PAT$,K,1)="x" THEN
7110           S=(S*75+74) MOD 65537
7120           IX=IX+INT(S/65537*7)-3
7130           IF IX<1 THEN IX=3
7140           IF IX>8 THEN IX=6
7150           NT=57+PENT(IX MOD 5)+12*(IX\5)
7160           N$=N$+FN NN$(NT)+"8 "
7170         ELSE
7180           N$=N$+"R8 "
7190         END IF
7200       NEXT K
7210       MEL$(B)=N$
7220     END IF
7230   NEXT B
7240   MEL$(NB-1)=FN NN$(64)+"4 "+FN NN$(60)+"4 "+FN NN$(57)+"2 "
7250 END PROC
7260 ' one bar of the eight voices; C = chord, R = its root (0..11), the third is 3 (minor) or 4 semitones up
7270 PROC MBAR(P,B,NB)
7280   LOCAL C,R,TH,K,N,SH,BS,S,PT$,H$
7290   C=B MOD 8
7300   R=CR(C)
7310   TH=4-CM(C)
7320   IF B>=FB(P,0) THEN LD$=LD$+MEL$(B) ELSE LD$=LD$+"R1 "
7330   ' arp: root, third, fifth, octave, fifth, third, fifth, third ... in sixteenths
7340   IF B>=FB(P,3) THEN
7350     SH=0
7360     IF (B MOD 4)>=2 THEN SH=12
7370     FOR K=0 TO 15
7380       N=VAL(MID$("0123212",(K MOD 8)+1,1))
7390       IF K MOD 8=7 THEN N=1
7400       IF N=0 THEN N=48+R
7410       IF N=1 THEN N=48+R+TH
7420       IF N=2 THEN N=48+R+7
7430       IF N=3 THEN N=48+R+12
7440       AR$=AR$+FN NN$(N+SH)+"16 "
7450     NEXT K
7460   ELSE
7470     AR$=AR$+"R1 "
7480   END IF
7490   IF B>=FB(P,2) THEN
7500     BS=36+R
7510     IF P=8 THEN
7520       BS$=BS$+FN NN$(BS)+"1 "
7530     ELSE
7540       FOR K=0 TO 7
7550         N=BS
7560         IF K=2 OR K=6 THEN N=BS+12
7570         BS$=BS$+FN NN$(N)+"8 "
7580       NEXT K
7590     END IF
7600   ELSE
7610     BS$=BS$+"R1 "
7620   END IF
7630   IF B>=FB(P,4) THEN
7640     PA$=PA$+FN NN$(36+R+12)+"1 ":PB$=PB$+FN NN$(36+R+TH+12)+"1 "
7650     ' bells: a rocking arpeggio in the later parts, a falling note in the last one
7660     IF P=3 OR P=5 OR P=6 OR P=8 THEN
7670       FOR K=0 TO 7
7680         N=VAL(MID$("0201320",K MOD 7+1,1))
7690         IF N=0 THEN N=72+R
7700         IF N=2 THEN N=72+R+7
7710         IF N=1 THEN N=72+R+TH
7720         IF N=3 THEN N=72+R+12
7730         IF P=8 AND K=0 THEN N=N-12
7740         IF K MOD 2=0 THEN PC$=PC$+FN NN$(N)+"8 " ELSE PC$=PC$+"R8 "
7750       NEXT K
7760     ELSE
7770       PC$=PC$+"R1 "
7780     END IF
7790   ELSE
7800     PA$=PA$+"R1 ":PB$=PB$+"R1 ":PC$=PC$+"R1 "
7810   END IF
7820   IF B>=FB(P,1) THEN
7830     PT$=DPAT$(P)
7840     IF P=6 AND B>=4 THEN PT$="SSSSSSSSSSSSSSSS"
7850     IF P=7 THEN
7860       PT$=""
7870       FOR K=1 TO 16
7880         S=(B*131+K*17+P) MOD 7
7890         IF S<2 THEN
7900           PT$=PT$+"K"
7910         ELSE
7920           IF S<5 THEN PT$=PT$+"S" ELSE PT$=PT$+"."
7930         END IF
7940       NEXT K
7950     END IF
7960     DR$=DR$+FN DRUMS$(PT$)
7970   ELSE
7980     DR$=DR$+"R1 "
7990   END IF
8000   IF B>=FB(P,5) AND P<>8 THEN
8010     IF P=7 THEN H$="xxxxxxxxxxxxxxxx" ELSE H$="x.x.x.x.x.x.x.x."
8020     HT$=HT$+FN HATS$(H$)
8030   ELSE
8040     HT$=HT$+"R1 "
8050   END IF
8060 END PROC
