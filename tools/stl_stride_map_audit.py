#!/usr/bin/env python3
"""Adjudicate the `SHIFT_OR_MASK` rows of tools/mpn_fuzzy_gap_census.py against
RETAIL BYTES -- i.e. decide whether a differing PPC shift amount is a source
defect or a MAP MISPAIR between sibling STL instantiations.

WHY THIS EXISTS (read before re-deriving the drained theory)
------------------------------------------------------------
The census surfaces functions counted in `matched_functions` (mpn == 100) whose
per-item `fuzzy_match_percent` is < 100 because a PPC shift/mask field differs.
Those fields decode to `InstructionArgValue::Opaque`, so `is_immediate` is false
and the penalty is normalized away.

★ THE TEMPTING THEORY IS WRONG AND HAS BEEN KILLED TWICE.
"differing shift ⇒ log2(sizeof(T)) mismatch ⇒ our struct size is wrong" was
tested by lane BZ-3 (2026-07-30) and again, independently, by lane DD-1
(2026-08-02) on the then-current 14-row population. **Neither found a single
genuine sizeof defect.** The refutation is at the language level and needs no
measurement: a `vector<T*>` cannot have a 2-byte stride, a `pair<T*,float>`
cannot have a 64-byte stride, and a `DataArrayPtr` holding one pointer cannot
have an 8-byte stride. Our sizes are right; the ADDRESS is wrong.

⇒ The shift amount is not a size oracle. It is a **SIBLING DISCRIMINATOR** --
the one field that distinguishes byte-identical STL COMDAT twins, and exactly
the field objdiff folds away. So byte-similarity naming cannot tell twins apart
and picks arbitrarily. Most flagged rows are MAP MISPAIRS.

★ TWO ICF REGIMES -- DO NOT TREAT THE FAMILIES ALIKE (DD-1, retail-verified)
Whether a repoint is even *possible* depends on whether the family was folded:

  * `~vector<T>` / `~_Vector_base<T>` (36 B, tail-call free). Its ONLY
    relocation is `MemOrPoolFreeSTL`, identical for every T. So all same-stride
    instantiations are reloc-identical and MSVC `/OPT:ICF` folds them. Measured
    in retail: **exactly 7 surviving bodies, one per stride 2,4,...,128.**
    ⇒ hundreds of names contend for 7 addresses. A cross-stride assignment is
    provably wrong, but the correct address is normally ALREADY CLAIMED, so the
    row is INJECTIVITY-BLOCKED and picking a name buys FALSE credit. Do not ship.

  * `_M_fill_insert` (108 B), `_M_allocate_and_copy` (100 B), the 132 B
    `~vector<T>` with a destroy loop. These carry PER-T relocations (destroy /
    copy helpers), so they are NOT folded: 29, 72 and 29 distinct retail bodies
    respectively. ⇒ here a repoint CAN be correct, and a stride mismatch is a
    genuine, fixable map defect.

  * The 44 B "free-then-rethrow" EH funclet is a third case: 31 retail
    instances collapsing to 12 masked signatures (multiplicity up to 8), and
    funclets are not independent COMDATs so ICF cannot fold them individually.
    These are ANONYMOUS on the target side and pair via the objdiff fork's
    `pair_funclets_by_bytes`, NOT via the map -- so they are not repointable at
    all and are not this tool's business.

WHAT THIS TOOL DOES
    For each shape, scans retail `.text` for every instance with the shift
    field WILDCARDED, reports the stride histogram, and for a queried row lists
    the same-stride candidates with their claim status in
    scripts/target_symbol_map.json. That is the evidence a map lane needs:
    "this row is cross-stride ⇒ wrong" plus "here is the candidate set".

⚠ LIMITS -- STATED SO THEY ARE NOT OVERREAD
  * "exactly one UNCLAIMED same-stride candidate" is NOT proof of identity.
    The other same-stride candidates are claimed by rows that may themselves be
    wrong. Uniqueness among unclaimed candidates is a HYPOTHESIS, not evidence.
  * Spatial/TU containment is NOT usable as corroboration for the fragmented
    homing-pinned units these rows live in: their splits.txt entries are often a
    single-function span placed *because* the map said so (TexBlender.cpp's
    entire pinned .text is 36 B = this one function). That argument is circular.
    It is also void for "sw2 scatter-include" units (e.g. default/ByteGrinder
    pulls in hamobj/HamBattleData.cpp), where the unit name carries no TU identity.
  * A differing constant is NOT automatically a differing computation. DD-1
    found `??4Target@BandCamShot@@` emits `rlwimi rD,rS,0,24,24` in retail vs
    `rlwimi rS,rD,0,25,31` in ours -- COMPLEMENTARY MASKS that were verified
    equal over all 65,536 byte pairs. Prove semantics before calling a defect.

ANTI-VACUITY GUARD (BZ-3): every scan asserts >= 4 fixed compared words AND
>= 50% of the body fixed, else it refuses. Without it a short body whose
wildcarded words cover most of it "matches" everything.

Read-only. Touches no build input.
"""
import argparse
import collections
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEXT_VA, TEXT_RAW, TEXT_SZ = 0x82270000, 0x264E00, 0x9DCE3C


def load_text(exe: Path):
    d = exe.read_bytes()
    t = d[TEXT_RAW:TEXT_RAW + TEXT_SZ]
    n = len(t) // 4
    return struct.unpack('>%dI' % n, t[:n * 4]), n


def shift_of(w):
    """Return (kind, SH) for srawi / rlwinm, else None."""
    op = w >> 26
    if op == 31 and ((w >> 1) & 0x3FF) == 824:
        return ('srawi', (w >> 11) & 0x1F)
    if op == 21:
        return ('rlwinm', (w >> 11) & 0x1F)
    return None


def is_branch(w):
    return (w >> 26) in (16, 18)


def scan(W, n, pattern, wild):
    """All retail offsets whose words equal `pattern` outside `wild`."""
    fixed = [i for i in range(len(pattern)) if i not in wild]
    if len(fixed) < 4:
        sys.exit("REFUSING (anti-vacuity): only %d fixed words, need >= 4" % len(fixed))
    if len(fixed) < 0.5 * len(pattern):
        sys.exit("REFUSING (anti-vacuity): %d/%d fixed words < 50%% of body"
                 % (len(fixed), len(pattern)))
    d0, a0 = fixed[0], pattern[fixed[0]]
    out = []
    for i in range(n - len(pattern)):
        if W[i + d0] != a0:
            continue
        if all(W[i + j] == pattern[j] for j in fixed):
            out.append((TEXT_VA + i * 4, W[i:i + len(pattern)]))
    return out, len(fixed), len(pattern)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=str(ROOT))
    ap.add_argument("--addr", required=True,
                    help="retail VA of the flagged row (from target_symbol_map.json)")
    ap.add_argument("--size", type=int, required=True, help="function size in bytes")
    ap.add_argument("--shift-index", type=int, action="append", required=True,
                    help="word index of a shift-bearing instruction (repeatable)")
    ap.add_argument("--our-stride", type=int, default=None,
                    help="log2(sizeof) our build emits; enables candidate listing")
    args = ap.parse_args()

    root = Path(args.root)
    W, n = load_text(root / "orig" / "45410914" / "band.exe")
    amap = json.loads((root / "scripts" / "target_symbol_map.json").read_text())

    va = int(args.addr, 16)
    base = (va - TEXT_VA) // 4
    pat = list(W[base:base + args.size // 4])
    wild = set(args.shift_index) | {i for i, w in enumerate(pat) if is_branch(w)}
    hits, nf, nt = scan(W, n, pat, wild)

    print("anti-vacuity guard: %d/%d words fixed (%.0f%% of body) -- OK"
          % (nf, nt, 100.0 * nf / nt))
    print("family at %08x: %d retail instances of this shape" % (va, len(hits)))
    hist = collections.Counter()
    for _, hw in hits:
        s = shift_of(hw[args.shift_index[0]])
        if s:
            hist[s[1]] += 1
    print("stride histogram (element bytes -> count): %s"
          % {2 ** k: v for k, v in sorted(hist.items())})
    folded = len(hits) <= 8 and all(v == 1 for v in hist.values())
    print("ICF regime: %s" % ("FOLDED one-per-stride -> repoints are "
                              "INJECTIVITY-BLOCKED, do not ship" if folded else
                              "NOT folded -> a repoint can be correct"))

    mine = shift_of(pat[args.shift_index[0]])
    print("retail stride at this address: %d bytes" % (2 ** mine[1]))
    if args.our_stride is None:
        return 0
    print("our build emits: %d bytes" % (2 ** args.our_stride))
    if args.our_stride == mine[1]:
        print("=> strides agree; this row is not a stride mispair.")
        return 0
    print("\nsame-stride candidates (what a correct repoint would target):")
    unclaimed = []
    for hv, hw in hits:
        s = shift_of(hw[args.shift_index[0]])
        if not s or s[1] != args.our_stride:
            continue
        nm = amap.get('0x%08x' % hv)
        nm = (nm[0] if isinstance(nm, list) else nm) or '<UNCLAIMED>'
        if nm == '<UNCLAIMED>':
            unclaimed.append(hv)
        print("  %08x  %s" % (hv, nm[:100]))
    n_same = sum(1 for _, hw in hits
                 if (shift_of(hw[args.shift_index[0]]) or (0, -1))[1] == args.our_stride)
    print("\n%d same-stride candidate(s), %d unclaimed." % (n_same, len(unclaimed)))
    if not unclaimed:
        print("⚠ NO unclaimed candidate: every correct-stride address is already "
              "taken, so a repoint needs a swap/permutation, not a free slot. "
              "INJECTIVITY-BLOCKED.")
    elif len(unclaimed) == 1:
        print("⚠ exactly one unclaimed candidate is a HYPOTHESIS, not proof: the "
              "claimed same-stride rows may themselves be wrong.")
    else:
        print("⚠ %d unclaimed candidates -- AMBIGUOUS; the metric cannot adjudicate "
              "between them and picking one buys FALSE credit." % len(unclaimed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
