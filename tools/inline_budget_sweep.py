#!/usr/bin/env python3
"""Detect MSVC INLINE-BUDGET truncation: retail inlines a small helper at all N
call sites, we inline the first M and then emit `bl helper` for the remaining K.

Lane W6-A (docs/decomp/DATAINITFUNCS_2026-09-11.md) found this on
`?DataInitFuncs@@` (8,068 B, fuzzy 71.4467 -> 100.0).  The diagnosis there was
ARITHMETIC, not impressionistic, and that arithmetic is what this tool
mechanises.

THE MODEL
---------
Let  R = retail body length in instructions (target_size / 4)
     s = instructions the helper occupies when inlined
     M = sites MSVC did inline      P = M * s   (the aligned stretch)
     K = sites it gave up on        (each now a `bl helper` in OUR body)
     N = M + K = total call sites

Then          R ~= overhead + N*s = overhead + P + K*s
so            s ~= (R - P) / K            ... (A)  derived from the tail
and           P  = M * s                  ... (B)  derived from the head

(A) and (B) are INDEPENDENT measurements of s.  The detector fires only when
they agree: the value of s implied by the un-inlined tail must also divide the
aligned head.  On W6-A: R=2017, P=898, K=85 -> s=13.16->13, and 898 = 69*13 + 1.

DEFINITIONS THAT WERE MEASURED, NOT GUESSED (all three were WRONG on the first
pass and were corrected against the live known-positive control):

  * P is the longest contiguous run of rows that are NEITHER `insert` NOR
    `delete`.  It is NOT the leading run of `equal` rows: W6-A's row has a
    leading equal-run of 2 and a longest equal-run of 13, because our prologue
    differs (frame -0x100 vs retail -0x190) and 495 rows inside the aligned
    stretch are `diff_arg`.  Only insert/delete shift the 1:1 correspondence.
  * The helper occurrences must NOT sit on `equal` rows.  An `equal` row means
    retail makes the same call there (possibly through an ICF fold alias), so
    it is not an un-inlined site at all.  This is the ONLY thing separating the
    disease from the TEMPLATE_ARGS_DIFFER family -- see the FP note below.
  * `P mod s == 0` is too brittle: the run boundary moves by a row or two with
    the prologue, so we require |P - M*s| <= 2.

MEASURED FALSE-POSITIVE MECHANISM (the reason for the `equal`-row rule):
`?SetName@CharIKFingers@@` calls `SetObjConcrete<RndTransformable>` 46 times
where retail calls `SetObjConcrete<BandCharacter>` -- the same helper, a
different template instantiation.  Comparing exact mangled names reads that as
"retail never calls this helper" and the row count is large enough to satisfy
the arithmetic by chance.  Every one of those 46 rows is `equal`.

USAGE
  python3 tools/inline_budget_sweep.py --selftest
  python3 tools/inline_budget_sweep.py --jsonl <objdiff batch jsonl> [--all]
where the jsonl comes from
  bin/objdiff-cli diff -p . --batch --include-instructions -f json \
      -o out.jsonl < symbols.txt
"""
import argparse, collections, json, sys

MIN_SIZE = 400          # bytes; a short row cannot show a long aligned stretch
MIN_P = 16              # instructions in the aligned stretch
MAX_P_FRAC = 0.95       # ... but it must not BE the whole body (else no disease)
MIN_INSDEL_FRAC = 0.05  # alignment must genuinely be lost after the stretch
MIN_K = 2               # a repeated helper call, not a one-off
MIN_M = 3               # and several sites really were inlined
S_TOL = 0.35            # slop in s from prologue/epilogue overhead
P_SLOP = 2              # slop in P from the run boundary


def aligned_prefix(rows):
    """Longest contiguous run of rows that keep the 1:1 correspondence."""
    best = cur = start = best_start = 0
    for i, r in enumerate(rows):
        if r['match_type'] not in ('insert', 'delete'):
            if cur == 0:
                start = i
            cur += 1
            if cur > best:
                best, best_start = cur, start
        else:
            cur = 0
    return best, best_start


def helper_candidates(rows):
    """base-side `bl` callees that retail's body never calls by that name."""
    base = collections.defaultdict(list)
    tgt = collections.Counter()
    for r in rows:
        for side in ('base', 'target'):
            s = r.get(side)
            if not s or s['opcode'] != 'bl':
                continue
            for ta in s.get('typed_args', []):
                if ta['type'] in ('Reloc', 'Symbol', 'BranchDest'):
                    if side == 'base':
                        base[str(ta['value'])].append((r['match_type'], 0))
                    else:
                        tgt[str(ta['value'])] += 1
    out = []
    for h, occ in base.items():
        if tgt.get(h, 0):
            continue
        kinds = collections.Counter(m for m, _ in occ)
        out.append((h, len(occ), kinds))
    out.sort(key=lambda x: -x[1])
    return out


def analyse(d):
    rows = d['instructions']
    R = d['target_size'] // 4
    P, P_at = aligned_prefix(rows)
    summ = d['instruction_summary']
    insdel = summ.get('insert', 0) + summ.get('delete', 0)
    return dict(sym=d['symbol'], unit=d['unit'], size=d['target_size'],
                fuzzy=d['fuzzy_match_percent'], R=R, P=P, P_at=P_at,
                insdel=insdel, helpers=helper_candidates(rows), summ=summ)


def verdict(a):
    """Return (hit_or_None, reason). Staged so the reject reason is reportable."""
    R, P = a['R'], a['P']
    if a['size'] < MIN_SIZE:
        return None, 'C1_too_short'
    if R == 0:
        return None, 'C1_empty'
    if P < MIN_P:
        return None, 'C2_no_aligned_stretch'
    if P / R > MAX_P_FRAC:
        return None, 'C2_aligned_throughout'
    if a['insdel'] / R < MIN_INSDEL_FRAC:
        return None, 'C3_not_misaligned'
    for h, K, kinds in a['helpers']:
        if K < MIN_K:
            continue
        if kinds.get('equal', 0):
            continue                       # retail calls it here -> fold/template, not un-inlined
        raw = (R - P) / K
        for s in {int(raw), round(raw), int(raw) + 1}:
            if s < 2 or abs(raw - s) > S_TOL:
                continue
            M = round(P / s)
            if M < MIN_M or abs(P - M * s) > P_SLOP:
                continue
            return dict(H=h, K=K, s=s, s_raw=round(raw, 4), M=M, N=M + K,
                        kinds=dict(kinds)), 'FIRE'
    return None, 'C4C5_no_helper_or_arith_disagrees'


# --------------------------------------------------------------------------
# Self-test.  A detector with no failing control is a heuristic that confirms
# whatever you point it at, so each fixture below asserts a DIFFERENT clause and
# the suite fails loudly if any clause silently stops discriminating.
def _fix(size, pattern, helper='?H@@YAXXZ', hkind='diff_arg', hcount=0,
         tgt_extra=None, hsep=False):
    """pattern: list of (match_type, count).  helper calls appended as hkind."""
    rows = []
    for mt, n in pattern:
        for _ in range(n):
            rows.append({'match_type': mt, 'target': {'opcode': 'nop', 'args': '', 'typed_args': []},
                         'base': {'opcode': 'nop', 'args': '', 'typed_args': []}})
    for _ in range(hcount):
        if hsep:   # keep the helper rows from themselves forming a long aligned run
            rows.append({'match_type': 'delete',
                         'target': {'opcode': 'nop', 'args': '', 'typed_args': []},
                         'base': None})
        rows.append({'match_type': hkind,
                     'target': {'opcode': 'nop', 'args': '', 'typed_args': []} if hkind != 'insert' else None,
                     'base': {'opcode': 'bl', 'args': helper,
                              'typed_args': [{'type': 'Reloc', 'value': helper}]}})
    for _ in range(tgt_extra or 0):
        rows.append({'match_type': 'equal',
                     'target': {'opcode': 'bl', 'args': helper,
                                'typed_args': [{'type': 'Reloc', 'value': helper}]},
                     'base': {'opcode': 'bl', 'args': helper,
                              'typed_args': [{'type': 'Reloc', 'value': helper}]}})
    summ = collections.Counter(r['match_type'] for r in rows)
    return {'symbol': 'FIXTURE', 'unit': 'u', 'target_size': size,
            'fuzzy_match_percent': 71.0, 'instructions': rows,
            'instruction_summary': dict(summ)}


def selftest():
    cases = []
    # 1. POSITIVE: W6-A's own geometry. R=2017, P=898 aligned, K=85, s=13.
    pos = _fix(8068, [('diff_arg', 898), ('delete', 400)], hcount=85)
    cases.append(('W6A_geometry_fires', pos, True))
    # 2. NEGATIVE: the same row with the helper calls on `equal` rows
    #    (= the CharIKFingers template-instantiation FP).
    neg_eq = _fix(8068, [('diff_arg', 898), ('delete', 400)], hcount=85, hkind='equal')
    cases.append(('equal_rows_do_not_fire', neg_eq, False))
    # 3. NEGATIVE: retail calls the helper too -> not un-inlined.
    neg_tgt = _fix(8068, [('diff_arg', 898), ('delete', 400)], hcount=85, tgt_extra=3)
    cases.append(('retail_also_calls_does_not_fire', neg_tgt, False))
    # 4. NEGATIVE: no aligned stretch (immediate divergence, the SHA1 shape).
    neg_p = _fix(8068, [('delete', 1), ('diff_arg', 3)] * 300, hcount=85)
    cases.append(('no_aligned_stretch_does_not_fire', neg_p, False))
    # 5. NEGATIVE: aligned throughout (an arg-charge-only row, fuzzy 99.9x).
    neg_al = _fix(8068, [('diff_arg', 2000)], hcount=85)
    cases.append(('aligned_throughout_does_not_fire', neg_al, False))
    # 6. NEGATIVE: arithmetic disagrees -- same P and K but a body length that
    #    makes (R-P)/K non-integral and inconsistent with P.  THIS is the clause
    #    that makes the detector more than "long row with repeated calls".
    neg_ar = _fix(8068, [('diff_arg', 700), ('delete', 400)], hcount=85)
    cases.append(('arithmetic_disagreement_does_not_fire', neg_ar, False))
    # 7. NEGATIVE: only one un-inlined call -> not a budget truncation.
    neg_k = _fix(8068, [('diff_arg', 898), ('delete', 400)], hcount=1)
    cases.append(('single_call_does_not_fire', neg_k, False))

    # 8. NEGATIVE: exactly ONE un-inlined call. Arithmetic is made consistent on
    #    purpose (R=1197, P=898, K=1 -> s=299, M=3) so ONLY the K>=2 clause can
    #    reject it: a single out-of-line call is an ordinary call, not a budget
    #    truncation.
    neg_k1 = _fix(4788, [('diff_arg', 898), ('delete', 200)], hcount=1)
    cases.append(('K_of_one_does_not_fire', neg_k1, False))
    # 9. NEGATIVE: only TWO sites were inlined (M=2). Arithmetic is consistent
    #    (R=1131, P=26, K=85 -> s=13, M=2) so ONLY the M>=3 clause rejects it.
    #    With M<3 the "head" measurement of s rests on too little evidence.
    neg_m2 = _fix(4524, [('diff_arg', 26), ('delete', 200)], hcount=85, hsep=True)
    cases.append(('M_of_two_does_not_fire', neg_m2, False))

    # 10. NEGATIVE: aligned stretch too SHORT to measure s from (P=10). The tail
    #     arithmetic is made consistent (R=265, K=85 -> s=3, M=3) so only the
    #     MIN_P clause can reject.
    neg_p10 = _fix(1060, [('diff_arg', 10), ('delete', 30)], hcount=85, hsep=True)
    cases.append(('short_aligned_stretch_does_not_fire', neg_p10, False))
    # 11. NEGATIVE: aligned essentially THROUGHOUT (P/R = 0.96) with consistent
    #     arithmetic (R=1000, P=960, K=10 -> s=4, M=240). A row that is aligned
    #     end to end has not lost alignment, so it is not this disease however
    #     well the numbers line up -- only MAX_P_FRAC rejects it.
    neg_pf = _fix(4000, [('diff_arg', 960), ('insert', 60)], hcount=10, hsep=True)
    cases.append(('aligned_fraction_too_high_does_not_fire', neg_pf, False))
    # 12. NEGATIVE: barely any insert/delete (0.8% of R) -- an arg-charge row,
    #     not a misalignment. Arithmetic consistent (R=1000, P=900, K=4 -> s=25,
    #     M=36); only MIN_INSDEL_FRAC rejects.
    neg_id = _fix(4000, [('diff_arg', 900), ('delete', 1), ('diff_arg', 95)],
                  hcount=4, hsep=True)
    cases.append(('too_little_insdel_does_not_fire', neg_id, False))

    ok = True
    for name, fx, want in cases:
        hit, why = verdict(analyse(fx))
        got = hit is not None
        status = 'ok' if got == want else 'FAIL'
        if got != want:
            ok = False
        print(f'  [{status}] {name}: fired={got} want={want} ({why})'
              + (f'  s={hit["s"]} M={hit["M"]} K={hit["K"]}' if hit else ''))
    print('SELFTEST', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--jsonl')
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--all', action='store_true', help='print rejects too')
    ap.add_argument('--json-out')
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if not a.jsonl:
        ap.error('--jsonl or --selftest')
    out = []
    for line in open(a.jsonl):
        an = analyse(json.loads(line))
        hit, why = verdict(an)
        an['hit'], an['why'] = hit, why
        an.pop('helpers', None) if not a.all else None
        out.append(an)
    fires = [x for x in out if x['hit']]
    print(f'population={len(out)}  FIRES={len(fires)}')
    print(collections.Counter(x['why'] for x in out).most_common())
    for x in sorted(fires, key=lambda y: -y['size']):
        h = x['hit']
        print(f"{x['size']:6d}B fz={x['fuzzy']:9.4f} R={x['R']:5d} P={x['P']:5d} "
              f"K={h['K']:3d} s={h['s']:3d}({h['s_raw']}) M={h['M']:4d} N={h['N']:4d} "
              f"| {x['sym'][:56]} | H={h['H'][:50]}")
    if a.json_out:
        json.dump(out, open(a.json_out, 'w'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
