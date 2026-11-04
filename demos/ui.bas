1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' UI DEMO - a form made of widgets: a panel, a text field, check boxes, radio buttons, a list, a slider with a progress bar, buttons, a pop-up menu and a modal
20 ' dialog. The UI subsystem (docs/ui-subsystem.md) owns the graphics layer while it is open: UI OPEN, then UI PANEL, UI BUTTON ... create the widgets (each has a
30 ' number, an id, that the program chooses) and UI UPDATE, once a frame, takes the mouse and the keys, draws what changed and queues events. UI("EVENT") is the
40 ' id of the next event (0 = none), UI("KIND") and UI("VALUE") say what happened: kind 1 click, 2 change, 3 select, 4 double click, 5 Return in a text field, 6 cancel.
50 ' Mouse: click, drag the slider, wheel over the list. Keys: Tab / Shift+Tab move the focus, Space and Return press, the arrow keys work in the list, the slider and
60 ' the radio buttons, a text field takes typing. (A game controller is not used here.)
70 ' Needs the window (engine1999): the UI draws on the graphics layer.
80 GFX RESET ALL
90 CLS
100 QT=0
110 CALL MAKEFORM
120 CALL STATUS("Ready: click, type, drag the slider; OK asks whether to quit.")
130 REPEAT
140   GFX VSYNC
150   UI UPDATE
160   E=UI("EVENT")
170   WHILE E<>0
180     CALL HANDLE(E,UI("KIND"),UI("VALUE"))
190     E=UI("EVENT")
200   WEND
210 UNTIL QT=1
220 UI CLOSE
230 GFX RESET
240 CLS
250 PRINT "UI demo finished."
260 END
270 ' ====================================================================== the form
280 PROC MAKEFORM
290   LOCAL I
300   UI OPEN
310   UI PANEL 1,20,10,300,270,"Settings"
320   UI LABEL 2,34,40,50,14,"Name:"
330   UI TEXT 3,90,37,210,18,"Engine1999"
340   UI CHECK 4,34,67,120,16,"Sound on"
350   UI CHECK 5,34,87,120,16,"Full screen"
360   UI VALUE 4,-1
370   UI RADIO 6,34,113,100,16,"Easy",1
380   UI RADIO 7,34,133,100,16,"Normal",1
390   UI RADIO 8,34,153,100,16,"Hard",1
400   UI VALUE 7,1
410   UI LIST 9,170,67,130,90
420   FOR I=1 TO 8
430     UI ITEM 9,"Level "+STR$(I)
440   NEXT I
450   UI VALUE 9,2
460   UI LABEL 10,34,182,50,14,"Volume"
470   UI SLIDER 11,90,178,210,20
480   UI VALUE 11,60
490   UI PROGRESS 12,34,208,266,16
500   UI VALUE 12,60
510   UI BUTTON 16,34,240,100,24,"Menu..."
520   UI BUTTON 13,150,240,70,24,"OK"
530   UI BUTTON 14,230,240,70,24,"Cancel"
540   UI FOCUS 3
550   ' the pop-up menu: a list that is hidden until UI POPUP shows it
560   UI LIST 15,0,0,90,66
570   UI ITEM 15,"New"
580   UI ITEM 15,"Open..."
590   UI ITEM 15,"Save"
600   UI ITEM 15,"Quit"
610   UI SHOW 15,0
620   ' a label on the desk for what happened last
630   UI LABEL 17,340,20,290,60,""
640 END PROC
650 PROC STATUS(T$)
660   UI CAPTION 17,T$
670 END PROC
680 ' ====================================================================== what happens
690 PROC HANDLE(E,K,V)
700   LOCAL T$
710   IF E=13 AND K=1 THEN CALL ASKQUIT
720   IF E=14 AND K=1 THEN CALL ASKQUIT
730   IF E=16 AND K=1 THEN UI POPUP 15,34,172
740   IF E=21 AND K=1 THEN QT=1
750   IF (E=22 AND K=1) OR (E=20 AND K=6) THEN CALL CLOSEASK
760   IF E=11 THEN
770     UI VALUE 12,V
780     CALL STATUS("Volume "+STR$(V))
790   END IF
800   IF E=3 AND K=2 THEN CALL STATUS("The text has "+STR$(V)+" characters")
810   IF E=3 AND K=5 THEN
820     UI GETTEXT 3,T$
830     CALL STATUS("Hello, "+T$+"!")
840   END IF
850   IF E=4 THEN
860     IF V THEN CALL STATUS("Sound is on") ELSE CALL STATUS("Sound is off")
870   END IF
880   IF E=5 THEN
890     IF V THEN CALL STATUS("Full screen is on") ELSE CALL STATUS("Full screen is off")
900   END IF
910   IF E>=6 AND E<=8 AND K=2 THEN
920     IF E=6 THEN T$="Easy"
930     IF E=7 THEN T$="Normal"
940     IF E=8 THEN T$="Hard"
950     CALL STATUS("Difficulty "+T$+" chosen")
960   END IF
970   IF E=9 AND (K=3 OR K=4) THEN
980     UI ITEMTEXT 9,V,T$
990     IF K=3 THEN CALL STATUS("Selected: "+T$) ELSE CALL STATUS("Double click: "+T$)
1000   END IF
1010   IF E=15 AND K=3 THEN
1020     UI ITEMTEXT 15,V,T$
1030     CALL STATUS("Menu: "+T$)
1040     IF V=3 THEN CALL ASKQUIT
1050   END IF
1060   IF E=15 AND K=6 THEN CALL STATUS("Menu closed")
1070 END PROC
1080 ' a modal dialog: a panel with two buttons; only its widgets react until it is closed
1090 PROC ASKQUIT
1100   UI PANEL 20,170,100,220,100,"Quit?"
1110   UI LABEL 23,190,135,180,14,"Really leave the demo?"
1120   UI BUTTON 21,190,160,70,24,"Yes"
1130   UI BUTTON 22,300,160,70,24,"No"
1140   UI MODAL 20
1150 END PROC
1160 PROC CLOSEASK
1170   UI MODAL 0
1180   UI REMOVE 23
1190   UI REMOVE 22
1200   UI REMOVE 21
1210   UI REMOVE 20
1220   CALL STATUS("Carry on.")
1230 END PROC
