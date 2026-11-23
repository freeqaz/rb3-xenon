#!/usr/bin/env python3
# scorediff.py a.log b.log: first frames where p0's score/hit/miss differ between two det runs
import re,sys
pat=re.compile(r'rb3-app-det: frame (\d+) .*p0 track \d+ en \d+ auto \d energy ([\d.]+) deploy (\d) long (\d+) score (\d+) hit (\d+) miss (\d+)')
def load(f):
    d={}
    for l in open(f,errors='replace'):
        m=pat.search(l)
        if m: d[int(m.group(1))]=m.groups()[1:]
    return d
a,b=load(sys.argv[1]),load(sys.argv[2])
n=0
for fr in sorted(set(a)&set(b)):
    if a[fr][3:]!=b[fr][3:] or a[fr][1]!=b[fr][1]:
        print(fr,'A',a[fr],'B',b[fr]); n+=1
        if n>=int(sys.argv[3] if len(sys.argv)>3 else 6): break
print('common frames',len(set(a)&set(b)),'diffs shown',n)
