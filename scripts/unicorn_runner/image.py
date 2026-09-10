"""The shipped image's initial global memory, shared by both sides.

Ported from dc3-decomp (lane U1-DEEPSCHED, 2026-09-10) and retitled for
rb3-xenon: `orig/45410914/band.exe` is the decompressed PE (PE32, image base
0x82000000, 12 sections) and `config/45410914/symbols.txt` maps names to
addresses in it. Both were verified present and parseable in this tree before
the module was written; the format of symbols.txt here is byte-identical to
dc3's (`NAME = .rdata:0x82000400; // type:object size:0x4 scope:global`).

WHY THIS EXISTS
---------------
The runner executes each function twice: once from the decomp .obj, once from
the original .obj the splitter carved out of the shipped image. Both sides read
globals through REFHI/REFLO/ADDR32 relocations, and the harness has to decide
what those globals *contain* before the function runs.

Until now each side answered that question from its own .obj alone:

  * a symbol the .obj DEFINES in .data/.rdata contributes its own section bytes
    (patcher.prepare_data_sections);
  * a symbol the .obj does NOT define -- an `extern` from another translation
    unit -- got a bare zero-filled slot in the GLOBAL region.

The two sides do not agree on which symbols they define. The decomp compiles
one .cpp at a time, so `static float kSampleRate = 48000.0f;` is real .data in
our object. The splitter, carving the same function out of the linked image,
has no idea which unit owned that word: it lives in some other split object, so
the original's object carries kSampleRate as an UNDEFINED external. Result: the
decomp divides by 48000.0f and the original divides by 0, and the runner blames
the *decomp*.

That asymmetry is the mechanism behind the artifact class lane S4 measured
here as "the mock-region artifact" -- a symbol that is `.rdata` for us and
absent/`.data` for the split target yields different mocked pointers with no
behavioural difference (367 of its 1,009 screened-out rows were the single pair
rdata->globals). Seeding from the image attacks that at the source instead of
screening it after the fact.

WHAT THIS DELIBERATELY DOES NOT DO
----------------------------------
It never touches a symbol the .obj *defines*. If our .obj puts a static in .bss
(zero) while the original defines it in .data holding 0x3F800000, that is a
dropped initializer in our source -- a real, behaviourally live bug, and in dc3
exactly how CharClipDisplay::sZoom and six others were found. Seeding defined
symbols from the image would have erased that entire signal. Only the
absent-definition case is filled in.

⚠ That asymmetry is the whole point for this lane: the `logic` class we are
hunting is *precisely* the case where our object's own bytes differ from the
image's, and it only becomes visible once every OTHER reason for the two sides
to hold different global memory has been removed.

Note also that the image is a LINK-TIME image, not a runtime snapshot: the PE
gives .data a virtual size of 0x1F5EAC but only 0x058200 bytes of file content,
so everything above 0x82CBC600 -- the real .bss -- reads as zero here, just as
it does on the console at load. Seeding cannot smuggle in a value that the game
only establishes at runtime, because the image does not contain one.
"""

import os
import re
import struct
import threading

# Data symbols only. A REFHI/REFLO pair may also name a *function* (taking its
# address); the value there is the address itself and the trampoline machinery
# already handles it, so code sections are excluded. `type:` is the primary
# filter (see _parse_symbols); this is belt-and-braces for rows that carry no
# type.
#
# Measured over this tree's 225,965-row symbols.txt before fixing the set:
# `.text` 96,815 rows / `BINK` 142 are overwhelmingly type:function, while
# `BINKCONS` is 6 of 6 type:object -- it is Bink's CONSTANT data, not code, so
# listing it here (as a first draft of this file did, by analogy with dc3's
# RADCODE) would have silently dropped 6 seedable symbols.
_CODE_SECTIONS = frozenset({".text", "BINK", "RADCODE"})

# `NAME = .section:0xADDR; // type:object size:0x4 ...`
_SYMBOL_RE = re.compile(
    r"^([^\s=]+)\s*=\s*([^:]+):0x([0-9A-Fa-f]+);(.*)$")
_SIZE_RE = re.compile(r"\bsize:0x([0-9A-Fa-f]+)")
_TYPE_RE = re.compile(r"\btype:(\w+)")

_DEFAULT_TITLE = "45410914"
_IMAGE_NAME = "band.exe"

_lock = threading.Lock()
_cached = {}


class ImageSymbol:
    """A data symbol's placement in the shipped image."""

    __slots__ = ("name", "address", "size", "section")

    def __init__(self, name, address, size, section):
        self.name = name
        self.address = address
        self.size = size
        self.section = section

    def __repr__(self):                      # pragma: no cover — debugging aid
        return (f"ImageSymbol({self.name!r}, 0x{self.address:08X}, "
                f"{self.size}, {self.section!r})")


class GlobalImage:
    """Read-only view of the shipped image's data, keyed by symbol name."""

    def __init__(self, exe_path, symbols_path):
        self.exe_path = exe_path
        self.symbols_path = symbols_path
        self._sections = []      # (name, va_start, vsize, raw_off, raw_size)
        self._symbols = {}       # name -> ImageSymbol
        self._data = None
        self.available = False
        self.reason = ""
        self._load()

    # ---- loading ---------------------------------------------------------

    def _load(self):
        if not (self.exe_path and os.path.exists(self.exe_path)):
            self.reason = f"image not found: {self.exe_path}"
            return
        if not (self.symbols_path and os.path.exists(self.symbols_path)):
            self.reason = f"symbols not found: {self.symbols_path}"
            return
        try:
            with open(self.exe_path, "rb") as f:
                self._data = f.read()
            self._parse_pe()
            self._parse_symbols()
        except Exception as e:                # pragma: no cover — corrupt input
            self._data = None
            self._sections = []
            self._symbols = {}
            self.reason = f"failed to load image: {e}"
            return
        self.available = bool(self._sections and self._symbols)
        if not self.available:
            self.reason = "image parsed but empty"

    def _parse_pe(self):
        d = self._data
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        if d[pe:pe + 4] != b"PE\x00\x00":
            raise ValueError("not a PE image")
        _machine, nsec, _ts, _symtab, _nsym, opt_size, _chars = \
            struct.unpack_from("<HHIIIHH", d, pe + 4)
        # PE32 only: ImageBase sits at optional-header +28 and is 4 bytes.
        # PE32+ would put it at +24 as 8 bytes, and every section address we
        # computed from it would be silently wrong.
        magic = struct.unpack_from("<H", d, pe + 24)[0]
        if magic != 0x010B:
            raise ValueError(f"not a PE32 image (optional header magic 0x{magic:04X})")
        image_base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
        sec_off = pe + 24 + opt_size
        for i in range(nsec):
            o = sec_off + i * 40
            name = d[o:o + 8].rstrip(b"\x00").decode("ascii", "replace")
            vsize, vaddr, raw_size, raw_off = struct.unpack_from("<IIII", d, o + 8)
            self._sections.append(
                (name, image_base + vaddr, vsize, raw_off, raw_size))

    def _parse_symbols(self):
        with open(self.symbols_path, "r", errors="replace") as f:
            for line in f:
                if "=" not in line or ":0x" not in line:
                    continue
                m = _SYMBOL_RE.match(line.strip())
                if not m:
                    continue
                name, section, addr_hex, tail = m.groups()
                section = section.strip()
                if section in _CODE_SECTIONS:
                    continue
                tm = _TYPE_RE.search(tail)
                if tm and tm.group(1) != "object":
                    continue
                sm = _SIZE_RE.search(tail)
                size = int(sm.group(1), 16) if sm else 0
                self._symbols[name] = ImageSymbol(
                    name, int(addr_hex, 16), size, section)

    # ---- queries ---------------------------------------------------------

    def lookup(self, name):
        """Return the ImageSymbol for a data symbol name, or None."""
        return self._symbols.get(name)

    def read(self, address, size):
        """Bytes at a guest address, or None if the address is not in a section.

        Addresses inside a section's virtual size but past its file content --
        the .bss tail -- read as zeros, which is what the console's loader does.
        """
        if size <= 0 or self._data is None:
            return None
        for _name, start, vsize, raw_off, raw_size in self._sections:
            extent = max(vsize, raw_size)
            if not (start <= address < start + extent):
                continue
            offset = address - start
            if offset >= raw_size:
                return b"\x00" * size            # zero-fill tail (.bss)
            avail = min(size, raw_size - offset)
            out = self._data[raw_off + offset:raw_off + offset + avail]
            if avail < size:
                out = out + b"\x00" * (size - avail)
            return bytes(out)
        return None

    def symbol_bytes(self, name, size=None):
        """Image content of a named data symbol, or None if unknown/unmapped."""
        sym = self._symbols.get(name)
        if sym is None:
            return None
        want = size if size is not None else sym.size
        if want <= 0:
            return None
        return self.read(sym.address, want)

    def is_image_pointer(self, value):
        """True if `value` is an address inside the image."""
        for _name, start, vsize, _raw_off, raw_size in self._sections:
            if start <= value < start + max(vsize, raw_size):
                return True
        return False

    def contains_image_pointer(self, content):
        """True if any aligned word of `content` points into the image.

        Content like this cannot be honoured: the harness maps none of the
        image, so a seeded pointer aims at an on-demand zero page. Zero is the
        better answer -- it is the harness's standard "no object here", it is
        what every other pointer in the fixture is, and it keeps both sides on
        the same null-guarded path. In dc3, seeding pointers instead of skipping
        them cost six EQUIVALENT->DIVERGENT flips in a 40-unit A/B: `gNullStr`
        went from a null the SetType functions guard against to a live pointer
        into zeros, and they walked off into a string compare that never
        terminated.
        """
        for i in range(0, len(content) - 3, 4):
            word = struct.unpack_from(">I", content, i)[0]
            if self.is_image_pointer(word):
                return True
        return False


class _MissingImage:
    """Stand-in used when the image is unavailable; seeds nothing."""

    available = False

    def __init__(self, reason):
        self.reason = reason

    def lookup(self, name):
        return None

    def read(self, address, size):
        return None

    def symbol_bytes(self, name, size=None):
        return None

    def is_image_pointer(self, value):
        return False

    def contains_image_pointer(self, content):
        return False


def project_root():
    return os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))


def get_global_image(root=None, title=_DEFAULT_TITLE):
    """Process-wide cached GlobalImage.

    Returns an object with .available == False (and .reason set) when the
    image or symbol map is missing, so callers can stay unconditional. The
    harness then behaves exactly as it did before this module existed --
    degraded, but never wrong in a new way.

    ⚠ That graceful degradation is also a VACUITY HAZARD for anyone measuring
    the effect of seeding: an unavailable image produces "no change" that looks
    exactly like "seeding changed nothing". Assert `.available` explicitly
    before interpreting a null result -- `scripts/unicorn/image_selfcheck.py`
    does this.
    """
    root = root or project_root()
    key = (os.path.abspath(root), title)
    with _lock:
        img = _cached.get(key)
        if img is None:
            exe = os.path.join(root, "orig", title, _IMAGE_NAME)
            syms = os.path.join(root, "config", title, "symbols.txt")
            img = GlobalImage(exe, syms)
            if not img.available:
                img = _MissingImage(img.reason)
            _cached[key] = img
        return img
