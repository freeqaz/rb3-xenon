#!/usr/bin/env python3
"""Census the rows where THE TWO HEADLINE RULERS DISAGREE, and surface the ones
that are counted as matched despite a GENUINELY WRONG CONSTANT.

WHY THIS EXISTS
---------------
report.json carries two scores per function and they are computed differently
(objdiff @ objdiff-cli/src/cmd/report.rs:866-879):

    matched_functions += 1     iff  match_percent_normalized == 100
    matched_code += size       iff  fuzzy_match_percent      == 100   <-- RAW

and `fuzzy_match_percent` is a MISNOMER -- report.rs:869 assigns it the RAW
`match_percent`, while `match_percent_normalized` is that score with
`arg_diff_score` subtracted (objdiff-core/src/diff/code.rs:276-291). So a row can
be normalized-100 (counted as a matched FUNCTION) while its bytes are withheld
from matched CODE. Lane DB-4 measured the disagreement at 219 rows / 101,996 B /
0.954pp and warned it was UNSIZED by mechanism; lane DC-4 sized it.

⚠⚠ THE RULER THIS TOOL ASSUMES. The ninja report edge hard-codes
`function_reloc_diffs: FunctionRelocDiffs::None` (report.rs:401 -- CLAUDE.md's
long-standing "report.rs:394" citation has DRIFTED by 7 lines). Under `None`,
`relax_reloc_diffs` is true (code.rs:927) and `reloc_eq` returns true for any two
relocations with matching flags REGARDLESS OF TARGET SYMBOL NAME (code.rs:942).

⇒ ★ A NAMING / WRONG-CALLEE DIFFERENCE COSTS ZERO IN **BOTH** RULERS, so it can
never be what withholds bytes. The "boundary/naming sub-class" that DB-4 believed
this population contained is not merely measured-zero, it is STRUCTURALLY
IMPOSSIBLE HERE. (Same fact as the memory "reloc args are SCORE-INVISIBLE".)
Measured at 4c1ae369: naming/boundary = 0 rows / 0 B, UNKNOWN bucket = 0 rows.
DB-4's own specimen `?Frame@SIVideo@@` was never a naming case and is absent at
HEAD (DA-1's fix held).

WHAT IS ACTUALLY IN THE POPULATION (4c1ae369: 219 rows / 101,996 B / 0.954244pp)
    PERMUTER-CLASS   205 rows / 100,844 B / 0.943466pp  (register 182,
                     branch-dest 21, scheduling 2) -- OFF by standing directive.
    SHIFT-OR-MASK     14 rows /   1,152 B / 0.010778pp  <-- THE ACTIONABLE CLASS
    naming/boundary    0 rows
    UNKNOWN            0 rows

★ THE FINDING WORTH ACTING ON. PPC shift amounts and `rlwinm` MB/ME fields render
as `InstructionArgValue::Opaque`, NOT `Signed|Unsigned`, so `is_immediate` is false
(code.rs:1245-1252) and their penalty is folded into `arg_diff_score` -- i.e.
normalized away. Those functions are COUNTED IN `matched_functions` WITH A WRONG
CONSTANT. This contradicts objdiff's own stated intent at code.rs:1254-1266
("Immediates ... represent real semantic differences ... must count toward the
normalized score"); the audit behind that comment evidently never covered
Opaque-rendered PPC fields. Specimens: `default/Character` fn_822A49EC
`slwi r3,r11,5` vs `slwi r3,r11,4` (a x32-vs-x16 STRUCT STRIDE, off by 2x);
`default/AccomplishmentProgress` fn_8244C85C `rlwinm r11,r11,0,28,26` vs
`0,31,29` (a different BITFIELD).
This is the standing directive "a metric that hides real bugs is worse than a
lower metric" with names attached.

GUARDS (this tool refuses rather than reporting a comfortable zero)
  * every report.json key read EXACTLY; a missing one REFUSES (no `.get(k, 0)`).
    `size` is serialized as a STRING; `fuzzy_match_percent` is a non-optional
    proto3 float OMITTED IFF 0.0 -- that default is justified, not assumed.
  * the four ruler identities are RE-DERIVED and must match `measures` exactly,
    else REFUSE: the census is meaningless if we cannot reproduce the rulers.
  * UNKNOWN arg kinds get their own bucket and are NEVER folded into a charged
    class. CW-2's first classifier scored an unmeasurable case as a defect --
    the defect-MANUFACTURING direction -- and its largest entry was exactly that.
  * `--null N` runs the identical classifier over the PAIRED-DIVERGENT stratum
    (0 < mpn < 100) for enrichment. ⚠ The naive null (all mpn<100) is ~87%
    unpaired 0% rows and is NOT a conditioned control.

Read-only. Mutates no build input.
"""
import argparse
import collections
import json
import os
import random
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
CFG = ["-c", "functionRelocDiffs=none", "-c", "combineTextSections=true",
       "-c", "combineDataSections=true", "-c", "ppc.calculatePoolRelocations=false"]

# Penalties, from objdiff-core/src/diff/code.rs.
P_INSERT_DELETE, P_REPLACE, P_REG, P_IMM = 100, 60, 5, 1


def charged(tgt, base):
    """(penalty, mechanism) for one listed arg pair, per arg_eq (code.rs:1055-1102)."""
    kt = (tgt or {}).get("type")
    kb = (base or {}).get("type")
    if kt in ("Signed", "Unsigned", "Other") and kb == "Symbol":
        return 0, "uncharged_literal_vs_reloc"
    if kt == "BranchDest" and kb == "Symbol":
        return 0, "uncharged_branchdest_vs_reloc"
    if kt == "Symbol" and kb == "Symbol":
        return 0, "uncharged_reloc_name_only"     # ★ the naming class: FREE
    if kt == "Symbol":
        return P_REG, "reloc_kind_asymmetry"
    if kt == "Register":
        return P_REG, "register"
    if kt == "Other":
        return P_REG, "shift_or_mask_field"       # ★ ACTIONABLE
    if kt == "BranchDest":
        return P_REG, "branch_dest"
    if kt in ("Signed", "Unsigned"):
        return P_IMM, "immediate"
    return P_REG, "UNKNOWN_kind:%s->%s" % (kt, kb)


def analyse(d):
    total, mech, unknown = 0, collections.Counter(), 0
    for ins in d.get("instructions") or []:
        mt = ins.get("match_type")
        if mt == "equal":
            continue
        if mt in ("insert", "delete"):
            total += P_INSERT_DELETE; mech["structural_insert_delete"] += 1; continue
        if mt == "replace":
            total += P_REPLACE; mech["replace"] += 1; continue
        if mt == "diff_op":
            total += P_REG; mech["opcode_mnemonic"] += 1
        for a in (ins.get("diff_breakdown") or {}).get("arguments") or []:
            p, m = charged(a.get("target"), a.get("base"))
            total += p
            if p:
                mech[m] += 1
                if m.startswith("UNKNOWN"):
                    unknown += 1
    return total, mech, unknown


def load_population(root: Path):
    rp = root / "build" / "45410914" / "report.json"
    r = json.loads(rp.read_text())
    m = r["measures"]
    for k in ("matched_functions", "matched_code", "total_code", "total_functions"):
        if k not in m:
            sys.exit("REFUSING: measures key missing: %s" % k)
    n_mpn100 = sum_fz100 = sum_all = n_all = 0
    pop, both100, divergent = [], 0, []
    for u in r["units"]:
        for f in u.get("functions") or []:
            for k in ("name", "size"):
                if k not in f:
                    sys.exit("REFUSING: function key %r missing in %s" % (k, u["name"]))
            if "match_percent_normalized" not in f:
                sys.exit("REFUSING: match_percent_normalized absent in %s/%s -- it is "
                         "always Some() at report.rs:870, so absence is an anomaly, "
                         "not a zero." % (u["name"], f["name"]))
            mpn = f["match_percent_normalized"]
            fz = f.get("fuzzy_match_percent", 0.0)   # justified: omitted iff 0.0
            sz = int(f["size"])
            n_all += 1; sum_all += sz
            n_mpn100 += mpn == 100.0
            if fz == 100.0:
                sum_fz100 += sz
            row = dict(unit=u["name"], name=f["name"], size=sz, fuzzy=fz, mpn=mpn)
            if mpn == 100.0 and fz < 100.0:
                pop.append(row)
            elif mpn == 100.0:
                both100 += 1
            elif 0.0 < mpn < 100.0:
                divergent.append(row)
    # ---- ruler self-verification: refuse if we cannot reproduce `measures` ----
    checks = [("matched_functions", int(m["matched_functions"]), n_mpn100),
              ("matched_code", int(m["matched_code"]), sum_fz100),
              ("total_code", int(m["total_code"]), sum_all),
              ("total_functions", int(m["total_functions"]), n_all)]
    print("=== ruler self-verification ===")
    bad = 0
    for k, rep, got in checks:
        ok = rep == got
        bad += not ok
        print("  %-18s reported %10d  re-derived %10d  %s"
              % (k, rep, got, "OK" if ok else "*** MISMATCH ***"))
    if bad:
        sys.exit("REFUSING: cannot reproduce %d of 4 ruler identities. The census "
                 "below would be measuring something else." % bad)
    return r, pop, both100, divergent, sum_all


def run_diff(root, cli, unit, sym):
    p = subprocess.run([cli, "diff", "-p", ".", "-u", unit, sym, "-f", "json",
                        "--include-instructions", "-o", "-"] + CFG,
                       cwd=str(root), capture_output=True, text=True, timeout=180)
    i = p.stdout.find("{")
    return json.loads(p.stdout[i:]) if i >= 0 else None


def classify(root, cli, rows, label):
    out = []
    for n, r in enumerate(rows):
        d = run_diff(root, cli, r["unit"], r["name"])
        if d is None or "diff_score" not in d:
            out.append(dict(r, error=1)); continue
        tot, mech, unk = analyse(d)
        out.append(dict(r, pred=tot, score=d["diff_score"]["score"],
                        mech=dict(mech), unknown=unk))
        if (n + 1) % 50 == 0:
            print("   %s %d/%d" % (label, n + 1, len(rows)), file=sys.stderr)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=str(PROJECT_ROOT))
    ap.add_argument("--cli", default="")
    ap.add_argument("--null", type=int, default=0,
                    help="classify N rows of the PAIRED-DIVERGENT (0<mpn<100) "
                         "stratum as a conditioned control")
    ap.add_argument("--out", default="")
    args = ap.parse_args()
    root = Path(args.root)
    cli = args.cli or str(root / "bin" / "objdiff-cli")

    rep, pop, both100, divergent, total_code = load_population(root)
    pb = sum(p["size"] for p in pop)
    print("\n=== POPULATION: match_percent_normalized==100 AND fuzzy<100 ===")
    print("  rows %d   bytes %d   %.6f pp of total_code" % (len(pop), pb,
                                                            100.0 * pb / total_code))
    print("  controls: mpn==100 & fuzzy==100 rows %d ; paired-divergent (0<mpn<100) "
          "rows %d" % (both100, len(divergent)))
    if not pop:
        print("  population empty -- nothing to classify.")
        return 0

    print("\n[classify] population ...", file=sys.stderr)
    got = classify(root, cli, pop, "pop")
    ok = [r for r in got if not r.get("error")]
    exact = sum(1 for r in ok if r["pred"] == r["score"])
    print("\n=== charged-model fidelity (reproduces objdiff's diff_score?) ===")
    print("  usable %d/%d   model EXACT on %d (%.1f%%)"
          % (len(ok), len(got), exact, 100.0 * exact / max(1, len(ok))))
    if exact < 0.9 * len(ok):
        print("  ⚠ model reproduces <90% of scores -- treat the breakdown as "
              "INDICATIVE, not exact.")

    rowcls, mechtot, unknown_rows = collections.Counter(), collections.Counter(), []
    bytecls = collections.Counter()
    actionable = []
    for r in ok:
        for k, v in r["mech"].items():
            mechtot[k] += v
        if r.get("unknown"):
            unknown_rows.append(r)
            rowcls["UNKNOWN"] += 1; bytecls["UNKNOWN"] += r["size"]
            continue
        if r["mech"].get("shift_or_mask_field"):
            rowcls["SHIFT_OR_MASK(actionable)"] += 1
            bytecls["SHIFT_OR_MASK(actionable)"] += r["size"]
            actionable.append(r)
        elif r["mech"].get("register"):
            rowcls["register(permuter)"] += 1; bytecls["register(permuter)"] += r["size"]
        elif r["mech"].get("branch_dest"):
            rowcls["branch_dest(layout)"] += 1; bytecls["branch_dest(layout)"] += r["size"]
        else:
            rowcls["other_charged"] += 1; bytecls["other_charged"] += r["size"]
    print("\n=== ROW CLASSES (UNKNOWN kept SEPARATE, never folded in) ===")
    for k, n in rowcls.most_common():
        print("  %-26s %4d rows  %7d B  %.6f pp"
              % (k, n, bytecls[k], 100.0 * bytecls[k] / total_code))
    print("\n  naming/boundary rows: %d  (STRUCTURALLY IMPOSSIBLE under "
          "functionRelocDiffs=none -- see module docstring)"
          % sum(1 for r in ok if set(r["mech"]) <= {"uncharged_reloc_name_only"}))
    print("\n=== charged args by mechanism ===")
    for k, v in mechtot.most_common():
        print("  %-34s %6d" % (k, v))

    if actionable:
        print("\n=== ★ ACTIONABLE: counted in matched_functions WITH A WRONG "
              "CONSTANT (%d rows / %d B) ===" % (len(actionable),
                                                 sum(r["size"] for r in actionable)))
        for r in sorted(actionable, key=lambda r: -r["size"]):
            print("  %6d B  %-28s %s" % (r["size"], r["unit"], r["name"][:78]))

    if args.null:
        print("\n[classify] conditioned null: paired-divergent stratum ...",
              file=sys.stderr)
        random.Random(23).shuffle(divergent)
        nl = [r for r in classify(root, cli, divergent[:args.null], "null")
              if not r.get("error")]
        nm = collections.Counter()
        for r in nl:
            for k, v in r["mech"].items():
                nm[k] += v
        tp, tn = sum(mechtot.values()), sum(nm.values())
        print("\n=== CONDITIONED NULL: mechanism share, population vs "
              "paired-divergent (n=%d) ===" % len(nl))
        print("  %-34s %9s %9s %9s" % ("mechanism", "pop%", "null%", "enrich"))
        MIN_N = 30
        for k in sorted(set(mechtot) | set(nm), key=lambda k: -mechtot[k]):
            a = 100.0 * mechtot[k] / max(1, tp)
            b = 100.0 * nm[k] / max(1, tn)
            e = ("%.1fx" % (a / b)) if b else "inf"
            # ★ UNDERPOWERED GUARD. Enrichment on a RARE mechanism is noise: two
            # runs of this very tool over the SAME seeded stratum disagreed by 13x
            # on shift_or_mask_field (10.9x at n=120 vs 0.8x at n=400) purely
            # because its null share is ~0.1%. Reporting such a ratio without the
            # count invites exactly the overclaim these censuses keep producing.
            flag = "  ⚠UNDERPOWERED(null n=%d)" % nm[k] if nm[k] < MIN_N else ""
            print("  %-34s %8.1f%% %8.1f%% %9s%s" % (k, a, b, e, flag))
        print("  ⚠ a mechanism at the same rate in both is NOT characteristic of "
              "this population.")
        print("  ⚠ ratios flagged UNDERPOWERED (<%d observed null args) are NOT "
              "usable; raise --null or do not quote them." % MIN_N)

    if args.out:
        Path(args.out).write_text(json.dumps(
            {"population": got, "actionable": actionable}, indent=1))
        print("\nwrote %s" % args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
