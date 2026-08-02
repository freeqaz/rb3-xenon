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
import os
import re
import struct
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools"))   # for the deferred coff_bodies_ext
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


_LEGACY_WARNED = False


def _warn_legacy(path):
    """One-time loud stderr notice. The whole defect class here is SILENCE, so a
    caller that lands on the frozen reader by accident must be told."""
    global _LEGACY_WARNED
    _LEGACY_WARNED = True
    print("⚠⚠ icf_fold_evidence.function_bodies() is the FROZEN LEGACY reader and "
          "DROPS EVERY EH-BEARING FUNCTION (78,190 of 95,247 symbols tree-wide; "
          "17,057 missing). First seen on %s. Use "
          "coff_bodies_ext.function_bodies_ext, or pass legacy_ok=True if the "
          "legacy population is genuinely what you want." % path, file=sys.stderr)


def function_bodies(path: Path, legacy_ok: bool = False):
    """⛔ FROZEN LEGACY READER -- DO NOT USE IN NEW CODE. Use
    ``coff_bodies_ext.function_bodies_ext`` (or, from here, ``main``'s reader
    switch), which is the corrected population.

    Yields (symbol_name, body_bytes, reloc_list) for each function COMDAT.
    /Gy is on, so each function is its own code section; this takes every code
    section that has exactly one external/static function symbol at offset 0.

    ⚠⚠ IT SILENTLY UNDERCOUNTS -- NEVER USE IT AS A SUPPLY-SYMBOL ENUMERATOR.
    BOTH halves of the gate below misfire on every EH-bearing function:
      * MSVC emits an 8-byte EH prefix at the START of the COMDAT, so the
        function symbol sits at **value 8** and the ``value == 0`` test fails;
      * ``__unwind$NNNNN`` is ALSO storage 2 / type 0x20, so ``len(defs) == 2``
        and the section is dropped entirely.
    Measured on BandPatchMesh.obj (lane DB-4c): 26 of 433 code sections dropped.
    Measured tree-wide (lane CW-2): 31,075 two-def and 8,849 three-def sections
    dropped. Measured tree-wide again (lane DC-4, 2026-08-02, 1,099 of our objs):
    this reader yields **78,190** function symbols where the corrected reader
    yields **95,247** -- it is missing **17,057 (21.8%)**.

    Same root cause as the pin-boundary straddle sized in
    tools/eh_prefix_straddle_census.py: the prefix precedes the entry, so any
    "the function starts here" assumption is wrong for EH functions.

    ★ WHY THIS IS FROZEN RATHER THAN FIXED IN PLACE (lane DC-4 consumer audit).
    Two shipped tools document an env-var hatch that promises to RE-DERIVE a
    specific published number from this exact population:
        tools/icf_site_census.py   ICF_CENSUS_LEGACY_READER=1  (CY-1's column 2)
        tools/xbin_adjudicate.py   CW2_LEGACY_READER=1         (CW-2's pre-fix leg)
    Silently changing the body of this function would turn both hatches into a
    THIRD, unnamed population while still claiming to reproduce the old numbers.
    That is precisely the defect family this reader already caused twice, so the
    population here is deliberately preserved BYTE-FOR-BYTE and every consumer
    was migrated instead. Callers that genuinely want the legacy population must
    say so with ``legacy_ok=True``; everyone else gets a one-time loud warning
    on stderr, because the failure mode of this reader is SILENCE.
    """
    if not legacy_ok and not _LEGACY_WARNED:
        _warn_legacy(path)
    path = Path(path)          # DC-4: accept str like function_bodies_ext does
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

    # ★ DC-4 (2026-08-02): the CORRECTED reader is the default. The frozen legacy
    # `function_bodies` dropped every EH-bearing function, so this generator saw
    # 78,190 of 95,247 function COMDATs (17,057 / 21.8% missing) and its fold
    # classes were computed over a population with a systematic hole.
    # ICF_FOLD_LEGACY_READER=1 re-derives any pre-DC-4 number. ⚠ DEPENDENT
    # NUMBERS MOVE -- see the reader-move table printed below.
    #
    # THE OBJECTION THAT HAD TO BE CLEARED FIRST, because this generator feeds
    # scripts/symbol_aliases.json which IS a build input (tools/
    # gen_symbol_alias_map.py, build.ninja): real MSVC /OPT:ICF folds COMDATs
    # identical INCLUDING the associated .xdata, but the ext reader discards the
    # __unwind$/__ehhandler$ slice. An unwind-BLIND fold test could therefore
    # assert folds the linker would refuse -- the fold-MANUFACTURING direction.
    # MEASURED over all 1,099 of our objs: of the 4,112 ext fold groups, 211
    # contain >=1 aux-bearing symbol and ZERO split when the unwind signature is
    # added to the key => 0 manufactured pairs. The test is not vacuous: under a
    # random reassignment of the same multiset of unwind signatures it splits
    # 1,383-1,430 groups / ~7.0M pairs across three seeds. Unwind descriptors are
    # perfectly correlated inside fold classes (identical bodies modulo
    # relocations have identical prologues), so the blind reader is SAFE HERE.
    use_legacy = os.environ.get("ICF_FOLD_LEGACY_READER") == "1"
    if use_legacy:
        print("[1/4] parsing our compiled objs (⚠ LEGACY reader) ...", file=sys.stderr)
        reader = lambda p: function_bodies(Path(p), legacy_ok=True)   # noqa: E731
    else:
        from coff_bodies_ext import function_bodies_ext               # deferred: cycle
        print("[1/4] parsing our compiled objs (corrected reader) ...", file=sys.stderr)
        reader = lambda p: ((n, r, rl)                                 # noqa: E731
                            for n, r, rl, _e in function_bodies_ext(Path(p)))
    bodies = {}
    dupes = set()
    # DC-4: sorted() is LOAD-BEARING, not tidiness. 364 symbol names collide
    # across objs with DIFFERING bytes and `setdefault` keeps the FIRST, so an
    # unsorted glob makes the fold-class count filesystem-order dependent.
    # Measured: 4,109 classes unsorted vs 4,112 sorted on the same tree.
    objs = sorted(glob.glob(str(PROJECT_ROOT / "build/45410914/src/**/*.obj"), recursive=True))
    for p in objs:
        for name, raw, relocs in reader(p):
            rec = (masked_body(raw, relocs), relocs)
            if name in bodies and bodies[name][0] != rec[0]:
                dupes.add(name)
            bodies.setdefault(name, rec)
    print(f"      {len(objs)} objs, {len(bodies)} function COMDATs "
          f"({len(dupes)} name collisions w/ differing bytes)", file=sys.stderr)
    # Population guard. The failure mode being guarded is a COLLAPSED read that
    # reports zeros shaped exactly like "nothing to fold" -- the decisive-negative
    # shape that closes veins (cf. f592571a / lane CZ-3). A tree with 1,099 objs
    # cannot legitimately yield a handful of COMDATs.
    if objs and len(bodies) < 0.5 * len(objs):
        sys.exit("REFUSING: read %d function COMDATs from %d objs -- the COFF read "
                 "collapsed. Do not read the fold counts below as a result."
                 % (len(bodies), len(objs)))

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
