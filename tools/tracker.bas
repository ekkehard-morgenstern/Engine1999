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
230 ' widget ids: 1 form, 10 Song, 11 Pattern, 12 Here, 13 Stop, 20 tempo slider (+1 label, +2 readout), 30 oct -, 31 oct label, 32 oct +, 33 inst <, 34 inst label, 35 inst >,
240 ' 36 Ch = Ins, 37 step button, 94 Mute, 91 Pan slider (+1, +2), 40 order <, 41 order label, 42 order >, 43 Pat<, 44 Pat>, 45 New, 46 Clone, 47 Ins, 48 Del, 49 Len,
250 ' 50 the grid, 96 Follow, 60 status, 70 Save, 71 Open, 72 Export, 73 Copy, 74 Clear, 75 Quit
260 LOAD LIBRARY "uikit"
270 LOAD LIBRARY "filereq"
280 REM the graphics of the calling program stay (no GFX RESET)
290 CLS
300 BREAK OFF
310 Q=0
320 HEADLESS=0
330 COLS=74
340 GRH=12
350 CENTER=4
360 DIM PN(16384),PJ(16384),PV(16384),PL(32),OD(64),CI(8),CP(8),CM(8),S$(8)
370 DIM FLO(4096),FLR(4096),FOFF(64),QC(8),RS(8),XI(8),XV(8),XO(8),CAPS$(8),SPL(16)
380 DIM NT$(12),NP$(12),LV(8),LS$(8),FO(4),FL(4)
390 FOR I=0 TO 11
400   READ NT$(I),NP$(I)
410 NEXT I
420 DATA "C-","C","C#","C#","D-","D","D#","D#","E-","E","F-","F","F#","F#","G-","G","G#","G#","A-","A","A#","A#","B-","B"
430 FOR I=0 TO 7
440   READ LV(I),LS$(I)
450 NEXT I
460 DATA 16,"1",12,"2.",8,"2",6,"4.",4,"4",3,"8.",2,"8",1,"16"
470 FO(0)=0:FO(1)=4:FO(2)=5:FO(3)=7
480 FL(0)=3:FL(1)=1:FL(2)=1:FL(3)=1
490 DR$=""
500 ' the song: ONUM slots of the order, NPAT patterns, BPM; the cursor: CO slot, CR row, CC channel, CF field (0 note, 1 / 2 the instrument digits, 3 volume)
510 BPM=125
520 ONUM=1
530 NPAT=1
540 CO=0
550 CR=0
560 CC=0
570 CF=0
580 OCV=3
590 INS=4
600 STP=1
610 PLG=0
620 PCO=0
630 PHROW=0
640 PT0=0
650 FT=0
660 AC$=""
670 CURI=0
680 CURV=12
690 CURO=-1
700 PN2=0
710 PS=0
720 PI2=0
730 PV2=-1
740 PS0=0
750 PRL=0
760 PBPM=125
770 RSA=0
780 FOLLOW=-1
790 LOOK=8
800 CAP=0
810 BEI=0
820 BEV=-1
830 PCO=0
840 PHROW=0
850 ' ====================================================================== functions (they must stand before the lines that use them)
860 FUNC NUM$(V)
870   RETURN STR$(INT(V*1000+0.5)/1000)
880 END FUNC
890 FUNC IX(P,R,C)
900   RETURN (P*64+R)*8+C
910 END FUNC
920 FUNC CLAMP(V,LO,HI)
930   IF V<LO THEN RETURN LO
940   IF V>HI THEN RETURN HI
950   RETURN V
960 END FUNC
970 FUNC NOTE$(N)
980   IF N=0 THEN RETURN "---"
990   IF N=97 THEN RETURN "==="
1000   RETURN NT$((N-1) MOD 12)+STR$((N-1)\12)
1010 END FUNC
1020 FUNC CELL$(P,R,C)
1030   LOCAL X,N,I,V,A$
1040   X=FN IX(P,R,C)
1050   N=PN(X):I=PJ(X):V=PV(X)
1060   A$=FN NOTE$(N)+" "
1070   IF I>0 THEN A$=A$+RIGHT$("0"+STR$(I),2) ELSE A$=A$+".."
1080   IF V>=0 THEN A$=A$+" "+MID$("0123456789ABCDEF",V+1,1) ELSE A$=A$+" ."
1090   RETURN A$
1100 END FUNC
1110 ' the name of instrument N (from its definition string: NAME xxx; ...)
1120 FUNC INAME$(N)
1130   LOCAL D$,E
1140   D$=INSTRUMENT$(N)
1150   IF LEFT$(D$,5)<>"NAME " THEN RETURN "?"
1160   E=INSTR(D$,";")
1170   IF E=0 THEN E=LEN(D$)+1
1180   RETURN MID$(D$,6,E-6)
1190 END FUNC
1200 ' the I-th (0..) word of S$ (blanks), "" if none
1210 FUNC TOKN$(S$,N)
1220   LOCAL I,K,C$,T$,INW
1230   K=0:T$="":INW=0
1240   FOR I=1 TO LEN(S$)+1
1250     IF I<=LEN(S$) THEN C$=MID$(S$,I,1) ELSE C$=" "
1260     IF C$=" " OR C$=CHR$(9) THEN
1270       IF INW THEN
1280         IF K=N THEN RETURN T$
1290         K=K+1:T$="":INW=0
1300       END IF
1310     ELSE
1320       T$=T$+C$:INW=-1
1330     END IF
1340   NEXT I
1350   RETURN ""
1360 END FUNC
1370 ' is instrument I (32..63) used by a channel or a cell of the song?
1380 FUNC USES(I)
1390   LOCAL P,R,C,X
1400   FOR C=0 TO 7
1410     IF CI(C)=I THEN RETURN -1
1420   NEXT C
1430   FOR P=0 TO NPAT-1
1440     FOR R=0 TO 63
1450       FOR C=0 TO 7
1460         IF PJ(FN IX(P,R,C))=I THEN RETURN -1
1470       NEXT C
1480     NEXT R
1490   NEXT P
1500   RETURN 0
1510 END FUNC
1520 ' the index of the cell of channel C in flat row T of the song (BUILDMAP made FLO / FLR: the slot and the row of each flat row)
1530 FUNC CX(T,C)
1540   RETURN FN IX(OD(FLO(T)),FLR(T),C)
1550 END FUNC
1560 ' the first flat row after T where channel C has a note field (a note or a note off), FT if there is none
1570 FUNC NEXTEV(C,T)
1580   LOCAL U
1590   U=T+1
1600   WHILE U<FT
1610     IF PN(FN CX(U,C))<>0 THEN RETURN U
1620     U=U+1
1630   WEND
1640   RETURN FT
1650 END FUNC
1660 ' the flat row of the note that still sounds before flat row T on channel C (the last note field is a note, not a note off), -1 if none
1670 FUNC LASTON(C,T)
1680   LOCAL U,N
1690   U=T-1
1700   WHILE U>=0
1710     N=PN(FN CX(U,C))
1720     IF N<>0 THEN
1730       IF N>=5 AND N<=96 THEN RETURN U
1740       RETURN -1
1750     END IF
1760     U=U-1
1770   WEND
1780   RETURN -1
1790 END FUNC
1800 ' ====================================================================== the song
1810 PROC CLEARPAT(P)
1820   LOCAL R,C,X
1830   FOR R=0 TO 63
1840     FOR C=0 TO 7
1850       X=FN IX(P,R,C)
1860       PN(X)=0:PJ(X)=0:PV(X)=-1
1870     NEXT C
1880   NEXT R
1890 END PROC
1900 PROC NEWSONG
1910   LOCAL P,C
1920   FOR P=0 TO 31
1930     CALL CLEARPAT(P)
1940     PL(P)=32
1950   NEXT P
1960   FOR P=0 TO 63
1970     OD(P)=0
1980   NEXT P
1990   ONUM=1:NPAT=1:CO=0:CR=0:CC=0:CF=0
2000   BPM=125
2010   CI(0)=4:CI(1)=5:CI(2)=6:CI(3)=7:CI(4)=13:CI(5)=14:CI(6)=15:CI(7)=9
2020   FOR C=0 TO 7
2030     CP(C)=0:CM(C)=0
2040   NEXT C
2050   CP(0)=-30:CP(1)=30:CP(2)=-60:CP(3)=60
2060 END PROC
2070 ' ====================================================================== the music strings
2080 ' the song as one list of rows ("flat rows": the patterns of the order one after the other): FLO = the slot, FLR = the row in its pattern, FOFF = the first flat row of
2090 ' a slot, FT = the number of rows. The cells are always read from the patterns, so a change of a cell is seen at once
2100 PROC BUILDMAP
2110   LOCAL O,R,P,T
2120   T=0
2130   FOR O=0 TO ONUM-1
2140     FOFF(O)=T
2150     P=OD(O)
2160     FOR R=0 TO PL(P)-1
2170       FLO(T)=O:FLR(T)=R
2180       T=T+1
2190     NEXT R
2200   NEXT O
2210   FT=T
2220 END PROC
2230 ' K rows of rests
2240 PROC EMITREST(K)
2250   LOCAL Z,I
2260   Z=K
2270   WHILE Z>0
2280     I=0
2290     WHILE LV(I)>Z
2300       I=I+1
2310     WEND
2320     Z=Z-LV(I)
2330     AC$=AC$+" R"+LS$(I)
2340   WEND
2350 END PROC
2360 ' the open note PN2 (instrument PI2, volume PV2) for K rows: pieces joined by legato, the last one with the normal gap
2370 PROC EMITNOTE(K)
2380   LOCAL Z,I,O,S
2390   IF PI2>0 AND PI2<>CURI THEN
2400     AC$=AC$+" I"+STR$(PI2)
2410     CURI=PI2
2420   END IF
2430   IF PV2>=0 AND PV2<>CURV THEN
2440     AC$=AC$+" V"+FN NUM$(PV2/15)
2450     CURV=PV2
2460   END IF
2470   O=(PN2-1)\12
2480   S=(PN2-1) MOD 12
2490   IF O<>CURO THEN
2500     AC$=AC$+" O"+STR$(O)
2510     CURO=O
2520   END IF
2530   Z=K
2540   WHILE Z>0
2550     I=0
2560     WHILE LV(I)>Z
2570       I=I+1
2580     WEND
2590     Z=Z-LV(I)
2600     IF Z>0 THEN AC$=AC$+" ML" ELSE AC$=AC$+" MN"
2610     AC$=AC$+" "+NP$(S)+LS$(I)
2620   WEND
2630 END PROC
2640 ' the instrument BEI (0 = none) and the volume BEV (-1 = none) that are in effect before flat row T on channel C: those of the last notes that set them
2650 PROC BACKSCAN(C,T)
2660   LOCAL U,X
2670   BEI=0:BEV=-1
2680   U=T-1
2690   WHILE U>=0 AND (BEI=0 OR BEV<0)
2700     X=FN CX(U,C)
2710     IF PN(X)>=5 AND PN(X)<=96 THEN
2720       IF BEI=0 AND PJ(X)>0 THEN BEI=PJ(X)
2730       IF BEV<0 AND PV(X)>=0 THEN BEV=PV(X)
2740     END IF
2750     U=U-1
2760   WEND
2770 END PROC
2780 ' the PLAY string of channel C is made in pieces as the song plays (the playhead needs only a few rows ahead, so a change is heard soon): STARTCH writes the
2790 ' header (tempo, pan, instrument, volume as they are in effect before flat row T) and a note that is still sounding there; FILLCH adds the notes and rests from
2800 ' QC(C) on up to the flat row TG; SENDCH hands the piece to the voice (or collects it when CAP is set: the tests and Export)
2810 PROC STARTCH(C,T)
2820   LOCAL L,E
2830   CALL BACKSCAN(C,T)
2840   XI(C)=CI(C)
2850   IF BEI>0 THEN XI(C)=BEI
2860   XV(C)=12
2870   IF BEV>=0 THEN XV(C)=BEV
2880   XO(C)=-1
2890   AC$="T"+STR$(PBPM)+" MN Y"+FN NUM$(CP(C)/100)+" I"+STR$(XI(C))+" V"+FN NUM$(XV(C)/15)
2900   QC(C)=T
2910   IF PN(FN CX(T,C))=0 THEN
2920     L=FN LASTON(C,T)
2930     IF L>=0 THEN
2940       E=FN NEXTEV(C,T)
2950       PN2=PN(FN CX(L,C)):PI2=XI(C):PV2=XV(C)
2960       CURI=XI(C):CURV=XV(C):CURO=-1
2970       CALL EMITNOTE(E-T)
2980       XO(C)=CURO
2990       QC(C)=E
3000     END IF
3010   END IF
3020 END PROC
3030 PROC FILLCH(C,TG)
3040   LOCAL R,N,E,X
3050   CURI=XI(C):CURV=XV(C):CURO=XO(C)
3060   IF LEN(AC$)=0 THEN
3070     AC$="T"+STR$(PBPM)+" V"+FN NUM$(XV(C)/15)
3080     CURO=-1
3090   END IF
3100   WHILE QC(C)<FT AND QC(C)<TG
3110     R=QC(C)
3120     X=FN CX(R,C)
3130     N=PN(X)
3140     E=FN NEXTEV(C,R)
3150     IF N>=5 AND N<=96 THEN
3160       PN2=N:PI2=PJ(X):PV2=PV(X)
3170       CALL EMITNOTE(E-R)
3180     ELSE
3190       CALL EMITREST(E-R)
3200     END IF
3210     QC(C)=E
3220   WEND
3230   XI(C)=CURI:XV(C)=CURV:XO(C)=CURO
3240 END PROC
3250 PROC SENDCH(C)
3260   LOCAL CH
3270   IF LEN(AC$)>0 THEN
3280     IF CAP THEN
3290       IF LEN(CAPS$(C))>0 THEN CAPS$(C)=CAPS$(C)+" "
3300       CAPS$(C)=CAPS$(C)+AC$
3310     ELSE
3320       CH=21+C
3330       PRINT #CH,AC$
3340     END IF
3350   END IF
3360   AC$=""
3370 END PROC
3380 ' the whole PLAY string of channel C from flat row T on (the tests and Export; BUILDMAP first)
3390 FUNC STRFROM$(C,T)
3400   CAP=-1
3410   CAPS$(C)=""
3420   PBPM=BPM
3430   AC$=""
3440   CALL STARTCH(C,T)
3450   CALL FILLCH(C,FT)
3460   CALL SENDCH(C)
3470   CAP=0
3480   RETURN CAPS$(C)
3490 END FUNC
3500 FUNC CHSTR$(C)
3510   RETURN FN STRFROM$(C,0)
3520 END FUNC
3530 ' ====================================================================== the screen
3540 PROC SETSTATUS(T$)
3550   IF HEADLESS THEN EXIT PROC
3560   UI CAPTION 60,T$
3570 END PROC
3580 ' the order label, the instrument label ... after anything changed
3590 PROC SYNCCTL
3600   IF HEADLESS THEN EXIT PROC
3610   UI CAPTION 31,"Oct "+STR$(OCV)
3620   UI CAPTION 34,RIGHT$("0"+STR$(INS),2)+" "+FN INAME$(INS)
3630   UI CAPTION 41,"Slot "+STR$(CO+1)+"/"+STR$(ONUM)+"  Pattern "+STR$(OD(CO))+"  Len "+STR$(PL(OD(CO)))
3640   UI CAPTION 37,"Step "+STR$(STP)
3650   CALL UK_SET(91,CP(CC))
3660   UI VALUE 94,CM(CC)
3670   UI CAPTION 95,"Channel "+STR$(CC+1)+" ("+FN INAME$(CI(CC))+")"
3680 END PROC
3690 ' the colors of the grid: the palette entries 240..255 of the UI's layer (the program's own are put back at the end)
3700 PROC SETPAL
3710   LOCAL I
3720   IF HEADLESS THEN EXIT PROC
3730   FOR I=0 TO 15
3740     SPL(I)=GFX("PALETTE",3,240+I)
3750   NEXT I
3760   GFX PALETTE 3,240,RGB(250,250,246)
3770   GFX PALETTE 3,241,RGB(228,236,248)
3780   GFX PALETTE 3,242,RGB(208,222,242)
3790   GFX PALETTE 3,243,RGB(255,232,150)
3800   GFX PALETTE 3,244,RGB(30,80,160)
3810   GFX PALETTE 3,245,RGB(255,255,255)
3820   GFX PALETTE 3,246,RGB(10,60,140)
3830   GFX PALETTE 3,247,RGB(130,40,160)
3840   GFX PALETTE 3,248,RGB(10,120,60)
3850   GFX PALETTE 3,249,RGB(176,90,0)
3860   GFX PALETTE 3,250,RGB(176,182,192)
3870   GFX PALETTE 3,251,RGB(192,32,32)
3880   GFX PALETTE 3,252,RGB(100,110,128)
3890   GFX PALETTE 3,253,RGB(58,66,86)
3900   GFX PALETTE 3,254,RGB(240,240,240)
3910   GFX PALETTE 3,255,RGB(128,48,48)
3920 END PROC
3930 PROC RESTOREPAL
3940   LOCAL I
3950   IF HEADLESS THEN EXIT PROC
3960   FOR I=0 TO 15
3970     GFX PALETTE 3,240+I,SPL(I)
3980   NEXT I
3990 END PROC
4000 ' the grid: the pattern of the slot under the cursor, the cursor row in the middle; every field has its color (note blue / purple, off red, instrument green,
4010 ' volume orange, empty fields grey, a muted channel grey), every 4th row has a tinted background, every 16th a stronger one, the row that plays is yellow
4020 PROC REDRAW
4030   LOCAL G,R,P,C,X,N,I,V,BG,FG,PB,HB,N$,I$,V$,M$,L$,CL
4040   IF HEADLESS THEN EXIT PROC
4050   P=OD(CO)
4060   UI GRIDPUT 50,0,0,"Row",254,253
4070   FOR C=0 TO 7
4080     N$=FN INAME$(CI(C))+"       "
4090     HB=253
4100     M$=" "
4110     IF CM(C)<>0 THEN
4120       HB=255:M$="m"
4130     END IF
4140     UI GRIDPUT 50,3+C*9,0," "+STR$(C+1)+M$+LEFT$(N$,5)+" ",254,HB
4150   NEXT C
4160   FOR G=1 TO GRH-1
4170     R=CR+G-CENTER
4180     IF R<0 OR R>=PL(P) THEN
4190       UI GRIDPUT 50,0,G,SPACE$(COLS),250,240
4200     ELSE
4210       BG=240
4220       IF R MOD 4=0 THEN BG=241
4230       IF R MOD 16=0 THEN BG=242
4240       IF PLG THEN
4250         IF CO=PCO AND R=PHROW THEN BG=243
4260       END IF
4270       UI GRIDPUT 50,0,G,RIGHT$("0"+STR$(R),2)+" ",252,BG
4280       FOR C=0 TO 7
4290         CL=CM(C)<>0
4300         X=FN IX(P,R,C)
4310         N=PN(X):I=PJ(X):V=PV(X)
4320         N$=FN NOTE$(N)
4330         FG=246
4340         IF N=0 THEN FG=250
4350         IF N=97 THEN FG=251
4360         IF N>0 AND N<97 THEN
4370           IF INSTR(N$,"#")>0 THEN FG=247
4380         END IF
4390         IF CL THEN FG=250
4400         UI GRIDPUT 50,3+C*9,G,N$,FG,BG
4410         I$=" .."
4420         FG=250
4430         IF I>0 THEN
4440           I$=" "+RIGHT$("0"+STR$(I),2)
4450           IF NOT CL THEN FG=248
4460         END IF
4470         UI GRIDPUT 50,6+C*9,G,I$,FG,BG
4480         V$=" ."
4490         FG=250
4500         IF V>=0 THEN
4510           V$=" "+MID$("0123456789ABCDEF",V+1,1)
4520           IF NOT CL THEN FG=249
4530         END IF
4540         UI GRIDPUT 50,9+C*9,G,V$,FG,BG
4550         IF C<7 THEN UI GRIDPUT 50,11+C*9,G,"|",250,BG
4560       NEXT C
4570     END IF
4580   NEXT G
4590   L$=FN CELL$(P,CR,CC)
4600   UI GRIDPUT 50,3+CC*9+FO(CF),CENTER,MID$(L$,FO(CF)+1,FL(CF)),245,244
4610 END PROC
4620 ' ====================================================================== playing
4630 PROC STOPPLAY
4640   SOUND OFF
4650   IF PLG THEN
4660     PLG=0
4670     CALL REDRAW
4680     CALL SETSTATUS("Stopped.")
4690   END IF
4700 END PROC
4710 ' the song is played from flat row T: BUILDMAP (the song as it is now), every channel is queued anew
4720 PROC FULLSYNC(O0,R0)
4730   LOCAL C,T
4740   CALL BUILDMAP
4750   IF O0>=ONUM THEN
4760     O0=ONUM-1:R0=0
4770   END IF
4780   IF R0>=PL(OD(O0)) THEN R0=PL(OD(O0))-1
4790   T=FOFF(O0)+R0
4800   SOUND OFF
4810   PBPM=BPM
4820   PS0=T:PT0=TI:PRL=T
4830   RSA=0
4840   PCO=O0:PHROW=R0
4850   FOR C=0 TO 7
4860     RS(C)=0
4870     AC$=""
4880     IF CM(C)=0 THEN
4890       CALL STARTCH(C,T)
4900       CALL FILLCH(C,T+LOOK)
4910       CALL SENDCH(C)
4920     END IF
4930   NEXT C
4940 END PROC
4950 ' one channel is queued anew from flat row T (after a change in what is queued already)
4960 PROC CHSYNC(C,T)
4970   RS(C)=0
4980   SOUND OFF C+1
4990   AC$=""
5000   CALL STARTCH(C,T)
5010   CALL FILLCH(C,T+LOOK)
5020   CALL SENDCH(C)
5030 END PROC
5040 PROC STARTPLAY(SO,SR)
5050   CALL FULLSYNC(SO,SR)
5060   PLG=-1
5070   CALL REDRAW
5080   CALL SETSTATUS("Playing: changes are heard as the song goes on.")
5090 END PROC
5100 ' called every frame: the playhead (from the clock), the channels that must be queued anew, the next notes
5110 PROC PLAYTICK
5120   LOCAL R,C
5130   IF NOT PLG THEN EXIT PROC
5140   R=PS0+INT((TI-PT0)*PBPM*4/3600)
5150   IF R>=FT THEN
5160     CALL STOPPLAY
5170     EXIT PROC
5180   END IF
5190   IF R<>PRL THEN
5200     IF R-PRL>3 THEN RSA=-1
5210     IF RSA THEN
5220       CALL FULLSYNC(FLO(R),FLR(R))
5230     ELSE
5240       PRL=R
5250       PCO=FLO(R):PHROW=FLR(R)
5260       FOR C=0 TO 7
5270         IF RS(C) AND CM(C)=0 THEN CALL CHSYNC(C,R)
5280         RS(C)=0
5290       NEXT C
5300     END IF
5310     IF FOLLOW THEN
5320       CO=PCO:CR=PHROW
5330       CALL SYNCCTL
5340     END IF
5350     CALL REDRAW
5360   END IF
5370   IF RSA THEN EXIT PROC
5380   FOR C=0 TO 7
5390     IF CM(C)=0 AND RS(C)=0 THEN
5400       IF QC(C)<PRL+LOOK AND QC(C)<FT THEN
5410         AC$=""
5420         CALL FILLCH(C,PRL+LOOK)
5430         CALL SENDCH(C)
5440       END IF
5450     END IF
5460   NEXT C
5470 END PROC
5480 ' an edit of the cell (pattern P, row R, channel C) while the song plays: if the cell lies in what the channel has queued already, it is queued anew at the next row
5490 PROC TOUCH(P,R,C)
5500   LOCAL O,T
5510   IF NOT PLG THEN EXIT PROC
5520   FOR O=0 TO ONUM-1
5530     IF OD(O)=P THEN
5540       T=FOFF(O)+R
5550       IF T>PRL THEN
5560         IF T<QC(C) THEN RS(C)=-1
5570         IF T=QC(C) AND PN(FN IX(P,R,C))=0 THEN RS(C)=-1
5580       END IF
5590     END IF
5600   NEXT O
5610 END PROC
5620 ' a change of the song's order, lengths or tempo while it plays: everything is queued anew at the next row
5630 PROC GLOBALEDIT
5640   IF PLG THEN RSA=-1
5650 END PROC
5660 ' the frequency of tracker note N (C-4 = 49 = middle C)
5670 FUNC NOTEF(N)
5680   RETURN 261.626*(2^((N-49)/12))
5690 END FUNC
5700 PROC PREVIEW(N,I)
5710   LOCAL W
5720   IF PLG THEN EXIT PROC
5730   IF N<5 OR N>96 THEN EXIT PROC
5740   W=I
5750   IF W=0 THEN W=INS
5760   WAVE CC+1,W
5770   SOUND CC+1,FN NOTEF(N),0.5,0.6
5780 END PROC
5790 ' ====================================================================== editing
5800 PROC GOROW(R)
5810   CR=FN CLAMP(R,0,PL(OD(CO))-1)
5820 END PROC
5830 PROC SETOCT(V)
5840   OCV=FN CLAMP(V,1,6)
5850 END PROC
5860 PROC SETINS(V)
5870   INS=FN CLAMP(V,4,63)
5880 END PROC
5890 ' the note of a piano key (0 if the key is none): Z S X D C V G B H N J M in the octave, Q 2 W 3 E R 5 T 6 Y 7 U I above it
5900 FUNC PIANO(K)
5910   LOCAL C$,P
5920   IF K<32 OR K>126 THEN RETURN 0
5930   C$=CHR$(K)
5940   IF C$>="A" AND C$<="Z" THEN C$=CHR$(K+32)
5950   P=INSTR("zsxdcvgbhnjm",C$)
5960   IF P>0 THEN RETURN (OCV*12)+P
5970   P=INSTR("q2w3er5t6y7ui",C$)
5980   IF P>0 THEN RETURN ((OCV+1)*12)+P
5990   RETURN 0
6000 END FUNC
6010 ' puts a note into the cursor cell (the instrument is the current one), plays it, goes down by the step
6020 PROC PUTNOTE(N)
6030   LOCAL X
6040   X=FN IX(OD(CO),CR,CC)
6050   PN(X)=N
6060   IF N>=5 AND N<=96 THEN
6070     PJ(X)=INS
6080     CALL PREVIEW(N,INS)
6090   END IF
6100   IF N=97 THEN
6110     PJ(X)=0:PV(X)=-1
6120   END IF
6130   CALL TOUCH(OD(CO),CR,CC)
6140   CALL GOROW(CR+STP)
6150 END PROC
6160 PROC TYPEDIGIT(D)
6170   LOCAL X,I
6180   X=FN IX(OD(CO),CR,CC)
6190   IF CF=1 THEN
6200     I=D*10+(PJ(X) MOD 10)
6210     IF I>63 THEN I=63
6220     PJ(X)=I
6230     CALL TOUCH(OD(CO),CR,CC)
6240     CF=2
6250   ELSE
6260     IF CF=2 THEN
6270       I=(PJ(X)\10)*10+D
6280       IF I>63 THEN I=63
6290       PJ(X)=I
6300       CALL TOUCH(OD(CO),CR,CC)
6310       CF=3
6320     END IF
6330   END IF
6340 END PROC
6350 PROC TYPEVOL(V)
6360   LOCAL X
6370   X=FN IX(OD(CO),CR,CC)
6380   PV(X)=V
6390   CALL TOUCH(OD(CO),CR,CC)
6400   CF=0
6410   CALL GOROW(CR+STP)
6420 END PROC
6430 PROC CLEARFIELD
6440   LOCAL X
6450   X=FN IX(OD(CO),CR,CC)
6460   IF CF=0 THEN
6470     PN(X)=0
6480   ELSE
6490     IF CF=3 THEN
6500       PV(X)=-1
6510     ELSE
6520       PJ(X)=0
6530     END IF
6540   END IF
6550   CALL TOUCH(OD(CO),CR,CC)
6560 END PROC
6570 ' one field to the right / left, over the channel borders
6580 PROC MOVEF(D)
6590   CF=CF+D
6600   IF CF>3 THEN
6610     IF CC<7 THEN
6620       CC=CC+1:CF=0
6630     ELSE
6640       CF=3
6650     END IF
6660   END IF
6670   IF CF<0 THEN
6680     IF CC>0 THEN
6690       CC=CC-1:CF=3
6700     ELSE
6710       CF=0
6720     END IF
6730   END IF
6740 END PROC
6750 PROC KEYIN(K)
6760   CALL KEYACT(K)
6770   CALL SYNCCTL
6780   CALL REDRAW
6790 END PROC
6800 PROC KEYACT(K)
6810   LOCAL M,N,V
6820   M=K AND 65536
6830   K=K AND 65535
6840   IF K=4097 THEN
6850     IF M<>0 THEN
6860       CC=FN CLAMP(CC-1,0,7):CF=0
6870     ELSE
6880       CALL MOVEF(-1)
6890     END IF
6900     EXIT PROC
6910   END IF
6920   IF K=4098 THEN
6930     IF M<>0 THEN
6940       CC=FN CLAMP(CC+1,0,7):CF=0
6950     ELSE
6960       CALL MOVEF(1)
6970     END IF
6980     EXIT PROC
6990   END IF
7000   IF K=4099 THEN
7010     CALL GOROW(CR-1)
7020     EXIT PROC
7030   END IF
7040   IF K=4100 THEN
7050     CALL GOROW(CR+1)
7060     EXIT PROC
7070   END IF
7080   IF K=4103 THEN
7090     CALL GOROW(CR-16)
7100     EXIT PROC
7110   END IF
7120   IF K=4104 THEN
7130     CALL GOROW(CR+16)
7140     EXIT PROC
7150   END IF
7160   IF K=4101 THEN
7170     CALL GOROW(0)
7180     EXIT PROC
7190   END IF
7200   IF K=4102 THEN
7210     CALL GOROW(99)
7220     EXIT PROC
7230   END IF
7240   IF K=4105 OR K=8 THEN
7250     CALL CLEARFIELD
7260     EXIT PROC
7270   END IF
7280   IF K=13 THEN
7290     CALL PLAYROW
7300     EXIT PROC
7310   END IF
7320   IF K=32 THEN
7330     IF PLG THEN
7340       CALL STOPPLAY
7350     ELSE
7360       CALL STARTPLAY(CO,CR)
7370     END IF
7380     EXIT PROC
7390   END IF
7400   IF K=45 THEN
7410     CALL SETOCT(OCV-1)
7420     EXIT PROC
7430   END IF
7440   IF K=43 THEN
7450     CALL SETOCT(OCV+1)
7460     EXIT PROC
7470   END IF
7480   IF K=91 THEN
7490     CALL SETINS(INS-1)
7500     EXIT PROC
7510   END IF
7520   IF K=93 THEN
7530     CALL SETINS(INS+1)
7540     EXIT PROC
7550   END IF
7560   IF CF=0 THEN
7570     IF K=49 THEN
7580       CALL PUTNOTE(97)
7590     ELSE
7600       N=FN PIANO(K)
7610       IF N>0 THEN CALL PUTNOTE(N)
7620     END IF
7630     EXIT PROC
7640   END IF
7650   IF CF=3 THEN
7660     IF K<128 THEN
7670       V=INSTR("0123456789abcdef",CHR$(K+32*(K>=65 AND K<=70)*-1))
7680       IF V>0 THEN CALL TYPEVOL(V-1)
7690     END IF
7700     EXIT PROC
7710   END IF
7720   IF K>=48 AND K<=57 THEN CALL TYPEDIGIT(K-48)
7730 END PROC
7740 ' the row under the cursor: all its notes at once
7750 PROC PLAYROW
7760   LOCAL C,X,N
7770   IF PLG THEN EXIT PROC
7780   FOR C=0 TO 7
7790     X=FN IX(OD(CO),CR,C)
7800     N=PN(X)
7810     IF N>=5 AND N<=96 AND CM(C)=0 THEN
7820       IF PJ(X)>0 THEN WAVE C+1,PJ(X) ELSE WAVE C+1,CI(C)
7830       SOUND C+1,FN NOTEF(N),0.5,0.6
7840     END IF
7850   NEXT C
7860 END PROC
7870 ' a click on a cell of the grid: V = row * COLS + column
7880 PROC CLICKIN(V)
7890   LOCAL G,C,R,W
7900   G=V\COLS
7910   C=V MOD COLS
7920   IF G<1 THEN EXIT PROC
7930   CALL GOROW(CR+G-CENTER)
7940   IF C>=3 THEN
7950     W=(C-3)\9
7960     IF W>7 THEN W=7
7970     CC=W
7980     R=(C-3)-W*9
7990     CF=0
8000     IF R=4 THEN CF=1
8010     IF R=5 THEN CF=2
8020     IF R>=7 THEN CF=3
8030   END IF
8040   CALL SYNCCTL
8050   CALL REDRAW
8060 END PROC
8070 ' ====================================================================== the order
8080 PROC SETCO(V)
8090   CO=FN CLAMP(V,0,ONUM-1)
8100   CR=FN CLAMP(CR,0,PL(OD(CO))-1)
8110 END PROC
8120 PROC NEWPATTERN
8130   IF NPAT>=32 THEN
8140     CALL SETSTATUS("There are 32 patterns already.")
8150     EXIT PROC
8160   END IF
8170   CALL CLEARPAT(NPAT)
8180   PL(NPAT)=32
8190   OD(CO)=NPAT
8200   NPAT=NPAT+1
8210   CR=0
8220   CALL GLOBALEDIT
8230 END PROC
8240 PROC CLONEPATTERN
8250   LOCAL R,C,X,Y,P
8260   IF NPAT>=32 THEN
8270     CALL SETSTATUS("There are 32 patterns already.")
8280     EXIT PROC
8290   END IF
8300   P=OD(CO)
8310   FOR R=0 TO 63
8320     FOR C=0 TO 7
8330       X=FN IX(P,R,C):Y=FN IX(NPAT,R,C)
8340       PN(Y)=PN(X):PJ(Y)=PJ(X):PV(Y)=PV(X)
8350     NEXT C
8360   NEXT R
8370   PL(NPAT)=PL(P)
8380   OD(CO)=NPAT
8390   NPAT=NPAT+1
8400   CALL GLOBALEDIT
8410 END PROC
8420 PROC INSSLOT
8430   LOCAL I
8440   IF ONUM>=64 THEN EXIT PROC
8450   FOR I=ONUM TO CO+2 STEP -1
8460     OD(I)=OD(I-1)
8470   NEXT I
8480   OD(CO+1)=OD(CO)
8490   ONUM=ONUM+1
8500   CO=CO+1
8510   CALL GLOBALEDIT
8520 END PROC
8530 PROC DELSLOT
8540   LOCAL I
8550   IF ONUM<=1 THEN EXIT PROC
8560   FOR I=CO TO ONUM-2
8570     OD(I)=OD(I+1)
8580   NEXT I
8590   ONUM=ONUM-1
8600   CALL SETCO(CO)
8610   CALL GLOBALEDIT
8620 END PROC
8630 PROC CYCLELEN
8640   LOCAL P,L
8650   P=OD(CO)
8660   L=PL(P)+16
8670   IF L>64 THEN L=16
8680   PL(P)=L
8690   CALL GOROW(CR)
8700   CALL GLOBALEDIT
8710 END PROC
8720 ' ====================================================================== files
8730 ' the song as text: TRACKER 1, BPM, CHANNEL lines, ORDER, the instruments 32..63 that the song uses, PATTERN lines and one C line (row channel note instrument volume) per used cell
8740 PROC SAVEF(F$)
8750   LOCAL P,R,C,X,I,L$
8760   OPEN F$ FOR OUTPUT AS #1
8770   PRINT #1,"TRACKER 1"
8780   PRINT #1,"BPM "+STR$(BPM)
8790   FOR C=0 TO 7
8800     PRINT #1,"CHANNEL "+STR$(C)+" "+STR$(CI(C))+" "+STR$(CP(C))+" "+STR$(CM(C)<>0)
8810   NEXT C
8820   L$="ORDER"
8830   FOR I=0 TO ONUM-1
8840     L$=L$+" "+STR$(OD(I))
8850   NEXT I
8860   PRINT #1,L$
8870   FOR I=32 TO 63
8880     IF FN USES(I) THEN PRINT #1,"INSTRUMENT "+STR$(I)+" "+INSTRUMENT$(I)
8890   NEXT I
8900   FOR P=0 TO NPAT-1
8910     PRINT #1,"PATTERN "+STR$(P)+" "+STR$(PL(P))
8920     FOR R=0 TO 63
8930       FOR C=0 TO 7
8940         X=FN IX(P,R,C)
8950         IF PN(X)<>0 OR PJ(X)<>0 OR PV(X)>=0 THEN PRINT #1,"C "+STR$(R)+" "+STR$(C)+" "+STR$(PN(X))+" "+STR$(PJ(X))+" "+STR$(PV(X))
8960       NEXT C
8970     NEXT R
8980   NEXT P
8990   PRINT #1,"END"
9000   CLOSE #1
9010   CALL SETSTATUS("Saved "+F$+".")
9020 END PROC
9030 PROC OPENF(F$)
9040   LOCAL L$,W$,P,N,C,X,A,B,D$,K
9050   OPEN F$ FOR INPUT AS #1
9060   CALL NEWSONG
9070   ONUM=0:NPAT=0
9080   P=0
9090   WHILE EOF(1)=0
9100     LINE INPUT #1,L$
9110     W$=FN TOKN$(L$,0)
9120     IF W$="BPM" THEN BPM=FN CLAMP(INT(VAL(FN TOKN$(L$,1))),30,300)
9130     IF W$="CHANNEL" THEN
9140       C=FN CLAMP(INT(VAL(FN TOKN$(L$,1))),0,7)
9150       CI(C)=FN CLAMP(INT(VAL(FN TOKN$(L$,2))),4,63)
9160       CP(C)=FN CLAMP(INT(VAL(FN TOKN$(L$,3))),-100,100)
9170       CM(C)=0
9180       IF VAL(FN TOKN$(L$,4))<>0 THEN CM(C)=-1
9190     END IF
9200     IF W$="ORDER" THEN
9210       ONUM=0
9220       FOR K=1 TO 64
9230         D$=FN TOKN$(L$,K)
9240         IF D$<>"" THEN
9250           OD(ONUM)=FN CLAMP(INT(VAL(D$)),0,31)
9260           ONUM=ONUM+1
9270         END IF
9280       NEXT K
9290     END IF
9300     IF W$="INSTRUMENT" THEN
9310       N=INT(VAL(FN TOKN$(L$,1)))
9320       IF N>=4 AND N<=63 THEN
9330         D$=MID$(L$,INSTR(INSTR(L$," ")+1,L$," ")+1)
9340         INSTRUMENT N,D$
9350       END IF
9360     END IF
9370     IF W$="PATTERN" THEN
9380       P=FN CLAMP(INT(VAL(FN TOKN$(L$,1))),0,31)
9390       PL(P)=FN CLAMP(INT(VAL(FN TOKN$(L$,2))),16,64)
9400       IF P+1>NPAT THEN NPAT=P+1
9410     END IF
9420     IF W$="C" THEN
9430       A=INT(VAL(FN TOKN$(L$,1))):C=INT(VAL(FN TOKN$(L$,2)))
9440       IF A>=0 AND A<64 AND C>=0 AND C<8 THEN
9450         X=FN IX(P,A,C)
9460         PN(X)=FN CLAMP(INT(VAL(FN TOKN$(L$,3))),0,97)
9470         PJ(X)=FN CLAMP(INT(VAL(FN TOKN$(L$,4))),0,63)
9480         PV(X)=FN CLAMP(INT(VAL(FN TOKN$(L$,5))),-1,15)
9490       END IF
9500     END IF
9510   WEND
9520   CLOSE #1
9530   IF ONUM=0 THEN
9540     ONUM=1:OD(0)=0
9550   END IF
9560   IF NPAT=0 THEN NPAT=1
9570   FOR K=0 TO ONUM-1
9580     IF OD(K)>=NPAT THEN OD(K)=0
9590   NEXT K
9600   CO=0:CR=0:CC=0:CF=0
9610   CALL TOCONTROLS
9620   CALL SYNCCTL
9630   CALL REDRAW
9640   CALL SETSTATUS("Opened "+F$+".")
9650 END PROC
9660 ' the PLAY lines of the whole song, one per call: CALLBACK-less, written to the file #1 or collected into the global list XL$ (for Copy)
9670 PROC EXPORTLINES(TOCLIP)
9680   LOCAL C,I,N,L,K,A$,B$,P,XN
9690   CALL BUILDMAP
9700   XL$=""
9710   XN=10
9720   XL$=XL$+STR$(XN)+" ' song made with the tracker of Engine1999 ("+STR$(BPM)+" beats a minute)"+CHR$(10)
9730   XN=XN+10
9740   FOR I=32 TO 63
9750     IF FN USES(I) THEN
9760       XL$=XL$+STR$(XN)+" INSTRUMENT "+STR$(I)+","+CHR$(34)+INSTRUMENT$(I)+CHR$(34)+CHR$(10)
9770       XN=XN+10
9780     END IF
9790   NEXT I
9800   FOR C=0 TO 7
9810     A$=FN CHSTR$(C)
9820     IF CM(C)<>0 THEN A$=""
9830     K=0
9840     WHILE LEN(A$)>0
9850       L=150
9860       IF LEN(A$)<=L THEN
9870         L=LEN(A$)
9880       ELSE
9890         WHILE L>1 AND MID$(A$,L+1,1)<>" "
9900           L=L-1
9910         WEND
9920       END IF
9930       B$=LEFT$(A$,L)
9940       A$=MID$(A$,L+1)
9950       IF K=0 THEN
9960         XL$=XL$+STR$(XN)+" V"+STR$(C+1)+"$="+CHR$(34)+B$+CHR$(34)+CHR$(10)
9970       ELSE
9980         XL$=XL$+STR$(XN)+" V"+STR$(C+1)+"$=V"+STR$(C+1)+"$+"+CHR$(34)+B$+CHR$(34)+CHR$(10)
9990       END IF
10000       XN=XN+10
10010       K=K+1
10020     WEND
10030     IF K=0 THEN
10040       XL$=XL$+STR$(XN)+" V"+STR$(C+1)+"$="+CHR$(34)+CHR$(34)+CHR$(10)
10050       XN=XN+10
10060     END IF
10070   NEXT C
10080   XL$=XL$+STR$(XN)+" PLAY V1$,V2$,V3$,V4$,V5$,V6$,V7$,V8$"+CHR$(10)
10090 END PROC
10100 PROC EXPORTF(F$)
10110   CALL EXPORTLINES(0)
10120   OPEN F$ FOR OUTPUT AS #1
10130   PRINT #1,XL$;
10140   CLOSE #1
10150   CALL SETSTATUS("Exported "+F$+" (MERGE or LOAD it, RUN plays it).")
10160 END PROC
10170 ' ====================================================================== the form
10180 PROC MAKEFORM
10190   UI PANEL 1,0,0,640,300,"Tracker"
10200   UI BUTTON 10,10,22,50,20,"Song"
10210   UI BUTTON 11,64,22,66,20,"Pattern"
10220   UI BUTTON 12,134,22,46,20,"Here"
10230   UI BUTTON 13,184,22,46,20,"Stop"
10240   CALL UK_SLIDER(20,246,22,384,"Tempo",40,240,BPM)
10250   UI BUTTON 30,10,46,20,20,"-"
10260   UI LABEL 31,34,48,46,14,""
10270   UI BUTTON 32,84,46,20,20,"+"
10280   UI BUTTON 33,114,46,20,20,"<"
10290   UI LABEL 34,138,48,130,14,""
10300   UI BUTTON 35,272,46,20,20,">"
10310   UI BUTTON 36,302,46,70,20,"Ch = Ins"
10320   UI BUTTON 37,376,46,64,20,"Step 1"
10330   UI LABEL 95,448,48,182,14,""
10340   UI BUTTON 40,10,70,20,20,"<"
10350   UI LABEL 41,34,72,230,14,""
10360   UI BUTTON 42,268,70,20,20,">"
10370   UI BUTTON 43,296,70,48,20,"Pat<"
10380   UI BUTTON 44,348,70,48,20,"Pat>"
10390   UI BUTTON 45,400,70,44,20,"New"
10400   UI BUTTON 46,448,70,52,20,"Clone"
10410   UI BUTTON 47,504,70,40,20,"Ins"
10420   UI BUTTON 48,548,70,40,20,"Del"
10430   UI BUTTON 49,592,70,38,20,"Len"
10440   UI GRID 50,10,94,COLS,GRH
10450   UI CHECK 94,10,246,80,16,"Mute"
10460   CALL UK_SLIDER(91,96,244,300,"Pan",-100,100,0)
10470   UI CHECK 96,410,246,100,16,"Follow"
10480   UI VALUE 96,-1
10490   UI LABEL 60,10,266,620,14,""
10500   UI BUTTON 70,10,278,60,20,"Save"
10510   UI BUTTON 71,74,278,60,20,"Open"
10520   UI BUTTON 72,138,278,66,20,"Export"
10530   UI BUTTON 73,208,278,50,20,"Copy"
10540   UI BUTTON 74,262,278,54,20,"Clear"
10550   UI BUTTON 75,570,278,60,20,"Quit"
10560   UI FOCUS 50
10570 END PROC
10580 ' the controls that show the song (after Open / Clear)
10590 PROC TOCONTROLS
10600   IF HEADLESS THEN EXIT PROC
10610   CALL UK_SET(20,BPM)
10620 END PROC
10630 ' ====================================================================== what happens
10640 PROC REACT(E,K,V)
10650   LOCAL F$
10660   IF E=50 AND K=9 THEN CALL KEYIN(V)
10670   IF E=50 AND K=10 THEN CALL CLICKIN(V)
10680   IF FN UK_TRACK(E,V) THEN
10690     IF E=20 THEN
10700       BPM=V
10710       IF PLG AND BPM<>PBPM THEN RSA=-1
10720     END IF
10730     IF E=91 THEN
10740       CP(CC)=V
10750       IF PLG THEN RS(CC)=-1
10760       CALL REDRAW
10770     END IF
10780   END IF
10790   IF E=94 AND K=2 THEN
10800     CM(CC)=V
10810     IF V<>0 THEN CM(CC)=-1
10820     IF PLG THEN
10830       IF CM(CC)<>0 THEN
10840         SOUND OFF CC+1
10850       ELSE
10860         RS(CC)=-1
10870       END IF
10880     END IF
10890     CALL REDRAW
10900     UI FOCUS 50
10910   END IF
10920   IF E=96 AND K=2 THEN
10930     FOLLOW=V<>0
10940     UI FOCUS 50
10950   END IF
10960   IF K=1 THEN
10970     IF E=10 THEN CALL STARTPLAY(0,0)
10980     IF E=11 THEN CALL STARTPLAY(CO,0)
10990     IF E=12 THEN CALL STARTPLAY(CO,CR)
11000     IF E=13 THEN CALL STOPPLAY
11010     IF E=30 THEN CALL SETOCT(OCV-1)
11020     IF E=32 THEN CALL SETOCT(OCV+1)
11030     IF E=33 THEN CALL SETINS(INS-1)
11040     IF E=35 THEN CALL SETINS(INS+1)
11050     IF E=36 THEN
11060       CI(CC)=INS
11070       IF PLG THEN RS(CC)=-1
11080       CALL SETSTATUS("Channel "+STR$(CC+1)+" starts with instrument "+STR$(INS)+".")
11090     END IF
11100     IF E=37 THEN
11110       STP=STP+1
11120       IF STP>4 THEN STP=0
11130     END IF
11140     IF E=40 THEN CALL SETCO(CO-1)
11150     IF E=42 THEN CALL SETCO(CO+1)
11160     IF E=43 THEN
11170       OD(CO)=FN CLAMP(OD(CO)-1,0,NPAT-1)
11180       CALL GOROW(CR)
11190       CALL GLOBALEDIT
11200     END IF
11210     IF E=44 THEN
11220       OD(CO)=FN CLAMP(OD(CO)+1,0,NPAT-1)
11230       CALL GOROW(CR)
11240       CALL GLOBALEDIT
11250     END IF
11260     IF E=45 THEN CALL NEWPATTERN
11270     IF E=46 THEN CALL CLONEPATTERN
11280     IF E=47 THEN CALL INSSLOT
11290     IF E=48 THEN CALL DELSLOT
11300     IF E=49 THEN CALL CYCLELEN
11310     IF E=70 THEN
11320       F$=FN FR_PICK$("Save the song as",DR$,"*.trk;*.txt",-1)
11330       DR$=FR_DIR$
11340       IF F$<>"" THEN CALL SAVEF(F$)
11350     END IF
11360     IF E=71 THEN
11370       F$=FN FR_PICK$("Open a song",DR$,"*.trk;*.txt",0)
11380       DR$=FR_DIR$
11390       IF F$<>"" THEN
11400         CALL STOPPLAY
11410         CALL OPENF(F$)
11420       END IF
11430     END IF
11440     IF E=72 THEN
11450       F$=FN FR_PICK$("Export as BASIC lines",DR$,"*.bas",-1)
11460       DR$=FR_DIR$
11470       IF F$<>"" THEN CALL EXPORTF(F$)
11480     END IF
11490     IF E=73 THEN
11500       CALL EXPORTLINES(-1)
11510       CLIPBOARD XL$
11520       CALL SETSTATUS("The PLAY lines are on the clipboard.")
11530     END IF
11540     IF E=74 THEN
11550       IF FN UK_ASK("Clear","Throw the song away and start a new one?") THEN
11560         CALL STOPPLAY
11570         CALL NEWSONG
11580         CALL TOCONTROLS
11590       END IF
11600     END IF
11610     IF E=75 THEN Q=1
11620     CALL SYNCCTL
11630     CALL REDRAW
11640     UI FOCUS 50
11650   END IF
11660 END PROC
11670 ' ====================================================================== the self-test (TRACKER_HEADLESS=1 in the console host: tests the song / string code, no window)
11680 PROC CHECK(OK,T$)
11690   IF OK=0 THEN
11700     PRINT "FAIL: ";T$
11710     BAD=BAD+1
11720   END IF
11730   NCHK=NCHK+1
11740 END PROC
11750 PROC SELFTEST
11760   LOCAL X,Y,A$,B$,I
11770   HEADLESS=-1
11780   BAD=0:NCHK=0
11790   CALL NEWSONG
11800   CALL CHECK(FN NOTE$(49)="C-4" AND FN NOTE$(40)="D#3" AND FN NOTE$(0)="---" AND FN NOTE$(97)="===" AND FN NOTE$(96)="B-7","note names")
11810   OCV=3
11820   CALL CHECK(FN PIANO(ASC("z"))=37 AND FN PIANO(ASC("m"))=48 AND FN PIANO(ASC("Z"))=37 AND FN PIANO(ASC("q"))=49 AND FN PIANO(ASC("i"))=61 AND FN PIANO(ASC("a"))=0,"piano keys")
11830   CALL CHECK(FN CELL$(0,0,0)="--- .. ." AND LEN(FN CELL$(0,0,0))=8,"an empty cell")
11840   X=FN IX(0,0,0)
11850   PN(X)=49:PJ(X)=4:PV(X)=15
11860   CALL CHECK(FN CELL$(0,0,0)="C-4 04 F","a filled cell")
11870   PN(FN IX(0,4,0))=97
11880   PN(FN IX(0,8,0))=40
11890   BPM=120:CP(0)=0:CI(0)=4
11900   CALL BUILDMAP
11910   CALL CHECK(FT=32,"the song has 32 rows")
11920   A$=FN CHSTR$(0)
11930   CALL CHECK(A$="T120 MN Y0 I4 V0.8 V1 O4 MN C4 R4 O3 ML D#1 MN D#2","the PLAY string of a channel: "+A$)
11940   CALL CHECK(FN CHSTR$(1)="T120 MN Y0.3 I5 V0.8 R1 R1","an empty channel is its header and rests: "+FN CHSTR$(1))
11950   ' the music string must be accepted by PLAY (it checks its strings), also when the notes use other instruments and volumes
11960   PN(FN IX(0,12,1))=60:PJ(FN IX(0,12,1))=33:PV(FN IX(0,12,1))=7
11970   PN(FN IX(0,13,1))=61
11980   PN(FN IX(0,15,1))=5
11990   CALL BUILDMAP
12000   B$=FN CHSTR$(1)
12010   CALL CHECK(B$="T120 MN Y0.3 I5 V0.8 R2. I33 V0.467 O4 MN B16 O5 MN C8 O0 ML E1 MN E16","a second channel: "+B$)
12020   PLAY A$,B$
12030   ' from a later row: the first notes are skipped, a note that started earlier is not played (its row is not in the range)
12040   A$=FN STRFROM$(0,8)
12050   CALL CHECK(A$="T120 MN Y0 I4 V1 O3 ML D#1 MN D#2","playing from row 8 (instrument and volume as they were before): "+A$)
12060   A$=FN STRFROM$(0,10)
12070   CALL CHECK(A$="T120 MN Y0 I4 V1 O3 ML D#1 MN D#4.","a note that is still sounding is carried on: "+A$)
12080   A$=FN STRFROM$(0,5)
12090   CALL CHECK(A$="T120 MN Y0 I4 V1 R8. O3 ML D#1 MN D#2","after a note off there is a rest: "+A$)
12100   PLAY A$
12110   ' the song is queued in pieces: the pieces together are the whole string (with the settings restated at the start of each piece)
12120   CAP=-1:CAPS$(0)="":PBPM=BPM:AC$=""
12130   CALL STARTCH(0,0)
12140   CALL FILLCH(0,6)
12150   CALL SENDCH(0)
12160   CALL CHECK(QC(0)=8,"the first piece ends at the next note: "+STR$(QC(0)))
12170   CALL FILLCH(0,20)
12180   CALL SENDCH(0)
12190   CALL CHECK(QC(0)=32 AND CAPS$(0)="T120 MN Y0 I4 V0.8 V1 O4 MN C4 R4 T120 V1 O3 ML D#1 MN D#2","the pieces: "+CAPS$(0))
12200   CAP=0
12210   ' a change in the song while it plays is queued anew when it lies in what is queued already
12220   PLG=-1:PRL=0
12230   FOR I=0 TO 7
12240     RS(I)=0
12250   NEXT I
12260   QC(0)=8
12270   CALL TOUCH(0,6,0)
12280   CALL CHECK(RS(0)<>0,"a change inside what is queued")
12290   RS(0)=0
12300   CALL TOUCH(0,20,0)
12310   CALL TOUCH(0,0,0)
12320   CALL TOUCH(0,8,0)
12330   CALL CHECK(RS(0)=0,"a change behind it, before the playhead, or a replaced note at its end")
12340   PN(FN IX(0,8,0))=0
12350   CALL TOUCH(0,8,0)
12360   CALL CHECK(RS(0)<>0,"a note that ends the queued piece is deleted")
12370   PN(FN IX(0,8,0))=40
12380   RS(0)=0
12390   CALL GLOBALEDIT
12400   CALL CHECK(RSA<>0,"an edit of the order")
12410   RSA=0:PLG=0
12420   ' two slots: the second pattern follows the first
12430   CALL NEWSONG
12440   CALL INSSLOT
12450   CALL NEWPATTERN
12460   CALL CHECK(ONUM=2 AND OD(0)=0 AND OD(1)=1 AND NPAT=2,"a new slot and pattern")
12470   PN(FN IX(1,0,2))=50
12480   CALL BUILDMAP
12490   CALL CHECK(FT=64 AND FLO(32)=1 AND FLR(32)=0 AND PN(FN CX(32,2))=50 AND FOFF(1)=32,"the order is flattened")
12500   A$=FN CHSTR$(2)
12510   CALL CHECK(LEFT$(A$,20)="T125 MN Y-0.6 I6 R1" OR LEFT$(A$,12)="T125 MN Y-0.","pan in the header: "+A$)
12520   ' the cursor and the editing
12530   CALL NEWSONG
12540   CR=0:CC=0:CF=0
12550   OCV=4:INS=5:STP=2
12560   CALL KEYACT(ASC("z"))
12570   CALL CHECK(PN(FN IX(0,0,0))=49 AND PJ(FN IX(0,0,0))=5 AND CR=2,"a piano key writes the note with the instrument and steps")
12580   CALL KEYACT(49)
12590   CALL CHECK(PN(FN IX(0,2,0))=97 AND CR=4,"1 puts a note off")
12600   CALL KEYACT(4098)
12610   CALL KEYACT(51)
12620   CALL KEYACT(55)
12630   CALL CHECK(CF=3 AND PJ(FN IX(0,4,0))=37 AND CR=4,"two digits are the instrument")
12640   CALL KEYACT(ASC("C"))
12650   CALL CHECK(PV(FN IX(0,4,0))=12 AND CF=0 AND CR=6,"a hex digit is the volume (upper or lower case)")
12660   CALL KEYACT(4097)
12670   CALL CHECK(CC=0 AND CF=0,"Left at the first field stays")
12680   CALL KEYACT(4098+65536)
12690   CALL CHECK(CC=1 AND CF=0,"Shift + Right jumps a channel")
12700   CALL KEYACT(4105)
12710   CALL KEYACT(4103)
12720   CALL CHECK(CR=0,"PageUp stops at the first row")
12730   CALL KEYACT(4102)
12740   CALL CHECK(CR=31,"End goes to the last row")
12750   ' save and open
12760   CALL NEWSONG
12770   BPM=99:CP(3)=-77:CM(5)=-1:CI(2)=33
12780   INSTRUMENT 33,"NAME TESTINS; OSC SAW 0.5; GAIN 1.1"
12790   CALL INSSLOT
12800   CALL NEWPATTERN
12810   PL(1)=48
12820   PN(FN IX(1,5,6))=60:PJ(FN IX(1,5,6))=33:PV(FN IX(1,5,6))=3
12830   PN(FN IX(0,31,7))=97
12840   CALL SAVEF("mem:song1")
12850   CALL NEWSONG
12860   INSTRUMENT 33
12870   CALL OPENF("mem:song1")
12880   CALL CHECK(BPM=99 AND CP(3)=-77 AND CM(5)<>0 AND CM(4)=0 AND CI(2)=33 AND ONUM=2 AND OD(1)=1 AND NPAT=2 AND PL(1)=48 AND PL(0)=32,"song header and order survive a save and open")
12890   CALL CHECK(PN(FN IX(1,5,6))=60 AND PJ(FN IX(1,5,6))=33 AND PV(FN IX(1,5,6))=3 AND PN(FN IX(0,31,7))=97 AND PN(FN IX(0,0,0))=0 AND PV(FN IX(0,0,0))=-1,"cells survive a save and open")
12900   CALL CHECK(FN INAME$(33)="TESTINS","the instrument of the song is defined again by Open: "+FN INAME$(33))
12910   ' export
12920   CALL EXPORTLINES(0)
12930   CALL CHECK(INSTR(XL$,"INSTRUMENT 33,")>0 AND INSTR(XL$,"PLAY V1$,V2$,V3$,V4$,V5$,V6$,V7$,V8$")>0 AND INSTR(XL$,"V1$=")>0,"the exported lines")
12940   ' TRACKER_HEADLESS=file: also opens that song and plays its strings (PLAY checks them)
12950   IF ENV$("TRACKER_HEADLESS")<>"1" THEN
12960     CALL OPENF(ENV$("TRACKER_HEADLESS"))
12970     CALL BUILDMAP
12980     FOR I=0 TO 7
12990       S$(I)=FN CHSTR$(I)
13000     NEXT I
13010     PLAY S$(0),S$(1),S$(2),S$(3),S$(4),S$(5),S$(6),S$(7)
13020     PRINT "song ";ENV$("TRACKER_HEADLESS");": ";ONUM;" slots, ";NPAT;" patterns, ";FT;" rows, strings of ";LEN(S$(0));" ";LEN(S$(1));" ";LEN(S$(2));" ";LEN(S$(3));" ";LEN(S$(4));" ";LEN(S$(5));" ";LEN(S$(6));" ";LEN(S$(7));" characters"
13030   END IF
13040   PRINT "tracker selftest ";
13050   IF BAD=0 THEN PRINT "ok ";NCHK ELSE PRINT BAD;" FAILED of ";NCHK
13060 END PROC
13070 ' ====================================================================== main program
13080 IF ENV$("TRACKER_HEADLESS")<>"" THEN
13090   CALL SELFTEST
13100   END
13110 END IF
13120 UI OPEN
13130 CALL SETPAL
13140 FOR I=0 TO 7
13150   CH2=21+I
13160   OPEN "play:"+STR$(I+1) FOR OUTPUT AS #CH2
13170 NEXT I
13180 CALL NEWSONG
13190 CALL MAKEFORM
13200 CALL SYNCCTL
13210 CALL REDRAW
13220 CALL SETSTATUS("Piano keys Z S X D C V G B H N J M write notes, Space plays, Tab leaves the grid.")
13230 ON ERROR GOTO ERRH
13240 ARGLOAD$=""
13250 IF ARGC>=1 THEN ARGLOAD$=ARG$(1)
13260 IF ARGLOAD$<>"" THEN CALL OPENF(ARGLOAD$)
13270 LABEL LOOPTOP
13280 ON ERROR GOTO ERRH
13290 ON UI GOSUB HANDLER
13300 REPEAT
13310   GFX VSYNC
13320   UI UPDATE
13330   CALL PLAYTICK
13340 UNTIL Q=1
13350 ON UI OFF
13360 ON ERROR OFF
13370 SOUND OFF
13380 FOR I=0 TO 7
13390   CH2=21+I
13400   CLOSE #CH2
13410 NEXT I
13420 CALL RESTOREPAL
13430 UI CLOSE
13440 REM no GFX RESET here
13450 CLS
13460 PRINT "Tracker finished."
13470 END
13480 LABEL ERRH
13490 ON UI OFF
13500 CALL UK_MSG("Error",ERR$+"|(line "+STR$(ERL)+")")
13510 RESUME LOOPTOP
13520 LABEL HANDLER
13530 E=UI("EVENT")
13540 WHILE E<>0
13550   EK=UI("KIND")
13560   EV=UI("VALUE")
13570   CALL REACT(E,EK,EV)
13580   E=UI("EVENT")
13590 WEND
13600 RETURN
