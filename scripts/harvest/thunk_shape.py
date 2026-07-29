#!/usr/bin/env python3
"""thunk_shape -- the vetted MSVC-X360 adjustor-thunk primitives, in ONE place.

This module is the single definition of the four facts every thunk-identity tool
in `scripts/harvest/` needs.  It was extracted verbatim from
`thunk_identity_namer.py` (landed `26284d0d`), whose derivation was validated
per-class against retail machine code:

    BandSwatch   (-4,600)(.,320)(.,324)(.,336)(.,340)  == ??_EBandSwatch
    BandList     (-4,856)(.,320)(.,324)(.,336)(.,340)  == ??_EBandList
    UIList       (-4,576)(.,320)(.,324)(.,336)(.,340)  == ??_EUIList
    BandCharacter(-4,2036)(.,616)(.,1180)(.,1192)      == ??_EBandCharacter
    MasterAudio  (.,4)(.,8)(.,48)                      == ??_EMasterAudio

It exists because the SAME four facts were re-implemented, wrongly, in
`dupname_identity_resolver.py`'s residue path, which invented a 41-class
"missing virtual" phantom that cost a lane a full investigation before being
refuted in `26284d0d`.  The four measured bugs there were:

  1. `??_G` / `??_E` SCOPE FOLD.  MSVC names a deleting-destructor BODY `??_G<C>`
     but names every adjustor thunk of it `??_E<C>`.  A check that demands a
     thunk whose scope is `??_G<C>@@` demands a symbol that CANNOT EXIST for any
     class -- 36 of 73 guaranteed false positives.  ==> `norm()`.
  2. NOT A THUNK AT ALL.  25 of 73 were plain tail calls
     (`mr r4,r3; li r3,16; b PoolFree` = operator delete;
      `addi r3,r3,16; b ~String`; a bare `b`).  "size <= 0x20 and ends in an
     unconditional `b`" is not a thunk test.  ==> `shape()` decodes the
     instruction sequence.
  3. `"$" in name` MISSES THE `W<n>@` SIMPLE-ADJUSTOR FORM.  MSVC writes `W`,
     not `$4`, when there is no vtordisp, and most multiple-inheritance thunks
     are `W`-form.  ==> `td()` accepts both.
  4. A thunk's name is a TOTAL FUNCTION of (callee prefix, vtordisp, this-adjust),
     so the derived name should be CONFIRMED against the unique symbol carrying
     that exact encoding -- not merely against a shared scope.  ==> `td()` +
     `prefix()` + `norm()` compared as a triple.

Keep this module free of I/O and of module-level state: `shape()` takes a
`word(va) -> uint32 | None` reader so both the raw-image tools and any future
obj-side caller can share it.
"""
import re

__all__ = ["shape", "mnum", "td", "prefix", "norm", "is_thunk_name"]


def shape(word, va, size):
    """(vtordisp, this_adjust, callee_va) if `va` holds an adjustor thunk, else None.

    An MSVC adjustor thunk is three instructions and nothing else:

        [lwz r11,-V(rX) ; subf rX,r11,rX]   vtordisp fetch  (optional)
        [addi rX,rX,-A]                     this-adjust     (optional)
        b   CALLEE

    Anything else -- including a small function that merely ENDS in an
    unconditional `b` -- is not a thunk.
    """
    if not size or size > 0x20:
        return None
    ws = [word(va + 4 * i) for i in range(size // 4)]
    if any(w is None for w in ws):
        return None
    while ws and ws[-1] == 0:
        ws.pop()
    if not ws:
        return None
    idx = 0
    vt = None
    adj = 0
    reg = None
    if len(ws) >= 3 and (ws[0] >> 26) == 32 and ((ws[0] >> 21) & 31) == 11:
        rA = (ws[0] >> 16) & 31
        imm = ws[0] & 0xFFFF
        if imm >= 0x8000:
            imm -= 0x10000
        if ((ws[1] >> 26) == 31 and ((ws[1] >> 1) & 0x3FF) == 40
                and ((ws[1] >> 21) & 31) == rA and ((ws[1] >> 16) & 31) == 11
                and ((ws[1] >> 11) & 31) == rA):
            vt = imm
            reg = rA
            idx = 2
        else:
            return None
    if idx < len(ws) and (ws[idx] >> 26) == 14:
        rD = (ws[idx] >> 21) & 31
        rA = (ws[idx] >> 16) & 31
        imm = ws[idx] & 0xFFFF
        if imm >= 0x8000:
            imm -= 0x10000
        if rD == rA and imm < 0 and (reg is None or rD == reg):
            adj = imm
            reg = rD
            idx += 1
        elif vt is None:
            return None
    if idx != len(ws) - 1:
        return None
    b = ws[idx]
    if b >> 26 != 18 or (b & 1) or ((b >> 1) & 1):
        return None
    li = b & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    if vt is None and adj == 0:
        return None
    if reg not in (3, 4):
        return None
    return (vt, -adj, va + 4 * idx + li)


def mnum(s, i):
    """MSVC mangled-number decode at s[i:] -> (value, next_index)."""
    if i >= len(s):
        return None, i
    if s[i] == '?':
        v, j = mnum(s, i + 1)
        return (None if v is None else -v), j
    if s[i].isdigit():
        return int(s[i]) + 1, i + 1
    j = i
    v = 0
    while j < len(s) and 'A' <= s[j] <= 'P':
        v = v * 16 + (ord(s[j]) - 65)
        j += 1
    if j == i:
        return None, i
    if j < len(s) and s[j] == '@':
        j += 1
    if v >= 0x80000000:
        v -= 0x100000000
    return v, j


# The qualified name of an MSVC symbol ends at the FIRST `@@` followed by either
# a thunk marker (`W<num>` simple adjustor / `$4` vtordisp adjustor) or an
# ordinary storage-class letter pair.  All three alternatives must be searched
# TOGETHER so the leftmost one wins.
#
# ★ Searching the thunk markers first, over the whole string, is WRONG and was a
#   live defect: an enum-typed PARAMETER is mangled `W4<Enum>@...`, so
#       ?Copy@BandCharacter@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z
#   (an ordinary virtual) contains `@@W4` and was read as a thunk with
#   this-adjust 5, and its prefix was truncated at `...@Hmx@@`.  Measured on the
#   901-VA duplicate fixture: 5 correctly-resolved `?Copy@<C>@@$4...` thunks were
#   re-classified NOT_PORTED purely because the CALLEE's prefix came back wrong.
#   Leftmost-wins fixes it: `@@UA` occurs before `@@W4` in every such name.
_TERM_RX = re.compile(r"@@(?:(W[0-9A-P?])|(\$4)|([QAEIMUBV][A-Z]))")


def _split(n):
    """(prefix_including_trailing_@@, marker_kind, index_after_@@) or None.

    marker_kind is 'W', '$4', or None (an ordinary, non-thunk symbol).
    """
    if not n:
        return None
    mo = _TERM_RX.search(n)
    if not mo:
        return None
    kind = 'W' if mo.group(1) else ('$4' if mo.group(2) else None)
    return n[:mo.start() + 2], kind, mo.start() + 2


def td(n):
    """(vtordisp, this_adjust) encoded in a thunk's mangled name, else None.

    Accepts BOTH MSVC thunk spellings:
      `<prefix>W<A>`     simple adjustor, no vtordisp  (the majority)
      `<prefix>$4<V><A>` vtordisp adjustor
    Testing for `"$" in name` alone misses every W-form thunk.
    """
    sp = _split(n)
    if not sp or sp[1] is None:
        return None
    _, kind, i = sp
    if kind == 'W':
        v, _ = mnum(n, i + 1)
        return (None, v) if v is not None else None
    vt, j = mnum(n, i + 2)
    ad, _ = mnum(n, j)
    return (vt, ad) if vt is not None and ad is not None else None


def is_thunk_name(n):
    """True iff `n` is spelled as an adjustor thunk (W-form or $4-form)."""
    return bool(n) and td(n) is not None


def prefix(n):
    """Qualified-name prefix shared by a virtual and its thunks, template-safe."""
    sp = _split(n)
    return sp[0] if sp else None


def norm(p):
    """`??_G` / `??_E` equivalence for deleting destructors.

    MSVC names the deleting-dtor BODY `??_G<C>` and every adjustor thunk of it
    `??_E<C>`.  Compare normalised prefixes or every polymorphic class reads as
    "thunk missing".
    """
    return "??_D*" + p[4:] if p.startswith(("??_G", "??_E")) else p
