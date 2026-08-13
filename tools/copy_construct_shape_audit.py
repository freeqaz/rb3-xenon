#!/usr/bin/env python3
"""copy_construct_shape_audit -- adjudicate the STLport `_Copy_Construct<T>` /
`_Param_Construct<T,U>` family on RETAIL BYTES, for the rows MASK-3 could not see.

Provenance: lane MAP-CC (2026-08-13), fourth pass over this family after
MASK-3 (`tools/copy_construct_audit.py`, 79576a74) and MAP-FIX (528a8144).
Read-only unless --write-map; not a build input.

★★★ THE HEADLINE FINDING: THE `memcpy` "CONTRADICTIONS" ARE AN ARTIFACT
-----------------------------------------------------------------------
MAP-FIX flagged this family because a naive witness -- "for a row named
`_Copy_Construct<T>`, the `bl` must be T's copy constructor, so its class must
be T" -- fires on ~a third of the family, the loudest case being
`_Copy_Construct<RndBone>` calling **memcpy**.  It was named the decisive
blocker on the family's addresses.

**It is not a defect class.  It is what the compiler does for an aggregate T.**

`new (p) T(v)` for a T with no user-declared copy constructor is compiled
MEMBERWISE: call each non-trivial member's copy constructor, and copy the
trivially-copyable runs inline or with `memcpy`.  So the `bl` at the variable
slot is the FIRST NON-TRIVIAL **MEMBER**'s constructor, not T's -- and when T is
trivially copyable throughout there is no constructor call at all, only memcpy.

Verified to the byte, three independent ways (cl.exe /d1reportSingleClassLayout):

    RndBone       retail: bl <ctor>(p+0); memcpy(p+12, v+12, 64)
                  layout: ObjPtr<RndTransformable> @0 (12 B), Transform @12,
                          **sizeof = 76**;  76 - 12 == 64.            EXACT
    BuildPoly     retail: identical shape, same 64-byte tail
                  layout: Polygon @0 (12 B), Transform @12, sizeof = 76. EXACT
    pair<Timer,TimerStats>
                  retail: memcpy(p, v, 48); memcpy(p+48, v+48, 548)
                  layout: **Timer sizeof = 48, TimerStats sizeof = 548**. EXACT

This is MASK-3's own W_C observation ("T's copy constructor installs the vtables
of its MEMBERS, not of itself") one level earlier: for these rows there is no T
constructor in the picture at all.  Only **3 rows in the whole family touch
memcpy**, not the ~36 the naive screen suggested.

⇒ **A callee whose class differs from T is the EXPECTED case, not evidence.**
Do not re-open this on the callee-name screen.  The screen that does work is
structural, and is what this tool implements.

WHY MASK-3 DID NOT COVER THESE ROWS (not an oversight)
------------------------------------------------------
MASK-3 censused the family by matching retail bytes against ONE exact 15-word
body (`BODY`, 60 B).  That is the right population for the question it asked --
that body is a single huge reloc-masked class where the ruler constrains nothing
-- but it is only 82 of the 115 map rows.  The other 33 have a DIFFERENT body
because their T is an aggregate, and they were never censused, never audited and
never counted in MASK-3's 39.6%.  Re-running MASK-3's tool at MAP-CC's HEAD
still reports 35 CONSISTENT / 0 MISIDENTIFIED: its class is settled and this
tool deliberately does not touch it.

    masked bodies in the family      21
    the MASK-3 60-byte class         82 rows   (audited there; untouched here)
    the pair class {POD4@0; ctor@4}   9 rows
    two 16-byte-T classes           3 + 3 rows
    the {ctor@0; memcpy tail} class   2 rows   (RndBone, BuildPoly)
   16 further rows are SINGLETON bodies -- structurally distinctive, so unlike
   the 60-byte class the ruler DOES constrain them.

THE WITNESSES (all read retail bytes; none reads the row's own name)
--------------------------------------------------------------------
  W_V  VTABLE / RTTI.  If the body stores a `lis/addi` constant into `*p`, that
       constant is a vtable; resolve it through `??_R4` and the class IS T (or a
       base of T installed first).  HARD verdict, map-independent.
  W_L  LAYOUT.  The body's member-ctor `this`-offsets, POD store offsets/widths
       and memcpy extents give T's layout and a lower bound on sizeof(T).
       Compare against cl.exe's layout for T.
  W_E  ELEMENT SIZE.  When a callee is an STLport container copy constructor it
       computes `finish - start` and divides by the element size with `srawi`.
       Reading that shift gives sizeof(element) with no name involved.

W_V's symmetry is the argument, exactly as MASK-3 required: the SAME instrument
CONFIRMED `_Copy_Construct<FilePath>` (installs `.?AVFilePath@@`) and
`_Copy_Construct<StandIn>` (installs `.?AVFixedSizeSaveable@@`, then
`.?AVStandIn@@` -- StandIn derives from FixedSizeSaveable) and REFUTED
`_Param_Construct<set<Symbol>>`.  A witness that only fires on flagged rows
proves nothing.

⛔⛔ TRAP -- W_L SILENTLY ANSWERS ABOUT A DIFFERENT CLASS OF THE SAME NAME
--------------------------------------------------------------------------
`/d1reportSingleClassLayout<Name>` matches on the INNERMOST name, so a nested
class resolves to whichever same-named class the TU happens to contain.  Asked
for `EventSink` it returned **`MsgSinks::EventSink`** (`{Symbol@0; bool
chainProxy@4; ObjList@8}`), which contradicts the retail body -- while the map
row is `EventSink@**MsgSource**`, i.e. `{Symbol ev@0; list<EventSinkElem>@4}`,
which matches it PERFECTLY.  Convicting on that reading would have deleted a
correct row.  Same failure mode as MASK-3's W_C: the witness was not lying, it
was answering a different question.  **This tool therefore returns W_L =
AMBIGUOUS whenever the mangled T is nested and the layout source cannot be tied
to the enclosing scope, and AMBIGUOUS never refutes.**

⚠ TRAP -- `bl` DESTINATIONS ARE **NOT** DISTINCT OUTSIDE THE 60-BYTE CLASS.
MASK-3 measured 112 members / 112 distinct destinations and built W_A on it.
Here THREE rows (BuildPoly, Key<vector<Vector2>>, StreakList@Scoring) share one
callee, 0x82686260 `??0vector<Extent>`: `vector<T>::vector(const vector&)` is
the same code for every 8-byte POD T.  A shared callee is not evidence, and the
callee's NAME cannot discriminate T.  W_E exists because of this -- it reads the
element size out of the callee's own arithmetic instead of its name.

MEASURED (lane MAP-CC, at 4f497557)
-----------------------------------
115 map rows.  82 are MASK-3's class and are not re-judged.  Of the other 33:
**29 CONSISTENT, 4 REFUTED** -- and every one of the 29 was flagged by the naive
callee-name screen.

    0x825e9668  `_Param_Construct<set<Symbol>,...>`      RE-HOMED
        W_V: installs `.?AVAccomplishmentLessonSongListConditional@@` after
        calling `??0AccomplishmentSongListConditional@@QAA@PAVDataArray@@H@Z`
        -- a derived-class constructor, forwarding (DataArray*, int).  Our tree
        AND the rb3-Wii oracle both declare exactly that signature, and the row
        already sat in unit `AccomplishmentDiscSongConditional` (corroboration
        only -- splits attribution can be circular).  Scored mpn 42.53, so the
        incumbent name was not collecting credit.
    0x822c9048  `_Copy_Construct<VocalEvent@MidiParser>`  DELETED
        Body needs T = {12-byte non-trivial head; 4-byte field @12} = 16 B, the
        shape its two class-mates genuinely have (ObjVector<Lod> 16,
        pair<vector<int>,int> 16).  `MidiParser::VocalEvent` is {DataNode @0
        (8 B); int mTick @8} = 12 -- in OUR header AND in rb3-Wii's.  The
        family's OTHER VocalEvent row, `_Param_Construct` @0x827eb7e0, has its
        tail store at +8 and matches exactly; both cannot be VocalEvent.
    0x82578d28  `_Copy_Construct<pair<Symbol const, vector<int>>>`  DELETED
        W_E: its callee 0x82576f18 divides the byte span by 8 (`srawi ...,3`)
        and calls `__uninitialized_copy<pair<float,float>*>` -- an 8-byte
        element.  The genuine `??0vector<int>` copy ctor is at 0x827c1378 with
        `srawi ...,2`, and IS the callee of `_Copy_Construct<LongJoyCheat>`,
        whose `vector<int> mSequence` our header confirms.  Positive control on
        both populations, one instrument.
    0x82767418  `_Param_Construct<list<int>,list<int>>`   DELETED
        Body is the pair shape: copy 4 bytes to p+0, then construct at p+4.
        `list<int>` is 8 bytes (its ctor self-links a circular head,
        `stw r3,0(r3); stw r3,4(r3)`) and has no 4-byte POD prefix.  T is some
        `pair<K4, list<int>>`; we cannot name it, so the row is emptied.
        ANONYMOUS BEATS WRONG.

Whole-binary A/B, both rulers -- see the merge message for the landed numbers.

★ WHAT THIS TOOL DELIBERATELY DOES NOT DO
------------------------------------------
It does not re-judge MASK-3's 60-byte class (settled, 0/35) and it does not try
to NAME the three emptied addresses.  Two of them are pair-like aggregates whose
key type is unrecoverable from the body alone; guessing would trade a wrong name
for a wrong name.  W_L is also not applied to template T (the layout tool cannot
be asked for an instantiation), so template rows rest on W_E and W_V only.
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tools.retail_rtti import RetailRtti  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP = 'scripts/target_symbol_map.json'

MEMBER_PFX = ('??$_Copy_Construct@', '??$_Param_Construct@')

#: MASK-3's exact 15-word `if (p) new (p) T(v)` body; None = the masked `bl`.
BODY60 = [0x7d8802a6, 0x9181fff8, 0xfbe1fff0, 0x3be1ff90, 0x9421ff90,
          0x907f0084, 0x907f0050, 0x2b030000, 0x419a0008, None,
          0x383f0070, 0x8181fff8, 0x7d8803a6, 0xebe1fff0, 0x4e800020]

LOAD_W = {32: 4, 34: 1, 40: 2, 48: 4, 50: 8, 58: 8}
STORE_W = {36: 4, 38: 1, 44: 2, 52: 4, 54: 8, 62: 8}


def bl_target(va, w):
    li = w & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    return (li if w & 2 else va + li) & 0xFFFFFFFF


class Audit:
    def __init__(self, project_dir):
        self.root = os.path.abspath(project_dir or ROOT)
        self.R = RetailRtti()
        self.d = self.R.data
        raw = json.load(open(os.path.join(self.root, MAP)))
        self.map = raw
        self.name = {}
        for a, n in raw.items():
            self.name[a] = n if isinstance(n, str) else '|'.join(n)

    def nm(self, va):
        return self.name.get('0x%08x' % va, '<anon>')

    def words(self, va, n):
        return list(struct.unpack_from('>%dI' % n, self.d, self.R.va2raw(va)))

    def extent(self, va):
        return self.R.extents.get(va)

    # ---------------------------------------------------------------- census --
    def family(self):
        """Map rows whose NAME is a family instantiation, with their retail body."""
        out = []
        for a, n in sorted(self.name.items()):
            if not n.startswith(MEMBER_PFX):
                continue
            va = int(a, 16)
            if self.R.va2raw(va) is None:
                continue
            sz = self.extent(va)
            out.append((a, va, n, sz))
        return out

    def masked_body(self, va, sz):
        """Body with every branch word wildcarded -- the ruler's view."""
        return tuple(0xFFFFFFFF if (w >> 26) == 18 else w
                     for w in self.words(va, sz // 4))

    def is_mask3_class(self, va, sz):
        if sz != 60:
            return False
        w = self.words(va, 15)
        return all(BODY60[i] is None or w[i] == BODY60[i] for i in range(15))

    # ------------------------------------------------------------- signature --
    def signature(self, va, sz):
        """Structural read of the body: member ctors, memcpy runs, POD stores,
        vtable installs.  Pure retail bytes; no name is consulted."""
        ws = self.words(va, sz // 4)
        # Seed the ABI arguments: r3 = destination `p`, r4 = source `&v`.  Without
        # this seeding an `addi r3,r3,4` (the pair class's "construct the second
        # member") loses its base and every member offset reads None.
        sym = {3: ('r3', 0), 4: ('r4', 0)}   # reg -> ('r3'|'r4'|'imm', disp)
        const = {}        # reg -> absolute constant (vtable candidates)
        ctors, memcpys, stores, vtables = [], [], [], []
        for i, w in enumerate(ws):
            op = w >> 26
            off = 4 * i
            if op == 18 and (w & 1):
                tgt = bl_target(va + off, w)
                dst, n = sym.get(3), sym.get(5)
                if self.nm(tgt) == 'memcpy':
                    memcpys.append(dict(off=off,
                                        dst=dst[1] if dst and dst[0] == 'r3' else None,
                                        n=n[1] if n and n[0] == 'imm' else None))
                elif not (off == 4 and ws[0] == 0x7d8802a6):
                    # A `bl` at +0x04 straight after `mflr r12` is MSVC's
                    # __savegprlr_N register-save helper, not a member ctor.
                    # Reporting it as one made every large body look like it
                    # constructed something at this+0.
                    ctors.append(dict(off=off, tgt='0x%08x' % tgt, name=self.nm(tgt),
                                      this=dst[1] if dst and dst[0] == 'r3' else None))
            elif op == 15:                                   # addis rt,0,imm
                rt, ra = (w >> 21) & 31, (w >> 16) & 31
                if ra == 0:
                    const[rt] = (w & 0xFFFF) << 16
                    sym.pop(rt, None)
            elif op == 14:                                   # addi rt,ra,si
                rt, ra = (w >> 21) & 31, (w >> 16) & 31
                v = w & 0xFFFF
                v = v - 0x10000 if v & 0x8000 else v
                if ra == 0:
                    sym[rt] = ('imm', v)
                    const.pop(rt, None)
                elif ra in const:
                    const[rt] = (const[ra] + v) & 0xFFFFFFFF
                    sym.pop(rt, None)
                elif ra in sym and sym[ra][0] != 'imm':
                    sym[rt] = (sym[ra][0], sym[ra][1] + v)
                    const.pop(rt, None)
                else:
                    sym.pop(rt, None)
                    const.pop(rt, None)
            elif op == 31 and ((w >> 1) & 0x3FF) == 444:     # or rA,rS,rB (mr)
                rs, ra, rb = (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
                if rs == rb:
                    if rs in sym:
                        sym[ra] = sym[rs]
                    else:
                        sym[ra] = ('r%d' % rs, 0)
                    const.pop(ra, None)
            elif op in STORE_W:
                rs, ra = (w >> 21) & 31, (w >> 16) & 31
                dv = w & 0xFFFF
                dv = dv - 0x10000 if dv & 0x8000 else dv
                b = sym.get(ra)
                if b and b[0] == 'r3':
                    if rs in const:
                        cls = self.R.class_of_vtable(const[rs])
                        if cls:
                            vtables.append(dict(off=off, at=b[1] + dv,
                                                va='0x%08x' % const[rs], cls=cls))
                            continue
                    stores.append((b[1] + dv, STORE_W[op]))
            elif op in LOAD_W:
                rt = (w >> 21) & 31
                sym.pop(rt, None)
                const.pop(rt, None)
        hi = max([o + n for o, n in stores], default=0)
        for c in memcpys:
            if c['dst'] is not None and c['n'] is not None:
                hi = max(hi, c['dst'] + c['n'])
        for c in ctors:
            if c['this'] is not None:
                hi = max(hi, c['this'])
        return dict(ctors=ctors, memcpy=memcpys, stores=sorted(set(stores)),
                    vtables=vtables, implied_size=hi)

    # --------------------------------------------------------------- W_E --
    def element_size(self, va):
        """sizeof(element) for an STLport container copy ctor, from its own
        `srawi` on (finish - start).  No name is consulted.  None if absent or
        ambiguous (more than one distinct shift)."""
        sz = self.extent(va)
        if not sz:
            return None
        sh = {(w >> 11) & 31 for w in self.words(va, sz // 4)
              if (w >> 26) == 31 and ((w >> 1) & 0x3FF) == 824}
        return 1 << sh.pop() if len(sh) == 1 else None


def targ(name):
    """The mangled T of a family instantiation (text between the template
    prefix and the closing `@stlpmtx_std@@`), plus whether it is a template."""
    for p in MEMBER_PFX:
        if name.startswith(p):
            t = name[len(p):].split('@stlpmtx_std@@YAX')[0]
            return t, ('?$' in t)
    return None, False


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--project-dir', default=None,
                    help='worktree to read scripts/target_symbol_map.json from')
    ap.add_argument('--show', default=None, help='only rows whose name matches this regex')
    ap.add_argument('--all', action='store_true',
                    help="also list MASK-3's 60-byte class (settled; not re-judged here)")
    args = ap.parse_args()

    au = Audit(args.project_dir)
    rows = au.family()
    cls = collections.defaultdict(list)
    mask3 = 0
    for a, va, n, sz in rows:
        if sz and au.is_mask3_class(va, sz):
            mask3 += 1
            if not args.all:
                continue
        cls[(sz,) + (au.masked_body(va, sz) if sz else ('NOEXT',))].append((a, va, n, sz))

    print('family rows in map: %d   (MASK-3 60-byte class: %d, settled -- see '
          'tools/copy_construct_audit.py)' % (len(rows), mask3))
    print('distinct relocation-masked bodies among the rest: %d' % len(cls))
    print('★ a callee whose class != T is EXPECTED: aggregate T is copied '
          'MEMBERWISE.  See this file\'s docstring.\n')

    pat = re.compile(args.show) if args.show else None
    for key, members in sorted(cls.items(), key=lambda kv: (-len(kv[1]), str(kv[0][0]))):
        pinned = 'SINGLETON (ruler CONSTRAINS this row)' if len(members) == 1 \
                 else 'class of %d -- ruler constrains NOTHING; %d! bijections score equally' \
                      % (len(members), len(members))
        shown = [m for m in members if not pat or pat.search(m[2])]
        if not shown:
            continue
        print('=== masked body size=%s  %s' % (key[0], pinned))
        for a, va, n, sz in shown:
            T, is_tmpl = targ(n)
            sig = au.signature(va, sz or 96)
            print('  %s  T=%s%s' % (a, T[:86], '  [template]' if is_tmpl else ''))
            for v in sig['vtables']:
                print('        W_V  installs vtable %s at this+%d -> %s'
                      % (v['va'], v['at'], v['cls']))
            for c in sig['ctors']:
                es = au.element_size(int(c['tgt'], 16))
                extra = '   W_E element=%dB' % es if es else ''
                print('        member ctor at this+%s -> %s %s%s'
                      % (c['this'], c['tgt'], c['name'][:70], extra))
            for c in sig['memcpy']:
                print('        memcpy this+%s, %s bytes   [trivially-copyable run]'
                      % (c['dst'], c['n']))
            if sig['stores']:
                print('        POD stores (offset,width): %s' % (sig['stores'],))
            print('        => implied sizeof(T) >= %d' % sig['implied_size'])
        print()
    return 0


if __name__ == '__main__':
    sys.exit(main())
