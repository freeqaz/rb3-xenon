import json, sys, collections, statistics, random
from _paths import SCRATCH, REPO, BANDEXE  # noqa: E402
from vt_hier import VT, owned_slots
import vt_splits as SP
from vt_analyze import evidence, cluster, report, fstart

AUD = json.load(open(SCRATCH+'/audit.json'))
units = set(u for _, _, u in SP.RANGES)

# ---------- NEGATIVE CONTROL ----------
print('=== NEGATIVE CONTROL ===')
# (a) classes with zero owned slots AND zero ctor sites -> must decline
declined = []
zero_own = []
for cls in sorted(VT):
    e = evidence(cls)
    if not e:
        continue
    if not e['own'] and not e['nctor']:
        declined.append(cls)
    elif not e['own']:
        zero_own.append(cls)
print(f'classes with 0 owned slots AND 0 ctor sites (instrument emits nothing): {len(declined)}')
print('  e.g.', declined[:12])
print(f'classes with 0 owned slots but >=1 ctor site: {len(zero_own)}')

# (b) label permutation on the control set
ctrl = [c for c in sorted(VT) if c.split("@")[0] + '.cpp' in units]
random.seed(7)
perm = []
for c in ctrl:
    e = evidence(c)
    if not e:
        continue
    wrong = random.choice(ctrl).split('@')[0] + '.cpp'
    vs = list(e['ev'])
    if not vs:
        continue
    perm.append(sum(1 for v in vs if SP.owner(v) == wrong) / len(vs))
print(f'label-permutation precision (evidence vs a RANDOM pinned unit): '
      f'mean {statistics.mean(perm)*100:.2f}%  max {max(perm)*100:.1f}%  n={len(perm)}')

# ---------- HUNT ----------
print('\n=== HUNT: audit rows with rtti==true ===')
rows = []
for a in AUD:
    if not a['rtti']:
        continue
    cls = a['stem']
    if cls not in VT:
        rows.append(dict(a, status='NO_VTABLE'))
        continue
    r = report(cls)
    if r is None:
        rows.append(dict(a, status='NO_VTABLE'))
        continue
    own_ev = [v for v, t in r['ev'].items() if any(x.startswith('vt') for x in t)]
    ctor_ev = [v for v, t in r['ev'].items() if 'ctor' in t]
    if not own_ev and not ctor_ev:
        rows.append(dict(a, status='DECLINED'))
        continue
    top = r['clusters'][0]
    own_cnt = collections.Counter(SP.owner(v) for v in top)
    all_cnt = collections.Counter(SP.owner(v) for v in r['ev'])
    rows.append(dict(a, status='OK', nslots=r['nslots'], nown=r['nown'],
                     base=r['base'], nctor=r['nctor'], nev=r['nev'],
                     ncl=r['ncl'], top_n=r['top_n'],
                     lo=r['lo'], hi=r['hi'], span=r['span'],
                     top_owners={str(k): v for k, v in own_cnt.items()},
                     all_owners={str(k): v for k, v in all_cnt.items()},
                     ev={('%08X' % v): sorted(t) for v, t in sorted(r['ev'].items())},
                     ev_owner={('%08X' % v): str(SP.owner(v)) for v in sorted(r['ev'])},
                     clusters=[['%08X' % x for x in c] for c in r['clusters']]))

json.dump(rows, open(SCRATCH+'/hunt.json', 'w'), indent=1)

ok = [r for r in rows if r['status'] == 'OK']
ok.sort(key=lambda r: -r['wii_fns'])
print(f'{len(ok)} of {sum(1 for a in AUD if a["rtti"])} rtti-true rows produced evidence\n')
hdr = f'{"stem":34s} {"wf":>4s} {"sl":>3s} {"ow":>3s} {"ct":>3s} {"ev":>3s} {"cl":>2s}  span                    pin'
print(hdr)
for r in ok:
    top = r['top_owners']
    maj, n = max(top.items(), key=lambda kv: kv[1])
    pin = 'UNCLAIMED' if maj == 'None' else maj
    frac = f'{n}/{r["top_n"]}'
    print(f'{r["stem"]:34s} {r["wii_fns"]:4d} {r["nslots"]:3d} {r["nown"]:3d} '
          f'{r["nctor"]:3d} {r["nev"]:3d} {r["ncl"]:2d}  '
          f'{r["lo"]:08X}..{r["hi"]:08X} {r["span"]:>7x}  {pin} {frac}')
