1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
50 ' deqq2.bas - the house, garden and cellar of demos/deqq2.bas (Deqq's House II) as DATA lines: engine1999c deqq2.bas [file]  (without a file: standard output)
60 LOAD LIBRARY "mkw3d"
70 PROC WIN(ID,S$,LO,HI,Z1,Z2,G,RR)
80   LOCAL X1,Y1,X2,Y2,R,RT
85   R=RR\100:RT=RR MOD 100
90   IF S$="S" THEN X1=LO:X2=HI:Y1=300:Y2=330
100   IF S$="N" THEN X1=LO:X2=HI:Y1=680:Y2=730
110   IF S$="W" THEN X1=180:X2=200:Y1=LO:Y2=HI
120   IF S$="E" THEN X1=800:X2=840:Y1=LO:Y2=HI
130   CALL MK_ROOM(ID,X1,Y1,X2,Y2,Z1,Z2, "9,3,200,0,1")
140   IF S$="S" THEN
150     CALL MK_LINKX(G,"N",LO,HI,ID, "200,27,27,18,0,0")
160     CALL MK_LINKX(ID,"N",LO,HI,R, "200,"+STR$(RT)+","+STR$(RT)+",18,0,0")
170   END IF
180   IF S$="N" THEN
190     CALL MK_LINKX(R,"N",LO,HI,ID, "200,"+STR$(RT)+","+STR$(RT)+",18,0,0")
200     CALL MK_LINKX(ID,"N",LO,HI,G, "200,27,27,18,0,0")
210   END IF
220   IF S$="W" THEN
230     CALL MK_LINKX(G,"E",LO,HI,ID, "200,27,27,18,0,0")
240     CALL MK_LINKX(ID,"E",LO,HI,R, "200,"+STR$(RT)+","+STR$(RT)+",18,0,0")
250   END IF
260   IF S$="E" THEN
270     CALL MK_LINKX(R,"E",LO,HI,ID, "200,"+STR$(RT)+","+STR$(RT)+",18,0,0")
280     CALL MK_LINKX(ID,"E",LO,HI,G, "200,27,27,18,0,0")
290   END IF
300 END PROC
310 ' THE HOUSE (since item 89 a real two-storey house with an attic: the rooms upstairs lie OVER the rooms downstairs): x 180 .. 840, y 300 .. 730 (front y 300, back y 730); storeys: ground 0 .. 112, upper 112 .. 224, attic 224 .. 300
320 ' ground: kitchen 10 (200..400, 330..680), hall = a ring round the stairs (11 front, 65 west arm, 66 east arm, 67 back), living room 12 (560..800, 330..680); stairs 13..20 (x 450..510, y 400..592, rising north, 8 steps of 24)
330 ' upper: bathroom 23 (over the kitchen), landing 21 (over the back of the hall), bedroom 22 (over the living room) + 48 (its lobby by the landing), ladder 24..31 (x 608..800, y 620..680, rising east), attic 32 (200..800, 330..620) + 47 (200..680, 620..680)
340 ' windows (niche sectors 80..95, glass = texture 200): ground z 40..100, upper z 152..212, attic z 238..298; the same window is seen from outside and from inside
350 ' textures: 29 the backdrop behind the fences (128 x 300: tree line and night sky above a brick fence), 27 the plain front of the house (128 x 300: roof band, belt courses, bricks), 28 a door in it (64 x 300), 1 gray brick, 2 plaster purple (hall), 3 plaster teal (kitchen), 4 plaster magenta (living room), 5 plaster blue (bedroom), 6 bath tiles,
360 ' 7 plaster sand (landing), 8 stone (cellar), 9 planks, 10 grass, 11 sky, 12 brown door, 13 hall floor, 14 kitchen floor, 15 red carpet, 16 blue carpet,
370 ' 17 bath floor, 18 stair planks, 19 mossy stone, 20 gray door, 23 dark alcove, 24 sand stone (front garden), 25 attic wall (planks darker), 26 cupboard wood, 200 glass, 205 water
380 ' tags: 1 front door, 2 back door, 3 shed door, 4 hatch, 50 a hiding place, 40 the chair platform, 99 nothing; rooms: 10 front garden, 11 west garden, 12 back yard,
390 ' 13 crash site, 14 east garden, 15 north-east corner, 16 shed, 20 kitchen, 21 hall, 22 living room, 23 landing, 24 bedroom, 25 bathroom, 26 attic, 27 cellar
400 ' outdoors: floor grass (10), sky ceiling, fence = gray brick (1); 300 high: the house front stands on the north side of the front garden
410 CALL MK_ROOM(0,-380,0,1240,300,0,300, "24,-1,255,10,1")
420 CALL MK_ROOM(1,-380,300,180,700,0,300, "10,-1,255,11,1")
430 CALL MK_ROOM(2,-380,700,180,1250,0,300, "10,-1,255,12,1")
440 CALL MK_ROOM(3,-380,1250,-100,1500,0,300, "10,-1,255,13,1")
450 CALL MK_ROOM(4,840,300,1220,700,0,300, "10,-1,255,14,1")
460 CALL MK_ROOM(5,840,700,1220,1000,0,300, "10,-1,255,15,1")
470 CALL MK_ROOM(69,180,730,840,1000,0,300, "10,-1,255,12,1")
480 CALL MK_ROOM(9,1260,760,1460,960,0,128, "9,9,150,16,9")
490 CALL MK_ROOM(6,440,300,520,330,0,100, "13,2,200,1,12")
500 CALL MK_ROOM(7,180,520,200,584,0,0, "14,3,190,2,12")
510 CALL MK_ROOM(8,1240,820,1260,884,0,0, "9,9,190,3,12")
520 CALL MK_ROOM(61,1220,820,1240,884,0,100, "10,-1,255,0,1")
530 ' the ground floor
540 CALL MK_ROOM(10,200,330,400,680,0,112, "14,3,205,20,3")
550 CALL MK_ROOM(11,400,330,560,400,0,112, "13,2,215,21,2")
560 CALL MK_ROOM(65,400,400,450,592,0,112, "13,2,215,21,2")
570 CALL MK_ROOM(66,510,400,560,592,0,112, "13,2,215,21,2")
580 CALL MK_ROOM(67,400,592,560,680,0,112, "13,2,215,21,2")
590 CALL MK_ROOM(12,560,330,800,680,0,112, "15,4,200,22,4")
600 CALL MK_LINKX(0,"N",440,520,6, "12,27,-1,0,0,0")
610 CALL MK_LINKT(6,"N",440,520,11,12)
620 CALL MK_LINK(1,"S",-380,180,0)
630 CALL MK_LINK(2,"S",-380,180,1)
640 CALL MK_LINK(3,"S",-380,-100,2)
650 CALL MK_LINK(4,"S",840,1220,0)
660 CALL MK_LINK(5,"S",840,1220,4)
670 CALL MK_LINK(2,"E",730,1000,69)
680 CALL MK_LINK(69,"E",730,1000,5)
690 CALL MK_LINKT(1,"E",520,584,7,28)
700 CALL MK_LINKT(7,"E",520,584,10,12)
710 CALL MK_LINK(5,"E",820,884,61)
720 CALL MK_LINKT(61,"E",820,884,8,12)
730 CALL MK_LINKT(8,"E",820,884,9,12)
740 CALL MK_LINK(10,"E",430,560,65)
750 CALL MK_LINK(66,"E",430,560,12)
760 CALL MK_LINK(11,"N",400,450,65)
770 CALL MK_LINK(11,"N",510,560,66)
780 CALL MK_LINK(65,"N",400,450,67)
790 CALL MK_LINK(66,"N",510,560,67)
800 ' the stairs up to the landing: eight steps of 24, in the middle of the hall (a ring of floor round them)
810 FOR I=1 TO 8
820   CALL MK_ROOM(12+I,450,400+24*(I-1),510,400+24*I,14*I,112+14*I, "18,2,195,0,18")
830 NEXT I
840 CALL MK_LINKT(11,"N",450,510,13,18)
850 FOR I=1 TO 7
860   CALL MK_LINKT(12+I,"N",450,510,13+I,18)
870 NEXT I
880 ' the upper floor: landing (over the back of the hall), bathroom (over the kitchen), bedroom (over the living room, with a lobby by the landing)
890 CALL MK_ROOM(21,400,592,560,680,112,224, "9,7,205,23,7")
900 CALL MK_ROOM(23,200,330,400,680,112,224, "17,6,215,25,6")
910 CALL MK_ROOM(22,560,330,800,620,112,224, "16,5,195,24,5")
920 CALL MK_ROOM(48,560,620,608,680,112,224, "16,5,195,24,5")
930 CALL MK_LINKT(20,"N",450,510,21,18)
940 CALL MK_LINK(21,"W",610,670,23)
950 CALL MK_LINK(21,"E",625,675,48)
960 CALL MK_LINK(22,"N",560,608,48)
970 ' the attic ladder (steep steps along the north wall of the bedroom, rising east) and the attic over the whole upper floor
980 FOR I=1 TO 8
990   Z=224
1000   IF I>=3 THEN Z=300
1010   CALL MK_ROOM(23+I,608+24*(I-1),620,608+24*I,680,112+14*I,Z, "9,25,160,0,9")
1020 NEXT I
1030 CALL MK_LINKT(48,"E",620,680,24,9)
1040 FOR I=1 TO 7
1050   CALL MK_LINKT(23+I,"E",620,680,24+I,9)
1060 NEXT I
1070 CALL MK_ROOM(32,200,330,800,620,224,300, "9,25,150,26,25")
1080 CALL MK_ROOM(47,200,620,656,680,224,300, "9,25,150,26,25")
1090 CALL MK_LINK(32,"N",200,656,47)
1100 FOR I=3 TO 8
1110   CALL MK_LINKT(32,"N",608+24*(I-1),608+24*I,23+I,9)
1120 NEXT I
1130 ' the cellar: the hatch door in the west garden's west wall, five steps down
1140 CALL MK_ROOM(33,-420,420,-400,484,0,0, "8,8,120,4,20")
1150 CALL MK_ROOM(60,-400,420,-380,484,0,100, "10,-1,255,0,1")
1160 CALL MK_LINK(1,"W",420,484,60)
1170 CALL MK_LINKT(60,"W",420,484,33,20)
1180 FOR I=1 TO 5
1190   CALL MK_ROOM(33+I,-420-24*I,420,-420-24*(I-1),484,-16*I,112, "8,8,90,0,8")
1200 NEXT I
1210 CALL MK_LINKT(33,"W",420,484,34,20)
1220 FOR I=1 TO 4
1230   CALL MK_LINKT(33+I,"W",420,484,34+I,8)
1240 NEXT I
1250 CALL MK_ROOM(39,-1000,330,-540,650,-80,40, "8,8,45,27,8")
1260 CALL MK_LINKT(38,"W",420,484,39,8)
1270 ' hiding places: small dark alcoves (tag 50)
1280 CALL MK_ROOM(40,410,680,440,730,0,112, "13,23,25,50,23")
1290 CALL MK_LINK(67,"N",410,440,40)
1300 CALL MK_ROOM(41,220,680,270,730,0,112, "14,23,25,50,23")
1310 CALL MK_LINK(10,"N",220,270,41)
1320 CALL MK_ROOM(42,700,680,760,730,0,112, "15,23,25,50,23")
1330 CALL MK_LINK(12,"N",700,760,42)
1340 CALL MK_ROOM(43,562,680,604,730,112,224, "16,23,25,50,23")
1350 CALL MK_LINK(48,"N",562,604,43)
1360 CALL MK_ROOM(44,220,680,280,730,112,224, "17,23,25,50,23")
1370 CALL MK_LINK(23,"N",220,280,44)
1380 CALL MK_ROOM(45,-1050,400,-1000,460,-80,40, "8,23,20,50,23")
1390 CALL MK_LINK(39,"W",400,460,45)
1400 CALL MK_ROOM(46,800,340,840,400,224,300, "9,23,20,50,23")
1410 CALL MK_LINK(32,"E",340,400,46)
1420 ' furniture blocks (holes in rooms): the tall cupboard with the crystal on top, the chair platform, the bed, the kitchen table
1430 CALL MK_ROOM(50,620,570,700,640,100,112, "26,26,200,0,26")
1440 CALL MK_INNER(12,50,26)
1450 CALL MK_ROOM(51,640,516,680,556,0,200, "26,4,200,40,26")
1460 CALL MK_INNER(12,51,26)
1470 CALL MK_ROOM(52,660,400,780,460,142,224, "16,5,195,0,26")
1480 CALL MK_INNER(22,52,26)
1490 CALL MK_ROOM(53,250,430,330,500,36,112, "26,3,205,0,26")
1500 CALL MK_INNER(10,53,26)
1510 ' windows: WIN id, side (S front y 300..330 / N back y 680..730 / W west x 180..200 / E east x 800..840), lo, hi (along the wall), z1, z2, the garden, the room
1520 ' (the glass: texture 200, flags 2 + 16 = nothing passes, everybody sees through it; the pictures above and below it are the plain facade outside and the room's wall inside)
1530 CALL WIN(80,"S",250,314,40,100,0,1003)
1531 CALL WIN(81,"S",640,704,40,100,0,1204)
1532 CALL WIN(82,"S",322,386,152,212,0,2306)
1533 CALL WIN(83,"S",720,784,152,212,0,2205)
1534 CALL WIN(84,"S",560,624,238,298,0,3225)
1535 CALL WIN(85,"W",380,444,40,100,1,1003)
1536 CALL WIN(86,"W",450,514,152,212,1,2306)
1537 CALL WIN(88,"E",400,464,40,100,4,1204)
1538 CALL WIN(89,"E",480,544,152,212,4,2205)
1539 CALL WIN(90,"E",560,620,238,298,4,3225)
1540 CALL WIN(92,"N",600,664,40,100,69,1204)
1541 CALL WIN(93,"N",300,364,152,212,69,2306)
1542 CALL WIN(94,"N",450,514,152,212,69,2107)
1543 CALL WIN(95,"N",536,596,238,298,69,4725)
1680 ' the pond by the well, level with the grass: the water (texture 205) is animated (201..204) and scrolls
1690 CALL MK_ROOM(70,990,810,1060,860,0,300, "205,-1,255,0,19")
1700 CALL MK_INNER(5,70,19)
1710 ' the sides of the outdoor rooms that are the house (the rest becomes fence sectors)
1720 CALL MK_HOUSE(0,"N",180,840)
1730 CALL MK_HOUSE(1,"E",300,700)
1740 CALL MK_HOUSE(2,"E",700,730)
1750 CALL MK_HOUSE(4,"W",300,700)
1760 CALL MK_HOUSE(5,"W",700,730)
1770 CALL MK_HOUSE(69,"S",180,840)
1780 CALL MK_OUTDOOR(3)
1790 CALL MK_FINISH(27,300,29)
1800 CALL MK_EMIT(ARG$(1))
