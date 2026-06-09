#!/usr/bin/env python3
"""Cross-binary COFF content-matcher: identify RB3 functions by DC3 name.

Why
---
DC3 is the byte-faithful twin (same Milo engine, same /O1 /Oi /GR /EHsc
compiler). Where RB3's engine source is identical to DC3's, the *instruction
stream* is identical modulo relocation operands (which differ because the two
binaries lay out code/data at different addresses). So: mask the reloc operands
(the COFF reloc table tells us exactly where) and hash the rest. A DC3 function
and an RB3 function with the same masked hash are the SAME function -> transfer
DC3's mangled name onto RB3's anonymous fn_<addr>.

This is more precise than BinDiff (structural/fuzzy) for the identical-source
case, and it pre-confirms which functions are byte-identical (= instant objdiff
matches once pinned+named).

Inputs
------
RB3 .text: build/45410914/obj/auto_03_*_text.obj  (whole .text, per-fn COMDAT
           sections, already renamed where known). RB3 addr recovered from the
           fn_<addr> symbol, or reverse target_symbol_map for already-renamed.
DC3 .text: ../dc3-decomp/build/373307D9/obj/*.obj  (matched, mangled names).

Output
------
JSON list of {rb3_addr, dc3_name, dc3_obj, size, masked_sha, n_rb3_collisions,
n_dc3_collisions}. Only 1:1 unambiguous masked-hash matches are emitted by
default (a hash that is unique on BOTH sides), which is the high-confidence set.
"""
import argparse
import glob
import hashlib
import json
import os
import re
import struct
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dc3_obj_source import DC3_OBJ_DIR, iter_dc3_objs

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RB3_OBJ_GLOB = os.path.join(ROOT, "build", "45410914", "obj", "auto_03_*_text.obj")
# DC3_OBJ_DIR now comes from the shared dc3_obj_source module: the retail-DC3
# TARGET tree (.dc3_text_scratch/named/obj), the byte-faithful oracle. Previously
# this defaulted to dc3-decomp's COMPILED port (build/373307D9/obj), which
# diverges from retail in unmatched units and carries ICF merged_* names — the
# root cause of the global_fuzzy_pairs vs dc3_content_match disagreement.
TSM = os.path.join(ROOT, "scripts", "target_symbol_map.json")


def read_coff_functions(path, mask=True):
    """Yield (sym_name, code_bytes, masked_hash, size) per code COMDAT section."""
    data = open(path, "rb").read()
    if len(data) < 20:
        return
    machine, nsec, ts, symptr, nsym, opt, chars = struct.unpack_from("<HHIIIHH", data, 0)
    if machine != 0x01F2:
        return
    strtab_off = symptr + nsym * 18
    def read_str_at(off):
        end = data.index(b"\x00", strtab_off + off)
        return data[strtab_off + off:end].decode("latin1")
    def sym_name(raw):
        if raw[0:4] == b"\x00\x00\x00\x00":
            return read_str_at(struct.unpack_from("<I", raw, 4)[0])
        return raw.rstrip(b"\x00").decode("latin1")

    # sections
    secs = []
    for i in range(nsec):
        o = 20 + i * 40
        name, vsz, vaddr, rawsz, rawptr, relptr, lnptr, nrel, nln, sc = \
            struct.unpack_from("<8sIIIIIIHHI", data, o)
        if name[0:1] == b"/":
            nm = read_str_at(int(name.rstrip(b"\x00")[1:]))
        else:
            nm = name.rstrip(b"\x00").decode("latin1")
        secs.append((nm, rawsz, rawptr, relptr, nrel, sc))

    # first defining symbol per section number (val==0, function class)
    sec_sym = {}
    i = 0
    while i < nsym:
        o = symptr + i * 18
        nm_raw = data[o:o + 8]
        val, sec, typ, cls, naux = struct.unpack_from("<IhHBB", data, o + 8)
        if sec > 0 and cls in (2, 6) and val == 0 and sec not in sec_sym:
            sec_sym[sec] = sym_name(nm_raw)
        i += 1 + naux

    for idx, (nm, rawsz, rawptr, relptr, nrel, sc) in enumerate(secs, start=1):
        if not nm.startswith(".text"):
            continue
        if rawsz == 0 or idx not in sec_sym:
            continue
        code = bytearray(data[rawptr:rawptr + rawsz])
        if mask and relptr and nrel:
            for r in range(nrel):
                ro = relptr + r * 10
                if ro + 10 > len(data):
                    break
                rva, symidx, rtype = struct.unpack_from("<IIH", data, ro)
                # zero the 4-byte instruction word at the reloc site (symmetric
                # across binaries because identical source => same reloc offsets)
                if rva + 4 <= len(code):
                    code[rva:rva + 4] = b"\x00\x00\x00\x00"
        h = hashlib.sha1(bytes(code)).hexdigest()
        yield sec_sym[idx], bytes(code), h, rawsz


def rb3_addr_of(name, rev_tsm):
    m = re.match(r"fn_([0-9A-Fa-f]+)$", name)
    if m:
        return int(m.group(1), 16)
    a = rev_tsm.get(name)
    return a


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rb3-glob", default=RB3_OBJ_GLOB)
    ap.add_argument("--dc3-dir", default=DC3_OBJ_DIR)
    ap.add_argument("--tsm", default=TSM)
    ap.add_argument("--out", default=os.path.join(ROOT, "dc3_content_match.json"))
    ap.add_argument("--min-size", type=int, default=16,
                    help="ignore tiny functions (<N bytes); collide too easily. "
                         "Tested 8: net-negative (tiny trivial/unwind bodies collide "
                         "and poison real matches' uniqueness). 16 is the sweet spot.")
    ap.add_argument("--validate", action="store_true",
                    help="cross-check against unified_id.json confident matches")
    args = ap.parse_args()

    tsm = json.load(open(args.tsm))
    rev_tsm = {}
    for k, v in tsm.items():
        if k.lower().startswith("0x"):
            rev_tsm[v] = int(k, 16)

    # RB3: masked_hash -> {addr: size}  (dedup by address; the auto-obj set can
    # parse the same function twice from overlapping chunks)
    rb3_by_hash = defaultdict(dict)
    rb3_files = sorted(glob.glob(args.rb3_glob))
    for f in rb3_files:
        for name, code, h, sz in read_coff_functions(f):
            if sz < args.min_size:
                continue
            a = rb3_addr_of(name, rev_tsm)
            if a is None:
                continue
            rb3_by_hash[h][a] = sz
    print(f"RB3: {len(rb3_files)} objs, {sum(len(v) for v in rb3_by_hash.values())} "
          f"fns >= {args.min_size}B, {len(rb3_by_hash)} distinct masked hashes",
          file=sys.stderr)

    # DC3: masked_hash -> {name: (obj, size)}  (dedup by name)
    dc3_by_hash = defaultdict(dict)
    dc3_files = iter_dc3_objs(args.dc3_dir)
    for f in dc3_files:
        obj = os.path.basename(f)
        for name, code, h, sz in read_coff_functions(f):
            if sz < args.min_size:
                continue
            if name.startswith("fn_") or name.startswith("sub_") or name.startswith("FUN_"):
                continue
            dc3_by_hash[h][name] = (obj, sz)
    print(f"DC3: {len(dc3_files)} objs, {sum(len(v) for v in dc3_by_hash.values())} "
          f"named fns >= {args.min_size}B, {len(dc3_by_hash)} distinct masked hashes",
          file=sys.stderr)

    # 1:1 unambiguous matches (hash maps to exactly one addr AND one name)
    matches = []
    ambiguous = 0
    for h, rb3map in rb3_by_hash.items():
        dc3map = dc3_by_hash.get(h)
        if not dc3map:
            continue
        if len(rb3map) == 1 and len(dc3map) == 1:
            addr, sz = next(iter(rb3map.items()))
            name, (obj, dsz) = next(iter(dc3map.items()))
            matches.append({
                "rb3_addr": "0x%08X" % addr,
                "dc3_name": name,
                "dc3_obj": obj,
                "size": sz,
                "masked_sha": h,
            })
        else:
            ambiguous += 1
    matches.sort(key=lambda m: m["rb3_addr"])
    json.dump(matches, open(args.out, "w"), indent=1)
    print(f"\n1:1 content matches: {len(matches)}   (ambiguous hashes: {ambiguous})")
    print(f"wrote {args.out}")

    # per-dc3-obj coverage
    byobj = defaultdict(int)
    for m in matches:
        byobj[m["dc3_obj"]] += 1
    top = sorted(byobj.items(), key=lambda x: -x[1])[:15]
    print("top DC3 objs by matched fns:")
    for o, n in top:
        print(f"  {o:40s} {n}")

    if args.validate:
        uni = json.load(open(os.path.join(ROOT, "unified_id.json")))
        uni_by_addr = {e["rb3_addr"].lower(): e for e in uni
                       if "bindiff" in (e.get("source") or "")
                       and e.get("confidence", 0) >= 0.95}
        agree = disagree = novel = 0
        for m in matches:
            a = m["rb3_addr"].lower()
            e = uni_by_addr.get(a)
            if not e:
                novel += 1
            elif e["dc3_name"] == m["dc3_name"]:
                agree += 1
            else:
                disagree += 1
        print(f"\nvalidate vs unified_id (conf>=0.95): agree={agree} "
              f"disagree={disagree} novel(not in unified)={novel}")


if __name__ == "__main__":
    main()
