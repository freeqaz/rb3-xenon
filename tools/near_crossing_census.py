#!/usr/bin/env python3
"""Enumerate the paired rows that are one or two charges from crossing.

`matched_code` is all-or-nothing at `fuzzy_match_percent == 100`, so a row at
99.9 pays nothing and the same row at 100 pays its whole size.  This tool lists
the rows in that band, ranked by size-if-it-crosses, and -- with --classify --
reads each row's CHARGED SITES so the band can be split by what actually has to
be fixed.

Why the split matters (lane W8-B, 2026-09-13):

  * `mpn` (match_percent_normalized) excludes `register` argument diffs but
    CHARGES `symbol` (relocation-name) ones.  So `mpn == fuzzy` does NOT mean
    "no argument charges" -- it is in fact the signature of the NAME/fold
    stratum, which is the most fold-like case, not the least.
  * of the 200 largest rows in the band, 138 (163,812 B) are symbol-only and
    exactly 2 (5,624 B) are pure insert/delete.  The near-crossing frontier is
    a NAMING frontier, not a porting one.

Pricing (measured exact on 33 single-kind rows, max deviation 0.002):

    charges = (100 - fuzzy) / (5/N),  N = target_size/4

    diff_arg argument (symbol OR register) ... 1.000 * (5/N) PER ARGUMENT
    insert / delete / replace ................ 20.000 * (5/N)  == 100/N

  ! The unit is the charged ARGUMENT, not the charged instruction: one
    `diff_arg` row carrying two differing registers prices as two.
  ! Do NOT apply the model to rows containing `immediate` args -- fitting it
    over those gives a NEGATIVE immediate price, i.e. the model is wrong there,
    not merely imprecise (the denominator also stops being target_size/4 once
    inserts exist).

Reads report.json only; --classify shells out to `bin/objdiff-cli diff` WITHOUT
`--build`, so it never bypasses the six post-compile obj patchers.
"""
import argparse, json, subprocess, sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def rows(report, lo, hi):
    d = json.loads(Path(report).read_text())
    prov = d.get("provenance", {})
    cfg = [c for c in prov.get("diff_config", []) if "RelocDiffs" in c]
    m = d["measures"]
    out = []
    for u in d["units"]:
        for f in u.get("functions", []):
            fz = float(f.get("fuzzy_match_percent", 0) or 0)
            if lo <= fz < hi:
                out.append(dict(size=int(f.get("size", 0) or 0), fuzzy=fz,
                                mpn=float(f.get("match_percent_normalized", 0) or 0),
                                unit=u["name"], name=f.get("name", "")))
    out.sort(key=lambda r: -r["size"])
    return out, m, cfg


def charged(unit, sym, cli):
    p = subprocess.run([str(cli), "diff", "-p", str(ROOT), "-u", unit, sym,
                        "--include-instructions", "-f", "json", "-o", "-"],
                       capture_output=True, text=True, cwd=str(ROOT))
    if p.returncode != 0:
        return None
    d = json.loads(p.stdout)
    c = Counter()
    for i in d["instructions"]:
        mt = i.get("match_type")
        if mt == "equal":
            continue
        if mt == "diff_arg":
            args = (i.get("diff_breakdown") or {}).get("arguments") or []
            for a in args:
                c[a.get("arg_type") or "arg?"] += 1
            if not args:
                c["arg?"] += 1
        else:
            c[mt] += 1
    return c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--report", default="build/45410914/report.json")
    ap.add_argument("--min-fuzzy", type=float, default=99.5)
    ap.add_argument("--top", type=int, default=60)
    ap.add_argument("--classify", type=int, default=0,
                    help="read charged sites for the N largest rows")
    ap.add_argument("--cli", default="bin/objdiff-cli")
    a = ap.parse_args()

    rs, m, cfg = rows(ROOT / a.report, a.min_fuzzy, 100.0)
    print("ruler: %s" % (cfg or "UNKNOWN -- refusing to price"))
    if not cfg:
        sys.exit(2)
    print("binary: matched_code=%s / total_code=%s (%s%%)"
          % (m.get("matched_code"), m.get("total_code"), m.get("matched_code_percent")))
    tot = sum(r["size"] for r in rs)
    print("band %.2f <= fuzzy < 100: %d rows, %d B (%.3f%% of total_code)"
          % (a.min_fuzzy, len(rs), tot, 100.0 * tot / int(m["total_code"])))
    pure = [r for r in rs if r["mpn"] >= 99.9999]
    print("  mpn == 100 (register/branch-dest only, permuter-class): %d rows, %d B"
          % (len(pure), sum(r["size"] for r in pure)))
    print("  mpn <  100 (name charge or real instruction diff):      %d rows, %d B"
          % (len(rs) - len(pure), tot - sum(r["size"] for r in pure)))

    if not a.classify:
        print("\n%7s %10s %10s  %-34s %s" % ("size", "fuzzy", "charges", "unit", "symbol"))
        for r in rs[: a.top]:
            n = r["size"] / 4.0
            print("%7d %10.5f %10.2f  %-34s %s"
                  % (r["size"], r["fuzzy"], (100.0 - r["fuzzy"]) / (5.0 / n) if n else 0,
                     r["unit"][:34], r["name"]))
        return

    cli = ROOT / a.cli
    agg, byt = Counter(), Counter()
    print("\n%7s %10s  %-40s %s" % ("size", "fuzzy", "symbol", "charge kinds"))
    for r in rs[: a.classify]:
        c = charged(r["unit"], r["name"], cli)
        if c is None:
            print("%7d %10.5f  %-40s ERROR" % (r["size"], r["fuzzy"], r["name"][:40]))
            continue
        argk = c["register"] + c["symbol"] + c["immediate"] + c["branch_dest"] + c["arg?"]
        ins = c["insert"] + c["delete"] + c["replace"]
        cls = ("symbol-only" if c["symbol"] and not (c["register"] or c["immediate"] or ins)
               else "register-only" if c["register"] and not (c["symbol"] or c["immediate"] or ins)
               else "insdel-only" if ins and not argk else "mixed")
        agg[cls] += 1
        byt[cls] += r["size"]
        print("%7d %10.5f  %-40s %-14s %s"
              % (r["size"], r["fuzzy"], r["name"][:40], cls, dict(c)))
    print("\n=== class totals over the %d largest ===" % a.classify)
    for k in sorted(agg, key=lambda k: -byt[k]):
        print("  %-14s rows=%-4d bytes=%d" % (k, agg[k], byt[k]))


if __name__ == "__main__":
    main()
