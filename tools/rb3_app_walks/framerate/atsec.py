#!/usr/bin/env python3
# atsec.py run.log sec...: p0's score/hit/miss/energy at the first game frame whose song seconds >= sec
import re,sys
fn=sys.argv[1]; secs=[float(x) for x in sys.argv[2:]]; out={}
for line in open(fn,errors='replace'):
    if not line.startswith('rb3-app-det: frame') or ' screen game_screen' not in line: continue
    f=line.split(); fr=int(f[2]); s=float(f[10])
    m=re.search(r' p0 track .*? energy ([\d.]+) deploy (\d).*? deploys (\d+).*? score (\d+) hit (\d+) miss (\d+) streak (\d+)',line)
    if not m: continue
    for x in secs:
        if x not in out and s>=x and s<x+5: out[x]=(fr,s)+m.groups()
for x in secs: print(x, out.get(x))
