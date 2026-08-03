#!/usr/bin/env python3
"""
r31_role_census.py -- what does r31 actually HOLD in a retail function?

WHY THIS EXISTS (lane DP-2, 2026-08-03)
=======================================
A recurring screen routes work as:

    "many r31-relative OFFSET diffs, few REG diffs  =>  class-LAYOUT defect"

That inference is only valid when r31 is an OBJECT pointer.  When r31 is the
FRAME BASE, the identical diff shape means STACK-SLOT ASSIGNMENT -- a codegen /
temporary-allocation artifact, i.e. permuter class, not a source layout bug.

The two are indistinguishable in the objdiff mismatch table (both render as
`addi r3, r31, 0xNN` with an [off:+N] note), and one lane already mis-routed
three large rows this way -- reading `addi r3, r31, 0x184` as "past the end of
BandDirector (sizeof 0x16c), therefore a base-subobject adjustment on another
object", when r31 was simply the frame base of a 0x240-byte frame.

MEASURED BASE RATE (this tool, at e18482bb, over 8,753 fresh target .s files):

    functions >= 100 insns:  FRAME 54.7%  /  OBJECT 45.3%   (n=3,339 decidable)
    functions >= 400 insns:  FRAME 56.4%  /  OBJECT 43.6%   (n=  367 decidable)
    functions >= 800 insns:  FRAME 55.0%  /  OBJECT 45.0%   (n=  100 decidable)

=> "r31-relative offset diff" is worth ~a coin flip about layout-vs-stack.
   It is NOT a classifier.  Read the prologue instead -- which is what this
   tool does, deterministically and for free.

USAGE
-----
    python3 tools/r31_role_census.py                     # whole-tree census
    python3 tools/r31_role_census.py --symbol fn_822914D0
    python3 tools/r31_role_census.py --symbol '?OnFileLoaded@BandDirector@@QAA?AVDataNode@@PAVDataArray@@@Z'
    python3 tools/r31_role_census.py --project-dir ~/tmp/wt-foo

INSTRUMENT DISCIPLINE (docs/decomp/INSTRUMENT_DESIGN.md)
--------------------------------------------------------
 * rule 1  -- hard-asserts known positives; REFUSES (exit 2) rather than
              reporting a number it has not shown it can produce correctly.
 * rule 4  -- asserts BOTH labels appear on known-opposite cases, so a
              single-label (constant-function) regression fails loudly.
 * rule 13 -- records the bug this tool's own first draft had: it matched only
              `mr r31, r3` as an object and dumped `mr r31, r4/r5/r6/r7` into an
              "OTHER" bucket, which under-counted OBJECT and inflated the
              apparent FRAME:OBJECT ratio to ~2:1.  The true ratio is ~1.25:1.
              r31 = ANY incoming pointer argument is object-like.
 * CLAUDE.md -- .s files older than 2026-07-15 are stale TU0-era generations
              whose address axis is garbage (4,364 addresses where two .s files
              disagree).  They are skipped, and the skip count is REPORTED so a
              silent filter failure is visible.
"""

import argparse
import collections
import datetime
import json
import os
import re
import sys

# .s generations older than this are stale TU0-era output -- see CLAUDE.md.
STALE_CUTOFF = datetime.datetime(2026, 7, 15).timestamp()

INSN = re.compile(
    r"/\*\s*([0-9A-F]{8})\s+[0-9A-F]+\s+(?:[0-9A-F]{2} ){3}[0-9A-F]{2}\s*\*/\s*(.*)$"
)
FN_START = re.compile(r"^\s*\.fn\s+(\S+?),")
FN_END = re.compile(r"^\s*\.endfn")

# Ground truth read by hand from retail prologues (lane DP-2).  Both labels are
# represented on purpose: a single-label classifier must fail rule 4 here.
KNOWN = {
    "fn_8228B380": "FRAME",   # BandCharacter::Handle       subi r31, r1, 0x150
    "fn_822914D0": "FRAME",   # BandDirector::OnFileLoaded  subi r31, r1, 0x240
    "fn_8255F9D0": "FRAME",   # UIStats::MaybePublish       subf r31, r12, r1
    "fn_8228C8B0": "OBJECT",  # small BandCharacter fn      mr   r31, r3
}

ARG_REGS = {"r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10"}

# How far into the body to look for the instruction that establishes r31.
PROLOGUE_WINDOW = 20


def classify(insns):
    """insns: [(addr, text), ...] in order. -> (label, evidence_text)

    FRAME  r31 derived from r1  (the stack pointer)
    OBJECT r31 derived from an incoming pointer arg, or loaded through one
    CONST  r31 set to a constant (li/lis) -- not a base pointer
    UNRESOLVED  r31 written from something we do not model
    NO_R31_WRITE  r31 never established in the prologue window
    """
    for addr, t in insns[:PROLOGUE_WINDOW]:
        m = re.match(r"(subi|addi|subf|mr|or|add|lwz|li|lis)\s+r31\s*,\s*(.*)$", t)
        if not m:
            continue
        op, rest = m.group(1), m.group(2)
        parts = [x.strip() for x in rest.split(",")]
        if op == "subf":
            # subf rD, rA, rB  =>  rD = rB - rA.  Frame form: subf r31, r12, r1
            return ("FRAME" if parts[-1] == "r1" else "OBJECT"), "%s  %s" % (addr, t)
        if op in ("li", "lis"):
            return "CONST", "%s  %s" % (addr, t)
        if op == "lwz":
            # a pointer loaded out of something -- object-like, never the frame
            return "OBJECT", "%s  %s" % (addr, t)
        if "r1" in parts:
            return "FRAME", "%s  %s" % (addr, t)
        if any(p in ARG_REGS for p in parts):
            return "OBJECT", "%s  %s" % (addr, t)
        return "UNRESOLVED", "%s  %s" % (addr, t)
    return "NO_R31_WRITE", ""


def scan(asm_root):
    """-> (results, n_files, n_stale)  results: name -> (label, n_insns, evidence, path)"""
    results = {}
    n_files = 0
    n_stale = 0
    for root, _dirs, files in os.walk(asm_root):
        for x in files:
            if not x.endswith(".s"):
                continue
            path = os.path.join(root, x)
            try:
                if os.path.getmtime(path) < STALE_CUTOFF:
                    n_stale += 1
                    continue
            except OSError:
                continue
            n_files += 1
            cur = None
            buf = []
            try:
                fh = open(path, encoding="utf-8", errors="replace")
            except OSError:
                continue
            with fh:
                for line in fh:
                    m = FN_START.match(line)
                    if m:
                        cur, buf = m.group(1), []
                        continue
                    if FN_END.match(line):
                        if cur and buf:
                            prev = results.get(cur)
                            if prev is None or len(buf) > prev[1]:
                                lab, ev = classify(buf)
                                results[cur] = (lab, len(buf), ev, path)
                        cur, buf = None, []
                        continue
                    if cur is not None:
                        mi = INSN.search(line)
                        if mi:
                            buf.append((mi.group(1), mi.group(2).strip()))
    return results, n_files, n_stale


def run_controls(results):
    """rule 1 + rule 4. Returns a list of problems (empty == PASS)."""
    problems = []
    for fn, want in KNOWN.items():
        got = results.get(fn)
        if got is None:
            problems.append("known positive %s NOT FOUND (scan may be vacuous)" % fn)
        elif got[0] != want:
            problems.append(
                "known positive %s: want %s, got %s  [%s]" % (fn, want, got[0], got[2])
            )
    labels = {v[0] for v in results.values()}
    for need in ("FRAME", "OBJECT"):
        if need not in labels:
            problems.append(
                "classifier never produced %s -- single-label output, see rule 4" % need
            )
    return problems


def resolve_symbol(project_dir, sym):
    """Accept a mangled symbol and map it to fn_<ADDR> via target_symbol_map.json."""
    if re.match(r"^fn_[0-9A-Fa-f]{8}$", sym):
        return "fn_" + sym[3:].upper()
    mp = os.path.join(project_dir, "scripts", "target_symbol_map.json")
    try:
        with open(mp, encoding="utf-8") as fh:
            m = json.load(fh)
    except OSError:
        return None
    for addr, name in m.items():
        if name == sym:
            return "fn_" + addr[2:].upper()
    return None


def main():
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project-dir", default=here,
                    help="repo or worktree root (default: this tool's repo)")
    ap.add_argument("--symbol", help="report r31's role for one function "
                                     "(fn_8228B380 or a mangled symbol)")
    args = ap.parse_args()

    asm_root = os.path.join(args.project_dir, "build", "45410914", "asm")
    if not os.path.isdir(asm_root):
        print("no target asm at %s -- run a build first" % asm_root)
        return 3

    results, n_files, n_stale = scan(asm_root)

    problems = run_controls(results)
    if problems:
        print("REFUSED -- instrument failed its own controls (no numbers reported):")
        for p in problems:
            print("   *", p)
        return 2
    print("controls PASS: %d known positives reproduced; both labels present"
          % len(KNOWN))
    print("scanned %d .s files; skipped %d stale (pre-2026-07-15) generations"
          % (n_files, n_stale))
    print("functions classified: %d\n" % len(results))

    if args.symbol:
        key = resolve_symbol(args.project_dir, args.symbol)
        if key is None or key not in results:
            print("symbol not found: %s (resolved to %s)" % (args.symbol, key))
            return 3
        lab, n, ev, path = results[key]
        print("%s  (%s)" % (args.symbol, key))
        print("  r31 role : %s" % lab)
        print("  evidence : %s" % (ev or "<r31 never established in prologue>"))
        print("  size     : %d instructions" % n)
        print("  file     : %s" % path)
        print()
        if lab == "FRAME":
            print("  => r31-relative offset diffs in this function are STACK SLOTS.")
            print("     Do NOT read them as a class-layout defect.")
        elif lab == "OBJECT":
            print("  => r31 holds an object pointer; offset diffs here MAY be layout.")
            print("     Confirm which object before editing any header.")
        return 0

    for lo in (0, 100, 200, 400, 800):
        sub = [v for v in results.values() if v[1] >= lo]
        if not sub:
            continue
        c = collections.Counter(v[0] for v in sub)
        dec = c["FRAME"] + c["OBJECT"]
        print("--- functions with >= %4d instructions   (n=%d) ---" % (lo, len(sub)))
        for lab in ("FRAME", "OBJECT", "CONST", "UNRESOLVED", "NO_R31_WRITE"):
            print("      %-13s %6d   %5.1f%%" % (lab, c.get(lab, 0),
                                                 100.0 * c.get(lab, 0) / len(sub)))
        if dec:
            print("      among DECIDABLE (FRAME+OBJECT, n=%d):  FRAME %.1f%%  OBJECT %.1f%%"
                  % (dec, 100.0 * c["FRAME"] / dec, 100.0 * c["OBJECT"] / dec))
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
