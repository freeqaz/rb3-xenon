"""Tests for prober.py — multi-input probing logic.

These are pure-logic tests over mocks, but ``prober`` reaches ``run`` for
``ComparisonBundle`` and ``run`` imports the emulator at module scope, so the
module cannot even be COLLECTED without the Unicorn bindings. Skip at module
level rather than letting that surface as a collection error — an absent
optional dependency is a skip, not a red.

Force the skip path with ``UNICORN_DIR=/nonexistent`` to verify this guard is
live; see scripts/unicorn_runner/unicorn_dep.py.
"""

import unittest
from unittest.mock import patch, MagicMock

from scripts.unicorn_runner.unicorn_dep import HAS_UNICORN, SKIP_REASON

if not HAS_UNICORN:  # pragma: no cover - depends on the box
    raise unittest.SkipTest(SKIP_REASON)

from .helpers import MockExecutionResult

from scripts.unicorn_runner.comparator import ComparisonResult
from scripts.unicorn_runner.prober import ProbeResult, probe_function, format_probe_result
from scripts.unicorn_runner.run import ComparisonBundle, EXIT_EQUIVALENT, EXIT_DIVERGENT, EXIT_SKIPPED


class TestProbeResult(unittest.TestCase):
    """Tests for ProbeResult dataclass."""

    def test_confidence_none_when_empty(self):
        p = ProbeResult()
        self.assertEqual(p.confidence, "none")

    def test_confidence_high_when_all_equiv(self):
        p = ProbeResult(total_runs=4, equiv_runs=4, stable_equiv=True)
        self.assertEqual(p.confidence, "high")

    def test_confidence_stable_divergent(self):
        p = ProbeResult(total_runs=4, divergent_runs=4, stable_divergent=True)
        self.assertEqual(p.confidence, "stable_divergent")

    def test_confidence_input_sensitive(self):
        p = ProbeResult(total_runs=4, equiv_runs=2, divergent_runs=2, input_sensitive=True)
        self.assertEqual(p.confidence, "input_sensitive")


class TestProbeFunction(unittest.TestCase):
    """Tests for probe_function() with mocked _run_comparison_core."""

    def _make_equiv_bundle(self):
        result = ComparisonResult("EQUIVALENT", {"call_count": 0, "r3": 0, "f1": 0})
        return ComparisonBundle(
            result=result,
            decomp_result=MockExecutionResult(),
            orig_result=MockExecutionResult(),
            decomp_relocs=[],
            orig_relocs=[],
        )

    def _make_div_bundle(self, reason="call_arg_mismatch"):
        result = ComparisonResult("DIVERGENT", {
            "reason": reason,
            "call_index": 0,
            "register": "r3",
            "decomp_val": 1,
            "orig_val": 2,
            "decomp_args": {"r3": 1, "r4": 0, "r5": 0, "r6": 0},
            "orig_args": {"r3": 2, "r4": 0, "r5": 0, "r6": 0},
        })
        return ComparisonBundle(
            result=result,
            decomp_result=MockExecutionResult(call_log=[(0, 0, 0, 1, 0, 0, 0)]),
            orig_result=MockExecutionResult(call_log=[(0, 0, 0, 2, 0, 0, 0)]),
            decomp_relocs=[],
            orig_relocs=[],
        )

    @patch("scripts.unicorn_runner.prober._run_comparison_core")
    def test_all_equivalent(self, mock_core):
        bundle = self._make_equiv_bundle()
        mock_core.return_value = (EXIT_EQUIVALENT, bundle, [], None)

        probe = probe_function("TestSym", None, None, runs=4, seed=42)
        self.assertIsNotNone(probe)
        self.assertEqual(probe.total_runs, 4)
        self.assertEqual(probe.equiv_runs, 4)
        self.assertEqual(probe.divergent_runs, 0)
        self.assertTrue(probe.stable_equiv)
        self.assertFalse(probe.stable_divergent)
        self.assertFalse(probe.input_sensitive)
        self.assertEqual(probe.confidence, "high")

    @patch("scripts.unicorn_runner.prober._run_comparison_core")
    def test_all_divergent(self, mock_core):
        bundle = self._make_div_bundle()
        mock_core.return_value = (EXIT_DIVERGENT, bundle, [], None)

        probe = probe_function("TestSym", None, None, runs=4, seed=42)
        self.assertIsNotNone(probe)
        self.assertEqual(probe.equiv_runs, 0)
        self.assertEqual(probe.divergent_runs, 4)
        self.assertTrue(probe.stable_divergent)
        self.assertEqual(probe.confidence, "stable_divergent")
        # Small non-pointer arg diff → classified as regalloc
        self.assertIn("regalloc", probe.divergence_classes)

    @patch("scripts.unicorn_runner.prober._run_comparison_core")
    def test_mixed_results(self, mock_core):
        equiv_bundle = self._make_equiv_bundle()
        div_bundle = self._make_div_bundle()
        # Alternate between equiv and div
        mock_core.side_effect = [
            (EXIT_EQUIVALENT, equiv_bundle, [], None),
            (EXIT_DIVERGENT, div_bundle, [], None),
            (EXIT_EQUIVALENT, equiv_bundle, [], None),
            (EXIT_DIVERGENT, div_bundle, [], None),
        ]

        probe = probe_function("TestSym", None, None, runs=4, seed=42)
        self.assertEqual(probe.equiv_runs, 2)
        self.assertEqual(probe.divergent_runs, 2)
        self.assertTrue(probe.input_sensitive)
        self.assertEqual(probe.confidence, "input_sensitive")

    @patch("scripts.unicorn_runner.prober._run_comparison_core")
    def test_skipped_returns_none(self, mock_core):
        mock_core.return_value = (EXIT_SKIPPED, None, [], "SKIPPED")

        probe = probe_function("TestSym", None, None, runs=4, seed=42)
        self.assertIsNone(probe)

    @patch("scripts.unicorn_runner.prober._run_comparison_core")
    def test_per_run_details(self, mock_core):
        bundle = self._make_equiv_bundle()
        mock_core.return_value = (EXIT_EQUIVALENT, bundle, [], None)

        probe = probe_function("TestSym", None, None, runs=3, seed=42)
        self.assertEqual(len(probe.per_run), 3)
        # First run should be zero fill, second 0xCD
        self.assertIsNone(probe.per_run[0].fill_pattern)
        self.assertEqual(probe.per_run[1].fill_pattern, 0xCD)


class TestFormatProbeResult(unittest.TestCase):
    """Tests for format_probe_result()."""

    def test_format_stable_equiv(self):
        p = ProbeResult(total_runs=4, equiv_runs=4, stable_equiv=True)
        output = format_probe_result(p, symbol="TestFunc")
        self.assertIn("TestFunc", output)
        self.assertIn("4 equiv", output)
        self.assertIn("high", output)

    def test_format_with_classes(self):
        p = ProbeResult(total_runs=4, divergent_runs=4, stable_divergent=True,
                        divergence_classes={"build_env": 3, "logic": 1})
        output = format_probe_result(p)
        self.assertIn("build_env: 3", output)
        self.assertIn("logic: 1", output)

    def test_format_input_sensitive(self):
        from scripts.unicorn_runner.prober import RunDetail
        p = ProbeResult(
            total_runs=4, equiv_runs=2, divergent_runs=2,
            input_sensitive=True,
            per_run=[
                RunDetail(fill_pattern=None, exit_code=EXIT_EQUIVALENT),
                RunDetail(fill_pattern=0xCD, exit_code=EXIT_DIVERGENT),
                RunDetail(fill_pattern=0x55, exit_code=EXIT_EQUIVALENT),
                RunDetail(fill_pattern=0xAA, exit_code=EXIT_DIVERGENT),
            ])
        output = format_probe_result(p)
        self.assertIn("input_sensitive", output)
        self.assertIn("Equiv fills:", output)
        self.assertIn("Div fills:", output)


if __name__ == "__main__":
    unittest.main()
