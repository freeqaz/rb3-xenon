#!/usr/bin/env python3
"""Run this repo's Python test suites — the population that had no caller.

Why this exists
---------------
Measured 2026-08-17 on branch point ``ff22c440``: **nothing in this repo runs
any Python test.** 16 tracked test files, zero invocations. There is no
``[tool.pytest]`` section, no ``testpaths``, no tox/nox/Makefile target, and no
shell script or Python entry point that shells out to pytest. CI runs exactly
one selftest (``tools/source_category.py selftest``) and no pytest at all.

That is the same defect class as a vacuous assertion, approached from the other
side: the assertions here are fine, nobody ever asks them anything. This script
is the caller. Ported from decomp-synth's ``scripts/test_tools.py`` (landed
there as ``0bda4538``) and matching its contract, with one addition the game
repos need — see "Script mode" below.

Why a separate runner instead of a ``testpaths`` pin
----------------------------------------------------
1. **The baseline is not green.** Making a bare ``pytest`` red for every peer on
   day one gets the change reverted, not the tests fixed. This runner holds a
   checked-in list of the known-bad entries
   (``scripts/test_tools_known_bad.txt``) and fails only on a failure that is
   NOT on it, so pre-existing breakage is visible and inert while a NEW failure
   is loud.
2. **One pytest process per collection root.** Test directories that are not
   packages put each root on ``sys.path``, and same-named modules across roots
   then shadow each other. ``testpaths`` cannot express per-root processes.
3. **Some of these files are not pytest tests at all** (script mode, below).

Script mode
-----------
The decomp-synth original assumes every ``test_*.py`` is a pytest module. In the
game repos that is false: several files named ``test_*.py`` / ``*_test.py`` are
``main()``-style self-checks that pytest collects **zero** tests from (exit code
5). A naive lane calls those BROKEN, which is wrong — some of them are real
checks (``scripts/test_certify_floor.py`` in dc3 builds a synthetic
``decomp.db`` and asserts against it). So there are three arms, not one:

* **pytest roots** — one pytest process each, red list diffed against the
  manifest.
* **SCRIPT_ARM** — run as ``python3 <file>``; a nonzero exit is the failure.
* **EXCLUDED** — files this lane deliberately does not run, each with a written
  reason, printed on every run. This is the escape hatch that must never be
  quiet: an excluded file is a file with no caller, which is the thing this lane
  exists to abolish.

Anti-staleness
--------------
The roots and the two tables are described by hand. Any tracked test file that
matches no root, no script-arm entry and no exclusion is reported as UNCOVERED
and exits non-zero — so adding a test directory is a two-line change here, not a
silent escape. A manifest entry that now PASSES is reported as STALE, so the
manifest cannot rot in the other direction either.

⚠ THAT CHECK HAD A BLIND SPOT, closed 2026-09-11 (lane TESTCI): it equated
"claimed by a root" with "run". Three tracked files were `main()`-style, so
pytest collected ZERO tests from each — the lane imported them (executing their
module-level side effects) and asked them nothing — while ``coverage_gaps()``
reported 0. A file that counts as covered while contributing nothing is worse
than one reported UNCOVERED: it is an escape hatch that does not print. A
``--collect-only`` sweep (~6 s) now reports those as HOLLOW and exits non-zero.

CI
--
``.github/workflows/build.yml`` runs this lane with ``--ci --strict-manifest``
immediately after the Build step. ``--strict-manifest`` is deliberate: it makes
a known-bad entry that has started passing FATAL, so the allow-list must shrink
as tests are fixed rather than rotting into a list nobody prunes. ``--ci``
applies ``CI_DESELECT``, the explicit list of tests that pass on a developer
tree and cannot pass in the container for environment reasons.

Placement is after Build because the lane is NOT build-independent — measured,
not assumed. On a tracked-files-only export of HEAD three tests fail that pass
on a built tree (they need ``build/<v>/report.json``, ``build.ninja``, or a
settled tree for ``patch_guard``), and ``scripts/test_split_guard.py`` exits 1
without ``build.ninja``.

⚠ RUNNING THE LANE TAKES THE BUILD LOCK. ``test_verdict_identity.py::
test_batch_check_does_not_select_it`` drives the real ``scripts/batch_check.py``,
which calls ``ensure_patched_tree(PROJECT_ROOT)`` at line 87 with the default
``build=True`` — i.e. it runs ninja. Measured: the orchestrator root took 87 s
on a tree that owed a build and 16 s once settled. That is harmless in CI and in
your own worktree, but do NOT run this lane in the shared main checkout while
other lanes are building.

Usage
-----
    python3 scripts/test_tools.py                 # whole lane
    python3 scripts/test_tools.py --root tools
    python3 scripts/test_tools.py --list
    python3 scripts/test_tools.py --strict-manifest   # stale entries are fatal
    python3 scripts/test_tools.py --ci                # apply CI_DESELECT
    python3 scripts/test_tools.py --no-script-arm     # pytest roots only
    python3 scripts/test_tools.py --no-hollow-check   # skip the collect sweep

Exit codes: 0 = no NEW failure; 1 = new failure, timeout, broken root, script
failure, uncovered file, or hollow file.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
MANIFEST = REPO_ROOT / "scripts" / "test_tools_known_bad.txt"

# ── collection roots ──────────────────────────────────────────────────────────
# One pytest process each. ``ignore`` keeps a parent root from swallowing a
# child root (or a script-arm file). ``timeout`` is a per-root wall-clock
# ceiling in seconds, sized generously against the baseline measured
# 2026-08-17 on this box; a root that hits it is reported as TIMEOUT, a FAILURE
# outcome distinct from a red test, because a hang is exactly the shape this
# lane must not silently absorb.
STATIC_ROOTS: list[dict] = [
    # scripts/ top level only — the subdirectories below are their own roots.
    {"path": "scripts", "timeout": 300,
     "ignore": ["scripts/harvest", "scripts/orchestrator",
                "scripts/unicorn", "scripts/unicorn_runner"]},
    {"path": "scripts/harvest", "timeout": 300},
    # test_verdict_identity.py measured ~51 s.
    {"path": "scripts/orchestrator", "timeout": 600},
    {"path": "scripts/unicorn_runner/tests", "timeout": 600},
    {"path": "tools", "timeout": 600},
]

# ── script mode ───────────────────────────────────────────────────────────────
# ``main()``-style self-checks: pytest collects 0 tests (rc=5), but running them
# as a program is a real check. Run as ``python3 <path>``; rc 0 is pass.
SCRIPT_ARM: list[dict] = [
    # The negative control for scripts/test_obj_pairing.py: it applies ten
    # deliberate defects to a SANDBOX COPY of obj_pairing.py /
    # obj_guard_patcher.py and requires the named test to go red for each. It
    # is not a pytest module (pytest collects 0 tests from it) and it is the
    # only thing in this repo that proves those tests can fail at all, so it
    # belongs here rather than being run by hand and then never again.
    #
    # Safe to run concurrently with a build fleet: it never writes to the
    # checkout. It is also why test_obj_pairing.py is worth trusting — see
    # that file's header for the `.pyc` staleness trap this class of harness
    # falls into.
    {"path": "scripts/sabotage_obj_pairing.py", "timeout": 300},
    # The negative control for scripts/orchestrator/test_project_dir_guard.py:
    # it restores the main-repo fallback (and six sibling defects) in an
    # extracted SANDBOX copy of mcp_server.py's guard region and requires the
    # named test to go red for each, plus a NULL arm that must break nothing.
    # Registered here for the same reason as its sibling above: it is the only
    # thing that proves those assertions can fail, and a control run once by
    # hand and never again is not a control.
    #
    # Safe next to a build fleet: it never writes to the checkout (the sandbox
    # is a fresh temp dir per arm) and it does not build.
    {"path": "scripts/sabotage_project_dir_guard.py", "timeout": 300},
    # Sabotage suite for scripts/verify_split_current.py: every GREEN is paired
    # with a RED produced by breaking the specific thing that GREEN covers, and
    # every RED is checked for the RIGHT REASON. Registered 2026-09-11 (lane
    # TESTCI): it is named test_*.py, so `coverage_gaps` counted it as covered
    # by the `scripts` root -- but pytest collects ZERO tests from it, so the
    # lane imported it and ran nothing. Measured: rc=0 in ~1 s on a configured
    # tree.
    #
    # Works against a scratch COPY of the repo's config and build metadata,
    # never the tree itself, so it is safe beside a build fleet. It DOES need
    # `build.ninja` (it locates the split edge there) and exits 1 with
    # `FATAL: could not find a split edge` without one -- loudly, not silently,
    # which is why it is safe to register rather than exclude. See the CI step
    # in .github/workflows/build.yml for why that forces placement after Build.
    {"path": "scripts/test_split_guard.py", "timeout": 300},
    # Fold-poisoning regression gate, fixtures are real retail bytes. Same
    # discovery as its sibling above: zero tests collected, so the lane was
    # importing it and asking it nothing.
    #
    # Measured environment-independent (rc=0 both on a built tree and on a
    # pristine tracked-files-only checkout), so it is safe anywhere in the
    # workflow.
    #
    # ⚠ DELIBERATE DUPLICATION: .github/workflows/build.yml ALSO runs this file
    # directly, twice (plain, then `--self-break`). That is not redundant
    # bookkeeping to clean up -- the `--self-break` leg is the proof the gate
    # can fail, which this runner does not perform, and the direct step keeps
    # working if this runner is ever removed. The overlap costs ~1 s.
    {"path": "tools/test_icf_fold_safe.py", "timeout": 300},
]

# ── deliberate exclusions ─────────────────────────────────────────────────────
# Every entry needs a reason. Printed on every run.
EXCLUDED: list[dict] = [
    # ⛔ STATE-DEPENDENT AGAINST THE LIVE SHARED TREE. This is the one tracked
    # test file in the repo that reads the REAL build directory
    # (`REPO / "build"`, line 56) and then MUTATES it: it plants sabotage into
    # `build/<v>/split_inputs.stamp`, drives
    # `patch_guard.ensure_patched_tree(REPO, build=False)` against it, and
    # restores in a `finally`. Its assertions are good -- it proves the guard
    # RAISES rather than merely warning, which is the right end of that
    # question -- but its subject is global mutable state.
    #
    # Registered here rather than in SCRIPT_ARM for two measured reasons:
    #
    #   1. Every existing SCRIPT_ARM entry advertises "never writes to the
    #      checkout", because this repo runs a fleet of concurrent lanes. This
    #      file writes to the shared build tree. Running the lane would then
    #      race any lane mid-build, and a `finally` does not survive SIGKILL.
    #   2. Its verdict depends on whether the tree happens to owe a build, so
    #      in CI it would depend on STEP ORDERING -- and three later steps in
    #      build.yml deliberately perturb the tree (the symbols.txt fixpoint
    #      guard forces its own re-split, and its `--self-break` leg plants a
    #      violation). A test whose colour depends on which guard ran first is
    #      not a regression signal.
    #
    # ⚠ It was ALREADY inert before this lane, silently: pytest collects zero
    # tests from it, so the `scripts` root imported it (running its
    # module-level `os.environ[SPLIT_WAIT_ENV] = "3"`) and called nothing.
    # Excluding it changes no coverage; it makes the absence VISIBLE, which is
    # the point of this list.
    #
    # ⇒ THE FIX IS TO POINT IT AT A FIXTURE, not to soften `patch_guard` --
    # the guard is correct and the test is the wrong end. `test_patch_state.py`
    # (26 tests) and `test_split_guard.py` both already build a scratch tree
    # and assert against that; this file is the odd one out and should follow
    # them. Not done here: it rewrites a test this lane does not own.
    {"path": "scripts/test_patch_guard_split_hook.py",
     "why": "mutates the LIVE shared build tree (plants into "
            "build/<v>/split_inputs.stamp) and its verdict depends on whether "
            "the tree owes a build -- flaky beside a build fleet, and "
            "order-dependent in CI. Fix = give it a fixture like "
            "test_patch_state.py does. Already collected 0 tests, so nothing "
            "is lost by excluding it."},
]

# ── CI deselections (`--ci` only) ─────────────────────────────────────────────
# Tests that pass on a DEVELOPER tree and cannot pass in the CI container, for
# a reason that is a property of the ENVIRONMENT rather than of the code. Each
# is deselected from the pytest run under `--ci`, and printed with its reason on
# every run, exactly like EXCLUDED.
#
# WHY THIS IS NOT A KNOWN-BAD ENTRY. The manifest is a single static list read
# in both environments, so an entry for a test that passes locally and fails in
# CI is STALE locally the moment it is added -- and `--strict-manifest` (which
# CI passes, so the allow-list cannot rot) would then make it fatal in the other
# direction. A deselected test is simply not judged anywhere, which is the
# honest shape for "green here, red there".
#
# ⚠ KEEP THIS LIST AT ZERO IF YOU CAN. Every entry is coverage CI does not have.
# Prefer fixing the environment gap over recording it.
CI_DESELECT: list[dict] = [
    # Asserts that every `<dir> / "bin" / "<name>"` in mcp_server.py names a
    # file that exists -- a real invariant on a developer tree, where
    # `bin/objdiff-cli` is a hand-made symlink onto the shared
    # ../objdiff/target/release build.
    #
    # That symlink is EXPLICITLY GITIGNORED (.gitignore:145 `/bin/objdiff-cli`,
    # verified with `git check-ignore -v`), and nothing in configure.py or
    # tools/project.py creates it -- the build resolves objdiff to an absolute
    # fork path or to build/tools/, never through `bin/`. So on a fresh
    # `actions/checkout` the directory is empty and this test fails with
    # `handler(s) reference bin/ executable(s) that do not exist:
    # ['objdiff-cli']` no matter where in the workflow it runs. Measured on a
    # tracked-files-only export of HEAD (lane TESTCI, 2026-09-11).
    #
    # It is deselected rather than "fixed" because the invariant it defends is
    # about the developer/MCP environment, which CI does not have and does not
    # need: nothing in build.yml runs mcp_server.py. Creating the symlink in CI
    # to make it green would be fabricating the precondition rather than
    # testing it.
    {"id": "scripts/orchestrator/test_advertised_tools.py::"
           "test_no_handler_shells_out_to_a_missing_repo_executable",
     "why": "needs bin/objdiff-cli, which is gitignored (.gitignore:145) and "
            "created by nothing in the build -- structurally absent on a fresh "
            "checkout. Passes on a developer tree; env, not code."},
]

TEST_FILE_RE = re.compile(r"(^|/)(test_[^/]*\.py|[^/]*_test\.py)$")

# This runner is itself named test_tools.py, so it matches TEST_FILE_RE. Once it
# was committed the lane reported ITSELF as an UNCOVERED tracked test file and
# exited 1 (measured 2026-08-17 — the anti-staleness check working correctly on
# the wrong input). It is also handed to pytest by any root that contains it,
# which collects zero tests from it. Excluded from both.
SELF = Path(__file__).resolve().relative_to(REPO_ROOT).as_posix()

# pytest short-summary lines. The id runs to the first " - " (pytest's separator
# before the exception's first line); parametrized ids contain spaces, so
# ``\S+`` would truncate them.
SUMMARY_RE = re.compile(r"^(FAILED|ERROR)\s+(.+?)(?:\s+-\s.*)?$")
COUNT_RE = re.compile(
    r"(\d+) (passed|failed|error|errors|skipped|xfailed|xpassed|deselected)")


def _strip_ansi(text: str) -> str:
    return re.sub(r"\x1b\[[0-9;]*m", "", text)


# ── discovery ─────────────────────────────────────────────────────────────────

def discover_roots() -> list[dict]:
    return [dict(r) for r in STATIC_ROOTS]


def tracked_test_files() -> list[str]:
    """Every TRACKED test file in the repo (untracked WIP is out of scope)."""
    try:
        out = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "ls-files"],
            capture_output=True, text=True, check=True).stdout
        files = out.splitlines()
    except (subprocess.CalledProcessError, FileNotFoundError):
        files = [p.relative_to(REPO_ROOT).as_posix()
                 for p in REPO_ROOT.rglob("*.py")]
    return sorted(f for f in files if TEST_FILE_RE.search(f) and f != SELF)


def _claimed_by_a_root(f: str, roots: list[dict]) -> bool:
    owners = [r for r in roots
              if f == r["path"] or f.startswith(r["path"].rstrip("/") + "/")]
    for r in owners:
        ignored = any(f == i or f.startswith(i.rstrip("/") + "/")
                      for i in r.get("ignore", []))
        if not ignored:
            return True
    return False


def coverage_gaps(roots: list[dict]) -> list[str]:
    """Tracked test files that nothing in this lane would run."""
    handled = ({e["path"] for e in SCRIPT_ARM} | {e["path"] for e in EXCLUDED})
    return [f for f in tracked_test_files()
            if f not in handled and not _claimed_by_a_root(f, roots)]


# ── known-bad manifest ────────────────────────────────────────────────────────

def load_manifest() -> list[str]:
    if not MANIFEST.is_file():
        return []
    entries = []
    for line in MANIFEST.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            entries.append(line)
    return entries


def manifest_match(entry: str, observed: str) -> bool:
    """An entry matches a failure id exactly, or covers a whole file/class.

    ``a/test_b.py`` covers ``a/test_b.py::test_c`` and its parametrizations;
    ``...::test_c`` covers ``...::test_c[case-1]``.
    """
    return (observed == entry
            or observed.startswith(entry + "::")
            or observed.startswith(entry + "["))


# ── running ───────────────────────────────────────────────────────────────────

def root_selection_args(root: dict) -> list[str]:
    """The --ignore/--deselect args for a root.

    Factored out so the real run and the hollow-coverage sweep below build the
    SAME selection. If the sweep could drift from the run it would report on a
    population the lane never executes, which is the vacuity this whole file
    exists to prevent.
    """
    args: list[str] = []
    ignores = list(root.get("ignore", []))
    if SELF.startswith(root["path"].rstrip("/") + "/"):
        ignores.append(SELF)
    # A script-arm or excluded file inside this root must not be collected:
    # pytest would report it as an error or as "no tests ran".
    for entry in SCRIPT_ARM + EXCLUDED:
        p = entry["path"]
        if p.startswith(root["path"].rstrip("/") + "/") and p not in ignores:
            ignores.append(p)
    for ig in ignores:
        args.append(f"--ignore={ig}")
    for ds in root.get("deselect", []):
        args.extend(["--deselect", ds])
    return args


def collected_files(root: dict, python: str) -> tuple[set[str], set[str]]:
    """(files pytest collects >=1 test from, files that ERRORED at collection).

    A `--collect-only` pass with the run's own selection. Cheap: the whole
    sweep measured ~6 s across all five roots (lane TESTCI, 2026-09-11).
    """
    cmd = [python, "-m", "pytest", "--collect-only", "-q",
           "--continue-on-collection-errors", "-p", "no:cacheprovider"]
    cmd += root_selection_args(root)
    cmd.append(root["path"])
    proc = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True)
    have: set[str] = set()
    errored: set[str] = set()
    for line in _strip_ansi(proc.stdout + proc.stderr).splitlines():
        line = line.strip()
        if "::" in line and not line.startswith(("FAILED", "ERROR", "<")):
            have.add(line.split("::", 1)[0])
        m = SUMMARY_RE.match(line)
        if m and line.startswith("ERROR"):
            errored.add(m.group(2).split("::", 1)[0])
    return have, errored


def hollow_files(roots: list[dict], python: str,
                 known_bad: list[str]) -> list[str]:
    """Tracked test files a pytest root CLAIMS but collects ZERO tests from.

    The gap this closes, found 2026-09-11 (lane TESTCI): `coverage_gaps()`
    returned 0 while THREE files were being imported and asked nothing --
    `test_patch_guard_split_hook.py`, `test_split_guard.py` and
    `tools/test_icf_fold_safe.py` are all `main()`-style, so pytest collected
    0 tests from each. The anti-staleness check could not see it, because
    "claimed by a root" was treated as "run". A file counted as covered while
    contributing nothing is strictly worse than one reported UNCOVERED: it is
    an escape hatch that does not print.

    A file that ERRORED at collection is NOT hollow -- it is a different,
    already-reported failure -- and neither is one the known-bad manifest
    covers, so a manifested collection error does not also fire here.
    """
    handled = {e["path"] for e in SCRIPT_ARM} | {e["path"] for e in EXCLUDED}
    tracked = [f for f in tracked_test_files() if f not in handled]
    hollow: list[str] = []
    for root in roots:
        mine = [f for f in tracked if _claimed_by_a_root(f, [root])]
        if not mine:
            continue
        have, errored = collected_files(root, python)
        for f in mine:
            if f in have or f in errored:
                continue
            if any(manifest_match(e, f) for e in known_bad):
                continue
            hollow.append(f)
    return sorted(set(hollow))


def run_root(root: dict, python: str, extra: list[str], log_dir: Path) -> dict:
    cmd = [python, "-m", "pytest", "-q", "--tb=no", "-rEf",
           "--continue-on-collection-errors", "-p", "no:cacheprovider"]
    cmd += root_selection_args(root)
    cmd.append(root["path"])
    cmd.extend(extra)

    # Spool pytest's output to a FILE, never to a pipe the runner buffers: a
    # test that emits enough captured stdout can MemoryError pytest's own
    # capture buffer, and a pipe-buffering parent would die alongside it.
    log_path = log_dir / (root["path"].replace("/", "_") + ".log")
    started = time.time()
    with log_path.open("wb") as fh:
        proc = subprocess.Popen(cmd, cwd=REPO_ROOT, stdout=fh,
                                stderr=subprocess.STDOUT)
        try:
            rc = proc.wait(timeout=root["timeout"])
            timed_out = False
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            rc = -1
            timed_out = True

    out = _strip_ansi(log_path.read_text(errors="replace")[-2_000_000:])

    observed = []
    for line in out.splitlines():
        m = SUMMARY_RE.match(line.strip())
        if m:
            observed.append(m.group(2))
    tail = [ln for ln in out.splitlines() if COUNT_RE.search(ln)]

    # A root that neither passed cleanly (rc 0) nor produced a parseable red
    # list (rc 1 + at least one FAILED/ERROR line) has gone wrong in a way the
    # known-bad manifest cannot express: internal error, usage error, nothing
    # collected (rc 5 — the script-mode shape), OOM, killed. That must be
    # FATAL, otherwise a crashed root reports as "no new failures".
    broken = (not timed_out
              and (rc not in (0, 1) or (rc == 1 and not observed)))
    return {
        "root": root["path"],
        "rc": rc,
        "timed_out": timed_out,
        "broken": broken,
        "observed": sorted(set(observed)),
        "counts": tail[-1].strip() if tail else "(no count line)",
        "elapsed": time.time() - started,
        "log": log_path,
    }


def run_script(entry: dict, python: str, log_dir: Path) -> dict:
    """Run a main()-style self-check as a program. rc 0 is pass."""
    log_path = log_dir / ("script_" + entry["path"].replace("/", "_") + ".log")
    started = time.time()
    with log_path.open("wb") as fh:
        proc = subprocess.Popen([python, entry["path"]], cwd=REPO_ROOT,
                                stdin=subprocess.DEVNULL, stdout=fh,
                                stderr=subprocess.STDOUT)
        try:
            rc = proc.wait(timeout=entry.get("timeout", 300))
            timed_out = False
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            rc = -1
            timed_out = True
    tail = _strip_ansi(log_path.read_text(errors="replace")).strip().splitlines()
    return {
        "path": entry["path"],
        "rc": rc,
        "timed_out": timed_out,
        "last": tail[-1][:80] if tail else "(no output)",
        "elapsed": time.time() - started,
        "log": log_path,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", action="append", default=None,
                    help="only run this collection root (repeatable)")
    ap.add_argument("--list", action="store_true",
                    help="list the roots, the script arm and the exclusions")
    ap.add_argument("--python", default=sys.executable,
                    help="interpreter used to run pytest (default: this one)")
    ap.add_argument("--strict-manifest", action="store_true",
                    help="a known-bad entry that now PASSES is fatal too")
    ap.add_argument("--no-script-arm", action="store_true",
                    help="skip the main()-style self-checks")
    ap.add_argument("--ci", action="store_true",
                    help="apply the CI_DESELECT list (tests that cannot pass "
                         "in the CI container for environment reasons)")
    ap.add_argument("--no-hollow-check", action="store_true",
                    help="skip the collect-only sweep that finds test files a "
                         "root claims but collects zero tests from (~6s)")
    ap.add_argument("--verbose-failures", action="store_true",
                    help="dump each failing root's output")
    ap.add_argument("--log-dir", default=None,
                    help="where per-root logs are spooled "
                         "(default: a temp dir, path printed)")
    ap.add_argument("pytest_args", nargs="*",
                    help="extra args forwarded to every pytest invocation")
    args = ap.parse_args()

    roots = discover_roots()
    if args.ci:
        # Attach each CI deselection to the root that owns it. An id naming a
        # file no root claims is a FATAL config error, not a silent no-op --
        # otherwise a renamed test would quietly stop being deselected and the
        # list would rot without saying so.
        for entry in CI_DESELECT:
            path = entry["id"].split("::", 1)[0]
            owners = [r for r in roots if _claimed_by_a_root(path, [r])]
            if not owners:
                print(f"CI_DESELECT names a test no root claims: {entry['id']}",
                      file=sys.stderr)
                return 1
            for r in owners:
                r.setdefault("deselect", []).append(entry["id"])
    if args.list:
        for r in roots:
            ig = f"  (ignore: {', '.join(r['ignore'])})" if r.get("ignore") else ""
            print(f"pytest  {r['path']}{ig}   timeout={r['timeout']}s")
        for e in SCRIPT_ARM:
            print(f"script  {e['path']}   timeout={e.get('timeout', 300)}s")
        for e in EXCLUDED:
            print(f"EXCL    {e['path']}   {e['why']}")
        for e in CI_DESELECT:
            print(f"CI-DESEL {e['id']}\n         {e['why']}")
        return 0
    if args.root:
        wanted = {p.rstrip("/") for p in args.root}
        roots = [r for r in roots if r["path"] in wanted]
        if not roots:
            print(f"no such root: {args.root}", file=sys.stderr)
            return 1

    # Preflight: without pytest every root comes back BROKEN with `rc=1`, five
    # times over. That is loud, which is the right direction -- but it reads as
    # "the lane is broken" rather than "this environment has no pytest", and a
    # confusing red is the kind that gets a CI step reverted instead of fixed.
    # Say it once, plainly, and name the remedy.
    probe = subprocess.run([args.python, "-m", "pytest", "--version"],
                           capture_output=True, text=True)
    if probe.returncode != 0:
        print(f"FATAL: `{args.python} -m pytest` is unavailable, so this lane "
              f"cannot run any pytest root.\n"
              f"       Install pytest in this environment (in CI: the build "
              f"container needs it).\n"
              f"       {(probe.stderr or probe.stdout).strip().splitlines()[-1] if (probe.stderr or probe.stdout).strip() else ''}",
              file=sys.stderr)
        return 1

    known_bad = load_manifest()
    gaps = coverage_gaps(discover_roots())

    log_dir = Path(args.log_dir) if args.log_dir else Path(
        tempfile.mkdtemp(prefix="test-tools-"))
    log_dir.mkdir(parents=True, exist_ok=True)

    script_arm = [] if args.no_script_arm else SCRIPT_ARM
    print(f"test lane — {len(roots)} pytest root(s), {len(script_arm)} "
          f"script-mode file(s), {len(known_bad)} known-bad entr(ies)")
    print(f"logs: {log_dir}\n")

    results = []
    for r in roots:
        res = run_root(r, args.python, args.pytest_args, log_dir)
        results.append(res)
        state = ("TIMEOUT" if res["timed_out"]
                 else "BROKEN" if res["broken"]
                 else "ok" if res["rc"] == 0 else "red")
        print(f"  [{state:>7}] {res['root']:<44} {res['counts']} "
              f"({res['elapsed']:.0f}s)", flush=True)

    script_results = []
    for e in script_arm:
        res = run_script(e, args.python, log_dir)
        script_results.append(res)
        state = ("TIMEOUT" if res["timed_out"]
                 else "ok" if res["rc"] == 0 else "FAILED")
        print(f"  [{state:>7}] {res['path']:<44} rc={res['rc']} "
              f"{res['last']} ({res['elapsed']:.0f}s)", flush=True)

    hollow: list[str] = []
    if not args.no_hollow_check:
        hollow = hollow_files(roots, args.python, known_bad)

    print()
    if EXCLUDED:
        print(f"EXCLUDED — tracked test-named files this lane does NOT run "
              f"({len(EXCLUDED)}). Each is a file with no caller; shrink this "
              f"list, do not grow it:")
        for e in EXCLUDED:
            print(f"    {e['path']}\n        {e['why']}")
        print()

    deselected = [(r["path"], d) for r in roots for d in r.get("deselect", [])]
    if deselected:
        why_of = {e["id"]: e["why"] for e in CI_DESELECT}
        print(f"DESELECTED — the known-bad manifest cannot cover these, "
              f"because a manifested test still RUNS (these hang, corrupt the "
              f"tree, or are green here and red there). Reasons are in "
              f"{Path(__file__).name}, STATIC_ROOTS / CI_DESELECT "
              f"({len(deselected)}):")
        for _root, d in deselected:
            print(f"    {d}")
            if d in why_of:
                print(f"        {why_of[d]}")
        print()

    new_failures: list[str] = []
    expected_hit: set[str] = set()
    for res in results:
        for obs in res["observed"]:
            hit = next((e for e in known_bad if manifest_match(e, obs)), None)
            if hit:
                expected_hit.add(hit)
            else:
                new_failures.append(obs)

    # A root that TIMED OUT or came back BROKEN produced no trustworthy
    # observation list, so every manifest entry inside it looks like it passed.
    # Reporting those as stale would tell the reader to delete entries that were
    # never actually re-checked — silence read as success, which is the exact
    # failure mode this lane exists to prevent. Measured 2026-08-17: after
    # tools/compiler_trace/tests hit its wall-clock, its whole-file manifest
    # entry was reported STALE alongside a genuine one, and following that
    # advice would have deleted live coverage.
    untrusted = {r["root"] for r in results if r["timed_out"] or r["broken"]}

    # A manifest entry for a root we did not run this time is not stale, and
    # neither is one inside a root's ``ignore`` subtree, nor one inside a root
    # whose result we cannot trust.
    def _in_scope(entry: str) -> bool:
        for r in roots:
            if not (entry == r["path"]
                    or entry.startswith(r["path"].rstrip("/") + "/")):
                continue
            if any(entry == i or entry.startswith(i.rstrip("/") + "/")
                   for i in r.get("ignore", [])):
                continue
            if r["path"] in untrusted:
                return False
            return True
        return False

    stale = [e for e in known_bad if e not in expected_hit and _in_scope(e)]
    shielded = sorted(e for e in known_bad
                      if e not in expected_hit and not _in_scope(e)
                      and any(e == u or e.startswith(u.rstrip("/") + "/")
                              for u in untrusted))


    timeouts = [r["root"] for r in results if r["timed_out"]]
    timeouts += [r["path"] for r in script_results if r["timed_out"]]
    broken = [(r["root"], r["rc"]) for r in results if r["broken"]]
    script_failed = [(r["path"], r["rc"]) for r in script_results
                     if r["rc"] != 0 and not r["timed_out"]]

    if expected_hit:
        print(f"KNOWN-BAD, reported not failed ({len(expected_hit)}):")
        for e in sorted(expected_hit):
            print(f"    {e}")
        print()
    if stale:
        print(f"STALE known-bad entries — these PASSED, drop them from "
              f"{MANIFEST.relative_to(REPO_ROOT)} ({len(stale)}):")
        for e in sorted(stale):
            print(f"    {e}")
        print()
    if shielded:
        print(f"NOT JUDGED — these manifest entries live in a root that timed "
              f"out or came back broken, so this run never re-checked them. "
              f"They are NOT stale; do not delete them on the strength of a "
              f"run that could not see them ({len(shielded)}):")
        for e in shielded:
            print(f"    {e}")
        print()
    if gaps:
        print(f"UNCOVERED tracked test files — no root, no script-arm entry "
              f"and no exclusion claims them; add one in "
              f"{Path(__file__).name} ({len(gaps)}):")
        for g in gaps:
            print(f"    {g}")
        print()
    if hollow:
        print(f"HOLLOW coverage — a pytest root CLAIMS these tracked test "
              f"files, so they are NOT reported as uncovered, but pytest "
              f"collects ZERO tests from each: the lane imports them and asks "
              f"them nothing. Either they are main()-style (add them to "
              f"SCRIPT_ARM) or their tests stopped being collected. Do not "
              f"leave them claimed-but-silent ({len(hollow)}):")
        for f in hollow:
            print(f"    {f}")
        print()
    if timeouts:
        print(f"TIMEOUT ({len(timeouts)}): " + ", ".join(timeouts) + "\n")
    if broken:
        print(f"BROKEN roots — pytest exited unusably (internal error, no "
              f"tests collected, OOM, killed); the manifest cannot cover these "
              f"({len(broken)}):")
        for root, rc in broken:
            print(f"    {root}  (pytest rc={rc})")
        print()
    if script_failed:
        print(f"SCRIPT-MODE FAILURES ({len(script_failed)}):")
        for path, rc in script_failed:
            print(f"    {path}  (rc={rc})")
        print()
    if new_failures:
        print(f"NEW FAILURES ({len(new_failures)}):")
        for f in sorted(new_failures):
            print(f"    {f}")
        print()

    if args.verbose_failures:
        for res in results:
            if res["rc"] != 0 or res["timed_out"]:
                body = res["log"].read_text(errors="replace")[-40_000:]
                print(f"───── {res['root']} ─────\n{_strip_ansi(body)}\n")
        for res in script_results:
            if res["rc"] != 0:
                body = res["log"].read_text(errors="replace")[-40_000:]
                print(f"───── {res['path']} ─────\n{_strip_ansi(body)}\n")

    bad = bool(new_failures or gaps or hollow or timeouts or broken
               or script_failed or (args.strict_manifest and stale))
    print("RESULT: " + ("FAIL" if bad else "PASS")
          + f"  (new={len(new_failures)} timeout={len(timeouts)} "
            f"broken={len(broken)} script-fail={len(script_failed)} "
            f"uncovered={len(gaps)} hollow={len(hollow)} stale={len(stale)} "
            f"known-bad-hit={len(expected_hit)})")
    # Name the failing suites on the last line too: a CI log is read from the
    # bottom, and "RESULT: FAIL" with no subject sends the reader hunting.
    if bad:
        subjects = sorted({f.split("::", 1)[0] for f in new_failures}
                          | {p for p, _ in script_failed}
                          | set(timeouts) | {r for r, _ in broken}
                          | set(hollow) | set(gaps)
                          | (set(stale) if args.strict_manifest else set()))
        print("FAILING: " + ", ".join(subjects))
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
