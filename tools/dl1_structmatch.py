#!/usr/bin/env python3
"""Lane DL-1: does STRUCTURAL matching RB3<->DC3 work at all?

Runs directly on the two retail PEs -- orig/45410914/band.exe (RB3, TU5) and
dc3-decomp/orig/373307D9/ham_xbox_r.exe (DC3) -- so it is immune to the staleness
that killed tools/bindiff_match.json (a TU0-era artifact: 84.9% of its rb3_addrs
are function starts in the TU0 binary, only 3.1% in the current one).

No Ghidra, no BinDiff, no import.  BinDiff is one IMPLEMENTATION of structural
matching; the question is whether the INFORMATION is there.

INSTRUMENT
  extents   .pdata RUNTIME_FUNCTION [BeginAddress][PrologLen:8|FuncLen:22|..]
            VALIDATED: 21,376/21,377 sizes agree with report.json (100.0%).
  tokens    per 4-byte PPC instruction, with cross-binary noise masked by
            OPCODE CLASS (symmetric on both sides):
              op 18 (b/bl)      -> displacement MASKED, keep AA/LK
              op 16 (bc)        -> displacement MASKED, keep BO/BI/AA/LK
              op 14,15,24,25,26,27,28,29 (addi/addis/ori/.../andis)
                                -> 16-bit immediate MASKED (address formation)
              everything else   -> opcode + register fields + D-form offset KEPT
                                   (struct offsets are real cross-binary signal)
            LIMITATION: .text base-relocations are not consulted (the PE .reloc
            parse returned 0 entries and was not debugged), so masking is by
            opcode class, not by true relocation.  This over-masks real
            immediates and under-masks nothing -- it can only make two functions
            look MORE alike, i.e. it biases toward FALSE POSITIVES, never false
            negatives.  Stated so the null is read correctly.
  score     Jaccard over 4-gram token shingles (bottom-k sketch to shortlist, then
            EXACT Jaccard on the shortlist).  Same family as BinDiff's
            structural scoring, at instruction granularity.

CALIBRATION
  POSITIVE  RB3 functions that are (a) named at match_percent_normalized==100 in
            report.json -- so our source compiles to retail's bytes there and the
            (address,name) pairing is BYTE-VERIFIED -- and (b) whose exact mangled
            name also appears in DC3's leaked map.  Truth = that DC3 VA.
            Measured: top-1 retrieval accuracy over ALL named DC3 functions.
  STRATIFY  *** the load-bearing split ***  Is the recovered pair already
            token-hash IDENTICAL?  Byte/mask identity is an ALREADY-DRAINED
            channel (tools/dc3_content_match.py).  A structural channel only
            earns its keep on pairs whose bodies DIFFER.  Recovery is therefore
            reported separately for IDENTICAL vs DIVERGENT bodies.
  NULL      RB3 GOLD functions whose name is ABSENT from DC3's map -- i.e. code
            DC3 does not contain.  These are the operationally relevant decoys:
            the 147 unpinned TUs will contain many.  A usable channel must score
            these LOW.  If the decoy top-1 score distribution overlaps the
            positive one, a score threshold cannot separate "found it" from
            "found something that merely resembles it".
"""
import hashlib
import json
import os
import random
import struct
import sys
from collections import defaultdict


REPO = "/home/free/code/milohax/rb3-xenon"
DC3EXE = "/home/free/code/milohax/dc3-decomp/orig/373307D9/ham_xbox_r.exe"
DC3MAP = "/home/free/code/milohax/dc3-decomp/orig/373307D9/ham_xbox_r.map"
OUT = "/home/free/tmp/laneDL1"
sys.path.insert(0, os.path.join(REPO, "tools"))
from retail_rtti import RetailPE  # noqa: E402
import dc3_map  # noqa: E402

MASK_IMM = {14, 15, 24, 25, 26, 27, 28, 29}


def load_fns(path):
    """VA -> (size, code bytes).  Extent decode validated at 100% (21376/21377)."""
    R = RetailPE(path)
    pd = [s for s in R.sections if s.name == ".pdata"][0]
    tx = [s for s in R.sections if s.name == ".text"][0]
    raw = R.data[pd.rawptr:pd.rawptr + pd.vsize]
    out = {}
    for i in range(0, len(raw) - 7, 8):
        beg, fl = struct.unpack_from(">II", raw, i)
        if not beg:
            continue
        n = ((fl >> 8) & 0x3FFFFF) * 4
        if n <= 0 or not (tx.va <= beg < tx.va + tx.vsize):
            continue
        off = tx.rawptr + (beg - tx.va)
        body = R.data[off:off + n]
        if len(body) == n:
            out[beg] = body
    return out


def tokens(body):
    """Cross-binary-normalized instruction tokens."""
    t = []
    for i in range(0, len(body) - 3, 4):
        w = struct.unpack_from(">I", body, i)[0]
        op = w >> 26
        if op == 18:                      # b / bl / ba / bla
            t.append(0x12000000 | (w & 3))
        elif op == 16:                    # bc
            t.append(0x10000000 | (w & 0x03FF0003))
        elif op in MASK_IMM:              # address-forming immediates
            t.append(w & 0xFFFF0000)   # keep opcode+RT+RA, drop immediate
        else:
            t.append(w)
    return t


def shingles(tok, k=4):
    if len(tok) < k:
        return {hash(tuple(tok)) & 0xFFFFFFFF} if tok else set()
    return {hash(tuple(tok[i:i + k])) & 0xFFFFFFFF for i in range(len(tok) - k + 1)}


def tokhash(tok):
    return hashlib.sha1(struct.pack(f">{len(tok)}q", *tok)).hexdigest()[:16]


# --------------------------------------------------------------------------
# Bottom-k sketch inverted index (pure stdlib; numpy is unavailable and
# installing is out of scope).  Two sets sharing a small-hash element is a
# standard unbiased shortlist for Jaccard; exact Jaccard then rescores.
SKETCH_K = 32


def sketch(sh):
    return sorted(sh)[:SKETCH_K]


def jac(x, y):
    if not x or not y:
        return 0.0
    return len(x & y) / len(x | y)


def main():
    print("[1] extracting function bodies from both retail PEs ...")
    rb3 = load_fns(os.path.join(REPO, "orig/45410914/band.exe"))
    dc3 = load_fns(DC3EXE)
    print(f"    RB3 {len(rb3)} fns   DC3 {len(dc3)} fns")

    dmap = dc3_map.parse_map(DC3MAP)
    assert dmap.get("?Poll@Character@@UAAXXZ", {}).get("addr") == 0x82351090, \
        "C1 FAILED: DC3 map known-positive missing; every zero below is an artifact"
    dc3_name_by_va = defaultdict(list)
    for n, e in dmap.items():
        dc3_name_by_va[e["addr"]].append(n)
    print(f"[C1 PASS] DC3 map {len(dmap)} names over {len(dc3_name_by_va)} VAs")

    # candidate pool = DC3 functions with a real .pdata extent AND a map name
    pool_va = [va for va in dc3 if va in dc3_name_by_va]
    print(f"[2] DC3 candidate pool (pdata extent AND map name): {len(pool_va)}")

    pool_tok = [tokens(dc3[va]) for va in pool_va]
    pool_sh = [shingles(t) for t in pool_tok]
    pool_hash = [tokhash(t) for t in pool_tok]
    print("[3] building bottom-k sketch index over DC3 pool ...")
    postings = defaultdict(list)
    for i, sh in enumerate(pool_sh):
        for h in sketch(sh):
            postings[h].append(i)
    va_index = {va: i for i, va in enumerate(pool_va)}
    print(f"    postings keys={len(postings)}")

    # ---- ground truth ----------------------------------------------------
    tsm = json.load(open(os.path.join(REPO, "scripts/target_symbol_map.json")))
    tsm = {a: n for a, n in tsm.items() if a.startswith("0x") and isinstance(n, str)}
    n2a = {}
    for a, n in tsm.items():
        n2a.setdefault(n, int(a, 16))
    rep = json.load(open(os.path.join(REPO, "build/45410914/report.json")))
    gold = {}
    for u in rep["units"]:
        for f in u.get("functions") or []:
            n = f.get("name", "")
            if n.startswith("fn_") or not n:
                continue
            if f.get("match_percent_normalized") != 100.0:
                continue
            va = n2a.get(n)
            if va is not None and va in rb3:
                gold[va] = n
    pos = [(va, n) for va, n in gold.items()
           if n in dmap and dmap[n]["addr"] in va_index]
    neg = [(va, n) for va, n in gold.items() if n not in dmap]
    print(f"[4] GOLD byte-verified RB3 fns: {len(gold)}")
    print(f"    POSITIVE (name present in DC3 map, DC3 fn in pool): {len(pos)}")
    print(f"    DECOY    (name ABSENT from DC3 map = DC3 lacks it): {len(neg)}")

    def retrieve(qsh, topn=150):
        hits = defaultdict(int)
        for h in sketch(qsh):
            pl = postings.get(h)
            if not pl or len(pl) > 4000:   # skip degenerate ultra-common shapes
                continue
            for i in pl:
                hits[i] += 1
        if not hits:
            return []
        cand = sorted(hits, key=hits.get, reverse=True)[:topn]
        return sorted(((jac(qsh, pool_sh[c]), c) for c in cand), reverse=True)

    NQ = 500
    rnd = random.Random(7)
    psample = rnd.sample(pos, min(NQ, len(pos)))
    nsample = rnd.sample(neg, min(NQ, len(neg)))

    print(f"\n[5] POSITIVE CONTROL: top-1 retrieval, {len(psample)} queries "
          f"over {len(pool_va)} DC3 candidates")
    res = {"ident": [0, 0], "diverg": [0, 0]}   # [hit, total]
    pos_scores, pos_hit_scores = [], []
    misses = []
    for va, name in psample:
        qt = tokens(rb3[va])
        qsh = shingles(qt)
        truth_va = dmap[name]["addr"]
        identical = tokhash(qt) == pool_hash[va_index[truth_va]]
        strat = "ident" if identical else "diverg"
        best = retrieve(qsh)
        if not best:
            res[strat][1] += 1
            continue
        s1, c1 = best[0]
        pos_scores.append(s1)
        hit = (pool_va[c1] == truth_va) or (name in dc3_name_by_va[pool_va[c1]])
        res[strat][1] += 1
        if hit:
            res[strat][0] += 1
            pos_hit_scores.append(s1)
        elif len(misses) < 15:
            misses.append((hex(va), name[:55], s1,
                           dc3_name_by_va[pool_va[c1]][0][:55]))
    for k in ("ident", "diverg"):
        h, t = res[k]
        lab = ("token-IDENTICAL bodies (already-drained channel)" if k == "ident"
               else "DIVERGENT bodies (what structural must earn)")
        print(f"    {lab}: {h}/{t} = {100*h/t if t else 0:.1f}% top-1 correct")
    tot_h = res['ident'][0] + res['diverg'][0]
    tot_t = res['ident'][1] + res['diverg'][1]
    print(f"    OVERALL top-1: {tot_h}/{tot_t} = {100*tot_h/tot_t if tot_t else 0:.1f}%")

    print(f"\n[6] DECOY NULL: {len(nsample)} RB3 fns DC3 does NOT contain")
    neg_scores = []
    for va, name in nsample:
        qsh = shingles(tokens(rb3[va]))
        best = retrieve(qsh)
        neg_scores.append(best[0][0] if best else 0.0)

    def d(v):
        if not v:
            return "n=0"
        v = sorted(v)
        n = len(v)
        return (f"n={n} p05={v[n//20]:.3f} p25={v[n//4]:.3f} med={v[n//2]:.3f} "
                f"p75={v[3*n//4]:.3f} p95={v[19*n//20]:.3f} max={v[-1]:.3f}")
    print(f"    POSITIVE top-1 score (correct hits) : {d(pos_hit_scores)}")
    print(f"    POSITIVE top-1 score (all queries)  : {d(pos_scores)}")
    print(f"    DECOY    top-1 score                : {d(neg_scores)}")

    # separation: at what threshold does the decoy FP rate fall below 5%?
    if neg_scores and pos_hit_scores:
        ns = sorted(neg_scores)
        thr = ns[int(0.95 * len(ns))]
        keep = sum(1 for s in pos_hit_scores if s >= thr)
        print(f"\n[7] SEPARATION: threshold at DECOY 95th pct = {thr:.3f}")
        print(f"    correct positives surviving that threshold: "
              f"{keep}/{len(pos_hit_scores)} = "
              f"{100*keep/len(pos_hit_scores):.1f}%")
    print("\n[8] sample POSITIVE misses (truth vs what was retrieved)")
    for m in misses:
        print(f"    {m[0]} truth={m[1]:55s} got(sim={m[2]:.3f})={m[3]}")

    json.dump({
        "rb3_fns": len(rb3), "dc3_fns": len(dc3), "pool": len(pool_va),
        "gold": len(gold), "positive_avail": len(pos), "decoy_avail": len(neg),
        "top1_identical": res["ident"], "top1_divergent": res["diverg"],
        "pos_hit_scores": pos_hit_scores, "pos_scores": pos_scores,
        "decoy_scores": neg_scores,
    }, open(os.path.join(OUT, "calibration_structural.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
