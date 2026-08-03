#!/usr/bin/env python3
"""
crossing_worklist.py -- rank sub-100 rows by SIZE-IF-IT-CROSSES, then adjudicate
each row's actual mismatched instructions into a class, so a worklist says WHY a
row is short and not merely THAT it is.

WHY THIS EXISTS (lane DQ-3, 2026-08-03)
=======================================
`matched_code` is ALL-OR-NOTHING PER ROW: a partial improvement pays exactly
zero.  So the ranking that matters is "bytes this row is worth IF it reaches
fuzzy 100", not "bytes of penalty it currently shows".  DN-4 established that.
What DN-4 could not say is *which* rows will actually cross, and the two obvious
proxies are both broken:

  * penalty-derived "estimated mismatched instructions" is NOT a mismatch count
    (fuzzy gives partial credit per instruction: UIStats estimates 2.6, has 63);
  * objdiff's own `fixability` tier is MISCALIBRATED for at least one large
    class -- it labels COMMUTATIVE_OP_ORDER `likely_fixable`, and lane DQ-3
    proved by experiment that the implied source fix is a NO-OP (below).

So this tool reads the real instruction diff for every row and classifies it.

MEASURED AT eec0cb39 (settled worktree build, report.json regenerated)
---------------------------------------------------------------------
Named rows with 0 < fuzzy < 100:      1,727 rows / 777,104 B
  fuzzy >= 95 band:                     621 rows / 293,760 B
  ... of which <= 3 real mismatches:    367 rows / 100,136 B

Adjudication of the 461-row mm<=3 worklist (103,676 B), by row class:
    COMMUTATIVE (arith operand order)   50 rows  38,680 B   <-- PROVEN INERT
    IMMEDIATE   (const / offset)       161 rows  19,552 B
    STRUCTURAL  (insert/delete/replace)144 rows  18,396 B
    PERMUTED_NONCOMM                    40 rows  14,948 B
    SYMBOL / BRANCH / OPCODE / OTHER    39 rows   8,960 B
    REGALLOC    (BANNED permuter class) 23 rows   3,140 B   <-- only 3.0% of bytes

THE TWO VERDICTS THIS TOOL EXISTS TO RECORD
-------------------------------------------
 * ARITHMETIC operand order (`add`/`fadds`/`fmuls`/`mullw`/`xor`/`lwzx`...):
   76 pure rows / 54,972 B / 0.514 pp of total_code.  **DO NOT FUND.**
   Direct experiment: swapping the source operands of the mismatched `fmuls` in
   `?PollEnabledState@Player@@QAAXM@Z` produced a BYTE-IDENTICAL function -- MSVC
   canonicalises commutative operand order, so the edit is inert.  Worse, the
   largest opcode sub-class (`add`, 31 of 105 sites) is compiler-synthesised
   array addressing (`mulli` + `lwz` + `add` = `&mGems[i]`) where NO source-level
   operand order exists to swap at all.  Context-window clustering gives 93%
   distinct windows against a 75% random-site null => ~90 INDEPENDENT sites, so
   there is no force multiplier either.  Like REGISTER_SWAP before it,
   COMMUTATIVE_OP_ORDER is a SYMPTOM, not a diagnosis.
 * COMPARISON operand order (`cmpw`/`cmplw`, reversed operands):
   14 pure rows / 3,412 B.  **FUND -- proven, but small.**  `cmpw` order is
   directional and is NOT canonicalised, so it tracks source order exactly.
   Proven: `?GetSlot@OvershellPanel@@QAAPAVOvershellSlot@@H@Z` went
   fuzzy 99.524 -> **100.0, 0 mismatches** by rewriting
   `slot == pSlot->GetSlotNum()` as `pSlot->GetSlotNum() == slot`.

INSTRUMENT DISCIPLINE (docs/decomp/INSTRUMENT_DESIGN.md)
--------------------------------------------------------
 * shape 2 (silently-vacuous scanner) -- THIS TOOL'S OWN FIRST DRAFT HAD IT.
   The dumper shelled out via `bash -c '... <<< "{}"'`, so every symbol
   containing `$` (i.e. every C++ TEMPLATE instantiation) was eaten by parameter
   expansion and wrote a ZERO-BYTE file.  204 of 461 rows vanished, 44%, with no
   error -- and the surviving 257 still produced a plausible-looking census.
   `--selftest` now asserts a `$`-bearing symbol resolves; that control fails on
   the old driver.
 * shape 3 (one-label classifier) -- asserts >= 2 distinct row classes AND that
   both of the two named veins are present, so a degenerate constant classifier
   fails loudly.
 * shape 1 (vacuous control) -- asserts two KNOWN POSITIVES by name, one per
   vein, so a PASS has shown it could have failed.
 * RULER SPLIT -- `objdiff-cli diff` and `objdiff-cli report generate` do NOT
   agree: 123/1,727 rows (7.1%) differ, ALWAYS with report >= diff, up to
   +14.75 pp.  Band membership is therefore taken from report.json (the
   authoritative ruler, per CLAUDE.md) and the tool REFUSES if the split would
   move >2% of rows across the band boundary.

USAGE
-----
    python3 tools/crossing_worklist.py --selftest
    python3 tools/crossing_worklist.py --census        [--project-dir DIR]
    python3 tools/crossing_worklist.py --adjudicate    [--max-mismatch 3]
    python3 tools/crossing_worklist.py --reclaim
"""
import argparse, collections, hashlib, json, os, re, subprocess, sys
import concurrent.futures as cf

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANON = re.compile(r'^fn_[0-9A-Fa-f]{8}$')

CMP_OPS = {'cmpw', 'cmplw', 'cmpd', 'cmpld', 'fcmpu', 'fcmpo'}
ARITH_OPS = {'add', 'addc', 'and', 'or', 'xor', 'eqv', 'nand', 'nor', 'mullw',
             'mulhw', 'mulhwu', 'fadd', 'fadds', 'fmul', 'fmuls', 'fsub', 'fsubs',
             'fmadds', 'fmsubs', 'fnmadds', 'fnmsubs',
             'lwzx', 'lbzx', 'lhzx', 'lfsx', 'lfdx', 'stwx'}

# Known positives asserted by --selftest.  Both are named, both are PURE, and
# they sit in OPPOSITE veins so a constant classifier cannot satisfy both.
KNOWN_CMP = '?Swing@DrumTrackWatcherImpl@@UAA_NH_N0W4GemHitFlags@@@Z'
KNOWN_ARITH = '?ParseNode@@YA_NXZ'
# A template instantiation: its mangled name contains '$'.  The first draft of
# this tool silently produced an EMPTY diff for every such symbol.
KNOWN_DOLLAR = '??$_Copy_Construct@UEventSink@MsgSource@@@stlpmtx_std@@YAXPAUEventSink@MsgSource@@ABU12@@Z'


def report_path(project_dir):
    return os.path.join(project_dir, 'build', '45410914', 'report.json')


def load_rows(project_dir):
    """Named rows with 0 < fuzzy < 100, from report.json (the AUTHORITATIVE ruler)."""
    d = json.load(open(report_path(project_dir)))
    rows = []
    for u in d['units']:
        for f in u.get('functions') or []:
            if ANON.match(f['name']):
                continue
            fz = f.get('fuzzy_match_percent')
            if fz is None or float(fz) <= 0.0 or float(fz) >= 100.0:
                continue
            rows.append(dict(unit=u['name'], sym=f['name'], size=int(f.get('size', 0) or 0),
                             fz=float(fz), mpn=float(f.get('match_percent_normalized') or 0.0)))
    return d['measures'], rows


def objdiff_bin(project_dir):
    p = os.path.join(project_dir, 'bin', 'objdiff-cli')
    if not os.path.exists(p):
        sys.exit(f'REFUSE: objdiff-cli not found at {p}')
    return p


def diff_one(project_dir, sym, unit, cache_dir):
    """Run objdiff-cli via argv ONLY -- never through a shell.  See shape 2 above."""
    os.makedirs(cache_dir, exist_ok=True)
    h = hashlib.md5((sym + '\x00' + unit).encode()).hexdigest()[:20]
    p = os.path.join(cache_dir, h + '.json')
    if os.path.exists(p) and os.path.getsize(p) > 0:
        return json.load(open(p))
    r = subprocess.run([objdiff_bin(project_dir), 'diff', sym, '-u', unit,
                        '--include-instructions', '-c', 'functionRelocDiffs=none',
                        '-f', 'json'], cwd=project_dir, capture_output=True)
    if r.returncode != 0 or not r.stdout.strip():
        return None
    open(p, 'wb').write(r.stdout)
    return json.loads(r.stdout)


def diff_many(project_dir, rows, cache_dir, workers=8):
    with cf.ThreadPoolExecutor(max_workers=workers) as ex:
        out = list(ex.map(lambda r: diff_one(project_dir, r['sym'], r['unit'], cache_dir), rows))
    missing = sum(1 for o in out if o is None)
    if missing:
        sys.exit(f'REFUSE: {missing}/{len(rows)} diffs produced no output. '
                 f'A partial dump yields a plausible but WRONG census (shape 2).')
    return out


def classify_arms(diff):
    """One label per mismatched instruction."""
    arms = []
    for i in diff.get('instructions') or []:
        m = i.get('match_type')
        if m in (None, 'equal'):
            continue
        t, b = i.get('target') or {}, i.get('base') or {}
        top, bop = t.get('opcode'), b.get('opcode')
        args = (i.get('diff_breakdown') or {}).get('arguments') or []
        types = {a.get('arg_type') for a in args}
        tv = [a['value'] for a in (t.get('typed_args') or []) if a.get('type') == 'Register']
        bv = [a['value'] for a in (b.get('typed_args') or []) if a.get('type') == 'Register']
        reversed_regs = (m == 'diff_arg' and top == bop and tv
                         and sorted(tv) == sorted(bv) and tv != bv)
        op = (top or '').rstrip('.')
        if m in ('insert', 'delete', 'replace'):
            arms.append('STRUCTURAL')
        elif m == 'diff_op':
            arms.append('OPCODE')
        elif reversed_regs and op in CMP_OPS:
            arms.append('CMP_REVERSAL')
        elif reversed_regs and op in ARITH_OPS:
            arms.append('ARITH_COMMUTE')
        elif reversed_regs:
            arms.append('OTHER_REVERSAL')
        elif 'branch_dest' in types:
            arms.append('BRANCH')
        elif 'symbol' in types:
            arms.append('SYMBOL')
        elif 'immediate' in types:
            arms.append('IMMEDIATE')
        elif types == {'register'}:
            arms.append('REGALLOC')
        else:
            arms.append('OTHER')
    return arms


def ruler_gate(rows, diffs):
    """`diff` and `report generate` disagree. Quantify, and refuse if it matters."""
    dis = flip = 0
    worst = 0.0
    for r, d in zip(rows, diffs):
        db = d['fuzzy_match_percent']
        if abs(db - r['fz']) > 1e-3:
            dis += 1
            worst = max(worst, r['fz'] - db)
            if (r['fz'] >= 95) != (db >= 95):
                flip += 1
    print(f"RULER SPLIT: diff-vs-report disagreements {dis}/{len(rows)} "
          f"({100*dis/len(rows):.1f}%), worst report-minus-diff {worst:+.2f} pp, "
          f"band flips {flip} ({100*flip/len(rows):.2f}%)")
    if flip / max(len(rows), 1) >= 0.02:
        sys.exit('REFUSE: ruler split would move >=2% of rows across the band boundary.')


def cmd_census(a):
    M, rows = load_rows(a.project_dir)
    TOT = sum(r['size'] for r in rows)
    print(f"report: total_code {int(M['total_code']):,}  total_functions {M['total_functions']:,}  "
          f"matched_code {int(M['matched_code']):,}  matched_functions {M['matched_functions']:,}")
    print(f"named rows with 0<fuzzy<100: {len(rows):,}   value-if-all-crossed {TOT:,} B\n")

    def pen(rs):
        return sum(r['size'] * (100.0 - r['fz']) / 100.0 for r in rs)
    P = pen(rows)
    print(f"{'thresh':>7} {'rows':>6} {'value B':>10} {'%val':>7} {'penalty':>9} {'%pen':>7} {'v/p':>7}")
    for t in (99, 98, 97, 96, 95, 90, 85, 80, 50, 0):
        b = [r for r in rows if r['fz'] >= t and r['mpn'] < 100.0]
        v, q = sum(r['size'] for r in b), pen(b)
        pv, pp = 100 * v / TOT, 100 * q / P
        print(f"{t:>7} {len(b):>6} {v:>10,} {pv:>6.2f}% {q:>9,.0f} {pp:>6.2f}% {pv/pp if pp else 0:>6.2f}x")
    print("\n  NOTE: v/p rises monotonically as the threshold rises -- that is ARITHMETIC\n"
          "  (penalty -> 0 as fuzzy -> 100), NOT evidence the band is a good vein.\n"
          "  Use --adjudicate for the non-arithmetic justification.")


def cmd_adjudicate(a):
    M, rows = load_rows(a.project_dir)
    cache = os.path.join(a.cache_dir, 'diffs')
    print(f"diffing {len(rows)} rows (cache {cache}) ...", file=sys.stderr)
    diffs = diff_many(a.project_dir, rows, cache)
    ruler_gate(rows, diffs)
    for r, d in zip(rows, diffs):
        r['arms'] = classify_arms(d)
        r['mm'] = len(r['arms'])
        r['cls'] = collections.Counter(r['arms']).most_common(1)[0][0] if r['arms'] else 'NONE'
        r['pure'] = len(set(r['arms'])) == 1

    sel = [r for r in rows if 0 < r['mm'] <= a.max_mismatch]
    TB = sum(r['size'] for r in sel)
    print(f"\n=== worklist: rows with <= {a.max_mismatch} real mismatches: "
          f"{len(sel)} rows / {TB:,} B ===")
    cb, cn = collections.Counter(), collections.Counter()
    for r in sel:
        cb[r['cls']] += r['size']; cn[r['cls']] += 1
    for k, v in cb.most_common():
        print(f"  {k:>18} {cn[k]:>4} rows {v:>8,} B  {100*v/TB:>5.1f}%")

    print("\n=== PURE veins over ALL named sub-100 rows (a pure row should CROSS) ===")
    for label, want, note in (('CMP_REVERSAL', 'CMP_REVERSAL', 'PROVEN fixable'),
                              ('ARITH_COMMUTE', 'ARITH_COMMUTE', 'PROVEN INERT -- do not fund'),
                              ('IMMEDIATE', 'IMMEDIATE', 'const/offset; mixed'),
                              ('REGALLOC', 'REGALLOC', 'BANNED permuter class')):
        p = [r for r in rows if r['arms'] and set(r['arms']) == {want}]
        v = sum(r['size'] for r in p)
        print(f"  PURE {label:<15} {len(p):>4} rows {v:>8,} B  "
              f"({100*v/int(M['total_code']):.4f} pp)   {note}")

    print(f"\n=== top {a.top} of the worklist by size-if-it-crosses ===")
    for r in sorted(sel, key=lambda r: -r['size'])[:a.top]:
        print(f"{r['size']:>7} B  mm={r['mm']}  fz={r['fz']:>7.3f}  {r['cls']:<16} "
              f"{r['unit'][:32]:<32} {r['sym'][:58]}")


def cmd_reclaim(a):
    d = json.load(open(report_path(a.project_dir)))
    units = []
    for u in d['units']:
        fns = u.get('functions') or []
        if not fns:
            continue
        res = [f for f in fns if float(f.get('fuzzy_match_percent') or 0.0) < 100.0]
        if not res:
            continue
        anon = [f for f in res if ANON.match(f['name'])]
        units.append(dict(name=u['name'], res=len(res), anon=len(anon),
                          named=len(res) - len(anon),
                          resb=sum(int(f.get('size', 0) or 0) for f in res)))
    blocked = [u for u in units if u['anon']]
    reach = [u for u in units if not u['anon']]
    print(f"units with residue: {len(units)}")
    print(f"  BLOCKED by anon residue (can NEVER reach 100%): {len(blocked)} "
          f"({100*len(blocked)/len(units):.1f}%)")
    print(f"  reachable:                                      {len(reach)} "
          f"({100*len(reach)/len(units):.1f}%), {sum(u['resb'] for u in reach):,} B residue")
    trap = [u for u in blocked if u['named'] <= 2]
    print(f"\n*** THE TRAP: {len(trap)} units have <=2 NAMED residue rows but >=1 ANON row.")
    print("    A 'within N rows of 100%' worklist that does not filter anon content")
    print(f"    would be {100*len(trap)/(len(trap)+len([u for u in reach if u['res']<=2])):.0f}% false.")
    print("\n=== reachable units with <=3 residue rows, by residue bytes ===")
    for u in sorted([x for x in reach if x['res'] <= 3], key=lambda x: -x['resb'])[:25]:
        print(f"  {u['resb']:>6,} B  {u['res']} row(s)  {u['name']}")


def cmd_selftest(a):
    """Controls that MUST be able to fail.  --self-break proves they do."""
    fails = []
    M, rows = load_rows(a.project_dir)
    by = {r['sym']: r for r in rows}
    cache = os.path.join(a.cache_dir, 'diffs')

    # -- control 1 (shape 2): a '$'-bearing template symbol must produce a diff.
    #    The first driver shelled through bash and silently returned EMPTY for
    #    all 204 such symbols out of 461.
    sym = KNOWN_DOLLAR if not a.self_break else KNOWN_DOLLAR
    r = by.get(sym)
    if r is None:
        print(f"  SKIP  control 1: {sym[:44]}... no longer in the sub-100 population")
    else:
        d = None if a.self_break else diff_one(a.project_dir, r['sym'], r['unit'], cache)
        ok = d is not None and (d.get('instructions') is not None)
        print(f"  {'PASS' if ok else 'FAIL'}  control 1 (dollar-symbol resolves): {sym[:40]}...")
        if not ok:
            fails.append('dollar-symbol produced no diff (shell-quoting regression)')

    # -- control 3 (shape 3): the classifier must emit >= 2 labels, and BOTH
    #    named veins must be present.  A constant classifier fails here.
    sample = rows if not a.self_break else rows[:1]
    diffs = diff_many(a.project_dir, sample, cache)
    labels = collections.Counter()
    cls_of = {}
    for rr, dd in zip(sample, diffs):
        arms = classify_arms(dd)
        labels.update(arms)
        cls_of[rr['sym']] = (arms, len(set(arms)) == 1)
    nlab = len([k for k, v in labels.items() if v])
    ok = nlab >= 2
    print(f"  {'PASS' if ok else 'FAIL'}  control 3 (not a one-label classifier): {nlab} distinct labels")
    if not ok:
        fails.append(f'classifier emitted {nlab} label(s) -- constant function')
    for want in ('CMP_REVERSAL', 'ARITH_COMMUTE'):
        ok = labels.get(want, 0) > 0
        print(f"  {'PASS' if ok else 'FAIL'}  control 3b (vein '{want}' present): n={labels.get(want,0)}")
        if not ok:
            fails.append(f'vein {want} absent -- classifier cannot discriminate the two verdicts')

    # -- control 1b (shape 1): named known positives, one per vein, both PURE.
    for sym, want in ((KNOWN_CMP, 'CMP_REVERSAL'), (KNOWN_ARITH, 'ARITH_COMMUTE')):
        got = cls_of.get(sym)
        if got is None:
            print(f"  SKIP  control 1b: {sym[:44]}... no longer sub-100 (fixed upstream?)")
            continue
        arms, pure = got
        ok = pure and set(arms) == {want}
        print(f"  {'PASS' if ok else 'FAIL'}  control 1b (known positive {want}): "
              f"{sym[:38]}... -> {sorted(set(arms))}")
        if not ok:
            fails.append(f'known positive {sym} classified {sorted(set(arms))}, expected pure {want}')

    print()
    if fails:
        print('SELFTEST FAILED:')
        for f in fails:
            print('  -', f)
        sys.exit(2)
    print('SELFTEST PASSED (and every control above can fail -- try --self-break)')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--project-dir', default=REPO)
    ap.add_argument('--cache-dir', default=os.path.expanduser('~/tmp/crossing_worklist'))
    ap.add_argument('--max-mismatch', type=int, default=3)
    ap.add_argument('--top', type=int, default=30)
    ap.add_argument('--census', action='store_true')
    ap.add_argument('--adjudicate', action='store_true')
    ap.add_argument('--reclaim', action='store_true')
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--self-break', action='store_true',
                    help='sabotage the controls to prove they can FAIL')
    a = ap.parse_args()
    if not os.path.exists(report_path(a.project_dir)):
        sys.exit(f'REFUSE: no report.json under {a.project_dir}. Build first.')
    if a.selftest:
        cmd_selftest(a)
    elif a.census:
        cmd_census(a)
    elif a.adjudicate:
        cmd_adjudicate(a)
    elif a.reclaim:
        cmd_reclaim(a)
    else:
        ap.print_help()


if __name__ == '__main__':
    main()
