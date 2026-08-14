#!/usr/bin/env python3
"""SRCCAND-1 AXIS 5: the ADJUSTOR-THUNK INVARIANT.

THE INVARIANT.  An MSVC adjustor thunk -- ``vtordisp{a,b}``, ``adjustor{n}``, and
the ``$4``/``W``/``$R`` mangled forms -- exists for exactly one purpose: fix up
``this`` and transfer to THE SAME METHOD on the same class.  A thunk named
``[thunk] RndLine::Load`vtordisp{-4,0}'`` calls ``RndLine::Load``.  Not Save, not
SetType.  This is a property of the code generator, so it holds with no reference
to target_symbol_map.json, no retail bytes and no oracle -- which is what makes it
usable where screens A and B (both of which read bodies at MAPPED addresses) are
exhausted.

WHAT A VIOLATION MEANS.  If our thunk's slot names ``M`` (correct by the
invariant) and retail's names ``N != M``, then one of THREE map assignments is
wrong -- addr(M), addr(N), or the THUNK'S OWN ADDRESS.  When both callee names
are independently body-attested (WC2 screen A on each), the first two are
excluded and the defect is on the thunk's address: our thunk for M has been
paired against retail's thunk for N.  Adjustor thunks are 8-byte stubs that are
byte-identical except for a masked branch displacement, so they form a MASKED
CLASS in which body evidence cannot constrain the name assignment at all.  That
is the ``masked-class false pairing`` failure mode, and this screen is the
constraint that body evidence cannot supply.

>> ``--selfcheck`` requires the invariant to HOLD on the overwhelming majority of
   thunk slots.  A screen reporting that most thunks call the wrong method would
   be measuring its own name parsing, not the binary.
"""

import argparse
import collections
import glob
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "maprow_audit"))
from icf_alias_build import collect, placeholder  # noqa: E402
from sc1_characterize import undname              # noqa: E402

THUNK = re.compile(r"^\[thunk\]:\s*(.*?)`(?:vtordisp\{[^}]*\}|adjustor\{[^}]*\})'")


def method_key(demangled):
    """'Class::Method' out of a demangled signature, ignoring return/args."""
    s = demangled
    i = s.find("(")
    if i >= 0:
        s = s[:i]
    parts = s.split()
    return parts[-1] if parts else s


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--out", default="/home/free/tmp/srccand1_thunkinv.json")
    args = ap.parse_args()

    tgt = collect(sorted(glob.glob(str(ROOT / "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(str(ROOT / "build/45410914/src/**/*.obj"), recursive=True)), "o")

    al = json.load(open(ROOT / "scripts/symbol_aliases.json"))
    eq = {}
    for g in al["groups"]:
        grp = set([g["survivor"]] + list(g["folded"]))
        for n in grp:
            eq.setdefault(n, set()).update(grp)

    # every charged slot whose VICTIM is an adjustor thunk
    cand = []
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
            if placeholder(rn) or placeholder(on):
                continue
            cand.append((name, ro, rn, on, beq))

    names = sorted({c[0] for c in cand} | {c[2] for c in cand} | {c[3] for c in cand})
    d = undname(names)

    thunks, res = [], collections.Counter()
    for vic, off, rn, on, beq in cand:
        m = THUNK.match(d[vic])
        if not m:
            continue
        vm = method_key(m.group(1))
        rm, om = method_key(d[rn]), method_key(d[on])
        if om == vm and rm != vm:
            v = "OURS_HOLDS_RETAIL_VIOLATES"
        elif rm == vm and om != vm:
            v = "RETAIL_HOLDS_OURS_VIOLATES"
        elif rm == vm and om == vm:
            v = "BOTH_HOLD"
        else:
            v = "NEITHER_HOLDS"
        res[v] += 1
        thunks.append({"victim": vic, "victim_method": vm, "off": off,
                       "retail": rn, "our": on, "retail_method": rm,
                       "our_method": om, "verdict": v, "body_equal": beq})

    # CONTROL: over ALL thunk slots (charged or not), how often does our side
    # satisfy the invariant?  If the invariant fails broadly, the parser is wrong.
    hold = viol = 0
    for name, (mb, rel, sz) in ours.items():
        m = THUNK.match(d.get(name, "")) if name in d else None
        if not m:
            continue
        vm = method_key(m.group(1))
        for (oo, on, oty) in rel:
            if on in d and not placeholder(on):
                if method_key(d[on]) == vm:
                    hold += 1
                else:
                    viol += 1

    print("\n=== AXIS 5: adjustor-thunk invariant ===")
    print("  charged slots inside adjustor thunks: %d" % len(thunks))
    for k, v in res.most_common():
        print("     %-28s %4d" % (k, v))
    print("  CONTROL over our own thunk slots: invariant HOLDS %d / VIOLATES %d (%.1f%% hold)"
          % (hold, viol, 100.0 * hold / max(1, hold + viol)))

    if args.selfcheck:
        ok = (hold + viol) > 0 and hold > 3 * viol and len(res) > 1
        print("  parser sane (our side satisfies the invariant broadly): %s" % (hold > 3 * viol))
        print("  screen returns >1 verdict:                              %s" % (len(res) > 1))
        return 0 if ok else 1

    print("\n=== violations where OURS holds and RETAIL does not (map suspects) ===")
    for t in sorted([t for t in thunks if t["verdict"] == "OURS_HOLDS_RETAIL_VIOLATES"],
                    key=lambda t: (not t["body_equal"], t["victim_method"])):
        print("  %-5s %-46s  retail names %-38s"
              % ("BEQ" if t["body_equal"] else "-", t["victim_method"][:46], t["retail_method"][:38]))

    json.dump(thunks, open(args.out, "w"), indent=1)
    print("\nwrote", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
