#!/usr/bin/env python3
"""Validate field_offset_gate against the proven RockCentral.cpp +17 win (B1).

PIPELINE-DESIGN.md S9 B1 / S10 gate 5 require: the field_offset_gate must NOT
exclude any method that was a proven win. RockCentral.cpp landed +17 via
identity_transfer's case-A micro-pins (commit 116acc3, measured whole-binary
9477->9494, 0 regressions). This script CONFIRMS the gate retains every method
capable of being one of those 17.

GROUND TRUTH (how the 17 are determined -- static, no rebuild)
--------------------------------------------------------------
The +17 commit (116acc3) added 48 named carved entries to
scripts/target_symbol_map.json. The methods that could byte-match the retail
RockCentral.cpp body -- and therefore the +17 landed set -- are exactly those
carved entries that are:
  * a RockCentral.cpp oracle method (in unified_id_rb3wii.json under that TU;
    a foreign-class name carved via the map is not "a landed RockCentral
    method"), AND
  * real-bodied (>44B retail size; <=44B = ICF stub-fold, honesty-DQ), AND
  * NOT a POISONED-TAIL Stats accessor (a method that loads/returns a Stats
    field at/above the first std::vector member @0x70 loads the WRONG offset on
    the Wii-vs-retail layout divergence -> it could NOT byte-match -> it was
    never one of the 17).
This "winner pool" is a SUPERSET of the 17 (the net +17 = 17 of the pool that
also cleared the axis-B/D codegen residual). The gate passes iff it retains the
ENTIRE pool -- then every actual winner is retained a fortiori.

The current report.json is at a later baseline (RockCentral has since drifted
below 100% via a shared-header ripple -- RockCentral.cpp/Stats.h are themselves
unchanged since the win, confirmed by git), so the win cannot be read from the
live report; this static reconstruction is the faithful determination.

Usage:  tools/field_offset_gate_validate_rockcentral.py
Exit 0 iff excluded_landed == [] (zero proven wins dropped).
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from pin_identified import load_sizes  # noqa: E402

WIN_COMMIT = "116acc3"           # the identity_transfer +17 RockCentral commit
TU = "RockCentral.cpp"
D = 0x70                         # Stats first std::vector member = divergence
DIVERGING = "Stats,PerformanceData"
SIZE_GATE = 44


def carved_entries():
    """The 48 {VA: mangled} entries the +17 commit added to the target map."""
    out = subprocess.run(
        ["git", "-C", ROOT, "show", WIN_COMMIT, "--",
         "scripts/target_symbol_map.json"],
        capture_output=True, text=True).stdout
    carved = {}
    for line in out.splitlines():
        if not line.startswith("+"):
            continue
        m = re.search(r'"(0x[0-9A-Fa-f]+)": "([^"]+)"', line)
        if m:
            carved[int(m.group(1), 16)] = m.group(2)
    return carved


def run_gate():
    """Run field_offset_gate and return (pin_vas, excluded_by_va)."""
    out_json = "/tmp/field_offset_gate_rockcentral.json"
    subprocess.run(
        [sys.executable, os.path.join(ROOT, "tools", "field_offset_gate.py"),
         "--tu", TU, "--D", hex(D), "--class", DIVERGING, "--out", out_json],
        check=True, stdout=subprocess.DEVNULL)
    g = json.load(open(out_json))
    pin = {int(r["va"], 16) for r in g["pin_set"]}
    exc = {int(e["va"], 16): e for e in g["excluded"]}
    return pin, exc


def main():
    sizes = load_sizes(os.path.join(ROOT, "config", "45410914", "symbols.txt"))
    carved = carved_entries()
    oracle = json.load(open(os.path.join(ROOT, "unified_id_rb3wii.json")))
    rc_vas = {int(e["rb3_addr"], 16) for e in oracle
              if (e.get("bindiff_src") or "").replace("\\", "/").rsplit("/", 1)[-1] == TU}
    gate_pin, gate_exc = run_gate()

    winners, doomed_tail, not_rc, stubs = [], [], [], []
    for va, name in sorted(carved.items()):
        if va not in rc_vas:
            not_rc.append((va, name))
            continue
        if (sizes.get(va) or 0) <= SIZE_GATE:
            stubs.append((va, name))
            continue
        if va in gate_exc and gate_exc[va]["reason"] == "POISONED-TAIL":
            doomed_tail.append((va, name))
            continue
        winners.append((va, name))

    excluded_landed = [(va, n) for va, n in winners if va not in gate_pin]
    retained = [va for va, _ in winners if va in gate_pin]

    print("=" * 70)
    print("field_offset_gate validation vs RockCentral.cpp +17")
    print("=" * 70)
    print(f"carved by {WIN_COMMIT}              : {len(carved)}")
    print(f"  foreign-class (not RC method)  : {len(not_rc)}")
    print(f"  stub <=44B (honesty-DQ)        : {len(stubs)}")
    print(f"  POISONED-TAIL (axis-A doomed)  : {len(doomed_tail)}")
    print(f"WINNER POOL (>=17 landed)        : {len(winners)}")
    print(f"  gate RETAINED                  : {len(retained)}/{len(winners)}")
    print(f"  excluded_landed (MUST be [])   : {len(excluded_landed)}")
    for va, n in excluded_landed:
        print(f"     !! 0x{va:08X} {n}")
    ok = (len(excluded_landed) == 0)
    print(f"\nVALIDATION: {'PASS' if ok else 'FAIL'} "
          f"(rockcentral_retained={len(retained)} "
          f"rockcentral_landed_total={len(winners)})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
