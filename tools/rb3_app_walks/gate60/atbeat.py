#!/usr/bin/env python3
# atbeat.py run.log beat...: (lane walk60) at the first game_screen frame whose song beat >= each
# beat, the band score and each player's score/hit/miss/deploys: a rate-independent song position,
# so a legacy 30 Hz run and a 60 Hz run of the same song compare at the same point of the chart.
import re, sys
fn = sys.argv[1]; beats = [float(x) for x in sys.argv[2:]]; out = {}
for l in open(fn, 'rb'):
    l = l.decode('latin-1')
    if not l.startswith('rb3-app-det: frame') or ' screen game_screen' not in l: continue
    m = re.search(r'frame (\d+) .* beat ([\d.-]+)', l)
    if not m: continue
    fr, b = int(m.group(1)), float(m.group(2))
    for x in beats:
        if x not in out and b >= x:
            band = re.search(r'band acc \d+ stars [\d.]+ mult \d+ score (\d+)', l)
            ps = re.findall(r'(p\d) track (\d+) (.*?)score (\d+) hit (\d+) miss (\d+)', l)
            out[x] = f'frame {fr} beat {b:.2f} band {band.group(1) if band else "?"} ' + ' '.join(
                f'{p}:t{t} s{s} h{h} m{mi}' + (' d' + d.group(1) if (d := re.search(r'deploys (\d+)', mid)) else '')
                + (' E' + e.group(1) if (e := re.search(r'energy ([\d.]+)', mid)) else '')
                for p, t, mid, s, h, mi in ps)
for x in beats: print(x, out.get(x))
