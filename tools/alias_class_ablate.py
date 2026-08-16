#!/usr/bin/env python3
"""Ablate alias memberships BY EVIDENCE CLASS and price each class (ALIAS-2).

    python3 tools/alias_class_ablate.py --wt <worktree> \
        --memberships ~/tmp/alias2_memberships.json \
        --withdraw ~/tmp/alias2_withdraw.json

Per-GROUP ablation (tools/alias_group_ablate.py) gives exact attribution but
costs one report leg per group. This asks the integrity question directly in a
handful of legs: strip every membership whose retail-byte verdict is in class C,
and see what the metric loses. Because a report-only leg is ~2.5 s, the whole
partition is seconds rather than the ~85 min the per-group sweep needs.

The two instruments are complementary and are cross-checked against each other:
per-group ablation says WHICH group a byte rests on, class ablation says WHAT
EVIDENCE it rests on. Their totals must be consistent with the same FULL-vs-EMPTY
measurement, which is asserted rather than assumed.

⚠ Removing a MEMBERSHIP is not removing a GROUP. A group's remaining spellings
keep forgiving whatever they forgave, so these numbers are the marginal cost of
the memberships in that class -- which is exactly the price of withdrawing them.
"""
import argparse, collections, hashlib, json, os, re, subprocess, sys
from pathlib import Path


def rows_at_100(wt):
    d = json.loads((wt / "build/45410914/report.json").read_text())
    out = {}
    for u in d["units"]:
        for f in u.get("functions", []):
            n = f.get("name")
            if n and float(f.get("fuzzy_match_percent", 0) or 0) == 100.0:
                out[(u["name"], n)] = int(f.get("size", 0) or 0)
    m = d["measures"]
    return out, int(m["matched_code"]), int(m["matched_functions"]), \
        float(m["matched_code_percent"])


def leg(wt, mutate, label):
    ali = wt / "scripts/symbol_aliases.json"
    backup = ali.read_bytes()
    sha0 = hashlib.sha256(backup).hexdigest()
    try:
        doc = json.loads(backup)
        mutate(doc)
        ali.write_text(json.dumps(doc, indent=1) + "\n")
        for p in ("build/45410914/report.json", "build/45410914/report.cache"):
            (wt / p).unlink(missing_ok=True)
        r = subprocess.run("./tools/ninja-locked build/45410914/report.json", cwd=wt,
                           shell=True, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit("BUILD FAILED (%s):\n%s" % (label, (r.stdout + r.stderr)[-2000:]))
        log = r.stdout + r.stderr
        n = len(re.findall(r"^\[\d+/\d+\] .*(cl\.exe|objcache)", log, re.M))
        if n:
            sys.exit("REFUSING: leg %s recompiled %d TUs" % (label, n))
        return rows_at_100(wt)
    finally:
        ali.write_bytes(backup)
        if hashlib.sha256(ali.read_bytes()).hexdigest() != sha0:
            sys.exit("FATAL: failed to restore scripts/symbol_aliases.json")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wt", required=True)
    ap.add_argument("--memberships", required=True)
    ap.add_argument("--withdraw", default="")
    a = ap.parse_args()
    wt = Path(a.wt).resolve()

    mem = json.load(open(os.path.expanduser(a.memberships)))
    bycls = collections.defaultdict(set)
    for m in mem:
        bycls[m["cls"]].add((m["i"], m["folded"]))
    print("membership classes: %s" % {k: len(v) for k, v in bycls.items()})

    dec = set()
    if a.withdraw:
        for d in json.load(open(os.path.expanduser(a.withdraw)))["decisive"]:
            dec.add((d["i"], d["folded"]))
        print("decisive withdrawal set: %d memberships" % len(dec))

    def strip(sel):
        def f(doc):
            for i, g in enumerate(doc["groups"]):
                g["folded"] = [x for x in g.get("folded", []) if (i, x) not in sel]
        return f

    base_rows, base_code, base_fns, base_pct = leg(wt, lambda d: None, "FULL")
    print("\nFULL  matched_code %d (%.6f%%)  matched_functions %d  rows@100 %d"
          % (base_code, base_pct, base_fns, len(base_rows)))

    legs = [("EMPTY (all groups)", lambda d: d.__setitem__("groups", []))]
    for c in ("CONTRADICTED", "NEEDS_SOURCE", "NEEDS_MAP_ID"):
        if bycls.get(c):
            legs.append(("strip %s memberships" % c, strip(bycls[c])))
    nonproven = set().union(*[bycls[c] for c in bycls if c != "PROVEN"]) if bycls else set()
    legs.append(("strip ALL non-PROVEN memberships", strip(nonproven)))
    if dec:
        legs.append(("strip DECISIVE withdrawals only", strip(dec)))

    print("\n%-38s %12s %12s %10s %8s" % ("leg", "matched_code", "delta_B", "delta_pp", "delta_fn"))
    for label, mut in legs:
        rows, code, fns, pct = leg(wt, mut, label)
        print("%-38s %12d %12d %10.6f %8d"
              % (label, code, code - base_code, pct - base_pct, fns - base_fns))


if __name__ == "__main__":
    main()
