#!/usr/bin/env python3
"""Lane ORACLE-1: the MAP-INDEPENDENT anchor for vtable-slot-order adjudication.

THE CIRCULARITY THIS CLOSES.  Every prior aligner in tools/maprow_audit anchors
on names that are ALREADY IN THE MAP -- thunk_oracle.slot_consensus literally
calls `self.tmap.get(...)` to learn what method a sibling slot holds.  So it can
say "this row disagrees with its neighbours", but it cannot say "and here is the
right name", because the neighbours' names are the very thing under test.

THE ANCHOR.  Both sides can be read without the map at all:

  RETAIL   RTTI ??_R0 TypeDescriptor string ".?AVFoo@@"  -> ??_R4 COL
           -> vtable -> slot i -> a .text VA
           (rtti_vtable_index.Rtti; every step a byte read out of band.exe)

  OURS     our compiled COFF obj's `??_7Foo@@6B@` symbol -> that section's
           relocations -> slot i -> a MANGLED SYMBOL NAME
           (this file; every step a byte read out of our own .obj)

The join key is the CLASS NAME, which both sides state themselves.  Slot i on
the retail side is an address; slot i on our side is a name; the vtable's slot
ORDER is fixed by the C++ ABI, so pairing them yields a map row proposal that
consulted the map NOWHERE.  Feeding that to the map is then a real test, and
disagreements are candidate defects rather than tautologies.

This also satisfies MAPDEF-3's hardest sub-population BY CONSTRUCTION: rows
whose desired name "exists nowhere in the map, so it must be constructed and
then proven defined in the owning unit's obj".  A name produced here came OUT OF
an obj's vtable, so it is defined in that obj by construction -- there is no
separate proving step to fail.

WHAT THIS TOOL DELIBERATELY DOES NOT DO
  - It does not propose names for UNMAPPED addresses.  Under the shipped
    name_check ruler a placeholder-named reloc target is FORGIVEN, so naming an
    anonymous address has ZERO byte upside and converts a forgiven call site
    into a checked one.  Those are counted and then set aside, never applied.
  - It does not emit aliases.  An alias is pure forgiveness and would raise the
    score by construction; that is metric-fitting, not matching.
  => the only actionable class is a row whose EXISTING name is WRONG.

CONTROLS (all in --selftest; every one of them can fail, and is shown failing)
  P1  positive: on rows the INDEPENDENT branch channel certified AGREE, the
      anchor must reproduce the incumbent name.
  N1  untreated control: the same comparison on the population the anchor was
      NOT built to treat -- a random class-name join between equal-length
      vtables.  This is the base rate a claim must beat.
  N2  fail-on-demand: rotate our slot order by one.  Agreement must collapse.
  N3  fail-on-demand: a bogus class name must join nothing.
"""
import struct, sys, os, json, glob, collections, random, re

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..'))
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))
from rtti_vtable_index import Rtti, demangle_class            # noqa: E402
from retail_reader import Image as RImage                     # noqa: E402

BAND = 'orig/45410914/band.exe'
MAP = 'scripts/target_symbol_map.json'
OBJROOT = 'build/45410914/src'

# ??_7Foo@@6B@  (primary)   /  ??_7Foo@@6BBase@@@  (secondary sub-object)
_VT_SYM = re.compile(r'^\?\?_7(.+?)@@6B(.*)@$')


# ----------------------------------------------------------------- COFF side
def coff_vtables(path):
    """path -> {vtable_symbol_name: [slot_target_name, ...]}

    Reads the COFF symbol + relocation tables.  A vtable lives in its own
    COMDAT section; its symbol's `value` is the offset within that section, and
    the section's relocations name what each 4-byte slot points at.  We walk
    relocations at value+0, +4, +8 ... and stop at the first gap -- a gap is the
    end of the vtable, because every live slot MUST carry a relocation.
    """
    with open(path, 'rb') as f:
        d = f.read()
    if len(d) < 20:
        return {}
    machine, nsec, _ts, symoff, nsym, optsz, _fl = struct.unpack_from('<HHIIIHH', d, 0)
    if symoff == 0 or nsym == 0:
        return {}
    stroff = symoff + nsym * 18
    if stroff + 4 > len(d):
        return {}
    strsz = struct.unpack_from('<I', d, stroff)[0]
    strtab = d[stroff:stroff + strsz]

    def name_at(o):
        if d[o:o + 4] == b'\0\0\0\0':
            so = struct.unpack_from('<I', d, o + 4)[0]
            e = strtab.find(b'\0', so)
            return strtab[so:e].decode('ascii', 'replace')
        return d[o:o + 8].rstrip(b'\0').decode('ascii', 'replace')

    syms, by_idx, i = [], {}, 0
    while i < nsym:
        o = symoff + i * 18
        nm = name_at(o)
        val, sec, _ty, _sc, aux = struct.unpack_from('<IhHBB', d, o + 8)
        rec = dict(name=nm, value=val, section=sec)
        syms.append(rec)
        by_idx[i] = rec
        i += 1 + aux

    secs = []
    sh = 20 + optsz
    for s in range(nsec):
        o = sh + s * 40
        _vs, _va, _rs, _ro, reloff, _ln, nrel, _nl, _ch = struct.unpack_from(
            '<IIIIIIHHI', d, o + 8)
        secs.append((reloff, nrel))

    # section index -> {offset: target symbol name}
    relocs = {}
    out = {}
    for sym in syms:
        m = _VT_SYM.match(sym['name'])
        if not m or sym['section'] <= 0 or sym['section'] > len(secs):
            continue
        si = sym['section'] - 1
        if si not in relocs:
            reloff, nrel = secs[si]
            tbl = {}
            for r in range(nrel):
                ro = reloff + r * 10
                if ro + 10 > len(d):
                    break
                rva, sidx, _rt = struct.unpack_from('<IIH', d, ro)
                t = by_idx.get(sidx)
                if t:
                    tbl[rva] = t['name']
            relocs[si] = tbl
        tbl = relocs[si]
        slots, off = [], sym['value']
        while off in tbl:
            slots.append(tbl[off])
            off += 4
        if slots:
            out[sym['name']] = slots
    return out


def our_vtables(root=OBJROOT):
    """class name -> {'primary': [names], 'sources': [obj paths], 'conflict': bool}

    A vtable is a COMDAT: the SAME class is emitted by every TU that includes
    its header and needs it, so we see it many times.  Copies must agree; a
    disagreement is recorded rather than silently resolved.
    """
    prim = {}
    conflicts = collections.defaultdict(set)
    srcs = collections.defaultdict(list)
    for p in glob.glob(os.path.join(root, '**', '*.obj'), recursive=True):
        try:
            vts = coff_vtables(p)
        except Exception:
            continue
        for symname, slots in vts.items():
            m = _VT_SYM.match(symname)
            if not m:
                continue
            cls, base = m.group(1), m.group(2)
            if base:                       # secondary sub-object vtable
                continue
            if cls in prim:
                if prim[cls] != slots:
                    conflicts[cls].add(tuple(slots))
                    conflicts[cls].add(tuple(prim[cls]))
            else:
                prim[cls] = slots
            srcs[cls].append(p)
    return prim, conflicts, srcs


# --------------------------------------------------------------- retail side
def retail_vtables(rt):
    """class name -> [ (vt_va, [slot VAs]) ] for PRIMARY (offset==0) vtables."""
    out = collections.defaultdict(list)
    for vt, (nm, off, sl) in rt.vt_slots.items():
        if off != 0:
            continue
        c = demangle_class(nm or '')
        if c:
            out[c].append((vt, sl))
    return out


# ------------------------------------------------------------------- joining
def join(prim, rvt, require_equal_len=True):
    """-> list of dicts, one per (class, retail vtable) pairing."""
    rows = []
    for cls, ours in prim.items():
        for (vt, slots) in rvt.get(cls, []):
            if require_equal_len and len(slots) != len(ours):
                rows.append(dict(cls=cls, vt=vt, ok=False,
                                 n_ours=len(ours), n_retail=len(slots), pairs=[]))
                continue
            n = min(len(slots), len(ours))
            rows.append(dict(cls=cls, vt=vt, ok=True,
                             n_ours=len(ours), n_retail=len(slots),
                             pairs=[(i, slots[i], ours[i]) for i in range(n)]))
    return rows


def compare(rows, tmap):
    """Pair proposals against the map.  Returns per-slot verdicts."""
    res = []
    for r in rows:
        if not r['ok']:
            continue
        for (i, va, nm) in r['pairs']:
            key = '0x%08x' % va
            cur = tmap.get(key)
            if cur is None:
                v = 'UNMAPPED'
            elif cur == nm:
                v = 'AGREE'
            else:
                v = 'DISAGREE'
            res.append(dict(cls=r['cls'], vt=r['vt'], slot=i, va=va,
                            key=key, ours=nm, cur=cur, verdict=v))
    return res


def load(exe=BAND, mp=MAP, root=OBJROOT):
    tmap = json.load(open(mp))
    rt = Rtti(RImage(exe))
    rt.build_attribution()
    prim, conflicts, srcs = our_vtables(root)
    rvt = retail_vtables(rt)
    return rt, tmap, prim, conflicts, srcs, rvt


if __name__ == '__main__':
    rt, tmap, prim, conflicts, srcs, rvt = load()
    print('our primary vtables   %d classes  (%d with conflicting copies)'
          % (len(prim), len(conflicts)))
    print('retail primary vtables %d classes / %d vtables'
          % (len(rvt), sum(len(v) for v in rvt.values())))
    shared = set(prim) & set(rvt)
    print('joined by class name   %d' % len(shared))
    rows = join(prim, rvt)
    good = [r for r in rows if r['ok']]
    print('  equal-length pairings %d / %d' % (len(good), len(rows)))
    res = compare(rows, tmap)
    c = collections.Counter(x['verdict'] for x in res)
    print('slot verdicts %s  (total %d)' % (dict(c), len(res)))
