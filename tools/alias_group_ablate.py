#!/usr/bin/env python3
"""EXACT per-group attribution of alias forgiveness, by ablation.

    python3 tools/alias_group_ablate.py --wt <worktree> --out ~/tmp/ablate.jsonl

WHY IT EXISTS (lane ALIAS-2, 2026-08-16)
----------------------------------------
GROUNDED-1 attributed the forgiven bytes to alias PAIRS using icf_site_census's
name-keyed charged-pair records, and 11.00% / 79,288 B came back UNATTRIBUTED --
rows whose target-side symbol is anonymous (`fn_8277C1AC`), which a name-keyed
census structurally cannot pair.  That is a gap in the INSTRUMENT, not a
different kind of evidence.

This tool removes the instrument.  A report-only leg costs ~2.5 s (measured), so
ablating ONE group at a time and diffing the fall set attributes every forgiven
byte to the group responsible BY CONSTRUCTION -- no name matching anywhere, so
an anonymous row is attributed exactly as well as a named one.

WHAT IT MEASURES -- necessity, which is the right notion for integrity
---------------------------------------------------------------------
For each group g, the rows that drop below fuzzy==100 when ONLY g is removed are
the rows that NEED g.  A row needing two groups is reported under both; the
per-group byte columns therefore OVERLAP and MUST NOT be summed to a total (that
is checked, not assumed -- see `--verify`).  Necessity is what integrity turns
on: a row's forgiveness is legitimate only if EVERY group it depends on is
proven, so the union of a row's necessary groups is the object to adjudicate.

⚠ A group whose ablation drops NOTHING is not thereby useless: it may be
redundant with another group on the same site, or (the documented case) a
STALE_SPELLING / UNWITNESSED group that becomes live as porting advances.
Pruning on a zero here is exactly the change `a745039e` had to reverse at
+94,616 B.  This tool reports; it never prunes.

REFUSALS
--------
* any leg that recompiles a TU (the probe is licensed only for report-only legs)
* failure to restore scripts/symbol_aliases.json byte-for-byte, on every path
"""
import argparse, hashlib, json, os, re, subprocess, sys, time
from pathlib import Path


def rows_at_100(wt):
    d = json.loads((wt / "build/45410914/report.json").read_text())
    out = {}
    for u in d["units"]:
        for f in u.get("functions", []):
            n = f.get("name")
            if not n:
                continue
            if float(f.get("fuzzy_match_percent", 0) or 0) == 100.0:
                out[(u["name"], n)] = int(f.get("size", 0) or 0)
    m = d["measures"]
    return out, int(m["matched_code"]), int(m["matched_functions"])


def run_report(wt, label):
    for p in ("build/45410914/report.json", "build/45410914/report.cache"):
        (wt / p).unlink(missing_ok=True)
    r = subprocess.run("./tools/ninja-locked build/45410914/report.json", cwd=wt,
                       shell=True, capture_output=True, text=True)
    log = r.stdout + r.stderr
    if r.returncode != 0:
        sys.exit("BUILD FAILED (%s):\n%s" % (label, log[-3000:]))
    n = len(re.findall(r"^\[\d+/\d+\] .*(cl\.exe|objcache)", log, re.M))
    if n:
        sys.exit("REFUSING: leg %s recompiled %d TUs -- report-only legs only." % (label, n))
    return rows_at_100(wt)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wt", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--start", type=int, default=0)
    ap.add_argument("--limit", type=int, default=0)
    a = ap.parse_args()

    wt = Path(a.wt).resolve()
    ali = wt / "scripts/symbol_aliases.json"
    backup = ali.read_bytes()
    sha0 = hashlib.sha256(backup).hexdigest()
    doc = json.loads(backup)
    groups = doc["groups"]

    outp = Path(os.path.expanduser(a.out))
    done = set()
    if outp.exists():
        for line in outp.read_text().splitlines():
            if line.strip():
                done.add(json.loads(line)["i"])
        print("resuming: %d groups already done" % len(done))

    try:
        base_rows, base_code, base_fns = run_report(wt, "FULL")
        print("FULL baseline: %d rows at fuzzy==100, matched_code %d, matched_functions %d"
              % (len(base_rows), base_code, base_fns))

        idxs = range(a.start, len(groups) if not a.limit else min(len(groups), a.start + a.limit))
        t0 = time.time()
        with outp.open("a") as fh:
            for c, i in enumerate(idxs):
                if i in done:
                    continue
                g = groups[i]
                doc["groups"] = groups[:i] + groups[i + 1:]
                ali.write_text(json.dumps(doc, indent=1) + "\n")
                rows, code, fns = run_report(wt, "ablate#%d" % i)
                fell = [["\t".join(k), v] for k, v in base_rows.items() if k not in rows]
                rec = {"i": i,
                       "survivor": g.get("survivor"),
                       "name": g.get("name"),
                       "address": g.get("address"),
                       "n_folded": len(g.get("folded", [])),
                       "evidence": g.get("evidence"),
                       "withdrawn": bool(g.get("withdrawn")),
                       "d_code": base_code - code,
                       "d_fns": base_fns - fns,
                       "n_fell": len(fell),
                       "fell_bytes": sum(v for _, v in fell),
                       "fell": fell}
                fh.write(json.dumps(rec) + "\n")
                fh.flush()
                if c % 25 == 0:
                    el = time.time() - t0
                    print("[%d/%d] i=%d %s  fell=%d (%d B)  %.1fs elapsed"
                          % (c, len(idxs), i, (g.get("name") or "?")[:40],
                             len(fell), sum(v for _, v in fell), el), flush=True)
    finally:
        ali.write_bytes(backup)
        if hashlib.sha256(ali.read_bytes()).hexdigest() != sha0:
            sys.exit("FATAL: failed to restore scripts/symbol_aliases.json")
        print("restored scripts/symbol_aliases.json (sha ok)")


if __name__ == "__main__":
    main()
