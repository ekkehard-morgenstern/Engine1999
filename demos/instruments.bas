1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' Instrument board: the 16 factory instruments of the synthesizer (WAVE ch,4..19 / I 4..19 in PLAY),
20 ' and five of your own that INSTRUMENT defines (20 a bass, 21 an FM electric piano, 22 an FM bell, 23 a polyphonic pad,
25 ' 24 a saw for the cutoff sweep). C plays a chord, W a filter sweep.
30 DIM N$(25)
40 FOR I=4 TO 19:READ N$(I):NEXT I
50 N$(20)="MYBASS":N$(21)="FMPIANO":N$(22)="FMBELL":N$(23)="POLYPAD":N$(24)="SWEEP"
60 DATA "BASS","LEAD","PAD","PLUCK","ORGAN","STRINGS","BRASS","FLUTE","BELL","KICK","SNARE","HIHAT","TOM","CLAP","ZAP","EPIANO"
70 INSTRUMENT 20,"NAME MYBASS; OSC SAW 0.8; OSC SQUARE 0.6 0.5 0 0.3; FILTER 3 0.3 2 0.15; ENV 0.005 0.2 0.6 0.1; GAIN 0.6"
72 INSTRUMENT 21,"NAME FMPIANO; OSC SINE 0.5 1 0 0.5 FM 3; OSC SINE 1 2 0 0.5 MOD; FMDECAY 0.4; ENV 0 0.8 0 0.3; GAIN 1.8"
74 INSTRUMENT 22,"NAME FMBELL; OSC SINE 0.4 1 0 0.5 FM 6; OSC SINE 1 3.5 0 0.5 MOD; FMDECAY 1.5; ENV 0 1.5 0 1; GAIN 1.7"
76 INSTRUMENT 23,"NAME POLYPAD; OSC SAW 0.4 1 -9; OSC SAW 0.4 1 9; FILTER 3 0.1; ENV 0.2 0.2 0.8 1.2; POLY 6; GAIN 1.5"
78 INSTRUMENT 24,"NAME SWEEP; OSC SAW 0.5; OSC SAW 0.4 1 7; FILTER 1 0.4; ENV 0.02 0 1 0.3; GAIN 1.3"
80 INS=4:OC=3
90 GOSUB 500
100 GET K$
110 IF K$>="a" AND K$<="z" THEN K$=CHR$(ASC(K$)-32)
120 IF K$="Q" THEN SOUND OFF:END
130 IF K$="N" THEN INS=INS+1:IF INS>24 THEN INS=4
140 IF K$="P" THEN INS=INS-1:IF INS<4 THEN INS=24
150 IF K$="+" AND OC<5 THEN OC=OC+1
160 IF K$="-" AND OC>1 THEN OC=OC-1
170 P=INSTR("ASDFGHJK",K$)
180 IF P>0 THEN PLAY "I"+STR$(INS)+" O"+STR$(OC)+" L2 "+MID$("CDEFGAB",P,1):IF P=8 THEN PLAY "I"+STR$(INS)+" O"+STR$(OC+1)+" L2 C"
190 IF K$="T" THEN GOSUB 600
200 IF K$="U" THEN GOSUB 700
202 IF K$="C" THEN PLAY "I"+STR$(INS)+" O"+STR$(OC)+" L2 [CEG]":REM a chord: only a POLY instrument (23) plays all three notes
204 IF K$="W" THEN PLAY "I24 K0.8 K30,3 O2 L1 C":REM the cutoff glides from 0.8 to 30 times the note in 3 seconds
210 GOSUB 500
220 GOTO 100
500 ' the screen
510 CLS
520 PRINT "INSTRUMENT BOARD"
530 PRINT
540 FOR I=4 TO 24
550   IF I=INS THEN PRINT ">";  ELSE PRINT " ";
560   PRINT I;" ";N$(I)
570 NEXT I
580 PRINT
590 PRINT "N/P instrument  +/- octave ";OC
595 PRINT "A S D F G H J K play C D E F G A B C   T tune   U demo   Q quit"
597 PRINT "C chord (instrument 23 has voices for it)   W filter sweep"
599 RETURN
600 ' a short tune with the selected instrument
610 PLAY "I"+STR$(INS)+" T130 O"+STR$(OC)+" L8 CEGE CEG4 FA>C<A FAC4 GBDG4"
620 RETURN
700 ' a little band: bass, pad, lead and drums on four channels
710 B$="I4 T120 O1 L8 CCGG AAEE FFCC GGDD CCGG AAEE FFCC G4G4"
720 P$="I6 T120 O3 L1 CE AC FA GB"
730 L$="I5 T120 O4 L4 EG>C<G EG>A<E FA>C<A GB>D<G"
740 D$="I14 T120 O2 L8 RCRC RCRC RCRC RCRC RCRC RCRC RCRC RCRC"
750 PLAY B$,P$,L$,D$
760 RETURN
