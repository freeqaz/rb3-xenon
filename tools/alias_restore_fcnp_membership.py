#!/usr/bin/env python3
"""Restore ONE FABRICATED_CLOSURE_NOT_PARTITION-withdrawn alias membership, on evidence.

Lane W16-GM (2026-09-30). Mirrors the record shape lanes W16-Y / W16-FM / W16-GK
used by hand in group 0x823d14c0: the withdrawn record is REMOVED from
`withdrawn[]`, preserved verbatim under `restored[].withdrawal_record_superseded`,
and the spelling is appended to `folded` (kept sorted, as that group is).
`tools/alias_withdrawal_audit.py`'s live-and-withdrawn count therefore does not move.

One spelling per invocation, on purpose: each restoration is its own commit with its
own evidence text. Refuses if the spelling is already live in the group, is not
FCNP-withdrawn from it, or if the file's serialization would change for any reason
other than this edit (format pinned to what is on disk: indent=1, ensure_ascii=True,
no trailing newline -- measured 2026-09-30).

    python3 tools/alias_restore_fcnp_membership.py --address 0x823d14c0 \
        --spelling '?insert@...' --lane 'W16-GM 2026-09-30' \
        --verdict '...' --why-file ~/tmp/why.txt
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALIASES = ROOT / "scripts/symbol_aliases.json"
FCNP = "FABRICATED_CLOSURE_NOT_PARTITION"


def dump(d):
    return json.dumps(d, indent=1, ensure_ascii=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--address", required=True)
    ap.add_argument("--spelling", required=True)
    ap.add_argument("--lane", required=True)
    ap.add_argument("--verdict", required=True)
    ap.add_argument("--why-file", required=True, help="text file: why_the_original_premise_failed")
    ap.add_argument("--aliases", default=str(ALIASES))
    a = ap.parse_args()

    raw = open(a.aliases).read()
    d = json.loads(raw)
    if dump(d) != raw:
        sys.exit("REFUSE: on-disk serialization differs from indent=1/ensure_ascii=True; "
                 "a rewrite would produce a whole-file diff. Inspect first.")
    gs = [g for g in d["groups"] if (g.get("address") or "").lower() == a.address.lower()]
    if len(gs) != 1:
        sys.exit(f"REFUSE: {len(gs)} groups at {a.address}")
    g = gs[0]
    if a.spelling in g["folded"] or a.spelling == g["survivor"]:
        sys.exit("REFUSE: spelling already live in this group")
    w = g.get("withdrawn") or []
    hits = [e for e in w if isinstance(e, dict) and e.get("spelling") == a.spelling and e.get("class") == FCNP]
    if len(hits) != 1:
        sys.exit(f"REFUSE: {len(hits)} FCNP withdrawn records for this spelling in this group")
    rec = hits[0]
    g["withdrawn"] = [e for e in w if e is not rec]
    why = open(a.why_file).read().strip()
    g.setdefault("restored", []).append({
        "spelling": a.spelling,
        "lane": a.lane,
        "class": FCNP,
        "verdict": a.verdict,
        "why_the_original_premise_failed": why,
        "withdrawal_record_superseded": rec,
    })
    g["folded"] = sorted(set(g["folded"]) | {a.spelling})
    open(a.aliases, "w").write(dump(d))
    print(f"RESTORED {a.spelling[:80]}... into {a.address}: folded now {len(g['folded'])}, "
          f"withdrawn now {len(g['withdrawn'])}, restored now {len(g['restored'])}")


if __name__ == "__main__":
    main()
