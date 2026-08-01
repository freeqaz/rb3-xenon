#!/usr/bin/env python3
"""Split the CHARGED rows (E/F/G) into 'name is wrong' vs 'we don't compile it yet'.

v2 fixes two defects in v1:
  * F rows were counted twice (once from detail.json, once recomputed) -> the
    denominator was 441 instead of the census's 269.
  * core() only matched ?Method@Class@@; free functions (?Foo@@YA...) and
    templates (??$Foo@...) fell into a 185-row UNPARSED hole.

Qualified name = everything before the FIRST '@@' (MSVC: ?name@scope..@@<sig>).

⚠ CALIBRATION OF CLAIMS: a same-name-different-form hit is a CANDIDATE defect,
not a proven one -- an adjustor thunk legitimately coexists with its non-adjustor
form, and our class may simply lack the virtual base that would emit one.  The
'necessary not sufficient' warning cuts both ways, so these are reported as
'needs adjudication', not as repairs.
"""
import json, os, re, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cj4_coff as coff, cj4_norm as norm

WT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', '..')
cfg = json.load(open(os.path.join(WT, 'objdiff.json')))
unit_base = {u['name']: u.get('base_path') for u in cfg['units']}
m = json.load(open(os.path.join(WT, 'scripts/target_symbol_map.json')))
rows = {k: v for k, v in m.items() if k.lower().startswith('0x') and isinstance(v, str)}

cache = {}
def state(unit):
    if unit not in cache:
        p = unit_base.get(unit)
        # ⚠ MUST key by norm.key -- objdiff normalizes MSVC anon-namespace names
        # ignoring the unique id, so a raw-name lookup MISSES every ?A0x.... row
        # and over-charges it as PORTING (CI-3's 124/369 phantom-flag class).
        cache[unit] = ({norm.key(n): st for n, st in
                        coff.classify(open(os.path.join(WT, p), 'rb').read()).items()}
                       if p and os.path.exists(os.path.join(WT, p)) else None)
    return cache[unit]

tgt_def = collections.defaultdict(list)
for u in cfg['units']:
    p = u.get('target_path')
    fp = os.path.join(WT, p) if p else None
    if fp and os.path.exists(fp):
        for n, st in coff.classify(open(fp, 'rb').read()).items():
            if st == 'DEFINED':
                tgt_def[norm.key(n)].append(u['name'])

base_def_anywhere = collections.defaultdict(list)
for u in cfg['units']:
    s = state(u['name'])
    if s:
        for n, st in s.items():
            if st == 'DEFINED':
                base_def_anywhere[n].append(u['name'])


def qual(name):
    """Fully-qualified name = text before the first '@@'."""
    i = name.find('@@')
    return name[:i] if i > 0 else name


def scope(name):
    """Innermost enclosing scope (class) of a qualified name, or None."""
    q = qual(name).lstrip('?')
    if q.startswith('$'):
        q = q.split('@', 1)[-1]
    parts = q.split('@')
    return parts[1] if len(parts) > 1 else None


charged = {}          # addr -> (name, cls, home)   DEDUPED by addr
for addr, name in rows.items():
    nk = norm.key(name)
    homes = tgt_def.get(nk, [])
    if len(homes) != 1:
        continue
    home = homes[0]
    s = state(home)
    if s is None:
        continue                       # unpinned / auto region
    st = s.get(nk)
    if st == 'DEFINED':
        continue                       # class C, not charged
    c = {'WEAK': 'D_weak_external', 'UNDEF': 'E_undefined_ref',
         'COMMON': 'E_undefined_ref'}.get(
             st, 'F_defined_elsewhere' if base_def_anywhere.get(nk) else 'G_absent_everywhere')
    charged[addr] = (name, c, home)

print(f'charged rows (deduped by address): {len(charged)}')
print(collections.Counter(v[1] for v in charged.values()))

split = collections.Counter()
bycls = collections.defaultdict(collections.Counter)
ex = collections.defaultdict(list)
for addr, (name, c, home) in charged.items():
    s = state(home)
    defined = [x for x, st in s.items() if st == 'DEFINED']
    q, sc = qual(name), scope(name)
    if c == 'F_defined_elsewhere':
        v = 'MAP/LAYOUT: defined in ANOTHER unit (do NOT move -- CH-3 measured -4)'
    elif c == 'D_weak_external':
        v = 'MAP DEFECT: weak external, DEFINED form exists (repairable)'
    elif any(x.startswith(q + '@@') for x in defined):
        v = 'NEEDS ADJUDICATION: same qualified name, different signature/form'
    elif sc and any(('@%s@@' % sc) in x or ('@%s@' % sc) in x or
                    x.startswith('?%s@' % sc) for x in defined):
        v = 'PORTING: class present in our obj, this member absent'
    else:
        v = 'PORTING: class absent from our obj entirely'
    split[v] += 1
    bycls[c][v] += 1
    if len(ex[v]) < 5:
        ex[v].append((addr, name[:78], home))

tot = len(charged)
print(f'\n=== CHARGED ROWS: name-wrong vs not-ported  (denominator {tot}) ===')
for k, v in sorted(split.items(), key=lambda x: -x[1]):
    print(f'  {v:5d}  ({100*v/tot:5.1f}%)  {k}')
print('\nby census class:')
for c in sorted(bycls):
    print(f'  {c}: {dict(bycls[c])}')
print()
for k in sorted(ex):
    print(f'-- {k}')
    for e in ex[k]:
        print('     ', e[0], e[1], '|', e[2])
json.dump({a: list(v) for a, v in charged.items()},
          open(os.environ.get('CJ4_CHARGED','/home/free/tmp/laneCJ4/charged.json'), 'w'), indent=1)
