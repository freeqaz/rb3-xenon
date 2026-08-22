#!/usr/bin/env python3
"""Does `patch_guard.ensure_patched_tree` actually refuse a stale SPLIT?

`ensure_patched_tree` establishes that the BASE objects (build/<v>/src/**) are
a fixed point of the post-compile patchers.  It said nothing about the TARGET
objects (build/<v>/obj/**) -- written by `dtk xex split`, an edge that declares
only config.json and is therefore invisible to ninja, to the patch manifest and
to every mtime.  Since 2026-08-22 it also asserts split currency, and every
caller that goes through it -- run_objdiff, sync_objdiff, diff_inspect,
batch_check -- inherits that.

WHAT THIS TEST IS WRITTEN AGAINST.  "ensure_patched_tree returned a note" is
equally true with and without the hook, so it is not the assertion.  Neither is
"it printed a warning" -- that is precisely how a sibling lane's sabotage
passed this week, going red on a message rather than on the absence of a
refusal.  The assertion here is that it RAISES `UnpatchedTreeError`, and that
the message NAMES the file that was broken.  Each RED is bracketed by a GREEN
on the same tree, so a guard stuck in the refusing position would fail too.

Run against the real tree; the split record is backed up and restored in a
`finally`, including on Ctrl-C.

    python3 scripts/test_patch_guard_split_hook.py
"""

from __future__ import annotations

import json
import os
import shutil
import sys
import tempfile
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts" / "orchestrator"))

import patch_guard  # noqa: E402

# Plant a `running` record and the guard is SUPPOSED to wait it out, so tell it
# to wait seconds instead of three minutes. Production never sets this.
os.environ[patch_guard.SPLIT_WAIT_ENV] = "3"

_fails: list[str] = []


def check(ok: bool, what: str) -> None:
    print(f"  {'GREEN' if ok else 'RED  '}  {what}")
    if not ok:
        _fails.append(what)


def main() -> int:
    edge_stamp = None
    for candidate in (REPO / "build").glob("*/split_inputs.stamp"):
        edge_stamp = candidate
        break
    if edge_stamp is None:
        print("SKIP: no build/<v>/split_inputs.stamp -- run the build first")
        return 0

    backup = Path(tempfile.mktemp(prefix="split-stamp-"))
    shutil.copy2(edge_stamp, backup)
    try:
        note = patch_guard.ensure_patched_tree(REPO, build=False)
        check("split current" in note,
              "BASELINE: a clean tree returns a note that reports split currency")

        print("\n  -- SABOTAGE: the record says a split is in flight --")
        doc = json.loads(backup.read_text())
        doc["state"] = "running"
        edge_stamp.write_text(json.dumps(doc))
        t0 = time.monotonic()
        try:
            patch_guard.ensure_patched_tree(REPO, build=False)
            check(False, "an in-flight split must RAISE, not return a note")
        except patch_guard.UnpatchedTreeError as exc:
            elapsed = time.monotonic() - t0
            check("running" in str(exc),
                  "an in-flight split RAISES UnpatchedTreeError saying `running`")
            check(elapsed >= 3.0,
                  f"...and it WAITED it out first ({elapsed:.1f}s >= 3s) rather "
                  f"than refusing a normal concurrent build outright")
        except Exception as exc:  # noqa: BLE001
            check(False, f"raised the WRONG exception type: {type(exc).__name__}")

        print("\n  -- SABOTAGE: the record disagrees with symbols.txt on disk --")
        doc = json.loads(backup.read_text())
        key = next((k for k in doc["inputs"] if k.endswith("symbols.txt")), None)
        if key is None:
            check(False, "the split record names no symbols.txt -- coverage gap")
        else:
            doc["inputs"][key] = "0" * 64
            edge_stamp.write_text(json.dumps(doc))
            try:
                patch_guard.ensure_patched_tree(REPO, build=False)
                check(False, "config drift must RAISE, not return a note")
            except patch_guard.UnpatchedTreeError as exc:
                check("symbols.txt" in str(exc),
                      "config drift RAISES and NAMES symbols.txt (red for the "
                      "right reason, not merely red)")
                check("running" not in str(exc),
                      "...and it is NOT the in-flight message -- the two "
                      "failures are distinguishable")

        print("\n  -- RESTORE --")
        shutil.copy2(backup, edge_stamp)
        note = patch_guard.ensure_patched_tree(REPO, build=False)
        check("split current" in note,
              "restoring the record returns a clean note (a guard stuck "
              "refusing would fail here)")
    finally:
        shutil.copy2(backup, edge_stamp)
        backup.unlink(missing_ok=True)

    print(f"\n{'FAILED' if _fails else 'ALL GREEN'}")
    for w in _fails:
        print(f"  - {w}")
    return 1 if _fails else 0


if __name__ == "__main__":
    sys.exit(main())
