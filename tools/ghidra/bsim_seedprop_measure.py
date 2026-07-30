#!/usr/bin/env python3
"""Measurement: cross-tool precision + per-stem densification for the seed-prop experiment.
Inputs: seedprop_matches.json, baseline_matches.json, seeds.json, class_to_stem.json,
        ../unified_id_rb3wii.json. See the .md/.json verdict for the numbers this reproduces."""
import json,sys,os
from collections import defaultdict
# --- dead-index guard (lane BX-4) -------------------------------------------
# These address indices are TU0-era and INFORMATIONLESS after the 2026-07-15
# TU0->TU5 flip (2-6% of their addresses are real .text function starts; an
# arbitrary address list scores ~2-3% by chance). Acting on them yields
# plausible-looking WRONG artifacts, so the load is hard-gated.
# Audit: python3 tools/dead_index_guard.py --audit
import os as _dig_os, sys as _dig_sys
_dig_d = _dig_os.path.dirname(_dig_os.path.abspath(__file__))
while _dig_d != "/" and not _dig_os.path.exists(
        _dig_os.path.join(_dig_d, "tools", "dead_index_guard.py")):
    _dig_d = _dig_os.path.dirname(_dig_d)
_dig_sys.path.insert(0, _dig_os.path.join(_dig_d, "tools"))
from dead_index_guard import load_guarded as _guarded_load, assert_live as _assert_live  # noqa: E402
# ----------------------------------------------------------------------------
B='/tmp/bsim_seed'
tr=json.load(open(f'{B}/seedprop_matches.json')); bl=json.load(open(f'{B}/baseline_matches.json'))
seeds=json.load(open(f'{B}/seeds.json')); cls_stem=json.load(open(f'{B}/class_to_stem.json'))
uni=_guarded_load('/home/free/code/milohax/rb3-xenon/unified_id_rb3wii.json')
oracle={u['rb3_addr'].lower():(u.get('wii_fn') or u.get('wii_name'),u.get('confidence',0)) for u in uni}
seed_dst=set(s['dst_va'].lower() for s in seeds)
cls=lambda n:( (n or '').split('(')[0].split('::')[-2] if len((n or '').split('(')[0].split('::'))>=2 else None)
norm=lambda n:(n or '').split('(')[0].replace(' ','').replace('_','').lower()
def xtool(rows,get,sm):
    ag=di=0
    for r in rows:
        t=get(r)
        if not t or t['dst_va'].lower() in seed_dst or t['sz']<=44 or t['sim']<sm: continue
        o=oracle.get(t['dst_va'].lower())
        if not o or o[1]<0.9: continue
        if cls(t['wii_name'])==cls(o[0]) or norm(t['wii_name'])==norm(o[0]): ag+=1
        else: di+=1
    return ag,di
tg=lambda r:({'dst_va':r['dst_va'],'wii_name':r['wii_name'],'sim':r.get('sim') or 0,'sz':r['dst_size']} if r['matchset']=='BSim Function Matching' else None)
bg=lambda r:({'dst_va':r['dst_va'],'wii_name':r['top'][0]['wii_name'],'sim':r['top'][0]['sim'],'sz':r['dst_size']} if r['top'] else None)
for sm in (0.5,0.7,0.9,0.95):
    ta,td=xtool(tr,tg,sm); ba,bd=xtool(bl,bg,sm)
    print(f"sim>={sm}: TREAT {ta}/{ta+td}={ta/(ta+td):.2f} | BASE {ba}/{ba+bd}={ba/(ba+bd):.2f}" if (ta+td and ba+bd) else f"sim>={sm}: thin")
