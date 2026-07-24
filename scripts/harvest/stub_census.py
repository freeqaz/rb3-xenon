#!/usr/bin/env python3
"""stub_census — rank pinned-but-stubbed/partial TUs whose oracle source is
materially fuller than the in-tree .cpp (the SongParser full-file-port vein).

For every wired TU in report.json (has source_path + is pinned/compiled):
  - in-tree .cpp line count (worktree src/)
  - oracle line count: dc3 for src/system/*, rb3-Wii for src/band3|meta_ham|network
    (both trees indexed; exact relpath preferred, basename fallback)
  - unit matched_functions_percent + remaining fn count
Flags TUs where oracle >> xenon AND match% low = stub/partial where a full-file
port pairs a whole COMDAT cluster. Classifies via native_scope_map (CORE/SOON only).
"""
import json, os, sys
from pathlib import Path

ROOT = Path('/home/free/tmp/wt-lane2-songparser')
DC3 = Path('/home/free/code/milohax/dc3-decomp/src')
WII = Path('/home/free/code/milohax/rb3/src')
sys.path.insert(0, str(ROOT / 'scripts'))
from native_scope_map import classify, milestone  # noqa

def index_tree(root):
    by_rel = {}   # relpath(str, from src/) -> Path
    by_base = {}  # basename.lower() -> [Path]
    for p in root.rglob('*.cpp'):
        rel = str(p.relative_to(root))
        by_rel[rel.lower()] = p
        by_base.setdefault(p.name.lower(), []).append(p)
    return by_rel, by_base

dc3_rel, dc3_base = index_tree(DC3)
wii_rel, wii_base = index_tree(WII)

def linecount(p):
    try:
        return sum(1 for _ in open(p, 'rb'))
    except Exception:
        return 0

def oracle_lookup(relpath):
    """relpath like 'system/beatmatch/MasterAudio.cpp' (already stripped of src/).
    Returns (oracle_name, lines, matched_path) preferring the correct oracle."""
    base = os.path.basename(relpath).lower()
    is_engine = relpath.startswith('system/')
    prefer = [('dc3', dc3_rel, dc3_base), ('wii', wii_rel, wii_base)]
    if not is_engine:
        prefer = [('wii', wii_rel, wii_base), ('dc3', dc3_rel, dc3_base)]
    # exact relpath match first (either tree, prefer order)
    results = {}
    for name, rel, bs in prefer:
        if relpath.lower() in rel:
            p = rel[relpath.lower()]
            results[name] = linecount(p)
        elif base in bs and len(bs[base]) == 1:
            results[name] = linecount(bs[base][0])
        elif base in bs:
            # ambiguous basename: pick the one whose relpath shares the most trailing components
            want = relpath.lower().split('/')
            best, bestscore = None, -1
            for cand in bs[base]:
                cparts = str(cand).lower().split('/')
                sc = 0
                for a, b in zip(reversed(want), reversed(cparts)):
                    if a == b: sc += 1
                    else: break
                if sc > bestscore:
                    bestscore, best = sc, cand
            results[name] = linecount(best)
    if not results:
        return (None, 0, {})
    # report both, but headline = preferred non-zero
    headline = None
    for name, _, _ in prefer:
        if results.get(name):
            headline = name; break
    return (headline, results.get(headline, 0), results)

def main():
    d = json.load(open(ROOT / 'build/45410914/report.json'))
    rows = []
    for u in d['units']:
        md = u.get('metadata') or {}
        sp = md.get('source_path')
        if not sp or md.get('auto_generated'):
            continue
        m = u['measures']
        tf = m.get('total_functions', 0)
        mf = m.get('matched_functions', 0)
        pct = m.get('matched_functions_percent', 0.0)
        if tf == 0:
            continue
        cls = classify(sp)
        if cls not in ('NATIVE-CORE', 'NATIVE-SOON'):
            continue
        rel = sp[4:] if sp.startswith('src/') else sp
        xpath = ROOT / sp
        xlines = linecount(xpath)
        oname, olines, oall = oracle_lookup(rel)
        rem = tf - mf
        fns = u.get('functions', [])
        anon = sum(1 for f in fns if f.get('name', '').startswith('fn_'))
        rows.append(dict(unit=u['name'], sp=sp, cls=cls, ms=milestone(sp),
                         xlines=xlines, oname=oname, olines=olines, oall=oall,
                         pct=pct, tf=tf, mf=mf, rem=rem, anon=anon))
    # scoring: fuller oracle + lower match% + more remaining fns
    def tier(r):
        if r['olines'] == 0 or r['xlines'] == 0:
            return 'no-oracle'
        # fully done: no remaining unmatched fns
        if r['rem'] == 0 or r['pct'] >= 99.0:
            return 'done'
        ratio = r['olines'] / max(1, r['xlines'])
        fuller = ratio >= 1.30      # oracle materially larger than in-tree stub
        vfuller = ratio >= 2.0
        low = r['pct'] < 60
        r['ratio'] = ratio
        if not fuller:
            # in-tree ~= oracle: source present, residue is body-divergence (walled)
            # secondary per-fn bodyport residue if lots of unmatched fns remain
            return 'partial-body' if (low and r['rem'] >= 4) else 'ported'
        if vfuller and low and r['rem'] >= 4:
            return 'HIGH'
        if fuller and low and r['rem'] >= 3:
            return 'MED'
        return 'LOW'
    for r in rows:
        r['tier'] = tier(r)
        r['ratio'] = r['olines'] / max(1, r['xlines'])
    order = {'HIGH':0,'MED':1,'LOW':2,'partial-body':3,'ported':4,'done':5,'no-oracle':6}
    rows.sort(key=lambda r: (order[r['tier']], -r['ratio'], r['pct'], -r['rem']))
    json.dump(rows, open('/home/free/tmp/census_rows.json','w'), indent=1)
    from collections import Counter
    c = Counter(r['tier'] for r in rows)
    print('TIER COUNTS:', dict(c))
    print('CORE+SOON wired TUs scanned:', len(rows))
    print()
    print('### Full-file-port candidates (oracle materially fuller than in-tree stub)\n')
    print('| # | TU | class | milestone | xenon_ln | oracle_ln | oracle | ratio | fn% | rem | tier |')
    print('|--:|---|---|---|--:|--:|---|--:|--:|--:|---|')
    i = 0
    for r in rows:
        if r['tier'] not in ('HIGH','MED','LOW'):
            continue
        i += 1
        oa = r['oall']; ostr = '/'.join(f"{k}:{v}" for k,v in oa.items())
        print(f"| {i} | {r['unit'].split('/')[-1]} | {r['cls'].replace('NATIVE-','')} | {r['ms']} | "
              f"{r['xlines']} | {r['olines']} | {ostr} | {r['ratio']:.2f} | {r['pct']:.1f} | {r['rem']} | {r['tier']} |")
    print('\n### Secondary: partial-body residue (source ~present, per-fn bodyport, lower yield)\n')
    print('| # | TU | class | milestone | xenon_ln | oracle_ln | ratio | fn% | rem |')
    print('|--:|---|---|---|--:|--:|--:|--:|--:|')
    j = 0
    for r in rows:
        if r['tier'] != 'partial-body':
            continue
        j += 1
        print(f"| {j} | {r['unit'].split('/')[-1]} | {r['cls'].replace('NATIVE-','')} | {r['ms']} | "
              f"{r['xlines']} | {r['olines']} | {r['ratio']:.2f} | {r['pct']:.1f} | {r['rem']} |")

    # AUTOMAP-recovery seeds: pinned units carrying unmapped anon fn_ target
    # symbols (the SongParser lever proven this session: source present, map gap).
    amap = sorted([r for r in rows if r['anon'] >= 2],
                  key=lambda r: (-r['anon'], r['pct']))
    print(f"\n### AUTOMAP-recovery seeds (unmapped anon fn_ in span; SongParser lever) — {len(amap)} units\n")
    print('| # | TU | class | milestone | anon_fn | tf | strict fn% | ratio |')
    print('|--:|---|---|---|--:|--:|--:|--:|')
    for k, r in enumerate(amap[:40], 1):
        print(f"| {k} | {r['unit'].split('/')[-1]} | {r['cls'].replace('NATIVE-','')} | {r['ms']} | "
              f"{r['anon']} | {r['tf']} | {r['pct']:.1f} | {r['ratio']:.2f} |")

    # ---- UNWIRED scan: oracle .cpp with no objects.json entry ----------------
    obj = json.load(open(ROOT / 'config/45410914/objects.json'))
    wired = set()
    for grp in obj.values():
        for k in (grp.get('objects') or {}):
            wired.add(k.lower())            # rel to src/, e.g. system/beatmatch/X.cpp
    def is_scope(rel):
        return classify('src/' + rel) in ('NATIVE-CORE', 'NATIVE-SOON')
    unwired = []
    # engine unwired -> dc3 ; game unwired -> wii
    def is_noise(rel):
        base = os.path.basename(rel)
        return (base.startswith('.') or 'permuter_work' in rel or '/.' in rel
                or base.startswith('symbols') or 'dataflex_target' in base)
    for tag, rel_index in (('dc3', dc3_rel), ('wii', wii_rel)):
        for rel, p in rel_index.items():
            if rel in wired:
                continue
            if not is_scope(rel) or is_noise(rel):
                continue
            # skip if an in-tree src file already exists (wired-elsewhere / scaffolded)
            if (ROOT / 'src' / rel).exists():
                continue
            # only count if the sibling tree agrees this is in-scope game/engine
            unwired.append((rel, tag, linecount(p)))
    # dedup by rel (prefer wii for game, dc3 for engine already scope-gated)
    seen = {}
    for rel, tag, ln in unwired:
        if rel not in seen or ln > seen[rel][1]:
            seen[rel] = (tag, ln)
    unw = sorted(seen.items(), key=lambda kv: -kv[1][1])
    print(f"\n### UNWIRED in-scope oracle TUs (no objects.json entry) — {len(unw)} files\n")
    print('_(.cpp present in oracle, CORE/SOON scope, not yet wired — gameport/wire lever)_\n')
    print('| # | rel path | oracle | oracle_ln |')
    print('|--:|---|---|--:|')
    for k, (rel, (tag, ln)) in enumerate(unw[:40], 1):
        print(f"| {k} | {rel} | {tag} | {ln} |")
    json.dump({'unwired': [(r, t, l) for r, (t, l) in unw]},
              open('/home/free/tmp/census_unwired.json', 'w'), indent=1)

if __name__ == '__main__':
    main()
