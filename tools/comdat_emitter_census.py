#!/usr/bin/env python3
"""Which of OUR compiled objects EMITS each symbol hosted by a pinned .text block?

WHY THIS EXISTS (lane W15-F sec5, lane W16-E).  Many `.text` blocks host COMDAT
content: `DECLARE_MESSAGE`'s `static Symbol Type()`, inline ctors, STL template
members.  A COMDAT is emitted by EVERY TU that uses it and reduced by the linker
to ONE arbitrary survivor, so "which retail TU owned it" is NOT a question with
an answer, and dc3's leaked map answers it only for DC3's link order.

The answerable question -- and the one that decides PAIRABILITY -- is which of
OUR objects defines the symbol.  objdiff pairs target<->base BY NAME, so a target
row whose base obj cannot define that name reads 0% however correct our code is.

MOVE RULE: re-home a block only when exactly one of our objects emits EVERY
symbol the block hosts AND that object is not the pinned unit.  Everything else
stays: a plural emitter set that includes the pinned unit already pairs, and a
symbol NO object emits cannot be helped by any pin move (it needs a TU that
instantiates it).

★ SELF-VALIDATION.  The census is only trustworthy because it DISCRIMINATES.
Ordinary single-owner blocks must resolve to exactly one object that IS the
pinned unit (FileCache, UsbMidiGuitar, System_Xbox, UsbMidiKeyboard all do).  A
census that answered "one emitter" -- or "many" -- everywhere would confirm
whatever it was pointed at.  Check those controls before believing a verdict.

⚠ A DEFINITION is SectionNumber > 0.  A symbol merely referenced has section 0
(UNDEF) and does NOT make the object an emitter.

⚠ Run a FULL `./tools/ninja-locked` first.  A fresh worktree's reflinked target
objs are PRE-RENAMER, so every mangled-name lookup reads "absent" and every
negative is vacuous.

Usage:
    python3 tools/comdat_emitter_census.py --lo 0x8251A000 --hi 0x8251E100
"""
import argparse, json, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
from extract_decomp_symbols import (  # noqa: E402
    read_coff_header, read_section_headers, read_string_table, read_symbols)

BUILD = ROOT / "build" / "45410914"


def text_blocks(lo, hi):
    """(start, end, pinned_unit) for every .text split overlapping [lo, hi)."""
    out, unit = [], None
    for line in (ROOT / "config/45410914/splits.txt").read_text().splitlines():
        st = line.strip()
        if st and not line.startswith((" ", "\t")) and st.endswith(":"):
            unit = st[:-1]
        elif st.startswith(".text") and unit:
            kv = dict(p.split(":", 1) for p in st.split() if ":" in p)
            if "start" in kv and "end" in kv:
                s, e = int(kv["start"], 16), int(kv["end"], 16)
                if s < hi and e > lo:
                    out.append((s, e, unit))
    return sorted(out)


def defined_symbols():
    """symbol name -> [obj relpath] for every DEFINITION in our compiled objs."""
    defs = {}
    objs = sorted((BUILD / "src").rglob("*.obj"))
    for o in objs:
        data = o.read_bytes()
        h = read_coff_header(data)
        if not h:
            continue
        secs = read_section_headers(data, h["num_sections"])
        strt = read_string_table(data, h["symbol_table_offset"], h["num_symbols"])
        for sym in read_symbols(data, h, secs, strt):
            if sym["section_number"] > 0:          # >0 == DEFINITION, not a reference
                defs.setdefault(sym["name"], []).append(str(o.relative_to(BUILD / "src")))
    return len(objs), defs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lo", default="0x8251A000")
    ap.add_argument("--hi", default="0x8251E100")
    a = ap.parse_args()
    lo, hi = int(a.lo, 16), int(a.hi, 16)

    tmap = json.load(open(ROOT / "scripts/target_symbol_map.json"))
    named = sorted((int(k, 16), v) for k, v in tmap.items()
                   if k.startswith("0x") and isinstance(v, str))
    nobj, defs = defined_symbols()
    print(f"indexed {nobj} compiled objects, {len(defs)} defined symbol names\n")

    for s, e, unit in text_blocks(lo, hi):
        hosted = [(addr, n) for addr, n in named if s <= addr < e]
        print(f"0x{s:08X}-0x{e:08X}  {e - s:5d} B  pinned={unit}")
        for addr, n in hosted:
            emitters = defs.get(n, [])
            pinned_emits = any(Path(p).stem == Path(unit).stem for p in emitters)
            if not emitters:
                verdict = "NONE -- unpairable by absence; no pin move can help"
            elif len(emitters) == 1 and not pinned_emits:
                verdict = f"MOVE CANDIDATE -> {emitters[0]}"
            else:
                verdict = ("pinned unit emits it; keep" if pinned_emits
                           else f"{len(emitters)} emitters: " + ", ".join(emitters))
            print(f"      0x{addr:08X}  {n[:76]}")
            print(f"                  emitted by: {emitters or 'NONE'}")
            print(f"                  => {verdict}")
        if not hosted:
            print("      (no named rows)")
        print()


if __name__ == "__main__":
    main()
