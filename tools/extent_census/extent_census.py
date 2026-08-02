#!/usr/bin/env python3
"""Target-vs-base function EXTENT census + artifact/codegen adjudication.

Built for lane CN-2 (2026-08-02) to answer: after CM-3 fixed the 8-byte EH
prefix class (+193 matched), are the REMAINING ~1,200 paired functions whose
extent differs from the target also boundary/attribution artifacts, or are they
real codegen differences?

ANSWER: real codegen, all of them.  See `docs/` / the lane report.  This tool is
committed so the question is not re-opened from scratch.

Subcommands
-----------
  validate   CONTROL. Reproduce report.json's TARGET `size` for every function
             row using coffx.py.  Must print 69359/69359 (or whatever the
             current total_functions is) before ANY other output is trusted.
  census     Compute target-vs-base extent deltas for all paired functions.
  adjudicate Per-row artifact-vs-codegen verdict from three instruments:
               A) start-aligned masked byte equality (anti-vacuity gated)
               C) END-aligned masked equality  -- catches FRONT-boundary errors,
                  which A is structurally blind to (CM-3's defect was a PREFIX)
               M) mechanism test: is there a symbol at the shorter extent's end?
  null       RANDOM-OFFSET NULL for the mechanism test.  Required: $M##### block
             labels occupy ~1.9% of word slots, so ~2.7% of ANY offset lands on
             one.  Measured enrichment was 0.99x -- i.e. the 33 "boundary hits"
             were pure chance.  Do not report M without this.

Usage
-----
  python3 tools/extent_census/extent_census.py validate   [--root .]
  python3 tools/extent_census/extent_census.py census     [--root .] [--out census.json]
  python3 tools/extent_census/extent_census.py adjudicate [--root .]
  python3 tools/extent_census/extent_census.py null       [--root .]
"""
import argparse
import json
import os
import random
import sys
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coffx

# ---------------------------------------------------------------- helpers


def _load_cfg(root):
    rep = json.load(open(os.path.join(root, "build/45410914/report.json")))
    cfg = json.load(open(os.path.join(root, "objdiff.json")))
    return rep, cfg


def _fn_index(root, path):
    """name -> [(sym, section)] for non-hidden Function symbols in code sections."""
    loaded = coffx.load(os.path.join(root, path))
    idx = defaultdict(list)
    if not loaded:
        return idx
    secs, syms = loaded
    bysec = {s.index: s for s in secs}
    for sy in syms:
        if sy.sec > 0 and sy.kind == coffx.K_FUNCTION:
            sc = bysec.get(sy.sec)
            if sc is not None and sc.code and not coffx.is_hidden(sy.name):
                idx[sy.name].append((sy, sc))
    for v in idx.values():
        v.sort(key=lambda t: t[0].size)
    return idx


def _masked_eq(tb, bb, tmask, bmask):
    """House anti-vacuity guard: >=4 unmasked words AND >=50% of body unmasked."""
    nw = min(len(tb), len(bb)) // 4
    mask = tmask | bmask
    um = [i for i in range(nw) if i not in mask]
    if nw == 0 or len(um) < 4 or len(um) / nw < 0.5:
        return "vacuous"
    return all(tb[i * 4:i * 4 + 4] == bb[i * 4:i * 4 + 4] for i in um)


def _window(sy, sc, start, n):
    off = sy.addr - sc.addr + start
    data = sc.data[off:off + n]
    lo = sy.addr + start
    mask = {(rva - lo) // 4 for (rva, _, _) in sc.relocs if lo <= rva < lo + n}
    return data, mask


# ---------------------------------------------------------------- validate

def cmd_validate(a):
    rep, cfg = _load_cfg(a.root)
    tgt = {u["name"]: u.get("target_path") for u in cfg["units"]}
    ok = bad = absent = 0
    ex = []
    for u in rep["units"]:
        fns = u.get("functions") or []
        tp = tgt.get(u["name"])
        if not fns or not tp or not os.path.exists(os.path.join(a.root, tp)):
            continue
        mine = defaultdict(list)
        for nm, lst in _fn_index(a.root, tp).items():
            mine[nm] = [s.size for s, _ in lst]
        for f in fns:
            want, lst = int(f["size"]), mine.get(f["name"])
            if not lst:
                absent += 1
            elif want in lst:
                lst.remove(want)
                ok += 1
            else:
                bad += 1
                if len(ex) < 10:
                    ex.append((u["name"], f["name"], want, list(lst)))
    print(f"target-size reproduction: ok={ok} mismatched={bad} absent={absent}")
    for e in ex:
        print("   ", e)
    print("VERDICT:", "PASS" if bad == 0 and absent == 0 else "FAIL")
    return 0 if bad == 0 and absent == 0 else 1


# ---------------------------------------------------------------- census

def cmd_census(a):
    rep, cfg = _load_cfg(a.root)
    paths = {u["name"]: (u.get("target_path"), u.get("base_path")) for u in cfg["units"]}
    rows = []
    for u in rep["units"]:
        fns = u.get("functions") or []
        tp, bp = paths.get(u["name"], (None, None))
        if not fns or not tp or not bp:
            continue
        B = _fn_index(a.root, bp)
        if not B:
            continue
        used = defaultdict(int)
        for f in fns:
            nm = f["name"]
            bl = B.get(nm)
            if not bl:
                continue
            k = used[nm]
            used[nm] += 1
            b = bl[k][0].size if k < len(bl) else bl[-1][0].size
            rows.append({"unit": u["name"], "name": nm, "tgt": int(f["size"]),
                         "base": b, "delta": b - int(f["size"]),
                         "mpn": f.get("match_percent_normalized"),
                         "fuzzy": f.get("fuzzy_match_percent"),
                         "target_path": tp, "base_path": bp})
    json.dump(rows, open(a.out, "w"))
    c = Counter(r["delta"] for r in rows)
    nz = sum(v for d, v in c.items() if d)
    print(f"paired functions: {len(rows)}   nonzero-delta: {nz}   (delta==0: {c[0]})")
    for d, n in sorted(((d, n) for d, n in c.items() if d), key=lambda x: -x[1])[:24]:
        print(f"  {d:+6d}: {n:5d}")
    return 0


# ---------------------------------------------------------------- adjudicate

def _classify_surplus(b, reloc_words, nwords):
    """★ ORDER MATTERS.  The first version returned `reloc_words` before testing
    for real code, so every `bl`/`b` (whose whole word is relocation-covered)
    was mis-filed as a data word -- 1,004 of 1,234 rows piled into one bucket.
    `reloc_data` now requires EVERY word to be relocation-covered, which is the
    actual EH-prefix / pointer-table signature.  Even so, the surviving 156 all
    turned out to be `b __restgprlr_NN` epilogue branches -- i.e. real code."""
    if not b:
        return "none"
    w = [b[i:i + 4] for i in range(0, len(b) - 3, 4)]
    if all(x == b"\x00\x00\x00\x00" for x in w):
        return "zeros"
    if all(x == b"\x60\x00\x00\x00" for x in w):
        return "nops"
    if all(x in (b"\x00\x00\x00\x00", b"\x60\x00\x00\x00") for x in w):
        return "zeros+nops"
    if nwords and reloc_words == nwords:
        return "reloc_data"
    return "real_code"


def cmd_adjudicate(a):
    rows = [r for r in json.load(open(a.out)) if r["delta"] != 0]
    cache = {}

    def idx(p):
        if p not in cache:
            cache[p] = _fn_index(a.root, p)
        return cache[p]

    def pick(p, nm, sz):
        lst = idx(p).get(nm) or []
        for x in lst:
            if x[0].size == sz:
                return x
        return lst[0] if lst else (None, None)

    out = []
    for r in rows:
        tsy, tsc = pick(r["target_path"], r["name"], r["tgt"])
        bsy, bsc = pick(r["base_path"], r["name"], r["base"])
        if tsy is None or bsy is None:
            continue
        n = min(r["tgt"], r["base"])
        tb, tm = _window(tsy, tsc, 0, n)
        bb, bm = _window(bsy, bsc, 0, n)
        A = _masked_eq(tb, bb, tm, bm)
        # C: END-aligned -- A is blind to a boundary error at the FRONT
        tb2, tm2 = _window(tsy, tsc, r["tgt"] - n, n)
        bb2, bm2 = _window(bsy, bsc, r["base"] - n, n)
        C = _masked_eq(tb2, bb2, tm2, bm2)
        # surplus on the longer side
        ssy, ssc = (bsy, bsc) if r["delta"] > 0 else (tsy, tsc)
        ln = abs(r["delta"])
        sb, sm = _window(ssy, ssc, n, ln)
        S = _classify_surplus(sb, len(sm), ln // 4)
        # M: mechanism -- a symbol sitting at the shorter extent's end
        _, syms = coffx.load(os.path.join(
            a.root, r["base_path"] if r["delta"] > 0 else r["target_path"]))
        at_end = [s for s in syms if s.sec == ssy.sec
                  and s.addr == ssy.addr + n and s is not ssy]
        out.append({**r, "A": A, "C": C, "surplus": S,
                    "M": [(s.name, s.sclass) for s in at_end],
                    "surplus_hex": sb.hex()})
    json.dump(out, open(a.adj, "w"))
    print(f"adjudicated {len(out)} nonzero-delta rows")
    print("A (start-aligned masked equality):", Counter(r["A"] for r in out))
    print("C (end-aligned masked equality)  :", Counter(r["C"] for r in out))
    print("surplus class                    :", Counter(r["surplus"] for r in out))
    print("M (symbol at shorter extent end) :",
          Counter(bool(r["M"]) for r in out), "-> ALWAYS compare against `null`")
    print("\nARTIFACT CANDIDATES (A==True and surplus is not real code):",
          sum(1 for r in out if r["A"] is True and r["surplus"] not in ("real_code", "reloc_data")))
    return 0


# ---------------------------------------------------------------- null

def cmd_null(a):
    rows = json.load(open(a.adj))
    cache = {}
    random.seed(11)
    obs, n, nulls = 0, 0, [0] * 5
    for r in rows:
        p = r["base_path"] if r["delta"] > 0 else r["target_path"]
        lsz = r["base"] if r["delta"] > 0 else r["tgt"]
        ssz = r["tgt"] if r["delta"] > 0 else r["base"]
        if p not in cache:
            cache[p] = coffx.load(os.path.join(a.root, p))
        loaded = cache[p]
        if not loaded:
            continue
        _, syms = loaded
        cand = [s for s in syms if s.name == r["name"] and s.size == lsz]
        if not cand:
            continue
        sy = cand[0]
        n += 1
        offs = {s.addr - sy.addr for s in syms
                if s.sec == sy.sec and s is not sy and 0 < s.addr - sy.addr < lsz + 8}
        if ssz in offs:
            obs += 1
        slots = max(1, lsz // 4)
        for k in range(5):
            if 4 * random.randint(1, slots) in offs:
                nulls[k] += 1
    mean = sum(nulls) / len(nulls)
    print(f"rows={n}  OBSERVED={obs} ({100.0*obs/n:.2f}%)  "
          f"NULLS={nulls} mean={mean:.1f}")
    print(f"ENRICHMENT = {obs/mean if mean else float('inf'):.2f}x"
          "   (<=~1.2x means the mechanism hits are CHANCE)")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["validate", "census", "adjudicate", "null"])
    ap.add_argument("--root", default=".", help="repo/worktree root")
    ap.add_argument("--out", default="census.json")
    ap.add_argument("--adj", default="adjudicated.json")
    a = ap.parse_args()
    return {"validate": cmd_validate, "census": cmd_census,
            "adjudicate": cmd_adjudicate, "null": cmd_null}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
