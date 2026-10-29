10 ' Ode to Joy, from the 9th symphony by Ludwig van Beethoven (1824, public domain).
20 ' The melody is in voice 1, a simple bass line in voice 2.
30 PRINT "ODE TO JOY - Beethoven"
40 PRINT "Any key stops the music."
50 WAVE 1,2:ENVELOPE 1,0.01,0.1,0.7,0.12
60 WAVE 2,0:ENVELOPE 2,0.02,0.1,0.8,0.3
70 FOR LINE=1 TO 4
80 READ M$,B$
90 PLAY "T104 L4 O4 "+M$,"T104 O2 L1 ML "+B$
100 NEXT LINE
110 REPEAT
115 WAIT 3
120 IF INKEY$<>"" THEN SOUND OFF:END
130 UNTIL PLAYING(1)=0 AND PLAYING(2)=0
140 PRINT "The end."
150 END
160 DATA "E E F G G F E D C C D E E4. D8 D2","C G C G"
170 DATA "E E F G G F E D C C D E D4. C8 C2","C G C C"
180 DATA "D D E C D E8 F8 E C D E8 F8 E D C D <G2","G C G G"
190 DATA ">E E F G G F E D C C D E D4. C8 C2","C G C C"
