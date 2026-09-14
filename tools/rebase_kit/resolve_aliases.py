#!/usr/bin/env python3
"""Three-way merge for scripts/symbol_aliases.json during a lane rebase.

Usage:  python3 tools/rebase_kit/resolve_aliases.py <worktree>
            (inside a stopped rebase: ours=:2:, base=REBASE_HEAD~1, theirs=REBASE_HEAD)
        python3 tools/rebase_kit/resolve_aliases.py <repo> --test OURS BASE THEIRS EXPECT
            (replay; compare against EXPECT ref, print differing groups)

⚠ --test exits 1 on ANY difference from EXPECT, INCLUDING an `evidence`-string-only
difference, which is not a merge defect. Do not use its exit code as a pass/fail
gate -- call three_way() and compare STRUCTURALLY. tools/rebase_kit/test_rebase_kit.py
does exactly that; see README.md.

⚠ Round-trip spelling is load-bearing: indent=1, and ensure_ascii is DETECTED from
the HEAD file rather than assumed (encoding_of).

v2 (2026-09-14): v1 replaced a both-changed group with the lane's copy and treated a survivor rename as
delete+add, which dropped main's additions to two groups on the W16-J landing (-3,728 B). Now: groups are
paired by composite key, unpaired base/theirs groups are re-paired by unique address (a rekey), and a group
changed on both sides is merged field by field; anything it cannot merge is a hard stop (exit 3), never a guess.
"""
import json, subprocess, sys, os

P = 'scripts/symbol_aliases.json'
KEYF = ('name', 'address', 'survivor')

def show(spec): return json.loads(subprocess.check_output(['git', 'show', spec]))
def key(g): return tuple(g.get(f) for f in KEYF)
def canon(x): return json.dumps(x, sort_keys=True)

def merge_list(ov, bv, tv, where):
    bset = {canon(x) for x in bv}; tset = {canon(x) for x in tv}
    removed = bset - tset; added = [x for x in tv if canon(x) not in bset]
    out = [x for x in ov if canon(x) not in removed]
    have = {canon(x) for x in out}
    for x in added:
        if canon(x) not in have: out.append(x); have.add(canon(x))
    return out

def merge_field(f, ov, bv, tv, where):
    if tv == bv: return ov
    if ov == bv or ov == tv: return tv
    if isinstance(ov, list) and isinstance(bv, list) and isinstance(tv, list):
        return merge_list(ov, bv, tv, where)
    if isinstance(ov, str) and isinstance(bv, str) and isinstance(tv, str) and ov.startswith(bv) and tv.startswith(bv):
        return ov + tv[len(bv):]          # both appended a note to the same base text
    print(f'CONFLICT {where}: field {f!r} changed on both sides and is not mergeable\n  base={bv!r}\n  ours={ov!r}\n  theirs={tv!r}')
    sys.exit(3)

def merge_group(og, bg, tg, where):
    out = {}
    for f in list(og) + [f for f in bg if f not in og] + [f for f in tg if f not in og and f not in bg]:
        v = merge_field(f, og.get(f), bg.get(f), tg.get(f), where)
        if v is not None or f in og or f in tg: out[f] = v
    return {k: v for k, v in out.items() if not (v is None and k not in tg and k not in og)}

def three_way(ours, base, theirs):
    bmap = {key(g): g for g in base['groups']}; tmap = {key(g): g for g in theirs['groups']}
    assert len(bmap) == len(base['groups']) and len(tmap) == len(theirs['groups']), "non-unique group keys (name,address,survivor)"
    # pair base<->theirs: same key, else rekey by unique address
    b_only = [k for k in bmap if k not in tmap]; t_only = [k for k in tmap if k not in bmap]
    def by_addr(ks): 
        d = {}
        for k in ks: d.setdefault(k[1], []).append(k)
        return d
    ba, ta = by_addr(b_only), by_addr(t_only)
    rekey = {}
    for a, ks in ba.items():
        if a is not None and len(ks) == 1 and len(ta.get(a, [])) == 1: rekey[ks[0]] = ta[a][0]
    pairs = [(k, k) for k in bmap if k in tmap and bmap[k] != tmap[k]]
    pairs += [(bk, tk) for bk, tk in rekey.items()]
    deletes = [k for k in b_only if k not in rekey]
    adds = [k for k in t_only if k not in set(rekey.values())]
    omap = {key(g): i for i, g in enumerate(ours['groups'])}
    n = 0
    def find_ours(bk):
        if bk in omap: return omap[bk]
        a = bk[1]
        hits = [i for i, g in enumerate(ours['groups']) if a is not None and g.get('address') == a]
        return hits[0] if len(hits) == 1 else None
    for bk, tk in pairs:
        n += 1; i = find_ours(bk)
        if i is None: print(f'CONFLICT: group {bk} changed by the lane but absent from HEAD'); sys.exit(3)
        og = ours['groups'][i]
        ours['groups'][i] = tmap[tk] if og == bmap[bk] else merge_group(og, bmap[bk], tmap[tk], f'group {bk[1]}')
    for bk in deletes:
        n += 1; i = find_ours(bk)
        if i is None: continue
        if ours['groups'][i] != bmap[bk]: print(f'CONFLICT: lane deleted group {bk} that HEAD modified'); sys.exit(3)
        ours['groups'].pop(i)
    omap = {key(g): i for i, g in enumerate(ours['groups'])}
    for tk in adds:
        n += 1
        if tk in omap:
            if ours['groups'][omap[tk]] != tmap[tk]: print(f'CONFLICT: lane added group {tk} that HEAD has differently'); sys.exit(3)
        else: ours['groups'].append(tmap[tk])
    if base.get('_comment') != theirs.get('_comment'): ours['_comment'] = theirs['_comment']; n += 1
    return n

def encoding_of(raw, d):
    for cand in (True, False):
        if (json.dumps(d, indent=1, ensure_ascii=cand) + '\n').encode() == raw: return cand
    raise SystemExit('HEAD aliases file does not round-trip with either ensure_ascii setting')

if __name__ == '__main__':
    os.chdir(sys.argv[1])
    if len(sys.argv) > 2 and sys.argv[2] == '--test':
        o_ref, b_ref, t_ref, e_ref = sys.argv[3:7]
        ours, base, theirs, expect = (show(f'{r}:{P}') for r in (o_ref, b_ref, t_ref, e_ref))
        n = three_way(ours, base, theirs)
        emap = {key(g): g for g in expect['groups']}; omap = {key(g): g for g in ours['groups']}
        diff = [k for k in set(emap) | set(omap) if emap.get(k) != omap.get(k)]
        print(f'TEST: {n} deltas applied; {len(ours["groups"])} groups vs expect {len(expect["groups"])}; differing groups: {len(diff)}')
        for k in diff:
            print('  DIFF', k[1], k[0], (k[2] or '')[:50])
            e, o = emap.get(k), omap.get(k)
            if e and o:
                for f in set(e) | set(o):
                    if e.get(f) != o.get(f): print('     field', f, '| expect:', canon(e.get(f))[:200], '| got:', canon(o.get(f))[:200])
        sys.exit(1 if diff else 0)
    sha = subprocess.check_output(['git', 'rev-parse', 'REBASE_HEAD']).decode().strip()
    raw = subprocess.check_output(['git', 'show', ':2:' + P])
    ours = json.loads(raw); ea = encoding_of(raw, json.loads(raw))
    base = show(sha + '~1:' + P); theirs = show(sha + ':' + P)
    n = three_way(ours, base, theirs)
    open(P, 'w').write(json.dumps(ours, indent=1, ensure_ascii=ea) + '\n')
    print('encoding ensure_ascii=', ea)
    print(f"{sha[:12]}: applied {n} group deltas onto HEAD's aliases ({len(ours['groups'])} groups)")
