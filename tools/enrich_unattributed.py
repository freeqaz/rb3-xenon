#!/usr/bin/env python3
"""Sub-classify the UNATTRIBUTED near-miss bucket by instruction-level signature.

The fork's pattern detector finds no recognized pattern for ~560 real-bodied
near-misses. That bucket is a grab-bag. This pass pulls the per-instruction diff
(`objdiff-cli diff --include-instructions`) for each and buckets the residual
mismatches into:

  CALL_NAMING  : `bl fn_XXXX` (unnamed target callee) vs `bl ?Named` -- the
                 target obj just lacks a symbol name; resolves via naming /
                 identity-transfer, NOT codegen. Source-immune at obj level.
  PEEPHOLE     : opcode-level replace (e.g. cmplwi <-> extsb., mr <-> cmplwi) --
                 compiler-internal instruction selection. The strcpy NUL-test
                 wall. UNREACHABLE by source/flag/permuter.
  REGALLOC     : same-opcode register-only arg diff the regswap detector missed
                 (below its 3-swap threshold). Permuter-class. GPR/FPR split.
  IMM_OFFSET   : same-opcode immediate/displacement diff (struct offset / stack).
  BODY         : insert/delete (length diff) or many opcode diffs = genuine code
                 divergence. Body-port reachable (a DIFFERENT, mostly-spent lever).
  OTHER        : leftover.

Dominant per-fn class chosen by codegen relevance. Aggregates the PEEPHOLE
opcode-transition pairs globally (to size the strcpy wall).

Read-only. Usage:
  tools/enrich_unattributed.py [--jobs 12] [--bucket UNATTRIBUTED]
"""
import argparse
import json
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OBJDIFF_CLI = os.path.join(ROOT, "bin", "objdiff-cli")
CACHE = "/tmp/claude/nearmiss_inventory.jsonl"
OUT = "/tmp/claude/unattributed_enriched.jsonl"

UNNAMED = re.compile(r'(fn_[0-9A-Fa-f]+|lbl_[0-9A-Fa-f]+|sub_[0-9A-Fa-f]+|loc_[0-9A-Fa-f]+)')
BRANCH_OPS = {"bl", "b", "beq", "bne", "blt", "bgt", "ble", "bge", "bdnz", "bdz", "bctrl", "bctr"}
REG_RE = re.compile(r'^(r\d+|f\d+|v\d+|cr\d+)$')
IMM_RE = re.compile(r'^(-?\d+|0x[0-9A-Fa-f]+|-?0x[0-9A-Fa-f]+)$')


def toks(s):
    return [t for t in re.split(r'[,\s()]+', s or '') if t]


def diff_one(unit, sym):
    out = f"/tmp/claude/_enr_{os.getpid()}_{abs(hash((unit,sym)))%99999}.json"
    try:
        subprocess.run([OBJDIFF_CLI, "diff", "-p", ".", "-u", unit, sym,
                        "--include-instructions", "-f", "json", "-o", out],
                       capture_output=True, text=True, cwd=ROOT, timeout=120)
        d = json.load(open(out))
    except Exception:
        return None
    finally:
        try:
            os.remove(out)
        except OSError:
            pass
    return d


def classify_insn(ins):
    """Return (subclass, detail) for one non-equal instruction."""
    mt = ins.get("match_type")
    if mt == "equal":
        return None
    t = ins.get("target") or {}
    b = ins.get("base") or {}
    top, bop = t.get("opcode"), b.get("opcode")
    targs, bargs = t.get("args") or "", b.get("args") or ""
    if not t or not b:
        return ("BODY", "indel")
    if top != bop:
        # opcode-level change
        if top in BRANCH_OPS and bop in BRANCH_OPS:
            return ("BODY", f"{top}->{bop}")
        return ("PEEPHOLE", f"{top}->{bop}")
    # same opcode -> arg diff
    if top in BRANCH_OPS:
        # bl/b target diff: is the target side an unnamed fn_/lbl_?
        if UNNAMED.search(targs) or UNNAMED.search(bargs):
            return ("CALL_NAMING", top)
        return ("CALL_OTHER", top)
    tt, bt = toks(targs), toks(bargs)
    if len(tt) != len(bt):
        return ("BODY", "argcount")
    diffs = [(x, y) for x, y in zip(tt, bt) if x != y]
    if not diffs:
        return None
    # symbol token where target unnamed?
    if any(UNNAMED.search(x) or UNNAMED.search(y) for x, y in diffs):
        return ("CALL_NAMING", "memref")
    if all(REG_RE.match(x) and REG_RE.match(y) for x, y in diffs):
        cls = "FPR" if any(x.startswith("f") for x, y in diffs) else "GPR"
        return ("REGALLOC", cls)
    if all(IMM_RE.match(x) and IMM_RE.match(y) for x, y in diffs):
        return ("IMM_OFFSET", f"{diffs[0][0]}->{diffs[0][1]}")
    return ("OTHER", "|".join(f"{x}->{y}" for x, y in diffs[:2]))


# priority for the per-fn dominant codegen-relevance label
PRIORITY = ["PEEPHOLE", "REGALLOC", "IMM_OFFSET", "BODY", "CALL_OTHER", "OTHER", "CALL_NAMING"]


def enrich(rec):
    d = diff_one(rec["unit"], rec["name"])
    out = {"unit": rec["unit"], "name": rec["name"], "size": rec["size"],
           "report_pct": rec.get("report_pct")}
    if d is None:
        out["sub"] = "DIFF_FAIL"
        return out
    sub = Counter()
    peep = Counter()
    details = Counter()
    for ins in d.get("instructions", []):
        c = classify_insn(ins)
        if not c:
            continue
        sub[c[0]] += 1
        details[f"{c[0]}:{c[1]}"] += 1
        if c[0] == "PEEPHOLE":
            peep[c[1]] += 1
    out["subcounts"] = dict(sub)
    out["peephole_ops"] = dict(peep)
    # codegen-relevant residual = everything except CALL_NAMING
    nonnaming = {k: v for k, v in sub.items() if k != "CALL_NAMING"}
    if not nonnaming:
        out["sub"] = "CALL_NAMING_ONLY"   # resolves by naming; not a codegen wall
    else:
        for p in PRIORITY:
            if p in nonnaming:
                out["sub"] = p
                break
        else:
            out["sub"] = "OTHER"
    out["details"] = dict(details)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", type=int, default=12)
    ap.add_argument("--bucket", default="UNATTRIBUTED")
    ap.add_argument("--cache", default=CACHE)
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--include-tiny", action="store_true",
                    help="also enrich size==40 artifacts (default: skip)")
    args = ap.parse_args()

    recs = []
    for line in open(args.cache):
        r = json.loads(line)
        if r.get("primary") != args.bucket:
            continue
        if r["size"] == 40 and not args.include_tiny:
            continue
        recs.append(r)
    print(f"enriching {len(recs)} {args.bucket} fns (size!=40)", file=sys.stderr)

    results = []
    done = 0
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for out in ex.map(enrich, recs):
            results.append(out)
            done += 1
            if done % 50 == 0:
                print(f"\r  {done}/{len(recs)}", end="", file=sys.stderr)
    print("", file=sys.stderr)

    with open(args.out, "w") as fh:
        for r in results:
            fh.write(json.dumps(r) + "\n")

    # ---- summary ----
    print("\n" + "=" * 70)
    print(f"UNATTRIBUTED SUB-CLASSIFICATION ({len(results)} real-bodied)")
    print("=" * 70)
    dom = Counter(r["sub"] for r in results)
    named = Counter()
    for r in results:
        if r["name"].startswith("?"):
            named[r["sub"]] += 1
    print(f"  {'SUBCLASS':<20}{'count':>6}{'named':>7}")
    for c, ct in dom.most_common():
        print(f"  {c:<20}{ct:>6}{named[c]:>7}")

    # global peephole opcode transitions
    peep = Counter()
    for r in results:
        for k, v in (r.get("peephole_ops") or {}).items():
            peep[k] += v
    print("\n## PEEPHOLE opcode transitions (target->base), top 25")
    for k, v in peep.most_common(25):
        print(f"  {v:>5}  {k}")


if __name__ == "__main__":
    main()
