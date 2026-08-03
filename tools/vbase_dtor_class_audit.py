"""Refined ??_D sweep: 'installs a BASE of the claimed class' is the ICF
fold-alias signature (a secondary vtable folds with the base's own), NOT a
mispair. Only 'installs a class that is NEITHER the claim NOR a base of it'
is a candidate -- and even then K must discriminate (lane DL-3 / DK-1)."""
import json, re, sys
sys.path.insert(0, 'tools')
from retail_rtti import RetailRtti

R = RetailRtti()
m = json.load(open('scripts/target_symbol_map.json'))
rep = json.load(open('build/45410914/report.json'))
size_of, unit_of, pct_of = {}, {}, {}
for u in rep['units']:
    for f in u.get('functions') or []:
        n = f.get('name')
        if n:
            size_of[n] = int(f.get('size', 0) or 0)
            unit_of[n] = u['name']; pct_of[n] = f.get('match_percent_normalized', 0.0)

_bases = {}
def bases_of(cls):
    """all TypeDescriptor names appearing in ANY COL hierarchy of cls (incl. itself)"""
    if cls in _bases: return _bases[cls]
    out = set()
    try:
        for col_va, chd, bcds in R.hierarchy_of_class(cls):
            for b in bcds:
                out.add(b.name)
    except Exception:
        pass
    _bases[cls] = out
    return out

rows = sorted((k, v) for k, v in m.items()
              if re.fullmatch(r'0x[0-9a-fA-F]{8}', k) and v.startswith('??_D'))
CONFIRM = FOLD = CAND = NOVT = 0
cands = []
for k, sym in rows:
    mm = re.fullmatch(r'\?\?_D(.+?)@@QAAXXZ', sym)
    if not mm: continue
    cls, sz = mm.group(1), size_of.get(sym, 0)
    if not sz: continue
    inst = sorted({c for _, c in R.classes_installed_by(int(k, 16), sz)})
    if not inst: NOVT += 1; continue
    want = '.?AV%s@@' % cls
    if want in inst:
        CONFIRM += 1; continue
    bs = bases_of(cls)
    if all(c in bs for c in inst):
        FOLD += 1; continue                     # base-of-claim => ICF fold-alias
    CAND += 1
    cands.append((k, sym, cls, inst, sorted(c for c in inst if c not in bs),
                  sz, unit_of.get(sym), pct_of.get(sym)))

print(f"??_D rows={len(rows)}  CONFIRM={CONFIRM}  ICF_FOLD_ALIAS(base-of-claim)={FOLD}  "
      f"CANDIDATE={CAND}  no-vtable-store={NOVT}")
print()
for k, sym, cls, inst, foreign, sz, un, pc in cands:
    print(f"  {k} size={sz:4d} mpn={pc if pc is not None else -1:7.3f} unit={un}")
    print(f"      claims {cls!r}; installs {inst}")
    print(f"      NOT a base of the claim: {foreign}")
