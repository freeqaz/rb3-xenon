#!/usr/bin/env python3
"""W16-AD: audit the refutation guard's own conservatism.

`Witnesser.guard_pair` calls a pair INCONCLUSIVE_TWIN when the two retail
bodies compare identical under `masked_body`, which masks branch displacements
AND 16-bit immediates.  CD-7's 51-surplus class -- the one the brief's guard is
aimed at -- is narrower: bodies identical *INCLUDING call targets*.  Bodies
identical only when call targets are IGNORED are CD-7's other population (3,967
survivors in 1,061 groups) and are exactly what `/OPT:ICF` is EXPECTED to leave
alone, because differing `bl` targets make the COMDATs non-identical.

So the guard is conservative in the safe direction (it yields INCONCLUSIVE
rather than a refutation, and a false refutation is the costly error), but its
reach is wider than advertised.  This measures the cost of that: how many rows
the guard blocked, and how many of those separate cleanly under the STRICT test.

It CHANGES NOTHING.  It reports, so the deliverable can state the refuted count
as a lower bound with a number attached instead of a hedge.
"""
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from wrong_callee_triage import Image        # noqa: E402

ROWS = ROOT / "docs/decomp/W16AD_fold_witness_2026-09-14.json"


def main():
    img = Image(ROOT / "orig" / "45410914" / "band.exe")
    begins = {}
    for name, _sva, raw, rawsz in img.secs:
        if name == ".pdata":
            for i in range(0, rawsz - 7, 8):
                b, f = struct.unpack_from(">II", img.data, raw + i)
                if b:
                    begins[b] = ((f >> 8) & 0x3FFFFF) * 4

    def words(va):
        n = begins.get(va)
        o = img.off(va)
        if not n or o is None:
            return None
        return [struct.unpack_from(">I", img.data, o + 4 * i)
                for i in range(n // 4)]

    def raw_words(va):
        n = begins.get(va)
        o = img.off(va)
        if not n or o is None:
            return None
        return tuple(struct.unpack_from(">I", img.data, o + 4 * i)[0]
                     for i in range(n // 4))

    rows = json.loads(ROWS.read_text())
    tot = {"INCONCLUSIVE_TWIN": 0, "flips_to_SEPARATE": 0,
           "stays_TWIN": 0, "unreadable": 0}
    bytes_flip = 0
    flips = []
    for r in rows:
        if r.get("witness_verdict") != "WITNESS_INCONCLUSIVE":
            continue
        for p in r.get("pairs", []):
            g = p.get("guards") or {}
            if g.get("verdict") != "INCONCLUSIVE_TWIN":
                continue
            tot["INCONCLUSIVE_TWIN"] += 1
            a = p.get("N_strong") or p.get("N_medium") or []
            b = p.get("S_strong") or p.get("S_medium") or []
            if not a or not b:
                tot["unreadable"] += 1
                continue
            av, bv = int(a[0], 16), int(b[0], 16)
            wa, wb = raw_words(av), raw_words(bv)
            if wa is None or wb is None:
                tot["unreadable"] += 1
                continue
            # STRICT: identical INCLUDING call targets
            if wa == wb:
                tot["stays_TWIN"] += 1
            else:
                tot["flips_to_SEPARATE"] += 1
                bytes_flip += int(r.get("bytes", 0))
                flips.append((r["gi"], r.get("addr"), int(r.get("bytes", 0)),
                              f"0x{av:08x}", f"0x{bv:08x}",
                              sum(1 for x, y in zip(wa, wb) if x != y),
                              len(wa), len(wb)))
            break

    print("GUARD CONSERVATISM AUDIT  (INCONCLUSIVE_TWIN rows only)")
    print("=" * 74)
    for k, v in tot.items():
        print(f"   {k:22s} {v}")
    print(f"\nrows that would flip to SEPARATE under the STRICT (CD-7 51-surplus)")
    print(f"test, i.e. refutations the guard currently blocks: "
          f"{tot['flips_to_SEPARATE']} rows / {bytes_flip} B")
    print(f"\n{'gi':>6} {'parent addr':>12} {'B':>6}  {'c_N retail':>10} "
          f"{'c_S retail':>10}  diffwords  lenN lenS")
    for gi, addr, by, av, bv, nd, la, lb in sorted(flips,
                                                   key=lambda x: -x[2])[:40]:
        print(f"{gi:6d} {addr:>12} {by:6d}  {av:>10} {bv:>10}  "
              f"{nd:9d}  {la:4d} {lb:4d}")
    if len(flips) > 40:
        print(f"   ... and {len(flips)-40} more")


if __name__ == "__main__":
    main()
