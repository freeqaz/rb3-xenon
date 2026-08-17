#!/usr/bin/env python3
"""Audit: do scope_map's PINNED-unit function addresses land on real .text?

Reproduction + regression detector for the SCOPEMAP-VA defect: scope_map's
pinned-unit branch computed a mangled-named function's address as
`base + report_relative_offset`.  report.json's per-function `address` is a
per-unit CUMULATIVE offset, so for a MULTI-BLOCK unit that is
`first_block_start + cumulative offset` -- the same synthetic formula CLAUDE.md
documents for dtk's `.s` address columns, independently reimplemented here.

Run against a scope_map build to count rows whose address falls OUTSIDE every
real `.text` block of the row's OWN unit, split single-block vs multi-block
(the control: the defect is structurally impossible in a single-block unit,
so single-block is the untreated population).

Usage:
    python3 tools/scope_map_addr_audit.py                 # summary + control table
    python3 tools/scope_map_addr_audit.py --samples 20    # show sample bad rows
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

SPLITS = os.path.join(ROOT, "config", "45410914", "splits.txt")
REPORT = os.path.join(ROOT, "build", "45410914", "report.json")

SPLIT_HDR_RE = re.compile(r"^(\S.*?):\s*$")
SPLIT_TEXT_RE = re.compile(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")
FN_ADDR_RE = re.compile(r"^fn_([0-9A-Fa-f]{8})$")


def load_split_blocks(path=SPLITS):
    """heading -> [(start,end), ...] for every .text block, in file order.

    Keyed on the FULL heading as written.  CLAUDE.md: key on full path, never
    basename -- `Movie.obj` genuinely collides between rnddx9/ and rndobj/.
    The basename alias is built separately and collisions are reported.
    """
    blocks = {}
    cur = None
    for line in open(path):
        h = SPLIT_HDR_RE.match(line)
        if h and not line.startswith((" ", "\t")):
            cur = h.group(1)
        elif cur:
            t = SPLIT_TEXT_RE.search(line)
            if t:
                blocks.setdefault(cur, []).append(
                    (int(t.group(1), 16), int(t.group(2), 16))
                )
    return blocks


def build_lookup(blocks):
    """(exact-heading map, basename-stem map, colliding stems)."""
    by_stem = {}
    collide = set()
    for head, blks in blocks.items():
        stem = os.path.basename(head).rsplit(".", 1)[0]
        if stem in by_stem:
            collide.add(stem)
        by_stem.setdefault(stem, []).append((head, blks))
    return blocks, by_stem, collide


def blocks_for_unit(unit, sp, blocks, by_stem, collide, anchors=()):
    """Resolve a report unit to its splits .text blocks. Full path wins.

    ⚠ Colliding basenames are disambiguated by GROUND TRUTH, not by string
    suffix.  `Game`, `UIStats` and `AccomplishmentProgress` each have BOTH a
    bare `Foo.cpp:` and a nested `band3/.../Foo.cpp:` heading, and a
    source_path-suffix match picks the NESTED one while the unit's real code
    lives under the BARE one.  That mis-resolution produced 79 phantom
    "outside" verdicts on the first run of this script -- CLAUDE.md's
    bare-vs-nested trap reproducing itself inside the detector.  `anchors` are
    the unit's own `fn_<addr>` VAs (authoritative); the candidate containing
    the most of them wins.
    """
    if sp and sp in blocks:
        return blocks[sp], "exact:source_path"
    stem = unit.split("/")[-1]
    cands = by_stem.get(stem)
    if not cands and sp:
        cands = by_stem.get(os.path.basename(sp).rsplit(".", 1)[0])
    if not cands:
        return None, "no-splits-entry"
    if len(cands) == 1:
        return cands[0][1], "stem:unique"
    if anchors:
        best, best_hits = None, 0
        for _head, blks in cands:
            hits = sum(1 for a in anchors if in_any_block(a, blks))
            if hits > best_hits:
                best, best_hits = blks, hits
        if best is not None:
            return best, "stem:anchor-resolved"
    if sp:
        for head, blks in cands:
            if sp.endswith(head) or head.endswith(sp):
                return blks, "stem:suffix-fallback"
    return None, "ambiguous-stem"


def in_any_block(addr, blks):
    return any(s <= addr < e for s, e in blks)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--samples", type=int, default=0)
    ap.add_argument("--report", default=REPORT)
    ap.add_argument("--json", help="write machine-readable result here")
    args = ap.parse_args()

    import scope_map

    blocks, by_stem, collide = build_lookup(load_split_blocks())
    rep = json.load(open(args.report))

    # Address every row exactly as scope_map's build path does.
    funcs, _ = scope_map.load_functions(args.report, dedup=False)
    # index rows by (unit, name) so we can attribute back to the report unit
    addr_of = {}
    for addr, size, matched, sp, unit, name, fz in funcs:
        addr_of[(unit, name)] = (addr, size, sp)

    stats = {
        "single": {"named": 0, "bad": 0},
        "multi": {"named": 0, "bad": 0},
    }
    unresolved_units = {}
    bad_rows = []
    bad_units = set()
    bad_bytes = 0

    for u in rep["units"]:
        unit = u["name"]
        sp = (u.get("metadata") or {}).get("source_path")
        if not sp:
            continue  # catch-all units are not this defect's population
        fns = u.get("functions") or []
        if not fns:
            continue
        anchors = [
            int(m.group(1), 16)
            for m in (FN_ADDR_RE.match(f["name"]) for f in fns)
            if m
        ]
        blks, how = blocks_for_unit(unit, sp, blocks, by_stem, collide, anchors)
        if not blks:
            unresolved_units[how] = unresolved_units.get(how, 0) + 1
            continue
        klass = "multi" if len(blks) > 1 else "single"
        for fn in fns:
            name = fn["name"]
            if FN_ADDR_RE.match(name):
                continue  # fn_ rows carry their VA in the name; not the population
            got = addr_of.get((unit, name))
            if got is None:
                continue
            addr, size, _sp = got
            stats[klass]["named"] += 1
            if not in_any_block(addr, blks):
                stats[klass]["bad"] += 1
                bad_units.add(unit)
                bad_bytes += size
                if len(bad_rows) < 100000:
                    bad_rows.append((unit, name, addr, size, blks[0][0], len(blks)))

    tot_named = stats["single"]["named"] + stats["multi"]["named"]
    tot_bad = stats["single"]["bad"] + stats["multi"]["bad"]

    print("=" * 74)
    print("scope_map pinned-unit ADDRESS audit")
    print("  population: mangled-named (non-fn_) functions in PINNED units")
    print("  bad = resolved address outside EVERY real .text block of its OWN unit")
    print("=" * 74)
    for k, label in (("single", "single-block units (CONTROL)"),
                     ("multi", "multi-block units  (TREATED)")):
        n, b = stats[k]["named"], stats[k]["bad"]
        pct = (100.0 * b / n) if n else 0.0
        print(f"  {label:32s}  {b:6d} / {n:6d} = {pct:6.2f}%")
    sp_, mp_ = (
        (100.0 * stats["single"]["bad"] / stats["single"]["named"]) if stats["single"]["named"] else 0.0,
        (100.0 * stats["multi"]["bad"] / stats["multi"]["named"]) if stats["multi"]["named"] else 0.0,
    )
    if sp_:
        print(f"  enrichment (multi / single)        {mp_ / sp_:6.2f}x")
    print(f"  TOTAL BAD ROWS                     {tot_bad} / {tot_named}")
    print(f"  bad bytes                          {bad_bytes:,}")
    print(f"  distinct units with >=1 bad row    {len(bad_units)}")
    if unresolved_units:
        print(f"  units skipped (no/ambiguous splits): {unresolved_units}")
    if collide:
        print(f"  NOTE colliding splits basenames: {sorted(collide)}")

    if args.samples:
        print("\nsample bad rows:")
        for unit, name, addr, size, b0, nb in bad_rows[: args.samples]:
            print(f"  {addr:08X} sz={size:<5d} blocks={nb:<3d} first={b0:08X} "
                  f"{unit}  {name[:70]}")

    if args.json:
        with open(args.json, "w") as f:
            json.dump(
                {
                    "single_named": stats["single"]["named"],
                    "single_bad": stats["single"]["bad"],
                    "multi_named": stats["multi"]["named"],
                    "multi_bad": stats["multi"]["bad"],
                    "total_bad": tot_bad,
                    "total_named": tot_named,
                    "bad_bytes": bad_bytes,
                    "bad_units": len(bad_units),
                    "bad_rows": [
                        {"unit": u, "name": n, "addr": a, "size": s}
                        for u, n, a, s, _b, _nb in bad_rows
                    ],
                },
                f,
            )
        print(f"\nwrote {args.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
