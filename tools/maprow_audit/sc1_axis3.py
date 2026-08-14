#!/usr/bin/env python3
"""SRCCAND-1 AXIS 3: is the charge TRUSTWORTHY, and is it ADJUDICABLE off-map?

AXIS 3a -- VICTIM BODY EQUALITY.  ``wc2_classify`` charges a slot whenever the
two sides agree on relocation COUNT, OFFSET and TYPE.  It does NOT require the
victim's body to match.  So a victim we have not matched can align its slots with
retail's by coincidence, and the pair it emits is then not "retail calls X where
we call Y at the same instruction" but two unrelated instructions at the same
offset.  That is where the ``UNRELATED`` shapes come from (``Symbol::Symbol`` vs
``String::~String``), and counting them as wrong callees manufactures work.

  BODY_EQUAL victim  -> the instruction IS the same instruction; the charge is a
                       real disagreement about which function it calls.
  BODY_DIFFERS       -> slot alignment is unattested; the pair is an artifact
                       candidate, not a defect.

AXIS 3b -- OFF-MAP ADJUDICABILITY.  A row can only be repaired if some channel
that does NOT route through target_symbol_map.json can say which callee is right.
Both screens A and B read bodies at MAPPED addresses, so the classification so
far is map-dependent end to end.  The channels that are not:

  RDATA_LITERAL   the callee family is selected by a string literal we can read
                  out of orig/45410914/band.exe (registration lists, ClassName,
                  Symbol construction).  Strongest channel in the project.
  ARITY_SIG       retail's call sequence fixes the callee's signature (the
                  ``operator>>(bool&)`` -> ``li r5,1`` witness).
  TYPE_IMPOSSIBLE the retail name is impossible for the receiver.
  NONE            no channel; the row is undecidable without new evidence.

>> ``--selfcheck`` proves the body-equality split can return BOTH values.
"""

import argparse
import collections
import glob
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "maprow_audit"))
from icf_alias_build import collect, placeholder  # noqa: E402
from sc1_characterize import shape                # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--classified", default="/home/free/tmp/srccand1_classified.json")
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--out", default="/home/free/tmp/srccand1_axis3.json")
    args = ap.parse_args()

    tgt = collect(sorted(glob.glob(str(ROOT / "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(str(ROOT / "build/45410914/src/**/*.obj"), recursive=True)), "o")

    rows = json.load(open(args.classified))
    sel = [r for r in rows if r["cls"] == "SOURCE_CAND" and r["screenA_on"] == "EQUAL"]

    # victim body equality, recomputed per charged slot
    al = json.load(open(ROOT / "scripts/symbol_aliases.json"))
    eq = {}
    for g in al["groups"]:
        grp = set([g["survivor"]] + list(g["folded"]))
        for n in grp:
            eq.setdefault(n, set()).update(grp)

    pair_victims = collections.defaultdict(set)
    for name, (mb, rel, sz) in ours.items():
        rt = tgt.get(name)
        if not rt or len(rt[1]) != len(rel):
            continue
        beq = (rt[0] == mb)
        for (ro, rn, rty), (oo, on, oty) in zip(rt[1], rel):
            if ro != oo or rty != oty or rn == on:
                continue
            if on in eq.get(rn, ()) or rn in eq.get(on, ()):
                continue
            pair_victims[(rn, on)].add((name, beq))

    res = []
    for r in sel:
        k = (r["retail_name"], r["our_name"])
        vs = pair_victims.get(k, set())
        n_eq = sum(1 for _v, b in vs if b)
        res.append(dict(r, shape=shape(*k), n_victims_total=len(vs),
                        n_victims_body_equal=n_eq,
                        body_equal=bool(n_eq),
                        victims_eq=sorted(v for v, b in vs if b)[:4]))

    if args.selfcheck:
        c = collections.Counter(r["body_equal"] for r in res)
        print("SELFCHECK -- victim-body-equality split over %d rows: %s" % (len(res), dict(c)))
        print("  CAN RETURN BOTH VALUES: %s" % (len(c) == 2))
        return 0 if len(c) == 2 else 1

    print("\n=== AXIS 3a: victim body equality over %d both-verified rows ===" % len(res))
    ok = [r for r in res if r["body_equal"]]
    no = [r for r in res if not r["body_equal"]]
    print("   BODY_EQUAL victim (real disagreement)   %4d pairs  %4d sites"
          % (len(ok), sum(r["sites"] for r in ok)))
    print("   BODY_DIFFERS only (alignment unattested)%4d pairs  %4d sites"
          % (len(no), sum(r["sites"] for r in no)))

    print("\n=== shape x body_equal ===")
    x = collections.Counter((r["shape"], r["body_equal"]) for r in res)
    shapes = sorted({r["shape"] for r in res})
    print("   %-20s %8s %8s" % ("shape", "BODY_EQ", "unattest"))
    for s in shapes:
        print("   %-20s %8d %8d" % (s, x[(s, True)], x[(s, False)]))

    ps = {(r["retail_name"], r["our_name"]) for r in res}
    for label, pop in (("ALL", res), ("BODY_EQUAL", ok)):
        pp = {(r["retail_name"], r["our_name"]) for r in pop}
        rec = {p for p in pp if (p[1], p[0]) in ps}
        print("   reciprocal within %-10s %3d / %3d = %.1f%%"
              % (label, len(rec), len(pp), 100.0 * len(rec) / max(1, len(pp))))

    json.dump(res, open(args.out, "w"), indent=1)
    print("\nwrote", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
