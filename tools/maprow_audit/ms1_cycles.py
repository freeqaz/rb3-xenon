#!/usr/bin/env python3
"""MAPSUS-1 AXIS 1: is the MAP_SUSPECT population ROTATIONS, CHAINS, or neither?

THE POPULATION.  SRCCAND-1's type-consistency screen (``sc1_typecons.py``) read
168 charged pairs whose victim body is equal, and returned MAP_SUSPECT for 74 of
them (44.0%) against a shuffled-callee null of 1.8%.  MAP_SUSPECT means: the
victim's type family CONTAINS our callee's type and does NOT contain retail's,
so the name the map put on the address retail actually calls is wrong.

THE QUESTION THIS FILE ANSWERS is the one the brief calls the durable
deliverable -- not "is the enrichment real" (established) but "what SHAPE is the
defect".  A wrong name at an address is only repairable if you can say what the
right name is, and the only repair that is injective BY CONSTRUCTION is a
permutation of names the map already carries.  So:

  Each MAP_SUSPECT row is a PROPOSAL:  addr(retail_name) should hold our_name.
  our_name currently sits at addr(our_name) =: B.  Draw the edge A -> B, read
  "the name at B belongs at A".

  * A CLOSED CYCLE (every node in the component has in-degree == out-degree == 1)
    is a permutation.  Applying it moves each name to exactly one address and
    leaves the name multiset untouched => injectivity is preserved by
    construction, and the repair needs no invented name and no alias.
  * An OPEN CHAIN has a source node whose name has nowhere to go and a sink whose
    new name is unaccounted for.  Applying it would DUPLICATE one name and ORPHAN
    another.  It is NOT a permutation and must not be written.
  * A CONFLICT is an address with two different proposals.  At most one can be
    right and this file cannot say which.

⚠ THE POINT OF THE CYCLE TEST IS THAT IT CAN FAIL, AND MOSTLY DOES.  SRCCAND-1's
``sc1_thunkfix`` closed 0 of 107 proposals and REFUSED to write rather than emit
a shift that lifts name_check by construction.  This file keeps that property:
it reports chains and conflicts as UNREPAIRABLE-HERE and writes nothing.

CONTROL (--selfcheck).  Cycle closure must not be an artifact of how the graph is
built.  The null re-draws every edge to a RANDOM name drawn from the same map,
preserving node count and out-degree exactly, and re-runs the identical component
analysis.  If the null closes cycles at the real rate, the instrument is
measuring its own construction and its output is worthless.

⛔⛔ AND THAT NULL PASSED WHILE BEING THE WRONG NULL -- READ THIS BEFORE USING
CYCLE CLOSURE AS A SIGNAL.  On MAP_SUSPECT the real data closes 7 two-cycles and
the random-rewiring null closes 0.000 over 200 trials, which reads as decisive.
It is not, because randomly rewiring 74 edges over a 28,933-name map can never
close a cycle -- the null is not measuring the alternative hypothesis, it is
measuring nothing.  The RIGHT control is the UNTREATED POPULATION: run the same
census on the other verdict classes of the same queue.

    --verdict MAP_SUSPECT      74 rows -> 7 CYCLE_len2
    --verdict BOTH_CONSISTENT  74 rows -> 9 CYCLE_len2 + 1 CYCLE_len3
    --verdict BOTH_INCONSIST.  19 rows -> 0

The class where the type screen explicitly CANNOT separate the two sides closes
MORE cycles than the class it indicts, at identical row count.  So closure is a
property of the queue's structure -- template families in which both directions
happen to get charged -- and carries NO information about map rotation.  It is
corroborated independently: ms1_words.py adjudicated all 14 MAP_SUSPECT cycle
nodes on masked body words and returned MAP_CONFIRMED 14/14.

⇒ CYCLE CLOSURE IS A DEAD LEVER FOR THIS QUEUE.  The census is kept because the
   shape decomposition is still the useful output, and because the failure is
   worth more than the tool: a perfect null on a well-formed instrument proved
   nothing, and only the untreated-population control caught it.

⛔ WHAT THIS FILE DELIBERATELY DOES NOT CLAIM.  A closed cycle is a *coherent*
proposal, not a *proven* one.  A rotation among N names is closed for every one
of the (N-1)! non-identity rotations of those N addresses; closure picks out that
the population is permutation-shaped, it does not pick the permutation.  For a
2-cycle -- a transposition -- there is exactly ONE non-identity option, which is
why transpositions are the only sub-population this lane treats as adjudicable
without a further channel.  Longer cycles are reported and left alone.
"""

import argparse
import collections
import json
import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent


def load_map(path=None):
    p = Path(path) if path else ROOT / "scripts" / "target_symbol_map.json"
    raw = json.load(open(p))
    return {k: v for k, v in raw.items() if k.startswith("0x") and isinstance(v, str)}


def invert(m):
    inv = collections.defaultdict(list)
    for a, n in m.items():
        inv[n].append(a)
    return inv


def build_edges(rows, inv):
    """Edge A -> B: the name currently at B belongs at A.

    Rows whose endpoint is missing or ambiguous in the map are dropped and
    counted -- an ambiguous inversion cannot name an address.
    """
    edges, dropped = [], collections.Counter()
    for r in rows:
        ra, oa = inv.get(r["retail_name"], []), inv.get(r["our_name"], [])
        if len(ra) != 1 or len(oa) != 1:
            dropped["R%d_O%d" % (len(ra), len(oa))] += 1
            continue
        edges.append((ra[0], oa[0], r))
    return edges, dropped


def components(edges):
    """Split the proposal digraph into weakly-connected components.

    Returns (components, conflicts) where a conflict is an address carrying two
    DIFFERENT proposals (out-degree > 1 with distinct heads).
    """
    out = collections.defaultdict(set)
    inn = collections.defaultdict(set)
    for a, b, _ in edges:
        out[a].add(b)
        inn[b].add(a)
    conflicts = {a: sorted(bs) for a, bs in out.items() if len(bs) > 1}

    adj = collections.defaultdict(set)
    for a, b, _ in edges:
        adj[a].add(b)
        adj[b].add(a)
    seen, comps = set(), []
    for n in adj:
        if n in seen:
            continue
        stack, comp = [n], []
        seen.add(n)
        while stack:
            x = stack.pop()
            comp.append(x)
            for y in adj[x]:
                if y not in seen:
                    seen.add(y)
                    stack.append(y)
        comps.append(sorted(comp))
    return comps, conflicts, out, inn


def classify(comp, out, inn):
    """CYCLE iff every node has exactly one proposal in and one out, within comp."""
    for n in comp:
        if len(out.get(n, ())) != 1 or len(inn.get(n, ())) != 1:
            return "CHAIN_OR_CONFLICT"
    return "CYCLE"


def analyse(edges):
    comps, conflicts, out, inn = components(edges)
    res = collections.Counter()
    cycles, chains = [], []
    for c in comps:
        k = classify(c, out, inn)
        res["%s_len%d" % (k, len(c))] += 1
        (cycles if k == "CYCLE" else chains).append(c)
    return res, cycles, chains, conflicts, out, inn


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--typecons", default="/home/free/tmp/srccand1_typecons.json")
    ap.add_argument("--verdict", default="MAP_SUSPECT")
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--out", default="/home/free/tmp/mapsus1_cycles.json")
    args = ap.parse_args()

    m = load_map()
    inv = invert(m)
    allrows = json.load(open(args.typecons))
    rows = [r for r in allrows if r["verdict"] == args.verdict]
    edges, dropped = build_edges(rows, inv)
    print("rows verdict=%s: %d   edges built: %d   dropped: %s"
          % (args.verdict, len(rows), len(edges), dict(dropped) or "none"))

    res, cycles, chains, conflicts, out, inn = analyse(edges)

    if args.selfcheck:
        # NULL: same node count, same out-degree, heads drawn at random from the
        # SAME map.  Preserves everything about the construction except which
        # name each proposal points at.
        names = sorted(m.values())
        random.seed(7)
        nullres = collections.Counter()
        trials = 200
        ncyc = 0
        for _ in range(trials):
            ne = [(a, inv[random.choice(names)][0], r) for a, _b, r in edges]
            r2, c2, _, _, _, _ = analyse(ne)
            nullres.update(r2)
            ncyc += sum(len(c) for c in c2)
        print("\nSELFCHECK")
        print("  real: %s" % dict(res))
        print("  real nodes in closed cycles: %d" % sum(len(c) for c in cycles))
        print("  null (%d trials) mean nodes in closed cycles: %.3f" % (trials, ncyc / trials))
        print("  null classes: %s" % {k: round(v / trials, 3) for k, v in nullres.items()})
        fires = sum(len(c) for c in cycles) > 0
        print("  instrument FIRES on real data: %s" % fires)
        return 0 if fires else 1

    print("\n=== component classes ===")
    for k, v in sorted(res.items()):
        print("   %-24s %d" % (k, v))

    print("\n=== CLOSED CYCLES (permutations; injective by construction) ===")
    emap = {}
    for a, b, r in edges:
        emap.setdefault(a, []).append((b, r))
    for c in sorted(cycles, key=len):
        print("  cycle of %d:" % len(c))
        for a in c:
            b = list(out[a])[0]
            print("    %s  '%.68s'" % (a, m[a]))
            print("        <- take the name at %s  '%.60s'" % (b, m[b]))

    print("\n=== OPEN CHAINS / CONFLICTS (NOT repairable here) ===")
    for c in sorted(chains, key=len):
        print("  component of %d nodes:" % len(c))
        for a in sorted(c):
            o = sorted(out.get(a, ()))
            i = sorted(inn.get(a, ()))
            print("    %s in=%d out=%d  '%.60s'" % (a, len(i), len(o), m[a]))
    if conflicts:
        print("\n  CONFLICTING addresses (two different proposals):")
        for a, bs in conflicts.items():
            print("    %s  '%.55s'" % (a, m[a]))
            for b in bs:
                print("        -> %s '%.55s'" % (b, m[b]))

    json.dump({
        "classes": dict(res),
        "cycles": [[{"addr": a, "cur": m[a], "take_from": list(out[a])[0],
                     "new": m[list(out[a])[0]]} for a in c] for c in cycles],
        "chains": [sorted(c) for c in chains],
        "conflicts": conflicts,
    }, open(args.out, "w"), indent=1)
    print("\nwrote", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
