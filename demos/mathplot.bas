1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' Math plots: a shaded surface (ripples) and graphs of SIN, COS, SQR, LOG, ATN. Any key ends.
20 RAD:CLS:GFX LAYER 3,ON:GFX CLEAR:GFX BACKDROP RGB(0,0,24)
30 FOR I=0 TO 63:T=I/63*6.283:GFX PALETTE 3,16+I,RGB(INT(127+127*SIN(T)),INT(127+127*SIN(T+2.1)),INT(127+127*SIN(T+4.2))):NEXT I
40 FOR Y=0 TO 59:FOR X=0 TO 63
50   U=(X-32)/5:V=(Y-30)/5:R=SQR(U*U+V*V)
60   Z=SIN(R*2)/(1+R*0.3)+0.5*SIN(U)*COS(V):C=16+INT((Z+1.5)/3*63)
70   GFX LINE X*5,Y*5,X*5+4,Y*5+4,MAX(16,MIN(79,C)),BF
80 NEXT X:NEXT Y
90 GFX LINE 340,80,630,80,7:GFX LINE 340,20,340,140,7:GFX LINE 340,280,630,280,7
100 FOR P=0 TO 289
110   X=P/289*12.566-6.283:GFX PSET 340+P,80-INT(SIN(X)*50),30:GFX PSET 340+P,80-INT(COS(X)*50),60
120   X=P/289*10:GFX PSET 340+P,280-INT(SQR(X)*20),40:GFX PSET 340+P,280-INT(LOG(X+1)*20),55:GFX PSET 340+P,280-INT(ATN(X)*30),70
130 NEXT P
140 GFX TEXT 340,4,"SIN / COS  -2PI..2PI",7:GFX TEXT 340,150,"SQR  LOG(x+1)  ATN  0..10",7
150 REPEAT:FRAME 6:UNTIL INKEY$<>""
