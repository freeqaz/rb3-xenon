#!/usr/bin/env python3
"""vcall_slot_census -- name-free vtable ORDER check from virtual-call sites.

Lane W16-PB (2026-10-03).

`vtable_order_sweep.py` judges slot order by MAP NAME and leaves every table
whose retail slots carry no name UNRESOLVED (748 in W16-OT's scope).  Its own
docstring names the authoritative instrument: the CALL SITE.  A virtual call
is

    lwz  rV, M(rObj)      # vfptr of the subobject at M
    lwz  rF, D(rV)        # slot D/4
    mtctr rF
    bctr / bctrl

and D (and M) are IMMEDIATES in retail's own machine code: no relocation, so
no ICF fold or map name can poison them.  A header with a virtual declared in
the wrong place (or missing, or extra) moves D for every caller of every
later slot, so it shows up here as the same (D_retail -> D_ours) shift on many
rows.

Population: rows report.json scores 0 < fuzzy < 100 whose name exists in both
the dtk target obj and our compiled obj.  Rows at fuzzy 100 already agree on
every immediate (an immediate diff is charged), so they cannot hold a slot
disagreement and are skipped.

For each row the ordered sequence of (M, D) pairs is extracted from both
bodies and aligned with difflib.  Every equal-length `replace` block yields
per-position disagreements.  Output is aggregated by (M, D_retail, D_ours) so
a systematic header shift is one line with a large count, and a one-off
source divergence (a different method called) is a line with count 1.

Every row is a CANDIDATE; the adjudication is on retail bytes (which class is
the receiver, what is retail's slot D/4).

Usage:
    python3 tools/vcall_slot_census.py [--project-dir .] [--json OUT]
        [--exclude src/network src/xdk]
    python3 tools/vcall_slot_census.py --selftest
"""
import argparse
import collections
import difflib
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

MTCTR_MASK, MTCTR = 0xFC1FFFFF, 0x7C0903A6
BCTR, BCTRL = 0x4E800420, 0x4E800421


def _lwz(w):
    if w >> 26 != 32:
        return None
    d = w & 0xFFFF
    if d & 0x8000:
        d -= 0x10000
    return (w >> 21) & 31, (w >> 16) & 31, d


def vcalls(body):
    """Ordered [(M, D)] for every lwz/lwz/mtctr/bctr[l] virtual call in `body`."""
    n = len(body) // 4
    ws = struct.unpack('>%dI' % n, body[:4 * n])
    out = []
    for i, w in enumerate(ws):
        if (w & MTCTR_MASK) != MTCTR:
            continue
        if not any(ws[j] in (BCTR, BCTRL) for j in range(i + 1, min(n, i + 3))):
            continue
        rs = (w >> 21) & 31
        d = m = None
        for j in range(i - 1, max(-1, i - 8), -1):
            l = _lwz(ws[j])
            if l and l[0] == rs:
                d, rv = l[2], l[1]
                for k in range(j - 1, max(-1, j - 10), -1):
                    l2 = _lwz(ws[k])
                    if l2 and l2[0] == rv:
                        m = l2[2]
                        # fold a preceding `addi rB, rC, k` into M: the same
                        # vfptr reached as `addi r3,r3,0x20; lwz r11,0(r3)`
                        # instead of `lwz r11,0x20(r11)` (W16-PB control row
                        # BandRetargetVignette::Poll).
                        for q in range(k - 1, max(-1, k - 6), -1):
                            w2 = ws[q]
                            if w2 >> 26 == 14 and (w2 >> 21) & 31 == l2[1]:
                                im = w2 & 0xFFFF
                                m += im - 0x10000 if im & 0x8000 else im
                                break
                            if (w2 >> 21) & 31 == l2[1] and w2 >> 26 not in (36, 44, 38):
                                break   # rB redefined by something else
                        break
                break
        if d is not None:
            out.append((m, d))
    return out


def load_rows(project_dir):
    rep = json.load(open(os.path.join(project_dir, 'build', '45410914', 'report.json')))
    units = {u['name']: u for u in json.load(open(os.path.join(project_dir, 'objdiff.json')))['units']}
    want = collections.defaultdict(list)
    for u in rep['units']:
        ou = units.get(u['name'])
        if not ou or 'base_path' not in ou or 'target_path' not in ou:
            continue
        for f in u.get('functions', []):
            fz = float(f.get('fuzzy_match_percent', 0) or 0)
            if 0 < fz < 100:
                want[u['name']].append((f['name'], fz, int(f.get('size', 0) or 0)))
    return want, units


def bodies(path):
    from coff_bodies_ext import function_bodies_ext
    out = {}
    try:
        for n, b, _rl, _v in function_bodies_ext(path):
            out.setdefault(n, b)
    except Exception:
        pass
    return out


def run(a):
    want, units = load_rows(a.project_dir)
    agg = collections.defaultdict(list)
    stats = collections.Counter()
    for un, rows in sorted(want.items()):
        ou = units[un]
        src = (ou.get('metadata') or {}).get('source_path', '') or ou['base_path']
        if any(ex.replace('src/', '') in src.replace('build/45410914/src/', '') for ex in a.exclude):
            stats['excluded_units'] += 1
            continue
        tb = bodies(os.path.join(a.project_dir, ou['target_path']))
        bb = bodies(os.path.join(a.project_dir, ou['base_path']))
        for name, fz, size in rows:
            if name not in tb or name not in bb:
                stats['rows_unpaired'] += 1
                continue
            stats['rows'] += 1
            rt, ob = vcalls(tb[name]), vcalls(bb[name])
            stats['vcalls_retail'] += len(rt)
            sm = difflib.SequenceMatcher(a=rt, b=ob, autojunk=False)
            for op, i1, i2, j1, j2 in sm.get_opcodes():
                if op == 'equal':
                    stats['vcalls_equal'] += i2 - i1
                elif op == 'replace' and i2 - i1 == j2 - j1:
                    for (rm, rd), (om, od) in zip(rt[i1:i2], ob[j1:j2]):
                        if rm == om and rd != od:
                            agg[(rm, rd, od)].append((un, name, fz, size))
                            stats['slot_disagree'] += 1
                        else:
                            stats['other_disagree'] += 1
                else:
                    stats['unaligned'] += max(i2 - i1, j2 - j1)
    rows = []
    for (m, rd, od), hits in sorted(agg.items(), key=lambda kv: -len(kv[1])):
        rows.append(dict(vfptr_off=m, retail_slot=rd // 4 if rd % 4 == 0 else rd,
                         ours_slot=od // 4 if od % 4 == 0 else od, retail_D=rd, ours_D=od,
                         n=len(hits), rows=[f'{u}:{n} ({fz:.1f})' for u, n, fz, _s in hits][:8]))
    for r in rows:
        print(f"M={r['vfptr_off']} D {r['retail_D']:#x}->{r['ours_D']:#x} "
              f"(slot {r['retail_slot']}->{r['ours_slot']}) n={r['n']}")
        for x in r['rows']:
            print('    ' + x)
    print(dict(stats))
    if a.json:
        json.dump(dict(stats=dict(stats), rows=rows), open(a.json, 'w'), indent=1)
    return rows, stats


def selftest(a):
    """The extractor must find a known call and the aligner must report a
    planted slot shift; vacuity floor on the population."""
    fails = []
    # lwz r11,0(r3); lwz r11,0x30(r11); mtctr r11; bctrl
    body = struct.pack('>4I', 0x81630000, 0x816B0030, 0x7D6903A6, 0x4E800421)
    if vcalls(body) != [(0, 0x30)]:
        fails.append(f'extractor: {vcalls(body)}')
    shifted = struct.pack('>4I', 0x81630000, 0x816B0034, 0x7D6903A6, 0x4E800421)
    sm = difflib.SequenceMatcher(a=vcalls(body), b=vcalls(shifted), autojunk=False)
    if [op for op, *_ in sm.get_opcodes()] != ['replace']:
        fails.append('aligner did not report the planted shift')
    want, _u = load_rows(a.project_dir)
    nrows = sum(len(v) for v in want.values())
    if nrows < 1000:
        fails.append(f'vacuous population: {nrows} sub-100 rows')
    print(f'selftest: sub-100 rows={nrows} -> ' + ('PASS' if not fails else 'FAIL: ' + '; '.join(fails)))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--project-dir', default=ROOT)
    ap.add_argument('--json')
    ap.add_argument('--exclude', nargs='*', default=['src/network', 'src/xdk'])
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        return selftest(a)
    run(a)
    return 0


if __name__ == '__main__':
    sys.exit(main())
