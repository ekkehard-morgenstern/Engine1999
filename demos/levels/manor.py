from mkw3d import Level
L=Level()
# textures: 1 brick 2 plaster sand 3 plaster azure 4 stone 5 hall tiles 6 carpet 7 planks 8 grass 9 sky 10 door 11 window 12 grate 14 stone sand 15 plaster lime 16 hedge stone 30 water
R=L.room
R(0, 0,0,640,320, 0,128, 8,-1, 255, 10, 1)            # garden
R(1, 288,320,352,340, 0,0, 5,2, 190, 1, 2)             # front door (closed: ceiling = floor)
R(2, 192,340,448,540, 0,128, 5,2, 215, 11, 2)          # hall
R(3, 448,340,704,540, 0,128, 6,2, 195, 12, 3)          # living room
R(4, -64,340,192,540, 0,128, 7,2, 190, 13, 15)         # study
L.link(0,'N',288,352,1, tex=10)
L.link(1,'N',288,352,2, tex=10)
L.link(2,'E',380,500,3)
L.link(2,'W',360,420,4)
L.link(2,'W',440,504,4, tex=11, flags=3, yoff=-32)
# stairs up to the gallery
R(5, 288,540,352,564, 16,136, 14,2, 185, 0, 14)
for i in range(2,9):
    R(4+i, 288,540+24*(i-1),352,540+24*i, 16*i,120+16*i, 14,2, 185, 0, 14)
R(13, 160,732,480,952, 128,240, 7,2, 205, 14, 2)
L.link(2,'N',288,352,5, tex=14)
for i in range(1,8):
    L.link(4+i,'N',288,352,5+i, tex=14)
L.link(12,'N',288,352,13, tex=14)
L.pillar(2, 220,380,236,396, 4); L.pillar(2, 404,380,420,396, 4)
L.pillar(2, 220,484,236,500, 4); L.pillar(2, 404,484,420,500, 4)
# the cupola in the living room (a higher ceiling), a pool in the garden (a step down of 32, too much to walk into)
R(14, 540,400,620,480, 0,256, 6,2, 215, 12, 3)
L.inner(3,14)
R(15, 440,60,580,200, -32,128, 30,-1, 230, 16, 16)
L.inner(0,15)
L.pillar(0, 100,120,124,144, 4)
# the cellar: a door in the study's west wall, six steps down
R(16, -84,400,-64,464, 0,0, 4,4, 120, 5, 4)
L.link(4,'W',400,464,16, tex=10)
for i in range(1,7):
    R(16+i, -84-24*i,400,-84-24*(i-1),464, -16*i,112, 4,4, 90, 0, 4)
L.link(16,'W',400,464,17, tex=10)
for i in range(1,6):
    L.link(16+i,'W',400,464,17+i, tex=4)
R(23, -484,330,-228,550, -96,40, 4,4, 70, 15, 4)
L.link(22,'W',400,464,23, tex=4)
L.pillar(23, -370,420,-346,444, 4)
import sys; open(sys.argv[1] if len(sys.argv) > 1 else '/dev/stdout','w').write(L.emit())

