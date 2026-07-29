#!/usr/bin/env python3
"""In-place ROW DELETER for scripts/target_symbol_map.json (lane BP-7).

WHY THIS EXISTS.  map_repoint_apply.py can only change the NAME an address maps
to; tu5_map_apply_fragment.py can only INSERT.  Neither can remove a row, and
removal is a first-class operation once you accept that some entries are
actively harmful: a row naming a class that does not exist in the retail binary
does not merely fail to help, it EARNS FALSE CREDIT.  The renamer rewrites the
target obj's anonymous `fn_<addr>` to that name, our identically-shaped COMDAT
pairs with it, and (because objdiff runs functionRelocDiffs=None, so the only
distinguishing field -- the string relocation of an OBJ_CLASSNAME body -- is
invisible) it scores a clean 100.0%.  It also BLOCKS the real owner: the VA is
taken, so the correct class's COMDAT can never pair there.

Deleting such a row LOWERS `matched_functions`.  That is the intended direction,
not a regression: the metric was over-counting.  Price a deletion wave by the
count of false 100s removed, and state the drop explicitly.

HARD PROJECT INVARIANT (shared with the other two appliers): never
json.dump-rewrite the map.  It is a ~27k-entry, 1-space-indent file whose
formatting is a load-bearing convention.  This tool removes exactly the whole
physical lines belonging to the named addresses and leaves every other byte
identical.

FRAGMENT FORMAT -- a list of explicit, justified deletions:
    [{"va": "0x823c7338",
      "old": "?Load@FlowIf@@UAAXAAVBinStream@@@Z",
      "op":  "delete",
      "why": "<per-row justification, REQUIRED>"}, ...]

`old` is asserted against the file so a fragment written against a stale map
fails loudly rather than deleting someone else's landing.

USAGE
    python3 scripts/harvest/map_row_delete.py <fragment.json> \
            scripts/target_symbol_map.json [--dry-run]
"""

import argparse
import json
import re
from pathlib import Path


def apply_deletes(frag_path, map_path, dry_run=False):
    frag = json.loads(Path(frag_path).read_text())
    if not isinstance(frag, list):
        raise SystemExit("fragment must be a LIST of op dicts")
    text = Path(map_path).read_text()
    lines = text.split("\n")

    idx = {}
    for i, ln in enumerate(lines):
        m = re.match(r'^\s*"(0x[0-9a-fA-F]+)":\s*(".*?"),\s*$', ln)
        if m:
            idx.setdefault(m.group(1).lower(), i)

    drop = set()
    for row in frag:
        for k in ("va", "old", "op", "why"):
            if not row.get(k):
                raise SystemExit("fragment row missing required %r: %r" % (k, row))
        if row["op"] != "delete":
            raise SystemExit("this tool only handles op=delete, got %r" % row["op"])
        va = row["va"].lower()
        if va not in idx:
            raise SystemExit("address not present as a simple string entry: %s" % va)
        i = idx[va]
        m = re.match(r'^\s*"0x[0-9a-fA-F]+":\s*("(?:[^"\\]|\\.)*"),\s*$', lines[i])
        if not m:
            raise SystemExit("unparsable line for %s: %r" % (va, lines[i]))
        cur = json.loads(m.group(1))
        if cur != row["old"]:
            raise SystemExit("STALE fragment: %s currently maps to %r, fragment "
                             "expected %r" % (va, cur, row["old"]))
        drop.add(i)

    out_lines = [ln for i, ln in enumerate(lines) if i not in drop]
    out = "\n".join(out_lines)

    # The map must still be valid JSON and must have shrunk by exactly len(drop).
    before, after = json.loads(text), json.loads(out)
    if len(before) - len(after) != len(drop):
        raise SystemExit("key-count assertion failed: %d -> %d, expected -%d"
                         % (len(before), len(after), len(drop)))
    for row in frag:
        if row["va"].lower() in {k.lower() for k in after if k.startswith("0x")}:
            raise SystemExit("post-condition failed: %s still present" % row["va"])

    if not dry_run:
        Path(map_path).write_text(out)
    print("%s %d row(s); map keys %d -> %d"
          % ("would delete" if dry_run else "deleted", len(drop), len(before), len(after)))
    return len(drop)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("fragment")
    ap.add_argument("map")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    apply_deletes(a.fragment, a.map, a.dry_run)


if __name__ == "__main__":
    main()
