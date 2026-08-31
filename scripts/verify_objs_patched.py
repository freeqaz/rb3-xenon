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
"""

import argparse
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
MANIFEST_VERSION = 2


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
    """
    mpath = build_dir(repo) / "patch_state.json"
    if not mpath.exists():
        print(f"REFUSE: {mpath} is absent -- this build tree has never been "
              f"verified patched.  Run `./tools/ninja-locked` in it.",
              file=sys.stderr)
        return 2
    doc = json.loads(mpath.read_text())

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

    print("=" * 72, file=sys.stderr)
    print("BUILD TREE DRIFTED SINCE IT WAS LAST VERIFIED PATCHED", file=sys.stderr)
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
    print("\nA DECOMP object that changed without the manifest being rewritten "
          "was produced OUTSIDE the full build graph (a targeted "
          "`ninja build/.../Foo.obj`, or `objdiff-cli --build` without "
          "--full-build), so the post-compile patch passes never ran on it.\n"
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
