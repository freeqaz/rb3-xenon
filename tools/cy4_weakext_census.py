#!/usr/bin/env python3
"""Lane CY-4 part 3: size the COFF weak-external alias class over the WHOLE
name-mismatch census, and roll the new verdicts up to FUNCTIONS.

Part 1 (cy4_weakext_adjudicate) proved the mechanism on the at-100 UNKNOWN block:
MSVC emits ??_E<C> (vector deleting destructor) as an undefined WEAK EXTERNAL
whose aux record defaults to ??_G<C> (scalar deleting destructor), 10,119/10,119
with zero exceptions. A `bl ??_E<C>` therefore LINKS TO ??_G<C>, so a census row
charging "target says ??_G<C>, base says ??_E<C>" is an ALIAS, not a wrong callee.

This part answers two questions the at-100 slice cannot:

  (1) HOW BIG IS IT? The at-100 block is a small slice of the 25,310 charged
      sites. If the alias class is systematic it should appear across the whole
      census, including rows nobody has adjudicated.

  (2) WHAT DOES IT DO TO THE FUNCTION-LEVEL ACCOUNTING? The brief's units are
      functions, not pairs. Roll up with worst-verdict-dominates, exactly as
      at100_adjudicate.py does, so the numbers compose with CW-2's.

DENOMINATORS ARE PRINTED AT EVERY STAGE. The unknown bucket stays explicit; a row
this channel cannot explain is never reclassified into a decided class.

Read-only.
"""

import argparse
import collections
import json
import os
import sys
from pathlib import Path

ROOT = os.environ.get("CY4_ROOT", str(Path(__file__).resolve().parent.parent))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from cy4_weakext_adjudicate import build_index                # noqa: E402

PLACEHOLDER = __import__("re").compile(
    r'^_?(fn|lbl|jumptable|code|data|bss|rdata)_[0-9a-fA-F_]+$')


def forgiven(n):
    return bool(PLACEHOLDER.match(n)) or n.startswith('$')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=ROOT)
    ap.add_argument("--sites", required=True)
    ap.add_argument("--report", required=True)
    ap.add_argument("--adj", required=True)
    ap.add_argument("--resolved", required=True)
    ap.add_argument("--out", default=None)
    a = ap.parse_args()

    weak, defined, _per, nobj, conflicts = build_index(a.root)
    print("objs %d   undefined weak externals %d   name->default conflicts %d"
          % (nobj, len(weak), conflicts))

    sites = json.load(open(a.sites))["records"]
    rep = json.load(open(a.report))
    fnsize, fnpct = {}, {}
    # ★ CZ-3: FULL unit name, not stem -- see at100_adjudicate.load_rep. After
    # f592571a the census emits full unit paths and this join silently went to 0.
    for u in rep["units"]:
        for f in (u.get("functions") or []):
            fnsize[(u["name"], f["name"])] = int(f["size"])
            fnpct[(u["name"], f["name"])] = f["match_percent_normalized"]
    if not ({u for u, _f, _r in sites} & {u for u, _f in fnpct}):
        sys.exit("REFUSING: sites/report unit keys do not join (f592571a breakage)")

    # ---------- (1) whole-census sizing -----------------------------------
    tot_sites = tot_charged_fns = 0
    alias_sites = 0
    alias_fns = set()
    allcharged_fns = set()
    fully_explained = set()
    per_fn_alias = collections.Counter()
    per_fn_other = collections.Counter()
    for unit, fn, rows in sites:
        rs = [r for r in rows if not forgiven(r[1])]
        if not rs:
            continue
        tot_charged_fns += 1
        allcharged_fns.add((unit, fn))
        for _k, T, B in rs:
            tot_sites += 1
            if weak.get(B) == T and B not in defined:
                alias_sites += 1
                alias_fns.add((unit, fn))
                per_fn_alias[(unit, fn)] += 1
            else:
                per_fn_other[(unit, fn)] += 1
    for f in alias_fns:
        if per_fn_other[f] == 0:
            fully_explained.add(f)

    print("\n=== WEAK-EXTERNAL ALIAS CLASS OVER THE WHOLE CENSUS ===")
    print("   charged sites (placeholder-forgiven)        %6d" % tot_sites)
    print("   of which weak-external ALIAS                %6d  (%5.2f%%)"
          % (alias_sites, 100.0 * alias_sites / max(1, tot_sites)))
    print("   charged functions                           %6d" % tot_charged_fns)
    print("   touching >=1 alias site                     %6d  (%5.2f%%)"
          % (len(alias_fns), 100.0 * len(alias_fns) / max(1, tot_charged_fns)))
    print("   *** charged ONLY by alias sites -> the whole")
    print("       charge on these functions is spurious   %6d  (%5.2f%%) ***"
          % (len(fully_explained),
             100.0 * len(fully_explained) / max(1, tot_charged_fns)))
    b = sum(fnsize.get(f, 0) for f in fully_explained)
    print("   bytes in those fully-explained functions    %6d" % b)
    at100_fe = [f for f in fully_explained if fnpct.get(f) == 100.0]
    print("   of them at normalized-100                   %6d  (%d B)"
          % (len(at100_fe), sum(fnsize.get(f, 0) for f in at100_fe)))

    # ---------- (2) function roll-up of the at-100 class-(b) UNKNOWN ------
    adj = {(t, bb): (cls, v) for t, bb, cls, v, _r, _h, _n
           in json.load(open(a.adj))["pairs"]}
    newv = {(r[0], r[1]): r[5] for r in json.load(open(a.resolved))["rows"]}
    wk = json.load(open(a.out.replace("census", "weakext"))) if False else None

    at100 = []
    for unit, fn, rows in sites:
        rs = [r for r in rows if not forgiven(r[1])]
        if not rs or fnpct.get((unit, fn)) != 100.0:
            continue
        at100.append((unit, fn, rs))

    ORDER = ["a_wrong", "a_wrong_shapetwin", "b_backlog", "d_body_unreadable",
             "d_no_target_addr", "c_shapetwin_ours", "c_fold_ours"]

    def newverdict(T, B):
        """CW-2 verdict, then CY-4's two refinements. UNKNOWN stays UNKNOWN
        unless a channel positively decides it."""
        base = adj.get((T, B), (None, "UNKNOWN"))[1]
        if base != "UNKNOWN":
            return base, "cw2"
        if weak.get(B) == T and B not in defined:
            return "BENIGN", "cy4_weakext_alias"
        nv = newv.get((T, B))
        if nv and nv != "UNKNOWN":
            return ("BENIGN" if nv.startswith("BENIGN") else nv), "cy4_resolved_byte"
        return "UNKNOWN", "still"

    cnt = collections.Counter()
    byt = collections.Counter()
    prov = collections.Counter()
    for unit, fn, rs in at100:
        clss = {adj[(T, B)][0] for _k, T, B in rs if (T, B) in adj}
        if not clss:
            continue
        home = next((c for c in ORDER if c in clss), None)
        if home != "b_backlog":
            continue
        vs = []
        for _k, T, B in rs:
            if adj.get((T, B), (None,))[0] != "b_backlog":
                continue
            v, p = newverdict(T, B)
            vs.append(v)
            prov[p] += 1
        if not vs:
            continue
        w = next((x for x in ("WRONG_2ch", "WRONG_1ch", "UNKNOWN") if x in vs),
                 "BENIGN")
        cnt[w] += 1
        byt[w] += fnsize.get((unit, fn), 0)

    tot = sum(cnt.values())
    totb = sum(byt.values())
    print("\n=== CLASS (b) FUNCTION ROLL-UP after CY-4  (denominator %d fns / %d B) ==="
          % (tot, totb))
    for k in ("BENIGN", "WRONG_2ch", "WRONG_1ch", "UNKNOWN"):
        print("   %-10s fns %5d (%5.1f%%)   bytes %7d (%5.1f%%)"
              % (k, cnt[k], 100.0 * cnt[k] / max(1, tot),
                 byt[k], 100.0 * byt[k] / max(1, totb)))
    print("\n   pair-verdict provenance: %s" % dict(prov))

    if a.out:
        json.dump({"alias_sites": alias_sites, "tot_sites": tot_sites,
                   "alias_fns": len(alias_fns),
                   "fully_explained": len(fully_explained),
                   "fully_explained_bytes": b,
                   "rollup": dict(cnt), "rollup_bytes": dict(byt),
                   "provenance": dict(prov)}, open(a.out, "w"))
        print("\nwrote " + a.out)


if __name__ == "__main__":
    main()
