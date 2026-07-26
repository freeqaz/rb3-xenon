#!/usr/bin/env python3
"""Lane AE round 2 — the PER-UNIT pairing funnel.

`unemitted_symbol_scan.py` answers "does ANY obj in our build define this
target's mangled name?".  That is necessary but **not sufficient**: objdiff
pairs target <-> base **within a unit**, so the symbol has to be defined by
the obj whose pinned `.text` span physically contains the retail body.

Retail is not LTCG, but it *is* ICF + `/Ob2`, and it scattered inline/template
COMDATs across TUs.  So a symbol we emit correctly from its natural owner TU
still reads 0% when retail parked that COMDAT in a different TU's span.

This scanner folds the per-unit filter into the funnel and splits the named-0%
pool three ways:

  NOWHERE    the name is defined by no obj at all
             -> needs a SOURCE definition (and maybe a scatter-wire too)
  ELSEWHERE  the name is defined, but not by the landing unit's obj
             -> SCATTER-WIRE: add an ODR-use / explicit instantiation at the
                bottom of the landing unit's .cpp (inline+template case), or
                `#include "<owner>.cpp"` (out-of-line case)
  SAME-UNIT  the name is defined by the landing unit's obj
             -> already pairable; the 0% is body divergence, a different lane

ELSEWHERE is the actionable output: it is the pool where the *only* thing wrong
is which TU emits the COMDAT.  The inline-COMDAT force-emit shape measured
~100% reliable when it applies.

Usage:
    python3 scripts/harvest/scatter_pairing_scan.py                # funnel + ELSEWHERE table
    python3 scripts/harvest/scatter_pairing_scan.py --json OUT     # all three lists
    python3 scripts/harvest/scatter_pairing_scan.py --classname    # OBJ_CLASSNAME subset only
"""

import argparse
import collections
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from unemitted_symbol_scan import coff_defined_symbols  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent.parent
BUILD = ROOT / "build" / "45410914"

# Map/tooling artifacts and other lanes' pools -- never source opportunities here.
ARTIFACT_RE = re.compile(r"^(lbl_|merged_|__MERGED_|\$|__unwind\$)")
SCOPE_SKIP_RE = re.compile(r"auto_03_")

# OBJ_CLASSNAME(classname) defines both of these *inline in the class body*, so
# MSVC only emits the COMDAT in a TU that odr-uses it.  Proven force-emit shape.
CLASSNAME_RE = re.compile(
    r"^\?(?:StaticClassName@(\w+)@@SA|ClassName@(\w+)@@UBA)\?AVSymbol@@XZ$"
)


def emitted_index():
    """sym -> {obj stems that define it}, and stem -> {syms it defines}."""
    by_sym = collections.defaultdict(set)
    by_stem = collections.defaultdict(set)
    for obj in (BUILD / "src").rglob("*.obj"):
        for sym in coff_defined_symbols(obj):
            by_sym[sym].add(obj.stem)
            by_stem[obj.stem].add(sym)
    return by_sym, by_stem


def scan():
    by_sym, by_stem = emitted_index()
    report = json.loads((BUILD / "report.json").read_text())

    counts = collections.Counter()
    nowhere, elsewhere, sameunit = [], [], []

    for unit in report["units"]:
        uname = unit["name"]
        stem = uname.split("/")[-1]
        for fn in unit.get("functions", []):
            counts["total"] += 1
            if fn.get("match_percent_normalized") != 0.0:
                continue
            counts["zero"] += 1
            if SCOPE_SKIP_RE.search(uname):
                continue
            counts["in_scope"] += 1
            name = fn["name"]
            if name.startswith("fn_"):
                counts["anon"] += 1
                continue
            if ARTIFACT_RE.match(name):
                counts["artifact"] += 1
                continue
            counts["named"] += 1
            size = int(fn.get("size", 0) or 0)
            owners = by_sym.get(name)
            row = [uname, name, size]
            if not owners:
                nowhere.append(row)
            elif stem in owners:
                sameunit.append(row + [sorted(owners)])
            else:
                elsewhere.append(row + [sorted(owners)])

    return counts, nowhere, elsewhere, sameunit, len(by_stem), len(by_sym)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", help="write all three lists to this path")
    ap.add_argument("--classname", action="store_true",
                    help="restrict the printed table to OBJ_CLASSNAME COMDATs")
    args = ap.parse_args()

    counts, nowhere, elsewhere, sameunit, nobjs, nsyms = scan()

    print("FUNNEL (per-unit pairing filter applied)")
    print(f"  objs scanned / distinct emitted symbols  {nobjs} / {nsyms}")
    print(f"  target fns in report.json .............. {counts['total']}")
    print(f"  ... at 0% .............................. {counts['zero']}")
    print(f"  ... in scope (non auto_03_) ............ {counts['in_scope']}")
    print(f"  ... unmapped fn_* (other lane's pool) .. {counts['anon']}")
    print(f"  ... map/tooling artifacts .............. {counts['artifact']}")
    print(f"  ... NAMED and 0% ....................... {counts['named']}")
    for label, rows in (("NOWHERE   (no obj emits the name) ",  nowhere),
                        ("ELSEWHERE (wrong unit) = SCATTER  ",  elsewhere),
                        ("SAME-UNIT (body work, not ours)   ",  sameunit)):
        print(f"      {label} {len(rows):5d}  bytes={sum(r[2] for r in rows)}")

    rows = elsewhere
    if args.classname:
        rows = [r for r in elsewhere if CLASSNAME_RE.match(r[1])]
        print(f"\n(--classname: {len(rows)} of {len(elsewhere)} ELSEWHERE rows)")
    print("\n=== ELSEWHERE / SCATTER-WIRE candidates ===")
    print("  fix = ODR-use or explicit instantiation at the bottom of the")
    print("  LANDING unit's .cpp (inline/template), or #include \"<owner>.cpp\".")
    for uname, name, size, owners in sorted(rows, key=lambda r: -r[2]):
        who = ",".join(owners)
        print(f"{size:6d}  land={uname:36s} emitted_by={who[:52]:52s} {name}")

    if args.json:
        Path(args.json).write_text(json.dumps(
            {"NOWHERE": nowhere, "ELSEWHERE": elsewhere, "SAMEUNIT": sameunit},
            indent=1))
        print(f"\nwrote {args.json}")


if __name__ == "__main__":
    main()
