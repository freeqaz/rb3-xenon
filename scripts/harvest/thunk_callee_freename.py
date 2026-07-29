#!/usr/bin/env python3
"""thunk_callee_freename -- for every retail adjustor thunk whose OWN name is
already in the target symbol map, read the callee VA out of its machine code and
name it from our obj's relocation (compiled_callee[thunk_name]).

No heuristic: the thunk's mangled name is a total function of (callee prefix,
vtordisp, this-adjust), so the compiled thunk with that exact name relocates to
exactly the virtual the retail thunk branches to.

Classes:
  MAPPED_OK   VA already carries that name
  MAPPED_BAD  VA carries a different name          (map conflict, do not touch)
  FREE        VA unmapped and the name is unused   <- pure +1 candidates
  TAKEN       VA unmapped but the name is already on another VA
Read-only; emits JSON.
"""
import sys, json, struct, re, collections
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts" / "harvest"))
from size_order_automap import _ordered_funcs, _asm_target_funcs
from thunk_callee_bodydiff import shape, load_units, paths, coff_callees

BUILD = ROOT / "build" / "45410914"
raw = json.load(open(ROOT / "scripts" / "target_symbol_map.json"))
MAP = {int(k, 16): v for k, v in raw.items()
       if k.lower().startswith("0x") and isinstance(v, str)}
BYNAME = collections.defaultdict(list)
for k, v in MAP.items():
    BYNAME[v].append(k)

res = []
st = collections.Counter()
for u in sorted(load_units()):
    asm, bobj = paths(u)
    if not (asm.exists() and bobj.exists()):
        continue
    try:
        tf = [(va, sz, mk) for va, sz, mk in _asm_target_funcs(asm) if va]
        bf = _ordered_funcs(bobj)
        callees = coff_callees(bobj)
    except Exception:
        continue
    byname = {f['name']: f for f in bf}
    tgt_body = {va: mk for va, sz, mk in tf}
    for va, sz, mk in tf:
        s = shape(va, sz)
        if not s:
            continue
        tname = MAP.get(va)
        if not tname:
            st['thunk_unmapped'] += 1
            continue
        cl = [c for c in set(callees.get(tname, [])) if c.startswith('?') or c.startswith('_')]
        cl = [c for c in cl if c != tname]
        if len(cl) != 1:
            st['reloc_ambiguous_%d' % min(len(cl), 2)] += 1
            continue
        cname = cl[0]
        cva = s[2]
        cur = MAP.get(cva)
        if cur == cname:
            cls = 'MAPPED_OK'
        elif cur is not None:
            cls = 'MAPPED_BAD'
        elif cname in BYNAME:
            cls = 'TAKEN'
        else:
            cls = 'FREE'
        st[cls] += 1
        if cls in ('FREE', 'MAPPED_BAD', 'TAKEN'):
            same = (cname in byname and cva in tgt_body
                    and byname[cname]['masked'] == tgt_body[cva])
            res.append(dict(unit=u, thunk_va="0x%08x" % va, thunk=tname,
                            callee_va="0x%08x" % cva, name=cname, cls=cls,
                            cur=cur, in_unit=(cva in tgt_body),
                            have_sym=(cname in byname),
                            tgt_size=len(tgt_body.get(cva, b'')),
                            base_size=byname[cname]['size'] if cname in byname else None,
                            byte_same=same))

print(dict(st))
out = Path.home() / "tmp" / "bodyport_freename.json"
json.dump(res, open(out, "w"), indent=1)
print("wrote", out, len(res))
free = [r for r in res if r['cls'] == 'FREE' and r['in_unit'] and r['have_sym']]
free.sort(key=lambda r: abs((r['tgt_size'] or 0) - (r['base_size'] or 0)))
print("FREE + in-unit + we have the symbol:", len(free))
seen = set()
for r in free:
    if r['callee_va'] in seen:
        continue
    seen.add(r['callee_va'])
    d = (r['tgt_size'] or 0) - (r['base_size'] or 0)
    print(f"  {r['callee_va']} d={d:+6d} tgt={r['tgt_size']:6d} same={int(r['byte_same'])} "
          f"{r['unit'][:34]:34s} {r['name'][:78]}")
