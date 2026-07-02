#!/usr/bin/env python3
"""oracle_contiguity_scan.py — stub-filtered contiguity scanner for option-C
port-then-pin target selection.

Ranks UNPINNED DC3-oracle TUs by *honest matchable real-body bytes* in free
splits.txt space, so target selection is driven by real code (not funclet- or
stub-fold-inflated oracle counts).

Build-first, stdlib-only. Inputs (all relative to the repo root that contains
this tool):
  - dc3_oracle.json                     : {dc3_va, rb3_va, dc3_name, dc3_tu,
                                            similarity, confidence} x ~34k
  - config/45410914/symbols.txt         : real fn sizes
                                            (fn_<VA> = .text:0x<VA>; // ... size:0xHEX)
  - config/45410914/splits.txt          : pinned .text ranges
  - scripts/target_symbol_map.json      : existing names (corroboration)
  - fingerprints.json (optional)        : unused fallback for sizes

Oracle C++ source trees (for the source-existence annotation) are resolved from
a candidate list so the tool works from a worktree whose parent has no siblings.

Algorithm
---------
1. Split oracle rows into funclet rows (dc3_name startswith "__unwind") and body
   rows. Funclet islands (>=8 contiguous funclets for one dc3_tu) are kept as a
   SECONDARY signal ("a TU with EH lives adjacent"), never as honest bytes.
2. Drop body rows whose RB3 fn size <= 64 bytes AND sim < 0.9 (stub-fold decoys;
   the icf_alias_check 44-byte lesson, padded to 64).
3. Cluster surviving body rows per dc3_tu by rb3_va gap <= 0x1000.
4. Intersect clusters with UNPINNED splits.txt gaps; honest matchable body bytes
   = sum of symbols.txt sizes of real-bodied cluster members in unpinned space.
5. Rank TUs by honest_bytes x cluster density (unpinned real-body member count);
   annotate with source existence in dc3/rb3 trees, count of members already in
   target_symbol_map.json, and the gap's pinned neighbors.

Modes:
  (default)      print the ranked table of fresh unpinned TUs.
  --validate     run the self-check gates and exit non-zero on failure.
  --tu NAME      dump the per-member detail for one dc3_tu (e.g. NavListNode.obj).
  --top N        limit the ranked table to N rows (default 30).
  --json         emit machine-readable JSON instead of the table.
"""
import argparse
import json
import os
import re
import sys
from collections import defaultdict

STUB_SIZE_MAX = 64          # <= this AND sim < STUB_SIM => stub-fold decoy
STUB_SIM = 0.9
CLUSTER_GAP = 0x1000        # rb3_va gap that splits a cluster
FUNCLET_ISLAND_MIN = 8      # contiguous funclets => "TU with EH lives adjacent"
REAL_BODY_MIN = 0x2D        # > 44 bytes = a real body (icf 44-byte fold rule)

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _first_existing(cands):
    for c in cands:
        if c and os.path.isdir(c):
            return c
    return None


def resolve_oracle_src():
    dc3 = _first_existing([
        os.environ.get("DC3_SRC"),
        os.path.join(REPO_ROOT, "..", "dc3-decomp", "src"),
        "/home/free/code/milohax/dc3-decomp/src",
    ])
    rb3 = _first_existing([
        os.environ.get("RB3WII_SRC"),
        os.path.join(REPO_ROOT, "..", "rb3", "src"),
        "/home/free/code/milohax/rb3/src",
    ])
    return dc3, rb3


def load_oracle(path):
    with open(path) as f:
        return json.load(f)


def load_symbol_sizes(path):
    """VA(int) -> size(int) for type:function .text symbols."""
    sizes = {}
    rx = re.compile(
        r"^\S+\s*=\s*\.text:0x([0-9A-Fa-f]+);\s*//\s*type:function\s+size:0x([0-9A-Fa-f]+)")
    with open(path) as f:
        for line in f:
            m = rx.match(line)
            if m:
                sizes[int(m.group(1), 16)] = int(m.group(2), 16)
    return sizes


def load_pinned_text(path):
    """Return sorted list of (start,end,unit) pinned .text ranges."""
    ranges = []
    unit = None
    hdr = re.compile(r"^(\S.*?):\s*$")
    txt = re.compile(r"^\s+\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)\s*$")
    with open(path) as f:
        for line in f:
            m = hdr.match(line)
            if m:
                unit = m.group(1)
                continue
            m = txt.match(line)
            if m and unit:
                ranges.append((int(m.group(1), 16), int(m.group(2), 16), unit))
    ranges.sort()
    return ranges


def load_target_map(path):
    if not os.path.exists(path):
        return set()
    with open(path) as f:
        d = json.load(f)
    out = set()
    for k in d:
        try:
            out.add(int(k, 16))
        except ValueError:
            pass
    return out


def basename_cpp(dc3_tu):
    """meta_ham:NavListNode.obj -> NavListNode.cpp"""
    base = (dc3_tu or "").split(":")[-1]
    if base.endswith(".obj"):
        base = base[:-4] + ".cpp"
    return base


def index_sources(root):
    """basename(lower) -> list of full paths, recursive."""
    idx = defaultdict(list)
    if not root:
        return idx
    for dp, _dn, fn in os.walk(root):
        for name in fn:
            if name.endswith((".cpp", ".c")):
                idx[name.lower()].append(os.path.join(dp, name))
    return idx


def pinned_units_by_cpp(pinned):
    """set of .cpp basenames that have >=1 pinned .text range."""
    out = set()
    for _s, _e, unit in pinned:
        out.add(os.path.basename(unit))
    return out


def va_in_pinned(va, pinned):
    for s, e, unit in pinned:
        if s <= va < e:
            return unit
        if va < s:
            break
    return None


def gap_neighbors(lo, hi, pinned):
    """pinned units immediately below lo and above hi."""
    below = above = None
    for s, e, unit in pinned:
        if e <= lo:
            below = (unit, s, e)
        if s >= hi and above is None:
            above = (unit, s, e)
    return below, above


def cluster_rows(rows):
    """rows: list of dict with 'va' int; split on gap>CLUSTER_GAP. sorted."""
    rows = sorted(rows, key=lambda r: r["va"])
    clusters = []
    cur = []
    for r in rows:
        if cur and r["va"] - cur[-1]["va"] > CLUSTER_GAP:
            clusters.append(cur)
            cur = []
        cur.append(r)
    if cur:
        clusters.append(cur)
    return clusters


def funclet_islands(funclet_rows):
    """dc3_tu -> max contiguous funclet-island size (>=FUNCLET_ISLAND_MIN)."""
    per = defaultdict(int)
    by_tu = defaultdict(list)
    for r in funclet_rows:
        by_tu[r["dc3_tu"]].append(r["va"])
    for tu, vas in by_tu.items():
        for cl in cluster_rows([{"va": v} for v in vas]):
            per[tu] = max(per[tu], len(cl))
    return {tu: n for tu, n in per.items() if n >= FUNCLET_ISLAND_MIN}


def analyze(oracle, sizes, pinned, tmap):
    funclet_rows = []
    body_rows = []
    for e in oracle:
        name = e.get("dc3_name") or ""
        try:
            va = int(e["rb3_va"], 16)
        except (KeyError, ValueError):
            continue
        row = {
            "va": va,
            "dc3_tu": e.get("dc3_tu", ""),
            "name": name,
            "sim": float(e.get("similarity", 0.0)),
            "size": sizes.get(va, 0),
        }
        if name.startswith("__unwind"):
            funclet_rows.append(row)
        else:
            body_rows.append(row)

    islands = funclet_islands(funclet_rows)

    # step 2: stub-fold drop
    kept = [r for r in body_rows
            if not (r["size"] <= STUB_SIZE_MAX and r["sim"] < STUB_SIM)]

    per_tu = defaultdict(list)
    for r in kept:
        per_tu[r["dc3_tu"]].append(r)

    pinned_cpps = pinned_units_by_cpp(pinned)

    results = []
    for tu, rows in per_tu.items():
        clusters = cluster_rows(rows)
        cpp = basename_cpp(tu)
        tu_pinned_members = 0
        # Best UNPINNED contiguous cluster (single free .text region) is the
        # honest port target. Summing across scattered clusters would count
        # ICF decoys (e.g. Sound.obj's binary-wide low-sim scatter) as bytes.
        best = None  # (honest_bytes, members, unp_list)
        best_key = (-1, -1)
        for cl in clusters:
            unp = [r for r in cl if va_in_pinned(r["va"], pinned) is None]
            pin = [r for r in cl if va_in_pinned(r["va"], pinned) is not None]
            tu_pinned_members += len(pin)
            reals = [r for r in unp if r["size"] >= REAL_BODY_MIN]
            hbytes = sum(r["size"] for r in reals)
            key = (len(reals), hbytes)
            if key > best_key:
                best_key = key
                best = (hbytes, reals, unp)

        best_honest = best[0] if best else 0
        best_reals = best[1] if best else []
        best_members = len(best_reals)
        mean_sim = (sum(r["sim"] for r in best_reals) / best_members
                    if best_members else 0.0)
        # names already content-matched (byte-exact) inside the cluster are the
        # single strongest port-success predictor — reward them.
        named_in_cluster = sum(1 for r in best_reals if r["va"] in tmap)

        # consumed = a splits.txt unit with this TU's .cpp basename is pinned.
        # (We do NOT use "most members pinned": ICF/misattribution scatters a
        # TU's low-sim decoy rows across OTHER TUs' pinned ranges — NavListNode
        # has 13 such scattered pinned decoys yet its own dense cluster is free.)
        pinned_by_name = cpp in pinned_cpps
        consumed = pinned_by_name

        named = sum(1 for r in rows if r["va"] in tmap)

        lo = min((r["va"] for r in best_reals), default=None)
        hi = max((r["va"] + max(r["size"], 4) for r in best_reals), default=None)
        gap_below = gap_above = None
        if lo is not None:
            gap_below, gap_above = gap_neighbors(lo, hi, pinned)

        # score: honest bytes x mean-sim confidence x content-match corroboration
        score = int(best_honest * (0.5 + mean_sim) * (1 + named_in_cluster))

        results.append({
            "tu": tu,
            "cpp": cpp,
            "honest_bytes": best_honest,
            "unpinned_real_members": best_members,
            "pinned_members": tu_pinned_members,
            "density": best_members,
            "mean_sim": round(mean_sim, 3),
            "score": score,
            "named_in_map": named,
            "named_in_cluster": named_in_cluster,
            "consumed": consumed,
            "pinned_by_name": pinned_by_name,
            "funclet_island": islands.get(tu, 0),
            "best_cluster_lo": lo,
            "best_cluster_hi": hi,
            "gap_below": gap_below,
            "gap_above": gap_above,
        })
    return results, islands


def _match_src(paths, prefix):
    """A source path counts only if the dc3_tu dir-prefix appears as a path
    segment — kills basename collisions (xgraphics:scheduler.obj vs an rb3
    scheduler.cpp). Prefix-less TUs accept any basename hit."""
    if not paths:
        return None
    if not prefix:
        return paths[0]
    for p in paths:
        segs = p.lower().split(os.sep)
        if prefix.lower() in segs:
            return p
    return None


def annotate_sources(results, dc3_idx, rb3_idx):
    for r in results:
        base = r["cpp"].lower()
        prefix = (r["tu"] or "").split(":")[0] if ":" in (r["tu"] or "") else ""
        dc3 = _match_src(dc3_idx.get(base), prefix)
        rb3 = _match_src(rb3_idx.get(base), prefix)
        r["src_dc3"] = dc3 is not None
        r["src_rb3"] = rb3 is not None
        r["src_dc3_path"] = dc3
        r["src_rb3_path"] = rb3


MIN_CLUSTER_MEMBERS = 3  # a real portable cluster is contiguous & dense


def fresh_ranked(results):
    """Unpinned, portable (source-available) TUs with a dense contiguous
    free-space cluster, ranked source-first then by score desc.

    Requiring a single contiguous cluster of >=3 real bodies is what rejects
    ICF-decoy TUs like Sound.obj (23 low-sim rows scattered binary-wide, each a
    singleton cluster). Source availability is primary because a TU with no
    oracle source cannot be ported (drops the xgraphics/d3dx9 mega-libs)."""
    fresh = [r for r in results
             if not r["consumed"]
             and (r["src_dc3"] or r["src_rb3"])
             and (r["unpinned_real_members"] >= MIN_CLUSTER_MEMBERS
                  or (r["unpinned_real_members"] >= 2 and r["mean_sim"] >= 0.9))]
    fresh.sort(key=lambda r: (r["score"], r["honest_bytes"]), reverse=True)
    return fresh


def fmt_va(v):
    return f"0x{v:08X}" if v is not None else "-"


def print_table(fresh, top, islands):
    print(f"{'rank':>4}  {'score':>7}  {'honest':>6}  {'#bod':>4}  {'sim':>5}  "
          f"{'cnm':>3}  {'src':>3}  {'cluster span':>23}  TU")
    print("-" * 108)
    for i, r in enumerate(fresh[:top], 1):
        src = ("D" if r["src_dc3"] else "-") + ("W" if r["src_rb3"] else "-")
        span = f"{fmt_va(r['best_cluster_lo'])}-{fmt_va(r['best_cluster_hi'])}"
        isl = f"  [+EH:{r['funclet_island']}]" if r["funclet_island"] else ""
        print(f"{i:>4}  {r['score']:>7}  {r['honest_bytes']:>6}  "
              f"{r['unpinned_real_members']:>4}  {r['mean_sim']:>5.3f}  "
              f"{r['named_in_cluster']:>3}  {src:>3}  {span:>23}  {r['tu']}{isl}")
    print()
    print("(honest = matchable real-body bytes in unpinned space; #bod = unpinned "
          "real bodies;\n cnm = cluster members already content-matched in target "
          "map; src D=dc3 W=rb3-Wii;\n [+EH:n] = adjacent n-funclet EH island)")


def print_tu_detail(oracle, sizes, pinned, tmap, want):
    want = want.lower()
    rows = []
    for e in oracle:
        tu = e.get("dc3_tu") or ""
        if want not in tu.lower():
            continue
        try:
            va = int(e["rb3_va"], 16)
        except (KeyError, ValueError):
            continue
        rows.append({
            "va": va, "tu": tu, "name": e.get("dc3_name", ""),
            "sim": float(e.get("similarity", 0.0)), "size": sizes.get(va, 0),
        })
    rows.sort(key=lambda r: r["va"])
    print(f"{'rb3_va':>10}  {'size':>6}  {'sim':>5}  {'pin':>3}  {'map':>3}  name")
    print("-" * 110)
    for r in rows:
        funclet = r["name"].startswith("__unwind")
        stub = (r["size"] <= STUB_SIZE_MAX and r["sim"] < STUB_SIM)
        pin = va_in_pinned(r["va"], pinned)
        mark = "F" if funclet else ("s" if stub else " ")
        print(f"{fmt_va(r['va'])}  0x{r['size']:04X}  {r['sim']:.3f}  "
              f"{'Y' if pin else '.':>3}  {'Y' if r['va'] in tmap else '.':>3}  "
              f"{mark} {r['name'][:78]}")
        if pin:
            print(f"            ^ pinned in {pin}")


# Substantial targets (>=5 real bodies) must rank in the top 10.
VALIDATE_SUBSTANTIAL = {"MoggClip.obj", "NavListNode.obj"}
# Small real targets (2-3 bodies) must be SELECTED (present in fresh list), but
# rank mid-list because they are objectively tiny — forcing them above larger
# real clusters would be dishonest.
VALIDATE_SMALL = {"MotionBlur.obj", "SoftParticles.obj"}
VALIDATE_NOT = {"Sound.obj", "SongInfoAudioType.obj"}
VALIDATE_CONSUMED = {"AccomplishmentProgress.obj", "MetaMusic.obj"}
SUBSTANTIAL_TOP_N = 10


def run_validate(results, fresh):
    def base(tu):
        return (tu or "").split(":")[-1]

    ranks = {base(r["tu"]): i for i, r in enumerate(fresh, 1)}
    all_fresh_bases = set(ranks)
    by_base = defaultdict(list)
    for r in results:
        by_base[base(r["tu"])].append(r)

    ok = True
    print("=== VALIDATION GATES ===\n")

    print(f"[gate 1a] substantial targets rank in top {SUBSTANTIAL_TOP_N}:")
    for name in sorted(VALIDATE_SUBSTANTIAL):
        rank = ranks.get(name)
        good = rank is not None and rank <= SUBSTANTIAL_TOP_N
        if not good:
            ok = False
        print(f"   {'PASS' if good else 'FAIL'}  {name:<24} rank={rank}")

    print("\n[gate 1b] small real targets are SELECTED (present in fresh list):")
    for name in sorted(VALIDATE_SMALL):
        rank = ranks.get(name)
        good = rank is not None
        if not good:
            ok = False
        print(f"   {'PASS' if good else 'FAIL'}  {name:<24} rank={rank}"
              f"  (mid-list is correct: tiny TU)")

    print("\n[gate 2] funclet-only TUs must NOT rank (no honest contiguous body):")
    for name in sorted(VALIDATE_NOT):
        present = name in all_fresh_bases
        status = "PASS" if not present else "FAIL"
        if present:
            ok = False
        hb = max((r["honest_bytes"] for r in by_base.get(name, [])), default=0)
        isl = max((r["funclet_island"] for r in by_base.get(name, [])), default=0)
        print(f"   {status}  {name:<24} honest_bytes={hb} funclet_island={isl} "
              f"in_ranked={present}")

    print("\n[gate 3] known-consumed TUs show as consumed/pinned:")
    for name in sorted(VALIDATE_CONSUMED):
        recs = by_base.get(name, [])
        consumed = any(r["consumed"] for r in recs)
        status = "PASS" if consumed else "FAIL"
        if not consumed:
            ok = False
        pinned_by_name = any(r["pinned_by_name"] for r in recs)
        pm = max((r["pinned_members"] for r in recs), default=0)
        print(f"   {status}  {name:<24} consumed={consumed} "
              f"pinned_by_name={pinned_by_name} pinned_members={pm}")

    print("\n=== " + ("ALL GATES PASS" if ok else "GATES FAILED") + " ===")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--oracle", default=os.path.join(REPO_ROOT, "dc3_oracle.json"))
    ap.add_argument("--symbols",
                    default=os.path.join(REPO_ROOT, "config/45410914/symbols.txt"))
    ap.add_argument("--splits",
                    default=os.path.join(REPO_ROOT, "config/45410914/splits.txt"))
    ap.add_argument("--map",
                    default=os.path.join(REPO_ROOT, "scripts/target_symbol_map.json"))
    ap.add_argument("--top", type=int, default=30)
    ap.add_argument("--validate", action="store_true")
    ap.add_argument("--tu", help="dump per-member detail for one dc3_tu substring")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    oracle = load_oracle(args.oracle)
    sizes = load_symbol_sizes(args.symbols)
    pinned = load_pinned_text(args.splits)
    tmap = load_target_map(args.map)

    if args.tu:
        print_tu_detail(oracle, sizes, pinned, tmap, args.tu)
        return

    results, islands = analyze(oracle, sizes, pinned, tmap)
    dc3_idx, rb3_idx = resolve_oracle_src()
    annotate_sources(results, index_sources(dc3_idx), index_sources(rb3_idx))
    fresh = fresh_ranked(results)

    if args.json:
        print(json.dumps({"fresh": fresh}, indent=1))
        return

    if args.validate:
        ok = run_validate(results, fresh)
        print()
        print_table(fresh, min(args.top, 15), islands)
        sys.exit(0 if ok else 1)

    print_table(fresh, args.top, islands)


if __name__ == "__main__":
    main()
