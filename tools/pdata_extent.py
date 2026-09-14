#!/usr/bin/env python3
"""Authoritative X360 .pdata function-extent decoder (corrected).

Origin: lane W15-B wrote this as scratch (`~/tmp/w15d/probe.py`); W15-D found its
packed-word decode used `>>2` where the field sits at `>>8`; lane W16-F corrected
it, added the selftest below, and landed it here so the fix is not lost with the
scratch directory.

THE BUG. The X360 packed .pdata second word is BIG-ENDIAN and laid out
`PrologLen:8 | FunctionLength:22 | flags:2`, so FunctionLength is bits 29..8 and
is counted in INSTRUCTIONS:

    flen = ((f >> 8) & 0x3FFFFF) * 4      # NOT (f >> 2)

Measured over all 57,733 retail .pdata entries: `>>8` fits the distance to the
next entry 57,732/57,732 (100%, 0 overruns, 61.41% exact); `>>2` OVERRUNS
57,730/57,732 with a maximum "length" of 2,466,308 B.

    python3 tools/pdata_extent.py --selftest

WHAT THE SELFTEST IS WORTH. Check (1), containment, needs no external oracle: a
function cannot extend past the next entry's BeginAddress. It carries a CONTROL
that MUST fail -- if the known-buggy `>>2` ever reports 0 overruns the test is
vacuous and exits non-zero. Check (2) compares three known extents against dtk's
own carve in config/45410914/symbols.txt, which is a SECOND IMPLEMENTATION, not
an independent oracle; stated honestly rather than sold as confirmation.
"""
import os, subprocess

def _repo_root():
    try:
        return subprocess.check_output(['git','rev-parse','--show-toplevel'],
                                       stderr=subprocess.DEVNULL,
                                       cwd=os.path.dirname(os.path.abspath(__file__))).decode().strip()
    except Exception:
        return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

import struct, json, bisect
BIN=os.environ.get('RB3_BAND_EXE') or os.path.join(_repo_root(),'orig','45410914','band.exe')
data=open(BIN,'rb').read()
# PE sections
pe=struct.unpack_from('<I',data,0x3c)[0]
nsec=struct.unpack_from('<H',data,pe+6)[0]
opt=struct.unpack_from('<H',data,pe+20)[0]
base=struct.unpack_from('<I',data,pe+24+28)[0]
SEC=[]
for i in range(nsec):
    o=pe+24+opt+i*40
    nm=data[o:o+8].rstrip(b'\0').decode()
    vs,va,rs,rp=struct.unpack_from('<IIII',data,o+8)
    SEC.append((nm,base+va,vs,rp,rs))
def off(vma):
    for nm,sva,vs,rp,rs in SEC:
        if sva<=vma<sva+max(vs,rs): return rp+(vma-sva)
    return None
def rd(vma,n):
    o=off(vma); return data[o:o+n] if o is not None else b''
def be32(vma): return struct.unpack('>I',rd(vma,4))[0]
# .pdata  (BIG-ENDIAN entries: BeginAddress, then packed flags)
PD=[s for s in SEC if s[0]=='.pdata'][0]
pd_va,pd_sz,pd_rp=PD[1],PD[2],PD[3]
ents=[]
for i in range(pd_sz//8):
    b,f=struct.unpack_from('>II',data,pd_rp+i*8)
    if b==0: continue
    ents.append((b,f))
ents.sort()
starts=[e[0] for e in ents]
def pdata_extent(vma):
    i=bisect.bisect_right(starts,vma)-1
    if i<0: return None
    b,f=ents[i]
    nxt=starts[i+1] if i+1<len(starts) else None
    # X360 packed .pdata second word (BIG-ENDIAN): PrologLen:8 | FunctionLength:22 | flags:2
    # FunctionLength is bits 29..8, in INSTRUCTIONS -> *4 for bytes.
    # CORRECTED by lane W16-F 2026-09-14: this was `(f>>2)` which is off by 6 bits.
    # Measured over all 57,733 retail .pdata entries: `>>8` fits the distance to the
    # next entry 57,732/57,732 (100%, 0 overruns, 61.41% exact); `>>2` OVERRUNS
    # 57,730/57,732 with a max "length" of 2,466,308 B. See selftest() below.
    flen=((f>>8)&0x3FFFFF)*4
    return b,flen,nxt
def selftest():
    """Validate the .pdata decode. Two independent checks:
    (1) CONTAINMENT (no external oracle): a function cannot extend past the next
        .pdata entry's BeginAddress. Counts overruns for both candidate decodes.
    (2) THREE KNOWN EXTENTS cross-checked against dtk's own carve in
        config/45410914/symbols.txt (a second implementation, not a fully
        independent oracle -- stated honestly).
    Returns True iff both pass."""
    ok=True
    for sh,label in ((8,'>>8 (corrected)'),(2,'>>2 (old, buggy)')):
        over=0
        for i,(b,f) in enumerate(ents[:-1]):
            if ((f>>sh)&0x3FFFFF)*4 > starts[i+1]-b: over+=1
        print(f'  containment {label:18}: overruns {over}/{len(ents)-1}')
        if sh==8 and over: ok=False
        if sh==2 and over==0: ok=False   # the control MUST fail, else vacuous
    KNOWN=[(0x822CBC10,0x68),(0x8232A148,0x13C),(0x82344238,0x474)]
    for a,exp in KNOWN:
        e=pdata_extent(a)
        got=e[1] if e and e[0]==a else None
        flag='OK' if got==exp else 'FAIL'
        if got!=exp: ok=False
        print(f'  extent 0x{a:08x}: decoded {got} vs dtk symbols.txt {exp}  {flag}')
    print('SELFTEST', 'PASS' if ok else 'FAIL')
    return ok

if __name__=='__main__':
    import sys
    if '--selftest' in sys.argv[1:]:
        raise SystemExit(0 if selftest() else 1)
    for a in sys.argv[1:]:
        v=int(a,16)
        e=pdata_extent(v)
        print(f'{a}: pdata_begin=0x{e[0]:08x} len={e[1]} next=0x{e[2]:08x}' if e else f'{a}: none')
