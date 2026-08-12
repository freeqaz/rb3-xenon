#!/usr/bin/env python3
"""Account every `name_check` CHARGE against the ICF-alias pipeline's decision.

Why this exists
---------------
`tools/icf_alias_build.py` prints a decision census over its CANDIDATE
population -- the pairs `tools/icf_site_census.py` observed plus, with
`--enumerate both`, the body-hash collisions.  That population is ~28k pairs /
~100k relocation slots, and it is NOT the population objdiff charges.  Reading a
refusal count off it and calling it "the lever worth pulling next" is a category
error: most of those slots are never charged, so clearing them buys nothing.

The banked triage MANIFEST (decomp-bench
`archive/runs/namecheck-lane-triage-and-fixers-20260812/`) does exactly that.  It
names `reject_survivor_not_mapped` (23,024 pairs) "a coverage limit of the map,
not an absence of folds, and the lever worth pulling next for that project."

This script joins the two populations, so a refusal count is always reported over
the charges it could actually clear.  Measured 2026-08-12 on rb3-xenon at build
45410914 the answer is that the lever is 41 pairs, not 23,024:

    decision on CHARGED pairs             pairs   sites     fns
    NEVER_PROPOSED                         2082    6737    6176
      -> ingest_drop: our callee mapped    1153    4347    3813
      -> census MISS (alignment gate)       927    2356    2349
    reject_RELOC_TARGETS_DIFFER             582    1393     852
    reject_RETAIL_DIFFER                    516     730     630
    reject_no_evidence                      323    1320     969
    reject_T2_over_cap                      292     979     715
    reject_gate_c_target_naming              43      53      47
    reject_survivor_not_mapped               41     132      66

22,983 of the 23,024 refused pairs have a survivor spelled `fn_<hex>` or
`lbl_<hex>`.  objdiff's `name_check` tolerates exactly that shape unconditionally
(`objdiff-core/src/diff/code.rs`, `is_placeholder_symbol_name`, applied before
any name comparison): a placeholder-named target is an unidentified split symbol,
so the site is UNVERIFIABLE, not a mismatch.  Those slots are never charged and
an alias for them cannot move the metric.  The 41 that survive the join are all
`vftable_<hex>` -- dtk's vtable placeholder, which objdiff's predicate does NOT
cover.

Usage
-----
    python3 tools/icf_site_census.py --root . --out <d>/sites_census.json
    python3 tools/icf_fold_evidence.py --out <d>/evidence.json
    python3 tools/icf_alias_build.py --enumerate both --sites <d>/sites_census.json \\
        --evidence <d>/evidence.json --out <d>/alias.json --why <d>/why.json
    python3 scripts/namecheck_gate_accounting.py --why <d>/why.json \\
        --charges <d>/sites.jsonl --census <d>/sites_census.json
"""

import argparse
import collections
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# objdiff-core/src/diff/code.rs::is_placeholder_symbol_name, transcribed.
_PLACEHOLDER = re.compile(
    r"^_?(fn_|lbl_|jumptable_|code_|data_|bss_|rdata_)[0-9a-fA-F_]+$")


def objdiff_placeholder(name: str) -> bool:
    return bool(_PLACEHOLDER.match(name))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--why", required=True, help="icf_alias_build.py --why dump")
    ap.add_argument("--charges", required=True,
                    help="sites.jsonl from namecheck_triage.py (the CHARGED population)")
    ap.add_argument("--census", default="", help="icf_site_census.py output (optional, "
                                                 "splits NEVER_PROPOSED)")
    ap.add_argument("--out", default="")
    args = ap.parse_args()

    why = {(t, b): v for t, b, v in json.loads(Path(args.why).read_text())["decisions"]}
    mapped = {v for k, v in json.loads(
        (ROOT / "scripts" / "target_symbol_map.json").read_text()).items()
        if isinstance(k, str) and k.lower().startswith("0x") and isinstance(v, str)}

    cen_pairs = set()
    if args.census:
        for _u, _f, rr in json.loads(Path(args.census).read_text())["records"]:
            for _k, t, b in rr:
                if isinstance(t, str) and isinstance(b, str):
                    cen_pairs.add((t, b))

    sites = collections.Counter()
    fns = collections.defaultdict(set)
    for line in open(args.charges):
        r = json.loads(line)
        k = (r["target"], r["base"])
        sites[k] += 1
        fns[k].add((r["unit"], r["func"]))

    def bucket(k):
        v = why.get(k)
        if v is not None:
            return v
        t, b = k
        ts = t if isinstance(t, str) else ""
        bs = b if isinstance(b, str) else ""
        if not ts or not bs:
            return "NEVER_PROPOSED / not a symbol pair"
        if bs in mapped:
            return "NEVER_PROPOSED / ingest_drop: our callee is map-resident"
        if bs.startswith("__") or ts.startswith("__"):
            return "NEVER_PROPOSED / ingest_drop: __ prefix"
        if ts.startswith("except_data_") or "unwind" in bs or "chain" in bs:
            return "NEVER_PROPOSED / ingest_drop: eh/unwind"
        if args.census and k not in cen_pairs:
            return "NEVER_PROPOSED / census MISS (alignment gate)"
        return "NEVER_PROPOSED / unaccounted"

    tp, ts_, tf = collections.Counter(), collections.Counter(), collections.defaultdict(set)
    for k, n in sites.items():
        v = bucket(k)
        tp[v] += 1
        ts_[v] += n
        tf[v] |= fns[k]
    print("charged: %d pairs, %d sites, %d functions"
          % (len(sites), sum(sites.values()), len({x for s in fns.values() for x in s})))
    print("%-56s %6s %7s %7s" % ("decision", "pairs", "sites", "fns"))
    for k, v in tp.most_common():
        print("%-56s %6d %7d %7d" % (k, v, ts_[k], len(tf[k])))

    # The headline check: how much of a refusal lives on charged slots at all?
    print()
    for r in sorted({v for v in why.values() if v.startswith("reject")}):
        cand = [k for k, v in why.items() if v == r]
        ph = sum(1 for t, _b in cand if isinstance(t, str) and objdiff_placeholder(t))
        chg = sum(1 for k in cand if k in sites)
        print("%-34s candidates %6d  (survivor is an objdiff placeholder: %6d)"
              "  CHARGED %5d" % (r, len(cand), ph, chg))

    if args.out:
        Path(args.out).write_text(json.dumps(
            {"charged_pairs": len(sites), "charged_sites": sum(sites.values()),
             "by_decision": {k: {"pairs": tp[k], "sites": ts_[k], "fns": len(tf[k])}
                             for k in tp}}, indent=1) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
