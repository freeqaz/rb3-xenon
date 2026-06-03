#!/usr/bin/env python3
"""Pin helper for byte-IDENTIFIED but UNPINNED functions.

WHY
---
Many backlog identifications (global fuzzy index ~1739, game) are byte-matched to
a DC3/Wii name, but their addr sits in UNPINNED `.text` space. objdiff only
measures functions inside a pinned unit split, so even a correctly-NAMED unpinned
fn never registers. This tool EXTENDS the owning unit's existing `.text` split
(UNION, never shrink) -- or adds a small fragment range -- to cover the addr,
snapped to NON-bisecting function boundaries and clipped to never overlap another
unit's pin. Fail-closed: if a clean range can't be carved, SKIP (never guess).

Regression-proof, like relocate_engine_splits.py:
  - snap edges to a function start NOT covered by an earlier larger fn (dtk fails
    "Split ... ends within symbol" otherwise);
  - clip to neighbour pins; GUARD: drop any proposal that would still overlap;
  - never shrink an existing pin;
  - ICF-pool addrs with an AMBIGUOUS owning unit are SKIPPED (don't guess).

INPUT: an {addr: owning_unit} mapping. owning_unit may be a basename (`Cheats`),
a basename.cpp, or a path (`band3/foo/Cheats.cpp`). Accepts:
  - flat JSON  {"0xADDR": "Cheats.cpp", ...}
  - records    [{"rb3_addr","dc3_obj"|"unit"|"owner", ...}]  (dc3_obj Ham*->Band*)
The owning unit MUST already have a .text pin in splits.txt (we extend it). An
addr whose owner is not pinned, or is ambiguous, is reported and SKIPPED.

USAGE
-----
  tools/pin_identified.py --map ids.json                       # dry-run report
  tools/pin_identified.py --map ids.json --apply --splits <wt>/.../splits.txt
"""
import argparse
import bisect
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEF_SPLITS = os.path.join(ROOT, "config", "45410914", "splits.txt")
DEF_SYMBOLS = os.path.join(ROOT, "config", "45410914", "symbols.txt")

UNIT_RE = re.compile(r"^(\S+\.(?:cpp|c|cc)):\s*$")
TEXT_RE = re.compile(r"^(\s*)\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")


def load_sizes(path):
    sizes = {}
    rx = re.compile(
        r"fn_([0-9A-Fa-f]+) = \.text:0x[0-9A-Fa-f]+; // type:function size:0x([0-9A-Fa-f]+)"
    )
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


def compute_starts(sizes):
    """Valid split boundaries = a fn start NOT covered by an earlier larger fn."""
    starts = []
    running_end = 0
    for a in sorted(sizes.keys()):
        if a >= running_end:
            starts.append(a)
        running_end = max(running_end, a + sizes[a])
    return starts


def owner_basename(val):
    """Normalize an owner value to lowercase basename (no ext)."""
    b = os.path.splitext(val.split("/")[-1])[0].lower()
    # DC3 Ham*.obj/.cpp -> RB3 Band* owner alias
    if b.startswith("ham"):
        b = "band" + b[3:]
    return b


def load_map(path):
    """Return {addr_int: owner_basename_lower}."""
    raw = json.load(open(path)) if path != "-" else json.load(sys.stdin)
    out = {}
    if isinstance(raw, dict):
        for k, v in raw.items():
            if not str(k).lower().startswith("0x"):
                continue
            out[int(k, 16)] = owner_basename(str(v))
        return out
    for e in raw:
        if not isinstance(e, dict):
            continue
        a = e.get("rb3_addr") or e.get("addr")
        owner = e.get("owner") or e.get("unit") or e.get("dc3_obj")
        if not a or not owner:
            continue
        out[int(a, 16)] = owner_basename(str(owner))
    return out


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--map", required=True,
                    help="{addr: owning_unit} JSON (or records); '-' = stdin")
    ap.add_argument("--splits", default=DEF_SPLITS)
    ap.add_argument("--symbols", default=DEF_SYMBOLS)
    ap.add_argument("--max-extend", type=lambda x: int(x, 0), default=0x4000,
                    help="refuse to grow a single pin by more than this many "
                         "bytes in one direction (sanity clamp); default 0x4000")
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    sizes = load_sizes(args.symbols)
    starts = compute_starts(sizes)
    lines, units = parse_splits(args.splits)
    # index units by lowercase basename
    by_base = {}
    for cpp, u in units.items():
        if u.get("lo") is None:
            continue
        by_base.setdefault(os.path.splitext(cpp.split("/")[-1])[0].lower(), cpp)

    amap = load_map(args.map)

    # all pins (obstacles)
    pins = [(u["lo"], u["hi"], cpp) for cpp, u in units.items()
            if u.get("lo") is not None]
    pins.sort()
    pstarts = [p[0] for p in pins]

    def covering_pin(addr):
        i = bisect.bisect_right(pstarts, addr) - 1
        if 0 <= i < len(pins) and pins[i][0] <= addr < pins[i][1]:
            return pins[i]
        return None

    skip_already = skip_noowner = skip_ambig = skip_guard = skip_far = 0
    # group target addrs by owning unit cpp
    want = {}  # cpp -> set(addr)
    for addr, owner in sorted(amap.items()):
        cp = covering_pin(addr)
        if cp is not None:
            skip_already += 1  # already inside SOME pin -> already measurable
            continue
        cpp = by_base.get(owner)
        if cpp is None:
            skip_noowner += 1
            continue
        want.setdefault(cpp, set()).add(addr)

    proposals = []
    for cpp, addrs in want.items():
        u = units[cpp]
        cur_lo, cur_hi = u["lo"], u["hi"]
        desired_lo = min(cur_lo, min(addrs))
        desired_hi = max(cur_hi, max(addrs) + sizes.get(max(addrs), 4))

        # clip to nearest OTHER-unit pin
        left, right = 0, 1 << 32
        for plo, phi, pn in pins:
            if pn == cpp:
                continue
            if phi <= desired_lo:
                left = max(left, phi)
            elif plo >= desired_hi:
                right = min(right, plo)
            else:
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
        # never shrink the current pin
        nlo = min(nlo, cur_lo)
        nhi = max(nhi, cur_hi)

        # sanity clamp on how far we grew
        if (cur_lo - nlo) > args.max_extend or (nhi - cur_hi) > args.max_extend:
            skip_far += len(addrs)
            continue

        # which requested addrs are now covered (and weren't before)?
        covered = sorted(a for a in addrs if nlo <= a < nhi)
        if not covered:
            # clipping pushed us off the targets -> can't pin cleanly
            skip_ambig += len(addrs)
            continue

        # GUARD (fail-closed): never overlap another unit's pin
        if any(pn != cpp and not (phi <= nlo or plo >= nhi)
               for plo, phi, pn in pins):
            skip_guard += len(addrs)
            continue

        proposals.append((len(covered), cpp, cur_lo, cur_hi, nlo, nhi, covered,
                          len(addrs) - len(covered)))
        skip_ambig += len(addrs) - len(covered)

    proposals.sort(reverse=True)
    newly = sum(p[0] for p in proposals)
    print(f"unpinned ids requested        : {len(amap)}", file=sys.stderr)
    print(f"  already inside a pin (skip) : {skip_already}", file=sys.stderr)
    print(f"  owner not pinned (skip)     : {skip_noowner}", file=sys.stderr)
    print(f"  clip/ambiguous (skip)       : {skip_ambig}", file=sys.stderr)
    print(f"  grew too far > max-extend   : {skip_far}", file=sys.stderr)
    print(f"  guard tripped (skip)        : {skip_guard}", file=sys.stderr)
    print(f"units to extend               : {len(proposals)}", file=sys.stderr)
    print(f"addrs newly pinned            : {newly}", file=sys.stderr)
    for n, cpp, olo, ohi, nlo, nhi, cov, dropped in proposals[:30]:
        print(f"  {os.path.basename(cpp):28s} +{n:3d}  "
              f"0x{olo:08X}..0x{ohi:08X} -> 0x{nlo:08X}..0x{nhi:08X} "
              f"({nhi-nlo}B){' drop%d' % dropped if dropped else ''}",
              file=sys.stderr)

    if not args.apply:
        print("\n(dry-run; --apply to rewrite splits.txt)", file=sys.stderr)
        return 0

    for n, cpp, olo, ohi, nlo, nhi, cov, dropped in proposals:
        u = units[cpp]
        lines[u["text_line"]] = (
            f'{u["indent"]}.text       start:0x{nlo:08X} end:0x{nhi:08X}\n')
    open(args.splits, "w").writelines(lines)
    print(f"\nwrote {args.splits} ({len(proposals)} units extended)",
          file=sys.stderr)
    print("next: rm -f build/45410914/target_symbol_renames.stamp && "
          "touch config/45410914/config.yml && ./tools/ninja-locked",
          file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
