1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' deqq1.bas - the places of demos/deqq.bas (Deqq's House I) as DATA lines for the W3D renderer: engine1999c deqq1.bas [file]  (without a file: standard output)
20 ' Every place is a box of its own (x = west to east 0 .. 600, z = depth 0 .. 400, the camera stands in front of it at z = -100) in a plan of its own: place R (the
30 ' game's room number 1 .. 14) lies at x = (R mod 5) * 800, y = (R \ 5) * 700 of the plan, its shell is the sector R * 40 (tag = R), the furniture are the sectors
40 ' R * 40 + 1 .. (blocks of the shell: floor = the top of the furniture, a negative top is a hole in the floor). Heights are the game's (y 0 .. 170).
50 ' Textures (the game draws them): 100 .. 114 plaster of the hue 0 .. 14 (walls, furniture), 118 floor planks, 119 carpet, 120 floor tiles, 121 stone, 122 grass,
60 ' 123 gravel, 124 ceiling, 130 door (closed), 131 doorway (dark), 132 window, 133 cupboard doors, 134 dark inside.
70 LOAD LIBRARY "mkw3d"
80 DIM MH(16),FT(20),WT(20),HT(20),NB(20)
90 ' the game's 16 hues (0 gray, 1 brown, 2 red, 3 orange, 4 yellow, 5 green, 6 teal, 7 sky, 8 blue, 9 purple, 10 pink, 11 cream, 12 dark purple, 13 slate, 14 lime, 15 mint)
100 ' as the 15 hues of the library w3dkit (0 gray, 1 red, 2 orange, 3 yellow, 4 lime, 5 green, 6 teal, 7 cyan, 8 azure, 9 blue, 10 purple, 11 magenta, 12 brown, 13 sand, 14 pink)
110 FOR I=0 TO 15
120   READ MH(I)
130 NEXT I
140 DATA 0,12,1,2,3,5,6,7,9,10,14,13,10,0,4,6
150 ' the place R: x offset, y offset
160 FUNC OX(R)
170   RETURN (R MOD 5)*800
180 END FUNC
190 FUNC OY(R)
200   RETURN (R\5)*700
210 END FUNC
220 ' a plain plaster texture for a game hue
230 FUNC PT(H)
240   RETURN 100+MH(H)
250 END FUNC
260 ' the shell of place R: floor texture, wall hue, ceiling height, light
270 PROC SHELL(R,FTX,WH,CH,LT)
280   LOCAL L$
290   L$=STR$(FTX)+",-1,"+STR$(LT)
300   CALL MK_ROOM(R*40,FN OX(R),FN OY(R),FN OX(R)+600,FN OY(R)+400,0,CH,L$+","+STR$(R)+","+STR$(FN PT(WH)))
310   CALL MK_ROOM(R*40+39,FN OX(R),FN OY(R)-560,FN OX(R)+600,FN OY(R),0,400,L$+",99,"+STR$(FN PT(WH)))
320   CALL MK_LINKT(R*40+39,"N",FN OX(R),FN OX(R)+600,R*40,FN PT(WH))
330   NB(R)=0
340 END PROC
350 ' the shell of an outdoor place R: the ground texture, 300 high with a sky above, the backdrop (texture 29) all round
360 PROC OSHELL(R,FTX)
370   LOCAL L$
380   L$=STR$(FTX)+",-1,255"
390   HT(R)=300
400   CALL MK_ROOM(R*40,FN OX(R),FN OY(R),FN OX(R)+600,FN OY(R)+400,0,300,L$+","+STR$(R)+",29")
410   CALL MK_ROOM(R*40+39,FN OX(R),FN OY(R)-560,FN OX(R)+600,FN OY(R),0,400,L$+",99,29")
420   CALL MK_LINKT(R*40+39,"N",FN OX(R),FN OX(R)+600,R*40,29)
430   NB(R)=0
440 END PROC
450 ' a block of furniture in place R: x0 .. x1, z0 .. z1, top (the height of its top face), hue
460 PROC BLK(R,X0,X1,Z0,Z1,TOP,HU)
470   LOCAL S
480   X0=MAX(X0,2):X1=MIN(X1,598):Z1=MIN(Z1,398)
490   NB(R)=NB(R)+1
500   S=R*40+NB(R)
510   CALL MK_ROOM(S,FN OX(R)+X0,FN OY(R)+Z0,FN OX(R)+X1,FN OY(R)+Z1,TOP,HT(R),STR$(FN PT(HU))+",-1,"+STR$(WT(R))+",0,"+STR$(FN PT(HU)))
520   CALL MK_INNER(R*40,S,FN PT(HU))
530 END PROC
540 ' the same with the texture given (a game texture number instead of a hue)
550 PROC BLKT(R,X0,X1,Z0,Z1,TOP,TX)
560   LOCAL S
570   X0=MAX(X0,2):X1=MIN(X1,598):Z1=MIN(Z1,398)
580   NB(R)=NB(R)+1
590   S=R*40+NB(R)
600   CALL MK_ROOM(S,FN OX(R)+X0,FN OY(R)+Z0,FN OX(R)+X1,FN OY(R)+Z1,TOP,HT(R),STR$(TX)+",-1,"+STR$(WT(R))+",0,"+STR$(TX))
610   CALL MK_INNER(R*40,S,TX)
620 END PROC
630 ' seven steps along the right wall (they run away from the camera): the stairs of the hall and the cellar
640 PROC STAIRSUP(R,HU)
650   LOCAL K
660   FOR K=0 TO 6
670     CALL BLK(R,500,600,70+K*40,108+K*40,14*(K+1),HU)
680   NEXT K
690 END PROC
700 ' a door in a wall: the back wall (S$="B": x0 .. x1), the left wall (S$="L": z0 .. z1) or the right wall (S$="R")
710 PROC DOOR(R,S$,A0,A1,H)
720   IF S$="B" THEN CALL BLKT(R,A0,A1,394,400,H,130)
730   IF S$="L" THEN CALL BLKT(R,0,6,A0,A1,H,130)
740   IF S$="R" THEN CALL BLKT(R,594,600,A0,A1,H,130)
750 END PROC
760 ' a hole in the floor with steps going down
770 PROC HOLE(R,X0,X1,Z0,Z1)
780   CALL BLKT(R,X0,X1,Z0,Z1,-40,121)
790 END PROC
800 ' the game's heights and lights of the places (the shells are 170 high, the outdoor places 300)
810 FOR R=1 TO 14
820   HT(R)=170:WT(R)=255
830 NEXT R
840 ' -- the indoor places --------------------------------------------------------------------------------------------------------------------------------------------
850 ' 7 hall: wall hue 11, planks
860 CALL SHELL(7,118,11,170,255)
870 CALL STAIRSUP(7,1)
880 CALL BLK(7,180,300,300,396,150,1)
890 CALL BLK(7,30,120,250,330,70,1)
900 CALL DOOR(7,"B",470,560,135)
910 CALL DOOR(7,"L",140,230,135)
920 ' 8 living room: wall hue 6, planks
930 CALL SHELL(8,118,6,170,255)
940 CALL BLK(8,80,250,350,396,120,1)
950 CALL BLK(8,430,580,330,396,165,1)
960 CALL BLK(8,555,600,90,250,80,9)
970 CALL BLK(8,500,553,90,250,38,9)
980 CALL BLK(8,0,45,100,200,110,13)
990 CALL BLK(8,190,330,150,260,60,1)
1000 CALL BLKT(8,180,420,50,130,1,119)
1010 CALL DOOR(8,"L",300,380,135)
1020 ' 9 kitchen: wall hue 4, tiles
1030 CALL SHELL(9,120,4,170,255)
1040 CALL BLK(9,0,60,320,396,140,1)
1050 CALL BLK(9,230,310,330,396,125,13)
1060 CALL BLK(9,330,430,330,396,85,13)
1070 CALL BLK(9,450,590,330,396,85,13)
1080 CALL BLK(9,230,380,170,260,60,1)
1090 CALL BLK(9,0,40,240,310,55,1)
1100 CALL DOOR(9,"R",150,230,135)
1110 CALL DOOR(9,"B",90,170,135)
1120 CALL HOLE(9,40,150,110,230)
1130 ' 10 landing: wall hue 9, planks
1140 CALL SHELL(10,118,9,170,255)
1150 CALL BLK(10,430,560,300,396,50,1)
1160 CALL BLKT(10,300,340,392,400,165,133)
1170 CALL DOOR(10,"L",140,230,135)
1180 CALL DOOR(10,"R",140,230,135)
1190 CALL HOLE(10,60,170,120,260)
1200 ' 11 bedroom: wall hue 10, carpet
1210 CALL SHELL(11,119,10,170,255)
1220 CALL BLK(11,290,570,200,388,52,10)
1230 CALL BLK(11,290,570,390,396,95,1)
1240 CALL BLK(11,40,140,320,396,150,1)
1250 CALL BLK(11,200,265,340,396,55,1)
1260 CALL BLK(11,0,55,90,200,38,9)
1270 CALL DOOR(11,"L",140,230,135)
1280 ' 12 bathroom: wall hue 6, tiles of hue 7
1290 CALL SHELL(12,120,6,170,255)
1300 CALL BLK(12,330,590,300,396,48,0)
1310 CALL BLK(12,330,590,294,298,150,10)
1320 CALL BLK(12,70,170,330,396,72,0)
1330 CALL BLK(12,0,22,260,330,150,13)
1340 CALL DOOR(12,"R",140,230,135)
1350 ' 13 cellar: wall hue 13, stone
1360 CALL SHELL(13,121,13,170,140)
1370 CALL STAIRSUP(13,13)
1380 CALL BLK(13,130,240,330,396,100,0)
1390 CALL BLK(13,380,450,340,396,60,1)
1400 CALL BLK(13,460,530,330,396,65,1)
1410 CALL BLK(13,270,370,375,396,84,1)
1420 CALL BLK(13,594,600,200,260,120,13)
1430 ' 14 attic: wall hue 1, planks
1440 CALL SHELL(14,118,1,170,255)
1450 CALL HOLE(14,440,550,110,230)
1460 CALL BLK(14,50,150,350,396,125,13)
1470 CALL BLK(14,430,560,300,396,50,1)
1480 CALL BLK(14,200,290,320,396,55,1)
1490 CALL BLK(14,170,230,250,300,40,1)
1500 CALL BLK(14,372,388,322,338,50,1)
1510 ' 5 shed: wall hue 1, stone
1520 CALL SHELL(5,121,1,170,255)
1530 CALL BLK(5,430,560,300,390,60,1)
1540 CALL BLK(5,340,420,330,380,85,2)
1550 CALL BLK(5,40,170,370,396,86,1)
1560 CALL DOOR(5,"L",140,240,135)
1570 ' -- the outdoor places (the house is a block 300 high with the picture of a house front, texture 27) --------------------------------------------------------
1580 ' 1 front garden
1590 CALL OSHELL(1,123)
1600 CALL BLKT(1,90,510,300,350,200,27)
1610 CALL BLK(1,500,530,150,190,62,2)
1620 ' 2 west side of the house
1630 CALL OSHELL(2,122)
1640 CALL BLKT(2,90,510,300,350,200,27)
1650 CALL BLK(2,220,380,230,290,5,0)
1660 CALL BLK(2,70,130,170,230,62,1)
1670 CALL BLK(2,440,560,170,230,34,1)
1680 ' 3 back yard
1690 CALL OSHELL(3,122)
1700 CALL BLKT(3,90,510,300,350,200,27)
1710 CALL BLK(3,110,170,180,240,48,3)
1720 CALL BLK(3,380,386,170,176,100,1)
1730 CALL BLK(3,550,556,170,176,100,1)
1740 ' 4 north-east corner: the well, the tree, the shed
1750 CALL OSHELL(4,122)
1760 CALL BLKT(4,90,510,300,350,200,27)
1770 CALL BLK(4,250,330,170,240,38,0)
1780 CALL BLK(4,238,246,195,205,110,1)
1790 CALL BLK(4,334,342,195,205,110,1)
1800 CALL BLK(4,430,590,150,270,100,1)
1810 ' 6 crash site
1820 CALL OSHELL(6,121)
1830 CALL BLK(6,250,350,150,230,12,0)
1840 CALL MK_EMIT(ARG$(1))
