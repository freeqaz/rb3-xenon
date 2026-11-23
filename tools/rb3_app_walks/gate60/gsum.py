#!/usr/bin/env python3
# gsum.py run.log...: (lane walk60) a det walk's screen sequence (frame each settled screen is
# first current) and, per song (a run of game_screen frames), the last frame's band score/stars
# and each player's score/hit/miss/stars, plus FAIL and wait TIMEOUT counts.
import re, sys
P = re.compile(r'(p\d) track (\d+) .*? score (\d+) hit (\d+) miss (\d+) streak \d+ stars ([\d.]+)')
for fn in sys.argv[1:]:
    seq = []; prev = None; songs = []; ing = False; last = None; fails = 0; tmo = 0; fr = 0
    for l in open(fn, 'rb'):
        l = l.decode('latin-1')
        if 'FAIL' in l and 'Line:' in l: fails += 1
        if 'TIMEOUT' in l: tmo += 1
        if not l.startswith('rb3-app-det: frame '): continue
        m = re.search(r'frame (\d+) .* screen (\S+) transition (\S+)', l)
        if not m: continue
        fr = int(m.group(1)); scr = m.group(2)
        if m.group(3) == 'none' and scr != prev:
            seq.append(f'{scr.replace("_screen", "")}@{fr}'); prev = scr
        g = scr == 'game_screen'
        if g: last = (fr, l)
        if ing and not g and last:
            if songs and last[0] - songs[-1][0] < 300: songs[-1] = last  # a transition blip, same song
            else: songs.append(last)
            last = None
        ing = g
    if ing and last: songs.append(last)
    print(f'== {fn} frames {fr} FAIL {fails} TIMEOUT {tmo}')
    print('  ' + ' > '.join(seq))
    for fr_, l in songs:
        b = re.search(r'band acc \d+ stars [\d.]+ mult \d+ score (\d+) hit \d+ miss \d+ streak \d+ stars ([\d.]+)', l)
        ps = ['%s t%s %s/%s/%s %s' % x for x in P.findall(l.split(' band ')[1] if ' band ' in l else l)]
        print(f'  song end frame {fr_}: band {b.group(1) if b else "?"} stars {b.group(2) if b else "?"} | ' + ' | '.join(ps))
