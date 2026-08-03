#!/usr/bin/env python3
"""lane EB-1: how big is the truncation class REALLY?

The sweep is gated on C1 (our compiled body is LONGER than the pin), so it can
only see a truncation we happen to out-compile.  A pin that is truncated AND
whose body we also compile short is invisible to it.  So size the class using
only the two witnesses that actually decide it, independent of our build:

  W1  retail .pdata has no exact record at the symbol  (where .pdata speaks, the
      pin is right -- 26/26 landings and 11/21 candidate refutations)
  W2  the claimed extent's last instruction is not a terminator -> control falls
      off the end -> provably truncated

Then split by whether the sweep could reach it, so the next lane knows what is
left and why.
"""
import json, re, os, bisect, collections
import adj
import sweep_base2

WT = adj.WT
BASE2 = sweep_base2.BASE2
MAP = json.load(open(os.path.join(WT, 'scripts/target_symbol_map.json')))
REP = json.load(open(os.path.join(WT, 'build/45410914/report.json')))
rows = {}
for u in REP['units']:
    for f in (u.get('functions') or []):
        rows[(u['name'], f['name'])] = f

spans, cur = [], None
for line in open(os.path.join(WT, 'config/45410914/splits.txt')):
    if line.strip() and not line[0].isspace() and line.rstrip().endswith(':'):
        cur = line.strip().rstrip(':')
    else:
        m = re.search(r'\.text\s+start:0x([0-9A-Fa-f]+) end:0x([0-9A-Fa-f]+)', line)
        if m:
            spans.append((int(m.group(1), 16), int(m.group(2), 16), cur))
spans.sort()
SP = [s[0] for s in spans]
STEM = re.compile(r'\.(cpp|c|s)')

st = collections.Counter()
buckets = collections.defaultdict(list)

for addr, typ, name, size in adj.TEXT:
    if typ != 'function':
        continue
    i = bisect.bisect_right(SP, addr) - 1
    if i < 0 or not (spans[i][0] <= addr < spans[i][1]):
        continue
    unit = spans[i][2]
    mn = MAP.get('0x%08x' % addr)
    f = rows.get(('default/' + STEM.sub('', unit), mn)) if mn else None
    mpn = (f.get('match_percent_normalized') if f else None)
    named = mn is not None and f is not None
    st['pinned_fns'] += 1

    if adj.pdata_exact(addr):          # W1 fails
        continue
    st['no_pdata'] += 1
    eff = size
    while eff >= 8 and adj.word(addr + eff - 4) == 0:
        eff -= 4
    if adj.raw_exit_kind(adj.word(addr + eff - 4), addr + eff - 4, addr, addr + eff):
        continue                        # W2 fails: extent is self-consistent
    st['TRUNCATED'] += 1

    if not named:
        st['  unnamed/unpaired (no report row)'] += 1
        k = 'ANON'
    elif mpn == 100.0:
        st['  ** AT 100 DESPITE BEING TRUNCATED **'] += 1
        k = 'AT100'
    else:
        st['  named, charged'] += 1
        bs = BASE2.get(mn)
        if bs is None:
            st['     no compiled size -> sweep BLIND'] += 1
            k = 'NOSIZE'
        elif bs <= size:
            st['     our body is NOT longer -> sweep BLIND'] += 1
            k = 'SHORTBODY'
        else:
            end = addr + bs
            if not adj.raw_exit_kind(adj.word(end - 4), end - 4, addr, end):
                st['     our end is not an exit -> sweep rejects (C2)'] += 1
                k = 'C2FAIL'
            else:
                st['     SWEEP-REACHABLE'] += 1
                k = 'REACHABLE'
        buckets[k].append((addr, size, mpn, unit, mn))
        continue
    buckets[k].append((addr, size, mpn, unit, mn))

for k, v in st.items():
    print(f"  {k:48} {v}")
print()
print("=== named+charged TRUNCATED rows the sweep CANNOT see, by why ===")
for k in ('NOSIZE', 'SHORTBODY', 'C2FAIL'):
    b = buckets[k]
    tot = sum(x[1] for x in b)
    print(f"  {k:10} {len(b):5} rows, {tot:8} pinned bytes")
print()
print("=== top 12 by claimed size, sweep-blind (NOSIZE+SHORTBODY) ===")
cand = buckets['NOSIZE'] + buckets['SHORTBODY']
for a, s, mpn, u, mn in sorted(cand, key=lambda r: -r[1])[:12]:
    print(f"  0x{a:08X} claim=0x{s:<5X} mpn={(mpn or 0):6.2f}  {u} :: {mn[:60]}")
