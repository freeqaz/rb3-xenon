#!/usr/bin/env python3
"""lane EB-1: whole-binary per-row set-diff between two snapshots.

Reports, on BOTH rulers separately (they are computed differently and disagree):
  * rows that GAINED 100 / LOST 100 / VANISHED while at 100 / APPEARED at 100
  * unit AT_100 membership by SET-DIFF, with a mechanism label -- a unit can
    complete because a wrong row left the DENOMINATOR, with Delta-matched 0, and
    an arithmetic check cannot see that.
The whole-binary aggregate has concealed a 968-byte deletion of byte-exact retail
code before; only the per-row set-diff caught it.  So LOST is the headline, not
the total.
"""
import json, sys

A = json.load(open(sys.argv[1]))
B = json.load(open(sys.argv[2]))


def at100(s, idx):
    return {k for k, v in s['rows'].items() if v[idx] == 100.0}


def units100(s):
    return {u for u, (mf, tf) in s['units'].items() if tf and mf == tf}


print("=== AGGREGATE ===")
for k in ('matched_functions', 'matched_code', 'matched_code_percent',
          'fuzzy_match_percent', 'masked_equal_functions', 'total_code', 'total_functions'):
    a, b = A['measures'][k], B['measures'][k]
    d = b - a
    print(f"  {k:24} {a} -> {b}   {d:+}")
ha = A['measures']['matched_functions'] - A['measures']['masked_equal_functions']
hb = B['measures']['matched_functions'] - B['measures']['masked_equal_functions']
print(f"  {'honest':24} {ha} -> {hb}   {hb-ha:+}")
print()

for idx, ruler in ((0, 'match_percent_normalized (units/functions)'),
                   (1, 'fuzzy_match_percent (bytes)')):
    a100, b100 = at100(A, idx), at100(B, idx)
    gained = b100 - a100
    lost = a100 - b100
    vanished = {k for k in lost if k not in B['rows']}
    real_lost = lost - vanished
    print(f"=== ROWS, ruler = {ruler} ===")
    print(f"  at 100 before {len(a100)}  after {len(b100)}")
    print(f"  GAINED 100 : {len(gained)}")
    for k in sorted(gained):
        u, n = k.split('\x00')
        print(f"     + {A['rows'].get(k, ['-', '-', 0])[idx]:>7} -> 100.0  {u} :: {n[:70]}")
    print(f"  LOST 100 (still present)   : {len(real_lost)}")
    for k in sorted(real_lost):
        u, n = k.split('\x00')
        print(f"     ! 100.0 -> {B['rows'][k][idx]:>7}  {u} :: {n[:70]}")
    print(f"  VANISHED while at 100      : {len(vanished)}")
    for k in sorted(vanished):
        u, n = k.split('\x00')
        print(f"     x GONE (was 100, size {A['rows'][k][2]})  {u} :: {n[:70]}")
    print()

ua, ub = units100(A), units100(B)
print("=== UNITS AT 100% (set-diff) ===")
print(f"  before {len(ua)}  after {len(ub)}   delta {len(ub)-len(ua):+}")
for u in sorted(ub - ua):
    ma, ta = A['units'].get(u, (None, None))
    mb, tb = B['units'][u]
    if ta is None:
        mech = 'NEW_UNIT'
    elif tb == ta and mb > ma:
        mech = 'MATCHED_ROSE'
    elif tb < ta and mb == ma:
        mech = 'DENOMINATOR_SHRANK'
    elif tb < ta and mb != ma:
        mech = 'MIXED'
    else:
        mech = 'OTHER'
    print(f"  + {u}   {ma}/{ta} -> {mb}/{tb}   [{mech}]")
for u in sorted(ua - ub):
    ma, ta = A['units'][u]
    mb, tb = B['units'].get(u, (None, None))
    print(f"  - {u}   {ma}/{ta} -> {mb}/{tb}   *** UNIT FELL OFF 100% ***")
print()

# rows that moved at all, in the units we touched or anywhere with a big swing
print("=== LARGEST NON-100 MOVES (|delta fuzzy| >= 1.0) ===")
moved = []
for k in set(A['rows']) & set(B['rows']):
    d = B['rows'][k][1] - A['rows'][k][1]
    if abs(d) >= 1.0:
        moved.append((d, k))
for d, k in sorted(moved, key=lambda x: -abs(x[0]))[:25]:
    u, n = k.split('\x00')
    print(f"  {d:+8.2f}  {A['rows'][k][1]:7.2f} -> {B['rows'][k][1]:7.2f}  {u} :: {n[:66]}")
print(f"  ({len(moved)} rows moved >= 1.0 fuzzy)")
