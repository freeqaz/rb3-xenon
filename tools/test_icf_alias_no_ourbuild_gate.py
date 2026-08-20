"""Pin: the alias generator must not adjudicate folds out of OUR build.

`tools/icf_alias_build.py` emits a star as a clique -- each tier adjudicates one
(survivor, folded) pair and the group is read as an equivalence class. The
obvious fix is decomp-synth's resolved-operand read (`symbol_equivalences` gate
(g)), which partitions a group by whether OUR compiled members agree on their
resolved operands. It was built, measured and reverted (`760cb450`).

It must not come back, because it answers the wrong question. ICF happened in
RETAIL's link. On a tree that matches 36.7%, an unmatched callee makes two of
our bodies differ where retail's single body does not, so the test refuses true
folds systematically rather than randomly. Measured over the 517 memberships it
withdraws (`<decomp-bench>/archive/runs/2026-08-20-gen-partition/`, instruments
from `2026-08-20-reloc-reconcile/tools/`): band.exe confirms 164 TRUE and
refutes at most 57.

A correct partition predicate is anchored on the retail image -- `retail_fold`
(one body in the masked class, at the group's address) and `callsite_consensus`
(where the linker's own resolved `bl` lands). This pin does not require one; it
requires that the wrong one is not silently reintroduced, on a generator whose
output every regeneration overwrites.

No objects, no toolchain, no image: this must stay cheap enough to run anywhere.
"""
import ast
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GEN = ROOT / "tools" / "icf_alias_build.py"

#: The our-build fold test, by the module names that carry it.
OURBUILD_GATE_MODULES = {"resolved_operands", "alias_repair", "symbol_equivalences"}


def _imported_names(path):
    """Every module name the file imports, however it spells it."""
    tree = ast.parse(path.read_text())
    out = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            out |= {a.name.split(".")[0] for a in node.names}
        elif isinstance(node, ast.ImportFrom) and node.module:
            out.add(node.module.split(".")[0])
        elif (isinstance(node, ast.Call)
              and isinstance(node.func, ast.Name)
              and node.func.id == "__import__"
              and node.args and isinstance(node.args[0], ast.Constant)):
            out.add(str(node.args[0].value).split(".")[0])
    return out


def test_the_generator_does_not_import_the_our_build_fold_test():
    assert not (_imported_names(GEN) & OURBUILD_GATE_MODULES), (
        "icf_alias_build.py imports the our-build resolved-operand read. That "
        "test refuses true retail folds systematically (164 confirmed true vs "
        "57 refuted over 517 withdrawals); see the module docstring here and "
        "the note at the group-emission site.")


def _code_strings(path):
    """String literals the file EXECUTES — docstrings excluded, comments absent
    from the AST entirely. Prose about decomp-synth is documentation; a path
    built at runtime is a reach."""
    tree = ast.parse(path.read_text())
    docs = set()
    for node in ast.walk(tree):
        if isinstance(node, (ast.Module, ast.FunctionDef, ast.AsyncFunctionDef,
                             ast.ClassDef)):
            body = getattr(node, "body", None)
            if (body and isinstance(body[0], ast.Expr)
                    and isinstance(body[0].value, ast.Constant)
                    and isinstance(body[0].value.value, str)):
                docs.add(id(body[0].value))
    return [n.value for n in ast.walk(tree)
            if isinstance(n, ast.Constant) and isinstance(n.value, str)
            and id(n) not in docs]


def test_the_generator_does_not_reach_for_decomp_synth_by_path():
    """The import guard is spelled on module names, so a `sys.path` insert into
    decomp-synth's `il_witness` would slip past it. `--help` text counts as
    executed string data and is deliberately in scope: a flag that offers the
    our-build partition is the thing being prevented."""
    bad = [s for s in _code_strings(GEN)
           if "il_witness" in s or "DECOMP_SYNTH" in s]
    assert not bad, (
        f"icf_alias_build.py reaches into decomp-synth at runtime ({bad[:2]!r}); "
        f"the only reason to is the our-build fold test")


def test_the_diagnosis_is_recorded_where_the_closure_is_formed():
    """The defect is real and the fix is not; a reader arriving at the union
    site must find both. This pins the note, not its wording -- delete the note
    and the next agent re-derives the reverted change from scratch."""
    src = GEN.read_text()
    i = src.index('g["folded"].append(b)')
    window = src[max(0, i - 1400):i]
    assert "RETAIL" in window and "760cb450" in window, (
        "the group-emission site has lost the note recording the star-vs-clique "
        "defect and why the our-build partition was reverted")
