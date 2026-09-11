#!/usr/bin/env python3
"""Every advertised MCP tool must actually resolve (lane W4-F, 2026-09-11).

`run_analyze_function` was advertised in `list_tools` for months while its
handler shelled out to `<project_dir>/bin/analyze-function`, a file that has
never existed in this repo (no commit ever added or deleted one; the
advertisement was ported from DC3 without the tool).  Every call returned
`Error: analyze-function not found` AFTER the model had paid to pick it off the
tool list.  A dead advertisement is worse than an absent one, because the model
cannot tell the difference until it has spent the call.

Both directions matter and neither was checked:
  * advertised with no handler  -> the tool errors or falls through.
  * handler with no advertisement -> dead code nothing can reach.

These read the SOURCE with `ast` rather than importing and starting a server:
the module pulls in the `mcp` package and a DB, and a test that needs a live
server is a test that gets skipped.  Skipped guards are how this survived.
"""
import ast
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
SERVER = REPO / "scripts" / "orchestrator" / "mcp_server.py"
TREE = ast.parse(SERVER.read_text())


def _advertised():
    """`name=` of every `Tool(...)` constructed in the module."""
    out = []
    for node in ast.walk(TREE):
        if isinstance(node, ast.Call) and getattr(node.func, "id", None) == "Tool":
            for kw in node.keywords:
                if kw.arg == "name" and isinstance(kw.value, ast.Constant):
                    out.append(kw.value.value)
    return out


def _dispatched():
    """Every string compared against the dispatch variable `name`."""
    out = set()
    for node in ast.walk(TREE):
        if not isinstance(node, ast.Compare) or not isinstance(node.left, ast.Name):
            continue
        if node.left.id != "name" or not isinstance(node.ops[0], ast.Eq):
            continue
        rhs = node.comparators[0]
        if isinstance(rhs, ast.Constant) and isinstance(rhs.value, str):
            out.add(rhs.value)
    return out


def test_the_extractors_are_not_vacuous():
    """If either extractor returns nothing, both tests below pass by being
    blind -- which is exactly how a list/handler mismatch stayed invisible."""
    adv, disp = _advertised(), _dispatched()
    assert len(adv) >= 8, f"only {len(adv)} Tool(...) advertisements parsed"
    assert len(disp) >= 8, f"only {len(disp)} dispatch branches parsed"
    assert "run_objdiff" in adv and "run_objdiff" in disp, "known-live tool not seen"


def test_no_advertised_tool_lacks_a_handler():
    missing = sorted(set(_advertised()) - _dispatched())
    assert not missing, (
        f"advertised with no dispatch branch: {missing}. A model pays for the "
        f"call before it learns the tool does not resolve."
    )


def test_no_handler_is_unreachable():
    orphan = sorted(_dispatched() - set(_advertised()))
    assert not orphan, f"dispatch branch with no advertisement (dead code): {orphan}"


def test_advertisements_are_unique():
    adv = _advertised()
    dupes = sorted({n for n in adv if adv.count(n) > 1})
    assert not dupes, f"advertised twice: {dupes}"


def test_no_handler_shells_out_to_a_missing_repo_executable():
    """The actual `run_analyze_function` defect: a `project_dir / "bin" / "x"`
    where `bin/x` does not exist in this repo.  `project_dir` is a worktree at
    runtime, but a worktree is a copy of THIS tree, so a name absent here is
    absent there too."""
    missing = []
    for node in ast.walk(TREE):
        # match the BinOp chain `<anything> / "bin" / "<name>"`
        if not isinstance(node, ast.BinOp) or not isinstance(node.op, ast.Div):
            continue
        if not (isinstance(node.right, ast.Constant) and isinstance(node.right.value, str)):
            continue
        inner = node.left
        if not (isinstance(inner, ast.BinOp) and isinstance(inner.op, ast.Div)
                and isinstance(inner.right, ast.Constant) and inner.right.value == "bin"):
            continue
        if not (REPO / "bin" / node.right.value).exists():
            missing.append(node.right.value)
    assert not missing, (
        f"handler(s) reference bin/ executable(s) that do not exist: "
        f"{sorted(set(missing))}"
    )


def test_the_missing_executable_detector_can_fail():
    """Must-be-able-to-fail: the AST shape above has to match the real idiom.
    Parse the exact line the deleted handler used and require a hit."""
    t = ast.parse('analyze_script = project_dir / "bin" / "analyze-function"')
    found = []
    for node in ast.walk(t):
        if not isinstance(node, ast.BinOp) or not isinstance(node.op, ast.Div):
            continue
        if not (isinstance(node.right, ast.Constant) and isinstance(node.right.value, str)):
            continue
        inner = node.left
        if (isinstance(inner, ast.BinOp) and isinstance(inner.op, ast.Div)
                and isinstance(inner.right, ast.Constant) and inner.right.value == "bin"):
            found.append(node.right.value)
    assert found == ["analyze-function"], f"detector missed the idiom: {found}"
    assert not (REPO / "bin" / "analyze-function").exists(), (
        "bin/analyze-function exists again -- if it was legitimately added, "
        "re-advertise run_analyze_function and delete this assertion"
    )
