#!/usr/bin/env python3
"""Classify every `new <Class>` object factory in the TARGET objs as
  INLINED  = retail inlined the class's own operator new -- a `?StaticClassName@`
             call plus the shared 2-arg MemAlloc thunk (the OBJ_MEM_OVERLOAD
             shape, see src/system/utl/MemMgr.h), or
  OUTLINED = retail called a folded out-of-line `??2X@@SAPAXI@Z` thunk (the
             MEM_OVERLOAD shape).

This is the discriminator for the OBJ_MEM_OVERLOAD inline lever (laneAT-f4,
2026-07-27). There is no derivable property -- size, allocator, vtable, base
class -- that separates the two groups; the split is simply which allocation
macro the class was declared with in retail, and that is readable per class
straight from the bytes. Measured over the whole target: 118 INLINED-only,
156 OUTLINED-only, 0 in conflict.

★ Class identity comes from the CTOR relocation (`??0Class@@...`), NEVER from
the StaticClassName name or the factory's own symbol name: at an ICF-folded VA
`scripts/target_symbol_map.json` keeps only ONE name and it is frequently the
wrong class (e.g. `?NewObject@CharInterest@@` in ContextChecker.obj actually
constructs PatchPanel).

Read-only; touches only build/45410914/obj/**. Requires a full build first.
Usage: python3 scripts/harvest/newobj_inline_classify.py <worktree>
"""
import sys, re, json, glob, os, collections
wt = sys.argv[1]
sys.path.insert(0, wt + '/scripts/analysis')
from coffx import read_coff, infer_sizes
m = json.load(open(wt + '/scripts/target_symbol_map.json'))
def rn(x):
    mm = re.fullmatch(r'fn_([0-9A-Fa-f]{8})', x)
    return m.get('0x' + mm.group(1).lower(), x) if mm else x

CTOR = re.compile(r'^\?\?0([A-Za-z_][\w@]*?)@@')
OPNEW = re.compile(r'^\?\?2([\w@]+)@@SAPAXI@Z$')
rows = []
for p in sorted(glob.glob(wt + '/build/45410914/obj/**/*.obj', recursive=True)):
    try: d = open(p, 'rb').read()
    except OSError: continue
    secs, syms = read_coff(d)
    if secs is None: continue
    infer_sizes(secs, syms)
    byi = {s.index: s for s in syms}
    for s in syms:
        if s.sec <= 0 or s.sec - 1 >= len(secs): continue
        sec = secs[s.sec - 1]
        if not sec.is_code or not s.size or s.size > 160: continue
        rl = [rn(byi[si].name) for (va, si, t) in sec.relocs
              if s.value <= va < s.value + s.size and si in byi]
        scn = [x for x in rl if '?StaticClassName@' in x]
        opn = [x for x in rl if OPNEW.match(x)]
        alloc = 'fn_827BCD38' in rl
        if not (scn or opn): continue
        ctors = [CTOR.match(x).group(1) for x in rl if CTOR.match(x)]
        # only keep things that look like a factory: exactly one ctor call
        if len(set(ctors)) != 1: continue
        cls = ctors[0]
        if scn and alloc: kind = 'INLINED'
        elif opn and not scn: kind = 'OUTLINED'
        else: continue
        nm = s.name
        rows.append({'unit': os.path.relpath(p, wt + '/build/45410914/obj'),
                     'sym': nm, 'class': cls, 'kind': kind,
                     'size': s.size, 'thunk': opn[0] if opn else None})
byclass = collections.defaultdict(set)
for r in rows: byclass[r['class']].add(r['kind'])
inl = sorted(c for c, k in byclass.items() if k == {'INLINED'})
out = sorted(c for c, k in byclass.items() if k == {'OUTLINED'})
both = sorted(c for c, k in byclass.items() if len(k) > 1)
print('classes INLINED-only :', len(inl))
print('classes OUTLINED-only:', len(out))
print('classes BOTH (conflict):', len(both), both[:20])
json.dump({'inlined': inl, 'outlined': out, 'both': both, 'rows': rows},
          open(os.environ.get('NEWOBJ_OUT', 'newobj_classify.json'), 'w'), indent=1)
print('\nOUTLINED (need the per-class opt-out):')
for c in out: print('  ', c)
