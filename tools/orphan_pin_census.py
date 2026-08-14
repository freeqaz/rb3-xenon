#!/usr/bin/env python3
"""Census of ORPHAN PINS: target-obj function symbols that carry a real (renamed)
mangled name but whose paired BASE obj does not define that name at all.

Such a row is structurally unpairable where it sits -- objdiff pairs by name, so
an absent base symbol means the row reads 0% REGARDLESS of how correct the name
is.  This is the shape lane CONTAINER-1 hit at 0x824f8968 (three
<Symbol,DataNode> COMDATs pinned to BandSongMetadata.cpp, which emits only the
<Symbol,String> variants).

Unit pairing is taken from objdiff.json (target_path / base_path), NEVER
reconstructed from basenames -- bare-vs-nested splits headings have broken four
consecutive lanes' scans, and Movie.obj genuinely collides between rnddx9/ and
rndobj/.

Self-validation: every symbol flagged must read fuzzy 0.0 in report.json.  A
flagged symbol scoring above 0 would mean the parser or the pairing is wrong.
"""
import json
import os
import struct
import sys
from collections import defaultdict

PLACEHOLDER_PREFIXES = ("fn_", "lbl_", "jumptable_", "data_", "bss_", "rdata_", "func_")


def coff_symbols(path):
    """Return (defined_names, all_names) for a PE/COFF object.

    defined_names = symbols with SectionNumber > 0 (actually defined here).
    all_names     = every symbol name in the table (incl. undefined/external refs).
    COFF headers are little-endian even for big-endian PowerPC targets.
    """
    with open(path, "rb") as fh:
        data = fh.read()
    if len(data) < 20:
        return set(), set()
    (_machine, _nsec, _ts, ptr_sym, n_sym, _opt, _chars) = struct.unpack_from("<HHIIIHH", data, 0)
    if ptr_sym == 0 or n_sym == 0 or ptr_sym + n_sym * 18 > len(data):
        return set(), set()
    strtab_off = ptr_sym + n_sym * 18
    strtab = data[strtab_off:]

    defined, allnames = set(), set()
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
        if name:
            allnames.add(name)
            if secnum > 0:
                defined.add(name)
        i += 1 + naux
    return defined, allnames


def main():
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")
    objdiff = json.load(open(os.path.join(root, "objdiff.json")))
    report = json.load(open(os.path.join(root, "build/45410914/report.json")))

    # report.json rows, keyed (unit_name, symbol) -> (size, fuzzy, mpn)
    rows = {}
    unit_rows = defaultdict(list)
    for u in report["units"]:
        un = u.get("name", "")
        for f in u.get("functions", []):
            nm = f.get("name", "")
            rec = (int(f.get("size", 0)), float(f.get("fuzzy_match_percent", 0.0)),
                   float(f.get("match_percent_normalized", 0.0)))
            rows[(un, nm)] = rec
            unit_rows[un].append(nm)

    orphans = []          # (unit, symbol, size, fuzzy)
    stats = defaultdict(int)
    checked_units = 0

    for unit in objdiff["units"]:
        name = unit.get("name", "")
        tp = unit.get("target_path")
        bp = unit.get("base_path")
        if not tp or not bp:
            stats["unit_no_pairing"] += 1
            continue
        tpa, bpa = os.path.join(root, tp), os.path.join(root, bp)
        if not os.path.exists(tpa):
            stats["unit_no_target_obj"] += 1
            continue
        if not os.path.exists(bpa):
            stats["unit_no_base_obj"] += 1   # the known 230 no-source class
            continue
        checked_units += 1
        tdef, _ = coff_symbols(tpa)
        bdef, ball = coff_symbols(bpa)
        for sym in tdef:
            key = (name, sym)
            if key not in rows:
                continue                      # not a scored function row
            size, fuzzy, mpn = rows[key]
            if sym.startswith(PLACEHOLDER_PREFIXES):
                stats["anon_rows"] += 1       # unnamed: a different, known class
                continue
            stats["named_rows"] += 1
            if sym in bdef:
                stats["named_paired"] += 1
                continue
            # base obj does not DEFINE it. Distinguish "referenced but undefined"
            # from "totally absent" -- both are unpairable, but the former means
            # the TU at least knows the symbol.
            stats["orphan"] += 1
            stats["orphan_ref_undef" if sym in ball else "orphan_absent"] += 1
            orphans.append((name, sym, size, fuzzy, mpn, sym in ball))

    # ---- self-validation: every orphan must read fuzzy 0.0 ----
    bad = [o for o in orphans if o[3] != 0.0]
    print(f"units checked            : {checked_units}")
    for k in sorted(stats):
        print(f"  {k:24s}: {stats[k]}")
    print()
    print(f"ORPHAN PINS              : {len(orphans)} rows, "
          f"{sum(o[2] for o in orphans):,} B")
    print(f"  base refs it (undef)   : {sum(1 for o in orphans if o[5])}")
    print(f"  base never mentions it : {sum(1 for o in orphans if not o[5])}")
    print()
    print(f"SELF-VALIDATION: orphans with fuzzy != 0.0 -> {len(bad)} "
          f"({'PASS' if not bad else 'FAIL'})")
    for o in bad[:10]:
        print("   ", o[0], o[1][:70], "fuzzy", o[3])

    by_unit = defaultdict(lambda: [0, 0])
    for u, s, sz, fz, mp, r in orphans:
        by_unit[u][0] += 1
        by_unit[u][1] += sz
    print("\nTop 30 units by orphaned bytes:")
    for u, (n, b) in sorted(by_unit.items(), key=lambda kv: -kv[1][1])[:30]:
        print(f"  {b:9,} B  {n:3} rows  {u}")

    with open(os.path.join(root, "orphan_pins.json"), "w") as fh:
        json.dump([{"unit": u, "symbol": s, "size": sz, "fuzzy": fz,
                    "mpn": mp, "referenced_by_base": r}
                   for u, s, sz, fz, mp, r in orphans], fh, indent=1)
    print("\nwrote orphan_pins.json")


if __name__ == "__main__":
    main()
