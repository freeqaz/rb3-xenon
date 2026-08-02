#!/usr/bin/env python3
"""MAP-INDEPENDENT sweep for TRUNCATED PINS (lane CJ-1, queue #292).

WHY THIS EXISTS
---------------
Lane CI-4 (0eb2d0b5) proved that a unit's `.text` block frequently stops ONE
FUNCTION SHORT of a trailing member -- typically a tiny `$4PPPPPPPM@A@` vbase
thunk, which HAS NO `.pdata` ENTRY and is therefore invisible to every
`.pdata`-driven size tool.  Extending the pin recovers not just that function
but the WHOLE TRUNCATED TAIL: CI-4 predicted +8/+12 and measured +19/+25.

CI-4 LOCATED truncations via MAP ROWS (an address in target_symbol_map.json
naming a class whose defining unit is pinned nearby).  That bounds the sweep to
the map's 27.5k identified addresses.  This tool is MAP-INDEPENDENT: it never
reads target_symbol_map.json.

THE TWO LEGS (independent by construction)
------------------------------------------
LEG 1 -- COFF base-only symbols.  For unit U we take the function symbols
DEFINED (SectionNumber>0) in OUR COMPILED obj and subtract those defined in the
dtk-split TARGET obj.  A function truncated out of the pin is necessarily in
this set, so LEG 1 is a SUPERSET prefilter: it cannot produce false negatives.
It is deliberately noisy (it also contains functions that are inside the pin but
unnamed in the map, and functions retail never emitted at all).

LEG 2 -- RELOC-MASKED RETAIL BYTE MATCH.  For each LEG-1 candidate we take the
function's bytes out of our COMDAT `.text` section, mask every relocation FIELD
(24-bit branch displacement, 16-bit hi/lo, 32-bit absolute), and scan the
UNPINNED bytes adjacent to U's block edges in retail `band.exe` for a byte-equal
window under the SAME mask.  A hit is direct evidence that retail placed that
exact function there and that our source reproduces it.

★ LEG 2 IS ALSO A YIELD PREDICTOR, NOT JUST A LOCATOR.  A masked byte match is
(modulo relocation targets) the same condition objdiff scores as 100%.  So the
number of LEG-2 hits swept in by an extension is a PREDICTION of the matched
delta -- which is the instrument CI-4 lacked when it under-predicted twice.

★ ADJACENCY IS OWNERSHIP EVIDENCE.  Retail RB3 has no whole-program
optimization, so TU spatial grouping in `.text` is preserved (CLAUDE.md).  A
function of U's that sits in unpinned space touching U's block edge belongs to
U's TU.

★★★ SPLITS RANGES ARE HALF-OPEN [start, end).  An inclusive-end read
manufactures a phantom defect class -- 2,748 pin-end addresses hold a map row,
988 naming a foreign class, and those are the pin CORRECTLY stopping where a
foreign TU begins.  Every interval test here is `s <= va < e`.

CONTROLS (see --selftest)
-------------------------
C0  geometry sanity (block count / covered bytes)
C1  VA->file mapping proven on a KNOWN vbase thunk at 0x8261FFC8
C2  fail-on-demand: a corrupted mask must STOP matching (failure branch reached)
C3  fail-on-demand: a bogus symbol must be absent from the obj index
C4  UNTREATED-POPULATION NULL: the same masked patterns searched in RANDOM
    unpinned windows -- the base rate any adjacency claim must beat
C5  low-entropy guard: patterns with too few unmasked bytes are REFUSED, since a
    12-byte thunk with its branch masked is 8 bytes of near-boilerplate

WHAT THIS TOOL DELIBERATELY DOES NOT DO
---------------------------------------
It does not adjudicate map rows and it does not edit the map.  It proposes
`.text` extensions only.  `.pdata` is DERIVED OUTPUT and is never touched.
"""
import os
import re
import sys
import json
import random
import struct
import bisect
import argparse
import collections

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.environ.get('CJ1_ROOT') or os.path.abspath(os.path.join(HERE, '..', '..'))
BUILD = os.path.join(ROOT, 'build', '45410914')
SPLITS = os.path.join(ROOT, 'config', '45410914', 'splits.txt')
BANDEXE = os.path.join(ROOT, 'orig', '45410914', 'band.exe')

# band.exe is the DECOMPRESSED RETAIL PE.  .text RVA 0x270000 / raw 0x264E00.
# ⚠ the naive `va - 0x82000000` file offset is valid ONLY for .rdata.
IMAGE_BASE = 0x82000000
TEXT_VA = IMAGE_BASE + 0x270000
TEXT_RAW = 0x264E00
TEXT_SIZE = 0x9DCE3C
TEXT_END = TEXT_VA + TEXT_SIZE

IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3
IMAGE_SYM_CLASS_WEAK_EXTERNAL = 105

# IMAGE_REL_PPC_*  -> width of the field the linker rewrites.
REL_MASK = {
    0x0001: 0xFFFFFFFF,  # ADDR32
    0x0002: 0x03FFFFFC,  # ADDR24
    0x0003: 0x0000FFFF,  # ADDR16
    0x0004: 0x0000FFFC,  # ADDR14
    0x0005: 0x03FFFFFC,  # REL24
    0x0006: 0x0000FFFC,  # REL14
    0x000A: 0x0000FFFF,  # TOCREL16
    0x000B: 0x0000FFFC,  # TOCREL14
    0x000F: 0xFFFFFFFF,  # ADDR32NB
    0x0010: 0xFFFFFFFF,  # SECREL
    0x0011: 0x0000FFFF,  # SECTION
    0x0014: 0x0000FFFF,  # SECREL16
    0x0015: 0x0000FFFF,  # REFHI
    0x0016: 0x0000FFFF,  # REFLO
    0x0017: 0xFFFFFFFF,  # PAIR
    0x0018: 0x0000FFFF,  # SECRELLO
}
DEFAULT_MASK = 0xFFFFFFFF  # unknown reloc type -> mask the whole word (safe)

MIN_UNMASKED = 12   # C5 low-entropy guard: >= 3 fully-unmasked instructions
MIN_SIZE = 8


# ------------------------------------------------------------------ splits
def parse_splits(path=None):
    """unit -> [(start,end)] for .text only.  Half-open [start,end)."""
    path = path or SPLITS
    units = collections.OrderedDict()
    unit = None
    for ln in open(path):
        s = ln.rstrip('\n')
        if not s.strip() or s.lstrip().startswith('#'):
            continue
        if not s.startswith((' ', '\t')) and s.rstrip().endswith(':'):
            unit = s.strip()[:-1]
            if unit != 'Sections':
                units.setdefault(unit, [])
            else:
                unit = None
        elif unit is not None and s.strip().startswith('.text'):
            m = re.search(r'start:(0x[0-9a-fA-F]+)\s+end:(0x[0-9a-fA-F]+)', s)
            if m:
                units[unit].append((int(m.group(1), 16), int(m.group(2), 16)))
    return units


class Pins:
    """Union of all pinned .text intervals + per-unit blocks."""

    def __init__(self, units):
        self.units = units
        self.flat = []
        for u, rs in units.items():
            for s, e in rs:
                self.flat.append((s, e, u))
        self.flat.sort()
        # merged union, for unpinned-gap queries
        merged = []
        for s, e, _u in self.flat:
            if merged and s <= merged[-1][1]:
                merged[-1][1] = max(merged[-1][1], e)
            else:
                merged.append([s, e])
        self.merged = [tuple(x) for x in merged]
        self._starts = [s for s, _ in self.merged]

    def is_pinned(self, va):
        i = bisect.bisect_right(self._starts, va) - 1
        return i >= 0 and self.merged[i][0] <= va < self.merged[i][1]

    def next_pinned_start(self, va):
        i = bisect.bisect_right(self._starts, va)
        return self.merged[i][0] if i < len(self.merged) else TEXT_END

    def prev_pinned_end(self, va):
        i = bisect.bisect_right(self._starts, va) - 1
        return self.merged[i][1] if i >= 0 else TEXT_VA

    def forward_window(self, e, maxw):
        """Unpinned bytes immediately AFTER block end e (half-open)."""
        if self.is_pinned(e):
            return (e, e)
        return (e, min(e + maxw, self.next_pinned_start(e), TEXT_END))

    def backward_window(self, s, maxw):
        """Unpinned bytes immediately BEFORE block start s."""
        if s <= TEXT_VA:
            return (s, s)
        if self.is_pinned(s - 1):
            return (s, s)
        return (max(s - maxw, self.prev_pinned_end(s - 1), TEXT_VA), s)


# ------------------------------------------------------------------ COFF
class Obj:
    """Minimal COFF reader: function symbols with bytes + relocation masks."""

    def __init__(self, path):
        self.path = path
        with open(path, 'rb') as f:
            self.d = d = f.read()
        self.ok = len(d) >= 20
        self.funcs = {}      # name -> (bytes, maskbytes)
        self.defined = set()
        if not self.ok:
            return
        mach, nsec, ts, symoff, nsym, oh, ch = struct.unpack_from('<HHIIIHH', d, 0)
        if symoff == 0 or nsym == 0 or symoff + 18 * nsym > len(d):
            self.ok = False
            return
        strtab = symoff + 18 * nsym
        # sections
        secs = []
        off = 20 + oh
        for _ in range(nsec):
            name = d[off:off + 8].rstrip(b'\x00').decode('ascii', 'replace')
            vs, va, rawsz, rawptr, relptr, lnptr, nrel, nln, flags = \
                struct.unpack_from('<IIIIIIHHI', d, off + 8)
            secs.append(dict(name=name, size=rawsz, ptr=rawptr, relptr=relptr,
                             nrel=nrel, flags=flags))
            off += 40
        self.secs = secs
        # symbols
        syms = []
        i = 0
        while i < nsym:
            o = symoff + 18 * i
            raw = d[o:o + 8]
            value, secnum, typ, sclass, naux = struct.unpack_from('<IhHBB', d, o + 8)
            if raw[:4] == b'\x00\x00\x00\x00':
                so = struct.unpack_from('<I', raw, 4)[0]
                e = d.find(b'\x00', strtab + so)
                name = d[strtab + so:e].decode('ascii', 'replace')
            else:
                name = raw.rstrip(b'\x00').decode('ascii', 'replace')
            syms.append((name, value, secnum, sclass))
            i += 1 + naux
        self.syms = syms
        # ⚠ a substring scan cannot prove a symbol is DEFINED -- an undefined
        # external reference carries the same bytes.  SectionNumber>0 is the
        # only definition test.  WEAK_EXTERNAL (105) is NOT a definition.
        for name, value, secnum, sclass in syms:
            if secnum > 0 and sclass in (IMAGE_SYM_CLASS_EXTERNAL,
                                         IMAGE_SYM_CLASS_STATIC):
                self.defined.add(name)

        # group function symbols by their .text COMDAT section
        bysec = collections.defaultdict(list)
        for name, value, secnum, sclass in syms:
            if secnum <= 0 or secnum > len(secs):
                continue
            if sclass not in (IMAGE_SYM_CLASS_EXTERNAL, IMAGE_SYM_CLASS_STATIC):
                continue
            s = secs[secnum - 1]
            if not s['name'].startswith('.text'):
                continue
            if s['name'] == '.text' or s['name'].startswith('.text$'):
                bysec[secnum].append((value, name))
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
                m = REL_MASK.get(rtype, DEFAULT_MASK)
                w = rva & ~3
                if w + 4 > len(mask):
                    continue
                for b in range(4):
                    keep = (~m >> (8 * (3 - b))) & 0xFF
                    mask[w + b] &= keep
            lst.sort()
            for idx, (value, name) in enumerate(lst):
                end = lst[idx + 1][0] if idx + 1 < len(lst) else s['size']
                if end <= value:
                    continue
                self.funcs[name] = (bytes(body[value:end]),
                                    bytes(mask[value:end]))


def masked(buf, mask):
    return bytes(b & m for b, m in zip(buf, mask))


def unmasked_count(mask):
    return sum(1 for m in mask if m == 0xFF)


def pick_anchor(mask):
    """Offset of a 4-aligned, FULLY UNMASKED word, or None."""
    for k in range(0, (len(mask) // 4) * 4, 4):
        if mask[k] == 0xFF and mask[k + 1] == 0xFF and \
           mask[k + 2] == 0xFF and mask[k + 3] == 0xFF:
            return k
    return None


# ------------------------------------------------------------------ retail
class Retail:
    def __init__(self, path=None):
        with open(path or BANDEXE, 'rb') as f:
            self.d = f.read()

    def bytes_at(self, va, n):
        if va < TEXT_VA or va + n > TEXT_END:
            return None
        o = va - TEXT_VA + TEXT_RAW
        return self.d[o:o + n]

    def find_masked(self, lo, hi, body, mask, limit=64, anchor=None):
        """4-aligned masked-equal windows for `body` in [lo,hi).  Returns VAs.

        PERFORMANCE, and why it is not a correctness shortcut: a naive 4-byte
        step over every window costs ~780M masked compares tree-wide.  Instead
        we take an ANCHOR -- a 4-byte word of the pattern that is FULLY
        UNMASKED (no relocation touches it) -- and let `bytes.find` (C speed)
        propose candidate offsets, then verify the FULL masked compare at each.
        The anchor is a strict SUPERSET filter: any position that masked-equals
        the whole pattern necessarily contains the anchor word at that offset,
        so this cannot introduce false negatives.  If no fully-unmasked word
        exists, we fall back to the exhaustive scan.
        """
        n = len(body)
        want = masked(body, mask)
        out = []
        base = (lo + 3) & ~3
        seg = self.bytes_at(base, max(0, hi - base))
        if not seg:
            return out
        if anchor is None:
            anchor = pick_anchor(mask)
        if anchor is None:
            va = base
            while va + n <= hi:
                got = self.bytes_at(va, n)
                if got is not None and masked(got, mask) == want:
                    out.append(va)
                    if len(out) >= limit:
                        break
                va += 4
            return out
        k = anchor
        word = body[k:k + 4]
        p = seg.find(word)
        while p >= 0:
            if p >= k and (p - k) % 4 == 0:
                st = p - k
                if st + n <= len(seg) and masked(seg[st:st + n], mask) == want:
                    out.append(base + st)
                    if len(out) >= limit:
                        break
            p = seg.find(word, p + 1)
        return out


# ------------------------------------------------------------------ mapping
def unit_objs(units):
    """splits unit key -> (target_obj_path, [compiled_obj_paths])."""
    # compiled objs, indexed by basename AND by repo-relative path suffix
    bybase = collections.defaultdict(list)
    bypath = {}
    srcdir = os.path.join(BUILD, 'src')
    for dirpath, _dirs, files in os.walk(srcdir):
        for fn in files:
            if not fn.endswith('.obj'):
                continue
            p = os.path.join(dirpath, fn)
            rel = os.path.relpath(p, srcdir)
            bybase[fn[:-4]].append(p)
            bypath[rel[:-4].replace(os.sep, '/')] = p
    out = {}
    for u in units:
        stem = re.sub(r'\.(cpp|c|cc|s)$', '', u)
        tgt = os.path.join(BUILD, 'obj', stem + '.obj')
        # ⚠ splits.txt MIXES bare names and full paths.  Try the PATH form
        # first (exact), then fall back to basename.  A basename-only join is
        # what manufactured CI-4's 4,722 phantom "homeless" rows.
        cand = []
        if stem in bypath:
            cand = [bypath[stem]]
        else:
            cand = bybase.get(os.path.basename(stem), [])
        out[u] = (tgt, cand)
    return out


# ------------------------------------------------------------------ sweep
def sweep(maxw=4096, min_unmasked=MIN_UNMASKED, verbose=True, null=True,
          only_units=None):
    units = parse_splits()
    pins = Pins(units)
    retail = Retail()
    uobjs = unit_objs(units)

    stats = collections.Counter()
    rows = []
    tgt_cache = {}

    unit_list = [u for u in units if (only_units is None or u in only_units)]
    for u in unit_list:
        tgtp, srcps = uobjs[u]
        if not srcps or not os.path.exists(tgtp):
            stats['unit_no_obj'] += 1
            continue
        stats['unit_scanned'] += 1
        tgt = tgt_cache.get(tgtp)
        if tgt is None:
            tgt = Obj(tgtp)
            tgt_cache[tgtp] = tgt
        base = Obj(srcps[0])
        if not base.ok:
            stats['unit_bad_obj'] += 1
            continue
        base_only = {n: v for n, v in base.funcs.items()
                     if n not in tgt.defined and len(v[0]) >= MIN_SIZE}
        stats['base_only_total'] += len(base_only)
        if not base_only:
            continue

        # candidate windows: unpinned space touching ANY of U's block edges
        wins = []
        for s, e in units[u]:
            fw = pins.forward_window(e, maxw)
            if fw[1] > fw[0]:
                wins.append(('fwd', s, e, fw))
            bw = pins.backward_window(s, maxw)
            if bw[1] > bw[0]:
                wins.append(('bwd', s, e, bw))
        if not wins:
            stats['unit_no_window'] += 1
            continue

        for name, (body, mask) in base_only.items():
            um = unmasked_count(mask)
            if um < min_unmasked:
                stats['refused_low_entropy'] += 1
                continue
            stats['probed'] += 1
            for kind, bs, be, (lo, hi) in wins:
                hits = retail.find_masked(lo, hi, body, mask, limit=8)
                if not hits:
                    continue
                for h in hits:
                    rows.append(dict(unit=u, sym=name, addr=h, size=len(body),
                                     unmasked=um, kind=kind,
                                     block=(bs, be),
                                     gap=(h - be) if kind == 'fwd' else (bs - (h + len(body))),
                                     nhits=len(hits)))
                stats['located'] += 1
                break
    return units, pins, rows, stats


# ------------------------------------------------------------------ null
def null_control(units, pins, rows, maxw=4096, seed=991, trials=None):
    """C4 UNTREATED-POPULATION NULL.

    Re-run the SAME masked patterns against RANDOM unpinned windows of the SAME
    size, drawn from elsewhere in .text.  This is the base rate that the
    adjacency claim must beat.  ⚠ Without it, "the pattern was found next to the
    block" is uninterpretable -- a low-entropy masked thunk could match almost
    anywhere.
    """
    random.seed(seed)
    retail = Retail()
    uobjs = unit_objs(units)
    # build the list of unpinned intervals of decent size
    unp = []
    prev = TEXT_VA
    for s, e in pins.merged:
        if s > prev:
            unp.append((prev, s))
        prev = max(prev, e)
    if prev < TEXT_END:
        unp.append((prev, TEXT_END))
    big = [(a, b) for a, b in unp if b - a >= maxw]
    if not big:
        return 0, 0
    hits = 0
    n = 0
    objcache = {}
    sample = rows if trials is None else random.sample(rows, min(trials, len(rows)))
    for r in sample:
        tgtp, srcps = uobjs[r['unit']]
        if not srcps:
            continue
        o = objcache.get(srcps[0])
        if o is None:
            o = Obj(srcps[0])
            objcache[srcps[0]] = o
        v = o.funcs.get(r['sym'])
        if not v:
            continue
        body, mask = v
        a, b = random.choice(big)
        lo = random.randrange(a, max(a + 1, b - maxw))
        n += 1
        if retail.find_masked(lo, min(lo + maxw, b), body, mask, limit=1):
            hits += 1
    return hits, n


# ------------------------------------------------------------------ selftest
def selftest():
    ok = True
    print('=== truncated_pins selftest ===')

    units = parse_splits()
    pins = Pins(units)
    cov = sum(e - s for s, e, _ in pins.flat)
    c0 = len(pins.flat) > 4000 and cov > 5_000_000
    print(f'[C0] geometry: {len(units)} units, {len(pins.flat)} .text blocks, '
          f'{cov:,} pinned bytes, {len(pins.merged)} merged runs -> '
          f'{"OK" if c0 else "SUSPECT"}')
    ok &= c0

    # C1 POSITIVE CONTROL on the VA->file mapping, on a KNOWN answer.
    # 0x8261FFC8 is the vbase thunk lane CI-4 recovered by extending a block
    # START backward (0x82620000 -> 0x8261FFC8).  Its bytes must decode as
    #   lwz r11,-4(r4) ; subf r4,r11,r4 ; b <target>
    # ⚠ the naive `va - 0x82000000` offset (valid only for .rdata) gives
    # garbage here -- that is the whole point of the control.
    r = Retail()
    got = r.bytes_at(0x8261FFC8, 12)
    want = bytes.fromhex('8164fffc7c8b2050')
    c1 = got[:8] == want and got[8] & 0xFC == 0x48
    print(f'[C1] VA->file on known thunk 0x8261FFC8: {got.hex()} -> '
          f'{"OK" if c1 else "BROKEN"}')
    naive = r.d[0x8261FFC8 - IMAGE_BASE:0x8261FFC8 - IMAGE_BASE + 8]
    print(f'     naive va-0x82000000 would read {naive.hex()} (differs: '
          f'{naive != want}) -- control is DISCRIMINATING')
    ok &= c1
    ok &= (naive != want)

    # C2 FAIL-ON-DEMAND: corrupt the pattern; the matcher MUST stop finding it.
    # Proves the failure branch is REACHABLE, i.e. the matcher is not a
    # yes-machine.
    body = got
    mask = b'\xff' * 8 + b'\x00\x00\x00\x00'
    hit_good = r.find_masked(0x8261FF00, 0x82620000, body, mask, limit=4)
    bad = bytes([body[0] ^ 0xFF]) + body[1:]
    hit_bad = r.find_masked(0x8261FF00, 0x82620000, bad, mask, limit=4)
    c2 = (0x8261FFC8 in hit_good) and not hit_bad
    print(f'[C2] fail-on-demand matcher: good={[hex(x) for x in hit_good]} '
          f'corrupted={[hex(x) for x in hit_bad]} -> '
          f'{"OK (both branches reached)" if c2 else "BROKEN"}')
    ok &= c2

    # C2b anchor equivalence: the fast anchor path and the exhaustive path
    # must agree.  If the anchor optimisation were a correctness shortcut this
    # would diverge.
    slow = r.find_masked(0x8261FF00, 0x82620000, body, mask, limit=4, anchor=-1) \
        if False else None
    exhaustive = []
    va = 0x8261FF00
    from_ = masked(body, mask)
    while va + len(body) <= 0x82620000:
        g = r.bytes_at(va, len(body))
        if masked(g, mask) == from_:
            exhaustive.append(va)
        va += 4
    c2b = exhaustive == hit_good
    print(f'[C2b] anchor path == exhaustive path: {c2b} -> '
          f'{"OK" if c2b else "BROKEN"}')
    ok &= c2b

    # C3 FAIL-ON-DEMAND on the DEFINITION test.
    uo = unit_objs(units)
    probe_unit = next(u for u in units if os.path.basename(u) == 'CharHair.cpp')
    o = Obj(uo[probe_unit][1][0])
    c3a = any(n.startswith('?') for n in o.funcs)
    c3b = '?ZzNotReal@CJ1@@UAEXXZ' not in o.defined
    print(f'[C3] obj index: {len(o.funcs)} .text function bodies, bogus symbol '
          f'absent={c3b} -> {"OK" if (c3a and c3b) else "BROKEN"}')
    ok &= (c3a and c3b)

    # C5 the low-entropy guard must actually FIRE somewhere, else it is inert.
    fired = sum(1 for n, (b, m) in o.funcs.items()
                if unmasked_count(m) < MIN_UNMASKED)
    print(f'[C5] low-entropy guard fires on {fired}/{len(o.funcs)} bodies in '
          f'the probe unit -> {"OK (guard is live)" if fired > 0 else "INERT"}')

    print('SELFTEST', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


# ------------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--maxw', type=int, default=4096)
    ap.add_argument('--min-unmasked', type=int, default=MIN_UNMASKED)
    ap.add_argument('--json', default=None)
    ap.add_argument('--no-null', action='store_true')
    ap.add_argument('--top', type=int, default=40)
    a = ap.parse_args()
    if a.selftest:
        return selftest()

    units, pins, rows, stats = sweep(maxw=a.maxw, min_unmasked=a.min_unmasked)
    print('--- sweep stats ---')
    for k in sorted(stats):
        print(f'  {k:24s} {stats[k]:,}')
    print(f'  LOCATED rows: {len(rows):,}')

    # group into per-unit extension proposals
    props = collections.defaultdict(list)
    for r0 in rows:
        props[r0['unit']].append(r0)
    print(f'  units with >=1 located function: {len(props):,} / '
          f'{stats["unit_scanned"]:,}')

    if not a.no_null:
        h, n = null_control(units, pins, rows, maxw=a.maxw)
        print(f'--- C4 untreated null: the SAME masked patterns hit a RANDOM '
              f'unpinned window {h}/{n} = {100*h/max(1,n):.2f}%')
        print('    (denominator printed on purpose; adjacency enrichment must '
              'be read against this)')

    if a.json:
        with open(a.json, 'w') as f:
            json.dump(dict(rows=rows, stats=dict(stats)), f, indent=1)
        print('wrote', a.json)
    return 0


if __name__ == '__main__':
    sys.exit(main())
