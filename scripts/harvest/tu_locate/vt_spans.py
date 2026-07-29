import json, sys, bisect, collections
from _paths import SCRATCH, REPO, BANDEXE  # noqa: E402
import vt_splits as SP
from vt_pe import FUNCS, FSTARTS, TEXT_LO, TEXT_HI
from vt_analyze import report
from vt_hier import VT

# merged claimed intervals
merged = []
for s, e, u in sorted(SP.RANGES):
    if merged and s <= merged[-1][1]:
        merged[-1][1] = max(merged[-1][1], e)
    else:
        merged.append([s, e])
mstarts = [m[0] for m in merged]


def hole(va):
    """maximal unclaimed interval containing va, or None if claimed"""
    i = bisect.bisect_right(mstarts, va) - 1
    if i >= 0 and merged[i][0] <= va < merged[i][1]:
        return None
    lo = merged[i][1] if i >= 0 else TEXT_LO
    hi = merged[i + 1][0] if i + 1 < len(merged) else TEXT_HI
    return (lo, hi)


def snap(lo, hi):
    """expand [lo,hi] to whole pdata functions"""
    i = bisect.bisect_right(FSTARTS, lo) - 1
    if i >= 0 and FUNCS[i][0] <= lo < FUNCS[i][0] + FUNCS[i][1]:
        lo = FUNCS[i][0]
    j = bisect.bisect_right(FSTARTS, hi) - 1
    if j >= 0:
        hi = max(hi, FUNCS[j][0] + FUNCS[j][1])
    return lo, hi


def candidate(cls):
    r = report(cls)
    if not r:
        return None
    out = []
    for cl in r['clusters']:
        lo, hi = snap(cl[0], cl[-1])
        h = hole(cl[0])
        owners = collections.Counter(SP.owner(v) for v in cl)
        out.append(dict(n=len(cl), lo=lo, hi=hi, hole=h,
                        owners={str(k): v for k, v in owners.items()}))
    return dict(cls=cls, clusters=out, ev=r['ev'], nown=r['nown'],
                nctor=r['nctor'], nslots=r['nslots'], base=r['base'])


if __name__ == '__main__':
    print('merged claimed intervals:', len(merged),
          'total claimed bytes: %#x' % sum(e - s for s, e in merged),
          'of .text %#x' % (TEXT_HI - TEXT_LO))
    for c in sys.argv[1:]:
        d = candidate(c)
        print(json.dumps(d, indent=1, default=str))
