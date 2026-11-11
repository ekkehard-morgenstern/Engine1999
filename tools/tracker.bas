1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 '
20 ' A tool of Engine1999: start it with TOOL "tracker" (or TOOL "tracker","file" to open a song) from the prompt or from a program;
30 ' the program you are working on stays as it is. It can also be run as a program of its own (LOAD "tools/tracker", RUN; ARG$(1) is read the same way).
40 ' TRACKER - a pattern based music editor in the style of the Amiga trackers. A song is a list of PATTERNS (up to 32 of 16..64 rows) played in an ORDER
50 ' (up to 64 slots), on 8 channels. Every cell of a pattern holds a note (C-4, C#4 ...; === ends the note), an instrument (04..63, .. = the one before) and a
60 ' volume (0..F, . = the one before). A row is a sixteenth note; the tempo is in beats (quarter notes) a minute. Play turns the song into the music strings of
70 ' PLAY (one per channel: the Export button writes them as BASIC lines, so the tracker is also a way to find out what a PLAY string for some music looks like)
80 ' and plays them on the channels 1 to 8. It is written in BASIC on the UI subsystem (the character grid UI GRID, new for this tool) and uses the libraries
90 ' uikit and filereq. Needs the window (engine1999).
100 '   The grid is the pattern. Click a cell or use the cursor keys (Left / Right go through the fields note, instrument, volume and on to the next channel,
110 '   Shift + Left / Right jump a channel, Up / Down a row, PageUp / PageDown 16 rows, Home / End the first / last row). Tab leaves the grid.
120 '   In the note field the keys of a piano play the note: Z S X D C V G B H N J M are C to B of the octave shown, Q 2 W 3 E R 5 T 6 Y 7 U I the octave above
130 '   (and the next C); 1 puts a note off; Delete / Backspace clears the field. The note is written with the current instrument (the buttons < >, or the
140 '   keys [ and ]) and the cursor goes down by the step. In the instrument field type two digits (04..63), in the volume field a hex digit.
150 '   - and + change the octave. Enter plays the row, Space plays the song from the cursor and stops it again.
160 '   Channel: the check Mute silences it, the slider Pan puts it in the stereo picture, Ch = Ins makes the current instrument the one it starts with.
170 '   Order: < > go through the slots, Pat< Pat> choose the pattern of a slot, New starts an empty one, Clone copies the pattern, Ins / Del add or remove a slot,
180 '   Len cycles the length 16 / 32 / 48 / 64.   Save / Open keep a song in a text file (.trk, with the instruments of 32..63 that it uses), Export writes BASIC
190 '   lines, Copy puts the PLAY lines on the clipboard, Clear starts a new song.
200 ' widget ids: 1 form, 10 Song, 11 Pattern, 12 Here, 13 Stop, 20 tempo slider (+1 label, +2 readout), 30 oct -, 31 oct label, 32 oct +, 33 inst <, 34 inst label, 35 inst >,
210 ' 36 Ch = Ins, 37 step button, 94 Mute, 91 Pan slider (+1, +2), 40 order <, 41 order label, 42 order >, 43 Pat<, 44 Pat>, 45 New, 46 Clone, 47 Ins, 48 Del, 49 Len,
220 ' 50 the grid, 60 status, 70 Save, 71 Open, 72 Export, 73 Copy, 74 Clear, 75 Quit
230 LOAD LIBRARY "uikit"
240 LOAD LIBRARY "filereq"
250 REM the graphics of the calling program stay (no GFX RESET)
260 CLS
270 BREAK OFF
280 Q=0
290 HEADLESS=0
300 COLS=74
310 GRH=12
320 CENTER=4
330 DIM PN(16384),PJ(16384),PV(16384),PL(32),OD(64),CI(8),CP(8),CM(8),S$(8)
340 DIM FLN(32768),FLI(32768),FLV(32768),FLO(4096),FLR(4096)
350 DIM NT$(12),NP$(12),LV(8),LS$(8),FO(4),FL(4)
360 FOR I=0 TO 11
370   READ NT$(I),NP$(I)
380 NEXT I
390 DATA "C-","C","C#","C#","D-","D","D#","D#","E-","E","F-","F","F#","F#","G-","G","G#","G#","A-","A","A#","A#","B-","B"
400 FOR I=0 TO 7
410   READ LV(I),LS$(I)
420 NEXT I
430 DATA 16,"1",12,"2.",8,"2",6,"4.",4,"4",3,"8.",2,"8",1,"16"
440 FO(0)=0:FO(1)=4:FO(2)=5:FO(3)=7
450 FL(0)=3:FL(1)=1:FL(2)=1:FL(3)=1
460 DR$=""
470 ' the song: ONUM slots of the order, NPAT patterns, BPM; the cursor: CO slot, CR row, CC channel, CF field (0 note, 1 / 2 the instrument digits, 3 volume)
480 BPM=125
490 ONUM=1
500 NPAT=1
510 CO=0
520 CR=0
530 CC=0
540 CF=0
550 OCV=3
560 INS=4
570 STP=1
580 PLG=0
590 PCO=0
600 PHROW=0
610 PT0=0
620 FT=0
630 AC$=""
640 CURI=0
650 CURV=12
660 CURO=-1
670 PN2=0
680 PS=0
690 PI2=0
700 PV2=-1
710 ' ====================================================================== functions (they must stand before the lines that use them)
720 FUNC NUM$(V)
730   RETURN STR$(INT(V*1000+0.5)/1000)
740 END FUNC
750 FUNC IX(P,R,C)
760   RETURN (P*64+R)*8+C
770 END FUNC
780 FUNC CLAMP(V,LO,HI)
790   IF V<LO THEN RETURN LO
800   IF V>HI THEN RETURN HI
810   RETURN V
820 END FUNC
830 FUNC NOTE$(N)
840   IF N=0 THEN RETURN "---"
850   IF N=97 THEN RETURN "==="
860   RETURN NT$((N-1) MOD 12)+STR$((N-1)\12)
870 END FUNC
880 FUNC CELL$(P,R,C)
890   LOCAL X,N,I,V,A$
900   X=FN IX(P,R,C)
910   N=PN(X):I=PJ(X):V=PV(X)
920   A$=FN NOTE$(N)+" "
930   IF I>0 THEN A$=A$+RIGHT$("0"+STR$(I),2) ELSE A$=A$+".."
940   IF V>=0 THEN A$=A$+" "+MID$("0123456789ABCDEF",V+1,1) ELSE A$=A$+" ."
950   RETURN A$
960 END FUNC
970 ' the name of instrument N (from its definition string: NAME xxx; ...)
980 FUNC INAME$(N)
990   LOCAL D$,E
1000   D$=INSTRUMENT$(N)
1010   IF LEFT$(D$,5)<>"NAME " THEN RETURN "?"
1020   E=INSTR(D$,";")
1030   IF E=0 THEN E=LEN(D$)+1
1040   RETURN MID$(D$,6,E-6)
1050 END FUNC
1060 ' the I-th (0..) word of S$ (blanks), "" if none
1070 FUNC TOKN$(S$,N)
1080   LOCAL I,K,C$,T$,INW
1090   K=0:T$="":INW=0
1100   FOR I=1 TO LEN(S$)+1
1110     IF I<=LEN(S$) THEN C$=MID$(S$,I,1) ELSE C$=" "
1120     IF C$=" " OR C$=CHR$(9) THEN
1130       IF INW THEN
1140         IF K=N THEN RETURN T$
1150         K=K+1:T$="":INW=0
1160       END IF
1170     ELSE
1180       T$=T$+C$:INW=-1
1190     END IF
1200   NEXT I
1210   RETURN ""
1220 END FUNC
1230 ' is instrument I (32..63) used by a channel or a cell of the song?
1240 FUNC USES(I)
1250   LOCAL P,R,C,X
1260   FOR C=0 TO 7
1270     IF CI(C)=I THEN RETURN -1
1280   NEXT C
1290   FOR P=0 TO NPAT-1
1300     FOR R=0 TO 63
1310       FOR C=0 TO 7
1320         IF PJ(FN IX(P,R,C))=I THEN RETURN -1
1330       NEXT C
1340     NEXT R
1350   NEXT P
1360   RETURN 0
1370 END FUNC
1380 ' ====================================================================== the song
1390 PROC CLEARPAT(P)
1400   LOCAL R,C,X
1410   FOR R=0 TO 63
1420     FOR C=0 TO 7
1430       X=FN IX(P,R,C)
1440       PN(X)=0:PJ(X)=0:PV(X)=-1
1450     NEXT C
1460   NEXT R
1470 END PROC
1480 PROC NEWSONG
1490   LOCAL P,C
1500   FOR P=0 TO 31
1510     CALL CLEARPAT(P)
1520     PL(P)=32
1530   NEXT P
1540   FOR P=0 TO 63
1550     OD(P)=0
1560   NEXT P
1570   ONUM=1:NPAT=1:CO=0:CR=0:CC=0:CF=0
1580   BPM=125
1590   CI(0)=4:CI(1)=5:CI(2)=6:CI(3)=7:CI(4)=13:CI(5)=14:CI(6)=15:CI(7)=9
1600   FOR C=0 TO 7
1610     CP(C)=0:CM(C)=0
1620   NEXT C
1630   CP(0)=-30:CP(1)=30:CP(2)=-60:CP(3)=60
1640 END PROC
1650 ' ====================================================================== the music strings
1660 ' the rows of the song from slot SO, row SR on, as flat arrays: FLN notes, FLI instruments, FLV volumes (row * 8 + channel), FLO / FLR where the row came from
1670 PROC FLATTEN(SO,SR)
1680   LOCAL O,R,P,C,X,T,R0
1690   T=0
1700   FOR O=SO TO ONUM-1
1710     P=OD(O)
1720     R0=0
1730     IF O=SO THEN R0=SR
1740     FOR R=R0 TO PL(P)-1
1750       FLO(T)=O:FLR(T)=R
1760       FOR C=0 TO 7
1770         X=FN IX(P,R,C)
1780         FLN(T*8+C)=PN(X):FLI(T*8+C)=PJ(X):FLV(T*8+C)=PV(X)
1790       NEXT C
1800       T=T+1
1810     NEXT R
1820   NEXT O
1830   FT=T
1840 END PROC
1850 ' K rows of rests
1860 PROC EMITREST(K)
1870   LOCAL Z,I
1880   Z=K
1890   WHILE Z>0
1900     I=0
1910     WHILE LV(I)>Z
1920       I=I+1
1930     WEND
1940     Z=Z-LV(I)
1950     AC$=AC$+" R"+LS$(I)
1960   WEND
1970 END PROC
1980 ' the open note PN2 (instrument PI2, volume PV2) for K rows: pieces joined by legato, the last one with the normal gap
1990 PROC EMITNOTE(K)
2000   LOCAL Z,I,O,S
2010   IF PI2>0 AND PI2<>CURI THEN
2020     AC$=AC$+" I"+STR$(PI2)
2030     CURI=PI2
2040   END IF
2050   IF PV2>=0 AND PV2<>CURV THEN
2060     AC$=AC$+" V"+FN NUM$(PV2/15)
2070     CURV=PV2
2080   END IF
2090   O=(PN2-1)\12
2100   S=(PN2-1) MOD 12
2110   IF O<>CURO THEN
2120     AC$=AC$+" O"+STR$(O)
2130     CURO=O
2140   END IF
2150   Z=K
2160   WHILE Z>0
2170     I=0
2180     WHILE LV(I)>Z
2190       I=I+1
2200     WEND
2210     Z=Z-LV(I)
2220     IF Z>0 THEN AC$=AC$+" ML" ELSE AC$=AC$+" MN"
2230     AC$=AC$+" "+NP$(S)+LS$(I)
2240   WEND
2250 END PROC
2260 ' the PLAY string of channel C for the rows FLATTEN made
2270 FUNC CHSTR$(C)
2280   LOCAL R,N,SS
2290   AC$="T"+STR$(BPM)+" MN Y"+FN NUM$(CP(C)/100)+" I"+STR$(CI(C))+" V"+FN NUM$(12/15)
2300   CURI=CI(C):CURV=12:CURO=-1
2310   PN2=0:PS=0:PI2=0:PV2=-1:SS=0
2320   FOR R=0 TO FT
2330     IF R<FT THEN N=FLN(R*8+C) ELSE N=-1
2340     IF N<>0 THEN
2350       IF PN2>0 THEN
2360         CALL EMITNOTE(R-PS)
2370       ELSE
2380         IF R>SS THEN CALL EMITREST(R-SS)
2390       END IF
2400       IF N>=5 AND N<=96 THEN
2410         PN2=N:PS=R:PI2=FLI(R*8+C):PV2=FLV(R*8+C)
2420       ELSE
2430         PN2=0:SS=R
2440       END IF
2450     END IF
2460   NEXT R
2470   RETURN AC$
2480 END FUNC
2490 ' ====================================================================== the screen
2500 PROC SETSTATUS(T$)
2510   IF HEADLESS THEN EXIT PROC
2520   UI CAPTION 60,T$
2530 END PROC
2540 ' the order label, the instrument label ... after anything changed
2550 PROC SYNCCTL
2560   IF HEADLESS THEN EXIT PROC
2570   UI CAPTION 31,"Oct "+STR$(OCV)
2580   UI CAPTION 34,RIGHT$("0"+STR$(INS),2)+" "+FN INAME$(INS)
2590   UI CAPTION 41,"Slot "+STR$(CO+1)+"/"+STR$(ONUM)+"  Pattern "+STR$(OD(CO))+"  Len "+STR$(PL(OD(CO)))
2600   UI CAPTION 37,"Step "+STR$(STP)
2610   CALL UK_SET(91,CP(CC))
2620   UI VALUE 94,CM(CC)
2630   UI CAPTION 95,"Channel "+STR$(CC+1)+" ("+FN INAME$(CI(CC))+")"
2640 END PROC
2650 PROC REDRAW
2660   LOCAL G,R,P,L$,C,FG,BG,VR,CO2,H$,M$,N$,X
2670   IF HEADLESS THEN EXIT PROC
2680   CO2=CO:VR=CR
2690   IF PLG THEN
2700     CO2=PCO:VR=PHROW
2710   END IF
2720   P=OD(CO2)
2730   H$="Row"
2740   FOR C=0 TO 7
2750     N$=FN INAME$(CI(C))+"       "
2760     M$=" "
2770     IF CM(C)<>0 THEN M$="m"
2780     H$=H$+" "+STR$(C+1)+M$+LEFT$(N$,5)+" "
2790   NEXT C
2800   UI GRIDPUT 50,0,0,H$,0,225
2810   FOR G=1 TO GRH-1
2820     R=VR+G-CENTER
2830     IF R<0 OR R>=PL(P) THEN
2840       UI GRIDPUT 50,0,G,SPACE$(COLS),0,0
2850     ELSE
2860       BG=0
2870       IF R MOD 4=0 THEN BG=225
2880       IF PLG AND R=PHROW THEN BG=236
2890       FG=0
2900       IF PLG AND R=PHROW THEN FG=230
2910       UI GRIDPUT 50,0,G,RIGHT$("0"+STR$(R),2)+" ",FG,BG
2920       FOR C=0 TO 7
2930         L$=FN CELL$(P,R,C)
2940         IF C<7 THEN L$=L$+"|"
2950         FG=0
2960         IF CM(C)<>0 THEN FG=232
2970         X=FN IX(P,R,C)
2980         IF PN(X)=0 AND PJ(X)=0 AND PV(X)<0 THEN FG=232
2990         IF PLG AND R=PHROW THEN FG=230
3000         UI GRIDPUT 50,3+C*9,G,L$,FG,BG
3010       NEXT C
3020     END IF
3030   NEXT G
3040   IF NOT PLG THEN
3050     L$=FN CELL$(P,CR,CC)
3060     UI GRIDPUT 50,3+CC*9+FO(CF),CENTER,MID$(L$,FO(CF)+1,FL(CF)),230,229
3070   END IF
3080 END PROC
3090 ' ====================================================================== playing
3100 PROC STOPPLAY
3110   SOUND OFF
3120   IF PLG THEN
3130     PLG=0
3140     CALL REDRAW
3150     CALL SETSTATUS("Stopped.")
3160   END IF
3170 END PROC
3180 PROC STARTPLAY(SO,SR)
3190   LOCAL C
3200   CALL FLATTEN(SO,SR)
3210   IF FT=0 THEN EXIT PROC
3220   FOR C=0 TO 7
3230     S$(C)=""
3240     IF CM(C)=0 THEN S$(C)=FN CHSTR$(C)
3250   NEXT C
3260   SOUND OFF
3270   PLAY S$(0),S$(1),S$(2),S$(3),S$(4),S$(5),S$(6),S$(7)
3280   PLG=-1
3290   PT0=TI
3300   PCO=SO:PHROW=SR
3310   CALL REDRAW
3320   CALL SETSTATUS("Playing "+STR$(FT)+" rows...")
3330 END PROC
3340 ' called every frame: the playhead
3350 PROC PLAYTICK
3360   LOCAL R
3370   IF NOT PLG THEN EXIT PROC
3380   R=INT((TI-PT0)*BPM*4/3600)
3390   IF R>=FT THEN
3400     CALL STOPPLAY
3410     EXIT PROC
3420   END IF
3430   IF FLO(R)<>PCO OR FLR(R)<>PHROW THEN
3440     PCO=FLO(R):PHROW=FLR(R)
3450     CALL REDRAW
3460   END IF
3470 END PROC
3480 ' the frequency of tracker note N (C-4 = 49 = middle C)
3490 FUNC NOTEF(N)
3500   RETURN 261.626*(2^((N-49)/12))
3510 END FUNC
3520 PROC PREVIEW(N,I)
3530   LOCAL W
3540   IF PLG THEN EXIT PROC
3550   IF N<5 OR N>96 THEN EXIT PROC
3560   W=I
3570   IF W=0 THEN W=INS
3580   WAVE CC+1,W
3590   SOUND CC+1,FN NOTEF(N),0.5,0.6
3600 END PROC
3610 ' ====================================================================== editing
3620 PROC GOROW(R)
3630   CR=FN CLAMP(R,0,PL(OD(CO))-1)
3640 END PROC
3650 PROC SETOCT(V)
3660   OCV=FN CLAMP(V,1,6)
3670 END PROC
3680 PROC SETINS(V)
3690   INS=FN CLAMP(V,4,63)
3700 END PROC
3710 ' the note of a piano key (0 if the key is none): Z S X D C V G B H N J M in the octave, Q 2 W 3 E R 5 T 6 Y 7 U I above it
3720 FUNC PIANO(K)
3730   LOCAL C$,P
3740   IF K<32 OR K>126 THEN RETURN 0
3750   C$=CHR$(K)
3760   IF C$>="A" AND C$<="Z" THEN C$=CHR$(K+32)
3770   P=INSTR("zsxdcvgbhnjm",C$)
3780   IF P>0 THEN RETURN (OCV*12)+P
3790   P=INSTR("q2w3er5t6y7ui",C$)
3800   IF P>0 THEN RETURN ((OCV+1)*12)+P
3810   RETURN 0
3820 END FUNC
3830 ' puts a note into the cursor cell (the instrument is the current one), plays it, goes down by the step
3840 PROC PUTNOTE(N)
3850   LOCAL X
3860   X=FN IX(OD(CO),CR,CC)
3870   PN(X)=N
3880   IF N>=5 AND N<=96 THEN
3890     PJ(X)=INS
3900     CALL PREVIEW(N,INS)
3910   END IF
3920   IF N=97 THEN
3930     PJ(X)=0:PV(X)=-1
3940   END IF
3950   CALL GOROW(CR+STP)
3960 END PROC
3970 PROC TYPEDIGIT(D)
3980   LOCAL X,I
3990   X=FN IX(OD(CO),CR,CC)
4000   IF CF=1 THEN
4010     I=D*10+(PJ(X) MOD 10)
4020     IF I>63 THEN I=63
4030     PJ(X)=I
4040     CF=2
4050   ELSE
4060     IF CF=2 THEN
4070       I=(PJ(X)\10)*10+D
4080       IF I>63 THEN I=63
4090       PJ(X)=I
4100       CF=3
4110     END IF
4120   END IF
4130 END PROC
4140 PROC TYPEVOL(V)
4150   LOCAL X
4160   X=FN IX(OD(CO),CR,CC)
4170   PV(X)=V
4180   CF=0
4190   CALL GOROW(CR+STP)
4200 END PROC
4210 PROC CLEARFIELD
4220   LOCAL X
4230   X=FN IX(OD(CO),CR,CC)
4240   IF CF=0 THEN
4250     PN(X)=0
4260   ELSE
4270     IF CF=3 THEN
4280       PV(X)=-1
4290     ELSE
4300       PJ(X)=0
4310     END IF
4320   END IF
4330 END PROC
4340 ' one field to the right / left, over the channel borders
4350 PROC MOVEF(D)
4360   CF=CF+D
4370   IF CF>3 THEN
4380     IF CC<7 THEN
4390       CC=CC+1:CF=0
4400     ELSE
4410       CF=3
4420     END IF
4430   END IF
4440   IF CF<0 THEN
4450     IF CC>0 THEN
4460       CC=CC-1:CF=3
4470     ELSE
4480       CF=0
4490     END IF
4500   END IF
4510 END PROC
4520 PROC KEYIN(K)
4530   CALL KEYACT(K)
4540   CALL SYNCCTL
4550   CALL REDRAW
4560 END PROC
4570 PROC KEYACT(K)
4580   LOCAL M,N,V
4590   M=K AND 65536
4600   K=K AND 65535
4610   IF K=4097 THEN
4620     IF M<>0 THEN
4630       CC=FN CLAMP(CC-1,0,7):CF=0
4640     ELSE
4650       CALL MOVEF(-1)
4660     END IF
4670     EXIT PROC
4680   END IF
4690   IF K=4098 THEN
4700     IF M<>0 THEN
4710       CC=FN CLAMP(CC+1,0,7):CF=0
4720     ELSE
4730       CALL MOVEF(1)
4740     END IF
4750     EXIT PROC
4760   END IF
4770   IF K=4099 THEN
4780     CALL GOROW(CR-1)
4790     EXIT PROC
4800   END IF
4810   IF K=4100 THEN
4820     CALL GOROW(CR+1)
4830     EXIT PROC
4840   END IF
4850   IF K=4103 THEN
4860     CALL GOROW(CR-16)
4870     EXIT PROC
4880   END IF
4890   IF K=4104 THEN
4900     CALL GOROW(CR+16)
4910     EXIT PROC
4920   END IF
4930   IF K=4101 THEN
4940     CALL GOROW(0)
4950     EXIT PROC
4960   END IF
4970   IF K=4102 THEN
4980     CALL GOROW(99)
4990     EXIT PROC
5000   END IF
5010   IF K=4105 OR K=8 THEN
5020     CALL CLEARFIELD
5030     EXIT PROC
5040   END IF
5050   IF K=13 THEN
5060     CALL PLAYROW
5070     EXIT PROC
5080   END IF
5090   IF K=32 THEN
5100     IF PLG THEN
5110       CALL STOPPLAY
5120     ELSE
5130       CALL STARTPLAY(CO,CR)
5140     END IF
5150     EXIT PROC
5160   END IF
5170   IF K=45 THEN
5180     CALL SETOCT(OCV-1)
5190     EXIT PROC
5200   END IF
5210   IF K=43 THEN
5220     CALL SETOCT(OCV+1)
5230     EXIT PROC
5240   END IF
5250   IF K=91 THEN
5260     CALL SETINS(INS-1)
5270     EXIT PROC
5280   END IF
5290   IF K=93 THEN
5300     CALL SETINS(INS+1)
5310     EXIT PROC
5320   END IF
5330   IF CF=0 THEN
5340     IF K=49 THEN
5350       CALL PUTNOTE(97)
5360     ELSE
5370       N=FN PIANO(K)
5380       IF N>0 THEN CALL PUTNOTE(N)
5390     END IF
5400     EXIT PROC
5410   END IF
5420   IF CF=3 THEN
5430     IF K<128 THEN
5440       V=INSTR("0123456789abcdef",CHR$(K+32*(K>=65 AND K<=70)*-1))
5450       IF V>0 THEN CALL TYPEVOL(V-1)
5460     END IF
5470     EXIT PROC
5480   END IF
5490   IF K>=48 AND K<=57 THEN CALL TYPEDIGIT(K-48)
5500 END PROC
5510 ' the row under the cursor: all its notes at once
5520 PROC PLAYROW
5530   LOCAL C,X,N
5540   IF PLG THEN EXIT PROC
5550   FOR C=0 TO 7
5560     X=FN IX(OD(CO),CR,C)
5570     N=PN(X)
5580     IF N>=5 AND N<=96 AND CM(C)=0 THEN
5590       IF PJ(X)>0 THEN WAVE C+1,PJ(X) ELSE WAVE C+1,CI(C)
5600       SOUND C+1,FN NOTEF(N),0.5,0.6
5610     END IF
5620   NEXT C
5630 END PROC
5640 ' a click on a cell of the grid: V = row * COLS + column
5650 PROC CLICKIN(V)
5660   LOCAL G,C,R,W
5670   G=V\COLS
5680   C=V MOD COLS
5690   IF G<1 THEN EXIT PROC
5700   IF PLG THEN EXIT PROC
5710   CALL GOROW(CR+G-CENTER)
5720   IF C>=3 THEN
5730     W=(C-3)\9
5740     IF W>7 THEN W=7
5750     CC=W
5760     R=(C-3)-W*9
5770     CF=0
5780     IF R=4 THEN CF=1
5790     IF R=5 THEN CF=2
5800     IF R>=7 THEN CF=3
5810   END IF
5820   CALL SYNCCTL
5830   CALL REDRAW
5840 END PROC
5850 ' ====================================================================== the order
5860 PROC SETCO(V)
5870   CO=FN CLAMP(V,0,ONUM-1)
5880   CR=FN CLAMP(CR,0,PL(OD(CO))-1)
5890 END PROC
5900 PROC NEWPATTERN
5910   IF NPAT>=32 THEN
5920     CALL SETSTATUS("There are 32 patterns already.")
5930     EXIT PROC
5940   END IF
5950   CALL CLEARPAT(NPAT)
5960   PL(NPAT)=32
5970   OD(CO)=NPAT
5980   NPAT=NPAT+1
5990   CR=0
6000 END PROC
6010 PROC CLONEPATTERN
6020   LOCAL R,C,X,Y,P
6030   IF NPAT>=32 THEN
6040     CALL SETSTATUS("There are 32 patterns already.")
6050     EXIT PROC
6060   END IF
6070   P=OD(CO)
6080   FOR R=0 TO 63
6090     FOR C=0 TO 7
6100       X=FN IX(P,R,C):Y=FN IX(NPAT,R,C)
6110       PN(Y)=PN(X):PJ(Y)=PJ(X):PV(Y)=PV(X)
6120     NEXT C
6130   NEXT R
6140   PL(NPAT)=PL(P)
6150   OD(CO)=NPAT
6160   NPAT=NPAT+1
6170 END PROC
6180 PROC INSSLOT
6190   LOCAL I
6200   IF ONUM>=64 THEN EXIT PROC
6210   FOR I=ONUM TO CO+2 STEP -1
6220     OD(I)=OD(I-1)
6230   NEXT I
6240   OD(CO+1)=OD(CO)
6250   ONUM=ONUM+1
6260   CO=CO+1
6270 END PROC
6280 PROC DELSLOT
6290   LOCAL I
6300   IF ONUM<=1 THEN EXIT PROC
6310   FOR I=CO TO ONUM-2
6320     OD(I)=OD(I+1)
6330   NEXT I
6340   ONUM=ONUM-1
6350   CALL SETCO(CO)
6360 END PROC
6370 PROC CYCLELEN
6380   LOCAL P,L
6390   P=OD(CO)
6400   L=PL(P)+16
6410   IF L>64 THEN L=16
6420   PL(P)=L
6430   CALL GOROW(CR)
6440 END PROC
6450 ' ====================================================================== files
6460 ' the song as text: TRACKER 1, BPM, CHANNEL lines, ORDER, the instruments 32..63 that the song uses, PATTERN lines and one C line (row channel note instrument volume) per used cell
6470 PROC SAVEF(F$)
6480   LOCAL P,R,C,X,I,L$
6490   OPEN F$ FOR OUTPUT AS #1
6500   PRINT #1,"TRACKER 1"
6510   PRINT #1,"BPM "+STR$(BPM)
6520   FOR C=0 TO 7
6530     PRINT #1,"CHANNEL "+STR$(C)+" "+STR$(CI(C))+" "+STR$(CP(C))+" "+STR$(CM(C)<>0)
6540   NEXT C
6550   L$="ORDER"
6560   FOR I=0 TO ONUM-1
6570     L$=L$+" "+STR$(OD(I))
6580   NEXT I
6590   PRINT #1,L$
6600   FOR I=32 TO 63
6610     IF FN USES(I) THEN PRINT #1,"INSTRUMENT "+STR$(I)+" "+INSTRUMENT$(I)
6620   NEXT I
6630   FOR P=0 TO NPAT-1
6640     PRINT #1,"PATTERN "+STR$(P)+" "+STR$(PL(P))
6650     FOR R=0 TO 63
6660       FOR C=0 TO 7
6670         X=FN IX(P,R,C)
6680         IF PN(X)<>0 OR PJ(X)<>0 OR PV(X)>=0 THEN PRINT #1,"C "+STR$(R)+" "+STR$(C)+" "+STR$(PN(X))+" "+STR$(PJ(X))+" "+STR$(PV(X))
6690       NEXT C
6700     NEXT R
6710   NEXT P
6720   PRINT #1,"END"
6730   CLOSE #1
6740   CALL SETSTATUS("Saved "+F$+".")
6750 END PROC
6760 PROC OPENF(F$)
6770   LOCAL L$,W$,P,N,C,X,A,B,D$,K
6780   OPEN F$ FOR INPUT AS #1
6790   CALL NEWSONG
6800   ONUM=0:NPAT=0
6810   P=0
6820   WHILE EOF(1)=0
6830     LINE INPUT #1,L$
6840     W$=FN TOKN$(L$,0)
6850     IF W$="BPM" THEN BPM=FN CLAMP(INT(VAL(FN TOKN$(L$,1))),30,300)
6860     IF W$="CHANNEL" THEN
6870       C=FN CLAMP(INT(VAL(FN TOKN$(L$,1))),0,7)
6880       CI(C)=FN CLAMP(INT(VAL(FN TOKN$(L$,2))),4,63)
6890       CP(C)=FN CLAMP(INT(VAL(FN TOKN$(L$,3))),-100,100)
6900       CM(C)=0
6910       IF VAL(FN TOKN$(L$,4))<>0 THEN CM(C)=-1
6920     END IF
6930     IF W$="ORDER" THEN
6940       ONUM=0
6950       FOR K=1 TO 64
6960         D$=FN TOKN$(L$,K)
6970         IF D$<>"" THEN
6980           OD(ONUM)=FN CLAMP(INT(VAL(D$)),0,31)
6990           ONUM=ONUM+1
7000         END IF
7010       NEXT K
7020     END IF
7030     IF W$="INSTRUMENT" THEN
7040       N=INT(VAL(FN TOKN$(L$,1)))
7050       IF N>=4 AND N<=63 THEN
7060         D$=MID$(L$,INSTR(INSTR(L$," ")+1,L$," ")+1)
7070         INSTRUMENT N,D$
7080       END IF
7090     END IF
7100     IF W$="PATTERN" THEN
7110       P=FN CLAMP(INT(VAL(FN TOKN$(L$,1))),0,31)
7120       PL(P)=FN CLAMP(INT(VAL(FN TOKN$(L$,2))),16,64)
7130       IF P+1>NPAT THEN NPAT=P+1
7140     END IF
7150     IF W$="C" THEN
7160       A=INT(VAL(FN TOKN$(L$,1))):C=INT(VAL(FN TOKN$(L$,2)))
7170       IF A>=0 AND A<64 AND C>=0 AND C<8 THEN
7180         X=FN IX(P,A,C)
7190         PN(X)=FN CLAMP(INT(VAL(FN TOKN$(L$,3))),0,97)
7200         PJ(X)=FN CLAMP(INT(VAL(FN TOKN$(L$,4))),0,63)
7210         PV(X)=FN CLAMP(INT(VAL(FN TOKN$(L$,5))),-1,15)
7220       END IF
7230     END IF
7240   WEND
7250   CLOSE #1
7260   IF ONUM=0 THEN
7270     ONUM=1:OD(0)=0
7280   END IF
7290   IF NPAT=0 THEN NPAT=1
7300   FOR K=0 TO ONUM-1
7310     IF OD(K)>=NPAT THEN OD(K)=0
7320   NEXT K
7330   CO=0:CR=0:CC=0:CF=0
7340   CALL TOCONTROLS
7350   CALL SYNCCTL
7360   CALL REDRAW
7370   CALL SETSTATUS("Opened "+F$+".")
7380 END PROC
7390 ' the PLAY lines of the whole song, one per call: CALLBACK-less, written to the file #1 or collected into the global list XL$ (for Copy)
7400 PROC EXPORTLINES(TOCLIP)
7410   LOCAL C,I,N,L,K,A$,B$,P,XN
7420   CALL FLATTEN(0,0)
7430   XL$=""
7440   XN=10
7450   XL$=XL$+STR$(XN)+" ' song made with the tracker of Engine1999 ("+STR$(BPM)+" beats a minute)"+CHR$(10)
7460   XN=XN+10
7470   FOR I=32 TO 63
7480     IF FN USES(I) THEN
7490       XL$=XL$+STR$(XN)+" INSTRUMENT "+STR$(I)+","+CHR$(34)+INSTRUMENT$(I)+CHR$(34)+CHR$(10)
7500       XN=XN+10
7510     END IF
7520   NEXT I
7530   FOR C=0 TO 7
7540     A$=FN CHSTR$(C)
7550     IF CM(C)<>0 THEN A$=""
7560     K=0
7570     WHILE LEN(A$)>0
7580       L=150
7590       IF LEN(A$)<=L THEN
7600         L=LEN(A$)
7610       ELSE
7620         WHILE L>1 AND MID$(A$,L+1,1)<>" "
7630           L=L-1
7640         WEND
7650       END IF
7660       B$=LEFT$(A$,L)
7670       A$=MID$(A$,L+1)
7680       IF K=0 THEN
7690         XL$=XL$+STR$(XN)+" V"+STR$(C+1)+"$="+CHR$(34)+B$+CHR$(34)+CHR$(10)
7700       ELSE
7710         XL$=XL$+STR$(XN)+" V"+STR$(C+1)+"$=V"+STR$(C+1)+"$+"+CHR$(34)+B$+CHR$(34)+CHR$(10)
7720       END IF
7730       XN=XN+10
7740       K=K+1
7750     WEND
7760     IF K=0 THEN
7770       XL$=XL$+STR$(XN)+" V"+STR$(C+1)+"$="+CHR$(34)+CHR$(34)+CHR$(10)
7780       XN=XN+10
7790     END IF
7800   NEXT C
7810   XL$=XL$+STR$(XN)+" PLAY V1$,V2$,V3$,V4$,V5$,V6$,V7$,V8$"+CHR$(10)
7820 END PROC
7830 PROC EXPORTF(F$)
7840   CALL EXPORTLINES(0)
7850   OPEN F$ FOR OUTPUT AS #1
7860   PRINT #1,XL$;
7870   CLOSE #1
7880   CALL SETSTATUS("Exported "+F$+" (MERGE or LOAD it, RUN plays it).")
7890 END PROC
7900 ' ====================================================================== the form
7910 PROC MAKEFORM
7920   UI PANEL 1,0,0,640,300,"Tracker"
7930   UI BUTTON 10,10,22,50,20,"Song"
7940   UI BUTTON 11,64,22,66,20,"Pattern"
7950   UI BUTTON 12,134,22,46,20,"Here"
7960   UI BUTTON 13,184,22,46,20,"Stop"
7970   CALL UK_SLIDER(20,246,22,384,"Tempo",40,240,BPM)
7980   UI BUTTON 30,10,46,20,20,"-"
7990   UI LABEL 31,34,48,46,14,""
8000   UI BUTTON 32,84,46,20,20,"+"
8010   UI BUTTON 33,114,46,20,20,"<"
8020   UI LABEL 34,138,48,130,14,""
8030   UI BUTTON 35,272,46,20,20,">"
8040   UI BUTTON 36,302,46,70,20,"Ch = Ins"
8050   UI BUTTON 37,376,46,64,20,"Step 1"
8060   UI LABEL 95,448,48,182,14,""
8070   UI BUTTON 40,10,70,20,20,"<"
8080   UI LABEL 41,34,72,230,14,""
8090   UI BUTTON 42,268,70,20,20,">"
8100   UI BUTTON 43,296,70,48,20,"Pat<"
8110   UI BUTTON 44,348,70,48,20,"Pat>"
8120   UI BUTTON 45,400,70,44,20,"New"
8130   UI BUTTON 46,448,70,52,20,"Clone"
8140   UI BUTTON 47,504,70,40,20,"Ins"
8150   UI BUTTON 48,548,70,40,20,"Del"
8160   UI BUTTON 49,592,70,38,20,"Len"
8170   UI GRID 50,10,94,COLS,GRH
8180   UI CHECK 94,10,246,80,16,"Mute"
8190   CALL UK_SLIDER(91,96,244,300,"Pan",-100,100,0)
8200   UI LABEL 60,10,266,620,14,""
8210   UI BUTTON 70,10,278,60,20,"Save"
8220   UI BUTTON 71,74,278,60,20,"Open"
8230   UI BUTTON 72,138,278,66,20,"Export"
8240   UI BUTTON 73,208,278,50,20,"Copy"
8250   UI BUTTON 74,262,278,54,20,"Clear"
8260   UI BUTTON 75,570,278,60,20,"Quit"
8270   UI FOCUS 50
8280 END PROC
8290 ' the controls that show the song (after Open / Clear)
8300 PROC TOCONTROLS
8310   IF HEADLESS THEN EXIT PROC
8320   CALL UK_SET(20,BPM)
8330 END PROC
8340 ' ====================================================================== what happens
8350 PROC REACT(E,K,V)
8360   LOCAL F$
8370   IF E=50 AND K=9 THEN CALL KEYIN(V)
8380   IF E=50 AND K=10 THEN CALL CLICKIN(V)
8390   IF FN UK_TRACK(E,V) THEN
8400     IF E=20 THEN BPM=V
8410     IF E=91 THEN
8420       CP(CC)=V
8430       CALL REDRAW
8440     END IF
8450   END IF
8460   IF E=94 AND K=2 THEN
8470     CM(CC)=V
8480     IF V<>0 THEN CM(CC)=-1
8490     CALL REDRAW
8500   END IF
8510   IF K=1 THEN
8520     IF E=10 THEN CALL STARTPLAY(0,0)
8530     IF E=11 THEN CALL STARTPLAY(CO,0)
8540     IF E=12 THEN CALL STARTPLAY(CO,CR)
8550     IF E=13 THEN CALL STOPPLAY
8560     IF E=30 THEN CALL SETOCT(OCV-1)
8570     IF E=32 THEN CALL SETOCT(OCV+1)
8580     IF E=33 THEN CALL SETINS(INS-1)
8590     IF E=35 THEN CALL SETINS(INS+1)
8600     IF E=36 THEN
8610       CI(CC)=INS
8620       CALL SETSTATUS("Channel "+STR$(CC+1)+" starts with instrument "+STR$(INS)+".")
8630     END IF
8640     IF E=37 THEN
8650       STP=STP+1
8660       IF STP>4 THEN STP=0
8670     END IF
8680     IF E=40 THEN CALL SETCO(CO-1)
8690     IF E=42 THEN CALL SETCO(CO+1)
8700     IF E=43 THEN
8710       OD(CO)=FN CLAMP(OD(CO)-1,0,NPAT-1)
8720       CALL GOROW(CR)
8730     END IF
8740     IF E=44 THEN
8750       OD(CO)=FN CLAMP(OD(CO)+1,0,NPAT-1)
8760       CALL GOROW(CR)
8770     END IF
8780     IF E=45 THEN CALL NEWPATTERN
8790     IF E=46 THEN CALL CLONEPATTERN
8800     IF E=47 THEN CALL INSSLOT
8810     IF E=48 THEN CALL DELSLOT
8820     IF E=49 THEN CALL CYCLELEN
8830     IF E=70 THEN
8840       F$=FN FR_PICK$("Save the song as",DR$,"*.trk;*.txt",-1)
8850       DR$=FR_DIR$
8860       IF F$<>"" THEN CALL SAVEF(F$)
8870     END IF
8880     IF E=71 THEN
8890       F$=FN FR_PICK$("Open a song",DR$,"*.trk;*.txt",0)
8900       DR$=FR_DIR$
8910       IF F$<>"" THEN CALL OPENF(F$)
8920     END IF
8930     IF E=72 THEN
8940       F$=FN FR_PICK$("Export as BASIC lines",DR$,"*.bas",-1)
8950       DR$=FR_DIR$
8960       IF F$<>"" THEN CALL EXPORTF(F$)
8970     END IF
8980     IF E=73 THEN
8990       CALL EXPORTLINES(-1)
9000       CLIPBOARD XL$
9010       CALL SETSTATUS("The PLAY lines are on the clipboard.")
9020     END IF
9030     IF E=74 THEN
9040       IF FN UK_ASK("Clear","Throw the song away and start a new one?") THEN
9050         CALL STOPPLAY
9060         CALL NEWSONG
9070         CALL TOCONTROLS
9080       END IF
9090     END IF
9100     IF E=75 THEN Q=1
9110     CALL SYNCCTL
9120     CALL REDRAW
9130     UI FOCUS 50
9140   END IF
9150 END PROC
9160 ' ====================================================================== the self-test (TRACKER_HEADLESS=1 in the console host: tests the song / string code, no window)
9170 PROC CHECK(OK,T$)
9180   IF OK=0 THEN
9190     PRINT "FAIL: ";T$
9200     BAD=BAD+1
9210   END IF
9220   NCHK=NCHK+1
9230 END PROC
9240 PROC SELFTEST
9250   LOCAL X,Y,A$,B$,I
9260   HEADLESS=-1
9270   BAD=0:NCHK=0
9280   CALL NEWSONG
9290   CALL CHECK(FN NOTE$(49)="C-4" AND FN NOTE$(40)="D#3" AND FN NOTE$(0)="---" AND FN NOTE$(97)="===" AND FN NOTE$(96)="B-7","note names")
9300   OCV=3
9310   CALL CHECK(FN PIANO(ASC("z"))=37 AND FN PIANO(ASC("m"))=48 AND FN PIANO(ASC("Z"))=37 AND FN PIANO(ASC("q"))=49 AND FN PIANO(ASC("i"))=61 AND FN PIANO(ASC("a"))=0,"piano keys")
9320   CALL CHECK(FN CELL$(0,0,0)="--- .. ." AND LEN(FN CELL$(0,0,0))=8,"an empty cell")
9330   X=FN IX(0,0,0)
9340   PN(X)=49:PJ(X)=4:PV(X)=15
9350   CALL CHECK(FN CELL$(0,0,0)="C-4 04 F","a filled cell")
9360   PN(FN IX(0,4,0))=97
9370   PN(FN IX(0,8,0))=40
9380   BPM=120:CP(0)=0:CI(0)=4
9390   CALL FLATTEN(0,0)
9400   CALL CHECK(FT=32,"the song has 32 rows")
9410   A$=FN CHSTR$(0)
9420   CALL CHECK(A$="T120 MN Y0 I4 V0.8 V1 O4 MN C4 R4 O3 ML D#1 MN D#2","the PLAY string of a channel: "+A$)
9430   CALL CHECK(FN CHSTR$(1)="T120 MN Y0.3 I5 V0.8 R1 R1","an empty channel is its header and rests: "+FN CHSTR$(1))
9440   ' the music string must be accepted by PLAY (it checks its strings), also when the notes use other instruments and volumes
9450   PN(FN IX(0,12,1))=60:PJ(FN IX(0,12,1))=33:PV(FN IX(0,12,1))=7
9460   PN(FN IX(0,13,1))=61
9470   PN(FN IX(0,15,1))=5
9480   CALL FLATTEN(0,0)
9490   B$=FN CHSTR$(1)
9500   CALL CHECK(B$="T120 MN Y0.3 I5 V0.8 R2. I33 V0.467 O4 MN B16 O5 MN C8 O0 ML E1 MN E16","a second channel: "+B$)
9510   PLAY A$,B$
9520   ' from a later row: the first notes are skipped, a note that started earlier is not played (its row is not in the range)
9530   CALL FLATTEN(0,8)
9540   CALL CHECK(FT=24 AND FN CHSTR$(0)="T120 MN Y0 I4 V0.8 O3 ML D#1 MN D#2" AND FLR(0)=8,"playing from row 8: "+FN CHSTR$(0))
9550   ' two slots: the second pattern follows the first
9560   CALL NEWSONG
9570   CALL INSSLOT
9580   CALL NEWPATTERN
9590   CALL CHECK(ONUM=2 AND OD(0)=0 AND OD(1)=1 AND NPAT=2,"a new slot and pattern")
9600   PN(FN IX(1,0,2))=50
9610   CALL FLATTEN(0,0)
9620   CALL CHECK(FT=64 AND FLO(32)=1 AND FLR(32)=0 AND FLN(32*8+2)=50,"the order is flattened")
9630   A$=FN CHSTR$(2)
9640   CALL CHECK(LEFT$(A$,20)="T125 MN Y-0.6 I6 R1" OR LEFT$(A$,12)="T125 MN Y-0.","pan in the header: "+A$)
9650   ' the cursor and the editing
9660   CALL NEWSONG
9670   CR=0:CC=0:CF=0
9680   OCV=4:INS=5:STP=2
9690   CALL KEYACT(ASC("z"))
9700   CALL CHECK(PN(FN IX(0,0,0))=49 AND PJ(FN IX(0,0,0))=5 AND CR=2,"a piano key writes the note with the instrument and steps")
9710   CALL KEYACT(49)
9720   CALL CHECK(PN(FN IX(0,2,0))=97 AND CR=4,"1 puts a note off")
9730   CALL KEYACT(4098)
9740   CALL KEYACT(51)
9750   CALL KEYACT(55)
9760   CALL CHECK(CF=3 AND PJ(FN IX(0,4,0))=37 AND CR=4,"two digits are the instrument")
9770   CALL KEYACT(ASC("C"))
9780   CALL CHECK(PV(FN IX(0,4,0))=12 AND CF=0 AND CR=6,"a hex digit is the volume (upper or lower case)")
9790   CALL KEYACT(4097)
9800   CALL CHECK(CC=0 AND CF=0,"Left at the first field stays")
9810   CALL KEYACT(4098+65536)
9820   CALL CHECK(CC=1 AND CF=0,"Shift + Right jumps a channel")
9830   CALL KEYACT(4105)
9840   CALL KEYACT(4103)
9850   CALL CHECK(CR=0,"PageUp stops at the first row")
9860   CALL KEYACT(4102)
9870   CALL CHECK(CR=31,"End goes to the last row")
9880   ' save and open
9890   CALL NEWSONG
9900   BPM=99:CP(3)=-77:CM(5)=-1:CI(2)=33
9910   INSTRUMENT 33,"NAME TESTINS; OSC SAW 0.5; GAIN 1.1"
9920   CALL INSSLOT
9930   CALL NEWPATTERN
9940   PL(1)=48
9950   PN(FN IX(1,5,6))=60:PJ(FN IX(1,5,6))=33:PV(FN IX(1,5,6))=3
9960   PN(FN IX(0,31,7))=97
9970   CALL SAVEF("mem:song1")
9980   CALL NEWSONG
9990   INSTRUMENT 33
10000   CALL OPENF("mem:song1")
10010   CALL CHECK(BPM=99 AND CP(3)=-77 AND CM(5)<>0 AND CM(4)=0 AND CI(2)=33 AND ONUM=2 AND OD(1)=1 AND NPAT=2 AND PL(1)=48 AND PL(0)=32,"song header and order survive a save and open")
10020   CALL CHECK(PN(FN IX(1,5,6))=60 AND PJ(FN IX(1,5,6))=33 AND PV(FN IX(1,5,6))=3 AND PN(FN IX(0,31,7))=97 AND PN(FN IX(0,0,0))=0 AND PV(FN IX(0,0,0))=-1,"cells survive a save and open")
10030   CALL CHECK(FN INAME$(33)="TESTINS","the instrument of the song is defined again by Open: "+FN INAME$(33))
10040   ' export
10050   CALL EXPORTLINES(0)
10060   CALL CHECK(INSTR(XL$,"INSTRUMENT 33,")>0 AND INSTR(XL$,"PLAY V1$,V2$,V3$,V4$,V5$,V6$,V7$,V8$")>0 AND INSTR(XL$,"V1$=")>0,"the exported lines")
10070   ' TRACKER_HEADLESS=file: also opens that song and plays its strings (PLAY checks them)
10080   IF ENV$("TRACKER_HEADLESS")<>"1" THEN
10090     CALL OPENF(ENV$("TRACKER_HEADLESS"))
10100     CALL FLATTEN(0,0)
10110     FOR I=0 TO 7
10120       S$(I)=FN CHSTR$(I)
10130     NEXT I
10140     PLAY S$(0),S$(1),S$(2),S$(3),S$(4),S$(5),S$(6),S$(7)
10150     PRINT "song ";ENV$("TRACKER_HEADLESS");": ";ONUM;" slots, ";NPAT;" patterns, ";FT;" rows, strings of ";LEN(S$(0));" ";LEN(S$(1));" ";LEN(S$(2));" ";LEN(S$(3));" ";LEN(S$(4));" ";LEN(S$(5));" ";LEN(S$(6));" ";LEN(S$(7));" characters"
10160   END IF
10170   PRINT "tracker selftest ";
10180   IF BAD=0 THEN PRINT "ok ";NCHK ELSE PRINT BAD;" FAILED of ";NCHK
10190 END PROC
10200 ' ====================================================================== main program
10210 IF ENV$("TRACKER_HEADLESS")<>"" THEN
10220   CALL SELFTEST
10230   END
10240 END IF
10250 UI OPEN
10260 CALL NEWSONG
10270 CALL MAKEFORM
10280 CALL SYNCCTL
10290 CALL REDRAW
10300 CALL SETSTATUS("Piano keys Z S X D C V G B H N J M write notes, Space plays, Tab leaves the grid.")
10310 ON ERROR GOTO ERRH
10320 ARGLOAD$=""
10330 IF ARGC>=1 THEN ARGLOAD$=ARG$(1)
10340 IF ARGLOAD$<>"" THEN CALL OPENF(ARGLOAD$)
10350 LABEL LOOPTOP
10360 ON ERROR GOTO ERRH
10370 ON UI GOSUB HANDLER
10380 REPEAT
10390   GFX VSYNC
10400   UI UPDATE
10410   CALL PLAYTICK
10420 UNTIL Q=1
10430 ON UI OFF
10440 ON ERROR OFF
10450 SOUND OFF
10460 UI CLOSE
10470 REM no GFX RESET here
10480 CLS
10490 PRINT "Tracker finished."
10500 END
10510 LABEL ERRH
10520 ON UI OFF
10530 CALL UK_MSG("Error",ERR$+"|(line "+STR$(ERL)+")")
10540 RESUME LOOPTOP
10550 LABEL HANDLER
10560 E=UI("EVENT")
10570 WHILE E<>0
10580   EK=UI("KIND")
10590   EV=UI("VALUE")
10600   CALL REACT(E,EK,EV)
10610   E=UI("EVENT")
10620 WEND
10630 RETURN
