#!/usr/bin/env python3
"""Verify every `?Save@`/`?Load@` map entry against the RETAIL body's stream
direction.  Lane BP-4.

THE PROOF THIS AUTOMATES
    A Milo `Save(BinStream&) const` and the matching `Load(BinStream&)` compile
    to the SAME instruction skeleton: set up `r3 = stream`, `r4 = &member`, and
    `bl` a stream operator, once per serialised member.  The ONLY difference is
    WHICH operator is called -- and objdiff runs with functionRelocDiffs=None,
    so the call TARGET is invisible to the score.  A map entry that names a
    retail Load body `?Save@...` therefore reads a clean, and completely false,
    100.0%.  (This is the at-100% defect class from
    memory:project_correctness_vs_metric.)

    The discriminator is the MSVC mangling of the callee:
        ??6  ==  operator<<   ==>  WRITE side  ==>  the body is a Save
        ??5  ==  operator>>   ==>  READ  side  ==>  the body is a Load
    So: disassemble retail at the mapped VA, resolve each `bl` target through
    target_symbol_map.json, read the prefix.  The resolved callees adjudicate
    the body's direction independently of what the map calls it.

    Anchor case that motivated this (verified by hand first): 0x82690B28 is
    mapped `?Save@SetUserDifficultyMsg@@UBAXAAVBinStream@@@Z`, but its two `bl`
    targets resolve to `??5@YAAAVBinStream@@AAV0@AAVHxGuid@@@Z` and
    `??5BinStream@@QAAAAV0@AAVString@@@Z` -- both `??5`.  It is a Load.

SECONDARY EVIDENCE also collected (a Save may serialise via helpers rather than
operators): a `bl` to another `?Save@`/`?Load@` (base-class chaining) is
direction-carrying too, as is `?Write@`/`?Read@` on BinStream.

WHAT A CONTRADICTION MEANS
    The retail body at VA is the OPPOSITE direction from its map name.  The
    method half of the name is provably wrong.  NOTE that this does NOT by
    itself prove the CLASS half is right -- the observed member offsets/types
    must be matched against a candidate class to settle that.  So a
    CONTRADICT row is a confirmed defect but not automatically a repoint: emit
    it for hand adjudication.

USAGE
    python3 scripts/harvest/saveload_direction_scan.py --out ~/tmp/bp4_saveload.json
"""

import argparse
import json
import struct
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))

from icf_contradiction_adjudicate import PE, load_symbols, BANDEXE  # noqa: E402

TSM = ROOT / "scripts" / "target_symbol_map.json"
REPORT = ROOT / "build" / "45410914" / "report.json"


def load_map():
    m = json.loads(TSM.read_text())
    va2names, name2va = {}, {}
    for va, v in m.items():
        if not va.startswith("0x"):
            continue                      # metadata keys, e.g. _splits_fill_*
        names = v if isinstance(v, list) else [v]
        va2names[int(va, 16)] = names
        for n in names:
            name2va.setdefault(n, int(va, 16))
    return va2names, name2va


def branch_targets(body, va):
    """-> [(insn_va, target_va, is_call)] for op-18 branches."""
    out = []
    for i in range(0, len(body) - 3, 4):
        w = struct.unpack_from(">I", body, i)[0]
        if (w >> 26) & 0x3F != 18:
            continue
        li = w & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        tgt = (li if (w & 2) else va + i + li) & 0xFFFFFFFF
        out.append((va + i, tgt, bool(w & 1)))
    return out


def direction_of(name):
    """WRITE / READ / None for a resolved callee name."""
    if name.startswith("??6"):
        return "WRITE"
    if name.startswith("??5"):
        return "READ"
    if name.startswith("?Save@"):
        return "WRITE"
    if name.startswith("?Load@"):
        return "READ"
    # BinStream member helpers
    if name.startswith("?Write@BinStream@@") or name.startswith("?WriteEndian@"):
        return "WRITE"
    if name.startswith("?Read@BinStream@@") or name.startswith("?ReadEndian@"):
        return "READ"
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    pe = PE(BANDEXE)
    syms = load_symbols()
    va2names, name2va = load_map()
    rep = {}
    r = json.loads(REPORT.read_text())
    for u in r["units"]:
        for f in u.get("functions") or []:
            rep.setdefault(f["name"], (f.get("match_percent_normalized"), u["name"]))

    rows = []
    for va, names in sorted(va2names.items()):
        for name in names:
            if not (name.startswith("?Save@") or name.startswith("?Load@")):
                continue
            declared = "WRITE" if name.startswith("?Save@") else "READ"
            size = syms.get(va, (None, 0, None))[1]
            body, sec = pe.read(va, size) if size else (None, None)
            callees, dirs = [], Counter()
            if body:
                for _, tgt, is_call in branch_targets(body, va):
                    tnames = va2names.get(tgt)
                    tn = tnames[0] if tnames else syms.get(tgt, (None,))[0]
                    callees.append("%#010x=%s" % (tgt, tn))
                    d = direction_of(tn) if tn else None
                    if d:
                        dirs[d] += 1
            if not size:
                verdict = "NO_SIZE"
            elif not dirs:
                verdict = "NO_EVIDENCE"
            elif len(dirs) > 1:
                verdict = "MIXED"
            else:
                observed = next(iter(dirs))
                verdict = "AGREE" if observed == declared else "CONTRADICT"
            rows.append(dict(va="%#010x" % va, name=name, declared=declared,
                             retail_size=size, observed=dict(dirs),
                             verdict=verdict, callees=callees,
                             match_pct=rep.get(name, (None, None))[0],
                             unit=rep.get(name, (None, None))[1],
                             in_report=name in rep))

    Path(a.out).expanduser().write_text(json.dumps(rows, indent=1))
    c = Counter(x["verdict"] for x in rows)
    print("scanned %d ?Save@/?Load@ map entries" % len(rows), file=sys.stderr)
    for k, v in c.most_common():
        print("  %-12s %4d" % (k, v), file=sys.stderr)
    bad = [x for x in rows if x["verdict"] == "CONTRADICT"]
    print("\n=== CONTRADICTIONS (retail body is the OPPOSITE direction) ===",
          file=sys.stderr)
    for x in bad:
        print("  %s  declared=%-5s observed=%s  pct=%s  %s"
              % (x["va"], x["declared"], x["observed"], x["match_pct"], x["name"]),
              file=sys.stderr)
        for c2 in x["callees"]:
            print("        -> %s" % c2, file=sys.stderr)
    print("\nwrote %s" % a.out, file=sys.stderr)


if __name__ == "__main__":
    main()
