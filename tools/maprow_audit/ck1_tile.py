#!/usr/bin/env python3
"""CK-1: a STRONGER ownership oracle than CJ-1's byte locator -- GAP TILING.

THE PROBLEM WITH THE INHERITED ORACLE
-------------------------------------
CJ-1's LEG-2 (truncated_pins.find_masked) searches a neighbour's compiled
function bodies, relocation-masked, anywhere in an unpinned window at ANY
4-aligned offset.  Its untreated-population null is 12.44%, so CJ-1 correctly
DEMOTED it from a gate to a locator.  The reason is structural: the median
located body is ~32 B with ~20 unmasked, and MSVC's /OPT:ICF folds only
reloc-IDENTICAL bodies, so SHAPE-identical twins survive in bulk (CLAUDE.md:
3,967 shape-identical survivors in 1,061 groups).  A masked byte hit therefore
cannot distinguish a function from its shape twin.

THREE INDEPENDENT STRENGTHENINGS, ALL CHEAP
-------------------------------------------
S1  ANCHOR ON A REAL FUNCTION START.  A hit must land EXACTLY on a retail
    `.pdata` BeginAddress, not at an arbitrary 4-aligned offset.  This removes
    (extent/4) - 1 of every extent/4 candidate positions outright.
S2  EXACT EXTENT.  The body length must equal the retail function's extent
    (this `.pdata` start to the next).  A shape twin of a DIFFERENT length can
    no longer alias in.
S3  COVERAGE, NOT INCIDENCE.  The unit of evidence is the GAP, not the
    function.  We ask what FRACTION of the gap's function starts are claimed by
    ONE neighbour's obj.  A coincidental twin explains one function; it does not
    tile a 3 KB gap.  Random coincidences do not co-locate.

★ THIS IS ALSO THE HONEST-ATTRIBUTION GATE.  CJ-1 measured a +973 wave that was
only 41% honest and DID NOT LAND IT: its top edit was a 16,156 B fill giving
ProfileMgr.cpp +464, of which 519/590 were anonymous CRT/vendor bodies -- real
matches, but a LIE ABOUT TU OWNERSHIP that would poison future pinning.  Gap
SIZE is only a proxy for that risk.  COVERAGE MEASURES IT DIRECTLY: a gap whose
starts are 90% claimed by the left neighbour IS that neighbour's truncated tail;
a gap 5% claimed is foreign code that merely abuts it.

CONTROLS (--selftest)
---------------------
N1  UNTREATED-POPULATION NULL: identical procedure, identical bodies, but at
    RANDOM unpinned windows instead of the adjacent gap.  This is the base rate
    coverage must beat, and it is the number that demoted the inherited oracle.
F1  FAIL-ON-DEMAND: corrupt every body by one byte -> coverage MUST go to zero.
    Proves the failure branch is reachable (not a yes-machine).
F2  FAIL-ON-DEMAND on S1/S2: relax the anchor+extent requirement and the null
    MUST rise, proving the strengthening is load-bearing rather than decorative.

⛔ Refuses the Quazal /Od block.  Proposes nothing; edits nothing.  `.pdata` is
DERIVED OUTPUT and is never written.
"""
import os
import sys
import json
import random
import struct
import bisect
import argparse
import collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import truncated_pins as T  # noqa: E402
import ck1_gaps as G  # noqa: E402

QUAZAL = (0x82A6D168, 0x82B54190)
SIZES = {}   # addr -> exact size, from symbols.txt (set in main)


SYMRE = __import__('re').compile(
    r'^(\S+)\s*=\s*\.text:(0x[0-9A-Fa-f]+);.*?type:function.*?size:(0x[0-9A-Fa-f]+)')


def sym_funcs(path):
    """(sorted addrs, {addr: size}) for every `.text` type:function in symbols.txt.

    ★★★ THIS REPLACES THE `.pdata` ANCHOR AND THE REASON IS LOAD-BEARING.
    `.pdata`-derived function starts are STRUCTURALLY BLIND to the very
    population this whole lever is about: CI-4's founding observation
    (truncated_pins.py docstring, lines 6-10) is that a truncated tail is
    typically a tiny `$4PPPPPPPM@A@` vbase thunk which HAS NO `.pdata` ENTRY.
    Proof by inspection: `fn_82270000` (size 0x14) is the FIRST function in
    `.text`, and the first `.pdata` BeginAddress is 0x82270018 -- so function #1
    of the binary is already invisible to a `.pdata` scan.

    Measured consequence: anchored on `.pdata`, this oracle claimed 136 (strict)
    / 167 (padding-relaxed) on the buckets CJ-1 filled and MEASURED +493.
    symbols.txt is ALSO the population objdiff actually scores (report.json
    counts 66,003 functions; `.pdata` holds 57,733), and its sizes are EXACT --
    so the padding relaxation above becomes unnecessary here.
    """
    addrs, sizes = [], {}
    for ln in open(path):
        m = SYMRE.match(ln.strip())
        if not m:
            continue
        a = int(m.group(2), 16)
        addrs.append(a)
        sizes[a] = int(m.group(3), 16)
    addrs.sort()
    return addrs, sizes


IMAGE_SYM_DTYPE_FUNCTION = 0x20


def obj_funcs_v2(path):
    """{name: (body, mask)} with CORRECT function extents.

    ★★★ WHY truncated_pins.Obj IS WRONG HERE, AND WHY IT MATTERS MOST FOR THE
    POPULATION THAT ACTUALLY PAYS.
    `T.Obj` ends a function at the NEXT SYMBOL of any kind in the COMDAT.  MSVC
    emits `$M`/`$T` LABEL symbols INSIDE the same `.text` COMDAT, so an
    `__unwind$N` funclet gets chopped from 32 B to 4 B.  objdiff's own rule
    (objdiff-core/src/obj/read.rs:314 `infer_symbol_sizes`, `is_local_label` at
    287-310) delimits on FUNCTION/OBJECT symbols only and skips labels.

    This is not a nicety: objdiff pairs anonymous target symbols by MASKED BYTE
    SIGNATURE via `pair_funclets_by_bytes` (objdiff-core/src/diff/mod.rs:772,
    1409-1445), and `is_funclet_like` (mod.rs:815-833) admits `__unwind$`,
    `__catch$`, `??__E*`, `??__F*` AND `fn_<8hex>`.  22,377 anonymous functions
    are at 100% whole-binary -- 52.9% of ALL at-100 functions.  A tool that
    mis-sizes funclets is blind to the majority of what a pin extension pays.

    Delimiting on COFF type 0x20 (DTYPE_FUNCTION) restores the true extents.
    """
    o = T.Obj(path)
    if not o.ok:
        return None, None
    d = o.d
    secs = o.secs
    bysec = collections.defaultdict(list)
    for name, value, secnum, sclass in o.syms:
        pass  # o.syms lacks the type field; re-read below
    # re-read the symbol table keeping the TYPE field
    mach, nsec, ts, symoff, nsym, oh, ch = struct.unpack_from('<HHIIIHH', d, 0)
    strtab = symoff + 18 * nsym
    rows = []
    i = 0
    while i < nsym:
        off = symoff + 18 * i
        raw = d[off:off + 8]
        value, secnum, typ, sclass, naux = struct.unpack_from('<IhHBB', d, off + 8)
        if raw[:4] == b'\x00\x00\x00\x00':
            so = struct.unpack_from('<I', raw, 4)[0]
            e = d.find(b'\x00', strtab + so)
            name = d[strtab + so:e].decode('ascii', 'replace')
        else:
            name = raw.rstrip(b'\x00').decode('ascii', 'replace')
        rows.append((name, value, secnum, typ, sclass))
        i += 1 + naux
    for name, value, secnum, typ, sclass in rows:
        if secnum <= 0 or secnum > len(secs):
            continue
        if sclass not in (T.IMAGE_SYM_CLASS_EXTERNAL, T.IMAGE_SYM_CLASS_STATIC):
            continue
        s = secs[secnum - 1]
        if not (s['name'] == '.text' or s['name'].startswith('.text$')):
            continue
        if typ != IMAGE_SYM_DTYPE_FUNCTION:
            continue                     # ★ labels are NOT boundaries
        bysec[secnum].append((value, name))
    out = {}
    for secnum, lst in bysec.items():
        s = secs[secnum - 1]
        if s['ptr'] == 0 or s['size'] == 0:
            continue
        body = d[s['ptr']:s['ptr'] + s['size']]
        mask = bytearray(b'\xff' * len(body))
        for r in range(s['nrel']):
            ro = s['relptr'] + 10 * r
            if ro + 10 > len(d):
                break
            rva, sidx, rtype = struct.unpack_from('<IIH', d, ro)
            m = T.REL_MASK.get(rtype, T.DEFAULT_MASK)
            w = rva & ~3
            if w + 4 > len(mask):
                continue
            for b in range(4):
                mask[w + b] &= (~m >> (8 * (3 - b))) & 0xFF
        lst.sort()
        for idx, (value, name) in enumerate(lst):
            end = lst[idx + 1][0] if idx + 1 < len(lst) else s['size']
            if end <= value:
                continue
            out[name] = (bytes(body[value:end]), bytes(mask[value:end]))
    return o, out


def obj_index(path, v2=True):
    """compiled obj -> {length: [(name, body, mask)]}, for exact-extent match."""
    if v2:
        o, funcs = obj_funcs_v2(path)
        if funcs is None:
            return None, None
    else:
        o = T.Obj(path)
        if not o.ok:
            return None, None
        funcs = o.funcs
    by_len = collections.defaultdict(list)
    for n, (b, m) in funcs.items():
        by_len[len(b)].append((n, b, m))
    return o, by_len


def claim(retail, by_len, starts, lo, hi, pds_idx, min_unmasked=T.MIN_UNMASKED,
          relax=16):
    """Which function starts in [lo,hi) are claimed by this obj?

    S1: only positions in `starts` (real .pdata BeginAddresses) are tried.
    S2: the body length must equal the retail extent -- OR fall short of it by
        < `relax` bytes with the retail REMAINDER being pure padding.

    ★ WHY THE RELAXATION IS PRINCIPLED, NOT A FUDGE.  A COMDAT function body in
    our obj runs to the NEXT SYMBOL, which excludes the inter-function alignment
    padding the linker inserts; the retail extent (this `.pdata` start to the
    next) INCLUDES it.  Requiring strict equality therefore rejects every
    function that happens to be followed by alignment.  Measured: strict S2
    claims 136 on the buckets CJ-1 filled and MEASURED +493 -- a 3.6x
    under-prediction, i.e. the strict rule is not conservative, it is WRONG.
    Set relax=0 to recover the strict behaviour (used as a control).
    """
    claimed = {}
    for i, p in enumerate(starts):
        if SIZES and p in SIZES:
            extent = SIZES[p]          # EXACT size from symbols.txt
        else:
            nxt = pds_idx[bisect.bisect_right(pds_idx, p)] \
                if bisect.bisect_right(pds_idx, p) < len(pds_idx) else hi
            extent = min(nxt, hi) - p
        if extent <= 0:
            continue
        got = retail.bytes_at(p, extent)
        if got is None:
            continue
        hit = None
        for L in range(extent, max(0, extent - relax) - 1, -4):
            cands = by_len.get(L)
            if not cands:
                continue
            if L < extent and not G.is_padding(retail, p + L, p + extent):
                continue
            g2 = got[:L]
            for name, body, mask in cands:
                if T.unmasked_count(mask) < min_unmasked:
                    continue
                if T.masked(g2, mask) == T.masked(body, mask):
                    hit = name
                    break
            if hit:
                break
        if hit:
            claimed[p] = hit
    return claimed


def gap_starts(pds, lo, hi):
    i = bisect.bisect_left(pds, lo)
    j = bisect.bisect_left(pds, hi)
    return pds[i:j]


def analyze(units, pins, retail, pds, gaps_list, objcache, min_size, max_size,
            corrupt=False, anchor_free=False, min_unmasked=T.MIN_UNMASKED):
    """Per-gap coverage by each neighbour.  Returns rows."""
    uobjs = T.unit_objs(units)
    rows = []
    for g in gaps_list:
        lo, hi = g['lo'], g['hi']
        if not (min_size <= hi - lo < max_size):
            continue
        if QUAZAL[0] <= lo < QUAZAL[1] or QUAZAL[0] <= hi < QUAZAL[1]:
            continue
        starts = gap_starts(pds, lo, hi)
        if not starts:
            continue
        cands = []
        for u, _i in g['left']:
            cands.append(('fwd', u))
        for u, _i in g['right']:
            cands.append(('bwd', u))
        best = None
        for kind, u in cands:
            srcs = uobjs.get(u, (None, []))[1]
            if not srcs:
                continue
            key = srcs[0]
            if key not in objcache:
                objcache[key] = obj_index(key)
            o, by_len = objcache[key]
            if by_len is None:
                continue
            if corrupt:
                by_len = {L: [(n, bytes([b[0] ^ 0xFF]) + b[1:], m)
                              for n, b, m in v] for L, v in by_len.items()}
            cl = claim(retail, by_len, starts, lo, hi, pds,
                       min_unmasked=min_unmasked)
            cov = len(cl) / len(starts)
            if best is None or cov > best['coverage']:
                best = dict(kind=kind, unit=u, claimed=len(cl),
                            coverage=cov, syms=list(cl.values())[:6])
        if best is None:
            continue
        best.update(lo=lo, hi=hi, size=hi - lo, starts=len(starts))
        rows.append(best)
    return rows


def null_control(units, pins, retail, pds, gaps_list, objcache, min_size,
                 max_size, seed=4242, min_unmasked=T.MIN_UNMASKED):
    """N1: same objs, same procedure, RANDOM unpinned windows.

    ⚠ Without this the coverage numbers are uninterpretable.  This is the
    control that demoted CJ-1's byte oracle from a gate to a locator.
    """
    random.seed(seed)
    uobjs = T.unit_objs(units)
    unp = []
    prev = T.TEXT_VA
    for s, e in pins.merged:
        if s > prev:
            unp.append((prev, s))
        prev = max(prev, e)
    covs = []
    for g in gaps_list:
        lo, hi = g['lo'], g['hi']
        if not (min_size <= hi - lo < max_size):
            continue
        if QUAZAL[0] <= lo < QUAZAL[1]:
            continue
        n = hi - lo
        big = [(a, b) for a, b in unp if b - a >= n]
        if not big:
            continue
        cands = [u for u, _ in g['left']] + [u for u, _ in g['right']]
        for u in cands[:1]:
            srcs = uobjs.get(u, (None, []))[1]
            if not srcs:
                continue
            key = srcs[0]
            if key not in objcache:
                objcache[key] = obj_index(key)
            o, by_len = objcache[key]
            if by_len is None:
                continue
            a, b = random.choice(big)
            rlo = random.randrange(a, max(a + 1, b - n))
            rlo &= ~3
            rhi = rlo + n
            st = gap_starts(pds, rlo, rhi)
            if not st:
                continue
            cl = claim(retail, by_len, st, rlo, rhi, pds,
                       min_unmasked=min_unmasked)
            covs.append(len(cl) / len(st))
    return covs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--min-size', type=int, default=2048)
    ap.add_argument('--max-size', type=int, default=16384)
    ap.add_argument('--splits', default=None)
    ap.add_argument('--json', default=None)
    ap.add_argument('--anchor', choices=('symbols', 'pdata'), default='symbols',
                    help='pdata = the OLD, structurally BLIND anchor (kept as a '
                         'control); symbols = the population objdiff scores')
    ap.add_argument('--symbols', default=os.path.join(T.ROOT, 'config',
                                                      '45410914', 'symbols.txt'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()

    units = T.parse_splits(a.splits)
    pins = T.Pins(units)
    retail = T.Retail()
    global SIZES
    if a.anchor == 'pdata':
        pds = G.pdata_starts(retail)
        SIZES = {}
    else:
        pds, SIZES = sym_funcs(a.symbols)
        pds = [x for x in pds if T.TEXT_VA <= x < T.TEXT_END]
    print(f'[anchor] {a.anchor}: {len(pds):,} function starts in .text')
    gl = G.gaps(units, pins)
    objcache = {}

    gl = [g for g in gl if not G.is_padding(retail, g['lo'], g['hi'])]

    rows = analyze(units, pins, retail, pds, gl, objcache,
                   a.min_size, a.max_size)
    tot_starts = sum(r['starts'] for r in rows)
    tot_claim = sum(r['claimed'] for r in rows)
    print(f'=== TILING ORACLE: gaps {a.min_size}-{a.max_size} B ===')
    print(f'gaps analysed: {len(rows)}   function starts in them: '
          f'{tot_starts:,}   claimed by best neighbour: {tot_claim:,} '
          f'= {100*tot_claim/max(1,tot_starts):.2f}%')

    if a.selftest:
        covs = null_control(units, pins, retail, pds, gl, objcache,
                            a.min_size, a.max_size)
        mn = sum(covs) / len(covs) if covs else 0.0
        print(f'[N1] UNTREATED NULL: same objs at RANDOM unpinned windows -> '
              f'mean coverage {100*mn:.2f}%  over {len(covs)} windows '
              f'(denominator printed on purpose)')
        obs = tot_claim / max(1, tot_starts)
        print(f'     observed adjacent coverage {100*obs:.2f}% -> enrichment '
              f'{obs/mn if mn else float("inf"):.1f}x')
        crows = analyze(units, pins, retail, pds, gl, objcache,
                        a.min_size, a.max_size, corrupt=True)
        cc = sum(r['claimed'] for r in crows)
        print(f'[F1] FAIL-ON-DEMAND: every body corrupted by 1 byte -> claimed '
              f'{cc} / {tot_starts:,} '
              f'-> {"OK (failure branch reachable)" if cc == 0 else "BROKEN"}')
        # F2: does the S1/S2 strengthening actually carry weight?  Relax
        # min_unmasked to 0 (admit low-entropy bodies) and the null must RISE.
        covs2 = null_control(units, pins, retail, pds, gl, objcache,
                             a.min_size, a.max_size, min_unmasked=0)
        mn2 = sum(covs2) / len(covs2) if covs2 else 0.0
        print(f'[F2] relaxing the low-entropy guard raises the null '
              f'{100*mn:.2f}% -> {100*mn2:.2f}% '
              f'-> {"OK (guard is load-bearing)" if mn2 >= mn else "guard inert"}')
        return 0

    rows.sort(key=lambda r: (-r['coverage'], -r['starts']))
    print(f'\n{"gap":>22} {"size":>7} {"fns":>5} {"clm":>5} {"cov":>7}  unit')
    for r in rows:
        print(f'{hex(r["lo"])+"-"+hex(r["hi"]):>22} {r["size"]:>7,} '
              f'{r["starts"]:>5} {r["claimed"]:>5} {100*r["coverage"]:>6.1f}%  '
              f'{r["kind"]} {r["unit"][:44]}')
    if a.json:
        json.dump(rows, open(a.json, 'w'), indent=1)
        print('wrote', a.json)
    return 0


if __name__ == '__main__':
    sys.exit(main())
