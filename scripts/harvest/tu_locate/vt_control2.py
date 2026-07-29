"""Positive + negative control for the owned-slot / ctor-site instrument."""
import json, sys, statistics, collections
from _paths import SCRATCH, REPO, BANDEXE  # noqa: E402
from vt_hier import VT, primary, owned_slots
import vt_splits as SP
from vt_analyze import evidence, cluster, fstart, report
import vt_constidx as constidx

units = set(u for _, _, u in SP.RANGES)
rows = []
for cls in sorted(VT):
    u = cls.split('@')[0] + '.cpp'
    if u not in units:
        continue
    e = evidence(cls)
    if not e:
        continue
    slots, own = e['slots'], e['own']
    if len(slots) < 2:
        continue
    degenerate = (e['base'] is None)       # no base -> owned == all, no filter
    allv = [fstart(v) for v in slots]
    ownv = [fstart(slots[i]) for i in own]
    ctorv = [v for v, t in e['ev'].items() if 'ctor' in t]
    evv = list(e['ev'])

    def p(vs):
        if not vs:
            return None
        return sum(1 for v in vs if SP.owner(v) == u) / len(vs)

    r = report(cls)
    top_ok = SP.owner(r['lo']) == u or r['owners'].most_common(1)[0][0] == u
    rows.append(dict(cls=cls, unit=u, degenerate=degenerate,
                     nslots=len(slots), nown=len(own), nctor=len(ctorv),
                     p_all=p(allv), p_own=p(ownv), p_ctor=p(ctorv),
                     p_ev=p(evv), top_ok=top_ok,
                     top_purity=r['owners'].most_common(1)[0][1] / r['top_n'],
                     top_owner=r['owners'].most_common(1)[0][0],
                     span=r['span'], ncl=r['ncl'], top_n=r['top_n']))

json.dump(rows, open(SCRATCH+'/control2.json', 'w'), indent=1)


def summarize(sel, label):
    s = [r for r in rows if sel(r)]
    if not s:
        print(label, 'n=0')
        return
    def m(k):
        v = [r[k] for r in s if r[k] is not None]
        return (statistics.mean(v) * 100, statistics.median(v) * 100, len(v))
    a = m('p_all'); o = m('p_own'); c = m('p_ctor'); ev = m('p_ev')
    ok = sum(1 for r in s if r['top_ok']) / len(s) * 100
    print(f'{label}  n={len(s)}')
    print(f'   ALL slots      mean {a[0]:5.1f}%  median {a[1]:5.1f}%')
    print(f'   OWNED slots    mean {o[0]:5.1f}%  median {o[1]:5.1f}%')
    print(f'   CTOR sites     mean {c[0]:5.1f}%  median {c[1]:5.1f}%  (n={c[2]})')
    print(f'   OWNED+CTOR     mean {ev[0]:5.1f}%  median {ev[1]:5.1f}%')
    print(f'   top-cluster majority owner == true unit: {ok:.1f}%')


summarize(lambda r: True, 'ALL control classes')
summarize(lambda r: not r['degenerate'], 'NON-DEGENERATE (has a base class -> filter is informative)')
summarize(lambda r: not r['degenerate'] and r['nown'] <= r['nslots'] * 0.6,
          'STRICT (base exists AND owned <= 60% of slots)')
