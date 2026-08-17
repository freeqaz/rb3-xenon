"""The interior EH prefix belongs to the region that FOLLOWS it -- whatever it
is called.

MSVC X360 precedes every EH region with an unnamed 8-byte prefix, so a reader
that slices `[value_i, value_{i+1})` over function symbols bills the successor's
prefix to the preceding symbol.  `coff_bodies_ext` has fixed that twice: once by
handing the prefix back at all (2026-08-16), and once by asking the right
question (2026-08-17).  The 2026-08-16 gate was "is the SUCCESSOR funclet-named",
which is a proxy -- 3 of build 45410914's 6,141 interior prefixes precede an
ORDINARY function and the trim never fired for them.

What this file pins is therefore the DISCRIMINATOR, not just the arithmetic:
the `$EH*` marker first, the prefix's own byte+relocation signature second, the
successor's name never.  The `$M` and null-vector cases pin the mirror-image
defect -- a boundary rule loose enough to truncate a body at its prologue.
"""
import struct
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import coff_bodies_ext as cbe            # noqa: E402
import ident_body_channel as ibc         # noqa: E402

BLR = b"\x4e\x80\x00\x20"
MFLR = b"\x7d\x88\x02\xa6"
BL = b"\x48\x00\x00\x01"
ZERO8 = b"\x00" * 8

#: 96 bytes of plausible PPC that does not end in a nop or a zero word.
BODY_96 = (MFLR + b"\x94\x21\xff\xd0") + (b"\x81\x63\x00\x10" * 5) + BL \
    + (b"\x7c\x0b\x18\x00" * 15) + BLR
CATCH_40 = (b"\x3b\xe1\xff\x80" * 9) + BLR
assert len(BODY_96) == 96 and len(CATCH_40) == 40

REL_ADDR32 = 6          # the prefix's two pointer words
REL_BRANCH = 26         # the `bl` inside the body
REL_PAIR = 0x12         # IMAGE_REL_PPC_PAIR -- VirtualAddress is a DISPLACEMENT
BODY_BL_OFF = 28        # offset of BL within BODY_96

SCN_CNT_CODE = 0x20
SCN_LNK_COMDAT = 0x1000


def coff(sections) -> bytes:
    """Minimal PPC-BE COFF writer (no optional header, no aux records).

    sections: [{data, syms: [(name, value, type, storage_class)],
                relocs: [(off, type, symbol)], char}]
    """
    syms = []
    for i, s in enumerate(sections, start=1):
        for (nm, val, typ, scl) in s.get("syms", ()):
            syms.append((nm, val, i, typ, scl))
    known = {e[0] for e in syms}
    for s in sections:
        for (_o, _t, nm) in s.get("relocs", ()):
            if nm is not None and nm not in known:
                known.add(nm)
                syms.append((nm, 0, 0, 0x00, 2))       # UNDEF external
    idx = {e[0]: i for i, e in enumerate(syms)}

    nsec = len(sections)
    off = 20 + nsec * 40
    raw_ptr, rel_ptr = [], []
    for s in sections:
        raw_ptr.append(off if s["data"] else 0)
        off += len(s["data"])
    for s in sections:
        r = s.get("relocs", ())
        rel_ptr.append(off if r else 0)
        off += 10 * len(r)
    symptr = off

    symtab, strtab = bytearray(), bytearray(b"\0\0\0\0")
    for (nm, val, secnum, typ, scl) in syms:
        raw = nm.encode("ascii")
        if len(raw) <= 8:
            fld = raw + b"\0" * (8 - len(raw))
        else:
            fld = struct.pack("<II", 0, len(strtab))
            strtab += raw + b"\0"
        symtab += fld + struct.pack("<IhHBB", val, secnum, typ, scl, 0)
    struct.pack_into("<I", strtab, 0, len(strtab))

    out = bytearray(struct.pack("<HHIIIHH", 0x01F2, nsec, 0, symptr,
                                len(syms), 0, 0))
    for i, s in enumerate(sections):
        out += b".text\0\0\0" + struct.pack(
            "<IIIIII", 0, 0, len(s["data"]), raw_ptr[i], rel_ptr[i], 0)
        out += struct.pack("<HHI", len(s.get("relocs", ())), 0,
                           s.get("char", SCN_CNT_CODE | SCN_LNK_COMDAT))
    for s in sections:
        out += s["data"]
    for s in sections:
        for (o, t, nm) in s.get("relocs", ()):
            out += struct.pack("<IIH", o, idx.get(nm, 0), t)
    return bytes(out) + bytes(symtab) + bytes(strtab)


def eh_section(*, eh_marker=True, zero_prefix=True, prefix_relocs=True,
               successor="__catch$1", interior_label=True, pair_decoy=False):
    """The layout MSVC emits: `[8B prefix][f, 96B][8B prefix][successor, 40B]`.

    Each keyword removes (or plants) one discriminator, so a test can ask which
    one is load-bearing.
    """
    prefix = ZERO8 if zero_prefix else (MFLR + BLR)
    data = ZERO8 + BODY_96 + prefix + CATCH_40
    syms = [("f", 8, 0x20, 2), (successor, 112, 0x20, 3)]
    if eh_marker:
        syms.append(("$EH00000", 104, 0x00, 3))
    if interior_label:
        # A class-6 `$M` debug label inside the body. Bounding a slice on one of
        # these truncates the function at its prologue -- 463,619 of them exist
        # in build 45410914, so a loose rule here is not a corner case.
        syms.append(("$M42", 40, 0x00, 6))
    relocs = [(BODY_BL_OFF + 8, REL_BRANCH, "callee")]
    if prefix_relocs:
        relocs = [(0, REL_ADDR32, "__CxxFrameHandler"),
                  (4, REL_ADDR32, "__ehfuncinfo$f")] + relocs + [
                  (104, REL_ADDR32, "__CxxFrameHandler"),
                  (108, REL_ADDR32, "__ehfuncinfo$f")]
    if pair_decoy:
        # A PAIR pseudo-reloc whose "VirtualAddress" (a DISPLACEMENT) collides
        # with the interior prefix. Appended LAST, so a last-writer-wins reloc
        # map would let it overwrite `__CxxFrameHandler` and kill the trim.
        relocs = relocs + [(104, REL_PAIR, "callee"), (108, REL_PAIR, "callee")]
    return {"data": data, "syms": syms, "relocs": relocs}


def sizes(obj, tmp_path, name="o.obj"):
    p = tmp_path / name
    p.write_bytes(obj)
    return {n: len(b) for n, b, _rl, _e in cbe.function_bodies_ext(str(p))}


def ident_sizes(obj, tmp_path, name="o.obj"):
    p = tmp_path / name
    p.write_bytes(obj)
    return {n: len(b) for n, b, _rl, _sel in ibc.function_slices(p)}


def both(obj, tmp_path):
    """Sizes from `coff_bodies_ext` AND `ident_body_channel`, asserted equal.

    The two readers are independent implementations over independent COFF
    parsers; every case below is checked through both so they cannot drift.
    """
    a = sizes(obj, tmp_path, "a.obj")
    b = ident_sizes(obj, tmp_path, "b.obj")
    assert a == b, (a, b)
    return a


# --------------------------------------------------------------------------
# positive control
# --------------------------------------------------------------------------
def test_interior_eh_prefix_is_handed_back_to_the_successor(tmp_path):
    """An EH-bearing function reads its own 96 bytes, not 104."""
    s = both(coff([eh_section()]), tmp_path)
    assert s == {"f": 96, "__catch$1": 40}


def test_the_eh_marker_alone_is_enough(tmp_path):
    """Rule 1: the `$EH*` symbol the build injects, with no byte or reloc
    evidence at all, still yields the right extent."""
    assert both(coff([eh_section(prefix_relocs=False)]), tmp_path)["f"] == 96


def test_the_byte_and_reloc_signature_alone_is_enough(tmp_path):
    """Rule 2, the no-marker fallback: an unpatched obj, or a build from before
    `obj_eh_boundary_patcher.py` existed."""
    assert both(coff([eh_section(eh_marker=False)]), tmp_path)["f"] == 96


# --------------------------------------------------------------------------
# the defect this lane fixed
# --------------------------------------------------------------------------
@pytest.mark.parametrize("successor", [
    "?IsInDuplicationSet@DuplicatedObject@Quazal@@QBA_NVDOHandle@2@@Z",
    "??1KeyedChecksumAlgorithm@Quazal@@UAA@XZ",
    "?g@@YAXXZ",
])
@pytest.mark.parametrize("kw", [{}, {"eh_marker": False}])
def test_an_ordinary_function_can_own_the_prefix(successor, kw, tmp_path):
    """The successor need not be a funclet. Both real sites on build 45410914
    are of this shape, and the successor-name gate the 2026-08-16 fix shipped
    read them 8 bytes long."""
    s = both(coff([eh_section(successor=successor, **kw)]), tmp_path)
    assert s["f"] == 96, (successor, kw, s)


def test_the_successor_name_is_never_consulted(tmp_path):
    """Same bytes, same markers, same relocations, different successor NAME:
    the extent must not move. This is the property, stated directly."""
    got = {nm: both(coff([eh_section(successor=nm)]), tmp_path)["f"]
           for nm in ("__catch$1", "__unwindfunclet$3", "?g@@YAXXZ", "_plainC")}
    assert set(got.values()) == {96}, got


# --------------------------------------------------------------------------
# null vector and negative controls
# --------------------------------------------------------------------------
def test_a_body_that_genuinely_ends_in_two_zero_words_is_not_trimmed(tmp_path):
    """No marker, no EH successor, no relocation pair: the zero tail is the
    body's own."""
    data = BODY_96[:-8] + ZERO8
    obj = coff([{"data": data, "syms": [("g", 0, 0x20, 2)], "relocs": []}])
    assert both(obj, tmp_path) == {"g": 96}


def test_zero_tail_before_a_catch_without_marker_or_relocs_is_not_trimmed(tmp_path):
    """Structure plus zero bytes is NOT evidence.

    This is the one place the 2026-08-17 semantics are STRICTER than the
    2026-08-16 ones, which trimmed on (funclet name + zero bytes) alone. Swept
    over all 4,136 objects of build 45410914 the two rules disagree on 3 slices,
    all in this direction's opposite: nothing in the build trims here.
    """
    sec = eh_section(eh_marker=False, prefix_relocs=False)
    assert both(coff([sec]), tmp_path)["f"] == 104


def test_a_non_zero_prefix_is_not_trimmed(tmp_path):
    """Real instructions where the prefix would be: leave the slice alone."""
    sec = eh_section(eh_marker=False, zero_prefix=False)
    assert both(coff([sec]), tmp_path)["f"] == 104


def test_an_interior_M_label_never_truncates_a_body(tmp_path):
    """`$M#####` is class 6 and sits INSIDE a body. Admitting one as a boundary
    would cut this function at 32 instead of 96."""
    for kw in ({}, {"eh_marker": False},
               {"eh_marker": False, "prefix_relocs": False}):
        s = both(coff([eh_section(interior_label=True, **kw)]), tmp_path)
        assert s["f"] >= 96, (kw, s)


def test_an_M_label_at_the_slice_end_is_not_an_eh_marker(tmp_path):
    """The `$M` label the compiler DOES leave at a function's true end (class 6)
    must not be read as the class-3 `$EH` boundary. Without the marker and
    without the reloc pair there is no evidence, so no trim."""
    sec = eh_section(eh_marker=False, prefix_relocs=False)
    sec["syms"].append(("$M99", 104, 0x00, 6))
    assert both(coff([sec]), tmp_path)["f"] == 104


def test_the_target_split_is_untouched(tmp_path):
    """Value 0, no funclets, no prefixes: the artifact is ONE-SIDED, and the
    sweep confirms it -- 0 of 6,141 trims land on a target obj."""
    obj = coff([{"data": BODY_96, "syms": [("f", 0, 0x20, 2)],
                 "relocs": [(BODY_BL_OFF, REL_BRANCH, "callee")]}])
    assert both(obj, tmp_path) == {"f": 96}


# --------------------------------------------------------------------------
# PAIR: a displacement is not an address
# --------------------------------------------------------------------------
def test_a_pair_pseudo_reloc_cannot_veto_the_fallback(tmp_path):
    """A type-18 PAIR record's `VirtualAddress` is the paired half of a
    displacement, not an offset in the section. Indexing a reloc map on it lets
    a PAIR record squat on the prefix offset and hide `__CxxFrameHandler`."""
    sec = eh_section(eh_marker=False, pair_decoy=True)
    assert both(coff([sec]), tmp_path)["f"] == 96


def test_a_pair_pseudo_reloc_cannot_veto_the_fallback_ident(tmp_path):
    """Same decoy read through `ident_body_channel` alone, so the reader that
    carried this defect is pinned by name and not only via `both`."""
    sec = eh_section(eh_marker=False, pair_decoy=True)
    assert ident_sizes(coff([sec]), tmp_path)["f"] == 96


def test_a_duplicate_reloc_offset_is_first_writer_wins(tmp_path):
    """Two records at one offset: the map must keep the first. A later
    non-PAIR record at the prefix offset would otherwise hide the personality
    routine just as a PAIR did."""
    sec = eh_section(eh_marker=False)
    sec["relocs"] = sec["relocs"] + [(104, REL_ADDR32, "callee")]
    assert both(coff([sec]), tmp_path)["f"] == 96


# --------------------------------------------------------------------------
# the trim itself
# --------------------------------------------------------------------------
def test_the_trim_can_only_shorten_and_never_empties_a_slice():
    n = cbe.EH_PREFIX_BYTES
    for marks in ({96}, set()):
        assert cbe.eh_prefix_end(104, 8, marks, b"\0" * 200, {}) in (104, 104 - n)
    # a marker that would empty (or invert) the slice is refused
    assert cbe.eh_prefix_end(16, 8, {8}, b"\0" * 200, {}) == 16
    assert cbe.eh_prefix_end(8, 8, {0}, b"\0" * 200, {}) == 8


def test_eh_boundaries_keys_on_name_and_class_and_type():
    syms = [{"value": 104, "name": "$EH00000", "storage": 3, "type": 0},
            {"value": 40, "name": "$M42", "storage": 6, "type": 0},
            {"value": 200, "name": "$EHbad", "storage": 2, "type": 0},
            {"value": 300, "name": "$EHfn", "storage": 3, "type": 0x20},
            {"value": 400, "name": "?f@@YAXXZ", "storage": 3, "type": 0}]
    assert cbe.eh_boundaries(syms) == {104}
