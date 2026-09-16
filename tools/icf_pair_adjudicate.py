#!/usr/bin/env python3
"""Adjudicate ONE (retail survivor, our spelling) alias candidate on retail bytes.

``tools/icf_alias_build.py`` is the batch generator; it only ever adjudicates
pairs some *enumerator* proposed, and its summary cannot distinguish "this pair
was REFUTED" from "this pair was never proposed". When a single charged site is
worth thousands of bytes, that distinction is the whole question, so this tool
takes an explicit pair and prints the T1 decision with every input shown.

It reuses ``icf_alias_build``'s primitives verbatim (``collect``, ``relocs_agree``,
``vacuous``) so a verdict here is the same verdict the generator would reach --
this is a magnifying glass on that adjudicator, NOT a second one.

T1 asks: are the RETAIL bytes at the survivor's address byte-identical, modulo
relocated fields, to what OUR compiler emits for the folded spelling -- AND do
the two agree on relocation TARGETS, not merely on shape? (Masked bytes alone
are vacuous for template twins: ``vector<Foo>::erase`` and ``vector<Bar>::erase``
have identical machine bytes and differ ONLY in the destructor they call.)

It additionally reports UNIQUENESS on both sides, because a body shared by many
functions proves nothing about which one a call site meant -- picking one of an
ICF-folded group is a coin flip.

    python3 tools/icf_pair_adjudicate.py --survivor '?Foo@@...' --ours '?Bar@@...'
    python3 tools/icf_pair_adjudicate.py --selftest      # controls; run this first
"""

import argparse
import collections
import glob
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from icf_alias_build import collect, relocs_agree, vacuous, placeholder  # noqa: E402


def load_sides():
    tgt = collect(sorted(glob.glob(str(ROOT / "build/45410914/obj/**/*.obj"), recursive=True)),
                  "retail target objs")
    ours = collect(sorted(glob.glob(str(ROOT / "build/45410914/src/**/*.obj"), recursive=True)),
                   "our objs")
    return tgt, ours


def body_index(side):
    idx = collections.defaultdict(list)
    for name, (mb, _r, _s) in side.items():
        idx[mb].append(name)
    return idx


def adjudicate(tgt, ours, survivor, our_name, mapped, verbose=True):
    """Return (verdict, detail dict). Verdict in PROVEN / REFUTED / UNDECIDABLE."""
    d = {"survivor": survivor, "ours": our_name}
    rt, ob = tgt.get(survivor), ours.get(our_name)
    if rt is None:
        return "UNDECIDABLE", dict(d, why="survivor absent from the dtk target objs "
                                          "(address outside every pinned .text span)")
    if ob is None:
        return "UNDECIDABLE", dict(d, why="our spelling is in no compiled obj")
    d["retail_size"], d["our_size"] = rt[2], ob[2]
    if vacuous(rt) or vacuous(ob):
        return "UNDECIDABLE", dict(d, why="VACUOUS: body under 4 words or over half "
                                          "the words masked -- compares equal to too much")
    if rt[0] != ob[0]:
        return "REFUTED", dict(d, why="masked bodies DIFFER (retail did not keep the "
                                      "code our spelling compiles to)")
    tally = collections.Counter()
    if not relocs_agree(rt, ob, mapped, strict=True, tally=tally):
        return "REFUTED", dict(d, why="masked bodies match but relocation TARGETS "
                                      "disagree -- template-twin, not a fold",
                               reloc_tally=dict(tally))
    d["reloc_tally"] = dict(tally)
    d["n_relocs"] = len(rt[1])
    return "PROVEN", d


# ★ W16-GG.  Set ONLY by --self-break.  When true, chase()'s vacuous branch stops
# accounting for the relocation DESTINATION and admits any vacuous pair whose
# masked bytes and relocation SHAPE agree -- i.e. exactly the permissive failure
# the relaxation must not have.  It exists so the VACUOUS DECOY control can be
# SHOWN to go red on demand: a control nobody has watched fail is an assumption,
# not a control.  (House pattern: grep_binary_guard.py --self-break,
# verify_ruler_agreement.py --selftest, scripts/sabotage_obj_pairing.py.)
_SELF_BREAK = False


def _slots_agree(tgt, ours, rt, ob, survivor, our_name, mapped, depth, stack,
                 memo, out, maxdepth, tolerate_placeholders):
    """Pairwise relocation-slot comparison, recursing on differing real names.

    ★ ONE comparator, TWO callers, and the ONLY difference between them is
    ``tolerate_placeholders``.  That is deliberate: the two call sites used to be
    two hand-written tests (a literal ``list(rt[1]) == list(ob[1])`` in the
    vacuous branch and this loop in the general one), and a reviewer had no way
    to see how they differed except by reading both.  Now the difference is a
    named boolean with a stated reason at each call site.

    ``tolerate_placeholders=True``  -- the general (non-vacuous) path.  Required:
    recursing into placeholder slots instead of tolerating them REGRESSED the
    positive control (see the note at the call site).
    ``tolerate_placeholders=False`` -- the vacuous path.  In a body whose bytes
    are (almost) all masked, a tolerated slot means NOTHING was compared, so a
    placeholder on either side is a refusal.
    """
    rr, orr = rt[1], ob[1]
    if len(rr) != len(orr):
        out.append((depth, "RELOC-COUNT", survivor, our_name))
        return False

    stack.append((survivor, our_name))
    ok = True
    for (ro, rn, rty), (oo, on, oty) in zip(rr, orr):
        if ro != oo or rty != oty:
            out.append((depth, "RELOC-SHAPE", survivor, our_name))
            ok = False
            break
        if rn == on:
            continue
        if rn.startswith(("fn_", "lbl_")) and on in mapped:
            # CD-9: retail spells a callee fn_<B> only when B is absent from the
            # map; our callee being map-resident at A != B means retail's slot
            # demonstrably calls a DIFFERENT function. Not a tolerance.
            out.append((depth, "MAPPED-VS-PLACEHOLDER", rn, on))
            ok = False
            break
        if placeholder(rn) or placeholder(on):
            # ★ CHASE MUST BE A STRICT SUPERSET OF FLAT T1.  Measured: recursing
            # into placeholder slots instead of tolerating them REGRESSED the
            # positive control -- a landed, flat-T1-PROVEN group went REFUTED,
            # because retail's slot reads `fn_8275B378` whose target-obj body is
            # not our callee's.  That is a stricter *different* test, not the
            # relaxation this mode exists to add, and shipping it would have
            # silently re-litigated every landed group under a rule nobody
            # gated.  Recursion is applied ONLY to the branch flat T1 refuses:
            # both sides carry real, differing names.
            if tolerate_placeholders:
                continue
            # ★ W16-GG: the vacuous caller cannot afford this tolerance -- the
            # destination IS the body there.  Refusing keeps the relaxed vacuous
            # branch STRICTER than the general path on this axis.
            out.append((depth, "VACUOUS-PLACEHOLDER-SLOT", rn[:70], on[:70]))
            ok = False
            break
        if _SELF_BREAK and not tolerate_placeholders:
            # --self-break ONLY: drop the destination proof, keep the shape
            # check.  This is "the relaxation with its recursion removed".
            out.append((depth, "SELF-BREAK-TOLERATED", rn[:70], on[:70]))
            continue
        if not chase(tgt, ours, rn, on, mapped, depth + 1, stack, memo, out,
                     maxdepth):
            out.append((depth, "SLOT-REFUTED", rn[:70], on[:70]))
            ok = False
            break
        out.append((depth + 1, "SLOT-FOLD-OK", rn[:70], on[:70]))
    stack.pop()
    return ok


def chase(tgt, ours, survivor, our_name, mapped, depth=0, stack=None, memo=None,
          out=None, maxdepth=12):
    """RECURSIVE T1: verify a fold through relocation-target EQUIVALENCE.

    WHY the flat T1 tier cannot decide this class.  ``relocs_agree`` compares
    relocation target NAMES literally, so it refutes any fold whose callees are
    THEMSELVES folded -- and MSVC's /OPT:ICF is iterative, so that is the normal
    case for template families.  Measured on the CustomizePanel row: retail's
    ``hash_map<int,T*>::operator[]`` survivor calls the ``_M_find`` survivor
    named for SongMetadata and the ``_M_insert`` survivor named for SongStatus,
    i.e. THREE different T in one function.  A literal-name comparator calls that
    a template-twin refutation; it is in fact the fold signature.

    ★ THE SAFETY PROPERTY: this NEVER SEARCHES.  It is handed a specific pair and
    walks the pair chain RETAIL'S OWN RELOCATIONS dictate -- slot i of retail is
    checked against slot i of ours, and nothing else is ever considered.  There
    is no "find the best candidate" step, so there is no coin flip to lose.  The
    flat T1 base test (masked bytes equal, reloc offsets/types equal, non-vacuous)
    must hold at EVERY level; recursion only relaxes the NAME equality.

    Cycles are accepted coinductively (a pair already on the stack is assumed) --
    that is what a linker does with mutually recursive COMDATs -- and every such
    assumption is reported so it can be audited rather than trusted silently.
    """
    stack = stack if stack is not None else []
    memo = memo if memo is not None else {}
    out = out if out is not None else []
    key = (survivor, our_name)
    if key in memo:
        return memo[key]
    if key in stack:
        out.append((depth, "CYCLE-ASSUMED", survivor, our_name))
        return True
    if survivor == our_name and depth > 0:
        # A same-name SLOT (retail's reloc names S and so does ours) is exactly
        # what flat T1's relocs_agree accepts: name equality IS the evidence.
        return True
    # ★ W16-AE: NOT at depth 0.  A self-pair [S, S] handed in at the top used to
    # short-circuit here and read PROVEN without a single byte compared -- a
    # vacuous verdict shaped like the strongest one.  Measured: six [S,S] pairs
    # whose OUR-side COMDAT contradicts the retail body at S's mapped address
    # (0x8231a578 ??1Automator 224 B retail vs a different ~Automator of ours,
    # 0x822c5600 operator>><BAMPhrase> ...) all chased PROVEN.  At depth 0 the
    # question being asked is "is our COMDAT for S the retail body named S?",
    # and that is answered below by the same byte/reloc tests as any pair.
    if depth > maxdepth:
        out.append((depth, "DEPTH-CAP", survivor, our_name))
        return False

    rt, ob = tgt.get(survivor), ours.get(our_name)
    if rt is None or ob is None:
        # Retail-side placeholders carry no address information; our side being
        # absent means we never compile that spelling. Neither is evidence FOR a
        # fold, so both are refusals, not tolerances.
        out.append((depth, "MISSING(%s)" % ("retail" if rt is None else "ours"),
                    survivor, our_name))
        return False
    if vacuous(rt) or vacuous(ob):
        # ★ W10-B.  The vacuity guard exists because a MASKED comparison of a
        # tiny body is non-discriminating: a 4-byte `b X` compares equal to
        # every other 4-byte `b Y` once the displacement is masked out.  That
        # reasoning is about what MASKING HIDES, and it simply does not apply
        # when nothing is hidden -- if the bodies are byte-identical AND every
        # relocation agrees on offset, type AND TARGET NAME, then the two
        # COMDATs satisfy /OPT:ICF's own folding condition exactly and there is
        # no masked field left for a coincidence to occupy.  Refusing here is
        # not conservative, it is vacuous in the other direction.
        #
        # MEASURED: this is what blocked `list<char*>::insert` for two lanes
        # (W8-D, W9-B).  The chase walks cleanly through BOTH allocator levels
        # -- their bodies match -- and dies on the 8-byte `operator new` thunk
        # ??2CriticalSection@@SAPAXI@Z / ??2ChunkAllocator@@SAPAXI@Z, whose two
        # sides are identical in every byte and whose single relocation names
        # `?MemAlloc@@YAPAXHH@Z` on BOTH sides.  W9-B read the resulting chain
        # of SLOT-REFUTED frames -- which are only the leaf failure propagating
        # back up the recursion -- as an "allocator debug-overload" blocker.  It
        # never was one.
        #
        # The 8-byte class is exactly what tools/alloc_fold_gate.py was built to
        # adjudicate ("Eight bytes is BETTER evidence than four, not worse"), and
        # its --shape-census measured the discrimination: 712 retail 8-byte
        # <word>+<branch> bodies carry 707 distinct (word, destination) combos,
        # so the destination does all the work.  Here both destinations are the
        # same NAME, which is the strongest agreement available, not a blur.
        #
        # STRICTNESS: full equality is required, relocation target names
        # INCLUDED.  A vacuous pair whose reloc names differ still REFUSES, so
        # this cannot admit a template twin -- see --chasetest's in-family decoy.
        #
        # ★★★ W16-GG: A DESTINATION MAY BE ACCOUNTED FOR BY A PROOF, NOT ONLY BY
        # LITERAL NAME EQUALITY.  The invariant this branch has always maintained
        # is *every masked field is accounted for*.  For a 4-byte tail-call thunk
        # (`b <target>`; masked body 0x00000000 because the whole instruction IS
        # the relocated field) the destination is the entire information content,
        # so it must be pinned exactly.  Literal name equality pins it -- and so
        # does a RECURSIVELY PROVEN FOLD of the two destinations, because
        # /OPT:ICF is a FIXED POINT: if retail folded X and Y then `b X` and
        # `b Y` resolve to the SAME address, and the two thunks satisfy the
        # folding condition themselves.  Refusing that is not strictness, it is a
        # gap: it refuses a fold BECAUSE the linker folded iteratively.
        #
        # MEASURED (lane W16-GD, refused by GD on purpose because GD would have
        # collected the payout; relaxed by W16-GG, which does not install it):
        # ?CopyTypeProperties@@YAXPAVObject@Hmx@@0@Z, 1,472 B, 4 charges, ALL the
        # same pair -- retail ??$__destroy_aux@UEntry@LocalePanel@@ vs our
        # ??1?$list@VSymbol@@, each 4 B / masked 0x00000000 / one type-6 reloc at
        # offset 0, destinations ?clear@?$_List_base@PAVSynthPollable@@ and
        # ?clear@?$_List_base@VSymbol@@ -- a pair that is FLAT-T1 PROVEN (88 B
        # both sides, retail_bodytwins 1).  The destination fold was proven and
        # the thunk tail-calling it was refused.  Same shape the `list<char*>`
        # note above records, with one difference: there the thunk named the SAME
        # symbol on both sides, so literal equality admitted it.
        #
        # ⚠ STRICTNESS IS *RAISED* ON THE OTHER AXIS, DELIBERATELY.  The general
        # path tolerates a placeholder target; this branch must NOT, because in a
        # vacuous body a tolerated slot means nothing was compared at all.
        # Measured scope of that hazard on this tree: retail vacuous single-reloc
        # thunks against ours of the same masked body and relocation shape form
        # 2,289,106 shape-compatible cross-pairs.  The destination does 100% of
        # the discriminating -- tolerate it and the comparator admits two
        # million pairs.  Hence tolerate_placeholders=False.
        #
        # NET: strictly WIDER than its old self (literal equality still
        # short-circuits below, with no recursion at all, so every previously
        # admitted pair is admitted unchanged) and strictly NARROWER than the
        # general path.  --chasetest's VACUOUS DECOY proves the widening did not
        # dissolve the discriminator; --self-break proves that decoy can go red.
        if rt[0] == ob[0]:
            if list(rt[1]) == list(ob[1]):
                out.append((depth, "VACUOUS-BUT-IDENTICAL", survivor, our_name))
                return True
            if _slots_agree(tgt, ours, rt, ob, survivor, our_name, mapped, depth,
                            stack, memo, out, maxdepth,
                            tolerate_placeholders=False):
                out.append((depth, "VACUOUS-DESTINATION-FOLD-PROVEN",
                            survivor, our_name))
                return True
        out.append((depth, "VACUOUS", survivor, our_name))
        return False
    if rt[0] != ob[0]:
        out.append((depth, "BYTES-DIFFER", survivor, our_name))
        return False
    # The general path KEEPS its placeholder tolerance -- see the note inside
    # _slots_agree; removing it regressed a landed positive control.
    ok = _slots_agree(tgt, ours, rt, ob, survivor, our_name, mapped, depth,
                      stack, memo, out, maxdepth, tolerate_placeholders=True)
    memo[key] = ok
    return ok


def family(tgt, ours, our_name, survivor):
    """PIGEONHOLE evidence: N of our spellings collapse to how many retail addresses?

    WHY this and not --chase for a template family.  --chase verifies our whole
    call subtree is byte-identical to retail's, which is STRICTLY STRONGER than
    the question that matters and fails for an irrelevant reason: measured on the
    hash_map row, the chain breaks three levels down because OUR
    ``_Stl_prime<bool>::_S_next_size`` (92 B) differs from what retail's
    ``resize`` calls (96 B).  That is OUR divergence, and it is present in BOTH
    instantiations equally, so it cannot bear on whether RETAIL folded them.

    The retail-internal question is a count.  Partition our spellings and retail's
    addresses by (body, body-of-slot-0-callee) -- a discriminator, not a blur: it
    correctly separates the int-keyed hash_map family from the Symbol-keyed one
    whose ``_M_find`` has different bytes.  If N of ours map onto 1 retail
    address, retail folded them, and the survivor's map name is one arbitrary
    member's spelling.

    ⚠ SCOPE: the retail side counts only PINNED target objs, so an unpinned
    instantiation is invisible.  That can only ADD retail copies, and the address
    at issue is read off retail's own relocation rather than searched for, so the
    bound does not weaken the conclusion.
    """
    import hashlib

    def slot0(side, name):
        rec = side.get(name)
        if not rec or not rec[1]:
            return ("NOREC",)
        callee = side.get(rec[1][0][1])
        return ("BODY", hashlib.sha1(callee[0]).hexdigest()) if callee \
            else ("EXT", rec[1][0][1])

    mb = ours[our_name][0]
    ref = slot0(tgt, survivor)
    ours_fam = sorted(n for n, (m, _r, _s) in ours.items()
                      if m == mb and slot0(ours, n) == ref)
    ret_fam = sorted(n for n, (m, _r, _s) in tgt.items()
                     if m == mb and slot0(tgt, n) == ref)
    ret_all = [n for n, (m, _r, _s) in tgt.items() if m == mb]
    return {"our_family": ours_fam, "retail_family": ret_fam,
            "retail_bodytwins_all": ret_all,
            "excluded_by_slot0": [n for n in ret_all if n not in ret_fam],
            "our_slot0_matches_retail": slot0(ours, our_name) == ref}


def uniqueness(tgt, ours, survivor, our_name):
    ti, oi = body_index(tgt), body_index(ours)
    out = {}
    if survivor in tgt:
        peers = ti[tgt[survivor][0]]
        out["retail_bodytwins"] = len(peers)
        out["retail_bodytwin_names"] = sorted(peers)[:8]
    if our_name in ours:
        peers = oi[ours[our_name][0]]
        out["our_bodytwins"] = len(peers)
        out["our_bodytwin_names"] = sorted(peers)[:8]
    return out


def load_mapped():
    m = json.load(open(ROOT / "scripts/target_symbol_map.json"))
    out = set()
    for _a, n in m.items():
        for x in (n if isinstance(n, list) else [n]):
            if x:
                out.add(x)
    return out


def vacuous_pair(tgt, ours, want_fold):
    """Pick from the LIVE tree a VACUOUS thunk pair for --chasetest.

    Shape, identical for both controls: masked bodies EQUAL, exactly one
    relocation each at the SAME offset and type, both destination names real
    (non-placeholder) and DIFFERENT, both destinations resolvable on their own
    side.  This is precisely the input class W16-GG's relaxation widened, so the
    two controls differ ONLY in whether the destinations are a fold:

    want_fold=True  -- destination bodies byte-identical AND their own
                       relocations literally agree (flat T1), so the fold is
                       proven by the strictest tier available.  EXPECT PROVEN:
                       without this the relaxation would be inert.
    want_fold=False -- ★ THE DECOY.  Destination bodies DIFFER while being the
                       SAME SIZE, so the destinations are demonstrably not one
                       folded COMDAT and a size test cannot stand in for the byte
                       test.  EXPECT REFUTED: this is the direction the
                       relaxation widened, and a widening nobody probed in its
                       own direction is untested.

    Chosen from the live tree in sorted order rather than hardcoded, so the
    control survives map repairs and rebuilds (same reason as the W16-AE
    self-pair controls).  Returns (survivor, ours) or raises.
    """
    import collections
    oidx = collections.defaultdict(list)
    for n in ours:
        mb, rl, _sz = ours[n]
        if len(rl) == 1 and vacuous(ours[n]) and not placeholder(n):
            oidx[(mb, rl[0][0], rl[0][2])].append(n)
    for k in oidx:
        oidx[k].sort()

    for rn_ in sorted(tgt):
        if placeholder(rn_):
            continue
        rmb, rrl, _ = tgt[rn_]
        if len(rrl) != 1 or not vacuous(tgt[rn_]):
            continue
        rdst = rrl[0][1]
        if placeholder(rdst) or rdst not in tgt:
            continue
        rdrec = tgt[rdst]
        for on_ in oidx.get((rmb, rrl[0][0], rrl[0][2]), ()):
            odst = ours[on_][1][0][1]
            if odst == rdst or placeholder(odst) or odst not in ours:
                continue
            odrec = ours[odst]
            if want_fold:
                if odrec[0] == rdrec[0] and list(odrec[1]) == list(rdrec[1]):
                    return rn_, on_
            else:
                if odrec[0] != rdrec[0] and odrec[2] == rdrec[2]:
                    return rn_, on_
    raise SystemExit("REFUSING: no live %s vacuous-thunk pair found -- the "
                     "control would be VACUOUS, which is worse than absent."
                     % ("fold" if want_fold else "decoy"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--survivor")
    ap.add_argument("--ours")
    ap.add_argument("--pairs", help="json list of [survivor, ours] pairs")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--chase", action="store_true",
                    help="recursive T1: relax relocation-target NAME equality to "
                         "a recursively-verified fold of that slot's callee pair")
    ap.add_argument("--family", action="store_true",
                    help="pigeonhole: how many of our spellings collapse onto "
                         "how many retail addresses")
    ap.add_argument("--chasetest", action="store_true",
                    help="controls for --chase, including an IN-FAMILY DECOY")
    ap.add_argument("--self-break", action="store_true",
                    help="run --chasetest with the vacuous branch's destination "
                         "proof REMOVED (shape still checked). The VACUOUS DECOY "
                         "control MUST go red; exits 0 only if it does. Proves "
                         "the control can fail instead of assuming it.")
    a = ap.parse_args()
    if a.self_break:
        globals()["_SELF_BREAK"] = True
        a.chasetest = True

    mapped = load_mapped()
    tgt, ours = load_sides()

    pairs = []
    if a.selftest:
        # A gate that cannot FAIL is worse than no gate, and a gate that cannot
        # PASS is equally useless. Both directions are exercised here against
        # ground truth taken from the already-landed scripts/symbol_aliases.json.
        al = json.load(open(ROOT / "scripts/symbol_aliases.json"))
        pos = None
        for g in al["groups"]:
            for f in g["folded"]:
                if g["survivor"] in tgt and f in ours:
                    pos = (g["survivor"], f)
                    break
            if pos:
                break
        neg_s = next(n for n in tgt if n.startswith("?") and not vacuous(tgt[n])
                     and tgt[n][2] > 400)
        neg_o = next(n for n in ours if n.startswith("?") and not vacuous(ours[n])
                     and ours[n][2] > 400 and ours[n][0] != tgt[neg_s][0])
        pairs = [("POSITIVE CONTROL (expect PROVEN)", pos[0], pos[1]),
                 ("NEGATIVE CONTROL (expect REFUTED)", neg_s, neg_o)]
    elif a.chasetest:
        # ★ The decoy is the point. A random negative control only shows the
        # comparator dislikes unrelated code; it says nothing about whether a
        # RECURSIVE comparator has gone permissive. So the decoy is IN-FAMILY:
        # fn_827B0E78 has the SAME masked body as the int-keyed operator[]
        # survivor (identical machine code) and differs ONLY in that its two
        # callees belong to the Symbol-keyed hashtable family. If --chase
        # accepts it, the recursion has dissolved exactly the discriminator it
        # must preserve, and every verdict it produces is worthless.
        UIC = ("??A?$hash_map@HPAVUIComponent@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@"
               "V?$StlNodeAlloc@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@@3@@"
               "stlpmtx_std@@QAAAAPAVUIComponent@@ABH@Z")
        al = json.load(open(ROOT / "scripts/symbol_aliases.json"))
        pos = next((g["survivor"], f) for g in al["groups"] for f in g["folded"]
                   if g["survivor"] in tgt and f in ours
                   and not vacuous(tgt[g["survivor"]]))
        pairs = [("IN-FAMILY DECOY (expect REFUTED)", "fn_827B0E78", UIC),
                 ("FLAT-T1 GROUP (expect PROVEN)", pos[0], pos[1])]
        # ★ W16-AE SELF-PAIR CONTROLS.  chase() used to return True for any
        # [S, S] pair before comparing a byte, so a survivor whose own COMDAT
        # contradicts the retail body at its mapped address read PROVEN.  The
        # negative control is a name both sides define, non-vacuous, whose
        # retail extent differs from our COMDAT's -- our code for S is NOT the
        # body retail names S, and the chase must say so.  The positive control
        # is a self-pair that is byte- and reloc-identical, so the fix cannot
        # be "refuse every self-pair" either.  Both are chosen from the live
        # tree rather than hardcoded, so the control survives map repairs.
        def _self(pred):
            return next(n for n in tgt if n.startswith("?") and n in ours
                        and not vacuous(tgt[n]) and not vacuous(ours[n])
                        and tgt[n][2] > 100 and pred(tgt[n], ours[n]))
        self_neg = _self(lambda r, o: r[2] != o[2])
        self_pos = _self(lambda r, o: r[0] == o[0] and list(r[1]) == list(o[1]))
        pairs += [("SELF-PAIR NEGATIVE, our COMDAT contradicts retail (expect REFUTED)",
                   self_neg, self_neg),
                  ("SELF-PAIR POSITIVE, byte+reloc identical (expect PROVEN)",
                   self_pos, self_pos)]
        # ★ W16-GG VACUOUS-BRANCH CONTROLS.  chase()'s vacuous branch used to
        # demand LITERAL relocation-name equality and so refused a thunk pair
        # whose destinations are a PROVEN fold (W16-GD, ?CopyTypeProperties@@).
        # Relaxing it to accept a recursively-proven destination widens the
        # branch in exactly one direction, so it is probed in exactly that
        # direction: the DECOY is the same input class with destinations that
        # are NOT a fold (different bodies, identical sizes).  If the decoy ever
        # reads PROVEN the recursion has dissolved the only discriminator a
        # 4-byte thunk has, and every verdict the tool produces is worthless.
        # Run `--self-break` to watch the decoy go red on demand.
        vd_s, vd_o = vacuous_pair(tgt, ours, want_fold=False)
        vf_s, vf_o = vacuous_pair(tgt, ours, want_fold=True)
        pairs += [("VACUOUS DECOY, destinations are NOT a fold (expect REFUTED)",
                   vd_s, vd_o),
                  ("VACUOUS FOLD, destinations proven folded (expect PROVEN)",
                   vf_s, vf_o)]
        a.chase = True
    elif a.pairs:
        pairs = [("", s, o) for s, o in json.load(open(a.pairs))]
    else:
        pairs = [("", a.survivor, a.ours)]

    rc = 0
    for label, s, o in pairs:
        verdict, det = adjudicate(tgt, ours, s, o, mapped)
        det.update(uniqueness(tgt, ours, s, o))
        det["survivor_map_resident"] = s in mapped
        print("\n=== %s" % (label or "%s  <->  %s" % (s[:60], o[:60])))
        print("  survivor : %s" % s)
        print("  ours     : %s" % o)
        print("  FLAT T1  : %s" % verdict)
        for k, v in det.items():
            if k in ("survivor", "ours"):
                continue
            print("      %-28s %s" % (k, v))
        if a.family and s in tgt and o in ours:
            f = family(tgt, ours, o, s)
            print("  FAMILY   : %d of ours -> %d retail address(es)"
                  % (len(f["our_family"]), len(f["retail_family"])))
            print("      our_slot0_matches_retail   %s" % f["our_slot0_matches_retail"])
            print("      excluded_by_slot0          %s"
                  % [x[:48] for x in f["excluded_by_slot0"]])
            for n in f["our_family"]:
                print("        ours   %s" % n[:86])
            for n in f["retail_family"]:
                print("        RETAIL %s" % n[:86])
        if a.chase:
            trace = []
            ok = chase(tgt, ours, s, o, mapped, out=trace)
            verdict = "PROVEN" if ok else "REFUTED"
            print("  CHASED T1: %s" % verdict)
            for d, kind, x, y in trace:
                print("      %s%-22s %s" % ("  " * d, kind, x[:64]))
                print("      %s%-22s %s" % ("  " * d, "", y[:64]))
        if a.selftest or a.chasetest:
            want = "REFUTED" if ("NEGATIVE" in label or "DECOY" in label) else "PROVEN"
            if verdict != want:
                print("  ** CONTROL FAILED: wanted %s **" % want)
                rc = 1
    if a.self_break:
        # Under --self-break the relaxation is deliberately broken, so a GREEN
        # run is the failure: it would mean the decoy cannot detect the very
        # permissiveness it exists to detect.
        if rc:
            print("\nself-break OK -- the VACUOUS DECOY went RED with the "
                  "destination proof removed, so the control discriminates.")
            return 0
        print("\nself-break FAILED -- controls stayed GREEN with the destination "
              "proof removed. The decoy control is VACUOUS; do not trust it.")
        return 1
    if a.selftest or a.chasetest:
        print("\nselftest %s" % ("FAILED" if rc else "PASSED -- the instrument can "
                                                     "both pass and fail"))
    return rc


if __name__ == "__main__":
    sys.exit(main())
