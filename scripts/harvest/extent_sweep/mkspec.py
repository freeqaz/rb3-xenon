#!/usr/bin/env python3
"""lane EA-1: emit the carve spec for the EXACT-verdict rows, generated (never
transcribed) from the two witnesses."""
import json, re, os, sys
import adj, basesize

WT = adj.WT
idx = basesize.build_index(os.path.join(WT, 'build/45410914/src'))
MAP = json.load(open(os.path.join(WT, 'scripts/target_symbol_map.json')))

CANDS = [
    (0x822DF9A0, 'Mesh.cpp'), (0x82356300, 'GemTrackResourceManager.cpp'),
    (0x82273E28, 'PatchDir.cpp'), (0x824D86C0, 'Spotlight.cpp'),
    (0x82518FE8, 'FileCache.cpp'), (0x824F4D68, 'SHA1.cpp'),
    (0x827DC090, 'HttpGet.cpp'), (0x82433B38, 'Cam.cpp'),
    (0x8242F7E8, 'PostProc.cpp'), (0x826EFC38, 'band3/game/TrainerGemTab.cpp'),
    (0x8270F4F8, 'OutfitConfig.cpp'), (0x82AE5FA8, 'StringConversion.cpp'),
    (0x824BD980, 'CameraShot.cpp'), (0x824F5480, 'Color.cpp'),
    (0x82346360, 'FileCache.cpp'), (0x822907F0, 'PropKeys.cpp'),
    (0x822CFD10, 'MoveMgr.cpp'), (0x82498D80, 'Lit.cpp'),
    (0x8249B200, 'EventTrigger.cpp'), (0x82524900, 'OnlineID.cpp'),
    (0x82524A08, 'Joypad.cpp'), (0x82739948, 'Rnd_Xbox.cpp'),
    (0x8280DE80, 'UIListState.cpp'),
]

EXCLUDE = set(int(x, 16) for x in sys.argv[2:]) if len(sys.argv) > 2 else set()

spec = []
for a, unit in CANDS:
    if a in EXCLUDE:
        continue
    r = adj.adjudicate(a)
    mn = MAP.get('0x%08x' % a)
    te = r['true_end_guess']
    if not te:
        continue
    ts = te - a
    bs = (idx.get(mn) or (None,))[0]
    if bs != ts:
        continue                     # EXACT only
    # re-assert both witnesses at spec time, so a spec can never encode a row
    # that stopped qualifying
    assert r['n_exits'] == 0, f"{mn}: claimed extent has exits, not truncated"
    assert ts > r['size'], f"{mn}: true extent not longer than claim"
    spec.append({'addr': '0x%08X' % a, 'size': '0x%X' % ts,
                 'label': f'{unit}::{mn}', 'unit': unit, 'name': mn,
                 'old_size': '0x%X' % r['size'], 'base_size': '0x%X' % bs})

json.dump(spec, open(sys.argv[1], 'w'), indent=1)
print(f"wrote {len(spec)} EXACT carves to {sys.argv[1]}")
for c in spec:
    print(f"  {c['addr']} {c['old_size']:>6} -> {c['size']:>6}  (base {c['base_size']:>6})  {c['label'][:66]}")
