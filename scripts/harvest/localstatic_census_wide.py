#!/usr/bin/env python3
"""WIDENED local-static census (laneAX-W1, 2026-07-27).

> **SUPERSEDED 2026-07-27 (laneAY) -- USE `localstatic_census_v2.py`.**
> This tool enumerates its universe by globbing `build/45410914/obj/**/*.obj`,
> a directory that is NEVER CLEANED. Measured at 39,266: 9,911 of its 13,932
> reported excess statics (71%) came from objs no live unit owns -- 8,891
> `auto_*` dtk carves plus **112 STALE objs orphaned by earlier splits.txt
> generations**. Worse, its `max(variants)` ranking actively PREFERRED the
> stale obj: its #1 "actionable" row was `band3/game/VocalPlayer` (46 statics)
> -- a dead carve, mis-attributed to `Poll`, whose live counterpart
> `default/VocalPlayer ?Handle@...` is 46/46 = ZERO excess. Its `pct` join
> (`u['name'].split('/', 1)[-1]`) also guesses at a nesting the two trees do
> not share. `localstatic_census_v2.py` keeps this resolver verbatim and
> enumerates from `objdiff.json` instead (name / target_path / base_path).

scripts/harvest/localstatic_population_scan.py reports 102 functions / 58 TUs /
403 excess init calls. That is a FLOOR, not the population: reading its code, it
discards rows on four separate grounds --

  1. `if nm.startswith('fn_') ... continue`  -> ANONYMOUS target symbols are
     dropped entirely, so funclets and unnamed carriers are invisible.
  2. `cb = bc.get(nm); if cb is None: continue` -> a function present in the
     TARGET but absent (or differently named) in our obj contributes NOTHING,
     when in fact its whole target-side count is excess.
  3. `p = pct.get(...); if p is None ... continue` -> no report.json pairing,
     no row.
  4. per-function only -> a TU whose carriers are all anonymous/unpaired scores
     ZERO and never ranks.

...and it also OVER-fires, because it counts RAW `??0Symbol@@QAA@PBD@Z` /
`??0Message@@QAA@VSymbol@@@Z` / `atexit` relocations. A plain `Symbol s(str)`
temporary, or a Symbol built from a runtime MakeString, is indistinguishable
from a function-local static in that metric.

This census fixes both directions at once using the GUARD-VERIFIED,
STRING-RESOLVED site list from localstatic_patch_gen.py, computed SYMMETRICALLY
on the target obj and on our compiled obj:

  * recall  -- every code symbol on the target side is scanned, including fn_*,
    funclets and unpaired ones; results aggregate to a TU total that needs no
    report.json and no name pairing at all.
  * precision -- a site only counts if the MSVC guard-bit test/set wraps it,
    which is what a function-local static is and a temporary is not.

★ TWO THINGS THIS TOOL REFUTED ABOUT ITS OWN BRIEF
  a) "count $S / ??_B / ??__E / ??__F COMDAT sections target-vs-base" does NOT
     work here. splits.txt pins only .text, so guard words (.data) and the
     init/atexit thunks are simply NOT PRESENT as named symbols in a dtk target
     obj -- measured: TrackPanelDirBase target has {'??__E': 1} and BandTrack
     target has {} against {'??_B': 4, '??__F': 3} / {'??_B': 2, '??__F': 10}
     on our side. The pairing-free structural signal that DOES work is the
     GUARD WORD ITSELF: each distinct (guard VA, guard bit) in the target obj
     is exactly one function-local static, visible with no symbol names at all.
  b) `auto_NN_<VA>_text` target objs are dtk's leftover carve of UNPINNED
     .text. They have no source file, so their excess is real but not
     actionable; they are reported separately, never in the ranked list.

Usage: python3 scripts/harvest/localstatic_census_wide.py <worktree> [--json out]
"""
import argparse, collections, glob, json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import localstatic_patch_gen as G

# TUs owned by other lanes -- marked, and excluded from the actionable list.
LANE_AT_OWNED = {
    'BandTrack', 'SigninScreen', 'TrackPanelDirBase', 'PatchDir', 'TrackPanel',
    'ChordbookPanel', 'RGTrainerPanel', 'BandSongMetadata', 'NewAwardPanel',
    'SongSelectPanel', 'BandStarDisplay',
}
LANE_AX_INFLIGHT = {
    'VocalPlayer', 'AccomplishmentSongConditional', 'BandList', 'GemTrackDir',
    'MusicLibrary', 'BandWardrobe', 'ContentMgr',
}

# Container/allocator template instantiations. None of these can legitimately
# declare a function-local static Symbol naming a game property -- when the
# target symbol under one of these names carries a coherent property list, the
# name is a target_symbol_map mispair and the strings identify the real owner.
# MSVC derives the ?A0x<hash> of an anonymous namespace from the machine name
# and source path, so ours never equals retail's. objdiff pairs across it
# (ContextChecker's ?InternalCheckContext@?A0x1e5d0754@@ vs our
# ?InternalCheckContext@?A0xb844f63d@@ is a 100.00% match), but a name-keyed
# base lookup does NOT -- which invented 20 phantom "missing" statics in a
# function whose source already has all 20. Key on the normalised name.
ANONNS_RE = re.compile(r'\?A0x[0-9a-fA-F]+@')


def norm(name):
    return ANONNS_RE.sub('?A@', name)


STL_RE = re.compile(r'@stlpmtx_std@@|_M_[a-z]|_Copy_Construct|_Destroy|'
                    r'@\?\$vector@|@\?\$_Rb_tree@|@\?\$list@|@\?\$map@|'
                    r'@\?\$deque@|@\?\$set@|@\?\$_?[Ss]tring|'
                    r'@\?\$ObjPtr|@\?\$ObjVector|@\?\$ObjOwnerPtr|'
                    r'@\?\$ObjPtrList|@\?\$ObjDirPtr')
ANON_RE = re.compile(r'^(fn_|__unwind\$|__catch\$|\?\?__|\$)')


def is_auto(unit):
    return os.path.basename(unit).startswith('auto_')


def base_index(wt):
    by_rel, by_name = {}, collections.defaultdict(list)
    root = os.path.join(wt, 'build/45410914/src')
    for p in glob.glob(os.path.join(root, '**', '*.obj'), recursive=True):
        by_rel[os.path.relpath(p, root)] = p
        by_name[os.path.basename(p)].append(p)
    return by_rel, by_name


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('worktree')
    ap.add_argument('--json')
    ap.add_argument('--top', type=int, default=45)
    a = ap.parse_args()
    wt = a.worktree

    img = G.Image(os.path.join(wt, 'orig/45410914/band.exe'))
    _tmap, rev = G.load_target_map(wt)
    tellname = G.build_tellname(rev)
    by_rel, by_name = base_index(wt)

    pct = {}
    for u in json.load(open(os.path.join(wt, 'build/45410914/report.json')))['units']:
        for f in (u.get('functions') or []):
            pct[(u['name'].split('/', 1)[-1], f['name'])] = \
                f['match_percent_normalized']

    troot = os.path.join(wt, 'build/45410914/obj')
    tu_rows, fn_rows = [], []
    strings_of = collections.defaultdict(set)      # real units only

    for tp in sorted(glob.glob(os.path.join(troot, '**', '*.obj'), recursive=True)):
        rel = os.path.relpath(tp, troot)
        unit = rel[:-4]
        bp = by_rel.get(rel)
        if bp is None:
            c = by_name.get(os.path.basename(tp))
            bp = c[0] if c and len(c) == 1 else None

        tgt = G.scan_obj(tp, img, tellname)
        if not tgt:
            continue
        bsz_raw = {}
        base_raw = G.scan_obj_base(bp, bsz_raw) if bp else None
        # normalise anon-namespace hashes on the base side so name lookups
        # survive the ?A0x<hash> mismatch objdiff already pairs across
        bsizes = {norm(k): v for k, v in bsz_raw.items()}
        base = None if base_raw is None else {norm(k): v for k, v in base_raw.items()}

        tn = tb = 0
        named_excess = 0
        for name, (size, sites, _ord) in tgt.items():
            ls = [s for s in sites if s['form'] == 'LOCAL_STATIC']
            if not ls:
                continue
            tn += len(ls)
            if not is_auto(unit):
                strings_of[norm(name)].add(
                    tuple(s['string'] for s in ls if s['string']))
            bl = (base or {}).get(norm(name)) or []
            tb += len(bl)
            excess = len(ls) - len(bl)
            if excess <= 0:
                continue
            anonymous = bool(ANON_RE.match(name))
            p = pct.get((unit, name))
            labels = []
            if base is None:
                labels.append('NO_BASE_OBJ')
            elif norm(name) not in bsizes:
                # the symbol does not exist in our compiled obj AT ALL (the old
                # scan skipped these outright; their whole target count is
                # excess)
                labels.append('UNPAIRED_BASE')
            if anonymous:
                labels.append('ANON_TARGET')
            else:
                named_excess += excess
            if p is None:
                labels.append('NO_REPORT_PAIRING')
            if STL_RE.search(name):
                labels.append('MISPAIR_STL')
            bsz = bsizes.get(norm(name))
            if bsz and (size > 3 * bsz or bsz > 3 * size):
                labels.append('MISPAIR_SIZE')
            if any(s['string'] is None for s in ls):
                labels.append('NO_STRING')
            fn_rows.append({
                'unit': unit, 'sym': name, 'pct': p, 'tgt': len(ls),
                'base': len(bl), 'excess': excess, 'size': size,
                'auto': is_auto(unit), 'anon': anonymous, 'labels': labels,
                'statics': [{'kind': s['kind'], 'arity': s.get('arity'),
                             'string': s['string'], 'off': s['off'],
                             'guard_va': s['guard_va'],
                             'guard_bit': s['guard_bit']} for s in ls],
            })
        if tn:
            tu_rows.append({'unit': unit, 'tgt': tn, 'base': tb,
                            'excess': tn - tb, 'paired': bp is not None,
                            'auto': is_auto(unit), 'named_excess': named_excess})

    # cross-obj mispair: the same mangled name resolving to a DIFFERENT string
    # SEQUENCE in two different real target objs means at least one of the two
    # VAs the map assigns that name is wrong. (Comparing sequences, not the
    # union of strings -- comparing the union flags every function that simply
    # has two statics.)
    xobj = {n for n, seqs in strings_of.items() if len(seqs) > 1}
    for r in fn_rows:
        if not r['auto'] and norm(r['sym']) in xobj:
            r['labels'].append('MISPAIR_XOBJ')

    # what the EXISTING per-function scan sees (its TU set), for the delta
    try:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import localstatic_population_scan as OLD           # noqa: F401
    except Exception:
        pass
    old_units = set()
    old_json = os.environ.get('LOCALSTATIC_OLD_JSON')
    if old_json and os.path.exists(old_json):
        od = json.load(open(old_json))
        old_units = {t['unit'][:-4] for t in od['per_tu']}
        print('== reference floor (localstatic_population_scan.py) ==')
        print('   %d functions / %d TUs / %d excess init calls\n'
              % (len(od['rows']), len(od['per_tu']),
                 sum(t['excess'] for t in od['per_tu'])))

    real = [t for t in tu_rows if not t['auto']]
    auto = [t for t in tu_rows if t['auto']]
    real_pos = [t for t in real if t['excess'] > 0]
    real_paired = [t for t in real_pos if t['paired']]

    print('== A) TU-LEVEL AGGREGATE (pairing-free, guard-verified) ==')
    print('   real TUs with target-side local statics : %d' % len(real))
    print('   real TUs with NET EXCESS                : %d  (%d statics)'
          % (len(real_pos), sum(t['excess'] for t in real_pos)))
    print('   ...that have a compiled base obj        : %d  (%d statics)'
          % (len(real_paired), sum(t['excess'] for t in real_paired)))
    print('   auto_* carve TUs (no source file)       : %d  (%d statics) '
          '-- NOT ACTIONABLE'
          % (len([t for t in auto if t['excess'] > 0]),
             sum(t['excess'] for t in auto if t['excess'] > 0)))
    if old_units:
        silent = [t for t in real_paired if t['unit'] not in old_units]
        print('   ★ real TUs with TU-level excess that score ZERO in the')
        print('     existing per-function census        : %d  (%d statics)'
              % (len(silent), sum(t['excess'] for t in silent)))

    # string-resolution honesty: the overall stringless rate is dominated by
    # auto_* carve objs, whose dtk symbol BOUNDS are unreliable (an over-carved
    # symbol swallows the next function, so relocations from code that isn't
    # really in it get attributed to it). In real pinned TUs the resolver is
    # near-perfect, which is what matters -- those are the only editable rows.
    res = collections.Counter()
    for r in fn_rows:
        k = ('auto' if r['auto'] else 'real', 'anon' if r['anon'] else 'named')
        res[k + ('t',)] += len(r['statics'])
        res[k + ('n',)] += sum(1 for s in r['statics'] if s['string'] is None)
    print('\n   string resolution (excess rows only):')
    for k in sorted({(a, b) for a, b, _ in res}):
        t, n = res[k + ('t',)], res[k + ('n',)]
        print('     %-5s %-6s %5d statics, %4d unresolved (%.1f%%)'
              % (k[0], k[1], t, n, 100.0 * n / max(t, 1)))

    rf = [r for r in fn_rows if not r['auto']]
    print('\n== B) PER-FUNCTION (UNPAIRED_BASE scored as 0), real TUs ==')
    print('   functions with excess : %d across %d TUs, %d excess statics'
          % (len(rf), len({r['unit'] for r in rf}), sum(r['excess'] for r in rf)))
    for k, v in collections.Counter(l for r in rf for l in r['labels']).most_common():
        print('     %-20s %d' % (k, v))

    bad = {'MISPAIR_STL', 'MISPAIR_XOBJ', 'MISPAIR_SIZE', 'NO_STRING'}
    clean = [r for r in rf if not (bad & set(r['labels']))]
    killed = [r for r in rf if (bad & set(r['labels']))]
    named = [r for r in clean if not r['anon']]
    print('\n== C) PRECISION FILTER (drop MISPAIR_STL/XOBJ/SIZE + NO_STRING) ==')
    print('   killed %d rows (%d excess statics)'
          % (len(killed), sum(r['excess'] for r in killed)))
    print('   surviving : %d functions / %d TUs / %d excess statics'
          % (len(clean), len({r['unit'] for r in clean}),
             sum(r['excess'] for r in clean)))
    print('   ...of which NAMED (directly editable): %d functions / %d TUs / '
          '%d excess' % (len(named), len({r['unit'] for r in named}),
                         sum(r['excess'] for r in named)))

    print('\n== RANKED ACTIONABLE TUs ==')
    print('   named+guard-verified+string-resolved excess, base obj present')
    print('   [AT]=laneAT-owned  [AX]=laneAX in-flight  (both EXCLUDED)')
    # A source file can be pinned in TWO target objs (a flat `Foo.obj` and a
    # path-qualified `band3/game/Foo.obj`); both fall back to the same compiled
    # base obj, so ranking by target-obj path double-counts the TU. Key the
    # ranking by the source stem and keep the max, not the sum.
    per_tu = collections.defaultdict(dict)
    for r in named:
        stem = os.path.basename(r['unit'])
        d = per_tu[stem].setdefault(r['unit'], {'n': 0, 'f': 0})
        d['n'] += r['excess']
        d['f'] += 1
    per_tu_fns = {}
    flat = {}
    for stem, variants in per_tu.items():
        best = max(variants.items(), key=lambda kv: kv[1]['n'])
        flat[best[0]] = best[1]['n']
        per_tu_fns[best[0]] = best[1]['f']
    per_tu = collections.Counter(flat)
    shown = 0
    for u, n in per_tu.most_common():
        stem = os.path.basename(u)
        mark = '[AT]' if stem in LANE_AT_OWNED else \
               ('[AX]' if stem in LANE_AX_INFLIGHT else '    ')
        excl = mark.strip() != ''
        tag = 'EXCLUDED' if excl else ''
        print('   %s %4d statics  %2d fns  %-9s %s' % (mark, n, per_tu_fns[u],
                                                       tag, u))
        shown += 1
        if shown >= a.top:
            break
    tot_act = sum(n for u, n in per_tu.items()
                  if os.path.basename(u) not in LANE_AT_OWNED
                  and os.path.basename(u) not in LANE_AX_INFLIGHT)
    print('\n   ACTIONABLE TOTAL (excluding both owned sets): %d statics '
          'across %d TUs'
          % (tot_act, len([u for u in per_tu
                           if os.path.basename(u) not in LANE_AT_OWNED
                           and os.path.basename(u) not in LANE_AX_INFLIGHT])))

    if a.json:
        json.dump({'tu': tu_rows, 'fn': fn_rows}, open(a.json, 'w'), indent=1)


if __name__ == '__main__':
    main()
