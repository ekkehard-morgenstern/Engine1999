# The town round Deqq's House II (map and design)

Drawn first, built second (owner's advice, 2026-11-13). Coordinates are the level's: x east, y north; one unit is about a centimetre
(a person is 56 high, a storey 112). North is up.

```
                      north fields: two rows of yards (S-facing houses)          y 1800 .. 2400
        +------------------------------------------------------------------+
        |  yard  yard  yard  yard  yard  yard  yard      (house 1-2 storeys) |
        |  yard  yard  yard  yard  yard  yard                              |
        +------------------------------------------------------------------+
   north road (160 wide)                                                        y 1580 .. 1740
  +--+==================================================================+--+
  |W |  hedge wall (the block's inner side)                              | E|
  |e |      Deqq's premises (fenced): garden, house, shed, well, crash   | a|
  |s |      site; the cellar lies under the west fields                 | s|
  |t |            [house 2 storeys + attic]   [fence = masked railing]  | t|
  |  |                                                                   |  |
  |r |  west fields: yards   <--  block x -440 .. 1530, y -60 .. 1580    |  | east fields:
  |o |  (x -1280 .. -640;                                                |ro| yards (x 1730 ..
  |a |   a gap in front of the cellar)                                   |ad| 2370), houses face
  |d |                                                                   |  | west
  +--+==================================================================+--+
        pavement + railing + GATE (x 440 .. 520)        (booth: NE corner, see below)
   south road (main street, 160 wide)                                           y -220 .. -60
   +-----------+-----------+-----------+-----------+ ... +---------------------------+
   |  BAKERY   |  GROCER   |   CAFE    | PHARMACY  |     |   GAS STATION             |
   |  224 high |           |           |           |     |   forecourt, kiosk, 3 pumps|
   |  x -560.. | -280..    | 0..256    | 280..536  |     |   canopy pillars, price sign|
   +-----------+-----------+-----------+-----------+     |   x 600 .. 1100            |
                                                         +---------------------------+
   two more rows of yards: houses with a fenced garden each, facing the street
```

- **Main street** = the south road in front of Deqq's gate: shops on the far side (two storeys, no yard, 256 wide, their own picture
  with a sign), then the gas station (forecourt of asphalt, kiosk, pumps, four canopy pillars, price sign on a pole), then yards.
- **Yards** (lots of 320 x 280): grass, a fence all round (masked railing, you see through it), a house 128 wide, 100 deep,
  one or two storeys (112 / 224 high, four colors), pushed to the street side (small front yard, deep back yard), a door, one or two trees.
  Deqq's house is the odd one out on purpose: two storeys and an attic, behind a brick-posted fence and a hedge.
- **Open country** behind the rows: grass out to a far backdrop (a tree line); no walls near the road.
- **Phone booth**: at (1730,1660) in the north-east corner beside the road (the far end of the town from the gate, about 2000 units); the game starts next to it, the player at (1730,1570) facing it (when Deqq throws you out you reappear at the gate, (480,-35), as before): the player is an investigator
  who has just called the chief and walks up to the gate (a banner says so).
- Things of the town: the generator writes them after the sectors (`DATA n,x,y,texture,width,height,feet`, ended by `DATA -1`): doors, trees, pumps, the booth.
  Lamps and the verge trees are placed by `TOWNTHINGS` in the game.
- Numbers: sectors 400..417 the ring road, 418..421 pavement / railing / gate, 422..425 the four field strips, 430.. forecourt, lots, houses.
- Generator: `demos/levels/deqq2.bas` (`YARD`, `SHOP`, `BLDG`, `ADDTH`); art: `demos/lib/deqqart.lib` (`DA_TOWN`, `DA_SKYLINE`).
