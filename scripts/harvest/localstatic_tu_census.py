#!/usr/bin/env python3
"""localstatic_tu_census.py -- TU-LEVEL census for the BULK-CONVERSION LAW.

Background (docs/plans/decomp-state-2026-07-19.md, TRANSFERABLE LEVERS #1):
retail declares Symbols/Messages as *function-local statics*; the rb3-Wii oracle
often uses file-scope `Symbols*.h` globals.  Converting ONE function in a TU
reads NET-NEGATIVE (its new statics collaterally re-pair EH funclets); converting
EVERY straggler in the TU at once reads strongly positive.

So the unit of work is the TU, and the thing worth finding is a TU that is
*partially* converted: some of its functions already carry the local-static form
(and score 100) while others still reference the globals (and score < 100).

This script produces that census.  Read-only.

Per unit it reports:
  ls_fns        functions whose TARGET asm contains >=1 guarded Symbol-ctor init
                (i.e. retail used a function-local static there)
  done          ...of those, how many already score >= 99.999
  strag         ...of those, how many still score < 99.999   <-- the work
  tstat         total target-side local statics summed over the stragglers
  srcstatic     `static Symbol|DataPoint` decls found in the unit's source .cpp
                (>0 with strag>0 == PARTIALLY CONVERTED, the sweet spot)
  glob          distinct Symbols*.h globals referenced by the unit's source
  unmapped      count of still-anonymous fn_ entries in the unit (funclet pool
                proxy -- this is where the cascade multiplier lives)

Usage:
  venv/bin/python scripts/harvest/localstatic_tu_census.py
  venv/bin/python scripts/harvest/localstatic_tu_census.py --json ~/tmp/census.json
"""
import argparse
import glob
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MAP = os.path.join(REPO, "scripts", "target_symbol_map.json")
REPORT = os.path.join(REPO, "build", "45410914", "report.json")
ASM = os.path.join(REPO, "build", "45410914", "asm", "*.s")
OBJECTS = os.path.join(REPO, "config", "45410914", "objects.json")

SYMBOL_CTOR_MANGLED = "??0Symbol@@QAA@PBD@Z"
FN_HDR = re.compile(r"^\.fn (fn_[0-9A-Fa-f]+),")
ORI_GUARD = re.compile(r"\bori\s+r\d+, r\d+, 0x[0-9a-fA-F]+")
STRICT = 99.999
MACRO_BODIES = ("SyncProperty", "?Handle@", "?Type@", "PropSync", "StaticClassName")


def _names(v):
    return v if isinstance(v, list) else [v]


def symbols_globals():
    s = set()
    for n in ("", "2", "3", "4"):
        p = os.path.join(REPO, "src", "system", "utl", "Symbols%s.h" % n)
        if os.path.exists(p):
            for ln in open(p, errors="replace"):
                m = re.match(r"\s*extern\s+Symbol\s+(\w+)\s*;", ln)
                if m:
                    s.add(m.group(1))
    return s


def unit_sources():
    """report-unit-name -> list of absolute source paths."""
    out = {}
    try:
        objs = json.load(open(OBJECTS))
    except Exception:
        return out
    for grp in objs.get("units", objs) if isinstance(objs, dict) else objs:
        pass
    return out


def build_target_static_counts():
    symmap = json.load(open(MAP))
    va2name = {k.lower(): _names(v) for k, v in symmap.items()}
    ctor = None
    for va, val in symmap.items():
        if SYMBOL_CTOR_MANGLED in _names(val):
            ctor = "fn_" + va[2:].lower()
            break
    if ctor is None:
        sys.exit("Symbol ctor not in map")
    counts = {}
    for path in sorted(glob.glob(ASM)):
        cur, body = None, []
        for line in open(path, errors="ignore"):
            line = line.rstrip("\n")
            hdr = FN_HDR.match(line)
            if hdr:
                cur, body = hdr.group(1), []
                continue
            if line.startswith(".endfn"):
                if cur:
                    n = 0
                    for i, ins in enumerate(body):
                        if ("bl " + ctor) in ins.lower():
                            win = body[max(0, i - 8):i]
                            if any(ORI_GUARD.search(w) for w in win) and any(
                                "\tstw " in w for w in win
                            ):
                                n += 1
                    if n:
                        for nm in va2name.get("0x" + cur[3:].lower(), []):
                            counts[nm] = max(counts.get(nm, 0), n)
                cur, body = None, []
                continue
            if cur is not None:
                body.append(line)
    return counts


def find_sources(unit_name):
    """Guess source file(s) for a report unit name like default/band3/game/Foo."""
    rel = unit_name.split("/", 1)[1] if "/" in unit_name else unit_name
    cands = []
    base = os.path.basename(rel)
    for root in ("src", ):
        for p in glob.glob(os.path.join(REPO, root, "**", base + ".cpp"), recursive=True):
            cands.append(p)
    # prefer a path that contains the unit's directory hint
    hint = os.path.dirname(rel)
    if hint:
        pref = [c for c in cands if hint in c]
        if pref:
            return pref
    return cands


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", default=None)
    ap.add_argument("--min-strag", type=int, default=1)
    ap.add_argument("--exclude-macro-bodies", action="store_true", default=True)
    args = ap.parse_args()

    tstat = build_target_static_counts()
    globs = symbols_globals()
    report = json.load(open(REPORT))

    rows = []
    for unit in report["units"]:
        un = unit["name"]
        fns = unit.get("functions", [])
        if not fns:
            continue
        ls_done = ls_strag = 0
        strag_stat = 0
        strag_list = []
        unmapped = 0
        for f in fns:
            nm = f["name"]
            if nm.startswith("fn_"):
                unmapped += 1
                continue
            n = tstat.get(nm)
            if not n:
                continue
            if args.exclude_macro_bodies and any(k in nm for k in MACRO_BODIES):
                continue
            p = f.get("match_percent_normalized") or 0.0
            if p >= STRICT:
                ls_done += 1
            else:
                ls_strag += 1
                strag_stat += n
                strag_list.append((round(p, 2), nm, n))
        if ls_strag < args.min_strag:
            continue
        srcs = find_sources(un)
        srcstatic = 0
        globrefs = set()
        for s in srcs:
            try:
                txt = open(s, errors="replace").read()
            except Exception:
                continue
            srcstatic += len(re.findall(r"\bstatic\s+(?:Symbol|DataPoint)\b", txt))
            for g in globs:
                if re.search(r"\b%s\b" % re.escape(g), txt):
                    globrefs.add(g)
        rows.append(dict(unit=un, ls_done=ls_done, ls_strag=ls_strag,
                         tstat=strag_stat, srcstatic=srcstatic,
                         glob=len(globrefs), unmapped=unmapped,
                         srcs=[os.path.relpath(s, REPO) for s in srcs],
                         stragglers=sorted(strag_list)))

    # rank: stragglers x (funclet pool present)
    rows.sort(key=lambda r: (r["ls_strag"], r["tstat"], r["unmapped"]), reverse=True)

    print("%-46s %5s %5s %5s %6s %5s %6s  %s" %
          ("unit", "done", "strag", "tstat", "srcst", "glob", "unmapd", "src"))
    for r in rows:
        print("%-46s %5d %5d %5d %6d %5d %6d  %s" %
              (r["unit"][:46], r["ls_done"], r["ls_strag"], r["tstat"],
               r["srcstatic"], r["glob"], r["unmapped"],
               ",".join(r["srcs"])[:60]))
    print("# %d units with >=%d straggler(s)" % (len(rows), args.min_strag))
    if args.json:
        json.dump(rows, open(args.json, "w"), indent=2)


if __name__ == "__main__":
    main()
