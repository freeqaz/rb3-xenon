#!/usr/bin/env python3
"""Pinning queue from the TARGET SYMBOL MAP — a TU5-native identification source.

Why this exists
---------------
`tools/pin_candidates.py` merges five identification oracles (bindiff,
callgraph, RTTI, vtable, string-autoid) — but **all four of its oracle JSONs are
TU0-era and DEAD**: the 2026-07-15 TU0->TU5 flip re-laid-out `.text`, so only
2.7-5.5% of their addresses are real function starts (chance ~2-3%).
`dead_index_guard` correctly refuses them and the ranker emits 0 rows. It is
inert until those oracles are regenerated.

`scripts/target_symbol_map.json` is **not** dead. Measured 2026-08-06:
**27,952 of 28,009 addresses (99.80%) are real `.text` function starts.** It is
the live TU5 identification oracle, and thousands of the functions it names sit
outside every explicit pin — i.e. in dtk's `auto_*` leftover units, where they
have no source file and can never be matched. For those, identification is
already done and **only the pin is missing**.

This groups those named-but-unpinned functions by owning class and emits one
candidate `.text` span each.

    python3 tools/pin_from_symnames.py                          # summary + head
    python3 tools/pin_from_symnames.py -o docs/decomp/queue.tsv
    python3 tools/pin_from_symnames.py --clean-only --min-fns 3

⚠ Provenance: `docs/decomp/pin-queue-symmap-2026-08-06.tsv` was cut with
`--min-fns 3` (307 clean regions / 1,800,480 B) — NOT the default `--min-fns 2`,
which yields 399 / 1,873,812 B. Verified byte-identical reproduction 2026-08-06.
The stderr summary now prints the effective params so a queue's provenance is
always in its generation log.

Provenance flags carried through from the map (do not ignore these)
-------------------------------------------------------------------
- `_denylist` — hand-flagged bad guesses. **Always excluded.**
- `_bijection_arbitrary` (1,119) — name assigned by a bijection over a
  reloc-masked byte-identical equivalence class. The BYTES are right; the NAME
  is one arbitrary choice among twins.
- `_icf_arbitrary` (27) — arbitrary pick among linker-ICF-folded twins.

Both arbitrary classes are fine for **map value** (the address really is that
code) but unreliable for **choosing a source file**, so they are counted per
cluster in `n_arbitrary` rather than silently dropped. A cluster that is mostly
arbitrary is a pin candidate, not a decomp candidate.

Hazards handled deliberately (all from CLAUDE.md)
-------------------------------------------------
- **Never reads a `.s` address column** — SYNTHETIC for multi-block units.
  Addresses come from the map; sizes from `symbols.txt`.
- **Straddles are reported, not dropped** — overlapping an existing pin or a
  sibling usually means interleaved TUs, which is real information.
- **A liveness gate runs on every invocation** and REFUSES below a floor, so
  this tool can never quietly become the next dead index.

⚠ What this does NOT do
-----------------------
- It does not decide the true object extent. `span_lo`/`span_hi` are the
  **INNER** span (first->last member); the real boundary lies in the gap before
  the next cluster. Inner-span vs gap-extended is a judgement call.
- A class name is not a file name. Confirm against an oracle tree before
  writing splits.
- It cannot see functions that are neither pinned nor named — so `n_fns` is a
  **LOWER BOUND** (lane PIN-A: dtk found 29 functions in a span the queue
  advertised as 27; the 2 extras were unnamed).

⛔ Defects measured by the manual-pin validation lanes (2026-08-06, PIN-A/PIN-B)
-------------------------------------------------------------------------------
A v2 must fix these before the queue drives a pin wave:

1. **`cls` is a PLURALITY VOTE and can be outright FALSE.** Unmangled C symbols
   vote None and are silently uncounted, so one bogus mangled entry can label a
   whole run (the `MetaPerformer` row was 26/27 XDK voice-chat code labelled by
   a single wrong map entry, since denylisted). Queue-wide at --min-fns 3:
   18.2% of rows unlabeled, 26.9% minority-labelled, 11 rows labelled by ONE
   member. Emit label_votes/label_conf; NEVER derive a splits unit name from
   `cls` (it would have created a duplicate unit heading here).
2. **One-unit-per-row is wrong granularity for most of the mass.** DC3's leaked
   linker map (../dc3-decomp/orig/373307D9/ham_xbox_r.map) resolves the owning
   .obj for members in 270/308 rows, and 88 rows (58.9% of queue bytes) span
   >=2 objects. Sub-split runs by DC3-map obj and use its unit names (PIN-B
   landed xdk/xgraphics/{optimize,xenosem,buildssa}.cpp this way, 40d568ce).
3. **The ?$-template skip breaks run CONTINUITY** — a skipped template opens a
   >gap hole that cuts a run mid-object (~24% of rows have a template-artifact
   boundary). Keep templates in runs for adjacency; exclude from labeling only.
4. **A splits heading with no objects.json entry is HARD-REFUSED by
   configure.py.** Rows are pin *candidates*, not applyable pins: each pin
   needs an objects.json `NonMatching` entry (no source file required —
   PIN-B's landed pins prove that path).

✅ Also settled by those lanes: **pinning is METRIC-NEUTRAL** (Δ exactly 0 on
matched/masked/honest/code%/total_code, both lanes, settled ab_measure legs).
`auto_*` units are already in the denominator — report keys are
`default/auto_…`, so a `startswith('auto_')` filter is a false negative. A pin
only reattributes bytes; a pin wave needs no A/B.
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import sys
from bisect import bisect_right
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SYMBOLS = REPO / "config" / "45410914" / "symbols.txt"
SPLITS = REPO / "config" / "45410914" / "splits.txt"
TARGET_MAP = REPO / "scripts" / "target_symbol_map.json"

SYM_RE = re.compile(r"^(?P<name>\S+)\s*=\s*\.text:0x(?P<addr>[0-9A-Fa-f]+);(?P<rest>.*)$")
SIZE_RE = re.compile(r"size:0x([0-9A-Fa-f]+)")
UNIT_RE = re.compile(r"^(?P<unit>\S[^:]*):\s*$")
TEXT_RE = re.compile(r"^\s+\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")

_CLASS_RES = (
    re.compile(r"^\?\?[0-9A-Za-z_]([A-Za-z0-9_]+)@@"),   # ??0Foo@@ / ??1Foo@@
    re.compile(r"^\?[A-Za-z0-9_]+@([A-Za-z0-9_]+)@"),    # ?Bar@Foo@
)
_TEMPLATE = re.compile(r"\?\$")

LIVENESS_FLOOR = 0.50   # dead TU0 oracles scored 0.027-0.055


def owning_class(mangled: str):
    for rx in _CLASS_RES:
        m = rx.match(mangled)
        if m:
            return m.group(1)
    return None


def load_pins(path: Path):
    pins, unit = [], None
    for line in path.read_text(errors="replace").splitlines():
        m = UNIT_RE.match(line)
        if m:
            unit = m.group("unit")
            continue
        m = TEXT_RE.match(line)
        if m:
            pins.append((int(m.group(1), 16), int(m.group(2), 16), unit or "?"))
    pins.sort()
    return pins


def load_sizes(path: Path):
    """addr -> size, plus the set of real .text function starts (liveness gate)."""
    sizes, starts = {}, set()
    for line in path.read_text(errors="replace").splitlines():
        m = SYM_RE.match(line)
        if not m or "type:function" not in m.group("rest"):
            continue
        a = int(m.group("addr"), 16)
        starts.add(a)
        sm = SIZE_RE.search(m.group("rest"))
        if sm:
            sizes[a] = int(sm.group(1), 16)
    return sizes, starts


class Cover:
    """O(log n) 'is this address inside any pinned range'."""

    def __init__(self, pins):
        self.starts = [p[0] for p in pins]
        self.pins = pins

    def __call__(self, addr: int) -> bool:
        i = bisect_right(self.starts, addr) - 1
        return i >= 0 and addr < self.pins[i][1]


def span_overlap(a: int, b: int, ranges) -> int:
    t = 0
    for r in ranges:
        x, y = max(a, r[0]), min(b, r[1])
        if y > x:
            t += y - x
    return t


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--out", help="write the TSV here")
    ap.add_argument("--min-fns", type=int, default=2,
                    help="drop runs with fewer than N unpinned members (default 2)")
    ap.add_argument("--gap", type=lambda s: int(s, 0), default=0x40,
                    help="max byte gap between adjacent functions in one run "
                         "(default 0x40; inter-function padding is small, so a "
                         "tight gap keeps runs honest)")
    ap.add_argument("--clean-only", action="store_true",
                    help="keep only clusters with zero pinned AND zero sibling overlap")
    ap.add_argument("--include-templates", action="store_true",
                    help="include ?$ template/STL COMDATs (default excluded: they ICF-fold)")
    ap.add_argument("--symbols", default=str(SYMBOLS))
    ap.add_argument("--splits", default=str(SPLITS))
    ap.add_argument("--target-map", default=str(TARGET_MAP))
    args = ap.parse_args(argv)

    log = sys.stderr
    print(f"params: min_fns={args.min_fns} gap=0x{args.gap:x} "
          f"clean_only={args.clean_only} include_templates={args.include_templates}",
          file=log)
    raw = json.loads(Path(args.target_map).read_text())
    deny = set(raw.get("_denylist") or [])
    arb = set(raw.get("_bijection_arbitrary") or []) | set(raw.get("_icf_arbitrary") or [])
    named = {int(k, 16): v for k, v in raw.items()
             if k.startswith("0x") and k not in deny}
    arb_addrs = {int(a, 16) for a in arb if a.startswith("0x")}

    pins = load_pins(Path(args.splits))
    sizes, starts = load_sizes(Path(args.symbols))
    if not pins or not sizes or not named:
        print("REFUSING: empty pins / symbols / map — check input paths", file=log)
        return 2

    # ---- liveness gate: never let this become the next dead index -------------
    live = sum(1 for a in named if a in starts) / len(named)
    print(f"target map liveness : {live*100:.2f}% of {len(named):,} addrs are real "
          f".text starts", file=log)
    if live < LIVENESS_FLOOR:
        print(f"REFUSING: liveness {live*100:.2f}% < floor {LIVENESS_FLOOR*100:.0f}% — "
              f"this map looks TU-stale (dead TU0 oracles scored 2.7-5.5%).", file=log)
        return 2

    covered = Cover(pins)
    print(f"pinned .text blocks : {len(pins):,}", file=log)

    unpinned = [(a, n) for a, n in named.items() if not covered(a)]
    ub = sum(sizes.get(a, 0) for a, _ in unpinned)
    print(f"NAMED + UNPINNED    : {len(unpinned):,} functions ({ub:,} B)", file=log)

    # ---- group into CONTIGUOUS ADDRESS RUNS, not by class ---------------------
    # Class grouping is the wrong unit here: several classes routinely share one
    # object (measured — all 120 class-clusters in the D3D shader-compiler region
    # straddle a sibling, because those classes interleave inside a few big vendor
    # objects). A pin is an address range, so cluster by adjacency and *label*
    # with the classes present.
    members_all = []
    skipped_tpl = 0
    for a, n in sorted(unpinned):
        if not args.include_templates and _TEMPLATE.search(n):
            skipped_tpl += 1
            continue
        members_all.append((a, n, sizes.get(a, 0)))
    print(f"  template/STL skipped: {skipped_tpl:,}", file=log)

    runs, cur = [], []
    for rec in members_all:
        if cur:
            pa, _, ps_ = cur[-1]
            if rec[0] - (pa + ps_) > args.gap:
                runs.append(cur)
                cur = []
        cur.append(rec)
    if cur:
        runs.append(cur)
    print(f"  contiguous runs (gap<=0x{args.gap:x}): {len(runs):,}", file=log)

    cands = []
    for run in runs:
        if len(run) < args.min_fns:
            continue
        lo = run[0][0]
        hi = max(a + s for a, _, s in run)
        cls_ct = collections.Counter()
        for a, n, _ in run:
            c = owning_class(n)
            if c:
                cls_ct[c] += 1
        label = ",".join(c for c, _ in cls_ct.most_common(3)) or "-"
        cands.append({
            "cls": label, "n": len(run), "lo": lo, "hi": hi,
            "span": hi - lo, "own": sum(s for _, _, s in run),
            "narb": sum(1 for a, _, _ in run if a in arb_addrs),
            "ov_pin": span_overlap(lo, hi, pins),
        })

    # ⚠ With run grouping, runs are DISJOINT BY CONSTRUCTION (built from sorted
    # non-overlapping members), so ov_sib is structurally ~0 — it survives from
    # the class-grouping era and is kept only so a future grouping change that
    # reintroduces overlap becomes visible. "CLEAN" therefore effectively means
    # "no overlap with EXISTING PINS", not "passed a sibling test".
    ranges = [(c["lo"], c["hi"], c["cls"]) for c in cands]
    for c in cands:
        c["ov_sib"] = span_overlap(c["lo"], c["hi"],
                                   [r for r in ranges if r[2] != c["cls"]])

    total = len(cands)
    if args.clean_only:
        cands = [c for c in cands if not c["ov_pin"] and not c["ov_sib"]]
    clean = [c for c in cands if not c["ov_pin"] and not c["ov_sib"]]
    cands.sort(key=lambda c: (bool(c["ov_pin"]), bool(c["ov_sib"]), -c["own"]))

    print(f"\nclusters (>={args.min_fns} fns): {total:,}"
          f"    CLEAN (no overlap): {len(clean):,}", file=log)
    print(f"CLEAN owned bytes   : {sum(c['own'] for c in clean):,}"
          f"  in {sum(c['span'] for c in clean):,} B of span", file=log)
    narb = sum(c["narb"] for c in clean)
    if narb:
        print(f"  ⚠ {narb:,} CLEAN members carry an ARBITRARY name "
              f"(bijection/ICF twin) — pin value yes, source-file choice no", file=log)
    if total and len(clean) < total:
        print(f"  ⚠ {total-len(clean):,} clusters straddle a pin or sibling — "
              f"interleaved TUs, adjudicate before pinning", file=log)

    hdr = ("cls\tn_fns\tn_arbitrary\tspan_lo\tspan_hi\tspan_bytes\towned_bytes"
           "\toverlap_pinned\toverlap_sibling")
    lines = [hdr] + [
        f"{c['cls']}\t{c['n']}\t{c['narb']}\t0x{c['lo']:08X}\t0x{c['hi']:08X}"
        f"\t{c['span']}\t{c['own']}\t{c['ov_pin']}\t{c['ov_sib']}" for c in cands]

    if args.out:
        Path(args.out).write_text("\n".join(lines) + "\n")
        print(f"\nwrote {args.out} ({len(cands)} rows)", file=log)
    else:
        print("\n".join(lines[:26]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
