"""Pin: the DC-4 join guard measures CALL sites, not every charged site.

`tools/icf_alias_build.py` refuses to run when the sites census stops joining
against the COFF symbol tables -- the shape that once reported an empty tree as
done. The statistic has to be taken over the names that can join. A `refhi` /
`reflo` pair names data (`lbl_<addr>`, `??_C@...`, `__real@...`), never a
function COMDAT, so counting it in the denominator makes the guard track the
census's code/data mix instead.

It did. CY-1 (`f592571a`) fixed unit pairing, the site population grew ~4x on the
base side, and the all-sites reading fell 46.4% -> 19.5% on the same tree while
the call-site reading held at 87.0% -> 88.0%. The guard fired on the fix, and
the recipe in `scripts/symbol_aliases.json` stopped being runnable at all.

No objects, no toolchain, no image.
"""
import ast
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GEN = ROOT / "tools" / "icf_alias_build.py"


def _tree():
    return ast.parse(GEN.read_text())


def _names_in(node):
    return {n.id for n in ast.walk(node) if isinstance(n, ast.Name)}


def _assigns_to(tree, target):
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == target for t in node.targets):
            yield node


def test_the_refused_statistic_is_taken_over_call_sites():
    """`_sn` is the guard's denominator. Built from `pairs` it counts data
    symbols that cannot be in the tables under any healthy census."""
    srcs = [_names_in(a.value) for a in _assigns_to(_tree(), "_sn")]
    assert srcs, "the join guard's `_sn` assignment is gone"
    assert all("call_pairs" in s for s in srcs), (
        "the join guard's denominator is no longer restricted to call sites; "
        "it will fire on a census whose data-reference share moved")
    assert all("pairs" not in s for s in srcs), (
        "the join guard's denominator includes every charged pair again")


def test_call_pairs_is_populated_only_at_a_bl_site():
    """The restriction is only honest if the restricted set really is the calls:
    widen this collection and the guard silently goes back to the old mix."""
    tree = _tree()
    guarded = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.If):
            continue
        test = ast.unparse(node.test)
        if "kind" not in test or "'bl'" not in test.replace('"', "'"):
            continue
        guarded.append(any(
            isinstance(c, ast.Call) and isinstance(c.func, ast.Attribute)
            and c.func.attr == "add"
            and isinstance(c.func.value, ast.Name)
            and c.func.value.id == "call_pairs"
            for c in ast.walk(node)))
    assert guarded and any(guarded), (
        "nothing adds to `call_pairs` under a `kind == 'bl'` test")

    adds = [n for n in ast.walk(tree)
            if isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute)
            and n.func.attr == "add" and isinstance(n.func.value, ast.Name)
            and n.func.value.id == "call_pairs"]
    assert len(adds) == sum(g for g in guarded), (
        "`call_pairs` is written somewhere other than the `bl` branch")


def test_a_census_with_no_call_site_at_all_is_still_refused():
    """Restricting the denominator must not convert a census-format break into a
    quiet zero: no `bl` anywhere is the break, not an honest empty result."""
    # Via the AST: the message is written as adjacent literals, which the parser
    # joins and a raw-source grep does not.
    strings = " ".join(n.value.lower() for n in ast.walk(_tree())
                       if isinstance(n, ast.Constant) and isinstance(n.value, str))
    assert "not one is a call site" in strings, (
        "the all-data census case no longer refuses; a census that stops "
        "emitting `bl` rows would read as 'no folds exist'")
