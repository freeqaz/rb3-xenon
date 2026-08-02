#!/usr/bin/env python3
"""CH1-proper: VTABLE SLOT CONSENSUS, map-independent, WITH a base-rate control.

For a virtual ?F@C@@U...:
  k    = index of ?F@C@@ in OUR compiled ??_7C@@6B@   (COFF relocations)
  Aret = retail RTTI vtable for class C, slot k       (band.exe .rdata bytes)
CONFIRM iff Aret == A (the mapped address).  Neither side consults the map.

CONTROLS
  (a) COVERAGE: how many rows' classes even have a retail RTTI vtable.  A miss
      there is NOT evidence against the row.
  (b) BASE RATE on a KNOWN-GOOD population: virtual functions currently at
      mpn==100 in the same units.  9/79 is unreadable without this.
  (c) MIS-NAME / fail-on-demand: deliberately shift the slot index by +1 and
      show confirmation collapses.  If a shifted slot still "confirms", the
      test is vacuous.
"""
import json, os, random, re, sys
from collections import Counter
ROOT = "/home/free/tmp/laneCO4/wt"
for p in ("tools", "tools/maprow_dtor", "tools/maprow_audit", "tools/extent_census"):
    sys.path.insert(0, os.path.join(ROOT, p))
import coffx, struct
from retail_reader import Image
from rtti_vtable_index import Rtti, demangle_class

img = Image(os.path.join(ROOT, "orig/45410914/band.exe"))

def follow(a, depth=0):
    """Retail vtable slots for vbase classes hold the ADJUSTOR THUNK, which tail-
    `b`s to the body.  Not following it made 23.8% of KNOWN-GOOD rows CONTRADICT."""
    if depth > 4: return a
    b = img.body(a, 64)
    if not b: return a
    for i in range(0, len(b) - 3, 4):
        w = struct.unpack_from(">I", b, i)[0]
        if (w >> 26) == 18 and not (w & 1):
            d = w & 0x03FFFFFC
            if d & 0x02000000: d -= 0x04000000
            return follow(a + i + d, depth + 1) if not (w & 2) else d
        if (w >> 26) == 18 and (w & 1): break
        if w == 0x4E800020: break
    return a
rt = Rtti(img); rt.build_attribution()
# class -> list of (vtable_va, col_offset, slots)
byclass = {}
for vt, (nm, coff, sl) in rt.vt_slots.items():
    c = demangle_class(nm)
    byclass.setdefault(c, []).append((vt, coff, sl))

_c = {}
def load(p):
    if p not in _c: _c[p] = coffx.load(os.path.join(ROOT, p))
    return _c[p]

def _slot_match(slotname, target, pre):
    """A vtable slot may hold the VBASE-ADJUSTOR THUNK (?F@C@@$4PPPPPPPM@A@...)
    rather than the direct symbol.  Match prefix ?F@C@@ plus the argument
    suffix (target minus its one-char access specifier)."""
    if slotname == target: return True
    if not slotname.startswith(pre): return False
    rest = slotname[len(pre):]
    return rest.startswith("$") and slotname.endswith(target[len(pre)+1:])

def our_slots(objpath, cls, target=None):
    """-> set of slot indices where `target` (or its thunk) appears, across ALL
    ??_7<cls>@@6B* vtables in our compiled obj."""
    L = load(objpath)
    if not L: return None
    secs_, syms = L
    bysec = {s.index: s for s in secs_}
    pre = f"?{target.split('@')[0][1:]}@{cls}@@" if target else None
    RAW = {sy.raw: sy for sy in syms}
    vts = [s for s in syms if s.sec > 0 and s.name.startswith(f"??_7{cls}@@6B")]
    if not vts: return None
    ks = set(); seen_any = False
    for sy in vts:
        sc = bysec.get(sy.sec)
        if sc is None: continue
        seen_any = True
        for (rva, si, typ) in sc.relocs:
            if sy.addr <= rva < sy.addr + max(sy.size, 4):
                t = RAW.get(si)
                if t is not None and target and _slot_match(t.name, target, pre):
                    ks.add((rva - sy.addr)//4)
    return ks if seen_any else None

def parse_cls(m):
    r = re.match(r"\?[A-Za-z_0-9]+@([A-Za-z_0-9]+)@@", m)
    if r: return r.group(1)
    r = re.match(r"\?\?[01_][A-Za-z]?([A-Za-z_0-9]+)@@", m)
    return r.group(1) if r else None

MAP = {k: v for k, v in json.load(open(os.path.join(ROOT,"scripts/target_symbol_map.json"))).items()
       if k.startswith("0x")}
NAME2A = {}
for a, n in MAP.items(): NAME2A.setdefault(n, []).append(int(a,16))

cfg = json.load(open(os.path.join(ROOT, "objdiff.json")))
paths = {u["name"]: (u.get("target_path"), u.get("base_path")) for u in cfg["units"]}

def test(unit, name, A, shift=0):
    """-> ('CONFIRM'|'CONTRADICT'|'NO_CLASS_VT'|'NOT_IN_OUR_VT'|'NO_OBJ', detail)"""
    cls = parse_cls(name)
    if not cls: return ("NO_CLASS", None)
    _, bp = paths[unit]
    ks = our_slots(bp, cls, name)
    if not ks: return ("NOT_IN_OUR_VT", None)
    ks = {k + shift for k in ks}
    vts = byclass.get(cls)
    if not vts: return ("NO_CLASS_VT", sorted(ks))
    seen = []
    for (vt, coff, sl) in vts:
        for k in ks:
            if k < len(sl):
                seen.append((k, hex(sl[k])))
                if sl[k] == A or follow(sl[k]) == A: return ("CONFIRM", (k, hex(sl[k])))
    return ("CONTRADICT", seen[:4]) if seen else ("NO_CLASS_VT", sorted(ks))

# ---------------- treatment: the 79 rows -------------------------------
adj2 = json.load(open(os.path.expanduser("~/tmp/laneCO4/adj2.json")))
res = []
for r in adj2:
    if r.get("addr") is None:
        res.append({**r, "slot": "NO_ADDR"}); continue
    v, d = test(r["unit"], r["name"], int(r["addr"],16))
    res.append({**r, "slot": v, "slot_detail": str(d)})
print("TREATMENT (79 rows):", Counter(r["slot"] for r in res))

# ---------------- control (b): known-good population --------------------
rep = json.load(open(os.path.join(ROOT, "build/45410914/report.json")))
units79 = {r["unit"] for r in adj2}
good = []
for u in rep["units"]:
    if u["name"] not in units79: continue
    for f in (u.get("functions") or []):
        if f.get("match_percent_normalized") == 100 and re.match(r"\?[A-Za-z_0-9]+@[A-Za-z_0-9]+@@[UM]", f["name"]):
            a = NAME2A.get(f["name"])
            if a and len(a) == 1: good.append((u["name"], f["name"], a[0]))
random.seed(7); random.shuffle(good); good = good[:400]
cg = Counter(test(*g)[0] for g in good)
print(f"CONTROL known-good virtuals at mpn==100 (n={len(good)}):", cg)

# ---------------- control (c): mis-name / slot+1 fail-on-demand ---------
mis = Counter(test(*g, shift=1)[0] for g in good)
print(f"FAIL-ON-DEMAND slot+1 on the SAME known-good rows:", mis)
json.dump(res, open(os.path.expanduser("~/tmp/laneCO4/adj3.json"),"w"), indent=1)
