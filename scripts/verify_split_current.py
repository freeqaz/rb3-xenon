#!/usr/bin/env python3
"""Refuse to measure a target object tree that did not come from the current split.

Why this exists
---------------
``report.json`` is produced by diffing thousands of pairs of objects.  The BASE
side (``build/<v>/src/**.obj``) is compiled by ninja edges, so ninja knows about
it.  The TARGET side (``build/<v>/obj/**``) is different in kind: it is written
by ``dtk <fmt> split``, whose only DECLARED ninja output is
``build/<v>/config.json``.  The thousands of target objects are undeclared side
effects.  No ninja edge names them and nothing stats them.

That matters because their CONTENT depends on ``config/<v>/symbols.txt``: dtk
writes each function under the name symbols.txt gives its address, so a
symbols.txt edit rewrites the symbol tables of every unit it touches.  A report
taken against target objects split from a DIFFERENT symbols.txt is a different
measurement, and it is silent -- the mispaired functions read 0.0% rather than
erroring.

Reproduced in the sibling repo ``dc3-decomp`` (Xbox 360 MSVC, title 373307D9)
on 2026-08-21, in one worktree, with one objdiff-cli (4.2.7 / 76c8da87e040),
with the report cache COLD on both runs and an unchanged symbols.txt on disk::

    report started 2 s into `dtk xex split`   ->  29,497 matched functions
    the same command after it finished        ->  29,838

A 341-function gap from the same tree and the same command, discriminated only
by WHEN it ran.  The first run neither failed nor warned.  There is a second,
DETERMINISTIC form: restore symbols.txt with an *older* mtime than config.json
(``cp -a``, ``tar -x``, a reflinked worktree) and ``ninja -n`` does not plan
SPLIT at all -- so the report measures objects that disagree with the config
indefinitely.

Why this is not an import of dc3's script
-----------------------------------------
dc3 landed this first, and its contract is the model here: bracket the split,
then refuse rather than return a number.  Its *implementation* hardcodes
``VERSION = "373307D9"``, a fixed tuple of config paths and dc3's own build
layout, so importing it would either answer about the wrong title or raise.
The same lesson was learned porting the sibling patch-state verifier: the
repo-agnostic version reads what it needs out of the build system instead of
assuming it.  So this module DISCOVERS:

  * the split edge, the build directory and the config file, out of
    ``build.ninja``;
  * the split's input set, in priority order, from dtk's own records --
    ``<build>/split_manifest.json`` (dtk declares every file it wrote and read;
    added 2026-08-22), else ``<build>/dep`` (dtk's depfile), else the
    ``object``/``symbols``/``splits``/``map``/``selfile`` fields of the config
    itself.

Which source was used is printed, because a guard whose coverage you cannot see
is a guard you cannot size.

What this checks
----------------
``--begin`` / ``--complete`` bracket the split, writing
``<build>/split_inputs.stamp``.  ``--check`` passes only if:

  * a record exists (a tree that never recorded a split cannot be vouched for);
  * that record says ``complete``, not ``running``; and
  * the hashes of the split's config inputs still equal the recorded ones.

The ``running`` state is not decoration.  The reproduction above is a report
that overlapped a split rewriting the very objects it was reading, and the
input hashes ALONE do not catch it: a split re-run with an unchanged
symbols.txt matches its own record the whole time it is running.

Once dtk emits ``split_manifest.json``, that file is the record and the stamp
becomes belt-and-braces: the manifest is written LAST, so a split that is
mid-rewrite with changed inputs already reads red, and a split that is
mid-rewrite with unchanged inputs is writing bytes identical to the ones
already there.

What this deliberately does NOT check
-------------------------------------
The target objects' own bytes.  Two reasons, one of them measured here:

  * it would be a different assertion ("nobody edited a target object"), and
  * in THIS repo it would be wrong.  ``scripts/obj_target_symbol_renamer.py``
    rewrites target objects in place after every split -- measured 2026-08-22,
    1,826 of 3,091 objects patched, 85,184 symbol renames -- so target-object
    bytes are legitimately not what dtk wrote.

Exit codes
----------
    0   the objects on disk correspond to the current split config
    1   they do not, or the state cannot be established (message says which)
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

STAMP_NAME = "split_inputs.stamp"
MANIFEST_NAME = "split_manifest.json"

STATE_RUNNING = "running"
STATE_COMPLETE = "complete"

#: Config keys that name a file the split reads.  Used only as the last-resort
#: input source, when neither dtk's manifest nor its depfile is on disk.
CONFIG_INPUT_KEYS = ("object", "symbols", "splits", "map", "selfile")

#: Inputs large enough that hashing them on every measurement buys nothing this
#: project can act on -- the original binary is immutable.  Recorded by SIZE so
#: a swapped binary is still visible, but not digested.
SIZE_ONLY_SUFFIXES = (".xex", ".dol", ".elf", ".exe", ".rel")


class StaleSplitError(RuntimeError):
    """The target object tree does not correspond to the current split config."""


# --------------------------------------------------------------------------
# Discovery: never assume this repo's layout, read it out of the build system
# --------------------------------------------------------------------------

#: `build <out> [| <implicit outputs>...] : split <config> [| <implicit deps>...]`
#: The implicit-output clause is not optional decoration: this guard's own
#: wiring adds `| build/<v>/split_inputs.stamp` there, and the first version of
#: this regex did not allow it -- so installing the guard broke the guard's
#: ability to find its own edge. Caught by the test suite, which is the point of
#: having one.
_SPLIT_EDGE_RE = re.compile(
    r"^build\s+(?P<out>\S*config\.json)(?P<implicit_outputs>[^:\n]*?)\s*:\s*"
    r"split\s+(?P<cfg>\S+)",
    re.MULTILINE,
)


def discover_split_edge(project_dir: Path) -> tuple[Path, Path] | None:
    """Return ``(build_dir, config_path)`` for this repo's split edge.

    Read out of ``build.ninja`` rather than assumed, so this file works
    unmodified in a repo with a different title id, a different splitter
    (``dol split`` vs ``xex split``), or more than one build directory.
    Returns ``None`` when the repo has no split edge at all -- which is a
    legitimate state (a not-yet-configured tree), not a failure.
    """
    ninja = project_dir / "build.ninja"
    try:
        text = ninja.read_text(errors="replace")
    except OSError:
        return None
    # ninja wraps long lines with `$\n`; unwrap before matching.
    text = text.replace("$\n", " ")
    m = _SPLIT_EDGE_RE.search(text)
    if not m:
        return None
    out = Path(m.group("out"))
    return out.parent, Path(m.group("cfg"))


def _parse_depfile(path: Path) -> list[str]:
    """The dependency paths out of a make-style depfile, ignoring the target."""
    try:
        text = path.read_text(errors="replace")
    except OSError:
        return []
    text = text.replace("\\\n", " ")
    _, _, rhs = text.partition(":")
    return [p for p in rhs.split() if p]


def _config_declared_inputs(project_dir: Path, config_path: Path) -> list[str]:
    try:
        import yaml  # noqa: PLC0415  (optional; only the last-resort path needs it)
    except ImportError:
        return []
    try:
        doc = yaml.safe_load((project_dir / config_path).read_text()) or {}
    except (OSError, Exception):  # noqa: BLE001 - a broken config is "no inputs"
        return []
    base = doc.get("base") if isinstance(doc.get("base"), dict) else doc
    out = []
    for key in CONFIG_INPUT_KEYS:
        val = base.get(key)
        if isinstance(val, str) and val:
            out.append(val)
    return out


def split_inputs(project_dir: Path, build_dir: Path, config_path: Path) -> tuple[list[str], str]:
    """The files whose CONTENT decides what the split writes, and where we got them.

    Priority order, most authoritative first.  The source is returned so the
    caller can state its denominator: an input set of 1 is a green check over
    almost nothing, and the difference is invisible unless it is printed.
    """
    manifest = read_manifest(project_dir, build_dir)
    if manifest and manifest.get("inputs"):
        return sorted(manifest["inputs"]), f"dtk manifest ({build_dir / MANIFEST_NAME})"

    dep = _parse_depfile(project_dir / build_dir / "dep")
    if dep:
        return sorted(set(dep) | {str(config_path)}), f"dtk depfile ({build_dir / 'dep'})"

    declared = _config_declared_inputs(project_dir, config_path)
    if declared:
        return sorted(set(declared) | {str(config_path)}), f"config fields ({config_path})"

    return [str(config_path)], f"config path only ({config_path}) -- WEAK"


# --------------------------------------------------------------------------
# Hashing and the stamp
# --------------------------------------------------------------------------

def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def hash_inputs(project_dir: Path, inputs: list[str]) -> dict:
    out: dict[str, object] = {}
    for rel in inputs:
        p = project_dir / rel
        if not p.exists():
            out[rel] = None
        elif p.suffix.lower() in SIZE_ONLY_SUFFIXES:
            out[rel] = f"size:{p.stat().st_size}"
        else:
            out[rel] = _sha256(p)
    return out


def read_manifest(project_dir: Path, build_dir: Path) -> dict | None:
    p = project_dir / build_dir / MANIFEST_NAME
    try:
        doc = json.loads(p.read_text())
    except (OSError, ValueError):
        return None
    return doc if isinstance(doc, dict) and "outputs" in doc else None


def read_stamp(project_dir: Path, build_dir: Path) -> dict | None:
    p = project_dir / build_dir / STAMP_NAME
    try:
        return json.loads(p.read_text())
    except (OSError, ValueError):
        return None


def write_stamp(project_dir: Path, build_dir: Path, state: str) -> Path:
    inputs, source = split_inputs(project_dir, build_dir, discover_config(project_dir, build_dir))
    p = project_dir / build_dir / STAMP_NAME
    p.parent.mkdir(parents=True, exist_ok=True)
    doc = {
        "state": state,
        "input_source": source,
        "inputs": hash_inputs(project_dir, inputs),
        "pid": os.getpid(),
        "unix_time": time.time(),
        "note": (
            "Written by scripts/verify_split_current.py around the dtk split. "
            "`state` is `running` between --begin and --complete; a check that "
            "sees `running` is looking at a tree mid-rewrite."
        ),
    }
    p.write_text(json.dumps(doc, indent=2, sort_keys=True) + "\n")
    return p


def discover_config(project_dir: Path, build_dir: Path) -> Path:
    edge = discover_split_edge(project_dir)
    if edge and edge[0] == build_dir:
        return edge[1]
    # A stamp written before build.ninja exists still needs SOME config path.
    return Path("config") / build_dir.name / "config.yml"


# --------------------------------------------------------------------------
# The check
# --------------------------------------------------------------------------

def _describe_drift(was_inputs: dict, now_inputs: dict) -> list[str]:
    drift = []
    for key in sorted(set(was_inputs) | set(now_inputs)):
        was, now = was_inputs.get(key), now_inputs.get(key)
        if was != now:
            drift.append(f"    {key}\n      split with: {was}\n      on disk now: {now}")
    return drift


def check(project_dir: Path) -> str:
    """Return a one-line note, or raise :class:`StaleSplitError` naming the drift."""
    project_dir = Path(project_dir).resolve()
    edge = discover_split_edge(project_dir)
    if edge is None:
        raise StaleSplitError(
            f"REFUSING TO VOUCH FOR {project_dir}: no `split` edge found in "
            f"{project_dir / 'build.ninja'}.\n\n"
            f"Either the tree is not configured (run configure.py), or the "
            f"build system changed shape and this guard's discovery needs "
            f"updating. It does NOT mean the split is fine -- an unconfigured "
            f"tree has no split to be current."
        )
    build_dir, config_path = edge

    manifest = read_manifest(project_dir, build_dir)
    stamp = read_stamp(project_dir, build_dir)

    if manifest is None and stamp is None:
        raise StaleSplitError(
            f"REFUSING TO VOUCH FOR {project_dir}: neither "
            f"{build_dir / MANIFEST_NAME} (written by dtk) nor "
            f"{build_dir / STAMP_NAME} (written by this guard around the split) "
            f"exists, so there is NO record of which config produced "
            f"{build_dir / 'obj'}.\n\n"
            f"The target objects are not a declared ninja output, so their age "
            f"cannot be inferred from mtimes either. Run `ninja` in that "
            f"directory (the split edge writes the stamp) and retry."
        )

    # The stamp is the only thing that can see a split IN FLIGHT, and that is
    # ALL it is consulted for when dtk's own manifest is present.  Ordering
    # matters: an in-flight split with unchanged inputs leaves a manifest that
    # still agrees with disk, so the manifest alone would pass it.
    if stamp is not None:
        state = stamp.get("state")
        if state != STATE_COMPLETE:
            raise StaleSplitError(
                f"REFUSING TO VOUCH FOR {project_dir}: the split is recorded as "
                f"`{state}`, not `{STATE_COMPLETE}`.\n\n"
                f"Either the split is running right now and is rewriting "
                f"{build_dir / 'obj'} underneath you, or the last one died "
                f"partway and left a mixed tree. A report taken over a "
                f"half-split tree does not fail -- it silently reads the "
                f"PRE-split number (measured in dc3-decomp 2026-08-21: 29,497 "
                f"instead of 29,838, a 341-function gap, no warning). Wait for "
                f"the build to finish, or re-run the build."
            )

    if manifest is not None:
        # dtk records sha1; this module's own stamp records sha256 or a size
        # sentinel.  Compare in dtk's terms rather than converting, so the two
        # records stay independent instruments rather than one dressed as two.
        recorded = {k: v.get("sha1") for k, v in (manifest.get("inputs") or {}).items()}
        drift = []
        for rel, want in sorted(recorded.items()):
            p = project_dir / rel
            if not p.exists():
                drift.append(f"    {rel}\n      split with: {want}\n      on disk now: MISSING")
                continue
            h = hashlib.sha1()
            with open(p, "rb") as fh:
                for chunk in iter(lambda: fh.read(1 << 20), b""):
                    h.update(chunk)
            if h.hexdigest() != want:
                drift.append(
                    f"    {rel}\n      split with: {want}\n      on disk now: {h.hexdigest()}"
                )
        if drift:
            raise StaleSplitError(
                f"REFUSING TO VOUCH FOR {project_dir}: dtk's own "
                f"{build_dir / MANIFEST_NAME} says {build_dir / 'obj'} was "
                f"written from inputs that are no longer on disk.\n\n"
                + "\n".join(drift)
                + "\n\nRe-run the build to re-split, then retry."
            )
        return (
            f"split current ({len(recorded)} inputs match dtk's own "
            f"{build_dir / MANIFEST_NAME}, {len(manifest.get('outputs') or {})} "
            f"declared outputs)"
        )

    # Stamp-only tree (dtk predating the manifest).  Hash exactly the keys the
    # stamp recorded -- NOT the currently-preferred input source.  Recomputing
    # the key set here made a fresh manifest look like drift on every key the
    # stamp had and the manifest did not: `on disk now: None` for files that
    # were sitting right there.  A guard whose red means "I changed my mind
    # about what to look at" is worse than no guard.
    stamp_inputs = stamp.get("inputs") or {}
    live = hash_inputs(project_dir, sorted(stamp_inputs))
    drift = _describe_drift(stamp_inputs, live)
    if drift:
        raise StaleSplitError(
            f"REFUSING TO VOUCH FOR {project_dir}: {build_dir / 'obj'} was "
            f"split from a DIFFERENT config than the one on disk.\n\n"
            + "\n".join(drift)
            + f"\n\ndtk writes each function under the name symbols.txt "
            f"gives its address, so the target objects currently name "
            f"functions this config does not. Every such function reads "
            f"0.0% and NOTHING errors. Re-run the build to re-split, then "
            f"retry."
        )
    return (
        f"split current ({len(stamp_inputs)} inputs match "
        f"{build_dir / STAMP_NAME}; source: {stamp.get('input_source')})"
    )


def ensure_split_current(project_dir: Path | str, *, wait_seconds: float = 180.0) -> str:
    """Assert, waiting out a split that is merely in flight.

    A concurrent build is the common case in these trees, and refusing on it
    would make the guard a nuisance that gets disabled.  A split that is
    *running* is waited out, bounded; a split that is *stale* is refused
    immediately, because waiting cannot fix it.
    """
    project_dir = Path(project_dir).resolve()
    deadline = time.monotonic() + wait_seconds
    while True:
        try:
            return check(project_dir)
        except StaleSplitError as exc:
            if f"`{STATE_RUNNING}`" not in str(exc) or time.monotonic() >= deadline:
                raise
            time.sleep(2.0)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--project-dir", default=str(REPO_ROOT))
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--begin", action="store_true",
                      help="Record `running` before the split")
    mode.add_argument("--complete", action="store_true",
                      help="Record `complete` after a successful split")
    mode.add_argument("--check", action="store_true",
                      help="Exit 1 if the target objects do not match the config")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--stamp-out", default=None,
                    help="With --check: write a digest of the verified state to "
                         "this path, but ONLY when it differs. The ninja edge is "
                         "`always`-dirty by design (both failure modes are "
                         "mtime-invisible), so without write-if-changed + restat "
                         "every build would re-run REPORT on a tree where "
                         "nothing moved.")
    args = ap.parse_args(argv)

    project_dir = Path(args.project_dir).resolve()

    if args.begin or args.complete:
        edge = discover_split_edge(project_dir)
        if edge is None:
            print("[split-guard] no split edge in build.ninja; nothing to bracket",
                  file=sys.stderr)
            return 0
        state = STATE_RUNNING if args.begin else STATE_COMPLETE
        p = write_stamp(project_dir, edge[0], state)
        if not args.quiet:
            print(f"[split-guard] {state}: {p}")
        return 0

    try:
        note = check(project_dir)
    except StaleSplitError as exc:
        print(f"[split-guard] {exc}", file=sys.stderr)
        return 1

    if args.stamp_out:
        edge = discover_split_edge(project_dir)
        build_dir = edge[0] if edge else Path("build")
        src = project_dir / build_dir / MANIFEST_NAME
        if not src.exists():
            src = project_dir / build_dir / STAMP_NAME
        digest = hashlib.sha256(src.read_bytes()).hexdigest() + "\n"
        out = Path(args.stamp_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        if not out.exists() or out.read_text() != digest:
            out.write_text(digest)

    if not args.quiet:
        print(f"[split-guard] {note}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
