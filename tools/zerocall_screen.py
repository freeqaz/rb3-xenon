#!/usr/bin/env python3
"""Zero-call-control screen: find retail map names our build NEVER calls.

Why this exists
---------------
objdiff's ``-c functionRelocDiffs=name_check`` (fleet fork) compares relocation
TARGET SYMBOL NAMES instead of unconditionally forgiving them. Used as an audit
instrument it exposes thousands of call sites where the retail map names one
callee and our compiled code calls another. Most of that is ICF fold-alias
noise; the job is separating noise from real defects.

The discriminator that works is a CONTROL, not a score. For every symbol name,
count the COFF ``IMAGE_REL_PPC_REL24`` (type 0x6) call relocations targeting it
across all of our compiled objects. If a name that the retail map uses has
**zero** call sites anywhere in our tree, our build can never agree with that
map row -- the row is unfalsifiable by construction.

This screen found two map defects worth +1.600411 pp of ``matched_code_percent``
under name_check (lane CB-11):
  * ``0x8274b0f8`` mapped ``?EasePolyIn@@YAMMMM@Z`` (0 call sites) was really
    ``?Int@DataNode@@QBAHPBVDataArray@@@Z`` (6,056 call sites in 514 objs).
  * ``0x8257b4a0`` mapped ``?Main@ObjectDir@@SAPAV1@XZ`` (0 call sites; it is
    defined inline in Dir.h so /O1 /Ob2 never emits it out of line) was really
    ``?Current@MetaPerformer@@SAPAV1@XZ`` (167 call sites in 47 objs).

CAVEAT -- THIS IS A CANDIDATE GENERATOR, NOT A VERDICT
------------------------------------------------------
Zero call sites is equally consistent with:
  (b) a WRONG MAP ROW, and
  (c) a legitimate ICF FOLD-ALIAS, where retail folded the body we emit into a
      sibling and the map happened to name the sibling.
It only becomes decisive when you ALSO read the retail body at the mapped
address and find it CONTRADICTS the name (no floating point in a float
function; a tail-branch where a plain getter is claimed; a body sitting in the
wrong TU's .text span). Measured on the full census, only ~23% of screen hits
are trivial (<=8 byte) bodies; the >48-byte bucket is the richest defect vein
because a large function cannot fold by accident.

Empirically the trivial fold groups are indexed by (instruction pattern, member
offset): every ``lwz r3, K(r3); blr`` getter in the binary folds to ONE survivor
per K (e.g. K=0x0 at 0x8274a9a8, K=0x10 at 0x8252e068). Those are class (c) and
belong in scripts/symbol_aliases.json, not in a map repoint.

TRAP: do NOT grep the mangled name in an .obj to answer "do we call this?".
grep matches COMDAT definitions and string-table entries, so it reported 162
objs for ?Main@ObjectDir@@ and 575 for ?EasePolyIn@@ -- both of which have zero
actual call sites. You must parse the relocation table.

Usage
-----
  # count call sites for specific symbols (fast, positive-control friendly)
  python3 tools/zerocall_screen.py --count '?Int@DataNode@@QBAHPBVDataArray@@@Z' ...

  # build/refresh the whole-tree REL24 index (~minutes)
  python3 tools/zerocall_screen.py --build-index

  # rank census target names that have a zero-call control
  python3 tools/zerocall_screen.py --screen ~/tmp/cb9_allsites.pkl

Always print a positive control (a symbol you KNOW is called) alongside any
zero result -- a header-parsing slip silently returns 0 for everything.
"""
import argparse
import collections
import json
import os
import pickle
import re
import struct
import sys

REL24 = 0x6  # IMAGE_REL_PPC_REL24
DEFAULT_INDEX = os.path.expanduser("~/tmp/cb11_relindex.pkl")
OBJ_ROOT = "build/45410914/src"
ASM_DIR = "build/45410914/asm"
MAP_PATH = "scripts/target_symbol_map.json"


def rel24_counts(path):
    """Return Counter of {symbol_name: n_REL24_call_relocations} for one COFF obj."""
    with open(path, "rb") as fh:
        d = fh.read()
    if len(d) < 20:
        return collections.Counter()
    # NB: nsec is at offset 2, but PointerToSymbolTable/NumberOfSymbols are at
    # offsets 8 and 12. Reading all three as one '<HII' from offset 2 picks up
    # TimeDateStamp instead and makes every count come back 0.
    (nsec,) = struct.unpack_from("<H", d, 2)
    symptr, nsym = struct.unpack_from("<II", d, 8)
    strtab = symptr + nsym * 18
    out = collections.Counter()
    cache = {}

    def symname(i):
        if i in cache:
            return cache[i]
        off = symptr + i * 18
        raw = d[off:off + 8]
        if raw[:4] == b"\0\0\0\0":
            (o,) = struct.unpack_from("<I", raw, 4)
            end = d.index(b"\0", strtab + o)
            v = d[strtab + o:end].decode("latin1")
        else:
            v = raw.rstrip(b"\0").decode("latin1")
        cache[i] = v
        return v

    for s in range(nsec):
        so = 20 + s * 40
        (relptr,) = struct.unpack_from("<I", d, so + 24)
        (nrel,) = struct.unpack_from("<H", d, so + 32)
        for r in range(nrel):
            _va, idx, typ = struct.unpack_from("<IIH", d, relptr + r * 10)
            if typ == REL24:
                out[symname(idx)] += 1
    return out


def build_index(root=OBJ_ROOT, out_path=DEFAULT_INDEX):
    total, nobjs = collections.Counter(), collections.Counter()
    for dirpath, _, files in os.walk(root):
        for f in files:
            if not f.endswith(".obj"):
                continue
            try:
                c = rel24_counts(os.path.join(dirpath, f))
            except Exception:
                continue
            total.update(c)
            for k in c:
                nobjs[k] += 1
    with open(out_path, "wb") as fh:
        pickle.dump((total, nobjs), fh)
    print("indexed %d distinct called symbols, %d total REL24 call sites -> %s"
          % (len(total), sum(total.values()), out_path))
    return total, nobjs


def load_index(path=DEFAULT_INDEX):
    if not os.path.exists(path):
        sys.exit("no index at %s -- run --build-index first" % path)
    with open(path, "rb") as fh:
        return pickle.load(fh)


def addr_sizes(asm_dir=ASM_DIR):
    """address -> retail function size, parsed from the dtk-split .s headers."""
    rx = re.compile(r"#\s+\.text:0x[0-9A-Fa-f]+\s+\|\s+0x([0-9A-Fa-f]+)\s+\|\s+size:\s+0x([0-9A-Fa-f]+)")
    sz = {}
    if not os.path.isdir(asm_dir):
        return sz
    for f in os.listdir(asm_dir):
        if not f.endswith(".s"):
            continue
        with open(os.path.join(asm_dir, f), errors="replace") as fh:
            for line in fh:
                m = rx.match(line)
                if m:
                    sz["0x" + m.group(1).lower()] = int(m.group(2), 16)
    return sz


def screen(sites_pkl, index_path=DEFAULT_INDEX):
    total, _ = load_index(index_path)
    with open(sites_pkl, "rb") as fh:
        sites = pickle.load(fh)
    with open(MAP_PATH) as fh:
        m = json.load(fh)
    inv = {}
    for k, v in m.items():
        if isinstance(v, str):
            inv.setdefault(v, k)
    sizes = addr_sizes()

    tcount = collections.Counter()
    bases = collections.defaultdict(collections.Counter)
    for _unit, _fn, lst in sites:
        for _kind, t, b in lst:
            if not isinstance(t, str) or not isinstance(b, str):
                continue
            tcount[t] += 1
            bases[t][b] += 1

    rows = [(c, t) for t, c in tcount.items()
            if total.get(t, 0) == 0 and t
            and not t.startswith(("except_data", "__save", "__rest"))]
    rows.sort(reverse=True)
    all_sites = sum(tcount.values())
    hit_sites = sum(c for c, _ in rows)
    print("ZERO-CALL-CONTROL SCREEN")
    print("  %d of %d distinct target names (%.1f%%) are never called by our build"
          % (len(rows), len(tcount), 100.0 * len(rows) / max(1, len(tcount))))
    print("  covering %d of %d census sites (%.1f%%)"
          % (hit_sites, all_sites, 100.0 * hit_sites / max(1, all_sites)))
    print("  CANDIDATE GENERATOR ONLY -- confirm each by reading the retail body.\n")
    for c, t in rows:
        a = inv.get(t)
        s = sizes.get(a)
        print("%5d sites  size=%s  @%s  %s"
              % (c, ("0x%X" % s) if s else "?", a or "UNMAPPED", t))
        for b, bc in bases[t].most_common(3):
            print("        %5d -> we call %s (our calls=%d)" % (bc, b, total.get(b, 0)))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build-index", action="store_true")
    ap.add_argument("--index", default=DEFAULT_INDEX)
    ap.add_argument("--count", nargs="+", metavar="SYMBOL")
    ap.add_argument("--screen", metavar="ALLSITES_PKL")
    a = ap.parse_args()
    if a.build_index:
        build_index(out_path=a.index)
    if a.count:
        total, nobjs = (load_index(a.index) if os.path.exists(a.index)
                        else build_index(out_path=a.index))
        for s in a.count:
            print("%7d call sites in %5d objs   %s" % (total.get(s, 0), nobjs.get(s, 0), s))
    if a.screen:
        screen(a.screen, a.index)
    if not (a.build_index or a.count or a.screen):
        ap.print_help()


if __name__ == "__main__":
    main()
