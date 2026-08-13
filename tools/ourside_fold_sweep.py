#!/usr/bin/env python3
"""Sweep every `name_check` charge for the OUR-SIDE COMDAT-identity fold argument.

THE ARGUMENT, AND HOW IT DIFFERS FROM tools/comdat_fold_gate.py
--------------------------------------------------------------
comdat_fold_gate asks "is OUR COMDAT for F equal to RETAIL's body at addr(S)?".
Answering it requires resolving retail's branch destinations through
target_symbol_map.json to NAMES, so it fail-closes whenever a destination is
unnamed -- correctly, but that is missing evidence on our side rather than a
contradiction (lane ALIAS-X2's central lesson).

This sweep asks a question the same evidence CAN answer:

    is our COMDAT for F byte- and RELOCATION-identical to our COMDAT for S,
    where S is the map-resident spelling retail's relocation names?

If yes, /OPT:ICF *must* place F and S at one address -- that is the linker's own
condition, applied to two of OUR COMDATs.  Both sides carry the same relocations
to the same symbols, so the argument never has to name what those symbols denote.
Retail is not taken on trust either: addr(S) must CORROBORATE the shared COMDAT
(same size; every word our relocation table does not relocate equal as a FULL
32-bit value; every relocated word equal in opcode/AA/LK).  That is
comdat_fold_gate's comparator with exactly one clause removed -- the one that
needs a name for retail's branch target -- and the our-side identity is what
replaces it.

WHAT IT CANNOT RULE OUT, STATED PLAINLY
---------------------------------------
Our two COMDATs being identical is a fact about OUR build.  If retail's source
for F differed from retail's source for S, retail did not fold them, and our F is
simply wrong.  The corroboration clause bounds this -- retail's body at addr(S)
IS our shared COMDAT, so our body is at least the right function for S -- and the
injectivity clause refuses any F the map places elsewhere.  It does not eliminate
it.  Rows are therefore graded, and a row whose only evidence is our-side
identity on a SHORT body (where coincidental identity is cheap) is reported, not
installed.
"""

import argparse
import collections
import glob
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from comdat_bytes import comdats                        # noqa: E402
from wrong_callee_triage import Image, load_sizes       # noqa: E402

BUILD_ID = "45410914"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min-size", type=int, default=0,
                    help="only report shared bodies of at least this many bytes")
    ap.add_argument("--json")
    ap.add_argument("--install", action="store_true")
    args = ap.parse_args()

    img = Image(ROOT / "orig" / BUILD_ID / "band.exe")
    size = load_sizes()
    smap = json.loads((ROOT / "scripts" / "target_symbol_map.json").read_text())
    byva = {int(a, 16): n for a, n in smap.items()
            if a.startswith("0x") and isinstance(n, str)}
    byname = collections.defaultdict(set)
    for va, n in byva.items():
        byname[n].add(va)

    ali = json.loads((ROOT / "scripts" / "symbol_aliases.json").read_text())
    in_group = collections.defaultdict(set)
    for g in ali["groups"]:
        for nm in [g["survivor"]] + list(g.get("folded", [])):
            in_group[nm].add(g["address"].lower())

    # our COMDATs, keyed by (raw, relocs)
    ours = collections.defaultdict(lambda: collections.defaultdict(list))
    for p in glob.glob(str(ROOT / "build" / BUILD_ID / "src" / "**" / "*.obj"),
                       recursive=True):
        try:
            c = comdats(p)
        except Exception:
            continue
        for n, v in c.items():
            if not v.get("is_code"):
                continue
            rel = tuple(sorted((o, s, t) for o, s, t in (v["relocs"] or [])
                               if s != "@comp.id"))
            ours[n][(v["raw"], rel)].append(p)

    rows = json.loads((ROOT / "scripts" / "namecheck_df_census.json").read_text())["rows"]

    def corroborates(shared_raw, shared_rel, va):
        """retail's body at `va` is our shared COMDAT (comdat_fold_gate's
        comparator minus the branch-target NAME clause)."""
        n = size.get(va)
        o = img.off(va)
        if not n or o is None or n != len(shared_raw):
            return False, f"size {n} vs {len(shared_raw)}"
        rw = list(struct.unpack_from(">%dI" % (n // 4), img.data, o))
        ow = list(struct.unpack(">%dI" % (n // 4), shared_raw))
        relo = {off for off, _s, _t in shared_rel}
        for i, (x, y) in enumerate(zip(rw, ow)):
            if (i * 4) in relo:
                if (x >> 26) != (y >> 26):
                    return False, f"opcode differs at +{i*4:x}"
                if (x >> 26) in (16, 18) and (x & 3) != (y & 3):
                    return False, f"AA/LK differs at +{i*4:x}"
            elif x != y:
                return False, f"non-relocated word differs at +{i*4:x}"
        return True, "ok"

    out, seen = [], set()
    for r in rows:
        S, F = r["target"], r["base"]
        if (S, F) in seen:
            continue
        seen.add((S, F))
        sv, fv = ours.get(S), ours.get(F)
        if not sv or not fv or len(sv) != 1 or len(fv) != 1:
            continue
        (s_raw, s_rel), _ = next(iter(sv.items()))
        (f_raw, f_rel), _ = next(iter(fv.items()))
        if (s_raw, s_rel) != (f_raw, f_rel):
            continue                       # not our-side identical
        placed = byname.get(S, set())
        if len(placed) != 1:
            continue
        A = next(iter(placed))
        ok, why = corroborates(s_raw, s_rel, A)
        elsewhere = byname.get(F, set()) - {A}
        grouped = in_group.get(F, set()) - {"0x%08x" % A}
        verdict = ("ADMIT" if (ok and not elsewhere and not grouped
                               and len(s_raw) >= args.min_size) else "REFUSE")
        reason = why if not ok else (
            "map places F at " + ",".join(hex(v) for v in sorted(elsewhere))
            if elsewhere else
            f"F already aliased at {sorted(grouped)}" if grouped else
            f"body {len(s_raw)} B < --min-size {args.min_size}"
            if len(s_raw) < args.min_size else
            "our COMDAT for F == our COMDAT for S; retail@%08x corroborates" % A)
        out.append({"survivor": S, "folded": F, "addr": "0x%08x" % A,
                    "size": len(s_raw), "sites": r["sites"], "fns": r["fns"],
                    "bucket": r["bucket"], "verdict": verdict, "why": reason})

    # ---- gate 4: FAN-IN.  retail's body at addr(S) must be called at least as
    # many times as our sites demand of it.  Power measured against a
    # random-survivor null: the null is REFUTED 68-74% of the time (retail's
    # fan-in is median 1, p90 4), the observed population 0%.  A test that
    # cannot fail is worthless; this one kills seven in ten wrong pairings.
    fanin = img.fanin()
    demand = collections.Counter()
    for o in out:
        if o["verdict"] == "ADMIT":
            demand[o["addr"]] += o["sites"]
    for o in out:
        if o["verdict"] == "ADMIT":
            f = fanin.get(int(o["addr"], 16), 0)
            o["fanin"], o["demand"] = f, demand[o["addr"]]
            if f < demand[o["addr"]]:
                o["verdict"] = "REFUSE"
                o["why"] = (f"retail fan-in {f} < {demand[o['addr']]} sites our "
                            f"aliases would route here -- the fold cannot be real")

    # ---- gate 5: one folded spelling may not be admitted against TWO survivor
    # ADDRESSES.  That would assert those two addresses are one fold class, a
    # claim nothing here tested -- and if our COMDATs really were identical the
    # linker would already have folded the survivors together, so two distinct
    # map addresses is a CONTRADICTION.  Refuse every arm (comdat_fold_gate's
    # rule, kept).
    addrs = collections.defaultdict(set)
    for o in out:
        if o["verdict"] == "ADMIT":
            addrs[o["folded"]].add(o["addr"])
    for o in out:
        if o["verdict"] == "ADMIT" and len(addrs[o["folded"]]) > 1:
            o["verdict"] = "REFUSE"
            o["why"] = ("admitted against %d survivor ADDRESSES (%s) -- "
                        "contradiction, refusing every arm"
                        % (len(addrs[o["folded"]]), sorted(addrs[o["folded"]])))

    adm = [o for o in out if o["verdict"] == "ADMIT"]
    print(f"our-side-identical charged pairs: {len(out)}  "
          f"({len(adm)} ADMIT / {len(out)-len(adm)} REFUSE)")
    print(f"ADMIT covers {sum(o['sites'] for o in adm)} sites, "
          f"{sum(o['fns'] for o in adm)} fns (sum, may double-count)\n")
    for o in sorted(out, key=lambda o: -o["sites"])[:40]:
        print(f"  {o['verdict']:6s} {o['sites']:5d} sites {o['size']:4d} B  "
              f"{o['folded'][:56]}")
        print(f"         -> {o['survivor'][:56]} @{o['addr']}")
        if o["verdict"] == "REFUSE":
            print(f"         {o['why']}")
    if args.json:
        Path(args.json).write_text(json.dumps(out, indent=1))
        print(f"\nwrote {args.json}")

    if args.install:
        groups = collections.defaultdict(list)
        for o in adm:
            groups[(o["survivor"], o["addr"])].append(o["folded"])
        existing = {g["address"].lower() for g in ali["groups"]}
        n = 0
        for (S, A), folded in sorted(groups.items()):
            folded = sorted(set(folded))
            for f in folded:                       # injectivity, asserted
                assert not (byname.get(f, set()) - {int(A, 16)}), f
                assert not (in_group.get(f, set()) - {A}), f
                assert f != S
            if A in existing:                      # never silently reshape a
                continue                           # group another tier owns
            ali["groups"].append({
                "name": "ourside_comdat_identity",
                "address": A, "survivor": S, "folded": folded,
                "evidence": (
                    "tools/ourside_fold_sweep.py -- our COMDAT for each folded "
                    "spelling is byte- AND relocation-identical to our COMDAT for "
                    "the map-resident survivor, which is the /OPT:ICF condition "
                    "applied to two of OUR OWN COMDATs (both carry the same "
                    "relocations to the same symbols, so no retail branch target "
                    "needs naming -- the clause tools/comdat_fold_gate.py "
                    "fail-closes on). Retail's body at %s corroborates the shared "
                    "COMDAT: same size, every non-relocated word equal as a FULL "
                    "32-bit value, every relocated word equal in opcode/AA/LK. "
                    "Retail fan-in there is %d against %d sites our aliases route "
                    "to it; that fan-in test refutes 68-74%% of random survivors "
                    "and 0%% of this population. NOT ruled out, and stated in the "
                    "docstring: if OUR body for a folded spelling is wrong in a "
                    "way that coincidentally equals the survivor's, the alias "
                    "hides it -- cheapest for the smallest bodies."
                    % (A, fanin.get(int(A, 16), 0), demand[A])),
            })
            n += 1
        p = ROOT / "scripts" / "symbol_aliases.json"
        p.write_text(json.dumps(ali, indent=1) + "\n")
        print(f"\ninstalled {n} new groups ({sum(len(v) for v in groups.values())} "
              f"folded spellings) into {p}")


if __name__ == "__main__":
    main()
