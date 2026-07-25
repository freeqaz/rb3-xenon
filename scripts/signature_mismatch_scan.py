#!/usr/bin/env python3
"""Whole-tree scan for MANGLED-NAME DIVERGENCE.

A retail map entry and one of our compiled symbols can be the SAME function
(same qualified name) yet differ in the mangled suffix, because our declaration
has the wrong access / const / virtual / static-ness / return type / params.
objdiff pairs by name, so such a function reads 0% no matter how good the body.

Classes emitted:
  ACCESS-ONLY : signature after the access char is identical -> 1-char fix
                (public<->protected<->private, +/- virtual, +/- const)
  SIG-DIFF    : same qualified name, different return/params
Only reported when the map name is NOT already defined by us, and we have
exactly ONE defined symbol sharing that qualified name (no overload ambiguity).
"""
import glob, json, os, re, struct, sys, bisect
from collections import defaultdict
from pathlib import Path

ROOT = Path(os.environ.get("RESIDUE_ROOT", "/home/free/tmp/wt-residue"))

def parse_defined(data):
    if len(data) < 20: return []
    so = struct.unpack_from("<I", data, 8)[0]; n = struct.unpack_from("<I", data, 12)[0]
    if not so or not n: return []
    st = so + n * 18; out = []; i = 0
    while i < n:
        eo = so + i * 18
        if eo + 18 > len(data): break
        nb = data[eo:eo+8]
        if nb[:4] == b"\x00\x00\x00\x00":
            o = struct.unpack_from("<I", nb, 4)[0]; ao = st + o
            try: name = data[ao:data.index(b"\x00", ao)].decode("ascii", "replace")
            except ValueError: name = ""
        else: name = nb.split(b"\x00")[0].decode("ascii", "replace")
        secn = struct.unpack_from("<h", data, eo+12)[0]; aux = data[eo+17]
        if secn > 0: out.append(name)
        i += 1 + aux
    return out

sym_obj = {}
defined = set()
for f in glob.glob(str(ROOT/"build/45410914/src/**/*.obj"), recursive=True):
    try: d = parse_defined(open(f, "rb").read())
    except Exception: continue
    for s in d:
        defined.add(s); sym_obj.setdefault(s, f)

ANON = re.compile(r'\?A0x[0-9a-f]+')
def norm(n): return ANON.sub('?A0xANON', n)
ndefined = {norm(x) for x in defined}

ACCESS = {
 'A':'private','B':'private const','C':'private static','E':'private virtual',
 'F':'private virtual const','I':'protected','J':'protected const',
 'K':'protected static','M':'protected virtual','N':'protected virtual const',
 'Q':'public','R':'public const','S':'public static','U':'public virtual',
 'V':'public virtual const','Y':'free function',
}

def split_sym(s):
    """-> (qualified-name-including-trailing-@@, access-char, signature)"""
    if not s.startswith('?') or s.startswith('??$') or '?$' in s: return None
    i = s.find('@@')
    if i < 0: return None
    qual = s[:i+2]; rest = s[i+2:]
    if not rest: return None
    return qual, rest[0], rest[1:]

# index OUR defined symbols by qualified name
ours = defaultdict(list)
for s in defined:
    p = split_sym(s)
    if p: ours[norm(p[0])].append((s, p[1], p[2]))

# splits ranges (to know whether the map entry is pinned = objdiff would pair it)
txt = (ROOT/"config/45410914/splits.txt").read_text()
cur = None; unit_ranges = []
for line in txt.splitlines():
    if line and not line[0].isspace() and line.rstrip().endswith(":") and line.rstrip()[:-1].endswith((".cpp", ".c")):
        cur = line.rstrip()[:-1]; continue
    mt = re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", line)
    if mt and cur: unit_ranges.append((int(mt.group(1),16), int(mt.group(2),16), cur))
unit_ranges.sort(); starts=[r[0] for r in unit_ranges]
def unit_of(a):
    i = bisect.bisect_right(starts, a) - 1
    if i >= 0 and unit_ranges[i][0] <= a < unit_ranges[i][1]: return unit_ranges[i][2]
    return None

raw = json.loads((ROOT/"scripts/target_symbol_map.json").read_text())
access_only, sig_diff = [], []
for k, v in raw.items():
    if not k.lower().startswith("0x"): continue
    try: a = int(k.lower().removeprefix("0x"), 16)
    except ValueError: continue
    if a >= 0x82800000: continue
    if norm(v) in ndefined: continue          # we already emit this exact name
    p = split_sym(v)
    if not p: continue
    q, acc, sig = p
    cands = ours.get(norm(q), [])
    if len(cands) != 1: continue               # overload ambiguity -> skip
    ds, dacc, dsig = cands[0]
    u = unit_of(a)
    rec = {"addr": f"0x{a:08X}", "unit": u, "target": v, "ours": ds,
           "t_access": ACCESS.get(acc, acc), "o_access": ACCESS.get(dacc, dacc),
           "obj": os.path.relpath(sym_obj.get(ds, ""), ROOT)}
    if sig == dsig: access_only.append(rec)
    else:
        rec["t_sig"] = sig; rec["o_sig"] = dsig
        sig_diff.append(rec)

print(f"ACCESS-ONLY (1-char decl fix): {len(access_only)}")
print(f"SIG-DIFF   (ret/params differ): {len(sig_diff)}")
json.dump({"access_only": access_only, "sig_diff": sig_diff},
          open(ROOT/"scripts/_sig_mismatch.json", "w"), indent=1)

byunit = defaultdict(lambda: [0,0])
for r in access_only: byunit[r["unit"]][0] += 1
for r in sig_diff:    byunit[r["unit"]][1] += 1
print(f"\n{'unit':52} {'acc':>4} {'sig':>4}")
for u, (x, y) in sorted(byunit.items(), key=lambda z: -(z[1][0]*3+z[1][1]))[:40]:
    print(f"{str(u)[:52]:52} {x:>4} {y:>4}")
