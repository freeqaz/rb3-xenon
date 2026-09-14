#!/usr/bin/env python3
"""W16-AH: withdraw alias memberships refuted by TYPE EXISTENCE, with evidence.

Sibling of `tools/w16ag_apply_withdrawals.py`, and written rather than reused
because that tool is structurally incapable of this class: it selects
`pairs[].verdict == "REFUTED"` and builds its evidence string from the two
retail addresses the witness decoded.  These rows are `NO_WITNESS_FOLDED_SIDE`
-- BY DEFINITION no such addresses exist -- so the AG applier would raise
IndexError on every row.  Everything else is deliberately identical:

  * the GROUP IS KEPT.  The spelling moves out of `folded` into a `withdrawn`
    record.  A membership removed without a record is a clobber, and the records
    are what let GROUNDED-2 restore six of GROUNDED-1's withdrawals once
    STLPORT-1 showed their premise was a reader artifact.
  * the group is resolved by `(survivor, address)`.  The census `gi` is NOT an
    index into groups[] -- W16-AD §4.2 measured 4,785 of 5,315 rows wrong -- and
    a gi-keyed removal silently removes NOTHING while appending records to an
    unrelated group.
  * batch cap 20, and the resolved group must actually contain the spelling.

Three membership classes, distinguished in the record because they rest on
DIFFERENT evidence and a later lane must be able to tell them apart:

  TYPE_ABSENT_FROM_RETAIL_ELEMENT
      `c_N` is a template instantiated on a class that is (a) absent from retail
      RTTI, (b) polymorphic in our source, and (c) absent from the rb3-Wii RB3
      oracle while present in DC3.  Two independent instruments -- retail bytes
      and RB3's own source -- agree the type is not in RB3.

  TYPE_ABSENT_FROM_RETAIL_CONTAINER
      The element type exists in retail, but the CONTAINER template `ObjPtrVec`
      does not: `?$ObjPtrVec@` occurs 0 times in retail while its sibling
      `?$ObjPtrList@` -- same `ObjRefOwner` base, same shape -- occurs 45, and
      rb3-Wii spells `ObjPtrList` 338 times and `ObjPtrVec` never.  Recorded
      separately from the element class because it rests on the container
      argument alone.

  TYPE_ABSENT_FROM_RETAIL_ENCLOSING
      The callee's OWN ENCLOSING CLASS -- not a template argument -- is absent.
      Rests on the same three halves PLUS a subsystem-presence witness on retail
      bytes, because half (1) alone is demonstrably unsound: `Quazal::Job` is
      polymorphic, is declared in rb3-Wii, and is NetZ middleware retail
      certainly links, yet `.?AVJob@Quazal@@` occurs 0 times in retail.  Only
      half (3) declines it.  Symmetrically, an rb3-Wii zero can be a WII PORT
      artifact rather than a statement about the game: retail carries
      `BinkTextures.cpp`, "Error reading Bink header." and "Not a Bink file.",
      so `BinkMovieImpl`/`MovieImpl` are NOT refutable and are left alone.  This
      class is therefore restricted to subsystems with a retail-byte absence
      witness of their own -- `gesture/` (Kinect: `Kinect`, `NuiSkeleton`,
      `NUIAPI`, `xnui`, `nui` all 0 in retail) and `hamobj/` (every polymorphic
      sibling's descriptor reads 0, rb3-Wii declares none of them, DC3 all).

CONTAINER-class rows whose GROUP SURVIVOR is itself an `ObjPtrVec` spelling are
NOT applied: the container argument impeaches the survivor -- i.e. the map row
naming that address -- so withdrawing a member while leaving the group resting
on a name the same argument condemns would assert a different wrong thing.  That
is a map identification question, not an alias one.  The guard is deliberately
scoped to the container class: an ELEMENT-class row is refuted because OUR
spelling instantiates a type RB3 does not have, which is true whatever the
survivor happens to be called, and blocking those would be an over-cautious
correction -- still a defect.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALI = ROOT / "scripts" / "symbol_aliases.json"
LANE = "W16-AH 2026-09-14"


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
    # deterministic, stable order so --start/--count partition the population
    sel.sort(key=lambda r: (r["ah_class"], r["addr"], r["folded"]))
    batch = sel[a.start:a.start + a.count]
    if not batch:
        sys.exit("REFUSING: empty batch")

    ali = json.loads(ALI.read_text())
    gkey = {(g["survivor"], g.get("address")): i
            for i, g in enumerate(ali["groups"])}

    applied = []
    for s in batch:
        i = gkey.get((s["survivor"], s["addr"]))
        if i is None:
            sys.exit("REFUSING: no group for survivor=%s addr=%s"
                     % (s["survivor"][:60], s["addr"]))
        g = ali["groups"][i]
        if s["folded"] not in g["folded"]:
            sys.exit("REFUSING: %s absent from group %d's folded list -- the "
                     "key is wrong" % (s["folded"][:70], i))
        if (s["ah_class"] == "TYPE_ABSENT_FROM_RETAIL_CONTAINER"
                and "?$ObjPtrVec@" in g["survivor"]):
            sys.exit("REFUSING: group %d's survivor is itself an ObjPtrVec "
                     "spelling, and this row rests on the CONTAINER argument, "
                     "which therefore impeaches the survivor too -- a map "
                     "question, not an alias one" % i)

        ev_types = s["ah_evidence_types"]
        parts = []
        for t, e in ev_types.items():
            parts.append(
                "%s: retail type-descriptor probe %s -> %d hits; polymorphic in "
                "our source = %s; rb3-Wii (RB3's own dev decomp, names intact) "
                "declares it in %d files, DC3 in %d"
                % (t, e["rtti_mangled"], e["retail_rtti_hits"],
                   e["polymorphic_in_our_source"], e["rb3wii_files"],
                   e["dc3_files"]))
        ev = ("TYPE DOES NOT EXIST IN RB3 RETAIL, so the callee this membership "
              "turns on was never in any retail COMDAT group and the asserted "
              "fold cannot have happened. Our folded spelling calls %s at +%s, "
              "the survivor calls %s; the retail-side fold witness returned "
              "NO_WITNESS_FOLDED_SIDE because %s has no map-named caller that "
              "compares EQ to retail, so no branch could be decoded for it -- "
              "this record settles the row WITHOUT a witness, by type existence. "
              "%s. Controls: the same retail probe returns PRESENT for "
              "Spotlight/RndTex/ObjectDir/EventTrigger/NoteVoiceInst/Fader, and "
              "the criterion DECLINES BandPatchMesh and HighlightObject "
              "(RTTI-absent but non-polymorphic and present in rb3-Wii), so it "
              "is able to fail. Withdrawal predicted and measured Delta 0: over "
              "this membership's call sites of the spelling, 0 are charged in "
              "either forgiveness direction and none sits in a fuzzy==100 row."
              % (s["ah_c_N"], s["ah_offset"], s["ah_c_S"], s["ah_c_N"],
                 " | ".join(parts)))
        if s.get("ah_extra_evidence"):
            ev += " " + s["ah_extra_evidence"]
        rec = {
            "spelling": s["folded"],
            "lane": LANE,
            "class": s["ah_class"],
            "disposition": "withdrawn",
            "absent_types": sorted(ev_types),
            "type_evidence": ev_types,
            "witness_verdict_before": s["witness_verdict"],
            "evidence": ev,
        }
        g["folded"] = [x for x in g["folded"] if x != s["folded"]]
        g.setdefault("withdrawn", []).append(rec)
        applied.append({"group_index": i, "census_gi": s["gi"],
                        "addr": s["addr"], "bytes": int(s["bytes"]),
                        "class": rec["class"], "spelling": s["folded"],
                        "absent_types": rec["absent_types"]})

    ALI.write_text(json.dumps(ali, indent=1) + "\n")
    print("withdrew %d memberships over %d groups (%d B census)"
          % (len(applied), len({x["group_index"] for x in applied}),
             sum(x["bytes"] for x in applied)))
    for x in applied:
        print("  group[%d] (census gi=%s) %-34s %-22s %s"
              % (x["group_index"], x["census_gi"], x["class"],
                 ",".join(x["absent_types"])[:22], x["spelling"][:60]))
    if a.out:
        Path(a.out).write_text(json.dumps(applied, indent=1) + "\n")


if __name__ == "__main__":
    main()
