#!/usr/bin/env python3
"""CK-1: COVERAGE-GATED gap fill.  Emits a splits.txt; measures nothing.

THE DIFFERENCE FROM CJ-1's gapfill
----------------------------------
`cj1_wave.py --mode gapfill` selects gaps by SIZE and picks the owner by "a map
row in the gap that a neighbour defines, else default to the LEFT block".  Size
is only a PROXY for attribution risk, and the left-block default is precisely
the step that manufactured the 16,156 B ProfileMgr.cpp fill CJ-1 measured at
+464 / 41% honest and correctly REFUSED TO LAND.

This tool selects by MEASURED COVERAGE (ck1_tile.py): the fraction of the gap's
`symbols.txt` function starts whose retail bytes are reloc-masked-equal to a
function DEFINED IN THAT NEIGHBOUR'S OWN COMPILED OBJ.  Independent check that
this is the right axis: the ProfileMgr gap scores 0.0% (0 of 528) -- worst in
class -- while BandTrack.cpp's 2,140 B gap scores 94.4% (17 of 18).  Coverage
and size are not the same ordering.

★ EXTEND ONLY ACROSS CLAIMED CODE.  We do not fill the whole gap: a `fwd`
extension stops at the END of the LAST CLAIMED function, a `bwd` extension
starts at the FIRST CLAIMED function.  Trailing unclaimed code stays unpinned
rather than being falsely attributed.  This is what keeps the honest fraction
high, and it is the concrete lesson of CJ-1's refused wave.

⛔ SPLITS RULES OBSERVED
  - ranges are HALF-OPEN [start,end); every edge lands on a `symbols.txt`
    function boundary, so no range can end MID-FUNCTION (dtk hard-fails that)
  - ONLY `.text` is written.  `.pdata` is DERIVED OUTPUT, re-derived from
    `.text` on every split run, and is never touched here
  - extensions GROW ONLY INTO UNPINNED SPACE -> no overlap, and Delta >= 0
    structurally (functions there were in no unit, so nothing can be lost)
  - a pure EXTENSION never drains a unit's last block, so the "delete the whole
    unit entry" hazard cannot arise
  - the Quazal /Od block is excluded upstream by ck1_tile.py
"""
import os
import re
import sys
import json
import bisect
import argparse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import truncated_pins as T  # noqa: E402
import ck1_tile as K  # noqa: E402
import ck1_gaps as G  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tile', required=True, help='ck1_tile.py --json output')
    ap.add_argument('--min-cov', type=float, default=0.20)
    ap.add_argument('--max-cov', type=float, default=1.01,
                    help='UNTREATED-POPULATION CONTROL: set --min-cov 0 '
                         '--max-cov 0.20 to build a wave from exactly the gaps '
                         'the gate REFUSED.  Measuring that wave is what proves '
                         'the gate DISCRIMINATES rather than merely correlating '
                         'with gap size.  Such a wave is measured, NEVER landed.')
    ap.add_argument('--out', required=True)
    ap.add_argument('--report', default=None)
    ap.add_argument('--symbols', default=os.path.join(
        T.ROOT, 'config', '45410914', 'symbols.txt'))
    ap.add_argument('--whole-gap', action='store_true',
                    help='CONTROL ONLY: fill the entire gap instead of stopping '
                         'at the last claimed function (this is the CJ-1 '
                         'behaviour that produced the 41%%-honest wave)')
    a = ap.parse_args()

    rows = json.load(open(a.tile))
    units = T.parse_splits()
    pins = T.Pins(units)
    retail = T.Retail()
    addrs, sizes = K.sym_funcs(a.symbols)
    K.SIZES = sizes
    pds = [x for x in addrs if T.TEXT_VA <= x < T.TEXT_END]
    objcache = {}
    uobjs = T.unit_objs(units)

    new = {u: [list(b) for b in rs] for u, rs in units.items()}
    changes = []
    skipped = []

    for r in rows:
        if not (a.min_cov <= r['coverage'] < a.max_cov):
            skipped.append((r['unit'], r['coverage'], 'outside_cov_band'))
            continue
        u, lo, hi, kind = r['unit'], r['lo'], r['hi'], r['kind']
        # recompute the CLAIMED set so we know exactly which addresses justify
        # the extension (the tile json stores only a sample of symbol names)
        srcs = uobjs.get(u, (None, []))[1]
        if not srcs:
            skipped.append((u, r['coverage'], 'no_obj'))
            continue
        if srcs[0] not in objcache:
            objcache[srcs[0]] = K.obj_index(srcs[0])
        _o, by_len = objcache[srcs[0]]
        st = K.gap_starts(pds, lo, hi)
        cl = K.claim(retail, by_len, st, lo, hi, pds)
        if not cl:
            skipped.append((u, r['coverage'], 'no_claims_on_recheck'))
            continue
        cad = sorted(cl)
        if kind == 'fwd':
            # left neighbour: its block ENDS at lo.  Extend to the end of the
            # LAST claimed function.  end is exclusive and lands on a function
            # boundary, so it cannot split a symbol.
            end = max(x + sizes[x] for x in cad) if not a.whole_gap else hi
            idx = next((i for i, (s, e) in enumerate(units[u]) if e == lo), None)
            if idx is None:
                skipped.append((u, r['coverage'], 'block_not_found'))
                continue
            if end <= new[u][idx][1]:
                skipped.append((u, r['coverage'], 'no_growth'))
                continue
            changes.append(dict(unit=u, kind=kind, frm=new[u][idx][1], to=end,
                                claimed=len(cl), starts=len(st),
                                swallowed=bisect.bisect_left(pds, end) -
                                bisect.bisect_left(pds, lo)))
            new[u][idx][1] = end
        else:
            start = min(cad) if not a.whole_gap else lo
            idx = next((i for i, (s, e) in enumerate(units[u]) if s == hi), None)
            if idx is None:
                skipped.append((u, r['coverage'], 'block_not_found'))
                continue
            if start >= new[u][idx][0]:
                skipped.append((u, r['coverage'], 'no_growth'))
                continue
            changes.append(dict(unit=u, kind=kind, frm=start, to=new[u][idx][0],
                                claimed=len(cl), starts=len(st),
                                swallowed=bisect.bisect_left(pds, hi) -
                                bisect.bisect_left(pds, start)))
            new[u][idx][0] = start

    # ---------------- mechanical verification (never assume, always check)
    flat = []
    for u, rs in new.items():
        for s, e in rs:
            if e <= s:
                print(f'!! EMPTY/INVERTED block {u} {hex(s)}-{hex(e)}')
                return 1
            flat.append((s, e, u))
    flat.sort()
    ov = [(flat[i], flat[i + 1]) for i in range(len(flat) - 1)
          if flat[i][1] > flat[i + 1][0]]
    print(f'[verify] overlaps: {len(ov)}')
    if ov:
        for x, y in ov[:5]:
            print('  !!', x, y)
        return 1
    # every new edge must be a symbols.txt function boundary
    # A splits edge is legal iff it is a function START or exactly ONE-PAST a
    # function end.  Both forms are legal in BOTH directions:
    #   fwd  -> `to` is an exclusive end, usually one-past-end
    #   bwd  -> `frm` is an inclusive start, usually a start, BUT under
    #           --whole-gap it is the PREVIOUS BLOCK'S END, i.e. one-past-end.
    # ⚠ An earlier version accepted only STARTS on the bwd side and therefore
    # REFUSED a legal --whole-gap control wave with "1 edge mid-function".  That
    # was a FALSE POSITIVE in the verifier, not a defect in the wave: a guard
    # that refuses valid input is exactly as damaging as one that passes invalid
    # input, and it is harder to notice because it looks like caution.
    bad = 0
    sset = set(pds)
    eset = {x + sizes[x] for x in pds}
    for c in changes:
        edge = c['to'] if c['kind'] == 'fwd' else c['frm']
        if edge not in sset and edge not in eset and edge != T.TEXT_END:
            bad += 1
            print(f'  !! edge {hex(edge)} ({c["kind"]}, {c["unit"]}) is neither '
                  f'a function start nor one-past-end')
    print(f'[verify] new edges landing MID-FUNCTION: {bad} '
          f'(dtk hard-fails these) -> {"OK" if bad == 0 else "REFUSE"}')
    if bad:
        return 1

    grew = sum(abs(c['to'] - c['frm']) for c in changes)
    tot_claim = sum(c['claimed'] for c in changes)
    tot_swal = sum(c['swallowed'] for c in changes)
    print(f'--- {len(changes)} extensions across '
          f'{len(set(c["unit"] for c in changes))} units, +{grew:,} pinned B')
    print(f'--- claimed(owned) {tot_claim} of {tot_swal} swallowed function '
          f'starts = PREDICTED honest fraction {100*tot_claim/max(1,tot_swal):.1f}%')
    print(f'--- skipped {len(skipped)} gaps '
          f'({sum(1 for s in skipped if s[2]=="below_min_cov")} below '
          f'min-cov {a.min_cov})')
    for c in sorted(changes, key=lambda c: -c['claimed']):
        print(f'    {c["unit"][:46]:46s} {c["kind"]} '
              f'{hex(c["frm"])}->{hex(c["to"])} claim={c["claimed"]}/'
              f'{c["swallowed"]}')

    # ---------------- write splits (.text ONLY)
    src = open(T.SPLITS).read().split('\n')
    out = []
    cur = None
    bi = 0
    for ln in src:
        if ln.strip() and not ln.startswith((' ', '\t')) and \
                ln.rstrip().endswith(':'):
            cur = ln.strip()[:-1]
            bi = 0
            out.append(ln)
            continue
        m = re.match(r'^(\s*\.text\s+start:)(0x[0-9a-fA-F]+)(\s+end:)'
                     r'(0x[0-9a-fA-F]+)(.*)$', ln)
        if m and cur in new:
            nb = new[cur]
            if bi < len(nb):
                s2, e2 = nb[bi]
                ln = f'{m.group(1)}0x{s2:08X}{m.group(3)}0x{e2:08X}{m.group(5)}'
            bi += 1
        out.append(ln)
    open(a.out, 'w').write('\n'.join(out))
    print('wrote', a.out)
    if a.report:
        json.dump(dict(changes=changes, grew=grew, claimed=tot_claim,
                       swallowed=tot_swal), open(a.report, 'w'), indent=1)
    return 0


if __name__ == '__main__':
    sys.exit(main())
