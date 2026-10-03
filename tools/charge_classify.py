#!/usr/bin/env python3
"""Charge classifier for sub-100 rows: what does the graded ruler actually charge?

This is W16-NA's classifier (`~/tmp/w16na/cls.py`, copied verbatim by W16-NP's
`diffall.py`), committed, with one correction (lane W16-OW, 2026-10-03):

    A differing *symbol* argument whose TARGET (retail, left-side) name is a
    splitter placeholder (fn_/lbl_/jumptable_/code_/data_/bss_/rdata_/vftable_
    + hex) or an MSVC `$`-label is NOT charged under `functionRelocDiffs=
    name_check` -- objdiff-core `reloc_eq` returns true for it
    (diff/code.rs, `is_placeholder_symbol_name` / `is_compiler_local_label`).

The old classifier counted every differing symbol argument, so a register-only
row whose only symbol differences were forgiven placeholders was filed
NAME+REG ("relocation name + register") and no register sweep ever saw it
(W16-OV, CAMPAIGN_STATE_2026-10-03 §4.1(3): 33 rows / 17,256 B whole binary).
Forgiven symbol args are now counted under `symbol_forgiven` and ignored by
`classify()`.

Usage:
    python3 tools/charge_classify.py <worktree> [--min-fuzzy 0] [--out cls.json]
        [--control]            # re-diff every row the correction moved at
                               # functionRelocDiffs=none and require equal fuzzy,
                               # plus a negative control on real-name rows
    python3 tools/charge_classify.py --selftest

The population is every paired row (target and base both present) with
min_fuzzy <= fuzzy_match_percent < 100 in a unit with a source_path, read
from <worktree>/build/45410914/report.json.
"""
import argparse
import collections
import json
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

PLACEHOLDER_PREFIXES = ("fn_", "lbl_", "jumptable_", "code_", "data_", "bss_", "rdata_", "vftable_")
_HEX_ = re.compile(r"^[0-9A-Fa-f_]+$")


def is_placeholder(name):
    """Mirror of objdiff-core `is_placeholder_symbol_name`."""
    if name.startswith("_"):
        name = name[1:]
    for p in PLACEHOLDER_PREFIXES:
        if name.startswith(p):
            rest = name[len(p):]
            return bool(rest) and bool(_HEX_.match(rest))
    return False


def is_forgiven_target(name):
    """True when name_check never charges a symbol arg with this TARGET name."""
    return is_placeholder(name) or (name.startswith("$") and len(name) > 1)


def arg_kinds(instructions):
    """Count charge kinds over a diff's instructions (W16-NA rules + the correction)."""
    mt = collections.Counter()
    ak = collections.Counter()
    for i in instructions:
        t = i["match_type"]
        if t == "equal":
            continue
        mt[t] += 1
        if t != "diff_arg":
            continue
        tg = i.get("target") or {}
        regs = [a.get("value") for a in tg.get("typed_args", []) if a.get("type") == "Register"]
        for a in (i.get("diff_breakdown") or {}).get("arguments", []):
            k = a.get("arg_type")
            if k == "immediate" and "r1" in regs:
                k = "imm_stack"
            if k == "symbol" and is_forgiven_target(str((a.get("target") or {}).get("value", ""))):
                k = "symbol_forgiven"
            ak[k] += 1
    return mt, ak


def classify(mt, ak, legacy=False):
    """W16-NA cls.py classify(). legacy=True reproduces the uncorrected filing."""
    if not mt:
        return "NO_CHARGE"
    if mt.get("insert") or mt.get("delete"):
        return "STRUCT_INSDEL"
    other = set(mt) - {"diff_arg"}
    if other:
        return "OPCODE:" + ",".join(sorted(other))
    ak = dict(ak)
    forgiven = ak.pop("symbol_forgiven", 0)
    if legacy and forgiven:
        ak["symbol"] = ak.get("symbol", 0) + forgiven
    ks = set(ak)
    if not ks:
        return "FORGIVEN_ONLY"  # every differing arg is a forgiven placeholder
    if "immediate" in ks:
        return "IMMEDIATE"
    if "imm_stack" in ks and ks - {"imm_stack", "register"} == set():
        return "STACK_REG"
    if ks <= {"symbol"}:
        return "NAME_ONLY"
    if "symbol" in ks and ks <= {"symbol", "register", "imm_stack"}:
        return "NAME+REG"
    if ks <= {"register"}:
        return "REG_ONLY"
    return "ARG_OTHER:" + ",".join(sorted(ks))


def diff_row(wt, unit, name, ruler=None):
    cmd = [wt + "/bin/objdiff-cli", "diff", "-p", wt, "-u", unit, name, "-f", "json",
           "--include-instructions", "-o", "-"]
    if ruler:
        cmd += ["-c", "functionRelocDiffs=" + ruler]
    p = subprocess.run(cmd, capture_output=True, text=True, cwd=wt)
    try:
        return json.loads(p.stdout)
    except Exception:
        return None


def population(wt, min_fuzzy):
    rep = json.load(open(wt + "/build/45410914/report.json"))
    out = []
    for u in rep["units"]:
        sp = (u.get("metadata") or {}).get("source_path")
        if not sp:
            continue
        for f in u.get("functions", []):
            fz = float(f.get("fuzzy_match_percent", 0))
            if fz < 100 and fz >= min_fuzzy and fz > 0:
                out.append(dict(unit=u["name"], src=sp, name=f["name"], size=int(f.get("size", 0)),
                                fuzzy=fz, mpn=float(f.get("match_percent_normalized", 0))))
    return out


def run(wt, rows, jobs=16):
    def one(r):
        d = diff_row(wt, r["unit"], r["name"])
        r = dict(r)
        if d is None:
            r["cls"] = r["legacy"] = "ERR"
            return r
        if not d.get("base_size"):
            r["cls"] = r["legacy"] = "UNPAIRED"
            return r
        mt, ak = arg_kinds(d["instructions"])
        r.update(mt=dict(mt), ak=dict(ak), graded=d.get("fuzzy_match_percent"),
                 cls=classify(mt, ak), legacy=classify(mt, ak, legacy=True))
        return r
    with ThreadPoolExecutor(jobs) as ex:
        return list(ex.map(one, rows))


def control(wt, res):
    """Moved rows must read the same fuzzy at `none` (their symbol args are uncharged).
    Negative control: NAME+REG rows that still carry a real-name charge must NOT all
    read the same -- otherwise the control could not have failed."""
    moved = [r for r in res if r.get("legacy") != r.get("cls")]
    real = [r for r in res if r.get("cls") in ("NAME+REG", "NAME_ONLY")]
    def nf(r):
        d = diff_row(wt, r["unit"], r["name"], "none")
        return None if d is None else d.get("fuzzy_match_percent")
    with ThreadPoolExecutor(16) as ex:
        mv = list(ex.map(nf, moved))
        rl = list(ex.map(nf, real[:60]))
    bad = [(r["name"], r["graded"], n) for r, n in zip(moved, mv) if n is None or abs(n - r["graded"]) > 1e-4]
    differ = sum(1 for r, n in zip(real[:60], rl) if n is not None and abs(n - r["graded"]) > 1e-4)
    print(f"[control] moved rows: {len(moved)}; none==graded on {len(moved) - len(bad)}/{len(moved)}")
    for b in bad:
        print("   MISMATCH", b)
    print(f"[control] negative: {differ}/{min(60, len(real))} real-name rows read differently at none")
    if bad or (real and differ == 0):
        print("[control] FAIL")
        return 1
    print("[control] PASS")
    return 0


def selftest():
    ok = True
    for n, want in [("fn_82345678", True), ("lbl_829fc4a0", True), ("_bss_00456208", True),
                    ("vftable_82000010", True), ("fn_helper", False), ("fn_", False),
                    ("?Poll@CharServoBone@@UAAXXZ", False), ("$SG3489", True), ("$", False),
                    ("__real@3f800000", False)]:
        if is_forgiven_target(n) != want:
            print("selftest FAIL", n); ok = False
    ins = [{"match_type": "diff_arg", "target": {"typed_args": [{"type": "Register", "value": "r10"}]},
            "diff_breakdown": {"arguments": [
                {"arg_type": "register"},
                {"arg_type": "symbol", "target": {"value": "lbl_8201C818"}}]}}]
    mt, ak = arg_kinds(ins)
    if classify(mt, ak) != "REG_ONLY" or classify(mt, ak, legacy=True) != "NAME+REG":
        print("selftest FAIL placeholder correction", classify(mt, ak)); ok = False
    ins[0]["diff_breakdown"]["arguments"][1]["target"]["value"] = "?TheUI@@3PAVUIManager@@A"
    mt, ak = arg_kinds(ins)
    if classify(mt, ak) != "NAME+REG":
        print("selftest FAIL real name must stay NAME+REG"); ok = False
    print("selftest", "PASSED" if ok else "FAILED")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("worktree", nargs="?")
    ap.add_argument("--min-fuzzy", type=float, default=0.0)
    ap.add_argument("--out")
    ap.add_argument("--control", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    wt = a.worktree.rstrip("/")
    res = run(wt, population(wt, a.min_fuzzy))
    if a.out:
        json.dump(res, open(a.out, "w"))
    c = collections.Counter(); b = collections.Counter()
    for x in res:
        c[x["cls"]] += 1; b[x["cls"]] += x["size"]
    for k in sorted(c, key=lambda k: -b[k]):
        print(f"{k:40s} {c[k]:5d} {b[k]:9d}")
    moved = [x for x in res if x.get("legacy") != x.get("cls")]
    tr = collections.Counter((x["legacy"], x["cls"]) for x in moved)
    print(f"\nrows refiled by the placeholder correction: {len(moved)} / {sum(x['size'] for x in moved)} B")
    for (o, n), k in tr.most_common():
        print(f"   {o} -> {n}: {k}")
    return control(wt, res) if a.control else 0


if __name__ == "__main__":
    sys.exit(main())
