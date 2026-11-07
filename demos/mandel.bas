1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' Mandelbrot zoom into the Seahorse Valley: 160 x 75 cells, each zoom step halves the view and adds iterations. Any key ends.
20 RAD:CLS:GFX LAYER 3,ON:GFX CLEAR:GFX BACKDROP RGB(0,0,0):GFX PALETTE 3,15,RGB(255,255,255):GFX PALETTE 3,16,RGB(0,0,0)
30 FOR I=0 TO 63:H=I/64*6.283:GFX PALETTE 3,17+I,RGB(INT(127+127*SIN(H)),INT(127+127*SIN(H+0.8)),INT(127+127*SIN(H+2))):NEXT I
40 CX=-0.743643887037151:CY=0.13182590420533:W=3.2:Z=0
50 REPEAT
60   MI=60+Z*12
70   FOR PY=0 TO 74:FOR PX=0 TO 159:A=CX+(PX-80)/160*W:B=CY+(PY-37)/160*W:X=0:Y=0:N=0
80     WHILE N<MI AND X*X+Y*Y<4:T=X*X-Y*Y+A:Y=2*X*Y+B:X=T:N=N+1:WEND
90     C=16:IF N<MI THEN C=17+N-INT(N/64)*64
100    GFX LINE PX*4,PY*4,PX*4+3,PY*4+3,C,BF
110  NEXT PX
120  IF INKEY$<>"" THEN END
130  NEXT PY
140  GFX LINE 2,2,110,16,16,BF:GFX TEXT 4,4,"zoom x"+STR$(INT(2^Z)),15:FRAME 60
150  W=W/2:Z=Z+1:IF Z>14 THEN W=3.2:Z=0
160 UNTIL 0
