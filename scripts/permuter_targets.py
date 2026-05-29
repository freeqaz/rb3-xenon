#!/usr/bin/env python3
"""permuter_targets.py — rank the permuter's work queue from report.json.

The source permuter (scripts/permuter, wired via the `permute` skill) mechanizes
the FP/GPR register-swap, local-declaration-order, and statement-reassociation
fixes that hand-editing cannot reliably do. Its sweet spot is functions that
already match 80–99.99% (a small codegen residue), in TUs that compile + are
pinned (so a target .obj exists). This tool produces that ranked queue from
`build/45410914/report.json` alone — no build, no objdiff — so it is safe to run
while a pin-wave build is in flight (point --report at a snapshot if so).

WINNABILITY HEURISTIC (report.json-only; no diff residue available without a
build): the permuter wins most on SMALL functions CLOSE to 100% whose residue is
real codegen (regswap/order), and loses on ICF/LINKER_MERGED template bloat.
We can detect the latter straight from the mangled name (STL container member-
template instantiations), so those are flagged `likely_icf` and down-ranked.

  score = band_weight * size_factor * icf_factor
    band_weight : 99–99.99 -> 3.0 ; 95–99 -> 2.0 ; 80–95 -> 1.0
    size_factor : 1.0 for <=128B, decaying to ~0.3 by 1KB (small = small search)
    icf_factor  : 0.15 if the mangled name looks like an STL template instantiation
                  (likely linker-folded / unfixable from source), else 1.0

Usage:
  venv/bin/python scripts/permuter_targets.py rank [--report PATH] [--top N]
                                                    [--include-icf] [--min-pct 80]
Outputs (gitignored, regenerable):
  permuter_targets.json   — full ranked records
  permuter_targets.txt    — human queue, grouped by unit
Each record carries the mangled `name` (pass verbatim to the permuter --symbol),
the demangled name, unit, size, match%, score, and the likely_icf flag.
"""
import argparse, json, os, re, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_REPORT = os.path.join(REPO, "build", "45410914", "report.json")

# Mangled-name fragments that mark STL container member-template instantiations.
# These are overwhelmingly identical-COMDAT-folded (ICF) in the target and cannot
# be matched by source iteration — the permuter will spin on them fruitlessly.
ICF_NAME_MARKERS = (
    "?$vector@", "?$list@", "?$_Rb_tree", "?$set@", "?$map@", "?$ObjVector@",
    "?$ObjPtrList@", "?$ObjPtrVec@", "StlNodeAlloc", "_M_fill_insert",
    "_M_insert", "_M_realloc_insert", "__uninitialized_", "_Destroy_Range",
    "_S_create_storage", "?$allocator@", "?$_List_node", "?$pair@",
)


def n(x, default=0.0):
    try:
        return float(x)
    except (TypeError, ValueError):
        return default


def looks_icf(name: str) -> bool:
    return any(m in name for m in ICF_NAME_MARKERS)


def band_weight(pct: float) -> float:
    if pct >= 99.0:
        return 3.0
    if pct >= 95.0:
        return 2.0
    return 1.0  # 80–95


def size_factor(sz: int) -> float:
    if sz <= 128:
        return 1.0
    if sz >= 1024:
        return 0.3
    # linear decay 128->1024 mapping 1.0->0.3
    return 1.0 - 0.7 * (sz - 128) / (1024 - 128)


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("rank", help="emit ranked permuter work queue")
    r.add_argument("--report", default=DEFAULT_REPORT)
    r.add_argument("--top", type=int, default=0, help="limit (0 = all)")
    r.add_argument("--min-pct", type=float, default=80.0)
    r.add_argument("--include-icf", action="store_true",
                   help="keep likely-ICF template instantiations in the queue")
    args = ap.parse_args()

    if not os.path.exists(args.report):
        sys.exit(f"report not found: {args.report}")
    d = json.load(open(args.report))
    units = d["units"]

    cands = []
    for u in units:
        um = u.get("measures", {})
        if n(um.get("matched_functions", 0)) == 0:
            continue  # unit not compiled+pinned (no target to permute against)
        unit = u["name"]
        for f in u.get("functions", []):
            mp = f.get("match_percent_normalized")
            if mp is None:
                continue
            mp = n(mp)
            if mp >= 100.0 or mp < args.min_pct:
                continue
            name = f["name"]
            sz = int(n(f.get("size", 0)))
            icf = looks_icf(name) or looks_icf(
                (f.get("metadata") or {}).get("demangled_name", ""))
            if icf and not args.include_icf:
                # still record it but at the very bottom, flagged
                pass
            icf_factor = 0.15 if icf else 1.0
            score = band_weight(mp) * size_factor(sz) * icf_factor
            cands.append({
                "score": round(score, 3),
                "match_percent": round(mp, 4),
                "size": sz,
                "unit": unit,
                "name": name,
                "demangled": (f.get("metadata") or {}).get("demangled_name", ""),
                "address": f.get("address"),
                "likely_icf": icf,
            })

    cands.sort(key=lambda c: (-c["score"], -c["match_percent"], c["size"]))
    if args.top:
        cands = cands[:args.top]

    real = [c for c in cands if not c["likely_icf"]]
    icf = [c for c in cands if c["likely_icf"]]
    out_json = os.path.join(REPO, "permuter_targets.json")
    json.dump({"n_total": len(cands), "n_real": len(real), "n_likely_icf": len(icf),
               "candidates": cands}, open(out_json, "w"), indent=1)

    # human queue grouped by unit (real targets first, then a flagged ICF tail)
    out_txt = os.path.join(REPO, "permuter_targets.txt")
    with open(out_txt, "w") as fh:
        fh.write(f"Permuter work queue from {os.path.relpath(args.report, REPO)}\n")
        fh.write(f"  {len(cands)} fns at {args.min_pct:.0f}-99.99%  "
                 f"({len(real)} real / {len(icf)} likely-ICF)\n")
        fh.write("  invoke: venv/bin/python -m scripts.permuter_rb3xenon "
                 "-m scripts.permuter.scan_and_permute --symbol '<name>'\n")
        fh.write("  (re-check scripts/permuter* + .claude/skills/permute/SKILL.md "
                 "for the current invocation — proper-port may have changed it)\n\n")
        fh.write(f"{'score':>6} {'pct':>7} {'size':>6}  unit / symbol\n")
        fh.write("-" * 78 + "\n")
        for c in real:
            fh.write(f"{c['score']:6.3f} {c['match_percent']:7.3f} {c['size']:6}  "
                     f"{c['unit']}\n          {c['demangled'] or c['name']}\n")
        if icf:
            fh.write(f"\n--- likely-ICF / template bloat ({len(icf)}; "
                     f"permuter usually can't fix — use --include-icf to rank inline) ---\n")
            for c in icf[:40]:
                fh.write(f"{c['score']:6.3f} {c['match_percent']:7.3f} {c['size']:6}  "
                         f"{c['unit']}  {c['name'][:60]}\n")

    print(f"[permuter-targets] {len(cands)} candidates "
          f"({len(real)} real, {len(icf)} likely-ICF) -> {out_json}, {out_txt}")
    print(f"[permuter-targets] TOP 15 REAL (winnable) targets:")
    for c in real[:15]:
        print(f"  score={c['score']:5.2f} {c['match_percent']:6.2f}% {c['size']:>5}B  "
              f"{c['unit']:<26} {c['demangled'][:48] or c['name'][:48]}")


if __name__ == "__main__":
    main()
