#!/usr/bin/env python3
"""Screen the named sub-100 population for rows whose retail body CANNOT be the
function the name claims -- i.e. the name<->address assignment is wrong, so no
amount of source work can ever cross the row.

WHY THIS EXISTS (lane BODIES-3, successor to BODIES-1)
  BODIES-1's stub sweep ranks candidates by "retail is big, ours is tiny". Its
  single largest candidate (BandHighlight::Save, 1,140 B) turned out to be a
  FALSE PAIRING -- the body is a Load, not a Save -- and its splits pin of
  exactly 0x474 LOOKED like independent corroboration. BODIES-3 then found three
  more in the same 17-row list. So roughly a QUARTER of that list is not a
  defect list at all, and every lane that opens it pays the triage cost again.

  This encodes the cheap half of that triage so it runs once, in bulk.

WHAT IT DETECTS (detector A: EH unwind funclets)
  MSVC X360 emits unwind funclets as separate .pdata-bearing extents. A funclet
  recovers its PARENT's frame through r12 and then runs cleanup, so it opens:

      subi r31, r12, 0x80      <- r12-relative parent-frame recovery
      mflr r12 / stwu r1, -0x60(r1)
      lwz  r11, 0x94(r31)      <- reach INTO the parent frame
      addi r3, r11, 0x34
      bl   ??1String@@UAA@XZ   <- destruct a member

  No argument setup, no return value, a destructor call. When such an extent is
  pinned to a method name, the row can never match: our compiler will never emit
  a funclet under that symbol.

THE CONTROL, AND WHY IT IS NOT OPTIONAL
  A screen that only ever runs on the rows you suspect confirms whatever you
  point it at -- this repo has burned several lanes that way. So the detector is
  also run over the UNTREATED population: every named row objdiff scores at
  mpn == 100. A matched row is by construction correctly paired, so the
  detector MUST NOT fire there. Measured on the BODIES-3 baseline:

      CONTROL  named mpn==100 : 0 / 21,349   (0.0000%)
      TREATED  named sub-100  : 4 /  6,680   (0.0599%)

  Zero false positives over 21k rows. The tool REFUSES to print candidates if
  the control ever fires, because that would mean the detector is matching
  ordinary prologues and every candidate below is suspect.

WHAT IT DOES *NOT* DETECT
  Detector A is narrow on purpose. The other two false pairings BODIES-3 found
  are SEMANTIC, not structural, and no cheap byte pattern finds them:
    * BandSongMgr::HasContentAltDirs pinned under the BASE name
      ContentMgr::Callback::HasContentAltDirs. Adjudicated by asking the
      COMPILER for the member offset (mContentAltDirs is at 0x160, and the body
      divides by 12 = sizeof(String)) -- see scripts/harvest/class_layout_report.py.
    * BandHighlight::Save whose body calls ReadEndian (BODIES-1).
  So an EMPTY result here does NOT mean a candidate list is clean. It means the
  funclet half of it is. Read the retail body before believing any candidate.

  ALSO: keying is on the `.fn fn_<addr>` SYMBOL, never the .s address column --
  dtk's address/file-offset columns are SYNTHETIC for multi-block units and name
  addresses the function does not live at.
"""
import argparse
import collections
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# strip dtk's "/* ADDR OFFSET BYTES */\t" instruction prefix
STRIP = re.compile(r"^/\*.*?\*/\s*")
# detector A: r12-relative parent-frame recovery as the FIRST instruction
FUNCLET = re.compile(r"^(subi|addi)\s+r\d+,\s*r12,")


def index_target_bodies(project_dir):
    """-> {fn_symbol_lower: [asm_path, [instruction, ...]]}

    Keyed on the `.fn` SYMBOL. The address column is deliberately ignored.
    """
    bodies = {}
    fnre = re.compile(r"^\s*\.fn\s+(fn_[0-9A-Fa-f]+)")
    pat = os.path.join(project_dir, "build/45410914/asm/**/*.s")
    for path in glob.glob(pat, recursive=True):
        cur, buf = None, None
        with open(path, errors="replace") as fh:
            for line in fh:
                m = fnre.match(line)
                if m:
                    cur, buf = m.group(1), []
                    continue
                if cur is None:
                    continue
                if line.strip().startswith(".endfn"):
                    bodies.setdefault(cur.lower(), [path, buf])
                    cur, buf = None, None
                else:
                    s = line.strip()
                    if s and s[0] not in "#.":
                        buf.append(STRIP.sub("", s))
    return bodies


def invert_symbol_map(path):
    m = json.load(open(path))
    n2a = collections.defaultdict(list)
    for addr, name in m.items():
        for x in (name if isinstance(name, list) else [name]):
            n2a[x].append(addr)
    return n2a


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-dir", default=ROOT)
    ap.add_argument("--report", default=None)
    ap.add_argument("--show-body", action="store_true",
                    help="print the retail instructions for each candidate")
    args = ap.parse_args()

    pd = os.path.abspath(args.project_dir)
    report = args.report or os.path.join(pd, "build/45410914/report.json")
    rep = json.load(open(report))
    n2a = invert_symbol_map(os.path.join(pd, "scripts/target_symbol_map.json"))
    bodies = index_target_bodies(pd)

    def body_for(name):
        for a in n2a.get(name, []):
            k = "fn_" + a[2:].lower()
            if k in bodies:
                return bodies[k]
        return None

    ctrl_hit = ctrl_tot = 0
    cands = []
    for u in rep["units"]:
        for f in u.get("functions", []):
            n = f.get("name", "")
            if not n or n.startswith("fn_") or n.startswith("auto_"):
                continue
            b = body_for(n)
            if not b or not b[1]:
                continue
            hit = bool(FUNCLET.match(b[1][0]))
            # int()-coerce: report.json ships several numerics as JSON STRINGS
            if f.get("match_percent_normalized", 0.0) == 100.0:
                ctrl_tot += 1
                ctrl_hit += hit
            elif hit:
                cands.append((int(f.get("size", 0)),
                              f.get("match_percent_normalized", 0.0),
                              n, u.get("name", ""), b))

    rate = ctrl_hit / max(ctrl_tot, 1)
    print(f"CONTROL  named mpn==100 : funclet-signature "
          f"{ctrl_hit}/{ctrl_tot} = {100 * rate:.4f}%")
    if ctrl_hit:
        print("\nREFUSING to print candidates: the detector fired on the matched "
              "population, so it is matching ordinary prologues, not funclets. "
              "Every candidate would be suspect.", file=sys.stderr)
        return 2

    print(f"CANDIDATES (named sub-100 rows whose retail extent is a funclet): "
          f"{len(cands)}, {sum(c[0] for c in cands)} B\n")
    for sz, mpn, n, unit, b in sorted(cands, reverse=True):
        print(f"  {sz:6d} B  mpn={mpn:6.2f}  {unit}")
        print(f"           {n}")
        print(f"           first: {b[1][0]}")
        if args.show_body:
            for i, x in enumerate(b[1]):
                print(f"             {i:3d} {x}")
    print("\nThese are MAP defects, not body defects: no source change can cross "
          "them.\nFix by correcting the name<->address assignment, not by writing "
          "a body.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
