#!/usr/bin/env python3
"""Audit functions the normalized metric counts as "matched" but that are NOT
byte-exact, to find whether the masked arg diffs are benign (register/branch/
stack-frame noise) or real semantic differences the metric is hiding:

  - reloc_target : a relocation points at a DIFFERENT symbol  (wrong call / wrong global)
  - member       : non-stack memory offset differs            (wrong struct field / vtable slot)
  - constant     : a literal immediate differs                (wrong constant)

These three are the bugs that survive into a native port (registers get
reallocated by the host compiler; constants/offsets/call-targets do not).

DC3 port of the RB3 auditor. Platform/ABI: Xbox 360 PowerPC (32-bit BE),
MSVC compiler (CL/PPC). Same opcode tables as Wii/Gekko except no paired-
single (`psq_*`) ops; register conventions are the same (r1 = sp, r2 = TOC).

Read-only: diffs already-built .obj files (NO --build), so it is safe to run
alongside the build/permuter fleet and never touches the ninja lock.

Usage:
    python3 scripts/analysis/audit_normalized_masking.py            # inflated set (norm==100 & raw<100)
    python3 scripts/analysis/audit_normalized_masking.py --all-gap  # every fn where norm>raw
    python3 scripts/analysis/audit_normalized_masking.py --workers 4 --limit 50
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# DC3 ships an older bin/objdiff-cli (Mar 24); pin to the metric-tweak build
# directly so the fresh per-fn diffs use honest immediates instead of the
# stale inflated metric that wrote report.json originally.
OBJDIFF_CANDIDATES = [
    os.environ.get("OBJDIFF_CLI", ""),
    os.path.normpath(os.path.join(REPO, "..", "objdiff", "target", "release", "objdiff-cli")),
    os.path.join(REPO, "bin", "objdiff-cli"),
]
OBJDIFF = next((p for p in OBJDIFF_CANDIDATES if p and os.path.isfile(p)), OBJDIFF_CANDIDATES[1])
REPORT = os.path.join(REPO, "build", "45410914", "report.json")

# PPC D-form memory ops: [dataReg, dispImm, baseReg]
# Xbox 360 PPC is plain PowerPC — no Gekko paired-singles. Everything else
# (load/store-with-update, multiple-word, FP) is identical to the RB3 list.
LOADSTORE = {
    "lwz", "lwzu", "lbz", "lbzu", "lhz", "lhzu", "lha", "lhau", "lwa",
    "lfs", "lfsu", "lfd", "lfdu", "lmw",
    "stw", "stwu", "stb", "stbu", "sth", "sthu",
    "stfs", "stfsu", "stfd", "stfdu", "stmw",
    # Xbox 360 is 64-bit-capable PPC; MSVC PPC uses doubleword load/store
    # when spilling pairs of GPRs across function calls.
    "ld", "ldu", "std", "stdu",
}
ADDR_CALC = {"addi", "addic", "addic.", "subi", "addis"}
FRAME_REGS = {"r1", "sp"}
# Xbox 360 MSVC has no small-data area; r2 is the TOC pointer but the
# compiler doesn't materialise SDA-relative addressing the way MetroWorks
# does, so the effective "drop these displacements" set is just r2.
SDA_REGS = {"r2"}


def base_reg_after_imm(typed_args):
    """For a D-form op, the base register is the Register that follows the immediate."""
    seen_imm = False
    for a in typed_args:
        t = a.get("type")
        if t in ("Signed", "Unsigned"):
            seen_imm = True
        elif t == "Register" and seen_imm:
            return a.get("value")
    return None


def second_register(typed_args):
    regs = [a.get("value") for a in typed_args if a.get("type") == "Register"]
    return regs[1] if len(regs) > 1 else (regs[0] if regs else None)


REVIEW_CATS = ("reloc_target", "member", "constant", "addr_const")


def norm_sym(s):
    """Collapse benign symbol-name noise so genuine wrong-target diffs stand out."""
    s = str(s)
    # Pool float/double constants: same value, different storage placement (benign).
    if (s.startswith("@F_") or s.startswith("@D_")
            or "floatBase" in s or "doubleBase" in s or "@stringBase" in s):
        return "POOL"
    # MSVC mangling: `?stringConst@?1??Func@@...` and `__real@40400000` literal
    # constants are layout pool symbols (same value, different placement).
    if s.startswith("__real@") or s.startswith("__xmm@"):
        return "POOL"
    if "??_C@_" in s:  # MSVC string-literal symbol
        return "POOL"
    # Anonymous-namespace discriminators and permuter working-copy artifacts.
    s = re.sub(r"@unnamed@.*$", "", s)
    s = re.sub(r"@.*$", "", s)
    # Compiler discriminators: __FUNCTION__$58088, foo__123 (benign renumbering).
    s = re.sub(r"\$\d+$", "", s)
    s = re.sub(r"__\d+$", "", s)
    return s


def value_sig(opcode, typed_args, extra_frame_regs=()):
    """Per-instruction value signature: opcode + non-register value tokens, with
    frame/SDA memory displacements dropped (stack layout is allowed to differ).
    Returns None if there's nothing value-bearing to compare.

    `extra_frame_regs` is a set of additional registers that the function's
    prologue set up as frame pointers (MSVC PPC habit on large frames).
    """
    if opcode in LOADSTORE:
        base = base_reg_after_imm(typed_args)
    elif opcode in ADDR_CALC:
        base = second_register(typed_args)
    else:
        base = None
    drop_imm = base in FRAME_REGS or base in SDA_REGS or base in extra_frame_regs
    imms, syms = [], []
    for a in typed_args:
        t = a.get("type")
        v = a.get("value")
        if t == "Register":
            continue
        if t in ("Signed", "Unsigned"):
            if not drop_imm:
                imms.append(("imm", int(v)))
        else:  # Symbol / Reloc / address label
            sv = str(v)
            # Bare address literal (intra-function branch target / jump label):
            # pure relative-layout noise, never a semantic difference.
            if re.fullmatch(r"-?0x[0-9a-fA-F]+", sv) or re.fullmatch(r"-?\d+", sv):
                continue
            syms.append(("sym", norm_sym(sv)))
    # When a relocation symbol is present, the immediate is its addend (an offset
    # into a pooled data/string/vtable section) — layout-dependent, so drop it and
    # compare only the symbol. Pure-immediate instructions (no reloc) keep their
    # immediate: that's a genuine member offset / constant (the TempoMap class).
    vals = syms if syms else imms
    return (opcode, tuple(vals))


def categorize_sig(sig):
    """Bucket a leftover (unmatched) value signature."""
    opcode, vals = sig
    if not vals:
        return "frame"          # only frame/sda displacement remained → benign
    syms = [v for v in vals if v[0] == "sym"]
    if syms:
        if all(v[1] == "POOL" for v in syms):
            return "pool"       # pool constant placement → benign
        return "reloc_target"   # references a genuinely different symbol
    if opcode in LOADSTORE:
        return "member"         # non-frame memory offset (wrong field / vtable slot)
    if opcode in ADDR_CALC:
        return "addr_const"
    return "constant"           # cmpwi / li / ori / mulli ... wrong literal


def detect_frame_ptr_regs(instructions, limit=20):
    """MSVC PPC commonly reserves r31 as a frame pointer when the stack frame
    is large (`subi r31, r1, N` or `addi r31, r1, N` in prologue). Treat memory
    ops based on that register as frame-relative (stack-layout noise), not as
    `this->member`. RB3 (MetroWorks/Wii) never does this — r31 there is always
    a callee-saved general-purpose reg or `this`.

    Checks BOTH sides (target + base) of each instruction. If target sets up
    r31 = sp-N and base sets up r30 = sp-N (or any frame pointer), we need to
    drop BOTH `0xNN(r31)` (target) and `0xNN(r30)` (base) displacements as
    stack noise."""
    fp = set()
    for x in instructions[:limit]:
        for side in ("target", "base"):
            ins = x.get(side) or {}
            op = ins.get("opcode", "")
            targs = ins.get("typed_args") or []
            if op in ("subi", "addi") and targs:
                regs = [a.get("value") for a in targs if a.get("type") == "Register"]
                if len(regs) >= 2 and regs[0] not in ("r1", "sp") and regs[1] in ("r1", "sp"):
                    fp.add(regs[0])
    return fp


# Sandbox-friendly scratch dir: honor $TMPDIR (set by the harness sandbox), else /tmp.
AUDIT_TMP = os.path.join(os.environ.get("TMPDIR", "/tmp"), "objdiff_audit")


def diff_one(symbol, unit):
    """Run a read-only objdiff and classify masked arg diffs. Returns dict or None."""
    fd, path = tempfile.mkstemp(suffix=".json", prefix="audit_", dir=AUDIT_TMP)
    os.close(fd)
    try:
        cmd = [OBJDIFF, "diff", "-p", REPO, symbol, "-u", unit,
               "--include-instructions", "-f", "json", "-o", path]
        r = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, timeout=120)
        if r.returncode != 0:
            return {"symbol": symbol, "unit": unit, "error": (r.stderr or "")[-200:]}
        with open(path) as f:
            d = json.load(f)
    except Exception as e:  # noqa: BLE001
        return {"symbol": symbol, "unit": unit, "error": str(e)[:200]}
    finally:
        try:
            os.unlink(path)
        except OSError:
            pass

    # Detect MSVC frame-pointer regs (usually r31 = sp-offset) so loads/stores
    # through them get dropped as stack-layout noise, not flagged as members.
    extra_frame_regs = detect_frame_ptr_regs(d.get("instructions", []))

    # Surface-level counts for context.
    surf = Counter()        # reg / branch
    other_mt = Counter()    # non diff_arg, non equal (should be ~0 when norm==100)
    # Whole-function value multisets (registers dropped, frame/sda displacements
    # dropped). A pure register-rename + instruction-reorder nets to an EQUAL
    # multiset; only genuine value differences survive.
    tgt_sigs, base_sigs = Counter(), Counter()
    sig_text = {}           # sig -> (ttext, stext) example for reporting

    for ins in d.get("instructions", []):
        mt = ins.get("match_type")
        if mt == "equal":
            continue
        if mt != "diff_arg":
            other_mt[mt] += 1
            continue
        bd = ins.get("diff_breakdown") or {}
        for arg in bd.get("arguments", []):
            at = arg.get("arg_type")
            if arg.get("target", {}).get("value") == arg.get("base", {}).get("value"):
                continue
            if at == "register":
                surf["reg"] += 1
            elif at == "branch_dest":
                surf["branch"] += 1
        tgt = ins.get("target") or {}
        base = ins.get("base") or {}
        ttext = f"{tgt.get('opcode','?')} {tgt.get('args','')}"
        stext = f"{base.get('opcode','?')} {base.get('args','')}"
        ts = value_sig(tgt.get("opcode", ""), tgt.get("typed_args") or [], extra_frame_regs)
        bs = value_sig(base.get("opcode", ""), base.get("typed_args") or [], extra_frame_regs)
        if ts is not None:
            tgt_sigs[ts] += 1
            sig_text.setdefault(ts, (ttext, stext))
        if bs is not None:
            base_sigs[bs] += 1
            sig_text.setdefault(bs, (ttext, stext))

    # Leftover = value sigs present on one side but not matched on the other.
    leftover = (tgt_sigs - base_sigs) + (base_sigs - tgt_sigs)
    cats = Counter()
    concerns = []
    for sig, n in leftover.items():
        cat = categorize_sig(sig)
        cats[cat] += n
        if cat in REVIEW_CATS:
            tt, st = sig_text.get(sig, ("", ""))
            concerns.append((cat, tt, st))

    SEV = {"reloc_target": 10, "member": 8, "constant": 5, "addr_const": 3}
    severity = sum(SEV.get(k, 0) * v for k, v in cats.items())
    verdict = "REVIEW" if any(cats[k] for k in REVIEW_CATS) else "BENIGN"
    return {
        "symbol": symbol, "unit": unit,
        "raw": d.get("raw_match_percent"),
        "norm": d.get("normalized_match_percent"),
        "surface": dict(surf),
        "cats": dict(cats),
        "other_mt": dict(other_mt),
        "severity": severity,
        "verdict": verdict,
        "concerns": concerns[:12],
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--workers", type=int, default=6)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--all-gap", action="store_true",
                    help="audit every fn where norm>raw (not just norm==100)")
    ap.add_argument("--out", default=os.path.join(AUDIT_TMP, "results.json"))
    args = ap.parse_args()

    os.makedirs(AUDIT_TMP, exist_ok=True)
    print(f"OBJDIFF: {OBJDIFF}", file=sys.stderr)
    print(f"REPORT : {REPORT}", file=sys.stderr)
    rep = json.load(open(REPORT))
    targets = []
    for u in rep["units"]:
        un = u.get("name")
        for f in (u.get("functions") or []):
            n = f.get("match_percent_normalized")
            raw = f.get("fuzzy_match_percent")
            if n is None or raw is None:
                continue
            keep = (n > raw) if args.all_gap else (n == 100.0 and raw < 100.0)
            if keep:
                targets.append((f["name"], un, raw, n, int(f.get("size", 0))))
    targets.sort(key=lambda t: t[2])  # lowest raw first
    if args.limit:
        targets = targets[:args.limit]
    print(f"Auditing {len(targets)} functions with {args.workers} workers (read-only)...",
          file=sys.stderr)

    results = []
    done = 0
    with ThreadPoolExecutor(max_workers=args.workers) as ex:
        futs = {ex.submit(diff_one, sym, un): (sym, un) for sym, un, *_ in targets}
        for fut in as_completed(futs):
            results.append(fut.result())
            done += 1
            if done % 100 == 0:
                print(f"  {done}/{len(targets)}", file=sys.stderr)

    json.dump(results, open(args.out, "w"), indent=1)
    review = [r for r in results if r.get("verdict") == "REVIEW"]
    benign = [r for r in results if r.get("verdict") == "BENIGN"]
    errors = [r for r in results if r.get("error")]

    agg = Counter()
    for r in results:
        for k, v in (r.get("cats") or {}).items():
            agg[k] += v

    print("\n" + "=" * 70)
    print("NORMALIZED-METRIC MASKING AUDIT")
    print("=" * 70)
    print(f"audited     : {len(results)}")
    print(f"  BENIGN    : {len(benign)}  (only reg/branch/frame/sda diffs)")
    print(f"  REVIEW    : {len(review)}  (has reloc-target / member-offset / constant diffs)")
    print(f"  errors    : {len(errors)}")
    print("\nUnmatched value-sig totals by category (genuine value differences only;")
    print("register renames and instruction reorders already cancelled out):")
    for k, v in agg.most_common():
        tag = {"frame": "benign", "pool": "benign", "addr_const": "REVIEW",
               "constant": "REVIEW", "member": "REVIEW", "reloc_target": "REVIEW"}.get(k, "?")
        print(f"  {k:14s}: {v:6d}  [{tag}]")

    review.sort(key=lambda r: -r.get("severity", 0))
    print("\n" + "-" * 70)
    print("TOP REVIEW CANDIDATES (metric may be hiding a real semantic diff)")
    print("-" * 70)
    for r in review[:50]:
        cats = r.get("cats", {})
        flags = ",".join(f"{k}={cats[k]}" for k in REVIEW_CATS if cats.get(k))
        print(f"\n[{r['severity']:3d}] raw {r['raw']:.2f}%  {r['unit']}  ::  {r['symbol'][:55]}")
        print(f"      {flags}")
        for cat, tgt, src in r.get("concerns", [])[:4]:
            print(f"        [{cat}]  TGT {tgt[:42]:42s}  SRC {src[:42]}")
    print(f"\nFull results: {args.out}")


if __name__ == "__main__":
    main()
