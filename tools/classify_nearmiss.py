#!/usr/bin/env python3
"""Classify the mismatch CAUSE of near-miss functions via objdiff per-fn JSON.

For each function in a match band, run objdiff diff -f json and bucket each
non-equal instruction into a cause class, then assign the function a dominant
class. This empirically separates the "naming climbs it for free" population
from the "source-fidelity bug" / "permuter-class" / "mis-paired" populations.

Cause classes per mismatch instruction:
  NAME_RELOC  : same opcode, args differ ONLY by a symbol token where the
                target side is fn_/lbl_/sub_ (unnamed) and base is named.
                -> climbs when we name that target symbol.
  WRONG_PAIR  : a bl/data ref where BOTH sides are named but to DIFFERENT,
                semantically-unrelated symbols (e.g. RndMat vs CharPollableSorter)
                -> objdiff mis-paired the top-level symbol; needs re-pin.
  OFFSET      : same opcode, a Signed/disp immediate differs (struct offset / stack)
                -> source-fidelity (struct layout) or regalloc(stack) bug.
  REG         : same opcode, only a Register field differs -> permuter-class (regalloc).
  OPCODE      : opcode itself differs -> genuine code divergence.
  OTHER       : length mismatch / missing side / branch target / misc.
"""
import json, subprocess, sys, re, os, argparse
from collections import Counter, defaultdict

ROOT="/home/free/code/milohax/rb3-xenon"
UNNAMED=re.compile(r'\b(fn_[0-9A-Fa-f]+|lbl_[0-9A-Fa-f]+|sub_[0-9A-Fa-f]+|loc_[0-9A-Fa-f]+)\b')

def tok_syms(args):
    # return set of symbol-like tokens (mangled names / labels), strip pure regs/imm
    out=set()
    for t in re.split(r'[,\s]+', args or ''):
        t=t.strip()
        if not t: continue
        if re.fullmatch(r'r\d+|f\d+|cr\d+|0x[0-9A-Fa-f]+|-?\d+', t): continue
        out.add(t)
    return out

def classify_insn(ins):
    mt=ins.get('match_type')
    if mt=='equal': return None
    tg=ins.get('target'); bs=ins.get('base')
    if not tg or not bs:
        return 'OTHER'
    if tg.get('opcode')!=bs.get('opcode'):
        return 'OPCODE'
    # same opcode; compare typed args
    ta=tg.get('typed_args',[]); ba=bs.get('typed_args',[])
    # register-only diff?
    reg_diff=False; imm_diff=False; sym_diff=False
    tsy=tok_syms(tg.get('args')); bsy=tok_syms(bs.get('args'))
    if tsy!=bsy:
        # symbol token differs
        only_t=tsy-bsy; only_b=bsy-tsy
        t_unnamed=any(UNNAMED.fullmatch(x) for x in only_t)
        b_named=any(not UNNAMED.fullmatch(x) for x in only_b)
        if t_unnamed and b_named:
            return 'NAME_RELOC'
        # both named but different
        if only_t and only_b and not t_unnamed:
            return 'WRONG_PAIR'
        return 'NAME_RELOC' if t_unnamed else 'WRONG_PAIR'
    # args differ but symbol tokens same -> register or immediate
    if len(ta)==len(ba):
        for x,y in zip(ta,ba):
            if x.get('value')!=y.get('value'):
                if x.get('type')=='Register': reg_diff=True
                else: imm_diff=True
    else:
        return 'OTHER'
    if imm_diff: return 'OFFSET'
    if reg_diff: return 'REG'
    return 'OTHER'

def diff_fn(unit, sym):
    out="/home/free/tmp/_cls.json"
    r=subprocess.run([os.path.join(ROOT,'bin','objdiff-cli'),'diff','-p',ROOT,'-u',unit,
                      sym,'-f','json','-o',out,'--include-instructions'],
                     capture_output=True, text=True, timeout=120)
    try:
        return json.load(open(out))
    except Exception:
        return None

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--lo",type=float,default=99.0)
    ap.add_argument("--hi",type=float,default=100.0)
    ap.add_argument("--limit",type=int,default=0)
    ap.add_argument("--units",default="")
    ap.add_argument("--out",default="/home/free/tmp/nearmiss_classified.json")
    a=ap.parse_args()
    rep=json.load(open(os.path.join(ROOT,'build/45410914/report.json')))
    unit_filter=set(u.strip() for u in a.units.split(',') if u.strip())
    targets=[]
    for unit in rep['units']:
        un=unit['name']
        if unit_filter and un not in unit_filter: continue
        for f in unit.get('functions',[]):
            mp=f.get('match_percent_normalized',0.0)
            if a.lo<=mp<a.hi:
                targets.append((un,f['name'],mp,int(f.get('size',0))))
    targets.sort(key=lambda t:-t[3])
    if a.limit: targets=targets[:a.limit]
    print(f"classifying {len(targets)} fns in [{a.lo},{a.hi})",file=sys.stderr)
    results=[]
    fn_class_counter=Counter()
    for i,(un,sym,mp,sz) in enumerate(targets):
        d=diff_fn(un,sym)
        if not d:
            fn_class_counter['DIFF_FAIL']+=1; continue
        cc=Counter()
        for ins in d.get('instructions',[]):
            c=classify_insn(ins)
            if c: cc[c]+=1
        # dominant class for the function
        if not cc:
            dom='CLEAN'  # no mismatches found (rounding)
        else:
            # priority: WRONG_PAIR / OPCODE > OFFSET > NAME_RELOC > REG
            if cc.get('WRONG_PAIR') and cc['WRONG_PAIR']>=3: dom='WRONG_PAIR'
            elif cc.get('OPCODE'): dom='OPCODE'
            elif cc.get('OFFSET'): dom='OFFSET'
            elif cc.get('WRONG_PAIR'): dom='WRONG_PAIR'
            elif cc.get('NAME_RELOC'): dom='NAME_RELOC'
            elif cc.get('REG'): dom='REG'
            else: dom='OTHER'
        fn_class_counter[dom]+=1
        results.append({'unit':un,'sym':sym,'mp':mp,'size':sz,'dominant':dom,'counts':dict(cc)})
        if (i+1)%50==0: print(f"  {i+1}/{len(targets)}",file=sys.stderr)
    json.dump(results,open(a.out,'w'),indent=1)
    print("\n=== DOMINANT CLASS DISTRIBUTION ===",file=sys.stderr)
    for k,v in fn_class_counter.most_common():
        print(f"  {k:12s} {v}",file=sys.stderr)
    print(f"wrote {a.out}",file=sys.stderr)

if __name__=='__main__': main()
