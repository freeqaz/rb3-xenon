#!/usr/bin/env python3
"""S3-ABLATE: re-run the alias ablation on the `none` ruler.

`none` ignores relocation NAMES. If alias forgiveness works through
relocation-name comparison, the FULL-vs-EMPTY delta must be EXACTLY 0 here --
by construction. A nonzero delta means the aliases move the score through some
channel other than relocation names, which is the explanation for mpn moving.

Restores BOTH objdiff.json and scripts/symbol_aliases.json on every exit path,
verified by sha256.
"""
import hashlib, json, re, subprocess, sys
from pathlib import Path

WT = Path(sys.argv[1]).resolve()
RULER = sys.argv[2] if len(sys.argv) > 2 else "none"


def measures(wt):
    d = json.loads((wt / "build/45410914/report.json").read_text())
    m = d["measures"]
    prov = d.get("provenance", {}).get("diff_config", {})
    return (int(m["matched_code"]), int(m["matched_functions"]),
            float(m["matched_code_percent"]), int(m["masked_equal_functions"]),
            prov.get("functionRelocDiffs"))


def run(label):
    for p in ("build/45410914/report.json", "build/45410914/report.cache"):
        (WT / p).unlink(missing_ok=True)
    r = subprocess.run("./tools/ninja-locked build/45410914/report.json", cwd=WT,
                       shell=True, capture_output=True, text=True)
    log = r.stdout + r.stderr
    if r.returncode != 0:
        sys.exit("BUILD FAILED (%s):\n%s" % (label, log[-3000:]))
    n = len(re.findall(r"^\[\d+/\d+\] .*(cl\.exe|objcache)", log, re.M))
    if n:
        sys.exit("REFUSING: leg %s recompiled %d TUs" % (label, n))
    return measures(WT)


oj, ali = WT / "objdiff.json", WT / "scripts/symbol_aliases.json"
oj_b, ali_b = oj.read_bytes(), ali.read_bytes()
oj_s, ali_s = hashlib.sha256(oj_b).hexdigest(), hashlib.sha256(ali_b).hexdigest()
try:
    d = json.loads(oj_b)
    d.setdefault("options", {})["functionRelocDiffs"] = RULER
    oj.write_text(json.dumps(d, indent=2) + "\n")

    full = run("FULL@" + RULER)
    doc = json.loads(ali_b)
    for g in doc["groups"]:
        g["folded"] = []
    ali.write_text(json.dumps(doc, indent=1) + "\n")
    empty = run("EMPTY@" + RULER)

    print("ruler requested=%s   report provenance says: FULL=%s EMPTY=%s"
          % (RULER, full[4], empty[4]))
    print("FULL   matched_code %d (%.6f%%)  matched_functions %d  masked_equal %d"
          % (full[0], full[2], full[1], full[3]))
    print("EMPTY  matched_code %d (%.6f%%)  matched_functions %d  masked_equal %d"
          % (empty[0], empty[2], empty[1], empty[3]))
    print("DELTA  %+d B / %+.6f pp   matched_functions %+d   masked_equal %+d"
          % (full[0] - empty[0], full[2] - empty[2], full[1] - empty[1], full[3] - empty[3]))
finally:
    oj.write_bytes(oj_b); ali.write_bytes(ali_b)
    ok = (hashlib.sha256(oj.read_bytes()).hexdigest() == oj_s
          and hashlib.sha256(ali.read_bytes()).hexdigest() == ali_s)
    print("restored objdiff.json + symbol_aliases.json: %s" % ("sha ok" if ok else "FAILED"))
    if not ok:
        sys.exit("FATAL: restore failed")
