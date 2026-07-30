#!/usr/bin/env python3
"""Sibling-transposition discriminator (lane BZ-3).

THE DEFECT
----------
STL container COMDATs come in families of same-size, near-identical bodies that
differ ONLY in the element-size shift field (`slwi`/`srawi`/`rlwinm` SH).
objdiff's PPC arch renders that field via `Argument::OpaqueU` ->
`InstructionPart::opaque` (arch/ppc/mod.rs:173), so `is_immediate` is FALSE
(diff/code.rs:1152) and the penalty is folded into `arg_diff_score` -- i.e. it
is normalized AWAY.  Consequence: a map row can name the WRONG sibling of a
container family and still score `match_percent_normalized == 100.0`, which is
what `matched_functions` credits.

So the defect is invisible on the function axis but costs the whole body on the
code axis, because `matched_code` only counts `match_percent == 100.0`
(objdiff-cli/src/cmd/report.rs:825).  Repointing such a row is therefore
**Δ0 on matched_functions and +size on matched_code** -- verified exactly.

DIRECTION OF THE FIX
--------------------
The shift amount is NOT a `sizeof` oracle for correcting our headers.  In every
resolvable case in the norm-100/raw<100 population OUR size was right and the
MAP was wrong.  Three cases prove it at the language level -- a
`vector<T*>` cannot have 2-byte elements, a `vector<unsigned short>` cannot
have 4-byte elements, a `vector<pair<T*,float>>` cannot have 64-byte elements.
Treat a shift mismatch as a SIBLING DISCRIMINATOR, not a layout bug.

★ ANTI-VACUITY GUARD (load-bearing)
-----------------------------------
Masked byte-compare must refuse to adjudicate when the union of both sides'
relocated words covers the body.  Without the guard a 16B vbase-adjustor thunk
"matched" `?FastCos@@YAMM@Z` because all four words were masked and the
comparison was vacuously true.  Small bodies are where this bites; the guard
(>=4 compared words AND >=50% of the body) removed 5 of 8 spurious hits.

Usage:
  python3 scripts/harvest/sibling_shift_disc.py                 # scan + report
  python3 scripts/harvest/sibling_shift_disc.py --json out.json
"""
import argparse
import collections
import json
import os
import struct
import sys

PROJ = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


# --------------------------------------------------------------------------
# COFF reader -- resolves each symbol's OWN section (never "first .text";
# some target objs, e.g. BandCamShot.obj, carry hundreds of sections).
# --------------------------------------------------------------------------
def read_coff(path):
    d = open(path, "rb").read()
    mach, nsec, ts, psym, nsym, osz, ch = struct.unpack_from("<HHIIIHH", d, 0)
    if psym == 0 or nsym == 0:
        raise ValueError("no symbol table")
    strtab = psym + nsym * 18
    secs = []
    for s in range(nsec):
        off = 20 + osz + s * 40
        vsz, va, rawsz, rawptr = struct.unpack_from("<IIII", d, off + 8)
        prel = struct.unpack_from("<I", d, off + 24)[0]
        nreloc = struct.unpack_from("<H", d, off + 32)[0]
        flags = struct.unpack_from("<I", d, off + 36)[0]
        secs.append(dict(rawptr=rawptr, rawsz=rawsz, flags=flags,
                         nreloc=nreloc, prel=prel))
    syms = []
    i = 0
    while i < nsym:
        off = psym + i * 18
        raw = d[off:off + 8]
        if raw[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", raw, 4)[0]
            e = d.index(b"\0", strtab + so)
            name = d[strtab + so:e].decode("latin1")
        else:
            name = raw.rstrip(b"\0").decode("latin1")
        val, secn, typ, sc, naux = struct.unpack_from("<IhHBB", d, off + 8)
        i += 1 + naux
        if secn <= 0 or secn > len(secs):
            continue
        s = secs[secn - 1]
        if not (s["flags"] & 0x20) or s["rawptr"] == 0:
            continue
        syms.append(dict(name=name, val=val, s=s))
    return d, syms


def body_and_mask(d, s, val, size):
    base = s["rawptr"] + val
    b = d[base:base + size]
    relocated = set()
    for r in range(s["nreloc"]):
        off = s["prel"] + r * 10
        va, symidx, ty = struct.unpack_from("<IIH", d, off)
        w = va - val
        if 0 <= w < size:
            relocated.add(w // 4)
    return b, relocated


def masked_eq(b1, m1, b2, m2, min_words=4, min_frac=0.5):
    """Masked equality WITH the anti-vacuity guard.  See module docstring."""
    if len(b1) != len(b2) or not b1:
        return False
    mask = m1 | m2
    nwords = len(b1) // 4
    compared = 0
    for k in range(0, len(b1) - 3, 4):
        if k // 4 in mask:
            continue
        compared += 1
        if b1[k:k + 4] != b2[k:k + 4]:
            return False
    return compared >= min_words and compared >= min_frac * nwords


# --------------------------------------------------------------------------
def population(report):
    """Functions credited by matched_functions (norm==100) that nevertheless
    carry a real raw diff (fuzzy<100).  report.json's per-function
    `fuzzy_match_percent` is the RAW match_percent (confusing name)."""
    out = []
    for u in report["units"]:
        for f in u.get("functions", []):
            n = f.get("match_percent_normalized")
            if n == 100.0 and f.get("fuzzy_match_percent", 100.0) < 100.0:
                out.append((u["name"], f["name"]))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", default=PROJ)
    ap.add_argument("--json")
    a = ap.parse_args()
    os.chdir(a.project)

    report = json.load(open("build/45410914/report.json"))
    units = {u["name"]: u for u in json.load(open("objdiff.json"))["units"]}
    pop = population(report)

    cache = {}
    def load(p):
        if p not in cache:
            try:
                cache[p] = read_coff(p)
            except Exception:
                cache[p] = None
        return cache[p]

    stat = collections.Counter()
    unique, ambiguous = [], []
    for unit, sym in pop:
        u = units.get(unit)
        if not u or not u.get("base_path") or not u.get("target_path"):
            stat["no_paths"] += 1
            continue
        T, B = load(u["target_path"]), load(u["base_path"])
        if T is None or B is None:
            stat["unreadable"] += 1
            continue
        td, tsyms = T
        bd, bsyms = B
        tgt = [x for x in tsyms if x["name"] == sym]
        base = [x for x in bsyms if x["name"] == sym]
        if not tgt:
            stat["target_symbol_missing"] += 1
            continue
        if not base:
            # objdiff paired this by its fuzzy byte-fallback, not by the map --
            # there is no map row to repoint.
            stat["anon_byte_fallback(no map row)"] += 1
            continue
        t = tgt[0]
        tb, tm = body_and_mask(td, t["s"], t["val"], t["s"]["rawsz"] - t["val"])
        b0 = base[0]
        bb, bm = body_and_mask(bd, b0["s"], b0["val"], b0["s"]["rawsz"] - b0["val"])
        if masked_eq(tb, tm, bb, bm):
            stat["named_body_already_equal"] += 1
            continue
        hits = set()
        for c in bsyms:
            if c["name"] == sym or c["name"].startswith("."):
                continue  # skip self and COFF section symbols
            csz = c["s"]["rawsz"] - c["val"]
            if csz != len(tb):
                continue
            cb, cm = body_and_mask(bd, c["s"], c["val"], csz)
            if masked_eq(tb, tm, cb, cm):
                hits.add(c["name"])
        hits = sorted(hits)
        if len(hits) == 1:
            stat["UNIQUE_repoint"] += 1
            unique.append(dict(unit=unit, current=sym, true=hits[0], size=len(tb)))
        elif hits:
            stat["ambiguous(ICF fold class)"] += 1
            ambiguous.append(dict(unit=unit, current=sym, cands=hits, size=len(tb)))
        else:
            stat["no_sibling(real body divergence)"] += 1

    print(f"=== norm-100 / raw<100 population: {len(pop)} ===")
    for k, v in stat.most_common():
        print(f"  {k:36s} {v:5d}")
    print(f"\n=== UNIQUE repoints ({len(unique)}) "
          f"-- each worth +size bytes of matched_code, Δ0 matched_functions ===")
    tot = 0
    for r in unique:
        tot += r["size"]
        print(f"  {r['unit']:26s} {r['size']:4d}B")
        print(f"     current: {r['current'][:104]}")
        print(f"     true   : {r['true'][:104]}")
    print(f"\n  recoverable matched_code: {tot} bytes")
    print(f"\n=== AMBIGUOUS ({len(ambiguous)}) -- every candidate is byte-exact, so "
          f"the metric CANNOT adjudicate correctness; do not pick for the metric ===")
    for r in ambiguous:
        print(f"  {r['unit']:26s} {r['size']:4d}B  {len(r['cands'])} candidates  "
              f"{r['current'][:60]}")

    if a.json:
        json.dump(dict(unique=unique, ambiguous=ambiguous, stat=dict(stat)),
                  open(a.json, "w"), indent=1)


if __name__ == "__main__":
    main()
