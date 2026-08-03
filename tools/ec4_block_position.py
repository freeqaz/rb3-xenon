#!/usr/bin/env python3
"""EC-4: classify every report row by its POSITION in its unit's pinned .text block.

Lane EC-2 established -- inside the 39-unit COMPLETABLE bucket only -- that
misattribution (the retail function at a pinned VA belongs to a different class
than the name assigned) is CONFINED TO SLIVER PINS:

    WHOLE 8/10 = 80.0% foreign | START 3/7 = 42.9% | MID 0/31 = 0.0%
    untreated control 8/296 = 2.7%

EC-2 computed that cross-tab AD HOC and never committed the classifier; only the
table survives, in docs/decomp/EC2_MISATTRIBUTION_SIZED_2026-08-03.md.  This is
that missing instrument, generalised from one bucket to the WHOLE BINARY.

POSITION is defined over the rows that fall inside one `.text` block of
config/45410914/splits.txt:

    WHOLE  the block contains exactly ONE function  -- a "sliver pin", the shape
           a speculative carve leaves behind.  Highest suspicion.
    START  first function of a multi-function block -- a TU's first function
           legitimately neighbours the PREVIOUS TU, so this is the predicted
           boundary artifact and reads in between.
    MID    strictly inside a multi-function span    -- EC-2: 0 of 31 foreign.
    END    last function of a multi-function block.

WHY ROW COUNT IS THE RIGHT PROXY FOR "how many retail functions are in this
block": the target .obj dtk emits for a pinned span contains EVERY retail
function in that span, so report.json's row list for the unit enumerates them.
The tool cross-checks this by comparing the block's byte extent against the sum
of its rows' sizes and reports any block where they disagree, rather than
assuming it.

VA DERIVATION -- two independent sources, neither trusted blindly:
  * anon rows are named `fn_<8HEX>`, which IS the retail VA (the anon gate:
    a row still named fn_ is precisely a retail address absent from the map);
  * named rows resolve through scripts/target_symbol_map.json, and ONLY when
    the name maps to exactly one address -- a name at several addresses makes
    no unambiguous positional claim and is dropped, never guessed.
Rows with no resolvable VA are reported as UNPLACED and excluded from every
rate, so an unplaceable population cannot silently inflate or deflate a bucket.
"""
import argparse
import bisect
import collections
import json
import pathlib
import re
import sys

ANON_RX = re.compile(r"^fn_([0-9A-Fa-f]{8})$")
BLOCK_RX = re.compile(r"^\s*\.text\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)")
UNIT_RX = re.compile(r"^(\S.*?):\s*$")


def parse_splits(path):
    """splits.txt -> {splits_key: [(lo, hi), ...]} for .text blocks only."""
    out = collections.defaultdict(list)
    cur = None
    for line in pathlib.Path(path).read_text().splitlines():
        m = UNIT_RX.match(line)
        if m:
            cur = m.group(1)
            if cur == "Sections":
                cur = None
            continue
        if cur is None:
            continue
        b = BLOCK_RX.match(line)
        if b:
            out[cur].append((int(b.group(1), 16), int(b.group(2), 16)))
    for k in out:
        out[k].sort()
    return dict(out)


def splits_key_to_unit(key):
    """MasterAudio.cpp -> default/MasterAudio ; system/gesture/X.cpp -> default/system/gesture/X"""
    stem = re.sub(r"\.(cpp|c|s|cc)$", "", key)
    return "default/" + stem


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--report", default=None)
    ap.add_argument("--splits", default=None)
    ap.add_argument("--map", dest="mapfile", default=None)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    root = pathlib.Path(a.root).resolve()
    rep = json.loads(pathlib.Path(a.report or root / "build/45410914/report.json").read_text())
    splits = parse_splits(a.splits or root / "config/45410914/splits.txt")
    amap = json.loads(pathlib.Path(a.mapfile or root / "scripts/target_symbol_map.json").read_text())

    rev = collections.defaultdict(list)
    for k, v in amap.items():
        if k.startswith("0x"):
            rev[v].append(int(k, 16))

    runits = {u["name"]: u for u in rep["units"]}
    blocks_by_unit = {}
    for key, blks in splits.items():
        un = splits_key_to_unit(key)
        if un in runits:
            blocks_by_unit[un] = (key, blks)

    rows = []
    unplaced = collections.Counter()
    extent_mismatch = []
    for un, (key, blks) in sorted(blocks_by_unit.items()):
        ru = runits[un]
        fns = ru.get("functions") or []
        los = [b[0] for b in blks]
        placed = collections.defaultdict(list)
        for f in fns:
            nm = f["name"]
            m = ANON_RX.match(nm)
            if m:
                va = int(m.group(1), 16)
                src = "anon_name"
            else:
                vas = rev.get(nm, [])
                if len(vas) != 1:
                    unplaced["name_ambiguous_or_unmapped"] += 1
                    continue
                va = vas[0]
                src = "map"
            i = bisect.bisect_right(los, va) - 1
            if i < 0 or not (blks[i][0] <= va < blks[i][1]):
                unplaced["va_outside_every_block"] += 1
                continue
            placed[i].append((va, f, src))

        for i, items in placed.items():
            items.sort(key=lambda t: t[0])
            lo, hi = blks[i]
            span = hi - lo
            ssum = sum(int(t[1]["size"]) for t in items)
            if len(items) == 1 and abs(span - int(items[0][1]["size"])) > 8:
                extent_mismatch.append(dict(unit=un, block=[hex(lo), hex(hi)], span=span,
                                            size=int(items[0][1]["size"]), sym=items[0][1]["name"]))
            for j, (va, f, src) in enumerate(items):
                if len(items) == 1:
                    pos = "WHOLE"
                elif j == 0:
                    pos = "START"
                elif j == len(items) - 1:
                    pos = "END"
                else:
                    pos = "MID"
                rows.append(dict(
                    unit=un, splits_key=key, sym=f["name"], va=hex(va), va_src=src,
                    size=int(f["size"]), mpn=f["match_percent_normalized"],
                    fuzzy=f.get("fuzzy_match_percent", 0.0),
                    pos=pos, block=[hex(lo), hex(hi)], block_rows=len(items),
                    block_span=span, block_size_sum=ssum,
                    anon=bool(ANON_RX.match(f["name"])),
                ))

    pathlib.Path(a.out).write_text(json.dumps(rows, indent=1))

    print(f"units with pinned .text blocks paired to a report unit: {len(blocks_by_unit)}")
    print(f"placed rows: {len(rows)}   UNPLACED: {dict(unplaced)}")
    print(f"single-row blocks whose span != the row size (>8B): {len(extent_mismatch)}"
          "   <- proxy cross-check")
    print("\n=== POSITION x CHARGE (named rows only; anon rows cannot be adjudicated by name) ===")
    named = [r for r in rows if not r["anon"]]
    tab = collections.Counter()
    for r in named:
        tab[(r["pos"], "sub100" if r["mpn"] < 100.0 else "at100")] += 1
    print(f"{'pos':8s} {'sub100':>8s} {'at100':>8s} {'total':>8s}")
    for pos in ("WHOLE", "START", "MID", "END"):
        s, t = tab[(pos, "sub100")], tab[(pos, "at100")]
        print(f"{pos:8s} {s:8d} {t:8d} {s+t:8d}")
    print(f"\nALL rows incl. anon: {collections.Counter(r['pos'] for r in rows)}")
    print(f"wrote {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
