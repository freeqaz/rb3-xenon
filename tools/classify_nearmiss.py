#!/usr/bin/env python3
"""VOID-OUTPUT WINDOW 2026-08-16 -> 2026-08-17. The `args` spelling change
INVERTED this tool's headline verdict: NAME_RELOC fell to 0 and OFFSET filled
with 43 phantom rows per 219 mismatches, so "naming climbs it for free" was
reported as "source-fidelity struct bug" and vice versa.

Every number this tool printed between the first rebuild carrying objdiff-cli
fdc5113 ("ruler I", committed 2026-08-16 08:34:03 UTC with its release binary
deliberately NOT rebuilt; confirmed live by 21:30 that day) and the repair
described below is VOID. Re-run it; do not carry it forward. Audit:
`ARGS_READER_AUDIT.md` in decomp-bench `archive/runs/objdiff-silent-flags-and-
dead-controls-2026-08-16/` (task #96); repair task #103. Swept 2026-08-17: NO
committed artifact in this repo, and no file at any of these tools' default
output paths, falls inside that window -- this banner exists for outputs held
outside git.

Classify the mismatch CAUSE of near-miss functions via objdiff per-fn JSON.

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
import json, subprocess, sys, re, os, argparse, atexit, shutil, tempfile
from collections import Counter, defaultdict

ROOT="/home/free/code/milohax/rb3-xenon"

_SCRATCH=None
def _scratch():
    # Lazy so that merely importing this module (the tests do) creates nothing.
    global _SCRATCH
    if _SCRATCH is None:
        _SCRATCH=tempfile.mkdtemp(prefix='classify_nearmiss.')
        atexit.register(shutil.rmtree, _SCRATCH, True)
    return _SCRATCH
UNNAMED=re.compile(r'\b(fn_[0-9A-Fa-f]+|lbl_[0-9A-Fa-f]+|sub_[0-9A-Fa-f]+|loc_[0-9A-Fa-f]+)\b')

def flat_args(side):
    """Rebuild the pre-fdc5113 flat operand join from `typed_args`.

    objdiff-cli fdc5113 ("ruler I", 2026-08-16) changed the JSON `args` string
    from a comma-join of the COMPARISON arg list to the DISPLAY spelling. Three
    things moved, and this tool was tuned against all three of the old ones:

      * d-form operands gained parens:  `0x38, r4`  ->  `0x38(r4)`
      * COFF relocations gained suffixes: `sym`     ->  `sym@h` / `sym@l(r22)`
      * the trailing NON-DISPLAYED relocation vanished from the string
        (`mr r5, r6, sym` -> `mr r5, r6`); it survives only as the last
        `typed_args` entry, of type Symbol.

    tok_syms treats anything that is not a bare register or immediate as a
    SYMBOL token, so under the new spelling `0x38(r4)` read as a symbol and the
    tool's headline verdict inverted: NAME_RELOC ("naming climbs it for free")
    went to zero and OFFSET ("source-fidelity struct bug") filled with rows that
    are nothing of the kind. Rebuilding from typed_args restores the exact old
    string -- crucially INCLUDING the hidden trailing Symbol, so a row whose two
    sides are now byte-identical on `args` but still differ in the relocation is
    scored NAME_RELOC (a relocation-name difference) instead of falling through
    to the typed_args scan and being mislabelled an immediate difference.

    Rendering matches objdiff-core/src/obj/mod.rs' Display impls exactly:
    Signed/BranchDest as signed hex, Unsigned as hex, everything else verbatim.
    """
    ta = side.get('typed_args')
    if ta is None:
        return side.get('args') or ''
    out=[]
    for a in ta:
        t=a.get('type'); v=a.get('value')
        if t in ('Signed','BranchDest') and isinstance(v,int):
            out.append(('-0x%x'%-v) if v<0 else '0x%x'%v)
        elif t=='Unsigned' and isinstance(v,int):
            out.append('0x%x'%v)
        else:
            out.append(str(v))
    return ', '.join(out)


# Bare register / immediate operands, which tok_syms strips before comparing
# symbol tokens.
#
# The immediate half was `0x[0-9A-Fa-f]+|-?\d+` until 2026-08-17, which is BLIND
# TO NEGATIVE HEX. `flat_args` renders a Signed/BranchDest operand as `-0x%x`
# (see its docstring), so every negative displacement -- the ordinary spelling of
# a stack slot below the frame pointer, and of a negative struct offset -- failed
# the fullmatch, fell through to `out.add(t)`, and was compared as if it were a
# SYMBOL. Two sides differing only in a negative displacement therefore produced
# `only_t={'-0x8'}, only_b={'-0xc'}`, neither matching UNNAMED, and classify_insn
# returned WRONG_PAIR ("objdiff mis-paired the top-level symbol; needs re-pin")
# instead of reaching the typed_args scan and returning OFFSET ("struct layout /
# stack bug"). Positive hex and both signs of decimal were always covered.
#
# This defect is SPELLING-INDEPENDENT and PREDATES objdiff-cli fdc5113 -- it is
# not part of the args-spelling regression the module banner describes, and the
# fdc5113 repair (task #103) deliberately left it alone to hold an exact-parity
# restoration target. `0[xX]` also admits uppercase-prefixed hex, which no
# current producer emits but which costs nothing to accept.
_BARE_OPERAND_RE = re.compile(r'r\d+|f\d+|cr\d+|-?(?:0[xX][0-9a-fA-F]+|\d+)')


def tok_syms(args):
    # return set of symbol-like tokens (mangled names / labels), strip pure regs/imm
    # NOTE: pass flat_args(side), never side['args'] -- see flat_args' docstring.
    out=set()
    for t in re.split(r'[,\s]+', args or ''):
        t=t.strip()
        if not t: continue
        if _BARE_OPERAND_RE.fullmatch(t): continue
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
    tsy=tok_syms(flat_args(tg)); bsy=tok_syms(flat_args(bs))
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
    # Per-process scratch path. This was a FIXED absolute path
    # (`/home/free/tmp/_cls.json`) until 2026-08-17, which is a silent
    # cross-run corruption hazard: two concurrent invocations -- two lanes, or
    # one interactive run beside a batch -- alternately overwrite and read the
    # same file, so each can load the OTHER function's diff and classify it
    # under its own symbol name, with no error anywhere. Nothing in the output
    # would look wrong. Also removes a write to `~/tmp`, which is not storage.
    out=os.path.join(_scratch(), '_cls.json')
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
