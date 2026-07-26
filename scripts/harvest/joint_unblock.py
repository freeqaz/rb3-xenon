#!/usr/bin/env python3
"""joint_unblock.py -- the map<->splits cross-feed.

THE PROBLEM THE TWO SINGLE-OWNER LANES CANNOT SEE
-------------------------------------------------
`map_displace_round.py` proves, by reloc-masked byte identity, that a mangled
name lives at a retail VA.  It then *throws the proof away* whenever
`span_predictor.py` says the VA does not PAY:

    displace round: {..., 'refuse-span-UNPINNED': 23, 'refuse-span-WRONG-UNIT': 7}

Those 30 are not wrong.  They are **correct map repairs that `splits.txt`
forbids from scoring**, because objdiff can only pair a target symbol against
our obj when the VA's pinned unit is a unit whose obj defines that name.

Symmetrically, the splits lane refuses to move a span it has no name evidence
for.  Each lane's residue is the other lane's input; run alone, both report
FIXPOINT with the joint pool untouched.  This tool expresses the joint move.

    UNPINNED    the VA is in no pinned `.text` range at all
                -> ADD a range covering exactly that retail function to a unit
                   whose obj defines the name.
    WRONG-UNIT  the VA is pinned to unit O, and O's obj does not define the
                name, but some other unit's obj does
                -> MOVE that function's extent from O to the definer.

EXTENTS ARE EXACT, NOT GUESSED
------------------------------
`config/45410914/symbols.txt` is dtk's carve table for the whole retail image:
every `.text` function with its address and size.  A pin is emitted as exactly
[fn.start, fn.end) of the retail function *containing* the VA, so it can never
bisect a neighbour.  A candidate whose VA is not a retail function start is
refused rather than snapped blindly.

CLAIMANT CHOICE
---------------
A COMDAT is defined by every obj that instantiates it, so `n_definers` is often
> 1 (STL templates: up to 13).  Byte identity fixes the VA; it cannot fix the
unit.  Two tiers are emitted so they can be priced separately:

    T_SOLE      exactly one of our units defines the name.  No choice to make.
    T_SPATIAL   several definers; keep only when EXACTLY ONE of them owns a
                pinned span adjacent to (or nearest, within --spatial-window)
                the VA.  Retail is not LTCG-built, so `.text` preserves per-TU
                grouping -- this is the same positive spatial fact that
                measured +21/-0 as map_repoint_round.py's discriminator 2.
                Ambiguous cases are REFUSED, never coin-flipped.

REFUSALS (the three measured criteria, plus extent sanity)
----------------------------------------------------------
  1.  a mapped, name-paired symbol inside the span that the claimant's obj does
      not define  (objdiff pairs by NAME, so that symbol is a guaranteed loss)
  2.  the claimant already defines a strict-100 symbol of the same mangled name
      (name pairing would split them and kill the existing match)
  3.  nothing carved in the span
Plus: VA not a retail function start; extent overlaps an existing range (ADD);
span not fully inside one donor range (MOVE -- splits_move enforces this too).

USAGE
-----
    joint_unblock.py plan --worktree WT --span-detail <displace _detail.json.span>
                          --report REPORT --out-moves M.json --out-blocks B.json
                          [--tiers T_SOLE,T_SPATIAL]

`--out-moves` feeds `splits_move.py apply --moves`; `--out-blocks` feeds
`homing_apply4.py --blocks` (both are textual, both re-audit).
"""
import argparse
import bisect
import json
import os
import re
import sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from span_predictor import build_definer_index  # noqa: E402

RANGE_RE = re.compile(r'^\s*\.(\w+)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)')
SYM_RE = re.compile(
    r'^(\S+)\s*=\s*\.text:0x([0-9A-Fa-f]+);.*?type:function.*?size:0x([0-9A-Fa-f]+)')


def load_retail_functions(worktree):
    """[(start, end, name)] for every .text function in dtk's carve table."""
    fns = []
    path = os.path.join(worktree, 'config/45410914/symbols.txt')
    for ln in open(path):
        m = SYM_RE.match(ln)
        if m:
            a = int(m.group(2), 16)
            fns.append((a, a + int(m.group(3), 16), m.group(1)))
    fns.sort()
    return fns


def load_spans(worktree):
    """[(start, end, unit)] for every pinned .text range, sorted."""
    spans, cur = [], None
    path = os.path.join(worktree, 'config/45410914/splits.txt')
    for ln in open(path):
        ln = ln.rstrip('\n')
        if ln.endswith(':') and ln and not ln.startswith((' ', '\t')):
            cur = ln[:-1]
            continue
        m = RANGE_RE.match(ln)
        if m and cur and m.group(1) == 'text':
            spans.append((int(m.group(2), 16), int(m.group(3), 16), cur))
    spans.sort()
    return spans


def unit_matches(tu, header):
    """splits headers are bare basenames or partial paths; obj keys are paths."""
    want = tu + '.cpp'
    return want == header or want.endswith('/' + header)


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('plan')
    p.add_argument('--worktree', required=True)
    p.add_argument('--span-detail', required=True,
                   help="map_displace_round.py's `<out>_detail.json.span` "
                        'sidecar: every candidate with its PAYS/UNPINNED/'
                        'WRONG-UNIT class')
    p.add_argument('--map', default='scripts/target_symbol_map.json')
    p.add_argument('--report', default='build/45410914/report.json',
                   help='current report.json, for refusal criterion (2)')
    p.add_argument('--out-moves')
    p.add_argument('--out-blocks')
    p.add_argument('--out-frag', help='map fragment (VA -> name) to assert')
    p.add_argument('--tiers', default='T_SOLE,T_SPATIAL')
    p.add_argument('--spatial-window', type=lambda s: int(s, 0), default=0x4000)
    p.add_argument('--evidence')
    a = ap.parse_args()

    wt = a.worktree
    tiers = set(a.tiers.split(','))
    fns = load_retail_functions(wt)
    fstarts = [f[0] for f in fns]
    spans = load_spans(wt)
    sstarts = [s[0] for s in spans]
    definers = build_definer_index(wt)

    mp = json.load(open(os.path.join(wt, a.map)))
    name_at = {k.lower(): v for k, v in mp.items() if k.lower().startswith('0x')}

    strict = defaultdict(set)          # unit header -> {strict-100 names}
    rep = os.path.join(wt, a.report)
    if os.path.exists(rep):
        r = json.load(open(rep))
        for u in r['units']:
            for f in u.get('functions', []):
                if f.get('match_percent_normalized') == 100.0:
                    strict[u['name']].add(f['name'])
    # report unit names are obj keys ("default/Spotlight"); index by basename
    strict_by_base = defaultdict(set)
    for u, names in strict.items():
        strict_by_base[u.split('/')[-1]] |= names

    def fn_at(va):
        i = bisect.bisect_right(fstarts, va) - 1
        if i >= 0 and fns[i][0] <= va < fns[i][1]:
            return fns[i]
        return None

    def span_at(va):
        i = bisect.bisect_right(sstarts, va) - 1
        if i >= 0 and spans[i][0] <= va < spans[i][1]:
            return spans[i]
        return None

    # unit header -> sorted list of its pinned ranges
    by_unit = defaultdict(list)
    for s, e, u in spans:
        by_unit[u].append((s, e))

    def unit_distance(header, va):
        best = None
        for s, e in by_unit.get(header, ()):
            d = 0 if s <= va < e else (s - va if va < s else va - e)
            best = d if best is None else min(best, d)
        return best

    stats = Counter()
    moves, blocks, frag, evid = [], defaultdict(list), {}, []
    cands = json.load(open(a.span_detail))

    for c in cands:
        cls = c.get('cls')
        if cls == 'PAYS':
            stats['already-pays'] += 1
            continue
        va = int(c['va'], 16)
        name = c['name']

        # SCOPE: XDK vendor + Quazal band is hard-skipped for this lane.
        if 0x82800000 <= va < 0x82C00000:
            own = span_at(va)
            if own and own[2].startswith('auto_03'):
                stats['skip-out-of-scope'] += 1
                continue

        f = fn_at(va)
        if f is None:
            stats['refuse-no-retail-fn'] += 1
            continue
        if f[0] != va:
            # the VA is interior to a carved function: the map entry cannot be
            # that function, so this is not a splits problem.
            stats['refuse-va-not-fn-start'] += 1
            continue
        start, end = f[0], f[1]

        defs = sorted(definers.get(name, ()))
        if not defs:
            stats['refuse-no-definer'] += 1
            continue

        # ---- claimant choice -------------------------------------------
        # map obj keys ("default/Spotlight") onto splits headers
        cand_headers = []
        for u in by_unit:
            if any(unit_matches(d, u) for d in defs):
                cand_headers.append(u)
        if len(defs) == 1 and len(cand_headers) == 1:
            tier, claim = 'T_SOLE', cand_headers[0]
        elif not cand_headers:
            stats['refuse-definer-has-no-span'] += 1
            continue
        else:
            near = sorted((unit_distance(h, va), h) for h in cand_headers)
            if len(near) > 1 and near[0][0] == near[1][0]:
                stats['refuse-spatial-tie'] += 1
                continue
            if near[0][0] > a.spatial_window:
                stats['refuse-spatial-far'] += 1
                continue
            tier, claim = 'T_SPATIAL', near[0][1]
        if tier not in tiers:
            stats['refuse-tier-' + tier] += 1
            continue

        # ---- refusal criterion (2): claimant already 100% on this name --
        if name in strict_by_base.get(claim.split('/')[-1].replace('.cpp', ''), set()):
            stats['refuse-claimant-already-100-same-name'] += 1
            continue

        # ---- refusal criterion (1): a mapped name in the span the claimant
        #      does not define is a guaranteed regression (objdiff pairs by
        #      NAME).  Walk every retail fn in [start, end).
        bad = None
        i = bisect.bisect_left(fstarts, start)
        n_carved = 0
        while i < len(fns) and fns[i][0] < end:
            n_carved += 1
            other = name_at.get('0x%08x' % fns[i][0])
            if other and other != name:
                od = definers.get(other, ())
                if od and not any(unit_matches(d, claim) for d in od):
                    bad = other
                    break
            i += 1
        if bad:
            stats['refuse-mapped-symbol-claimant-lacks'] += 1
            continue
        if n_carved == 0:
            stats['refuse-nothing-carved'] += 1
            continue

        rec = dict(va=c['va'], name=name, cls=cls, tier=tier, claimant=claim,
                   start='0x%08X' % start, end='0x%08X' % end,
                   n_definers=len(defs), n_carved=n_carved)

        if cls == 'UNPINNED':
            # must not collide with any existing range
            j = bisect.bisect_right(sstarts, start) - 1
            clash = False
            for k in (j, j + 1):
                if 0 <= k < len(spans) and spans[k][0] < end and start < spans[k][1]:
                    clash = True
            if clash:
                stats['refuse-add-overlaps'] += 1
                continue
            blocks[claim].append((start, end))
            stats['ADD-' + tier] += 1
        else:
            own = span_at(va)
            if own is None:
                stats['refuse-owner-vanished'] += 1
                continue
            if not (own[0] <= start and end <= own[1]):
                stats['refuse-span-not-inside-one-donor'] += 1
                continue
            if own[2] == claim:
                stats['refuse-donor-is-claimant'] += 1
                continue
            rec['donor'] = own[2]
            moves.append(dict(donor=own[2], claimant=claim,
                              start=rec['start'], end=rec['end']))
            stats['MOVE-' + tier] += 1
        frag['0x%08x' % va] = name
        evid.append(rec)

    print('joint unblock:', dict(sorted(stats.items(), key=lambda kv: -kv[1])))
    print('  %d MOVE, %d ADD across %d units, %d map assertions'
          % (len(moves), sum(len(v) for v in blocks.values()), len(blocks),
             len(frag)))
    if a.out_moves:
        json.dump(moves, open(a.out_moves, 'w'), indent=1)
    if a.out_blocks:
        json.dump([dict(unit=u, ranges=[['0x%08X' % s, '0x%08X' % e]
                                        for s, e in sorted(v)])
                   for u, v in sorted(blocks.items())],
                  open(a.out_blocks, 'w'), indent=1)
    if a.out_frag:
        json.dump(frag, open(a.out_frag, 'w'), indent=1)
    if a.evidence:
        json.dump(evid, open(a.evidence, 'w'), indent=1)


if __name__ == '__main__':
    main()
