1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' ON UI DEMO - the main loop only calls UI UPDATE once a frame and does its own work (a bar that sweeps back and forth, a frame counter);
20 ' every event of the widgets runs the handler at the label HANDLER, which was set with ON UI GOSUB. The handler reads the events with
30 ' UI("EVENT") until it returns 0 and says what happened in the labels. Things to try: the check box stops and starts the bar (the main
40 ' loop is not blocked by anything), the slider changes its speed, the list and the text field report their events, Beep plays a note,
50 ' Quit ends the program. Look at "Frames": it keeps counting while you click, type and drag. ESC (the break key) stops the program, too.
60 ' Needs the window (engine1999): the UI draws on the graphics layer. Docs: docs/ui-subsystem.md, "ON UI".
70 GFX RESET ALL
80 CLS
90 F=0
100 EV=0
110 Q=0
120 P=0
130 D=1
140 RB=-1
150 SP=2
160 UI OPEN
170 UI PANEL 1,100,20,440,260,"ON UI demo"
180 UI CHECK 2,120,50,200,16,"Run the bar"
190 UI VALUE 2,1
200 UI LABEL 13,120,76,60,14,"Speed"
210 UI SLIDER 3,180,72,340,20
220 UI VALUE 3,20
230 UI PROGRESS 4,120,102,400,16
240 UI LIST 5,120,128,190,70
250 UI ITEM 5,"Apple"
260 UI ITEM 5,"Banana"
270 UI ITEM 5,"Cherry"
280 UI ITEM 5,"Damson"
290 UI ITEM 5,"Elderberry"
300 UI ITEM 5,"Fig"
310 UI TEXT 6,330,128,190,18,"type here"
320 UI BUTTON 7,330,160,90,24,"Beep"
330 UI BUTTON 8,430,160,90,24,"Quit"
340 UI LABEL 10,120,208,400,14,"Last event: none yet"
350 UI LABEL 11,120,228,400,14,"Frames: 0"
360 UI LABEL 12,120,246,400,14,"Events handled by ON UI: 0"
370 ON UI GOSUB HANDLER
380 REPEAT
390   GFX VSYNC
400   UI UPDATE
410   F=F+1
420   IF RB THEN
430     P=P+D*SP
440     IF P>=100 THEN
450       P=100
460       D=-1
470     END IF
480     IF P<=0 THEN
490       P=0
500       D=1
510     END IF
520     UI VALUE 4,P
530   END IF
540   IF F MOD 6=0 THEN UI CAPTION 11,"Frames: "+STR$(F)
550 UNTIL Q=1
560 ON UI OFF
570 UI CLOSE
580 GFX RESET
590 CLS
600 PRINT "ON UI demo finished: ";F;" frames, ";EV;" events handled."
610 END
620 LABEL HANDLER
630 E=UI("EVENT")
640 WHILE E<>0
650   K=UI("KIND")
660   V=UI("VALUE")
670   EV=EV+1
680   T$="widget "+STR$(E)+", kind "+STR$(K)+", value "+STR$(V)
690   IF E=2 THEN
700     RB=V
710     IF V THEN T$=T$+"  (bar runs)" ELSE T$=T$+"  (bar stopped)"
720   END IF
730   IF E=3 THEN SP=1+V\10
740   IF E=5 AND (K=3 OR K=4) THEN
750     UI ITEMTEXT 5,V,A$
760     T$="list: "+A$
770   END IF
780   IF E=6 AND K=5 THEN
790     UI GETTEXT 6,A$
800     T$="Return in the text field: "+A$
810   END IF
820   IF E=7 AND K=1 THEN PLAY "T240 O5 C E G"
830   IF E=8 AND K=1 THEN Q=1
840   UI CAPTION 10,"Last event: "+T$
850   UI CAPTION 12,"Events handled by ON UI: "+STR$(EV)
860   E=UI("EVENT")
870 WEND
880 RETURN
