#!/usr/bin/env python3
"""Find TUs whose RETAIL Handle/OnMsg bodies construct handler Symbols as
function-local statics (bl ??0Symbol@@QAA@PBD@Z inside the body) while OUR
compiled body does not -- i.e. TUs that want /DRB3_HANDLE_LOCAL_STATIC."""
import json, os, re, struct, sys

PROJ = sys.argv[1] if len(sys.argv) > 1 else os.getcwd()
SYMCTOR = "??0Symbol@@QAA@PBD@Z"

def read_coff(path):
    try: d = open(path,'rb').read()
    except OSError: return None
    if len(d) < 20: return None
    mach,nsec,ts,psym,nsym,osz,ch = struct.unpack_from("<HHIIIHH", d, 0)
    if not psym or not nsym: return None
    strtab = psym + nsym*18
    secs=[]
    for s in range(nsec):
        off = 20+osz+s*40
        name = d[off:off+8].rstrip(b'\0').decode('latin1')
        vsz,va,rawsz,rawptr = struct.unpack_from("<IIII", d, off+8)
        prel,plno,nrel,nlno = struct.unpack_from("<IIHH", d, off+24)
        flags = struct.unpack_from("<I", d, off+36)[0]
        secs.append(dict(name=name,rawptr=rawptr,rawsz=rawsz,prel=prel,nrel=nrel,flags=flags))
    # symbol table
    syms=[]; i=0
    while i < nsym:
        off = psym+i*18
        raw = d[off:off+8]
        if raw[:4]==b"\0\0\0\0":
            so = struct.unpack_from("<I", raw, 4)[0]
            e = d.index(b"\0", strtab+so); name = d[strtab+so:e].decode('latin1')
        else:
            name = raw.rstrip(b"\0").decode('latin1')
        val,secn,typ,sc,naux = struct.unpack_from("<IhHBB", d, off+8)
        syms.append((name,val,secn,sc))
        for _ in range(naux): syms.append(None)
        i += 1+naux
    # code symbols with their section
    funcs={}
    for idx,s in enumerate(syms):
        if not s: continue
        name,val,secn,sc = s
        if secn<=0 or secn>len(secs): continue
        sec = secs[secn-1]
        if not (sec['flags'] & 0x20): continue
        funcs.setdefault(name,(secn-1,val))
    # count relocs to SYMCTOR per section
    ctor_per_sec={}
    for si,sec in enumerate(secs):
        if not sec['nrel']: continue
        c=0
        for r in range(sec['nrel']):
            o = sec['prel']+r*10
            va,symidx,typ = struct.unpack_from("<IIH", d, o)
            s = syms[symidx] if symidx < len(syms) else None
            if s and s[0]==SYMCTOR: c+=1
        if c: ctor_per_sec[si]=c
    return funcs, ctor_per_sec

def main():
    od = json.load(open(os.path.join(PROJ,'objdiff.json')))
    rep = json.load(open(os.path.join(PROJ,'build/45410914/report.json')))
    repunits = {u['name']:u for u in rep['units']}
    objs = json.load(open(os.path.join(PROJ,'config/45410914/objects.json')))
    flags={}
    def walk(o):
        if isinstance(o,dict):
            n=o.get('name')
            if isinstance(n,str) and n.endswith('.cpp'):
                flags[n]=('RB3_HANDLE_LOCAL_STATIC' in json.dumps(o.get('extra_cflags',[])))
            for v in o.values(): walk(v)
        elif isinstance(o,list):
            for v in o: walk(v)
    walk(objs)
    out=[]
    for u in od['units']:
        md=u.get('metadata') or {}
        if md.get('auto_generated'): continue
        tp,bp = u.get('target_path'), u.get('base_path')
        if not tp or not bp: continue
        ru = repunits.get(u['name'])
        if not ru: continue
        src = md.get('source_path')
        if src and flags.get(src, False): continue   # already gated
        t = read_coff(os.path.join(PROJ,tp)); b = read_coff(os.path.join(PROJ,bp))
        if not t or not b: continue
        tf,tc = t; bf,bc = b
        pct = {f['name']:f.get('match_percent_normalized',0.0) for f in ru.get('functions',[])}
        size = {f['name']:int(f.get('size',0) or 0) for f in ru.get('functions',[])}
        tot_t=0; tot_b=0; fns=[]
        for name,(secn,val) in tf.items():
            if not (name.startswith('?Handle@') or name.startswith('?OnMsg@')): continue
            p = pct.get(name)
            if p is None or p>=100.0: continue
            tcn = tc.get(secn,0)
            bsec = bf.get(name)
            bcn = bc.get(bsec[0],0) if bsec else 0
            if tcn>bcn:
                fns.append((name,p,tcn,bcn,size.get(name,0)))
                tot_t+=tcn; tot_b+=bcn
        if fns:
            out.append(dict(unit=u['name'],src=src,fns=fns,gain=tot_t-tot_b))
    out.sort(key=lambda r:-r['gain'])
    for r in out:
        print("%-4d %-46s %s" % (r['gain'], r['unit'], r['src']))
        for n,p,tc_,bc_,sz in sorted(r['fns'],key=lambda x:-(x[2]-x[3]))[:6]:
            print("        %5.1f%%  tgtSymCtor=%-3d ourSymCtor=%-3d sz=%-5d %s" % (p,tc_,bc_,sz,n[:70]))
    print("\nunits needing gate:", len(out))

main()
