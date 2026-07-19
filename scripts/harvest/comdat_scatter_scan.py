#!/usr/bin/env python3
"""COMDAT-scatter scanner: locate NAMED functions stuck at 0% whose machine
code is actually being emitted by a *different* compiled object than the unit
that "owns" the address span.

Background
----------
Retail MSVC/X360 (/O1, no LTCG) emits every function into its own COMDAT and
the linker scatters those COMDATs across `.text`.  dtk's SPLIT carves the retail
binary into per-*source-file* target objs by address range, so a function whose
COMDAT the linker placed inside unit X's address span is attributed to X — even
though the function's real source lives in unit Y and *our* compiled obj for Y
is the one that emits it.  objdiff then reads unit X at 0% for that symbol
(nobody in X emits it), while unit Y's obj holds the matching bytes under a
name objdiff never pairs into X.

The fix shapes (see commits 177cb4b9, c25769a8, ce936710) make the span-owning
.cpp emit the scattered COMDATs too — either by `#include`-ing the owner .cpp
whole, or by duplicating the small retail-arity body under `#ifndef HX_NATIVE`.
This scanner finds and ranks those opportunities, and separates them from the
genuinely UNWIRED pool (named 0% funcs no compiled obj emits at all — gameport
work).

What it does
------------
For every NAMED (non-`fn_`/`lbl_`) function at exactly 0%
(`match_percent_normalized == 0.0`) in a non-auto-generated unit of
`build/45410914/report.json`:

  a. Scan the COFF symbol tables of every compiled `.obj` under
     `build/45410914/src/` and record which objs *define* that symbol
     (SectionNumber > 0, i.e. not UNDEF/ABS/DEBUG).
  b. If some obj OTHER than the unit's own obj defines it -> SCATTER candidate.
     Recorded as (unit, symbol, emitting_obj, size) and grouped by
     (owning unit, emitting source file) into a ranked proposal table:
        "unit X should include/duplicate from owner Y (N fns, M bytes)".
  c. If NO compiled obj defines it -> UNWIRED (gameport pool).

Output
------
  - JSON: <out>/comdat_scatter_scan.json  (full machine-readable detail)
  - Human table printed to stdout (and mirrored to <out>/comdat_scatter_scan.txt)

Usage
-----
    python3 scripts/harvest/comdat_scatter_scan.py \
        [--report build/45410914/report.json] \
        [--obj-root build/45410914/src] \
        [--out-dir .] \
        [--min-bytes 0] [--top 60]

Re-runnable and fast: one pass over report.json + one pass over each .obj
(COFF symbol tables only, no relocation/section-data decode).  ~1-2s for the
whole tree.
"""

import argparse
import json
import struct
import sys
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Tuple

# --- COFF symbol-table parse (defined-external symbols only) ----------------
# Reuses the layout logic from scripts/obj_target_symbol_renamer.py, extended
# to capture SectionNumber + StorageClass so we can filter to *defined* symbols
# (a symbol an obj actually emits, vs an UNDEF import reference).

IMAGE_SYM_UNDEFINED = 0     # SectionNumber == 0
IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3
IMAGE_SYM_CLASS_LABEL = 6
IMAGE_SYM_DTYPE_FUNCTION = 0x20  # (Type >> 4) == 2


def coff_defined_symbols(data: bytes) -> Dict[str, int]:
    """Return {symbol_name: value/size_hint} for every DEFINED symbol.

    "Defined" = SectionNumber > 0 (has real content in this obj), storage
    class EXTERNAL or STATIC.  We don't require the function DTYPE because
    dtk-split target objs sometimes tag entry points as data/label; the
    caller only cares that the obj *emits* the symbol.  Value is returned as
    a best-effort size hint (COMDAT section length when resolvable, else the
    symbol Value field)."""
    out: Dict[str, int] = {}
    if len(data) < 20:
        return out
    nsec = struct.unpack_from("<H", data, 2)[0]
    sym_offset = struct.unpack_from("<I", data, 8)[0]
    num_syms = struct.unpack_from("<I", data, 12)[0]
    if sym_offset == 0 or num_syms == 0:
        return out
    str_table_offset = sym_offset + num_syms * 18

    # Section header table: 20-byte COFF header + nsec * 40-byte headers.
    sec_sizes: Dict[int, int] = {}
    sec_base = 20
    for s in range(nsec):
        off = sec_base + s * 40
        if off + 40 > len(data):
            break
        size = struct.unpack_from("<I", data, off + 16)[0]  # SizeOfRawData
        sec_sizes[s + 1] = size  # SectionNumber is 1-based

    i = 0
    while i < num_syms:
        entry_off = sym_offset + i * 18
        if entry_off + 18 > len(data):
            break
        name_bytes = data[entry_off:entry_off + 8]
        if name_bytes[:4] == b"\x00\x00\x00\x00":
            str_off = struct.unpack_from("<I", name_bytes, 4)[0]
            abs_off = str_table_offset + str_off
            if 0 <= abs_off < len(data):
                end = data.find(b"\x00", abs_off)
                if end < 0:
                    end = len(data)
                name = data[abs_off:end].decode("ascii", errors="replace")
            else:
                name = ""
        else:
            name = name_bytes.split(b"\x00")[0].decode("ascii", errors="replace")

        value = struct.unpack_from("<I", data, entry_off + 8)[0]
        sec_num = struct.unpack_from("<h", data, entry_off + 12)[0]
        storage = data[entry_off + 16]
        aux_count = data[entry_off + 17]

        if name and sec_num > 0 and storage in (
            IMAGE_SYM_CLASS_EXTERNAL,
            IMAGE_SYM_CLASS_STATIC,
        ):
            size_hint = sec_sizes.get(sec_num, value)
            # Prefer the larger of section size / value as a size hint; keep
            # the first definition seen (COMDAT primary).
            if name not in out:
                out[name] = size_hint
        i += 1 + aux_count
    return out


# --- report parsing ---------------------------------------------------------

def is_named(sym: str) -> bool:
    """A function is 'named' if it is not a dtk placeholder (fn_/lbl_)."""
    s = sym
    if s.startswith("?"):
        return True  # MSVC-mangled
    if s.startswith("fn_") or s.startswith("lbl_"):
        return False
    return True


def source_to_obj(source_path: str, obj_root: Path) -> Path:
    """src/system/beatmatch/MasterAudio.cpp ->
    <obj_root>/system/beatmatch/MasterAudio.obj

    obj_root is build/45410914/src, so strip the leading 'src/' from the
    source path and swap the extension."""
    p = source_path
    if p.startswith("src/"):
        p = p[len("src/"):]
    stem = p.rsplit(".", 1)[0]
    return obj_root / (stem + ".obj")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--report", default="build/45410914/report.json")
    ap.add_argument("--obj-root", default="build/45410914/src")
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--min-bytes", type=int, default=0,
                    help="drop proposals below this total scattered-byte count")
    ap.add_argument("--top", type=int, default=60,
                    help="rows in the human proposal table")
    args = ap.parse_args()

    report_path = Path(args.report)
    obj_root = Path(args.obj_root)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if not report_path.exists():
        print(f"ERROR: {report_path} not found", file=sys.stderr)
        return 2

    report = json.loads(report_path.read_text())

    # 1. Collect named-0% functions per non-auto unit.
    #    named0[symbol] -> list of (unit_name, source_path, own_obj, size)
    named0: Dict[str, List[Tuple[str, str, Path, int]]] = defaultdict(list)
    unit_src: Dict[str, str] = {}
    n_named0 = 0
    for u in report["units"]:
        md = u.get("metadata") or {}
        if md.get("auto_generated"):
            continue
        src = md.get("source_path") or ""
        unit_src[u["name"]] = src
        own_obj = source_to_obj(src, obj_root) if src else None
        for f in u.get("functions", []):
            sym = f["name"]
            if not is_named(sym):
                continue
            pct = f.get("match_percent_normalized")
            if pct is None or float(pct) != 0.0:
                continue
            size = int(f.get("size") or 0)
            named0[sym].append((u["name"], src, own_obj, size))
            n_named0 += 1

    # 2. Build the defined-symbol index over every compiled obj.
    #    defined[symbol] -> list of (obj_relpath, size_hint)
    defined: Dict[str, List[Tuple[str, int]]] = defaultdict(list)
    target_syms = set(named0.keys())
    objs = sorted(obj_root.rglob("*.obj"))
    for obj in objs:
        try:
            data = obj.read_bytes()
        except OSError:
            continue
        syms = coff_defined_symbols(data)
        rel = str(obj.relative_to(obj_root))
        for sym in target_syms:
            if sym in syms:
                defined[sym].append((rel, syms[sym]))

    # 3. Classify each (unit, symbol) occurrence.
    scatter_rows: List[dict] = []   # one per (unit, symbol) scatter candidate
    unwired_rows: List[dict] = []
    # A scatter candidate: an emitting obj distinct from the unit's own obj.
    for sym, occ in named0.items():
        emitters = defined.get(sym, [])
        for (unit_name, src, own_obj, size) in occ:
            own_rel = str(own_obj.relative_to(obj_root)) if own_obj else None
            foreign = [(o, sz) for (o, sz) in emitters if o != own_rel]
            if foreign:
                # pick the emitter with the largest size hint as the owner guess
                emit_obj, emit_sz = max(foreign, key=lambda t: t[1])
                scatter_rows.append({
                    "unit": unit_name,
                    "symbol": sym,
                    "size": size,
                    "emitting_obj": emit_obj,
                    "emit_size_hint": emit_sz,
                    "all_emitters": [o for o, _ in foreign],
                    "own_obj": own_rel,
                })
            else:
                unwired_rows.append({
                    "unit": unit_name,
                    "symbol": sym,
                    "size": size,
                    "own_obj": own_rel,
                    "src": src,
                })

    # 4. Group scatter candidates by (owning unit, emitting source obj).
    groups: Dict[Tuple[str, str], dict] = {}
    for r in scatter_rows:
        key = (r["unit"], r["emitting_obj"])
        g = groups.setdefault(key, {
            "unit": r["unit"],
            "owner_obj": r["emitting_obj"],
            "n_funcs": 0,
            "bytes": 0,
            "symbols": [],
        })
        g["n_funcs"] += 1
        g["bytes"] += r["size"]
        g["symbols"].append(r["symbol"])

    ranked = sorted(groups.values(), key=lambda g: (-g["bytes"], -g["n_funcs"]))
    ranked = [g for g in ranked if g["bytes"] >= args.min_bytes]

    # Group unwired by unit for the report.
    unwired_by_unit: Dict[str, dict] = {}
    for r in unwired_rows:
        g = unwired_by_unit.setdefault(r["unit"], {
            "unit": r["unit"], "src": r["src"], "n_funcs": 0, "bytes": 0,
            "symbols": [],
        })
        g["n_funcs"] += 1
        g["bytes"] += r["size"]
        g["symbols"].append(r["symbol"])
    unwired_ranked = sorted(unwired_by_unit.values(),
                            key=lambda g: (-g["n_funcs"], -g["bytes"]))

    # 5. Emit JSON.
    out_json = {
        "totals": {
            "named_0pct_occurrences": n_named0,
            "scatter_candidates": len(scatter_rows),
            "unwired_candidates": len(unwired_rows),
            "scatter_proposals": len(ranked),
            "unwired_units": len(unwired_ranked),
        },
        "scatter_proposals": ranked,
        "scatter_rows": scatter_rows,
        "unwired_by_unit": unwired_ranked,
    }
    json_path = out_dir / "comdat_scatter_scan.json"
    json_path.write_text(json.dumps(out_json, indent=2))

    # 6. Human table.
    lines: List[str] = []
    t = out_json["totals"]
    lines.append("=== COMDAT-scatter scan ===")
    lines.append(f"named-0% occurrences : {t['named_0pct_occurrences']}")
    lines.append(f"  SCATTER (emitted elsewhere) : {t['scatter_candidates']} funcs "
                 f"in {t['scatter_proposals']} (unit<-owner) proposals")
    lines.append(f"  UNWIRED (no obj emits)      : {t['unwired_candidates']} funcs "
                 f"in {t['unwired_units']} units")
    lines.append("")
    lines.append(f"--- TOP {args.top} SCATTER PROPOSALS (unit should pull from owner) ---")
    lines.append(f"{'bytes':>7} {'fns':>4}  unit  <-  owner_obj")
    for g in ranked[:args.top]:
        lines.append(f"{g['bytes']:>7} {g['n_funcs']:>4}  {g['unit']}  <-  {g['owner_obj']}")
    lines.append("")
    lines.append(f"--- TOP {args.top} UNWIRED UNITS (gameport pool) ---")
    lines.append(f"{'fns':>4} {'bytes':>7}  unit  ({'src'})")
    for g in unwired_ranked[:args.top]:
        lines.append(f"{g['n_funcs']:>4} {g['bytes']:>7}  {g['unit']}  ({g['src']})")
    text = "\n".join(lines)
    (out_dir / "comdat_scatter_scan.txt").write_text(text)
    print(text)
    print(f"\n[wrote {json_path} and {out_dir/'comdat_scatter_scan.txt'}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
