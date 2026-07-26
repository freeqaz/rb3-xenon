#!/usr/bin/env python3
"""autocarve_funnel.py -- measure the `auto_03_*` unowned-address pool honestly.

THE POOL
--------
`config/45410914/splits.txt` pins per-object `.text` ranges; dtk carves the
retail XEX into one target `.obj` per pinned unit.  Any `.text` that no unit
claims is auto-carved into a *synthetic* unit named `default/auto_03_<VA>_text`
(`metadata.auto_generated = true` in `build/45410914/report.json`).

Address-space views of this pool read very large -- ~27k functions -- and have
repeatedly been proposed as "the biggest remaining bucket".  This tool exists so
nobody has to re-derive the funnel by hand, because the raw count is misleading
in three separate ways:

  1.  **The vendor window dominates it.**  `0x82800000 .. 0x82D00000` is XDK +
      Quazal, hard-skipped by the project owner: 15,940 of the 27,454 functions
      (58%).  Any count that does not exclude it is meaningless.
  2.  **It counts `.fn` entries, not functions.**  ~3/4 of the in-scope
      remainder is <= 68 bytes with modal sizes 40 B and 32 B -- MSVC PPC EH
      cleanup funclets and `??__E`/`??__F` init/atexit thunks, each carved
      separately because each owns a `.pdata` record.  One source function is
      not one `.fn`.
  3.  **Attribution alone cannot score an anonymous function.**  objdiff pairs
      Code symbols by NAME; there is no positional fallback.  The only
      name-free path is `pair_funclets_by_bytes`, a reloc-masked byte
      signature restricted to funclet-shaped names -- uniqueness-gated ONLY in
      its first pass (objdiff-core/src/diff/mod.rs ~1471; passes 2/2b/3 pair
      ambiguous, over-subscribed and same-size-fuzzy groups with NO uniqueness
      requirement, and all four passes landed in the same objdiff commit
      b01e3efa, so "uniqueness-gated" was never accurate -- see
      docs/plans/lane-am-diffunit-2026-07-26.md and
      docs/plans/lane-an-pdata-parentage-2026-07-26.md) -- and the
      global reconcile pass explicitly REFUSES anonymous names.  So pinning an
      unowned span makes its real anonymous functions score exactly never; it
      only harvests the boilerplate crumbs, and only once the owning TU is
      already ported.

Full evidence: `docs/plans/lane-al-autocarve-2026-07-26.md`.

WHAT IT PRINTS
--------------
  * the funnel: raw -> in-scope -> named vs anonymous -> map-known
  * the size histogram that separates funclet crumbs from real code
  * contiguous-run geometry (whole missing TUs vs COMDAT scatter fragments)
  * the MIDDLE-hole candidates -- gaps that start exactly at one pinned unit's
    `.text` end and end exactly at another's start.  This is the only span class
    with a defensible ADD/extend attribution, and `--middle-out` writes it as
    JSON for `homing_apply4.py` / `splits_move.py` to consume.  Calibration: a
    prior lane measured 87.2% of splits holes to be *genuine COMDAT scatter*,
    not defects, so expect a low hit rate.

USAGE
-----
  autocarve_funnel.py [--worktree WT] [--middle-out middle.json] [--quiet]
"""
import argparse
import collections
import json
import os
import re
import statistics
import sys

VENDOR_LO = 0x82800000   # XDK + Quazal: hard-skipped by the project owner
VENDOR_HI = 0x82D00000
FUNCLET_MAX = 68         # measured upper bound of the auto-pairing crumb class


def load_pins(splits_path):
    """-> sorted [(start, end, unit)] of every pinned .text range."""
    pins, cur = [], None
    with open(splits_path) as fh:
        for line in fh:
            m = re.match(r"^(\S.*):\s*$", line)
            if m:
                cur = m.group(1)
                continue
            m = re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", line)
            if m and cur:
                pins.append((int(m.group(1), 16), int(m.group(2), 16), cur))
    pins.sort()
    return pins


def load_spans(report_path):
    """-> sorted [dict] of every auto_03 .text span, with in_scope flagged."""
    rep = json.load(open(report_path))
    spans = []
    for u in rep["units"]:
        name = u["name"]
        if "/auto_03_" not in name:
            continue
        va = int(name.split("auto_03_")[1][:8], 16)
        secs = u.get("sections") or []
        size = int(secs[0]["size"]) if secs else 0
        fns = [
            dict(name=f["name"], size=int(f["size"]), pct=f.get("match_percent_normalized"))
            for f in u.get("functions", [])
        ]
        spans.append(dict(unit=name, va=va, end=va + size, bytes=size, fns=fns,
                          in_scope=not (VENDOR_LO <= va < VENDOR_HI)))
    spans.sort(key=lambda s: s["va"])
    return spans


def merge_runs(spans, slack=16):
    """Merge address-contiguous spans into runs (whole-TU vs scatter signal)."""
    runs = []
    for s in spans:
        if runs and s["va"] <= runs[-1]["end"] + slack:
            runs[-1]["end"] = max(runs[-1]["end"], s["end"])
            runs[-1]["nf"] += len(s["fns"])
        else:
            runs.append(dict(va=s["va"], end=s["end"], nf=len(s["fns"])))
    return runs


def middle_holes(spans, pins):
    """Gaps starting exactly at a pin END and ending exactly at a pin START."""
    ends = {e: u for _, e, u in pins}
    starts = {s: u for s, _, u in pins}
    out = []
    for s in spans:
        if not s["in_scope"] or not s["fns"]:
            continue
        if s["va"] in ends and s["end"] in starts:
            out.append(dict(
                va=hex(s["va"]), end=hex(s["end"]), bytes=s["bytes"],
                prev=ends[s["va"]], next=starts[s["end"]],
                same_unit=ends[s["va"]] == starts[s["end"]],
                nf=len(s["fns"]),
                sizes=[f["size"] for f in s["fns"]],
                names=[f["name"] for f in s["fns"]],
            ))
    out.sort(key=lambda c: -c["nf"])
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--worktree", default=os.environ.get(
        "AUTOCARVE_WT", "/home/free/code/milohax/rb3-xenon"))
    ap.add_argument("--middle-out", help="write MIDDLE-hole candidates as JSON")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    wt = args.worktree
    report = os.path.join(wt, "build/45410914/report.json")
    splits = os.path.join(wt, "config/45410914/splits.txt")
    for p in (report, splits):
        if not os.path.exists(p):
            sys.exit(f"missing {p} -- build the worktree first")

    spans = load_spans(report)
    pins = load_pins(splits)
    inscope = [s for s in spans if s["in_scope"]]

    raw_fns = sum(len(s["fns"]) for s in spans)
    ins_fns = [f for s in inscope for f in s["fns"]]
    anon = [f for f in ins_fns if f["name"].startswith("fn_")]
    named = [f for f in ins_fns if not f["name"].startswith("fn_")]

    tmap_path = os.path.join(wt, "scripts/target_symbol_map.json")
    mapped = 0
    if os.path.exists(tmap_path):
        tmap = json.load(open(tmap_path))
        mva = {k.lower() for k in tmap if k.startswith("0x")}
        mapped = sum(1 for f in anon if "0x" + f["name"][3:].lower() in mva)

    print("=== FUNNEL ===")
    print(f"{len(spans):6d} auto_03 .text spans    {raw_fns:6d} functions (RAW)")
    print(f"{len(inscope):6d} in-scope spans        {len(ins_fns):6d} functions "
          f"(excl. vendor 0x{VENDOR_LO:08X}-0x{VENDOR_HI:08X})")
    print(f"{'':6s}   ...named (identity known) {len(named):6d}")
    print(f"{'':6s}   ...anonymous fn_          {len(anon):6d}")
    print(f"{'':6s}      of which in target_symbol_map: {mapped}")
    print("  NOTE: attribution alone cannot score an anonymous function -- objdiff")
    print("        pairs Code symbols by NAME (see module docstring).")

    sizes = sorted(f["size"] for f in anon)
    if sizes:
        print("\n=== SIZE HISTOGRAM (anonymous, in-scope) ===")
        for th in (16, 32, 44, FUNCLET_MAX, 128, 256, 512):
            n = sum(1 for s in sizes if s <= th)
            print(f"  <= {th:4d} B: {n:6d} ({100.0 * n / len(sizes):5.1f}%)")
        print(f"  median {statistics.median(sizes):.0f} B, mean {statistics.mean(sizes):.1f} B,"
              f" total {sum(sizes)} B")
        crumbs = sum(1 for s in sizes if s <= FUNCLET_MAX)
        print(f"  -> {crumbs} (<= {FUNCLET_MAX} B) are the funclet/thunk crumb class:")
        print("     they pair for free by byte signature ONCE the owning TU is ported.")
        print(f"  -> {len(sizes) - crumbs} above that are the only plausibly-real code,")
        print("     and each needs an IDENTITY (map lane) before any source work helps.")
        top = collections.Counter(s for s in sizes if s <= FUNCLET_MAX).most_common(6)
        print(f"  modal small sizes: {top}")

    runs = merge_runs(inscope)
    big = [r for r in runs if r["end"] - r["va"] >= 8192]
    print("\n=== GEOMETRY ===")
    print(f"  {len(inscope)} spans merge into {len(runs)} contiguous runs "
          f"({sum(r['end'] - r['va'] for r in runs)} B)")
    print(f"  runs >= 8 KB: {len(big)} holding {sum(r['nf'] for r in big)} functions")
    print(f"  the other {len(runs) - len(big)} runs are small interleaved fragments"
          " -- COMDAT scatter, not whole missing TUs")
    if not args.quiet:
        for r in sorted(big, key=lambda r: -r["nf"])[:10]:
            print(f"    0x{r['va']:08X} bytes={r['end'] - r['va']:7d} fns={r['nf']}")

    mids = middle_holes(inscope, pins)
    same = [c for c in mids if c["same_unit"]]
    print("\n=== MIDDLE HOLES (the only defensible ADD/extend class) ===")
    print(f"  {len(mids)} gaps with >=1 carved function, {sum(c['nf'] for c in mids)} functions total")
    print(f"  {len(same)} of them have the SAME unit on both sides (interior hole)")
    print("  CALIBRATION: a prior lane measured 87.2% of splits holes to be genuine")
    print("               COMDAT scatter, not defects. Expect a low hit rate.")
    if not args.quiet:
        for c in mids[:15]:
            tag = "SAME" if c["same_unit"] else "diff"
            print(f"    {c['va']}..{c['end']} {c['bytes']:6d}B nf={c['nf']:3d} [{tag}] "
                  f"{c['prev']} -> {c['next']}")

    if args.middle_out:
        json.dump(mids, open(args.middle_out, "w"), indent=1)
        print(f"\n  wrote {len(mids)} MIDDLE candidates -> {args.middle_out}")


if __name__ == "__main__":
    main()
