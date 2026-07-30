#!/usr/bin/env python3
"""string_absence_scan.py -- prove, against the BINARY, which of our string
literals retail does not contain.

WHY (lane BX-2, generalising lane BW-2's Rnd::SetPostProcOverride win).
Retail inlines a small accessor; we emit a `bl` because OUR body carries a
DC3-era MILO_LOG/MILO_WARN block retail never had.  BW-2 proved that by hand for
one function.  The proof generalises and is cheap: a format string we emit that
does NOT occur anywhere in `band.exe` is proof retail cannot be logging it, so
that body is ours, not retail's.

★ This is MAP-INDEPENDENT.  It does not care which retail VA a function lives at,
so it is immune to the symbol map covering only ~27k of retail's ~57k functions.
Per `project_binary_absence_proof_2026-07-30`, absence in band.exe is the
strongest evidence class we have -- PROVIDED positive controls are run, because a
"0 hits" result is otherwise indistinguishable from a broken scan.

METHOD
  * Read string bytes from OUR COMPILED OBJS (the `??_C@` COMDAT string symbols),
    not from the source.  A regex over C++ source has already poisoned one
    scanner in this project (a comment matched).  The obj is what we actually emit.
  * Search the WHOLE band.exe file for those bytes, both raw ASCII and UTF-16LE.
    Whole-file search deliberately avoids any VA->offset arithmetic, so the
    0xB200 `.text` skew trap cannot apply.
  * Attribute each string back to the function(s) whose relocations reference it.

POSITIVE CONTROLS (--controls) are mandatory before believing any absence claim.

Usage:
  scripts/harvest/string_absence_scan.py --proj <worktree> [--out FILE]
  scripts/harvest/string_absence_scan.py --proj <worktree> --controls
"""
import argparse
import importlib.util
import json
import os
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

MIN_LEN = 8          # shorter literals collide by chance / are substrings


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


def obj_strings(C, path):
    """-> ({str_symbol: bytes}, {func_name: set(str_symbols referenced)})"""
    d, secs, syms, idx = C.parse(path)
    strs, funcs = {}, {}
    sec_syms = defaultdict(list)
    for (name, val, secnum, typ, sc, si) in syms:
        if secnum <= 0 or secnum > len(secs):
            continue
        sec_syms[secnum].append((val, name, sc))

    # string COMDATs: symbol name begins ??_C@ ; take bytes from its section
    for secnum, ents in sec_syms.items():
        sec = secs[secnum - 1]
        for (val, name, sc) in ents:
            if not name.startswith('??_C@'):
                continue
            raw = d[sec['rawptr'] + val: sec['rawptr'] + sec['rawsz']]
            z = raw.find(b'\0')
            if z > 0:
                strs[name] = raw[:z]

    # functions -> referenced string symbols, via .text relocations
    for secnum, ents in sec_syms.items():
        sec = secs[secnum - 1]
        if not sec['name'].startswith('.text'):
            continue
        fents = sorted((v, n) for (v, n, sc) in ents if sc in (2, 3))
        rels = []
        for r in range(sec['nrel']):
            o = sec['relptr'] + r * 10
            va, symidx, rtyp = struct.unpack_from('<IIH', d, o)
            s = idx.get(symidx)
            rels.append((va, s[0] if s else None))
        for k, (off, name) in enumerate(fents):
            end = fents[k + 1][0] if k + 1 < len(fents) else sec['rawsz']
            refs = {nm for (va, nm) in rels
                    if nm and off <= va < end and nm.startswith('??_C@')}
            if refs:
                funcs.setdefault(name, set()).update(refs)
    return strs, funcs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--proj', required=True)
    ap.add_argument('--out', default=os.path.expanduser('~/tmp/laneBX2/string_absence.json'))
    ap.add_argument('--controls', action='store_true')
    args = ap.parse_args()

    proj = Path(args.proj)
    C = _load('coff_func_bodies', proj / 'scripts' / 'harvest' / 'coff_func_bodies.py')
    band = (proj / 'orig' / '45410914' / 'band.exe').read_bytes()
    print(f'band.exe: {len(band):,} bytes', file=sys.stderr)

    # Fast path: pre-extract every NUL-terminated printable run in the image.
    # A naive band.find() per string is O(#strings * 14MB) -- ~30k strings would
    # scan ~420 GB.  Exact set-membership catches the overwhelming majority; the
    # expensive substring/UTF-16 search runs only on the misses.
    retail_set = set(re.findall(rb'[\x20-\x7e]{%d,}' % MIN_LEN, band))
    print(f'retail printable runs indexed: {len(retail_set):,}', file=sys.stderr)

    def present(b: bytes) -> bool:
        if b in retail_set:                     # exact run
            return True
        if band.find(b) >= 0:                   # substring of a longer run
            return True
        try:                                    # UTF-16LE variant
            return band.find(b.decode('latin1').encode('utf-16-le')) >= 0
        except Exception:                       # noqa: BLE001
            return False

    report = json.load(open(proj / 'build' / '45410914' / 'report.json'))

    if args.controls:
        # GROUND-TRUTH POSITIVE CONTROL.  Hand-guessed control strings are
        # worthless -- two of this author's first ten guesses ("Rock Band",
        # "%s: %s") were simply absent from retail, which says nothing about the
        # scanner.  So derive the control from the build instead: a function at
        # fuzzy 100 is BYTE-IDENTICAL to retail, therefore every string literal
        # it references MUST exist in retail.  Any such string reported absent is
        # a scanner defect, and the whole run is void.
        perfect = set()
        for u in report['units']:
            if not (u.get('metadata') or {}).get('source_path'):
                continue
            for fn in u.get('functions') or []:
                if fn.get('fuzzy_match_percent') == 100.0:
                    perfect.add(fn['name'])
        tested = miss = 0
        examples = []
        for u in report['units']:
            sp = (u.get('metadata') or {}).get('source_path')
            if not sp:
                continue
            obj = proj / 'build' / '45410914' / re.sub(r'\.(cpp|c)$', '.obj', sp)
            if not obj.exists():
                continue
            try:
                strs, funcs = obj_strings(C, str(obj))
            except Exception:                    # noqa: BLE001
                continue
            for fname, refs in funcs.items():
                if fname not in perfect:
                    continue
                for sname in refs:
                    b = strs.get(sname)
                    if not b or len(b) < MIN_LEN:
                        continue
                    tested += 1
                    if not present(b):
                        miss += 1
                        if len(examples) < 10:
                            examples.append((fname, b[:80]))
            if tested > 4000:
                break
        neg = [b'ZZZZ_this_string_is_not_in_retail_12345',
               b'lane_BX2_negative_control_qqq']
        okn = sum(1 for s in neg if not present(s))
        print(f'POSITIVE (strings referenced by fuzzy-100, byte-identical fns):')
        print(f'  tested {tested}   reported ABSENT (should be 0): {miss}')
        for fn, b in examples:
            print(f'    !! {b!r}  in {fn[:60]}')
        print(f'NEGATIVE (synthetic): {okn}/{len(neg)} correctly absent')
        rate = (miss / tested) if tested else 1.0
        if tested < 200 or okn < len(neg) or rate > 0.01:
            print('!! CONTROL FAILURE -- absence results are NOT trustworthy')
            return 1
        print(f'controls PASS (false-absence rate {rate:.4%})')
        return 0

    sizes = {}
    unit_of = {}
    for u in report['units']:
        if not (u.get('metadata') or {}).get('source_path'):
            continue
        for fn in u.get('functions') or []:
            sizes[fn['name']] = (int(fn.get('size') or 0),
                                 fn.get('fuzzy_match_percent'), u['name'])

    seen_str = {}
    rows = []
    nobj = 0
    for u in report['units']:
        sp = (u.get('metadata') or {}).get('source_path')
        if not sp:
            continue
        obj = proj / 'build' / '45410914' / re.sub(r'\.(cpp|c)$', '.obj', sp)
        if not obj.exists():
            continue
        nobj += 1
        try:
            strs, funcs = obj_strings(C, str(obj))
        except Exception as e:                  # noqa: BLE001
            print(f'  !! {obj.name}: {e}', file=sys.stderr)
            continue
        for fname, refs in funcs.items():
            absent = []
            for sname in refs:
                b = strs.get(sname)
                if not b or len(b) < MIN_LEN:
                    continue
                if sname not in seen_str:
                    seen_str[sname] = present(b)
                if not seen_str[sname]:
                    absent.append(b.decode('latin1', 'replace'))
            if absent:
                sz, pct, un = sizes.get(fname, (0, None, u['name']))
                rows.append(dict(unit=u['name'], fn=fname, size=sz, pct=pct,
                                 absent=sorted(absent)))

    rows.sort(key=lambda r: -r['size'])
    json.dump(rows, open(args.out, 'w'), indent=1)
    print(f'\nobjs scanned {nobj}; distinct strings tested {len(seen_str)}; '
          f'absent-string functions {len(rows)}', file=sys.stderr)
    print(f'wrote {args.out}')
    print('\nTOP 30 functions carrying retail-ABSENT strings (by size):')
    for r in rows[:30]:
        p = f'{r["pct"]:6.2f}%' if isinstance(r['pct'], (int, float)) else '   n/a'
        print(f'  {r["size"]:6,}B {p}  {r["unit"][:38]:38s} {r["fn"][:56]}')
        for a in r['absent'][:2]:
            print(f'            ABSENT: {a[:96]!r}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
