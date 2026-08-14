#!/usr/bin/env python3
"""Census the RULER GAP: rows that read `fuzzy == 100` under `functionRelocDiffs=none`
but score below 100 on the SHIPPED graded (`name_check`) ruler.

WHY THIS POPULATION EXISTS
--------------------------
Lane MCPRULER-1 (2026-08-14) found `mcp_server.py` hardcoded `-c
functionRelocDiffs=none`, and objdiff-cli applies `-c` LAST, so it was ACTIVELY
OVERRIDING the shipped `name_check`. Every `run_objdiff` reading taken before
that fix was on the wrong ruler, and rows dismissed as "Complete -- No action
needed" could be withholding all of their bytes from `matched_code`.

★ THE CENTRAL STRUCTURAL RESULT (lane RULERGAP-1): THIS POPULATION CONTAINS
  ZERO INSTRUCTION DEFECTS, BY CONSTRUCTION.
`functionRelocDiffs` changes ONLY relocation-argument comparison. So a row at
`fuzzy == 100` under `none` already has every mnemonic and every non-relocation
argument equal; every charge it takes on the graded ruler is a relocation-NAME
charge. Verified live: CharBone::SyncProperty reads "205 instructions | all
equal" while scoring 98.4% graded.

⇒ It is therefore a strict subset of lane MPNGAP-1's `mpn == 100 & fuzzy < 100`
stratum (measured here: |B \\ A| == 0), because "all penalties are relocation
args" implies "all penalties are arg-only", which is what `mpn` excludes. The
211-row remainder of A is DC-4's register/branch-dest class.
*** DO NOT RE-FUND THIS AS A NEW VEIN. It is MPNGAP-1's stratum reached from
the other side, and MPNGAP-1's census already answers it. ***

⛔ THE TRAP THIS TOOL EXISTS TO PREVENT
---------------------------------------
MPNGAP-1 established TRANSPOSITION (a crossed (T,B) pair) as the one signature
ICF folding cannot produce, and licensed its two landings on the rule "both
callees are themselves fuzzy==100 rows, so their map names are VALIDATED BY BODY
MATCH".

*** THAT VALIDATION IS VACUOUS WHENEVER THE TWO TRANSPOSED CALLEES HAVE
    IDENTICAL BODIES -- WHICH IS EXACTLY WHEN A TRANSPOSITION CAN BE MANUFACTURED
    BY AN ARBITRARY MAP ASSIGNMENT. ***
An identical body matches at EITHER address, so body match cannot say which name
belongs to which; the map's choice between them is a coin flip. Editing source to
"fix" such a row lifts the metric BY CONSTRUCTION while resting on nothing --
the "never fix source to satisfy a fold" hazard.

Measured on the 7 payable transposition rows at 7286bfd1: 3 of them (1,036 B --
RndDir::SyncProperty, SynapseAPO::OnSetParameters, Sfx::Copy) have callee pairs
IDENTICAL in both size and fuzzy, i.e. UNADJUDICABLE. Only the 4 Crowd rows
(656 B, one Char3D ctor-vs-assign root cause) are distinguishable (84 B @ 99.76
vs 84 B @ 15.33) -- and MPNGAP-1 deferred those as shared-header risk.

`--transposition` reports the split and REFUSES to call an indistinguishable
pair payable.
"""
from __future__ import annotations
import argparse, collections, json, re, sys
from pathlib import Path

ANON = re.compile(r'^(fn_|lbl_|jumptable_|data_|bss_|rdata_)')


def load_rows(path):
    """report.json is protobuf-JSON (defaults OMITTED) and several numerics are
    JSON *strings* -- coerce every one, never index bare."""
    d = json.load(open(path))
    rows, byname = {}, collections.defaultdict(list)
    for u in d['units']:
        for f in u.get('functions', []):
            n = f.get('name')
            if not n:
                continue
            fz = float(f.get('fuzzy_match_percent', 0) or 0)
            sz = int(f.get('size', 0) or 0)
            rows[(u['name'], n)] = (fz,
                                    float(f.get('match_percent_normalized', 0) or 0),
                                    sz,
                                    bool(f.get('masked_equal', False)))
            byname[n].append((sz, fz))
    return rows, byname, d['measures']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--graded', required=True, help='report.json on the SHIPPED ruler')
    ap.add_argument('--none', required=True, help='report.json regenerated at -c functionRelocDiffs=none')
    ap.add_argument('--pairs', default='', help='WS-4 pairs_folded2.json (adds FOLD/GENUINE pricing)')
    ap.add_argument('--transposition', action='store_true')
    a = ap.parse_args()

    G, gname, gm = load_rows(a.graded)
    N, _, nm = load_rows(a.none)
    if set(G) != set(N):
        sys.exit('REFUSING: the two reports do not cover the same key space')

    inverted = [k for k in G if G[k][0] > N[k][0] + 1e-9]
    if inverted:
        sys.exit('REFUSING: %d rows score HIGHER graded than at `none`; the '
                 'legs are not the same tree/ruler pair' % len(inverted))

    B = [k for k in N if N[k][0] == 100.0 and G[k][0] < 100.0]
    A = [k for k in G if G[k][1] == 100.0 and G[k][0] < 100.0]
    tot = sum(G[k][2] for k in B)
    print('ruler gap: matched_code %d -> %d B  (%.6f -> %.6f%%)' % (
        int(nm['matched_code']), int(gm['matched_code']),
        float(nm['matched_code_percent']), float(gm['matched_code_percent'])))
    print('[B] fuzzy==100 at `none`, <100 graded : %5d rows / %8d B' % (len(B), tot))
    print('[A] MPNGAP-1 mpn==100 & fuzzy<100     : %5d rows / %8d B' % (
        len(A), sum(G[k][2] for k in A)))
    print('    B subset of A: %s   (|B\\A| = %d)  <- expected True/0 BY CONSTRUCTION'
          % (set(B) <= set(A), len(set(B) - set(A))))

    anon = [k for k in B if ANON.match(k[1])]
    named = [k for k in B if not ANON.match(k[1])]
    print('    anonymous (all masked_equal funclet pairings, UNADDRESSABLE): '
          '%d rows / %d B' % (len(anon), sum(G[k][2] for k in anon)))
    print('    named                                                      : '
          '%d rows / %d B' % (len(named), sum(G[k][2] for k in named)))

    if not a.pairs:
        return
    pairs = json.load(open(a.pairs))
    bysym = collections.defaultdict(list)
    for r in pairs:
        bysym[(r['unit'], r['sym'])].append(r)

    def kind(r):
        f = r.get('fold', '')
        return 'FOLD' if f.startswith('FOLD') else ('GENUINE' if f.startswith('GENUINE') else 'OTHER')

    # ALL-OR-NOTHING: matched_code is per-row, so ONE unclosable site withholds
    # the whole row. Never sum sites; sum rows whose EVERY site is closable.
    buckets, bbytes = collections.Counter(), collections.Counter()
    for k in named:
        if k not in bysym:
            buckets['UNJOINED'] += 1; bbytes['UNJOINED'] += G[k][2]; continue
        cs = {kind(r) for r in bysym[k]}
        b = ('ALL_FOLD' if cs == {'FOLD'} else 'ALL_GENUINE' if cs == {'GENUINE'}
             else 'ALL_OTHER' if cs == {'OTHER'} else 'MIXED')
        buckets[b] += 1; bbytes[b] += G[k][2]
    print('\nall-or-nothing pricing of the named surface:')
    for b, c in bbytes.most_common():
        print('    %-12s %5d rows / %8d B  (%.1f%% of named)'
              % (b, buckets[b], c, 100.0 * c / max(1, sum(G[k][2] for k in named))))
    print('  ALL_GENUINE is an UPPER BOUND on defects, not a defect count: a body '
          'difference\n  at a charged site is EITHER our source calling the wrong '
          'callee OR the map\n  mis-attributing the name at that VA (WS-4).')

    if not a.transposition:
        return
    print('\ntransposition rows (a crossed pair cannot be produced by a fold):')
    ok = bad = 0; okb = badb = 0
    for k in sorted(named, key=lambda k: -G[k][2]):
        sites = bysym.get(k, [])
        ps = {(r['target_symbol'], r['base_symbol']) for r in sites}
        crossed = {p for p in ps if (p[1], p[0]) in ps}
        if not crossed or not all((r['target_symbol'], r['base_symbol']) in crossed
                                  for r in sites):
            continue
        seen, dist = set(), True
        for T, Bn in crossed:
            if (Bn, T) in seen:
                continue
            seen.add((T, Bn))
            if gname.get(T, [(None, None)])[0] == gname.get(Bn, [(None, None)])[0]:
                dist = False
        if dist:
            ok += 1; okb += G[k][2]
        else:
            bad += 1; badb += G[k][2]
        print('    %6d B  %-8s %s' % (G[k][2],
              'ADJUDICABLE' if dist else 'REFUSED', k[1][:64]))
    print('    => adjudicable %d rows / %d B ; REFUSED (identical callee bodies, '
          'map assignment is a coin flip) %d rows / %d B' % (ok, okb, bad, badb))


if __name__ == '__main__':
    main()
