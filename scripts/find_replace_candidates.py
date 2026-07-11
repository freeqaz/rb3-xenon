#!/usr/bin/env python3
"""List candidate unmatched functions (size in {192,236,260}, no/low match)
across the set of pinned units that instantiate an unmapped
ObjPtrList<T,ObjectDir>::Replace, for RTTI probing."""
import json
import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
REPORT = PROJECT_ROOT / "build" / "45410914" / "report.json"

SIZES = {192, 236, 260}

def main():
    only_units = None
    if len(sys.argv) > 1:
        only_units = set(sys.argv[1:])
    r = json.loads(REPORT.read_text())
    for u in r['units']:
        if only_units is not None and u['name'] not in only_units:
            continue
        for f in u['functions']:
            try:
                size = int(f.get('size'))
            except (TypeError, ValueError):
                continue
            mp = f.get('fuzzy_match_percent', f.get('match_percent_normalized'))
            if size in SIZES and (mp is None or mp < 100.0) and f['name'].startswith('fn_'):
                print(f"{u['name']:35s} {f['name']} size={size} addr_off={f.get('address')} match={mp}")

if __name__ == '__main__':
    main()
