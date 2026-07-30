#!/usr/bin/env python3
"""inline_census.py -- whole-binary CALLER-SIDE census of inline-policy divergence.

THE VEIN (lane BW-2, proven: `?Handle@Rnd@@` 6,416 bytes driven byte-identical,
+0.0606 pp from ONE function).  Retail INLINES a small accessor where we emit a
`bl`.  Two sub-causes:
  (a) our body is too complex to inline (DC3-era logging/assert code retail lacks)
  (b) our body is fine but is DEFINED AFTER its use site (/Ob2 only inlines
      definitions already seen) -- free + safe to fix.

WHY CALLER-SIDE (this is the load-bearing design point).
A tempting detector is "callee F is in the symbol map but has retail bl in-degree
0".  That is CIRCULAR: a function retail inlined *everywhere* has no out-of-line
body at all, so it is absent from the map entirely.  Verified: retail has ZERO map
entries for `Rnd::SetPostProcOverride` while its sibling `GetPostProcOverride`
exists with in-degree 1.  So we detect from the CALLER:

    for each paired function P at retail VA A with retail size S:
        ours_bl   = # bl instructions in OUR compiled body of P
        retail_bl = # bl instructions in retail [A, A+S)
        delta     = ours_bl - retail_bl        # > 0  =>  retail inlined something

`delta` is a PURE INSTRUCTION COUNT.  It needs no name resolution, so it is
immune to the symbol map covering only ~27k of retail's ~57k functions.  Name
attribution (which callee) is a SECONDARY best-effort pass used only to rank.

RANKING KEY is bytes-in-callers, not site count.  Per the standing
"site count != defect count" discipline, fan-out is blast radius, never yield:
one 6,416-byte caller beat 300 small rows.

TRAPS OBEYED
  * band.exe `.text` is RVA 0x270000 / raw 0x264E00 (0xB200 skew).  We go through
    the PE section table via anon_reloc_cmp.Retail, which self-tests the anchor
    off(0x824DAAD0)==0x004CF8D0.  `va-0x82000000` is valid ONLY for .rdata.
  * We decode `bl` from the INSTRUCTION ENCODING ((w & 0xFC000003)==0x48000001),
    never from a printed dtk label -- dtk mislabels internal branches.
  * We never read build/45410914/asm/ (~13,082 .s files, only ~952 live; stale
    ones are 0x6A00 off and disagree on bytes at the same VA).

Usage:
  scripts/harvest/inline_census.py --proj <worktree> [--out FILE] [--min-size N]
  scripts/harvest/inline_census.py --proj <worktree> --sym '?Handle@Rnd@@...'
"""
import argparse
import importlib.util
import json
import os
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

BL_MASK = 0xFC000003
BL_VAL = 0x48000001          # bl: opcode 18, AA=0, LK=1


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


def bl_targets(code: bytes, base_va: int):
    """-> [(byte_offset, target_va)] for every bl in `code` (big-endian PPC)."""
    out = []
    for i in range(len(code) // 4):
        w = struct.unpack_from('>I', code, i * 4)[0]
        if (w & BL_MASK) == BL_VAL:
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            out.append((i * 4, (base_va + i * 4 + li) & 0xFFFFFFFF))
    return out


def func_bodies_with_reloc_offsets(C, path):
    """-> {func_name: (body_bytes, [(offset_within_func, target_sym_name, reltype)])}

    coff_func_bodies.func_reloc_names() drops the reloc OFFSET, but we need it to
    attribute a callee name to a specific `bl` site.  Same section/symbol walk,
    offsets retained.
    """
    d, secs, syms, idx = C.parse(path)
    out, bysec = {}, {}
    for (name, val, secnum, typ, sc, si) in syms:
        if secnum <= 0 or secnum > len(secs):
            continue
        if sc not in (2, 3):                       # EXTERNAL / STATIC
            continue
        if not secs[secnum - 1]['name'].startswith('.text'):
            continue
        bysec.setdefault(secnum, []).append((val, name))
    for secnum, ents in bysec.items():
        sec = secs[secnum - 1]
        ents.sort()
        rels = []
        for r in range(sec['nrel']):
            o = sec['relptr'] + r * 10
            va, symidx, rtyp = struct.unpack_from('<IIH', d, o)
            s = idx.get(symidx)
            rels.append((va, s[0] if s else '?%d' % symidx, rtyp))
        rels.sort()
        for k, (off, name) in enumerate(ents):
            end = ents[k + 1][0] if k + 1 < len(ents) else sec['rawsz']
            body = d[sec['rawptr'] + off: sec['rawptr'] + end]
            rn = [(va - off, nm, t) for (va, nm, t) in rels if off <= va < end]
            prev = out.get(name)
            if prev is None or len(body) > len(prev[0]):
                out[name] = (body, rn)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--proj', required=True)
    ap.add_argument('--out', default=os.path.expanduser('~/tmp/laneBX2/inline_census.json'))
    ap.add_argument('--min-size', type=int, default=0)
    ap.add_argument('--sym', default=None, help='single-function dump')
    args = ap.parse_args()

    proj = Path(args.proj)
    harvest = proj / 'scripts' / 'harvest'
    A = _load('anon_reloc_cmp', harvest / 'anon_reloc_cmp.py')
    C = _load('coff_func_bodies', harvest / 'coff_func_bodies.py')

    retail = A.Retail(proj / 'orig' / '45410914' / 'band.exe')
    print('band.exe anchor OK', file=sys.stderr)

    # --- symbol map: retail VA <-> mangled name -------------------------------
    raw = json.load(open(proj / 'scripts' / 'target_symbol_map.json'))
    a2s, s2a = {}, {}
    for k, v in raw.items():
        if not k.startswith('0x') or not isinstance(v, str):
            continue
        va = int(k, 16)
        a2s[va] = v
        s2a.setdefault(v, va)
    print(f'map: {len(a2s)} addressed rows', file=sys.stderr)

    report = json.load(open(proj / 'build' / '45410914' / 'report.json'))

    rows = []
    obj_cache = {}
    n_units = n_join = n_nosize = 0

    for unit in report['units']:
        md = unit.get('metadata') or {}
        sp = md.get('source_path')
        if not sp or not unit.get('functions'):
            continue
        obj = proj / 'build' / '45410914' / sp.replace('.cpp', '.obj').replace('.c', '.obj')
        if not obj.exists():
            continue
        n_units += 1
        try:
            ours = func_bodies_with_reloc_offsets(C, str(obj))
        except Exception as e:               # noqa: BLE001
            print(f'  !! {obj.name}: {e}', file=sys.stderr)
            continue
        obj_cache[unit['name']] = True

        for fn in unit['functions']:
            name = fn['name']
            if args.sym and name != args.sym:
                continue
            if name.startswith('__unwind$') or name.startswith('$'):
                continue
            size = int(fn.get('size') or 0)
            if size < args.min_size or size == 0:
                n_nosize += 1
                continue
            va = s2a.get(name)
            if va is None and name.startswith('fn_'):
                try:
                    va = int(name[3:], 16)
                except ValueError:
                    va = None
            if va is None:
                continue
            got = ours.get(name)
            if got is None:
                continue
            body, rels = got
            n_join += 1

            rcode = retail.read(va, size)
            if rcode is None:
                continue

            our_bl = bl_targets(body, 0)          # base irrelevant; we want offsets
            ret_bl = bl_targets(rcode, va)

            # our callee name = the relocation sitting on the bl instruction
            rel_at = {o: nm for (o, nm, t) in rels}
            our_names = [rel_at.get(o) for (o, _t) in our_bl]

            rows.append(dict(
                unit=unit['name'], fn=name, size=size,
                pct=fn.get('fuzzy_match_percent'),
                our_bl=len(our_bl), ret_bl=len(ret_bl),
                delta=len(our_bl) - len(ret_bl),
                ret_names=[a2s.get(t) for (_o, t) in ret_bl],
                our_names=our_names,
            ))

    print(f'units scanned {n_units}  functions joined {n_join}', file=sys.stderr)
    json.dump(rows, open(args.out, 'w'))
    print(f'wrote {args.out}  rows={len(rows)}', file=sys.stderr)

    for r in rows:
        if not isinstance(r.get('pct'), (int, float)):
            r['pct'] = -1.0

    pos = [r for r in rows if r['delta'] > 0]
    neg = [r for r in rows if r['delta'] < 0]
    eq = [r for r in rows if r['delta'] == 0]
    print(f'\nCENSUS  delta>0 (retail inlined / we call): {len(pos)}'
          f'   delta<0 (we inlined / retail calls): {len(neg)}'
          f'   equal: {len(eq)}')
    print(f'BLAST RADIUS bytes-in-callers, delta>0: {sum(r["size"] for r in pos):,}'
          '  <-- NOT yield; a function only pays code% at raw fuzzy 100')

    # SHIP GATE: the inline defect must plausibly be the ONLY remaining defect.
    # BW-2's proven case sat at 99.713% normalized with exactly one cluster.
    ship = [r for r in pos if r['pct'] >= 97.0 and r['delta'] <= 3]
    ship.sort(key=lambda r: -r['size'])
    print(f'\nSHIP GATE (pct>=97, delta<=3): {len(ship)} fns, '
          f'{sum(r["size"] for r in ship):,} bytes-in-callers')
    for r in ship[:30]:
        print(f'  {r["size"]:7,}B  d={r["delta"]:+3d}  {r["pct"]:7.3f}%  '
              f'{r["unit"][:34]:34s} {r["fn"][:66]}')

    # Force-multiplier view: which CALLEE do we emit that retail's counterpart
    # does not?  Ranked by summed caller bytes (the ranking key that matters).
    agg = defaultdict(lambda: [0, 0, []])
    for r in pos:
        rn = Counter(n for n in r['ret_names'] if n)
        on = Counter(n for n in r['our_names'] if n)
        residue = on - rn
        for callee, k in residue.items():
            a = agg[callee]
            a[0] += r['size']
            a[1] += k
            a[2].append((r['fn'], r['size'], r['pct']))
    ranked = sorted(agg.items(), key=lambda kv: -kv[1][0])
    print('\nTOP 30 CALLEES we emit that retail does not (by summed caller bytes):')
    for callee, (b, k, callers) in ranked[:30]:
        inmap = 'IN-MAP ' if callee in s2a else 'ABSENT '
        print(f'  {b:8,}B  sites={k:3d}  callers={len(callers):3d}  {inmap}{callee[:78]}')
    json.dump([dict(callee=c, caller_bytes=b, sites=k, in_retail_map=(c in s2a),
                    callers=cl) for c, (b, k, cl) in ranked],
              open(args.out.replace('.json', '_callees.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
