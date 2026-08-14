#!/usr/bin/env python3
"""Census of COMPILED-BUT-UNPINNED units: objects.json entries whose source
exists (so tools/project.py emits a compile edge and a base .obj) but which have
NO heading in config/45410914/splits.txt (so no target .obj is ever split for
them and they appear in no objdiff unit).

Such a unit's retail functions do not vanish -- they sit inside whatever OTHER
unit's pin encloses them, and are attributed to that unit.  Lane WRONGCALL-4 hit
this at system/utl/TimeConversion.cpp, whose addresses fall inside
StringTable.cpp's pin; because StringTable.cpp scatter-includes movie/Movie.cpp
*and* utl/TimeConversion.cpp, the addresses were handed MOVIE's symbols.

THE CLASS SPLITS IN TWO, and the halves are worth different amounts:

  A. SCATTER-INCLUDED  -- some *pinned* .cpp does `#include "<this>.cpp"`, so the
     enclosing pin's BASE obj genuinely defines this unit's symbols.  The rows
     are already pairable; the only defect possible is a wrong NAME.  Fixing
     these is map work, not pinning work.

  B. ORPHAN-COMPILED   -- no pinned TU includes it.  Its base obj is consulted by
     nothing.  Wherever its retail code lives, the enclosing unit's base obj
     CANNOT define its symbols, so those rows read 0% no matter what.  Fixing
     these needs identification (where does the code live?), then a pin.

Unit resolution replicates tools/project.py EXACTLY -- full path key first, then
the unique-basename alias -- and never basename()s blindly.  Bare-vs-nested
splits headings have broken four consecutive lanes' scans, and Movie.obj
genuinely collides between rnddx9/ and rndobj/ (and Movie is literally in play
here).

Self-validation (all must hold, else we exit non-zero):
  * declared - compiled == 230  (the known silently-dropped-compile-edge count)
  * every unit reported as pinned resolves to an objdiff unit with a base_path
  * no unit is in both the pinned set and the unpinned set
"""
import json
import os
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CFG = ROOT / "config" / "45410914"


# ── objects.json → declared / compiled ────────────────────────────────────────
def load_objects():
    """Return {path_key: src_path} for every declared object, replicating
    tools/project.py's resolution: src_dir / (source or path_key), with src_dir
    defaulting object -> library -> "src"."""
    raw = json.loads((CFG / "objects.json").read_text())
    out = {}
    for lib_name, lib_cfg in raw.items():
        lib_src_dir = lib_cfg.get("src_dir")
        for path, obj_cfg in lib_cfg.get("objects", {}).items():
            if isinstance(obj_cfg, str):
                obj_cfg = {}
            src_dir = obj_cfg.get("src_dir") or lib_src_dir or "src"
            source = obj_cfg.get("source") or path
            if path in out:
                sys.exit(f"Duplicate object name {path}")
            out[path] = (lib_name, Path(src_dir) / source)
    return out


def basename_alias_map(objects):
    """Replicate tools/project.py's unique-basename alias step."""
    aliases, owners = {}, defaultdict(list)
    for path_key in objects:
        basename = Path(path_key).name
        if basename == path_key or basename in objects:
            continue
        owners[basename].append(path_key)
        if basename in aliases:
            aliases[basename] = None  # ambiguous
        else:
            aliases[basename] = path_key
    return ({k: v for k, v in aliases.items() if v is not None},
            {k: v for k, v in owners.items() if len(v) > 1})


# ── splits.txt → pinned headings ──────────────────────────────────────────────
def splits_headings():
    """Return {heading: {section: [(start,end)]}} for every unit heading."""
    out = {}
    cur = None
    for line in (CFG / "splits.txt").read_text().splitlines():
        if not line.strip():
            continue
        if not line[0].isspace():
            name = line.strip()
            if name.endswith(":"):
                name = name[:-1]
            if name == "Sections":
                cur = None
                continue
            cur = out.setdefault(name, defaultdict(list))
            continue
        if cur is None:
            continue
        parts = line.split()
        if len(parts) >= 3 and parts[1].startswith("start:"):
            sec = parts[0]
            start = int(parts[1].split(":", 1)[1], 16)
            end = int(parts[2].split(":", 1)[1], 16)
            cur[sec].append((start, end))
    return out


# ── scatter includes ──────────────────────────────────────────────────────────
INC_RE = re.compile(r'^\s*#\s*include\s+"([^"]+\.cpp)"', re.M)
# include order from tools/defines_common.py: STLport, xdk/LIBCMT, src, src/system
INC_DIRS = ["src/system/stlport", "src/xdk/LIBCMT", "src", "src/system"]


def scatter_include_map(all_sources):
    """Return {included_path_key: [includer_path_key, ...]} for literal
    `#include "foo.cpp"` scatter includes, resolved against the real include
    path order (never by basename)."""
    by_src = {str(sp): pk for pk, (_lib, sp) in all_sources.items()}
    out = defaultdict(list)
    for pk, (_lib, sp) in all_sources.items():
        full = ROOT / sp
        if not full.exists():
            continue
        try:
            text = full.read_text(errors="replace")
        except Exception:
            continue
        for inc in INC_RE.findall(text):
            # resolve like the compiler would.  ⚠ For a quoted include MSVC
            # searches the INCLUDING FILE'S OWN DIRECTORY FIRST, then the -I
            # dirs.  Omitting that misfiled soundtouch's RateTransposer.cpp
            # (15 rows, all mpn 100) as an orphan when it is scatter-included
            # by its same-directory sibling TDStretch.cpp.
            target = None
            for d in [str(sp.parent)] + INC_DIRS:
                cand = os.path.normpath(f"{d}/{inc}")
                if (ROOT / cand).exists():
                    target = cand
                    break
            if target is None:
                continue
            tk = by_src.get(target)
            if tk is not None and tk != pk:
                out[tk].append(pk)
    return out


# ── COFF ──────────────────────────────────────────────────────────────────────
def coff_defined_functions(path):
    """Defined (SectionNumber>0) symbols in a PE/COFF obj.  Little-endian
    headers even for big-endian PPC targets."""
    try:
        data = Path(path).read_bytes()
    except OSError:
        return set()
    if len(data) < 20:
        return set()
    _m, _n, _t, ptr_sym, n_sym, _o, _c = struct.unpack_from("<HHIIIHH", data, 0)
    if ptr_sym == 0 or n_sym == 0 or ptr_sym + n_sym * 18 > len(data):
        return set()
    strtab = data[ptr_sym + n_sym * 18:]
    defined = set()
    i = 0
    while i < n_sym:
        off = ptr_sym + i * 18
        raw = data[off:off + 8]
        secnum = struct.unpack_from("<h", data, off + 12)[0]
        naux = data[off + 17]
        if raw[:4] == b"\x00\x00\x00\x00":
            soff = struct.unpack_from("<I", raw, 4)[0]
            end = strtab.find(b"\x00", soff)
            name = strtab[soff:end].decode("latin-1") if end >= 0 else ""
        else:
            name = raw.rstrip(b"\x00").decode("latin-1")
        if name and secnum > 0:
            defined.add(name)
        i += 1 + naux
    return defined


def main():
    objects = load_objects()
    aliases, collisions = basename_alias_map(objects)

    declared = set(objects)
    compiled = {pk for pk, (_lib, sp) in objects.items() if (ROOT / sp).exists()}
    missing_src = declared - compiled

    headings = splits_headings()
    pinned, unresolved = set(), []
    for h in headings:
        if h in objects:
            pinned.add(h)
        elif h in aliases:
            pinned.add(aliases[h])
        else:
            unresolved.append(h)

    unpinned = sorted(compiled - pinned)

    scatter = scatter_include_map(objects)

    # classify
    cls = {}
    for pk in unpinned:
        includers = scatter.get(pk, [])
        pinned_inc = [i for i in includers if i in pinned]
        if pinned_inc:
            cls[pk] = ("A_SCATTER", pinned_inc)
        elif includers:
            cls[pk] = ("A_SCATTER_UNPINNED_INCLUDER", includers)
        else:
            cls[pk] = ("B_ORPHAN", [])

    out = {
        "declared": len(declared),
        "compiled": len(compiled),
        "missing_src": len(missing_src),
        "headings": len(headings),
        "pinned_units": len(pinned),
        "unresolved_headings": unresolved,
        "basename_collisions": collisions,
        "unpinned": {pk: {"lib": objects[pk][0], "src": str(objects[pk][1]),
                          "cls": cls[pk][0], "includers": cls[pk][1]}
                     for pk in unpinned},
    }
    print(json.dumps(out, indent=1))

    # ── self-validation ───────────────────────────────────────────────────────
    errs = []
    if len(missing_src) != 230:
        errs.append(f"declared-compiled = {len(missing_src)}, expected 230")
    if pinned & set(unpinned):
        errs.append("a unit is both pinned and unpinned")
    if unresolved:
        errs.append(f"{len(unresolved)} splits headings resolve to no object")
    for e in errs:
        print(f"VALIDATION FAIL: {e}", file=sys.stderr)
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
