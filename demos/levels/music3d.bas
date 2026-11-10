1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' music3d.bas - the tunes of demos/lib/music3d.lib (music strings for PLAY) with a check of their lengths: engine1999c music3d.bas [file]
20 ' writes the BASIC source of the procedure that queues a tune (MU_PLAY) and of the function with the loop lengths (MU_LEN), and prints the length of every tune in seconds.
30 ' (A tune is one music string per voice; all voices of a tune must be equally long, else the loop drifts.)
40 DIM TUN$(40,8),TNV(40),TST(40),TMD(40)
50 NT=0
60 ' the length of a music string in quarter notes (T and L as in mml.c)
70 FUNC CH$(I,S$)
80   LOCAL C$
90   IF I>LEN(S$) THEN RETURN ""
100   C$=MID$(S$,I,1)
110   IF C$=";" THEN C$=" "
120   RETURN C$
130 END FUNC
140 FUNC ISNUM(C$)
150   IF C$="" THEN RETURN 0
160   RETURN (C$>="0" AND C$<="9")
170 END FUNC
180 FUNC INSET(C$,S$)
190   LOCAL I
200   IF C$="" THEN RETURN 0
210   FOR I=1 TO LEN(S$)
220     IF MID$(S$,I,1)=C$ THEN RETURN -1
230   NEXT I
240   RETURN 0
250 END FUNC
260 FUNC SKIPWS(I,S$)
270   LOCAL J
280   J=I
290   WHILE FN CH$(J,S$)=" "
300     J=J+1
310   WEND
320   RETURN J
330 END FUNC
340 FUNC BEATS(S$)
350   LOCAL L,TOT,I,N,C$,J,K,T$,V,B,F,D,NN,DOTS
360   L=4:TOT=0:I=1:N=LEN(S$)
370   WHILE I<=N
380     C$=FN CH$(I,S$)
390     IF C$="M" AND FN INSET(FN CH$(I+1,S$),"LNS") THEN
400       I=I+2
410     ELSE
420       IF FN INSET(C$,"TOLVWI") THEN
430         J=FN SKIPWS(I+1,S$):K=J
440         WHILE FN ISNUM(FN CH$(K,S$)) OR FN CH$(K,S$)="."
450           K=K+1
460         WEND
470         IF K>J THEN
480           IF C$="L" THEN L=INT(VAL(MID$(S$,J,K-J)))
490           I=K
500         ELSE
510           I=I+1
520         END IF
530       ELSE
540         IF FN INSET(C$,"<>") THEN
550           I=I+1
560         ELSE
570           IF FN INSET(C$,"ABCDEFGRP") THEN
580             J=I+1
590             IF FN INSET(FN CH$(J,S$),"#+-") THEN J=J+1
600             J=FN SKIPWS(J,S$):K=J
610             WHILE FN ISNUM(FN CH$(K,S$))
620               K=K+1
630             WEND
640             IF K>J THEN NN=INT(VAL(MID$(S$,J,K-J))) ELSE NN=L
650             DOTS=0
660             WHILE FN CH$(K,S$)="."
670               K=K+1:DOTS=DOTS+1
680             WEND
690             B=4/NN:F=B:D=B
700             FOR V=1 TO DOTS
710               D=D/2:F=F+D
720             NEXT V
730             TOT=TOT+F
740             I=K
750           ELSE
760             I=I+1
770           END IF
780         END IF
790       END IF
800     END IF
810   WEND
820   RETURN TOT
830 END FUNC
840 ' the tempo: the number behind the first T
850 FUNC TEMPOOF(S$)
860   LOCAL I,J,K
870   FOR I=1 TO LEN(S$)
880     IF MID$(S$,I,1)="T" THEN
890       J=FN SKIPWS(I+1,S$):K=J
900       WHILE FN ISNUM(FN CH$(K,S$))
910         K=K+1
920       WEND
930       IF K>J THEN RETURN INT(VAL(MID$(S$,J,K-J)))
940     END IF
950   NEXT I
960   RETURN 0
970 END FUNC
980 ' S$ repeated N times, separated by spaces
990 FUNC REP$(S$,N)
1000   LOCAL I,R$
1010   R$=""
1020   FOR I=1 TO N
1030     IF I>1 THEN R$=R$+" "
1040     R$=R$+S$
1050   NEXT I
1060   RETURN R$
1070 END FUNC
1080 FUNC NUMF$(X,P)
1090   LOCAL M,N,S$
1100   M=10
1110   IF P=2 THEN M=100
1120   IF P=3 THEN M=1000
1130   N=INT(X*M+0.5):S$=STR$(N)
1140   WHILE LEN(S$)<=P
1150     S$="0"+S$
1160   WEND
1170   RETURN LEFT$(S$,LEN(S$)-P)+"."+RIGHT$(S$,P)
1180 END FUNC
1190 PROC TUNE(ST,MD)
1200   TST(NT)=ST:TMD(NT)=MD:TNV(NT)=0:NT=NT+1
1210 END PROC
1220 PROC V(S$)
1230   TUN$(NT-1,TNV(NT-1))=S$:TNV(NT-1)=TNV(NT-1)+1
1240 END PROC
1250 FUNC BASSLINE$(T,I,VOL$,NOTES$)
1260   RETURN "T"+STR$(T)+" I"+STR$(I)+" V"+VOL$+" MN O2 L2 "+NOTES$
1270 END FUNC
1280 ' ---------------------------------------------------------------------------------------------------------- Deqq's House II (set 2): D minor, slow and mysterious
1290 BASS$="D A D A B- F A E D A G D A E D D"
1300 HEART$=FN REP$("C16 R16 C16 R16",32)
1310 CALM1$=FN BASSLINE$(54,4,"0.44",BASS$)
1320 CALM2$="T54 I6 V0.12 ML O3 L1 F F D C# F B- C# A"
1330 CALM3$="T54 I19 V0.38 O4 L4 A2 G F E2. R4 F2 D F E2 C#2 D F A2 G2 B- A C#2. E D1"
1340 CALL TUNE(2,2):CALL V(CALM1$):CALL V(CALM2$):CALL V(CALM3$)
1350 CALL TUNE(2,3):CALL V(CALM1$):CALL V(CALM2$):CALL V(CALM3$):CALL V("T54 I13 V0.12 ML O1 L16 "+HEART$)
1360 SHIMMER$="D F A F D F A F D F A F D F A F B- D F D B- D F D A C# E C# A C# E C# D F A F D F A F G B- D B- G B- D B- A C# E C# A C# E C# D F A F D F A F"
1370 CALL TUNE(2,1):CALL V(CALM1$):CALL V(CALM2$):CALL V(CALM3$):CALL V("T54 I12 V0.06 ML O5 L8 "+SHIMMER$)
1380 DB$="T150 I4 V0.31 MN O2 L8 "+FN REP$("DDDDDDDD DDDDDDDD B-B-B-B-B-B-B-B- AAAAAAAA",2)
1390 ARPS$=""
1400 FOR I=1 TO 8
1410   IF I=1 OR I=5 THEN B$="O4 D F A O5 D"
1420   IF I=2 OR I=6 THEN B$="O4 D F A O5 D"
1430   IF I=3 OR I=7 THEN B$="O4 B- O5 D F B-"
1440   IF I=4 OR I=8 THEN B$="O4 A O5 C# E A"
1450   IF I>1 THEN ARPS$=ARPS$+" "
1460   ARPS$=ARPS$+FN REP$(B$,4)
1470 NEXT I
1480 DSN$="T150 I14 V0.19 ML O3 L8 "+FN REP$("R4 C R R4 C R",8)
1490 DLEAD$="T150 I5 V0.27 O5 L8 D D F D A4 G4 F E D C# D4 R4 D D F D B-4 A4 E E A E C#4 E4 D D F D A4 G4 F E D C# D4 A4 D D F D B-4 A4 E E A E C#4 R4"
1500 CALL TUNE(2,4):CALL V(DB$):CALL V("T150 I7 V0.09 ML L16 "+ARPS$):CALL V(DSN$):CALL V(DLEAD$)
1510 CALL TUNE(2,5):CALL V("T100 I9 V0.34 O4 L4 E D C# O3 A2 R2 R4"):CALL V("T100 I4 V0.66 O2 L2 A A A R2")
1520 EB$="D A A E G D D A G D A E D A D D"
1530 CALL TUNE(2,6):CALL V(FN BASSLINE$(72,4,"0.46",EB$)):CALL V("T72 I6 V0.14 ML O3 L1 F# C# B F# B C# F# A"):CALL V("T72 I10 V0.40 O4 L4 F#2 A B C#2 E2 G2 B A F#2. E G2 B2 A2 C#2 F#2 A2 D1")
1540 ' ---------------------------------------------------------------------------------------------------------- Bug Hunt (set 1): A minor, a bunker
1550 BB$="A A F E A A F E"
1560 BH1$="T66 I4 V0.62 MN O2 L1 "+BB$
1570 BH2$="T66 I6 V0.18 ML O3 L1 C C A G# C C A G#"
1580 BH3$="T66 I12 V0.11 MN O5 L8 R2 E4 R4 R4 C4 R2 R2 C4 R4 R4 B4 R2 R2 E4 R4 R4 C4 R2 R2 A4 R4 R4 G#4 R2"
1590 CALL TUNE(1,2):CALL V(BH1$):CALL V(BH2$):CALL V(BH3$)
1600 CALL TUNE(1,3):CALL V(BH1$):CALL V(BH2$):CALL V(BH3$):CALL V("T66 I13 V0.16 ML O1 L16 "+HEART$)
1610 CALL TUNE(1,1):CALL V("T84 I4 V0.37 MN O2 L4 A A A A A A A A F F F F E E E E A A A A A A A A F F F F E E E E"):CALL V("T84 I6 V0.26 ML O3 L1 C C A G# C C A G#"):CALL V("T84 I5 V0.29 O4 L4 A2 C E B2 A G# A2 C F E2 G# B A2 C E D2 C B C2 A F E1")
1620 BD$="T160 I4 V0.31 MN O2 L8 "+FN REP$("AAAAAAAA AAAAAAAA FFFFFFFF EEEEEEEE",2)
1630 BARP$=""
1640 FOR I=1 TO 8
1650   IF I=1 OR I=2 OR I=5 OR I=6 THEN B$="O4 A O5 C E A"
1660   IF I=3 OR I=7 THEN B$="O4 F A O5 C F"
1670   IF I=4 OR I=8 THEN B$="O4 E G# B E"
1680   IF I>1 THEN BARP$=BARP$+" "
1690   BARP$=BARP$+FN REP$(B$,4)
1700 NEXT I
1710 BSN$="T160 I14 V0.19 ML O3 L8 "+FN REP$("R4 C R R4 C R",8)
1720 BLEAD$="T160 I5 V0.27 O5 L8 A A C A E4 D4 C B A G# A4 R4 F F A F C4 B-4 E E G# E B4 G#4 A A C A E4 D4 C B A G# A4 E4 F F A F C4 B-4 E E G# E B4 R4"
1730 CALL TUNE(1,4):CALL V(BD$):CALL V("T160 I7 V0.09 ML L16 "+BARP$):CALL V(BSN$):CALL V(BLEAD$)
1740 CALL TUNE(1,5):CALL V("T60 I9 V0.38 O4 L2 A F E1 D1 R1"):CALL V("T60 I4 V0.62 O2 L1 A D E1 R1")
1750 CALL TUNE(1,6):CALL V("T120 I10 V0.38 O4 L8 C E G O5 C4 O4 G E C4 R4 F A O5 C F4 C O4 A4 R4 G B O5 D G4 R4 C1"):CALL V("T120 I4 V0.62 O2 L2 C G F C G C1 R1")
1760 ' ---------------------------------------------------------------------------------------------------------- check, report, write
1770 Q$=CHR$(34)
1780 F$=ARG$(1):IF F$="" THEN F$="stdout:"
1790 DIM SECS(40),BT(40),LNG(40)
1800 FOR T=0 TO NT-1
1810   B0=FN BEATS(TUN$(T,0)):T0=FN TEMPOOF(TUN$(T,0)):LONG=0
1820   FOR J=0 TO TNV(T)-1
1830     IF FN TEMPOOF(TUN$(T,J))<>T0 THEN
1840       PRINT "tempos differ in set";TST(T);"mood";TMD(T)
1850       BYE 1
1860     END IF
1870     IF ABS(FN BEATS(TUN$(T,J))-B0)>=0.000001 THEN
1880       PRINT "voices of different length in set";TST(T);"mood";TMD(T)
1890       BYE 1
1900     END IF
1910     IF LEN(TUN$(T,J))>LONG THEN LONG=LEN(TUN$(T,J))
1920   NEXT J
1930   SECS(T)=B0*60/T0
1940   BT(T)=B0:LNG(T)=LONG
1950 NEXT T
1960 FOR S=1 TO 2
1970   FOR M=1 TO 6
1980     FOR T=0 TO NT-1
1990       IF TST(T)=S AND TMD(T)=M THEN
2000         B$=STR$(BT(T)):IF BT(T)=INT(BT(T)) THEN B$=B$+".0"
2010         PRINT "set ";STR$(S);" mood ";STR$(M);" beats ";B$;" seconds ";FN NUMF$(SECS(T),2);" voices ";STR$(TNV(T));" longest string ";STR$(LNG(T))
2020       END IF
2030     NEXT T
2040   NEXT M
2050 NEXT S
2060 OPEN F$ FOR OUTPUT AS #1
2070 PRINT #1,"PROC MU_PLAY"
2080 FOR S=1 TO 2
2090   FOR M=1 TO 6
2100     FOR T=0 TO NT-1
2110       IF TST(T)=S AND TMD(T)=M THEN
2120         L$="  IF MU_SET="+STR$(S)+" AND MU_CUR="+STR$(M)+" THEN PLAY "
2130         FOR J=0 TO TNV(T)-1
2140           IF J>0 THEN L$=L$+","
2150           L$=L$+Q$+TUN$(T,J)+Q$
2160         NEXT J
2170         PRINT #1,L$
2180       END IF
2190     NEXT T
2200   NEXT M
2210 NEXT S
2220 PRINT #1,"END PROC"
2230 PRINT #1,"FUNC MU_LEN(M)"
2240 FOR S=1 TO 2
2250   FOR M=1 TO 6
2260     FOR T=0 TO NT-1
2270       IF TST(T)=S AND TMD(T)=M THEN PRINT #1,"  IF MU_SET="+STR$(S)+" AND M="+STR$(M)+" THEN RETURN "+FN NUMF$(SECS(T),3)
2280     NEXT T
2290   NEXT M
2300 NEXT S
2310 PRINT #1,"  RETURN 10"
2320 PRINT #1,"END FUNC"
2330 CLOSE #1
