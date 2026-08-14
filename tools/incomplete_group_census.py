#!/usr/bin/env python3
"""incomplete_group_census.py -- census the ALREADY-DECLARED-BUT-STILL-CHARGED alias groups.

Lane INCOMPLETE-1 (2026-08-14). Follows ONMSG-1 (34b0a56a), which found that TWO
groups already present in scripts/symbol_aliases.json were STILL CHARGED, because
"a pairwise byte comparator finds only SOME members while a dispatcher enumerates
the class EXHAUSTIVELY". Completing them paid +11,424 B.

THE QUESTION THIS ANSWERS: how large is that blind spot tree-wide?

METHOD. Pair functions BY NAME between the dtk target obj and our compiled obj,
walk the two relocation tables together under icf_site_census's alignment gate
(same size, same full (offset,reloc_type) sequence), and record every slot where
the target-side callee NAME differs from ours. Then apply BOTH forgiveness rules
that objdiff's name_check applies and a hand-rolled reloc diff does not:

  (1) PLACEHOLDER forgiveness -- objdiff-core's is_placeholder_symbol_name
      forgives fn_/lbl_/jumptable_/data_/bss_/rdata_ targets outright;
  (2) SYMBOL EQUIVALENCE -- a pair already inside one installed alias group is
      already free.

Skipping either is exactly the overcount trap recorded in
docs/decomp/template-args-C-class-is-folds-2026-08-14.md (41 pairs / 70 sites for
InterstitialMgr, whose top two entries were both already free).

CLASSIFICATION of what survives, against the installed groups:

  MEMBERSHIP  retail-side name is IN an installed group, our-side name is in NO
              group. The group exists and is missing a spelling -- ONMSG-1's
              exact shape. Additive: asserts one more spelling folded into an
              address the group ALREADY claims.
  MERGE       both names are in installed groups, and they are DIFFERENT groups.
              This asserts that TWO DISTINCT RETAIL ADDRESSES FOLD, which is a
              strictly stronger claim than a membership. Adjudicated separately.
  INTERNAL    both names in the SAME group but still charged -- would be an
              alias-map generation defect; expected count 0.
  FRESH       neither name is in any group. Not this lane's population.

RECALL BOUND, stated because it bounds the headline: the alignment gate is
deliberately conservative, so a pair whose enclosing functions never align is
invisible here. This census is therefore a LOWER BOUND on the incomplete-group
population, not a closed set.

Read-only. Mutates no build input.
"""

import collections
import glob
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

from icf_alias_build import collect, placeholder  # noqa: E402


def load_groups(path):
    """Return (name -> group index, groups list)."""
    doc = json.load(open(path))
    groups = doc["groups"]
    owner = {}
    for i, g in enumerate(groups):
        for n in [g["survivor"]] + list(g.get("folded", [])):
            if n:
                owner.setdefault(n, i)
    return owner, groups


def charged_pairs(tgt, ours):
    """(retail_name, our_name) -> {'sites': n, 'victims': set(fn)} under the alignment gate."""
    sites = collections.Counter()
    victims = collections.defaultdict(set)
    aligned = 0
    for name, (_mb, rel, _sz) in ours.items():
        rt = tgt.get(name)
        if not rt:
            continue
        trel = rt[1]
        if len(trel) != len(rel):
            continue
        if any(ro != oo or rty != oty for (ro, _rn, rty), (oo, _on, oty) in zip(trel, rel)):
            continue
        aligned += 1
        for (_ro, rn, _rty), (_oo, on, _oty) in zip(trel, rel):
            if rn == on:
                continue
            sites[(rn, on)] += 1
            victims[(rn, on)].add(name)
    return sites, victims, aligned


def main():
    tgt = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/src/**/*.obj"), recursive=True)), "o")
    owner, groups = load_groups(os.path.join(ROOT, "scripts/symbol_aliases.json"))

    sites, victims, aligned = charged_pairs(tgt, ours)
    raw_pairs = len(sites)
    raw_sites = sum(sites.values())

    # --- forgiveness rule (1): placeholders
    ph = [p for p in sites if placeholder(p[0]) or placeholder(p[1])]
    for p in ph:
        del sites[p]
    ph_pairs, after_ph = len(ph), len(sites)

    # --- forgiveness rule (2): already equivalent inside one group
    eq = []
    for p in list(sites):
        gi, gj = owner.get(p[0]), owner.get(p[1])
        if gi is not None and gi == gj:
            eq.append(p)
            del sites[p]

    # --- classify the survivors
    cls = collections.defaultdict(list)
    for p, n in sites.items():
        gi, gj = owner.get(p[0]), owner.get(p[1])
        if gi is not None and gj is not None:
            k = "MERGE"
        elif gi is not None or gj is not None:
            k = "MEMBERSHIP"
        else:
            k = "FRESH"
        cls[k].append((p, n))

    # --- price against report.json: a row pays only if it is below fuzzy==100
    rep = json.load(open(os.path.join(ROOT, "build/45410914/report.json")))
    fuzzy, size = {}, {}
    for u in rep["units"]:
        for f in u.get("functions", []):
            fuzzy[f["name"]] = float(f.get("fuzzy_match_percent", 0.0))
            size[f["name"]] = int(f["size"])

    def price(pairs):
        rows = set()
        for p, _n in pairs:
            rows |= victims[p]
        sub = [r for r in rows if fuzzy.get(r, 100.0) < 100.0]
        return len(rows), len(sub), sum(size.get(r, 0) for r in sub)

    print("=" * 78)
    print("INCOMPLETE-GROUP CENSUS  (installed groups: %d)" % len(groups))
    print("=" * 78)
    print("aligned function pairs        : %d" % aligned)
    print("raw charged pairs / sites     : %d / %d" % (raw_pairs, raw_sites))
    print("  - placeholder-forgiven pairs: %d   -> %d" % (ph_pairs, after_ph))
    print("  - already-equivalent pairs  : %d   -> %d" % (len(eq), len(sites)))
    print()
    print("%-12s %8s %8s %8s %8s %10s" % ("class", "pairs", "sites", "rows", "rows<100", "bytes<100"))
    for k in ("MEMBERSHIP", "MERGE", "INTERNAL", "FRESH"):
        if k not in cls:
            continue
        pairs = cls[k]
        r, sub, b = price(pairs)
        print("%-12s %8d %8d %8d %8d %10d"
              % (k, len(pairs), sum(n for _p, n in pairs), r, sub, b))

    out = {}
    for k, pairs in cls.items():
        recs = []
        for p, n in sorted(pairs, key=lambda x: -x[1]):
            vs = sorted(victims[p])
            sub = [r for r in vs if fuzzy.get(r, 100.0) < 100.0]
            recs.append({
                "retail": p[0], "ours": p[1], "sites": n,
                "group_retail": owner.get(p[0]), "group_ours": owner.get(p[1]),
                "rows": vs, "rows_sub100": sub,
                "bytes_sub100": sum(size.get(r, 0) for r in sub),
            })
        out[k] = recs
    dest = os.environ.get("CENSUS_OUT", "/home/free/tmp/incomplete_census.json")
    json.dump(out, open(dest, "w"), indent=1)
    print("\nwrote %s" % dest)
    return 0


if __name__ == "__main__":
    sys.exit(main())
