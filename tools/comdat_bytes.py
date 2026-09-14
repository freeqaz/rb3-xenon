#!/usr/bin/env python3
"""Extract the section bytes backing each external DEFINITION in a COFF object.

Adapted for rb3-xenon (MSVC PPC / Xbox 360 COFF) from the dc3-side extractor.

For an ICF fold argument the question is not where a name sits in a linker map,
it is whether the two COMDATs are byte-identical -- that is the condition
/OPT:ICF actually tests.  Relocations are masked: a data COMDAT holding a
pointer has its target patched at link time, so the raw 4 bytes differ while
the COMDAT is still foldable iff the reloc TARGETS agree.

rb3-xenon deltas vs the dc3 original:

  * the RAW (unmasked) bytes are returned alongside the masked ones.  The
    retail side of this comparison is a LINKED image, where a `bl` carries its
    displacement in the instruction rather than in a relocation record, so the
    two sides can only be compared after applying the SAME instruction-aware
    mask to both.  Zeroing the relocated field outright (what the dc3 version
    does) throws away the opcode/AA/LK bits and makes `b X` compare equal to
    `bc X` -- see tools/fold_thunk_gate.py:mask_word.
  * the symbol's `value` is honoured.  Our EH-bearing functions carry an 8-byte
    prefix inside the same section, so the definition does not always start at
    offset 0 (the DC-4 body-reader finding).
  * relocation offsets are returned RELATIVE to the definition start.
"""
import struct
import sys
from pathlib import Path

EXTERNAL = 2


def comdats(path):
    d = Path(path).read_bytes()
    nsec, = struct.unpack_from("<H", d, 2)
    psym, nsym = struct.unpack_from("<II", d, 8)
    if not psym or not nsym:
        return {}
    opt, = struct.unpack_from("<H", d, 16)
    sh = 20 + opt
    sec = []
    for s in range(nsec):
        b = sh + s * 40
        name = d[b:b + 8].rstrip(b"\0").decode("latin1")
        size, = struct.unpack_from("<I", d, b + 16)
        praw, = struct.unpack_from("<I", d, b + 20)
        prel, = struct.unpack_from("<I", d, b + 24)
        nrel, = struct.unpack_from("<H", d, b + 32)
        chars, = struct.unpack_from("<I", d, b + 36)
        sec.append((name, size, praw, prel, nrel, chars))
    strt = psym + nsym * 18
    names, recs = {}, []
    i = 0
    while i < nsym:
        rec = d[psym + i * 18: psym + i * 18 + 18]
        if rec[:4] == b"\0\0\0\0":
            off, = struct.unpack_from("<I", rec, 4)
            end = d.index(b"\0", strt + off)
            nm = d[strt + off:end].decode("latin1")
        else:
            nm = rec[:8].rstrip(b"\0").decode("latin1")
        val, = struct.unpack_from("<I", rec, 8)
        secnum, = struct.unpack_from("<h", rec, 12)
        sclass = rec[16]
        naux = rec[17]
        names[i] = nm
        recs.append((i, nm, val, secnum, sclass, naux))
        i += 1 + naux
    # ---- FUNCTION-EXTENT BOUNDARIES (lane W16-S, 2026-09-14).
    # `body = whole[val:]` runs to the END OF THE SECTION, and an EH-bearing
    # MSVC /Gy COMDAT holds the function AND its trailing `__unwind$` funclet in
    # one section.  So `size`/`raw` bill the funclet into the body, while a
    # retail extent from .pdata is the FUNCTION ONLY.  That is a ONE-SIDED
    # reader artifact: it CANCELS when both sides are read this way (our-N vs
    # our-S) and does NOT cancel against retail, where it manufactures a
    # confident size "contradiction".  Measured: 587 alias memberships over 30
    # STLport addresses, every one a constant -44 B == the `__unwind$` funclet
    # (`_Copy_Construct` @0x823d3ac8: section 112, val 8, funclet at 68 =>
    # function is [8,68) = 60 B = retail's .pdata extent exactly).
    # Same family as STLPORT-1's phantom "+8 B STLport source bug".
    #
    # ADDITIVE ONLY: `size`/`raw`/`bytes`/`relocs` keep their old meanings so no
    # existing consumer changes behaviour.  New keys `fn_size`/`fn_raw`/
    # `fn_relocs`/`fn_bounded` carry the function-only extent; use THOSE for any
    # comparison against retail.
    FUNCLET = ("__unwind$", "__catch$", "__ehhandler$", "__unwindfunclet$",
               "__tryblocktable$", "__ehfuncinfo$", "$EH")
    bounds = {}          # secnum -> sorted list of candidate end offsets
    for (i, nm, val, secnum, sclass, naux) in recs:
        if secnum <= 0:
            continue
        # class 6 (LABEL: $M…, $LN…) sits INSIDE the body -- never a boundary.
        if sclass == EXTERNAL or (sclass == 3 and nm.startswith(FUNCLET)):
            bounds.setdefault(secnum, []).append(val)

    out = {}
    for (i, nm, val, secnum, sclass, naux) in recs:
        if secnum <= 0 or sclass != EXTERNAL or secnum > len(sec):
            continue
        sname, size, praw, prel, nrel, chars = sec[secnum - 1]
        whole = d[praw:praw + size] if praw else bytes(size)
        raw = bytearray(whole)
        rel = []
        for r in range(nrel):
            va, si = struct.unpack_from("<II", d, prel + r * 10)
            ty, = struct.unpack_from("<H", d, prel + r * 10 + 8)
            rel.append((va, names.get(si, "?"), ty))
            for k in range(va, min(va + 4, len(raw))):
                raw[k] = 0
        body = whole[val:]
        rel_rebased = sorted((o - val, n, t) for (o, n, t) in rel if o >= val)
        nxt = [v for v in bounds.get(secnum, []) if v > val]
        end = min(nxt) if nxt else size
        fn_body = whole[val:end]
        out[nm] = {
            "section": sname, "section_size": size, "value": val,
            "size": len(body),
            "raw": body,                     # UNMASKED, from the definition start
            "bytes": bytes(raw[val:]),       # relocated fields zeroed (dc3-compatible)
            "relocs": rel_rebased,           # offsets relative to the definition
            "is_code": bool(chars & 0x20),
            # function-only extent -- USE THESE AGAINST RETAIL (see note above)
            "fn_end": end,
            "fn_size": len(fn_body),
            "fn_raw": fn_body,
            "fn_relocs": [(o, n, t) for (o, n, t) in rel_rebased
                          if o < len(fn_body)],
            "fn_bounded": bool(nxt),         # False => no boundary symbol found
        }
    return out


if __name__ == "__main__":
    for k, v in sorted(comdats(Path(sys.argv[1])).items()):
        if len(sys.argv) > 2 and sys.argv[2] not in k:
            continue
        print(f"{k}  sec={v['section']} val={v['value']} size={v['size']} "
              f"code={v['is_code']} raw={v['raw'].hex()} relocs={v['relocs']}")
