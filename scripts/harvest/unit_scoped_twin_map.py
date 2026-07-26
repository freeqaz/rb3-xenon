#!/usr/bin/env python3
"""Unit-scoped byte-twin symbol-map generator (laneAT, 2026-07-26).

Problem
-------
A pinned target unit's anonymous ``fn_<VA>`` symbols only pair with our compiled
obj through objdiff's *funclet* byte-pairing, which is gated to funclet-shaped
base symbols.  Everything else reads 0% until ``scripts/target_symbol_map.json``
gives the target symbol the MSVC-mangled name our obj defines.

Approach
--------
For every target obj that has a corresponding compiled base obj, index both
sides by objdiff's ``funclet_signature`` (symbol bytes with a 4-byte window
zeroed at every relocation, then canonicalised by stripping trailing all-zero
words that are not covered by a relocation window -- base-side symbols absorb
inter-function alignment padding via distance-to-next-symbol size inference,
dtk-split target symbols do not).  Then propose a name for every sub-100%
anonymous target symbol whose signature appears in the *same unit's* base obj.

Unit scoping is the whole point.  laneAP's binary-wide version drowned in
ubiquitous STL/stereotype shapes (1,101 of its 2,350 hits had a twin in >50
distinct objs and carried no identity evidence).  Restricted to the owning unit
the same signature is strong: measured 730/730 conversion, 0 regressions.

Three passes, decreasing identity confidence:

  1. ``unique``       -- exactly one distinct base name in the unit carries the
                         signature.  Identity-certain.
  2. ``bijection``    -- k targets and exactly k free base names share it;
                         assign i-th target (ascending VA) to i-th base name
                         (symbol-table order).
  3. ``greedy``       -- more free base names than targets.  Every name in a
                         signature group is masked byte-identical, so the match
                         lands whichever we pick, but the *identity* is a guess.
                         Byte-justified, not identity-justified.

Rejections that matter: a base name already mapped at another VA (a duplicate
mangled name inside one unit is a guaranteed regression), and a base name
already consumed by another mapped target symbol in the same unit.

Run to fixpoint -- each applied round frees nothing but consumes names, so the
scan converges in 2 rounds.  Rebuild between rounds (``rm -f
build/45410914/{report.cache,target_symbol_renames.stamp}`` +
``touch config/45410914/config.yml``) so ``report.json`` reflects the new pairs.

Usage
-----
    python3 scripts/harvest/unit_scoped_twin_map.py <worktree> [--pass 1|2|3]
                                                    [--apply] [--json out.json]
"""
import argparse, collections, glob, json, os, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', 'analysis'))
try:
    from coffx import read_coff, infer_sizes, funclet_signature, K_SEC
except ImportError:
    sys.path.insert(0, '/home/free/tmp/laneAM')
    from coffx import read_coff, infer_sizes, funclet_signature, K_SEC


def canon_sigs(path):
    """-> [(canonical_masked_signature, symbol_name, size)] for Code symbols."""
    try:
        data = open(path, 'rb').read()
    except OSError:
        return []
    secs, syms = read_coff(data)
    if secs is None:
        return []
    infer_sizes(secs, syms)
    out = []
    for s in syms:
        if s.sec <= 0 or s.size == 0 or s.kind == K_SEC or s.cls not in (2, 3):
            continue
        sec = secs[s.sec - 1]
        if not sec.is_code:
            continue
        g = funclet_signature(sec, s)
        if g is None:
            continue
        lo, hi = s.value, s.value + s.size
        rend = 0
        for (va, si, typ) in sec.relocs:
            if lo <= va < hi:
                rend = max(rend, va - lo + 4)
        end = len(g)
        while end >= 4 and g[end - 4:end] == b'\0\0\0\0' and rend <= end - 4:
            end -= 4
        if end:
            out.append((g[:end], s.name, s.size))
    return out


def scan(wt, mode):
    map_path = os.path.join(wt, 'scripts/target_symbol_map.json')
    raw = json.load(open(map_path))
    tmap = {k.lower(): v for k, v in raw.items()
            if isinstance(v, str) and k.startswith('0x')}
    all_names = set(tmap.values())
    rep = json.load(open(os.path.join(wt, 'build/45410914/report.json')))
    pct = {f['name']: f['match_percent_normalized']
           for u in rep['units'] for f in (u.get('functions') or [])}

    base_by_name = collections.defaultdict(list)
    for p in glob.glob(os.path.join(wt, 'build/45410914/src/**/*.obj'),
                       recursive=True):
        base_by_name[os.path.basename(p)].append(p)

    stats = collections.Counter()
    out = []
    tgt_root = os.path.join(wt, 'build/45410914/obj')
    for tp in sorted(glob.glob(os.path.join(tgt_root, '**', '*.obj'),
                               recursive=True)):
        rel = os.path.relpath(tp, tgt_root)
        bp = os.path.join(wt, 'build/45410914/src', rel)
        if not os.path.exists(bp):
            cand = base_by_name.get(os.path.basename(tp))
            if not cand or len(cand) != 1:
                stats['no_base_obj'] += 1
                continue
            bp = cand[0]
        tsigs, bsigs = canon_sigs(tp), canon_sigs(bp)
        if not tsigs or not bsigs:
            continue
        bgroup = collections.defaultdict(list)
        for sig, nm, _ in bsigs:
            bgroup[sig].append(nm)
        tgroup = collections.defaultdict(list)
        used = set()
        for sig, nm, sz in tsigs:
            if not nm.startswith('fn_'):
                continue
            mapped = tmap.get('0x' + nm[3:].lower())
            if mapped:
                used.add(mapped)
                continue
            p = pct.get(nm)
            if p is None or p >= 100.0:
                continue
            tgroup[sig].append((int(nm[3:], 16), nm, sz))
        for sig, tl in tgroup.items():
            names = bgroup.get(sig)
            if not names:
                stats['no_twin'] += len(tl)
                continue
            # NOTE: the name filter is UNIT-SCOPED on purpose. The target
            # symbol renamer runs per-target-obj, so the same mangled name may
            # legitimately be claimed in two different units; only a duplicate
            # WITHIN one unit is a guaranteed regression. laneAT-p1 measured
            # that the older global filter was rejecting real matches.
            seen, free = set(), []
            for n in names:
                if n in used or n in seen:
                    continue
                seen.add(n)
                free.append(n)
            if not free:
                stats['no_free_base'] += 1
                continue
            distinct = len(set(names))
            if mode == 1 and (distinct != 1 or len(free) != 1 or len(tl) != 1):
                continue
            if mode == 2 and len(tl) != len(free):
                continue
            tl.sort()
            k = min(len(tl), len(free))
            kind = ('unique' if distinct == 1 and len(tl) == 1 else
                    'bijection' if len(tl) == len(free) else
                    'greedy_base_surplus' if len(free) > len(tl) else
                    'greedy_tgt_surplus')
            stats['ACCEPT'] += k
            stats['grp_' + kind] += 1
            for (va, nm, sz), bn in zip(tl[:k], free[:k]):
                out.append({'va': '0x%08x' % va, 'sym': nm, 'size': sz,
                            'unit': rel, 'base_name': bn, 'kind': kind,
                            'ntgt': len(tl), 'nfree': len(free)})
    return stats, out, map_path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('worktree')
    ap.add_argument('--pass', dest='mode', type=int, default=3, choices=(1, 2, 3))
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('--json')
    a = ap.parse_args()
    stats, out, map_path = scan(a.worktree, a.mode)
    print(json.dumps(stats, indent=1))
    print('proposed %d entries' % len(out))
    print(json.dumps(dict(collections.Counter(r['kind'] for r in out)), indent=1))
    if a.json:
        json.dump(out, open(a.json, 'w'), indent=1)
    if a.apply:
        # Append preserving the file's existing key order and formatting -- a
        # re-serialise with sort_keys churns all ~25k lines and makes the map
        # unmergeable against other lanes' concurrent edits.
        text = open(map_path).read()
        m = json.loads(text)
        add = {r['va']: r['base_name'] for r in out if r['va'] not in m}
        body = text.rstrip()
        assert body.endswith('}')
        chunks = [body[:-1].rstrip()]
        for k, v in add.items():
            chunks.append(',\n ' + json.dumps(k) + ': ' + json.dumps(v))
        chunks.append('\n}')
        merged = ''.join(chunks)
        assert json.loads(merged) == {**m, **add}
        open(map_path, 'w').write(merged)
        print('applied %d new entries -> %s' % (len(add), map_path))


if __name__ == '__main__':
    main()
