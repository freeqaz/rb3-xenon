#!/usr/bin/env python3
"""Conservation instrument for pin waves: prove a pin moved MAP, not MATCHES.

Pinning a `.text` span in `config/45410914/splits.txt` reattributes bytes from
dtk's leftover `auto_*` units into a named unit. That is a MAP operation: it
must leave every MATCHING key exactly unchanged while moving bytes from the
auto side to the named side. This tool measures precisely that, and REFUSES
rather than printing a plausible-looking number when its preconditions fail.

    python3 tools/report_conservation.py snap  ~/tmp/before.json --label baseline
    ...apply pins, rebuild...
    python3 tools/report_conservation.py snap  ~/tmp/after.json  --label batch1
    python3 tools/report_conservation.py cmp   ~/tmp/before.json ~/tmp/after.json

Promoted from lane PIN-D's scratch `~/tmp/snap_report.py` + `~/tmp/cmp_snap.py`
by lane PIN-E (pin wave 2). The scratch versions hard-coded PIN-D's worktree
path, had no staleness guard, and compared per-unit aggregates rather than rows.

WHAT IT REPORTS
---------------
* the four MATCHING keys — `matched_functions`, `matched_code`,
  `masked_equal_functions`, and (as context) `matched_code_percent`. These MUST
  be Δ0 across a pin. Any movement is a STOP condition: a pin is damaging real
  matched code.
* `total_code` / `total_functions` — these are NOT pin-neutral (see below).
* the auto-vs-named byte/function split, and the conservation arithmetic
  `Δauto + Δnamed == Δtotal`, which must hold to the byte.
* an enumeration of EVERY changed function row, as a name+size multiset diff.

★ THE HEADLINE FINDING THIS TOOL EXISTS TO CATCH (wave 1, lane PIN-D)
---------------------------------------------------------------------
Pinning is neutral on the MATCHING keys but NOT on `total_code`. Wave 1 moved
`total_code` by −5,120 B / −1 function, because a pin EVICTED A PHANTOM
`type:label` ROW whose extent straddled four of the new pins and was
DOUBLE-COUNTING 33 real function rows summing 5,024 B. Evicting it makes the
denominator MORE TRUTHFUL. Per project doctrine (accuracy beats headline %)
that is a WIN, not a regression — but you can only tell the difference by
enumerating the changed rows, which is what `cmp` does.

⛔⛔ TWO VACUOUS INSTRUMENTS — DO NOT REBUILD EITHER OF THEM
------------------------------------------------------------
Both were tried in wave 1 and both produce confident, wrong answers.

(1) **NEVER derive an address from a `fn_<addr>` symbol name.** The pre-compile
    target-symbol renamer (`scripts/obj_target_symbol_renamer`) rewrites the
    dtk-split target obj's anonymous symbols to MSVC mangled names, so a row
    named `fn_82C1D608` may be reported as `g726adpcm` — and a row that KEEPS
    an `fn_`/`lbl_` name is not thereby at that address. Wave-1 evidence: the
    evicted row is named `lbl_82BE3D20` but symbols.txt puts it at
    **0x82C16DF0**, inside the xhv2 region — a different region entirely. THE
    NAME LIES. (report.json's own `address` field is no help either: every row
    ships `address: '0'`.)

(2) **NEVER key a unit diff on auto-unit NAMES.** dtk renumbers/renames its
    `auto_*` leftover units on every split, so a name-keyed unit diff reported
    "937 new units" for a wave whose true footprint was **31 units**. Unit
    aggregates are printed here for orientation ONLY; adjudication is done on
    the row multiset.

⇒ The instrument that WORKS is a **name+size multiset over all ~69k function
rows**, compared wholesale. That is `_rows` in the snapshot.

TYPE HAZARD (this is not theoretical)
-------------------------------------
Several `report.json` numerics are JSON **STRINGS**: `total_code`,
`matched_code`, `complete_code`, and every function row's `size`. Un-coerced,
`+` silently CONCATENATES and `>` compares LEXICOGRAPHICALLY — a lane lost an
entire census to this. Every numeric read here goes through `_int()`/`_float()`.

REFUSALS (each exits non-zero; none of them prints a number you could mistake
for a result)
--------------------------------------------------------------------------
* `snap`  — report.json missing                           -> exit 2
* `snap`  — report.json OLDER than a config input          -> exit 3  (stale)
* `snap`  — Σ(row sizes) != total_code, or row count !=
            total_functions                                -> exit 5  (internal
            inconsistency: the snapshot does not describe the report it read)
* `cmp`   — snapshot missing/malformed/wrong schema        -> exit 2
* `cmp`   — BOTH snapshots read the SAME PHYSICAL report
            (identical mtime+size+digest)                  -> exit 6
* `cmp`   — a MATCHING key moved                           -> exit 4
* `cmp`   — `Δauto + Δnamed != Δtotal`                     -> exit 5

Exit 6 is the one that matters most in practice. The wave-1 lane hit exactly
this failure: a `&&` chain swallowed a failed `configure.py`, the build never
reran, and the "baseline" and "after" legs were THE SAME STALE FILE READ TWICE.
An mtime-vs-config check CANNOT catch that here, because
`scripts/setup_worktree.sh` normalizes config mtimes to 2020-01-01, so a
reflinked stale report always looks "newer" than its inputs. Comparing report
provenance between the two snapshots is what catches it.

WORKED EXAMPLE — WAVE 1 (lane PIN-D, commit 7b59e120). A correct run looks like
this; if your numbers are shaped differently, suspect the run, not the finding.

    matched_functions       44,248 ->  44,248   delta 0
    matched_code         4,340,756 -> 4,340,756 delta 0
    masked_equal_functions  22,864 ->  22,864   delta 0
    total_code          10,646,496 -> 10,641,376 delta -5,120
    total_functions         69,229 ->  69,228   delta -1
    auto_code   -377,848  +  named_code   +372,728  = -5,120   (== delta total_code)
    auto_fns        -802  +  named_fns        +801  =     -1   (== delta total_functions)
    CHANGED ROWS: exactly 2
      lbl_82BE3D20                     5,116 B  REMOVED   (phantom label eviction)
      ComputeDotProductPrecision  96 -> 92 B    SIZE      (4-byte alignment pad)

Note the shape: the auto side loses MORE than the named side gains, and the
difference is exactly the evicted phantom. A wave with no phantom in its span
reads Δtotal_code == 0 (that is what lane PIN-B measured).

★ WAVE 2 (lane PIN-E) FOUND THE BIGGER MECHANISM, AND A CONTROL
---------------------------------------------------------------
Wave 2 pinned 292 rows / 168 units / 1,332,288 B in four batches. The matching
keys were Δ0 throughout; `total_code` moved −320,684 across just 9 changed
rows. Six of those were RESIZED, and every one collapsed to the size
`config/45410914/symbols.txt` already declared:

    fn_828B23A8  210,136 -> 204 B   (symbols.txt size:0xCC)
    fn_82BF9F48   51,292 ->  64 B   (size:0x40)
    fn_8287C430   46,816 ->  12 B   (size:0xC)
    fn_82BE4E70   14,148 ->  76 B   (size:0x4C)
    fn_82BCC8C0    6,972 ->   8 B   (size:0x8)
    ?ParseBooleanCastNode@…  128 -> 124 B   (size:0x7C)

⇒ In an UNPINNED region dtk cannot bound a symbol, so it runs the extent to the
next known boundary and bills a 204-byte function as 210,136 bytes — that one
row alone was ~2% of the binary's reported code. Pinning supplies the boundary
and the row collapses to the truth. So wave 1's "phantom label evicted" and
wave 2's "function extent corrected" are the SAME cause with different surface
presentations, and wave 1's second row (ComputeDotProductPrecision 96 -> 92,
filed as "a 4-byte alignment pad") is really this convergence too.

★ THE CONTROL THAT MAKES THAT CLAIM SAFE: wave 2's batch 2 pinned 70,840 B
across 45 units in 8 different families and produced **ZERO changed rows** —
the row population came back byte-for-byte identical, with 70,840 B simply
migrating auto -> named. So pinning is inherently denominator-neutral;
`total_code` moves ONLY when the span happens to contain a symbol dtk was
mis-sizing. Do not read a Δtotal_code as "pinning perturbs the denominator".

⚠ ADJUDICATE ADDED LABELS, AND DO IT AT THE symbols.txt ADDRESS. Wave 2 added
three label rows. For `lbl_82858E94` the containment probe was first run at the
name-derived address and found 1 real function inside the extent — a confident,
cheap, WRONG "double-counting" verdict. symbols.txt puts that label at
0x82887AE4, where the extent contains 0 functions. Second independent instance
of trap (1) above; it is systematic.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from collections import Counter
from pathlib import Path

SCHEMA = "report-conservation/1"

REPO = Path(__file__).resolve().parent.parent
TITLE = "45410914"
DEFAULT_REPORT = Path("build") / TITLE / "report.json"

# Inputs whose mtime, if NEWER than report.json, means the report predates the
# current pin state. Necessary but NOT sufficient — see the exit-6 note above.
CONFIG_INPUTS = [
    Path("config") / TITLE / "splits.txt",
    Path("config") / TITLE / "objects.json",
    Path("config") / TITLE / "config.yml",
]

# The four keys a pin must not move. matched_code_percent is derived, but it is
# printed because a human reading the log wants it.
MATCHING_KEYS = ["matched_functions", "matched_code", "masked_equal_functions"]
DENOMINATOR_KEYS = ["total_code", "total_functions"]
CONTEXT_KEYS = ["matched_code_percent", "fuzzy_match_percent",
                "matched_functions_percent"]

AUTO_PREFIX = "default/auto_"


def _die(code: int, msg: str) -> None:
    print(f"REFUSING: {msg}", file=sys.stderr)
    sys.exit(code)


def _int(d: dict, key: str) -> int:
    """int()-coerce; report.json ships several of these as JSON strings."""
    v = d.get(key, 0)
    if v in (None, ""):
        return 0
    return int(v)


def _float(d: dict, key: str) -> float:
    v = d.get(key, 0.0)
    if v in (None, ""):
        return 0.0
    return float(v)


# ---------------------------------------------------------------- snap


def build_snapshot(report_path: Path, project_dir: Path, label: str,
                   allow_stale: bool) -> dict:
    if not report_path.is_file():
        _die(2, f"no report.json at {report_path} — build it first "
                f"(./tools/ninja-locked, then the report target)")

    st = report_path.stat()
    stale = []
    for rel in CONFIG_INPUTS:
        p = project_dir / rel
        if p.is_file() and p.stat().st_mtime > st.st_mtime:
            stale.append(f"{rel} ({p.stat().st_mtime:.0f} > "
                         f"{st.st_mtime:.0f})")
    if stale and not allow_stale:
        _die(3, "report.json is OLDER than config inputs, so it does not "
                "describe the current pin state:\n  " + "\n  ".join(stale) +
                "\n  Rebuild, or pass --allow-stale if you truly mean it.")

    raw = report_path.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()[:16]
    rep = json.loads(raw)
    m = rep["measures"]
    units = rep["units"]

    auto = [u for u in units if u["name"].startswith(AUTO_PREFIX)]
    named = [u for u in units if not u["name"].startswith(AUTO_PREFIX)]

    rows: Counter = Counter()
    for u in units:
        for f in u.get("functions", []):
            rows[(f["name"], _int(f, "size"))] += 1

    n_rows = sum(rows.values())
    row_bytes = sum(name_size[1] * c for name_size, c in rows.items())

    snap = {
        "schema": SCHEMA,
        "label": label,
        "report_path": str(report_path),
        "report_mtime": st.st_mtime,
        "report_size": st.st_size,
        "report_sha256_16": digest,
    }
    for k in MATCHING_KEYS + DENOMINATOR_KEYS:
        snap[k] = _int(m, k)
    for k in CONTEXT_KEYS:
        snap[k] = _float(m, k)

    snap.update({
        "n_units": len(units),
        "n_auto_units": len(auto),
        "n_named_units": len(named),
        "auto_code": sum(_int(u["measures"], "total_code") for u in auto),
        "auto_fns": sum(_int(u["measures"], "total_functions") for u in auto),
        "named_code": sum(_int(u["measures"], "total_code") for u in named),
        "named_fns": sum(_int(u["measures"], "total_functions") for u in named),
        "n_rows": n_rows,
        "row_bytes": row_bytes,
        # name+size multiset — THE adjudication key. Never address-derived.
        "_rows": sorted([n, s, c] for (n, s), c in rows.items()),
        # orientation only; unit names are UNSTABLE across splits.
        "_per_unit": {u["name"]: [_int(u["measures"], "total_code"),
                                  _int(u["measures"], "total_functions")]
                      for u in units},
    })

    # Internal consistency: total_code is EXACTLY the sum of listed row sizes,
    # and total_functions is exactly the row count. If either fails, the
    # snapshot does not describe the report and every delta computed from it
    # would be fiction.
    problems = []
    if row_bytes != snap["total_code"]:
        problems.append(f"sum(row sizes)={row_bytes:,} != "
                        f"total_code={snap['total_code']:,}")
    if n_rows != snap["total_functions"]:
        problems.append(f"row count={n_rows:,} != "
                        f"total_functions={snap['total_functions']:,}")
    if problems:
        _die(5, "report.json is internally inconsistent:\n  " +
                "\n  ".join(problems))

    snap["auto_named_check"] = (snap["auto_code"] + snap["named_code"]
                                == snap["total_code"])
    return snap


def cmd_snap(args) -> int:
    project_dir = Path(args.project_dir).resolve() if args.project_dir else REPO
    report = (Path(args.report).resolve() if args.report
              else project_dir / DEFAULT_REPORT)
    s = build_snapshot(report, project_dir, args.label, args.allow_stale)
    Path(args.out).write_text(json.dumps(s, indent=1))

    print(f"snapshot '{s['label']}' <- {s['report_path']}")
    print(f"  sha256[:16]={s['report_sha256_16']}  "
          f"mtime={s['report_mtime']:.0f}  size={s['report_size']:,}")
    for k in MATCHING_KEYS + DENOMINATOR_KEYS:
        print(f"  {k:26s} = {s[k]:,}")
    for k in CONTEXT_KEYS:
        print(f"  {k:26s} = {s[k]}")
    for k in ("n_units", "n_auto_units", "n_named_units",
              "auto_code", "auto_fns", "named_code", "named_fns", "n_rows"):
        print(f"  {k:26s} = {s[k]:,}")
    print(f"  self-check: sum(row sizes) == total_code   OK")
    print(f"  self-check: row count      == total_functions OK")
    print(f"  wrote {args.out}")
    return 0


# ---------------------------------------------------------------- cmp


def _load_snap(path: str) -> dict:
    p = Path(path)
    if not p.is_file():
        _die(2, f"snapshot not found: {path}")
    try:
        s = json.loads(p.read_text())
    except (json.JSONDecodeError, UnicodeDecodeError) as e:
        _die(2, f"snapshot {path} is not valid JSON: {e}")
    if not isinstance(s, dict) or s.get("schema") != SCHEMA:
        _die(2, f"snapshot {path} has schema {s.get('schema')!r}, "
                f"expected {SCHEMA!r} — regenerate it with this tool")
    required = (MATCHING_KEYS + DENOMINATOR_KEYS +
                ["_rows", "auto_code", "named_code", "auto_fns", "named_fns",
                 "report_mtime", "report_size", "report_sha256_16"])
    missing = [k for k in required if k not in s]
    if missing:
        _die(2, f"snapshot {path} is missing required keys: {missing}")
    return s


def cmd_cmp(args) -> int:
    a = _load_snap(args.before)
    b = _load_snap(args.after)

    # THE guard that catches wave-1's actual failure: same physical report read
    # twice, dressed up as two legs.
    same = (a["report_sha256_16"] == b["report_sha256_16"]
            and a["report_size"] == b["report_size"]
            and a["report_mtime"] == b["report_mtime"])
    if same and not args.allow_same_report:
        _die(6, "BOTH snapshots were taken from the SAME PHYSICAL report.json "
                f"(sha256[:16]={a['report_sha256_16']}, "
                f"mtime={a['report_mtime']:.0f}). The build did not rerun "
                "between legs, so this comparison is vacuous — it would report "
                "a perfect Δ0 no matter what you changed. Check that "
                "configure.py and the build actually SUCCEEDED (a && chain "
                "swallowed exactly this in wave 1), and that report.json's "
                "mtime advanced.")

    print(f"=== {a['label']!r} -> {b['label']!r} ===")
    print(f"  before: sha={a['report_sha256_16']} mtime={a['report_mtime']:.0f}")
    print(f"  after : sha={b['report_sha256_16']} mtime={b['report_mtime']:.0f}")

    print("\n--- MATCHING KEYS (must be exactly 0; any movement is a STOP) ---")
    moved = []
    for k in MATCHING_KEYS:
        d = b[k] - a[k]
        if d:
            moved.append(k)
        print(f"  {k:26s} {a[k]:>14,} -> {b[k]:>14,}  delta={d:+,}"
              f"{'   <<<< MOVED' if d else ''}")
    for k in CONTEXT_KEYS:
        print(f"  {k:26s} {a[k]:>14} -> {b[k]:>14}  "
              f"delta={b[k]-a[k]:+.6f}")

    print("\n--- DENOMINATOR (pins are NOT neutral here; adjudicate rows) ---")
    for k in DENOMINATOR_KEYS:
        print(f"  {k:26s} {a[k]:>14,} -> {b[k]:>14,}  delta={b[k]-a[k]:+,}")

    print("\n--- ATTRIBUTION (auto -> named) ---")
    for k in ("n_units", "n_auto_units", "n_named_units",
              "auto_code", "auto_fns", "named_code", "named_fns"):
        print(f"  {k:26s} {a.get(k,0):>14,} -> {b.get(k,0):>14,}  "
              f"delta={b.get(k,0)-a.get(k,0):+,}")

    d_auto_c = b["auto_code"] - a["auto_code"]
    d_named_c = b["named_code"] - a["named_code"]
    d_total_c = b["total_code"] - a["total_code"]
    d_auto_f = b["auto_fns"] - a["auto_fns"]
    d_named_f = b["named_fns"] - a["named_fns"]
    d_total_f = b["total_functions"] - a["total_functions"]

    print("\n--- CONSERVATION (must reconcile exactly) ---")
    ok_c = (d_auto_c + d_named_c) == d_total_c
    ok_f = (d_auto_f + d_named_f) == d_total_f
    print(f"  auto_code {d_auto_c:+,} + named_code {d_named_c:+,} "
          f"= {d_auto_c + d_named_c:+,}   vs delta total_code {d_total_c:+,}"
          f"   {'OK' if ok_c else '*** MISMATCH ***'}")
    print(f"  auto_fns  {d_auto_f:+,} + named_fns  {d_named_f:+,} "
          f"= {d_auto_f + d_named_f:+,}   vs delta total_functions "
          f"{d_total_f:+,}   {'OK' if ok_f else '*** MISMATCH ***'}")

    # ---- row-level multiset diff: the ONLY sound adjudication ----
    ra = Counter({(n, s): c for n, s, c in a["_rows"]})
    rb = Counter({(n, s): c for n, s, c in b["_rows"]})
    removed = ra - rb
    added = rb - ra

    by_name_rm: dict[str, list] = {}
    by_name_ad: dict[str, list] = {}
    for (n, s), c in removed.items():
        by_name_rm.setdefault(n, []).append((s, c))
    for (n, s), c in added.items():
        by_name_ad.setdefault(n, []).append((s, c))

    resized, gone, appeared = [], [], []
    for n in sorted(set(by_name_rm) | set(by_name_ad)):
        rm, ad = by_name_rm.get(n), by_name_ad.get(n)
        if rm and ad and len(rm) == 1 and len(ad) == 1 and rm[0][1] == ad[0][1]:
            resized.append((n, rm[0][0], ad[0][0], rm[0][1]))
        else:
            for s, c in (rm or []):
                gone.append((n, s, c))
            for s, c in (ad or []):
                appeared.append((n, s, c))

    n_changed = len(resized) + len(gone) + len(appeared)
    print(f"\n--- CHANGED ROWS (name+size multiset over "
          f"{a['n_rows']:,} -> {b['n_rows']:,} rows): {n_changed} ---")
    if not n_changed:
        print("  (none — the row population is byte-for-byte identical)")

    lim = None if args.all_rows else args.top

    def _emit(title, items, fmt):
        if not items:
            return
        print(f"  {title}: {len(items)}")
        shown = items if lim is None else items[:lim]
        for it in shown:
            print("    " + fmt(it))
        if lim is not None and len(items) > lim:
            print(f"    ... {len(items) - lim} more (--all-rows to list)")

    _emit("REMOVED", sorted(gone, key=lambda t: -t[1] * t[2]),
          lambda t: f"{t[0]:<52} {t[1]:>9,} B  x{t[2]}  REMOVED")
    _emit("ADDED", sorted(appeared, key=lambda t: -t[1] * t[2]),
          lambda t: f"{t[0]:<52} {t[1]:>9,} B  x{t[2]}  ADDED")
    _emit("RESIZED", sorted(resized, key=lambda t: -abs(t[2] - t[1])),
          lambda t: f"{t[0]:<52} {t[1]:>9,} -> {t[2]:,} B  x{t[3]}  "
                    f"({t[2]-t[1]:+} B)")

    net = (sum(s * c for _, s, c in appeared) - sum(s * c for _, s, c in gone)
           + sum((s2 - s1) * c for _, s1, s2, c in resized))
    print(f"  net row-byte delta = {net:+,}  vs delta total_code "
          f"{d_total_c:+,}   {'OK' if net == d_total_c else '*** MISMATCH ***'}")

    print()
    verdict = []
    if moved:
        verdict.append(f"MATCHING KEYS MOVED: {moved}")
    if not (ok_c and ok_f):
        verdict.append("CONSERVATION FAILED")
    if net != d_total_c:
        verdict.append("ROW DELTA DOES NOT EXPLAIN total_code")
    if verdict:
        print("VERDICT: *** " + " | ".join(verdict) + " ***")
        return 4 if moved else 5
    print("VERDICT: MATCH-NEUTRAL — matching keys Δ0, conservation exact, "
          f"{n_changed} row(s) changed (adjudicate each above)")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__.split("\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("snap", help="snapshot report.json")
    sp.add_argument("out", help="output snapshot JSON path")
    sp.add_argument("--report", help="explicit report.json path")
    sp.add_argument("--project-dir", help="worktree root (default: this repo)")
    sp.add_argument("--label", default="snapshot")
    sp.add_argument("--allow-stale", action="store_true",
                    help="do not refuse when report.json predates its config inputs")
    sp.set_defaults(func=cmd_snap)

    cp = sub.add_parser("cmp", help="compare two snapshots")
    cp.add_argument("before")
    cp.add_argument("after")
    cp.add_argument("--top", type=int, default=40,
                    help="max rows to list per class (default 40)")
    cp.add_argument("--all-rows", action="store_true")
    cp.add_argument("--allow-same-report", action="store_true",
                    help="permit comparing two snapshots of the SAME report "
                         "file (only ever useful for testing this tool)")
    cp.set_defaults(func=cmd_cmp)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
