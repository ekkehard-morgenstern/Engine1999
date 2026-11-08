from mkw3d import Level
L=Level()
R=L.room
# textures: 1 metal gray, 2 metal azure, 3 brick gray, 4 floor tiles, 5 carpet blue, 6 hazard metal (red), 7 stone, 8 red door, 9 blue door, 12 lift (yellow metal), 13 exit stone
R(0, 0,0,256,256, 0,128, 4,1, 190, 1, 1)            # start room
R(1, 96,256,160,512, 0,112, 4,1, 130, 2, 2)          # corridor
R(2, -96,512,352,832, 0,128, 4,1, 210, 3, 3)         # arena
R(3, -352,600,-96,760, 0,128, 5,1, 170, 4, 2)        # west room (red key)
R(4, 352,640,376,700, 0,0, 4,1, 150, 21, 8)          # red door
R(5, 376,560,632,780, 0,128, 4,1, 190, 5, 6)         # east room
R(6, 96,832,160,928, 0,112, 4,1, 130, 2, 2)          # corridor north
R(7, -64,928,320,1152, 0,128, 4,1, 160, 6, 3)        # hall 2
R(8, 96,1152,160,1216, 0,200, 4,1, 200, 30, 12)      # lift (floor moves 0 .. 96)
R(9, -100,1216,360,1400, 96,224, 5,1, 200, 7, 2)     # upper room (blue key)
R(10, 130,1400,190,1424, 96,96, 4,1, 160, 22, 9)     # blue door
R(11, 60,1424,260,1560, 96,224, 4,13, 255, 99, 13)   # exit room
L.link(0,'N',96,160,1)
L.link(1,'N',96,160,2)
L.link(2,'W',640,700,3)
L.link(2,'E',640,700,4, tex=8)
L.link(4,'E',640,700,5, tex=8)
L.link(2,'N',96,160,6)
L.link(6,'N',96,160,7)
L.link(7,'N',96,160,8, tex=12)
L.link(8,'N',96,160,9, tex=12)
L.link(9,'N',130,190,10, tex=9)
L.link(10,'N',130,190,11, tex=9)
for x,y in ((-10,590),(220,590),(-10,760),(220,760)):
    L.pillar(2, x,y,x+30,y+30, 7)
L.pillar(5, 480,640,510,670, 6); L.pillar(5, 480,690,510,720, 6)
L.pillar(7, 40,1000,70,1030, 3); L.pillar(7, 190,1000,220,1030, 3)
L.pillar(9, 30,1280,60,1310, 7); L.pillar(9, 250,1280,280,1310, 7)
import sys; open(sys.argv[1] if len(sys.argv) > 1 else '/dev/stdout','w').write(L.emit())

