#!/usr/bin/env python3
"""CH2 (replacement): MASKED SHARED-PREFIX length, retail body at A vs our body.

Directly tests the framing "shared prefix then divergence".  Masking is
POSITION-WISE from OUR relocation set, applied symmetrically to both sides, so
the linked retail value at a relocated field is never compared.
  * REL24  (branch): only bits 2..25 are masked -- the 6-bit opcode and AA/LK
    survive, per the documented trap that zeroing the whole word makes bl==b.
  * 16-bit hi/lo: only the low halfword is masked.
  * 32-bit absolute: whole word (no opcode there).

NULL: the same prefix computed against RANDOM retail .pdata function starts.
Prologues are stereotyped, so a few words of agreement is CHANCE; the null
prices that.
"""
import bisect, json, os, random, struct, sys
from collections import Counter
ROOT = "/home/free/tmp/laneCO4/wt"
for p in ("tools", "tools/maprow_dtor", "tools/extent_census"):
    sys.path.insert(0, os.path.join(ROOT, p))
import va_size, coffx
from retail_reader import Image

img = Image(os.path.join(ROOT, "orig/45410914/band.exe"))
data, base, secs = va_size.load(os.path.join(ROOT, va_size.PE))
starts = va_size.pdata_starts(data, secs)

# PE PPC reloc types (winnt.h IMAGE_REL_PPC_*)
REL24 = {0x0006}                       # REL24
HALF  = {0x0004, 0x0005, 0x0017, 0x0010, 0x0011}  # ADDR16*/REFHI/REFLO variants
ABS32 = {0x0002, 0x0003}               # ADDR32/ADDR32NB

_cache = {}
def load(p):
    if p not in _cache: _cache[p] = coffx.load(os.path.join(ROOT, p))
    return _cache[p]

def our_fn(objpath, name, size):
    L = load(objpath)
    if not L: return None
    secs_, syms = L
    bysec = {s.index: s for s in secs_}
    c = [s for s in syms if s.name == name and s.sec > 0 and s.kind == coffx.K_FUNCTION
         and bysec.get(s.sec) and bysec[s.sec].code]
    if not c: return None
    sy = min(c, key=lambda s: abs(s.size - size))
    sc = bysec[sy.sec]
    off = sy.addr - sc.addr
    body = sc.data[off:off + sy.size]
    masks = {}
    for (rva, si, typ) in sc.relocs:
        if sy.addr <= rva < sy.addr + sy.size:
            w = (rva - sy.addr) // 4
            if typ in REL24: masks[w] = 0xFC000003          # keep opcode+AA/LK
            elif typ in HALF: masks[w] = 0xFFFF0000
            else: masks[w] = 0x00000000
    return body, masks

def prefix(ourbody, masks, va, limit):
    """(prefix_words, unmasked_words_in_prefix)"""
    n = min(len(ourbody)//4, limit//4)
    rb = img.body(va, n*4)
    if not rb or len(rb) < n*4: return (0, 0)
    pw = un = 0
    for i in range(n):
        m = masks.get(i, 0xFFFFFFFF)
        a = struct.unpack_from(">I", ourbody, i*4)[0] & m
        b = struct.unpack_from(">I", rb, i*4)[0] & m
        if a != b: break
        pw += 1
        if m == 0xFFFFFFFF: un += 1
    return (pw, un)

adj = json.load(open(os.path.expanduser("~/tmp/laneCO4/adj.json")))
cfg = json.load(open(os.path.join(ROOT, "objdiff.json")))
paths = {u["name"]: (u.get("target_path"), u.get("base_path")) for u in cfg["units"]}
random.seed(404)

out = []
nulls_all = []
for r in adj:
    if r.get("addr") is None:
        out.append({**r, "pfx": None}); continue
    A = int(r["addr"], 16)
    _, bp = paths[r["unit"]]
    got = our_fn(bp, r["name"], r["base"])
    if not got:
        out.append({**r, "pfx": None}); continue
    body, masks = got
    lim = r.get("pdata_size") or r["tgt"]
    pw, un = prefix(body, masks, A, lim)
    # NULL: 8 random retail function starts, same our-body, same mask
    nl = []
    for _ in range(8):
        va = random.choice(starts)
        nl.append(prefix(body, masks, va, lim)[0])
    nulls_all.extend(nl)
    out.append({**r, "pfx": pw, "pfx_unmasked": un, "null_max": max(nl),
                "null_mean": sum(nl)/len(nl)})

json.dump(out, open(os.path.expanduser("~/tmp/laneCO4/adj2.json"), "w"), indent=1)
have = [r for r in out if r.get("pfx") is not None]
print(f"rows with prefix computed: {len(have)}/{len(out)}")
print("NULL prefix-word distribution over", len(nulls_all), "draws:",
      dict(sorted(Counter(nulls_all).items())[:8]),
      f" mean={sum(nulls_all)/max(1,len(nulls_all)):.2f} max={max(nulls_all) if nulls_all else 0}")
print("OBSERVED prefix words:", dict(sorted(Counter(r['pfx'] for r in have).items())))
strong = [r for r in have if r["pfx"] >= 8 and r["pfx_unmasked"] >= 4]
print(f"STRONG (>=8 words prefix AND >=4 unmasked): {len(strong)}")
