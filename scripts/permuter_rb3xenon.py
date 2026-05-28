#!/usr/bin/env python3
"""rb3-xenon project shim for the (symlinked) DC3 source permuter.

Why this exists
---------------
`scripts/permuter` is a symlink into ../dc3-decomp/scripts/permuter (the same
package rb3-Wii reuses). That package's project autodetection
(`scripts.permuter.project`) only knows two projects:

    - DC3 (Dance Central 3): title 373307D9, MSVC PPC cl.exe, .obj, `cd dir && ...`
    - RB3 (Rock Band 3 Wii): title SZBE69_B8, MetroWerks mwcceppc, .o

rb3-xenon is *neither*: it's the Xbox-360 RB3 retail decomp — MSVC PPC cl.exe
(.obj, /Fo, like DC3) but with title id **45410914** and a build dir of
`build/45410914`. Worse, the autodetector's name heuristic sees "rb3" in our
repo path and mis-classifies us as the Wii/mwcceppc project (.o, -o), which
points every build/objdiff path at a directory that doesn't exist.

I cannot edit `project.py` to teach it about 45410914 because it physically
lives inside the dc3-decomp repo (reached through the symlink) — editing it
would corrupt DC3's tree and break the shared package for other repos.

So this shim lives in *our* scripts/ (outside the symlink) and monkeypatches
`scripts.permuter.project` at process start, BEFORE `scan_and_permute`/`scorer`
import their project config (scan_and_permute caches `_project` at module
import time). It forces the DC3 / MSVC codepath but overrides `build_id` to
45410914, then re-execs the requested permuter module as __main__.

Usage (mirror of the SKILL invocation, just swap the leading module):

    venv/bin/python -m scripts.permuter_rb3xenon -m scripts.permuter.scan_and_permute \
        --symbol 'Geo::OnSide' --max-rounds 10 --max-variants 100 ...

It also honours being run with the scan_and_permute args directly (no inner
`-m`), defaulting the target module to scripts.permuter.scan_and_permute.
"""

from __future__ import annotations

import os
import runpy
import sys
from pathlib import Path

# rb3-xenon project facts.
_BUILD_ID = "45410914"


def _install_project_patch() -> None:
    import scripts.permuter.project as project

    # Force the MSVC (DC3-style) codepath everywhere project_type is consulted:
    # .obj extension, /Fo output flag, cd-or-no-cd parse handled by the DC3
    # branch (our ninja command has no `cd` prefix, so parse_ninja_command
    # returns cwd=None and the scorer falls back to repo_root — which is exactly
    # where our compile command runs from). The only DC3 fact that's wrong for
    # us is the title id, which we override below.
    os.environ.setdefault("PERMUTER_PROJECT", "dc3")

    ProjectType = project.ProjectType
    _orig_make = project._make_config

    def _make_config_rb3xenon(project_type, repo_root):  # noqa: ANN001
        cfg = _orig_make(ProjectType.DC3, repo_root)
        # Rebuild the frozen dataclass with our build id.
        import dataclasses
        return dataclasses.replace(cfg, build_id=_BUILD_ID)

    def _detect_rb3xenon(repo_root):  # noqa: ANN001
        return ProjectType.DC3

    project._make_config = _make_config_rb3xenon
    project._detect_project_type = _detect_rb3xenon
    # The config is lru_cached on repo_root; clear it so the patched factory is
    # used (scan_and_permute may have already imported project, but the cache
    # is only populated on first get_project_config() call).
    project._get_project_config_cached.cache_clear()

    _patch_target_obj_mapping(project)


def _patch_target_obj_mapping(project) -> None:  # noqa: ANN001
    """Fix target-obj path derivation for rb3-xenon's flat obj/ layout.

    DC3's ProjectConfig.target_obj_for_base_obj() derives the target obj by
    mirroring the src subtree:
        build/<id>/src/system/math/Rot.obj -> build/<id>/obj/system/math/Rot.obj
    But rb3-xenon's dtk split emits *flat* target objs keyed by basename:
        build/45410914/obj/Rot.obj
    (per objdiff.json's target_path). The src-subtree mirror points objdiff at a
    file that doesn't exist, so every variant score errors out.

    We rebind the method to consult objdiff.json's authoritative
    base_path -> target_path map, falling back to a flat obj/<basename>.obj.
    """
    def target_obj_for_base_obj_rb3xenon(self, base_obj):  # noqa: ANN001
        repo_root = self.repo_root
        abs_base = base_obj if Path(base_obj).is_absolute() else repo_root / base_obj
        abs_base = Path(abs_base)
        mapping = _build_base_to_target_map(repo_root)
        # Key the map by both relative and absolute base paths.
        try:
            rel_base = str(abs_base.relative_to(repo_root))
        except ValueError:
            rel_base = str(base_obj)
        target_rel = mapping.get(rel_base) or mapping.get(str(base_obj))
        if target_rel:
            return repo_root / target_rel
        # Fallback: rb3-xenon's flat obj/ layout keyed by basename.
        return repo_root / "build" / _BUILD_ID / "obj" / abs_base.name

    project.ProjectConfig.target_obj_for_base_obj = target_obj_for_base_obj_rb3xenon


def _build_base_to_target_map(repo_root: Path) -> dict:
    import json
    mapping: dict[str, str] = {}
    objdiff_json = repo_root / "objdiff.json"
    try:
        data = json.loads(objdiff_json.read_text())
    except (OSError, ValueError):
        return mapping
    for unit in data.get("units", []):
        base = unit.get("base_path")
        target = unit.get("target_path")
        if base and target:
            mapping[base] = target
    return mapping


def main() -> None:
    _install_project_patch()

    argv = sys.argv[1:]
    if argv and argv[0] == "-m":
        module = argv[1]
        sys.argv = [module] + argv[2:]
    else:
        module = "scripts.permuter.scan_and_permute"
        sys.argv = [module] + argv

    runpy.run_module(module, run_name="__main__", alter_sys=True)


if __name__ == "__main__":
    # Ensure repo root is importable as the `scripts` package root.
    repo_root = Path(__file__).resolve().parent.parent
    if str(repo_root) not in sys.path:
        sys.path.insert(0, str(repo_root))
    main()
