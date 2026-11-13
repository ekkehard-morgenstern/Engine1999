1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
50 ' deqq2.bas - the house, garden and cellar of demos/deqq2.bas (Deqq's House II) as DATA lines: engine1999c deqq2.bas [file]  (without a file: standard output)
60 LOAD LIBRARY "mkw3d"
70 PROC WIN(ID,S$,LO,HI,Z1,Z2,G,RR)
80   LOCAL X1,Y1,X2,Y2,R,RT
90   R=RR\100:RT=RR MOD 100
100   IF S$="S" THEN X1=LO:X2=HI:Y1=300:Y2=330
110   IF S$="N" THEN X1=LO:X2=HI:Y1=680:Y2=730
120   IF S$="W" THEN X1=180:X2=200:Y1=LO:Y2=HI
130   IF S$="E" THEN X1=800:X2=840:Y1=LO:Y2=HI
140   CALL MK_ROOM(ID,X1,Y1,X2,Y2,Z1,Z2, "9,3,200,0,1")
150   IF S$="S" THEN
160     CALL MK_LINKX(G,"N",LO,HI,ID, "200,27,27,18,0,0")
170     CALL MK_LINKX(ID,"N",LO,HI,R, "200,"+STR$(RT)+","+STR$(RT)+",18,0,0")
180   END IF
190   IF S$="N" THEN
200     CALL MK_LINKX(R,"N",LO,HI,ID, "200,"+STR$(RT)+","+STR$(RT)+",18,0,0")
210     CALL MK_LINKX(ID,"N",LO,HI,G, "200,27,27,18,0,0")
220   END IF
230   IF S$="W" THEN
240     CALL MK_LINKX(G,"E",LO,HI,ID, "200,27,27,18,0,0")
250     CALL MK_LINKX(ID,"E",LO,HI,R, "200,"+STR$(RT)+","+STR$(RT)+",18,0,0")
260   END IF
270   IF S$="E" THEN
280     CALL MK_LINKX(R,"E",LO,HI,ID, "200,"+STR$(RT)+","+STR$(RT)+",18,0,0")
290     CALL MK_LINKX(ID,"E",LO,HI,G, "200,27,27,18,0,0")
300   END IF
310 END PROC
320 ' THE HOUSE (since item 89 a real two-storey house with an attic: the rooms upstairs lie OVER the rooms downstairs): x 180 .. 840, y 300 .. 730 (front y 300, back y 730); storeys: ground 0 .. 112, upper 112 .. 224, attic 224 .. 300
330 ' ground: kitchen 10 (200..400, 330..680), hall = a ring round the stairs (11 front, 65 west arm, 66 east arm, 67 back), living room 12 (560..800, 330..680); stairs 13..20 (x 450..510, y 400..592, rising north, 8 steps of 24)
340 ' upper: bathroom 23 (over the kitchen), landing 21 (over the back of the hall), bedroom 22 (over the living room) + 48 (its lobby by the landing), ladder 24..31 (x 608..800, y 620..680, rising east), attic 32 (200..800, 330..620) + 47 (200..680, 620..680)
350 ' windows (niche sectors 80..95, glass = texture 200): ground z 40..100, upper z 152..212, attic z 238..298; the same window is seen from outside and from inside
360 ' textures: 29 the backdrop behind the fences (128 x 300: tree line and night sky above a brick fence), 27 the plain front of the house (128 x 300: roof band, belt courses, bricks), 28 a door in it (64 x 300), 1 gray brick, 2 plaster purple (hall), 3 plaster teal (kitchen), 4 plaster magenta (living room), 5 plaster blue (bedroom), 6 bath tiles,
370 ' 7 plaster sand (landing), 8 stone (cellar), 9 planks, 10 grass, 11 sky, 12 brown door, 13 hall floor, 14 kitchen floor, 15 red carpet, 16 blue carpet,
380 ' 17 bath floor, 18 stair planks, 19 mossy stone, 20 gray door, 23 dark alcove, 24 sand stone (front garden), 25 attic wall (planks darker), 26 cupboard wood, 200 glass, 205 water
390 ' tags: 1 front door, 2 back door, 3 shed door, 4 hatch, 50 a hiding place, 40 the chair platform, 99 nothing; rooms: 10 front garden, 11 west garden, 12 back yard,
400 ' 13 crash site, 14 east garden, 15 north-east corner, 16 shed, 20 kitchen, 21 hall, 22 living room, 23 landing, 24 bedroom, 25 bathroom, 26 attic, 27 cellar
410 ' outdoors: floor grass (10), sky ceiling, fence = gray brick (1); 300 high: the house front stands on the north side of the front garden
420 CALL MK_ROOM(0,-380,0,1240,300,0,300, "24,-1,255,10,1")
430 CALL MK_ROOM(1,-380,300,180,700,0,300, "10,-1,255,11,1")
440 CALL MK_ROOM(2,-380,700,180,1250,0,300, "10,-1,255,12,1")
450 CALL MK_ROOM(3,-380,1250,-100,1500,0,300, "10,-1,255,13,1")
460 CALL MK_ROOM(4,840,300,1220,700,0,300, "10,-1,255,14,1")
470 CALL MK_ROOM(5,840,700,1220,1000,0,300, "10,-1,255,15,1")
480 CALL MK_ROOM(69,180,730,840,1000,0,300, "10,-1,255,12,1")
490 CALL MK_ROOM(9,1260,760,1460,960,0,128, "9,9,150,16,9")
500 CALL MK_ROOM(6,440,300,520,330,0,100, "13,2,200,1,12")
510 CALL MK_ROOM(7,180,520,200,584,0,0, "14,3,190,2,12")
520 CALL MK_ROOM(8,1240,820,1260,884,0,0, "9,9,190,3,12")
530 CALL MK_ROOM(61,1220,820,1240,884,0,100, "10,-1,255,0,1")
540 ' the ground floor
550 CALL MK_ROOM(10,200,330,400,680,0,112, "14,3,205,20,3")
560 CALL MK_ROOM(11,400,330,560,400,0,112, "13,2,215,21,2")
570 CALL MK_ROOM(65,400,400,450,592,0,112, "13,2,215,21,2")
580 CALL MK_ROOM(66,510,400,560,592,0,112, "13,2,215,21,2")
590 CALL MK_ROOM(67,400,592,560,680,0,112, "13,2,215,21,2")
600 CALL MK_ROOM(12,560,330,800,680,0,112, "15,4,200,22,4")
610 CALL MK_LINKX(0,"N",440,520,6, "12,27,-1,0,0,0")
620 CALL MK_LINKT(6,"N",440,520,11,12)
630 CALL MK_LINK(1,"S",-380,180,0)
640 CALL MK_LINK(2,"S",-380,180,1)
650 CALL MK_LINK(3,"S",-380,-100,2)
660 CALL MK_LINK(4,"S",840,1220,0)
670 CALL MK_LINK(5,"S",840,1220,4)
680 CALL MK_LINK(2,"E",730,1000,69)
690 CALL MK_LINK(69,"E",730,1000,5)
700 CALL MK_LINKT(1,"E",520,584,7,28)
710 CALL MK_LINKT(7,"E",520,584,10,12)
720 CALL MK_LINK(5,"E",820,884,61)
730 CALL MK_LINKT(61,"E",820,884,8,12)
740 CALL MK_LINKT(8,"E",820,884,9,12)
750 CALL MK_LINK(10,"E",430,560,65)
760 CALL MK_LINK(66,"E",430,560,12)
770 CALL MK_LINK(11,"N",400,450,65)
780 CALL MK_LINK(11,"N",510,560,66)
790 CALL MK_LINK(65,"N",400,450,67)
800 CALL MK_LINK(66,"N",510,560,67)
810 ' the stairs up to the landing: eight steps of 24, in the middle of the hall (a ring of floor round them)
820 FOR I=1 TO 8
830   CALL MK_ROOM(12+I,450,400+24*(I-1),510,400+24*I,14*I,112+14*I, "18,2,195,0,18")
840 NEXT I
850 CALL MK_LINKT(11,"N",450,510,13,18)
860 FOR I=1 TO 7
870   CALL MK_LINKT(12+I,"N",450,510,13+I,18)
880 NEXT I
890 ' the upper floor: landing (over the back of the hall), bathroom (over the kitchen), bedroom (over the living room, with a lobby by the landing)
900 CALL MK_ROOM(21,400,592,560,680,112,224, "9,7,205,23,7")
910 CALL MK_ROOM(23,200,330,400,680,112,224, "17,6,215,25,6")
920 CALL MK_ROOM(22,560,330,800,620,112,224, "16,5,195,24,5")
930 CALL MK_ROOM(48,560,620,608,680,112,224, "16,5,195,24,5")
940 CALL MK_LINKT(20,"N",450,510,21,18)
950 CALL MK_LINK(21,"W",610,670,23)
960 CALL MK_LINK(21,"E",625,675,48)
970 CALL MK_LINK(22,"N",560,608,48)
980 ' the attic ladder (steep steps along the north wall of the bedroom, rising east) and the attic over the whole upper floor
990 FOR I=1 TO 8
1000   Z=224
1010   IF I>=3 THEN Z=300
1020   CALL MK_ROOM(23+I,608+24*(I-1),620,608+24*I,680,112+14*I,Z, "9,25,160,0,9")
1030 NEXT I
1040 CALL MK_LINKT(48,"E",620,680,24,9)
1050 FOR I=1 TO 7
1060   CALL MK_LINKT(23+I,"E",620,680,24+I,9)
1070 NEXT I
1080 CALL MK_ROOM(32,200,330,800,620,224,300, "9,25,150,26,25")
1090 CALL MK_ROOM(47,200,620,656,680,224,300, "9,25,150,26,25")
1100 CALL MK_LINK(32,"N",200,656,47)
1110 FOR I=3 TO 8
1120   CALL MK_LINKT(32,"N",608+24*(I-1),608+24*I,23+I,9)
1130 NEXT I
1140 ' the cellar: the hatch door in the west garden's west wall, five steps down
1150 CALL MK_ROOM(33,-420,420,-400,484,0,0, "8,8,120,4,20")
1160 CALL MK_ROOM(60,-400,420,-380,484,0,100, "10,-1,255,0,1")
1170 CALL MK_LINK(1,"W",420,484,60)
1180 CALL MK_LINKT(60,"W",420,484,33,20)
1190 FOR I=1 TO 5
1200   CALL MK_ROOM(33+I,-420-24*I,420,-420-24*(I-1),484,-16*I,112, "8,8,90,0,8")
1210 NEXT I
1220 CALL MK_LINKT(33,"W",420,484,34,20)
1230 FOR I=1 TO 4
1240   CALL MK_LINKT(33+I,"W",420,484,34+I,8)
1250 NEXT I
1260 CALL MK_ROOM(39,-1000,330,-540,650,-80,40, "8,8,45,27,8")
1270 CALL MK_LINKT(38,"W",420,484,39,8)
1280 ' hiding places: small dark alcoves (tag 50)
1290 CALL MK_ROOM(40,410,680,440,730,0,112, "13,23,25,50,23")
1300 CALL MK_LINK(67,"N",410,440,40)
1310 CALL MK_ROOM(41,220,680,270,730,0,112, "14,23,25,50,23")
1320 CALL MK_LINK(10,"N",220,270,41)
1330 CALL MK_ROOM(42,700,680,760,730,0,112, "15,23,25,50,23")
1340 CALL MK_LINK(12,"N",700,760,42)
1350 CALL MK_ROOM(43,562,680,604,730,112,224, "16,23,25,50,23")
1360 CALL MK_LINK(48,"N",562,604,43)
1370 CALL MK_ROOM(44,220,680,280,730,112,224, "17,23,25,50,23")
1380 CALL MK_LINK(23,"N",220,280,44)
1390 CALL MK_ROOM(45,-1050,400,-1000,460,-80,40, "8,23,20,50,23")
1400 CALL MK_LINK(39,"W",400,460,45)
1410 CALL MK_ROOM(46,800,340,840,400,224,300, "9,23,20,50,23")
1420 CALL MK_LINK(32,"E",340,400,46)
1430 ' furniture blocks (holes in rooms): the tall cupboard with the crystal on top, the chair platform, the bed, the kitchen table
1440 CALL MK_ROOM(50,620,570,700,640,100,112, "26,26,200,0,26")
1450 CALL MK_INNER(12,50,26)
1460 CALL MK_ROOM(51,640,516,680,556,0,200, "26,4,200,40,26")
1470 CALL MK_INNER(12,51,26)
1480 CALL MK_ROOM(52,660,400,780,460,142,224, "16,5,195,0,26")
1490 CALL MK_INNER(22,52,26)
1500 CALL MK_ROOM(53,250,430,330,500,36,112, "26,3,205,0,26")
1510 CALL MK_INNER(10,53,26)
1520 ' windows: WIN id, side (S front y 300..330 / N back y 680..730 / W west x 180..200 / E east x 800..840), lo, hi (along the wall), z1, z2, the garden, the room
1530 ' (the glass: texture 200, flags 2 + 16 = nothing passes, everybody sees through it; the pictures above and below it are the plain facade outside and the room's wall inside)
1540 CALL WIN(80,"S",250,314,40,100,0,1003)
1550 CALL WIN(81,"S",640,704,40,100,0,1204)
1560 CALL WIN(82,"S",322,386,152,212,0,2306)
1570 CALL WIN(83,"S",720,784,152,212,0,2205)
1580 CALL WIN(84,"S",560,624,238,298,0,3225)
1590 CALL WIN(85,"W",380,444,40,100,1,1003)
1600 CALL WIN(86,"W",450,514,152,212,1,2306)
1610 CALL WIN(88,"E",400,464,40,100,4,1204)
1620 CALL WIN(89,"E",480,544,152,212,4,2205)
1630 CALL WIN(90,"E",560,620,238,298,4,3225)
1640 CALL WIN(92,"N",600,664,40,100,69,1204)
1650 CALL WIN(93,"N",300,364,152,212,69,2306)
1660 CALL WIN(94,"N",450,514,152,212,69,2107)
1670 CALL WIN(95,"N",536,596,238,298,69,4725)
1680 ' the pond by the well, level with the grass: the water (texture 205) is animated (201..204) and scrolls
1690 CALL MK_ROOM(70,990,810,1060,860,0,300, "205,-1,255,0,19")
1700 CALL MK_INNER(5,70,19)
1710 ' the sides of the outdoor rooms that are the house (the rest becomes fence sectors)
1720 ' THE TOWN (item 90): a ring road (160 wide) round the block that holds the premises (x -440 .. 1530, y -60 .. 1580); the walls of the street are painted house fronts (textures 210..214, a segment each); in front of the premises a pavement (118), a low railing (119, 121: a masked picture, you see over it, nothing passes) and the gate (120: its floor rises to 80 when it is shut)
1730 ' south road 100..104 (x -600 .. 1690, y -220 .. -60), west road 105..108 (x -600 .. -440, y -60 .. 1580), north road 109..113 (y 1580 .. 1740), east road 114..117 (x 1530 .. 1690); the cars drive on the outer lanes (anticlockwise)
1740 DIM XS(7),YS(6),TS(6),TW(6),TN(6),TE(6)
1750 XS(0)=-600:XS(1)=-142:XS(2)=316:XS(3)=774:XS(4)=1232:XS(5)=1690
1760 YS(0)=-60:YS(1)=350:YS(2)=760:YS(3)=1170:YS(4)=1580
1770 TS(0)=210:TS(1)=211:TS(2)=212:TS(3)=213:TS(4)=211
1780 TN(0)=213:TN(1)=211:TN(2)=214:TN(3)=212:TN(4)=210
1790 TW(0)=212:TW(1)=214:TW(2)=210:TW(3)=213
1800 TE(0)=211:TE(1)=210:TE(2)=214:TE(3)=212
1810 FOR I=0 TO 4
1820   CALL MK_ROOM(400+I,XS(I),-220,XS(I+1),-60,0,300, "220,-1,200,30,"+STR$(TS(I)))
1830   CALL MK_ROOM(409+I,XS(I),1580,XS(I+1),1740,0,300, "220,-1,200,30,"+STR$(TN(I)))
1840 NEXT I
1850 FOR I=0 TO 3
1860   CALL MK_ROOM(405+I,-600,YS(I),-440,YS(I+1),0,300, "220,-1,200,30,"+STR$(TW(I)))
1870   CALL MK_ROOM(414+I,1530,YS(I),1690,YS(I+1),0,300, "220,-1,200,30,"+STR$(TE(I)))
1880 NEXT I
1890 FOR I=0 TO 3
1900   CALL MK_LINK(400+I,"E",-220,-60,401+I)
1910   CALL MK_LINK(409+I,"E",1580,1740,410+I)
1920 NEXT I
1930 FOR I=0 TO 2
1940   CALL MK_LINK(405+I,"N",-600,-440,406+I)
1950   CALL MK_LINK(414+I,"N",1530,1690,415+I)
1960 NEXT I
1970 CALL MK_LINK(400,"N",-600,-440,405)
1980 CALL MK_LINK(404,"N",1530,1690,414)
1990 CALL MK_LINK(408,"N",-600,-440,409)
2000 CALL MK_LINK(417,"N",1530,1690,413)
2010 CALL MK_ROOM(418,-380,-60,1240,-10,0,300, "24,-1,255,31,214")
2020 CALL MK_LINK(418,"S",-380,-142,400)
2030 CALL MK_LINK(418,"S",-142,316,401)
2040 CALL MK_LINK(418,"S",316,774,402)
2050 CALL MK_LINK(418,"S",774,1232,403)
2060 CALL MK_ROOM(419,-380,-10,440,0,0,300, "24,-1,255,0,251")
2070 CALL MK_ROOM(420,440,-10,520,0,0,300, "24,-1,255,0,251")
2080 CALL MK_ROOM(421,520,-10,1240,0,0,300, "24,-1,255,0,251")
2090 CALL MK_LINKX(418,"N",-380,440,419, "250,251,251,3,0,0")
2100 CALL MK_LINKX(419,"N",-380,440,0, "250,251,251,3,0,0")
2110 CALL MK_LINKX(418,"N",440,520,420, "-1,251,251,0,0,0")
2120 CALL MK_LINKX(420,"N",440,520,0, "-1,251,251,0,0,0")
2130 CALL MK_LINKX(418,"N",520,1240,421, "250,251,251,3,0,0")
2140 CALL MK_LINKX(421,"N",520,1240,0, "250,251,251,3,0,0")
2150 CALL MK_HOUSE(0,"N",180,840)
2160 CALL MK_HOUSE(1,"E",300,700)
2170 CALL MK_HOUSE(2,"E",700,730)
2180 CALL MK_HOUSE(4,"W",300,700)
2190 CALL MK_HOUSE(5,"W",700,730)
2200 CALL MK_HOUSE(69,"S",180,840)
2210 CALL MK_OUTDOOR(3)
2220 CALL MK_FINISH(27,300,29)
2230 CALL MK_EMIT(ARG$(1))
