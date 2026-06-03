#!/usr/bin/env python3
"""Relocate under-pinned engine TU splits onto their real bodies, from
content-match identifications (tools/dc3_content_match.py).

The existing engine stub splits often point at COMDAT-pool junk, not the TU's
body (see project_engine_split_relocation memory). dc3_content_match.json gives
byte-exact rb3_addr -> DC3 name (each a guaranteed objdiff match once pinned +
named). For each wired engine unit whose current pin is a STUB, this:
  1. takes the unit's content-matched addrs,
  2. finds the dominant contiguous cluster (the real body),
  3. sets the unit's .text range to that cluster span (clipped so it never
     overlaps another unit's pin -- the only hard constraint),
  4. writes the matched rb3_addr -> DC3 name into target_symbol_map.json so the
     renamer pairs them.

Only STUB units are touched (current .text range < --stub-max bytes), so
well-pinned units are never disturbed. Expanding into unpinned space cannot
regress (unpinned = unmeasured).

  tools/relocate_engine_splits.py                 # dry-run report
  tools/relocate_engine_splits.py --apply         # rewrite splits.txt + tsm
  tools/relocate_engine_splits.py --apply --splits <worktree>/config/.../splits.txt --tsm ...
"""
import argparse
import bisect
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEF_MATCH = os.path.join(ROOT, "dc3_content_match.json")
DEF_SPLITS = os.path.join(ROOT, "config", "45410914", "splits.txt")
DEF_SYMBOLS = os.path.join(ROOT, "config", "45410914", "symbols.txt")
DEF_OBJECTS = os.path.join(ROOT, "config", "45410914", "objects.json")
DEF_TSM = os.path.join(ROOT, "scripts", "target_symbol_map.json")

UNIT_RE = re.compile(r"^(\S+\.(?:cpp|c|cc)):\s*$")
TEXT_RE = re.compile(r"^(\s*)\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")


def load_sizes(path):
    sizes = {}
    rx = re.compile(r"fn_([0-9A-Fa-f]+) = \.text:0x[0-9A-Fa-f]+; // type:function size:0x([0-9A-Fa-f]+)")
    for line in open(path):
        m = rx.match(line)
        if m:
            sizes[int(m.group(1), 16)] = int(m.group(2), 16)
    return sizes


def parse_splits(path):
    lines = open(path).read().splitlines(keepends=True)
    units = {}
    cur = None
    for i, line in enumerate(lines):
        m = UNIT_RE.match(line)
        if m:
            cur = m.group(1)
            units.setdefault(cur, {"name": cur, "text_line": None})
            continue
        if cur:
            t = TEXT_RE.match(line)
            if t and units[cur]["text_line"] is None:
                units[cur].update(text_line=i, indent=t.group(1),
                                  lo=int(t.group(2), 16), hi=int(t.group(3), 16))
    return lines, units


def wired_engine(objects_path):
    o = json.load(open(objects_path))
    eng = {}
    for grp in ("engine", "hamobj", "main"):
        for fn in o.get(grp, {}).get("objects", {}):
            eng[fn.split("/")[-1].lower()] = fn
    return eng


def dominant_cluster(addrs, sizes, gap):
    addrs = sorted(addrs)
    runs, s = [], 0
    for k in range(1, len(addrs)):
        if addrs[k] - addrs[k - 1] > gap:
            runs.append((s, k - 1)); s = k
    runs.append((s, len(addrs) - 1))
    bi, bj = max(runs, key=lambda r: r[1] - r[0] + 1)
    cl = addrs[bi:bj + 1]
    lo = cl[0]
    hi = cl[-1] + sizes.get(cl[-1], 4)
    return lo, hi, cl, len(cl) / len(addrs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--match", default=DEF_MATCH)
    ap.add_argument("--splits", default=DEF_SPLITS)
    ap.add_argument("--symbols", default=DEF_SYMBOLS)
    ap.add_argument("--objects", default=DEF_OBJECTS)
    ap.add_argument("--tsm", default=DEF_TSM)
    ap.add_argument("--stub-max", type=lambda x: int(x, 0), default=0x400,
                    help="only relocate units whose current .text range is "
                         "smaller than this (a stub); default 0x400")
    ap.add_argument("--min-cluster", type=int, default=4,
                    help="min content-matches in the dominant cluster")
    ap.add_argument("--min-frac", type=float, default=0.5,
                    help="dominant cluster must hold >= this fraction of the "
                         "unit's content-matches")
    ap.add_argument("--cluster-gap", type=lambda x: int(x, 0), default=0x8000)
    ap.add_argument("--only", default="")
    ap.add_argument("--extend", action="store_true",
                    help="EXTEND mode: instead of relocating stub units, grow "
                         "NON-stub units (current span >= --stub-max) to UNION "
                         "their current pin with the dominant cluster, but ONLY "
                         "when the cluster overlaps the current pin (extend in "
                         "place). Never shrinks; clips to neighbour pins; "
                         "fail-closed if the result would overlap another pin.")
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    only = set(s.strip().lower() for s in args.only.split(",") if s.strip())
    matches = json.load(open(args.match))
    sizes = load_sizes(args.symbols)
    lines, units = parse_splits(args.splits)
    # index by lowercase basename (RB3 splits.txt uses original-case basenames)
    units = {k.split("/")[-1].lower(): v for k, v in units.items()}
    eng = wired_engine(args.objects)
    tsm = json.load(open(args.tsm))
    # Valid split boundaries: a function start that is NOT covered by an earlier,
    # larger overlapping function (symbols.txt has overlapping symbols, e.g.
    # fn_8241C650 sits inside fn_8241C5D0's pdata-authoritative 0x130 length).
    # dtk fails "Split ... ends within symbol" if a range edge bisects a function.
    starts = []
    running_end = 0
    for a in sorted(sizes.keys()):
        if a >= running_end:
            starts.append(a)
        running_end = max(running_end, a + sizes[a])

    # content matches per engine cpp
    bycpp = {}
    name_of = {}
    for e in matches:
        cpp = e["dc3_obj"].lower().replace(".obj", ".cpp")
        if cpp not in eng:
            continue
        a = int(e["rb3_addr"], 16)
        bycpp.setdefault(cpp, []).append(a)
        name_of[a] = e["dc3_name"]

    # other units' pins (obstacles)
    pins = [(u["lo"], u["hi"], n) for n, u in units.items() if u.get("lo") is not None]
    pins.sort()

    proposals = []
    for cpp, addrs in bycpp.items():
        if only and cpp not in only:
            continue
        u = units.get(cpp)
        if not u or u.get("lo") is None:
            continue
        cur_lo, cur_hi = u["lo"], u["hi"]
        is_stub = (cur_hi - cur_lo) < args.stub_max
        # default mode -> stubs only (relocate); --extend -> non-stubs only (grow)
        if args.extend == is_stub:
            continue
        lo, hi, cl, frac = dominant_cluster(addrs, sizes, args.cluster_gap)
        if len(cl) < args.min_cluster or frac < args.min_frac:
            continue
        if args.extend:
            # extend in place ONLY: the cluster must overlap the current pin
            # (a far cluster = current pin is junk = the riskier "relocate-far"
            # case, deliberately NOT handled here).
            if hi <= cur_lo or lo >= cur_hi:
                continue
            desired_lo, desired_hi = min(cur_lo, lo), max(cur_hi, hi)
        else:
            desired_lo, desired_hi = lo, hi
        # clip to nearest OTHER-unit pin (relative to the desired span)
        left, right = 0, 1 << 32
        for plo, phi, pn in pins:
            if pn == cpp:
                continue
            if phi <= desired_lo:
                left = max(left, phi)
            elif plo >= desired_hi:
                right = min(right, plo)
            else:
                # other pin intrudes into desired span -> clip to its near edge
                if plo >= desired_lo:
                    right = min(right, plo)
                else:
                    left = max(left, phi)
        nlo, nhi = max(desired_lo, left), min(desired_hi, right)
        # snap to non-bisecting function boundaries, then re-clip
        li = bisect.bisect_right(starts, nlo) - 1
        if li >= 0:
            nlo = starts[li]
        hi_i = bisect.bisect_left(starts, nhi)
        nhi = starts[hi_i] if hi_i < len(starts) else nhi
        nlo = max(nlo, left)
        nhi = min(nhi, right)
        if args.extend:
            # never shrink the current pin
            nlo = min(nlo, cur_lo)
            nhi = max(nhi, cur_hi)
        # GUARD (fail-closed): never overlap another unit's pin
        if any(pn != cpp and not (phi <= nlo or plo >= nhi)
               for plo, phi, pn in pins):
            print(f"  SKIP {cpp}: guard tripped (would overlap a pin)")
            continue
        # newly-covered content-matches (outside the old pinned range)
        kept = [a for a in cl if nlo <= a < nhi and not (cur_lo <= a < cur_hi)]
        if len(kept) < 1 or nhi <= nlo:
            continue
        proposals.append((len(kept), cpp, cur_lo, cur_hi, nlo, nhi, kept))

    proposals.sort(reverse=True)
    total = sum(p[0] for p in proposals)
    mode = "extend non-stub" if args.extend else "relocate stub"
    print(f"engine units to {mode}: {len(proposals)}")
    print(f"content-matched functions newly pinned+named: {total}")
    for n, cpp, olo, ohi, nlo, nhi, kept in proposals[:30]:
        print(f"  {cpp:26s} +{n:3d}  0x{olo:08X}..0x{ohi:08X} -> "
              f"0x{nlo:08X}..0x{nhi:08X} ({nhi-nlo}B)")

    if not args.apply:
        print("\n(dry-run; --apply to write splits.txt + target_symbol_map.json)")
        return

    existing = set(k.lower() for k in tsm if k.lower().startswith("0x"))
    added = 0
    for n, cpp, olo, ohi, nlo, nhi, kept in proposals:
        u = units[cpp]
        lines[u["text_line"]] = f'{u["indent"]}.text       start:0x{nlo:08X} end:0x{nhi:08X}\n'
        for a in kept:
            key = "0x%08X" % a
            if key.lower() in existing:
                continue  # never overwrite a verified name (ICF aliases etc.)
            tsm[key] = name_of[a]
            added += 1
    print(f"target_symbol_map: +{added} new names (existing preserved)")
    open(args.splits, "w").writelines(lines)
    json.dump(tsm, open(args.tsm, "w"), indent=1)
    print(f"\nwrote {args.splits} ({len(proposals)} units) and {args.tsm}")
    print("next: rm -f build/45410914/target_symbol_renames.stamp && "
          "touch config/45410914/config.yml && ./tools/ninja-locked")


if __name__ == "__main__":
    main()
