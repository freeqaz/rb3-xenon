#!/usr/bin/env python3
"""WRONGCALL-2: rank the rows that are blocked from 100 ONLY by relocation names.

The campaign's byte metric is ALL-OR-NOTHING PER ROW, so the question that pays is
not "how many charged sites exist" but "which ROWS would cross 100 if their charged
relocation names were repaired, and how big are they".

A row qualifies when, against its dtk-split target twin:
  * the MASKED bodies are byte-equal (every non-relocated word agrees), and
  * the relocation offset/type sequences agree,
so the ONLY residual difference is relocation TARGET NAMES -- exactly what the
shipped `name_check` ruler charges and what `none` forgives.

For each such row it reports the charged slots split into:
  FIXABLE_ORDER  both names are real and OUR OWN source chose the callee, so a
                 source or map repair can move it
  FOLD_SUSPECT   the two names are plausible /OPT:ICF twins -- NOT actionable
                 here; an unproven alias is pure forgiveness and lifts the score
                 BY CONSTRUCTION (see CAMPAIGN_STATE 2026-08-14 section 6)

>> A row with even ONE unresolved FOLD_SUSPECT slot CANNOT cross, so its
   byte value is 0 no matter how many order defects you repair in it.  That is
   the whole point of printing both columns: it stops a lane from pre-registering
   bytes that are structurally unreachable.

    python3 tools/maprow_audit/wc2_relocblocked.py --selfcheck
    python3 tools/maprow_audit/wc2_relocblocked.py --min-size 200
"""

import argparse
import collections
import glob
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from icf_alias_build import collect, placeholder  # noqa: E402

_ANON = re.compile(r"\?A0x[0-9a-f]{8}")


def report_rows():
    """size / fuzzy / mpn per function name, coerced -- report.json numerics are
    JSON STRINGS and protobuf-JSON omits defaults."""
    d = json.load(open(ROOT / "build/45410914/report.json"))
    out = {}
    for u in d["units"]:
        for f in u.get("functions", []):
            n = f.get("name")
            if not n:
                continue
            out[n] = (u["name"], int(f.get("size", 0) or 0),
                      float(f.get("fuzzy_match_percent", 0) or 0),
                      float(f.get("match_percent_normalized", 0) or 0))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min-size", type=int, default=0)
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--out", default="/home/free/tmp/wc2_relocblocked.json")
    args = ap.parse_args()

    tgt = collect(sorted(glob.glob(str(ROOT / "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(str(ROOT / "build/45410914/src/**/*.obj"), recursive=True)), "o")
    rep = report_rows()

    # >> MANDATORY: subtract the SHIPPED aliases.  objdiff consults
    # SymbolEquivalences and drops the charge, so counting an aliased pair as
    # "charged" INFLATES this census BY CONSTRUCTION -- the exact failure mode
    # CAMPAIGN_STATE_2026-08-14 section 4a lists among the instruments that
    # could not fail.  Symptom if you skip it: rows print fuzzy 100.0 while
    # still showing "charged" slots, which is arithmetically impossible.
    al = json.load(open(ROOT / "scripts/symbol_aliases.json"))
    eq = {}
    for g in al["groups"]:
        grp = set([g["survivor"]] + list(g["folded"]))
        for n in grp:
            eq.setdefault(n, set()).update(grp)

    rows = []
    for name, (mb, rel, sz) in ours.items():
        rt = tgt.get(name)
        if not rt or len(rt[1]) != len(rel):
            continue
        if rt[0] != mb:
            continue                      # body differs -> not reloc-only
        charged, forgiven = [], 0
        shape_ok = True
        for (ro, rn, rty), (oo, on, oty) in zip(rt[1], rel):
            if ro != oo or rty != oty:
                shape_ok = False
                break
            if rn == on:
                continue
            # Forgive exactly what the ruler forgives:
            #  * a placeholder on EITHER side (retail fn_/lbl_, our $LN local labels)
            #  * a shipped SymbolEquivalences alias
            #  * anonymous-namespace spellings differing ONLY in the ?A0x<hash>,
            #    which MSVC derives from machine name + source path and the
            #    scripts/ anon_ns obj patcher normalizes away
            if (placeholder(rn) or placeholder(on)
                    or on in eq.get(rn, ()) or rn in eq.get(on, ())
                    or _ANON.sub("?A", rn) == _ANON.sub("?A", on)):
                forgiven += 1
                continue
            charged.append((rn, on))
        if not shape_ok or not charged:
            continue
        meta = rep.get(name)
        if not meta:
            continue
        unit, size, fuzzy, mpn = meta
        if size < args.min_size:
            continue
        rows.append({"name": name, "unit": unit, "size": size, "fuzzy": fuzzy, "mpn": mpn,
                     "charged": len(charged), "forgiven": forgiven,
                     "distinct": sorted({c for c in charged})})
    rows.sort(key=lambda r: -r["size"])

    if args.selfcheck:
        # The screen must be able to EXCLUDE rows, or it is just "every row".
        tot_both = sum(1 for n in ours if n in tgt)
        print("SELFCHECK -- reloc-name-BLOCKED screen")
        print("   symbols present on both sides : %d" % tot_both)
        print("   rows selected (body EQUAL, >=1 charged reloc): %d" % len(rows))
        print("   => screen EXCLUDES %d rows (%.1f%%)" %
              (tot_both - len(rows), 100.0 * (tot_both - len(rows)) / max(1, tot_both)))
        print("   CAN FIRE: %s   CAN EXCLUDE: %s" % (len(rows) > 0, tot_both > len(rows)))
        return 0 if (rows and tot_both > len(rows)) else 1

    # CONSISTENCY GATE (can fire): a row with a genuinely CHARGED relocation name
    # cannot simultaneously be at fuzzy == 100 on the name_check ruler.  Any
    # survivor here means the charge model still over-counts (missing alias
    # subtraction, placeholder rule drift, or a reloc the ruler does not read).
    bad = [r for r in rows if r["fuzzy"] >= 100.0]
    print("CONSISTENCY: %d selected rows report fuzzy>=100 despite a charged reloc "
          "(expect 0; nonzero => the charge model over-counts)" % len(bad))
    for r in bad[:8]:
        print("   !! %-60s %s" % (r["name"][:60], r["distinct"][:1]))

    print("ROWS BLOCKED FROM 100 BY RELOCATION NAMES ONLY: %d rows, %d bytes at stake"
          % (len(rows), sum(r["size"] for r in rows)))
    print("%-7s %-9s %-4s %s" % ("size", "fuzzy", "chg", "symbol"))
    for r in rows[:40]:
        print("%-7d %-9.4f %-4d %s" % (r["size"], r["fuzzy"], r["charged"], r["name"][:78]))
        for rn, on in r["distinct"][:6]:
            print("            %-44s <- %s" % (rn[:44], on[:44]))
    json.dump(rows, open(args.out, "w"), indent=1)
    print("wrote", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
