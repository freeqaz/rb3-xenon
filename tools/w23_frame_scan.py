#!/usr/bin/env python3
"""W23-FRAMESWEEP: whole-binary detector for the W22 frame-shortfall signature.

WHAT IT DETECTS
---------------
Lane W22 closed `?Handle@VocalPlayer@@` (+5,296 B, the largest single result of
2026-08-17) off one observation: our stack frame was short of retail's by
exactly 0x10, and the tell was that N EH funclets differed from retail in
*nothing but* the parent-frame displacement `subi r31, r12, <parent frame>`.
Every one of those funclets crosses for free when the parent frame is fixed
(9 x 40 B of bonus on top of Handle's 4,936 B body).

This tool finds that shape everywhere.  It is deliberately NOT an objdiff
consumer: it decodes the PowerPC prologue out of the COFF bodies of both the
dtk-split TARGET obj and our compiled BASE obj, so it runs over all ~3,091
paired units in seconds rather than 69k objdiff invocations.

    frame:   stwu r1, -N(r1)              word>>26 == 37, rS == rA == 1
    funclet: addi rD, r12, -N             word>>26 == 14, rA == 12
             (`subi rD,r12,N` is the disassembler's spelling of the same word)

FUNCLETS DO NOT PAIR BY NAME -- AND TWO OBVIOUS WAYS TO PAIR THEM ARE REFUTED
-----------------------------------------------------------------------------
In the target obj a funclet is an anonymous `fn_826E95D0`; in ours it is
`__unwind$344432`.  They never share a symbol name, so a name join reads 0.

⛔ REFUTED SPELLING 1 -- "mask the parent-frame immediate, match on the
remaining bytes".  Every funclet carries at least one relocation (the `bl` to a
dtor), and that word differs between the sides by construction: different
displacement, and often a different symbol entirely (`fn_82B69618` on the target
side against our mangled spelling).  Masking only the immediate matched nothing.

⛔ REFUTED SPELLING 2 -- "mask the immediate AND every relocated word, i.e.
objdiff's own `funclet_signature`".  Correct in principle and still reads 0,
because it was driven through `coff_bodies_ext.function_bodies_ext`, whose
`is_aux_code_symbol()` drops `__unwind$*` -- which on our side IS the funclet.
See `raw_disp_counts` below.

★ WHAT WORKS: the per-unit MULTISET OF DISPLACEMENTS, target vs base, exactly as
guard_funclet_census.py does for guard bits.  The displacement immediate *is* the
parent's frame size, so the multiset is self-keying and needs no pairing at all.
Surplus on both sides (target has N at 0x1d0 that we lack, we have N at 0x1c0
that it lacks) is the W22 corroboration -- one defect observed N extra times,
not N independent rows.

Validated against W22's measured answer: `Poll@VocalPlayer` reads 3,388 B body +
4 funclets x 40 B, and `Handle@VocalPlayer` -- fixed by W22 in this tree --
correctly reads CLEAR.  Both halves matter; a detector that only ever fires
proves nothing.

RANKING
-------
`matched_code` is ALL-OR-NOTHING per row, so rows are ranked by SIZE-IF-IT-
CROSSES = target body extent + 40 B per corroborating funclet, never by penalty
or by fuzzy%.  Sizes come from the COFF slice (the ASM EXTENT), not from
report.json, whose `size` field is a known targeting hazard.

WHAT IT CANNOT TELL YOU
-----------------------
Collectability.  A row can carry relocation-NAME charges against ICF
fold-survivor names where our source is already correct, in which case no source
change collects the bytes however good the body is (W22 sized `Poll@VocalPlayer`
as exactly that, 3,388 B unreachable).  Run tools/w23_collectable.py on the
shortlist BEFORE porting anything.
"""
import collections
import json
import os
import struct
import sys
from pathlib import Path

ROOT = Path(os.environ.get("W23_ROOT", Path(__file__).resolve().parent.parent))
sys.path.insert(0, str(ROOT / "tools"))
from coff_bodies_ext import function_bodies_ext  # noqa: E402
from icf_fold_evidence import parse_coff, IMAGE_SCN_CNT_CODE  # noqa: E402

FUNCLET_MAX = 0x100  # funclets are tiny; 40 B typical, cap generously


def _s16(v):
    return v - 0x10000 if v & 0x8000 else v


def frame_size(body):
    """Frame bytes from `stwu r1,-N(r1)`, or None.

    Scans the first 16 instructions: MSVC X360 puts `mfspr r12,lr` and
    `bl __savegprlr_NN` ahead of the stwu, so it is not at offset 0.
    """
    n = min(len(body) // 4, 16)
    for i in range(n):
        w = struct.unpack_from(">I", body, i * 4)[0]
        if w >> 26 == 37 and (w >> 21) & 31 == 1 and (w >> 16) & 31 == 1:
            d = _s16(w & 0xFFFF)
            if d < 0:
                return -d
        # stwux r1,r1,rB -- frames >= 32 KB.  Reported, never guessed at.
        if w >> 26 == 31 and (w >> 1) & 0x3FF == 183 and (w >> 21) & 31 == 1 \
                and (w >> 16) & 31 == 1:
            return "LARGE"
    return None


def parent_disp(body):
    """(word_index, displacement) of `addi rD,r12,-N` -- a funclet re-deriving
    its parent's frame pointer.  Returns None if absent or not unique."""
    hits = []
    for i in range(len(body) // 4):
        w = struct.unpack_from(">I", body, i * 4)[0]
        if w >> 26 == 14 and (w >> 16) & 31 == 12:
            d = _s16(w & 0xFFFF)
            if d < 0:
                hits.append((i, -d))
    return hits[0] if len(hits) == 1 else None


def raw_disp_counts(path):
    """Multiset of parent-frame displacements over EVERY code word in the obj.

    ⛔ THIS CANNOT GO THROUGH `coff_bodies_ext.function_bodies_ext`, AND THE
    FAILURE IS A SILENT ZERO.  On OUR side the EH cleanup funclets are emitted
    as `__unwind$NNNNNN` symbols inside the parent's own `.text` COMDAT, and
    `is_aux_code_symbol()` drops exactly that prefix by design (they are never a
    C++ callee).  A first cut of this tool paired funclets through that reader
    and reported 0 corroborating funclets for `Poll@VocalPlayer` -- a row W22
    had already MEASURED at 4.  A detector that reads zero everywhere looks
    exactly like a detector that found nothing.

    So the base side is scanned as raw code words, symbol table ignored.  The
    displacement immediate IS the parent's frame size, so the multiset is
    self-keying: no name pairing is needed or possible (target funclets are
    anonymous `fn_826E95D0`, ours are `__unwind$344432`).
    """
    data = Path(path).read_bytes()
    sections, _syms = parse_coff(data)
    cnt = collections.Counter()
    for sec in sections:
        if not (sec["chars"] & IMAGE_SCN_CNT_CODE):
            continue
        raw = sec["raw"]
        for off in range(0, len(raw) - 3, 4):
            w = struct.unpack_from(">I", raw, off)[0]
            if w >> 26 == 14 and (w >> 16) & 31 == 12:
                d = _s16(w & 0xFFFF)
                if d < 0:
                    cnt[-d] += 1
    return cnt


def target_funclet_rows(tgt):
    """Target funclet SYMBOLS keyed by parent displacement.

    These are the objdiff rows that pay 40 B each, so they are counted from real
    symbols (not raw words): a funclet is a symbol whose FIRST word re-derives
    the parent frame pointer, which is exactly how MSVC lays one out.
    """
    out = collections.defaultdict(list)
    for name, (body, _r) in tgt.items():
        if not body or len(body) > FUNCLET_MAX or len(body) < 8:
            continue
        w = struct.unpack_from(">I", body, 0)[0]
        if w >> 26 == 14 and (w >> 16) & 31 == 12:
            d = _s16(w & 0xFFFF)
            if d < 0:
                out[-d].append((name, len(body)))
    return out


def savegpr(relocs):
    for _off, name, _t in relocs:
        if "savegprlr" in name:
            return name.split("_")[-1]
    return None


def scan_unit(tpath, bpath):
    tgt = {}
    base = {}
    for store, path in ((tgt, tpath), (base, bpath)):
        try:
            for name, body, relocs, _e in function_bodies_ext(path):
                store[name] = (body, relocs)
        except Exception as e:  # noqa: BLE001 -- a malformed obj must not kill the sweep
            return None, None, str(e)
    return tgt, base, None


def funclet_corroboration(tdisp, bdisp, trows, tf, bf, fuzzy):
    """How many funclet ROWS a frame fix from `bf` to `tf` would cross.

    Target funclets sitting at `tf` that we do NOT emit at `tf`, matched against
    base funclets at `bf` that the target does NOT have there.  Both surpluses
    are required: a target funclet with no base counterpart at all is a funclet
    we simply never emit, which a frame fix does not conjure.

    Only target funclet rows currently BELOW fuzzy 100 are priced -- a row
    already matching cannot pay twice.
    """
    t_surplus = tdisp.get(tf, 0) - bdisp.get(tf, 0)
    b_surplus = bdisp.get(bf, 0) - tdisp.get(bf, 0)
    n = max(0, min(t_surplus, b_surplus))
    if not n:
        return 0, 0
    open_rows = [(nm, sz) for nm, sz in trows.get(tf, [])
                 if (fuzzy.get(nm) or 0) < 100.0]
    open_rows.sort(key=lambda x: -x[1])
    take = open_rows[:n]
    return len(take), sum(sz for _nm, sz in take)


def main():
    ap = __import__("argparse").ArgumentParser()
    ap.add_argument("--project", default=str(ROOT))
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--min-fuzzy", type=float, default=0.0,
                    help="only report primary rows at or above this report fuzzy%%")
    ap.add_argument("--only-unit", default=None)
    args = ap.parse_args()

    proj = Path(args.project)
    cfg = json.load(open(proj / "objdiff.json"))
    rep = json.load(open(proj / "build/45410914/report.json"))

    fuzzy, mpn, unit_of = {}, {}, {}
    for u in rep["units"]:
        for f in u.get("functions", []):
            fuzzy[f["name"]] = float(f.get("fuzzy_match_percent", 0) or 0)
            mpn[f["name"]] = float(f.get("match_percent_normalized", 0) or 0)
            unit_of[f["name"]] = u["name"]

    rows, unit_funclets = [], {}
    stats = collections.Counter()
    for u in cfg["units"]:
        if args.only_unit and args.only_unit not in u["name"]:
            continue
        tp, bp = proj / u["target_path"], proj / u.get("base_path", "")
        if not (tp.exists() and u.get("base_path") and bp.exists()):
            stats["unit_unpaired"] += 1
            continue
        tgt, base, err = scan_unit(tp, bp)
        if err:
            stats["unit_error"] += 1
            continue
        stats["unit_scanned"] += 1

        unit_funclets[u["name"]] = (raw_disp_counts(tp), raw_disp_counts(bp),
                                    target_funclet_rows(tgt))

        for name in set(tgt) & set(base):
            tf = frame_size(tgt[name][0])
            bf = frame_size(base[name][0])
            if tf is None or bf is None:
                stats["no_frame"] += 1
                continue
            stats["frame_compared"] += 1
            if tf == bf:
                stats["frame_equal"] += 1
                continue
            stats["frame_differ"] += 1
            rows.append({
                "unit": u["name"],
                "symbol": name,
                "tgt_frame": tf,
                "base_frame": bf,
                "delta": (tf - bf) if isinstance(tf, int) and isinstance(bf, int) else None,
                "tgt_size": len(tgt[name][0]),
                "base_size": len(base[name][0]),
                "tgt_savegpr": savegpr(tgt[name][1]),
                "base_savegpr": savegpr(base[name][1]),
                "fuzzy": fuzzy.get(name),
                "mpn": mpn.get(name),
            })

    # Attach funclet corroboration: how many funclets in this unit differ by
    # exactly this row's frame delta (target disp == tgt_frame, base == base).
    for r in rows:
        tdisp, bdisp, trows = unit_funclets.get(
            r["unit"], (collections.Counter(), collections.Counter(), {}))
        if isinstance(r["tgt_frame"], int) and isinstance(r["base_frame"], int):
            n, nb = funclet_corroboration(tdisp, bdisp, trows,
                                          r["tgt_frame"], r["base_frame"], fuzzy)
        else:
            n, nb = 0, 0
        r["funclets"] = n
        r["funclet_bytes"] = nb
        r["prize"] = r["tgt_size"] + r["funclet_bytes"]

    rows = [r for r in rows if (r["fuzzy"] or 0) >= args.min_fuzzy]
    rows.sort(key=lambda r: -r["prize"])

    print("== scan stats ==")
    for k in sorted(stats):
        print("  %-18s %d" % (k, stats[k]))
    print()
    print("%9s %8s %8s %7s %7s %5s %4s  %s"
          % ("PRIZE", "BODY", "FUNCLET", "TGT", "OURS", "FUZZY", "FCL", "SYMBOL"))
    for r in rows[:80]:
        tf = r["tgt_frame"] if not isinstance(r["tgt_frame"], int) else hex(r["tgt_frame"])
        bf = r["base_frame"] if not isinstance(r["base_frame"], int) else hex(r["base_frame"])
        print("%9d %8d %8d %7s %7s %5.1f %4d  %s"
              % (r["prize"], r["tgt_size"], r["funclet_bytes"], tf, bf,
                 r["fuzzy"] or 0, r["funclets"], r["symbol"][:70]))

    if args.json_out:
        json.dump(rows, open(args.json_out, "w"), indent=1)
        print("\nwrote %d rows -> %s" % (len(rows), args.json_out))


if __name__ == "__main__":
    main()
