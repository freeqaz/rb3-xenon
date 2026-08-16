#!/usr/bin/env python3
"""Sub-object reference-local signature scanner (lane AA, 2026-07-26).

Detects the "named sub-object reference local" codegen lever, first landed on
`FocusTracker::SavePlayerStats` / `DeployCountTracker::SavePlayerStats`
(commit df7f5091, branch laneAA-clusters).

THE SHAPE
---------
Under /O1 the compiler decides *once per source expression form* whether to
materialise the address of a sub-object into a register:

    // form M ("outer pointer kept")     -> mr rX, rY   then  st* f0, BIG(rX)
    pPlayer->mStats.field = v;

    // form A ("sub-object named")       -> addi rX, rY, K   then st* f0, BIG-K(rX)
    Stats &stats = pPlayer->mStats;
    ... intervening call ...
    stats.field = v;

So a function that picked the wrong form shows objdiff rows:

    [i] replace : target `addi rX, rY, K`  vs  base `mr rX, rY`     (or reversed)
    [j] diff_arg: <load/store> ... base register rX,
                  imm(target) - imm(base) == -K                     (or +K)

The offset delta on the dependent memory ops MUST equal the addi immediate --
that corroboration is what separates this from ordinary regalloc noise.

WHAT IT IS NOT
--------------
* uniform member-offset drift on *every* this-relative access with no addi/mr
  replace row  -> that is the member-delta / struct-layout lever, not this one.
* `addi rX,rY,K1` vs `addi rX,rY,K2` with matching mem deltas -> emitted as
  class ADDI_ADDI (wrong sub-object named, still a call-site fix, weaker prior).

PIPELINE
--------
stage 0  pool      : report.json -> named, sub-100, real-bodied functions
stage 1  batch gate: one `objdiff-cli diff --batch` process over the whole pool
                     (~4 s for 855 syms) -> keep anything with >=1 `replace`
                     or (with --wide) any diff_arg-bearing function
stage 2  shape     : per-symbol `objdiff-cli diff --include-instructions`
                     (~0.08 s each) -> apply the signature above

Scores/percentages printed here are objdiff's, for RANKING ONLY.  Strict match
is report.json `match_percent_normalized == 100.0`; always A/B whole-binary.

Usage:
  python3 scripts/harvest/subobject_ref_scan.py -p . -o ~/tmp/subobj_scan.json
  python3 scripts/harvest/subobject_ref_scan.py -p . --sym '?Foo@Bar@@QAAXXZ'
"""
import argparse
import json
import os
import subprocess
import sys

MEM_OPS = {
    "lwz", "lwzu", "lhz", "lhzu", "lha", "lhau", "lbz", "lbzu", "lmw",
    "lfs", "lfsu", "lfd", "lfdu", "ld", "lwa",
    "stw", "stwu", "sth", "sthu", "stb", "stbu", "stmw",
    "stfs", "stfsu", "stfd", "stfdu", "std", "stdu",
    "addi", "addic", "subi",
}


def _typed(ins):
    return ins.get("typed_args") or []


def _regs_and_imm(ins):
    """Return (list_of_registers, list_of_signed_immediates) for an instruction."""
    regs, imms = [], []
    for a in _typed(ins):
        t = a.get("type")
        if t == "Register":
            regs.append(a.get("value"))
        elif t in ("Signed", "Unsigned"):
            imms.append(int(a.get("value")))
    return regs, imms


def _mem_row(ins):
    """(base_reg, imm) for a memory-ish instruction, else None.

    objdiff prints PPC D-form as `rD, imm, rA` -> typed args
    [Register rD, Signed imm, Register rA].  `addi` is the same shape.
    """
    if ins is None:
        return None
    if ins.get("opcode") not in MEM_OPS:
        return None
    ta = _typed(ins)
    if len(ta) < 3:
        return None
    if ta[1].get("type") not in ("Signed", "Unsigned"):
        return None
    if ta[2].get("type") != "Register":
        return None
    return ta[2].get("value"), int(ta[1].get("value"))


def classify(instructions):
    """Return a list of hit dicts for the sub-object-reference signature."""
    hits = []
    anchors = []  # (index, kind, dest_reg, src_reg, K)  K = target_off - base_off

    for row in instructions:
        mt = row.get("match_type")
        t, b = row.get("target"), row.get("base")
        if mt == "replace" and t and b:
            to, bo = t.get("opcode"), b.get("opcode")
            tr, ti = _regs_and_imm(t)
            br, bi = _regs_and_imm(b)
            if to == "addi" and bo == "mr" and len(tr) >= 2 and len(br) >= 2 and ti:
                if tr[0] == br[0] and tr[1] == br[1]:
                    anchors.append((row["index"], "TGT_ADDI", tr[0], tr[1], ti[0]))
            elif to == "mr" and bo == "addi" and len(tr) >= 2 and len(br) >= 2 and bi:
                if tr[0] == br[0] and tr[1] == br[1]:
                    anchors.append((row["index"], "BASE_ADDI", tr[0], tr[1], -bi[0]))
        elif mt == "delete" and t is not None and t.get("opcode") == "addi":
            # retail materialises a sub-object pointer we never form at all
            tr, ti = _regs_and_imm(t)
            if len(tr) >= 2 and ti:
                anchors.append((row["index"], "TGT_ONLY_ADDI", tr[0], tr[1], ti[0]))
        elif mt == "insert" and b is not None and b.get("opcode") == "addi":
            br, bi = _regs_and_imm(b)
            if len(br) >= 2 and bi:
                anchors.append((row["index"], "BASE_ONLY_ADDI", br[0], br[1],
                                -bi[0]))
        elif mt == "diff_arg" and t and b and t.get("opcode") == "addi" \
                and b.get("opcode") == "addi":
            tr, ti = _regs_and_imm(t)
            br, bi = _regs_and_imm(b)
            if len(tr) >= 2 and len(br) >= 2 and ti and bi and tr[0] == br[0] \
                    and tr[1] == br[1] and ti[0] != bi[0]:
                anchors.append((row["index"], "ADDI_ADDI", tr[0], tr[1],
                                ti[0] - bi[0]))

    if not anchors:
        return hits

    # Corroboration: dependent mem ops on the anchor's dest register whose
    # immediate delta equals -K.
    for idx, kind, dest, src, K in anchors:
        corro, contra = [], []
        for row in instructions:
            if row.get("match_type") != "diff_arg":
                continue
            tm = _mem_row(row.get("target"))
            bm = _mem_row(row.get("base"))
            if not tm or not bm:
                continue
            if tm[0] != dest or bm[0] != dest:
                continue
            d = tm[1] - bm[1]
            entry = {
                "index": row["index"],
                "opcode": row["target"].get("opcode"),
                "target_off": tm[1], "base_off": bm[1], "delta": d,
            }
            (corro if d == -K else contra).append(entry)
        # r1-based anchors are stack-frame layout drift, not sub-object naming.
        frame = (src == "r1" or dest == "r1")
        weak = kind in ("ADDI_ADDI", "TGT_ONLY_ADDI", "BASE_ONLY_ADDI")
        if corro or (not weak and not frame):
            hits.append({
                "anchor_index": idx, "class": kind, "dest": dest, "src": src,
                "K": K, "frame_relative": frame,
                "n_corroborating": len(corro),
                "n_contradicting": len(contra),
                "corroborating": corro, "contradicting": contra,
            })
    return hits


def run_symbol(objdiff, project, symbol, out_dir):
    path = os.path.join(out_dir, "sr_%d.json" % (abs(hash(symbol)) % (10 ** 12)))
    cmd = [objdiff, "diff", "-p", project, symbol,
           "--include-instructions", "-f", "json", "-o", path]
    r = subprocess.run(cmd, cwd=project, capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(path):
        return None
    try:
        with open(path) as fh:
            d = json.load(fh)
    finally:
        try:
            os.unlink(path)
        except OSError:
            pass
    return d


def build_pool(project, min_pct, min_size):
    rep = os.path.join(project, "build/45410914/report.json")
    with open(rep) as fh:
        r = json.load(fh)
    pool = []
    for u in r["units"]:
        for f in u.get("functions", []):
            p = f.get("match_percent_normalized")
            if p is None or p == 100.0 or p < min_pct:
                continue
            n = f["name"]
            if n.startswith("fn_") or n.startswith("__unwind") or "$" in n:
                continue
            if int(f.get("size", 0)) < min_size:
                continue
            pool.append({"unit": u["name"], "name": n, "pct": p,
                         "size": int(f["size"])})
    return pool


def batch_gate(objdiff, project, symbols, wide):
    # NB: no --include-instructions here.  This gate reads only
    # `instruction_summary` and `symbol`; the per-row instruction stream is
    # stage 2's business (run_symbol).  objdiff-cli used to drop the flag in
    # --batch mode, so asking for it was free; 4.2.3 honours it and the
    # stage-1 stdout grows ~25x (0.22 MB -> 5.7 MB on a 60-symbol sample) for
    # output no consumer here reads.  Every other field is identical.
    proc = subprocess.run(
        [objdiff, "diff", "-p", project, "--batch",
         "-f", "json", "-o", "-"],
        cwd=project, input="\n".join(symbols) + "\n",
        capture_output=True, text=True)
    keep = []
    for line in proc.stdout.splitlines():
        if not line.startswith("{"):
            continue
        try:
            d = json.loads(line)
        except ValueError:
            continue
        s = d.get("instruction_summary") or {}
        if wide:
            if s.get("diff_arg", 0) or s.get("replace", 0):
                keep.append(d["symbol"])
        elif s.get("replace", 0) >= 1:
            keep.append(d["symbol"])
    return keep


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--project-dir", default=".")
    ap.add_argument("-o", "--output")
    ap.add_argument("--sym", help="scan a single symbol")
    ap.add_argument("--min-pct", type=float, default=40.0)
    ap.add_argument("--min-size", type=int, default=24)
    ap.add_argument("--wide", action="store_true",
                    help="stage-1 gate keeps diff_arg-only functions too "
                         "(catches ADDI_ADDI)")
    ap.add_argument("--limit", type=int)
    args = ap.parse_args()

    project = os.path.abspath(args.project_dir)
    objdiff = os.path.join(project, "bin", "objdiff-cli")
    if not os.path.exists(objdiff):
        sys.exit("objdiff-cli not found at %s" % objdiff)
    out_dir = os.path.expanduser("~/tmp")

    if args.sym:
        d = run_symbol(objdiff, project, args.sym, out_dir)
        if not d:
            sys.exit("objdiff failed for %s" % args.sym)
        print(json.dumps(classify(d.get("instructions") or []), indent=2))
        return

    pool = build_pool(project, args.min_pct, args.min_size)
    print("pool: %d named sub-100 functions >= %.0f%%" % (len(pool), args.min_pct),
          file=sys.stderr)
    meta = {p["name"]: p for p in pool}
    survivors = batch_gate(objdiff, project, [p["name"] for p in pool], args.wide)
    print("stage-1 gate: %d survivors" % len(survivors), file=sys.stderr)
    if args.limit:
        survivors = survivors[:args.limit]

    records = []
    for i, sym in enumerate(survivors):
        if i % 50 == 0:
            print("  ... %d/%d" % (i, len(survivors)), file=sys.stderr)
        d = run_symbol(objdiff, project, sym, out_dir)
        if not d:
            continue
        hits = classify(d.get("instructions") or [])
        if not hits:
            continue
        m = meta.get(sym, {})
        records.append({
            "symbol": sym,
            "demangled": d.get("demangled"),
            "unit": m.get("unit"),
            "report_pct": m.get("pct"),
            "size": m.get("size"),
            "objdiff_pct": d.get("normalized_match_percent"),
            "hits": hits,
        })

    # Rank: object-relative (non-frame) first, then fully-corroborated,
    # then the strong REPLACE classes, then corroboration weight and %.
    flat = [(rec, h) for rec in records for h in rec["hits"]]

    def rank(item):
        rec, h = item
        return (h["frame_relative"], -(h["n_contradicting"] == 0),
                h["class"] == "ADDI_ADDI", -h["n_corroborating"],
                -(rec["report_pct"] or 0))
    flat.sort(key=rank)

    n_obj = sum(1 for _, h in flat if not h["frame_relative"])
    print("\n=== %d anchors over %d functions (%d object-relative) ==="
          % (len(flat), len(records), n_obj))
    for rec, h in flat:
        print("%-6.2f %-28s %-9s %s K=%+5d %s<-%-4s corro=%d contra=%d  %s"
              % (rec["report_pct"], (rec["unit"] or "")[:28], h["class"],
                 "FRAME" if h["frame_relative"] else "OBJ  ",
                 h["K"], h["dest"], h["src"], h["n_corroborating"],
                 h["n_contradicting"], rec["symbol"][:90]))

    if args.output:
        with open(os.path.expanduser(args.output), "w") as fh:
            json.dump({"records": records,
                       "summary": {"pool": len(pool),
                                   "stage1": len(survivors),
                                   "hits": len(records)}}, fh, indent=1)
        print("\nwrote %s" % args.output, file=sys.stderr)


if __name__ == "__main__":
    main()
