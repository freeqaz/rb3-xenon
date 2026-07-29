"""Per-class evidence: class-owned vtable slots + vtable-materialisation sites
(ctors/dtors) -> pdata function boundaries -> cluster -> candidate .text span,
with splits.txt pin attribution."""
import json, sys, statistics, collections
from _paths import SCRATCH, REPO, BANDEXE, WII_SRC  # noqa: E402
from vt_hier import VT, primary, owned_slots, bases
import vt_splits as SP
from vt_pe import func_of
import vt_constidx as constidx

GAP = 0x6000


def fstart(va):
    f = func_of(va)
    return f[0] if f else va


def evidence(cls):
    r = owned_slots(cls)
    if not r:
        return None
    slots, own, pb, nb = r
    rec = primary(cls)
    ev = collections.OrderedDict()  # start_va -> set(tags)
    for i in own:
        ev.setdefault(fstart(slots[i]), set()).add('vt%d' % i)
    # ctor/dtor sites: any code that materialises ANY of the class's vtables
    ctor = []
    for rr in VT.get(cls, []):
        for s in constidx.sites(rr['vt']):
            ctor.append(fstart(s))
    for s in set(ctor):
        ev.setdefault(s, set()).add('ctor')
    return dict(slots=slots, own=own, base=pb, nbase=nb, ev=ev,
                nctor=len(set(ctor)))


def cluster(vas, gap=GAP):
    vas = sorted(vas)
    if not vas:
        return []
    cl = [[vas[0]]]
    for v in vas[1:]:
        if v - cl[-1][-1] <= gap:
            cl[-1].append(v)
        else:
            cl.append([v])
    cl.sort(key=lambda c: (-len(c), c[0]))
    return cl


def report(cls):
    e = evidence(cls)
    if not e:
        return None
    vas = list(e['ev'])
    cl = cluster(vas)
    if not cl:
        return None
    top = cl[0]
    owners = collections.Counter(SP.owner(v) for v in top)
    all_owners = collections.Counter(SP.owner(v) for v in vas)
    return dict(cls=cls, nslots=len(e['slots']), nown=len(e['own']),
                base=e['base'], nctor=e['nctor'],
                nev=len(vas), ncl=len(cl), top_n=len(top),
                lo=top[0], hi=top[-1], span=top[-1] - top[0],
                owners=owners, all_owners=all_owners, ev=e['ev'], clusters=cl)


if __name__ == '__main__':
    for c in sys.argv[1:]:
        r = report(c)
        if not r:
            print(c, 'NO VTABLE')
            continue
        print(f"{c}: slots={r['nslots']} own={r['nown']} base={r['base']} "
              f"ctorsites={r['nctor']} evidence-fns={r['nev']} clusters={r['ncl']}")
        print(f"  top cluster {r['top_n']} fns  {r['lo']:08X}..{r['hi']:08X} "
              f"(span {r['span']:#x})  owners={dict(r['owners'])}")
        for va, tags in sorted(r['ev'].items()):
            print(f"    {va:08X}  {SP.owner(va)}  {sorted(tags)}")
