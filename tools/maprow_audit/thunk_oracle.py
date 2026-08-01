#!/usr/bin/env python3
"""Adjudicate forwarder-thunk map rows with the MAP-INDEPENDENT RTTI oracle.

Lane CI-2 (2026-08-01).  Lane CH-1 swept the forwarder-thunk population and
left ~228 METHOD_DIFFERS + ~250 TARGET_UNNAMED rows unrepaired, stating its
blocker exactly: its branch channel "CANNOT separate 'thunk row wrong' from
'TARGET row wrong' -- it blames the thunk either way."  This tool supplies the
independent oracle, built on lane CH-4's rtti_vtable_index.

** THE ORACLE IS ONE-SIDED, AND THAT IS STRUCTURAL, NOT A DEFECT. **
A thunk's whole purpose is to occupy a vtable slot, so the thunk address is in
some retail vtable 227/228 = 99.6% of the time -- while the body it forwards to
is reachable only THROUGH it and appears in a vtable just 7/228 = 3.1% of the
time (AGREE control: 99.9% vs 5.5%).  So RTTI can corroborate the THUNK row
directly and can never corroborate the TARGET row the same way.

CHANNEL 1 -- RTTI ANCESTRY (new here; CH-4 validated ??_R3 but never decoded it)
    ??_R4 COL +16 -> ??_R3 +8 numBaseClasses, +12 -> ??_R2 array -> ??_R1 +0 -> ??_R0

  ** "claimed class == RTTI class of the containing vtable" is a DEAD INDEX.
     DO NOT USE IT. **  MSVC names a vtordisp thunk after the class that
     DECLARED the method, while the thunk physically lives in the COMPLETE
     object's vtable -- so `?PostSave@ObjectDir@@$4...` sits in PatchDir's
     vtable and reads as a defect.  Measured: 25.8% of KNOWN-GOOD AGREE rows
     "contradicted", vs 43.0% of METHOD_DIFFERS = 1.67x enrichment.  Acting on
     it would have condemned ~400 correct rows.

  The correct relation is ANCESTOR-OF (claimed class is the host class or one
  of its RTTI base classes):
        AGREE (false-positive floor)        2.2%
        METHOD_DIFFERS                     26.8%   => 12.2x
        null: ancestor sets shuffled between classes, 3 seeds
                                    AGREE 24.0 / 24.7 / 24.2%
  i.e. the ancestry information is load-bearing and the detector fails on demand.

CHANNEL 2 -- VTABLE-FAMILY SLOT CONSENSUS
  A derived class's vtable mirrors the base's slot ORDER, and every
  non-overridden slot literally repeats the base's function VA.  So vtables
  sharing >=3 slot VAs at IDENTICAL indices are one family (map-independent),
  and within a family slot i is the same METHOD in every member.  Other members'
  slot i therefore VOTE on the method without consulting the row under test.
        reproduces the AGREE row's method              89.4%
        METHOD_DIFFERS                                  5.0%
        null: vote at a RANDOM slot index, 3 seeds  3.8 / 3.6 / 4.2%   (~23x)

RESULT -- the split lane CH-1 could not make (n=228 METHOD_DIFFERS):
        THUNK-wrong  (either channel condemns the thunk)   200   (87.7%)
        TARGET-wrong (thunk corroborated by BOTH channels)   7   ( 3.1%)
        unadjudicable (no vtable / no slot consensus)       21   ( 9.2%)
  => CH-1's instrument was RIGHT to blame the thunk ~96% of the time.

** A CANDIDATE FROM THIS ORACLE IS NOT A REPAIR. ** Candidates must be VERIFIED
by simulating the edit and re-running CH-1's branch channel (thunk_identity.
adjudicate_strict), an instrument not used to generate them.  Of 4 mutual swap
pairs it REJECTED 2 -- including the ONLY pair that passed mutual+unique+
same-unit, which was therefore the edit most likely to be landed on the strength
of the oracle alone.

** A CHANNEL THAT LOOKS EQUALLY GOOD AND IS A DEAD INDEX: class RELATEDNESS **
("thunk class and target class must be related in the hierarchy").  Its
false-positive rate on CH-1's known-legitimate AGREE_METHOD population is 75%
(24/32 rows where both classes have RTTI).  Note the natural control -- AGREE
rows -- is VACUOUS for it: AGREE means the classes are EQUAL, so relatedness
holds by construction and can never fail.
  ⚠ This channel produced a false alarm that the better instrument REFUTED: it
  suggested CH-1's AGREE_METHOD bucket was contaminated.  Channel 2 gives
  AGREE_METHOD 100.0% (73/73) -- BETTER than AGREE's own 89.4%.  CH-1's
  classification STANDS; the unrelated-class pairs are the ICF-fold confound
  rtti_vtable_index already documents.

NAME SYNTHESIS (for TARGET_UNNAMED): a thunk's mangled name CONTAINS the body's
full signature -- strip `@@$<n>PPPPPPPM@<adj>@` and insert the access code
($0->E private, $2->M protected, $4->U public virtual); adjustor `W` -> `U`.
  control: reproduces the real target row on 1565/1565 = 100.0% of AGREE rows.
  ⚠ The FIRST version scored 1.9% by walking straight into the two traps
  thunk_identity.py documents: the adjustor regex fired on the PARAMETER type
  `W4CopyType@23@@Z`, and the `$4` regex demanded two numeric fields after
  `PPPPPPPM@` when that literal IS the first field.  The `(?=[A-Z]{2})` guard
  and the single-field form are both load-bearing.
  ⚠ `??_E` is a weak external aliasing `??_G`: accept either or you manufacture
  false defects.
"""
import sys, os, json, collections, random, re

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..'))
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))
from rtti_vtable_index import Rtti, demangle_class, pdata_starts      # noqa: E402
from retail_reader import Image as RImage                            # noqa: E402
from thunk_identity import (Image as TImage, thunk_kind, code_population,  # noqa: E402
                            adjudicate_strict, _adjustment_from_code,
                            _adjustment_from_name)

BAND = 'orig/45410914/band.exe'
MAP = 'scripts/target_symbol_map.json'
_SPECIAL = ("??_E", "??_G", "??1", "??0", "??_D", "??_7")
ACC = {'0': 'E', '2': 'M', '4': 'U'}
_VT = re.compile(r'@@\$([0-4])PPPPPPPM@(?:[A-P]+@|[0-9])')
_ADJ = re.compile(r'@@([WXOPGH])(?:[A-P]+@|[0-9])(?=[A-Z]{2})')


def qcls(n):
    """FULL qualified class scope: '?M@Cls@Ns@@...' -> 'Cls@Ns'."""
    if not isinstance(n, str) or not n.startswith('?'):
        return None
    for pre in _SPECIAL:
        if n.startswith(pre):
            return n[len(pre):].split('@@')[0] or None
    p = n[1:].split('@@')[0].split('@')
    return '@'.join(p[1:]) if len(p) >= 2 else None


def method_of(n):
    if not isinstance(n, str) or not n.startswith('?'):
        return None
    for pre in _SPECIAL[:5]:
        if n.startswith(pre):
            return 'DTOR'
    if n.startswith('??$'):
        i = n.find('@')
        return n[3:i] if i > 0 else None
    return n[1:].split('@@')[0].split('@')[0]


def synth(name):
    """thunk name -> plain virtual body name(s) it must forward to."""
    if not isinstance(name, str):
        return []
    m = _VT.search(name)
    if m:
        base = name[:m.start()] + '@@' + ACC[m.group(1)] + name[m.end():]
    else:
        m = _ADJ.search(name)
        if not m:
            return []
        base = name[:m.start()] + '@@U' + name[m.end():]
    out = [base]
    if base.startswith('??_E'):
        out.append('??_G' + base[4:])
    if base.startswith('??_G'):
        out.append('??_E' + base[4:])
    return out


class Oracle(Rtti):
    def prepare(self, tmap):
        self.tmap = tmap
        self.build_attribution()
        self.anc = {}
        for col, d in self.cols.items():
            nm = demangle_class(self.tds.get(d['td'], '') or '')
            r3 = self.img.word(col + 16)
            if r3 is None:
                continue
            nb, arr = self.img.word(r3 + 8), self.img.word(r3 + 12)
            if nb is None or arr is None or not (0 < nb < 200):
                continue
            names = set()
            for i in range(nb):
                p = self.img.word(arr + 4 * i)
                td = self.img.word(p) if p is not None else None
                n2 = self.tds.get(td)
                if n2:
                    names.add(demangle_class(n2))
            if nm:
                self.anc.setdefault(nm, set()).update(names | {nm})
        self.vt = {vt: sl for vt, (nm, off, sl) in self.vt_slots.items()}
        self.idx = collections.defaultdict(set)
        for vt, sl in self.vt.items():
            for i, va in enumerate(sl):
                self.idx[(i, va)].add(vt)
        return self

    def family(self, vt, min_share=3):
        cnt = collections.Counter()
        for i, va in enumerate(self.vt[vt]):
            for w in self.idx[(i, va)]:
                if w != vt:
                    cnt[w] += 1
        return {w for w, c in cnt.items() if c >= min_share}

    def hosts(self, va):
        return {demangle_class(k) for k in self.attr.get(va, {})}

    def class_ok(self, addr, name):
        """True/False/None -- claimed class is a host class or an ANCESTOR of one."""
        c = qcls(name)
        h = self.hosts(int(addr, 16))
        if not h or not c:
            return None
        return any(c == x or c in self.anc.get(x, set()) for x in h)

    def slot_consensus(self, addr, margin=0.75, jitter=False):
        A = int(addr, 16)
        votes = collections.Counter()
        for _cls, lst in self.attr.get(A, {}).items():
            for (vt, i, _off) in lst:
                j = random.randrange(max(len(self.vt[vt]), 1)) if jitter else i
                for w in self.family(vt):
                    sl = self.vt[w]
                    if j >= len(sl) or sl[j] == A:
                        continue
                    m = method_of(self.tmap.get('0x%08x' % sl[j]))
                    if m:
                        votes[m] += 1
        if not votes:
            return None
        top, n = votes.most_common(1)[0]
        return top if n / sum(votes.values()) >= margin else None


def load(exe=BAND, mp=MAP):
    tmap = json.load(open(mp))
    o = Oracle(RImage(exe)).prepare(tmap)
    return o, tmap, TImage(exe)


def selftest():
    o, tmap, timg = load()
    recs = [adjudicate_strict(timg, tmap, a, n)
            for a, n in sorted(code_population(timg, tmap).items())]
    by = collections.defaultdict(list)
    for r in recs:
        by[r['verdict']].append(r)
    ok = True
    print('population %d  %s' % (len(recs), {k: len(v) for k, v in by.items()}))

    # C1 POSITIVE, DIRECTIONAL: known inheritance must hold, reverse must not.
    for c, b, want in [('PatchDir', 'ObjectDir', True), ('RndDir', 'RndDrawable', True),
                       ('ObjectDir', 'PatchDir', False)]:
        got = b in o.anc.get(c, set())
        print('  [C1] %-12s derives from %-12s = %-5s want %-5s -> %s'
              % (c, b, got, want, 'OK' if got == want else 'BROKEN'))
        ok &= got == want

    # C2 the AGREE population is the false-positive FLOOR of each channel.
    def rate(grp, fn):
        s = by[grp]
        return 100.0 * sum(1 for r in s if fn(r)) / max(len(s), 1)
    fa = rate('AGREE', lambda r: o.class_ok(r['addr'], r['proposed']) is False)
    fm = rate('METHOD_DIFFERS', lambda r: o.class_ok(r['addr'], r['proposed']) is False)
    print('  [C2] ancestry channel: AGREE floor %.1f%%  METHOD_DIFFERS %.1f%%  -> %.1fx'
          % (fa, fm, fm / max(fa, 0.01)))
    ok &= fa < 6.0 and fm / max(fa, 0.01) > 4.0

    # C3 FAIL ON DEMAND: shuffle ancestor sets; the AGREE floor must EXPLODE.
    real = dict(o.anc)
    keys = list(real)
    random.seed(5)
    vals = [real[k] for k in keys]
    random.shuffle(vals)
    o.anc = dict(zip(keys, vals))
    fs = rate('AGREE', lambda r: o.class_ok(r['addr'], r['proposed']) is False)
    o.anc = real
    print('  [C3] fail-on-demand: shuffled ancestry AGREE floor %.1f%% (real %.1f%%) -> %s'
          % (fs, fa, 'OK' if fs > 4 * fa else 'BROKEN (channel fits noise)'))
    ok &= fs > 4 * fa

    # C4 slot channel + its randomized-slot null.
    sa = rate('AGREE', lambda r: o.slot_consensus(r['addr']) == method_of(r['proposed']))
    sm = rate('METHOD_DIFFERS', lambda r: o.slot_consensus(r['addr']) == method_of(r['proposed']))
    random.seed(11)
    sn = rate('AGREE', lambda r: o.slot_consensus(r['addr'], jitter=True) == method_of(r['proposed']))
    print('  [C4] slot channel: AGREE %.1f%%  METHOD_DIFFERS %.1f%%  RANDOM-SLOT null %.1f%%'
          % (sa, sm, sn))
    ok &= sa > 80 and sn < 10

    # C5 name synthesis must reproduce the REAL target row on every AGREE row.
    n = len(by['AGREE'])
    hit = sum(1 for r in by['AGREE'] if r['target_row'] in synth(r['proposed']))
    print('  [C5] synthesis reproduces AGREE target row %d/%d = %.1f%%' % (hit, n, 100.0 * hit / n))
    ok &= hit == n

    # C6 FAIL ON DEMAND for C5: a WRONG access code must not reproduce them.
    global ACC
    good = ACC
    ACC = {'0': 'E', '2': 'E', '4': 'E'}
    bad = sum(1 for r in by['AGREE'] if r['target_row'] in synth(r['proposed']))
    ACC = good
    print('  [C6] fail-on-demand: wrong access code reproduces %d/%d -> %s'
          % (bad, n, 'OK' if bad < n else 'BROKEN (synthesis ignores the code)'))
    ok &= bad < n

    print('SELFTEST', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(selftest())
