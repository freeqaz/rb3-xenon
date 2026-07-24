#!/usr/bin/env python3
"""Round-3 dedup generator: splits pins + map fragment from plain-UNIQUE hits.

Same dedup discipline as homing_gen (round 2):
 * plain-UNIQUE only (drop UNIQUE-ICF / MULTI / ALL-MAPPED / NOMATCH),
 * cross-unit VA dedup (one owner per VA, deterministic order),
 * name dedup,
 * drop VAs already inside an existing splits .text/.pdata range,
 * drop names/VAs already present in target_symbol_map.

All five round-3 TUs are brand-new headers (none pre-exist in splits.txt).
"""
import json, re
from collections import defaultdict, OrderedDict

WT = "/home/free/tmp/wt-homing3"
RESULTS = "/home/free/tmp/homing_results.json"
SPLITS = f"{WT}/config/45410914/splits.txt"
TMAP = f"{WT}/scripts/target_symbol_map.json"

# scan-key -> splits.txt unit header (full prefixed path, matching existing convention)
UNIT = OrderedDict([
    ('VoiceBeat',             'system/synth/VoiceBeat.cpp'),
    ('VibratoDetector',       'system/dsp/VibratoDetector.cpp'),
    ('CommonPhraseCapturer',  'band3/game/CommonPhraseCapturer.cpp'),
    ('TambourineDetector',    'band3/game/TambourineDetector.cpp'),
    ('VocalScoreHistory',     'band3/game/VocalScoreHistory.cpp'),
])

# existing splits ranges
existing = []
rng = re.compile(r'\.(?:text|pdata)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)')
for line in open(SPLITS):
    m = rng.search(line)
    if m:
        existing.append((int(m.group(1), 16), int(m.group(2), 16)))

def covered(va, size):
    for s, e in existing:
        if va < e and va + size > s:
            return True
    return False

# existing target_symbol_map keys (VAs) + values (names)
tmap = json.load(open(TMAP))
map_vas = set()
map_names = set()
for k, v in tmap.items():
    if k.startswith('_'):
        continue
    try:
        map_vas.add(int(k, 16))
    except ValueError:
        pass
    if isinstance(v, str):
        map_names.add(v)

results = json.load(open(RESULTS))
claimed_va = {}
claimed_name = {}
pins = defaultdict(list)
mapfrag = {}
skipped_covered = []
skipped_mapped = []
dup_va = []
dup_name = []

for tu in UNIT:
    res = results.get(tu)
    if not isinstance(res, list):
        continue
    for r in res:
        if r['cls'] != 'UNIQUE':
            continue
        va = int(r['va'], 16); size = r['size']; name = r['name']
        if covered(va, size):
            skipped_covered.append((tu, r['va'], name)); continue
        if va in map_vas or name in map_names:
            skipped_mapped.append((tu, r['va'], name)); continue
        if va in claimed_va:
            dup_va.append((tu, r['va'], name, claimed_va[va])); continue
        if name in claimed_name:
            dup_name.append((tu, r['va'], name)); continue
        claimed_va[va] = tu
        claimed_name[name] = va
        pins[UNIT[tu]].append((va, size, name))
        mapfrag[r['va']] = name

new_blocks = []
pin_summary = {}
for tu, unit in UNIT.items():
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
    block = [f"{unit}:"]
    for s, e in ranges:
        block.append(f"\t.text\tstart:0x{s:08X} end:0x{e:08X}")
    new_blocks.append("\n".join(block))

open('/home/free/tmp/homing3_pins.txt', 'w').write("\n".join(new_blocks) + "\n")
json.dump(mapfrag, open('/home/free/tmp/homing3_map_fragment.json', 'w'), indent=1)

print("UNIQUE map entries proposed:", len(mapfrag))
print("skipped (covered by existing splits range):", len(skipped_covered))
for tu, va, nm in skipped_covered:
    print(f"   COVERED {tu} {va} {nm[:64]}")
print("skipped (already in target_symbol_map):", len(skipped_mapped))
for tu, va, nm in skipped_mapped:
    print(f"   INMAP   {tu} {va} {nm[:64]}")
print("cross-unit VA dups (dropped):", len(dup_va))
for tu, va, nm, owner in dup_va:
    print(f"   DUPVA  {va} {tu} -> owned by {owner}  {nm[:56]}")
print("name dups (dropped):", len(dup_name))
for tu, va, nm in dup_name:
    print(f"   DUPNAME {va} {tu} {nm[:56]}")
print("\nPin summary:")
for u, s in pin_summary.items():
    print(f"  {u}: {s['nfns']} fns, {s['nranges']} ranges, span {s['span']}")
