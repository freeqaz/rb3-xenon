#!/usr/bin/env python3
"""lane EB-1: recover the base sizes EA-1 had to throw away.

EA-1's guard (instrument defect #3) refuses any .text section holding more than
one function, because a SECTION size is only a BODY size when the section holds
exactly one body.  That is correct, but it discards 642 of 1,764 charged rows
(36%) -- a coverage hole, not a defect.

Within a multi-function section the sizes are still fully determined: sort the
function symbols by value, and

    size_i = value_{i+1} - value_i          (last: sectionsize - value_last)

with the trailing alignment padding TRIMMED off each body from the section's own
raw bytes.  Trimming matters: without it a derived size is inflated up to the
next 4/8/16-byte boundary, which would make C1 (base > claim) spuriously true and
manufacture candidates.

Cross-obj disagreement still forces None, exactly as before.
"""
import os, glob, struct, collections
import adj

WT = adj.WT

PAD = (0x00000000, 0x60000000)


def _trim(raw, lo, hi):
    """drop trailing zero/nop words from body [lo,hi) of a section's raw bytes"""
    while hi - lo >= 8:
        w = struct.unpack_from('>I', raw, hi - 4)[0]
        if w not in PAD:
            break
        hi -= 4
    return hi - lo


def base_index2():
    sizes = collections.defaultdict(set)
    stats = collections.Counter()
    for p in glob.glob(os.path.join(WT, 'build/45410914/src/**/*.obj'), recursive=True):
        try:
            d = open(p, 'rb').read()
            if len(d) < 20:
                continue
            nsec = struct.unpack_from('<H', d, 2)[0]
            symoff = struct.unpack_from('<I', d, 8)[0]
            nsym = struct.unpack_from('<I', d, 12)[0]
            if not symoff or not nsym:
                continue
            strt = symoff + nsym * 18
            secs = []
            for i in range(nsec):
                o = 20 + i * 40
                secs.append((d[o:o + 8].rstrip(b'\0').decode('latin1'),
                             struct.unpack_from('<I', d, o + 16)[0],   # SizeOfRawData
                             struct.unpack_from('<I', d, o + 20)[0]))  # PointerToRawData
            bysec = collections.defaultdict(list)
            i = 0
            while i < nsym:
                e = symoff + i * 18
                z = d[e:e + 8]
                if z[:4] == b'\0\0\0\0':
                    off = struct.unpack_from('<I', z, 4)[0]
                    end = d.index(b'\0', strt + off)
                    n = d[strt + off:end].decode('latin1')
                else:
                    n = z.rstrip(b'\0').decode('latin1')
                value, secnum, typ, sclass, naux = struct.unpack_from('<IhHBB', d, e + 8)
                if 0 < secnum <= len(secs) and (typ >> 4) == 0x2 and secs[secnum - 1][0].startswith('.text'):
                    bysec[secnum].append((value, n))
                i += 1 + naux
            for secnum, ents in bysec.items():
                nm, secsize, praw = secs[secnum - 1]
                ents.sort()
                # de-dup aliases at the same value (ICF-style multiple names on one body)
                if len(ents) == 1:
                    stats['single'] += 1
                else:
                    stats['multi'] += 1
                for j, (value, n) in enumerate(ents):
                    nxt = None
                    for k in range(j + 1, len(ents)):
                        if ents[k][0] != value:
                            nxt = ents[k][0]
                            break
                    hi = nxt if nxt is not None else secsize
                    if praw == 0 or hi > secsize or hi <= value:
                        sizes[n].add(None)
                        continue
                    sz = _trim(d, praw + value, praw + hi)
                    sizes[n].add(sz)
        except Exception:
            continue
    return ({k: (list(v)[0] if len(v) == 1 and None not in v else None)
             for k, v in sizes.items()}, stats)


BASE2, BSTATS = base_index2()
