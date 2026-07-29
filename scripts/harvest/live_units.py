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

    ★DO NOT USE THIS AS A FILTER JOIN KEY -- kept only for callers that want a
    display/name set. Basename is NOT injective over units: measured on
    2026-07-29, 21 live-unit basenames COLLIDE (Rnd, Utl, Dir, Synth, the
    FxSend* family), and the tree carries BOTH flat and nested files for the
    same stem (asm/Rnd.s, asm/system/rnddx9/Rnd.s, asm/system/rndobj/Rnd.s).
    A basename join therefore admits stale orphans that share a live stem --
    exactly the small nested files most likely to read as a real finding.
    Use live_rel_keys()/filter_live() instead."""
    return {os.path.splitext(os.path.basename(p))[0] for p in live_target_paths(repo_root)}


def _rel_key(path):
    """Key a build artifact by its path RELATIVE to its obj/ or asm/ root, minus
    extension: 'system/rndobj/Rnd', 'MasterAudio'. This is injective where the
    basename is not."""
    p = os.path.normpath(os.path.abspath(path)).replace(os.sep, "/")
    for marker in ("/build/45410914/obj/", "/build/45410914/asm/"):
        i = p.find(marker)
        if i != -1:
            return os.path.splitext(p[i + len(marker):])[0]
    return os.path.splitext(os.path.basename(p))[0]


def live_rel_keys(repo_root):
    """Relative-path keys (no extension) of every LIVE unit. The .obj listed in
    objdiff.json and its sibling .s under build/45410914/asm/ share this key."""
    return {_rel_key(p) for p in live_target_paths(repo_root)}


def filter_live(paths, repo_root):
    """Given obj/asm file paths, return only the ones that are a currently-LIVE
    unit, joined on the RELATIVE PATH under obj//asm/ (not the basename -- see
    live_unit_names). Callers wanting a stale count can diff
    len(paths) - len(result)."""
    live = live_rel_keys(repo_root)
    return [p for p in paths if _rel_key(p) in live]


def repo_root_from_build_subdir(build_subdir):
    """Best-effort repo root for a path like '<root>/build/45410914/{obj,asm}':
    strip the 3 known trailing components. Lets callers pass a custom
    --asm-dir / --obj-dir (e.g. pointing at a different worktree) and still
    resolve THAT worktree's own objdiff.json rather than the caller's."""
    p = os.path.abspath(build_subdir)
    return os.path.dirname(os.path.dirname(os.path.dirname(p)))
