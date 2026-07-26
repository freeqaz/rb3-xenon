#!/usr/bin/env python3
"""How much identification headroom does the MULTI residue actually contain?

Lane M, 2026-07-26.

Every identification lane so far has sized its opportunity by counting
``homing_scan`` **records** -- "26,223 MULTI", "3,843 BIG-FAMILY", "8,752
exception-flagged".  Those counts are badly inflated, and this tool measures by
how much.

Two independent deflators
-------------------------
1. **Records are (name x TU) pairs, not names.**  A template instantiated in 40
   TUs contributes 40 MULTI records but is one symbol with one retail home.
2. **Retail ICF-folded the swarms, so there are fewer addresses than names.**
   A "family" here is a set of our names sharing one byte-identical hit tuple.
   In 112 of 294 families there are strictly fewer retail addresses than our
   names.  The extreme cases are brutal: 130 of our names over 4 addresses.

Consequence: the map is 1:1 by construction (one VA -> one mangled name), so a
family can absorb at most ``min(#unhomed names, #free addresses)`` new homings
**no matter how good the discriminator is**.  Summing that bound over all
families gives the true ceiling for the whole residue.

Measured at 27,629 strict (HEAD d83ca54f):

    MULTI records                                        26,223
    distinct names in those families                      5,853
    retail addresses in those families                    9,228
    of which still unmapped                               6,670
    HEADROOM (sum of per-family min(unhomed, free))    ->  1,695

So the entire MULTI residue is worth at most ~1,695 homings, not 26,223.  Lane
K's 3,843 refused BIG-FAMILY *names* are drawn from this same pool, which means
most of them have no address left to be homed to at all -- and that, rather than
the 96.65 % precision, is the real reason not to fund them.

Usage
-----
    python3 scripts/harvest/residue_headroom.py --results merged.json
    python3 scripts/harvest/residue_headroom.py --results merged.json --top 25
"""

from __future__ import annotations

import argparse
import json
import os
from collections import Counter, defaultdict
from pathlib import Path


def load_tmap_vas(path: Path) -> dict[int, str]:
    """VA -> name.  264 legacy keys are uppercase ``0X...`` -- compare lowercased."""
    raw = json.load(open(path))
    out: dict[int, str] = {}
    for k, v in raw.items():
        kk = str(k).lower()
        if kk.startswith("0x"):
            try:
                out[int(kk, 16)] = v
            except ValueError:
                continue
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--results", type=Path, required=True, help="homing_scan merged.json")
    ap.add_argument("--worktree", type=Path, default=Path(os.environ.get("HOMING_WT", ".")))
    ap.add_argument("--tmap", type=Path, default=None)
    ap.add_argument("--cls", default="MULTI", help="record class to analyse (default MULTI)")
    ap.add_argument("--top", type=int, default=12, help="show N largest families")
    a = ap.parse_args()

    tmap = load_tmap_vas(a.tmap or (a.worktree / "scripts" / "target_symbol_map.json"))
    homed = set(tmap.values())
    merged = json.load(open(a.results))

    fam_names: dict[tuple, set] = defaultdict(set)
    fam_recs: Counter = Counter()
    cls_census: Counter = Counter()
    for _tu, recs in merged.items():
        for r in recs:
            cls_census[r["cls"]] += 1
            if r["cls"] != a.cls:
                continue
            key = tuple(sorted(int(h, 16) for h in r["hits"]))
            fam_names[key].add(r["name"])
            fam_recs[key] += 1

    regime = Counter()
    tot_names = tot_addr = tot_recs = 0
    free_addr = unhomed_names = 0
    headroom = 0
    rows = []
    for key, names in fam_names.items():
        n, m = len(names), len(key)
        tot_names += n
        tot_addr += m
        tot_recs += fam_recs[key]
        regime["m<n" if m < n else "m==n" if m == n else "m>n"] += 1
        free = [v for v in key if v not in tmap]
        unhomed = [x for x in names if x not in homed]
        free_addr += len(free)
        unhomed_names += len(unhomed)
        h = min(len(unhomed), len(free))
        headroom += h
        rows.append((h, len(unhomed), len(free), n, m, fam_recs[key], sorted(names)[0]))

    print("== homing_scan record census ==")
    for k, v in cls_census.most_common():
        print("  %-12s %d" % (k, v))

    print("\n== %s families ==" % a.cls)
    print("  families                                  %d" % len(fam_names))
    print("  regime (retail addrs m vs our names n)    %s" % dict(regime))
    print("  records (name x TU)                       %d" % tot_recs)
    print("  distinct names                            %d" % tot_names)
    print("  retail addresses                          %d" % tot_addr)
    print("  ... still unmapped                        %d" % free_addr)
    print("  ... names still unhomed                   %d" % unhomed_names)
    print("\n  HEADROOM  sum over families of min(unhomed names, free addrs)")
    print("            = %d" % headroom)
    if tot_recs:
        print("            = %.1f %% of the %d %s records"
              % (100.0 * headroom / tot_recs, tot_recs, a.cls))
    print("\n  Read that as: no discriminator, however precise, can home more")
    print("  than %d names out of this pool.  The binding constraint is retail" % headroom)
    print("  ICF folding plus the 1:1 map, not the quality of the evidence.")

    rows.sort(reverse=True)
    print("\n== %d largest families by headroom ==" % a.top)
    print("  %-8s %-8s %-7s %-6s %-6s %-7s %s"
          % ("head", "unhomed", "freeVA", "names", "addrs", "records", "example"))
    for h, u, f, n, m, rec, ex in rows[: a.top]:
        print("  %-8d %-8d %-7d %-6d %-6d %-7d %s" % (h, u, f, n, m, rec, ex[:70]))


if __name__ == "__main__":
    main()
