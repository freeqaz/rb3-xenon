#!/usr/bin/env python3
"""W16-AH: turn the type-existence verdicts into an APPLY selection, in classes.

Reads the selector's two outputs (`w16ah_type_existence.py sel.json ev.json`)
and emits the rows to withdraw, each carrying the evidence its `withdrawn`
record will quote, plus a `ah_class`:

  TYPE_ABSENT_FROM_RETAIL_ELEMENT    -- c_N instantiates a DC3-only polymorphic
                                        ELEMENT type (Flow, Ham*, ...).
  TYPE_ABSENT_FROM_RETAIL_CONTAINER  -- element type is retail-PRESENT; only the
                                        ObjPtrVec CONTAINER is absent.

and HOLDS BACK the container-class rows whose group survivor is itself an
ObjPtrVec spelling (the argument would impeach the survivor/map row as well).

Printing the three populations separately is the point: they rest on different
evidence, and a later lane must not have to re-derive which is which.
"""
import collections
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

ELEMENT_TYPES = {"Flow", "FlowLabel", "FlowNode", "FlowOutPort", "HamCharacter",
                 "HamMove", "RhythmDetector", "DepthBuffer3D",
                 "HamSupereasyData"}
CONTAINER_TYPES = {"ObjPtrVec"}


def main():
    sel = json.loads(Path(sys.argv[1]).read_text())
    out_apply = Path(sys.argv[2])
    out_hold = Path(sys.argv[3])

    apply_rows, hold_rows = [], []
    for r in sel:
        q = set(r["qualifying_types"])
        elem = sorted(q & ELEMENT_TYPES)
        cont = sorted(q & CONTAINER_TYPES)
        if elem:
            cls, types = "TYPE_ABSENT_FROM_RETAIL_ELEMENT", elem
        elif cont:
            cls, types = "TYPE_ABSENT_FROM_RETAIL_CONTAINER", cont
        else:
            sys.exit("REFUSING: row qualifies on an unclassified type: %s" % q)
        p = r["pairs"][0]
        e = dict(r)
        e["ah_class"] = cls
        e["ah_evidence_types"] = {t: r["type_evidence"][t] for t in types}
        e["ah_c_N"] = p["c_N"]
        e["ah_c_S"] = p["c_S"]
        e["ah_offset"] = p.get("offset")
        if (cls == "TYPE_ABSENT_FROM_RETAIL_CONTAINER"
                and "?$ObjPtrVec@" in r["survivor"]):
            e["ah_hold_reason"] = (
                "group survivor is itself an ObjPtrVec spelling: the container "
                "argument impeaches the map row naming this address, so the row "
                "is a map identification question, not an alias withdrawal")
            hold_rows.append(e)
        else:
            apply_rows.append(e)

    out_apply.write_text(json.dumps(apply_rows, indent=1) + "\n")
    out_hold.write_text(json.dumps(hold_rows, indent=1) + "\n")

    def show(tag, rows):
        c = collections.Counter(x["ah_class"] for x in rows)
        print("%-8s %4d rows %6d B  %s"
              % (tag, len(rows), sum(int(x["bytes"]) for x in rows), dict(c)))
    show("APPLY", apply_rows)
    show("HOLD", hold_rows)
    for cls in sorted({x["ah_class"] for x in apply_rows}):
        rs = [x for x in apply_rows if x["ah_class"] == cls]
        t = collections.Counter(t for x in rs for t in x["ah_evidence_types"])
        print("   %-34s %4d rows %6d B  types=%s"
              % (cls, len(rs), sum(int(x["bytes"]) for x in rs), dict(t)))
    print("   batches of 20: %d" % ((len(apply_rows) + 19) // 20))


if __name__ == "__main__":
    main()
