#!/usr/bin/env python3
"""live_units.py -- shared helper: which build/45410914/{obj,asm} files are LIVE.

Not every *.obj/*.s under build/45410914/{obj,asm}/ is a live artifact of the
CURRENT splits.txt/objects.json configuration. Stale files pile up whenever a
split range moves or an auto-carve unit is renamed/removed, because ninja only
ADDS outputs -- it never deletes orphans from a prior split layout. Measured:
~7,013 auto_03_*_text.{obj,s} pairs on disk, only ~2,395 LIVE; ~2,439 of the
stale ones predate the 2026-07-15 TU5 flip and contain bytes for a DIFFERENT
binary revision that occurs nowhere in the current target. Two independent
analyses were silently corrupted by globbing the stale set (a 36% inflation
and a 1.91x over-count) before this helper existed.

`objdiff.json` (dtk's per-unit manifest, regenerated on every split run) is
the single source of truth for what is currently LIVE: it lists every unit's
`target_path`. Anything glob-matched under obj/ or asm/ whose basename isn't
in that set is a stale orphan.

Usage:
    from live_units import live_unit_names, filter_live
    kept = filter_live(glob.glob(".../asm/**/*.s", recursive=True), repo_root)
"""
import json
import os


def _load_objdiff(repo_root):
    with open(os.path.join(repo_root, "objdiff.json")) as f:
        return json.load(f)


def live_target_paths(repo_root):
    """Every LIVE unit's target .obj path (repo-root-relative or absolute, as
    stored in objdiff.json), normalized to an absolute path under repo_root."""
    d = _load_objdiff(repo_root)
    out = set()
    for u in d.get("units", []):
        tp = u.get("target_path")
        if tp:
            out.add(os.path.normpath(os.path.join(repo_root, tp)))
    return out


def live_unit_names(repo_root):
    """Basenames (no extension) of every LIVE unit, e.g. 'auto_03_82270000_text'.
    Both the .obj (in objdiff.json) and its sibling .s (in build/45410914/asm/,
    not itself listed in objdiff.json) share this stem, so basename is the
    right join key for filtering either directory's glob."""
    return {os.path.splitext(os.path.basename(p))[0] for p in live_target_paths(repo_root)}


def filter_live(paths, repo_root):
    """Given obj/asm file paths, return only the ones whose basename (no
    extension) is a currently-LIVE unit. Callers wanting a stale count can
    diff len(paths) - len(result)."""
    live = live_unit_names(repo_root)
    return [p for p in paths if os.path.splitext(os.path.basename(p))[0] in live]


def repo_root_from_build_subdir(build_subdir):
    """Best-effort repo root for a path like '<root>/build/45410914/{obj,asm}':
    strip the 3 known trailing components. Lets callers pass a custom
    --asm-dir / --obj-dir (e.g. pointing at a different worktree) and still
    resolve THAT worktree's own objdiff.json rather than the caller's."""
    p = os.path.abspath(build_subdir)
    return os.path.dirname(os.path.dirname(os.path.dirname(p)))
