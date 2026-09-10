"""Tests for return-type-aware return-value comparison.

Written as negative controls: each test names the OLD wrong answer as well as
the new right one, so a fix that degenerates into "always say scratch" fails
just as loudly as the bug it replaced.

Every mangled name below is a real DC3 symbol that carried
`unicorn_class='return_value'` in decomp.db on 2026-08-19.
"""

import unittest

from .helpers import MockExecutionResult


# Real symbols, with the return type llvm-undname reports for each.
VOID_SYMS = [
    "?Transform@CSHA1@@AAAXPAIPBE@Z",          # private: void CSHA1::Transform(...)
    "?getMasher@KeyChain@@YAXPAE@Z",           # void KeyChain::getMasher(...)
    "?Reset@EQEffect@@QAAXXZ",                 # public: void EQEffect::Reset(void)
    "?getKeyImpl@@YAXPAEPAD0@Z",               # void getKeyImpl(...)
]
FLOAT_SYM = ("?CompareSkeletonJointDisplacement@FreestyleMoveRecorder@@ABAM"
             "PBUFreestyleMoveFrame@@HPBVBaseSkeleton@@AAM@Z")   # private: float
INT_SYMS = [
    "?get_free_node@Trie@@QAAIXZ",             # public: unsigned int
    "?GetBreedData@@YAPAUBreedData@@H@Z",      # struct BreedData *
]
UNMANGLED_SYM = "rijndael_ecb_encrypt"         # plain C -- no return type available
CTOR_SYM = "??0Vector3@@QAA@XZ"                # ctor: returns `this` in r3


class TestReturnTypeClass(unittest.TestCase):
    def setUp(self):
        from scripts.unicorn_runner.rettype import return_type_class
        self.rtc = return_type_class

    def test_void_symbols(self):
        for sym in VOID_SYMS:
            self.assertEqual(self.rtc(sym), "void", sym)

    def test_float_symbol(self):
        self.assertEqual(self.rtc(FLOAT_SYM), "float")

    def test_int_symbols(self):
        for sym in INT_SYMS:
            self.assertEqual(self.rtc(sym), "int", sym)

    def test_unmangled_is_unknown_not_guessed(self):
        """A C symbol carries no return type. Say so; do not invent one."""
        self.assertIsNone(self.rtc(UNMANGLED_SYM))

    def test_constructor_is_unknown_not_void(self):
        """MSVC ctors really do return `this` in r3, so r3 IS meaningful.

        Calling a ctor 'void' would suppress a genuine signal -- the opposite
        failure to the one being fixed.
        """
        self.assertIsNone(self.rtc(CTOR_SYM))

    def test_accepts_a_pre_demangled_string(self):
        self.assertEqual(
            self.rtc("whatever", "public: void __cdecl EQEffect::Reset(void)"),
            "void")
        self.assertEqual(
            self.rtc("whatever", "private: float __cdecl Foo::Bar(int)"), "float")
        self.assertEqual(
            self.rtc("whatever", "struct BreedData * __cdecl GetBreedData(int)"),
            "int")


class TestReturnRegisterAwareCompare(unittest.TestCase):
    def setUp(self):
        from scripts.unicorn_runner.comparator import compare, classify_divergence
        self.compare = compare
        self.classify = classify_divergence

    # -- the bug ------------------------------------------------------------

    def test_void_r3_was_a_false_return_value_bug(self):
        decomp = MockExecutionResult(r3=0x11111111, f1=0)
        orig = MockExecutionResult(r3=0x22222222, f1=0)

        # OLD behaviour, still reachable when no symbol is supplied.
        old = self.compare(decomp, orig, [], [])
        self.assertEqual(old.details["reason"], "return_value_mismatch")
        self.assertEqual(self.classify(old, decomp, orig, [], []), "return_value")

        # NEW: r3 is not this function's return register.
        for sym in VOID_SYMS:
            new = self.compare(decomp, orig, [], [], symbol=sym)
            self.assertEqual(new.verdict, "DIVERGENT", sym)   # NOT silenced
            self.assertEqual(new.details["reason"],
                             "scratch_return_reg_mismatch", sym)
            self.assertEqual(self.classify(new, decomp, orig, [], []),
                             "scratch_return_reg", sym)

    def test_float_r3_no_longer_shadows_the_f1_check(self):
        """The float case was worse: a real f1 divergence was reported as r3.

        r3 was compared first and unconditionally, so on a float-returning
        function a scratch r3 difference short-circuited the fpr_return_mismatch
        branch entirely.
        """
        decomp = MockExecutionResult(r3=0x11111111, f1=0x3FF0000000000000)
        orig = MockExecutionResult(r3=0x22222222, f1=0x4000000000000000)

        old = self.compare(decomp, orig, [], [])
        self.assertEqual(old.details["reason"], "return_value_mismatch")   # wrong reg

        new = self.compare(decomp, orig, [], [], symbol=FLOAT_SYM)
        self.assertEqual(new.details["reason"], "fpr_return_mismatch")
        self.assertEqual(self.classify(new, decomp, orig, [], []), "fpr_precision")

    def test_float_scratch_r3_alone_is_not_a_return_bug(self):
        decomp = MockExecutionResult(r3=0x11111111, f1=0x1234)
        orig = MockExecutionResult(r3=0x22222222, f1=0x1234)
        res = self.compare(decomp, orig, [], [], symbol=FLOAT_SYM)
        self.assertEqual(res.verdict, "DIVERGENT")
        self.assertEqual(self.classify(res, decomp, orig, [], []),
                         "scratch_return_reg")

    # -- the guard must not over-fire ---------------------------------------

    def test_int_return_still_reports_return_value(self):
        """Companion control: a real r3 return bug must still be `return_value`."""
        decomp = MockExecutionResult(r3=7, f1=0)
        orig = MockExecutionResult(r3=9, f1=0)
        for sym in INT_SYMS:
            res = self.compare(decomp, orig, [], [], symbol=sym)
            self.assertEqual(res.details["reason"], "return_value_mismatch", sym)
            self.assertEqual(self.classify(res, decomp, orig, [], []),
                             "return_value", sym)

    def test_unknown_return_type_reproduces_old_behaviour_exactly(self):
        decomp = MockExecutionResult(r3=7, f1=0)
        orig = MockExecutionResult(r3=9, f1=0)
        with_sym = self.compare(decomp, orig, [], [], symbol=UNMANGLED_SYM)
        without = self.compare(decomp, orig, [], [])
        self.assertEqual(with_sym.details["reason"], without.details["reason"])
        self.assertEqual(
            self.classify(with_sym, decomp, orig, [], []),
            self.classify(without, decomp, orig, [], []))

    def test_identical_registers_are_still_equivalent(self):
        decomp = MockExecutionResult(r3=5, f1=1)
        orig = MockExecutionResult(r3=5, f1=1)
        for sym in VOID_SYMS + [FLOAT_SYM] + INT_SYMS:
            self.assertEqual(
                self.compare(decomp, orig, [], [], symbol=sym).verdict,
                "EQUIVALENT", sym)

    def test_classify_safety_net_without_a_symbol_in_compare(self):
        """Old results (already in the DB) can be re-classified after the fact."""
        decomp = MockExecutionResult(r3=0x1111, f1=0)
        orig = MockExecutionResult(r3=0x2222, f1=0)
        old = self.compare(decomp, orig, [], [])       # no symbol -> old reason
        self.assertEqual(old.details["reason"], "return_value_mismatch")
        self.assertEqual(
            self.classify(old, decomp, orig, [], [], symbol=VOID_SYMS[0]),
            "scratch_return_reg")
        self.assertEqual(
            self.classify(old, decomp, orig, [], [], symbol=INT_SYMS[0]),
            "return_value")


if __name__ == "__main__":
    unittest.main()
