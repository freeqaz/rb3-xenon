#!/usr/bin/env python3
"""Classify the 90.0-99.99% near-miss pool by CODEGEN root cause.

Body-divergence wall #2 diagnostic. For every function report.json scores in
[90, 99.99), run `objdiff-cli diff --batch --analyze --verdict` (the freeqaz
objdiff fork's pattern detector) and bucket the *residual* (noise-stripped)
mismatch by its dominant codegen pattern + the fork's own reachability label.

The fork already labels each pattern with a Fixability and splits REGISTER_SWAP
into volatile (scheduling-driven, RarelyHandFixable) vs callee-saved
(declaration-order driven, MaybeFixable). We aggregate per-function: strip the
source-immune NOISE patterns (LINKER_MERGED = ICF, ADDRESS_RELOCATION_NOISE =
.text layout) that report.json already discounts, then assign a PRIMARY codegen
class from what remains.

Outputs:
  - /tmp/claude/nearmiss_inventory.jsonl  (one record per function, cached raw)
  - stdout summary table (counts per class + reachability)

Read-only: diffs already-built objs via objdiff.json paths; no ninja, no writes
to the repo. Safe to run against the shared main tree.

Usage:
  tools/classify_nearmiss_codegen.py                 # full run (uses cache)
  tools/classify_nearmiss_codegen.py --refresh       # ignore cache, re-diff
  tools/classify_nearmiss_codegen.py --jobs 8
  tools/classify_nearmiss_codegen.py --lo 90 --hi 99.99
"""
import argparse
import json
import os
import subprocess
import sys
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OBJDIFF_CLI = os.path.join(ROOT, "bin", "objdiff-cli")
REPORT = os.path.join(ROOT, "build", "45410914", "report.json")
CACHE = "/tmp/claude/nearmiss_inventory.jsonl"

# Source-immune: report.json (with icf_aliases.map) already discounts these.
NOISE = {"LINKER_MERGED", "ADDRESS_RELOCATION_NOISE"}
# Build-environment / naming: addressed by the wired COFF obj patchers.
BUILDENV = {
    "ANONYMOUS_NAMESPACE_HASH", "STATIC_GUARD_COUNTER", "SCOPE_COUNTER_MISMATCH",
    "DYNAMIC_CAST_MISMATCH", "MAKESTRING_TEMPLATE_MISMATCH", "PROLOGUE_MISMATCH",
    "ALLOCA_MISMATCH",
}


def is_fpr(reg):
    return reg.startswith("f") and reg[1:].split(".")[0].isdigit()


def is_gpr(reg):
    return reg.startswith("r") and reg[1:].isdigit()


def load_nearmiss(lo, hi):
    d = json.load(open(REPORT))
    out = []
    for u in d["units"]:
        un = u.get("name")
        for f in (u.get("functions") or []):
            v = f["match_percent_normalized"]
            if lo <= v < hi:
                out.append({"unit": un, "name": f["name"],
                            "report_pct": v, "size": int(f["size"])})
    return out


def run_unit_batch(unit, symbols):
    inp = "\n".join(symbols) + "\n"
    try:
        p = subprocess.run(
            [OBJDIFF_CLI, "diff", "-p", ".", "-u", unit, "--batch",
             "--analyze", "--verdict", "-f", "json"],
            input=inp, capture_output=True, text=True, cwd=ROOT, timeout=600)
    except subprocess.TimeoutExpired:
        return {}
    res = {}
    for line in p.stdout.splitlines():
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            j = json.loads(line)
        except json.JSONDecodeError:
            continue
        if "symbol" in j:
            res[j["symbol"]] = j
    return res


def classify(rec, raw):
    out = dict(rec)
    if raw is None:
        out["primary"] = "NO_DIFF_DATA"
        out["reach"] = "unknown"
        return out

    isum = raw.get("instruction_summary", {})
    total_mismatch = sum(isum.get(k, 0) for k in
                         ("diff_arg", "diff_op", "replace", "insert", "delete"))
    unattr = raw.get("analysis", {}).get("unattributed_mismatches", 0)
    pats = raw.get("analysis", {}).get("patterns", [])
    out["objdiff_pct"] = raw.get("normalized_match_percent")
    out["total_mismatch"] = total_mismatch
    out["unattributed"] = unattr
    out["verdict"] = raw.get("verdict", {}).get("classification")

    noise_ct = buildenv_ct = codegen_ct = 0
    codegen_pats = []
    for p in pats:
        name = p["pattern"]
        ct = p.get("instruction_count", 0)
        if name in NOISE:
            noise_ct += ct
        elif name in BUILDENV:
            buildenv_ct += ct
        else:
            codegen_ct += ct
            codegen_pats.append((ct, name, p.get("fixability"), p.get("details", {})))
    out["noise_ct"] = noise_ct
    out["buildenv_ct"] = buildenv_ct
    out["codegen_ct"] = codegen_ct
    out["patterns"] = [p["pattern"] for p in pats]

    if codegen_ct == 0:
        if unattr > 0:
            out["primary"], out["reach"] = "UNATTRIBUTED", "unknown"
        elif buildenv_ct > 0:
            out["primary"], out["reach"] = "BUILD_ENV", "patcher"
        elif noise_ct > 0:
            out["primary"], out["reach"] = "NOISE_ONLY", "source_immune"
        else:
            out["primary"], out["reach"] = "CLEAN_OR_TINY", "source_immune"
        return out

    codegen_pats.sort(reverse=True)
    ct, name, fix, details = codegen_pats[0]
    out["dom_pattern"] = name
    out["dom_fixability"] = fix

    if name == "REGISTER_SWAP":
        swaps = details.get("swaps", []) if isinstance(details, dict) else []
        regs = []
        for s in swaps:
            regs += [s.get("target_reg", ""), s.get("base_reg", "")]
        fpr = any(is_fpr(r) for r in regs)
        gpr = any(is_gpr(r) for r in regs)
        kind = "FPR" if (fpr and not gpr) else ("GPR" if (gpr and not fpr) else "MIXED")
        sub = "CALLEE" if fix == "maybe_fixable" else "VOLATILE"
        out["primary"] = f"REGALLOC_{kind}_{sub}"
        out["swaps"] = swaps
        out["reach"] = "permuter_decl" if sub == "CALLEE" else "scheduling"
    elif name == "OFFSET_SWAP":
        out["primary"], out["reach"] = "STRUCT_OFFSET", "header_lever"
    elif name == "COMMUTATIVE_OP_ORDER":
        out["primary"], out["reach"] = "COMMUTATIVE", "permuter_commute"
    elif name == "COMPARISON_STYLE":
        out["primary"], out["reach"] = "INSTR_SELECT_CMP", "instr_select"
    elif name == "BOOLEAN_NEGATION":
        out["primary"], out["reach"] = "INSTR_SELECT_BOOL", "instr_select"
    elif name in ("FSEL_TERNARY", "FLOAT_PRECISION_MISMATCH", "FLOAT_TO_INT_TO_FLOAT"):
        out["primary"], out["reach"] = "FLOAT_SELECT", "float_source"
    elif name == "CONTROL_FLOW":
        out["primary"], out["reach"] = "CONTROL_FLOW", "scheduling"
    elif name == "DEAD_STORE_ELIMINATION":
        out["primary"], out["reach"] = "DEAD_STORE", "scheduling"
    elif name == "BOOL_MASK":
        out["primary"], out["reach"] = "BOOL_MASK", "permuter_bool"
    else:
        out["primary"], out["reach"] = f"OTHER_{name}", "unknown"
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lo", type=float, default=90.0)
    ap.add_argument("--hi", type=float, default=99.99)
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--refresh", action="store_true")
    ap.add_argument("--cache", default=CACHE)
    args = ap.parse_args()

    os.makedirs(os.path.dirname(args.cache), exist_ok=True)
    nm = load_nearmiss(args.lo, args.hi)
    print(f"near-miss pool [{args.lo},{args.hi}): {len(nm)} functions", file=sys.stderr)

    cached = {}
    if not args.refresh and os.path.exists(args.cache):
        for line in open(args.cache):
            try:
                r = json.loads(line)
                cached[(r["unit"], r["name"])] = r
            except Exception:
                pass
        print(f"cache hit: {len(cached)} prior records", file=sys.stderr)

    todo = [r for r in nm if (r["unit"], r["name"]) not in cached]
    by_unit = defaultdict(list)
    for r in todo:
        by_unit[r["unit"]].append(r)
    print(f"to diff: {len(todo)} fns across {len(by_unit)} units", file=sys.stderr)

    results = dict(cached)

    def work(unit):
        syms = [r["name"] for r in by_unit[unit]]
        raws = run_unit_batch(unit, syms)
        recs = {}
        for r in by_unit[unit]:
            recs[(r["unit"], r["name"])] = classify(r, raws.get(r["name"]))
        return recs

    done = 0
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for recs in ex.map(work, list(by_unit.keys())):
            results.update(recs)
            done += len(recs)
            print(f"\r  diffed {done}/{len(todo)}", end="", file=sys.stderr)
    print("", file=sys.stderr)

    with open(args.cache, "w") as fh:
        for r in results.values():
            fh.write(json.dumps(r) + "\n")

    recs = [results[(r["unit"], r["name"])] for r in nm]
    summarize(recs)


def summarize(recs):
    n = len(recs)
    tiny40 = [r for r in recs if r["size"] == 40]
    real = [r for r in recs if r["size"] != 40]
    print("\n" + "=" * 78)
    print(f"NEAR-MISS CODEGEN INVENTORY  (total {n}; size==40 artifacts {len(tiny40)}; real-bodied {len(real)})")
    print("=" * 78)

    def table(rows, title):
        print(f"\n## {title} ({len(rows)} fns)")
        cls = Counter(r.get("primary", "?") for r in rows)
        reach = defaultdict(lambda: Counter())
        named = Counter()
        for r in rows:
            reach[r.get("primary", "?")][r.get("reach", "?")] += 1
            if r["name"].startswith("?"):
                named[r.get("primary", "?")] += 1
        print(f"  {'CLASS':<26}{'count':>6}{'named':>7}  reachability")
        for c, ct in cls.most_common():
            rch = ",".join(f"{k}:{v}" for k, v in reach[c].items())
            print(f"  {c:<26}{ct:>6}{named[c]:>7}  {rch}")

    table(real, "REAL-BODIED near-misses (size != 40)")
    table(tiny40, "SIZE-40 funclet/stub artifacts")

    print("\n## REACHABILITY ROLLUP (real-bodied only)")
    rr = Counter(r.get("reach", "?") for r in real)
    for k, v in rr.most_common():
        print(f"  {k:<18}{v:>6}")

    named_real = [r for r in real if r["name"].startswith("?")]
    print(f"\n## NAMED real-bodied near-misses: {len(named_real)} (have oracle source)")
    nc = Counter(r.get("primary", "?") for r in named_real)
    for c, ct in nc.most_common():
        print(f"  {c:<26}{ct:>6}")


if __name__ == "__main__":
    main()
