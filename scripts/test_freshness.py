#!/usr/bin/env python3
"""Sabotage tests for scripts/analysis/freshness.py.

Run: python3 scripts/test_freshness.py      (no compiler, no build dir, no toolchain)

Why these are shaped the way they are
-------------------------------------
The thing being tested is a REFUSAL, and a refusal is the easiest kind of
behaviour to fake: any exception, from any cause, looks like the guard working.
So, following scripts/test_patch_state.py's two rules:

1.  Assert on the SPECIFIC evidence, never on "it raised".  Each test requires
    the refusal message to name its own cause (`VACUOUS MANIFEST`, `STALE
    REPORT`, ...), because a test that only asserted `StaleTreeError` would
    still pass with every check but one ripped out.
2.  The negative control lives inside the test body: every sabotage is paired
    with the same fixture, repaired, reading GREEN.  Without that half, a
    fixture that is broken for an unrelated reason (a typo'd path, say) would
    make the whole file pass while proving nothing.

`test_vacuity_floor_is_load_bearing` is the strongest of these.  It asserts
that the UNDERLYING oracle -- verify_objs_patched --verify-manifest, reached
through the tree's own patch_guard -- reports success on a manifest recording
zero objects, and that freshness refuses it anyway.  That is not hypothetical:
run against a real empty manifest, verify_objs_patched prints
"[patch-state] OK: 0 decomp, 0 target objects match" and exits 0.  If someone
deletes the floor as redundant, this test goes red.
"""

import json
import os
import sys
import textwrap
import time
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO))

from scripts.analysis import freshness  # noqa: E402

BUILD = "build/45410914"

#: A patch_guard stand-in.  The real one hashes 4,293 objects; these tests are
#: about the checks LAYERED ON IT, so it is stubbed -- but stubbed such that it
#: can still fail, because a stub that always succeeds would quietly convert
#: "freshness consults the object oracle" into an untested claim.
STUB_GUARD = '''
class UnpatchedTreeError(RuntimeError):
    pass


def ensure_patched_tree(project_dir, *, build=True):
    import os
    if os.environ.get("STUB_GUARD_FAIL"):
        raise UnpatchedTreeError("stub oracle says: 2 content differs")
    return "stub patch state verified"
'''


class Fixture:
    """A tree shaped like the real one, with nothing in it that needs building."""

    def __init__(self, root: Path, n_objects: int = 4293):
        self.root = root
        (root / BUILD).mkdir(parents=True, exist_ok=True)
        (root / "scripts" / "orchestrator").mkdir(parents=True, exist_ok=True)
        (root / "scripts" / "orchestrator" / "patch_guard.py").write_text(
            textwrap.dedent(STUB_GUARD))
        self.write_manifest(n_objects)
        self.write_report()

    def write_manifest(self, n_objects: int) -> None:
        decomp = n_objects // 4
        (self.root / BUILD / "patch_state.json").write_text(json.dumps({
            "n_objects": n_objects,
            "n_decomp_objects": decomp if n_objects else 0,
            "n_target_objects": n_objects - decomp if n_objects else 0,
            "generated_utc": "2026-09-01T08:25:25Z",
            "tree_sha256": "0" * 64,
            "objects": {}, "target_objects": {},
        }))

    def write_report(self, total_code: int = 10_245_956,
                     total_functions: int = 69_219, units: int = 3) -> None:
        # protobuf-JSON shape: numerics arrive as STRINGS, defaults omitted.
        (self.root / BUILD / "report.json").write_text(json.dumps({
            "measures": {"total_code": str(total_code),
                         "total_functions": str(total_functions)},
            "units": [{"name": f"default/U{i}"} for i in range(units)],
        }))

    def order(self, report_newer: bool) -> None:
        """Set the report/manifest mtime ordering explicitly."""
        m = self.root / BUILD / "patch_state.json"
        r = self.root / BUILD / "report.json"
        now = time.time()
        os.utime(m, (now, now))
        os.utime(r, (now + 60, now + 60) if report_newer else (now - 60, now - 60))

    def check(self, **kw) -> str:
        return freshness.ensure_measurable(self.root, consumer="test", **kw)


class FreshnessTest(unittest.TestCase):
    def setUp(self):
        import tempfile
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        os.environ.pop("STUB_GUARD_FAIL", None)

    def tearDown(self):
        os.environ.pop("STUB_GUARD_FAIL", None)
        self._tmp.cleanup()

    # -- control: a healthy fixture must PASS, or every test below is vacuous --
    def test_healthy_tree_passes(self):
        f = Fixture(self.root)
        f.order(report_newer=True)
        note = f.check()
        self.assertIn("report.json OK", note)
        self.assertIn("69,219", note)

    def test_vacuity_floor_is_load_bearing(self):
        """An EMPTY manifest verifies perfectly.  That must not read as fresh."""
        f = Fixture(self.root, n_objects=0)
        f.order(report_newer=True)
        # The oracle itself is happy: nothing recorded, nothing drifted.
        import importlib.util
        spec = importlib.util.spec_from_file_location(
            "_stub_guard", self.root / "scripts" / "orchestrator" / "patch_guard.py")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        self.assertEqual(mod.ensure_patched_tree(self.root, build=False),
                         "stub patch state verified")
        with self.assertRaises(freshness.StaleTreeError) as cm:
            f.check()
        self.assertIn("VACUOUS MANIFEST", str(cm.exception))
        # control: the SAME fixture with a plausible count passes.
        f.write_manifest(4293)
        f.order(report_newer=True)
        self.assertIn("report.json OK", f.check())

    def test_below_floor_but_nonzero_refused(self):
        """The FLOOR itself, not the zero-count guard beside it.

        Written after the mutation battery caught the previous test lying:
        deleting `MIN_MANIFEST_OBJECTS` left the suite GREEN, because a
        zero-object fixture is also caught by `n_decomp <= 0`.  A manifest with
        40 objects has every count positive, so only the floor can refuse it.
        """
        f = Fixture(self.root, n_objects=40)      # 10 decomp / 30 target
        f.order(report_newer=True)
        with self.assertRaises(freshness.StaleTreeError) as cm:
            f.check()
        self.assertIn("below the", str(cm.exception))
        f.write_manifest(4293)                    # control: repair passes
        f.order(report_newer=True)
        self.assertIn("report.json OK", f.check())

    def test_inconsistent_manifest_refused(self):
        """A plausible TOTAL hiding an empty half.

        The floor alone cannot catch this -- n_objects clears 500 while one
        whole population is empty, which is what a manifest written against a
        half-populated build dir looks like.  Added because the mutation
        battery showed the zero-count guard was otherwise untested.
        """
        f = Fixture(self.root)
        (self.root / BUILD / "patch_state.json").write_text(json.dumps({
            "n_objects": 4293, "n_decomp_objects": 0, "n_target_objects": 4293,
            "generated_utc": "x", "tree_sha256": "0" * 64,
            "objects": {}, "target_objects": {},
        }))
        f.order(report_newer=True)
        with self.assertRaises(freshness.StaleTreeError) as cm:
            f.check()
        self.assertIn("0 decomp", str(cm.exception))
        f.write_manifest(4293)                    # control: repair passes
        f.order(report_newer=True)
        self.assertIn("report.json OK", f.check())

    def test_object_oracle_is_actually_consulted(self):
        f = Fixture(self.root)
        f.order(report_newer=True)
        self.assertIn("report.json OK", f.check())          # control
        os.environ["STUB_GUARD_FAIL"] = "1"
        with self.assertRaises(freshness.StaleTreeError) as cm:
            f.check()
        self.assertIn("2 content differs", str(cm.exception))

    def test_stale_report_refused_then_repaired(self):
        f = Fixture(self.root)
        f.order(report_newer=False)
        with self.assertRaises(freshness.StaleTreeError) as cm:
            f.check()
        self.assertIn("STALE REPORT", str(cm.exception))
        f.order(report_newer=True)                          # repair
        self.assertIn("report.json OK", f.check())

    def test_vacuous_report_refused(self):
        f = Fixture(self.root)
        f.write_report(total_code=0)
        f.order(report_newer=True)
        with self.assertRaises(freshness.StaleTreeError) as cm:
            f.check()
        self.assertIn("VACUOUS REPORT", str(cm.exception))

    def test_absent_manifest_refused(self):
        f = Fixture(self.root)
        (self.root / BUILD / "patch_state.json").unlink()
        with self.assertRaises(freshness.StaleTreeError) as cm:
            f.check()
        self.assertIn("patch_state.json", str(cm.exception))

    def test_absent_patch_guard_refused(self):
        """Absence of the oracle is a REFUSAL, never a degraded pass."""
        f = Fixture(self.root)
        f.order(report_newer=True)
        (self.root / "scripts" / "orchestrator" / "patch_guard.py").unlink()
        with self.assertRaises(freshness.StaleTreeError) as cm:
            f.check()
        self.assertIn("patch_guard.py", str(cm.exception))

    def test_no_report_mode_skips_only_the_report_checks(self):
        f = Fixture(self.root)
        f.order(report_newer=False)          # would fail with need_report
        self.assertNotIn("report.json OK", f.check(need_report=False))

    # ---- tool identity + alias map: the non-object staleness axes ----------
    def _with_tool(self, f, commit="deadbeefcafe", xxh3="9b2bb6f1f3a21062",
                   live_xxh3="9b2bb6f1f3a21062", map_rel=None):
        """Give the fixture a fake objdiff-cli whose --version we control."""
        cli = self.root / "bin" / "objdiff-cli"
        cli.parent.mkdir(parents=True, exist_ok=True)
        cli.write_text("#!/bin/sh\n"
                       f'echo "objdiff-cli 4.2.8 ({commit}, xxh3 {live_xxh3})"\n')
        cli.chmod(0o755)
        prov = {"tool_binary_hash": xxh3, "tool_commit": commit}
        if map_rel:
            prov["map_file"] = map_rel
            prov["map_file_entries"] = 5449
        doc = json.loads((self.root / BUILD / "report.json").read_text())
        doc["provenance"] = prov
        (self.root / BUILD / "report.json").write_text(json.dumps(doc))
        f.order(report_newer=True)

    def test_tool_swap_refused_then_matching_tool_passes(self):
        f = Fixture(self.root)
        self._with_tool(f, live_xxh3="faf3390631a58473")   # rebuilt under us
        with self.assertRaises(freshness.StaleTreeError) as cm:
            f.check()
        msg = str(cm.exception)
        self.assertIn("STALE TOOL", msg)
        self.assertIn("9b2bb6f1f3a21062", msg)             # names BOTH rulers
        self.assertIn("faf3390631a58473", msg)
        self._with_tool(f)                                 # control: same tool
        self.assertIn("tool OK", f.check())

    def test_absent_report_tool_identity_is_declared_unverifiable(self):
        """No provenance is NOT agreement -- it must say so out loud."""
        f = Fixture(self.root)
        f.order(report_newer=True)
        self.assertIn("tool identity UNVERIFIABLE", f.check())

    def test_alias_map_newer_than_report_refused(self):
        f = Fixture(self.root)
        m = self.root / BUILD / "icf_aliases.map"
        m.write_text("alias map")
        self._with_tool(f, map_rel=f"{BUILD}/icf_aliases.map")
        self.assertIn("alias map OK", f.check())           # control first
        r = (self.root / BUILD / "report.json").stat().st_mtime
        os.utime(m, (r + 120, r + 120))                    # edited out of graph
        with self.assertRaises(freshness.StaleTreeError) as cm:
            f.check()
        self.assertIn("STALE ALIAS MAP", str(cm.exception))

    def test_every_stale_subject_is_named_not_just_the_first(self):
        """Two axes stale at once must both appear, or the remedy is wrong."""
        f = Fixture(self.root)
        self._with_tool(f, live_xxh3="faf3390631a58473")
        os.environ["STUB_GUARD_FAIL"] = "1"                # objects stale too
        with self.assertRaises(freshness.StaleTreeError) as cm:
            f.check()
        msg = str(cm.exception)
        self.assertIn("STALE TOOL", msg)
        self.assertIn("STALE OBJECTS", msg)
        self.assertIn("2 stale input(s)", msg)

    def test_allow_stale_banners_the_cause_and_does_not_claim_freshness(self):
        import contextlib
        import io
        f = Fixture(self.root, n_objects=0)
        buf = io.StringIO()
        with contextlib.redirect_stderr(buf):
            note = f.check(allow_stale=True)
        err = buf.getvalue()
        self.assertIn("VACUOUS MANIFEST", err)        # names WHAT is stale
        self.assertIn("--allow-stale", err)
        self.assertIn("ONE-DIRECTIONAL", err)
        self.assertTrue(note.startswith("STALE"))     # never reads as a pass


if __name__ == "__main__":
    unittest.main(verbosity=2)
