#!/usr/bin/env python3
"""Classify every near-miss function in a set of units by its CHARGED sites.

Prices from report.json's charged-site list, at the SHIPPED ruler, never from a
`none`-ruler mismatch count (CLAUDE.md: the count undercounts and manufactures
phantom prizes). Emits one row per function and a class tally, so a lane can see
whether its residual is source-fixable or an alias/map question before spending.
"""
import argparse, gzip, json, re, subprocess, sys
from pathlib import Path

MISMATCH_ROW = re.compile(r"^\|\s*(\d+)\s*\|\s*`([^`]*)`\s*\|\s*`([^`]*)`\s*\|\s*(\w+)\s*\|")
PATTERN_ROW = re.compile(r"^- \*\*([A-Z_]+)\*\* \((\w+)\)")
UNATTRIB = re.compile(r"\*\*Unattributed mismatches\*\*:\s*(\d+)")


def load_report(path):
    op = gzip.open if str(path).endswith(".gz") else open
    with op(path, "rt") as fh:
        return json.load(fh)


def near_misses(report, units):
    out = []
    for u in report["units"]:
        if u["name"] not in units:
            continue
        for f in u["functions"]:
            p = f.get("match_percent_normalized", 0.0)
            if 0.0 < p < 100.0:
                out.append((u["name"], f["name"], int(f["size"]), p,
                            f.get("metadata", {}).get("demangled_name", f["name"])))
    out.sort(key=lambda r: -r[3])
    return out


def diff_one(project, unit, symbol, ruler):
    cmd = [str(Path(project) / "bin/objdiff-cli"), "diff", "-p", str(project),
           "-u", unit, "-c", f"functionRelocDiffs={ruler}",
           "--include-instructions", "--analyze", symbol]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        return None, r.stderr.strip()[:200]
    sites, pats, unattributed = [], [], None
    for line in r.stdout.splitlines():
        m = MISMATCH_ROW.match(line)
        if m and m.group(1).isdigit():
            sites.append({"index": int(m.group(1)), "target": m.group(2),
                          "base": m.group(3), "kind": m.group(4)})
        m = PATTERN_ROW.match(line)
        if m:
            pats.append(m.group(1))
        m = UNATTRIB.search(line)
        if m:
            unattributed = int(m.group(1))
    return {"sites": sites, "patterns": sorted(set(pats)),
            "unattributed": unattributed}, None


def classify(rec):
    """One label per function, from its charged sites. Order matters: a single
    unattributed mismatch is the only thing a source edit can reach directly."""
    if rec is None:
        return "ERROR"
    if not rec["sites"]:
        return "NO_CHARGED_SITE"
    if rec["unattributed"]:
        return "UNATTRIBUTED"
    kinds = {s["kind"] for s in rec["sites"]}
    if "LINKER_MERGED" in rec["patterns"] and kinds == {"diff_arg"}:
        return "ICF_ALIAS_GAP"
    if kinds == {"diff_arg"}:
        return "DIFF_ARG_OTHER"
    return "STRUCTURAL"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", required=True)
    ap.add_argument("--report", required=True)
    ap.add_argument("--units", required=True, help="comma-separated unit names")
    ap.add_argument("--ruler", default="name_check")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    units = set(a.units.split(","))
    rows = near_misses(load_report(a.report), units)
    print(f"{len(rows)} near-miss functions across {len(units)} units "
          f"(ruler={a.ruler})", file=sys.stderr)

    results, tally = [], {}
    for unit, sym, size, pct, demangled in rows:
        rec, err = diff_one(a.project, unit, sym, a.ruler)
        label = classify(rec)
        tally[label] = tally.get(label, 0) + 1
        results.append({"unit": unit, "symbol": sym, "demangled": demangled,
                        "size": size, "match_percent": pct, "label": label,
                        "error": err, **(rec or {})})
        n = len(rec["sites"]) if rec else -1
        print(f"  {pct:8.4f} {size:5d}B {label:16s} sites={n:2d} {demangled[:70]}",
              file=sys.stderr)

    Path(a.out).write_text(json.dumps(
        {"ruler": a.ruler, "units": sorted(units), "tally": tally,
         "functions": results}, indent=1))
    print("\ntally:", json.dumps(tally), file=sys.stderr)


if __name__ == "__main__":
    main()
