"""Real bodies for MSVC's Xenon register save/restore helpers.

WHY THIS EXISTS
---------------
Xenon MSVC does not open-code its callee-save spills. It calls out-of-line
helpers, and a typical prologue/epilogue pair looks like this (App::Poll):

    mflr  r12                 ; the return address, handed to the helper in r12
    bl    __savegprlr_26      ; std r26..r31 below r1, then stw r12,-0x8(r1)
    addi  r12, r1, -0x38      ; r12 = bottom of the GPR save area
    bl    __savefpr_28        ; stfd f28..f31 below r12
    ...
    addi  r12, r1, -0x38
    bl    __restfpr_28        ; lfd f28..f31   (note: LK=1, but a RESTORE)
    b     __restgprlr_26      ; ld r26..r31, then lwz r12,-0x8(r1); mtlr; blr

Those helper names are external REL24 targets, and the harness gave every
external REL24 target the same `li r3,0; blr` TRAMPOLINE_STUB. Two things
break at once:

  1. `bl __savegprlr_N` executes `li r3,0` -- the incoming `this`/arg0 is
     destroyed at the *second instruction of the function*, on both sides, so
     the whole body then runs against a null object.

  2. `b __restgprlr_N` is a TAIL branch, and the real helper is the thing that
     reloads LR from -0x8(r1). The stub does not, so its `blr` returns to
     whatever LR holds -- which, after any `bl` in the body, is an address
     just past the last call. The function re-enters its own tail and spins
     until the instruction cap. Measured: DxRnd::D3DFormatForBitmap cycles
     +0xB0 -> +0x148 -> +0x14C -> +0x150 -> +0xB0; IsValidSwipePosition runs
     its 3-instruction cycle 16,616 times in a 50k budget.

The consequence is that essentially every helper-using function that makes at
least one call was an infinite loop under emulation, and its recorded verdict
described the spin, not the code.

DESIGN: SYNTHESISE THE REAL BODIES, DO NOT REWRITE CALL SITES
------------------------------------------------------------
Two earlier prototypes (scripts/cap_helpers.py on fix/cap-exhausted-decomp-
mining, prologue_helper_probe.py on fix/pinpoint-divergences-a) rewrote the
*call sites* instead: `bl __savegprlr_N` -> `stw r12,-0x8(r1)`, the tail
branch -> an appended `lwz/mtlr/blr` thunk, FPR helpers -> `nop`. That
restores control flow, but it silently drops the save/restore of r14..r31 and
f14..f31 -- which is invisible for a leaf, and wrong the moment a co-loaded
callee is involved: the callee's spill becomes a nop, so it keeps the caller's
r29 when it returns and the caller's locals are corrupted. It also edits the
code under test, which is exactly the thing the oracle is measuring.

This module instead installs the helpers' REAL bodies at fixed addresses in a
dedicated HELPER region and points the relocation at them. The code under test
is untouched; the call goes where it went on the console and does what it did
there. The bodies are short, fixed, and fully determined -- they are recovered
byte-for-byte from the shipped image in the tests.

std/ld ARE REWRITTEN, EXACTLY AS FUNCTION BODIES ALREADY ARE
------------------------------------------------------------
The helpers use `std`/`ld` (PPC64 DS-form), which Unicorn's PPC32 mode
rejects with UC_ERR_EXCEPTION. The harness has always handled that for
function bodies by rewriting them to `stw`/`lwz` at the same address
(patcher.rewrite_ppc64_insns). We apply the identical rewrite here, so a
function that open-codes its spills and one that calls the helper end up
storing the same 32-bit halves to the same addresses. `emulated_body()` is
literally `rewrite_ppc64_insns(raw_body())`, and a test asserts it.

CLASSIFY ON THE SYMBOL NAME, NEVER ON THE LINK BIT
--------------------------------------------------
`__restfpr_N` is reached by `bl` (LK=1) and `__restgprlr_N` by `b` (LK=0) in
the *same* epilogue. The first prototype keyed its rewrite on the link bit,
read `bl __restfpr_28` as a save, wrote garbage into the LR slot, and
manufactured UC_ERR_EXCEPTION `error`-class divergences in precisely the
functions that spill FPRs. Nothing in this module looks at LK: the symbol name
is the only classifier, and test_save_helpers pins that.

VMX: APPROXIMATED, AND SAID SO
------------------------------
`__savevmx_N`/`__restvmx_N` move vector registers with `stvx`/`lvx` (and, for
N >= 64, VMX128 opcodes Unicorn does not implement at all). The harness models
no vector state and compares none, so there is nothing to preserve. Their one
GPR-visible effect is that they leave **r11** holding the last offset they
used -- `li r11, -0x10` immediately precedes the final vector op in BOTH banks
of both routines (0x82E5DEB8/0x82E5E0BC in __savevmx, 0x82E5E150/
0x82E5E354 in __restvmx) -- and the
base register they index off is r12, which they do not write. We reproduce the
r11 value and return. This is an approximation, unlike the GPR/FPR bodies, but
it is strictly better than the old stub, which returned to a stale LR and
zeroed r3. 39 REL24 references in the original objects, 2 in the decomp, touch
these.

The first cut of this module wrote `li r0` here, having misread rD in
`3960FFF0` as 0. Nothing observable depended on it -- VMX appears in two units
only (synth_xbox/FFT, xdk/nuiapi/headtracker) and both swept functions there
were EQUIVALENT either way -- but it was wrong against the image, which is the
standard the rest of this module is held to.
"""

import re
import struct

from .memory_map import HELPER_BASE, HELPER_SLOT_SIZE, REGION_SIZE

# `__savegprlr_29`, `__restfpr_14`, `__savevmx_127`, and the bare forms
# (`__savegprlr`), which are the first entry point of their bank.
_HELPER_RE = re.compile(r"^__(save|rest)(gprlr|fpr|vmx)(?:_(\d+))?$")

# Entry-point registers per bank, in the order the helper region lays them out.
_BANK_REGS = {
    "gprlr": tuple(range(14, 32)),
    "fpr": tuple(range(14, 32)),
    # VMX has two banks in one routine: v14..v31, then the VMX128 range
    # v64..v127. Both end their sequence at offset -0x10.
    "vmx": tuple(range(14, 32)) + tuple(range(64, 128)),
}

_NOP = 0x60000000
_BLR = 0x4E800020
_MTLR_R12 = 0x7D8803A6


def _std(rs, offset, ra):
    """std rS, offset(rA) — DS-form, offset must be a multiple of 4."""
    return (62 << 26) | (rs << 21) | (ra << 16) | (offset & 0xFFFC)


def _ld(rd, offset, ra):
    """ld rD, offset(rA)"""
    return (58 << 26) | (rd << 21) | (ra << 16) | (offset & 0xFFFC)


def _stw(rs, offset, ra):
    return (36 << 26) | (rs << 21) | (ra << 16) | (offset & 0xFFFF)


def _lwz(rd, offset, ra):
    return (32 << 26) | (rd << 21) | (ra << 16) | (offset & 0xFFFF)


def _stfd(frs, offset, ra):
    return (54 << 26) | (frs << 21) | (ra << 16) | (offset & 0xFFFF)


def _lfd(frd, offset, ra):
    return (50 << 26) | (frd << 21) | (ra << 16) | (offset & 0xFFFF)


def _li(rd, imm):
    return (14 << 26) | (rd << 21) | (imm & 0xFFFF)


def classify(symbol):
    """(op, bank, n) for a save/restore helper symbol, else None.

    op is "save" or "rest"; bank is "gprlr", "fpr" or "vmx"; n is the first
    register the entry point covers. The bare name (`__savegprlr`) is the
    bank's first entry point, matching the linker map.
    """
    m = _HELPER_RE.match(symbol or "")
    if not m:
        return None
    op, bank, num = m.group(1), m.group(2), m.group(3)
    regs = _BANK_REGS[bank]
    n = int(num) if num is not None else regs[0]
    if n not in regs:
        return None
    return op, bank, n


def is_helper(symbol):
    """True if `symbol` names one of the register save/restore helpers."""
    return classify(symbol) is not None


def _slot_index(op, bank, n):
    base = 0
    for b in ("gprlr", "fpr", "vmx"):
        for o in ("save", "rest"):
            if (o, b) == (op, bank):
                return base + _BANK_REGS[bank].index(n)
            base += len(_BANK_REGS[b])
    raise KeyError((op, bank, n))       # pragma: no cover — guarded by classify


def helper_address(symbol):
    """Fixed guest address of a helper's body, or None if not a helper.

    The layout is a pure function of the symbol name — identical on both
    sides, unlike the trampoline slots, which are handed out in each side's
    own relocation order.
    """
    kind = classify(symbol)
    if kind is None:
        return None
    return HELPER_BASE + _slot_index(*kind) * HELPER_SLOT_SIZE


def raw_body(symbol):
    """The helper's body exactly as the shipped image has it (PPC64 form).

    GPR save area, relative to the incoming r1:
        r14 at -0x98 ... r31 at -0x10, then the caller's LR (in r12) at -0x8.
    FPR save area, relative to r12, which the caller has already pointed at
    the bottom of the GPR area:
        f14 at -0x90 ... f31 at -0x8.
    """
    kind = classify(symbol)
    if kind is None:
        return None
    op, bank, n = kind
    words = []
    if bank == "gprlr":
        for r in range(n, 32):
            off = -8 * (33 - r)
            words.append(_std(r, off, 1) if op == "save" else _ld(r, off, 1))
        if op == "save":
            words.append(_stw(12, -0x8, 1))
        else:
            words.append(_lwz(12, -0x8, 1))
            words.append(_MTLR_R12)
    elif bank == "fpr":
        for f in range(n, 32):
            off = -8 * (32 - f)
            words.append(_stfd(f, off, 12) if op == "save" else _lfd(f, off, 12))
    else:                                # vmx — see the module docstring
        words.append(_li(11, -0x10))
    words.append(_BLR)
    return b"".join(struct.pack(">I", w) for w in words)


def emulated_body(symbol):
    """`raw_body` with std/ld lowered to stw/lwz, as Unicorn PPC32 needs.

    Identical to patcher.rewrite_ppc64_insns(raw_body(symbol)); imported
    lazily so patcher can import this module at load time.
    """
    raw = raw_body(symbol)
    if raw is None:
        return None
    from .patcher import rewrite_ppc64_insns
    buf = bytearray(raw)
    rewrite_ppc64_insns(buf)
    return bytes(buf)


def all_helper_symbols():
    """Every entry-point symbol the region provides a body for."""
    out = []
    for bank in ("gprlr", "fpr", "vmx"):
        for op in ("save", "rest"):
            for n in _BANK_REGS[bank]:
                out.append(f"__{op}{bank}_{n}")
    return out


def build_helper_region():
    """The HELPER region's contents: every body at its fixed slot."""
    region = bytearray(REGION_SIZE)
    for sym in all_helper_symbols():
        body = emulated_body(sym)
        off = helper_address(sym) - HELPER_BASE
        if off + len(body) > REGION_SIZE:       # pragma: no cover — layout bug
            raise ValueError(f"helper region overflow at {sym}")
        if len(body) > HELPER_SLOT_SIZE:        # pragma: no cover — layout bug
            raise ValueError(f"{sym} body {len(body)}B exceeds slot "
                             f"{HELPER_SLOT_SIZE}B")
        region[off:off + len(body)] = body
    return bytes(region)


_region_cache = []


def helper_region():
    """Cached HELPER region image.

    Built on first use, not at import: `patcher` imports this module at load
    time and `emulated_body` needs `patcher.rewrite_ppc64_insns`, so building
    eagerly closes the cycle.
    """
    if not _region_cache:
        _region_cache.append(build_helper_region())
    return _region_cache[0]
