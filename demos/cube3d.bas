1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
5 COLOR 140: ' bright text, so the Ready prompt stays readable after a break
10 ' A spinning solid cube: six colored faces, lit by a light, pulsing in size; hidden faces are skipped, the rest is outlined and filled with GFX PAINT. Two layers take turns. Any key ends.
20 RAD:CLS:DIM VX(8),VY(8),VZ(8),SX(8),SY(8),FC(6,4):GFX BACKDROP RGB(0,0,24)
30 FOR L=0 TO 3 STEP 3:GFX PALETTE L,80,RGB(255,255,255):FOR F=0 TO 5:K=F+1:FOR S=0 TO 9:Q=0.25+0.75*S/9
31   GFX PALETTE L,16+F*10+S,RGB(INT((55+200*(K AND 1))*Q),INT((55+100*(K AND 2))*Q),INT((55+50*(K AND 4))*Q)):NEXT S:NEXT F:NEXT L
40 FOR F=0 TO 5:FOR K=0 TO 3:READ FC(F,K):NEXT K:NEXT F
50 DATA 0,1,3,2, 4,6,7,5, 0,4,5,1, 2,3,7,6, 0,2,6,4, 1,5,7,3
60 GFX LAYER 0,OFF:GFX LAYER 3,OFF:GFX TARGET 3:B=3
70 REPEAT
80   GFX CLEAR:CA=COS(A):SA=SIN(A):CB=COS(B2):SB=SIN(B2):S0=48+8*SIN(A*3)
90   FOR I=0 TO 7:X=(I AND 1)*2-1:Y=(I AND 2)-1:Z=(I AND 4)/2-1:X1=X*CA+Z*SA:Z1=Z*CA-X*SA:VY(I)=Y*CB-Z1*SB:VZ(I)=Y*SB+Z1*CB:VX(I)=X1
100    F=5/(5+VZ(I)):SX(I)=320+VX(I)*F*S0:SY(I)=150+VY(I)*F*S0:NEXT I
110  FOR F=0 TO 5:CX=0:CY=0:CZ=0:PX=0:PY=0
120    FOR K=0 TO 3:P=FC(F,K):CX=CX+VX(P)/4:CY=CY+VY(P)/4:CZ=CZ+VZ(P)/4:PX=PX+SX(P)/4:PY=PY+SY(P)/4:NEXT K
130    IF CZ<-0.2 THEN C=16+F*10+MAX(0,MIN(9,INT((1.1-CX*0.4+CY*0.5-CZ*0.7)/2.2*10))):GOSUB 200
140  NEXT F
150  GFX LAYER B,ON:GFX LAYER 3-B,OFF:B=3-B:GFX TARGET B:A=A+0.03:B2=B2+0.02:FRAME
160 UNTIL INKEY$<>""
170 END
200 FOR K=0 TO 3:P=FC(F,K):Q=FC(F,(K+1) AND 3):GFX LINE SX(P),SY(P),SX(Q),SY(Q),C:NEXT K
210 GFX PAINT PX,PY,C,C
220 FOR K=0 TO 3:P=FC(F,K):Q=FC(F,(K+1) AND 3):GFX LINE SX(P),SY(P),SX(Q),SY(Q),80:NEXT K
230 RETURN
