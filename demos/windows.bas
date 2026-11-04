1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' WINDOWS DEMO - overlapping, draggable windows of the UI subsystem (UI WINDOW). Drag a window by its title bar, click one to bring it to the front, click the
20 ' small box at the right of the title bar to close it; "New window" opens another one (up to 8). The widgets of a window (UI LABEL, BUTTON, CHECK ...
30 ' created inside its rectangle) belong to it and move and rise with it. The title bar of the window in front is blue, the others are gray.
40 ' The program is driven by ON UI: the handler gets the events (a close box is the event kind 6, cancel, of the window's id). Remove a window's widgets first, then the
50 ' window. A game controller works, too (LB / RB change the focus). Needs the window (engine1999). ESC stops the program.
60 GFX RESET ALL
70 CLS
80 DIM CL(12)
90 NW=0
100 Q=0
110 UI OPEN
120 UI WINDOW 100,380,20,240,90,"Control"
130 UI BUTTON 101,396,52,100,24,"New window"
140 UI BUTTON 102,506,52,100,24,"Quit"
150 CALL NEWWIN
160 CALL NEWWIN
170 ON UI GOSUB HANDLER
180 REPEAT
190   GFX VSYNC
200   UI UPDATE
210 UNTIL Q=1
220 ON UI OFF
230 UI CLOSE
240 GFX RESET
250 CLS
260 PRINT "Windows demo finished."
270 END
280 ' a window with a label, a button and a check box; its id is a multiple of 100 (200, 300 ...), the widgets are the id + 1 .. + 3
290 PROC NEWWIN
300   LOCAL W,X,Y
310   IF NW>=8 THEN EXIT PROC
320   NW=NW+1
330   W=100+NW*100
340   X=20+((NW-1)*47) MOD 200
350   Y=20+((NW-1)*61) MOD 150
360   CL(NW)=0
370   UI WINDOW W,X,Y,210,110,"Window "+STR$(NW)
380   UI LABEL W+1,X+14,Y+30,180,14,"Not clicked yet"
390   UI BUTTON W+2,X+14,Y+52,90,24,"Click me"
400   UI CHECK W+3,X+14,Y+82,150,16,"A check box"
410 END PROC
420 PROC KILLWIN(W)
430   UI REMOVE W+3
440   UI REMOVE W+2
450   UI REMOVE W+1
460   UI REMOVE W
470 END PROC
480 LABEL HANDLER
490 E=UI("EVENT")
500 WHILE E<>0
510   K=UI("KIND")
520   IF E=101 AND K=1 THEN CALL NEWWIN
530   IF E=102 AND K=1 THEN Q=1
540   IF E=100 AND K=6 THEN Q=1
550   IF E>=200 AND E MOD 100=0 AND K=6 THEN CALL KILLWIN(E)
560   IF E>=200 AND E MOD 100=2 AND K=1 THEN
570     N=E\100-1
580     CL(N)=CL(N)+1
590     UI CAPTION E-1,"Clicked "+STR$(CL(N))+" times"
600   END IF
610   E=UI("EVENT")
620 WEND
630 RETURN
