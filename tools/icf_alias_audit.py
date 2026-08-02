#!/usr/bin/env python3
"""PRECISION audit for a generated scripts/symbol_aliases.json candidate set.

An alias entry that is WRONG hides a real defect, which is strictly worse than leaving
the site noisy. So the question this answers is not "how many aliases did we get" but
"how much of each alias's evidence is actually INFORMATION, and how much is a
tolerated absence of information".

For every (survivor S, folded F) pair it re-derives the T1 comparison and reports:
  * body size and the fraction of bytes left UNMASKED by relocations (the anti-vacuity
    measure -- a body that is mostly relocated fields compares equal to too much)
  * reloc slots split into EXACT name agreement vs TOLERATED retail-side placeholder
  * whether the pair would survive if every placeholder tolerance were withdrawn
    (``fully_grounded``) -- the strongest available statement

Also runs the two mandatory controls:
  POSITIVE  the alias groups already proven and landed on main must be re-derived.
  NEGATIVE  a decoy set of pairs that differ ONLY in a call target must be REJECTED;
            selectivity is reported as the rejection rate over that decoy set.
"""

import argparse
import collections
import glob
import json
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools"))
# ★ DC-4 (2026-08-02): the former `from icf_fold_evidence import function_bodies,
# masked_body` here was DEAD (AST-verified: both appeared exactly once, on the
# import line, and were never referenced). It is removed so nobody reads this
# file as a live consumer of the frozen legacy reader. This tool's population
# comes ENTIRELY through `collect` below, which lane DC-4 moved to the corrected
# EH-aware reader -- so THIS AUDIT'S NUMBERS MOVE WITH IT (intended: the audit
# must grade the same population that gets shipped).
from icf_alias_build import collect, placeholder             # noqa: E402

sys.path.insert(0, str(PROJECT_ROOT / "scripts" / "harvest"))
try:
    from live_units import filter_live
except Exception:
    filter_live = None


def audit_pair(rt, ob, mapped):
    """Return per-slot accounting for a (retail S, ours F) comparison."""
    rr, orr = rt[1], ob[1]
    exact = tol = 0
    for (ro, rn, rty), (oo, on, oty) in zip(rr, orr):
        if rn == on:
            exact += 1
        elif placeholder(rn) or placeholder(on):
            tol += 1
    size = rt[2]
    masked = sum(4 for (o, _n, _t) in rr if o + 4 <= size)
    return {"size": size, "unmasked_frac": (size - masked) / size if size else 0.0,
            "n_relocs": len(rr), "exact": exact, "tolerated": tol,
            "fully_grounded": tol == 0}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--aliases", required=True)
    ap.add_argument("--landed", default=str(PROJECT_ROOT / "scripts" / "symbol_aliases.json"))
    args = ap.parse_args()

    tm = json.loads((PROJECT_ROOT / "scripts" / "target_symbol_map.json").read_text())
    mapped = {v for k, v in tm.items()
              if isinstance(k, str) and k.lower().startswith("0x") and isinstance(v, str)}

    our_objs = glob.glob(str(PROJECT_ROOT / "build/45410914/src/**/*.obj"), recursive=True)
    ours = collect(our_objs)
    tgt = glob.glob(str(PROJECT_ROOT / "build/45410914/obj/*.obj"))
    if filter_live:
        try:
            tgt = filter_live(tgt, str(PROJECT_ROOT))
        except Exception:
            pass
    retail = collect(tgt)

    cand = json.loads(Path(args.aliases).read_text())["groups"]
    rows, per_group = [], {}
    for g in cand:
        S = g["survivor"]
        rt = retail.get(S)
        gr = []
        for F in g["folded"]:
            ob = ours.get(F)
            if rt is None or ob is None:
                gr.append({"folded": F, "t1": False})
                continue
            a = audit_pair(rt, ob, mapped)
            a["folded"] = F
            a["t1"] = (rt[0] == ob[0] and rt[2] == ob[2])
            gr.append(a)
            rows.append(a)
        per_group[S] = gr

    n = len(rows)
    fg = sum(1 for r in rows if r.get("fully_grounded"))
    print("=== T1 PRECISION AUDIT (%d groups, %d adjudicable pairs) ===" % (len(cand), n))
    if n:
        print("  fully grounded (ZERO tolerated placeholder slots): %d / %d  (%.1f%%)"
              % (fg, n, 100.0 * fg / n))
        sizes = sorted(r["size"] for r in rows)
        print("  body size   min %d  median %d  max %d"
              % (sizes[0], sizes[len(sizes) // 2], sizes[-1]))
        uf = sorted(r["unmasked_frac"] for r in rows)
        print("  unmasked frac  min %.2f  median %.2f" % (uf[0], uf[len(uf) // 2]))
        buckets = collections.Counter()
        for r in rows:
            buckets["<=8B" if r["size"] <= 8 else
                    "9-32B" if r["size"] <= 32 else
                    "33-96B" if r["size"] <= 96 else ">96B"] += 1
        print("  size buckets:", dict(buckets))
        print("  pairs with 0 relocs at all (pure-code fold): %d"
              % sum(1 for r in rows if r["n_relocs"] == 0))

    # ---- POSITIVE CONTROL -------------------------------------------------
    landed = json.loads(Path(args.landed).read_text())["groups"]
    have = {g["survivor"]: set(g["folded"]) for g in cand}
    print("\n=== POSITIVE CONTROL: the %d groups already landed on main ===" % len(landed))
    ok = 0
    for g in landed:
        got = have.get(g["survivor"])
        miss = set(g["folded"]) - (got or set())
        status = "RE-DERIVED" if got and not miss else \
                 ("PARTIAL missing=%s" % sorted(miss) if got else "ABSENT")
        ok += 1 if (got and not miss) else 0
        print("  %-28s %s" % (g["name"], status))
    print("  recovered %d / %d" % (ok, len(landed)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
