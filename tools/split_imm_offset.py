#!/usr/bin/env python3
"""Split the IMM_OFFSET near-miss sub-bucket into STACK vs STRUCT vs CONST.

For each IMM_OFFSET fn, pull the instruction diff and look at the differing
immediate's *context*:
  STACK   : displacement off r1 (sp) -- stack frame slot. Decl-order / regalloc
            cascade. Permuter-class (in principle).
  STRUCT  : displacement off a non-sp pointer in a load/store (lwz/stw/lhz/...
            rX, 0xNN(rY) where rY != r1) -- struct member offset. HEADER lever
            (DC3-drift class), out of the codegen-mission scope.
  CONST   : immediate operand of li/addi/cmplwi/... not a memory displacement --
            a literal constant. Data/source.
  MIXED/OTHER.

Read-only. Usage: tools/split_imm_offset.py [--jobs 12]
"""
import argparse
import json
import os
import re
import subprocess
import sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OBJDIFF_CLI = os.path.join(ROOT, "bin", "objdiff-cli")
ENRICHED = "/tmp/claude/unattributed_enriched.jsonl"

LOADSTORE = {"lwz", "lwzu", "stw", "stwu", "lhz", "lha", "lbz", "stb", "sth",
             "lfs", "lfd", "stfs", "stfd", "lwzx", "stwx", "ld", "std"}
MEMREF = re.compile(r'(-?0x[0-9A-Fa-f]+|-?\d+)\(\s*(r\d+)\s*\)')
IMM_RE = re.compile(r'^(-?\d+|-?0x[0-9A-Fa-f]+)$')


def toks(s):
    return [t for t in re.split(r'[,\s]+', s or '') if t]


def diff_one(unit, sym):
    out = f"/tmp/claude/_sio_{os.getpid()}_{abs(hash((unit, sym))) % 99999}.json"
    try:
        subprocess.run([OBJDIFF_CLI, "diff", "-p", ".", "-u", unit, sym,
                        "--include-instructions", "-f", "json", "-o", out],
                       capture_output=True, text=True, cwd=ROOT, timeout=120)
        return json.load(open(out))
    except Exception:
        return None
    finally:
        try:
            os.remove(out)
        except OSError:
            pass


def classify(rec):
    d = diff_one(rec["unit"], rec["name"])
    out = {"unit": rec["unit"], "name": rec["name"], "size": rec["size"]}
    if d is None:
        out["kind"] = "FAIL"
        return out
    c = Counter()
    for ins in d.get("instructions", []):
        if ins.get("match_type") in (None, "equal"):
            continue
        t = ins.get("target") or {}
        b = ins.get("base") or {}
        if t.get("opcode") != b.get("opcode"):
            continue  # not same-opcode imm diff
        op = t.get("opcode")
        ta, ba = t.get("args") or "", b.get("args") or ""
        if ta == ba:
            continue
        # memory displacement?
        mt, mb = MEMREF.search(ta), MEMREF.search(ba)
        if mt and mb and mt.group(1) != mb.group(1):
            base_reg = mt.group(2)
            if base_reg == "r1":
                c["STACK"] += 1
            else:
                c["STRUCT"] += 1
            continue
        # same-opcode token imm diff not in (mem) form
        tt, bt = toks(ta), toks(ba)
        if len(tt) == len(bt):
            diffs = [(x, y) for x, y in zip(tt, bt) if x != y]
            if diffs and all(IMM_RE.match(x) and IMM_RE.match(y) for x, y in diffs):
                # arithmetic/compare immediate
                if op in ("addi", "addic", "subfic") and tt and tt[0] != "r1":
                    # addi rX, rY, imm  -> could be address calc (struct) or const
                    c["STRUCT_OR_CONST"] += 1
                else:
                    c["CONST"] += 1
    out["counts"] = dict(c)
    if not c:
        out["kind"] = "NONE"
    else:
        out["kind"] = c.most_common(1)[0][0]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", type=int, default=12)
    args = ap.parse_args()
    recs = [json.loads(l) for l in open(ENRICHED)]
    imm = [r for r in recs if r.get("sub") == "IMM_OFFSET"]
    print(f"splitting {len(imm)} IMM_OFFSET fns", file=sys.stderr)
    res = []
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for o in ex.map(classify, imm):
            res.append(o)
    with open("/tmp/claude/imm_offset_split.jsonl", "w") as fh:
        for r in res:
            fh.write(json.dumps(r) + "\n")
    dom = Counter(r["kind"] for r in res)
    named = Counter(r["kind"] for r in res if r["name"].startswith("?"))
    print("\n## IMM_OFFSET split (dominant per fn)")
    print(f"  {'KIND':<18}{'count':>6}{'named':>7}")
    for k, v in dom.most_common():
        print(f"  {k:<18}{v:>6}{named[k]:>7}")


if __name__ == "__main__":
    main()
