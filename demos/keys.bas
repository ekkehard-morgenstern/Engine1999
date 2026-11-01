1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' KEY(name$): is a key held down right now? A small keyboard that lights up the keys you hold.
20 DIM R$(3)
30 R$(0)="QWERTYUIOP":R$(1)="ASDFGHJKL":R$(2)="ZXCVBNM"
40 CLS
50 PRINT "KEY(name$) tells whether a key is held down: unlike INKEY$ it knows"
60 PRINT "when you let go, and several keys at once. Hold some keys. Q quits."
70 REPEAT
80   FOR R=0 TO 2
90     FOR I=1 TO LEN(R$(R))
100      K$=MID$(R$(R),I,1)
110      GOSUB 300
120      LOCATE 6+R*2+I*4,6+R*2
130      PRINT "[";K$;"]";
140    NEXT I
150  NEXT R
160  RESTORE 400
170  FOR I=1 TO 8
180    READ K$,X,Y
190    GOSUB 300
200    LOCATE X,Y
210    PRINT " ";K$;" ";
220  NEXT I
230  COLOR 1,0
240  LOCATE 6,22
250  IF KEY("ANY") THEN PRINT "a key is down   " ELSE PRINT "no key is down  "
260  WAIT 1
270 UNTIL KEY("Q")
280 COLOR 1,0:CLS:PRINT "Bye."
290 END
300 ' light the key K$ if it is down
310 IF KEY(K$) THEN COLOR 1,15 ELSE COLOR 1,0
320 RETURN
400 DATA "LEFT",10,15,"UP",18,14,"DOWN",18,16,"RIGHT",25,15
410 DATA "SPACE",12,19,"SHIFT",33,19,"CTRL",42,19,"ALT",50,19
