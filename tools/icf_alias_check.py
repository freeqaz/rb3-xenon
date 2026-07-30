#!/usr/bin/env python3
"""icf_alias_check.py — honesty audit for "ICF-alias inflation" in pinned spans.

THE LESSON (codified here)
--------------------------
When we pin a ``.text`` span (or a dual-range / identity-transfer micro-pin) over
a set of functions, some of the resulting "100% matches" are FAKE.  Tiny
functions (<= 44 bytes) — ``??_E`` deleting-dtor thunks, one-line getters,
static-init guard stubs, trivial STL accessors — ICF-fold byte-identically across
UNRELATED translation units.  Because objdiff pairs target<->base strictly by
*content* for those, such a stub registers as 100% even though the pin does not
actually OWN that code.  This produced +57 of fake matches in wave 14
(OvershellSlot-head, LockStepMgr) and another +57 in wave 15 (MusicLibrary
dual-range), each caught only by hand.

This tool automates that hand-audit.  Of the newly- (or currently-) 100%-matched
functions attributed to a pin, it asks for each: is it a REAL-BODIED method of the
claimed TU (a genuine ``> 44`` byte body, OR a high-confidence oracle attribution
to the claimed TU), or a low-confidence ``<= 44`` byte stub-fold (small AND either
no oracle / low-sim oracle / oracle pointing at a DIFFERENT TU)?

A pin is HONEST when the matched set has real-bodied anchors and is not dominated
by foreign stub-folds.  It is ICF-ALIAS INFLATION when the matched set is
dominated by foreign stub-folds with no own-bodied anchors and/or a long
contiguous run of foreign/stub functions.

Exit code 0 = HONEST, 1 = ICF-ALIAS INFLATION (so a wave audit can gate on it).

DATA SOURCES (read, never written)
----------------------------------
* ``build/45410914/report.json``        objdiff per-unit report.  Each unit has
  ``metadata.source_path`` (the claimed TU) and ``functions[]`` with ``name``,
  ``size`` (decimal bytes), ``address`` (unit-relative; NOT a VA past the first
  contiguous range — do not use it for VA), ``match_percent_normalized``.
  A matched function's ``name`` is either ``fn_<VA>`` (anonymous) or an MSVC
  mangled name (renamed by the target-symbol-renamer).
* ``config/45410914/symbols.txt``       authoritative ``fn_<VA> = .text:0x<VA>;
  // type:function size:0x<HEX>`` — sizes for every anonymous function.
* ``scripts/target_symbol_map.json``     ``"0x<VA>" -> "<mangled name>"``.
  Inverted, it recovers the VA of a renamed matched function.
* ``unified_id_rb3wii.json``             rb3-Wii BinDiff oracle, a list of
  ``{rb3_addr, wii_name, bindiff_src, similarity, ...}`` keyed by VA.  Gives the
  oracle attribution (source TU) + similarity for each VA.

STUB threshold = 44 bytes (the recurring ``<= 44B`` figure from the lesson).
Oracle "high confidence" threshold = similarity >= 0.5 (the oracle's own median
is ~0.25; only ~700 of 9300 entries clear 0.5, so >= 0.5 is a meaningful anchor).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from collections import Counter
# --- dead-index guard (lane BX-4) -------------------------------------------
# TU0-era address indices are INFORMATIONLESS after the 2026-07-15 TU0->TU5 flip
# (2-6% of their addresses are real .text function starts; chance is ~2-3%).
# Audit: python3 tools/dead_index_guard.py --audit
import os as _dig_os, sys as _dig_sys
_dig_d = _dig_os.path.dirname(_dig_os.path.abspath(__file__))
while _dig_d != "/" and not _dig_os.path.exists(
        _dig_os.path.join(_dig_d, "tools", "dead_index_guard.py")):
    _dig_d = _dig_os.path.dirname(_dig_d)
_dig_sys.path.insert(0, _dig_os.path.join(_dig_d, "tools"))
from dead_index_guard import load_guarded as _guarded_load, assert_live as _assert_live  # noqa: E402
# ----------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Paths (resolved relative to repo root = parent of this file's dir)
# ---------------------------------------------------------------------------
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

REPORT = os.path.join(ROOT, "build", "45410914", "report.json")
SYMBOLS = os.path.join(ROOT, "config", "45410914", "symbols.txt")
TSM = os.path.join(ROOT, "scripts", "target_symbol_map.json")
ORACLE = os.path.join(ROOT, "unified_id_rb3wii.json")

# ---------------------------------------------------------------------------
# Tunables
# ---------------------------------------------------------------------------
STUB_MAX = 44           # functions <= this many bytes are stub-fold candidates
SIM_HIGH = 0.5          # oracle similarity at/above this is a real attribution
# Verdict gate: a set is "inflated" when stub-folds dominate AND there is no
# own-bodied anchor support.  STUB_DOMINANCE is the fraction of the matched set
# that must be stub-folds for the headline to flip to inflation.
STUB_DOMINANCE = 0.60
# Set by main() when --no-oracle is used: the FOREIGN-attribution signal is
# unavailable, so no "HONEST" verdict from this run may be treated as a pass.
DEGRADED_NO_ORACLE = False
# A long contiguous foreign/stub run is itself a red flag (the manual heuristic).
FOREIGN_RUN_FLAG = 8


# ---------------------------------------------------------------------------
# Loaders
# ---------------------------------------------------------------------------
def load_json(path):
    with open(path) as f:
        return json.load(f)


def load_symbol_sizes(path=SYMBOLS):
    """fn_<VA> sizes from symbols.txt: {va_int: size_bytes}."""
    sizes = {}
    pat = re.compile(r"^fn_([0-9A-Fa-f]+)\s*=\s*\.text:0x([0-9A-Fa-f]+);.*size:0x([0-9A-Fa-f]+)")
    with open(path) as f:
        for line in f:
            m = pat.match(line)
            if m:
                sizes[int(m.group(2), 16)] = int(m.group(3), 16)
    return sizes


def load_name_to_va(path=TSM):
    """Invert target_symbol_map.json: {mangled_name: [va_int, ...]}.

    ICF folding means one mangled name can map to several VAs; we keep the list
    and only trust it when it resolves uniquely.
    """
    raw = load_json(path)
    out = {}
    for va, name in raw.items():
        if not isinstance(va, str) or not va.startswith("0x"):
            continue  # skip "_comment" etc.
        out.setdefault(name, []).append(int(va, 16))
    return out


def load_oracle(path=ORACLE):
    """{va_int: oracle_entry} from unified_id_rb3wii.json.

    dead-index guard (lane BX-4): this tool is used as a LANDING GATE
    (.claude/wave2_finalize.js). Its INFLATED/clean verdict partly rests on
    oracle attribution, so a dead oracle would let the gate emit a confident
    verdict from noise. Refuse instead.
    """
    out = {}
    for e in _guarded_load(str(path), what="rb3-Wii oracle (icf_alias_check LANDING GATE)"):
        addr = e.get("rb3_addr")
        if not addr:
            continue
        try:
            out[int(addr, 16)] = e
        except ValueError:
            continue
    return out


# ---------------------------------------------------------------------------
# Report helpers
# ---------------------------------------------------------------------------
def fn_va(fn, name_to_va):
    """Recover a function's VA from its report entry.

    Anonymous ``fn_<VA>`` names encode the VA directly.  Renamed (mangled)
    matched functions are looked up in the inverted target_symbol_map, trusted
    only when the name resolves to a single VA.
    """
    name = fn["name"]
    if name.startswith("fn_"):
        try:
            return int(name[3:], 16)
        except ValueError:
            return None
    vas = name_to_va.get(name)
    if vas and len(vas) == 1:
        return vas[0]
    return None


def is_matched(fn):
    return float(fn.get("match_percent_normalized", 0) or 0) >= 100.0


def unit_tu_basename(unit):
    sp = (unit.get("metadata") or {}).get("source_path") or unit.get("name") or ""
    return os.path.basename(sp)


def iter_units_report(report):
    return report.get("units", [])


# ---------------------------------------------------------------------------
# Classification
# ---------------------------------------------------------------------------
class FnVerdict:
    __slots__ = ("name", "va", "size", "matched", "sim", "src", "claimed_tu",
                 "klass", "reason")

    def __init__(self, name, va, size, matched, sim, src, claimed_tu):
        self.name = name
        self.va = va
        self.size = size
        self.matched = matched
        self.sim = sim                # oracle similarity or None
        self.src = src                # oracle bindiff_src basename or None
        self.claimed_tu = claimed_tu  # basename of the TU we're judging for
        self.klass = None             # "REAL" | "STUB"
        self.reason = ""

    def classify(self):
        big = self.size is not None and self.size > STUB_MAX
        own_oracle = (
            self.sim is not None and self.sim >= SIM_HIGH and
            self.src is not None and self.claimed_tu is not None and
            self.src.lower() == self.claimed_tu.lower()
        )
        if big:
            self.klass = "REAL"
            self.reason = f"size {self.size} > {STUB_MAX}"
        elif own_oracle:
            self.klass = "REAL"
            self.reason = f"oracle sim {self.sim:.2f} -> {self.src} (own TU)"
        else:
            self.klass = "STUB"
            if self.size is not None and self.size <= STUB_MAX:
                if self.src and self.claimed_tu and self.src.lower() != self.claimed_tu.lower():
                    self.reason = f"<= {STUB_MAX}B, oracle -> {self.src} (FOREIGN, sim {self.sim:.2f})"
                elif self.sim is not None:
                    self.reason = f"<= {STUB_MAX}B, low oracle sim {self.sim:.2f}"
                else:
                    self.reason = f"<= {STUB_MAX}B, no oracle attribution"
            else:
                self.reason = "no size (unresolved VA) and no own-oracle anchor"
        return self


def build_fn_verdict(fn, claimed_tu, sym_sizes, name_to_va, oracle):
    va = fn_va(fn, name_to_va)
    # size: prefer symbols.txt (authoritative, hex) keyed by VA, else report size.
    size = None
    if va is not None and va in sym_sizes:
        size = sym_sizes[va]
    if size is None:
        try:
            size = int(fn.get("size"))
        except (TypeError, ValueError):
            size = None
    oe = oracle.get(va) if va is not None else None
    sim = oe.get("similarity") if oe else None
    src = os.path.basename(oe.get("bindiff_src") or "") if oe else None
    if src == "":
        src = None
    fv = FnVerdict(fn["name"], va, size, is_matched(fn), sim, src, claimed_tu)
    return fv.classify()


# ---------------------------------------------------------------------------
# Subset selectors
# ---------------------------------------------------------------------------
def select_tu(report, tu, matched_only=True):
    """All (currently-matched) functions of units whose source_path basename
    matches ``tu``.  Returns (claimed_tu_basename, [fn, ...])."""
    target = os.path.basename(tu).lower()
    fns = []
    claimed = os.path.basename(tu)
    for u in iter_units_report(report):
        if unit_tu_basename(u).lower() == target:
            claimed = unit_tu_basename(u)
            for fn in u.get("functions", []):
                if (not matched_only) or is_matched(fn):
                    fns.append(fn)
    return claimed, fns


def select_range(report, lo, hi, name_to_va, matched_only=True):
    """All (currently-matched) functions whose recovered VA is in [lo, hi).
    claimed_tu is taken from whichever unit contributes most fns in range."""
    picked = []
    src_votes = Counter()
    for u in iter_units_report(report):
        tu = unit_tu_basename(u)
        for fn in u.get("functions", []):
            if matched_only and not is_matched(fn):
                continue
            va = fn_va(fn, name_to_va)
            if va is not None and lo <= va < hi:
                picked.append(fn)
                src_votes[tu] += 1
    claimed = src_votes.most_common(1)[0][0] if src_votes else None
    return claimed, picked


def matched_set_from_report(report, name_to_va):
    """{va: fn} for every 100%-matched function with a resolvable VA, plus a
    list of matched fns whose VA could not be resolved (for diffing)."""
    by_va = {}
    unresolved = []
    for u in iter_units_report(report):
        for fn in u.get("functions", []):
            if not is_matched(fn):
                continue
            va = fn_va(fn, name_to_va)
            if va is None:
                unresolved.append((unit_tu_basename(u), fn))
            else:
                by_va[va] = (unit_tu_basename(u), fn)
    return by_va, unresolved


def select_worktree_diff(wt_report, base_report, name_to_va):
    """Functions that are 100% in wt_report but NOT 100% in base_report
    (the newly-matched set).  claimed_tu per-fn comes from the worktree unit."""
    base_va, _ = matched_set_from_report(base_report, name_to_va)
    wt_va, wt_unres = matched_set_from_report(wt_report, name_to_va)
    new = []
    claimed_per = {}
    for va, (tu, fn) in wt_va.items():
        if va not in base_va:
            new.append(fn)
            claimed_per[id(fn)] = tu
    return claimed_per, new


# ---------------------------------------------------------------------------
# Foreign/stub run measurement
# ---------------------------------------------------------------------------
def longest_foreign_run(verdicts):
    """Longest contiguous (by ascending VA) run of STUB-classified functions
    whose oracle attribution is foreign or absent.  Returns (length, [fv,...])."""
    ordered = sorted((v for v in verdicts if v.va is not None), key=lambda v: v.va)
    best = []
    cur = []
    for v in ordered:
        foreign = v.klass == "STUB"
        if foreign:
            cur.append(v)
            if len(cur) > len(best):
                best = list(cur)
        else:
            cur = []
    return len(best), best


# ---------------------------------------------------------------------------
# Report / verdict printing
# ---------------------------------------------------------------------------
def print_verdict(label, verdicts, claimed_tu, show_list=False):
    matched = [v for v in verdicts if v.matched]
    total = len(matched)
    if total == 0:
        print(f"=== {label} ===")
        print(f"claimed TU: {claimed_tu}")
        print("no 100%-matched functions in the selected set — nothing to judge.")
        print("VERDICT: HONEST (empty set)"
              + (" [DEGRADED size-only -- NOT A PASS]" if DEGRADED_NO_ORACLE else ""))
        return 0

    real = [v for v in matched if v.klass == "REAL"]
    stub = [v for v in matched if v.klass == "STUB"]
    # foreign = stub whose oracle attributes to a different TU (the strongest tell)
    foreign = [v for v in stub
               if v.src and claimed_tu and v.src.lower() != claimed_tu.lower()]
    n_real, n_stub = len(real), len(stub)
    stub_frac = n_stub / total
    run_len, run = longest_foreign_run(matched)

    # own-bodied anchors: REAL fns that are real because of size/own-oracle.
    anchors = len(real)

    print(f"=== {label} ===")
    print(f"claimed TU: {claimed_tu}")
    print(f"matched (100%): {total}")
    print(f"  REAL-BODIED : {n_real:4d}  ({100*n_real/total:.1f}%)")
    print(f"  STUB-FOLD   : {n_stub:4d}  ({100*stub_frac:.1f}%)"
          f"   of which {len(foreign)} oracle-attribute to a FOREIGN TU")
    print(f"longest contiguous stub/foreign run: {run_len}"
          f"{' (>= flag threshold)' if run_len >= FOREIGN_RUN_FLAG else ''}")

    # ---- verdict logic -------------------------------------------------
    # Inflation when stub-folds DOMINATE the set AND there are no real-bodied
    # anchors to vouch for the pin (the wave-14/15 shape: a span whose every
    # match is a tiny foreign-folding stub).  A long foreign run with zero
    # anchors is also inflation even below the dominance fraction.
    inflated = False
    if anchors == 0 and n_stub > 0:
        inflated = True
        why = f"zero real-bodied anchors; all {n_stub} matches are stub-folds"
    elif stub_frac >= STUB_DOMINANCE and anchors == 0:
        inflated = True
        why = f"stub-folds dominate ({n_stub}/{total}) with no anchors"
    elif run_len >= FOREIGN_RUN_FLAG and anchors == 0:
        inflated = True
        why = f"contiguous foreign/stub run of {run_len} with no anchors"
    else:
        why = (f"{anchors} real-bodied anchor(s) present"
               + (f"; {n_stub} interspersed stub-folds are plausibly own getters"
                  if n_stub else ""))

    if inflated:
        print(f"VERDICT: ICF-ALIAS INFLATION (stub-fold-dominated: {n_stub} of {total}) "
              f"-- {why}")
    else:
        print(f"VERDICT: HONEST (real-bodied-dominated) -- {why}"
              + (" [DEGRADED size-only: FOREIGN-attribution signal UNAVAILABLE,"
                 " this does NOT rule out inflation -- NOT A PASS]"
                 if DEGRADED_NO_ORACLE else ""))

    if show_list:
        print("\n  fn (sorted by VA):")
        for v in sorted(matched, key=lambda v: (v.va is None, v.va or 0)):
            va = f"0x{v.va:08X}" if v.va is not None else "   ??     "
            sz = f"{v.size:>5}" if v.size is not None else "    ?"
            print(f"    {va}  {sz}B  {v.klass:4s}  {v.name[:46]:46s}  {v.reason}")

    return 1 if inflated else 0


# ---------------------------------------------------------------------------
# Worktree-diff report loading
# ---------------------------------------------------------------------------
def load_report_for_ref(base_ref, baseline_report, worktree):
    """Resolve the baseline report.json for the --worktree diff.

    Preference order:
      1. explicit --baseline-report path,
      2. a report.json checked out at <base-ref> via `git show` (only works if
         report.json is tracked at that ref — it usually is NOT, since it's a
         build artifact), with a clear error otherwise.
    """
    if baseline_report:
        return load_json(baseline_report), f"--baseline-report {baseline_report}"
    if base_ref:
        import subprocess
        try:
            blob = subprocess.check_output(
                ["git", "-C", worktree or ROOT, "show",
                 f"{base_ref}:build/45410914/report.json"],
                stderr=subprocess.DEVNULL)
            return json.loads(blob), f"git {base_ref}:build/45410914/report.json"
        except Exception:
            raise SystemExit(
                f"error: could not read build/45410914/report.json at ref "
                f"'{base_ref}' (it's a gitignored build artifact, so it is "
                f"almost never committed).\n"
                f"  -> build the baseline report from that ref in a separate "
                f"worktree and pass it via --baseline-report <path>.")
    raise SystemExit("error: --worktree requires either --base-ref or "
                     "--baseline-report")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main(argv=None):
    p = argparse.ArgumentParser(
        prog="icf_alias_check.py",
        description="Honesty audit for ICF-alias inflation in pinned .text "
                    "spans. Exit 0 = HONEST, 1 = ICF-ALIAS INFLATION.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "examples:\n"
            "  # judge every currently-100% fn attributed to a TU's pins\n"
            "  tools/icf_alias_check.py --tu RockCentral.cpp\n\n"
            "  # judge functions in a VA range\n"
            "  tools/icf_alias_check.py --range 0x824E8C00-0x824EA100\n\n"
            "  # judge the newly-matched set of a worktree vs a baseline report\n"
            "  tools/icf_alias_check.py --worktree /path/wt "
            "--baseline-report /path/base_report.json\n"))
    mode = p.add_mutually_exclusive_group(required=True)
    mode.add_argument("--tu", metavar="Foo.cpp",
                      help="judge all currently-100%% functions of this TU's units")
    mode.add_argument("--range", metavar="0xAAAA-0xBBBB", dest="varange",
                      help="judge currently-100%% functions in this VA range")
    mode.add_argument("--worktree", metavar="PATH",
                      help="judge a worktree's NEWLY-matched set vs a baseline")
    p.add_argument("--base-ref", metavar="GITREF",
                   help="(with --worktree) git ref to diff against")
    p.add_argument("--baseline-report", metavar="PATH",
                   help="(with --worktree) explicit baseline report.json path")
    p.add_argument("--report", metavar="PATH", default=None,
                   help=f"override report.json path (default: {REPORT})")
    p.add_argument("--symbols", metavar="PATH", default=SYMBOLS)
    p.add_argument("--oracle", metavar="PATH", default=ORACLE)
    p.add_argument("--no-oracle", action="store_true",
                   help="run DEGRADED (size-only): skip the rb3-Wii oracle "
                        "entirely. The FOREIGN-attribution signal -- the "
                        "strongest inflation tell -- is UNAVAILABLE in this "
                        "mode, so a 'clean' result is NOT a pass. Use this "
                        "while the oracle is dead (see tools/dead_index_guard.py).")
    p.add_argument("--tsm", metavar="PATH", default=TSM)
    p.add_argument("--list", action="store_true",
                   help="list every judged function with its classification")
    args = p.parse_args(argv)

    # Shared data
    sym_sizes = load_symbol_sizes(args.symbols)
    name_to_va = load_name_to_va(args.tsm)
    # dead-index guard (lane BX-4). The oracle drives `own_oracle` (REAL vs
    # STUB-FOLD) and the FOREIGN attribution that the verdict calls "the
    # strongest tell". A DEAD oracle resolves every lookup to None, which
    # empties the FOREIGN set and biases this LANDING GATE toward "clean" --
    # i.e. it would silently pass inflated pins. So: hard-fail by default,
    # and make the degraded size-only mode explicit and loudly labelled.
    if args.no_oracle:
        globals()["DEGRADED_NO_ORACLE"] = True
        oracle = {}
        sys.stderr.write(
            "\n" + "!" * 78 +
            "\n!! icf_alias_check: DEGRADED MODE (--no-oracle) -- size-only.\n"
            "!! The FOREIGN-attribution signal is UNAVAILABLE. Stub-folds that\n"
            "!! belong to another TU are indistinguishable from unattributed\n"
            "!! ones here, so this run can only ever FIND inflation, never\n"
            "!! RULE IT OUT. A 'HONEST'/clean verdict below is NOT a pass.\n" +
            "!" * 78 + "\n\n")
        sys.stderr.flush()
    else:
        oracle = load_oracle(args.oracle)

    if args.worktree:
        wt_report_path = os.path.join(args.worktree, "build", "45410914",
                                      "report.json")
        if not os.path.exists(wt_report_path):
            raise SystemExit(f"error: no report.json at {wt_report_path} "
                             f"(build the worktree first)")
        wt_report = load_json(wt_report_path)
        base_report, src_desc = load_report_for_ref(
            args.base_ref, args.baseline_report, args.worktree)
        claimed_per, new_fns = select_worktree_diff(
            wt_report, base_report, name_to_va)
        # classify each with its own claimed TU
        verdicts = []
        for fn in new_fns:
            tu = claimed_per.get(id(fn))
            verdicts.append(build_fn_verdict(fn, tu, sym_sizes, name_to_va, oracle))
        # for the verdict we need a single claimed_tu label; use the plurality
        votes = Counter(claimed_per.values())
        claimed = votes.most_common(1)[0][0] if votes else "(mixed)"
        label = f"worktree {args.worktree} newly-matched vs {src_desc}"
        return print_verdict(label, verdicts, claimed, show_list=args.list)

    report = load_json(args.report or REPORT)

    if args.tu:
        claimed, fns = select_tu(report, args.tu, matched_only=True)
        verdicts = [build_fn_verdict(fn, claimed, sym_sizes, name_to_va, oracle)
                    for fn in fns]
        return print_verdict(f"--tu {args.tu}", verdicts, claimed,
                             show_list=args.list)

    if args.varange:
        m = re.match(r"\s*(0x[0-9A-Fa-f]+)\s*-\s*(0x[0-9A-Fa-f]+)\s*$",
                     args.varange)
        if not m:
            raise SystemExit("error: --range must be 0xAAAA-0xBBBB")
        lo, hi = int(m.group(1), 16), int(m.group(2), 16)
        if hi <= lo:
            raise SystemExit("error: range hi must be > lo")
        claimed, fns = select_range(report, lo, hi, name_to_va, matched_only=True)
        verdicts = [build_fn_verdict(fn, claimed, sym_sizes, name_to_va, oracle)
                    for fn in fns]
        return print_verdict(f"--range {args.varange}", verdicts, claimed,
                             show_list=args.list)

    p.error("no mode selected")


if __name__ == "__main__":
    sys.exit(main())
