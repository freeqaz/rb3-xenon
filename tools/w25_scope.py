#!/usr/bin/env python3
"""W25-UI: scope the UILabel / UIListWidget / UI* cluster from report.json.

Reads report.json with the documented traps handled:
  - protobuf-JSON omits defaults  -> .get(k, default) everywhere
  - several numerics are JSON strings -> int()/float() coerce everything
  - some units have no 'functions' key at all
  - matched_functions counts rows with match_percent_normalized == 100
  - matched_code sums sizes of rows with fuzzy_match_percent == 100

SELF-VALIDATES against the whole-binary measures before reporting anything:
if the row sums do not reproduce total_functions / total_code /
matched_functions / matched_code exactly, it REFUSES.
"""
import json
import sys
from collections import defaultdict


def I(x, d=0):
    return d if x is None else int(x)


def F(x, d=0.0):
    return d if x is None else float(x)


def fns_of(u):
    return u.get("functions") or []


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "build/45410914/report.json"
    pattern = sys.argv[2] if len(sys.argv) > 2 else "UI"
    with open(path) as f:
        rep = json.load(f)

    prov = rep.get("provenance", {})
    dc = prov.get("diff_config", [])
    ruler = [x for x in dc if "functionRelocDiffs" in x]
    m = rep.get("measures", {})
    print("== provenance ==")
    print("  ruler:", ruler[0] if ruler else "UNKNOWN")
    print("  tool_commit:", prov.get("tool_commit"))

    # ---- self-validation: reproduce the headline keys from the rows ----
    n_rows = n_mfn = 0
    b_tot = b_match = 0
    mpn_missing_with_fuzzy = 0
    for u in rep.get("units", []):
        for f in fns_of(u):
            n_rows += 1
            sz = I(f.get("size"))
            b_tot += sz
            fz = F(f.get("fuzzy_match_percent"))
            mpn = F(f.get("match_percent_normalized"))
            if "match_percent_normalized" not in f and \
               "fuzzy_match_percent" in f:
                mpn_missing_with_fuzzy += 1
            if mpn == 100.0:
                n_mfn += 1
            if fz == 100.0:
                b_match += sz
                n_mfn += 0
    exp = {
        "total_functions": I(m.get("total_functions")),
        "total_code": I(m.get("total_code")),
        "matched_functions": I(m.get("matched_functions")),
        "matched_code": I(m.get("matched_code")),
    }
    got = {
        "total_functions": n_rows,
        "total_code": b_tot,
        "matched_functions": n_mfn,
        "matched_code": b_match,
    }
    print("== self-validation (row sums vs headline keys) ==")
    ok = True
    for k in exp:
        flag = "OK " if exp[k] == got[k] else "MISMATCH"
        if exp[k] != got[k]:
            ok = False
        print(f"  {flag} {k}: headline={exp[k]} rowsum={got[k]}")
    print(f"  rows with fuzzy but NO mpn key (protobuf-default trap): "
          f"{mpn_missing_with_fuzzy}")
    mf, me = I(m.get("matched_functions")), I(m.get("masked_equal_functions"))
    print(f"  honest (matched - masked_equal) = {mf - me}")
    print(f"  matched_code_percent = {m.get('matched_code_percent')}")
    if not ok:
        print("REFUSING: row sums do not reproduce the headline keys.")
        sys.exit(2)

    # ---- cluster scope ----
    print()
    print(f"== units whose name contains {pattern!r} ==")
    rows_all = []
    hdr = (f"{'unit':<32} {'rows':>5} {'m_fn':>5} {'bytes':>8} "
           f"{'m_byte':>8} {'gap_B':>8}")
    print(hdr)
    print("-" * len(hdr))
    tot = defaultdict(int)
    for u in rep.get("units", []):
        name = u.get("name", "")
        if pattern not in name:
            continue
        fl = fns_of(u)
        nrows = len(fl)
        size = sum(I(f.get("size")) for f in fl)
        mbytes = sum(I(f.get("size")) for f in fl
                     if F(f.get("fuzzy_match_percent")) == 100.0)
        mfn = sum(1 for f in fl
                  if F(f.get("match_percent_normalized")) == 100.0)
        print(f"{name:<32} {nrows:>5} {mfn:>5} {size:>8} {mbytes:>8} "
              f"{size - mbytes:>8}")
        tot["rows"] += nrows
        tot["mfn"] += mfn
        tot["size"] += size
        tot["mbytes"] += mbytes
        for f in fl:
            rows_all.append((name, f))
    print("-" * len(hdr))
    print(f"{'TOTAL':<32} {tot['rows']:>5} {tot['mfn']:>5} {tot['size']:>8} "
          f"{tot['mbytes']:>8} {tot['size']-tot['mbytes']:>8}")

    print()
    print("== TOP candidates by SIZE-IF-IT-CROSSES (fuzzy < 100) ==")
    print("   cert PURE = fuzzy == mpn exactly => ZERO relocation-name")
    print("   charges; residual is purely instruction-level.")
    print("   ⛔ PURE IS GATED ON fuzzy > 0. Ungated it is VACUOUS: an UNPAIRED")
    print("      row has fuzzy == mpn == 0 and satisfies the equality trivially.")
    print("      Measured on this tree: 22,090 of 22,687 ungated PURE rows")
    print("      (97.4%, 5,196,904 of 5,245,780 B) are that 0 == 0 case.")
    cands = []
    for name, f in rows_all:
        fz = F(f.get("fuzzy_match_percent"))
        if fz >= 100.0:
            continue
        sz = I(f.get("size"))
        if sz == 0:
            continue
        mpn = F(f.get("match_percent_normalized"))
        cands.append((sz, fz, mpn, name, f.get("name", "")))
    cands.sort(reverse=True)
    hdr2 = (f"{'size':>7} {'fuzzy':>10} {'mpn':>10} {'cert':>6}  "
            f"{'unit':<24} symbol")
    print(hdr2)
    print("-" * 118)
    for sz, fz, mpn, unit, sym in cands[:40]:
        # ⛔ GATED: `fz > 0`. See the banner above -- an unpaired row is
        #    fuzzy == mpn == 0 and would be certified PURE for no reason.
        cert = ("PURE" if (abs(fz - mpn) < 1e-9 and fz > 0.0)
                else "UNPAIR" if fz <= 0.0 else "argchg")
        print(f"{sz:>7} {fz:>10.4f} {mpn:>10.4f} {cert:>6}  {unit:<24} "
              f"{sym[:60]}")

    print()
    print(f"== gap composition over {len(cands)} sub-100 rows ==")
    # ⛔ `pure` IS GATED ON fuzzy > 0 -- ungated, this bucket is 97.4% the
    #    trivial 0 == 0 case and certifies the UNPAIRABLE stratum as a clean
    #    instruction-level residual. That reading was briefed to numerous lanes
    #    for a full day before it was caught.
    pure = [c for c in cands if abs(c[1] - c[2]) < 1e-9 and c[1] > 0.0]
    vac = [c for c in cands if abs(c[1] - c[2]) < 1e-9 and c[1] <= 0.0]
    argc = [c for c in cands if abs(c[1] - c[2]) >= 1e-9]
    zero = [c for c in cands if c[1] == 0.0]
    anon = [c for c in cands if c[4].startswith(("fn_", "lbl_"))]
    print(f"  PURE   (fuzzy==mpn AND fuzzy>0 -- GATED):    "
          f"{len(pure):>4} rows  {sum(c[0] for c in pure):>8} B")
    print(f"  ⛔ would-be PURE at fuzzy==0 (VACUOUS 0==0): "
          f"{len(vac):>4} rows  {sum(c[0] for c in vac):>8} B")
    print(f"  argchg (fuzzy<mpn, has arg charges):         "
          f"{len(argc):>4} rows  {sum(c[0] for c in argc):>8} B")
    print(f"  fuzzy==0 (unpaired / no base symbol):        "
          f"{len(zero):>4} rows  {sum(c[0] for c in zero):>8} B")
    print(f"  anonymous fn_/lbl_ rows:                     "
          f"{len(anon):>4} rows  {sum(c[0] for c in anon):>8} B")
    named_nonzero = [c for c in cands
                     if c[1] > 0.0 and not c[4].startswith(("fn_", "lbl_"))]
    print(f"  NAMED and fuzzy>0 (the workable stratum):    "
          f"{len(named_nonzero):>4} rows  "
          f"{sum(c[0] for c in named_nonzero):>8} B")


if __name__ == "__main__":
    main()
