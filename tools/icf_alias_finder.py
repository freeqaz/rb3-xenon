#!/usr/bin/env python3
"""Find / validate PROVEN ICF-merged-symbol alias pairs (the PoolAlloc gap).

The problem
-----------
Retail RB3 ICF-folds debug-stripped allocator overloads (2-arg ``PoolAlloc``,
1-arg ``MemOrPoolAlloc``/``MemOrPoolAllocSTL``) onto their named debug siblings.
Only the named (debug) spelling survives at a single address in the XEX, so
``scripts/target_symbol_map.json`` has just that one. Our compiled objs emit the
byte-IDENTICAL call site but reference the folded (debug-stripped) spelling, so
objdiff's by-name ``reloc_eq`` flags a ``[sym]`` mismatch. ``scripts/
symbol_aliases.json`` + ``tools/gen_symbol_alias_map.py`` declare the folds to
objdiff's ``map_file``/``symbol_equivalences`` mechanism, neutralizing the noise.

This tool is the EVIDENCE engine behind that data file:

  --validate   Confirm scripts/symbol_aliases.json is well-formed and grounded:
               (a) each group's survivor is in target_symbol_map.json,
               (b) every folded spelling is referenced by >=1 compiled obj,
               (c) the survivor is the ONLY group member named in the target objs
                   (a real ICF fold keeps exactly one spelling).
               Exit 1 on any failure. Use in CI / before landing edits.

  --scan       Discover NEW fold candidates from objdiff diffs: scan the non-100
               function pool for paired ``bl <A>`` / ``bl <B>`` rows where the
               relocation differs ONLY by symbol name (byte-identical call site).
               Reports, per candidate alias pair, the victim functions and how
               many would flip to 100% if aliased (the "pure" victims). Use this
               to seed new groups -- ONLY add a pair the scan proves by the
               byte-identical-call-site signature, never by fuzzy matching.

  --report     Apply the current alias map to the non-100 pool and report the
               per-function norm-% delta (with vs without the synthetic map).
               Shows the realized lever: which functions flip to 100 and which
               only gain partial credit (other divergences remain).

All three are read-only (no build inputs mutated).
"""

import argparse
import glob
import json
import os
import struct
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
ALIASES = PROJECT_ROOT / "scripts" / "symbol_aliases.json"
TARGET_MAP = PROJECT_ROOT / "scripts" / "target_symbol_map.json"
OBJDIFF_JSON = PROJECT_ROOT / "objdiff.json"
REPORT_JSON = PROJECT_ROOT / "build" / "45410914" / "report.json"
OBJDIFF_CLI = PROJECT_ROOT / "bin" / "objdiff-cli"
GEN_MAP = PROJECT_ROOT / "tools" / "gen_symbol_alias_map.py"


# ---------------------------------------------------------------------------
# COFF symbol table reader (mirrors obj_target_symbol_renamer.py's parser).
# ---------------------------------------------------------------------------
def coff_referenced_symbols(data: bytes) -> set:
    """All symbol names present in a COFF .obj (defined + undefined refs)."""
    if len(data) < 20:
        return set()
    sym_off = struct.unpack_from("<I", data, 8)[0]
    num = struct.unpack_from("<I", data, 12)[0]
    if not sym_off or not num:
        return set()
    str_start = sym_off + num * 18
    out = set()
    i = 0
    while i < num:
        eo = sym_off + i * 18
        if eo + 18 > len(data):
            break
        nb = data[eo:eo + 8]
        if nb[:4] == b"\x00\x00\x00\x00":
            so = struct.unpack_from("<I", nb, 4)[0]
            ao = str_start + so
            if ao < len(data):
                end = data.index(b"\x00", ao)
                name = data[ao:end].decode("ascii", "replace")
            else:
                name = ""
        else:
            name = nb.split(b"\x00")[0].decode("ascii", "replace")
        out.add(name)
        i += 1 + data[eo + 17]
    return out


def load_aliases() -> list:
    return json.loads(ALIASES.read_text()).get("groups", [])


def target_map_names() -> set:
    raw = json.loads(TARGET_MAP.read_text())
    return {v for k, v in raw.items() if k.lower().startswith("0x")}


def compiled_obj_symbol_index() -> dict:
    """name -> list of compiled-obj stems referencing it."""
    idx = defaultdict(list)
    for p in glob.glob(str(PROJECT_ROOT / "build/45410914/src/**/*.obj"), recursive=True):
        for s in coff_referenced_symbols(Path(p).read_bytes()):
            idx[s].append(Path(p).stem)
    return idx


def target_obj_symbol_index() -> dict:
    """name -> count of dtk-split target objs referencing it (post-renamer)."""
    idx = defaultdict(int)
    for p in glob.glob(str(PROJECT_ROOT / "build/45410914/obj/*.obj")):
        for s in coff_referenced_symbols(Path(p).read_bytes()):
            idx[s] += 1
    return idx


# ---------------------------------------------------------------------------
# --validate
# ---------------------------------------------------------------------------
def cmd_validate() -> int:
    groups = load_aliases()
    tmap = target_map_names()
    compiled = compiled_obj_symbol_index()
    target_objs = target_obj_symbol_index()
    ok = True
    for g in groups:
        name = g.get("name", g.get("survivor"))
        survivor = g["survivor"]
        folded = g.get("folded", [])
        # (a) survivor is in target_symbol_map.json
        if survivor not in tmap:
            print(f"FAIL [{name}]: survivor {survivor} NOT in target_symbol_map.json")
            ok = False
        # (b) every folded spelling referenced by >=1 compiled obj
        for f in folded:
            if f not in compiled:
                print(f"FAIL [{name}]: folded {f} referenced by 0 compiled objs")
                ok = False
        # (c) survivor is the ONLY group member named in the target objs
        named_in_target = [s for s in (survivor, *folded) if target_objs.get(s, 0) > 0]
        if named_in_target != [survivor]:
            print(f"FAIL [{name}]: target objs name {named_in_target}, "
                  f"expected only [{survivor}] (a real ICF fold keeps one spelling)")
            ok = False
        if ok:
            n_compiled = len(set(c for f in folded for c in compiled.get(f, [])))
            print(f"OK   [{name}]: survivor in target_map; "
                  f"{len(folded)} folded spelling(s) over {n_compiled} compiled objs; "
                  f"target objs ref survivor {target_objs.get(survivor,0)}x")
    print("VALIDATE: PASS" if ok else "VALIDATE: FAIL")
    return 0 if ok else 1


# ---------------------------------------------------------------------------
# objdiff diff helper
# ---------------------------------------------------------------------------
def diff_fn(tp, bp, fn, mapf=None, include_instructions=False):
    cmd = [str(OBJDIFF_CLI), "diff", "-1", tp, "-2", bp, "-f", "json", "-o", "-", fn]
    if include_instructions:
        cmd.append("--include-instructions")
    if mapf:
        cmd += ["--map-file", mapf]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        return json.loads(out.stdout)
    except Exception:
        return None


def units_index() -> dict:
    cfg = json.loads(OBJDIFF_JSON.read_text())
    return {u["name"]: u for u in cfg.get("units", [])}


def pool_referencing_unit_stems(folded_names: set) -> set:
    """Compiled-obj stems referencing any folded spelling (candidate victims)."""
    stems = set()
    for p in glob.glob(str(PROJECT_ROOT / "build/45410914/src/**/*.obj"), recursive=True):
        syms = coff_referenced_symbols(Path(p).read_bytes())
        if syms & folded_names:
            stems.add(Path(p).stem)
    return stems


def is_alias_bl_row(row, pair_set) -> bool:
    """True if this is a paired ``bl`` whose target/base refs are an alias pair."""
    t = (row.get("target") or {})
    b = (row.get("base") or {})
    if t.get("opcode") != "bl" or b.get("opcode") != "bl":
        return False
    ta = t.get("args", "") or ""
    ba = b.get("args", "") or ""
    for a, c in pair_set:
        if a in ta and c in ba:
            return True
    return False


def build_pair_set(groups) -> set:
    """All (target_name, base_name) ordered alias pairs from the groups."""
    pairs = set()
    for g in groups:
        survivor = g["survivor"]
        for f in g.get("folded", []):
            pairs.add((survivor, f))   # target=survivor, base=folded
    return pairs


def cmd_scan(args) -> int:
    groups = load_aliases()
    pair_set = build_pair_set(groups)
    folded = {f for g in groups for f in g.get("folded", [])}
    units = units_index()
    report = json.loads(REPORT_JSON.read_text())
    stems = pool_referencing_unit_stems(folded)
    cands = []
    for u in report.get("units", []):
        if u["name"].split("/")[-1] not in stems:
            continue
        for f in u.get("functions", []):
            pct = f.get("match_percent_normalized", 0)
            if pct < 100.0:
                cands.append((pct, u["name"], f["name"]))
    print(f"scanning {len(cands)} non-100 fns in {len(stems)} alias-referencing units...",
          file=sys.stderr)
    pure, withd = [], []
    checked = 0
    for pct, un, fn in cands:
        uu = units.get(un)
        if not uu:
            continue
        tp, bp = uu["target_path"], uu["base_path"]
        if not (os.path.exists(tp) and os.path.exists(bp)):
            continue
        d = diff_fn(tp, bp, fn, include_instructions=True)
        if not d:
            continue
        checked += 1
        rows = d.get("instructions", [])
        nonmatch = [r for r in rows if r.get("match_type") != "equal"]
        if not nonmatch:
            continue
        has_alias = any(is_alias_bl_row(r, pair_set) for r in nonmatch)
        if not has_alias:
            continue
        all_alias = all(is_alias_bl_row(r, pair_set) for r in nonmatch)
        rec = {"pct": pct, "unit": un, "fn": fn,
               "alias_rows": sum(1 for r in nonmatch if is_alias_bl_row(r, pair_set)),
               "other_rows": sum(1 for r in nonmatch if not is_alias_bl_row(r, pair_set))}
        withd.append(rec)
        if all_alias:
            pure.append(rec)
    print(f"checked {checked} fns")
    print(f"victims with an alias-pair bl diff row: {len(withd)}")
    print(f"PURE victims (alias is ONLY diff -> would flip to 100): {len(pure)}")
    for r in sorted(pure, key=lambda x: x["pct"]):
        print(f"  PURE {r['pct']:6.2f}  {r['unit']}  {r['fn'][:60]}")
    for r in sorted(withd, key=lambda x: x["pct"]):
        if r in pure:
            continue
        print(f"  WITH {r['pct']:6.2f}  +{r['other_rows']} other  {r['unit']}  {r['fn'][:50]}")
    if args.out:
        Path(args.out).write_text(json.dumps({"pure": pure, "with_other": withd}, indent=2))
        print(f"wrote {args.out}")
    return 0


def cmd_report(args) -> int:
    """Realized-lever report: per-fn norm-% with vs without the synthetic map."""
    # Ensure the synthetic map exists.
    mapf = args.map or str(PROJECT_ROOT / "build" / "45410914" / "icf_aliases.map")
    if not os.path.exists(mapf):
        subprocess.run([sys.executable, str(GEN_MAP), "--out", mapf], check=True)
    groups = load_aliases()
    pair_set = build_pair_set(groups)
    folded = {f for g in groups for f in g.get("folded", [])}
    units = units_index()
    report = json.loads(REPORT_JSON.read_text())
    stems = pool_referencing_unit_stems(folded)
    flips, gains = [], []
    cands = []
    for u in report.get("units", []):
        if u["name"].split("/")[-1] not in stems:
            continue
        for f in u.get("functions", []):
            pct = f.get("match_percent_normalized", 0)
            if pct < 100.0:
                cands.append((pct, u["name"], f["name"]))
    print(f"measuring {len(cands)} non-100 fns with/without alias map...", file=sys.stderr)
    for pct, un, fn in cands:
        uu = units.get(un)
        if not uu:
            continue
        tp, bp = uu["target_path"], uu["base_path"]
        if not (os.path.exists(tp) and os.path.exists(bp)):
            continue
        d0 = diff_fn(tp, bp, fn, include_instructions=True)
        if not d0:
            continue
        rows = d0.get("instructions", [])
        nonmatch = [r for r in rows if r.get("match_type") != "equal"]
        if not any(is_alias_bl_row(r, pair_set) for r in nonmatch):
            continue
        n0 = d0["normalized_match_percent"]
        d1 = diff_fn(tp, bp, fn, mapf=mapf)
        n1 = d1["normalized_match_percent"] if d1 else n0
        if n1 >= 100.0 and n0 < 100.0:
            flips.append((n0, n1, un, fn))
        elif n1 > n0:
            gains.append((n0, n1, un, fn))
    print(f"FLIPS to 100% (matched-count gain): {len(flips)}")
    for n0, n1, un, fn in sorted(flips):
        print(f"  FLIP {n0:6.2f} -> {n1:6.2f}  {un}  {fn[:55]}")
    print(f"partial-credit gains (still <100, other divergence remains): {len(gains)}")
    for n0, n1, un, fn in sorted(gains):
        print(f"  GAIN {n0:6.3f} -> {n1:6.3f}  {un}  {fn[:50]}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="mode")
    ap.add_argument("--validate", action="store_true",
                    help="validate scripts/symbol_aliases.json against the objs")
    ap.add_argument("--scan", action="store_true",
                    help="discover new fold candidates from objdiff diffs")
    ap.add_argument("--report", action="store_true",
                    help="realized-lever per-fn delta with/without the alias map")
    ap.add_argument("--out", default=None, help="write scan/report JSON to PATH")
    ap.add_argument("--map", default=None, help="synthetic map path (for --report)")
    args = ap.parse_args()
    if args.validate:
        return cmd_validate()
    if args.scan:
        return cmd_scan(args)
    if args.report:
        return cmd_report(args)
    ap.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
