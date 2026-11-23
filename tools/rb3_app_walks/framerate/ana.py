#!/usr/bin/env python3
# ana.py run.log [pN]: print the frames where p0's (or pN's) bonus / multiplier / energy state changes
import re,sys
fn=sys.argv[1]; pn=sys.argv[2] if len(sys.argv)>2 else 'p0'
keys=['deploy','uni','od','deploys','mult','solo','coda','pts','ssolo','spct','scoda','whm0']
prev=None; last=None
for line in open(fn,errors='replace'):
    if line.startswith('rb3-app-det: song coda') or line.startswith('rb3-app-chartplay: song') or 'coda freestyle' in line: print(line.strip()); continue
    if not line.startswith('rb3-app-det: frame') or ' screen game_screen' not in line: continue
    fr=int(line.split()[2])
    i=line.find(' %s track'%pn)
    if i<0: continue
    seg=line[i:]; j=seg.find(' p',3)
    m=re.search(r'energy ([\d.]+) deploy (\d) long (\d+) uni (\S+) od (\S+) deploys (\d+) mult (\d+ \([\d,]+\)) ind \d+ faildep (\d) saved \d+(?: solo (\S+) coda (\S+) pts (\d+) ssolo (\d+) spct (\d+) scoda (\d+) whm (\S+))? score (\d+) hit (\d+) miss (\d+) streak (\d+)',seg)
    if not m: continue
    g=m.groups()
    bc=re.search(r'bcoda (\S+)',line); bm=re.search(r'bmult (\S+) bonus (\S+)',line); un=re.search(r'unison dis (\S+) fin (\S+) in (\d+)',line)
    st=dict(deploy=g[1],uni=g[3],od=g[4],deploys=g[5],mult=g[6],faildep=g[7],solo=g[8],coda=g[9],pts=g[10],ssolo=g[11],spct=g[12],scoda=g[13],whm0=(g[14] or '').split('/')[0],bcoda=bc and bc.group(1),bmult=bm and bm.groups(),uin=un and un.group(3))
    if st!=prev:
        ch=[k for k in st if prev is None or st[k]!=prev[k]]
        print('fr %d score %s hit %s miss %s streak %s energy %s | '%(fr,g[15],g[16],g[17],g[18],g[0])+' '.join('%s=%s'%(k,st[k]) for k in ch))
    prev=st; last=(fr,g[15],g[16],g[17],g[0])
print('last',last)
