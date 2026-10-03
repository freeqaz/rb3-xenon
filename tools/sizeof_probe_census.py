#!/usr/bin/env python3
"""sizeof_probe_census -- retail sizeof (retail_sizeof_witness) vs the COMPILER,
per class, probed INSIDE A TU THAT REALLY COMPILES THE CLASS'S HEADER.

Lane W16-OT (2026-10-03).  W16-OR §3 measured its 151 classes with a
header-only probe TU, which it called a SCREEN because a header-only TU does
not carry per-TU `/D` flags (RB3_MAP_0x1C, ...).  This tool removes that
caveat: for each witnessed class it picks a compiled `.cpp` that includes the
defining header (the same-stem `.cpp` first), takes that TU's real compile
command from build.ninja (class_layout_report.build_command -- PCH neutralised,
`/Fo` to scratch), and compiles a wrapper

    #include "<the TU's own .cpp>"
    template <int N> struct __szp;   __szp<sizeof(::T)> __s0;  ...

so every `/D` the TU carries is in force.  The two-line C2079 diagnostic
(`... '__szp<N>'` then `N=200`) carries the value; the parser is
class_layout_report's.

Population: classes named by retail RTTI with >= 1 non-MEMBER0 retail_sizeof
witness, DEFINED (`class|struct X {` / `: base`) in a header under --dirs,
not also defined under --exclude-dirs.

Read-only (scratch objects under ~/tmp).  Usage:
    python3 tools/sizeof_probe_census.py --witness W.json --dirs src/system/rndobj ...
        [--exclude-dirs src/network ...] [--json OUT] [--control N]
"""
import argparse
import collections
import glob
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'scripts', 'harvest'))
import class_layout_report as clr  # noqa: E402

DEF = r'^\s*(?:class|struct)\s+(?:__declspec\([^)]*\)\s+)?%s\s*(?:final\s*)?(?::(?!:)|\{|$)'


def defining_headers(project_dir, name, dirs):
    pat = re.compile(DEF % re.escape(name), re.M)
    hits = []
    for d in dirs:
        for f in glob.glob(os.path.join(project_dir, d, '**', '*.h'), recursive=True):
            try:
                if pat.search(open(f, errors='ignore').read()):
                    hits.append(os.path.relpath(f, project_dir))
            except OSError:
                pass
    return hits


def tus_including(project_dir, header, cache={}):
    """compiled .cpp files whose text #includes some spelling of `header`."""
    if 'cpps' not in cache:
        cache['cpps'] = {}
        for f in glob.glob(os.path.join(project_dir, 'src', '**', '*.cpp'), recursive=True):
            try:
                cache['cpps'][os.path.relpath(f, project_dir)] = open(f, errors='ignore').read()
            except OSError:
                pass
    sp = clr._include_spellings(header)
    stem = os.path.splitext(header)[0]
    out = []
    for rel, txt in cache['cpps'].items():
        for m in re.finditer(r'#\s*include\s+"([^"]+)"', txt):
            if m.group(1) in sp:
                out.append(rel)
                break
    out.sort(key=lambda r: (os.path.splitext(r)[0] != stem, len(r)))
    return out


def probe(project_dir, src, names):
    """{qualified name: (sizeof, alignof)} for `names`, compiled in TU `src`."""
    obj = clr._obj_for_source(project_dir, src)
    if not obj:
        return None, 'no_compile_edge'
    with tempfile.TemporaryDirectory(dir=os.path.expanduser('~/tmp')) as td:
        argv, env, cwd = clr.build_command(project_dir, obj, None,
                                           os.path.join(td, 'probe.obj'), all_classes=True)
        argv = [a for a in argv if a != '/d1reportAllClassLayout']
        src_abs = os.path.abspath(os.path.join(cwd, argv[-1]))
        wrap = os.path.join(td, 'szp.cpp')
        lines = ['#include "%s"' % src_abs, 'template <int N> struct __szp;',
                 'template <int N> struct __alp;']
        for i, k in enumerate(names):
            lines.append(f'__szp<sizeof(::{k})> __s{i};')
            lines.append(f'__alp<__alignof(::{k})> __a{i};')
        open(wrap, 'w').write('\n'.join(lines) + '\n')
        argv[-1] = os.path.relpath(wrap, cwd)
        p = subprocess.run(argv, cwd=cwd, capture_output=True, text=True, env=env)
    # An UNRESOLVED name still produces C2079 with N=0 (C2065 + C2070 first):
    # any probe line that carries another error is void, never a size.
    bad = set()
    for line in (p.stdout + p.stderr).splitlines():
        m = re.search(r'szp\.cpp\((\d+)\) : error (C\d+)', line)
        if m and m.group(2) != 'C2079':
            bad.add(int(m.group(1)))
    got, pending = {}, None
    for line in (p.stdout + p.stderr).splitlines():
        m = re.search(r"'__([sa])(\d+)' uses undefined struct", line)
        if m:
            pending = (m.group(1), int(m.group(2)))
            m2 = re.search(r'__(?:szp|alp)<(\d+)>', line)
            if m2:
                got[pending], pending = int(m2.group(1)), None
            continue
        m = re.match(r'^\s*N=(\d+)\s*$', line)
        if m and pending:
            got[pending], pending = int(m.group(1)), None
    res = {}
    for i, k in enumerate(names):
        if ('s', i) in got and (4 + 2 * i) not in bad:
            res[k] = (got[('s', i)], got.get(('a', i)))
    return res, (p.stdout + p.stderr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--project-dir', default=ROOT)
    ap.add_argument('--witness', required=True, help='retail_sizeof_witness --json output')
    ap.add_argument('--dirs', nargs='+', required=True)
    ap.add_argument('--exclude-dirs', nargs='*', default=['src/network'])
    ap.add_argument('--json')
    ap.add_argument('--batch', type=int, default=40, help='classes per compile (<100 errors)')
    a = ap.parse_args()
    pd = a.project_dir
    W = json.load(open(a.witness))
    rows, stats = [], collections.Counter()
    by_tu = collections.defaultdict(list)
    for td, v in sorted(W.items()):
        if not v['sizes']:
            continue
        qual = v['name']
        if '<' in qual or '`' in qual or '?' in qual:
            stats['skip_template_or_anon'] += 1
            continue
        short = qual.split('::')[-1]
        hdrs = defining_headers(pd, short, a.dirs)
        if not hdrs:
            continue
        if a.exclude_dirs and defining_headers(pd, short, a.exclude_dirs):
            stats['excluded_also_defined_elsewhere'] += 1
            continue
        stats['population'] += 1
        tus = []
        for h in hdrs:
            tus += [t for t in tus_including(pd, h) if t not in tus]
        cands = [t for t in tus if clr._obj_for_source(pd, t)][:4]
        tu = cands[0] if cands else None
        rec = dict(cls=qual, td=td, headers=hdrs, retail=v['sizes'], tu=tu, cands=cands)
        rows.append(rec)
        if tu is None:
            rec['verdict'] = 'NO_TU'
            stats['NO_TU'] += 1
            continue
        by_tu[tu].append(rec)
    retry = collections.defaultdict(list)
    def judge(tu, recs, last):
        for i in range(0, len(recs), a.batch):
            chunk = recs[i:i + a.batch]
            res, why = probe(pd, tu, [r['cls'] for r in chunk])
            for r in chunk:
                got = (res or {}).get(r['cls'])
                if got is None:
                    nxt = r['cands'][r['cands'].index(tu) + 1:] if tu in r['cands'] else []
                    if nxt and not last:
                        r['tu'] = nxt[0]
                        retry[nxt[0]].append(r)
                        continue
                    r['verdict'] = 'UNPROBED'
                    stats['UNPROBED'] += 1
                    continue
                r['ours'], r['align'] = got
                rs = sorted(int(k) for k in r['retail'])
                if len(rs) > 1:
                    r['verdict'] = 'RETAIL_CONFLICT'
                elif rs[0] == got[0]:
                    r['verdict'] = 'AGREE'
                else:
                    r['verdict'] = 'DIFFER'
                stats[r['verdict']] += 1
            print(f'# {tu}: {len(chunk)} probed', file=sys.stderr)
    for tu, recs in sorted(by_tu.items()):
        judge(tu, recs, False)
    for _round in range(3):
        cur = dict(retry)
        retry.clear()
        for tu, recs in sorted(cur.items()):
            judge(tu, recs, _round == 2)
    for r in rows:
        if r.get('verdict') != 'AGREE':
            print(f"{r.get('verdict'):16s} {r['cls']:40s} retail={r['retail']} ours={r.get('ours')} tu={r['tu']}")
    print(dict(stats))
    if a.json:
        json.dump(rows, open(a.json, 'w'), indent=1)
    return 0


if __name__ == '__main__':
    sys.exit(main())
