1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' Instrument board: the 28 factory instruments of the synthesizer (WAVE ch,4..31 / I 4..31 in PLAY),
20 ' and seven of your own that INSTRUMENT defines (32 a bass, 33 an FM electric piano, 34 an FM bell, 35 a polyphonic pad,
25 ' 36 a saw for the cutoff sweep, 37 an FM oscillator with feedback, 38 two carriers on one modulator). C plays a chord, W a filter sweep, Y moves the notes across the stereo picture.
30 DIM N$(40),OF(64)
40 FOR I=4 TO 27:READ N$(I):NEXT I
45 FOR I=28 TO 31:N$(I)="UNASSIGNED":NEXT I
50 N$(32)="MYBASS":N$(33)="FMPIANO":N$(34)="FMBELL":N$(35)="POLYPAD":N$(36)="SWEEP":N$(37)="FEEDBACK":N$(38)="TWINFM"
60 DATA "BASS","LEAD","PAD","PLUCK","ORGAN","STRINGS","BRASS","FLUTE","BELL","KICK","SNARE","HIHAT","TOM","CLAP","ZAP","EPIANO","ACOUSTIC","METAL","DISTGUIT","METALLOPHONE","MARIMBA","HARP","CLARINET","ACIDBASS"
70 INSTRUMENT 32,"NAME MYBASS; OSC SAW 0.8; OSC SQUARE 0.6 0.5 0 0.3; FILTER 3 0.3 2 0.15; ENV 0.005 0.2 0.6 0.1; GAIN 0.6"
72 INSTRUMENT 33,"NAME FMPIANO; OSC SINE 0.5 1 0 0.5 FM 3; OSC SINE 1 2 0 0.5 MOD; FMDECAY 0.4; ENV 0 0.8 0 0.3; GAIN 1.8"
74 INSTRUMENT 34,"NAME FMBELL; OSC SINE 0.4 1 0 0.5 FM 6; OSC SINE 1 3.5 0 0.5 MOD; FMDECAY 1.5; ENV 0 1.5 0 1; GAIN 1.7"
76 INSTRUMENT 35,"NAME POLYPAD; OSC SAW 0.4 1 -9; OSC SAW 0.4 1 9; FILTER 3 0.1; ENV 0.2 0.2 0.8 1.2; POLY 6; GAIN 1.5"
77 INSTRUMENT 37,"NAME FEEDBACK; OSC SINE 0.5 1 0 0.5 FB 2.5; ENV 0.01 0.3 0.6 0.3; GAIN 1.4"
78 INSTRUMENT 36,"NAME SWEEP; OSC SAW 0.5; OSC SAW 0.4 1 7; FILTER 1 0.4; ENV 0.02 0 1 0.3; GAIN 1.3"
79 INSTRUMENT 38,"NAME TWINFM; OSC SINE 0.35 1 0 0.5 FM 2; OSC SINE 0.35 2.01 0 0.5 FM 2; OSC SINE 1 1 0 0.5 MOD; ALGO 1; FMDECAY 0.6; ENV 0 0.8 0 0.3; GAIN 1.6"
80 INS=4:OC=3:OF(4)=1:OF(27)=1:REM bass instruments sound one octave higher: a bass below 100 Hz hardly comes out of small speakers
90 GOSUB 500
100 GET K$
110 IF K$>="a" AND K$<="z" THEN K$=CHR$(ASC(K$)-32)
120 IF K$="Q" THEN SOUND OFF:END
130 IF K$="N" THEN INS=INS+1:IF INS>38 THEN INS=4
140 IF K$="P" THEN INS=INS-1:IF INS<4 THEN INS=38
150 IF K$="+" AND OC<5 THEN OC=OC+1
160 IF K$="-" AND OC>1 THEN OC=OC-1
170 P=INSTR("ASDFGHJK",K$)
180 IF P>0 THEN PLAY "I"+STR$(INS)+" O"+STR$(OC+OF(INS))+" L2 "+MID$("CDEFGAB",P,1):IF P=8 THEN PLAY "I"+STR$(INS)+" O"+STR$(OC+OF(INS)+1)+" L2 C"
190 IF K$="T" THEN GOSUB 600
200 IF K$="U" THEN GOSUB 700
202 IF K$="C" THEN PLAY "I"+STR$(INS)+" O"+STR$(OC+OF(INS))+" L2 [CEG]":REM a chord: only a POLY instrument (e.g. 20, 24, 35) plays all three notes
203 IF K$="Y" THEN PLAY "I"+STR$(INS)+" O"+STR$(OC+OF(INS))+" L8 Y-1 C Y-0.5 E Y0 G Y0.5 E Y1 C Y0":REM stereo positions: the notes travel from the left to the right
204 IF K$="W" THEN PLAY "I36 K0.8 K30,3 O2 L1 C":REM the cutoff glides from 0.8 to 30 times the note in 3 seconds
210 GOSUB 500
220 GOTO 100
500 ' the screen
510 CLS
520 PRINT "INSTRUMENT BOARD"
530 PRINT
540 FOR I=4 TO 38
545   VTAB 3+(I-4) MOD 17:HTAB 1+((I-4)\17)*26
550   IF I=INS THEN PRINT ">";  ELSE PRINT " ";
560   PRINT I;" ";N$(I)
570 NEXT I
575 VTAB 21
590 PRINT "N/P instrument  +/- octave ";OC
595 PRINT "A S D F G H J K play C D E F G A B C   T tune   U demo   Q quit"
597 PRINT "C chord (POLY instruments, e.g. 24 or 35)   W filter sweep   Y stereo positions"
599 RETURN
600 ' a short tune with the selected instrument
610 PLAY "I"+STR$(INS)+" T130 O"+STR$(OC+OF(INS))+" L8 CEGE CEG4 FA>C<A FAC4 GBDG4"
620 RETURN
700 ' a little band: bass, pad, lead and drums on four channels
710 B$="I4 T120 O2 L8 CCGG AAEE FFCC GGDD CCGG AAEE FFCC G4G4"
720 P$="I6 T120 O3 L1 CE AC FA GB"
730 L$="I5 T120 O4 L4 EG>C<G EG>A<E FA>C<A GB>D<G"
740 D$="I14 T120 O2 L8 RCRC RCRC RCRC RCRC RCRC RCRC RCRC RCRC"
750 PLAY B$,P$,L$,D$
760 RETURN
