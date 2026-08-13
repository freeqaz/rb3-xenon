#!/usr/bin/env python3
"""Size the dtk `auto_*` (unattributed) class: what fraction is ATTRIBUTABLE?

Lane NOOBJ-1 established that `matched_code_percent` has a structural ceiling
(only units with a compiled base obj can ever draw credit) and that the
`auto_*` class -- address ranges dtk could not attribute to any TU -- is the
only unpairable class that can shrink.  Shrinking it raises the ceiling itself.

This tool answers the question nobody had sized: **how much of that class is
actually attributable with the instruments we have?**

METHOD.  Retail RB3 is built without whole-program optimization, so TU spatial
grouping in `.text` is preserved (CLAUDE.md).  An unattributed row therefore
inherits provenance from the pinned units that flank it in ADDRESS space.  For
each `auto_*` row we take the pinned `.text` span immediately before and after
it and classify by whether those units' SOURCE is available:

    SRC_REAL       source file present and substantial (>= MIN_REAL_LINES)
    SRC_SCAFFOLD   present but a map-only stub (< MIN_REAL_LINES lines)
    SRC_MISSING    declared in objects.json, file absent (the xdk/ vendor case)
    NO_ENTRY       pinned but absent from objects.json

Only a row flanked by SRC_REAL on BOTH sides is attributable-and-portable: it
can be pinned to a TU we can actually compile.  A row flanked by scaffolds can
be *named* but never ported, which buys a pairable row at 0% with no content --
the `ForceEmit_*`-class metric fitting NOOBJ-1 correctly refused.

CONTROLS (--control).  A provenance-by-adjacency instrument is worthless
unaudited, and a control whose population is defined by the absence of what it
measures cannot fail.  So the control runs on the PINNED spans, where the truth
is known: hide each span's identity, predict its class from its two flanking
spans, and compare.  It commits only when both neighbours agree and ABSTAINS
otherwise, so precision is reported per verdict -- that is the number the
sizing rests on.  Measured 2026-08-13 at 7a6de44d:

    ours-vs-thirdparty : OURS 99.27% precision (FP 0.73%), THIRDPARTY 93.03%
    flanking-srcclass  : 98.85% overall (FP 1.15%), 151 abstains

A third, weaker instrument is reported and deliberately NOT used: "enclosed by
the same heading on both sides implies membership in that heading" scores only
66.24% (FP 33.76%).  It looked like the cheapest possible attribution and it
failed its control; it is kept here so nobody re-derives it.

TRAPS this tool exists to avoid (each has cost a lane):

  * splits.txt headings are bare/nested and basenames COLLIDE (Movie.cpp is in
    both rnddx9/ and rndobj/).  Everything keys on the FULL heading, and object
    resolution replicates tools/project.py's own objects() + basename-alias
    step rather than reconstructing paths.
  * The `.s`/report address columns are SYNTHETIC for multi-block units.  Row
    VAs are taken from the `fn_<VA>`/`lbl_<VA>` SYMBOL NAME only; named rows get
    their VA by reverse lookup through target_symbol_map.json.
  * report.json numerics are JSON *strings* and protobuf-JSON omits defaults,
    so every read is int(x.get(k, 0)).
  * total_code / total_functions MOVE when pins change -- always read them.

Self-validation: rows/bytes over all units must equal total_functions /
total_code exactly, or the join dropped rows and every number is wrong.

    python3 tools/autoid_census.py
    python3 tools/autoid_census.py --control       # run + print the controls
    python3 tools/autoid_census.py --queue 40      # the attributable work queue
"""

import argparse
import bisect
import collections
import importlib.util
import json
import os
import re
import sys
from pathlib import Path

DEFAULT_REPORT = "build/45410914/report.json"
MIN_REAL_LINES = 20  # below this a .cpp is a map-only scaffold, not portable source

THIRD_PARTY_PREFIXES = ("xdk/", "network/")


# --------------------------------------------------------------------------- #
# inputs
# --------------------------------------------------------------------------- #
def load_text_spans(splits_path):
    """[(start, end, heading)] for every .text range, keyed on the FULL heading."""
    spans, head = [], None
    with open(splits_path) as fh:
        for line in fh:
            if line and not line[0].isspace():
                s = line.strip()
                head = s[:-1] if s.endswith(":") else None
                continue
            if head and head != "Sections" and ".text" in line and "start:" in line:
                parts = line.split()
                start = int(next(p for p in parts if p.startswith("start:"))[6:], 16)
                end = int(next(p for p in parts if p.startswith("end:"))[4:], 16)
                spans.append((start, end, head))
    spans.sort()
    return spans


def unit_of_heading(head):
    for suf in (".cpp", ".c", ".s"):
        if head.endswith(suf):
            return head[: -len(suf)]
    return head


def resolve_objects():
    """objects.json path_key -> Object, with project.py's basename aliases.

    Mirrors generate_build() so this tool cannot drift from what the build
    actually resolves.  Ambiguous basenames are deliberately NOT aliased.
    """
    sys.argv = ["configure.py"]
    sys.path.insert(0, os.getcwd())
    import tools.project as _project

    # configure.py calls generate_build() at import time, which REWRITES
    # build.ninja / objdiff.json.  This census is read-only.
    _project.generate_build = lambda *a, **kw: None
    spec = importlib.util.spec_from_file_location("cfg", "configure.py")
    cfg = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(cfg)
    except SystemExit:
        pass
    objects = cfg.config.objects()
    alias = {}
    for path_key, obj in objects.items():
        base = Path(path_key).name
        if base == path_key or base in objects:
            continue
        alias[base] = None if base in alias else obj
    for base, obj in alias.items():
        if obj is not None and base not in objects:
            objects[base] = obj
    return objects


class SrcClassifier:
    """heading -> SRC_REAL / SRC_SCAFFOLD / SRC_MISSING / NO_SRC_PATH / NO_ENTRY."""

    def __init__(self, objects):
        self.objects = objects
        self.memo = {}

    def __call__(self, head):
        unit = unit_of_heading(head)
        if unit in self.memo:
            return self.memo[unit]
        verdict = "NO_ENTRY"
        for cand in (unit + ".cpp", unit + ".c", unit + ".s", unit):
            obj = self.objects.get(cand)
            if obj is None:
                continue
            if obj.src_path is None:
                verdict = "NO_SRC_PATH"
            elif not obj.src_path.exists():
                verdict = "SRC_MISSING"
            else:
                try:
                    with open(obj.src_path, errors="ignore") as fh:
                        nlines = sum(1 for _ in fh)
                except OSError:
                    nlines = 0
                verdict = "SRC_SCAFFOLD" if nlines < MIN_REAL_LINES else "SRC_REAL"
            break
        self.memo[unit] = verdict
        return verdict


def is_third_party(head):
    return head.startswith(THIRD_PARTY_PREFIXES)


# --------------------------------------------------------------------------- #
# controls -- run on PINNED spans, where the truth is known
# --------------------------------------------------------------------------- #
def run_controls(spans, srcclass):
    print("\n=== CONTROLS (on PINNED spans, where provenance is known) ===")
    print("Each hides a span's identity and predicts it from the two flanking spans.")
    print("It COMMITS only when both neighbours agree, and ABSTAINS otherwise, so")
    print("what matters is PRECISION PER VERDICT -- that is what the sizing rests on.\n")

    def neighbours(i):
        j = i - 1
        while j >= 0 and spans[j][2] == spans[i][2]:
            j -= 1
        k = i + 1
        while k < len(spans) and spans[k][2] == spans[i][2]:
            k += 1
        return (j, k) if (j >= 0 and k < len(spans)) else (None, None)

    for label, fn in (
        ("ours-vs-thirdparty", lambda h: "THIRDPARTY" if is_third_party(h) else "OURS"),
        ("flanking-srcclass", srcclass),
    ):
        prec = collections.defaultdict(lambda: [0, 0])
        abstain = 0
        for i in range(len(spans)):
            j, k = neighbours(i)
            if j is None:
                continue
            a, b = fn(spans[j][2]), fn(spans[k][2])
            if a != b:
                abstain += 1
                continue
            prec[a][0] += 1
            if a == fn(spans[i][2]):
                prec[a][1] += 1
        tot = sum(v[0] for v in prec.values())
        ok = sum(v[1] for v in prec.values())
        print(
            "  [%s] commits %d, abstains %d -- overall %.2f%% (FP %.2f%%)"
            % (label, tot, abstain, 100.0 * ok / max(1, tot), 100.0 * (tot - ok) / max(1, tot))
        )
        for verdict, (n, good) in sorted(prec.items(), key=lambda kv: -kv[1][0]):
            print(
                "      says %-14s right %5d/%5d = %6.2f%%  (FP %5.2f%%)"
                % (verdict, good, n, 100.0 * good / n, 100.0 * (n - good) / n)
            )

    # The instrument that FAILED, kept so nobody re-derives it.
    ok = bad = 0
    for i in range(1, len(spans) - 1):
        if spans[i - 1][2] == spans[i + 1][2]:
            if spans[i][2] == spans[i - 1][2]:
                ok += 1
            else:
                bad += 1
    print(
        "\n  [REJECTED] 'enclosed by same heading on both sides implies membership':"
    )
    print(
        "      %d/%d = %.2f%% (FP %.2f%%) -- too weak to use; do not re-derive."
        % (ok, ok + bad, 100.0 * ok / max(1, ok + bad), 100.0 * bad / max(1, ok + bad))
    )


# --------------------------------------------------------------------------- #
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--report", default=DEFAULT_REPORT)
    ap.add_argument("--splits", default="config/45410914/splits.txt")
    ap.add_argument("--objdiff", default="objdiff.json")
    ap.add_argument("--symbol-map", default="scripts/target_symbol_map.json")
    ap.add_argument("--control", action="store_true", help="run the instrument controls")
    ap.add_argument("--queue", type=int, default=0, help="print top N attributable gaps")
    args = ap.parse_args()

    report = json.load(open(args.report))
    objdiff = {u["name"]: u for u in json.load(open(args.objdiff))["units"]}
    measures = report["measures"]
    total_code = int(measures["total_code"])
    total_functions = int(measures["total_functions"])

    spans = load_text_spans(args.splits)
    starts = [s[0] for s in spans]
    srcclass = SrcClassifier(resolve_objects())

    # ---- partition every unit, and self-validate the join ------------------ #
    rows_seen = bytes_seen = 0
    auto_units = []
    pairable = [0, 0, 0]
    for unit in report["units"]:
        fns = unit.get("functions") or []
        nrows = len(fns)
        nbytes = sum(int(f.get("size", 0)) for f in fns)
        rows_seen += nrows
        bytes_seen += nbytes
        od = objdiff.get(unit["name"], {})
        if od.get("base_path"):
            pairable[0] += 1
            pairable[1] += nrows
            pairable[2] += nbytes
        elif od.get("metadata", {}).get("auto_generated"):
            auto_units.append((unit, nrows, nbytes))
    if rows_seen != total_functions or bytes_seen != total_code:
        sys.exit(
            "REFUSING: join dropped rows -- %d/%d rows, %d/%d bytes."
            % (rows_seen, total_functions, bytes_seen, total_code)
        )
    print(
        "join self-validates: %d rows == total_functions, %d bytes == total_code"
        % (rows_seen, bytes_seen)
    )

    auto_rows = sum(a[1] for a in auto_units)
    auto_bytes = sum(a[2] for a in auto_units)
    nonempty = sum(1 for a in auto_units if a[1] > 0)
    print(
        "\nauto_* class: %d units, %d rows, %d B (%.2f%% of total_code)"
        % (len(auto_units), auto_rows, auto_bytes, 100.0 * auto_bytes / total_code)
    )
    print(
        "  of those units only %d carry ANY row; %d are empty shells "
        "(.pdata + zero-row .text slivers)" % (nonempty, len(auto_units) - nonempty)
    )

    if args.control:
        run_controls(spans, srcclass)

    # ---- classify each auto row by flanking source availability ------------ #
    symmap = json.load(open(args.symbol_map))
    name_to_va = {n: int(a, 16) for a, n in symmap.items() if a.startswith("0x")}

    verdicts = collections.defaultdict(lambda: [0, 0])
    gaps = collections.defaultdict(lambda: [0, 0])
    for unit, _, _ in auto_units:
        for f in unit.get("functions") or []:
            name = f["name"]
            size = int(f.get("size", 0))
            m = re.fullmatch(r"(?:fn|lbl)_([0-9A-Fa-f]{8})", name)
            va = int(m.group(1), 16) if m else name_to_va.get(name)
            if va is None:
                verdicts["NO_VA"][0] += 1
                verdicts["NO_VA"][1] += size
                continue
            i = bisect.bisect_right(starts, va) - 1
            prev = spans[i][2] if i >= 0 else None
            nxt = spans[i + 1][2] if i + 1 < len(spans) else None
            if prev is None or nxt is None:
                key = "EDGE"
            else:
                a, b = srcclass(prev), srcclass(nxt)
                key = a if a == b else "MIXED(%s/%s)" % (a, b)
                if a == b == "SRC_REAL":
                    gaps[(prev, nxt)][0] += 1
                    gaps[(prev, nxt)][1] += size
            verdicts[key][0] += 1
            verdicts[key][1] += size

    print("\n=== auto_* rows by FLANKING SOURCE AVAILABILITY ===")
    print("%-34s %7s %10s %8s %8s" % ("verdict", "rows", "bytes", "%auto", "%code"))
    for k, v in sorted(verdicts.items(), key=lambda kv: -kv[1][1]):
        print(
            "  %-32s %7d %10d %7.1f%% %7.2f%%"
            % (k, v[0], v[1], 100.0 * v[1] / auto_bytes, 100.0 * v[1] / total_code)
        )

    att_rows, att_bytes = verdicts["SRC_REAL"]
    print(
        "\n>>> ATTRIBUTABLE-AND-PORTABLE (SRC_REAL on both flanks): "
        "%d rows / %d B = %.1f%% of the auto class, %.2f%% of total_code"
        % (att_rows, att_bytes, 100.0 * att_bytes / auto_bytes, 100.0 * att_bytes / total_code)
    )
    upper = att_bytes + sum(
        v[1] for k, v in verdicts.items() if k.startswith("MIXED") or k == "EDGE"
    )
    print(
        "    upper bound if every MIXED/EDGE row were also ours: %d B (%.2f%% of total_code)"
        % (upper, 100.0 * upper / total_code)
    )
    print(
        "    ceiling now: PAIRABLE %d B = %.2f%% of total_code"
        % (pairable[2], 100.0 * pairable[2] / total_code)
    )

    if args.queue:
        print("\n=== attributable work queue (gaps flanked by real source) ===")
        print("%9s %6s   %s" % ("bytes", "rows", "gap between"))
        for (a, b), v in sorted(gaps.items(), key=lambda kv: -kv[1][1])[: args.queue]:
            print("%9d %6d   %s  ->  %s" % (v[1], v[0], a, b))


if __name__ == "__main__":
    main()
