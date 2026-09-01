#!/usr/bin/env python3
"""header_offset_adjudicate.py -- turn header-audit ROWS into an adjudicated
verdict against the target binary.

WHY THIS IS A SEPARATE TOOL
---------------------------
``header_offset_audit*.py`` compares our ``// 0xHEX`` comments against **our own
compiler**.  Every row it emits means "the comment disagrees with our layout" --
class A.  It has **no target-binary input at all**, so it cannot distinguish

    A  wrong comment, right layout   -> cosmetic + struct_db, metric-neutral
    B  wrong LAYOUT                  -> a real bug; moves the ruler

and its own docstring says so ("Never let a B masquerade as an A").  This script
supplies the missing input: for every flagged class it asks report.json what the
binary has to say, and files the class into one of three buckets.

THE THREE BUCKETS, AND THE TWO TRAPS THAT DEFINE THEM
-----------------------------------------------------
``WITNESSED_LAYOUT_OK``  the class has at least one function whose base object
    reproduces the target's bytes with ZERO mismatched instructions.  Our layout
    IS the target's layout there, so a disagreeing comment is drift.
    ⚠ TRAP 1: a DISPLAYED 100.0 is not byte-identity.  Only a zero mismatch
    COUNT is; ``match_percent_normalized`` is an exact f32 and 100.0 there is
    the strong reading, but the count is what this script keys on where it can.
    ⚠ TRAP 2 (the one that nearly cost DC3 a wrong fix): "the target uses the
    COMMENTED offset" is NOT evidence on such a function.  Base and target are
    the same bytes there, so an instruction bearing the commented offset is
    addressing some OTHER object.  Six DC3 rows read that way; all six were
    artifacts.

``NEEDS_INSTRUCTION_WITNESS``  the class has compiled functions but none is
    perfect.  Adjudicating requires an actual instruction: a store/load at an
    offset only one of the two layouts can produce, or a destructor destroying
    members at exact offsets.  ⚠ ``sizeof`` agreement is NOT enough -- on DC3's
    PitchDetector, deleting a DIFFERENT member also made the offsets line up and
    dropped ``SpectralAnalysis::Analyze`` from 71.85 to 64.10.

``UNWITNESSED``  no compiled function of this class exists in report.json, so
    the binary has nothing to say.  Correct the comment to compiler truth if you
    like, but do NOT count it as confirmed.

USAGE
    python3 tools/header_offset_adjudicate.py --audit out.json \
        --report build/45410914/report.json [--json triage.json]
"""
import argparse
import collections
import json
import re
import sys


def classes_in_symbol(sym):
    """Every class name appearing in an MSVC-mangled name's qualifier chain.

    `?Foo@Bar@Baz@@...` -> {'Bar', 'Baz'}; the leading component is the member.
    Good enough to answer "does this class have compiled code", which is all we
    need -- a false POSITIVE here would put a class in a stronger bucket, so the
    matching is deliberately anchored on the qualifier chain only.
    """
    if not sym.startswith("?"):
        return set()
    body = sym[1:]
    at = body.find("@@")
    chain = body[:at] if at >= 0 else body
    parts = [p for p in chain.split("@") if p]
    return set(parts[1:])          # drop the member/function name itself


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--audit", required=True, help="header_offset_audit*.py --json output")
    ap.add_argument("--report", required=True, help="report.json")
    ap.add_argument("--json", help="write the triage here")
    args = ap.parse_args()

    audit = json.load(open(args.audit))
    findings = audit["findings"]
    flagged = {}                                   # class -> (header, rows)
    for hdr, per in findings.items():
        for cls, rows in per.items():
            flagged[cls.split("::")[-1]] = (hdr, rows)

    rep = json.load(open(args.report))
    # class -> list of (unit, symbol, pct, size)
    by_cls = collections.defaultdict(list)
    for u in rep["units"]:
        for f in (u.get("functions") or []):
            sym = f["name"]
            pct = f.get("match_percent_normalized")
            size = int(f.get("size") or 0)
            for c in classes_in_symbol(sym) & flagged.keys():
                by_cls[c].append((u["name"], sym, pct, size))

    # ⚠ A PERFECT *TRIVIAL* FUNCTION WITNESSES NOTHING.  `?GetX@C@@QBAHXZ`
    # compiling to `lwz r3,0x8(r3); blr` is 100% in a thousand classes and is
    # very likely ICF-folded with a hundred unrelated ones; it says nothing
    # about the class's layout beyond the one offset it happens to load.  The
    # witness therefore has to be a function with a real body.  MIN_WITNESS_SIZE
    # is deliberately conservative (0x40 = 16 instructions).
    MIN_WITNESS_SIZE = 0x40

    buckets = collections.defaultdict(list)
    for cls, (hdr, rows) in sorted(flagged.items()):
        fns = by_cls.get(cls) or []
        if not fns:
            buckets["UNWITNESSED"].append((cls, hdr, len(rows), 0, None))
            continue
        perfect = [f for f in fns
                   if f[2] is not None and f[2] >= 100.0 and f[3] >= MIN_WITNESS_SIZE]
        best = max((f[2] for f in fns if f[2] is not None), default=None)
        if perfect:
            buckets["WITNESSED_LAYOUT_OK"].append((cls, hdr, len(rows), len(fns), best))
        else:
            buckets["NEEDS_INSTRUCTION_WITNESS"].append(
                (cls, hdr, len(rows), len(fns), best))

    print("=" * 72)
    print(f"flagged classes : {len(flagged)}")
    print(f"flagged rows    : {sum(len(r) for _h, r in flagged.values())}")
    for b in ("WITNESSED_LAYOUT_OK", "NEEDS_INSTRUCTION_WITNESS", "UNWITNESSED"):
        n = len(buckets[b])
        rows = sum(x[2] for x in buckets[b])
        print(f"  {b:<28} classes={n:<5} rows={rows}")
    print("=" * 72)
    print("\n⚠ WITNESSED_LAYOUT_OK is a claim about the LAYOUT, not about any")
    print("  individual comment: it says the binary agrees with our compiler, so")
    print("  a disagreeing comment is drift.  It is NOT a licence to read the")
    print("  commented offset out of the target -- see TRAP 2 in the docstring.")

    for b in ("NEEDS_INSTRUCTION_WITNESS", "UNWITNESSED"):
        print(f"\n--- {b} ---")
        for cls, hdr, nrows, nfns, best in sorted(buckets[b], key=lambda x: -x[2])[:60]:
            bs = "n/a" if best is None else f"{best:.4f}"
            print(f"  {cls:<34} rows={nrows:<4} fns={nfns:<4} best={bs:<9} {hdr}")

    if args.json:
        with open(args.json, "w") as fh:
            json.dump({b: [{"class": c, "header": h, "rows": r, "functions": n,
                            "best_pct": p} for c, h, r, n, p in v]
                       for b, v in buckets.items()}, fh, indent=1)
        print(f"\nwrote {args.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
