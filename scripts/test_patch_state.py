#!/usr/bin/env python3
"""Sabotage tests for scripts/verify_objs_patched.py and patch_guard.py.

Run: python3 scripts/test_patch_state.py        (no compiler, no build dir)

Why these are shaped the way they are
-------------------------------------
This repo has shipped guards that passed on deliberately broken code -- one of
them written specifically to catch the bug it then missed.  A sibling repo's
patch-state lane hit the same trap in a subtler form: its sabotage "went red"
on a WARNING MESSAGE rather than on the absence of an exception, so the obvious
`assertRaises`-shaped assertion was equally true with and without the bug.

So every test here is built to two rules:

1.  **Assert on the specific evidence, never on the exit code alone.**  A
    non-zero exit proves something failed, not that the RIGHT thing failed --
    the verifier exits 2 for a missing manifest and 1 for drift, and a test
    that only checked "non-zero" would pass with the drift detection ripped
    out.  Each test therefore asserts the offending path is NAMED in the
    output, and under the correct section heading.

2.  **The negative control lives inside the test.**  Every sabotage is paired,
    in the same test body, with (a) the same fixture unsabotaged reading GREEN
    and (b) where applicable, a repair reading GREEN again.  If someone weakens
    a fixture so the sabotage stops being a sabotage, the control fails rather
    than the assertion silently passing.  `test_check_runs_every_patcher` is
    the strongest of these: it fails if the verifier reports a fixed point
    while having actually run fewer than all six passes -- i.e. it is the test
    for "the check passed because it checked nothing".
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
VERIFY = REPO / "scripts" / "verify_objs_patched.py"
BUILD_ID = "45410914"

PATCHERS = [
    "obj_anon_ns_patcher.py",
    "obj_dynamic_init_patcher.py",
    "obj_guard_patcher.py",
    "obj_bool_mangle_patcher.py",
    "obj_atexit_scope_patcher.py",
    "obj_eh_boundary_patcher.py",
]

#: A stub patcher that records that it ran and exits with a chosen code. The
#: recording is what makes "did the verifier actually run all six?" testable.
STUB = textwrap.dedent("""\
    import os, sys, pathlib
    pathlib.Path(os.environ["PATCH_STUB_LOG"]).open("a").write(
        pathlib.Path(__file__).name + "\\n")
    rc = int(os.environ.get("PATCH_STUB_RC_" +
             pathlib.Path(__file__).stem.upper(), "0"))
    if rc:
        print("FAIL[stub]: 1 pending patch(es)", file=sys.stderr)
    sys.exit(rc)
""")


class Fixture:
    """A throwaway repo shaped enough for verify_objs_patched.py to run.

    Deliberately synthetic: these tests must not need a compiler, must not
    need a 334 MB build tree, and above all must never touch a shared
    checkout's objects -- the patchers rewrite in place.
    """

    def __init__(self, tmp: Path, *, n_decomp=3, n_target=2):
        self.root = tmp
        self.build = tmp / "build" / BUILD_ID
        self.src = self.build / "src" / "sys"
        self.obj = self.build / "obj"
        self.scripts = tmp / "scripts"
        for d in (self.src, self.obj, self.scripts):
            d.mkdir(parents=True, exist_ok=True)
        shutil.copy(VERIFY, self.scripts / "verify_objs_patched.py")
        # The verifier derives coverage from the patchers' OWN pairing module
        # rather than reimplementing the rule, so the fixture needs it too.
        shutil.copy(REPO / "scripts" / "obj_pairing.py",
                    self.scripts / "obj_pairing.py")
        self.decomp = []
        for i in range(n_decomp):
            p = self.src / f"d{i}.obj"
            p.write_bytes(b"DECOMP-PATCHED-%d" % i)
            self.decomp.append(p)
        self.targets = []
        for i in range(n_target):
            p = self.obj / f"t{i}.obj"
            p.write_bytes(b"TARGET-RENAMED-%d" % i)
            self.targets.append(p)
        (tmp / "objdiff.json").write_text(json.dumps({"units": [
            {"name": f"default/d{i}",
             "target_path": f"build/{BUILD_ID}/obj/t0.obj",
             "base_path": f"build/{BUILD_ID}/src/sys/d{i}.obj"}
            for i in range(n_decomp)]}))
        self.log = tmp / "stub.log"
        for name in PATCHERS:
            (self.scripts / name).write_text(STUB)

    def run(self, *args, rc_overrides=None, min_declared=1):
        """Invoke the verifier against this fixture.

        `min_declared` defaults to 1 because a 3-object fixture cannot satisfy
        the real tree's 900-object vacuity floor.  Lowering it here is safe
        ONLY because `test_the_real_min_declared_floor_is_armed` asserts that
        the DEFAULT floor still rejects this same fixture -- without that
        control, passing the flag everywhere would silently disarm the check
        for the whole suite.
        """
        env = dict(os.environ, RB3_VERSION=BUILD_ID,
                   PATCH_STUB_LOG=str(self.log), PYTHONDONTWRITEBYTECODE="1")
        for k, v in (rc_overrides or {}).items():
            env[f"PATCH_STUB_RC_{k.upper()}"] = str(v)
        if self.log.exists():
            self.log.unlink()
        if min_declared is not None:
            args = (*args, "--min-declared", str(min_declared))
        return subprocess.run(
            [sys.executable, "-B",
             str(self.scripts / "verify_objs_patched.py"),
             "--repo", str(self.root), *args],
            capture_output=True, text=True, env=env, cwd=str(self.root))

    def ran(self):
        return set(self.log.read_text().split()) if self.log.exists() else set()


class PatchStateTests(unittest.TestCase):

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory(dir=os.path.expanduser("~/tmp"))
        self.fx = Fixture(Path(self._tmp.name))

    def tearDown(self):
        self._tmp.cleanup()

    # ── the manifest half: patcher-independent, so it is the load-bearing one ──

    def test_decomp_drift_is_caught_and_named(self):
        self.assertEqual(self.fx.run("--check", "--emit").returncode, 0)
        # CONTROL: unsabotaged, the very same call must read GREEN. Without
        # this, an always-red verifier would pass the sabotage assertion.
        green = self.fx.run("--verify-manifest")
        self.assertEqual(green.returncode, 0, green.stderr)

        victim = self.fx.decomp[1]
        victim.write_bytes(b"RAW-COMPILER-OUTPUT")
        red = self.fx.run("--verify-manifest")
        self.assertEqual(red.returncode, 1, red.stdout + red.stderr)
        # Assert on the EVIDENCE, not the code: rc=1 alone would also be true
        # if the manifest had simply vanished.
        self.assertIn("d1.obj", red.stderr)
        self.assertIn("[decomp]", red.stderr)

        # CONTROL: repair restores GREEN, proving the detector responded to
        # this mutation and not to some persistent property of the fixture.
        victim.write_bytes(b"DECOMP-PATCHED-1")
        self.assertEqual(self.fx.run("--verify-manifest").returncode, 0)

    def test_target_side_drift_is_caught_separately(self):
        """The rb3-xenon-specific half: pre-compile renamer state.

        A verifier that only covered decomp objects -- i.e. a straight port of
        dc3's, which has no target-side pass -- passes every other test in
        this file. This is the one that fails.
        """
        self.assertEqual(self.fx.run("--check", "--emit").returncode, 0)
        doc = json.loads((self.fx.build / "patch_state.json").read_text())
        # CONTROL, and it caught a real hole in an earlier draft of this test:
        # a verifier that records NO target objects still goes red below --
        # every target obj is then "not in the manifest" -- and the assertions
        # that follow would certify a verifier that had stopped hashing the
        # target side entirely. Pin the recorded set, then insist the red is
        # specifically a CONTENT disagreement.
        self.assertEqual(set(doc["target_objects"]),
                         {f"build/{BUILD_ID}/obj/t{i}.obj" for i in range(2)})

        victim = self.fx.targets[0]
        victim.write_bytes(b"fn_82001234-NOT-RENAMED")
        red = self.fx.run("--verify-manifest")
        self.assertEqual(red.returncode, 1, red.stdout + red.stderr)
        self.assertIn("[target]", red.stderr)
        target_block = red.stderr.split("[target]")[1]
        self.assertIn("content differs", target_block)
        self.assertIn("t0.obj", target_block)
        # CONTROL: the decomp section must NOT be implicated -- otherwise the
        # test would pass on a verifier that just reports everything.
        decomp_block = red.stderr.split("[target]")[0]
        self.assertNotIn("d0.obj", decomp_block)

    def test_drift_is_caught_even_when_mtime_is_preserved(self):
        """The reason the manifest is content-keyed at all.

        Every patcher restores the object's mtime after rewriting it (their
        `_write_preserving_mtime` docstrings explain why: ninja's `deps = msvc`
        records otherwise force an endless recompile/repatch oscillation). So
        patch state is invisible in timestamps, and a manifest keyed on mtime
        would be a guard that cannot fail.
        """
        self.assertEqual(self.fx.run("--check", "--emit").returncode, 0)
        victim = self.fx.decomp[0]
        st = victim.stat()
        victim.write_bytes(b"DIFFERENT-CONTENT-X")
        os.utime(victim, ns=(st.st_atime_ns, st.st_mtime_ns))
        # CONTROL, inside the test: if this assertion fails the fixture is not
        # exercising the mtime-preserving case at all, and the real assertion
        # below would be passing for the wrong reason.
        self.assertEqual(victim.stat().st_mtime_ns, st.st_mtime_ns,
                         "fixture failed to preserve mtime -- this test would "
                         "otherwise be silently testing the easy case")
        red = self.fx.run("--verify-manifest")
        self.assertEqual(red.returncode, 1, red.stdout + red.stderr)
        self.assertIn("d0.obj", red.stderr)

    def test_new_unrecorded_object_is_caught(self):
        self.assertEqual(self.fx.run("--check", "--emit").returncode, 0)
        (self.fx.src / "sneaked.obj").write_bytes(b"NEW")
        red = self.fx.run("--verify-manifest")
        self.assertEqual(red.returncode, 1)
        self.assertIn("sneaked.obj", red.stderr)

    def test_missing_manifest_is_a_DISTINCT_outcome(self):
        """rc=2 (never verified) must not be confused with rc=1 (drifted).

        If both collapsed to "non-zero", every drift assertion in this file
        would pass against a verifier whose drift detection was deleted.
        """
        red = self.fx.run("--verify-manifest")
        self.assertEqual(red.returncode, 2, red.stdout + red.stderr)
        self.assertIn("never been verified patched", red.stderr)

    # ── WHY an object differs: three causes, three verdicts ───────────────
    #
    # This tool used to print ONE explanation for every disagreement -- "produced
    # OUTSIDE the full build graph ... the post-compile patch passes never ran on
    # it" -- which is a MECHANISM IT CANNOT OBSERVE.  On 2026-09-11 a lane
    # sampled main 2m40s into an ordinary full build and got that accusation; two
    # investigations were opened off the message and both concluded wrongly.
    # Each test below pairs its verdict with the SAME drift under the other
    # condition, so a verifier that collapsed the three back into one fails here.

    def test_a_build_in_flight_is_a_DISTINCT_verdict_not_an_accusation(self):
        """rc=5: a held build lock means the tree is not adjudicable at all."""
        import fcntl
        self.assertEqual(self.fx.run("--check", "--emit").returncode, 0)
        self.fx.decomp[0].write_bytes(b"REWRITTEN-BY-A-BUILD")

        # CONTROL: with no build running, this very same drift IS corruption.
        # Without it, a verifier that always returned 5 would pass below.
        lone = self.fx.run("--verify-manifest")
        self.assertEqual(lone.returncode, 1, lone.stdout + lone.stderr)
        self.assertIn("OUTSIDE the full build graph", lone.stderr)

        fd = os.open(str(self.fx.root / ".ninja-build.lock"),
                     os.O_CREAT | os.O_RDWR, 0o644)
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            busy = self.fx.run("--verify-manifest")
        finally:
            os.close(fd)
        self.assertEqual(busy.returncode, 5, busy.stdout + busy.stderr)
        self.assertIn("BUILD IN PROGRESS", busy.stderr)
        self.assertNotIn("OUTSIDE the full build graph", busy.stderr)

        # ...and it reverts to corruption once the lock is released, so rc=5 is
        # attributable to the LOCK and not to the mutation.
        self.assertEqual(self.fx.run("--verify-manifest").returncode, 1)

    def test_tree_advanced_is_REBUILD_PENDING_not_corruption(self):
        """rc=4: a merge moves the split inputs, and the next build rewrites
        objects legitimately.  The discriminator is recorded IN the manifest,
        so it describes the state the hashes were taken over."""
        cfg = self.fx.root / "config" / BUILD_ID
        cfg.mkdir(parents=True, exist_ok=True)
        splits = cfg / "splits.txt"
        splits.write_text("Foo.cpp:\n    .text start:0x82000000 end:0x82000010\n")

        self.assertEqual(self.fx.run("--check", "--emit").returncode, 0)
        self.fx.decomp[0].write_bytes(b"REBUILT-AT-A-NEW-STATE")

        # CONTROL: identical drift, split inputs UNCHANGED -> still corruption.
        same = self.fx.run("--verify-manifest")
        self.assertEqual(same.returncode, 1, same.stdout + same.stderr)
        self.assertIn("OUTSIDE the full build graph", same.stderr)

        splits.write_text("Foo.cpp:\n    .text start:0x82000000 end:0x82000020\n")
        moved = self.fx.run("--verify-manifest")
        self.assertEqual(moved.returncode, 4, moved.stdout + moved.stderr)
        self.assertIn("REBUILD IS PENDING", moved.stderr)
        self.assertIn("splits.txt", moved.stderr)
        self.assertNotIn("OUTSIDE the full build graph", moved.stderr)

    def test_a_provenanceless_manifest_says_UNDETERMINED_rather_than_guessing(self):
        """A pre-v3 manifest cannot say WHY, so the tool must not say why.

        Still rc=1 -- the safe direction -- but without an accusation it has no
        evidence for.
        """
        self.assertEqual(self.fx.run("--check", "--emit").returncode, 0)
        mpath = self.fx.build / "patch_state.json"
        doc = json.loads(mpath.read_text())
        doc.pop("provenance")
        doc["manifest_version"] = 2
        mpath.write_text(json.dumps(doc))
        self.fx.decomp[0].write_bytes(b"RAW")

        red = self.fx.run("--verify-manifest")
        self.assertEqual(red.returncode, 1, red.stdout + red.stderr)
        self.assertIn("CAUSE UNDETERMINED", red.stderr)
        self.assertNotIn("OUTSIDE the full build graph", red.stderr)

    # ── the --check half: orchestration over the six real passes ──────────

    def test_check_runs_every_patcher(self):
        """The anti-vacuity control: a green light must be EARNED by all six.

        This is the test for "the check passed because it checked nothing".
        Truncate verify_objs_patched.PATCHERS, misspell an entry, or make the
        loop exit early, and this fails while every other test still passes.
        """
        res = self.fx.run("--check")
        self.assertEqual(res.returncode, 0, res.stderr)
        self.assertEqual(self.fx.ran(), set(PATCHERS),
                         "verifier reported a fixed point without running "
                         "every post-compile pass")

    def test_any_single_pending_pass_turns_the_tree_red(self):
        for name in PATCHERS:
            with self.subTest(patcher=name):
                stem = Path(name).stem
                red = self.fx.run("--check", rc_overrides={stem: 2})
                self.assertEqual(red.returncode, 1, red.stdout + red.stderr)
                self.assertIn(name, red.stderr)
                self.assertIn("NOT FULLY PATCHED", red.stderr)
                # CONTROL: the same fixture with that pass green reads GREEN,
                # so the red above is attributable to this pass alone.
                self.assertEqual(self.fx.run("--check").returncode, 0)

    def test_check_failure_blocks_the_manifest(self):
        """A red --check must not be papered over by emitting a manifest.

        Otherwise the next --verify-manifest would cheerfully certify an
        unpatched tree as the reference state.
        """
        red = self.fx.run("--check", "--emit",
                          rc_overrides={"obj_eh_boundary_patcher": 2})
        self.assertEqual(red.returncode, 1)
        self.assertFalse((self.fx.build / "patch_state.json").exists(),
                         "manifest was written despite a failing --check")

    # ── coverage reporting: a green light must state its denominator ──────

    def test_manifest_records_pairing_coverage(self):
        """The manifest must state what the green light was worth.

        Schema is MANIFEST_VERSION 3: counts are over DISTINCT COMPILED
        OBJECTS, not objdiff.json units (v1 counted units, which
        double-counted the three objects the real tree declares twice and
        reported "347 of 1048" for a loop that examined 344 of 1045), and v3
        adds the `provenance` block that lets a later disagreement be
        EXPLAINED rather than guessed at.
        """
        self.assertEqual(self.fx.run("--check", "--emit").returncode, 0)
        doc = json.loads((self.fx.build / "patch_state.json").read_text())
        self.assertEqual(doc["manifest_version"], 3)
        self.assertIn("split_inputs", doc["provenance"])
        self.assertIn("git_head", doc["provenance"])
        cov = doc["pairing_coverage"]
        # The fixture points all three units at t0.obj while their base objs
        # are d0/d1/d2, so relpath pairing reaches NONE of them -- the real
        # repo's defect in miniature.  Asserting the 0 is what keeps this from
        # degenerating into a test of a constant.
        self.assertEqual(cov["relpath_only_reachable"], 0,
                         "fixture stopped reproducing the relpath blindness")
        self.assertEqual(cov["objects_declared"], 3)
        self.assertEqual(cov["objects_paired"], 3,
                         "objdiff.json pairing failed to reach the population "
                         "relpath keying could not")
        self.assertEqual(cov["declared_unpaired"], [])
        self.assertEqual(doc["n_decomp_objects"], 3)
        self.assertEqual(doc["n_target_objects"], 2)

    def test_unpairable_declared_object_fails_the_build(self):
        """A declared object with no target must be an ERROR, not a low number.

        The whole defect class: an unpaired object is UNEXAMINED, and every
        pairing-driven pass reports it exactly as it reports a clean one.
        """
        self.assertEqual(self.fx.run("--check", "--emit").returncode, 0)  # control

        (self.fx.build / "patch_state.json").unlink()
        (self.fx.build / "obj" / "t0.obj").unlink()                       # sabotage
        red = self.fx.run("--check", "--emit")
        self.assertNotEqual(red.returncode, 0)
        self.assertIn("PAIRING IS INCOMPLETE", red.stderr)
        self.assertIn("UNEXAMINED", red.stderr)
        self.assertFalse((self.fx.build / "patch_state.json").exists(),
                         "a manifest was written over an unverifiable pairing")

    def test_the_real_min_declared_floor_is_armed(self):
        """The rest of this suite passes --min-declared 1; prove that is a
        fixture accommodation and not a disarmed check.

        Without this control, someone could set DEFAULT_MIN_DECLARED to 0 and
        every test here would still be green.
        """
        self.assertEqual(self.fx.run("--check").returncode, 0)   # with the flag
        default = self.fx.run("--check", min_declared=None)      # without it
        self.assertNotEqual(default.returncode, 0,
                            "the default vacuity floor accepts a 3-object "
                            "tree -- it is not a floor")
        self.assertIn("vacuous", default.stderr)


class PatchGuardTests(unittest.TestCase):
    """scripts/orchestrator/patch_guard.py must RAISE, not return a number."""

    def setUp(self):
        sys.path.insert(0, str(REPO / "scripts" / "orchestrator"))
        import patch_guard
        self.pg = patch_guard
        self._tmp = tempfile.TemporaryDirectory(dir=os.path.expanduser("~/tmp"))
        self.fx = Fixture(Path(self._tmp.name))

    def tearDown(self):
        self._tmp.cleanup()

    def test_absent_verifier_refuses_rather_than_assuming_clean(self):
        (self.fx.scripts / "verify_objs_patched.py").unlink()
        with self.assertRaises(self.pg.UnpatchedTreeError) as cm:
            self.pg.ensure_patched_tree(self.fx.root, build=False)
        self.assertIn("cannot be established", str(cm.exception))

    def test_drifted_tree_refuses_with_the_object_named(self):
        env = dict(os.environ, RB3_VERSION=BUILD_ID,
                   PATCH_STUB_LOG=str(self.fx.log))
        subprocess.run([sys.executable,
                        str(self.fx.scripts / "verify_objs_patched.py"),
                        "--repo", str(self.fx.root), "--check", "--emit",
                        "--min-declared", "1"],
                       check=True, capture_output=True, env=env,
                       cwd=str(self.fx.root))
        # CONTROL: clean tree returns a note and does NOT raise.
        note = self.pg.ensure_patched_tree(self.fx.root, build=False)
        self.assertIn("verified", note)

        self.fx.decomp[0].write_bytes(b"RAW")
        with self.assertRaises(self.pg.UnpatchedTreeError) as cm:
            self.pg.ensure_patched_tree(self.fx.root, build=False)
        # Assert on evidence: "it raised" is also true if the module raised
        # for an unrelated reason (missing file, bad path, timeout).
        self.assertIn("d0.obj", str(cm.exception))
        self.assertIn("REFUSING TO MEASURE", str(cm.exception))

    def test_reads_custom_make_from_objdiff_json(self):
        """rb3-xenon's custom_make is tools/ninja-locked, not bare ninja.

        Bare ninja here races the SPLIT->configure regeneration loop that the
        wrapper's flock exists to prevent. A guard that hardcoded "ninja" would
        pass every other test in this file.
        """
        cfg = json.loads((self.fx.root / "objdiff.json").read_text())
        cfg["custom_make"] = "tools/ninja-locked"
        cfg["custom_args"] = ["-k", "0"]
        (self.fx.root / "objdiff.json").write_text(json.dumps(cfg))
        self.assertEqual(self.pg._make_command(self.fx.root),
                         ["tools/ninja-locked", "-k", "0"])
        # CONTROL: with no custom_make declared it must fall back to ninja,
        # so the assertion above is reading the config and not a constant.
        cfg.pop("custom_make")
        cfg.pop("custom_args")
        (self.fx.root / "objdiff.json").write_text(json.dumps(cfg))
        self.assertEqual(self.pg._make_command(self.fx.root), ["ninja"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
