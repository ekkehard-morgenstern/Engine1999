1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' UI DEMO - a form made of widgets: a panel, a text field, check boxes, radio buttons, a list, a slider with a progress bar, buttons, a pop-up menu and a modal
20 ' dialog. The UI subsystem (docs/ui-subsystem.md) owns the graphics layer while it is open: UI OPEN, then UI PANEL, UI BUTTON ... create the widgets (each has a
30 ' number, an id, that the program chooses) and UI UPDATE, once a frame, takes the mouse and the keys, draws what changed and queues events. UI("EVENT") is the
40 ' id of the next event (0 = none), UI("KIND") and UI("VALUE") say what happened: kind 1 click, 2 change, 3 select, 4 double click, 5 Return in a text field, 6 cancel.
50 ' Mouse: click, drag the slider, wheel over the list. Keys: Tab / Shift+Tab move the focus, Space and Return press, the arrow keys work in the list, the slider and
60 ' the radio buttons, a text field takes typing. (A game controller works, too: D-pad or left stick move the focus, A presses, B is Escape, LB / RB are Shift+Tab / Tab.)
70 ' ESC is an ordinary key here (BREAK OFF): it cancels a dialog or a pop-up; Ctrl+C stops the program. Scroll bars: UI SCROLL, UI RANGE, UI PAGE; a list shows its own when it has more rows than fit.
80 ' Needs the window (engine1999): the UI draws on the graphics layer.
90 GFX RESET ALL
100 CLS
110 BREAK OFF
120 QT=0
130 CALL MAKEFORM
140 CALL STATUS("Ready: click, type, drag the slider; OK asks whether to quit.")
150 REPEAT
160   GFX VSYNC
170   UI UPDATE
180   E=UI("EVENT")
190   WHILE E<>0
200     CALL HANDLE(E,UI("KIND"),UI("VALUE"))
210     E=UI("EVENT")
220   WEND
230 UNTIL QT=1
240 UI CLOSE
250 GFX RESET
260 CLS
270 PRINT "UI demo finished."
280 END
290 ' ====================================================================== the form
300 PROC MAKEFORM
310   LOCAL I
320   UI OPEN
330   UI PANEL 1,20,10,300,270,"Settings"
340   UI LABEL 2,34,40,50,14,"Name:"
350   UI TEXT 3,90,37,210,18,"Engine1999"
360   UI CHECK 4,34,67,120,16,"Sound on"
370   UI CHECK 5,34,87,120,16,"Full screen"
380   UI VALUE 4,-1
390   UI RADIO 6,34,113,100,16,"Easy",1
400   UI RADIO 7,34,133,100,16,"Normal",1
410   UI RADIO 8,34,153,100,16,"Hard",1
420   UI VALUE 7,1
430   UI LIST 9,170,67,130,90
440   FOR I=1 TO 8
450     UI ITEM 9,"Level "+STR$(I)
460   NEXT I
470   UI VALUE 9,2
480   UI LABEL 10,34,182,50,14,"Volume"
490   UI SLIDER 11,90,178,210,20
500   UI VALUE 11,60
510   UI PROGRESS 12,34,208,266,16
520   UI VALUE 12,60
530   UI BUTTON 16,34,240,100,24,"Menu..."
540   UI BUTTON 13,150,240,70,24,"OK"
550   UI BUTTON 14,230,240,70,24,"Cancel"
560   UI FOCUS 3
570   ' the pop-up menu: a list that is hidden until UI POPUP shows it
580   UI LIST 15,0,0,90,66
590   UI ITEM 15,"New"
600   UI ITEM 15,"Open..."
610   UI ITEM 15,"Save"
620   UI ITEM 15,"Quit"
630   UI SHOW 15,0
640   ' a label on the desk for what happened last
650   UI LABEL 17,340,20,290,30,""
660   UI LABEL 30,340,70,290,14,"Scroll bars (arrows, track, knob, wheel):"
670   UI SCROLL 31,340,95,16,150
680   UI RANGE 31,0,90
690   UI PAGE 31,10
700   UI SCROLL 32,370,95,250,16
710   UI RANGE 32,0,200
720   UI PAGE 32,50
730   UI LABEL 33,370,125,250,14,"Vertical 0   Horizontal 0"
740   UI LIST 34,370,150,250,60
750   FOR I=1 TO 30
760     UI ITEM 34,"A long list: row "+STR$(I)
770   NEXT I
780 END PROC
790 PROC STATUS(T$)
800   UI CAPTION 17,T$
810 END PROC
820 ' ====================================================================== what happens
830 PROC HANDLE(E,K,V)
840   LOCAL T$
850   IF E=13 AND K=1 THEN CALL ASKQUIT
860   IF E=14 AND K=1 THEN CALL ASKQUIT
870   IF E=16 AND K=1 THEN UI POPUP 15,34,172
880   IF E=31 OR E=32 THEN CALL STATUS("Scroll bar "+STR$(E)+" at "+STR$(V))
890   IF E=31 OR E=32 THEN UI CAPTION 33,"Vertical "+STR$(UI("GET",31))+"   Horizontal "+STR$(UI("GET",32))
900   IF E=21 AND K=1 THEN QT=1
910   IF (E=22 AND K=1) OR (E=20 AND K=6) THEN CALL CLOSEASK
920   IF E=11 THEN
930     UI VALUE 12,V
940     CALL STATUS("Volume "+STR$(V))
950   END IF
960   IF E=3 AND K=2 THEN CALL STATUS("The text has "+STR$(V)+" characters")
970   IF E=3 AND K=5 THEN
980     UI GETTEXT 3,T$
990     CALL STATUS("Hello, "+T$+"!")
1000   END IF
1010   IF E=4 THEN
1020     IF V THEN CALL STATUS("Sound is on") ELSE CALL STATUS("Sound is off")
1030   END IF
1040   IF E=5 THEN
1050     IF V THEN CALL STATUS("Full screen is on") ELSE CALL STATUS("Full screen is off")
1060   END IF
1070   IF E>=6 AND E<=8 AND K=2 THEN
1080     IF E=6 THEN T$="Easy"
1090     IF E=7 THEN T$="Normal"
1100     IF E=8 THEN T$="Hard"
1110     CALL STATUS("Difficulty "+T$+" chosen")
1120   END IF
1130   IF E=9 AND (K=3 OR K=4) THEN
1140     UI ITEMTEXT 9,V,T$
1150     IF K=3 THEN CALL STATUS("Selected: "+T$) ELSE CALL STATUS("Double click: "+T$)
1160   END IF
1170   IF E=15 AND K=3 THEN
1180     UI ITEMTEXT 15,V,T$
1190     CALL STATUS("Menu: "+T$)
1200     IF V=3 THEN CALL ASKQUIT
1210   END IF
1220   IF E=15 AND K=6 THEN CALL STATUS("Menu closed")
1230 END PROC
1240 ' a modal dialog: a panel with two buttons; only its widgets react until it is closed
1250 PROC ASKQUIT
1260   UI PANEL 20,170,100,220,100,"Quit?"
1270   UI LABEL 23,190,135,180,14,"Really leave the demo?"
1280   UI BUTTON 21,190,160,70,24,"Yes"
1290   UI BUTTON 22,300,160,70,24,"No"
1300   UI MODAL 20
1310 END PROC
1320 PROC CLOSEASK
1330   UI MODAL 0
1340   UI REMOVE 23
1350   UI REMOVE 22
1360   UI REMOVE 21
1370   UI REMOVE 20
1380   CALL STATUS("Carry on.")
1390 END PROC
