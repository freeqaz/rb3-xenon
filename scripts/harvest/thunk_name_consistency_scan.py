#!/usr/bin/env python3
"""Check every adjustor-thunk map entry against the name of the body it jumps to.
Lane BP-4.

WHY THIS IS THE STRONGEST MAP CHECK AVAILABLE
    An MSVC `this`-adjustor thunk is three instructions and no logic:

        lwz  r11, -4(r3)      ; load the vbtable/vtordisp displacement
        subf r3, r11, r3      ; this -= disp
        b    BODY             ; tail-jump to the real override

    (or the simpler non-virtual form `addi r3, r3, -N ; b BODY`.)  Its mangled
    name is by construction `?<Method>@<Class>@@$4...<sig>` for exactly the
    `?<Method>@<Class>@@` that BODY implements.  So the thunk's name is FULLY
    DETERMINED by its jump target's name.

    That makes this check SELF-CONSISTENT rather than oracle-dependent: even if
    BODY's own name is itself wrong, the thunk must still agree with it.  A
    disagreement is therefore an unambiguous internal contradiction in the map
    -- no external oracle, no source, no band.exe interpretation required
    beyond reading one branch.  Contrast the class-identity questions elsewhere
    in this worklist, which bottom out in size-degenerate template families that
    the available evidence cannot separate.

WHAT IT FOUND (the motivating case, ScoreDisplay):
    0x8231FB40  ?PostLoad@ScoreDisplay@@$4...  -> 0x8231F748 ?PostLoad@...  OK
    0x8231FB60  ?Save@ScoreDisplay@@$4...      -> 0x8231F6F8 ?Copy@...      MISMATCH
    0x82320390  ?Load@ScoreDisplay@@$4...      -> 0x8231FEE0 ?Save@...      MISMATCH
    i.e. the thunk names in a class's thunk run are displaced by one slot, each
    carrying its neighbour's name.  Those two read a clean FALSE 100.0% because
    a 3-instruction thunk is byte-identical to every other thunk with the same
    displacement, and objdiff runs functionRelocDiffs=None so the jump target
    is invisible to the score.

USAGE
    python3 scripts/harvest/thunk_name_consistency_scan.py --out ~/tmp/bp4_thunks.json
"""

import argparse
import json
import re
import struct
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))

from icf_contradiction_adjudicate import PE, load_symbols, BANDEXE  # noqa: E402
from saveload_direction_scan import load_map, branch_targets        # noqa: E402

REPORT = ROOT / "build" / "45410914" / "report.json"

# `?Method@Class@@$4PPPPPPPM@A@<sig>` / `$R`/`$B` variants: split at the $-tag.
THUNK_RE = re.compile(r"^(?P<head>\?[^@]*@[^$]*@@)\$(?P<tag>[0-9A-Z])")


def head_of(name):
    """`?Save@ScoreDisplay@@$4PPPPPPPM@A@AAX...` -> `?Save@ScoreDisplay@@`;
    `?Save@ScoreDisplay@@UAAXAAVBinStream@@@Z` -> `?Save@ScoreDisplay@@`."""
    m = re.match(r"^(\?[^@]*@(?:[^@]*@)*?@)", name)
    if not m:
        return None
    return m.group(1)


def qual(name):
    """method+class qualifier, tolerant of both thunk and body manglings."""
    if not name or not name.startswith("?"):
        return None
    m = THUNK_RE.match(name)
    if m:
        return m.group("head")
    # body: strip the trailing signature after the class list terminator '@@'
    i = name.find("@@")
    return name[:i + 2] if i > 0 else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    pe = PE(BANDEXE)
    syms = load_symbols()
    va2n, _ = load_map()
    rep = {}
    r = json.loads(REPORT.read_text())
    for u in r["units"]:
        for f in u.get("functions") or []:
            rep.setdefault(f["name"], (f.get("match_percent_normalized"), u["name"]))

    rows = []
    for va in sorted(va2n):
        for name in va2n[va]:
            if not THUNK_RE.match(name):
                continue
            size = syms.get(va, (None, 0, None))[1]
            body, _ = pe.read(va, size) if size else (None, None)
            if not body:
                rows.append(dict(va="%#010x" % va, name=name, verdict="NO_BODY"))
                continue
            brs = branch_targets(body, va)
            # the thunk's tail jump is its LAST branch and is not a call
            tail = [t for _, t, is_call in brs if not is_call]
            if len(brs) != 1 or not tail:
                rows.append(dict(va="%#010x" % va, name=name, retail_size=size,
                                 verdict="NOT_SIMPLE_THUNK", nbranch=len(brs)))
                continue
            tgt = tail[-1]
            tnames = va2n.get(tgt)
            tn = tnames[0] if tnames else syms.get(tgt, (None,))[0]
            tq, sq = qual(tn), qual(name)
            if tn is None:
                verdict = "TARGET_UNKNOWN"
            elif tn.startswith("fn_"):
                verdict = "TARGET_UNNAMED"
            elif tq and sq and tq == sq:
                verdict = "OK"
            else:
                verdict = "MISMATCH"
            rows.append(dict(va="%#010x" % va, name=name, retail_size=size,
                             target="%#010x" % tgt, target_name=tn,
                             thunk_qual=sq, target_qual=tq, verdict=verdict,
                             match_pct=rep.get(name, (None, None))[0],
                             unit=rep.get(name, (None, None))[1],
                             suggest=(sq and tq and (name.replace(sq, tq, 1)
                                                     if verdict == "MISMATCH" else None))))

    Path(a.out).expanduser().write_text(json.dumps(rows, indent=1))
    c = Counter(x["verdict"] for x in rows)
    print("scanned %d adjustor-thunk map entries" % len(rows), file=sys.stderr)
    for k, v in c.most_common():
        print("  %-18s %4d" % (k, v), file=sys.stderr)
    bad = [x for x in rows if x["verdict"] == "MISMATCH"]
    at100 = [x for x in bad if x.get("match_pct") == 100.0]
    print("\nMISMATCH at a FALSE 100%%: %d of %d" % (len(at100), len(bad)),
          file=sys.stderr)
    for x in bad[:40]:
        print("  %s pct=%-6s %s\n        jumps to %s = %s"
              % (x["va"], x.get("match_pct"), x["name"][:70],
                 x["target"], (x["target_name"] or "?")[:70]), file=sys.stderr)
    print("\nwrote %s" % a.out, file=sys.stderr)


if __name__ == "__main__":
    main()
