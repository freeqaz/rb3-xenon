#!/usr/bin/env python3
"""Round-4 (full-tree) homing generator: splits pins + map fragment.

Round 3 (homing_gen3.py) was hardcoded to five brand-new TUs. Round 4 sweeps
*every* built obj, so it must additionally handle:

  * gap-fill into units that ALREADY have a splits block (append ranges to the
    existing header rather than emitting a duplicate block -- a duplicate
    header would make dtk carve two objs for one source file),
  * spatial cluster ownership: a byte-identical COMDAT surfaces UNIQUE in every
    TU that scatter-includes it, so per-VA "first TU wins" scatters one retail
    neighbourhood across several units.  We vote per spatial cluster instead.

Discipline preserved from rounds 1-3:
  * plain-UNIQUE only (UNIQUE-ICF / MULTI / ALL-MAPPED / NOMATCH dropped),
  * drop VAs already covered by ANY existing splits .text range,
  * drop VAs / names already present in target_symbol_map (case-insensitive on
    the addr key -- the map holds 264 legacy uppercase "0X..." keys),
  * exactly one owner per VA, one VA per name, deterministic.

Usage:
  homing_gen4.py --results merged.json --worktree /path/to/wt \
                 --out-prefix /home/free/tmp/homing4 [--only-units a,b,c]
"""
import argparse
import json
import os
import re
from collections import Counter, OrderedDict, defaultdict

# spatial clustering gap: retail packs a TU's functions contiguously; 0x600 of
# already-carved/other-TU bytes between two hits still counts as one cluster.
CLUSTER_GAP = 0x600


# ---------------------------------------------------------------- splits I/O
def parse_splits(path):
    """-> OrderedDict unit_header -> [(sec, start, end)] (source order)."""
    units = OrderedDict()
    cur = None
    rng = re.compile(r'\.(\w+)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)')
    for line in open(path):
        if line.strip().endswith(':') and not line.startswith((' ', '\t')):
            cur = line.strip()[:-1]
            if cur == 'Sections':
                cur = None
                continue
            units.setdefault(cur, [])
            continue
        m = rng.search(line)
        if m and cur:
            units[cur].append((m.group(1), int(m.group(2), 16), int(m.group(3), 16)))
    return units


def build_coverage(units):
    ivs = []
    for u, rs in units.items():
        for sec, s, e in rs:
            if sec == 'text':
                ivs.append((s, e, u))
    ivs.sort()
    return ivs


def covered(ivs, va, size):
    for s, e, u in ivs:
        if va < e and va + size > s:
            return u
        if s >= va + size:
            break
    return None


# ------------------------------------------------------------------ map I/O
def load_map(path):
    m = json.load(open(path))
    vas, names = set(), set()
    for k, v in m.items():
        if k.startswith('_'):
            continue
        try:
            vas.add(int(k, 16))          # int(,16) already handles "0X.." case
        except ValueError:
            pass
        if isinstance(v, str):
            names.add(v)
    return vas, names


# --------------------------------------------------- obj key -> splits header
def unit_header_for(objkey, objects_keys, splits_units):
    """objkey e.g. 'system/bandobj/BandCrowdMeter' (obj path rel to src/).

    Prefer an EXISTING splits block (matched by basename or full path) so we
    gap-fill rather than duplicate.  Otherwise use the objects.json key.
    """
    base = objkey.split('/')[-1] + '.cpp'
    full = objkey + '.cpp'
    if full in splits_units:
        return full, False
    if base in splits_units:
        return base, False
    if full in objects_keys:
        return full, True
    if base in objects_keys:
        return base, True
    return None, None


# ---------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--results', required=True, help='merged homing_scan results json')
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--out-prefix', required=True)
    ap.add_argument('--only-units', default=None,
                    help='comma-separated scan keys to emit (default: all)')
    ap.add_argument('--min-cluster', type=int, default=1,
                    help='drop spatial clusters smaller than this many fns')
    ap.add_argument('--reveal-frag', default=None,
                    help='also emit a map-ONLY fragment for VAs that are already '
                         'inside an existing splits .text range (round 4 did this '
                         'sub-wave by hand). No pin is needed for these; the name '
                         'alone flips them.')
    args = ap.parse_args()

    wt = args.worktree
    splits_path = f'{wt}/config/45410914/splits.txt'
    objects_path = f'{wt}/config/45410914/objects.json'
    tmap_path = f'{wt}/scripts/target_symbol_map.json'

    splits_units = parse_splits(splits_path)
    ivs = build_coverage(splits_units)
    map_vas, map_names = load_map(tmap_path)
    objects_keys = set()
    for lib, d in json.load(open(objects_path)).items():
        objects_keys.update(d['objects'])

    results = json.load(open(args.results))
    only = set(args.only_units.split(',')) if args.only_units else None

    # ---- collect plain-UNIQUE, per VA -> candidate TUs
    cand = defaultdict(list)          # va -> [(tu, name, size)]
    for tu, res in results.items():
        if not isinstance(res, list):
            continue
        for r in res:
            if r['cls'] != 'UNIQUE':
                continue
            cand[int(r['va'], 16)].append((tu, r['name'], r['size']))

    stats = Counter()
    live = {}
    reveal = {}
    for va, lst in sorted(cand.items()):
        size = lst[0][2]
        if va in map_vas:
            stats['drop_va_in_map'] += 1
            continue
        cu = covered(ivs, va, size)
        if cu:
            stats['drop_covered'] += 1
            names = {n for _, n, _ in lst}
            if len(names) == 1:
                n = names.pop()
                if n not in map_names:
                    reveal[f'0x{va:08x}'] = n
            continue
        names = {n for _, n, _ in lst}
        if len(names) != 1:
            stats['drop_name_ambiguous'] += 1
            continue
        name = names.pop()
        if name in map_names:
            stats['drop_name_in_map'] += 1
            continue
        live[va] = (size, name, sorted({t for t, _, _ in lst}))

    # ---- spatial clusters -> vote an owner TU
    order = sorted(live)
    clusters, cur, prev_end = [], [], None
    for va in order:
        size = live[va][0]
        if prev_end is not None and va - prev_end > CLUSTER_GAP:
            clusters.append(cur)
            cur = []
        cur.append(va)
        prev_end = va + size
    if cur:
        clusters.append(cur)

    assign = {}          # va -> tu
    for cl in clusters:
        pending = list(cl)
        while pending:
            votes = Counter()
            for va in pending:
                for t in live[va][2]:
                    votes[t] += 1
            best_n = max(votes.values())
            tied = sorted(t for t, n in votes.items() if n == best_n)
            if len(tied) > 1:
                # tie-break: TU whose basename appears in a mangled name it owns
                def classhit(t):
                    b = t.split('/')[-1]
                    return sum(1 for va in pending
                               if t in live[va][2] and b in live[va][1])
                tied.sort(key=lambda t: (-classhit(t), t))
            owner = tied[0]
            took = [va for va in pending if owner in live[va][2]]
            if len(took) < args.min_cluster:
                for va in took:
                    stats['drop_small_cluster'] += 1
            else:
                for va in took:
                    assign[va] = owner
            pending = [va for va in pending if va not in set(took)]

    # ---- resolve unit headers, drop unresolvable
    by_unit = defaultdict(list)
    unresolved = Counter()
    new_units = set()
    for va, tu in sorted(assign.items()):
        if only and tu not in only:
            continue
        hdr, is_new = unit_header_for(tu, objects_keys, splits_units)
        if hdr is None:
            unresolved[tu] += 1
            stats['drop_no_unit'] += 1
            continue
        if is_new:
            new_units.add(hdr)
        size, name, _ = live[va]
        by_unit[hdr].append((va, size, name))

    # ---- emit merged tight ranges + map fragment
    mapfrag = {}
    blocks = []
    summary = []
    for hdr in sorted(by_unit):
        fns = sorted(by_unit[hdr])
        ranges = []
        for va, size, name in fns:
            end = va + size
            if ranges and va <= ranges[-1][1]:
                ranges[-1] = (ranges[-1][0], max(ranges[-1][1], end))
            else:
                ranges.append((va, end))
            mapfrag[f'0x{va:08x}'] = name
        body = '\n'.join(f'\t.text\tstart:0x{s:08X} end:0x{e:08X}' for s, e in ranges)
        blocks.append(dict(unit=hdr, new=hdr in new_units, body=body,
                           ranges=ranges, nfns=len(fns)))
        summary.append((hdr, len(fns), len(ranges), hdr in new_units))

    if args.reveal_frag:
        json.dump(reveal, open(args.reveal_frag, 'w'), indent=1)
        print(f'reveal (map-only, already covered by a splits range): {len(reveal)}'
              f' -> {args.reveal_frag}')

    json.dump(blocks, open(args.out_prefix + '_blocks.json', 'w'), indent=1)
    json.dump(mapfrag, open(args.out_prefix + '_map_fragment.json', 'w'), indent=1)

    print(f'plain-UNIQUE VAs seen: {len(cand)}')
    for k, v in sorted(stats.items()):
        print(f'  {k}: {v}')
    print(f'assigned: {len(assign)}  emitted: {len(mapfrag)} '
          f'across {len(by_unit)} units ({len(new_units)} new)')
    if unresolved:
        print('unresolved scan keys:', dict(unresolved))
    for hdr, n, nr, isnew in sorted(summary, key=lambda x: -x[1]):
        print(f'  {"NEW " if isnew else "gap "}{hdr}: {n} fns, {nr} ranges')


if __name__ == '__main__':
    main()
