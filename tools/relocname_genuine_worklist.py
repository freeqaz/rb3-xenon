#!/usr/bin/env python3
"""GENUINE-residue worklist from the decomp-synth relocation-name audit (WS-4).

WHAT THE INPUT IS
-----------------
decomp-synth's ``tools/revcomp/probes/probe_icf_foldtest.py`` took every
relocation-name disagreement this tree produces at the ``name_check`` ruler --
12,679 (T, B) call-site records, T = the name the retail split gives the callee,
B = the name our object references -- and compared our compiled body for B
against RETAIL BYTES at addr(T) read out of the PE image, chasing one level of
tail-call thunk. Archived, with both its verdicts per record, at
``decomp-bench/archive/harvest/relocname-audit-2026-08-06/pairs_folded2.json``.

  FOLD / FOLD via thunk            7,882   -> fed to tools/icf_alias_build.py --xfold
  GENUINE: different size          3,169   -> THIS FILE
  GENUINE: same size, code differs   187   -> THIS FILE
  non-call / one-sided reloc       1,000
  unresolved                         441

WHAT "GENUINE" MEANS, AND WHAT IT DOES NOT
------------------------------------------
*** GENUINE IS AN UPPER BOUND ON DEFECTS, NOT A DEFECT COUNT. *** A body
difference at a charged site has two causes and this instrument cannot separate
them:

  1. our source calls the wrong function            -- a SOURCE defect; or
  2. the retail symbol map mis-attributes the name at the destination VA
                                                    -- a MAP defect, in which
     case our source is right and the charge is wrong.

This repo's entire ``map(...)`` lane series exists to repair cause 2, and the
project's own adjudicated figure for the whole at-100 charged population is
**298 fns / 25,920 B / 0.2425 pp WRONG** (lane CW-2, ``34017f74``), against
lane CV-4's earlier *estimate* of 353 fns / 24,836 B / 0.23 pp (``34b44dd6``).
So treat a row here as *a locus worth reading retail bytes at*, never as a
confirmed bug, and never auto-apply anything from it.

⚠ THE UPSTREAM COMPARATOR IS COARSER THAN THIS REPO'S. ``foldtest2.shape()``
masks EVERY D-form displacement and EVERY branch displacement unconditionally --
not merely the relocated ones -- and never compares relocation TARGET NAMES.
That runs in the benign-manufacturing direction, so it OVER-produces FOLD and
therefore UNDER-produces GENUINE: this worklist is, if anything, short. It is
not a population census and must not be quoted as one. The instruments that can
adjudicate a row are ``tools/xbin_adjudicate.py`` (cross-binary, masks exactly
the relocated field using our own relocation table, two channels, calibrated)
and ``tools/at100_adjudicate.py``.

BUCKET NAMES ARE THE MEASURED FACT, NOT AN INFERRED REMEDY (rule 14). The
``body`` column says only what was compared and how it came out. Nothing here
labels a row map-fixable or source-fixable; ``tools/at100_sibling_split.py``
supplies the one structural split that can (T and B two instantiations of the
same template => our source is INCAPABLE of the alleged bug => it must be a map
defect).

STALENESS
---------
The tool refuses on an input whose sha256 does not match the recorded one, and
reports (never silently drops) how far the audit's names still join against the
CURRENT ``scripts/target_symbol_map.json`` and our CURRENT compiled objs. A row
whose ``joins`` column is not ``TB`` has aged: the map or the build moved under
it since 2026-08-06 and it must be re-derived before it is worked.

Usage:
  tools/relocname_genuine_worklist.py --out docs/decomp/relocname-genuine-worklist-WS4.tsv
  tools/relocname_genuine_worklist.py --check docs/decomp/relocname-genuine-worklist-WS4.tsv
"""

import argparse
import collections
import glob
import hashlib
import json
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools"))

DEFAULT_PAIRS = (Path.home() / "code/milohax/decomp-bench/archive/harvest"
                 / "relocname-audit-2026-08-06" / "pairs_folded2.json")

# sha256 of the archived classification this worklist was cut from
# (decomp-bench 6cc3caa6, 12,679 records, both verdicts per record).
PAIRS_SHA256 = "af61ba6ae8b4e1f0cda7ec7fe3769c1da9ee089c567ceeb457aefe6239da55e1"

AUDIT_TREE = "a236686e"          # rb3-xenon commit the audit was measured at
RULER_SHA256 = "ca2be75232767f531cf997dd94f3b98c7d29a5ae3805f5a737d49fc1b2809a2b"

BODY = {
    "GENUINE: different size": "DIFF_SIZE",
    "GENUINE: same size, different code": "DIFF_SAMESIZE",
}

COLS = ["pair_sites", "fn_sites", "body", "map", "joins", "opcode", "unit",
        "enclosing_fn", "target_callee", "our_callee"]


def head_commit() -> str:
    try:
        return subprocess.run(["git", "rev-parse", "--short", "HEAD"],
                              cwd=PROJECT_ROOT, capture_output=True, text=True,
                              check=True).stdout.strip()
    except Exception:
        return "?"


def compiled_symbol_names() -> set:
    """Every symbol name our built objs define or reference."""
    from icf_alias_finder import coff_referenced_symbols
    objs = sorted(glob.glob(str(PROJECT_ROOT / "build/45410914/src/**/*.obj"),
                            recursive=True))
    names = set()
    for p in objs:
        names |= coff_referenced_symbols(Path(p).read_bytes())
    if len(names) < 0.5 * max(len(objs), 1):
        sys.exit("REFUSING: the COFF read collapsed (%d names from %d objs). A "
                 "`joins` column of all-N would read exactly like 'the whole "
                 "worklist aged out'." % (len(names), len(objs)))
    return names


def build_rows(pairs_path: Path):
    raw = pairs_path.read_bytes()
    got = hashlib.sha256(raw).hexdigest()
    if got != PAIRS_SHA256:
        sys.exit("REFUSING: %s has sha256 %s, expected %s. This worklist is cut "
                 "from one frozen classification; re-cut it deliberately rather "
                 "than silently against a different input." % (pairs_path, got,
                                                               PAIRS_SHA256))
    recs = json.loads(raw)
    gen = [r for r in recs if str(r.get("fold", "")) in BODY]
    if not gen:
        sys.exit("REFUSING: 0 GENUINE records out of %d. Schema drift reads "
                 "exactly like 'the residue is empty'." % len(recs))

    tm = json.loads((PROJECT_ROOT / "scripts" / "target_symbol_map.json").read_text())
    mapped = {v for k, v in tm.items()
              if isinstance(k, str) and k.lower().startswith("0x") and isinstance(v, str)}
    ours = compiled_symbol_names()

    # One INPUT RECORD is one charged call site, and a function can charge the
    # same (T,B) several times, so the raw records carry duplicate rows. Collapse
    # to (unit, fn, T, B) and keep the multiplicity in `fn_sites` -- lossless,
    # and it stops a 5x-repeated row reading as five separate leads.
    pair_sites = collections.Counter((r["target_symbol"], r["base_symbol"]) for r in gen)
    fn_sites = collections.Counter(
        (r.get("unit", "?"), r.get("sym", "?"), r["target_symbol"], r["base_symbol"])
        for r in gen)
    seen, rows = set(), []
    for r in gen:
        t, b = r["target_symbol"], r["base_symbol"]
        key = (r.get("unit", "?"), r.get("sym", "?"), t, b)
        if key in seen:
            continue
        seen.add(key)
        rows.append({
            "pair_sites": pair_sites[(t, b)],
            "fn_sites": fn_sites[key],
            "body": BODY[r["fold"]],
            "map": r.get("verdict", "?"),
            "joins": ("T" if t in mapped else "-") + ("B" if b in ours else "-"),
            "opcode": r.get("opcode", "?"),
            "unit": r.get("unit", "?"),
            "enclosing_fn": r.get("sym", "?"),
            "target_callee": t,
            "our_callee": b,
        })
    assert sum(r["fn_sites"] for r in rows) == len(gen), \
        "collapse lost sites: %d != %d" % (sum(r["fn_sites"] for r in rows), len(gen))
    rows.sort(key=lambda x: (-x["pair_sites"], x["unit"], x["enclosing_fn"],
                             x["target_callee"], x["our_callee"]))
    return rows


def render(rows) -> str:
    jt = collections.Counter(r["joins"] for r in rows)
    bt = collections.Counter(r["body"] for r in rows)
    out = [
        "# GENUINE residue of the relocation-name audit -- WORKLIST, NOT A DEFECT LIST.",
        "# Regenerate: tools/relocname_genuine_worklist.py --out <this file>",
        "# Source     : decomp-bench/archive/harvest/relocname-audit-2026-08-06/"
        "pairs_folded2.json (sha256 %s...)" % PAIRS_SHA256[:16],
        "# Audit tree : rb3-xenon@%s   ruler: name_check, objdiff-cli sha256 %s..."
        % (AUDIT_TREE, RULER_SHA256[:16]),
        "# Cut at     : rb3-xenon@%s" % head_commit(),
        "# Rows       : %d rows = %d charged call sites over %d distinct (T,B) "
        "pairs, %d enclosing functions, %d units"
        % (len(rows), sum(r["fn_sites"] for r in rows),
           len({(r["target_callee"], r["our_callee"]) for r in rows}),
           len({(r["unit"], r["enclosing_fn"]) for r in rows}),
           len({r["unit"] for r in rows})),
        "# pair_sites : charged sites for this (T,B) pair BINARY-WIDE."
        "  fn_sites: charged sites for it inside THIS enclosing function.",
        "# body       : %s" % "  ".join("%s=%d" % kv for kv in sorted(bt.items())),
        "#              DIFF_SIZE = our body for B differs in LENGTH from retail at"
        " addr(T); DIFF_SAMESIZE = same length, different code.",
        "# map        : the SYMBOL-MAP verdict for the same pair. It disagrees with"
        " `body` constantly and the body test is the one to believe --",
        "#              6,677 pairs the map test calls a defect are byte-identical"
        " bodies. Kept only so the divergence stays visible.",
        "# joins      : T = target callee still in scripts/target_symbol_map.json;"
        " B = our callee still named by a built obj, AT THE CUT COMMIT.",
        "#              %s" % "  ".join("%s=%d" % kv for kv in sorted(jt.items())),
        "#",
        "# *** GENUINE IS AN UPPER BOUND ON DEFECTS. *** A body difference means"
        " EITHER our source calls the wrong function OR the retail map",
        "# mis-attributes the name at the destination VA -- and this repo's whole"
        " map(...) lane series exists to repair the latter. The adjudicated",
        "# figure for the whole at-100 charged population is 298 fns / 25,920 B /"
        " 0.2425 pp WRONG (lane CW-2, 34017f74); CV-4's earlier estimate was",
        "# 353 fns / 24,836 B / 0.23 pp (34b44dd6). Adjudicate a row with"
        " tools/xbin_adjudicate.py (cross-binary, masks exactly the relocated",
        "# field via our own relocation table, calibrated) or"
        " tools/at100_adjudicate.py -- NEVER from the symbol map, and NEVER"
        " auto-apply.",
        "# The upstream comparator masks every D-form and branch displacement"
        " unconditionally and ignores relocation target names, so it",
        "# over-produces FOLD and this residue is if anything SHORT. Not a"
        " population census.",
        "\t".join(COLS),
    ]
    for r in rows:
        out.append("\t".join(str(r[c]) for c in COLS))
    return "\n".join(out) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pairs", default=str(DEFAULT_PAIRS))
    ap.add_argument("--out", default="")
    ap.add_argument("--check", default="",
                    help="regenerate and compare against an existing TSV; exit 3 "
                         "if the data rows differ (the header carries the cut "
                         "commit and is expected to move)")
    args = ap.parse_args()

    rows = build_rows(Path(args.pairs).expanduser())
    text = render(rows)

    if args.check:
        have = Path(args.check).read_text()
        a = [l for l in have.splitlines() if not l.startswith("#")]
        b = [l for l in text.splitlines() if not l.startswith("#")]
        if a != b:
            print("STALE: %s differs from a fresh cut (%d vs %d data lines)"
                  % (args.check, len(a), len(b)))
            return 3
        print("CURRENT: %s matches a fresh cut (%d data lines)" % (args.check, len(a)))
        return 0

    if args.out:
        Path(args.out).write_text(text)
        print("wrote %s (%d rows)" % (args.out, len(rows)))
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
