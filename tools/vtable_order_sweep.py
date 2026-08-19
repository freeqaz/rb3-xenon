#!/usr/bin/env python3
"""vtable_order_sweep.py -- compare RETAIL vtable slot order against OURS.

WHY THIS IS THE HIGH-VALUE DEFECT CLASS
---------------------------------------
A wrong `virtual` DECLARATION ORDER in a header is a real bug that the scoring
ruler is structurally blind to: `match_percent_normalized` is arg-blind, so a
class whose vtable slots are permuted can score 100 before AND after the fix.
It is nonetheless a genuine defect in the native runtime -- a call through slot
N dispatches to the wrong method.  Precedent from this project:
`bandobj/TrackInterface.h` declared `UserName`/`GetTrackIcon` in the wrong order,
found via retail `.rdata` across 3 independent vtable copies.

THE TWO SIDES
-------------
  retail : MSVC puts a `??_R4` Complete Object Locator pointer at vtable[-1];
           the COL chains to a TypeDescriptor carrying the literal class-name
           string.  So a retail vtable is identifiable BY NAME, directly from
           RTTI -- no alignment heuristic and no confidence margin, which is
           what makes this strictly better than aligning a name-multiset against
           an `.rdata` run (vtables are adjacent with no separator, so that
           approach mis-aligns across a vtable boundary).
           ⇒ DELEGATED to tools/retail_rtti.py.  Do NOT re-derive the address
           arithmetic: `va - 0x82000000` is valid ONLY for .rdata, and three
           lanes have already got this wrong.
  ours   : the `??_7<Class>@@6B@` symbol's relocations in our compiled COFF.
           ⇒ DELEGATED to scripts/dump_vtable.py.

VERDICTS
--------
  PERMUTED   same multiset of slot names, DIFFERENT order  <- the real prize:
             a pure declaration-order bug, fixable in the header.
  SET_DIFFER slot names differ as a set -- usually a porting gap (a method we
             do not declare, or an ICF fold), not a reordering.
  SAME       orders agree.
  UNRESOLVED too few retail slots carry a mapped name to judge.

⚠ NAME COVERAGE IS THE LIMIT, NOT THE INSTRUMENT.  Retail slots are function
VAs; turning them into names needs scripts/target_symbol_map.json, which names
only a fraction of the binary.  A slot whose VA has no map entry is UNKNOWN and
is excluded from the comparison -- it is NOT evidence of agreement.  Every
verdict below is reported with its covered-slot count so an UNRESOLVED can never
be read as a clean bill.

Usage:
  python3 tools/vtable_order_sweep.py --selftest
  python3 tools/vtable_order_sweep.py --class RndSet -v
  python3 tools/vtable_order_sweep.py --json out.json          # full sweep
  python3 tools/vtable_order_sweep.py --map-audit               # map defects
"""
import argparse
import collections
import importlib.util
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'scripts'))


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def retail():
    from retail_rtti import RetailRtti
    return RetailRtti()


# --------------------------------------------------------------------------
# retail side
# --------------------------------------------------------------------------
def enumerate_retail_vtables(R, section='.rdata'):
    """[(vtable_va, '.?AVFoo@@')] for every vtable whose slot[-1] is a COL.

    Single linear pass over the section's words: a word that points at a
    plausible COL marks the slot BEFORE the vtable, so the vtable begins 4
    bytes later.  Complete by construction -- no scanning window, no margin.
    """
    out = []
    for sec in R.sections:
        if sec.name != section:
            continue
        raw = R.data[sec.rawptr:sec.rawptr + sec.rawsize]
        for off in range(0, len(raw) - 3, 4):
            w = struct.unpack_from('>I', raw, off)[0]
            if not R.is_image_va(w):
                continue
            c = R.decode_col(w)
            if not R._col_is_plausible(c):
                continue
            n = R.td_name(c.ptd)
            if n and n.startswith('.?A'):
                out.append((sec.va + off + 4, n))
    return out


def read_retail_slots(R, vt_va, exts, starts, text, max_slots=512):
    """Function VAs in a retail vtable, bounded by the NEXT vtable's COL slot.

    ⚠ The stop condition matters more than it looks.  `.rdata` is full of
    function-pointer tables, so "keep going while the word is a function VA"
    RUNS PAST THE END of the table: an early version read `FilePath`'s vtable
    into a `String::operator+=` that is obviously not one of its virtuals.
    Since every vtable start is enumerated up front, the honest bound is the
    next start minus its own COL slot.

    ⚠ `.pdata` is authoritative for function starts but EXCLUDES tiny leaf
    stubs (an 8-byte leaf touches neither stack nor LR, so it gets no unwind
    record -- CLAUDE.md AUDIT-NC).  A slot inside .text but absent from .pdata
    is ACCEPTED and flagged, never silently dropped.
    """
    import bisect
    i = bisect.bisect_right(starts, vt_va)
    nxt = starts[i] - 4 if i < len(starts) else None
    slots = []
    va = vt_va
    while (nxt is None or va < nxt) and len(slots) < max_slots:
        w = R.u32(va)
        if w is None or not R.is_image_va(w):
            break
        in_pdata = w in exts
        in_text = bool(text and text[0] <= w < text[1])
        if not (in_pdata or in_text):
            break
        slots.append((va, w, in_pdata))
        va += 4
    return slots


def fold_counts(tables):
    """slot address -> number of DISTINCT vtables it appears in.

    ⛔ THE BINDING CONSTRAINT ON THIS WHOLE SWEEP.  ICF folds identical
    COMDATs, so one retail address serves many classes' slots -- measured, a
    single address is a slot in hundreds of distinct vtables, and
    target_symbol_map.json can only name it with ONE arbitrary survivor
    spelling.  Comparing such a slot by NAME conflates "folded" with "wrong",
    the same disease that makes objdiff's LINKER_MERGED verdict uninformative.
    Only slots whose address appears in exactly ONE vtable are comparable; for
    the rest, retail itself destroyed the distinction and no instrument can
    recover it.
    """
    occ = collections.Counter()
    for slots in tables.values():
        for w in {w for (_va, w, _p) in slots}:
            occ[w] += 1
    return occ


# --------------------------------------------------------------------------
# our side
# --------------------------------------------------------------------------
_OBJ_INDEX = None


def _obj_index(project_dir):
    """vtable symbol name -> obj path, built once by scanning every compiled obj.

    ⚠ `dump_vtable.find_obj_file()` returned None for EVERY class tried
    (`RndHighlightable` included), which made the whole sweep report
    UNRESOLVED -- a vacuity that looks exactly like "no defects found".
    Indexing by the actual `??_7...@@6B@` symbol is exact and also immune to
    the basename collision (`Movie.obj` exists in both rnddx9/ and rndobj/).
    """
    global _OBJ_INDEX
    if _OBJ_INDEX is not None:
        return _OBJ_INDEX
    sys.path.insert(0, os.path.join(project_dir, 'scripts', 'harvest'))
    import coff_func_bodies as cfb
    idx = {}
    base = os.path.join(project_dir, 'build', '45410914', 'src')
    for dirpath, _d, files in os.walk(base):
        for fn in files:
            if not fn.endswith('.obj'):
                continue
            p = os.path.join(dirpath, fn)
            try:
                _d2, _secs, syms, _i = cfb.parse(p)
            except Exception:
                continue
            for (name, _val, secnum, _t, _sc, _i2) in syms:
                if secnum > 0 and name.startswith('??_7') and '@@6B' in name:
                    idx.setdefault(name, p)
    _OBJ_INDEX = idx
    return idx


def read_our_vtable(objpath, symname):
    """Ordered slot symbols of one `??_7...` vtable in our compiled obj.

    Relocations are filtered to the SYMBOL's own extent and sorted by offset.
    ⚠ `dump_vtable.find_vtable()` returns every relocation of the whole
    SECTION, which is fine only while each vtable owns its COMDAT -- for a
    multi-vtable class that would interleave two tables' slots.
    """
    sys.path.insert(0, os.path.join(os.path.dirname(objpath), ''))
    import coff_func_bodies as cfb
    try:
        d, secs, syms, _ = cfb.parse(objpath)
    except Exception:
        return []
    me = None
    for (n, val, secnum, _t, _sc, i) in syms:
        if n == symname and secnum > 0:
            me = (val, secnum)
            break
    if me is None:
        return []
    val, secnum = me
    sec = secs[secnum - 1]
    # end = next symbol in the same section strictly after us
    ends = [v for (n, v, sn, _t, _sc, _i) in syms
            if sn == secnum and v > val and not n.startswith('$')]
    end = min(ends) if ends else sec['rawsz']
    idx = {}
    for (n, _v, _sn, _t, _sc, i) in syms:
        idx[i] = n
    out = []
    for r in range(sec['nrel']):
        off = sec['relptr'] + r * 10
        rva, sidx, _rt = struct.unpack_from('<IIH', d, off)
        if val <= rva < end:
            out.append((rva, idx.get(sidx, f'<unk{sidx}>')))
    out.sort()
    return [n for _o, n in out]


def our_vtable(cls, project_dir, base=None):
    """Our vtable slot symbols for `cls`; `base` selects a secondary table."""
    idx = _obj_index(project_dir)
    if base:
        cands = [f'??_7{cls}@@6B{base}@@@']
    else:
        cands = [f'??_7{cls}@@6B@', f'??_7{cls}@@6B0@@']
    for sym in cands:
        p = idx.get(sym)
        if p:
            return [dict(symbol=s) for s in read_our_vtable(p, sym)]
    return []


# --------------------------------------------------------------------------
# comparison
# --------------------------------------------------------------------------
def bare_class(mangled_rtti):
    """'.?AVRndText@@' -> 'RndText'  ('.?AU' = struct, '.?AV' = class)."""
    s = mangled_rtti
    if s.startswith('.?A') and len(s) > 4:
        s = s[4:]
    return s[:-2] if s.endswith('@@') else s


def normalize_dtor(sym):
    """Fold `??_E` (vector deleting dtor) and `??_G` (scalar deleting dtor).

    MSVC emits both; their bodies are frequently ICF-identical, so which
    spelling the map records for the surviving address is arbitrary.  Treating
    them as distinct would report a reordering that does not exist.
    """
    if sym and sym.startswith('??_E'):
        return '??_G' + sym[4:]
    return sym


def compare_orders(retail_names, our_names):
    """Compare two slot->name lists, ignoring slots retail could not name.

    Returns (verdict, covered, detail).  Only slots where BOTH sides have a
    name participate: an unnamed retail slot is UNKNOWN, and excluding it is
    the honest choice -- counting it as agreement would manufacture SAMEs.
    """
    pairs = [(i, r, o) for i, (r, o) in enumerate(zip(retail_names, our_names))
             if r and o]
    covered = len(pairs)
    if covered < 2:
        return 'UNRESOLVED', covered, []
    mism = [(i, r, o) for (i, r, o) in pairs if r != o]
    if not mism:
        return 'SAME', covered, []
    rs = collections.Counter(r for (_i, r, _o) in pairs)
    os_ = collections.Counter(o for (_i, _r, o) in pairs)
    verdict = 'PERMUTED' if rs == os_ else 'SET_DIFFER'
    return verdict, covered, mism


def retail_subobject_base(R, vt_va):
    """Which BASE subobject this retail vtable belongs to, or None for primary.

    `COL.offset` is the vftable's offset within the complete object; the
    ClassHierarchyDescriptor's BaseClassDescriptors carry each base's `mdisp`.
    Matching offset->mdisp names the subobject, which is exactly what our COFF
    encodes in `??_7Class@@6B<Base>@@@`.  That makes the multi-vtable join
    PRINCIPLED rather than a guess -- previously all 1,306 such vtables were
    refused outright.
    """
    col_ptr = R.u32(vt_va - 4)
    if not R.is_image_va(col_ptr):
        return None, None
    c = R.decode_col(col_ptr)
    if not R._col_is_plausible(c):
        return None, None
    if c.offset == 0:
        return 0, None
    got = R.bases_of_col(col_ptr)
    if not got:
        return c.offset, None
    _c, _chd, bases = got
    for b in bases:
        if b.mdisp == c.offset and b.name:
            n = b.name
            if n.startswith('.?A') and len(n) > 4:
                n = n[4:]
            if n.endswith('@@'):
                n = n[:-2]
            return c.offset, n
    return c.offset, None


def sweep_class(R, cls_rtti, vt_va, slots, occ, addr2name, project_dir,
                n_vtables=1):
    """Compare one class.  Folded slots are EXCLUDED, not counted as agreement."""
    bare = bare_class(cls_rtti)
    sub_off, sub_base = retail_subobject_base(R, vt_va)
    if n_vtables > 1 and sub_off is None:
        # ⛔ 440 of 1,354 retail classes have MORE THAN ONE vtable (multiple or
        # virtual inheritance -- the `$4...` adjustor-thunk classes).  Our COFF
        # exposes a single `??_7X@@6B@`, so comparing it against whichever
        # retail vtable we happened to enumerate aligns two DIFFERENT tables and
        # manufactures a permutation.  UIFontImporter was exactly this: it
        # reported a 5-slot rotation of Hmx::Object's virtuals, which cannot be
        # real -- a wrong Hmx::Object order would break essentially every match
        # in the binary, and 44,514 functions match.
        return dict(cls=bare, rtti=cls_rtti, vt_va=vt_va,
                    retail_slots=len(slots), our_slots=0, folded_slots=0,
                    verdict='AMBIGUOUS_MULTI_VTABLE', covered=0, mismatches=[])
    if n_vtables > 1 and sub_off != 0 and sub_base is None:
        # secondary vtable whose subobject we could not name -> still ambiguous
        return dict(cls=bare, rtti=cls_rtti, vt_va=vt_va,
                    retail_slots=len(slots), our_slots=0, folded_slots=0,
                    verdict='AMBIGUOUS_MULTI_VTABLE', covered=0, mismatches=[])
    # ⚠ TWO fold shapes, and the first version caught only one.
    #   (a) ACROSS vtables -- one address serves many classes' slots.
    #   (b) WITHIN this vtable -- the SAME address occupies two slots, because
    #       two of the class's own virtuals have identical bodies.  Measured on
    #       MCContainerXbox: `Format()` and `Unformat()` are both
    #       `{ return (MCResult)0xD; }`, so retail's slots 9 and 10 hold ONE
    #       address and the map names it `Format`.  `occ` is 1 (it appears in a
    #       single vtable) so the across-vtables filter passed it, and the row
    #       was reported as a SET_DIFFER "Format vs Unformat" defect that does
    #       not exist.  Retail cannot distinguish them; neither can we.
    within = collections.Counter(w for (_va, w, _p) in slots)
    retail_names = []
    n_folded = 0
    for (_va, w, _p) in slots:
        if occ.get(w, 0) != 1 or within[w] != 1:
            retail_names.append(None)
            n_folded += 1
            continue
        retail_names.append(addr2name.get(f"0x{w:08x}"))
    ours = our_vtable(bare, project_dir, base=sub_base)
    # ⚠ OUR COFF vtable symbol includes the `??_R4` Complete Object Locator as
    # its first entry; the RETAIL table is read from AFTER the COL (it sits at
    # vtable[-1]).  Comparing them raw shifts every slot by one and made the
    # first full sweep report SAME=0 / SET_DIFFER=472 -- an instrument that can
    # never agree.  A zero-agreement result is the tell; drop the COL.
    our_names = [e['symbol'] for e in ours if not e['symbol'].startswith('??_R4')]
    retail_names = [normalize_dtor(x) for x in retail_names]
    our_names = [normalize_dtor(x) for x in our_names]
    n = max(len(retail_names), len(our_names))
    retail_names += [None] * (n - len(retail_names))
    our_names += [None] * (n - len(our_names))
    verdict, covered, mism = compare_orders(retail_names, our_names)
    return dict(cls=bare, rtti=cls_rtti, vt_va=vt_va,
                retail_slots=len(slots), our_slots=len(ours),
                folded_slots=n_folded,
                verdict=verdict, covered=covered,
                mismatches=[dict(slot=i, retail=r, ours=o) for (i, r, o) in mism])


_VIRT = set('EMU')          # E private virtual, M protected virtual, U public virtual
_NONVIRT = set('QAIS')      # Q public, A private, I protected, S static


def access_class(sym):
    """MSVC member-function access class, or None if not decodable.

    ⚠ ADJUSTOR THUNKS MUST BE EXCLUDED.  `?F@C@@$4PPPPPPPM@A@AAX...` carries a
    displacement encoding between the `@@` and the real access class, so a
    naive scan reads a letter out of THAT and reports every thunk as
    non-virtual -- 1,379 false positives (29.3%) before the exclusion, 47 (1.6%)
    after.
    """
    if not sym or not sym.startswith('?') or sym.startswith('??'):
        return None
    if '@@$' in sym:
        return None
    m = re.search(r'@@([A-Z])', sym)
    return m.group(1) if m else None


def map_audit(tables, occ, addr2name):
    """Vtable membership PROVES virtuality -- so a slot named by a non-virtual
    symbol is a map-spelling defect.

    Control: 96.6% of plain named unfolded slots decode as virtual, which is
    what makes the 1.6% residue meaningful rather than detector noise.
    Mechanism is usually an ICF fold where the map recorded a static or
    non-virtual twin (e.g. `?StaticByteCode@NetPushScreenMsg@@SAEXZ` sitting in
    a vtable slot); under `name_check` our source spelling then reads as a wrong
    callee.
    """
    bad, seen, virt = [], 0, 0
    for va, slots in tables.items():
        within = collections.Counter(w for (_v, w, _p) in slots)
        for i, (_v, w, _p) in enumerate(slots):
            if occ.get(w, 0) != 1 or within[w] != 1:
                continue
            nm = addr2name.get(f"0x{w:08x}")
            a = access_class(nm) if nm else None
            if a is None:
                continue
            seen += 1
            if a in _VIRT:
                virt += 1
            elif a in _NONVIRT:
                bad.append(dict(vtable=va, slot=i, addr=w, name=nm, access=a))
    return dict(seen=seen, virtual=virt, bad=bad)


def selftest(project_dir):
    """Prove each half works AND that the comparator can return each verdict."""
    ok = True

    def chk(label, got, want):
        nonlocal ok
        good = got == want
        if not good:
            ok = False
        print(f"  [{'ok ' if good else 'FAIL'}] {label}: {got!r} (want {want!r})")

    chk('bare_class(.?AVRndText@@)', bare_class('.?AVRndText@@'), 'RndText')
    chk('bare_class(.?AUFoo@@)', bare_class('.?AUFoo@@'), 'Foo')

    # the comparator must be able to return every verdict it advertises
    chk('SAME', compare_orders(['a', 'b'], ['a', 'b'])[0], 'SAME')
    chk('PERMUTED', compare_orders(['a', 'b'], ['b', 'a'])[0], 'PERMUTED')
    chk('SET_DIFFER', compare_orders(['a', 'b'], ['a', 'c'])[0], 'SET_DIFFER')
    chk('UNRESOLVED (all unnamed)', compare_orders([None, None], ['a', 'b'])[0],
        'UNRESOLVED')
    # an unnamed retail slot must NOT be scored as agreement
    chk('unnamed slot excluded from coverage',
        compare_orders(['a', None, 'b'], ['a', 'zz', 'b'])[1], 2)

    R = retail()
    vts = enumerate_retail_vtables(R)
    good = len(vts) > 500
    if not good:
        ok = False
    print(f"  [{'ok ' if good else 'FAIL'}] retail vtables enumerated: {len(vts)} (want >500)")

    names = {n for _va, n in vts}
    for want in ('.?AVRndText@@', '.?AVHitSink@@'):
        good = want in names
        if not good:
            ok = False
        print(f"  [{'ok ' if good else 'FAIL'}] enumeration contains {want}")

    exts = R.extents
    good = len(exts) > 40000
    if not good:
        ok = False
    print(f"  [{'ok ' if good else 'FAIL'}] .pdata extents: {len(exts)} (want >40000)")

    print("\nSELFTEST", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--project-dir', default=ROOT)
    ap.add_argument('--class', dest='cls')
    ap.add_argument('--sweep', action='store_true')
    ap.add_argument('--json')
    ap.add_argument('--limit', type=int, default=0)
    ap.add_argument('-v', '--verbose', action='store_true')
    ap.add_argument('--map-audit', action='store_true',
                    help='report vtable slots the map names with a NON-VIRTUAL '
                         'symbol (impossible => map-spelling defect)')
    ap.add_argument('--selftest', action='store_true')
    args = ap.parse_args()

    if args.selftest:
        return selftest(args.project_dir)

    R = retail()
    exts = R.extents
    with open(os.path.join(args.project_dir, 'scripts', 'target_symbol_map.json')) as fh:
        raw = json.load(fh)
    addr2name = {}
    for k, v in raw.items():
        if isinstance(v, str):
            addr2name[k.lower()] = v
        elif isinstance(v, list) and v and isinstance(v[0], str):
            addr2name[k.lower()] = v[0]

    vts = sorted(enumerate_retail_vtables(R))
    starts = [va for va, _n in vts]
    text = [(s.va, s.va + s.rawsize) for s in R.sections if s.name == '.text'][0]
    tables = {va: read_retail_slots(R, va, exts, starts, text) for va, _n in vts}
    occ = fold_counts(tables)

    if args.map_audit:
        ma = map_audit(tables, occ, addr2name)
        pct = 100.0 * ma['virtual'] / max(ma['seen'], 1)
        print(f"plain named unfolded vtable slots : {ma['seen']}")
        print(f"  virtual (E/M/U) -- the control  : {ma['virtual']} ({pct:.1f}%)")
        print(f"  NON-VIRTUAL => MAP DEFECT       : {len(ma['bad'])} "
              f"({100.0 * len(ma['bad']) / max(ma['seen'], 1):.1f}%)")
        for b in ma['bad'][:40]:
            print(f"    0x{b['addr']:08x} slot {b['slot']:<3} {b['access']}  {b['name'][:72]}")
        if args.json:
            with open(args.json, 'w') as fh:
                json.dump(ma, fh, indent=1)
            print(f"\nwrote {args.json}")
        return 0

    sel = vts
    if args.cls:
        sel = [(va, n) for va, n in vts if bare_class(n) == args.cls]
        if not sel:
            print(f"no retail vtable found for class {args.cls!r}")
            return 1
    if args.limit:
        sel = sel[:args.limit]

    nvt = collections.Counter(bare_class(n) for _va, n in vts)

    results = []
    for va, n in sel:
        results.append(sweep_class(R, n, va, tables[va], occ, addr2name,
                                   args.project_dir,
                                   n_vtables=nvt[bare_class(n)]))

    by = collections.Counter(r['verdict'] for r in results)
    print(f"retail vtables examined: {len(results)}")
    for k in ('PERMUTED', 'SET_DIFFER', 'SAME', 'UNRESOLVED',
              'AMBIGUOUS_MULTI_VTABLE'):
        print(f"  {k:<24} {by[k]}")

    interesting = [r for r in results if r['verdict'] == 'PERMUTED']
    interesting.sort(key=lambda r: -r['covered'])
    if interesting:
        print(f"\n=== PERMUTED (declaration-order bug candidates), top by covered slots ===")
        for r in interesting[:20]:
            print(f"  {r['cls']:<32} covered={r['covered']:<4} "
                  f"retail_slots={r['retail_slots']:<4} ours={r['our_slots']:<4} "
                  f"vt=0x{r['vt_va']:08x}")
            for m in r['mismatches'][:6]:
                print(f"      slot {m['slot']:<3} retail={m['retail']}")
                print(f"      {'':<8} ours  ={m['ours']}")

    if args.verbose and args.cls:
        for r in results:
            print(f"\n{json.dumps(r, indent=1)[:3000]}")

    if args.json:
        with open(args.json, 'w') as fh:
            json.dump(results, fh, indent=1)
        print(f"\nwrote {args.json}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
