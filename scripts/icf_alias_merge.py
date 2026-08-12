#!/usr/bin/env python3
"""Merge a generated alias DELTA into an alias file, by address.

An alias group is identified by its ADDRESS, not by its survivor spelling --
`tools/gen_symbol_alias_map.py` renders every member as a `<name> <address>`
line and objdiff's `parse_msvc_map` buckets by address.  So merging is a union
of name sets per address.  The invariant worth watching is ONE NAME, ONE
ADDRESS; it is REPORTED rather than enforced, because the installed set already
violates it 842 times -- see the comment on the check for what objdiff actually
does with a duplicated name.  `--strict` turns the report into a refusal.

    # measure a delta without touching the tracked file
    python3 scripts/icf_alias_merge.py --into scripts/symbol_aliases.json \
        --delta <delta.json> --out /tmp/merged.json
    python3 tools/gen_symbol_alias_map.py --aliases /tmp/merged.json

    # install it
    python3 scripts/icf_alias_merge.py --into scripts/symbol_aliases.json \
        --delta <delta.json> --in-place && python3 tools/gen_symbol_alias_map.py
"""

import argparse
import json
import sys
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--into", required=True)
    ap.add_argument("--delta", required=True)
    ap.add_argument("--out", default="")
    ap.add_argument("--in-place", action="store_true")
    ap.add_argument("--strict", action="store_true",
                    help="refuse if any name lands at more than one address")
    args = ap.parse_args()
    if not args.out and not args.in_place:
        sys.exit("need --out or --in-place")

    base = json.loads(Path(args.into).read_text())
    delta = json.loads(Path(args.delta).read_text())

    by_addr = {}
    order = []
    for g in base.get("groups", []):
        a = g["address"].lower()
        if a not in by_addr:
            by_addr[a] = dict(g)
            by_addr[a]["folded"] = list(g.get("folded", []))
            order.append(a)
        else:
            by_addr[a]["folded"] += g.get("folded", [])

    added_groups = added_names = 0
    for g in delta.get("groups", []):
        a = g["address"].lower()
        if a in by_addr:
            cur = by_addr[a]
            known = {cur["survivor"], *cur["folded"]}
            new = [f for f in g.get("folded", []) if f not in known]
            if g["survivor"] not in known:
                new.append(g["survivor"])
            cur["folded"] += new
            added_names += len(new)
            cur["evidence"] = cur.get("evidence", "") + " | " + g.get("evidence", "")
        else:
            by_addr[a] = {k: v for k, v in g.items() if not k.startswith("_meta")}
            by_addr[a]["folded"] = list(g.get("folded", []))
            order.append(a)
            added_groups += 1
            added_names += len(by_addr[a]["folded"])

    # ONE NAME, ONE ADDRESS -- reported, not enforced, because THE INSTALLED SET
    # ALREADY VIOLATES IT (measured 2026-08-12 on the 1,347-group file: 842 of
    # its 6,209 names sit in more than one group, the worst in 67).  objdiff's `parse_msvc_map` builds
    # one name->group entry per symbol and `equivalences.insert` is LAST-WINS over
    # a HashMap iteration, so for a duplicated name the forward lookup lands in an
    # arbitrary one of its groups -- nondeterministically across runs.  It is not
    # an over-merge (per-address group sets are never unioned) and `reloc_eq`
    # checks BOTH directions, so the survivor's side still resolves; but a pair
    # whose two names are both duplicated is a coin flip.  `--strict` refuses.
    seen, dup = {}, {}
    for a in order:
        g = by_addr[a]
        g["folded"] = sorted(set(g["folded"]) - {g["survivor"]})
        for n in (g["survivor"], *g["folded"]):
            if seen.setdefault(n, a) != a:
                dup.setdefault(n, {seen[n]}).add(a)
    if dup:
        worst = max(dup.items(), key=lambda kv: len(kv[1]))
        print("WARNING: %d names appear at more than one address (worst: %d, %r)"
              % (len(dup), len(worst[1]), worst[0][:70]), file=sys.stderr)
        if args.strict:
            sys.exit("REFUSING (--strict): one name, one address.")

    out = dict(base)
    out["groups"] = [by_addr[a] for a in order]
    text = json.dumps(out, indent=1) + "\n"
    Path(args.out or args.into).write_text(text)
    print("merged: %d groups (+%d new, +%d names) -> %s"
          % (len(out["groups"]), added_groups, added_names, args.out or args.into))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
