#!/usr/bin/env python3
"""List every retail `bl` call site that reaches a given .text address.

WHY THIS EXISTS (lane W2-ENGINE, 2026-08-17)
────────────────────────────────────────────
`functionRelocDiffs=name_check` charges a row whenever retail's callee NAME
differs from ours. CLAUDE.md warns that objdiff's own `LINKER_MERGED` /
`AT_LIMIT` verdict on such a row is "the detector restating its own input":
*target calls A, we call B, A != B* is bit-for-bit the definition of BOTH a
genuine ICF fold AND a wrong map name. The metric cannot separate them, and an
`AT_LIMIT` label closes the vein.

The CALL GRAPH separates them, and it is map-independent evidence:

  * A map name is **WRONG (fixable)** when the competing name has no possible
    RB3 owner, or when the call CONTEXT pins the slot. Two settled examples:
      - 0x82272140 was named `map<Hmx::CRC,float>::operator[]`. All 18 callers
        are int-keyed users, including `operator>>(BinStream&, map<int,float>&)`
        whose own map name spells the key `H` -- the map contradicted itself.
        Corroborated by a binary-absence proof (`.?AVHamMove@@` occurs 0 times
        in retail, with 5 positive controls at 1). => +8,048 B.
      - 0x82298560 was named `BandDirector::OnGetCatList`. It has exactly ONE
        caller, and that `bl` sits in the branch that builds a Symbol from the
        .rdata string at 0x82016FB4, which reads literally "copy_cats".
        => it is OnCopyCats. +4,732 B.

  * A map name is **ARBITRARY (irreducible)** when both names have real RB3
    owners AND the bodies are byte-identical -- ICF destroyed which name the
    site meant. Example NOT fixed: 0x8235c2e0 is called by 17 unrelated
    classes' SyncProperty (=> looks like `Hmx::Object::SyncProperty`) but our
    `Tour::SyncProperty` is byte-identical to it and pairs at 99.44%. Renaming
    to capture the 17 call sites would be picking the higher-scoring arbitrary
    name, i.e. metric fitting. Left alone deliberately.

⚠ COUNTING CALLERS IS ALSO A FOLD TEST. If two of our functions have identical
source bodies and retail folded them, the survivor is called TWICE from a
dispatch chain that references both. BandDirector's handlers are identical
source in our tree, yet 0x82298560 has ONE caller -- so retail did NOT fold
them, which is what made "wrong name" the only surviving reading.

⚠ Do NOT adjudicate these with `grep` on the binary: the agent shell routes
grep through `ugrep -I`, which is binary-blind and yields false negatives
shaped like decisive ones (CLAUDE.md). This reads the PE in Python.

Usage:  python3 tools/retail_callers.py 82272140 [more addrs...]
"""
import struct
import sys

PE = "orig/45410914/band.exe"


def sections(buf):
    pe_off = struct.unpack_from("<I", buf, 0x3C)[0]
    assert buf[pe_off:pe_off + 4] == b"PE\0\0"
    coff = pe_off + 4
    nsec = struct.unpack_from("<H", buf, coff + 2)[0]
    optsz = struct.unpack_from("<H", buf, coff + 16)[0]
    opt = coff + 20
    imgbase = struct.unpack_from("<I", buf, opt + 28)[0]
    secs = []
    off = opt + optsz
    for _ in range(nsec):
        raw = buf[off:off + 40]
        name = raw[:8].rstrip(b"\0").decode("latin1")
        vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", raw, 8)
        secs.append((name, imgbase + vaddr, vsize, rawptr, rawsize))
        off += 40
    return imgbase, secs


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    targets = {int(a, 16) for a in sys.argv[1:]}
    buf = open(PE, "rb").read()
    _imgbase, secs = sections(buf)
    _n, vaddr, vsize, rawptr, rawsize = [s for s in secs if s[0] == ".text"][0]
    data = buf[rawptr:rawptr + min(rawsize, vsize)]
    hits = {t: [] for t in targets}
    for off in range(0, len(data) // 4 * 4, 4):
        w = struct.unpack_from(">I", data, off)[0]
        if (w >> 26) != 18 or not (w & 1):     # branch-immediate with LK => bl
            continue
        li = w & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        pc = vaddr + off
        dest = li if ((w >> 1) & 1) else pc + li   # AA=1 => absolute
        if dest in hits:
            hits[dest].append(pc)
    for t in sorted(targets):
        print(f"== bl -> 0x{t:08x}: {len(hits[t])} call site(s)")
        for pc in hits[t]:
            print(f"   0x{pc:08x}")


if __name__ == "__main__":
    main()
