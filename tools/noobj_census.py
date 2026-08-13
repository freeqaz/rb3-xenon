#!/usr/bin/env python3
"""Census of report rows that can NEVER pair, and why.

A row pinned to a unit with no compiled base object can never pair under any
ruler: it sits in total_functions/total_code as pure denominator with zero
possible credit.  This tool decomposes the whole binary into

    PAIRABLE            unit has a base obj  -> credit is reachable
    UNPAIRABLE_NOSRC    named unit, no base obj (source file absent)
    UNPAIRABLE_AUTO     dtk auto_* unit (address range not attributed to a TU)

and, for the named-unpairable class, says *why* each unit produces no obj:

    SRC_MISSING         declared in objects.json, src_path does not exist
    NO_OBJECTS_ENTRY    pinned in splits.txt but absent from objects.json
                        (-> addable as NonMatching scaffolding)

Run from a built worktree (needs objdiff.json + build/<id>/report.json).

    python3 tools/noobj_census.py
    python3 tools/noobj_census.py --map      # also census target_symbol_map rows
                                             # + the class-(c) mis-pin test

TRAPS this tool exists to avoid (each has cost a lane):

  * splits.txt headings are 707 bare / 569 nested and basenames COLLIDE
    (Utl.cpp x5, Movie.cpp across rnddx9/ and rndobj/).  Everything here keys
    on the FULL unit path.  Never join on basename -- a basename fallback
    reports rnddx9/Cam.cpp as "pinned" when the bare Cam.cpp heading belongs
    to an entirely different unit.
  * report.json numerics are JSON *strings* and protobuf-JSON omits defaults,
    so every read is int(x.get(k, 0)).  An un-coerced compare returns a clean,
    decisive-looking "0 rows".
  * total_code / total_functions MOVE when splits pins change.  Read them from
    report.json; never hardcode.
  * The mis-pin test must not be run against the symbol map alone: the map is
    address->name and the build enforces name-injectivity, so "is this name in
    the map twice" is VACUOUS and returns 0 by construction.  The honest test
    asks whether a *compiled obj* defines the symbol, and ships with a control
    (healthy rows) that must fire.

Self-validation: rows/bytes summed over all units must equal total_functions /
total_code exactly, or the join dropped rows and every number below is wrong.
"""

import argparse
import bisect
import collections
import glob
import json
import os
import struct
import sys
from pathlib import Path

DEFAULT_REPORT = "build/45410914/report.json"


def load_text_spans(splits_path):
    """[(start, end, heading)] for every .text range, keyed on FULL heading."""
    spans, head = [], None
    with open(splits_path) as fh:
        for line in fh:
            if line and not line[0].isspace():
                s = line.strip()
                head = s[:-1] if s.endswith(":") else None
                continue
            if head and head != "Sections" and ".text" in line and "start:" in line:
                parts = line.split()
                start = int(next(p for p in parts if p.startswith("start:"))[6:], 16)
                end = int(next(p for p in parts if p.startswith("end:"))[4:], 16)
                spans.append((start, end, head))
    spans.sort()
    return spans


def unit_of_heading(head):
    for suf in (".cpp", ".c", ".s"):
        if head.endswith(suf):
            return head[: -len(suf)]
    return head


def coff_defined_symbols(path):
    """External symbols DEFINED by a COFF object (SectionNumber > 0)."""
    data = open(path, "rb").read()
    if len(data) < 20:
        return []
    _mach, _nsec, _ts, psym, nsym, _opt, _ch = struct.unpack_from("<HHIIIHH", data, 0)
    if psym == 0 or nsym == 0 or psym + nsym * 18 > len(data):
        return []
    strtab = psym + nsym * 18
    out, i = [], 0
    while i < nsym:
        off = psym + i * 18
        raw = data[off : off + 8]
        _val, secnum, _typ, sclass, naux = struct.unpack_from("<IhHBB", data, off + 8)
        if raw[:4] == b"\x00\x00\x00\x00":
            soff = struct.unpack_from("<I", raw, 4)[0]
            end = data.index(b"\x00", strtab + soff)
            name = data[strtab + soff : end].decode("latin1")
        else:
            name = raw.rstrip(b"\x00").decode("latin1")
        if sclass == 2 and secnum > 0:  # IMAGE_SYM_CLASS_EXTERNAL, defined
            out.append(name)
        i += 1 + naux
    return out


def resolve_objects():
    """objects.json path_key -> Object, with project.py's basename aliases.

    Mirrors generate_build() so this tool cannot drift from what the build
    actually resolves.  Ambiguous basenames are deliberately NOT aliased.
    """
    sys.argv = ["configure.py"]
    import importlib.util

    # configure.py calls generate_build() at import time, which REWRITES
    # build.ninja / objdiff.json / compile_commands.json.  This census is
    # read-only, so no-op it before the module body binds the name.  (Without
    # this, merely running the census dirties the worktree's build graph.)
    sys.path.insert(0, os.getcwd())
    import tools.project as _project

    _project.generate_build = lambda *a, **kw: None
    spec = importlib.util.spec_from_file_location("cfg", "configure.py")
    cfg = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(cfg)
    except SystemExit:
        pass
    objects = cfg.config.objects()
    alias, owners = {}, {}
    for path_key, obj in objects.items():
        base = Path(path_key).name
        if base == path_key or base in objects:
            continue
        owners.setdefault(base, []).append(path_key)
        alias[base] = None if base in alias else obj
    for base, obj in alias.items():
        if obj is not None and base not in objects:
            objects[base] = obj
    return objects, {k: v for k, v in owners.items() if len(v) > 1}


def why_no_obj(unit_name, objects):
    """Why does this pinned unit produce no compiled obj?"""
    for cand in (unit_name + ".cpp", unit_name + ".c", unit_name + ".s", unit_name):
        obj = objects.get(cand)
        if obj is None:
            continue
        if obj.src_path is None:
            return "NO_SRC_PATH", None
        exists = obj.src_path.exists()
        return ("SRC_PRESENT" if exists else "SRC_MISSING"), str(obj.src_path)
    return "NO_OBJECTS_ENTRY", None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--report", default=DEFAULT_REPORT)
    ap.add_argument("--splits", default="config/45410914/splits.txt")
    ap.add_argument("--objdiff", default="objdiff.json")
    ap.add_argument("--map", action="store_true", help="also census symbol-map rows")
    ap.add_argument("--symbol-map", default="scripts/target_symbol_map.json")
    args = ap.parse_args()

    report = json.load(open(args.report))
    objdiff = {u["name"]: u for u in json.load(open(args.objdiff))["units"]}
    m = report["measures"]
    total_code = int(m["total_code"])
    total_functions = int(m["total_functions"])
    matched_code = int(m["matched_code"])

    objects, ambiguous = resolve_objects()

    buckets = collections.defaultdict(lambda: [0, 0, 0])  # units, rows, bytes
    reasons = collections.defaultdict(lambda: [0, 0, 0])
    subsystem = collections.defaultdict(lambda: [0, 0])
    noobj_units = []
    rows_seen = bytes_seen = 0

    for unit in report["units"]:
        name = unit["name"]
        od = objdiff.get(name, {})
        fns = unit.get("functions") or []
        nrows = len(fns)
        nbytes = sum(int(f.get("size", 0)) for f in fns)
        rows_seen += nrows
        bytes_seen += nbytes
        auto = od.get("metadata", {}).get("auto_generated", False)
        if od.get("base_path"):
            key = "PAIRABLE"
        elif auto:
            key = "UNPAIRABLE_AUTO"
        else:
            key = "UNPAIRABLE_NOSRC"
            short = name[len("default/") :]
            reason, src = why_no_obj(short, objects)
            r = reasons[reason]
            r[0] += 1
            r[1] += nrows
            r[2] += nbytes
            parts = short.split("/")
            sub = "/".join(parts[:2]) if parts[0] == "xdk" else parts[0]
            subsystem[sub][0] += nrows
            subsystem[sub][1] += nbytes
            noobj_units.append((short, nrows, nbytes, reason, src))
        b = buckets[key]
        b[0] += 1
        b[1] += nrows
        b[2] += nbytes

    # ---- self-validation: the join must not drop rows -----------------------
    if rows_seen != total_functions or bytes_seen != total_code:
        sys.exit(
            "REFUSING: join dropped rows -- %d/%d rows, %d/%d bytes. Every number "
            "below would be wrong." % (rows_seen, total_functions, bytes_seen, total_code)
        )
    print("join self-validates: %d rows == total_functions, %d bytes == total_code"
          % (rows_seen, bytes_seen))
    print("ambiguous basenames (alias suppressed by project.py): %d" % len(ambiguous))

    print("\n%-22s %6s %8s %12s %8s" % ("class", "units", "rows", "bytes", "%code"))
    for key in ("PAIRABLE", "UNPAIRABLE_NOSRC", "UNPAIRABLE_AUTO"):
        u, r, b = buckets[key]
        print("%-22s %6d %8d %12d %7.2f%%" % (key, u, r, b, 100.0 * b / total_code))

    pairable = buckets["PAIRABLE"][2]
    print(
        "\nSTRUCTURAL CEILING on matched_code_percent at current pinning: %.2f%%"
        % (100.0 * pairable / total_code)
    )
    print(
        "  matched_code %d = %.2f%% of the PAIRABLE surface"
        % (matched_code, 100.0 * matched_code / pairable)
    )

    print("\nwhy the named-unpairable units produce no obj:")
    for k in sorted(reasons, key=lambda k: -reasons[k][2]):
        u, r, b = reasons[k]
        print("   %-18s %5d units %6d rows %10d B" % (k, u, r, b))
    if reasons.get("NO_OBJECTS_ENTRY"):
        print("   ^ NO_OBJECTS_ENTRY is the cheap class: addable as NonMatching.")

    print("\nnamed-unpairable by subsystem:")
    for k in sorted(subsystem, key=lambda k: -subsystem[k][1]):
        print("   %-24s %6d rows %10d B" % (k, *subsystem[k]))

    if args.map:
        census_map(args, total_code)


def census_map(args, total_code):
    """Census the symbol map, and run the class-(c) mis-pin test WITH a control."""
    spans = load_text_spans(args.splits)
    starts = [s[0] for s in spans]
    objdiff = json.load(open(args.objdiff))["units"]
    hasobj = {u["name"][len("default/") :]: bool(u.get("base_path")) for u in objdiff}
    symmap = json.load(open(args.symbol_map))

    noobj, healthy, outside = [], [], 0
    for addr, name in symmap.items():
        if not addr.startswith("0x"):
            continue  # metadata keys (_denylist, _comment, ...)
        a = int(addr, 16)
        i = bisect.bisect_right(starts, a) - 1
        if i < 0 or not (spans[i][0] <= a < spans[i][1]):
            outside += 1
            continue
        unit = unit_of_heading(spans[i][2])
        (healthy if hasobj.get(unit, False) else noobj).append((addr, name, unit))

    print("\n=== symbol-map rows ===")
    print("  in unit WITH obj : %d" % len(healthy))
    print("  in unit NO obj   : %d" % len(noobj))
    print("  outside any .text: %d  (data symbols)" % outside)

    print("\nbuilding defined-symbol index from compiled objs ...")
    defined = set()
    objs = glob.glob("build/45410914/src/**/*.obj", recursive=True)
    for p in objs:
        try:
            defined.update(coff_defined_symbols(p))
        except Exception:
            pass
    print("  %d objs, %d distinct defined external symbols" % (len(objs), len(defined)))

    hit_h = sum(1 for _, n, _ in healthy if n in defined)
    hit_n = [(a, n, u) for a, n, u in noobj if n in defined]
    print("\nclass-(c) mis-pin test -- is the symbol defined by a unit that compiles?")
    print(
        "  CONTROL   healthy rows defined by some obj: %d / %d (%.1f%%)"
        % (hit_h, len(healthy), 100.0 * hit_h / max(1, len(healthy)))
    )
    print(
        "  TREATMENT no-obj  rows defined by some obj: %d / %d (%.1f%%)"
        % (len(hit_n), len(noobj), 100.0 * len(hit_n) / max(1, len(noobj)))
    )
    if hit_h == 0:
        print("  ⚠ CONTROL DID NOT FIRE -- the test is vacuous; ignore the treatment.")
    for a, n, u in hit_n[:20]:
        print("     %s pinned:%s  %s" % (a, u, n[:70]))


if __name__ == "__main__":
    main()
