"""Pin: a generated alias group is a PARTITION class, never a closure.

`tools/icf_alias_build.py` adjudicates ONE PAIR (survivor, folded) per evidence
tier and appends every accepted spelling to a single list keyed on the survivor
name. That makes the evidence a star and the emitted group a clique: two folded
spellings are never compared with each other, so a group can claim folds no tier
ever adjudicated. Two fabrication classes retired in August 2026 -- the 43
identical-folded clusters and the 158 gate-(g) refusals -- were both that defect.

The read that sees it is decomp-synth `tools/il_witness/resolved_operands.py`:
every ruler here masks relocated words, so two bodies differing only in a
relocation TARGET are identical to all of them. `partition_emitted` puts that
read at generation time.

WHY THIS TEST IS SHAPED THIS WAY
--------------------------------
The headline assertion is a ZERO, and a zero is worth nothing unless the
instrument that produced it can be shown to fire. So the first test is a
POSITIVE CONTROL taken from the same object tree and the same reader: if no
masked-equal / resolved-divergent pair can be found and charged, every other
assertion here is vacuous and the suite fails saying so. The denominators are
floors, never equalities -- the accepted set and the body index both grow, and
an `==` pin against a growing artefact goes red on success.
"""
import json
import os
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
ALIASES = ROOT / "scripts" / "symbol_aliases.json"
DS = Path(os.environ.get("DECOMP_SYNTH_ROOT") or (ROOT.parent / "decomp-synth"))
IL = DS / "tools" / "il_witness"

#: Floors, not equalities. Today: 861 accepted, 158,927 bodies indexed.
MIN_ACCEPTED = 700
MIN_BODIES = 100_000

if str(IL) not in sys.path:
    sys.path.insert(0, str(IL))
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))


def _need(cond, why):
    if not cond:
        pytest.skip(why, allow_module_level=True)


_need((IL / "resolved_operands.py").is_file(),
      f"no resolved-operand read at {IL} (set DECOMP_SYNTH_ROOT)")
_need((ROOT / "objdiff.json").is_file(),
      "objdiff.json absent -- configure the tree first")

import resolved_operands as ro          # noqa: E402
import symbol_equivalences as se        # noqa: E402
import gen_symbol_alias_map as gsam     # noqa: E402
import icf_alias_build as iab           # noqa: E402


@pytest.fixture(scope="module")
def groups():
    return json.loads(ALIASES.read_text())["groups"]


@pytest.fixture(scope="module")
def tree():
    """`(bodies, canon, accepted_survivors, frozen_names)` over the built tree.

    `accepted` is taken against the map THIS FILE renders, because gate (f)
    reads `build/45410914/icf_aliases.map` and ninja renders that map from this
    JSON -- pairing a file with somebody else's map is not a reading of it.
    """
    bodies, _amb = ro.index_bodies(ROOT)
    _need(len(bodies) >= MIN_BODIES,
          f"only {len(bodies)} compiled bodies indexed -- tree not built")
    block, _rounds = ro.icf_congruence(bodies)
    gs = json.loads(ALIASES.read_text())["groups"]
    mc = se.parse_msvc_map_classes(gsam.render_map(gs))
    rep = se.validate_groups(ROOT, gs, map_classes=mc)
    occ = {}
    for g in gs:
        for n in set([g.get("survivor")] + list(g.get("folded") or [])):
            if n:
                occ[n] = occ.get(n, 0) + 1
    return (bodies, (lambda s: block.get(s, s)),
            set(rep["canon"].values()), {n for n, c in occ.items() if c > 1})


def test_the_read_can_charge_a_closure_at_all(tree):
    """POSITIVE CONTROL. Everything below asserts a zero; this asserts the
    instrument fires. Two compiled bodies that agree under masking and disagree
    on their resolved operands are exactly the shape the generator's key cannot
    see, and the object tree is full of them. If none can be found and charged,
    the zeros below are a broken reader, not a clean file."""
    bodies, canon, _acc, _frozen = tree
    buckets = {}
    for n, f in bodies.items():
        buckets.setdefault(ro.masked_words(f), []).append(n)
    found = None
    for names in buckets.values():
        if len(names) < 2:
            continue
        a = names[0]
        for b in names[1:]:
            if ro.divergence(bodies[a], bodies[b], canon)["refuse"]:
                found = (a, b)
                break
        if found:
            break
    assert found is not None, (
        "no masked-equal / resolved-divergent pair anywhere in the built tree "
        "-- the resolved-operand read cannot charge, so every zero in this "
        "module is vacuous")
    a, b = found
    chk = ro.check_group(bodies, a, [b], canon)
    assert chk["refuse"], f"the read did not charge its own witness {found!r}"


def test_no_accepted_group_carries_a_charged_closure(groups, tree):
    """THE BAR. Not one group inside the admission predicate may claim a fold
    the resolved-operand read refutes. Gate (e) drops a name occurring in more
    than one group BEFORE gate (g) runs, so the class adjudicated here is the
    one the predicate actually sees."""
    bodies, canon, accepted, frozen = tree
    assert len(accepted) >= MIN_ACCEPTED, (
        f"only {len(accepted)} accepted classes -- below the {MIN_ACCEPTED} "
        f"floor, so a zero below would be a small denominator, not a clean file")
    charged = []
    for g in groups:
        surv = g.get("survivor")
        if surv not in accepted:
            continue
        folded = [f for f in (g.get("folded") or []) if f and f not in frozen]
        if ro.check_group(bodies, surv, folded, canon)["refuse"]:
            charged.append((g.get("address"), surv))
    assert not charged, (
        f"{len(charged)} accepted group(s) are under-partitioned closures. The "
        f"generator owes a partition and emitted a star; run "
        f"tools/icf_alias_build.py with --partition accepted. First: "
        f"{charged[:5]!r}")


def test_gate_g_costs_nothing_on_this_file(groups):
    """The admission-level statement of the same fact, through the function the
    admission path calls: arming gate (g) must not move the canon by one bit.
    Catches a charge the per-group loop above could miss for a gate-ordering
    reason, and it is the reading a ruling would quote."""
    mc = se.parse_msvc_map_classes(gsam.render_map(groups))

    def canon_of(armed):
        old = os.environ.get(se.ENV_RESOLVED_OPERAND_GATE)
        os.environ[se.ENV_RESOLVED_OPERAND_GATE] = "1" if armed else "0"
        try:
            return se.validate_groups(ROOT, groups, map_classes=mc)["canon"]
        finally:
            if old is None:
                os.environ.pop(se.ENV_RESOLVED_OPERAND_GATE, None)
            else:
                os.environ[se.ENV_RESOLVED_OPERAND_GATE] = old

    unarmed, armed = canon_of(False), canon_of(True)
    assert armed == unarmed, (
        f"arming gate (g) withdraws {len(unarmed) - len(armed)} canon name(s): "
        f"the file carries closures the resolved-operand read refutes")


def test_partition_is_a_no_op_on_the_landed_file(groups):
    """The generator's own step, run over the landed file, must split nothing.

    This is the regression pin proper: it fails the moment the file re-grows a
    closure OR the step stops splitting one, and it does so through the exact
    code path a regeneration takes -- not a re-implementation of it."""
    out, rep = iab.partition_emitted([dict(g) for g in groups], "accepted")
    assert rep["n_repaired"] == 0 and rep["n_memberships_withdrawn"] == 0, (
        f"the generation-time partition splits {rep['n_repaired']} landed "
        f"group(s) / {rep['n_memberships_withdrawn']} membership(s), so the "
        f"landed file is not what this generator would emit")
    before = [(g.get("survivor"), tuple(g.get("folded") or [])) for g in groups]
    after = [(g.get("survivor"), tuple(g.get("folded") or [])) for g in out]
    assert after == before


def test_partition_refuses_rather_than_skipping(monkeypatch, tmp_path):
    """Fail closed. With no resolved-operand read reachable the step must
    REFUSE, not silently emit the closure file it exists to prevent -- nothing
    downstream can tell a skipped partition from a clean one."""
    monkeypatch.setenv("DECOMP_SYNTH_ROOT", str(tmp_path))
    with pytest.raises(SystemExit) as e:
        iab.partition_emitted([], "accepted")
    assert "REFUSING" in str(e.value)


def test_partition_is_on_by_default_and_runs_after_the_merge():
    """A default of `off` would make every guarantee above opt-in, and a call
    site BEFORE the `--merge` block would leave the carry-forward free to
    re-grow the closures the split just dropped -- which is how the 158 got back
    into the file the first time. Both are read out of the source because the
    ordering, not just the flag, is the invariant."""
    src = (ROOT / "tools" / "icf_alias_build.py").read_text()
    assert 'ap.add_argument("--partition"' in src
    assert 'default="accepted"' in src
    call = src.index('if args.partition != "off":')
    merge = src.index("if args.merge:")
    write = src.index("Path(args.out).write_text")
    assert merge < call < write, (
        "the partition must run AFTER the --merge carry-forward and BEFORE the "
        "file is written")
