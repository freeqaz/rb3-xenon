#!/usr/bin/env python3
"""lane EB-1: our compiled function sizes, from the COMDAT section table of every
obj we build.  Lifted verbatim from lane EA-1's sweep.py so the instrument is the
same one that produced the 15/15 confirm rate.

EA-1 instrument defect #3, inherited: a section size is OUR body size only when
the section holds EXACTLY ONE function (19 of 221 PostProc sections hold >1).
Anything else -> None (ambiguous), never a guess.
"""
import os, glob, struct
import adj

WT = adj.WT


def base_index():
    sizes = {}
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
            nfn = {}
            for i in range(nsec):
                o = 20 + i * 40
                secs.append((d[o:o + 8].rstrip(b'\0').decode('latin1'),
                             struct.unpack_from('<I', d, o + 16)[0]))
            ents = []
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
                    ents.append((n, secnum, value))
                    nfn[secnum] = nfn.get(secnum, 0) + 1
                i += 1 + naux
            for n, secnum, value in ents:
                if value != 0 or nfn.get(secnum, 0) != 1:
                    sizes.setdefault(n, set()).add(None)   # ambiguous COMDAT
                else:
                    sizes.setdefault(n, set()).add(secs[secnum - 1][1])
        except Exception:
            continue
    return {k: (list(v)[0] if len(v) == 1 and None not in v else None)
            for k, v in sizes.items()}


BASE = base_index()
