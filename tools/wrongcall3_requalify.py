#!/usr/bin/env python3
"""wrongcall3_requalify.py -- re-qualify NOGROUP-1's 313 "wrong-callee source defects".

Lane WRONGCALL-3 (2026-08-14).  NOGROUP-1 classified a pair (S=retail callee,
F=our callee) as a SOURCE_DEFECT when

    T_S  retail bytes at addr(S) == our body for S      (map corroborated)
    !T_F retail bytes at addr(S) != our body for F      (we call something else)

Both tests run through icf_alias_build.relocs_agree, which masks relocated fields
but COMPARES RELOCATION TARGET NAMES.  Two consequences the census did not apply on
this branch:

  A. FOLD-SHAPED.  For two template instantiations over layout-compatible types the
     emitted code is identical while the callees are per-instantiation symbols
     (_Rb_tree<CRC,...> vs _Rb_tree<int,...>), so !T_F holds BY CONSTRUCTION whether
     or not /OPT:ICF folded them.  Proof case: Hmx::CRC (utl/CRC.h) is a lone `int
     mCRC` whose operator< is `mCRC < c.mCRC`, i.e. bit-identical to less<int>, so
     map<CRC,float> and map<int,float> cannot differ in a single instruction.  Where
     the MASKED bodies are byte-equal the pair is fold-vs-wrong-callee UNDECIDABLE --
     the census's own UNDECIDABLE_relocs category -- not a source defect.

  B. VACUOUS T_S.  nogroup_census computes `vac = vacuous(rt) or vacuous(ob)` for
     every record but consults it ONLY in the fold branch, where it took the class
     23 -> 4.  On this branch a sub-floor retail body (<16 B, or >50% relocated)
     makes the map-corroboration leg worthless -- e.g. `stb r4,0x40(r3); blr`
     matches ANY class with a byte at 0x40.
     ⚠ This weakens the MAP leg only.  Vacuity manufactures false EQUALITY, and the
     load-bearing evidence here is a DIFFERENCE, so it is not by itself a refutation.

⛔ The actionable warning this file exists to carry: a fold-shaped row must NOT be
"fixed" by changing our container/type to retail's spelling.  Our map<int,float> for
SongData::mRangeShifts is CORRECT (AddRangeShift(int,float) indexes by int); editing
it to map<CRC,float> to chase 8,212 B would break working code to satisfy a fold.
Nor may an alias be installed -- that forgives the genuinely-wrong-callee members.

Read-only apart from rewriting the queue TSV with three extra columns.
"""
import sys, glob, json, argparse, collections

ROOT = "/home/free/tmp/wt-wrongcall3"
sys.path.insert(0, ROOT + "/tools")
from icf_alias_build import collect, vacuous  # noqa: E402

QUEUE = ROOT + "/docs/decomp/nogroup-wrong-callee-queue-NOGROUP1.tsv"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--verdicts", default="/home/free/tmp/wc3_verdicts.json")
    ap.add_argument("--out", default=QUEUE)
    a = ap.parse_args()

    tgt = collect(sorted(glob.glob(ROOT + "/build/45410914/obj/**/*.obj", recursive=True)), "t")
    ours = collect(sorted(glob.glob(ROOT + "/build/45410914/src/**/*.obj", recursive=True)), "o")
    V = json.load(open(a.verdicts))
    rec = {(r["retail"], r["ours"]): r for r in V}

    lines = open(QUEUE).read().splitlines()
    head, body = lines[0], lines[1:]
    out = [head + "\twc3_shape\twc3_TS_vacuous\twc3_action"]
    tally, bytes_by = collections.Counter(), collections.Counter()
    for ln in body:
        p = ln.split("\t")
        if len(p) < 6:
            continue
        S, F = p[4], p[5]
        rt, ob = tgt.get(S), ours.get(F)
        if rt is None or ob is None:
            shape = "no_body"
        elif rt[2] != ob[2]:
            shape = "SIZE_DIFFERS"
        elif rt[0] == ob[0]:
            shape = "MASKED_IDENTICAL"
        else:
            shape = "BODY_DIFFERS"
        vac = "yes" if (rt is not None and vacuous(rt)) else "no"
        if shape == "MASKED_IDENTICAL":
            act = "DO_NOT_FIX_reclassify_UNDECIDABLE_relocs"
        elif vac == "yes":
            act = "ADJUDICATE_map_leg_is_vacuous"
        else:
            act = "wrong-callee reading STANDS (verify map on retail bytes)"
        tally[shape] += 1
        bytes_by[shape] += int(p[0])
        out.append(ln + "\t%s\t%s\t%s" % (shape, vac, act))

    open(a.out, "w").write("\n".join(out) + "\n")
    tot = sum(bytes_by.values())
    print("re-qualified %d rows -> %s\n" % (len(out) - 1, a.out))
    print("%-18s %5s %10s %8s" % ("shape", "rows", "solo_B", "share"))
    for k, v in tally.most_common():
        print("%-18s %5d %10d %7.1f%%" % (k, v, bytes_by[k], 100.0 * bytes_by[k] / max(tot, 1)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
