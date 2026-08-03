#!/usr/bin/env python3
"""Lane ED-2: OUTLIER-BLOCK detector for the uniform-immediate-delta class.

A prior lane adjudicated ?Unload@CampaignSongInfoPanel by hand: its retail
address (0x8261feb8) sits in a .text block ~0x29000 away from the unit's own
0x825f5-0x825f6 cluster, i.e. it is an ICF fold-alias of some OTHER class's
identically-shaped function, not evidence about this class's layout. That lane
ADDED unk40/unk44 padding for it and then REVERTED.

Generalise that: for every row, locate the .text block containing its mapped
retail address and measure how far that block sits from the unit's dominant
(largest-by-bytes) cluster of blocks. A distant outlier block is a fold-alias
candidate; an address inside the dominant cluster is a genuine own-code row.
"""
import json, os, re, collections

WT = os.environ.get("ED2_WT", "/home/free/tmp/laneED2/wt")
GAP = 0x4000  # blocks within this distance are one cluster


def load_splits():
    """basename.cpp -> [(start,end), ...] of .text blocks"""
    out = collections.defaultdict(list)
    cur = None
    for line in open(f"{WT}/config/45410914/splits.txt"):
        s = line.rstrip("\n")
        if not s.strip():
            continue
        if not s.startswith((" ", "\t")) and s.rstrip().endswith(":"):
            cur = s.rstrip()[:-1].strip()
            continue
        if cur and ".text" in s:
            m = re.search(r"start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)", s)
            if m:
                out[cur].append((int(m.group(1), 16), int(m.group(2), 16)))
    return out


def clusters(blocks):
    blocks = sorted(blocks)
    cl, cur = [], []
    for b in blocks:
        if cur and b[0] - cur[-1][1] > GAP:
            cl.append(cur); cur = []
        cur.append(b)
    if cur:
        cl.append(cur)
    return cl


def main():
    splits = load_splits()
    m = {a: n for a, n in json.load(open(f"{WT}/scripts/target_symbol_map.json")).items()
         if isinstance(n, str) and a.startswith("0x")}
    rev = collections.defaultdict(list)
    for a, n in m.items():
        rev[n].append(int(a, 16))

    rows = json.load(open("/home/free/tmp/laneED2/ed2_classified.json"))
    out = []
    for r in rows:
        base = os.path.basename(r.get('src') or '')
        blocks = splits.get(base, [])
        addrs = rev.get(r['sym'], [])
        cls = clusters(blocks)
        # dominant cluster = most bytes
        dom = max(cls, key=lambda c: sum(e - s for s, e in c)) if cls else []
        dlo = min(s for s, e in dom) if dom else 0
        dhi = max(e for s, e in dom) if dom else 0
        verdict, dist, addr = 'NO_ADDR', None, None
        for a in addrs:
            if any(s <= a < e for s, e in blocks):
                addr = a
                if dlo <= a < dhi:
                    verdict, dist = 'IN_MAIN_CLUSTER', 0
                else:
                    dist = min(abs(a - dhi), abs(dlo - a))
                    verdict = 'OUTLIER_BLOCK'
                break
        r2 = dict(sym=r['sym'], size=r['size'], fuzzy=r['fuzzy'],
                  unit=r['unit'], src=base,
                  labels=sorted(r.get('labels', {})),
                  n_blocks=len(blocks), n_clusters=len(cls),
                  addr=hex(addr) if addr else None,
                  verdict=verdict, dist=hex(dist) if dist else dist)
        out.append(r2)
    json.dump(out, open("/home/free/tmp/laneED2/ed2_outlier.json", "w"), indent=1)
    c = collections.Counter(r['verdict'] for r in out)
    print(c)
    print()
    for r in sorted(out, key=lambda r: (r['verdict'], -r['size'])):
        print(f"{r['verdict']:<16} dist={str(r['dist']):<9} blk={r['n_blocks']:<2} "
              f"cl={r['n_clusters']:<2} {r['size']:>5}B {r['src']:<28} {r['sym'][:70]}")


if __name__ == '__main__':
    main()
