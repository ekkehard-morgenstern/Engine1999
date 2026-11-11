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
80 ' and plays them on the channels 1 to 8. While it plays the strings are made in pieces, a few rows ahead of the playhead (through the voices of the play: driver), so you can go on
90 ' editing: a change of a cell, Mute, Pan, the start instrument, the tempo or the order is heard as soon as the playhead gets there (at the next row if it lies in what is queued already).
100 ' The check Follow lets the cursor run with the playhead; without it the cursor stays where it is (edit ahead of the playhead). The fields have colors (notes blue / purple, note off red,
110 ' instrument green, volume orange, empty grey; the palette entries 240..255 of the UI's layer are set for it and put back at the end). It is written in BASIC on the UI subsystem (the character grid UI GRID, new for this tool) and uses the libraries
120 ' uikit and filereq. Needs the window (engine1999).
130 '   The grid is the pattern. Click a cell or use the cursor keys (Left / Right go through the fields note, instrument, volume and on to the next channel,
140 '   Shift + Left / Right jump a channel, Up / Down a row, PageUp / PageDown 16 rows, Home / End the first / last row). Tab leaves the grid.
150 '   In the note field the keys of a piano play the note: Z S X D C V G B H N J M are C to B of the octave shown, Q 2 W 3 E R 5 T 6 Y 7 U I the octave above
160 '   (and the next C); 1 puts a note off; Delete / Backspace clears the field. The note is written with the current instrument (the buttons < >, or the
170 '   keys [ and ]) and the cursor goes down by the step. In the instrument field type two digits (04..63), in the volume field a hex digit.
180 '   - and + change the octave. Enter plays the row, Space plays the song from the cursor and stops it again.
190 '   Channel: the check Mute silences it, the slider Pan puts it in the stereo picture, Ch = Ins makes the current instrument the one it starts with.
200 '   Order: < > go through the slots, Pat< Pat> choose the pattern of a slot, New starts an empty one, Clone copies the pattern, Ins / Del add or remove a slot,
210 '   Len cycles the length 16 / 32 / 48 / 64.   Save / Open keep a song in a text file (.trk, with the instruments of 32..63 that it uses), Export writes BASIC
220 '   lines, Copy puts the PLAY lines on the clipboard, Clear starts a new song.
230 ' The pane under the grid has three tabs (the strip, id 80): Order = slots and patterns, Channel = Mute / Pan / start instrument, File = Save / Open / Export / Copy / Clear.
240 ' widget ids: 80 tab strip, 1 form, 10 Song, 11 Pattern, 12 Here, 13 Stop, 20 tempo slider (+1 label, +2 readout), 30 oct -, 31 oct label, 32 oct +, 33 inst <, 34 inst label, 35 inst >,
250 ' 36 Ch = Ins, 37 step button, 94 Mute, 91 Pan slider (+1, +2), 40 order <, 41 order label, 42 order >, 43 Pat<, 44 Pat>, 45 New, 46 Clone, 47 Ins, 48 Del, 49 Len,
260 ' 50 the grid, 96 Follow, 60 status, 70 Save, 71 Open, 72 Export, 73 Copy, 74 Clear, 75 Quit
270 LOAD LIBRARY "uikit"
280 LOAD LIBRARY "filereq"
290 REM the graphics of the calling program stay (no GFX RESET)
300 CLS
310 BREAK OFF
320 Q=0
330 HEADLESS=0
340 COLS=74
350 GRH=13
360 CENTER=5
370 DIM PN(16384),PJ(16384),PV(16384),PL(32),OD(64),CI(8),CP(8),CM(8),S$(8)
380 DIM FLO(4096),FLR(4096),FOFF(64),QC(8),RS(8),XI(8),XV(8),XO(8),CAPS$(8),SPL(16)
390 DIM NT$(12),NP$(12),LV(8),LS$(8),FO(4),FL(4)
400 FOR I=0 TO 11
410   READ NT$(I),NP$(I)
420 NEXT I
430 DATA "C-","C","C#","C#","D-","D","D#","D#","E-","E","F-","F","F#","F#","G-","G","G#","G#","A-","A","A#","A#","B-","B"
440 FOR I=0 TO 7
450   READ LV(I),LS$(I)
460 NEXT I
470 DATA 16,"1",12,"2.",8,"2",6,"4.",4,"4",3,"8.",2,"8",1,"16"
480 FO(0)=0:FO(1)=4:FO(2)=5:FO(3)=7
490 FL(0)=3:FL(1)=1:FL(2)=1:FL(3)=1
500 DR$=""
510 ' the song: ONUM slots of the order, NPAT patterns, BPM; the cursor: CO slot, CR row, CC channel, CF field (0 note, 1 / 2 the instrument digits, 3 volume)
520 BPM=125
530 ONUM=1
540 NPAT=1
550 CO=0
560 CR=0
570 CC=0
580 CF=0
590 OCV=3
600 INS=4
610 STP=1
620 PLG=0
630 PCO=0
640 PHROW=0
650 PT0=0
660 FT=0
670 AC$=""
680 CURI=0
690 CURV=12
700 CURO=-1
710 PN2=0
720 PS=0
730 PI2=0
740 PV2=-1
750 PS0=0
760 PRL=0
770 PBPM=125
780 RSA=0
790 FOLLOW=-1
800 LOOK=8
810 CAP=0
820 BEI=0
830 BEV=-1
840 PCO=0
850 PHROW=0
860 ' ====================================================================== functions (they must stand before the lines that use them)
870 FUNC NUM$(V)
880   RETURN STR$(INT(V*1000+0.5)/1000)
890 END FUNC
900 FUNC IX(P,R,C)
910   RETURN (P*64+R)*8+C
920 END FUNC
930 FUNC CLAMP(V,LO,HI)
940   IF V<LO THEN RETURN LO
950   IF V>HI THEN RETURN HI
960   RETURN V
970 END FUNC
980 FUNC NOTE$(N)
990   IF N=0 THEN RETURN "---"
1000   IF N=97 THEN RETURN "==="
1010   RETURN NT$((N-1) MOD 12)+STR$((N-1)\12)
1020 END FUNC
1030 FUNC CELL$(P,R,C)
1040   LOCAL X,N,I,V,A$
1050   X=FN IX(P,R,C)
1060   N=PN(X):I=PJ(X):V=PV(X)
1070   A$=FN NOTE$(N)+" "
1080   IF I>0 THEN A$=A$+RIGHT$("0"+STR$(I),2) ELSE A$=A$+".."
1090   IF V>=0 THEN A$=A$+" "+MID$("0123456789ABCDEF",V+1,1) ELSE A$=A$+" ."
1100   RETURN A$
1110 END FUNC
1120 ' the name of instrument N (from its definition string: NAME xxx; ...)
1130 FUNC INAME$(N)
1140   LOCAL D$,E
1150   D$=INSTRUMENT$(N)
1160   IF LEFT$(D$,5)<>"NAME " THEN RETURN "?"
1170   E=INSTR(D$,";")
1180   IF E=0 THEN E=LEN(D$)+1
1190   RETURN MID$(D$,6,E-6)
1200 END FUNC
1210 ' the I-th (0..) word of S$ (blanks), "" if none
1220 FUNC TOKN$(S$,N)
1230   LOCAL I,K,C$,T$,INW
1240   K=0:T$="":INW=0
1250   FOR I=1 TO LEN(S$)+1
1260     IF I<=LEN(S$) THEN C$=MID$(S$,I,1) ELSE C$=" "
1270     IF C$=" " OR C$=CHR$(9) THEN
1280       IF INW THEN
1290         IF K=N THEN RETURN T$
1300         K=K+1:T$="":INW=0
1310       END IF
1320     ELSE
1330       T$=T$+C$:INW=-1
1340     END IF
1350   NEXT I
1360   RETURN ""
1370 END FUNC
1380 ' is instrument I (32..63) used by a channel or a cell of the song?
1390 FUNC USES(I)
1400   LOCAL P,R,C,X
1410   FOR C=0 TO 7
1420     IF CI(C)=I THEN RETURN -1
1430   NEXT C
1440   FOR P=0 TO NPAT-1
1450     FOR R=0 TO 63
1460       FOR C=0 TO 7
1470         IF PJ(FN IX(P,R,C))=I THEN RETURN -1
1480       NEXT C
1490     NEXT R
1500   NEXT P
1510   RETURN 0
1520 END FUNC
1530 ' the index of the cell of channel C in flat row T of the song (BUILDMAP made FLO / FLR: the slot and the row of each flat row)
1540 FUNC CX(T,C)
1550   RETURN FN IX(OD(FLO(T)),FLR(T),C)
1560 END FUNC
1570 ' the first flat row after T where channel C has a note field (a note or a note off), FT if there is none
1580 FUNC NEXTEV(C,T)
1590   LOCAL U
1600   U=T+1
1610   WHILE U<FT
1620     IF PN(FN CX(U,C))<>0 THEN RETURN U
1630     U=U+1
1640   WEND
1650   RETURN FT
1660 END FUNC
1670 ' the flat row of the note that still sounds before flat row T on channel C (the last note field is a note, not a note off), -1 if none
1680 FUNC LASTON(C,T)
1690   LOCAL U,N
1700   U=T-1
1710   WHILE U>=0
1720     N=PN(FN CX(U,C))
1730     IF N<>0 THEN
1740       IF N>=5 AND N<=96 THEN RETURN U
1750       RETURN -1
1760     END IF
1770     U=U-1
1780   WEND
1790   RETURN -1
1800 END FUNC
1810 ' ====================================================================== the song
1820 PROC CLEARPAT(P)
1830   LOCAL R,C,X
1840   FOR R=0 TO 63
1850     FOR C=0 TO 7
1860       X=FN IX(P,R,C)
1870       PN(X)=0:PJ(X)=0:PV(X)=-1
1880     NEXT C
1890   NEXT R
1900 END PROC
1910 PROC NEWSONG
1920   LOCAL P,C
1930   FOR P=0 TO 31
1940     CALL CLEARPAT(P)
1950     PL(P)=32
1960   NEXT P
1970   FOR P=0 TO 63
1980     OD(P)=0
1990   NEXT P
2000   ONUM=1:NPAT=1:CO=0:CR=0:CC=0:CF=0
2010   BPM=125
2020   CI(0)=4:CI(1)=5:CI(2)=6:CI(3)=7:CI(4)=13:CI(5)=14:CI(6)=15:CI(7)=9
2030   FOR C=0 TO 7
2040     CP(C)=0:CM(C)=0
2050   NEXT C
2060   CP(0)=-30:CP(1)=30:CP(2)=-60:CP(3)=60
2070 END PROC
2080 ' ====================================================================== the music strings
2090 ' the song as one list of rows ("flat rows": the patterns of the order one after the other): FLO = the slot, FLR = the row in its pattern, FOFF = the first flat row of
2100 ' a slot, FT = the number of rows. The cells are always read from the patterns, so a change of a cell is seen at once
2110 PROC BUILDMAP
2120   LOCAL O,R,P,T
2130   T=0
2140   FOR O=0 TO ONUM-1
2150     FOFF(O)=T
2160     P=OD(O)
2170     FOR R=0 TO PL(P)-1
2180       FLO(T)=O:FLR(T)=R
2190       T=T+1
2200     NEXT R
2210   NEXT O
2220   FT=T
2230 END PROC
2240 ' K rows of rests
2250 PROC EMITREST(K)
2260   LOCAL Z,I
2270   Z=K
2280   WHILE Z>0
2290     I=0
2300     WHILE LV(I)>Z
2310       I=I+1
2320     WEND
2330     Z=Z-LV(I)
2340     AC$=AC$+" R"+LS$(I)
2350   WEND
2360 END PROC
2370 ' the open note PN2 (instrument PI2, volume PV2) for K rows: pieces joined by legato, the last one with the normal gap
2380 PROC EMITNOTE(K)
2390   LOCAL Z,I,O,S
2400   IF PI2>0 AND PI2<>CURI THEN
2410     AC$=AC$+" I"+STR$(PI2)
2420     CURI=PI2
2430   END IF
2440   IF PV2>=0 AND PV2<>CURV THEN
2450     AC$=AC$+" V"+FN NUM$(PV2/15)
2460     CURV=PV2
2470   END IF
2480   O=(PN2-1)\12
2490   S=(PN2-1) MOD 12
2500   IF O<>CURO THEN
2510     AC$=AC$+" O"+STR$(O)
2520     CURO=O
2530   END IF
2540   Z=K
2550   WHILE Z>0
2560     I=0
2570     WHILE LV(I)>Z
2580       I=I+1
2590     WEND
2600     Z=Z-LV(I)
2610     IF Z>0 THEN AC$=AC$+" ML" ELSE AC$=AC$+" MN"
2620     AC$=AC$+" "+NP$(S)+LS$(I)
2630   WEND
2640 END PROC
2650 ' the instrument BEI (0 = none) and the volume BEV (-1 = none) that are in effect before flat row T on channel C: those of the last notes that set them
2660 PROC BACKSCAN(C,T)
2670   LOCAL U,X
2680   BEI=0:BEV=-1
2690   U=T-1
2700   WHILE U>=0 AND (BEI=0 OR BEV<0)
2710     X=FN CX(U,C)
2720     IF PN(X)>=5 AND PN(X)<=96 THEN
2730       IF BEI=0 AND PJ(X)>0 THEN BEI=PJ(X)
2740       IF BEV<0 AND PV(X)>=0 THEN BEV=PV(X)
2750     END IF
2760     U=U-1
2770   WEND
2780 END PROC
2790 ' the PLAY string of channel C is made in pieces as the song plays (the playhead needs only a few rows ahead, so a change is heard soon): STARTCH writes the
2800 ' header (tempo, pan, instrument, volume as they are in effect before flat row T) and a note that is still sounding there; FILLCH adds the notes and rests from
2810 ' QC(C) on up to the flat row TG; SENDCH hands the piece to the voice (or collects it when CAP is set: the tests and Export)
2820 PROC STARTCH(C,T)
2830   LOCAL L,E
2840   CALL BACKSCAN(C,T)
2850   XI(C)=CI(C)
2860   IF BEI>0 THEN XI(C)=BEI
2870   XV(C)=12
2880   IF BEV>=0 THEN XV(C)=BEV
2890   XO(C)=-1
2900   AC$="T"+STR$(PBPM)+" MN Y"+FN NUM$(CP(C)/100)+" I"+STR$(XI(C))+" V"+FN NUM$(XV(C)/15)
2910   QC(C)=T
2920   IF PN(FN CX(T,C))=0 THEN
2930     L=FN LASTON(C,T)
2940     IF L>=0 THEN
2950       E=FN NEXTEV(C,T)
2960       PN2=PN(FN CX(L,C)):PI2=XI(C):PV2=XV(C)
2970       CURI=XI(C):CURV=XV(C):CURO=-1
2980       CALL EMITNOTE(E-T)
2990       XO(C)=CURO
3000       QC(C)=E
3010     END IF
3020   END IF
3030 END PROC
3040 PROC FILLCH(C,TG)
3050   LOCAL R,N,E,X
3060   CURI=XI(C):CURV=XV(C):CURO=XO(C)
3070   IF LEN(AC$)=0 THEN
3080     AC$="T"+STR$(PBPM)+" V"+FN NUM$(XV(C)/15)
3090     CURO=-1
3100   END IF
3110   WHILE QC(C)<FT AND QC(C)<TG
3120     R=QC(C)
3130     X=FN CX(R,C)
3140     N=PN(X)
3150     E=FN NEXTEV(C,R)
3160     IF N>=5 AND N<=96 THEN
3170       PN2=N:PI2=PJ(X):PV2=PV(X)
3180       CALL EMITNOTE(E-R)
3190     ELSE
3200       CALL EMITREST(E-R)
3210     END IF
3220     QC(C)=E
3230   WEND
3240   XI(C)=CURI:XV(C)=CURV:XO(C)=CURO
3250 END PROC
3260 PROC SENDCH(C)
3270   LOCAL CH
3280   IF LEN(AC$)>0 THEN
3290     IF CAP THEN
3300       IF LEN(CAPS$(C))>0 THEN CAPS$(C)=CAPS$(C)+" "
3310       CAPS$(C)=CAPS$(C)+AC$
3320     ELSE
3330       CH=21+C
3340       PRINT #CH,AC$
3350     END IF
3360   END IF
3370   AC$=""
3380 END PROC
3390 ' the whole PLAY string of channel C from flat row T on (the tests and Export; BUILDMAP first)
3400 FUNC STRFROM$(C,T)
3410   CAP=-1
3420   CAPS$(C)=""
3430   PBPM=BPM
3440   AC$=""
3450   CALL STARTCH(C,T)
3460   CALL FILLCH(C,FT)
3470   CALL SENDCH(C)
3480   CAP=0
3490   RETURN CAPS$(C)
3500 END FUNC
3510 FUNC CHSTR$(C)
3520   RETURN FN STRFROM$(C,0)
3530 END FUNC
3540 ' ====================================================================== the screen
3550 PROC SETSTATUS(T$)
3560   IF HEADLESS THEN EXIT PROC
3570   UI CAPTION 60,T$
3580 END PROC
3590 ' the order label, the instrument label ... after anything changed
3600 PROC SYNCCTL
3610   IF HEADLESS THEN EXIT PROC
3620   UI CAPTION 31,"Oct "+STR$(OCV)
3630   UI CAPTION 34,RIGHT$("0"+STR$(INS),2)+" "+FN INAME$(INS)
3640   UI CAPTION 41,"Slot "+STR$(CO+1)+"/"+STR$(ONUM)+"  Pattern "+STR$(OD(CO))+"  Len "+STR$(PL(OD(CO)))
3650   UI CAPTION 37,"Step "+STR$(STP)
3660   CALL UK_SET(91,CP(CC))
3670   UI VALUE 94,CM(CC)
3680   UI CAPTION 95,"Channel "+STR$(CC+1)+" ("+FN INAME$(CI(CC))+")"
3690 END PROC
3700 ' the colors of the grid: the palette entries 240..255 of the UI's layer (the program's own are put back at the end)
3710 PROC SETPAL
3720   LOCAL I
3730   IF HEADLESS THEN EXIT PROC
3740   FOR I=0 TO 15
3750     SPL(I)=GFX("PALETTE",3,240+I)
3760   NEXT I
3770   GFX PALETTE 3,240,RGB(250,250,246)
3780   GFX PALETTE 3,241,RGB(228,236,248)
3790   GFX PALETTE 3,242,RGB(208,222,242)
3800   GFX PALETTE 3,243,RGB(255,232,150)
3810   GFX PALETTE 3,244,RGB(30,80,160)
3820   GFX PALETTE 3,245,RGB(255,255,255)
3830   GFX PALETTE 3,246,RGB(10,60,140)
3840   GFX PALETTE 3,247,RGB(130,40,160)
3850   GFX PALETTE 3,248,RGB(10,120,60)
3860   GFX PALETTE 3,249,RGB(176,90,0)
3870   GFX PALETTE 3,250,RGB(176,182,192)
3880   GFX PALETTE 3,251,RGB(192,32,32)
3890   GFX PALETTE 3,252,RGB(100,110,128)
3900   GFX PALETTE 3,253,RGB(58,66,86)
3910   GFX PALETTE 3,254,RGB(240,240,240)
3920   GFX PALETTE 3,255,RGB(128,48,48)
3930 END PROC
3940 PROC RESTOREPAL
3950   LOCAL I
3960   IF HEADLESS THEN EXIT PROC
3970   FOR I=0 TO 15
3980     GFX PALETTE 3,240+I,SPL(I)
3990   NEXT I
4000 END PROC
4010 ' the grid: the pattern of the slot under the cursor, the cursor row in the middle; every field has its color (note blue / purple, off red, instrument green,
4020 ' volume orange, empty fields grey, a muted channel grey), every 4th row has a tinted background, every 16th a stronger one, the row that plays is yellow
4030 PROC REDRAW
4040   LOCAL G,R,P,C,X,N,I,V,BG,FG,PB,HB,N$,I$,V$,M$,L$,CL
4050   IF HEADLESS THEN EXIT PROC
4060   P=OD(CO)
4070   UI GRIDPUT 50,0,0,"Row",254,253
4080   FOR C=0 TO 7
4090     N$=FN INAME$(CI(C))+"       "
4100     HB=253
4110     M$=" "
4120     IF CM(C)<>0 THEN
4130       HB=255:M$="m"
4140     END IF
4150     UI GRIDPUT 50,3+C*9,0," "+STR$(C+1)+M$+LEFT$(N$,5)+" ",254,HB
4160   NEXT C
4170   FOR G=1 TO GRH-1
4180     R=CR+G-CENTER
4190     IF R<0 OR R>=PL(P) THEN
4200       UI GRIDPUT 50,0,G,SPACE$(COLS),250,240
4210     ELSE
4220       BG=240
4230       IF R MOD 4=0 THEN BG=241
4240       IF R MOD 16=0 THEN BG=242
4250       IF PLG THEN
4260         IF CO=PCO AND R=PHROW THEN BG=243
4270       END IF
4280       UI GRIDPUT 50,0,G,RIGHT$("0"+STR$(R),2)+" ",252,BG
4290       FOR C=0 TO 7
4300         CL=CM(C)<>0
4310         X=FN IX(P,R,C)
4320         N=PN(X):I=PJ(X):V=PV(X)
4330         N$=FN NOTE$(N)
4340         FG=246
4350         IF N=0 THEN FG=250
4360         IF N=97 THEN FG=251
4370         IF N>0 AND N<97 THEN
4380           IF INSTR(N$,"#")>0 THEN FG=247
4390         END IF
4400         IF CL THEN FG=250
4410         UI GRIDPUT 50,3+C*9,G,N$,FG,BG
4420         I$=" .."
4430         FG=250
4440         IF I>0 THEN
4450           I$=" "+RIGHT$("0"+STR$(I),2)
4460           IF NOT CL THEN FG=248
4470         END IF
4480         UI GRIDPUT 50,6+C*9,G,I$,FG,BG
4490         V$=" ."
4500         FG=250
4510         IF V>=0 THEN
4520           V$=" "+MID$("0123456789ABCDEF",V+1,1)
4530           IF NOT CL THEN FG=249
4540         END IF
4550         UI GRIDPUT 50,9+C*9,G,V$,FG,BG
4560         IF C<7 THEN UI GRIDPUT 50,11+C*9,G,"|",250,BG
4570       NEXT C
4580     END IF
4590   NEXT G
4600   L$=FN CELL$(P,CR,CC)
4610   UI GRIDPUT 50,3+CC*9+FO(CF),CENTER,MID$(L$,FO(CF)+1,FL(CF)),245,244
4620 END PROC
4630 ' ====================================================================== playing
4640 PROC STOPPLAY
4650   SOUND OFF
4660   IF PLG THEN
4670     PLG=0
4680     CALL REDRAW
4690     CALL SETSTATUS("Stopped.")
4700   END IF
4710 END PROC
4720 ' the song is played from flat row T: BUILDMAP (the song as it is now), every channel is queued anew
4730 PROC FULLSYNC(O0,R0)
4740   LOCAL C,T
4750   CALL BUILDMAP
4760   IF O0>=ONUM THEN
4770     O0=ONUM-1:R0=0
4780   END IF
4790   IF R0>=PL(OD(O0)) THEN R0=PL(OD(O0))-1
4800   T=FOFF(O0)+R0
4810   SOUND OFF
4820   PBPM=BPM
4830   PS0=T:PT0=TI:PRL=T
4840   RSA=0
4850   PCO=O0:PHROW=R0
4860   FOR C=0 TO 7
4870     RS(C)=0
4880     AC$=""
4890     IF CM(C)=0 THEN
4900       CALL STARTCH(C,T)
4910       CALL FILLCH(C,T+LOOK)
4920       CALL SENDCH(C)
4930     END IF
4940   NEXT C
4950 END PROC
4960 ' one channel is queued anew from flat row T (after a change in what is queued already)
4970 PROC CHSYNC(C,T)
4980   RS(C)=0
4990   SOUND OFF C+1
5000   AC$=""
5010   CALL STARTCH(C,T)
5020   CALL FILLCH(C,T+LOOK)
5030   CALL SENDCH(C)
5040 END PROC
5050 PROC STARTPLAY(SO,SR)
5060   CALL FULLSYNC(SO,SR)
5070   PLG=-1
5080   CALL REDRAW
5090   CALL SETSTATUS("Playing: changes are heard as the song goes on.")
5100 END PROC
5110 ' called every frame: the playhead (from the clock), the channels that must be queued anew, the next notes
5120 PROC PLAYTICK
5130   LOCAL R,C
5140   IF NOT PLG THEN EXIT PROC
5150   R=PS0+INT((TI-PT0)*PBPM*4/3600)
5160   IF R>=FT THEN
5170     CALL STOPPLAY
5180     EXIT PROC
5190   END IF
5200   IF R<>PRL THEN
5210     IF R-PRL>3 THEN RSA=-1
5220     IF RSA THEN
5230       CALL FULLSYNC(FLO(R),FLR(R))
5240     ELSE
5250       PRL=R
5260       PCO=FLO(R):PHROW=FLR(R)
5270       FOR C=0 TO 7
5280         IF RS(C) AND CM(C)=0 THEN CALL CHSYNC(C,R)
5290         RS(C)=0
5300       NEXT C
5310     END IF
5320     IF FOLLOW THEN
5330       CO=PCO:CR=PHROW
5340       CALL SYNCCTL
5350     END IF
5360     CALL REDRAW
5370   END IF
5380   IF RSA THEN EXIT PROC
5390   FOR C=0 TO 7
5400     IF CM(C)=0 AND RS(C)=0 THEN
5410       IF QC(C)<PRL+LOOK AND QC(C)<FT THEN
5420         AC$=""
5430         CALL FILLCH(C,PRL+LOOK)
5440         CALL SENDCH(C)
5450       END IF
5460     END IF
5470   NEXT C
5480 END PROC
5490 ' an edit of the cell (pattern P, row R, channel C) while the song plays: if the cell lies in what the channel has queued already, it is queued anew at the next row
5500 PROC TOUCH(P,R,C)
5510   LOCAL O,T
5520   IF NOT PLG THEN EXIT PROC
5530   FOR O=0 TO ONUM-1
5540     IF OD(O)=P THEN
5550       T=FOFF(O)+R
5560       IF T>PRL THEN
5570         IF T<QC(C) THEN RS(C)=-1
5580         IF T=QC(C) AND PN(FN IX(P,R,C))=0 THEN RS(C)=-1
5590       END IF
5600     END IF
5610   NEXT O
5620 END PROC
5630 ' a change of the song's order, lengths or tempo while it plays: everything is queued anew at the next row
5640 PROC GLOBALEDIT
5650   IF PLG THEN RSA=-1
5660 END PROC
5670 ' the frequency of tracker note N (C-4 = 49 = middle C)
5680 FUNC NOTEF(N)
5690   RETURN 261.626*(2^((N-49)/12))
5700 END FUNC
5710 PROC PREVIEW(N,I)
5720   LOCAL W
5730   IF PLG THEN EXIT PROC
5740   IF N<5 OR N>96 THEN EXIT PROC
5750   W=I
5760   IF W=0 THEN W=INS
5770   WAVE CC+1,W
5780   SOUND CC+1,FN NOTEF(N),0.5,0.6
5790 END PROC
5800 ' ====================================================================== editing
5810 PROC GOROW(R)
5820   CR=FN CLAMP(R,0,PL(OD(CO))-1)
5830 END PROC
5840 PROC SETOCT(V)
5850   OCV=FN CLAMP(V,1,6)
5860 END PROC
5870 PROC SETINS(V)
5880   INS=FN CLAMP(V,4,63)
5890 END PROC
5900 ' the note of a piano key (0 if the key is none): Z S X D C V G B H N J M in the octave, Q 2 W 3 E R 5 T 6 Y 7 U I above it
5910 FUNC PIANO(K)
5920   LOCAL C$,P
5930   IF K<32 OR K>126 THEN RETURN 0
5940   C$=CHR$(K)
5950   IF C$>="A" AND C$<="Z" THEN C$=CHR$(K+32)
5960   P=INSTR("zsxdcvgbhnjm",C$)
5970   IF P>0 THEN RETURN (OCV*12)+P
5980   P=INSTR("q2w3er5t6y7ui",C$)
5990   IF P>0 THEN RETURN ((OCV+1)*12)+P
6000   RETURN 0
6010 END FUNC
6020 ' puts a note into the cursor cell (the instrument is the current one), plays it, goes down by the step
6030 PROC PUTNOTE(N)
6040   LOCAL X
6050   X=FN IX(OD(CO),CR,CC)
6060   PN(X)=N
6070   IF N>=5 AND N<=96 THEN
6080     PJ(X)=INS
6090     CALL PREVIEW(N,INS)
6100   END IF
6110   IF N=97 THEN
6120     PJ(X)=0:PV(X)=-1
6130   END IF
6140   CALL TOUCH(OD(CO),CR,CC)
6150   CALL GOROW(CR+STP)
6160 END PROC
6170 PROC TYPEDIGIT(D)
6180   LOCAL X,I
6190   X=FN IX(OD(CO),CR,CC)
6200   IF CF=1 THEN
6210     I=D*10+(PJ(X) MOD 10)
6220     IF I>63 THEN I=63
6230     PJ(X)=I
6240     CALL TOUCH(OD(CO),CR,CC)
6250     CF=2
6260   ELSE
6270     IF CF=2 THEN
6280       I=(PJ(X)\10)*10+D
6290       IF I>63 THEN I=63
6300       PJ(X)=I
6310       CALL TOUCH(OD(CO),CR,CC)
6320       CF=3
6330     END IF
6340   END IF
6350 END PROC
6360 PROC TYPEVOL(V)
6370   LOCAL X
6380   X=FN IX(OD(CO),CR,CC)
6390   PV(X)=V
6400   CALL TOUCH(OD(CO),CR,CC)
6410   CF=0
6420   CALL GOROW(CR+STP)
6430 END PROC
6440 PROC CLEARFIELD
6450   LOCAL X
6460   X=FN IX(OD(CO),CR,CC)
6470   IF CF=0 THEN
6480     PN(X)=0
6490   ELSE
6500     IF CF=3 THEN
6510       PV(X)=-1
6520     ELSE
6530       PJ(X)=0
6540     END IF
6550   END IF
6560   CALL TOUCH(OD(CO),CR,CC)
6570 END PROC
6580 ' one field to the right / left, over the channel borders
6590 PROC MOVEF(D)
6600   CF=CF+D
6610   IF CF>3 THEN
6620     IF CC<7 THEN
6630       CC=CC+1:CF=0
6640     ELSE
6650       CF=3
6660     END IF
6670   END IF
6680   IF CF<0 THEN
6690     IF CC>0 THEN
6700       CC=CC-1:CF=3
6710     ELSE
6720       CF=0
6730     END IF
6740   END IF
6750 END PROC
6760 PROC KEYIN(K)
6770   CALL KEYACT(K)
6780   CALL SYNCCTL
6790   CALL REDRAW
6800 END PROC
6810 PROC KEYACT(K)
6820   LOCAL M,N,V
6830   M=K AND 65536
6840   K=K AND 65535
6850   IF K=4097 THEN
6860     IF M<>0 THEN
6870       CC=FN CLAMP(CC-1,0,7):CF=0
6880     ELSE
6890       CALL MOVEF(-1)
6900     END IF
6910     EXIT PROC
6920   END IF
6930   IF K=4098 THEN
6940     IF M<>0 THEN
6950       CC=FN CLAMP(CC+1,0,7):CF=0
6960     ELSE
6970       CALL MOVEF(1)
6980     END IF
6990     EXIT PROC
7000   END IF
7010   IF K=4099 THEN
7020     CALL GOROW(CR-1)
7030     EXIT PROC
7040   END IF
7050   IF K=4100 THEN
7060     CALL GOROW(CR+1)
7070     EXIT PROC
7080   END IF
7090   IF K=4103 THEN
7100     CALL GOROW(CR-16)
7110     EXIT PROC
7120   END IF
7130   IF K=4104 THEN
7140     CALL GOROW(CR+16)
7150     EXIT PROC
7160   END IF
7170   IF K=4101 THEN
7180     CALL GOROW(0)
7190     EXIT PROC
7200   END IF
7210   IF K=4102 THEN
7220     CALL GOROW(99)
7230     EXIT PROC
7240   END IF
7250   IF K=4105 OR K=8 THEN
7260     CALL CLEARFIELD
7270     EXIT PROC
7280   END IF
7290   IF K=13 THEN
7300     CALL PLAYROW
7310     EXIT PROC
7320   END IF
7330   IF K=32 THEN
7340     IF PLG THEN
7350       CALL STOPPLAY
7360     ELSE
7370       CALL STARTPLAY(CO,CR)
7380     END IF
7390     EXIT PROC
7400   END IF
7410   IF K=45 THEN
7420     CALL SETOCT(OCV-1)
7430     EXIT PROC
7440   END IF
7450   IF K=43 THEN
7460     CALL SETOCT(OCV+1)
7470     EXIT PROC
7480   END IF
7490   IF K=91 THEN
7500     CALL SETINS(INS-1)
7510     EXIT PROC
7520   END IF
7530   IF K=93 THEN
7540     CALL SETINS(INS+1)
7550     EXIT PROC
7560   END IF
7570   IF CF=0 THEN
7580     IF K=49 THEN
7590       CALL PUTNOTE(97)
7600     ELSE
7610       N=FN PIANO(K)
7620       IF N>0 THEN CALL PUTNOTE(N)
7630     END IF
7640     EXIT PROC
7650   END IF
7660   IF CF=3 THEN
7670     IF K<128 THEN
7680       V=INSTR("0123456789abcdef",CHR$(K+32*(K>=65 AND K<=70)*-1))
7690       IF V>0 THEN CALL TYPEVOL(V-1)
7700     END IF
7710     EXIT PROC
7720   END IF
7730   IF K>=48 AND K<=57 THEN CALL TYPEDIGIT(K-48)
7740 END PROC
7750 ' the row under the cursor: all its notes at once
7760 PROC PLAYROW
7770   LOCAL C,X,N
7780   IF PLG THEN EXIT PROC
7790   FOR C=0 TO 7
7800     X=FN IX(OD(CO),CR,C)
7810     N=PN(X)
7820     IF N>=5 AND N<=96 AND CM(C)=0 THEN
7830       IF PJ(X)>0 THEN WAVE C+1,PJ(X) ELSE WAVE C+1,CI(C)
7840       SOUND C+1,FN NOTEF(N),0.5,0.6
7850     END IF
7860   NEXT C
7870 END PROC
7880 ' a click on a cell of the grid: V = row * COLS + column
7890 PROC CLICKIN(V)
7900   LOCAL G,C,R,W
7910   G=V\COLS
7920   C=V MOD COLS
7930   IF G<1 THEN EXIT PROC
7940   CALL GOROW(CR+G-CENTER)
7950   IF C>=3 THEN
7960     W=(C-3)\9
7970     IF W>7 THEN W=7
7980     CC=W
7990     R=(C-3)-W*9
8000     CF=0
8010     IF R=4 THEN CF=1
8020     IF R=5 THEN CF=2
8030     IF R>=7 THEN CF=3
8040   END IF
8050   CALL SYNCCTL
8060   CALL REDRAW
8070 END PROC
8080 ' ====================================================================== the order
8090 PROC SETCO(V)
8100   CO=FN CLAMP(V,0,ONUM-1)
8110   CR=FN CLAMP(CR,0,PL(OD(CO))-1)
8120 END PROC
8130 PROC NEWPATTERN
8140   IF NPAT>=32 THEN
8150     CALL SETSTATUS("There are 32 patterns already.")
8160     EXIT PROC
8170   END IF
8180   CALL CLEARPAT(NPAT)
8190   PL(NPAT)=32
8200   OD(CO)=NPAT
8210   NPAT=NPAT+1
8220   CR=0
8230   CALL GLOBALEDIT
8240 END PROC
8250 PROC CLONEPATTERN
8260   LOCAL R,C,X,Y,P
8270   IF NPAT>=32 THEN
8280     CALL SETSTATUS("There are 32 patterns already.")
8290     EXIT PROC
8300   END IF
8310   P=OD(CO)
8320   FOR R=0 TO 63
8330     FOR C=0 TO 7
8340       X=FN IX(P,R,C):Y=FN IX(NPAT,R,C)
8350       PN(Y)=PN(X):PJ(Y)=PJ(X):PV(Y)=PV(X)
8360     NEXT C
8370   NEXT R
8380   PL(NPAT)=PL(P)
8390   OD(CO)=NPAT
8400   NPAT=NPAT+1
8410   CALL GLOBALEDIT
8420 END PROC
8430 PROC INSSLOT
8440   LOCAL I
8450   IF ONUM>=64 THEN EXIT PROC
8460   FOR I=ONUM TO CO+2 STEP -1
8470     OD(I)=OD(I-1)
8480   NEXT I
8490   OD(CO+1)=OD(CO)
8500   ONUM=ONUM+1
8510   CO=CO+1
8520   CALL GLOBALEDIT
8530 END PROC
8540 PROC DELSLOT
8550   LOCAL I
8560   IF ONUM<=1 THEN EXIT PROC
8570   FOR I=CO TO ONUM-2
8580     OD(I)=OD(I+1)
8590   NEXT I
8600   ONUM=ONUM-1
8610   CALL SETCO(CO)
8620   CALL GLOBALEDIT
8630 END PROC
8640 PROC CYCLELEN
8650   LOCAL P,L
8660   P=OD(CO)
8670   L=PL(P)+16
8680   IF L>64 THEN L=16
8690   PL(P)=L
8700   CALL GOROW(CR)
8710   CALL GLOBALEDIT
8720 END PROC
8730 ' ====================================================================== files
8740 ' the song as text: TRACKER 1, BPM, CHANNEL lines, ORDER, the instruments 32..63 that the song uses, PATTERN lines and one C line (row channel note instrument volume) per used cell
8750 PROC SAVEF(F$)
8760   LOCAL P,R,C,X,I,L$
8770   OPEN F$ FOR OUTPUT AS #1
8780   PRINT #1,"TRACKER 1"
8790   PRINT #1,"BPM "+STR$(BPM)
8800   FOR C=0 TO 7
8810     PRINT #1,"CHANNEL "+STR$(C)+" "+STR$(CI(C))+" "+STR$(CP(C))+" "+STR$(CM(C)<>0)
8820   NEXT C
8830   L$="ORDER"
8840   FOR I=0 TO ONUM-1
8850     L$=L$+" "+STR$(OD(I))
8860   NEXT I
8870   PRINT #1,L$
8880   FOR I=32 TO 63
8890     IF FN USES(I) THEN PRINT #1,"INSTRUMENT "+STR$(I)+" "+INSTRUMENT$(I)
8900   NEXT I
8910   FOR P=0 TO NPAT-1
8920     PRINT #1,"PATTERN "+STR$(P)+" "+STR$(PL(P))
8930     FOR R=0 TO 63
8940       FOR C=0 TO 7
8950         X=FN IX(P,R,C)
8960         IF PN(X)<>0 OR PJ(X)<>0 OR PV(X)>=0 THEN PRINT #1,"C "+STR$(R)+" "+STR$(C)+" "+STR$(PN(X))+" "+STR$(PJ(X))+" "+STR$(PV(X))
8970       NEXT C
8980     NEXT R
8990   NEXT P
9000   PRINT #1,"END"
9010   CLOSE #1
9020   CALL SETSTATUS("Saved "+F$+".")
9030 END PROC
9040 PROC OPENF(F$)
9050   LOCAL L$,W$,P,N,C,X,A,B,D$,K
9060   OPEN F$ FOR INPUT AS #1
9070   CALL NEWSONG
9080   ONUM=0:NPAT=0
9090   P=0
9100   WHILE EOF(1)=0
9110     LINE INPUT #1,L$
9120     W$=FN TOKN$(L$,0)
9130     IF W$="BPM" THEN BPM=FN CLAMP(INT(VAL(FN TOKN$(L$,1))),30,300)
9140     IF W$="CHANNEL" THEN
9150       C=FN CLAMP(INT(VAL(FN TOKN$(L$,1))),0,7)
9160       CI(C)=FN CLAMP(INT(VAL(FN TOKN$(L$,2))),4,63)
9170       CP(C)=FN CLAMP(INT(VAL(FN TOKN$(L$,3))),-100,100)
9180       CM(C)=0
9190       IF VAL(FN TOKN$(L$,4))<>0 THEN CM(C)=-1
9200     END IF
9210     IF W$="ORDER" THEN
9220       ONUM=0
9230       FOR K=1 TO 64
9240         D$=FN TOKN$(L$,K)
9250         IF D$<>"" THEN
9260           OD(ONUM)=FN CLAMP(INT(VAL(D$)),0,31)
9270           ONUM=ONUM+1
9280         END IF
9290       NEXT K
9300     END IF
9310     IF W$="INSTRUMENT" THEN
9320       N=INT(VAL(FN TOKN$(L$,1)))
9330       IF N>=4 AND N<=63 THEN
9340         D$=MID$(L$,INSTR(INSTR(L$," ")+1,L$," ")+1)
9350         INSTRUMENT N,D$
9360       END IF
9370     END IF
9380     IF W$="PATTERN" THEN
9390       P=FN CLAMP(INT(VAL(FN TOKN$(L$,1))),0,31)
9400       PL(P)=FN CLAMP(INT(VAL(FN TOKN$(L$,2))),16,64)
9410       IF P+1>NPAT THEN NPAT=P+1
9420     END IF
9430     IF W$="C" THEN
9440       A=INT(VAL(FN TOKN$(L$,1))):C=INT(VAL(FN TOKN$(L$,2)))
9450       IF A>=0 AND A<64 AND C>=0 AND C<8 THEN
9460         X=FN IX(P,A,C)
9470         PN(X)=FN CLAMP(INT(VAL(FN TOKN$(L$,3))),0,97)
9480         PJ(X)=FN CLAMP(INT(VAL(FN TOKN$(L$,4))),0,63)
9490         PV(X)=FN CLAMP(INT(VAL(FN TOKN$(L$,5))),-1,15)
9500       END IF
9510     END IF
9520   WEND
9530   CLOSE #1
9540   IF ONUM=0 THEN
9550     ONUM=1:OD(0)=0
9560   END IF
9570   IF NPAT=0 THEN NPAT=1
9580   FOR K=0 TO ONUM-1
9590     IF OD(K)>=NPAT THEN OD(K)=0
9600   NEXT K
9610   CO=0:CR=0:CC=0:CF=0
9620   CALL TOCONTROLS
9630   CALL SYNCCTL
9640   CALL REDRAW
9650   CALL SETSTATUS("Opened "+F$+".")
9660 END PROC
9670 ' the PLAY lines of the whole song, one per call: CALLBACK-less, written to the file #1 or collected into the global list XL$ (for Copy)
9680 PROC EXPORTLINES(TOCLIP)
9690   LOCAL C,I,N,L,K,A$,B$,P,XN
9700   CALL BUILDMAP
9710   XL$=""
9720   XN=10
9730   XL$=XL$+STR$(XN)+" ' song made with the tracker of Engine1999 ("+STR$(BPM)+" beats a minute)"+CHR$(10)
9740   XN=XN+10
9750   FOR I=32 TO 63
9760     IF FN USES(I) THEN
9770       XL$=XL$+STR$(XN)+" INSTRUMENT "+STR$(I)+","+CHR$(34)+INSTRUMENT$(I)+CHR$(34)+CHR$(10)
9780       XN=XN+10
9790     END IF
9800   NEXT I
9810   FOR C=0 TO 7
9820     A$=FN CHSTR$(C)
9830     IF CM(C)<>0 THEN A$=""
9840     K=0
9850     WHILE LEN(A$)>0
9860       L=150
9870       IF LEN(A$)<=L THEN
9880         L=LEN(A$)
9890       ELSE
9900         WHILE L>1 AND MID$(A$,L+1,1)<>" "
9910           L=L-1
9920         WEND
9930       END IF
9940       B$=LEFT$(A$,L)
9950       A$=MID$(A$,L+1)
9960       IF K=0 THEN
9970         XL$=XL$+STR$(XN)+" V"+STR$(C+1)+"$="+CHR$(34)+B$+CHR$(34)+CHR$(10)
9980       ELSE
9990         XL$=XL$+STR$(XN)+" V"+STR$(C+1)+"$=V"+STR$(C+1)+"$+"+CHR$(34)+B$+CHR$(34)+CHR$(10)
10000       END IF
10010       XN=XN+10
10020       K=K+1
10030     WEND
10040     IF K=0 THEN
10050       XL$=XL$+STR$(XN)+" V"+STR$(C+1)+"$="+CHR$(34)+CHR$(34)+CHR$(10)
10060       XN=XN+10
10070     END IF
10080   NEXT C
10090   XL$=XL$+STR$(XN)+" PLAY V1$,V2$,V3$,V4$,V5$,V6$,V7$,V8$"+CHR$(10)
10100 END PROC
10110 PROC EXPORTF(F$)
10120   CALL EXPORTLINES(0)
10130   OPEN F$ FOR OUTPUT AS #1
10140   PRINT #1,XL$;
10150   CLOSE #1
10160   CALL SETSTATUS("Exported "+F$+" (MERGE or LOAD it, RUN plays it).")
10170 END PROC
10180 ' ====================================================================== the form
10190 PROC MAKEFORM
10200   UI PANEL 1,0,0,640,300,"Tracker"
10210   UI BUTTON 10,10,22,50,20,"Song"
10220   UI BUTTON 11,64,22,66,20,"Pattern"
10230   UI BUTTON 12,134,22,46,20,"Here"
10240   UI BUTTON 13,184,22,46,20,"Stop"
10250   CALL UK_SLIDER(20,246,22,384,"Tempo",40,240,BPM)
10260   UI BUTTON 30,10,46,20,20,"-"
10270   UI LABEL 31,34,48,46,14,""
10280   UI BUTTON 32,84,46,20,20,"+"
10290   UI BUTTON 33,114,46,20,20,"<"
10300   UI LABEL 34,138,48,130,14,""
10310   UI BUTTON 35,272,46,20,20,">"
10320   UI BUTTON 37,302,46,64,20,"Step 1"
10330   UI CHECK 96,376,48,90,16,"Follow"
10340   UI VALUE 96,-1
10350   UI BUTTON 75,570,46,60,20,"Quit"
10360   UI GRID 50,10,70,COLS,GRH
10370   UI TAB 80,10,234,300,22,"Order|Channel|File"
10380   UI BUTTON 40,10,260,20,20,"<"
10390   UI LABEL 41,34,262,230,14,""
10400   UI BUTTON 42,268,260,20,20,">"
10410   UI BUTTON 43,296,260,48,20,"Pat<"
10420   UI BUTTON 44,348,260,48,20,"Pat>"
10430   UI BUTTON 45,400,260,44,20,"New"
10440   UI BUTTON 46,448,260,52,20,"Clone"
10450   UI BUTTON 47,504,260,40,20,"Ins"
10460   UI BUTTON 48,548,260,40,20,"Del"
10470   UI BUTTON 49,592,260,38,20,"Len"
10480   UI CHECK 94,10,262,80,16,"Mute"
10490   CALL UK_SLIDER(91,96,262,300,"Pan",-100,100,0)
10500   UI BUTTON 36,404,260,70,20,"Ch = Ins"
10510   UI LABEL 95,480,262,150,14,""
10520   UI BUTTON 70,10,260,60,20,"Save"
10530   UI BUTTON 71,74,260,60,20,"Open"
10540   UI BUTTON 72,138,260,66,20,"Export"
10550   UI BUTTON 73,208,260,50,20,"Copy"
10560   UI BUTTON 74,262,260,54,20,"Clear"
10570   UI LABEL 60,10,284,620,14,""
10580   CALL SHOWPAGE(0)
10590   UI FOCUS 50
10600 END PROC
10610 ' the tab pane under the grid: page 0 Order (slots and patterns), 1 the channel (Mute, Pan, start instrument), 2 files
10620 PROC SHOWPAGE(N)
10630   LOCAL I
10640   FOR I=40 TO 49
10650     UI SHOW I,(N=0)
10660   NEXT I
10670   FOR I=91 TO 95
10680     UI SHOW I,(N=1)
10690   NEXT I
10700   UI SHOW 36,(N=1)
10710   FOR I=70 TO 74
10720     UI SHOW I,(N=2)
10730   NEXT I
10740 END PROC
10750 ' the controls that show the song (after Open / Clear)
10760 PROC TOCONTROLS
10770   IF HEADLESS THEN EXIT PROC
10780   CALL UK_SET(20,BPM)
10790 END PROC
10800 ' ====================================================================== what happens
10810 PROC REACT(E,K,V)
10820   LOCAL F$
10830   IF E=50 AND K=9 THEN CALL KEYIN(V)
10840   IF E=50 AND K=10 THEN CALL CLICKIN(V)
10850   IF FN UK_TRACK(E,V) THEN
10860     IF E=20 THEN
10870       BPM=V
10880       IF PLG AND BPM<>PBPM THEN RSA=-1
10890     END IF
10900     IF E=91 THEN
10910       CP(CC)=V
10920       IF PLG THEN RS(CC)=-1
10930       CALL REDRAW
10940     END IF
10950   END IF
10960   IF E=94 AND K=2 THEN
10970     CM(CC)=V
10980     IF V<>0 THEN CM(CC)=-1
10990     IF PLG THEN
11000       IF CM(CC)<>0 THEN
11010         SOUND OFF CC+1
11020       ELSE
11030         RS(CC)=-1
11040       END IF
11050     END IF
11060     CALL REDRAW
11070     UI FOCUS 50
11080   END IF
11090   IF E=80 AND K=2 THEN
11100     CALL SHOWPAGE(V)
11110     UI FOCUS 50
11120   END IF
11130   IF E=96 AND K=2 THEN
11140     FOLLOW=V<>0
11150     UI FOCUS 50
11160   END IF
11170   IF K=1 THEN
11180     IF E=10 THEN CALL STARTPLAY(0,0)
11190     IF E=11 THEN CALL STARTPLAY(CO,0)
11200     IF E=12 THEN CALL STARTPLAY(CO,CR)
11210     IF E=13 THEN CALL STOPPLAY
11220     IF E=30 THEN CALL SETOCT(OCV-1)
11230     IF E=32 THEN CALL SETOCT(OCV+1)
11240     IF E=33 THEN CALL SETINS(INS-1)
11250     IF E=35 THEN CALL SETINS(INS+1)
11260     IF E=36 THEN
11270       CI(CC)=INS
11280       IF PLG THEN RS(CC)=-1
11290       CALL SETSTATUS("Channel "+STR$(CC+1)+" starts with instrument "+STR$(INS)+".")
11300     END IF
11310     IF E=37 THEN
11320       STP=STP+1
11330       IF STP>4 THEN STP=0
11340     END IF
11350     IF E=40 THEN CALL SETCO(CO-1)
11360     IF E=42 THEN CALL SETCO(CO+1)
11370     IF E=43 THEN
11380       OD(CO)=FN CLAMP(OD(CO)-1,0,NPAT-1)
11390       CALL GOROW(CR)
11400       CALL GLOBALEDIT
11410     END IF
11420     IF E=44 THEN
11430       OD(CO)=FN CLAMP(OD(CO)+1,0,NPAT-1)
11440       CALL GOROW(CR)
11450       CALL GLOBALEDIT
11460     END IF
11470     IF E=45 THEN CALL NEWPATTERN
11480     IF E=46 THEN CALL CLONEPATTERN
11490     IF E=47 THEN CALL INSSLOT
11500     IF E=48 THEN CALL DELSLOT
11510     IF E=49 THEN CALL CYCLELEN
11520     IF E=70 THEN
11530       F$=FN FR_PICK$("Save the song as",DR$,"*.trk;*.txt",-1)
11540       DR$=FR_DIR$
11550       IF F$<>"" THEN CALL SAVEF(F$)
11560     END IF
11570     IF E=71 THEN
11580       F$=FN FR_PICK$("Open a song",DR$,"*.trk;*.txt",0)
11590       DR$=FR_DIR$
11600       IF F$<>"" THEN
11610         CALL STOPPLAY
11620         CALL OPENF(F$)
11630       END IF
11640     END IF
11650     IF E=72 THEN
11660       F$=FN FR_PICK$("Export as BASIC lines",DR$,"*.bas",-1)
11670       DR$=FR_DIR$
11680       IF F$<>"" THEN CALL EXPORTF(F$)
11690     END IF
11700     IF E=73 THEN
11710       CALL EXPORTLINES(-1)
11720       CLIPBOARD XL$
11730       CALL SETSTATUS("The PLAY lines are on the clipboard.")
11740     END IF
11750     IF E=74 THEN
11760       IF FN UK_ASK("Clear","Throw the song away and start a new one?") THEN
11770         CALL STOPPLAY
11780         CALL NEWSONG
11790         CALL TOCONTROLS
11800       END IF
11810     END IF
11820     IF E=75 THEN Q=1
11830     CALL SYNCCTL
11840     CALL REDRAW
11850     UI FOCUS 50
11860   END IF
11870 END PROC
11880 ' ====================================================================== the self-test (TRACKER_HEADLESS=1 in the console host: tests the song / string code, no window)
11890 PROC CHECK(OK,T$)
11900   IF OK=0 THEN
11910     PRINT "FAIL: ";T$
11920     BAD=BAD+1
11930   END IF
11940   NCHK=NCHK+1
11950 END PROC
11960 PROC SELFTEST
11970   LOCAL X,Y,A$,B$,I
11980   HEADLESS=-1
11990   BAD=0:NCHK=0
12000   CALL NEWSONG
12010   CALL CHECK(FN NOTE$(49)="C-4" AND FN NOTE$(40)="D#3" AND FN NOTE$(0)="---" AND FN NOTE$(97)="===" AND FN NOTE$(96)="B-7","note names")
12020   OCV=3
12030   CALL CHECK(FN PIANO(ASC("z"))=37 AND FN PIANO(ASC("m"))=48 AND FN PIANO(ASC("Z"))=37 AND FN PIANO(ASC("q"))=49 AND FN PIANO(ASC("i"))=61 AND FN PIANO(ASC("a"))=0,"piano keys")
12040   CALL CHECK(FN CELL$(0,0,0)="--- .. ." AND LEN(FN CELL$(0,0,0))=8,"an empty cell")
12050   X=FN IX(0,0,0)
12060   PN(X)=49:PJ(X)=4:PV(X)=15
12070   CALL CHECK(FN CELL$(0,0,0)="C-4 04 F","a filled cell")
12080   PN(FN IX(0,4,0))=97
12090   PN(FN IX(0,8,0))=40
12100   BPM=120:CP(0)=0:CI(0)=4
12110   CALL BUILDMAP
12120   CALL CHECK(FT=32,"the song has 32 rows")
12130   A$=FN CHSTR$(0)
12140   CALL CHECK(A$="T120 MN Y0 I4 V0.8 V1 O4 MN C4 R4 O3 ML D#1 MN D#2","the PLAY string of a channel: "+A$)
12150   CALL CHECK(FN CHSTR$(1)="T120 MN Y0.3 I5 V0.8 R1 R1","an empty channel is its header and rests: "+FN CHSTR$(1))
12160   ' the music string must be accepted by PLAY (it checks its strings), also when the notes use other instruments and volumes
12170   PN(FN IX(0,12,1))=60:PJ(FN IX(0,12,1))=33:PV(FN IX(0,12,1))=7
12180   PN(FN IX(0,13,1))=61
12190   PN(FN IX(0,15,1))=5
12200   CALL BUILDMAP
12210   B$=FN CHSTR$(1)
12220   CALL CHECK(B$="T120 MN Y0.3 I5 V0.8 R2. I33 V0.467 O4 MN B16 O5 MN C8 O0 ML E1 MN E16","a second channel: "+B$)
12230   PLAY A$,B$
12240   ' from a later row: the first notes are skipped, a note that started earlier is not played (its row is not in the range)
12250   A$=FN STRFROM$(0,8)
12260   CALL CHECK(A$="T120 MN Y0 I4 V1 O3 ML D#1 MN D#2","playing from row 8 (instrument and volume as they were before): "+A$)
12270   A$=FN STRFROM$(0,10)
12280   CALL CHECK(A$="T120 MN Y0 I4 V1 O3 ML D#1 MN D#4.","a note that is still sounding is carried on: "+A$)
12290   A$=FN STRFROM$(0,5)
12300   CALL CHECK(A$="T120 MN Y0 I4 V1 R8. O3 ML D#1 MN D#2","after a note off there is a rest: "+A$)
12310   PLAY A$
12320   ' the song is queued in pieces: the pieces together are the whole string (with the settings restated at the start of each piece)
12330   CAP=-1:CAPS$(0)="":PBPM=BPM:AC$=""
12340   CALL STARTCH(0,0)
12350   CALL FILLCH(0,6)
12360   CALL SENDCH(0)
12370   CALL CHECK(QC(0)=8,"the first piece ends at the next note: "+STR$(QC(0)))
12380   CALL FILLCH(0,20)
12390   CALL SENDCH(0)
12400   CALL CHECK(QC(0)=32 AND CAPS$(0)="T120 MN Y0 I4 V0.8 V1 O4 MN C4 R4 T120 V1 O3 ML D#1 MN D#2","the pieces: "+CAPS$(0))
12410   CAP=0
12420   ' a change in the song while it plays is queued anew when it lies in what is queued already
12430   PLG=-1:PRL=0
12440   FOR I=0 TO 7
12450     RS(I)=0
12460   NEXT I
12470   QC(0)=8
12480   CALL TOUCH(0,6,0)
12490   CALL CHECK(RS(0)<>0,"a change inside what is queued")
12500   RS(0)=0
12510   CALL TOUCH(0,20,0)
12520   CALL TOUCH(0,0,0)
12530   CALL TOUCH(0,8,0)
12540   CALL CHECK(RS(0)=0,"a change behind it, before the playhead, or a replaced note at its end")
12550   PN(FN IX(0,8,0))=0
12560   CALL TOUCH(0,8,0)
12570   CALL CHECK(RS(0)<>0,"a note that ends the queued piece is deleted")
12580   PN(FN IX(0,8,0))=40
12590   RS(0)=0
12600   CALL GLOBALEDIT
12610   CALL CHECK(RSA<>0,"an edit of the order")
12620   RSA=0:PLG=0
12630   ' two slots: the second pattern follows the first
12640   CALL NEWSONG
12650   CALL INSSLOT
12660   CALL NEWPATTERN
12670   CALL CHECK(ONUM=2 AND OD(0)=0 AND OD(1)=1 AND NPAT=2,"a new slot and pattern")
12680   PN(FN IX(1,0,2))=50
12690   CALL BUILDMAP
12700   CALL CHECK(FT=64 AND FLO(32)=1 AND FLR(32)=0 AND PN(FN CX(32,2))=50 AND FOFF(1)=32,"the order is flattened")
12710   A$=FN CHSTR$(2)
12720   CALL CHECK(LEFT$(A$,20)="T125 MN Y-0.6 I6 R1" OR LEFT$(A$,12)="T125 MN Y-0.","pan in the header: "+A$)
12730   ' the cursor and the editing
12740   CALL NEWSONG
12750   CR=0:CC=0:CF=0
12760   OCV=4:INS=5:STP=2
12770   CALL KEYACT(ASC("z"))
12780   CALL CHECK(PN(FN IX(0,0,0))=49 AND PJ(FN IX(0,0,0))=5 AND CR=2,"a piano key writes the note with the instrument and steps")
12790   CALL KEYACT(49)
12800   CALL CHECK(PN(FN IX(0,2,0))=97 AND CR=4,"1 puts a note off")
12810   CALL KEYACT(4098)
12820   CALL KEYACT(51)
12830   CALL KEYACT(55)
12840   CALL CHECK(CF=3 AND PJ(FN IX(0,4,0))=37 AND CR=4,"two digits are the instrument")
12850   CALL KEYACT(ASC("C"))
12860   CALL CHECK(PV(FN IX(0,4,0))=12 AND CF=0 AND CR=6,"a hex digit is the volume (upper or lower case)")
12870   CALL KEYACT(4097)
12880   CALL CHECK(CC=0 AND CF=0,"Left at the first field stays")
12890   CALL KEYACT(4098+65536)
12900   CALL CHECK(CC=1 AND CF=0,"Shift + Right jumps a channel")
12910   CALL KEYACT(4105)
12920   CALL KEYACT(4103)
12930   CALL CHECK(CR=0,"PageUp stops at the first row")
12940   CALL KEYACT(4102)
12950   CALL CHECK(CR=31,"End goes to the last row")
12960   ' save and open
12970   CALL NEWSONG
12980   BPM=99:CP(3)=-77:CM(5)=-1:CI(2)=33
12990   INSTRUMENT 33,"NAME TESTINS; OSC SAW 0.5; GAIN 1.1"
13000   CALL INSSLOT
13010   CALL NEWPATTERN
13020   PL(1)=48
13030   PN(FN IX(1,5,6))=60:PJ(FN IX(1,5,6))=33:PV(FN IX(1,5,6))=3
13040   PN(FN IX(0,31,7))=97
13050   CALL SAVEF("mem:song1")
13060   CALL NEWSONG
13070   INSTRUMENT 33
13080   CALL OPENF("mem:song1")
13090   CALL CHECK(BPM=99 AND CP(3)=-77 AND CM(5)<>0 AND CM(4)=0 AND CI(2)=33 AND ONUM=2 AND OD(1)=1 AND NPAT=2 AND PL(1)=48 AND PL(0)=32,"song header and order survive a save and open")
13100   CALL CHECK(PN(FN IX(1,5,6))=60 AND PJ(FN IX(1,5,6))=33 AND PV(FN IX(1,5,6))=3 AND PN(FN IX(0,31,7))=97 AND PN(FN IX(0,0,0))=0 AND PV(FN IX(0,0,0))=-1,"cells survive a save and open")
13110   CALL CHECK(FN INAME$(33)="TESTINS","the instrument of the song is defined again by Open: "+FN INAME$(33))
13120   ' export
13130   CALL EXPORTLINES(0)
13140   CALL CHECK(INSTR(XL$,"INSTRUMENT 33,")>0 AND INSTR(XL$,"PLAY V1$,V2$,V3$,V4$,V5$,V6$,V7$,V8$")>0 AND INSTR(XL$,"V1$=")>0,"the exported lines")
13150   ' TRACKER_HEADLESS=file: also opens that song and plays its strings (PLAY checks them)
13160   IF ENV$("TRACKER_HEADLESS")<>"1" THEN
13170     CALL OPENF(ENV$("TRACKER_HEADLESS"))
13180     CALL BUILDMAP
13190     FOR I=0 TO 7
13200       S$(I)=FN CHSTR$(I)
13210     NEXT I
13220     PLAY S$(0),S$(1),S$(2),S$(3),S$(4),S$(5),S$(6),S$(7)
13230     PRINT "song ";ENV$("TRACKER_HEADLESS");": ";ONUM;" slots, ";NPAT;" patterns, ";FT;" rows, strings of ";LEN(S$(0));" ";LEN(S$(1));" ";LEN(S$(2));" ";LEN(S$(3));" ";LEN(S$(4));" ";LEN(S$(5));" ";LEN(S$(6));" ";LEN(S$(7));" characters"
13240   END IF
13250   PRINT "tracker selftest ";
13260   IF BAD=0 THEN PRINT "ok ";NCHK ELSE PRINT BAD;" FAILED of ";NCHK
13270 END PROC
13280 ' ====================================================================== main program
13290 IF ENV$("TRACKER_HEADLESS")<>"" THEN
13300   CALL SELFTEST
13310   END
13320 END IF
13330 UI OPEN
13340 CALL SETPAL
13350 FOR I=0 TO 7
13360   CH2=21+I
13370   OPEN "play:"+STR$(I+1) FOR OUTPUT AS #CH2
13380 NEXT I
13390 CALL NEWSONG
13400 CALL MAKEFORM
13410 CALL SYNCCTL
13420 CALL REDRAW
13430 CALL SETSTATUS("Piano keys Z S X D C V G B H N J M write notes, Space plays, Tab leaves the grid.")
13440 ON ERROR GOTO ERRH
13450 ARGLOAD$=""
13460 IF ARGC>=1 THEN ARGLOAD$=ARG$(1)
13470 IF ARGLOAD$<>"" THEN CALL OPENF(ARGLOAD$)
13480 LABEL LOOPTOP
13490 ON ERROR GOTO ERRH
13500 ON UI GOSUB HANDLER
13510 REPEAT
13520   GFX VSYNC
13530   UI UPDATE
13540   CALL PLAYTICK
13550 UNTIL Q=1
13560 ON UI OFF
13570 ON ERROR OFF
13580 SOUND OFF
13590 FOR I=0 TO 7
13600   CH2=21+I
13610   CLOSE #CH2
13620 NEXT I
13630 CALL RESTOREPAL
13640 UI CLOSE
13650 REM no GFX RESET here
13660 CLS
13670 PRINT "Tracker finished."
13680 END
13690 LABEL ERRH
13700 ON UI OFF
13710 CALL UK_MSG("Error",ERR$+"|(line "+STR$(ERL)+")")
13720 RESUME LOOPTOP
13730 LABEL HANDLER
13740 E=UI("EVENT")
13750 WHILE E<>0
13760   EK=UI("KIND")
13770   EV=UI("VALUE")
13780   CALL REACT(E,EK,EV)
13790   E=UI("EVENT")
13800 WEND
13810 RETURN
