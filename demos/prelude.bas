10 ' Prelude in C major (BWV 846), from the Well-Tempered Clavier by Johann Sebastian Bach.
20 ' Public domain; the first eight bars. Each bar is five notes, played in a pattern of
30 ' sixteenth notes, twice (voice 1), over a held bass note (voice 2).
40 PRINT "PRELUDE IN C - Bach"
50 PRINT "Any key stops the music."
60 DIM N$(5)
70 WAVE 1,2:ENVELOPE 1,0.005,0.25,0.35,0.15
80 WAVE 2,0:ENVELOPE 2,0.01,0.1,0.8,0.4
90 FOR BAR=1 TO 8
100 READ B$
110 FOR I=0 TO 4:READ N$(I):NEXT I
120 A$=N$(0)+N$(1)+N$(2)+N$(3)+N$(4)+N$(2)+N$(3)+N$(4)
130 PLAY "T72 L16 "+A$+A$,"T72 L1 ML "+B$
140 NEXT BAR
150 PLAY "T72 ML O3 C1","T72 ML O3 G1","T72 ML O4 E1"
160 REPEAT
165 WAIT 3
170 IF INKEY$<>"" THEN SOUND OFF:END
180 UNTIL PLAYING(1)=0 AND PLAYING(2)=0 AND PLAYING(3)=0
190 PRINT "The end."
200 END
210 DATA "O2 C","O4 C ","O4 E ","O4 G ","O5 C ","O5 E "
220 DATA "O2 C","O4 C ","O4 D ","O4 A ","O5 D ","O5 F "
230 DATA "O1 G","O3 B ","O4 D ","O4 G ","O5 D ","O5 F "
240 DATA "O2 C","O4 C ","O4 E ","O4 G ","O5 C ","O5 E "
250 DATA "O2 A","O4 C ","O4 E ","O4 A ","O5 E ","O5 A "
260 DATA "O2 D","O4 C ","O4 D ","O4 F+","O4 A ","O5 D "
270 DATA "O1 G","O3 B ","O4 D ","O4 G ","O5 D ","O5 G "
280 DATA "O2 C","O3 B ","O4 C ","O4 E ","O4 G ","O5 C "
