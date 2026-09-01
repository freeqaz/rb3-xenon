#!/usr/bin/env python3
"""Audit ObjDirItr<T> map names by TEMPLATE-FORCED CALL IDENTITY.

THE INVARIANT
-------------
    template<class T> ObjDirItr<T>& ObjDirItr<T>::operator++() { Advance(); ... }

so `ObjDirItr<T>::operator++` calls `ObjDirItr<T>::Advance` FOR THE SAME T. The
compiler cannot emit a call from one instantiation into a sibling's body. This
is the one place tools/at100_sibling_split.py's structural argument genuinely
applies, because here the CALLER IS the sibling instantiation.

So for every operator++ the map names, read the retail bytes, follow its single
`bl`, and ask what the map calls the destination. If the two T's disagree, one
of the two names is WRONG -- provably, without consulting any oracle.

BREAKING THE TIE (which of the pair is wrong)
---------------------------------------------
The `bl` alone is a 2-colouring with two solutions. ObjDirItr<T>::Advance loads
a per-T ??_R0 RTTI type descriptor out of .data whose decorated name is the
filter type in plain text ('.?AVRndAnimatable@@'). That names Advance
independently of the map, so the tie always breaks toward Advance and the
operator++ name is the one adjudicated.

ANTI-VACUITY
------------
A run that resolves no operator++ at all, or reads no RTTI name at all, REFUSES
rather than reporting a clean "0 defects" -- this audit's whole value is a
negative result, and a negative that cannot fail is worthless. Verified against
the known answer: 0x82404240 / 0x82404310 were swapped (lane S2-CONTAINER).
"""
import json, os, re, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from retail_body import Img, MAP


def load_map():
    raw = json.load(open(MAP))
    a2n, n2a = {}, {}
    for k, v in raw.items():
        if not k.startswith("0x") or not isinstance(v, str):
            continue
        a2n[int(k, 16)] = v
        n2a.setdefault(v, set()).add(int(k, 16))
    return a2n, n2a


def targ_of(fam, name):
    """?$ObjDirItr@<ARG>@@ -> ARG"""
    m = re.search(r"\?\$" + fam + r"@(.+?)@@", name)
    return m.group(1) if m else None


def bl_targets(img, va, maxwords=64):
    """Every bl destination in the body starting at va (stop at blr)."""
    out = []
    for i in range(maxwords):
        w = img.read(va + 4 * i, 4)
        if len(w) < 4:
            break
        ins = struct.unpack(">I", w)[0]
        if ins == 0x4E800020:                      # blr
            break
        if (ins >> 26) == 18 and (ins & 1):        # bl
            d = ins & 0x03FFFFFC
            if d & 0x02000000:
                d -= 0x04000000
            out.append(va + 4 * i + d)
    return out


def rtti_of_advance(img, va, a2n, maxwords=80):
    """The per-T ??_R0 decorated name an Advance body loads.

    *** IT LOADS TWO. *** Every instantiation loads `.?AVObject@Hmx@@` (the dir's
    common base) AND the per-T filter descriptor:

        addi r30, r11, 18900   -> 0x82c649d4  .?AVObject@Hmx@@     <- common
        addi r29, r10, -19388  -> 0x82c6b444  .?AVRndAnimatable@@  <- the answer

    Returning the FIRST match makes this function answer `.?AVObject@Hmx@@` for
    essentially every T, which reads as a flood of false inconsistencies. Collect
    all of them and prefer the non-Object one; if Object is the only descriptor,
    T genuinely IS Hmx::Object.
    """
    found, hi = [], {}
    for i in range(maxwords):
        w = img.read(va + 4 * i, 4)
        if len(w) < 4:
            break
        ins = struct.unpack(">I", w)[0]
        if ins == 0x4E800020:
            break
        op, rt, ra = ins >> 26, (ins >> 21) & 31, (ins >> 16) & 31
        if op == 15 and ra == 0:                                   # lis
            hi[rt] = (ins & 0xFFFF) << 16
        elif op == 14 and ra in hi:                                # addi
            lo = ins & 0xFFFF
            if lo & 0x8000:
                lo -= 0x10000
            cand = hi[ra] + lo
            blob = img.read(cand, 96)
            if len(blob) >= 12:
                nm = blob[8:].split(b"\0")[0]
                if nm.startswith(b".?AV"):
                    found.append((cand, nm.decode("ascii", "replace")))
    if not found:
        return None, None
    for cand, nm in found:
        if nm != ".?AVObject@Hmx@@":
            return cand, nm
    return found[0]


def main():
    fam = sys.argv[1] if len(sys.argv) > 1 else "ObjDirItr"
    img = Img()
    a2n, n2a = load_map()

    incs = {a: n for a, n in a2n.items() if n.startswith("??E?$" + fam + "@")}
    advs = {a: n for a, n in a2n.items()
            if n.startswith("?Advance@?$" + fam + "@")}
    print(f"[audit] {fam}: {len(incs)} operator++ / {len(advs)} Advance in map")

    # independent identity for every Advance, from its RTTI descriptor
    adv_rtti = {}
    for a in advs:
        _, nm = rtti_of_advance(img, a, a2n)
        if nm:
            adv_rtti[a] = nm
    print(f"[audit] resolved RTTI type for {len(adv_rtti)}/{len(advs)} Advance bodies")

    # Build sibling spellings by SUBSTITUTING the template argument inside a name
    # we have actually OBSERVED, never by re-assembling the mangling by hand: the
    # class-template arg closes with its own "@@" before the template's "@@", so
    # a hand-built "...@VCharBone@@QAAAAV0@XZ" misses the real
    # "...@VCharBone@@@@QAAAAV0@XZ" and EVERY lookup silently returns nothing --
    # which here would have made every row look FOLD_SUSPECT via a bug.
    def subst(observed, new_t):
        old_t = targ_of(fam, observed)
        return observed.replace("?$" + fam + "@" + old_t + "@@",
                                "?$" + fam + "@" + new_t + "@@", 1)

    inc_proto = next(iter(incs.values()))
    adv_proto = next(iter(advs.values()))

    def inc_name(t):
        return subst(inc_proto, t)

    def adv_name(t):
        return subst(adv_proto, t)

    # self-check: the prototypes must round-trip, or the substitution is wrong
    for proto, f in ((inc_proto, inc_name), (adv_proto, adv_name)):
        if f(targ_of(fam, proto)) != proto:
            sys.exit(f"REFUSING: name substitution does not round-trip on {proto}")

    resolved, bad, foldish = 0, [], []
    for a, n in sorted(incs.items()):
        tg = [d for d in bl_targets(img, a) if d in advs]
        if len(tg) != 1:
            continue
        resolved += 1
        dst = tg[0]
        want, got = targ_of(fam, n), targ_of(fam, advs[dst])
        rt = adv_rtti.get(dst)
        if want == got:
            continue
        # *** AN INCONSISTENCY IS NOT YET A DEFECT. ***
        # ICF folds Advance<A> into Advance<B> when their bodies agree, and then
        # operator++<A> and operator++<B> fold too -- leaving ONE surviving
        # address for each, each named after an ARBITRARY member of its fold
        # class, and the two survivors need not have picked the same member.
        # That produces exactly this bl inconsistency with NO wrong name.
        # The fold ALWAYS destroys names: the collapsed spellings vanish from an
        # address-keyed map. So demand the COMPLETE 2x2 -- both operator++ and
        # both Advance present, at four distinct addresses. A fold cannot
        # produce that, because it would have collapsed the very rows we can see.
        cycle = (inc_name(want) in n2a and inc_name(got) in n2a
                 and adv_name(want) in n2a and adv_name(got) in n2a)
        addrs = set()
        for nm_ in (inc_name(want), inc_name(got), adv_name(want), adv_name(got)):
            addrs |= n2a.get(nm_, set())
        if cycle and len(addrs) == 4:
            bad.append((a, n, dst, advs[dst], rt))
        else:
            foldish.append((a, n, dst, advs[dst], rt, len(addrs)))

    if not resolved or not adv_rtti:
        sys.exit(f"REFUSING: resolved={resolved} advance_rtti={len(adv_rtti)} -- "
                 "the audit could not fire, so a '0 defects' result would be "
                 "VACUOUS. Check the family name and the PE reader.")
    print(f"[audit] {resolved} operator++ bodies resolved to exactly one Advance")
    print(f"[audit] ADJUDICABLE defects (complete 2x2, fold excluded): {len(bad)}")
    print(f"[audit] FOLD_SUSPECT  (a name is missing => cannot separate): "
          f"{len(foldish)}\n")
    for a, n, dst, dn, rt in bad:
        print(f"  DEFECT 0x{a:08x} {n}")
        print(f"      calls 0x{dst:08x} {dn}")
        print(f"      that Advance's RTTI says: {rt}")
        print(f"      => 0x{a:08x} is really operator++ for {rt}")
    for a, n, dst, dn, rt, na in foldish:
        print(f"  FOLD_SUSPECT 0x{a:08x} {n}")
        print(f"      calls 0x{dst:08x} {dn}  (RTTI {rt}); only {na}/4 of the "
              f"cycle's names survive in the map -- DO NOT RENAME")
    return 0 if not bad else 1


if __name__ == "__main__":
    sys.exit(main())
