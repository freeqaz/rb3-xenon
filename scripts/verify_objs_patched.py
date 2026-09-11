#!/usr/bin/env python3
"""Assert that rb3-xenon's build tree is a FIXED POINT of the six post-compile
patchers -- and record a content manifest so a later bypass is detectable from
outside the build.

Why
---
`configure.py` chains six patcher edges onto the `post-compile` phony.  They
rewrite ninja's own outputs IN PLACE, and they are DOWNSTREAM of the compile
edges.  Ninja builds only a named target's ANCESTORS, so

    ninja build/45410914/src/system/rndobj/Utl.obj

stops exactly one edge short of every patcher, and the fresh compile
OVERWRITES the previously-patched bytes.  The same is true of
`objdiff-cli diff --build` (without `--full-build`), which is literally
`ninja <base_obj_path>` -- and, per tools/ninja-locked's own KNOWN GAP note,
also bypasses this repo's build lock, because objdiff-cli hardcodes
`Command::new("ninja")` and ignores `custom_make`.

That leaves one raw-compiler object behind in a tree that otherwise looks
finished, for EVERY subsequent reader: report.json, measure_progress.sh, the
orchestrator, and any concurrent lane.  Because the patchers deliberately
preserve each object's mtime (see any patcher's `_write_preserving_mtime`
docstring -- without it ninja's `deps = msvc` records force an endless
recompile/repatch oscillation), the degraded state is INVISIBLE IN TIMESTAMPS
and leaves no record of which measurements ran against which state.

Measured here, 2026-08-21, on a worktree off main `0f7f213b` -- one `touch` of
`src/band3/meta_band/BandUI.cpp` plus one targeted `.obj` build, nothing else:

    ruler (rb3-xenon)                        patched      unpatched
    unit default/BandUI matched_code_percent  93.299904   91.293884  (-2.006)
    unit default/BandUI matched_code             18604       18204   (-400 B)
    ?InitPanels@BandUI@@QAAXXZ  (400 B)          100.0        99.7
    whole-build matched_code_percent          36.738945   36.735040  (-0.0039)
    whole-build matched_functions                42204       42203

A full `ninja` restored the object to its exact prior sha256 and report.json's
measures to byte-identical, twice.  So the delta is 100% patcher effect and 0%
build nondeterminism -- do not dismiss a single-function move here as noise.

Two checks, because there are two ways to reach the degraded state
-----------------------------------------------------------------
`--check` (wired into the default build, after the patch stamps)
    Re-runs every patcher in dry-run and FAILS THE BUILD if any would still
    change a file.  It uses the patchers' own detection logic, so it cannot
    drift from them, and it catches a regression of the dependency graph
    itself -- including "someone added a seventh patcher and forgot the edge".

`--emit` (same edge, after a passing `--check`)
    Writes `build/45410914/patch_state.json`: sha256 of every object at the
    moment the tree was verified patched.  `--verify-manifest` recomputes it.
    This catches what no build-time check can: a tool that compiles a single
    TU outside the full graph and leaves one unpatched object behind.
    Consumers can check it without a toolchain and without parsing an object.

★ Why the manifest is the load-bearing half here, more than it is on dc3
-----------------------------------------------------------------------
`--check` can only be as good as the passes it re-runs, and on rb3-xenon
THREE OF THE SIX PASSES ARE CURRENTLY IDLE.  Measured on a fully built tree in
APPLY mode: `guard` 0 files, `bool_mangle` 0 files, `atexit_scope` 0 files.  So
a green `--check` is earned by three passes, not six, and a sabotage that only
those three would notice would slip past it.

The manifest has no such dependence: it is content-keyed, so ANY object that
changed without the full graph re-running is caught regardless of which pass
would have touched it.  That is why `--verify-manifest` -- not `--check` -- is
what scripts/orchestrator/patch_guard.py asserts on.

How this differs from dc3-decomp's verify_objs_patched.py
---------------------------------------------------------
Ported from dc3 (`2f35703d0`) and deliberately not identical:

1.  **Six patchers, not five** (rb3-xenon adds `obj_eh_boundary_patcher.py`),
    and five of the six had no `--check` until it was added alongside this
    file.  dc3's five all had one already.

2.  **It covers the TARGET objects too**, which dc3 has no equivalent of.
    rb3-xenon runs a *pre*-compile pass, `obj_target_symbol_renamer.py`, that
    rewrites the dtk-split target objs' anonymous `fn_<addr>` symbols to MSVC
    mangled names.  A tree whose target objs are pre-renamer answers "absent"
    to every mangled-name lookup -- and CLAUDE.md records lane FOLDPROVE-2
    getting a unanimous "100/100 refuted, exactly the answer it was primed to
    expect" from precisely that, caught only because a symbol count disagreed
    (69,438 vs 69,415).  A vacuity that agrees with your prior is the hardest
    kind to catch, so the manifest records both sides and reports drift in
    each separately.  (`tools/check_target_objs_renamed.py` guards this at
    build time; what it cannot do is answer a consumer who is not running
    ninja.)

3.  **It states its denominators, and now ENFORCES them.**  Three of the six
    passes are idle (`guard`/`bool_mangle`/`atexit_scope` all report 0 pending
    on a fully built tree), so a green `--check` is earned by three passes, not
    six.  Those same three used to pair target-to-base by RELPATH.

    ⚠ **The figure this file used to carry -- "347 of the 1,048 pairs, 3
    mispaired" -- was measured against the wrong denominator and is corrected
    here.**  Re-derived on main at `0d125b35`, rb3-xenon, title 45410914:

      * `1,048` counted objdiff.json UNITS, but the patcher loops iterate
        distinct COMPILED OBJECTS, of which there are **1,045**.  The
        difference is exactly the 3 objects declared by two units each.
      * `347` likewise double-counted those 3.  The loops examined **344**,
        which is what the patchers themselves printed all along
        (`344 files checked`).
      * The "3 mispaired" were not a patcher choosing wrongly.  They are a
        **splits.txt defect**: `UIStats`, `AccomplishmentProgress` and `Game`
        each have BOTH a path-qualified and a bare heading, dtk emits two
        target objects, and `tools/project.py`'s basename alias binds our one
        compiled object to both.  The two halves are one retail TU -- address
        ranges contiguous/interleaved, report.json function sets disjoint
        (overlap 0), so nothing is double-counted.  Whichever half a pass
        picked, it saw an arbitrary half of retail's symbols.

    So the honest statement of the old gap is **701 of 1,045 distinct compiled
    objects (67.1%) invisible**, and it was not free: running the three passes
    over the objdiff.json pairing found **7 pending `$S` -> `??_B` guard
    renames, all 7 inside the invisible 701**.

    `scripts/obj_pairing.py` now owns the pairing for all three, and this file
    ASSERTS its coverage rather than merely printing it: `--check` fails if any
    declared compiled object resolves to no target, and fails separately if the
    pairing is vacuous (no objdiff.json, nothing declared).  Printing a
    denominator that nobody checks is how the 701 survived being written down.

4.  **It drives builds through `custom_make`.**  Not this file's job, but its
    sibling patch_guard.py's -- noted here because bare `ninja` on this repo
    races the SPLIT->configure loop that tools/ninja-locked exists to prevent.

Exit codes -- every non-zero state is a DIFFERENT statement
-----------------------------------------------------------
    0   the objects on disk are the ones this tree was verified patched over
    1   CORRUPTION, or an undeterminable cause.  Same git HEAD, same split
        inputs, no build running -- and the content differs anyway.
    2   no manifest at all: this tree has never been verified patched
    3   the pairing is VACUOUS (objdiff.json declares too few objects to make
        a green light mean anything)
    4   REBUILD PENDING: the tree ADVANCED (git HEAD or a split input moved)
        since the manifest was written, so the objects belong to a different
        state.  Not measurable, but nothing is wrong.
    5   BUILD IN PROGRESS: `tools/ninja-locked`'s flock is held, so a build is
        rewriting these objects as the check runs.

⛔ Codes 4 and 5 exist because this tool USED TO ASSERT A MECHANISM IT CANNOT
OBSERVE.  It printed "produced OUTSIDE the full build graph ... the
post-compile patch passes never ran on it" for ANY content difference.  On
2026-09-11 a lane sampled `main` 2m40s into an ordinary full build -- the
build lock was held, the build completed normally at 01:10:56 -- and got that
accusation at rc=1.  It cost two separate investigations, and BOTH diagnoses
reached from the message were wrong (the first blamed an MCP tool writing to
the shared tree; the second blamed a benign "pending rebuild", which is also
wrong, because a merge changes SOURCE and not objects, so content differing
always means something did write them).  A check that cries corruption
whenever the tree is busy fires hardest exactly when a shared tree is busiest,
and is the kind of check people learn to skip.

⚠ The accusation itself is NOT weakened.  It is now CONDITIONED: rc=1 is
reached only after establishing that no build holds the lock and that the
tree is at the same state the manifest was taken over.  Those two facts are
printed with it, so the reader can check the reasoning rather than trust it.
"""

import argparse
import fcntl
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
VERSION = os.environ.get("RB3_VERSION", "45410914")

#: In the order `configure.py` chains them.  Each accepts `--batch --check`;
#: five of them only since 2026-08-21 (see this file's header).
PATCHERS = [
    "obj_anon_ns_patcher.py",
    "obj_dynamic_init_patcher.py",
    "obj_guard_patcher.py",
    "obj_bool_mangle_patcher.py",
    "obj_atexit_scope_patcher.py",
    "obj_eh_boundary_patcher.py",
]

#: 2 (lane PAIRFIX): `pairing_coverage` changed shape.  v1 counted objdiff.json
#: UNITS with relpath-only keys (`declared_pairs`/`relpath_reachable`/
#: `relpath_disagrees`/`invisible`); v2 counts DISTINCT COMPILED OBJECTS and is
#: produced by scripts/obj_pairing.py, the same module the patchers pair with.
#: A reader that finds v1 is reading a manifest whose coverage block
#: double-counted the three multi-target objects.
#:
#: 3 (lane GATE-DISC): added the `provenance` block -- git HEAD plus the hashes
#: of the split inputs -- recording the tree state the manifest was taken over.
#: A reader that finds v2 or lower is reading a manifest that CANNOT say why an
#: object differs, only that it does, and `--verify-manifest` reports the cause
#: as UNDETERMINED rather than guessing one.
MANIFEST_VERSION = 3


def build_dir(repo: Path) -> Path:
    return repo / "build" / VERSION


def src_dir(repo: Path) -> Path:
    """Decomp objects -- what the six post-compile patchers rewrite."""
    return build_dir(repo) / "src"


def target_dir(repo: Path) -> Path:
    """dtk-split target objects -- what the PRE-compile renamer rewrites."""
    return build_dir(repo) / "obj"


def decomp_objects(repo: Path):
    return sorted(p for p in src_dir(repo).rglob("*.obj") if p.is_file())


def target_objects(repo: Path):
    d = target_dir(repo)
    if not d.is_dir():
        return []
    return sorted(p for p in d.rglob("*.obj") if p.is_file())


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ── is a build running, and what state was the manifest taken over? ─────────


#: The lock `tools/ninja-locked` holds for the DURATION of a build
#: (`exec 9>"$REPO/.ninja-build.lock"` then `flock`).
BUILD_LOCK_NAME = ".ninja-build.lock"


def build_lock_held(repo: Path) -> bool:
    """True if a build currently holds tools/ninja-locked's flock.

    ⛔ Consulted by `--verify-manifest` ONLY, and that restriction is
    load-bearing: `--check`/`--emit` run as a ninja edge INSIDE the build that
    holds this lock, so probing there would see it held on EVERY build and
    refuse forever.  Do not "helpfully" extend this to them.

    The probe is SHARED and non-blocking: it fails immediately when a builder
    holds the exclusive lock, and is released before returning, so it can
    neither block a build nor be mistaken for one by a concurrent probe.

    An absent lock file means no build has ever run through the wrapper here.
    That is "no evidence of a build", not "a build is running" -- returning
    True there would make every fresh tree unverifiable.
    """
    p = repo / BUILD_LOCK_NAME
    if not p.exists():
        return False
    try:
        fd = os.open(str(p), os.O_RDONLY)
    except OSError:
        return False
    try:
        try:
            fcntl.flock(fd, fcntl.LOCK_SH | fcntl.LOCK_NB)
        except OSError:
            return True
        fcntl.flock(fd, fcntl.LOCK_UN)
        return False
    finally:
        os.close(fd)


def split_input_relpaths() -> tuple:
    """The files whose content decides what a build PRODUCES.

    Not a general "did the source change" check -- source changes alone do not
    rewrite objects.  These three are the inputs that make the dtk split emit
    DIFFERENT target objects and the renamer install different names, which is
    the drift a consumer actually trips over after a merge.
    """
    return (
        f"config/{VERSION}/splits.txt",
        f"config/{VERSION}/symbols.txt",
        "scripts/target_symbol_map.json",
    )


def _git_head(repo: Path):
    try:
        p = subprocess.run(["git", "rev-parse", "HEAD"], cwd=str(repo),
                           capture_output=True, text=True, timeout=30)
    except (OSError, subprocess.TimeoutExpired):
        return None
    return (p.stdout.strip() or None) if p.returncode == 0 else None


def current_provenance(repo: Path) -> dict:
    """The tree state a manifest is being taken over."""
    return {
        "git_head": _git_head(repo),
        "split_inputs": {
            rel: (sha256(repo / rel) if (repo / rel).is_file() else None)
            for rel in split_input_relpaths()
        },
    }


def provenance_moved(recorded: dict, current: dict) -> list:
    """-> reasons the tree ADVANCED since `recorded`; empty means it did not.

    An empty list is the only condition under which "content differs"
    licenses the word corruption.

    ⚠ `None` on either side is UNKNOWN, not different.  An absent git, or a
    config path this version does not use, must never manufacture a reason --
    that would turn every such tree into a permanent "rebuild pending" and
    disarm the corruption branch entirely.
    """
    out = []
    was_head, now_head = recorded.get("git_head"), current.get("git_head")
    if was_head and now_head and was_head != now_head:
        out.append(f"git HEAD {was_head[:8]} -> {now_head[:8]}")
    rec = recorded.get("split_inputs") or {}
    cur = current.get("split_inputs") or {}
    for rel in sorted(set(rec) | set(cur)):
        was, now = rec.get(rel), cur.get(rel)
        if was is not None and now is not None and was != now:
            out.append(f"{rel} changed since the manifest was written")
        elif (was is None) != (now is None):
            out.append(f"{rel} {'appeared' if was is None else 'disappeared'}")
    return out


# ── coverage: what a green --check is actually worth ────────────────────────


def pairing_coverage(repo: Path) -> dict:
    """How much of the declared population the pairing-driven passes can see.

    ★ Computed by `scripts/obj_pairing.py` -- THE SAME CODE THE PATCHERS USE,
    on purpose.  The previous version of this function reimplemented the
    pairing rule here, and a reimplementation can only ever report on itself:
    it double-counted the three multi-target objects (`347`/`1,048` for a loop
    that examined 344 of 1,045) and could have drifted arbitrarily far from the
    passes it claimed to describe without anything noticing.
    """
    sys.path.insert(0, str(repo / "scripts"))
    import obj_pairing  # noqa: E402  (deliberately late: repo-relative)
    return obj_pairing.ObjPairing(
        repo, target_dir(repo), src_dir(repo), repo / "objdiff.json").coverage()


def _coverage_line(cov: dict) -> str:
    sys.path.insert(0, str(REPO / "scripts"))
    import obj_pairing  # noqa: E402
    return "[patch-state] " + obj_pairing.coverage_line(cov)


#: Floor for "objdiff.json still describes a real project".  Deliberately far
#: below the live count (1,045 on main at `0d125b35`) so that adding or
#: retiring translation units never trips it, and far above zero so that a
#: configure.py that emitted a mostly-empty config does.  Without an absolute
#: floor the vacuity is undetectable: a config declaring one object has 100%
#: coverage, an empty `declared_unpaired` list, and every ratio green.
DEFAULT_MIN_DECLARED = 900


def check_pairing(repo: Path, quiet: bool = False,
                  min_declared: int = DEFAULT_MIN_DECLARED) -> int:
    """Refuse a build whose patchers cannot see the population they claim to.

    This is the half of the assertion that `--check` alone cannot make.
    `--check` asks "would any pass still change a file?", and a pass that is
    BLIND to an object answers "no" -- the same answer it gives for an object
    that is genuinely clean.  That is the whole defect: 701 of 1,045 declared
    objects answered "no" because nobody looked, and 7 of them had a pending
    guard rename.

    So coverage is asserted separately, from the patchers' own pairing module,
    and a shortfall fails the build.
    """
    cov = pairing_coverage(repo)
    sys.path.insert(0, str(repo / "scripts"))
    import obj_pairing  # noqa: E402
    try:
        obj_pairing.assert_full_coverage(cov, min_declared=min_declared)
    except obj_pairing.PairingCoverageError as e:
        print("=" * 72, file=sys.stderr)
        print("POST-COMPILE PATCH PAIRING IS INCOMPLETE", file=sys.stderr)
        print("=" * 72, file=sys.stderr)
        print(str(e), file=sys.stderr)
        print("\nAn unpaired object is not 'clean' -- it is UNEXAMINED.  Every "
              "pairing-driven pass skips it silently, and the measurement that "
              "follows is taken over an object that was never patched.\n"
              "Fix: scripts/obj_pairing.py resolves the pairing from "
              "objdiff.json; if that file is stale or a unit lost its "
              "base_path, re-run configure.py.", file=sys.stderr)
        return 3
    if not quiet:
        print(_coverage_line(cov))
        for rel in cov["multi_target_objects"]:
            print(f"[patch-state] multi-target (splits.txt declares two "
                  f"headings for one TU): {rel}")
    return 0


def run_check(repo: Path, quiet: bool = False,
              min_declared: int = DEFAULT_MIN_DECLARED) -> int:
    """Dry-run every patcher; non-zero if the tree is not a fixed point."""
    rc = check_pairing(repo, quiet=quiet, min_declared=min_declared)
    if rc:
        return rc
    failures = []
    for script in PATCHERS:
        p = subprocess.run(
            [sys.executable, str(repo / "scripts" / script), "--batch", "--check"],
            cwd=str(repo), capture_output=True, text=True)
        if p.returncode != 0:
            failures.append((script, p.returncode,
                             (p.stderr or p.stdout).strip().splitlines()[-3:]))
    if not failures:
        if not quiet:
            print(f"[patch-state] OK: tree is a fixed point of "
                  f"{len(PATCHERS)} post-compile passes")
        return 0
    print("=" * 72, file=sys.stderr)
    print("BUILD TREE IS NOT FULLY PATCHED", file=sys.stderr)
    print("=" * 72, file=sys.stderr)
    print(f"{len(failures)} of {len(PATCHERS)} post-compile passes still have "
          f"pending work in {src_dir(repo)}.", file=sys.stderr)
    print("These objects were COMPILED but never POST-PROCESSED, so every "
          "symbol name, storage class and relocation they carry describes the "
          "raw compiler output and not the shape this project matches "
          "against.  Anything measured from this tree reads LOW and "
          "one-directionally (measured -2.006 pp of unit matched_code on one "
          "object; see this script's header).", file=sys.stderr)
    for script, rc, tail in failures:
        print(f"\n  {script} (exit {rc}):", file=sys.stderr)
        for line in tail:
            print(f"    {line}", file=sys.stderr)
    print("\nFix: run a full `./tools/ninja-locked` (the patch stamps take "
          "`all_source` as a real input, so they re-run behind any recompile). "
          "If this fires during a FULL build, the dependency graph in "
          "configure.py has regressed.", file=sys.stderr)
    return 1


def _entries(repo: Path, paths):
    out = {}
    for p in paths:
        st = p.stat()
        out[str(p.relative_to(repo))] = {
            "sha256": sha256(p), "size": st.st_size, "mtime_ns": st.st_mtime_ns}
    return out


def emit(repo: Path) -> int:
    decomp = _entries(repo, decomp_objects(repo))
    target = _entries(repo, target_objects(repo))
    tree = hashlib.sha256(
        "".join(f"{k}:{v['sha256']}\n"
                for k, v in sorted({**decomp, **target}.items()))
        .encode()).hexdigest()
    doc = {
        "manifest_version": MANIFEST_VERSION,
        "build_id": VERSION,
        "generated_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "patchers": PATCHERS,
        "n_objects": len(decomp) + len(target),
        "n_decomp_objects": len(decomp),
        "n_target_objects": len(target),
        "tree_sha256": tree,
        # Recorded so a consumer reading this file knows what the green light
        # was worth, without having to re-derive it.
        "pairing_coverage": pairing_coverage(repo),
        # The tree state these hashes were taken over.  Without it a later
        # disagreement can only be REPORTED, never EXPLAINED -- and this tool
        # used to explain it anyway, by asserting a mechanism it had no way to
        # observe.  See the exit-code table in this module's docstring.
        "provenance": current_provenance(repo),
        "objects": decomp,
        "target_objects": target,
    }
    out = build_dir(repo) / "patch_state.json"
    tmp = out.with_suffix(".json.tmp")
    tmp.write_text(json.dumps(doc, indent=1, sort_keys=True))
    tmp.replace(out)
    print(f"[patch-state] {len(decomp)} decomp + {len(target)} target objects "
          f"verified patched, tree_sha256={tree[:16]} "
          f"-> {out.relative_to(repo)}")
    return 0


def verify_manifest(repo: Path, quiet: bool = False) -> int:
    """Recompute the manifest and refuse if any object drifted from it.

    This is the check a CONSUMER of the tree runs.  It needs no toolchain, no
    compiler and no COFF parsing -- only the manifest and the objects -- and,
    unlike `--check`, it does not depend on any patcher still being active.

    A disagreement is reported as one of THREE different things -- a build in
    flight (5), a tree that has advanced (4), or corruption (1).  See the
    exit-code table in this module's docstring for why that separation exists.
    """
    mpath = build_dir(repo) / "patch_state.json"
    if not mpath.exists():
        print(f"REFUSE: {mpath} is absent -- this build tree has never been "
              f"verified patched.  Run `./tools/ninja-locked` in it.",
              file=sys.stderr)
        return 2
    doc = json.loads(mpath.read_text())

    # Sampled BEFORE the scan and again after it.  tools/ninja-locked holds the
    # lock for a whole build, so any build overlapping this scan is visible at
    # one of the two samples; one sample alone races the build's own start.
    lock_before = build_lock_held(repo)

    sections = (
        ("decomp", doc.get("objects") or {}, decomp_objects(repo),
         "compiled by this repo and rewritten by the six post-compile "
         "patchers"),
        ("target", doc.get("target_objects") or {}, target_objects(repo),
         "dtk-split originals rewritten by the PRE-compile symbol renamer"),
    )
    report, bad = [], False
    for label, recorded, present, blurb in sections:
        drift, missing = [], []
        for rel, ent in sorted(recorded.items()):
            p = repo / rel
            if not p.exists():
                missing.append(rel)
                continue
            st = p.stat()
            if st.st_size != ent["size"] or sha256(p) != ent["sha256"]:
                drift.append(rel)
        have = {str(p.relative_to(repo)) for p in present}
        extra = sorted(have - set(recorded))
        if drift or missing or extra:
            bad = True
        report.append((label, blurb, len(recorded), drift, missing, extra))

    if not bad:
        if not quiet:
            counts = ", ".join(f"{n} {label}" for label, _, n, *_ in report)
            print(f"[patch-state] OK: {counts} objects match "
                  f"{doc['generated_utc']} "
                  f"(tree_sha256={doc['tree_sha256'][:16]})")
        return 0

    n_diff = sum(len(d) + len(m) + len(e) for _, _, _, d, m, e in report)

    # A build was in flight across this scan: the objects were being rewritten
    # BY the build system.  Neither a pass nor corruption -- and on a shared
    # tree with concurrent lanes this is the commonest way the check is reached
    # at all, so it must be said FIRST and said plainly.
    if lock_before or build_lock_held(repo):
        print("=" * 72, file=sys.stderr)
        print("BUILD IN PROGRESS -- THIS TREE IS BEING REWRITTEN RIGHT NOW",
              file=sys.stderr)
        print("=" * 72, file=sys.stderr)
        print(f"{n_diff} object(s) disagree with the manifest written "
              f"{doc.get('generated_utc')}, and tools/ninja-locked's build "
              f"lock ({BUILD_LOCK_NAME}) is HELD -- a build is rewriting them "
              f"as this ran.\n\n"
              f"This is NOT a pass and NOT corruption: the tree is simply not "
              f"in a state anyone can adjudicate. Wait for the build to "
              f"finish and re-run.", file=sys.stderr)
        return 5

    # Only now is a disagreement worth explaining.  `moved` is None when the
    # manifest predates provenance recording -- an unknown cause, which is
    # reported as unknown rather than guessed.
    moved = None
    recorded_prov = doc.get("provenance")
    if isinstance(recorded_prov, dict):
        moved = provenance_moved(recorded_prov, current_provenance(repo))

    print("=" * 72, file=sys.stderr)
    print("BUILD TREE ADVANCED -- A REBUILD IS PENDING" if moved else
          "BUILD TREE DRIFTED -- CAUSE UNDETERMINED" if moved is None else
          "BUILD TREE DRIFTED SINCE IT WAS LAST VERIFIED PATCHED",
          file=sys.stderr)
    print("=" * 72, file=sys.stderr)
    print(f"manifest written {doc.get('generated_utc')} over "
          f"{doc.get('n_objects')} objects", file=sys.stderr)
    for label, blurb, n, drift, missing, extra in report:
        if not (drift or missing or extra):
            continue
        print(f"\n  [{label}] {blurb} ({n} recorded):", file=sys.stderr)
        for name, rows in (("content differs", drift), ("now missing", missing),
                           ("not in the manifest", extra)):
            if rows:
                print(f"    {len(rows)} {name}:", file=sys.stderr)
                for r in rows[:10]:
                    print(f"      {r}", file=sys.stderr)
                if len(rows) > 10:
                    print(f"      ... and {len(rows) - 10} more", file=sys.stderr)
    if moved:
        print("\nThe inputs that decide what a build PRODUCES have moved since "
              "this manifest was written:\n  " + "\n  ".join(moved)
              + "\n\nSo these objects belong to a different state of the tree "
                "than the manifest describes, and a build is owed. Run "
                "`./tools/ninja-locked` and re-check.\n"
                "This is an ordinary pending rebuild and NOT evidence that "
                "anything was corrupted -- but it is equally NOT a licence to "
                "measure: the objects and the manifest still describe "
                "different trees.", file=sys.stderr)
        return 4

    if moved is None:
        print(f"\nThis manifest is schema v{doc.get('manifest_version')}, which "
              f"predates provenance recording (v{MANIFEST_VERSION} records git "
              f"HEAD and the split-input hashes), so WHY these objects differ "
              f"cannot be established FROM IT -- and guessing a cause here is "
              f"exactly the defect this branch exists to avoid.\n"
              f"Re-run a full `./tools/ninja-locked`; the manifest it writes "
              f"can tell a pending rebuild from a corrupted tree.",
              file=sys.stderr)
        return 1

    print("\nNo build holds the build lock, and the tree is at the SAME state "
          "this manifest was taken over -- same git HEAD, same split inputs. "
          "So these objects were rewritten by something OUTSIDE the full build "
          "graph.\n"
          "A DECOMP object in that state was produced by a targeted "
          "`ninja build/.../Foo.obj`, or by `objdiff-cli --build` without "
          "--full-build, so the post-compile patch passes never ran on it.\n"
          "A TARGET object that drifted means the dtk split re-ran, or the "
          "pre-compile symbol renamer has not run since it did -- in which "
          "state every mangled-name lookup answers 'absent' and any negative "
          "result is vacuous (cf. lane FOLDPROVE-2).\n"
          "Re-run a full `./tools/ninja-locked` before measuring anything "
          "from this tree.", file=sys.stderr)
    return 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--repo", default=str(REPO),
                    help="repo root (default: this checkout)")
    ap.add_argument("--check", action="store_true",
                    help="dry-run every patcher; fail if the tree is not a "
                         "fixed point")
    ap.add_argument("--emit", action="store_true",
                    help="write build/<version>/patch_state.json")
    ap.add_argument("--verify-manifest", action="store_true",
                    help="recompute patch_state.json and fail on any drift")
    ap.add_argument("--stamp", help="touch this file on success (ninja edge)")
    ap.add_argument("--min-declared", type=int, default=DEFAULT_MIN_DECLARED,
                    help="refuse if objdiff.json declares fewer than this many "
                         "pairable compiled objects (default: %d).  Lower it "
                         "only for a synthetic fixture -- lowering it on the "
                         "real tree disarms the vacuity check, which is the "
                         "one thing a green light cannot tell you about "
                         "itself." % DEFAULT_MIN_DECLARED)
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    repo = Path(a.repo).resolve()
    if not (a.check or a.emit or a.verify_manifest):
        a.check = a.emit = True
    rc = 0
    if a.check:
        rc = run_check(repo, quiet=a.quiet, min_declared=a.min_declared)
        if rc:
            return rc
    if a.emit:
        rc = emit(repo)
        if rc:
            return rc
    if a.verify_manifest:
        rc = verify_manifest(repo, quiet=a.quiet)
        if rc:
            return rc
    if a.stamp:
        Path(a.stamp).parent.mkdir(parents=True, exist_ok=True)
        Path(a.stamp).write_text("")
    return rc


if __name__ == "__main__":
    sys.exit(main())
