#!/usr/bin/env python3
"""P2 stage-2 + P3: generate TU5-remapped VA-layer artifacts.
Reads ONLY the frozen baseline; writes ONLY under tu5_valayer/. No build."""
import json, bisect, re, statistics, os, argparse
from collections import Counter, defaultdict

# Path resolution: BASE = this script's dir; WT root two up. Override via --base / env TU5_BASE.
_ap = argparse.ArgumentParser()
_ap.add_argument("--base", default=os.environ.get("TU5_BASE"))
_args, _ = _ap.parse_known_args()
BASE = _args.base or (os.path.dirname(os.path.abspath(__file__)) + "/")
if not BASE.endswith("/"): BASE += "/"
_WT = os.path.abspath(os.path.join(BASE, "..", "..")) + "/"
FROZ = BASE + "valayer_baseline_main/"
OUT  = BASE + "tu5_valayer/"
os.makedirs(OUT, exist_ok=True)
def h(s): return int(s, 16)

TU5_TEXT_LO, TU5_TEXT_HI = 0x82270000, 0x82270000 + 0x9dce3c   # from tu5_section_table

# ---------- load frozen inputs ----------
MAP = {h(k): h(v) for k, v in json.load(open(BASE+"base_to_tu5_map.full.json"))["map"].items()}
CH  = {h(w["base_va"]): (w["why_unmatched"], w.get("size",0))
       for w in json.load(open(BASE+"tu5_changed_worklist.json"))["worklist"]}

# full symbol table (all sections) + function extents
SYMS = []          # (name, section, base_va, comment_tail)
FN = {}            # base_va -> size (only .text type:function)
symline = re.compile(r'^(\S+) = (\.?\w+):0x([0-9A-Fa-f]+);(.*)$')
fnsize  = re.compile(r'type:function size:0x([0-9A-Fa-f]+)')
for line in open(FROZ+"symbols.txt"):
    m = symline.match(line)
    if not m:
        SYMS.append((None, None, None, line.rstrip("\n")))   # passthrough (e.g. blank)
        continue
    name, sec, va, tail = m.group(1), m.group(2), h(m.group(3)), m.group(4)
    SYMS.append((name, sec, va, tail))
    fm = fnsize.search(tail)
    if sec == ".text" and fm:
        FN[va] = int(fm.group(1), 16)
FN_STARTS = sorted(FN)
mstarts = sorted(MAP)
print(f"loaded MAP={len(MAP)} CHANGED={len(CH)} FN={len(FN)} SYMS={len(SYMS)}")

# ---------- Decode TU5 .pdata (authoritative boundaries) EARLY so densify can snap ----------
import struct as _struct
_PE = _WT + "orig/45410914/band_tu5.exe"
_pd = open(_PE, "rb").read()[0x1f1600:0x1f1600 + 0x70e00]
PDATA = []
for _i in range(len(_pd) // 8):
    _b, _w = _struct.unpack_from(">II", _pd, _i*8)
    _L = ((_w >> 8) & 0x3FFFFF) * 4
    if TU5_TEXT_LO <= _b < TU5_TEXT_HI and _L > 0:
        PDATA.append((_b, _b + _L))
PDATA.sort()
PBEG = [b for b, e in PDATA]
PBEGSET = set(PBEG)
def pdata_encl(va):
    i = bisect.bisect_right(PBEG, va) - 1
    if i >= 0 and PDATA[i][0] <= va < PDATA[i][1]: return PDATA[i]
    return None
def snap_start_pdata(va):
    e = pdata_encl(va); return e[0] if (e and e[0] < va) else va
def snap_end_pdata(va):
    e = pdata_encl(va); return e[1] if (e and e[0] < va < e[1]) else va
print(f"decoded TU5 .pdata: {len(PDATA)} functions")

# ---------- DENSIFY: place gap (unmapped-unchanged) + AMBIG funcs by contiguity ----------
# Exclude only MISS (genuinely changed body); AMBIG bodies are UNCHANGED (skeleton-present-
# unplaced) so contiguity between their mapped neighbours disambiguates the placement.
MISS = {b for b,(why,_) in CH.items() if why == "MISS"}
gap_funcs = [b for b in FN_STARTS if b not in MAP and b not in MISS]
PLACE = {}          # base_va -> (tu5_va, confidence)
place_stats = Counter()
for b in gap_funcs:
    i = bisect.bisect_left(mstarts, b)
    P = mstarts[i-1] if i > 0 else None
    N = mstarts[i] if i < len(mstarts) else None
    fwd = MAP[N] - (N - b) if N is not None else None
    bwd = MAP[P] + (b - P) if P is not None else None
    conf = None; tu5 = None
    if fwd is not None and bwd is not None and fwd == bwd:
        tu5, conf = fwd, "RIGID"
    elif P is not None and P + FN.get(P, 0) == b:
        tu5, conf = MAP[P] + FN[P], "CONTIG"      # sits right after mapped P
    elif N is not None and b + FN[b] == N:
        tu5, conf = MAP[N] - FN[b], "CONTIG"
    elif fwd is not None or bwd is not None:
        cand = []
        if P is not None: cand.append((b - P, bwd))
        if N is not None: cand.append((N - b, fwd))
        cand.sort(); tu5, conf = cand[0][1], "LOOSE"
    else:
        conf = "NOANCHOR"
    if tu5 is not None and TU5_TEXT_LO <= tu5 < TU5_TEXT_HI:
        # Contiguity can land a placement mid-.pdata-function: that means this base
        # function ICF-FOLDED into the enclosing TU5 function. Snap to the .pdata
        # start (the ICF representative) so the placement is a real TU5 boundary.
        enc = pdata_encl(tu5)
        if enc is not None and enc[0] < tu5:
            tu5 = enc[0]; conf += "-ICF"
        tag = ("AMBIG-" if b in CH else "") + conf
        PLACE[b] = (tu5, tag); place_stats[tag] += 1
    else:
        place_stats["REJECT-oob" if tu5 is not None else "NOANCHOR"] += 1

# unified base->tu5 for any .text function: MAP (direct) or PLACE (densified)
def tu5_of_fn(b):
    if b in MAP: return MAP[b], "DIRECT"
    if b in PLACE: return PLACE[b][0], PLACE[b][1]
    return None, None

# write densified map
dens = {"meta": {"source": "base_to_tu5_map.full.json + contiguity densify",
                 "direct": len(MAP), "densified": len(PLACE),
                 "by_conf": dict(place_stats)},
        "map": {f"0x{b:x}": f"0x{v:x}" for b, v in sorted(MAP.items())}}
for b, (v, c) in PLACE.items():
    dens["map"][f"0x{b:x}"] = f"0x{v:x}"
json.dump(dens, open(OUT+"base_to_tu5_map.densified.json", "w"))
gap_unchanged = [b for b in gap_funcs if b not in CH]
placed_unchanged = sum(1 for b in gap_unchanged if b in PLACE)
print(f"DENSIFY: gap(unmapped)={len(gap_funcs)} placed={len(PLACE)} conf={dict(place_stats)}")
print(f"  gap-UNCHANGED={len(gap_unchanged)} placed={placed_unchanged} unplaced={len(gap_unchanged)-placed_unchanged}")

# ---------- global delta-trend model (rejects map mis-resolution outliers) ----------
# For each mapped base fn, expected TU5 delta = median delta of its base-neighbourhood.
mdeltas = [MAP[b]-b for b in mstarts]
WIN = 20
def expected_delta_idx(i):
    lo = max(0, i-WIN); hi = min(len(mstarts), i+WIN+1)
    w = sorted(mdeltas[lo:hi])
    return w[len(w)//2]
# precompute a coarse expected-delta over base space (sampled, monotone-ish)
GTOL = 0x8000   # a function whose delta deviates >32KB from local trend is an outlier
def is_outlier(b):
    i = bisect.bisect_left(mstarts, b)
    if i >= len(mstarts) or mstarts[i] != b:
        # not a mapped fn; use nearest index
        i = min(i, len(mstarts)-1)
    exp = expected_delta_idx(i)
    return abs((MAP[b]-b) - exp) > GTOL

# ---------- Reconcile symbols.txt to the (already-decoded) TU5 .pdata authority ----------
# dtk treats the TU5 .pdata unwind table as the authoritative function-boundary source
# and OVERRIDES symbols.txt sizes. We build the sized-symbol table from the decoded
# .pdata RUNTIME_FUNCTIONs (+ leaf functions in gaps), so dtk's override is a no-op.
_TSM_base = json.load(open(FROZ+"target_symbol_map.json"))
TSM_BASE_KEYS = set(h(k) for k in _TSM_base if k.startswith("0x"))

def _tu5_of_fnstart(f):
    if f in MAP: return MAP[f]
    if f in PLACE: return PLACE[f][0]
    return None

def remap_addr(va):
    """Remap ANY base .text address to TU5 (fn start / interior label / gap object)."""
    if va in MAP: return MAP[va]
    if va in PLACE: return PLACE[va][0]
    i = bisect.bisect_right(FN_STARTS, va) - 1
    if i >= 0:
        f = FN_STARTS[i]
        if f <= va < f + FN[f]:
            t = _tu5_of_fnstart(f)
            if t is not None: return t + (va - f)
    j = bisect.bisect_right(FN_STARTS, va)     # gap object hugs the NEXT function
    if j < len(FN_STARTS):
        t = _tu5_of_fnstart(FN_STARTS[j])
        if t is not None: return t - (FN_STARTS[j] - va)
    if i >= 0:
        t = _tu5_of_fnstart(FN_STARTS[i])
        if t is not None: return t + (va - FN_STARTS[i])
    return None

# reverse densified map: TU5 addr -> canonical base fn (prefer one with a TSM pairing)
REV = {}
for b in list(MAP.keys()) + list(PLACE.keys()):
    t = _tu5_of_fnstart(b)
    if t is None: continue
    if t not in REV or (b in TSM_BASE_KEYS and REV[t] not in TSM_BASE_KEYS):
        REV[t] = b
_myfn = sorted(REV.keys())
LEAF = [f for f in _myfn if f not in PBEGSET and pdata_encl(f) is None]   # real leaves (gaps)
DEMOTED = set(f for f in _myfn if f not in PBEGSET and pdata_encl(f) is not None)  # dtk->label
_fun_starts = sorted(PBEGSET | set(LEAF))
def _next_fun(v):
    j = bisect.bisect_right(_fun_starts, v)
    return _fun_starts[j] if j < len(_fun_starts) else TU5_TEXT_HI

_szre = re.compile(r'size:0x([0-9A-Fa-f]+)')
# FINAL_SIZED tu5 -> (name, size, kind, base_va).  Functions come from .pdata (+leaves);
# their extents EQUAL .pdata extents so dtk's override is a no-op.
FINAL_SIZED = {}
for (B, E) in PDATA:                                  # 1) .pdata functions (authority)
    FINAL_SIZED[B] = (f"fn_{B:08X}", E - B, "function", REV.get(B, -1))
for L in LEAF:                                        # 2) leaf functions in .pdata gaps
    b = REV.get(L); bsz = FN.get(b, 0) if b is not None else 0
    size = min(bsz, _next_fun(L) - L) if bsz else (_next_fun(L) - L)
    size = max(4, snap_end_pdata(L + size) - L)       # never extend into a .pdata fn
    FINAL_SIZED[L] = (f"fn_{L:08X}", size, "function", b if b is not None else -1)
_seen = set(FINAL_SIZED)                              # 3) except_data objects (deduped)
_objs = []
for name, sec, va, tail in SYMS:
    if name is None or sec != ".text" or "type:function" in tail: continue
    m = _szre.search(tail)
    if not m: continue
    t = remap_addr(va)
    if t is None or not (TU5_TEXT_LO <= t < TU5_TEXT_HI): continue
    if t in _seen: continue
    if pdata_encl(t) is not None: continue            # skip objects inside a .pdata fn
    _objs.append((t, va, name, int(m.group(1), 16)))
_objs.sort()
for t, bva, name, bsz in _objs:
    if t in _seen: continue
    _seen.add(t); FINAL_SIZED[t] = (name, bsz, "object", bva)
SIZED_ADDRS = sorted(FINAL_SIZED)
for idx, t in enumerate(SIZED_ADDRS):                 # tiling: never extend past next symbol
    nm, sz, kind, bva = FINAL_SIZED[t]
    nextb = SIZED_ADDRS[idx+1] if idx+1 < len(SIZED_ADDRS) else TU5_TEXT_HI
    FINAL_SIZED[t] = (nm, min(sz, nextb - t), kind, bva)
BSET = SIZED_ADDRS
def next_boundary(v):
    j = bisect.bisect_right(BSET, v)
    return BSET[j] if j < len(BSET) else TU5_TEXT_HI
icf_size_reconciled = len(DEMOTED)
print(f"PDATA model: pdata_funcs={len(PDATA)} leaves={len(LEAF)} demoted-inside-pdata={len(DEMOTED)} "
      f"objects={len(_objs)} -> sized_symbols={len(SIZED_ADDRS)}")

# ---------- robust per-pin remap ----------
TOL = 0x10000
def remap_span(s, e):
    """Return (start_tu5, end_tu5, status, note)."""
    lo = bisect.bisect_left(mstarts, s); hi = bisect.bisect_left(mstarts, e)
    base_in = [mstarts[i] for i in range(lo, hi)]
    base_in = [b for b in base_in if not is_outlier(b)]   # drop map mis-resolutions
    if base_in:
        deltas = [MAP[b]-b for b in base_in]
        med = statistics.median(deltas)
        cons = [b for b in base_in if abs((MAP[b]-b)-med) <= TOL]
    else:
        cons = []
    # candidate function starts in span with a tu5 (consensus-direct OR placed)
    fi = bisect.bisect_left(FN_STARTS, s); fj = bisect.bisect_left(FN_STARTS, e)
    span_fns = FN_STARTS[fi:fj]
    consset = set(cons)
    def good(b):
        if b in consset: return MAP[b]
        if b in PLACE and PLACE[b][1].endswith(("RIGID","CONTIG")):
            v = PLACE[b][0]
            if not cons or abs(v - (b+med)) <= TOL: return v
        return None
    placed = [(b, good(b)) for b in span_fns]
    placed = [(b, v) for b, v in placed if v is not None]
    if not placed:
        return None, None, "DROP", "no-anchor-in-span"
    first_b, first_v = placed[0]
    last_b,  last_v  = placed[-1]           # last function BY BASE ORDER (natural unit end)
    # START/END snap to a .pdata function edge (never mid-function). END = the last
    # function's own end (.pdata end for a pdata fn, else its sized/next-boundary end).
    start_tu5 = snap_start_pdata(first_v)
    if last_v in FINAL_SIZED:
        end_tu5 = last_v + FINAL_SIZED[last_v][1]
    else:
        end_tu5 = next_boundary(last_v)
    end_tu5 = snap_end_pdata(end_tu5)
    note = ""
    if first_b != s: note += f"snap+{first_b-s} "
    if end_tu5 <= start_tu5: return None, None, "DROP", "degenerate"
    # pin-level global consistency: start delta must track the map trend at this base
    i = bisect.bisect_left(mstarts, first_b); i = min(i, len(mstarts)-1)
    exp = expected_delta_idx(i)
    if abs((start_tu5 - first_b) - exp) > GTOL:
        return None, None, "DROP", f"pin-delta-outlier(off 0x{abs((start_tu5-first_b)-exp):x})"
    return start_tu5, end_tu5, "OK", note.strip()

# ---------- rewrite splits.txt (two-pass: remap -> clamp benign overlaps -> emit) ----------
# Pass 1: tokenize into passthrough lines + pin records with a stable id.
tokens = []          # list of ("line", text) or ("pin", id)
pins_meta = {}       # id -> dict(unit,indent,sec,s,e, st,en,status,note)
dropped = []
pin_stats = Counter()
cur_unit = None
# NOTE: capture optional trailing `rename:.text$xx` — these COMDAT sub-section
# splits occupy .text VA space and MUST be remapped + overlap-checked like plain .text.
seg_re = re.compile(r'^(\s+)(\.?\w+)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)(\s+rename:\S+)?\s*$')
pid = 0
for line in open(FROZ+"splits.txt"):
    m = seg_re.match(line)
    if m and 'type:' not in line:
        indent, sec, s, e = m.group(1), m.group(2), h(m.group(3)), h(m.group(4))
        rename = (m.group(5) or "").rstrip()   # e.g. " rename:.text$yd" or ""
        if sec == ".pdata":
            pin_stats["pdata-dropped(dtk-rederive)"] += 1
            continue
        if sec == ".text":
            st, en, status, note = remap_span(s, e)
            pins_meta[pid] = dict(unit=cur_unit, indent=indent, s=s, e=e,
                                  st=st, en=en, status=status, note=note, rename=rename)
            tokens.append(("pin", pid)); pid += 1
            continue
        pin_stats[f"{sec}-dropped(no-datamap)"] += 1
        dropped.append({"unit": cur_unit, "section": sec,
                        "base_start": f"0x{s:08X}", "base_end": f"0x{e:08X}", "reason": "no data-section map"})
        continue
    hm = re.match(r'^(\S.*):\s*$', line)
    if hm and 'start:' not in line and 'type:' not in line and hm.group(1) != "Sections":
        cur_unit = hm.group(1)
    tokens.append(("line", line))

# Pass 1.5: capture the COMPLETE RAW overlap list (before resolution) with root cause.
raw = sorted([p for p, mt in pins_meta.items() if mt["status"] == "OK"],
             key=lambda i: (pins_meta[i]["st"], pins_meta[i]["en"]))
raw_overlaps = []
# sweep with a running max-end and remember which pin owns it
active = []
for i in raw:
    mt = pins_meta[i]
    for j in raw:
        if j == i: break
    # compare against all still-overlapping earlier pins via simple back-scan
for a_idx in range(len(raw)):
    A = pins_meta[raw[a_idx]]
    for b_idx in range(a_idx+1, len(raw)):
        B = pins_meta[raw[b_idx]]
        if B["st"] >= A["en"]:
            break                      # sorted by start; no further overlap with A
        # A and B overlap in TU5
        base_ov = not (A["e"] <= B["s"] or B["e"] <= A["s"])
        raw_overlaps.append({
            "unitA": A["unit"] + (A.get("rename","") or ""), "tu5A": f'0x{A["st"]:08X}-0x{A["en"]:08X}', "baseA": f'0x{A["s"]:08X}-0x{A["e"]:08X}',
            "unitB": B["unit"] + (B.get("rename","") or ""), "tu5B": f'0x{B["st"]:08X}-0x{B["en"]:08X}', "baseB": f'0x{B["s"]:08X}-0x{B["e"]:08X}',
            "overlap_bytes": min(A["en"],B["en"]) - max(A["st"],B["st"]),
            "root_cause": "PRE-EXISTING base-range overlap" if base_ov else "REMAP-COLLAPSE (base non-overlapping)"})
json.dump({"count": len(raw_overlaps), "overlaps": raw_overlaps}, open(OUT+"overlaps_complete.json","w"), indent=1)
print(f"  RAW overlaps detected (pre-resolution): {len(raw_overlaps)}")

# Pass 2: resolve ALL overlaps (plain .text AND rename `.text$xx` splits together).
# Phantom units (Flow.cpp — retail has NO Flow system) and fully-nested/mis-mapped
# intruders are DROPPED (never truncate a real enclosing unit for them). Genuine
# adjacent-TU boundary grazes are clamped.
PHANTOM_UNITS = {"Flow.cpp"}
def pin_delta_ok(pid_):
    mt = pins_meta[pid_]
    j = bisect.bisect_left(mstarts, mt["s"]); j = min(j, len(mstarts)-1)
    return abs((mt["st"] - mt["s"]) - expected_delta_idx(j)) <= GTOL
clamp_benign = 0; clamp_trunc = []; icf_drop = 0; phantom_drop = 0; nested_drop = 0
overlap_log = []
# iterate to a fixed point
for _ in range(6):
    ok = sorted([p for p, mt in pins_meta.items() if mt["status"] == "OK"],
                key=lambda i: (pins_meta[i]["st"], pins_meta[i]["en"]))
    changed = False
    prev = None
    for i in ok:
        mt = pins_meta[i]
        if prev is None:
            prev = i; continue
        pm = pins_meta[prev]
        if mt["st"] < pm["en"]:
            delta = pm["en"] - mt["st"]
            # DROP only for: (1) phantom unit, (2) a mis-mapped (bad-delta) pin,
            # (3) an exact ICF fold (identical TU5 start). NEVER drop a real,
            # correctly-mapped unit just because it is nested — instead clamp the
            # OVERSHOOTING earlier end down to this real start (preserves both units).
            drop = None
            if mt["unit"] in PHANTOM_UNITS and pm["unit"] not in PHANTOM_UNITS: drop = i
            elif pm["unit"] in PHANTOM_UNITS and mt["unit"] not in PHANTOM_UNITS: drop = prev
            elif not pin_delta_ok(i) and pin_delta_ok(prev): drop = i
            elif pin_delta_ok(i) and not pin_delta_ok(prev): drop = prev
            elif mt["st"] == pm["st"]:            # identical TU5 start -> ICF dup; drop shorter
                drop = i if (mt["en"]-mt["st"]) <= (pm["en"]-pm["st"]) else prev
            if drop is not None:
                d = pins_meta[drop]
                reason = ("Flow-phantom-inside-real" if d["unit"] in PHANTOM_UNITS
                          else "mis-map-intruder" if not pin_delta_ok(drop)
                          else "fully-nested-dup")
                d["status"] = "DROP"; d["note"] = f"overlap:{reason} vs {pins_meta[i if drop==prev else prev]['unit']}"
                if d["unit"] in PHANTOM_UNITS: phantom_drop += 1
                elif "mis-map" in reason: nested_drop += 1
                else: nested_drop += 1
                overlap_log.append({"dropped_unit": d["unit"], "dropped_va": f'0x{d["st"]:08X}-0x{d["en"]:08X}',
                                    "reason": reason, "vs_unit": pins_meta[i if drop==prev else prev]["unit"]})
                changed = True
                if drop == prev:
                    prev = i
                continue
            # else clamp prev end down to current start
            pm["en"] = mt["st"]
            if pm["en"] <= pm["st"]:
                pm["status"] = "DROP"; pm["note"] = "overlap:ICF-fold/nested-dup (same TU5 start)"
                icf_drop += 1; changed = True; prev = i; continue
            if delta <= 64: clamp_benign += 1
            else:
                clamp_trunc.append({"unit": pm["unit"], "trunc_bytes": delta,
                                    "new_end": f'0x{pm["en"]:08X}', "into_unit": mt["unit"]})
            changed = True
        prev = i
    if not changed:
        break
clamped = clamp_benign
json.dump(overlap_log, open(OUT+"overlap_drops.json","w"), indent=1)

# Pass 3: emit
out_lines = []
new_text_ranges = []
for kind, payload in tokens:
    if kind == "line":
        out_lines.append(payload); continue
    mt = pins_meta[payload]
    if mt["status"] == "OK" and mt["en"] > mt["st"]:
        suffix = mt.get("rename", "") or ""
        out_lines.append(f'{mt["indent"]}.text       start:0x{mt["st"]:08X} end:0x{mt["en"]:08X}{suffix}\n')
        new_text_ranges.append((mt["unit"], mt["st"], mt["en"], mt["s"], mt["e"]))
        pin_stats["text-remapped"] += 1
    else:
        dropped.append({"unit": mt["unit"], "section": ".text",
                        "base_start": f'0x{mt["s"]:08X}', "base_end": f'0x{mt["e"]:08X}',
                        "size": mt["e"]-mt["s"], "reason": (mt["note"] or mt["status"]) + (mt.get("rename","") or "")})
        pin_stats["text-dropped"] += 1
# Pass 3.5: compact away unit-header blocks left range-less by dropped .text pins.
# dtk emits a degenerate 42-byte COFF for an empty `Foo.cpp:` entry, which objdiff
# cannot parse (breaks `report generate`). A unit with all ranges dropped must have
# NO entry at all (mirrors the P4-gate committed splits.txt).
_hdr_re = re.compile(r'^\S.*:\s*$')
_range_re = re.compile(r'^\s+\.\w+\s+start:0x')
compact = []
i = 0
_empty_units = []
while i < len(out_lines):
    ln = out_lines[i]
    is_hdr = _hdr_re.match(ln) and 'start:' not in ln and ln.strip() != "Sections:"
    if is_hdr:
        j = i + 1
        body = []
        while j < len(out_lines) and not (_hdr_re.match(out_lines[j]) and 'start:' not in out_lines[j] and out_lines[j].strip() != "Sections:"):
            body.append(out_lines[j]); j += 1
        if any(_range_re.match(b) for b in body):
            compact.append(ln); compact.extend(body)
        else:
            _empty_units.append(ln.strip())   # drop header + its (blank-only) body
        i = j
    else:
        compact.append(ln); i += 1
if _empty_units:
    print(f"  compacted {len(_empty_units)} range-less unit entries: {_empty_units}")
out_lines = compact
open(OUT+"splits.txt", "w").writelines(out_lines)
json.dump({"count": len(dropped), "dropped": dropped}, open(OUT+"dropped_pins.json","w"), indent=1)
json.dump({"benign_clamps(<=64B)": clamp_benign, "icf_nested_drops": icf_drop,
           "phantom_drops": phantom_drop, "intruder/nested_drops": nested_drop,
           "truncations(>64B)": clamp_trunc, "overlap_drops": overlap_log},
          open(OUT+"overlap_resolution.json","w"), indent=1)
print(f"SPLITS: {dict(pin_stats)}  dropped={len(dropped)}")
print(f"  overlap-resolve: benign_clamps={clamp_benign} icf_drops={icf_drop} "
      f"phantom_drops={phantom_drop} intruder/nested_drops={nested_drop} truncations>64B={len(clamp_trunc)}")

# ---------- rewrite symbols.txt (deduped: ONE sized symbol per TU5 addr) ----------
unresolved = Counter()
unresolved_samples = defaultdict(list)
sym_out = []
resolved = 0
# 1) sized symbols: one per TU5 addr with a tiling size (no conflicting sizes).
#    Regenerate anon fn_/except-style names at the TU5 address so the target-symbol
#    renamer (keyed by TU5 addr) still pairs.
for t in SIZED_ADDRS:
    name, size, kind, bva = FINAL_SIZED[t]
    if kind == "function":
        nm = f"fn_{t:08X}" if name.startswith("fn_") else name
        sym_out.append(f"{nm} = .text:0x{t:08X}; // type:function size:0x{size:X}\n")
    else:
        sym_out.append(f"{name} = .text:0x{t:08X}; // type:object size:0x{size:X} scope:global\n")
    resolved += 1
# 2) unsized .text labels: remap, skip any that collide with a sized-symbol address.
seen = set(SIZED_ADDRS)
for name, sec, va, tail in SYMS:
    if name is None:
        continue
    if sec != ".text":
        unresolved[sec] += 1
        if len(unresolved_samples[sec]) < 5: unresolved_samples[sec].append(f"0x{va:08X} {name[:50]}")
        continue
    if _szre.search(tail):
        continue                                   # already emitted (sized)
    tv = remap_addr(va)
    if tv is None:
        unresolved[".text"] += 1
        if len(unresolved_samples[".text"]) < 5: unresolved_samples[".text"].append(f"0x{va:08X} {name[:60]}")
        continue
    if tv in seen:
        continue                                   # would collide with a sized symbol
    seen.add(tv)
    sym_out.append(f"{name} = .text:0x{tv:08X};{tail}\n"); resolved += 1
open(OUT+"symbols.txt", "w").writelines(sym_out)
json.dump({"resolved_text": resolved,
           "unresolved_by_section": dict(unresolved),
           "note": "non-.text sections have no per-symbol base->TU5 map (map is .text-only, non-affine); these are re-derived by dtk's TU5 analysis pass on flip",
           "samples": {k: v for k, v in unresolved_samples.items()}},
          open(OUT+"unresolved_symbols.json","w"), indent=1)
print(f"SYMBOLS: resolved(.text)={resolved} unresolved={dict(unresolved)}")

# ---------- rewrite target_symbol_map.json (dedup by TU5 addr) ----------
TSM = json.load(open(FROZ+"target_symbol_map.json"))
tsm_out = {}; tsm_unres = []; tsm_folded = []
# canonical base VA kept at each folded TU5 addr (so pairing name matches symbols.txt)
canon_bva = {t: FINAL_SIZED[t][3] for t in FINAL_SIZED}
for va_s, sym in TSM.items():
    if not va_s.startswith("0x"):
        tsm_out[va_s] = sym; continue
    va = h(va_s)
    tv = remap_addr(va)
    if tv is None:
        tsm_unres.append({"base": va_s, "sym": sym}); continue
    key = f"0x{tv:08x}"
    if key in tsm_out:
        # ICF fold: address already claimed. Keep the entry whose base VA is the
        # symbol-model canonical (matches symbols.txt); fold the other out.
        if canon_bva.get(tv) == va:
            tsm_folded.append({"tu5": key, "replaced": tsm_out[key], "with": sym})
            tsm_out[key] = sym
        else:
            tsm_folded.append({"tu5": key, "kept": tsm_out[key], "folded_out": sym})
        continue
    tsm_out[key] = sym
json.dump(tsm_out, open(OUT+"target_symbol_map.json","w"), indent=1)
print(f"TARGET_SYMBOL_MAP: in={len(TSM)} out={len(tsm_out)} unresolved={len(tsm_unres)} icf_folded={len(tsm_folded)}")
json.dump({"unresolved": tsm_unres, "icf_folded": tsm_folded}, open(OUT+"target_symbol_map.unresolved.json","w"), indent=1)

# copy config.yml verbatim
import shutil; shutil.copy(FROZ+"config.yml", OUT+"config.yml")

# stash for validation script
json.dump({"new_text_ranges": [[u,s,e,bs,be] for u,s,e,bs,be in new_text_ranges]},
          open(OUT+"_new_text_ranges.json","w"))
print("DONE stage-2 artifacts written to tu5_valayer/")
