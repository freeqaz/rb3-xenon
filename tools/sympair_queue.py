#!/usr/bin/env python3
"""Whole-binary REALIZABILITY queue: rows that would cross to `fuzzy == 100`
if one relocation-symbol NAME were corrected.

Promoted from lane W2-ENGINE's throwaway `charged.py` + `sweep.py`
(2026-08-17, `b5a242c4`) by lane W7-SYMPAIR. W2's scripts lived in `~/tmp` and
would have died there; the two map defects they found were worth +12,780 B.

WHY THIS EXISTS
───────────────
`matched_code` is **all-or-nothing per row** (CLAUDE.md, residue-decomposition
rule), so the correct ranking is **size-if-it-crosses**, never penalty size and
never a mismatch count.  And a mismatch count is the wrong instrument twice
over:

  * "N/N instructions equal" is INSTRUCTION-level and excludes argument-level
    `diff_arg` charges.  A row can read "205 instructions | all equal" while
    scoring 98.4% graded.  A lane pre-registered +96 B off a "24/24 equal"
    reading and measured −92 B.
  * objdiff's own `LINKER_MERGED` / `AT_LIMIT` verdict on a relocation-name
    charge is *the detector restating its own input*: "target calls A, we call
    B, A != B" is bit-for-bit the definition of BOTH a genuine ICF fold AND a
    wrong map name.  It was measured WRONG on rows a lane then fixed by editing
    source (+6,304 B, predicted exactly).

So this tool enumerates each row's CHARGED SITES on the grader's ruler, splits
them into relocation-symbol-pair charges vs everything else, and surfaces the
rows whose ONLY charges are symbol pairs -- those are realizable by naming work
alone.

⚠ WHAT THIS TOOL DOES NOT DO: it does NOT tell you whether a pair is a wrong
map name (fixable, pays) or an ICF fold (irreducible).  The metric CANNOT
separate them.  Adjudicate on retail bytes with `tools/retail_callers.py`;
every queue row carries the target callee's retail address for exactly that.

POPULATION (and why each exclusion is sound)
────────────────────────────────────────────
  * `mpn == 100 and 0 < fuzzy < 100` is the DEFAULT scan set, and it is a
    SOUND restriction rather than a shortcut: `mpn` excludes non-immediate arg
    penalties, so a row whose every charge is a symbol-arg diff has `mpn == 100`
    BY CONSTRUCTION.  The crossable class is therefore a subset.  Pass
    `--all-strata` to also scan `mpn < 100` rows -- they can never be crossable
    by naming alone, but their symbol pairs still inform the shared-defect
    census.
  * `fuzzy == 0` rows are UNPAIRABLE (no base obj); excluded.
  * placeholder names (`fn_`/`lbl_`/`auto_`/`jumptable_`/`data_`) and
    `auto_generated` units are excluded -- ⚠ note that an "equal" verdict on a
    PLACEHOLDER TARGET carries no information either, because `name_check`
    forgives placeholder targets by construction.

SELF-VALIDATION (run it; do not trust a rewrite)
───────────────────────────────────────────────
`--selftest` replays a committed fixture of W2's original 45-row engine sweep
and asserts it still derives **23 rows / 41,088 B**, W2's reported figure.  A
rewrite that silently changes the population is the single most common failure
mode in this repo.  The gate is proven able to FAIL (`--selftest --mutate`).

Usage:
  python3 tools/sympair_queue.py --project-dir ~/tmp/wt-foo            # full sweep
  python3 tools/sympair_queue.py --project-dir ~/tmp/wt-foo --categories engine --top 45
  python3 tools/sympair_queue.py --selftest
"""
import argparse
import collections
import concurrent.futures
import json
import os
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "scripts"))

PLACEHOLDER_PREFIXES = ("fn_", "lbl_", "auto_", "jumptable_", "data_")
FIXTURE = os.path.join(REPO, "tools", "testdata", "sympair_w2_control.json")
W2_EXPECT_ROWS = 23
W2_EXPECT_BYTES = 41088


# ── charge classification (kept bit-for-bit compatible with W2's charged.py) ──

def classify(ins):
    """Return (kind, detail) for one charged instruction, or None if equal."""
    mt = ins.get("match_type")
    if mt == "equal":
        return None
    if mt != "diff_arg":
        return (mt, "")
    bd = ins.get("diff_breakdown") or {}
    kinds = []
    for arg in bd.get("arguments", []):
        at = arg.get("arg_type", "")
        tv = (arg.get("target") or {}).get("value")
        bv = (arg.get("base") or {}).get("value")
        if tv == bv:
            continue
        if at == "register":
            kinds.append(f"reg {tv}->{bv}")
        elif at == "symbol":
            kinds.append(f"SYM {tv} -> {bv}")
        elif at == "immediate":
            kinds.append(f"imm {tv}->{bv}")
        elif at == "branch_dest":
            kinds.append(f"br {tv}->{bv}")
        else:
            kinds.append(f"{at} {tv}->{bv}")
    return ("diff_arg", "; ".join(kinds) if kinds else "(no breakdown)")


def charges_of_diff(data, symbol):
    """Extract [(index, kind, detail)] from an objdiff-cli diff JSON blob."""
    fn = None
    for sec in data.get("sections", []):
        for s in sec.get("symbols", []):
            if s.get("name") == symbol or (s.get("symbol") or {}).get("name") == symbol:
                fn = s
    if fn is None:
        fn = data
    instrs = fn.get("instructions") or data.get("instructions") or []
    out = []
    for ins in instrs:
        c = classify(ins)
        if c:
            out.append((ins["index"], c[0], c[1]))
    return out, len(instrs)


def sym_pairs(charges):
    """Split charges into (symbol-pair charges, other charges)."""
    syms, other = [], []
    for _idx, k, d in charges:
        if k == "diff_arg" and d.startswith("SYM"):
            syms.append(d)
        else:
            other.append((k, d))
    pairs = []
    for s in syms:
        body = s[4:]
        if " -> " in body:
            t, b = body.split(" -> ", 1)
            pairs.append((t.strip(), b.strip()))
    return pairs, other


# ───────────────────────────── population ─────────────────────────────

def load_population(report_path, categories=None, all_strata=False):
    d = json.load(open(report_path))
    rows = []
    for u in d["units"]:
        md = u.get("metadata", {}) or {}
        if md.get("auto_generated"):
            continue
        cats = md.get("progress_categories") or []
        if categories and not any(c in cats for c in categories):
            continue
        for fn in u.get("functions", []):
            n = fn["name"]
            if n.startswith(PLACEHOLDER_PREFIXES):
                continue
            # ⚠ report.json is protobuf-JSON: defaults omitted, numerics are strings
            sz = int(fn.get("size", 0))
            fz = float(fn.get("fuzzy_match_percent", 0.0))
            mpn = float(fn.get("match_percent_normalized", 0.0))
            if fz == 0.0 or fz >= 100.0:
                continue
            if not all_strata and mpn < 100.0:
                continue
            rows.append(dict(size=sz, fuzzy=fz, mpn=mpn, symbol=n, unit=u["name"],
                             categories=",".join(cats)))
    rows.sort(key=lambda r: -r["size"])
    return rows, d


def run_diff(symbol, project_dir, ruler_args, outdir):
    import hashlib
    h = hashlib.md5(symbol.encode()).hexdigest()[:12]
    out = os.path.join(outdir, f"d_{h}.json")
    cmd = [os.path.join(project_dir, "bin", "objdiff-cli"), "diff", symbol,
           "--include-instructions", "-f", "json", "-o", out] + ruler_args
    p = subprocess.run(cmd, cwd=project_dir, capture_output=True, text=True)
    if not os.path.exists(out):
        return None, (p.stdout[-400:] + p.stderr[-400:])
    with open(out) as fh:
        data = json.load(fh)
    os.unlink(out)
    return data, None


# ───────────────────────────── main sweep ─────────────────────────────

def sweep(rows, project_dir, ruler_args, jobs=8, progress=True):
    results = []
    errors = []
    with tempfile.TemporaryDirectory(prefix="sympair_") as outdir:
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
            futs = {ex.submit(run_diff, r["symbol"], project_dir, ruler_args, outdir): r
                    for r in rows}
            done = 0
            for fut in concurrent.futures.as_completed(futs):
                r = futs[fut]
                done += 1
                if progress and done % 200 == 0:
                    print(f"  ... {done}/{len(rows)}", file=sys.stderr, flush=True)
                try:
                    data, err = fut.result()
                except Exception as e:  # noqa: BLE001
                    errors.append((r["symbol"], repr(e)))
                    continue
                if data is None:
                    errors.append((r["symbol"], err))
                    continue
                ch, ninstr = charges_of_diff(data, r["symbol"])
                pairs, other = sym_pairs(ch)
                rr = dict(r)
                rr.update(charged=len(ch), instructions=ninstr,
                          sym_sites=len(pairs), other_charges=len(other),
                          pairs=pairs, other=other)
                results.append(rr)
    return results, errors


def classify_row(r):
    """Realizability class for a swept row."""
    if not r["pairs"]:
        return "NO_SYMPAIR"
    if r["other_charges"]:
        return "MIXED"
    n = len(set(r["pairs"]))
    return "SYMPAIR_ONLY_1" if n == 1 else f"SYMPAIR_ONLY_{n}"


def build_address_index(project_dir):
    """name -> [retail addresses], from the target symbol map (for adjudication)."""
    p = os.path.join(project_dir, "scripts", "target_symbol_map.json")
    idx = collections.defaultdict(list)
    arbitrary = set()
    if os.path.exists(p):
        m = json.load(open(p))
        for k in ("_bijection_arbitrary", "_icf_arbitrary"):
            arbitrary |= {a.lower() for a in m.get(k, []) if isinstance(a, str)}
        for addr, name in m.items():
            if not addr.startswith("0x"):
                continue  # metadata keys (_denylist, _bijection_arbitrary, ...)
            # a few map rows carry a LIST of names at one address (aliases)
            for nm in (name if isinstance(name, list) else [name]):
                if isinstance(nm, str):
                    idx[nm].append(addr)
    return idx, arbitrary


def triage(results, addr_idx, arbitrary):
    """Label each PAIR with the adjudication class it falls in.

    ⚠ These are TRIAGE HINTS, not verdicts. The metric cannot separate a fold
    from a wrong name; only retail bytes can (tools/retail_callers.py).

      FOLD_FANIN  -- ≥2 of OUR distinct functions map to this one target
                     address. That is the ICF-survivor signature: retail folded
                     N identical bodies and the map can spell only one of them.
                     Irreducible; do NOT chase.
      RECIPROCAL  -- the reverse pair (ours -> target) also occurs, at a
                     DIFFERENT retail address. A fold maps two names onto ONE
                     address and so CANNOT produce a transposition across two;
                     this is the signature of a wrong/arbitrary map bijection.
                     ⚠ Still not proof -- see docs/decomp/sympair-queue.md.
      OURS_UNMAPPED -- our callee has no retail address at all. This is the
                     "no identified retail address" triage backlog, not noise.
      UNKNOWN     -- none of the above.
    """
    fan = collections.defaultdict(set)
    for r in results:
        for t, o in r["pairs"]:
            fan[t].add(o)
    pairset = {p for r in results for p in r["pairs"]}
    out = {}
    for t, o in pairset:
        at, ao = addr_idx.get(t, []), addr_idx.get(o, [])
        if (o, t) in pairset and at and ao and set(at) != set(ao):
            k = "RECIPROCAL"
        elif len(fan[t]) > 1:
            k = "FOLD_FANIN"
        elif not ao:
            k = "OURS_UNMAPPED"
        else:
            k = "UNKNOWN"
        out[(t, o)] = k
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project-dir", default=REPO)
    ap.add_argument("--report", default=None)
    ap.add_argument("--categories", default=None,
                    help="comma list, e.g. 'engine' or 'game'; default = ALL")
    ap.add_argument("--top", type=int, default=0, help="0 = all sizes")
    ap.add_argument("--all-strata", action="store_true",
                    help="also scan mpn<100 rows (never crossable; census only)")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--out-queue", default=None)
    ap.add_argument("--out-pairs", default=None)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--mutate", action="store_true",
                    help="with --selftest: corrupt the fixture to prove the gate can fail")
    args = ap.parse_args()

    if args.selftest:
        return selftest(mutate=args.mutate)

    pd = os.path.abspath(os.path.expanduser(args.project_dir))
    report = args.report or os.path.join(pd, "build", "45410914", "report.json")
    from analysis.ruler import graded_ruler
    ruler = graded_ruler(pd)
    print(f"# ruler: {ruler.label()}")

    cats = args.categories.split(",") if args.categories else None
    rows, _doc = load_population(report, cats, args.all_strata)
    if args.top:
        rows = rows[:args.top]
    print(f"# population: {len(rows)} rows / {sum(r['size'] for r in rows)} B"
          f"  categories={cats or 'ALL'} all_strata={args.all_strata}")

    results, errors = sweep(rows, pd, ruler.args, jobs=args.jobs)
    print(f"# swept {len(results)} rows, {len(errors)} errors")
    for s, e in errors[:10]:
        print(f"#   ERR {s[:70]}: {str(e)[:160]}")

    addr_idx, arbitrary = build_address_index(pd)

    for r in results:
        r["class"] = classify_row(r)
    pair_class = triage(results, addr_idx, arbitrary)

    # ── pair census (shared defects) ──
    pair_sites = collections.Counter()
    pair_bytes = collections.Counter()
    pair_rows = collections.defaultdict(set)
    for r in results:
        for p in r["pairs"]:
            pair_sites[p] += 1
            pair_rows[p].add(r["symbol"])
        for p in set(r["pairs"]):
            pair_bytes[p] += r["size"]

    crossable = [r for r in results if r["class"].startswith("SYMPAIR_ONLY")]
    crossable.sort(key=lambda r: -r["size"])
    tot = sum(r["size"] for r in crossable)
    print(f"\n*** CROSSABLE (only charges are symbol pairs): "
          f"{len(crossable)} rows / {tot} B ***")
    byclass = collections.Counter(r["class"] for r in results)
    print("# classes:", dict(byclass.most_common()))
    tri = collections.Counter(); trib = collections.Counter()
    for r in crossable:
        ks = {pair_class.get(p, "?") for p in set(r["pairs"])}
        k = ("FOLD_FANIN" if "FOLD_FANIN" in ks else
             "ALL_RECIPROCAL" if ks == {"RECIPROCAL"} else
             "ALL_OURS_UNMAPPED" if ks == {"OURS_UNMAPPED"} else "MIXED/UNKNOWN")
        tri[k] += 1; trib[k] += r["size"]
    print("# ROW TRIAGE (what would have to be true for the row to cross):")
    for k in sorted(trib, key=lambda x: -trib[x]):
        print(f"#   {k:20s} rows={tri[k]:5d} bytes={trib[k]:8d} ({100.0*trib[k]/max(tot,1):5.2f}%)")

    # ── outputs ──
    qpath = args.out_queue or os.path.join(pd, "docs", "decomp", "sympair-queue.tsv")
    os.makedirs(os.path.dirname(qpath), exist_ok=True)
    with open(qpath, "w") as fh:
        fh.write("# sympair queue -- rows realizable by relocation-NAME correction alone.\n")
        fh.write("# Generated by tools/sympair_queue.py (lane W7-SYMPAIR, from W2-ENGINE's charged.py/sweep.py).\n")
        fh.write("# ⚠ A pair here is EITHER a wrong map name (fixable, pays) OR an ICF fold\n")
        fh.write("#   (irreducible). The metric CANNOT separate them -- adjudicate on retail\n")
        fh.write("#   bytes with tools/retail_callers.py using tgt_addr below.\n")
        fh.write("# size_if_it_crosses = bytes this row adds to matched_code IF every pair is fixed.\n")
        fh.write("# pair_class: FOLD_FANIN=irreducible ICF survivor · RECIPROCAL=transposed\n")
        fh.write("#   map bijection (a fold cannot transpose across two addresses) ·\n")
        fh.write("#   OURS_UNMAPPED=our callee has no retail address (identification backlog).\n")
        fh.write("# map_arbitrary: target addr is in the map's own _bijection_arbitrary/_icf_arbitrary list.\n")
        fh.write("\t".join(["size", "fuzzy", "mpn", "class", "pair_class", "map_arbitrary",
                            "unit", "categories",
                            "symbol", "n_pairs", "sites", "tgt_addr",
                            "target_symbol", "our_symbol"]) + "\n")
        for r in crossable:
            uniq = sorted(set(r["pairs"]))
            for t, b in uniq:
                fh.write("\t".join([
                    str(r["size"]), f"{r['fuzzy']:.5f}", f"{r['mpn']:.3f}", r["class"],
                    pair_class.get((t, b), "?"),
                    "yes" if any(a.lower() in arbitrary for a in addr_idx.get(t, [])) else "no",
                    r["unit"], r["categories"], r["symbol"], str(len(uniq)),
                    str(sum(1 for p in r["pairs"] if p == (t, b))),
                    ",".join(addr_idx.get(t, [])) or "-", t, b]) + "\n")
    print(f"# wrote queue -> {qpath}")

    ppath = args.out_pairs or os.path.join(pd, "docs", "decomp", "sympair-pairs.tsv")
    with open(ppath, "w") as fh:
        fh.write("# Repeated (target -> ours) relocation-symbol pairs = SHARED defects.\n")
        fh.write("# One correct name can cross many rows. crossable_bytes counts only rows\n")
        fh.write("# whose ONLY charges are symbol pairs; total_bytes includes MIXED rows.\n")
        fh.write("\t".join(["sites", "rows", "total_bytes", "tgt_addr",
                            "target_symbol", "our_symbol"]) + "\n")
        for p, c in pair_sites.most_common():
            fh.write("\t".join([str(c), str(len(pair_rows[p])), str(pair_bytes[p]),
                                ",".join(addr_idx.get(p[0], [])) or "-",
                                p[0], p[1]]) + "\n")
    print(f"# wrote pair census -> {ppath}")

    print("\n# top 25 crossable rows:")
    for r in crossable[:25]:
        print(f"{r['size']:7d} fz={r['fuzzy']:9.5f} {r['unit'][:28]:28s} {r['symbol'][:52]}")
        for t, b in sorted(set(r["pairs"])):
            print(f"        TGT[{','.join(addr_idx.get(t, [])) or '-'}] {t[:100]}")
            print(f"        OUR {b[:100]}")
    return 0


# ───────────────────────────── selftest ─────────────────────────────

def selftest(mutate=False):
    """Replay W2-ENGINE's original 45-row engine sweep from the committed fixture.

    W2 reported 23 rows / 41,088 B of single-symbol-pair-crossable engine bytes.
    If this rewrite still derives that, the population and the classifier are
    unchanged. If it does not, STOP -- do not widen the sweep.
    """
    if not os.path.exists(FIXTURE):
        print(f"FAIL: fixture missing: {FIXTURE}")
        return 1
    fx = json.load(open(FIXTURE))
    rows = fx["rows"]
    if mutate:
        # prove the gate CAN fail: drop one crossable row's sole symbol charge
        for r in rows:
            ch = [(c[0], c[1], c[2]) for c in r["charges"]]
            pairs, other = sym_pairs(ch)
            if pairs and not other and len(set(pairs)) == 1:
                r["charges"] = [c for c in r["charges"]
                                if not (c[1] == "diff_arg" and c[2].startswith("SYM"))]
                print(f"# MUTATED: stripped symbol charges from {r['symbol'][:60]}")
                break
    crossable = []
    for r in rows:
        ch = [(c[0], c[1], c[2]) for c in r["charges"]]
        pairs, other = sym_pairs(ch)
        if pairs and not other and len(set(pairs)) == 1:
            crossable.append(r)
    n, b = len(crossable), sum(r["size"] for r in crossable)
    print(f"selftest: replayed {len(rows)} rows from W2-ENGINE's engine top-45 sweep")
    print(f"  derived crossable: {n} rows / {b} B")
    print(f"  W2 reported      : {W2_EXPECT_ROWS} rows / {W2_EXPECT_BYTES} B")
    if n == W2_EXPECT_ROWS and b == W2_EXPECT_BYTES:
        print("PASS -- population and classifier reproduce W2-ENGINE exactly")
        return 0
    print("FAIL -- the rewrite CHANGED THE POPULATION; do not trust a wider sweep")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
