#!/usr/bin/env python3
"""laneBU4 -- LIVE EXACT_AMBIG funnel producer.  THE MISSING INPUT.

`emit_reloc_frag.py --funnels` consumes JSON rows tagged cls=="EXACT_AMBIG" for
the LIVE (unmapped) pool.  No producer for those rows existed anywhere on main or
on laneAS-B, which is why the EMITTER was never runnable and only the calibration
path (heldout_reloc.py, already-mapped VAs where truth is known) could ever be
run.  This closes that gap.

Contract, read off emit_reloc_frag.main():  the emitter consumes exactly four
fields per row -- cls / unit / va / size -- and RE-DERIVES the candidate class
itself from the same masked-byte grouping.  So this producer is a *nomination*
pass, not a decision pass: it answers "at which target VAs is identification
currently blocked by a reloc-masked byte tie?" and leaves adjudication to the
emitter.  The grouping below is deliberately byte-identical to the emitter's
(same supply filter, same dedup-by-stripped-name) so that a row nominated here
is never silently dropped there for a different reason.

LIVE = the target VA has no entry in scripts/target_symbol_map.json.  That is
the population the discriminator exists to resolve: more than one candidate base
symbol is byte-equal under relocation masking, so byte identity cannot name it.

Unit universe: D.unit_iter, which excludes auto_* carve units.  This matches the
population heldout_reloc.py calibrates on -- emitting over a wider universe than
was calibrated would silently void the measured precision.

Usage:
  livefunnel.py --worktree WT --out funnel.json [--include-mapped]
"""
import argparse
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reloclib as R          # noqa: E402
import relocdisc as D         # noqa: E402


def build(wt: Path, include_mapped=False):
    S = R.load_S(wt)
    cur = json.loads((wt / "scripts/target_symbol_map.json").read_text())
    mapped_vas = set()
    for k, v in cur.items():
        if isinstance(v, str) and k.startswith("0x"):
            try:
                mapped_vas.add(int(k, 16))
            except ValueError:
                pass

    rows = []
    stats = Counter()
    units = list(D.unit_iter(wt))
    for k, (name, tobj, tasm, cobj) in enumerate(units):
        if k % 100 == 0:
            print(f"[{k}/{len(units)}] {name}", file=sys.stderr)
        if not (tobj.exists() and tasm.exists() and cobj.exists()):
            stats["unit_missing_artifact"] += 1
            continue
        try:
            tf = R.target_funcs(tasm)
            _, _, tsyms = S._parse_coff(tobj)
            tnames = {S.anon_ns_strip(s["name"]) for s in tsyms}
            bf, _ = R.base_funcs(cobj)
        except Exception as e:
            print(f"  parse fail {name}: {e}", file=sys.stderr)
            stats["unit_parse_fail"] += 1
            continue
        stats["units"] += 1

        # supply := unpaired base code symbols (identical filter to the emitter)
        supply = []
        for f in bf:
            nm = S.anon_ns_strip(f["name"])
            if nm in tnames or S.is_internal(f["name"]):
                continue
            if D.FUNCLET_LIKE.match(f["name"]):
                continue
            supply.append(f)
        by_bytes = defaultdict(list)
        for f in supply:
            by_bytes[f["masked"]].append(f)

        for va, ti in tf.items():
            stats["target_fns"] += 1
            if va in mapped_vas:
                stats["skip_already_mapped"] += 1
                if not include_mapped:
                    continue
            grp = by_bytes.get(ti["masked"], [])
            if not grp:
                stats["no_byte_class"] += 1
                continue
            seen = []
            for g in grp:
                n2 = S.anon_ns_strip(g["name"])
                if n2 not in seen:
                    seen.append(n2)
            if len(seen) < 2:
                stats["unique_byte_match"] += 1
                continue
            stats["EXACT_AMBIG"] += 1
            rows.append(dict(cls="EXACT_AMBIG", unit=name, va=va,
                             size=ti["size"], n=len(seen),
                             nrel=len(ti["relocs"]),
                             mapped=bool(va in mapped_vas)))
    return rows, stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--include-mapped", action="store_true",
                    help="also emit rows for already-mapped VAs (diagnostic "
                         "only; the emitter drops them)")
    args = ap.parse_args()

    wt = Path(args.worktree).resolve()
    rows, stats = build(wt, args.include_mapped)
    Path(args.out).write_text(json.dumps(rows, indent=1))

    print(f"\n=== LIVE EXACT_AMBIG funnel  n={len(rows)} -> {args.out} ===")
    for k, v in stats.most_common():
        print(f"  {k:26s} {v:7d}")

    def band(s):
        return "<=32" if s <= 32 else ("33-68" if s <= 68 else ">68")
    print("\n  size bands:")
    for b, c in Counter(band(r["size"]) for r in rows).most_common():
        print(f"    {b:6s} {c:6d}")
    print("\n  class size n:")
    for b, c in sorted(Counter(min(r["n"], 9) for r in rows).items()):
        print(f"    n={b}{'+' if b == 9 else ' '}   {c:6d}")
    print("\n  top units:")
    for u, c in Counter(r["unit"] for r in rows).most_common(15):
        print(f"    {u:44s} {c:5d}")


if __name__ == "__main__":
    main()
