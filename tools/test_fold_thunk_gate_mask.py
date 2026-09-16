#!/usr/bin/env python3
"""W16-EK: the fold-thunk gate's mask must follow COFF records, not form.

Guards three properties that were each measured WRONG before 2026-09-16:

1. ANTI-VACUITY.  `mask_word` used to zero the low 16 bits of every IMM16-form
   instruction unconditionally, so `stb r4,8(r3)`, `stb r4,0xc(r3)` and
   `stb r4,0x7ff(r3)` all compared EQUAL.  A gate cannot check a `+8` it has
   masked away.  An UNRELOCATED word must now survive whole.
2. ONE-SIDEDNESS.  Retail's relocation set used to be INFERRED from instruction
   form while ours was READ from COFF, so a relocation-free thunk refused with
   "relocated fields at different offsets: retail [0] vs ours []" -- pure
   asymmetry, after the words had already compared equal.
3. SCOPE.  The conditional mask is correct ONLY where exactly one side has
   relocation records (our COFF vs the retail image).  For LINKED-vs-LINKED
   (retail vs dc3, the FT3 homonym witness) form-inference is right, because
   neither side has records and the inference cancels.  Driving that off our
   COFF records flipped a 1,180-site pair from ADMIT to REFUSE.

Runs in milliseconds and needs no image, no build and no worklist.
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fold_thunk_gate import mask_word, compare          # noqa: E402

STB_8   = 0x98830008        # stb r4, 8(r3)
STB_C   = 0x9883000C        # stb r4, 0xc(r3)
STB_7FF = 0x988307FF        # stb r4, 0x7ff(r3)
BLR     = 0x4E800020
BL      = 0x48000000 | (0x40 & 0x03FFFFFC) | 1          # bl +0x40

fails = []


def check(cond, msg):
    if not cond:
        fails.append(msg)


# ---- 1. anti-vacuity: an unrelocated field is compared whole ---------------
check(mask_word(STB_8, False) == STB_8, "unrelocated stb must survive masking whole")
check(mask_word(STB_8, False) != mask_word(STB_C, False),
      "VACUOUS: stb +8 and stb +0xc compare equal unrelocated")
check(mask_word(STB_8, False) != mask_word(STB_7FF, False),
      "VACUOUS: stb +8 and stb +0x7ff compare equal unrelocated")
check(mask_word(BL, False) == BL, "unrelocated branch must survive masking whole")

# ---- 3. scope: a RELOCATED field (and the linked-vs-linked default) still
#         masks by form, or the homonym witness dies ------------------------
check(mask_word(STB_8, True) == mask_word(STB_C, True),
      "a RELOCATED imm16 field must still mask (linked-vs-linked cancels)")
check(mask_word(STB_8) == mask_word(STB_C),
      "the DEFAULT (relocated=True) must keep masking: it is the "
      "linked-vs-linked path used by homonym()/FT3")
check(mask_word(BL, True) == mask_word(BL ^ 0x40, True),
      "a RELOCATED branch displacement must still mask")

# ---- 2. one-sidedness: identical reloc-free bodies must COMPARE EQUAL ------
words = list(struct.unpack(">2I", bytes.fromhex("988300084e800020")))
ok, why = compare(words, {}, list(words), {})
check(ok, "identical relocation-free bodies must ADMIT, got: %s" % why)

# ...and the test must be able to FAIL, or it proves nothing ----------------
bad = [STB_C, BLR]
ok_bad, why_bad = compare(words, {}, bad, {})
check(not ok_bad, "a +0xc body must NOT compare equal to a +8 body")
check("masked words differ" in (why_bad or ""),
      "the refusal must name the differing word, got: %s" % why_bad)

# a relocation on our side that retail cannot resolve still refuses ---------
ok_u, why_u = compare(words, {0: None}, list(words), {0: "gSomething"})
check(not ok_u, "an unresolvable relocated field must refuse")

if fails:
    for f in fails:
        print("FAIL: %s" % f)
    sys.exit(1)
print("[ ok] fold_thunk_gate mask: anti-vacuity, symmetry and linked-vs-linked scope")
