#!/usr/bin/env python3
"""laneAV lead -- ADDITIVE-ONLY merge of one splits.txt delta into another.

Motivation: `land.sh`'s 3-way dict-union silently corrupts splits when two lanes
each add `.text` pins (unioned `.pdata` back-fills -> hard split failure), and a
plain `git apply --3way` conflicts.  This instead parses both files structurally
and copies ONLY the (unit, section, start, end) ranges that the donor has and
the target lacks -- never removing, never editing an endpoint.  Refuses on any
interval overlap.

  splits_additive_merge.py <target.txt> <donor_base.txt> <donor.txt>
"""
import re, sys, collections

HDR = re.compile(r'^(\S+\.(?:cpp|c)):\s*$')
RNG = re.compile(r'^\s*(\S+)\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)\s*$')

def parse(path):
    units = collections.OrderedDict(); cur = None; pre = []
    for ln in open(path).read().split('\n'):
        m = HDR.match(ln)
        if m:
            cur = m.group(1); units.setdefault(cur, [])
            continue
        m = RNG.match(ln)
        if m and cur is not None:
            units[cur].append((m.group(1), int(m.group(2),16), int(m.group(3),16)))
        elif cur is None:
            pre.append(ln)
    return pre, units

def main(tgt, dbase, donor):
    pre, T = parse(tgt)
    _,  B  = parse(dbase)
    _,  D  = parse(donor)
    newr = collections.defaultdict(list)
    for u, rs in D.items():
        have = set(B.get(u, []))
        for r in rs:
            if r not in have and r not in set(T.get(u, [])):
                newr[u].append(r)
    # overlap gate, per section, across the MERGED set
    merged = collections.defaultdict(list)
    for u, rs in T.items():
        for (s,a,b) in rs: merged[s].append((a,b,u))
    for u, rs in newr.items():
        for (s,a,b) in rs: merged[s].append((a,b,u))
    for sec, iv in merged.items():
        iv.sort()
        for i in range(1, len(iv)):
            if iv[i][0] < iv[i-1][1]:
                sys.exit('OVERLAP in %s: %s vs %s' % (sec, iv[i-1], iv[i]))
    # write: append new ranges into each unit's existing block
    out, cur = [], None
    added = 0
    lines = open(tgt).read().split('\n')
    i = 0
    while i < len(lines):
        ln = lines[i]; out.append(ln)
        m = HDR.match(ln)
        if m:
            cur = m.group(1)
            j = i + 1
            blk = []
            while j < len(lines) and RNG.match(lines[j]):
                blk.append(lines[j]); j += 1
            out.extend(blk)
            for (s,a,b) in sorted(newr.get(cur, []), key=lambda x:(x[0],x[1])):
                out.append('\t%-11s start:0x%08X end:0x%08X' % (s,a,b)); added += 1
            newr.pop(cur, None)
            i = j; continue
        i += 1
    # units absent from target entirely
    for u, rs in newr.items():
        out.append(''); out.append('%s:' % u)
        for (s,a,b) in sorted(rs, key=lambda x:(x[0],x[1])):
            out.append('\t%-11s start:0x%08X end:0x%08X' % (s,a,b)); added += 1
    open(tgt,'w').write('\n'.join(out))
    print('additive merge: %d ranges added, 0 removed, 0 endpoints edited' % added)

main(*sys.argv[1:4])
