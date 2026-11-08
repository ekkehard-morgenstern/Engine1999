#!/usr/bin/env python3
# The tunes of demos/lib/music3d.lib (music strings for PLAY) with a check of their lengths: python3 music3d.py [file]
# writes the BASIC source of the procedure that queues a tune (MU_PLAY) and prints the length of every tune in seconds.
import re, sys
def beats(s):
    """the length of a music string in quarter notes (T and L as in mml.c)"""
    L=4; total=0.0; i=0; s=s.upper().replace(';',' ')
    toks=re.findall(r'ML|MN|MS|[TOLVW]\s*[\d.]+|[<>]|[A-GRP][#+\-]?\s*\d*\.*',s)
    for t in toks:
        t=t.replace(' ','')
        if t[0]=='L': L=int(t[1:]); continue
        if t[0] in 'TOVW' or t in ('ML','MN','MS','<','>'): continue
        m=re.match(r'([A-GRP])([#+\-]?)(\d*)(\.*)',t)
        n=int(m.group(3)) if m.group(3) else L
        b=4.0/n; f=b; d=b
        for _ in m.group(4): d/=2; f+=d
        total+=f
    return total
def tempo(s):
    m=re.search(r'T\s*(\d+)',s.upper()); return int(m.group(1))
tunes={}   # (set, mood) -> [voice strings]
def tune(st,mood,*v): tunes[(st,mood)]=list(v)
# ---------------------------------------------------------------------------------------------------------- Deqq's House II (set 2): D minor, slow and mysterious
BASS="D A D A B- F A E D A G D A E D D"
def bassline(t,w,vol,notes,octv='O2',ln='L2'): return "T%d W%d V%s MN %s %s %s"%(t,w,vol,octv,ln,notes)
calm=[bassline(54,2,'0.5',BASS),
      "T54 W2 V0.2 ML O3 L1 F F D C# F B- C# A",
      "T54 W0 V0.32 O4 L4 A2 G F E2. R4 F2 D F E2 C#2 D F A2 G2 B- A C#2. E D1"]
tune(2,2,*calm)
heart=" ".join(["C16 R16 C16 R16"]*32)
tune(2,3,*calm,"T54 W3 V0.3 ML O1 L16 "+heart)
shimmer=" ".join(x for ch in ["D F A F D F A F","D F A F D F A F","B- D F D B- D F D","A C# E C# A C# E C#","D F A F D F A F","G B- D B- G B- D B-","A C# E C# A C# E C#","D F A F D F A F"] for x in [ch])
tune(2,1,bassline(54,2,'0.5',BASS),"T54 W2 V0.2 ML O3 L1 F F D C# F B- C# A",calm[2],"T54 W0 V0.1 ML O5 L8 "+shimmer)
bars=["DDDDDDDD","DDDDDDDD","B-B-B-B-B-B-B-B-","AAAAAAAA"]*2
db="T150 W1 V0.28 MN O2 L8 "+" ".join(bars)
def arp(root,chord):
    # four sixteenths: root, third, fifth, octave of the chord
    return chord
arpbars=["O4 D F A O5 D","O4 D F A O5 D","O4 B- O5 D F B-","O4 A O5 C# E A"]*2
arps=" ".join(" ".join([b]*4) for b in arpbars)
dsn="T150 W3 V0.22 ML O3 L8 "+" ".join(["R4 C R R4 C R"]*8)
dlead="T150 W1 V0.2 O5 L8 D D F D A4 G4 F E D C# D4 R4 D D F D B-4 A4 E E A E C#4 E4 D D F D A4 G4 F E D C# D4 A4 D D F D B-4 A4 E E A E C#4 R4"
tune(2,4,db,"T150 W1 V0.12 ML L16 "+arps,dsn,dlead)
tune(2,5,"T100 W1 V0.3 O4 L4 E D C# O3 A2 R2 R4","T100 W2 V0.4 O2 L2 A A A R2")
eb="D A A E G D D A G D A E D A D D"
tune(2,6,bassline(72,2,'0.5',eb),"T72 W2 V0.2 ML O3 L1 F# C# B F# B C# F# A","T72 W0 V0.35 O4 L4 F#2 A B C#2 E2 G2 B A F#2. E G2 B2 A2 C#2 F#2 A2 D1")
# ---------------------------------------------------------------------------------------------------------- Bug Hunt (set 1): A minor, a bunker
bb="A A F E A A F E"
tune(1,2,"T66 W2 V0.5 MN O2 L1 "+bb,"T66 W2 V0.2 ML O3 L1 C C A G# C C A G#","T66 W1 V0.11 MN O5 L8 R2 E4 R4 R4 C4 R2 R2 C4 R4 R4 B4 R2 R2 E4 R4 R4 C4 R2 R2 A4 R4 R4 G#4 R2")
tune(1,3,"T66 W2 V0.5 MN O2 L1 "+bb,"T66 W2 V0.2 ML O3 L1 C C A G# C C A G#","T66 W1 V0.11 MN O5 L8 R2 E4 R4 R4 C4 R2 R2 C4 R4 R4 B4 R2 R2 E4 R4 R4 C4 R2 R2 A4 R4 R4 G#4 R2","T66 W3 V0.3 ML O1 L16 "+heart.replace('32','32'))
tune(1,1,"T84 W1 V0.26 MN O2 L4 A A A A A A A A F F F F E E E E A A A A A A A A F F F F E E E E","T84 W2 V0.2 ML O3 L1 C C A G# C C A G#","T84 W1 V0.2 O4 L4 A2 C E B2 A G# A2 C F E2 G# B A2 C E D2 C B C2 A F E1")
bd="T160 W1 V0.28 MN O2 L8 "+" ".join(["AAAAAAAA","AAAAAAAA","FFFFFFFF","EEEEEEEE"]*2)
barps=["O4 A O5 C E A","O4 A O5 C E A","O4 F A O5 C F","O4 E G# B E"]*2
barp=" ".join(" ".join([b]*4) for b in barps)
bsn="T160 W3 V0.22 ML O3 L8 "+" ".join(["R4 C R R4 C R"]*8)
blead="T160 W1 V0.2 O5 L8 A A C A E4 D4 C B A G# A4 R4 F F A F C4 B-4 E E G# E B4 G#4 A A C A E4 D4 C B A G# A4 E4 F F A F C4 B-4 E E G# E B4 R4"
tune(1,4,bd,"T160 W1 V0.12 ML L16 "+barp,bsn,blead)
tune(1,5,"T60 W2 V0.35 O4 L2 A F E1 D1 R1","T60 W2 V0.45 O2 L1 A D E1 R1")
tune(1,6,"T120 W1 V0.3 O4 L8 C E G O5 C4 O4 G E C4 R4 F A O5 C F4 C O4 A4 R4 G B O5 D G4 R4 C1","T120 W2 V0.4 O2 L2 C G F C G C1 R1")
out=[]
secs={}
for (st,mood),v in sorted(tunes.items()):
    bs=[beats(x) for x in v]
    ts=[tempo(x) for x in v]
    assert len(set(ts))==1,(st,mood,ts)
    assert all(abs(b-bs[0])<1e-6 for b in bs),(st,mood,bs)
    secs[(st,mood)]=bs[0]*60.0/ts[0]
    print('set',st,'mood',mood,'beats',bs[0],'seconds %.2f'%secs[(st,mood)],'voices',len(v),'longest string',max(len(x) for x in v))
lines=[]
for (st,mood),v in sorted(tunes.items()):
    lines.append('  IF MU_SET=%d AND MU_CUR=%d THEN PLAY %s'%(st,mood,','.join('"%s"'%x for x in v)))
lens=[]
for (st,mood),sec in sorted(secs.items()):
    lens.append('  IF MU_SET=%d AND M=%d THEN RETURN %.3f'%(st,mood,sec))
txt='PROC MU_PLAY\n'+'\n'.join(lines)+'\nEND PROC\nFUNC MU_LEN(M)\n'+'\n'.join(lens)+'\n  RETURN 10\nEND FUNC\n'
open(sys.argv[1] if len(sys.argv)>1 else '/dev/stdout','w').write(txt)
