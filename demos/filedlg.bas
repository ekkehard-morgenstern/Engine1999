1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' FILE REQUESTER DEMO - two buttons open the file requester of the library filereq (LOAD LIBRARY "filereq"): "Open..." chooses an existing
20 ' file (the pattern is *.bas;*.lib), "Save as..." may name a new one and asks before it replaces a file. The requester is a modal
30 ' dialog on the open UI (docs/ui-subsystem.md, "The file requester"); the label shows what it returned. It remembers the directory
40 ' (FR_DIR$) between the two calls. Needs the window (engine1999). ESC cancels the dialog; Ctrl+C stops the program.
50 LOAD LIBRARY "filereq"
60 GFX RESET ALL
70 CLS
80 BREAK OFF
90 D$=""
100 UI OPEN
110 UI PANEL 1,120,60,400,170,"File requester demo"
120 UI BUTTON 2,140,100,110,24,"Open..."
130 UI BUTTON 3,260,100,110,24,"Save as..."
140 UI BUTTON 4,420,100,80,24,"Quit"
150 UI LABEL 5,140,150,360,14,"Nothing chosen yet."
160 UI LABEL 6,140,170,360,14,""
170 Q=0
180 REPEAT
190   GFX VSYNC
200   UI UPDATE
210   E=UI("EVENT")
220   WHILE E<>0
230     IF UI("KIND")=1 THEN
240       IF E=2 THEN F$=FN FR_PICK$("Open a program",D$,"*.bas;*.lib",0):D$=FR_DIR$:CALL SHOWIT(F$,"Open")
250       IF E=3 THEN F$=FN FR_PICK$("Save the program as",D$,"*.bas",-1):D$=FR_DIR$:CALL SHOWIT(F$,"Save")
260       IF E=4 THEN Q=1
270     END IF
280     E=UI("EVENT")
290   WEND
300 UNTIL Q=1
310 UI CLOSE
320 GFX RESET
330 CLS
340 PRINT "File requester demo finished."
350 END
360 PROC SHOWIT(F$,W$)
370   LOCAL I,P
380   IF F$="" THEN
390     UI CAPTION 5,W$+": cancelled."
400     UI CAPTION 6,""
410   ELSE
420     P=0
430     FOR I=1 TO LEN(F$)
440       IF MID$(F$,I,1)="/" THEN P=I
450     NEXT I
460     UI CAPTION 5,W$+": "+MID$(F$,P+1)
470     UI CAPTION 6,"in "+LEFT$(F$,P)
480   END IF
490 END PROC
