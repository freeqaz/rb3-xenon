#!/usr/bin/env python3
"""Audit splits.txt .text ranges against the authoritative function-boundary
table in symbols.txt (post-truncation-fix, grown sizes).

Finds split ranges whose boundaries don't align to real function boundaries —
the class of bug introduced when a range was pinned against an OLDER jeff that
reported truncated/oversized/phantom function sizes. Build validate only
forbids a symbol *straddling* start/end; it tolerates gap-ends, leading/trailing
padding, and end-short under-pins, which silently drop matchable code.

Outputs a ranked candidate list for a verification fan-out.
"""
import re, sys, json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SYM = ROOT / "config/45410914/symbols.txt"
SPLITS = ROOT / "config/45410914/splits.txt"

# ---- 1. function-boundary table from symbols.txt ----------------------------
# fn_82260000 = .text:0x82260000; // type:function size:0x14
fn_re = re.compile(r"^(\S+)\s*=\s*\.text:0x([0-9A-Fa-f]+);.*?\btype:function\b.*?\bsize:0x([0-9A-Fa-f]+)", )
funcs = []  # (addr, size, name)
for line in SYM.read_text().splitlines():
    m = fn_re.search(line)
    if m:
        funcs.append((int(m.group(2), 16), int(m.group(3), 16), m.group(1)))
funcs.sort()
addrs = [f[0] for f in funcs]
by_addr = {f[0]: f for f in funcs}
ends = {f[0] + f[1] for f in funcs}
import bisect
def fn_starting_at(a):  # function whose start == a
    return by_addr.get(a)
def fn_containing(a):   # function strictly containing address a (start<a<end)
    i = bisect.bisect_right(addrs, a) - 1
    if i < 0: return None
    s, sz, nm = funcs[i]
    return (s, sz, nm) if s < a < s + sz else None
def next_fn_after(a):   # first function with start >= a
    i = bisect.bisect_left(addrs, a)
    return funcs[i] if i < len(funcs) else None
def last_fn_before(a):  # last function with start < a
    i = bisect.bisect_left(addrs, a) - 1
    return funcs[i] if i >= 0 else None

print(f"[symbols] {len(funcs)} code functions", file=sys.stderr)

# ---- 2. parse splits.txt .text ranges ---------------------------------------
text_ranges = []  # (tu, start, end, lineno)
cur = None
for i, line in enumerate(SPLITS.read_text().splitlines(), 1):
    hm = re.match(r"^(\S+\.(?:cpp|c|cc)):\s*$", line)
    if hm:
        cur = hm.group(1); continue
    tm = re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", line)
    if tm and cur:
        text_ranges.append((cur, int(tm.group(1),16), int(tm.group(2),16), i))
print(f"[splits] {len(text_ranges)} .text ranges across pinned TUs", file=sys.stderr)

# ---- 3. scan ---------------------------------------------------------------
anomalies = []
for tu, start, end, ln in text_ranges:
    issues = []
    # start alignment
    if fn_starting_at(start) is None:
        c = fn_containing(start)
        if c:
            issues.append(("START_IN_FN", f"start 0x{start:X} inside {c[2]} [0x{c[0]:X},0x{c[0]+c[1]:X})"))
        else:
            nf = next_fn_after(start)
            gap = (nf[0]-start) if nf else -1
            issues.append(("START_IN_GAP", f"start 0x{start:X} not a fn-start; next fn {nf[2] if nf else '-'} +0x{gap:X}"))
    # end alignment
    if end not in ends:
        c = fn_containing(end)
        if c:
            issues.append(("END_IN_FN", f"end 0x{end:X} inside {c[2]} [0x{c[0]:X},0x{c[0]+c[1]:X}) (BUILD-BREAK)"))
        else:
            lb = last_fn_before(end)
            lb_end = lb[0]+lb[1] if lb else 0
            issues.append(("END_IN_GAP", f"end 0x{end:X} not a fn-end; last fn {lb[2] if lb else '-'} ends 0x{lb_end:X} (gap 0x{end-lb_end:X})"))
    # next function immediately after range end (possible under-pin / next TU)
    nf = fn_starting_at(end)
    if nf:
        issues.append(("NEXT_FN_ABUTS", f"fn {nf[2]} starts exactly at end (0x{end:X}, size 0x{nf[1]:X})"))
    if issues:
        anomalies.append((tu, start, end, ln, issues))

# ---- 4. report -------------------------------------------------------------
# Categorize. The actionable ones: START_IN_GAP, END_IN_GAP, END_IN_FN, START_IN_FN.
cats = {}
for tu,s,e,ln,iss in anomalies:
    for code,_ in iss:
        cats.setdefault(code, 0); cats[code]+=1
print("\n=== anomaly category counts ===")
for k,v in sorted(cats.items(), key=lambda x:-x[1]):
    print(f"  {k:16} {v}")

# Focus report: ranges with mis-alignment (gap/inside), excluding the benign NEXT_FN_ABUTS-only
focus = [a for a in anomalies if any(c in ("START_IN_GAP","END_IN_GAP","END_IN_FN","START_IN_FN") for c,_ in a[4])]
print(f"\n=== {len(focus)} ranges with start/end MISALIGNMENT (gap or inside-fn) ===")
for tu,s,e,ln,iss in sorted(focus, key=lambda x:x[0]):
    flags = [c for c,_ in iss if c in ("START_IN_GAP","END_IN_GAP","END_IN_FN","START_IN_FN")]
    print(f"  L{ln:<5} {tu:<34} .text 0x{s:X}..0x{e:X}  {','.join(flags)}")
    for c,d in iss:
        if c in ("START_IN_GAP","END_IN_GAP","END_IN_FN","START_IN_FN"):
            print(f"            {c}: {d}")

# dump JSON for the workflow (NEVER into the main tree — see wave-18 build incident)
out = Path("/tmp/splits_audit.json")
json.dump([{"tu":tu,"start":s,"end":e,"line":ln,
            "issues":[{"code":c,"detail":d} for c,d in iss]} for tu,s,e,ln,iss in anomalies],
          open(out,"w"), indent=1)
print(f"\n[json] {out}")
