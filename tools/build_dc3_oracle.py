#!/usr/bin/env python3
"""build_dc3_oracle.py — DC3-VA <-> RB3Xenon-VA engine oracle.

Joins a BinDiff DC3-vs-RB3Xenon result with DC3's leaked ham_xbox_r.map to emit
a name+VA oracle for the SHARED Milo engine. DC3 and RB3-360 are the same
Xbox-360 / MSVC-X360 toolchain at the same preferred load address (0x82000000),
so a high BinDiff similarity on a shared engine function implies the same VA
(dc3_va == rb3_va). See docs/decomp/research/2026-06-21-dc3-engine-oracle-feasibility.md §3.

Output rows (sorted by similarity desc), modeled on unified_id_rb3wii.json:
  {dc3_va, rb3_va, dc3_name, dc3_tu, similarity, confidence}

Reads the BinDiff SQLite 'function' table: address1 = DC3 (primary) VA,
address2 = RB3Xenon (secondary) VA, similarity, confidence.
Parses ham_xbox_r.map "Publics by Value" section 0005 (.text/CODE) lines:
  ' 0005:OOOOOOOO   <mangled_name>   <VA8hex> [flags] <Lib:Object>'
into {dc3_va: (mangled_name, Lib:Object)}. ICF-folded duplicates (many names at
one VA) keep the first map entry per VA.
"""
import argparse
import json
import re
import sqlite3
import sys
# --- dead-index guard (lane BX-4) -------------------------------------------
# This tool PRODUCES an address index. It validates its OWN OUTPUT so a fresh
# index that is already dead is caught at birth rather than months later by a
# consumer. Audit any index: python3 tools/dead_index_guard.py --audit
import os as _dig_os, sys as _dig_sys
_dig_d = _dig_os.path.dirname(_dig_os.path.abspath(__file__))
while _dig_d != "/" and not _dig_os.path.exists(
        _dig_os.path.join(_dig_d, "tools", "dead_index_guard.py")):
    _dig_d = _dig_os.path.dirname(_dig_d)
_dig_sys.path.insert(0, _dig_os.path.join(_dig_d, "tools"))
from dead_index_guard import measure as _dig_measure, LIVE_THRESHOLD_PCT as _DIG_MIN  # noqa: E402


def _dig_report_output(path):
    """Measure a freshly written index and say plainly whether it is usable."""
    try:
        n, pct = _dig_measure(str(path))
    except Exception as e:                                    # noqa: BLE001
        _dig_sys.stderr.write(f"[dead_index_guard] could not verify {path}: {e}\n")
        return
    if not n:
        _dig_sys.stderr.write(f"[dead_index_guard] {path}: no addresses found to verify.\n")
    elif pct < _DIG_MIN:
        _dig_sys.stderr.write(
            "\n" + "!" * 74 +
            f"\n!! WROTE A DEAD INDEX: {path}\n"
            f"!! only {pct:.2f}% of its {n:,} addresses are real .text function starts\n"
            f"!! in config/45410914/symbols.txt (chance is ~2-3%; need >= {_DIG_MIN:.0f}%).\n"
            "!! Its inputs are almost certainly stale w.r.t. the current binary.\n"
            "!! DO NOT consume this file -- every tool that reads it will refuse.\n" +
            "!" * 74 + "\n\n")
    else:
        _dig_sys.stderr.write(
            f"[dead_index_guard] {path}: OK -- {pct:.2f}% of {n:,} addresses are live.\n")
# ----------------------------------------------------------------------------

# ' 0005:00000c10   ??$Find@...@Z 82330c10 f i App.obj'
# section : offset           name                       va8       flags...   lib:object
_MAP_LINE = re.compile(
    r'^\s+([0-9a-fA-F]{4}):[0-9a-fA-F]{8}\s+'   # 1: section
    r'(\S+)\s+'                                  # 2: mangled name
    r'([0-9a-fA-F]{8})'                          # 3: Rva+Base (absolute VA)
    r'((?:\s+[a-z]){0,3})\s+'                    # 4: 0-3 single-char flags (f/i/...)
    r'(\S.*\S|\S)\s*$'                           # 5: Lib:Object
)


def parse_dc3_map(path):
    """Return {dc3_va_int: (mangled_name, lib_object_tu)} for .text (section 0005)."""
    out = {}
    with open(path, errors='replace') as fh:
        for line in fh:
            m = _MAP_LINE.match(line)
            if not m:
                continue
            section, name, va_hex, _flags, tu = m.groups()
            if section != '0005':
                continue
            va = int(va_hex, 16)
            if va not in out:           # keep first (ICF folds many names per VA)
                out[va] = (name, tu)
    return out


def read_bindiff_functions(path):
    """Yield (dc3_va, rb3_va, similarity, confidence) from the BinDiff 'function' table."""
    con = sqlite3.connect(path)
    try:
        cur = con.execute(
            "SELECT address1, address2, similarity, confidence FROM function"
        )
        for a1, a2, sim, conf in cur:
            yield int(a1), int(a2), float(sim), float(conf)
    finally:
        con.close()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--bindiff', required=True, help='dc3_vs_rb3xenon.BinDiff (SQLite)')
    ap.add_argument('--dc3-map', required=True, help='ham_xbox_r.map')
    ap.add_argument('--out', required=True, help='output dc3_oracle.json')
    ap.add_argument('--min-sim', type=float, default=0.0,
                    help='drop pairs below this similarity (default 0 = keep all)')
    args = ap.parse_args(argv)

    dc3_names = parse_dc3_map(args.dc3_map)
    sys.stderr.write(f"parsed {len(dc3_names)} DC3 .text symbols from map\n")

    rows = []
    paired = 0
    named = 0
    for dc3_va, rb3_va, sim, conf in read_bindiff_functions(args.bindiff):
        paired += 1
        if sim < args.min_sim:
            continue
        name, tu = dc3_names.get(dc3_va, (None, None))
        if name is not None:
            named += 1
        rows.append({
            'dc3_va': f'0x{dc3_va:08x}',
            'rb3_va': f'0x{rb3_va:08x}',
            'dc3_name': name,
            'dc3_tu': tu,
            'similarity': round(sim, 4),
            'confidence': round(conf, 4),
        })
    sys.stderr.write(f"bindiff pairs={paired} kept(>= {args.min_sim})={len(rows)} "
                     f"with-dc3-name={named}\n")

    rows.sort(key=lambda r: r['similarity'], reverse=True)
    with open(args.out, 'w') as fh:
        json.dump(rows, fh, indent=1)
    sys.stderr.write(f"wrote {len(rows)} rows -> {args.out}\n")
    # NB: only the rb3_va side is checked -- dc3_va indexes the DC3
    # binary and is unaffected by RB3 .text moves.
    _dig_report_output(args.out)


if __name__ == '__main__':
    main()
