#!/usr/bin/env python3
# summ.py <run.log>...: overdrive summary of a deploywalk det log -- per player: deploys, max
# multiplier, players saved, failed-deploy flag, score/hit/miss at the last game_screen frame;
# band: max bonus level, max bmult, band score; frames spent at each bonus level.
import re, sys, collections
for path in sys.argv[1:]:
    last=None; bonus=collections.Counter(); maxm=collections.defaultdict(int); ad=0
    for line in open(path, errors='replace'):
        if line.startswith('rb3-app-det: autodeploy'): ad+=1; continue
        if not line.startswith('rb3-app-det: frame') or 'screen game_screen' not in line or ' p0 ' not in line: continue
        last=line
        m=re.search(r'bonus (\d+)/', line)
        if m: bonus[int(m.group(1))]+=1
        for pm in re.finditer(r' (p\d) track .*? mult (\d+) ', line):
            pass
        for seg in re.split(r'(?= p\d track )', line)[1:]:
            p=seg.split()[0]; mm=re.search(r' mult (\d+) \(', seg)
            if mm: maxm[p]=max(maxm[p], int(mm.group(1)))
    print('==', path, 'autodeploy-lines', ad)
    if not last: print('  no game_screen frame'); continue
    print('  last game frame', re.search(r'frame (\d+)', last).group(1), 'band:', re.search(r'band acc \d+ stars \S+ mult \d+ score (\d+)', last).group(1),
          re.search(r'bmult \S+', last).group(0) if 'bmult' in last else '', 'frames at bonus level', dict(sorted(bonus.items())))
    for seg in re.split(r'(?= p\d track )', last)[1:]:
        g=lambda k: (re.search(k, seg).group(1) if re.search(k, seg) else '?')
        print('  %s track %s en %s deploys %s od %s uni %s saved %s faildep %s score %s hit %s miss %s energy %s maxmult %d' % (
            seg.split()[0], g(r'track (\d+)'), g(r' en (\d+)'), g(r'deploys (\d+)'), g(r' od (\S+)'), g(r'uni (\S+)'), g(r'saved (\d+)'),
            g(r'faildep (\d+)'), g(r'mult \d+ \S+ ind \d+ faildep \d+ saved \d+ score (\d+)'), g(r' hit (\d+)'), g(r' miss (\d+)'), g(r'energy (\S+)'), maxm[seg.split()[0]]))
