#!/usr/bin/env python3
"""W23-FRAMESWEEP: collectability + cross-instrument check for frame candidates.

WHY THIS RUNS BEFORE ANY PORTING
================================
`matched_code` keys on `fuzzy == 100` and is ALL-OR-NOTHING per row.  A row can
therefore be a perfect body-port target and still pay ZERO, if what remains
after the body is right is a relocation-NAME charge against an ICF fold-survivor
name where OUR SOURCE IS ALREADY CORRECT.  No source change closes those.

W22 sized `?Poll@VocalPlayer@@` as exactly that: 3,388 B, 10 real name charges,
8 of them fold-survivors (`vector<Dep*>::reserve` where we correctly spell
`vector<VocalPart*>`).  Knowing a candidate is uncollectable is worth as much as
porting one that is -- it stops a lane spending its budget on bytes that cannot
move.

THE CHARGE RULE (lane W19, and it is NOT the naive one)
=======================================================
objdiff FORGIVES a placeholder target name (`fn_`/`lbl_`/`jumptable_`/...), so
such a site never reaches `diff_arg` at all.  Therefore:

    arg:{Symbol}            alone  -> a REAL, non-forgiven name charge
    arg:{Register, Symbol}         -> charged by the REGISTER; symbol incidental

Counting every instruction whose Symbol args differ read **138 name charges** on
a row whose true count is **ZERO**.  That is the difference between "walled by
construction" and "the largest collectable row in the unit".

THE FRAME CROSS-CHECK
=====================
`--verify-frame` is a control on tools/w23_frame_scan.py itself, using a
different instrument (objdiff's own graded diff) than the detector (raw COFF
prologue decode).  It looks for a charged site whose two immediates differ by
exactly the detected frame delta.  A candidate the detector claims but objdiff
shows no frame-delta site for is a DETECTOR FALSE POSITIVE and is labelled
NO-FRAME-SITE rather than quietly ranked.
"""
import argparse
import collections
import json
import os
import sys
from pathlib import Path

ROOT = Path(os.environ.get("W23_ROOT", Path(__file__).resolve().parent.parent))
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts" / "analysis"))
from cascade_price import ruler_args, run_diff, is_placeholder  # noqa: E402


def classify(diff):
    """Per-row charged-site profile under the graded ruler."""
    p = dict(hard=0, name=0, reg=0, imm=0, branch=0, other=0)
    pairs = collections.Counter()
    imm_deltas = collections.Counter()
    for ins in diff.get("instructions", []):
        mt = ins.get("match_type")
        if mt == "equal":
            continue
        if mt != "diff_arg":
            p["hard"] += 1
            continue
        t = (ins.get("target") or {}).get("typed_args") or []
        b = (ins.get("base") or {}).get("typed_args") or []
        kinds, sym, ivals = set(), None, None
        for x, y in zip(t, b):
            xv, yv = (x or {}).get("value"), (y or {}).get("value")
            if xv == yv:
                continue
            k = (x or {}).get("type")
            kinds.add(k)
            if k == "Symbol":
                sym = (str(xv), str(yv))
            if k in ("Signed", "Unsigned", "Opaque", "Immediate"):
                try:
                    ivals = (int(xv), int(yv))
                except (TypeError, ValueError):
                    pass
        if kinds == {"Symbol"}:
            # W19's rule: a BARE Symbol arg is the only real name charge.
            if sym and not (is_placeholder(sym[0]) and is_placeholder(sym[1])):
                p["name"] += 1
                pairs[sym] += 1
            else:
                p["other"] += 1
        elif "Register" in kinds:
            p["reg"] += 1
        elif "BranchDest" in kinds:
            p["branch"] += 1
        elif ivals:
            p["imm"] += 1
            imm_deltas[ivals[0] - ivals[1]] += 1
        else:
            p["other"] += 1
    return p, pairs, imm_deltas


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", default=str(ROOT))
    ap.add_argument("--frames", default="/home/free/tmp/w23_frames.json")
    ap.add_argument("--min-fuzzy", type=float, default=80.0)
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument("--json-out", default=None)
    args = ap.parse_args()

    root = Path(args.project)
    rargs, rlabel = ruler_args(str(root), "graded")
    print("ruler: %s" % rlabel)

    rows = json.load(open(args.frames))
    rows = [r for r in rows if (r["fuzzy"] or 0) >= args.min_fuzzy]
    rows.sort(key=lambda r: -r["prize"])
    rows = rows[:args.top]

    out = []
    print("\n%9s %6s %5s %4s %5s %5s %5s %5s  %-14s %s"
          % ("PRIZE", "FUZZY", "MPN", "FCL", "hard", "NAME", "reg", "imm",
             "VERDICT", "SYMBOL"))
    for r in rows:
        diff, err = run_diff(str(root), r["symbol"], r["unit"], rargs,
                             cache_dir="/home/free/tmp/w23diff")
        if diff is None:
            r["verdict"] = "DIFF-FAIL"
            r["err"] = err
            out.append(r)
            print("%9d %6.2f %5s %4d %5s %5s %5s %5s  %-14s %s"
                  % (r["prize"], r["fuzzy"], "-", r["funclets"], "-", "-", "-",
                     "-", "DIFF-FAIL", r["symbol"][:56]))
            continue
        p, pairs, imm_deltas = classify(diff)
        delta = r.get("delta")
        # Cross-instrument control on the detector: does objdiff independently
        # show a charged site whose immediates differ by the detected delta?
        frame_site = bool(delta) and (imm_deltas.get(delta, 0) > 0)
        r.update(p)
        r["name_pairs"] = [{"retail": a, "ours": b, "n": n}
                           for (a, b), n in pairs.most_common()]
        r["frame_site_seen"] = frame_site
        r["imm_deltas"] = {str(k): v for k, v in imm_deltas.most_common(6)}

        if (r["fuzzy"] or 0) >= 100.0:
            v = "ALREADY"
        elif not frame_site:
            v = "NO-FRAME-SITE"
        elif p["name"] == 0:
            v = "COLLECTABLE"
        else:
            v = "NAME-BLOCKED"
        r["verdict"] = v
        out.append(r)
        print("%9d %6.2f %5.1f %4d %5d %5d %5d %5d  %-14s %s"
              % (r["prize"], r["fuzzy"], r["mpn"] or 0, r["funclets"],
                 p["hard"], p["name"], p["reg"], p["imm"], v, r["symbol"][:56]))

    print()
    agg = collections.Counter(r["verdict"] for r in out)
    for k in sorted(agg):
        b = sum(r["prize"] for r in out if r["verdict"] == k)
        print("  %-14s %3d rows  %8d B" % (k, agg[k], b))

    if args.json_out:
        json.dump(out, open(args.json_out, "w"), indent=1)
        print("\nwrote -> %s" % args.json_out)


if __name__ == "__main__":
    main()
