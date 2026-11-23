#!/usr/bin/env python3
"""hits.py run.log: p0's (hit, miss, score) at every frame where any of them changes (first song)."""
import re, sys
prev = None
for ln in open(sys.argv[1], errors='replace'):
    if not ln.startswith('rb3-app-det: frame') or ' p0 track' not in ln: continue
    fr = int(ln.split()[2]); p0 = ln[ln.index(' p0 track'):]
    m = re.search(r'score (\d+) hit (\d+) miss (\d+)', p0)
    cur = m.groups()
    if cur != prev:
        print(fr, *cur)
        prev = cur
