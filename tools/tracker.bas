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
240 ' Loop: the radio buttons Once / Loop pat (the pattern of the slot that plays) / Loop song (ids 97..99) say what happens at the end of the pattern or song.
250 ' widget ids: 97..99 loop, 80 tab strip, 1 form, 10 Song, 11 Pattern, 12 Here, 13 Stop, 20 tempo slider (+1 label, +2 readout), 30 oct -, 31 oct label, 32 oct +, 33 inst <, 34 inst label, 35 inst >,
260 ' 36 Ch = Ins, 37 step button, 94 Mute, 91 Pan slider (+1, +2), 40 order <, 41 order label, 42 order >, 43 Pat<, 44 Pat>, 45 New, 46 Clone, 47 Ins, 48 Del, 49 Len,
270 ' 50 the grid, 96 Follow, 60 status, 70 Save, 71 Open, 72 Export, 73 Copy, 74 Clear, 75 Quit
280 LOAD LIBRARY "uikit"
290 LOAD LIBRARY "filereq"
300 REM the graphics of the calling program stay (no GFX RESET)
310 CLS
320 BREAK OFF
330 Q=0
340 HEADLESS=0
350 COLS=74
360 GRH=13
370 CENTER=5
380 DIM PN(16384),PJ(16384),PV(16384),PL(32),OD(64),CI(8),CP(8),CM(8),S$(8)
390 DIM FLO(4096),FLR(4096),FOFF(64),QC(8),RS(8),XI(8),XV(8),XO(8),CAPS$(8),SPL(16)
400 DIM NT$(12),NP$(12),LV(8),LS$(8),FO(4),FL(4)
410 FOR I=0 TO 11
420   READ NT$(I),NP$(I)
430 NEXT I
440 DATA "C-","C","C#","C#","D-","D","D#","D#","E-","E","F-","F","F#","F#","G-","G","G#","G#","A-","A","A#","A#","B-","B"
450 FOR I=0 TO 7
460   READ LV(I),LS$(I)
470 NEXT I
480 DATA 16,"1",12,"2.",8,"2",6,"4.",4,"4",3,"8.",2,"8",1,"16"
490 FO(0)=0:FO(1)=4:FO(2)=5:FO(3)=7
500 FL(0)=3:FL(1)=1:FL(2)=1:FL(3)=1
510 DR$=""
520 ' the song: ONUM slots of the order, NPAT patterns, BPM; the cursor: CO slot, CR row, CC channel, CF field (0 note, 1 / 2 the instrument digits, 3 volume)
530 BPM=125
540 ONUM=1
550 NPAT=1
560 CO=0
570 CR=0
580 CC=0
590 CF=0
600 OCV=3
610 INS=4
620 STP=1
630 PLG=0
640 PCO=0
650 PHROW=0
660 PT0=0
670 FT=0
680 AC$=""
690 CURI=0
700 CURV=12
710 CURO=-1
720 PN2=0
730 PS=0
740 PI2=0
750 PV2=-1
760 PS0=0
770 PRL=0
780 PBPM=125
790 RSA=0
800 FOLLOW=-1
810 LOOK=8
820 LOOPM=0
830 ACTL=0
840 LS=0
850 LE=0
860 LL=1
870 VEND=0
880 CAP=0
890 BEI=0
900 BEV=-1
910 PCO=0
920 PHROW=0
930 ' ====================================================================== functions (they must stand before the lines that use them)
940 FUNC NUM$(V)
950   RETURN STR$(INT(V*1000+0.5)/1000)
960 END FUNC
970 FUNC IX(P,R,C)
980   RETURN (P*64+R)*8+C
990 END FUNC
1000 FUNC CLAMP(V,LO,HI)
1010   IF V<LO THEN RETURN LO
1020   IF V>HI THEN RETURN HI
1030   RETURN V
1040 END FUNC
1050 FUNC NOTE$(N)
1060   IF N=0 THEN RETURN "---"
1070   IF N=97 THEN RETURN "==="
1080   RETURN NT$((N-1) MOD 12)+STR$((N-1)\12)
1090 END FUNC
1100 FUNC CELL$(P,R,C)
1110   LOCAL X,N,I,V,A$
1120   X=FN IX(P,R,C)
1130   N=PN(X):I=PJ(X):V=PV(X)
1140   A$=FN NOTE$(N)+" "
1150   IF I>0 THEN A$=A$+RIGHT$("0"+STR$(I),2) ELSE A$=A$+".."
1160   IF V>=0 THEN A$=A$+" "+MID$("0123456789ABCDEF",V+1,1) ELSE A$=A$+" ."
1170   RETURN A$
1180 END FUNC
1190 ' the name of instrument N (from its definition string: NAME xxx; ...)
1200 FUNC INAME$(N)
1210   LOCAL D$,E
1220   D$=INSTRUMENT$(N)
1230   IF LEFT$(D$,5)<>"NAME " THEN RETURN "?"
1240   E=INSTR(D$,";")
1250   IF E=0 THEN E=LEN(D$)+1
1260   RETURN MID$(D$,6,E-6)
1270 END FUNC
1280 ' the I-th (0..) word of S$ (blanks), "" if none
1290 FUNC TOKN$(S$,N)
1300   LOCAL I,K,C$,T$,INW
1310   K=0:T$="":INW=0
1320   FOR I=1 TO LEN(S$)+1
1330     IF I<=LEN(S$) THEN C$=MID$(S$,I,1) ELSE C$=" "
1340     IF C$=" " OR C$=CHR$(9) THEN
1350       IF INW THEN
1360         IF K=N THEN RETURN T$
1370         K=K+1:T$="":INW=0
1380       END IF
1390     ELSE
1400       T$=T$+C$:INW=-1
1410     END IF
1420   NEXT I
1430   RETURN ""
1440 END FUNC
1450 ' is instrument I (32..63) used by a channel or a cell of the song?
1460 FUNC USES(I)
1470   LOCAL P,R,C,X
1480   FOR C=0 TO 7
1490     IF CI(C)=I THEN RETURN -1
1500   NEXT C
1510   FOR P=0 TO NPAT-1
1520     FOR R=0 TO 63
1530       FOR C=0 TO 7
1540         IF PJ(FN IX(P,R,C))=I THEN RETURN -1
1550       NEXT C
1560     NEXT R
1570   NEXT P
1580   RETURN 0
1590 END FUNC
1600 ' the index of the cell of channel C in flat row T of the song (BUILDMAP made FLO / FLR: the slot and the row of each flat row)
1610 ' the virtual row V (the rows of the song as they are played: with a loop the rows after the end of the loop count on) -> the flat row. ACTL: 0 no loop, else a loop over
1620 ' the flat rows LS .. LE-1 (LL of them: a pattern or the whole song)
1630 FUNC FV(V)
1640   IF ACTL=0 THEN RETURN V
1650   IF V<LE THEN RETURN V
1660   RETURN LS+((V-LE) MOD LL)
1670 END FUNC
1680 ' the end of the song or of the round of the loop that the virtual row V belongs to, and the first row of that round
1690 FUNC SEGEND(V)
1700   IF ACTL=0 THEN RETURN FT
1710   IF V<LE THEN RETURN LE
1720   RETURN LE+(INT((V-LE)/LL)+1)*LL
1730 END FUNC
1740 FUNC SEGST(V)
1750   IF ACTL=0 THEN RETURN 0
1760   IF V<LE THEN RETURN 0
1770   RETURN LE+INT((V-LE)/LL)*LL
1780 END FUNC
1790 FUNC CX(T,C)
1800   LOCAL F
1810   F=FN FV(T)
1820   RETURN FN IX(OD(FLO(F)),FLR(F),C)
1830 END FUNC
1840 ' the first flat row after T where channel C has a note field (a note or a note off), FT if there is none
1850 FUNC NEXTEV(C,T)
1860   LOCAL U,E
1870   U=T+1
1880   E=FN SEGEND(T)
1890   WHILE U<E
1900     IF PN(FN CX(U,C))<>0 THEN RETURN U
1910     U=U+1
1920   WEND
1930   RETURN E
1940 END FUNC
1950 ' the flat row of the note that still sounds before flat row T on channel C (the last note field is a note, not a note off), -1 if none
1960 FUNC LASTON(C,T)
1970   LOCAL U,N,B
1980   U=T-1
1990   B=FN SEGST(T)
2000   WHILE U>=B
2010     N=PN(FN CX(U,C))
2020     IF N<>0 THEN
2030       IF N>=5 AND N<=96 THEN RETURN U
2040       RETURN -1
2050     END IF
2060     U=U-1
2070   WEND
2080   RETURN -1
2090 END FUNC
2100 ' ====================================================================== the song
2110 ' does an edit of the cell (pattern P, row R, channel C) that lies in the virtual row V change what the channel has queued already?
2120 FUNC HIT(V,C,P,R)
2130   IF V<=PRL THEN RETURN 0
2140   IF V<QC(C) THEN RETURN -1
2150   IF V=QC(C) AND PN(FN IX(P,R,C))=0 THEN RETURN -1
2160   RETURN 0
2170 END FUNC
2180 PROC CLEARPAT(P)
2190   LOCAL R,C,X
2200   FOR R=0 TO 63
2210     FOR C=0 TO 7
2220       X=FN IX(P,R,C)
2230       PN(X)=0:PJ(X)=0:PV(X)=-1
2240     NEXT C
2250   NEXT R
2260 END PROC
2270 PROC NEWSONG
2280   LOCAL P,C
2290   FOR P=0 TO 31
2300     CALL CLEARPAT(P)
2310     PL(P)=32
2320   NEXT P
2330   FOR P=0 TO 63
2340     OD(P)=0
2350   NEXT P
2360   ONUM=1:NPAT=1:CO=0:CR=0:CC=0:CF=0
2370   BPM=125
2380   CI(0)=4:CI(1)=5:CI(2)=6:CI(3)=7:CI(4)=13:CI(5)=14:CI(6)=15:CI(7)=9
2390   FOR C=0 TO 7
2400     CP(C)=0:CM(C)=0
2410   NEXT C
2420   CP(0)=-30:CP(1)=30:CP(2)=-60:CP(3)=60
2430 END PROC
2440 ' ====================================================================== the music strings
2450 ' the song as one list of rows ("flat rows": the patterns of the order one after the other): FLO = the slot, FLR = the row in its pattern, FOFF = the first flat row of
2460 ' a slot, FT = the number of rows. The cells are always read from the patterns, so a change of a cell is seen at once
2470 PROC BUILDMAP
2480   LOCAL O,R,P,T
2490   T=0
2500   FOR O=0 TO ONUM-1
2510     FOFF(O)=T
2520     P=OD(O)
2530     FOR R=0 TO PL(P)-1
2540       FLO(T)=O:FLR(T)=R
2550       T=T+1
2560     NEXT R
2570   NEXT O
2580   FT=T
2590 END PROC
2600 ' K rows of rests
2610 PROC EMITREST(K)
2620   LOCAL Z,I
2630   Z=K
2640   WHILE Z>0
2650     I=0
2660     WHILE LV(I)>Z
2670       I=I+1
2680     WEND
2690     Z=Z-LV(I)
2700     AC$=AC$+" R"+LS$(I)
2710   WEND
2720 END PROC
2730 ' the open note PN2 (instrument PI2, volume PV2) for K rows: pieces joined by legato, the last one with the normal gap
2740 PROC EMITNOTE(K)
2750   LOCAL Z,I,O,S
2760   IF PI2>0 AND PI2<>CURI THEN
2770     AC$=AC$+" I"+STR$(PI2)
2780     CURI=PI2
2790   END IF
2800   IF PV2>=0 AND PV2<>CURV THEN
2810     AC$=AC$+" V"+FN NUM$(PV2/15)
2820     CURV=PV2
2830   END IF
2840   O=(PN2-1)\12
2850   S=(PN2-1) MOD 12
2860   IF O<>CURO THEN
2870     AC$=AC$+" O"+STR$(O)
2880     CURO=O
2890   END IF
2900   Z=K
2910   WHILE Z>0
2920     I=0
2930     WHILE LV(I)>Z
2940       I=I+1
2950     WEND
2960     Z=Z-LV(I)
2970     IF Z>0 THEN AC$=AC$+" ML" ELSE AC$=AC$+" MN"
2980     AC$=AC$+" "+NP$(S)+LS$(I)
2990   WEND
3000 END PROC
3010 ' the instrument BEI (0 = none) and the volume BEV (-1 = none) that are in effect before flat row T on channel C: those of the last notes that set them
3020 PROC BACKSCAN(C,T)
3030   LOCAL U,X
3040   BEI=0:BEV=-1
3050   U=T-1
3060   WHILE U>=0 AND (BEI=0 OR BEV<0)
3070     X=FN CX(U,C)
3080     IF PN(X)>=5 AND PN(X)<=96 THEN
3090       IF BEI=0 AND PJ(X)>0 THEN BEI=PJ(X)
3100       IF BEV<0 AND PV(X)>=0 THEN BEV=PV(X)
3110     END IF
3120     U=U-1
3130   WEND
3140 END PROC
3150 ' the PLAY string of channel C is made in pieces as the song plays (the playhead needs only a few rows ahead, so a change is heard soon): STARTCH writes the
3160 ' header (tempo, pan, instrument, volume as they are in effect before flat row T) and a note that is still sounding there; FILLCH adds the notes and rests from
3170 ' QC(C) on up to the flat row TG; SENDCH hands the piece to the voice (or collects it when CAP is set: the tests and Export)
3180 PROC STARTCH(C,T)
3190   LOCAL L,E
3200   CALL BACKSCAN(C,T)
3210   XI(C)=CI(C)
3220   IF BEI>0 THEN XI(C)=BEI
3230   XV(C)=12
3240   IF BEV>=0 THEN XV(C)=BEV
3250   XO(C)=-1
3260   AC$="T"+STR$(PBPM)+" MN Y"+FN NUM$(CP(C)/100)+" I"+STR$(XI(C))+" V"+FN NUM$(XV(C)/15)
3270   QC(C)=T
3280   IF PN(FN CX(T,C))=0 THEN
3290     L=FN LASTON(C,T)
3300     IF L>=0 THEN
3310       E=FN NEXTEV(C,T)
3320       PN2=PN(FN CX(L,C)):PI2=XI(C):PV2=XV(C)
3330       CURI=XI(C):CURV=XV(C):CURO=-1
3340       CALL EMITNOTE(E-T)
3350       XO(C)=CURO
3360       QC(C)=E
3370     END IF
3380   END IF
3390 END PROC
3400 PROC FILLCH(C,TG)
3410   LOCAL R,N,E,X
3420   CURI=XI(C):CURV=XV(C):CURO=XO(C)
3430   IF LEN(AC$)=0 THEN
3440     AC$="T"+STR$(PBPM)+" V"+FN NUM$(XV(C)/15)
3450     CURO=-1
3460   END IF
3470   WHILE QC(C)<VEND AND QC(C)<TG
3480     R=QC(C)
3490     X=FN CX(R,C)
3500     N=PN(X)
3510     E=FN NEXTEV(C,R)
3520     IF N>=5 AND N<=96 THEN
3530       PN2=N:PI2=PJ(X):PV2=PV(X)
3540       CALL EMITNOTE(E-R)
3550     ELSE
3560       CALL EMITREST(E-R)
3570     END IF
3580     QC(C)=E
3590   WEND
3600   XI(C)=CURI:XV(C)=CURV:XO(C)=CURO
3610 END PROC
3620 PROC SENDCH(C)
3630   LOCAL CH
3640   IF LEN(AC$)>0 THEN
3650     IF CAP THEN
3660       IF LEN(CAPS$(C))>0 THEN CAPS$(C)=CAPS$(C)+" "
3670       CAPS$(C)=CAPS$(C)+AC$
3680     ELSE
3690       CH=21+C
3700       PRINT #CH,AC$
3710     END IF
3720   END IF
3730   AC$=""
3740 END PROC
3750 ' the whole PLAY string of channel C from flat row T on (the tests and Export; BUILDMAP first)
3760 FUNC STRFROM$(C,T)
3770   CAP=-1
3780   CAPS$(C)=""
3790   PBPM=BPM
3800   AC$=""
3810   ACTL=0:VEND=FT
3820   CALL STARTCH(C,T)
3830   CALL FILLCH(C,FT)
3840   CALL SENDCH(C)
3850   CAP=0
3860   RETURN CAPS$(C)
3870 END FUNC
3880 FUNC CHSTR$(C)
3890   RETURN FN STRFROM$(C,0)
3900 END FUNC
3910 ' ====================================================================== the screen
3920 PROC SETSTATUS(T$)
3930   IF HEADLESS THEN EXIT PROC
3940   UI CAPTION 60,T$
3950 END PROC
3960 ' the order label, the instrument label ... after anything changed
3970 PROC SYNCCTL
3980   IF HEADLESS THEN EXIT PROC
3990   UI CAPTION 31,"Oct "+STR$(OCV)
4000   UI CAPTION 34,RIGHT$("0"+STR$(INS),2)+" "+FN INAME$(INS)
4010   UI CAPTION 41,"Slot "+STR$(CO+1)+"/"+STR$(ONUM)+"  Pattern "+STR$(OD(CO))+"  Len "+STR$(PL(OD(CO)))
4020   UI CAPTION 37,"Step "+STR$(STP)
4030   CALL UK_SET(91,CP(CC))
4040   UI VALUE 94,CM(CC)
4050   UI CAPTION 95,"Channel "+STR$(CC+1)+" ("+FN INAME$(CI(CC))+")"
4060 END PROC
4070 ' the colors of the grid: the palette entries 240..255 of the UI's layer (the program's own are put back at the end)
4080 PROC SETPAL
4090   LOCAL I
4100   IF HEADLESS THEN EXIT PROC
4110   FOR I=0 TO 15
4120     SPL(I)=GFX("PALETTE",3,240+I)
4130   NEXT I
4140   GFX PALETTE 3,240,RGB(250,250,246)
4150   GFX PALETTE 3,241,RGB(228,236,248)
4160   GFX PALETTE 3,242,RGB(208,222,242)
4170   GFX PALETTE 3,243,RGB(255,232,150)
4180   GFX PALETTE 3,244,RGB(30,80,160)
4190   GFX PALETTE 3,245,RGB(255,255,255)
4200   GFX PALETTE 3,246,RGB(10,60,140)
4210   GFX PALETTE 3,247,RGB(130,40,160)
4220   GFX PALETTE 3,248,RGB(10,120,60)
4230   GFX PALETTE 3,249,RGB(176,90,0)
4240   GFX PALETTE 3,250,RGB(176,182,192)
4250   GFX PALETTE 3,251,RGB(192,32,32)
4260   GFX PALETTE 3,252,RGB(100,110,128)
4270   GFX PALETTE 3,253,RGB(58,66,86)
4280   GFX PALETTE 3,254,RGB(240,240,240)
4290   GFX PALETTE 3,255,RGB(128,48,48)
4300 END PROC
4310 PROC RESTOREPAL
4320   LOCAL I
4330   IF HEADLESS THEN EXIT PROC
4340   FOR I=0 TO 15
4350     GFX PALETTE 3,240+I,SPL(I)
4360   NEXT I
4370 END PROC
4380 ' the grid: the pattern of the slot under the cursor, the cursor row in the middle; every field has its color (note blue / purple, off red, instrument green,
4390 ' volume orange, empty fields grey, a muted channel grey), every 4th row has a tinted background, every 16th a stronger one, the row that plays is yellow
4400 PROC REDRAW
4410   LOCAL G,R,P,C,X,N,I,V,BG,FG,PB,HB,N$,I$,V$,M$,L$,CL
4420   IF HEADLESS THEN EXIT PROC
4430   P=OD(CO)
4440   UI GRIDPUT 50,0,0,"Row",254,253
4450   FOR C=0 TO 7
4460     N$=FN INAME$(CI(C))+"       "
4470     HB=253
4480     M$=" "
4490     IF CM(C)<>0 THEN
4500       HB=255:M$="m"
4510     END IF
4520     UI GRIDPUT 50,3+C*9,0," "+STR$(C+1)+M$+LEFT$(N$,5)+" ",254,HB
4530   NEXT C
4540   FOR G=1 TO GRH-1
4550     R=CR+G-CENTER
4560     IF R<0 OR R>=PL(P) THEN
4570       UI GRIDPUT 50,0,G,SPACE$(COLS),250,240
4580     ELSE
4590       BG=240
4600       IF R MOD 4=0 THEN BG=241
4610       IF R MOD 16=0 THEN BG=242
4620       IF PLG THEN
4630         IF CO=PCO AND R=PHROW THEN BG=243
4640       END IF
4650       UI GRIDPUT 50,0,G,RIGHT$("0"+STR$(R),2)+" ",252,BG
4660       FOR C=0 TO 7
4670         CL=CM(C)<>0
4680         X=FN IX(P,R,C)
4690         N=PN(X):I=PJ(X):V=PV(X)
4700         N$=FN NOTE$(N)
4710         FG=246
4720         IF N=0 THEN FG=250
4730         IF N=97 THEN FG=251
4740         IF N>0 AND N<97 THEN
4750           IF INSTR(N$,"#")>0 THEN FG=247
4760         END IF
4770         IF CL THEN FG=250
4780         UI GRIDPUT 50,3+C*9,G,N$,FG,BG
4790         I$=" .."
4800         FG=250
4810         IF I>0 THEN
4820           I$=" "+RIGHT$("0"+STR$(I),2)
4830           IF NOT CL THEN FG=248
4840         END IF
4850         UI GRIDPUT 50,6+C*9,G,I$,FG,BG
4860         V$=" ."
4870         FG=250
4880         IF V>=0 THEN
4890           V$=" "+MID$("0123456789ABCDEF",V+1,1)
4900           IF NOT CL THEN FG=249
4910         END IF
4920         UI GRIDPUT 50,9+C*9,G,V$,FG,BG
4930         IF C<7 THEN UI GRIDPUT 50,11+C*9,G,"|",250,BG
4940       NEXT C
4950     END IF
4960   NEXT G
4970   L$=FN CELL$(P,CR,CC)
4980   UI GRIDPUT 50,3+CC*9+FO(CF),CENTER,MID$(L$,FO(CF)+1,FL(CF)),245,244
4990 END PROC
5000 ' ====================================================================== playing
5010 PROC STOPPLAY
5020   SOUND OFF
5030   IF PLG THEN
5040     PLG=0
5050     CALL REDRAW
5060     CALL SETSTATUS("Stopped.")
5070   END IF
5080 END PROC
5090 ' the song is played from flat row T: BUILDMAP (the song as it is now), every channel is queued anew
5100 PROC FULLSYNC(O0,R0)
5110   LOCAL C,T
5120   CALL BUILDMAP
5130   IF O0>=ONUM THEN
5140     O0=ONUM-1:R0=0
5150   END IF
5160   IF R0>=PL(OD(O0)) THEN R0=PL(OD(O0))-1
5170   T=FOFF(O0)+R0
5180   ACTL=0:VEND=FT
5190   IF LOOPM=1 THEN
5200     ACTL=-1:LS=FOFF(O0):LE=LS+PL(OD(O0)):LL=LE-LS:VEND=1000000000
5210   END IF
5220   IF LOOPM=2 THEN
5230     ACTL=-1:LS=0:LE=FT:LL=FT:VEND=1000000000
5240   END IF
5250   SOUND OFF
5260   PBPM=BPM
5270   PS0=T:PT0=TI:PRL=T
5280   RSA=0
5290   PCO=O0:PHROW=R0
5300   FOR C=0 TO 7
5310     RS(C)=0
5320     AC$=""
5330     IF CM(C)=0 THEN
5340       CALL STARTCH(C,T)
5350       CALL FILLCH(C,T+LOOK)
5360       CALL SENDCH(C)
5370     END IF
5380   NEXT C
5390 END PROC
5400 ' one channel is queued anew from flat row T (after a change in what is queued already)
5410 PROC CHSYNC(C,T)
5420   RS(C)=0
5430   SOUND OFF C+1
5440   AC$=""
5450   CALL STARTCH(C,T)
5460   CALL FILLCH(C,T+LOOK)
5470   CALL SENDCH(C)
5480 END PROC
5490 PROC STARTPLAY(SO,SR)
5500   LOCAL L$
5510   CALL FULLSYNC(SO,SR)
5520   PLG=-1
5530   CALL REDRAW
5540   L$="Playing"
5550   IF LOOPM=1 THEN L$="Playing (looping the pattern)"
5560   IF LOOPM=2 THEN L$="Playing (looping the song)"
5570   CALL SETSTATUS(L$+": changes are heard as the song goes on.")
5580 END PROC
5590 ' called every frame: the playhead (from the clock), the channels that must be queued anew, the next notes
5600 PROC PLAYTICK
5610   LOCAL R,C,F
5620   IF NOT PLG THEN EXIT PROC
5630   R=PS0+INT((TI-PT0)*PBPM*4/3600)
5640   IF ACTL=0 AND R>=FT THEN
5650     CALL STOPPLAY
5660     EXIT PROC
5670   END IF
5680   IF R<>PRL THEN
5690     IF R-PRL>3 THEN RSA=-1
5700     F=FN FV(R)
5710     IF RSA THEN
5720       CALL FULLSYNC(FLO(F),FLR(F))
5730     ELSE
5740       PRL=R
5750       PCO=FLO(F):PHROW=FLR(F)
5760       FOR C=0 TO 7
5770         IF RS(C) AND CM(C)=0 THEN CALL CHSYNC(C,R)
5780         RS(C)=0
5790       NEXT C
5800     END IF
5810     IF FOLLOW THEN
5820       CO=PCO:CR=PHROW
5830       CALL SYNCCTL
5840     END IF
5850     CALL REDRAW
5860   END IF
5870   IF RSA THEN EXIT PROC
5880   FOR C=0 TO 7
5890     IF CM(C)=0 AND RS(C)=0 THEN
5900       IF QC(C)<PRL+LOOK AND QC(C)<VEND THEN
5910         AC$=""
5920         CALL FILLCH(C,PRL+LOOK)
5930         CALL SENDCH(C)
5940       END IF
5950     END IF
5960   NEXT C
5970 END PROC
5980 ' an edit of the cell (pattern P, row R, channel C) while the song plays: if the cell lies in what the channel has queued already, it is queued anew at the next row
5990 PROC TOUCH(P,R,C)
6000   LOCAL O,T,V,J,E
6010   IF NOT PLG THEN EXIT PROC
6020   E=FT
6030   IF ACTL<>0 THEN E=LE
6040   FOR O=0 TO ONUM-1
6050     IF OD(O)=P THEN
6060       T=FOFF(O)+R
6070       IF T<E THEN
6080         IF FN HIT(T,C,P,R) THEN RS(C)=-1
6090       END IF
6100       IF ACTL<>0 THEN
6110         IF T>=LS AND T<LE THEN
6120           V=LE+(T-LS)
6130           IF V<=PRL THEN
6140             J=INT((PRL-V)/LL)+1
6150             V=V+J*LL
6160           END IF
6170           IF FN HIT(V,C,P,R) THEN RS(C)=-1
6180         END IF
6190       END IF
6200     END IF
6210   NEXT O
6220 END PROC
6230 ' a change of the song's order, lengths or tempo while it plays: everything is queued anew at the next row
6240 PROC GLOBALEDIT
6250   IF PLG THEN RSA=-1
6260 END PROC
6270 ' the frequency of tracker note N (C-4 = 49 = middle C)
6280 FUNC NOTEF(N)
6290   RETURN 261.626*(2^((N-49)/12))
6300 END FUNC
6310 PROC PREVIEW(N,I)
6320   LOCAL W
6330   IF PLG THEN EXIT PROC
6340   IF N<5 OR N>96 THEN EXIT PROC
6350   W=I
6360   IF W=0 THEN W=INS
6370   WAVE CC+1,W
6380   SOUND CC+1,FN NOTEF(N),0.5,0.6
6390 END PROC
6400 ' ====================================================================== editing
6410 PROC GOROW(R)
6420   CR=FN CLAMP(R,0,PL(OD(CO))-1)
6430 END PROC
6440 PROC SETOCT(V)
6450   OCV=FN CLAMP(V,1,6)
6460 END PROC
6470 PROC SETINS(V)
6480   INS=FN CLAMP(V,4,63)
6490 END PROC
6500 ' the note of a piano key (0 if the key is none): Z S X D C V G B H N J M in the octave, Q 2 W 3 E R 5 T 6 Y 7 U I above it
6510 FUNC PIANO(K)
6520   LOCAL C$,P
6530   IF K<32 OR K>126 THEN RETURN 0
6540   C$=CHR$(K)
6550   IF C$>="A" AND C$<="Z" THEN C$=CHR$(K+32)
6560   P=INSTR("zsxdcvgbhnjm",C$)
6570   IF P>0 THEN RETURN (OCV*12)+P
6580   P=INSTR("q2w3er5t6y7ui",C$)
6590   IF P>0 THEN RETURN ((OCV+1)*12)+P
6600   RETURN 0
6610 END FUNC
6620 ' puts a note into the cursor cell (the instrument is the current one), plays it, goes down by the step
6630 PROC PUTNOTE(N)
6640   LOCAL X
6650   X=FN IX(OD(CO),CR,CC)
6660   PN(X)=N
6670   IF N>=5 AND N<=96 THEN
6680     PJ(X)=INS
6690     CALL PREVIEW(N,INS)
6700   END IF
6710   IF N=97 THEN
6720     PJ(X)=0:PV(X)=-1
6730   END IF
6740   CALL TOUCH(OD(CO),CR,CC)
6750   CALL GOROW(CR+STP)
6760 END PROC
6770 PROC TYPEDIGIT(D)
6780   LOCAL X,I
6790   X=FN IX(OD(CO),CR,CC)
6800   IF CF=1 THEN
6810     I=D*10+(PJ(X) MOD 10)
6820     IF I>63 THEN I=63
6830     PJ(X)=I
6840     CALL TOUCH(OD(CO),CR,CC)
6850     CF=2
6860   ELSE
6870     IF CF=2 THEN
6880       I=(PJ(X)\10)*10+D
6890       IF I>63 THEN I=63
6900       PJ(X)=I
6910       CALL TOUCH(OD(CO),CR,CC)
6920       CF=3
6930     END IF
6940   END IF
6950 END PROC
6960 PROC TYPEVOL(V)
6970   LOCAL X
6980   X=FN IX(OD(CO),CR,CC)
6990   PV(X)=V
7000   CALL TOUCH(OD(CO),CR,CC)
7010   CF=0
7020   CALL GOROW(CR+STP)
7030 END PROC
7040 PROC CLEARFIELD
7050   LOCAL X
7060   X=FN IX(OD(CO),CR,CC)
7070   IF CF=0 THEN
7080     PN(X)=0
7090   ELSE
7100     IF CF=3 THEN
7110       PV(X)=-1
7120     ELSE
7130       PJ(X)=0
7140     END IF
7150   END IF
7160   CALL TOUCH(OD(CO),CR,CC)
7170 END PROC
7180 ' one field to the right / left, over the channel borders
7190 PROC MOVEF(D)
7200   CF=CF+D
7210   IF CF>3 THEN
7220     IF CC<7 THEN
7230       CC=CC+1:CF=0
7240     ELSE
7250       CF=3
7260     END IF
7270   END IF
7280   IF CF<0 THEN
7290     IF CC>0 THEN
7300       CC=CC-1:CF=3
7310     ELSE
7320       CF=0
7330     END IF
7340   END IF
7350 END PROC
7360 PROC KEYIN(K)
7370   CALL KEYACT(K)
7380   CALL SYNCCTL
7390   CALL REDRAW
7400 END PROC
7410 PROC KEYACT(K)
7420   LOCAL M,N,V
7430   M=K AND 65536
7440   K=K AND 65535
7450   IF K=4097 THEN
7460     IF M<>0 THEN
7470       CC=FN CLAMP(CC-1,0,7):CF=0
7480     ELSE
7490       CALL MOVEF(-1)
7500     END IF
7510     EXIT PROC
7520   END IF
7530   IF K=4098 THEN
7540     IF M<>0 THEN
7550       CC=FN CLAMP(CC+1,0,7):CF=0
7560     ELSE
7570       CALL MOVEF(1)
7580     END IF
7590     EXIT PROC
7600   END IF
7610   IF K=4099 THEN
7620     CALL GOROW(CR-1)
7630     EXIT PROC
7640   END IF
7650   IF K=4100 THEN
7660     CALL GOROW(CR+1)
7670     EXIT PROC
7680   END IF
7690   IF K=4103 THEN
7700     CALL GOROW(CR-16)
7710     EXIT PROC
7720   END IF
7730   IF K=4104 THEN
7740     CALL GOROW(CR+16)
7750     EXIT PROC
7760   END IF
7770   IF K=4101 THEN
7780     CALL GOROW(0)
7790     EXIT PROC
7800   END IF
7810   IF K=4102 THEN
7820     CALL GOROW(99)
7830     EXIT PROC
7840   END IF
7850   IF K=4105 OR K=8 THEN
7860     CALL CLEARFIELD
7870     EXIT PROC
7880   END IF
7890   IF K=13 THEN
7900     CALL PLAYROW
7910     EXIT PROC
7920   END IF
7930   IF K=32 THEN
7940     IF PLG THEN
7950       CALL STOPPLAY
7960     ELSE
7970       CALL STARTPLAY(CO,CR)
7980     END IF
7990     EXIT PROC
8000   END IF
8010   IF K=45 THEN
8020     CALL SETOCT(OCV-1)
8030     EXIT PROC
8040   END IF
8050   IF K=43 THEN
8060     CALL SETOCT(OCV+1)
8070     EXIT PROC
8080   END IF
8090   IF K=91 THEN
8100     CALL SETINS(INS-1)
8110     EXIT PROC
8120   END IF
8130   IF K=93 THEN
8140     CALL SETINS(INS+1)
8150     EXIT PROC
8160   END IF
8170   IF CF=0 THEN
8180     IF K=49 THEN
8190       CALL PUTNOTE(97)
8200     ELSE
8210       N=FN PIANO(K)
8220       IF N>0 THEN CALL PUTNOTE(N)
8230     END IF
8240     EXIT PROC
8250   END IF
8260   IF CF=3 THEN
8270     IF K<128 THEN
8280       V=INSTR("0123456789abcdef",CHR$(K+32*(K>=65 AND K<=70)*-1))
8290       IF V>0 THEN CALL TYPEVOL(V-1)
8300     END IF
8310     EXIT PROC
8320   END IF
8330   IF K>=48 AND K<=57 THEN CALL TYPEDIGIT(K-48)
8340 END PROC
8350 ' the row under the cursor: all its notes at once
8360 PROC PLAYROW
8370   LOCAL C,X,N
8380   IF PLG THEN EXIT PROC
8390   FOR C=0 TO 7
8400     X=FN IX(OD(CO),CR,C)
8410     N=PN(X)
8420     IF N>=5 AND N<=96 AND CM(C)=0 THEN
8430       IF PJ(X)>0 THEN WAVE C+1,PJ(X) ELSE WAVE C+1,CI(C)
8440       SOUND C+1,FN NOTEF(N),0.5,0.6
8450     END IF
8460   NEXT C
8470 END PROC
8480 ' a click on a cell of the grid: V = row * COLS + column
8490 PROC CLICKIN(V)
8500   LOCAL G,C,R,W
8510   G=V\COLS
8520   C=V MOD COLS
8530   IF G<1 THEN EXIT PROC
8540   CALL GOROW(CR+G-CENTER)
8550   IF C>=3 THEN
8560     W=(C-3)\9
8570     IF W>7 THEN W=7
8580     CC=W
8590     R=(C-3)-W*9
8600     CF=0
8610     IF R=4 THEN CF=1
8620     IF R=5 THEN CF=2
8630     IF R>=7 THEN CF=3
8640   END IF
8650   CALL SYNCCTL
8660   CALL REDRAW
8670 END PROC
8680 ' ====================================================================== the order
8690 PROC SETCO(V)
8700   CO=FN CLAMP(V,0,ONUM-1)
8710   CR=FN CLAMP(CR,0,PL(OD(CO))-1)
8720 END PROC
8730 PROC NEWPATTERN
8740   IF NPAT>=32 THEN
8750     CALL SETSTATUS("There are 32 patterns already.")
8760     EXIT PROC
8770   END IF
8780   CALL CLEARPAT(NPAT)
8790   PL(NPAT)=32
8800   OD(CO)=NPAT
8810   NPAT=NPAT+1
8820   CR=0
8830   CALL GLOBALEDIT
8840 END PROC
8850 PROC CLONEPATTERN
8860   LOCAL R,C,X,Y,P
8870   IF NPAT>=32 THEN
8880     CALL SETSTATUS("There are 32 patterns already.")
8890     EXIT PROC
8900   END IF
8910   P=OD(CO)
8920   FOR R=0 TO 63
8930     FOR C=0 TO 7
8940       X=FN IX(P,R,C):Y=FN IX(NPAT,R,C)
8950       PN(Y)=PN(X):PJ(Y)=PJ(X):PV(Y)=PV(X)
8960     NEXT C
8970   NEXT R
8980   PL(NPAT)=PL(P)
8990   OD(CO)=NPAT
9000   NPAT=NPAT+1
9010   CALL GLOBALEDIT
9020 END PROC
9030 PROC INSSLOT
9040   LOCAL I
9050   IF ONUM>=64 THEN EXIT PROC
9060   FOR I=ONUM TO CO+2 STEP -1
9070     OD(I)=OD(I-1)
9080   NEXT I
9090   OD(CO+1)=OD(CO)
9100   ONUM=ONUM+1
9110   CO=CO+1
9120   CALL GLOBALEDIT
9130 END PROC
9140 PROC DELSLOT
9150   LOCAL I
9160   IF ONUM<=1 THEN EXIT PROC
9170   FOR I=CO TO ONUM-2
9180     OD(I)=OD(I+1)
9190   NEXT I
9200   ONUM=ONUM-1
9210   CALL SETCO(CO)
9220   CALL GLOBALEDIT
9230 END PROC
9240 PROC CYCLELEN
9250   LOCAL P,L
9260   P=OD(CO)
9270   L=PL(P)+16
9280   IF L>64 THEN L=16
9290   PL(P)=L
9300   CALL GOROW(CR)
9310   CALL GLOBALEDIT
9320 END PROC
9330 ' ====================================================================== files
9340 ' the song as text: TRACKER 1, BPM, CHANNEL lines, ORDER, the instruments 32..63 that the song uses, PATTERN lines and one C line (row channel note instrument volume) per used cell
9350 PROC SAVEF(F$)
9360   LOCAL P,R,C,X,I,L$
9370   OPEN F$ FOR OUTPUT AS #1
9380   PRINT #1,"TRACKER 1"
9390   PRINT #1,"BPM "+STR$(BPM)
9400   FOR C=0 TO 7
9410     PRINT #1,"CHANNEL "+STR$(C)+" "+STR$(CI(C))+" "+STR$(CP(C))+" "+STR$(CM(C)<>0)
9420   NEXT C
9430   L$="ORDER"
9440   FOR I=0 TO ONUM-1
9450     L$=L$+" "+STR$(OD(I))
9460   NEXT I
9470   PRINT #1,L$
9480   FOR I=32 TO 63
9490     IF FN USES(I) THEN PRINT #1,"INSTRUMENT "+STR$(I)+" "+INSTRUMENT$(I)
9500   NEXT I
9510   FOR P=0 TO NPAT-1
9520     PRINT #1,"PATTERN "+STR$(P)+" "+STR$(PL(P))
9530     FOR R=0 TO 63
9540       FOR C=0 TO 7
9550         X=FN IX(P,R,C)
9560         IF PN(X)<>0 OR PJ(X)<>0 OR PV(X)>=0 THEN PRINT #1,"C "+STR$(R)+" "+STR$(C)+" "+STR$(PN(X))+" "+STR$(PJ(X))+" "+STR$(PV(X))
9570       NEXT C
9580     NEXT R
9590   NEXT P
9600   PRINT #1,"END"
9610   CLOSE #1
9620   CALL SETSTATUS("Saved "+F$+".")
9630 END PROC
9640 PROC OPENF(F$)
9650   LOCAL L$,W$,P,N,C,X,A,B,D$,K
9660   OPEN F$ FOR INPUT AS #1
9670   CALL NEWSONG
9680   ONUM=0:NPAT=0
9690   P=0
9700   WHILE EOF(1)=0
9710     LINE INPUT #1,L$
9720     W$=FN TOKN$(L$,0)
9730     IF W$="BPM" THEN BPM=FN CLAMP(INT(VAL(FN TOKN$(L$,1))),30,300)
9740     IF W$="CHANNEL" THEN
9750       C=FN CLAMP(INT(VAL(FN TOKN$(L$,1))),0,7)
9760       CI(C)=FN CLAMP(INT(VAL(FN TOKN$(L$,2))),4,63)
9770       CP(C)=FN CLAMP(INT(VAL(FN TOKN$(L$,3))),-100,100)
9780       CM(C)=0
9790       IF VAL(FN TOKN$(L$,4))<>0 THEN CM(C)=-1
9800     END IF
9810     IF W$="ORDER" THEN
9820       ONUM=0
9830       FOR K=1 TO 64
9840         D$=FN TOKN$(L$,K)
9850         IF D$<>"" THEN
9860           OD(ONUM)=FN CLAMP(INT(VAL(D$)),0,31)
9870           ONUM=ONUM+1
9880         END IF
9890       NEXT K
9900     END IF
9910     IF W$="INSTRUMENT" THEN
9920       N=INT(VAL(FN TOKN$(L$,1)))
9930       IF N>=4 AND N<=63 THEN
9940         D$=MID$(L$,INSTR(INSTR(L$," ")+1,L$," ")+1)
9950         INSTRUMENT N,D$
9960       END IF
9970     END IF
9980     IF W$="PATTERN" THEN
9990       P=FN CLAMP(INT(VAL(FN TOKN$(L$,1))),0,31)
10000       PL(P)=FN CLAMP(INT(VAL(FN TOKN$(L$,2))),16,64)
10010       IF P+1>NPAT THEN NPAT=P+1
10020     END IF
10030     IF W$="C" THEN
10040       A=INT(VAL(FN TOKN$(L$,1))):C=INT(VAL(FN TOKN$(L$,2)))
10050       IF A>=0 AND A<64 AND C>=0 AND C<8 THEN
10060         X=FN IX(P,A,C)
10070         PN(X)=FN CLAMP(INT(VAL(FN TOKN$(L$,3))),0,97)
10080         PJ(X)=FN CLAMP(INT(VAL(FN TOKN$(L$,4))),0,63)
10090         PV(X)=FN CLAMP(INT(VAL(FN TOKN$(L$,5))),-1,15)
10100       END IF
10110     END IF
10120   WEND
10130   CLOSE #1
10140   IF ONUM=0 THEN
10150     ONUM=1:OD(0)=0
10160   END IF
10170   IF NPAT=0 THEN NPAT=1
10180   FOR K=0 TO ONUM-1
10190     IF OD(K)>=NPAT THEN OD(K)=0
10200   NEXT K
10210   CO=0:CR=0:CC=0:CF=0
10220   CALL TOCONTROLS
10230   CALL SYNCCTL
10240   CALL REDRAW
10250   CALL SETSTATUS("Opened "+F$+".")
10260 END PROC
10270 ' the PLAY lines of the whole song, one per call: CALLBACK-less, written to the file #1 or collected into the global list XL$ (for Copy)
10280 PROC EXPORTLINES(TOCLIP)
10290   LOCAL C,I,N,L,K,A$,B$,P,XN
10300   CALL BUILDMAP
10310   XL$=""
10320   XN=10
10330   XL$=XL$+STR$(XN)+" ' song made with the tracker of Engine1999 ("+STR$(BPM)+" beats a minute)"+CHR$(10)
10340   XN=XN+10
10350   FOR I=32 TO 63
10360     IF FN USES(I) THEN
10370       XL$=XL$+STR$(XN)+" INSTRUMENT "+STR$(I)+","+CHR$(34)+INSTRUMENT$(I)+CHR$(34)+CHR$(10)
10380       XN=XN+10
10390     END IF
10400   NEXT I
10410   FOR C=0 TO 7
10420     A$=FN CHSTR$(C)
10430     IF CM(C)<>0 THEN A$=""
10440     K=0
10450     WHILE LEN(A$)>0
10460       L=150
10470       IF LEN(A$)<=L THEN
10480         L=LEN(A$)
10490       ELSE
10500         WHILE L>1 AND MID$(A$,L+1,1)<>" "
10510           L=L-1
10520         WEND
10530       END IF
10540       B$=LEFT$(A$,L)
10550       A$=MID$(A$,L+1)
10560       IF K=0 THEN
10570         XL$=XL$+STR$(XN)+" V"+STR$(C+1)+"$="+CHR$(34)+B$+CHR$(34)+CHR$(10)
10580       ELSE
10590         XL$=XL$+STR$(XN)+" V"+STR$(C+1)+"$=V"+STR$(C+1)+"$+"+CHR$(34)+B$+CHR$(34)+CHR$(10)
10600       END IF
10610       XN=XN+10
10620       K=K+1
10630     WEND
10640     IF K=0 THEN
10650       XL$=XL$+STR$(XN)+" V"+STR$(C+1)+"$="+CHR$(34)+CHR$(34)+CHR$(10)
10660       XN=XN+10
10670     END IF
10680   NEXT C
10690   XL$=XL$+STR$(XN)+" PLAY V1$,V2$,V3$,V4$,V5$,V6$,V7$,V8$"+CHR$(10)
10700 END PROC
10710 PROC EXPORTF(F$)
10720   CALL EXPORTLINES(0)
10730   OPEN F$ FOR OUTPUT AS #1
10740   PRINT #1,XL$;
10750   CLOSE #1
10760   CALL SETSTATUS("Exported "+F$+" (MERGE or LOAD it, RUN plays it).")
10770 END PROC
10780 ' ====================================================================== the form
10790 PROC MAKEFORM
10800   UI PANEL 1,0,0,640,300,"Tracker"
10810   UI BUTTON 10,10,22,50,20,"Song"
10820   UI BUTTON 11,64,22,66,20,"Pattern"
10830   UI BUTTON 12,134,22,46,20,"Here"
10840   UI BUTTON 13,184,22,46,20,"Stop"
10850   CALL UK_SLIDER(20,246,22,310,"Tempo",40,240,BPM)
10860   UI BUTTON 75,570,22,60,20,"Quit"
10870   UI BUTTON 30,10,46,20,20,"-"
10880   UI LABEL 31,34,48,40,14,""
10890   UI BUTTON 32,78,46,20,20,"+"
10900   UI BUTTON 33,106,46,20,20,"<"
10910   UI LABEL 34,130,48,80,14,""
10920   UI BUTTON 35,214,46,20,20,">"
10930   UI BUTTON 37,242,46,58,20,"Step 1"
10940   UI CHECK 96,308,48,68,16,"Follow"
10950   UI VALUE 96,-1
10960   UI RADIO 97,384,48,52,16,"Once",11
10970   UI RADIO 98,440,48,84,16,"Loop pat",11
10980   UI RADIO 99,528,48,92,16,"Loop song",11
10990   UI VALUE 97,-1
11000   UI GRID 50,10,70,COLS,GRH
11010   UI TAB 80,10,234,300,22,"Order|Channel|File"
11020   UI BUTTON 40,10,260,20,20,"<"
11030   UI LABEL 41,34,262,230,14,""
11040   UI BUTTON 42,268,260,20,20,">"
11050   UI BUTTON 43,296,260,48,20,"Pat<"
11060   UI BUTTON 44,348,260,48,20,"Pat>"
11070   UI BUTTON 45,400,260,44,20,"New"
11080   UI BUTTON 46,448,260,52,20,"Clone"
11090   UI BUTTON 47,504,260,40,20,"Ins"
11100   UI BUTTON 48,548,260,40,20,"Del"
11110   UI BUTTON 49,592,260,38,20,"Len"
11120   UI CHECK 94,10,262,80,16,"Mute"
11130   CALL UK_SLIDER(91,96,262,300,"Pan",-100,100,0)
11140   UI BUTTON 36,404,260,70,20,"Ch = Ins"
11150   UI LABEL 95,480,262,150,14,""
11160   UI BUTTON 70,10,260,60,20,"Save"
11170   UI BUTTON 71,74,260,60,20,"Open"
11180   UI BUTTON 72,138,260,66,20,"Export"
11190   UI BUTTON 73,208,260,50,20,"Copy"
11200   UI BUTTON 74,262,260,54,20,"Clear"
11210   UI LABEL 60,10,284,620,14,""
11220   CALL SHOWPAGE(0)
11230   UI FOCUS 50
11240 END PROC
11250 ' the tab pane under the grid: page 0 Order (slots and patterns), 1 the channel (Mute, Pan, start instrument), 2 files
11260 PROC SHOWPAGE(N)
11270   LOCAL I
11280   FOR I=40 TO 49
11290     UI SHOW I,(N=0)
11300   NEXT I
11310   FOR I=91 TO 95
11320     UI SHOW I,(N=1)
11330   NEXT I
11340   UI SHOW 36,(N=1)
11350   FOR I=70 TO 74
11360     UI SHOW I,(N=2)
11370   NEXT I
11380 END PROC
11390 ' the controls that show the song (after Open / Clear)
11400 PROC TOCONTROLS
11410   IF HEADLESS THEN EXIT PROC
11420   CALL UK_SET(20,BPM)
11430 END PROC
11440 ' ====================================================================== what happens
11450 PROC REACT(E,K,V)
11460   LOCAL F$
11470   IF E=50 AND K=9 THEN CALL KEYIN(V)
11480   IF E=50 AND K=10 THEN CALL CLICKIN(V)
11490   IF FN UK_TRACK(E,V) THEN
11500     IF E=20 THEN
11510       BPM=V
11520       IF PLG AND BPM<>PBPM THEN RSA=-1
11530     END IF
11540     IF E=91 THEN
11550       CP(CC)=V
11560       IF PLG THEN RS(CC)=-1
11570       CALL REDRAW
11580     END IF
11590   END IF
11600   IF E=94 AND K=2 THEN
11610     CM(CC)=V
11620     IF V<>0 THEN CM(CC)=-1
11630     IF PLG THEN
11640       IF CM(CC)<>0 THEN
11650         SOUND OFF CC+1
11660       ELSE
11670         RS(CC)=-1
11680       END IF
11690     END IF
11700     CALL REDRAW
11710     UI FOCUS 50
11720   END IF
11730   IF E=80 AND K=2 THEN
11740     CALL SHOWPAGE(V)
11750     UI FOCUS 50
11760   END IF
11770   IF E>=97 AND E<=99 AND K=2 THEN
11780     IF V<>0 THEN
11790       LOOPM=E-97
11800       IF PLG THEN RSA=-1
11810     END IF
11820     UI FOCUS 50
11830   END IF
11840   IF E=96 AND K=2 THEN
11850     FOLLOW=V<>0
11860     UI FOCUS 50
11870   END IF
11880   IF K=1 THEN
11890     IF E=10 THEN CALL STARTPLAY(0,0)
11900     IF E=11 THEN CALL STARTPLAY(CO,0)
11910     IF E=12 THEN CALL STARTPLAY(CO,CR)
11920     IF E=13 THEN CALL STOPPLAY
11930     IF E=30 THEN CALL SETOCT(OCV-1)
11940     IF E=32 THEN CALL SETOCT(OCV+1)
11950     IF E=33 THEN CALL SETINS(INS-1)
11960     IF E=35 THEN CALL SETINS(INS+1)
11970     IF E=36 THEN
11980       CI(CC)=INS
11990       IF PLG THEN RS(CC)=-1
12000       CALL SETSTATUS("Channel "+STR$(CC+1)+" starts with instrument "+STR$(INS)+".")
12010     END IF
12020     IF E=37 THEN
12030       STP=STP+1
12040       IF STP>4 THEN STP=0
12050     END IF
12060     IF E=40 THEN CALL SETCO(CO-1)
12070     IF E=42 THEN CALL SETCO(CO+1)
12080     IF E=43 THEN
12090       OD(CO)=FN CLAMP(OD(CO)-1,0,NPAT-1)
12100       CALL GOROW(CR)
12110       CALL GLOBALEDIT
12120     END IF
12130     IF E=44 THEN
12140       OD(CO)=FN CLAMP(OD(CO)+1,0,NPAT-1)
12150       CALL GOROW(CR)
12160       CALL GLOBALEDIT
12170     END IF
12180     IF E=45 THEN CALL NEWPATTERN
12190     IF E=46 THEN CALL CLONEPATTERN
12200     IF E=47 THEN CALL INSSLOT
12210     IF E=48 THEN CALL DELSLOT
12220     IF E=49 THEN CALL CYCLELEN
12230     IF E=70 THEN
12240       F$=FN FR_PICK$("Save the song as",DR$,"*.trk;*.txt",-1)
12250       DR$=FR_DIR$
12260       IF F$<>"" THEN CALL SAVEF(F$)
12270     END IF
12280     IF E=71 THEN
12290       F$=FN FR_PICK$("Open a song",DR$,"*.trk;*.txt",0)
12300       DR$=FR_DIR$
12310       IF F$<>"" THEN
12320         CALL STOPPLAY
12330         CALL OPENF(F$)
12340       END IF
12350     END IF
12360     IF E=72 THEN
12370       F$=FN FR_PICK$("Export as BASIC lines",DR$,"*.bas",-1)
12380       DR$=FR_DIR$
12390       IF F$<>"" THEN CALL EXPORTF(F$)
12400     END IF
12410     IF E=73 THEN
12420       CALL EXPORTLINES(-1)
12430       CLIPBOARD XL$
12440       CALL SETSTATUS("The PLAY lines are on the clipboard.")
12450     END IF
12460     IF E=74 THEN
12470       IF FN UK_ASK("Clear","Throw the song away and start a new one?") THEN
12480         CALL STOPPLAY
12490         CALL NEWSONG
12500         CALL TOCONTROLS
12510       END IF
12520     END IF
12530     IF E=75 THEN Q=1
12540     CALL SYNCCTL
12550     CALL REDRAW
12560     UI FOCUS 50
12570   END IF
12580 END PROC
12590 ' ====================================================================== the self-test (TRACKER_HEADLESS=1 in the console host: tests the song / string code, no window)
12600 PROC CHECK(OK,T$)
12610   IF OK=0 THEN
12620     PRINT "FAIL: ";T$
12630     BAD=BAD+1
12640   END IF
12650   NCHK=NCHK+1
12660 END PROC
12670 PROC SELFTEST
12680   LOCAL X,Y,A$,B$,I
12690   HEADLESS=-1
12700   BAD=0:NCHK=0
12710   CALL NEWSONG
12720   CALL CHECK(FN NOTE$(49)="C-4" AND FN NOTE$(40)="D#3" AND FN NOTE$(0)="---" AND FN NOTE$(97)="===" AND FN NOTE$(96)="B-7","note names")
12730   OCV=3
12740   CALL CHECK(FN PIANO(ASC("z"))=37 AND FN PIANO(ASC("m"))=48 AND FN PIANO(ASC("Z"))=37 AND FN PIANO(ASC("q"))=49 AND FN PIANO(ASC("i"))=61 AND FN PIANO(ASC("a"))=0,"piano keys")
12750   CALL CHECK(FN CELL$(0,0,0)="--- .. ." AND LEN(FN CELL$(0,0,0))=8,"an empty cell")
12760   X=FN IX(0,0,0)
12770   PN(X)=49:PJ(X)=4:PV(X)=15
12780   CALL CHECK(FN CELL$(0,0,0)="C-4 04 F","a filled cell")
12790   PN(FN IX(0,4,0))=97
12800   PN(FN IX(0,8,0))=40
12810   BPM=120:CP(0)=0:CI(0)=4
12820   CALL BUILDMAP
12830   CALL CHECK(FT=32,"the song has 32 rows")
12840   A$=FN CHSTR$(0)
12850   CALL CHECK(A$="T120 MN Y0 I4 V0.8 V1 O4 MN C4 R4 O3 ML D#1 MN D#2","the PLAY string of a channel: "+A$)
12860   CALL CHECK(FN CHSTR$(1)="T120 MN Y0.3 I5 V0.8 R1 R1","an empty channel is its header and rests: "+FN CHSTR$(1))
12870   ' the music string must be accepted by PLAY (it checks its strings), also when the notes use other instruments and volumes
12880   PN(FN IX(0,12,1))=60:PJ(FN IX(0,12,1))=33:PV(FN IX(0,12,1))=7
12890   PN(FN IX(0,13,1))=61
12900   PN(FN IX(0,15,1))=5
12910   CALL BUILDMAP
12920   B$=FN CHSTR$(1)
12930   CALL CHECK(B$="T120 MN Y0.3 I5 V0.8 R2. I33 V0.467 O4 MN B16 O5 MN C8 O0 ML E1 MN E16","a second channel: "+B$)
12940   PLAY A$,B$
12950   ' from a later row: the first notes are skipped, a note that started earlier is not played (its row is not in the range)
12960   A$=FN STRFROM$(0,8)
12970   CALL CHECK(A$="T120 MN Y0 I4 V1 O3 ML D#1 MN D#2","playing from row 8 (instrument and volume as they were before): "+A$)
12980   A$=FN STRFROM$(0,10)
12990   CALL CHECK(A$="T120 MN Y0 I4 V1 O3 ML D#1 MN D#4.","a note that is still sounding is carried on: "+A$)
13000   A$=FN STRFROM$(0,5)
13010   CALL CHECK(A$="T120 MN Y0 I4 V1 R8. O3 ML D#1 MN D#2","after a note off there is a rest: "+A$)
13020   PLAY A$
13030   ' the song is queued in pieces: the pieces together are the whole string (with the settings restated at the start of each piece)
13040   CAP=-1:CAPS$(0)="":PBPM=BPM:AC$=""
13050   CALL STARTCH(0,0)
13060   CALL FILLCH(0,6)
13070   CALL SENDCH(0)
13080   CALL CHECK(QC(0)=8,"the first piece ends at the next note: "+STR$(QC(0)))
13090   CALL FILLCH(0,20)
13100   CALL SENDCH(0)
13110   CALL CHECK(QC(0)=32 AND CAPS$(0)="T120 MN Y0 I4 V0.8 V1 O4 MN C4 R4 T120 V1 O3 ML D#1 MN D#2","the pieces: "+CAPS$(0))
13120   CAP=0
13130   ' a loop: the rows after its end are those of its start again, a held note is cut at the end
13140   ACTL=-1:LS=0:LE=12:LL=12:VEND=1000000000
13150   CAP=-1:CAPS$(0)="":PBPM=BPM:AC$=""
13160   CALL STARTCH(0,0)
13170   CALL FILLCH(0,30)
13180   CALL SENDCH(0)
13190   CAP=0
13200   CALL CHECK(CAPS$(0)="T120 MN Y0 I4 V0.8 V1 O4 MN C4 R4 O3 MN D#4 O4 MN C4 R4 O3 MN D#4 O4 MN C4 R4" AND QC(0)=32,"a loop of 12 rows: "+CAPS$(0))
13210   CALL CHECK(FN FV(11)=11 AND FN FV(12)=0 AND FN FV(25)=1 AND FN SEGEND(5)=12 AND FN SEGEND(12)=24 AND FN SEGST(30)=24,"virtual rows")
13220   PLG=-1:PRL=13
13230   FOR I=0 TO 7
13240     RS(I)=0
13250   NEXT I
13260   QC(0)=20
13270   CALL TOUCH(0,2,0)
13280   CALL CHECK(RS(0)<>0,"a change in the next round of the loop")
13290   RS(0)=0
13300   CALL TOUCH(0,9,0)
13310   CALL CHECK(RS(0)=0,"a change that is in the round after what is queued")
13320   PLG=0:ACTL=0:VEND=FT
13330   ' a change in the song while it plays is queued anew when it lies in what is queued already
13340   PLG=-1:PRL=0
13350   FOR I=0 TO 7
13360     RS(I)=0
13370   NEXT I
13380   QC(0)=8
13390   CALL TOUCH(0,6,0)
13400   CALL CHECK(RS(0)<>0,"a change inside what is queued")
13410   RS(0)=0
13420   CALL TOUCH(0,20,0)
13430   CALL TOUCH(0,0,0)
13440   CALL TOUCH(0,8,0)
13450   CALL CHECK(RS(0)=0,"a change behind it, before the playhead, or a replaced note at its end")
13460   PN(FN IX(0,8,0))=0
13470   CALL TOUCH(0,8,0)
13480   CALL CHECK(RS(0)<>0,"a note that ends the queued piece is deleted")
13490   PN(FN IX(0,8,0))=40
13500   RS(0)=0
13510   CALL GLOBALEDIT
13520   CALL CHECK(RSA<>0,"an edit of the order")
13530   RSA=0:PLG=0
13540   ' two slots: the second pattern follows the first
13550   CALL NEWSONG
13560   CALL INSSLOT
13570   CALL NEWPATTERN
13580   CALL CHECK(ONUM=2 AND OD(0)=0 AND OD(1)=1 AND NPAT=2,"a new slot and pattern")
13590   PN(FN IX(1,0,2))=50
13600   CALL BUILDMAP
13610   CALL CHECK(FT=64 AND FLO(32)=1 AND FLR(32)=0 AND PN(FN CX(32,2))=50 AND FOFF(1)=32,"the order is flattened")
13620   A$=FN CHSTR$(2)
13630   CALL CHECK(LEFT$(A$,20)="T125 MN Y-0.6 I6 R1" OR LEFT$(A$,12)="T125 MN Y-0.","pan in the header: "+A$)
13640   ' the cursor and the editing
13650   CALL NEWSONG
13660   CR=0:CC=0:CF=0
13670   OCV=4:INS=5:STP=2
13680   CALL KEYACT(ASC("z"))
13690   CALL CHECK(PN(FN IX(0,0,0))=49 AND PJ(FN IX(0,0,0))=5 AND CR=2,"a piano key writes the note with the instrument and steps")
13700   CALL KEYACT(49)
13710   CALL CHECK(PN(FN IX(0,2,0))=97 AND CR=4,"1 puts a note off")
13720   CALL KEYACT(4098)
13730   CALL KEYACT(51)
13740   CALL KEYACT(55)
13750   CALL CHECK(CF=3 AND PJ(FN IX(0,4,0))=37 AND CR=4,"two digits are the instrument")
13760   CALL KEYACT(ASC("C"))
13770   CALL CHECK(PV(FN IX(0,4,0))=12 AND CF=0 AND CR=6,"a hex digit is the volume (upper or lower case)")
13780   CALL KEYACT(4097)
13790   CALL CHECK(CC=0 AND CF=0,"Left at the first field stays")
13800   CALL KEYACT(4098+65536)
13810   CALL CHECK(CC=1 AND CF=0,"Shift + Right jumps a channel")
13820   CALL KEYACT(4105)
13830   CALL KEYACT(4103)
13840   CALL CHECK(CR=0,"PageUp stops at the first row")
13850   CALL KEYACT(4102)
13860   CALL CHECK(CR=31,"End goes to the last row")
13870   ' save and open
13880   CALL NEWSONG
13890   BPM=99:CP(3)=-77:CM(5)=-1:CI(2)=33
13900   INSTRUMENT 33,"NAME TESTINS; OSC SAW 0.5; GAIN 1.1"
13910   CALL INSSLOT
13920   CALL NEWPATTERN
13930   PL(1)=48
13940   PN(FN IX(1,5,6))=60:PJ(FN IX(1,5,6))=33:PV(FN IX(1,5,6))=3
13950   PN(FN IX(0,31,7))=97
13960   CALL SAVEF("mem:song1")
13970   CALL NEWSONG
13980   INSTRUMENT 33
13990   CALL OPENF("mem:song1")
14000   CALL CHECK(BPM=99 AND CP(3)=-77 AND CM(5)<>0 AND CM(4)=0 AND CI(2)=33 AND ONUM=2 AND OD(1)=1 AND NPAT=2 AND PL(1)=48 AND PL(0)=32,"song header and order survive a save and open")
14010   CALL CHECK(PN(FN IX(1,5,6))=60 AND PJ(FN IX(1,5,6))=33 AND PV(FN IX(1,5,6))=3 AND PN(FN IX(0,31,7))=97 AND PN(FN IX(0,0,0))=0 AND PV(FN IX(0,0,0))=-1,"cells survive a save and open")
14020   CALL CHECK(FN INAME$(33)="TESTINS","the instrument of the song is defined again by Open: "+FN INAME$(33))
14030   ' export
14040   CALL EXPORTLINES(0)
14050   CALL CHECK(INSTR(XL$,"INSTRUMENT 33,")>0 AND INSTR(XL$,"PLAY V1$,V2$,V3$,V4$,V5$,V6$,V7$,V8$")>0 AND INSTR(XL$,"V1$=")>0,"the exported lines")
14060   ' TRACKER_HEADLESS=file: also opens that song and plays its strings (PLAY checks them)
14070   IF ENV$("TRACKER_HEADLESS")<>"1" THEN
14080     CALL OPENF(ENV$("TRACKER_HEADLESS"))
14090     CALL BUILDMAP
14100     FOR I=0 TO 7
14110       S$(I)=FN CHSTR$(I)
14120     NEXT I
14130     PLAY S$(0),S$(1),S$(2),S$(3),S$(4),S$(5),S$(6),S$(7)
14140     PRINT "song ";ENV$("TRACKER_HEADLESS");": ";ONUM;" slots, ";NPAT;" patterns, ";FT;" rows, strings of ";LEN(S$(0));" ";LEN(S$(1));" ";LEN(S$(2));" ";LEN(S$(3));" ";LEN(S$(4));" ";LEN(S$(5));" ";LEN(S$(6));" ";LEN(S$(7));" characters"
14150   END IF
14160   PRINT "tracker selftest ";
14170   IF BAD=0 THEN PRINT "ok ";NCHK ELSE PRINT BAD;" FAILED of ";NCHK
14180 END PROC
14190 ' ====================================================================== main program
14200 IF ENV$("TRACKER_HEADLESS")<>"" THEN
14210   CALL SELFTEST
14220   END
14230 END IF
14240 UI OPEN
14250 CALL SETPAL
14260 FOR I=0 TO 7
14270   CH2=21+I
14280   OPEN "play:"+STR$(I+1) FOR OUTPUT AS #CH2
14290 NEXT I
14300 CALL NEWSONG
14310 CALL MAKEFORM
14320 CALL SYNCCTL
14330 CALL REDRAW
14340 CALL SETSTATUS("Piano keys Z S X D C V G B H N J M write notes, Space plays, Tab leaves the grid.")
14350 ON ERROR GOTO ERRH
14360 ARGLOAD$=""
14370 IF ARGC>=1 THEN ARGLOAD$=ARG$(1)
14380 IF ARGLOAD$<>"" THEN CALL OPENF(ARGLOAD$)
14390 LABEL LOOPTOP
14400 ON ERROR GOTO ERRH
14410 ON UI GOSUB HANDLER
14420 REPEAT
14430   GFX VSYNC
14440   UI UPDATE
14450   CALL PLAYTICK
14460 UNTIL Q=1
14470 ON UI OFF
14480 ON ERROR OFF
14490 SOUND OFF
14500 FOR I=0 TO 7
14510   CH2=21+I
14520   CLOSE #CH2
14530 NEXT I
14540 CALL RESTOREPAL
14550 UI CLOSE
14560 REM no GFX RESET here
14570 CLS
14580 PRINT "Tracker finished."
14590 END
14600 LABEL ERRH
14610 ON UI OFF
14620 CALL UK_MSG("Error",ERR$+"|(line "+STR$(ERL)+")")
14630 RESUME LOOPTOP
14640 LABEL HANDLER
14650 E=UI("EVENT")
14660 WHILE E<>0
14670   EK=UI("KIND")
14680   EV=UI("VALUE")
14690   CALL REACT(E,EK,EV)
14700   E=UI("EVENT")
14710 WEND
14720 RETURN
