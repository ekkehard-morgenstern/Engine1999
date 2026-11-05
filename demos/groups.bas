1 ' License: free for everybody to use, copy, change and build into executables (BUILD).
2 ' An executable made with BUILD contains the Engine1999 runtime: the Engine1999 Runtime Exception
3 ' (file LICENSE-EXCEPTION of Engine1999) applies if the runtime is unmodified and its notice stays
4 ' in the executable (it shows with the command line option --engine1999-about).
10 ' GROUPS - sprite groups and tile groups. A big ball made of six sprites (3 x 2) (SPRITE n,SIZE w,h: the head rules the block, here it moves, bounces and
20 ' collides as one sprite), a ghost of four sprites that floats up and down, and walls made of tile groups (the library tilegrp puts a block of tiles
30 ' on the tile screen). The ball turns round when it hits a wall (SPRITE n,COLLISION ON,TILES) and counts the meetings with the ghost. Needs the window
40 ' (engine1999). ESC stops it.
50 LOAD LIBRARY "tilegrp"
60 GFX RESET ALL
70 CLS
80 BREAK OFF
90 PRINT "The big ball is one sprite group of 3 x 2 sprites; the walls are tile groups."
100 FOR L=1 TO 4 STEP 3
110   GFX PALETTE L,1,RGB(40,60,200)
120   GFX PALETTE L,2,RGB(70,100,240)
130   GFX PALETTE L,3,RGB(120,150,255)
140   GFX PALETTE L,4,RGB(190,210,255)
150   GFX PALETTE L,5,RGB(255,255,255)
160   GFX PALETTE L,6,RGB(150,70,40)
170   GFX PALETTE L,7,RGB(210,200,180)
180   GFX PALETTE L,8,RGB(230,200,40)
190   GFX PALETTE L,9,RGB(240,240,240)
200   GFX PALETTE L,10,RGB(30,30,30)
210 NEXT L
220 CALL MAKEBALL
230 CALL MAKEGHOST
240 CALL MAKEWALL
250 CALL TG_DEF(0,10,2,2)
260 CALL TG_DEF(1,14,1,1)
270 FOR Y=0 TO 11 STEP 2
280   CALL TG_PUT(0,0,Y)
290   CALL TG_PUT(0,38,Y)
300 NEXT Y
310 CALL TG_REPEAT(1,2,11,36,1)
320 SPRITE 20,SIZE 3,2
330 SPRITE 20,100,100,20
340 SPRITE 20,COLLISION ON,TILES
350 SPRITE 20,ON
360 SPRITE 30,SIZE 2,2
370 SPRITE 30,320,60,30
380 SPRITE 30,COLLISION ON
390 SPRITE 30,ON
400 BX=100
410 VX=3
420 GY=60
430 VY=1
440 HT=0
445 CD=0
450 REPEAT
460   GFX VSYNC
470   BX=BX+VX
480   SPRITE 20,BX,150
490   GY=GY+VY
500   IF GY<20 OR GY>150 THEN VY=-VY
510   SPRITE 30,320,GY
520   ' collisions are checked once per frame, so a hit that is read now may belong to the position before the last move: after a bounce
522   ' the next hits are stale and are read (to clear them) but ignored for 2 frames
524   H=SPRITE("HITS",20) AND 2
530   IF CD>0 THEN
540     CD=CD-1
545   ELSE
546     IF H THEN
550       IF BX>320 THEN VX=-ABS(VX) ELSE VX=ABS(VX)
552       CD=2
554     END IF
556   END IF
560   IF SPRITE("COLLIDE",20)=30 THEN
570     IF GM=0 THEN HT=HT+1
580     GM=1
590   ELSE
600     GM=0
610   END IF
620   LOCATE 0,2
630   PRINT "Meetings with the ghost:";HT;"  "
640 UNTIL INKEY$<>""
650 SPRITE 20,SIZE 1,1
660 SPRITE 30,SIZE 1,1
670 GFX RESET ALL
680 CLS
690 END
700 ' the ball: six images, 3 x 2 sprites = 48 x 48 pixels, shaded by the distance from the middle of the block
710 PROC MAKEBALL
720   LOCAL K,X,Y,GX,GY,DX,DY,D,C,A$
730   FOR K=0 TO 5
740     A$=""
750     FOR Y=0 TO 23
760       FOR X=0 TO 15
770         GX=(K MOD 3)*16+X
780         GY=(K\3)*24+Y
790         DX=(GX-24)/24
800         DY=(GY-24)/24
810         D=SQR(DX*DX+DY*DY)
820         C=0
830         IF D<1 THEN C=1+INT((1-D)*4)
840         IF (DX+0.45)^2+(DY+0.45)^2<0.02 THEN C=5
850         A$=A$+CHR$(C)
860       NEXT X
870     NEXT Y
880     SPRITE DEF 20+K,A$
890   NEXT K
900 END PROC
910 ' the ghost: 2 x 2 sprites, a rounded body with two eyes
920 PROC MAKEGHOST
930   LOCAL K,X,Y,GX,GY,C,A$
940   FOR K=0 TO 3
950     A$=""
960     FOR Y=0 TO 23
970       FOR X=0 TO 15
980         GX=(K MOD 2)*16+X
990         GY=(K\2)*24+Y
1000         C=0
1010         IF (GX-16)^2+(GY-16)^2<256 OR (GY>=16 AND GX>=1 AND GX<=30) THEN C=9
1020         IF GY>=42 AND (GX MOD 8)<4 THEN C=0
1030         IF (GX-10)^2+(GY-16)^2<12 OR (GX-22)^2+(GY-16)^2<12 THEN C=10
1040         A$=A$+CHR$(C)
1050       NEXT X
1060     NEXT Y
1070     SPRITE DEF 30+K,A$
1080   NEXT K
1090 END PROC
1100 ' the wall: four tiles in a block (a brick pattern over 32 x 48 pixels) and one floor tile
1110 PROC MAKEWALL
1120   LOCAL K,X,Y,GX,GY,C,A$
1130   FOR K=0 TO 3
1140     A$=""
1150     FOR Y=0 TO 23
1160       FOR X=0 TO 15
1170         GX=(K MOD 2)*16+X
1180         GY=(K\2)*24+Y
1190         C=6
1200         IF GY MOD 12=0 OR (GX+((GY\12) MOD 2)*8) MOD 16=0 THEN C=7
1210         A$=A$+CHR$(C)
1220       NEXT X
1230     NEXT Y
1240     TILE DEF 10+K,A$
1250   NEXT K
1260   A$=""
1270   FOR Y=0 TO 23
1280     FOR X=0 TO 15
1290       A$=A$+CHR$(8-(Y>3)*2)
1300     NEXT X
1310   NEXT Y
1320   TILE DEF 14,A$
1330 END PROC
