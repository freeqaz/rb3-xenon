#!/usr/bin/env python3
"""EE2-B: rank the body-port queue by DEFECT SIGNATURE, not mismatch count.

Lane EE2-A measured that the queue's cheapest-first (mismatch-count) ordering is
ANTI-correlated with tractability: the 1-3 mismatch head is all "STACKISH" MSVC
stack-temp allocation artifacts, while the row that actually paid sat at #14.

This re-measures every row live (the shipped queue is stale) and classifies the
residual by the *kind* of instruction that differs, which is what distinguishes a
source defect from a codegen artifact.
"""
import json
import subprocess
import sys
import csv

WT = "/home/free/tmp/wt-ee2b"
CFG = ["-c", "functionRelocDiffs=none", "-c", "ppc.calculatePoolRelocations=false"]

# opcode families
BRANCH_COND = {
    "beq", "bne", "blt", "bgt", "ble", "bge", "bdnz", "bdz",
    "beqlr", "bnelr", "bltlr", "bgtlr", "blelr", "bgelr",
}
WIDTH_OPS = {"clrlwi", "extsb", "extsh", "extsw", "rlwinm", "clrlwi.", "extrwi", "clrrwi", "insrwi"}
CMP_OPS = {"cmpw", "cmplw", "cmpwi", "cmplwi", "cmpd", "cmpld"}
LOADSTORE = {
    "lwz", "stw", "lhz", "sth", "lbz", "stb", "lfs", "stfs", "lfd", "stfd",
    "lwzx", "stwx", "lha", "lmw", "stmw", "lwzu", "stwu",
}
FP_OPS_PREFIX = ("fadd", "fsub", "fmul", "fdiv", "fmadd", "fmsub", "fnmadd",
                 "fnmsub", "fneg", "fabs", "fsel", "fres", "frsqrte", "fctiw",
                 "fmr", "fcmp")


def frame_rel(ins):
    """True if this is a load/store relative to a frame register (r1/r31)."""
    if ins is None or ins.get("opcode") not in LOADSTORE:
        return False
    regs = [a["value"] for a in ins.get("typed_args", []) if a["type"] == "Register"]
    return any(r in ("r1", "r31") for r in regs)


def diff_row(unit, symbol):
    p = subprocess.run(
        [f"{WT}/bin/objdiff-cli", "diff", "-p", WT, "-u", unit, symbol]
        + CFG + ["--include-instructions", "-f", "json", "-o", "-"],
        capture_output=True, text=True, cwd=WT,
    )
    if p.returncode != 0:
        return None
    try:
        return json.loads(p.stdout)
    except Exception:
        return None


def classify(d):
    """Return (label, evidence-list). Labels ranked by how source-like they are."""
    mism = [i for i in d["instructions"] if i.get("match_type") != "equal"]
    tags = set()
    ev = []
    for i in mism:
        t, b = i.get("target"), i.get("base")
        to = t.get("opcode") if t else None
        bo = b.get("opcode") if b else None
        mt = i.get("match_type")

        # branch-polarity: both sides a conditional branch, different mnemonic.
        # This is the `it != end()` vs `it < end()` signature EE2-A landed on.
        if to in BRANCH_COND and bo in BRANCH_COND and to != bo:
            tags.add("BRANCH_POLARITY")
            ev.append(f"  idx{i['index']}: BRANCH {to} -> {bo}")
            continue
        # width op present on exactly one side = narrowed/widened local.
        if (to in WIDTH_OPS) != (bo in WIDTH_OPS):
            tags.add("WIDTH")
            ev.append(f"  idx{i['index']}: WIDTH {to} | {bo}")
            continue
        # a call appearing/vanishing or retargeted = real call-graph divergence
        if to == "bl" or bo == "bl":
            if to != bo or mt in ("insert", "delete"):
                tags.add("CALL")
                ev.append(f"  idx{i['index']}: CALL {to}({t.get('args') if t else ''}) | "
                          f"{bo}({b.get('args') if b else ''})")
                continue
        # comparison operand/const differences = wrong constant or wrong type
        if to in CMP_OPS or bo in CMP_OPS:
            tags.add("CMP")
            ev.append(f"  idx{i['index']}: CMP {to}({t.get('args') if t else ''}) | "
                      f"{bo}({b.get('args') if b else ''})")
            continue
        if (to and to.startswith(FP_OPS_PREFIX)) or (bo and bo.startswith(FP_OPS_PREFIX)):
            tags.add("FP")
            ev.append(f"  idx{i['index']}: FP {to} | {bo}")
            continue
        # pure frame-relative load/store churn = MSVC stack-temp allocation.
        if frame_rel(t) or frame_rel(b):
            tags.add("STACKISH")
            ev.append(f"  idx{i['index']}: STACK {to}({t.get('args') if t else ''}) | "
                      f"{bo}({b.get('args') if b else ''})")
            continue
        if to == bo:
            tags.add("REGSWAP_OR_IMM")
            ev.append(f"  idx{i['index']}: SAME-OP {to}: {t.get('args') if t else ''} | "
                      f"{b.get('args') if b else ''}")
            continue
        tags.add("OTHER")
        ev.append(f"  idx{i['index']}: OTHER {to}({t.get('args') if t else ''}) | "
                  f"{bo}({b.get('args') if b else ''})")
    return tags, ev, len(mism)


def main():
    rows = list(csv.DictReader(open(sys.argv[1]), delimiter="\t"))
    out = []
    for r in rows:
        d = diff_row(r["unit"], r["symbol"])
        if d is None:
            out.append((r, None, {"ERROR"}, [], -1))
            continue
        tags, ev, n = classify(d)
        out.append((r, d, tags, ev, n))

    # rank: source-like signatures first
    PRIORITY = ["BRANCH_POLARITY", "WIDTH", "CALL", "CMP", "REGSWAP_OR_IMM",
                "OTHER", "FP", "STACKISH"]

    def key(x):
        tags = x[2]
        best = min((PRIORITY.index(t) for t in tags if t in PRIORITY), default=99)
        return (best, -int(x[0]["size"]))

    out.sort(key=key)
    for r, d, tags, ev, n in out:
        live = f"{d['fuzzy_match_percent']:.3f}" if d else "?"
        stale = " <<STALE" if d and abs(d["fuzzy_match_percent"] - float(r["fuzzy"])) > 0.01 else ""
        print(f"[{'+'.join(sorted(tags))}] {r['size']}B  queue={r['fuzzy']} live={live}{stale} "
              f"nmis={n}  {r['symbol']}  ({r['unit']})")
        for line in ev[:14]:
            print(line)
        print()


if __name__ == "__main__":
    main()
