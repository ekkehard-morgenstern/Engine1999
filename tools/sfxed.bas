1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
6 ' A tool of Engine1999: start it with TOOL "sfxed" (or TOOL "sfxed","file" to open a sound effect file) from the prompt or from a program;
7 ' the program you are working on stays as it is. It can also be run as a program of its own (LOAD "tools/sfxed", RUN; ARG$(1) is read the same way).
10 ' SOUND EFFECT EDITOR - builds the string of an SFX effect (a music string with the effect commands W T ML V X ~ ^ S H, see docs/sound.md)
20 ' from sliders and lets you hear it. It is written in BASIC on the UI subsystem and uses the libraries uikit (sliders, dialogs), mml
30 ' (tokenizer) and filereq (file requester). Needs the window (engine1999).
40 '   The buttons on top load a ready-made effect into the controls (Random makes a new one). The sliders are the pitch the effect starts at
50 '   and ends at (a sweep; with Sweep off it is a plain tone of the start pitch), its length, its volume, the envelope (attack, decay, the
60 '   sustain level and the release, when Envelope is on), vibrato and tremolo. Every change rebuilds the string; Play (or Play on change)
70 '   plays it as the effect 64. You may also type or paste a string into the field and press Return: the controls follow it.
80 '   Save keeps the string in a text file, Open reads one, Export writes the BASIC lines  SFX n,"..."  and  SFX n  that you can MERGE or LOAD.
90 ' widget ids: 1 form, 200..208 presets, sliders 210 start, 213 end, 216 length, 219 volume, 222 attack, 225 decay, 228 sustain, 231 release,
100 ' 234 vibrato rate, 237 vibrato depth, 240 tremolo rate, 243 tremolo depth (each +1 label, +2 readout), 250 sweep, 251 envelope, 252..255 wave,
110 ' 260 the string, 270..276 buttons, 272 play on change, 280 status
120 LOAD LIBRARY "uikit"
130 LOAD LIBRARY "mml"
140 LOAD LIBRARY "filereq"
150 REM the graphics of the calling program stay (no GFX RESET)
160 CLS
170 BREAK OFF
180 Q=0
190 DR$=""
200 DIM PV(16)
210 ' PV: the values of the controls in the order of the presets: wave, start Hz, end Hz, length ms, volume %, envelope, attack ms, decay ms,
220 ' sustain %, release ms, vibrato rate, vibrato depth, tremolo rate, tremolo depth %, sweep
230 AUTOP=0
240 LASTP=0
250 ' ====================================================================== functions (they must stand before the lines that use them)
260 ' the pitch of slider position P (0..300) in Hz: 20 Hz .. 20 kHz on a logarithmic scale
270 FUNC HZ(P)
280   RETURN INT(20*EXP(P/300*LOG(1000))+0.5)
290 END FUNC
300 FUNC POSOF(F)
310   LOCAL P
320   IF F<20 THEN RETURN 0
330   P=INT(300*LOG(F/20)/LOG(1000)+0.5)
340   IF P>300 THEN P=300
350   RETURN P
360 END FUNC
370 ' a number with at most three decimals, as text
380 FUNC NUM$(V)
390   RETURN STR$(INT(V*1000+0.5)/1000)
400 END FUNC
410 FUNC WAVEV()
420   LOCAL I,R
430   R=0
440   FOR I=0 TO 3
450     IF UI("GET",252+I)<>0 THEN R=I
460   NEXT I
470   RETURN R
480 END FUNC
490 ' the string of the effect from the controls
500 FUNC MAKE$()
510   LOCAL A$,T,F1,F2
520   T=INT(60000/UI("GET",216)+0.5)
530   IF T<1 THEN T=1
540   IF T>1000 THEN T=1000
550   A$="W"+STR$(FN WAVEV())+" T"+STR$(T)+" ML V"+FN NUM$(FN UK_VAL(219))
560   IF UI("GET",251)<>0 THEN
570     A$=A$+" X"+FN NUM$(FN UK_VAL(222))+","+FN NUM$(FN UK_VAL(225))+","+FN NUM$(FN UK_VAL(228))+","+FN NUM$(FN UK_VAL(231))
580   END IF
590   IF UI("GET",234)>0 AND UI("GET",237)>0 THEN A$=A$+" ~"+FN NUM$(FN UK_VAL(234))+","+FN NUM$(FN UK_VAL(237))
600   IF UI("GET",240)>0 AND UI("GET",243)>0 THEN A$=A$+" ^"+FN NUM$(FN UK_VAL(240))+","+FN NUM$(FN UK_VAL(243))
610   F1=FN HZ(UI("GET",210))
620   F2=FN HZ(UI("GET",213))
630   IF UI("GET",250)<>0 THEN
640     A$=A$+" S"+STR$(F1)+","+STR$(F2)+",4"
650   ELSE
660     A$=A$+" H"+STR$(F1)+",4"
670   END IF
680   RETURN A$
690 END FUNC
700 ' ====================================================================== main program
710 UI OPEN
720 CALL MAKEFORM
730 CALL PRESET(1)
740 CALL SETSTATUS("Pick a preset or move the sliders. Play plays the effect.")
750 ON ERROR GOTO ERRH
751 ARGLOAD$=""
752 IF ARGC>=1 THEN ARGLOAD$=ARG$(1)
753 IF ARGLOAD$<>"" THEN CALL OPENF(ARGLOAD$)
760 LABEL LOOPTOP
770 ON ERROR GOTO ERRH
780 ON UI GOSUB HANDLER
790 REPEAT
800   GFX VSYNC
810   UI UPDATE
820 UNTIL Q=1
830 ON UI OFF
840 ON ERROR OFF
850 SOUND OFF
860 UI CLOSE
870 REM no GFX RESET here
880 CLS
890 PRINT "Sound effect editor finished."
900 END
910 LABEL ERRH
920 ON UI OFF
930 CALL UK_MSG("Error",ERR$+"|(line "+STR$(ERL)+")")
940 RESUME LOOPTOP
950 ' ====================================================================== the form
960 PROC MAKEFORM
970   LOCAL I,N$
980   UI PANEL 1,0,0,640,300,"Sound effect editor"
990   RESTORE PRESETS
1000   FOR I=0 TO 7
1010     READ N$
1020     UI BUTTON 200+I,10+I*70,22,66,22,N$
1030     CALL SKIPPRESET
1040   NEXT I
1050   UI BUTTON 208,10+8*70,22,66,22,"Random"
1060   CALL UK_SLIDER(210,10,52,304,"Start Hz",0,300,150)
1070   CALL UK_SLIDER(213,10,74,304,"End Hz",0,300,150)
1080   CALL UK_DIVISOR(1000)
1090   CALL UK_SLIDER(216,10,96,304,"Length",50,2000,300)
1100   CALL UK_DIVISOR(100)
1110   CALL UK_SLIDER(219,10,118,304,"Volume",0,100,50)
1120   CALL UK_DIVISOR(1000)
1130   CALL UK_SLIDER(222,326,52,304,"Attack",0,1000,0)
1140   CALL UK_DIVISOR(1000)
1150   CALL UK_SLIDER(225,326,74,304,"Decay",0,1000,100)
1160   CALL UK_DIVISOR(100)
1170   CALL UK_SLIDER(228,326,96,304,"Sustain",0,100,50)
1180   CALL UK_DIVISOR(1000)
1190   CALL UK_SLIDER(231,326,118,304,"Release",0,2000,100)
1200   CALL UK_SLIDER(234,10,146,304,"Vib rate",0,30,0)
1210   CALL UK_SLIDER(237,10,168,304,"Vib depth",0,24,0)
1220   CALL UK_SLIDER(240,326,146,304,"Trem rate",0,30,0)
1230   CALL UK_DIVISOR(100)
1240   CALL UK_SLIDER(243,326,168,304,"Trem depth",0,100,0)
1250   UI LABEL 281,10,198,40,14,"Wave"
1260   UI RADIO 252,56,196,60,16,"Sine",1
1270   UI RADIO 253,120,196,70,16,"Square",1
1280   UI RADIO 254,194,196,76,16,"Triangle",1
1290   UI RADIO 255,274,196,60,16,"Noise",1
1300   UI CHECK 250,348,196,70,16,"Sweep"
1310   UI CHECK 251,424,196,90,16,"Envelope"
1320   UI CHECK 272,520,196,110,16,"Play on change"
1330   UI LABEL 282,10,218,400,14,"The effect string (Return applies it)"
1340   UI TEXT 260,10,234,620,18,""
1350   UI BUTTON 270,10,260,70,22,"Play"
1360   UI BUTTON 271,84,260,70,22,"Stop"
1370   UI BUTTON 273,158,260,70,22,"Save..."
1380   UI BUTTON 274,232,260,70,22,"Open..."
1390   UI BUTTON 275,306,260,70,22,"Export..."
1400   UI BUTTON 276,560,260,70,22,"Quit"
1410   UI LABEL 280,10,285,620,14,""
1420 END PROC
1430 PROC SETSTATUS(T$)
1440   UI CAPTION 280,T$
1450 END PROC
1460 ' ====================================================================== presets: name, wave, start Hz, end Hz, length ms, volume %, envelope,
1470 ' attack ms, decay ms, sustain %, release ms, vibrato rate, depth, tremolo rate, depth %, sweep
1480 PROC SKIPPRESET
1490   LOCAL J,X
1500   FOR J=1 TO 15
1510     READ X
1520   NEXT J
1530 END PROC
1540 ' the readouts of the pitch sliders are in Hz
1550 PROC HZREADS
1560   CALL UK_READ(210,STR$(FN HZ(UI("GET",210))))
1570   CALL UK_READ(213,STR$(FN HZ(UI("GET",213))))
1580 END PROC
1590 ' puts values into the controls
1600 PROC SETALL
1610   IF PV(0)<0 OR PV(0)>3 THEN PV(0)=0
1620   UI VALUE 252+PV(0),1
1630   CALL UK_SET(210,FN POSOF(PV(1)))
1640   CALL UK_SET(213,FN POSOF(PV(2)))
1650   CALL UK_SET(216,PV(3))
1660   CALL UK_SET(219,PV(4))
1670   UI VALUE 251,PV(5)
1680   CALL UK_SET(222,PV(6))
1690   CALL UK_SET(225,PV(7))
1700   CALL UK_SET(228,PV(8))
1710   CALL UK_SET(231,PV(9))
1720   CALL UK_SET(234,PV(10))
1730   CALL UK_SET(237,PV(11))
1740   CALL UK_SET(240,PV(12))
1750   CALL UK_SET(243,PV(13))
1760   UI VALUE 250,PV(14)
1770   CALL HZREADS
1780 END PROC
1790 PROC PRESET(N)
1800   LOCAL I,NM$
1810   RESTORE PRESETS
1820   FOR I=1 TO N-1
1830     CALL SKIPPRESET2
1840   NEXT I
1850   READ NM$
1860   FOR I=0 TO 14
1870     READ PV(I)
1880   NEXT I
1890   CALL SETALL
1900   CALL REFRESH
1910   CALL SETSTATUS("Preset "+NM$+".")
1920 END PROC
1930 PROC SKIPPRESET2
1940   LOCAL NM$
1950   READ NM$
1960   CALL SKIPPRESET
1970 END PROC
1980 PROC RANDOMFX
1990   PV(0)=INT(RND*4)
2000   PV(1)=INT(20+RND*RND*3000)
2010   PV(2)=INT(20+RND*RND*3000)
2020   PV(3)=INT(80+RND*700)
2030   PV(4)=INT(40+RND*20)
2040   PV(5)=-INT(RND*2)
2050   PV(6)=INT(RND*100)
2060   PV(7)=INT(RND*400)
2070   PV(8)=INT(10+RND*70)
2080   PV(9)=INT(RND*600)
2090   PV(10)=0
2100   PV(11)=0
2110   IF RND<0.3 THEN
2120     PV(10)=INT(2+RND*10)
2130     PV(11)=INT(1+RND*4)
2140   END IF
2150   PV(12)=0
2160   PV(13)=0
2170   IF RND<0.3 THEN
2180     PV(12)=INT(3+RND*12)
2190     PV(13)=INT(20+RND*60)
2200   END IF
2210   PV(14)=-INT(RND*2)
2220   CALL SETALL
2230   CALL REFRESH
2240   CALL SETSTATUS("A random effect.")
2250 END PROC
2260 ' ====================================================================== the string
2270 PROC REFRESH
2280   UI CAPTION 260,FN MAKE$()
2290   UI ENABLE 213,UI("GET",250)
2300 END PROC
2310 PROC PLAYFX
2320   LOCAL A$
2330   A$=FN MAKE$()
2340   SFX 64,A$
2350   SFX 64
2360   LASTP=TI
2370 END PROC
2380 ' sets the controls from a string (the commands the editor knows; others are skipped)
2390 PROC APPLY(A$)
2400   LOCAL T$,C$,TT,K,N
2410   PV(0)=0
2420   PV(1)=440
2430   PV(2)=440
2440   PV(3)=300
2450   PV(4)=50
2460   PV(5)=0
2470   PV(6)=0
2480   PV(7)=100
2490   PV(8)=50
2500   PV(9)=100
2510   PV(10)=0
2520   PV(11)=0
2530   PV(12)=0
2540   PV(13)=0
2550   PV(14)=0
2560   TT=120
2570   CALL MK_SET(A$)
2580   T$=FN MK_NEXT$()
2590   WHILE T$<>""
2600     C$=LEFT$(T$,1)
2610     IF C$="W" THEN PV(0)=FN MK_ARG(T$,0)
2620     IF C$="T" THEN TT=FN MK_ARG(T$,0)
2630     IF C$="V" THEN PV(4)=INT(FN MK_ARG(T$,0)*100+0.5)
2640     IF T$<>"XR" AND C$="X" THEN
2650       PV(5)=-1
2660       PV(6)=INT(FN MK_ARG(T$,0)*1000+0.5)
2670       PV(7)=INT(FN MK_ARG(T$,1)*1000+0.5)
2680       PV(8)=INT(FN MK_ARG(T$,2)*100+0.5)
2690       PV(9)=INT(FN MK_ARG(T$,3)*1000+0.5)
2700     END IF
2710     IF C$="~" THEN
2720       PV(10)=INT(FN MK_ARG(T$,0)+0.5)
2730       PV(11)=INT(FN MK_ARG(T$,1)+0.5)
2740     END IF
2750     IF C$="^" THEN
2760       PV(12)=INT(FN MK_ARG(T$,0)+0.5)
2770       PV(13)=INT(FN MK_ARG(T$,1)*100+0.5)
2780     END IF
2790     IF C$="S" OR C$="H" THEN
2800       PV(1)=FN MK_ARG(T$,0)
2810       K=1
2820       PV(14)=0
2830       IF C$="S" THEN
2840         PV(2)=FN MK_ARG(T$,1)
2850         K=2
2860         PV(14)=-1
2870       ELSE
2880         PV(2)=PV(1)
2890       END IF
2900       N=FN MK_ARG(T$,K)
2910       IF N<1 THEN N=4
2920       PV(3)=INT(4/N*60000/TT+0.5)
2930     END IF
2940     T$=FN MK_NEXT$()
2950   WEND
2960   CALL SETALL
2970   CALL REFRESH
2980 END PROC
2990 ' ====================================================================== files
3000 PROC SAVEF(F$)
3010   OPEN F$ FOR OUTPUT AS #1
3020   PRINT #1,FN MAKE$()
3030   CLOSE #1
3040   CALL SETSTATUS("Saved "+F$+".")
3050 END PROC
3060 PROC OPENF(F$)
3070   LOCAL L$
3080   OPEN F$ FOR INPUT AS #1
3090   L$=""
3100   IF EOF(1)=0 THEN LINE INPUT #1,L$
3110   CLOSE #1
3120   CALL APPLY(L$)
3130   CALL SETSTATUS("Opened "+F$+".")
3140 END PROC
3150 PROC EXPORTF(F$)
3160   LOCAL N$
3170   N$=FN UK_INPUT$("Export","The number of the effect (1..64):","1")
3180   IF UK_OK=0 THEN EXIT PROC
3190   IF VAL(N$)<1 OR VAL(N$)>64 THEN
3200     CALL UK_MSG("Export","The number must be 1..64.")
3210     EXIT PROC
3220   END IF
3230   N$=STR$(INT(VAL(N$)))
3240   OPEN F$ FOR OUTPUT AS #1
3250   PRINT #1,"10 ' sound effect made with the sound effect editor of Engine1999"
3260   PRINT #1,"20 SFX "+N$+","+CHR$(34)+FN MAKE$()+CHR$(34)
3270   PRINT #1,"30 SFX "+N$
3280   CLOSE #1
3290   CALL SETSTATUS("Exported SFX "+N$+" to "+F$+" (MERGE or LOAD it).")
3300 END PROC
3310 ' ====================================================================== what happens
3320 LABEL HANDLER
3330 E=UI("EVENT")
3340 WHILE E<>0
3350   K=UI("KIND")
3360   V=UI("VALUE")
3370   CALL REACT(E,K,V)
3380   E=UI("EVENT")
3390 WEND
3400 RETURN
3410 PROC REACT(E,K,V)
3420   LOCAL F$
3430   IF FN UK_TRACK(E,V) THEN
3440     IF E=210 OR E=213 THEN CALL HZREADS
3450   END IF
3460   IF E>=210 AND E<=255 AND K=2 THEN
3470     CALL REFRESH
3480     IF UI("GET",272)<>0 AND TI>LASTP+8 THEN CALL PLAYFX
3490   END IF
3500   IF E>=200 AND E<=207 AND K=1 THEN
3510     CALL PRESET(E-199)
3520     CALL PLAYFX
3530   END IF
3540   IF E=208 AND K=1 THEN
3550     CALL RANDOMFX
3560     CALL PLAYFX
3570   END IF
3580   IF E=260 AND K=5 THEN
3590     UI GETTEXT 260,F$
3600     CALL APPLY(F$)
3610     CALL SETSTATUS("The string was applied to the controls.")
3620   END IF
3630   IF E=270 AND K=1 THEN CALL PLAYFX
3640   IF E=271 AND K=1 THEN SOUND OFF
3650   IF E=273 AND K=1 THEN
3660     F$=FN FR_PICK$("Save the effect as",DR$,"*.sfx;*.txt",-1)
3670     DR$=FR_DIR$
3680     IF F$<>"" THEN CALL SAVEF(F$)
3690   END IF
3700   IF E=274 AND K=1 THEN
3710     F$=FN FR_PICK$("Open an effect",DR$,"*.sfx;*.txt",0)
3720     DR$=FR_DIR$
3730     IF F$<>"" THEN CALL OPENF(F$)
3740   END IF
3750   IF E=275 AND K=1 THEN
3760     F$=FN FR_PICK$("Export as BASIC lines",DR$,"*.bas",-1)
3770     DR$=FR_DIR$
3780     IF F$<>"" THEN CALL EXPORTF(F$)
3790   END IF
3800   IF E=276 AND K=1 THEN Q=1
3810 END PROC
3820 LABEL PRESETS
3830 DATA "Laser",1,1800,200,250,50,0,0,0,0,0,0,0,0,0,-1
3840 DATA "Blast",3,900,60,900,60,-1,0,500,10,600,0,0,0,0,-1
3850 DATA "Coin",1,988,1319,200,40,-1,0,60,30,100,0,0,0,0,-1
3860 DATA "Jump",2,200,640,220,50,-1,0,120,0,80,0,0,0,0,-1
3870 DATA "Power-up",1,300,1500,450,45,-1,0,150,50,200,7,3,0,0,-1
3880 DATA "Hit",3,400,80,150,60,-1,0,80,0,60,0,0,0,0,-1
3890 DATA "Siren",0,500,900,1000,50,0,0,0,0,0,6,3,0,0,-1
3900 DATA "Beep",1,880,880,150,40,-1,10,40,70,60,0,0,0,0,0
