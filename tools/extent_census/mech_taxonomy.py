#!/usr/bin/env python3
"""Lane DB-2: NAME-FREE mechanism taxonomy.

v1 was VACUOUS: it compared callee NAMES across the dtk-split target obj
(fn_8XXXXXXX / $M labels / section relocs) and our compiled obj (MSVC mangled)
-- two disjoint namespaces, so 78% fell into "identity differs" by construction.

v2 uses only the `bl` COUNT, which is namespace-independent, plus a one-sided
memcpy test (our obj HAS real names; we do not need the target's).

  delta = base(ours) - tgt(retail).   delta>0 => OUR body LONGER.
  dbl   = base_bl   - tgt_bl.         dbl>0   => WE call more.

POSITIVE CONTROL: zero-delta at-100 rows must show dbl==0 (identical bodies).
"""
import json, os, sys, struct
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coffx
ROOT = os.environ.get("RB3_ROOT", os.getcwd())

_c = {}
def load(p):
    if p not in _c: _c[p] = coffx.load(os.path.join(ROOT, p))
    return _c[p]

def index(p):
    k = ("i", p)
    if k not in _c:
        L = load(p); idx = defaultdict(list)
        if L:
            secs, syms = L; bysec = {s.index: s for s in secs}
            for sy in syms:
                if sy.sec > 0 and sy.kind == coffx.K_FUNCTION:
                    sc = bysec.get(sy.sec)
                    if sc is not None and sc.code and not coffx.is_hidden(sy.name):
                        idx[sy.name].append((sy, sc))
            for v in idx.values(): v.sort(key=lambda t: t[0].size)
        _c[k] = idx
    return _c[k]

def names(p):
    """raw-COFF-index -> symbol name.

    ★ LANE DC-3 FIX.  This used to return a POSITIONAL list
    (`[s.name for s in L[1]]`), but coffx compacts the symbol table: it skips
    auxiliary records and drops C_FILE symbols entirely, while a relocation's
    SymbolTableIndex is the RAW index.  Measured skew on MasterAudio.obj:
    2561/2563 symbols had raw != positional.  Every relocation therefore
    resolved to the WRONG symbol, which made the `our_memcpy` probe -- and so
    the whole of class A -- silently vacuous: it reported 0 of 1,211 when the
    true count is 39.  A decisive-looking negative produced by a lookup bug.
    Only the memcpy probe was affected; the E/F/G/C/D split is computed from
    `bl` COUNTS off raw instruction words and never consults a symbol name.
    """
    k = ("n", p)
    if k not in _c:
        L = load(p); _c[k] = {s.raw: s.name for s in L[1]} if L else {}
    return _c[k]

MEM = ("memcpy", "memset", "memmove")
def scan(p, sy, sc):
    off = sy.addr - sc.addr
    body = sc.data[off:off + sy.size]
    nm = names(p)
    rel = {va: si for (va, si, ty) in sc.relocs}
    nbl = 0; mem = 0
    for i in range(0, len(body) - 3, 4):
        w = struct.unpack_from(">I", body, i)[0]
        if (w & 0xFC000003) == 0x48000001:
            nbl += 1
            si = rel.get(sy.addr + i)
            if si is not None:
                b = nm.get(si, "").lstrip("_?")
                if any(b.startswith(m) for m in MEM): mem += 1
    return nbl, mem

def pick(p, n, sz):
    l = index(p).get(n) or []
    for x in l:
        if x[0].size == sz: return x
    return l[0] if l else (None, None)

def mp(r): return 0.0 if r.get("mpn") is None else float(r["mpn"])

# ---------------- POSITIVE CONTROL on zero-delta at-100 rows ----------------
allrows = json.load(open(sys.argv[1]))
ctrl = [r for r in allrows if r["delta"] == 0 and mp(r) >= 99.9999][:400]
bad = 0; n = 0
for r in ctrl:
    t = pick(r["target_path"], r["name"], r["tgt"]); b = pick(r["base_path"], r["name"], r["base"])
    if t[0] is None or b[0] is None: continue
    n += 1
    if scan(r["target_path"], *t)[0] != scan(r["base_path"], *b)[0]: bad += 1
print(f"POSITIVE CONTROL (zero-delta at-100): {n-bad}/{n} agree on bl count "
      f"-> {'PASS' if bad==0 else 'FAIL ('+str(bad)+' disagree)'}")
if bad:
    print("  bl counter is unreliable; taxonomy below is NOT trustworthy")

# ---------------- taxonomy on the genuine population ----------------
rows = [r for r in allrows if r["delta"] != 0]
out = []
for r in rows:
    t = pick(r["target_path"], r["name"], r["tgt"]); b = pick(r["base_path"], r["name"], r["base"])
    if t[0] is None or b[0] is None: continue
    tbl, _tm = scan(r["target_path"], *t)
    bbl, bmem = scan(r["base_path"], *b)
    out.append({**r, "tgt_bl": tbl, "base_bl": bbl, "dbl": bbl - tbl, "our_memcpy": bmem})

def mech(r):
    d, x = r["delta"], r["dbl"]
    if r["our_memcpy"] and d < 0:
        return "A_we_call_memcpy_retail_inlines_copy"   # CU-3 / gate-1 class
    if x > 0 and d < 0:  return "C_retail_INLINED_our_callee"
    if x < 0 and d > 0:  return "D_we_INLINED_retail_callee"
    if x == 0 and d != 0: return "E_same_call_count_codegen_differs"
    if x > 0 and d > 0:  return "F_we_have_EXTRA_work(calls+code)"
    if x < 0 and d < 0:  return "G_we_are_MISSING_work(calls+code)"
    return "H_other"

for r in out: r["mech"] = mech(r)

print(f"\nclassified {len(out)} genuine rows")
print("\n=== MECHANISM TAXONOMY (name-free) ===")
T = len(out)
for m, k in Counter(r["mech"] for r in out).most_common():
    g = [x for x in out if x["mech"] == m]
    s = sorted(mp(x) for x in g)
    hi = sum(1 for x in g if mp(x) >= 90)
    print(f"  {m:40s} {k:5d} ({100.0*k/T:5.1f}%)  med_mpn={s[len(s)//2]:6.2f}  n>=90mpn={hi}")

print("\n=== A: we call memcpy AND are shorter (gate-1 one-line class) ===")
for r in sorted((x for x in out if x["mech"].startswith("A_")), key=lambda x: -mp(x)):
    print(f"  {mp(r):6.2f} d={r['delta']:+5d} dbl={r['dbl']:+3d} {r['unit'][:30]:30s} {r['name'][:70]}")

print("\n=== G: we are MISSING work -- fewer calls AND shorter (missing code path) ===")
g = [x for x in out if x["mech"].startswith("G_")]
for r in sorted(g, key=lambda x: -mp(x))[:25]:
    print(f"  {mp(r):6.2f} d={r['delta']:+5d} dbl={r['dbl']:+3d} {r['unit'][:30]:30s} {r['name'][:70]}")

print("\n=== C: retail inlined our callee -- closest to done ===")
for r in sorted((x for x in out if x["mech"].startswith("C_")), key=lambda x: -mp(x))[:20]:
    print(f"  {mp(r):6.2f} d={r['delta']:+5d} dbl={r['dbl']:+3d} {r['unit'][:30]:30s} {r['name'][:70]}")

json.dump(out, open(sys.argv[2], "w"))
print(f"\nwrote -> {sys.argv[2]}")
