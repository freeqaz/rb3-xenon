#!/usr/bin/env python3
"""build_data_symbol_map.py -- DATA names the repo already knows, for Ghidra (phase 2 of
docs/plans/ghidra-data-sync-2026-11-22.md). Output {"0x82...": {"symbol", "kind", "source"}}.

Sources, in priority order (a later source never overrides an earlier one at the same VA):
  1. scripts/target_symbol_map.json rows whose VA is not code (strings ??_C@, __real@, vtables, RTTI,
     .data objects) -- the map is authoritative where it speaks.
  2. Retail RTTI decoded from orig/45410914/band.exe with tools/retail_rtti.py:
       TypeDescriptor  ??_R0<.?AVName@@ minus the dot>@8
       COL             ??_R4Class@@6B@            (offset-0 vftable)  /  ??_R4Class@@6B<Base>@ (secondary)
       vftable         ??_7Class@@6B@             /  ??_7Class@@6B<Base>@
     <Base> for a secondary vftable is the first non-virtual base, in the hierarchy's preorder, whose
     member displacement equals the COL offset. That suffix is a heuristic; the map-vs-RTTI agreement
     on rows both name is printed as this phase's naming control.
  3. Named non-placeholder objects in config/45410914/symbols.txt (only __imp_ import thunks survive the
     filter: except_record_/except_data_/pdata@ are dtk address-derived names, not knowledge).
"""
import argparse
import json
import os
import re
import struct
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, REPO)
from tools.retail_rtti import RetailRtti  # noqa: E402


def cls_of(tdname):
    # '.?AVFoo@Hmx@@' -> 'Foo@Hmx@@'
    return tdname[4:]


def simple_frags(qual):
    """Identifier fragments of a plain qualified name 'Foo@Hmx@@' -> ['Foo', 'Hmx'].
    Template names ('?$...') are not decomposed: they get no backrefs (heuristic, measured by the
    map-agreement control)."""
    if "?$" in qual:
        return []
    return [f for f in qual.rstrip("@").split("@") if f]


def backref(qual, frags):
    """Re-spell a base's qualified name with MSVC name backrefs against the enclosing name."""
    if "?$" in qual:
        return qual
    parts = [f for f in qual.rstrip("@").split("@") if f]
    out = []
    table = list(frags)
    for f in parts:
        if f in table[:10]:
            out.append(str(table.index(f)))
        else:
            out.append(f + "@")
            table.append(f)
    return "".join(out) + "@"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", default=os.path.join(REPO, "scripts/target_symbol_map.json"))
    ap.add_argument("--symbols", default=os.path.join(REPO, "config/45410914/symbols.txt"))
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    R = RetailRtti()
    text = next(s for s in R.sections if s.name == ".text")
    # word index: value -> [VA of aligned word holding it]
    idx = {}
    for s in R.sections:
        if not s.has_data or s.name in (".text", ".pdata", ".reloc"):
            continue
        blk = R.data[s.rawptr:s.rawptr + s.rawsize]
        for i, (w,) in enumerate(struct.iter_unpack(">I", blk[:len(blk) // 4 * 4])):
            if 0x82000000 <= w < 0x83000000:
                idx.setdefault(w, []).append(s.va + 4 * i)

    out = {}

    def put(va, sym, kind, source):
        k = "0x%08x" % va
        if k not in out:
            out[k] = {"symbol": sym, "kind": kind, "source": source}

    # 1. map data rows
    m = json.load(open(args.map))
    map_data = {}
    for k, v in m.items():
        if not (k.lower().startswith("0x") and isinstance(v, str) and v):
            continue
        va = int(k, 16)
        if text.va <= va < text.va + text.vsize:
            continue
        map_data[va] = v
        kind = ("vftable" if v.startswith("??_7") else "rtti" if v.startswith("??_R")
                else "string" if v.startswith("??_C@") else "real" if v.startswith("__real@") else "data")
        put(va, v, kind, "target_symbol_map")

    # 2. RTTI
    rtti = {}
    tds = []
    for pat in (b".?AV", b".?AU"):
        for name_va in R.find_bytes(pat):
            n = R.cstr(name_va)
            if n and n.endswith("@@") and re.match(r"^\.\?A[VU][A-Za-z0-9_@?$]+$", n):
                tds.append((name_va - 8, n))
    n_td = n_col = n_vt = 0
    cols_of = {}
    for td_va, n in tds:
        rtti[td_va] = ("??_R0" + n[1:] + "@8", "rtti_td")
        n_td += 1
        for ref in idx.get(td_va, []):
            r = R.bases_of_col(ref - 12)
            if r and r[0].ptd == td_va:
                cols_of.setdefault(td_va, []).append(r)
    for td_va, n in tds:
        cols = cols_of.get(td_va, [])
        cls = cls_of(n)
        frags = simple_frags(cls)
        for c, chd, bases in cols:
            if len(cols) == 1 and c.offset == 0:
                suffix = "6B@"
            else:
                # First base (preorder) at this displacement. Measured 2026-11-22 against the 64 map
                # rows that also name an RTTI vftable/COL: this rule agrees on 61; "deepest base on
                # the chain" agrees on 60 (it fixes CharUpperTwist->RndPollable but breaks two
                # Object@Hmx->ObjRef). The map wins wherever it names the address.
                base = next((b.name for b in bases[1:] if b.mdisp == c.offset and b.pdisp == -1
                             and b.name), None)
                if base is None:
                    if c.offset != 0:
                        continue
                    suffix = "6B0@@"       # the class's own vfptr: backref to its own name
                else:
                    suffix = "6B" + backref(cls_of(base), frags) + "@"
            rtti[c.va] = ("??_R4" + cls + suffix, "rtti_col")
            n_col += 1
            for slot in idx.get(c.va, []):
                rtti[slot + 4] = ("??_7" + cls + suffix, "vftable")
                n_vt += 1
    agree = differ = 0
    diffs = []
    for va, (sym, kind) in rtti.items():
        if va in map_data:
            if map_data[va] == sym:
                agree += 1
            else:
                differ += 1
                diffs.append(("0x%08x" % va, map_data[va], sym))
    for va, (sym, kind) in sorted(rtti.items()):
        put(va, sym, kind, "retail_rtti")

    # 3. symbols.txt __imp_
    n_imp = 0
    for line in open(args.symbols):
        mm = re.match(r"(__imp_\S+) = \.\w+:0x([0-9A-Fa-f]+);", line)
        if mm:
            put(int(mm.group(2), 16), mm.group(1), "import", "symbols.txt")
            n_imp += 1

    json.dump(out, open(args.out, "w"), indent=0)
    print("[build_data_symbol_map] map_data=%d rtti: td=%d col=%d vftable=%d (map agrees %d / differs %d) "
          "imports=%d -> %d entries" % (len(map_data), n_td, n_col, n_vt, agree, differ, n_imp, len(out)))
    for d in diffs[:15]:
        print("  DIFF %s map=%s rtti=%s" % d)
    return 0


if __name__ == "__main__":
    sys.exit(main())
