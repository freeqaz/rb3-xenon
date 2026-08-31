#!/usr/bin/env python3
"""Deliberately break the pairing, and require test_obj_pairing.py to notice.

Run: python3 -B scripts/sabotage_obj_pairing.py        (exit 0 = every
                                                        sabotage was caught)

Why this file exists at all
---------------------------
A passing test suite proves nothing about whether the suite CAN fail.  This
repo has shipped guards that passed on deliberately broken code, one of them
written specifically to catch the bug it then missed, so "the tests are green"
is not evidence until each test has been watched going red for the right
reason.  Each entry below names the test that MUST fail, and the harness fails
if some *other* test fails instead -- a sabotage caught by the wrong assertion
is a coincidence, not coverage.

⚠ THE `.pyc` TRAP, which makes this whole harness lie if ignored
----------------------------------------------------------------
Every sabotage here patches a source file, runs the suite, and restores the
file -- typically well inside one second.  CPython's bytecode cache is keyed on
`(source mtime, source size)` at 1-second granularity, so a byte-length-
PRESERVING edit applied and reverted within the same second leaves both fields
unchanged: the interpreter loads the STALE `.pyc`, the sabotage never executes,
and the harness cheerfully reports that a test which never saw the bug caught
it.  Three defences, all of them on:

  * every subprocess runs with `-B` and `PYTHONDONTWRITEBYTECODE=1`;
  * any `__pycache__` under `scripts/` is removed before each run;
  * `_selftest_pyc_trap` below proves the mechanism is live by asserting that
    a sabotage which changes nothing is NOT reported as caught.

Run this three times in a row before believing it.  The class is intermittent
by construction.
"""

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
ENV = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")

#: Everything test_obj_pairing.py touches.  The harness runs against a COPY of
#: these, never the checkout -- it rewrites source files mid-run, and this repo
#: is a shared working surface with a build fleet on it.  A harness that can
#: leave a patcher sabotaged because it was interrupted is not acceptable here.
SANDBOXED = ("obj_pairing.py", "obj_guard_patcher.py", "test_obj_pairing.py")

_SANDBOX = tempfile.mkdtemp(prefix="sabotage-pairing-")
SCRIPTS = Path(_SANDBOX) / "scripts"
SCRIPTS.mkdir(parents=True)
for _n in SANDBOXED:
    shutil.copy(REPO / "scripts" / _n, SCRIPTS / _n)
SUITE = SCRIPTS / "test_obj_pairing.py"

#: (label, file, find, replace, test that must go red)
SABOTAGES = [
    (
        "pairing falls back to relpath keying, the original bug",
        "obj_pairing.py",
        """        declared = self.declared.get(base_rel)
        if declared:
            return list(declared)""",
        """        declared = None  # SABOTAGE: ignore objdiff.json entirely
        if declared:
            return list(declared)""",
        "test_relpath_pairing_goes_silently_blind",
    ),
    (
        "multi-target object collapses to a single arbitrary target",
        "obj_pairing.py",
        """            if t_rel not in seen:
                seen.append(t_rel)""",
        """            if not seen:  # SABOTAGE: first unit wins, rest discarded
                seen.append(t_rel)""",
        "test_multi_target_object_yields_every_declared_target",
    ),
    (
        "the incompleteness assertion is removed",
        "obj_pairing.py",
        """    bad = cov["declared_unpaired"]
    if bad:""",
        """    bad = []  # SABOTAGE: never report a lost object
    if bad:""",
        "test_incomplete_coverage_names_the_object_it_lost",
    ),
    (
        "'examined zero things' is allowed to read as success",
        "obj_pairing.py",
        """    if cov["objects_declared"] < min_declared:""",
        """    if False:  # SABOTAGE: a vacuous pairing is fine now""",
        "test_examining_nothing_is_not_success",
    ),
    (
        "an unreadable objdiff.json is treated as an empty-but-valid one",
        "obj_pairing.py",
        """    if not cov.get("config_readable"):""",
        """    if False:  # SABOTAGE: no config is no problem""",
        "test_examining_nothing_is_not_success",
    ),
    (
        "the pairing invents a target for an undeclared object",
        "obj_pairing.py",
        """        if (self.obj_dir / base_rel).exists():
            return [base_rel]
        return []""",
        """        return [base_rel]  # SABOTAGE: pair against a file that may not exist""",
        "test_undeclared_object_falls_back_to_relpath_but_never_invents",
    ),
    (
        "the guard patcher goes back to relpath pairing",
        "obj_guard_patcher.py",
        """        orig_paths = pairing.target_paths_for(rel)""",
        """        orig_paths = [obj_dir / rel] if (obj_dir / rel).exists() else []""",
        "test_the_patcher_finds_a_patch_the_old_rule_could_not",
    ),
    (
        "the guard patcher stops writing (a silent no-op --apply)",
        "obj_guard_patcher.py",
        """    if apply:
        _write_preserving_mtime(decomp_path, decomp_data)""",
        """    if False:  # SABOTAGE: --apply writes nothing
        _write_preserving_mtime(decomp_path, decomp_data)""",
        "test_patch_is_actually_applied_and_mtime_preserved",
    ),
    (
        "the guard patcher bumps mtime, re-arming the ninja oscillation",
        "obj_guard_patcher.py",
        """def _write_preserving_mtime(path, data):""",
        """def _write_preserving_mtime(path, data):
    # SABOTAGE: drop the mtime restore
    with open(path, 'wb') as _f:
        _f.write(data)
    return""",
        "test_patch_is_actually_applied_and_mtime_preserved",
    ),
    (
        "the CLI --check swallows its own failure",
        "obj_pairing.py",
        """            print("FAIL[pairing]: %s" % e, file=sys.stderr)
            return 2""",
        """            print("FAIL[pairing]: %s" % e, file=sys.stderr)
            return 0  # SABOTAGE: report the failure, exit clean""",
        "test_cli_check_exits_nonzero_on_a_blind_pairing",
    ),
]


def _drop_pyc():
    for p in SCRIPTS.rglob("__pycache__"):
        shutil.rmtree(p, ignore_errors=True)


def run_suite():
    """Return (returncode, set of failing test method names)."""
    _drop_pyc()
    p = subprocess.run([sys.executable, "-B", str(SUITE), "-v"],
                       cwd=str(SCRIPTS.parent), capture_output=True, text=True,
                       env=ENV)
    failed = set()
    for line in (p.stderr + p.stdout).splitlines():
        # unittest -v prints "name (module) ... FAIL" / "... ERROR", and for
        # a test with a docstring the verdict lands on a later line -- so scan
        # the FAIL:/ERROR: summary headers instead, which are unambiguous.
        for tag in ("FAIL: ", "ERROR: "):
            if line.startswith(tag):
                failed.add(line[len(tag):].split(" ")[0])
    return p.returncode, failed


def _selftest_pyc_trap():
    """A no-op 'sabotage' must NOT be reported as caught.

    If this passes it means an edit that changes nothing still makes tests
    fail, i.e. the harness is measuring noise -- or that a stale `.pyc` made a
    REAL sabotage invisible in the other direction.  Either way the harness is
    not trustworthy and says so.
    """
    src = SCRIPTS / "obj_pairing.py"
    original = src.read_text()
    try:
        src.write_text(original)          # rewrite identical bytes, same second
        rc, failed = run_suite()
    finally:
        src.write_text(original)
    if rc != 0:
        print("SELFTEST FAILED: an identical rewrite made the suite red "
              f"({sorted(failed)}) -- the harness cannot distinguish a "
              "sabotage from noise.", file=sys.stderr)
        return False
    print("  selftest: identical rewrite leaves the suite green (as it must)")
    return True


def main():
    print(f"[sabotage] baseline: {SUITE.name} on unmodified sources")
    rc, failed = run_suite()
    if rc != 0:
        print(f"REFUSING: the suite is already red before any sabotage "
              f"({sorted(failed)}). Fix that first.", file=sys.stderr)
        return 1
    print("  baseline green")
    if not _selftest_pyc_trap():
        return 1

    caught = miscaught = missed = 0
    for label, fname, find, repl, expect in SABOTAGES:
        src = SCRIPTS / fname
        original = src.read_text()
        if find not in original:
            print(f"  UNAPPLICABLE  {label}\n"
                  f"      anchor not found in {fname} -- this sabotage has "
                  f"rotted and is silently testing nothing", file=sys.stderr)
            missed += 1
            continue
        try:
            src.write_text(original.replace(find, repl, 1))
            rc, failed = run_suite()
        finally:
            src.write_text(original)
        if rc == 0:
            print(f"  NOT CAUGHT    {label}", file=sys.stderr)
            missed += 1
        elif expect in failed:
            print(f"  caught        {label}  (by {expect})")
            caught += 1
        else:
            print(f"  WRONG TEST    {label}\n"
                  f"      expected {expect} to fail, got {sorted(failed)}",
                  file=sys.stderr)
            miscaught += 1

    total = len(SABOTAGES)
    print(f"\n[sabotage] {caught}/{total} caught by the intended test, "
          f"{miscaught} caught by the wrong one, {missed} not caught at all")
    _drop_pyc()
    shutil.rmtree(_SANDBOX, ignore_errors=True)
    return 0 if caught == total else 1


if __name__ == "__main__":
    sys.exit(main())
