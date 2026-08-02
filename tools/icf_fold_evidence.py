#!/usr/bin/env python3
"""Derive PROVEN ICF fold classes from EVIDENCE, and emit alias-group candidates.

Two independent evidence sources, per the CB-11/A brief:

 (1) DIRECT -- our own compiled objects.  Run the real identical-COMDAT-folding
     algorithm over every function COMDAT in ``build/45410914/src/**/*.obj``:
     two bodies fold iff their machine bytes are equal once relocated fields are
     masked AND their relocations agree in (offset, type, target-equivalence-class).
     Target classes are refined ITERATIVELY to a fixpoint, which is exactly what a
     real ICF pass does (a fold can enable another fold).  This is evidence at OUR
     compiler (X360 cl 11886) on OUR source.

 (2) TRANSFER -- dc3's leaked linker map (``ham_xbox_r.map``).  Rows sharing an
     ADDRESS are a proven fold in a shipped binary built from the same Milo engine
     with the same ``/O1 /Oi /GR /EHsc`` flags.  dc3 is NECESSARY-NOT-SUFFICIENT
     evidence for RB3 (different game, older retail compiler build 10224 vs 11886),
     but it is decisive in the NEGATIVE direction: two names at DIFFERENT dc3
     addresses were NOT folded there, so their bodies differ, so RB3 would not fold
     them either.  We treat that as a REFUTATION.

Outputs a candidate-group JSON with per-group evidence provenance, group size, and
body size, so the caller can apply a size / information-content cap.

Read-only: mutates no build input.
"""

import argparse
import collections
import glob
import json
import re
import struct
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DC3_MAP = Path("/home/free/code/milohax/dc3-decomp/orig/373307D9/ham_xbox_r.map")
TARGET_MAP = PROJECT_ROOT / "scripts" / "target_symbol_map.json"

IMAGE_SCN_LNK_NRELOC_OVFL = 0x01000000
IMAGE_SCN_CNT_CODE = 0x00000020


# ---------------------------------------------------------------------------
# COFF
# ---------------------------------------------------------------------------
def parse_coff(data: bytes):
    """Return (sections, symbols).

    sections: list of dicts {name, size, raw, relocs:[(off, symidx, type)], chars}
    symbols:  list of dicts {name, value, section, type, storage, aux}
    """
    if len(data) < 20:
        return [], []
    nsec, sym_off, nsym = struct.unpack_from("<HxxxxII", data, 2)
    str_start = sym_off + nsym * 18

    def sname(nb):
        if nb[:4] == b"\x00\x00\x00\x00":
            so = struct.unpack_from("<I", nb, 4)[0]
            ao = str_start + so
            if ao >= len(data):
                return ""
            end = data.index(b"\x00", ao)
            return data[ao:end].decode("ascii", "replace")
        return nb.split(b"\x00")[0].decode("ascii", "replace")

    sections = []
    for i in range(nsec):
        o = 20 + i * 40
        if o + 40 > len(data):
            break
        name = sname(data[o:o + 8])
        vsize, vaddr, rawsize, rawptr, relptr, _lptr, nrel, _nl, chars = \
            struct.unpack_from("<IIIIIIHHI", data, o + 8)
        if (chars & IMAGE_SCN_LNK_NRELOC_OVFL) and nrel == 0xFFFF:
            nrel = struct.unpack_from("<I", data, relptr)[0] - 1
            relptr += 10
        relocs = []
        for r in range(nrel):
            ro = relptr + r * 10
            if ro + 10 > len(data):
                break
            va, si, ty = struct.unpack_from("<IIH", data, ro)
            relocs.append((va, si, ty))
        raw = data[rawptr:rawptr + rawsize] if rawptr else b""
        sections.append({"name": name, "size": rawsize, "raw": raw,
                         "relocs": relocs, "chars": chars})

    symbols = []
    i = 0
    while i < nsym:
        eo = sym_off + i * 18
        if eo + 18 > len(data):
            break
        name = sname(data[eo:eo + 8])
        value, secnum, styp, sclass, naux = struct.unpack_from("<IhHBB", data, eo + 8)
        symbols.append({"name": name, "value": value, "section": secnum,
                        "type": styp, "storage": sclass, "aux": naux, "idx": i})
        for _ in range(naux):
            symbols.append(None)
        i += 1 + naux
    return sections, symbols


def function_bodies(path: Path):
    """Yield (symbol_name, body_bytes, reloc_list) for each function COMDAT.

    /Gy is on, so each function is its own code section; we take every code
    section that has exactly one external/static function symbol at offset 0.

    ⚠⚠ THIS SILENTLY UNDERCOUNTS -- DO NOT USE AS A SUPPLY-SYMBOL ENUMERATOR.
    The `value == 0` requirement below drops every EH-BEARING function. MSVC
    emits an 8-byte EH prefix at the START of the COMDAT, so the function symbol
    sits at **value 8**, `defs` comes back empty, and the section is skipped with
    no warning. Measured on BandPatchMesh.obj (lane DB-4c, 2026-08-02): **26 of
    433 code sections dropped**, and the dropped set includes real targets of
    interest -- DA-2's own specimens `PropSync<RndMesh>` and
    `PatchPair::PatchPair` among them.

    Same root cause as the pin-boundary straddle sized in
    tools/eh_prefix_straddle_census.py: the prefix precedes the entry, so any
    "the function starts here" assumption is wrong for EH functions.

    For enumeration use a next-function-symbol extent model instead (lane DB-4c
    built one at /home/free/tmp/laneDB4c/sizer.py). The `value == 0` filter is
    left AS IS deliberately: this helper has other consumers and changing its
    population silently would be the exact defect class it just caused. Fixing
    it needs its own lane with a consumer audit.
    """
    data = path.read_bytes()
    sections, symbols = parse_coff(data)
    if not sections:
        return
    # symbol index -> name (for reloc targets)
    idx_name = {}
    by_sec = collections.defaultdict(list)
    for s in symbols:
        if s is None:
            continue
        idx_name[s["idx"]] = s["name"]
        if s["section"] > 0:
            by_sec[s["section"] - 1].append(s)
    for si, sec in enumerate(sections):
        if not (sec["chars"] & IMAGE_SCN_CNT_CODE):
            continue
        # candidate defining symbols at offset 0, function type, not the section symbol
        defs = [s for s in by_sec.get(si, [])
                if s["value"] == 0 and s["name"] != sec["name"]
                and s["storage"] in (2, 3) and s["type"] == 0x20]
        if len(defs) != 1:
            continue
        yield defs[0]["name"], sec["raw"], [(o, idx_name.get(i, "?"), t)
                                            for (o, i, t) in sec["relocs"]]


def masked_body(raw: bytes, relocs) -> bytes:
    b = bytearray(raw)
    for off, _n, _t in relocs:
        if off + 4 <= len(b):
            b[off:off + 4] = b"\x00\x00\x00\x00"
    return bytes(b)


def icf_classes(bodies):
    """bodies: {name: (masked_bytes, [(off, target_name, type), ...])}

    Iteratively refine equivalence classes exactly as a linker's ICF does.
    Returns {name: class_id}.
    """
    # initial class: masked bytes + reloc (offset, type) shape
    def shape(rec):
        _mb, rl = rec
        return (_mb, tuple((o, t) for o, _n, t in rl))

    cls = {}
    for n, rec in bodies.items():
        cls[n] = shape(rec)
    # normalize to ints
    ids = {}
    cur = {n: ids.setdefault(k, len(ids)) for n, k in cls.items()}
    for _ in range(12):
        nxt_key = {}
        for n, rec in bodies.items():
            _mb, rl = rec
            # a reloc target that is itself a function we compiled contributes its
            # class; anything else (data, external) contributes its literal name
            tgt = tuple(cur[tn] if tn in cur else ("N", tn) for _o, tn, _t in rl)
            nxt_key[n] = (cur[n], tgt)
        ids2 = {}
        nxt = {n: ids2.setdefault(k, len(ids2)) for n, k in nxt_key.items()}
        if len(ids2) == len(set(cur.values())) and \
           all(_same_partition(cur, nxt)):
            cur = nxt
            break
        cur = nxt
    return cur


def _same_partition(a, b):
    ga = collections.defaultdict(set)
    gb = collections.defaultdict(set)
    for n, c in a.items():
        ga[c].add(n)
    for n, c in b.items():
        gb[c].add(n)
    sa = {frozenset(v) for v in ga.values()}
    sb = {frozenset(v) for v in gb.values()}
    yield sa == sb


# ---------------------------------------------------------------------------
# dc3 map
# ---------------------------------------------------------------------------
DC3_RX = re.compile(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+(\S+)\s+([0-9a-fA-F]{8})\s+(.*)$')


def dc3_addr_of():
    """name -> address (str). Names appearing at >1 address are dropped as ambiguous."""
    seen = collections.defaultdict(set)
    for line in DC3_MAP.open(errors="replace"):
        m = DC3_RX.match(line)
        if m:
            seen[m.group(3)].add(m.group(4))
    return {n: next(iter(a)) for n, a in seen.items() if len(a) == 1}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=str(Path.home() / "tmp" / "cb11A_evidence.json"))
    args = ap.parse_args()

    print("[1/4] parsing our compiled objs ...", file=sys.stderr)
    bodies = {}
    dupes = set()
    objs = glob.glob(str(PROJECT_ROOT / "build/45410914/src/**/*.obj"), recursive=True)
    for p in objs:
        for name, raw, relocs in function_bodies(Path(p)):
            rec = (masked_body(raw, relocs), relocs)
            if name in bodies and bodies[name][0] != rec[0]:
                dupes.add(name)
            bodies.setdefault(name, rec)
    print(f"      {len(objs)} objs, {len(bodies)} function COMDATs "
          f"({len(dupes)} name collisions w/ differing bytes)", file=sys.stderr)

    print("[2/4] running ICF fixpoint ...", file=sys.stderr)
    cls = icf_classes(bodies)
    groups = collections.defaultdict(list)
    for n, c in cls.items():
        groups[c].append(n)
    ours = {c: sorted(v) for c, v in groups.items() if len(v) > 1}
    print(f"      {len(ours)} our-compiler fold classes covering "
          f"{sum(len(v) for v in ours.values())} symbols", file=sys.stderr)

    print("[3/4] parsing dc3 map ...", file=sys.stderr)
    dc3 = dc3_addr_of()
    print(f"      {len(dc3)} unambiguous dc3 names", file=sys.stderr)

    out = {
        "our_classes": [{"members": v,
                         "body_size": len(bodies[v[0]][0]),
                         "n_relocs": len(bodies[v[0]][1])}
                        for v in ours.values()],
        "dc3_addr": dc3,
    }
    Path(args.out).write_text(json.dumps(out))
    print(f"[4/4] wrote {args.out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
