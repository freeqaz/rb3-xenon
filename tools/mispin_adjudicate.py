#!/usr/bin/env python3
"""Adjudicate MIS-PIN SUSPECT units on retail bytes -- RARITY-WEIGHTED.

⛔ WHY THE NAIVE VERSION IS VACUOUS.  Two channels look like evidence and are
not:
  * `?OnHit@KeyboardTrackWatcherImpl@@...` is called by **644 functions**
    binary-wide.  It is an ICF fold survivor wearing whichever spelling the
    linker's coin-flip kept; reading it as "this block is track-watcher code"
    is reading the linker's arbitrary choice as a fact about the TU.
  * `ui/startup/eng/startup_autosave_esrb_keep.milo` appears in **1,740**
    functions, `Assertion failed: %s (%s:%u)` in 456, `timer_script` in 406.
    These are common-pool strings, not per-function references.

Both are shaped like decisive TU evidence and would have produced confident
mis-pin verdicts on three units.  So: only strings appearing in FEWER THAN
`--max-freq` functions, and only callees whose target address has fan-in below
`--max-fanin`, are counted.  Everything else is printed as suppressed noise so
the suppression is auditable rather than silent.

The CONTROL is the unit's own TU-owned named rows put through the identical
filter: if the flagged block's rare evidence names the same subsystem as the
control, the PIN is fine and our SOURCE diverges; if it names another
subsystem, the PIN is carrying a foreign TU.
"""
import argparse
import collections
import json
import sys
from pathlib import Path

WT = Path(__import__('os').environ.get('MISPIN_WT',
                     str(Path(__file__).resolve().parent.parent)))
sys.path.insert(0, str(WT / 'tools'))

ap = argparse.ArgumentParser()
ap.add_argument('--max-freq', type=int, default=20)
ap.add_argument('--max-fanin', type=int, default=20)
ap.add_argument('--only')
ap.add_argument('--npick', type=int, default=6)
A = ap.parse_args()

report = json.loads((WT / 'build/45410914/report.json').read_text())
objdiff = json.loads((WT / 'objdiff.json').read_text())
unit_cfg = {u['name']: u for u in objdiff['units']}
smap = json.loads((WT / 'scripts/target_symbol_map.json').read_text())
fp = json.loads((WT / 'fingerprints.json').read_text())
# Global frequency tables -- the rarity weighting IS the instrument (see
# docstring).  Derived from fingerprints.json on every run so they can never
# go stale against it.
sfreq, cfreq = collections.Counter(), collections.Counter()
for _v in fp.values():
    for _s in (_v.get('strings') or []):
        sfreq[_s] += 1
    for _c in (_v.get('callees') or []):
        cfreq[str(_c).lower()] += 1

addr_name = {int(k, 16): v for k, v in smap.items()
             if isinstance(v, str) and k.startswith('0x')}
name_addr = {}
for a, n in addr_name.items():
    name_addr.setdefault(n, a)

from ident_body_channel import build_supply
_supply, name_units, _sz = build_supply(WT, unit_cfg, verbose=False)


def tu_owned(n):
    v = name_units.get(n)
    if v is None or len(v) != 1:
        return False
    return '@stlpmtx_std@@' not in n and '?$' not in n


def ev(addr):
    """Rare strings and rare TU-owned callees for a retail address."""
    f = fp.get('%08X' % addr) or fp.get('%08x' % addr) or {}
    rs, ns = [], []
    for s in (f.get('strings') or []):
        (rs if sfreq.get(s, 0) < A.max_freq else ns).append(s)
    rc, nc = [], []
    for c in (f.get('callees') or []):
        key = str(c).lower()
        ca = int(c, 16) if isinstance(c, str) else c
        nm = addr_name.get(ca)
        fan = cfreq.get(key, 0)
        if nm and fan < A.max_fanin and tu_owned(nm):
            rc.append((nm, fan))
        elif nm:
            nc.append((nm, fan))
    return rs, ns, rc, nc


rows_by_unit = collections.defaultdict(list)
named_by_unit = collections.defaultdict(list)
for u in report['units']:
    for f in u.get('functions') or []:
        n = f['name']
        sz = int(f.get('size', 0))
        if n.startswith('fn_'):
            rows_by_unit[u['name']].append((int(n[3:], 16), sz, n))
        elif n in name_addr and tu_owned(n):
            named_by_unit[u['name']].append((name_addr[n], n))


def votes_for(rows):
    v = collections.Counter()
    for a, sz, _n in rows:
        _rs, _ns, rc, _nc = ev(a)
        for nm, _f in rc:
            for ou in (name_units.get(nm) or []):
                v[ou] += 1
    return v


def dump(unit, lo=None, hi=None):
    print('=' * 76)
    cfg = unit_cfg.get(unit, {})
    print('%s\n  src: %s' % (unit, (cfg.get('metadata') or {}).get('source_path')))
    named = sorted(named_by_unit.get(unit, []))
    if named:
        print('  hull 0x%08X-0x%08X (%d TU-owned named rows)'
              % (named[0][0], named[-1][0], len(named)))
    print('  -- CONTROL: rare evidence on the unit\'s OWN named rows --')
    shown = 0
    for a, n in named:
        rs, _ns, rc, _nc = ev(a)
        if (rs or rc) and shown < 4:
            shown += 1
            print('     %-46s' % n[:46])
            if rs:
                print('        str %s' % (rs[:4],))
            if rc:
                print('        cal %s' % ([x for x, _ in rc][:3],))
    if not shown:
        print('     (no rare evidence on any named row)')
    cv = votes_for([(a, 0, n) for a, n in named])
    print('     CONTROL callee-owner votes: %s' % (cv.most_common(4),))

    rows = sorted(rows_by_unit.get(unit, []))
    if lo is not None:
        rows = [r for r in rows if lo <= r[0] < hi]
    print('  -- FLAGGED BLOCK 0x%08X-0x%08X: %d rows / %d B --'
          % (lo or 0, hi or 0, len(rows), sum(r[1] for r in rows)))
    for a, sz, n in sorted(rows, key=lambda r: -r[1])[:A.npick]:
        rs, ns, rc, nc = ev(a)
        print('     0x%08X %6d B' % (a, sz))
        if rs:
            print('        str  %s' % (rs[:5],))
        if rc:
            print('        cal  %s' % ([f'{x}(fan{f})' for x, f in rc][:4],))
        if not rs and not rc:
            sup = [f'{x}({sfreq.get(x,0)})' for x in ns[:2]] + \
                  [f'{x[0][:28]}(fan{x[1]})' for x in nc[:2]]
            print('        (no rare evidence; suppressed: %s)' % (sup,))
    bv = votes_for(rows)
    print('     BLOCK callee-owner votes: %s' % (bv.most_common(5),))
    print()


FLAGGED = [
    ('default/RockCentral', 0x82509530, 0x8250A0C8),
    ('default/VocalTrackDir', 0x822ECC48, 0x822EE484),
    ('default/AccomplishmentSongConditional', 0x825EB0D0, 0x825EBD50),
    ('default/Joypad', 0x82526A00, 0x825273A4),
    ('default/system/rnddx9/Rnd', 0x82731E68, 0x82732A94),
    ('default/band3/meta_band/RetryAudioPanel', 0x82631130, 0x826319D4),
    ('default/Debug', 0x82510BB8, 0x82511428),
    ('default/Font', 0x82475A20, 0x824764A8),
    ('default/DuplicatedObject', 0x00000000, 0xFFFFFFFF),
]

for u, lo, hi in FLAGGED:
    if A.only and A.only not in u:
        continue
    dump(u, lo, hi)
