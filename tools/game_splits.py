#!/usr/bin/env python3
"""game_splits.py — derive TARGET-ONLY .text splits for GAME TUs from the
rb3-Wii cross-binary bindiff oracle (unified_id_rb3wii.json).

Why this exists separately from pin_candidates.py: that tool has a
source-present gate (it only proposes TUs whose .cpp already exists in our
src/ tree, so they can be compiled+matched). For GAME code only ~41 of 267
oracle-identified TUs have source yet, so it yields almost nothing. But a
splits .text range produces a dtk TARGET .obj regardless of whether we have
source — and a target .obj is exactly what defines "what our target is" and
what grows the Game-Code denominator (verified: RockCentral.cpp is pinned +
counted with no source via project.py warn_missing_source).

So this keeps the sourceless TUs. It reuses pin_candidates' symbol-boundary
snapping + dominant-cluster gap-split, adds strict non-overlap validation
against already-pinned ranges AND between candidates, and emits append blobs
for splits.txt + objects.json plus a manifest.

Spans are PROVISIONAL: derived from the dominant contiguous cluster of oracle
fns per TU. Because RB3 preserves TU spatial grouping (.text is a concatenation
of contiguous per-TU blocks, no LTCG reordering), every function between a TU's
first and last oracle fn belongs to that TU — so the snapped [lo,hi) captures
the full extent BETWEEN the oracle endpoints (missing only TU code outside them).
"""
import argparse, bisect, json, os, re, sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
def _p(*a): return os.path.join(ROOT, *a)

SYM_FN_RE = re.compile(
    r"^(\S+) = \.text:0x([0-9A-Fa-f]+);.*type:function(?:.*size:0x([0-9A-Fa-f]+))?")
SPLITS_HEADER_RE = re.compile(r"^([A-Za-z0-9_]+\.cpp):\s*$")
SPLITS_TEXT_RE = re.compile(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")


def load_symbols(path):
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


def load_pinned_text_ranges(splits_path):
    """Return sorted list of (lo, hi) .text ranges already pinned, plus the set
    of already-used basenames."""
    ranges, names = [], set()
    cur = None
    with open(splits_path) as f:
        for line in f:
            hm = SPLITS_HEADER_RE.match(line)
            if hm:
                cur = hm.group(1)
                names.add(cur.lower())
                continue
            tm = SPLITS_TEXT_RE.search(line)
            if tm and cur:
                ranges.append((int(tm.group(1), 16), int(tm.group(2), 16)))
    ranges.sort()
    return ranges, names


def map_oracle_src(bindiff_src):
    """rb3-Wii build path -> (our objects.json rel path, group, basename).
    'band3/src/game/Stats.cpp'  -> ('band3/game/Stats.cpp', 'band3')
    'network/src/net/NetSession.cpp' -> ('network/net/NetSession.cpp', 'network')
    """
    p = bindiff_src.replace("\\", "/")
    parts = p.split("/")
    if len(parts) >= 2 and parts[1] == "src":
        rel = parts[0] + "/" + "/".join(parts[2:])
    else:
        rel = p
    group = "band3" if rel.startswith("band3/") else (
        "network" if rel.startswith("network/") else "?")
    return rel, group, os.path.basename(rel)


def overlaps(lo, hi, ranges, range_los):
    """True if [lo,hi) intersects any (rlo,rhi) in ranges (sorted by rlo)."""
    i = bisect.bisect_right(range_los, lo) - 1
    for j in (i, i + 1):
        if 0 <= j < len(ranges):
            rlo, rhi = ranges[j]
            if lo < rhi and rlo < hi:
                return (rlo, rhi)
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--oracle", default=_p("unified_id_rb3wii.json"))
    ap.add_argument("--symbols", default=_p("config", "45410914", "symbols.txt"))
    ap.add_argument("--splits", default=_p("config", "45410914", "splits.txt"))
    ap.add_argument("--min-conf", type=float, default=0.90)
    ap.add_argument("--min-fns", type=int, default=1,
                    help="only emit TUs with >= this many oracle fns")
    ap.add_argument("--cluster-gap", type=lambda x: int(x, 0), default=0x10000)
    ap.add_argument("--out", default=_p("game_splits.json"))
    ap.add_argument("--splits-append", default=_p("game_splits.append"))
    ap.add_argument("--objects-append", default=_p("game_splits.objects.append"))
    args = ap.parse_args()

    oracle = json.load(open(args.oracle))
    funcs = load_symbols(args.symbols)
    fn_los = [a for a, _, _ in funcs]
    fn_starts = set(fn_los)
    fn_ends = sorted({a + s for a, s, _ in funcs if s})
    size_by_addr = {a: s for a, s, _ in funcs}
    pinned, pinned_names = load_pinned_text_ranges(args.splits)
    pinned_los = [r[0] for r in pinned]
    print(f"[game] symbols: {len(funcs)} fns | pinned .text ranges: {len(pinned)} "
          f"| pinned basenames: {len(pinned_names)}", file=sys.stderr)

    def snap_lo(x):
        i = bisect.bisect_right(fn_los, x) - 1
        return fn_los[i] if i >= 0 else x

    def snap_hi(x):
        i = bisect.bisect_left(fn_ends, x)
        return fn_ends[i] if i < len(fn_ends) else x

    def fns_in_range(lo, hi):
        return bisect.bisect_left(fn_los, hi) - bisect.bisect_left(fn_los, lo)

    def gap_split(addrs, gap):
        clusters, cur = [], [addrs[0]]
        for a in addrs[1:]:
            if a - cur[-1] > gap:
                clusters.append(cur); cur = [a]
            else:
                cur.append(a)
        clusters.append(cur)
        return clusters

    # group oracle fns by TU (band3/network only, conf gate)
    by_tu = defaultdict(list)
    for e in oracle:
        if e.get("confidence", 0) < args.min_conf:
            continue
        src = e.get("bindiff_src") or ""
        if not (("band3/" in src) or ("network/" in src)):
            continue
        try:
            addr = int(e["rb3_addr"], 16)
        except (KeyError, ValueError):
            continue
        rel, group, base = map_oracle_src(src)
        by_tu[(rel, group, base)].append((addr, e))

    cands = []
    for (rel, group, base), recs in by_tu.items():
        if base.lower() in pinned_names:
            continue  # already pinned (by basename — dtk matches blocks by basename)
        addrs = sorted({a for a, _ in recs})
        if len(addrs) < args.min_fns:
            continue
        clusters = gap_split(addrs, args.cluster_gap)
        clusters.sort(key=lambda c: (-len(c), c[-1] - c[0]))
        primary = clusters[0]
        raw_lo = primary[0]
        raw_hi = max(a + (size_by_addr.get(a) or 4) for a in primary)
        lo, hi = snap_lo(raw_lo), snap_hi(raw_hi)
        if hi <= lo:
            continue
        total = fns_in_range(lo, hi) or 1
        cands.append({
            "rel": rel, "group": group, "base": base,
            "n_oracle_fns": len(addrs), "n_primary": len(primary),
            "n_outliers": len(addrs) - len(primary), "n_clusters": len(clusters),
            "lo": lo, "hi": hi, "span_bytes": hi - lo, "fns_in_span": total,
            "density": round(len(primary) / total, 4),
            "lo_on_boundary": lo in fn_starts, "hi_on_boundary": hi in fn_ends,
            "max_conf": round(max(e.get("confidence", 0) for _, e in recs), 4),
            "names": sorted({(e.get("wii_name") or "?") for _, e in recs})[:8],
        })

    # ---- non-overlap validation: vs pinned, then greedily among candidates ----
    cands.sort(key=lambda c: (-c["n_oracle_fns"], c["span_bytes"]))
    accepted, acc_ranges = [], []
    rej_pinned = rej_cand = rej_basedup = 0
    seen_base = set()
    for c in cands:
        hit = overlaps(c["lo"], c["hi"], pinned, pinned_los)
        if hit:
            c["reject"] = f"overlaps_pinned 0x{hit[0]:08X}-0x{hit[1]:08X}"
            rej_pinned += 1; continue
        acc_los = sorted(r[0] for r in acc_ranges)
        hit = overlaps(c["lo"], c["hi"], sorted(acc_ranges), acc_los)
        if hit:
            c["reject"] = f"overlaps_candidate 0x{hit[0]:08X}-0x{hit[1]:08X}"
            rej_cand += 1; continue
        if c["base"].lower() in seen_base:
            c["reject"] = "duplicate_basename"
            rej_basedup += 1; continue
        seen_base.add(c["base"].lower())
        accepted.append(c); acc_ranges.append((c["lo"], c["hi"]))

    accepted.sort(key=lambda c: (-c["n_oracle_fns"], c["span_bytes"]))
    json.dump({"accepted": accepted,
               "rejected": [c for c in cands if "reject" in c],
               "summary": {
                   "n_accepted": len(accepted),
                   "n_rejected_overlap_pinned": rej_pinned,
                   "n_rejected_overlap_candidate": rej_cand,
                   "n_rejected_basedup": rej_basedup,
                   "accepted_bytes": sum(c["span_bytes"] for c in accepted),
                   "accepted_target_fns": sum(c["fns_in_span"] for c in accepted),
                   "min_conf": args.min_conf, "min_fns": args.min_fns,
               }}, open(args.out, "w"), indent=1)

    with open(args.splits_append, "w") as f:
        f.write("# Generated by tools/game_splits.py — PROVISIONAL target-only "
                "GAME spans (rb3-Wii bindiff oracle, dominant cluster).\n"
                "# Pin only the .text line; dtk back-fills the matching .pdata.\n\n")
        for c in accepted:
            f.write(f"# {c['base']}: oracle_fns={c['n_oracle_fns']} "
                    f"primary={c['n_primary']} span=0x{c['span_bytes']:x} "
                    f"fns_in_span={c['fns_in_span']} density={c['density']*100:.0f}% "
                    f"group={c['group']} maxconf={c['max_conf']}\n")
            f.write(f"{c['rel']}:\n\t.text       "
                    f"start:0x{c['lo']:08X} end:0x{c['hi']:08X}\n\n")

    with open(args.objects_append, "w") as f:
        f.write("// Generated by tools/game_splits.py — add to the matching "
                "objects.json group (as NonMatching).\n")
        for grp in ("band3", "network"):
            g = [c for c in accepted if c["group"] == grp]
            if not g:
                continue
            f.write(f"\n// group: {grp}\n")
            for c in g:
                f.write(f'    "{c["rel"]}": "NonMatching",\n')

    s = json.load(open(args.out))["summary"]
    print(f"[game] accepted {s['n_accepted']} TUs | "
          f"target bytes={s['accepted_bytes']:,} "
          f"target fns={s['accepted_target_fns']:,}", file=sys.stderr)
    print(f"[game] rejected: overlap_pinned={s['n_rejected_overlap_pinned']} "
          f"overlap_candidate={s['n_rejected_overlap_candidate']} "
          f"basedup={s['n_rejected_basedup']}", file=sys.stderr)
    print(f"[game] -> {args.out}\n[game] -> {args.splits_append}\n"
          f"[game] -> {args.objects_append}", file=sys.stderr)


if __name__ == "__main__":
    main()
