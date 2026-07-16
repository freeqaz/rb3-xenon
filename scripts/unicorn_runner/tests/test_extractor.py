"""Tests for extractor.py — indirect branch classification."""

import unittest

from .helpers import assemble, ppc_li, ppc_blr, ppc_bctr, ppc_bctrl, ppc_mtctr, ppc_nop
from .helpers import make_reloc, MockCOFF


class TestHasIndirectBranch(unittest.TestCase):
    """Tests for has_indirect_branch()."""

    def setUp(self):
        from scripts.unicorn_runner.extractor import has_indirect_branch
        self.has_indirect_branch = has_indirect_branch

    def test_no_indirect(self):
        code = assemble(ppc_li(3, 0), ppc_blr())
        self.assertIsNone(self.has_indirect_branch(code))

    def test_bctrl_detected(self):
        code = assemble(ppc_li(3, 0), ppc_bctrl(), ppc_blr())
        self.assertEqual(self.has_indirect_branch(code), "bctrl")

    def test_bctr_detected(self):
        code = assemble(ppc_li(3, 0), ppc_bctr())
        self.assertEqual(self.has_indirect_branch(code), "bctr")

    def test_bctrl_before_bctr(self):
        """bctrl found first in scan order takes priority."""
        code = assemble(ppc_bctrl(), ppc_bctr())
        self.assertEqual(self.has_indirect_branch(code), "bctrl")


class TestClassifyIndirectBranch(unittest.TestCase):
    """Tests for classify_indirect_branch()."""

    def setUp(self):
        from scripts.unicorn_runner.extractor import classify_indirect_branch
        self.classify = classify_indirect_branch

    def test_no_indirect(self):
        code = assemble(ppc_li(3, 0), ppc_blr())
        self.assertIsNone(self.classify(code, []))

    def test_bctrl_classified(self):
        code = assemble(ppc_li(3, 0), ppc_bctrl(), ppc_blr())
        result = self.classify(code, [])
        self.assertEqual(result, "bctrl")

    def test_bctr_with_rdata_relocs(self):
        """bctr + REFHI/REFLO to .rdata section → bctr_switch."""
        code = assemble(ppc_nop(), ppc_nop(), ppc_mtctr(12), ppc_bctr())
        relocs = [
            make_reloc(0, "$SG1234", "REFHI"),
            make_reloc(4, "$SG1234", "REFLO"),
        ]
        coff = MockCOFF(
            sections=[{"name": ".rdata$1", "raw_size": 64}],
            symbol_map={"$SG1234": {"section": 1, "value": 0}},
        )
        result = self.classify(code, relocs, coff=coff)
        self.assertEqual(result, "bctr_switch")

    def test_bctr_without_rdata(self):
        """bctr with no .rdata references → bctr_tailcall."""
        code = assemble(ppc_mtctr(12), ppc_bctr())
        relocs = [make_reloc(0, "some_func", "REL24")]
        coff = MockCOFF(
            sections=[{"name": ".text", "raw_size": 64}],
            symbol_map={"some_func": {"section": 1, "value": 0}},
        )
        result = self.classify(code, relocs, coff=coff)
        self.assertEqual(result, "bctr_tailcall")

    def test_bctrl_takes_priority(self):
        """Both bctrl and bctr present, no .rdata → bctrl wins."""
        code = assemble(ppc_bctrl(), ppc_nop(), ppc_bctr())
        result = self.classify(code, [])
        self.assertEqual(result, "bctrl")

    def test_mixed_bctrl_bctr_with_rdata(self):
        """Both bctrl and bctr + REFHI/REFLO to .rdata → bctrl_switch."""
        code = assemble(ppc_bctrl(), ppc_nop(), ppc_nop(), ppc_mtctr(12), ppc_bctr())
        relocs = [
            make_reloc(4, "$SG5678", "REFHI"),
            make_reloc(8, "$SG5678", "REFLO"),
        ]
        coff = MockCOFF(
            sections=[{"name": ".rdata$2", "raw_size": 128}],
            symbol_map={"$SG5678": {"section": 1, "value": 0}},
        )
        result = self.classify(code, relocs, coff=coff)
        self.assertEqual(result, "bctrl_switch")

    def test_classify_no_coff(self):
        """bctr + coff=None → bctr_tailcall (conservative)."""
        code = assemble(ppc_mtctr(12), ppc_bctr())
        result = self.classify(code, [], coff=None)
        self.assertEqual(result, "bctr_tailcall")


class TestHasPpc64Insns(unittest.TestCase):
    """Tests for has_ppc64_insns()."""

    def setUp(self):
        from scripts.unicorn_runner.extractor import has_ppc64_insns
        self.has_ppc64 = has_ppc64_insns

    def test_no_ppc64(self):
        code = assemble(ppc_li(3, 0), ppc_blr())
        self.assertIsNone(self.has_ppc64(code))

    def test_has_std(self):
        from .helpers import ppc_std
        code = assemble(ppc_std(31, -8, 1), ppc_blr())
        self.assertEqual(self.has_ppc64(code), "std/ld")

    def test_has_ld(self):
        from .helpers import ppc_ld
        code = assemble(ppc_ld(31, -8, 1), ppc_blr())
        self.assertEqual(self.has_ppc64(code), "std/ld")


if __name__ == "__main__":
    unittest.main()
