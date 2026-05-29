#!/usr/bin/env python3
"""
build_symbol_map.py — Extract a high-confidence  fn_<addr> -> mangled_symbol  map
for the RB3 retail XEX, so the names can be imported into the Ghidra project
(see apply_symbols.py).

WHY
---
RB3's Ghidra project has only anonymous fn_<addr> function names. Our decomp has
matched/pinned functions whose *base* mangled symbol objdiff pairs to a *target*
function. The target function's absolute XEX virtual address is what Ghidra keys
on. This tool recovers, for each matched/partial function, the absolute address
of its target so we can rename Ghidra's fn_<addr> -> mangled symbol.

SOURCES (all read-only, all in this repo; verified 2026-05-29)
--------------------------------------------------------------
* build/45410914/obj/<unit>.obj   dtk-emitted TARGET object (XBOX360 PPC COFF,
                                  machine 0x01f2). Contains the matched functions
                                  named with their MANGLED symbol (objdiff/dtk
                                  applied source names) plus unmatched fn_<addr>,
                                  as COMDAT (.text$dup) or shared (.text) symbols.
                                  Section VirtualAddresses are zeroed by dtk, so
                                  the absolute address is NOT directly in the COFF.
* build/45410914/asm/<unit>.s     dtk's disassembly listing. Each function carries
                                  a comment `# .text:0xOFF | 0xABSADDR | size: 0xN`
                                  giving the ABSOLUTE XEX address + size, and the
                                  fn_<addr> labels. This is the authoritative
                                  absolute-address source.
* build/45410914/report.json      objdiff report -> demangled_name per symbol.
* decomp.db (read-only)           functions table -> symbol, unit, current_percent,
                                  verdict (drives which bands to include).

RESOLUTION (mangled symbol -> absolute address), in confidence order
--------------------------------------------------------------------
1. text_exact   : symbol lives in the shared `.text` section with a non-zero
                  `value` (= offset). textbase (derived from any fn_<addr> in the
                  shared `.text`, since its name IS its absolute address) + value
                  = absolute address. Exact, no inference.
2. comdat_size  : symbol is a COMDAT (.text$dup) function (value 0, own section).
                  Match its COMDAT byte-size against the `.s` functions whose
                  absolute address is not already claimed by pass 1. If exactly
                  one .s function of that size remains -> that's the address.
3. comdat_reloc : if multiple .s candidates share the size, disambiguate by the
                  COMDAT's relocation callee-symbol sequence vs the `bl <callee>`
                  sequence disassembled at each candidate address in the `.s`.
                  If exactly one candidate's callee sequence matches -> that's it.

fn_<addr> symbols carry their address in the name (100% reliable) and are included
for general Ghidra usefulness but are NOT renames (Ghidra already shows them so).

Only HIGH-confidence pairings (a unique resolved address) are emitted.

USAGE
-----
  python3 tools/ghidra/build_symbol_map.py \
      [--min-percent 80] [--out tools/ghidra/rb3_symbol_map.json]

Output JSON: { "0x82463e08": {"symbol": "?CalcScale@RndFlare@@IAAXXZ",
                              "demangled": "...", "unit": "default/Flare",
                              "percent": 98.88, "method": "comdat_size"}, ... }
"""
import argparse
import glob
import json
import os
import re
import sqlite3
import struct
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OBJ_DIR = os.path.join(REPO, "build/45410914/obj")
ASM_DIR = os.path.join(REPO, "build/45410914/asm")
REPORT = os.path.join(REPO, "build/45410914/report.json")
DECOMP_DB = os.path.join(REPO, "decomp.db")

COFF_MACHINE_XBOX360 = 0x01F2
# XBOX360 PPC COFF relocation record: vaddr(u32) + symidx(u32) + type(u16) = 10 bytes
RELOC_SIZE = 10


def _read_coff(path):
    """Return (sections, symbols) for an XBOX360 PPC COFF object, or None.

    sections: list of dict(name, rawsize, rawptr, relptr, nrel)
    symbols : list of dict(name, value, secnum (1-based), sclass)
    """
    with open(path, "rb") as f:
        d = f.read()
    if len(d) < 20:
        return None
    machine, nsec, ts, symptr, nsym, optsz, chars = struct.unpack_from("<HHIIIHH", d, 0)
    if machine != COFF_MACHINE_XBOX360:
        return None
    strtab = symptr + 18 * nsym

    def resolve_name(raw8):
        if raw8[0:1] == b"/":
            off = int(raw8.rstrip(b"\x00")[1:])
            end = d.index(b"\x00", strtab + off)
            return d[strtab + off:end].decode("latin1")
        if raw8[0:4] == b"\x00\x00\x00\x00":
            off = struct.unpack_from("<I", raw8, 4)[0]
            end = d.index(b"\x00", strtab + off)
            return d[strtab + off:end].decode("latin1")
        return raw8.rstrip(b"\x00").decode("latin1")

    sec_off = 20 + optsz
    secs = []
    for _ in range(nsec):
        raw = d[sec_off:sec_off + 8]
        name = resolve_name(raw)
        (vsize, vaddr, rawsize, rawptr, relptr, lnptr, nrel, nln) = struct.unpack_from(
            "<IIIIIIHH", d, sec_off + 8
        )
        secs.append(dict(name=name, rawsize=rawsize, rawptr=rawptr, relptr=relptr, nrel=nrel))
        sec_off += 40

    syms = []
    i = 0
    off = symptr
    while i < nsym:
        raw = d[off:off + 18]
        nm = raw[0:8]
        value, secnum, typ, sclass, naux = struct.unpack_from("<IhHBB", raw, 8)
        name = resolve_name(nm)
        syms.append(dict(name=name, value=value, secnum=secnum, sclass=sclass))
        i += 1 + naux
        off += 18 * (1 + naux)

    # second pass: resolve reloc callee names per section (need full symbol list)
    for s in secs:
        callees = []
        if s["nrel"] and s["relptr"]:
            for k in range(s["nrel"]):
                ro = s["relptr"] + k * RELOC_SIZE
                if ro + RELOC_SIZE > len(d):
                    break
                rva, symi, rtype = struct.unpack_from("<IIH", d, ro)
                if symi < len(syms):
                    callees.append(syms[symi]["name"])
        s["callees"] = callees
    return secs, syms


_S_FN_RE = re.compile(
    r"# \.text:0x([0-9A-Fa-f]+) \| 0x([0-9A-Fa-f]+) \| size: 0x([0-9A-Fa-f]+)"
)
# A dtk instruction line looks like:
#   /* 824DCBD4 004D83D4  4B F4 3F D1 */\tbl fn_8279FFC8
# Capture the absolute address (first hex word) and, for bl, the call target.
_S_INSN_RE = re.compile(r"/\* ([0-9A-Fa-f]{8}) ")
_S_CALL_RE = re.compile(r"\*/\s*bl\s+([A-Za-z_?@$.][\w?@$<>.]*)")


def _parse_s(path):
    """Parse a dtk .s listing.

    Returns list of dicts {addr, size, callees:[...]} in file order (== ascending
    address), size>0 only. callees = bl call targets within that function's body,
    assigned by absolute instruction address falling inside [addr, addr+size).
    """
    fns = []
    bls = []  # (insn_addr, callee)
    with open(path) as f:
        for line in f:
            m = _S_FN_RE.match(line)
            if m:
                addr = int(m.group(2), 16)
                size = int(m.group(3), 16)
                if size > 0:
                    fns.append(dict(addr=addr, size=size, callees=[]))
                continue
            cm = _S_CALL_RE.search(line)
            if cm:
                am = _S_INSN_RE.search(line)
                if am:
                    bls.append((int(am.group(1), 16), cm.group(1)))
    fns.sort(key=lambda f: f["addr"])
    # Assign each bl to its containing function (functions are non-overlapping).
    for ia, callee in bls:
        for fn in fns:
            if fn["addr"] <= ia < fn["addr"] + fn["size"]:
                fn["callees"].append(callee)
                break
    return fns


def _is_text(secname):
    return secname == ".text"


def _is_comdat_text(secname):
    return secname.startswith(".text$") or secname == "/4"


def resolve_unit(obj_path, s_path):
    """Resolve every mangled/fn_ primary symbol in a unit to an absolute address.

    Returns dict symbol -> (addr|None, method).
    """
    coff = _read_coff(obj_path)
    if not coff:
        return {}
    secs, syms = coff
    sfns = _parse_s(s_path)

    # Derive shared-.text base from any fn_<addr> living in the shared .text section.
    textbase = None
    for sym in syms:
        sn = sym["secnum"]
        if 1 <= sn <= len(secs) and _is_text(secs[sn - 1]["name"]) and sym["name"].startswith("fn_"):
            try:
                textbase = int(sym["name"][3:], 16) - sym["value"]
                break
            except ValueError:
                pass

    res = {}
    claimed = set()

    # Pass 1: shared .text exact (textbase + value)
    if textbase is not None:
        for sym in syms:
            sn = sym["secnum"]
            name = sym["name"]
            if not (1 <= sn <= len(secs)):
                continue
            if not _is_text(secs[sn - 1]["name"]):
                continue
            if not (name.startswith("?") or name.startswith("fn_")):
                continue
            addr = textbase + sym["value"]
            method = "name" if name.startswith("fn_") else "text_exact"
            res[name] = (addr, method)
            claimed.add(addr)

    # Build leftover .s candidates (not already claimed by pass 1), grouped by size.
    leftover = [f for f in sfns if f["addr"] not in claimed]
    by_size = {}
    for f in leftover:
        by_size.setdefault(f["size"], []).append(f)

    # Pass 2/3: COMDAT functions
    for sym in syms:
        sn = sym["secnum"]
        name = sym["name"]
        if not (1 <= sn <= len(secs)):
            continue
        sec = secs[sn - 1]
        if not _is_comdat_text(sec["name"]):
            continue
        if not (name.startswith("?") or name.startswith("fn_")):
            continue
        if name in res:
            continue
        if name.startswith("fn_"):
            try:
                res[name] = (int(name[3:], 16), "name")
            except ValueError:
                res[name] = (None, "bad_fn_name")
            continue
        size = sec["rawsize"]
        cands = by_size.get(size, [])
        if len(cands) == 1:
            res[name] = (cands[0]["addr"], "comdat_size")
        elif len(cands) > 1:
            # Disambiguate by call-target fingerprint: the COMDAT's relocation
            # callees (real function names only) vs the bl-targets disassembled
            # at each candidate address in the .s. Both sides see the same calls
            # (the obj relocates them; the .s shows them resolved), so the unique
            # candidate whose .s call-set is exactly the obj's call-set is it.
            def fn_names(seq):
                return {c for c in seq
                        if (c.startswith("?") or c.startswith("fn_"))
                        and not c.startswith("__")}
            obj_fns = fn_names(sec["callees"])
            matches = []
            for cand in cands:
                if obj_fns == fn_names(cand["callees"]):
                    matches.append(cand)
            if len(matches) == 1:
                res[name] = (matches[0]["addr"], "comdat_reloc")
            else:
                res[name] = (None, "comdat_ambiguous")
        else:
            res[name] = (None, "comdat_nocand")
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--min-percent", type=float, default=80.0,
                    help="Minimum current_percent to include (default 80).")
    ap.add_argument("--out", default=os.path.join(REPO, "tools/ghidra/rb3_symbol_map.json"))
    ap.add_argument("--include-fn", action="store_true",
                    help="Also emit fn_<addr> entries (identity; usually skipped).")
    args = ap.parse_args()

    # demangled names from report.json
    demangled = {}
    with open(REPORT) as f:
        rep = json.load(f)
    for u in rep.get("units", []):
        for fn in u.get("functions", []):
            meta = fn.get("metadata") or {}
            if meta.get("demangled_name"):
                demangled[fn["name"]] = meta["demangled_name"]

    # target set from decomp.db
    con = sqlite3.connect(f"file:{DECOMP_DB}?mode=ro", uri=True)
    rows = con.execute(
        "SELECT symbol, unit, current_percent FROM functions "
        "WHERE current_percent >= ? AND symbol NOT LIKE 'fn\\_%' ESCAPE '\\'",
        (args.min_percent,),
    ).fetchall()
    con.close()

    by_unit = {}
    pct_of = {}
    for sym, unit, pct in rows:
        if not unit:
            continue
        by_unit.setdefault(unit, []).append(sym)
        pct_of[sym] = pct

    stats = dict(text_exact=0, comdat_size=0, comdat_reloc=0,
                 comdat_ambiguous=0, comdat_nocand=0,
                 notfound=0, no_unit_files=0, dup_addr=0)
    addr_map = {}        # "0xADDR" -> entry
    sym_to_addr = {}     # symbol -> addr (dedup detection)

    for unit, syms in by_unit.items():
        base = unit.split("/")[-1]
        op = os.path.join(OBJ_DIR, base + ".obj")
        sp = os.path.join(ASM_DIR, base + ".s")
        if not (os.path.exists(op) and os.path.exists(sp)):
            stats["no_unit_files"] += len(syms)
            continue
        res = resolve_unit(op, sp)
        for s in syms:
            if s not in res:
                stats["notfound"] += 1
                continue
            addr, method = res[s]
            if addr is None:
                stats[method] = stats.get(method, 0) + 1
                continue
            if method not in ("text_exact", "comdat_size", "comdat_reloc"):
                # fn_ identity etc. — count but only emit if requested
                continue
            stats[method] += 1
            key = f"0x{addr:08x}"
            entry = dict(symbol=s, demangled=demangled.get(s),
                         unit=unit, percent=pct_of.get(s), method=method)
            if key in addr_map and addr_map[key]["symbol"] != s:
                # Two distinct symbols resolved to the same address: ICF fold.
                # Keep the higher-percent one; record collision.
                stats["dup_addr"] += 1
                existing = addr_map[key]
                if (entry["percent"] or 0) <= (existing["percent"] or 0):
                    continue
            addr_map[key] = entry
            sym_to_addr[s] = addr

    high = len(addr_map)
    with open(args.out, "w") as f:
        json.dump(addr_map, f, indent=1, sort_keys=True)

    print(f"target symbols (>= {args.min_percent}%, non-fn_): {len(pct_of)}")
    print("resolution breakdown:")
    for k in ("text_exact", "comdat_size", "comdat_reloc",
              "comdat_ambiguous", "comdat_nocand", "notfound",
              "no_unit_files", "dup_addr"):
        print(f"  {k:18}: {stats.get(k,0)}")
    # band split
    matched = sum(1 for e in addr_map.values() if (e["percent"] or 0) >= 99.9)
    partial = high - matched
    print(f"HIGH-CONFIDENCE mapped: {high}  (matched>=99.9%: {matched}, partial[{args.min_percent},99.9): {partial})")
    skipped = stats["comdat_ambiguous"] + stats["comdat_nocand"] + stats["notfound"]
    print(f"skipped as low-confidence/unresolvable: {skipped}")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
