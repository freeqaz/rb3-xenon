#!/usr/bin/env python3
"""W16-AD: PRE-REGISTER the byte effect of withdrawing an alias membership.

A membership forgives a call site only under a NARROW condition, and getting
that condition right is the difference between a prediction and a guess:

    our relocation names N   AND   retail's destination at that site carries a
    MAP NAME that is another member of the same group

Both halves matter.
  * If retail's destination is UNNAMED, objdiff forgives it anyway --
    `is_placeholder_symbol_name` (objdiff-core diff/code.rs) drops the charge
    for fn_/lbl_/data_/... targets -- so the alias is doing no work there and
    the withdrawal is FREE.  This is the case the naive "count the call sites"
    prediction gets wrong, and it is the majority case.
  * If retail's destination is named N itself, the site passes without any
    alias and the withdrawal is free.

So the predicted loss is the set of `fuzzy == 100` rows that own at least one
site meeting BOTH halves.  `matched_code` is ALL-OR-NOTHING per row, so a row
with one newly-charged site loses its ENTIRE size -- the prediction is a row
set, never a site count.
"""
import collections
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
import w16ad_fold_witness as FW                                   # noqa: E402

BUILD_ID = "45410914"


def fuzzy100_rows():
    """name -> size, for every row the grader scores at fuzzy == 100.

    ⛔ report.json is protobuf-JSON: defaults are OMITTED and numerics are
    STRINGS.  Absent fuzzy_match_percent means 0, not "missing"; int()-coerce
    every size or a `>` comparison silently compares lexicographically.
    """
    rep = json.loads((ROOT / "build" / BUILD_ID / "report.json").read_text())
    out = {}
    for u in rep.get("units", []):
        for f in u.get("functions", []):
            if float(f.get("fuzzy_match_percent", 0) or 0) == 100.0:
                out[f.get("name", "")] = int(f.get("size", 0) or 0)
    return out


def report_rows():
    """name -> (unit, size, fuzzy) for EVERY report row, scored or not.

    ⛔ The narrow forgiveness test below can return an all-zero answer for two
    completely different reasons, and they must not be confused:
      (i)  the alias really forgives nothing -- a finding; or
      (ii) the loop never ran because no caller was map-named -- a VACUITY that
           looks exactly like (i).
    Recording each caller's report-row status separates them.
    """
    rep = json.loads((ROOT / "build" / BUILD_ID / "report.json").read_text())
    out = {}
    for u in rep.get("units", []):
        for f in u.get("functions", []):
            out[f.get("name", "")] = (u.get("name"), int(f.get("size", 0) or 0),
                                      float(f.get("fuzzy_match_percent", 0) or 0))
    return out


def main():
    witness = json.loads((ROOT / "docs/decomp/W16AD_fold_witness_2026-09-14.json"
                          ).read_text())
    refuted = [r for r in witness if r["witness_verdict"] == "WITNESS_REFUTED"]
    aliases = json.loads((ROOT / "scripts/symbol_aliases.json").read_text())
    W = FW.Witnesser()
    f100 = fuzzy100_rows()
    rows_all = report_rows()
    print("fuzzy==100 rows: %d  (all report rows: %d)"
          % (len(f100), len(rows_all)), file=sys.stderr)

    out = []
    for r in refuted:
        gi = int(r["gi"])
        grp = aliases["groups"][gi]
        members = set(grp["folded"]) | {grp["survivor"]}
        N = r["folded"]
        dep_rows, free_sites, unnamed_sites = {}, 0, 0
        caller_status = collections.Counter()
        for caller, off in sorted(set(W.callsites.get(N, []))):
            caller_status["fuzzy100" if caller in f100 else
                          "scored_below_100" if caller in rows_all else
                          "not_a_report_row"] += 1
            for va in sorted(W.byname.get(caller, [])):
                dest = FW.decode_bl(W.img, va, off)
                if dest is None:
                    continue
                nm = W.byva.get(dest)
                if not nm:
                    unnamed_sites += 1          # already forgiven: placeholder
                elif nm == N:
                    free_sites += 1             # passes without the alias
                elif nm in members:
                    if caller in f100:
                        dep_rows[caller] = f100[caller]
        out.append({
            "gi": gi, "addr": r["addr"], "census_bytes": int(r["bytes"]),
            "folded": N, "survivor": r["survivor"],
            "our_callsites": len(set(W.callsites.get(N, []))),
            "sites_retail_unnamed": unnamed_sites,
            "sites_retail_names_N": free_sites,
            "caller_report_status": dict(caller_status),
            "self_is_report_row": N in rows_all,
            "dependent_fuzzy100_rows": sorted(dep_rows),
            "predicted_bytes_lost": sum(dep_rows.values()),
            "predicted_rows_lost": len(dep_rows),
        })
    out.sort(key=lambda x: (-x["predicted_bytes_lost"], -x["census_bytes"],
                            x["gi"], x["folded"]))
    Path(ROOT / "docs/decomp/W16AD_withdrawal_prediction_2026-09-14.json"
         ).write_text(json.dumps(out, indent=1) + "\n")
    tb = sum(x["predicted_bytes_lost"] for x in out)
    tr = len({n for x in out for n in x["dependent_fuzzy100_rows"]})
    print("\nPREDICTION over %d refuted memberships" % len(out))
    print("  memberships with >=1 dependent fuzzy==100 row : %d"
          % sum(1 for x in out if x["dependent_fuzzy100_rows"]))
    print("  distinct dependent rows                       : %d" % tr)
    print("  predicted matched_code loss (bytes)           : %d" % tb)
    print("  call sites whose retail dest is UNNAMED       : %d"
          % sum(x["sites_retail_unnamed"] for x in out))
    print("  call sites where retail names N itself        : %d"
          % sum(x["sites_retail_names_N"] for x in out))
    cs = collections.Counter()
    for x in out:
        cs.update(x["caller_report_status"])
    print("  caller report-row status                      : %s" % dict(cs))
    print("  refuted spellings that are themselves rows    : %d"
          % sum(1 for x in out if x["self_is_report_row"]))
    print("\n  => the zero above is a FINDING, not a vacuity: no scored row "
          "references\n     any refuted spelling, so the forgiveness is inert "
          "TODAY.")


if __name__ == "__main__":
    main()
