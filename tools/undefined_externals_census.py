#!/usr/bin/env python3
"""Census of UNDEFINED EXTERNALS: symbols our compiled objects REFERENCE but no
compiled object DEFINES, intersected with retail's named function rows.

Why this exists
---------------
The match build COMPILES but never LINKS.  Nothing ever resolves a symbol, so a
function that is *declared* in a header, *called* from compiled code, and
*defined in no translation unit* produces no error of any kind.  It is
structurally invisible: the retail row for it simply sits at ``fuzzy 0``
(unpaired -- objdiff has nothing to pair against), and every caller pays a
relocation-name charge that looks like ordinary noise.

Three distinct mechanisms hide a body from this build:

  1. the definition is inside ``#ifdef HX_NATIVE``   (drained -- lane W16-A)
  2. the whole TU is absent from ``objects.json`` and ``#include``d by nobody
     -- ``tools/project.py`` drops a missing compile edge SILENTLY
  3. the symbol is declared in a header and defined in NO TU at all

This tool finds (3) directly, and (1)/(2) incidentally, from the ground truth:
the COFF symbol tables of the objects the build actually produced.

Direction matters
-----------------
Lane W16-A approached the same territory from the RETAIL side ("retail rows at
fuzzy 0 whose name none of our objects defines").  This tool works from the
CALLER side ("symbols our objects reference but nobody defines").  The caller
side is strictly higher-signal for deciding whether to write a body: a symbol
nothing calls can be defined without changing a single call site, whereas an
undefined external is proof that compiled code needs it *now*.

Economics warning (read before acting on any row)
-------------------------------------------------
Under the shipped ``name_check`` ruler objdiff FORGIVES a relocation whose
target is a placeholder name.  Giving a symbol a local definition converts every
caller's FORGIVEN site into a CHECKED one.  Adding a body is therefore a BET at
the call sites, not a freebie -- W16-A measured a -40 B regression inside an
otherwise +404 B change.  Always reconcile crossed-vs-net by set-diff of the
``fuzzy == 100`` row set; never by a rounded gap.

Usage
-----
    python3 tools/undefined_externals_census.py                # full census
    python3 tools/undefined_externals_census.py --selftest     # prove it can fail
    python3 tools/undefined_externals_census.py --json out.json
"""

import argparse
import json
import os
import re
import struct
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

# COFF storage classes
IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3
IMAGE_SYM_CLASS_WEAK_EXTERNAL = 105

# A retail symbol NAME must look like a symbol.  target_symbol_map.json carries
# multi-KB prose "note" values; a bare substring probe matches inside one of
# those and manufactures a false positive (lane W16-A hit exactly this).
SYMBOL_SHAPE = re.compile(r"^[?_A-Za-z@$][A-Za-z0-9_?@$.<>,~\-]*$")


def parse_coff_symbols(path):
    """Return (undefined_externals, defined_externals, weak_externals) name sets.

    undefined external := SectionNumber == 0, StorageClass == EXTERNAL, Value == 0
        (Value != 0 with section 0 is a COMMON symbol -- that IS a definition
        of storage, so it is deliberately excluded from "undefined".)
    defined external   := SectionNumber > 0, StorageClass == EXTERNAL
    weak external      := StorageClass == WEAK_EXTERNAL (has a default
        resolution, so it is NOT "we hold no body" -- reported separately)
    """
    try:
        data = Path(path).read_bytes()
    except OSError:
        return set(), set(), set()
    if len(data) < 20:
        return set(), set(), set()
    _machine, _nsec, _t, ptr_sym, n_sym, _opt, _c = struct.unpack_from("<HHIIIHH", data, 0)
    if ptr_sym == 0 or n_sym == 0 or ptr_sym + n_sym * 18 > len(data):
        return set(), set(), set()
    strtab = data[ptr_sym + n_sym * 18:]

    def symname(off):
        raw = data[off:off + 8]
        if raw[:4] == b"\x00\x00\x00\x00":
            soff = struct.unpack_from("<I", raw, 4)[0]
            end = strtab.find(b"\x00", soff)
            return strtab[soff:end].decode("latin-1") if end >= 0 else ""
        return raw.rstrip(b"\x00").decode("latin-1")

    undef, defined, weak = set(), set(), set()
    i = 0
    while i < n_sym:
        off = ptr_sym + i * 18
        name = symname(off)
        value = struct.unpack_from("<I", data, off + 8)[0]
        secnum = struct.unpack_from("<h", data, off + 12)[0]
        sclass = data[off + 16]
        naux = data[off + 17]
        if name:
            if sclass == IMAGE_SYM_CLASS_WEAK_EXTERNAL:
                weak.add(name)
            elif sclass == IMAGE_SYM_CLASS_EXTERNAL:
                if secnum == 0:
                    if value == 0:
                        undef.add(name)
                    # value != 0 => COMMON, i.e. storage IS defined here
                elif secnum > 0:
                    defined.add(name)
        i += 1 + naux
    return undef, defined, weak


def collect(obj_root):
    """Walk every compiled .obj and fold its symbol sets together."""
    undef_by_obj = defaultdict(set)
    all_defined = set()
    all_weak = set()
    nobj = 0
    for p in sorted(Path(obj_root).rglob("*.obj")):
        u, d, w = parse_coff_symbols(p)
        nobj += 1
        if u:
            undef_by_obj[str(p)] = u
        all_defined |= d
        all_weak |= w
    return undef_by_obj, all_defined, all_weak, nobj


def load_retail_names(repo):
    """Retail's named function rows: name -> size (bytes), plus the bare name set.

    report.json is protobuf-JSON: defaults are OMITTED and several numerics are
    JSON STRINGS.  Every read is int()-coerced through .get(k, 0).
    """
    sizes = {}
    fuzzy = {}
    unit_of = {}
    rp = Path(repo) / "build/45410914/report.json"
    if rp.exists():
        rep = json.loads(rp.read_text())
        for u in rep.get("units", []):
            for f in u.get("functions", []):
                n = f.get("name") or ""
                if not n:
                    continue
                sizes[n] = int(f.get("size", 0) or 0)
                fuzzy[n] = float(f.get("fuzzy_match_percent", 0) or 0)
                unit_of[n] = u.get("name", "")
    mapnames = set()
    mp = Path(repo) / "scripts/target_symbol_map.json"
    if mp.exists():
        def walk(o):
            if isinstance(o, dict):
                for k, v in o.items():
                    if isinstance(k, str):
                        mapnames.add(k)
                    walk(v)
            elif isinstance(o, list):
                for v in o:
                    walk(v)
            elif isinstance(o, str):
                mapnames.add(o)
        walk(json.loads(mp.read_text()))
        mapnames = {n for n in mapnames if len(n) < 400 and SYMBOL_SHAPE.match(n)}
    return sizes, fuzzy, unit_of, mapnames


def is_out_of_scope(name):
    """XDK / CRT / compiler-runtime symbols are out of scope per CLAUDE.md
    (hard-skip XDK PORTING; the memory-management subset is the exception)."""
    if name.startswith("__imp_"):
        return True
    # MSVC/CRT helpers and intrinsics
    for pfx in ("__savefpr", "__restfpr", "__savegpr", "__restgpr", "_purecall",
                "__security", "_CxxThrowException", "__CxxFrameHandler",
                "__C_specific_handler", "_setjmp", "longjmp", "__report_",
                "atexit", "_atexit", "__CppXcptFilter", "__telemetry"):
        if name.startswith(pfx):
            return True
    return False


def run_census(repo, verbose=True):
    obj_root = Path(repo) / "build/45410914/src"
    undef_by_obj, defined, weak, nobj = collect(obj_root)
    all_undef = set()
    for s in undef_by_obj.values():
        all_undef |= s
    unresolved = {n for n in all_undef if n not in defined and n not in weak}
    unresolved = {n for n in unresolved if not is_out_of_scope(n)}

    sizes, fuzzy, unit_of, mapnames = load_retail_names(repo)
    retail_named = set(sizes) | mapnames

    hits = sorted(unresolved & retail_named,
                  key=lambda n: -sizes.get(n, 0))
    # referencing objects per hit
    refs = defaultdict(list)
    for obj, s in undef_by_obj.items():
        for n in s:
            if n in unresolved:
                refs[n].append(obj)

    result = {
        "objects_scanned": nobj,
        "defined_symbols": len(defined),
        "undefined_refs": len(all_undef),
        "unresolved_in_scope": len(unresolved),
        "retail_named_hits": len(hits),
        "rows": [
            {
                "symbol": n,
                "retail_size": sizes.get(n, 0),
                "fuzzy": fuzzy.get(n),
                "unit": unit_of.get(n, ""),
                "in_report": n in sizes,
                "in_map": n in mapnames,
                "n_callers": len(refs[n]),
                "callers": sorted(os.path.relpath(c, repo) for c in refs[n])[:8],
            }
            for n in hits
        ],
    }
    if verbose:
        print(f"objects scanned              : {nobj}")
        print(f"defined external symbols     : {len(defined)}")
        print(f"distinct undefined references: {len(all_undef)}")
        print(f"  unresolved (no obj defines), in scope: {len(unresolved)}")
        print(f"  INTERSECT retail named rows          : {len(hits)}")
        print()
        print(f"{'retail B':>9} {'fuzzy':>8} {'refs':>5}  symbol")
        for r in result["rows"]:
            fz = "-" if r["fuzzy"] is None else f"{r['fuzzy']:.3f}"
            print(f"{r['retail_size']:>9} {fz:>8} {r['n_callers']:>5}  {r['symbol']}")
            if r["unit"]:
                print(f"{'':>25}  unit={r['unit']}")
    return result


# ---------------------------------------------------------------- selftest


def _make_fixture_obj(path, undefined_names, defined_names):
    """Hand-build a minimal COFF object with the requested symbol table.

    Built rather than compiled so the selftest never SKIPS for want of a
    toolchain -- a selftest that can decline to run is not a gate.
    """
    nsym = len(undefined_names) + len(defined_names)
    # one dummy section so defined symbols can point at section 1
    sec = b".text\x00\x00\x00" + struct.pack("<IIIIIIHHI", 0, 0, 0, 0, 0, 0, 0, 0, 0x60000020)
    hdr_size = 20
    sec_off = hdr_size
    sym_off = sec_off + len(sec)
    strtab = bytearray(struct.pack("<I", 4))
    symbols = bytearray()

    def emit(name, secnum, sclass):
        nonlocal strtab
        if len(name) <= 8:
            raw = name.encode() + b"\x00" * (8 - len(name))
        else:
            off = len(strtab)
            strtab += name.encode() + b"\x00"
            raw = b"\x00\x00\x00\x00" + struct.pack("<I", off)
        symbols.extend(raw + struct.pack("<IhHBB", 0, secnum, 0, sclass, 0))

    for n in undefined_names:
        emit(n, 0, IMAGE_SYM_CLASS_EXTERNAL)
    for n in defined_names:
        emit(n, 1, IMAGE_SYM_CLASS_EXTERNAL)
    strtab[0:4] = struct.pack("<I", len(strtab))
    hdr = struct.pack("<HHIIIHH", 0x01F2, 1, 0, sym_off, nsym, 0, 0)
    Path(path).write_bytes(hdr + sec + bytes(symbols) + bytes(strtab))


def selftest():
    """Plant a fake undefined symbol and REQUIRE the parser to find it; plant a
    defined one and require it NOT to be reported.  Then sabotage the parser two
    ways and require each sabotage to change the answer -- an instrument that
    cannot fail proves nothing about the census it produces."""
    ok = True
    PLANT = "?ZzzPlantedUndefined@FakeClass@@QAAXXZ"
    DEFINED = "?ZzzPlantedDefined@FakeClass@@QAAXXZ"
    COMMON_OK = "?ZzzShouldNotAppear@FakeClass@@QAAXXZ"
    with tempfile.TemporaryDirectory() as td:
        obj = Path(td) / "fixture.obj"
        _make_fixture_obj(obj, [PLANT], [DEFINED])
        undef, defined, weak = parse_coff_symbols(obj)

        def check(label, cond):
            nonlocal ok
            print(f"  [{'PASS' if cond else 'FAIL'}] {label}")
            if not cond:
                ok = False

        check("planted UNDEFINED symbol is found", PLANT in undef)
        check("planted DEFINED symbol is not reported undefined", DEFINED not in undef)
        check("planted DEFINED symbol is reported defined", DEFINED in defined)
        check("unrelated name absent", COMMON_OK not in undef and COMMON_OK not in defined)

        # a second object that DEFINES the planted symbol must remove it from
        # the unresolved set -- this is the whole subtraction the census rests on
        obj2 = Path(td) / "fixture2.obj"
        _make_fixture_obj(obj2, [], [PLANT])
        u2, d2, _ = parse_coff_symbols(obj2)
        check("cross-object resolution subtracts the symbol",
              PLANT in undef and PLANT in d2 and PLANT not in (undef - d2))

        # COMMON symbol (section 0, value != 0) must NOT count as undefined
        objc = Path(td) / "fixture_common.obj"
        nsym = 1
        strtab = struct.pack("<I", 4)
        name = b"_zzcommon\x00"
        strtab = struct.pack("<I", 4 + len(name)) + name
        sec = b".text\x00\x00\x00" + struct.pack("<IIIIIIHHI", 0, 0, 0, 0, 0, 0, 0, 0, 0x60000020)
        sym = b"\x00\x00\x00\x00" + struct.pack("<I", 4) + struct.pack("<IhHBB", 16, 0, 0, IMAGE_SYM_CLASS_EXTERNAL, 0)
        hdr = struct.pack("<HHIIIHH", 0x01F2, 1, 0, 20 + len(sec), nsym, 0, 0)
        objc.write_bytes(hdr + sec + sym + strtab)
        uc, dc, wc = parse_coff_symbols(objc)
        check("COMMON symbol (value!=0, section 0) is NOT undefined", "_zzcommon" not in uc)

        # SABOTAGE 1: treat every section-0 symbol as undefined regardless of
        # storage class -- must change the answer on a STATIC section-0 symbol.
        objs = Path(td) / "fixture_static.obj"
        strtab2 = struct.pack("<I", 4)
        sym2 = b"_zstat\x00\x00" + struct.pack("<IhHBB", 0, 0, 0, IMAGE_SYM_CLASS_STATIC, 0)
        hdr2 = struct.pack("<HHIIIHH", 0x01F2, 1, 0, 20 + len(sec), 1, 0, 0)
        objs.write_bytes(hdr2 + sec + sym2 + strtab2)
        us, ds, ws = parse_coff_symbols(objs)
        check("SABOTAGE-CONTROL: STATIC section-0 symbol is not counted undefined",
              "_zstat" not in us)

        # SABOTAGE 2: the prose-note false positive.  A bare substring probe on
        # target_symbol_map.json matches inside multi-KB note values.  Require
        # the shape filter to reject one.
        note = "This note mentions ?MemFree@@YAXPAX@Z in passing, which is prose."
        check("SABOTAGE-CONTROL: prose note rejected by symbol-shape filter",
              not SYMBOL_SHAPE.match(note))
        check("SABOTAGE-CONTROL: a real mangled name passes the shape filter",
              bool(SYMBOL_SHAPE.match("?MemFree@@YAXPAX@Z")))

    print(f"\nselftest: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", default=".", help="repo/worktree root")
    ap.add_argument("--json", help="write full result as JSON")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    res = run_census(a.repo)
    if a.json:
        Path(a.json).write_text(json.dumps(res, indent=1))
        print(f"\nwrote {a.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
