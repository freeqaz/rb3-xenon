#!/usr/bin/env python3
"""Refuse to measure a build tree that skipped the post-compile patchers.

Why this exists
---------------
`configure.py` chains six patcher edges onto the `post-compile` phony, then
(as of 2026-08-21) `scripts/verify_objs_patched.py --check --emit`.  Those
edges are DOWNSTREAM of the objects, taking the `all_source` phony as an
implicit input.  Ninja builds only a named target's ANCESTORS, so

    ninja build/45410914/src/system/rndobj/Utl.obj

stops exactly one edge short of every patcher, and the fresh compile
OVERWRITES the previously-patched bytes.

That was treated as a caveat for humans typing ninja by hand.  It is not:
`objdiff-cli diff --build` (without `--full-build`) IS `ninja <base_obj_path>`
-- a single-object target.  Worse here than on dc3, because per
tools/ninja-locked's own KNOWN GAP note objdiff-cli hardcodes
`Command::new("ninja")` and ignores `custom_make`, so that path ALSO bypasses
this repo's build lock.

Measured 2026-08-21 in a worktree off main `0f7f213b`, one `.cpp` touched and
one targeted `.obj` build, nothing else:

    ruler (rb3-xenon)                        patched      unpatched
    unit default/BandUI matched_code_percent  93.299904   91.293884  (-2.006)
    unit default/BandUI matched_code             18604       18204   (-400 B)
    ?InitPanels@BandUI@@QAAXXZ  (400 B)          100.0        99.7
    whole-build matched_code_percent          36.738945   36.735040
    whole-build matched_functions                42204       42203

...i.e. a function that IS matched reads as a near-miss.  Byte-exact is the
admission gate, so that is a destroyed crack, and the bias is one-directional:
an unpatched object can only lose points, never gain them.  A full build
restored the object's exact prior sha256 and report.json's measures to
byte-identical, twice -- so this is 100% patcher effect and 0% build
nondeterminism.

The contract here
-----------------
Build through `post-compile`, never through the bare `.obj`, and then ASSERT
the manifest.  If either half fails, raise -- callers must surface the error
instead of diffing.  Silently answering low is the behaviour being removed; it
is not to be replaced with silently answering some other way.

★ This guard REFUSES on BUILD OWED (verifier rc=6), and the choice is
deliberate rather than incidental
--------------------------------------------------------------------------
rc=6 says the objects are a perfect patched fixed point that was compiled from
SOURCE THAT IS NO LONGER IN THE TREE -- a merge without a build.  Refusing is
right for the same reason every other refusal here is: the number would be
reported under the current source's name while describing the previous
source's objects.  That is not a slightly stale measurement, it is a
mislabelled one, and it is strictly harder to detect afterwards than a low
number (recorded 2026-09-11: a ledger snapshot off such a tree read 42,505
matched functions where the built tree read 42,514, and nothing in the tree
disagreed with it).

What it costs, checked before choosing it:

*   `build=True` (the default, and every orchestrator path) builds
    `post-compile` FIRST, which compiles the changed TUs and re-emits the
    manifest -- so the state that produces rc=6 is repaired before the
    assertion runs and the refusal is unreachable on that path.
*   `build=False` is the read-only look, and there rc=6 is exactly the case
    worth refusing: the caller asked not to build and the tree is behind.

So the refusal fires precisely where a measurement would be mislabelled, and
nowhere else.  The residual is a tree whose changed inputs feed no compile
edge at all -- ninja does no work, the manifest is not re-emitted, and the
verdict persists; `_refusal_headline`/the remedy text below name the bounded
re-baseline for that case and its precondition.

`post-compile` reaches every object through `all_source`, so the specific
`.obj` a caller cares about is still compiled first; the patch stamps then
re-fire because the patchers preserve each object's mtime, which is what makes
an object newer than a stamp mean "this one needs patching again" instead of
an endless recompile/repatch oscillation.

Two rb3-xenon specifics
-----------------------
*   The build is driven through **`custom_make`** (`tools/ninja-locked`), read
    out of `objdiff.json` rather than assumed.  A bare `ninja` here races the
    SPLIT->configure manifest-regeneration loop that the wrapper's flock
    exists to prevent -- see the wrapper's header.

*   The assertion is `--verify-manifest`, **not** `--check`, and that choice is
    load-bearing.  `--check` is only as good as the passes it re-runs, and
    three of rb3-xenon's six (`guard`, `bool_mangle`, `atexit_scope`) are
    currently idle -- 0 files patched repo-wide in APPLY mode on a fully built
    tree.  The manifest is content-keyed and therefore catches any object that
    changed outside the graph regardless of which pass would have touched it.
    It also covers the dtk-split TARGET objects, whose pre-compile renamer
    state is what lane FOLDPROVE-2 was silently wrong about.

Cost, measured on this tree: `./tools/ninja-locked post-compile` on an
already-consistent tree is `ninja: no work to do.`; `--verify-manifest` over
1,205 decomp + 3,091 target objects is ~0.56 s.  So the guard costs ~half a
second on the repeat calls that dominate a lane, and buys back the
one-way-low bias on the calls that follow an edit.
"""

from __future__ import annotations

import importlib.util
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

__all__ = ["UnpatchedTreeError", "ensure_patched_tree",
           "ensure_patched_tree_once", "POST_COMPILE_TARGET"]

#: The ninja target that owns the patch passes (see `configure.py`
#: `custom_build_steps`).  Naming the `.obj` instead is the defect.
POST_COMPILE_TARGET = "post-compile"

#: Fallback only.  rb3-xenon's objdiff.json declares `tools/ninja-locked`.
_DEFAULT_MAKE = "ninja"

_BUILD_TIMEOUT = 3600
_VERIFY_TIMEOUT = 600


class UnpatchedTreeError(RuntimeError):
    """The tree is not a verified fixed point of the post-compile patchers.

    Deliberately a hard error: a measurement taken from a partially-patched
    tree is not a slightly worse measurement, it is a measurement of symbol
    names, storage classes and relocations that this project does not match
    against.
    """


def _make_command(project_dir: Path | str) -> list[str]:
    """Mirror `objdiff-cli --build`: `custom_make` + `custom_args`, else ninja.

    Read from the project file, never hardcoded -- on this repo the answer is
    `tools/ninja-locked` and using bare `ninja` instead is a concurrency bug,
    not a style preference.
    """
    project_dir = Path(project_dir)
    make, args = _DEFAULT_MAKE, []
    cfg = project_dir / "objdiff.json"
    if cfg.exists():
        try:
            doc = json.loads(cfg.read_text())
        except (OSError, json.JSONDecodeError):
            doc = {}
        if isinstance(doc, dict):
            make = doc.get("custom_make") or _DEFAULT_MAKE
            args = list(doc.get("custom_args") or [])
    return [make, *args]


#: Seconds to wait out an in-flight split before refusing. Concurrent builds
#: are the common case in these trees, and a guard that trips on one gets
#: switched off; a STALE split is still refused immediately, because waiting
#: cannot fix it.
_SPLIT_WAIT_DEFAULT = 180.0
SPLIT_WAIT_ENV = "RB3X_SPLIT_GUARD_WAIT"


def _tail(text: str, n: int = 25) -> str:
    lines = [ln for ln in (text or "").splitlines() if ln.strip()]
    return "\n".join(lines[-n:])


#: The verifier's exit codes are each a DIFFERENT statement (see its module
#: docstring).  This guard refuses on all of them -- every one means the number
#: you are about to read describes something other than the tree you think you
#: are measuring -- but it must not describe them all the same way.
#:
#: ⛔ It used to.  Every non-zero got the raw-compiler-output accusation, which
#: is precisely the disease lane GATE-DISC removed from the verifier itself:
#: asserting a mechanism the tool has not observed.  Two investigations were
#: opened off that message and both concluded wrongly.  A guard that refuses
#: correctly and explains wrongly still costs a lane its afternoon.
_REFUSAL_HEADLINES = {
    6: ("a BUILD IS OWED. Its objects are a verified patched fixed point and "
        "match their manifest exactly -- nothing is corrupt -- but a build "
        "INPUT has changed since they were compiled, so they were built from "
        "SOURCE THAT IS NO LONGER IN THE TREE. Measuring here reports the "
        "previous state of the source under the current state's name (recorded "
        "2026-09-11: a ledger snapshot off such a tree read 42,505 matched "
        "functions where the built tree read 42,514)."),
    5: ("a BUILD IS IN FLIGHT. tools/ninja-locked holds the build lock, so "
        "these objects are being rewritten as they are read. Nothing is wrong "
        "and nothing is adjudicable -- wait and retry."),
    4: ("a REBUILD IS PENDING. The tree advanced since the manifest was "
        "written AND the objects have already moved, so they belong to a "
        "different state of the tree than anything measured from them would "
        "be attributed to."),
    2: ("its patch state has NEVER been established -- there is no manifest "
        "in this build tree at all."),
    3: ("the patcher PAIRING IS VACUOUS, so a green light from it would carry "
        "no information about the population it claims to cover."),
}

_DEFAULT_REFUSAL = (
    "its objects are not a verified fixed point of the post-compile patchers, "
    "so every symbol name, storage class and relocation in them describes raw "
    "compiler output. A diff taken here reads LOW and one-directional "
    "(measured -2.006 pp of unit matched_code, and one 400-byte function "
    "100.0 -> 99.7, on ONE object)."
)


def _refusal_headline(rc: int) -> str:
    return _REFUSAL_HEADLINES.get(rc, _DEFAULT_REFUSAL)


def ensure_patched_tree(project_dir: Path | str, *, build: bool = True) -> str:
    """Bring `project_dir`'s object tree to the post-compile fixed point.

    Returns a one-line note suitable for echoing to the caller.  Raises
    `UnpatchedTreeError` -- it never returns a plausible number -- if the tree
    cannot be brought to, or verified at, that fixed point.

    `build=False` skips the build (the caller asked for a read-only look) but
    still verifies, because reading an unpatched object is the failure being
    prevented, not the build.
    """
    project_dir = Path(project_dir).resolve()
    verify = project_dir / "scripts" / "verify_objs_patched.py"
    if not verify.exists():
        raise UnpatchedTreeError(
            f"{verify} is absent, so this tree's patch state cannot be "
            f"established. Refusing to measure: a diff taken here would "
            f"describe raw compiler output, not the shape this project "
            f"matches against."
        )

    notes = []

    if build:
        make = _make_command(project_dir)
        exe = make[0]
        if (shutil.which(exe) is None and not (project_dir / exe).exists()
                and not Path(exe).exists()):
            raise UnpatchedTreeError(
                f"build tool `{exe}` (from {project_dir.name}/objdiff.json "
                f"custom_make) is not on PATH and does not exist. Refusing to "
                f"measure rather than diffing whatever objects happen to be "
                f"on disk."
            )
        # Resolve a repo-relative wrapper so cwd changes cannot silently pick
        # up a different one.
        if (project_dir / exe).exists():
            make = [str(project_dir / exe), *make[1:]]
        cmd = [*make, POST_COMPILE_TARGET]
        # tools/ninja-locked appends `configure.py progress` -- a 40-line
        # dashboard, measured by the wrapper's own header at 534 ms -- to every
        # invocation that did not already run the PROGRESS rule.  A guard that
        # fires once per measurement would pay that every time, for a dashboard
        # no caller reads, and it buries the one line worth echoing.  The
        # wrapper documents this env var for exactly this case.
        env = dict(os.environ, NINJA_LOCKED_SKIP_PROGRESS="1")
        try:
            proc = subprocess.run(
                cmd, cwd=str(project_dir), capture_output=True, text=True,
                timeout=_BUILD_TIMEOUT, env=env,
            )
        except subprocess.TimeoutExpired:
            raise UnpatchedTreeError(
                f"`{' '.join(cmd)}` timed out after {_BUILD_TIMEOUT}s in "
                f"{project_dir}."
            ) from None
        if proc.returncode != 0:
            raise UnpatchedTreeError(
                f"`{' '.join(cmd)}` failed (exit {proc.returncode}) in "
                f"{project_dir}.\n\n{_tail(proc.stderr) or _tail(proc.stdout)}"
                f"\n\nThe measurement is NOT being reported: whatever objects "
                f"are on disk did not come from a complete build."
            )
        head = [ln for ln in (proc.stdout or "").strip().splitlines() if ln.strip()]
        notes.append(head[-1] if head else "post-compile up to date")

    try:
        proc = subprocess.run(
            [sys.executable, str(verify), "--verify-manifest", "--quiet"],
            cwd=str(project_dir), capture_output=True, text=True,
            timeout=_VERIFY_TIMEOUT,
        )
    except subprocess.TimeoutExpired:
        raise UnpatchedTreeError(
            f"verify_objs_patched.py --verify-manifest timed out after "
            f"{_VERIFY_TIMEOUT}s in {project_dir}."
        ) from None

    if proc.returncode != 0:
        if not build:
            remedy = "Run `./tools/ninja-locked` in that directory, then retry."
        elif proc.returncode == 6:
            # ⚠ The one case where "the build graph has regressed" would be a
            # WRONG accusation.  `post-compile` re-emits the manifest only if
            # it does work, and ninja is blind to some build inputs by design
            # -- measured: a splits.txt CONTENT change produces 0 ninja edges,
            # and a source file no TU compiles or includes produces none
            # either.  So a build that legitimately did nothing leaves the
            # manifest describing the older inputs.
            remedy = (
                "`post-compile` ran and the inputs STILL disagree. If that "
                "build reported `no work to do`, then by the build system's "
                "own dependency knowledge these objects ARE current and the "
                "changed inputs are ones no compile edge consumes -- re-"
                "baseline with `python3 scripts/verify_objs_patched.py "
                "--check --emit`, which re-verifies the patch fixed point "
                "before recording the new input state.\n"
                "⛔ Run that ONLY after a full build reports no work. Running "
                "it over a tree that genuinely owes compiles records stale "
                "objects as the reference state and restores the exact blind "
                "spot this check exists to close.\n"
                "If a config pin moved, the split is ninja-invisible: "
                "`touch config/<version>/config.yml` first."
            )
        else:
            remedy = (
                "`post-compile` ran and the tree STILL does not match its "
                "manifest -- that is a regression of the build graph itself, "
                "not a stale tree."
            )
        raise UnpatchedTreeError(
            f"REFUSING TO MEASURE {project_dir}: {_refusal_headline(proc.returncode)}"
            f"\n\n{_tail(proc.stderr) or _tail(proc.stdout)}\n\n{remedy}"
        )

    notes.append((proc.stdout or proc.stderr or "").strip() or "patch state verified")

    # ... and the OTHER side of every diff. Everything above establishes that
    # the BASE objects (build/<v>/src/**) are a fixed point of the post-compile
    # patchers. It says nothing about the TARGET objects (build/<v>/obj/**),
    # which are written by an edge -- `dtk xex split` -- that declares only
    # config.json and so is invisible to ninja, to the patch manifest, and to
    # every mtime. A report taken while a split is rewriting them, or over
    # objects split from a symbols.txt that has since moved, does not fail: it
    # reads a plausible LOWER number (measured in dc3-decomp 2026-08-21: 29,497
    # matched functions instead of 29,838, a 341-function gap, no warning).
    #
    # An in-flight split is WAITED OUT rather than refused -- concurrent builds
    # are the common case in these trees and a guard that trips on them gets
    # switched off -- but a STALE one is refused immediately, because waiting
    # cannot fix it.
    notes.append(_ensure_split_current(project_dir, wait_seconds=_split_wait()))
    return " | ".join(n for n in notes if n)


def _split_wait() -> float:
    """How long to wait out a split that is merely in flight.

    Env-overridable ONLY so the sabotage test can plant a `running` record and
    observe the refusal in seconds instead of three minutes. Production callers
    never set it; a shorter wait in a fleet would turn a normal concurrent
    build into a refusal.
    """
    try:
        return float(os.environ.get(SPLIT_WAIT_ENV, "") or _SPLIT_WAIT_DEFAULT)
    except ValueError:
        return _SPLIT_WAIT_DEFAULT


def _ensure_split_current(project_dir: Path, *, wait_seconds: float) -> str:
    """Assert the target objects came from the config on disk.

    Imported lazily and tolerantly: a worktree or a checkout predating
    scripts/verify_split_current.py must degrade to a loud note rather than
    make every measurement in this repo impossible.
    """
    guard_path = project_dir / "scripts" / "verify_split_current.py"
    if not guard_path.exists():
        return f"split currency UNVERIFIED ({guard_path} absent)"
    spec = importlib.util.spec_from_file_location(
        "_rb3x_verify_split_current", guard_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    try:
        return module.ensure_split_current(project_dir, wait_seconds=wait_seconds)
    except module.StaleSplitError as exc:
        raise UnpatchedTreeError(str(exc)) from None


#: Memo for :func:`ensure_patched_tree_once`, keyed on the resolved tree.
_ENSURED: dict[str, str] = {}


def ensure_patched_tree_once(project_dir: Path | str, **kw) -> str:
    """`ensure_patched_tree`, at most once per tree per process.

    For the scripts that measure MANY symbols out of one tree in a loop.  Each
    call is `ninja: no work to do.` plus a ~0.56 s hash of 4,296 objects --
    nothing once, but minutes across a few hundred symbols.

    The tradeoff is explicit and it is not free: this trusts that nothing else
    unpatches the tree while the loop runs.  That is sound for the loops it is
    used in -- they no longer build, precisely because this change stopped them
    building -- and it is NOT sound for anything that compiles between
    measurements.  Those callers use :func:`ensure_patched_tree` directly.

    A refusal is deliberately not memoized: a raise leaves the memo empty, so a
    caller that repairs the tree and retries gets an honest second answer
    rather than the cached complaint.
    """
    key = str(Path(project_dir).resolve())
    note = _ENSURED.get(key)
    if note is None:
        note = ensure_patched_tree(project_dir, **kw)
        _ENSURED[key] = note
    return note
