from _paths import SCRATCH, REPO, BANDEXE, WII_SRC  # noqa: E402
import json, os, sys, bisect
import str_locate as locate
from collections import defaultdict

AUD = json.load(open(SCRATCH+'/audit.json'))
AUD.sort(key=lambda r: -r['wii_fns'])
GAP = int(__import__("os").environ.get("GAP","0x400"),16)

UNITRANGES = defaultdict(list)
for a, b, u in locate.CLAIMS:
    UNITRANGES[u].append((a, b))


def containing(va):
    i = bisect.bisect_right(locate.CSTART, va) - 1
    if i < 0:
        return None
    a, b, u = locate.CLAIMS[i]
    if a <= va < b:
        return (a, b, u)
    return None


out = []
for r in AUD:
    paths = locate.wii_lits(r['canon'])
    if not paths:
        continue
    lits = locate.lits_of(paths)
    sel, fnhits = locate.locate(lits, 8)
    if not sel:
        continue
    cl = locate.cluster(fnhits, GAP)
    for c in cl:
        if len(c['lits']) < 3:
            continue
        ct = containing(c['lo'])
        out.append({
            'stem': r['stem'], 'canon': r['canon'], 'wii_fns': r['wii_fns'],
            'lo': c['lo'], 'hi': c['hi'], 'nfn': c['nfn'], 'corr': len(c['lits']),
            'lits': c['lits'],
            'unclaimed_fns': c['unclaimed_fns'],
            'claim': None if not ct else {'lo': ct[0], 'hi': ct[1], 'unit': ct[2],
                                          'size': ct[1] - ct[0],
                                          'unit_total': sum(b - a for a, b in UNITRANGES[ct[2]]),
                                          'unit_nranges': len(UNITRANGES[ct[2]])},
        })
out.sort(key=lambda d: (-d['corr'], -d['wii_fns']))
json.dump(out, open(SCRATCH+'/final.json', 'w'), indent=1)
for d in out:
    c = d['claim']
    tag = 'UNCLAIMED' if not c else ('claimed-by %s [%08X,%08X) sz=%#x unit_tot=%#x nrng=%d'
                                     % (c['unit'], c['lo'], c['hi'], c['size'], c['unit_total'], c['unit_nranges']))
    print('%-30s wfns=%3d span=[%08X,%08X) sz=%#-7x nfn=%d corr=%2d  %s' % (
        d['stem'], d['wii_fns'], d['lo'], d['hi'], d['hi'] - d['lo'], d['nfn'], d['corr'], tag))
    print('     lits: %s' % (d['lits'][:14],))
print(f'\n{len(out)} clusters with >=3 corroborating selective literals')
