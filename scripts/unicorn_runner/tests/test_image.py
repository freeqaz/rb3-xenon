"""Tests for image.py + patcher.seed_image_globals — one initial global image.

The harness runs each function from two different .obj files, and those two
files do not define the same set of globals. Whichever side leaves a symbol
undefined used to read zero for it, so the sides disagreed about constants
neither of them computed. These tests pin the fix and, just as importantly,
pin what it must NOT do: a symbol our own .obj defines keeps our bytes, so a
dropped initializer still shows up as a divergence.
"""

import struct
import unittest

from scripts.unicorn_runner.image import (
    GlobalImage, ImageSymbol, get_global_image, project_root)
from scripts.unicorn_runner.memory_map import GLOBAL_BASE, RDATA_BASE

from .helpers import make_reloc, MockCOFF


_FAKE_IMAGE_LO = 0x82000000
_FAKE_IMAGE_HI = 0x83200000


class FakeImage:
    """Stand-in for GlobalImage with a hand-written symbol table."""

    available = True
    reason = ""

    def __init__(self, entries):
        # name -> (address, bytes)
        self._entries = entries

    def is_image_pointer(self, value):
        return _FAKE_IMAGE_LO <= value < _FAKE_IMAGE_HI

    def contains_image_pointer(self, content):
        for i in range(0, len(content) - 3, 4):
            if self.is_image_pointer(struct.unpack_from(">I", content, i)[0]):
                return True
        return False

    def lookup(self, name):
        entry = self._entries.get(name)
        if entry is None:
            return None
        addr, data = entry
        return ImageSymbol(name, addr, len(data), ".data")

    def read(self, address, size):
        for addr, data in self._entries.values():
            if addr <= address < addr + len(data):
                off = address - addr
                out = data[off:off + size]
                return out + b"\x00" * (size - len(out))
        return None

    def symbol_bytes(self, name, size=None):
        entry = self._entries.get(name)
        if entry is None:
            return None
        return entry[1]


class UnavailableImage:
    available = False
    reason = "test"

    def lookup(self, name):
        return None

    def read(self, address, size):
        return None

    def symbol_bytes(self, name, size=None):
        return None


def _bss_coff(defined):
    """A COFF that defines `defined` (name -> section name), nothing else."""
    sections = [{"name": ".text"}, {"name": ".data"}, {"name": ".bss"}]
    index = {".text": 1, ".data": 2, ".bss": 3}
    symbol_map = {name: {"name": name, "value": 0, "section": index[sec]}
                  for name, sec in defined.items()}
    return MockCOFF(sections=sections, symbol_map=symbol_map)


class TestSeedImageGlobals(unittest.TestCase):

    def setUp(self):
        from scripts.unicorn_runner.patcher import seed_image_globals
        self.seed = seed_image_globals

    def test_undefined_scalar_is_seeded_in_place(self):
        """kSampleRate: undefined here, 48000.0f in the image, read as 0."""
        coff = _bss_coff({})
        relocs = [make_reloc(0x10, "kSampleRate", "REFHI"),
                  make_reloc(0x14, "kSampleRate", "REFLO")]
        globals_map = {"kSampleRate": GLOBAL_BASE}
        image = FakeImage({"kSampleRate": (0x82F499AC,
                                           struct.pack(">f", 48000.0))})

        rdata, init = self.seed(coff, relocs, globals_map, None, image=image)

        self.assertEqual(init, {GLOBAL_BASE: struct.pack(">f", 48000.0)})
        # Address is untouched: a store to the global must still land in the
        # region the comparator diffs.
        self.assertEqual(globals_map["kSampleRate"], GLOBAL_BASE)
        self.assertIsNone(rdata)

    def test_symbol_we_define_is_never_seeded(self):
        """A .bss definition of ours stays zero — that is the bug signal.

        `float CharClipDisplay::sZoom;` lands in .bss and reads 0 while the
        original holds 0x3F800000. That divergence is a real dropped
        initializer (4f8b6e036 fixed seven of them). Seeding over our own
        definition would erase the entire class.
        """
        coff = _bss_coff({"?sZoom@CharClipDisplay@@0MA": ".bss"})
        relocs = [make_reloc(0, "?sZoom@CharClipDisplay@@0MA", "REFHI")]
        globals_map = {"?sZoom@CharClipDisplay@@0MA": GLOBAL_BASE}
        image = FakeImage({"?sZoom@CharClipDisplay@@0MA":
                           (0x82F00000, struct.pack(">f", 1.0))})

        rdata, init = self.seed(coff, relocs, globals_map, None, image=image)

        self.assertEqual(init, {})
        self.assertEqual(globals_map["?sZoom@CharClipDisplay@@0MA"], GLOBAL_BASE)

    def test_symbol_we_define_in_data_is_never_seeded(self):
        coff = _bss_coff({"gThing": ".data"})
        relocs = [make_reloc(0, "gThing", "REFLO")]
        globals_map = {"gThing": GLOBAL_BASE}
        image = FakeImage({"gThing": (0x82F00000, b"\x11\x22\x33\x44")})

        _rdata, init = self.seed(coff, relocs, globals_map, None, image=image)

        self.assertEqual(init, {})

    def test_zero_content_is_not_seeded(self):
        """TEXT_REV is zero in the image too — seeding it would be a no-op."""
        coff = _bss_coff({})
        relocs = [make_reloc(0, "?TEXT_REV@@3HA", "REFHI")]
        globals_map = {"?TEXT_REV@@3HA": GLOBAL_BASE}
        image = FakeImage({"?TEXT_REV@@3HA": (0x830A6560, b"\x00\x00\x00\x00")})

        rdata, init = self.seed(coff, relocs, globals_map, None, image=image)

        self.assertEqual(init, {})
        self.assertIsNone(rdata)

    def test_unknown_symbol_is_left_alone(self):
        coff = _bss_coff({})
        relocs = [make_reloc(0, "?someLocalThing@@3HA", "REFHI")]
        globals_map = {"?someLocalThing@@3HA": GLOBAL_BASE}
        image = FakeImage({})

        rdata, init = self.seed(coff, relocs, globals_map, None, image=image)

        self.assertEqual(init, {})
        self.assertIsNone(rdata)

    def test_large_symbol_moves_to_rdata(self):
        """Aggregates do not fit a 4-byte slot; they go to the RDATA buffer."""
        coff = _bss_coff({})
        blob = bytes(range(32))
        relocs = [make_reloc(0, "??_7Foo@@6B@", "REFHI")]
        globals_map = {"??_7Foo@@6B@": GLOBAL_BASE}
        image = FakeImage({"??_7Foo@@6B@": (0x82F10000, blob)})

        rdata, init = self.seed(coff, relocs, globals_map, None, image=image)

        self.assertEqual(init, {})
        self.assertIsNotNone(rdata)
        addr = globals_map["??_7Foo@@6B@"]
        self.assertTrue(RDATA_BASE <= addr < RDATA_BASE + 0x10000)
        off = addr - RDATA_BASE
        self.assertEqual(rdata[off:off + len(blob)], blob)

    def test_large_symbol_appends_after_existing_rdata(self):
        coff = _bss_coff({})
        existing = b"\xAA" * 6
        blob = bytes(range(16))
        relocs = [make_reloc(0, "gBlob", "REFHI")]
        globals_map = {"gBlob": GLOBAL_BASE}
        image = FakeImage({"gBlob": (0x82F10000, blob)})

        rdata, _init = self.seed(coff, relocs, globals_map, existing,
                                 image=image)

        self.assertEqual(rdata[:6], existing)
        off = globals_map["gBlob"] - RDATA_BASE
        self.assertEqual(rdata[off:off + len(blob)], blob)

    def test_symbol_already_mapped_to_rdata_is_skipped(self):
        """Data sections already gave this side real bytes; do not relocate."""
        coff = _bss_coff({})
        relocs = [make_reloc(0, "gThing", "REFHI")]
        globals_map = {"gThing": RDATA_BASE + 0x40}
        image = FakeImage({"gThing": (0x82F10000, b"\x01\x02\x03\x04")})

        _rdata, init = self.seed(coff, relocs, globals_map, None, image=image)

        self.assertEqual(init, {})
        self.assertEqual(globals_map["gThing"], RDATA_BASE + 0x40)

    def test_rel24_targets_are_not_seeded(self):
        """Call targets get trampolines, not data."""
        coff = _bss_coff({})
        relocs = [make_reloc(0, "?Func@@YAXXZ", "REL24")]
        globals_map = {}
        image = FakeImage({"?Func@@YAXXZ": (0x82F10000, b"\x01\x02\x03\x04")})

        _rdata, init = self.seed(coff, relocs, globals_map, None, image=image)

        self.assertEqual(init, {})

    def test_unavailable_image_is_a_no_op(self):
        coff = _bss_coff({})
        relocs = [make_reloc(0, "kSampleRate", "REFHI")]
        globals_map = {"kSampleRate": GLOBAL_BASE}

        rdata, init = self.seed(coff, relocs, globals_map, b"keep",
                                image=UnavailableImage())

        self.assertEqual(init, {})
        self.assertEqual(rdata, b"keep")

    def test_pointer_valued_scalar_is_not_seeded(self):
        """gNullStr holds 0x82001BCC — an address the harness cannot honour.

        Seeding it turned a null the SetType functions guard against into a
        live pointer at an on-demand zero page, and six functions went
        EQUIVALENT -> DIVERGENT in a 40-unit A/B.
        """
        coff = _bss_coff({})
        relocs = [make_reloc(0, "?gNullStr@@3PBDB", "REFHI")]
        globals_map = {"?gNullStr@@3PBDB": GLOBAL_BASE}
        image = FakeImage({"?gNullStr@@3PBDB":
                           (0x82F10000, struct.pack(">I", 0x82001BCC))})

        rdata, init = self.seed(coff, relocs, globals_map, None, image=image)

        self.assertEqual(init, {})
        self.assertIsNone(rdata)

    def test_pointer_bearing_aggregate_is_not_seeded(self):
        """A vtable is nothing but addresses into code we do not map."""
        coff = _bss_coff({})
        vtable = struct.pack(">IIII", 0x8233ABCD, 0x8233ABDD, 0, 0)
        relocs = [make_reloc(0, "??_7Foo@@6B@", "REFHI")]
        globals_map = {"??_7Foo@@6B@": GLOBAL_BASE}
        image = FakeImage({"??_7Foo@@6B@": (0x82F10000, vtable)})

        rdata, init = self.seed(coff, relocs, globals_map, None, image=image)

        self.assertEqual(init, {})
        self.assertIsNone(rdata)
        self.assertEqual(globals_map["??_7Foo@@6B@"], GLOBAL_BASE)

    def test_string_literal_is_still_seeded(self):
        """Characters are not addresses; strings are exactly what to seed."""
        coff = _bss_coff({})
        blob = b"types\x00\x00\x00"
        relocs = [make_reloc(0, "??_C@_05LHNBAFH@types?$AA@", "REFHI")]
        globals_map = {"??_C@_05LHNBAFH@types?$AA@": GLOBAL_BASE}
        image = FakeImage({"??_C@_05LHNBAFH@types?$AA@": (0x82F10000, blob)})

        rdata, _init = self.seed(coff, relocs, globals_map, None, image=image)

        off = globals_map["??_C@_05LHNBAFH@types?$AA@"] - RDATA_BASE
        self.assertEqual(rdata[off:off + len(blob)], blob)

    def test_oversized_symbol_is_skipped(self):
        coff = _bss_coff({})
        blob = b"\x7F" * 8192
        relocs = [make_reloc(0, "gHugeTable", "REFHI")]
        globals_map = {"gHugeTable": GLOBAL_BASE}
        image = FakeImage({"gHugeTable": (0x82F10000, blob)})

        rdata, init = self.seed(coff, relocs, globals_map, None, image=image)

        self.assertEqual(init, {})
        self.assertIsNone(rdata)
        self.assertEqual(globals_map["gHugeTable"], GLOBAL_BASE)


class TestCompareWrittenMemory(unittest.TestCase):
    """Words neither side wrote are placement, not behaviour."""

    def setUp(self):
        from scripts.unicorn_runner.comparator import compare_written_memory
        self.compare = compare_written_memory
        self.size = 16

    def _mem(self, words):
        return b"".join(struct.pack(">I", w) for w in words)

    def test_untouched_seed_difference_is_ignored(self):
        init_d = self._mem([0, 0, 0, 0])
        init_o = self._mem([0x473B8000, 0, 0, 0])   # orig seeded kSampleRate
        diffs = self.compare(init_d, init_o, init_d, init_o,
                             GLOBAL_BASE, self.size)
        self.assertEqual(diffs, [])

    def test_one_sided_write_is_reported(self):
        init_d = self._mem([0, 0, 0, 0])
        init_o = self._mem([0, 0, 0, 0])
        final_d = self._mem([0, 0xDEAD, 0, 0])
        final_o = init_o
        diffs = self.compare(final_d, final_o, init_d, init_o,
                             GLOBAL_BASE, self.size)
        self.assertEqual(diffs, [(GLOBAL_BASE + 4, 0xDEAD, 0)])

    def test_differing_writes_are_reported(self):
        init_d = self._mem([0, 0, 0, 0])
        init_o = self._mem([0x473B8000, 0, 0, 0])
        final_d = self._mem([1, 0, 0, 0])
        final_o = self._mem([2, 0, 0, 0])
        diffs = self.compare(final_d, final_o, init_d, init_o,
                             GLOBAL_BASE, self.size)
        self.assertEqual(diffs, [(GLOBAL_BASE, 1, 2)])

    def test_write_over_a_seeded_word_is_reported(self):
        """Orig overwrites its seeded constant, decomp leaves its zero."""
        init_d = self._mem([0, 0, 0, 0])
        init_o = self._mem([0x473B8000, 0, 0, 0])
        final_d = init_d
        final_o = self._mem([0x11111111, 0, 0, 0])
        diffs = self.compare(final_d, final_o, init_d, init_o,
                             GLOBAL_BASE, self.size)
        self.assertEqual(diffs, [(GLOBAL_BASE, 0, 0x11111111)])

    def test_identical_memory_is_equivalent(self):
        mem = self._mem([1, 2, 3, 4])
        self.assertEqual(
            self.compare(mem, mem, self._mem([0, 0, 0, 0]),
                         self._mem([0, 0, 0, 0]), GLOBAL_BASE, self.size),
            [])

    def test_missing_initials_fall_back_to_plain_compare(self):
        a = self._mem([1, 0, 0, 0])
        b = self._mem([2, 0, 0, 0])
        diffs = self.compare(a, b, None, None, GLOBAL_BASE, self.size)
        self.assertEqual(diffs, [(GLOBAL_BASE, 1, 2)])


_IMAGE = get_global_image(project_root())


@unittest.skipUnless(getattr(_IMAGE, "available", False),
                     f"shipped image unavailable: {getattr(_IMAGE, 'reason', '')}")
class TestRealShippedImage(unittest.TestCase):
    """Against the actual band.exe + symbols.txt, when present.

    ⚠ Witnesses RETITLED for rb3-xenon by lane U1-DEEPSCHED. dc3's were
    kSampleRate / ?TEXT_REV@@3HA / ?gNullStr@@3PBDB, none of which exist in
    RB3's symbol map, so inheriting them would have produced four failures
    that say nothing about this binary.

    Picking replacements is harder here than in dc3 for a structural reason
    worth recording: RB3 retail is stripped, so its symbols.txt names data
    almost entirely as `lbl_<addr>`. Of 129,008 indexed data symbols, the
    named-and-nonzero-and-not-a-pointer population is dominated by MSVC EH
    records, and there are ZERO source-named pointer-valued or bss-tail data
    symbols to choose from. The witnesses below are therefore addresses, not
    pretty names -- but each is pinned to a value that is independently
    meaningful, so a regression still cannot pass.
    """

    # 0x19930522 is MSVC's FuncInfo magic -- the same constant this project's
    # CLAUDE.md cites (8,541 records) as evidence that retail was built /EHs*.
    # A four-byte read that returns it proves section->file-offset arithmetic.
    FUNCINFO_MAGIC = 0x19930522

    def test_except_record_carries_the_funcinfo_magic(self):
        sym = _IMAGE.lookup("except_record_82270210")
        self.assertIsNotNone(sym)
        self.assertEqual(sym.size, 4)
        self.assertEqual(_IMAGE.symbol_bytes("except_record_82270210"),
                         struct.pack(">I", self.FUNCINFO_MAGIC))

    def test_bss_tail_reads_as_zero(self):
        """A symbol past .data's file content is zero, as at load time.

        .data is VA 0x82C64400, vsize 0x1F5EAC, raw 0x058200 -- so everything
        at or above 0x82CBC600 is the real .bss and has no file content.
        13,494 indexed symbols live there.

        Worth pinning: it is the reason seeding cannot smuggle in a value the
        game only establishes at runtime. The image is a link-time image, not
        a snapshot of a running console.
        """
        sym = _IMAGE.lookup("lbl_82CBC600")
        self.assertIsNotNone(sym)
        self.assertGreaterEqual(sym.address, 0x82CBC600)
        self.assertEqual(_IMAGE.symbol_bytes("lbl_82CBC600"), b"\x00" * 4)

    def test_image_pointer_is_recognised_as_a_pointer(self):
        """lbl_82000A00 holds 0x821CED74, an address inside .text.

        Such a symbol must NOT be seeded: the harness maps none of the image,
        so the pointer would aim at an on-demand zero page.
        """
        content = _IMAGE.symbol_bytes("lbl_82000A00")
        self.assertIsNotNone(content)
        self.assertEqual(content, struct.pack(">I", 0x821CED74))
        self.assertTrue(_IMAGE.contains_image_pointer(content))

    def test_scalar_constant_is_not_a_pointer(self):
        self.assertFalse(_IMAGE.contains_image_pointer(
            _IMAGE.symbol_bytes("except_record_82270210")))

    def test_function_symbols_are_not_data(self):
        self.assertIsNone(_IMAGE.lookup("?Poll@App@@UAAXXZ"))

    def test_unmapped_address_reads_none(self):
        self.assertIsNone(_IMAGE.read(0x10000000, 4))

    def test_missing_image_degrades_quietly(self):
        img = GlobalImage("/nonexistent/ham.exe", "/nonexistent/symbols.txt")
        self.assertFalse(img.available)
        self.assertTrue(img.reason)


if __name__ == "__main__":
    unittest.main()
