#!/usr/bin/env python3
"""Sabotage tests for scripts/obj_pairing.py and the coverage assertion it feeds.

Run: python3 -B scripts/test_obj_pairing.py     (no compiler, no build tree)

What this is defending
----------------------
Three post-compile patchers used to find an object's retail counterpart with
`obj_dir / rel`.  On rb3-xenon that guess resolved for 344 of the 1,045
compiled objects objdiff.json declares a target for; the other 701 hit a
`continue` and were counted as nothing at all.  Seven real pending guard
renames were sitting in that 701.

The failure is not "a wrong answer" -- it is "no answer, reported as a clean
one".  A test that only checks the happy path cannot see that, because the
happy path was ALWAYS green: the passes printed `344 files checked, 0 files
patched` and every consumer read the zero.

So every test here is built to the rules scripts/test_patch_state.py states:

1.  **Assert on the specific evidence, never on the exit code alone.**  The
    assertion helper raises for two quite different reasons (vacuous vs
    incomplete) and a test that only checked "it raised" would pass with the
    incompleteness detection removed.  Each test asserts the offending object
    is NAMED, and which class of failure it was filed under.

2.  **The negative control lives inside the test.**  Every sabotage is paired,
    in the same body, with the same fixture reading GREEN before it and (where
    applicable) after the repair.  `test_relpath_pairing_goes_silently_blind`
    is the strongest: it reproduces the ORIGINAL BUG in miniature and fails if
    the old relpath rule would have found the pending patch anyway -- i.e. it
    fails if the fixture stopped being a reproduction.

⚠ `.pyc` trap.  Two of these tests write a Python file, run it, rewrite it and
run it again, all inside one second.  A byte-length-preserving edit leaves
`(mtime, size)` unchanged, CPython loads the STALE bytecode, and the sabotage
silently never runs while the test reports a pass.  Every subprocess here is
launched with `-B` and `PYTHONDONTWRITEBYTECODE=1`, and the source edits are
deliberately length-CHANGING as a second line of defence.  Run this file three
times in a row before believing it; the class is intermittent by construction.
"""

import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))
import obj_pairing  # noqa: E402

BUILD_ID = "45410914"

NO_PYC = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")


# ── a minimal, real COFF object ─────────────────────────────────────────────

def make_coff(symbols):
    """A COFF/PPC object carrying exactly `symbols` = [(name, storage_class)].

    Sectionless and deliberately minimal: the guard patcher reads only the
    COFF header's symbol-table pointer/count, the 18-byte symbol entries and
    the string table, so this is enough to exercise the real parser and the
    real rewriter rather than a mock of them.
    """
    n = len(symbols)
    header = bytearray(20)
    struct.pack_into("<H", header, 0, 0x01F2)      # machine: PPCBE
    struct.pack_into("<H", header, 2, 0)           # sections
    struct.pack_into("<I", header, 8, 20)          # symbol table follows header
    struct.pack_into("<I", header, 12, n)
    entries, strtab = bytearray(), bytearray(b"\0\0\0\0")  # size patched below
    for name, cls in symbols:
        e = bytearray(18)
        struct.pack_into("<I", e, 0, 0)            # long name marker
        struct.pack_into("<I", e, 4, len(strtab))  # offset into string table
        struct.pack_into("<Ih", e, 8, 0, 0)        # value, section
        e[16] = cls
        e[17] = 0                                  # aux count
        entries += e
        strtab += name.encode() + b"\0"
    struct.pack_into("<I", strtab, 0, len(strtab))
    return bytes(header + entries + strtab)


#: One guard variable, spelled the way our compiler emits it (STATIC=3) and the
#: way retail does (EXTERNAL=2).  This is the exact shape of all seven real
#: pending patches the pairing fix uncovered.
GUARD = "?$S1@?1??Configure@Widget@@QAAXXZ@4IA"
OURS = [(GUARD, 3)]
RETAIL = [(GUARD, 2)]


class Fixture:
    """A throwaway repo shaped enough for the pairing + patchers to run.

    The layout REPRODUCES the real defect: the compiled object is nested
    (`src/band3/meta_band/Widget.obj`) while its target is FLAT
    (`obj/Widget.obj`), exactly as dtk names a bare `Widget.cpp:` splits
    heading.  `obj_dir / rel` therefore cannot find it.
    """

    def __init__(self, tmp):
        self.root = Path(tmp)
        self.build = self.root / "build" / BUILD_ID
        self.src = self.build / "src" / "band3" / "meta_band"
        self.obj = self.build / "obj"
        self.scripts = self.root / "scripts"
        for d in (self.src, self.obj, self.scripts):
            d.mkdir(parents=True, exist_ok=True)
        (self.src / "Widget.obj").write_bytes(make_coff(OURS))
        (self.obj / "Widget.obj").write_bytes(make_coff(RETAIL))
        for name in ("obj_pairing.py", "obj_guard_patcher.py"):
            shutil.copy(REPO / "scripts" / name, self.scripts / name)
        self.write_config([("default/Widget",
                            "build/%s/obj/Widget.obj" % BUILD_ID,
                            "build/%s/src/band3/meta_band/Widget.obj" % BUILD_ID)])

    def write_config(self, units):
        (self.root / "objdiff.json").write_text(json.dumps({
            "units": [{"name": n, "target_path": t, "base_path": b}
                      for n, t, b in units]}))

    def pairing(self):
        return obj_pairing.ObjPairing(
            self.root, self.obj, self.build / "src",
            self.root / "objdiff.json")

    def run_guard(self, *args):
        return subprocess.run(
            [sys.executable, "-B", str(self.scripts / "obj_guard_patcher.py"),
             "--batch", "--obj-dir", str(self.obj),
             "--src-dir", str(self.build / "src"),
             "--objdiff-config", str(self.root / "objdiff.json"), *args],
            cwd=str(self.root), capture_output=True, text=True, env=NO_PYC)


class PairingTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.fx = Fixture(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    # ── the original bug, reproduced and then closed ─────────────────────

    def test_relpath_pairing_goes_silently_blind(self):
        """The reproduction. objdiff.json finds the pair; relpath cannot.

        The control is the first assertion: if the fixture ever stops being a
        relpath-invisible pair, `relpath_only_reachable` becomes 1 and this
        test fails rather than quietly testing nothing.
        """
        cov = self.fx.pairing().coverage()
        self.assertEqual(cov["relpath_only_reachable"], 0,
                         "fixture no longer reproduces the bug: the relpath "
                         "guess can see this pair, so there is nothing to fix")
        self.assertEqual(cov["objects_declared"], 1)
        self.assertEqual(cov["objects_paired"], 1)
        self.assertEqual(self.fx.pairing().targets_for(
            "band3/meta_band/Widget.obj"), ["Widget.obj"])

    def test_the_patcher_finds_a_patch_the_old_rule_could_not(self):
        """End to end: the pending guard patch is real, and only pairing sees it.

        Sabotage = withdraw the objdiff.json declaration, which is exactly what
        the world looked like to the old relpath rule.  The pending patch then
        vanishes from the report WITHOUT any error -- that silence is the
        defect, so it is asserted explicitly rather than inferred.
        """
        green = self.fx.run_guard()
        self.assertEqual(green.returncode, 0, green.stderr)
        self.assertIn("1 files checked", green.stdout)
        self.assertIn("1 total symbol patches", green.stdout)

        self.fx.write_config([])                      # sabotage
        blind = self.fx.run_guard()
        self.assertEqual(blind.returncode, 0,
                         "the old behaviour was SILENT, not an error -- if "
                         "this now errors the test is asserting the wrong thing")
        self.assertIn("0 files checked", blind.stdout)
        self.assertIn("0 total symbol patches", blind.stdout)

        self.fx.write_config([("default/Widget",                 # repair
                               "build/%s/obj/Widget.obj" % BUILD_ID,
                               "build/%s/src/band3/meta_band/Widget.obj" % BUILD_ID)])
        self.assertIn("1 total symbol patches", self.fx.run_guard().stdout)

    def test_patch_is_actually_applied_and_mtime_preserved(self):
        before = (self.fx.src / "Widget.obj").read_bytes()
        st = (self.fx.src / "Widget.obj").stat()
        self.assertEqual(self.fx.run_guard("--apply").returncode, 0)
        after = (self.fx.src / "Widget.obj").read_bytes()
        self.assertNotEqual(before, after, "--apply changed nothing")
        self.assertEqual((self.fx.src / "Widget.obj").stat().st_mtime_ns,
                         st.st_mtime_ns,
                         "mtime was bumped -- ninja's deps=msvc records will "
                         "force a recompile/repatch oscillation")
        # And the tree is now a fixed point: the control that proves the first
        # run did the whole job rather than half of it.
        self.assertIn("0 total symbol patches", self.fx.run_guard().stdout)

    # ── the assertion: incomplete coverage ───────────────────────────────

    def test_declared_but_unpairable_object_raises_and_is_named(self):
        """A declared object whose target vanished must fail, by name."""
        obj_pairing.assert_full_coverage(self.fx.pairing().coverage(),
                                         min_declared=1)   # control: green

        (self.fx.obj / "Widget.obj").unlink()               # sabotage
        cov = self.fx.pairing().coverage()
        # It is now undeclared (the unit drops for a missing file) AND
        # unpairable, so the VACUITY arm must be the one that fires -- filing
        # it as "incomplete" would be the wrong diagnosis.
        with self.assertRaises(obj_pairing.PairingCoverageError) as cm:
            obj_pairing.assert_full_coverage(cov, min_declared=1)
        self.assertIn("vacuous", str(cm.exception))
        self.assertEqual(cov["units_dropped_missing_file"], 1)

    def test_incomplete_coverage_names_the_object_it_lost(self):
        """Two declared objects, one unpairable -> the incompleteness arm."""
        for n in ("A", "B"):
            (self.fx.build / "src" / (n + ".obj")).write_bytes(make_coff(OURS))
            (self.fx.obj / (n + ".obj")).write_bytes(make_coff(RETAIL))
        units = [("default/%s" % n, "build/%s/obj/%s.obj" % (BUILD_ID, n),
                  "build/%s/src/%s.obj" % (BUILD_ID, n)) for n in ("A", "B")]
        self.fx.write_config(units)
        cov = self.fx.pairing().coverage()
        self.assertEqual(cov["objects_declared"], 2)
        obj_pairing.assert_full_coverage(cov, min_declared=1)     # control

        # Sabotage: teach the resolver to answer nothing for B, the way a
        # reverted pairing rule would.  Subclassing (not editing source) keeps
        # the real module out of reach of a stale .pyc.
        class Blind(obj_pairing.ObjPairing):
            def targets_for(self, base_rel):
                return [] if str(base_rel) == "B.obj" else super().targets_for(base_rel)

        cov = Blind(self.fx.root, self.fx.obj, self.fx.build / "src",
                    self.fx.root / "objdiff.json").coverage()
        with self.assertRaises(obj_pairing.PairingCoverageError) as cm:
            obj_pairing.assert_full_coverage(cov, min_declared=1)
        msg = str(cm.exception)
        self.assertIn("B.obj", msg, "the lost object was not named")
        self.assertNotIn("vacuous", msg, "misfiled as vacuity, not incompleteness")
        self.assertEqual(cov["declared_unpaired"], ["B.obj"])

    # ── the assertion: vacuity ───────────────────────────────────────────

    def test_examining_nothing_is_not_success(self):
        """No config, empty config and a too-small population all refuse.

        This is the defect that opened the whole line of work: a sweep that
        exits 0 having analysed nothing.  Each arm is checked separately
        because they reach the failure by different routes.
        """
        obj_pairing.assert_full_coverage(self.fx.pairing().coverage(),
                                         min_declared=1)   # control

        (self.fx.root / "objdiff.json").unlink()
        cov = self.fx.pairing().coverage()
        self.assertFalse(cov["config_readable"])
        with self.assertRaises(obj_pairing.PairingCoverageError) as cm:
            obj_pairing.assert_full_coverage(cov, min_declared=1)
        self.assertIn("unreadable", str(cm.exception))

        self.fx.write_config([])
        cov = self.fx.pairing().coverage()
        self.assertTrue(cov["config_readable"])
        self.assertEqual(cov["objects_declared"], 0)
        with self.assertRaises(obj_pairing.PairingCoverageError) as cm:
            obj_pairing.assert_full_coverage(cov, min_declared=1)
        self.assertIn("vacuous", str(cm.exception))

    def test_a_shrunken_population_refuses(self):
        """A tree that pairs 1 object cannot satisfy a 900-object floor.

        This is the arm that catches configure.py emitting a mostly-empty
        objdiff.json -- every ratio is 100% and every list is empty, so only an
        absolute floor can tell the difference between 'complete' and 'gone'.
        """
        cov = self.fx.pairing().coverage()
        obj_pairing.assert_full_coverage(cov, min_declared=1)      # control
        with self.assertRaises(obj_pairing.PairingCoverageError) as cm:
            obj_pairing.assert_full_coverage(cov, min_declared=900)
        self.assertIn("vacuous", str(cm.exception))

    # ── the ambiguity the real tree has ──────────────────────────────────

    def test_multi_target_object_yields_every_declared_target(self):
        """One base, two units, two targets -> both, and it is counted.

        Reproduces UIStats/AccomplishmentProgress/Game.  The control is the
        single-target assertion first: if the fixture degenerated to one unit,
        `objects_multi_target` would be 0 and the test would fail.
        """
        self.assertEqual(self.fx.pairing().coverage()["objects_multi_target"], 0)

        (self.fx.obj / "band3").mkdir()
        (self.fx.obj / "band3" / "meta_band").mkdir()
        (self.fx.obj / "band3" / "meta_band" / "Widget.obj").write_bytes(
            make_coff(RETAIL))
        base = "build/%s/src/band3/meta_band/Widget.obj" % BUILD_ID
        self.fx.write_config([
            ("default/Widget", "build/%s/obj/Widget.obj" % BUILD_ID, base),
            ("default/band3/meta_band/Widget",
             "build/%s/obj/band3/meta_band/Widget.obj" % BUILD_ID, base)])
        p = self.fx.pairing()
        cov = p.coverage()
        self.assertEqual(cov["objects_multi_target"], 1)
        self.assertEqual(cov["objects_declared"], 1,
                         "objects, not units -- conflating them is how "
                         "'347 of 1048' was reported for a loop that ran 344")
        self.assertEqual(cov["declared_units"], 2)
        self.assertEqual(sorted(p.targets_for("band3/meta_band/Widget.obj")),
                         ["Widget.obj", "band3/meta_band/Widget.obj"])

    def test_undeclared_object_falls_back_to_relpath_but_never_invents(self):
        """160 real objects are built but unpinned; they must not be forged."""
        (self.fx.build / "src" / "Loose.obj").write_bytes(make_coff(OURS))
        p = self.fx.pairing()
        self.assertEqual(p.targets_for("Loose.obj"), [],
                         "invented a pairing for an object with no target")
        (self.fx.obj / "Loose.obj").write_bytes(make_coff(RETAIL))
        self.assertEqual(self.fx.pairing().targets_for("Loose.obj"),
                         ["Loose.obj"])
        # Undeclared objects must never count against declared coverage.
        cov = self.fx.pairing().coverage()
        self.assertEqual(cov["objects_declared"], 1)
        self.assertEqual(cov["objects_undeclared_but_on_disk"], 1)
        obj_pairing.assert_full_coverage(cov, min_declared=1)

    # ── the CLI, which is what a human and a ninja edge actually call ────

    def test_cli_check_exits_nonzero_on_a_blind_pairing(self):
        def run():
            return subprocess.run(
                [sys.executable, "-B", str(self.fx.scripts / "obj_pairing.py"),
                 "--repo", str(self.fx.root), "--obj-dir", str(self.fx.obj),
                 "--src-dir", str(self.fx.build / "src"),
                 "--objdiff-config", str(self.fx.root / "objdiff.json"),
                 "--check"], capture_output=True, text=True, env=NO_PYC)
        ok = run()                                          # control
        self.assertEqual(ok.returncode, 0, ok.stderr)
        self.assertIn("1/1 declared", ok.stdout)

        self.fx.write_config([])                            # sabotage
        bad = run()
        self.assertEqual(bad.returncode, 2)
        self.assertIn("FAIL[pairing]", bad.stderr)
        self.assertIn("vacuous", bad.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
