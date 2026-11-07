1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
5 COLOR 140: ' bright text, so the Ready prompt stays readable after a break
10 ' Bouncing balls: gravity, bounces that lose energy, and elastic collisions of balls of different mass. Shaded with GFX CIRCLE + GFX PAINT. Two layers take turns. Any key ends.
20 RAD:RANDOMIZE:CLS:N=7:DIM X(7),Y(7),VX(7),VY(7),R(7),M(7):GFX BACKDROP RGB(0,0,24):GFX TARGET 3:B=3:GFX LAYER 0,OFF:GFX LAYER 3,OFF
30 FOR L=0 TO 3 STEP 3:GFX PALETTE L,80,RGB(255,255,255):GFX PALETTE L,81,RGB(90,100,130):FOR I=0 TO N-1:H=I*0.9
40   GFX PALETTE L,16+I*2,RGB(INT(70+60*SIN(H)),INT(70+60*SIN(H+2.1)),INT(70+60*SIN(H+4.2))):GFX PALETTE L,17+I*2,RGB(INT(170+80*SIN(H)),INT(170+80*SIN(H+2.1)),INT(170+80*SIN(H+4.2))):NEXT I:NEXT L
50 FOR I=0 TO N-1:R(I)=10+INT(RND(1)*16):M(I)=R(I)*R(I):NEXT I
60 GOSUB 200
70 REPEAT
80   T=T+1:IF T>500 THEN T=0:GOSUB 200
90   FOR I=0 TO N-1:VY(I)=VY(I)+0.25:X(I)=X(I)+VX(I):Y(I)=Y(I)+VY(I):IF Y(I)>280-R(I) THEN Y(I)=280-R(I):VY(I)=-VY(I)*0.88:VX(I)=VX(I)*0.98
100    IF X(I)<R(I) THEN X(I)=R(I):VX(I)=-VX(I)*0.95
110    IF X(I)>639-R(I) THEN X(I)=639-R(I):VX(I)=-VX(I)*0.95
120    FOR J=0 TO I-1:DX=X(J)-X(I):DY=Y(J)-Y(I):D=SQR(DX*DX+DY*DY):IF D<R(I)+R(J) AND D>0 THEN GOSUB 300
125    NEXT J
130  NEXT I
140  GFX CLEAR:GFX LINE 0,280,639,280,81
150  FOR I=0 TO N-1:C=16+I*2:GFX CIRCLE X(I),Y(I),R(I),C:GFX PAINT X(I),Y(I),C,C:Q=R(I)*0.55:GFX CIRCLE X(I)-R(I)*0.2,Y(I)-R(I)*0.2,Q,C+1:GFX PAINT X(I)-R(I)*0.2,Y(I)-R(I)*0.2,C+1,C+1
155    GFX CIRCLE X(I)-R(I)*0.35,Y(I)-R(I)*0.35,R(I)*0.15,80:NEXT I
160  GFX LAYER B,ON:GFX LAYER 3-B,OFF:B=3-B:GFX TARGET B:FRAME
170 UNTIL INKEY$<>""
180 END
200 FOR I=0 TO N-1:X(I)=40+I*80:Y(I)=40+RND(1)*100:VX(I)=RND(1)*8-4:VY(I)=RND(1)*4-2:NEXT I:RETURN
300 NX=DX/D:NY=DY/D:O=(R(I)+R(J)-D)/2:X(I)=X(I)-NX*O:Y(I)=Y(I)-NY*O:X(J)=X(J)+NX*O:Y(J)=Y(J)+NY*O
310 V=(VX(I)-VX(J))*NX+(VY(I)-VY(J))*NY:IF V>0 THEN P=2*V/(M(I)+M(J)):VX(I)=VX(I)-P*M(J)*NX:VY(I)=VY(I)-P*M(J)*NY:VX(J)=VX(J)+P*M(I)*NX:VY(J)=VY(J)+P*M(I)*NY
320 RETURN
