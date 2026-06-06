#!/usr/bin/env python3
"""Symbol-map "reveal" sweep.

Find unmapped target ``fn_<addr>`` whose normalized bytes equal a not-yet-matched
real symbol in OUR compiled obj for the SAME pinned unit, and emit
``target_symbol_map.json`` candidate entries that reveal the (already byte-exact)
match to objdiff.

Why this exists (the lever, see project_tu_to_100_sweep.md): objdiff pairs
target<->base strictly by NAME. A target ``fn_<addr>`` with no entry in
scripts/target_symbol_map.json stays anonymous, so objdiff cannot pair it to our
compiled mangled symbol and reports it 0% -- EVEN WHEN our compiled object emits
byte-identical code. Naming it (addr -> mangled) reveals the match. This is
SELF-VALIDATING: a wrong address cannot produce a byte-exact (word_eq==1.0)
normalized match, so any entry that the build confirms at 100% is correct.

This is the GAP left by the content-match pipeline (tools/dc3_content_match.py,
game_content_match.py, fuzzy_content_match.py, global_fuzzy_index.py), which all
match against the DC3 / rb3-Wii ORACLE binaries. The reveal case compares OUR
freshly-compiled base obj vs the retail target, per pinned unit -- no oracle. It
reuses fuzzy_content_match's COFF reader + normalized word-equality primitive and
objdiff.json's authoritative per-unit target<->base pairing.

Pipeline (the candidates are NOT trusted until a build confirms them):
  tools/reveal_sweep.py --out reveal_candidates.json --emit-fragment reveal_frag.json
  tools/safe_name_merge.py --gate reveal_frag.json --out reveal_safe.json   # ICF/collision/non-real gate
  # in a worktree: merge reveal_safe.json into scripts/target_symbol_map.json,
  #   rm build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml && ninja
  # keep ONLY entries whose function lands at 100% in the rebuilt report.json.
"""
import argparse
import json
import os
import re
import sys
from collections import defaultdict

import struct  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fuzzy_content_match import read_coff_functions, word_eq_frac  # noqa: E402


def read_coff_base(path, include_static=False):
    """Like fuzzy_content_match.read_coff_functions, but the base side can also
    include STATIC functions (COFF storage class 3). read_coff_functions only
    yields externals (class 2/6); static C functions (e.g. oggvorbis sort32a,
    _ilog) are skipped, so they can never be revealed. Section symbols (also
    class 3, names starting with '.') are excluded; we prefer the first
    function-named defining symbol per section."""
    if not include_static:
        yield from read_coff_functions(path)
        return
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

    secs = []
    for i in range(nsec):
        o = 20 + i * 40
        name, vsz, vaddr, rawsz, rawptr, relptr, lnptr, nrel, nln, sc = \
            struct.unpack_from("<8sIIIIIIHHI", data, o)
        if name[0:1] == b"/":
            nm = read_str_at(int(name.rstrip(b"\x00")[1:]))
        else:
            nm = name.rstrip(b"\x00").decode("latin1")
        secs.append((nm, rawsz, rawptr, relptr, nrel))
    sec_sym = {}
    i = 0
    while i < nsym:
        o = symptr + i * 18
        nm_raw = data[o:o + 8]
        val, sec, typ, cls, naux = struct.unpack_from("<IhHBB", data, o + 8)
        # externals (2/6) and statics (3); val==0 => defining symbol at section
        # start; skip section symbols (name starts with '.') and aux'd records.
        if sec > 0 and cls in (2, 3, 6) and val == 0 and sec not in sec_sym:
            nm = sym_name(nm_raw)
            if not nm.startswith("."):
                sec_sym[sec] = nm
        i += 1 + naux
    for idx, (nm, rawsz, rawptr, relptr, nrel) in enumerate(secs, start=1):
        if not nm.startswith(".text") or rawsz == 0 or idx not in sec_sym:
            continue
        code = bytes(data[rawptr:rawptr + rawsz])
        reloff = set()
        if relptr and nrel:
            for r in range(nrel):
                ro = relptr + r * 10
                if ro + 10 > len(data):
                    break
                rva, symidx, rtype = struct.unpack_from("<IIH", data, ro)
                if rva % 4 == 0 and rva + 4 <= rawsz:
                    reloff.add(rva)
        yield {"name": sec_sym[idx], "code": code, "reloc": reloff, "size": rawsz}

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TSM = os.path.join(ROOT, "scripts", "target_symbol_map.json")
OBJDIFF = os.path.join(ROOT, "objdiff.json")
REPORT = os.path.join(ROOT, "build", "45410914", "report.json")

ADDR_RE = re.compile(r"^(?:fn|lbl)_([0-9A-Fa-f]{6,8})$")

# Base symbols whose name has no stable cross-compile identity. Mirrors
# safe_name_merge.NON_REAL_PREFIXES so we never propose one (naming a funclet
# un-pairs objdiff's address-matched funclets -- the BandDirector -16 footgun).
NON_REAL = (
    "__unwind$", "__catch$", "__ehhandler$", "__sep$", "__GSHandler",
    "__safe_se", "__tls", "fn_", "lbl_", "jumptable_", "$LN", "__real@",
    "__xmm@", "??_C@",
)


def is_non_real(name):
    return name.startswith(NON_REAL)


def norm_addr(v):
    return "0x%08X" % (v if isinstance(v, int) else int(str(v), 16))


def load_units():
    """objdiff.json units with both an existing target obj and base obj."""
    o = json.load(open(OBJDIFF))
    out = []
    for u in o["units"]:
        tp = u.get("target_path")
        bp = u.get("base_path")
        if not tp or not bp:
            continue
        tpa = os.path.join(ROOT, tp)
        bpa = os.path.join(ROOT, bp)
        if not (os.path.isfile(tpa) and os.path.isfile(bpa)):
            continue
        src = (u.get("metadata") or {}).get("source_path") or u["name"]
        out.append((u["name"], tpa, bpa, src))
    return out


def units_done_names():
    """{unit_name: set(symbol names already at 100%)} from report.json, to skip
    base symbols that are already paired+matched (purely an optimization; the
    gate's name_collision_tsm would reject them anyway)."""
    done = defaultdict(set)
    if not os.path.isfile(REPORT):
        return done
    for u in json.load(open(REPORT))["units"]:
        for f in u.get("functions") or []:
            if f.get("match_percent_normalized", 0) >= 100.0:
                done[u["name"]].add(f["name"])
    return done


# Precision floors. Tiny and/or relocation-dominated functions collide under
# normalized byte-equality (e.g. $4... vtable adjustor thunks are all
# `addi r3,r3,-N; b <masked>` -> byte-identical once the branch reloc is masked),
# so word_eq==1.0 does NOT establish identity for them. Require both a minimum
# size and a minimum count of NON-relocation instruction words (real, identity-
# bearing content), and only accept UNIQUE 1:1 matches within the unit.
MIN_SIZE = 0x18          # >= 6 instructions
MIN_REAL_WORDS = 5       # >= 5 non-masked instruction words distinguish it


def sweep(limit_units=None, verbose=False, min_size=MIN_SIZE,
          min_real=MIN_REAL_WORDS, allow_ambiguous=False, include_static=False):
    tsm = json.load(open(TSM))
    mapped = {k.lower() for k in tsm if str(k).lower().startswith("0x")}
    done = units_done_names()

    candidates = []          # {rb3_addr, mangled_name, unit, size}
    stats = defaultdict(int)
    ambiguous = 0

    units = load_units()
    if limit_units:
        units = [u for u in units if any(g in u[0] for g in limit_units)]

    for uname, tpath, bpath, src in units:
        stats["units"] += 1
        # Target pool: unmapped fn_/lbl_ functions with enough real content.
        tgt = []
        for f in read_coff_functions(tpath):
            m = ADDR_RE.match(f["name"])
            if not m or f["size"] < min_size:
                continue
            if (f["size"] // 4) - len(f["reloc"]) < min_real:
                stats["skip_reloc_dominated"] += 1
                continue
            addr = norm_addr(int(m.group(1), 16))
            if addr.lower() in mapped:
                continue
            tgt.append((addr, f))
        if not tgt:
            continue
        # Base pool: real symbols not already matched at 100% in this unit.
        done_here = done.get(uname, set())
        base_by_size = defaultdict(list)
        for f in read_coff_base(bpath, include_static):
            n = f["name"]
            if is_non_real(n) or n in done_here or f["size"] < min_size:
                continue
            base_by_size[f["size"]].append(f)
        if not base_by_size:
            continue
        # Collect all word_eq==1.0 edges, then keep only UNIQUE 1:1 (a target
        # addr matching exactly one base name AND that base name matching exactly
        # one target addr in this unit). Drop ambiguous/ICF-collided edges:
        # identity is undetermined by bytes alone -> leave them for the
        # content-match pipeline / hand work.
        edges = []  # (addr, name)
        for addr, tf in tgt:
            for bf in base_by_size.get(tf["size"], ()):
                if word_eq_frac(tf["code"], tf["reloc"], bf["code"], bf["reloc"]) == 1.0:
                    edges.append((addr, bf["name"], tf["size"]))
        addr_deg = defaultdict(set)
        name_deg = defaultdict(set)
        for addr, name, _ in edges:
            addr_deg[addr].add(name)
            name_deg[name].add(addr)
        for addr, name, size in edges:
            if len(addr_deg[addr]) == 1 and len(name_deg[name]) == 1:
                candidates.append({
                    "rb3_addr": addr, "mangled_name": name,
                    "unit": src, "size": size,
                })
                stats["candidates"] += 1
            else:
                ambiguous += 1
                if verbose and len(addr_deg[addr]) > 1:
                    print(f"  AMBIG {addr} {uname}: {sorted(addr_deg[addr])[:6]}"
                          f"{'...' if len(addr_deg[addr]) > 6 else ''}",
                          file=sys.stderr)
        # ambiguous counted per-edge; collapse to per-addr for the stat
    # dedupe candidates (an addr can appear once; safety)
    seen = set()
    uniq = []
    for c in candidates:
        if c["rb3_addr"] in seen:
            continue
        seen.add(c["rb3_addr"])
        uniq.append(c)
    return uniq, stats, ambiguous


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default="reveal_candidates.json",
                    help="full candidate report (list of dicts)")
    ap.add_argument("--emit-fragment", metavar="FILE",
                    help="also write a {addr:name} list for safe_name_merge --gate "
                         "(same as --out shape; load_candidates accepts it)")
    ap.add_argument("--units", help="comma list of unit-name substrings to limit to")
    ap.add_argument("--include-static", action="store_true",
                    help="also reveal STATIC base functions (COFF class 3), e.g. "
                         "oggvorbis/zlib static C helpers skipped by default")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    limit = args.units.split(",") if args.units else None
    cands, stats, ambiguous = sweep(limit, args.verbose,
                                    include_static=args.include_static)

    json.dump(cands, open(args.out, "w"), indent=1)
    if args.emit_fragment:
        json.dump(cands, open(args.emit_fragment, "w"), indent=1)

    print(f"[reveal] units scanned     : {stats['units']}", file=sys.stderr)
    print(f"[reveal] candidates emitted: {stats['candidates']}", file=sys.stderr)
    print(f"[reveal] ambiguous (>1 name, took first): {ambiguous}", file=sys.stderr)
    print(f"[reveal] wrote {args.out}"
          + (f" + {args.emit_fragment}" if args.emit_fragment else ""),
          file=sys.stderr)
    # quick by-unit top counts
    byu = defaultdict(int)
    for c in cands:
        byu[c["unit"]] += 1
    top = sorted(byu.items(), key=lambda x: -x[1])[:15]
    if top:
        print("[reveal] top units by candidate count:", file=sys.stderr)
        for u, n in top:
            print(f"         {n:4d}  {u}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
