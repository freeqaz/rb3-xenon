#!/usr/bin/env python3
"""Repair provably mis-assigned target_symbol_map entries (laneAT, 2026-07-26).

An entry is provably wrong when, in the unit it lives in, the target symbol it
names is NOT relocation-masked byte-equal to our base symbol of that name, yet
some OTHER base symbol in the same unit IS byte-equal and is not itself claimed
by any map entry.  Re-pointing the entry to that free exact twin converts the
function.

Measured on main at 620bfb21: 2,437 named target symbols sit below 100%; 1,936
are honest body divergence, 113 are masked-equal-yet-below-100 (normalised diff
is stricter than masked equality), 188 name a base symbol the unit does not
define, 45 have an exact twin already taken -- and **155 have a FREE exact
twin**.  Repairing 120 of them (35 skipped: alternative already claimed, or the
current name is mapped at more than one VA) measured **+99, 0 losses**.

Note the target objs have already been renamed in-place by the pre-compile
``obj_target_symbol_renamer`` build step, so mapped symbols appear under their
mangled name, not as ``fn_<VA>`` -- this tool keys off the mangled name.

Requires a FULL build in the worktree first: setup_worktree.sh reflinks main's
dirty build dir, and a pre-build scan reads other lanes' uncommitted objs.

Usage:
    python3 scripts/harvest/map_misassign_repair.py <worktree> [--apply]
"""
import argparse, collections, glob, json, os, re, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'analysis'))
try:
    from coffx import read_coff, infer_sizes, funclet_signature, K_SEC
except ImportError:
    sys.path.insert(0, '/home/free/tmp/laneAM')
    from coffx import read_coff, infer_sizes, funclet_signature, K_SEC


def canon_sigs(path):
    try:
        data = open(path, 'rb').read()
    except OSError:
        return None
    secs, syms = read_coff(data)
    if secs is None:
        return None
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('worktree')
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('--drop-dangling', action='store_true',
                    help='remove entries naming a symbol their unit does not define')
    a = ap.parse_args()
    wt = a.worktree
    map_path = os.path.join(wt, 'scripts/target_symbol_map.json')
    m = json.load(open(map_path))
    name2va = collections.defaultdict(list)
    for k, v in m.items():
        if isinstance(v, str):
            name2va[v].append(k)
    rep = json.load(open(os.path.join(wt, 'build/45410914/report.json')))
    pct = {f['name']: f['match_percent_normalized']
           for u in rep['units'] for f in (u.get('functions') or [])}
    base_by_name = collections.defaultdict(list)
    for p in glob.glob(os.path.join(wt, 'build/45410914/src/**/*.obj'), recursive=True):
        base_by_name[os.path.basename(p)].append(p)

    stats, repairs, drops = collections.Counter(), {}, set()
    root = os.path.join(wt, 'build/45410914/obj')
    for tp in sorted(glob.glob(os.path.join(root, '**', '*.obj'), recursive=True)):
        rel = os.path.relpath(tp, root)
        bp = os.path.join(wt, 'build/45410914/src', rel)
        if not os.path.exists(bp):
            c = base_by_name.get(os.path.basename(tp))
            if not c or len(c) != 1:
                continue
            bp = c[0]
        ts, bs = canon_sigs(tp), canon_sigs(bp)
        if not ts or not bs:
            continue
        by_sig, by_name = collections.defaultdict(list), {}
        for g, nm, sz in bs:
            by_sig[g].append(nm)
            by_name.setdefault(nm, g)
        tnames = {nm for g, nm, sz in ts}
        for g, nm, sz in ts:
            if nm.startswith('fn_') or nm.startswith('__') or '$' in nm[:2]:
                continue
            p = pct.get(nm)
            if p is None or p >= 100.0:
                continue
            stats['named_below100'] += 1
            bg = by_name.get(nm)
            if bg is None:
                # The entry names a symbol this unit does not define, so the
                # renamed target symbol can never pair. Dead weight, and it
                # holds the name hostage from a target that could use it.
                stats['no_such_base_symbol'] += 1
                vas = name2va.get(nm, [])
                if a.drop_dangling and len(vas) == 1:
                    stats['DANGLING_DROPPED'] += 1
                    drops.add(vas[0])
                continue
            if bg == g:
                stats['masked_equal_yet_below100'] += 1
                continue
            alt = [x for x in by_sig.get(g, [])
                   if x not in tnames and x not in name2va]
            if not alt:
                stats['exact_alt_taken' if by_sig.get(g) else 'body_divergence'] += 1
                continue
            vas = name2va.get(nm, [])
            if len(vas) != 1:
                stats['current_name_multi_mapped'] += 1
                continue
            stats['REPAIRABLE'] += 1
            repairs[vas[0]] = alt[0]
    print(json.dumps(stats, indent=1))
    print('repairs: %d' % len(repairs))
    if a.apply:
        # line-level substitution keeps the diff to one line per repair; a
        # re-serialise churns the whole 25k-line file and blocks concurrent lanes.
        lines = open(map_path).read().split('\n')
        done = dropped = 0
        keep = []
        for i, ln in enumerate(lines):
            mm = re.match(r'^(\s*)("0[xX][0-9a-fA-F]+")\s*:\s*(.*?)(,?)$', ln)
            if not mm:
                keep.append(ln)
                continue
            key = json.loads(mm.group(2))
            if key in drops:
                dropped += 1
                continue
            if key in repairs:
                ln = '%s%s: %s%s' % (mm.group(1), mm.group(2),
                                     json.dumps(repairs[key]), mm.group(4))
                done += 1
            keep.append(ln)
        # a dropped line may have carried the trailing comma of the last entry
        for i in range(len(keep) - 1, -1, -1):
            t = keep[i].rstrip()
            if t.endswith(','):
                keep[i] = keep[i].rstrip()[:-1]
                break
            if t.endswith('}') or not t:
                continue
            break
        print('dropped %d dangling entries' % dropped)
        open(map_path, 'w').write('\n'.join(keep))
        chk = json.load(open(map_path))
        assert all(chk.get(k) == v for k, v in repairs.items())
        assert not (drops & set(chk))
        print('applied %d repairs -> %s' % (done, map_path))


if __name__ == '__main__':
    main()
