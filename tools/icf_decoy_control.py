#!/usr/bin/env python3
"""NEGATIVE (decoy) control for the ICF alias adjudicator.

The failure mode this guards against is the one lane CD-7 named explicitly: a
masked-byte comparator is SILENTLY VACUOUS. Two template instantiations --
``vector<Foo>::erase`` and ``vector<Bar>::erase`` -- emit identical machine bytes and
differ ONLY in which destructor they ``bl``. Masking relocated fields erases exactly
the discriminator, so a byte comparator "proves" a fold that never happened. An alias
built on that reasoning would permanently hide a real wrong-callee defect.

So the control is: assemble the decoy population = pairs that PASS masked-byte and size
equality (i.e. a naive comparator accepts them) but whose relocations resolve to
DIFFERENT, BOTH-NAMED symbols. Every one of these MUST be rejected. Selectivity is the
share of naive-comparator candidates the relocation gate throws out; if that share is
~0 the gate is inert, and if the decoys are not genuinely different functions the
control is vacuous in its own right -- so we print the actual differing callee names.
"""

import argparse
import collections
import glob
import json
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools"))
from icf_alias_build import collect, relocs_agree, vacuous, placeholder  # noqa: E402

sys.path.insert(0, str(PROJECT_ROOT / "scripts" / "harvest"))
try:
    from live_units import filter_live
except Exception:
    filter_live = None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=12)
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

    # naive-comparator candidate population: masked bytes + size equal
    buckets = collections.defaultdict(list)
    for F, rec in ours.items():
        buckets[(rec[0], rec[2])].append(F)

    naive = accepted = rejected = 0
    decoys = []
    for S, rt in retail.items():
        if S not in mapped or vacuous(rt):
            continue
        for F in buckets.get((rt[0], rt[2]), ()):
            if F == S:
                continue
            naive += 1
            ob = ours[F]
            if relocs_agree(rt, ob, mapped, True, None):
                accepted += 1
                continue
            rejected += 1
            if len(decoys) < args.limit:
                diff = [(rn, on) for (_ro, rn, _rt), (_oo, on, _ot) in zip(rt[1], ob[1])
                        if rn != on and not placeholder(rn) and not placeholder(on)]
                if diff:
                    decoys.append((S, F, diff[:2]))

    print("=== NEGATIVE / DECOY CONTROL ===")
    print("  naive masked-byte comparator would accept : %d pairs" % naive)
    print("  relocation gate REJECTS                   : %d" % rejected)
    print("  relocation gate accepts                   : %d" % accepted)
    print("  SELECTIVITY (share of naive candidates killed): %.1f%%"
          % (100.0 * rejected / naive if naive else 0.0))
    print("\n  Decoys must be REAL twins, not tooling noise -- the differing")
    print("  BOTH-NAMED callees below are the discriminator a byte compare erases:")
    for S, F, diff in decoys:
        print("\n   survivor %s" % S[:78])
        print("   folded?  %s" % F[:78])
        for rn, on in diff:
            print("      retail calls %-42s  ours calls %s" % (rn[:42], on[:42]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
