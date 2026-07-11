#!/usr/bin/env python3
"""Variant of tu_wiring_census.py that reports the COMPILED-NOT-PINNED bucket
(map entries whose mangled name IS a defined symbol in a compiled obj, but
whose address is in NO pinned .text range) with full detail: address, name,
class, owning object file, and distance to nearest pinned range boundary.
"""
import glob
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path("/home/free/tmp/closeout31/wt-r3")
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
    defined = {}
    files = glob.glob(str(SRC_OBJ / "**" / "*.obj"), recursive=True)
    for f in files:
        try:
            for name in parse_defined_syms(open(f, "rb").read()):
                defined.setdefault(name, []).append(f)
        except Exception as e:
            print(f"WARN {f}: {e}", file=sys.stderr)
    return defined, len(files)


def load_pinned_ranges():
    ranges = []
    cur_unit = None
    for line in SPLITS.read_text().splitlines():
        mu = re.match(r"^(\S.*\.(?:cpp|c)):\s*$", line)
        if mu:
            cur_unit = mu.group(1)
        m = re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", line)
        if m:
            ranges.append((int(m.group(1), 16), int(m.group(2), 16), cur_unit))
    ranges.sort()
    return ranges


def in_pinned(addr, ranges):
    lo, hi = 0, len(ranges)
    while lo < hi:
        mid = (lo + hi) // 2
        s, e, _ = ranges[mid]
        if addr < s:
            hi = mid
        elif addr >= e:
            lo = mid + 1
        else:
            return True
    return False


def nearest_boundary(addr, ranges):
    """Return (distance, side, unit, start, end) for the closest pinned range."""
    best = None
    for s, e, u in ranges:
        d_start = abs(addr - s)
        d_end = abs(addr - e)
        d = min(d_start, d_end)
        side = "before-start" if addr < s else ("after-end" if addr >= e else "inside")
        if best is None or d < best[0]:
            best = (d, side, u, s, e)
    return best


def extract_class(mangled):
    if not mangled.startswith("?"):
        return "<nonstd>"
    body = mangled[1:]
    if body.startswith("?"):
        body = body[1:]
    idx = body.find("@@")
    if idx < 0:
        return "<global>"
    scope = body[:idx]
    parts = scope.split("@")
    if mangled.startswith("??"):
        cls = parts[1:] if len(parts) > 1 else parts
    else:
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

    compiled_not_pinned = []
    n_pinned = n_orphan = 0
    for addr, name in entries:
        if in_pinned(addr, ranges):
            n_pinned += 1
            continue
        if name in defined:
            objs = defined[name]
            d, side, unit, s, e = nearest_boundary(addr, ranges)
            compiled_not_pinned.append({
                "addr": f"0x{addr:08X}",
                "name": name,
                "class": extract_class(name),
                "objs": objs,
                "nearest_dist": d,
                "nearest_side": side,
                "nearest_unit": unit,
                "nearest_range": f"0x{s:08X}-0x{e:08X}",
            })
        else:
            n_orphan += 1
    print(f"pinned: {n_pinned}, compiled-not-pinned: {len(compiled_not_pinned)}, orphan: {n_orphan}", file=sys.stderr)

    compiled_not_pinned.sort(key=lambda x: x["addr"])
    out = ROOT / "scripts" / "_census_compiled_not_pinned_r3.json"
    out.write_text(json.dumps(compiled_not_pinned, indent=1))
    print(f"wrote {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
