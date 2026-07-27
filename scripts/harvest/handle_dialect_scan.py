#!/usr/bin/env python3
"""RB3_HANDLE_LOCAL_STATIC ELIGIBILITY scan (laneAX-W9, 2026-07-27).

★ WHY THIS EXISTS
`/DRB3_HANDLE_LOCAL_STATIC` only gates the HANDLE/HANDLE_EXPR/HANDLE_ACTION*
macros defined in `src/system/obj/ObjMacros.h`.  There is a SECOND, independent
copy of the same macro family in `src/system/obj/Object.h` (~line 1032), and
that copy emits the function-local static UNCONDITIONALLY:

    #define _NEW_STATIC_SYMBOL(str) static Symbol _s(#str);
    #define HANDLE(s, func) { _NEW_STATIC_SYMBOL(s) if (sym == _s) ... }

`src/system/obj/dialect_object_push.h` documents the two dialects explicitly
and shims between them at COMDAT-scatter `#include "<owner>.cpp"` sites.

CONSEQUENCE: for an **Object.h-dialect** TU the flag is a structural NO-OP --
its dispatch Symbols are *already* function-local statics (verified: the
compiled objs carry `?_s@?N@??Handle@<Class>@@...@4VSymbol@@A` symbols, and
adding the define leaves the `?Handle@...` COMDAT BYTE-IDENTICAL).  Counting
"TUs that use BEGIN_HANDLERS and lack the flag" therefore massively
over-states the pool: measured 365 such TUs, of which only 39 are actually
ObjMacros.h-dialect and hence eligible.

The dialect a TU's own Handle uses = whether ObjMacros.h had been included by
the time its BEGIN_HANDLERS block was preprocessed.  Object.h does NOT include
ObjMacros.h, so the proxy used here (does the TU's own text mention
ObjMacros.h) is exact for the in-tree sources: local statics named `_hs`
(ObjMacros dialect) vs `_s` (Object.h dialect) confirm it per-obj.

Usage:
    python3 scripts/harvest/handle_dialect_scan.py <worktree> [--verify-objs]

Prints the eligible pool (ObjMacros dialect, no flag) and, when report.json is
present, each one's current `?Handle@` match%.
"""
import argparse
import json
import os
import re
import sys


def load_objects(wt):
    cfg = json.load(open(os.path.join(wt, 'config/45410914/objects.json')))
    flagged, allobj = set(), []
    for _g, gv in cfg.items():
        for k, v in (gv.get('objects') or {}).items():
            allobj.append(k)
            cf = v.get('extra_cflags', []) if isinstance(v, dict) else []
            if '/DRB3_HANDLE_LOCAL_STATIC' in cf:
                flagged.add(k)
    return allobj, flagged


def dialect(wt, rel):
    try:
        t = open(os.path.join(wt, 'src', rel), errors='ignore').read()
    except OSError:
        return None
    if 'BEGIN_HANDLERS' not in t and 'BEGIN_CUSTOM_HANDLERS' not in t:
        return None
    return 'ObjMacros' if 'ObjMacros.h' in t else 'ObjectH'


def handle_pcts(wt):
    """(unit basename) -> [(name, pct)] for every ?Handle@ symbol."""
    p = os.path.join(wt, 'build/45410914/report.json')
    if not os.path.exists(p):
        return {}
    out = {}
    for u in json.load(open(p))['units']:
        un = u['name'].split('/')[-1]
        for f in (u.get('functions') or []):
            if re.match(r'\?Handle@\w', f['name']):
                out.setdefault(un, []).append(
                    (f['name'], f['match_percent_normalized']))
    return out


def verify_objs(wt, rels):
    """Per-obj ground truth: count `_hs` (ObjMacros) vs `_s` (Object.h) local
    statics inside Handle bodies of the COMPILED obj."""
    sys.path.insert(0, os.path.join(wt, 'scripts/harvest'))
    import localstatic_patch_gen as L  # noqa: E402
    out = {}
    for rel in rels:
        op = os.path.join(wt, 'build/45410914/src', rel[:-4] + '.obj')
        if not os.path.exists(op):
            continue
        try:
            secs, syms = L.read_coff(open(op, 'rb').read())
        except Exception:
            continue
        if secs is None:
            continue
        hs = sum(1 for s in syms if s.name.startswith('?_hs@?')
                 and 'Handle@' in s.name)
        st = sum(1 for s in syms if s.name.startswith('?_s@?')
                 and 'Handle@' in s.name)
        out[rel] = (hs, st)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('worktree')
    ap.add_argument('--verify-objs', action='store_true')
    a = ap.parse_args()
    wt = a.worktree

    allobj, flagged = load_objects(wt)
    buckets = {}
    for rel in sorted(set(allobj)):
        d = dialect(wt, rel)
        if d is None:
            continue
        buckets.setdefault((d, rel in flagged), []).append(rel)

    print('BEGIN_HANDLERS TUs by macro dialect x flag:')
    for k in sorted(buckets):
        print('   dialect=%-9s flagged=%-5s  %d' % (k[0], k[1], len(buckets[k])))
    print()
    print('  Object.h dialect  => flag is a NO-OP (local statics already emitted)')
    print('  ObjMacros dialect => flag is LOAD-BEARING')
    print()

    pool = buckets.get(('ObjMacros', False), [])
    pcts = handle_pcts(wt)
    print('ELIGIBLE POOL: ObjMacros dialect, flag ABSENT (%d)' % len(pool))
    for rel in pool:
        un = os.path.basename(rel)[:-4]
        subs = [(n, p) for n, p in pcts.get(un, []) if p < 99.999]
        tag = ''
        if subs:
            tag = '  <-- sub-100: ' + ', '.join(
                '%s %.2f%%' % (n.split('@')[1], p) for n, p in subs[:4])
        print('   %-52s%s' % (rel, tag))

    inert = buckets.get(('ObjectH', True), [])
    if inert:
        print()
        print('FLAG PRESENT BUT INERT (Object.h dialect) (%d)' % len(inert))
        for rel in inert:
            print('   %s' % rel)

    if a.verify_objs:
        print()
        print('OBJ GROUND TRUTH (_hs = ObjMacros local statics, '
              '_s = Object.h local statics)')
        rels = pool + buckets.get(('ObjMacros', True), [])[:10] \
            + buckets.get(('ObjectH', False), [])[:10]
        for rel, (hs, st) in sorted(verify_objs(wt, rels).items()):
            print('   %-52s _hs=%-4d _s=%-4d' % (rel, hs, st))


main()
