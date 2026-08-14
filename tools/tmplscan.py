#!/usr/bin/env python3
"""tmplscan.py -- census the TEMPLATE_ARGS stratum into fold / map-name / wrong-arg.

THE STRATUM (lane SIGSCAN-1, 5b9778f3)
======================================
`tools/sigscan.py` classifies every charged relocation-name site tree-wide.
Its biggest class is TEMPLATE_ARGS: target and base name THE SAME TEMPLATE
MEMBER with DIFFERENT TEMPLATE ARGUMENTS (`_Rb_tree<Symbol,String>::_M_insert`
vs `_Rb_tree<Symbol,DataNode>::_M_insert`). ~371 kB of uncrossed rows.

Three hypotheses with OPPOSITE correct actions:
  (A) genuine ICF fold      -- alias, but ONLY if proven.
  (B) wrong map name        -- fix scripts/target_symbol_map.json.
  (C) wrong template arg    -- fix OUR SOURCE.

WHAT THIS ADDS OVER sigscan
===========================
sigscan prices the stratum; it does not adjudicate it. This runs lane
CONTAINER-1's discriminator (227324b3) mechanically over the whole stratum.

★ THE INSTRUMENT. For a charged pair (RN = the name the map gives retail's
symbol, ON = the name OUR obj gives its own), fetch BOTH BODIES -- retail's
from the dtk-split target objs, ours from our compiled objs -- and compare
word-wise with relocated words masked. The verdicts and what each licenses:

  IMM_DIVERGENT  same length, differ in >=1 NON-RELOCATED word.
        ★ DECISIVE. The differing word is a literal the compiler derived from
        the template argument -- for a node-allocating container it is
        `li r3, sizeof(node)` and reads sizeof(T) DIRECTLY (CONTAINER-1's
        `0x1c` vs `0x20`). It ALSO kills (A) outright: different-size COMDATs
        have nothing to fold into. One instruction settles fold-vs-defect.

  BODY_EQ_RELOCS_EQ   masked bodies equal AND every relocation resolves to the
        same target name. The two spellings are ONE BODY. Consistent with a
        real fold, and equally consistent with a plain misname; this member
        cannot tell them apart because the template argument DOES NOT REACH IT.

  RELOC_NAME_ONLY     masked bodies equal, relocation TARGET NAMES differ.
        ⚠ READ THIS CAREFULLY. It is tempting to call it a fold refutation --
        ICF requires identical relocations (CD-7) -- but the differing names
        are THEMSELVES map assignments, i.e. the same defect one level down.
        So it is NOT a refutation; it is a POINTER AT THE SIBLING TO ADJUDICATE
        (this is exactly the `insert_unique -> bl _M_create_node` edge
        CONTAINER-1 walked). Undecidable here, decisive next door.

  LEN_DIFF       bodies differ in length: cannot be one COMDAT. (A) dead.
  NO_TARGET_BODY RN is not defined in any pinned target obj -- the address it
        names lands outside every pinned unit, so retail bytes are unavailable
        here. Undecidable BY PIN, not by nature.
  NO_OUR_BODY    ON is not defined in our compiled objs.

★★★ AND THE TRAP THAT MAKES THIS STRATUM DECEPTIVE, which is why the verdict is
per-MEMBER and then rolled up per-FAMILY: `insert_unique`/`_M_insert` are
T-INDEPENDENT -- they touch only `_Rb_tree_node_base` pointers -- so they score
fuzzy 100.0 against the WRONG T BY CONSTRUCTION. A 100% row is NOT evidence the
template argument is right. Only a T-DEPENDENT member discriminates, so the
useful ranking is "does this family own a decisive member at all", NOT bytes.
Families with none are reported as UNDECIDABLE rather than guessed.
"""
import argparse
import collections
import glob
import json
import os
import struct
import sys

sys.path.insert(0, "tools")
sys.path.insert(0, ".")
from icf_alias_build import placeholder                     # noqa: E402
from coff_bodies_ext import function_bodies_ext             # noqa: E402
from sigscan import demangle_all, split_name, strip_templates, load_report  # noqa: E402


def index(paths):
    """{name: (raw_body, relocs)} over a set of objs. Last definition wins."""
    out = {}
    for p in paths:
        try:
            for name, raw, rel, _val in function_bodies_ext(p):
                out.setdefault(name, (raw, rel))
        except Exception:
            continue
    return out


def masked_words(raw, relocs):
    """[(offset, word_or_None)] -- None where a relocation lands."""
    rel = {o for (o, _n, _t) in relocs}
    ws = []
    for i in range(0, len(raw) - 3, 4):
        ws.append((i, None if i in rel else struct.unpack(">I", raw[i:i + 4])[0]))
    return ws


def compare(rt, ob):
    """(verdict, detail) for retail body rt vs our body ob."""
    (rraw, rrel), (oraw, orel) = rt, ob
    if len(rraw) != len(oraw):
        return "LEN_DIFF", {"retail_len": len(rraw), "our_len": len(oraw)}
    rw, ow = masked_words(rraw, rrel), masked_words(oraw, orel)
    diffs = []
    for (ro, rv), (oo, ov) in zip(rw, ow):
        if rv is None or ov is None:
            # one side relocated and the other not is itself a shape difference
            if (rv is None) != (ov is None):
                diffs.append((ro, rv, ov, "reloc_shape"))
            continue
        if rv != ov:
            diffs.append((ro, rv, ov, "word"))
    if diffs:
        return "IMM_DIVERGENT", {"diffs": diffs[:12], "ndiff": len(diffs)}
    # masked bodies agree -- now the relocation TARGETS
    rmap = {o: n for (o, n, _t) in rrel}
    omap = {o: n for (o, n, _t) in orel}
    if set(rmap) != set(omap):
        return "RELOC_NAME_ONLY", {"shape": "offsets differ"}
    namediff = [(o, rmap[o], omap[o]) for o in sorted(rmap) if rmap[o] != omap[o]]
    if namediff:
        return "RELOC_NAME_ONLY", {"namediff": namediff, "n": len(namediff)}
    return "BODY_EQ_RELOCS_EQ", {}


class Resolver:
    """★ THE SIBLING WALK, MECHANISED.

    CONTAINER-1 adjudicated `insert_unique` by following its `bl _M_create_node`
    edge to the member the template argument actually REACHES. That walk is not
    special to `_Rb_tree`: it is a transitive closure over relocation edges.

    same(RN, ON) answers "is retail's body for RN the same COMDAT as our body
    for ON?":
        DIFFERENT  some word differs, or lengths differ, or SOME CALLEE PAIR
                   resolves DIFFERENT.  Two bodies that call different functions
                   are not one COMDAT (CD-7), so this REFUTES a fold.
        SAME       every word agrees and every callee pair resolves SAME.
        UNKNOWN    a body is unavailable (unpinned target / uninstantiated
                   spelling), or the walk hit a cycle it could not break.

    ⚠ UNKNOWN IS A FIRST-CLASS ANSWER and is reported as such. The failure mode
    to avoid is a resolver that defaults UNKNOWN to SAME, which would manufacture
    folds out of missing pins -- the "instrument confirms whatever you point it
    at" shape. Cycles resolve to UNKNOWN, never to SAME.
    """

    DIFFERENT, SAME, UNKNOWN = "DIFFERENT", "SAME", "UNKNOWN"

    def __init__(self, tgt, ours, depth=6):
        self.tgt, self.ours, self.depth = tgt, ours, depth
        self.memo = {}

    def same(self, rn, on, _stack=None):
        # ⛔ NO `rn == on -> SAME` SHORTCUT. It looks harmless (and a callee slot
        # where both sides name the same symbol is indeed no difference), but
        # `namediff` only ever contains DISAGREEING slots, so the shortcut is
        # unreachable during recursion and fires ONLY on the top-level
        # map-name self-consistency query same(RN, RN) -- turning that test into
        # a tautology that returns CORROBORATED for every pair. It read 659
        # C_OUR_SOURCE before removal.
        key = (rn, on)
        if key in self.memo:
            return self.memo[key]
        _stack = _stack or set()
        if key in _stack or len(_stack) > self.depth:
            return self.UNKNOWN                       # cycle / too deep
        rt, ob = self.tgt.get(rn), self.ours.get(on)
        if rt is None or ob is None:
            self.memo[key] = self.UNKNOWN
            return self.UNKNOWN
        v, detail = compare(rt, ob)
        if v in ("IMM_DIVERGENT", "LEN_DIFF"):
            self.memo[key] = self.DIFFERENT
            return self.DIFFERENT
        if v == "BODY_EQ_RELOCS_EQ":
            self.memo[key] = self.SAME
            return self.SAME
        # RELOC_NAME_ONLY -- recurse on each disagreeing callee slot
        nd = detail.get("namediff")
        if nd is None:
            self.memo[key] = self.UNKNOWN
            return self.UNKNOWN
        res, sub = self.SAME, _stack | {key}
        for _o, crn, con in nd:
            c = self.same(crn, con, sub)
            if c == self.DIFFERENT:
                res = self.DIFFERENT
                break
            if c == self.UNKNOWN:
                res = self.UNKNOWN
        self.memo[key] = res
        return res


def family_of(dem_r, dem_o):
    """A stable key for 'the same template member, differing in its args'."""
    a, b = split_name(dem_r), split_name(dem_o)
    if not a or not b:
        return None
    return strip_templates(a[0])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--controls", action="store_true",
                    help="run the positive/negative controls and exit")
    ap.add_argument("--dump", type=int, default=40)
    ap.add_argument("--json", default=os.path.expanduser("~/tmp/tmplscan.json"))
    args = ap.parse_args()

    print("== indexing objs ==", file=sys.stderr)
    tgt = index(sorted(glob.glob("build/45410914/obj/**/*.obj", recursive=True)))
    ours = index(sorted(glob.glob("build/45410914/src/**/*.obj", recursive=True)))
    print("   target %d syms / ours %d syms" % (len(tgt), len(ours)), file=sys.stderr)

    # ⚠ The alias file's groups are {survivor, folded[]} -- NOT a bare name list.
    # A `.get("symbols")` read returns nothing and subtracts ZERO forgiven pairs
    # while looking like it worked; that inflated this census by 713 pairs on the
    # first run. Built exactly as tools/sigscan.py builds it, and ASSERTED non-empty.
    eq = collections.defaultdict(set)
    aj = json.load(open("scripts/symbol_aliases.json"))
    for g in aj["groups"]:
        grp = set([g["survivor"]] + list(g["folded"]))
        for n in grp:
            eq[n].update(grp)
    assert len(eq) > 100, "alias set parsed as %d names -- format changed?" % len(eq)
    print("   alias set: %d names over %d groups" % (len(eq), len(aj["groups"])),
          file=sys.stderr)

    if args.controls:
        # ★ CAN THE RESOLVER RETURN BOTH ANSWERS? A classifier that only ever
        # says DIFFERENT would "discover" a defect in every pair, which is the
        # shape this project calls "the instrument confirms whatever you point
        # it at". Two controls with real failure modes:
        #
        #  POSITIVE -- scripts/symbol_aliases.json groups are folds already
        #  adjudicated on RETAIL BYTES (icf_alias_build's T1 tier). The resolver
        #  must call (survivor, folded) SAME. Every DIFFERENT here is a
        #  false-refutation the census would report as a phantom defect.
        #
        #  NEGATIVE -- pairs drawn from DIFFERENT template families are not one
        #  body by construction. The resolver must call them DIFFERENT; SAME
        #  here would be a false fold.
        import random
        r = Resolver(tgt, ours)
        pos = collections.Counter()
        posc = collections.Counter()
        for g in aj["groups"]:
            s = g["survivor"]
            # is OUR build's copy of the survivor itself matched? Only then can a
            # body comparison against it mean anything -- otherwise DIFFERENT just
            # restates that the function is unmatched.
            corrob = (s in tgt and s in ours and r.same(s, s) == Resolver.SAME)
            for f in g["folded"]:
                if s in tgt and f in ours:
                    v = r.same(s, f)
                    pos[v] += 1
                    if corrob:
                        posc[v] += 1
        print("\n=== POSITIVE control: %d proven-fold pairs (retail-byte T1) ===" % sum(pos.values()))
        for k, v in pos.most_common():
            print("   %-10s %5d  %5.1f%%" % (k, v, 100.0 * v / max(1, sum(pos.values()))))
        print("   -- restricted to CORROBORATED survivors (%d pairs) --" % sum(posc.values()))
        for k, v in posc.most_common():
            print("   %-10s %5d  %5.1f%%" % (k, v, 100.0 * v / max(1, sum(posc.values()))))

        # the OUR-BUILD fold test on the same proven-fold population. Both sides
        # come from our build, so the allocator contamination that wrecked the
        # retail-vs-ours walk cannot reach it -- this control measures whether
        # that claim actually holds.
        of = Resolver(ours, ours)
        pf = collections.Counter()
        pf1 = collections.Counter()
        pfc = collections.Counter()
        for g in aj["groups"]:
            s = g["survivor"]
            # ⚠ NOT every group is retail-byte evidence. icf_alias_build ships three
            # tiers and only T1 reads RB3 retail bytes; T2 is our own build's fold
            # classes and T3 is a dc3 transfer. Scoring a retail-grounded instrument
            # against T2/T3 groups measures agreement with a weaker instrument, not
            # correctness -- so the T1-only figure is the one to quote.
            t1 = "T1" in (g.get("evidence") or "")
            corrob = (s in tgt and s in ours and r.same(s, s) == Resolver.SAME)
            for f in g["folded"]:
                if s in ours and f in ours:
                    v = of.same(s, f)
                    pf[v] += 1
                    if t1:
                        pf1[v] += 1
                    if t1 and corrob:
                        pfc[v] += 1
        print("   -- OUR-BUILD fold test on the same proven folds (%d pairs) --" % sum(pf.values()))
        for k, v in pf.most_common():
            print("   %-10s %5d  %5.1f%%" % (k, v, 100.0 * v / max(1, sum(pf.values()))))
        print("   -- OUR-BUILD fold test, T1 (retail-byte) groups only (%d pairs) --" % sum(pf1.values()))
        for k, v in pf1.most_common():
            print("   %-10s %5d  %5.1f%%" % (k, v, 100.0 * v / max(1, sum(pf1.values()))))
        # ★ THE ERROR BAR THAT ACTUALLY APPLIES: this is the exact gate
        # C_PROVEN_CALLEE uses (fold test DIFFERENT *and* survivor CORROBORATED).
        print("   -- ... AND survivor CORROBORATED = the C_PROVEN_CALLEE gate (%d pairs) --" % sum(pfc.values()))
        for k, v in pfc.most_common():
            print("   %-10s %5d  %5.1f%%" % (k, v, 100.0 * v / max(1, sum(pfc.values()))))

        rnd = random.Random(20260814)
        cand = [n for n in tgt if n in ours]
        neg = collections.Counter()
        negf = collections.Counter()
        ocand = list(ours)
        for _ in range(400):
            a, b = rnd.choice(cand), rnd.choice(cand)
            if a != b:
                neg[r.same(a, b)] += 1
            a, b = rnd.choice(ocand), rnd.choice(ocand)
            if a != b:
                negf[of.same(a, b)] += 1
        print("=== NEGATIVE control: %d unrelated pairs (retail-vs-ours) ===" % sum(neg.values()))
        for k, v in neg.most_common():
            print("   %-10s %5d  %5.1f%%" % (k, v, 100.0 * v / max(1, sum(neg.values()))))
        print("=== NEGATIVE control: %d unrelated pairs (our-build fold test) ===" % sum(negf.values()))
        for k, v in negf.most_common():
            print("   %-10s %5d  %5.1f%%" % (k, v, 100.0 * v / max(1, sum(negf.values()))))
        return

    # ---- charged sites: identical definition to sigscan ---------------------
    sites = collections.Counter()
    victims = collections.defaultdict(collections.Counter)
    per_fn_charged = collections.Counter()
    for name, (oraw, orel) in ours.items():
        rt = tgt.get(name)
        if not rt or len(rt[1]) != len(orel):
            continue
        for (ro, rn, rty), (oo, on, oty) in zip(rt[1], orel):
            if ro != oo or rty != oty or rn == on:
                continue
            if placeholder(rn) or placeholder(on):
                continue
            if on in eq.get(rn, ()) or rn in eq.get(on, ()):
                continue
            sites[(rn, on)] += 1
            victims[(rn, on)][name] += 1
            per_fn_charged[name] += 1

    names = set()
    for rn, on in sites:
        names.add(rn)
        names.add(on)
    print("== demangling %d names ==" % len(names), file=sys.stderr)
    dem = demangle_all(sorted(names))

    # ---- TEMPLATE_ARGS selection -------------------------------------------
    tmpl = []
    for (rn, on), c in sites.items():
        a, b = split_name(dem.get(rn, "")), split_name(dem.get(on, ""))
        if not a or not b:
            continue
        if a[0] == b[0]:
            continue                       # SIG_SAME_QUALNAME
        if strip_templates(a[0]) != strip_templates(b[0]):
            continue                       # UNRELATED
        tmpl.append((rn, on, c))
    print("== TEMPLATE_ARGS: %d pairs / %d sites ==" % (
        len(tmpl), sum(t[2] for t in tmpl)), file=sys.stderr)

    _r, rows = load_report()

    res = Resolver(tgt, ours)
    # ★★★ THE FOLD TEST, AND WHY IT COMPARES OUR BUILD AGAINST ITSELF.
    # A retail-vs-ours walk is CONTAMINATED by shared T-INDEPENDENT callees: every
    # STL container bottoms out at `MemOrPoolAlloc` (retail) vs `MemOrPoolAllocSTL`
    # (ours) -> `fn_827BCD38` 644 B vs our `MemAlloc` 20 B. That single unrelated
    # allocator divergence marks EVERY instantiation DIFFERENT, so a retail-vs-ours
    # DIFFERENT says nothing about the TEMPLATE ARGUMENT -- it is a global constant
    # wearing a per-pair disguise. (Measured: it flags 51.4% of retail-byte-PROVEN
    # folds as DIFFERENT.)
    #
    # Comparing OUR ours[RN] against OUR ours[ON] cancels it exactly: both sides
    # are our build, so every shared callee is identical BY CONSTRUCTION and the
    # only surviving differences are the ones the template argument actually
    # causes. This is CONTAINER-1's "different-size COMDATs cannot fold",
    # generalised past node size to the whole body:
    #     SAME      -> the two spellings ARE one body -> retail folded them ->
    #                  the charge is a FOLD ALIAS (A), not a source defect.
    #     DIFFERENT -> genuinely different code -> nothing to fold into -> retail
    #                  kept both, so calling the other one is a REAL defect.
    ourfold = Resolver(ours, ours)
    recs = []
    for rn, on, c in tmpl:
        rt, ob = tgt.get(rn), ours.get(on)
        if rt is None:
            verdict, detail = "NO_TARGET_BODY", {}
        elif ob is None:
            verdict, detail = "NO_OUR_BODY", {}
        else:
            verdict, detail = compare(rt, ob)

        # ---- transitive body identity ------------------------------------
        ident = res.same(rn, on) if (rt and ob) else Resolver.UNKNOWN

        # ---- ★ MAP-NAME SELF-CONSISTENCY ---------------------------------
        # Does retail's body at the address the map calls RN match what OUR
        # compiler produces for THAT VERY SPELLING? This is what separates
        # "our source calls the wrong instantiation" from "the map misnamed the
        # address", and it needs no fold model:
        #   ours[RN] present and SAME     -> the map name is corroborated -> (C)
        #   ours[RN] present and DIFFERENT-> the map name is refuted      -> (B)
        #   ours[RN] absent               -> we never instantiate that spelling,
        #                                    which is exactly what (C) predicts,
        #                                    but is not proof. Reported separately.
        if rn not in ours:
            mapname = "NOT_INSTANTIATED"
        elif rn not in tgt:
            mapname = "UNKNOWN"
        else:
            mapname = {Resolver.SAME: "CORROBORATED",
                       Resolver.DIFFERENT: "REFUTED"}.get(res.same(rn, rn), "UNKNOWN")

        # ---- roll up ------------------------------------------------------
        # ⛔ `REFUTED` DOES NOT ESTABLISH (B), AND CALLING IT B_MAP_NAME WAS WRONG.
        # same(RN,RN) compares retail's body at address(RN) with OUR compiled RN.
        # This tree matches ~35% of the binary, so a function that is correctly
        # NAMED but simply NOT YET MATCHED also reads DIFFERENT. The test has one
        # sound direction only: CORROBORATED is proof (our bytes reproduce retail's
        # at that address, so retail's callee IS that instantiation); REFUTED is
        # merely ABSENCE of proof. Labelled accordingly.
        fold = ourfold.same(rn, on) if (rn in ours and on in ours) else Resolver.UNKNOWN
        if fold == Resolver.SAME:
            klass = "A_FOLD_CONSISTENT"
        elif fold == Resolver.DIFFERENT and mapname == "CORROBORATED":
            klass = "C_PROVEN_CALLEE"
        elif fold == Resolver.DIFFERENT:
            klass = "BC_DIFFERENT_UNRESOLVED"
        elif ident == Resolver.SAME:
            # our side cannot be compared (we never instantiate RN), but retail's
            # body IS reproduced by our ON -> one body, fold or misname.
            klass = "AB_ONE_BODY"
        else:
            klass = "UNDECIDABLE"
        byts, rowinfo, seen = 0, [], set()
        for enc, n in victims[(rn, on)].items():
            for row in rows.get(enc, []):
                if row["fuzzy"] >= 100.0 or enc in seen:
                    continue
                seen.add(enc)
                byts += row["size"]
                rowinfo.append({"size": row["size"], "sym": enc, "tier": row["tier"],
                                "unit": row["unit"], "pair_sites": n,
                                "row_sites": per_fn_charged[enc], "fuzzy": row["fuzzy"]})
        rowinfo.sort(key=lambda x: -x["size"])
        if isinstance(detail.get("namediff"), list):
            detail = dict(detail, namediff=detail["namediff"][:8])
        recs.append({"target": rn, "ours": on, "sites": c,
                     "target_dem": dem.get(rn), "ours_dem": dem.get(on),
                     "family": family_of(dem.get(rn, ""), dem.get(on, "")),
                     "verdict": verdict, "ident": ident, "mapname": mapname,
                     "fold": fold, "klass": klass, "detail": detail,
                     "bytes": byts, "rows": rowinfo})

    # ---- census -------------------------------------------------------------
    print("\n=== TEMPLATE_ARGS census by adjudication verdict ===")
    print("  %-20s %6s %7s %7s %12s" % ("verdict", "pairs", "sites", "rows", "uncrossed B"))
    agg = collections.defaultdict(lambda: [0, 0, 0, 0])
    for r in recs:
        a = agg[r["verdict"]]
        a[0] += 1
        a[1] += r["sites"]
        a[2] += len(r["rows"])
        a[3] += r["bytes"]
    for k in sorted(agg, key=lambda k: -agg[k][3]):
        a = agg[k]
        print("  %-20s %6d %7d %7d %12d" % (k, a[0], a[1], a[2], a[3]))
    tot = [sum(agg[k][i] for k in agg) for i in range(4)]
    print("  %-20s %6d %7d %7d %12d" % ("TOTAL", *tot))

    # ---- ★ THE DELIVERABLE: (A)/(B)/(C)/undecidable -------------------------
    print("\n=== stratum split into (A) fold / (B) map name / (C) our source ===")
    print("  %-22s %6s %7s %7s %12s" % ("class", "pairs", "sites", "rows", "uncrossed B"))
    ag2 = collections.defaultdict(lambda: [0, 0, 0, 0])
    for r in recs:
        a = ag2[r["klass"]]
        a[0] += 1
        a[1] += r["sites"]
        a[2] += len(r["rows"])
        a[3] += r["bytes"]
    for k in sorted(ag2, key=lambda k: -ag2[k][3]):
        a = ag2[k]
        print("  %-22s %6d %7d %7d %12d" % (k, a[0], a[1], a[2], a[3]))
    t2 = [sum(ag2[k][i] for k in ag2) for i in range(4)]
    print("  %-22s %6d %7d %7d %12d" % ("TOTAL", *t2))

    print("\n  cross-tab: transitive identity x map-name self-consistency")
    ct = collections.Counter((r["ident"], r["mapname"]) for r in recs)
    for (i, m), n in sorted(ct.items(), key=lambda x: -x[1]):
        print("    %-10s %-18s %5d pairs" % (i, m, n))

    # ---- per family: does a DECISIVE member exist? --------------------------
    print("\n=== families ranked by whether a DECISIVE member exists ===")
    fam = collections.defaultdict(list)
    for r in recs:
        fam[r["family"] or "?"].append(r)
    frows = []
    for f, rs in fam.items():
        dec = [r for r in rs if r["verdict"] in ("IMM_DIVERGENT", "LEN_DIFF")]
        frows.append((bool(dec), sum(r["bytes"] for r in rs), len(rs), len(dec), f))
    frows.sort(key=lambda x: (-x[0], -x[1]))
    print("  %-5s %12s %6s %6s  %s" % ("dec?", "bytes", "pairs", "decis", "family"))
    for d, b, n, nd, f in frows[:args.dump]:
        print("  %-5s %12d %6d %6d  %s" % ("YES" if d else "no", b, n, nd, f[:70]))

    dec_b = sum(b for d, b, _n, _nd, _f in frows if d)
    und_b = sum(b for d, b, _n, _nd, _f in frows if not d)
    print("  ---- families with a decisive member: %d (%d B) ; without: %d (%d B)"
          % (sum(1 for x in frows if x[0]), dec_b,
             sum(1 for x in frows if not x[0]), und_b))

    json.dump(recs, open(args.json, "w"), indent=1)
    print("\n  -> %s" % args.json)


if __name__ == "__main__":
    main()
