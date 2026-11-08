1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' manor.bas - the level of demos/manor.bas (Manor Walk) as DATA lines: engine1999c manor.bas [file]  (without a file: standard output)
20 ' textures: 1 brick 2 plaster sand 3 plaster azure 4 stone 5 hall tiles 6 carpet 7 planks 8 grass 9 sky 10 door 11 window 12 grate 14 stone sand 15 plaster lime 16 hedge stone 30 water
30 ' CALL MK_ROOM(id,x1,y1,x2,y2,floor,ceil,"floortex,ceiltex,light,tag,walltex")
40 LOAD LIBRARY "mkw3d"
50 CALL MK_ROOM(0, 0,0,640,320, 0,128, "8,-1,255,10,1")
60 CALL MK_ROOM(1, 288,320,352,340, 0,0, "5,2,190,1,2")
70 CALL MK_ROOM(2, 192,340,448,540, 0,128, "5,2,215,11,2")
80 CALL MK_ROOM(3, 448,340,704,540, 0,128, "6,2,195,12,3")
90 CALL MK_ROOM(4, -64,340,192,540, 0,128, "7,2,190,13,15")
100 CALL MK_LINKT(0,"N",288,352,1, 10)
110 CALL MK_LINKT(1,"N",288,352,2, 10)
120 CALL MK_LINK(2,"E",380,500,3)
130 CALL MK_LINK(2,"W",360,420,4)
140 CALL MK_LINKX(2,"W",440,504,4, "11,-1,-1,3,0,-32")
150 ' stairs up to the gallery
160 CALL MK_ROOM(5, 288,540,352,564, 16,136, "14,2,185,0,14")
170 FOR I=2 TO 8
180   CALL MK_ROOM(4+I, 288,540+24*(I-1),352,540+24*I, 16*I,120+16*I, "14,2,185,0,14")
190 NEXT I
200 CALL MK_ROOM(13, 160,732,480,952, 128,240, "7,2,205,14,2")
210 CALL MK_LINKT(2,"N",288,352,5, 14)
220 FOR I=1 TO 7
230   CALL MK_LINKT(4+I,"N",288,352,5+I, 14)
240 NEXT I
250 CALL MK_LINKT(12,"N",288,352,13, 14)
260 CALL MK_PILLAR(2, 220,380,236,396, 4):CALL MK_PILLAR(2, 404,380,420,396, 4)
270 CALL MK_PILLAR(2, 220,484,236,500, 4):CALL MK_PILLAR(2, 404,484,420,500, 4)
280 ' the cupola in the living room (a higher ceiling), a pool in the garden (a step down of 32, too much to walk into)
290 CALL MK_ROOM(14, 540,400,620,480, 0,256, "6,2,215,12,3")
300 CALL MK_INNER(3,14,-1)
310 CALL MK_ROOM(15, 440,60,580,200, -32,128, "30,-1,230,16,16")
320 CALL MK_INNER(0,15,-1)
330 CALL MK_PILLAR(0, 100,120,124,144, 4)
340 ' the cellar: a door in the study's west wall, six steps down
350 CALL MK_ROOM(16, -84,400,-64,464, 0,0, "4,4,120,5,4")
360 CALL MK_LINKT(4,"W",400,464,16, 10)
370 FOR I=1 TO 6
380   CALL MK_ROOM(16+I, -84-24*I,400,-84-24*(I-1),464, -16*I,112, "4,4,90,0,4")
390 NEXT I
400 CALL MK_LINKT(16,"W",400,464,17, 10)
410 FOR I=1 TO 5
420   CALL MK_LINKT(16+I,"W",400,464,17+I, 4)
430 NEXT I
440 CALL MK_ROOM(23, -484,330,-228,550, -96,40, "4,4,70,15,4")
450 CALL MK_LINKT(22,"W",400,464,23, 4)
460 CALL MK_PILLAR(23, -370,420,-346,444, 4)
470 CALL MK_EMIT(ARG$(1))
