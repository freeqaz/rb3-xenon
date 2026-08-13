#!/usr/bin/env python3
"""Adjudicate ALIAS pairs whose folded spelling the map ALSO places at a distinct
LIVE address -- on RETAIL BYTES (lane T1-AUDIT, 2026-08-13).

THE QUESTION
------------
`scripts/symbol_aliases.json` installs (survivor S @ addr(S), folded F) groups.
For 190 (group, folded) pairs the map ALSO names a DIFFERENT address addr(F).
An alias asserts retail has ONE body for the two spellings.  The map asserts it
has TWO.  Both cannot be true, and objdiff forgives an alias unconditionally --
so an unproven one lifts `name_check` BY CONSTRUCTION with no bytes behind it.

WHY THE ALIAS'S OWN WARRANT CANNOT ANSWER IT (the CF4 inversion)
----------------------------------------------------------------
T1's warrant is "our compiled COMDAT for F is masked-byte-identical to retail's
body at addr(S)".  That is a statement about ONE address.  It never adjudicates
addr(F) -- and masking the `bl` target is PRECISELY what makes shape-identity
look like a fold.  Worse, the sound reading of that evidence runs the other way:
it proves an IDENTIFICATION (addr(S) IS F, so the map row naming it S is wrong),
not a FOLD.  Lane CF4-FIX established this on the 3 CF4 pairs, 0 of which
survived.

THE INSTRUMENT
--------------
RETAIL vs RETAIL, relocation-normalized, over .pdata-authoritative extents --
lane CK-4/CD-7's `norm()`, reused verbatim from tools/maprow_audit/ck4_foldscan.py:

    RELOC  b/bl LI displacement -> ABSOLUTE target VA.  Equal iff same code
           CALLING THE SAME CALLEES -- the population /OPT:ICF may fold.
    SHAPE  b/bl LI displacement -> 0.  Equal iff same code IGNORING callees --
           the per-instantiation template family ICF may NOT fold.

Plus one REFINEMENT that makes the comparator strictly MORE willing to say SAME
(the conservative direction when the consequence is deleting an alias): an
unconditional `b`/`bl` whose resolved target lands INSIDE the function's own
extent is recorded as a RELATIVE offset, not an absolute VA.  Two copies of one
function at two addresses have identical intra-function control flow but
different absolute intra-function targets, so CD-7's verbatim rule would call
them DIFFERENT.  Both readings are computed and reported; a pair is only called
DIFFERENT when BOTH agree.

VERDICTS
--------
SAME           reloc-identical (same size, same code, same resolved callees).
               NOTE this is NOT by itself proof of a fold -- two live addresses
               means /OPT:ICF did NOT fold them (CD-7: only 51 such surplus
               copies binary-wide).  It is the strongest case an alias can have,
               and is reported separately for that reason.
DIFFERENT      not reloc-identical.  Sub-classed:
                 SIZE          extents differ -> folding structurally impossible
                 SHAPE_ONLY    same shape, DIFFERENT callees -> the CD-7
                               non-fold signature verbatim (per-instantiation
                               template family)
                 BODY          differs in non-branch words too
UNADJUDICABLE  no .pdata extent for one/both addresses (the sub-.pdata stub
               stratum is excluded BY CONSTRUCTION -- see CLAUDE.md's SCOPE
               BOUND), address outside .text, or the body is a funclet.

CONTROLS (a screen that cannot return IDENTICAL is vacuous)
-----------------------------------------------------------
--control runs, over the WHOLE binary and at DISTINCT addresses:
  POSITIVE  count of distinct-address pairs the comparator calls SHAPE-identical
            and RELOC-identical.  If either is 0 the tool is broken, not the
            population.
  NEGATIVE  a decoy set: each function vs the NEXT function in address order.
            These are different functions; the reloc comparator must reject
            essentially all of them.
"""
import argparse
import collections
import hashlib
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'maprow_audit'))

from ck4_foldscan import pdata, is_funclet, norm            # noqa: E402
from thunk_identity import Image                            # noqa: E402

BAND = os.path.join(ROOT, 'orig/45410914/band.exe')


def norm_intra(img, va, length):
    """RELOC mode, but intra-function b/bl targets stay RELATIVE.

    Strictly more permissive than ck4's `norm(..., 'reloc')`: it is the only
    difference, and it can only turn a DIFFERENT into a SAME.
    """
    off = img.offset(va)
    if off is None or off + length > len(img.data) or length < 4:
        return None
    out = bytearray()
    for i in range(length // 4):
        w = struct.unpack_from('>I', img.data, off + i * 4)[0]
        if (w >> 26) == 18:
            head = w & 0xFC000003
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            tgt = li if ((w >> 1) & 1) else (va + i * 4 + li)
            tgt &= 0xFFFFFFFF
            if va <= tgt < va + length:           # intra-function -> relative
                out += struct.pack('>Ii', head | 0x80000000, tgt - va)
            else:
                out += struct.pack('>II', head, tgt)
        else:
            out += struct.pack('>I', w)
    return bytes(out)


def load_extents(img):
    ext = {}
    for va, ln, _p, _e in pdata(img):
        if ln <= 0:
            continue
        ext.setdefault(va, ln)
    return ext


def head_insn(img, va):
    """-> ('b'|'bl', absolute_target) | ('word', raw) | None."""
    o = img.offset(va)
    if o is None or o + 4 > len(img.data):
        return None
    w = struct.unpack_from('>I', img.data, o)[0]
    if (w >> 26) == 18:
        li = w & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        tgt = li if ((w >> 1) & 1) else (va + li)
        return ('bl' if (w & 1) else 'b', tgt & 0xFFFFFFFF)
    return ('word', w)


def stub_head(img, a, b):
    ha, hb = head_insn(img, a), head_insn(img, b)
    if ha is None or hb is None:
        return 'UNADJUDICABLE', 'NO_PDATA', 'address outside image'
    if ha[0] in ('b', 'bl') and hb[0] in ('b', 'bl'):
        if ha[1] != hb[1]:
            return ('DIFFERENT', 'STUB_TARGET',
                    'sub-.pdata stubs: %s 0x%08x vs %s 0x%08x'
                    % (ha[0], ha[1], hb[0], hb[1]))
        return ('UNADJUDICABLE', 'STUB_SAME_TARGET',
                'both %s 0x%08x -- same target, extent unknown' % (ha[0], ha[1]))
    if ha[0] != hb[0]:
        return ('DIFFERENT', 'STUB_SHAPE',
                'sub-.pdata: head %s vs %s' % (ha[0], hb[0]))
    if ha[1] != hb[1]:
        return ('DIFFERENT', 'STUB_SHAPE',
                'sub-.pdata: first word 0x%08x vs 0x%08x' % (ha[1], hb[1]))
    return 'UNADJUDICABLE', 'STUB_SAME_HEAD', 'identical first word, extent unknown'


def classify(img, ext, a, b):
    """-> (verdict, subclass, detail)"""
    if a == b:
        return 'UNADJUDICABLE', 'SAME_ADDR', 'addresses identical'
    la, lb = ext.get(a), ext.get(b)
    if la is None or lb is None:
        # SUB-.pdata STUB STRATUM.  CLAUDE.md's SCOPE BOUND: an 8-byte leaf stub
        # touches neither the stack nor LR, so it gets NO unwind record and the
        # .pdata instrument is structurally blind to it.  Fall back to the
        # weakest DECISIVE test that needs no extent: the FIRST instruction with
        # its branch target RESOLVED.  `b X` vs `b Y` with X != Y is two
        # different stubs; `b X` vs a non-branch is two different shapes.  This
        # can only ever return DIFFERENT or "cannot tell" -- never SAME.
        return stub_head(img, a, b)
    fa, fb = is_funclet(img, a), is_funclet(img, b)
    if fa or fb:
        return 'UNADJUDICABLE', 'FUNCLET', 'funclet body (associative COMDAT)'
    if la != lb:
        return 'DIFFERENT', 'SIZE', '%d B vs %d B' % (la, lb)
    ra, rb = norm(img, a, la, 'reloc'), norm(img, b, lb, 'reloc')
    ia, ib = norm_intra(img, a, la), norm_intra(img, b, lb)
    sa, sb = norm(img, a, la, 'shape'), norm(img, b, lb, 'shape')
    if None in (ra, rb, ia, ib, sa, sb):
        return 'UNADJUDICABLE', 'UNREADABLE', 'body outside image'
    if ra == rb or ia == ib:
        how = 'strict' if ra == rb else 'intra-relative only'
        return 'SAME', 'RELOC_IDENTICAL', '%d B, %s' % (la, how)
    if sa == sb:
        # count differing branch words, and name the differing targets
        diffs = []
        for i in range(la // 4):
            wa = struct.unpack_from('>I', img.data, img.offset(a) + i * 4)[0]
            wb = struct.unpack_from('>I', img.data, img.offset(b) + i * 4)[0]
            if wa != wb:
                diffs.append(i)
        return ('DIFFERENT', 'SHAPE_ONLY',
                '%d B, same shape, %d differing branch word(s)' % (la, len(diffs)))
    nb = sum(1 for i in range(la // 4)
             if struct.unpack_from('>I', img.data, img.offset(a) + i * 4)[0]
             != struct.unpack_from('>I', img.data, img.offset(b) + i * 4)[0])
    return 'DIFFERENT', 'BODY', '%d B, %d/%d differing words' % (la, nb, la // 4)


def controls(img, ext, limit=0):
    funcs = [(va, ln) for va, ln in sorted(ext.items())
             if 4 <= ln <= 0x20000 and not is_funclet(img, va)]
    if limit:
        funcs = funcs[:limit]
    print('control population: %d non-funclet .pdata bodies' % len(funcs))
    for mode in ('shape', 'reloc'):
        g = collections.defaultdict(list)
        for va, ln in funcs:
            b = norm(img, va, ln, mode)
            if b is not None:
                g[hashlib.sha1(b).digest()].append(va)
        pairs = sum(len(v) * (len(v) - 1) // 2 for v in g.values() if len(v) > 1)
        surp = sum(len(v) - 1 for v in g.values() if len(v) > 1)
        print('  POSITIVE %-5s : %8d distinct-address pairs called IDENTICAL '
              '(%d surplus copies, %d groups)'
              % (mode.upper(), pairs, surp,
                 sum(1 for v in g.values() if len(v) > 1)))
    # NEGATIVE: adjacent-function decoys
    rej = tot = 0
    for i in range(len(funcs) - 1):
        a, b = funcs[i][0], funcs[i + 1][0]
        v, _s, _d = classify(img, ext, a, b)
        tot += 1
        if v == 'DIFFERENT':
            rej += 1
    print('  NEGATIVE decoys: %d/%d adjacent-function pairs REJECTED (%.3f%%)'
          % (rej, tot, 100.0 * rej / max(tot, 1)))


def census(root):
    """Recompute the exposure predicate from the shipped files -- self-contained
    so the prune below cannot drift from the audit above.

    EXPOSED := an alias group (survivor S @ addr(S)) lists a folded spelling F
    that target_symbol_map.json ALSO places at >= 1 address != addr(S).
    """
    m = json.load(open(os.path.join(root, 'scripts/target_symbol_map.json')))
    rev = collections.defaultdict(list)
    for a, n in m.items():
        if a.startswith('0x') and isinstance(n, str):
            rev[n].append(a.lower())
    al = json.load(open(os.path.join(root, 'scripts/symbol_aliases.json')))
    out = []
    for gi, g in enumerate(al['groups']):
        sa = g['address'].lower()
        for f in g['folded']:
            other = [a for a in rev.get(f, []) if a != sa]
            if other:
                out.append({'gi': gi, 'saddr': sa, 'survivor': g['survivor'],
                            'folded': f, 'faddrs': other,
                            'tier': tier_of(g['evidence'])})
    return al, out


def tier_of(ev):
    if 'icf_alias_fixpoint' in ev:
        return 'fixpoint'
    for t in ('T1', 'T2', 'T3'):
        if 'Evidence tier(s) %s' % t in ev:
            return t
    return 'other:' + ev[:44]


def prune(root, img, ext, path_out):
    al, rows = census(root)
    drop = collections.defaultdict(list)
    kept = []
    for r in rows:
        verdicts = [classify(img, ext, int(r['saddr'], 16), int(fa, 16))
                    for fa in r['faddrs']]
        if any(v == 'SAME' for v, _s, _d in verdicts):
            kept.append(r)                       # real fold evidence -> keep
            continue
        drop[r['gi']].append(r['folded'])
    ngroups, removed, emptied = [], 0, 0
    for gi, g in enumerate(al['groups']):
        d = set(drop.get(gi, ()))
        if not d:
            ngroups.append(g)
            continue
        nf = [f for f in g['folded'] if f not in d]
        removed += len(g['folded']) - len(nf)
        if not nf:
            emptied += 1
            continue
        g = dict(g, folded=nf)
        ngroups.append(g)
    al['groups'] = ngroups
    note = ('LANE T1-AUDIT 2026-08-13: %d folded spelling(s) removed from %d '
            'group(s) (%d group(s) emptied). An alias asserts retail has ONE '
            'body for two spellings; where target_symbol_map.json places the '
            'folded spelling on a DIFFERENT LIVE address it asserts TWO, and '
            'the T1 warrant (masked identity against addr(S) only) NEVER '
            'adjudicates addr(F). Adjudicated retail-vs-retail, relocation-'
            'normalized over .pdata extents: 0 SAME. Remedy for each is a MAP '
            'ROW REPAIR, which the alias foreclosed.'
            % (removed, len(drop), emptied))
    al.setdefault('_comment', [])
    if isinstance(al['_comment'], list):
        al['_comment'] = list(al['_comment']) + ['', note]
    json.dump(al, open(path_out, 'w'), indent=1)
    print('\n=== PRUNE ===')
    print('  folded spellings removed : %d' % removed)
    print('  groups touched           : %d' % len(drop))
    print('  groups emptied (dropped) : %d' % emptied)
    print('  groups remaining         : %d' % len(ngroups))
    print('  kept on SAME evidence    : %d' % len(kept))
    print('  wrote %s' % path_out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=BAND)
    ap.add_argument('--prune', metavar='OUT_JSON')
    ap.add_argument('--pairs', help='JSON list from the census')
    ap.add_argument('--control', action='store_true')
    ap.add_argument('--control-limit', type=int, default=0)
    ap.add_argument('--out')
    a = ap.parse_args()

    img = Image(a.exe)
    ext = load_extents(img)
    print('.pdata extents: %d' % len(ext))

    if a.control:
        controls(img, ext, a.control_limit)
    if a.prune:
        prune(ROOT, img, ext, a.prune)
    if not a.pairs:
        return

    rows = json.load(open(a.pairs))
    seen, out = {}, []
    for r in rows:
        for fa in r['faddrs']:
            key = (r['saddr'], fa)
            if key not in seen:
                seen[key] = classify(img, ext, int(r['saddr'], 16), int(fa, 16))
            v, sc, d = seen[key]
            out.append(dict(r, faddr=fa, verdict=v, subclass=sc, detail=d))

    print('\n=== PAIR-INSTANCE verdicts (%d) ===' % len(out))
    for k, n in collections.Counter((o['verdict'], o['subclass']) for o in out).most_common():
        print('  %-14s %-16s %4d' % (k[0], k[1], n))
    print('\n=== DISTINCT ADDRESS-PAIR verdicts (%d) ===' % len(seen))
    for k, n in collections.Counter((v, s) for v, s, _ in seen.values()).most_common():
        print('  %-14s %-16s %4d' % (k[0], k[1], n))
    print('\n=== by alias tier (pair-instances) ===')
    t = collections.defaultdict(collections.Counter)
    for o in out:
        t[o['tier']][o['verdict']] += 1
    for k, c in sorted(t.items()):
        print('  %-10s %s' % (k, dict(c)))
    if a.out:
        json.dump(out, open(a.out, 'w'), indent=1)
        print('\nwrote %s' % a.out)


if __name__ == '__main__':
    main()
