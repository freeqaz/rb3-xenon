#!/usr/bin/env python3
"""Run the six post-compile object patchers over ONE object, off the build graph.

Why this exists
---------------
configure.py hangs the patcher chain off the `post-compile` phony, not off the
compile edge (see the block at configure.py:700-840).  So `ninja <one .obj>` --
and equally a harness that replays the compile argv out of `ninja -t commands`,
which is what decomp-synth's permuter does -- produces RAW COMPILER OUTPUT.
That is harmless while the ruler is `functionRelocDiffs=none`, which never reads
a relocation target's name, and it is a false-near-miss factory at `name_check`,
which is what this repo's own objdiff.json declares.  decomp-synth refuses to
score in that combination (decomp_synth/patch_state.py) rather than throw away
cracks silently.  This script is the condition that refusal asks for.

What it is NOT: a replacement for `post-compile`.  It patches the ONE object it
is pointed at, in place, and it does not touch the build tree unless you point
it at the build tree.  The permuter points it at a private candidate object in
a temp dir, which is why `--obj` is separable from `--unit`.

`--unit` vs `--obj`
-------------------
Five of the six patchers pair our object against the retail TARGET object, and
they find that target from the object's path relative to build/<id>/src.  A
candidate object does not live there.  So the two are separate arguments:

  --unit  the LOGICAL unit path (relative to build/<id>/src) -- decides pairing
  --obj   the actual file to read and rewrite (default: build/<id>/src/<unit>)

Chain order is configure.py's, and it is load-bearing: the five name-rewriting
passes are serialized because they read-modify-write the same bytes, and
eh_boundary is last because it only appends.

Cross-object state
------------------
Only obj_anon_ns_patcher has any: a name->hash index unioned over every RETAIL
object that some compiled object is paired with.  Its inputs are retail objects
and compiled objects' PATHS -- never a compiled object's CONTENTS -- so a
single-TU recompile cannot move it, and per-object patching gives the same
bytes as the full graph.  Proven by control, not by argument:
<decomp-bench>/archive/runs/2026-08-21-permuter-name-check-path/.

That index costs ~1.5 s to build and the permuter runs this per candidate, so
it is memoized to build/<id>/anon_ns_index.pkl behind a fingerprint of every
input (retail obj sizes+mtimes, objdiff.json, the compiled-obj path list).  A
stale or unreadable cache is rebuilt, never trusted; `--no-cache` skips it.

Usage:
    python3 scripts/obj_patch_chain.py --apply --unit system/movie/TexMovie.obj
    python3 scripts/obj_patch_chain.py --apply --unit <rel> --obj /tmp/cand.obj
    python3 scripts/obj_patch_chain.py --apply --pairs-json pairs.json
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import pickle
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

SCRIPTS_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPTS_DIR.parent
BUILD_ID = "45410914"

#: Bumped whenever the pickle's shape or the index derivation changes, so a
#: cache written by an older script can never be read as if it were current.
_CACHE_VERSION = 1


def _load(name: str):
    """Import a sibling patcher by filename (they are scripts, not a package)."""
    spec = importlib.util.spec_from_file_location(name, SCRIPTS_DIR / f"{name}.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


anon_ns = _load("obj_anon_ns_patcher")
dynamic_init = _load("obj_dynamic_init_patcher")
guard = _load("obj_guard_patcher")
bool_mangle = _load("obj_bool_mangle_patcher")
atexit_scope = _load("obj_atexit_scope_patcher")
eh_boundary = _load("obj_eh_boundary_patcher")


# ── obj_anon_ns_patcher's cross-object index ────────────────────────────────


def _fingerprint(obj_dir: Path, src_dir: Path, config_path: Path) -> str:
    """Everything the index reads, cheaply.

    Retail objects by (relpath, size, mtime_ns) -- a re-SPLIT rewrites them and
    moves both.  objdiff.json the same, because it decides the pairing.  And the
    compiled objects' RELPATHS, because `live_targets` is derived from which
    compiled objects exist; their contents are deliberately absent, which is the
    whole reason a candidate recompile does not invalidate this.
    """
    h = hashlib.sha256()
    h.update(f"v{_CACHE_VERSION}\n".encode())
    for root, _dirs, files in sorted(os.walk(obj_dir)):
        for f in sorted(files):
            if not f.endswith(".obj") or f.startswith("auto_"):
                continue
            p = os.path.join(root, f)
            st = os.stat(p)
            h.update(f"{os.path.relpath(p, obj_dir)}\0{st.st_size}\0"
                     f"{st.st_mtime_ns}\n".encode())
    try:
        st = config_path.stat()
        h.update(f"cfg\0{st.st_size}\0{st.st_mtime_ns}\n".encode())
    except OSError:
        h.update(b"cfg\0missing\n")
    for root, _dirs, files in sorted(os.walk(src_dir)):
        for f in sorted(files):
            if f.endswith(".obj"):
                h.update(f"src\0{os.path.relpath(os.path.join(root, f), src_dir)}\n"
                         .encode())
    return h.hexdigest()


def _build_anon_index(obj_dir: Path, src_dir: Path, config_path: Path) -> dict:
    """Reproduce obj_anon_ns_patcher.process_batch's index build, verbatim.

    Kept structurally parallel to that function on purpose: if the two ever
    disagree the per-object path stops being the full graph's answer, and the
    only defence is that this is a transcription short enough to diff by eye.
    """
    orig_by_relpath, decomp_by_relpath = anon_ns.build_obj_mappings(obj_dir, src_dir)
    config_pairs = anon_ns.load_config_pairs(config_path, obj_dir, src_dir)

    live_targets = set()
    for relpath in decomp_by_relpath:
        got, _ = anon_ns.resolve_orig(relpath, orig_by_relpath, config_pairs)
        if got is not None:
            live_targets.add(got)

    orig_index = {}
    g_templates = defaultdict(set)
    g_tokens = defaultdict(set)
    for relpath in sorted(live_targets):
        templates, tokens, weights = anon_ns.index_object(orig_by_relpath[relpath])
        if not templates:
            continue
        orig_index[relpath] = (templates, tokens, weights)
        for k, v in templates.items():
            g_templates[k] |= v
        for k, v in tokens.items():
            g_tokens[k] |= v

    return {
        "orig_by_relpath": orig_by_relpath,
        "config_pairs": config_pairs,
        "orig_index": orig_index,
        "global_index": (dict(g_templates), dict(g_tokens)),
    }


def load_anon_index(obj_dir: Path, src_dir: Path, config_path: Path,
                    cache_path: Path | None) -> dict:
    fp = _fingerprint(obj_dir, src_dir, config_path)
    if cache_path is not None:
        try:
            with open(cache_path, "rb") as fh:
                doc = pickle.load(fh)
            if doc.get("fingerprint") == fp:
                return doc["index"]
        except Exception:
            pass  # corrupt, stale, absent, half-written by a peer -- rebuild
    index = _build_anon_index(obj_dir, src_dir, config_path)
    if cache_path is not None:
        # Atomic: several permuter workers race here, and a half-written pickle
        # read by the next one would be a silent wrong index, not a crash.
        try:
            cache_path.parent.mkdir(parents=True, exist_ok=True)
            fd, tmp = tempfile.mkstemp(dir=str(cache_path.parent), suffix=".tmp")
            with os.fdopen(fd, "wb") as fh:
                pickle.dump({"fingerprint": fp, "index": index}, fh,
                            protocol=pickle.HIGHEST_PROTOCOL)
            os.replace(tmp, cache_path)
        except Exception:
            pass  # the cache is an optimisation; never fail the patch over it
    return index


# ── the chain ───────────────────────────────────────────────────────────────


def _patch_anon_ns(obj_path: Path, unit_rel: str, index: dict) -> int:
    """obj_anon_ns_patcher.process_batch's per-file body, for one object."""
    data = obj_path.read_bytes()
    if not anon_ns.ANON_NS_PATTERN.search(data):
        return 0
    orig_relpath, _via = anon_ns.resolve_orig(
        unit_rel, index["orig_by_relpath"], index["config_pairs"])
    if orig_relpath is None or orig_relpath not in index["orig_index"]:
        return 0
    edits, _stats, unresolved = anon_ns.plan_object(
        data, index["orig_index"][orig_relpath], index["global_index"])
    if unresolved:
        return 0
    changed = {o: h for o, h in edits.items() if data[o:o + 8] != h}
    if not changed:
        return 0
    anon_ns._write_preserving_mtime(obj_path, anon_ns.apply_edits(data, edits))
    return len(changed)


def patch_one(obj_path: Path, unit_rel: str, obj_dir: Path, src_dir: Path,
              index: dict, verbose: bool = False) -> dict:
    """Run all six patchers, in configure.py's order, on one object.

    Every pairing rule below is the one that patcher's own batch pass uses --
    note that only obj_anon_ns_patcher consults objdiff.json; the other four
    paired passes key on the plain relpath and simply skip a unit whose target
    is not there.  Do not "improve" that here: the point is to be the same
    answer as the build graph, including where the build graph does nothing.
    """
    counts = {}
    counts["anon_ns"] = _patch_anon_ns(obj_path, unit_rel, index)

    counts["dynamic_init"] = len(dynamic_init.patch_obj(str(obj_path), apply=True))

    orig = obj_dir / unit_rel
    if orig.exists():
        counts["guard"] = guard.patch_obj_file(
            str(obj_path), str(orig), apply=True)[0]
        counts["bool_mangle"] = bool_mangle.patch_obj_file(
            str(obj_path), str(orig), apply=True)[0]
        try:
            counts["atexit_scope"] = atexit_scope.patch_obj_pair(
                str(orig), str(obj_path), apply=True)["num_renamed"]
        except Exception as exc:
            # Batch swallows a per-pair exception and moves on; mirror that
            # rather than failing a score over one unit's symbol table.
            counts["atexit_scope"] = 0
            if verbose:
                print(f"  atexit_scope ERROR on {unit_rel}: {exc}", file=sys.stderr)
    else:
        counts["guard"] = counts["bool_mangle"] = counts["atexit_scope"] = 0

    n, err = eh_boundary.patch_file(obj_path, True)
    counts["eh_boundary"] = n
    if err and verbose:
        print(f"  eh_boundary ERROR on {unit_rel}: {err}", file=sys.stderr)
    return counts


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--unit", help="unit path relative to --src-dir (decides pairing)")
    ap.add_argument("--obj", help="the file to patch (default: <src-dir>/<unit>)")
    ap.add_argument("--pairs-json",
                    help='JSON list of {"unit": <rel>, "obj": <path>} -- one '
                         "index build amortised over the whole batch")
    ap.add_argument("--obj-dir", default=str(PROJECT_ROOT / "build" / BUILD_ID / "obj"))
    ap.add_argument("--src-dir", default=str(PROJECT_ROOT / "build" / BUILD_ID / "src"))
    ap.add_argument("--objdiff-config", default=str(PROJECT_ROOT / "objdiff.json"))
    ap.add_argument("--cache",
                    default=str(PROJECT_ROOT / "build" / BUILD_ID / "anon_ns_index.pkl"))
    ap.add_argument("--no-cache", action="store_true")
    ap.add_argument("--apply", action="store_true",
                    help="required; this script has no dry run (it is a chain of "
                         "six in-place rewrites, and a dry run of the chain is "
                         "not a dry run of any patcher in it)")
    ap.add_argument("--verbose", "-v", action="store_true")
    args = ap.parse_args(argv)

    if not args.apply:
        ap.error("--apply is required")
    obj_dir, src_dir = Path(args.obj_dir), Path(args.src_dir)

    if args.pairs_json:
        pairs = [(str(d["unit"]), Path(d.get("obj") or src_dir / d["unit"]))
                 for d in json.loads(Path(args.pairs_json).read_text())]
    elif args.unit:
        pairs = [(args.unit, Path(args.obj) if args.obj else src_dir / args.unit)]
    else:
        ap.error("give --unit or --pairs-json")

    missing = [str(p) for _u, p in pairs if not p.exists()]
    if missing:
        print(f"ERROR: no such object: {', '.join(missing)}", file=sys.stderr)
        return 1

    index = load_anon_index(obj_dir, src_dir, Path(args.objdiff_config),
                            None if args.no_cache else Path(args.cache))

    total = defaultdict(int)
    for unit_rel, obj_path in pairs:
        counts = patch_one(obj_path, os.path.normpath(unit_rel), obj_dir, src_dir,
                           index, verbose=args.verbose)
        for k, v in counts.items():
            total[k] += v
        if args.verbose:
            print(f"{unit_rel} -> {obj_path}: "
                  + ", ".join(f"{k}={v}" for k, v in counts.items()))
    print("[patch-chain] " + str(len(pairs)) + " object(s): "
          + ", ".join(f"{k}={total[k]}" for k in
                      ("anon_ns", "dynamic_init", "guard", "bool_mangle",
                       "atexit_scope", "eh_boundary")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
