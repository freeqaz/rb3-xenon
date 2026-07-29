from _paths import SCRATCH, REPO, BANDEXE  # noqa: E402
"""Positive + negative control for the string-xref locator."""
import os, sys, json, re, bisect
from collections import defaultdict
import str_locate as locate

WII = locate.WII

# index wii source by stem
STEMS = defaultdict(list)
for root, dirs, files in os.walk(WII):
    for f in files:
        b, e = os.path.splitext(f)
        if e == '.cpp':
            STEMS[b.lower()].append(os.path.join(root, f))

# pinned units: stem -> merged claim ranges
UNITS = defaultdict(list)
for a, b, u in locate.CLAIMS:
    UNITS[u].append((a, b))


def true_span(unit):
    r = UNITS[unit]
    return min(x[0] for x in r), max(x[1] for x in r), r


def in_pin(va, unit):
    for a, b in UNITS[unit]:
        if a <= va < b:
            return True
    return False


def run(unit, maxsites=8, minlen=5):
    stem = unit[:-4] if unit.endswith('.cpp') else unit
    paths = STEMS.get(stem.lower())
    if not paths or len(paths) > 1:
        return None
    hdr = paths[0][:-4] + '.h'
    src = [paths[0]] + ([hdr] if os.path.exists(hdr) else [])
    lits = locate.lits_of(src)
    sel, fnhits = locate.locate(lits, maxsites, minlen)
    cl = locate.cluster(fnhits, int(os.environ.get('GAP','0x2000'),16))
    return {'unit': unit, 'src': paths[0], 'nlits': len(lits), 'nsel': len(sel),
            'clusters': cl, 'fnhits': fnhits}


def main():
    cands = []
    for unit in sorted(UNITS):
        r = run(unit)
        if not r:
            continue
        best = r['clusters'][0] if r['clusters'] else None
        ncorr = len(best['lits']) if best else 0
        cands.append((ncorr, unit, r))
    cands.sort(key=lambda x: -x[0])
    picked = [c for c in cands if c[0] >= 3][:20]
    print(f'# pinned units with unique wii .cpp: {len(cands)}; with >=3 corroborating selective lits: {sum(1 for c in cands if c[0]>=3)}')
    ok = 0
    errs = []
    for ncorr, unit, r in picked:
        best = r['clusters'][0]
        ta, tb, ranges = true_span(unit)
        cen = (best['lo'] + best['hi']) // 2
        hit = any(a <= cen < b for a, b in ranges)
        # per-function hit rate over ALL sites
        allfn = sorted(r['fnhits'])
        nin = sum(1 for f in allfn if in_pin(f[0], unit))
        if hit:
            ok += 1
            err = 0
        else:
            err = min(abs(cen - a) if cen < a else (cen - b if cen >= b else 0) for a, b in ranges)
        errs.append(err)
        print(f"{unit:34s} corr={ncorr:3d} pred=[{best['lo']:08X},{best['hi']:08X}) "
              f"true=[{ta:08X},{tb:08X}) centroid_in_pin={hit} err={err:#x} "
              f"fn_in_pin={nin}/{len(allfn)} claims={best['claims'][:3]}")
    errs.sort()
    med = errs[len(errs) // 2] if errs else -1
    print(f'\nPOSITIVE CONTROL: precision={ok}/{len(picked)}={ok/max(1,len(picked)):.2f} median_abs_err={med:#x}')


if __name__ == '__main__':
    main()
