#!/usr/bin/env python3
"""lane EB-1: CONTROL OF THE CONTROL.

EA-1 advertised its predictor with "null 0/15,576 already-matched rows".  But
C1 is  base_size > claimed_size, and a row at mpn==100 has, by construction, an
instruction sequence equal to the target's -- hence equal byte size.  If that is
true then C1 is FALSE for every null row and the null could never have fired:
a gate that cannot fail is not evidence.

This measures the size relation directly on both strata, and reports how far
down the C1/C2/C3 chain each stratum actually gets.
"""
import json, re, os, bisect, glob, struct, collections
import adj

WT = adj.WT
import sweep_base  # noqa  (base_index lives here)

BASE = sweep_base.BASE
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

stat = {k: collections.Counter() for k in ('null', 'charged')}
gt_examples = {'null': [], 'charged': []}

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
    # EA-1 defect #2: fuzzy None means 0.0, not "missing".  Same for mpn.
    k = 'null' if mpn == 100.0 else 'charged'
    S = stat[k]
    S['rows'] += 1
    bs = BASE.get(mn)
    if mn not in BASE:
        S['no_base_symbol'] += 1
        continue
    if bs is None:
        S['ambiguous_comdat'] += 1
        continue
    S['have_base'] += 1
    if bs == size:
        S['size_EQUAL'] += 1
        continue
    if bs < size:
        S['base_SHORTER'] += 1
        continue
    S['C1_base_LONGER'] += 1
    end = addr + bs
    if not adj.raw_exit_kind(adj.word(end - 4), end - 4, addr, end):
        S['C2_fail_no_exit_at_end'] += 1
        continue
    S['C2_pass'] += 1
    inner = adj.syms_in(addr + 4, end)
    if not all(ABSORB.match(s[2]) for s in inner):
        S['C3_fail_named_inside'] += 1
        continue
    S['FIRED'] += 1
    gt_examples[k].append((addr, size, bs, mpn, b[2], mn))

for k in ('null', 'charged'):
    print(f"=== {k.upper()} ===")
    for kk in ('rows', 'no_base_symbol', 'ambiguous_comdat', 'have_base',
               'size_EQUAL', 'base_SHORTER', 'C1_base_LONGER',
               'C2_fail_no_exit_at_end', 'C2_pass', 'C3_fail_named_inside', 'FIRED'):
        print(f"  {kk:26} {stat[k][kk]}")
    print()

print("=== VERDICT ON THE NULL ===")
nl = stat['null']
print(f"  null rows reaching C1 (base longer than claim): {nl['C1_base_LONGER']}")
if nl['C1_base_LONGER'] == 0:
    print("  => the null NEVER REACHES the first test.  0/N is STRUCTURAL, not evidence.")
else:
    print("  => the null CAN fire; its zero is real discrimination.")

print()
print("null rows that FIRED (should be 0):")
for e in gt_examples['null'][:20]:
    print("   0x%08X claim=0x%X base=0x%X mpn=%s %s :: %s" % e)
