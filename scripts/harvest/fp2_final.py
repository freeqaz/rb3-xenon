#!/usr/bin/env python3
"""Final ranked candidate assembly: filter noise, grade, check source LOC + dc3/wii existence."""
import json, os, glob
ROOT="/home/free/code/milohax/rb3-xenon"
g=json.load(open('/home/free/tmp/fp2_runs.json'))

# round-1 basenames to exclude/flag
r1={'NewAwardPanel.cpp','CampaignGoalsLeaderboardPanel.cpp','CharCache.cpp','StoreOfferProvider.cpp',
 'RGTrainerPanel.cpp','InterstitialMgr.cpp','ModifierMgr.cpp','MetaNetMsgs.cpp','BandStorePanel.cpp',
 'Platform.cpp','ConnectionInfoDDL.cpp','BandwidthCounter.cpp','Scoring.cpp','GemRepTemplate.cpp',
 'ContentLoadingPanel.cpp'}

def channel(c):
    p=c['src']
    if '/band3/' in p or '/network/' in p: return 'game'
    if 'dc3-decomp' in p or '/system/' in p: return 'engine'
    return '?'

def looc(base):
    # source LOC from rb3-Wii or dc3
    for root in ['../rb3/src','../dc3-decomp/src']:
        hits=glob.glob(f"{ROOT}/{root}/**/{base}",recursive=True)
        if hits:
            try: return sum(1 for _ in open(hits[0])), hits[0].split('/src/')[-1]
            except: pass
    return None,None

rows=[]
for c in g:
    if not c['base']: continue
    if c['base'].startswith('App.') or c['base'].startswith('.permuter'): continue
    if c['base'].endswith('.h'): continue  # inlined header attribution, skip
    ch=channel(c)
    if ch=='?': continue
    if c['nb']>40000: continue  # multi-TU blob
    loc,srcpath=looc(c['base'])
    rows.append(dict(base=c['base'],ch=ch,start=c['start'],end=c['end'],nf=c['nf'],nb=c['nb'],
        nstr=c['nstr'],loc=loc,srcpath=srcpath,intree=c['intree'],
        r1=c['base'] in r1, strings=c['strings']))

# rank by string evidence
rows.sort(key=lambda r:(-r['nstr'],-r['nf']))
json.dump(rows,open('/home/free/tmp/fp2_final.json','w'),indent=1)
print(f"{'BASE':30s} {'CH':6s} {'SPAN':21s} {'FN':>4s} {'BYT':>6s} {'STR':>4s} {'LOC':>5s} r1 intree")
for r in rows:
    tag='R1' if r['r1'] else '  '
    it='IT' if r['intree'] else '  '
    print(f"{r['base']:30s} {r['ch']:6s} {r['start']:08x}..{r['end']:08x} {r['nf']:4d} {r['nb']:6d} {r['nstr']:4d} {str(r['loc']):>5s} {tag} {it}")
