#!/usr/bin/env python3
"""gap_content_evidence.py -- prove gap ownership by CONTENT, not adjacency.

WHY
---
Both gap funnels (`diffunit_gap_funnel.py`, and the interior-gap sweep) can only
argue ownership *geometrically*: "this hole is fenced by unit U, therefore it is
probably U's".  For interior holes that contiguity argument is strong (laneAL
measured 53.6%); for different-unit holes it does not exist at all.

This tool supplies the missing evidence channel.  For a candidate span it reads
dtk's own auto-carve asm, collects every `lbl_<VA>` data reference, decodes each
as a C string out of `orig/45410914/band.exe`, and classifies:

  SELF -- a decoded string is a source path naming the claimant unit itself
          (e.g. `.\\DuplicatedObject.cpp`).  MSVC bakes these in via MILO_FAIL /
          assert macros.  This is the hardest ownership evidence available and
          it is independent of address.
  SRC  -- some other source path is referenced (often a foreign interleave --
          `LEAPCORE`, `NUISPEECH`, XDK `xgraphics\\...\\block.cpp`).
  STR  -- unit-flavoured Symbol strings only (e.g. `update_tourdesc_provider`,
          `is_tour_available` for TourDescPanel; `deploy_to_save_lefty` for
          bandtrack/Track).  Soft but in practice highly specific.
  -    -- no string references; contiguity is the only argument.

★ STALE-ASM TRAP -- THE REASON THIS TOOL EXISTS IN THIS FORM
------------------------------------------------------------
`build/45410914/asm/auto_03_*_text.s` is NOT garbage-collected.  A warm worktree
carries thousands of blocks from *earlier split states* beside the live ones --
measured **4,426 stale vs 2,504 live**, one of them 13 days old and spanning
55 KB straight across current pins.

Reading them unfiltered produces FALSE CONTENT EVIDENCE.  It attributed XDK
`e:\\xenon\\xdk-main-feb10\\...\\xgraphics\\ucode\\compiler\\ir\\block.cpp`
strings (plus `IF_HEADER` / `LOOP_FOOTER` / `inst_prev.owningBlock != 0`) to a
`DuplicatedObject` gap -- which the *live* asm proves is DuplicatedObject's own
code, carrying the literal `.\\DuplicatedObject.cpp` path string.  That nearly
caused a provably-correct span to be rejected as a mis-carve.

So: every auto asm file is filtered by **mtime vs `build/45410914/config.json`**.
Never skip that filter.  Run a split first if config.json is older than the file
you care about.

USAGE
  gap_content_evidence.py --worktree WT --span 0x82A71FF8 0x82A732F8 [--unit X]
  gap_content_evidence.py --worktree WT --gaps gaps.json [--top N]
"""
import argparse
import glob
import json
import os
import re
import struct
import sys


# ---------------------------------------------------------------- band.exe --
class Image:
    def __init__(self, path):
        self.data = open(path, 'rb').read()
        d = self.data
        pe = struct.unpack_from('<I', d, 0x3C)[0]
        assert d[pe:pe + 4] == b'PE\0\0', 'not a PE image: %s' % path
        _, nsec, _, _, _, optsz, _ = struct.unpack_from('<HHIIIHH', d, pe + 4)
        base = struct.unpack_from('<I', d, pe + 4 + 20 + 28)[0]
        off = pe + 4 + 20 + optsz
        self.secs = []
        for i in range(nsec):
            nm, vsz, va, rawsz, rawptr = struct.unpack_from('<8sIIII', d, off + i * 40)
            self.secs.append((nm.rstrip(b'\0').decode('ascii', 'replace'),
                              va + base, max(vsz, rawsz), rawptr))

    def va2off(self, va):
        for nm, sva, sz, rawptr in self.secs:
            if sva <= va < sva + sz:
                return rawptr + (va - sva)
        return None

    def cstr(self, va, maxlen=200):
        o = self.va2off(va)
        if o is None:
            return None
        e = self.data.find(b'\0', o, o + maxlen)
        if e < 0:
            return None
        s = self.data[o:e]
        if len(s) < 4:
            return None
        try:
            t = s.decode('ascii')
        except UnicodeDecodeError:
            return None
        if not all(32 <= ord(c) < 127 or c in '\t\n' for c in t):
            return None
        return t


# -------------------------------------------------------------- live asm ----
def live_auto_blocks(wt):
    """-> sorted [(start_va, path)] for auto_03 asm newer than config.json.

    See the STALE-ASM TRAP note in the module docstring: this filter is
    load-bearing, not a tidy-up."""
    asmdir = os.path.join(wt, 'build/45410914/asm')
    cfg = os.path.join(wt, 'build/45410914/config.json')
    try:
        cutoff = os.path.getmtime(cfg) - 5
    except OSError:
        sys.exit('missing %s -- run a split first' % cfg)
    live, stale = [], 0
    for f in glob.glob(os.path.join(asmdir, 'auto_03_*_text.s')):
        m = re.match(r'auto_03_([0-9A-F]{8})_text\.s', os.path.basename(f))
        if not m:
            continue
        if os.path.getmtime(f) < cutoff:
            stale += 1
            continue
        live.append((int(m.group(1), 16), f))
    live.sort()
    return live, stale


def span_evidence(img, live, lo, hi):
    labels, nfn = set(), 0
    for a, f in live:
        if not (lo <= a < hi):
            continue
        txt = open(f).read()
        nfn += txt.count('\n.fn ')
        for m in re.finditer(r'\blbl_([0-9A-F]{8})\b', txt):
            labels.add(int(m.group(1), 16))
    src, other = [], []
    for lb in sorted(labels):
        s = img.cstr(lb)
        if not s:
            continue
        if '.cpp' in s or '.h' in s or s.startswith('..') or s.startswith('.\\'):
            src.append((lb, s))
        else:
            other.append((lb, s))
    return src, other, nfn


def classify(unit, src, other):
    stem = os.path.basename(unit or '').rsplit('.', 1)[0].lower()
    if stem and any(stem in s.lower() for _, s in src):
        return 'SELF'
    if src:
        return 'SRC'
    if other:
        return 'STR'
    return '-'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', default='.')
    ap.add_argument('--span', nargs=2, metavar=('LO', 'HI'))
    ap.add_argument('--unit', default='')
    ap.add_argument('--gaps', help='JSON list with unit/start/end or left_unit/va_lo/va_hi')
    ap.add_argument('--top', type=int, default=60)
    a = ap.parse_args()

    wt = a.worktree
    img = Image(os.path.join(wt, 'orig/45410914/band.exe'))
    live, stale = live_auto_blocks(wt)
    print('auto blocks: %d live, %d stale-skipped (mtime filter)' % (len(live), stale),
          file=sys.stderr)

    if a.span:
        lo, hi = int(a.span[0], 16), int(a.span[1], 16)
        src, other, nfn = span_evidence(img, live, lo, hi)
        print('%s %#x-%#x (%d B, %d fns)' % (classify(a.unit, src, other), lo, hi, hi - lo, nfn))
        for lb, s in src:
            print('   src %#010x  %r' % (lb, s))
        for lb, s in other[:30]:
            print('   str %#010x  %r' % (lb, s))
        return 0

    if not a.gaps:
        ap.error('need --span or --gaps')
    rows = json.load(open(a.gaps))
    norm = []
    for r in rows:
        unit = r.get('unit') or r.get('left_unit')
        lo = r.get('start', r.get('va_lo'))
        hi = r.get('end', r.get('va_hi'))
        norm.append((unit, lo, hi))
    norm.sort(key=lambda t: -(t[2] - t[1]))
    for unit, lo, hi in norm[:a.top]:
        src, other, nfn = span_evidence(img, live, lo, hi)
        print('[%4s] %7dB fns=%3d %-38s %#x' %
              (classify(unit, src, other), hi - lo, nfn, unit, lo))
        for _, s in src[:4]:
            print('          src: %r' % s)
        if not src:
            for _, s in other[:4]:
                print('          str: %r' % s)
    return 0


if __name__ == '__main__':
    sys.exit(main())
