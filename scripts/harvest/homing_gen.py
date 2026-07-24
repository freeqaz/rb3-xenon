#!/usr/bin/env python3
"""Round-2 dedup generator: splits pins + map fragment from plain-UNIQUE hits.

Differs from homing_gen.py: dedups a VA to exactly ONE owning unit (ICF-folded
STL helpers surface as UNIQUE in several TUs at the same VA; pinning one VA in
two units would double-carve and double-count). Also dedups by name and filters
VAs already inside an existing splits .text/.pdata range.
"""
import json, re
from collections import defaultdict, OrderedDict

WT = "/home/free/tmp/wt-homing2"
RESULTS = "/home/free/tmp/homing2_results.json"
SPLITS = f"{WT}/config/45410914/splits.txt"

# splits.txt unit header per TU. New TUs use the prefixed form (round-1 style);
# VocalNoteList/BeatMap already have basename headers -> append there.
UNIT = OrderedDict([
    ('DrumMap',               'system/beatmatch/DrumMap.cpp'),
    ('DrumMixDB',             'system/beatmatch/DrumMixDB.cpp'),
    ('PhraseDB',              'system/beatmatch/PhraseDB.cpp'),
    ('PhraseAnalyzer',        'system/beatmatch/PhraseAnalyzer.cpp'),
    ('PlayerTrackConfigList', 'system/beatmatch/PlayerTrackConfigList.cpp'),
    ('FillInfo',              'system/beatmatch/FillInfo.cpp'),
    ('TimeSpanVector',        'system/beatmatch/TimeSpanVector.cpp'),
    ('UserGuid',              'system/utl/UserGuid.cpp'),
    ('VocalNoteList',         'VocalNoteList.cpp'),   # existing basename header
    ('BeatMap',               'BeatMap.cpp'),         # existing basename header
])
# units that already have a header in splits.txt (append, don't create)
EXISTING = {'VocalNoteList.cpp', 'BeatMap.cpp'}

existing = []
rng = re.compile(r'\.(?:text|pdata)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)')
for line in open(SPLITS):
    m = rng.search(line)
    if m:
        existing.append((int(m.group(1), 16), int(m.group(2), 16)))

def covered(va, size):
    for s, e in existing:
        if va < e and va+size > s:
            return True
    return False

results = json.load(open(RESULTS))
claimed_va = {}      # va -> owning tu
claimed_name = {}    # name -> va
pins = defaultdict(list)   # unit -> [(va, size, name)]
mapfrag = {}
skipped_covered = []
dup_va = []
dup_name = []

for tu in UNIT:   # deterministic owner order
    res = results.get(tu)
    if not isinstance(res, list):
        continue
    for r in res:
        if r['cls'] != 'UNIQUE':
            continue
        va = int(r['va'], 16); size = r['size']; name = r['name']
        if covered(va, size):
            skipped_covered.append((tu, r['va'], name))
            continue
        if va in claimed_va:
            dup_va.append((tu, r['va'], name, claimed_va[va]))
            continue
        if name in claimed_name:
            dup_name.append((tu, r['va'], name))
            continue
        claimed_va[va] = tu
        claimed_name[name] = va
        pins[UNIT[tu]].append((va, size, name))
        mapfrag[r['va']] = name

# per-unit contiguous merge
new_blocks = []       # blocks for brand-new headers
append_ranges = {}    # existing unit -> [(s,e)]
pin_summary = {}
for unit in UNIT.values():
    if unit not in pins:
        continue
    fns = sorted(pins[unit])
    ranges = []
    for va, size, name in fns:
        end = va + size
        if ranges and va <= ranges[-1][1]:
            ranges[-1] = (ranges[-1][0], max(ranges[-1][1], end))
        else:
            ranges.append((va, end))
    pin_summary[unit] = dict(nfns=len(fns), nranges=len(ranges),
                             span=f"0x{fns[0][0]:08x}-0x{fns[-1][0]+fns[-1][1]:08x}")
    if unit in EXISTING:
        append_ranges[unit] = ranges
    else:
        block = [f"{unit}:"]
        for s, e in ranges:
            block.append(f"\t.text\tstart:0x{s:08X} end:0x{e:08X}")
        new_blocks.append("\n".join(block))

open('/home/free/tmp/homing2_pins.txt', 'w').write("\n".join(new_blocks) + "\n")
json.dump(mapfrag, open('/home/free/tmp/homing2_map_fragment.json', 'w'), indent=1)
json.dump(append_ranges, open('/home/free/tmp/homing2_append_ranges.json', 'w'), indent=1)

objentries = {UNIT[tu]: "NonMatching" for tu in
              ['DrumMap','DrumMixDB','PhraseDB','PhraseAnalyzer',
               'PlayerTrackConfigList','FillInfo','TimeSpanVector','UserGuid']}
json.dump(objentries, open('/home/free/tmp/homing2_objects_entries.json', 'w'), indent=1)

print("UNIQUE map entries proposed:", len(mapfrag))
print("skipped (covered by existing splits range):", len(skipped_covered))
for tu, va, nm in skipped_covered:
    print(f"   COVERED {tu} {va} {nm[:64]}")
print("cross-unit VA dups (dropped, kept first owner):", len(dup_va))
for tu, va, nm, owner in dup_va:
    print(f"   DUPVA  {va} {tu} -> owned by {owner}  {nm[:56]}")
print("name dups (dropped):", len(dup_name))
for tu, va, nm in dup_name:
    print(f"   DUPNAME {va} {tu} {nm[:56]}")
print("\nappend-to-existing:", {k: len(v) for k, v in append_ranges.items()})
print("Pin summary:")
for u, s in pin_summary.items():
    print(f"  {u}: {s['nfns']} fns, {s['nranges']} ranges, span {s['span']}")
