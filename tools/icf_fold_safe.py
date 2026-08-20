#!/usr/bin/env python3
"""Fold-poison-safe slot identity for any retail-vs-ours name comparison.

★ WHY THIS EXISTS AS A TYPE AND NOT AS A HELPER FUNCTION
========================================================

ICF folds identical COMDATs, so ONE retail address serves many classes' vtable
slots, and `target_symbol_map.json` can name that address with only ONE
arbitrary survivor spelling.  Comparing such a slot BY NAME conflates "folded"
with "wrong" -- the same disease that makes objdiff's `LINKER_MERGED` verdict
uninformative (see CLAUDE.md, "a tool's confident 'unfixable' is the claim most
worth auditing").

That much was already known, documented, and FIXED once.  It was then rebuilt
ONE DAY LATER by a new longest-common-prefix scan that forgave `<unnamed>`
retail slots but CHARGED folded ones, and confidently reported
`XboxContent INTERIOR@3` for a table whose slots 0-13 had already been read by
hand as aligning.  Slot 3 is a named ICF survivor
(`??$Obj@VCharPollable@@@DataNode@@...`) sitting in the `Location` slot.

⛔ SO THE LESSON IS NOT "EXTRACT A HELPER".  The author of that scan KNEW about
fold-poisoning and had personally fixed it the previous day.  A helper you must
REMEMBER TO CALL fails in exactly the same way, because the new scan did not
start from the comparator -- it started from the recorded slot COUNTS and
re-derived names itself.  There were already TWO copies of the
`occ[w] != 1 or within[w] != 1` predicate inside `vtable_order_sweep.py` alone
(its `sweep_class` and its `map_audit`); the poisoned scan was the third.

⇒ THE FIX IS TO MAKE THE UNSAFE OPERATION FAIL LOUDLY, not to make the safe
operation available.  `Slot.__eq__` RAISES `FoldPoisonError` when either side's
identity was destroyed by ICF or never named by the map.  A future scan that
writes the natural `if retail != ours:` therefore gets a traceback naming this
file, instead of a silent, confident, wrong verdict of the exact kind that
CLOSES VEINS and that nobody re-opens.

★ The general shape, worth reusing beyond vtables: when an instrument has a
class of inputs on which it CANNOT have an opinion, represent that class as a
value that refuses to answer -- never as a name that compares unequal.  An
"unknown" that silently reads as "different" manufactures defects; an "unknown"
that raises cannot.

Provenance: lane VTGRIND 2026-08-20;
`docs/decomp/VTABLE_SLOT_COUNT_FIXES_2026-08-20.md`,
`docs/decomp/patterns/icf-fold-poisoning.md`.
"""

import collections

__all__ = [
    'FoldPoisonError', 'Slot', 'INCOMPARABLE_REASONS',
    'normalize_dtor', 'fold_counts', 'retail_slots', 'our_slot_names',
    'comparable_pairs', 'assert_can_agree', 'VacuousInstrumentError',
    'access_class', 'name_is_nonvirtual', 'name_owned_by', 'exclusion_counts',
    'charge',
]


class FoldPoisonError(RuntimeError):
    """Raised when code compares a slot whose identity ICF destroyed.

    Seeing this is the guard WORKING.  The fix is never to catch it: it means
    the comparison being attempted cannot be answered by any instrument,
    because retail itself destroyed the distinction.  Filter with
    `comparable_pairs()` (or `Slot.comparable`) and EXCLUDE those slots -- do
    not score them as agreement either, which would manufacture SAMEs.
    """


class VacuousInstrumentError(RuntimeError):
    """Raised when a comparator produced zero agreements over a real population.

    The `SAME = 0` tell.  An instrument that can never agree is not reporting
    defects, it is broken -- the first full vtable sweep read
    `SAME=0 / SET_DIFFER=472` purely because our COFF vtable symbol leads with
    the `??_R4` Complete Object Locator and retail's does not, shifting every
    slot by one.
    """


INCOMPARABLE_REASONS = ('folded_across', 'folded_within', 'unnamed', 'absent',
                        'nonvirtual_name', 'unrelated_owner')

_VIRT = set('EMU')        # E private virtual, M protected virtual, U public virtual
_NONVIRT = set('QAIS')    # Q public, A private, I protected, S static


def access_class(sym):
    """MSVC member-function access class letter, or None if not decodable.

    ⚠ ADJUSTOR THUNKS MUST BE EXCLUDED.  `?F@C@@$4PPPPPPPM@A@AAX...` carries a
    displacement encoding between the `@@` and the real access class, so a
    naive scan reads a letter out of THAT and calls every thunk non-virtual --
    1,379 false positives (29.3%) before the exclusion, 47 (1.6%) after.
    `??`-prefixed names (dtors, operators) are not decodable here and return
    None, which callers must treat as "no opinion", never as non-virtual.
    """
    import re
    if not sym or not sym.startswith('?') or sym.startswith('??'):
        return None
    if '@@$' in sym:
        return None
    m = re.search(r'@@([A-Z])', sym)
    return m.group(1) if m else None


def name_owned_by(sym, hierarchy):
    """True if `sym` is qualified by the class itself or one of its RTTI bases.

    A vtable slot of class C can only hold a function declared by C or by a
    base of C.  A name owned by an UNRELATED class is therefore not a
    reordering -- it is a fold survivor (or a mis-pin), and comparing it by
    name manufactures a defect.  Measured on the 2026-08-20 sweep: 51 of 141
    reported mismatches, and 40 of 86 mismatching classes are ENTIRELY this.
    Hand-checked examples, all genuine artifacts:

      MemStream[4]       ?Ranked@MatchmakingSettings@@QBA_NXZ  vs ?Fail@MemStream@@
      RndMat[0]          ??_GModalKeyListener@@                vs ??_GRndMat@@
      SaveLoadManager[6] ?Handle@MemcardMgr@@$4PPPPPPPM@A@     vs ?Handle@SaveLoadManager@@$4PPPPPPPM@A@
      OvershellSlot[17]  ?GetBufferSize@HttpGet@@QAAIXZ        vs ?DataDir@OvershellSlot@@

    The last three are the shapes that fold most readily: deleting destructors,
    adjustor thunks (`addi r3,r3,-N; b target`), and one-line accessors.

    ⚠ CONSERVATIVE BY CHOICE, AND IT COSTS COVERAGE.  A template base whose
    RTTI spelling differs from the mangled qualifier can be misjudged as
    unrelated (`BoolKeys[15]`, `?Load@FloatKeys@@` vs `?Load@BoolKeys@@`, is
    the case to watch).  Excluding a real defect costs COVERAGE; reporting a
    fake one costs a LANE and closes a vein.  We take the former -- but the
    caller MUST surface the exclusion counts, never drop them silently
    (CLAUDE.md, "no silent caps").
    """
    if not sym or not hierarchy:
        return True          # no opinion -> do not exclude
    return any(('@' + n + '@@') in sym for n in hierarchy)


def name_is_nonvirtual(sym):
    """True only when the spelling DECODES as a non-virtual member.

    Returns False for anything undecodable -- absence of evidence is not
    evidence.  See `retail_slots` for why this makes a slot uncomparable.
    """
    return access_class(sym) in _NONVIRT


# ⛔ TWO TIERS, AND CONFLATING THEM COSTS COVERAGE FOR NOTHING.
#
# HARD -- retail itself destroyed the slot's identity (ICF fold, or no name at
# all).  Nothing can be said: not "equal", not "different".
#
# SOFT -- the slot HAS a name and the name is merely SUSPECT (it decodes
# non-virtual, or it is owned by a class outside the hierarchy).  A suspect
# name can only ever manufacture a FALSE DISAGREEMENT; it cannot manufacture a
# false agreement, because our side would have to independently produce the
# very same spelling.  ⇒ AGREEMENT NEEDS NO FORGIVENESS.
#
# Treating SOFT as HARD was measured: it moved 127 classes SAME -> UNRESOLVED
# and destroyed 528 comparable slots.  Every one of those was an AGREEING slot
# -- provably, since a class whose verdict was SAME had zero mismatches by
# definition -- so the strictness bought no defect prevention whatsoever.
_HARD = ('folded_across', 'folded_within', 'unnamed', 'absent')
_SOFT = ('nonvirtual_name', 'unrelated_owner')


class Slot:
    """One vtable slot's identity, carrying WHETHER it can be compared at all.

    `comparable` is False when ICF or the map destroyed the slot's identity.
    Comparing an incomparable Slot raises rather than answering, because there
    is no honest answer.  `suspect` marks a slot that has a usable name which
    should nonetheless never be used to CHARGE a mismatch.
    """

    __slots__ = ('name', 'addr', 'reason')

    def __init__(self, name=None, addr=None, reason=None):
        self.name = name
        self.addr = addr
        self.reason = reason

    @property
    def comparable(self):
        return self.name is not None and self.reason not in _HARD

    @property
    def suspect(self):
        return self.reason in _SOFT

    def _refuse(self, other):
        who = self if not self.comparable else other
        raise FoldPoisonError(
            "refusing to compare vtable slot %s: %s. ICF/map destroyed this "
            "slot's identity, so neither 'equal' nor 'different' is an honest "
            "answer -- EXCLUDE it (see comparable_pairs()) and do not score it "
            "as agreement. tools/icf_fold_safe.py" % (
                ("at 0x%08x" % who.addr) if who.addr is not None else "<no addr>",
                who.reason or "unnamed"))

    def __eq__(self, other):
        if isinstance(other, Slot):
            if not (self.comparable and other.comparable):
                self._refuse(other)
            return self.name == other.name
        if not self.comparable:
            self._refuse(Slot(name=other))
        return self.name == other

    def __ne__(self, other):
        return not self.__eq__(other)

    # ⚠ Unhashable ON PURPOSE.  A scan that drops slots into a set/Counter to
    # do set-difference would otherwise silently treat every folded slot as a
    # distinct member and manufacture a SET_DIFFER.  TypeError here is the
    # same guard by another door.
    __hash__ = None

    def __repr__(self):
        if self.comparable:
            return "Slot(%r)" % (self.name,)
        return "Slot(<incomparable:%s@%s>)" % (
            self.reason, ("0x%08x" % self.addr) if self.addr is not None else "?")


def normalize_dtor(sym):
    """Fold `??_E` (vector deleting dtor) into `??_G` (scalar deleting dtor).

    MSVC emits both and their bodies are frequently ICF-identical, so which
    spelling the map recorded for the surviving address is arbitrary.  Treating
    them as distinct reports a reordering that does not exist.
    """
    if sym and sym.startswith('??_E'):
        return '??_G' + sym[4:]
    return sym


def fold_counts(tables):
    """slot address -> number of DISTINCT vtables it appears in.

    `tables` maps vtable VA -> list of (slot_va, target_word, in_pdata).
    An address with count > 1 is folded ACROSS vtables and is uncomparable.
    """
    occ = collections.Counter()
    for slots in tables.values():
        for w in {w for (_va, w, _p) in slots}:
            occ[w] += 1
    return occ


def retail_slots(slots, occ, addr2name, hierarchy=None):
    """Build fold-safe `Slot`s for one retail vtable.

    Handles BOTH fold shapes -- the first version of this logic caught only one:

      (a) ACROSS vtables: one address serves many classes' slots (`occ`).
      (b) WITHIN this vtable: the SAME address occupies two slots, because two
          of the class's own virtuals have identical bodies.  Measured on
          `MCContainerXbox`, where `Format()` and `Unformat()` are both
          `{ return (MCResult)0xD; }`, so retail's slots 9 and 10 hold ONE
          address and the map names it `Format`.  `occ` is 1 there, so the
          across-vtables filter passed it and the row was reported as a
          SET_DIFFER "Format vs Unformat" defect THAT DOES NOT EXIST.
    """
    within = collections.Counter(w for (_va, w, _p) in slots)
    out = []
    for (_va, w, _p) in slots:
        if occ.get(w, 0) != 1:
            out.append(Slot(addr=w, reason='folded_across'))
        elif within[w] != 1:
            out.append(Slot(addr=w, reason='folded_within'))
        else:
            nm = addr2name.get("0x%08x" % w)
            if not nm:
                out.append(Slot(addr=w, reason='unnamed'))
            elif name_is_nonvirtual(nm):
                # SOFT: keep the name, but never charge a disagreement on it.
                # ⛔ VTABLE MEMBERSHIP PROVES VIRTUALITY.  A spelling that
                # decodes NON-VIRTUAL cannot be the true occupant of a vtable
                # slot, so it proves the NAME is wrong (fold survivor or
                # mis-pin) -- it says nothing about our function.  This is the
                # criterion `map_audit` already used; `sweep_class` never
                # consulted it, so the two halves of one file disagreed about
                # the same slot.
                out.append(Slot(name=normalize_dtor(nm), addr=w,
                                reason='nonvirtual_name'))
            elif not name_owned_by(nm, hierarchy):
                out.append(Slot(name=normalize_dtor(nm), addr=w,
                                reason='unrelated_owner'))
            else:
                out.append(Slot(name=normalize_dtor(nm), addr=w))
    return out


def exclusion_counts(slots):
    """Per-reason census of what a slot list could not speak to.

    Callers MUST report this.  An instrument that quietly narrows its own
    population reads as "covered everything" when it did not.
    """
    return collections.Counter(s.reason for s in slots if s.reason)


def our_slot_names(entries):
    """Fold-safe `Slot`s for OUR compiled `??_7...@@6B@` vtable.

    ⚠ Drops the leading `??_R4` Complete Object Locator.  Our COFF vtable
    symbol includes it; the RETAIL table is read from AFTER the COL (it sits at
    vtable[-1]).  Comparing them raw shifts every slot by one -- that is
    exactly what made the first full sweep report `SAME=0 / SET_DIFFER=472`.
    A zero-agreement result is the tell; see `assert_can_agree`.
    """
    return [Slot(name=normalize_dtor(e['symbol']))
            for e in entries if not e['symbol'].startswith('??_R4')]


def comparable_pairs(retail, ours):
    """Zip two Slot lists, keeping ONLY index pairs both sides can speak to.

    Returns [(index, retail_slot, our_slot)].  Slots retail could not name are
    UNKNOWN; excluding them is the honest choice -- counting them as agreement
    would manufacture SAMEs, and counting them as disagreement is the
    fold-poisoning defect this module exists to prevent.

    Note the padding: the lists may differ in length (a count mismatch), and a
    missing slot is `absent`, which is also uncomparable.
    """
    n = max(len(retail), len(ours))
    r = list(retail) + [Slot(reason='absent')] * (n - len(retail))
    o = list(ours) + [Slot(reason='absent')] * (n - len(ours))
    return [(i, a, b) for i, (a, b) in enumerate(zip(r, o))
            if a.comparable and b.comparable]


def charge(pairs):
    """Split comparable pairs into (agreements, mismatches, withheld).

    ★ THE RULE: a SUSPECT name may confirm, but may never accuse.

    A pair that AGREES is an agreement no matter how suspect the spelling --
    our side had to produce the identical mangled name independently, which a
    fold survivor or a mis-pin cannot arrange.  A pair that DISAGREES on a
    suspect name is WITHHELD: it is exactly the shape that manufactured
    `XboxContent INTERIOR@3`, and charging it is the fold-poisoning defect.

    Withheld pairs are returned, not dropped, so a caller can adjudicate them
    deliberately on retail bytes instead of inheriting a silent verdict.
    """
    agree, mism, withheld = [], [], []
    for (i, r, o) in pairs:
        if r == o:
            agree.append((i, r, o))
        elif r.suspect or o.suspect:
            withheld.append((i, r, o))
        else:
            mism.append((i, r, o))
    return agree, mism, withheld


def assert_can_agree(n_agree, n_population, label='comparator', minimum=25):
    """Refuse a sweep that never once agreed -- the `SAME = 0` tell.

    A comparator run over a real population that produces ZERO agreements is
    reporting its own breakage, not defects.  This is cheap and it has already
    caught a real off-by-one (the `??_R4` COL shift).  `minimum` guards against
    firing on a tiny or filtered population where 0 is unremarkable.
    """
    if n_population >= minimum and n_agree == 0:
        raise VacuousInstrumentError(
            "%s produced 0 agreements over %d comparisons. An instrument that "
            "can NEVER agree is broken, not decisive -- suspect an off-by-one "
            "(the ??_R4 COL shift) or a fold-blind name comparison before "
            "believing any defect it reports. tools/icf_fold_safe.py"
            % (label, n_population))
