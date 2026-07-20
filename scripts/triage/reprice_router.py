#!/usr/bin/env python3
"""reprice_router.py — measured per-bucket flip-rates for the triage router.

Joins real grind-attempt outcomes from decomp.db against the triage bucket
labels in triage_results.json to compute MEASURED per-bucket flip-rates,
replacing hand-estimated priors.

Read-only. No state, idempotent. Regenerable via:
    venv/bin/python scripts/triage/reprice_router.py

A "flip" (function-level success) = a function has ANY attempt with
exit_status='complete' OR end_percent >= 100. Aggregation is at the FUNCTION
level: a function with 3 failed attempts then 1 complete is ONE attempted
function that flipped.

Outputs:
  scripts/triage/measured_priors.json   (machine-readable)
  docs/plans/router-measured-priors.md  (human-readable, also to stdout)
"""
import argparse
import datetime
import json
import math
import os
import sqlite3
import sys
from collections import defaultdict

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_DB = os.path.join(REPO_ROOT, "decomp.db")
DEFAULT_TRIAGE = os.path.expanduser("~/tmp/triage_results.json")
DEFAULT_OUT_JSON = os.path.join(REPO_ROOT, "scripts", "triage", "measured_priors.json")
DEFAULT_OUT_MD = os.path.join(REPO_ROOT, "docs", "plans", "router-measured-priors.md")

TRUST_THRESHOLD = 8  # n_attempted >= this => trusted measurement
Z = 1.96  # Wilson score interval (95%)


def wilson_low(k, n, z=Z):
    """Standard Wilson score interval lower bound."""
    if n == 0:
        return 0.0
    p = k / n
    denom = 1.0 + z * z / n
    center = p + z * z / (2.0 * n)
    margin = z * math.sqrt(p * (1.0 - p) / n + z * z / (4.0 * n * n))
    return max(0.0, (center - margin) / denom)


def load_triage(path):
    """Return (labels, has_stratum). labels: (unit,name) -> {bucket, stratum}."""
    with open(path) as f:
        data = json.load(f)
    labels = {}
    has_stratum = False
    for r in data["functions"]:
        key = (r["unit"], r["name"])
        stratum = r.get("stratum")
        if stratum is not None:
            has_stratum = True
        labels[key] = {"bucket": r.get("bucket"), "stratum": stratum}
    return labels, has_stratum


def aggregate_functions(db_path):
    """Aggregate attempts to the function level.

    Returns dict: fid -> {
        unit, symbol, n_attempts, flipped(bool),
        out_tokens (int|None), cost (float|None)
    }
    plus total_attempts count.
    """
    con = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True)
    cur = con.cursor()
    rows = cur.execute(
        """
        SELECT a.function_id, f.unit, f.symbol,
               a.exit_status, a.end_percent,
               a.output_tokens, a.actual_cost_usd
        FROM attempts a
        JOIN functions f ON a.function_id = f.id
        """
    ).fetchall()
    total_attempts = len(rows)

    per = {}
    for fid, unit, symbol, status, end_pct, out_tok, cost in rows:
        f = per.get(fid)
        if f is None:
            f = {
                "unit": unit,
                "symbol": symbol,
                "n_attempts": 0,
                "flipped": False,
                "out_tokens": None,
                "cost": None,
            }
            per[fid] = f
        f["n_attempts"] += 1
        if status == "complete" or (end_pct is not None and end_pct >= 100):
            f["flipped"] = True
        if out_tok is not None:
            f["out_tokens"] = (f["out_tokens"] or 0) + out_tok
        if cost is not None:
            f["cost"] = (f["cost"] or 0.0) + cost
    con.close()
    return per, total_attempts


def summarize(group):
    """group: list of per-function dicts -> summary metrics."""
    n_attempted = len(group)
    n_flipped = sum(1 for f in group if f["flipped"])
    n_attempts_total = sum(f["n_attempts"] for f in group)
    rate = (n_flipped / n_attempted) if n_attempted else 0.0
    wl = wilson_low(n_flipped, n_attempted)

    # tokens / cost per flip — computed only from flipped functions that carry data
    flipped = [f for f in group if f["flipped"]]
    tok_vals = [f["out_tokens"] for f in flipped if f["out_tokens"] is not None]
    cost_vals = [f["cost"] for f in flipped if f["cost"] is not None]
    mean_tok = (sum(tok_vals) / len(tok_vals)) if tok_vals else None
    mean_cost = (sum(cost_vals) / len(cost_vals)) if cost_vals else None

    return {
        "rate": round(rate, 4),
        "wilson_low": round(wl, 4),
        "n_attempted": n_attempted,
        "n_flipped": n_flipped,
        "n_attempts_total": n_attempts_total,
        "mean_output_tokens_per_flip": (round(mean_tok, 1) if mean_tok is not None else None),
        "mean_cost_usd_per_flip": (round(mean_cost, 6) if mean_cost is not None else None),
        "trusted": n_attempted >= TRUST_THRESHOLD,
    }


def build_md(buckets, overall, all_triage_buckets, join_diag, has_stratum,
             strata_table, source_attempts, ts):
    lines = []
    lines.append("# Router measured priors")
    lines.append("")
    lines.append(f"_Auto-generated by `scripts/triage/reprice_router.py` at {ts}._")
    lines.append("_Do not edit by hand — re-run the script to regenerate._")
    lines.append("")
    lines.append(
        f"Measured per-bucket flip-rates joining {source_attempts} grind attempts "
        f"(function-level) against triage bucket labels. A *flip* = a function with "
        f"any `complete` attempt or `end_percent >= 100`. `trusted` = "
        f"n_attempted >= {TRUST_THRESHOLD}."
    )
    lines.append("")
    lines.append("## Join diagnostics")
    lines.append("")
    for k, v in join_diag.items():
        lines.append(f"- {k}: {v}")
    lines.append("")
    lines.append("## Measured flip-rates (sorted by n_attempted desc)")
    lines.append("")
    lines.append("| bucket | n_attempted | n_flipped | rate | wilson_low | tok/flip | trusted |")
    lines.append("|---|---:|---:|---:|---:|---:|:---:|")

    def fmt_row(name, s):
        tok = "-" if s["mean_output_tokens_per_flip"] is None else f"{s['mean_output_tokens_per_flip']:.0f}"
        trust = "Y" if s["trusted"] else "n"
        return (f"| {name} | {s['n_attempted']} | {s['n_flipped']} | "
                f"{s['rate']*100:.1f}% | {s['wilson_low']*100:.1f}% | {tok} | {trust} |")

    lines.append(fmt_row("OVERALL", overall))
    for name, s in sorted(buckets.items(), key=lambda kv: -kv[1]["n_attempted"]):
        lines.append(fmt_row(name, s))
    lines.append("")

    # zero-attempt buckets (estimate-only)
    measured_names = set(buckets)
    zero = sorted(b for b in all_triage_buckets if b not in measured_names)
    lines.append("## Estimate-only buckets (zero grind attempts, no ground truth yet)")
    lines.append("")
    if zero:
        for b in zero:
            lines.append(f"- {b} ({all_triage_buckets[b]} labeled functions in pool)")
    else:
        lines.append("_None — every triage bucket has at least one attempted function._")
    lines.append("")

    if has_stratum and strata_table:
        lines.append("## Secondary breakdown: (bucket, stratum)")
        lines.append("")
        lines.append("| bucket | stratum | n_attempted | n_flipped | rate | trusted |")
        lines.append("|---|---|---:|---:|---:|:---:|")
        for (bucket, stratum), s in sorted(
            strata_table.items(), key=lambda kv: (-kv[1]["n_attempted"], kv[0])
        ):
            trust = "Y" if s["trusted"] else "n"
            lines.append(
                f"| {bucket} | {stratum} | {s['n_attempted']} | {s['n_flipped']} | "
                f"{s['rate']*100:.1f}% | {trust} |"
            )
        lines.append("")

    return "\n".join(lines) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--db", default=DEFAULT_DB)
    ap.add_argument("--triage", default=DEFAULT_TRIAGE)
    ap.add_argument("--out-json", default=DEFAULT_OUT_JSON)
    ap.add_argument("--out-md", default=DEFAULT_OUT_MD)
    args = ap.parse_args(argv)

    labels, has_stratum = load_triage(args.triage)
    per_func, total_attempts = aggregate_functions(args.db)

    # bucket populations in the triage pool (for estimate-only reporting)
    all_triage_buckets = defaultdict(int)
    for v in labels.values():
        all_triage_buckets[v["bucket"]] += 1

    # join attempted functions -> bucket
    bucket_groups = defaultdict(list)
    strata_groups = defaultdict(list)
    n_with_bucket = 0
    for fid, f in per_func.items():
        lab = labels.get((f["unit"], f["symbol"]))
        if lab is None:
            continue
        n_with_bucket += 1
        bucket_groups[lab["bucket"]].append(f)
        if lab["stratum"] is not None:
            strata_groups[(lab["bucket"], lab["stratum"])].append(f)

    buckets = {name: summarize(g) for name, g in bucket_groups.items()}
    overall = summarize(list(per_func.values()))
    strata_table = {k: summarize(g) for k, g in strata_groups.items()} if has_stratum else {}

    total_attempted_funcs = len(per_func)
    n_bucketed_attempted = sum(s["n_attempted"] for s in buckets.values())
    join_diag = {
        "triage_functions_total": len(labels),
        "attempted_functions_total": total_attempted_funcs,
        "attempted_functions_with_triage_bucket": n_with_bucket,
        "attempted_functions_without_bucket": total_attempted_funcs - n_with_bucket,
        "join_rate_pct": round(100.0 * n_with_bucket / total_attempted_funcs, 1) if total_attempted_funcs else 0.0,
        "sum_bucket_n_attempted": n_bucketed_attempted,
        "total_grind_attempts": total_attempts,
    }

    ts = datetime.datetime.now().astimezone().replace(microsecond=0).isoformat()

    out = {
        "generated": ts,
        "source_attempts": total_attempts,
        "join_diagnostics": join_diag,
        "buckets": dict(buckets),
    }
    out["buckets"]["OVERALL"] = overall

    os.makedirs(os.path.dirname(args.out_json), exist_ok=True)
    with open(args.out_json, "w") as f:
        json.dump(out, f, indent=2)
        f.write("\n")

    md = build_md(buckets, overall, dict(all_triage_buckets), join_diag,
                  has_stratum, strata_table, total_attempts, ts)
    os.makedirs(os.path.dirname(args.out_md), exist_ok=True)
    with open(args.out_md, "w") as f:
        f.write(md)

    sys.stdout.write(md)
    sys.stderr.write(
        f"\n[wrote {args.out_json}]\n[wrote {args.out_md}]\n"
    )


if __name__ == "__main__":
    main()
