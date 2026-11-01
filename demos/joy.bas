1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' JOY(name$, player): the game controllers. Plug in up to four pads (Xbox, PlayStation, Switch Pro ...) and press their buttons. Q quits. (Window version only.)
20 DIM BN$(16)
30 FOR I=0 TO 15:READ BN$(I):NEXT I
40 GFX RESET:CLS
50 PRINT "Game controllers: plug in a pad, press its buttons, move its sticks. Q quits."
60 PRINT "JOY(";CHR$(34);"COUNT";CHR$(34);") = pads plugged in now"
70 REPEAT
80   LOCATE 60,1:COLOR 1,0:PRINT "pads:";JOY("COUNT");" ";
90   FOR P=1 TO 4
100     CALL PAD(P)
110   NEXT P
120   WAIT 1
130   A$=INKEY$
140 UNTIL A$="q" OR A$="Q"
150 COLOR 1,0:GFX RESET:CLS:PRINT "Bye."
160 END
200 ' one player's column: the buttons that are held are lit, the sticks and the triggers are drawn
210 PROC PAD(P)
220   LOCAL X0,L,I,B,X,Y
230   X0=(P-1)*20+1
240   LOCATE X0,4:COLOR 1,0
250   IF JOY("CONNECTED",P) THEN PRINT "PLAYER ";P;"       " ELSE PRINT "player ";P;" (none)"
260   IF NOT JOY("CONNECTED",P) THEN GFX LINE (P-1)*160,60,(P-1)*160+159,260,0,BF:EXIT PROC
270   FOR I=0 TO 15
280     LOCATE X0+(I MOD 3)*6,6+I\3
290     IF JOY(BN$(I),P) THEN COLOR 1,15 ELSE COLOR 1,0
300     PRINT " ";BN$(I);" ";
310   NEXT I
320   COLOR 1,0
330   CALL STICK(P,0,(P-1)*160+8,JOY("LX",P),JOY("LY",P))
340   CALL STICK(P,1,(P-1)*160+84,JOY("RX",P),JOY("RY",P))
350   LOCATE X0,18:PRINT "LT";:CALL BAR(P,0,JOY("LT",P))
360   LOCATE X0,19:PRINT "RT";:CALL BAR(P,1,JOY("RT",P))
370 END PROC
400 ' a stick: a box of 64 x 64 pixels with a square for the stick's place
410 PROC STICK(P,S,BX,VX,VY)
420   LOCAL BY,KX,KY
430   BY=152
440   GFX LINE BX,BY,BX+63,BY+63,0,BF
450   GFX LINE BX,BY,BX+63,BY+63,6,B
460   GFX LINE BX+32,BY+1,BX+32,BY+62,5
470   GFX LINE BX+1,BY+32,BX+62,BY+32,5
480   KX=BX+32+INT(VX*28):KY=BY+32+INT(VY*28)
490   GFX LINE KX-3,KY-3,KX+3,KY+3,2,BF
500 END PROC
600 ' a trigger as a bar of up to 100 pixels
610 PROC BAR(P,T,V)
620   LOCAL BX,BY
630   BX=(P-1)*160+24:BY=18*12+T*12+2
640   GFX LINE BX,BY,BX+100,BY+7,0,BF
650   GFX LINE BX,BY,BX+100,BY+7,6,B
660   IF V>0 THEN GFX LINE BX+1,BY+1,BX+1+INT(V*98),BY+6,2,BF
670 END PROC
900 DATA "A","B","X","Y","LB","RB","BACK","START","GUIDE","LS","RS","UP","DOWN","LEFT","RIGHT","SHARE"
