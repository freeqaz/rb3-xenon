#!/usr/bin/env python3
"""scatter_audit_report.py -- the two failure directions, with DEFENSIBLE criteria.

Direction B (a target links code it never asked for) is decidable from the
graph alone: if a target compiles ZERO TUs standalone from module M, yet a
scatter host it does compile drags in files from M, that target is linking M
by accident. Nobody chose it; an X360 packing decision did.

Direction A (a target is MISSING code it needs) is NOT decidable from the graph
alone -- "needs" is a demand property that only the link can answer, and every
target links today. So we do NOT guess. We report the strictly weaker but
fully-decidable predictor:

    files whose code reaches NO native target at all
    -- i.e. not compiled standalone anywhere, and every unconditional scatter
    host of theirs is also absent from every target.

Those are precisely the files that will produce "undefined reference" the
moment anything references them, which is the wall X9 hit. Reporting this set
is a PREDICTION, and it is labelled as one.
"""

import json
import os
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scatter_audit import (REPO, closure, hosts_map, rel, target_sources)  # noqa


def module_of(p):
    parts = p.split("/")
    return "/".join(parts[:3]) if len(parts) >= 3 else p


def main():
    targets = target_sources()
    hosts = hosts_map()

    reach = {}
    for name, S in targets.items():
        E, edges = closure(S)
        reach[name] = (S, E, edges)

    print("=" * 78)
    print("DIRECTION B -- targets linking a whole MODULE they compile none of")
    print("=" * 78)
    rows = []
    for name in sorted(targets):
        S, E, edges = reach[name]
        smod = defaultdict(int)
        for f in S:
            smod[module_of(rel(f))] += 1
        emod = defaultdict(list)
        for f in E - S:
            emod[module_of(rel(f))].append(rel(f))
        for m, files in sorted(emod.items()):
            if smod.get(m, 0) == 0:
                via = sorted({rel(h) for h, g in edges
                              if rel(g) in files})
                rows.append((name, m, len(files), sorted(files), via))
    for name, m, n, files, via in rows:
        print(f"\n  {name}: compiles 0 TUs of {m}, but LINKS {n} of them")
        for f in files:
            print(f"      {f}")
        print(f"      via: {', '.join(via[:4])}")
    if not rows:
        print("  (none)")

    print()
    print("=" * 78)
    print("DIRECTION A (PREDICTOR) -- code that reaches NO native target")
    print("=" * 78)
    anywhere = set()
    for name, (S, E, _) in reach.items():
        anywhere |= S | E
    orphan = []
    for g, hs in hosts.items():
        uh = [h for h, k in hs if k == "uncond"]
        if not uh:
            continue
        if g in anywhere:
            continue
        orphan.append((rel(g), [rel(x) for x in uh]))
    print(f"  {len(orphan)} scatter-guest files are in NO native target's link.")
    print("  Each will produce 'undefined reference' the moment it is referenced.")
    for g, uh in sorted(orphan)[:40]:
        print(f"      {g}\n          only emitted by: {', '.join(uh)}")

    print()
    print("=" * 78)
    print("MULTI-HOST GUESTS -- duplicate-definition landmines")
    print("=" * 78)
    for g, hs in sorted(hosts.items()):
        uh = [rel(h) for h, k in hs if k == "uncond"]
        if len(uh) > 1:
            live = [n for n in sorted(targets)
                    if sum(1 for h, k in hs if k == "uncond"
                           and (os.path.abspath(h) in reach[n][0]
                                or os.path.abspath(h) in reach[n][1])) > 1]
            flag = "  <== BOTH HOSTS LIVE IN: " + ",".join(live) if live else ""
            print(f"  {rel(g)}\n      hosts: {', '.join(uh)}{flag}")


if __name__ == "__main__":
    main()
