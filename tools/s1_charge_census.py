#!/usr/bin/env python3
"""S1-FOLDTOOL census: re-derive the pure relocation-NAME charge class (GAP-C "E").

WHY THIS EXISTS AS A SEPARATE STEP
==================================
The lane brief asserts class E = 650,480 B / 2,183 rows / 1,735 distinct pairs.
The standing rule is that a briefed figure is TESTED LITERALLY before anything is
built on it -- and there is a specific reason to doubt it here: lane W33 measured
"rows whose ONLY charges are genuine relocation names" at **0 rows / 0 B**
binary-wide across the frame queue.  If that generalised, class E would be
uncollectable by construction and this lane would be over before it started.
(W33's population was the FRAME queue -- rows selected for a stack-frame
difference, which implies register/immediate charges -- so the two are not
necessarily in conflict.  Deciding that requires measuring, not reasoning.)

THE DISCRIMINATOR (lane W19's, reused verbatim in spirit)
=========================================================
  * charged instruction with match_type != 'diff_arg'         -> HARD
  * diff_arg whose differing typed_args are EXACTLY {Symbol}   -> NAME charge
  * diff_arg where a Register also differs -> charged BY THE REGISTER.
    Naive counting reads these as name charges; W19 measured a row whose
    "138 name charges" were truly ZERO.

Class E := a row all of whose charges are NAME charges (hard==reg==imm==br==0).
Because `matched_code` is ALL-OR-NOTHING per row, only such a row can ever be
crossed by fold/alias/map work alone.  Rows carrying any other charge are class
F ("mixed") and are worth 0 B to this lane no matter how many folds are proven.
"""
import argparse
import collections
import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from scripts.analysis import ruler as ruler_mod  # noqa: E402


def I(x, d=0):
    return d if x is None else int(x)


def F(x, d=0.0):
    return d if x is None else float(x)


def profile(rec):
    """Classify every charged instruction in one objdiff symbol record."""
    hard = namechg = regchg = immchg = brchg = 0
    pairs = collections.Counter()
    for i in rec.get("instructions", []) or []:
        mt = i.get("match_type")
        if mt == "equal":
            continue
        if mt != "diff_arg":
            hard += 1
            continue
        t = i.get("target") or {}
        b = i.get("base") or {}
        ta = t.get("typed_args", []) or []
        ba = b.get("typed_args", []) or []
        kinds = set()
        sp = None
        for x, y in zip(ta, ba):
            if x.get("value") != y.get("value"):
                kinds.add(x.get("type"))
                if x.get("type") == "Symbol":
                    sp = (x.get("value"), y.get("value"))
        if kinds == {"Symbol"} and sp:
            namechg += 1
            pairs[sp] += 1
        elif "Register" in kinds:
            regchg += 1
        elif "BranchDest" in kinds:
            brchg += 1
        else:
            immchg += 1
    return dict(hard=hard, name=namechg, reg=regchg, imm=immchg, br=brchg,
                pairs=pairs)


def run_unit(args):
    proj, cli, rkargs, uname, syms = args
    cmd = [cli, "diff", "-p", str(proj), "-u", uname, "--batch",
           "-f", "json", "-o", "-", "--include-instructions"] + rkargs
    try:
        out = subprocess.run(cmd, capture_output=True, text=True,
                             timeout=1800, input="\n".join(syms) + "\n")
    except subprocess.TimeoutExpired:
        return uname, None, "timeout"
    if out.returncode != 0:
        return uname, None, out.stderr[:200]
    txt = out.stdout.strip()
    if not txt:
        return uname, [], None
    recs = []
    try:
        j = json.loads(txt)
        recs = j if isinstance(j, list) else [j]
    except json.JSONDecodeError:
        for line in txt.splitlines():
            line = line.strip()
            if line:
                try:
                    recs.append(json.loads(line))
                except json.JSONDecodeError:
                    pass
    return uname, recs, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", default=".")
    ap.add_argument("--jobs", type=int, default=12)
    ap.add_argument("--out", default=None)
    ap.add_argument("--limit-units", type=int, default=0)
    ap.add_argument("--include-placeholder", action="store_true",
                    help="include placeholder-named rows (fn_/lbl_/...) -- "
                         "GAP-C's larger population, for reconciliation")
    a = ap.parse_args()

    proj = Path(a.project).resolve()
    rk = ruler_mod.resolve_ruler(proj)
    print("== ruler ==")
    print(rk.banner())

    rep = json.load(open(proj / "build/45410914/report.json"))
    m = rep["measures"]
    print(f"== baseline ==  total_code={I(m.get('total_code'))} "
          f"matched_code={I(m.get('matched_code'))} "
          f"code%={F(m.get('matched_code_percent')):.6f} "
          f"total_functions={I(m.get('total_functions'))}")

    want = {}
    for u in rep["units"]:
        for f in (u.get("functions") or []):
            fz = F(f.get("fuzzy_match_percent"))
            nm = f.get("name", "")
            if fz >= 100.0 or fz <= 0.0:
                continue
            if not a.include_placeholder and nm.startswith(
                    ("fn_", "lbl_", "jumptable_", "data_", "bss_", "rdata_")):
                continue
            want[(u["name"], nm)] = (I(f.get("size")), fz,
                                     F(f.get("match_percent_normalized")))
    print(f"== population ==  named partial rows: {len(want)} "
          f"({sum(v[0] for v in want.values())} B)")
    if not want:
        print("REFUSE: empty population -- an empty population passes every "
              "check by construction.", file=sys.stderr)
        return 3

    byunit = collections.defaultdict(list)
    for (un, sym) in want:
        byunit[un].append(sym)
    units = sorted(byunit)
    if a.limit_units:
        units = units[:a.limit_units]

    cli = str(proj / "bin/objdiff-cli")
    tasks = [(proj, cli, rk.args, un, byunit[un]) for un in units]
    rows = []
    failed = []
    done = 0
    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        for uname, recs, err in ex.map(run_unit, tasks):
            done += 1
            if done % 100 == 0:
                print(f"  ... {done}/{len(tasks)} units", file=sys.stderr)
            if recs is None:
                failed.append((uname, err))
                continue
            for r in recs:
                sym = r.get("symbol") or r.get("name") or ""
                key = (uname, sym)
                if key not in want or r.get("error"):
                    continue
                sz, fz, mpn = want[key]
                p = profile(r)
                rows.append(dict(unit=uname, sym=sym, size=sz, fuzzy=fz,
                                 mpn=mpn, **{k: v for k, v in p.items()
                                             if k != "pairs"},
                                 pairs=[[list(k), v]
                                        for k, v in p["pairs"].items()]))

    got = {(r["unit"], r["sym"]) for r in rows}
    missing = [(v[0], k) for k, v in want.items() if k not in got]
    print(f"== coverage ==  profiled {len(rows)} rows / "
          f"{sum(r['size'] for r in rows)} B; "
          f"NOT profiled {len(missing)} rows / {sum(x[0] for x in missing)} B; "
          f"unit failures {len(failed)}")
    for un, err in failed[:5]:
        print(f"   FAIL {un}: {err}")

    # ---- the partition ----
    def cls(r):
        other = r["hard"] + r["reg"] + r["imm"] + r["br"]
        if r["name"] > 0 and other == 0:
            return "E_pure_name"
        if r["name"] > 0:
            return "F_mixed_name"
        return "other_no_name"

    agg = collections.defaultdict(lambda: [0, 0])
    for r in rows:
        c = cls(r)
        agg[c][0] += 1
        agg[c][1] += r["size"]
    print("\n== charge partition of named partial rows ==")
    print(f"{'class':<16} {'rows':>7} {'bytes':>12}")
    for c in ("E_pure_name", "F_mixed_name", "other_no_name"):
        print(f"{c:<16} {agg[c][0]:>7} {agg[c][1]:>12}")
    tot_r = sum(v[0] for v in agg.values())
    tot_b = sum(v[1] for v in agg.values())
    print(f"{'TOTAL':<16} {tot_r:>7} {tot_b:>12}")
    assert tot_r == len(rows), "partition dropped rows"

    epairs = collections.Counter()
    for r in rows:
        if cls(r) != "E_pure_name":
            continue
        for (pair, n) in r["pairs"]:
            epairs[tuple(pair)] += n
    print(f"\nclass E distinct (target,base) charged pairs: {len(epairs)}")
    print(f"class E charged instances: {sum(epairs.values())}")

    if a.out:
        Path(a.out).write_text(json.dumps(
            dict(rows=rows, failed=failed,
                 missing=[[x[0], list(x[1])] for x in missing],
                 baseline=m), indent=0))
        print(f"\nwrote {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
