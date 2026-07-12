#!/usr/bin/env python3
"""Build the base(TU0) -> TU5 function VA map for every NAMED base function.

Two-stage method (proven on the 7-address spike + 25-fn sample):

  STAGE 1  reloc-normalized opcode-skeleton EXACT match. A TU0 function's masked
           body (bl/bc targets + D-form imm16 masked out) that occurs EXACTLY
           ONCE in the TU5 .text masked stream == HIGH-confidence 1:1 remap
           (body_identical). These dense uniques are the anchors.

  STAGE 2  contiguity CO-WALK. Functions keep their .text emission order across
           TU0->TU5, so from any anchor the neighbouring function's TU5 VA is
           anchor_tu5 + (neighbour_base - anchor_base); we VERIFY that predicted
           slot's masked skeleton before accepting (safety net). This walks
           through fn_ funcs too, resolving clusters of identical-skeleton
           getters by ordinal/position, and NATURALLY breaks at genuinely
           changed functions (verify fails) -> those are the changed worklist.

Both PEs are read section-mapped (tu5_va.load_sections); flat 0x3000+VA is WRONG
on the TU5 "basic"-format image. Named universe = target_symbol_map.json joined
with symbols.txt sizes. The co-walk uses the FULL symbols.txt function list
(named + fn_) for contiguity, but only named functions are emitted in the map.

Outputs:
  _tu5probe/tu5_migrate/base_to_tu5_map.json
  _tu5probe/tu5_migrate/tu5_changed_worklist.json
  _tu5probe/map.json
"""
import json
import os
import re
import struct
import sys
from collections import Counter

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__))))
from tu5_va import load_sections  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TU0_PE = os.path.join(ROOT, "orig", "45410914", "band.exe")
TU5_PE = os.path.join(ROOT, "orig", "45410914", "band_tu5.exe")
SYMS = os.path.join(ROOT, "config", "45410914", "symbols.txt")
TSM = os.path.join(ROOT, "scripts", "target_symbol_map.json")
OUT_MAP = os.path.join(ROOT, "_tu5probe", "tu5_migrate", "base_to_tu5_map.json")
OUT_WORK = os.path.join(ROOT, "_tu5probe", "tu5_migrate", "tu5_changed_worklist.json")
OUT_CP = os.path.join(ROOT, "_tu5probe", "map.json")

DFORM = {3, 7, 8, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29,
         32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
         48, 49, 50, 51, 52, 53, 54, 55}


def mask_word(w):
    op = (w >> 26) & 0x3F
    if op == 18:
        return w & 0xFC000003
    if op == 16:
        return w & 0xFFFF0003
    if op in DFORM:
        return w & 0xFFFF0000
    return w


def masked_text_stream(data, secs):
    for name, sva, vsize, rawptr, rawsize in secs:
        if name == ".text":
            n = min(vsize, rawsize)
            n -= n % 4
            out = bytearray(n)
            for i in range(0, n, 4):
                struct.pack_into(">I", out, i,
                                 mask_word(struct.unpack_from(">I", data, rawptr + i)[0]))
            return sva, bytes(out)
    raise SystemExit("no .text")


def load_funcs():
    """All type:function symbols in .text -> sorted [(va,size)]; plus va->size."""
    rx = re.compile(r"^\S+ = \.text:0x([0-9A-Fa-f]+);.*type:function"
                    r"(?:.*size:0x([0-9A-Fa-f]+))?")
    funcs = []
    for line in open(SYMS):
        m = rx.match(line)
        if m:
            va = int(m.group(1), 16)
            size = int(m.group(2), 16) if m.group(2) else 0
            funcs.append((va, size))
    funcs.sort()
    return funcs


def main():
    d0, b0, s0 = load_sections(TU0_PE)
    d5, b5, s5 = load_sections(TU5_PE)
    t0_va, t0_mask = masked_text_stream(d0, s0)
    t5_va, t5_mask = masked_text_stream(d5, s5)
    t0_end = t0_va + len(t0_mask)
    t5_len = len(t5_mask)
    print(f"TU0 .text {t0_va:#x}..{t0_end:#x} ({len(t0_mask)} B)")
    print(f"TU5 .text {t5_va:#x}..{t5_va+t5_len:#x} ({t5_len} B)")

    funcs = load_funcs()
    N = len(funcs)
    va_index = {va: i for i, (va, _) in enumerate(funcs)}
    print(f"full .text functions: {N}")

    tsm = json.load(open(TSM))
    named = {}  # va -> mangled
    for vhex, mangled in tsm.items():
        if vhex.lower().startswith("0x"):
            named[int(vhex, 16)] = mangled

    def needle(va, size):
        if size < 4 or va < t0_va or va + size > t0_end:
            return None
        off = va - t0_va
        return t0_mask[off:off + size]

    def find_hits(nd, cap=3):
        hits, start = [], 0
        while len(hits) < cap:
            idx = t5_mask.find(nd, start)
            if idx < 0:
                break
            if idx % 4 == 0:
                hits.append(t5_va + idx)
            start = idx + 4
        return hits

    def verify(tu5, nd):
        off = tu5 - t5_va
        return 0 <= off and off + len(nd) <= t5_len and t5_mask[off:off + len(nd)] == nd

    # ---- STAGE 1: unique-skeleton anchors over the FULL function list ----
    MINI = 0x14
    tu5_of = {}      # base_va -> tu5_va
    unique = set()   # base_va matched uniquely (HIGH)
    for va, size in funcs:
        nd = needle(va, size)
        if nd is None or size < MINI:
            continue
        hits = find_hits(nd, cap=2)
        if len(hits) == 1:
            tu5_of[va] = hits[0]
            unique.add(va)
    print(f"stage1 unique anchors: {len(unique)}")

    # ---- STAGE 2: contiguity co-walk (forward + backward, iterate) ----
    ndc = {}  # cache needles

    def nd_of(va, size):
        if va not in ndc:
            ndc[va] = needle(va, size)
        return ndc[va]

    for _pass in range(8):
        changed = 0
        for i in range(1, N):
            va, size = funcs[i]
            if va in tu5_of:
                continue
            pva, _ = funcs[i - 1]
            if pva in tu5_of:
                nd = nd_of(va, size)
                if nd is not None:
                    cand = tu5_of[pva] + (va - pva)
                    if verify(cand, nd):
                        tu5_of[va] = cand
                        changed += 1
        for i in range(N - 2, -1, -1):
            va, size = funcs[i]
            if va in tu5_of:
                continue
            nva, _ = funcs[i + 1]
            if nva in tu5_of:
                nd = nd_of(va, size)
                if nd is not None:
                    cand = tu5_of[nva] - (nva - va)
                    if verify(cand, nd):
                        tu5_of[va] = cand
                        changed += 1
        print(f"  co-walk pass {_pass}: +{changed} (total {len(tu5_of)})")
        if changed == 0:
            break

    # ---- classify NAMED functions ----
    recs = []
    for va in sorted(named):
        mangled = named[va]
        i = va_index.get(va)
        size = funcs[i][1] if i is not None else 0
        rec = {"base_va": f"0x{va:08x}", "symbol": mangled, "size": size,
               "tu5_va": None, "confidence": None, "method": None,
               "body_identical": False}
        if i is None:
            rec["confidence"] = "SKIP"
            rec["method"] = "not-in-symbols-text"
        elif va in tu5_of:
            rec["tu5_va"] = f"0x{tu5_of[va]:08x}"
            rec["body_identical"] = True
            if va in unique:
                rec["confidence"] = "HIGH"
                rec["method"] = "skeleton-unique"
            else:
                rec["confidence"] = "MED"
                rec["method"] = "cowalk-verified"
        else:
            nd = nd_of(va, size)
            present = nd is not None and t5_mask.find(nd) >= 0
            rec["confidence"] = "AMBIG" if present else "MISS"
            rec["method"] = ("skeleton-present-unplaced" if present
                             else "skeleton-changed")
        recs.append(rec)

    conf_c = Counter(r["confidence"] for r in recs)
    total = len(recs)
    skipped = conf_c.get("SKIP", 0)
    func_total = total - skipped            # actual .text functions (drop data syms)
    matched = sum(1 for r in recs if r["tu5_va"])
    changed = [r for r in recs if r["confidence"] in ("MISS", "AMBIG")]
    match_pct = round(100.0 * matched / func_total, 3)
    meta = {"total_named_symbols": total, "non_function_data_symbols": skipped,
            "func_total": func_total, "matched": matched, "changed": len(changed),
            "match_pct_over_functions": match_pct, "by_confidence": dict(conf_c),
            "high": conf_c.get("HIGH", 0), "med": conf_c.get("MED", 0),
            "ambig": conf_c.get("AMBIG", 0), "miss": conf_c.get("MISS", 0),
            "stage1_unique_anchors_fulltext": len(unique)}
    json.dump({"meta": meta, "functions": recs}, open(OUT_MAP, "w"), indent=1)
    # FULL base->TU5 remap over ALL .text functions (named + fn_), for P2-P5
    # re-anchoring of splits.txt / symbols.txt / decomp.db fn_ VAs.
    OUT_FULL = os.path.join(ROOT, "_tu5probe", "tu5_migrate", "base_to_tu5_map.full.json")
    full = {f"0x{va:08x}": f"0x{tu5_of[va]:08x}" for va, _ in funcs if va in tu5_of}
    json.dump({"meta": {"total_text_funcs": N, "resolved": len(full),
                        "resolved_pct": round(100.0 * len(full) / N, 3),
                        "unique_anchors": len(unique)},
               "map": full}, open(OUT_FULL, "w"))
    print(f"full remap: {len(full)}/{N} .text funcs ({round(100.0*len(full)/N,2)}%) -> {OUT_FULL}")
    work = [{"symbol": r["symbol"], "base_va": r["base_va"], "size": r["size"],
             "why_unmatched": r["confidence"], "method": r["method"]}
            for r in changed]
    json.dump({"count": len(work), "worklist": work}, open(OUT_WORK, "w"), indent=1)
    json.dump(meta, open(OUT_CP, "w"), indent=1)

    print("\n=== AGGREGATE (named functions) ===")
    print(f"named symbols       : {total}  (incl {skipped} non-func data syms)")
    print(f"named .text funcs    : {func_total}")
    print(f"matched (HIGH+MED)   : {matched}  ({match_pct}% of functions)")
    for k in ("HIGH", "MED", "AMBIG", "MISS", "SKIP"):
        print(f"  {k:6}: {conf_c.get(k,0)}")
    print(f"changed-set (AMBIG+MISS) : {len(changed)}")
    print(f"wrote {OUT_MAP}")
    print(f"      {OUT_WORK}")
    print(f"      {OUT_CP}")


if __name__ == "__main__":
    main()
