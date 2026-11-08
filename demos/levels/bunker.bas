1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' bunker.bas - the level of demos/bughunt.bas (Bug Hunt) as DATA lines: engine1999c bunker.bas [file]  (without a file: standard output)
20 LOAD LIBRARY "mkw3d"
30 ' textures: 1 metal gray, 2 metal azure, 3 brick gray, 4 floor tiles, 5 carpet blue, 6 hazard metal (red), 7 stone, 8 red door, 9 blue door, 12 lift (yellow metal), 13 exit stone
40 CALL MK_ROOM(0,0,0,256,256,0,128, "4,1,190,1,1") ' start room
50 CALL MK_ROOM(1,96,256,160,512,0,112, "4,1,130,2,2") ' corridor
60 CALL MK_ROOM(2,-96,512,352,832,0,128, "4,1,210,3,3") ' arena
70 CALL MK_ROOM(3,-352,600,-96,760,0,128, "5,1,170,4,2") ' west room (red key)
80 CALL MK_ROOM(4,352,640,376,700,0,0, "4,1,150,21,8") ' red door
90 CALL MK_ROOM(5,376,560,632,780,0,128, "4,1,190,5,6") ' east room
100 CALL MK_ROOM(6,96,832,160,928,0,112, "4,1,130,2,2") ' corridor north
110 CALL MK_ROOM(7,-64,928,320,1152,0,128, "4,1,160,6,3") ' hall 2
120 CALL MK_ROOM(8,96,1152,160,1216,0,200, "4,1,200,30,12") ' lift (floor moves 0 .. 96)
130 CALL MK_ROOM(9,-100,1216,360,1400,96,224, "5,1,200,7,2") ' upper room (blue key)
140 CALL MK_ROOM(10,130,1400,190,1424,96,96, "4,1,160,22,9") ' blue door
150 CALL MK_ROOM(11,60,1424,260,1560,96,224, "4,13,255,99,13") ' exit room
160 CALL MK_LINK(0,"N",96,160,1)
170 CALL MK_LINK(1,"N",96,160,2)
180 CALL MK_LINK(2,"W",640,700,3)
190 CALL MK_LINKT(2,"E",640,700,4,8)
200 CALL MK_LINKT(4,"E",640,700,5,8)
210 CALL MK_LINK(2,"N",96,160,6)
220 CALL MK_LINK(6,"N",96,160,7)
230 CALL MK_LINKT(7,"N",96,160,8,12)
240 CALL MK_LINKT(8,"N",96,160,9,12)
250 CALL MK_LINKT(9,"N",130,190,10,9)
260 CALL MK_LINKT(10,"N",130,190,11,9)
270 FOR I=1 TO 4
280   READ X,Y
290   CALL MK_PILLAR(2, X,Y,X+30,Y+30, 7)
300 NEXT I
310 CALL MK_PILLAR(5, 480,640,510,670, 6):CALL MK_PILLAR(5, 480,690,510,720, 6)
320 CALL MK_PILLAR(7, 40,1000,70,1030, 3):CALL MK_PILLAR(7, 190,1000,220,1030, 3)
330 CALL MK_PILLAR(9, 30,1280,60,1310, 7):CALL MK_PILLAR(9, 250,1280,280,1310, 7)
340 CALL MK_EMIT(ARG$(1))
350 END
360 DATA -10,590,220,590,-10,760,220,760
