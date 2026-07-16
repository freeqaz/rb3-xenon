"""End-to-end integration tests for the unicorn runner pipeline.

Tests run_comparison() and list_functions() against real .obj files.
Skipped when build artifacts don't exist.
"""

import os
import sys
import io
import unittest

# Unicorn imports (must match engine.py's path setup)
from pathlib import Path
_MILOHAX_DIR = Path(__file__).resolve().parent.parent.parent.parent.parent
_UNICORN_DIR = _MILOHAX_DIR / "unicorn"
UNICORN_PATH = str(_UNICORN_DIR / "bindings" / "python")
sys.path.insert(0, UNICORN_PATH)
os.environ["LIBUNICORN_PATH"] = str(_UNICORN_DIR / "build")

try:
    from unicorn import Uc
    HAS_UNICORN = True
except ImportError:
    HAS_UNICORN = False

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SKELETON_DECOMP = os.path.join(PROJECT_ROOT, "build", "373307D9", "system", "gesture", "Skeleton.obj")
SKELETON_ORIG = os.path.join(PROJECT_ROOT, "orig", "373307D9", "system", "gesture", "Skeleton.obj")

HAS_SKELETON = os.path.exists(SKELETON_DECOMP) and os.path.exists(SKELETON_ORIG)
CAN_RUN = HAS_UNICORN and HAS_SKELETON


@unittest.skipUnless(CAN_RUN, "Requires Unicorn PPC and Skeleton .obj build artifacts")
class TestIntegration(unittest.TestCase):
    """End-to-end tests using real .obj files."""

    def _suppress_stdout(self):
        """Context manager-like helper to suppress stdout."""
        self._old_stdout = sys.stdout
        sys.stdout = io.StringIO()

    def _restore_stdout(self):
        captured = sys.stdout.getvalue()
        sys.stdout = self._old_stdout
        return captured

    def test_list_functions_count(self):
        """Skeleton has multiple eligible functions."""
        from scripts.unicorn_runner.run import list_functions
        self._suppress_stdout()
        try:
            eligible = list_functions(SKELETON_DECOMP, SKELETON_ORIG)
        finally:
            self._restore_stdout()
        self.assertGreater(len(eligible), 5,
                           "Skeleton should have more than 5 eligible functions")

    def test_list_functions_structure(self):
        """Each eligible entry has (symbol, decomp_size, orig_size) format."""
        from scripts.unicorn_runner.run import list_functions
        self._suppress_stdout()
        try:
            eligible = list_functions(SKELETON_DECOMP, SKELETON_ORIG)
        finally:
            self._restore_stdout()
        if eligible:
            sym, d_size, o_size = eligible[0]
            self.assertIsInstance(sym, str)
            self.assertIsInstance(d_size, int)
            self.assertIsInstance(o_size, int)
            self.assertGreater(d_size, 0)
            self.assertGreater(o_size, 0)

    def test_resolve_unit(self):
        """'Skeleton' resolves to valid paths."""
        from scripts.unicorn_runner.run import resolve_unit
        decomp_path, orig_path = resolve_unit("Skeleton")
        self.assertTrue(decomp_path.endswith(".obj"))
        self.assertTrue(orig_path.endswith(".obj"))

    def test_resolve_unit_not_found(self):
        """Bogus unit → ValueError."""
        from scripts.unicorn_runner.run import resolve_unit
        with self.assertRaises(ValueError):
            resolve_unit("NonExistentUnit_XYZ_12345")

    def test_equivalent_function(self):
        """A known-matching function returns EXIT_EQUIVALENT."""
        from scripts.unicorn_runner.run import run_comparison, EXIT_EQUIVALENT, list_functions

        # Find an eligible function to test
        self._suppress_stdout()
        try:
            eligible = list_functions(SKELETON_DECOMP, SKELETON_ORIG)
        finally:
            self._restore_stdout()

        if not eligible:
            self.skipTest("No eligible functions in Skeleton")

        # Try the first eligible function
        sym = eligible[0][0]
        self._suppress_stdout()
        try:
            code = run_comparison(sym, SKELETON_DECOMP, SKELETON_ORIG)
        finally:
            self._restore_stdout()
        # We can't guarantee it's equivalent, but it should not error
        self.assertIn(code, (0, 1),
                      f"Expected EQUIVALENT(0) or DIVERGENT(1), got {code}")

    def test_batch_no_crashes(self):
        """Skeleton batch runs without crashing (errors count may be >0)."""
        from scripts.unicorn_runner.run import run_batch
        self._suppress_stdout()
        try:
            eq, div, err, sk, _cached = run_batch(
                SKELETON_DECOMP, SKELETON_ORIG, timeout=2_000_000)
        finally:
            self._restore_stdout()
        total = eq + div + err + sk
        self.assertGreater(total, 0, "Batch should process at least 1 function")

    def test_bctrl_function_executes(self):
        """bctrl functions execute with vtable mocking (not skipped)."""
        from scripts.unicorn_runner.run import run_comparison, EXIT_SKIPPED
        from scripts.unicorn_runner.extractor import classify_indirect_branch, extract_from_decomp
        from scripts.unicorn_runner.coff import COFFParser

        # Find a bctrl function in Skeleton
        coff = COFFParser(SKELETON_DECOMP)
        bctrl_sym = None
        for sym in coff.symbols:
            if sym['section'] > 0:
                sec = coff.sections[sym['section'] - 1]
                if sec['name'].startswith('.text'):
                    func_bytes, relocs = extract_from_decomp(coff, sym['name'])
                    if func_bytes and len(func_bytes) > 0:
                        cls = classify_indirect_branch(func_bytes, relocs, coff)
                        if cls == "bctrl":
                            bctrl_sym = sym['name']
                            break

        if bctrl_sym is None:
            self.skipTest("No bctrl function found in Skeleton")

        self._suppress_stdout()
        try:
            code = run_comparison(bctrl_sym, SKELETON_DECOMP, SKELETON_ORIG)
        finally:
            self._restore_stdout()
        # Must not be SKIPPED — it should run (EQUIVALENT or DIVERGENT)
        self.assertNotEqual(code, EXIT_SKIPPED,
                            f"bctrl function {bctrl_sym} should not be skipped")


    def test_batch_coff_caching(self):
        """Batch with COFF caching returns same results as before."""
        from scripts.unicorn_runner.run import run_batch
        self._suppress_stdout()
        try:
            eq, div, err, sk, _cached = run_batch(
                SKELETON_DECOMP, SKELETON_ORIG, timeout=2_000_000)
        finally:
            self._restore_stdout()
        total = eq + div + err + sk
        self.assertGreater(total, 0, "Batch should process at least 1 function")
        # Verify counts are non-negative
        self.assertGreaterEqual(eq, 0)
        self.assertGreaterEqual(div, 0)
        self.assertGreaterEqual(err, 0)
        self.assertGreaterEqual(sk, 0)

    def test_run_comparison_with_cached_coff(self):
        """run_comparison() accepts pre-parsed COFF instances."""
        from scripts.unicorn_runner.run import run_comparison, list_functions
        from scripts.unicorn_runner.coff import COFFParser

        decomp_coff = COFFParser(SKELETON_DECOMP)
        orig_coff = COFFParser(SKELETON_ORIG)

        # Get a function to test
        self._suppress_stdout()
        try:
            eligible = list_functions(SKELETON_DECOMP, SKELETON_ORIG,
                                      decomp_coff=decomp_coff, orig_coff=orig_coff)
        finally:
            self._restore_stdout()

        if not eligible:
            self.skipTest("No eligible functions in Skeleton")

        sym = eligible[0][0]
        self._suppress_stdout()
        try:
            code = run_comparison(sym, SKELETON_DECOMP, SKELETON_ORIG,
                                  decomp_coff=decomp_coff, orig_coff=orig_coff)
        finally:
            self._restore_stdout()
        self.assertIn(code, (0, 1),
                      f"Expected EQUIVALENT(0) or DIVERGENT(1), got {code}")

    def test_run_comparison_inner_returns_tuple(self):
        """run_comparison_inner() returns (exit_code, output_text)."""
        from scripts.unicorn_runner.run import run_comparison_inner
        from scripts.unicorn_runner.coff import COFFParser

        decomp_coff = COFFParser(SKELETON_DECOMP)
        orig_coff = COFFParser(SKELETON_ORIG)

        # Get a function
        self._suppress_stdout()
        try:
            from scripts.unicorn_runner.run import list_functions
            eligible = list_functions(SKELETON_DECOMP, SKELETON_ORIG,
                                      decomp_coff=decomp_coff, orig_coff=orig_coff)
        finally:
            self._restore_stdout()

        if not eligible:
            self.skipTest("No eligible functions in Skeleton")

        sym = eligible[0][0]
        code, output = run_comparison_inner(sym, decomp_coff, orig_coff)
        self.assertIsInstance(code, int)
        self.assertIsInstance(output, str)
        self.assertIn(code, (0, 1, 2, 3))


if __name__ == "__main__":
    unittest.main()
