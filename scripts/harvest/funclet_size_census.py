import struct, sys, re, glob
from pathlib import Path
from collections import Counter
def is_funclet_like(n):
    for pre in ("__unwind$","__catch$"):
        if n.startswith(pre): return n[len(pre):].isdigit()
    if n.startswith("__unwind__merged_"): return True
    if n.startswith("fn_"):
        r=n[3:]; return len(r)==8 and all(c in "0123456789abcdefABCDEF" for c in r)
    return n.startswith("??__E") or n.startswith("??__F")
def syms(path):
    d=open(path,'rb').read()
    if len(d)<20: return []
    nsec=struct.unpack_from("<H",d,2)[0]
    symptr,nsym=struct.unpack_from("<II",d,8)
    if not symptr or not nsym: return []
    strtab=symptr+nsym*18
    # section headers: name(8) vsize paddr size ptr ...
    sechdr=struct.unpack_from("<H",d,16)[0]  # optional header size
    so=20+sechdr
    secflags=[]; secsize=[]
    for i in range(nsec):
        o=so+i*40
        secsize.append(struct.unpack_from("<I",d,o+16)[0])
        secflags.append(struct.unpack_from("<I",d,o+36)[0])
    def s_at(off):
        e=d.index(b"\0",strtab+off); return d[strtab+off:e].decode('latin1')
    out=[]; i=0
    while i<nsym:
        o=symptr+i*18
        raw=d[o:o+8]
        name=s_at(struct.unpack_from("<I",raw,4)[0]) if raw[:4]==b"\0\0\0\0" else raw.rstrip(b"\0").decode('latin1')
        val,sec,typ,cls,naux=struct.unpack_from("<IhHBB",d,o+8)
        if sec>0 and cls in (2,3) and not name.startswith('.'):
            out.append((name,sec,val,secsize[sec-1],secflags[sec-1]))
        i+=1+naux
    return out
root=Path('build/45410914/src')
fl=Counter(); allc=Counter(); mx=(0,None)
nobj=0
for p in root.rglob('*.obj'):
    nobj+=1
    ss=syms(p)
    # group by section: for COMDAT sections, one symbol per section -> size = section size
    bysec={}
    for name,sec,val,ssz,flags in ss:
        bysec.setdefault(sec,[]).append((val,name,ssz,flags))
    for sec,items in bysec.items():
        items.sort()
        for j,(val,name,ssz,flags) in enumerate(items):
            if not (flags & 0x20): continue   # CNT_CODE
            end = items[j+1][0] if j+1<len(items) else ssz
            size = end-val
            if size<=0: continue
            allc[size]+=1
            if is_funclet_like(name):
                fl[size]+=1
                if size>mx[0]: mx=(size,"%s %s"%(p.name,name))
print("objs", nobj)
print("funclet-like code symbols:", sum(fl.values()), "max size:", mx)
print(">84B funclet-like:", sum(v for k,v in fl.items() if k>84))
print("funclet-like size top:", sorted(fl.items())[-12:])
print("ALL code symbols:", sum(allc.values()), " >84B:", sum(v for k,v in allc.items() if k>84))
