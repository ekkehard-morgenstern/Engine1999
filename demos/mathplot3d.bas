1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' A rotating 3D wireframe of Z=2*SIN(2R)/(1+R). Two layers take turns (draw hidden, then show): no flicker. Any key ends.
20 RAD:CLS:DIM Z(21,21),PX(21,21),PY(21,21):T=0.9:CT=COS(T):ST=SIN(T):GFX BACKDROP RGB(0,0,24)
30 FOR L=0 TO 3 STEP 3:FOR I=0 TO 63:H=I/63*4.2
40   GFX PALETTE L,16+I,RGB(INT(127+127*SIN(H)),INT(127+127*SIN(H+2.1)),INT(127+127*SIN(H+4.2))):NEXT I:NEXT L
50 FOR I=0 TO 20:FOR J=0 TO 20:R=SQR((I-10)^2+(J-10)^2)*0.3:Z(I,J)=2*SIN(R*2)/(1+R):NEXT J:NEXT I
60 GFX LAYER 0,OFF:GFX LAYER 3,OFF:GFX TARGET 3:B=3
70 REPEAT
80   GFX CLEAR:CA=COS(A):SA=SIN(A)
90   FOR I=0 TO 20:FOR J=0 TO 20:X=(I-10)*0.3:Y=(J-10)*0.3:XR=X*CA-Y*SA:YR=X*SA+Y*CA
100    F=9/(9+YR*CT-Z(I,J)*ST):PX(I,J)=320+XR*F*38:PY(I,J)=150-(YR*ST+Z(I,J)*CT)*F*38:NEXT J:NEXT I
110  FOR I=0 TO 20:FOR J=0 TO 20:C=MAX(16,MIN(79,16+INT((Z(I,J)+1)/2.6*63)))
120    IF I<20 THEN GFX LINE PX(I,J),PY(I,J),PX(I+1,J),PY(I+1,J),C
130    IF J<20 THEN GFX LINE PX(I,J),PY(I,J),PX(I,J+1),PY(I,J+1),C
140  NEXT J:NEXT I
150  GFX LAYER B,ON:GFX LAYER 3-B,OFF:B=3-B:GFX TARGET B:A=A+0.04:FRAME
160 UNTIL INKEY$<>""
