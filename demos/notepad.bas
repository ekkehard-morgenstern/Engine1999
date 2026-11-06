1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' NOTEPAD - a small text editor, written in BASIC on the UI subsystem: it shows the multi-line editor widget (UI EDIT) that gives the user
20 ' selection with Shift + the cursor keys or the mouse (double click = word, triple click = line), Ctrl+A / C / X / V (select all, copy, cut, paste:
30 ' the system clipboard), Ctrl+Z / Y (undo, redo), Ctrl+Home / End and a scroll bar. New, Open and Save work on text files (at most 60000 bytes).
40 ' Ctrl+Insert / Shift+Insert / Shift+Delete copy, paste and cut as well; Tab moves to the buttons. Needs the window (engine1999).
45 ' Syntax switches the colors of BASIC source text on and off (the same colors as LIST), Theme cycles the color themes (UI THEME): classic, dark, high contrast.
50 ' widget ids: 1 the panel, 10 the editor, 20..26 the buttons, 30 the status line
60 LOAD LIBRARY "uikit"
70 LOAD LIBRARY "filereq"
80 GFX RESET ALL
90 CLS
100 BREAK OFF
110 Q=0
115 SY=0
116 TH=0
120 F$=""
130 DR$=""
140 DIRTY=0
160 UI OPEN
170 UI PANEL 1,0,0,640,300,"Notepad"
180 UI EDIT 10,6,24,628,218,""
185 UI BUTTON 25,6,248,130,20,"Syntax: off"
186 UI BUTTON 26,142,248,150,20,"Theme: classic"
190 UI BUTTON 20,6,272,60,22,"New"
200 UI BUTTON 21,72,272,60,22,"Open"
210 UI BUTTON 22,138,272,60,22,"Save"
220 UI BUTTON 23,204,272,80,22,"Save as"
230 UI BUTTON 24,290,272,60,22,"Quit"
240 UI LABEL 30,362,277,274,14,""
250 UI FOCUS 10
260 CALL STATUS
270 ON UI GOSUB 400
280 N=0
290 WHILE Q=0
300   GFX VSYNC
310   UI UPDATE
320   N=N+1
330   IF N MOD 6=0 THEN CALL STATUS
340 WEND
350 ON UI OFF
360 UI CLOSE
370 GFX RESET ALL
380 CLS
390 END
400 ' ---------------------------------------------------------------------- the events
410 E=UI("EVENT")
420 WHILE E<>0
430   K=UI("KIND")
440   IF E=10 AND K=2 THEN DIRTY=-1
450   IF K=1 THEN CALL BUTTON(E)
460   E=UI("EVENT")
470 WEND
480 RETURN
490 PROC STATUS
500   LOCAL T$
510   T$=F$
520   IF T$="" THEN T$="(no name)"
530   T$=RIGHT$(T$,30)+"   Line "+STR$(UI("LINE",10)+1)+"  Col "+STR$(UI("COLUMN",10)+1)
540   IF DIRTY THEN T$=T$+"  *"
550   UI CAPTION 30,T$
560 END PROC
570 PROC BUTTON(B)
580   IF B=20 THEN CALL NEWFILE
590   IF B=21 THEN CALL OPENFILE
600   IF B=22 THEN CALL SAVEFILE(0)
610   IF B=23 THEN CALL SAVEFILE(-1)
620   IF B=24 THEN CALL QUITIT
  IF B=25 THEN CALL SYNTAXMODE
  IF B=26 THEN CALL THEMEMODE
630 END PROC
640 FUNC SURE(TTL$)
650   IF DIRTY=0 THEN RETURN -1
660   RETURN FN UK_ASK(TTL$,"The text has changes.|Discard them?")
670 END FUNC
680 PROC NEWFILE
690   IF FN SURE("New")=0 THEN EXIT PROC
700   UI CAPTION 10,""
710   F$=""
720   DIRTY=0
730 END PROC
740 PROC OPENFILE
750   LOCAL N$,A$
760   IF FN SURE("Open")=0 THEN EXIT PROC
770   N$=FN FR_PICK$("Open a text file",DR$,"",0)
780   DR$=FR_DIR$
790   IF N$="" THEN EXIT PROC
800   OPEN N$ FOR INPUT AS #1
810   IF LOF(1)>60000 THEN CLOSE #1:CALL UK_MSG("Open","The file is longer than 60000 bytes."):EXIT PROC
820   A$=INPUT$(LOF(1),#1)
830   CLOSE #1
840   UI CAPTION 10,A$
850   F$=N$
860   DIRTY=0
870 END PROC
880 PROC SAVEFILE(AS)
890   LOCAL N$,T$
891   N$=F$
892   IF AS OR N$="" THEN N$=FN FR_PICK$("Save the text as",DR$,"",-1):DR$=FR_DIR$
893   IF N$="" THEN EXIT PROC
894   UI GETTEXT 10,T$
895   OPEN N$ FOR OUTPUT AS #1
896   PRINT #1,T$;
897   CLOSE #1
898   F$=N$
899   DIRTY=0
900 END PROC
910 PROC QUITIT
920   IF FN SURE("Quit")<>0 THEN Q=-1
930 END PROC
940 PROC SYNTAXMODE
950   SY=1-SY
960   UI SYNTAX 10,SY
970   IF SY THEN UI CAPTION 25,"Syntax: BASIC" ELSE UI CAPTION 25,"Syntax: off"
980 END PROC
990 PROC THEMEMODE
1000  TH=(TH+1) MOD 3
1010  UI THEME TH
1020  IF TH=0 THEN UI CAPTION 26,"Theme: classic"
1021  IF TH=1 THEN UI CAPTION 26,"Theme: dark"
1022  IF TH=2 THEN UI CAPTION 26,"Theme: contrast"
1030 END PROC
