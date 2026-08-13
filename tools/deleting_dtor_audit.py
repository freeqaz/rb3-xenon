#!/usr/bin/env python3
"""deleting_dtor_audit -- find map entries that score on a FALSE PAIRING because
retail's function there is a *deleting destructor* (`??_G` / `??_E`).

Provenance: lane DTOR-A (2026-08-13), the named follow-up to lane MAP-B
(`tools/classname_forwarder_audit.py`, commit 5b56326d).  MAP-B audited the
3rd-largest reloc-masked class (337 `ClassName` forwarders, 20.6% misidentified);
this is the LARGEST one.  Read-only unless --write-map; not a build input.

THE HAZARD
----------
A scalar deleting destructor is always the SAME 19 words on X360::

    mflr r12 / stw r12,-8(r1) / std r30,-0x18(r1) / std r31,-0x10(r1)
    stwu r1,-0x70(r1) / mr r31,r3 / mr r30,r4
    bl <this class's ??1 destructor>              <-- +0x1c
    rlwinm. r11,r30,0,31,31 / beq +0xc / mr r3,r31
    bl <operator delete>                          <-- +0x2c
    mr r3,r31 / addi r1,r1,0x70 / lwz r12,-8(r1) / mtlr r12
    ld r30,-0x18(r1) / ld r31,-0x10(r1) / blr

17 of the 19 words are IDENTICAL for every class; only the two `bl`
displacements differ -- and the graded ruler runs `functionRelocDiffs=none`,
which masks exactly those.  So all 577 of these functions are ONE reloc-masked
equivalence class: ANY bijection of their names over their addresses scores 100
on every pair.  A wrong name still scores.  The map documents this itself under
`_bijection_arbitrary` -- but only 35% of the rows this tool corrects were
flagged there.

THE INSTRUMENT: the two words the ruler masks.
  W_B (PRIMARY)  which class's VTABLE CONTAINS this function.  A deleting dtor
                 is the class's own vtable dtor slot (612 of 628 refs are slot
                 0), and destructors are never inherited -- every class emits
                 its own.  So a single owner IS the class.
  W_A (CORROB.)  resolve the `bl` at +0x1c and ask which vtables that destructor
                 installs.  Independent of W_B: it reads code, not tables.

★ POSITIVE CONTROL (the whole reason to trust the MISIDENTIFIED verdicts):
W_A confirms W_B on **92.0%** of the rows the map already gets RIGHT and
**89.9%** of the rows this tool calls WRONG.  Equal performance on both
populations => the misidentifications are not an artifact of the witness failing
selectively where a correction is proposed.

★ TRAP 1 -- THE SHAPE IS NECESSARY, NOT SUFFICIENT.  MAP-B had 9 shape-matches
whose callee was not a StaticClassName; judging on shape alone would have
"corrected" 5 CORRECT entries.  Here the callee must actually BE a destructor:
the +0x2c target must be one of the handful of `operator delete`s, and the
+0x1c target must install a vtable or be a named `??1`.  Rows failing this are
reported NOT_JUDGED and never rewritten.

★ TRAP 2 -- A SHARED CALLEE IS NOT EVIDENCE, AND INJECTIVITY IS THE CHECK THAT
CATCHES IT.  Two `??_G`s legitimately call the same `??1` when the destructors
themselves were ICF-folded.  The map must stay INJECTIVE ON NAME (see the map's
own `_name_injectivity_comment`).  Asserting it caught 4 collisions in this
lane -- in every one, the incumbent address was referenced ONLY from `.pdata`
(no vtable anywhere, i.e. zero identity evidence) while the rewrite target held
slot 0 of that exact class's vtable.  The assert is not a formality; run it.

★ TRAP 3 -- THE SPLITS PIN CAN BE CIRCULAR.  MAP-B found 26 rows sitting on
48-byte sliver `.text` blocks that had themselves been filled BY BYTE SIGNATURE
over the same masked class, so using the pin to disambiguate is circular.  This
tool answers that trap STRUCTURALLY: it uses NO unit / splits / source-path
evidence at ALL.  Every verdict comes from retail RTTI and retail code bytes.
The sliver population is still counted and printed, as a standing reminder.

★ TRAP 4 -- A DIRECTORY HEURISTIC IS NOT A SOURCE WITNESS.  MAP-B's Rnd/Dx/Ng
directory rule rewrote a CORRECT `NgLight` because NgLight lives in
`rndobj/Lit_NG.h`.  No directory or filename heuristic appears here either.

★★ TRAP 5 -- **NEW, AND IT WOULD HAVE MANUFACTURED ~100 FALSE CORRECTIONS.**
OUR HEADERS AND RETAIL DISAGREE ON TEMPLATE ARITY.  `src/system/obj/Object.h`
declares `template <class T> class ObjPtr`, but retail's is a 2-parameter
`ObjPtr<T, ObjectDir>`; meanwhile `ObjPtrList` is 2-parameter on BOTH sides and
`ObjDirPtr<ObjectDir>`'s only argument IS `VObjectDir@@`.  A map name is a
PAIRING KEY AGAINST OUR BUILD, not a claim about retail's mangling, so comparing
raw mangled strings reads every 1-arg-vs-2-arg row as MISIDENTIFIED.  Before
normalisation this tool measured 49% error; after, 20.7% on the dialect-immune
(non-template) rows -- which independently reproduces MAP-B's 20.6%.  A blind
"strip a trailing VObjectDir@@" is WRONG (it would gut `ObjDirPtr<ObjectDir>`),
so `canon()` parses the argument list and drops the DEFAULTED trailing argument
only when there are >=2 arguments.

WHAT THIS TOOL DELIBERATELY DOES NOT DO
---------------------------------------
* It does not touch the 87 anonymous members (they pay ~0 and each needs its own
  adjudication), nor the 22 rows whose function is in NO vtable (referenced only
  from `.pdata`) -- for those W_B is silent and there is NO evidence of error,
  so leaving them alone is the honest action, not laziness.
* ANONYMOUS BEATS WRONG: when the true class is known but our build emits no
  destructor symbol for it, the entry is REMOVED rather than given a synthesised
  name.
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
MAP = os.path.join(ROOT, 'scripts/target_symbol_map.json')
SPLITS = os.path.join(ROOT, 'config/45410914/splits.txt')
OBJDIR = os.path.join(ROOT, 'build/45410914/src')

#: the 19-word deleting-destructor body; None = the two masked `bl` slots.
DTOR = [0x7d8802a6, 0x9181fff8, 0xfbc1ffe8, 0xfbe1fff0, 0x9421ff90,
        0x7c7f1b78, 0x7c9e2378, None, 0x57cb07ff, 0x4182000c, 0x7fe3fb78,
        None, 0x7fe3fb78, 0x38210070, 0x8181fff8, 0x7d8803a6, 0xebc1ffe8,
        0xebe1fff0, 0x4e800020]
BL_DTOR, BL_DELETE = 0x1c, 0x2c
SIZE = len(DTOR) * 4

DG = re.compile(r'^\?\?_[GE](.+)@@[A-Z]{2,3}PAXI@Z$')


def bl_target(a, w):
    if (w >> 26) != 18 or not (w & 1) or (w & 2):
        return None
    li = w & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    return (a + li) & 0xFFFFFFFF


# ------------------------------------------------------- TRAP 5: dialect ----
def split_args(inner):
    """Split an MSVC template argument list; None if anything is unparseable."""
    args, i = [], 0
    while i < len(inner):
        c = inner[i]
        if c in 'VUTPQA':
            j = inner.find('@@', i + 1)
            if j < 0:
                return None
            args.append(inner[i:j + 2]); i = j + 2
        elif c == '_':
            args.append(inner[i:i + 2]); i += 2
        elif c == '$':
            j = inner.find('@', i + 1)
            if j < 0:
                return None
            args.append(inner[i:j + 1]); i = j + 1
        elif c in 'CDEFGHIJKMNOX':
            args.append(c); i += 1
        else:
            return None
    return args


def canon(cls):
    """Dialect-insensitive class key.  Drops a DEFAULTED trailing `VObjectDir@@`
    template argument -- and ONLY when >=2 arguments exist, so the single
    meaningful argument of `?$ObjDirPtr@VObjectDir@@` survives (TRAP 5)."""
    if not cls.startswith('?$'):
        return cls
    j = cls.find('@')
    if j < 0:
        return cls
    base, inner = cls[2:j], cls[j + 1:]
    a = split_args(inner)
    if a is None:
        return cls
    if len(a) >= 2 and a[-1] == 'VObjectDir@@':
        a = a[:-1]
    return '?$' + base + '@' + ''.join(a)


def td2cls(t):
    """'.?AVFoo@@' -> 'Foo'."""
    return t[4:-2] if t.startswith(('.?AV', '.?AU')) and t.endswith('@@') else None


# ------------------------------------------------- our build's own symbols ---
def our_dtor_symbols(objdir=OBJDIR):
    """Deleting-dtor symbols OUR build actually emits.  A map name is a pairing
    key against this side; a name we never emit pairs with nothing."""
    out = collections.defaultdict(set)
    for dp, _, fns in os.walk(objdir):
        for fn in fns:
            if not fn.endswith('.obj'):
                continue
            path = os.path.join(dp, fn)
            try:
                d = open(path, 'rb').read()
            except OSError:
                continue
            if len(d) < 20:
                continue
            _m, _ns, _ts, so, nsym, _oh, _ch = struct.unpack_from('<HHlIIHH', d, 0)
            if so == 0 or nsym == 0 or so + 18 * nsym > len(d):
                continue
            strt = so + 18 * nsym
            i = 0
            while i < nsym:
                o = so + 18 * i
                secnum, = struct.unpack_from('<h', d, o + 12)
                naux = d[o + 17]
                if d[o:o + 4] == b'\0\0\0\0':
                    off, = struct.unpack_from('<I', d, o + 4)
                    e = d.find(b'\0', strt + off)
                    name = d[strt + off:e].decode('latin1')
                else:
                    name = d[o:o + 8].rstrip(b'\0').decode('latin1')
                if secnum > 0 and name.startswith(('??_G', '??_E')):
                    out[name].add(path)
                i += 1 + naux
    return out


# ------------------------------------------------------------------ audit ----
class Audit:
    def __init__(self):
        self.R = RetailRtti()
        self.ext = self.R.extents
        self.map = json.load(open(MAP))
        self.named = {int(k, 16): v for k, v in self.map.items()
                      if k.lower().startswith('0x') and isinstance(v, str)}
        self.text = [s for s in self.R.sections if s.name == '.text'][0]
        self._slivers = self._load_sliver_pins()

    def is_text(self, v):
        return v is not None and self.text.va <= v < self.text.va + self.text.vsize

    # -- TRAP 3: counted and PRINTED, but never consulted ------------------
    def _load_sliver_pins(self):
        pins, cur = [], None
        try:
            fh = open(SPLITS)
        except OSError:
            return []
        for line in fh:
            m = re.match(r'^(\S+):\s*$', line)
            if m:
                cur = m.group(1); continue
            m = re.match(r'\s+\.text\s+start:(0x[0-9a-fA-F]+)\s+end:(0x[0-9a-fA-F]+)', line)
            if m and cur:
                s, e = int(m.group(1), 16), int(m.group(2), 16)
                if e - s <= 96:
                    pins.append((s, e))
        return pins

    def on_sliver(self, a):
        return any(s <= a < e for s, e in self._slivers)

    # -- W_B: which class's vtable CONTAINS this function ------------------
    def owners(self, a, maxback=400):
        """Strict: every word from the vtable head down to the slot must be a
        .text pointer, so a coincidental data word cannot fabricate an owner.
        (Verified identical to the loose scan on all 577 rows -- 0 differences.)"""
        out = []
        for slot in self.R.word_refs(a, sections=None):
            head = None
            for back in range(0, maxback * 4, 4):
                h = slot - back
                n = self.R.class_of_vtable(h)
                if n:
                    head = (h, n); break
                if not self.is_text(self.R.u32(h)):
                    break
            if head is None:
                continue
            h, n = head
            if all(self.is_text(self.R.u32(h + o)) for o in range(0, slot - h + 4, 4)):
                out.append((n, h, (slot - h) // 4))
        return out

    # -- census ------------------------------------------------------------
    def census(self):
        members = []
        for a, n in sorted(self.ext.items()):
            if n != SIZE:
                continue
            words = [self.R.u32(a + i) for i in range(0, SIZE, 4)]
            if any(w is None for w in words):
                continue
            if any(e is not None and words[i] != e for i, e in enumerate(DTOR)):
                continue
            members.append(a)
        return members

    def run(self):
        members = self.census()
        # TRAP 1 -- the +0x2c callee must be a real `operator delete`: shared by
        # >=2 members and itself never a vtable member.  Derived from the data,
        # not hardcoded, so it stays true if the population changes.
        dels = collections.Counter(bl_target(a + BL_DELETE, self.R.u32(a + BL_DELETE))
                                   for a in members)
        opdel = {t for t, c in dels.items() if t and c >= 2 and not self.owners(t)}
        rows = []
        for a in members:
            d = bl_target(a + BL_DTOR, self.R.u32(a + BL_DTOR))
            dl = bl_target(a + BL_DELETE, self.R.u32(a + BL_DELETE))
            name = self.named.get(a)
            # TRAP 1: is the +0x1c callee actually a DESTRUCTOR?
            st, _sc, inst = (self.R.classes_installed_by(d) if d else ('NONE', None, []))
            dtor_ok = bool(inst) or str(self.named.get(d, '')).startswith('??1')
            if dl not in opdel or not dtor_ok:
                # ★ retail_rtti doctrine: UNBOUNDED (a LEAF callee with no
                # .pdata entry) means UNDECIDABLE, *not* "installs nothing".
                # Both are refusals here, but conflating the two labels is the
                # exact error retail_rtti exists to prevent, so keep them apart.
                if dl not in opdel:
                    why = '+0x2c is not an operator delete'
                elif st == 'UNBOUNDED':
                    why = '+0x1c callee is a LEAF -- undecidable, not disproved'
                else:
                    why = '+0x1c callee installs no vtable / is not a named ??1'
                rows.append(dict(addr=a, map_name=name, verdict='NOT_JUDGED',
                                 true=None, owners=[], why=why))
                continue
            oc = sorted({canon(td2cls(x[0])) for x in self.owners(a) if td2cls(x[0])})
            wa = {canon(td2cls(n)) for _v, n in inst if td2cls(n)}
            if name is None:
                v, why = 'ANONYMOUS', ''
            elif not oc:
                v, why = 'UNRESOLVED', 'function is in NO vtable (referenced only from .pdata)'
            else:
                im = canon(DG.match(name).group(1)) if DG.match(name) else None
                v = 'CONSISTENT' if im in oc else 'MISIDENTIFIED'
                why = ('W_A confirms' if (set(oc) & wa) else
                       'W_A silent' if not wa else 'W_A differs (base-vtable restore)')
            rows.append(dict(addr=a, map_name=name, verdict=v, true=oc, owners=oc,
                             why=why, wa=sorted(wa), sliver=self.on_sliver(a)))
        return rows


# --------------------------------------------------------------- repair ----
def build_plan(rows, ours):
    by_canon = collections.defaultdict(list)
    for s in ours:
        m = DG.match(s)
        if m:
            by_canon[canon(m.group(1))].append(s)
    plan = []
    for r in rows:
        if r['verdict'] != 'MISIDENTIFIED':
            continue
        oc = r['true']
        if len(oc) == 1:
            t = by_canon.get(oc[0], [])
            if len(t) == 1:
                plan.append((r['addr'], r['map_name'], t[0], oc[0], 'REWRITE'))
            elif not t:
                plan.append((r['addr'], r['map_name'], None, oc[0], 'REMOVE_not_emitted'))
            else:
                plan.append((r['addr'], r['map_name'], None, oc[0], 'REMOVE_ambiguous_GE'))
        else:
            plan.append((r['addr'], r['map_name'], None, '|'.join(oc), 'REMOVE_icf_group'))
    return plan


def injectivity(mapping, allow):
    d = collections.defaultdict(list)
    for k, v in mapping.items():
        if k.lower().startswith('0x') and isinstance(v, str):
            d[v].append(k.lower())
    return {k: v for k, v in d.items() if len(v) > 1 and k not in allow}


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--json', help='write full rows here')
    ap.add_argument('--show', default='MISIDENTIFIED',
                    help='comma-separated verdicts to print (or NONE)')
    ap.add_argument('--write-map', action='store_true',
                    help='apply the repair to scripts/target_symbol_map.json')
    args = ap.parse_args()

    au = Audit()
    rows = au.run()
    c = collections.Counter(r['verdict'] for r in rows)
    print(f'deleting-destructor masked class: {len(rows)} members of {SIZE} bytes')
    print(f'  named: {sum(1 for r in rows if r["map_name"])}   {dict(c)}')
    nj = [r for r in rows if r['verdict'] == 'NOT_JUDGED']
    print(f'  TRAP 1 refusals (callee is not a destructor / delete): {len(nj)}')
    for r in nj[:10]:
        print(f'      {r["addr"]:08x}  {r["map_name"]}  -- {r["why"]}')
    print(f'  TRAP 3: rows on a sliver .text pin: '
          f'{sum(1 for r in rows if r.get("sliver"))} '
          f'(NO unit/splits evidence is used by this tool, so none can bias a verdict)')
    judged = c['CONSISTENT'] + c['MISIDENTIFIED']
    if judged:
        print(f'  ERROR RATE: {c["MISIDENTIFIED"]}/{judged} = '
              f'{100*c["MISIDENTIFIED"]/judged:.1f}% of judged named rows')
    # ★ Split out the DIALECT-IMMUNE stratum.  Template rows are the ones TRAP 5
    # could in principle still distort, so the non-template rows are the control
    # that stands on its own -- and it independently reproduces MAP-B's 20.6%.
    for label, pred in (('non-template (dialect-immune CONTROL)',
                         lambda n: not DG.match(n).group(1).startswith('?$')),
                        ('template', lambda n: DG.match(n).group(1).startswith('?$'))):
        sub = [r for r in rows if r['verdict'] in ('CONSISTENT', 'MISIDENTIFIED')
               and DG.match(r['map_name'] or '') and pred(r['map_name'])]
        bad = sum(1 for r in sub if r['verdict'] == 'MISIDENTIFIED')
        if sub:
            print(f'      {label}: {bad}/{len(sub)} = {100*bad/len(sub):.1f}%')
    # positive control
    pc = collections.Counter((r['verdict'], r['why']) for r in rows
                             if r['verdict'] in ('CONSISTENT', 'MISIDENTIFIED'))
    print('  POSITIVE CONTROL (independent W_A vs W_B):')
    for k in sorted(pc):
        print(f'      {k[0]:<14} {k[1]:<34} {pc[k]}')

    show = set(args.show.split(','))
    for r in rows:
        if r['verdict'] in show:
            print('%08x [%s] map=%s\n       TRUE=%s  (%s)'
                  % (r['addr'], r['verdict'], r['map_name'], r['true'], r['why']))

    if args.json:
        json.dump(rows, open(args.json, 'w'), indent=1, default=str)
        print(f'wrote {args.json}')

    if args.write_map:
        ours = our_dtor_symbols()
        if not ours:
            print('REFUSING --write-map: no compiled .obj symbols found; the '
                  'repair needs to know which names OUR build emits.')
            return 2
        plan = build_plan(rows, ours)
        m = json.load(open(MAP))
        new = dict(m)
        for a, old, tgt, cls, kind in plan:
            k = '0x%08x' % a
            k = next((x for x in m if x.lower() == k), k)
            if kind == 'REWRITE':
                new[k] = tgt
            else:
                new.pop(k, None)
        allow = set(m.get('_internal_linkage_allow', []))
        before = injectivity(m, allow)
        dup = injectivity(new, allow)
        touched = {'0x%08x' % a for a, *_ in plan}
        intro = {k: v for k, v in dup.items() if k not in before}
        # TRAP 2: a collision means the name we are writing already sits
        # somewhere.  Strip it from whichever address has NO vtable evidence.
        stripped = 0
        for k, vs in intro.items():
            for va in vs:
                if va not in touched:
                    kk = next((x for x in new if x.lower() == va), None)
                    if kk and not au.owners(int(va, 16)):
                        new.pop(kk); stripped += 1
        dup2 = injectivity(new, allow)
        print(f'\nplan: {collections.Counter(k for *_x, k in plan)}')
        print(f'INJECTIVITY: baseline dups {len(before)}, after plan {len(dup)} '
              f'(introduced {len(intro)}), stripped {stripped} unevidenced '
              f'incumbents -> final {len(dup2)}')
        if len(dup2) > len(before):
            print('REFUSING to write: the plan would leave NEW duplicate names.')
            return 3
        bij = [x for x in new.get('_bijection_arbitrary', [])
               if x.lower() not in touched]
        new['_bijection_arbitrary'] = bij
        json.dump(new, open(MAP, 'w'), indent=1)
        print(f'wrote {MAP}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
