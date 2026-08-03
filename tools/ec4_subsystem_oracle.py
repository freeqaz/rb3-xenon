#!/usr/bin/env python3
"""EC-4: a SUBSYSTEM-aware neighbourhood oracle, and whether it rescues the class.

WHY A SECOND ORACLE AT ALL
--------------------------
tools/ec2_neighbourhood_attribution.py flags a row when NO map-named neighbour
shares a CLASS NAME with the row's unit.  Measured binary-wide with an
isolation-matched control that instrument does not discriminate (1.13-1.23x),
and the reason is visible in its false positives:

    CharSleeve::Poll              (1,980B, mpn 100) -- neighbours CharNeckTwist
    AccomplishmentOneShot::Init.. (  600B, mpn 100) -- neighbours AccomplishmentPlayerConditional
    BinkClip::Handle              (  460B, mpn 100) -- neighbours MoggClip

Every one of those is byte-equal to retail, hence PROVABLY attributed correctly,
and every one is flagged because the adjacent TU holds a DIFFERENT class from
the SAME SUBSYSTEM.  RB3 has no LTCG, so `.text` groups TUs by subsystem; exact
class-name equality therefore fails at essentially every TU boundary, and a
WHOLE sliver pin is nothing but boundary.

THE REFINEMENT: resolve each neighbour CLASS to the unit(s) that define it in
report.json, map those units to their SOURCE DIRECTORY via the census, and ask
whether any neighbour comes from the same directory as the row's own unit.  That
is the question the original instrument was reaching for.

It is reported against the SAME isolation-matched control, because a refinement
measured against a confounded control would just reproduce the confound.
"""
import argparse
import bisect
import collections
import json
import pathlib
import posixpath
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))


def cls_of(sym):
    if not sym or not sym.startswith("?"):
        return None
    if sym.startswith("??"):
        m = re.match(r"^\?\?[_0-9A-Za-z]{1,2}([A-Za-z_][\w]*)@@", sym)
        return m.group(1) if m else None
    m = re.match(r"^\?[^@?]+@([A-Za-z_][\w]*)@", sym)
    return m.group(1) if m else None


BINS = [(0.0, 0.001), (0.001, 0.25), (0.25, 0.50), (0.50, 0.75), (0.75, 1.001)]
BIN_NAMES = ["iso=0%", "0-25%", "25-50%", "50-75%", "75-100%"]


def bin_of(x):
    for i, (lo, hi) in enumerate(BINS):
        if lo <= x < hi:
            return i
    return len(BINS) - 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--census", required=True)
    ap.add_argument("--positions", required=True)
    ap.add_argument("--k", type=int, default=6)
    ap.add_argument("--window", type=lambda x: int(x, 0), default=0x600)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    root = pathlib.Path(a.root).resolve()
    rep = json.loads((root / "build/45410914/report.json").read_text())
    cen = json.loads(pathlib.Path(a.census).read_text())
    amap = json.loads((root / "scripts/target_symbol_map.json").read_text())
    pos = json.loads(pathlib.Path(a.positions).read_text())

    srcdir = {}
    for u in cen["units"]:
        sp = u.get("source_path")
        srcdir[u["unit"]] = posixpath.dirname(sp) if sp else None

    # class -> set of units defining it
    cls_units = collections.defaultdict(set)
    for u in rep["units"]:
        for f in (u.get("functions") or []):
            c = cls_of(f["name"])
            if c:
                cls_units[c].add(u["name"])

    pairs = sorted((int(k, 16), v) for k, v in amap.items() if k.startswith("0x"))
    addrs = [p[0] for p in pairs]

    blocks = collections.defaultdict(set)
    for r in pos:
        blocks[r["unit"]].add(tuple(int(x, 16) for x in r["block"]))

    own_cls = {}
    for u in rep["units"]:
        s = {c for c in (cls_of(f["name"]) for f in (u.get("functions") or [])) if c}
        own_cls[u["name"]] = s

    TMPL = ("??$", "?$vector@", "?$_Rb_tree@", "?$list@", "@stlpmtx_std@@")
    rows = []
    for r in pos:
        if r["anon"]:
            continue
        sym, un = r["sym"], r["unit"]
        if any(t in sym for t in TMPL):
            continue
        if not own_cls.get(un):
            continue                      # vacuity guard 1: no own class at all
        if cls_of(sym) is None:
            continue                      # vacuity guard 3 (STRENGTHENED): a FREE
                                          # FUNCTION has no class, so it can never
                                          # match a neighbour -- vacuous even inside
                                          # a unit that does have classes.
        va = int(r["va"], 16)
        i = bisect.bisect_left(addrs, va)
        same_iso = tot = 0
        nb_dirs, nb_cls = set(), []
        mydir = srcdir.get(un)
        for j in range(max(0, i - a.k), min(len(pairs), i + a.k + 1)):
            nva, nn = pairs[j]
            if nva == va or abs(nva - va) > a.window:
                continue
            tot += 1
            if any(lo <= nva < hi for lo, hi in blocks[un]):
                same_iso += 1
            c = cls_of(nn)
            if not c:
                continue
            nb_cls.append(c)
            for v in cls_units.get(c, ()):
                d = srcdir.get(v)
                if d:
                    nb_dirs.add(d)
        if not tot or not nb_cls:
            continue
        cls_agree = bool(set(nb_cls) & own_cls[un])
        dir_agree = bool(mydir) and mydir in nb_dirs
        rows.append(dict(unit=un, sym=sym, va=r["va"], pos=r["pos"], size=r["size"],
                         fuzzy=r["fuzzy"], mpn=r["mpn"], iso=same_iso / tot,
                         mydir=mydir, nb_dirs=sorted(nb_dirs)[:6],
                         foreign_cls=not cls_agree, foreign_dir=not dir_agree,
                         stratum="CHARGED" if r["mpn"] < 100.0 else "CONTROL"))
    pathlib.Path(a.out).write_text(json.dumps(rows, indent=1))

    ch = [r for r in rows if r["stratum"] == "CHARGED"]
    co = [r for r in rows if r["stratum"] == "CONTROL"]
    print(f"adjudicable rows (non-template, has own class, IS a class member): "
          f"charged={len(ch)} control={len(co)}\n")

    for key, label in (("foreign_cls", "EC-2 exact class-name test"),
                       ("foreign_dir", "EC-4 same-source-DIRECTORY test")):
        print(f"=== {label} ===")
        print(f"{'bin':10s} {'chg_f':>6s} {'chg_n':>6s} {'chg%':>7s} | "
              f"{'ctl_f':>6s} {'ctl_n':>6s} {'ctl%':>7s} | {'enr':>7s}")
        cb = [[r for r in ch if bin_of(r["iso"]) == i] for i in range(len(BINS))]
        ob = [[r for r in co if bin_of(r["iso"]) == i] for i in range(len(BINS))]
        for i, nm in enumerate(BIN_NAMES):
            cf = sum(1 for r in cb[i] if r[key]); cn = len(cb[i])
            of = sum(1 for r in ob[i] if r[key]); on = len(ob[i])
            cp = 100 * cf / cn if cn else 0
            op = 100 * of / on if on else 0
            print(f"{nm:10s} {cf:6d} {cn:6d} {cp:6.2f}% | {of:6d} {on:6d} {op:6.2f}% | "
                  f"{(cp/op) if op else float('inf'):6.2f}x")
        craw = 100 * sum(1 for r in ch if r[key]) / len(ch)
        oraw = 100 * sum(1 for r in co if r[key]) / len(co)
        num = den = 0.0
        for i in range(len(BINS)):
            if not ob[i] or not cb[i]:
                continue
            num += len(ob[i]) * (sum(1 for r in cb[i] if r[key]) / len(cb[i]))
            den += len(ob[i])
        adj = 100 * num / den if den else float("nan")
        print(f"  CRUDE {craw:.2f}% vs {oraw:.2f}% = {craw/oraw if oraw else 0:.2f}x   "
              f"|  ISOLATION-STANDARDISED {adj:.2f}% vs {oraw:.2f}% = "
              f"{adj/oraw if oraw else 0:.2f}x\n")
    print(f"wrote {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
