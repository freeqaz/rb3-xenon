#!/usr/bin/env python3
"""Lane AE — unemitted-symbol scan.

Finds TARGET functions that are **unclaimable by any name our build compiles**:
the target obj carries a mangled symbol name N (from scripts/target_symbol_map.json,
applied by obj_target_symbol_renamer), but *no object file anywhere in our build*
defines a symbol named N.  objdiff pairs target<->base **by name**, so such a
function is structurally pinned at 0% no matter how correct the source body is.

The seed case: ObjRefConcrete<T>::SetObj(Object*) reads r5, i.e. the retail body
is really `Replace(from, to)` -- and our build emits ZERO
`?Replace@?$ObjRefConcrete@...` symbols.  The fix is a SOURCE change (emit the
symbol retail has), not a map/splits change.

Usage:
    python3 scripts/harvest/unemitted_symbol_scan.py            # funnel summary
    python3 scripts/harvest/unemitted_symbol_scan.py --json OUT  # full records
"""

import argparse
import collections
import json
import os
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
BUILD = ROOT / "build" / "45410914"
SRC_OBJ = BUILD / "src"
REPORT = BUILD / "report.json"
MAP = ROOT / "scripts" / "target_symbol_map.json"

# --- XDK / Quazal vendor spans are hard-skipped for this lane -----------------
SCOPE_SKIP_UNIT_RE = re.compile(r"auto_03_")


def coff_defined_symbols(path):
    """Return the set of symbol names DEFINED (section > 0) by a COFF obj."""
    try:
        data = path.read_bytes()
    except OSError:
        return set()
    if len(data) < 20:
        return set()
    try:
        (machine, num_sections, ts, symtab_off, num_syms,
         opt_hdr_size, flags) = struct.unpack_from("<HHIIIHH", data, 0)
    except struct.error:
        return set()
    if symtab_off == 0 or num_syms == 0:
        return set()
    strtab_off = symtab_off + num_syms * 18
    if strtab_off + 4 > len(data):
        return set()
    strtab_size = struct.unpack_from("<I", data, strtab_off)[0]
    strtab = data[strtab_off:strtab_off + strtab_size]

    out = set()
    i = 0
    while i < num_syms:
        so = symtab_off + i * 18
        if so + 18 > len(data):
            break
        if data[so:so + 4] == b"\0\0\0\0":
            str_off = struct.unpack_from("<I", data, so + 4)[0]
            end = strtab.find(b"\0", str_off)
            name = strtab[str_off:end].decode("ascii", "replace") if end >= 0 else ""
        else:
            name = data[so:so + 8].rstrip(b"\0").decode("ascii", "replace")
        value, section, type_val, storage, aux = struct.unpack_from(
            "<IhHBB", data, so + 8)
        # section > 0  => defined in this obj (0 = extern/undefined, <0 = abs/debug)
        if section > 0 and name:
            out.add(name)
        i += 1 + aux
    return out


def emitted_symbol_set(verbose=False):
    """Union of every symbol DEFINED by any object our build compiles."""
    syms = set()
    n = 0
    for dirpath, _dirs, files in os.walk(SRC_OBJ):
        for fn in files:
            if fn.endswith(".obj"):
                syms |= coff_defined_symbols(Path(dirpath) / fn)
                n += 1
    if verbose:
        print(f"[emitted] {n} objs -> {len(syms)} defined symbols", file=sys.stderr)
    return syms, n


# Retail coverage-breadcrumb stubs and carving artifacts we must NOT "fix".
def is_excluded_name(name):
    if name.startswith("fn_"):
        return True          # unmapped: a different pool (map coverage, not source)
    if name.startswith("__unwind$") or name.startswith("$"):
        return True
    if "?_unwind" in name:
        return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", help="write full candidate records here")
    ap.add_argument("--min-size", type=int, default=0)
    args = ap.parse_args()

    report = json.load(open(REPORT))
    tmap = json.load(open(MAP))
    icf_arbitrary = set(tmap.get("_icf_arbitrary", []))

    emitted, nobjs = emitted_symbol_set(verbose=True)

    # --- funnel counters ---
    total_fns = 0
    zero_fns = 0
    inscope = 0
    named_zero = 0
    unemitted = []
    unmapped_zero = 0
    emitted_but_zero = 0

    for u in report["units"]:
        uname = u["name"]
        skip_unit = bool(SCOPE_SKIP_UNIT_RE.search(uname))
        for f in u.get("functions", []):
            total_fns += 1
            pct = f.get("match_percent_normalized")
            if pct != 0.0:
                continue
            zero_fns += 1
            if skip_unit:
                continue
            inscope += 1
            name = f["name"]
            if name.startswith("fn_"):
                unmapped_zero += 1
                continue
            if is_excluded_name(name):
                continue
            named_zero += 1
            if name in emitted:
                emitted_but_zero += 1
                continue
            size = int(f.get("size", 0) or 0)
            if size < args.min_size:
                continue
            unemitted.append({
                "unit": uname,
                "symbol": name,
                "size": size,
                "demangled": f.get("metadata", {}).get("demangled_name", ""),
            })

    # ------------------------------------------------------------------
    # GENERALISATION 1 — FIX-SIG vs ADD-DECL.
    # If our build emits the SAME scope::method with a DIFFERENT signature,
    # the bug is not a missing definition, it is a **wrong declaration**, and
    # retail's mangled name is the ground truth for the correct one.  These
    # are by far the most actionable rows (const-ness, virtual-ness, an extra
    # parameter, __restrict, a different class in a parameter type...).
    # ------------------------------------------------------------------
    def scope_key(sym):
        """scope+method prefix = everything before the signature's '@@'."""
        i = sym.find("@@")
        return sym[:i] if i > 0 else None

    emitted_by_scope = collections.defaultdict(list)
    for x in emitted:
        k = scope_key(x)
        if k:
            emitted_by_scope[k].append(x)

    for r in unemitted:
        alts = emitted_by_scope.get(scope_key(r["symbol"]) or "", [])
        r["verdict"] = "FIX-SIG" if alts else "ADD-DECL"
        r["ours"] = sorted(alts)[:3]

    # ------------------------------------------------------------------
    # GENERALISATION 2 — the "wrong variant selected" detector.
    # The project has two source lineages in-tree (RB3 `Band*` / `meta_band`
    # and DC3 `Ham*` / `meta_ham`).  Retail contains BOTH families, so a
    # candidate whose symbol becomes an EMITTED symbol under a Ham<->Band
    # substitution is a case where both variants exist in our tree and we
    # instantiated the wrong one.  (Confirmed inverse of the usual
    # "retail predates the DEV additions" trap.)
    # ------------------------------------------------------------------
    for r in unemitted:
        r["variant_swap"] = None
        for a, b in (("Ham", "Band"), ("Band", "Ham")):
            alt = r["symbol"].replace(a, b)
            if alt != r["symbol"] and alt in emitted:
                r["variant_swap"] = {"dir": f"{a}->{b}", "ours": alt}
                break

    # --- cluster by "family": the mangled scope (class / template) -------------
    def family(sym):
        # ?Method@Class@@... -> Class ; template args collapsed
        m = re.match(r"\?\??[A-Za-z0-9_]*@(.*?)@@", sym)
        if m:
            return m.group(1)
        return sym[:24]

    fam = collections.defaultdict(list)
    for r in unemitted:
        fam[family(r["symbol"])].append(r)

    ranked = sorted(fam.items(),
                    key=lambda kv: -(len(kv[1]) * sum(x["size"] for x in kv[1])))

    print("=" * 74)
    print("LANE AE FUNNEL — target functions with NO emittable claimant")
    print("=" * 74)
    print(f"  objs scanned                      : {nobjs}")
    print(f"  distinct symbols our build defines: {len(emitted)}")
    print(f"  target functions in report        : {total_fns}")
    print(f"  ... at 0%                         : {zero_fns}")
    print(f"  ... in scope (non-XDK/Quazal)     : {inscope}")
    print(f"  ... unmapped (fn_*, other lane)   : {unmapped_zero}")
    print(f"  ... NAMED and 0%                  : {named_zero}")
    print(f"      of which name IS emitted      : {emitted_but_zero}   (body/pairing work)")
    print(f"      of which name NOT emitted     : {len(unemitted)}   <== THIS LANE")
    print(f"  distinct families                 : {len(fam)}")
    print(f"  total bytes unclaimable           : {sum(r['size'] for r in unemitted)}")
    nfix = sum(1 for r in unemitted if r["verdict"] == "FIX-SIG")
    nswap = sum(1 for r in unemitted if r["variant_swap"])
    print(f"  ... FIX-SIG  (wrong declaration): {nfix}")
    print(f"  ... ADD-DECL (name never emitted): {len(unemitted) - nfix}")
    print(f"  ... of which wrong Ham/Band variant selected: {nswap}")
    print()
    print("TOP FAMILIES (count x bytes):")
    for name, rows in ranked[:40]:
        b = sum(x["size"] for x in rows)
        print(f"  {len(rows):4d} fns {b:7d} B  {name[:88]}")

    print()
    print("FIX-SIG rows (retail name is ground truth for the correct signature):")
    for r in sorted((x for x in unemitted if x["verdict"] == "FIX-SIG"),
                    key=lambda x: -x["size"])[:25]:
        print(f"  {r['size']:5d} {r['unit'][:32]:32s} TGT {r['symbol'][:76]}")
        for a in r["ours"]:
            print(f"  {'':38s} OUR {a[:76]}")

    if args.json:
        json.dump({
            "funnel": {
                "objs": nobjs, "emitted_symbols": len(emitted),
                "total_fns": total_fns, "zero_fns": zero_fns, "inscope": inscope,
                "unmapped_zero": unmapped_zero, "named_zero": named_zero,
                "emitted_but_zero": emitted_but_zero,
                "unemitted": len(unemitted),
            },
            "candidates": unemitted,
            "families": {k: [r["symbol"] for r in v] for k, v in ranked},
        }, open(args.json, "w"), indent=1)
        print(f"\n-> {args.json}")


if __name__ == "__main__":
    main()
