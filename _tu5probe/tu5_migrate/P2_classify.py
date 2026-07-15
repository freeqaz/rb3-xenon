#!/usr/bin/env python3
import json, bisect, re, os, argparse
from collections import Counter, defaultdict

# Path resolution: BASE = this script's dir (_tu5probe/tu5_migrate/); WT root two up.
# Override via --base / --report or env TU5_BASE / TU5_REPORT.
_ap = argparse.ArgumentParser()
_ap.add_argument("--base", default=os.environ.get("TU5_BASE"))
_ap.add_argument("--report", default=os.environ.get("TU5_REPORT"))
_args, _ = _ap.parse_known_args()
BASE = _args.base or (os.path.dirname(os.path.abspath(__file__)) + "/")
if not BASE.endswith("/"): BASE += "/"
_WT = os.path.abspath(os.path.join(BASE, "..", "..")) + "/"
SPLITS = BASE + "valayer_baseline_main/splits.txt"
REPORT = _args.report or (_WT + "build/45410914/report.json")

def h(s): return int(s, 16)

# ---- load map (function starts base->tu5) ----
mapfull = json.load(open(BASE + "base_to_tu5_map.full.json"))
MAP = {h(k): h(v) for k, v in mapfull["map"].items()}   # base_int -> tu5_int

# ---- load rich named records (size, tu5, body_identical) ----
recs = json.load(open(BASE + "base_to_tu5_map.json"))["functions"]
# base_int -> dict
REC = {}
for r in recs:
    b = h(r["base_va"])
    REC[b] = {
        "size": r.get("size"),
        "tu5": h(r["tu5_va"]) if r.get("tu5_va") else None,
        "bi": r.get("body_identical", False),
        "sym": r.get("symbol", ""),
    }

# ---- named_funcs: base_int -> size (real sizes) ----
nf = json.load(open(BASE + "named_funcs.json"))
NFSIZE = {}
for b, sz, sym in nf:
    NFSIZE[b] = sz

# ---- changed worklist ----
wl = json.load(open(BASE + "tu5_changed_worklist.json"))["worklist"]
CHANGED = {}          # base_int -> (why, size)
for w in wl:
    CHANGED[h(w["base_va"])] = (w["why_unmatched"], w.get("size", 0))
CHANGED_STARTS = sorted(CHANGED.keys())
CHANGED_SPAN = [(b, CHANGED[b][1]) for b in CHANGED_STARTS]

# ---- Complete base function-extent table from symbols.txt (authoritative) ----
import re as _re
fnline = _re.compile(r'^\S+ = \.text:0x([0-9A-Fa-f]+); // type:function size:0x([0-9A-Fa-f]+)')
FULL_FN = {}   # base_int -> size  (69,346 functions)
for line in open(BASE + "valayer_baseline_main/symbols.txt"):
    mm = fnline.match(line)
    if mm:
        FULL_FN[h(mm.group(1))] = int(mm.group(2), 16)
FN_STARTS = sorted(FULL_FN.keys())
print(f"FULL_FN functions: {len(FULL_FN)}")

def containing_fn(va):
    """Return start of the symbols.txt function whose extent contains va, or None."""
    i = bisect.bisect_right(FN_STARTS, va) - 1
    if i < 0:
        return None
    s = FN_STARTS[i]
    if s <= va < s + FULL_FN[s]:
        return s
    return None

# ---- Parent interval table (legacy, named-size) kept for reference ----
PARENTS = {}   # base_int -> (size, tu5_start)
for b in MAP:
    if b in CHANGED:
        continue
    size = NFSIZE.get(b) or (REC.get(b, {}).get("size"))
    if size:
        PARENTS[b] = (size, MAP[b])
PSTARTS = sorted(PARENTS.keys())

# Also a CHANGED-parent interval table (for detecting pins interior to a changed body)
CH_SIZE = {}
for b in CHANGED:
    size = CHANGED[b][1] or NFSIZE.get(b) or (REC.get(b, {}).get("size"))
    if size:
        CH_SIZE[b] = size
CHSTARTS = sorted(CH_SIZE.keys())

def find_parent(va, starts, table):
    """Return start of interval containing va, or None."""
    i = bisect.bisect_right(starts, va) - 1
    if i < 0:
        return None
    s = starts[i]
    size = table[s][0] if isinstance(table[s], tuple) else table[s]
    if s <= va < s + size:
        return s
    return None

def nearest_anchor_dist(va):
    """distance from va to nearest MAP key (function start)."""
    i = bisect.bisect_left(PSTARTS_ALL, va)
    best = 1 << 62
    for j in (i-1, i):
        if 0 <= j < len(PSTARTS_ALL):
            best = min(best, abs(PSTARTS_ALL[j] - va))
    return best

PSTARTS_ALL = sorted(MAP.keys())

# ---- classification of a single VA ----
def classify_va(va):
    if va in MAP:
        return ("DIRECT", MAP[va], None)
    if va in CHANGED:
        why = CHANGED[va][0]
        return ("CHANGED-" + why, None, va)
    p = containing_fn(va)          # authoritative extent lookup
    if p is not None and p != va:
        # va is INTERIOR to function p
        if p in CHANGED:
            return ("CHANGED-" + CHANGED[p][0], None, p)
        if p in MAP:
            return ("INTERIOR-DERIVABLE", MAP[p] + (va - p), p)
        # interior to an UNMAPPED (but unchanged) function -> gap
        return ("GAP-UNCHANGED", None, p)
    if p == va:
        # va IS a function start, not in MAP, not in CHANGED ->
        # unchanged body that the resolver failed to anchor (map coverage gap).
        return ("GAP-UNCHANGED", None, va)
    # not inside any known function -> true hole
    return ("UNMAPPABLE", None, None)

# ---- parse splits.txt ----
pins = []  # (unit, start, end)
cur = None
unit_re = re.compile(r'^([A-Za-z0-9_./\\-]+.*):\s*$')
text_re = re.compile(r'^\s+\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)')
for line in open(SPLITS):
    m = unit_re.match(line)
    if m and 'start:' not in line:
        cur = m.group(1)
        continue
    t = text_re.match(line)
    if t:
        pins.append((cur, h(t.group(1)), h(t.group(2))))

print(f"parsed {len(pins)} .text pins")

# ---- classify starts ----
start_class = Counter()
unmappable = []
gaps = []
end_class = Counter()
end_underivable = []

def first_fn_in(s, e):
    """first FULL_FN start in [s,e), or None."""
    i = bisect.bisect_left(FN_STARTS, s)
    if i < len(FN_STARTS) and FN_STARTS[i] < e:
        return FN_STARTS[i]
    return None

for unit, s, e in pins:
    cls, tu5, ref = classify_va(s)
    # If the start byte lands in inter-function padding (a hole), snap forward to
    # the first real function in the span -- that is what the pin boundary means.
    snapped = False
    if cls == "UNMAPPABLE":
        ff = first_fn_in(s, e)
        if ff is not None and ff != s:
            cls2, _, _ = classify_va(ff)
            if cls2 != "UNMAPPABLE":
                cls = cls2 + "*"   # * = snapped past padding
                snapped = True
    start_class[cls] += 1
    if cls == "UNMAPPABLE":
        unmappable.append((unit, s, e, nearest_anchor_dist(s)))
    elif cls == "GAP-UNCHANGED":
        gaps.append((unit, s, e, nearest_anchor_dist(s)))

    # END (exclusive): end_tu5 = last_fn_start_tu5 + last_fn_tu5_size.
    # Find the last FULL_FN whose start is in [s, e).
    i = bisect.bisect_left(FN_STARTS, e) - 1
    ls = FN_STARTS[i] if i >= 0 and FN_STARTS[i] >= s else None
    if ls is None:
        end_class["UNDERIVABLE"] += 1
        end_underivable.append((unit, s, e, "no-fn-in-span"))
        continue
    ecls, _, _ = classify_va(ls)
    if ecls in ("DIRECT", "INTERIOR-DERIVABLE"):
        end_class["DERIVABLE"] += 1
    elif ecls == "GAP-UNCHANGED":
        end_class["DERIVABLE-GAP"] += 1     # unchanged body => tu5 size==base size, recoverable
    elif ecls.startswith("CHANGED"):
        end_class["CHANGED"] += 1
    else:
        end_class["UNDERIVABLE"] += 1
        end_underivable.append((unit, s, e, "last-fn-unmappable"))

print("\n=== START classification ===")
tot = len(pins)
CLASSES = ["DIRECT","INTERIOR-DERIVABLE","GAP-UNCHANGED","CHANGED-AMBIG","CHANGED-MISS","UNMAPPABLE",
           "DIRECT*","GAP-UNCHANGED*","CHANGED-AMBIG*","CHANGED-MISS*"]
for k in CLASSES:
    if start_class.get(k,0):
        print(f"  {k:22s} {start_class.get(k,0):6d}  {100*start_class.get(k,0)/tot:5.2f}%")
print("  (* = start byte was inter-fn padding; snapped to first fn in span)")
print("  other:", {k:v for k,v in start_class.items() if k not in CLASSES})

print("\n=== END classification ===")
for k,v in end_class.items():
    print(f"  {k:22s} {v:6d}  {100*v/tot:5.2f}%")

print(f"\n=== UNMAPPABLE ({len(unmappable)}) ===")
for unit, s, e, d in sorted(unmappable, key=lambda x:-( x[2]-x[1])):
    print(f"  0x{s:08X}-0x{e:08X} size={e-s:5d} nearest_anchor={d:6d}  {unit}")
print(f"\n=== END-UNDERIVABLE ({len(end_underivable)}) ===")
for unit, s, e, why in end_underivable[:60]:
    print(f"  0x{s:08X}-0x{e:08X} size={e-s:6d}  {why:18s} {unit}")

# ---- FLOOR prediction: map matched functions to survivability ----
rep = json.load(open(REPORT))
# our matched functions are keyed by symbol/name; we need their BASE VA.
# Build symbol -> base_va from REC (named records) and named_funcs.
SYM2VA = {}
for b, sz, sym in nf:
    SYM2VA.setdefault(sym, b)
for r in recs:
    SYM2VA.setdefault(r.get("symbol",""), h(r["base_va"]))

# Authoritative symbol->base_va: target_symbol_map.json (va->sym) inverted,
# plus symbols.txt full dtk table (.text symbols).
TSM = json.load(open(BASE + "valayer_baseline_main/target_symbol_map.json"))
for va_s, sym in TSM.items():
    if not va_s.startswith("0x"):
        continue
    SYM2VA.setdefault(sym, h(va_s))
# symbols.txt: "NAME = .text:0xVA; // ..."
symtxt_re = re.compile(r'^(\S+) = \.text:0x([0-9A-Fa-f]+);')
n_txt = 0
for line in open(BASE + "valayer_baseline_main/symbols.txt"):
    mm = symtxt_re.match(line)
    if mm:
        SYM2VA.setdefault(mm.group(1), h(mm.group(2)))
        n_txt += 1
print(f"symbols.txt .text symbols: {n_txt}; total SYM2VA: {len(SYM2VA)}")

matched = []
for u in rep["units"]:
    for f in u.get("functions") or []:
        if f.get("match_percent_normalized") == 100.0:
            matched.append(f["name"])
print(f"\n=== FLOOR: {len(matched)} matched(100%) functions ===")

surv = Counter()
noval = 0
for name in matched:
    va = SYM2VA.get(name)
    if va is None:
        noval += 1
        surv["NO_VA(unknown)"] += 1
        continue
    cls, _, _ = classify_va(va)
    surv[cls] += 1

hard = surv.get("DIRECT",0) + surv.get("INTERIOR-DERIVABLE",0)
soft = hard + surv.get("GAP-UNCHANGED",0) + surv.get("NO_VA(unknown)",0)
atrisk_hard = surv.get("CHANGED-MISS",0)                      # genuinely changed bodies drop
atrisk_worst = surv.get("CHANGED-AMBIG",0)+surv.get("CHANGED-MISS",0)+surv.get("UNMAPPABLE",0)
for k in ["DIRECT","INTERIOR-DERIVABLE","GAP-UNCHANGED","CHANGED-AMBIG","CHANGED-MISS","UNMAPPABLE","NO_VA(unknown)"]:
    print(f"  {k:22s} {surv.get(k,0):6d}")
print(f"  HARD-SURVIVE (direct+interior)          = {hard}")
print(f"  SOFT-SURVIVE (+gap-unchanged+no_va)     = {soft}")
print(f"  GUARANTEED FLOOR (drop only MISS)       = {len(matched)-atrisk_hard}")
print(f"  WORST FLOOR (drop AMBIG+MISS+UNMAPPABLE) = {len(matched)-atrisk_worst}")
print(f"  (NO_VA resolves via symbol not in our maps: {noval})")

# save machine-readable
out = {
  "pins_total": tot,
  "start_class": dict(start_class),
  "end_class": dict(end_class),
  "unmappable": [{"unit":u,"start":f"0x{s:08X}","end":f"0x{e:08X}","size":e-s,"nearest_anchor":d} for u,s,e,d in unmappable],
  "gap_unchanged": [{"unit":u,"start":f"0x{s:08X}","end":f"0x{e:08X}","size":e-s,"nearest_anchor":d} for u,s,e,d in gaps],
  "matched_total": len(matched),
  "floor": dict(surv),
  "hard_survive": hard, "soft_survive": soft,
  "guaranteed_floor": len(matched)-atrisk_hard,
  "worst_floor": len(matched)-atrisk_worst,
}
json.dump(out, open(BASE+"P2_result.json","w"), indent=1)
print("\nwrote P2_result.json")
