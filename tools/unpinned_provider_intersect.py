#!/usr/bin/env python3
"""Intersect PINHOME-1's ORPHAN PIN population (orphan_pins.json: target rows
whose paired base obj does not define the name, so they read 0% by construction)
with this lane's COMPILED-BUT-UNPINNED unit census.

An orphan pin whose name is OWNED by a compiled-but-unpinned unit has a
different, sharper fix than the generic re-homing PINHOME-1 applied: the
provider TU exists and compiles, it simply has no splits.txt heading, so its
address range is swallowed by whichever pin encloses it.  Giving that unit its
own heading makes the row pairable against its own base obj.

Reports, separately:
  * provider is COMPILED-BUT-UNPINNED  -> fix = add a splits heading (this lane)
  * provider is some other PINNED unit -> fix = move the pin (PINHOME-1's class)
  * no compiled obj owns the name      -> absent source / wrong map name; a pin
                                          change cannot help at all
"""
import json
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from coff_owned import analyze as coff_owned  # noqa: E402
from unpinned_unit_census import (ROOT, basename_alias_map, load_objects,  # noqa: E402
                                  splits_headings)


def main():
    orphans = json.loads((ROOT / "orphan_pins.json").read_text())
    objects = load_objects()
    aliases, _ = basename_alias_map(objects)
    headings = splits_headings()
    pinned = set()
    for h in headings:
        pinned.add(h if h in objects else aliases.get(h))
    pinned.discard(None)

    compiled = {pk for pk, (_l, sp) in objects.items() if (ROOT / sp).exists()}
    unpinned = compiled - pinned

    # name -> providers, split by pin status
    owner_of = defaultdict(list)
    for pk in compiled:
        objp = ROOT / "build" / "45410914" / "src" / (pk.rsplit(".", 1)[0] + ".obj")
        if not objp.exists():
            continue
        owned, _shared = coff_owned(objp)
        for n in owned:
            owner_of[n].append(pk)

    buckets = defaultdict(list)
    for row in orphans:
        provs = owner_of.get(row["symbol"], [])
        up = [p for p in provs if p in unpinned]
        pp = [p for p in provs if p in pinned]
        if up:
            buckets["UNPINNED_PROVIDER"].append((row, up, pp))
        elif pp:
            buckets["PINNED_PROVIDER"].append((row, up, pp))
        else:
            buckets["NO_PROVIDER"].append((row, up, pp))

    tot_rows = len(orphans)
    tot_bytes = sum(r["size"] for r in orphans)
    print(f"orphan pins total: {tot_rows} rows / {tot_bytes:,} B")
    for k in ("UNPINNED_PROVIDER", "PINNED_PROVIDER", "NO_PROVIDER"):
        rows = buckets[k]
        b = sum(r["size"] for r, _u, _p in rows)
        print(f"  {k:20s} {len(rows):4d} rows / {b:8,} B")
    print()
    print("=== UNPINNED_PROVIDER detail (fixable by adding a splits heading) ===")
    for row, up, _pp in sorted(buckets["UNPINNED_PROVIDER"],
                               key=lambda x: -x[0]["size"]):
        print(f"  {row['size']:6d} B  {row['unit']:44s} {row['symbol'][:60]}")
        print(f"          provider(s): {up}")


if __name__ == "__main__":
    main()
