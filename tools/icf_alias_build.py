#!/usr/bin/env python3
"""Build scripts/symbol_aliases.json groups from EVIDENCE, ordered by strength.

Evidence tiers (strongest first), per the CB-11 coordinator's ordering:

  T1  RB3 RETAIL ground truth.  The retail bytes at the map-resident survivor S
      (read from the dtk-split target objs) are byte-identical -- modulo relocated
      fields -- to what OUR compiler emits for the folded spelling F.  If retail
      kept exactly the code F compiles to, F and S are one body in the shipped
      binary.  Adjudicates each (S,F) PAIR directly, so it needs no fold-class
      heuristic and no size cap.  Guarded against vacuity (>=4 words, >=50% of
      the body unmasked) -- a 4-byte ``blr`` compares equal to everything.

  T2  OUR COMPILED OBJS.  S and F land in the same our-compiler ICF fold class
      (real ICF algorithm, relocation-target classes refined to a fixpoint).
      Used where retail cannot adjudicate (S outside any pinned unit).  Subject
      to the class-size cap, because the larger the class the weaker the signal.

  T3  dc3 TRANSFER.  Both names at one address in ham_xbox_r.map.  POSITIVE ONLY:
      a dc3 non-fold does NOT refute, because dc3 is the newer engine and its
      bodies drift (RB3's DataNode::Int has no type check; dc3's does).

Hard gates preserved from the existing file:
  * exactly ONE map-resident symbol per group (the survivor)
  * every folded spelling is referenced by >=1 of our compiled objs
  * only spellings OBSERVED as census noise against that survivor are emitted
"""

import argparse
import collections
import glob
import json
import pickle
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools"))
from icf_fold_evidence import function_bodies, masked_body  # noqa: E402
from icf_alias_finder import coff_referenced_symbols        # noqa: E402

sys.path.insert(0, str(PROJECT_ROOT / "scripts" / "harvest"))
try:
    from live_units import filter_live
except Exception:
    filter_live = None

MIN_WORDS, MIN_UNMASKED_FRAC = 4, 0.50

# Retail-side symbol spellings that carry no identifying information: the dtk split
# leaves un-named functions as fn_<addr>, labels as lbl_/jumptable_, and section /
# string-pool symbols are not comparable across the two objs. These are TOLERATED on
# the retail side (the same tolerance objdiff's NameCheck applies).
_PLACEHOLDER = ("fn_", "lbl_", "jumptable_", "$", ".", "except_data_", "vftable_",
                "string_", "unnamed_", "??_C@", "__")


def placeholder(n: str) -> bool:
    return (not n) or n.startswith(_PLACEHOLDER)


def relocs_agree(rt, ob, mapped=frozenset(), strict=True, tally=None) -> bool:
    """Do retail(S) and ours(F) agree on relocation TARGETS, not just shape?

    ICF folds COMDATs only when their relocations resolve to the SAME symbols, so a
    masked-byte comparison alone is vacuous for template twins: vector<Foo>::erase and
    vector<Bar>::erase have identical machine bytes and differ ONLY in the destructor
    they call. Masking that field folds exactly the discriminator. Compare the target
    names, tolerating retail-side placeholders that carry no information.

    ★ LANE CD-9 PRECISION FIX (``strict=True``, the default).
    The blanket placeholder tolerance above is UNSOUND, and it is unsound in exactly
    the direction that costs precision -- it silently re-opens the template-twin hole
    it was written to close. The dtk split only leaves a retail callee spelled
    ``fn_<B>`` when address B is ABSENT from scripts/target_symbol_map.json (had B been
    mapped, obj_target_symbol_renamer would have rewritten it to the mangled name).
    So if OUR callee ``on`` IS map-resident, at address A, then A != B necessarily, and
    retail's slot demonstrably calls a DIFFERENT function than ours does. Two bodies
    that call different functions are not one COMDAT and cannot have been folded.
    Tolerating that slot manufactures a fold from an absence of information.

    Therefore: a retail-side ``fn_``/``lbl_`` placeholder is tolerated ONLY when our
    side is also unresolvable. When our side is map-resident it is a REFUTATION.
    (Placeholders that are not address-bearing -- ``$``, ``.``, ``__`` -- keep the old
    tolerance; they carry no address to contradict.)
    """
    rr, orr = rt[1], ob[1]
    if len(rr) != len(orr):
        return False
    for (ro, rn, rty), (oo, on, oty) in zip(rr, orr):
        if ro != oo or rty != oty:
            return False
        if rn == on:
            continue
        if strict and rn.startswith(("fn_", "lbl_")) and on in mapped:
            if tally is not None:
                tally["refuted_mapped_callee_vs_placeholder"] += 1
            return False
        if placeholder(rn) or placeholder(on):
            if tally is not None and rn.startswith(("fn_", "lbl_")):
                tally["tolerated_placeholder"] += 1
            continue
        return False
    return True


def collect(paths):
    out = {}
    for p in paths:
        for name, raw, relocs in function_bodies(Path(p)):
            out.setdefault(name, (masked_body(raw, relocs), relocs, len(raw)))
    return out


def vacuous(rec):
    mb, relocs, size = rec
    if size < MIN_WORDS * 4:
        return True
    masked = sum(4 for (o, _n, _t) in relocs if o + 4 <= size)
    return (size - masked) < MIN_UNMASKED_FRAC * size


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--max-class", type=int, default=8,
                    help="T2 evidence-class size cap (default 8)")
    ap.add_argument("--tiers", default="1,2,3")
    ap.add_argument("--out", required=True)
    ap.add_argument("--stats", default="")
    ap.add_argument("--worklist", default="", help="write the post-alias triage backlog")
    ap.add_argument("--sites", default=str(Path.home() / "tmp" / "cd9_allsites.json"),
                    help="census from tools/icf_site_census.py (.json) or a legacy .pkl")
    ap.add_argument("--evidence", default=str(Path.home() / "tmp" / "cd9_evidence.json"),
                    help="output of tools/icf_fold_evidence.py")
    ap.add_argument("--merge", default="",
                    help="carry forward hand-verified groups from an existing "
                         "symbol_aliases.json that the generator cannot re-derive")
    ap.add_argument("--enumerate", choices=("census", "hash", "both"), default="census",
                    help="candidate source: observed diff sites (census), retail-vs-ours "
                         "body-hash collisions (hash), or the union (both)")
    ap.add_argument("--observed-only", action="store_true",
                    help="emit only aliases that actually fire at an observed site "
                         "(precision-first: an alias that fires nowhere today can still "
                         "hide a real defect tomorrow)")
    ap.add_argument("--loose-placeholders", action="store_true",
                    help="restore the pre-CD-9 blanket retail-placeholder tolerance "
                         "(UNSOUND -- for A/B measurement of the gate only)")
    args = ap.parse_args()
    tiers = {int(x) for x in args.tiers.split(",") if x.strip()}
    strict = not args.loose_placeholders

    ev = json.loads(Path(args.evidence).read_text())
    dc3 = ev["dc3_addr"]
    tm = json.loads((PROJECT_ROOT / "scripts" / "target_symbol_map.json").read_text())
    addr_of = {v: k for k, v in tm.items()
               if isinstance(k, str) and k.lower().startswith("0x") and isinstance(v, str)}
    mapped = set(addr_of)
    cls_of, cls_size = {}, {}
    for i, c in enumerate(ev["our_classes"]):
        for m in c["members"]:
            cls_of[m] = i
            cls_size[m] = len(c["members"])

    our_objs = glob.glob(str(PROJECT_ROOT / "build/45410914/src/**/*.obj"), recursive=True)
    print("reading our objs ...", file=sys.stderr)
    ours = collect(our_objs)
    referenced = set()
    for p in our_objs:
        referenced |= coff_referenced_symbols(Path(p).read_bytes())
    tgt = glob.glob(str(PROJECT_ROOT / "build/45410914/obj/*.obj"))
    if filter_live:
        try:
            tgt = filter_live(tgt, str(PROJECT_ROOT))
        except Exception:
            pass
    print("reading retail target objs ...", file=sys.stderr)
    retail = collect(tgt)
    # tools/icf_alias_finder.py --validate gate (c): the target objs must name
    # EXACTLY the survivor out of each group. So the survivor has to be present in a
    # live pinned unit, and no folded spelling may be. T1 satisfies this implicitly
    # (we read the survivor's retail body); T2/T3 do not, so enforce it for all tiers.
    target_named = set()
    for p in tgt:
        target_named |= coff_referenced_symbols(Path(p).read_bytes())

    sp = Path(args.sites)
    if sp.suffix == ".pkl":
        sites = pickle.load(sp.open("rb"))
    else:
        sites = json.loads(sp.read_text())["records"]
    pairs = collections.Counter()
    hash_cand = set()
    if args.enumerate in ("hash", "both"):
        # ★ CD-9 PRINCIPLED ENUMERATOR (lane CD-7's proposal, cross-binary form).
        # A census can only ever propose folds it happened to OBSERVE, so its recall is
        # bounded by how well the CALLING functions matched -- which has nothing to do
        # with whether the callee folded. Enumerate instead from the bodies themselves:
        # bucket every retail survivor body and every body our compiler emits by
        # (masked bytes, size); any collision is a candidate fold, and the ordinary T1
        # adjudicator then decides it. This finds every fold the evidence can support,
        # not merely the ones a diff happened to charge.
        buckets = collections.defaultdict(list)
        for F, rec in ours.items():
            # same exclusions the census ingestion applies to the base-side name:
            # a callee that is ITSELF map-resident is a real difference, not a fold.
            if F in referenced and F not in target_named and F not in mapped \
                    and not F.startswith("__"):
                buckets[(rec[0], rec[2])].append(F)
        for S, rec in retail.items():
            if S not in mapped or S not in target_named or vacuous(rec):
                continue
            for F in buckets.get((rec[0], rec[2]), ()):
                if F != S:
                    hash_cand.add((S, F))
        print("enumerator: %d hash-collision candidate pairs" % len(hash_cand),
              file=sys.stderr)
    pair_fns = collections.defaultdict(set)
    for unit, fn, rows in sites:
        for kind, t, b in rows:
            if not isinstance(t, str) or not isinstance(b, str):
                continue
            if b in mapped or b.startswith("__") or t.startswith("__"):
                continue
            if t.startswith("except_data_") or "unwind" in b or "chain" in b:
                continue
            pairs[(t, b)] += 1
            pair_fns[(t, b)].add((unit, fn))
    observed = set(pairs)
    if args.enumerate == "hash":
        pairs = collections.Counter({p: pairs.get(p, 0) for p in hash_cand})
    elif args.enumerate == "both":
        for p in hash_cand:
            pairs.setdefault(p, 0)

    groups, stats, ssites = {}, collections.Counter(), collections.Counter()
    reltally = collections.Counter()
    why = {}
    for (t, b), n in pairs.items():
        if t not in mapped or not t or not b:
            stats["reject_survivor_not_mapped"] += 1
            ssites["reject_survivor_not_mapped"] += n
            why[(t, b)] = "reject_survivor_not_mapped"
            continue
        if b not in referenced:
            stats["reject_folded_not_referenced"] += 1
            ssites["reject_folded_not_referenced"] += n
            why[(t, b)] = "reject_folded_not_referenced"
            continue
        if t not in target_named or b in target_named:
            stats["reject_gate_c_target_naming"] += 1
            ssites["reject_gate_c_target_naming"] += n
            why[(t, b)] = "reject_gate_c_target_naming"
            continue
        rt, ob = retail.get(t), ours.get(b)
        tier = None
        if rt is not None and ob is not None and not vacuous(rt):
            if rt[0] == ob[0] and rt[2] == ob[2] and \
                    relocs_agree(rt, ob, mapped, strict, reltally):
                tier = 1
            elif rt[0] == ob[0] and rt[2] == ob[2]:
                stats["reject_RELOC_TARGETS_DIFFER"] += 1
                ssites["reject_RELOC_TARGETS_DIFFER"] += n
                why[(t, b)] = "reject_RELOC_TARGETS_DIFFER"
                continue
            else:
                stats["reject_RETAIL_DIFFER"] += 1
                ssites["reject_RETAIL_DIFFER"] += n
                why[(t, b)] = "reject_RETAIL_DIFFER"
                continue
        if tier is None and cls_of.get(t) is not None and cls_of.get(b) == cls_of.get(t):
            if cls_size[t] <= args.max_class:
                tier = 2
            else:
                stats["reject_T2_over_cap"] += 1
                ssites["reject_T2_over_cap"] += n
                why[(t, b)] = "reject_T2_over_cap"
                continue
        if tier is None and t in dc3 and b in dc3 and dc3[t] == dc3[b]:
            tier = 3
        if tier is None or tier not in tiers:
            _k = "reject_no_evidence" if tier is None else "reject_tier_off"
            stats[_k] += 1
            ssites[_k] += n
            why[(t, b)] = _k
            continue
        stats[f"ACCEPT_T{tier}"] += 1
        ssites[f"ACCEPT_T{tier}"] += n
        why[(t, b)] = f"ACCEPT_T{tier}"
        g = groups.setdefault(t, {"name": None, "address": addr_of[t], "survivor": t,
                                  "folded": [], "_meta": []})
        g["folded"].append(b)
        g["_meta"].append({"folded": b, "tier": tier, "sites": n,
                           "fns": len(pair_fns[(t, b)]),
                           "class_size": cls_size.get(t),
                           "dc3": "CONFIRM" if (t in dc3 and b in dc3 and dc3[t] == dc3[b])
                                  else ("nonfold" if (t in dc3 and b in dc3) else "silent")})

    # drop any group whose survivor collides with another group's folded name
    all_folded = {b for g in groups.values() for b in g["folded"]}
    for t in list(groups):
        if t in all_folded:
            del groups[t]

    gl = sorted(groups.values(), key=lambda g: -sum(m["sites"] for m in g["_meta"]))
    for g in gl:
        g["folded"].sort()
        top = max(g["_meta"], key=lambda m: m["sites"])
        g["name"] = g["survivor"].split("@")[0].lstrip("?") or g["address"]
        tset = sorted({m["tier"] for m in g["_meta"]})
        g["evidence"] = (
            "ICF fold group derived by tools/icf_alias_build.py. Evidence tier(s) "
            + "+".join("T%d" % x for x in tset) + ". "
            + "T1=RB3 retail bytes at the survivor address are byte-identical (modulo "
              "relocated fields, >=4 words, >=50%% unmasked) to our compiled body for the "
              "folded spelling; T2=same our-compiler ICF fold class (class size %s, cap %d); "
              "T3=both names at one address in dc3 ham_xbox_r.map (positive-only transfer). "
            % (top.get("class_size"), args.max_class)
            + "%d folded spelling(s), %d census sites over %d functions."
            % (len(g["folded"]), sum(m["sites"] for m in g["_meta"]),
               len({m["fns"] for m in g["_meta"]})))

    n_sites = sum(m["sites"] for g in gl for m in g["_meta"])
    fset = set()
    for (t, b), n in pairs.items():
        for g in gl:
            if g["survivor"] == t and b in g["folded"]:
                fset |= pair_fns[(t, b)]
    print("\n=== decision census ===")
    for k in sorted(stats, key=lambda k: -ssites[k]):
        print("  %-32s %5d pairs %6d sites" % (k, stats[k], ssites[k]))
    print("  -- reloc-slot adjudication (placeholders %s) --"
          % ("STRICT" if strict else "LOOSE/unsound"))
    for k, v in reltally.most_common():
        print("     %-38s %6d slots" % (k, v))
    print("\ngroups=%d aliases=%d sites=%d fns=%d"
          % (len(gl), sum(len(g["folded"]) for g in gl), n_sites, len(fset)))

    emitted = [{k: v for k, v in g.items() if not k.startswith("_")} for g in gl]

    # ★ CD-9 REPRODUCIBILITY FIX. Lane CB-11/A's committed 465-group file was
    # 461 generated groups PLUS 4 HAND-VERIFIED ones merged in by a step that was
    # never committed, so re-running the committed tools could not reproduce the
    # committed artifact -- and the hand-verified groups are exactly the ones this
    # generator CANNOT re-derive (their survivors are not inside any live pinned unit,
    # so T1 has no retail body to read, and we never compile the debug spelling, so T2
    # has no fold class). Losing them on every regeneration would silently drop proven
    # folds. Carry them explicitly instead, and refuse any conflict.
    if args.merge:
        keep = json.loads(Path(args.merge).read_text())["groups"]
        have_s = {g["survivor"] for g in emitted}
        have_f = {f for g in emitted for f in g["folded"]}
        merged = kept = 0
        for g in keep:
            if g["survivor"] in have_s:
                merged += 1
                continue
            if g["survivor"] in have_f or (set(g["folded"]) & (have_s | have_f)):
                print("  !! CONFLICT, hand group %s overlaps a generated group -- "
                      "NOT merged; adjudicate by hand" % g["name"], file=sys.stderr)
                continue
            emitted.append(g)
            kept += 1
        print("\nmerge: carried %d pre-existing group(s), %d already re-derived"
              % (kept, merged))

    if args.worklist:
        # ★ CD-9 DELIVERABLE. What is left after aliasing is NOT "noise" -- lane CD-7
        # refuted that model. It is a TRIAGE BACKLOG: call sites where retail names one
        # callee and we emit another, and the evidence could not (yet) prove a fold.
        # It is heavily head-weighted, so rank by site count and carry the adjudication
        # verdict, because the verdict is what tells the next lane WHICH instrument to
        # reach for: reject_survivor_not_mapped => the survivor has no identified retail
        # address (an IDENTIFICATION job, not a fold job); reject_RETAIL_DIFFER => the
        # bodies genuinely differ (a real wrong-callee or a body-port job);
        # reject_RELOC_TARGETS_DIFFER => twins separated only by a callee, which may
        # become a fold once THAT callee is itself aliased (see the fixpoint note).
        # Pairs already silenced by a carried-forward hand-verified group are NOT
        # backlog -- they are aliased. Counting them would overstate the head and send
        # the next lane at work that is already done.
        aliased = {(g["survivor"], f) for g in emitted for f in g["folded"]}
        wl = []
        for (t, b), n in pairs.items():
            r = why.get((t, b), "unclassified")
            if r.startswith("ACCEPT") or (t, b) in aliased:
                continue
            ex = sorted(pair_fns.get((t, b), ()))[:3]
            wl.append({"sites": n, "verdict": r, "target_names": t, "our_name": b,
                       "n_functions": len(pair_fns.get((t, b), ())),
                       "examples": ["%s::%s" % (u, f) for u, f in ex]})
        wl.sort(key=lambda r: -r["sites"])
        tot = sum(r["sites"] for r in wl)
        head = sum(r["sites"] for r in wl[:100])
        Path(args.worklist).write_text(json.dumps(
            {"total_pairs": len(wl), "total_sites": tot,
             "top100_sites": head,
             "top100_share": (head / tot) if tot else 0.0,
             "by_verdict": {k: sum(r["sites"] for r in wl if r["verdict"] == k)
                            for k in {r["verdict"] for r in wl}},
             "pairs": wl}, indent=1))
        print("\nworklist: %d pairs / %d sites; top 100 = %d sites (%.1f%%) -> %s"
              % (len(wl), tot, head, 100.0 * head / tot if tot else 0, args.worklist))

    out = {"groups": emitted}
    Path(args.out).write_text(json.dumps(out, indent=2))
    if args.stats:
        Path(args.stats).write_text(json.dumps(
            {"stats": dict(stats), "sites": dict(ssites),
             "groups": [dict(g) for g in gl]}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
