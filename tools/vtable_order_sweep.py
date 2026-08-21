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

⛔⛔ READ BEFORE EDITING ANY SOURCE ON A VERDICT FROM THIS TOOL
--------------------------------------------------------------
**A `SET_DIFFER`/`PERMUTED` row does NOT distinguish "our declaration order is
wrong" from "the MAP's name for that slot is wrong" -- both produce the
identical row**, because the retail side of the comparison is a *map name*.
Wave 6 (2026-08-21) adjudicated **6 of these on retail bytes and got 6 MAP
defects and 0 source defects**, after two source edits made on apparently
strong evidence were measured at **-2 matched / -40 B** and reverted.

★★★ **THE AUTHORITATIVE INSTRUMENT IS THE CALL SITE, NOT THE NAME.** A caller
dispatching a virtual emits `lwz r11, (slot*4)(r11); mtctr; bctr[l]` -- the slot
index is an **immediate in retail's own machine code**.  It cannot be poisoned
by ICF (it is not a relocation) and depends on no name.  `objdiff` already
surfaces it as a `diff_arg` on that `lwz`.  Three weaker things that all agreed
with each other and were all WRONG:

  1. the map name at the disputed slot -- that IS the claim under test;
  2. the rb3-Wii / DC3 oracle's declaration order -- a different build, not
     binding on retail-360 (see the four oracle-fidelity modes);
  3. "retail slot N tail-calls slot M, and OUR slot M is X" -- ⛔ circular: it
     reads retail's index through OUR numbering, which is the thing in dispute.
     An off-by-one is invisible to a test that uses the numbering being tested.

⇒ Treat the residual worklist as a **map-defect** worklist.  Full write-up and
the 6-row score card: docs/decomp/VTABLE_SLOT_COUNT_FIXES_2026-08-20.md §12.

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

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import icf_fold_safe as ifs

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


# ⛔ THE FOLD-AWARENESS THIS SWEEP DEPENDS ON NOW LIVES IN ONE PLACE.
# It used to be an inline `occ[w] != 1 or within[w] != 1` predicate, duplicated
# in `sweep_class` AND `map_audit` right here in this file -- and a third,
# fold-BLIND copy in a prefix scan written a day later rebuilt the defect and
# confidently reported `XboxContent INTERIOR@3`.  Do not re-derive it; import
# it.  `Slot.__eq__` RAISES on a poisoned comparison, so a fold-blind copy now
# crashes instead of producing a wrong verdict.
fold_counts = ifs.fold_counts


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
                if secnum > 0 and '@@6B' in name and (
                        name.startswith('??_7') or name.startswith('??_R4')):
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


def our_col_offset(objpath, symname):
    """`offset` field of one of OUR `??_R4` Complete Object Locators, or None.

    MSVC's COL is 5 DWORDs -- signature, **offset**, cdOffset, pTypeDescriptor,
    pClassDescriptor -- so the vftable's offset within the complete object is
    the DWORD at +4.

    ⛔ **BIG-ENDIAN.**  The COFF *headers* are little-endian but the X360
    *payload* is big-endian PowerPC (same trap CLAUDE.md records for `.pdata`).
    Read little-endian and `0x3c` comes back as `1006632960`, which is not
    obviously wrong -- it is just a large number that no retail offset will ever
    equal, so every join silently MISSES and the sweep degrades to "ambiguous"
    rather than erroring.  `test_vtable_offset_join` pins the endianness against
    three compiler-verified offsets for exactly this reason.
    """
    sys.path.insert(0, os.path.join(os.path.dirname(objpath), ''))
    import coff_func_bodies as cfb
    try:
        d, secs, syms, _ = cfb.parse(objpath)
    except Exception:
        return None
    for (n, val, secnum, _t, _sc, _i) in syms:
        if n == symname and secnum > 0:
            sec = secs[secnum - 1]
            off = sec['rawptr'] + val
            if off + 8 > len(d):
                return None
            return struct.unpack_from('>I', d, off + 4)[0]
    return None


def our_vtable_candidates(cls, project_dir):
    """Every `??_7{cls}@@6B<suffix>` we compiled, as {suffix: (vt_sym, path)}."""
    idx = _obj_index(project_dir)
    pre = f'??_7{cls}@@6B'
    return {k[len(pre):]: (k, v) for k, v in idx.items() if k.startswith(pre)}


def our_vtable_by_offset(cls, project_dir, want_offset):
    """Our vftable at `want_offset`, joined via OUR OWN RTTI -- never by name.

    ★★★ The rule this REPLACES was a false premise, and the compiler refuted it.
    The sweep used to treat bare `??_7X@@6B@` as "the offset-0 (primary) table".
    For `CustomizePanel` (`class CustomizePanel : public UIPanel, public
    ContentMgr::Callback`) `cl /d1reportSingleClassLayoutCustomizePanel` places

        0x0   {vfptr} [UIPanel]      <- retail's COL.offset == 0 means THIS
        0x3c  {vfptr} [Callback]     <- and this is `??_7CustomizePanel@@6B@`
        0xb8  {vfptr} [Object > ObjRefOwner]

    so the bare name is a SECONDARY table.  Comparing it against retail's
    primary aligns two different tables and manufactures a full-width
    disagreement -- 17 of 22 `SET_DIFFER` verdicts were exactly this, and our
    source was correct in every one.  Same shape as the thunk-twin artifact:
    the tool read its input correctly and joined the wrong two things.

    ⇒ Join OFFSET to OFFSET.  Our objs are built `/GR`, so they carry `??_R4`
    COLs whose names parallel the `??_7` vftables one-for-one; each COL states
    its own offset.  Both sides are then authoritative RTTI and no mangled-name
    rule is involved.  **Ambiguity REFUSES rather than guesses** -- returning
    None yields AMBIGUOUS_MULTI_VTABLE, which is a worklist entry, not a
    verdict about the source.
    """
    cands = our_vtable_candidates(cls, project_dir)
    if not cands:
        return None, 'no_vtable'
    if len(cands) == 1 and want_offset in (0, None):
        # Single table and retail says primary -- no ambiguity to resolve.
        sfx, (sym, path) = next(iter(cands.items()))
        return [dict(symbol=s) for s in read_our_vtable(path, sym)], f'sole:{sym}'
    if want_offset is None:
        return None, 'retail_offset_unknown'
    idx = _obj_index(project_dir)
    hits = []
    for sfx, (sym, path) in cands.items():
        col = f'??_R4{cls}@@6B{sfx}'
        cp = idx.get(col)
        if cp is None:
            continue
        off = our_col_offset(cp, col)
        if off == want_offset:
            hits.append((sym, path))
    if len(hits) != 1:
        return None, f'col_join_{len(hits)}_hits'
    sym, path = hits[0]
    return [dict(symbol=s) for s in read_our_vtable(path, sym)], f'col:{sym}'


# --------------------------------------------------------------------------
# comparison
# --------------------------------------------------------------------------
def bare_class(mangled_rtti):
    """'.?AVRndText@@' -> 'RndText'  ('.?AU' = struct, '.?AV' = class)."""
    s = mangled_rtti
    if s.startswith('.?A') and len(s) > 4:
        s = s[4:]
    return s[:-2] if s.endswith('@@') else s


normalize_dtor = ifs.normalize_dtor


def hierarchy_names(R, vt_va):
    """Every class name in this vtable's RTTI hierarchy (self + all bases).

    Used to reject slot names owned by an UNRELATED class -- see
    `icf_fold_safe.name_owned_by`.  Returns an empty set when RTTI cannot be
    decoded, which the primitive treats as "no opinion" rather than as grounds
    to exclude.
    """
    out = set()
    col = R.u32(vt_va - 4)
    if not R.is_image_va(col):
        return out
    got = R.bases_of_col(col)
    if not got:
        return out
    _c, _chd, bases = got
    for b in bases:
        n = b.name or ''
        if n.startswith('.?A') and len(n) > 4:
            n = n[4:]
        if n.endswith('@@'):
            n = n[:-2]
        if n:
            out.add(n)
    return out


def compare_orders(retail, ours):
    """Compare two `ifs.Slot` lists, ignoring slots retail could not speak to.

    Returns (verdict, covered, detail).  Only slots BOTH sides can name
    participate: an incomparable retail slot is UNKNOWN, and excluding it is
    the honest choice -- counting it as agreement would manufacture SAMEs, and
    counting it as disagreement is the fold-poisoning defect.
    """
    if retail and not isinstance(retail[0], ifs.Slot):
        # tolerate plain-string callers (selftest fixtures)
        retail = [ifs.Slot(name=x) if x else ifs.Slot(reason='unnamed')
                  for x in retail]
        ours = [ifs.Slot(name=x) if x else ifs.Slot(reason='unnamed')
                for x in ours]
    pairs = ifs.comparable_pairs(retail, ours)
    agree, mism_p, withheld = ifs.charge(pairs)
    # Coverage counts what we were willing to CHARGE ON: agreements plus real
    # disagreements.  Withheld pairs are reported separately -- never folded
    # into coverage, which would overstate what the instrument actually saw.
    covered = len(agree) + len(mism_p)
    if covered < 2:
        return 'UNRESOLVED', covered, [], withheld
    if not mism_p:
        return 'SAME', covered, [], withheld
    scored = agree + mism_p
    rs = collections.Counter(r.name for (_i, r, _o) in scored)
    os_ = collections.Counter(o.name for (_i, _r, o) in scored)
    verdict = 'PERMUTED' if rs == os_ else 'SET_DIFFER'
    return verdict, covered, [(i, r.name, o.name) for (i, r, o) in mism_p], withheld


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
    # ⚠ `sub_base` (the base NAME from the retail hierarchy descriptor) is
    # deliberately NOT used to pick our table any more -- see
    # `our_vtable_by_offset` for the compiler-verified refutation of the
    # name-based rule.  It is kept only for reporting.
    # All fold/name-trust reasoning is delegated -- see tools/icf_fold_safe.py
    # for the four shapes it handles and why it is a TYPE rather than a helper.
    hier = hierarchy_names(R, vt_va) | {bare}
    retail_sl = ifs.retail_slots(slots, occ, addr2name, hierarchy=hier)
    # SOFT-mark interchangeable tail-call thunks.  Their addresses are distinct,
    # so both fold filters above pass them, but nothing in the bytes says which
    # name belongs to which twin -- see ifs.mark_thunk_twins for the measurement
    # (7 slots = 100% of the PERMUTED population) and the retail-byte
    # adjudication that showed our source was the correct side.
    _txt = [(s.va, s.va + s.rawsize) for s in R.sections if s.name == '.text'][0]
    retail_sl = ifs.mark_thunk_twins(
        retail_sl, lambda va: R.u32(va) if _txt[0] <= va < _txt[1] else None)
    ours, how = our_vtable_by_offset(bare, project_dir, sub_off)
    if how == 'no_vtable':
        ours = []       # we simply do not compile this class -> UNRESOLVED
    elif ours is None:
        # Could not PROVE which of our tables retail means.  Refuse: an
        # AMBIGUOUS row is a worklist entry, a verdict would be a claim about
        # the source we cannot support.
        return dict(cls=bare, rtti=cls_rtti, vt_va=vt_va,
                    retail_slots=len(slots), our_slots=0, folded_slots=0,
                    verdict='AMBIGUOUS_MULTI_VTABLE', covered=0, mismatches=[],
                    excluded={}, withheld=[], join=how)
    if how == 'no_vtable':
        ours = []       # we simply do not compile this class -> UNRESOLVED
    our_sl = ifs.our_slot_names(ours)
    verdict, covered, mism, withheld = compare_orders(retail_sl, our_sl)
    excl = ifs.exclusion_counts(retail_sl)
    return dict(cls=bare, rtti=cls_rtti, vt_va=vt_va,
                retail_slots=len(slots), our_slots=len(ours),
                folded_slots=excl.get('folded_across', 0) + excl.get('folded_within', 0),
                # ★ surfaced, not swallowed: an instrument that quietly narrows
                # its own population reads as "covered everything" when it did
                # not (CLAUDE.md, "no silent caps").
                excluded=dict(excl),
                verdict=verdict, covered=covered,
                mismatches=[dict(slot=i, retail=r, ours=o) for (i, r, o) in mism],
                # disagreements we refused to CHARGE because the retail name is
                # suspect -- a worklist for byte-level adjudication, not noise.
                withheld=[dict(slot=i, retail=r.name, ours=o.name,
                               reason=(r.reason or o.reason))
                          for (i, r, o) in withheld])


_VIRT = set('EMU')          # E private virtual, M protected virtual, U public virtual
_NONVIRT = set('QAIS')      # Q public, A private, I protected, S static


# Single definition, in the fold-safe module (the adjustor-thunk exclusion that
# makes it correct is documented there -- 1,379 false positives without it).
access_class = ifs.access_class


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
        # NOTE: no `hierarchy` here ON PURPOSE.  This audit's whole job is to
        # find slots whose NAME is untrustworthy, so excluding them up front
        # would be circular -- it would hide exactly the population it exists
        # to count.  It only needs the two ICF fold shapes.
        raw = ifs.retail_slots(slots, occ, addr2name)
        for i, (_v, w, _p) in enumerate(slots):
            if raw[i].reason in ('folded_across', 'folded_within'):
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

    # ---- the our-side table join, pinned against COMPILER ground truth ------
    # `cl /d1reportSingleClassLayoutCustomizePanel` reports
    #     0x0 {vfptr} [UIPanel] · 0x3c {vfptr} [Callback] · 0xb8 {vfptr} [Object]
    # These three constants are transcribed from that report, so a wrong
    # endianness, a wrong COL field offset, or a regression to picking the
    # table by mangled name all FAIL here rather than silently mis-joining.
    idx = _obj_index(project_dir)
    if '??_R4CustomizePanel@@6B@' not in idx:
        print("  [skip] COL-offset join: CustomizePanel.obj not built")
    else:
        for sym, want in (('??_R4CustomizePanel@@6BUIPanel@@@', 0x00),
                          ('??_R4CustomizePanel@@6B@', 0x3c),
                          ('??_R4CustomizePanel@@6BObject@Hmx@@@', 0xb8)):
            chk(f'COL offset {sym}', our_col_offset(idx[sym], sym), want)
        # retail COL.offset == 0 must select the UIPanel table (15 slots),
        # NOT the bare `??_7...@@6B@` name (14 slots, the Callback subobject).
        sl, how = our_vtable_by_offset('CustomizePanel', project_dir, 0)
        chk('join(0) picks UIPanel table', 'UIPanel' in (how or ''), True)
        chk('join(0) slot count', len(sl or []), 15)
        chk('join(0) slot 0', (sl or [{}])[0].get('symbol'),
            '?Load@CustomizePanel@@UAAXXZ')
        # ★ ambiguity must REFUSE, never fall back to a guess
        sl3, how3 = our_vtable_by_offset('CustomizePanel', project_dir, 0x999)
        chk('bogus offset refuses', (sl3, how3), (None, 'col_join_0_hits'))

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

    # ★ Report what the instrument could NOT see, every run.  A sweep that
    # quietly narrows its own population reads as "covered everything" when it
    # did not, and the withheld list is a byte-adjudication worklist, not noise.
    excl = collections.Counter()
    for r in results:
        excl.update(r.get('excluded') or {})
    n_withheld = sum(len(r.get('withheld') or []) for r in results)
    print(f"\ncomparable slots charged on : {sum(r['covered'] for r in results)}")
    print(f"withheld (suspect name, disagreed): {n_withheld}"
          "   <- adjudicate on retail bytes, not by name")
    for k, v in excl.most_common():
        print(f"  excluded {k:<18} {v}")

    # The SAME=0 tell.  Cheap, and it has already caught a real off-by-one.
    ifs.assert_can_agree(by['SAME'], by['SAME'] + by['SET_DIFFER'] + by['PERMUTED'],
                         label='vtable order sweep')

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
