#!/usr/bin/env python3
"""Find DECOMP COPY-PASTE TWINS: two of OUR functions with one body, where retail
has two DIFFERENT bodies -- i.e. one of the two was never ported.

THE CLASS, AND WHY THE METRIC CANNOT SEE IT
-------------------------------------------
Lane W2-ENGINE found `BandDirector::OnGetCatList` and `BandDirector::OnCopyCats`
byte-identical in our tree. The body implements copy_cats semantics, so
OnGetCatList's real body was never ported -- yet nothing scored badly, because
the copied body pairs against retail's copy_cats body at 99.96%. The copy scores
high BECAUSE it is a copy. `mpn` and `fuzzy` are structurally incapable of
flagging this, so no amount of grinding the residue queue surfaces it.

⛔ RAW memcmp IS SILENTLY VACUOUS HERE and would return a confident nothing:
PC-relative `bl` displacements differ at different addresses, so identical
functions are NOT identical bytes. This tool reuses `icf_alias_build.collect`,
the relocation-normalized body reader this repo established for exactly this
comparison (via the CORRECTED `coff_bodies_ext` reader -- see its docstring for
the EH-prefix artifact that faked a uniform +8 B and cost a whole lane).

THE THREE ARMS, AND WHICH ONE IS ACTUALLY HIDDEN
------------------------------------------------
Group our compiled symbols by (masked body, relocation targets). For a group,
count how many members are MAP-RESIDENT (present in the dtk-split target objs,
i.e. some retail address is named for them):

  ARM A  >=2 map-resident.  Decisive and cheap: compare the retail bodies
         directly.  ⚠ But this arm is NOT the hidden class -- if retail's two
         bodies differ, our copied twin ALREADY scores <100 against its own
         retail address, so objdiff sees it.  Reported for completeness.

  ARM B  exactly 1 map-resident.  THE HIDDEN CLASS.  The unmapped twin is
         UNPAIRED, so it carries no penalty whatsoever.  But this arm conflates
         two opposite situations, and that conflation is the whole difficulty:
             (i)  retail ICF-FOLDED the two -> one surviving address -> our
                  identical source is CORRECT.  Leave alone.
             (ii) retail has two distinct bodies; ours is a copy-paste and the
                  other body is UNPORTED.  DEFECT.

  ARM C  0 map-resident.  Neither twin is pinned; no retail evidence exists on
         either side.  UNADJUDICABLE -- reported as coverage, never as a verdict.

THE ARM-B DISCRIMINATOR: CALLER MULTIPLICITY
--------------------------------------------
Generalised from the adjudication lane W2 did by hand for the known case.

If a caller C calls both twins, then in retail:
    folded      -> C calls the ONE survivor address TWICE
    not folded  -> C calls TWO DIFFERENT addresses, and only one of them is named
So, over every caller C present on BOTH sides:

    ours_slots   = # relocations in OUR C targeting ANY member of the group
    retail_slots = # relocations in RETAIL C targeting the survivor's name

    retail_slots == ours_slots   =>  consistent with a FOLD        (correct)
    retail_slots <  ours_slots   =>  some site went somewhere else  (DEFECT)

This never needs to name what the other address is, and it never does address
arithmetic -- it compares relocation TARGET NAMES, which is the same evidence
`relocs_agree` is built on. A caller absent from the target objs is COVERAGE
LOSS, not evidence: it is skipped and counted, never scored as agreement.
(Silence must not read as a fold -- that is the direction that manufactures a
benign verdict, which the standing directive calls worse than a lower metric.)

CONTROLS -- run `--selftest` FIRST; a sweep from an uncalibrated instrument that
returns "nothing found" is the most dangerous outcome available here, because it
agrees with the null and closes the vein permanently.

    POSITIVE: BandDirector::OnGetCatList / OnCopyCats must be grouped, must land
              in ARM B, and must be classified DEFECT.
    NEGATIVE: groups drawn from scripts/symbol_aliases.json's T1-PROVEN folds
              (proven on retail bytes) must be classified FOLD.

    python3 tools/twin_body_sweep.py --selftest
    python3 tools/twin_body_sweep.py --arm B --min-size 64
"""

import argparse
import collections
import glob
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from icf_alias_build import collect  # noqa: E402

# A 4-byte `blr` compares equal to everything; the same floors icf_alias_build uses.
DEFAULT_MIN_SIZE = 32


def load_sides():
    tgt = collect(sorted(glob.glob(str(ROOT / "build/45410914/obj/**/*.obj"), recursive=True)),
                  "target")
    ours = collect(sorted(glob.glob(str(ROOT / "build/45410914/src/**/*.obj"), recursive=True)),
                   "ours")
    return tgt, ours


def demangle_parts(n):
    """(class, method) from an MSVC mangled name, best effort. '' when unknown."""
    m = re.match(r"^\?\??_?[A-Za-z0-9_]*\?*", n)
    mm = re.match(r"^\?([A-Za-z0-9_]+)@([A-Za-z0-9_]+)@@", n)
    if mm:
        return mm.group(2), mm.group(1)
    mm = re.match(r"^\?\?([0-9A-Z])([A-Za-z0-9_]+)@@", n)
    if mm:
        return mm.group(2), "??" + mm.group(1)
    return "", ""


def group_ours(ours, min_size):
    """Group our symbols by full relocation-normalized identity."""
    g = collections.defaultdict(list)
    for n, (body, relocs, size) in ours.items():
        if size < min_size:
            continue
        g[(body, tuple(relocs))].append(n)
    return {k: sorted(v) for k, v in g.items() if len(v) > 1}


def build_callsite_index(side, wanted):
    """{callee_name: {caller_name: slot_count}} restricted to callees in `wanted`."""
    idx = collections.defaultdict(lambda: collections.Counter())
    for caller, (_body, relocs, _sz) in side.items():
        for (_off, tname, _ty) in relocs:
            if tname in wanted:
                idx[tname][caller] += 1
    return idx


def adjudicate_armB(members, survivor, our_idx, tgt_idx, ours, tgt):
    """Caller-multiplicity discriminator. Returns (verdict, detail)."""
    ours_by_caller = collections.Counter()
    for m in members:
        for caller, k in our_idx.get(m, {}).items():
            ours_by_caller[caller] += k
    retail_by_caller = tgt_idx.get(survivor, {})

    tot_ours = tot_retail = 0
    covered = skipped = 0
    per_caller = []
    for caller, k in sorted(ours_by_caller.items()):
        if caller not in tgt:            # caller unpinned on the retail side
            skipped += 1
            continue
        covered += 1
        r = retail_by_caller.get(caller, 0)
        tot_ours += k
        tot_retail += r
        if r != k:
            per_caller.append((caller, k, r))

    if covered == 0:
        return "UNADJUDICATED_NO_SHARED_CALLER", {
            "callers_ours": len(ours_by_caller), "callers_skipped": skipped}
    detail = {"shared_callers": covered, "callers_skipped": skipped,
              "slots_ours": tot_ours, "slots_retail": tot_retail,
              "disagreeing": per_caller[:8]}
    if tot_retail < tot_ours:
        return "DEFECT", detail
    if tot_retail == tot_ours:
        return "FOLD", detail
    return "RETAIL_EXCEEDS", detail


def run(min_size, arm_filter, want_json=False, quiet=False):
    tgt, ours = load_sides()
    groups = group_ours(ours, min_size)
    all_members = {n for v in groups.values() for n in v}
    our_idx = build_callsite_index(ours, all_members)
    survivors = {n for v in groups.values() for n in v if n in tgt}
    tgt_idx = build_callsite_index(tgt, survivors)

    rows = []
    for k, members in groups.items():
        resident = [n for n in members if n in tgt]
        size = ours[members[0]][2]
        classes = {demangle_parts(n)[0] for n in members}
        methods = {demangle_parts(n)[1] for n in members}
        # A decomp copy-paste is one CLASS with two DIFFERENT method names.
        # Two classes sharing a method name is a template/twin instantiation.
        copypaste_shape = (len(classes) == 1 and "" not in classes and len(methods) > 1)
        if len(resident) >= 2:
            arm = "A"
            bodies = {n: (tgt[n][0], tuple(tgt[n][1])) for n in resident}
            uniq = len(set(bodies.values()))
            verdict = "DEFECT" if uniq > 1 else "RETAIL_ALSO_IDENTICAL"
            detail = {"retail_distinct_bodies": uniq, "resident": resident}
        elif len(resident) == 1:
            arm = "B"
            verdict, detail = adjudicate_armB(members, resident[0], our_idx, tgt_idx, ours, tgt)
        else:
            arm = "C"
            verdict, detail = "UNADJUDICABLE_NO_PIN", {}
        rows.append({"arm": arm, "verdict": verdict, "size": size,
                     "members": members, "resident": resident,
                     "copypaste_shape": copypaste_shape, "detail": detail})

    rows.sort(key=lambda r: (-r["size"], r["members"][0]))
    if arm_filter:
        rows = [r for r in rows if r["arm"] in arm_filter]
    if want_json:
        print(json.dumps(rows, indent=1, default=str))
        return rows
    if quiet:
        return rows

    tally = collections.Counter((r["arm"], r["verdict"]) for r in rows)
    print(f"\n=== groups (min_size={min_size}B, reloc-normalized identity) ===")
    for (a, v), c in sorted(tally.items()):
        print(f"  ARM {a}  {v:<32s} {c:5d}")
    print("\n=== DEFECT candidates, largest first ===")
    for r in rows:
        if r["verdict"] != "DEFECT":
            continue
        star = "  *COPY-PASTE SHAPE*" if r["copypaste_shape"] else ""
        print(f"\n[ARM {r['arm']}] {r['size']} B{star}")
        for m in r["members"]:
            print(f"    {'MAP' if m in r['resident'] else '   '}  {m}")
        print(f"    {r['detail']}")
    return rows


def selftest():
    """The instrument must FIRE on the known defect and FAIL on proven folds."""
    ok = True
    tgt, ours = load_sides()
    groups = group_ours(ours, DEFAULT_MIN_SIZE)
    all_members = {n for v in groups.values() for n in v}
    our_idx = build_callsite_index(ours, all_members)
    survivors = {n for v in groups.values() for n in v if n in tgt}
    tgt_idx = build_callsite_index(tgt, survivors)

    print("\n--- POSITIVE CONTROL: BandDirector::OnGetCatList / OnCopyCats ---")
    GCL = "?OnGetCatList@BandDirector@@QAA?AVDataNode@@PAVDataArray@@@Z"
    hit = [v for v in groups.values() if GCL in v]
    if not hit:
        print("  FAIL: the known twin pair was not even grouped")
        return False
    members = hit[0]
    resident = [n for n in members if n in tgt]
    print(f"  grouped with: {members}")
    print(f"  map-resident: {resident}  (ARM {'A' if len(resident)>=2 else 'B' if resident else 'C'})")
    if len(resident) != 1:
        print(f"  FAIL: expected ARM B (exactly 1 map-resident), got {len(resident)}")
        ok = False
    else:
        v, d = adjudicate_armB(members, resident[0], our_idx, tgt_idx, ours, tgt)
        print(f"  verdict: {v}  {d}")
        if v != "DEFECT":
            print("  FAIL: instrument did NOT fire on the known defect")
            ok = False
        else:
            print("  PASS: instrument FIRES on the known defect")

    print("\n--- NEGATIVE CONTROL: T1-PROVEN folds from scripts/symbol_aliases.json ---")
    ap = ROOT / "scripts" / "symbol_aliases.json"
    if not ap.exists():
        print("  SKIP: symbol_aliases.json absent")
        return ok
    aliases = json.loads(ap.read_text())
    grp = aliases.get("groups", aliases) if isinstance(aliases, dict) else aliases
    proven = []
    if isinstance(grp, list):
        for g in grp:
            if not isinstance(g, dict):
                continue
            if str(g.get("tier", g.get("evidence", ""))).upper().startswith("T1"):
                s, f = g.get("survivor"), g.get("folded") or []
                if s and f:
                    proven.append((s, list(f)))
    print(f"  T1-proven alias groups available: {len(proven)}")
    tested = fold = other = 0
    for s, folded in proven:
        for f in folded:
            hit = [v for v in groups.values() if f in v and s in v]
            if not hit:
                continue
            members = hit[0]
            resident = [n for n in members if n in tgt]
            if len(resident) != 1:
                continue
            v, _d = adjudicate_armB(members, resident[0], our_idx, tgt_idx, ours, tgt)
            tested += 1
            if v == "FOLD":
                fold += 1
            else:
                other += 1
                if other <= 6:
                    print(f"    non-FOLD on proven pair: {v}  {s} <- {f}")
    print(f"  tested {tested} proven-fold pairs: FOLD={fold} other={other}")
    if tested == 0:
        print("  ⚠ WEAK: no proven fold pair was testable -- negative control did not run")
    elif fold == 0:
        print("  FAIL: instrument flags every proven fold -- it discriminates nothing")
        ok = False
    else:
        print(f"  PASS: instrument does NOT fire on {fold}/{tested} proven folds")
    print(f"\nSELFTEST {'PASS' if ok else 'FAIL'}")
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--min-size", type=int, default=DEFAULT_MIN_SIZE)
    ap.add_argument("--arm", default="")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        sys.exit(0 if selftest() else 1)
    run(a.min_size, set(a.arm) if a.arm else None, a.json)


if __name__ == "__main__":
    main()
