#!/usr/bin/env python3
"""Audit ``*_SUPERCLASS`` macro chains against the rb3-Wii / DC3 oracles.

Motivation
----------
DC3 is *newer* than retail RB3, so DC3-derived engine sources carry macro-chain
entries retail never had.  The ``SYNC_SUPERCLASS`` sweep (see
``docs/plans/funclet-cascade-lever-2026-07-25.md`` §23.1/§25) found that DC3 had
appended a trailing ``SYNC_SUPERCLASS(Hmx::Object)`` to 97 classes; removing it
banked +29 strict matches.

This tool generalises that audit to every ``BEGIN_*``/``END_*`` macro block so a
family can be *priced before it is funded*.  It reports the full funnel:

    raw sites -> paired -> paired & sub-100 -> chain differs from oracle

Only the last column is workable: a chain fix can only score on a function that
objdiff actually pairs, and only if our chain really differs from retail's.

Two correctness details that matter (both were live traps):

* **Preprocessor awareness.**  The ``SYNC_SUPERCLASS`` sweep *guarded* removals
  with ``#ifdef HX_NATIVE`` rather than deleting them, so the text is still
  present but dead for the X360 build.  A naive grep double-counts those as
  unfixed.  We track an ``HX_NATIVE`` ifdef stack and only count *active* sites.
* **Oracle blindness is not agreement.**  rb3-Wii has *zero* ``BEGIN_SAVES`` /
  ``SAVE_SUPERCLASS`` sites, so it cannot adjudicate the SAVE family at all.
  Those are reported as ``blind``, never as ``same`` -- they need target-asm
  evidence (size parity + absence of an insert cluster), not an oracle diff.

Usage
-----
    python3 scripts/harvest/superclass_chain_audit.py            # funnel table
    python3 scripts/harvest/superclass_chain_audit.py --list     # candidates
    python3 scripts/harvest/superclass_chain_audit.py --json out.json
"""

from __future__ import annotations

import argparse
import collections
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WII_ORACLE = "/home/free/code/milohax/rb3/src"
DC3_ORACLE = "/home/free/code/milohax/dc3-decomp/src"
REPORT = os.path.join(REPO, "build", "45410914", "report.json")

BEGIN = re.compile(
    r"\bBEGIN_(HANDLERS|COPYS|SAVES|LOADS|PROPSYNCS|CUSTOM_PROPSYNC)\s*\(\s*([A-Za-z_][\w:]*)"
)
END = re.compile(r"\bEND_(HANDLERS|COPYS|SAVES|LOADS|PROPSYNCS|CUSTOM_PROPSYNC)\b")
SUP = re.compile(
    r"\b(HANDLE|COPY|SAVE|LOAD|SYNC)(?:_VIRTUAL)?_SUPERCLASS(?:_FROM)?\s*\(\s*([A-Za-z_][\w:]*)"
)
IFDEF = re.compile(r"^\s*#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)")

FAMILY_OF_BLOCK = {
    "HANDLERS": "HANDLE",
    "COPYS": "COPY",
    "SAVES": "SAVE",
    "LOADS": "LOAD",
    "PROPSYNCS": "SYNC",
    "CUSTOM_PROPSYNC": "SYNC",
}
# the virtual method each family's block defines, used to pair against report.json
METHOD_OF_FAMILY = {
    "HANDLE": "Handle",
    "COPY": "Copy",
    "SAVE": "Save",
    "LOAD": "Load",
    "SYNC": "SyncProperty",
}
FAMILIES = ["HANDLE", "COPY", "SAVE", "LOAD", "SYNC"]


def iter_active_lines(path):
    """Yield ``(lineno, text, active)``.

    ``active`` is False when the line sits in an arm that the X360 build drops,
    i.e. inside ``#ifdef HX_NATIVE`` (HX_NATIVE is undefined for the decomp
    target).  Only HX_NATIVE conditionals are modelled; every other conditional
    is treated as live so we never silently under-count real sites.
    """
    try:
        with open(path, errors="ignore") as fh:
            lines = fh.read().split("\n")
    except OSError:
        return
    stack = []  # (is_hx_native, arm_is_live)
    for i, line in enumerate(lines):
        m = IFDEF.match(line)
        if m:
            directive, rest = m.group(1), m.group(2)
            if directive in ("ifdef", "if"):
                hx = "HX_NATIVE" in rest
                stack.append((hx, not hx if hx else True))
            elif directive == "ifndef":
                hx = "HX_NATIVE" in rest
                stack.append((hx, True))
            elif directive in ("else", "elif"):
                if stack:
                    hx, live = stack[-1]
                    stack[-1] = (hx, (not live) if hx else True)
            elif directive == "endif":
                if stack:
                    stack.pop()
            continue
        yield i + 1, line, all(live for _, live in stack)


def scan_tree(root):
    """Return a list of macro blocks found under ``root``."""
    blocks = []
    for dirpath, _, filenames in os.walk(root):
        for name in filenames:
            if not name.endswith((".cpp", ".h")):
                continue
            path = os.path.join(dirpath, name)
            cur = None
            for lineno, line, active in iter_active_lines(path):
                begin = BEGIN.search(line)
                if begin:
                    cur = {
                        "file": os.path.relpath(path, root),
                        "fam": FAMILY_OF_BLOCK.get(begin.group(1)),
                        "cls": begin.group(2).split("::")[-1],
                        "line": lineno,
                        "parents": [],
                    }
                    continue
                if cur is None:
                    continue
                sup = SUP.search(line)
                if sup and active:
                    cur["parents"].append(
                        {"p": sup.group(2).split("::")[-1], "line": lineno}
                    )
                if END.search(line):
                    blocks.append(cur)
                    cur = None
    return blocks


def index_by_class(blocks):
    out = {}
    for b in blocks:
        out.setdefault((b["fam"], b["cls"]), []).append([p["p"] for p in b["parents"]])
    return out


def load_report_functions(report_path):
    """Map ``(method, class)`` -> list of (unit, symbol, match%) from report.json."""
    with open(report_path) as fh:
        report = json.load(fh)
    fns = {}
    pat = re.compile(r"^\?(\w+)@([\w@]+?)@@")
    for unit in report["units"]:
        for fn in unit.get("functions", []):
            m = pat.match(fn.get("name") or "")
            if not m:
                continue
            key = (m.group(1), m.group(2).split("@")[0])
            fns.setdefault(key, []).append(
                (unit["name"], fn["name"], fn.get("match_percent_normalized"))
            )
    return fns


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", default=os.path.join(REPO, "src"))
    ap.add_argument("--report", default=REPORT)
    ap.add_argument("--wii", default=WII_ORACLE)
    ap.add_argument("--dc3", default=DC3_ORACLE)
    ap.add_argument("--list", action="store_true",
                    help="list the DIFF / ORACLE-BLIND candidates")
    ap.add_argument("--json", help="write candidate records to this path")
    args = ap.parse_args()

    if not os.path.exists(args.report):
        sys.exit(f"report.json not found at {args.report} -- run ./tools/ninja-locked first")

    ours = scan_tree(args.src)
    wii = index_by_class(scan_tree(args.wii)) if os.path.isdir(args.wii) else {}
    dc3 = index_by_class(scan_tree(args.dc3)) if os.path.isdir(args.dc3) else {}
    fns = load_report_functions(args.report)

    raw = collections.Counter()
    paired = collections.Counter()
    sub100 = collections.Counter()
    diff = collections.Counter()
    blind = collections.Counter()
    candidates = []

    for b in ours:
        fam, n = b["fam"], len(b["parents"])
        if fam not in METHOD_OF_FAMILY or not n:
            continue
        raw[fam] += n
        hit = fns.get((METHOD_OF_FAMILY[fam], b["cls"]))
        if not hit:
            continue  # unpaired: cannot score, so cannot be workable
        paired[fam] += n
        best = max((h[2] if h[2] is not None else -1) for h in hit)
        if best >= 100.0:
            continue  # already matching: no defect here
        sub100[fam] += n

        ourchain = [p["p"] for p in b["parents"]]
        oracle = wii.get((fam, b["cls"]))
        if not oracle:
            blind[fam] += n
            tag = "ORACLE-BLIND"
        elif ourchain != oracle[0]:
            diff[fam] += n
            tag = "DIFF"
        else:
            continue
        candidates.append(
            {
                "tag": tag, "fam": fam, "cls": b["cls"], "file": b["file"],
                "line": b["line"], "pct": best, "ours": ourchain,
                "wii": oracle[0] if oracle else None,
                "dc3": (dc3.get((fam, b["cls"])) or [None])[0],
                "unit": hit[0][0], "sym": hit[0][1],
            }
        )

    hdr = f"{'family':8}{'raw':>6}{'paired':>8}{'sub-100':>9}{'DIFF':>6}{'blind':>7}"
    print(hdr)
    print("-" * len(hdr))
    for fam in FAMILIES:
        print(f"{fam:8}{raw[fam]:6}{paired[fam]:8}{sub100[fam]:9}{diff[fam]:6}{blind[fam]:7}")
    print("-" * len(hdr))
    print(f"{'TOTAL':8}{sum(raw.values()):6}{sum(paired.values()):8}"
          f"{sum(sub100.values()):9}{sum(diff.values()):6}{sum(blind.values()):7}")
    print("\nOnly the DIFF column is directly workable. 'blind' = the rb3-Wii oracle")
    print("has no such block at all (e.g. it has ZERO BEGIN_SAVES) and therefore")
    print("cannot adjudicate; those need target-asm evidence, not an oracle diff.")

    if args.list:
        print("\n=== candidates (paired, sub-100, chain differs or oracle-blind) ===")
        for c in sorted(candidates, key=lambda x: -x["pct"]):
            print(f"{c['tag']:12} {c['fam']:6} {c['cls'][:26]:26} {c['pct']:7.2f} "
                  f"ours={c['ours']} wii={c['wii']}")
    if args.json:
        with open(args.json, "w") as fh:
            json.dump(candidates, fh, indent=1)
        print(f"\nwrote {len(candidates)} candidate records -> {args.json}")


if __name__ == "__main__":
    main()
