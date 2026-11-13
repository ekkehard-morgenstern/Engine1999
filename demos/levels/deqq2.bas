1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' deqq2.bas - the house, garden and cellar of demos/deqq2.bas (Deqq's House II) as DATA lines: engine1999c deqq2.bas [file]  (without a file: standard output)
20 LOAD LIBRARY "mkw3d"
30 ' textures: 29 the backdrop behind the fences (128 x 300: tree line and night sky above a brick fence), 27 the front of the house (128 x 300: roof, two floors with windows), 28 a door in it (64 x 300), 1 gray brick, 2 plaster purple (hall), 3 plaster teal (kitchen), 4 plaster magenta (living room), 5 plaster blue (bedroom), 6 bath tiles,
40 ' 7 plaster sand (landing), 8 stone (cellar), 9 planks, 10 grass, 11 sky, 12 brown door, 13 hall floor, 14 kitchen floor, 15 red carpet, 16 blue carpet,
50 ' 17 bath floor, 18 stair planks, 19 mossy stone, 20 gray door, 23 dark alcove, 24 sand stone (front garden), 25 attic wall (planks darker), 26 cupboard wood
60 ' tags: 1 front door, 2 back door, 3 shed door, 4 hatch, 50 a hiding place, 40 the chair platform, 99 nothing; rooms: 10 front garden, 11 west garden, 12 back yard,
70 ' 13 crash site, 14 east garden, 15 north-east corner, 16 shed, 20 kitchen, 21 hall, 22 living room, 23 landing, 24 bedroom, 25 bathroom, 26 attic, 27 cellar
80 ' outdoors: floor grass (10), sky ceiling, fence = gray brick (1)
90 CALL MK_ROOM(0,-380,0,1240,300,0,300, "24,-1,255,10,1") ' front garden (a road of sand stone); 300 high: the house front stands on its north side
100 CALL MK_ROOM(1,-380,300,180,700,0,300, "10,-1,255,11,1") ' west garden (the hatch); 300 high: the west side of the house
110 CALL MK_ROOM(2,-380,700,180,1250,0,300, "10,-1,255,12,1") ' back yard; 300 high: the back of the house
120 CALL MK_ROOM(3,-380,1250,-100,1500,0,300, "10,-1,255,13,1") ' the crash site (300 high like the rest of the garden)
130 CALL MK_ROOM(4,840,300,1220,700,0,300, "10,-1,255,14,1") ' east garden; 300 high: the east side of the house
140 CALL MK_ROOM(5,840,700,1220,1000,0,300, "10,-1,255,15,1") ' north-east corner (the well, the tree); 300 high: the east side of the house
150 CALL MK_ROOM(9,1260,760,1460,960,0,128, "9,9,150,16,9") ' the tool shed
160 CALL MK_ROOM(6,440,300,520,330,0,100, "13,2,200,1,12") ' front door (open: ceiling 100)
170 CALL MK_ROOM(7,180,520,200,584,0,0, "14,3,190,2,12") ' back door (locked, closed: ceiling = floor)
180 CALL MK_ROOM(8,1240,820,1260,884,0,0, "9,9,190,3,12") ' shed door
190 CALL MK_ROOM(61,1220,820,1240,884,0,100, "10,-1,255,0,1") ' a niche in the garden's east wall in front of the shed door (fence high)
191 CALL MK_ROOM(62,180,1050,460,1192,0,300, "10,-1,255,12,1") ' the back yard's nook behind the house: the back of the house (the north face south of it), 300 high
200 ' the house
210 CALL MK_ROOM(10,200,330,400,680,0,128, "14,3,205,20,3") ' kitchen
220 CALL MK_ROOM(11,400,330,560,680,0,128, "13,2,215,21,2") ' hall
230 CALL MK_ROOM(12,560,330,800,680,0,128, "15,4,200,22,4") ' living room
240 CALL MK_LINKX(0,"N",440,520,6, "12,27,-1,0,0,0")
250 CALL MK_LINKT(6,"N",440,520,11,12)
260 CALL MK_LINK(1,"S",-380,180,0)
270 CALL MK_LINK(2,"S",-380,180,1)
280 CALL MK_LINK(3,"S",-380,-100,2)
290 CALL MK_LINK(4,"S",840,1220,0)
300 CALL MK_LINK(5,"S",840,1220,4)
301 CALL MK_LINK(2,"E",1050,1192,62)
310 CALL MK_LINKT(1,"E",520,584,7,28)
320 CALL MK_LINKT(7,"E",520,584,10,12)
330 CALL MK_LINK(5,"E",820,884,61)
340 CALL MK_LINKT(61,"E",820,884,8,12)
350 CALL MK_LINKT(8,"E",820,884,9,12)
360 CALL MK_LINK(10,"E",430,560,11)
370 CALL MK_LINK(11,"E",430,560,12)
380 ' the stairs up to the landing (8 steps of 16)
390 FOR I=1 TO 8
400   CALL MK_ROOM(12+I,440,680+24*(I-1),520,680+24*I,16*I,120+16*I, "18,2,195,0,18")
410 NEXT I
420 CALL MK_LINKT(11,"N",440,520,13,18)
430 FOR I=1 TO 7
440   CALL MK_LINKT(12+I,"N",440,520,13+I,18)
450 NEXT I
460 CALL MK_ROOM(21,380,872,620,1000,128,256, "9,7,205,23,7") ' landing
470 CALL MK_ROOM(22,620,872,800,1000,128,256, "16,5,195,24,5") ' bedroom
480 CALL MK_ROOM(23,200,872,380,1000,128,256, "17,6,215,25,6") ' bathroom
490 CALL MK_LINKT(20,"N",440,520,21,18)
500 CALL MK_LINK(21,"E",900,960,22)
510 CALL MK_LINK(21,"W",900,960,23)
520 ' the attic ladder (steep steps) and the attic
530 FOR I=1 TO 8
540   CALL MK_ROOM(23+I,460,1000+24*(I-1),540,1000+24*I,128+16*I,248+16*I, "9,25,160,0,9")
550 NEXT I
560 CALL MK_ROOM(32,360,1192,640,1400,256,384, "9,25,150,26,25") ' attic
570 CALL MK_LINKT(21,"N",460,540,24,9)
580 FOR I=1 TO 7
590   CALL MK_LINKT(23+I,"N",460,540,24+I,9)
600 NEXT I
610 CALL MK_LINKT(31,"N",460,540,32,9)
620 ' the cellar: the hatch door in the west garden's west wall, five steps down
630 CALL MK_ROOM(33,-420,420,-400,484,0,0, "8,8,120,4,20")
640 CALL MK_ROOM(60,-400,420,-380,484,0,100, "10,-1,255,0,1") ' a niche in the garden's west wall in front of the hatch (fence high)
650 CALL MK_LINK(1,"W",420,484,60)
660 CALL MK_LINKT(60,"W",420,484,33,20)
670 FOR I=1 TO 5
680   CALL MK_ROOM(33+I,-420-24*I,420,-420-24*(I-1),484,-16*I,112, "8,8,90,0,8")
690 NEXT I
700 CALL MK_LINKT(33,"W",420,484,34,20)
710 FOR I=1 TO 4
720   CALL MK_LINKT(33+I,"W",420,484,34+I,8)
730 NEXT I
740 CALL MK_ROOM(39,-1000,330,-540,650,-80,40, "8,8,45,27,8") ' cellar (dark: a lantern lights it)
750 CALL MK_LINKT(38,"W",420,484,39,8)
760 ' hiding places: small dark alcoves (tag 50)
770 CALL MK_ROOM(40,410,680,440,730,0,128, "13,23,25,50,23") ' the coat closet (hall)
780 CALL MK_LINK(11,"N",410,440,40)
790 CALL MK_ROOM(41,220,680,270,730,0,128, "14,23,25,50,23") ' the pantry (kitchen)
800 CALL MK_LINK(10,"N",220,270,41)
810 CALL MK_ROOM(42,700,680,760,730,0,128, "15,23,25,50,23") ' behind the sofa (living room)
820 CALL MK_LINK(12,"N",700,760,42)
830 CALL MK_ROOM(43,700,1000,760,1050,128,256, "16,23,25,50,23") ' the wardrobe (bedroom)
840 CALL MK_LINK(22,"N",700,760,43)
850 CALL MK_ROOM(44,220,1000,280,1050,128,256, "17,23,25,50,23") ' behind the shower curtain (bathroom)
860 CALL MK_LINK(23,"N",220,280,44)
870 CALL MK_ROOM(45,-1050,400,-1000,460,-80,40, "8,23,20,50,23") ' between the barrels (cellar)
880 CALL MK_LINK(39,"W",400,460,45)
890 CALL MK_ROOM(46,640,1250,700,1310,256,384, "9,23,20,50,23") ' the trunk (attic)
900 CALL MK_LINK(32,"E",1250,1310,46)
910 ' furniture blocks (holes in rooms): the tall cupboard with the crystal on top, the chair platform, the bed, the kitchen table, the fridge
920 CALL MK_ROOM(50,620,570,700,640,100,128, "26,26,200,0,26") ' the cupboard: its top (a floor at 100) is out of reach
930 CALL MK_INNER(12,50,26)
940 CALL MK_ROOM(51,640,516,680,556,0,200, "26,4,200,40,26") ' the chair platform (the floor rises to 56 when a chair is placed)
950 CALL MK_INNER(12,51,26)
960 CALL MK_ROOM(52,660,900,780,960,30,128, "16,5,195,0,26") ' the bed (a wooden frame, a blue blanket on top)
970 CALL MK_INNER(22,52,26)
980 CALL MK_ROOM(53,250,430,330,500,36,128, "26,3,205,0,26") ' the kitchen table
990 CALL MK_INNER(10,53,26)
1000 ' the sides of the outdoor rooms that are the house (the rest becomes fence sectors)
1010 CALL MK_HOUSE(0,"N",180,840)
1020 CALL MK_HOUSE(1,"E",300,700)
1030 CALL MK_HOUSE(2,"E",700,1050)
1040 CALL MK_HOUSE(4,"W",300,700)
1050 CALL MK_HOUSE(5,"W",700,1000)
1051 CALL MK_HOUSE(62,"S",180,460)
1052 CALL MK_HOUSE(62,"E",1050,1192)
1053 CALL MK_HOUSE(62,"N",360,460)
1054 ' glass panes: window niches in the south wall of the kitchen (63) and the living room (64), 40 .. 100 high, translucent glass (texture 200, flags 2 + 16: nothing passes, you see through it)
1055 CALL MK_ROOM(63,250,300,314,330,40,100, "9,3,200,0,1")
1056 CALL MK_LINKX(0,"N",250,314,63, "200,1,1,18,0,0")
1057 CALL MK_LINKX(63,"N",250,314,10, "200,3,3,18,0,0")
1058 CALL MK_ROOM(64,640,300,704,330,40,100, "9,4,200,0,1")
1059 CALL MK_LINKX(0,"N",640,704,64, "200,1,1,18,0,0")
1060 CALL MK_LINKX(64,"N",640,704,12, "200,4,4,18,0,0")
1061 ' the pond by the well, level with the grass (a flat floor stays visible from afar): the water texture 205 is animated (201..204) and scrolls
1062 CALL MK_ROOM(70,990,810,1060,860,0,300, "205,-1,255,0,19")
1063 CALL MK_INNER(5,70,19)
1080 CALL MK_OUTDOOR(3)
1090 CALL MK_FINISH(27,300,29)
1100 CALL MK_EMIT(ARG$(1))
