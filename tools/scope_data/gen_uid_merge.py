#!/usr/bin/env python3
"""Generate tools/scope_data/uid_merge.json from unified_id.json + unified_id_rb3wii.json.
Addr-keyed (HEXADDR-no-0x, 8 upper) provenance layer for scope_map. Reproducible
from checkout (the two unified_id*.json live at repo root). Conservative thresholds:
dc3 uid sim>=0.9, rb3wii uid sim>=0.7."""
import json, os
from collections import Counter
ROOT=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) \
    if '__file__' in dir() else '/home/free/code/milohax/rb3-xenon'
ROOT='/home/free/code/milohax/rb3-xenon'

THIRDPARTY_MARKERS=("/zlib/","/oggvorbis/","/json-c/","/curl/","/tomcrypt/",
    "/speex/","/expat/","/stlport/","/libpng/","/jpeg/","/openssl/","binkxenon")
VENDOR_XDK_DIRS=('/d3dx9/','/xgraphics/','/xaudio2/','/d3d9i/','/xhv2/','/xapilibi/',
'/xonline/','/xmic/','/xinput2/','/xmcore/','/xnet/','/xlrc/','/xmp/','/xparty/',
'/xjson/','/xbdm/','/nuiaudio/','/nuispeech/','/ST/','/xact3/','/xmedia/')

def bucket_uid_src(src):
    if not src: return None
    low=('/'+src.lower().replace('../dc3-decomp/src/','').replace('../rb3/src/',''))
    for m in THIRDPARTY_MARKERS:
        if m in low: return 'thirdparty'
    if '/xdk/libcmt/' in low: return 'crt'
    for v in VENDOR_XDK_DIRS:
        if v in low: return 'vendor'
    if '/xdk/nuiapi/' in low: return 'xdk'
    if '/xdk/' in low: return 'vendor'
    if 'bink' in low or '/rad' in low: return 'vendor'
    if low.startswith('/band3/') or low.startswith('/network/') or '/lazer/' in low:
        return 'game'
    if '/system/' in low: return 'engine'
    base=low.lstrip('/')
    if base.count('/')==0 and base.endswith('.cpp'): return 'engine'
    return None

merged={}  # HEXADDR -> {bucket, src, sim, source, conf}
stats=Counter()

def add(a_hex, bucket, src, sim, source, conf):
    cur=merged.get(a_hex)
    # dc3 beats wii (higher trust); higher sim beats lower on ties
    rank=(0 if source=='uid_rb3wii' else 1, sim or 0)
    if cur is None or rank>cur['_rank']:
        merged[a_hex]={'bucket':bucket,'src':src,'sim':round(sim,4) if sim else sim,
                       'source':source,'conf':conf,'_rank':rank}

# dc3 uid: sim>=0.9 -> conf 0.95
d=json.load(open(ROOT+'/unified_id.json'))
for e in d:
    if (e.get('similarity') or 0)<0.9: continue
    a=e.get('rb3_addr','')
    if not a.startswith('0x'): continue
    b=bucket_uid_src(e.get('bindiff_src') or '')
    if b is None: stats['dc3_nobucket']+=1; continue
    add('%08X'%int(a,16), b, e.get('bindiff_src'), e.get('similarity'), 'uid_dc3', 0.95)
    stats['dc3_'+b]+=1

# rb3wii uid: sim>=0.7 -> conf 0.7 (median 0.25 is noise; 0.7 floor per spike)
d2=json.load(open(ROOT+'/unified_id_rb3wii.json'))
for e in d2:
    if (e.get('similarity') or 0)<0.7: continue
    a=e.get('rb3_addr','')
    if not a.startswith('0x'): continue
    b=bucket_uid_src(e.get('bindiff_src') or '')
    if b is None: stats['wii_nobucket']+=1; continue
    add('%08X'%int(a,16), b, e.get('bindiff_src'), e.get('similarity'), 'uid_rb3wii', 0.7)
    stats['wii_'+b]+=1

for v in merged.values(): v.pop('_rank',None)
out={'_meta':{'thresholds':{'dc3':0.9,'rb3wii':0.7},'stats':dict(stats)},
     'entries':merged}
json.dump(out, open(ROOT+'/tools/scope_data/uid_merge.json','w'), indent=0, sort_keys=True)
print('wrote uid_merge.json:', len(merged),'entries')
print('stats:', dict(stats))
