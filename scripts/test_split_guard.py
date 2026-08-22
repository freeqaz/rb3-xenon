#!/usr/bin/env python3
"""Sabotage tests for scripts/verify_split_current.py.

A guard you cannot make go RED is not a guard, and this repo has been burned by
exactly that: a check that "passed" on deliberately broken input because the
assertion it made was true either way.  So every GREEN below is paired with a
RED produced by breaking the specific thing that GREEN claims to cover, and
every RED is checked for the RIGHT REASON -- the message must name the file
that was broken, not merely be non-empty.

Two traps this is written against, both hit by sibling lanes this week:

  * asserting on a *warning* rather than on the refusal.  Here every assertion
    reads the process EXIT CODE, and reads it without a pipe -- ``cmd | head``
    reports head's status, which made an earlier manual walkthrough print
    ``rc=0`` under a refusal.
  * a test that is vacuous because "not recorded" produces the same evidence
    the test asserts on.  Hence :func:`test_drift_is_not_merely_absence`, which
    proves the guard distinguishes *changed* from *missing*.

Everything runs against a scratch COPY of the repo's config and build metadata,
never against the tree itself.  Run from a worktree.

    python3 scripts/test_split_guard.py
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
GUARD = REPO / "scripts" / "verify_split_current.py"

_results: list[tuple[bool, str]] = []


def record(ok: bool, what: str, detail: str = "") -> None:
    _results.append((ok, what))
    print(f"  {'GREEN' if ok else 'RED  '}  {what}")
    if not ok and detail:
        for line in detail.splitlines()[:8]:
            print(f"           {line}")


def run_guard(project_dir: Path, *args: str) -> tuple[int, str]:
    """Run the guard and return (exit code, combined output).

    NO PIPE.  ``guard | head`` reports head's exit status, which silently turns
    every refusal into a pass -- observed while developing this.
    """
    proc = subprocess.run(
        [sys.executable, str(GUARD), "--project-dir", str(project_dir), *args],
        capture_output=True, text=True,
    )
    return proc.returncode, proc.stdout + proc.stderr


def make_scratch() -> Path:
    """A minimal stand-in tree: build.ninja's split edge, the dep file, config.

    Only the split edge is needed from build.ninja, so it is written rather than
    copied -- a 22,000-line copy would make the fixture's failures harder to
    read than the thing under test.
    """
    edge = _discover_real_edge()
    if edge is None:
        sys.exit("FATAL: could not find a split edge in the repo's build.ninja; "
                 "configure the tree first")
    build_dir, config_rel = edge

    scratch = Path(tempfile.mkdtemp(prefix="split-guard-test-"))
    (scratch / build_dir).mkdir(parents=True, exist_ok=True)
    (scratch / config_rel).parent.mkdir(parents=True, exist_ok=True)

    # Written in the SAME shape configure.py emits, implicit-output clause and
    # all. An earlier fixture omitted `| .../split_inputs.stamp` and therefore
    # kept passing after the guard's own wiring made the real edge unparseable.
    (scratch / "build.ninja").write_text(
        "rule split\n"
        "  command = dtk split $in $out_dir\n"
        f"build {build_dir}/config.json | {build_dir}/split_inputs.stamp: "
        f"split {config_rel} | dtk\n"
        f"  out_dir = {build_dir}\n"
    )

    # Two real-ish inputs with distinguishable content, and a dep file naming
    # them -- the same shape dtk writes.
    syms = config_rel.parent / "symbols.txt"
    splits = config_rel.parent / "splits.txt"
    (scratch / syms).write_text("Foo = .text:0x82000000; // type:function\n")
    (scratch / splits).write_text("Foo.cpp:\n\t.text start:0x82000000 end:0x82000010\n")
    (scratch / config_rel).write_text(
        f"object: orig/fake.bin\nsymbols: {syms}\nsplits: {splits}\n"
    )
    (scratch / build_dir / "dep").write_text(
        f"{build_dir}/config.json: \\\n  {syms} \\\n  {splits}\n"
    )
    (scratch / build_dir / "config.json").write_text('{"units": []}\n')
    return scratch


def _discover_real_edge() -> tuple[Path, Path] | None:
    """Discovery against THIS repo's real build.ninja.

    Deliberately not stubbed. The fixture below is synthetic, so if discovery
    only worked on the fixture every assertion in this file would be about a
    tree that does not exist.
    """
    sys.path.insert(0, str(REPO / "scripts"))
    import verify_split_current as guard  # noqa: PLC0415
    return guard.discover_split_edge(REPO)


def sha1_of(p: Path) -> str:
    return hashlib.sha1(p.read_bytes()).hexdigest()


def main() -> int:
    scratch = make_scratch()
    build_dir, config_rel = _discover_real_edge()
    syms = scratch / config_rel.parent / "symbols.txt"
    stamp = scratch / build_dir / "split_inputs.stamp"
    manifest = scratch / build_dir / "split_manifest.json"

    print(f"scratch tree: {scratch}")
    print(f"discovered split edge: build_dir={build_dir} config={config_rel}")

    print("\n== 1. a tree with no record cannot be vouched for ==")
    rc, out = run_guard(scratch, "--check")
    record(rc == 1, "no stamp and no manifest -> exit 1", out)
    record("NO record" in out or "neither" in out,
           "...and the message says WHY (no record), not something else", out)

    print("\n== 2. bracketing, and the GREEN that makes the REDs meaningful ==")
    rc, out = run_guard(scratch, "--complete", "--quiet")
    record(rc == 0, "--complete writes a stamp", out)
    rc, out = run_guard(scratch, "--check")
    record(rc == 0, "an untouched tree passes -> exit 0", out)
    n_inputs = json.loads(stamp.read_text())["inputs"]
    record(len(n_inputs) >= 3,
           f"DENOMINATOR: the stamp covers {len(n_inputs)} inputs "
           f"({', '.join(sorted(n_inputs))})", "")

    print("\n== 3. SABOTAGE: edit symbols.txt after the split ==")
    original = syms.read_bytes()
    syms.write_bytes(original + b"Bar = .text:0x82000010; // type:function\n")
    rc, out = run_guard(scratch, "--check")
    record(rc == 1, "a symbols.txt edit -> exit 1", out)
    record("symbols.txt" in out,
           "...and the refusal NAMES symbols.txt (red for the right reason)", out)
    syms.write_bytes(original)
    rc, out = run_guard(scratch, "--check")
    record(rc == 0, "restoring symbols.txt -> exit 0 again", out)

    print("\n== 4. SABOTAGE: the mtime-invisible form ==")
    # This is the deterministic variant that ninja cannot see at all: same
    # content-changed file, but with an OLDER mtime than config.json. A guard
    # built on mtimes passes this; a content guard must not.
    syms.write_bytes(original + b"Baz = .text:0x82000020; // type:function\n")
    old = (scratch / build_dir / "config.json").stat().st_mtime - 86400
    os.utime(syms, (old, old))
    rc, out = run_guard(scratch, "--check")
    record(rc == 1, "changed content with an OLDER mtime still -> exit 1", out)
    record(syms.stat().st_mtime < (scratch / build_dir / "config.json").stat().st_mtime,
           "...and the control holds: the sabotaged file really is older", "")
    syms.write_bytes(original)
    run_guard(scratch, "--complete", "--quiet")

    print("\n== 5. SABOTAGE: a split in flight ==")
    rc, out = run_guard(scratch, "--begin", "--quiet")
    record(rc == 0, "--begin writes `running`", out)
    rc, out = run_guard(scratch, "--check")
    record(rc == 1, "a split recorded as `running` -> exit 1", out)
    record("running" in out,
           "...and the refusal says `running` -- NOT a config-drift message, "
           "which is the failure the input hashes alone cannot see", out)
    run_guard(scratch, "--complete", "--quiet")
    rc, _ = run_guard(scratch, "--check")
    record(rc == 0, "--complete clears it -> exit 0", "")

    print("\n== 6. dtk's own manifest is honoured, and can go red ==")
    # Shape as emitted by `dtk <fmt> split` since 2026-08-22 (util/split_manifest.rs).
    doc = {
        "manifest_version": 1,
        "tool_version": "1.13.0",
        "tool_commit": "0" * 40,
        "command": "xex split",
        "config": str(config_rel),
        "out_dir": str(build_dir),
        "inputs": {
            str(config_rel.parent / "symbols.txt"): {
                "size": syms.stat().st_size, "sha1": sha1_of(syms)},
        },
        "outputs": {f"{build_dir}/obj/Foo.obj": {"size": 1, "sha1": "0" * 40}},
    }
    manifest.write_text(json.dumps(doc, indent=1))
    rc, out = run_guard(scratch, "--check")
    record(rc == 0, "a truthful manifest -> exit 0", out)
    record("manifest" in out,
           "...and the guard SAYS it used the manifest, not the stamp", out)

    doc["inputs"][str(config_rel.parent / "symbols.txt")]["sha1"] = "f" * 40
    manifest.write_text(json.dumps(doc, indent=1))
    rc, out = run_guard(scratch, "--check")
    record(rc == 1, "a manifest that disagrees with disk -> exit 1", out)
    record("symbols.txt" in out, "...naming the input that moved", out)

    print("\n== 7. drift is not merely absence (anti-vacuity) ==")
    # The trap: if "input missing" and "input changed" produced the same
    # evidence, test 3 would pass for a guard that only ever noticed absence.
    doc["inputs"][str(config_rel.parent / "symbols.txt")]["sha1"] = sha1_of(syms)
    doc["inputs"]["config/does-not-exist.txt"] = {"size": 0, "sha1": "a" * 40}
    manifest.write_text(json.dumps(doc, indent=1))
    rc, out = run_guard(scratch, "--check")
    record(rc == 1, "a missing declared input -> exit 1", out)
    record("MISSING" in out and "does-not-exist" in out,
           "...and it is reported as MISSING, distinctly from a hash change", out)

    print("\n== 8. --stamp-out does not churn (the restat contract) ==")
    del doc["inputs"]["config/does-not-exist.txt"]
    manifest.write_text(json.dumps(doc, indent=1))
    out_stamp = scratch / build_dir / "checked.stamp"
    run_guard(scratch, "--check", "--quiet", "--stamp-out", str(out_stamp))
    first = out_stamp.stat().st_mtime_ns
    run_guard(scratch, "--check", "--quiet", "--stamp-out", str(out_stamp))
    record(out_stamp.stat().st_mtime_ns == first,
           "a second passing check leaves the stamp's mtime alone", "")
    # NEGATIVE CONTROL: it must still move when the verified state really moves.
    doc["tool_commit"] = "1" * 40
    manifest.write_text(json.dumps(doc, indent=1))
    run_guard(scratch, "--check", "--quiet", "--stamp-out", str(out_stamp))
    record(out_stamp.stat().st_mtime_ns != first,
           "...but DOES move when the split record changes "
           "(a stamp that never moves is not a stamp)", "")

    shutil.rmtree(scratch, ignore_errors=True)

    failed = [w for ok, w in _results if not ok]
    print(f"\n{len(_results) - len(failed)}/{len(_results)} green")
    if failed:
        print("FAILED:")
        for w in failed:
            print(f"  - {w}")
        return 1
    print("ALL GREEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
