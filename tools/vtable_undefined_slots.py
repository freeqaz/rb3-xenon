#!/usr/bin/env python3
"""vtable_undefined_slots -- vtable slots whose function NO compiled object defines.

Lane W16-PB (2026-10-03).

The X360 match build compiles but never links, so a virtual that is declared,
referenced by an emitted `??_7` vtable, and defined nowhere (or defined only
under `#ifdef HX_NATIVE`, or only in a TU the match build does not compile) is
an undefined external that nothing reports.  Retail has a body for every slot
by construction.  W16-OT found three of these through the BODY check
(`BeatMatchController` 31/32, `User` 28), but that check only reaches slots
whose RETAIL body is a tiny leaf.  This census reaches every slot:

    for every compiled obj, for every `??_7...` vftable symbol it defines,
    every relocation target in that table's extent must be defined (secnum > 0)
    by SOME compiled obj -- else it is reported.

`_purecall` and the RTTI COL (`??_R4...`, slot -1) are expected undefined /
defined elsewhere and are skipped.  Each row is then classified by a source
grep for the method's out-of-line definition (`Class::Method(`):
    HX_NATIVE_ONLY   the only definitions sit inside `#ifdef HX_NATIVE`
    NOT_COMPILED     defined in a .cpp the match build does not compile
    NOWHERE          no definition in src/ at all
    OTHER            a definition exists in a compiled TU (inline-only /
                     template / grep miss) -- read by hand
Every row is a CANDIDATE: retail's slot body is the authority.

Usage:
    python3 tools/vtable_undefined_slots.py [--project-dir .] [--json OUT]
        [--exclude-dir src/network src/xdk]
    python3 tools/vtable_undefined_slots.py --selftest
"""
import argparse
import collections
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def _cfb(project_dir):
    sys.path.insert(0, os.path.join(project_dir, 'scripts', 'harvest'))
    import coff_func_bodies as cfb  # noqa: E402
    return cfb


def scan(project_dir):
    """-> (defined: set, tables: [(obj, vt_sym, [slot targets])])"""
    cfb = _cfb(project_dir)
    sys.path.insert(0, HERE)
    import vtable_order_sweep as vos  # noqa: E402
    base = os.path.join(project_dir, 'build', '45410914', 'src')
    defined, tables = set(), []
    objs = []
    for dp, _d, fs in os.walk(base):
        for f in fs:
            if f.endswith('.obj'):
                objs.append(os.path.join(dp, f))
    for p in sorted(objs):
        try:
            _d2, _secs, syms, _i = cfb.parse(p)
        except Exception:
            continue
        vts = []
        for (n, _v, sn, _t, sc, _i2) in syms:
            # sclass 105 = IMAGE_SYM_CLASS_WEAK_EXTERNAL: MSVC emits ??_E (vector
            # deleting dtor) as a weak alias of ??_G -- defined, not missing.
            if sn > 0 or sc == 105:
                defined.add(n)
                if n.startswith('??_7'):
                    vts.append(n)
        for vt in vts:
            tables.append((p, vt, vos.read_our_vtable(p, vt)))
    return defined, tables, len(objs)


def compiled_sources(project_dir):
    """Source files the match build compiles, from build.ninja (repo-relative)."""
    out = set()
    try:
        txt = open(os.path.join(project_dir, 'build.ninja'), errors='ignore').read()
    except OSError:
        return out
    for m in re.finditer(r'^build \S+\.obj: msvc\S* (\S+\.(?:cpp|c))', txt, re.M):
        s = m.group(1)
        out.add(os.path.normpath(os.path.relpath(s, project_dir) if os.path.isabs(s) else s))
    return out


def demangle_method(sym):
    """'?GetX@Foo@@UBAHXZ' -> ('Foo', 'GetX'); thunks/specials -> None."""
    m = re.match(r'^\?([A-Za-z_]\w*)@((?:[A-Za-z_]\w*@)+)@', sym)
    if not m:
        return None
    scopes = [s for s in m.group(2).split('@') if s]
    return '::'.join(reversed(scopes)), m.group(1)


def hx_native_ranges(lines):
    """Set of 0-based line indices inside an `#ifdef HX_NATIVE` / `#if ...HX_NATIVE` arm."""
    inside, stack = set(), []
    for i, ln in enumerate(lines):
        s = ln.strip()
        if re.match(r'#\s*if', s):
            stack.append('HX_NATIVE' in s and '!' not in s.split('HX_NATIVE')[0][-3:])
        elif re.match(r'#\s*el', s):
            if stack:
                stack[-1] = False
        elif re.match(r'#\s*endif', s):
            if stack:
                stack.pop()
        elif any(stack):
            inside.add(i)
    return inside


def classify(project_dir, cls, meth, compiled):
    short = cls.split('::')[-1]
    pat = rf'\b{re.escape(short)}::{re.escape(meth)}\s*\('
    try:
        out = subprocess.run(['rg', '-n', '--no-heading', '-g', '*.cpp', '-g', '*.h',
                              pat, os.path.join(project_dir, 'src')],
                             capture_output=True, text=True).stdout
    except FileNotFoundError:
        return 'OTHER', []
    hits = []
    for line in out.splitlines():
        f, ln, _ = line.split(':', 2)
        hits.append((os.path.relpath(f, project_dir), int(ln)))
    if not hits:
        return 'NOWHERE', []
    where = []
    for f, ln in hits:
        lines = open(os.path.join(project_dir, f), errors='ignore').read().splitlines()
        hx = (ln - 1) in hx_native_ranges(lines)
        where.append((f, ln, hx))
    nonhx = [w for w in where if not w[2]]
    if not nonhx:
        return 'HX_NATIVE_ONLY', where
    if all(w[0] not in compiled and not w[0].endswith('.h') for w in nonhx):
        return 'NOT_COMPILED', where
    return 'OTHER', where


def run(a):
    defined, tables, nobj = scan(a.project_dir)
    compiled = compiled_sources(a.project_dir)
    miss = collections.defaultdict(list)    # target -> [(vt, slot, obj)]
    nslots = 0
    for p, vt, slots in tables:
        for i, s in enumerate(slots):
            nslots += 1
            if s == '_purecall' or s.startswith('??_R4') or s in defined:
                continue
            miss[s].append((vt, i, os.path.relpath(p, a.project_dir)))
    rows = []
    for s, refs in sorted(miss.items()):
        dm = demangle_method(s)
        kind, where = ('OTHER', []) if dm is None else classify(a.project_dir, dm[0], dm[1], compiled)
        objs = sorted({r[2] for r in refs})
        if a.exclude_dir and objs and all(
                any(o.startswith(os.path.join('build', '45410914', ed)) for ed in a.exclude_dir)
                for o in objs):
            continue
        rows.append(dict(symbol=s, kind=kind, refs=[f'{v}[{i}]' for v, i, _o in refs][:6],
                         nrefs=len(refs), objs=objs[:4],
                         where=[f'{f}:{ln}{" (HX_NATIVE)" if hx else ""}' for f, ln, hx in where][:4]))
    stats = collections.Counter(r['kind'] for r in rows)
    stats.update(objs=nobj, vtables=len(tables), slots=nslots, defined=len(defined))
    for r in rows:
        print(f"{r['kind']:15s} {r['symbol']}\n    refs({r['nrefs']}): {', '.join(r['refs'])}"
              f"\n    defs: {', '.join(r['where']) or '-'}")
    print(dict(stats))
    if a.json:
        json.dump(dict(stats=dict(stats), rows=rows), open(a.json, 'w'), indent=1)
    return rows, stats


def selftest(a):
    """Vacuity floor + a fixture that MUST be found: a vtable referencing a
    symbol no obj defines.  We remove one known-defined slot target from the
    defined set and require the census to report it."""
    defined, tables, nobj = scan(a.project_dir)
    fails = []
    if nobj < 900 or len(tables) < 2000:
        fails.append(f'vacuous scan: {nobj} objs / {len(tables)} vtables')
    probe = '?Handle@Object@Hmx@@UAA?AVDataNode@@PAVDataArray@@_N@Z'
    if probe not in defined:
        fails.append(f'probe {probe} not defined -- tree unbuilt?')
    hits = sum(1 for _p, _vt, sl in tables for s in sl if s == probe)
    if hits < 100:
        fails.append(f'probe referenced by only {hits} vtables')
    defined.discard(probe)
    found = sum(1 for _p, _vt, sl in tables for s in sl if s not in defined and s == probe)
    if found != hits:
        fails.append(f'census missed the removed probe ({found}/{hits})')
    print(f'selftest: objs={nobj} vtables={len(tables)} probe_refs={hits} -> '
          + ('PASS' if not fails else 'FAIL: ' + '; '.join(fails)))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--project-dir', default=ROOT)
    ap.add_argument('--json')
    ap.add_argument('--exclude-dir', nargs='*', default=['src/network', 'src/xdk'],
                    help='drop rows whose referencing objs all live under these dirs')
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        return selftest(a)
    run(a)
    return 0


if __name__ == '__main__':
    sys.exit(main())
