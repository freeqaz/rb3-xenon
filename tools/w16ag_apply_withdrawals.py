#!/usr/bin/env python3
"""W16-AG: withdraw retail-refuted alias memberships, in batches, with evidence.

Same shape as `tools/w16ad_apply_withdrawals.py` -- the group is KEPT, the
spelling moves out of `folded` and into a `withdrawn` record carrying the
retail addresses that refuted it.  A membership removed without that record is
a clobber, and the records are what let GROUNDED-2 restore six of GROUNDED-1's
withdrawals once STLPORT-1 showed their premise was a reader artifact.

⛔ The group is resolved by `(survivor, address)`.  The census `gi` is NOT an
index into groups[] -- measured here over this lane's own input: 0 of 63
WITNESS_REFUTED rows have gi equal to the true index, and 0 have their folded
spelling present in groups[gi].  Keying on gi appends records to unrelated
groups and removes nothing (removal filters by name), leaving the file looking
edited and the aliases untouched.

Two membership classes, distinguished in the record:
  RETAIL_WITNESS_REFUTED               -- the witness refuted it outright.
  RETAIL_WITNESS_REFUTED_GUARD_CLEARED -- the witness refuted it and W16-AD's
      INCONCLUSIVE_TWIN guard blocked it; the guard's TRUE refusal criterion
      (bodies identical INCLUDING call targets, CD-7's 51-surplus class) is
      measured NOT met, so the block was a false refusal.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALI = ROOT / "scripts" / "symbol_aliases.json"
WIT = ROOT / "docs/decomp/W16AG_fold_witness_2026-09-14.json"
ADJ = ROOT / "docs/decomp/W16AG_guard_adjudication_2026-09-14.json"
LANE = "W16-AG 2026-09-14"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("selection")
    ap.add_argument("--start", type=int, default=0)
    ap.add_argument("--count", type=int, default=20)
    ap.add_argument("--out")
    a = ap.parse_args()
    if a.count > 20:
        sys.exit("REFUSING: batch cap is 20")

    sel = json.loads(Path(a.selection).read_text())
    sel.sort(key=lambda r: (r["class"], -int(r["bytes"]), int(r["gi"]),
                            r["folded"]))
    batch = sel[a.start:a.start + a.count]
    if not batch:
        sys.exit("REFUSING: empty batch")

    wit = {(r["survivor"], r["addr"], r["folded"]): r
           for r in json.loads(WIT.read_text())}
    adj = {(r["survivor"], r["addr"], r["folded"]): r
           for r in json.loads(ADJ.read_text())}
    ali = json.loads(ALI.read_text())
    gkey = {(g["survivor"], g.get("address")): i
            for i, g in enumerate(ali["groups"])}

    applied = []
    for s in batch:
        key = (s["survivor"], s["addr"], s["folded"])
        r = wit[key]
        i = gkey.get((s["survivor"], s["addr"]))
        if i is None:
            sys.exit("REFUSING: no group for survivor=%s addr=%s"
                     % (s["survivor"][:60], s["addr"]))
        g = ali["groups"][i]
        if s["folded"] not in g["folded"]:
            sys.exit("REFUSING: %s absent from group %d's folded list -- the "
                     "key is wrong" % (s["folded"][:70], i))
        guard_cleared = s["class"] == "GUARD_CLEARED"
        want = "INCONCLUSIVE_TWIN" if guard_cleared else "REFUTED"
        p = [q for q in r["pairs"] if q.get("verdict") == want][0]
        wN = (p["N_witness"] or [{}])[0]
        wS = (p["S_witness"] or [{}])[0]
        ev = ("RETAIL KEEPS THE DISCRIMINATING CALLEES APART. At +%d our folded "
              "spelling calls %s and the survivor calls %s. Retail places them "
              "at %s and %s respectively: %s is witnessed by map-named caller "
              "%s @%s +%d (tier %s), %s by %s @%s +%d (tier %s). Retail's own "
              "body at X=%s calls %s at +%d, so X IS the survivor and our "
              "spelling is not the body there. Guards: both destinations are "
              ".pdata BeginAddresses (%s / %s B)."
              % (p["offset"], p["c_N"], p["c_S"],
                 p["N_strong"][0], p["S_strong"][0], p["N_strong"][0],
                 wN.get("caller", "?"), wN.get("caller_va", "?"),
                 wN.get("off", -1), wN.get("tier", "?"),
                 p["S_strong"][0], wS.get("caller", "?"),
                 wS.get("caller_va", "?"), wS.get("off", -1),
                 wS.get("tier", "?"), s["addr"], p["retail_dest_at_X"],
                 p["offset"], p["guards"].get("a_size"),
                 p["guards"].get("b_size")))
        rec = {
            "spelling": s["folded"],
            "lane": LANE,
            "class": ("RETAIL_WITNESS_REFUTED_GUARD_CLEARED" if guard_cleared
                      else "RETAIL_WITNESS_REFUTED"),
            "disposition": "withdrawn",
            "witness_c_N_retail_addr": p["N_strong"][0],
            "witness_c_S_retail_addr": p["S_strong"][0],
            "evidence": ev,
        }
        if guard_cleared:
            d = adj[key]
            rec["guard_adjudication"] = (
                "W16-AD's guard called this INCONCLUSIVE_TWIN because the two "
                "retail bodies compare identical under masked_body, which masks "
                "branch displacements AND imm16. The guard exists for CD-7's "
                "51-surplus class: bodies identical INCLUDING call targets, "
                "which /OPT:ICF could have folded and did not, and for which two "
                "addresses prove nothing. That criterion is NOT met here -- %s "
                "Measured with branches landing inside the extent compared by "
                "offset-from-base and branches leaving it compared by absolute "
                "destination, because raw words are vacuous for duplicates "
                "(PC-relative displacements differ at different addresses) and "
                "plain absolute resolution mislabels internal control flow as a "
                "changed callee. The strict test is shown able to return TWIN: "
                "over 57,733 retail .pdata functions it finds 1,529 "
                "distinct-address groups identical including call targets."
                % d.get("why", ""))
            rec["guard_strict_verdict"] = d["strict"]
            rec["guard_diff_words"] = d.get("n_diff_words")
        g["folded"] = [x for x in g["folded"] if x != s["folded"]]
        g.setdefault("withdrawn", []).append(rec)
        applied.append({"group_index": i, "census_gi": s["gi"],
                        "addr": s["addr"], "bytes": int(s["bytes"]),
                        "class": rec["class"], "spelling": s["folded"]})

    ALI.write_text(json.dumps(ali, indent=1) + "\n")
    print("withdrew %d memberships over %d groups (%d B census)"
          % (len(applied), len({x["group_index"] for x in applied}),
             sum(x["bytes"] for x in applied)))
    for x in applied:
        print("  group[%d] (census gi=%s) %-38s %s"
              % (x["group_index"], x["census_gi"], x["class"],
                 x["spelling"][:70]))
    if a.out:
        Path(a.out).write_text(json.dumps(applied, indent=1) + "\n")


if __name__ == "__main__":
    main()
