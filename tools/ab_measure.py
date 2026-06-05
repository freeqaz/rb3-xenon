#!/usr/bin/env python3
"""A/B measure a worktree against a baseline report.json.

The canonical "did my struct/body edit help?" check. Optionally (re)builds the
worktree, then diffs its build/45410914/report.json against a frozen baseline,
reporting the net matched_functions delta plus per-unit regressions and
improvements. Coupled-base edits MUST be judged on the whole-family net (this
surfaces silent sibling regressions); body-port edits on the target unit.

Usage:
  # freeze a baseline first (copy main's report.json somewhere stable):
  cp build/45410914/report.json ~/tmp/grind/baseline.report.json

  # after editing source in a worktree:
  python3 tools/ab_measure.py --worktree ~/tmp/wt-charcrowd \
      --baseline ~/tmp/grind/baseline.report.json --build

Flags:
  --worktree DIR   worktree to measure (default: cwd)
  --baseline FILE  frozen baseline report.json to diff against (required)
  --build          run ./tools/ninja-locked in the worktree first (incremental)
  --resplit        before building, reset the target-symbol-renamer stamp + touch
                   config.yml (only needed when splits.txt changed; NOT for pure
                   source/header edits)
  --json           machine-readable output
"""
import json, os, subprocess, sys, argparse

def measures(report_path):
    r = json.load(open(report_path))
    return r["measures"]["matched_functions"], {
        u["name"]: u["measures"].get("matched_functions", 0) for u in r["units"]
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", default=os.getcwd())
    ap.add_argument("--baseline", required=True)
    ap.add_argument("--build", action="store_true")
    ap.add_argument("--resplit", action="store_true")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()

    wt = os.path.abspath(a.worktree)
    rep = os.path.join(wt, "build/45410914/report.json")

    build_exit = None
    if a.build:
        if a.resplit:
            subprocess.run(["rm", "-f", "build/45410914/target_symbol_renames.stamp"],
                           cwd=wt)
            subprocess.run(["touch", "config/45410914/config.yml"], cwd=wt)
        p = subprocess.run(["./tools/ninja-locked"], cwd=wt,
                           capture_output=True, text=True)
        build_exit = p.returncode
        if build_exit != 0:
            tail = (p.stdout + p.stderr).splitlines()[-25:]
            out = {"build_exit": build_exit, "error": "BUILD FAILED",
                   "tail": tail}
            print(json.dumps(out, indent=1) if a.json else
                  "BUILD FAILED (exit %d):\n%s" % (build_exit, "\n".join(tail)))
            sys.exit(2)

    base_tot, base_u = measures(a.baseline)
    cand_tot, cand_u = measures(rep)
    regs, imps = [], []
    for u in sorted(set(base_u) | set(cand_u)):
        d = cand_u.get(u, 0) - base_u.get(u, 0)
        if d < 0: regs.append((d, u, base_u.get(u, 0), cand_u.get(u, 0)))
        elif d > 0: imps.append((d, u, base_u.get(u, 0), cand_u.get(u, 0)))
    regs.sort(); imps.sort(reverse=True)

    out = {
        "build_exit": build_exit,
        "baseline_matched": base_tot, "candidate_matched": cand_tot,
        "net_delta": cand_tot - base_tot,
        "n_regressed_units": len(regs), "n_improved_units": len(imps),
        "regressions": [{"unit": u, "delta": d, "from": b, "to": c} for d, u, b, c in regs],
        "improvements": [{"unit": u, "delta": d, "from": b, "to": c} for d, u, b, c in imps],
    }
    if a.json:
        print(json.dumps(out, indent=1)); return
    print(f"baseline {base_tot} -> candidate {cand_tot}  NET {cand_tot-base_tot:+d}"
          f"  ({len(imps)} units up, {len(regs)} units down)")
    if imps:
        print("  improvements:")
        for d, u, b, c in imps: print(f"    {d:+3d}  {u}  ({b}->{c})")
    if regs:
        print("  REGRESSIONS:")
        for d, u, b, c in regs: print(f"    {d:+3d}  {u}  ({b}->{c})")

if __name__ == "__main__":
    main()
