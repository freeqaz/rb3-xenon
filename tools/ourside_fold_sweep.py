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

⛔ DO NOT DROP GROUPS ON icf_alias_finder.py --validate CHECK (c).  MEASURED.
------------------------------------------------------------------------------
Check (c) -- "the survivor must be the ONLY group member named in the live dtk
target objs (a real ICF fold keeps one spelling)" -- fails on 11 of this lane's
54 groups with `target objs name []`, i.e. it finds NEITHER spelling.  That
reads like an unwitnessed, therefore inert, group.

It is not.  The prediction "unwitnessed in live target objs => the alias can
never fire => dropping them is inert" was stated first and then MEASURED, and it
is FALSE: dropping those 11 groups cost

    name_check  34.266037% -> 34.201584%   -0.064453pp / -6,652 B
    units at 100% (fuzzy)  119 -> 117      2 FELL OFF

So the aliases do fire and check (c) is returning a FALSE NEGATIVE -- its index
(`target_obj_symbol_index`, live-filtered `coff_referenced_symbols`) misses
references that objdiff's reloc_eq plainly sees.  Note the same check fails on
391 of the 1,671 PRE-EXISTING groups (23%), a rate this lane's 20% matches, so
the defect is in the instrument and long-standing, not in these groups.

The substantive gates (1-5 above) are what adjudicate a group.  Check (c) is a
COVERAGE property of our own pinning -- whether a split obj happens to name the
symbol -- and says nothing about whether retail's linker folded anything.
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
    ap.add_argument("--install-min-relocs", type=int, default=1,
                    help="withhold from --install any ADMITted pair whose shared "
                         "body carries fewer than this many relocations. Default 1: "
                         "a zero-relocation body (a bare `blr`, or a 2-3 instruction "
                         "accessor) is what an UNIMPLEMENTED STUB in our tree also "
                         "compiles to, so the defect the gate cannot rule out is "
                         "exactly what would admit it. NARROWING ONLY -- never "
                         "widens the gate. 0 restores the old blanket behaviour.")
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
        # ---- INSTALL-SIDE CLASS FILTER.  NARROWING ONLY -- it can remove an
        # ADMIT from the install set, never add one, and it never runs before or
        # inside the gate above.  It implements the policy this file's own
        # docstring already states ("a row whose only evidence is our-side
        # identity on a SHORT body, where coincidental identity is cheap, is
        # reported, not installed") which --min-size defaulting to 0 did not.
        #
        # WHY RELOCATION COUNT IS THE RIGHT AXIS, measured (lane INSTALL-1):
        # the failure mode this gate cannot rule out is "our body for F is
        # WRONG in a way that coincidentally equals the survivor's".  The
        # overwhelmingly common instance of that is an UNIMPLEMENTED STUB, and a
        # stub compiles to exactly `blr` -- 4 bytes, ZERO relocations.  So the
        # zero-reloc class is precisely where the defect MANUFACTURES the
        # identity that admits it.  Measured pool sizes in our own build: 7,535
        # distinct code COMDATs are byte-identical to a bare `blr`, versus 384
        # for `b ?MemFree@@YAXPAX@Z` and 40 for `b ??1String@@UAA@XZ`.  A stub
        # cannot compile to a tail-call thunk carrying a relocation to a NAMED
        # symbol, so that class is structurally out of the stub's reach.
        # Concrete case withheld by this filter: survivor
        # ??0?$reverse_iterator@PAPAVSynchronizable@@@stlpmtx_std@@QAA@ABV01@@Z
        # folded with ?LiteralSym@DataNode@@QBA?AVSymbol@@PBVDataArray@@@Z --
        # unrelated functions that coincide only as a trivial 12-byte copy.
        # `--install-min-relocs 0` restores the blanket behaviour.
        def _nrelocs(name):
            (_raw, rel), _objs = next(iter(ours[name].items()))
            return len(rel)

        withheld = [o for o in adm if _nrelocs(o["folded"]) < args.install_min_relocs]
        adm_i = [o for o in adm if _nrelocs(o["folded"]) >= args.install_min_relocs]
        if withheld:
            wc = collections.Counter(
                "%dB/%drel" % (o["size"], _nrelocs(o["folded"])) for o in withheld)
            print(f"\nWITHHELD by --install-min-relocs {args.install_min_relocs}: "
                  f"{len(withheld)} ADMITted pairs / "
                  f"{sum(o['sites'] for o in withheld)} sites -- {dict(wc)}")
            print("  (gate-ADMITted and DELIBERATELY not installed; see the note "
                  "above. These are reported, not aliased away.)")

        groups = collections.defaultdict(list)
        for o in adm_i:
            groups[(o["survivor"], o["addr"])].append(o["folded"])
        # One group per ADDRESS is this file's invariant (1,493 groups /
        # 1,493 distinct addresses at b574f653) and it is also the RENDERED
        # semantics: tools/gen_symbol_alias_map.py emits one map line per symbol
        # at the group's address, and objdiff's parse_msvc_map groups every
        # symbol sharing an address.  So a second group at A would be
        # semantically identical to merging -- merging is what keeps the file's
        # invariant true.
        by_addr = {g["address"].lower(): g for g in ali["groups"]}
        n = added = merged = skipped = 0
        for (S, A), folded in sorted(groups.items()):
            folded = sorted(set(folded))
            for f in folded:                       # injectivity, asserted
                assert not (byname.get(f, set()) - {int(A, 16)}), f
                assert not (in_group.get(f, set()) - {A}), f
                assert f != S
            g = by_addr.get(A)
            if g is not None:
                # ---- THE BUG THIS REPLACES: `if A in existing: continue`.
                # It refused to merge a newly-admitted spelling into a group that
                # already existed at that address, stranding 289 ADMITted pairs /
                # 721 sites tree-wide (lane INSTALL-1; 288-289 of them from this
                # one clause).  The original concern -- "never silently reshape a
                # group another tier owns" -- is REAL and is preserved exactly,
                # by the membership guard below rather than by dropping the pair.
                #
                # Every name in a rendered group is MUTUALLY equivalent, so
                # adding F to the group at A asserts F == (every member).  The
                # gate proved F == S and nothing else, so the merge is only
                # sound when S is ALREADY a member of that group -- then F == S
                # is exactly what the group already says.  Measured at
                # b574f653: S is a member in 638 of 638 stranded pairs (100%),
                # so this guard withholds nothing today; it is here so the tool
                # cannot invent an equivalence a future group's survivor would
                # not support.
                members = set([g["survivor"]] + list(g.get("folded", [])))
                if S not in members:
                    print(f"  SKIP merge at {A}: survivor {S[:48]} is not a member "
                          f"of the group already there (owner {g.get('name')}); "
                          f"the gate proved F == S, not F == that group")
                    skipped += len(folded)
                    continue
                fresh = [f for f in folded if f not in members]
                if not fresh:
                    continue
                g["folded"] = sorted(set(g.get("folded", [])) | set(fresh))
                g.setdefault("merged_in", []).append({
                    "name": "ourside_comdat_identity",
                    "survivor": S, "folded": fresh,
                    "evidence": ("tools/ourside_fold_sweep.py --install: our "
                                 "COMDAT for each of these is byte- AND "
                                 "relocation-identical to our COMDAT for %s, "
                                 "which the map places at %s -- a member of this "
                                 "group. Retail fan-in there is %d against %d "
                                 "sites. Merged into the pre-existing group at "
                                 "this address rather than dropped (the tool "
                                 "previously dropped these outright)."
                                 % (S, A, fanin.get(int(A, 16), 0), demand[A])),
                })
                merged += 1
                added += len(fresh)
                continue
            newg = {
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
            }
            ali["groups"].append(newg)
            by_addr[A] = newg
            n += 1
            added += len(folded)
        p = ROOT / "scripts" / "symbol_aliases.json"
        p.write_text(json.dumps(ali, indent=1) + "\n")
        print(f"\ninstalled {added} folded spellings into {p}"
              f"  ({n} new group(s), {merged} merged into an existing group"
              + (f", {skipped} skipped on the membership guard" if skipped else "")
              + ")")


if __name__ == "__main__":
    main()
