#!/usr/bin/env python3
import json, re, collections
from pathlib import Path
hits = json.load(open('/home/free/tmp/w16bx_objptr_census.json'))['objptr']
ROOT = Path('/home/free/tmp/wt-w16-bx')
fndef = re.compile(r'^[\w:<>,\*&\s]+?\b(\w+)::(~?\w+)\s*\(')
for h in hits:
    lines = (ROOT / h['file']).read_text(errors='replace').splitlines()
    h['encl']=None
    for j in range(h['line']-1, -1, -1):
        m = fndef.match(lines[j])
        if m and not lines[j].lstrip().startswith(('//','*')):
            h['encl']=f"{m.group(1)}::{m.group(2)}"; break

# MSVC mangled -> Class::Method
rep = json.load(open(ROOT/'build/45410914/report.json'))
rows = collections.defaultdict(list)
pat_m = re.compile(r'^\?(\w+)@(\w+)@')      # ?Meth@Class@@
pat_c = re.compile(r'^\?\?([01])(\w+)@')    # ??0Class@@ ctor / ??1 dtor
for u in rep['units']:
    for f in u.get('functions', []):
        n = f.get('name','')
        key=None
        m=pat_c.match(n)
        if m: key = f"{m.group(2)}::{'~' if m.group(1)=='1' else ''}{m.group(2)}"
        else:
            m=pat_m.match(n)
            if m: key = f"{m.group(2)}::{m.group(1)}"
        if key:
            rows[key].append((u['name'], n, int(f.get('size',0)),
                              float(f.get('fuzzy_match_percent',0) or 0)))
at100=below=norow=0; below_rows=[]
for h in hits:
    rs = rows.get(h['encl'] or '', [])
    if not rs: norow+=1; h['cls']='NO_ROW'; continue
    best = max(rs, key=lambda r: r[3])
    h['row']=best
    if best[3]>=100.0: at100+=1; h['cls']='AT_100'
    else: below+=1; h['cls']='BELOW'; below_rows.append((h,best))
print("census hits                       : %d" % len(hits))
print("  enclosing row at fuzzy == 100   : %d   <-- spelling cost NOTHING here" % at100)
print("  enclosing row below 100         : %d" % below)
print("  no pinned/paired row found      : %d" % norow)
print()
print("=== BELOW-100, largest first ===")
seen=set()
for h,b in sorted(below_rows, key=lambda x:-x[1][2]):
    if b[1] in seen: continue
    seen.add(b[1])
    print("  %-52s %7d B  fuzzy %8.4f  | %s:%d" % (b[1][:52], b[2], b[3], h['file'].split('/')[-1], h['line']))
print()
print("=== AT_100 sample (control) ===")
s=set()
for h in hits:
    if h['cls']=='AT_100' and h['row'][1] not in s:
        s.add(h['row'][1]); print("  %-52s %7d B  | %s:%d" % (h['row'][1][:52], h['row'][2], h['file'].split('/')[-1], h['line']))
    if len(s)>=12: break
json.dump(hits, open('/home/free/tmp/w16bx_census3.json','w'), indent=1)
