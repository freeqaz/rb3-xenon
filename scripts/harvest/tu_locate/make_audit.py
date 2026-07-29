from _paths import SCRATCH, REPO, BANDEXE, WII_SRC  # noqa: E402
"""Regenerate <SCRATCH>/audit.json: per-TU existence evidence for the 141
REAL_UNLOCATED rb3-Wii TUs (laneBB's oracle_coverage_matrix.py --reverse,
ABSENT & non-network & !WII_PLATFORM).  Inputs: real_unlocated.json (committed
next to this file), the retail PE, and the rb3-Wii source tree.

Fields per row: canon, stem, wii_fns, wii_bytes, has_src, rtti (exact
.?AV<Stem>@@ type descriptor), tier (RTTI_EXACT / RTTI_TMPL_OR_MEMBER /
CLASS_STRING / None), nlits, nlit_hit, lit_hits, nmap.
Controls are in the __main__ block and MUST both pass before the output is used.
"""
import json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
WII = WII_SRC
LIT = re.compile(r'"((?:[^"\\\n]|\\.){4,})"')

POS = ['BandCharacter', 'SongSortMgr', 'TrackPanelDir', 'StarDisplay', 'GemTrackDir',
       'VocalTrackDir', 'MetaPanel', 'BandSwatch', 'CrowdMeterIcon', 'OverdriveMeter',
       'PatchDir', 'MeshAnim', 'Character', 'UIPanel', 'BandProfile']
NEG = ['MicWii', 'ContentMgrWii', 'PlatformMgrWii', 'JoypadWii', 'MemcardWii', 'SynthWii',
       'WiiProfileMgr', 'HomeMenuWii', 'CommerceMgrWii', 'VoiceWii', 'FooBarBaz']


def binary_strings():
    out = subprocess.run(['strings', '-a', BANDEXE], capture_output=True, text=True).stdout
    return out, set(out.split('\n'))


def main():
    raw, strs = binary_strings()
    rtti_raw = '\n'.join(sorted(set(re.findall(r'\.\?A[VU][^\s]{2,80}@@', raw))))

    def tier(stem):
        if f'.?AV{stem}@@' in rtti_raw or f'.?AU{stem}@@' in rtti_raw:
            return 'RTTI_EXACT'
        if f'.?AV?${stem}@' in rtti_raw or f'@{stem}@' in rtti_raw or f'V{stem}@@' in rtti_raw:
            return 'RTTI_TMPL_OR_MEMBER'
        if stem in strs:
            return 'CLASS_STRING'
        return None

    npos = sum(1 for c in POS if tier(c))
    nneg = sum(1 for c in NEG if tier(c))
    print(f'positive control {npos}/{len(POS)}   negative control {nneg}/{len(NEG)} false positives')
    if npos < len(POS) - 1 or nneg > 0:
        sys.exit('CONTROL FAILED - do not trust this run')

    tm = json.load(open(os.path.join(REPO, 'scripts/target_symbol_map.json')))
    tmnames = set(v for v in tm.values() if isinstance(v, str))
    real = json.load(open(os.path.join(HERE, 'real_unlocated.json')))
    rows = []
    for canon, mod, stem_l, st, nf, nb in real:
        parts = canon.split('/')
        d = os.path.join(WII, *parts[:-1])
        src = None
        if os.path.isdir(d):
            for fn in os.listdir(d):
                if fn.lower() in (parts[-1] + '.cpp', parts[-1] + '.c'):
                    src = os.path.join(d, fn)
                    break
        stem = os.path.basename(src)[:-4] if src else parts[-1]
        lits = set()
        for ff in ([src] if src else []) + ([src[:-4] + '.h'] if src and os.path.exists(src[:-4] + '.h') else []):
            for m in LIT.finditer(open(ff, errors='replace').read()):
                s = m.group(1)
                if len(s) >= 5 and not s.startswith('%') and '\\' not in s:
                    lits.add(s)
        hit = sorted(s for s in lits if s in strs)
        rows.append(dict(canon=canon, stem=stem, wii_fns=nf, wii_bytes=nb, has_src=bool(src),
                         rtti=tier(stem) == 'RTTI_EXACT', tier=tier(stem), nlits=len(lits),
                         nlit_hit=len(hit), lit_hits=hit[:8],
                         nmap=sum(1 for n in tmnames if f'@{stem}@@' in n)))
    rows.sort(key=lambda r: -r['wii_fns'])
    json.dump(rows, open(SCRATCH + '/audit.json', 'w'), indent=1)
    print(f'wrote {SCRATCH}/audit.json  ({len(rows)} TUs)')


if __name__ == '__main__':
    main()
