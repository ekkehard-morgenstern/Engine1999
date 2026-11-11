1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' A tool of Engine1999: start it with TOOL "insted" (or TOOL "insted","file" to open an instrument file) from the prompt or from a program;
20 ' the program you are working on stays as it is. It can also be run as a program of its own (LOAD "tools/insted", RUN; ARG$(1) is read the same way).
30 ' INSTRUMENT EDITOR - edits the definition string of a synthesizer instrument (INSTRUMENT n,a$ and I n in PLAY, see docs/sound.md "Instruments") with
40 ' sliders and lets you hear it. It is written in BASIC on the UI subsystem and uses the libraries uikit (sliders, dialogs) and filereq (file requester).
50 ' Needs the window (engine1999).
60 '   Six pages (a tab strip): Osc 1, Osc 2, Osc 3 (wave, level, frequency ratio or a fixed frequency in Hz, detune in cents, pulse width, Post = behind
70 '   the filter, On = the oscillator is used), Filter (cutoff as a multiple of the note, resonance, filter envelope, pitch envelope) and Env
80 '   (the instrument's own ADSR envelope, vibrato, gain). The buttons < and > step through the 16 factory instruments, New starts from a plain sine.
90 '   Play plays the note (the slider Note, MIDI numbers), Chord three notes together, Stop silences. Hold keeps the note sounding until Stop: every
100 '   change of a control is heard at once (the instrument is redefined while it sounds), and the Note slider changes its pitch. Play on change plays
110 '   the note on every change. Every change rebuilds the definition string below; you may also type or paste a string and press Return.
120 '   Save keeps the string in a text file (.ins), Open reads one, Export writes the BASIC line  INSTRUMENT n,"..."  that you can MERGE or LOAD,
130 '   Copy puts the string on the clipboard. The tool uses instrument 31 and the sound channel 1 (and 2, 3 for the chord).
140 ' widget ids: 1 form, 200 prev, 201 next, 202 preset name, 203 New, 204 / 205 name, 210 the tab strip (Osc 1..3, Filter, Env, FM), 217 Hold, 216 Play on change, the oscillator page (shared by the
150 ' three oscillators): 300..304 wave radios, checks On 305, Fixed 306, Post 307, sliders Level 310, Ratio 313, Cents 316, Hz 319, Width 322, filter page 500.. (cutoff, resonance, filter env,
160 ' filter decay, pitch, pitch time), env page 520 (check), 523.. (attack, decay, sustain, release, vib rate, vib depth, gain), 560 / 561 the string,
170 ' 570 note slider, 575 note label, buttons 580..587, 590 status
180 LOAD LIBRARY "uikit"
190 LOAD LIBRARY "filereq"
200 REM the graphics of the calling program stay (no GFX RESET)
210 CLS
220 BREAK OFF
230 Q=0
240 DR$=""
250 HELD=0
260 LASTP=0
270 PRN=0
280 CURPG=0
290 CUROSC=0
300 NM$=""
310 DIM OW(3),OEN(3),OFX(3),OPO(3),OFM(3),OMD(3),OL(3),RT(3),OC(3),OHZ(3),OP(3),PGR$(6),XC$(40),WN$(5)
320 WN$(0)="SINE":WN$(1)="SQUARE":WN$(2)="TRIANGLE":WN$(3)="NOISE":WN$(4)="SAW"
330 ' the model, in the units of the sliders (the controls follow it, a loaded string sets it exactly): OL level / 100, RT ratio / 100, OC cents, OP pulse width / 100,
340 ' OHZ the fixed frequency in Hz (RT is not used while OFX is set), CUT cutoff / 10, RES / 100, FEN filter env / 10, FDC filter decay / 100, PIT semitones,
350 ' PTM pitch time / 100, EA ED ER attack decay release in ms, ES sustain / 100, VR vibrato rate / 10, VD depth / 100, GN gain / 100
360 ' ====================================================================== functions (they must stand before the lines that use them)
370 FUNC UP$(S$)
380   LOCAL I,C,R$
390   R$=""
400   FOR I=1 TO LEN(S$)
410     C=ASC(MID$(S$,I,1))
420     IF C>=97 AND C<=122 THEN C=C-32
430     R$=R$+CHR$(C)
440   NEXT I
450   RETURN R$
460 END FUNC
470 ' a number with at most three decimals, as text
480 FUNC NUM$(V)
490   RETURN STR$(INT(V*1000+0.5)/1000)
500 END FUNC
510 ' the I-th (0..) blank / comma separated word of S$, "" if there is none
520 FUNC TOKN$(S$,N)
530   LOCAL I,K,C$,T$,INW
540   K=0:T$="":INW=0
550   FOR I=1 TO LEN(S$)+1
560     IF I<=LEN(S$) THEN C$=MID$(S$,I,1) ELSE C$=" "
570     IF C$=" " OR C$="," OR C$=CHR$(9) THEN
580       IF INW THEN
590         IF K=N THEN RETURN T$
600         K=K+1:T$="":INW=0
610       END IF
620     ELSE
630       T$=T$+C$:INW=-1
640     END IF
650   NEXT I
660   RETURN ""
670 END FUNC
680 FUNC ISNUM(T$)
690   LOCAL C$
700   IF T$="" THEN RETURN 0
710   C$=LEFT$(T$,1)
720   RETURN (C$>="0" AND C$<="9") OR C$="-" OR C$="." OR C$="+"
730 END FUNC
740 FUNC WAVENO(T$)
750   LOCAL I,U$
760   U$=FN UP$(T$)
770   FOR I=0 TO 4
780     IF U$=WN$(I) THEN RETURN I
790   NEXT I
800   IF FN ISNUM(T$) THEN
810     I=INT(VAL(T$))
820     IF I<0 OR I>4 THEN I=0
830     RETURN I
840   END IF
850   RETURN 0
860 END FUNC
870 ' the pitch of slider position P (0..300) in Hz: 20 Hz .. 20 kHz on a logarithmic scale
880 FUNC HZ(P)
890   RETURN INT(20*EXP(P/300*LOG(1000))+0.5)
900 END FUNC
910 FUNC POSOF(F)
920   LOCAL P
930   IF F<20 THEN RETURN 0
940   P=INT(300*LOG(F/20)/LOG(1000)+0.5)
950   IF P>300 THEN P=300
960   RETURN P
970 END FUNC
980 FUNC NOTEF(N)
990   RETURN 440*(2^((N-69)/12))
1000 END FUNC
1010 FUNC BOOL(V)
1020   IF V<>0 THEN RETURN -1
1030   RETURN 0
1040 END FUNC
1050 ' a name made of letters, digits and _ only, 11 characters at most
1060 FUNC CLEANNAME$(S$)
1070   LOCAL I,C,R$
1080   R$=""
1090   FOR I=1 TO LEN(S$)
1100     C=ASC(MID$(S$,I,1))
1110     IF (C>=48 AND C<=57) OR (C>=65 AND C<=90) OR (C>=97 AND C<=122) OR C=95 THEN R$=R$+CHR$(C)
1120   NEXT I
1130   RETURN LEFT$(FN UP$(R$),11)
1140 END FUNC
1150 ' the definition string made from the model
1160 FUNC MAKE$()
1170   LOCAL A$,K,R$
1180   A$="NAME "+NM$
1190   IF NM$="" THEN A$="NAME USER"
1200   FOR K=0 TO 2
1210     IF OEN(K) THEN
1220       IF OFX(K) THEN
1230         R$=STR$(-OHZ(K))
1240       ELSE
1250         R$=FN NUM$(RT(K)/100)
1260         IF RT(K)=0 AND OW(K)<>3 THEN R$="0.01"
1270       END IF
1280       A$=A$+"; OSC "+WN$(OW(K))+" "+FN NUM$(OL(K)/100)+" "+R$+" "+FN NUM$(OC(K))+" "+FN NUM$(OP(K)/100)
1285       IF OFM(K)>0 THEN A$=A$+" FM "+FN NUM$(OFM(K)/100)
1287       IF OMD(K) THEN A$=A$+" MOD"
1290       IF OPO(K) THEN A$=A$+" POST"
1300     END IF
1310   NEXT K
1320   IF CUT>0 THEN A$=A$+"; FILTER "+FN NUM$(CUT/10)+" "+FN NUM$(RES/100)+" "+FN NUM$(FEN/10)+" "+FN NUM$(FDC/100)
1330   IF PIT<>0 THEN A$=A$+"; PITCH "+FN NUM$(PIT)+" "+FN NUM$(PTM/100)
1340   IF ENVON THEN A$=A$+"; ENV "+FN NUM$(EA/1000)+" "+FN NUM$(ED/1000)+" "+FN NUM$(ES/100)+" "+FN NUM$(ER/1000)
1350   IF VD>0 AND VR>0 THEN A$=A$+"; VIB "+FN NUM$(VR/10)+" "+FN NUM$(VD/100)
1355   IF FMD>0 THEN A$=A$+"; FMDECAY "+FN NUM$(FMD/100)
1357   IF PLY>1 THEN A$=A$+"; POLY "+STR$(PLY)
1360   A$=A$+"; GAIN "+FN NUM$(GN/100)
1370   RETURN A$
1380 END FUNC
1390 ' ====================================================================== the model
1400 PROC DEFAULTS
1410   LOCAL K
1420   FOR K=0 TO 2
1430     OW(K)=0:OEN(K)=0:OFX(K)=0:OPO(K)=0:OL(K)=100:RT(K)=100:OC(K)=0:OHZ(K)=1000:OP(K)=50:OFM(K)=0:OMD(K)=0
1440   NEXT K
1450   OEN(0)=-1
1460   CUT=0:RES=0:FEN=0:FDC=20:PIT=0:PTM=0
1470   ENVON=0:EA=0:ED=0:ES=100:ER=10
1480   VR=0:VD=0:GN=100:FMD=0:PLY=1
1490   NM$=""
1500 END PROC
1510 ' splits A$ at the semicolons into XC$() (empty ones left out); XN is their number
1520 PROC SPLIT(A$)
1530   LOCAL I,C$,T$
1540   XN=0
1550   T$=""
1560   FOR I=1 TO LEN(A$)+1
1570     IF I<=LEN(A$) THEN C$=MID$(A$,I,1) ELSE C$=";"
1580     IF C$=";" THEN
1590       IF FN TOKN$(T$,0)<>"" AND XN<40 THEN
1600         XC$(XN)=T$:XN=XN+1
1610       END IF
1620       T$=""
1630     ELSE
1640       T$=T$+C$
1650     END IF
1660   NEXT I
1670 END PROC
1680 ' one OSC command into the oscillator K
1690 PROC PARSEOSC(C$,K)
1700   LOCAL J,T$,V0,V1,V2,V3,H0,H1,H2,H3,R
1710   OW(K)=FN WAVENO(FN TOKN$(C$,1))
1720   H0=0:H1=0:H2=0:H3=0:V0=1:V1=1:V2=0:V3=0.5
1730   IF OW(K)=3 THEN V1=0
1740   T$=FN TOKN$(C$,2)
1750   IF FN ISNUM(T$) THEN V0=VAL(T$)
1760   T$=FN TOKN$(C$,3)
1770   IF FN ISNUM(T$) THEN V1=VAL(T$)
1780   T$=FN TOKN$(C$,4)
1790   IF FN ISNUM(T$) THEN V2=VAL(T$)
1800   T$=FN TOKN$(C$,5)
1810   IF FN ISNUM(T$) THEN V3=VAL(T$)
1820   OL(K)=INT(V0*100+0.5)
1830   IF V1<0 THEN
1840     OFX(K)=-1:OHZ(K)=INT(-V1+0.5)
1850     IF OHZ(K)<1 THEN OHZ(K)=1
1860     IF OHZ(K)>20000 THEN OHZ(K)=20000
1870     RT(K)=100
1880   ELSE
1890     OFX(K)=0:RT(K)=INT(V1*100+0.5)
1900     IF RT(K)>6400 THEN RT(K)=6400
1910   END IF
1920   OC(K)=INT(V2+0.5)
1930   OP(K)=INT(V3*100+0.5)
1940   OPO(K)=0:OMD(K)=0:OFM(K)=0
1950   FOR J=2 TO 12
1960     T$=FN UP$(FN TOKN$(C$,J))
1962     IF T$="POST" THEN OPO(K)=-1
1964     IF T$="MOD" THEN OMD(K)=-1
1966     IF T$="FM" THEN OFM(K)=INT(VAL(FN TOKN$(C$,J+1))*100+0.5)
1970   NEXT J
1980 END PROC
1990 ' sets the model from a definition string (what the string does not say stays at the default of a plain sine)
2000 PROC PARSE(A$)
2010   LOCAL I,OI,C$,W$
2020   CALL DEFAULTS
2030   CALL SPLIT(A$)
2040   OI=0
2050   FOR I=0 TO XN-1
2060     C$=XC$(I)
2070     W$=FN UP$(FN TOKN$(C$,0))
2080     IF W$="NAME" THEN NM$=FN CLEANNAME$(FN TOKN$(C$,1))
2090     IF W$="OSC" AND OI<3 THEN
2100       CALL PARSEOSC(C$,OI)
2110       OI=OI+1
2120     END IF
2130     IF W$="FILTER" THEN
2140       CUT=INT(VAL(FN TOKN$(C$,1))*10+0.5)
2150       RES=INT(VAL(FN TOKN$(C$,2))*100+0.5)
2160       FEN=INT(VAL(FN TOKN$(C$,3))*10+0.5)
2170       FDC=20
2180       IF FN TOKN$(C$,4)<>"" THEN FDC=INT(VAL(FN TOKN$(C$,4))*100+0.5)
2190     END IF
2200     IF W$="PITCH" THEN
2210       PIT=INT(VAL(FN TOKN$(C$,1))+0.5)
2220       PTM=INT(VAL(FN TOKN$(C$,2))*100+0.5)
2230     END IF
2240     IF W$="ENV" THEN
2250       ENVON=-1
2260       EA=INT(VAL(FN TOKN$(C$,1))*1000+0.5)
2270       ED=INT(VAL(FN TOKN$(C$,2))*1000+0.5)
2280       ES=INT(VAL(FN TOKN$(C$,3))*100+0.5)
2290       ER=INT(VAL(FN TOKN$(C$,4))*1000+0.5)
2300     END IF
2310     IF W$="VIB" THEN
2320       VR=INT(VAL(FN TOKN$(C$,1))*10+0.5)
2330       VD=INT(VAL(FN TOKN$(C$,2))*100+0.5)
2340     END IF
2345     IF W$="FMDECAY" THEN FMD=INT(VAL(FN TOKN$(C$,1))*100+0.5)
2347     IF W$="POLY" THEN PLY=INT(VAL(FN TOKN$(C$,1)))
2350     IF W$="GAIN" THEN GN=INT(VAL(FN TOKN$(C$,1))*100+0.5)
2360   NEXT I
2370   IF OI>0 THEN
2380     FOR I=0 TO 2
2390       OEN(I)=(I<OI)
2400     NEXT I
2410   END IF
2420 END PROC
2430 ' ====================================================================== main program
2440 IF ENV$("INSTED_HEADLESS")<>"" THEN
2450   CALL SELFTEST
2460   END
2470 END IF
2480 CUROSC=0
2490 UI OPEN
2500 CALL MAKEFORM
2510 CALL DEFAULTS
2520 CALL LOADPRESET(0)
2530 CALL SHOWPAGE(0)
2540 CALL SETSTATUS("Pick an instrument with < and >, or move the sliders. Play plays the note.")
2550 ON ERROR GOTO ERRH
2560 ARGLOAD$=""
2570 IF ARGC>=1 THEN ARGLOAD$=ARG$(1)
2580 IF ARGLOAD$<>"" THEN CALL OPENF(ARGLOAD$)
2590 LABEL LOOPTOP
2600 ON ERROR GOTO ERRH
2610 ON UI GOSUB HANDLER
2620 REPEAT
2630   GFX VSYNC
2640   UI UPDATE
2650 UNTIL Q=1
2660 ON UI OFF
2670 ON ERROR OFF
2680 SOUND OFF
2690 UI CLOSE
2700 REM no GFX RESET here
2710 CLS
2720 PRINT "Instrument editor finished."
2730 END
2740 LABEL ERRH
2750 ON UI OFF
2760 CALL UK_MSG("Error",ERR$+"|(line "+STR$(ERL)+")")
2770 RESUME LOOPTOP
2780 ' ====================================================================== the form
2790 ' registers the widget ids ID..ID+N-1 with the page P (a page is shown or hidden as a whole)
2800 PROC REGP(P,ID,N)
2810   LOCAL I
2820   FOR I=0 TO N-1
2830     PGR$(P)=PGR$(P)+MKI$(ID+I)
2840   NEXT I
2850 END PROC
2860 ' a slider of the page P: UK_SLIDER makes the widgets ID, ID+1 (label), ID+2 (readout)
2870 PROC PSLIDER(ID,X,Y,LB$,MN,MX,V,DV)
2880   CALL UK_DIVISOR(DV)
2890   CALL UK_SLIDER(ID,X,Y,304,LB$,MN,MX,V)
2900   CALL REGP(PGP,ID,3)
2910 END PROC
2920 PROC MAKEOSC
2930   UI RADIO 300,10,72,60,16,"Sine",10
2940   UI RADIO 301,72,72,70,16,"Square",10
2950   UI RADIO 302,146,72,76,16,"Triangle",10
2960   UI RADIO 303,226,72,60,16,"Noise",10
2970   UI RADIO 304,290,72,50,16,"Saw",10
2980   UI CHECK 305,350,72,44,16,"On"
2990   UI CHECK 306,398,72,60,16,"Fixed"
3000   UI CHECK 307,462,72,56,16,"Post"
3010   UI CHECK 308,522,72,50,16,"Mod"
3011   CALL REGP(0,300,9)
3020   PGP=0:CALL PSLIDER(310,10,98,"Level",0,200,100,100)
3030   PGP=0:CALL PSLIDER(313,10,120,"Ratio",0,2000,100,100)
3040   PGP=0:CALL PSLIDER(316,10,142,"Cents",-2400,2400,0,1)
3050   PGP=0:CALL PSLIDER(319,326,98,"Fixed Hz",0,300,150,1)
3060   PGP=0:CALL PSLIDER(322,326,120,"Width",2,98,50,100)
3065   PGP=0:CALL PSLIDER(325,326,142,"FM index",0,2000,0,100)
3070 END PROC
3080 PROC MAKEFORM
3090   UI PANEL 1,0,0,640,300,"Instrument editor"
3100   UI BUTTON 200,10,22,26,20,"<"
3110   UI BUTTON 201,40,22,26,20,">"
3120   UI LABEL 202,72,25,120,14,""
3130   UI BUTTON 203,196,22,44,20,"New"
3140   UI LABEL 204,250,25,40,14,"Name"
3150   UI TEXT 205,292,22,120,20,""
3160   UI TAB 210,10,44,340,22,"Osc 1|Osc 2|Osc 3|Filter|Env|FM"
3220   UI CHECK 217,366,46,50,16,"Hold"
3230   UI CHECK 216,420,46,130,16,"Play on change"
3240   CALL MAKEOSC
3250   PGP=3:CALL PSLIDER(500,10,76,"Cutoff",0,640,0,10)
3260   PGP=3:CALL PSLIDER(503,10,98,"Resonance",0,95,0,100)
3270   PGP=3:CALL PSLIDER(506,10,120,"Filt env",-60,60,0,10)
3280   PGP=3:CALL PSLIDER(509,326,76,"Filt decay",0,1000,20,100)
3290   PGP=3:CALL PSLIDER(512,326,98,"Pitch env",-48,48,0,1)
3300   PGP=3:CALL PSLIDER(515,326,120,"Pitch time",0,1000,0,100)
3310   UI CHECK 520,326,142,140,16,"Own envelope"
3320   CALL REGP(4,520,1)
3330   PGP=4:CALL PSLIDER(523,10,76,"Attack ms",0,2000,0,1)
3340   PGP=4:CALL PSLIDER(526,10,98,"Decay ms",0,2000,0,1)
3350   PGP=4:CALL PSLIDER(529,10,120,"Sustain",0,100,100,100)
3360   PGP=4:CALL PSLIDER(532,10,142,"Release ms",0,2000,100,1)
3370   PGP=4:CALL PSLIDER(535,326,76,"Vib rate",0,200,0,10)
3380   PGP=4:CALL PSLIDER(538,326,98,"Vib depth",0,400,0,100)
3390   PGP=4:CALL PSLIDER(541,326,120,"Gain",0,400,100,100)
3392   UI LABEL 552,10,76,620,14,"Per oscillator (Osc pages): FM index = phase modulation by the next oscillator, Mod = not mixed."
3393   CALL REGP(5,552,1)
3394   PGP=5:CALL PSLIDER(545,10,100,"FM decay s",0,1000,0,100)
3395   PGP=5:CALL PSLIDER(548,10,122,"Poly voices",1,8,1,1)
3400   UI LABEL 560,10,170,400,14,"The definition string (Return applies it)"
3410   UI TEXT 561,10,184,620,18,""
3420   CALL UK_SLIDER(570,10,208,304,"Note",24,96,57)
3430   UI LABEL 575,326,210,200,14,""
3440   UI BUTTON 580,10,236,70,22,"Play"
3450   UI BUTTON 581,84,236,70,22,"Chord"
3460   UI BUTTON 582,158,236,70,22,"Stop"
3470   UI BUTTON 583,232,236,70,22,"Save..."
3480   UI BUTTON 584,306,236,70,22,"Open..."
3490   UI BUTTON 585,380,236,70,22,"Export..."
3500   UI BUTTON 586,454,236,70,22,"Copy"
3510   UI BUTTON 587,560,236,70,22,"Quit"
3520   UI LABEL 590,10,266,620,14,""
3530 END PROC
3540 PROC SETSTATUS(T$)
3550   UI CAPTION 590,T$
3560 END PROC
3570 ' shows page N (0..2 an oscillator, 3 filter, 4 env), hides the others
3580 PROC SHOWPAGE(N)
3590   LOCAL P,I,ID,SH
3600   SH=N
3610   IF N<=2 THEN
3620     CUROSC=N
3630     SH=0
3640     CALL OSCTOCONTROLS
3650   END IF
3660   FOR P=0 TO 5
3670     FOR I=1 TO LEN(PGR$(P)) STEP 2
3680       ID=CVI(MID$(PGR$(P),I,2))
3690       UI SHOW ID,(P=SH)
3700     NEXT I
3710   NEXT P
3720   CURPG=N
3730 END PROC
3740 ' ====================================================================== model <-> controls
3750 PROC ENABLES
3760   UI ENABLE 313,(OFX(CUROSC)=0)
3770   UI ENABLE 319,OFX(CUROSC)
3780 END PROC
3790 ' the controls of the oscillator page show the oscillator CUROSC
3800 PROC OSCTOCONTROLS
3810   LOCAL K
3820   K=CUROSC
3830   UI VALUE 300+OW(K),1
3840   UI VALUE 305,OEN(K)
3850   UI VALUE 306,OFX(K)
3860   UI VALUE 307,OPO(K)
3865   UI VALUE 308,OMD(K)
3867   CALL UK_SET(325,OFM(K))
3870   CALL UK_SET(310,OL(K))
3880   CALL UK_SET(313,RT(K))
3890   CALL UK_SET(316,OC(K))
3900   CALL UK_SET(319,FN POSOF(OHZ(K)))
3910   CALL UK_READ(319,STR$(OHZ(K)))
3920   CALL UK_SET(322,OP(K))
3930   CALL ENABLES
3940   UI ENABLE 305,(K>0)
3950   IF K=0 THEN UI VALUE 305,-1
3960 END PROC
3970 PROC TOCONTROLS
3980   CALL OSCTOCONTROLS
3990   CALL UK_SET(500,CUT)
4000   CALL UK_SET(503,RES)
4010   CALL UK_SET(506,FEN)
4020   CALL UK_SET(509,FDC)
4030   CALL UK_SET(512,PIT)
4040   CALL UK_SET(515,PTM)
4050   UI VALUE 520,ENVON
4060   CALL UK_SET(523,EA)
4070   CALL UK_SET(526,ED)
4080   CALL UK_SET(529,ES)
4090   CALL UK_SET(532,ER)
4100   CALL UK_SET(535,VR)
4110   CALL UK_SET(538,VD)
4120   CALL UK_SET(541,GN)
4125   CALL UK_SET(545,FMD)
4127   CALL UK_SET(548,PLY)
4130   UI CAPTION 205,NM$
4140 END PROC
4150 PROC UPDATE(E,V)
4160   LOCAL K,O
4170   IF E>=300 AND E<400 THEN
4180     K=CUROSC
4190     O=E-300
4200     IF O<=4 AND V<>0 THEN OW(K)=O
4210     IF O=5 AND K>0 THEN OEN(K)=FN BOOL(V)
4220     IF O=6 THEN
4230       OFX(K)=FN BOOL(V)
4240       CALL ENABLES
4250     END IF
4260     IF O=7 THEN OPO(K)=FN BOOL(V)
4270     IF O=10 THEN OL(K)=V
4280     IF O=13 THEN RT(K)=V
4290     IF O=16 THEN OC(K)=V
4300     IF O=19 THEN
4310       OHZ(K)=FN HZ(V)
4320       CALL UK_READ(E,STR$(OHZ(K)))
4330     END IF
4340     IF O=22 THEN OP(K)=V
4342     IF O=8 THEN OMD(K)=FN BOOL(V)
4344     IF O=25 THEN OFM(K)=V
4350   END IF
4360   IF E=500 THEN CUT=V
4370   IF E=503 THEN RES=V
4380   IF E=506 THEN FEN=V
4390   IF E=509 THEN FDC=V
4400   IF E=512 THEN PIT=V
4410   IF E=515 THEN PTM=V
4420   IF E=520 THEN ENVON=FN BOOL(V)
4430   IF E=523 THEN EA=V
4440   IF E=526 THEN ED=V
4450   IF E=529 THEN ES=V
4460   IF E=532 THEN ER=V
4470   IF E=535 THEN VR=V
4480   IF E=538 THEN VD=V
4490   IF E=541 THEN GN=V
4495   IF E=545 THEN FMD=V
4497   IF E=548 THEN PLY=V
4500 END PROC
4510 PROC REFRESH
4520   UI CAPTION 561,FN MAKE$()
4530 END PROC
4540 PROC NOTELABEL
4550   LOCAL F
4560   F=FN NOTEF(UI("GET",570))
4570   UI CAPTION 575,STR$(INT(F*10+0.5)/10)+" Hz"
4580 END PROC
4590 ' ====================================================================== presets (the factory instruments 4..19 as definition strings)
4600 PROC LOADPRESET(N)
4610   LOCAL I,P$,D$
4620   RESTORE PRESETS
4630   FOR I=0 TO N
4640     READ P$,D$
4650   NEXT I
4660   PRN=N
4670   CALL PARSE(D$)
4680   CALL TOCONTROLS
4690   CALL REFRESH
4700   UI CAPTION 202,STR$(N+4)+" "+P$
4710 END PROC
4720 ' ====================================================================== sound
4730 PROC DEFINE
4740   INSTRUMENT 31,FN MAKE$()
4750 END PROC
4760 PROC PLAYNOTE
4770   LOCAL F
4780   F=FN NOTEF(UI("GET",570))
4790   CALL DEFINE
4800   SOUND OFF 1
4810   IF UI("GET",217)<>0 THEN
4820     ENVELOPE 1:VIBRATO 1,0,0:TREMOLO 1,0,0
4830     WAVE 1,31
4840     SOUND 1,F,-1,0.5
4850     HELD=-1
4860   ELSE
4870     HELD=0
4880     PLAY "XR ~0,0 ^0,0 I31 V0.5 T240 ML H"+FN NUM$(F)+",1"
4890   END IF
4900   LASTP=TI
4910 END PROC
4920 PROC PLAYCHORD
4930   LOCAL F
4940   F=FN NOTEF(UI("GET",570))
4950   CALL DEFINE
4960   SOUND OFF
4970   HELD=0
4980   PLAY "XR ~0,0 ^0,0 I31 V0.35 T120 ML H"+FN NUM$(F)+",1","XR ~0,0 ^0,0 I31 V0.35 T120 ML H"+FN NUM$(F*FN NOTEF(73)/FN NOTEF(69))+",1","XR ~0,0 ^0,0 I31 V0.35 T120 ML H"+FN NUM$(F*FN NOTEF(76)/FN NOTEF(69))+",1"
4990   LASTP=TI
5000 END PROC
5010 ' ====================================================================== files
5020 PROC SAVEF(F$)
5030   OPEN F$ FOR OUTPUT AS #1
5040   PRINT #1,FN MAKE$()
5050   CLOSE #1
5060   CALL SETSTATUS("Saved "+F$+".")
5070 END PROC
5080 PROC OPENF(F$)
5090   LOCAL L$
5100   OPEN F$ FOR INPUT AS #1
5110   L$=""
5120   IF EOF(1)=0 THEN LINE INPUT #1,L$
5130   CLOSE #1
5140   CALL PARSE(L$)
5150   CALL TOCONTROLS
5160   CALL REFRESH
5170   UI CAPTION 202,"from a file"
5180   CALL SETSTATUS("Opened "+F$+".")
5190 END PROC
5200 PROC EXPORTF(F$)
5210   LOCAL N$
5220   N$=FN UK_INPUT$("Export","The number of the instrument (4..31):","20")
5230   IF UK_OK=0 THEN EXIT PROC
5240   IF VAL(N$)<4 OR VAL(N$)>31 THEN
5250     CALL UK_MSG("Export","The number must be 4..31.")
5260     EXIT PROC
5270   END IF
5280   N$=STR$(INT(VAL(N$)))
5290   OPEN F$ FOR OUTPUT AS #1
5300   PRINT #1,"10 ' instrument made with the instrument editor of Engine1999"
5310   PRINT #1,"20 INSTRUMENT "+N$+","+CHR$(34)+FN MAKE$()+CHR$(34)
5320   CLOSE #1
5330   CALL SETSTATUS("Exported INSTRUMENT "+N$+" to "+F$+" (MERGE or LOAD it).")
5340 END PROC
5350 ' ====================================================================== what happens
5360 LABEL HANDLER
5370 E=UI("EVENT")
5380 WHILE E<>0
5390   K=UI("KIND")
5400   V=UI("VALUE")
5410   CALL REACT(E,K,V)
5420   E=UI("EVENT")
5430 WEND
5440 RETURN
5450 ' a control changed: rebuild the string, and make it heard (while a note is held at once, else with Play on change)
5460 PROC CHANGED
5470   CALL REFRESH
5480   IF HELD THEN
5490     CALL DEFINE
5500   ELSE
5510     IF UI("GET",216)<>0 AND TI>LASTP+8 THEN CALL PLAYNOTE
5520   END IF
5530 END PROC
5540 PROC REACT(E,K,V)
5550   LOCAL F$,T$
5560   IF E=210 AND K=2 THEN CALL SHOWPAGE(V)
5570   IF FN UK_TRACK(E,V) THEN
5580     IF E=570 THEN
5590       CALL NOTELABEL
5600       IF HELD THEN
5610         PITCH 1,FN NOTEF(UI("GET",570))
5620       ELSE
5630         IF UI("GET",216)<>0 AND TI>LASTP+8 THEN CALL PLAYNOTE
5640       END IF
5650     ELSE
5660       CALL UPDATE(E,V)
5670       CALL CHANGED
5680     END IF
5690   ELSE
5700     IF E>=300 AND E<=548 AND K=2 THEN
5710       CALL UPDATE(E,V)
5720       CALL CHANGED
5730     END IF
5740   END IF
5750   IF E=205 AND K=2 THEN
5760     UI GETTEXT 205,T$
5770     NM$=FN CLEANNAME$(T$)
5780     CALL REFRESH
5790   END IF
5800   IF E=217 AND K=2 AND V=0 AND HELD THEN
5810     SOUND OFF
5820     HELD=0
5830   END IF
5840   IF E=200 AND K=1 THEN
5850     CALL LOADPRESET((PRN+15) MOD 16)
5860     CALL CHANGED
5870   END IF
5880   IF E=201 AND K=1 THEN
5890     CALL LOADPRESET((PRN+1) MOD 16)
5900     CALL CHANGED
5910   END IF
5920   IF E=203 AND K=1 THEN
5930     CALL DEFAULTS
5940     CALL TOCONTROLS
5950     CALL REFRESH
5960     UI CAPTION 202,"plain sine"
5970     CALL CHANGED
5980   END IF
5990   IF E=561 AND K=5 THEN
6000     UI GETTEXT 561,F$
6010     CALL PARSE(F$)
6020     CALL TOCONTROLS
6030     CALL REFRESH
6040     UI CAPTION 202,"from the string"
6050     CALL SETSTATUS("The string was applied to the controls.")
6060     CALL CHANGED
6070   END IF
6080   IF E=580 AND K=1 THEN CALL PLAYNOTE
6090   IF E=581 AND K=1 THEN CALL PLAYCHORD
6100   IF E=582 AND K=1 THEN
6110     SOUND OFF
6120     HELD=0
6130   END IF
6140   IF E=583 AND K=1 THEN
6150     F$=FN FR_PICK$("Save the instrument as",DR$,"*.ins;*.txt",-1)
6160     DR$=FR_DIR$
6170     IF F$<>"" THEN CALL SAVEF(F$)
6180   END IF
6190   IF E=584 AND K=1 THEN
6200     F$=FN FR_PICK$("Open an instrument",DR$,"*.ins;*.txt",0)
6210     DR$=FR_DIR$
6220     IF F$<>"" THEN CALL OPENF(F$)
6230   END IF
6240   IF E=585 AND K=1 THEN
6250     F$=FN FR_PICK$("Export as BASIC lines",DR$,"*.bas",-1)
6260     DR$=FR_DIR$
6270     IF F$<>"" THEN CALL EXPORTF(F$)
6280   END IF
6290   IF E=586 AND K=1 THEN
6300     CLIPBOARD FN MAKE$()
6310     CALL SETSTATUS("The definition string is on the clipboard.")
6320   END IF
6330   IF E=587 AND K=1 THEN Q=1
6340 END PROC
6350 ' ====================================================================== the self-test (INSTED_HEADLESS=1 in the console host: tests the string <-> model code, no window)
6360 PROC SELFTEST
6370   LOCAL I,N,P$,D$,S1$,S2$,BAD
6380   BAD=0
6390   N=0
6400   RESTORE PRESETS
6410   FOR I=0 TO 15
6420     READ P$,D$
6430     CALL PARSE(D$)
6440     S1$=FN MAKE$()
6450     CALL PARSE(S1$)
6460     S2$=FN MAKE$()
6470     IF S1$<>S2$ THEN
6480       PRINT "FAIL round trip of ";P$
6490       PRINT S1$
6500       PRINT S2$
6510       BAD=BAD+1
6520     END IF
6530     INSTRUMENT 31,S1$
6540     IF NM$<>P$ THEN
6550       PRINT "FAIL name ";NM$;" <> ";P$
6560       BAD=BAD+1
6570     END IF
6580     N=N+1
6590   NEXT I
6600   ' odd input: lower case, commas, fixed frequency, POST, unknown commands, empty strings
6610   CALL PARSE("name x1; osc noise 0.1,-5000,0,0.5 post; Osc Saw; foo 1; filter 2; gain 1.5;;")
6620   S1$=FN MAKE$()
6630   IF S1$<>"NAME X1; OSC NOISE 0.1 -5000 0 0.5 POST; OSC SAW 1 1 0 0.5; FILTER 2 0 0 0.2; GAIN 1.5" THEN
6640     PRINT "FAIL odd input: ";S1$
6650     BAD=BAD+1
6660   END IF
6670   CALL PARSE("")
6680   S1$=FN MAKE$()
6690   IF S1$<>"NAME USER; OSC SINE 1 1 0 0.5; GAIN 1" THEN
6700     PRINT "FAIL empty string: ";S1$
6710     BAD=BAD+1
6720   END IF
6730   CALL PARSE("OSC 4 0.5 2 -7 0.25; ENV 0.01 0.2 0.5 0.3; VIB 5 0.1; PITCH 12 0.05")
6740   S1$=FN MAKE$()
6750   IF S1$<>"NAME USER; OSC SAW 0.5 2 -7 0.25; PITCH 12 0.05; ENV 0.01 0.2 0.5 0.3; VIB 5 0.1; GAIN 1" THEN
6760     PRINT "FAIL numbers: ";S1$
6770     BAD=BAD+1
6780   END IF
6785   CALL PARSE("OSC SINE 0.5 1 0 0.5 FM 2.5; OSC SINE 1 0.5 0 0.5 MOD; FMDECAY 0.3; POLY 4")
6787   S1$=FN MAKE$()
6788   IF S1$<>"NAME USER; OSC SINE 0.5 1 0 0.5 FM 2.5; OSC SINE 1 0.5 0 0.5 MOD; FMDECAY 0.3; POLY 4; GAIN 1" THEN
6789     PRINT "FAIL FM: ";S1$
6790     BAD=BAD+1
6791   END IF
6792   INSTRUMENT 31,S1$
6800   IF BAD=0 THEN PRINT "insted selftest ok ";N ELSE PRINT "insted selftest FAILED ";BAD
6810 END PROC
6820 ' ====================================================================== the 16 factory instruments (4..19) as definition strings; synthtest1 checks them against synth.c
6830 LABEL PRESETS
6840 DATA "BASS","NAME BASS; OSC SAW 0.8 1 0 0.5; OSC SQUARE 0.6 0.5 0 0.5; FILTER 3 0.3 2 0.15; ENV 0.005 0.2 0.6 0.1; GAIN 0.76"
6850 DATA "LEAD","NAME LEAD; OSC SQUARE 0.6 1 0 0.25; OSC SAW 0.4 1 7 0.5; FILTER 6 0.1 0 0; ENV 0.01 0.1 0.7 0.15; VIB 5.5 0.15; GAIN 0.83"
6860 DATA "PAD","NAME PAD; OSC SAW 0.33 1 -9 0.5; OSC SAW 0.33 1 0 0.5; OSC SAW 0.33 1 9 0.5; FILTER 3 0.1 0 0; ENV 0.5 0.3 0.8 0.8; POLY 4; GAIN 2.5"
6870 DATA "PLUCK","NAME PLUCK; OSC SAW 0.7 1 0 0.5; OSC SQUARE 0.4 1 5 0.5; FILTER 8 0.2 2.5 0.12; ENV 0 0.25 0 0.1; POLY 3; GAIN 2.86"
6880 DATA "ORGAN","NAME ORGAN; OSC SINE 0.5 1 0 0.5; OSC SINE 0.3 2 0 0.5; OSC SINE 0.2 3 0 0.5; ENV 0.01 0 1 0.05; POLY 4; GAIN 0.79"
6890 DATA "STRINGS","NAME STRINGS; OSC SAW 0.33 1 -12 0.5; OSC SAW 0.33 1 0 0.5; OSC SAW 0.33 1 12 0.5; FILTER 4 0.05 0 0; ENV 0.35 0.2 0.85 0.4; VIB 5 0.1; POLY 4; GAIN 2.03"
6900 DATA "BRASS","NAME BRASS; OSC SINE 0.55 1 0 0.5 FM 3; OSC SINE 1 1 0 0.5 MOD; OSC SAW 0.3 1 4 0.5; FILTER 2 0.1 2.5 0.25; ENV 0.06 0.15 0.8 0.1; FMDECAY 0.6; GAIN 1.24"
6910 DATA "FLUTE","NAME FLUTE; OSC SINE 0.9 1 0 0.5; OSC NOISE 0.08 -5000 0 0.5 POST; FILTER 4 0 0 0; ENV 0.08 0.05 0.9 0.15; VIB 5 0.2; GAIN 0.67"
6920 DATA "BELL","NAME BELL; OSC SINE 0.5 1 0 0.5 FM 4; OSC SINE 1 3.5 0 0.5 MOD; OSC SINE 0.2 5.4 0 0.5; ENV 0 1.5 0 1; FMDECAY 0.8; POLY 4; GAIN 1.89"
6930 DATA "KICK","NAME KICK; OSC SINE 1 1 0 0.5; PITCH 24 0.06; ENV 0 0.25 0 0.02; GAIN 0.84"
6940 DATA "SNARE","NAME SNARE; OSC NOISE 0.8 0 0 0.5; OSC TRIANGLE 0.4 1 0 0.5; PITCH 6 0.03; ENV 0 0.18 0 0.02; GAIN 1.22"
6950 DATA "HIHAT","NAME HIHAT; OSC NOISE 1 0 0 0.5; ENV 0 0.05 0 0.01; GAIN 1.09"
6960 DATA "TOM","NAME TOM; OSC SINE 1 1 0 0.5; PITCH 12 0.1; ENV 0 0.35 0 0.02; GAIN 0.84"
6970 DATA "CLAP","NAME CLAP; OSC NOISE 1 0 0 0.5; FILTER 8 0.5 0 0; ENV 0 0.12 0 0.02; GAIN 3.42"
6980 DATA "ZAP","NAME ZAP; OSC SAW 0.7 1 0 0.5; OSC SQUARE 0.4 1 0 0.3; FILTER 6 0.4 1.5 0.1; PITCH 36 0.1; ENV 0 0.2 0 0.05; GAIN 2.44"
6990 DATA "EPIANO","NAME EPIANO; OSC SINE 0.7 1 0 0.5 FM 2; OSC SINE 1 1 0 0.5 MOD; OSC SINE 0.08 14 0 0.5; ENV 0 0.8 0.15 0.3; FMDECAY 0.5; POLY 4; GAIN 2.14"
