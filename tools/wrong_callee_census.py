#!/usr/bin/env python3
"""DR-4: a CONTROLLED census of CALL-target (bl) name mismatches, with denominators.

WHAT QUESTION THIS ANSWERS
--------------------------
The default ruler is built with ``functionRelocDiffs=none``; objdiff's ``reloc_eq``
then returns true REGARDLESS of the target symbol name, and
``match_percent_normalized`` additionally excludes arg-only penalties.  A function
that calls the WRONG CALLEE can therefore read a clean 100 and be fully credited
(five lanes, five waves: TourWeightManager, LayerDir, SetPropertyValue, ...).

``tools/icf_site_census.py`` already enumerates the CHARGED population -- every
relocation slot whose target-side symbol name differs from the base-side one.
What it does NOT emit is the DENOMINATOR, and without a denominator a charge count
cannot be turned into a rate, a base rate, or an enrichment.  This tool emits both
sides of every ratio.

THE UNIT OF ANALYSIS IS A SLOT, NOT A ROW -- and that is the whole point
---------------------------------------------------------------------
A 5 KB function makes ~50 calls; a 20 B accessor makes one.  P(the row contains at
least one mismatched call) therefore rises with size for a purely mechanical
reason, and any per-ROW enrichment between a large-row stratum and a small-row one
is that mechanism, not a finding.  This is the Simpson's-paradox shape this
campaign has already been bitten by (a 3.32x per-row enrichment became 2.03x on the
comparable stratum).  So every rate below is **charged bl slots / total aligned bl
slots**, and every comparison is reported PER SIZE STRATUM as well as pooled.

THE THREE STRATA, named after what is MEASURED (design rule 14)
---------------------------------------------------------------
A charged bl slot falls into exactly one of:

  TGT_ANON    the retail-side callee symbol is a placeholder (``fn_8XXXXXXX``,
              ``lbl_...``).  The measured fact is "our callee has no identified
              retail address".  It is a TRIAGE BACKLOG, not noise and not a
              defect -- the "absent from the map => ICF fold-alias => noise" model
              is REFUTED (it measured identification coverage, enrichment only
              ~1.95x, and retail-byte adjudication refuted 641 pairs / 2,131 sites
              it had called noise).
  BASE_ANON   our own side is the placeholder.  Rare; recorded, never merged.
  NAMED_DIFF  BOTH sides carry a real mangled name and the names differ.  This is
              the only stratum where a wrong-callee CLAIM is even expressible.
              It still contains ICF fold-aliases and map defects -- adjudicate on
              retail bytes (tools/xbin_adjudicate.py), never on the name alone.

ALIGNMENT GATE
--------------
Slot-for-slot comparison is only sound when both sides describe the same code.
Tier 1 (STRICT) requires equal body size AND an identical ``(offset, reloc_type)``
sequence.  Tier 2 (BL) aligns only the type-0x06 call relocations by index, valid
because relocation order is code order; it is reported SEPARATELY and never pooled
into the strict rates, because a body whose data relocations drifted is a body
whose codegen drifted.

CONTROLS (``--selftest``; each is asserted to be able to FAIL)
-------------------------------------------------------------
  1 SPECIFICITY   walking a target obj against ITSELF must charge ZERO slots.
  2 SENSITIVITY   the same walk with base-side names PERMUTED within each function
                  must charge a large fraction -- proving the comparator reads
                  names at all.  A comparator hardcoded to return nothing passes
                  control 1 and fails this one.
  3 WRONG-METHOD  index-alignment WITHOUT the (offset,type) gate, applied to a
                  deliberately reloc-shifted body, must charge slots that the
                  gated method correctly refuses -- i.e. the wrong method must be
                  shown wrong on the same fixture (an unaligned pair is UNALIGNED,
                  not evidence).
  4 JOIN          the report.json join must not collapse (lane CZ-3: a landed fix
                  changed the unit key from bare stem to full path and FOUR tools
                  silently reported an empty tree as done).  Below --min-join the
                  tool REFUSES (exit 4) rather than printing zeros.

Read-only.  Mutates no build input; is not itself a build input.
"""
import argparse
import collections
import json
import os
import random
import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools"))
from coff_bodies_ext import function_bodies_ext  # noqa: E402

BL = 0x06
PLACEHOLDER = re.compile(r'^_?(fn|lbl|jumptable|code|data|bss|rdata)_[0-9a-fA-F_]+$')

# Size strata (bytes).  Chosen BEFORE looking at any rate, on the body-size
# distribution alone, so the boundaries cannot be fitted to a result.
STRATA = [(0, 32), (32, 64), (64, 128), (128, 256), (256, 512),
          (512, 1024), (1024, 4096), (4096, 1 << 30)]


def anon(n):
    return bool(PLACEHOLDER.match(n)) or n.startswith('$')


def stratum(sz):
    for lo, hi in STRATA:
        if lo <= sz < hi:
            return "%d-%d" % (lo, hi) if hi < (1 << 30) else "%d+" % lo
    return "?"


def load_unit_objs(root: Path):
    """unit name -> (target_obj, our_obj), from objdiff.json (authoritative)."""
    cfg = json.loads((root / "objdiff.json").read_text())
    out = {}
    for u in cfg.get("units", []):
        t, b = u.get("target_path"), u.get("base_path")
        if not t or not b:
            continue
        tp, bp = root / t, root / b
        if tp.exists() and bp.exists():
            out[u["name"]] = (tp, bp)
    return out


def index_fns(path):
    out = {}
    for name, raw, relocs, _entry in function_bodies_ext(path):
        out.setdefault(name, (len(raw), relocs))
    return out


def load_report(root: Path):
    """(unit, fn) -> (mpn, fuzzy, size).  Keyed on the FULL unit name."""
    r = json.loads((root / "build/45410914/report.json").read_text())
    o = {}
    for u in r["units"]:
        for f in (u.get("functions") or []):
            o[(u["name"], f["name"])] = (
                float(f.get("match_percent_normalized", 0.0)),
                float(f.get("fuzzy_match_percent", 0.0)),
                int(f.get("size", 0)))
    return o, r["measures"]


def walk(tf, of, permute_base=None):
    """Yield (fn, tier, size, slots) where slots is a list of
    (kind, target_name, base_name) for every ALIGNED bl relocation."""
    for fn, (tsize, trl) in tf.items():
        ob = of.get(fn)
        if ob is None:
            yield fn, "NOT_IN_BASE", tsize, None
            continue
        osize, orl = ob
        strict = (tsize == osize and len(trl) == len(orl) and
                  [(o, t) for o, _n, t in trl] == [(o, t) for o, _n, t in orl])
        if strict:
            tier = "STRICT"
            tb = [(o, n, t) for o, n, t in trl if t == BL]
            ob_ = [(o, n, t) for o, n, t in orl if t == BL]
        else:
            tb = [(o, n, t) for o, n, t in trl if t == BL]
            ob_ = [(o, n, t) for o, n, t in orl if t == BL]
            if not tb or len(tb) != len(ob_):
                yield fn, "UNALIGNED", tsize, None
                continue
            tier = "BL"
        names_b = [n for _o, n, _t in ob_]
        if permute_base is not None and len(names_b) > 1:
            names_b = list(names_b)
            permute_base.shuffle(names_b)
        slots = [(tn, bn) for (_o, tn, _t), bn in zip(tb, names_b)]
        yield fn, tier, tsize, slots


def classify(tn, bn):
    if tn == bn:
        return "AGREE"
    if anon(tn):
        return "TGT_ANON"
    if anon(bn):
        return "BASE_ANON"
    return "NAMED_DIFF"


# ------------------------------------------------------------------ selftest
def _selftest(root: Path) -> int:
    pairs = load_unit_objs(root)
    if not pairs:
        print("SELFTEST REFUSED: no paired units -- build the tree first", file=sys.stderr)
        return 5
    # Pick a fixture unit with plenty of bl slots so no control can be vacuous.
    fixture = None
    for unit, (tp, bp) in sorted(pairs.items()):
        tf = index_fns(tp)
        n = sum(1 for _f, _t, _s, sl in walk(tf, tf) if sl for _x in sl)
        if n >= 200:
            fixture = (unit, tp, bp, tf)
            break
    if fixture is None:
        print("SELFTEST REFUSED: no unit with >=200 self-aligned bl slots", file=sys.stderr)
        return 5
    unit, tp, bp, tf = fixture
    print("fixture unit: %s" % unit)

    # --- control 1: SPECIFICITY.  Target vs ITSELF must charge zero.
    tot = charged = 0
    for _fn, tier, _sz, sl in walk(tf, tf):
        if not sl:
            continue
        for tn, bn in sl:
            tot += 1
            if classify(tn, bn) != "AGREE":
                charged += 1
    print("  [1] specificity  self-vs-self: %d charged of %d slots" % (charged, tot))
    assert tot >= 200, "vacuous: fixture yielded %d slots" % tot
    assert charged == 0, "SPECIFICITY FAILED: self-comparison charged %d" % charged

    # --- control 2: SENSITIVITY.  Permute base names within each function.
    rnd = random.Random(1234)
    ptot = pcharged = 0
    for _fn, tier, _sz, sl in walk(tf, tf, permute_base=rnd):
        if not sl:
            continue
        for tn, bn in sl:
            ptot += 1
            if classify(tn, bn) != "AGREE":
                pcharged += 1
    rate = pcharged / ptot if ptot else 0.0
    print("  [2] sensitivity  permuted-name null: %d charged of %d slots (%.1f%%)"
          % (pcharged, ptot, 100 * rate))
    assert rate > 0.25, ("SENSITIVITY FAILED: permuting names charged only %.3f -- "
                         "the comparator is not reading names" % rate)

    # --- control 3: WRONG METHOD must be shown wrong on the same fixture.
    # Build a deliberately reloc-shifted copy of the fixture (drop the FIRST
    # non-bl relocation from every function).  The gated method must call these
    # UNALIGNED; the ungated index method happily compares shifted slots.
    # ⚠ Scope the control to the rows it actually PERTURBED.  The first version
    # counted all rows and read 206 STRICT survivors -- functions with no non-bl
    # relocation, which `drop=None` leaves byte-identical and which therefore
    # SHOULD still align.  The control caught a defect in its own fixture, which
    # is the only reason it is worth having.
    shifted, perturbed = {}, set()
    for fn, (sz, rl) in tf.items():
        drop = next((i for i, (_o, _n, t) in enumerate(rl) if t != BL), None)
        if drop is None:
            shifted[fn] = (sz, rl)
            continue
        shifted[fn] = (sz, rl[:drop] + rl[drop + 1:])
        perturbed.add(fn)
    gated_strict = sum(1 for f, tier, _s, _sl in walk(tf, shifted)
                       if tier == "STRICT" and f in perturbed)
    ungated = 0
    for fn in perturbed:
        trl, srl = tf[fn][1], shifted[fn][1]
        # the WRONG method: align by index anyway, ignoring (offset,type)
        for (_o, tn, tt), (_o2, bn, bt) in zip(trl, srl):
            if tt != bt or tn != bn:
                ungated += 1
    print("  [3] wrong-method reloc-shifted fixture: %d rows perturbed; gated "
          "STRICT survivors=%d (want 0); ungated index-compare charged %d slots"
          % (len(perturbed), gated_strict, ungated))
    assert len(perturbed) >= 20, "vacuous: only %d rows perturbed" % len(perturbed)
    assert gated_strict == 0, "GATE FAILED: strict tier accepted a reloc-shifted body"
    assert ungated > 0, ("WRONG-METHOD control VACUOUS: the ungated comparator "
                         "produced nothing to be wrong about")

    print("SELFTEST PASS (3 controls, each asserted able to fail)")
    return 0


# ------------------------------------------------------------- foldability
def _foldability(root: Path, census_json: str, out: str) -> int:
    """Is the pair (T,B) ICF-fold-explicable, judged from OUR OWN objs?

    A NAME cannot answer this, and the retail map cannot either (it names only
    41.7% of functions).  Our own compiled bodies can: if our T and our B are
    relocation-masked EQUAL then the two are interchangeable machine code, retail
    could have folded them, and the charge is a NAMING question rather than a
    call-target defect.

    ⚠ This is byte identity used the ONLY way it is sound -- to ask "are these two
    interchangeable", never "where does this body live" (design rule 9).  It is
    calibrated against a LENGTH-CONDITIONED random-pairing null, because "two
    36-byte bodies have the same masked bytes" is common on its own and an
    unconditioned null would flatter the test into manufacturing BENIGN verdicts.
    The rule-7 anti-vacuity guard (>=4 real words AND masked < 50% of body) is a
    hard SKIP, never a verdict.
    """
    os.environ.setdefault("CW2_ROOT", str(root))
    import xbin_adjudicate as xb                                    # noqa: E402

    d = json.loads(Path(census_json).read_text())
    strict = [r for r in d["rows"] if r["tier"] == "STRICT"]
    pairs = collections.Counter()
    fn_of = collections.defaultdict(set)
    for r in strict:
        if r["mpn"] != 100.0:
            continue
        for t, b in r["nd_pairs"]:
            pairs[(t, b)] += 1
            fn_of[(t, b)].add((r["unit"], r["fn"]))
    if not pairs:
        print("REFUSED (exit 4): no NAMED_DIFF pairs at mpn==100 -- an empty "
              "input here is the shape of a decisive negative, not a result.",
              file=sys.stderr)
        return 4

    x = xb.Xbin(str(root))

    def masked(name):
        rec = x.ours.get(name)
        if rec is None:
            return None
        raw, relocs = rec
        m, n = xb.mask_words(raw, relocs)
        if m is None or len(raw) // 4 < 4 or n * 2 >= len(raw) // 4:
            return None                     # rule 7 anti-vacuity guard
        return m

    def foldable(t, b):
        if t not in x.ours:
            return "SKIP:no_our_T"
        if b not in x.ours:
            return "SKIP:no_our_B"
        if len(x.ours[t][0]) != len(x.ours[b][0]):
            return "DISTINCT_OURS"          # different length => cannot fold
        mt, mb = masked(t), masked(b)
        if mt is None or mb is None:
            return "SKIP:guard"
        return "FOLDABLE_OURS" if mt == mb else "DISTINCT_OURS"

    v = {p: foldable(*p) for p in pairs}
    cp, cs = collections.Counter(), collections.Counter()
    for p, r in v.items():
        k = "SKIP" if r.startswith("SKIP") else r
        cp[k] += 1
        cs[k] += pairs[p]
    tot_sites = sum(pairs.values())
    print("=== BYTE FOLDABILITY of NAMED_DIFF pairs at mpn==100 (from OUR objs) ===")
    print("  %d pairs / %d sites / %d caller rows"
          % (len(pairs), tot_sites, len({f for s in fn_of.values() for f in s})))
    for k in ("FOLDABLE_OURS", "DISTINCT_OURS", "SKIP"):
        print("   %-16s pairs %5d (%5.1f%%)   sites %6d (%5.1f%%)"
              % (k, cp[k], 100.0 * cp[k] / len(pairs), cs[k], 100.0 * cs[k] / tot_sites))

    # length-conditioned random-pairing null
    rnd = random.Random(20260803)
    bylen = collections.defaultdict(list)
    for n, (raw, _r) in x.ours.items():
        bylen[len(raw)].append(n)
    nl = collections.Counter()
    for (t, b) in [p for p, r in v.items() if not r.startswith("SKIP")]:
        if t not in x.ours:
            continue
        cand = bylen.get(len(x.ours[t][0]), ())
        if len(cand) < 3:
            continue
        u = w = None
        for _ in range(6):
            u, w = rnd.choice(cand), rnd.choice(cand)
            if u != w:
                break
        if u == w:
            continue
        nl[foldable(u, w)] += 1
    dec = cp["FOLDABLE_OURS"] + cp["DISTINCT_OURS"]
    ndec = nl["FOLDABLE_OURS"] + nl["DISTINCT_OURS"]
    tr = cp["FOLDABLE_OURS"] / max(1, dec)
    nr = nl["FOLDABLE_OURS"] / max(1, ndec)
    print("\n=== CONTROL: length-conditioned random-pairing null ===")
    print("   treated FOLDABLE %5d / %5d decided = %6.2f%%"
          % (cp["FOLDABLE_OURS"], dec, 100 * tr))
    print("   null    FOLDABLE %5d / %5d decided = %6.2f%%"
          % (nl["FOLDABLE_OURS"], ndec, 100 * nr))
    print("   enrichment = %.2fx" % (tr / nr if nr else float("inf")))
    if ndec < 200:
        print("   ⚠ null under-powered (%d decided) -- do not quote the enrichment"
              % ndec)

    # reciprocity: T->B and B->T both charged == bijection-arbitrary signature
    S = set(pairs)
    rec = [p for p in pairs if (p[1], p[0]) in S]
    print("\n=== RECIPROCITY (T->B and B->T both charged) ===")
    print("   %d / %d pairs (%.1f%%) are reciprocal -- the bijection-arbitrary "
          "signature: two shape-identical bodies whose map assignment is a coin "
          "flip, so 'fixing' our source only moves the arbitrariness."
          % (len(rec), len(pairs), 100.0 * len(rec) / len(pairs)))

    Path(out).write_text(json.dumps(
        {"pairs": [[t, b, v[(t, b)], pairs[(t, b)],
                    sorted(map(list, fn_of[(t, b)]))[:50]] for t, b in pairs]}))
    print("\nwrote %s" % out)
    return 0


# ---------------------------------------------------------------------- main
def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=str(PROJECT_ROOT))
    ap.add_argument("--out", default=str(Path.home() / "tmp" / "dr4_callee_census.json"))
    ap.add_argument("--min-join", type=float, default=0.50,
                    help="REFUSE (exit 4) if fewer than this fraction of walked "
                         "functions join report.json -- CZ-3's collapsed-join trap")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--foldability", metavar="CENSUS_JSON",
                    help="second pass: for every NAMED_DIFF pair at mpn==100, ask "
                         "whether OUR OWN compiled bodies for T and B are "
                         "relocation-masked EQUAL (=> ICF-fold-explicable, benign) "
                         "or DISTINCT, calibrated against a length-conditioned "
                         "random-pairing null")
    a = ap.parse_args()
    root = Path(a.root)
    if a.selftest:
        return _selftest(root)
    if a.foldability:
        return _foldability(root, a.foldability, a.out)

    pairs = load_unit_objs(root)
    rep, measures = load_report(root)
    print("[1/3] %d paired units, %d report rows" % (len(pairs), len(rep)), file=sys.stderr)

    rows = []          # per-function records
    st = collections.Counter()
    joined = walked = 0
    for unit, (tp, bp) in sorted(pairs.items()):
        try:
            tf, of = index_fns(tp), index_fns(bp)
        except Exception as e:                                     # pragma: no cover
            st["unit_parse_error"] += 1
            print("   !! %s: %s" % (unit, e), file=sys.stderr)
            continue
        st["units"] += 1
        for fn, tier, tsize, slots in walk(tf, of):
            st["tier_" + tier] += 1
            if slots is None:
                continue
            walked += 1
            r = rep.get((unit, fn))
            if r is not None:
                joined += 1
            mpn, fuzzy, rsize = r if r else (None, None, tsize)
            c = collections.Counter(classify(tn, bn) for tn, bn in slots)
            rows.append({
                "unit": unit, "fn": fn, "tier": tier, "size": rsize,
                "mpn": mpn, "fuzzy": fuzzy, "bl": len(slots),
                "agree": c["AGREE"], "tgt_anon": c["TGT_ANON"],
                "base_anon": c["BASE_ANON"], "named_diff": c["NAMED_DIFF"],
                "nd_pairs": [[tn, bn] for tn, bn in slots
                             if classify(tn, bn) == "NAMED_DIFF"],
                "ta_pairs": [[tn, bn] for tn, bn in slots
                             if classify(tn, bn) == "TGT_ANON"],
            })

    frac = joined / walked if walked else 0.0
    print("[2/3] report join: %d/%d = %.3f" % (joined, walked, frac), file=sys.stderr)
    if frac < a.min_join:
        print("REFUSED (exit 4): report.json join collapsed to %.3f < %.2f. "
              "A key-schema change (see lane CZ-3) empties this pipeline SILENTLY."
              % (frac, a.min_join), file=sys.stderr)
        return 4

    Path(a.out).write_text(json.dumps({"rows": rows, "stats": dict(st),
                                       "measures": measures}))
    print("[3/3] wrote %s" % a.out, file=sys.stderr)
    report(rows, st)
    return 0


def report(rows, st):
    def rates(sel):
        t = collections.Counter()
        for r in sel:
            for k in ("bl", "agree", "tgt_anon", "base_anon", "named_diff"):
                t[k] += r[k]
            t["rows"] += 1
        return t

    strict = [r for r in rows if r["tier"] == "STRICT"]
    blt = [r for r in rows if r["tier"] == "BL"]
    print("\n=== POPULATION (aligned rows with >=0 bl slots) ===")
    for lab, sel in (("STRICT", strict), ("BL-only", blt)):
        t = rates(sel)
        print("  %-8s rows=%-6d bl slots=%-7d  agree=%-7d tgt_anon=%-6d "
              "base_anon=%-4d NAMED_DIFF=%d"
              % (lab, t["rows"], t["bl"], t["agree"], t["tgt_anon"],
                 t["base_anon"], t["named_diff"]))

    print("\n=== CONTROL: per-SLOT rates, mpn==100 (treated) vs mpn<100 (untreated) ===")
    print("  the row unit is a bl SLOT, which removes the mechanical "
          "'big functions make more calls' confound")
    hdr = "  %-10s %8s %8s %8s %8s %8s %8s" % (
        "stratum", "n100", "nd100%", "ta100%", "nLT", "ndLT%", "taLT%")
    print(hdr)
    pool = collections.Counter()
    for lo, hi in STRATA:
        lab = "%d-%d" % (lo, hi) if hi < (1 << 30) else "%d+" % lo
        a100 = [r for r in strict if r["mpn"] == 100.0 and lo <= r["size"] < hi]
        alt = [r for r in strict if r["mpn"] is not None and r["mpn"] < 100.0
               and lo <= r["size"] < hi]
        t1, t2 = rates(a100), rates(alt)
        pool["n100"] += t1["bl"]; pool["nd100"] += t1["named_diff"]; pool["ta100"] += t1["tgt_anon"]
        pool["nlt"] += t2["bl"]; pool["ndlt"] += t2["named_diff"]; pool["talt"] += t2["tgt_anon"]
        f = lambda a, b: (100.0 * a / b) if b else float('nan')
        print("  %-10s %8d %8.3f %8.2f %8d %8.3f %8.2f"
              % (lab, t1["bl"], f(t1["named_diff"], t1["bl"]), f(t1["tgt_anon"], t1["bl"]),
                 t2["bl"], f(t2["named_diff"], t2["bl"]), f(t2["tgt_anon"], t2["bl"])))
    f = lambda a, b: (100.0 * a / b) if b else float('nan')
    print("  %-10s %8d %8.3f %8.2f %8d %8.3f %8.2f"
          % ("POOLED", pool["n100"], f(pool["nd100"], pool["n100"]),
             f(pool["ta100"], pool["n100"]), pool["nlt"],
             f(pool["ndlt"], pool["nlt"]), f(pool["talt"], pool["nlt"])))
    if pool["ndlt"] and pool["n100"]:
        print("  enrichment (NAMED_DIFF, mpn100 / mpn<100) = %.3fx"
              % ((pool["nd100"] / pool["n100"]) / (pool["ndlt"] / pool["nlt"])))
        print("  enrichment (TGT_ANON,   mpn100 / mpn<100) = %.3fx"
              % ((pool["ta100"] / pool["n100"]) / (pool["talt"] / pool["nlt"])))

    print("\n=== THE ADJUDICABLE STRATUM: NAMED_DIFF rows at mpn==100 ===")
    hid = [r for r in strict if r["mpn"] == 100.0 and r["named_diff"] > 0]
    print("  rows=%d  bytes=%d  sites=%d  distinct pairs=%d"
          % (len(hid), sum(r["size"] for r in hid),
             sum(r["named_diff"] for r in hid),
             len({tuple(p) for r in hid for p in r["nd_pairs"]})))
    dp = collections.Counter(tuple(p) for r in hid for p in r["nd_pairs"])
    print("\n  top 15 (target callee <- our callee):")
    for (t, b), n in dp.most_common(15):
        print("   %4d  %s\n         <- %s" % (n, t, b))


if __name__ == "__main__":
    sys.exit(main())
