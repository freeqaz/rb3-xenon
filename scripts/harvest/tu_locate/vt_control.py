"""Positive control: for classes whose own TU <Class>.cpp is pinned in
splits.txt, measure precision of ALL vtable slots vs CLASS-OWNED slots only."""
import json, sys, statistics, os
from _paths import SCRATCH, REPO, BANDEXE, WII_SRC  # noqa: E402
from vt_hier import VT, primary, owned_slots, bases
import vt_splits as SP
from vt_pe import func_of

units = set(u for _, _, u in SP.RANGES)


def stem_unit(cls):
    base = cls.split('@')[0]
    u = base + '.cpp'
    return u if u in units else None


rows = []
for cls in sorted(VT):
    u = stem_unit(cls)
    if not u:
        continue
    r = owned_slots(cls)
    if not r:
        continue
    slots, own, pb, nb = r
    if len(slots) < 2:
        continue
    rngs = SP.unit_ranges(u)

    def inpin(va):
        return any(s <= va < e for s, e, _ in rngs)

    allhit = sum(1 for v in slots if inpin(v))
    ownhit = sum(1 for i in own if inpin(slots[i]))
    rows.append(dict(cls=cls, unit=u, nslots=len(slots), nown=len(own),
                     base=pb, all_hit=allhit, own_hit=ownhit,
                     all_p=allhit / len(slots),
                     own_p=(ownhit / len(own)) if own else None,
                     pin_lo=min(s for s, e, _ in rngs),
                     pin_hi=max(e for s, e, _ in rngs),
                     own_vas=[slots[i] for i in own]))

rows.sort(key=lambda r: -r['nown'])
json.dump(rows, open(SCRATCH+'/control.json', 'w'), indent=1)

print(f'{len(rows)} classes with a same-named pinned TU\n')
print(f'{"class":34s} {"slots":>5s} {"own":>4s} {"all%":>6s} {"own%":>6s}  base')
for r in rows:
    op = f'{r["own_p"]*100:5.0f}%' if r['own_p'] is not None else '   -- '
    print(f'{r["cls"]:34s} {r["nslots"]:5d} {r["nown"]:4d} {r["all_p"]*100:5.0f}% {op}  {r["base"]}')

A = [r['all_p'] for r in rows]
O = [r['own_p'] for r in rows if r['own_p'] is not None]
print(f'\nALL-SLOTS  precision: mean {statistics.mean(A)*100:.1f}%  median {statistics.median(A)*100:.1f}%  n={len(A)}')
print(f'OWNED-ONLY precision: mean {statistics.mean(O)*100:.1f}%  median {statistics.median(O)*100:.1f}%  n={len(O)}')
