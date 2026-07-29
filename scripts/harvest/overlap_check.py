#!/usr/bin/env python3
"""overlap_check.py - splits.txt section-range overlap gate (PIPELINE-DESIGN.md
Phase 7 / honesty gate #7; SOP step 2 from scripts/harvest/README.md).

THE PROBLEM (the SOP snippet, lifted into a callable script)
------------------------------------------------------------
``config/45410914/splits.txt`` pins one or more ``.text start:..  end:..`` ranges
per TU; dtk SPLITs each into a per-TU target ``.obj``.  Two INDEPENDENTLY-developed
lanes (or an identity-transfer micro-pin appended next to an existing span) can
pin OVERLAPPING ``.text`` ranges.  jeff/dtk's ``validate_splits`` forbids two
units owning one address, so an overlap **breaks the build** -- and the wave-loop
union scripts (``resolve_splits_union.py``) do a line-union that does NOT detect a
range overlap (wave-9 CriticalUserListener/ViewSetting boundary collision).

This is the SOP "splits overlap self-check BEFORE building" snippet
(``scripts/harvest/README.md`` step 2) promoted to a real, importable gate so the
identity-transfer driver (``scripts/idtransfer_harvest.py``) AND ``land.sh`` share
one implementation instead of copy-pasting prose.

THE GATE
--------
``.text`` overlap is a HARD FAIL (exit 1): ANY two ``.text`` ranges that overlap
abort.  ``.pdata`` overlap is ALSO reported and (by default) a hard fail, because
the SOP checks both; pass ``--text-only`` to gate strictly on ``.text`` (the
task-B3 contract: "ABORT on ANY two .text ranges overlapping").

A range is half-open ``[start, end)``; two ranges overlap iff
``a.start < b.end and b.start < a.end`` (touching end-to-start is NOT an overlap).

USAGE (CLI)
-----------
  scripts/harvest/overlap_check.py [SPLITS]            # default config/.../splits.txt
  scripts/harvest/overlap_check.py /path/wt/config/45410914/splits.txt
  scripts/harvest/overlap_check.py --text-only         # gate on .text only
  scripts/harvest/overlap_check.py --quiet             # only print on overlap

Exit code 0 = no overlap (clean), 1 = overlap detected (abort).

USAGE (import)
--------------
  from overlap_check import find_overlaps, check_splits
  overlaps = find_overlaps(splits_path, section="text")   # list of OverlapPair
  rc = check_splits(splits_path, text_only=True)           # 0 clean / 1 overlap
"""
import argparse
import os
import re
import sys

# A unit header line: ``Foo.cpp:`` (or .c / .cc) alone on its line.
UNIT_RE = re.compile(r"^(\S+\.(?:cpp|c|cc)):\s*$")
# A section range line: ``.text start:0xAAAA end:0xBBBB`` (any section name).
RANGE_RE = re.compile(
    r"^\s*\.(\w+)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")


class Range:
    """One pinned ``[lo, hi)`` range in splits.txt, tagged with its owning unit
    header and the source line number (for actionable error messages)."""
    __slots__ = ("section", "lo", "hi", "unit", "lineno")

    def __init__(self, section, lo, hi, unit, lineno):
        self.section = section
        self.lo = lo
        self.hi = hi
        self.unit = unit
        self.lineno = lineno

    def __repr__(self):
        return (f"0x{self.lo:08X}..0x{self.hi:08X} .{self.section} "
                f"[{self.unit or '?'}:{self.lineno}]")


class OverlapPair:
    """Two ranges that overlap, with the overlapping byte interval."""
    __slots__ = ("a", "b", "olo", "ohi")

    def __init__(self, a, b):
        self.a = a
        self.b = b
        self.olo = max(a.lo, b.lo)
        self.ohi = min(a.hi, b.hi)

    def describe(self):
        return (f".{self.a.section} OVERLAP 0x{self.olo:08X}..0x{self.ohi:08X}\n"
                f"      {self.a!r}\n"
                f"  vs  {self.b!r}")


def parse_ranges(splits_path):
    """Return [Range, ...] for EVERY section range in splits.txt, each tagged
    with the unit header it appears under (multi-range / multi-section aware)."""
    ranges = []
    cur_unit = None
    with open(splits_path) as fh:
        for i, line in enumerate(fh, start=1):
            m = UNIT_RE.match(line)
            if m:
                cur_unit = m.group(1)
                continue
            r = RANGE_RE.match(line)
            if r:
                ranges.append(Range(
                    section=r.group(1),
                    lo=int(r.group(2), 16),
                    hi=int(r.group(3), 16),
                    unit=cur_unit,
                    lineno=i,
                ))
    return ranges


def find_overlaps(splits_path, section="text"):
    """Return [OverlapPair, ...] for all overlapping ranges in ``section``.

    Overlap is half-open: ranges that merely touch (a.hi == b.lo) do NOT overlap.
    A degenerate range (hi <= lo) is reported separately by the CLI but never
    counts as overlapping (it has no interior to collide with)."""
    rs = [r for r in parse_ranges(splits_path)
          if r.section == section and r.hi > r.lo]
    rs.sort(key=lambda r: (r.lo, r.hi))
    # prefix_max_hi[j] = max(hi) over rs[0..j].  The back-scan below must stop
    # only when NO earlier range can reach cur.lo -- stopping at the first
    # non-reaching NEIGHBOR (the pre-2026-07-29 behavior, `rs[j].hi > cur.lo`
    # as the loop condition) silently missed a WIDE early range swallowing a
    # later small one whenever a short non-overlapping range sat between them
    # (found by splits_move.py's synthetic wide-swallow audit test).
    prefix_max_hi = []
    m = 0
    for r in rs:
        m = max(m, r.hi)
        prefix_max_hi.append(m)
    overlaps = []
    for i in range(1, len(rs)):
        cur = rs[i]
        j = i - 1
        while j >= 0 and prefix_max_hi[j] > cur.lo:
            other = rs[j]
            if other.lo < cur.hi and cur.lo < other.hi:
                overlaps.append(OverlapPair(other, cur))
            j -= 1
    return overlaps


def degenerate_ranges(splits_path, section=None):
    """Return ranges with hi <= lo (malformed pins). ``section=None`` = all."""
    return [r for r in parse_ranges(splits_path)
            if r.hi <= r.lo and (section is None or r.section == section)]


def check_splits(splits_path, text_only=False, quiet=False):
    """The gate: return 0 if clean, 1 if any gated section has an overlap.

    ``text_only=True`` gates ONLY on ``.text`` (the task-B3 contract). Otherwise
    BOTH ``.text`` and ``.pdata`` are gated (the SOP checks both)."""
    if not os.path.isfile(splits_path):
        print(f"overlap_check: ERROR splits file not found: {splits_path}",
              file=sys.stderr)
        return 2

    sections = ["text"] if text_only else ["text", "pdata"]
    any_overlap = False
    total_overlaps = 0
    for sec in sections:
        overlaps = find_overlaps(splits_path, section=sec)
        total_overlaps += len(overlaps)
        if overlaps:
            any_overlap = True
            print(f"overlap_check: !! {len(overlaps)} .{sec} OVERLAP(S) in "
                  f"{splits_path}", file=sys.stderr)
            for op in overlaps:
                print("  " + op.describe(), file=sys.stderr)
        elif not quiet:
            n = sum(1 for r in parse_ranges(splits_path) if r.section == sec
                    and r.hi > r.lo)
            print(f"overlap_check: .{sec} OK ({n} ranges, 0 overlaps)")

    # Degenerate ranges are a separate malformity; warn but don't gate on them
    # (a hi==lo pin is a no-op, not a build break) -- a hi<lo pin IS broken.
    bad = [r for r in degenerate_ranges(splits_path) if r.hi < r.lo]
    if bad:
        print(f"overlap_check: WARN {len(bad)} malformed range(s) (hi < lo):",
              file=sys.stderr)
        for r in bad:
            print(f"  {r!r}", file=sys.stderr)

    if any_overlap:
        print(f"overlap_check: ABORT -- {total_overlaps} overlap(s) "
              f"(independently-developed pins collide; jeff validate_splits "
              f"will reject the build).", file=sys.stderr)
        return 1
    return 0


def main(argv=None):
    repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    default_splits = os.path.join(repo, "config", "45410914", "splits.txt")

    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("splits", nargs="?", default=default_splits,
                    help=f"path to splits.txt (default: {default_splits})")
    ap.add_argument("--text-only", action="store_true",
                    help="gate ONLY on .text overlaps (task-B3 contract); "
                         "default also gates .pdata (the SOP checks both)")
    ap.add_argument("--quiet", action="store_true",
                    help="suppress the per-section OK lines; only print on overlap")
    args = ap.parse_args(argv)
    return check_splits(args.splits, text_only=args.text_only, quiet=args.quiet)


if __name__ == "__main__":
    sys.exit(main())
