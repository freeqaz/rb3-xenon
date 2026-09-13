#!/usr/bin/env python3
"""Negative control for the W8-A withdrawal denylist: BREAK IT, AND REQUIRE IT TO FAIL.

A green guard run proves nothing on its own.  This campaign has repeatedly caught
probes that could not have failed -- including a can-fail leg whose regex
substituted a pattern with ITSELF, and the reflinked-worktree lookup that read a
unanimous "refuted" because every name was absent.  So the claim
"`icf_alias_build.py` no longer re-fabricates withdrawn aliases" is only worth
what its negative control is worth.

Each defect below neuters ONE load-bearing part of the guard.  For each, the
generator is re-run and the output must REGROW withdrawn memberships (or, for
the anti-vacuity defects, the run must stop refusing).  A defect that does not
change the outcome is itself a finding: it means that part of the guard was not
carrying anything.

Every mutation asserts that it actually changed the file's bytes -- the
self-substituting-regex failure mode -- and the tree is restored and verified
against `git diff --exit-code` on the way out, including on exception.
"""

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GEN = ROOT / "tools" / "icf_alias_build.py"
MOD = ROOT / "tools" / "alias_withdrawals.py"
LEDGER = ROOT / "scripts" / "symbol_aliases.json"

# (name, file, old, new, expectation)
DEFECTS = [
    ("lookup_always_misses", MOD,
     "        w = self.by_survivor.get((survivor, spelling))",
     "        w = None  # SABOTAGE",
     "every denial silently misses; all withdrawn memberships regrow"),
    ("address_index_dropped", MOD,
     "            if w.address:\n                self.by_address.setdefault((w.address, w.spelling), w)",
     "            if False:  # SABOTAGE\n                self.by_address.setdefault((w.address, w.spelling), w)",
     "address-keyed denials miss; the null-address/renamed-survivor class regrows"),
    ("generation_gate_removed", GEN,
     "        if ledger is not None:\n            _w = ledger.lookup(t, addr_of.get(t), b)",
     "        if False:  # SABOTAGE\n            _w = ledger.lookup(t, addr_of.get(t), b)",
     "generation path re-fabricates; carry-forward still holds its own"),
    ("carry_gate_removed", GEN,
     "        def _wd_denied(g, f):\n            if ledger is None:\n                return False",
     "        def _wd_denied(g, f):\n            if True:  # SABOTAGE\n                return False",
     "--merge launders the pre-existing live-and-withdrawn memberships forward"),
    ("vacuity_floor_removed", MOD,
     "    if len(out) < min_records:",
     "    if False:  # SABOTAGE",
     "an EMPTY ledger no longer refuses -- the guard runs unprotected, silently"),
]


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()[:16]


def apply_defect(d):
    _, path, old, new, _ = d
    src = path.read_text()
    if old not in src:
        raise SystemExit("SABOTAGE ANCHOR MISSING in %s for %s -- the guard was "
                         "refactored and this control is stale. Fix the anchor; do "
                         "NOT delete the control." % (path.name, d[0]))
    if old == new:
        raise SystemExit("SABOTAGE %s substitutes a pattern with ITSELF -- that is "
                         "the exact vacuity this file exists to prevent." % d[0])
    before = sha(path)
    path.write_text(src.replace(old, new, 1))
    after = sha(path)
    assert before != after, "mutation did not change %s" % path.name
    return before, after


def restore():
    subprocess.run(["git", "-C", str(ROOT), "checkout", "--",
                    "tools/icf_alias_build.py", "tools/alias_withdrawals.py"],
                   check=True)
    r = subprocess.run(["git", "-C", str(ROOT), "diff", "--exit-code", "--quiet",
                        "tools/icf_alias_build.py", "tools/alias_withdrawals.py"])
    if r.returncode != 0:
        raise SystemExit("RESTORE FAILED -- the worktree is still sabotaged. Fix by "
                         "hand before doing anything else.")


def exposure(out_path, ledger_path=LEDGER):
    """Withdrawn memberships present as LIVE in a generated file."""
    cur = json.loads(Path(ledger_path).read_text())["groups"]
    wsv, wad = set(), set()
    for g in cur:
        for rec in g.get("withdrawn", []) or []:
            sp = rec if isinstance(rec, str) else rec.get("spelling")
            if not sp:
                continue
            wsv.add((g["survivor"], sp))
            if g.get("address"):
                wad.add((g["address"], sp))
    got = json.loads(Path(out_path).read_text())["groups"]
    return sum(1 for x in got for f in x["folded"]
               if (x["survivor"], f) in wsv or (x.get("address"), f) in wad)


def run_gen(args, out, extra=()):
    cmd = [sys.executable, str(GEN), "--enumerate", "census",
           "--sites", args.sites, "--evidence", args.evidence,
           "--merge", str(LEDGER), "--out", out, *extra]
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.returncode, (p.stdout + p.stderr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sites", required=True)
    ap.add_argument("--evidence", required=True)
    ap.add_argument("--scratch", default=str(Path.home() / "tmp" / "w8a" / "sabotage"))
    ap.add_argument("--baseline-exposure", type=int, required=True,
                    help="withdrawn-memberships-live in the UNGUARDED run; each "
                         "defect is scored against this")
    a = ap.parse_args()
    Path(a.scratch).mkdir(parents=True, exist_ok=True)

    print("=== CONTROL: guard intact ===")
    rc, _ = run_gen(a, a.scratch + "/clean.json")
    clean = exposure(a.scratch + "/clean.json")
    print("  rc=%d  withdrawn-live=%d  (expect 0)" % (rc, clean))
    ok = (rc == 0 and clean == 0)
    print("  %s" % ("PASS" if ok else "FAIL -- the guard is not working; every "
                                      "result below is meaningless"))
    if not ok:
        return 1

    results = []
    for d in DEFECTS:
        name = d[0]
        print("\n=== DEFECT %s ===\n    %s" % (name, d[4]))
        b, aft = apply_defect(d)
        try:
            if name == "vacuity_floor_removed":
                empty = Path(a.scratch) / "empty_ledger.json"
                doc = json.loads(LEDGER.read_text())
                for g in doc["groups"]:
                    g.pop("withdrawn", None)
                empty.write_text(json.dumps(doc))
                rc, log = run_gen(a, a.scratch + "/%s.json" % name,
                                  ("--withdrawals", str(empty)))
                # red == the run no longer REFUSES on an empty ledger
                red = (rc == 0 and "REFUSING" not in log)
                detail = "rc=%d refused=%s" % (rc, "REFUSING" in log)
            else:
                rc, log = run_gen(a, a.scratch + "/%s.json" % name)
                n = exposure(a.scratch + "/%s.json" % name)
                red = (n > 0)
                detail = "withdrawn-live=%d (clean run: 0, unguarded: %d)" % (
                    n, a.baseline_exposure)
        finally:
            restore()
        print("    %s -> %s  %s" % (b + "->" + aft, detail,
                                    "RED (control works)" if red else
                                    "GREEN -- THIS DEFECT DID NOT FAIL"))
        results.append((name, red, detail))

    print("\n=== SUMMARY ===")
    for n, red, det in results:
        print("  %-26s %-5s %s" % (n, "RED" if red else "GREEN", det))
    bad = [n for n, red, _ in results if not red]
    if bad:
        print("\n%d defect(s) did not make the guard fail: %s\n"
              "Either that part of the guard carries nothing, or the control is "
              "vacuous. Both need an answer before the guard is trusted."
              % (len(bad), ", ".join(bad)))
        return 2
    print("\nAll %d defects produced a failure. The guard is load-bearing in "
          "every part this control can reach." % len(results))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BaseException:
        restore()
        raise
