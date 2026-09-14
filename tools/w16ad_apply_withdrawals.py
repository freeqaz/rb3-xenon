#!/usr/bin/env python3
"""W16-AD: withdraw the retail-refuted alias memberships, with their evidence.

⛔ RESOLVE THE GROUP BY (survivor, address), NEVER BY THE CENSUS'S `gi`.
Measured 2026-09-14: only 503 of 5,315 census rows have gi equal to the true
index into scripts/symbol_aliases.json groups[].  Keying on gi here would have
appended `withdrawn` records to unrelated groups while removing nothing (the
removal filters by name, so a wrong group is a silent no-op) -- a clobber that
leaves the file looking edited and the aliases untouched.

A withdrawal is NOT a prune: the group is kept, the spelling is recorded in
`withdrawn[]` with the retail addresses that refuted it, and the decision stays
auditable and reversible.  That distinction is what the house rule protects --
six of GROUNDED-1's ten withdrawals were later RESTORED once STLPORT-1 showed
their premise was a one-sided reader artifact, and the records are what made
that possible.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALI = ROOT / "scripts" / "symbol_aliases.json"
LANE = "W16-AD 2026-09-14"
LIMIT = 20


def main():
    wit = json.loads((ROOT / "docs/decomp/W16AD_fold_witness_2026-09-14.json"
                      ).read_text())
    ref = [r for r in wit if r["witness_verdict"] == "WITNESS_REFUTED"]
    ref.sort(key=lambda r: (-int(r["bytes"]), int(r["gi"]), r["folded"]))
    sel, rest = ref[:LIMIT], ref[LIMIT:]

    ali = json.loads(ALI.read_text())
    gkey = {(g["survivor"], g.get("address")): i
            for i, g in enumerate(ali["groups"])}

    applied = []
    for r in sel:
        i = gkey.get((r["survivor"], r["addr"]))
        if i is None:
            sys.exit("REFUSING: no group for survivor=%s addr=%s"
                     % (r["survivor"][:60], r["addr"]))
        g = ali["groups"][i]
        if r["folded"] not in g["folded"]:
            sys.exit("REFUSING: %s absent from group %d's folded list -- the "
                     "key is wrong" % (r["folded"][:70], i))
        p = [q for q in r["pairs"] if q["verdict"] == "REFUTED"][0]
        wN = (p["N_witness"] or [{}])[0]
        wS = (p["S_witness"] or [{}])[0]
        ev = ("RETAIL KEEPS THE DISCRIMINATING CALLEES APART. At +%d our folded "
              "spelling calls %s and the survivor calls %s. Retail places them "
              "at %s and %s respectively: %s is witnessed by map-named caller "
              "%s @%s +%d (tier %s), %s by %s @%s +%d (tier %s). Retail's own "
              "body at X=%s calls %s at +%d, so X IS the survivor and our "
              "spelling is not the body there. Guards: both destinations are "
              ".pdata BeginAddresses (%s / %s B) and their relocation-"
              "normalised bodies are NOT identical, so this is not the "
              "unfolded-duplicate (CD-7 51-surplus) class."
              % (p["offset"], p["c_N"], p["c_S"],
                 p["N_strong"][0], p["S_strong"][0], p["N_strong"][0],
                 wN.get("caller", "?"), wN.get("caller_va", "?"),
                 wN.get("off", -1), wN.get("tier", "?"),
                 p["S_strong"][0], wS.get("caller", "?"),
                 wS.get("caller_va", "?"), wS.get("off", -1),
                 wS.get("tier", "?"), r["addr"], p["retail_dest_at_X"],
                 p["offset"], p["guards"].get("a_size"),
                 p["guards"].get("b_size")))
        g["folded"] = [x for x in g["folded"] if x != r["folded"]]
        g.setdefault("withdrawn", []).append({
            "spelling": r["folded"],
            "lane": LANE,
            "class": "RETAIL_WITNESS_REFUTED",
            "disposition": "withdrawn",
            "witness_c_N_retail_addr": p["N_strong"][0],
            "witness_c_S_retail_addr": p["S_strong"][0],
            "evidence": ev,
        })
        applied.append((i, r["gi"], r["folded"]))

    ALI.write_text(json.dumps(ali, indent=1) + "\n")
    print("withdrew %d memberships over %d groups"
          % (len(applied), len({a[0] for a in applied})))
    for gidx, gi, n in applied:
        print("  group[%d] (census gi=%d)  %s" % (gidx, gi, n[:84]))
    Path(ROOT / "docs/decomp/W16AD_withdrawn_20_2026-09-14.json").write_text(
        json.dumps({"applied": [{"group_index": a[0], "census_gi": a[1],
                                 "spelling": a[2]} for a in applied],
                    "not_landed_refuted": [{"census_gi": r["gi"],
                                            "addr": r["addr"],
                                            "bytes": int(r["bytes"]),
                                            "folded": r["folded"],
                                            "survivor": r["survivor"]}
                                           for r in rest]}, indent=1) + "\n")
    print("\n%d further REFUTED memberships NOT landed (batch cap), listed in "
          "docs/decomp/W16AD_withdrawn_20_2026-09-14.json" % len(rest))


if __name__ == "__main__":
    main()
