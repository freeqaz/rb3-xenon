#!/usr/bin/env python3
"""Recover correct TU5 VAs for a set of TU0 functions via section-mapped,
relocation-normalized opcode-skeleton matching.

Both PEs are read through the PE section table (tu5_va.load_sections), so this
is correct for the section-mapped TU5 image (flat 0x3000+VA is WRONG on TU5).

Masking (proven spike scheme):
  opcode 18 (b/bl)      -> word & 0xFC000003   (keep AA/LK, drop 24-bit target)
  opcode 16 (bc)        -> word & 0xFFFF0003   (keep BO/BI/AA/LK, drop BD)
  D-form (imm16 forms)  -> word & 0xFFFF0000   (drop 16-bit immediate)
  else                  -> word unchanged

A TU0 function's masked skeleton is searched (4-byte aligned) across the whole
TU5 .text masked stream; a UNIQUE hit == HIGH-confidence 1:1 entry VA.
"""
import struct
import sys

sys.path.insert(0, __file__.rsplit("/", 1)[0])
from tu5_va import load_sections, va_to_off  # noqa: E402

TU0_PE = "orig/45410914/band.exe"
TU5_PE = "orig/45410914/band_tu5.exe"

# D-form (16-bit signed/unsigned immediate) primary opcodes.
DFORM = {
    3, 7, 8, 10, 11, 12, 13, 14, 15,          # tw*, mulli, sub/addic, cmp*, addi(c), addis
    24, 25, 26, 27, 28, 29,                     # ori(s), xori(s), andi(s).
    32, 33, 34, 35, 36, 37, 38, 39,             # lwz(u), lbz(u), stw(u), stb(u)
    40, 41, 42, 43, 44, 45, 46, 47,             # lhz(u), lha(u), sth(u), lmw/stmw
    48, 49, 50, 51, 52, 53, 54, 55,             # lfs(u), lfd(u), stfs(u), stfd(u)
}


def mask_word(w):
    op = (w >> 26) & 0x3F
    if op == 18:
        return w & 0xFC000003
    if op == 16:
        return w & 0xFFFF0003
    if op in DFORM:
        return w & 0xFFFF0000
    return w


def read_range(data, secs, va, size):
    off, sec = va_to_off(va, secs)
    if off is None:
        raise SystemExit(f"VA 0x{va:08x} not mapped")
    body = data[off:off + size]
    return [struct.unpack_from(">I", body, i)[0] for i in range(0, len(body) - 3, 4)]


def bound_by_blr(data, secs, va, cap=0x400):
    """Return words from va up to and including first blr (0x4E800020)."""
    off, _ = va_to_off(va, secs)
    words = []
    for i in range(0, cap, 4):
        w = struct.unpack_from(">I", data, off + i)[0]
        words.append(w)
        if w == 0x4E800020:
            break
    return words


def masked_bytes(words):
    return b"".join(struct.pack(">I", mask_word(w)) for w in words)


def find_unique(needle, haystack, hay_base):
    """Find all 4-byte-aligned occurrences of needle in haystack (both masked bytes)."""
    hits = []
    start = 0
    while True:
        idx = haystack.find(needle, start)
        if idx < 0:
            break
        if idx % 4 == 0:
            hits.append(hay_base + idx)
        start = idx + 4
    return hits


def main():
    d0, b0, s0 = load_sections(TU0_PE)
    d5, b5, s5 = load_sections(TU5_PE)
    # TU5 .text masked stream
    t5 = next(x for x in s5 if x[0] == ".text")
    _, t5_va, t5_vsize, t5_ptr, t5_raw = t5
    t5_words = [struct.unpack_from(">I", d5, t5_ptr + i)[0]
                for i in range(0, min(t5_vsize, t5_raw) - 3, 4)]
    t5_mask = b"".join(struct.pack(">I", mask_word(w)) for w in t5_words)

    targets = [
        ("ResolvePartWaitStates", 0x8259D948, 0x54C),
        ("ProcessConfig",         0x8274ACF8, 0x78),
        ("RecalcGemList",         0x8276FBB0, 0x4C),
        ("GameGemDB::Duplicate",  0x8276E590, 0xA4),
        ("GameGemList::CopyFrom", 0x82769450, 0x90),
        ("GameGemDB::GetDiffList", 0x8276E010, None),   # leaf, bound by blr
        ("IsActive",              0x8264B5F8, 0x380),
    ]

    results = {}
    for name, va, size in targets:
        if size is None:
            words = bound_by_blr(d0, s0, va)
        else:
            words = read_range(d0, s0, va, size)
        needle = masked_bytes(words)
        hits = find_unique(needle, t5_mask, t5_va)
        # If not unique, try trimming trailing padding / retry with first 32 words
        note = ""
        if len(hits) != 1 and len(words) > 32:
            needle2 = masked_bytes(words[:32])
            hits2 = find_unique(needle2, t5_mask, t5_va)
            if len(hits2) == 1:
                hits = hits2
                note = "matched on first 128B skeleton"
        conf = "HIGH" if len(hits) == 1 else ("AMBIG" if len(hits) > 1 else "MISS")
        tu5_va = hits[0] if len(hits) == 1 else None
        results[f"0x{va:08x}"] = {
            "symbol": name, "tu0_va": f"0x{va:08x}", "size": len(words) * 4,
            "tu5_va": (f"0x{tu5_va:08x}" if tu5_va else None),
            "conf": conf, "n_hits": len(hits), "note": note,
        }
        print(f"{name:24} TU0 0x{va:08x} sz {len(words)*4:5} -> "
              f"{('0x%08x' % tu5_va) if tu5_va else 'NONE':12} {conf} "
              f"(hits={len(hits)}) {note}")

    import json
    print("\n" + json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
