#!/usr/bin/env python3
"""Generate splits.txt pin fragments + map fragment from plain-UNIQUE homing hits.
Filters VAs already inside any existing splits.txt .text/.pdata range."""
import json, re
from collections import defaultdict

WT = "/home/free/tmp/wt-homing"
RESULTS = "/home/free/tmp/homing_results.json"
SPLITS = f"{WT}/config/45410914/splits.txt"

# unit key in splits.txt / objects.json per TU
UNIT = {
    'GameGem':      'system/beatmatch/GameGem.cpp',
    'GameGemList':  'system/beatmatch/GameGemList.cpp',
    'GameGemDB':    'system/beatmatch/GameGemDB.cpp',
    'BeatMatchUtl': 'system/beatmatch/BeatMatchUtl.cpp',
    'TrackType':    'system/beatmatch/TrackType.cpp',
    'Msg':          'Msg.cpp',
}

# parse existing splits .text ranges (global, to avoid double-carve)
existing = []
rng = re.compile(r'\.(?:text|pdata)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)')
for line in open(SPLITS):
    m = rng.search(line)
    if m:
        existing.append((int(m.group(1), 16), int(m.group(2), 16)))

def covered(va, size):
    for s, e in existing:
        if va < e and va+size > s:  # overlap
            return True
    return False

results = json.load(open(RESULTS))
pins = defaultdict(list)   # unit -> list of (va, size, name)
mapfrag = {}
skipped_covered = []
for tu, res in results.items():
    if isinstance(res, dict):
        continue
    for r in res:
        if r['cls'] != 'UNIQUE':
            continue
        va = int(r['va'], 16); size = r['size']
        if covered(va, size):
            skipped_covered.append((tu, r['va'], r['name']))
            continue
        pins[UNIT[tu]].append((va, size, r['name']))
        mapfrag[r['va']] = r['name']

# merge adjacent ranges per unit
pin_lines = []
pin_summary = {}
for unit in sorted(pins):
    fns = sorted(pins[unit])
    ranges = []
    for va, size, name in fns:
        end = va + size
        if ranges and va <= ranges[-1][1]:      # contiguous/adjacent
            ranges[-1] = (ranges[-1][0], max(ranges[-1][1], end))
        else:
            ranges.append((va, end))
    pin_lines.append(f"{unit}:")
    for s, e in ranges:
        pin_lines.append(f"\t.text\tstart:0x{s:08X} end:0x{e:08X}")
    pin_summary[unit] = dict(nfns=len(fns), nranges=len(ranges),
                             span=f"0x{fns[0][0]:08x}-0x{fns[-1][0]+fns[-1][1]:08x}")

open('/home/free/tmp/homing_pins.txt', 'w').write("\n".join(pin_lines) + "\n")
json.dump(mapfrag, open('/home/free/tmp/homing_map_fragment.json', 'w'), indent=1)

# objects.json entries
objentries = {UNIT[tu]: "NonMatching" for tu in ['GameGem','GameGemList','GameGemDB','BeatMatchUtl','TrackType']}
json.dump(objentries, open('/home/free/tmp/homing_objects_entries.json', 'w'), indent=1)

print("UNIQUE map entries proposed:", len(mapfrag))
print("skipped (already covered by a splits range):", len(skipped_covered))
for tu, va, nm in skipped_covered:
    print(f"   COVERED {tu} {va} {nm[:60]}")
print("\nPin summary:")
for u, s in pin_summary.items():
    print(f"  {u}: {s['nfns']} fns, {s['nranges']} ranges, span {s['span']}")
