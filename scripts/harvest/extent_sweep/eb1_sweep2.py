#!/usr/bin/env python3
"""lane EB-1: the two-sided extent sweep, re-run with recovered base sizes.

Two things happen here:
  A) VALIDATION of the recovered sizes against EA-1's original single-function
     index, on the rows both can see.  Any disagreement is a finding about
     EA-1's index (which did NOT trim trailing padding), not just about mine.
  B) the sweep itself, over the enlarged population.

The null keeps working, but its JOB HAS CHANGED.  It cannot measure specificity
(C1 is unreachable for a matched row -- see eb1_nullcap.py).  What it CAN do now
is catch a DERIVATION BUG: if my multi-function size arithmetic over-estimates a
body, C1 goes spuriously true on rows that are already perfect.  A null fire is
therefore a self-test failure, and must be read as one.
"""
import json, re, os, bisect, sys, collections
import adj
import sweep_base
import sweep_base2

WT = adj.WT
BASE, BASE2 = sweep_base.BASE, sweep_base2.BASE2

# ---------- A) validate recovered sizes against EA-1's index ----------------
agree = dis = 0
dis_ex = []
for k, v in BASE.items():
    if v is None:
        continue
    w = BASE2.get(k)
    if w is None:
        continue
    if v == w:
        agree += 1
    else:
        dis += 1
        dis_ex.append((k, v, w))
print("=== A) recovered sizes vs EA-1's single-function index ===")
print(f"   agree {agree}   disagree {dis}")
for k, v, w in dis_ex[:10]:
    print(f"     {k[:70]:70} EA1=0x{v:X} EB1=0x{w:X}  (delta {w - v:+d})")
print(f"   sections: {dict(sweep_base2.BSTATS)}")
print(f"   symbols with a size: EA-1 {sum(1 for v in BASE.values() if v is not None)}"
      f"   EB-1 {sum(1 for v in BASE2.values() if v is not None)}")
print()

# ---------- B) the sweep ----------------------------------------------------
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


def blk(va):
    i = bisect.bisect_right(SP, va) - 1
    if i < 0:
        return None
    s, e, u = spans[i]
    return (s, e, u, i) if s <= va < e else None


ABSORB = re.compile(r'^(fn_[0-9A-Fa-f]{8}|except_data_[0-9A-Fa-f]{8})$')

stat = collections.Counter()
charged, nullfire = [], []
for addr, typ, name, size in adj.TEXT:
    if typ != 'function':
        continue
    b = blk(addr)
    if not b:
        continue
    mn = MAP.get('0x%08x' % addr)
    if not mn:
        continue
    stem = 'default/' + STEM.sub('', b[2])
    f = rows.get((stem, mn))
    if not f:
        continue
    mpn = f.get('match_percent_normalized')
    bs = BASE2.get(mn)
    if bs is None:
        continue
    k = 'null' if mpn == 100.0 else 'charged'
    stat[k + '_have_base'] += 1
    if bs <= size:
        continue
    stat[k + '_C1'] += 1
    end = addr + bs
    if not adj.raw_exit_kind(adj.word(end - 4), end - 4, addr, end):
        continue
    stat[k + '_C2'] += 1
    inner = adj.syms_in(addr + 4, end)
    if not all(ABSORB.match(s[2]) for s in inner):
        stat[k + '_C3fail'] += 1
        continue
    stat[k + '_FIRED'] += 1
    s, e, u, i = b
    nxt = spans[i + 1] if i + 1 < len(spans) else None
    cross = end > e
    same = cross and nxt and nxt[2] == u and nxt[0] == e
    disp = 'clean' if not cross else ('SAME_UNIT_MERGE' if same else 'CONTESTED')
    fz = f.get('fuzzy_match_percent')
    rec = (addr, size, bs, mpn, (fz if fz is not None else 0.0), u, mn, disp,
           len(inner), mn in BASE and BASE[mn] is not None)
    (nullfire if k == 'null' else charged).append(rec)

print("=== B) sweep over the enlarged population ===")
for kk in ('null_have_base', 'null_C1', 'null_C2', 'null_FIRED',
           'charged_have_base', 'charged_C1', 'charged_C2', 'charged_C3fail', 'charged_FIRED'):
    print(f"   {kk:20} {stat[kk]}")
print()
if nullfire:
    print("*** NULL FIRED -- this is a DERIVATION BUG self-test failure, not a candidate ***")
    for r in nullfire[:20]:
        print("   0x%08X claim=0x%X base=0x%X %s :: %s" % (r[0], r[1], r[2], r[5], r[6]))
    print()

print(f"{'addr':>10} {'claim':>6} {'base':>6} {'mpn':>7} {'fuzzy':>7} {'ab':>3} {'seen_by_EA1':>11} {'splits':>16}  unit :: name")
print('-' * 150)
for r in sorted(charged, key=lambda r: (not r[9], -(r[4] or 0))):
    a, cs, bs, mpn, fz, u, mn, disp, nab, seen = r
    print(f"0x{a:08X} {cs:6X} {bs:6X} {(mpn or 0):7.2f} {fz:7.2f} {nab:3} {str(seen):>11} {disp:>16}  {u} :: {mn[:46]}")

json.dump([{'addr': '0x%08X' % a, 'size': '0x%X' % bs, 'label': f'{u}::{mn}',
            'unit': u, 'name': mn, 'old_size': '0x%X' % cs, 'base_size': '0x%X' % bs,
            'mpn': mpn, 'fuzzy': fz, 'disp': disp, 'absorb': nab, 'seen_by_EA1': seen}
           for a, cs, bs, mpn, fz, u, mn, disp, nab, seen in charged],
          open(sys.argv[1], 'w'), indent=1)
print(f"\nwrote {sys.argv[1]}  ({len(charged)} candidates, "
      f"{sum(1 for r in charged if not r[9])} NEW to EB-1)")
