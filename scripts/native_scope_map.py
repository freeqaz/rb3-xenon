#!/usr/bin/env python3
"""native_scope_map.py — authoritative NATIVE-SCOPE decomp map for rb3-xenon.

Decomp scope is defined by the NATIVE PORT (native/ x86_64 engine; see
docs/plans/engine-reuse-and-asset-rendering.md and memory project_native_port).
This tool classifies every wired TU in build/45410914/report.json into:

  NATIVE-CORE     linked into native/ today (obj/utl/os/math + rndobj/Anim.cpp),
                  i.e. the DTA/song-parse runtime path that already boots.
  NATIVE-SOON     RB3 game logic for the next runtime milestones (song load,
                  metadata, scoring, practice): band3/, meta_ham/, beatmatch/,
                  midi/, track/, hamobj/.
  NATIVE-VIA-DC3  engine rendering/asset stack DC3 supplies wholesale
                  (rndobj/char/world/ui/bandobj/gesture/movie/meta/flow/synth) —
                  decomp here is REDUNDANT for the port.
  360-ONLY        out of scope: rnddx9/Dx*/xdk/Quazal(network,net,net_ham)/
                  synth_xbox(XMA)/moviebink/vendor codecs/*_Xbox.
  UNKNOWN-anon    dtk auto-split blobs with no source_path (unidentified target
                  code — the frontier that may still hide native-scope logic).

Emits the per-class table (fns/bytes/strict%), the native-scope coverage
headline vs whole-binary, and the ranked in-scope backlog. Re-run after each
wave:  python3 scripts/native_scope_map.py [report.json]

Read-only. Prints markdown to stdout.
"""
import json
import sys
from collections import defaultdict

# ---- classification rules (first match wins) ----------------------------------
# 360-ONLY / vendor substrings
OUT = [
    'src/xdk/', 'src/system/stlport/', 'src/system/msvc-src/',
    'src/network/', 'src/net_ham/', 'src/system/net/', 'src/system/rnddx9/',
    'src/system/synth_xbox/', 'src/system/moviebink/', 'src/system/jpeg/',
    'src/system/oggvorbis/', 'src/system/speex/', 'src/system/zlib/', 'src/curl/',
]
CORE = ['src/system/obj/', 'src/system/utl/', 'src/system/os/', 'src/system/math/']
SOON = ['src/band3/', 'src/meta_ham/', 'src/system/beatmatch/',
        'src/system/midi/', 'src/system/track/', 'src/system/hamobj/']
VIA_DC3 = ['src/system/rndobj/', 'src/system/char/', 'src/system/world/',
           'src/system/ui/', 'src/system/bandobj/', 'src/system/gesture/',
           'src/system/movie/', 'src/system/meta/', 'src/system/flow/',
           'src/system/synth/', 'src/system/dsp/']


def classify(sp):
    if not sp:
        return 'UNKNOWN-anon'
    s = sp
    if any(x in s for x in OUT):
        return '360-ONLY'
    if s.endswith('_Xbox.cpp') or s.endswith('_X360.cpp') or 'Dx9' in s or 'keygen' in s:
        return '360-ONLY'
    if 'src/system/rndobj/Anim.cpp' in s:   # the one rndobj TU native links
        return 'NATIVE-CORE'
    if any(x in s for x in CORE):
        return 'NATIVE-CORE'
    if any(x in s for x in SOON):
        return 'NATIVE-SOON'
    if any(x in s for x in VIA_DC3):
        return 'NATIVE-VIA-DC3'
    return 'UNKNOWN-other'


def milestone(sp):
    """Native runtime milestone a NATIVE-SOON/CORE TU serves."""
    s = sp or ''
    if any(x in s for x in ('src/system/obj/', 'src/system/utl/', 'src/system/os/',
                            'src/system/math/', 'rndobj/Anim')):
        return 'M0 dta/parse (booted)'
    if any(x in s for x in ('meta_band/BandSong', 'meta_band/MusicLibrary',
                            'meta_band/BandSongMgr', 'meta_band/MetaPerformer',
                            'SongInfo', 'SongMgr', 'SongSort')):
        return 'M1 song load/metadata'
    if 'src/system/midi/' in s:
        return 'M2 chart parse (MIDI)'
    if any(x in s for x in ('src/system/beatmatch/', 'bandtrack/GemManager',
                            'bandtrack/VocalTrack', 'game/GemPlayer',
                            'game/VocalPlayer', 'game/Player', 'game/Game')):
        return 'M3 gameplay/scoring'
    if 'src/system/hamobj/' in s or 'src/meta_ham/' in s:
        return 'M3 game data (Ham)'
    if any(x in s for x in ('meta_band/Profile', 'meta_band/UIStats',
                            'meta_band/SaveLoad', 'meta_band/Campaign',
                            'meta_band/Accomplishment')):
        return 'M4 profile/save/practice'
    if 'src/band3/' in s:
        return 'M4 game UI/flow'
    return 'M? in-scope'


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else 'build/45410914/report.json'
    d = json.load(open(path))
    agg = defaultdict(lambda: {'units': 0, 'tf': 0, 'mf': 0, 'tc': 0, 'mc': 0})
    rows = []
    for u in d['units']:
        sp = u['metadata'].get('source_path')
        cls = classify(sp)
        m = u['measures']
        a = agg[cls]
        a['units'] += 1
        a['tf'] += m.get('total_functions', 0)
        a['mf'] += m.get('matched_functions', 0)
        a['tc'] += int(m.get('total_code', 0))
        a['mc'] += int(m.get('matched_code', 0))
        if cls in ('NATIVE-CORE', 'NATIVE-SOON'):
            tf = m.get('total_functions', 0)
            mf = m.get('matched_functions', 0)
            tc = int(m.get('total_code', 0))
            mc = int(m.get('matched_code', 0))
            rows.append((tc - mc, tf - mf, 100 * mf / tf if tf else 0,
                         cls, sp, tf, mf, milestone(sp)))

    order = ['NATIVE-CORE', 'NATIVE-SOON', 'NATIVE-VIA-DC3', '360-ONLY',
             'UNKNOWN-other', 'UNKNOWN-anon']
    print('## Native-scope class table\n')
    print('| class | units | fns | m_fns | strict fn% | bytes | m_bytes | strict byte% |')
    print('|---|--:|--:|--:|--:|--:|--:|--:|')
    for k in order:
        a = agg[k]
        if not a['units']:
            continue
        fp = 100 * a['mf'] / a['tf'] if a['tf'] else 0
        bp = 100 * a['mc'] / a['tc'] if a['tc'] else 0
        print(f"| {k} | {a['units']} | {a['tf']} | {a['mf']} | {fp:.2f}% | "
              f"{a['tc']} | {a['mc']} | {bp:.2f}% |")

    # coverage headline
    ns_tf = agg['NATIVE-CORE']['tf'] + agg['NATIVE-SOON']['tf']
    ns_mf = agg['NATIVE-CORE']['mf'] + agg['NATIVE-SOON']['mf']
    ns_tc = agg['NATIVE-CORE']['tc'] + agg['NATIVE-SOON']['tc']
    ns_mc = agg['NATIVE-CORE']['mc'] + agg['NATIVE-SOON']['mc']
    wf = sum(a['mf'] for a in agg.values()) / sum(a['tf'] for a in agg.values())
    wc = sum(a['mc'] for a in agg.values()) / sum(a['tc'] for a in agg.values())
    print('\n## Headline\n')
    print(f"- **Native-scope (CORE+SOON, identified): "
          f"{100*ns_mf/ns_tf:.1f}% fns / {100*ns_mc/ns_tc:.1f}% bytes** "
          f"({ns_mf}/{ns_tf} fns).")
    print(f"- Whole-binary: {100*wf:.1f}% fns / {100*wc:.1f}% bytes "
          f"(dragged by UNKNOWN-anon + 360-ONLY).")

    print('\n## In-scope backlog (top 30 by remaining bytes)\n')
    print('| rem_bytes | rem_fns | strict fn% | class | milestone | src |')
    print('|--:|--:|--:|---|---|---|')
    rows.sort(key=lambda x: -x[0])
    for remb, rem, fp, cls, sp, tf, mf, ms in rows[:30]:
        short = sp.replace('src/system/', 'sys/').replace('src/band3/', 'b3/')
        print(f"| {remb} | {rem} | {fp:.1f}% | {cls} | {ms} | {short} |")


if __name__ == '__main__':
    main()
