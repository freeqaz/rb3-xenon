#!/usr/bin/env python3
"""NEGATIVE CONTROL for report_result's COMPLETE admission gate (lane W36).

WHY THIS EXISTS
---------------
`DecompMCPServer._report_result` refuses to record COMPLETE for a function
whose BASE symbol is empty (`base_size == 0`, i.e. our source has no body for
it). That guard is the ONLY admission gate into the COMPLETE verdict in this
project.

⛔⛔ IT HAD NEVER FIRED. Measured 2026-08-17: the guard shelled out to
`objdiff-cli diff` WITHOUT `-f json`, so objdiff emitted its default MARKDOWN,
`stdout.find("{")` returned **-1** on every call, and — because there was no
`else` branch — execution fell straight through and recorded COMPLETE. Three
fail-opens were stacked: the missing `-f json`, the missing `else`, and a bare
`except Exception: pass` commented *"If check fails, allow the report
through"*.

Corroboration that it never once rejected anything:
  * `is_stub = 1` is written in exactly one place in the tree — inside this
    guard — and the column reads **0 on all 86,675 rows** of `decomp.db`.
  * `scripts/reset_false_complete.py` exists solely to undo *"false COMPLETE
    functions caused by base_size=0"* at scale, i.e. the damage is on record.

★ A GUARD THAT CANNOT REJECT IS WORSE THAN NO GUARD, because it is believed.
  So this file does not check that the guard passes; it checks that the guard
  DISCRIMINATES — it must REJECT a real stub and still ADMIT a real function.

HOW IT AVOIDS THE FAILURE MODE IT IS TESTING FOR
------------------------------------------------
  * It drives the REAL `_report_result` coroutine, not a re-implementation of
    its logic. Testing a copy of a guard is how the original defect survived.
  * Both control symbols are DISCOVERED from the live tree (by actually asking
    objdiff for `base_size`), never hardcoded, so the test cannot rot into
    agreement with a changed tree.
  * ⛔ VACUITY GUARD: `all([])` is True, so if either control population comes
    up empty this REFUSES (exit 2) instead of printing a confident green.
  * `--self-break` restores the original defect (strips `-f json`) and asserts
    the test then FAILS. A control that cannot go red proves nothing.
  * It runs against a COPY of `decomp.db`; it never mutates the real one.

Usage:
    python3 scripts/orchestrator/complete_guard_selftest.py [--project-dir .]
    python3 scripts/orchestrator/complete_guard_selftest.py --self-break
"""

import argparse
import asyncio
import json
import shutil
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "scripts" / "analysis"))

MIN_SCAN = 400          # rows to probe when hunting for controls


def find_controls(root, limit=MIN_SCAN, in_db=None):
    """(stub_symbols, real_symbols) discovered by asking objdiff for base_size.

    `in_db` is a predicate: a control absent from decomp.db never reaches the
    guard at all, so it must not be SELECTED as a control -- otherwise the test
    would be measuring the database rather than the gate.
    """
    from ruler import resolve_ruler
    args = list(resolve_ruler(str(root)).args)
    rep = json.load(open(Path(root) / "build" / "45410914" / "report.json"))
    I = lambda x: int(x or 0)
    F = lambda x: float(x or 0.0)

    cands = []
    for u in rep.get("units", []):
        for f in (u.get("functions") or []):
            n = f.get("name", "")
            if n.startswith(("fn_", "lbl_", "jumptable_", "data_")):
                continue
            if I(f.get("size")) <= 0:
                continue
            # A stub lives among the unpaired rows; a real function among the
            # matched ones. Probe both ends so neither population is assumed.
            cands.append((F(f.get("fuzzy_match_percent")), n))
    cands.sort(key=lambda x: x[0])

    stubs, reals = [], []
    probe = cands[:limit] + cands[-limit:]
    for _fz, n in probe:
        if len(stubs) >= 2 and len(reals) >= 2:
            break
        if in_db is not None and not in_db(n):
            continue
        r = subprocess.run(
            [str(Path(root) / "bin" / "objdiff-cli"), "diff", "-p", str(root),
             n, *args, "-f", "json"],
            capture_output=True, text=True, timeout=60)
        j = r.stdout.find("{")
        if j < 0:
            continue
        try:
            bs = int(json.loads(r.stdout[j:]).get("base_size", 0) or 0)
        except Exception:
            continue
        if bs == 0 and len(stubs) < 2:
            stubs.append(n)
        elif bs > 0 and len(reals) < 2:
            reals.append(n)
    return stubs, reals


def run_case(server, symbol):
    """(admitted: bool, text: str) from the REAL handler."""
    out = asyncio.run(server._report_result({
        "symbol": symbol, "status": "complete", "percent": 100.0,
        "notes": "W36 complete-guard selftest", "model": "selftest",
    }))
    text = "\n".join(getattr(t, "text", "") for t in out)
    admitted = not text.startswith(("Cannot mark as COMPLETE", "REFUSED"))
    return admitted, text


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-dir", default=str(ROOT))
    ap.add_argument("--self-break", action="store_true",
                    help="restore the original defect (strip -f json) and "
                         "assert this selftest FAILS")
    a = ap.parse_args()
    root = Path(a.project_dir).resolve()

    print("COMPLETE-GUARD SELFTEST (report_result base_size==0 rejection)")
    print(f"  project: {root}")

    if a.self_break:
        print("\n⚠ --self-break: re-introducing the ORIGINAL defect "
              "(objdiff-cli called without `-f json`).")
        print("  The guard must then admit a stub, and this test must FAIL.")

    import mcp_server as ms

    # ⚠ `decomp.db` is gitignored, so a WORKTREE does not carry one. Fall back
    # to the real repository's copy (resolved via git, never a relative guess —
    # a sibling-relative path silently vanishes in a worktree, and the failure
    # is shaped like a legitimate "not applicable" rather than an error).
    src_db = Path(root) / "decomp.db"
    if not src_db.exists():
        try:
            common = subprocess.run(
                ["git", "-C", str(root), "rev-parse", "--git-common-dir"],
                capture_output=True, text=True, timeout=30).stdout.strip()
            cand = (Path(root) / common).resolve().parent / "decomp.db"
            if cand.exists():
                src_db = cand
        except Exception:
            pass
    if not src_db.exists():
        print(f"\nREFUSED (exit 2): no decomp.db found (looked at {src_db}). "
              f"The guard writes to it,\n  so a run without one would test "
              f"nothing.")
        return 2
    print(f"  db (copied, never mutated in place): {src_db}")

    tmp = Path(tempfile.mkdtemp(prefix="w36guard-"))
    db = tmp / "decomp.db"
    shutil.copy(src_db, db)
    # ⚠ record_attempts=True is the LIVE default and is REQUIRED here: the whole
    # guard sits inside `if symbol and self.record_attempts:`, so constructing
    # the server with False skips it entirely and every control would "pass"
    # admission for a reason that has nothing to do with the guard. (This test
    # made exactly that mistake once. Noted also as a latent hazard: nothing in
    # the tree passes --no-record-attempts today, but if anything ever does,
    # the COMPLETE gate silently stops running.)
    server = ms.DecompMCPServer(db_path=str(db), record_attempts=True)
    server.project_root = root

    # ⛔ A control absent from decomp.db never reaches the guard at all
    # (`get_function_by_symbol` returns None and the handler records the result
    # unconditionally), which would look IDENTICAL to "the guard admitted it".
    # So DB membership is a SELECTION criterion, not a post-hoc excuse.
    from database import get_function_by_symbol
    print("\nDiscovering controls from the live tree (never hardcoded)...")
    stubs, reals = find_controls(
        root, in_db=lambda s: bool(get_function_by_symbol(s, db_path=str(db))))
    print(f"  base_size==0 (must be REJECTED): {len(stubs)} -> {stubs}")
    print(f"  base_size >0 (must be ADMITTED): {len(reals)} -> {reals}")

    # ⛔ VACUITY GUARD -- the whole point. `all([])` is True.
    if not stubs or not reals:
        print("\nREFUSED (exit 2): could not discover BOTH a stub control and a "
              "real-function control.\n  A pass over an empty population is the "
              "`all([])` failure mode, not a result.")
        shutil.rmtree(tmp, ignore_errors=True)
        return 2

    # ★★ THE BREAK IS APPLIED **AFTER** CONTROL DISCOVERY, AND THAT ORDERING IS
    #    LOAD-BEARING. Spelled the obvious way -- patch first, then discover --
    #    the break also disables the DISCOVERY probe (which asks objdiff for
    #    `base_size` and therefore also needs `-f json`), the control population
    #    comes back EMPTY, and the run trips the VACUITY REFUSAL instead of
    #    producing the red it exists to demonstrate. Measured here on the first
    #    attempt: `0 -> []` on both populations, exit 2.
    #    W29 recorded exactly this hazard for the size selftest ("a self-break
    #    that cannot break is the disease one level up") and it reproduced here
    #    verbatim in a different tool. The general rule: A NEGATIVE CONTROL MUST
    #    NOT BE ABLE TO POISON ITS OWN POPULATION.
    if a.self_break:
        _orig = subprocess.run

        def _broken(cmd, *args_, **kw):
            if (isinstance(cmd, list) and cmd and "objdiff-cli" in str(cmd[0])
                    and "-f" in cmd):
                i = cmd.index("-f")
                cmd = cmd[:i] + cmd[i + 2:]      # strip `-f json` -> markdown
            return _orig(cmd, *args_, **kw)
        ms.subprocess.run = _broken

    print("\nRESULTS")
    ok = True
    for s in stubs:
        admitted, text = run_case(server, s)
        good = not admitted
        ok &= good
        print(f"  {'ok  ' if good else 'FAIL'}  REJECT  {s[:56]:<56} "
              f"{'rejected' if not admitted else 'ADMITTED (guard is open)'}")
    for s in reals:
        admitted, text = run_case(server, s)
        good = admitted
        ok &= good
        print(f"  {'ok  ' if good else 'FAIL'}  ADMIT   {s[:56]:<56} "
              f"{'admitted' if admitted else 'REFUSED (guard over-blocks)'}")

    n_stub = sqlite3.connect(db).execute(
        "SELECT COUNT(*) FROM functions WHERE is_stub = 1").fetchone()[0]
    print(f"\n  is_stub=1 rows written by the guard during this run: {n_stub} "
          f"(expect {len(stubs)} when the guard works)")
    shutil.rmtree(tmp, ignore_errors=True)

    if a.self_break:
        if ok:
            print("\nSELF-BREAK FAILED: the defect was re-introduced and the "
                  "selftest STILL PASSED.\n  It cannot go red and proves "
                  "nothing. Treat it as broken.")
            return 1
        print("\nSELF-BREAK OK: with `-f json` stripped this test goes RED, so "
              "it discriminates rather\n  than confirming whatever it is "
              "pointed at.")
        print("  ⚠ Note the SHAPE of the red, which is the repair's own "
              "signature: the guard now fails\n    CLOSED, so a broken probe "
              "makes it REFUSE REAL FUNCTIONS (over-block). Before the repair "
              "the\n    same broken probe made it ADMIT EVERY STUB "
              "(over-admit) — silently, which is why it\n    survived for "
              "months. Over-blocking announces itself; over-admitting does not.")
        return 0
    if not ok:
        print("\nFAIL: the COMPLETE guard does not discriminate.")
        return 1
    print("\nPASS: the guard REJECTS every stub control and ADMITS every real "
          "function.\n  Run with --self-break to see it go red.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
