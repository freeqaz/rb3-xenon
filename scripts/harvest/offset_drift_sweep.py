#!/usr/bin/env python3
"""Batch layout-drift sweep over a near-miss pool.

For every function in a gen_nearmiss_pool.py pool JSON, run objdiff and
extract the offset-delta fingerprint (via diff_inspect's parse_breakdowns).
Struct-layout drift shows up as immediate/offset arg diffs whose memory
operand base register is NOT the stack pointer; a dominant small delta
(+/-4, +/-8) across several instructions on the same base-reg class is the
tell that a shared header struct disagrees with retail.

Output: one JSON with per-function records:
  {sym, unit, pct, size, status, n_offset_diffs, deltas: {delta: count},
   struct_deltas: {delta: count},   # non-stack memory operands only
   examples: {delta: [{idx, tgt, src}]},
   reg_swaps, symbol_diffs, branch_diffs, inserts, deletes}

Usage:
  python3 scripts/harvest/offset_drift_sweep.py ~/tmp/pool.json \
      -o ~/tmp/drift_sweep.json [--project-dir .] [--limit N] [--resume]
"""
import argparse
import json
import os
import re
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "analysis"))
import diff_inspect  # noqa: E402

# objdiff arg text: "rD, offset, rBase" (loads/stores), optionally with a
# trailing relocation symbol: "r11, 0x22, r11, lbl_82C926B8"
MEM_RE = re.compile(r",\s*(-?0x[0-9a-fA-F]+|-?\d+),\s*(r\d+)\b")
LDST_RE = re.compile(r"^(l[bhwfd]|st[bhwfd]|lm|stm)")


def flat_args(side):
    """Rebuild the pre-4.2 flat comparison-join spelling from `typed_args`.

    objdiff-cli 4.2.x prints the *display* spelling in `side["args"]`:
    d-form operands are parenthesised (`r11, 0x22(r11)`), COFF relocations
    carry an `@h`/`@l` suffix, and — the part that matters here — a
    relocation that is not part of the printed operand is DROPPED from the
    string entirely. MEM_RE below and the `'global'` class both read the old
    flat join (`r11, 0x22, r11, lbl_82C926B8`), which is exactly
    `", ".join(typed_args)`: the paren goes away AND the dropped relocation
    comes back as a trailing token. Rebuild rather than "tolerate parens",
    because tolerating parens cannot recover the relocation — it is not in
    the display string at all.

    Note the one shape this does NOT turn into a 'global': a symbolic
    displacement (`lwz r11, lbl_X@l(r30)`, typed_args
    Register/Symbol/Register) rebuilds to `r11, lbl_X, r30`, where MEM_RE
    finds no integer offset and mem_operand_class returns None. That is the
    pre-4.2 behaviour too, preserved deliberately.
    """
    if not side:
        return ""
    out = []
    for a in side.get("typed_args") or []:
        t, v = a.get("type"), a.get("value")
        if t == "Signed" and isinstance(v, int):
            out.append(("-0x%x" % -v) if v < 0 else "0x%x" % v)
        elif t in ("Unsigned", "BranchDest") and isinstance(v, int):
            out.append("0x%x" % v)
        else:
            out.append(str(v))
    return ", ".join(out)


def mem_operand_class(side):
    """Classify an instruction side's memory operand.

    Returns one of: 'stack' (r1 base), 'frame' (r31/r30 — ambiguous, often
    frame copy but can be `this`), 'global' (trailing reloc symbol => member
    of a global object), 'struct' (any other base reg), or None (no memory
    operand / not a load-store).
    """
    if not side:
        return None
    op = (side.get("opcode") or "").strip()
    if not LDST_RE.match(op):
        return None
    args = flat_args(side)
    m = MEM_RE.search(args)
    if not m:
        return None
    # A relocation symbol after the base reg means global+offset addressing.
    tail = args[m.end():].strip()
    if tail.startswith(","):
        return "global"
    base = m.group(2)
    if base == "r1":
        return "stack"
    if base in ("r31", "r30"):
        return "frame"
    return "struct"


def sweep_one(entry, project_dir):
    sym = entry["sym"]
    unit = entry.get("unit")
    rec = {
        "sym": sym,
        "unit": unit,
        "pct": entry.get("pct"),
        "size": entry.get("size"),
        "demangled": entry.get("demangled", ""),
        "status": "ok",
    }
    try:
        json_path = diff_inspect.run_objdiff_for_symbol(
            sym, project_dir=project_dir, unit=unit)
    except SystemExit:
        rec["status"] = "objdiff_failed"
        return rec
    except Exception as e:  # noqa: BLE001
        rec["status"] = f"error: {e}"
        return rec

    try:
        with open(json_path) as f:
            data = json.load(f)
    except Exception as e:  # noqa: BLE001
        rec["status"] = f"json_error: {e}"
        return rec

    instrs = data.get("instructions", [])
    reg_swaps, offset_diffs, symbol_diffs, branch_diffs = \
        diff_inspect.parse_breakdowns(instrs)

    inserts = sum(1 for i in instrs if i.get("match_type") == "insert")
    deletes = sum(1 for i in instrs if i.get("match_type") == "delete")

    deltas = {}
    class_deltas = {}   # {class: {delta: count}} for struct/global/frame
    examples = {}
    for idx, tv, bv, delta in offset_diffs:
        key = str(int(delta))
        deltas[key] = deltas.get(key, 0) + 1
        ins = instrs[idx] if idx < len(instrs) else {}
        t = ins.get("target") or {}
        b = ins.get("base") or {}
        cls = mem_operand_class(t) or mem_operand_class(b)
        if cls and cls != "stack":
            class_deltas.setdefault(cls, {})
            class_deltas[cls][key] = class_deltas[cls].get(key, 0) + 1
        ex = examples.setdefault(key, [])
        if len(ex) < 4:
            ex.append({
                "idx": idx,
                "cls": cls or "?",
                "tgt": diff_inspect.fmt_instr(t).strip(),
                "src": diff_inspect.fmt_instr(b).strip(),
            })

    rec.update({
        "n_offset_diffs": len(offset_diffs),
        "deltas": deltas,
        "class_deltas": class_deltas,
        "examples": examples,
        "reg_swaps": len(reg_swaps),
        "symbol_diffs": len(symbol_diffs),
        "branch_diffs": len(branch_diffs),
        "inserts": inserts,
        "deletes": deletes,
    })
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pool")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--project-dir", default=".")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--resume", action="store_true",
                    help="skip syms already present in the output file")
    args = ap.parse_args()

    with open(args.pool) as f:
        pool = json.load(f)
    if args.limit:
        pool = pool[:args.limit]

    done = {}
    if args.resume and os.path.exists(args.out):
        with open(args.out) as f:
            done = {r["sym"]: r for r in json.load(f)}

    results = list(done.values())
    t0 = time.time()
    for i, entry in enumerate(pool):
        if entry["sym"] in done:
            continue
        rec = sweep_one(entry, args.project_dir)
        results.append(rec)
        if (i + 1) % 20 == 0 or i == len(pool) - 1:
            with open(args.out, "w") as f:
                json.dump(results, f, indent=1)
            el = time.time() - t0
            print(f"[{i+1}/{len(pool)}] {el:.0f}s elapsed", flush=True)

    with open(args.out, "w") as f:
        json.dump(results, f, indent=1)

    def nonstack_count(r):
        cd = r.get("class_deltas") or {}
        return sum(sum(h.values()) for c, h in cd.items()
                   if c in ("struct", "global"))

    flagged = [r for r in results if nonstack_count(r) >= 1]
    print(f"Done: {len(results)} swept, {len(flagged)} with struct/global "
          f"offset diffs (layout-drift candidates)")


# --- selftest -------------------------------------------------------------
# EVERY fixture below is a verbatim instruction side captured from live
# `objdiff-cli diff -f json --include-instructions` (4.2.3, 0fd82159607c) over
# rb3-xenon build/45410914 objects. Hand-authored fixtures are what let this
# parser sit dead for six months while a green battery certified it, so do not
# "simplify" these into invented strings -- re-capture them from the CLI.
_SELFTEST_SIDES = [
    # (expected mem_operand_class, side)
    ("stack", {"opcode": "stw", "args": "r12, -0x8(r1)", "typed_args": [
        {"type": "Register", "value": "r12"}, {"type": "Signed", "value": -8},
        {"type": "Register", "value": "r1"}]}),
    ("struct", {"opcode": "lwz", "args": "r10, 0x0(r11)", "typed_args": [
        {"type": "Register", "value": "r10"}, {"type": "Signed", "value": 0},
        {"type": "Register", "value": "r11"}]}),
    ("frame", {"opcode": "lwz", "args": "r11, 0x14(r31)", "typed_args": [
        {"type": "Register", "value": "r11"}, {"type": "Signed", "value": 20},
        {"type": "Register", "value": "r31"}]}),
    # THE CHANNEL RULER-I/J DELETED: the relocation is nowhere in `args`, so
    # this can only be classified 'global' via typed_args.
    ("global", {"opcode": "lwz", "args": "r10, 0x0(r28)", "typed_args": [
        {"type": "Register", "value": "r10"}, {"type": "Signed", "value": 0},
        {"type": "Register", "value": "r28"},
        {"type": "Symbol", "value": "lbl_82E00B88"}]}),
    # symbolic displacement: no integer offset -> not a class this tool models
    (None, {"opcode": "lwz", "args": "r11, lbl_82E00B8C@l(r30)", "typed_args": [
        {"type": "Register", "value": "r11"},
        {"type": "Symbol", "value": "lbl_82E00B8C"},
        {"type": "Register", "value": "r30"}]}),
    # not a load/store
    (None, {"opcode": "addi", "args": "r3, r1, 0x50", "typed_args": [
        {"type": "Register", "value": "r3"}, {"type": "Register", "value": "r1"},
        {"type": "Signed", "value": 80}]}),
]


def _selftest():
    bad = 0
    for want, side in _SELFTEST_SIDES:
        got = mem_operand_class(side)
        if got != want:
            bad += 1
            print(f"FAIL {side['opcode']:5s} {side['args']!r}: "
                  f"want {want!r}, got {got!r}")
    # flat_args must be an exact round-trip when there is nothing to restore
    plain = _SELFTEST_SIDES[0][1]
    if flat_args(plain) != "r12, -0x8, r1":
        bad += 1
        print(f"FAIL flat_args round-trip: {flat_args(plain)!r}")
    print(f"selftest: {len(_SELFTEST_SIDES) + 1 - bad}/"
          f"{len(_SELFTEST_SIDES) + 1} passed")
    return 1 if bad else 0


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(_selftest())
    main()
