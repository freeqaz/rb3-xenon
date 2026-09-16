#!/usr/bin/env python3
"""Census of MSVC packed function-local-static guards in the retail split asm.

MSVC packs up to 32 function-local statics into ONE guard int, claiming one bit
per static.  The per-static shape in retail PPC is:

    lwz     r11, lbl_<guard>@l(r30)     ; load the shared guard word
    clrlwi. r9, r11, 31   (or rlwinm.)  ; TEST this static's bit
    bne     .L_...                      ; already constructed -> skip
    ori     r11, r11, 0x1  (or oris)    ; CLAIM the bit
    stw     r11, lbl_<guard>@l(r30)     ; store it back
    ...     bl <ctor>

Two traps this detector is built around (both documented in CLAUDE.md / W16-FY):

  * A census keyed on the TEST (`andi.`/`andis.`) finds ZERO -- MSVC tests with
    `clrlwi.`/`rlwinm.` and only ever CLAIMS with `ori`/`oris`.  We key on the
    claim.
  * dtk's address/file-offset columns are SYNTHETIC for multi-block units, so
    every function is keyed on its `.fn fn_<addr>` symbol, never the column.

Output: one row per (function, guard label) with the number of distinct bits
claimed == the implied count of function-local statics.
"""
import re
import sys
import os
import json
import glob
import collections

FN_RE = re.compile(r'^\.fn\s+(fn_[0-9A-Fa-f]+|[^,\s]+)')
ENDFN_RE = re.compile(r'^\.endfn')
# instruction text lives after the closing "*/\t" of the byte-dump comment
INSN_RE = re.compile(r'\*/\s*(.*)$')

LWZ_RE = re.compile(r'^lwz\s+(r\d+),\s*(lbl_[0-9A-Fa-f]+)@l\((r\d+)\)')
STW_RE = re.compile(r'^stw\s+(r\d+),\s*(lbl_[0-9A-Fa-f]+)@l\((r\d+)\)')
ORI_RE = re.compile(r'^(ori|oris)\s+(r\d+),\s*(r\d+),\s*(0x[0-9A-Fa-f]+|\d+)')
TEST_RE = re.compile(r'^(clrlwi\.|rlwinm\.|andi\.|andis\.)\s')
BL_RE = re.compile(r'^bl\s+(\S+)')

WINDOW = 12  # instructions between the guard load and its store-back


def scan_file(path):
    """Yield (fnname, guardlabel, bits:set, nclaims, tests, ctors) per file."""
    fn = None
    insns = []
    out = []
    with open(path, 'r', errors='replace') as fh:
        for line in fh:
            line = line.rstrip('\n')
            m = FN_RE.match(line)
            if m:
                if fn is not None:
                    out.extend(analyze(fn, insns, path))
                fn = m.group(1)
                insns = []
                continue
            if ENDFN_RE.match(line):
                if fn is not None:
                    out.extend(analyze(fn, insns, path))
                fn = None
                insns = []
                continue
            if fn is None:
                continue
            m = INSN_RE.search(line)
            if m:
                insns.append(m.group(1).strip())
    if fn is not None:
        out.extend(analyze(fn, insns, path))
    return out


def analyze(fn, insns, path):
    """State machine: find lwz(L) ... ori/oris ... stw(L) claim sequences."""
    guards = collections.defaultdict(lambda: {
        'bits': set(), 'claims': 0, 'tests': 0, 'ctors': set(),
        'loads': 0, 'stores': 0})
    for i, ins in enumerate(insns):
        m = LWZ_RE.match(ins)
        if not m:
            continue
        reg, lbl, base = m.group(1), m.group(2), m.group(3)
        guards[lbl]['loads'] += 1
        bit = None
        saw_test = False
        for j in range(i + 1, min(i + 1 + WINDOW, len(insns))):
            nxt = insns[j]
            if TEST_RE.match(nxt):
                saw_test = True
            mo = ORI_RE.match(nxt)
            if mo and mo.group(2) == reg and mo.group(3) == reg:
                v = int(mo.group(4), 0)
                bit = (v << 16) if mo.group(1) == 'oris' else v
                continue
            ms = STW_RE.match(nxt)
            if ms and ms.group(1) == reg and ms.group(2) == lbl:
                if bit is not None:
                    guards[lbl]['bits'].add(bit)
                    guards[lbl]['claims'] += 1
                    if saw_test:
                        guards[lbl]['tests'] += 1
                    # the ctor call follows the claim, usually within ~6 insns
                    for k in range(j + 1, min(j + 8, len(insns))):
                        mb = BL_RE.match(insns[k])
                        if mb:
                            guards[lbl]['ctors'].add(mb.group(1))
                            break
                break
            # an unrelated store to the same reg aborts the window
            if ms and ms.group(1) == reg:
                break
    rows = []
    for lbl, g in guards.items():
        if g['claims'] >= 1:
            rows.append({
                'fn': fn, 'unit': os.path.basename(path)[:-2], 'guard': lbl,
                'nstatics': len(g['bits']), 'claims': g['claims'],
                'tests': g['tests'], 'loads': g['loads'],
                'bits_union': hex(0 if not g['bits'] else
                                  __import__('functools').reduce(
                                      lambda a, b: a | b, g['bits'])),
                'popcount': bin(0 if not g['bits'] else
                                __import__('functools').reduce(
                                    lambda a, b: a | b, g['bits'])).count('1'),
                'ctors': sorted(g['ctors']),
            })
    return rows


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else '.'
    asm = sorted(glob.glob(os.path.join(root, 'build/45410914/asm/*.s')))
    rows = []
    for p in asm:
        rows.extend(scan_file(p))
    rows.sort(key=lambda r: -r['nstatics'])
    print(json.dumps({'nfiles': len(asm), 'rows': rows}))


if __name__ == '__main__':
    main()
