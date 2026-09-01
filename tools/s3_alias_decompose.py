#!/usr/bin/env python3
"""S3-ABLATE: decompose the alias-forgiveness ablation by CHANNEL.

The 08-16 measurement had matched_functions +0 -- the arg-blind signature. Today
it moves. This splits the FULL-vs-EMPTY fall set into:

  ARG_ONLY  fuzzy fell below 100 but mpn stayed 100   (relocation-name arg charge)
  MPN_TOO   mpn ALSO fell below 100                   (not an arg-only penalty)
  UNPAIRED  the row lost its base symbol entirely     (the PAIRING channel)

Same safe leg protocol as tools/alias_forgiveness_audit.py: wipe report.json +
report.cache, report-only build, REFUSE on any compile, restore the alias file
byte-for-byte on every exit path.
"""
import hashlib, json, re, subprocess, sys
from pathlib import Path

WT = Path(sys.argv[1]).resolve()


def rows(wt):
    d = json.loads((wt / "build/45410914/report.json").read_text())
    out = {}
    for u in d["units"]:
        for f in u.get("functions", []):
            n = f.get("name")
            if not n:
                continue
            out[(u["name"], n)] = (
                float(f.get("fuzzy_match_percent", 0) or 0),
                float(f.get("match_percent_normalized", 0) or 0),
                int(f.get("size", 0) or 0),
            )
    m = d["measures"]
    return out, {k: (int(m[k]) if k != "matched_code_percent" else float(m[k]))
                 for k in ("matched_code", "matched_functions", "matched_code_percent",
                           "masked_equal_functions", "total_code", "total_functions")}


def leg(keep, label):
    ali = WT / "scripts/symbol_aliases.json"
    backup = ali.read_bytes()
    sha0 = hashlib.sha256(backup).hexdigest()
    try:
        if not keep:
            doc = json.loads(backup)
            # EMPTY the groups' folded lists -- do NOT prune the groups (a prune
            # cost +94,616 B to reverse). Equivalent forgiveness, no data loss.
            for g in doc["groups"]:
                g["folded"] = []
            ali.write_text(json.dumps(doc, indent=1) + "\n")
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
        return rows(WT)
    finally:
        ali.write_bytes(backup)
        if hashlib.sha256(ali.read_bytes()).hexdigest() != sha0:
            sys.exit("FATAL: failed to restore scripts/symbol_aliases.json")


fr, fm = leg(True, "FULL")
er, em = leg(False, "EMPTY(folded=[])")

print("FULL   matched_code %d (%.6f%%)  matched_functions %d  masked_equal %d"
      % (fm["matched_code"], fm["matched_code_percent"], fm["matched_functions"],
         fm["masked_equal_functions"]))
print("EMPTY  matched_code %d (%.6f%%)  matched_functions %d  masked_equal %d"
      % (em["matched_code"], em["matched_code_percent"], em["matched_functions"],
         em["masked_equal_functions"]))
print("DELTA  %d B / %.6f pp   matched_functions %+d   masked_equal %+d"
      % (fm["matched_code"] - em["matched_code"],
         fm["matched_code_percent"] - em["matched_code_percent"],
         fm["matched_functions"] - em["matched_functions"],
         fm["masked_equal_functions"] - em["masked_equal_functions"]))
print("total_code %d  total_functions %d (FULL) / %d %d (EMPTY)"
      % (fm["total_code"], fm["total_functions"], em["total_code"], em["total_functions"]))

fell, chan = [], {"ARG_ONLY": [0, 0], "MPN_TOO": [0, 0], "GONE_FROM_REPORT": [0, 0]}
for k, (fz, mp, sz) in fr.items():
    if fz != 100.0:
        continue
    e = er.get(k)
    if e is None:
        chan["GONE_FROM_REPORT"][0] += 1; chan["GONE_FROM_REPORT"][1] += sz
        fell.append((k, sz, "GONE_FROM_REPORT")); continue
    if e[0] < 100.0:
        c = "ARG_ONLY" if e[1] == 100.0 else "MPN_TOO"
        chan[c][0] += 1; chan[c][1] += sz
        fell.append((k, sz, c))

print("\nrows that fell: %d, totalling %d B -- %s"
      % (len(fell), sum(s for _, s, _ in fell),
         "RECONCILES" if sum(s for _, s, _ in fell) == fm["matched_code"] - em["matched_code"]
         else "MISMATCH"))
print("\n%-18s %8s %12s %8s" % ("channel", "rows", "bytes", "share"))
tb = sum(v[1] for v in chan.values()) or 1
for c, (n, b) in sorted(chan.items(), key=lambda x: -x[1][1]):
    print("%-18s %8d %12d %7.2f%%" % (c, n, b, 100.0 * b / tb))

# how many rows lost mpn==100 overall (should explain matched_functions delta)
lost_mpn = sum(1 for k, (fz, mp, sz) in fr.items()
               if mp == 100.0 and (k not in er or er[k][1] < 100.0))
print("\nrows losing mpn==100: %d   (matched_functions delta %+d)"
      % (lost_mpn, fm["matched_functions"] - em["matched_functions"]))

json.dump([["\t".join(k), s, c] for k, s, c in fell], open(sys.argv[2], "w"))
print("wrote %s" % sys.argv[2])
