#!/usr/bin/env python3
"""summ.py run.log...: per run, p0's vocal score at the end of each song (the last record line
before the screen leaves the game), plus sung-vs-detected pitch stats from rb3-app-mic lines."""
import re, sys, math
for path in sys.argv[1:]:
    songs, last, in_game = [], None, False
    mic = []
    for ln in open(path, errors='replace'):
        if ln.startswith('rb3-app-mic: ms'):
            m = re.search(r'sung ([-\d.]+) det ([-\d.]+) energy ([-\d.]+)', ln)
            mic.append(tuple(map(float, m.groups())))
            continue
        if not ln.startswith('rb3-app-det: frame'): continue
        g = ' p0 track' in ln
        if g:
            last = ln
        elif last is not None:
            songs.append(last); last = None
    if last: songs.append(last + ' (cut)')
    print(f'== {path}')
    for s in songs:
        fr = s.split()[2]
        p0 = s[s.index(' p0 track'):]
        m = re.search(r'score (\d+) hit (\d+) miss (\d+) streak (\d+) stars ([\d.]+) crowd ([\d.]+)', p0)
        print(f'  song ending frame {fr}: score {m[1]} hit {m[2]} miss {m[3]} streak {m[4]} stars {m[5]} crowd {m[6]}' + (' (cut)' if 'cut' in s else ''))
    if mic:
        both = [(s, d) for s, d, e in mic if s > 0 and d > 0]
        sung = [x for x in mic if x[0] > 0]
        errs = []
        for s, d in both:
            e = d - s
            fold = e - 12 * round(e / 12)
            errs.append((e, fold))
        print(f'  mic windows {len(mic)}: sung {len(sung)}, sung&detected {len(both)}, sung-but-silent {len(sung)-len(both)}')
        if errs:
            ae = sorted(abs(e) for e, f in errs); af = sorted(abs(f) for e, f in errs)
            w = lambda a, t: sum(1 for x in a if x <= t) / len(a)
            print(f'  |det-sung| median {ae[len(ae)//2]:.3f}; within 0.5 st {w(ae,.5):.3f}; octave-folded within 0.5 st {w(af,.5):.3f}; mean signed {sum(e for e,f in errs)/len(errs):.3f}')
