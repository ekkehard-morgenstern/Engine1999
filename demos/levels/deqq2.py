#!/usr/bin/env python3
# The house, the garden and the cellar of demos/deqq2.bas (Deqq's House II) as DATA lines: python3 deqq2.py [file]
# textures: 1 gray brick, 2 plaster purple (hall), 3 plaster teal (kitchen), 4 plaster magenta (living room), 5 plaster blue (bedroom), 6 bath tiles,
# 7 plaster sand (landing), 8 stone (cellar), 9 planks, 10 grass, 11 sky, 12 brown door, 13 hall floor, 14 kitchen floor, 15 red carpet, 16 blue carpet,
# 17 bath floor, 18 stair planks, 19 mossy stone, 20 gray door, 23 dark alcove, 24 sand stone (front garden), 25 attic wall (planks darker), 26 cupboard wood
# tags: 1 front door, 2 back door, 3 shed door, 4 hatch, 50 a hiding place, 40 the chair platform, 99 nothing; rooms: 10 front garden, 11 west garden, 12 back yard,
# 13 crash site, 14 east garden, 15 north-east corner, 16 shed, 20 kitchen, 21 hall, 22 living room, 23 landing, 24 bedroom, 25 bathroom, 26 attic, 27 cellar
from mkw3d import Level
import sys
L=Level()
R=L.room
SKY=-1
# outdoors: floor grass (10), sky ceiling, fence = gray brick (1)
R(0, -400,0,1240,300, 0,128, 24,SKY, 255, 10, 1)        # front garden (a road of sand stone)
R(1, -400,300,180,700, 0,128, 10,SKY, 255, 11, 1)       # west garden (the hatch)
R(2, -400,700,180,1250, 0,128, 10,SKY, 255, 12, 1)      # back yard
R(3, -400,1250,-100,1500, 0,128, 10,SKY, 255, 13, 1)    # the crash site
R(4, 840,300,1240,700, 0,128, 10,SKY, 255, 14, 1)       # east garden
R(5, 840,700,1240,1000, 0,128, 10,SKY, 255, 15, 1)      # north-east corner (the well, the tree)
R(9, 1260,760,1460,960, 0,128, 9,9, 150, 16, 9)         # the tool shed
R(6, 440,300,520,330, 0,100, 13,2, 200, 1, 12)          # front door (open: ceiling 100)
R(7, 180,520,200,584, 0,0, 14,3, 190, 2, 12)            # back door (locked, closed: ceiling = floor)
R(8, 1240,820,1260,884, 0,0, 9,9, 190, 3, 12)           # shed door
# the house
R(10, 200,330,400,680, 0,128, 14,3, 205, 20, 3)         # kitchen
R(11, 400,330,560,680, 0,128, 13,2, 215, 21, 2)         # hall
R(12, 560,330,800,680, 0,128, 15,4, 200, 22, 4)         # living room
L.link(0,'N',440,520,6, tex=12)
L.link(6,'N',440,520,11, tex=12)
L.link(0,'N',-400,180,1) if False else None
L.link(1,'S',-400,180,0)
L.link(2,'S',-400,180,1)
L.link(3,'S',-400,-100,2)
L.link(4,'S',840,1240,0)
L.link(5,'S',840,1240,4)
L.link(1,'E',520,584,7, tex=12)
L.link(7,'E',520,584,10, tex=12)
L.link(5,'E',820,884,8, tex=12)
L.link(8,'E',820,884,9, tex=12)
L.link(10,'E',430,560,11)
L.link(11,'E',430,560,12)
# the stairs up to the landing (8 steps of 16)
for i in range(1,9):
    R(12+i, 440,680+24*(i-1),520,680+24*i, 16*i,120+16*i, 18,2, 195, 0, 18)
L.link(11,'N',440,520,13, tex=18)
for i in range(1,8):
    L.link(12+i,'N',440,520,13+i, tex=18)
R(21, 380,872,620,1000, 128,256, 9,7, 205, 23, 7)       # landing
R(22, 620,872,800,1000, 128,256, 16,5, 195, 24, 5)      # bedroom
R(23, 200,872,380,1000, 128,256, 17,6, 215, 25, 6)      # bathroom
L.link(20,'N',440,520,21, tex=18)
L.link(21,'E',900,960,22)
L.link(21,'W',900,960,23)
# the attic ladder (steep steps) and the attic
for i in range(1,9):
    R(23+i, 460,1000+24*(i-1),540,1000+24*i, 128+16*i,248+16*i, 9,25, 160, 0, 9)
R(32, 360,1192,640,1400, 256,384, 9,25, 150, 26, 25)    # attic
L.link(21,'N',460,540,24, tex=9)
for i in range(1,8):
    L.link(23+i,'N',460,540,24+i, tex=9)
L.link(31,'N',460,540,32, tex=9)
# the cellar: the hatch door in the west garden's west wall, five steps down
R(33, -420,420,-400,484, 0,0, 8,8, 120, 4, 20)
L.link(1,'W',420,484,33, tex=20)
for i in range(1,6):
    R(33+i, -420-24*i,420,-420-24*(i-1),484, -16*i,112, 8,8, 90, 0, 8)
L.link(33,'W',420,484,34, tex=20)
for i in range(1,5):
    L.link(33+i,'W',420,484,34+i, tex=8)
R(39, -1000,330,-540,650, -80,40, 8,8, 45, 27, 8)       # cellar (dark: a lantern lights it)
L.link(38,'W',420,484,39, tex=8)
# hiding places: small dark alcoves (tag 50)
R(40, 410,680,440,730, 0,128, 13,23, 25, 50, 23)         # the coat closet (hall)
L.link(11,'N',410,440,40)
R(41, 220,680,270,730, 0,128, 14,23, 25, 50, 23)         # the pantry (kitchen)
L.link(10,'N',220,270,41)
R(42, 700,680,760,730, 0,128, 15,23, 25, 50, 23)         # behind the sofa (living room)
L.link(12,'N',700,760,42)
R(43, 700,1000,760,1050, 128,256, 16,23, 25, 50, 23)     # the wardrobe (bedroom)
L.link(22,'N',700,760,43)
R(44, 220,1000,280,1050, 128,256, 17,23, 25, 50, 23)     # behind the shower curtain (bathroom)
L.link(23,'N',220,280,44)
R(45, -1050,400,-1000,460, -80,40, 8,23, 20, 50, 23)     # between the barrels (cellar)
L.link(39,'W',400,460,45)
R(46, 640,1250,700,1310, 256,384, 9,23, 20, 50, 23)      # the trunk (attic)
L.link(32,'E',1250,1310,46)
# furniture blocks (holes in rooms): the tall cupboard with the crystal on top, the chair platform, the bed, the kitchen table, the fridge
R(50, 620,570,700,640, 100,128, 26,26, 200, 0, 26)        # the cupboard: its top (a floor at 100) is out of reach
L.inner(12,50,tex=26)
R(51, 640,516,680,556, 0,200, 26,4, 200, 40, 26)          # the chair platform (the floor rises to 56 when a chair is placed)
L.inner(12,51,tex=26)
R(52, 660,900,780,960, 30,128, 16,5, 195, 0, 26)          # the bed (a wooden frame, a blue blanket on top)
L.inner(22,52,tex=26)
R(53, 250,430,330,500, 36,128, 26,3, 205, 0, 26)          # the kitchen table
L.inner(10,53,tex=26)
out=sys.argv[1] if len(sys.argv)>1 else '/dev/stdout'
open(out,'w').write(L.emit())
