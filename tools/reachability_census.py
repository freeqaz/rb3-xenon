#!/usr/bin/env python3
"""Where does the remaining byte value ACTUALLY live?  Re-derive it; never inherit it.

Three rounds of this campaign were aimed off briefed "prizes" that turned out
unreachable, every time because a CHARGE CLASS was relayed as a DIAGNOSIS:

  * "COLLECTABLE"  -> reachable pool was 1,920 B, not 21,636 B (11x error)
  * "NAME-BLOCKED" -> 0 of 42 rows could cross; 32.9% was __savegprlr/__restgprlr,
                      a register COUNT wearing a symbol NAME
  * UpdateScrolling -> "618 diffs + 12 charges" understated the price 2.2x

So this tool prints a third column that those briefs omitted: whether a SOURCE
LEVER exists, separately from what the charges look like.

RULES IT IS BUILT ON (each learned expensively):
  * `matched_code` keys on `fuzzy == 100` and is ALL-OR-NOTHING per row, so rank
    by SIZE-IF-IT-CROSSES, never by penalty count, and a row's prize is
    uncollectable unless EVERY charged site can close.
  * Only a BARE `arg:{Symbol}` is a real relocation-name charge.  A diff_arg
    where a Register also differs is charged BY THE REGISTER.  Naive counting
    once read 138 name charges where the truth was ZERO.
  * Permuter is OFF, so `reg > 0` WALLS a row.  That single filter is what
    collapsed one queue from 21,636 B to 1,920 B.
  * Scores on the ruler resolved from report.json's own provenance, never
    hardcoded (the shipped ruler is name_check since 2026-08-12).
  * Reads total_code/total_functions FROM report.json and int()-coerces every
    numeric -- several are JSON *strings*, and un-coerced a size filter returns
    a clean, decisive-looking `0 rows`.

SELF-VALIDATION (three lanes did this and each caught a defect the raw run
reported cleanly): rows must sum to total_functions and bytes to total_code
EXACTLY, matched_code must reproduce as sum(size | fuzzy==100), and
matched_functions as count(mpn==100).  It exits 1 if any of those disagree.

⛔ A fresh worktree's reflinked target objs are PRE-RENAMER, so every retail
mangled name reads ABSENT until you build -- silently, and the failure agrees
with your prior.  BUILD FIRST.

Usage:
    python3 tools/reachability_census.py [project_dir] [--charges] [--top N]

    --charges  also run objdiff over every named row with 0 < fuzzy < 100 and
               classify its charges (slow: ~700 unit invocations).
"""
import argparse
import collections
import json
import subprocess
import sys
from pathlib import Path

PLACEHOLDER = ("fn_", "lbl_", "jumptable_", "data_", "bss_", "rdata_")


def I(x, d=0):
    return d if x is None else int(x)


def F(x, d=0.0):
    return d if x is None else float(x)


def load_rows(proj):
    rep = json.load(open(Path(proj) / "build/45410914/report.json"))
    m = rep["measures"]
    rows = []
    for u in rep["units"]:
        md = u.get("metadata") or {}
        for f in (u.get("functions") or []):
            rows.append(dict(
                unit=u["name"],
                auto=bool(md.get("auto_generated", False)),
                src=md.get("source_path"),
                name=f.get("name", ""),
                size=I(f.get("size")),
                fuzzy=F(f.get("fuzzy_match_percent")),
                mpn=F(f.get("match_percent_normalized")),
            ))
    return rep, m, rows


def self_validate(m, rows):
    totf, totc = I(m["total_functions"]), I(m["total_code"])
    mf, mc = I(m["matched_functions"]), I(m["matched_code"])
    d_rows, d_bytes = len(rows), sum(r["size"] for r in rows)
    d_mc = sum(r["size"] for r in rows if r["fuzzy"] == 100.0)
    d_mf = sum(1 for r in rows if r["mpn"] == 100.0)
    checks = [("rows == total_functions", d_rows, totf),
              ("bytes == total_code", d_bytes, totc),
              ("sum(size|fuzzy==100) == matched_code", d_mc, mc),
              ("count(mpn==100) == matched_functions", d_mf, mf)]
    print("=== SELF-VALIDATION (zero rows may be dropped) ===")
    ok = True
    for lbl, got, want in checks:
        good = got == want
        ok &= good
        print(f"  {lbl:<42} {got:>12,} vs {want:>12,}  {'OK' if good else 'MISMATCH'}")
    return ok, totf, totc, mc


def profile(rec):
    """Classify every charged instruction in one objdiff symbol record."""
    hard = name = reg = imm = br = 0
    kinds = collections.Counter()
    for i in rec.get("instructions") or []:
        mt = i.get("match_type")
        if mt == "equal":
            continue
        if mt != "diff_arg":
            hard += 1
            kinds[mt] += 1
            continue
        ta = (i.get("target") or {}).get("typed_args") or []
        ba = (i.get("base") or {}).get("typed_args") or []
        ks = {x.get("type") for x, y in zip(ta, ba) if x.get("value") != y.get("value")}
        if ks == {"Symbol"}:
            name += 1          # BARE Symbol -> a real relocation-name charge
        elif "Register" in ks:
            reg += 1           # charged BY THE REGISTER, not by the name
        elif "BranchDest" in ks:
            br += 1
        else:
            imm += 1
    return hard, name, reg, imm, br, kinds


def verdict(r):
    """ALL-OR-NOTHING: the row crosses only if EVERY charge class can close."""
    if r["reg"] > 0:
        return "WALLED_REG (permuter OFF)"
    if r["name"] > 0:
        return "NAME_ADJUDICATION"
    if r["hard"] or r["imm"] or r["br"]:
        return "SOURCE_LEVER"
    return "NO CHARGES SEEN (?)"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("project", nargs="?", default=".")
    ap.add_argument("--charges", action="store_true")
    ap.add_argument("--top", type=int, default=25)
    a = ap.parse_args()
    proj = Path(a.project).resolve()

    rep, m, rows = load_rows(proj)
    ok, totf, totc, mc = self_validate(m, rows)
    if not ok:
        print("!! census self-validation FAILED -- downstream numbers are untrustworthy")
        return 1
    gap = totc - mc
    print(f"\n=== THE GAP === matched_code {mc:,} / total_code {totc:,} "
          f"({100*mc/totc:.4f}%)   GAP {gap:,} B\n")

    def ph(n):
        return n.startswith(PLACEHOLDER)

    strata = collections.OrderedDict()
    for r in rows:
        if r["fuzzy"] >= 100.0:
            continue
        if r["fuzzy"] == 0.0:
            if ph(r["name"]):
                k = "IDENTIFICATION-BLOCKED (placeholder name, cannot pair)"
            elif r["src"] is None:
                k = "NO SOURCE (xdk/vendor -- out of scope)"
            else:
                k = "NAMED, 0% (paired name but no credit)"
        else:
            k = "PARTIAL (0<fuzzy<100)  <- the only credited residual"
        strata.setdefault(k, []).append(r)

    print("=== GAP PARTITION (size-if-it-crosses) ===")
    print(f"{'STRATUM':<56}{'rows':>7}{'bytes':>13}{'%gap':>8}{'%total':>8}")
    for k, v in sorted(strata.items(), key=lambda kv: -sum(r["size"] for r in kv[1])):
        b = sum(r["size"] for r in v)
        print(f"{k:<56}{len(v):>7}{b:>13,}{100*b/gap:>7.2f}%{100*b/totc:>7.3f}%")

    if not a.charges:
        print("\n(pass --charges to classify the PARTIAL stratum by charge class)")
        return 0

    # ---- charge-classify every NAMED row with 0 < fuzzy < 100 ----
    sys.path.insert(0, str(proj))
    from scripts.analysis import ruler as ruler_mod
    rk = ruler_mod.resolve_ruler(proj)
    print("\n== ruler ==\n" + rk.banner())

    want = {(r["unit"], r["name"]): r for r in rows
            if 0.0 < r["fuzzy"] < 100.0 and not ph(r["name"])}
    print(f"named partial rows: {len(want)} / {sum(r['size'] for r in want.values()):,} B")
    cli = str(proj / "bin/objdiff-cli")
    out, failed = [], []
    for uname in sorted({k[0] for k in want}):
        syms = [k[1] for k in want if k[0] == uname]
        cmd = [cli, "diff", "-p", str(proj), "-u", uname, "--batch", "-f", "json",
               "-o", "-", "--include-instructions"] + rk.args
        try:
            p = subprocess.run(cmd, capture_output=True, text=True, timeout=900,
                               input="\n".join(syms) + "\n")
        except subprocess.TimeoutExpired:
            failed.append(uname)
            continue
        if p.returncode != 0 or not p.stdout.strip():
            failed.append(uname)
            continue
        try:
            j = json.loads(p.stdout)
            recs = j if isinstance(j, list) else [j]
        except json.JSONDecodeError:
            recs = []
            for line in p.stdout.splitlines():
                if line.strip():
                    try:
                        recs.append(json.loads(line))
                    except json.JSONDecodeError:
                        pass
        for rec in recs:
            key = (uname, rec.get("symbol") or rec.get("name") or "")
            if key not in want or rec.get("error"):
                continue
            h, n, g, im, b, kinds = profile(rec)
            r = dict(want[key])
            r.update(hard=h, name_chg=n, reg=g, imm=im, br=b, kinds=dict(kinds))
            r["reg"], r["name"] = g, n  # verdict() reads these
            out.append(r)

    # coverage: a silently dropped row would UNDERSTATE charges
    got = {(r["unit"], r["name"]) for r in out}
    miss = [v for k, v in want.items() if k not in got]
    print(f"\n== COVERAGE == profiled {len(out)}/{len(want)} rows, "
          f"MISSING {len(miss)} ({sum(r['size'] for r in miss):,} B), "
          f"failed units {len(failed)}")
    if miss:
        print("  !! a silent drop understates charges -- treat results as a LOWER bound")

    agg_b, agg_n = collections.Counter(), collections.Counter()
    for r in out:
        v = verdict(r)
        agg_b[v] += r["size"]
        agg_n[v] += 1
    tot = sum(r["size"] for r in out)
    print(f"\n=== CHARGE CLASSES over the named partial stratum ({tot:,} B) ===")
    print(f"{'VERDICT':<28}{'rows':>7}{'size-if-crosses':>17}{'%strat':>8}{'%gap':>8}")
    for v, b in agg_b.most_common():
        print(f"{v:<28}{agg_n[v]:>7}{b:>15,} B{100*b/tot:>7.2f}%{100*b/gap:>7.2f}%")

    S = sorted([r for r in out if verdict(r) == "SOURCE_LEVER"], key=lambda r: -r["size"])
    print(f"\n=== SOURCE_LEVER: {len(S)} rows / {sum(r['size'] for r in S):,} B "
          f"({100*sum(r['size'] for r in S)/gap:.2f}% of the gap) ===")
    print("⚠ SOURCE_LEVER IS A CHARGE CLASS, NOT A PROMISE. Check the unit's own")
    print("  .cpp for a drained record before opening a row -- the largest member")
    print("  of this class has historically been refuted by ~22 measured spellings.")
    print(f"{'SIZE':>7}{'fuzzy':>10}{'chg':>5}  SYMBOL")
    for r in S[:a.top]:
        print(f"{r['size']:>7}{r['fuzzy']:>10.4f}"
              f"{r['hard']+r['imm']+r['br']:>5}  {r['name'][:64]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
