"""W16-TP: algebraic check of straight-line FP code, for rows with reassociated /fp:fast arithmetic.

Evaluates both sides of one row at five random points in float64 (retail literal-pool constants read from
band.exe through row_valueflow's normaliser) and compares every non-frame store.  Equal at all points means the
stored values are the same polynomial/rational expression up to rounding (Schwartz-Zippel), i.e. the difference
is association/scheduling.  Straight-line code only: branches and calls are not followed.
Usage: row_fp_algebra.py UNIT SYMBOL | --json FILE
"""
import json,sys,re,random,struct
import os
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__))); import row_valueflow as vflow
ENV={}
def sym(n):
    if n not in ENV: ENV[n]=random.uniform(-2,2)
    return ENV[n]
def run(L):
    R={}; M={}; out={}
    def g(r):
        if r not in R: R[r]=sym(r+'_in')
        return R[r]
    for I in L:
        if I is None: continue
        op=I['op']; a=I['args']
        m=re.match(r'(-?0x[0-9a-f]+|-?\d+|.*@l)\((r\d+)\)',a[1]) if len(a)>1 else None
        if op in('lfs','lfd') and m:
            off=m.group(1)
            if '__real@' in off:
                h=re.search(r'__real@([0-9a-f]+)',off).group(1)
                R[a[0]]=struct.unpack('>f' if len(h)==8 else '>d',bytes.fromhex(h))[0]
            else: R[a[0]]=sym(f"M_{m.group(2)}_{off}")
        elif op in('stfs','stfd') and m and m.group(2)!='r1': out[(m.group(2),m.group(1))]=g(a[0])
        elif op=='fmuls': R[a[0]]=g(a[1])*g(a[2])
        elif op=='fadds': R[a[0]]=g(a[1])+g(a[2])
        elif op=='fsubs': R[a[0]]=g(a[1])-g(a[2])
        elif op=='fmadds': R[a[0]]=g(a[1])*g(a[2])+g(a[3])
        elif op=='fmsubs': R[a[0]]=g(a[1])*g(a[2])-g(a[3])
        elif op=='fnmsubs': R[a[0]]=-(g(a[1])*g(a[2])-g(a[3]))
        elif op=='fnmadds': R[a[0]]=-(g(a[1])*g(a[2])+g(a[3]))
        elif op=='fneg': R[a[0]]=-g(a[1])
        elif op=='fmr': R[a[0]]=g(a[1])
        elif op=='fdivs': R[a[0]]=g(a[1])/g(a[2])
    return out
d=vflow.get_json(sys.argv[1],sys.argv[2]) if sys.argv[1]!='--json' else json.load(open(sys.argv[2]))
ins=d['instructions']
T=vflow.parse_side(ins,'target'); B=vflow.parse_side(ins,'base')
res={}
for trial in range(5):
    ENV.clear(); oT=run(T); oB=run(B)
    for k in set(oT)|set(oB):
        a=oT.get(k); b=oB.get(k)
        ok = a is not None and b is not None and abs(a-b)<=1e-9*max(1,abs(a),abs(b))
        res.setdefault(k,[]).append((ok,a,b))
for k in sorted(res): print(k, 'EQUAL' if all(x[0] for x in res[k]) else 'DIFF', '' if all(x[0] for x in res[k]) else res[k][0][1:])
