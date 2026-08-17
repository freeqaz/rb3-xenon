#!/usr/bin/env python3
"""VOID-OUTPUT WINDOW 2026-08-16 -> 2026-08-17. The `args` spelling change cut
CALL_NAMING by 78% and dropped every row whose relocation was the only
difference. (The d-form parens were harmless here -- `toks` already splits on
them; it was the vanished trailing relocation that did the damage.)

Every number this tool printed between the first rebuild carrying objdiff-cli
fdc5113 ("ruler I", committed 2026-08-16 08:34:03 UTC with its release binary
deliberately NOT rebuilt; confirmed live by 21:30 that day) and the repair
described below is VOID. Re-run it; do not carry it forward. Audit:
`ARGS_READER_AUDIT.md` in decomp-bench `archive/runs/objdiff-silent-flags-and-
dead-controls-2026-08-16/` (task #96); repair task #103. Swept 2026-08-17: NO
committed artifact in this repo, and no file at any of these tools' default
output paths, falls inside that window -- this banner exists for outputs held
outside git.

Sub-classify the UNATTRIBUTED near-miss bucket by instruction-level signature.

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


def flat_args(side):
    """Rebuild the pre-fdc5113 flat operand join from `typed_args`.

    objdiff-cli fdc5113 ("ruler I", 2026-08-16) changed the JSON `args` string
    from a comma-join of the COMPARISON arg list to the DISPLAY spelling. Of the
    three things that moved, this file was immune to two of them and fatally
    exposed to the third:

      * d-form parens (`0x38, r4` -> `0x38(r4)`): HARMLESS here. `toks` already
        splits on `(` and `)`, so the operand splits back into ['0x38','r4'] and
        both the token count and the IMM_RE/REG_RE classification survive. This
        is the one tokenizer in the repo that got it right by construction --
        do not "fix" it.
      * `@h`/`@l` reloc suffixes: harmless, because every consumer of a symbol
        token here is UNNAMED.search, a substring test.
      * the trailing NON-DISPLAYED relocation leaving the string: FATAL. That
        operand is where `bl fn_XXXX`-style naming evidence lives for pooled
        rows, so CALL_NAMING -- the "climbs when we name the symbol" bucket that
        is the whole point of this pass -- lost 78% of its rows, and rows whose
        relocation was the only difference produced no `diffs` at all and were
        dropped entirely.

    So the repair is the relocation channel, not the parens: rebuild from
    typed_args, whose trailing Symbol entry still carries the relocation. That
    reproduces the old join exactly (objdiff-core/src/obj/mod.rs' Display impls:
    Signed/BranchDest as signed hex, Unsigned as hex, everything else verbatim),
    and re-appending the symbol to the DISPLAY string instead would double-count
    it on rows like `stw r10, SYM@l(r11)` where it is already spelled out.
    """
    ta = side.get('typed_args')
    if ta is None:
        return side.get('args') or ''
    out = []
    for a in ta:
        t = a.get('type')
        v = a.get('value')
        if t in ('Signed', 'BranchDest') and isinstance(v, int):
            out.append(('-0x%x' % -v) if v < 0 else '0x%x' % v)
        elif t == 'Unsigned' and isinstance(v, int):
            out.append('0x%x' % v)
        else:
            out.append(str(v))
    return ', '.join(out)


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
    # flat_args, never side["args"] -- the display spelling drops the trailing
    # relocation this classifier reads. See flat_args' docstring.
    targs, bargs = flat_args(t), flat_args(b)
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
