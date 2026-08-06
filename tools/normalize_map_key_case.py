#!/usr/bin/env python3
"""Normalize `target_symbol_map.json` address keys to lowercase — SEMANTICS-FREE.

    python3 tools/normalize_map_key_case.py --target <map.json> [--dry-run]
    python3 tools/normalize_map_key_case.py --selftest

WHY
---
`scripts/obj_target_symbol_renamer.py` reads a key as

    int(k.lower().removeprefix("0x"), 16)

so key CASE carries no meaning to the only consumer that matters — but JSON key
identity is case-SENSITIVE, and so was `gated_map_write.py`'s P4 absence check.
`gated_map_write.py` P6 forces every NEW key lowercase. Put those together and
the 7 uppercase keys the live map shipped (`0x82357DD0`, `0x82364CA0`,
`0x82627FB8`, `0x826287F0`, `0x826288D8`, `0x826433A8`, `0x826550F0`) were
INVISIBLE to P4: the same address could be added a second time in the other
case, giving one address two live rows, no duplicate-key gate firing, and the
renamer honouring whichever row `json.load` happened to keep.

P4 now folds case (`gated_map_write.py`), which closes the hole on its own. This
tool closes it on the OTHER side — it makes the file itself uniform, so the
invariant "every key is lowercase" holds for every consumer, including ones that
do a plain `addr in map` membership test and will never consult a gate.

HOW — the same discipline as `gated_map_write.py`: NEVER `json.dump`
------------------------------------------------------------------
The map's 1-space indent and one-row-per-line layout are load-bearing. This tool
only ever rewrites the KEY TOKEN inside an existing line, then proves:

  N1 the result parses, and the root is an object
  N2 the case-folded {key: value} mapping is IDENTICAL before and after
     (this is the semantic no-op proof — the renamer sees the same map)
  N3 no duplicate root key is introduced, literally or up to case
  N4 the root row count and the total line count are unchanged
  N5 exactly the intended lines differ, and putting the old key tokens back
     reproduces the original bytes EXACTLY

Refuses (exit 4, file untouched) if any address is already present in BOTH
cases: that is a real collision needing a human decision on which row wins, not
a formatting problem. Atomic tmp+replace write, like the gated writer.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile
from collections import Counter
from pathlib import Path
from typing import List, Optional, Tuple

EXIT_OK = 0
EXIT_ERROR = 1
EXIT_REFUSED = 4

KEY_RX = re.compile(r"^0[xX][0-9a-fA-F]{8}$")


class Refused(Exception):
    def __init__(self, code: int, msg: str):
        super().__init__(msg)
        self.code = code


def root_pairs(text: str) -> List[Tuple[str, object]]:
    seen: List[List[Tuple[str, object]]] = []

    def hook(kv):
        seen.append(list(kv))
        return dict(kv)

    json.loads(text, object_pairs_hook=hook)
    if not seen:
        raise Refused(EXIT_REFUSED, "N1 document root is not an object")
    return seen[-1]


def normalize(text: str) -> Tuple[str, List[str]]:
    """Return (new_text, normalized_keys). Raises Refused on any anomaly."""
    pairs = root_pairs(text)
    keys = [k for k, _ in pairs]

    dup = [k for k, c in Counter(keys).items() if c > 1]
    if dup:
        raise Refused(EXIT_REFUSED,
                      f"N3 target already has duplicate root key(s) {dup[:3]} — "
                      f"refusing to build on a compromised file")
    dup_ci = [k for k, c in Counter(k.lower() for k in keys).items() if c > 1]
    if dup_ci:
        raise Refused(EXIT_REFUSED,
                      f"N3 address(es) {dup_ci[:3]} appear in BOTH cases — a real "
                      f"collision, not a formatting problem; a human must decide "
                      f"which row wins before this file can be normalized")

    targets = [k for k in keys if KEY_RX.match(k) and k != k.lower()]
    if not targets:
        return text, []

    new = text
    swaps: List[Tuple[str, str]] = []
    for k in targets:
        old_tok = f'"{k}":'
        if new.count(old_tok) != 1:
            raise Refused(EXIT_REFUSED,
                          f"N5 key token {old_tok!r} occurs {new.count(old_tok)} "
                          f"times in the raw text; refusing a non-unique rewrite")
        new_tok = f'"{k.lower()}":'
        if new_tok in new:
            raise Refused(EXIT_REFUSED,
                          f"N3 {new_tok!r} is already present — normalizing {k} "
                          f"would create a duplicate key")
        new = new.replace(old_tok, new_tok, 1)
        swaps.append((new_tok, old_tok))

    # ---- gates -----------------------------------------------------------
    after = root_pairs(new)                                        # N1
    if len(after) != len(pairs):                                   # N4
        raise Refused(EXIT_REFUSED,
                      f"N4 root rows {len(pairs)} -> {len(after)}")
    if len(new.splitlines()) != len(text.splitlines()):            # N4
        raise Refused(EXIT_REFUSED, "N4 line count changed")
    ak = [k for k, _ in after]
    if [k for k, c in Counter(ak).items() if c > 1]:               # N3
        raise Refused(EXIT_REFUSED, "N3 duplicate root key introduced")
    if [k for k, c in Counter(k.lower() for k in ak).items() if c > 1]:
        raise Refused(EXIT_REFUSED, "N3 case-folded duplicate root key introduced")
    if ({k.lower(): v for k, v in pairs}                           # N2
            != {k.lower(): v for k, v in after}):
        raise Refused(EXIT_REFUSED,
                      "N2 the case-folded mapping CHANGED — this is not a no-op")
    if [k.lower() for k in keys] != [k.lower() for k in ak]:       # N2
        raise Refused(EXIT_REFUSED, "N2 key ORDER changed")
    back = new                                                     # N5
    for new_tok, old_tok in swaps:
        back = back.replace(new_tok, old_tok, 1)
    if back != text:
        raise Refused(EXIT_REFUSED,
                      "N5 bytes outside the rewritten key tokens changed")
    ndiff = sum(1 for a, b in zip(text.splitlines(), new.splitlines()) if a != b)
    if ndiff != len(targets):
        raise Refused(EXIT_REFUSED,
                      f"N5 {ndiff} lines differ, expected exactly {len(targets)}")
    return new, targets


def _selftest() -> int:
    ok_all = True

    def check(name, cond, detail=""):
        nonlocal ok_all
        ok_all &= bool(cond)
        print(f"  [{'PASS' if cond else 'FAIL'}] {name}" + (f"\n         {detail}" if detail else ""))

    src = '{\n "0x82000AAA": "?A@@QAAXXZ",\n "0x82000bbb": "?B@@QAAXXZ",\n "_c": 1\n}\n'
    new, tgt = normalize(src)
    check("uppercase key normalized, one line changed, rest byte-identical",
          tgt == ["0x82000AAA"] and new ==
          '{\n "0x82000aaa": "?A@@QAAXXZ",\n "0x82000bbb": "?B@@QAAXXZ",\n "_c": 1\n}\n',
          repr(new))
    check("already-normal file is returned unchanged", normalize(new) == (new, []))

    coll = '{\n "0x82000AAA": "?A@@QAAXXZ",\n "0x82000aaa": "?B@@QAAXXZ"\n}\n'
    try:
        normalize(coll)
        check("both-cases collision REFUSES", False, "accepted")
    except Refused as r:
        check("both-cases collision REFUSES", r.code == EXIT_REFUSED, str(r)[:110])

    # VACUITY CONTROL: the gates must actually be able to fire.
    real = normalize.__globals__
    saved = real["root_pairs"]
    try:
        real["root_pairs"] = lambda t: [(k, v) for k, v in json.loads(t).items()] \
            if '"0x82000aaa"' not in t else [("0x82000aaa", "?SABOTAGED@@QAAXXZ")]
        try:
            normalize(src)
            check("VACUITY: a sabotaged post-state is REFUSED", False,
                  "gates accepted a changed mapping — N2/N4 are VACUOUS")
        except Refused as r:
            check("VACUITY: a sabotaged post-state is REFUSED", True, str(r)[:110])
    finally:
        real["root_pairs"] = saved

    print(f"  {'ALL PASS' if ok_all else 'FAILURES'}")
    return EXIT_OK if ok_all else EXIT_ERROR


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        description="Lowercase target_symbol_map.json address keys. "
                    "Semantics-preserving by construction and by gate; never json.dump.",
        epilog="Exit codes: 0 ok, 4 refused (file untouched), 1 error.")
    ap.add_argument("--target", type=Path,
                    help="map file to normalize (REQUIRED; no default, by design)")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args(argv)

    if a.selftest:
        return _selftest()
    if not a.target:
        ap.error("--target is required (there is no default: the live map is "
                 "too easy to clobber)")

    text = a.target.read_text()
    try:
        new, targets = normalize(text)
    except Refused as r:
        print(f"REFUSED: {r}", file=sys.stderr)
        return r.code
    if not targets:
        print("no uppercase address keys; nothing to do")
        return EXIT_OK
    print(f"normalizing {len(targets)} key(s): {targets}")
    print(f"N1-N5 pass; bytes outside those {len(targets)} key tokens are IDENTICAL")
    if a.dry_run:
        print("(dry-run, nothing written)")
        return EXIT_OK
    fd, tmp = tempfile.mkstemp(dir=str(a.target.parent), prefix=".nmkc.")
    with os.fdopen(fd, "w") as f:
        f.write(new)
    os.replace(tmp, a.target)
    print(f"wrote {a.target}")
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
