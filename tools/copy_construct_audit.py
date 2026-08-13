#!/usr/bin/env python3
"""copy_construct_audit -- find map entries that score on a FALSE PAIRING because
retail's function there is an STLport `_Copy_Construct<T>` / `_Param_Construct<T>`.

Provenance: lane MASK-3 (2026-08-13), the third audit in the reloc-masked-class
series after MAP-B (`tools/classname_forwarder_audit.py`, 5b56326d, 337 ClassName
forwarders, 20.6% misidentified) and DTOR-A (`tools/deleting_dtor_audit.py`,
15454a28, 577 deleting destructors, 19.8% non-template).  Read-only unless
--write-map; not a build input.

★ THE CLASS IS NOT A MILO IDIOM, AND THAT MATTERS
--------------------------------------------------
The first two classes were engine idioms emitted by a Milo macro (OBJ_CLASSNAME)
or by the compiler for a polymorphic class (??_G).  Both therefore had a witness
rooted in RTTI: a virtual lives in its own class's vtable.  **This class is an
STLport container helper and has NO RTTI handle at all**, so DTOR-A's primary
witness does not transfer.  See "WHY THE RTTI WITNESS FAILS HERE" below -- it
fails for a *correct* reason, and mistaking that for a defect would be the error.

THE HAZARD
----------
`_Copy_Construct<T>(T* p, const T& v)` is `if (p) new (p) T(v)`, which on X360 is
always the SAME 15 words::

    mflr r12 / stw r12,-8(r1) / std r31,-0x10(r1) / addi r31,r1,-0x70
    stwu r1,-0x70(r1) / stw r3,0x84(r31) / stw r3,0x50(r31)
    cmplwi cr6,r3,0 / beq cr6,+8
    bl ??0T@@QAA@ABV0@@Z                          <-- +0x24, THE ONLY VARIABLE WORD
    addi r1,r31,0x70 / lwz r12,-8(r1) / mtlr r12 / ld r31,-0x10(r1) / blr

14 of the 15 words are IDENTICAL for every T; only the `bl` displacement differs,
and the graded ruler runs `functionRelocDiffs=none`, which masks exactly that.
So all 112 of these functions are ONE reloc-masked equivalence class: **any of
the 112! bijections of their names over their addresses scores 100 on every
pair.**  A wrong name still scores.

★★ AND THE DESTINATIONS ARE ALL DISTINCT -- 112 members, 112 distinct `bl`
targets, zero duplicates.  This is lane ALIAS-X2's finding in a new population
(341 four-byte thunks -> 341 distinct destinations): **the shape proves nothing,
the resolved destination proves everything.**  It also means the class cannot be
collapsed by ICF, so every one of the 112 addresses is a real, separate pairing
decision that the metric does not constrain.

THE INSTRUMENT: the one word the ruler masks.

  W_A (PRIMARY, callee)   resolve the `bl` at +0x24.  That is T's copy
                          constructor, so its class IS T.
  W_B (INDEPENDENT, callers)  scan retail for every `bl` INTO this address.  The
                          callers are `vector<T>::_M_insert_overflow_aux`,
                          `list<T>::insert`, `__uninitialized_copy<T*>`,
                          `__uninitialized_fill_n<T*>` -- all instantiated over
                          the SAME T.  Different addresses, different symbols,
                          different bodies: genuinely independent of W_A.

★★★ WHY THIS IS NOT CIRCULAR, AND THE TEST THAT MAKES IT SO
------------------------------------------------------------
Both witnesses read a *name* out of the same map this tool is auditing, so on
their face they are circular.  They are admitted only when the supporting row is
**METRIC-PINNED**, which is a property that can be checked and which the audited
rows conspicuously lack:

    PINNED := the row's retail body is a SINGLETON masked class (no other
              function in the image has the same masked body)
              AND the row's name scores match_percent_normalized == 100.

Read those together: our compiled symbol N byte-matches, modulo relocations, a
body that occurs EXACTLY ONCE in the retail image.  There is no second address N
could have come from, so the ruler does constrain N -- unlike the 112 audited
rows, where it constrains nothing.  Evidence therefore flows strictly from the
constrained stratum to the unconstrained one.  Rows whose support is not pinned
are NOT_JUDGED and are never rewritten; that is 60 of the 112.

⚠ This is `masked_byte_identity.py`'s doctrine applied one level up: "a single
exact hit is a candidate, never a verdict".  A singleton retail body still does
not prove OUR symbol is the only one that could compile to it -- which is
precisely why W_B exists and why the positive control below is the load-bearing
number, not the error rate.

WHY THE RTTI WITNESS FAILS HERE (a correct behaviour, not a defect)
-------------------------------------------------------------------
DTOR-A resolved a class by asking which vtable contains the function.  Tried
here on the callee, `classes_installed_by` fires on only ~28 of 112 and
*disagrees* with the callee name 8 times out of 12 -- and every disagreement is
explainable: `??0Layer@LayerDir@@` reports `.?AVFilePath@@`,
`??0CallbackFile@ContentMgr@@` and `??0InlinedDir@ObjectDir@@` likewise.  T here
is usually a NON-POLYMORPHIC aggregate, so its copy constructor installs the
vtables of its MEMBERS, not of itself.  The witness is not lying; it is
answering a different question.  It is therefore reported as W_C and used for
nothing.

★ TRAP 1 -- SHAPE IS NECESSARY, NOT SUFFICIENT.  MAP-B had 9 shape-matches whose
callee was not a StaticClassName and judging on shape would have "corrected" 5
CORRECT entries; DTOR-A refused 30 on the same ground.  Here the callee must
actually BE a copy constructor (`??0X@@...@AB[VU]<backref>@@Z`).  3 rows fail
this and are refused.

★ TRAP 2 -- A SHARED CALLEE IS NOT EVIDENCE, AND INJECTIVITY IS THE CHECK.  The
map must stay injective on name (see the map's own `_name_injectivity_comment`).
This tool asserts it and REFUSES to write on a collision.

★ TRAP 3 -- THE SPLITS PIN CAN BE CIRCULAR.  MAP-B found rows on sliver `.text`
blocks that had themselves been filled BY BYTE SIGNATURE over the same masked
class.  Like DTOR-A, this tool answers that STRUCTURALLY: it consults NO unit,
splits, source-path or directory evidence whatsoever.  Every verdict comes from
retail bytes plus the pinned stratum of the map.

★ TRAP 4 -- A DIRECTORY HEURISTIC IS NOT A SOURCE WITNESS (MAP-B rewrote a
CORRECT `NgLight` because NgLight lives in `rndobj/Lit_NG.h`).  No filename or
directory rule appears here either.

★★ TRAP 5 -- TEMPLATE DIALECT.  DTOR-A found our headers declare
`template<class T> ObjPtr` where retail's is `ObjPtr<T,ObjectDir>`, and a raw
mangled comparison read 49% error instead of 19.8%.  Here BOTH sides of the
primary comparison are map names (our-build pairing keys), so the dialect is
consistent by construction -- but the map is not internally uniform (some rows
carry the 2-arg spelling), so `canon()` normalises a defaulted trailing
`VObjectDir@@` when and only when >=2 arguments are present, exactly as DTOR-A's
does, and results are STRATIFIED template vs non-template regardless.
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tools.retail_rtti import RetailRtti  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP = os.path.join(ROOT, 'scripts/target_symbol_map.json')
REPORT = os.path.join(ROOT, 'build/45410914/report.json')

#: the 15-word `if (p) new (p) T(v)` body; None = the masked `bl` slot.
BODY = [0x7d8802a6, 0x9181fff8, 0xfbe1fff0, 0x3be1ff90, 0x9421ff90,
        0x907f0084, 0x907f0050, 0x2b030000, 0x419a0008, None,
        0x383f0070, 0x8181fff8, 0x7d8803a6, 0xebe1fff0, 0x4e800020]
BL_CTOR = 0x24
SIZE = len(BODY) * 4

MEMBER_PFX = ('??$_Copy_Construct@', '??$_Param_Construct@')
CTOR_PARAM = re.compile(r'@AB[VU]\d+@@Z$')
CONTAINER = re.compile(r'\?\$(?:vector|list|deque|slist)@')
UNINIT_PFX = ('??$__uninitialized_copy@', '??$__uninitialized_fill_n@',
              '??$__uninitialized_fill@', '??$__uninitialized_copy_n@')


# ------------------------------------------------------ mangled-name parsing --
def read_qname(s, i):
    """Index just past a qualified name (fragments, then the terminating '@')."""
    while True:
        if i >= len(s):
            return None
        if s[i] == '@':
            return i + 1
        if s.startswith('?$', i):
            k = s.find('@', i + 2)
            if k < 0:
                return None
            i = k + 1
            while i < len(s) and s[i] != '@':
                j = read_type(s, i)
                if j is None:
                    return None
                i = j
            i += 1
        else:
            k = s.find('@', i)
            if k < 0:
                return None
            i = k + 1


def read_type(s, i):
    """Index just past one type.  Handles V/U/T and P*/A*/Q* indirection."""
    if i >= len(s):
        return None
    if s[i] in 'VUT':
        return read_qname(s, i + 1)
    if s[i] in 'PAQ' and i + 1 < len(s) and s[i + 1] in 'ABCD':
        return read_type(s, i + 2)
    return None


def type_text(s, i):
    """(class-qname text, index past) for the type at i, or (None, None)."""
    while i < len(s) and s[i] in 'PAQ' and i + 1 < len(s) and s[i + 1] in 'ABCD':
        i += 2
    if i >= len(s) or s[i] not in 'VUT':
        return None, None
    j = read_qname(s, i + 1)
    if j is None:
        return None, None
    return s[i + 1:j - 1], j


def first_targ(name, at):
    """First template argument's class text, given the index after the '@'."""
    t, _ = type_text(name, at)
    return t


def canon(t):
    """TRAP 5.  Drop a DEFAULTED trailing `VObjectDir@@` -- but only when the
    template has >=2 arguments, so `ObjDirPtr<ObjectDir>` (whose ONLY argument is
    ObjectDir) survives intact.  Applied to both sides, so it can only remove a
    dialect difference, never manufacture one."""
    if t is None or not t.startswith('?$'):
        return t
    k = t.find('@')
    if k < 0:
        return t
    args, i = [], k + 1
    while i < len(t) and t[i] != '@':
        s, j = type_text(t, i)
        if j is None:
            return t
        args.append(t[i:j])
        i = j
    if len(args) >= 2 and args[-1] == 'VObjectDir@@':
        return t[:k + 1] + ''.join(args[:-1]) + t[i:]
    return t


def member_T(name):
    for p in MEMBER_PFX:
        if name.startswith(p):
            return first_targ(name, len(p))
    return None


def ctor_class(name):
    """Class of a COPY constructor `??0X@@<cc>@AB[VU]<backref>@@Z` (TRAP 1)."""
    if not name or not name.startswith('??0') or not CTOR_PARAM.search(name):
        return None
    j = read_qname(name, 3)
    return name[3:j - 1] if j else None


def caller_T(name):
    """T of a container/algorithm instantiation that would call _Copy_Construct."""
    if not name:
        return None
    m = CONTAINER.search(name)
    if m:
        return first_targ(name, m.end())
    for p in UNINIT_PFX:
        if name.startswith(p):
            return first_targ(name, len(p))
    return None


def is_template(t):
    return bool(t) and t.startswith('?$')


# --------------------------------------------------------------------- audit --
class Audit:
    def __init__(self, project_dir=None):
        root = os.path.abspath(project_dir) if project_dir else ROOT
        self.R = RetailRtti()
        self.ext = self.R.extents
        self.map = json.load(open(os.path.join(root, 'scripts/target_symbol_map.json')))
        self.byaddr = {int(k, 16): v for k, v in self.map.items()
                       if k.lower().startswith('0x')}
        self.mpn100 = self._mpn100(os.path.join(root, 'build/45410914/report.json'))
        self.cls_size = self._masked_classes()

    @staticmethod
    def _mpn100(path):
        """Names at match_percent_normalized == 100 -- the ruler the count uses."""
        if not os.path.exists(path):
            raise SystemExit(f'missing {path}; build the report target first')
        out = set()
        for u in json.load(open(path))['units']:
            for f in u.get('functions', ()):
                if float(f.get('match_percent_normalized', 0)) == 100.0:
                    out.add(f['name'])
        return out

    def sig(self, a, n):
        ws = []
        for i in range(0, n, 4):
            w = self.R.u32(a + i)
            if w is None:
                return None
            op = w >> 26
            if op == 18:                       # b / bl : mask the displacement
                w &= 0xFC000003
            elif op == 15:                     # addis / lis : mask the immediate
                w &= 0xFFFF0000
            ws.append(w)
        return b''.join(x.to_bytes(4, 'big') for x in ws)

    def _masked_classes(self):
        """addr -> size of its reloc-masked equivalence class (1 == singleton)."""
        g = collections.defaultdict(list)
        for a, n in self.ext.items():
            if n > 0:
                s = self.sig(a, n)
                if s:
                    g[s].append(a)
        return {a: len(v) for v in g.values() for a in v}

    def pinned(self, a):
        """METRIC-PINNED: singleton masked body AND its name scores mpn == 100."""
        n = self.byaddr.get(a)
        return bool(n) and self.cls_size.get(a) == 1 and n in self.mpn100

    def bl(self, a, off):
        w = self.R.u32(a + off)
        if w is None or (w >> 26) != 18 or not (w & 1) or (w & 2):
            return None
        li = w & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        return (a + off + li) & 0xFFFFFFFF

    def members(self):
        out = []
        for a, n in sorted(self.ext.items()):
            if n != SIZE:
                continue
            ws = [self.R.u32(a + i) for i in range(0, SIZE, 4)]
            if any(w is None for w in ws):
                continue
            if any(e is not None and ws[i] != e for i, e in enumerate(BODY)):
                continue
            out.append(a)
        return out

    def caller_index(self, targets):
        """Every retail `bl` INTO each target (W_B).  Reads code, not tables."""
        tset, callers = set(targets), collections.defaultdict(list)
        for a, n in self.ext.items():
            for i in range(0, n, 4):
                w = self.R.u32(a + i)
                if w is None:
                    break
                if (w >> 26) == 18 and (w & 1) and not (w & 2):
                    li = w & 0x03FFFFFC
                    if li & 0x02000000:
                        li -= 0x04000000
                    t = (a + i + li) & 0xFFFFFFFF
                    if t in tset:
                        callers[t].append(a)
        return callers

    def run(self):
        mem = self.members()
        callers = self.caller_index(mem)
        rows = []
        for a in mem:
            name = self.byaddr.get(a)
            t = self.bl(a, BL_CTOR)

            # ---- W_A: the callee, admitted only if metric-pinned (TRAP 1) ----
            cn = self.byaddr.get(t) if t else None
            wa = why = None
            if t is None:
                why = 'no bl at +0x24'
            elif cn is None:
                why = 'callee anonymous'
            elif not CTOR_PARAM.search(cn) or not cn.startswith('??0'):
                why = 'callee is not a copy constructor'
            elif self.cls_size.get(t) != 1:
                why = f'callee body is in a masked class of {self.cls_size.get(t)}'
            elif cn not in self.mpn100:
                why = 'callee name does not score mpn==100'
            else:
                wa = canon(ctor_class(cn))
                if wa is None:
                    why = 'callee class unparsable'

            # ---- W_B: callers, independent of W_A -----------------------------
            # Admitted at two levels, both reported.  PINNED is the strict rule
            # (singleton body + mpn==100); SCORING only requires the caller's
            # name to score 100.  W_B is independent of W_A either way: different
            # addresses, different symbols, different bodies.
            wb, wbp, wbsrc = set(), set(), []
            for c in callers.get(a, ()):
                n = self.byaddr.get(c)
                if not n or n not in self.mpn100:
                    continue
                ct = canon(caller_T(n))
                if not ct:
                    continue
                wb.add(ct)
                wbsrc.append((c, ct))
                if self.pinned(c):
                    wbp.add(ct)
            wb_one = list(wb)[0] if len(wb) == 1 else None
            wbp_one = list(wbp)[0] if len(wbp) == 1 else None

            mt = canon(member_T(name)) if name else None
            if wa is None:
                verdict = 'UNMAPPED' if name is None else 'NOT_JUDGED'
            elif wb_one and wb_one != wa:
                # TRAP 2 in its general form: two independent witnesses disagree.
                # Refuse the row outright rather than pick the one we like.
                verdict = 'CONFLICT'
                why = f'W_A={wa} but independent caller witness says {wb_one}'
            elif name is None:
                verdict = 'UNMAPPED'
            elif mt is None:
                verdict = 'NOT_JUDGED'
                why = 'map name is not a _Copy/_Param_Construct instantiation'
            elif mt == wa:
                verdict = 'CONSISTENT'
            else:
                verdict = 'MISIDENTIFIED'
            rows.append(dict(addr=a, key='0x%08x' % a, map_name=name, map_T=mt,
                             callee=t and '0x%08x' % t, callee_name=cn,
                             W_A=wa, W_B=wb_one, W_B_pinned=wbp_one,
                             W_B_all=sorted(wb),
                             W_B_src=[('0x%08x' % c, x) for c, x in wbsrc],
                             n_callers=len(callers.get(a, ())),
                             verdict=verdict, why=why,
                             template=is_template(wa or mt)))
        return rows


# ------------------------------------------------------------------ reporting --
def positive_control(rows, key='W_B'):
    """DTOR-A's standard: an independent witness must perform EQUALLY on the rows
    the map gets RIGHT and the rows it gets WRONG.  A confirm rate measured only
    on flagged rows proves nothing."""
    out = {}
    for v in ('CONSISTENT', 'MISIDENTIFIED'):
        sub = [r for r in rows if r['verdict'] == v and r[key]]
        agree = sum(1 for r in sub if r[key] == r['W_A'])
        out[v] = (agree, len(sub))
    return out


def assert_injective(newmap):
    seen = collections.defaultdict(list)
    for k, v in newmap.items():
        if k.lower().startswith('0x') and isinstance(v, str):
            seen[v].append(k)
    return {n: ks for n, ks in seen.items() if len(ks) > 1}


def plan(rows):
    """Re-assign names over the class from the W_A verdicts.

    ANONYMOUS BEATS WRONG (MAP-B removed 1 entry, DTOR-A removed 33): a row whose
    true T is known but for which no `_Copy_Construct<T>` / `_Param_Construct<T>`
    name exists in the class's current name set is EMPTIED, never given a
    synthesised name.  A name that moves onto a judged address necessarily
    vacates wherever it used to sit -- that is forced by injectivity, not a
    second judgement."""
    have = {}                       # true T -> an existing map name for it
    for r in rows:
        if r['map_name'] and r['map_T']:
            have.setdefault(r['map_T'], r['map_name'])
    assign, freed = {}, {}
    for r in rows:
        if r['verdict'] in ('CONSISTENT', 'NOT_JUDGED', 'CONFLICT'):
            continue
        if not r['W_A']:
            continue
        want = have.get(r['W_A'])
        if want:
            assign[r['key']] = want
        elif r['map_name']:
            freed[r['key']] = r['map_name']          # emptied: no name available
    # every name that landed somewhere must vacate its previous address
    placed = set(assign.values())
    for r in rows:
        if r['map_name'] in placed and assign.get(r['key']) != r['map_name']:
            freed.setdefault(r['key'], r['map_name'])
    for k in assign:
        freed.pop(k, None)
    return assign, freed


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--project-dir', help='worktree to read map/report from')
    ap.add_argument('--json', help='write full rows here')
    ap.add_argument('--show', default='MISIDENTIFIED',
                    help='comma-separated verdicts to print')
    ap.add_argument('--write-map', action='store_true',
                    help='apply corrections (refuses on an injectivity collision)')
    args = ap.parse_args()

    au = Audit(args.project_dir)
    rows = au.run()
    c = collections.Counter(r['verdict'] for r in rows)
    print(f'_Copy_Construct/_Param_Construct masked class: {len(rows)} members, '
          f'{len({r["callee"] for r in rows if r["callee"]})} distinct bl destinations')
    print(f'  {dict(c)}')
    nj = collections.Counter(r['why'] for r in rows if r['verdict'] == 'NOT_JUDGED')
    print('  NOT_JUDGED reasons (never rewritten):')
    for k, v in sorted(nj.items(), key=lambda x: -x[1]):
        print(f'      {v:3d}  {k}')

    judged = [r for r in rows if r['verdict'] in ('CONSISTENT', 'MISIDENTIFIED')]
    mis = [r for r in judged if r['verdict'] == 'MISIDENTIFIED']
    if judged:
        print(f'\n  ERROR RATE (judged stratum): {len(mis)}/{len(judged)} = '
              f'{100.0 * len(mis) / len(judged):.1f}%')
        for lab, pred in (('non-template T (dialect-immune)', lambda r: not r['template']),
                          ('template T', lambda r: r['template'])):
            s = [r for r in judged if pred(r)]
            m = [r for r in s if r['verdict'] == 'MISIDENTIFIED']
            if s:
                print(f'      {lab:34s} {len(m):3d}/{len(s):3d} = '
                      f'{100.0 * len(m) / len(s):.1f}%')

    print('\n  POSITIVE CONTROL -- independent caller witness vs primary W_A.')
    print('  The load-bearing number is the SYMMETRY, not the rate: a witness that'
          '\n  only confirms flagged rows proves nothing.')
    for key, lab in (('W_B', 'callers scoring mpn==100'),
                     ('W_B_pinned', 'callers PINNED (singleton + mpn==100)')):
        print(f'    {lab}:')
        for v, (a, n) in positive_control(rows, key).items():
            side = 'RIGHT' if v == 'CONSISTENT' else 'WRONG'
            print(f'      on rows the map gets {side:5s}: {a}/{n}'
                  + (f' = {100.0 * a / n:.1f}%' if n else ' (no witness)'))

    asg, freed = plan(rows)
    print(f'\n  PLAN: {len(asg)} addresses re-assigned, {len(freed)} emptied '
          f'(anonymous beats wrong)')
    if args.write_map:
        m = json.load(open(os.path.join(
            os.path.abspath(args.project_dir) if args.project_dir else ROOT,
            'scripts/target_symbol_map.json')))
        for k in freed:
            m.pop(k, None)
        m.update(asg)
        coll = assert_injective(m)
        if coll:
            print('  ⛔ REFUSING TO WRITE -- injectivity collision (TRAP 2):')
            for n, ks in coll.items():
                print(f'      {n[:90]} -> {ks}')
            return 2
        path = os.path.join(os.path.abspath(args.project_dir) if args.project_dir
                            else ROOT, 'scripts/target_symbol_map.json')
        json.dump(m, open(path, 'w'), indent=1, sort_keys=False)
        print(f'  wrote {path}: injectivity asserted, no collisions')

    show = set(args.show.split(','))
    for r in sorted(rows, key=lambda x: x['addr']):
        if r['verdict'] in show:
            print('\n%08x [%s]\n     map  = %s\n     W_A  = %s  (callee %s %s)\n'
                  '     W_B  = %s  %s'
                  % (r['addr'], r['verdict'], (r['map_name'] or '(unmapped)')[:110],
                     r['W_A'], r['callee'], (r['callee_name'] or '')[:70],
                     r['W_B'], r['W_B_src'][:3]))
    if args.json:
        json.dump(rows, open(args.json, 'w'), indent=1)
        print(f'\nwrote {args.json}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
