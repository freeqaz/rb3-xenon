#!/usr/bin/env python3
"""W16-AD Item 3: SIMULATE the Accomplishment map-row swap before building it.

⛔ WHY A SIMULATION AND NOT AN ARGUMENT.  The cluster's map names are wrong
CONSISTENTLY: a caller row and its callee row are mislabelled in the same
direction, so their relocation NAMES agree and `name_check` charges nothing.
Renaming only the callee breaks the cancellation and manufactures a charge on a
row that reads 100 today.  7 of 16 caller sites in this cluster are in exactly
that state, so "the map is wrong, therefore fixing it pays" is false here.

The simulation replays `name_check`'s own question -- does our relocation's
target NAME equal the map name of the address retail branches to -- over every
call site in the cluster, under the CURRENT map and under the PROPOSED map, and
reports per row whether its charge count goes to zero (row crosses to
`fuzzy==100`, bytes gained) or off zero (row falls, bytes lost).

⚠ It deliberately prices only SITES IT CAN SEE: our caller must be a report row
AND map-resident, and retail's word at the offset must decode as a b/bl.  A
site it cannot see is reported, not silently dropped -- an invisible site is the
difference between a prediction and a guess.
"""
import collections
import glob
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from comdat_bytes import comdats                   # noqa: E402
from wrong_callee_triage import Image              # noqa: E402

BUILD = "45410914"

# the four mutual, both-charged, same-obj swap pairs proven by
# tools/w16ad_item3_adjudicate.py
SWAP_PAIRS = [
    (0x825fbd78, 0x825fb6c8),   # __merge_without_buffer      304 B
    (0x825f7250, 0x825f71a0),   # __unguarded_linear_insert    84 B
    (0x825f76a8, 0x825f74e8),   # __upper_bound               112 B
    (0x825f82d8, 0x825f8148),   # merge                       196 B
]
CLUSTER = ("AccomplishmentCmp", "AccomplishmentCategoryCmp",
           "AccomplishmentGroupCmp", "GoalCmp", "GoalAlpaCmp")


def load():
    smap = json.loads((ROOT / "scripts/target_symbol_map.json").read_text())
    byname, byva = {}, {}
    for k, v in smap.items():
        try:
            va = int(k, 16)
        except ValueError:
            continue
        if isinstance(v, str):
            byname[v] = va
            byva[va] = v
    return smap, byname, byva


def main():
    smap, byname, byva = load()

    # proposed map = current map with the pairs' names exchanged
    prop = dict(byva)
    for a, b in SWAP_PAIRS:
        assert a in byva and b in byva, f"0x{a:08x}/0x{b:08x} not both mapped"
        prop[a], prop[b] = byva[b], byva[a]

    # our code COMDATs in the cluster and everything that calls into it
    ours = {}
    for p in glob.glob(str(ROOT / "build" / BUILD / "src/**/*.obj"), recursive=True):
        try:
            c = comdats(p)
        except Exception:
            continue
        for n, v in c.items():
            if v.get("is_code"):
                ours.setdefault(n, (v["fn_relocs"] or [], Path(p).name))

    rep = json.load(open(ROOT / "build" / BUILD / "report.json"))
    fz = {}
    for u in rep["units"]:
        for f in u.get("functions", []):
            fz[f.get("name", "")] = (float(f.get("fuzzy_match_percent", 0)),
                                     int(f.get("size", 0)), u["name"])

    img = Image(ROOT / "orig" / BUILD / "band.exe")

    def dec(va, off):
        o = img.off(va + off)
        if o is None:
            return None
        w = struct.unpack_from(">I", img.data, o)[0]
        if (w >> 26) != 18 or (w & 2):
            return None
        d = w & 0x03FFFFFC
        if d & 0x02000000:
            d -= 0x04000000
        return (va + off + d) & 0xFFFFFFFF

    renamed = {byva[a] for a, b in SWAP_PAIRS} | {byva[b] for a, b in SWAP_PAIRS}

    # ⛔ THE CALLER'S OWN ADDRESS MOVES TOO.  objdiff pairs target<->base BY
    # NAME, so after the swap our `<AccomplishmentCmp>` body is compared
    # against the OTHER retail address.  A simulation that holds the caller
    # address fixed is answering a different question -- it prices the swap as
    # if only callees moved, and reports a uniform regression that does not
    # exist.  (First draft of this tool did exactly that.)
    prop_byname = {}
    for va, nm in prop.items():
        prop_byname[nm] = va

    # every site in a cluster row, plus every site anywhere targeting a renamed
    # symbol -- those are the only sites whose verdict can move
    rows = collections.defaultdict(lambda: {"before": 0, "after": 0,
                                            "blind": 0, "sites": []})
    for n, (rel, obj) in ours.items():
        in_cluster = "stlpmtx_std" in n and any(c in n for c in CLUSTER)
        hits_renamed = any(s in renamed for _o, s, _t in rel)
        if not (in_cluster or hits_renamed):
            continue
        cva_b = byname.get(n)          # address this row pairs with TODAY
        cva_a = prop_byname.get(n)     # address it pairs with AFTER the swap
        if cva_b is None or cva_a is None:
            continue
        for o, s, _t in rel:
            if s == "@comp.id":
                continue
            if byname.get(s) is None and prop_byname.get(s) is None:
                continue          # our target unmapped: retail dest reads as a
                                  # placeholder, which name_check FORGIVES
            db, da = dec(cva_b, o), dec(cva_a, o)
            if db is None or da is None:
                rows[n]["blind"] += 1
                continue
            # ⛔ name_check FORGIVES a placeholder destination.  An address the
            # map does not name reaches objdiff as dtk's `fn_<addr>`, which
            # `is_placeholder_symbol_name` waives -- so an unnamed retail
            # destination is NOT a charge, it is an uncharged site.  Counting
            # it as a charge inflates both legs and, worse, invents charges on
            # rows that measurably read `fuzzy == 100` today (which is how this
            # error was caught: a 408 B row at 100 with "2 charges").
            rb, ra = byva.get(db), prop.get(da)
            cb = (rb is not None and rb != s)
            ca = (ra is not None and ra != s)
            rows[n]["before"] += cb
            rows[n]["after"] += ca
            if cb != ca:
                rows[n]["sites"].append((o, s, db, da, cb, ca))

    gain, loss, flat = [], [], []
    for n, r in rows.items():
        f = fz.get(n)
        if f is None:
            continue
        cur100 = f[0] == 100.0
        if r["before"] > 0 and r["after"] == 0 and not cur100:
            gain.append((n, f))
        elif r["before"] == 0 and r["after"] > 0 and cur100:
            loss.append((n, f))
        elif r["before"] != r["after"]:
            flat.append((n, f, r["before"], r["after"]))

    print("PRE-REGISTERED PREDICTION for the 4-pair (8 row) Accomplishment swap")
    print("=" * 78)
    print(f"\nROWS PREDICTED TO CROSS TO fuzzy==100  ({len(gain)}):")
    gb = 0
    for n, f in sorted(gain, key=lambda x: -x[1][1]):
        gb += f[1]
        print(f"   +{f[1]:5d} B  fz {f[0]:9.5f} -> 100  {n[:78]}")
    print(f"   ---- predicted GAIN  +{gb} B / +{len(gain)} rows")

    print(f"\nROWS PREDICTED TO FALL OFF fuzzy==100  ({len(loss)}):")
    lb = 0
    for n, f in sorted(loss, key=lambda x: -x[1][1]):
        lb += f[1]
        print(f"   -{f[1]:5d} B  fz 100 -> <100  {n[:78]}")
    print(f"   ---- predicted LOSS  -{lb} B / -{len(loss)} rows")

    print(f"\nROWS CHANGING CHARGE COUNT BUT NOT CROSSING  ({len(flat)}):")
    for n, f, b, a in sorted(flat, key=lambda x: -x[1][1]):
        print(f"    {f[1]:5d} B  fz {f[0]:9.5f}  charges {b} -> {a}  {n[:66]}")

    blind = sum(r["blind"] for r in rows.values())
    print(f"\nsites the simulation could not decode (reported, not dropped): {blind}")
    print(f"\nNET PREDICTION: {gb - lb:+d} B, {len(gain) - len(loss):+d} rows")


if __name__ == "__main__":
    main()
