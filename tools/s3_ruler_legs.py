#!/usr/bin/env python3
"""S3-ABLATE: FULL-vs-EMPTY alias ablation at an ARBITRARY ruler, bypassing ninja.

ninja's CHECK RULER AGREEMENT gate (correctly) refuses a flipped objdiff.json, so
this drives the two steps the ablation actually needs -- regenerate the alias map,
regenerate the report -- directly, passing `-c functionRelocDiffs=<ruler>` (which
objdiff-cli applies LAST, so it wins).

    python3 tools/s3_ruler_legs.py <wt> <ruler>

CONTROL: run it at `name_check` and it must reproduce the ninja-path figure
(811,492 B / 7.920118 pp). A bypass that reproduces the known answer is licensed.

Restores scripts/symbol_aliases.json AND build/45410914/icf_aliases.map on every
exit path, verified by sha256.
"""
import hashlib, json, subprocess, sys
from pathlib import Path

WT = Path(sys.argv[1]).resolve()
RULER = sys.argv[2]
CLI = "/home/free/code/milohax/objdiff/target/release/objdiff-cli"
ALI = WT / "scripts/symbol_aliases.json"
MAP = WT / "build/45410914/icf_aliases.map"


def sh(cmd, label):
    r = subprocess.run(cmd, cwd=WT, shell=True, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("FAILED (%s):\n%s" % (label, (r.stdout + r.stderr)[-2500:]))
    return r.stdout + r.stderr


def leg(label, out):
    sh("python3 tools/gen_symbol_alias_map.py --out build/45410914/icf_aliases.map",
       "genmap " + label)
    (WT / "build/45410914/report.cache").unlink(missing_ok=True)
    sh("%s report generate -c functionRelocDiffs=%s -o %s" % (CLI, RULER, out),
       "report " + label)
    d = json.loads((WT / out).read_text())
    m = d["measures"]
    dc = d.get("provenance", {}).get("diff_config")
    if isinstance(dc, list):                       # list of {key, value} records
        dc = {e.get("key"): e.get("value") for e in dc if isinstance(e, dict)}
    return (int(m["matched_code"]), int(m["matched_functions"]),
            float(m["matched_code_percent"]), int(m["masked_equal_functions"]),
            (dc or {}).get("functionRelocDiffs"))


ali_b, map_b = ALI.read_bytes(), MAP.read_bytes()
ali_s, map_s = hashlib.sha256(ali_b).hexdigest(), hashlib.sha256(map_b).hexdigest()
try:
    full = leg("FULL", "build/45410914/s3_full.json")
    doc = json.loads(ali_b)
    for g in doc["groups"]:
        g["folded"] = []
    ALI.write_text(json.dumps(doc, indent=1) + "\n")
    empty = leg("EMPTY", "build/45410914/s3_empty.json")

    print("=== ruler requested %s ; report provenance FULL=%s EMPTY=%s ==="
          % (RULER, full[4], empty[4]))
    print("FULL   matched_code %d (%.6f%%)  matched_functions %d  masked_equal %d"
          % (full[0], full[2], full[1], full[3]))
    print("EMPTY  matched_code %d (%.6f%%)  matched_functions %d  masked_equal %d"
          % (empty[0], empty[2], empty[1], empty[3]))
    print("DELTA  %+d B / %+.6f pp   matched_functions %+d   masked_equal %+d"
          % (full[0] - empty[0], full[2] - empty[2], full[1] - empty[1], full[3] - empty[3]))
finally:
    ALI.write_bytes(ali_b)
    MAP.write_bytes(map_b)
    ok = (hashlib.sha256(ALI.read_bytes()).hexdigest() == ali_s
          and hashlib.sha256(MAP.read_bytes()).hexdigest() == map_s)
    for p in ("build/45410914/s3_full.json", "build/45410914/s3_empty.json"):
        (WT / p).unlink(missing_ok=True)
    print("restored alias json + icf_aliases.map: %s" % ("sha ok" if ok else "FAILED"))
    if not ok:
        sys.exit("FATAL: restore failed")
