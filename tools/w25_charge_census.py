#!/usr/bin/env python3
"""W25-UI collectability census over the UI cluster.

Establishes, BEFORE any row is opened, whether source work alone can collect
a row's bytes. Reuses lane W19's discriminator:

  * a charged instruction with match_type != 'diff_arg'  -> a HARD (instruction)
    mismatch: ordinary source work.
  * a 'diff_arg' whose differing typed_args are EXACTLY {Symbol} -> a real
    relocation-NAME charge (wrong callee, or an ICF fold-alias). Source work
    alone may not be able to close it.
  * a 'diff_arg' where a Register also differs -> charged BY THE REGISTER, not
    by the name. Naive counting reads these as name charges; W19 measured a row
    whose "138 name charges" were truly ZERO.

Scores on the ruler resolved from report.json's own provenance, never hardcoded.
"""
import collections
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from scripts.analysis import ruler as ruler_mod  # noqa: E402


def I(x, d=0):
    return d if x is None else int(x)


def F(x, d=0.0):
    return d if x is None else float(x)


def profile(rec):
    """Classify every charged instruction in one objdiff symbol record."""
    hard = namechg = regchg = immchg = brchg = 0
    pairs = collections.Counter()
    for i in rec.get("instructions", []) or []:
        mt = i.get("match_type")
        if mt == "equal":
            continue
        if mt != "diff_arg":
            hard += 1
            continue
        t = i.get("target") or {}
        b = i.get("base") or {}
        ta = t.get("typed_args", []) or []
        ba = b.get("typed_args", []) or []
        kinds = set()
        sp = None
        for x, y in zip(ta, ba):
            if x.get("value") != y.get("value"):
                kinds.add(x.get("type"))
                if x.get("type") == "Symbol":
                    sp = (x.get("value"), y.get("value"))
        if kinds == {"Symbol"}:
            namechg += 1
            pairs[sp] += 1
        elif "Register" in kinds:
            regchg += 1
        elif "BranchDest" in kinds:
            brchg += 1
        else:
            immchg += 1
    return hard, namechg, regchg, immchg, brchg, pairs


def main():
    proj = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
    pattern = sys.argv[2] if len(sys.argv) > 2 else "UI"
    rk = ruler_mod.resolve_ruler(proj)
    print("== ruler ==")
    print(rk.banner())
    print()

    rep = json.load(open(proj / "build/45410914/report.json"))
    units = [u for u in rep["units"] if pattern in u.get("name", "")]

    # Only rows with a real residual AND a real name are workable at all.
    want = {}
    for u in units:
        for f in (u.get("functions") or []):
            fz = F(f.get("fuzzy_match_percent"))
            nm = f.get("name", "")
            if fz >= 100.0 or fz <= 0.0:
                continue
            if nm.startswith(("fn_", "lbl_")):
                continue
            want[(u["name"], nm)] = (I(f.get("size")), fz,
                                     F(f.get("match_percent_normalized")))
    print(f"named rows with a real residual: {len(want)}  "
          f"({sum(v[0] for v in want.values())} B)")
    print()

    cli = str(proj / "bin/objdiff-cli")
    rows = []
    notfound = []
    for u in units:
        uname = u["name"]
        if not any(k[0] == uname for k in want):
            continue
        syms = [k[1] for k in want if k[0] == uname]
        cmd = [cli, "diff", "-p", str(proj), "-u", uname, "--batch",
               "-f", "json", "-o", "-", "--include-instructions"] + rk.args
        try:
            out = subprocess.run(cmd, capture_output=True, text=True,
                                 timeout=900, input="\n".join(syms) + "\n")
        except subprocess.TimeoutExpired:
            print(f"  TIMEOUT on {uname}", file=sys.stderr)
            continue
        if out.returncode != 0:
            print(f"  FAILED {uname}: {out.stderr[:200]}", file=sys.stderr)
            continue
        txt = out.stdout.strip()
        if not txt:
            continue
        recs = []
        try:
            j = json.loads(txt)
            recs = j if isinstance(j, list) else [j]
        except json.JSONDecodeError:
            for line in txt.splitlines():
                line = line.strip()
                if line:
                    try:
                        recs.append(json.loads(line))
                    except json.JSONDecodeError:
                        pass
        for r in recs:
            sym = r.get("symbol") or r.get("name") or ""
            key = (uname, sym)
            if key not in want:
                continue
            if r.get("error"):
                notfound.append((want[key][0], uname, sym, r["error"]))
                continue
            sz, fz, mpn = want[key]
            h, n, g, im, br, p = profile(r)
            rows.append((sz, fz, mpn, h, n, g, im, br, uname, sym, p))

    # ---- coverage: a silently-dropped row would understate name charges ----
    got = {(r[8], r[9]) for r in rows}
    missing = [(v[0], k[0], k[1]) for k, v in want.items() if k not in got]
    print("== census coverage (a silent drop would understate charges) ==")
    print(f"  requested: {len(want)} rows / {sum(v[0] for v in want.values())} B")
    print(f"  profiled : {len(rows)} rows / {sum(r[0] for r in rows)} B")
    print(f"  NOT profiled: {len(missing)} rows / "
          f"{sum(m[0] for m in missing)} B")
    for sz, un, sym in sorted(missing, reverse=True)[:10]:
        print(f"     {sz:>6}  {un:<26} {sym[:56]}")
    print()

    rows.sort(reverse=True)
    print("== per-row charge profile (named rows, fuzzy>0), by SIZE ==")
    print(f"{'SIZE':>6} {'FUZZY':>9} {'hard':>5} {'NAME':>5} {'reg':>4} "
          f"{'imm':>4} {'br':>4}  {'VERDICT':<12} SYMBOL")
    print("-" * 122)
    coll_b = coll_r = blk_b = blk_r = 0
    for sz, fz, mpn, h, n, g, im, br, un, sym, p in rows:
        if n == 0:
            verdict = "COLLECTABLE"
            coll_b += sz
            coll_r += 1
        else:
            verdict = "name-charged"
            blk_b += sz
            blk_r += 1
        print(f"{sz:>6} {fz:>9.4f} {h:>5} {n:>5} {g:>4} {im:>4} {br:>4}  "
              f"{verdict:<12} {sym[:56]}")
    print()
    print(f"  COLLECTABLE by source work alone: {coll_r} rows, {coll_b} B")
    print(f"  needs a name/map adjudication too: {blk_r} rows, {blk_b} B")

    print()
    print("== most frequent charged NAME pairs (target -> ours) ==")
    agg = collections.Counter()
    for *_, p in rows:
        agg.update(p)
    for (tgt, ours), c in agg.most_common(18):
        print(f"  x{c:<3} {str(tgt)[:56]}")
        print(f"        ours: {str(ours)[:56]}")


if __name__ == "__main__":
    main()
