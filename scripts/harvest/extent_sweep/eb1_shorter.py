#!/usr/bin/env python3
"""lane EB-1: the direction EA-1 never tested -- base_size < claimed_size.

EA-1 only asked "is our body LONGER than the pin" (C1: the pin is truncated).
The symmetric question is "is our body SHORTER than the pin", i.e. the pin is
OVER-LONG and has swallowed something.  Three outcomes, only one of which is a
map defect:

  PADDING     the surplus is all zero/nop words -> dtk trims it, harmless
              (this is what the 255 already-at-100 null rows should be)
  OVER_PINNED a SEPARATE function provably begins at addr+base_size -> map defect,
              splitting it out should land the row
  SHORT_SRC   the surplus is real code continuing the same body -> OUR SOURCE is
              short.  Not a map defect; real decomp work.

Third witness = retail's own .pdata unwind table, which is authoritative and
completely independent of both our compiled size and the symbols.txt pin.

The null here is REAL, unlike the longer direction: null rows can and do reach
the test, so a difference in disposition between strata is genuine evidence.
"""
import json, re, os, bisect, collections
import adj
import sweep_base

WT = adj.WT
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


def classify(addr, claim, bs):
    """disposition of the surplus [addr+bs, addr+claim)"""
    lo, hi = addr + bs, addr + claim
    ws = [adj.word(v) for v in range(lo, hi, 4)]
    if all(w is not None and adj.is_padding(w) for w in ws):
        return 'PADDING', ''
    pe2 = adj.pdata_exact(lo)
    pe1 = adj.pdata_exact(addr)
    notes = []
    if pe1:
        notes.append('pdata@sym len=0x%X%s' % (pe1[2], ' ==base' if pe1[2] == bs else ' !=base'))
    else:
        notes.append('no pdata@sym')
    if pe2:
        notes.append('PDATA RECORD AT SURPLUS START len=0x%X' % pe2[2])
        return 'OVER_PINNED', '; '.join(notes)
    # no separate record: is the surplus covered by the symbol's own record?
    cov = adj.pdata_covering(lo)
    if cov and cov[0] == addr:
        notes.append('surplus inside sym own pdata record')
        return 'SHORT_SRC', '; '.join(notes)
    return 'UNKNOWN', '; '.join(notes)


stat = {k: collections.Counter() for k in ('null', 'charged')}
detail = {k: [] for k in ('null', 'charged')}

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
    bs = BASE.get(mn)
    if bs is None or bs >= size:
        continue
    k = 'null' if mpn == 100.0 else 'charged'
    disp, notes = classify(addr, size, bs)
    stat[k][disp] += 1
    fz = f.get('fuzzy_match_percent')
    detail[k].append((addr, size, bs, mpn, (fz if fz is not None else 0.0),
                      b[2], mn, disp, notes))

print("SHORTER-DIRECTION SWEEP -- our compiled body is SHORTER than the pin")
print()
for k in ('null', 'charged'):
    tot = sum(stat[k].values())
    print(f"=== {k.upper()}  ({tot} rows) ===")
    for d, n in stat[k].most_common():
        print(f"   {d:12} {n:5}  ({100.0*n/max(tot,1):.1f}%)")
    print()

print("=== CHARGED, disposition OVER_PINNED (candidate map defects) ===")
print(f"{'addr':>10} {'claim':>6} {'base':>6} {'mpn':>7} {'fuzzy':>7}  unit :: name")
print('-' * 130)
op = [d for d in detail['charged'] if d[7] == 'OVER_PINNED']
for a, cs, bs, mpn, fz, u, mn, disp, notes in sorted(op, key=lambda r: -(r[4] or 0)):
    print(f"0x{a:08X} {cs:6X} {bs:6X} {(mpn or 0):7.2f} {fz:7.2f}  {u} :: {mn[:52]}")
print()
print("=== NULL, disposition OVER_PINNED (these are at 100 ALREADY -- do not touch) ===")
opn = [d for d in detail['null'] if d[7] == 'OVER_PINNED']
for a, cs, bs, mpn, fz, u, mn, disp, notes in opn[:15]:
    print(f"0x{a:08X} {cs:6X} {bs:6X} {(mpn or 0):7.2f} {fz:7.2f}  {u} :: {mn[:52]}")

json.dump([{'addr': '0x%08X' % a, 'claim': '0x%X' % cs, 'base': '0x%X' % bs,
            'mpn': mpn, 'fuzzy': fz, 'unit': u, 'name': mn, 'disp': disp,
            'notes': notes, 'stratum': k}
           for k in ('null', 'charged') for a, cs, bs, mpn, fz, u, mn, disp, notes in detail[k]],
          open('/home/free/tmp/laneEB1/eb1_shorter.json', 'w'), indent=1)
print("\nwrote eb1_shorter.json")
