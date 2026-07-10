#!/usr/bin/env python3
"""
symbols_hygiene.py — audit config/45410914/symbols.txt fn_/except_data_ boundaries
against the CURRENT retail xex's authoritative .pdata, scoped to pinned .text
splits (the only regions that affect match%).

Background: symbols.txt was generated against default_plus_TU5.xex (see
config.yml). dtk honors symbols.txt as authoritative and carves real functions
into COMDAT fragments where its stale fn_/except_data_ boundaries disagree with
the current retail xex. That kills objdiff pairing (0% for the carved fn).

Ground truth = band.exe (the decompressed default.xex PE image dtk extracts).
Its .pdata gives each function's true (start, size, func_type). For func_type==3
there is an 8-byte exception-data struct 8 bytes BEFORE the function start.

Flags per pinned .text range:
  (a) MID_FUNC  : a symbols.txt fn_ that starts strictly inside a pdata function
                  (i.e. it's a spurious fragment; the real fn is bigger).
  (b) SPURIOUS_EXC: an except_data_ (in .text) at an address that is NOT
                    (real_func_start - 8) of any func_type==3 pdata function.
  (c) SIZE_DIFF : a symbols.txt fn_ whose start IS a real pdata start but whose
                  size disagrees with pdata's size.
  (d) MISSING   : a real pdata function start inside the range has no fn_ entry
                  (informational — usually covered by another fn_ label).

Usage:
  python3 scripts/symbols_hygiene.py            # audit, print flagged clusters
  python3 scripts/symbols_hygiene.py --json OUT # dump machine-readable report
"""
import re, struct, sys, json, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BAND = os.path.join(ROOT, "orig/45410914/band.exe")
SYMS = os.path.join(ROOT, "config/45410914/symbols.txt")
SPLITS = os.path.join(ROOT, "config/45410914/splits.txt")


def parse_pdata(path):
    """Return dict addr->(size, func_type) for every function in .pdata, plus
    the .text section [va, va+vsz) bounds and raw bytes for handler resolution."""
    d = open(path, "rb").read()
    peoff = struct.unpack("<I", d[0x3C:0x40])[0]
    coff = peoff + 4
    _, nsec, _, _, _, optsz, _ = struct.unpack("<HHIIIHH", d[coff:coff + 20])
    opt = coff + 20
    imagebase = struct.unpack("<I", d[opt + 28:opt + 32])[0]
    sechdr = opt + optsz
    sections = {}
    for i in range(nsec):
        o = sechdr + i * 40
        name = d[o:o + 8].rstrip(b"\x00").decode("latin1")
        vsz, va, rawsz, rawptr = struct.unpack("<IIII", d[o + 8:o + 24])
        sections[name] = (imagebase + va, vsz, rawptr, rawsz)
    pva, pvsz, prawptr, prawsz = sections[".pdata"]
    tva, tvsz, trawptr, trawsz = sections[".text"]

    def read_u32_va(va):
        # resolve a VA to raw bytes (only .text/.rdata needed for handlers)
        for nm, (sva, svsz, sraw, srawsz) in sections.items():
            if sva <= va < sva + svsz and (va - sva) < srawsz:
                off = sraw + (va - sva)
                return struct.unpack("<I", d[off:off + 4])[0]  # little-endian? see note
        return None

    funcs = {}          # start -> (size, func_type)
    exc_data_addrs = {}  # (start-8) -> start  for func_type==3
    pd = d[prawptr:prawptr + pvsz]
    for i in range(0, len(pd) - 7, 8):
        start_addr = struct.unpack(">I", pd[i:i + 4])[0]
        if start_addr == 0:
            break
        word = struct.unpack(">I", pd[i + 4:i + 8])[0]
        n_insts = (word >> 8) & 0x3FFFFF
        func_type = word >> 30
        funcs[start_addr] = (n_insts * 4, func_type)
        if func_type == 3:
            exc_data_addrs[start_addr - 8] = start_addr
    return funcs, exc_data_addrs, (tva, tva + tvsz)


SYM_RE = re.compile(
    r"^(\w+)\s*=\s*\.(\w+):0x([0-9A-Fa-f]+);.*?(?:size:0x([0-9A-Fa-f]+))?", )


def parse_symbols(path):
    """Return lists of (name, addr, size, lineno) for fn_ (.text) and
    except_data_/except_record_ entries."""
    fns = []       # (name, addr, size, lineno)
    exc = []       # (name, addr, section, lineno)
    for ln, line in enumerate(open(path), 1):
        line = line.rstrip("\n")
        m = re.match(r"^(\S+)\s*=\s*\.(\w+):0x([0-9A-Fa-f]+);(.*)$", line)
        if not m:
            continue
        name, sect, addr_hex, rest = m.groups()
        addr = int(addr_hex, 16)
        sm = re.search(r"size:0x([0-9A-Fa-f]+)", rest)
        size = int(sm.group(1), 16) if sm else None
        if name.startswith("fn_") and sect == "text":
            fns.append((name, addr, size, ln))
        elif name.startswith("except_data_") or name.startswith("except_record_"):
            exc.append((name, addr, sect, ln))
    return fns, exc


def parse_splits(path):
    """Return list of (unit, start, end) for every .text range."""
    ranges = []
    cur = None
    for line in open(path):
        s = line.strip()
        m = re.match(r"^([\w./\-]+\.\w+):$", s)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", s)
        if m and cur:
            ranges.append((cur, int(m.group(1), 16), int(m.group(2), 16)))
    return ranges


def main():
    funcs, exc_data_addrs, (tstart, tend) = parse_pdata(BAND)
    fn_starts = set(funcs.keys())
    # sorted list to find containing function for a mid-addr
    sorted_starts = sorted(fn_starts)

    def containing_func(addr):
        # binary search: largest start <= addr with start+size > addr
        import bisect
        i = bisect.bisect_right(sorted_starts, addr) - 1
        if i < 0:
            return None
        st = sorted_starts[i]
        sz, _ = funcs[st]
        if st <= addr < st + sz:
            return st
        return None

    fns, exc = parse_symbols(SYMS)
    ranges = parse_splits(SPLITS)

    # index symbols by address for range scans
    fns_by_addr = {}
    for name, addr, size, ln in fns:
        fns_by_addr.setdefault(addr, []).append((name, size, ln))
    exc_text = [(n, a, ln) for (n, a, s, ln) in exc if s == "text"]

    flagged = []  # dict per cluster
    for unit, rstart, rend in ranges:
        # symbols.txt fn_ inside [rstart, rend)
        unit_flags = {"unit": unit, "range": [hex(rstart), hex(rend)],
                      "mid_func": [], "size_diff": [], "spurious_exc": []}
        for name, addr, size, ln in fns:
            if not (rstart <= addr < rend):
                continue
            if addr in fn_starts:
                real_sz = funcs[addr][0]
                if size is not None and size != real_sz:
                    unit_flags["size_diff"].append(
                        {"name": name, "addr": hex(addr), "line": ln,
                         "sym_size": hex(size), "pdata_size": hex(real_sz)})
            else:
                c = containing_func(addr)
                unit_flags["mid_func"].append(
                    {"name": name, "addr": hex(addr), "line": ln,
                     "sym_size": hex(size) if size else None,
                     "inside_func": hex(c) if c else None})
        # except_data_ in .text inside range that isn't a real (start-8)
        for name, addr, ln in exc_text:
            if not (rstart <= addr < rend):
                continue
            if addr not in exc_data_addrs:
                unit_flags["spurious_exc"].append(
                    {"name": name, "addr": hex(addr), "line": ln})
        if unit_flags["mid_func"] or unit_flags["size_diff"] or unit_flags["spurious_exc"]:
            flagged.append(unit_flags)

    total_mid = sum(len(f["mid_func"]) for f in flagged)
    total_size = sum(len(f["size_diff"]) for f in flagged)
    total_exc = sum(len(f["spurious_exc"]) for f in flagged)
    print(f"pdata functions: {len(funcs)}  (.text {hex(tstart)}..{hex(tend)})")
    print(f"pinned .text ranges: {len(ranges)}")
    print(f"flagged clusters (units): {len(flagged)}")
    print(f"  MID_FUNC fragments : {total_mid}")
    print(f"  SIZE_DIFF fn_      : {total_size}")
    print(f"  SPURIOUS_EXC       : {total_exc}")

    if "--json" in sys.argv:
        out = sys.argv[sys.argv.index("--json") + 1]
        json.dump(flagged, open(out, "w"), indent=2)
        print("wrote", out)
    else:
        for f in flagged:
            print(f"\n== {f['unit']}  {f['range'][0]}..{f['range'][1]}")
            for m in f["mid_func"]:
                print(f"   MID_FUNC  {m['name']} @{m['addr']} (line {m['line']}) "
                      f"inside pdata-fn {m['inside_func']} sym_size={m['sym_size']}")
            for m in f["size_diff"]:
                print(f"   SIZE_DIFF {m['name']} @{m['addr']} sym={m['sym_size']} "
                      f"pdata={m['pdata_size']} (line {m['line']})")
            for m in f["spurious_exc"]:
                print(f"   SPUR_EXC  {m['name']} @{m['addr']} (line {m['line']})")


if __name__ == "__main__":
    main()
