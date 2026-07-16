"""Tests for comparator.py — comparison logic with mock ExecutionResults."""

import json
import struct
import unittest

from .helpers import MockExecutionResult, make_call_log_entry, make_reloc


class TestCompare(unittest.TestCase):
    """Tests for compare()."""

    def setUp(self):
        from scripts.unicorn_runner.comparator import compare
        self.compare = compare

    def _make_pair(self, **overrides):
        """Make identical decomp/orig results, with optional overrides for decomp."""
        base = MockExecutionResult(r3=0, f1=0, call_log=[], error=None)
        decomp = MockExecutionResult(**{**{"r3": 0, "f1": 0, "call_log": [], "error": None}, **overrides})
        return decomp, base

    def test_equivalent_basic(self):
        decomp = MockExecutionResult(r3=100, f1=0)
        orig = MockExecutionResult(r3=100, f1=0)
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "EQUIVALENT")

    def test_equivalent_with_calls(self):
        log = [make_call_log_entry(0, r3=1, r4=2)]
        decomp = MockExecutionResult(r3=0, call_log=log)
        orig = MockExecutionResult(r3=0, call_log=list(log))
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "EQUIVALENT")
        self.assertEqual(result.details["call_count"], 1)

    def test_equivalent_f1_in_details(self):
        decomp = MockExecutionResult(r3=0, f1=0xDEADBEEF)
        orig = MockExecutionResult(r3=0, f1=0xDEADBEEF)
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "EQUIVALENT")
        self.assertEqual(result.details["f1"], 0xDEADBEEF)

    def test_divergent_r3_mismatch(self):
        decomp = MockExecutionResult(r3=42)
        orig = MockExecutionResult(r3=99)
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "DIVERGENT")
        self.assertEqual(result.details["reason"], "return_value_mismatch")
        self.assertEqual(result.details["decomp_r3"], 42)
        self.assertEqual(result.details["orig_r3"], 99)

    def test_divergent_f1_mismatch(self):
        decomp = MockExecutionResult(r3=0, f1=0x3FF0000000000000)
        orig = MockExecutionResult(r3=0, f1=0x4000000000000000)
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "DIVERGENT")
        self.assertEqual(result.details["reason"], "fpr_return_mismatch")

    def test_divergent_call_count(self):
        decomp = MockExecutionResult(call_log=[make_call_log_entry(0)])
        orig = MockExecutionResult(call_log=[make_call_log_entry(0), make_call_log_entry(1)])
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "DIVERGENT")
        self.assertEqual(result.details["reason"], "call_count_mismatch")

    def test_divergent_call_count_includes_matched_prefix(self):
        """call_count_mismatch details include matched_prefix count."""
        d_log = [make_call_log_entry(0, r3=1), make_call_log_entry(1, r3=2),
                 make_call_log_entry(2, r3=3)]
        o_log = [make_call_log_entry(0, r3=1), make_call_log_entry(1, r3=2)]
        decomp = MockExecutionResult(call_log=d_log)
        orig = MockExecutionResult(call_log=o_log)
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.details["reason"], "call_count_mismatch")
        self.assertEqual(result.details["matched_prefix"], 2)

    def test_divergent_call_count_with_arg_diff(self):
        """call_count_mismatch where args also diverge before count runs out."""
        d_log = [make_call_log_entry(0, r3=1), make_call_log_entry(1, r3=99),
                 make_call_log_entry(2, r3=3)]
        o_log = [make_call_log_entry(0, r3=1), make_call_log_entry(1, r3=2)]
        decomp = MockExecutionResult(call_log=d_log)
        orig = MockExecutionResult(call_log=o_log)
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.details["reason"], "call_count_mismatch")
        self.assertEqual(result.details["matched_prefix"], 1)

    def test_divergent_call_args(self):
        d_log = [make_call_log_entry(0, r3=1, r4=10), make_call_log_entry(1, r3=2, r4=99)]
        o_log = [make_call_log_entry(0, r3=1, r4=10), make_call_log_entry(1, r3=2, r4=50)]
        decomp = MockExecutionResult(call_log=d_log)
        orig = MockExecutionResult(call_log=o_log)
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "DIVERGENT")
        self.assertEqual(result.details["reason"], "call_arg_mismatch")
        self.assertEqual(result.details["call_index"], 1)
        self.assertEqual(result.details["register"], "r4")

    def test_call_arg_mismatch_includes_full_args(self):
        """call_arg_mismatch details include full decomp_args and orig_args."""
        d_log = [make_call_log_entry(0, r3=1, r4=99, r5=3, r6=4)]
        o_log = [make_call_log_entry(0, r3=1, r4=50, r5=3, r6=4)]
        decomp = MockExecutionResult(call_log=d_log)
        orig = MockExecutionResult(call_log=o_log)
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.details["decomp_args"], {"r3": 1, "r4": 99, "r5": 3, "r6": 4})
        self.assertEqual(result.details["orig_args"], {"r3": 1, "r4": 50, "r5": 3, "r6": 4})

    def test_divergent_memory(self):
        # Write different values at offset 0 in object memory
        decomp_mem = bytearray(0x10000)
        orig_mem = bytearray(0x10000)
        struct.pack_into(">I", decomp_mem, 0, 0xAAAAAAAA)
        struct.pack_into(">I", orig_mem, 0, 0xBBBBBBBB)

        decomp = MockExecutionResult(r3=0, object_memory=bytes(decomp_mem))
        orig = MockExecutionResult(r3=0, object_memory=bytes(orig_mem))
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "DIVERGENT")
        self.assertEqual(result.details["reason"], "memory_mismatch")
        self.assertTrue(len(result.details["object_diffs"]) > 0)

    def test_decomp_error(self):
        decomp = MockExecutionResult(error="Unexpected fetch from unmapped 0x00000000")
        orig = MockExecutionResult()
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "DIVERGENT")
        self.assertEqual(result.details["reason"], "decomp_error")

    def test_orig_error(self):
        decomp = MockExecutionResult()
        orig = MockExecutionResult(error="timeout")
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "DIVERGENT")
        self.assertEqual(result.details["reason"], "orig_error")

    def test_matching_errors_same_pc_equivalent(self):
        """Phase 2.1 (softened): matching errors AT MATCHING PC → EQUIVALENT.

        Under zero-fill fixtures both sides commonly null-deref to the
        same address — we don't flag that as divergent, but we DO tag
        the verdict so Phase 3 (unmapped fingerprint) can distinguish
        symmetric-null-deref from real matching faults.
        """
        err = "Unexpected fetch from unmapped 0x00000000"
        decomp = MockExecutionResult(error=err, final_pc=0x4)
        orig = MockExecutionResult(error=err, final_pc=0x4)
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "EQUIVALENT")
        self.assertEqual(result.details["matching_error"], err)
        self.assertEqual(result.details["matching_error_pc"], 0x4)
        self.assertTrue(len(result.warnings) > 0)

    def test_matching_errors_different_pc_wild_jump(self):
        """Phase 2.1: same error string but different PCs → DIVERGENT (wild_jump_match)."""
        err = "Unexpected fetch from unmapped 0x00000000"
        decomp = MockExecutionResult(error=err, final_pc=0x4)
        orig = MockExecutionResult(error=err, final_pc=0x10)
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "DIVERGENT")
        self.assertEqual(result.details["reason"], "wild_jump_match")

    def test_mismatched_errors_divergent(self):
        """Both sides error but with different messages → DIVERGENT."""
        decomp = MockExecutionResult(error="Unexpected fetch from unmapped 0x00000000")
        orig = MockExecutionResult(error="Unexpected fetch from unmapped 0xDEADBEEF")
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "DIVERGENT")
        self.assertEqual(result.details["reason"], "error_mismatch")

    def test_both_cap_exhausted_divergent(self):
        """Phase 2.2: both sides hitting the insn cap → DIVERGENT (cap_exhausted)."""
        decomp = MockExecutionResult(cap_exhausted=True, final_pc=0x80001234)
        orig = MockExecutionResult(cap_exhausted=True, final_pc=0x80005678)
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "DIVERGENT")
        self.assertEqual(result.details["reason"], "cap_exhausted_both")

    def test_one_sided_cap_exhausted_divergent(self):
        """Phase 2.2: one-sided cap exhaustion → DIVERGENT (cap_exhausted_decomp)."""
        decomp = MockExecutionResult(cap_exhausted=True, final_pc=0x80001234)
        orig = MockExecutionResult()  # terminated_normally=True by default
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "DIVERGENT")
        self.assertEqual(result.details["reason"], "cap_exhausted_decomp")

    def test_unmapped_fingerprint_mismatch_divergent(self):
        """Phase 3.2: decomp pokes null page, orig doesn't → DIVERGENT."""
        decomp = MockExecutionResult()
        decomp.unmapped_log = [("rd", 0x00000000, 0x00000004)]
        orig = MockExecutionResult()
        orig.unmapped_log = []
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "DIVERGENT")
        self.assertEqual(result.details["reason"], "unmapped_access_mismatch")

    def test_unmapped_fingerprint_match_equivalent(self):
        """Phase 3.2: both sides touch the same null page → EQUIVALENT (fingerprint matches)."""
        decomp = MockExecutionResult()
        decomp.unmapped_log = [("rd", 0x00000000, 0x00000004)]
        orig = MockExecutionResult()
        orig.unmapped_log = [("rd", 0x00000000, 0x00000010)]  # different addr, same page
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "EQUIVALENT")
        # Fingerprint should be present and non-empty
        self.assertNotEqual(result.details["unmapped_fingerprint"], "")


class TestFormatResult(unittest.TestCase):
    """Tests for format_result()."""

    def setUp(self):
        from scripts.unicorn_runner.comparator import compare, format_result
        self.compare = compare
        self.format_result = format_result

    def test_format_equivalent(self):
        decomp = MockExecutionResult(r3=42, f1=0)
        orig = MockExecutionResult(r3=42, f1=0)
        result = self.compare(decomp, orig, [], [])
        output = self.format_result(result, decomp, orig, [], [])
        self.assertIn("EQUIVALENT", output)
        self.assertIn("0x0000002A", output)  # r3=42

    def test_format_divergent_fpr(self):
        decomp = MockExecutionResult(r3=0, f1=0x3FF0000000000000)
        orig = MockExecutionResult(r3=0, f1=0x4000000000000000)
        result = self.compare(decomp, orig, [], [])
        output = self.format_result(result, decomp, orig, [], [])
        self.assertIn("DIVERGENT", output)
        self.assertIn("Float return value mismatch", output)
        self.assertIn("3FF0000000000000", output)
        self.assertIn("4000000000000000", output)

    def test_format_call_arg_mismatch_shows_all_regs(self):
        """call_arg_mismatch output shows all 4 registers and 'Differs' line."""
        d_log = [make_call_log_entry(0, r3=1, r4=99, r5=3, r6=4, source_offset=0x2C)]
        o_log = [make_call_log_entry(0, r3=1, r4=50, r5=3, r6=4, source_offset=0x2C)]
        orig_relocs = [make_reloc(0x2C, "Foo::Bar", "REL24")]
        decomp = MockExecutionResult(call_log=d_log)
        orig = MockExecutionResult(call_log=o_log)
        result = self.compare(decomp, orig, [], orig_relocs)
        output = self.format_result(result, decomp, orig, [], orig_relocs)
        self.assertIn("Foo::Bar", output)
        self.assertIn("offset 0x2C", output)
        self.assertIn("Decomp:", output)
        self.assertIn("Original:", output)
        self.assertIn("Differs: r4", output)

    def test_format_call_count_mismatch_shows_detail(self):
        """call_count_mismatch output shows matched calls and extra calls."""
        d_log = [make_call_log_entry(0, r3=1, source_offset=0x10),
                 make_call_log_entry(1, r3=2, source_offset=0x20),
                 make_call_log_entry(2, r3=3, source_offset=0x30)]
        o_log = [make_call_log_entry(0, r3=1, source_offset=0x10),
                 make_call_log_entry(1, r3=2, source_offset=0x20)]
        orig_relocs = [make_reloc(0x10, "Func::A", "REL24"),
                       make_reloc(0x20, "Func::B", "REL24")]
        decomp_relocs = [make_reloc(0x10, "Func::A", "REL24"),
                         make_reloc(0x20, "Func::B", "REL24"),
                         make_reloc(0x30, "Func::C", "REL24")]
        decomp = MockExecutionResult(call_log=d_log)
        orig = MockExecutionResult(call_log=o_log)
        result = self.compare(decomp, orig, decomp_relocs, orig_relocs)
        output = self.format_result(result, decomp, orig, decomp_relocs, orig_relocs)
        self.assertIn("Call count mismatch: decomp=3, orig=2", output)
        self.assertIn("Matched calls before divergence", output)
        self.assertIn("(both match)", output)
        self.assertIn("Extra decomp calls", output)


class TestToDict(unittest.TestCase):
    """Tests for ComparisonResult.to_dict()."""

    def test_to_dict_equivalent(self):
        from scripts.unicorn_runner.comparator import ComparisonResult
        result = ComparisonResult("EQUIVALENT", {"call_count": 5, "r3": 42}, ["warning1"])
        d = result.to_dict()
        self.assertEqual(d["verdict"], "EQUIVALENT")
        self.assertEqual(d["details"]["call_count"], 5)
        self.assertEqual(d["warnings"], ["warning1"])

    def test_to_dict_divergent(self):
        from scripts.unicorn_runner.comparator import ComparisonResult
        result = ComparisonResult("DIVERGENT", {"reason": "call_arg_mismatch"})
        d = result.to_dict()
        self.assertEqual(d["verdict"], "DIVERGENT")
        self.assertEqual(d["details"]["reason"], "call_arg_mismatch")
        self.assertEqual(d["warnings"], [])


class TestClassifyDivergence(unittest.TestCase):
    """Tests for classify_divergence()."""

    def setUp(self):
        from scripts.unicorn_runner.comparator import compare, classify_divergence
        from scripts.unicorn_runner.memory_map import GLOBAL_BASE, OBJECT_BASE
        self.compare = compare
        self.classify = classify_divergence
        self.GLOBAL_BASE = GLOBAL_BASE
        self.OBJECT_BASE = OBJECT_BASE

    def test_equivalent_returns_none(self):
        decomp = MockExecutionResult(r3=0)
        orig = MockExecutionResult(r3=0)
        result = self.compare(decomp, orig, [], [])
        self.assertIsNone(self.classify(result, decomp, orig, [], []))

    def test_build_env_globals_arg_mismatch(self):
        """Args pointing to globals region = likely __FILE__ string diff."""
        d_log = [make_call_log_entry(0, r4=self.GLOBAL_BASE + 0x100)]
        o_log = [make_call_log_entry(0, r4=self.GLOBAL_BASE + 0x200)]
        decomp = MockExecutionResult(call_log=d_log)
        orig = MockExecutionResult(call_log=o_log)
        result = self.compare(decomp, orig, [], [])
        self.assertEqual(result.verdict, "DIVERGENT")
        cls = self.classify(result, decomp, orig, [], [])
        self.assertEqual(cls, "build_env")

    def test_build_env_merged_symbol_warning(self):
        """Merged symbol in warning at same call index = build_env."""
        d_log = [make_call_log_entry(0, r3=1, source_offset=0x10)]
        o_log = [make_call_log_entry(0, r3=2, source_offset=0x10)]
        decomp = MockExecutionResult(call_log=d_log)
        orig = MockExecutionResult(call_log=o_log)
        # Manually build result with merged_ warning
        from scripts.unicorn_runner.comparator import ComparisonResult
        result = ComparisonResult("DIVERGENT", {
            "reason": "call_arg_mismatch",
            "call_index": 0,
            "register": "r3",
            "decomp_val": 1,
            "orig_val": 2,
            "decomp_args": {"r3": 1, "r4": 0, "r5": 0, "r6": 0},
            "orig_args": {"r3": 2, "r4": 0, "r5": 0, "r6": 0},
        }, warnings=["Call #0: decomp targets merged_82004C00, original targets RealFunc"])
        cls = self.classify(result, decomp, orig, [], [])
        self.assertEqual(cls, "build_env")

    def test_build_env_call_count_merged(self):
        """Call count mismatch with merged symbol warning = build_env."""
        d_log = [make_call_log_entry(0)]
        o_log = [make_call_log_entry(0), make_call_log_entry(1)]
        decomp = MockExecutionResult(call_log=d_log)
        orig = MockExecutionResult(call_log=o_log)
        from scripts.unicorn_runner.comparator import ComparisonResult
        result = ComparisonResult("DIVERGENT", {
            "reason": "call_count_mismatch",
            "decomp_calls": 1,
            "orig_calls": 2,
            "matched_prefix": 1,
        }, warnings=["Call #0: decomp targets merged_82004C00, original targets RealFunc"])
        cls = self.classify(result, decomp, orig, [], [])
        self.assertEqual(cls, "build_env")

    def test_build_env_return_globals(self):
        """Return value in globals region = build_env (string return)."""
        decomp = MockExecutionResult(r3=self.GLOBAL_BASE + 0x50)
        orig = MockExecutionResult(r3=self.GLOBAL_BASE + 0x100)
        result = self.compare(decomp, orig, [], [])
        cls = self.classify(result, decomp, orig, [], [])
        self.assertEqual(cls, "build_env")

    def test_build_env_globals_only_memory(self):
        """Only globals diffs (no object diffs) = build_env."""
        decomp_glob = bytearray(0x10000)
        orig_glob = bytearray(0x10000)
        import struct
        struct.pack_into(">I", decomp_glob, 0, 0xAA)
        struct.pack_into(">I", orig_glob, 0, 0xBB)
        decomp = MockExecutionResult(r3=0, globals_memory=bytes(decomp_glob))
        orig = MockExecutionResult(r3=0, globals_memory=bytes(orig_glob))
        result = self.compare(decomp, orig, [], [])
        cls = self.classify(result, decomp, orig, [], [])
        self.assertEqual(cls, "build_env")

    def test_regalloc_small_value_diff(self):
        """Same call count, small non-pointer arg diff = regalloc."""
        d_log = [make_call_log_entry(0, r3=1, r4=42)]
        o_log = [make_call_log_entry(0, r3=1, r4=99)]
        decomp = MockExecutionResult(call_log=d_log)
        orig = MockExecutionResult(call_log=o_log)
        result = self.compare(decomp, orig, [], [])
        cls = self.classify(result, decomp, orig, [], [])
        self.assertEqual(cls, "regalloc")

    def test_logic_error_mismatch(self):
        decomp = MockExecutionResult(error="fetch 0x00")
        orig = MockExecutionResult(error="fetch 0xFF")
        result = self.compare(decomp, orig, [], [])
        cls = self.classify(result, decomp, orig, [], [])
        self.assertEqual(cls, "logic")

    def test_logic_decomp_error(self):
        decomp = MockExecutionResult(error="crash")
        orig = MockExecutionResult()
        result = self.compare(decomp, orig, [], [])
        cls = self.classify(result, decomp, orig, [], [])
        self.assertEqual(cls, "logic")

    def test_logic_fpr_mismatch(self):
        decomp = MockExecutionResult(r3=0, f1=0x3FF0000000000000)
        orig = MockExecutionResult(r3=0, f1=0x4000000000000000)
        result = self.compare(decomp, orig, [], [])
        cls = self.classify(result, decomp, orig, [], [])
        self.assertEqual(cls, "logic")

    def test_logic_call_count_no_merged(self):
        """Call count mismatch without merged warning = logic."""
        d_log = [make_call_log_entry(0)]
        o_log = [make_call_log_entry(0), make_call_log_entry(1)]
        decomp = MockExecutionResult(call_log=d_log)
        orig = MockExecutionResult(call_log=o_log)
        result = self.compare(decomp, orig, [], [])
        cls = self.classify(result, decomp, orig, [], [])
        self.assertEqual(cls, "logic")


class TestFormatJsonResult(unittest.TestCase):
    """Tests for format_json_result()."""

    def setUp(self):
        from scripts.unicorn_runner.comparator import compare, format_json_result
        self.compare = compare
        self.format_json_result = format_json_result

    def _make_metadata(self, symbol="TestFunc", decomp_size=16, orig_size=16,
                       coloaded_callees=0, combined_code_size=16):
        return {
            "symbol": symbol,
            "decomp_size": decomp_size,
            "orig_size": orig_size,
            "coloaded_callees": coloaded_callees,
            "combined_code_size": combined_code_size,
        }

    def test_equivalent_json(self):
        decomp = MockExecutionResult(r3=42, f1=0)
        orig = MockExecutionResult(r3=42, f1=0)
        result = self.compare(decomp, orig, [], [])
        metadata = self._make_metadata()
        output = self.format_json_result(result, decomp, orig, [], metadata)
        data = json.loads(output)
        self.assertEqual(data["verdict"], "EQUIVALENT")
        self.assertEqual(data["symbol"], "TestFunc")
        self.assertEqual(data["decomp_size"], 16)
        self.assertEqual(data["r3"]["decomp"], 42)
        self.assertEqual(data["r3"]["orig"], 42)

    def test_divergent_json(self):
        decomp = MockExecutionResult(r3=1)
        orig = MockExecutionResult(r3=2)
        result = self.compare(decomp, orig, [], [])
        metadata = self._make_metadata()
        output = self.format_json_result(result, decomp, orig, [], metadata)
        data = json.loads(output)
        self.assertEqual(data["verdict"], "DIVERGENT")

    def test_call_log_resolved(self):
        log = [make_call_log_entry(0, r3=5, source_offset=0x10)]
        decomp = MockExecutionResult(r3=0, call_log=log)
        orig = MockExecutionResult(r3=0, call_log=list(log))
        orig_relocs = [make_reloc(0x10, "Foo::Bar", "REL24")]
        result = self.compare(decomp, orig, [], orig_relocs)
        metadata = self._make_metadata()
        output = self.format_json_result(result, decomp, orig, orig_relocs, metadata)
        data = json.loads(output)
        self.assertEqual(data["decomp_calls"][0]["symbol"], "Foo::Bar")
        self.assertEqual(data["decomp_call_count"], 1)

    def test_coloaded_metadata(self):
        decomp = MockExecutionResult(r3=0)
        orig = MockExecutionResult(r3=0)
        result = self.compare(decomp, orig, [], [])
        metadata = self._make_metadata(coloaded_callees=3, combined_code_size=128)
        output = self.format_json_result(result, decomp, orig, [], metadata)
        data = json.loads(output)
        self.assertEqual(data["coloaded_callees"], 3)
        self.assertEqual(data["combined_code_size"], 128)


if __name__ == "__main__":
    unittest.main()
