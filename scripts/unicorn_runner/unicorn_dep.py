"""Locate the Unicorn PPC bindings, and report honestly when they are absent.

This module exists because two defects were stacked on top of each other, and
the outer one disguised the inner one.

Defect 1 — the checkout is located by counting parent directories
-----------------------------------------------------------------
``engine.py`` used to do::

    _MILOHAX_DIR = Path(__file__).resolve().parent.parent.parent.parent
    _UNICORN_DIR = _MILOHAX_DIR / "unicorn"
    sys.path.insert(0, str(_UNICORN_DIR / "bindings" / "python"))
    os.environ["LIBUNICORN_PATH"] = str(_UNICORN_DIR / "build")

That is correct for exactly one layout: the repo sitting directly under
``~/code/milohax``. Inside a git worktree (``<repo>/.claude/worktrees/<name>``)
``__file__`` resolves next to the worktree, so ``_MILOHAX_DIR`` becomes
``<repo>/.claude/worktrees`` and the bindings path is a directory that does not
exist. The ``sys.path.insert`` is then a silent no-op — and worse, the
unconditional assignment to ``LIBUNICORN_PATH`` *overwrites a correct value the
caller exported* with a nonexistent one, so even ``PYTHONPATH=<real bindings>``
could not rescue it.

Defect 2 — the repo's own ``scripts/unicorn/`` shadows the bindings
-------------------------------------------------------------------
pytest's ``prepend`` import mode walks up from a test file while ``__init__.py``
exists and puts the first non-package ancestor on ``sys.path``. For
``scripts/unicorn_runner/tests/test_*.py`` that ancestor is ``scripts/`` — which
contains ``unicorn/``, this repo's own tooling package. Once defect 1 made the
real bindings path a no-op, ``import unicorn`` resolved to ``scripts/unicorn``
and every consumer got::

    ImportError: cannot import name 'Uc' from 'unicorn' (.../scripts/unicorn/__init__.py)

which reads like a broken third-party install rather than a name collision.
(In dc3 ``scripts/unicorn`` has an ``__init__.py``, so it is a regular package
and wins outright. In rb3-xenon it has none, so it is an implicit *namespace*
package, which loses to any regular ``unicorn`` found later on ``sys.path`` —
same symptom, different mechanism, and it is why the reported error text
differs between the two repos.)

The fix for defect 2 is ordering, not renaming: ``scripts.unicorn`` has real
callers (``scripts.unicorn.refresh_frontier``, ``.source_hash``,
``.corpus_check``, ``.batch_to_db``, ...), so it stays where it is and the
bindings directory is inserted *ahead* of it. Renaming was considered and
rejected: it would churn every ``scripts.unicorn.*`` import site to remove a
collision that correct ``sys.path`` ordering already removes.

Contract
--------
``ensure_unicorn_on_path()`` is idempotent, never raises, and never clobbers an
environment variable the caller set deliberately. ``probe()`` answers "is the
emulator usable" without raising, so tests can skip instead of erroring at
collection. ``require()`` raises an ``ImportError`` whose message names the
directories that were searched — an honest failure rather than a confusing one.

``UNICORN_DIR=/nonexistent`` forces the unavailable branch, which is how the
skip path is tested (see ``scripts/unicorn_runner/tests/README`` note in the
test files' module docstrings).
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

__all__ = [
    "find_unicorn_dir",
    "ensure_unicorn_on_path",
    "probe",
    "require",
    "HAS_UNICORN",
    "SKIP_REASON",
]

# <repo>/scripts/unicorn_runner/unicorn_dep.py -> <repo>
_REPO_ROOT = Path(__file__).resolve().parent.parent.parent


def _worktree_real_repo() -> Path | None:
    """The canonical checkout backing this tree, or None.

    In a linked worktree ``git rev-parse --git-common-dir`` points at the main
    checkout's ``.git``, so its parent is the real repo directory even when
    ``__file__`` sits under ``<repo>/.claude/worktrees/<name>``.
    """
    try:
        proc = subprocess.run(
            ["git", "rev-parse", "--git-common-dir"],
            cwd=str(_REPO_ROOT), capture_output=True, text=True, timeout=5,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    if proc.returncode != 0 or not proc.stdout.strip():
        return None
    gcd = Path(proc.stdout.strip())
    if not gcd.is_absolute():
        gcd = (_REPO_ROOT / gcd).resolve()
    return gcd.parent if gcd.name == ".git" else gcd


def find_unicorn_dir() -> Path | None:
    """The Unicorn checkout to use, or None when there is none.

    Order: explicit ``UNICORN_DIR`` override, the repo-adjacent path, the
    *real* repo's sibling (correct inside a worktree), then the conventional
    ``~/code/milohax`` layout. First existing candidate wins.

    An explicit ``UNICORN_DIR`` that does not exist returns None rather than
    falling through — an override the operator got wrong must not be silently
    replaced by a different checkout.
    """
    env = os.environ.get("UNICORN_DIR")
    if env:
        p = Path(env)
        return p if p.is_dir() else None

    candidates = [_REPO_ROOT.parent / "unicorn"]
    real = _worktree_real_repo()
    if real is not None:
        candidates.append(real.parent / "unicorn")
    candidates.append(Path.home() / "code" / "milohax" / "unicorn")

    for c in candidates:
        if (c / "bindings" / "python").is_dir():
            return c
    return None


def ensure_unicorn_on_path() -> Path | None:
    """Put the bindings ahead of ``scripts/unicorn`` on ``sys.path``.

    Returns the checkout used, or None. Idempotent. Uses ``setdefault`` for
    ``LIBUNICORN_PATH`` so an exported value wins over the derived one.
    """
    d = find_unicorn_dir()
    if d is None:
        return None
    bindings = str(d / "bindings" / "python")
    # Insert at 0: pytest puts ``scripts/`` at 0 for the unicorn_runner tests,
    # and ``scripts/unicorn`` must not win the name.
    if sys.path and sys.path[0] == bindings:
        pass
    else:
        while bindings in sys.path:
            sys.path.remove(bindings)
        sys.path.insert(0, bindings)
    build = d / "build"
    if build.is_dir():
        os.environ.setdefault("LIBUNICORN_PATH", str(build))
    return d


def _purge_shadow() -> None:
    """Drop a ``unicorn`` already imported from ``scripts/unicorn``.

    Without this, a caller that touched the shadow first would pin it in
    ``sys.modules`` and the ``sys.path`` reordering above would be inert.
    Only a module missing ``Uc`` is dropped, so a genuine bindings import is
    never disturbed.
    """
    mod = sys.modules.get("unicorn")
    if mod is None or hasattr(mod, "Uc"):
        return
    for name in [n for n in sys.modules
                 if n == "unicorn" or n.startswith("unicorn.")]:
        del sys.modules[name]


def probe() -> tuple[bool, str]:
    """``(available, reason)``. Never raises; ``reason`` is '' when available."""
    d = ensure_unicorn_on_path()
    _purge_shadow()
    try:
        from unicorn import Uc  # noqa: F401
    except Exception as exc:  # ImportError, OSError from the dynamic library
        searched = str(d) if d else (
            f"{_REPO_ROOT.parent / 'unicorn'} (and the git-common-dir sibling "
            f"and ~/code/milohax/unicorn)")
        return False, (
            f"Unicorn PPC bindings unavailable: {type(exc).__name__}: {exc}. "
            f"Searched: {searched}. Set UNICORN_DIR=<unicorn checkout> to "
            f"override. NOTE: this repo's own scripts/unicorn package takes "
            f"the `unicorn` name when the bindings are not found first."
        )
    return True, ""


def require() -> None:
    """Raise a self-explaining ImportError when the emulator is unusable."""
    ok, reason = probe()
    if not ok:
        raise ImportError(reason)


HAS_UNICORN, SKIP_REASON = probe()
