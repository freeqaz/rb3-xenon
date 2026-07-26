#!/usr/bin/env python3
"""splits_move.py -- find and atomically apply WRONG-UNIT `.text` span moves.

THE PROBLEM
-----------
`config/45410914/splits.txt` pins per-object `.text` address ranges; dtk carves
the retail XEX into one target `.obj` per pinned unit.  When a range is drawn
too wide, it swallows machine code that actually belongs to a *different* TU.
Two bad things follow:

  1.  The real owner's unit never gets that code, so its functions can never
      match -- the count is depressed.
  2.  Worse, objdiff pairs anonymous (`fn_8XXXXXXX`) target functions against
      our compiled COMDATs *positionally*.  Swallowed foreign code therefore
      manufactures **fake 100% matches**: a target function that is not the
      function we compiled, scoring 100 because the two happened to line up.

Fixing this is a **MOVE**: shrink donor unit A's range and hand the freed span
to claimant unit B.  It is only ever correct as **both halves at once** -- half
a move either leaves an overlap (dtk's `validate_splits` hard-fails: two units
may not own one address) or a hole.  `homing_apply4.py` cannot express this: it
only *adds* ranges and refuses on overlap.  Hence this tool.

Removing a fake match LOWERS the strict count while RAISING honesty.  `scan`
reports the fake-match exposure of every proposal up-front so a wave's delta can
be read correctly instead of being mistaken for a regression.

THE SIGNAL
----------
`scripts/target_symbol_map.json` is retail ground truth: VA -> MSVC mangled name
for ~21.7k functions.  Our compiled objs are COMDAT-per-function, so reading the
COFF symbol tables gives name -> {units that define it}.  Join them against the
pinned ranges:

  OK          VA's pinned owner A is (one of) the units whose obj defines the
              name -> the pin is right.
  WRONG-UNIT  the name is defined by our objs, but by NONE of them is it unit A
              -> the pin is wrong; the definers are the claimants.
  UNPORTED    no obj of ours defines the name -> no opinion, skipped.

`PARENT_OFFUNIT` from `funclet_cascade_rank.py` is the same defect seen from the
EH side (a `__unwind$` funclet landing in a different unit than its parent); the
`--offunit` input folds those boundaries in as extra evidence.

Contiguous WRONG-UNIT functions sharing one (donor, claimant) pair are clustered
into a single span, and a cluster is **refused** if any correctly-owned (OK) VA
of the donor falls inside it -- that would be a blind span across a gap.

SUBCOMMANDS
-----------
  scan   --worktree WT [--out proposals.json] [--offunit offunit.json]
  apply  --worktree WT --moves moves.json [--dry]
  audit  --worktree WT            (cross-unit overlap / inversion / dup-block)

`apply` re-runs `audit` on the result and refuses to write on any finding.
"""
import argparse
import bisect
import glob
import json
import os
import re
import struct
import sys
from collections import Counter, defaultdict

IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3

RANGE_RE = re.compile(
    r'^\s*\.(\w+)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)')
SYM_RE = re.compile(
    r'^(\S+)\s*=\s*\.text:0x([0-9A-Fa-f]+);.*type:function.*size:0x([0-9A-Fa-f]+)')

# XDK vendor + Quazal -- hard-skipped by project directive.
SKIP_LO, SKIP_HI = 0x82800000, 0x82D00000


def in_scope(va):
    return not (SKIP_LO <= va < SKIP_HI)


def unit_in_scope(u):
    return not os.path.basename(u).startswith('auto_03_')


# --------------------------------------------------------------------------
# parsing
# --------------------------------------------------------------------------
def parse_splits(path):
    """{unit: [(section, start, end), ...]} preserving file order."""
    units = defaultdict(list)
    cur = None
    for line in open(path):
        if line.rstrip('\n').endswith(':') and not line.startswith((' ', '\t')):
            cur = line.strip()[:-1]
            if cur == 'Sections':
                cur = None
            else:
                units.setdefault(cur, [])
            continue
        m = RANGE_RE.match(line)
        if m and cur:
            units[cur].append((m.group(1), int(m.group(2), 16),
                               int(m.group(3), 16)))
    return dict(units)


def text_ranges(units):
    return {u: [(s, e) for sec, s, e in rs if sec == 'text']
            for u, rs in units.items()}


def parse_symbols(path):
    """[(va, size)] sorted -- every function dtk carved."""
    out = []
    for line in open(path):
        m = SYM_RE.match(line)
        if m:
            out.append((int(m.group(2), 16), int(m.group(3), 16)))
    out.sort()
    return out


def coff_defined_symbols(data):
    """Names of every symbol this COFF obj DEFINES (SectionNumber > 0)."""
    out = set()
    if len(data) < 20:
        return out
    sym_offset = struct.unpack_from("<I", data, 8)[0]
    num_syms = struct.unpack_from("<I", data, 12)[0]
    if not sym_offset or not num_syms:
        return out
    strtab = sym_offset + num_syms * 18
    i = 0
    while i < num_syms:
        off = sym_offset + i * 18
        if off + 18 > len(data):
            break
        nb = data[off:off + 8]
        if nb[:4] == b"\x00\x00\x00\x00":
            a = strtab + struct.unpack_from("<I", nb, 4)[0]
            if 0 <= a < len(data):
                end = data.find(b"\x00", a)
                name = data[a:end if end >= 0 else len(data)].decode(
                    "ascii", errors="replace")
            else:
                name = ""
        else:
            name = nb.split(b"\x00")[0].decode("ascii", errors="replace")
        sec = struct.unpack_from("<h", data, off + 12)[0]
        if name and sec > 0 and data[off + 16] in (IMAGE_SYM_CLASS_EXTERNAL,
                                                   IMAGE_SYM_CLASS_STATIC):
            out.add(name)
        i += 1 + data[off + 17]
    return out


def build_definer_index(worktree):
    """{symbol_name: {unit_key}} over every compiled obj (COMDAT-per-function,
    so an inline/template symbol legitimately has many definers)."""
    idx = defaultdict(set)
    for root in glob.glob(os.path.join(worktree, 'build', '*', 'src')):
        for p in glob.glob(os.path.join(root, '**', '*.obj'), recursive=True):
            unit = os.path.relpath(p, root)[:-4]
            try:
                data = open(p, 'rb').read()
            except OSError:
                continue
            for n in coff_defined_symbols(data):
                idx[n].add(unit)
    return idx


def header_matches(unit_key, header):
    """obj unit key ('system/beatmatch/MasterAudio') vs splits header, which is
    sometimes a bare basename and sometimes a partial path.

    The extension must be tried, not assumed: retail built some C libraries as
    C++, so `system/obj/DataFlex.c` is a legitimate splits header.  Assuming
    `.cpp` made every `.c` unit unable to own its own range, and the whole
    DataFlex block surfaced as a bogus WRONG-UNIT proposal (caught in laneQ
    batch E1 -- it would have handed a correctly-pinned, already-matching TU to
    a source file that does not exist)."""
    for ext in ('.cpp', '.c', '.cc', '.cxx'):
        want = unit_key + ext
        if want == header or want.endswith('/' + header):
            return True
    return False


class Coverage:
    def __init__(self, tranges):
        self.iv = sorted((s, e, u) for u, rs in tranges.items() for s, e in rs)
        self.starts = [x[0] for x in self.iv]

    def owner(self, va):
        i = bisect.bisect_right(self.starts, va) - 1
        if i < 0:
            return None
        s, e, u = self.iv[i]
        return u if s <= va < e else None

    def range_of(self, va):
        i = bisect.bisect_right(self.starts, va) - 1
        if i < 0:
            return None
        s, e, u = self.iv[i]
        return (s, e, u) if s <= va < e else None


# --------------------------------------------------------------------------
# audit
# --------------------------------------------------------------------------
def audit(units, verbose=True):
    """Cross-unit .text overlaps, inverted ranges, duplicate unit blocks."""
    findings = []
    iv = []
    for u, rs in units.items():
        for sec, s, e in rs:
            if e <= s:
                findings.append(('INVERTED', u, sec, s, e, None))
            if sec == 'text':
                iv.append((s, e, u))
    iv.sort()
    for i in range(len(iv) - 1):
        s0, e0, u0 = iv[i]
        s1, e1, u1 = iv[i + 1]
        if s1 < e0:
            findings.append(('OVERLAP', u0, 'text', s0, e0, (u1, s1, e1)))
    if verbose:
        for f in findings:
            print('  !!', f[0], f[1], f[2], hex(f[3]), hex(f[4]),
                  '' if f[5] is None else f[5])
    return findings


def audit_dup_blocks(path):
    """A unit name emitted as TWO separate blocks makes dtk carve two objs for
    one source.  Appending to the existing block is fine; a second block is not.
    """
    seen, dups = set(), []
    for line in open(path):
        if line.rstrip('\n').endswith(':') and not line.startswith((' ', '\t')):
            u = line.strip()[:-1]
            if u == 'Sections':
                continue
            if u in seen:
                dups.append(u)
            seen.add(u)
    return dups


# --------------------------------------------------------------------------
# scan
# --------------------------------------------------------------------------
def scan(worktree, offunit_path=None, min_size=0):
    cfg = os.path.join(worktree, 'config/45410914')
    units = parse_splits(os.path.join(cfg, 'splits.txt'))
    tr = text_ranges(units)
    cov = Coverage(tr)
    syms = parse_symbols(os.path.join(cfg, 'symbols.txt'))
    sym_starts = [v for v, _ in syms]
    sizes = dict(syms)
    raw = json.load(open(os.path.join(worktree,
                                      'scripts/target_symbol_map.json')))
    vamap = {}
    for k, v in raw.items():
        if not k.startswith('0x'):
            continue          # '_comment' and friends
        try:
            vamap[int(k, 16)] = v
        except ValueError:
            continue
    definers = build_definer_index(worktree)

    stats = Counter()
    recs = []
    for va in sorted(vamap):
        if not in_scope(va):
            continue
        owner = cov.owner(va)
        if owner is None:
            stats['UNPINNED'] += 1
            continue
        if not unit_in_scope(owner):
            continue
        name = vamap[va]
        d = definers.get(name, set())
        if not d:
            stats['UNPORTED'] += 1
            cls = 'UNPORTED'
        elif any(header_matches(k, owner) for k in d):
            stats['OK'] += 1
            cls = 'OK'
        else:
            stats['WRONG-UNIT'] += 1
            cls = 'WRONG-UNIT'
        recs.append(dict(va=va, name=name, owner=owner, cls=cls,
                         definers=sorted(d),
                         size=sizes.get(va, 0)))

    # --- optional PARENT_OFFUNIT evidence: boundary -> repair count ---------
    offunit = {}
    if offunit_path and os.path.exists(offunit_path):
        for e in json.load(open(offunit_path)):
            key = (e.get('parent_unit'), e.get('funclet_unit'))
            offunit[key] = offunit.get(key, 0) + 1

    # --- cluster contiguous WRONG-UNIT runs --------------------------------
    by_va = {r['va']: r for r in recs}
    ordered = sorted(by_va)
    proposals = []
    i = 0
    while i < len(ordered):
        r = by_va[ordered[i]]
        if r['cls'] != 'WRONG-UNIT':
            i += 1
            continue
        donor = r['owner']
        drange0 = cov.range_of(r['va'])
        # Claimant: prefer a definer that is itself a pinned unit, and among
        # those the one whose nearest pinned range is CLOSEST to the span.
        # COMDAT ubiquity makes definition alone a weak signal -- an STL or
        # inline symbol is defined in every obj that instantiates it, so taking
        # the first definer picks arbitrarily (laneQ batch E2: HamMove's span
        # was routed to GemPlayer, whose nearest pin is 4.4 MB away, when
        # MessageTimer -- also a definer -- begins 4 bytes past the span end).
        cands = [k for k in r['definers']
                 if any(header_matches(k, h) for h in tr)]
        claim_key = min(cands, key=lambda k: _claim_distance(k, tr, r['va'])) \
            if cands else sorted(r['definers'])[0]
        claimant = next((h for h in tr if header_matches(claim_key, h)),
                        os.path.basename(claim_key) + '.cpp')
        j = i
        last = r
        while j + 1 < len(ordered):
            nxt = by_va[ordered[j + 1]]
            if nxt['cls'] == 'OK' and nxt['owner'] == donor:
                break          # a real donor function -- stop, do not swallow
            if nxt['cls'] != 'WRONG-UNIT':
                j += 1
                continue
            if nxt['owner'] != donor:
                break
            ncands = [k for k in nxt['definers']
                      if any(header_matches(k, h) for h in tr)]
            if claim_key not in (ncands or nxt['definers']):
                break
            j += 1
            last = nxt
        start = r['va']
        end = last['va'] + (last['size'] or 4)
        # snap end to the true end of the last carved function
        k = bisect.bisect_right(sym_starts, last['va']) - 1
        if k >= 0 and sym_starts[k] == last['va']:
            end = last['va'] + sizes[last['va']]
        drange = drange0
        # CLAMP to the donor range containing the span's start.  A cluster may
        # otherwise run past the end of that range, across a gap, and into a
        # *different* range -- possibly one the claimant already owns.  `apply`
        # refuses such a span ("not fully inside one donor .text range"), which
        # is the correct safety, but scan should not emit it at all (laneQ batch
        # E2: Player -> FadePanel proposed 0x826A8848-0x826A8A14, spanning a
        # Player range, an existing FadePanel range, an 8-byte gap, and the head
        # of a second Player range).
        if drange and end > drange[1]:
            end = drange[1]
        if end <= start:
            i = j + 1
            continue
        n_wrong = sum(1 for v in ordered[i:j + 1]
                      if by_va[v]['cls'] == 'WRONG-UNIT')
        # positional-pairing exposure: every carved function in the span is a
        # fake-match candidate, because none of them belong to the donor
        n_carved = bisect.bisect_left(sym_starts, end) - \
            bisect.bisect_left(sym_starts, start)
        if end - start >= min_size:
            proposals.append(dict(
                donor=donor, claimant=claimant, claim_key=claim_key,
                start=hex(start), end=hex(end), size=end - start,
                n_wrong=n_wrong, n_carved_in_span=n_carved,
                donor_range=[hex(drange[0]), hex(drange[1])] if drange else None,
                position=_position(drange, start, end),
                offunit_evidence=offunit.get((donor, claimant), 0),
                names=[by_va[v]['name'] for v in ordered[i:j + 1]
                       if by_va[v]['cls'] == 'WRONG-UNIT'][:8]))
        i = j + 1
    return stats, proposals, recs


def _claim_distance(unit_key, tranges, va):
    """Distance from `va` to the nearest pinned `.text` range of `unit_key`.

    Spatial adjacency is the discriminator when several units define a symbol:
    retail lays a TU's COMDATs out contiguously, so the true claimant's existing
    pin is almost always immediately adjacent to the mis-owned span."""
    best = float('inf')
    for h, rs in tranges.items():
        if not header_matches(unit_key, h):
            continue
        for s, e in rs:
            best = min(best, 0 if s <= va < e else min(abs(s - va), abs(e - va)))
    return best


def _position(drange, start, end):
    if not drange:
        return 'NO-RANGE'
    s, e, _ = drange
    at_start, at_end = start <= s, end >= e
    if at_start and at_end:
        return 'WHOLE'
    if at_start:
        return 'HEAD'
    if at_end:
        return 'TAIL'
    return 'MIDDLE'


# --------------------------------------------------------------------------
# apply
# --------------------------------------------------------------------------
def apply_moves(worktree, moves, dry=False):
    """Atomically: shrink each donor range, insert the span into the claimant.

    A move is refused (not silently skipped) unless the span is FULLY contained
    in exactly one donor `.text` range.  Splicing is textual so every unrelated
    line of splits.txt stays byte-identical.
    """
    path = os.path.join(worktree, 'config/45410914/splits.txt')
    lines = open(path).read().split('\n')

    def blocks():
        """{unit: (hdr_idx, [line indices of its .text lines])}"""
        out, cur = {}, None
        for i, ln in enumerate(lines):
            if ln.endswith(':') and ln and not ln.startswith((' ', '\t')):
                cur = ln[:-1]
                if cur == 'Sections':
                    cur = None
                    continue
                out.setdefault(cur, (i, []))
                continue
            m = RANGE_RE.match(ln)
            if m and cur and m.group(1) == 'text':
                out[cur][1].append(i)
        return out

    applied, refused = [], []
    for mv in moves:
        donor, claimant = mv['donor'], mv['claimant']
        s, e = int(str(mv['start']), 16), int(str(mv['end']), 16)
        b = blocks()
        if donor not in b:
            refused.append((mv, 'donor block not found'))
            continue
        host = None
        for idx in b[donor][1]:
            m = RANGE_RE.match(lines[idx])
            ds, de = int(m.group(2), 16), int(m.group(3), 16)
            if ds <= s and e <= de:
                host = (idx, ds, de)
                break
        if host is None:
            refused.append((mv, 'span not fully inside one donor .text range'))
            continue
        idx, ds, de = host
        indent = re.match(r'^(\s*)', lines[idx]).group(1)

        def fmt(a, z):
            return f'{indent}.text       start:0x{a:08X} end:0x{z:08X}'

        remnants = [(a, z) for a, z in ((ds, s), (e, de)) if z > a]
        if not remnants:
            # donor loses the whole range; keep the block (other sections /
            # future pins) but drop the line
            lines[idx] = None
        else:
            lines[idx] = fmt(*remnants[0])
            if len(remnants) == 2:
                lines.insert(idx + 1, fmt(*remnants[1]))
        lines = [l for l in lines if l is not None]

        b = blocks()
        newline = fmt(s, e)
        if claimant in b:
            hdr, tidx = b[claimant]
            pos = hdr + 1
            for ti in tidx:
                m = RANGE_RE.match(lines[ti])
                if int(m.group(2), 16) < s:
                    pos = ti + 1
                else:
                    pos = ti
                    break
            else:
                pos = (tidx[-1] + 1) if tidx else hdr + 1
            lines.insert(pos, newline)
        else:
            # brand-new unit: append ONE fresh block (never a duplicate)
            while lines and lines[-1] == '':
                lines.pop()
            lines += ['', f'{claimant}:', newline]
        applied.append(mv)

    text = '\n'.join(lines)
    if not text.endswith('\n'):
        text += '\n'
    tmp = path + '.laneQ.tmp'
    open(tmp, 'w').write(text)
    units = parse_splits(tmp)
    findings = audit(units, verbose=False)
    dups = audit_dup_blocks(tmp)
    if findings or dups:
        print('REFUSING TO WRITE -- post-move audit failed:')
        audit(units)
        for d in dups:
            print('  !! DUPLICATE BLOCK', d)
        os.unlink(tmp)
        return applied, refused + [(None, 'AUDIT FAILED')], False
    if dry:
        os.unlink(tmp)
        print('[dry] audit clean; would apply %d, refuse %d'
              % (len(applied), len(refused)))
        return applied, refused, True
    os.replace(tmp, path)
    print('applied %d move(s), refused %d; audit clean'
          % (len(applied), len(refused)))
    for mv, why in refused:
        print('  refused:', mv, '--', why)
    return applied, refused, True


# --------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='cmd', required=True)

    p = sub.add_parser('scan')
    p.add_argument('--worktree', required=True)
    p.add_argument('--out')
    p.add_argument('--offunit')
    p.add_argument('--min-size', type=int, default=0)
    p.add_argument('--top', type=int, default=40)

    p = sub.add_parser('apply')
    p.add_argument('--worktree', required=True)
    p.add_argument('--moves', required=True)
    p.add_argument('--dry', action='store_true')

    p = sub.add_parser('audit')
    p.add_argument('--worktree', required=True)

    a = ap.parse_args()

    if a.cmd == 'audit':
        path = os.path.join(a.worktree, 'config/45410914/splits.txt')
        units = parse_splits(path)
        f = audit(units)
        dups = audit_dup_blocks(path)
        for d in dups:
            print('  !! DUPLICATE BLOCK', d)
        print('audit: %d range finding(s), %d duplicate block(s)'
              % (len(f), len(dups)))
        return 1 if (f or dups) else 0

    if a.cmd == 'scan':
        stats, props, recs = scan(a.worktree, a.offunit, a.min_size)
        print('classification:', dict(stats))
        props.sort(key=lambda p: -p['size'])
        print('\n%d move proposal(s); top %d by size:' % (len(props), a.top))
        print('%-34s %-34s %-11s %-11s %6s %5s %5s %s'
              % ('DONOR', 'CLAIMANT', 'START', 'END', 'SIZE', 'WRNG', 'CARV',
                 'POS'))
        for pr in props[:a.top]:
            print('%-34s %-34s %-11s %-11s %6d %5d %5d %s'
                  % (pr['donor'][:34], pr['claimant'][:34], pr['start'],
                     pr['end'], pr['size'], pr['n_wrong'],
                     pr['n_carved_in_span'], pr['position']))
        if a.out:
            json.dump(props, open(a.out, 'w'), indent=1)
            json.dump([r for r in recs if r['cls'] == 'WRONG-UNIT'],
                      open(a.out.replace('.json', '_records.json'), 'w'),
                      indent=1)
            print('->', a.out)
        return 0

    if a.cmd == 'apply':
        moves = json.load(open(a.moves))
        applied, refused, ok = apply_moves(a.worktree, moves, a.dry)
        return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main() or 0)
