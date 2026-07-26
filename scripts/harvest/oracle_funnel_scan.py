#!/usr/bin/env python3
"""oracle_funnel_scan — measure the ORACLE-COVERAGE funnel honestly.

Pool definition (SCORED, not COFF-raw): objdiff's report.json lists, per unit,
every function it scores.  A target function that has no `target_symbol_map`
entry stays anonymous `fn_<VA>` and therefore cannot pair with our compiled
obj's MSVC-mangled symbol -> it scores 0 (or is positionally paired as an
anonymous funclet, which we must NOT touch).

Funnel stages
  0 raw            every `fn_<VA>` row in report.json
  1 - vendor       auto_ spans in 0x828-0x82C (XDK/Quazal, hard-skipped)
  2 - already-100  anonymous funclets already positionally paired (measured -13
                   if named)
  3 in-scope
  4 - no base obj  auto_ carve spans with no compiled TU: a map entry can never
                   pay because there is no obj emitting the name.  (splits/source
                   work -> a DIFFERENT lane)
  5 - no candidate name: the unit's compiled obj emits no unpaired non-__unwind$
                   code symbol.  Pairing is PER-UNIT, so a name emitted by any
                   other obj is unreachable.
  6 workable bound = min(in_scope_fn, unpaired_base_nonunwind) per unit

Read-only.
"""
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from size_order_automap import _ordered_funcs  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build" / "45410914"
FN_RX = re.compile(r"^fn_8[0-9A-Fa-f]{7}$")
AUTO_RX = re.compile(r"default/auto_\d+_(8[0-9A-Fa-f]{7})_text")
UNWIND_RX = re.compile(r"^__unwind\$|^\$")


def main():
    rep = json.load(open(BUILD / "report.json"))
    stage = defaultdict(int)
    rows = []

    for u in rep["units"]:
        uname = u["name"]
        meta = u.get("metadata", {})
        src = meta.get("source_path")
        m = AUTO_RX.match(uname)
        vendor = bool(m) and 0x82800000 <= int(m.group(1), 16) < 0x82D00000
        inscope = []
        for f in u.get("functions", []):
            if not FN_RX.match(f["name"]):
                continue
            stage["0_raw"] += 1
            if vendor:
                stage["1_vendor"] += 1
                continue
            if f.get("match_percent_normalized") == 100.0:
                stage["2_already100"] += 1
                continue
            inscope.append(f)
        if not inscope:
            continue
        stage["3_inscope"] += len(inscope)

        bobj = None
        if src:
            p = BUILD / "src" / (src[4:] if src.startswith("src/") else src)
            p = p.with_suffix(".obj")
            if p.exists():
                bobj = p
        if bobj is None:
            stage["4_no_base_obj"] += len(inscope)
            rows.append(dict(unit=uname, src=src, n_inscope=len(inscope),
                             n_cand=0, bound=0, reason="NO_BASE_OBJ",
                             bytes=sum(int(f["size"]) for f in inscope)))
            continue

        rel = uname[len("default/"):] if uname.startswith("default/") else uname
        tobj = BUILD / "obj" / (rel + ".obj")
        try:
            tnames = {f["name"] for f in _ordered_funcs(tobj)}
            bf = _ordered_funcs(bobj)
        except Exception:
            stage["parse_error"] += len(inscope)
            continue
        cand = [f["name"] for f in bf
                if f["name"] not in tnames and not UNWIND_RX.match(f["name"])]
        if not cand:
            stage["5_no_candidate_name"] += len(inscope)
            rows.append(dict(unit=uname, src=src, n_inscope=len(inscope),
                             n_cand=0, bound=0, reason="NO_CANDIDATE_NAME",
                             bytes=sum(int(f["size"]) for f in inscope)))
            continue
        bound = min(len(inscope), len(cand))
        stage["6_workable_bound"] += bound
        stage["6_units"] += 1
        rows.append(dict(unit=uname, src=src, n_inscope=len(inscope),
                         n_cand=len(cand), bound=bound, reason="WORKABLE",
                         bytes=sum(int(f["size"]) for f in inscope),
                         vas=[f["name"] for f in inscope],
                         sizes=[int(f["size"]) for f in inscope],
                         tier=meta.get("progress_categories", [])))

    rows.sort(key=lambda r: -r["bound"])
    outp = Path(sys.argv[1] if len(sys.argv) > 1 else "/home/free/tmp/oracle_funnel.json")
    outp.write_text(json.dumps(dict(stages=dict(stage), rows=rows), indent=1))
    for k in sorted(stage):
        print(f"{k:22s} {stage[k]}")
    print()
    for r in rows[:40]:
        if r["reason"] != "WORKABLE":
            continue
        print(f"  bound={r['bound']:4d} inscope={r['n_inscope']:4d} cand={r['n_cand']:5d} "
              f"{r['bytes']:7d}B  {r['unit']}")
    print(f"\nwrote {outp}")


if __name__ == "__main__":
    main()
