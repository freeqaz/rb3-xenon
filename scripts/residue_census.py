#!/usr/bin/env python3
"""Residue census: map entries INSIDE pinned .text ranges of WIRED units that
our compiled objs do NOT define. = body-port completion targets."""
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

# map obj file -> defined symbols; also global set
obj_defs = {}
defined = set()
for f in glob.glob(str(ROOT/"build/45410914/src/**/*.obj"), recursive=True):
    try: d = parse_defined(open(f, "rb").read())
    except Exception: continue
    obj_defs[f] = set(d); defined.update(d)

txt = (ROOT/"config/45410914/splits.txt").read_text()
cur = None; unit_ranges = []
for line in txt.splitlines():
    if line and not line[0].isspace() and line.rstrip().endswith(":") and line.rstrip()[:-1].endswith((".cpp", ".c")):
        cur = line.rstrip()[:-1]; continue
    mt = re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", line)
    if mt and cur:
        unit_ranges.append((int(mt.group(1), 16), int(mt.group(2), 16), cur))
unit_ranges.sort()
starts = [r[0] for r in unit_ranges]
def unit_of(a):
    i = bisect.bisect_right(starts, a) - 1
    if i >= 0 and unit_ranges[i][0] <= a < unit_ranges[i][1]:
        return unit_ranges[i][2]
    return None

oj = (ROOT/"config/45410914/objects.json").read_text()
wired = {os.path.basename(x) for x in re.findall(r'"([^"]+\.cpp)"', oj)}
wired |= {os.path.basename(x) for x in re.findall(r'"([^"]+\.c)"', oj)}

ANON=re.compile(r'\?A0x[0-9a-f]+')
def norm(n): return ANON.sub('?A0xANON', n)
ndefined={norm(x) for x in defined}
raw = json.loads((ROOT/"scripts/target_symbol_map.json").read_text())
per = defaultdict(lambda: {"tot": 0, "comp": 0, "miss": []})
for k, v in raw.items():
    if not k.lower().startswith("0x"): continue
    try: a = int(k.lower().removeprefix("0x"), 16)
    except ValueError: continue
    u = unit_of(a)
    if u is None: continue
    d = per[u]; d["tot"] += 1
    if norm(v) in ndefined: d["comp"] += 1
    else: d["miss"].append((a, v))

rows = []
for u, d in per.items():
    base = os.path.basename(u)
    if base not in wired: continue
    if not d["miss"]: continue
    rows.append((u, d["tot"], d["comp"], d["miss"]))
rows.sort(key=lambda r: -len(r[3]))

def cat(n):
    if n.startswith("__unwind$") or "__unwind$" in n: return "unwind"
    if n.startswith("??_E"): return "??_E"
    if n.startswith("??_G"): return "??_G"
    if n.startswith("??_") : return "??_x"
    if "?$" in n: return "template"
    return "named"

print(f"{'unit':50} {'tot':>5} {'miss':>5}  breakdown")
tot_miss = 0
for u, tot, comp, miss in rows[:60]:
    tot_miss += len(miss)
    c = defaultdict(int)
    for a, n in miss: c[cat(n)] += 1
    bd = " ".join(f"{k}:{v}" for k, v in sorted(c.items(), key=lambda x: -x[1]))
    print(f"{u[:50]:50} {tot:>5} {len(miss):>5}  {bd}")
print(f"\ntotal units with residue: {len(rows)}, total missing (top60): {tot_miss}, all: {sum(len(r[3]) for r in rows)}", file=sys.stderr)
json.dump([{"unit": r[0], "tot": r[1], "comp": r[2],
            "miss": [{"addr": f"0x{a:08X}", "sym": n} for a, n in sorted(r[3])]} for r in rows],
          open(ROOT/"scripts/_residue_census.json", "w"), indent=1)
