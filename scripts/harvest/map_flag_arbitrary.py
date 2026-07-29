#!/usr/bin/env python3
"""Append VAs to the `_bijection_arbitrary` / `_icf_arbitrary` metadata lists in
scripts/target_symbol_map.json (lane BP-7).

WHY A SEPARATE TOOL.  map_repoint_apply.py rewrites only simple
`"0xADDR": "name",` lines and cannot touch the list-valued metadata keys.  Those
lists are the project's record of "this VA's NAME was an arbitrary pick inside a
reloc-masked byte-identical equivalence class" -- the doctrine in the map's own
`_bijection_arbitrary_comment`.  A VA that is demonstrably inside such a class
but NOT listed is a silent trap: a downstream tool will read its name as
asserted evidence.  Registering it costs nothing (the renamer skips every key
that does not start with "0x", so this is metric-inert by construction) and
stops the next lane re-deriving identity from an arbitrary label.

Like the repoint applier, this NEVER json.dump-rewrites the map: each list is a
single physical line, so we rewrite exactly that one line and leave every other
byte identical.  Idempotent -- VAs already present are skipped.

USAGE
    python3 scripts/harvest/map_flag_arbitrary.py \
        --key _bijection_arbitrary --va 0x82690a10 --va 0x82690b28 \
        scripts/target_symbol_map.json [--dry-run]
"""
import argparse, json, re
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("map")
    ap.add_argument("--key", required=True,
                    choices=["_bijection_arbitrary", "_icf_arbitrary", "_denylist"])
    ap.add_argument("--va", action="append", required=True)
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()

    p = Path(a.map)
    lines = p.read_text().split("\n")
    pat = re.compile(r'^(\s*"%s":\s*)(\[.*\])(,?\s*)$' % re.escape(a.key))
    hits = [i for i, ln in enumerate(lines) if pat.match(ln)]
    if len(hits) != 1:
        raise SystemExit("expected exactly 1 line for %s, found %d" % (a.key, len(hits)))
    i = hits[0]
    m = pat.match(lines[i])
    cur = json.loads(m.group(2))
    have = {x.lower() for x in cur}
    add = [v.lower() for v in a.va if v.lower() not in have]
    if not add:
        print("all %d VA(s) already present in %s -- no change" % (len(a.va), a.key))
        return
    new = cur + add
    lines[i] = m.group(1) + json.dumps(new) + m.group(3)
    out = "\n".join(lines)
    # sanity: the file must still parse and the key must have grown by exactly len(add)
    parsed = json.loads(out)
    assert len(parsed[a.key]) == len(cur) + len(add), "list length assertion failed"
    if not a.dry_run:
        p.write_text(out)
    print("%s %d VA(s) to %s: %s" % ("would add" if a.dry_run else "added",
                                     len(add), a.key, ", ".join(add)))


if __name__ == "__main__":
    main()
