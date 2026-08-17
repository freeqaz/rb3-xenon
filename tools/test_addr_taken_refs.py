"""Tests for the reference scan behind the CF2/FT2 "nothing reaches this" discredit.

The property under test is the one whose absence shipped a false PASS: a
function whose ADDRESS IS TAKEN must not read as unreferenced.  `Image.fanin()`
counts `b`/`bl` targets, so a static factory registered by address --
`lis rX,hi` / `addi rD,rX,lo` -- has fan-in 0 and always will.  CF2 read that as
"nothing in the image references this", which is a different claim, and admitted
`?NewObject@FxSendChorus360@@SA...` on it (rb3-xenon 9156c659).

Everything here runs on a SYNTHETIC PE built in-memory.  A suite that skipped
when `orig/` is absent would be unable to pin the very predicate it exists for.
The three legs the fix owes are the three cases below: address-taken shows
nonzero, branch-only keeps its old count, and a genuinely unreferenced function
stays 0 -- the null vector, without which "refs() finds references" is
unfalsifiable.
"""
import struct
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from wrong_callee_triage import Image  # noqa: E402

BASE = 0x82000000
TEXT_VA = 0x00010000            # RVA
RDATA_VA = 0x00020000
SECT_RAW = 0x400


def _pe(text_words, rdata_words=(), text_va=TEXT_VA):
    """Minimal PE32 with a `.text` and a `.rdata` section, big-endian payloads."""
    text = b"".join(struct.pack(">I", w) for w in text_words)
    rdata = b"".join(struct.pack(">I", w) for w in rdata_words)
    secs = [(b".text", text_va, text), (b".rdata", RDATA_VA, rdata)]

    lfanew = 0x80
    opt = b"\x0b\x01" + b"\0" * 26 + struct.pack("<I", BASE) + b"\0" * 40
    hdr_end = lfanew + 4 + 20 + len(opt) + 40 * len(secs)
    raw0 = (hdr_end + SECT_RAW - 1) // SECT_RAW * SECT_RAW

    tbl, body, off = b"", b"", raw0
    for name, va, payload in secs:
        pad = (len(payload) + SECT_RAW - 1) // SECT_RAW * SECT_RAW
        tbl += (name.ljust(8, b"\0")
                + struct.pack("<IIII", len(payload), va, pad, off)
                + b"\0" * 16)
        body += payload.ljust(pad, b"\0")
        off += pad

    dos = b"MZ".ljust(0x3C, b"\0") + struct.pack("<I", lfanew)
    dos = dos.ljust(lfanew, b"\0")
    coff = struct.pack("<HHIIIHH", 0x1F2, len(secs), 0, 0, 0, len(opt), 0x102)
    return (dos + b"PE\0\0" + coff + opt + tbl).ljust(raw0, b"\0") + body


class _Blob:
    """Stands in for the pathlib.Path `Image.__init__` reads."""

    def __init__(self, blob):
        self._blob = blob

    def read_bytes(self):
        return self._blob


def img(text_words, rdata_words=(), text_va=TEXT_VA):
    return Image(_Blob(_pe(text_words, rdata_words, text_va)))


# --- instruction encoders ---------------------------------------------------
def lis(rt, hi):
    return (15 << 26) | (rt << 21) | (0 << 16) | (hi & 0xFFFF)


def addi(rd, ra, lo):
    return (14 << 26) | (rd << 21) | (ra << 16) | (lo & 0xFFFF)


def ori(rd, ra, lo):
    return (24 << 26) | (rd << 21) | (ra << 16) | (lo & 0xFFFF)


def lwz(rt, ra, d):
    return (32 << 26) | (rt << 21) | (ra << 16) | (d & 0xFFFF)


def bl(delta):
    return (18 << 26) | (delta & 0x03FFFFFC) | 1


NOP = 0x60000000                  # ori r0, r0, 0
BLR = 0x4E800020

TEXT = BASE + TEXT_VA


def ha_l(target):
    """(@ha, @l) such that `lis rX,@ha; addi rD,rX,@l` materialises `target`."""
    lo = target & 0xFFFF
    return ((target >> 16) + (1 if lo & 0x8000 else 0)) & 0xFFFF, lo


# ---------------------------------------------------------------------------
# 1. AN ADDRESS-TAKEN FUNCTION MUST SHOW NONZERO REFS
# ---------------------------------------------------------------------------
def test_address_taken_function_is_invisible_to_fanin_but_seen_by_refs():
    """The FxSendChorus360 shape: registered by address, branched to never."""
    factory = TEXT + 0x40
    hi, lo = ha_l(factory)
    i = img([BLR] * 4 + [lis(11, hi), addi(4, 11, lo), bl(0x100), BLR]
            + [NOP] * 8 + [BLR])
    assert i.fanin()[factory] == 0, "a taken address is never branched to"
    assert i.addr_taken()[factory] == 1
    assert i.refs()[factory] == 1
    d = i.refs_detail()
    assert (d["branch"][factory], d["addr_taken"][factory], d["data_ptr"][factory]) == (0, 1, 0)


def test_ori_form_is_seen_too():
    """`lis`/`ori` builds the value unsigned -- no @ha bias."""
    factory = TEXT + 0x40
    i = img([lis(11, factory >> 16), ori(4, 11, factory & 0xFFFF)] + [NOP] * 20)
    assert i.addr_taken()[factory] == 1


def test_negative_low_half_uses_ha_arithmetic():
    """lo >= 0x8000 makes `addi` subtract -- the @ha carry the live case needed.

    0x82B5ADC0 was built as `lis r11,0x82B6` / `addi r4,r11,0xADC0`, and reading
    0xADC0 as unsigned lands 0x10000 away from the function.
    """
    text_va = 0x0000B000           # puts every address in this image at lo >= 0x8000
    factory = BASE + text_va + 0x40
    hi, lo = ha_l(factory)
    assert lo & 0x8000 and hi == ((factory >> 16) + 1) & 0xFFFF
    i = img([lis(11, hi), addi(4, 11, lo)] + [NOP] * 40 + [BLR], text_va=text_va)
    assert i.addr_taken()[factory] == 1


def test_intervening_instruction_does_not_break_the_pair():
    """Retail's site has `lwz r3,0(r3)` between the `lis` and the `addi`."""
    factory = TEXT + 0x40
    hi, lo = ha_l(factory)
    i = img([lis(11, hi), lwz(3, 3, 0), addi(4, 11, lo)] + [NOP] * 20)
    assert i.addr_taken()[factory] == 1


def test_pointer_word_in_rdata_is_a_reference():
    """A vtable slot or registration table: the vtable-dispatch half of the hazard."""
    virt = TEXT + 0x20
    i = img([BLR] * 16, rdata_words=[0, virt, 0])
    assert i.fanin()[virt] == 0
    assert i.refs()[virt] == 1
    assert i.refs_detail()["data_ptr"][virt] == 1


# ---------------------------------------------------------------------------
# 2. A BRANCH-ONLY FUNCTION KEEPS ITS OLD COUNT
# ---------------------------------------------------------------------------
def test_branch_only_callee_keeps_its_fanin_and_refs_equals_it():
    callee = TEXT + 0x30
    i = img([bl(callee - TEXT), bl(callee - (TEXT + 4)), bl(callee - (TEXT + 8))]
            + [NOP] * 16)
    assert i.fanin()[callee] == 3
    assert i.addr_taken()[callee] == 0
    assert i.refs()[callee] == 3, "refs() must not double-count a plain call site"


def test_fanin_is_unchanged_by_the_new_scan_on_a_mixed_image():
    """`fanin()` KEEPS its meaning: fold-thunk thresholds ride on it."""
    callee = TEXT + 0x40
    hi, lo = ha_l(callee)
    i = img([bl(callee - TEXT), lis(11, hi), addi(4, 11, lo)] + [NOP] * 20,
            rdata_words=[callee])
    assert i.fanin()[callee] == 1
    assert i.refs()[callee] == 3
    d = i.refs_detail()
    assert (d["branch"][callee], d["addr_taken"][callee], d["data_ptr"][callee]) == (1, 1, 1)


# ---------------------------------------------------------------------------
# 3. THE NULL VECTOR -- a truly unreferenced function stays 0
# ---------------------------------------------------------------------------
def test_unreferenced_function_stays_zero():
    """Without this the fix is unfalsifiable: a scan that finds refs everywhere
    discredits nothing and CF2 would simply never fire."""
    orphan = TEXT + 0x38
    i = img([BLR] * 32, rdata_words=[0, 0, 0])
    assert i.fanin()[orphan] == 0
    assert i.addr_taken()[orphan] == 0
    assert i.data_pointers()[orphan] == 0
    assert i.refs()[orphan] == 0


def test_empty_image_is_empty_not_a_crash():
    i = img([], rdata_words=[])
    assert i.refs() == {} or sum(i.refs().values()) == 0


def test_immediate_pair_landing_outside_text_is_not_a_reference():
    """A data address built the same way must not be charged to `.text`."""
    data = BASE + RDATA_VA + 0x10
    hi, lo = ha_l(data)
    i = img([lis(11, hi), addi(4, 11, lo)] + [NOP] * 20)
    assert i.addr_taken()[data] == 0
    assert sum(i.addr_taken().values()) == 0


def test_unaligned_reconstruction_is_rejected():
    tgt = TEXT + 0x22          # not 4-aligned: never a function start
    hi, lo = ha_l(tgt)
    i = img([lis(11, hi), addi(4, 11, lo)] + [NOP] * 20)
    assert i.addr_taken()[tgt] == 0


def test_window_bounds_the_pairing():
    """A pair further apart than the window is a KNOWN MISS, not a silent one."""
    factory = TEXT + 0x100
    hi, lo = ha_l(factory)
    body = [lis(11, hi)] + [NOP] * 30 + [addi(4, 11, lo)] + [NOP] * 40
    i = img(body)
    assert i.addr_taken(window=8)[factory] == 0
    assert i.addr_taken(window=64)[factory] == 1


def test_clobbered_register_does_not_resurrect_a_stale_lis():
    """`lwz r11,0(r3)` overwrites the base; the later `addi` is unrelated."""
    factory = TEXT + 0x40
    hi, lo = ha_l(factory)
    i = img([lis(11, hi), lwz(11, 3, 0), addi(4, 11, lo)] + [NOP] * 20)
    assert i.addr_taken()[factory] == 0


# ---------------------------------------------------------------------------
# 4. THE SAME-COMDAT SPAN CHECK (the second, unexercised hazard)
# ---------------------------------------------------------------------------
class _FakeRetail:
    """Just enough of comdat_fold_gate.Retail to exercise `span_extents`."""

    def __init__(self, size, byva, img_):
        self.size, self.byva, self.img = size, byva, img_


@pytest.fixture
def span_extents():
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import comdat_fold_gate as G

    def call(size, va, nbytes, byva=None):
        return G.Retail.span_extents(_FakeRetail(size, byva or {}, None), va, nbytes)
    return call


def test_span_within_the_function_extent_is_fine(span_extents):
    ok, why = span_extents({0x1000: 0x40}, 0x1000, 0x40)
    assert ok, why


def test_span_shorter_than_the_function_refuses(span_extents):
    ok, why = span_extents({0x1000: 0x40}, 0x1000, 0x20)
    assert not ok and "SHORTER" in why


def test_exact_consecutive_chain_is_accepted(span_extents):
    """The live shape: 0x823864A8/60 + 0x823864E4/44 == our 104-byte COMDAT."""
    ok, why = span_extents({0x823864A8: 0x3C, 0x823864E4: 0x2C}, 0x823864A8, 104)
    assert ok, why
    assert "2 consecutive" in why


def test_a_gap_in_the_chain_refuses(span_extents):
    ok, why = span_extents({0x1000: 0x40}, 0x1000, 0x80)
    assert not ok and "no symbols.txt extent" in why


def test_an_overrunning_chain_refuses(span_extents):
    ok, why = span_extents({0x1000: 0x40, 0x1040: 0x40}, 0x1000, 0x60)
    assert not ok and "overruns" in why


def test_a_named_interior_function_refuses(span_extents):
    """THE hazard: the widened span swallowing an adjacent, unrelated function."""
    ok, why = span_extents({0x1000: 0x40, 0x1040: 0x40}, 0x1000, 0x80,
                           byva={0x1040: "?SomethingElse@@YAXXZ"})
    assert not ok and "swallows" in why and "SomethingElse" in why


def test_a_named_EH_funclet_interior_is_accepted(span_extents):
    """`__unwind$79385` at 0x82b5ae14 IS the survivor's tail, not another function.

    Without this the span check refuses the FxSendChorus360 pair for a reason
    that is not true, and the CF2 refusal that IS true never gets to fire.
    """
    ok, why = span_extents({0x82B5ADC0: 0x54, 0x82B5AE14: 0x28}, 0x82B5ADC0, 124,
                           byva={0x82B5AE14: "__unwind$79385"})
    assert ok, why


def test_missing_extent_refuses(span_extents):
    ok, why = span_extents({}, 0x1000, 0x40)
    assert not ok and "no symbols.txt extent" in why
