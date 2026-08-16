#!/usr/bin/env python3
"""Apply MAPID-1's withdrawals to scripts/symbol_aliases.json.

    python3 tools/mapid_apply_withdrawal.py --wt <wt> --withdraw <json> [--dry-run]

Per-MEMBERSHIP, never per-group; groups are KEPT even if emptied (a745039e: a
prune cost +94,616 B to reverse). Each removal records MAPID-1's own reason --
this lane's evidence is a retail-byte IDENTIFICATION of the blocking callee plus
a pigeonhole over map addresses, NOT ALIAS-2's COMDAT-size argument, so it does
not reuse alias_apply_withdrawal.py's note.
"""
import argparse, collections, json, os
from pathlib import Path

LANE = "MAPID-1 2026-08-16"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wt", required=True)
    ap.add_argument("--withdraw", required=True)
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    wt = Path(a.wt).resolve()
    ali = wt / "scripts/symbol_aliases.json"
    doc = json.loads(ali.read_text())
    groups = doc["groups"]

    dec = json.load(open(os.path.expanduser(a.withdraw)))["decisive"]
    byg = collections.defaultdict(list)
    for d in dec:
        byg[d["i"]].append(d)

    removed = missing = 0
    for i, xs in byg.items():
        g = groups[i]
        fold = list(g.get("folded", []))
        rec = g.setdefault("withdrawn", [])
        for x in xs:
            if x["folded"] in fold:
                fold.remove(x["folded"]); removed += 1
            else:
                missing += 1
            rec.append({"spelling": x["folded"], "lane": LANE,
                        "class": x["decisive"], "blocking_address": x["blocking_address"],
                        "why": x["detail"],
                        "note": "Measured cost of this withdrawal: 0 B. The whole "
                                "28,964 B NEEDS_MAP_ID class rests on the two "
                                "fn_827BCD38 memberships in group 1339, which are "
                                "LICENSED (identified), not withdrawn."})
        g["folded"] = fold
    print("memberships removed: %d (already absent: %d) across %d groups"
          % (removed, missing, len(byg)))
    assert removed == len(dec), "expected to remove %d, removed %d" % (len(dec), removed)
    if a.dry_run:
        print("DRY RUN -- no write"); return
    ali.write_text(json.dumps(doc, indent=1) + "\n")
    print("wrote %s" % ali)


if __name__ == "__main__":
    main()
