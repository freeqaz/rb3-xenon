#!/usr/bin/env python3
"""Produce a RANKED permuter-target list from the near-miss cause-class JSONs
(tools/classify_nearmiss.py output) — bucketed by true permuter-viability.

Viability buckets (per-function, from mismatch-instruction COUNTS not 'dominant'):
  PRIME  : codegen-only (REG/OPCODE/OTHER, NO struct-OFFSET, NO WRONG_PAIR).
           The permuter can shape regalloc/codegen here. Run these first.
  MIXED  : codegen + struct-OFFSET. Permuter MIGHT help after the layout bug is
           fixed; lower priority — do NOT throw the whole sweep at these.
  LAYOUT : OFFSET/WRONG_PAIR only -> base-class-layout wall. NOT permuter-class
           (needs struct edits). SKIP per project memory.
Only NAMED (mangled '?') functions are emitted — anon fn_ near-misses are
target-only pins with no source to permute.

Usage:
  tools/permuter_targets.py --in /home/free/tmp/permuter-scaled/nearmiss_90_100.json \
                            --in /home/free/tmp/permuter-scaled/nearmiss_50_90.json \
                            --out /home/free/tmp/permuter-scaled/permuter_targets_ranked.txt
"""
import argparse, json, sys


def viability(counts):
    off = counts.get("OFFSET", 0) + counts.get("WRONG_PAIR", 0)
    cg = counts.get("REG", 0) + counts.get("OPCODE", 0) + counts.get("OTHER", 0)
    nm = counts.get("NAME_RELOC", 0)
    if off == 0 and cg > 0:
        return "PRIME"
    if off > 0 and cg > 0:
        return "MIXED"
    if off > 0:
        return "LAYOUT"
    if nm > 0:
        return "NAME_ONLY"
    return "CLEAN"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="inputs", action="append", required=True)
    ap.add_argument("--out", default="/home/free/tmp/permuter-scaled/permuter_targets_ranked.txt")
    a = ap.parse_args()

    rows = []
    for fn in a.inputs:
        for x in json.load(open(fn)):
            if not x["sym"].startswith("?"):
                continue
            v = viability(x.get("counts", {}))
            rows.append({**x, "viability": v})

    buckets = {}
    for x in rows:
        buckets.setdefault(x["viability"], []).append(x)

    # Rank PRIME: pure REG/OPCODE small high-% first (cleanest flips).
    def keyf(x):
        c = x["counts"]
        pure = (c.get("REG", 0) + c.get("OPCODE", 0) + c.get("OTHER", 0)) and not c.get("NAME_RELOC", 0)
        return (0 if pure else 1, x["size"], -x["mp"])

    for k in buckets:
        buckets[k].sort(key=keyf)

    with open(a.out, "w") as f:
        f.write("# RANKED PERMUTER TARGETS (named near-misses)\n")
        f.write("# bucket counts: " + ", ".join(f"{k}={len(v)}" for k, v in sorted(buckets.items())) + "\n#\n")
        for bucket in ("PRIME", "MIXED", "NAME_ONLY", "LAYOUT", "CLEAN"):
            lst = buckets.get(bucket, [])
            f.write(f"\n## {bucket}  ({len(lst)})\n")
            if bucket in ("LAYOUT",):
                f.write("# SKIP for permuter (struct-layout class — needs header edits)\n")
            for x in lst:
                f.write(f"{x['sym']}\t{x['mp']:.2f}\t{x['size']}\t{x['unit']}\t{x['counts']}\n")

    # stderr summary
    print("=== permuter-target buckets (named) ===", file=sys.stderr)
    for k in ("PRIME", "MIXED", "NAME_ONLY", "LAYOUT", "CLEAN"):
        print(f"  {k:10s} {len(buckets.get(k,[]))}", file=sys.stderr)
    print(f"wrote {a.out}", file=sys.stderr)
    # Also emit a plain PRIME symbol list (one per line) for batch --symbol feeding
    prime_out = a.out.replace(".txt", "_PRIME_syms.txt")
    with open(prime_out, "w") as f:
        for x in buckets.get("PRIME", []):
            f.write(x["sym"] + "\n")
    print(f"wrote {prime_out} ({len(buckets.get('PRIME',[]))} PRIME symbols)", file=sys.stderr)


if __name__ == "__main__":
    main()
