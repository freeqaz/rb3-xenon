#!/usr/bin/env python3
"""MAPSUS-1 AXIS 3: the ONE channel in this population that cannot route through the map.

THE PROBLEM WITH EVERY OTHER CHANNEL.  A charged pair accuses two names.  Every
obvious way to adjudicate it -- "what does retail at this address CALL?", "what
calls it?", "what are its neighbours?" -- reads names that the SAME map supplied,
so a wholesale rotation of a cluster is self-consistent and every such channel
confirms it.  ``ms1_bytes.py`` shows this concretely: at the address the map
calls ``__final_insertion_sort<MoveDetector>`` retail calls something the map
calls ``__insertion_sort<FileCacheEntry>``.  That proves the map is internally
INCONSISTENT; it cannot say which of the two rows is the wrong one, because
transposing the ``__insertion_sort`` names repairs the inconsistency just as well
as transposing the ``__final_insertion_sort`` names.  A lane has already paid
1,248 B for treating two such channels as independent.

THE CHANNEL THAT DOES NOT.  ``collect()`` returns bodies with every RELOCATED
field masked out.  What survives is opcodes, registers and immediates -- values
the linker wrote from the compiler's output, which no symbol map can influence.
So:

    for a candidate address X, compare retail's MASKED WORDS at X against OUR
    compilation of each candidate name.  Whichever name our compiler reproduces
    word-for-word is the function that lives at X.

That is a positive identification of the CODE at an address, independent of what
anything is called.  It is the same T1 evidence tier ``icf_alias_build`` uses,
applied to the assignment question instead of the fold question.

WHEN IT ANSWERS AND WHEN IT DOES NOT.  It answers only when the two candidate
names compile to DIFFERENT masked words.  For a true masked class -- template
twins differing solely in a ``bl`` -- the words are identical, every candidate
matches, and the channel is silent.  Silence is reported as UNDECIDABLE_masked,
never as agreement.

⚠ THE VERDICT NAMES ARE WRITTEN TO MAKE THE REFUTING DIRECTION LOUD.  This file
exists to attack ms1_cycles' proposals, not to dress them up:
  MAP_CONFIRMED   retail@A reproduces our(map name at A) and NOT our(other) =>
                  the map is RIGHT here and the proposed swap is REFUTED.
  SWAP_CONFIRMED  retail@A reproduces our(other) and NOT our(map name at A).
  UNDECIDABLE_masked / NEITHER / ABSENT otherwise.

--selfcheck runs the identical comparison over a population where the answer is
known independently -- 300 random map rows whose name our compiler also emits --
and requires a high MAP_CONFIRMED rate.  A word comparator that matched nothing
would report every proposal UNDECIDABLE_masked, which reads exactly like "masked
class" and would silently protect any proposal from refutation.
"""

import argparse
import collections
import glob
import json
import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from icf_alias_build import collect  # noqa: E402

TARGET_GLOB = str(ROOT / "build" / "45410914" / "obj" / "**" / "*.obj")
OURS_GLOB = str(ROOT / "build" / "45410914" / "src" / "**" / "*.obj")
MIN_WORDS = 4


def words_equal(r_t, r_o):
    """Masked words + size equal.  None when either side is missing or too small
    to carry information (a 4-byte ``blr`` compares equal to everything)."""
    if r_t is None or r_o is None:
        return None
    if r_t[2] < MIN_WORDS * 4 or r_o[2] < MIN_WORDS * 4:
        return None
    return r_t[0] == r_o[0] and r_t[2] == r_o[2]


def adjudicate(tgt, ours, addr_name, other_name):
    """At the address whose MAP name is addr_name, is the code ours(addr_name)
    or ours(other_name)?"""
    rt = tgt.get(addr_name)
    a = words_equal(rt, ours.get(addr_name))
    b = words_equal(rt, ours.get(other_name))
    if rt is None or (a is None and b is None):
        return "ABSENT", a, b
    if a and b:
        return "UNDECIDABLE_masked", a, b
    if a and not b:
        return "MAP_CONFIRMED", a, b
    if b and not a:
        return "SWAP_CONFIRMED", a, b
    return "NEITHER", a, b


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cycles", default="/home/free/tmp/mapsus1_cycles.json")
    ap.add_argument("--typecons", default="/home/free/tmp/srccand1_typecons.json")
    ap.add_argument("--all", action="store_true",
                    help="adjudicate every MAP_SUSPECT proposal, not just the cycles")
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--out", default="/home/free/tmp/mapsus1_words.json")
    args = ap.parse_args()

    tgt = collect(glob.glob(TARGET_GLOB, recursive=True), "target")
    ours = collect(glob.glob(OURS_GLOB, recursive=True), "ours")

    if args.selfcheck:
        m = {k: v for k, v in json.load(
            open(ROOT / "scripts" / "target_symbol_map.json")).items()
            if k.startswith("0x") and isinstance(v, str)}
        cand = [n for n in m.values() if n in tgt and n in ours]
        random.seed(3)
        sample = random.sample(cand, min(300, len(cand)))
        c = collections.Counter()
        for n in sample:
            c[words_equal(tgt[n], ours[n])] += 1
        n_ok, n_no, n_na = c[True], c[False], c[None]
        print("SELFCHECK: 300 random map rows our compiler also emits")
        print("   words EQUAL  %d   DIFFER %d   too-small/absent %d" % (n_ok, n_no, n_na))
        # The comparator must be able to say YES on a real population.  A
        # comparator that never says YES makes every proposal UNDECIDABLE.
        ok = n_ok > 0.20 * len(sample)
        print("   comparator can CONFIRM identity on real data (>20%%): %s" % ok)
        return 0 if ok else 1

    props = []
    if args.all:
        m = {k: v for k, v in json.load(
            open(ROOT / "scripts" / "target_symbol_map.json")).items()
            if k.startswith("0x") and isinstance(v, str)}
        inv = collections.defaultdict(list)
        for a, n in m.items():
            inv[n].append(a)
        for r in json.load(open(args.typecons)):
            if r["verdict"] != "MAP_SUSPECT":
                continue
            ra, oa = inv[r["retail_name"]], inv[r["our_name"]]
            if len(ra) == 1 and len(oa) == 1:
                props.append((ra[0], r["retail_name"], r["our_name"], "row"))
    else:
        for c in json.load(open(args.cycles))["cycles"]:
            if len(c) != 2:
                continue
            props.append((c[0]["addr"], c[0]["cur"], c[1]["cur"], "cycle"))
            props.append((c[1]["addr"], c[1]["cur"], c[0]["cur"], "cycle"))

    out = []
    print("\n=== word-level identification of the code at each accused address ===")
    for (addr, mapname, other, kind) in props:
        v, a, b = adjudicate(tgt, ours, mapname, other)
        out.append(dict(addr=addr, map_name=mapname, other=other, verdict=v,
                        matches_map=a, matches_other=b, kind=kind))
        print("  %-12s %-34s %s" % (addr, v, mapname[:60]))
        if v in ("SWAP_CONFIRMED", "NEITHER"):
            print("        vs %s" % other[:70])

    print("\n=== summary ===")
    for k, v in collections.Counter(r["verdict"] for r in out).most_common():
        print("   %-26s %d" % (k, v))
    json.dump(out, open(args.out, "w"), indent=1)
    print("wrote", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
