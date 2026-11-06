1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
6 ' A tool of Engine1999: start it with TOOL "musiced" (or TOOL "musiced","file" to open a music file) from the prompt or from a program;
7 ' the program you are working on stays as it is. It can also be run as a program of its own (LOAD "tools/musiced", RUN; ARG$(1) is read the same way).
10 ' MUSIC EDITOR - builds the music strings of PLAY (up to three voices) from buttons, and plays them. It is written in BASIC on the UI
20 ' subsystem and uses the libraries uikit (dialogs, sliders), mml (tokenizer) and filereq (file requester). Needs the window (engine1999).
30 '   The string of a voice is a list of tokens (the list on the left): notes, rests and commands. A click on a piano key adds a note (and a
40 '   short preview sounds) with the octave, length and dot of the controls; Rest adds a rest. New notes go behind the selected token, or to
50 '   the end of the voice when "After selection" is off or nothing is selected. The first thing added to an empty voice is a header
60 '   (T, V, W and a note style from the controls below) so that the string plays as you hear it here.
70 '   Tempo / Volume / Wave / Style insert that command with the value shown (to change it in the middle of the piece).
80 '   Edit (or a double click on a token) changes it; you can type several tokens, the editor splits them. Delete removes the token.
90 '   Play plays all voices that are not empty together; Stop silences them. Save / Open keep the voices in a text file (one string per
100 '   line); Export writes a BASIC line  PLAY "voice 1","voice 2"  that you can MERGE or LOAD into a program. ESC cancels dialogs.
110 ' widget ids: 1 form, 20 token list, 21 string, 30..38 buttons, 40 status, 100..111 piano, 120..125 lengths, 126 dotted, 127 rest, 128 after
120 ' selection, 130..132 octave, 140 tempo, 143 volume (sliders: +1 label, +2 readout), 150..153 insert, 160..163 wave, 165..167 style, 170..172 voices, 180..184 labels
130 LOAD LIBRARY "uikit"
140 LOAD LIBRARY "mml"
150 LOAD LIBRARY "filereq"
160 REM the graphics of the calling program stay (no GFX RESET)
170 CLS
180 BREAK OFF
190 DIM TK$(1500)
200 DIM NT(3)
210 CV=0
220 SEL=-1
230 OV=4
240 DIRTY=0
250 Q=0
260 DR$=""
270 ' ====================================================================== functions (they must stand before the lines that use them)
280 FUNC NOTENAME$(I)
290   LOCAL N$
300   N$=MID$("C C#D D#E F F#G G#A A#B ",I*2+1,2)
310   IF RIGHT$(N$,1)=" " THEN N$=LEFT$(N$,1)
320   RETURN N$
330 END FUNC
340 ' the tokens of voice V as a string (blanks between them)
350 FUNC JOINV$(V)
360   LOCAL I,S$
370   S$=""
380   FOR I=0 TO NT(V)-1
390     IF I>0 THEN S$=S$+" "
400     S$=S$+TK$(V*500+I)
410   NEXT I
420   RETURN S$
430 END FUNC
440 ' the octave that is valid before token P of voice V (O n, < and >; 4 at the start)
450 FUNC OCTAT(V,P)
460   LOCAL I,O,T$
470   O=4
480   FOR I=0 TO P-1
490     T$=TK$(V*500+I)
500     IF LEFT$(T$,1)="O" THEN O=VAL(MID$(T$,2))
510     IF T$="<" THEN O=O-1
520     IF T$=">" THEN O=O+1
530   NEXT I
540   RETURN O
550 END FUNC
560 ' where a new token goes: behind the selected one, or at the end
570 FUNC INSPOS()
580   IF SEL>=0 AND SEL<NT(CV) AND UI("GET",128)<>0 THEN RETURN SEL+1
590   RETURN NT(CV)
600 END FUNC
610 FUNC WAVEV()
620   LOCAL I,R
630   R=1
640   FOR I=0 TO 3
650     IF UI("GET",160+I)<>0 THEN R=I
660   NEXT I
670   RETURN R
680 END FUNC
690 FUNC STYLE$()
700   IF UI("GET",165)<>0 THEN RETURN "ML"
710   IF UI("GET",167)<>0 THEN RETURN "MS"
720   RETURN "MN"
730 END FUNC
740 ' the note length number 1 2 4 8 16 32 and the dot
750 FUNC LENTXT$()
760   LOCAL I,N,R$
770   N=1
780   FOR I=0 TO 5
790     IF UI("GET",120+I)<>0 THEN N=INT(2^I)
800   NEXT I
810   R$=STR$(N)
820   IF UI("GET",126)<>0 THEN R$=R$+"."
830   RETURN R$
840 END FUNC
850 ' ====================================================================== main program
860 UI OPEN
870 CALL MAKEFORM
880 CALL SETSTATUS("Click the piano keys to add notes. The list shows the tokens of the voice.")
890 ON ERROR GOTO ERRH
891 ARGLOAD$=""
892 IF ARGC>=1 THEN ARGLOAD$=ARG$(1)
893 IF ARGLOAD$<>"" THEN CALL OPENF(ARGLOAD$)
900 LABEL LOOPTOP
910 ON ERROR GOTO ERRH
920 ON UI GOSUB HANDLER
930 REPEAT
940   GFX VSYNC
950   UI UPDATE
960 UNTIL Q=1
970 ON UI OFF
980 ON ERROR OFF
990 SOUND OFF
1000 UI CLOSE
1010 REM no GFX RESET here
1020 CLS
1030 PRINT "Music editor finished."
1040 END
1050 LABEL ERRH
1060 ON UI OFF
1070 CALL UK_MSG("Error",ERR$+"|(line "+STR$(ERL)+")")
1080 RESUME LOOPTOP
1090 ' ====================================================================== the form
1100 PROC MAKEFORM
1110   LOCAL I
1120   UI PANEL 1,0,0,640,300,"Music editor"
1130   UI LIST 20,10,22,236,150
1140   UI RADIO 170,10,176,74,16,"Voice 1",1
1150   UI RADIO 171,88,176,74,16,"Voice 2",1
1160   UI RADIO 172,166,176,74,16,"Voice 3",1
1170   UI VALUE 170,1
1180   UI LABEL 21,10,196,236,14,""
1190   UI BUTTON 30,10,214,56,22,"Play"
1200   UI BUTTON 31,70,214,56,22,"Stop"
1210   UI BUTTON 32,130,214,56,22,"Clear"
1220   UI BUTTON 33,190,214,56,22,"Delete"
1230   UI BUTTON 34,10,240,56,22,"Edit"
1240   UI BUTTON 35,70,240,56,22,"Save..."
1250   UI BUTTON 36,130,240,56,22,"Open..."
1260   UI BUTTON 37,190,240,56,22,"Export"
1270   UI BUTTON 38,190,260,56,22,"Quit"
1280   UI LABEL 40,10,285,620,14,""
1290   UI LABEL 180,258,26,50,14,"Octave"
1300   UI BUTTON 130,312,22,24,20,"-"
1310   UI LABEL 132,344,26,20,14,"4"
1320   UI BUTTON 131,368,22,24,20,"+"
1330   FOR I=0 TO 11
1340     UI BUTTON 100+I,258+I*31,48,30,28,FN NOTENAME$(I)
1350   NEXT I
1360   UI LABEL 181,258,86,50,14,"Length"
1370   FOR I=0 TO 5
1380     UI RADIO 120+I,310+I*50,84,46,16,STR$(INT(2^I)),2
1390   NEXT I
1400   UI VALUE 122,1
1410   UI CHECK 126,258,108,80,16,"Dotted"
1420   UI BUTTON 127,344,106,60,22,"Rest"
1430   UI CHECK 128,420,108,150,16,"After selection"
1440   UI VALUE 128,1
1450   CALL UK_SLIDER(140,258,136,372,"Tempo",40,240,120)
1460   CALL UK_DIVISOR(100)
1470   CALL UK_SLIDER(143,258,160,372,"Volume",0,100,50)
1480   UI LABEL 182,258,190,100,14,"Insert command:"
1490   UI BUTTON 150,362,186,60,22,"Tempo"
1500   UI BUTTON 151,426,186,60,22,"Volume"
1510   UI BUTTON 152,490,186,60,22,"Wave"
1520   UI BUTTON 153,554,186,60,22,"Style"
1530   UI LABEL 183,258,218,40,14,"Wave"
1540   UI RADIO 160,300,216,60,16,"Sine",3
1550   UI RADIO 161,364,216,70,16,"Square",3
1560   UI RADIO 162,438,216,76,16,"Triangle",3
1570   UI RADIO 163,518,216,60,16,"Noise",3
1580   UI VALUE 161,1
1590   UI LABEL 184,258,242,40,14,"Style"
1600   UI RADIO 165,300,240,70,16,"Legato",4
1610   UI RADIO 166,374,240,70,16,"Normal",4
1620   UI RADIO 167,448,240,80,16,"Staccato",4
1630   UI VALUE 166,1
1640 END PROC
1650 PROC SETSTATUS(T$)
1660   UI CAPTION 40,T$
1670 END PROC
1680 ' ====================================================================== the tokens
1690 ' the list and the string label from the tokens of the current voice
1700 PROC FILLLIST
1710   LOCAL I
1720   UI CLEARLIST 20
1730   FOR I=0 TO NT(CV)-1
1740     UI ITEM 20,TK$(CV*500+I)
1750   NEXT I
1760   IF SEL>=NT(CV) THEN SEL=NT(CV)-1
1770   IF SEL>=0 THEN UI VALUE 20,SEL
1780   CALL SHOWSTR
1790 END PROC
1800 PROC SHOWSTR
1810   LOCAL S$
1820   S$=FN JOINV$(CV)
1830   IF LEN(S$)>36 THEN S$="..."+RIGHT$(S$,33)
1840   UI CAPTION 21,S$
1850 END PROC
1860 ' puts token T$ at index P of the current voice (the list is not touched)
1870 PROC INSAT(P,T$)
1880   LOCAL I,B
1890   B=CV*500
1900   FOR I=NT(CV) DOWNTO P+1
1910     TK$(B+I)=TK$(B+I-1)
1920   NEXT I
1930   TK$(B+P)=T$
1940   NT(CV)=NT(CV)+1
1950 END PROC
1960 PROC INSTOK(T$)
1970   LOCAL P
1980   IF NT(CV)>=500 THEN
1990     CALL SETSTATUS("This voice is full (500 tokens).")
2000     EXIT PROC
2010   END IF
2020   P=FN INSPOS()
2030   CALL INSAT(P,T$)
2040   IF P=NT(CV)-1 THEN
2050     UI ITEM 20,T$
2060   ELSE
2070     CALL FILLLIST
2080   END IF
2090   SEL=P
2100   UI VALUE 20,P
2110   CALL SHOWSTR
2120   DIRTY=-1
2130 END PROC
2140 ' the first token of an empty voice: tempo, volume, wave and note style from the controls
2150 PROC HEADER
2160   CALL INSTOK("T"+STR$(FN UK_VAL(140)))
2170   CALL INSTOK("V"+STR$(FN UK_VAL(143)))
2180   CALL INSTOK("W"+STR$(FN WAVEV()))
2190   CALL INSTOK(FN STYLE$())
2200 END PROC
2210 PROC NOTE(I)
2220   IF NT(CV)=0 THEN CALL HEADER
2230   IF FN OCTAT(CV,FN INSPOS())<>OV THEN CALL INSTOK("O"+STR$(OV))
2240   CALL INSTOK(FN NOTENAME$(I)+FN LENTXT$())
2250   PLAY "T240 MN V"+STR$(FN UK_VAL(143))+" W"+STR$(FN WAVEV())+" O"+STR$(OV)+" "+FN NOTENAME$(I)+"8"
2260 END PROC
2270 PROC RESTTOK
2280   IF NT(CV)=0 THEN CALL HEADER
2290   CALL INSTOK("R"+FN LENTXT$())
2300 END PROC
2310 PROC DELTOK
2320   LOCAL I,B
2330   IF SEL<0 OR SEL>=NT(CV) THEN
2340     CALL SETSTATUS("Select a token first.")
2350     EXIT PROC
2360   END IF
2370   B=CV*500
2380   FOR I=SEL TO NT(CV)-2
2390     TK$(B+I)=TK$(B+I+1)
2400   NEXT I
2410   NT(CV)=NT(CV)-1
2420   IF SEL>=NT(CV) THEN SEL=NT(CV)-1
2430   CALL FILLLIST
2440   DIRTY=-1
2450 END PROC
2460 PROC EDITTOK
2470   LOCAL T$,P
2480   IF SEL<0 OR SEL>=NT(CV) THEN
2490     CALL SETSTATUS("Select a token first.")
2500     EXIT PROC
2510   END IF
2520   T$=FN UK_INPUT$("Edit token","The token (you may type several commands):",TK$(CV*500+SEL))
2530   IF UK_OK=0 THEN EXIT PROC
2540   CALL MK_SET(T$)
2550   T$=FN MK_NEXT$()
2560   IF T$="" THEN EXIT PROC
2570   TK$(CV*500+SEL)=T$
2580   P=SEL+1
2590   T$=FN MK_NEXT$()
2600   WHILE T$<>"" AND NT(CV)<500
2610     CALL INSAT(P,T$)
2620     P=P+1
2630     T$=FN MK_NEXT$()
2640   WEND
2650   CALL FILLLIST
2660   DIRTY=-1
2670 END PROC
2680 PROC CLEARVOICE
2690   IF NT(CV)=0 THEN EXIT PROC
2700   IF FN UK_ASK("Clear","Delete all tokens of this voice?")=0 THEN EXIT PROC
2710   NT(CV)=0
2720   SEL=-1
2730   CALL FILLLIST
2740   DIRTY=-1
2750 END PROC
2760 ' voice V gets the tokens of the music string A$
2770 PROC TOKENIZE(V,A$)
2780   LOCAL T$
2790   NT(V)=0
2800   CALL MK_SET(A$)
2810   T$=FN MK_NEXT$()
2820   WHILE T$<>"" AND NT(V)<500
2830     TK$(V*500+NT(V))=T$
2840     NT(V)=NT(V)+1
2850     T$=FN MK_NEXT$()
2860   WEND
2870 END PROC
2880 ' ====================================================================== sound
2890 PROC PLAYALL
2900   LOCAL I,N,S$,A$,B$,C$
2910   SOUND OFF
2920   N=0
2930   FOR I=0 TO 2
2940     S$=FN JOINV$(I)
2950     IF S$<>"" THEN
2960       N=N+1
2970       IF N=1 THEN A$=S$
2980       IF N=2 THEN B$=S$
2990       IF N=3 THEN C$=S$
3000     END IF
3010   NEXT I
3020   IF N=0 THEN
3030     CALL SETSTATUS("Nothing to play yet.")
3040     EXIT PROC
3050   END IF
3060   IF N=1 THEN PLAY A$
3070   IF N=2 THEN PLAY A$,B$
3080   IF N=3 THEN PLAY A$,B$,C$
3090   CALL SETSTATUS("Playing "+STR$(N)+" voice(s). Stop silences them.")
3100 END PROC
3110 PROC INSCMD(K)
3120   IF NT(CV)=0 THEN EXIT PROC
3130   IF K=0 THEN CALL INSTOK("T"+STR$(FN UK_VAL(140)))
3140   IF K=1 THEN CALL INSTOK("V"+STR$(FN UK_VAL(143)))
3150   IF K=2 THEN CALL INSTOK("W"+STR$(FN WAVEV()))
3160   IF K=3 THEN CALL INSTOK(FN STYLE$())
3170 END PROC
3180 ' ====================================================================== files
3190 PROC SAVEF(F$)
3200   LOCAL I
3210   OPEN F$ FOR OUTPUT AS #1
3220   FOR I=0 TO 2
3230     PRINT #1,FN JOINV$(I)
3240   NEXT I
3250   CLOSE #1
3260   DIRTY=0
3270   CALL SETSTATUS("Saved "+F$+".")
3280 END PROC
3290 PROC OPENF(F$)
3300   LOCAL I,L$
3310   OPEN F$ FOR INPUT AS #1
3320   FOR I=0 TO 2
3330     L$=""
3340     IF EOF(1)=0 THEN LINE INPUT #1,L$
3350     CALL TOKENIZE(I,L$)
3360   NEXT I
3370   CLOSE #1
3380   CV=0
3390   SEL=-1
3400   UI VALUE 170,1
3410   CALL FILLLIST
3420   DIRTY=0
3430   CALL SETSTATUS("Opened "+F$+".")
3440 END PROC
3450 PROC EXPORTF(F$)
3460   LOCAL I,L$,S$
3470   L$=""
3480   FOR I=0 TO 2
3490     S$=FN JOINV$(I)
3500     IF S$<>"" THEN
3510       IF L$<>"" THEN L$=L$+","
3520       L$=L$+CHR$(34)+S$+CHR$(34)
3530     END IF
3540   NEXT I
3550   IF L$="" THEN
3560     CALL SETSTATUS("Nothing to export yet.")
3570     EXIT PROC
3580   END IF
3590   OPEN F$ FOR OUTPUT AS #1
3600   PRINT #1,"10 ' music made with the music editor of Engine1999"
3610   PRINT #1,"20 PLAY "+L$
3620   CLOSE #1
3630   CALL SETSTATUS("Exported the PLAY line to "+F$+" (MERGE or LOAD it).")
3640 END PROC
3650 ' ====================================================================== what happens
3660 LABEL HANDLER
3670 E=UI("EVENT")
3680 WHILE E<>0
3690   K=UI("KIND")
3700   V=UI("VALUE")
3710   IF FN UK_TRACK(E,V)=0 THEN CALL BUTTON(E,K,V)
3720   E=UI("EVENT")
3730 WEND
3740 RETURN
3750 PROC BUTTON(E,K,V)
3760   LOCAL F$
3770   IF E=20 AND K=3 THEN SEL=V
3780   IF E=20 AND K=4 THEN
3790     SEL=V
3800     CALL EDITTOK
3810   END IF
3820   IF E>=100 AND E<=111 AND K=1 THEN CALL NOTE(E-100)
3830   IF E=127 AND K=1 THEN CALL RESTTOK
3840   IF E=130 AND K=1 AND OV>0 THEN OV=OV-1
3850   IF E=131 AND K=1 AND OV<8 THEN OV=OV+1
3860   IF E=130 OR E=131 THEN UI CAPTION 132,STR$(OV)
3870   IF E>=150 AND E<=153 AND K=1 THEN CALL INSCMD(E-150)
3880   IF E>=170 AND E<=172 AND K=2 THEN
3890     CV=E-170
3900     SEL=-1
3910     CALL FILLLIST
3920   END IF
3930   IF E=30 AND K=1 THEN CALL PLAYALL
3940   IF E=31 AND K=1 THEN
3950     SOUND OFF
3960     CALL SETSTATUS("Stopped.")
3970   END IF
3980   IF E=32 AND K=1 THEN CALL CLEARVOICE
3990   IF E=33 AND K=1 THEN CALL DELTOK
4000   IF E=34 AND K=1 THEN CALL EDITTOK
4010   IF E=35 AND K=1 THEN
4020     F$=FN FR_PICK$("Save the music as",DR$,"*.mus;*.txt",-1)
4030     DR$=FR_DIR$
4040     IF F$<>"" THEN CALL SAVEF(F$)
4050   END IF
4060   IF E=36 AND K=1 THEN
4070     F$=FN FR_PICK$("Open music",DR$,"*.mus;*.txt",0)
4080     DR$=FR_DIR$
4090     IF F$<>"" THEN CALL OPENF(F$)
4100   END IF
4110   IF E=37 AND K=1 THEN
4120     F$=FN FR_PICK$("Export as a BASIC line",DR$,"*.bas",-1)
4130     DR$=FR_DIR$
4140     IF F$<>"" THEN CALL EXPORTF(F$)
4150   END IF
4160   IF E=38 AND K=1 THEN
4170     IF DIRTY=0 THEN
4180       Q=1
4190     ELSE
4200       IF FN UK_ASK("Quit","The music has changed and is not saved.|Quit anyway?")<>0 THEN Q=1
4210     END IF
4220   END IF
4230 END PROC
