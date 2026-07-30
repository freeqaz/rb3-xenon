#!/usr/bin/env python3
"""Build class->bucket maps + vendor set, emit tools/scope_data/name_class.json
and uid_merge.json intermediates. Reusable; run from repo root."""
import json, os, re, subprocess
from collections import defaultdict, Counter
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
ROOT='/home/free/code/milohax/rb3-xenon'
DC3='/home/free/code/milohax/dc3-decomp/src'
RB3='/home/free/code/milohax/rb3/src'

NOISE={'for','in','that','if','the','to','of','is','as','T','T1','T2','T3','Impl',
'iterator','rebind','members','Entry','Node','Key','Range','Stream','Buffer','Callback',
'Loader','Message','Job','Friend'}

def extract(paths):
    """Grep `class/struct Name` decls from dirs (recursive) or single files."""
    out=set()
    for p in paths:
        args=['grep','-hoE',r'\b(class|struct)\s+[A-Za-z_][A-Za-z0-9_]*']
        if os.path.isdir(p):
            args=['grep','-rhoE',r'\b(class|struct)\s+[A-Za-z_][A-Za-z0-9_]*',p,
                  '--include=*.h','--include=*.hpp','--include=*.cpp']
        elif os.path.isfile(p):
            args.append(p)
        else:
            continue
        try:
            r=subprocess.run(args,capture_output=True,text=True)
        except Exception: continue
        for line in r.stdout.splitlines():
            t=line.split()
            if len(t)==2: out.add(t[1])
    return out

# root-level platform/app files (App.h, Main.cpp, ...) -> engine (root .cpp rule)
def root_files(base):
    import glob
    return [f for pat in ('*.h','*.cpp') for f in glob.glob(base+'/'+pat)]

engine_cls=extract([DC3+'/system', RB3+'/system']+root_files(DC3)+root_files(RB3))
game_cls=extract([RB3+'/band3', RB3+'/network', DC3+'/lazer'])
# drop noise tokens
engine_cls-=NOISE; game_cls-=NOISE

# empirical vendor classes from uid xdk attributions
VENDOR_XDK={'xdk/d3dx9','xdk/xgraphics','xdk/xaudio2','xdk/d3d9i','xdk/xhv2',
'xdk/xapilibi','xdk/xonline','xdk/xmic','xdk/xinput2','xdk/xmcore','xdk/xnet',
'xdk/xlrc','xdk/xmp','xdk/xparty','xdk/xjson','xdk/xbdm','xdk/nuiaudio','xdk/nuispeech','xdk/ST'}
d=_guarded_load(ROOT+'/unified_id.json')
def topclass(dn):
    if not dn: return None
    dn=re.sub(r'<.*?>','',dn)
    p=dn.split('::')
    return p[-2] if len(p)>=2 else None
cls_src=defaultdict(Counter)
for e in d:
    if e.get('similarity',0)<0.9: continue
    src=(e.get('bindiff_src') or '').replace('../dc3-decomp/src/','')
    key=src.split('/')[0]+('/'+src.split('/')[1] if '/' in src else '')
    c=topclass(e.get('dc3_name_demangled'))
    if c: cls_src[c][key]+=1
vendor_cls=set()
for c,cnt in cls_src.items():
    tot=sum(cnt.values()); vend=sum(v for k,v in cnt.items() if k in VENDOR_XDK)
    if tot and vend>=2 and vend/tot>=0.8:
        vendor_cls.add(c.lstrip('?$'))
# A class that ALSO exists in an in-scope source tree is matchable work, not
# vendor -- drop it from the vendor set so the emitted file is self-consistent
# with classify_name's in-scope-wins rule (Stack/Block/Scheduler/Queue collide).
collide=vendor_cls & (engine_cls | game_cls)
vendor_cls-=collide

print('engine_cls:',len(engine_cls),'game_cls:',len(game_cls),
      'vendor_cls:',len(vendor_cls),'(dropped %d in-scope collisions)'%len(collide))
# emit
os.makedirs(ROOT+'/tools/scope_data',exist_ok=True)
json.dump({'engine':sorted(engine_cls),'game':sorted(game_cls),
           'vendor':sorted(vendor_cls)},
          open(ROOT+'/tools/scope_data/name_class.json','w'),indent=0)
print('wrote name_class.json')
