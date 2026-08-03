#!/usr/bin/env python3
"""Lane DL-1 follow-up: is the 46.8% ceiling a limit of THE EVIDENCE or of MY
INSTRUMENT?

The global run scored 46.8% top-1 with a decoy null reaching 1.000.  Two rival
explanations:

  (H1) body-level structural evidence is genuinely insufficient to identify a
       function -- too many shape-identical bodies (dtors, accessors, template
       stamps) -- so NO scorer, BinDiff included, can separate them;
  (H2) my scorer is just weaker than BinDiff, which also uses call-graph and
       basic-block context.

These make OPPOSITE predictions under a LOCATION PRIOR.  Restrict the candidate
pool to the correct DC3 .obj (which the leaked map gives us) and re-measure:

  * if accuracy jumps to ~90%, the bodies were always distinguishable WITHIN a
    TU and the missing ingredient is LOCATION, not scoring power.  H1 holds in
    the specific form that matters: the global search space, not the score, is
    the problem -- and BinDiff's extra context is a location prior by another
    name.
  * if accuracy stays ~50%, the bodies are genuinely ambiguous even among their
    own TU siblings, and better scoring is what is missing.

This distinction decides whether re-running BinDiff against the current binary
could rescue the channel -- so it must be measured, not assumed.

CONTROL THAT CAN FAIL: the same scoped retrieval is also run with a DELIBERATELY
WRONG scope (a random other DC3 .obj).  Accuracy there must collapse to ~0; if a
wrong scope still "recovers" the truth, the scoped harness is measuring nothing.
"""
import json
import os
import random
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dl1_structmatch import (load_fns, tokens, shingles, tokhash, jac,  # noqa: E402
                         REPO, DC3EXE, DC3MAP)
sys.path.insert(0, os.path.join(REPO, "tools"))
import dc3_map  # noqa: E402


def main():
    rb3 = load_fns(os.path.join(REPO, "orig/45410914/band.exe"))
    dc3 = load_fns(DC3EXE)
    dmap = dc3_map.parse_map(DC3MAP)
    assert dmap.get("?Poll@Character@@UAAXXZ", {}).get("addr") == 0x82351090, \
        "C1 FAILED: known positive missing"

    by_va = defaultdict(list)
    for n, e in dmap.items():
        by_va[e["addr"]].append(n)
    # DC3 obj -> [VAs]   (the location prior the leaked map hands us for free)
    obj_vas = defaultdict(set)
    for n, e in dmap.items():
        if e["addr"] in dc3:
            obj_vas[e["obj"]].add(e["addr"])
    print(f"[in] DC3 objs with >=1 extent-bearing fn: {len(obj_vas)}")

    sh_cache = {}

    def SH(va):
        if va not in sh_cache:
            sh_cache[va] = shingles(tokens(dc3[va]))
        return sh_cache[va]

    # ---- ground truth (identical construction to the global run) ----------
    tsm = json.load(open(os.path.join(REPO, "scripts/target_symbol_map.json")))
    tsm = {a: n for a, n in tsm.items() if a.startswith("0x") and isinstance(n, str)}
    n2a = {}
    for a, n in tsm.items():
        n2a.setdefault(n, int(a, 16))
    rep = json.load(open(os.path.join(REPO, "build/45410914/report.json")))
    pos = []
    for u in rep["units"]:
        for f in u.get("functions") or []:
            n = f.get("name", "")
            if n.startswith("fn_") or not n:
                continue
            if f.get("match_percent_normalized") != 100.0:
                continue
            va = n2a.get(n)
            if va is None or va not in rb3 or n not in dmap:
                continue
            t = dmap[n]["addr"]
            if t in dc3:
                pos.append((va, n, t, dmap[n]["obj"]))
    print(f"[in] positive-control pairs available: {len(pos)}")

    rnd = random.Random(11)
    sample = rnd.sample(pos, min(600, len(pos)))
    all_objs = [o for o in obj_vas if len(obj_vas[o]) >= 2]

    hit = tot = 0
    sizes = []
    wrong_hit = wrong_tot = 0
    for va, name, truth, obj in sample:
        cands = sorted(obj_vas[obj])
        if len(cands) < 2:
            continue          # a 1-function scope is a vacuous test (rule 1)
        q = shingles(tokens(rb3[va]))
        best = max(cands, key=lambda c: (jac(q, SH(c)), -c))
        tot += 1
        sizes.append(len(cands))
        if best == truth or name in by_va.get(best, []):
            hit += 1
        # --- sabotage leg: a deliberately WRONG scope must not recover truth
        wobj = rnd.choice(all_objs)
        while wobj == obj:
            wobj = rnd.choice(all_objs)
        wc = sorted(obj_vas[wobj])
        wbest = max(wc, key=lambda c: (jac(q, SH(c)), -c))
        wrong_tot += 1
        if wbest == truth or name in by_va.get(wbest, []):
            wrong_hit += 1

    sizes.sort()
    print(f"\n=== SCOPED retrieval (candidate pool = the CORRECT DC3 .obj) ===")
    print(f"  queries: {tot}   median scope size: {sizes[len(sizes)//2]} fns "
          f"(vs 56,893 global)")
    print(f"  top-1 correct: {hit}/{tot} = {100*hit/tot:.1f}%")
    print(f"  [global run for comparison: 46.8%]")
    print(f"\n=== SABOTAGE LEG (candidate pool = a RANDOM WRONG DC3 .obj) ===")
    print(f"  top-1 'correct': {wrong_hit}/{wrong_tot} = "
          f"{100*wrong_hit/wrong_tot:.1f}%  (must be ~0 or the harness is vacuous)")

    json.dump({"scoped_hit": hit, "scoped_tot": tot,
               "sabotage_hit": wrong_hit, "sabotage_tot": wrong_tot,
               "median_scope": sizes[len(sizes)//2]},
              open("/home/free/tmp/laneDL1/scoped_control.json", "w"), indent=1)


if __name__ == "__main__":
    main()
