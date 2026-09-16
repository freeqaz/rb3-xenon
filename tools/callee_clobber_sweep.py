#!/usr/bin/env python3
"""W16-ET: whole-binary census for the CALLEE-CLOBBER lever (W16-ER's mechanism).

MSVC X360 /O1 performs intra-TU callee-clobber analysis: a caller's codegen
depends on what its callee provably writes.  So a sub-100 caller can be behind
charges that are *not its own* -- the lever is the callee's BODY.

Two preconditions are structural and therefore cheaply checkable without
running objdiff at all:

  1. the callee must live in the SAME translation unit (the analysis is
     intra-TU -- there is no LTCG in this build, verified in CLAUDE.md), and
  2. our callee body must be WRONG.  A byte-exact callee has a byte-exact
     clobber set by construction, so it can induce nothing.

Stage 1 (this file, free) builds the intra-TU call graph from the relocations of
our OWN compiled objects and intersects it with report.json's per-row scores.
Stage 2 (--verify) runs objdiff on the survivors and keeps only those whose
charges actually concentrate in the window preceding a call to a non-100 callee.

Usage:
  python3 tools/callee_clobber_sweep.py --project-dir <wt> [--json out.json]
  python3 tools/callee_clobber_sweep.py --project-dir <wt> --verify --top 60
"""
import argparse, collections, json, os, subprocess, sys, tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from coff_bodies_ext import function_bodies_ext

IMAGE_REL_PPC_REL24 = 0x06


def placeholder(n):
    return n.startswith(("fn_", "lbl_", "jumptable_", "data_", "bss_", "rdata_"))


def load_report(root):
    d = json.load(open(root / "build/45410914/report.json"))
    units = {}
    for u in d["units"]:
        rows = {}
        for f in u.get("functions", []):
            rows[f["name"]] = {
                "fuzzy": float(f.get("fuzzy_match_percent", 0)),
                "mpn": float(f.get("match_percent_normalized", 0)),
                "size": int(f.get("size", 0)),
            }
        units[u["name"]] = rows
    return d, units


def build_callgraph(root, base_path):
    """name -> set of intra-TU REL24 callee names, for one of OUR objects."""
    p = root / base_path
    if not p.exists():
        return None
    bodies = {}
    for name, body, rl, off in function_bodies_ext(str(p)):
        bodies.setdefault(name, []).append(rl)
    defined = set(bodies)
    out = {}
    for name, rls in bodies.items():
        callees = set()
        for rl in rls:
            for (_o, tgt, t) in rl:
                if t == IMAGE_REL_PPC_REL24 and tgt in defined and tgt != name:
                    callees.add(tgt)
        out[name] = callees
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-dir", required=True)
    ap.add_argument("--json")
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("--top", type=int, default=60)
    ap.add_argument("--limit", type=int, default=100000)
    ap.add_argument("--conc", type=float, default=0.8)
    ap.add_argument("--min-size", type=int, default=0)
    a = ap.parse_args()
    root = Path(a.project_dir)

    rep, runits = load_report(root)
    od = json.load(open(root / "objdiff.json"))

    stats = collections.Counter()
    cands = []
    for u in od["units"]:
        uname = u["name"]
        rows = runits.get(uname)
        if not rows:
            continue
        bp = u.get("base_path")
        if not bp:
            stats["unit_no_base_path"] += 1
            continue
        cg = build_callgraph(root, bp)
        if cg is None:
            stats["unit_no_base_obj"] += 1
            continue
        stats["unit_scanned"] += 1
        for caller, callees in cg.items():
            r = rows.get(caller)
            if r is None:
                stats["caller_unpaired"] += 1
                continue
            if placeholder(caller):
                continue
            stats["caller_named_rows"] += 1
            if r["fuzzy"] >= 100.0:
                continue
            stats["caller_named_sub100"] += 1
            if not callees:
                stats["caller_no_intratu_call"] += 1
                cands.append({"unit": uname, "caller": caller, "size": r["size"],
                              "fuzzy": r["fuzzy"], "mpn": r["mpn"], "n_callees": 0,
                              "struct_candidate": False, "bad_callees": []})
                continue
            stats["caller_has_intratu_call"] += 1
            bad = []
            for c in callees:
                cr = rows.get(c)
                if cr is None:
                    bad.append((c, "UNPAIRED", 0.0))
                elif cr["fuzzy"] < 100.0:
                    bad.append((c, "SUB100", cr["fuzzy"]))
            if not bad:
                stats["all_callees_exact"] += 1
            else:
                stats["CANDIDATE"] += 1
            if r["size"] < a.min_size:
                continue
            cands.append({
                "unit": uname, "caller": caller, "size": r["size"],
                "fuzzy": r["fuzzy"], "mpn": r["mpn"],
                "n_callees": len(callees),
                "struct_candidate": bool(bad),
                "bad_callees": sorted(bad, key=lambda x: -x[1].count("U"))[:5],
            })

    cands.sort(key=lambda c: -c["size"])
    print("=== STAGE 1: structural census ===")
    for k in sorted(stats):
        print(f"  {k:26s} {stats[k]}")
    tb = sum(c["size"] for c in cands)
    print(f"  candidate bytes            {tb}")
    print()
    print(f"{'size':>6} {'fuzzy':>9} {'mpn':>9}  unit / caller  <- bad callees")
    for c in cands[:a.top]:
        bc = ", ".join(f"{n}[{k}]" for n, k, _ in c["bad_callees"][:3])
        print(f"{c['size']:6d} {c['fuzzy']:9.4f} {c['mpn']:9.4f}  {c['unit']}/{c['caller'][:70]}")
        print(f"       <- {bc[:150]}")
    if a.verify:
        print("\n=== STAGE 2: diff-shape verification ===", file=sys.stderr)
        cands = verify(root, runits, od, cands, a.limit, a.conc)
    if a.json:
        json.dump({"stats": dict(stats), "candidates": cands}, open(a.json, "w"), indent=1)
        print(f"\nwrote {a.json} ({len(cands)} rows)")




# ---------------------------------------------------------------------------
# STAGE 2: diff-shape verification, with arms where the mechanism is IMPOSSIBLE
# ---------------------------------------------------------------------------
def run_objdiff(root, symbol, unit, out):
    cmd = [str(root / "bin/objdiff-cli"), "diff", "-p", str(root), symbol,
           "--include-instructions", "-c", "functionRelocDiffs=name_check",
           "-u", unit, "-f", "json", "-o", out]
    r = subprocess.run(cmd, cwd=str(root), capture_output=True, text=True)
    if r.returncode != 0:
        return None
    try:
        return json.load(open(out))
    except Exception:
        return None


def shape(d):
    """Partition instructions into call-setup blocks at `bl` boundaries.

    Returns (n_charges, best_fraction, callee_of_best_block, n_blocks).
    Parameter-free: a block is everything since the previous `bl`, ending at a
    `bl`.  ER's ScoreSinger had 18 of 19 charges inside ONE such block.
    """
    ins = d["instructions"]
    charged = [i for i, r in enumerate(ins) if r["match_type"] != "equal"]
    if not charged:
        return 0, 0.0, None, 0
    bounds, callees = [], []
    for i, r in enumerate(ins):
        t = r.get("target") or r.get("base") or {}
        if t.get("opcode") == "bl":
            sym = None
            for ta in t.get("typed_args", []):
                if ta.get("type") == "Symbol":
                    sym = ta.get("value")
            bounds.append(i)
            callees.append(sym)
    blocks, prev = [], -1
    for b, c in zip(bounds, callees):
        blocks.append((prev + 1, b, c))
        prev = b
    blocks.append((prev + 1, len(ins) - 1, None))   # tail block, no call
    best = (0, None)
    for lo, hi, c in blocks:
        n = sum(1 for i in charged if lo <= i <= hi)
        if n > best[0]:
            best = (n, c)
    return len(charged), best[0] / len(charged), best[1], len(blocks)


def verify(root, runits, od, cands_all, limit, conc):
    by_unit_rows = runits
    defined_cache = {}
    results = []
    tmp = tempfile.mkdtemp(prefix="w16et_")
    for n, c in enumerate(cands_all[:limit]):
        unit = c["unit"]
        if unit not in defined_cache:
            u = next((x for x in od["units"] if x["name"] == unit), None)
            defined_cache[unit] = build_callgraph(root, u["base_path"]) if u else {}
        out = os.path.join(tmp, "d.json")
        d = run_objdiff(root, c["caller"], unit, out)
        if d is None:
            c["verify"] = "OBJDIFF_FAIL"
            results.append(c)
            continue
        nch, frac, callee, nblk = shape(d)
        rows = by_unit_rows.get(unit, {})
        same_tu = set(defined_cache.get(unit) or {})
        if callee is None:
            klass = "NO_CALL_IN_BLOCK"
        elif callee not in same_tu:
            klass = "EXTERNAL"           # no LTCG => compiler cannot see body
        elif rows.get(callee) is None:
            klass = "SAME_TU_UNPAIRED"
        elif rows[callee]["fuzzy"] >= 100.0:
            klass = "SAME_TU_EXACT"      # mechanism IMPOSSIBLE (control)
        else:
            klass = "SAME_TU_SUB100"     # mechanism POSSIBLE
        c.update(n_charges=nch, conc=round(frac, 4), block_callee=callee,
                 callee_class=klass, n_blocks=nblk,
                 target_size=d.get("target_size"), base_size=d.get("base_size"),
                 signature=bool(frac >= conc and nch <= 30))
        results.append(c)
        if n % 200 == 0:
            print(f"  ... {n}/{min(limit,len(cands_all))}", file=sys.stderr)
    return results


if __name__ == "__main__":
    main()
