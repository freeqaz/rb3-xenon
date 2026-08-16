#!/usr/bin/env python3
"""nearfree_tier_worklist.py — route the "near-free" penalty tiers, cheaply.

WHY
---
`docs/plans/identical-pct-cluster-scan-2026-07-26.md` established that objdiff's
`match_percent_normalized` is **not a fuzzy score**: it losslessly encodes the
pair (penalty S, instruction count N).  objdiff computes

    max_score = N * PENALTY_INSERT_DELETE          (= N * 100)
    pct       = (1 - S / max_score) * 100          [computed and stored as f32]

so with `N = size / 4` recovered from the function's byte size, the integer
penalty inverts exactly:

    S = round((100 - pct) * N)

Penalties (objdiff-core/src/diff/code.rs): INSERT_DELETE=100, REPLACE=60,
REG_DIFF=5, IMM_DIFF=1.

That turns triage into arithmetic done **before any build**:

    S = 1    -> exactly ONE differing immediate operand
    S = 60   -> exactly ONE replaced instruction   (one wrong constant/offset/opcode)
    S = 100  -> exactly ONE inserted/deleted instruction
    S = 120  -> two replaces
    S = 160  -> one insert + one replace
    S = 200  -> two insert/deletes

These are the *near-free tiers*: a two-call objdiff read (instruction, index, and
DIRECTION -- target-only vs base-only) settles them.  This tool enumerates them.

★ MEASURED CORRECTIONS (lane AG round 2, docs/plans/lane-ag-deep-body-ports §5.3):

  * **S = 5 is NOT "one differing register."**  It is one differing *opcode*
    (`diff_op`), or five differing immediates.  Of five S=5 targets, one was a
    single `ble`->`beq` from a signed `(int)size()` cast (closed with one word),
    one a branch-polarity `diff_op`, three were 4-5 member-offset immediates.
    **None was the at_limit shape** -- do not classify S=5 as at_limit.
  * **The S in {1,2,5} tier is a LAYOUT tier wearing an immediate-operand mask.**
    Of 14 S=1 targets: 0 map mispairs, 0 reloc artifacts, but only 3 were
    literally one wrong constant -- the other 11 were 9 class/member-layout
    deltas + 2 stack-frame slot deltas.  Route it to whoever owns class layout,
    not to constant-hunting, and reach for
    `cl.exe /d1reportSingleClassLayout<Class>` (works through wibo).
  * **report.json rounds 99.953 up to 100.0**, so a strict count taken as
    `match_percent_normalized == 100.0` can credit a non-byte-match.
  * **Branch-target-only diffs are not scored** under `functionRelocDiffs=none`,
    so this S (from report.json) is *smaller* than objdiff-cli's raw S, which
    counts reloc/branch penalties.  Price from report.json -- that is the metric
    -- but expect the raw diff to look worse than S predicts.

WHAT IT FILTERS, AND WHY
------------------------
Per `docs/plans/lane-ag-deep-body-ports-2026-07-26.md` §1:

  * **cluster size >= 5 is refused.**  Measured yield by cluster size is
    ANTI-predictive: >=20 members 0/45, 10-19 0/13, 5-9 0/44, 3-4 -> 3/77.  Big
    clusters are big because the cause is systemic and unfixable.  Clustering is
    computed on the `score_shape` axis (name-shape, S) -- which merges the same
    cause across differently-sized instantiations -- unioned with the raw `pct`
    axis.
  * the STL element-`sizeof` family (`_M_fill_insert`, `_M_insert_overflow_aux`,
    `__uninitialized_*`, `_Rb_tree`, `resize`, `push_back`) is dropped: known
    systemic stride wall (`project_struct_stride_vein`).
  * `N < 8` is dropped (too small to carry a legible shape).
  * anonymous `fn_*`, `__unwind$*` funclets, and `$`-containing thunks dropped.
  * `xdk` units and `auto_03_*` spans dropped (vendor + Quazal, hard-skipped).

It then joins against `decomp.db` so previously-attempted functions are ranked
last.  Note AT_LIMIT in that table is **advisory, not authoritative** -- this
lane flipped `EditSetlistPanel::Exiting` which was recorded AT_LIMIT.

★ GOTCHA that breaks ~90% of naive round-trips: `report.json` stores the f32's
shortest decimal repr, and Python parses it back to a **double**.  Both sides of
the comparison must be forced through `struct.pack("<f", ...)`.

USAGE
-----
    python3 scripts/harvest/nearfree_tier_worklist.py                  # default tiers
    python3 scripts/harvest/nearfree_tier_worklist.py --tiers 60,100
    python3 scripts/harvest/nearfree_tier_worklist.py --max-cluster 3
    python3 scripts/harvest/nearfree_tier_worklist.py --include-attempted
    python3 scripts/harvest/nearfree_tier_worklist.py --json out.json
"""
import argparse
import json
import os
import re
import sqlite3
import struct
import sys
from collections import Counter

DEFAULT_TIERS = (60, 100, 120, 160, 180, 200)

STL_FAMILY = re.compile(
    r"_M_fill_insert|_M_insert_overflow_aux|__uninitialized|_M_insert_aux"
    r"|_Rb_tree|@resize@|push_back|_M_clone"
)


def f32(x):
    """Round-trip a value through IEEE single, the way objdiff stored it."""
    return struct.unpack("<f", struct.pack("<f", x))[0]


def name_shape(sym):
    """Leading identifier of an MSVC mangled name -- the 'same cause' axis."""
    m = re.match(r"\?\??([A-Za-z0-9_]+)@", sym)
    return m.group(1) if m else sym[:12]


def invert(pct, n):
    """Recover the exact integer penalty S, or None if the round-trip fails."""
    pf = f32(pct)
    s = round((100.0 - pf) * n)
    if s <= 0:
        return None
    if abs(f32((1.0 - s / (100.0 * n)) * 100.0) - pf) > 1e-4:
        return None
    return s


def collect(report_path, min_n):
    rows = []
    rep = json.load(open(report_path))
    for unit in rep["units"]:
        un = unit["name"]
        if un.startswith("xdk") or "auto_03" in un:
            continue
        for fn in unit.get("functions", []):
            pct = fn.get("match_percent_normalized")
            name = fn.get("name", "")
            if pct is None or pct <= 0 or pct >= 100:
                continue
            if name.startswith("fn_") or name.startswith("__unwind") or "$" in name:
                continue
            n = int(fn.get("size") or 0) // 4
            if n < min_n:
                continue
            s = invert(pct, n)
            if s is None:
                continue
            rows.append(
                dict(unit=un, name=name, pct=pct, N=n, S=s,
                     stl=bool(STL_FAMILY.search(name)))
            )
    return rows


def add_clusters(rows):
    by_shape = Counter((name_shape(r["name"]), r["S"]) for r in rows)
    by_pct = Counter(r["pct"] for r in rows)
    for r in rows:
        r["csz"] = max(by_shape[(name_shape(r["name"]), r["S"])], by_pct[r["pct"]])


def join_attempts(rows, db_path):
    if not os.path.exists(db_path):
        for r in rows:
            r["attempts"], r["verdict"] = None, None
        return
    db = sqlite3.connect(db_path)
    seen = {}
    for r in rows:
        if r["name"] not in seen:
            got = db.execute(
                "SELECT attempt_count, verdict FROM functions WHERE symbol = ?",
                (r["name"],),
            ).fetchone()
            seen[r["name"]] = got or (0, None)
        r["attempts"], r["verdict"] = seen[r["name"]]
    db.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("report", nargs="?", default="build/45410914/report.json")
    ap.add_argument("--db", default="decomp.db")
    ap.add_argument("--tiers", default=",".join(map(str, DEFAULT_TIERS)),
                    help="comma-separated penalty totals to keep")
    ap.add_argument("--max-cluster", type=int, default=4,
                    help="refuse clusters this size or larger (default 4, i.e. keep <=3... "
                         "see lane-ag doc: >=5 yields 0)")
    ap.add_argument("--min-n", type=int, default=8)
    ap.add_argument("--include-stl", action="store_true")
    ap.add_argument("--include-attempted", action="store_true")
    ap.add_argument("--json")
    args = ap.parse_args()

    tiers = {int(t) for t in args.tiers.split(",") if t.strip()}
    rows = collect(args.report, args.min_n)
    add_clusters(rows)
    join_attempts(rows, args.db)

    print(f"inverted {len(rows)} named paired sub-100 functions "
          f"(N>={args.min_n}); f32 round-trip succeeded for all of them")
    band = Counter()
    for r in rows:
        band[r["S"]] += 1
    print("full S histogram (top 12):",
          dict(sorted(band.items(), key=lambda kv: -kv[1])[:12]))

    sel = [r for r in rows
           if r["S"] in tiers
           and r["csz"] < args.max_cluster
           and (args.include_stl or not r["stl"])]
    # IDENTITY_UNESTABLISHED is DROPPED outright, not annotated.
    #
    # This file's stance on AT_LIMIT -- "advisory, not authoritative", because
    # this lane flipped an AT_LIMIT row -- is deliberate and stays. It does not
    # extend here. AT_LIMIT is a judgement about a floor that a harder attempt
    # may overturn; IDENTITY_UNESTABLISHED says the target body is not
    # established to be this function, so attempting it harder is the hazard,
    # not the remedy. There is nothing for a lane to overturn by trying.
    #
    # Dropping (rather than ranking last) matters because the fresh/stale split
    # below keys on attempt_count, so a NEVER-ATTEMPTED row in this state would
    # sort into "route these first" -- the top of the routing list.
    n_unident = sum(1 for r in sel if r.get("verdict") == "IDENTITY_UNESTABLISHED")
    if n_unident:
        sel = [r for r in sel if r.get("verdict") != "IDENTITY_UNESTABLISHED"]
        print(f"dropped {n_unident} IDENTITY_UNESTABLISHED row(s) "
              f"(target body not established; see docs/decomp/VERDICT_STATES.md)")

    if not args.include_attempted:
        fresh = [r for r in sel if not r["attempts"]]
        stale = [r for r in sel if r["attempts"]]
    else:
        fresh, stale = sel, []

    print(f"\nnear-free tiers {sorted(tiers)}, cluster<{args.max_cluster}: "
          f"{len(sel)} candidates -- {len(fresh)} never attempted, "
          f"{len(stale)} previously attempted")
    print("by S:", dict(sorted(Counter(r['S'] for r in fresh).items())))

    def dump(title, group):
        if not group:
            return
        print(f"\n=== {title} ===")
        for r in sorted(group, key=lambda r: (r["S"], -r["pct"])):
            print(f"{r['S']:>4} N={r['N']:<5} pct={r['pct']:<12} csz={r['csz']}  "
                  f"{r['unit']:<44} {r['name']}")

    dump("NEVER ATTEMPTED (route these first)", fresh)
    dump("PREVIOUSLY ATTEMPTED (verdict advisory only, not authoritative)", stale)

    if args.json:
        json.dump(dict(fresh=fresh, attempted=stale), open(args.json, "w"), indent=1)
        print(f"\nwrote {args.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
