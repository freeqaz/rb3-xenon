#!/usr/bin/env python3
"""The MCP build-and-score tools must never silently build in the shared main tree.

WHAT THIS PINS (lane W3-F, 2026-09-11)
--------------------------------------
`mcp_server.py` had THREE copies of "which tree do I measure?", and they had
drifted: `run_objdiff` and `run_analyze_function` fell back to the server's own
repo root -- which is the shared main checkout, because `.mcp.json` lives there
-- while `run_diff_inspect` refused. So an agent that omitted `project_dir`
(or whose `REPO_ROOT` was unset, which is the normal state of a subagent) built
in main and got back a number describing MAIN's source, in a shape
indistinguishable from "my change did nothing".

⚠ HONEST SCOPE. The two drift incidents originally cited as evidence for this
firing were RETRACTED by the coordinator on 2026-09-11: a full `ninja-locked`
rewrites objects progressively and only rewrites `patch_state.json` at its
terminal edge, so ANY `--verify-manifest` sample taken mid-build reads
"objects changed, manifest stale". Both incidents are consistent with a build in
flight. These tests therefore pin a REACHABLE defect, not a diagnosed incident.

WHY THESE TESTS EXIST AT ALL
----------------------------
The old logic was only reachable through a live async handler with an MCP
client attached, so nothing ever asked it anything -- which is why the drift
between the three copies survived. `resolve_project_dir` is pure and takes
`main_repo`/`env` as ARGUMENTS precisely so that this file can exist.

THE CONTROL: see `scripts/sabotage_project_dir_guard.py`, which restores the old
three-tier fallback in a sandbox copy and requires the named test below to go
red. A guard whose test cannot fail is not a guard.
"""

import os
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from orchestrator.mcp_server import (  # noqa: E402
    MAIN_REPO_OPT_IN,
    CompileRedirectRefusal,
    ProjectDirRefusal,
    redirect_compile_object,
    resolve_project_dir,
)


@pytest.fixture
def trees(tmp_path):
    """A fake main checkout and a fake worktree, both real directories."""
    main = tmp_path / "rb3-xenon"
    wt = tmp_path / "wt-lane"
    main.mkdir()
    wt.mkdir()
    return main, wt


# ── the refusals ─────────────────────────────────────────────────────────────

def test_absent_project_dir_is_refused(trees):
    """THE headline case: no argument, no REPO_ROOT -> refuse, do not pick main."""
    main, _ = trees
    with pytest.raises(ProjectDirRefusal) as e:
        resolve_project_dir(None, main, env={})
    assert "REQUIRED" in str(e.value)
    assert MAIN_REPO_OPT_IN in str(e.value)


def test_explicit_main_is_refused(trees):
    """Typing the hazard out does not neutralise it.

    This is also the only route by which a tool that already REQUIRED
    project_dir (run_diff_inspect, and asm_listing under it) could reach main.
    """
    main, _ = trees
    with pytest.raises(ProjectDirRefusal) as e:
        resolve_project_dir(str(main), main, env={})
    assert "shared main checkout" in str(e.value)


def test_repo_root_pointing_at_main_is_refused(trees):
    """REPO_ROOT is trusted for a worktree, not as a laundering route to main."""
    main, _ = trees
    with pytest.raises(ProjectDirRefusal):
        resolve_project_dir(None, main, env={"REPO_ROOT": str(main)})


def test_main_by_a_different_spelling_is_still_main(trees, tmp_path):
    """Resolution is by resolved path, so `main/./` and a symlink both refuse.

    A string compare would have let the fallback back in through the front door.
    """
    main, _ = trees
    link = tmp_path / "main-link"
    link.symlink_to(main)
    for spelling in (f"{main}/.", str(link)):
        with pytest.raises(ProjectDirRefusal):
            resolve_project_dir(spelling, main, env={})


def test_nonexistent_project_dir_is_refused(trees):
    main, _ = trees
    with pytest.raises(ProjectDirRefusal) as e:
        resolve_project_dir(str(main.parent / "nope"), main, env={})
    assert "does not exist" in str(e.value)


# ── the permissions ──────────────────────────────────────────────────────────

def test_a_worktree_is_accepted(trees):
    main, wt = trees
    assert resolve_project_dir(str(wt), main, env={}) == wt


def test_repo_root_worktree_is_accepted_when_arg_omitted(trees):
    """agent_runner.py sets REPO_ROOT to the agent's worktree; keep honouring it."""
    main, wt = trees
    assert resolve_project_dir(None, main, env={"REPO_ROOT": str(wt)}) == wt


def test_explicit_arg_outranks_repo_root(trees, tmp_path):
    main, wt = trees
    other = tmp_path / "wt-other"
    other.mkdir()
    assert resolve_project_dir(str(other), main, env={"REPO_ROOT": str(wt)}) == other


@pytest.mark.parametrize("val", ["1", "true", "TRUE", "yes", "on"])
def test_opt_in_restores_main(trees, val):
    """The escape hatch works -- measuring main is a legitimate coordinator act."""
    main, _ = trees
    assert resolve_project_dir(None, main, env={MAIN_REPO_OPT_IN: val}) == main.resolve()
    assert resolve_project_dir(str(main), main, env={MAIN_REPO_OPT_IN: val}) == main


@pytest.mark.parametrize("val", ["", "0", "false", "no", "off"])
def test_falsey_opt_in_does_not_open_the_door(trees, val):
    """`RB3_MCP_ALLOW_MAIN=0` must not read as "set, therefore true"."""
    main, _ = trees
    with pytest.raises(ProjectDirRefusal):
        resolve_project_dir(None, main, env={MAIN_REPO_OPT_IN: val})


def test_real_os_environ_is_the_default_env(trees, monkeypatch):
    """`env=None` must read the process environment, not an empty dict."""
    main, wt = trees
    monkeypatch.setenv("REPO_ROOT", str(wt))
    monkeypatch.delenv(MAIN_REPO_OPT_IN, raising=False)
    assert resolve_project_dir(None, main) == wt
    monkeypatch.setenv(MAIN_REPO_OPT_IN, "1")
    assert resolve_project_dir(str(main), main) == main


# ── the /FAs compile must not land on the tree's own object ──────────────────

NINJA_CMD = (
    "WIBO_FS_CACHE=1 /home/free/code/milohax/objcache/target/release/objcache "
    "exec --fo build/45410914/src/system/obj/Object.obj -- "
    "/home/free/code/milohax/wibo/build/release/wibo cl.exe /c /O1 "
    "/Fobuild/45410914/src/system/obj/Object.obj src/system/obj/Object.cpp"
)
OBJ = "build/45410914/src/system/obj/Object.obj"


def test_redirect_moves_every_output_path():
    """Both `--fo` (objcache) and `/Fo` (cl) must move, or the tree still gets hit."""
    out, n = redirect_compile_object(NINJA_CMD, OBJ, "/tmp/x/listing.obj")
    assert n == 2, "the real command names the object TWICE -- --fo and /Fo"
    assert OBJ not in out
    assert out.count("/tmp/x/listing.obj") == 2
    # the source file must survive untouched: it is not an output
    assert "src/system/obj/Object.cpp" in out


def test_redirect_refuses_when_it_cannot_find_the_object():
    """"Could not redirect" must never degrade into "wrote to the tree"."""
    with pytest.raises(CompileRedirectRefusal):
        redirect_compile_object("cl.exe /c src/foo.cpp", OBJ, "/tmp/x/listing.obj")


def test_source_paths_are_not_mistaken_for_the_object():
    """A .cpp whose name contains the obj stem must not be rewritten.

    Guards the same shape as the wibo byte-gate trap in CLAUDE.md, where a
    `/Fo(\\S+)` substitution also matched inside source paths.
    """
    cmd = f"cl.exe /c /Fo{OBJ} src/system/obj/Object.cpp"
    out, _ = redirect_compile_object(cmd, OBJ, "/tmp/x/listing.obj")
    assert out.endswith("src/system/obj/Object.cpp")


if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-q"]))
