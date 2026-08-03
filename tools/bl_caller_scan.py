#!/usr/bin/env python3
"""Scan retail band.exe .text for every `bl` targeting a given VA.

Python, not grep: the shell's grep is binary-blind (ugrep -I) and would return a
false negative shaped like a decisive negative.  TEXT_BIAS carries DO-2's
positive control, which caught a wrong bias before any verdict formed.
"""
import os, struct, sys

TEXT_BIAS = 0x8200B200
exe = open('/home/free/tmp/laneDP1/wt/orig/45410914/band.exe', 'rb').read()

# positive control (DO-2): must reproduce a known prologue, else refuse
o = 0x82548F70 - TEXT_BIAS
assert struct.unpack('>I', exe[o:o+4])[0] == 0x7D8802A6, 'TEXT_BIAS control FAILED -- refusing'

TEXT_LO, TEXT_HI = 0x82010000, 0x82D00000
targets = [int(a, 16) for a in sys.argv[1:]]
hits = {t: [] for t in targets}
n = 0
for off in range(0, len(exe) - 4, 4):
    va = off + TEXT_BIAS
    if not (TEXT_LO <= va < TEXT_HI):
        continue
    w = struct.unpack('>I', exe[off:off+4])[0]
    if (w >> 26) != 18:            # not b/bl
        continue
    if not (w & 1):                # not a bl (LK bit)
        continue
    if w & 2:                      # absolute
        continue
    d = w & 0x03FFFFFC
    if d & 0x02000000:
        d -= 0x04000000
    tgt = va + d
    n += 1
    if tgt in hits:
        hits[tgt].append(va)

print(f'scanned {n} bl instructions')
for t in targets:
    print(f'\n== callers of {t:#x}: {len(hits[t])} ==')
    for c in hits[t][:40]:
        print(f'   {c:#x}')
