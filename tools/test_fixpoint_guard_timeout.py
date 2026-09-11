#!/usr/bin/env python3
"""A TIMEOUT in symbols_fixpoint_guard must never be reportable as DRIFT.

The guard's exit codes carry verdicts:
    0 = PASS (symbols.txt is at jeff's split fixpoint)
    1 = DRIFT (the split rewrote its input; here is the re-carve)
    2 = REFUSAL / no verdict reached

Before this test, `subprocess.TimeoutExpired` escaped the module's `run()`
helper as an uncaught exception, and Python exits **1** on an uncaught
exception -- indistinguishable from the DRIFT verdict. On a machine where
several lanes share one build lock, the likely cause of a timeout is
contention, so the failure mode was "somebody else was building" rendered as
a confident claim that symbols.txt had drifted.

Both arms below are required. The NEGATIVE arm is the one that gives the
positive arm its meaning: if a non-timeout command also came back as a
refusal, the test would pass while proving nothing.
"""
import subprocess
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import importlib.util

_SRC = Path(__file__).resolve().parent / "symbols_fixpoint_guard.py"
_spec = importlib.util.spec_from_file_location("_sfg", _SRC)
_sfg = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_sfg)


class TimeoutIsNotDrift(unittest.TestCase):
    def test_timeout_raises_Fail_not_a_bare_exception(self):
        """The positive arm: a command that outlives its timeout refuses."""
        with self.assertRaises(_sfg.Fail) as caught:
            _sfg.run([sys.executable, "-c", "import time; time.sleep(30)"],
                     Path.cwd(), timeout=1)
        msg = str(caught.exception)
        self.assertIn("TIMEOUT", msg)
        self.assertIn("NOT symbols.txt drift", msg)

    def test_timeout_does_NOT_surface_as_TimeoutExpired(self):
        """TimeoutExpired escaping is the exact defect; Python exits 1 on it."""
        try:
            _sfg.run([sys.executable, "-c", "import time; time.sleep(30)"],
                     Path.cwd(), timeout=1)
        except _sfg.Fail:
            pass
        except subprocess.TimeoutExpired:
            self.fail("TimeoutExpired escaped run() -- an uncaught exception "
                      "exits 1, which is this guard's DRIFT verdict")

    def test_negative_arm_a_normal_command_is_NOT_a_refusal(self):
        """Without this, the positive arm would pass on a run() that always
        refused, proving nothing about timeouts specifically."""
        rc, out = _sfg.run([sys.executable, "-c", "print('hello')"],
                           Path.cwd(), timeout=60)
        self.assertEqual(rc, 0)
        self.assertIn("hello", out)

    def test_negative_arm_a_failing_command_reports_rc_not_a_refusal(self):
        """A non-zero exit is data the caller adjudicates, not a refusal."""
        rc, _ = _sfg.run([sys.executable, "-c", "raise SystemExit(3)"],
                         Path.cwd(), timeout=60)
        self.assertEqual(rc, 3)


if __name__ == "__main__":
    unittest.main(verbosity=2)
