#!/usr/bin/env python3
"""incomplete_group_install.py -- price and install lane INCOMPLETE-1's surviving memberships.

Installs ONLY pairs that pass every gate, including the retail-UNIQUENESS gate
that the batch generator reports but does not enforce:

    a fold class is usable as an alias only if retail kept ONE address for it.
    If the target objs hold the body at N>1 addresses, our call site's true
    target is one of several and the alias may forgive a genuinely WRONG callee.

⚠ That gate is EXACT for the nrel==0 subset (masked body == raw body, so
"same body" is literal) and deliberately CONSERVATIVE for nrel>0 (masked-equal
twins that would NOT actually fold, because their relocation targets differ, are
counted as ambiguity and cost us the pair). Erring strict is the intended
direction: an unproven alias lifts name_check BY CONSTRUCTION, so a false
membership is not merely a miss, it is a fabricated gain.

--price only reports; --install writes scripts/symbol_aliases.json.
"""

import argparse
import collections
import glob
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from icf_alias_build import collect, placeholder  # noqa: E402
from incomplete_group_census import charged_pairs, load_groups  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--install", action="store_true")
    args = ap.parse_args()

    keep = json.load(open("/home/free/tmp/incomplete_ambiguity.json"))["unambiguous"]
    tgt = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/src/**/*.obj"), recursive=True)), "o")
    owner, groups = load_groups(os.path.join(ROOT, "scripts/symbol_aliases.json"))

    sites, victims, _ = charged_pairs(tgt, ours)
    for p in [p for p in sites if placeholder(p[0]) or placeholder(p[1])]:
        del sites[p]
    for p in list(sites):
        gi, gj = owner.get(p[0]), owner.get(p[1])
        if gi is not None and gi == gj:
            del sites[p]

    keepset = {(r["retail"], r["ours"]) for r in keep}
    row_charges = collections.defaultdict(set)
    for p in sites:
        for r in victims[p]:
            row_charges[r].add(p)

    rep = json.load(open(os.path.join(ROOT, "build/45410914/report.json")))
    mpn, fuzzy, size, unit = {}, {}, {}, {}
    for u in rep["units"]:
        for f in u.get("functions", []):
            mpn[f["name"]] = float(f["match_percent_normalized"])
            fuzzy[f["name"]] = float(f.get("fuzzy_match_percent", 0.0))
            size[f["name"]] = int(f["size"])
            unit[f["name"]] = u["name"]

    rows = set()
    for p in keepset:
        rows |= victims[p]
    cross = [r for r in rows
             if fuzzy.get(r, 100.0) < 100.0 and mpn.get(r, 0.0) >= 100.0
             and row_charges[r] <= keepset]
    blocked = [r for r in rows
               if fuzzy.get(r, 100.0) < 100.0 and not (row_charges[r] <= keepset and mpn.get(r, 0.0) >= 100.0)]

    print("=" * 74)
    print("PRICING lane INCOMPLETE-1 -- %d unambiguous memberships" % len(keepset))
    print("=" * 74)
    print("  rows touched by a kept pair          : %d" % len(rows))
    print("  rows that CROSS (all charges cleared): %d" % len(cross))
    print("  PREDICTED matched_code delta         : +%d B" % sum(size.get(r, 0) for r in cross))
    print("  rows blocked by another charge/mpn   : %d" % len(blocked))
    byunit = collections.Counter()
    for r in cross:
        byunit[unit.get(r, "?")] += size.get(r, 0)
    print("\n  top units by predicted bytes:")
    for u, b in byunit.most_common(10):
        print("    %8d B  %s" % (b, u))

    if not args.install:
        return 0

    doc = json.load(open(os.path.join(ROOT, "scripts/symbol_aliases.json")))
    added = collections.Counter()
    byret = collections.defaultdict(list)
    for r in keep:
        byret[r["retail"]].append(r)
    for g in doc["groups"]:
        recs = byret.get(g["survivor"])
        if not recs:
            continue
        fold = list(g.get("folded", []))
        for r in recs:
            if r["ours"] in fold:
                continue
            fold.append(r["ours"])
            added[g["survivor"]] += 1
        g["folded"] = fold
        note = (" INCOMPLETE-1 (2026-08-14): %d spelling(s) added by CHARGED-SITE enumeration -- "
                "the group existed but was a SUBSET, the same blind spot ONMSG-1 found "
                "(a pairwise comparator finds only SOME members). Each added spelling is "
                "map-absent, same-size, byte-identical to retail at the survivor address modulo "
                "relocated fields with relocation target names agreeing (flat T1 or chase), and "
                "passes a retail-UNIQUENESS gate: the body occupies exactly ONE retail address, "
                "so the alias cannot forgive a wrong callee. witness=%s"
                % (added[g["survivor"]], "; ".join(sorted({x["verdict"] for x in recs}))))
        g["evidence"] = g.get("evidence", "") + note
    json.dump(doc, open(os.path.join(ROOT, "scripts/symbol_aliases.json"), "w"), indent=1)
    print("\ninstalled %d spellings across %d groups" % (sum(added.values()), len(added)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
