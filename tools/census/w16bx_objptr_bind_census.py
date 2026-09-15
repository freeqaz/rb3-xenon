#!/usr/bin/env python3
"""W16-BX census: ObjPtr<T> member bound to a T* local and then nullness-tested.

Retail emits `cmpwi rN,0` (SIGNED) for a nullness test written directly on an
ObjPtr<T> member; MSVC emits `cmplwi` (UNSIGNED) when the member is first bound
to a `T*` local.  Proven in VocalTrackDir::SetRange, which contains both forms
and a bool-test null.  This finds every other site with the divergent spelling.
"""
import re, sys, json, collections
from pathlib import Path

ROOT = Path('/home/free/tmp/wt-w16-bx/src')

# ---- 1. harvest ObjPtr-typed member names from every header -----------------
objptr_members = collections.defaultdict(set)   # member name -> {files}
plain_ptr_members = collections.defaultdict(set)
decl_objptr = re.compile(r'\bObjPtr\s*<[^;{}]*?>\s*(\w+)\s*(?:;|=)')
decl_plainp = re.compile(r'\b(?!return\b)(\w[\w:]*)\s*\*\s*(m\w+|unk\w+)\s*;')
for h in ROOT.rglob('*.h'):
    try: txt = h.read_text(errors='replace')
    except Exception: continue
    for m in decl_objptr.finditer(txt):
        objptr_members[m.group(1)].add(str(h))
    for m in decl_plainp.finditer(txt):
        plain_ptr_members[m.group(2)].add(str(h))

# ---- 2. find `T *local = <member>;` then a nullness test on local -----------
bind = re.compile(r'^\s*(?:const\s+)?([\w:]+)\s*\*\s*(\w+)\s*=\s*([\w\.\->]+?)\s*;\s*$')
hits_objptr, hits_plain = [], []
for c in sorted(ROOT.rglob('*.cpp')):
    try: lines = c.read_text(errors='replace').splitlines()
    except Exception: continue
    for i, ln in enumerate(lines):
        m = bind.match(ln)
        if not m: continue
        ctype, local, rhs = m.groups()
        base = rhs.split('->')[-1].split('.')[-1]
        # a nullness test on `local` within the next 4 lines
        win = '\n'.join(lines[i+1:i+5])
        if not re.search(r'\bif\s*\(\s*!?\s*%s\s*(?:\)|&&|\|\|)' % re.escape(local), win):
            continue
        rec = dict(file=str(c.relative_to(ROOT.parent)), line=i+1, ctype=ctype,
                   local=local, rhs=rhs, member=base, src=ln.strip())
        if base in objptr_members:   hits_objptr.append(rec)
        elif base in plain_ptr_members: hits_plain.append(rec)

print("ObjPtr member names declared in headers : %d" % len(objptr_members))
print("plain T* member names declared          : %d" % len(plain_ptr_members))
print()
print("=== TREATMENT: ObjPtr member bound to T* local, then nullness-tested ===")
print("hits: %d" % len(hits_objptr))
for r in hits_objptr:
    print("  %-52s:%-5d  %s" % (r['file'], r['line'], r['src']))
print()
print("=== NULL: plain T* member bound to T* local, then tested (NO divergence expected) ===")
print("hits: %d  (these are already unsigned on BOTH sides -- cmplwi is correct)" % len(hits_plain))
for r in hits_plain[:15]:
    print("  %-52s:%-5d  %s" % (r['file'], r['line'], r['src']))
json.dump(dict(objptr=hits_objptr, plain=hits_plain), open('/home/free/tmp/w16bx_objptr_census.json','w'), indent=1)
