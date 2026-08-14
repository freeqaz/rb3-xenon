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
    if survivor == our_name:
        return True
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
        out.append((depth, "VACUOUS", survivor, our_name))
        return False
    if rt[0] != ob[0]:
        out.append((depth, "BYTES-DIFFER", survivor, our_name))
        return False
    rr, orr = rt[1], ob[1]
    if len(rr) != len(orr):
        out.append((depth, "RELOC-COUNT", survivor, our_name))
        return False

    stack.append(key)
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
            continue
        if not chase(tgt, ours, rn, on, mapped, depth + 1, stack, memo, out, maxdepth):
            out.append((depth, "SLOT-REFUTED", rn[:70], on[:70]))
            ok = False
            break
        out.append((depth + 1, "SLOT-FOLD-OK", rn[:70], on[:70]))
    stack.pop()
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
    a = ap.parse_args()

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
    if a.selftest or a.chasetest:
        print("\nselftest %s" % ("FAILED" if rc else "PASSED -- the instrument can "
                                                     "both pass and fail"))
    return rc


if __name__ == "__main__":
    sys.exit(main())
