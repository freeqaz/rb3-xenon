#!/usr/bin/env python3
"""Regression gate for fold-poisoning in retail-vs-ours slot comparison.

Every fixture here is REAL DATA, read out of retail `default.xex` and
`scripts/target_symbol_map.json` on 2026-08-20, not invented.  The headline
case is the one a longest-common-prefix scan got wrong ONE DAY AFTER the same
defect had been found and fixed:

    XboxContent vtable @ 0x8208968c, slot 3 -> 0x8251ff70
      map name : ??$Obj@VCharPollable@@@DataNode@@QBAPAVCharPollable@@PBVDataArray@@@Z
      our name : ?Location@XboxContent@@UAA?AW4ContentLocT@@XZ

A fold-blind comparator charges that slot and reports `INTERIOR@3`, meaning
"every later slot is shifted, so every caller's vcall displacement is wrong" --
an alarming, confident, WRONG verdict for a table whose slots 0-13 had already
been read by hand as aligning.  Two independent facts say the NAME is untrue,
not our function: `DataNode` is nowhere in XboxContent's RTTI hierarchy, and
the spelling decodes to access class `Q` (public NON-virtual) while vtable
membership proves virtuality.

★ `--self-break` MUTATES the fold-awareness out and requires the suite to FAIL.
A gate that has never been shown to fail is worth nothing (CLAUDE.md).
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import icf_fold_safe as F


# --- real fixtures ---------------------------------------------------------

XBOXCONTENT_HIERARCHY = {'XboxContent', 'Content', 'Hmx::Object'}

# (slot_va, target_word, in_pdata) exactly as read_retail_slots() yields.
XBOXCONTENT_SLOTS = [
    (0x8208968c + 0, 0x82520190, True),
    (0x8208968c + 4, 0x8251fcc0, True),
    (0x8208968c + 8, 0x8251f8a0, True),
    (0x8208968c + 12, 0x8251ff70, True),   # <-- the poisoned slot
    (0x8208968c + 16, 0x8251f890, True),
    (0x8208968c + 20, 0x8251f898, True),
]

XBOXCONTENT_MAP = {
    '0x82520190': '??_GXboxContent@@UAAPAXI@Z',
    '0x8251fcc0': '?Root@XboxContent@@UAAPBDXZ',
    '0x8251f8a0': '?OnMemcard@XboxContent@@UAA_NXZ',
    '0x8251ff70': '??$Obj@VCharPollable@@@DataNode@@QBAPAVCharPollable@@PBVDataArray@@@Z',
    # 0x8251f890 is genuinely unnamed in the map
    '0x8251f898': '?HasValidLicenseBits@XboxContent@@UAA_NXZ',
}

XBOXCONTENT_OURS = [
    {'symbol': '??_R4XboxContent@@6B@'},          # COL, must be dropped
    {'symbol': '??_GXboxContent@@UAAPAXI@Z'},
    {'symbol': '?Root@XboxContent@@UAAPBDXZ'},
    {'symbol': '?OnMemcard@XboxContent@@UAA_NXZ'},
    {'symbol': '?Location@XboxContent@@UAA?AW4ContentLocT@@XZ'},
    {'symbol': '?IsCorrupt@XboxContent@@UAA_NXZ'},
    {'symbol': '?HasValidLicenseBits@XboxContent@@UAA_NXZ'},
]

# MCContainerXbox: Format() and Unformat() are both `{ return (MCResult)0xD; }`
# so retail's slots 9 and 10 hold ONE address.  `occ` is 1 (it is a slot in a
# single vtable), so the ACROSS-vtables filter passes it -- this is the second
# fold shape, and the first version of the logic caught only the first.
WITHIN_FOLD_SLOTS = [
    (0x1000, 0x82330000, True),
    (0x1004, 0x82330040, True),
    (0x1008, 0x82330040, True),   # same address twice
]
WITHIN_FOLD_MAP = {
    '0x82330000': '?Init@MCContainerXbox@@UAAXXZ',
    '0x82330040': '?Format@MCContainerXbox@@UAAXXZ',
}


FAILS = []


def check(label, cond):
    if cond:
        print("  ok   %s" % label)
    else:
        print("  FAIL %s" % label)
        FAILS.append(label)


def run():
    print("fold-poison guard")

    occ = F.fold_counts({0x8208968c: XBOXCONTENT_SLOTS})
    retail = F.retail_slots(XBOXCONTENT_SLOTS, occ, XBOXCONTENT_MAP,
                            hierarchy=XBOXCONTENT_HIERARCHY)
    ours = F.our_slot_names(XBOXCONTENT_OURS)

    # 1. the COL must be dropped, or every slot shifts by one (the SAME=0 tell)
    check("our COL (??_R4) dropped",
          len(ours) == 6 and ours[0].name == '??_GXboxContent@@UAAPAXI@Z')

    # 2. THE HEADLINE: slot 3 must never be CHARGED as a defect.
    check("XboxContent slot 3 is SUSPECT",
          retail[3].suspect)
    check("...and the recorded reason names the mechanism",
          retail[3].reason in ('nonvirtual_name', 'unrelated_owner'))

    pairs = F.comparable_pairs(retail, ours)
    agree, mism, withheld = F.charge(pairs)
    check("XboxContent is charged ZERO mismatches", not mism)
    check("...and slot 3 is WITHHELD (reported, not silently dropped)",
          [i for (i, _r, _o) in withheld] == [3])
    check("...while its agreeing slots still count", len(agree) >= 3)

    # 3. a fold-blind comparator on the SAME data reports INTERIOR@3.
    #    This is the control: it proves the fixture can distinguish, so the
    #    green result above is not vacuous.
    blind_r = [XBOXCONTENT_MAP.get("0x%08x" % w) for (_v, w, _p) in XBOXCONTENT_SLOTS]
    blind_o = [e['symbol'] for e in XBOXCONTENT_OURS
               if not e['symbol'].startswith('??_R4')]
    blind_first = next((i for i, (a, b) in enumerate(zip(blind_r, blind_o))
                        if a and b and a != b), None)
    check("CONTROL: a fold-blind comparator DOES charge slot 3",
          blind_first == 3)

    # 4. unnamed retail slot is excluded, never scored as agreement
    check("unnamed retail slot excluded", not retail[4].comparable
          and retail[4].reason == 'unnamed')

    # 5. comparing a HARD-excluded slot RAISES rather than answering
    try:
        _ = retail[4] == ours[4]
        check("Slot.__eq__ refuses a poisoned comparison", False)
    except F.FoldPoisonError:
        check("Slot.__eq__ refuses a poisoned comparison", True)
    try:
        _ = retail[4] != 'anything'
        check("Slot.__ne__ refuses a poisoned comparison", False)
    except F.FoldPoisonError:
        check("Slot.__ne__ refuses a poisoned comparison", True)

    # 6. unhashable on purpose -- a set-difference scan must not silently
    #    treat every poisoned slot as a distinct member
    try:
        {retail[4]}
        check("Slot is unhashable (blocks silent set-difference)", False)
    except TypeError:
        check("Slot is unhashable (blocks silent set-difference)", True)

    # 7. the WITHIN-vtable fold shape
    occ2 = F.fold_counts({0x1000: WITHIN_FOLD_SLOTS})
    w = F.retail_slots(WITHIN_FOLD_SLOTS, occ2, WITHIN_FOLD_MAP)
    check("within-vtable fold (Format/Unformat share an address) excluded",
          w[1].reason == 'folded_within' and w[2].reason == 'folded_within')

    # 8. across-vtable fold
    occ3 = F.fold_counts({0x1000: [(0x1000, 0x82330000, True)],
                          0x2000: [(0x2000, 0x82330000, True)]})
    a = F.retail_slots([(0x1000, 0x82330000, True)], occ3, WITHIN_FOLD_MAP)
    check("across-vtable fold excluded", a[0].reason == 'folded_across')

    # 9. dtor spellings normalise (??_E vector vs ??_G scalar deleting dtor)
    check("??_E normalises to ??_G",
          F.normalize_dtor('??_EFoo@@UAAPAXI@Z') == '??_GFoo@@UAAPAXI@Z')

    # 10. adjustor thunks must NOT be read as non-virtual (1,379 FPs if they are)
    check("adjustor thunk is not decoded as non-virtual",
          not F.name_is_nonvirtual(
              '?Handle@SaveLoadManager@@$4PPPPPPPM@A@AA?AVDataNode@@PAVDataArray@@_N@Z'))
    check("plain public virtual decodes virtual",
          not F.name_is_nonvirtual('?Root@XboxContent@@UAAPBDXZ'))
    check("plain public non-virtual decodes non-virtual",
          F.name_is_nonvirtual('?Ranked@MatchmakingSettings@@QBA_NXZ'))

    # 11. ownership: base-owned names are legitimate, strangers are not
    check("base-owned name accepted",
          F.name_owned_by('?Print@Hmx::Object@@UAAXXZ',
                          {'XboxContent', 'Hmx::Object'}))
    check("unrelated-owner name rejected",
          not F.name_owned_by('?GetBufferSize@HttpGet@@QAAIXZ',
                              {'OvershellSlot', 'UIComponent'}))
    check("no hierarchy -> no opinion (does not exclude)",
          F.name_owned_by('?Whatever@Foo@@UAAXXZ', None))

    # 12. the vacuity guard fires on SAME=0 and stays quiet otherwise
    try:
        F.assert_can_agree(0, 400, 'fixture')
        check("assert_can_agree fires at SAME=0", False)
    except F.VacuousInstrumentError:
        check("assert_can_agree fires at SAME=0", True)
    try:
        F.assert_can_agree(0, 3, 'fixture')
        check("assert_can_agree quiet on a tiny population", True)
    except F.VacuousInstrumentError:
        check("assert_can_agree quiet on a tiny population", False)

    # 13. exclusions are countable, so a caller can report coverage loss
    ec = F.exclusion_counts(retail)
    check("exclusion_counts reports the poisoned slots", sum(ec.values()) >= 2)

    # 14. ★ A SUSPECT NAME MAY CONFIRM BUT MAY NEVER ACCUSE.
    #     Treating "suspect" as "uncomparable" moved 127 classes SAME ->
    #     UNRESOLVED and destroyed 528 comparable slots, EVERY ONE of which was
    #     an agreeing slot -- so the strictness prevented no false defect at
    #     all.  Agreement needs no forgiveness: our side would have to
    #     independently produce the identical mangled spelling, which a fold
    #     survivor or a mis-pin cannot arrange.
    sus_agree = [(0, F.Slot(name='?X@A@@QBA_NXZ', reason='nonvirtual_name'),
                  F.Slot(name='?X@A@@QBA_NXZ'))]
    a, m, w = F.charge(sus_agree)
    check("suspect name that AGREES still counts as agreement",
          len(a) == 1 and not m and not w)

    sus_dis = [(0, F.Slot(name='?Ranked@MatchmakingSettings@@QBA_NXZ',
                          reason='unrelated_owner'),
                F.Slot(name='?Fail@MemStream@@UAA_NXZ'))]
    a, m, w = F.charge(sus_dis)
    check("suspect name that DISAGREES is withheld, not charged",
          not m and len(w) == 1)

    clean_dis = [(0, F.Slot(name='?A@C@@UAAXXZ'), F.Slot(name='?B@C@@UAAXXZ'))]
    a, m, w = F.charge(clean_dis)
    check("CONTROL: a clean disagreement IS still charged",
          len(m) == 1 and not w)

    # --- interchangeable tail-call thunks -------------------------------------
    # Real bytes, lifted from retail band.exe.  UIFontImporter's adjustor thunks
    # are `lwz r11,-4(r3); add r3,r11,r3; b <target>` -- identical except the
    # displacement, so the map's assignment between them is arbitrary.
    UIFI_A, UIFI_B = 0x82819D70, 0x8281C540      # Copy / SetType thunks
    SR360_A, SR360_B = 0x82B6BAE8, 0x82B6BAF8    # GetPlayCursor / PlayImpl thunks
    THUNKS = {
        UIFI_A:   [0x8163FFFC, 0x7C6B1850, 0x4BFFFE98],
        UIFI_B:   [0x8163FFFC, 0x7C6B1850, 0x4BFFED58],
        SR360_A:  [0x3D600001, 0x616B803C, 0x7C63582E, 0x4BFFB154],
        SR360_B:  [0x3D600001, 0x616B803C, 0x7C63582E, 0x4BFF965C],
        # a NON-thunk control: a real prologue (mflr / stw / stwu), no tail b
        0x82B6BA78: [0x7D8802A6, 0x9181FFF8, 0xFBC1FFE8, 0xFBE1FFF0,
                     0x9421FF90, 0x3D600001],
    }

    def rw(va):
        for base, ws in THUNKS.items():
            if base <= va < base + 4 * len(ws):
                return ws[(va - base) // 4]
        return None

    check("tail_thunk_shape identifies a 3-insn adjustor thunk",
          F.tail_thunk_shape(rw, UIFI_A) == (0x8163FFFC, 0x7C6B1850))
    check("two thunks differing ONLY in displacement share a shape",
          F.tail_thunk_shape(rw, UIFI_A) == F.tail_thunk_shape(rw, UIFI_B))
    check("StreamReceiver360's twins share a shape too",
          F.tail_thunk_shape(rw, SR360_A) == F.tail_thunk_shape(rw, SR360_B)
          is not None)
    check("CONTROL: a real prologue is NOT a tail-call thunk",
          F.tail_thunk_shape(rw, 0x82B6BA78) is None)
    check("CONTROL: shapes EXCLUDE the branch, so they must not be equal "
          "merely because both end in `b`",
          F.tail_thunk_shape(rw, UIFI_A) != F.tail_thunk_shape(rw, SR360_A))

    twins = F.mark_thunk_twins(
        [F.Slot(name='?Copy@UIFontImporter@@UAAXXZ', addr=UIFI_A),
         F.Slot(name='?SetType@UIFontImporter@@UAAXXZ', addr=UIFI_B),
         F.Slot(name='?PauseImpl@X@@UAAX_N@Z', addr=0x82B6BA78)], rw)
    check("both twins are SOFT-marked thunk_twin",
          twins[0].reason == 'thunk_twin' and twins[1].reason == 'thunk_twin')
    check("the non-thunk slot is left fully comparable",
          twins[2].reason is None and twins[2].comparable)
    check("a thunk_twin is SUSPECT, not INCOMPARABLE "
          "(it must still be able to confirm)",
          twins[0].suspect and twins[0].comparable)

    # The whole point: a SWAPPED map between twins must be WITHHELD, not charged.
    swapped = [(0, twins[0], F.Slot(name='?SetType@UIFontImporter@@UAAXXZ')),
               (1, twins[1], F.Slot(name='?Copy@UIFontImporter@@UAAXXZ'))]
    a, m, w = F.charge(swapped)
    check("a swapped thunk pair is WITHHELD, never charged as a defect",
          not m and len(w) == 2)

    agree = [(0, twins[0], F.Slot(name='?Copy@UIFontImporter@@UAAXXZ'))]
    a, m, w = F.charge(agree)
    check("CONTROL: twins that AGREE still count as agreement",
          len(a) == 1 and not m and not w)

    return not FAILS


def self_break():
    """Remove fold-awareness and REQUIRE the suite to fail."""
    print("--self-break: mutating fold-awareness out of retail_slots()\n")

    def blind(slots, occ, addr2name, hierarchy=None):
        return [F.Slot(name=F.normalize_dtor(addr2name.get("0x%08x" % w)),
                       addr=w)
                for (_va, w, _p) in slots]

    F.retail_slots = blind
    ok = run()
    print()
    if ok:
        print("SELF-BREAK FAILED: the suite PASSED with fold-awareness removed.")
        print("The gate is VACUOUS -- it cannot detect the defect it exists for.")
        return False
    print("SELF-BREAK OK: suite failed as required (%d checks)." % len(FAILS))
    print("  first failure: %s" % FAILS[0])
    return True


if __name__ == '__main__':
    if '--self-break' in sys.argv:
        sys.exit(0 if self_break() else 1)
    good = run()
    print()
    print("PASS" if good else "FAIL (%d)" % len(FAILS))
    sys.exit(0 if good else 1)
