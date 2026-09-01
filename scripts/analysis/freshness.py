#!/usr/bin/env python3
"""Is this tree MEASURABLE?  Refuse rather than return a number.

The defect this closes
----------------------
Every measurement-consuming tool in this repo reads TWO artifacts that it
assumes correspond, and nothing asserted that they do:

    build/<v>/report.json      the scores, taken at some past moment
    build/<v>/**/*.obj         the objects on disk, re-diffed live NOW

`report.json` is not a description of the objects, it is a score OF them, and
the two drift apart silently.  Measured on this repo 2026-09-01: 244 decomp
objects had been rebuilt outside the full build graph (a targeted
`ninja <one>.obj`, or `objdiff-cli --build` without `--full-build`, which IS
the same command) so the six post-compile patchers -- which are PART OF THE
RULER -- never ran on them.  A separate worktree measured 380 decomp + 1,823
target objects newer than the `report.json` sitting beside them.

The failure is not an error.  It is a plausible, one-directional LOW number:
an unpatched object costs a matched function 100.0 -> 99.7 and a unit
-2.006 pp, with no warning anywhere (see scripts/orchestrator/patch_guard.py
for the measured table).  `scripts/verify_ruler_agreement.py --selftest` was
red for exactly this reason -- 10 disagreements, 7 of them on rows report.json
scored at 100.0 -- and went green with 0 disagreements on a settled tree with
no code change, while the control flip still produced 31.  It was diagnosing
a ruler regression that did not exist.

What it checks, and why each one
--------------------------------
1.  The manifest is NON-VACUOUS.  This project has a long ledger of gates that
    passed on empty input, so an absent, unparseable or implausibly small
    `patch_state.json` REFUSES.  A manifest recording zero objects would
    otherwise verify perfectly.
2.  Object CONTENT identity, delegated to `patch_guard.ensure_patched_tree(
    build=False)` -- which is `verify_objs_patched.py --verify-manifest` plus
    the split-currency guard.  Not reimplemented here: the oracle exists,
    needs no toolchain, and costs ~0.3 s.
    ⚠ Content, deliberately NOT mtime.  A rebuild that reproduces identical
    bytes bumps mtime and changes nothing that can be measured; refusing on
    that would be a false alarm, and a guard that cries wolf gets switched off.
3.  The report is NON-VACUOUS -- `total_code`, `total_functions` and the unit
    list all > 0, every numeric `int()`-coerced because report.json is
    protobuf-JSON: defaults are OMITTED and several numerics are JSON STRINGS.
4.  The report is NOT OLDER than the manifest.  Combined with (2) this is the
    correspondence argument: (2) proves the objects still hold the content they
    held at manifest time T, and (4) proves the report was generated at or
    after T, so it scored that content.

`build=False` is not optional politeness: this helper must never trigger a
build.  It reads manifests.  A guard that costs five minutes is a guard people
route around.

Using it
--------
    from scripts.analysis import freshness
    freshness.ensure_measurable(proj, consumer="reachability census")

It RAISES `StaleTreeError`.  It does not warn and it does not return a
degraded answer -- a helper you must remember to check is not a guard.
`allow_stale=True` (wire it to an explicit `--allow-stale` flag) downgrades the
refusal to a loud banner on stderr NAMING what is stale, for the one legitimate
case: deliberate analysis of a known-stale artifact.
"""

from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path

__all__ = ["StaleTreeError", "ensure_measurable", "add_freshness_args"]

#: A manifest recording fewer objects than this is treated as vacuous rather
#: than as a clean bill of health.  The real tree records ~4,293 (1,205 decomp
#: + 3,088 target); anything near zero means the manifest is a stub, was
#: written against an empty build dir, or belongs to another tree.
MIN_MANIFEST_OBJECTS = 500


class StaleTreeError(RuntimeError):
    """report.json and the objects on disk do not provably correspond.

    Hard by design.  A measurement taken across this boundary is not a
    slightly worse measurement; it is a score of objects that are no longer
    there, and it reads LOW in one direction only.
    """


def _load_json(path: Path, what: str) -> dict:
    if not path.exists():
        raise StaleTreeError(
            f"{what} is absent at {path}. This tree has never been verified "
            f"measurable. Run `./tools/ninja-locked` in it -- NEVER "
            f"`ninja <one>.obj`, which skips the post-compile patchers."
        )
    try:
        return json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        raise StaleTreeError(f"{what} at {path} is unreadable: {exc}") from None


def _build_dir(project_dir: Path) -> Path:
    hits = sorted(project_dir.glob("build/*/patch_state.json"))
    if not hits:
        raise StaleTreeError(
            f"no build/*/patch_state.json under {project_dir} -- there is no "
            f"record of this tree ever having been built to the post-compile "
            f"fixed point, so nothing here is measurable. Run "
            f"`./tools/ninja-locked`."
        )
    return hits[0].parent


def _check_patch_state(build: Path) -> tuple[dict, Path]:
    path = build / "patch_state.json"
    doc = _load_json(path, "the patch-state manifest")
    n = int(doc.get("n_objects") or 0)
    n_decomp = int(doc.get("n_decomp_objects") or 0)
    n_target = int(doc.get("n_target_objects") or 0)
    if n < MIN_MANIFEST_OBJECTS or n_decomp <= 0 or n_target <= 0:
        raise StaleTreeError(
            f"VACUOUS MANIFEST: {path} records {n} objects "
            f"({n_decomp} decomp / {n_target} target), below the "
            f"{MIN_MANIFEST_OBJECTS} floor. An empty manifest verifies "
            f"perfectly against an empty tree, which is how a gate passes "
            f"while proving nothing. Refusing."
        )
    return doc, path


def _check_report(build: Path, manifest_path: Path) -> tuple[Path, str]:
    path = build / "report.json"
    doc = _load_json(path, "report.json")
    # protobuf-JSON: defaults OMITTED, numerics may be JSON STRINGS.
    m = doc.get("measures") or {}
    total_code = int(m.get("total_code") or 0)
    total_fns = int(m.get("total_functions") or 0)
    n_units = len(doc.get("units") or [])
    if total_code <= 0 or total_fns <= 0 or n_units <= 0:
        raise StaleTreeError(
            f"VACUOUS REPORT: {path} has total_code={total_code}, "
            f"total_functions={total_fns}, {n_units} units. An empty report "
            f"agrees with anything. Refusing."
        )
    r_mtime = path.stat().st_mtime
    m_mtime = manifest_path.stat().st_mtime
    if r_mtime < m_mtime:
        raise StaleTreeError(
            f"STALE REPORT: {path.name} was written "
            f"{m_mtime - r_mtime:.1f} s BEFORE the patch-state manifest "
            f"{manifest_path.name}. The objects on disk were verified patched "
            f"at a moment this report predates, so the report scored a "
            f"different set of objects than the one any live diff will read. "
            f"Regenerate it with a full `./tools/ninja-locked`."
        )
    return path, (f"report.json OK ({total_fns:,} fns / {total_code:,} B over "
                  f"{n_units} units, not older than the manifest)")


def _patch_guard(project_dir: Path):
    """Load the tree's OWN patch_guard, not this checkout's.

    The guard must describe the tree being measured; loading a sibling repo's
    copy would answer about the wrong build dir.
    """
    path = project_dir / "scripts" / "orchestrator" / "patch_guard.py"
    if not path.exists():
        raise StaleTreeError(
            f"{path} is absent, so this tree's patch state cannot be "
            f"established at all. Refusing to measure rather than assuming it "
            f"is fine."
        )
    spec = importlib.util.spec_from_file_location("_rb3x_patch_guard", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def ensure_measurable(project_dir, *, need_report: bool = True,
                      allow_stale: bool = False,
                      consumer: str = "this tool") -> str:
    """Assert `project_dir` can be measured; raise `StaleTreeError` if not.

    Returns a one-line note worth echoing.  Never builds.
    """
    project_dir = Path(project_dir).resolve()
    try:
        build = _build_dir(project_dir)
        doc, manifest_path = _check_patch_state(build)
        notes = [f"manifest {doc.get('generated_utc')} / "
                 f"{int(doc.get('n_objects') or 0):,} objects"]

        guard = _patch_guard(project_dir)
        try:
            # build=False: this is a READ-ONLY precondition. It verifies; it
            # must not compile.
            notes.append(guard.ensure_patched_tree(project_dir, build=False))
        except guard.UnpatchedTreeError as exc:
            raise StaleTreeError(str(exc)) from None

        if need_report:
            _, note = _check_report(build, manifest_path)
            notes.append(note)
    except StaleTreeError as exc:
        if not allow_stale:
            raise
        banner = "!" * 72
        print(f"\n{banner}\n!! --allow-stale: MEASURING A TREE THAT IS NOT "
              f"VERIFIABLY FRESH\n!! consumer: {consumer}\n{banner}\n"
              f"{exc}\n{banner}\n!! Every number below describes objects that "
              f"may no longer be on disk, and the bias is ONE-DIRECTIONAL "
              f"(low).\n!! Do NOT record these figures as a measurement.\n"
              f"{banner}\n", file=sys.stderr)
        return "STALE (--allow-stale)"

    return " | ".join(n for n in notes if n)


def add_freshness_args(ap) -> None:
    """Register the standard override flag, spelled identically everywhere."""
    ap.add_argument(
        "--allow-stale", action="store_true",
        help="measure even if report.json and the objects on disk do not "
             "provably correspond (prints a loud banner; NOT a measurement)")


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser(description="Is this tree measurable?")
    ap.add_argument("project", nargs="?", default=".")
    ap.add_argument("--no-report", action="store_true",
                    help="check objects only, not report.json correspondence")
    add_freshness_args(ap)
    a = ap.parse_args()
    try:
        note = ensure_measurable(a.project, need_report=not a.no_report,
                                 allow_stale=a.allow_stale,
                                 consumer="freshness.py CLI")
    except StaleTreeError as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        raise SystemExit(1)
    # Do not print "MEASURABLE" for the override path: that verdict line gets
    # relayed on its own, and this repo has already lost a lane to a PASS whose
    # disqualifier lived in a parenthetical.
    print("NOT VERIFIED FRESH (proceeding under --allow-stale)"
          if note.startswith("STALE") else f"MEASURABLE: {note}")
