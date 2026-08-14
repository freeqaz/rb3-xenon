#!/usr/bin/env python3
"""incomplete_group_control.py -- the two checks that can refute lane INCOMPLETE-1's PROVEN set.

(A) RETAIL AMBIGUITY. A fold class is only usable as an alias if retail kept ONE
    address for it. If the target objs hold the same body at N>1 addresses, then
    either /OPT:ICF did not fold them (so our spelling's true address is one of
    several and the alias may forgive a genuinely WRONG callee) or the map names
    the class more than once. Either way the pair is AMBIGUOUS and is dropped.
    ⚠ This matters most for the nrel==0 thunk class, where the body is 8 bytes of
    `lwz r3,K(r3); blr` and nothing distinguishes one getter from another.

(B) FALSE-POSITIVE CONTROL, DRAWN FROM THE POPULATION BEING JUDGED. Lane
    ALIASAUDIT-2's warning: a prior FP calibration was scored against pairs where
    our build folds BY CONSTRUCTION, so it could not fail. The decoys here are
    built by RE-PAIRING the very same charged sites -- each retail-side name is
    matched against an our-side name drawn from a DIFFERENT charged pair -- so the
    decoy population has the same size/shape/vacuity distribution as the treatment
    and the gate has a real opportunity to say YES when it should say NO.
"""

import collections
import glob
import json
import os
import random
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from icf_alias_build import collect, relocs_agree, vacuous, placeholder  # noqa: E402
from icf_pair_adjudicate import chase, family  # noqa: E402
from incomplete_group_census import charged_pairs, load_groups  # noqa: E402


def gate(tgt, ours, rn, on, mapped, name2addr, allow_family=True):
    """The same decision incomplete_group_adjudicate makes, as a pure function."""
    if name2addr.get(on):
        return "REFUTED_map_resident"
    rt, ob = tgt.get(rn), ours.get(on)
    if rt is None or ob is None:
        return "UNDECIDABLE_absent"
    if rt[2] != ob[2]:
        return "REFUTED_size"
    if (vacuous(rt) or vacuous(ob)) and len(ob[1]) > 0:
        if allow_family:
            f = family(tgt, ours, on, rn)
            if len(f["retail_family"]) == 1 and f["our_slot0_matches_retail"] and f["excluded_by_slot0"]:
                return "PROVEN_family_thunk"
        return "UNDECIDABLE_vacuous"
    if rt[0] != ob[0]:
        return "REFUTED_bytes"
    if relocs_agree(rt, ob, mapped, strict=True):
        return "PROVEN_flatT1"
    if chase(tgt, ours, rn, on, mapped, out=[]):
        return "PROVEN_chase"
    if allow_family:
        f = family(tgt, ours, on, rn)
        if len(f["retail_family"]) == 1 and len(f["our_family"]) > 1 and f["excluded_by_slot0"]:
            return "PROVEN_family"
    return "REFUTED_relocs"


def main():
    tgt = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/src/**/*.obj"), recursive=True)), "o")
    owner, groups = load_groups(os.path.join(ROOT, "scripts/symbol_aliases.json"))
    m = json.load(open(os.path.join(ROOT, "scripts/target_symbol_map.json")))
    name2addr = collections.defaultdict(set)
    for a, n in m.items():
        for x in (n if isinstance(n, list) else [n]):
            if x:
                name2addr[x].add(a)
    mapped = set(name2addr)

    # ---- (A) retail ambiguity over the PROVEN set
    verdicts = json.load(open("/home/free/tmp/verdicts_membership.json"))
    proven = [r for r in verdicts if r["verdict"].startswith("PROVEN")]

    bybody = collections.defaultdict(list)
    for n, (mb, _r, _s) in tgt.items():
        bybody[mb].append(n)

    amb, uniq = [], []
    for r in proven:
        ob = ours[r["ours"]]
        twins = bybody.get(ob[0], [])
        # how many DISTINCT retail ADDRESSES do those twin names occupy?
        addrs = set()
        for t in twins:
            addrs |= set(name2addr.get(t, ()))
        r["retail_bodytwin_names"] = len(twins)
        r["retail_bodytwin_addrs"] = len(addrs)
        (amb if len(addrs) > 1 else uniq).append(r)

    print("=" * 78)
    print("(A) RETAIL AMBIGUITY over %d PROVEN pairs" % len(proven))
    print("=" * 78)
    print("  ONE retail address for the body  : %d pairs   [usable]" % len(uniq))
    print("  >1 retail address for the body   : %d pairs   [AMBIGUOUS -> drop]" % len(amb))
    h = collections.Counter(r["retail_bodytwin_addrs"] for r in amb)
    if h:
        print("  ambiguity histogram (addrs):", dict(sorted(h.items())[:10]))
    for r in sorted(amb, key=lambda x: -x["sites"])[:8]:
        print("    %3d sites  %d addrs  %s <- %s"
              % (r["sites"], r["retail_bodytwin_addrs"], r["retail"][:58], r["ours"][:58]))
    json.dump({"unambiguous": uniq, "ambiguous": amb},
              open("/home/free/tmp/incomplete_ambiguity.json", "w"), indent=1)

    # ---- (B) FP control by re-pairing the same charged sites
    sites, victims, _ = charged_pairs(tgt, ours)
    for p in [p for p in sites if placeholder(p[0]) or placeholder(p[1])]:
        del sites[p]
    for p in list(sites):
        gi, gj = owner.get(p[0]), owner.get(p[1])
        if gi is not None and gi == gj:
            del sites[p]
    surv = {g["survivor"]: i for i, g in enumerate(groups)}
    real = [p for p in sites if p[0] in surv and owner.get(p[1]) is None]

    rng = random.Random(20260814)
    rs = [p[0] for p in real]
    os_ = [p[1] for p in real]
    decoys = set()
    for _ in range(4000):
        a, b = rng.choice(rs), rng.choice(os_)
        if (a, b) in sites or a == b:
            continue
        decoys.add((a, b))
    decoys = sorted(decoys)

    res = collections.Counter()
    for a, b in decoys:
        res[gate(tgt, ours, a, b, mapped, name2addr)] += 1
    print()
    print("=" * 78)
    print("(B) FALSE-POSITIVE CONTROL -- %d decoys re-paired from the SAME charged sites" % len(decoys))
    print("=" * 78)
    for k, v in res.most_common():
        print("  %-26s %5d  (%.2f%%)" % (k, v, 100.0 * v / len(decoys)))
    p = sum(v for k, v in res.items() if k.startswith("PROVEN"))
    print("  => decoy PROVEN rate: %d / %d = %.2f%%" % (p, len(decoys), 100.0 * p / len(decoys)))
    print("     treatment PROVEN rate: %d / %d = %.2f%%"
          % (len(proven), len(real), 100.0 * len(proven) / len(real)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
