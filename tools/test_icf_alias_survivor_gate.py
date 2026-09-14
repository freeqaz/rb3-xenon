#!/usr/bin/env python3
"""Unit test for tools/icf_alias_build.py's SURVIVOR SELF-CHECK (lane W16-AE).

WHAT IT GUARDS.  Tier-1 alias generation verified the FOLDED spelling's
compiled COMDAT against the retail body and then trusted the map for the
SURVIVOR's name -- ours.get(survivor) was never consulted.  Two landed "T1"
groups the pair adjudicator refutes came out of exactly that blind spot:

    0x82b9b1f8  retail 64 B WITH a bl to _M_erase   vs our survivor 92 B, 0 relocs
    0x82336af8  retail 40 B / 5 relocs               vs our survivor swap<...> 28 B / 0

The fixtures below are SYNTHETIC records in the builder's own (masked_body,
relocs, size) shape reproducing those two contradictions, plus controls:

    identical survivor   -> must be ACCEPTED (None) -- the gate is not "refuse all"
    same size/relocs, words differ, mode=shape -> ACCEPT (an imperfect port of the
                            right function is not a wrong identity); mode=strict
                            -> REFUSE
    mode=off             -> every contradiction ACCEPTED -- proves the verdict is
                            the gate's, and that `off` really is the old behaviour
    survivor not compiled (None) -> None (cannot be checked; counted, not refused)
    name-variant target inside ONE alias class (eq=) -> ACCEPT; the same shape
                            with a target in NO class -> REFUSE, naming the pair
    retail `lbl_` rdata placeholder vs our `__real@` literal -> ACCEPT

Exit 0 on pass, 1 on any failed expectation.  Registered in scripts/test_tools.py.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import icf_alias_build as B  # noqa: E402


def rec(size, relocs, seed):
    # non-vacuous body: distinct non-zero words so the vacuity guard cannot trip
    body = bytes(((seed * 7 + i * 13) % 251) + 1 for i in range(size))
    return (body, list(relocs), size)


def main():
    fails = 0

    def expect(label, got, want_none):
        nonlocal fails
        ok = (got is None) == want_none
        print("  %s  %-58s -> %s" % ("ok " if ok else "FAIL", label,
                                      "ACCEPT" if got is None else "REFUSE: " + got))
        if not ok:
            fails += 1

    # sanity: fixtures are non-vacuous, else every verdict below is vacuous
    for r in (rec(64, [(0x30, "?_M_erase@x@@AAAXXZ", "REL24")], 1), rec(92, [], 2),
              rec(40, [(0x4, "a", "ADDR16_HA"), (0x8, "a", "ADDR16_LO"), (0x10, "b", "REL24"),
                       (0x18, "c", "REL24"), (0x20, "d", "REL24")], 3), rec(28, [], 4)):
        if B.vacuous(r):
            print("FAIL fixture is vacuous: size=%d relocs=%d" % (r[2], len(r[1])))
            fails += 1

    print("case 0x82b9b1f8 shape: retail 64 B + bl _M_erase vs our survivor 92 B reloc-free")
    rt = rec(64, [(0x30, "?_M_erase@x@@AAAXXZ", "REL24")], 1)
    st = rec(92, [], 2)
    expect("mode=shape", B.survivor_self_check(rt, st, mode="shape"), want_none=False)
    expect("mode=strict", B.survivor_self_check(rt, st, mode="strict"), want_none=False)
    expect("mode=off (old behaviour)", B.survivor_self_check(rt, st, mode="off"), want_none=True)

    print("case 0x82336af8 shape: retail 40 B / 5 relocs vs our survivor 28 B / 0 relocs")
    rt = rec(40, [(0x4, "a", "ADDR16_HA"), (0x8, "a", "ADDR16_LO"), (0x10, "b", "REL24"),
                  (0x18, "c", "REL24"), (0x20, "d", "REL24")], 3)
    st = rec(28, [], 4)
    expect("mode=shape", B.survivor_self_check(rt, st, mode="shape"), want_none=False)
    expect("mode=off (old behaviour)", B.survivor_self_check(rt, st, mode="off"), want_none=True)

    print("same size, reloc COUNT differs (40 B: 5 vs 4)")
    st = rec(40, rt[1][:4], 5)
    expect("mode=shape", B.survivor_self_check(rt, st, mode="shape"), want_none=False)

    print("same size and count, reloc SHAPE differs (offset 0x10 -> 0x14)")
    st = rec(40, [rt[1][0], rt[1][1], (0x14, "b", "REL24"), rt[1][3], rt[1][4]], 6)
    expect("mode=shape", B.survivor_self_check(rt, st, mode="shape"), want_none=False)

    print("same size/count/shape, reloc TARGET differs (b -> zz), both mapped names")
    st = rec(40, [rt[1][0], rt[1][1], (0x10, "zz", "REL24"), rt[1][3], rt[1][4]], 7)
    expect("mode=shape", B.survivor_self_check(rt, st, mapped=frozenset({"b", "zz"}), mode="shape"),
           want_none=False)

    print("CONTROL: survivor byte- and reloc-identical to retail")
    st = (rt[0], list(rt[1]), rt[2])
    expect("mode=shape", B.survivor_self_check(rt, st, mode="shape"), want_none=True)
    expect("mode=strict", B.survivor_self_check(rt, st, mode="strict"), want_none=True)

    print("CONTROL: same size/relocs, masked body WORDS differ (imperfect port of the right fn)")
    st = rec(40, list(rt[1]), 8)
    expect("mode=shape (accept)", B.survivor_self_check(rt, st, mode="shape"), want_none=True)
    expect("mode=strict (refuse)", B.survivor_self_check(rt, st, mode="strict"), want_none=False)

    print("CONTROL: survivor spelling not compiled by us (None) / retail missing")
    expect("st=None", B.survivor_self_check(rt, None, mode="strict"), want_none=True)
    expect("rt=None", B.survivor_self_check(None, st, mode="strict"), want_none=True)

    # ★ W16-AE calibration: the first cut compared target NAMES literally and
    # over-refused 279 landed groups whose callees were name-VARIANT members of
    # one fold class (PreloadPanel: retail ??3BinStream vs ours ??3@YAXPAX@Z,
    # both in the 0x8240ddb0 operator-delete group; report.json scores it 100).
    # The gate must accept under the SAME equivalence objdiff applies, and must
    # STILL refuse a target in no class (AppLabel: retail ?MemFree vs ours
    # ??3BandLabel; report.json charges it, 99.74).
    print("EQUIVALENCE: reloc target differs by NAME only, both in one alias class")
    eq = {"??3BinStream@@SAXPAX@Z": "??3BinStream@@SAXPAX@Z",
          "??3@YAXPAX@Z": "??3BinStream@@SAXPAX@Z",
          "??3BandLabel@@SAXPAX@Z": "??3BinStream@@SAXPAX@Z"}
    rt = rec(84, [(0x1c, "??_DPreloadPanel@@QAAXXZ", "REL24"), (0x2c, "??3BinStream@@SAXPAX@Z", "REL24")], 9)
    st = rec(84, [(0x1c, "??_DPreloadPanel@@QAAXXZ", "REL24"), (0x2c, "??3@YAXPAX@Z", "REL24")], 9)
    mp = frozenset({"??3BinStream@@SAXPAX@Z", "??3@YAXPAX@Z", "?MemFree@@YAXPAX@Z", "??3BandLabel@@SAXPAX@Z"})
    expect("with eq (accept)", B.survivor_self_check(rt, st, mapped=mp, mode="shape", eq=eq), want_none=True)
    expect("without eq (literal names refuse -- the over-refusal)",
           B.survivor_self_check(rt, st, mapped=mp, mode="shape", eq=None), want_none=False)
    print("EQUIVALENCE CONTROL: target in NO class must still be refused (AppLabel MemFree case)")
    rt = rec(76, [(0x1c, "??_DAppLabel@@QAAXXZ", "REL24"), (0x2c, "?MemFree@@YAXPAX@Z", "REL24")], 10)
    st = rec(76, [(0x1c, "??_DAppLabel@@QAAXXZ", "REL24"), (0x2c, "??3BandLabel@@SAXPAX@Z", "REL24")], 10)
    got = B.survivor_self_check(rt, st, mapped=mp, mode="shape", eq=eq)
    expect("with eq (refuse)", got, want_none=False)
    if got is not None and "?MemFree@@YAXPAX@Z" not in got:
        print("  FAIL  refusal must NAME the differing pair; got: %s" % got); fails += 1
    print("EQUIVALENCE: retail rdata placeholder lbl_ vs our __real@ literal (AtFrame<Symbol>)")
    rt = rec(260, [(0x8, "lbl_82000D78", "ADDR16_HA"), (0xc, "lbl_82000D78", "ADDR16_LO")], 11)
    st = rec(260, [(0x8, "__real@00000000", "ADDR16_HA"), (0xc, "__real@00000000", "ADDR16_LO")], 11)
    expect("placeholder vs literal (accept)", B.survivor_self_check(rt, st, mapped=mp, mode="shape", eq=eq), want_none=True)
    print("load_equivalences / canon_relocs round-trip on a synthetic aliases file")
    import json, tempfile
    with tempfile.NamedTemporaryFile("w", suffix=".json", dir=os.path.expanduser("~/tmp"), delete=False) as fh:
        json.dump({"groups": [{"survivor": "S", "folded": ["F1", "F2"]}, {"survivor": "S2", "folded": ["F1"]}]}, fh)
        tmp = fh.name
    try:
        e = B.load_equivalences(tmp)
        ok = e == {"S": "S", "F1": "S", "F2": "S", "S2": "S2"}
        print("  %s  load_equivalences first-class-wins -> %s" % ("ok " if ok else "FAIL", e)); fails += 0 if ok else 1
        c = B.canon_relocs(("b", [(0, "F2", "REL24"), (4, "zz", "REL24")], 8), e)
        ok = c[1] == [(0, "S", "REL24"), (4, "zz", "REL24")]
        print("  %s  canon_relocs rewrites members, leaves strangers -> %s" % ("ok " if ok else "FAIL", c[1])); fails += 0 if ok else 1
        ok = B.load_equivalences("none") == {} and B.load_equivalences("") == {}
        print("  %s  load_equivalences('none'/'') -> {}" % ("ok " if ok else "FAIL")); fails += 0 if ok else 1
    finally:
        os.unlink(tmp)

    print("\n%s (%d failure(s))" % ("PASS" if fails == 0 else "FAIL", fails))
    return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
