#!/usr/bin/env python3
"""Scan all compiled .obj files for ObjPtrList-family Replace/dtor/ctor/Unlink
COMDAT symbols, and cross-reference against target_symbol_map.json to find
which instantiations already have a target mapping vs which don't."""
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from extract_decomp_symbols import parse_coff, PROJECT_ROOT, SRC_OBJ_DIR

MAP_PATH = PROJECT_ROOT / "scripts" / "target_symbol_map.json"

PATTERNS = [
    re.compile(r"\?Replace@\?\$ObjPtrList@"),
    re.compile(r"\?ReplaceNode@\?\$ObjPtrList@"),
    re.compile(r"\?Unlink@\?\$ObjPtrList@"),
    re.compile(r"\?\?0\?\$ObjPtrList@"),
    re.compile(r"\?\?1\?\$ObjPtrList@"),
    re.compile(r"\?\?_D\?\$ObjPtrList@"),
]

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
    import sys as _sys
    only_replace = '--replace-only' in _sys.argv
    pinned_only = '--pinned-only' in _sys.argv
    pinned = get_pinned_units() if pinned_only else None

    target_map = json.loads(MAP_PATH.read_text())
    mapped_names = set(v for v in target_map.values() if isinstance(v, str)) if isinstance(target_map, dict) else set()

    results = []
    for obj in sorted(SRC_OBJ_DIR.rglob("*.obj")):
        if pinned is not None and obj.stem + ".cpp" not in pinned:
            continue
        parsed = parse_coff(obj)
        if not parsed:
            continue
        sections, symbols = parsed
        for sym in symbols:
            name = sym['name']
            if sym['section_number'] <= 0:
                continue
            if only_replace and not name.startswith('?Replace@?$ObjPtrList@'):
                continue
            for pat in PATTERNS:
                if pat.search(name):
                    sec = sections[sym['section_number'] - 1] if 0 < sym['section_number'] <= len(sections) else None
                    is_mapped = name in mapped_names
                    results.append({
                        'obj': str(obj.relative_to(PROJECT_ROOT)),
                        'name': name,
                        'section': sec['name'] if sec else None,
                        'mapped': is_mapped,
                    })
                    break

    for r in results:
        print(f"{'MAPPED  ' if r['mapped'] else 'UNMAPPED'}  {r['obj']:60s} {r['name']}")
    print(f"\nTotal: {len(results)}  mapped={sum(1 for r in results if r['mapped'])}  unmapped={sum(1 for r in results if not r['mapped'])}")

if __name__ == '__main__':
    main()
