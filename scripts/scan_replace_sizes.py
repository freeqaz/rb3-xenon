#!/usr/bin/env python3
"""For every pinned unit, list ObjPtrList<T,ObjectDir>::Replace COMDAT symbols
with their exact section size (raw_size of their own COMDAT .text section),
and whether that exact mangled name is already claimed in target_symbol_map.json."""
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from extract_decomp_symbols import parse_coff, PROJECT_ROOT, SRC_OBJ_DIR

MAP_PATH = PROJECT_ROOT / "scripts" / "target_symbol_map.json"
SPLITS_PATH = PROJECT_ROOT / "config" / "45410914" / "splits.txt"

def get_pinned_units():
    pinned = set()
    for line in SPLITS_PATH.read_text().splitlines():
        line = line.rstrip()
        m = re.match(r"^([A-Za-z0-9_]+\.cpp):$", line)
        if m:
            pinned.add(m.group(1))
    return pinned

def main():
    pinned = get_pinned_units()
    target_map = json.loads(MAP_PATH.read_text())
    mapped_names = set(v for v in target_map.values() if isinstance(v, str))

    pat = re.compile(r"^\?Replace@\?\$ObjPtrList@V([A-Za-z0-9_]+)@(?:Hmx@)?@V?ObjectDir@@@@")

    for obj in sorted(SRC_OBJ_DIR.rglob("*.obj")):
        if obj.stem + ".cpp" not in pinned:
            continue
        parsed = parse_coff(obj)
        if not parsed:
            continue
        sections, symbols = parsed
        for sym in symbols:
            name = sym['name']
            if not name.startswith('?Replace@?$ObjPtrList@'):
                continue
            if sym['section_number'] <= 0 or sym['section_number'] > len(sections):
                continue
            sec = sections[sym['section_number'] - 1]
            size = sec.get('raw_size')
            mapped = name in mapped_names
            print(f"{'MAPPED  ' if mapped else 'UNMAPPED'}  size={size:<5} {obj.relative_to(PROJECT_ROOT)!s:55s} {name}")

if __name__ == '__main__':
    main()
