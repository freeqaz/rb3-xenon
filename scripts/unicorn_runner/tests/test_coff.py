"""Tests for coff.py — COFF parser (integration, needs .obj files).

These tests require build artifacts. They are skipped when .obj files don't exist.
"""

import os
import unittest

# Use Skeleton as a well-known, small unit that is almost always built
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SKELETON_DECOMP = os.path.join(PROJECT_ROOT, "build", "373307D9", "system", "gesture", "Skeleton.obj")
SKELETON_ORIG = os.path.join(PROJECT_ROOT, "orig", "373307D9", "system", "gesture", "Skeleton.obj")

HAS_SKELETON = os.path.exists(SKELETON_DECOMP) and os.path.exists(SKELETON_ORIG)


@unittest.skipUnless(HAS_SKELETON, "Skeleton .obj build artifacts required")
class TestCOFFParser(unittest.TestCase):
    """Integration tests for COFFParser against real .obj files."""

    def setUp(self):
        from scripts.unicorn_runner.coff import COFFParser, IMAGE_FILE_MACHINE_POWERPCBE
        self.COFFParser = COFFParser
        self.MACHINE = IMAGE_FILE_MACHINE_POWERPCBE
        self.decomp_coff = COFFParser(SKELETON_DECOMP)
        self.orig_coff = COFFParser(SKELETON_ORIG)

    def test_parse_header(self):
        """Machine type is PPC BE, sections and symbols counts are sensible."""
        self.assertEqual(self.decomp_coff.machine, self.MACHINE)
        self.assertEqual(self.orig_coff.machine, self.MACHINE)
        self.assertGreater(len(self.decomp_coff.sections), 0)
        self.assertGreater(len(self.orig_coff.sections), 0)
        self.assertGreater(len(self.decomp_coff.symbols), 0)
        self.assertGreater(len(self.orig_coff.symbols), 0)

    def test_symbol_lookup(self):
        """Known symbol found in symbol_map."""
        # Skeleton should have at least some function symbols
        self.assertGreater(len(self.orig_coff.symbol_map), 0)
        # Check that at least one symbol has a non-empty name
        found_named = False
        for name in self.orig_coff.symbol_map:
            if len(name) > 1 and not name.startswith('.'):
                found_named = True
                break
        self.assertTrue(found_named, "No named symbols found in original COFF")

    def test_section_relocations(self):
        """Text sections have relocations."""
        text_secs = self.orig_coff.get_text_sections()
        self.assertGreater(len(text_secs), 0)
        total_relocs = 0
        for sec in text_secs:
            sec_idx = sec['index'] - 1
            relocs = self.orig_coff.get_section_relocations(sec_idx)
            total_relocs += len(relocs)
            # Check reloc structure
            for r in relocs:
                self.assertIn('offset', r)
                self.assertIn('symbol_name', r)
                self.assertIn('type_name', r)
        self.assertGreater(total_relocs, 0, "No relocations found in any .text section")

    def test_section_data_length(self):
        """get_section_data length matches raw_size."""
        for i, sec in enumerate(self.decomp_coff.sections):
            if sec['raw_size'] > 0:
                data = self.decomp_coff.get_section_data(i)
                self.assertEqual(len(data), sec['raw_size'],
                                 f"Section '{sec['name']}' data length mismatch")
                break  # Just test first non-empty section

    def test_reloc_type_names(self):
        """Relocation type names are resolved (not UNKNOWN)."""
        text_secs = self.orig_coff.get_text_sections()
        for sec in text_secs:
            sec_idx = sec['index'] - 1
            relocs = self.orig_coff.get_section_relocations(sec_idx)
            for r in relocs:
                self.assertNotIn("UNKNOWN", r['type_name'],
                                 f"Unknown reloc type in section '{sec['name']}'")


if __name__ == "__main__":
    unittest.main()
