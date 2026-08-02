#!/usr/bin/env python3
"""Census of the STORAGE-CLASS divergence family via MSVC guard funclets.

Promoted into tools/ by lane CY-2 from lane CX-1's scratch `census2.py`
(/home/free/tmp/laneCX1/), which located the family that paid +99 matched /
+168 honest in commit d1234055.

────────────────────────────────────────────────────────────────────────────
WHAT IT DETECTS
────────────────────────────────────────────────────────────────────────────
Retail RB3 declares many Symbol/Message objects as FUNCTION-LOCAL STATICS
where our rb3-Wii-derived source references file-scope GLOBALS.  For every
function-local static, MSVC emits a 32-byte `??__F` atexit funclet whose entire
body clears that static's GUARD BIT:

    stwu r1,-0x60(r1)  ; lis r11,G@ha ; lwz r11,G@l(r11)
    rlwinm r11,r11,0,mb,me                  <-- clears exactly one bit
    lis r10,G@ha ; stw r11,G@l(r10) ; addi r1,r1,0x60 ; blr

That shape is a direct fingerprint of "retail declared a function-local static
here".  Unlike a Symbol-ctor-count diff it does NOT require the owning function
to be named, paired, or even identified.

★ THE ??__F SIGN IS INVERTED FROM THE OLD FEAR.  MSVC emits this funclet per
local static REGARDLESS of destructibility (the body only clears a guard bit).
Those funclets are TARGET functions we otherwise never emit, so each added
static tends to bring a FREE EXTRA MATCH -- 38 of CX-1's first 48 gains were
funclets.  Lane CW-4's five reverts were real but are NOT the general case.
PRICE PER SITE; ASSUME NEITHER SIGN.

────────────────────────────────────────────────────────────────────────────
HOW A FUNCLET ACTUALLY PAIRS  (measured by lane CY-2, and it is NOT by name)
────────────────────────────────────────────────────────────────────────────
objdiff pairs these through `pair_funclets_by_bytes` (objdiff-core/src/diff/
mod.rs) using `funclet_signature`, which ZEROES the 4-byte instruction word at
every relocation.  In a guard funclet the only non-reloc words are the
prologue, the `rlwinm`, and the epilogue -- so the signature collapses to

        *** WHICH GUARD BIT IS CLEARED, and nothing else ***

The guard word's identity is masked away entirely.  Consequences that matter:

  * A target funclet pairs with ANY base funclet in the SAME UNIT clearing the
    SAME BIT -- regardless of which function or guard word it came from.  The
    right unit of analysis is therefore the per-unit MULTISET OF BITS, which is
    what `--deficit` prints.  A unit can emit MORE funclets than retail overall
    and still be short at a particular bit (CharClipGroup: tgt=9, ours=11,
    deficit=1 at bit 7).
  * Over-subscription (N target funclets vs M<N base partners for one bit) is
    credited 100% by pass 2b and disclosed as `masked_equal_functions`.  Always
    quote honest = matched - masked_equal.
  * Both sides' funclets are ANONYMOUS-ish: the target side is `fn_XXXXXXXX`
    and OUR side is `__unwind$NNNNNN` (plus `$M` label markers).  So
    "anonymous ⇒ cannot pair" is FALSE.  Lane CY-2 built a classifier on that
    premise and its control refuted it: of 9,196 target guard funclets, ALL
    9,196 are anonymous, yet 8,886 match.

────────────────────────────────────────────────────────────────────────────
CALIBRATION AND KNOWN LIMITS  -- READ BEFORE BUDGETING OFF THIS TOOL
────────────────────────────────────────────────────────────────────────────
⚠ THE DEFAULT (UNMATCHED-COUNT) MODE IS A CORRECT LOCATOR AND A BAD ESTIMATOR.
  It UNDERCOUNTS PAYOFF ~5x.  Measured on lane CX-1: VocalTrackDir had 4 census
  rows and paid +20 matched; Gem had 1 row and paid +19.

  ★ LANE CY-2 FOUND THE CAUSE, AND `--deficit` CORRECTS IT.  The unmatched
  count sees only the shortfall objdiff left at 0%; it is BLIND to the
  shortfall that pass 2b already credited at a phantom 100% as
  `masked_equal`.  Tree-wide at HEAD 475a47ec:

        unmatched funclets            310
        masked_equal (phantom credit) 732
        --------------------------------------
        total guard-bit DEFICIT     1,042  across 137 units   <-- honest size

  The 1,042 is measured INDEPENDENTLY (per-unit multiset of bits, `--deficit`)
  and lands exactly on 310+732 = 1,042 -- two different instruments agreeing.  So the family is 3.4x LARGER than the
  unmatched count implies, and the honest drain is (9199-1042)/9199 = 88.7%,
  NOT the 96.6% the default mode suggests.  VocalTrackDir's deficit is 30 and
  it paid +20 -- a far better-calibrated forecast than its 4 unmatched rows.
  ⇒ RANK AND SIZE WITH `--deficit`.  Still never forecast a headline delta from
  it: fixing a site also repairs the OWNING body, which nothing here counts.
  Price with tools/ab_measure.py.

*** RULER CHANGE 2026-08-02 BROKE THE masked_equal HALF OF THAT TABLE. ***
  (lane DA-4).  The 310/732 split above was measured while
  `masked_equal_functions` disclosed pass-2b OVER-SUBSCRIPTION ONLY.  The
  objdiff fork now discloses EVERY funclet byte-signature pairing, so this
  tool's default mode reads, at HEAD f48bcad7:

        MATCHED 8,893  (of which masked_equal: 8,893)   <-- ALL of them
        UNMATCHED 303
        unmatched + masked = 9,196                      <-- the WHOLE population

  i.e. the old "honest family size" line degenerates into restating the total
  and looks like a 9x blow-up that never happened.  It is now guarded: the code
  refuses to print that sum when masked/matched > 50% and points here instead.
  *** `--deficit` IS UNAFFECTED *** -- it compares per-unit target-vs-ours bit
  MULTISETS and never consults masked_equal, which is exactly why it is the
  ruler-independent instrument.  Re-measured at f48bcad7 it gives a tree-wide
  deficit of 1,002 across the same shape of units (was 1,042 at 475a47ec; lane
  CZ-2's landings drained ~40).  See docs/decomp/RULER_CHANGE_2026-08-02.md.

⚠ Anti-vacuity: run it against units a previous batch already fixed.  If it
  does not go quiet there, it is measuring something else.

⚠ TRAP -- KEY BY UNIT NAME, NEVER BY source_path.  A single .cpp can back TWO
  objdiff units (two pinned .text spans): `AccomplishmentProgress.cpp` backs
  both `default/band3/meta_band/AccomplishmentProgress` and
  `default/AccomplishmentProgress`.  Selecting by source_path and taking [0]
  silently reads the wrong obj (measured: 6 target funclets instead of 20).

⚠ TRAP -- DO NOT filter symbol names on a leading `__`.  Our compiled objs name
  these funclets `__unwind$NNNNNN`; excluding `__` makes the BASE side read a
  uniform, entirely fictitious ZERO.

⚠ PLACEMENT IS CODEGEN-LOAD-BEARING, NOT STYLISTIC.  When you act on a row,
  inserting the static immediately before its first use is WRONG if that use is
  inside a LOOP -- it puts the guard test inside the loop while retail's is
  outside (measured regressions 87.04 -> 75.79 and 75.83 -> 60.76).  Hoist
  above the loop.

⚠ Literal NAMES are truncated at 32 chars by MSVC's mangling
  (`tour_goal_focus_player_contribut` vs retail's
  `tour_goal_focus_player_contribution_format`).  A decode artifact, NOT a
  defect -- do not "fix" those rows.

────────────────────────────────────────────────────────────────────────────
STATE AS OF 2026-08-02 (lane CY-2, HEAD 475a47ec)
────────────────────────────────────────────────────────────────────────────
  9,199 guard funclets in retail inside paired units; 8,886 matched; 310
  unmatched across 62 units; 3 carry no report.json row at all; total
  guard-bit deficit 1,042 across 137 units.
  (Lane CX-1 reported 9,196 total -- the 3 extra here are target symbols its
  filter dropped for a leading `__`.  Its message also says "8,856 matched",
  which is an arithmetic slip: 9,196 - 310 = 8,886.)

  Of the 310 unmatched: 235 sit in units that HAVE a compiled base obj
  (storage-class reachable) and 75 in 15 source-less `auto_03_*` units (pure
  identification work -- there is no source file to edit).

  The cheap `/D` lever (/DRB3_HANDLE_LOCAL_STATIC, /DRB3_SYNCPROP_LOCAL_STATIC)
  is EXHAUSTED: every residue TU that uses a gated macro already carries its
  gate.  The remainder needs per-site source statics (CX-1 batch 3 ran ~1
  match/site).  Units with a gate but ZERO uses of the gated macros are INERT
  -- that is why RetryAudioPanel measured delta-0 (it has 0 HANDLE sites).

Usage:
    tools/guard_funclet_census.py <worktree>              # tree-wide census
    tools/guard_funclet_census.py <worktree> --deficit    # per-unit bit deficit
    tools/guard_funclet_census.py <worktree> --deficit --unit <objdiff-unit-name>
"""
import argparse
import json
import os
import struct
import sys
from collections import Counter

# 32-byte guard-clearing ??__F funclet, exact encoding.
PROLOGUE = 0x9421FFA0   # stwu r1,-0x60(r1)
EPI_ADDI = 0x38210060   # addi r1,r1,0x60
EPI_BLR = 0x4E800020   # blr
OP_LIS, OP_LWZ, OP_RLWINM, OP_STW = 15, 32, 21, 36

# NOTE the absent "__": our base objs name these `__unwind$NNNNNN`.
SKIP_PREFIXES = (".", "except_data", "$", "lbl_")


def guard_bit(b):
    """Return the guard bit this 32-byte body clears, or None if not a guard funclet."""
    if len(b) != 32:
        return None
    w = struct.unpack(">8I", b)
    if w[0] != PROLOGUE or w[6] != EPI_ADDI or w[7] != EPI_BLR:
        return None
    if not ((w[1] >> 26) == OP_LIS and (w[2] >> 26) == OP_LWZ
            and (w[3] >> 26) == OP_RLWINM and (w[4] >> 26) == OP_LIS
            and (w[5] >> 26) == OP_STW):
        return None
    # `rlwinm rA,rS,0,mb,me` with mb == me+2 clears exactly bit (31-me-1).
    me = (w[3] >> 1) & 31
    return 31 - ((me + 1) % 32)


def scan(coff_cls, path):
    """-> list of (symbol_name, guard_bit) for every guard funclet in `path`."""
    try:
        c = coff_cls(path)
    except Exception:
        return None
    out, sd = [], {}
    for name, s in c.symbol_map.items():
        si = s.get("section", 0)
        if si <= 0 or name.startswith(SKIP_PREFIXES):
            continue
        if si not in sd:
            try:
                sd[si] = c.get_section_data(si - 1)
            except Exception:
                sd[si] = b""
        off = s.get("value", 0)
        bit = guard_bit(sd[si][off:off + 32])
        if bit is not None:
            out.append((name, bit))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("worktree")
    ap.add_argument("--deficit", action="store_true",
                    help="print the per-unit MULTISET-OF-BITS deficit (what to add)")
    ap.add_argument("--unit", action="append", default=[],
                    help="restrict to these objdiff unit NAMES (repeatable)")
    args = ap.parse_args()

    wt = args.worktree
    sys.path.insert(0, os.path.join(wt, "scripts"))
    from unicorn_runner.coff import COFFParser

    units = json.load(open(os.path.join(wt, "objdiff.json")))["units"]
    rep = json.load(open(os.path.join(wt, "build/45410914/report.json")))
    state = {}
    for u in rep["units"]:
        for f in u.get("functions", []):
            state[(u["name"], f["name"])] = (
                float(f.get("fuzzy_match_percent", 0.0)),
                int(f.get("size", 0)),
                bool(f.get("masked_equal")))

    oj = json.load(open(os.path.join(wt, "config/45410914/objects.json")))
    objs = {}

    def walk(o):
        if not isinstance(o, dict):
            return
        for k, v in o.items():
            if isinstance(k, str) and k.endswith(".cpp"):
                objs["src/" + k] = v
            else:
                walk(v)

    walk(oj)

    total = matched = masked = no_row = 0
    rows = []
    for u in units:
        if args.unit and u["name"] not in args.unit:
            continue
        tp = u.get("target_path")
        if not tp or not os.path.exists(os.path.join(wt, tp)):
            continue
        tgt = scan(COFFParser, os.path.join(wt, tp))
        if not tgt:
            continue
        bp = u.get("base_path")
        base = (scan(COFFParser, os.path.join(wt, bp))
                if bp and os.path.exists(os.path.join(wt, bp)) else None)

        unm, mk, absent = [], 0, 0
        for name, _bit in tgt:
            pct, _sz, me = state.get((u["name"], name), (None, None, False))
            if pct is None:
                # Present in the target obj but carrying NO report.json row at
                # all. Do NOT fold these into "unmatched" -- absent-from-report
                # is a third state, and `total - matched` silently miscounts it.
                absent += 1
                continue
            if pct >= 99.9999:
                matched += 1
                mk += int(me)
            else:
                unm.append((name, pct))
        total += len(tgt)
        masked += mk
        no_row += absent

        src = (u.get("metadata") or {}).get("source_path")
        entry = objs.get(src)
        flags = [] if isinstance(entry, str) else list((entry or {}).get("extra_cflags") or [])
        rows.append(dict(unit=u["name"], src=src, tgt=tgt, base=base,
                         unmatched=unm, masked=mk, flags=flags))

    if args.deficit:
        print(f"{'tgt':>5} {'ours':>5} {'DEF':>4}  unit / source")
        for r in sorted(rows, key=lambda r: -(sum(
                max(0, Counter(b for _, b in r["tgt"])[k]
                    - (Counter(b for _, b in r["base"])[k] if r["base"] else 0))
                for k in set(b for _, b in r["tgt"])))):
            tc = Counter(b for _, b in r["tgt"])
            bc = Counter(b for _, b in r["base"]) if r["base"] else Counter()
            defi = {k: tc[k] - bc.get(k, 0) for k in tc if tc[k] - bc.get(k, 0) > 0}
            if not defi:
                continue
            print(f"{sum(tc.values()):5d} {sum(bc.values()):5d} {sum(defi.values()):4d}  "
                  f"{r['src'] or '(NO SOURCE) ' + r['unit']}")
            print(f"        missing bits {dict(sorted(defi.items()))}")
        return

    unmatched = sum(len(r["unmatched"]) for r in rows)
    print(f"guard-clearing ??__F funclets in retail, inside paired units: {total}")
    print(f"   MATCHED   : {matched}  (of which masked_equal: {masked})")
    print(f"   UNMATCHED : {unmatched}  in "
          f"{sum(1 for r in rows if r['unmatched'])} units")
    print(f"   no report.json row (neither state): {no_row}")
    # *** RULER GUARD (lane DA-4, 2026-08-02). ***
    # `unmatched + masked` was a valid family-size estimate ONLY while
    # masked_equal_functions disclosed pass-2b OVER-SUBSCRIPTION alone (it read
    # 310 + 732 = 1,042 and cross-checked exactly against --deficit).  Since the
    # 2026-08-02 disclosure flip, masked_equal fires on EVERY funclet
    # byte-signature pairing, so the sum silently collapses to the whole
    # population (303 + 8,893 = 9,196) and reads like a catastrophe that did not
    # happen.  Refuse to print it rather than print a number that means nothing.
    share = (masked / matched) if matched else 0.0
    if share > 0.5:
        print(f"   => masked_equal covers {share:6.1%} of MATCHED -- this is the "
              f"POST-2026-08-02 ruler, where masked_equal discloses ALL funclet\n"
              f"      pairings, not just over-subscription.  `unmatched + masked` "
              f"is NOT a family size on this ruler (it would read\n"
              f"      {unmatched + masked}, i.e. essentially the whole population "
              f"of {total}).  *** USE --deficit -- it is ruler-independent. ***\n"
              f"      See docs/decomp/RULER_CHANGE_2026-08-02.md.")
    else:
        print(f"   => honest family size = unmatched + masked = {unmatched + masked} "
              f"(cross-check with --deficit)")
    print()
    print("  unmatched/total  gates[HS]  source")
    for r in sorted(rows, key=lambda r: -len(r["unmatched"])):
        if not r["unmatched"]:
            continue
        h = "H" if any("HANDLE_LOCAL" in f for f in r["flags"]) else "-"
        s = "S" if any("SYNCPROP_LOCAL" in f for f in r["flags"]) else "-"
        print(f"  {len(r['unmatched']):5d}/{len(r['tgt']):-4d}      [{h}{s}]     "
              f"{r['src'] or '(NO SOURCE) ' + r['unit']}")


if __name__ == "__main__":
    main()
