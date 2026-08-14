#!/usr/bin/env python3
"""SRCCAND-1 AXIS 5b: propose the thunk-name repair implied by the invariant.

For every charged slot inside an adjustor thunk where the invariant is violated
(``OURS_HOLDS_RETAIL_VIOLATES``), the repair implied is mechanical: the thunk
whose branch resolves to method ``N`` is ``N``'s thunk, so the map row on the
thunk's address should spell ``N``, not ``M``.

THREE GATES, because a name reassignment that is not a PERMUTATION is not a
repair -- it is a fabrication:

  (1) INJECTIVITY.  Within a class, the proposed names must be distinct and must
      be a permutation of the names already present.  Inventing a name that the
      map did not already carry, or collapsing two rows onto one name, is
      refused.  (An unproven reassignment lifts name_check BY CONSTRUCTION, which
      is the alias hazard in another costume.)
  (2) OUR SIDE DEFINES IT.  Our compiled objs must define the proposed thunk
      symbol, or the renamed target row cannot pair with anything and the repair
      is inert at best.
  (3) RECIPROCITY.  A permutation has to close.  A violation whose partner is
      absent means the cycle leaves the observed population, and the evidence
      does not determine the assignment.

>> This tool only PROPOSES.  Nothing is written to the map here.
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
from icf_alias_build import collect  # noqa: E402
from sc1_characterize import undname  # noqa: E402

THUNK = re.compile(r"^\[thunk\]:\s*(.*?)`(?:vtordisp\{[^}]*\}|adjustor\{[^}]*\})'")


def method_key(s):
    i = s.find("(")
    if i >= 0:
        s = s[:i]
    p = s.split()
    return p[-1] if p else s


def cls_of(mk):
    return mk.rsplit("::", 1)[0] if "::" in mk else ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--thunkinv", default="/home/free/tmp/srccand1_thunkinv.json")
    ap.add_argument("--out", default="/home/free/tmp/srccand1_thunkfix.json")
    args = ap.parse_args()

    inv = json.load(open(args.thunkinv))
    viol = [t for t in inv if t["verdict"] == "OURS_HOLDS_RETAIL_VIOLATES"]

    tm = json.load(open(ROOT / "scripts/target_symbol_map.json"))
    addr_of = {v: k for k, v in tm.items()
               if isinstance(k, str) and k.lower().startswith("0x") and isinstance(v, str)}
    ours = collect(sorted(glob.glob(str(ROOT / "build/45410914/src/**/*.obj"), recursive=True)), "o")

    # our thunk symbol -> its address per the map; proposed name = the thunk of
    # the method retail's branch actually reaches.
    names = sorted({t["victim"] for t in viol})
    d = undname(names)

    # index our thunk symbols by (class, method) so we can name the replacement
    thunk_sym = {}
    for n in ours:
        s = d.get(n)
        if s is None:
            continue
    # need demangling of ALL our thunk-ish symbols
    cand = [n for n in ours if n.startswith("?") and ("$4" in n or "@W" in n or "?_E" in n or "$R" in n)]
    d2 = undname(sorted(set(cand) | set(names)))
    for n in cand:
        m = THUNK.match(d2.get(n, ""))
        if m:
            thunk_sym.setdefault(method_key(m.group(1)), []).append(n)

    props, refuse = [], collections.Counter()
    for t in viol:
        vic = t["victim"]
        a = addr_of.get(vic)
        if a is None:
            refuse["victim_not_map_resident"] += 1
            continue
        want = t["retail_method"]          # the method retail's branch reaches
        opts = thunk_sym.get(want, [])
        if not opts:
            refuse["no_our_thunk_for_target_method"] += 1
            continue
        if len(opts) > 1:
            refuse["ambiguous_our_thunk"] += 1
            continue
        props.append({"address": a, "old": vic, "new": opts[0],
                      "old_method": t["victim_method"], "new_method": want,
                      "cls": cls_of(t["victim_method"]), "body_equal": t["body_equal"]})

    # gate 1: injectivity + permutation, per class
    byc = collections.defaultdict(list)
    for p in props:
        byc[p["cls"]].append(p)
    ok, bad = [], collections.Counter()
    for c, ps in byc.items():
        news = [p["new"] for p in ps]
        olds = [p["old"] for p in ps]
        if len(set(news)) != len(news):
            bad["non_injective_new"] += len(ps)
            continue
        if set(news) != set(olds):
            bad["not_a_closed_permutation"] += len(ps)
            continue
        if any(p["new"] not in ours for p in ps):
            bad["our_objs_do_not_define"] += len(ps)
            continue
        ok += ps

    print("\n=== AXIS 5b: thunk-name repair proposal ===")
    print("  invariant violations                 %4d" % len(viol))
    for k, v in refuse.most_common():
        print("     refused: %-32s %4d" % (k, v))
    print("  proposals formed                     %4d  (%d classes)" % (len(props), len(byc)))
    for k, v in bad.most_common():
        print("     gated:   %-32s %4d" % (k, v))
    print("  PASS all gates                       %4d" % len(ok))
    for p in sorted(ok, key=lambda p: p["cls"]):
        print("     %s  %-34s -> %-34s" % (p["address"], p["old_method"][:34], p["new_method"][:34]))

    json.dump({"pass": ok, "all": props}, open(args.out, "w"), indent=1)
    print("\nwrote", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
