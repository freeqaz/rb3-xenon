#!/usr/bin/env python3
"""TU-wiring census: find map entries (functions retail HAS) that our build
does NOT emit and are NOT pinned. Each orphan = a function whose owning TU is
unwired. Group by class + address cluster, classify source availability.

orphan = (address not in any pinned .text range in splits.txt)
     AND (mangled name not a DEFINED symbol in any compiled obj under build/45410914/src)
"""
import glob
import json
import os
import re
import struct
import sys
from pathlib import Path

ROOT = Path("/home/free/tmp/wt-tucensus")
MAP = ROOT / "scripts" / "target_symbol_map.json"
SPLITS = ROOT / "config" / "45410914" / "splits.txt"
SRC_OBJ = ROOT / "build" / "45410914" / "src"


def parse_defined_syms(data):
    if len(data) < 20:
        return []
    sym_off = struct.unpack_from("<I", data, 8)[0]
    n = struct.unpack_from("<I", data, 12)[0]
    if not sym_off or not n:
        return []
    st = sym_off + n * 18
    out = []
    i = 0
    while i < n:
        eo = sym_off + i * 18
        if eo + 18 > len(data):
            break
        nb = data[eo:eo + 8]
        if nb[:4] == b"\x00\x00\x00\x00":
            so = struct.unpack_from("<I", nb, 4)[0]
            ao = st + so
            try:
                end = data.index(b"\x00", ao)
                name = data[ao:end].decode("ascii", "replace")
            except ValueError:
                name = ""
        else:
            name = nb.split(b"\x00")[0].decode("ascii", "replace")
        secn = struct.unpack_from("<h", data, eo + 12)[0]
        aux = data[eo + 17]
        if secn > 0:
            out.append(name)
        i += 1 + aux
    return out


def load_defined():
    defined = set()
    files = glob.glob(str(SRC_OBJ / "**" / "*.obj"), recursive=True)
    for f in files:
        try:
            defined.update(parse_defined_syms(open(f, "rb").read()))
        except Exception as e:
            print(f"WARN {f}: {e}", file=sys.stderr)
    return defined, len(files)


def load_pinned_ranges():
    ranges = []
    for line in SPLITS.read_text().splitlines():
        m = re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", line)
        if m:
            ranges.append((int(m.group(1), 16), int(m.group(2), 16)))
    ranges.sort()
    return ranges


def in_pinned(addr, ranges):
    # binary search
    lo, hi = 0, len(ranges)
    while lo < hi:
        mid = (lo + hi) // 2
        s, e = ranges[mid]
        if addr < s:
            hi = mid
        elif addr >= e:
            lo = mid + 1
        else:
            return True
    return False


def extract_class(mangled):
    """Extract owning class from an MSVC mangled name.
    ?Method@Class@@...  / ??0Class@@... / ?Method@Ns@Class@@ (nested)
    Return the class scope string (innermost@...outer) or '<global>'."""
    if not mangled.startswith("?"):
        return "<nonstd>"
    body = mangled[1:]
    if body.startswith("?"):
        # special name ??0 ??1 ??_G etc: strip leading ?<op>
        # find first @ after the operator token
        body = body[1:]  # now like '0Class@@..' or '_GClass@@'
        # drop leading operator chars until a letter/underscore that starts name? messy.
        # Simpler: split on @@ ; take the scope part before @@
    # scope = everything up to the first '@@'
    idx = body.find("@@")
    if idx < 0:
        return "<global>"
    scope = body[:idx]
    # scope is  name@class@ns  (reversed). For '?Foo@Bar@@' -> 'Foo@Bar'
    parts = scope.split("@")
    # for special names ??0Class the first part starts with operator digit
    # e.g. '0Bar' -> class is parts[1:] . For normal '?Foo@Bar' parts=['Foo','Bar']
    # We want the class = the LAST named scope segment (outermost is last)
    # but nested classes: Foo@Inner@Outer -> class chain Inner::Outer
    # Drop the method name (first part) if this was a normal ?method form.
    if mangled.startswith("??"):
        # operator: parts[0] is like '0','1','_G','_E'; class = parts[1:]
        cls = parts[1:] if len(parts) > 1 else parts
    else:
        # ?method@Class...: class = parts[1:]
        cls = parts[1:] if len(parts) > 1 else ["<global>"]
    if not cls:
        return "<global>"
    return "::".join(reversed([p for p in cls if p]))


def main():
    raw = json.loads(MAP.read_text())
    entries = []
    for k, v in raw.items():
        if not k.lower().startswith("0x"):
            continue
        try:
            addr = int(k.lower().removeprefix("0x"), 16)
        except ValueError:
            continue
        entries.append((addr, v))
    print(f"map entries: {len(entries)}", file=sys.stderr)

    ranges = load_pinned_ranges()
    print(f"pinned .text ranges: {len(ranges)}", file=sys.stderr)
    defined, nfiles = load_defined()
    print(f"compiled src objs: {nfiles}, defined syms: {len(defined)}", file=sys.stderr)

    orphans = []
    n_pinned = n_compiled = 0
    for addr, name in entries:
        if in_pinned(addr, ranges):
            n_pinned += 1
            continue
        if name in defined:
            n_compiled += 1
            continue
        orphans.append((addr, name))
    print(f"pinned: {n_pinned}, compiled-not-pinned: {n_compiled}, ORPHANS: {len(orphans)}", file=sys.stderr)

    # write orphan list
    out = ROOT / "scripts" / "_census_orphans.json"
    orphans.sort()
    data = [{"addr": f"0x{a:08X}", "name": n, "class": extract_class(n)} for a, n in orphans]
    out.write_text(json.dumps(data, indent=1))
    print(f"wrote {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
