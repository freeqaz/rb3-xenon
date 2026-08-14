#!/usr/bin/env python3
"""Quantify the compiled-but-unpinned class produced by tools/unpinned_unit_census.py.

For each unpinned unit we read the DEFINED function symbols out of its own
compiled base .obj (the obj nothing consults) and ask where, if anywhere, those
names appear on the TARGET side of report.json.

  * name present as a target row  -> the retail function IS pinned, under
    whatever unit encloses it.  If that unit's base obj defines the name, the
    row pairs; that is the benign case.
  * name absent from every target row -> our source defines a function the map
    names nowhere.  Its retail body is sitting under a FOREIGN name, i.e. it is
    mis-attributed (this is exactly TimeConversion::TickToMs).

Sizes are read as int() -- several report.json numerics are JSON STRINGS, and an
un-coerced sum silently concatenates.
"""
import json
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from unpinned_unit_census import ROOT, coff_defined_functions, load_objects  # noqa: E402
from coff_owned import analyze as coff_owned  # noqa: E402

CENSUS = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/home/free/tmp/pinsrc1_census.json")
REPORT = ROOT / "build" / "45410914" / "report.json"

PLACEHOLDER = ("fn_", "lbl_", "jumptable_", "data_", "bss_", "rdata_", "func_")


def main():
    census = json.loads(CENSUS.read_text())
    report = json.loads(REPORT.read_text())
    objects = load_objects()

    # global target-row index: name -> list of (unit, size, mpn)
    rows = defaultdict(list)
    unit_of_row = {}
    for u in report["units"]:
        for f in u.get("functions", []):
            nm = f["name"]
            rows[nm].append((u["name"], int(f.get("size", 0)),
                             float(f.get("match_percent_normalized", 0.0))))
    # which base obj each objdiff unit consults
    objdiff = json.loads((ROOT / "objdiff.json").read_text())
    base_of_unit = {x["name"]: x.get("base_path") for x in objdiff["units"]}
    base_syms_cache = {}

    def base_syms(unit):
        bp = base_of_unit.get(unit)
        if not bp:
            return set()
        if bp not in base_syms_cache:
            base_syms_cache[bp] = coff_defined_functions(ROOT / bp)
        return base_syms_cache[bp]

    out = []
    for pk, info in sorted(census["unpinned"].items()):
        objp = ROOT / "build" / "45410914" / "src" / (pk.rsplit(".", 1)[0] + ".obj")
        syms, _shared = coff_owned(objp)
        syms = {s for s in syms if not s.startswith(PLACEHOLDER)}
        # only function-ish symbols: mangled C++ or plain C names present in rows
        present, absent = [], []
        for s in sorted(syms):
            if s in rows:
                present.append((s, rows[s]))
            else:
                absent.append(s)
        pres_bytes = sum(sz for _s, lst in present for (_u, sz, _m) in lst)
        # of the present ones, how many actually pair (enclosing base defines it)
        pairing = sum(1 for _s, lst in present for (un, _sz, _m) in lst if _s in base_syms(un))
        out.append({
            "unit": pk, "cls": info["cls"], "includers": info["includers"],
            "obj_exists": objp.exists(), "obj_size": objp.stat().st_size if objp.exists() else 0,
            "n_defined": len(syms), "n_present": len(present), "n_absent": len(absent),
            "present_bytes": pres_bytes, "n_pairing": pairing,
            "present": [(s, lst) for s, lst in present],
            "absent": absent,
        })

    print(json.dumps(out, indent=1))

    a = [r for r in out if r["cls"].startswith("A_")]
    b = [r for r in out if r["cls"] == "B_ORPHAN"]
    for lbl, grp in (("A_SCATTER", a), ("B_ORPHAN", b)):
        print(f"\n== {lbl}: {len(grp)} units", file=sys.stderr)
        print(f"   defined syms {sum(r['n_defined'] for r in grp)}"
              f"  present-as-target-row {sum(r['n_present'] for r in grp)}"
              f"  absent {sum(r['n_absent'] for r in grp)}", file=sys.stderr)
        print(f"   present bytes {sum(r['present_bytes'] for r in grp):,}"
              f"  of which pairing {sum(r['n_pairing'] for r in grp)}", file=sys.stderr)
        print(f"   units with ZERO defined syms: "
              f"{sum(1 for r in grp if r['n_defined'] == 0)}", file=sys.stderr)


if __name__ == "__main__":
    main()
