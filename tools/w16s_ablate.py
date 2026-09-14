#!/usr/bin/env python3
"""W16-S: size an alias VERDICT CLASS by ABLATION, never by a name-keyed census.

ALIAS-2's lesson: a name-keyed census of what an alias "covers" books its own
blind spot as risk (it reported 11% unattributable where ablation showed 0 of
1,894 rows depended on a non-proven membership).  The only honest size for
"what does this class of memberships forgive?" is to REMOVE the class, rebuild,
and read the difference out of report.json.

Restores scripts/symbol_aliases.json on EVERY exit path, including Ctrl-C and an
unhandled exception, and verifies the restore by re-reading the file -- a
cleanup that merely ran is not a cleanup that worked (lane TOOL-AB).
"""
import argparse
import collections
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALI = ROOT / "scripts" / "symbol_aliases.json"
REP = ROOT / "build" / "45410914" / "report.json"
KEYS = ("matched_functions", "matched_code", "matched_code_percent",
        "fuzzy_match_percent", "masked_equal_functions")


def measures():
    m = json.loads(REP.read_text())["measures"]
    out = {}
    for k in KEYS:
        if k not in m:                      # protobuf-JSON omits defaults, but a
            sys.exit("REFUSING: report.json has no key %r" % k)   # missing SCORE
        v = m[k]                            # key is a broken run, not a zero.
        out[k] = int(v) if k in ("matched_functions", "matched_code",
                                 "masked_equal_functions") else float(v)
    return out


def build(tag):
    log = Path.home() / "tmp" / ("rb3_build_w16s_%s.log" % tag)
    with open(log, "wb") as f:
        rc = subprocess.call([str(ROOT / "tools" / "ninja-locked")],
                             cwd=ROOT, stdout=f, stderr=subprocess.STDOUT)
    if rc != 0:
        sys.exit("REFUSING: build rc=%d (report.json is now STALE) -- see %s"
                 % (rc, log))
    return log


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--census", required=True)
    ap.add_argument("--classes", nargs="+", required=True)
    ap.add_argument("--json")
    args = ap.parse_args()

    rows = json.loads(Path(args.census).read_text())
    orig_text = ALI.read_text()
    # ⛔ THE CENSUS'S `gi` IS NOT AN INDEX INTO groups[].  Measured 2026-09-14
    # (lane W16-AD): of 5,315 census rows only 503 have gi == the true index;
    # 4,785 are wrong (skew +1 on 68 and +2 on 564 of the 643 UNDECIDED_MASKED
    # rows alone) and 27 resolve to no group at all.  Keying on gi therefore
    # edited the WRONG group -- and because the edit filters by NAME
    # membership, a wrong group simply does not contain those spellings, so the
    # ablation SILENTLY REMOVED NOTHING and under-reported what the class
    # forgives.  A no-op that looks like a measurement is the worst shape a
    # defect can take here.
    # (survivor, address) is unique over all 1,634 groups, and resolving the
    # 643 UNDECIDED_MASKED rows that way puts the folded spelling inside the
    # resolved group 643/643 -- which is the check that proves the key, since a
    # wrong key lands on a group that does not contain the spelling.
    gali = json.loads(orig_text)
    gkey = {(g["survivor"], g.get("address")): i
            for i, g in enumerate(gali["groups"])}

    def resolve(r):
        i = gkey.get((r["survivor"], r["addr"]))
        if i is None:
            sys.exit("REFUSING: census row (survivor=%s addr=%s) matches no "
                     "alias group -- the census and the alias file disagree"
                     % (r["survivor"][:60], r["addr"]))
        if r["folded"] not in gali["groups"][i]["folded"]:
            sys.exit("REFUSING: %s is not in the folded list of the group it "
                     "resolved to -- the key is wrong, and ablating here would "
                     "silently remove nothing" % r["folded"][:70])
        return i
    results = {}
    try:
        print("== leg A: baseline (alias file as committed) ==")
        build("ablate_base")
        base = measures()
        print("   ", base)
        for cls in args.classes:
            drop = collections.defaultdict(set)
            for r in rows:
                if r["verdict"] == cls:
                    drop[resolve(r)].add(r["folded"])
            n = sum(len(v) for v in drop.values())
            if not n:
                print("== %s: 0 memberships, skipped ==" % cls)
                continue
            d = json.loads(orig_text)
            for gi, names in drop.items():
                g = d["groups"][gi]
                g["folded"] = [x for x in g["folded"] if x not in names]
            ALI.write_text(json.dumps(d, indent=1, ensure_ascii=False) + "\n")
            print("== ablating %s: %d memberships over %d groups ==" % (cls, n, len(drop)))
            build("ablate_" + cls.lower())
            m = measures()
            delta = {k: round(m[k] - base[k], 6) for k in KEYS}
            print("   ", m)
            print("    DELTA", delta)
            results[cls] = {"memberships": n, "groups": len(drop),
                            "measures": m, "delta": delta}
            ALI.write_text(orig_text)
    finally:
        ALI.write_text(orig_text)
        assert ALI.read_text() == orig_text, "RESTORE FAILED -- alias file differs"
        print("\n[restore] scripts/symbol_aliases.json verified byte-identical "
              "to the pre-run state")
        print("[restore] rebuilding so the tree is left consistent")
        build("ablate_restore")
        print("   ", measures())
    if args.json:
        Path(args.json).write_text(json.dumps({"base": base, "classes": results},
                                              indent=1))
        print("wrote", args.json)


if __name__ == "__main__":
    main()
