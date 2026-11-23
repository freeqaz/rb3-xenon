#!/usr/bin/env python3
# harm.py <run.log>...: harmony summary of a harmwalk det log -- per vocal part the max phrase
# cap / score seen, frames each singer was assigned each part, and the last game_screen line's
# vocal fields (harmony stats, part percentages, od/uni, score); FAIL lines.
import re, sys, collections
for path in sys.argv[1:]:
    mx={}; asg=collections.Counter(); last=None; fails=collections.Counter(); parts=[]
    for l in open(path, errors='replace'):
        if l.startswith('FAIL') or 'UNREACHED' in l or 'runtime error' in l: fails[l.strip()[:150]]+=1
        if l.startswith('rb3-app-det: vocal part'): parts.append(l.strip()[13:])
        if not l.startswith('rb3-app-det: frame') or ' vp0 ' not in l: continue
        for m in re.finditer(r' vp(\d) ph (-?\d+) sc ([\d.]+)/([\d.]+)', l):
            k=int(m.group(1)); c=mx.setdefault(k,[0,0]); c[0]=max(c[0],float(m.group(4))); c[1]=max(c[1],float(m.group(3)))
        for m in re.finditer(r' s(\d) ap (-?\d+) as (-?\d+)', l):
            if m.group(3)!='-1': asg['s%s->%s'%(m.group(1),m.group(3))]+=1
        if 'screen game_screen' in l: last=l
    print('==', path)
    for p in parts: print('  ', p)
    print('  max cap/score per part', mx, ' assigned frames', dict(asg))
    if last:
        m=re.search(r' p0 track 3 .*?(?= p\d track |$)', last)
        print('  last', re.search(r'frame \d+', last).group(0), (m.group(0) if m else '')[:700])
        b=re.search(r'band acc .*?(?= p0 )', last); print('  band', b.group(0)[:300] if b else '')
    for k,v in fails.most_common(8): print('  x%d %s'%(v,k))
