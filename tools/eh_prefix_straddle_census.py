#!/usr/bin/env python3
"""Census: EH-prefix straddles at splits.txt `.text` pin boundaries.

WHAT A STRADDLE IS
------------------
MSVC emits an 8-byte EH prefix IMMEDIATELY BEFORE a function's entry (measured
39,922/39,922, zero exceptions -- the "EH PREFIX extent artifact" class).  In the
real link that prefix belongs to the same COMDAT as the function it precedes.
But our `.text` pins are cut at FUNCTION ENTRIES, so when unit A's span ends
exactly where unit B's first function begins, the last 8 bytes of A's span are
B's function's EH prefix.  dtk emits it into A's target obj as
`except_data_<B's entry addr>`.

Lane DA-2 found one instance by hand (BandPatchMesh's WorkVerts ctor prefix at
0x82349DB8 inside CheatProvider's pin) and flagged it unworked.  This tool exists
because it is not one instance.

THE MEASUREMENT (lane DB-4, 2026-08-02, at 9c8e4f2c)
----------------------------------------------------
1,555 of 6,016 distinct span ends (25.85%) carry the signature; 1,551 of those
are real TU boundaries (the end of one unit's span is the start of another's).

CONDITIONED NULL -- an `except_data` row at (span_end + delta):

    delta  -24 -20 -16 -12   -8   -4   +0   +4   +8  +12  +16
    count    1   1   3   1 1555    1  158   90   23    6   43

-8 is the ONLY spike.  Against the adjacent-offset null (0.02-0.05%) that is a
~500-1000x enrichment, so the class is real and systematic, not coincidence.
(The mild +0/+4 bumps are ordinary EH data that happens to start at a boundary.)

*** ADJUDICATED: DO NOT FIX.  Δ0 IS A FLOOR AS WELL AS A CEILING. ***
The tempting reading is "wrong bytes in the wrong obj, fix it for correctness
even at Δ0".  Measured, there is nothing to fix on any channel:

  * NOT IN THE DENOMINATOR.  `total_code` is EXACTLY the sum of listed function
    sizes -- whole binary 10,688,672 == 10,688,672, CheatProvider 7,512 == 7,512.
    The 8-byte prefixes have no function row, so they are outside total_code.
  * NOT IN THE NUMERATOR.  Only 31 of the 1,555 straddles have a duplicate
    phantom `fn_<end-8>` row (all size 8) -- DA-2's feared "unjoined
    fn_82349DB8" is one of them -- and **0 of those 31 appear in report.json as
    scored rows at all**.
    ⚠ That zero is a JOIN result, so it was controlled before being believed
    (an empty intersection reads exactly like a finding):
      - POSITIVE CONTROL: 41,149 of 69,189 symbols.txt `fn_` names DO appear in
        report.json (59.5%) => the join is live, not collapsed.
      - CONDITIONED CONTROL: size-8 `fn_` rows in general ARE scored, 479/1,446
        = 33.13% => the zero is not an artifact of being 8 bytes. At that base
        rate ~10 of 31 would be expected by chance; 0 were observed.
  * THE OWNING FUNCTION IS ALREADY PERFECT.  `??0WorkVerts@BandPatchMesh@@QAA@
    PAVRndMesh@@ABVVector2@@@Z` (460 B at 0x82349DC0) reads fuzzy 100.0 /
    mpn 100.0, so objdiff never needed the prefix to pair the body.
  * NOTHING IS HIDDEN.  dtk already names the object `except_data_82349DC0`,
    correctly attributing it to the function at 0x82349DC0.  The "a metric that
    hides real bugs is worse than a lower metric" rule does not bite: no
    wrong-code claim is being concealed, only a byte of EH metadata filed under
    the neighbouring TU.

And the fix is NOT local.  Repairing 0x82349DB8 alone would make the tree LESS
uniform (1 boundary of 1,555 treated differently).  The blanket repair -- shrink
1,555 span ends by 8 B -- is a large splits edit with a bad risk profile:
  * splits span changes are the documented trigger for symbols.txt fixpoint
    drift, so it would have to be landed together with a
    tools/symbols_fixpoint_guard.py re-run;
  * `.pdata` is DERIVED OUTPUT and would be wholly re-derived;
  * a span that drains a unit's last `.text` block must have the unit's whole
    splits.txt entry deleted in the same edit or report.json hard-fails with
    "Invalid COFF/PE section headers".
Large blast radius, measured benefit exactly zero.  Declined deliberately.

WHAT WOULD REOPEN IT
--------------------
A demonstrated case where the straddle makes OUR compiled obj's last function
extent over-run into the prefix.  That IS a real defect -- it is the separate
"+193 EH PREFIX extent artifact" class -- but it concerns OUR side's function
extents and is addressed by a post-compile patcher, NOT by moving splits pins.
⚠ And note the control trap recorded for that class: patching an obj IN THE
BUILD TREE is VACUOUS, because ninja recompiles and wipes it.

USAGE
-----
    python3 tools/eh_prefix_straddle_census.py            # counts + null
    python3 tools/eh_prefix_straddle_census.py --list     # the straddle rows
    python3 tools/eh_prefix_straddle_census.py --check-null  # fail if the
        signature stops being enriched (i.e. this census went vacuous)
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

DEFAULT_TITLE = "45410914"
NULL_DELTAS = (-24, -20, -16, -12, -8, -4, 0, 4, 8, 12, 16)
MIN_ENRICHMENT = 20.0   # -8 must beat the adjacent-offset mean by at least this

ROW = re.compile(
    r"^(\S+)\s*=\s*\.text:0x([0-9A-Fa-f]{8});\s*//\s*type:(\w+)"
    r"(?:\s+size:0x([0-9A-Fa-f]+))?")


def repo_root():
    here = Path(__file__).resolve().parent
    out = subprocess.run(["git", "-C", str(here), "rev-parse", "--show-toplevel"],
                         stdout=subprocess.PIPE, text=True, check=True).stdout
    return Path(out.strip())


def load(root, title):
    spans, cur = [], None
    sp = root / f"config/{title}/splits.txt"
    for line in sp.read_text(errors="replace").splitlines():
        if line and not line[0].isspace() and line.strip().endswith(":"):
            cur = line.strip()[:-1]
            continue
        m = re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", line)
        if m and cur:
            spans.append((int(m.group(1), 16), int(m.group(2), 16), cur))
    exc, fns = {}, {}
    sy = root / f"config/{title}/symbols.txt"
    for line in sy.read_text(errors="replace").splitlines():
        m = ROW.match(line.strip())
        if not m:
            continue
        name, addr, typ, size = m.group(1), int(m.group(2), 16), m.group(3), m.group(4)
        if name.startswith("except_data_"):
            exc[addr] = name
        elif typ == "function":
            fns[addr] = (name, int(size, 16) if size else 0)
    if not spans or not exc or not fns:
        raise SystemExit(
            f"REFUSING: parsed spans={len(spans)} except_data={len(exc)} "
            f"functions={len(fns)} -- an empty side would make every count 0 and "
            "the census would report a reassuring, meaningless zero.")
    return spans, exc, fns


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--version", default=DEFAULT_TITLE)
    ap.add_argument("--list", action="store_true", help="print each straddle")
    ap.add_argument("--check-null", action="store_true",
                    help="exit 1 unless -8 is still strongly enriched over the "
                         "adjacent offsets (guards against this census silently "
                         "becoming vacuous)")
    a = ap.parse_args()

    root = repo_root()
    spans, exc, fns = load(root, a.version)
    ends = {e for _, e, _ in spans}
    starts = {s for s, _, _ in spans}
    print(f"spans={len(spans)}  distinct span ends={len(ends)}  "
          f"except_data rows={len(exc)}  function rows={len(fns)}")

    counts = {}
    for d in NULL_DELTAS:
        counts[d] = sum(1 for e in ends if (e + d) in exc)
    print("\nCONDITIONED NULL -- except_data present at (span_end + delta):")
    for d in NULL_DELTAS:
        n = counts[d]
        star = "   <== EH-prefix straddle signature" if d == -8 else ""
        print(f"   delta {d:+4d}: {n:5d}  ({100.0*n/len(ends):5.2f}%){star}")

    adj = [counts[d] for d in NULL_DELTAS if d not in (-8, 0)]
    mean_adj = sum(adj) / len(adj)
    enrich = counts[-8] / mean_adj if mean_adj else float("inf")
    print(f"\n   -8 enrichment over adjacent-offset mean ({mean_adj:.2f}): {enrich:.1f}x")

    strad = sorted(e for e in ends if (e - 8) in exc and e in fns)
    real = [e for e in strad if e in starts]
    dup = [e for e in strad if (e - 8) in fns]
    print(f"\nSTRADDLES (except_data at end-8 AND a function begins at end): {len(strad)}")
    print(f"   boundary is also another unit's span START: {len(real)}")
    print(f"   with a duplicate phantom fn_ row at end-8:  {len(dup)}")
    print("\nADJUDICATED 2026-08-02 (lane DB-4): DO NOT FIX. Metric-inert in BOTH "
          "directions -- the prefixes are outside total_code (which is exactly "
          "sum(listed function sizes)) and none of the phantom rows is scored. "
          "See this file's docstring for the full reasoning and for what would "
          "reopen it.")

    if a.list:
        print("\n  span_end   except_data at end-8                    function at end")
        for e in strad:
            fn = fns[e][0]
            print(f"  0x{e:08X}  {exc[e-8]:38s} {fn}")

    if a.check_null:
        if enrich < MIN_ENRICHMENT:
            print(f"\nFAIL: -8 enrichment {enrich:.1f}x < {MIN_ENRICHMENT}x. Either the "
                  "straddle class is gone (someone re-cut the pins) or this census "
                  "has become vacuous. Do not trust its counts until resolved.")
            return 1
        print(f"\nPASS: -8 still enriched {enrich:.1f}x over adjacent offsets.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
