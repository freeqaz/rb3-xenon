#!/usr/bin/env python3
"""W16-AG: PRE-REGISTER the byte effect of withdrawing an alias membership.

Same question as `tools/w16ad_predict_withdrawal.py`, with two changes.

⛔⛔ 1. THAT TOOL RESOLVES THE GROUP AS `aliases["groups"][gi]`.

W16-AD §4.2 measured the census `gi` NOT to be an index into groups[] and fixed
`w16ad_apply_withdrawals.py` and `w16s_ablate.py` accordingly -- but the
PREDICTOR kept the defect.  Measured here over its own input: of the 63
WITNESS_REFUTED rows, **0** have gi equal to the true group index and **0** have
their folded spelling present in `groups[gi]`.  The `members` set it tests
against is therefore always some unrelated group's names, `nm in members` can
essentially never fire, and its "predicted loss: 0 bytes" was produced BY
CONSTRUCTION rather than measured.

That does not overturn W16-AD's Δ0 *measurement*, nor its independent argument
(no refuted spelling is a report row; all 74 call sites sit in functions that
are not report rows).  It means the PREDICTION carried no information, and an
all-zero prediction is exactly the shape CLAUDE.md says to distrust.  Keyed on
`(survivor, address)`, which is unique over all groups.

2. FORGIVENESS IS SYMMETRIC, SO BOTH DIRECTIONS ARE PRICED.

objdiff consults the equivalence class; removing N from it un-forgives a site in
EITHER orientation:
  (a) our relocation names N   and retail's destination is map-named M != N in
      the same group;  and
  (b) our relocation names some other member M and retail's destination is
      map-named N.
The W16-AD predictor only walked (a) -- call sites of N.  (b) is a real exposure
whenever the survivor is the spelling we emit, which is the common case.

A retail destination with NO map name is already forgiven as a placeholder
(`is_placeholder_symbol_name`), so it costs nothing either way.  `matched_code`
is all-or-nothing per row, so the unit of prediction is a ROW SET, never a site
count.
"""
import collections
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
import w16ad_fold_witness as FW                                   # noqa: E402

BUILD_ID = "45410914"


def report_rows():
    """name -> (unit, size, fuzzy).  protobuf-JSON: defaults omitted, numerics
    are STRINGS -- int()/float()-coerce or comparisons go lexicographic."""
    rep = json.loads((ROOT / "build" / BUILD_ID / "report.json").read_text())
    out = {}
    for u in rep.get("units", []):
        for f in u.get("functions", []):
            out[f.get("name", "")] = (u.get("name"),
                                      int(f.get("size", 0) or 0),
                                      float(f.get("fuzzy_match_percent", 0)
                                            or 0))
    return out


def main():
    sel = json.loads(Path(sys.argv[1]).read_text())        # rows to withdraw
    W = FW.Witnesser()
    rows_all = report_rows()
    f100 = {n: v[1] for n, v in rows_all.items() if v[2] == 100.0}
    aliases = json.loads((ROOT / "scripts/symbol_aliases.json").read_text())
    gkey = {(g["survivor"], g.get("address")): i
            for i, g in enumerate(aliases["groups"])}
    print("fuzzy==100 rows: %d  (all report rows: %d)"
          % (len(f100), len(rows_all)), file=sys.stderr)

    out = []
    for r in sel:
        i = gkey.get((r["survivor"], r["addr"]))
        if i is None:
            sys.exit("REFUSING: no group for survivor=%s addr=%s"
                     % (r["survivor"][:60], r["addr"]))
        grp = aliases["groups"][i]
        N = r["folded"]
        if N not in grp["folded"]:
            sys.exit("REFUSING: %s absent from group %d's folded list"
                     % (N[:70], i))
        others = (set(grp["folded"]) | {grp["survivor"]}) - {N}
        dep, unnamed, free = {}, 0, 0
        status = collections.Counter()
        charged = []

        # (a) our relocation names N, retail's destination names another member
        for caller, off in sorted(set(W.callsites.get(N, []))):
            status["fuzzy100" if caller in f100 else
                   "scored_below_100" if caller in rows_all else
                   "not_a_report_row"] += 1
            for va in sorted(W.byname.get(caller, [])):
                dest = FW.decode_bl(W.img, va, off)
                if dest is None:
                    continue
                nm = W.byva.get(dest)
                if not nm:
                    unnamed += 1
                elif nm == N:
                    free += 1
                elif nm in others:
                    charged.append(("a", caller, off, nm))
                    if caller in f100:
                        dep[caller] = f100[caller]

        # (b) our relocation names another member, retail's destination names N
        for M in sorted(others):
            for caller, off in sorted(set(W.callsites.get(M, []))):
                for va in sorted(W.byname.get(caller, [])):
                    dest = FW.decode_bl(W.img, va, off)
                    if dest is None:
                        continue
                    if W.byva.get(dest) == N:
                        charged.append(("b", caller, off, M))
                        status["reverse_dir"] += 1
                        if caller in f100:
                            dep[caller] = f100[caller]

        out.append({
            "gi": r["gi"], "addr": r["addr"], "group_index": i,
            "census_bytes": int(r["bytes"]), "folded": N,
            "survivor": r["survivor"],
            "our_callsites_of_N": len(set(W.callsites.get(N, []))),
            "sites_retail_unnamed": unnamed, "sites_retail_names_N": free,
            "caller_report_status": dict(status),
            "self_is_report_row": N in rows_all,
            "charged_sites": [{"dir": d, "caller": c, "off": o, "other": m}
                              for d, c, o, m in charged],
            "dependent_fuzzy100_rows": sorted(dep),
            "predicted_bytes_lost": sum(dep.values()),
            "predicted_rows_lost": len(dep),
        })

    if len(sys.argv) > 2:
        Path(sys.argv[2]).write_text(json.dumps(out, indent=1) + "\n")
    tb = sum(x["predicted_bytes_lost"] for x in out)
    tr = len({n for x in out for n in x["dependent_fuzzy100_rows"]})
    cs = collections.Counter()
    for x in out:
        cs.update(x["caller_report_status"])
    print("\nPREDICTION over %d memberships" % len(out))
    print("  memberships with >=1 dependent fuzzy==100 row : %d"
          % sum(1 for x in out if x["dependent_fuzzy100_rows"]))
    print("  distinct dependent rows (predicted to FALL)   : %d" % tr)
    print("  predicted matched_code loss (bytes)           : %d" % tb)
    print("  newly-charged sites, direction (a)            : %d"
          % sum(1 for x in out for c in x["charged_sites"]
                if c["dir"] == "a"))
    print("  newly-charged sites, direction (b)            : %d"
          % sum(1 for x in out for c in x["charged_sites"]
                if c["dir"] == "b"))
    print("  call sites of N whose retail dest is UNNAMED  : %d"
          % sum(x["sites_retail_unnamed"] for x in out))
    print("  call sites where retail names N itself        : %d"
          % sum(x["sites_retail_names_N"] for x in out))
    print("  caller report-row status                      : %s" % dict(cs))
    print("  withdrawn spellings that are themselves rows  : %d"
          % sum(1 for x in out if x["self_is_report_row"]))
    print("\nVACUITY CHECK -- an all-zero prediction must be distinguishable")
    print("  total call sites of N examined                : %d"
          % sum(x["our_callsites_of_N"] for x in out))
    print("  of those, in functions that are report rows   : %d"
          % sum(v for x in out for k, v in x["caller_report_status"].items()
                if k in ("fuzzy100", "scored_below_100")))
    for x in out:
        print("\n gi=%-5s %-10s %4d B  pred_lost=%d B / %d rows"
              % (x["gi"], x["addr"], x["census_bytes"],
                 x["predicted_bytes_lost"], x["predicted_rows_lost"]))
        print("   N=%s" % x["folded"][:96])
        print("   sites of N=%d unnamed=%d names_N=%d charged=%d %s"
              % (x["our_callsites_of_N"], x["sites_retail_unnamed"],
                 x["sites_retail_names_N"], len(x["charged_sites"]),
                 dict(x["caller_report_status"])))
        for c in x["charged_sites"][:6]:
            print("     CHARGE(%s) %s +%d  vs %s"
                  % (c["dir"], c["caller"][:60], c["off"], c["other"][:50]))


if __name__ == "__main__":
    main()
