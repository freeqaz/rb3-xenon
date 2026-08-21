#!/usr/bin/env python3
"""Post-compile patcher: give our compiled .obj files an EXTENT BOUNDARY at
every EH-funclet prefix, so objdiff stops a function where retail stops it.

The defect (lane CM-3, 2026-08-02)
----------------------------------
MSVC X360 lays an EH-enabled function out inside its `.text` COMDAT as

    +0x000  .long __CxxFrameHandler          <- 8-byte EH prefix
    +0x004  .long __ehfuncinfo$?Func@@...
    +0x008  ?Func@@...                       <- the function symbol
      ...   body ...
    +0x1a4  .long __CxxFrameHandler          <- EH prefix for the FUNCLET
    +0x1a8  .long __ehfuncinfo$?Func@@...
    +0x1ac  __catch$92771                    <- catch funclet

The main function's true end is 0x1a4. But MSVC puts no class-2/3 symbol
there -- only a class-6 (IMAGE_SYM_CLASS_LABEL) `$M#####` debug label, which
objdiff does not treat as a boundary. So our function's extent runs on to the
next real symbol (`__catch$...` at 0x1ac) and SWALLOWS THE FUNCLET'S 8-BYTE EH
PREFIX. objdiff then disassembles those two relocation words as trailing
instructions and reports:

    [N-2] insert: `<illegal> __CxxFrameHandler`
    [N-1] insert: `<illegal> __ehfuncinfo$<the function's own name>`

The TARGET side does not have this problem: dtk emits a named
`except_data_<addr>` symbol over each 8-byte prefix, which both bounds the
preceding function and is itself excluded from the report (measured: 0
`except_data_*` rows among report.json's 69,360 function rows). So retail's
extent is right and ours is 8 bytes too long.

★ This is an ATTRIBUTION artifact, not a codegen difference. The 8 bytes exist
in retail too; they simply belong to the funclet, not to the function. We are
not hiding a real mismatch -- we are moving a boundary to where retail's own
splitter already puts it.

Measured population (lane CM-3, at 42,947 matched):
  * 39,922 of 39,922 `.text` COMDATs carrying a `__CxxFrameHandler` reloc have
    EXACTLY this shape (fn symbol at +8, handler reloc at 0, funcinfo at 4) --
    zero exceptions, so the layout rule is uniform.
  * 346 of 21,414 paired functions differ from their target by exactly +/-8
    bytes; 265 of those are (ours-longer, EH-sectioned).
  * 166 of them are byte-equal to the target over the FULL target extent once
    relocation words are masked (anti-vacuity guard: >=4 unmasked words AND
    >=50% of the body unmasked; the guard rejected 4 candidates). Those 166
    are the ones this patcher can take to 100%: 28,984 target bytes across 77
    units => a CEILING of +166 matched / +0.271162pp code%.
    The other ~96 have real body differences as well and will not flip.

How
---
Append a STATIC (class 3), type 0 symbol at each funclet EH-prefix offset.
Appending at the END of the symbol table is what makes this safe:

  * relocation entries reference symbols BY INDEX, and appending disturbs no
    existing index;
  * the string table starts at `PointerToSymbolTable + NumberOfSymbols*18`,
    which is recomputed from the new count, so every long-name offset (which
    is relative to that start) stays valid;
  * names are <=8 bytes so they live inline and the string table is untouched.

Verified end-to-end before this script existed: hand-appending one such symbol
at 0x1a4 in ClipDistMap.obj moved
`?_M_insert_overflow_aux@?$vector@VVector3@@...` from
base-size 420 / 103-of-105 equal / 2 EH inserts  ->  base-size 412 / 103-of-103
equal / **100.0%**, diffed with objdiff-cli against untouchable file copies
(`-1`/`-2`, no `--build`) so the treatment could not be silently reverted.
⚠ The FIRST attempt at that experiment was VACUOUS: patching the obj in the
build tree and then asking objdiff to diff it let ninja recompile the obj and
wipe the patch, producing a clean-looking "no effect". Always prove the
boundary symbol is still present in the exact file being diffed.

Idempotent: a boundary is skipped if any class-2/3 symbol already sits at that
offset (including one this script added on an earlier run).

Usage
-----
    python3 scripts/obj_eh_boundary_patcher.py --batch --apply
    python3 scripts/obj_eh_boundary_patcher.py --apply path/to/one.obj
"""

import argparse
import glob
import os
import struct
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

HANDLER = "__CxxFrameHandler"
FUNCINFO_PREFIX = "__ehfuncinfo$"
# COFF storage classes that objdiff honours as an extent boundary.
BOUNDARY_CLASSES = (2, 3)  # EXTERNAL, STATIC


def _parse(data: bytes):
    """Return (nsec, symoff, nsym, opt, sections, symbols) or None."""
    if len(data) < 20:
        return None
    nsec = struct.unpack_from("<H", data, 2)[0]
    symoff = struct.unpack_from("<I", data, 8)[0]
    nsym = struct.unpack_from("<I", data, 12)[0]
    opt = struct.unpack_from("<H", data, 16)[0]
    if symoff == 0 or nsym == 0:
        return None
    strt = symoff + nsym * 18

    def sname(off: int) -> str:
        try:
            end = data.index(b"\x00", strt + off)
            return data[strt + off:end].decode("ascii", errors="replace")
        except (ValueError, IndexError):
            return ""

    symbols = []
    i = 0
    while i < nsym:
        eo = symoff + i * 18
        if eo + 18 > len(data):
            break
        nb = data[eo:eo + 8]
        if nb[:4] == b"\x00\x00\x00\x00":
            name = sname(struct.unpack_from("<I", nb, 4)[0])
        else:
            name = nb.split(b"\x00")[0].decode("ascii", errors="replace")
        value = struct.unpack_from("<I", data, eo + 8)[0]
        sec = struct.unpack_from("<h", data, eo + 12)[0]
        sclass = data[eo + 16]
        aux = data[eo + 17]
        # ★ Keep the RAW table index `i`. Relocation entries carry
        # SymbolTableIndex into the raw table, which COUNTS AUX RECORDS; this
        # loop skips them, so a list position is NOT a symbol index. Using the
        # list position made every reloc resolve to the wrong name and the
        # patcher found 0 boundaries (loudly, thankfully).
        symbols.append((name, value, sec, sclass, i))
        i += 1 + aux

    sections = []
    for s in range(nsec):
        so = 20 + opt + s * 40
        if so + 40 > len(data):
            break
        name = data[so:so + 8].rstrip(b"\x00").decode("ascii", errors="replace")
        size = struct.unpack_from("<I", data, so + 16)[0]
        prel = struct.unpack_from("<I", data, so + 24)[0]
        nrel = struct.unpack_from("<H", data, so + 32)[0]
        rels = []
        for r in range(nrel):
            ro = prel + r * 10
            if ro + 10 > len(data):
                break
            rva, sidx = struct.unpack_from("<II", data, ro)
            rels.append((rva, sidx))
        sections.append((s + 1, name, size, rels))
    return nsec, symoff, nsym, opt, sections, symbols


def find_boundaries(data: bytes) -> List[Tuple[int, int]]:
    """Return [(section_index, offset)] needing a boundary symbol."""
    parsed = _parse(data)
    if not parsed:
        return []
    _, _, _, _, sections, symbols = parsed
    byidx = {sy[4]: sy for sy in symbols}
    # symbol offsets that ALREADY act as boundaries, per section
    existing: Dict[int, set] = {}
    for name, value, sec, sclass, _raw in symbols:
        if sec > 0 and sclass in BOUNDARY_CLASSES:
            existing.setdefault(sec, set()).add(value)

    out: List[Tuple[int, int]] = []
    for sidx, name, size, rels in sections:
        if not name.startswith(".text"):
            continue
        handler_offs = set()
        funcinfo_offs = set()
        for rva, si in rels:
            nm = byidx.get(si, ("", 0, 0, 0, 0))[0]
            if nm == HANDLER:
                handler_offs.add(rva)
            elif nm.startswith(FUNCINFO_PREFIX):
                funcinfo_offs.add(rva)
        if not handler_offs:
            continue
        have = existing.get(sidx, set())
        for off in sorted(handler_offs):
            # A real EH prefix is a PAIR: handler at X, funcinfo at X+4.
            # Requiring the pair is what stops us inserting a boundary on a
            # stray handler reference (e.g. a plain call) and cutting a
            # function in half.
            if off + 4 not in funcinfo_offs:
                continue
            if off in have:
                continue  # already a boundary (incl. our own earlier run)
            if off + 8 > size:
                continue
            out.append((sidx, off))
    return out


def patch(data: bytearray, boundaries: List[Tuple[int, int]]) -> int:
    if not boundaries:
        return 0
    symoff = struct.unpack_from("<I", data, 8)[0]
    nsym = struct.unpack_from("<I", data, 12)[0]
    strt = symoff + nsym * 18
    blob = bytearray()
    for n, (sidx, off) in enumerate(boundaries):
        # <=8 chars keeps the name inline -> the string table is untouched.
        name = ("$EH%05X" % n)[:8].encode("ascii").ljust(8, b"\x00")
        blob += name + struct.pack("<IhHBB", off, sidx, 0, 3, 0)
    data[strt:strt] = blob
    struct.pack_into("<I", data, 12, nsym + len(boundaries))
    return len(boundaries)


def patch_file(path: Path, apply: bool) -> Tuple[int, Optional[str]]:
    data = bytearray(path.read_bytes())
    b = find_boundaries(bytes(data))
    if not b:
        return 0, None
    n = patch(data, b)
    # SELF-CHECK: the patched buffer must still parse, and must now expose
    # every boundary offset. A patcher that silently corrupts a symbol table
    # would show up as a mass regression with nothing failing.
    reparsed = _parse(bytes(data))
    if not reparsed:
        return 0, f"{path}: patched buffer no longer parses -- NOT written"
    got = {(s, v) for nm, v, s, sc, _r in reparsed[5]
           if nm.startswith("$EH") and sc == 3}
    missing = [x for x in b if x not in got]
    if missing:
        return 0, f"{path}: {len(missing)} boundaries missing after patch -- NOT written"
    if apply:
        # ★ PRESERVE THE MTIME. ninja records each output's mtime in
        # .ninja_log and treats an output modified after its build as DIRTY.
        # Without this, the 572 objs this patcher touches were RECOMPILED by
        # the very next ninja invocation -- which wipes the patch, so the
        # report is then generated from unpatched objs and the whole change
        # measures as nothing. Measured: ab_measure's leg-B report build came
        # back `msvc=572`, exactly the patched-file count, and REFUSED the run.
        st = path.stat()
        path.write_bytes(data)
        os.utime(path, ns=(st.st_atime_ns, st.st_mtime_ns))
    return n, None


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Insert EH-funclet extent boundaries into compiled .obj files")
    ap.add_argument("--batch", action="store_true")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--verbose", "-v", action="store_true")
    ap.add_argument("--obj-dir", default="build/45410914/src")
    ap.add_argument("--check", action="store_true",
                    help="Dry-run and EXIT 2 if any object in the build tree "
                         "still needs this pass")
    ap.add_argument("files", nargs="*")
    args = ap.parse_args()
    if args.check:
        # --check never writes.  A checker that could mutate the tree it is
        # auditing is not a checker.
        args.apply = False
    if not args.batch and not args.files:
        ap.error("Specify --batch or provide files")

    targets = ([Path(p) for p in sorted(
        glob.glob(str(Path(args.obj_dir) / "**/*.obj"), recursive=True))]
        if args.batch else [Path(f) for f in args.files])

    checked = patched = total = 0
    errors: List[str] = []
    for t in targets:
        if not t.exists():
            continue
        checked += 1
        n, err = patch_file(t, args.apply)
        if err:
            errors.append(err)
            continue
        if n:
            patched += 1
            total += n
            if args.verbose:
                print(f"{t}: {n} boundaries")
    mode = "APPLIED" if args.apply else "DRY RUN"
    print(f"[{mode}] {checked} files checked, {patched} files patched, "
          f"{total} total EH boundaries")
    for e in errors:
        print("ERROR: " + e, file=sys.stderr)
    if errors:
        return 1
    if args.check and patched:
        # Exit 2 -- the convention obj_anon_ns_patcher.py established in lane
        # CN-1.  The non-zero exit is the point: it is what lets an edge in
        # build.ninja, or scripts/verify_objs_patched.py, refuse rather than
        # report a number derived from raw compiler output.
        print("FAIL[eh_boundary]: %d pending patch(es) -- this build tree "
              "carries objects that were compiled but never post-processed."
              % patched, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
