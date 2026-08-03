#!/usr/bin/env python3
"""Read a retail BEGIN_HANDLERS chain's handler order straight out of the asm.

A Milo `Handle` dispatcher compares _msg->Sym(0) against one lazily-initialised
static Symbol per handler, IN SOURCE ORDER. Each arm sets up its Symbol from a
string literal, so the sequence of string literals in the function body IS the
source order of the HANDLE_* macros. Diffing that against our BEGIN_HANDLERS
block finds missing / extra / reordered handlers directly, instead of guessing
from insert/delete clusters.

Usage: handler_order.py <Unit.s> <func_va_hex> <size_hex_or_dec>

CONTROLS (INSTRUMENT_DESIGN):
  * a positive control on the string-reading formula -- if the .rdata bias is
    wrong, every string comes back garbage, so we REFUSE unless a known string
    round-trips.
  * strings are read in PYTHON, never via the shell's grep, which is
    binary-blind (ugrep -I) and returns only false negatives.
  * we report the count of arms found; a chain that yields 0 or 1 arms is
    reported as SUSPECT rather than as "no handlers".
"""
import re, sys, struct

import os
# Repo root by default; override with DECOMP_ROOT to point at a worktree.
WT = os.environ.get('DECOMP_ROOT') or os.environ.get('DP1_ROOT') \
     or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
exe = open(f'{WT}/orig/45410914/band.exe', 'rb').read()
RDATA_BIAS = 0x82000000   # valid for .rdata (see band.exe read traps)


def cstr(va):
    o = va - RDATA_BIAS
    if not (0 <= o < len(exe)):
        return None
    s = exe[o:o + 64].split(b'\x00')[0]
    try:
        t = s.decode('ascii')
    except UnicodeDecodeError:
        return None
    return t if t and all(32 <= ord(c) < 127 for c in t) else None


# positive control: a string we already resolved by hand in this lane
assert cstr(0x8209E288) == 'is_demo', 'RDATA_BIAS control FAILED -- refusing to report'


def arms(path, va0, size):
    """Ordered list of (offset, string) for every string literal referenced."""
    out, seen = [], set()
    pat = re.compile(r'^/\* ([0-9A-F]{8}) [0-9A-F]{8}  (?:[0-9A-F]{2} ){4}\*/\t(\S+)\s+(.*)$')
    lbl = re.compile(r'lbl_([0-9A-Fa-f]{8})@l')
    for line in open(path, errors='replace'):
        m = pat.match(line)
        if not m:
            continue
        va = int(m.group(1), 16)
        if not (va0 <= va < va0 + size):
            continue
        if m.group(2) not in ('addi', 'subi', 'lwz', 'ori'):
            continue
        g = lbl.search(m.group(3))
        if not g:
            continue
        s = cstr(int(g.group(1), 16))
        if s is None or len(s) < 2:
            continue
        key = (va, s)
        if key in seen:
            continue
        seen.add(key)
        out.append((va - va0, s))
    return out


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__.strip() + '\n\nerror: expected 3 arguments, got '
                 f'{len(sys.argv) - 1}')
    path, va0 = sys.argv[1], int(sys.argv[2], 16)
    size = int(sys.argv[3], 0)
    a = arms(path, va0, size)
    print(f'# {path}  {va0:#x} size {size:#x} -- {len(a)} string references, IN ORDER')
    if len(a) < 2:
        print('SUSPECT: fewer than 2 string references found -- do not read this as '
              '"no handlers"; check the span and the .s generation date.')
    for off, s in a:
        print(f'  +{off:#07x}  {s}')


if __name__ == '__main__':
    main()
