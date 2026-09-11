#!/usr/bin/env python3
"""Compare the population of `scripts/obj_eh_boundary_patcher.py` against the
population objdiff's OWN native EH-prefix back-off covers, and provide the
file-granular ablation (`strip`) the per-row swap needs.

WHY THIS EXISTS (lane W4-E, 2026-09-11)
---------------------------------------
The patcher plants a class-3 `$EH#####` symbol at every MSVC EH-funclet prefix
so objdiff stops our function where retail stops it.  As of objdiff `b76f376`
(2026-09-01, "Stop charging the MSVC EH funclet prefix to the preceding
function") objdiff does the same thing natively, by backing `next_address` off
by 8 in `infer_symbol_sizes`.  Two implementations of one mechanism, so the
question is not "does the pass work" but "does the pass still cover anything
objdiff does not".

The two firing conditions are NOT identical, which is the whole point of this
census.  At a candidate offset X in a `.text*` section:

    patcher (scripts/obj_eh_boundary_patcher.py:find_boundaries)
        reloc target at X   == "__CxxFrameHandler"      (exact)
        reloc target at X+4 startswith "__ehfuncinfo$"
        no class-2/3 symbol already at X
        X+8 <= SizeOfRawData
        -- content of the 8 bytes is NOT examined
        -- position relative to the next symbol is NOT examined

    objdiff (objdiff-core/src/obj/read.rs:msvc_eh_prefix_at + infer_symbol_sizes)
        the 8 bytes at X are ZERO                        <-- extra condition
        reloc target at X   startswith "__CxxFrameHandler" (broader)
        reloc target at X+4 startswith "__ehfuncinfo$"
        section kind is Code
        X == next_symbol.address - 8                     <-- extra condition,
        and the back-off is applied ONCE, not in a loop      positional

So three classes can exist where the patcher would still add value:

  NONZERO   the 8 prefix bytes are not zero -> objdiff declines, patcher fires.
  ADJACENT  the 8 bytes before X are themselves a prefix -> objdiff's single
            back-off reaches X but not X-8, the patcher bounds both.
  ORPHAN    no symbol starts at X+8 -> objdiff's positional test never looks
            at X at all.

If all three are empty, the pass is strictly redundant.  Anything non-empty is
residual coverage, which is a reason to keep the pass even at Delta 0 today.

MODES
-----
  census   classify every prefix site in the build tree (or given files)
  strip    write a copy of an .obj with every `$EH*` class-3 symbol removed --
           the file-granular ablation, for objdiff-cli -1/-2 against
           untouchable copies (the -1/-2 form is required: diffing inside the
           build tree lets ninja recompile the obj and wipe the treatment,
           which is how lane CM-3's first positive control came back vacuous)
  selftest three controls that can fail; see --selftest
  strip-tree   record a sha256 manifest of every compiled .obj, then remove the
               pass's `$EH` symbols from ALL of them, mtimes PRESERVED.  This is
               a whole-binary ablation of the pass that requires NO recompile
               and touches no ninja stamp, so `report generate` can be run
               directly on the result with exactly one variable changed.
               ⚠ It leaves the build tree ABLATED.  Restore with
                   python3 scripts/obj_eh_boundary_patcher.py --batch --apply
               then `verify-tree`, which fails unless every object is
               byte-identical to the manifest.  Control 1 is what licenses that
               restore: strip is the exact inverse of the pass.

CONTROLS (`--selftest`, on a real object from the build tree)
  1 ROUND-TRIP   strip then re-apply the patcher == the original bytes.  Proves
                 `strip` is the exact inverse of the pass, so an ablation diff
                 measures the pass and not some other edit.
  2 SABOTAGE     set one prefix word to a nonzero value -> that site MUST
                 reclassify from BOTH_COVER to NONZERO.  A classifier that
                 cannot report a difference between the two conditions would
                 report "strictly redundant" no matter what the tree held.
  3 VACUITY      the chosen object must contain >0 prefix sites, else controls
                 1 and 2 pass on an empty set.
"""

import argparse
import glob
import struct
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE.parent / "scripts"))

# Reuse the pass's own COFF parser -- one source of truth for the aux-record
# index arithmetic that a reimplementation got wrong once already (a relocation's
# SymbolTableIndex counts aux records; a list position does not).
import obj_eh_boundary_patcher as P  # noqa: E402

HANDLER_EXACT = P.HANDLER
FUNCINFO = P.FUNCINFO_PREFIX
PREFIX = 8


def _sections_with_data(data: bytes):
    """[(idx1, name, size_of_raw_data, ptr_to_raw_data, [(rva, symidx)])]."""
    nsec = struct.unpack_from("<H", data, 2)[0]
    opt = struct.unpack_from("<H", data, 16)[0]
    out = []
    for s in range(nsec):
        so = 20 + opt + s * 40
        if so + 40 > len(data):
            break
        name = data[so:so + 8].rstrip(b"\x00").decode("ascii", "replace")
        size = struct.unpack_from("<I", data, so + 16)[0]
        praw = struct.unpack_from("<I", data, so + 20)[0]
        prel = struct.unpack_from("<I", data, so + 24)[0]
        nrel = struct.unpack_from("<H", data, so + 32)[0]
        rels = []
        for r in range(nrel):
            ro = prel + r * 10
            if ro + 10 > len(data):
                break
            rels.append(struct.unpack_from("<II", data, ro))
        out.append((s + 1, name, size, praw, rels))
    return out


def classify(data: bytes) -> List[dict]:
    """One record per site THE PASS ACTS ON, classified against objdiff's own
    native back-off conditions as they would apply IN THE PASS'S ABSENCE.

    The object is stripped of its `$EH` symbols first, so the population is
    exactly `find_boundaries(pre-pass object)` and the `headed` test sees the
    symbol table objdiff would see without the treatment.  Keying on every
    prefix site in the file instead was wrong twice over: it counts each
    function's OWN leading prefix at COMDAT offset 0, where a real class-2/3
    symbol already sits and the pass deliberately does not fire, and it made the
    round-trip control pass on an object with nothing planted in it.
    """
    data, _ = strip_eh(data)
    pass_sites = {(s, o) for s, o in P.find_boundaries(data)}
    parsed = P._parse(data)
    if not parsed:
        return []
    symbols = parsed[5]
    byidx = {sy[4]: sy for sy in symbols}
    # Every symbol offset per section, split into boundary-capable (class 2/3)
    # and all-symbols (objdiff's positional test keys on the next symbol it
    # KEEPS, which for our purposes is the class-2/3 set: class-6 labels are
    # exactly what neither implementation treats as a boundary).
    bnd: Dict[int, set] = {}
    for name, value, sec, sclass, _raw in symbols:
        if sec > 0 and sclass in P.BOUNDARY_CLASSES:
            bnd.setdefault(sec, set()).add(value)

    recs: List[dict] = []
    for sidx, name, size, praw, rels in _sections_with_data(data):
        if not name.startswith(".text"):
            continue
        handler, funcinfo = set(), set()
        for rva, si in rels:
            nm = byidx.get(si, ("", 0, 0, 0, 0))[0]
            if nm == HANDLER_EXACT:
                handler.add(rva)
            elif nm.startswith(FUNCINFO):
                funcinfo.add(rva)
        if not handler:
            continue
        # Every structural prefix site, used only for the ADJACENT test...
        all_sites = {o for o in handler if o + 4 in funcinfo and o + PREFIX <= size}
        # ...but the POPULATION is only what the pass acts on.
        sites = sorted(o for o in all_sites if (sidx, o) in pass_sites)
        have = bnd.get(sidx, set())
        for off in sites:
            # objdiff's condition 1: the 8 bytes must be zero.
            body = data[praw + off:praw + off + PREFIX] if praw else b""
            zero = body == b"\x00" * PREFIX
            # objdiff's condition 2 (positional): some boundary symbol must
            # start at off+8, because objdiff only inspects next_address-8.
            # `have` is already the pre-pass symbol table (see strip above).
            headed = (off + PREFIX) in have
            # ADJACENT: the 8 bytes before this site are themselves a prefix,
            # which objdiff's single (non-looping) back-off cannot reach.
            adjacent = (off - PREFIX) in all_sites
            recs.append({
                "section": sidx,
                "offset": off,
                "zero": zero,
                "headed": headed,
                "adjacent": adjacent,
                "objdiff_covers": bool(zero and headed and not adjacent),
            })
    return recs


def _label(r: dict) -> str:
    if r["objdiff_covers"]:
        return "BOTH_COVER"
    if not r["zero"]:
        return "NONZERO"
    if r["adjacent"]:
        return "ADJACENT"
    return "ORPHAN"


def strip_eh(data: bytes) -> Tuple[bytes, int]:
    """Remove every class-3 `$EH*` symbol, restoring the pre-pass symbol table.

    The pass APPENDS its symbols at the end of the table, with inline (<=8 byte)
    names, so removing them is exact: no relocation index into the surviving
    prefix of the table moves, and the string table start -- derived from
    NumberOfSymbols -- returns to its original value.  Control 1 asserts the
    round-trip byte-for-byte rather than trusting that reasoning.
    """
    buf = bytearray(data)
    symoff = struct.unpack_from("<I", buf, 8)[0]
    nsym = struct.unpack_from("<I", buf, 12)[0]
    keep = bytearray()
    removed = 0
    i = 0
    while i < nsym:
        eo = symoff + i * 18
        rec = bytes(buf[eo:eo + 18])
        aux = rec[17]
        nb = rec[:8]
        inline = nb[:4] != b"\x00\x00\x00\x00"
        name = nb.split(b"\x00")[0].decode("ascii", "replace") if inline else ""
        if inline and name.startswith("$EH") and rec[16] == 3 and aux == 0:
            removed += 1
        else:
            keep += rec
            for a in range(aux):
                ao = eo + 18 * (a + 1)
                keep += bytes(buf[ao:ao + 18])
        i += 1 + aux
    strt = symoff + nsym * 18
    tail = bytes(buf[strt:])
    new = bytearray(buf[:symoff]) + keep + tail
    struct.pack_into("<I", new, 12, nsym - removed)
    return bytes(new), removed


def _one_obj_with_sites(objdir: str) -> Optional[Path]:
    """An object with >=1 site the pass ACTUALLY PLANTED a symbol at.

    Requiring a planted symbol, not merely a candidate site, is what keeps
    control 1 honest: on an object with nothing planted, strip removes 0 symbols
    and "byte-identical to original" is true no matter what strip does.  The
    first version of this selftest picked Main.obj and passed exactly that way.
    """
    for p in sorted(glob.glob(str(Path(objdir) / "**/*.obj"), recursive=True)):
        path = Path(p)
        data = path.read_bytes()
        if strip_eh(data)[1] > 0 and classify(data):
            return path
    return None


def selftest(objdir: str) -> int:
    obj = _one_obj_with_sites(objdir)
    if obj is None:
        print("SELFTEST VACUOUS: no object in %s carries a PLANTED $EH symbol. "
              "Build the tree first (a full ./tools/ninja-locked; the pass runs "
              "as a post-compile edge)." % objdir, file=sys.stderr)
        return 5
    original = obj.read_bytes()
    recs = classify(original)
    # Control 3: vacuity.
    n_planted = strip_eh(original)[1]
    print("control 3 VACUITY: %s carries %d planted $EH symbol(s) and %d site(s) "
          "in the pass population" % (obj.name, n_planted, len(recs)))
    if not recs or n_planted == 0:
        return 5
    rc = 0
    # Control 1: strip is the exact inverse of the pass.
    stripped, removed = strip_eh(original)
    sites_after = P.find_boundaries(stripped)
    reapplied = bytearray(stripped)
    n = P.patch(reapplied, sites_after)
    ok1 = bytes(reapplied) == original and removed > 0
    print("control 1 ROUND-TRIP: removed %d $EH symbol(s), patcher re-found %d, "
          "re-applied %d -> byte-identical to original: %s"
          % (removed, len(sites_after), n, ok1))
    if removed == 0:
        print("  FAIL: removed 0 symbols -- this comparison is VACUOUS.",
              file=sys.stderr)
    if not ok1:
        print("  FAIL: strip is not the inverse of the pass; an ablation diff "
              "using it would not be measuring the pass.", file=sys.stderr)
        rc = 1
    # Control 2: sabotage a prefix word -> the site must reclassify.
    target = next((r for r in recs if r["objdiff_covers"]), None)
    if target is None:
        print("control 2 SABOTAGE: SKIPPED -- no BOTH_COVER site in this object")
    else:
        secs = {s[0]: s for s in _sections_with_data(original)}
        praw = secs[target["section"]][3]
        sab = bytearray(original)
        sab[praw + target["offset"]] = 0x42  # nonzero -> objdiff declines
        after = [r for r in classify(bytes(sab))
                 if r["offset"] == target["offset"] and r["section"] == target["section"]]
        ok2 = bool(after) and _label(after[0]) == "NONZERO"
        print("control 2 SABOTAGE: site 0x%x BOTH_COVER -> %s (expected NONZERO): %s"
              % (target["offset"], _label(after[0]) if after else "GONE", ok2))
        if not ok2:
            print("  FAIL: the classifier cannot distinguish objdiff's "
                  "zero-bytes condition; a 'strictly redundant' verdict from "
                  "it would be unfalsifiable.", file=sys.stderr)
            rc = 1
    print("SELFTEST %s" % ("PASS" if rc == 0 else "FAIL"))
    return rc


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=("census", "strip", "selftest",
                                     "strip-tree", "verify-tree"))
    ap.add_argument("files", nargs="*")
    ap.add_argument("--obj-dir", default="build/45410914/src")
    ap.add_argument("--out", help="strip: output path")
    ap.add_argument("--manifest", default="/home/free/tmp/w4e/objs.sha256",
                    help="strip-tree/verify-tree: sha256 manifest path")
    ap.add_argument("--show", type=int, default=12,
                    help="census: how many non-BOTH_COVER sites to list")
    args = ap.parse_args()

    if args.mode == "selftest":
        return selftest(args.obj_dir)

    if args.mode in ("strip-tree", "verify-tree"):
        import hashlib
        objs = sorted(glob.glob(str(Path(args.obj_dir) / "**/*.obj"), recursive=True))
        man = Path(args.manifest)
        if args.mode == "strip-tree":
            man.parent.mkdir(parents=True, exist_ok=True)
            lines = []
            removed_total = files = 0
            for o in objs:
                path = Path(o)
                data = path.read_bytes()
                lines.append("%s  %s" % (hashlib.sha256(data).hexdigest(), o))
                new, removed = strip_eh(data)
                if removed:
                    st = path.stat()
                    path.write_bytes(new)
                    # PRESERVE MTIME -- an object newer than the patch stamps
                    # dirties `all_source` and makes the next ninja re-run the
                    # whole chain, which would silently un-ablate the tree.
                    import os as _os
                    _os.utime(path, ns=(st.st_atime_ns, st.st_mtime_ns))
                    removed_total += removed
                    files += 1
            man.write_text("\n".join(lines) + "\n")
            print("manifest: %s (%d objects)" % (man, len(objs)))
            print("ABLATED: %d files, %d $EH symbols removed" % (files, removed_total))
            if removed_total == 0:
                print("REFUSING to call this an ablation: 0 symbols removed. The "
                      "tree was already ablated, or the pass never ran.",
                      file=sys.stderr)
                return 5
            return 0
        want = {}
        for line in man.read_text().splitlines():
            h, _, o = line.partition("  ")
            want[o] = h
        bad = missing = 0
        for o in objs:
            if o not in want:
                missing += 1
                continue
            if hashlib.sha256(Path(o).read_bytes()).hexdigest() != want[o]:
                bad += 1
                print("DIFFERS: %s" % o)
        print("verify-tree: %d objects, %d differ, %d absent from manifest"
              % (len(objs), bad, missing))
        return 0 if (bad == 0 and missing == 0) else 1

    if args.mode == "strip":
        if len(args.files) != 1 or not args.out:
            ap.error("strip needs exactly one input file and --out")
        src = Path(args.files[0])
        data, removed = strip_eh(src.read_bytes())
        Path(args.out).write_bytes(data)
        print("stripped %d $EH symbol(s): %s -> %s" % (removed, src, args.out))
        return 0

    targets = ([Path(p) for p in sorted(
        glob.glob(str(Path(args.obj_dir) / "**/*.obj"), recursive=True))]
        if not args.files else [Path(f) for f in args.files])
    counts: Dict[str, int] = {}
    examples: Dict[str, List[str]] = {}
    files_with = 0
    for t in targets:
        if not t.exists():
            continue
        recs = classify(t.read_bytes())
        if recs:
            files_with += 1
        for r in recs:
            lab = _label(r)
            counts[lab] = counts.get(lab, 0) + 1
            if lab != "BOTH_COVER" and len(examples.setdefault(lab, [])) < args.show:
                examples[lab].append("%s sec%d +0x%x zero=%s headed=%s adjacent=%s"
                                     % (t.name, r["section"], r["offset"], r["zero"],
                                        r["headed"], r["adjacent"]))
    total = sum(counts.values())
    print("objects scanned: %d (%d carry >=1 prefix site)" % (len(targets), files_with))
    print("sites in the pass population: %d" % total)
    for lab in ("BOTH_COVER", "NONZERO", "ADJACENT", "ORPHAN"):
        n = counts.get(lab, 0)
        pct = (100.0 * n / total) if total else 0.0
        print("  %-11s %7d  %6.2f%%" % (lab, n, pct))
    residual = total - counts.get("BOTH_COVER", 0)
    print("RESIDUAL (patcher fires, objdiff's native back-off does not): %d" % residual)
    for lab, ex in sorted(examples.items()):
        print("-- %s --" % lab)
        for e in ex:
            print("   " + e)
    return 0


if __name__ == "__main__":
    sys.exit(main())
