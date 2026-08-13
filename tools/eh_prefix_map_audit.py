#!/usr/bin/env python3
"""Detect target_symbol_map.json rows that name an 8-byte EH PREFIX, not a function.

WHY THIS EXISTS (lane MAP-FIX, 2026-08-13)
------------------------------------------
An EH-bearing function's COMDAT begins with an 8-byte prefix -- a pointer to
__CxxFrameHandler (.text) followed by a pointer to __ehfuncinfo$<fn> (.rdata) --
and the function proper starts at +8:

    +0x00   8 B   EH prefix          <-- dtk invents a bogus `fn_<addr>` size-8
                                         "function" here; it is NOT a function
    +0x08   N B   the function       <-- the real .pdata BeginAddress
    +0x08+N       __unwind$ funclet  <-- its own .pdata entry, its own row

When a map row binds a mangled name to the PREFIX address, the renamer renames
that 8-byte pseudo-function, and objdiff then compares a real 100+ byte function
against an 8-byte stub. The row is undiffable and any conclusion drawn from it
is void. 32 such rows were found and repaired; this tool keeps the class at zero.

THE SIGNATURE (all three required, so it cannot fire on an ordinary unpinned row):
  1. the row's address is NOT a .pdata BeginAddress;
  2. address + 8 IS a .pdata BeginAddress;
  3. the 8 bytes at the address are a .text pointer followed by an .rdata/.data
     pointer.

REPAIR POLICY -- do not simply move the pin +8. Measured: 19 of the 32 rows had a
DIFFERENT name already at +8, and 17 of those incumbents are byte-identical to
retail (one at 0/210 words), so re-homing would have EVICTED verified-correct
credit. Re-home only when +8 is FREE *and* our COMDAT corroborates the name:

  * our symbol offset == 8 (our function is EH-bearing exactly as retail's is);
    symoff == 0 means our function has no prefix while retail's does -- the
    shapes disagree and the name does not belong there;
  * |our section size - (8 + contiguous .pdata chain from addr+8)| <= 16.

Otherwise DELETE the row: the name returns to the triage backlog, which is the
honest state, rather than being pinned to a boundary nothing corroborates.
An ungated re-home was measured harmful -- 0x82703998 -> __uninitialized_fill_n
renamed a function that had been sitting at fuzzy 100 as an anonymous
byte-signature pairing, and the explicit name scored 1.0.

Usage:
    python3 tools/eh_prefix_map_audit.py                # audit, exit 1 if any
    python3 tools/eh_prefix_map_audit.py --project-dir /path/to/worktree
"""
import argparse
import json
import os
import struct
import sys


class Retail:
    """Address-addressable view of the retail PE (same reader as
    tools/comdat_retail_verify.py; .pdata FunctionLen is 18 bits at bit offset 8
    of the second BIG-ENDIAN word -- getting either wrong inverts the verdict)."""

    def __init__(self, path):
        d = open(path, 'rb').read()
        pe = struct.unpack_from('<I', d, 0x3C)[0]
        nsec = struct.unpack_from('<H', d, pe + 6)[0]
        optsz = struct.unpack_from('<H', d, pe + 20)[0]
        self.base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
        self.d = d
        self.secs = []
        off = pe + 24 + optsz
        for _ in range(nsec):
            raw = d[off:off + 40]
            name = raw[:8].rstrip(b'\x00').decode()
            _vsz, va, _szraw, ptr = struct.unpack_from('<IIII', raw, 8)
            vsz = _vsz
            self.secs.append((name, va, vsz, ptr))
            off += 40

    def section_of(self, addr):
        for nm, va, vsz, _ptr in self.secs:
            if self.base + va <= addr < self.base + va + vsz:
                return nm
        return None

    def read(self, addr, n):
        for _nm, va, vsz, ptr in self.secs:
            if self.base + va <= addr < self.base + va + vsz:
                fo = ptr + (addr - (self.base + va))
                return self.d[fo:fo + n]
        return None

    def pdata_lengths(self):
        pd = [s for s in self.secs if s[0] == '.pdata'][0]
        raw = self.d[pd[3]:pd[3] + pd[2]]
        out = {}
        for i in range(0, len(raw), 8):
            a, f = struct.unpack_from('>II', raw, i)
            if a:
                out[a] = ((f >> 8) & 0x3FFFF) * 4
        return out


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--project-dir', default='.')
    args = ap.parse_args()
    root = args.project_dir

    retail = Retail(os.path.join(root, 'orig/45410914/band.exe'))
    lens = retail.pdata_lengths()
    tmap = json.load(open(os.path.join(root, 'scripts/target_symbol_map.json')))
    rows = {k: v for k, v in tmap.items() if k.startswith('0x')}

    def is_prefix(a):
        b = retail.read(a, 8)
        if not b or len(b) < 8:
            return False
        w0, w1 = struct.unpack('>II', b)
        return (retail.section_of(w0) == '.text'
                and retail.section_of(w1) in ('.rdata', '.data'))

    hits = []
    at_pdata = 0
    for k, v in rows.items():
        a = int(k, 16)
        if a in lens:
            at_pdata += 1
            continue
        if (a + 8) in lens and is_prefix(a):
            hits.append((a, v, lens[a + 8]))

    print('map rows                     : %d' % len(rows))
    print('rows at a .pdata BeginAddress: %d' % at_pdata)
    print('EH-PREFIX DEFECT rows        : %d' % len(hits))
    for a, v, n in sorted(hits):
        print('  0x%08x  real function at 0x%08x len=%d  %s' % (a, a + 8, n, v))
    if hits:
        print('\nFAIL: %d row(s) name an 8-byte EH prefix rather than a function.'
              '\nSee this file\'s docstring for the gated repair policy '
              '(re-home only with corroboration; otherwise delete).' % len(hits))
        return 1
    print('\nOK: no map row names an EH prefix.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
