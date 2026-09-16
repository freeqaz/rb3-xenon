#!/usr/bin/env python3
"""Honest census of the RE-HOMABLE vein: retail rows at ``fuzzy == 0`` whose
mangled name IS DEFINED by one of our compiled objects -- just not by the object
the row's pin points at.

Why this exists (lane W16-FO, 2026-09-16)
-----------------------------------------
objdiff pairs target<->base BY NAME.  A retail row pinned into unit U reads
``fuzzy 0`` if ``U``'s compiled object cannot define that name, *however correct
our source is*.  W16-FL/FM showed the linker scatters template COMDATs far from
their owning TU, so a ``.text``-span pin can attribute a body to the wrong unit.
Re-homing the pin makes such a row pairable.

⛔ THE INSTRUMENT THAT MUST NOT BE USED
    ``symbol_name.encode() in open(obj, 'rb').read()``
A symbol NAME appears in a COFF whether the symbol is DEFINED there or merely
REFERENCED (an undefined external, i.e. a call site).  That test therefore
reports every *caller* as a *definer*.  Its top hits were XDK/CRT imports
(``memcpy``, ``pow``, ``XGGetTextureLayout``) -- pure call sites.

This tool keys on StorageClass + SectionNumber via
``undefined_externals_census.parse_coff_symbols`` and reports BOTH instruments so
the disagreement is measured, not asserted.

Storage classes matter in BOTH directions:
  * EXTERNAL (2) + section > 0  -> a definition objdiff can pair  (DEF_EXT)
  * STATIC   (3) + section > 0  -> a definition too (file-static / anon-ns).
    ``parse_coff_symbols`` does not return these, so counting only DEF_EXT is a
    vacuity in the OPPOSITE direction.  Tracked separately as DEF_STATIC.
  * EXTERNAL (2) + section == 0, value == 0 -> UNDEFINED reference (a caller!)
  * section == 0 with value != 0 -> COMMON: storage IS defined.

Usage
-----
    python3 tools/rehome_census.py                 # census
    python3 tools/rehome_census.py --json out.json
    python3 tools/rehome_census.py --selftest      # prove the two instruments
                                                   # actually disagree on a
                                                   # synthetic known answer
"""
import argparse
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from undefined_externals_census import parse_coff_symbols  # noqa: E402

IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3

# objdiff's placeholder prefixes (see is_placeholder_symbol_name in objdiff-core)
PLACEHOLDER = re.compile(r"^(fn|lbl|jumptable|data|bss|rdata)_")


def parse_all_symbols(path):
    """Return (def_ext, def_static, undef, allnames).

    def_ext / undef mirror parse_coff_symbols exactly (asserted by the caller);
    def_static adds the StorageClass==STATIC, SectionNumber>0 definitions that
    parse_coff_symbols deliberately omits; allnames is EVERY symbol-table name
    regardless of class -- the population the vacuous substring test sees.
    """
    try:
        data = Path(path).read_bytes()
    except OSError:
        return set(), set(), set(), set()
    if len(data) < 20:
        return set(), set(), set(), set()
    _m, _n, _t, ptr_sym, n_sym, _o, _c = struct.unpack_from("<HHIIIHH", data, 0)
    if ptr_sym == 0 or n_sym == 0 or ptr_sym + n_sym * 18 > len(data):
        return set(), set(), set(), set()
    strtab = data[ptr_sym + n_sym * 18:]

    def symname(off):
        raw = data[off:off + 8]
        if raw[:4] == b"\x00\x00\x00\x00":
            soff = struct.unpack_from("<I", raw, 4)[0]
            end = strtab.find(b"\x00", soff)
            return strtab[soff:end].decode("latin-1") if end >= 0 else ""
        return raw.rstrip(b"\x00").decode("latin-1")

    def_ext, def_static, undef, allnames = set(), set(), set(), set()
    i = 0
    while i < n_sym:
        off = ptr_sym + i * 18
        name = symname(off)
        value = struct.unpack_from("<I", data, off + 8)[0]
        secnum = struct.unpack_from("<h", data, off + 12)[0]
        sclass = data[off + 16]
        naux = data[off + 17]
        if name:
            allnames.add(name)
            if sclass == IMAGE_SYM_CLASS_EXTERNAL:
                if secnum > 0:
                    def_ext.add(name)
                elif secnum == 0 and value == 0:
                    undef.add(name)
            elif sclass == IMAGE_SYM_CLASS_STATIC and secnum > 0:
                def_static.add(name)
        i += 1 + naux
    return def_ext, def_static, undef, allnames


def build_indexes(repo):
    """name -> set(unit_name) for each instrument, over OUR compiled objects."""
    od = json.loads((repo / "objdiff.json").read_text())
    unit_base = {}
    for u in od["units"]:
        bp = u.get("base_path")
        if bp:
            unit_base[u["name"]] = repo / bp

    def_ext_idx = defaultdict(set)
    def_static_idx = defaultdict(set)
    undef_idx = defaultdict(set)
    any_idx = defaultdict(set)
    seen_paths = {}
    nobj = 0
    for unit, path in unit_base.items():
        if not path.exists():
            continue
        key = str(path)
        if key not in seen_paths:
            seen_paths[key] = parse_all_symbols(path)
            nobj += 1
            # anti-vacuity: our reader must agree with the in-tree one
            ref_def, ref_undef = parse_coff_symbols(path)[1], parse_coff_symbols(path)[0]
            if ref_def != seen_paths[key][0] or ref_undef != seen_paths[key][2]:
                raise SystemExit(f"READER DISAGREES with parse_coff_symbols on {path}")
        de, ds, un, an = seen_paths[key]
        for n in de:
            def_ext_idx[n].add(unit)
        for n in ds:
            def_static_idx[n].add(unit)
        for n in un:
            undef_idx[n].add(unit)
        for n in an:
            any_idx[n].add(unit)
    return unit_base, def_ext_idx, def_static_idx, undef_idx, any_idx, nobj


def census(repo):
    report = json.loads((repo / "build/45410914/report.json").read_text())
    unit_base, defext, defstat, undef, anysym, nobj = build_indexes(repo)

    rows = []
    for u in report["units"]:
        for f in u.get("functions", []):
            if "fuzzy_match_percent" in f:      # protobuf omits 0.0 => present means non-zero
                continue
            name = f["name"]
            if PLACEHOLDER.match(name):
                continue
            rows.append({
                "unit": u["name"],
                "name": name,
                "size": int(f["size"]),
                "mpn": f.get("match_percent_normalized", 0.0),
            })

    buckets = defaultdict(list)
    for r in rows:
        own = r["unit"]
        de, ds = defext.get(r["name"], set()), defstat.get(r["name"], set())
        alldef = de | ds
        r["def_ext_units"] = sorted(de)
        r["def_static_units"] = sorted(ds)
        r["undef_units"] = sorted(undef.get(r["name"], set()))
        r["any_units"] = sorted(anysym.get(r["name"], set()))
        r["has_base_obj"] = own in unit_base and unit_base[own].exists()
        if own in alldef:
            b = "DEFINED_IN_OWN_UNIT"
        elif alldef:
            b = "DEFINED_ELSEWHERE"          # <- the re-homable candidates
        elif r["any_units"]:
            b = "REFERENCED_ONLY"            # <- what the vacuous test miscounts
        else:
            b = "ABSENT_EVERYWHERE"
        r["bucket"] = b
        buckets[b].append(r)
    return rows, buckets, nobj


def fmt(bkt, rows_total, bytes_total):
    n = len(bkt)
    b = sum(r["size"] for r in bkt)
    return f"{n:6d} rows  {b:10,d} B  ({100.0*b/bytes_total:5.2f}% of vein bytes)"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--json")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--top", type=int, default=25)
    a = ap.parse_args()
    if a.selftest:
        return selftest()

    repo = Path(a.repo).resolve()
    rows, buckets, nobj = census(repo)
    N = len(rows)
    B = sum(r["size"] for r in rows)
    print(f"compiled objects indexed          : {nobj}")
    print(f"fuzzy-0 NAMED rows (the vein)     : {N} rows / {B:,} B")
    print()
    order = ["DEFINED_IN_OWN_UNIT", "DEFINED_ELSEWHERE", "REFERENCED_ONLY", "ABSENT_EVERYWHERE"]
    for k in order:
        print(f"  {k:22s} {fmt(buckets[k], N, B)}")
    print()
    vac = len(buckets["DEFINED_ELSEWHERE"]) + len(buckets["REFERENCED_ONLY"]) + len(buckets["DEFINED_IN_OWN_UNIT"])
    vacb = sum(r["size"] for k in ("DEFINED_ELSEWHERE", "REFERENCED_ONLY", "DEFINED_IN_OWN_UNIT") for r in buckets[k])
    print(f"VACUOUS instrument would report   : {vac} rows / {vacb:,} B  (name appears in SOME obj)")
    hon = len(buckets["DEFINED_ELSEWHERE"])
    honb = sum(r["size"] for r in buckets["DEFINED_ELSEWHERE"])
    print(f"HONEST re-homable vein            : {hon} rows / {honb:,} B")
    print()
    print(f"-- top {a.top} DEFINED_ELSEWHERE by size-if-it-crosses --")
    for r in sorted(buckets["DEFINED_ELSEWHERE"], key=lambda r: -r["size"])[:a.top]:
        tgt = (r["def_ext_units"] + r["def_static_units"])
        print(f"  {r['size']:6d} B  {r['unit']:34s} -> {','.join(tgt)[:60]:60s} mpn={r['mpn']:.2f}")
        print(f"          {r['name'][:150]}")
    if a.json:
        Path(a.json).write_text(json.dumps({"rows": rows}, indent=1))
        print(f"\nwrote {a.json}")


def selftest():
    """Build two synthetic COFFs -- one DEFINING a name, one only REFERENCING it
    -- and require the honest instrument to separate them while the vacuous
    substring test conflates them.  A test that cannot fail proves nothing, so
    this asserts the vacuous side ALSO reports a hit."""
    import tempfile

    def make_obj(name, defined):
        # 1 section, 1 symbol; long name forced into the string table
        nsec = 1
        nsym = 1
        symoff = 20 + 40
        hdr = struct.pack("<HHIIIHH", 0x1F2, nsec, 0, symoff, nsym, 0, 0)
        sec = struct.pack("<8sIIIIIIHHI", b".text\0\0\0", 0, 0, 0, 0, 0, 0, 0, 0, 0x60000020)
        strtab_data = name.encode() + b"\0"
        # name(8: 0,stroff) value(4) secnum(2) type(2) sclass(1) naux(1) = 18 B
        sym = struct.pack("<IIIhHBB", 0, 4, 0, 1 if defined else 0, 0x20, 2, 0)
        strtab = struct.pack("<I", 4 + len(strtab_data)) + strtab_data
        return hdr + sec + sym + strtab

    NAME = "?_M_fill_insert_aux@?$vector@VSfxMap@@V?$allocator@VSfxMap@@@_STL@@@_STL@@QAAXPAVSfxMap@@IABV2@@Z"
    with tempfile.TemporaryDirectory() as td:
        pdef = Path(td) / "definer.obj"
        pref = Path(td) / "caller.obj"
        pdef.write_bytes(make_obj(NAME, True))
        pref.write_bytes(make_obj(NAME, False))
        d_def, d_stat, d_undef, d_all = parse_all_symbols(pdef)
        r_def, r_stat, r_undef, r_all = parse_all_symbols(pref)
        ok = True
        checks = [
            ("definer: honest says DEFINED", NAME in d_def),
            ("definer: honest says not-undef", NAME not in d_undef),
            ("caller : honest says UNDEFINED", NAME in r_undef),
            ("caller : honest says NOT defined", NAME not in r_def),
            ("caller : VACUOUS substring says HIT (the bug)",
             NAME.encode() in pref.read_bytes()),
            ("definer: VACUOUS substring says HIT", NAME.encode() in pdef.read_bytes()),
            ("agrees with parse_coff_symbols (definer)", parse_coff_symbols(pdef)[1] == d_def),
            ("agrees with parse_coff_symbols (caller)", parse_coff_symbols(pref)[0] == r_undef),
            # anti-vacuity: the two agreements above are worthless if both sides
            # are empty, which is exactly how a broken fixture passes.
            ("NON-VACUOUS: definer's defined set is non-empty", len(d_def) == 1),
            ("NON-VACUOUS: caller's undef set is non-empty", len(r_undef) == 1),
        ]
        for label, cond in checks:
            print(f"  [{'OK ' if cond else 'FAIL'}] {label}")
            ok &= bool(cond)
        print("SELFTEST", "PASS" if ok else "FAIL")
        return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main() or 0)
