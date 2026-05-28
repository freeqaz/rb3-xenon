#!/usr/bin/env python3
"""
pin_candidates.py — the unified oracle -> pin ranker.

Designed by docs/plans/execution-schedule.md S2. We have FIVE oracle sources
(bindiff, callgraph triangulation, RTTI+vtable transitivity, vtable-only, and
string-content autoid) but only bindiff ever fed a pin wave. The other ~1,952
identifications (callgraph/rtti/vtable) are inert leads lists. This tool merges
them all, weights confidence by per-oracle precision + multi-oracle consensus,
filters to functions whose dc3 source TU is actually present in OUR src/ tree
(so it can be compiled+pinned), groups the survivors by target TU, drops the
ones already pinned, derives a per-TU .text address span (snapped to symbol
boundaries from symbols.txt), and emits the NEW pin wave ranked by
consensus-weighted oracle-fn count.

It does NOT touch config/45410914/* or run a build — it only reads the (gitignored,
regenerable) oracle JSONs + symbols.txt + splits.txt + objects.json, and writes
a ranked proposal for a separate, quiescence-gated pin-wave step to consume.

  PIPELINE: merge by addr -> consensus tiers -> source-present gate
            -> TU grouping (drop pinned) -> provisional span snap -> rank/emit

Usage:
  pin_candidates.py rank [options]

  --oracles F [F ...]   oracle JSONs to merge
                        (default: unified_id.json unified_id_callgraph.json
                                  unified_id_rtti.json unified_id_vtable.json)
  --symbols FILE        symbols.txt boundary/snap oracle (default config one)
  --splits FILE         splits.txt (already-pinned set to skip)
  --objects FILE        objects.json (already-pinned set + group inference)
  --src-root DIR        our source tree root for the source-present gate
  --out FILE            ranked machine-readable wave (pin_candidates.json)
  --report FILE         human-readable report (pin_candidates_report.txt)
  --review FILE         low-confidence + disagreement queue (pin_candidates_review.json)
  --splits-append FILE  appendable splits.txt blob, ranked (pin_candidates.splits.append)
  --objects-append FILE appendable objects.json snippet (pin_candidates.objects.append)

Per docs/plans/execution-schedule.md the verified yield is ~194 NEW-pinnable TUs
covering ~981 oracle-identified functions. The tool prints whether it reproduces
that and explains any divergence.

CAVEAT (load-bearing — see project_jeff_asm_misnest.md): the symbols.txt boundary
oracle may contain overlapping / oversized "phantom" function symbols if it was
written by a pre-fix jeff. The tool auto-detects this (presence of the documented
fn_82BF8E48 phantom) and labels its output STALE vs FRESH. Either way every span
is PROVISIONAL and any span touching a residual overlap region is flagged
`jeff_blocked=true` for hand-verification / post-re-SPLIT re-derivation.

Two more nuances the plan's S2.7 snippet glossed (both verified from live data
and handled here):
  * Oracle hits for one dc3 TU are NOT one tight cluster — a TU has a DOMINANT
    contiguous cluster plus scattered ICF/inline-alias outliers across the whole
    binary, so a bare [min,max] hull is meaningless (e.g. Rnd_Xbox spans 8 MB by
    hull but 0x35cc by dominant cluster). We pin the dominant cluster and count
    the rest as outliers.
  * Source resolution uses the dc3 path-TAIL (not basename) to disambiguate the
    32 basename collisions in our src/ (ShaderMgr rndobj vs rnddx9, Utl ×6, …).
    --basename-match reproduces the plan's inflated basename grouping for audit.
"""
import argparse
import bisect
import json
import os
import re
import sys
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _p(*parts):
    return os.path.join(ROOT, *parts)


DEFAULT_ORACLES = [
    _p("unified_id.json"),
    _p("unified_id_callgraph.json"),
    _p("unified_id_rtti.json"),
    _p("unified_id_vtable.json"),
]
DEFAULT_SYMBOLS = _p("config", "45410914", "symbols.txt")
DEFAULT_SPLITS = _p("config", "45410914", "splits.txt")
DEFAULT_OBJECTS = _p("config", "45410914", "objects.json")
DEFAULT_SRC_ROOT = _p("src")

SYM_FN_RE = re.compile(
    r"^(\S+) = \.text:0x([0-9A-Fa-f]+);.*type:function(?:.*size:0x([0-9A-Fa-f]+))?")
SPLITS_HEADER_RE = re.compile(r"^([A-Za-z0-9_]+\.cpp):\s*$")


# ---------------------------------------------------------------------------
# Consensus tiers (per execution-schedule.md S2.2 + exploratory-techniques §2,
# measured per-oracle precision). A tier is a (label, weight) the rank uses;
# higher weight = more trustworthy single-fn identification.
#   S  multi-oracle agreement OR bindiff sim==1.0  -> 0.98
#   A  callgraph cg_tier==multi                     -> 0.94 (exploratory §1.1)
#   B  rtti_tier==HIGH                              -> 0.80 (raw 71%, ~85% post-ICF)
#   C  callgraph single / bindiff sim<1.0           -> 0.75
#   C- rtti LOW / vtable-only / contested           -> review-only (excluded from auto-pin)
# ---------------------------------------------------------------------------
TIER_WEIGHT = {"S": 0.98, "A": 0.94, "B": 0.80, "C": 0.75, "C-": 0.41}
AUTOPIN_TIERS = ("S", "A", "B", "C")  # C- goes to the review bucket only


def load_symbols(path):
    """Return sorted list of (addr, size, name) for .text function symbols."""
    funcs = []
    with open(path, "r", errors="replace") as f:
        for line in f:
            m = SYM_FN_RE.match(line)
            if not m:
                continue
            addr = int(m.group(2), 16)
            size = int(m.group(3), 16) if m.group(3) else 0
            funcs.append((addr, size, m.group(1)))
    funcs.sort()
    return funcs


def overlap_regions(funcs):
    """Return a sorted list of (lo, hi) closed-open intervals that are unsafe to
    snap against because the STALE symbols.txt has overlapping / oversized
    (phantom) function symbols there (see project_jeff_asm_misnest.md). An
    interval covers a maximal run of mutually-overlapping symbols.
    """
    regions = []
    cur_lo = cur_hi = None
    for i in range(len(funcs)):
        a, s, _ = funcs[i]
        end = a + (s or 0)
        if cur_lo is None:
            cur_lo, cur_hi = a, end
            cur_overlap = False
            continue
        if a < cur_hi:  # this symbol starts before the running max-end -> overlap
            cur_overlap = True
            cur_hi = max(cur_hi, end)
        else:
            if cur_overlap:
                regions.append((cur_lo, cur_hi))
            cur_lo, cur_hi, cur_overlap = a, end, False
    if cur_lo is not None and cur_overlap:
        regions.append((cur_lo, cur_hi))
    return regions


def point_in_regions(regions, region_los, x):
    """True if address x lies inside any unsafe overlap region."""
    i = bisect.bisect_right(region_los, x) - 1
    if i < 0:
        return False
    lo, hi = regions[i]
    return lo <= x < hi


def parse_pinned_basenames(splits_path, objects_path):
    """Set of already-pinned cpp basenames (lowercased). A TU counts as pinned
    if it appears either as a splits.txt block header OR as an objects.json
    object entry (some are wired-but-unpinned but we still skip them — they are
    not NEW)."""
    pinned = set()
    with open(splits_path) as f:
        for line in f:
            m = SPLITS_HEADER_RE.match(line)
            if m:
                pinned.add(os.path.basename(m.group(1)).lower())
    with open(objects_path) as f:
        objects = json.load(f)
    for grp in objects.values():
        for relpath in (grp.get("objects") or {}):
            if relpath.endswith(".cpp"):
                pinned.add(os.path.basename(relpath).lower())
    return pinned


def build_src_index(src_root):
    """Two indexes over our src/ tree:
       rel_index : 'system/math/color.cpp' (lower) -> 'system/math/Color.cpp'
       base_index: 'color.cpp' (lower)            -> ['system/math/Color.cpp', ...]
    rel paths are relative to src_root (mirror objects.json's object keys).
    """
    rel_index = {}
    base_index = defaultdict(list)
    for root, _dirs, files in os.walk(src_root):
        for fn in files:
            if not fn.endswith(".cpp"):
                continue
            rel = os.path.relpath(os.path.join(root, fn), src_root).replace(os.sep, "/")
            rel_index[rel.lower()] = rel
            base_index[fn.lower()].append(rel)
    return rel_index, base_index


def src_tail(bindiff_src):
    """Path under the first '/src/' marker, e.g.
    '../dc3-decomp/src/system/rndobj/ShaderMgr.cpp' -> 'system/rndobj/ShaderMgr.cpp'.
    """
    p = (bindiff_src or "").replace("\\", "/")
    i = p.find("/src/")
    return p[i + len("/src/"):] if i >= 0 else p


def resolve_src(bindiff_src, rel_index, base_index, basename_compat=False):
    """Resolve a dc3 bindiff_src to our src/ rel-path.

    Two-stage, collision-aware (the default, RIGOROUS mode):
      1. exact path-tail under src/ (disambiguates rndobj/ vs rnddx9/ etc.)
      2. unambiguous basename fallback (single candidate) — this is how dc3's
         lazer/meta_ham/Foo.cpp resolves to our band3/meta_band/Foo.cpp.
    Returns (rel_path, method) or (None, reason).
      method in {'tail', 'basename'}; reason in {'absent', 'ambiguous'}.

    --basename-match COMPAT mode reproduces execution-schedule.md S2.4's yield
    snippet exactly: it groups purely by basename (first src/ candidate wins),
    so cross-dir collisions (XDK main.cpp lumped into our Main.cpp, rndobj/
    rnddx9 Utl.cpp/Movie.cpp/ShaderMgr.cpp conflated) inflate the count. Use it
    only to audit the divergence; the default mode is correct.
    """
    if not bindiff_src:
        return None, "no_src"
    if basename_compat:
        base = os.path.basename(bindiff_src).lower()
        cands = base_index.get(base)
        if not cands:
            return None, "absent"
        return cands[0], "basename_compat"
    tail = src_tail(bindiff_src).lower()
    if tail in rel_index:
        return rel_index[tail], "tail"
    base = os.path.basename(bindiff_src).lower()
    cands = base_index.get(base)
    if not cands:
        return None, "absent"
    if len(cands) == 1:
        return cands[0], "basename"
    return None, "ambiguous"  # collision; cannot safely attribute -> review


def group_for_rel(rel):
    """Infer the objects.json group from a src/ rel-path (mirrors how objects.json
    is partitioned). Used to emit the right objects.json append snippet."""
    r = rel.lower()
    if r.startswith("band3/") or r.startswith("network/"):
        return "band3"
    if r.startswith("system/hamobj/"):
        return "hamobj"
    if r.startswith("xdk/"):
        return "xdk"
    if "/" not in rel or r in ("main.cpp", "memory_xbox.cpp", "keygen_xbox.cpp"):
        return "main"
    return "engine"


def classify(addr_recs):
    """Given the list of oracle records that all identify a single rb3 address,
    return (tier, consensus_confidence, names, flags).

    addr_recs may carry source in {'bindiff','both','autoid','callgraph','rtti',
    'vtable'}. 'both'/'autoid' come from unified_id.json; 'bindiff' likewise.
    """
    sources = {r.get("source") for r in addr_recs}
    distinct_oracles = set()
    for r in addr_recs:
        s = r.get("source")
        if s in ("bindiff", "both"):
            distinct_oracles.add("bindiff")
        elif s == "autoid":
            distinct_oracles.add("autoid")
        elif s:
            distinct_oracles.add(s)

    names = {r.get("dc3_name") for r in addr_recs if r.get("dc3_name")}
    multi_oracle = len(distinct_oracles) >= 2
    disagree = len(names) >= 2

    # base single-oracle tier
    has_bindiff_perfect = any(
        r.get("source") in ("bindiff", "both") and (r.get("similarity") or 0) >= 1.0
        for r in addr_recs)
    has_bindiff_any = any(r.get("source") in ("bindiff", "both") for r in addr_recs)
    has_cg_multi = any(r.get("source") == "callgraph" and r.get("cg_tier") == "multi"
                       for r in addr_recs)
    has_cg_single = any(r.get("source") == "callgraph" and r.get("cg_tier") == "single"
                        for r in addr_recs)
    has_cg_majority = any(r.get("source") == "callgraph" and r.get("cg_tier") == "majority"
                          for r in addr_recs)
    has_rtti_high = any(r.get("source") == "rtti" and r.get("rtti_tier") == "HIGH"
                        for r in addr_recs)
    has_rtti_low = any(r.get("source") == "rtti" and r.get("rtti_tier") == "LOW"
                       for r in addr_recs)
    has_vtable = any(r.get("source") == "vtable" for r in addr_recs)

    # tier precedence (best-evidence wins)
    if has_bindiff_perfect:
        tier = "S"
    elif has_cg_multi:
        tier = "A"
    elif has_rtti_high:
        tier = "B"
    elif has_bindiff_any or has_cg_single or has_cg_majority:
        tier = "C"
    elif has_rtti_low or has_vtable:
        tier = "C-"
    else:
        tier = "C"  # autoid-only string attribution: weak but present

    # multi-oracle consensus promotes (S) when independent oracles agree
    if multi_oracle and not disagree and tier in ("A", "B", "C"):
        promoted = "S"
    else:
        promoted = tier

    base = max((r.get("confidence") or 0.0) for r in addr_recs)
    weight = TIER_WEIGHT[promoted]
    conf = max(base, weight)
    if multi_oracle and not disagree:
        conf = min(1.0, conf + 0.15)
    if disagree:
        conf = max(0.0, conf - 0.30)

    flags = []
    if disagree:
        flags.append("name_disagreement")
    if multi_oracle:
        flags.append("multi_oracle")

    return promoted, conf, sorted(names), flags, sorted(distinct_oracles)


def rank(args):
    # ---- 1. LOAD + MERGE all oracle records, group by rb3 address ----------
    by_addr = defaultdict(list)   # 'fn_addr' (lower hex addr) -> [records]
    n_total = 0
    per_oracle = Counter()
    for path in args.oracles:
        if not os.path.isfile(path):
            print(f"[pin] WARN: oracle missing, skipping: {path}", file=sys.stderr)
            continue
        recs = json.load(open(path))
        for r in recs:
            try:
                addr = int(r["rb3_addr"], 16)
            except (KeyError, ValueError):
                continue
            by_addr[addr].append(r)
            n_total += 1
            per_oracle[os.path.basename(path)] += 1
    print(f"[pin] merged {n_total} oracle records from {len(args.oracles)} files; "
          f"{len(by_addr)} unique addrs", file=sys.stderr)
    for k, v in per_oracle.items():
        print(f"[pin]   {k}: {v}", file=sys.stderr)

    # ---- 2. CONSENSUS per address -----------------------------------------
    # addr -> {tier, conf, names, flags, oracles, size, bindiff_src, dc3_name,
    #          dc3_name_demangled}
    addr_info = {}
    tier_global = Counter()
    for addr, recs in by_addr.items():
        tier, conf, names, flags, oracles = classify(recs)
        tier_global[tier] += 1
        # pick a representative bindiff_src/dc3_name: prefer a bindiff/both rec,
        # else the highest-confidence rec.
        rep = None
        for r in recs:
            if r.get("source") in ("bindiff", "both") and r.get("bindiff_src"):
                rep = r
                break
        if rep is None:
            rep = max(recs, key=lambda r: (r.get("confidence") or 0.0))
        size = max((r.get("size") or 0) for r in recs)
        addr_info[addr] = {
            "tier": tier,
            "conf": conf,
            "names": names,
            "flags": flags,
            "oracles": oracles,
            "size": size,
            "bindiff_src": rep.get("bindiff_src"),
            "dc3_name": rep.get("dc3_name"),
            "dc3_name_demangled": rep.get("dc3_name_demangled"),
        }

    # ---- 3. SOURCE-PRESENCE gate ------------------------------------------
    rel_index, base_index = build_src_index(args.src_root)
    pinned = parse_pinned_basenames(args.splits, args.objects)
    print(f"[pin] src/ cpp files: {len(rel_index)} ({len(base_index)} basenames); "
          f"already-pinned basenames: {len(pinned)}", file=sys.stderr)

    # addr -> (rel, method); review addrs collected separately
    gate_stats = Counter()
    addr_rel = {}
    review_addrs = []  # (addr, info, reason)
    for addr, info in addr_info.items():
        rel, method = resolve_src(info["bindiff_src"], rel_index, base_index,
                                  basename_compat=args.basename_match)
        if rel is None:
            gate_stats[method] += 1
            if method == "ambiguous":
                review_addrs.append((addr, info, "ambiguous_src"))
            continue
        gate_stats["present(%s)" % method] += 1
        addr_rel[addr] = (rel, method)
    print(f"[pin] source-present gate: {len(addr_rel)} addrs pass; "
          f"breakdown {dict(gate_stats)}", file=sys.stderr)

    # ---- 4. GROUP by TU, drop already-pinned ------------------------------
    tu_addrs = defaultdict(list)   # rel -> [addr]
    tu_method = {}                 # rel -> resolution method used
    for addr, (rel, method) in addr_rel.items():
        tu_addrs[rel].append(addr)
        tu_method.setdefault(rel, method)

    new_tus = {rel: addrs for rel, addrs in tu_addrs.items()
               if os.path.basename(rel).lower() not in pinned}
    dropped_pinned = len(tu_addrs) - len(new_tus)
    print(f"[pin] TUs with source-present oracle hits: {len(tu_addrs)}; "
          f"already-pinned dropped: {dropped_pinned}; NEW-pinnable: {len(new_tus)}",
          file=sys.stderr)

    # ---- 5. PROVISIONAL SPAN snap (symbols.txt boundaries) ----------------
    funcs = load_symbols(args.symbols)
    fn_los = [a for a, _, _ in funcs]
    fn_starts = set(fn_los)
    # end := start + size for each fn (snap target for the high endpoint)
    fn_ends = sorted({a + s for a, s, _ in funcs if s})
    regions = overlap_regions(funcs)
    region_los = [r[0] for r in regions]
    # Heuristic staleness signal: the documented jeff phantom (fn_82BF8E48,
    # size 0x94, straddling the ogg framing fns) is PRESENT iff symbols.txt is
    # pre-fix. Its absence means a post-fix re-SPLIT has run and the boundaries
    # are fresh (~1734 phantoms pruned). Either way the residual overlap regions
    # are still flagged; we just report which world we're in.
    fn_names = {n for _, _, n in funcs}
    symbols_stale = "fn_82BF8E48" in fn_names
    staleness = ("STALE — pre jeff phantom-fix; spans PROVISIONAL"
                 if symbols_stale
                 else "FRESH — post jeff phantom-fix re-SPLIT; residual overlap "
                      "regions still flagged")
    print(f"[pin] symbols.txt: {len(funcs)} fn syms, {len(regions)} overlap "
          f"regions ({staleness})", file=sys.stderr)

    def snap_lo(x):
        """Largest symbol-start <= x."""
        i = bisect.bisect_right(fn_los, x) - 1
        return fn_los[i] if i >= 0 else x

    def snap_hi(x):
        """Smallest symbol-end >= x."""
        i = bisect.bisect_left(fn_ends, x)
        return fn_ends[i] if i < len(fn_ends) else x

    def fns_in_range(lo, hi):
        l = bisect.bisect_left(fn_los, lo)
        r = bisect.bisect_left(fn_los, hi)
        return r - l

    def cluster_addrs(addrs_sorted, gap):
        """Split address-ordered hits into clusters whenever the gap between
        consecutive hits exceeds `gap`. Oracle hits for one dc3 TU are NOT one
        tight cluster — a TU has a DOMINANT contiguous cluster plus scattered
        ICF/inline-alias outliers elsewhere in the binary (verified: Rnd_Xbox
        has 30 of 45 hits in a 0x3490 span + 8 outlier singletons across 8MB).
        A bare [min,max] convex hull is therefore meaningless. We pin the
        dominant cluster and report the rest as outliers."""
        clusters = []
        cur = [addrs_sorted[0]]
        for a in addrs_sorted[1:]:
            if a - cur[-1] > gap:
                clusters.append(cur)
                cur = [a]
            else:
                cur.append(a)
        clusters.append(cur)
        return clusters

    candidates = []
    for rel, addrs in new_tus.items():
        addrs_sorted = sorted(set(addrs))
        n_oracle = len(addrs_sorted)

        # DOMINANT-cluster span (gap-based). Pick the cluster carrying the most
        # oracle fns; ties -> tighter span. Outliers (other clusters) are noted
        # but NOT spanned — they are pinned separately or re-attributed.
        clusters = cluster_addrs(addrs_sorted, args.cluster_gap)
        clusters.sort(key=lambda cl: (-len(cl), cl[-1] - cl[0]))
        primary = clusters[0]
        n_outliers = n_oracle - len(primary)
        n_clusters = len(clusters)

        raw_lo = primary[0]
        raw_hi = max(a + (addr_info[a]["size"] or 4) for a in primary)
        lo = snap_lo(raw_lo)
        hi = snap_hi(raw_hi)
        span = hi - lo
        total_fns = fns_in_range(lo, hi) or 1
        n_primary = len(primary)
        density = n_primary / total_fns

        # full-hull (for reference / the human report)
        full_lo = addrs_sorted[0]
        full_hi = max(a + (addr_info[a]["size"] or 4) for a in addrs_sorted)

        # jeff_blocked: either endpoint, or the raw extrema, touch an unsafe
        # overlap/phantom region. Such spans must be re-derived post-fix.
        jeff_blocked = (
            point_in_regions(regions, region_los, lo) or
            point_in_regions(regions, region_los, raw_lo) or
            point_in_regions(regions, region_los, raw_hi) or
            point_in_regions(regions, region_los, max(lo, hi - 1)))

        # snap-quality flags
        lo_on_boundary = lo in fn_starts
        hi_on_boundary = hi in fn_ends

        # tiers / weighting over ALL oracle fns for this TU (the source-coverage
        # signal), plus the primary-cluster subset (the byte-match signal).
        infos = [addr_info[a] for a in addrs_sorted]
        primary_infos = [addr_info[a] for a in primary]
        tiers = Counter(i["tier"] for i in infos)
        weighted = sum(i["conf"] for i in primary_infos)  # rank on pinnable span
        n_autopin = sum(1 for i in primary_infos if i["tier"] in AUTOPIN_TIERS)
        n_review = n_oracle - sum(1 for i in infos if i["tier"] in AUTOPIN_TIERS)

        samples = sorted(
            ((a, addr_info[a]["dc3_name_demangled"] or addr_info[a]["dc3_name"] or "?",
              addr_info[a]["tier"]) for a in primary),
            key=lambda t: t[0])[:6]

        candidates.append({
            "rel": rel,
            "group": group_for_rel(rel),
            "resolution": tu_method[rel],
            "n_oracle_fns": n_oracle,
            "n_primary_cluster_fns": n_primary,
            "n_outlier_fns": n_outliers,
            "n_clusters": n_clusters,
            "n_autopin_fns": n_autopin,
            "n_review_fns": n_review,
            "weighted_score": round(weighted, 3),
            "tiers": dict(tiers),
            "span_lo": f"0x{lo:08X}",
            "span_hi": f"0x{hi:08X}",
            "span_bytes": span,
            "raw_lo": f"0x{raw_lo:08X}",
            "raw_hi": f"0x{raw_hi:08X}",
            "full_hull_lo": f"0x{full_lo:08X}",
            "full_hull_hi": f"0x{full_hi:08X}",
            "full_hull_bytes": full_hi - full_lo,
            "fns_in_span": total_fns,
            "density": round(density, 4),
            "span_provisional": True,
            "jeff_blocked": jeff_blocked,
            "lo_on_symbol_boundary": lo_on_boundary,
            "hi_on_symbol_boundary": hi_on_boundary,
            "samples": [{"addr": f"0x{a:08X}", "name": n, "tier": t}
                        for a, n, t in samples],
        })

    # ---- 6. RANK: weighted oracle-fn count desc, tighter span as tiebreak --
    candidates.sort(key=lambda c: (-c["weighted_score"], -c["n_oracle_fns"],
                                   c["span_bytes"]))

    # ---- 7. EMIT ----------------------------------------------------------
    total_oracle_fns = sum(c["n_oracle_fns"] for c in candidates)
    total_primary = sum(c["n_primary_cluster_fns"] for c in candidates)
    total_outliers = sum(c["n_outlier_fns"] for c in candidates)
    total_autopin = sum(c["n_autopin_fns"] for c in candidates)
    n_jeff_blocked = sum(1 for c in candidates if c["jeff_blocked"])

    summary = {
        "n_new_pinnable_tus": len(candidates),
        "n_oracle_fns_in_new_tus": total_oracle_fns,
        "n_primary_cluster_fns": total_primary,
        "n_outlier_fns": total_outliers,
        "n_autopin_fns_in_primary": total_autopin,
        "n_jeff_blocked_tus": n_jeff_blocked,
        "cluster_gap_bytes": args.cluster_gap,
        "n_overlap_regions": len(regions),
        "tier_breakdown_global": dict(tier_global),
        "spans_provisional": True,
        "symbols_txt_stale": symbols_stale,
        "note": ("span_lo/span_hi is the DOMINANT contiguous oracle cluster per "
                 "TU (gap-split at cluster_gap), NOT the full hull — scattered "
                 "ICF/inline-alias outliers (n_outlier_fns) are excluded from the "
                 "span. " + ("symbols.txt is STALE (pre jeff phantom-symbol fix); "
                             "jeff_blocked TUs must be re-derived after a post-fix "
                             "re-SPLIT."
                             if symbols_stale else
                             "symbols.txt is FRESH (post jeff phantom-fix "
                             "re-SPLIT); jeff_blocked TUs touch RESIDUAL overlap "
                             "regions (legit tail-call/pdata-anchored mis-sizing, "
                             "not phantoms) — verify those spans by hand.") +
                 " C- tier (rtti-LOW / vtable / contested) is NOT in these "
                 "candidates — see the review file."),
    }
    out = {"summary": summary, "candidates": candidates}
    json.dump(out, open(args.out, "w"), indent=1)
    print(f"[pin] wrote {len(candidates)} ranked NEW-pinnable TUs -> {args.out}",
          file=sys.stderr)

    # review bucket: ambiguous-src + name-disagreement + C- tier addrs
    review = {"ambiguous_src": [], "name_disagreement": [], "c_minus_tier": []}
    for addr, info, reason in review_addrs:
        review["ambiguous_src"].append({
            "rb3_addr": f"0x{addr:08X}", "dc3_name": info["dc3_name"],
            "bindiff_src": info["bindiff_src"], "oracles": info["oracles"]})
    for addr, info in addr_info.items():
        if "name_disagreement" in info["flags"]:
            review["name_disagreement"].append({
                "rb3_addr": f"0x{addr:08X}", "names": info["names"],
                "oracles": info["oracles"], "tier": info["tier"]})
        if info["tier"] == "C-":
            review["c_minus_tier"].append({
                "rb3_addr": f"0x{addr:08X}", "dc3_name": info["dc3_name"],
                "bindiff_src": info["bindiff_src"], "oracles": info["oracles"]})
    json.dump(review, open(args.review, "w"), indent=1)
    print(f"[pin] review queue: ambiguous_src={len(review['ambiguous_src'])} "
          f"name_disagreement={len(review['name_disagreement'])} "
          f"c_minus={len(review['c_minus_tier'])} -> {args.review}", file=sys.stderr)

    # appendable splits.txt blob (ranked; provisional + jeff_blocked annotated)
    with open(args.splits_append, "w") as f:
        f.write("# Generated by tools/pin_candidates.py — PROVISIONAL spans "
                "(dominant oracle cluster per TU).\n")
        if symbols_stale:
            f.write("# DO NOT auto-pin. symbols.txt was STALE (pre jeff phantom-\n")
            f.write("# symbol fix); re-derive '# JEFF-BLOCKED' spans after a post-fix re-SPLIT.\n")
        else:
            f.write("# symbols.txt is FRESH (post jeff phantom-fix re-SPLIT). "
                    "'# JEFF-BLOCKED' spans touch\n")
            f.write("# RESIDUAL overlap regions (tail-call/pdata-anchored mis-sizing) "
                    "— hand-verify those.\n")
        f.write("# Pin only the .text line; dtk back-fills the matching .pdata.\n#\n")
        for c in candidates:
            tags = []
            if c["jeff_blocked"]:
                tags.append("JEFF-BLOCKED (re-derive post-fix)")
            if not c["lo_on_symbol_boundary"] or not c["hi_on_symbol_boundary"]:
                tags.append("endpoint not on symbol boundary")
            tag = ("  # " + "; ".join(tags)) if tags else ""
            f.write(f"# {os.path.basename(c['rel'])}: oracle_fns={c['n_oracle_fns']} "
                    f"(primary_cluster={c['n_primary_cluster_fns']} "
                    f"outliers={c['n_outlier_fns']} autopin={c['n_autopin_fns']}) "
                    f"weighted={c['weighted_score']} tiers={c['tiers']} "
                    f"span={c['span_bytes']:#x} density={c['density']:.1%} "
                    f"group={c['group']} res={c['resolution']}{tag}\n")
            f.write(f"{os.path.basename(c['rel'])}:\n")
            f.write(f"\t.text       start:{c['span_lo']} end:{c['span_hi']}\n\n")
    print(f"[pin] splits append blob -> {args.splits_append}", file=sys.stderr)

    # appendable objects.json snippet, grouped
    by_group = defaultdict(list)
    for c in candidates:
        by_group[c["group"]].append(c["rel"])
    with open(args.objects_append, "w") as f:
        f.write("// Generated by tools/pin_candidates.py — add to the matching\n")
        f.write("// objects.json group's \"objects\" dict (as NonMatching).\n")
        for grp in sorted(by_group):
            f.write(f"\n// group: {grp}\n")
            for rel in sorted(by_group[grp]):
                f.write(f'    "{rel}": "NonMatching",\n')
    print(f"[pin] objects append snippet -> {args.objects_append}", file=sys.stderr)

    # human-readable report
    with open(args.report, "w") as f:
        f.write("Unified oracle -> pin ranker (tools/pin_candidates.py)\n")
        f.write("=" * 72 + "\n\n")
        f.write("SUMMARY\n-------\n")
        f.write(f"  NEW-pinnable TUs           : {summary['n_new_pinnable_tus']}\n")
        f.write(f"  oracle fns in those TUs    : {summary['n_oracle_fns_in_new_tus']}\n")
        f.write(f"  in dominant cluster (span) : {summary['n_primary_cluster_fns']}\n")
        f.write(f"  scattered outliers (excl.) : {summary['n_outlier_fns']}\n")
        f.write(f"  auto-pin fns in span (SABC): {summary['n_autopin_fns_in_primary']}\n")
        f.write(f"  jeff_blocked TUs           : {summary['n_jeff_blocked_tus']} "
                f"(span touches a symbols.txt overlap region — verify/re-derive)\n")
        f.write(f"  cluster gap (bytes)        : {summary['cluster_gap_bytes']:#x}\n")
        f.write(f"  symbols.txt overlap regions: {summary['n_overlap_regions']}\n")
        f.write(f"  symbols.txt stale (pre-fix): {summary['symbols_txt_stale']}\n")
        f.write(f"  global tier breakdown      : {summary['tier_breakdown_global']}\n")
        f.write("  span_lo/hi = DOMINANT contiguous oracle cluster (NOT full hull).\n")
        f.write("  Spans are PROVISIONAL; jeff_blocked ones need hand-verification.\n\n")
        f.write("TIERS\n-----\n")
        f.write("  S  multi-oracle agreement OR bindiff sim=1.0  (~0.98)\n")
        f.write("  A  callgraph multi-caller                     (0.94)\n")
        f.write("  B  rtti HIGH (matching slot counts)           (0.80)\n")
        f.write("  C  callgraph single / bindiff sim<1.0 / autoid(0.75)\n")
        f.write("  C- rtti LOW / vtable-only / contested -> REVIEW ONLY (excluded)\n\n")
        f.write("RANKED NEW-PINNABLE TUs (by consensus-weighted oracle-fn count)\n")
        f.write("-" * 72 + "\n")
        for i, c in enumerate(candidates, 1):
            blk = " [JEFF-BLOCKED]" if c["jeff_blocked"] else ""
            f.write(f"{i:3d}. {os.path.basename(c['rel']):34s} "
                    f"fns={c['n_oracle_fns']:3d} "
                    f"(prim={c['n_primary_cluster_fns']:3d} "
                    f"out={c['n_outlier_fns']:3d} autopin={c['n_autopin_fns']:3d}) "
                    f"wt={c['weighted_score']:6.1f} tiers={str(c['tiers']):26s} "
                    f"span={c['span_bytes']:#07x} dens={c['density']:5.1%} "
                    f"{c['group']:6s}{blk}\n")
            f.write(f"       rel={c['rel']}  res={c['resolution']}  "
                    f"span={c['span_lo']}..{c['span_hi']}  "
                    f"full_hull={c['full_hull_bytes']:#x}\n")
    print(f"[pin] human report -> {args.report}", file=sys.stderr)

    # ---- 8. CONSOLE: top 10 + validation vs plan -------------------------
    print(file=sys.stderr)
    print(f"[pin] TOP 10 NEW-pinnable TUs (rank by weighted oracle-fn count):",
          file=sys.stderr)
    for i, c in enumerate(candidates[:10], 1):
        blk = " [JEFF-BLOCKED]" if c["jeff_blocked"] else ""
        print(f"[pin]   {i:2d}. {os.path.basename(c['rel']):30s} "
              f"fns={c['n_oracle_fns']:3d} prim={c['n_primary_cluster_fns']:3d} "
              f"out={c['n_outlier_fns']:3d} autopin={c['n_autopin_fns']:3d} "
              f"span={c['span_bytes']:#07x} wt={c['weighted_score']:6.1f}{blk}",
              file=sys.stderr)
    print(file=sys.stderr)
    print(f"[pin] VALIDATION vs execution-schedule.md S2.4 (~194 TUs / ~981 fns): "
          f"got {summary['n_new_pinnable_tus']} TUs / "
          f"{summary['n_oracle_fns_in_new_tus']} oracle fns", file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    pr = sub.add_parser("rank", help="merge oracles -> rank NEW-pinnable TUs")
    pr.add_argument("--oracles", nargs="+", default=DEFAULT_ORACLES)
    pr.add_argument("--symbols", default=DEFAULT_SYMBOLS)
    pr.add_argument("--splits", default=DEFAULT_SPLITS)
    pr.add_argument("--objects", default=DEFAULT_OBJECTS)
    pr.add_argument("--src-root", default=DEFAULT_SRC_ROOT)
    pr.add_argument("--basename-match", action="store_true",
                    help="COMPAT: group by basename only (reproduces "
                         "execution-schedule.md S2.4's inflated ~194/981; the "
                         "default collision-aware tail resolution is correct)")
    pr.add_argument("--cluster-gap", type=lambda x: int(x, 0), default=0x10000,
                    help="gap (bytes) that splits a TU's oracle hits into "
                         "clusters; the dominant cluster becomes the pinned span "
                         "(default 0x10000=64KiB)")
    pr.add_argument("--out", default=_p("pin_candidates.json"))
    pr.add_argument("--report", default=_p("pin_candidates_report.txt"))
    pr.add_argument("--review", default=_p("pin_candidates_review.json"))
    pr.add_argument("--splits-append", default=_p("pin_candidates.splits.append"))
    pr.add_argument("--objects-append", default=_p("pin_candidates.objects.append"))
    pr.set_defaults(func=rank)
    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
