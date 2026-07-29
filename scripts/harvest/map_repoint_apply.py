#!/usr/bin/env python3
"""In-place REPOINT applier for scripts/target_symbol_map.json (lane BP-4).

tu5_map_apply_fragment.py only ever INSERTS new "addr": "name" lines, so it
cannot express a repoint (changing the name an existing address maps to) or a
swap (exchanging two addresses' names).  This tool does exactly that, and
nothing else.

HARD PROJECT INVARIANT (inherited from tu5_map_apply_fragment.py): never
json.dump-rewrite the map.  It is a ~27k-entry, 1-space-indent file whose
formatting is a load-bearing convention -- a full dump would reflow every line
and bury the real change in noise.  This applier rewrites ONLY the single line
belonging to each named address and leaves every other line byte-identical.

FRAGMENT FORMAT -- a list of explicit, justified operations:
    [{"va": "0x82630340",
      "old": "??1NewAwardPanel@@UAA@XZ",
      "new": "??1RetryAudioPanel@@UAA@XZ",
      "op":  "repoint",
      "why": "<per-row justification, REQUIRED>"}, ...]

`old` is asserted against the file, so a fragment written against a stale map
fails loudly instead of silently clobbering someone else's landing.  Every row
must carry a non-empty `why` -- an unjustified map edit is not reviewable, and
this map is the project's single most mispair-prone artifact.

Collision safety: after applying, the tool re-parses the map and asserts that no
NAME is used at two addresses (the invariant tu5_map_apply_fragment.py enforces
on insert).  A swap passes because both names remain used exactly once.

USAGE
    python3 scripts/harvest/map_repoint_apply.py <fragment.json> \
            scripts/target_symbol_map.json [--dry-run]
"""

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path


def apply_repoints(frag_path, map_path, dry_run=False):
    frag = json.loads(Path(frag_path).read_text())
    if not isinstance(frag, list):
        raise SystemExit("fragment must be a LIST of op dicts")
    text = Path(map_path).read_text()
    lines = text.split("\n")

    # index: lowercase addr -> line number (only plain "addr": "name" lines)
    idx = {}
    for i, ln in enumerate(lines):
        m = re.match(r'^\s*"(0x[0-9a-fA-F]+)":\s*(".*?"),\s*$', ln)
        if m:
            idx.setdefault(m.group(1).lower(), i)

    changed = []
    for row in frag:
        for k in ("va", "old", "new", "op", "why"):
            if not row.get(k):
                raise SystemExit("fragment row missing required %r: %r" % (k, row))
        va = row["va"].lower()
        if va not in idx:
            raise SystemExit("address not present as a simple string entry: %s" % va)
        i = idx[va]
        m = re.match(r'^(\s*"0x[0-9a-fA-F]+":\s*)("(?:[^"\\]|\\.)*")(,\s*)$', lines[i])
        if not m:
            raise SystemExit("unparsable line for %s: %r" % (va, lines[i]))
        cur = json.loads(m.group(2))
        if cur != row["old"]:
            raise SystemExit("STALE fragment: %s currently maps to %r, fragment "
                             "expected %r" % (va, cur, row["old"]))
        lines[i] = m.group(1) + json.dumps(row["new"]) + m.group(3)
        changed.append((va, cur, row["new"]))

    out = "\n".join(lines)

    # post-condition: introduce no NEW duplicate name.  Compared as a DELTA
    # because the map already carries 3 pre-existing duplicates
    # (?StaticClassName@Object@Hmx@@, ?StaticClassName@RndCam@@, ?NodeCmp@@) that
    # predate this lane; asserting absolutely would block every repoint forever
    # and tempt the next author to delete the check outright.
    def dupes_of(txt):
        names = Counter()
        for k, v in json.loads(txt).items():
            if not k.startswith("0x"):
                continue
            for n in (v if isinstance(v, list) else [v]):
                names[n] += 1
        return {n for n, c in names.items() if c > 1}

    before, after = dupes_of(text), dupes_of(out)
    new_dupes = after - before
    if new_dupes:
        raise SystemExit("post-apply NAME COLLISION introduced: %s"
                         % sorted(new_dupes)[:5])
    if before:
        print("note: %d pre-existing duplicate name(s) left untouched" % len(before))

    if not dry_run:
        Path(map_path).write_text(out)
    for va, old, new in changed:
        print("  %s  %s\n            -> %s" % (va, old, new))
    print("%s %d repoint(s)" % ("would apply" if dry_run else "applied", len(changed)))
    return len(changed)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("fragment")
    ap.add_argument("map")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    apply_repoints(a.fragment, a.map, a.dry_run)


if __name__ == "__main__":
    main()
