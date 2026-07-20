#!/usr/bin/env python3
"""carve-pilot: classify bindiff_r1 carving hints by source-availability +
RB3 VA contiguity, to size the Phase-3 carve campaign.

Run from repo root. Requires sibling repos ../rb3, ../dc3-decomp.
Emits the tractability-bucket table and the carvable-unwired unit list.
"""
import json, re, subprocess, collections, os, sys
ROOT=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# Sibling oracle repos (../rb3, ../dc3-decomp) live next to the MAIN checkout,
# not next to a ~/tmp worktree. Resolve to the real milohax dir.
SIB=os.path.dirname(ROOT)
if not os.path.isdir(os.path.join(SIB,'rb3','src')):
    SIB=os.environ.get('MILOHAX_ROOT','/home/free/code/milohax')
hints=json.load(open(os.path.join(ROOT,'scripts/harvest/bindiff_r1_carving_hints.json')))
xenon_obj=open(os.path.join(ROOT,'config/45410914/objects.json')).read()
srccache={}
def has_src(base):
    if base in srccache: return srccache[base]
    def f(*d):
        return bool(subprocess.run(['find',*[os.path.join(SIB,x) for x in d],'-iname',base+'.cpp'],
                    capture_output=True,text=True).stdout.strip())
    srccache[base]=(f('rb3/src'), f('dc3-decomp/src/system','dc3-decomp/src/xdk'), f('dc3-decomp/src/lazer'))
    return srccache[base]
buckets=collections.Counter(); carv=collections.defaultdict(list)
for h in hints:
    u=h['dc3_unit']
    if u is None: buckets['ANON: no dc3 unit']+=1; continue
    ul=u.lower()
    if any(x in ul for x in ('xboxkrnl','xam.xex','xapilib')):
        buckets['XDK/kernel import thunk (no source)']+=1; continue
    base=u.split('.obj')[0].split('/')[-1].split(' ')[-1]
    wii,dc3e,dc3g=has_src(base); wired=f'{base}.cpp"' in xenon_obj
    if wii and wired: buckets['GAME wii-twin ALREADY WIRED']+=1
    elif wii: buckets['GAME wii-twin UNWIRED (carvable)']+=1; carv[(base,'GAME')].append(int(h['rb3_va'],16))
    elif dc3e and wired: buckets['ENGINE dc3-src ALREADY WIRED']+=1
    elif dc3e: buckets['ENGINE dc3-src UNWIRED (carvable*)']+=1; carv[(base,'ENGINE')].append(int(h['rb3_va'],16))
    elif dc3g: buckets['DC3-only game class (false-ID / absent in RB3)']+=1
    else: buckets['NO SOURCE (XDK/CRT/middleware lib internal)']+=1
print("=== tractability buckets (n=%d) ==="%len(hints))
for k,v in buckets.most_common(): print(f"{v:4d}  {k}")
print("\n=== carvable-unwired units (require >=2 contiguous + real source) ===")
for (base,kind),vas in sorted(carv.items(),key=lambda x:-len(x[1])):
    vas.sort(); print(f"  {kind:6} {base:26} hits={len(vas)} span={vas[-1]-vas[0]:#x}")
print("\nNOTE: shader/main/system 'ENGINE/GAME' rows are false basename matches "
      "(shader.obj=XDK XGraphics, not rndobj/ShaderMgr). Only SaveLoadManager is a "
      "clean contiguous carvable TU in this hint set.")
