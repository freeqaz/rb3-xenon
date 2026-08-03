#!/usr/bin/env python3
"""lane EA-1: apply truncated-extent carves to symbols.txt.

A carve is two coupled edits and BOTH are required:
  1. widen the host function symbol's size from the truncated claim to the
     retail-byte true extent;
  2. delete every .text symbol that falls strictly INSIDE the new extent --
     the phantom 8-byte except_data_* EH prefixes and the spurious fn_<addr>
     fragments the splitter minted at the false boundary.  Leaving them would
     re-shatter the body on the next split.

Every carve here is double-witnessed:
  * retail bytes  -- the claimed extent contains ZERO exit instructions
                     (null rate for that witness: 0/5325), and the first exit
                     after the claim lands exactly at true_end;
  * our own build -- the compiled base body is EXACTLY true_end - addr bytes.
Two independent instruments agreeing on the same boundary.
"""
import re, sys, json

SYMPATH = '/home/free/tmp/laneEB1/wt/config/45410914/symbols.txt'
LINE = re.compile(r'^(\S+) = \.(\w+):0x([0-9A-Fa-f]+); // type:(\w+) size:0x([0-9A-Fa-f]+)(.*)$')


def apply(carves, path=SYMPATH, dry=False):
    """carves: list of (addr, new_size, label)"""
    lines = open(path).read().split('\n')
    out, changed, deleted = [], [], []
    # index: for each carve, the inner range
    ranges = [(a, a + n, lbl) for a, n, lbl in carves]
    bysize = {a: n for a, n, _ in carves}
    for ln in lines:
        m = LINE.match(ln)
        if not m:
            out.append(ln)
            continue
        name, sec, addr, typ, size = m.group(1), m.group(2), int(m.group(3), 16), m.group(4), int(m.group(5), 16)
        if sec != 'text':
            out.append(ln)
            continue
        if addr in bysize:
            new = bysize[addr]
            if size == new:
                out.append(ln)
                continue
            nl = f"{name} = .{sec}:0x{addr:08X}; // type:{typ} size:0x{new:X}{m.group(6)}"
            changed.append((addr, name, size, new))
            out.append(nl)
            continue
        # delete symbols strictly inside a carved range
        killed = False
        for lo, hi, lbl in ranges:
            if lo < addr < hi:
                deleted.append((addr, name, typ, size, lbl))
                killed = True
                break
        if not killed:
            out.append(ln)
    if not dry:
        open(path, 'w').write('\n'.join(out))
    return changed, deleted


if __name__ == '__main__':
    spec = json.load(open(sys.argv[1]))
    carves = [(int(c['addr'], 16), int(c['size'], 16), c['label']) for c in spec]
    ch, de = apply(carves, dry='--dry' in sys.argv)
    print(f"RESIZED {len(ch)} host symbols:")
    for a, n, o, nw in ch:
        print(f"  0x{a:08X} {n:<24} size 0x{o:X} -> 0x{nw:X}   (+{nw-o} B)")
    print(f"DELETED {len(de)} symbols absorbed into carved bodies:")
    for a, n, t, s, lbl in de:
        print(f"  0x{a:08X} {n:<28} type={t} size=0x{s:X}   [into {lbl}]")
    if len(ch) != len(carves):
        print(f"!! WARNING: {len(carves)} carves requested but only {len(ch)} host symbols resized")
        sys.exit(3)
