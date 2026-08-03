#!/usr/bin/env python3
"""lane EB-4: settle `objdiff-cli diff` vs `report generate` disagreement.

For each named sub-100 row in the report, run `objdiff-cli diff` and compare
every diff-output percent field against every report percent field.

Includes two SABOTAGE NULLS to prove the comparator can fail:
  --null permute : pair row i's report values with row (i+1)'s diff values
  --leg mcp      : only functionRelocDiffs=none (what the MCP tool passes)
  --leg default  : no -c at all (objdiff-cli diff's own default = data_value)
"""
import json, subprocess, sys, argparse, concurrent.futures as cf

WT = "/home/free/tmp/laneEB4/wt"
CLI = "/home/free/code/milohax/objdiff/target/release/objdiff-cli"

LEGS = {
    # replicate report generate's config exactly
    "full": ["-c", "functionRelocDiffs=none",
             "-c", "combineDataSections=true",
             "-c", "combineTextSections=true",
             "-c", "ppc.calculatePoolRelocations=false"],
    # what scripts/orchestrator/mcp_server.py actually passes
    "mcp": ["-c", "functionRelocDiffs=none"],
    # objdiff-cli diff's own hardcoded default (FunctionRelocDiffs::DataValue)
    "default": [],
    # isolation legs: mcp + exactly ONE of the three residual report-side fields
    "mcp_pool": ["-c", "functionRelocDiffs=none",
                 "-c", "ppc.calculatePoolRelocations=false"],
    "mcp_ctext": ["-c", "functionRelocDiffs=none",
                  "-c", "combineTextSections=true"],
    "mcp_cdata": ["-c", "functionRelocDiffs=none",
                  "-c", "combineDataSections=true"],
}


def run_one(args):
    unit, sym, leg = args
    cmd = [CLI, "diff", "-p", WT, "-u", unit, sym] + LEGS[leg] + ["-f", "json"]
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=120, cwd=WT)
        if p.returncode != 0 or "{" not in p.stdout:
            return (unit, sym, None, (p.stderr or "")[-200:])
        d = json.loads(p.stdout)
        return (unit, sym, {
            "fuzzy": d.get("fuzzy_match_percent"),
            "norm": d.get("normalized_match_percent"),
            "raw": d.get("raw_match_percent"),
            "tsize": d.get("target_size"),
            "bsize": d.get("base_size"),
        }, None)
    except Exception as e:
        return (unit, sym, None, repr(e)[:200])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--leg", default="full", choices=list(LEGS))
    ap.add_argument("--null", default="none", choices=["none", "permute"])
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--out", default=None)
    ap.add_argument("--rows", default="/home/free/tmp/laneEB4/eb4_sub100_named.json")
    a = ap.parse_args()

    rows = json.load(open(a.rows))
    if a.limit:
        rows = rows[: a.limit]
    tasks = [(r[0], r[1], a.leg) for r in rows]

    with cf.ThreadPoolExecutor(max_workers=16) as ex:
        res = list(ex.map(run_one, tasks))

    got = {(u, s): v for (u, s, v, e) in res if v is not None}
    errs = [(u, s, e) for (u, s, v, e) in res if v is None]

    # ---- pairing (with optional sabotage) ----
    diff_side = []
    for i, r in enumerate(rows):
        key = (rows[(i + 1) % len(rows)][0], rows[(i + 1) % len(rows)][1]) \
            if a.null == "permute" else (r[0], r[1])
        diff_side.append(got.get(key))

    n = 0
    agree_fuzzy = agree_mpn = 0
    dis_fuzzy = []
    dis_mpn = []
    for r, d in zip(rows, diff_side):
        if d is None or d["norm"] is None:
            continue
        n += 1
        rep_fuzzy, rep_mpn = r[3], r[4]
        gf = abs(d["norm"] - rep_fuzzy)
        gm = abs(d["norm"] - rep_mpn)
        if gf < 1e-4:
            agree_fuzzy += 1
        else:
            dis_fuzzy.append((r[0], r[1], r[2], rep_fuzzy, d["norm"], d["raw"], gf))
        if gm < 1e-4:
            agree_mpn += 1
        else:
            dis_mpn.append((r[0], r[1], r[2], rep_mpn, d["norm"], gm))

    print(f"leg={a.leg} null={a.null}  compared={n}  errors={len(errs)}")
    print(f"  diff.normalized VS report.fuzzy : agree {agree_fuzzy}/{n} "
          f"({100*agree_fuzzy/max(n,1):.2f}%)  DISAGREE {len(dis_fuzzy)} "
          f"({100*len(dis_fuzzy)/max(n,1):.2f}%)")
    print(f"  diff.normalized VS report.mpn   : agree {agree_mpn}/{n} "
          f"({100*agree_mpn/max(n,1):.2f}%)  DISAGREE {len(dis_mpn)} "
          f"({100*len(dis_mpn)/max(n,1):.2f}%)")
    if dis_mpn:
        signs = sum(1 for x in dis_mpn if x[3] > x[4])
        print(f"    of the mpn disagreements, report.mpn > diff.norm in {signs}/{len(dis_mpn)}"
              f"  max gap {max(x[5] for x in dis_mpn):.4f} pp")
    if dis_fuzzy:
        print(f"    max fuzzy gap {max(x[6] for x in dis_fuzzy):.6f} pp")
        for x in sorted(dis_fuzzy, key=lambda y: -y[6])[:15]:
            print(f"      {x[0]:32.32} {x[1][:60]:60.60} sz={x[2]:6d} "
                  f"rep_fuzzy={x[3]:10.5f} diff_norm={x[4]:10.5f} diff_raw={x[5]} gap={x[6]:.5f}")
    if errs:
        print(f"  first errors: {errs[:3]}")
    if a.out:
        json.dump({"dis_fuzzy": dis_fuzzy, "dis_mpn": dis_mpn,
                   "n": n, "errs": errs}, open(a.out, "w"))


if __name__ == "__main__":
    main()
