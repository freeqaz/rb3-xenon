#!/usr/bin/env python3
"""Cycle-aware repair of mispaired entries in scripts/target_symbol_map.json.

Background
----------
`multi_content_disambiguate.py --trust-audit` showed that 423 already-mapped
names sit on a retail VA whose *content* contradicts them: e.g.
`?StaticClassName@Sfx@@` is mapped to `0x826fc3e8`, and that VA references the
literal `"FxSendCompress"`.  The family is overwhelmingly the
`StaticClassName` / `Type()` / `DECLARE_MESSAGE` boilerplate, whose bodies are
`return Symbol("literal")` -- byte-identical apart from the string pointer.
objdiff's normalized diff masks relocations, so a mispair reads a clean 100%
and is invisible in the score.

Repairing them is a **permutation, not a lookup**: name A holds B's VA, B holds
C's, C holds A's.  A partial repair strands matches, and
`tu5_map_apply_fragment.py` (rightly) asserts on any addr/name collision, so it
cannot express a mid-rotation state.  This tool computes the *whole* assignment
first and writes it in one pass.

What it does
------------
1. **analyze** -- for every function we compile that the homing scan gave
   byte-identical retail hits for, run the map-free content resolver over
   *all* hits (never just the currently-mapped one) and record the winner.
   That yields `desired[name] = va` far beyond the 95 entries the lane-G audit
   published, which is what makes cycles closable.
2. **plan** -- turn `desired` into a concrete, conflict-free rewrite:
     * `set`      va -> name   (name's content-determined home)
     * `remove`   va           (vacated, nothing proven to live there)
   Names whose desired VA is held by a name that has *no* proven home of its
   own are refused unless `--evict` (the holder would be silently dropped and
   its -- itself bogus -- match lost).
3. **apply** -- textual, line-oriented rewrite of the map.  NEVER json.dump:
   the file is a ~21.6k-line 1-space-indent map whose formatting is a project
   invariant.  Existing untouched lines stay byte-identical.

Key comparison is always case-insensitive: 264 legacy keys are uppercase
`"0X..."`.

Usage
-----
    map_rotation_repair.py analyze --results merged.json --worktree WT \
        --out desired.json
    map_rotation_repair.py plan --desired desired.json --worktree WT \
        --only-names names.txt --out plan.json
    map_rotation_repair.py apply --plan plan.json --map scripts/target_symbol_map.json
"""
import argparse
import json
import os
import re
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from multi_content_disambiguate import Band, func_table, load_tmap, evaluate  # noqa: E402


# ------------------------------------------------------------------ analyze
def analyze(a):
    wt = a.worktree
    band = Band(a.band or os.path.join(wt, 'orig/45410914/band.exe'))
    tmap, name2va = load_tmap(a.map or os.path.join(wt, 'scripts/target_symbol_map.json'))
    res = json.load(open(a.results))

    picks = defaultdict(set)       # name -> set(winner va)
    verdicts = defaultdict(set)    # name -> set(verdict)
    seen_hits = defaultdict(set)   # name -> set(all hit vas)
    for tu, recs in sorted(res.items()):
        if not isinstance(recs, list):
            continue
        todo = [r for r in recs if r.get('hits')]
        if not todo:
            continue
        obj = os.path.join(wt, 'build/45410914/src', tu + '.obj')
        if not os.path.exists(obj):
            continue
        ft = func_table(obj)
        for r in todo:
            fn = ft.get(r['name'])
            if fn is None or fn['size'] != r['size']:
                continue
            hits = [int(h, 16) for h in r['hits']]
            seen_hits[r['name']].update(hits)
            # map-free evidence only: `sym` evidence inherits the map's own
            # mispairs, which is exactly what we are repairing.
            v, w, _ = evaluate(band, tmap, name2va, fn, hits, use_sym=False)
            verdicts[r['name']].add(v)
            if v == 'RESOLVED-STRONG':
                picks[r['name']].add(w)

    out = {}
    stats = defaultdict(int)
    for name, ws in sorted(picks.items()):
        if len(ws) > 1:
            stats['MULTI-TU-DISAGREE'] += 1
            continue
        w = next(iter(ws))
        cur = sorted(name2va.get(name, ()))
        out[name] = dict(desired='0x%08x' % w,
                         cur=['0x%08x' % v for v in cur],
                         agrees=bool(cur and w in set(cur)))
        stats['AGREE' if out[name]['agrees'] else 'MISPAIR'] += 1
    stats['names-with-hits'] = len(seen_hits)
    stats['resolved'] = len(out)
    json.dump(out, open(a.out, 'w'), indent=1)
    print('analyze:', dict(sorted(stats.items(), key=lambda kv: -kv[1])))
    print('->', a.out)


# --------------------------------------------------------------------- plan
def plan(a):
    wt = a.worktree
    mp = a.map or os.path.join(wt, 'scripts/target_symbol_map.json')
    raw = {k: v for k, v in json.load(open(mp)).items() if isinstance(v, str)}
    va2name = {k.lower(): v for k, v in raw.items()}
    name2va = defaultdict(set)
    for k, v in va2name.items():
        name2va[v].add(k)

    des = json.load(open(a.desired))
    only = None
    if a.only_names:
        only = {l.strip() for l in open(a.only_names) if l.strip()}

    movers = {}
    for name, d in des.items():
        if d['agrees']:
            continue
        if only is not None and name not in only:
            continue
        if name not in name2va:
            continue                       # not currently mapped; not a repair
        if len(name2va[name]) != 1:
            continue                       # name mapped at several VAs: refuse
        movers[name] = (sorted(name2va[name])[0], d['desired'].lower())

    # every VA a mover vacates, and every VA a mover wants
    want = defaultdict(list)
    for n, (cur, dst) in movers.items():
        want[dst].append(n)
    contested = {v: ns for v, ns in want.items() if len(ns) > 1}
    for v in contested:
        for n in want[v]:
            movers.pop(n, None)

    # A destination held by a name that is NOT itself moving out is BLOCKED.
    # Dropping a blocked mover can block another mover that was relying on it
    # to vacate, so iterate to a fixpoint.
    blocked = {}
    while True:
        newly = {}
        for n, (cur, dst) in movers.items():
            holder = va2name.get(dst)
            if holder is None or holder == n or holder in movers:
                continue                   # free, or holder moves out (cycle closes)
            newly[n] = holder
        if not newly:
            break
        blocked.update(newly)
        if a.evict:
            break
        for n in newly:
            movers.pop(n)

    # vacated must be computed AFTER the blocked fixpoint: a mover that was
    # dropped does not vacate anything.
    vacated = {cur for cur, _ in movers.values()}

    # ---- final assignment
    setmap = {}
    for n, (cur, dst) in movers.items():
        setmap[dst] = n
    remove = sorted(v for v in vacated if v not in setmap)
    # a removal that is also somebody's destination must not be removed
    evicted = sorted({va2name[d] for d in setmap
                      if d in va2name and va2name[d] not in movers})

    # ---- cycle decomposition (reporting / audit trail)
    nxt = {cur: dst for cur, dst in movers.values()}
    cycles, chains, seen = [], [], set()
    for start in nxt:
        if start in seen:
            continue
        path, v = [], start
        while v in nxt and v not in path:
            path.append(v)
            v = nxt[v]
        if v in path:                      # closed cycle
            i = path.index(v)
            cyc = path[i:]
            if not (set(cyc) & seen):
                cycles.append(cyc)
            seen.update(path)
        else:
            chains.append(path + [v])
            seen.update(path)

    p = dict(set={d: n for d, n in sorted(setmap.items())},
             remove=remove,
             evicted=evicted,
             blocked={k: v for k, v in sorted(blocked.items())},
             contested={k: v for k, v in sorted(contested.items())},
             cycles=[[('0x%s' % c[2:]) for c in cyc] for cyc in cycles],
             chains=[[('0x%s' % c[2:]) for c in ch] for ch in chains])
    json.dump(p, open(a.out, 'w'), indent=1)
    print('plan: %d moves (%d set, %d remove), %d closed cycles, %d chains, '
          '%d blocked, %d contested, %d evicted'
          % (len(movers), len(setmap), len(remove), len(cycles), len(chains),
             len(blocked), len(contested), len(evicted)))
    print('->', a.out)


# -------------------------------------------------------------------- apply
def apply(a):
    plan_ = json.load(open(a.plan))
    setmap = {k.lower(): v for k, v in plan_['set'].items()}
    remove = {v.lower() for v in plan_['remove']}
    assert not (set(setmap) & remove), 'plan sets and removes the same VA'

    text = open(a.map).read()
    lines = text.split('\n')
    assert lines[0] == '{', lines[0]

    cur = {k.lower(): v for k, v in json.load(open(a.map)).items()
           if isinstance(v, str)}
    # post-state sanity: no name may end up on two VAs, no VA on two names
    post = dict(cur)
    for v in remove:
        post.pop(v, None)
    for v, n in setmap.items():
        post[v] = n
    dupe = defaultdict(list)
    for v, n in post.items():
        dupe[n].append(v)
    newdupes = [n for n, vs in dupe.items()
                if len(vs) > 1 and len({x for x in cur if cur[x] == n}) < len(vs)]
    assert not newdupes, 'apply would duplicate names: %s' % newdupes[:5]

    out = [lines[0]]
    done = set()
    nset = nrem = 0
    # ---- A map ENTRY line is  "0xVA": "name",  -- it has a colon after the
    # closing quote.  The map ALSO contains provenance ARRAYS (`_denylist`,
    # `_icf_arbitrary`, `_bijection_arbitrary` = 1,207 entries) whose elements
    # are bare `    "0xVA",` strings.  A plain startswith('"0x') test matches
    # those too, and then this writer either silently DELETES an array element
    # (on `remove`) or rewrites it into a `"key": "value"` pair *inside a JSON
    # array* (on `set`) -- which is a hard parse error.  Measured here: applying
    # a 3-entry plan corrupted the map at line 1381 because `0x82754a48` is also
    # listed in `_bijection_arbitrary`.  The safety asserts above cannot catch
    # it: they build `cur` from `json.load(...)` filtered by
    # `isinstance(v, str)`, so the arrays are invisible to the checker and
    # visible only to this textual writer.  Require the colon.
    ENTRY = re.compile(r'^"0[xX][0-9a-fA-F]+"\s*:')
    for ln in lines[1:]:
        s = ln.strip()
        if ENTRY.match(s):
            key = s.split('"')[1].lower()
            if key in remove:
                nrem += 1
                continue
            if key in setmap:
                out.append(' "%s": %s,' % (s.split('"')[1],
                                           json.dumps(setmap[key])))
                done.add(key)
                nset += 1
                continue
        out.append(ln)
    ins = []
    for k, v in sorted(setmap.items()):
        if k not in done:
            ins.append(' "%s": %s,' % (k, json.dumps(v)))
            nset += 1
    out = [out[0]] + ins + out[1:]
    open(a.map, 'w').write('\n'.join(out))
    json.load(open(a.map))          # must still parse
    print('applied: %d set, %d removed -> %s' % (nset, nrem, a.map))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)

    an = sub.add_parser('analyze')
    an.add_argument('--results', required=True)
    an.add_argument('--worktree', required=True)
    an.add_argument('--band')
    an.add_argument('--map')
    an.add_argument('--out', required=True)
    an.set_defaults(fn=analyze)

    pl = sub.add_parser('plan')
    pl.add_argument('--desired', required=True)
    pl.add_argument('--worktree', required=True)
    pl.add_argument('--map')
    pl.add_argument('--only-names')
    pl.add_argument('--evict', action='store_true',
                    help='allow moving onto a VA held by a name with no proven '
                         'home of its own (that holder is dropped from the map)')
    pl.add_argument('--out', required=True)
    pl.set_defaults(fn=plan)

    apl = sub.add_parser('apply')
    apl.add_argument('--plan', required=True)
    apl.add_argument('--map', required=True)
    apl.set_defaults(fn=apply)

    a = ap.parse_args()
    a.fn(a)


if __name__ == '__main__':
    main()
