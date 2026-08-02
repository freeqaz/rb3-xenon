#!/usr/bin/env python3
"""Guard: a COFF relocation's SymbolTableIndex COUNTS AUX RECORDS.

WHY THIS EXISTS
---------------
A COFF symbol table is a flat array of 18-byte records.  Some records are
"auxiliary" -- extra payload belonging to the PRECEDING symbol (a `.file` name,
a section definition, a COMDAT selection, a function's size/line info).  The
usual way to walk the table is:

    i = 0
    while i < nsym:
        ...parse record i...
        syms.append(sym)          # <-- appends ONCE
        i += 1 + naux             # <-- but ADVANCES past the aux records

That loop is correct for *iterating* symbols and CATASTROPHICALLY WRONG for
*indexing* them, because a relocation's `SymbolTableIndex` is a RAW index into
the full array -- aux records included.  So `syms[symidx]` silently returns
some earlier, unrelated symbol's name, or falls off the end of the list.

MEASURED ON THIS REPO (400 of our compiled objs, 1,213,502 relocations):
only **20.99%** of relocations resolve to the correct name when the list is
built without padding.  431,297 aux records were skipped across those objs.
The other ~79% are GARBAGE NAMES -- not errors, not exceptions: plausible
strings that flow straight into a verdict.

THIS BUG HAS BITTEN AT LEAST THREE TIMES
----------------------------------------
  * lane CO-4 -- caught it ONLY because a known-good control failed 399/400.
    Without that control it would have shipped "0/79 confirmed" as a real
    finding.
  * lane CM-3 -- its first EH-boundary patcher found ZERO boundaries.
  * tools/arity_screen.py -- shipped defective; fixed by lane CP-3.

Every one of those failures had the same shape: a SILENT FALSE NEGATIVE that
looks exactly like a decisive negative result.  That is the verdict class that
closes veins and stops future work, so it must be policed by a control that
can actually fail.

THE TWO CORRECT FIXES
---------------------
  1. PAD the list: append `naux` placeholder entries after each real symbol, so
     list position == raw index.  (What arity_screen.py now does.)
  2. RECORD the raw index on the symbol and resolve through it -- `sy.raw`
     (tools/extent_census/coffx.py) or `s.index` (scripts/analysis/coffx.py) --
     and build an explicit {raw_index: sym} dict for relocation lookups.
     NEVER index those modules' returned LISTS positionally.

WHAT THIS GUARD ASSERTS
-----------------------
It builds its OWN synthetic COFF object with known aux records and known
relocations, so ground truth is exact and the guard ALWAYS runs -- it does not
depend on build/ artifacts, which are gitignored and absent from CI and fresh
worktrees.  A guard that skips when its input is missing is a guard that cannot
fail, which is the very defect it is policing.

The fixture is shaped to expose BOTH failure directions:
  * a relocation whose raw index still lands inside the short list  -> WRONG NAME
  * a relocation whose raw index is past the end of the short list  -> MISSED

PASS = every checked resolver maps relocation -> symbol name exactly as the raw
       COFF index says it must.
FAIL = a resolver returned a name that is provably not the relocation's target.

USAGE
-----
    python3 tools/reloc_symidx_guard.py            # normal
    python3 tools/reloc_symidx_guard.py -v         # show every probe

    # prove it can fail (falsification control, house style):
    python3 tools/reloc_symidx_guard.py --self-break

Exit 0 = pass, 1 = a resolver mis-resolved a relocation.
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)

C_EXTERNAL = 2
C_STATIC = 3
C_FILE = 103
DTYPE_FUNCTION = 0x20

ap = argparse.ArgumentParser(description=__doc__,
                             formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument("-v", "--verbose", action="store_true")
ap.add_argument("--self-break", action="store_true",
                help="deliberately use the DEFECTIVE (unpadded) resolver, to "
                     "prove this guard is capable of reporting a failure")
ap.add_argument("--real-objs", type=int, default=200,
                help="also cross-check N real objs under build/ if present")


# ---------------------------------------------------------------------------
# The fixture.  Ground truth is whatever we WRITE here, so it cannot drift.
# ---------------------------------------------------------------------------
# raw idx | symbol
#    0    | .file                       (C_FILE,   naux=1)
#    1    |   <aux: source file name>
#    2    | .text                       (C_STATIC, naux=1)
#    3    |   <aux: section definition>
#    4    | __savegprlr_14              (EXTERNAL, function, naux=0)   <- needle A
#    5    | ?Poll@Foo@@UAEXXZ           (EXTERNAL, function, naux=1)
#    6    |   <aux: function definition>
#    7    | __restgprlr_14              (EXTERNAL, function, naux=0)   <- needle B
#
# A padded list has length 8 and index == raw index.
# An UNPADDED list is [.file, .text, __savegprlr_14, ?Poll@Foo@@UAEXXZ,
# __restgprlr_14] -- length 5 -- so:
#   reloc symidx 4 -> "__restgprlr_14"  (WRONG NAME: it is needle B, not A)
#   reloc symidx 7 -> off the end       (MISSED ENTIRELY: silent false negative)
FIXTURE_SYMS = [
    (".file", 0, -2, 0, C_FILE, 1),
    ("__fixture.cpp", 0, 0, 0, 0, 0),                     # aux
    (".text", 0, 1, 0, C_STATIC, 1),
    ("\0" * 8, 0, 0, 0, 0, 0),                            # aux (section def)
    ("__savegprlr_14", 0x00, 1, DTYPE_FUNCTION, C_EXTERNAL, 0),
    ("?Poll@Foo@@UAEXXZ", 0x10, 1, DTYPE_FUNCTION, C_EXTERNAL, 1),
    ("\0" * 8, 0, 0, 0, 0, 0),                            # aux (function def)
    ("__restgprlr_14", 0x20, 1, DTYPE_FUNCTION, C_EXTERNAL, 0),
]
# (section_relative_vaddr, raw_symbol_index, reloc_type)
FIXTURE_RELOCS = [
    (0x04, 4, 6),    # -> __savegprlr_14   (in-range under the bug -> WRONG NAME)
    (0x08, 5, 6),    # -> ?Poll@Foo@@UAEXXZ
    (0x0C, 7, 6),    # -> __restgprlr_14   (out-of-range under the bug -> MISSED)
]
TEXT_SIZE = 0x30


def build_fixture(path):
    """Emit a minimal but structurally valid PPC COFF object."""
    text = b"\x60\x00\x00\x00" * (TEXT_SIZE // 4)      # nop sled

    # --- symbol table + string table ---------------------------------------
    strtab = bytearray(b"\0\0\0\0")          # size patched at the end
    symblob = bytearray()
    for name, val, sec, typ, cls, naux in FIXTURE_SYMS:
        nb = name.encode("latin1") if isinstance(name, str) else name
        if len(nb) <= 8:
            field = nb.ljust(8, b"\0")
        else:
            off = len(strtab)
            strtab += nb + b"\0"
            field = struct.pack("<II", 0, off)
        symblob += field + struct.pack("<IhHBB", val & 0xFFFFFFFF, sec, typ,
                                       cls, naux)
    struct.pack_into("<I", strtab, 0, len(strtab))

    nsym = len(FIXTURE_SYMS)
    hdrsz = 20 + 40                          # 1 section
    rawptr = hdrsz
    relptr = rawptr + len(text)
    symptr = relptr + len(FIXTURE_RELOCS) * 10

    out = bytearray()
    out += struct.pack("<HHIIIHH", 0x01F2, 1, 0, symptr, nsym, 0, 0)
    out += (b".text".ljust(8, b"\0")
            + struct.pack("<IIIIIIHHI", 0, 0, len(text), rawptr, relptr, 0,
                          len(FIXTURE_RELOCS), 0, 0x60000020))
    out += text
    for va, si, rt in FIXTURE_RELOCS:
        out += struct.pack("<IIH", va, si, rt)
    out += symblob
    out += strtab
    with open(path, "wb") as f:
        f.write(bytes(out))
    return path


# ---------------------------------------------------------------------------
# Reference resolver: the ONLY thing entitled to define ground truth.
# ---------------------------------------------------------------------------
def reference_resolve(path):
    """{reloc_vaddr: symbol_name} using RAW indices, per the COFF spec."""
    d = open(path, "rb").read()
    machine, nsec, _ts, symptr, nsym, optsz, _ch = struct.unpack_from("<HHIIIHH", d, 0)
    strbase = symptr + nsym * 18
    by_raw = {}
    i = 0
    while i < nsym:
        o = symptr + i * 18
        raw = d[o:o + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", d, o + 4)[0]
            e = d.index(b"\0", strbase + off)
            name = d[strbase + off:e].decode("latin1")
        else:
            name = raw.rstrip(b"\0").decode("latin1")
        naux = d[o + 17]
        by_raw[i] = name
        i += 1 + naux
    out = {}
    for k in range(nsec):
        o = 20 + optsz + k * 40
        relptr = struct.unpack_from("<I", d, o + 24)[0]
        nrel = struct.unpack_from("<H", d, o + 32)[0]
        for j in range(nrel):
            va, symidx, _rt = struct.unpack_from("<IIH", d, relptr + j * 10)
            # KEY BY (section, RELOC ORDINAL) -- unique by construction.
            # Two weaker keys were tried and BOTH silently lost records:
            #   * `va` alone      -> collides across .text/.pdata/.rdata, because
            #                        relocation vaddrs are SECTION-RELATIVE.
            #   * `(section, va)` -> STILL collides, because PowerPC COFF emits
            #                        IMAGE_REL_PPC_PAIR (type 18) at the SAME
            #                        vaddr as the preceding REFHI/REFLO (16/17).
            # Measured: 18,032 of 88,979 relocations (20.3%) are PAIR records.
            # NOTE: a PAIR record's SymbolTableIndex field is NOT a symbol index
            # -- it carries the low 16 bits of the displacement.  Resolving it to
            # a name yields a garbage symbol.  Callers must skip type 18.
            out[(k, j)] = by_raw.get(symidx)
    return out


def defective_resolve(path):
    """The BUG, reproduced verbatim: append once, advance past aux, index the
    short list.  Used only by --self-break, so the guard's failure path is
    exercised rather than merely asserted."""
    d = open(path, "rb").read()
    machine, nsec, _ts, symptr, nsym, optsz, _ch = struct.unpack_from("<HHIIIHH", d, 0)
    strbase = symptr + nsym * 18
    syms = []
    i = 0
    while i < nsym:
        o = symptr + i * 18
        raw = d[o:o + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", d, o + 4)[0]
            e = d.index(b"\0", strbase + off)
            name = d[strbase + off:e].decode("latin1")
        else:
            name = raw.rstrip(b"\0").decode("latin1")
        naux = d[o + 17]
        syms.append(name)
        i += 1 + naux                      # <-- no pad: THIS is the defect
    out = {}
    for k in range(nsec):
        o = 20 + optsz + k * 40
        relptr = struct.unpack_from("<I", d, o + 24)[0]
        nrel = struct.unpack_from("<H", d, o + 32)[0]
        for j in range(nrel):
            va, symidx, _rt = struct.unpack_from("<IIH", d, relptr + j * 10)
            out[(k, j)] = syms[symidx] if symidx < len(syms) else None
    return out


# ---------------------------------------------------------------------------
def main():
    args = ap.parse_args()
    failures = []
    tmpdir = os.path.join(os.environ.get("HOME", "/tmp"), "tmp", "reloc_guard")
    os.makedirs(tmpdir, exist_ok=True)
    fx = build_fixture(os.path.join(tmpdir, "fixture.obj"))

    truth = {(0, j): name for j, (va, si, rt) in enumerate(FIXTURE_RELOCS)
             for name in [FIXTURE_SYMS[si][0]]}
    print("== fixture ground truth (written by this file, cannot drift) ==")
    for key in sorted(truth):
        print("   sec%d reloc#%d (va 0x%02x) -> %s"
              % (key[0], key[1], FIXTURE_RELOCS[key[1]][0], truth[key]))

    # -- probe 0: the reference resolver reproduces the written truth --------
    ref = reference_resolve(fx)
    if ref != truth:
        failures.append("reference resolver disagrees with the written fixture: %r" % (ref,))
    else:
        print("\n[ok] reference resolver reproduces ground truth (3/3)")

    # -- probe 1: arity_screen.CoffObj is raw-index-addressable --------------
    # This is the tool that shipped defective.  Two independent assertions:
    #   (a) list length == raw symbol count  (the pad is present)
    #   (b) every relocation resolves to the correct name
    label = "DEFECTIVE (--self-break)" if args.self_break else "tools/arity_screen.py"
    try:
        if args.self_break:
            got = defective_resolve(fx)
            got_len, want_len = None, None
        else:
            from arity_screen import CoffObj
            co = CoffObj(fx)
            got_len, want_len = len(co.syms), len(FIXTURE_SYMS)
            got = {}
            d = co.d
            for si, (nm, rawsz, rawptr, relptr, nrel) in enumerate(co.secs):
                for j in range(nrel):
                    va, symidx, _rt = struct.unpack_from("<IIH", d, relptr + j * 10)
                    got[(si, j)] = (co.syms[symidx][0] if symidx < len(co.syms) else None)
    except Exception as ex:
        failures.append("%s raised %s: %s" % (label, type(ex).__name__, ex))
        got, got_len, want_len = {}, None, None

    print("\n== probe: %s ==" % label)
    if got_len is not None:
        ok = got_len == want_len
        print("   symbol-list length %d (raw symbol count %d) %s"
              % (got_len, want_len, "OK" if ok else "<-- NOT raw-index-addressable"))
        if not ok:
            failures.append("symbol list is not raw-index-addressable: %d != %d"
                            % (got_len, want_len))
    for key in sorted(truth):
        va = FIXTURE_RELOCS[key[1]][0]
        exp, act = truth[key], got.get(key)
        mark = "ok " if exp == act else "FAIL"
        if exp != act:
            why = "MISSED (index past end of short list)" if act is None \
                  else "WRONG NAME (resolved to a different symbol)"
            failures.append("reloc @0x%02x expected %r got %r -- %s" % (va, exp, act, why))
            print("   [%s] @0x%02x expected %-20r got %-20r  %s" % (mark, va, exp, act, why))
        elif args.verbose:
            print("   [%s] @0x%02x -> %s" % (mark, va, act))
    if not failures:
        print("   all %d relocations resolved correctly" % len(truth))

    # -- probe 2: cross-check against REAL objs, if this tree has any --------
    real = []
    bdir = os.path.join(REPO, "build", "45410914", "src")
    if os.path.isdir(bdir) and not args.self_break:
        for dp, _dn, fn in os.walk(bdir):
            for f in fn:
                if f.endswith(".obj"):
                    real.append(os.path.join(dp, f))
        real.sort()
        real = real[:args.real_objs]
    if real:
        from arity_screen import CoffObj
        tot = agree = 0
        bad_files = 0
        for p in real:
            try:
                co = CoffObj(p)
                rr = reference_resolve(p)
            except Exception:
                continue
            hit = True
            for si, (nm, rawsz, rawptr, relptr, nrel) in enumerate(co.secs):
                for j in range(nrel):
                    va, symidx, _rt = struct.unpack_from("<IIH", co.d, relptr + j * 10)
                    exp = rr.get((si, j))
                    act = co.syms[symidx][0] if symidx < len(co.syms) else None
                    tot += 1
                    if exp == act:
                        agree += 1
                    else:
                        hit = False
            if not hit:
                bad_files += 1
        pct = 100.0 * agree / tot if tot else 0.0
        print("\n== probe: %d real objs, %d relocations ==" % (len(real), tot))
        print("   resolved correctly: %d (%.2f%%), files with any mismatch: %d"
              % (agree, pct, bad_files))
        if tot and agree != tot:
            failures.append("real-obj cross-check: %d/%d relocations mis-resolved"
                            % (tot - agree, tot))
    else:
        print("\n== probe: real objs -- none present (fixture probe still ran) ==")

    print()
    if args.self_break:
        if failures:
            print("PASS (--self-break): the guard DETECTED the defective resolver:")
            for f in failures:
                print("   - " + f)
            print("\n=> This guard is capable of failing.  Its clean run is meaningful.")
            sys.exit(0)
        print("!! --self-break did NOT fail => THIS GUARD IS VACUOUS.")
        sys.exit(1)

    if failures:
        print("FAIL: %d problem(s)" % len(failures))
        for f in failures:
            print("   - " + f)
        sys.exit(1)
    print("PASS: relocation SymbolTableIndex resolves through aux records correctly.")
    sys.exit(0)


main()
