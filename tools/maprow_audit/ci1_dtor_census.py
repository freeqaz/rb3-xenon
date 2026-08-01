#!/usr/bin/env python3
"""LANE CI-1: adjudicate the ??_G / ??_E destructor map rows that retail's own
vtables contradict.

Built on lane CH-4's map-independent oracle (rtti_vtable_index / vtable_attr):
  ??_R0 TypeDescriptor -> ??_R4 COL -> vtable -> slots, all retail bytes.

WHAT THIS ADDS OVER CH-4
------------------------
1. CORRECTED DETHUNK. CH-4's `dethunk` recurses purely by SHAPE, so a real
   function whose body is a tail-call is indistinguishable from a forwarder and
   gets skipped over -- attributing the slot to the tail-call TARGET's class.
   Here recursion continues only through ANONYMOUS intermediates (a node with a
   map name is a real function: stop there). The two variants are computed side
   by side so the artifact is MEASURED, not assumed.

2. SLOT-ROLE instrument. "slot 0 == destructor" is a Milo Hmx::Object
   CONVENTION, not a rule (RndDrawable puts UpdateSphere first, and
   UIPanel-derived classes put Load first). So instead of assuming slot 0, we
   ask: at the slot index where this VA sits in class C's vtable, what do OTHER
   classes put? If the same index holds a ??_G/??_E in sibling/base vtables,
   the role is "deleting destructor" and the row is a destructor row for C.

3. Injectivity + home-unit gates before any repair is proposed.

INSTRUMENT INDEPENDENCE (read this before believing a conjunction)
  A  vtable membership   -- .rdata layout: is VA a slot of the vtable RTTI
                            labels C?
  B  dtor_bl             -- .text bytes: first non-helper `bl` in the body,
                            then that callee's MAP name. NOT map-independent:
                            it inherits whatever error the ??1 row carries.
  C  vftable store       -- .text bytes: the vtable VA materialised (lis/addi)
                            inside the ??_G or its ??1 callee. Map-independent,
                            but shares the RTTI->vtable naming step with A, so
                            A and C are only PARTIALLY independent. Reported
                            separately; never counted as a second instrument
                            when A already fired on the same vtable.
"""
import sys, os, json, re, struct, collections, random

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))
from retail_reader import Image, insns, branch_target                 # noqa
from rtti_vtable_index import Rtti, demangle_class, pdata_sizes, pdata_starts  # noqa
from vtable_attr import thunk_target, dethunk as shape_dethunk        # noqa

ROOT = os.path.join(HERE, '..', '..')
MAPP = os.path.join(ROOT, 'scripts/target_symbol_map.json')
# MSVC save/restore helpers etc: opcode allow-list copied from CH-4's audit
ALLOW = {36, 38, 44, 46, 54, 55, 50, 51, 32, 34, 40, 42, 14, 31, 37, 53,
         62, 58, 60, 63}


def load_map():
    raw = json.load(open(MAPP))
    mi = {}
    for k, v in raw.items():
        if k.startswith('0x') and isinstance(v, str):
            mi[int(k, 16)] = v
    return raw, mi


def named_dethunk(img, va, named, maxdepth=8):
    """Follow forwarder thunks, but recurse only through ANONYMOUS nodes.

    The start node is exempt (a vtable slot may itself be a named thunk); every
    node we LAND on that has a map name terminates the walk. Returns
    (final_va, depth, stopped_because)."""
    cur, seen = va, {va}
    for depth in range(maxdepth):
        t = thunk_target(img, cur)
        if t is None:
            return cur, depth, 'not_a_thunk'
        if t in seen:
            return cur, depth, 'cycle'
        seen.add(t)
        cur = t
        if cur in named:
            return cur, depth + 1, 'named'
    return cur, maxdepth, 'depth'


class Census:
    def __init__(self):
        self.img = Image()
        self.raw_map, self.mapi = load_map()
        self.named = set(self.mapi)
        self.r = Rtti(self.img)
        self.r.build_attribution()
        self.sizes = pdata_sizes(self.img)
        self._helper = {}
        self._build_attr()

    # ---------------------------------------------------------------- attrib
    def _build_attr(self):
        """attr_named / attr_shape: VA -> {class -> [(vt, slot, col_offset)]}"""
        self.attr_named = collections.defaultdict(lambda: collections.defaultdict(list))
        self.attr_shape = collections.defaultdict(lambda: collections.defaultdict(list))
        self.slot_named, self.slot_shape = {}, {}
        self.vt_of_class = collections.defaultdict(list)
        for vt, (nm, coloff, sl) in self.r.vt_slots.items():
            c = demangle_class(nm)
            self.vt_of_class[c].append((vt, coloff, sl))
            for i, fn in enumerate(sl):
                if fn not in self.slot_named:
                    self.slot_named[fn] = named_dethunk(self.img, fn, self.named)[0]
                    self.slot_shape[fn] = shape_dethunk(self.img, fn)[0]
                self.attr_named[self.slot_named[fn]][c].append((vt, i, coloff))
                self.attr_shape[self.slot_shape[fn]][c].append((vt, i, coloff))

    # ------------------------------------------------------------ instrument B
    def is_helper(self, va):
        if va in self._helper:
            return self._helper[va]
        r = False
        if not self.sizes.get(va):
            for a, i in insns(self.img, va, 96):
                if i == 0x4E800020:
                    r = True
                    break
                if (i >> 26) not in ALLOW:
                    break
        self._helper[va] = r
        return r

    def dtor_bl(self, va):
        sz = self.sizes.get(va)
        if not sz:
            return None
        for a, i in insns(self.img, va, sz):
            t = branch_target(a, i)
            if t and t[1]:
                if self.is_helper(t[0]):
                    continue
                return t[0]
        return None

    # ------------------------------------------------------------ instrument C
    def vftable_refs(self, va, follow=True):
        """Classes whose VTABLE ADDRESS is materialised inside this body
        (lis rX,hi ; addi rX,rX,lo) -- and optionally inside its first callee
        (the real ??1, which is where the vftable store usually lives)."""
        out = set()
        for a in ([va] + ([self.dtor_bl(va)] if follow else [])):
            if not a:
                continue
            sz = self.sizes.get(a)
            if not sz:
                continue
            his = {}
            for _, i in insns(self.img, a, min(sz, 4096)):
                op, rt = i >> 26, (i >> 21) & 31
                if op == 15:                       # lis rT, imm  (addis rT,0,imm)
                    if ((i >> 16) & 31) == 0:
                        his[rt] = (i & 0xFFFF) << 16
                elif op == 14:                     # addi rT, rA, imm
                    ra = (i >> 16) & 31
                    if ra in his:
                        lo = i & 0xFFFF
                        if lo & 0x8000:
                            lo -= 0x10000
                        cand = (his[ra] + lo) & 0xFFFFFFFF
                        if cand in self.r.vtables:
                            out.add(demangle_class(
                                self.r.tds[self.r.cols[self.r.vtables[cand]]['td']]))
        return {c for c in out if c}

    # -------------------------------------------------------------- slot role
    def slot_role(self, cls, slot_idx, coloff=0):
        """What do OTHER classes put at this slot index (same col_offset)?
        Returns Counter of map-name 'kinds' -- ??_G/??_E vs other."""
        kinds = collections.Counter()
        for va, cs in self.attr_named.items():
            for c, ents in cs.items():
                for vt, i, off in ents:
                    if i == slot_idx and off == coloff:
                        nm = self.mapi.get(va)
                        if nm:
                            kinds['dtor' if nm.startswith(('??_G', '??_E'))
                                  else 'other'] += 1
        return kinds


def cls_of_sym(s):
    m = re.match(r'\?\?_[GE]([A-Za-z_0-9@?$]+?)@@', s)
    return m.group(1) if m else None


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else '/home/free/tmp/laneCI1'
    os.makedirs(out_dir, exist_ok=True)
    C = Census()
    print(f'[i] vtables {len(C.r.vtables)}  attributed VAs(named-dethunk) '
          f'{len(C.attr_named)}  (shape-dethunk) {len(C.attr_shape)}')
    nflip = sum(1 for k in C.slot_named
                if C.slot_named[k] != C.slot_shape[k])
    print(f'[i] slots where named- and shape-dethunk DISAGREE: {nflip} / '
          f'{len(C.slot_named)}')

    rows = []
    for va, sym in sorted(C.mapi.items()):
        if not sym.startswith(('??_G', '??_E')):
            continue
        cls = cls_of_sym(sym)
        if not cls:
            continue
        an = {c: e for c, e in C.attr_named.get(va, {}).items()}
        ash = {c: e for c, e in C.attr_shape.get(va, {}).items()}
        rec = dict(addr=hex(va), sym=sym, cls=cls,
                   retail_named=sorted(an), retail_shape=sorted(ash),
                   slots=[[c, i, off] for c, e in an.items() for _, i, off in e])
        if not an:
            rec['verdict_named'] = 'SILENT'
        elif cls in an:
            rec['verdict_named'] = 'AGREE'
        else:
            rec['verdict_named'] = 'CONTRA'
        if not ash:
            rec['verdict_shape'] = 'SILENT'
        elif cls in ash:
            rec['verdict_shape'] = 'AGREE'
        else:
            rec['verdict_shape'] = 'CONTRA'
        rows.append(rec)

    cn = collections.Counter(r['verdict_named'] for r in rows)
    cs = collections.Counter(r['verdict_shape'] for r in rows)
    print(f'[census] {len(rows)} ??_G/??_E rows')
    print(f'   named-dethunk: {dict(cn)}')
    print(f'   shape-dethunk: {dict(cs)}')
    flips = [r for r in rows if r['verdict_named'] != r['verdict_shape']]
    print(f'   verdict FLIPS between the two dethunks: {len(flips)}')
    print('   flip breakdown: ' + str(collections.Counter(
        (r['verdict_shape'], r['verdict_named']) for r in flips)))

    # ---- NULL: shuffle row->address, recompute verdicts (CH-4's null) -------
    random.seed(4242)
    addrs = [int(r['addr'], 16) for r in rows]
    shuf = addrs[:]
    random.shuffle(shuf)
    nn = collections.Counter()
    for r, a2 in zip(rows, shuf):
        an = C.attr_named.get(a2, {})
        nn['SILENT' if not an else ('AGREE' if r['cls'] in an else 'CONTRA')] += 1
    print(f'[null] row->address shuffled: {dict(nn)}')

    json.dump(rows, open(os.path.join(out_dir, 'ci1_census.json'), 'w'), indent=1)
    print(f'-> {out_dir}/ci1_census.json')


if __name__ == '__main__':
    main()
