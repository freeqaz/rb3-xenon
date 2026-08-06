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

★ ``--xfold`` -- AN EXTERNAL CANDIDATE SOURCE, NOT A FOURTH TIER (lane WS-4).
    decomp-synth's ``tools/revcomp/probes/probe_icf_foldtest.py`` classified the
    12,679 (T,B) relocation-name disagreements this tree produces at the
    ``name_check`` ruler, reading RETAIL BYTES OUT OF THE PE IMAGE at addr(T)
    rather than out of the dtk split objs, and chasing one level of tail-call
    thunk.  Its 7,882 FOLD / FOLD-via-thunk records are supply this generator
    could not previously see: the census enumerator only proposes folds it
    happened to OBSERVE at an icf_site_census site, and the T1 adjudicator can
    only read a survivor that lands in a live pinned target obj.

    ⚠ THE EXTERNAL VERDICT IS *NOT* T1-SHAPED EVIDENCE AND IS NOT TREATED AS
    SUCH.  That probe's comparator (``foldtest2.shape``) masks EVERY D-form
    displacement and EVERY branch displacement unconditionally -- not merely the
    relocated ones -- and never compares relocation TARGET NAMES.  Both
    departures run in the benign-manufacturing direction that this project's
    standing directive calls worse than a lower metric, and the second is
    precisely the template-twin hole ``relocs_agree`` exists to close
    (``vector<Foo>::erase`` and ``vector<Bar>::erase`` have identical machine
    bytes and differ ONLY in the destructor they call).  It also carries no
    anti-vacuity guard.  So ``--xfold`` injects those pairs as CANDIDATES and
    lets the unchanged T1/T2/T3 adjudicators and every hard gate above decide
    them.  The accept rate is the measurement of how much the two comparators
    actually agree; see ``--why`` for the per-pair decision.
"""

import argparse
import collections
import glob
import json
import os
import pickle
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools"))
from icf_fold_evidence import function_bodies, masked_body  # noqa: E402
from coff_bodies_ext import function_bodies_ext             # noqa: E402
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


def collect(paths, label=""):
    """Index {name: (masked_body, relocs, size)} over a set of objs.

    ★ DC-4 (2026-08-02): reads through the CORRECTED body reader. The frozen
    legacy `icf_fold_evidence.function_bodies` accepted a code section only when
    it held EXACTLY ONE type-0x20 def AT OFFSET 0, and both halves of that gate
    misfire on every EH-bearing function (8-byte EH prefix pushes the entry to
    value 8; `__unwind$NNN` is a second type-0x20 def). Tree-wide on our objs
    that is 78,190 symbols seen of 95,247 -- 17,057 / 21.8% invisible.

    ⚠ THIS MOVES BOTH SIDES OF THE T1 COMPARISON, and it is the same population
    fix CY-1 made in icf_site_census: measured there, ext-minus-legacy is +55 on
    TARGET objs vs +2,822 on OUR objs (51x asymmetry), i.e. the recovery is in
    our MSVC /Gy COMDATs and the dtk target side barely moves. Symmetry of the
    EH prefix is preserved on both sides: dtk names the target-side prefix
    `except_data_` (a non-0x20 symbol) so it is excluded from the slice exactly
    as our unnamed prefix is, and `except_data_` is already in _PLACEHOLDER.
    ICF_ALIAS_LEGACY_READER=1 re-derives any pre-DC-4 candidate set.

    ⚠⚠ CONSUMERS: this function is ALSO what tools/icf_alias_audit.py reads
    (it imports `collect`, not the reader), so the audit's population moves with
    it. That is intended -- the audit exists to gate precision on this very set,
    and auditing a set built from a different population than the one shipped
    would be worse than useless.
    """
    use_legacy = os.environ.get("ICF_ALIAS_LEGACY_READER") == "1"
    out = {}
    n_objs = 0
    for p in paths:
        n_objs += 1
        if use_legacy:
            it = ((n, r, rl) for n, r, rl in function_bodies(Path(p), legacy_ok=True))
        else:
            it = ((n, r, rl) for n, r, rl, _e in function_bodies_ext(Path(p)))
        for name, raw, relocs in it:
            out.setdefault(name, (masked_body(raw, relocs), relocs, len(raw)))
    # Population guard: a collapsed COFF read yields a small dict and then a
    # table of zeros that reads exactly like "no alias evidence exists" -- the
    # decisive-negative shape (cf. f592571a / lane CZ-3). Refuse instead.
    if n_objs and len(out) < 0.5 * n_objs:
        sys.exit("REFUSING: collect(%s) read %d symbols from %d objs -- the COFF "
                 "read collapsed. Do not read the evidence counts as a result."
                 % (label or "?", len(out), n_objs))
    print("      collect(%s): %d objs -> %d symbols [%s reader]"
          % (label or "?", n_objs, len(out), "LEGACY" if use_legacy else "corrected"),
          file=sys.stderr)
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
    ap.add_argument("--why", default="",
                    help="★ DC-4: dump the PER-PAIR decision (the `why` dict). The\nsummary census cannot distinguish \"this pair was REFUTED\" from \"this pair was\nnever proposed\", and conflating those is the defect-manufacturing direction --\na landed alias that a reader change silently stopped reproducing must be shown\nto be an actual refutation before anyone acts on it.")
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
    ap.add_argument("--xfold", default="",
                    help="EXTERNAL cross-binary fold verdicts (pairs_folded2.json schema: "
                         "target_symbol / base_symbol / is_call / fold / unit / sym). Adds "
                         "the FOLD-verdict pairs as CANDIDATES -- they are adjudicated by "
                         "the unchanged T1/T2/T3 tiers and every hard gate. See the module "
                         "docstring for why the external verdict is not itself a tier.")
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

    # DC-4: sorted() is load-bearing -- see icf_fold_evidence.main (name
    # collisions + first-wins setdefault => order-dependent results).
    our_objs = sorted(glob.glob(str(PROJECT_ROOT / "build/45410914/src/**/*.obj"), recursive=True))
    print("reading our objs ...", file=sys.stderr)
    ours = collect(our_objs, "ours")
    referenced = set()
    for p in our_objs:
        referenced |= coff_referenced_symbols(Path(p).read_bytes())
    tgt = sorted(glob.glob(str(PROJECT_ROOT / "build/45410914/obj/*.obj")))
    if filter_live:
        try:
            tgt = filter_live(tgt, str(PROJECT_ROOT))
        except Exception:
            pass
    print("reading retail target objs ...", file=sys.stderr)
    retail = collect(tgt, "retail/target")
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
    xfold_pairs = set()
    if args.xfold:
        # ★ WS-4. External FOLD verdicts enter as CANDIDATES only (see the module
        # docstring). Every exclusion the census ingestion applies above is applied
        # here too, so the two supplies are gated identically and the accept census
        # below compares like with like.
        xr = json.loads(Path(args.xfold).read_text())
        xstats = collections.Counter()
        for r in xr:
            xstats["records"] += 1
            v = str(r.get("fold", ""))
            if not v.startswith("FOLD"):
                xstats["skip_not_fold"] += 1
                continue
            if not r.get("is_call"):
                xstats["skip_not_call"] += 1
                continue
            t, b = r.get("target_symbol"), r.get("base_symbol")
            if not isinstance(t, str) or not isinstance(b, str) or not t or not b:
                xstats["skip_no_names"] += 1
                continue
            if b in mapped or b.startswith("__") or t.startswith("__"):
                xstats["skip_census_exclusion"] += 1
                continue
            if t.startswith("except_data_") or "unwind" in b or "chain" in b:
                xstats["skip_census_exclusion"] += 1
                continue
            xstats["kept_sites"] += 1
            xstats["kept_thunk_sites"] += int("thunk" in v)
            xfold_pairs.add((t, b))
            pairs[(t, b)] += 1
            pair_fns[(t, b)].add((r.get("unit", "?"), r.get("sym", "?")))
        print("xfold: %d records -> %d kept sites (%d via-thunk) / %d distinct pairs, "
              "%d already proposed by the census"
              % (xstats["records"], xstats["kept_sites"], xstats["kept_thunk_sites"],
                 len(xfold_pairs), len(xfold_pairs & observed)), file=sys.stderr)
        for k in ("skip_not_fold", "skip_not_call", "skip_no_names",
                  "skip_census_exclusion"):
            print("       %-24s %6d" % (k, xstats[k]), file=sys.stderr)
        # Same failure SHAPE as the DC-4 join guard: a schema drift on either side
        # silently yields zero candidates, which reads exactly like "the external
        # audit found nothing to feed". Refuse instead of reporting a clean zero.
        if not xfold_pairs:
            sys.exit("REFUSING: --xfold %s contributed 0 candidate pairs out of %d "
                     "records. Do not read the census below as 'the external audit "
                     "supplies nothing'." % (args.xfold, xstats["records"]))
        _xn = {t for t, _b in xfold_pairs} | {b for _t, b in xfold_pairs}
        _xhit = len(_xn & (set(ours) | set(retail)))
        print("xfold join check: %d names, INTERSECTION %d (%.1f%%)"
              % (len(_xn), _xhit, 100.0 * _xhit / len(_xn)), file=sys.stderr)
        if _xhit < 0.20 * len(_xn):
            sys.exit("REFUSING: --xfold names do not join against the COFF symbol "
                     "tables (%d/%d)." % (_xhit, len(_xn)))

    # ★ DC-4: JOIN GUARD. Every (t, b) here comes from the sites census; the
    # adjudicators then look t up in `retail`/`mapped` and b up in `ours`. Those
    # are NAME-keyed joins against tables built from a completely different
    # source (the COFF objs and target_symbol_map.json). If a census-format or
    # reader change ever desynchronises the naming on either side, the join goes
    # to ~zero and every downstream count reads as "no fold evidence exists" --
    # the DECISIVE-NEGATIVE shape. That is not hypothetical: CY-1's f592571a
    # changed icf_site_census's emitted unit key from bare stem to full path,
    # four consumers still joined on stems (0/895), and cy4_final_accounting
    # REPORTED AN EMPTY TREE AS DONE with nothing raised (lane CZ-3, d8fbe230).
    # This tool joins on SYMBOL names rather than unit names so it did not share
    # that specific break, but the failure SHAPE is identical, so assert the join
    # landed instead of trusting a small number.
    if pairs:
        _sn = {t for t, _b in pairs} | {b for _t, b in pairs}
        _kn = set(ours) | set(retail)
        _hit = len(_sn & _kn)
        print("join check: census names %d, obj-table names %d, INTERSECTION %d "
              "(%.1f%%)" % (len(_sn), len(_kn), _hit, 100.0 * _hit / len(_sn)),
              file=sys.stderr)
        if _hit == 0 or _hit < 0.20 * len(_sn):
            sys.exit("REFUSING: the sites census does not join against the COFF "
                     "symbol tables (%d/%d names). Do not read the decision "
                     "census below as a result." % (_hit, len(_sn)))
    if args.enumerate == "hash":
        keep = hash_cand | xfold_pairs
        pairs = collections.Counter({p: pairs.get(p, 0) for p in keep})
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
                           "xfold": (t, b) in xfold_pairs,
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
               len({m["fns"] for m in g["_meta"]}))
            + ("" if not any(m.get("xfold") for m in g["_meta"]) else
               " Candidate(s) %s proposed by the decomp-synth relocation-name audit "
               "(2026-08-06) and adjudicated here by the tier above -- the external "
               "FOLD verdict is a candidate source, never the evidence."
               % ", ".join(sorted(m["folded"] for m in g["_meta"] if m.get("xfold")))))

    n_sites = sum(m["sites"] for g in gl for m in g["_meta"])
    fset = set()
    for (t, b), n in pairs.items():
        for g in gl:
            if g["survivor"] == t and b in g["folded"]:
                fset |= pair_fns[(t, b)]
    print("\n=== decision census ===")
    for k in sorted(stats, key=lambda k: -ssites[k]):
        print("  %-32s %5d pairs %6d sites" % (k, stats[k], ssites[k]))
    if xfold_pairs:
        # ★ WS-4. Split the SAME census by candidate source. Without this the
        # external supply's accept rate is invisible -- and that rate IS the
        # measurement of how far the two comparators agree, which is the whole
        # reason the external verdict is not trusted as a tier.
        xs = collections.Counter(why.get(p, "unclassified") for p in xfold_pairs)
        xacc = sum(v for k, v in xs.items() if k.startswith("ACCEPT"))
        print("  -- of which XFOLD-sourced (%d pairs, %d also census-observed) --"
              % (len(xfold_pairs), len(xfold_pairs & observed)))
        for k, v in sorted(xs.items(), key=lambda kv: -kv[1]):
            print("     %-38s %5d pairs" % (k, v))
        print("     XFOLD ACCEPT RATE %d/%d = %.1f%% -- the external comparator "
              "called all %d of these FOLD"
              % (xacc, len(xfold_pairs), 100.0 * xacc / len(xfold_pairs),
                 len(xfold_pairs)))
    print("  -- reloc-slot adjudication (placeholders %s) --"
          % ("STRICT" if strict else "LOOSE/unsound"))
    for k, v in reltally.most_common():
        print("     %-38s %6d slots" % (k, v))
    print("\ngroups=%d aliases=%d sites=%d fns=%d"
          % (len(gl), sum(len(g["folded"]) for g in gl), n_sites, len(fset)))
    if args.why:
        Path(args.why).write_text(json.dumps(
            {"decisions": [[t, b, w] for (t, b), w in why.items()]}))
        print("why: %d per-pair decisions -> %s" % (len(why), args.why))

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
        by_surv = {g["survivor"]: g for g in emitted}
        merged = kept = kept_m = drop_m = 0
        drop_g = drop_gm = 0

        # ★ ICF-GATE-CALIB FIX (2026-08-06): the carry-forward must enforce the
        # SAME hard gates the main adjudication path and
        # `icf_alias_finder.py --validate` enforce, or a landed group that a
        # LATER map correction refuted is re-landed forever. That is not
        # hypothetical: three landed groups (`_M_erase@vector<RCJob*>` @
        # 0x822a1520, `clear@_Rb_tree<..CharInfo@RndFont..>` @ 0x826ddd78,
        # `ClassName@CharCuff` @ 0x82397f08) had been refuted by map-layer
        # corrections (a0d03243 RB3-IMPOSSIBLE pin deletions, eda76311 lane
        # DC-1 proven-false deletions, 3b347d97 ClassName-trio rotation) and
        # this very block carried them straight back into the 2026-08-06
        # regeneration (a3e89f08). Retail-byte adjudication confirms all three
        # FALSE: the ClassName pair exists UNFOLDED at two addresses
        # (0x82397f08 bl->helper->"CharIKHand", 0x8239dc40 bl->helper->
        # "CharCuff"), and the other two survivors name bodies retail's bytes
        # refute (0x826ddd78 is 220 B == our _M_insert@..OverdriveTracker, not
        # the 80 B clear@..RndFont; RCJob has no RB3 RTTI so its vector can
        # never be attested). Gates applied to every carried group/member:
        #   (a) survivor still map-resident AND named in the target objs;
        #   (c) no folded spelling named in the target objs (retail keeping
        #       BOTH spellings is a refutation of the fold);
        #   (b) folded spelling still referenced by >=1 compiled obj (an
        #       unreferenced alias can never fire -- inert, and --validate
        #       fails it; e.g. the 7 stale-anon-namespace-hash Joypad
        #       ?A0x1be4aed2 groups and CheckContextSongLastSong).
        # Never-adjudicated members that PASS the gates are still carried --
        # the census-aging rationale above stands; only validator-refutable
        # carries are dropped, loudly.
        def _carry_group_veto(g):
            s = g["survivor"]
            if s not in mapped:
                return "survivor no longer in target_symbol_map.json (gate a)"
            if s not in target_named:
                return "survivor not named in any live target obj (gate c)"
            return None

        def _carry_member_veto(f):
            if f in target_named:
                return "folded spelling named in target objs -- retail kept BOTH (gate c)"
            if f not in referenced:
                return "folded spelling referenced by 0 compiled objs -- inert (gate b)"
            return None

        for g in keep:
            gveto = _carry_group_veto(g)
            if g["survivor"] in have_s:
                merged += 1
                # ★ WS-4 MEMBER-LEVEL CARRY-FORWARD. Carrying only whole groups
                # is not enough: a landed group whose survivor IS re-derived
                # silently loses any folded spelling this run did not re-derive,
                # and the CANDIDATE ENUMERATOR is a census snapshot, so a
                # spelling can vanish because the census aged -- not because the
                # evidence turned. That is precisely the distinction --why exists
                # to expose ("this pair was REFUTED" vs "never proposed"), and
                # conflating them is the defect-manufacturing direction.
                # So: carry a member back ONLY when it was NEVER ADJUDICATED, and
                # let a REFUSAL stand and be reported.
                cur = by_surv[g["survivor"]]
                for f in g["folded"]:
                    if f in cur["folded"]:
                        continue
                    if f in have_s or f in have_f:
                        # would break the one-survivor-per-group invariant
                        print("  !! landed alias NOT carried, %s already appears in "
                              "another generated group" % f, file=sys.stderr)
                        drop_m += 1
                        continue
                    mveto = _carry_member_veto(f)
                    if mveto is not None:
                        drop_gm += 1
                        print("  !! landed alias NOT carried, validator gate: %s\n"
                              "       survivor %s\n       folded   %s"
                              % (mveto, g["survivor"], f), file=sys.stderr)
                        continue
                    w = why.get((g["survivor"], f))
                    if w is None:
                        cur["folded"].append(f)
                        cur["folded"].sort()
                        kept_m += 1
                    else:
                        drop_m += 1
                        print("  !! landed alias DROPPED, this run REFUTES it (%s):"
                              "\n       survivor %s\n       folded   %s"
                              % (w, g["survivor"], f), file=sys.stderr)
                continue
            # ★ ICF-GATE-CALIB FIX (2026-08-06): only a SURVIVOR collision is a
            # real conflict. A folded spelling appearing under TWO survivors is
            # legitimate when retail kept two identical copies of the body --
            # measured witness: our ??1?$ObjRefConcrete@VWorldCrowd@... (116 B)
            # is masked-byte-identical to retail at BOTH 0x824be710 (map:
            # Spotlight dtor) and 0x824be7b8 (map: RndTransAnim dtor), and
            # dropping the 0x824be710 group costs a real row
            # (CameraShot ??1CamShotCrowd@@QAA@XZ, 100 -> 99.8 under
            # name_check). The old blanket check silently dropped that landed
            # group on every regeneration.
            if g["survivor"] in have_f or (set(g["folded"]) & have_s):
                print("  !! CONFLICT, hand group %s overlaps a generated group's "
                      "SURVIVOR -- NOT merged; adjudicate by hand" % g["name"],
                      file=sys.stderr)
                continue
            dup_f = set(g["folded"]) & have_f
            if dup_f:
                print("  .. note: carried group %s @ %s shares folded spelling(s) "
                      "with another group (retail kept >=2 identical copies): %s"
                      % (g.get("name"), g.get("address"),
                         sorted(x[:60] for x in dup_f)), file=sys.stderr)
            if gveto is not None:
                drop_g += 1
                print("  !! landed group DROPPED, validator gate: %s\n"
                      "       group    %s @ %s\n       survivor %s"
                      % (gveto, g.get("name"), g.get("address"), g["survivor"]),
                      file=sys.stderr)
                continue
            kept_f = []
            for f in g["folded"]:
                mveto = _carry_member_veto(f)
                if mveto is not None:
                    drop_gm += 1
                    print("  !! landed alias NOT carried, validator gate: %s\n"
                          "       survivor %s\n       folded   %s"
                          % (mveto, g["survivor"], f), file=sys.stderr)
                else:
                    kept_f.append(f)
            if not kept_f:
                drop_g += 1
                print("  !! landed group DROPPED, every folded member failed a "
                      "validator gate:\n       group    %s @ %s"
                      % (g.get("name"), g.get("address")), file=sys.stderr)
                continue
            gg = {k: v for k, v in g.items()}
            gg["folded"] = sorted(kept_f)
            emitted.append(gg)
            kept += 1
        print("\nmerge: carried %d pre-existing group(s), %d already re-derived; "
              "member carry-forward: %d never-adjudicated kept, %d REFUTED and "
              "dropped; validator-gate drops: %d group(s), %d member(s)"
              % (kept, merged, kept_m, drop_m, drop_g, drop_gm))

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
                       "xfold_candidate": (t, b) in xfold_pairs,
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
    if args.merge:
        # ★ WS-4: the landed file's `_comment` block is its documentation (how to
        # regenerate, what the gates are, why CB-11/A was not reproducible). The
        # generator used to emit `groups` only, so every regeneration silently
        # deleted it and somebody had to paste it back. Carry it.
        _c = json.loads(Path(args.merge).read_text()).get("_comment")
        if _c is not None:
            out = {"_comment": _c, "groups": emitted}
    Path(args.out).write_text(json.dumps(out, indent=2))
    if args.stats:
        Path(args.stats).write_text(json.dumps(
            {"stats": dict(stats), "sites": dict(ssites),
             "groups": [dict(g) for g in gl]}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
