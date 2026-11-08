#!/usr/bin/env python3
# mkw3d.py: a level made of rectangular sectors -> DATA lines for the W3D statements (used for the demos; the output is pasted into the program)
#   DATA nsectors
#   DATA s,floor,ceil,floortex,ceiltex,light,tag,nwalls
#   DATA x1,y1,x2,y2,tex,next,flags,upper,lower,xoff,yoff        (one line per wall)
class Room:
    def __init__(s, id, x1, y1, x2, y2, f, c, ft, ct, light, tag, wt):
        s.id=id; s.x1=x1; s.y1=y1; s.x2=x2; s.y2=y2; s.f=f; s.c=c; s.ft=ft; s.ct=ct; s.light=light; s.tag=tag; s.wt=wt
        s.links={'S':[],'E':[],'N':[],'W':[]}; s.loops=[]
class Level:
    def __init__(s): s.rooms={}
    def room(s, *a):
        r=Room(*a); s.rooms[r.id]=r; return r
    # a portal along the side of room a (S = y1 edge, N = y2 edge, W = x1 edge, E = x2 edge), span lo..hi along the side, to room b (whose opposite side must touch it)
    def link(s, a, side, lo, hi, b, tex=None, up=None, low=None, flags=0, xoff=0, yoff=0, both=True):
        A=s.rooms[a]; B=s.rooms[b]; opp={'S':'N','N':'S','E':'W','W':'E'}[side]
        ck={'S':(A.y1,B.y2),'N':(A.y2,B.y1),'W':(A.x1,B.x2),'E':(A.x2,B.x1)}[side]
        assert ck[0]==ck[1], ('rooms do not touch', a, side, b, ck)
        rng={'S':(A.x1,A.x2,B.x1,B.x2),'N':(A.x1,A.x2,B.x1,B.x2),'W':(A.y1,A.y2,B.y1,B.y2),'E':(A.y1,A.y2,B.y1,B.y2)}[side]
        assert rng[0]<=lo<hi<=rng[1] and rng[2]<=lo<hi<=rng[3], ('span outside', a, side, lo, hi, b)
        d=dict(tex=tex,up=up,low=low,flags=flags,xoff=xoff,yoff=yoff)
        A.links[side].append((lo,hi,b,d))
        if both: B.links[opp].append((lo,hi,a,dict(d)))
    def pillar(s, a, x1, y1, x2, y2, tex=None):
        s.rooms[a].loops.append(('solid', x1,y1,x2,y2,None,tex))
    def inner(s, a, b, tex=None):      # room b lies inside room a (a hole in a): all four sides of b are portals (tex: the picture of the walls seen from room a)
        B=s.rooms[b]
        s.rooms[a].loops.append(('portal', B.x1,B.y1,B.x2,B.y2,b,tex))
        for side,(lo,hi) in {'S':(B.x1,B.x2),'N':(B.x1,B.x2),'W':(B.y1,B.y2),'E':(B.y1,B.y2)}.items():
            B.links[side].append((lo,hi,a,dict(tex=None,up=None,low=None,flags=0,xoff=0,yoff=0)))
    def wall(s, r, x1,y1,x2,y2, tex, nxt, d=None):
        d=d or {}
        t=d.get('tex'); t = r.wt if t is None else t
        up=d.get('up'); up=-1 if up is None else up
        lo=d.get('low'); lo=-1 if lo is None else lo
        return (x1,y1,x2,y2,t,nxt,d.get('flags',0),up,lo,d.get('xoff',0),d.get('yoff',0))
    def walls(s, r):
        out=[]
        def side(name, fixed, a0, a1, rev, horiz):
            # segments along the side from a0 to a1 (ascending), split by the links
            L=sorted(r.links[name], key=lambda l:l[0]); pos=a0; segs=[]
            for lo,hi,b,d in L:
                assert lo>=pos, ('links overlap', r.id, name, lo, pos)
                if lo>pos: segs.append((pos,lo,None,None))
                segs.append((lo,hi,b,d)); pos=hi
            if pos<a1: segs.append((pos,a1,None,None))
            if rev: segs=segs[::-1]
            for lo,hi,b,d in segs:
                p,q=(hi,lo) if rev else (lo,hi)
                if horiz: out.append(s.wall(r,p,fixed,q,fixed,r.wt if b is None else None,-1 if b is None else b,d))
                else: out.append(s.wall(r,fixed,p,fixed,q,r.wt if b is None else None,-1 if b is None else b,d))
        side('S', r.y1, r.x1, r.x2, False, True)
        side('E', r.x2, r.y1, r.y2, False, False)
        side('N', r.y2, r.x1, r.x2, True, True)
        side('W', r.x1, r.y1, r.y2, True, False)
        for kind,x1,y1,x2,y2,b,tex in r.loops:      # clockwise
            t = r.wt if tex is None else tex
            nx = -1 if b is None else b
            out.append((x1,y1,x1,y2,t,nx,0,-1,-1,0,0)); out.append((x1,y2,x2,y2,t,nx,0,-1,-1,0,0))
            out.append((x2,y2,x2,y1,t,nx,0,-1,-1,0,0)); out.append((x2,y1,x1,y1,t,nx,0,-1,-1,0,0))
        return out
    def emit(s):
        ids=sorted(s.rooms); lines=['DATA %d' % len(ids)]
        for i in ids:
            r=s.rooms[i]; w=s.walls(r)
            lines.append('DATA %d,%g,%g,%d,%d,%d,%d,%d' % (r.id,r.f,r.c,r.ft,r.ct,r.light,r.tag,len(w)))
            for t in w: lines.append('DATA ' + ','.join('%g'%v if isinstance(v,float) else str(v) for v in t))
        return '\n'.join(lines)+'\n'
