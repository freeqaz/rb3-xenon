#!/usr/bin/env python3
"""callee_set_join.py -- FORWARD call-graph content join: name an UNNAMED retail VA.

Why
---
`caller_side_invert.py` (lane K, 99.09% held-out) exploits call-graph *content* in
the CALLER direction: a homed caller's relocation names the callee, so the retail
instruction at that offset derives the callee's VA.  It needs the callee's body to
be reloc-masked byte-identical to retail (hit-set containment guard), so it cannot
touch a retail function whose body we do not reproduce.

The funclet-cascade lane (`funclet_cascade_rank.py`) surfaced that pool: 1,809
pinned EH parents that have **never been named** (`fn_8XXXXXXX`).  They were
never homed by `homing_scan.py` precisely *because* their bodies diverge.

⚠ What naming them is worth (MEASURED, 2026-07-26, do not re-price this):
a parent's map entry has **no causal effect on whether its EH funclets match**.
Removing the entries of 116 frame-mismatched named parents (473 funclets) moved
the strict count by 0; removing 60 frame-OK parents holding 1,446 matched
funclets lost 47 functions and **zero funclets** -- all 47 were the parents
themselves.  The 56.9%-vs-96.7% calibration table in
`docs/plans/funclet-cascade-lever-2026-07-25.md` is CORRELATIONAL.  So the only
strict delta a naming wave banks is: **parents whose compiled body is already
reloc-masked byte-identical at the derived VA**.  `--propose` therefore splits
its output on exactly that test (BANKABLE vs WORKLIST) and never pretends
otherwise.

This tool is the FORWARD direction, and identifying does not need byte identity:

    a big EH-heavy function whose body only 40%-matches ours still CALLS
    mostly the same functions and still references the same string literals.

  RETAIL side  : disassemble [VA, VA+len) from the PE (`.pdata` gives len),
                 collect every `bl` target, resolve it through
                 `scripts/target_symbol_map.json` -> a multiset of callee NAMES;
                 plus every `lis`/lo-formed address that lands on a C string.
  OUR side     : for each compiled function in the SAME PINNED UNIT, read the
                 `.text` relocations out of the COFF -> the multiset of callee
                 symbol names the `bl` relocations point at, plus the contents of
                 the `??_C@` string COMDATs it references.
  JOIN         : multiset Jaccard / containment, restricted to candidates in the
                 same unit, with an evidence floor and a decisive margin.

Both sides of every token exist independently, so this is evidence and not a
retail-only fingerprint (the failure mode that killed `.pdata` prolog shape and
neighbourhood fingerprints -- see docs/plans/identification-discriminators-2026-07-25.md).

Honesty model
-------------
A mispaired name still reads 100% under objdiff's normalized diff, so a wrong
accept is invisible in the score and permanently corrupts the map.  Therefore:

* the winner must clear an ABSOLUTE floor (evidence count + jaccard +
  containment), not merely beat the runner-up;
* the winner must beat the runner-up by a decisive MARGIN; ties are REFUSED and
  counted;
* precision is measured HELD-OUT, with a threshold sweep, and additionally under
  a **negative control** in which the true answer is removed from the candidate
  set -- because in production the retail function may have no counterpart among
  our compiled symbols at all, and validation with the truth always present
  cannot see that failure mode.

Usage
-----
    callee_set_join.py --worktree WT --census
    callee_set_join.py --worktree WT --validate [--sweep] [--limit N]
    callee_set_join.py --worktree WT --propose --out prop.json \\
        [--targets ranked.json] [--min-evidence N] [--min-jaccard F] \\
        [--min-containment F] [--min-margin F]

`--propose` emits homing_scan result format (`tu -> [record]`, `cls=UNIQUE`) so
`span_predictor.py` / `homing_gen4.py` consume it unchanged, plus a second file
`<out>_worklist.json` holding the identified-but-not-byte-identical picks.

Measured (2026-07-26, 27,629 baseline)
--------------------------------------
Held-out precision at the shipped defaults (min_inter 8 / jaccard 0.70 /
margin 0.30), 11,393 validation targets:

    all held-out cases                     618/619 = 99.84%
    truth's body does NOT match (<100%)     86/ 86 = 100.00%  (Wilson LB 95.7%)
        ... fold 0 41/41, fold 1 45/45      (thresholds picked on fold 0 only)
    negative control (truth removed)         4 false accepts in 11,393

Channels, measured on the same held-out set:

    callee-name multiset + string set   96.4% on the <100 bucket at loose floors
    + call-ORDER bigrams                96.3%   -- NO GAIN, off by default
    + |size| tolerance guard            96.8%   -- marginal, off by default
    tightening the floors instead       100%    -- this is what actually works

Production run over all 11,844 unnamed pinned non-vendor, non-funclet VAs:
135 resolved, 26 ties REFUSED, of which **5 BANKABLE** (byte-identical; all 5
`PAYS` per span_predictor, applied -> 27,629 -> 27,634, 0 lost) and **130
WORKLIST** (correct-by-evidence, body diverges, banks nothing today).
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from funclet_cascade_rank import (  # noqa: E402
    PE, load_report, parse_pdata, parse_splits, span_unit, unit_function_vas,
    unit_report_key, unit_text_spans,
)
from multi_content_disambiguate import Band, func_table, load_tmap, sext  # noqa: E402

# prologue/epilogue register-save thunks: every function calls them, so they are
# pure noise that would inflate both the evidence count and the similarity.
BOILER_RX = re.compile(r'^__(?:save|rest)(?:gprlr|gpr|fpr|vmx|vr)')
VENDOR_UNIT_RX = re.compile(r'^(?:default/)?auto_\d+_')
D_FORM_OPS = set(range(32, 56)) | {14, 24, 25, 26, 27, 28, 29}


# --------------------------------------------------------------- retail tokens
def bigrams(seq):
    """Adjacent pairs of the *resolvable* call sequence.

    The multiset of callee names cannot tell two sibling methods of one class
    apart -- `VocalPlayer::Handle` and `VocalPlayer::Poll` call almost the same
    helpers.  The ORDER in which they call them is different, and order survives
    body divergence far better than exact bytes do.  Both sequences are filtered
    to names that exist on both sides (i.e. mapped), so adjacency is comparable.
    """
    return Counter(zip(seq, seq[1:]))


def retail_tokens(pe: PE, band: Band, va: int, size: int, va2name: dict,
                  want_str: bool = True):
    """Content-token multiset a retail function emits.

    -> dict(sym=Counter(name->n), strs=set(tok), bg=Counter(pair->n),
            n_calls, n_unmapped_calls)
    """
    n = size // 4
    if n <= 0 or n > 40000:
        return None
    words = pe.words(va, n)
    if not words:
        return None
    sym = Counter()
    strs = set()
    seq = []
    ncall = 0
    nunmapped = 0
    hi = {}
    for i, w in enumerate(words):
        cur = va + 4 * i
        if (w & 0xFC000003) == 0x48000001:          # bl, AA=0, LK=1
            t = (cur + sext(w & 0x03FFFFFC, 26)) & 0xFFFFFFFF
            if t == va:                              # self-recursion: in
                continue                             # production this VA is
            ncall += 1                               # unmapped, so it would
            nm = va2name.get(t)                      # carry no name.  Dropping
            if nm is None:                           # it also removes the only
                nunmapped += 1                       # validation leak.
            elif not BOILER_RX.match(nm):
                sym[nm] += 1
                seq.append(nm)
            continue
        op = w >> 26
        if op == 15 and ((w >> 16) & 31) == 0:      # lis rD, hi
            hi[(w >> 21) & 31] = w & 0xFFFF
        elif want_str and op in D_FORM_OPS:
            rA = (w >> 16) & 31
            if rA in hi:
                addr = ((hi[rA] << 16) + sext(w & 0xFFFF, 16)) & 0xFFFFFFFF
                if not band.in_text(addr):
                    s = band.cstr(addr)
                    if s is not None:
                        strs.add(('str', s))
                    else:
                        ws = band.wstr(addr)
                        if ws is not None:
                            strs.add(('wstr', ws))
    return dict(sym=sym, strs=strs, bg=bigrams(seq), n_calls=ncall,
                n_unmapped_calls=nunmapped)


def our_tokens(f: dict, name: str, name2va: dict, want_str: bool = True):
    """Same token multiset, read out of our compiled COFF function.

    The call sequence is filtered to *mapped* names so that its bigrams are
    comparable with the retail side, where an unmapped callee is invisible.
    """
    sym = Counter()
    strs = set()
    seq = []
    for off in sorted(f['refs']):
        tname, _ty, tok = f['refs'][off]
        w = f['words'].get(off)
        if w is not None and (w >> 26) == 18 and (w & 1):     # bl relocation
            if tname == name or BOILER_RX.match(tname):
                continue
            sym[tname] += 1
            if tname in name2va:
                seq.append(tname)
        elif want_str and tok is not None and tok[0] in ('str', 'wstr'):
            strs.add(tok)
    return dict(sym=sym, strs=strs, bg=bigrams(seq))


def score(A: dict, B: dict, want_bg: bool = True):
    """(inter, union, jaccard, containment) between retail A and ours B.

    `sym` is compared as a MULTISET (calling the same helper 3 times is stronger
    evidence than calling it once); `strs` as a SET, because our side records a
    string reference at both the hi and lo relocation slots while the retail scan
    materialises it once -- multiplicity is not comparable there; `bg` (adjacent
    pairs of the mapped call sequence) as a multiset, and it is what separates
    sibling methods of one class.
    """
    a, b = A['sym'], B['sym']
    inter = sum(min(a[k], b[k]) for k in (a.keys() & b.keys()))
    union = sum(a.values()) + sum(b.values()) - inter
    si = len(A['strs'] & B['strs'])
    su = len(A['strs'] | B['strs'])
    inter += si
    union += su
    tot_a = sum(a.values()) + len(A['strs'])
    sinter = inter                      # sym+str only: the interpretable floor
    if want_bg:
        x, y = A['bg'], B['bg']
        bi = sum(min(x[k], y[k]) for k in (x.keys() & y.keys()))
        inter += bi
        union += sum(x.values()) + sum(y.values()) - bi
        tot_a += sum(x.values())
    jac = inter / union if union else 0.0
    cont = inter / tot_a if tot_a else 0.0
    return sinter, inter, union, jac, cont


def is_funclet(f: dict) -> bool:
    """First instruction ``subi rX, r12, imm`` -> an EH funclet, never a target."""
    b = f['body']
    if len(b) < 4:
        return False
    w = int.from_bytes(b[:4], 'big')
    return (w >> 26) == 14 and ((w >> 16) & 31) == 12 and (w & 0x8000) != 0


# ------------------------------------------------------------------- the world
class World:
    def __init__(self, wt: str, exe: str | None = None):
        self.wt = Path(wt)
        exe = Path(exe) if exe else self.wt / 'orig/45410914/band.exe'
        self.pe = PE(exe)
        self.band = Band(str(exe))
        self.funcs = parse_pdata(self.pe)
        units = parse_splits(self.wt / 'config/45410914/splits.txt')
        self.va2unit_raw, _ = unit_function_vas(self.pe, units)
        self.spans = unit_text_spans(units)
        self.match, self.srcmap = load_report(self.wt / 'build/45410914/report.json')
        self.report_units = {u for (u, _n) in self.match}
        self.va2name, self.name2va = load_tmap(
            str(self.wt / 'scripts/target_symbol_map.json'))
        self.objdir = self.wt / 'build/45410914/src'
        self._ft = {}
        self._ucache = {}
        self._our = {}

    def unit_of(self, va):
        if va in self._ucache:
            return self._ucache[va]
        u = self.va2unit_raw.get(va) or span_unit(self.spans, va)
        r = unit_report_key(u, self.report_units) if u else None
        self._ucache[va] = r
        return r

    def obj_path(self, unit):
        sp = self.srcmap.get(unit)
        if not sp:
            return None
        p = self.objdir / (sp[4:] if sp.startswith('src/') else sp)
        return p.with_suffix('.obj')

    def ftable(self, unit):
        if unit not in self._ft:
            p = self.obj_path(unit)
            self._ft[unit] = func_table(str(p)) if (p and p.exists()) else {}
        return self._ft[unit]

    def our_tok(self, unit, name, want_str=True):
        k = (unit, name, want_str)
        if k not in self._our:
            f = self.ftable(unit).get(name)
            self._our[k] = (our_tokens(f, name, self.name2va, want_str)
                            if f else None)
        return self._our[k]

    def byte_identical(self, unit, name, va):
        """Is our compiled body reloc-masked byte-identical to retail at `va`?

        This is the ONLY thing a map entry converts into a strict match (measured
        by the lane-I follow-up: naming a parent has no causal effect on its EH
        funclets).  Picks that fail this are correct-by-evidence but bank nothing
        today -- they are a body-port worklist with a known target.
        """
        f = self.ftable(unit).get(name)
        if f is None:
            return False
        raw = self.band.text_bytes(va, f['size'])
        if raw is None or len(raw) != f['size']:
            return False
        bb = bytearray(raw)
        for off in f['offs']:
            for b in range(4):
                if off + b < len(bb):
                    bb[off + b] = 0
        return bytes(bb) == f['body']

    def candidates(self, unit, extra=None, min_size=0):
        """Our compiled functions in `unit` that production would consider."""
        out = []
        for name, f in self.ftable(unit).items():
            if name in self.name2va and name != extra:
                continue                                  # already homed
            if is_funclet(f):
                continue                                  # naming a funclet = regression
            if f['size'] < min_size:
                continue
            out.append(name)
        if extra and extra not in out and extra in self.ftable(unit):
            out.append(extra)
        return out


# ------------------------------------------------------------------- resolving
def rank(w: World, unit: str, rt: dict, cands: list, want_str=True, want_bg=True):
    """-> sorted [(jac, containment, sym_inter, name, our_size, inter)] best first."""
    rows = []
    ft = w.ftable(unit)
    for nm in cands:
        ot = w.our_tok(unit, nm, want_str)
        if ot is None:
            continue
        sinter, inter, _u, jac, cont = score(rt, ot, want_bg)
        rows.append((jac, cont, sinter, nm, ft[nm]['size'], inter))
    rows.sort(key=lambda r: (-r[0], -r[5], r[3]))
    return rows


def decide(rows, floors):
    """Apply the evidence floors.  -> (verdict, row, runner_up)"""
    if not rows:
        return 'NO-CANDIDATE', None, None
    top = rows[0]
    second = rows[1] if len(rows) > 1 else None
    if top[2] < floors['min_inter']:
        return 'WEAK-OVERLAP', top, second
    if top[0] < floors['min_jaccard']:
        return 'LOW-JACCARD', top, second
    if top[1] < floors['min_containment']:
        return 'LOW-CONTAINMENT', top, second
    if second is not None:
        if top[0] - second[0] < floors['min_margin']:
            return 'TIE', top, second
    return 'RESOLVED', top, second


# ---------------------------------------------------------------------- census
def census(w: World, ranked_path: str | None, args):
    pinned = named = unnamed = vendor = funclet_like = 0
    ev_hist = Counter()
    cand_hist = Counter()
    detail = []
    screened = set()
    for va, info in w.funcs.items():
        wds = w.pe.words(va, 1)
        if wds:
            x = wds[0]
            if (x >> 26) == 14 and ((x >> 16) & 31) == 12 and (x & 0x8000):
                screened.add(va)
    ranked = None
    if ranked_path:
        ranked = {int(r['parent_va'], 16) for r in json.load(open(ranked_path))
                  if 'UNNAMED' in r['flags'] and 'PARENT_UNPINNED' not in r['flags']}

    for va, info in sorted(w.funcs.items()):
        u = w.unit_of(va)
        if not u:
            continue
        if VENDOR_UNIT_RX.match(u):
            vendor += 1
            continue
        pinned += 1
        if va in w.va2name:
            named += 1
            continue
        if va in screened:
            funclet_like += 1
            continue
        unnamed += 1
        if ranked is not None and va not in ranked:
            continue
        rt = retail_tokens(w.pe, w.band, va, info['size'], w.va2name)
        if rt is None:
            continue
        ev = sum(rt['sym'].values()) + len(rt['strs'])
        ev_hist['%s' % ('0' if ev == 0 else '1-2' if ev <= 2 else '3-5' if ev <= 5
                        else '6-10' if ev <= 10 else '11+')] += 1
        cands = w.candidates(u)
        cand_hist['%s' % ('0' if not cands else '1' if len(cands) == 1
                          else '2-10' if len(cands) <= 10 else '11-50'
                          if len(cands) <= 50 else '51+')] += 1
        detail.append((va, u, ev, len(cands), rt['n_calls'], rt['n_unmapped_calls']))

    print('## Census (pinned, non-vendor .pdata functions)\n')
    print('* pinned functions      = %6d' % pinned)
    print('* already named         = %6d' % named)
    print('* EH funclets (skipped) = %6d' % funclet_like)
    print('* UNNAMED targets       = %6d' % unnamed)
    print('* vendor (skipped)      = %6d' % vendor)
    if ranked_path:
        print('* restricted to funclet-parent pool = %d' % len(detail))
    print('\n### retail-side evidence available (mapped callees + strings)\n')
    for k in ('0', '1-2', '3-5', '6-10', '11+'):
        if ev_hist[k]:
            print('  evidence %-5s : %5d' % (k, ev_hist[k]))
    print('\n### in-unit candidate-set size (our unmapped, non-funclet symbols)\n')
    for k in ('0', '1', '2-10', '11-50', '51+'):
        if cand_hist[k]:
            print('  candidates %-5s : %5d' % (k, cand_hist[k]))
    if args.census_out:
        json.dump([dict(va='0x%08X' % v, unit=u, evidence=e, candidates=c,
                        calls=nc, unmapped_calls=nu)
                   for v, u, e, c, nc, nu in detail],
                  open(args.census_out, 'w'), indent=1)
        print('\n-> %s' % args.census_out)


# ------------------------------------------------------------------ validation
def bucket_ev(e):
    return '1-2' if e <= 2 else '3-5' if e <= 5 else '6-10' if e <= 10 else '11+'


def bucket_cand(n):
    return '1' if n <= 1 else '2-10' if n <= 10 else '11-50' if n <= 50 else '51+'


def validate(w: World, args):
    """Held-out: hide the truth, feed all in-unit candidates, score the pick."""
    cases = []
    for va, info in sorted(w.funcs.items()):
        nm = w.va2name.get(va)
        if not nm:
            continue
        u = w.unit_of(va)
        if not u or VENDOR_UNIT_RX.match(u):
            continue
        ft = w.ftable(u)
        f = ft.get(nm)
        if f is None or is_funclet(f):
            continue
        if len(w.name2va.get(nm, ())) != 1:      # ambiguous truth
            continue
        cases.append((va, info, u, nm))
        if args.limit and len(cases) >= args.limit:
            break
    print('validation cases: %d' % len(cases), file=sys.stderr)

    recs = []
    for va, info, u, truth in cases:
        rt = retail_tokens(w.pe, w.band, va, info['size'], w.va2name,
                           want_str=not args.no_str)
        if rt is None:
            continue
        ev = sum(rt['sym'].values()) + len(rt['strs'])
        cands = w.candidates(u, extra=truth)
        rows = rank(w, u, rt, cands, want_str=not args.no_str,
                    want_bg=args.bigrams)
        # negative control: same target, truth removed from the candidate set
        rows_nc = [r for r in rows if r[3] != truth]
        m = w.match.get((u, truth))
        recs.append(dict(va=va, unit=u, truth=truth, ev=ev, ncand=len(cands),
                         rows=rows[:4], rows_nc=rows_nc[:4],
                         match=m, tgt_size=info['size'],
                         our_size=w.ftable(u)[truth]['size']))
    print('scored: %d' % len(recs), file=sys.stderr)

    if args.recs_out:
        json.dump([dict(r, rows=[list(x) for x in r['rows']],
                        rows_nc=[list(x) for x in r['rows_nc']],
                        va='0x%08X' % r['va']) for r in recs],
                  open(args.recs_out, 'w'), indent=1)

    grids = []
    if args.sweep:
        for mi in (2, 3, 4, 6):
            for mj in (0.30, 0.40, 0.50, 0.60):
                for mm in (0.05, 0.10, 0.20):
                    grids.append(dict(min_inter=mi, min_jaccard=mj,
                                      min_containment=args.min_containment,
                                      min_margin=mm))
    else:
        grids.append(dict(min_inter=args.min_evidence,
                          min_jaccard=args.min_jaccard,
                          min_containment=args.min_containment,
                          min_margin=args.min_margin))

    print('\n## Threshold sweep (held-out)\n')
    print('| min_inter | min_jac | min_margin | picks | correct | precision | '
          'FALSE-ACCEPT (truth absent) | of |')
    print('|--:|--:|--:|--:|--:|--:|--:|--:|')
    best = None
    for g in grids:
        hit = miss = 0
        fa = fa_tot = 0
        for r in recs:
            v, top, _s = decide(r['rows'], g)
            if v == 'RESOLVED':
                if top[3] == r['truth']:
                    hit += 1
                else:
                    miss += 1
            v2, top2, _s2 = decide(r['rows_nc'], g)
            fa_tot += 1
            if v2 == 'RESOLVED':
                fa += 1
        n = hit + miss
        prec = 100.0 * hit / n if n else 0.0
        print('| %d | %.2f | %.2f | %d | %d | %s | %d | %d |'
              % (g['min_inter'], g['min_jaccard'], g['min_margin'], n, hit,
                 '%.2f%%' % prec if n else '--', fa, fa_tot))
        if n >= 30 and (best is None or (prec, n) > best[0]):
            best = ((prec, n), g)

    g = (best[1] if (args.sweep and best) else grids[-1])
    print('\n## Detail at min_inter=%d min_jac=%.2f min_cont=%.2f min_margin=%.2f\n'
          % (g['min_inter'], g['min_jaccard'], g['min_containment'], g['min_margin']))
    verd = Counter()
    by_ev = defaultdict(lambda: [0, 0])
    by_cand = defaultdict(lambda: [0, 0])
    by_match = defaultdict(lambda: [0, 0])
    misses = []
    for r in recs:
        v, top, second = decide(r['rows'], g)
        verd[v] += 1
        if v != 'RESOLVED':
            continue
        ok = top[3] == r['truth']
        by_ev[bucket_ev(r['ev'])][0 if ok else 1] += 1
        by_cand[bucket_cand(r['ncand'])][0 if ok else 1] += 1
        mb = ('unmapped-match' if r['match'] is None else
              '100%' if r['match'] >= 100.0 else
              '90-100' if r['match'] >= 90 else '<90')
        by_match[mb][0 if ok else 1] += 1
        if not ok:
            misses.append((r, top, second))
    print('verdicts:', dict(verd.most_common()))
    tot_h = sum(v[0] for v in by_ev.values())
    tot_m = sum(v[1] for v in by_ev.values())
    print('\n**HELD-OUT PRECISION: %d/%d = %.2f%%**'
          % (tot_h, tot_h + tot_m,
             100.0 * tot_h / (tot_h + tot_m) if tot_h + tot_m else 0))
    for title, d, order in (
            ('mapped retail callees + strings available (evidence)', by_ev,
             ('1-2', '3-5', '6-10', '11+')),
            ('in-unit candidate-set size', by_cand,
             ('1', '2-10', '11-50', '51+')),
            ('our match%% of the truth (production pool is the <100 rows)',
             by_match, ('100%', '90-100', '<90', 'unmapped-match'))):
        print('\n### by %s\n' % title)
        for k in order:
            h, m = d[k]
            if h + m:
                print('  %-16s %4d/%4d = %6.2f%%' % (k, h, h + m,
                                                     100.0 * h / (h + m)))
    if misses and args.misses_out:
        with open(args.misses_out, 'w') as fh:
            for r, top, second in misses:
                fh.write('MISS 0x%08X %s truth=%s got=%s jac=%.3f cont=%.3f '
                         'inter=%d ev=%d ncand=%d\n'
                         % (r['va'], r['unit'], r['truth'], top[3], top[0],
                            top[1], top[2], r['ev'], r['ncand']))
        print('\nmisses -> %s' % args.misses_out)


# -------------------------------------------------------------------- proposal
def propose(w: World, args):
    floors = dict(min_inter=args.min_evidence, min_jaccard=args.min_jaccard,
                  min_containment=args.min_containment,
                  min_margin=args.min_margin)
    targets = []
    if args.targets:
        for r in json.load(open(args.targets)):
            if 'UNNAMED' in r['flags'] and 'PARENT_UNPINNED' not in r['flags']:
                targets.append(int(r['parent_va'], 16))
    else:
        for va in w.funcs:
            if va in w.va2name:
                continue
            # naming an already-matched anonymous EH funclet is a measured
            # REGRESSION (-13), so retail funclets are never targets.
            wd = w.pe.words(va, 1)
            if wd and (wd[0] >> 26) == 14 and ((wd[0] >> 16) & 31) == 12 \
                    and (wd[0] & 0x8000):
                continue
            u = w.unit_of(va)
            if u and not VENDOR_UNIT_RX.match(u):
                targets.append(va)
    targets = sorted(set(targets))
    print('targets: %d' % len(targets), file=sys.stderr)

    stats = Counter()
    picked = defaultdict(list)          # our name -> [(va, unit)]
    rows_out = []
    for va in targets:
        info = w.funcs.get(va)
        u = w.unit_of(va)
        if info is None or not u:
            stats['NO-UNIT'] += 1
            continue
        rt = retail_tokens(w.pe, w.band, va, info['size'], w.va2name,
                           want_str=not args.no_str)
        if rt is None:
            stats['NO-TEXT'] += 1
            continue
        cands = w.candidates(u)
        rr = rank(w, u, rt, cands, want_str=not args.no_str,
                  want_bg=args.bigrams)
        v, top, second = decide(rr, floors)
        stats[v] += 1
        if v != 'RESOLVED':
            continue
        picked[top[3]].append((va, u, top, rt, info))
        rows_out.append((va, u, top, second, rt, info))

    # a name may be proposed for only one VA (the map is 1:1 by construction)
    out = defaultdict(list)              # BANKABLE: reloc-masked byte-identical
    work = []                            # body-port worklist: identified, not equal
    n_contest = 0
    for nm, cl in picked.items():
        if len(cl) > 1:
            n_contest += 1
            stats['DROP-CONTESTED'] += len(cl)
            continue
        va, u, top, rt, info = cl[0]
        tu = w.srcmap.get(u, '')
        tu = tu[4:] if tu.startswith('src/') else tu
        tu = tu[:-4] if tu.endswith('.cpp') else tu
        ev = dict(jaccard=round(top[0], 4), containment=round(top[1], 4),
                  inter=top[2],
                  retail_evidence=sum(rt['sym'].values()) + len(rt['strs']),
                  tgt_size=info['size'], our_size=top[4])
        rec = dict(name=nm, size=top[4], cls='UNIQUE', va='0x%08x' % va,
                   n_unmapped=1, hits=['0x%08x' % va],
                   disambig='CALLEE-SET-JOIN', evidence=ev)
        # A map entry only converts into a strict match when our compiled body is
        # ALREADY reloc-masked byte-identical at that VA (measured: naming a
        # parent has no causal effect on its EH funclets).  Everything else is
        # an identification with a known target and a known gap -- real value,
        # but for a body-port lane, not for a map apply.
        if w.byte_identical(u, nm, va):
            stats['BANKABLE'] += 1
            out[tu].append(rec)
        else:
            stats['WORKLIST'] += 1
            work.append(dict(rec, unit=u, tu=tu))
    json.dump(dict(out), open(args.out, 'w'), indent=1)
    wp = args.out.replace('.json', '_worklist.json')
    json.dump(work, open(wp, 'w'), indent=1)
    print('verdicts:', dict(stats.most_common()))
    print('BANKABLE (byte-identical, map entry converts): %d across %d units'
          % (sum(len(v) for v in out.values()), len(out)))
    print('WORKLIST (identified, body diverges -> no strict delta today): %d'
          % len(work))
    print('contested names dropped: %d' % n_contest)
    print('->', args.out)
    print('->', wp)


# ------------------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--exe')
    ap.add_argument('--census', action='store_true')
    ap.add_argument('--census-out')
    ap.add_argument('--validate', action='store_true')
    ap.add_argument('--sweep', action='store_true')
    ap.add_argument('--propose', action='store_true')
    ap.add_argument('--targets', help='funclet_cascade_rank --json output')
    ap.add_argument('--out', default='/home/free/tmp/laneL/callee_prop.json')
    ap.add_argument('--recs-out')
    ap.add_argument('--misses-out')
    ap.add_argument('--limit', type=int, default=0)
    ap.add_argument('--no-str', action='store_true',
                    help='drop the string channel (measure its worth)')
    ap.add_argument('--bigrams', action='store_true',
                    help='ADD the call-ORDER bigram channel.  Measured on the '
                         'full held-out set it does NOT help (it slightly hurts: '
                         '<100-bucket 96.3%% vs 96.4%% at the same floors), so it '
                         'is off by default -- kept because the negative result '
                         'is worth being able to re-derive.')
    # Defaults = operating point "D", chosen by a 2-fold split of the held-out
    # set (see the lane report).  On the production analogue (truths whose body
    # does NOT match, i.e. the pool this tool exists for) it scores 41/41 on
    # fold 0 and 45/45 on fold 1 -- 86/86 = 100%, Wilson LB 95.7% -- and
    # 618/619 = 99.84% over all held-out cases, with 4 false accepts in 11,393
    # negative-control runs.  Loosening to (6, 0.50, 0.20) drops the analogue to
    # 96.2%; tightening further only costs yield.  Do not loosen these without
    # re-running --validate --sweep.
    ap.add_argument('--min-evidence', type=int, default=8,
                    help='minimum |retail tokens matched| (the evidence floor)')
    ap.add_argument('--min-jaccard', type=float, default=0.70)
    ap.add_argument('--min-containment', type=float, default=0.0)
    ap.add_argument('--min-margin', type=float, default=0.30)
    a = ap.parse_args()

    w = World(a.worktree, a.exe)
    if a.census:
        census(w, a.targets, a)
    elif a.validate:
        validate(w, a)
    elif a.propose:
        propose(w, a)
    else:
        ap.error('pick --census / --validate / --propose')


if __name__ == '__main__':
    main()
