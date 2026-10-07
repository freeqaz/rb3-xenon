"""W16-TP: mutation control for tools/row_valueflow.py.

Mutates the TARGET side of one row's diff (swap the sources of a non-commutative op, swap the sources of two
nearby `mr`, change an `li` immediate) and requires the value-flow report to change.  A mutation that changes
nothing observable (e.g. swapping the two operands of a square) is legitimately missed.
Usage: row_valueflow_ctl.py UNIT SYMBOL     (NMUT=n mutations, default 8)
"""
import json,subprocess,sys,re,copy
import os
HERE=os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0,HERE); import row_valueflow as vflow
import tempfile
TMP=tempfile.mkdtemp(prefix='rvf_')
unit,sym=sys.argv[1],sys.argv[2]
d=vflow.get_json(unit,sym)
def run(dd):
    json.dump(dd,open(TMP+'/mut.json','w'))
    p=subprocess.run([sys.executable,HERE+'/row_valueflow.py','--json',TMP+'/mut.json'],capture_output=True,text=True)
    return p.stdout.strip().splitlines() or [p.stderr[-300:]]
base=run(d); print('unmodified:',base[0], len(base)-1,'detail lines')
NONCOMM=('subf','fsubs','fsub','fdivs','divw','divwu','slw','srw','sraw','fdiv','subf.','cmpw','cmplw','fcmpu')
muts=[]
ins=d['instructions']
for k,x in enumerate(ins):
    t=x.get('target')
    if not t or not t.get('args'): continue
    a=[s.strip() for s in t['args'].split(',')]
    if t['opcode'] in NONCOMM:
        src=a[1:] if not a[0].startswith('cr') or t['opcode'].startswith('cmp') and len(a)==3 else a[1:]
        if len(a)==3 and a[1]!=a[2] and vflow.REGRE.match(a[1]) and vflow.REGRE.match(a[2]): muts.append((k,'swap-src',[a[0],a[2],a[1]]))
    if t['opcode']=='li' and len(a)==2:
        muts.append((k,'li-imm',[a[0], '0x1' if a[1]!='0x1' else '0x0']))
    if t['opcode']=='mr':
        for k2 in range(k+1,min(k+5,len(ins))):
            t2=ins[k2].get('target')
            if t2 and t2['opcode']=='mr':
                b=[s.strip() for s in t2['args'].split(',')]
                if b[1]!=a[1]: muts.append((k,'swap-mr',(k2,[a[0],b[1]],[b[0],a[1]]))); break
det=0;tot=0
import os
N=int(os.environ.get('NMUT','8'))
for k,kind,new in muts[:N]:
    dd=copy.deepcopy(d)
    if kind in ('swap-src','li-imm'): dd['instructions'][k]['target']['args']=', '.join(new)
    else:
        k2,n1,n2=new; dd['instructions'][k]['target']['args']=', '.join(n1); dd['instructions'][k2]['target']['args']=', '.join(n2)
    out=run(dd); tot+=1; hit=out!=base; det+=hit
    print(f'  mutate #{k} {kind} -> {out[0]} | {"detected" if hit else "MISSED"}')
print(f'CONTROL {det}/{tot} mutations detected')
